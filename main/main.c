/**
 * SimpleGo - Native SimpleX SMP Client for ESP32
 * v0.1.17-alpha - AgentConfirmation with Reply Queue
 * github.com/cannatoshi/SimpleGo
 * Autor: cannatoshi
 *
 * Phase 1 Refactoring: main.c contains only:
 * - Config, app_main(), smp_connect()
 * - TLS/Handshake (Steps 1-5)
 * - Message Receive Loop with transport parsing
 * - Post-confirmation orchestration (42d: KEY/HELLO/read reply)
 *
 * Extracted to modules:
 * - smp_wifi.c       (WiFi init)
 * - smp_ack.c        (ACK consolidation)
 * - smp_e2e.c        (Reply Queue E2E decrypt pipeline)
 * - smp_agent.c      (Agent protocol: PrivHeader dispatch, ratchet, Zstd)
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"

#include "sodium.h"

// SimpleGo modules
#include "smp_types.h"
#include "smp_utils.h"
#include "smp_crypto.h"
#include "smp_network.h"
#include "smp_ratchet.h"
#include "smp_contacts.h"
#include "smp_parser.h"
#include "smp_peer.h"
#include "smp_ack.h"
#include "smp_e2e.h"
#include "smp_agent.h"
#include "smp_wifi.h"
#include "smp_storage.h"
#include "smp_frame_pool.h"
#include "smp_tasks.h"

extern bool peer_send_hello(contact_t *contact);
extern int queue_raw_tls_read(uint8_t *buf, int buf_size, int timeout_ms);
extern bool queue_has_pending_msg(int *out_len);
#include "smp_x448.h"
#include "smp_queue.h"
#include "smp_handshake.h"  // Auftrag 50d: handshake_load_state
#include "simplex_crypto.h"

// T-Deck Display Driver
#include "tdeck_display.h"
#include "tdeck_lvgl.h"
#include "tdeck_touch.h"
#include "tdeck_keyboard.h"  // Auftrag 50d: T-Deck keyboard

// UI System
#include "ui_manager.h"
#include "ui/screens/ui_connect.h"
#include "ui_theme.h"

static const char *TAG = "SMP";

// Auftrag 50b: Session restoration flag (set in app_main, read in smp_connect)
static bool session_restored = false;

// Auftrag 50d: Keyboard → SMP thread communication via FreeRTOS Queue
#include "freertos/queue.h"
static QueueHandle_t kbd_msg_queue = NULL;   // char[256] messages from kbd task

static void keyboard_task(void *arg)
{
    (void)arg;
    static char buf[256] = {0};
    int pos = 0;
    
    ESP_LOGI("KBD_TASK", "⌨️ Keyboard task started (polling every 50ms)");
    
    while (1) {
        char key = tdeck_keyboard_read();
        
        if (key != 0) {
            if (key == '\r' || key == '\n') {
                // Enter → send buffered message
                if (pos > 0) {
                    buf[pos] = '\0';
                    ESP_LOGI("KBD_TASK", "⌨️ ENTER → \"%s\"", buf);
                    // Send copy to main loop via queue (don't block)
                    xQueueSend(kbd_msg_queue, buf, 0);
                    pos = 0;
                    buf[0] = '\0';
                }
            } else if (key == 0x08) {
                // Backspace
                if (pos > 0) {
                    pos--;
                    buf[pos] = '\0';
                }
                ESP_LOGI("KBD_TASK", "⌨️ Buffer: \"%s\"", buf);
            } else if (key >= 0x20 && key < 0x7F && pos < 254) {
                // Printable character
                buf[pos++] = key;
                buf[pos] = '\0';
                ESP_LOGI("KBD_TASK", "⌨️ Buffer: \"%s\"", buf);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============== CONFIG ==============
#define SMP_HOST      "smp1.simplexonflux.com"
#define SMP_PORT      5223

// ============== Main SMP Connection ==============

static void smp_connect(void) {
    int ret;
    int sock = -1;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    uint8_t session_id[32];
    uint8_t ca_hash[32];

    // Peer's sender auth key from received Confirmation
    uint8_t peer_sender_auth_key[44];
    bool has_peer_sender_auth = false;

    uint8_t *block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "Failed to allocate buffer!");
        return;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "|  SimpleGo v0.1.17-alpha Connection!    |");
    ESP_LOGI(TAG, "+----------------------------------------+");
    ESP_LOGI(TAG, "");

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) goto cleanup;

    // ========== Step 1: TCP + TLS ==========
    ESP_LOGI(TAG, "[1/5] Connecting to %s:%d...", SMP_HOST, SMP_PORT);
    sock = smp_tcp_connect(SMP_HOST, SMP_PORT);
    if (sock < 0) goto cleanup;

    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) goto cleanup;

    mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    static const char *alpn_list[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&conf, alpn_list);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) goto cleanup;

    mbedtls_ssl_set_hostname(&ssl, SMP_HOST);
    mbedtls_ssl_set_bio(&ssl, &sock, my_send_cb, my_recv_cb, NULL);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "      TLS failed: -0x%04X", -ret);
            goto cleanup;
        }
    }
    ESP_LOGI(TAG, "      TLS OK! ALPN: %s", mbedtls_ssl_get_alpn_protocol(&ssl));

    // ========== Step 2: ServerHello ==========
    ESP_LOGI(TAG, "[2/5] Waiting for ServerHello...");
    int content_len = smp_read_block(&ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "      No ServerHello");
        goto cleanup;
    }

    uint8_t *hello = block + 2;
    uint16_t minVer = (hello[0] << 8) | hello[1];
    uint16_t maxVer = (hello[2] << 8) | hello[3];
    uint8_t sessIdLen = hello[4];
    
    if (sessIdLen != 32) {
        ESP_LOGE(TAG, "      Unexpected sessionId length: %d", sessIdLen);
        goto cleanup;
    }
    memcpy(session_id, &hello[5], 32);
    
    ESP_LOGI(TAG, "      Versions: %d-%d", minVer, maxVer);
    ESP_LOGI(TAG, "      SessionId: %02x%02x%02x%02x...", 
             session_id[0], session_id[1], session_id[2], session_id[3]);

    // ========== Step 3: ClientHello ==========
    ESP_LOGI(TAG, "[3/5] Sending ClientHello...");
    
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);
    
    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
    }
    
    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;
    client_hello[pos++] = 32;
    memcpy(&client_hello[pos], ca_hash, 32);
    pos += 32;
    
    ret = smp_write_handshake_block(&ssl, block, client_hello, pos);
    if (ret != 0) goto cleanup;
    ESP_LOGI(TAG, "      ClientHello sent!");

    // ========== Step 4: Load or Create Contacts ==========
    ESP_LOGI(TAG, "[4/5] Loading contacts...");
    
    if (!session_restored) {
        // Fresh start for testing - comment out in production!
        ESP_LOGW(TAG, "      Clearing old contacts for fresh test...");
        clear_all_contacts();
    } else {
        ESP_LOGI(TAG, "      Session restored — keeping persisted contacts");
    }
    
    load_contacts_from_nvs();
    
    if (contacts_db.num_contacts == 0) {
        if (session_restored) {
            ESP_LOGE(TAG, "      ⚠️ Session restored but no contacts found! Falling back to fresh start...");
            session_restored = false;
        }
        ESP_LOGI(TAG, "      No contacts found - creating 'ESP32'...");
        int idx = add_contact(&ssl, block, session_id, "ESP32");
        if (idx < 0) {
            ESP_LOGE(TAG, "      Failed to create contact!");
            goto cleanup;
        }
    } else {
        ESP_LOGI(TAG, "      %d contact(s) loaded from NVS", contacts_db.num_contacts);
    }
    
    list_contacts();
    
    // ========== Step 5: Subscribe All Contacts ==========
    ESP_LOGI(TAG, "[5/5] Subscribing to queues...");
    subscribe_all_contacts(&ssl, block, session_id);
    
    print_invitation_links(ca_hash, SMP_HOST, SMP_PORT);
        
    // Send invite link to UI
    {
        static char invite_link[1500];
        if (get_invite_link(ca_hash, SMP_HOST, SMP_PORT, invite_link, sizeof(invite_link))) {
            ui_connect_set_invite_link(invite_link);
        }
    }
    
    // ========== Message Receive Loop ==========
    ESP_LOGI(TAG, "+--------------------------------------+");
    ESP_LOGI(TAG, "|   Waiting for messages...            |");
    ESP_LOGI(TAG, "|   (Connect with SimpleX App!)        |");
    ESP_LOGI(TAG, "+--------------------------------------+");
    ESP_LOGI(TAG, "");
    
    while (1) {
        // Auftrag 50d: Check keyboard queue (non-blocking)
        char kbd_msg[256];
        while (kbd_msg_queue && xQueueReceive(kbd_msg_queue, kbd_msg, 0) == pdTRUE) {
            ESP_LOGI(TAG, "⌨️ Sending: \"%s\"", kbd_msg);
            contact_t *msg_contact = &contacts_db.contacts[0];
            if (peer_send_chat_message(msg_contact, kbd_msg)) {
                ESP_LOGI(TAG, "   ✅ Keyboard message sent!");
            } else {
                ESP_LOGE(TAG, "   ❌ Keyboard message send failed!");
            }
        }

        content_len = smp_read_block(&ssl, block, 5000);
        
        if (content_len == -2) {
            // Timeout — loop back to poll keyboard
            continue;
        }
        
        ESP_LOGW(TAG, "47b: MAIN LOOP received block, content_len=%d", content_len);
        
        // 47e: Dump first 64 bytes of EVERY received block
        if (content_len > 0) {
            ESP_LOGW(TAG, "47e: First 64 bytes of block:");
            for (int i = 0; i < 64 && i < content_len + 2; i += 16) {
                char hex[64] = {0}; int hx = 0;
                for (int j = 0; j < 16 && (i+j) < content_len + 2; j++)
                    hx += sprintf(&hex[hx], "%02x ", block[i+j]);
                ESP_LOGW(TAG, "  +%04d: %s", i, hex);
            }
        }
        
        if (content_len < 0) {
            ESP_LOGW(TAG, "   Connection closed");
            break;
        }
        
        uint8_t *resp = block + 2;
        
        // Parse transport format
        int p = 0;
        // txCount is a sequence counter, increments per transaction on TLS session
        uint8_t tx_count = resp[p];
        ESP_LOGD(TAG, "   txCount: %d", tx_count);
        p++;
        p += 2;
        
        int authLen = resp[p++]; p += authLen;
        int sessLen = resp[p++]; p += sessLen;
        int corrLen = resp[p++]; p += corrLen;
        
        int entLen = resp[p++];
        uint8_t entity_id[24];
        if (entLen > 24) entLen = 24;
        memcpy(entity_id, &resp[p], entLen);
        p += entLen;
        
        int contact_idx = find_contact_by_recipient_id(entity_id, entLen);
        contact_t *contact = (contact_idx >= 0) ? &contacts_db.contacts[contact_idx] : NULL;
        
        // Check if this is our Reply Queue
        bool is_reply_queue = (our_queue.rcv_id_len > 0 && 
                               entLen == our_queue.rcv_id_len &&
                               memcmp(entity_id, our_queue.rcv_id, entLen) == 0);
        if (is_reply_queue) {
            ESP_LOGI(TAG, "   Message on REPLY QUEUE from peer!");
        }

        // Parse command
        ESP_LOGW(TAG, "47b: entity=%02x%02x%02x%02x, contact=%s, reply_q=%d, cmd=%c%c%c",
                 entity_id[0], entity_id[1], entity_id[2], entity_id[3],
                 contact ? contact->name : "NULL", is_reply_queue,
                 (p < content_len) ? resp[p] : '?',
                 (p+1 < content_len) ? resp[p+1] : '?',
                 (p+2 < content_len) ? resp[p+2] : '?');
        if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
            ESP_LOGI(TAG, "   OK");
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'N' && resp[p+2] == 'D') {
            if (contact) {
                ESP_LOGI(TAG, "   END [%s] - No more messages", contact->name);
            } else {
                ESP_LOGI(TAG, "   END - No more messages");
            }
        }
        else if (p + 3 < content_len && resp[p] == 'M' && resp[p+1] == 'S' && resp[p+2] == 'G' && resp[p+3] == ' ') {
            p += 4;
            
            uint8_t msgIdLen = resp[p++];
            uint8_t msg_id[24];
            memset(msg_id, 0, 24);
            if (msgIdLen > 24) msgIdLen = 24;
            memcpy(msg_id, &resp[p], msgIdLen);
            p += msgIdLen;
            
            int enc_len = content_len - p;
            
            if (contact) {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "+----------------------------------------------------------+");
                ESP_LOGI(TAG, "|   MESSAGE RECEIVED for [%s]!", contact->name);
                ESP_LOGI(TAG, "+----------------------------------------------------------+");
            } else if (is_reply_queue) {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "+----------------------------------------------------------+");
                ESP_LOGI(TAG, "|   MESSAGE on REPLY QUEUE!                                |");
                ESP_LOGI(TAG, "+----------------------------------------------------------+");
            } else {
                ESP_LOGI(TAG, "   MESSAGE (unknown contact)!");
            }
            ESP_LOGD(TAG, "   MsgId: %02x%02x%02x%02x...", msg_id[0], msg_id[1], msg_id[2], msg_id[3]);
            ESP_LOGD(TAG, "   Encrypted: %d bytes", enc_len);

            // ==============================================================
            // REPLY QUEUE: E2E Decrypt + Agent Process
            // ==============================================================
            if (is_reply_queue && our_queue.valid && enc_len > crypto_box_MACBYTES) {
                ESP_LOGI(TAG, "   Decrypting REPLY QUEUE message...");

                uint8_t *e2e_plain = NULL;
                size_t e2e_plain_len = 0;

                int e2e_ret = smp_e2e_decrypt_reply_message(
                    &resp[p], enc_len, msg_id, msgIdLen,
                    &e2e_plain, &e2e_plain_len);

                if (e2e_ret == 0 && e2e_plain) {
                    smp_agent_process_message(e2e_plain, e2e_plain_len,
                           &contacts_db.contacts[0],
                           peer_sender_auth_key, &has_peer_sender_auth);
                    free(e2e_plain);
                }

                // === Post-Confirmation: KEY + HELLO + Read Reply (42d) ===
                if (has_peer_sender_auth) {
                    ESP_LOGI(TAG, "   Reconnecting to Reply Queue for KEY...");

                    if (!queue_reconnect()) {
                        ESP_LOGE(TAG, "   Reconnect failed!");
                        goto skip_42d;
                    }
                    if (!queue_subscribe()) {
                        ESP_LOGE(TAG, "   SUB failed!");
                        goto skip_42d;
                    }
                    if (!queue_send_key(peer_sender_auth_key, 44)) {
                        ESP_LOGE(TAG, "   KEY failed!");
                        goto skip_42d;
                    }
                    ESP_LOGI(TAG, "   KEY accepted!");

                    // Send HELLO on Contact Queue Q_A
                    ESP_LOGI(TAG, "   Sending HELLO on Contact Queue Q_A...");
                    {
                        contact_t *hello_contact = &contacts_db.contacts[0];
                        if (peer_send_hello(hello_contact)) {
                            ESP_LOGI(TAG, "   HELLO sent!");
                        } else {
                            ESP_LOGE(TAG, "   HELLO send failed!");
                        }
                    }

                    // Read + Decrypt Reply Queue response
                    ESP_LOGI(TAG, "   Reading Reply Queue message...");
                    {
                        uint8_t *rq_block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
                        if (!rq_block) goto skip_42d;

                        for (int rq_try = 0; rq_try < 3; rq_try++) {
                            int rq_len = queue_read_raw(rq_block, SMP_BLOCK_SIZE, 15000);
                            if (rq_len < 0) break;

                            uint8_t *rq_resp = rq_block + 2;

                            // Parse SMP transport
                            int rp = 0;
                            rp++;  // txCount (sequence counter, always consume)
                            rp += 2;
                            int rq_authLen = rq_resp[rp++]; rp += rq_authLen;
                            int rq_sessLen = rq_resp[rp++]; rp += rq_sessLen;
                            int rq_corrLen = rq_resp[rp++]; rp += rq_corrLen;
                            int rq_entLen  = rq_resp[rp++]; rp += rq_entLen;

                            // Skip OK / END
                            if (rp + 1 < rq_len && rq_resp[rp] == 'O' && rq_resp[rp+1] == 'K') continue;
                            if (rp + 2 < rq_len && rq_resp[rp] == 'E' && rq_resp[rp+1] == 'N' && rq_resp[rp+2] == 'D') continue;
                            if (!(rp + 3 < rq_len && rq_resp[rp] == 'M' && rq_resp[rp+1] == 'S' && rq_resp[rp+2] == 'G')) continue;

                            // MSG received on Reply Queue
                            rp += 4;
                            uint8_t rq_msgIdLen = rq_resp[rp++];
                            uint8_t rq_msg_id[24] = {0};
                            if (rq_msgIdLen > 24) rq_msgIdLen = 24;
                            memcpy(rq_msg_id, &rq_resp[rp], rq_msgIdLen);
                            rp += rq_msgIdLen;

                            int rq_enc_len = rq_len - rp;
                            ESP_LOGI(TAG, "   Reply Queue MSG received! (%d bytes)", rq_enc_len);

                            // Decrypt + process using extracted modules
                            uint8_t *rq_plain = NULL;
                            size_t rq_plain_len = 0;

                            if (smp_e2e_decrypt_reply_message(&rq_resp[rp], rq_enc_len,
                                    rq_msg_id, rq_msgIdLen, &rq_plain, &rq_plain_len) == 0 && rq_plain) {
                                // Dummy params — auth key already extracted above
                                uint8_t dummy_key[44];
                                bool dummy_auth = false;
                                smp_agent_process_message(rq_plain, rq_plain_len,
                                                            &contacts_db.contacts[0],
                                                            dummy_key, &dummy_auth);
                                free(rq_plain);
                            }

                            queue_send_ack(rq_msg_id, rq_msgIdLen);
                            break;
                        }
                        free(rq_block);
                    }

                    // Send first chat message
                    ESP_LOGI(TAG, "   Sending first chat message in 3 seconds...");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    {
                        contact_t *msg_contact = &contacts_db.contacts[0];
                        if (peer_send_chat_message(msg_contact, "Hello from ESP32!")) {
                            ESP_LOGI(TAG, "   ✅ Chat message sent!");
                        } else {
                            ESP_LOGE(TAG, "   ❌ Chat message send failed!");
                        }
                    }

                    ESP_LOGI(TAG, "   Returning to main loop for incoming messages...");
                    has_peer_sender_auth = false;  // Don't re-trigger 42d

                    skip_42d: ;
                    // 47c Fix 2: Re-subscribe to Q_A after handshake to ensure we receive App messages
                    ESP_LOGW(TAG, "47c: Re-subscribing to Q_A after handshake...");
                    subscribe_all_contacts(&ssl, block, session_id);
                    ESP_LOGW(TAG, "47c: Re-SUB done, waiting for App messages on Q_A...");
                }

                // ACK Reply Queue message on main connection
                smp_send_ack(&ssl, block, session_id,
                             our_queue.rcv_id, our_queue.rcv_id_len,
                             msg_id, msgIdLen,
                             our_queue.rcv_auth_private);
            }

            // ==============================================================
            // CONTACT QUEUE: SMP Decrypt + Agent Parse
            // ==============================================================
            if (contact && contact->have_srv_dh && enc_len > crypto_box_MACBYTES) {
                ESP_LOGW(TAG, "47b: Contact Queue decrypt attempt! contact=%s, enc_len=%d", contact->name, enc_len);
                uint8_t *plain = malloc(enc_len);
                if (plain) {
                    int plain_len = 0;
                    if (decrypt_smp_message(contact, &resp[p], enc_len, msg_id, msgIdLen, plain, &plain_len)) {
                        ESP_LOGI(TAG, "   SMP-Level Decryption OK! (%d bytes)", plain_len);
                        
                        // Extract e2ePubKey from Contact Queue (for reference only)
                        // 47f: Do NOT cache as reply_queue key — Q_B has its own key!
                        if (contact && plain_len > 60) {
                            const uint8_t x25519_spki[] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 
                                                           0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00};
                            
                            if (memcmp(&plain[14], x25519_spki, 12) == 0) {
                                ESP_LOGI(TAG, "   47f: Contact Queue E2E key at offset 14 (NOT caching for reply queue)");
                                ESP_LOGI(TAG, "        Key: %02x%02x%02x%02x...",
                                         plain[26], plain[27], plain[28], plain[29]);
                            } else {
                                for (int i = 0; i < 100 && i < plain_len - 44; i++) {
                                    if (memcmp(&plain[i], x25519_spki, 12) == 0) {
                                        ESP_LOGI(TAG, "   47f: Contact Queue E2E key at offset %d (NOT caching)", i);
                                        break;
                                    }
                                }
                            }
                        }
                        
                        parse_agent_message(contact, plain, plain_len);
                        
                        smp_send_ack(&ssl, block, session_id,
                                     contact->recipient_id, contact->recipient_id_len,
                                     msg_id, msgIdLen,
                                     contact->rcv_auth_secret);
                    } else {
                        ESP_LOGE(TAG, "   Decryption failed!");
                    }
                    free(plain);
                }
            } else if (!is_reply_queue) {
                ESP_LOGW(TAG, "      Cannot decrypt - no contact keys");
            }
            ESP_LOGI(TAG, "");
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
            ESP_LOGE(TAG, "   ERR: %.*s", 
                     (content_len - p > 20) ? 20 : content_len - p, &resp[p]);
        }
        else {
            ESP_LOGW(TAG, "   Unknown: %c%c%c", 
                     (p < content_len) ? resp[p] : '?',
                     (p+1 < content_len) ? resp[p+1] : '?',
                     (p+2 < content_len) ? resp[p+2] : '?');
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "+--------------------------------------+");
    ESP_LOGI(TAG, "|       Session ended                  |");
    ESP_LOGI(TAG, "+--------------------------------------+");

cleanup:
    free(block);
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    if (sock >= 0) close(sock);
}

// ============== App Main ==============

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "SimpleGo v0.1.17-alpha starting...");
    
    if (sodium_init() < 0) {
        ESP_LOGE(TAG, "libsodium init failed!");
        return;
    }
    ESP_LOGI(TAG, "libsodium initialized");

    if (!x448_init()) {
        ESP_LOGE(TAG, "X448 init failed!");
        return;
    }
    ESP_LOGI(TAG, "X448 initialized (wolfSSL)");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Storage Phase 1: NVS only (before display, no SPI conflict)
    smp_storage_init();
    smp_storage_print_info();
    smp_storage_self_test();

    // Auftrag 51b: Frame Pool + Task infrastructure
    frame_pool_init();
    smp_tasks_init();

    // Display + LVGL Init
    ESP_LOGI(TAG, "Initializing Display...");
    ret = tdeck_display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Initializing LVGL...");
        ret = tdeck_lvgl_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LVGL init failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Initializing Touch...");
            ret = tdeck_touch_init();
            if (ret == ESP_OK) {
                tdeck_touch_register_lvgl();
                ESP_LOGI(TAG, "Touch input enabled!");
            } else {
                ESP_LOGW(TAG, "Touch init failed - continuing without touch");
            }
            
            // Auftrag 50d: Keyboard init (I2C bus shared with touch)
            ESP_LOGI(TAG, "Initializing Keyboard...");
            ret = tdeck_keyboard_init();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Keyboard input enabled! ⌨️");
                // Create message queue + background polling task
                kbd_msg_queue = xQueueCreate(4, 256);  // 4 messages, 256 bytes each
                xTaskCreate(keyboard_task, "kbd_task", 4096, NULL, 5, NULL);
            } else {
                ESP_LOGW(TAG, "Keyboard init failed - continuing without keyboard");
            }

            ESP_LOGI(TAG, "Initializing UI...");
            ui_manager_init();
            
            tdeck_lvgl_start();
            
            vTaskDelay(pdMS_TO_TICKS(50));
            tdeck_display_backlight(100);
        }
    }

    // Storage Phase 2: SD card (after display owns SPI bus)
    smp_storage_init_sd();

    smp_wifi_init();

    ESP_LOGI(TAG, "Waiting for WiFi...");
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // ========== Auftrag 50b: Session Restoration or Fresh Start ==========
    if (smp_storage_exists("rat_00") && smp_storage_exists("queue_our")) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
        ESP_LOGI(TAG, "║  📂 RESTORING PREVIOUS SESSION                               ║");
        ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
        ESP_LOGI(TAG, "");

        bool ratchet_ok = ratchet_load_state(0);
        bool queue_ok = queue_load_credentials();
        bool peer_ok = peer_load_state();
        bool hand_ok = handshake_load_state();

        if (ratchet_ok && queue_ok) {
            session_restored = true;
            ESP_LOGI(TAG, "✅ Session restored! Skipping queue creation. (peer=%s, hand=%s)",
                     peer_ok ? "✅" : "⚠️",
                     hand_ok ? "✅" : "⚠️");
        } else {
            ESP_LOGW(TAG, "⚠️ Partial restore failed (ratchet=%d, queue=%d) — fresh start",
                     ratchet_ok, queue_ok);
            session_restored = false;
        }
    } else {
        ESP_LOGI(TAG, "No previous session found — fresh start");
        session_restored = false;
    }

    if (!session_restored) {
        // Create Reply Queue (fresh start only)
        ESP_LOGI(TAG, "Creating reply queue on %s:%d...", SMP_HOST, SMP_PORT);
        
        if (!queue_create(SMP_HOST, SMP_PORT)) {
            ESP_LOGE(TAG, "Failed to create reply queue!");
            ESP_LOGW(TAG, "  Continuing without reply queue...");
        } else {
            ESP_LOGI(TAG, "Reply queue created! sndId: %02x%02x%02x%02x... (%d bytes)",
                     our_queue.snd_id[0], our_queue.snd_id[1],
                     our_queue.snd_id[2], our_queue.snd_id[3],
                     our_queue.snd_id_len);
        }
        
        queue_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    smp_connect();

    // Auftrag 51b: Start FreeRTOS tasks (stubs — real logic in Phase 3)
    smp_tasks_start();
    ESP_LOGI(TAG, "All tasks started — SimpleGo v0.1.17-alpha ready");

    smp_connect();

    ESP_LOGI(TAG, "Done!");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
