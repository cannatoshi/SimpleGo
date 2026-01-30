/**
 * SimpleGo - Native SimpleX SMP Client for ESP32
 * v0.1.17-alpha - AgentConfirmation with Reply Queue
 * github.com/cannatoshi/SimpleGo
 * Autor: cannatoshi
 */

#include <string.h>
#include <stdbool.h>
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
#include "smp_x448.h"
#include "smp_queue.h"

// T-Deck Display Driver
#include "tdeck_display.h"
#include "tdeck_lvgl.h"
#include "tdeck_touch.h"

// UI System
#include "ui_manager.h"
#include "ui/screens/ui_connect.h"
#include "ui_theme.h"

static const char *TAG = "SMP";

// ============== CONFIG ==============
// WiFi credentials from Kconfig (idf.py menuconfig)
// See: Network Configuration -> WiFi SSID/Password
#define SMP_HOST      "smp1.simplexonflux.com"
#define SMP_PORT      5223

// ============== WiFi ==============

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_connected = true;
    }
}

static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id, instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                    &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                    &wifi_event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = { .ssid = CONFIG_SIMPLEGO_WIFI_SSID, .password = CONFIG_SIMPLEGO_WIFI_PASSWORD },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

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
    
    // Fresh start for testing - comment out in production!
    ESP_LOGW(TAG, "      Clearing old contacts for fresh test...");
    clear_all_contacts();
    
    load_contacts_from_nvs();
    
    if (contacts_db.num_contacts == 0) {
        ESP_LOGI(TAG, "      No contacts found - creating 'ESP32'...");
        int idx = add_contact(&ssl, block, session_id, "ESP32");
        if (idx < 0) {
            ESP_LOGE(TAG, "      Failed to create contact!");
            goto cleanup;
        }
        // ADD DEBUG HERE: After Contact erstellen
        ESP_LOGI(TAG, "DEBUG shared_secret check: %02x%02x%02x%02x", 
                 our_queue.shared_secret[0], our_queue.shared_secret[1],
                 our_queue.shared_secret[2], our_queue.shared_secret[3]);
    } else {
        ESP_LOGI(TAG, "      %d contact(s) loaded from NVS", contacts_db.num_contacts);
    }
    
    list_contacts();
    
    // ========== Step 5: Subscribe All Contacts ==========
    ESP_LOGI(TAG, "[5/5] Subscribing to queues...");
    subscribe_all_contacts(&ssl, block, session_id);
    // ADD DEBUG HERE: After dem SUB senden
    ESP_LOGI(TAG, "DEBUG shared_secret check: %02x%02x%02x%02x", 
             our_queue.shared_secret[0], our_queue.shared_secret[1],
             our_queue.shared_secret[2], our_queue.shared_secret[3]);
    
    // Print connection info
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
        content_len = smp_read_block(&ssl, block, 60000);
        
        if (content_len == -2) {
            ESP_LOGI(TAG, "   ... still waiting ...");
            continue;
        }
        
        if (content_len < 0) {
            ESP_LOGW(TAG, "   Connection closed");
            break;
        }
        
        uint8_t *resp = block + 2;
        
        // Parse transport format
        int p = 0;
        if (resp[p] != 1) {
            ESP_LOGW(TAG, "   Unexpected txCount: %d", resp[p]);
            continue;
        }
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
            ESP_LOGI(TAG, "   DEBUG: msgIdLen = %d", msgIdLen);  // ADD THIS!
            uint8_t msg_id[24];
            memset(msg_id, 0, 24);
            if (msgIdLen > 24) msgIdLen = 24;
            memcpy(msg_id, &resp[p], msgIdLen);
            p += msgIdLen;
            
            int enc_len = content_len - p;
            
            // ADD THIS DEBUG:
            ESP_LOGI(TAG, "   DEBUG: content_len=%d, p=%d, enc_len=%d", content_len, p, enc_len);
            ESP_LOGI(TAG, "   DEBUG: bytes at p-4 to p+20:");
            printf("      ");
            for (int i = -4; i < 20 && (p+i) >= 0 && (p+i) < content_len; i++) {
                if (i == 0) printf("| ");  // Mark where p starts
                printf("%02x ", resp[p + i]);
            }
            printf("\n");
            
            if (contact) {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "+----------------------------------------------------------+");
                ESP_LOGI(TAG, "|   MESSAGE RECEIVED for [%s]!", contact->name);
                ESP_LOGI(TAG, "+----------------------------------------------------------+");
            } else {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "   MESSAGE (unknown contact)!");
            }
            ESP_LOGI(TAG, "   MsgId: %02x%02x%02x%02x...", msg_id[0], msg_id[1], msg_id[2], msg_id[3]);
            ESP_LOGI(TAG, "   Encrypted: %d bytes", enc_len);

            // ADD THIS DEBUG LINE:
            ESP_LOGI(TAG, "   Raw bytes at p: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                     resp[p], resp[p+1], resp[p+2], resp[p+3], 
                     resp[p+4], resp[p+5], resp[p+6], resp[p+7],
                     resp[p+8], resp[p+9], resp[p+10], resp[p+11],
                     resp[p+12], resp[p+13], resp[p+14], resp[p+15]);

            // ADD DEBUG HERE: Beim Message-Empfang (vor dem Decrypt)
            ESP_LOGI(TAG, "DEBUG shared_secret check: %02x%02x%02x%02x", 
                     our_queue.shared_secret[0], our_queue.shared_secret[1],
                     our_queue.shared_secret[2], our_queue.shared_secret[3]);

            // === REPLY QUEUE DECRYPTION ===
            if (is_reply_queue && our_queue.valid && enc_len > crypto_box_MACBYTES) {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "   🔓 Decrypting REPLY QUEUE message...");
                
                // Prepare nonce (24 bytes)
                uint8_t server_nonce[24];
                memset(server_nonce, 0, 24);
                memcpy(server_nonce, msg_id, msgIdLen);
                
                uint8_t *server_plain = malloc(enc_len);
                if (server_plain) {
                    // Layer 1: Server-level decrypt
                    if (crypto_box_open_easy_afternm(server_plain, &resp[p], enc_len, 
                                                      server_nonce, our_queue.shared_secret) == 0) {
                        int plain_len = enc_len - crypto_box_MACBYTES;
                        ESP_LOGI(TAG, "      ✅ Server-level decrypt SUCCESS! (%d bytes)", plain_len);
                        // === DETAILED DEBUG ===
                        ESP_LOGI(TAG, "      📊 First 64 bytes after server-decrypt:");
                        printf("         ");
                        for (int i = 0; i < 64 && i < plain_len; i++) {
                            printf("%02x ", server_plain[i]);
                            if ((i + 1) % 16 == 0) printf("\n         ");
                        }
                        printf("\n");
                        
                        // Parse the structure
                        ESP_LOGI(TAG, "      📋 Structure analysis:");
                        uint16_t len_prefix = (server_plain[0] << 8) | server_plain[1];
                        ESP_LOGI(TAG, "         [0-1] Length prefix: %d (0x%04x)", len_prefix, len_prefix);
                        ESP_LOGI(TAG, "         [2-5] Unknown: %02x %02x %02x %02x", 
                                 server_plain[2], server_plain[3], server_plain[4], server_plain[5]);
                        ESP_LOGI(TAG, "         [6-9] Unknown: %02x %02x %02x %02x (timestamp?)",
                                 server_plain[6], server_plain[7], server_plain[8], server_plain[9]);
                        ESP_LOGI(TAG, "         [10-13]: %02x %02x %02x %02x",
                                 server_plain[10], server_plain[11], server_plain[12], server_plain[13]);
                        
                        // Check for version marker at different offsets
                        for (int i = 0; i < 20; i++) {
                            if (server_plain[i] == 0x30 && server_plain[i+1] == 0x2a) {
                                ESP_LOGI(TAG, "         🔑 X25519 SPKI at offset %d", i);
                            }
                            if (server_plain[i] == 0x00 && server_plain[i+1] == 0x07) {
                                ESP_LOGI(TAG, "         📦 Agent version 7 marker at offset %d", i);
                            }
                        }
                        // === END DETAILED DEBUG ===
                        
// ========== TEST 4: CORRECT - Handle ClientMsgEnvelope structure ==========
ESP_LOGI(TAG, "      TEST4: Analyzing ClientMsgEnvelope structure...");

// ClientMsgEnvelope structure (after server-level decrypt):
// [0-1]   Length prefix (BE)
// [2-9]   Padding/timestamp (8 bytes)  
// [10-11] Version/flags
// [12-13] More flags
// [14]    maybe_corrId tag ('1' = Just, ',' = Nothing)
// [15]    maybe_e2e tag ('1' = Just with SPKI+nonce, ',' = Nothing)
// 
// If maybe_corrId == '1': next 44 bytes = corrId SPKI
// If maybe_e2e == '1': next 44 bytes = e2ePubKey SPKI, then 24 bytes nonce, then cmEncBody
// If maybe_e2e == ',': EncRatchetMessage follows directly!

int offset = 14;  // Start at maybe tags

uint8_t maybe_corrId = server_plain[offset];
uint8_t maybe_e2e = server_plain[offset + 1];

ESP_LOGI(TAG, "         maybe_corrId = '%c' (0x%02x) - %s", 
         maybe_corrId, maybe_corrId,
         maybe_corrId == '1' ? "Just (has corrId SPKI)" : "Nothing");
ESP_LOGI(TAG, "         maybe_e2e = '%c' (0x%02x) - %s",
         maybe_e2e, maybe_e2e,
         maybe_e2e == '1' ? "Just (has e2ePubKey)" : "Nothing - DIRECT TO RATCHET!");

// === BRUTE FORCE: Try all possible EncRatchetMessage offsets ===
ESP_LOGI(TAG, "      🔬 BRUTE FORCE: Testing offsets 56-150 for valid decrypt...");
for (int test_off = 56; test_off < 150 && test_off < plain_len - 140; test_off++) {
    // Skip if first byte after length would give invalid version
    uint8_t first = server_plain[test_off];
    
    // Could be length prefix 0x7b OR start of emHeader directly
    const uint8_t *test_data;
    int data_offset;
    
    if (first == 0x7b) {
        // Standard format with length prefix
        test_data = &server_plain[test_off + 1];
        data_offset = test_off + 1;
    } else {
        // Try without length prefix
        test_data = &server_plain[test_off];
        data_offset = test_off;
    }
    
    // Check if looks like valid emHeader (version should be 2 or 3)
    uint16_t version = (test_data[0] << 8) | test_data[1];
    if (version >= 1 && version <= 5) {
        ESP_LOGI(TAG, "         ✓ Offset %d: possible v%d header! first=0x%02x", 
                 test_off, version, first);
        ESP_LOGI(TAG, "           Bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
                 server_plain[test_off], server_plain[test_off+1],
                 server_plain[test_off+2], server_plain[test_off+3],
                 server_plain[test_off+4], server_plain[test_off+5],
                 server_plain[test_off+6], server_plain[test_off+7]);
    }
}

// === TEST: Try E2E DH decrypt with peer's e2e key from message ===
ESP_LOGI(TAG, "      🔬 TEST: Trying DH decrypt with E2E keys...");

// Peer's E2E public key is at offset 16 (SPKI) -> raw key at offset 28
uint8_t peer_e2e_pub[32];
// Use peer's DH key from INVITATION (not from message!)
ESP_LOGI(TAG, "         Saved peer DH: %02x%02x%02x%02x...",
         pending_peer.dh_public[0], pending_peer.dh_public[1], pending_peer.dh_public[2], pending_peer.dh_public[3]);

// Compute E2E DH secret: peer_e2e_pub * our_queue.e2e_private
uint8_t e2e_dh_secret[32];
crypto_box_beforenm(e2e_dh_secret, pending_peer.dh_public, our_queue.e2e_private);
ESP_LOGI(TAG, "         E2E DH secret: %02x%02x%02x%02x...",
         e2e_dh_secret[0], e2e_dh_secret[1], e2e_dh_secret[2], e2e_dh_secret[3]);

// Nonce at offset 60-83
uint8_t rq_nonce[24];
memcpy(rq_nonce, &server_plain[60], 24);
ESP_LOGI(TAG, "         Nonce: %02x %02x %02x %02x...",
         rq_nonce[0], rq_nonce[1], rq_nonce[2], rq_nonce[3]);

// Encrypted data starts at offset 84
const uint8_t *rq_enc = &server_plain[84];
size_t rq_enc_len = plain_len - 84 - 16;  // minus MAC
ESP_LOGI(TAG, "         Encrypted len: %zu", rq_enc_len);

// Decrypt with E2E DH secret
uint8_t *rq_plain = malloc(rq_enc_len);
if (rq_plain) {
    int rq_ret = crypto_box_open_afternm(rq_plain, rq_enc, rq_enc_len, rq_nonce,
                                          e2e_dh_secret);
    if (rq_ret == 0) {
        ESP_LOGI(TAG, "      🎉 E2E DECRYPT SUCCESS!");
        ESP_LOGI(TAG, "         First 20 bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                 rq_plain[0], rq_plain[1], rq_plain[2], rq_plain[3],
                 rq_plain[4], rq_plain[5], rq_plain[6], rq_plain[7],
                 rq_plain[8], rq_plain[9]);
        // Check for 0x7b 00 02 (EncRatchetMessage v2)
        if (rq_plain[0] == 0x7b && rq_plain[1] == 0x00 && rq_plain[2] == 0x02) {
            ESP_LOGI(TAG, "      🎯 FOUND EncRatchetMessage v2 header!");
        }
    } else {
        ESP_LOGE(TAG, "      ❌ E2E decrypt failed (ret=%d)", rq_ret);
    }
    free(rq_plain);
}

// Also check if there's AgentMsgEnvelope header (00 07 XX) before EncRatchetMessage
ESP_LOGI(TAG, "      🔎 Searching for AgentMsgEnvelope (00 07) before EncRatchetMessage...");
for (int i = 56; i < 150 && i < plain_len - 5; i++) {
    if (server_plain[i] == 0x00 && server_plain[i+1] == 0x07) {
        ESP_LOGI(TAG, "         ✓ Found '00 07' at offset %d, next bytes: %02x %02x %02x",
                 i, server_plain[i+2], server_plain[i+3], server_plain[i+4]);
        // If followed by 7b, that's our EncRatchetMessage!
        if (server_plain[i+3] == 0x7b || server_plain[i+2] == 0x7b) {
            ESP_LOGI(TAG, "         🎯 EncRatchetMessage likely at offset %d!", 
                     server_plain[i+2] == 0x7b ? i+2 : i+3);
        }
    }
}

offset += 2;  // Skip both maybe tags -> offset = 16

// Skip corrId SPKI if present
if (maybe_corrId == '1') {
    ESP_LOGI(TAG, "         Skipping corrId SPKI (44 bytes) at offset %d", offset);
    offset += 44;  // -> offset = 60
}

ESP_LOGI(TAG, "      📊 FULL HEX DUMP bytes 0-200:");
for (int row = 0; row < 200; row += 16) {
    printf("         %04d: ", row);
    for (int col = 0; col < 16 && (row + col) < plain_len; col++) {
        printf("%02x ", server_plain[row + col]);
    }
    printf(" | ");
    for (int col = 0; col < 16 && (row + col) < plain_len; col++) {
        uint8_t c = server_plain[row + col];
        printf("%c", (c >= 0x20 && c < 0x7f) ? c : '.');
    }
    printf("\n");
}

// Auch: Suche nach Agent Version marker "00 07" (Agent v7)
ESP_LOGI(TAG, "      🔎 Searching for Agent v7 marker (00 07)...");
for (int i = 60; i < 250 && i < plain_len - 2; i++) {
    if (server_plain[i] == 0x00 && server_plain[i+1] == 0x07) {
        ESP_LOGI(TAG, "         ✓ Found '00 07' at offset %d", i);
        ESP_LOGI(TAG, "           Context: %02x %02x [%02x %02x] %02x %02x %02x %02x",
                 server_plain[i-2], server_plain[i-1],
                 server_plain[i], server_plain[i+1],
                 server_plain[i+2], server_plain[i+3],
                 server_plain[i+4], server_plain[i+5]);
    }
}

// Suche nach v3 EncRatchetMessage (0x00 0x7B für Large encoding mit len=123)
ESP_LOGI(TAG, "      🔎 Searching for v3 markers...");
for (int i = 60; i < plain_len - 5; i++) {
    // v3 might use 00 03 as version after length
    if (server_plain[i] < 0x20 && server_plain[i+1] < 0x90) {
        uint16_t len = (server_plain[i] << 8) | server_plain[i+1];
        if (len >= 120 && len <= 130) {
            if (server_plain[i+2] == 0x00 && server_plain[i+3] == 0x03) {
                ESP_LOGI(TAG, "         ✓ Possible v3 at offset %d! len=%d", i, len);
            }
        }
    }
}

// CRITICAL DEBUG: Find where 0x7b actually is!
ESP_LOGI(TAG, "      🔎 Searching for EncRatchetMessage start (0x7b)...");
int found_7b = -1;
for (int search = 14; search < plain_len - 140; search++) {
    if (server_plain[search] == 0x7B) {
        // Check if this looks like v2 EncRatchetMessage
        // Next byte should be version high (0x00), then version low (0x02)
        if (server_plain[search + 1] == 0x00 && server_plain[search + 2] == 0x02) {
            ESP_LOGI(TAG, "         ✓ Found v2 EncRatchetMessage at offset %d!", search);
            ESP_LOGI(TAG, "           Bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
                     server_plain[search], server_plain[search+1], 
                     server_plain[search+2], server_plain[search+3],
                     server_plain[search+4], server_plain[search+5],
                     server_plain[search+6], server_plain[search+7]);
            found_7b = search;
            break;
        } else {
            ESP_LOGI(TAG, "         ? 0x7b at offset %d but not v2 header (next: %02x %02x)",
                     search, server_plain[search+1], server_plain[search+2]);
        }
    }
}

// Search for Large encoding: 00 7b 00 02 (2-byte prefix for length 123)
ESP_LOGI(TAG, "      🔎 Searching for Large encoding (00 7b 00 02)...");
for (int i = 60; i < plain_len - 4; i++) {
    if (server_plain[i] == 0x00 && server_plain[i+1] == 0x7b &&
        server_plain[i+2] == 0x00 && server_plain[i+3] == 0x02) {
        ESP_LOGI(TAG, "         ✓ Found Large encoding v2 at offset %d!", i);
        ESP_LOGI(TAG, "           Context: %02x %02x %02x %02x %02x %02x",
                 server_plain[i], server_plain[i+1], server_plain[i+2],
                 server_plain[i+3], server_plain[i+4], server_plain[i+5]);
    }
}

// Also try EVERY offset from 58-80 to see which gives valid header decrypt
ESP_LOGI(TAG, "      🔎 Brute-force trying offsets 58-80...");
for (int test_offset = 58; test_offset <= 80; test_offset++) {
    uint8_t first = server_plain[test_offset];
    uint8_t second = server_plain[test_offset + 1];
    // Check for 1-byte prefix (0x7b) or reasonable 2-byte prefix
    if (first == 0x7b || (first == 0x00 && second == 0x7b)) {
        ESP_LOGI(TAG, "         ✓ Potential EncRatchetMessage at offset %d: %02x %02x %02x %02x",
                 test_offset, server_plain[test_offset], server_plain[test_offset+1],
                 server_plain[test_offset+2], server_plain[test_offset+3]);
    }
}

// Also search for Large encoding (2-byte length where first byte < 0x20)
if (found_7b < 0) {
    ESP_LOGI(TAG, "      🔎 Searching for Large encoding EncRatchetMessage...");
    for (int search = 14; search < plain_len - 140; search++) {
        if (server_plain[search] < 0x20) {
            uint16_t len = (server_plain[search] << 8) | server_plain[search + 1];
            // Check if reasonable length (100-200 for emHeader)
            if (len >= 100 && len <= 200) {
                // Check version after length
                if (server_plain[search + 2] == 0x00 && 
                    (server_plain[search + 3] == 0x02 || server_plain[search + 3] == 0x03)) {
                    ESP_LOGI(TAG, "         ✓ Found Large encoding at offset %d! emHeader len=%d, version=%d",
                             search, len, server_plain[search + 3]);
                    found_7b = search;
                    break;
                }
            }
        }
    }
}

if (found_7b < 0) {
    ESP_LOGE(TAG, "      ❌ Could not find EncRatchetMessage start!");
    ESP_LOGI(TAG, "         Dumping bytes 60-120:");
    printf("            ");
    for (int i = 60; i < 120 && i < plain_len; i++) {
        printf("%02x ", server_plain[i]);
        if ((i - 59) % 16 == 0) printf("\n            ");
    }
    printf("\n");
}


if (maybe_e2e == '1') {
    // === CASE 1: Has per-queue E2E layer (Contact Queue style) ===
    ESP_LOGI(TAG, "         📦 Per-queue E2E layer present");
    
    int spki_offset = offset;
    int cm_nonce_offset = spki_offset + 44;
    int cm_enc_body_offset = cm_nonce_offset + 24;
    
    if (plain_len > cm_enc_body_offset + 16) {
        uint8_t cm_nonce[24];
        memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);
        ESP_LOGI(TAG, "         cmNonce: %02x %02x %02x %02x...",
                 cm_nonce[0], cm_nonce[1], cm_nonce[2], cm_nonce[3]);

        uint8_t peer_e2e_pub[32];
        memcpy(peer_e2e_pub, &server_plain[spki_offset + 12], 32);
        ESP_LOGI(TAG, "         peer_e2ePub: %02x %02x %02x %02x...",
                 peer_e2e_pub[0], peer_e2e_pub[1], peer_e2e_pub[2], peer_e2e_pub[3]);

        uint8_t e2e_dh_secret[32];
        crypto_box_beforenm(e2e_dh_secret, peer_e2e_pub, our_queue.rcv_dh_private);

        int cm_body_len = plain_len - cm_enc_body_offset;
        uint8_t *cm_plain = malloc(cm_body_len);
        if (cm_plain) {
            if (crypto_box_open_easy_afternm(cm_plain, &server_plain[cm_enc_body_offset], cm_body_len,
                                              cm_nonce, e2e_dh_secret) == 0) {
                int cm_plain_len = cm_body_len - crypto_box_MACBYTES;
                ESP_LOGI(TAG, "         ✅ Per-queue E2E decrypt SUCCESS! (%d bytes)", cm_plain_len);
                
                // Show first bytes of ClientMessage
                ESP_LOGI(TAG, "         First 32 bytes of ClientMessage:");
                printf("            ");
                for (int i = 0; i < 32 && i < cm_plain_len; i++) printf("%02x ", cm_plain[i]);
                printf("\n");
                
                // Parse AgentConfirmation and extract Reply Queue E2E key
                if (cm_plain_len > 4 && ratchet_is_initialized()) {
                    if (parse_agent_confirmation(cm_plain, cm_plain_len) == 0) {
                        ESP_LOGI(TAG, "         ✅ Reply Queue E2E key extracted!");
                    } else {
                        ESP_LOGW(TAG, "         ⚠️ Could not extract Reply Queue E2E key");
                    }
                } else if (!ratchet_is_initialized()) {
                    ESP_LOGW(TAG, "         ⚠️ Ratchet not initialized, cannot parse AgentConfirmation");
                }
            } else {
                ESP_LOGE(TAG, "         ❌ Per-queue E2E decrypt FAILED");
            }
            free(cm_plain);
        }
    }
    
} else if (maybe_e2e == ',') {
    // === KORRIGIERTE INTERPRETATION ===
    // [14] = '1' = phE2ePubDhKey = Just
    // [15] = 0x2c = 44 = SPKI Länge  
    // [16-59] = X25519 SPKI (44 bytes)
    // [60-83] = cmNonce (24 bytes)
    // [84+] = cmEncBody
    
    ESP_LOGI(TAG, "      🔧 KORRIGIERTE INTERPRETATION:");
    ESP_LOGI(TAG, "         [14] = 0x%02x - prüfe ob '1' (Just) oder '0' (Nothing)", server_plain[14]);
    
    if (server_plain[14] == '1') {
        uint8_t spki_len = server_plain[15];
        ESP_LOGI(TAG, "         [15] = %d = SPKI Länge", spki_len);
        
        if (spki_len == 44) {
            // ⭐ DETAILLIERTE OFFSET-ANALYSE
            ESP_LOGI(TAG, "         📊 OFFSET ANALYSIS:");
            ESP_LOGI(TAG, "            [14] Maybe tag: 0x%02x ('%c')", server_plain[14], server_plain[14]);
            ESP_LOGI(TAG, "            [15] SPKI len:  0x%02x (%d)", server_plain[15], server_plain[15]);
            
            // SPKI Header sollte sein: 30 2a 30 05 06 03 2b 65 6e 03 21 00
            ESP_LOGI(TAG, "            [16-27] SPKI header:");
            printf("               ");
            for (int i = 16; i < 28; i++) printf("%02x ", server_plain[i]);
            printf("\n");
            
            // Raw key bei [28-59]
            ESP_LOGI(TAG, "            [28-59] Raw X25519 key:");
            printf("               ");
            for (int i = 28; i < 60; i++) printf("%02x ", server_plain[i]);
            printf("\n");
            
            // Nonce bei [60-83]
            ESP_LOGI(TAG, "            [60-83] cmNonce:");
            printf("               ");
            for (int i = 60; i < 84; i++) printf("%02x ", server_plain[i]);
            printf("\n");
            
            // Erste bytes von cmEncBody
            ESP_LOGI(TAG, "            [84-99] cmEncBody start:");
            printf("               ");
            for (int i = 84; i < 100 && i < plain_len; i++) printf("%02x ", server_plain[i]);
            printf("\n");

            // ⭐ VERIFIZIERE KEYPAIR
            uint8_t derived_pub[32];
            crypto_scalarmult_base(derived_pub, our_queue.e2e_private);
            
            ESP_LOGI(TAG, "         🔍 KEYPAIR CHECK:");
            ESP_LOGI(TAG, "            our_queue.e2e_public:  %02x%02x%02x%02x...",
                    our_queue.e2e_public[0], our_queue.e2e_public[1],
                    our_queue.e2e_public[2], our_queue.e2e_public[3]);
            ESP_LOGI(TAG, "            derived from private:  %02x%02x%02x%02x...",
                    derived_pub[0], derived_pub[1], derived_pub[2], derived_pub[3]);
            
            if (memcmp(derived_pub, our_queue.e2e_public, 32) != 0) {
                ESP_LOGE(TAG, "         ❌ E2E KEYPAIR MISMATCH! Keys were overwritten!");
            } else {
                ESP_LOGI(TAG, "         ✅ E2E keypair verified - matches!");
            }
            
            // ⭐ FIX Session 14 v3: crypto_secretbox_open_detached
            // Haskell: [MAC 16 bytes][Ciphertext]
            // libsodium detached: MAC und Ciphertext separat
            
            // 1. Peer public key aus Nachricht [28-59]
            uint8_t e2e_peer_public[32];
            memcpy(e2e_peer_public, &server_plain[28], 32);
            ESP_LOGI(TAG, "         e2ePubKey: %02x%02x%02x%02x...",
                     e2e_peer_public[0], e2e_peer_public[1],
                     e2e_peer_public[2], e2e_peer_public[3]);
            
            // 2. Nonce aus [60-83]
            uint8_t cm_nonce[24];
            memcpy(cm_nonce, &server_plain[60], 24);
            ESP_LOGI(TAG, "         cmNonce: %02x%02x%02x%02x...",
                     cm_nonce[0], cm_nonce[1], cm_nonce[2], cm_nonce[3]);
            
            // 3. cmEncBody bei [84+]
            int cm_enc_offset = 84;
            int cm_enc_len = plain_len - cm_enc_offset;
            ESP_LOGI(TAG, "         cmEncBody offset: %d, len: %d", cm_enc_offset, cm_enc_len);
            
            // 4. Raw DH (OHNE HSalsa20!)
            uint8_t dh_secret[32];
            crypto_scalarmult(dh_secret, our_queue.e2e_private, e2e_peer_public);
            ESP_LOGI(TAG, "         DH secret: %02x%02x%02x%02x...",
                     dh_secret[0], dh_secret[1], dh_secret[2], dh_secret[3]);
            
            // 5. Haskell format: [MAC 16][Ciphertext]
            const uint8_t *mac = &server_plain[cm_enc_offset];          // erste 16 bytes = MAC
            const uint8_t *ciphertext = &server_plain[cm_enc_offset + 16];  // rest = ciphertext
            int ciphertext_len = cm_enc_len - 16;
            
            ESP_LOGI(TAG, "         MAC: %02x%02x%02x%02x...",
                     mac[0], mac[1], mac[2], mac[3]);
            ESP_LOGI(TAG, "         Ciphertext len: %d", ciphertext_len);
            ESP_LOGI(TAG, "         Trying crypto_secretbox_open_detached...");
            
            // 6. Decrypt mit crypto_secretbox_open_detached
            uint8_t *cm_plain = malloc(ciphertext_len);
            if (cm_plain) {
                int ret = crypto_secretbox_open_detached(
                    cm_plain,       // output
                    ciphertext,     // ciphertext (NACH dem MAC)
                    mac,            // MAC (erste 16 bytes)
                    ciphertext_len, // nur ciphertext länge
                    cm_nonce,
                    dh_secret
                );
                
                if (ret == 0) {
                    ESP_LOGI(TAG, "      ╔══════════════════════════════════════════════╗");
                    ESP_LOGI(TAG, "      ║  ✅ PER-QUEUE E2E DECRYPT SUCCESS!          ║");
                    ESP_LOGI(TAG, "      ╚══════════════════════════════════════════════╝");
                    ESP_LOGI(TAG, "         Decrypted: %d bytes", ciphertext_len);
                    printf("         First 32: ");
                    for (int i = 0; i < 32 && i < ciphertext_len; i++) printf("%02x ", cm_plain[i]);
                    printf("\n");
                    
                    // Parse PrivHeader + AgentMsgEnvelope...
                    if (ciphertext_len > 0) {
                        char priv_header_tag = cm_plain[0];
                        ESP_LOGI(TAG, "         PrivHeader: '%c' (0x%02x)", priv_header_tag, priv_header_tag);
                        
                        int agent_msg_offset = 1;
                        if (priv_header_tag == 'K') {
                            uint8_t sender_key_len = cm_plain[1];
                            agent_msg_offset = 2 + sender_key_len;
                            ESP_LOGI(TAG, "         SenderKey len: %d", sender_key_len);
                        }
                        
                        if (agent_msg_offset < ciphertext_len - 4) {
                            uint8_t *agent_msg = &cm_plain[agent_msg_offset];
                            
                            ESP_LOGI(TAG, "         AgentMsg first bytes: %02x %02x %02x %02x",
                                     agent_msg[0], agent_msg[1], agent_msg[2], agent_msg[3]);
                            
                            uint16_t agent_version = (agent_msg[0] << 8) | agent_msg[1];
                            char msg_type = agent_msg[2];
                            
                            ESP_LOGI(TAG, "         Agent Version: %d, Type: '%c'", agent_version, msg_type);
                            
                            if (msg_type == 'C') {
                                ESP_LOGI(TAG, "         📬 AgentConfirmation received!");
                            } else if (msg_type == 'M') {
                                ESP_LOGI(TAG, "         📨 AgentMsgEnvelope received!");
                            }
                        }
                    }
                    
                } else {
                    ESP_LOGE(TAG, "      ❌ Per-queue E2E decrypt FAILED (ret=%d)", ret);
                }
                free(cm_plain);
            }
        } else {
            ESP_LOGE(TAG, "         ❌ Unexpected SPKI length: %d (expected 44)", spki_len);
        }
    } else if (server_plain[14] == '0') {
        ESP_LOGI(TAG, "         phE2ePubDhKey = Nothing (pre-computed secret)");
        ESP_LOGE(TAG, "         ❌ Pre-computed secret not yet implemented!");
    } else {
        ESP_LOGE(TAG, "         ❌ Unknown Maybe tag: 0x%02x", server_plain[14]);
    }
}

// === END TEST4 ===

                    } else {
                        ESP_LOGE(TAG, "      ❌ Server-level decrypt FAILED!");
                    }
                    free(server_plain);
                }
            }
            // === END REPLY QUEUE DECRYPTION ===

            // Decrypt
            if (contact && contact->have_srv_dh && enc_len > crypto_box_MACBYTES) {
                uint8_t *plain = malloc(enc_len);
                if (plain) {
                    int plain_len = 0;
                    if (decrypt_smp_message(contact, &resp[p], enc_len, msg_id, msgIdLen, plain, &plain_len)) {
                        ESP_LOGI(TAG, "   SMP-Level Decryption OK! (%d bytes)", plain_len);
                        
                        // === DEBUG: HEX-DUMP of the decrypted Agent message ===
                        ESP_LOGI(TAG, "");
                        ESP_LOGI(TAG, "   RAW AGENT MESSAGE (first 60 bytes):");
                        printf("   ");
                        for (int i = 0; i < plain_len && i < 60; i++) {
                            printf("%02x ", plain[i]);
                        }
                        printf("\n");
                        ESP_LOGI(TAG, "   === END DEBUG ===");
                        ESP_LOGI(TAG, "");
                        // === CONTACT QUEUE: Extract e2ePubKey for Reply Queue ===
                        if (contact && plain_len > 60) {
                            int offset = 14;
                            uint8_t maybe_corrId = plain[offset];
                            uint8_t maybe_e2e = plain[offset + 1];
                            offset += 2;
                            
                            if (maybe_corrId == '1') offset += 44;
                            
                            if (maybe_e2e == '1' && !reply_queue_e2e_peer_valid) {
                                if (offset + 44 <= plain_len && 
                                    plain[offset] == 0x30 && plain[offset+1] == 0x2a) {
                                    
                                    ESP_LOGI(TAG, "   🔑 Found e2ePubKey in PubHeader at offset %d!", offset);
                                    memcpy(reply_queue_e2e_peer_public, &plain[offset + 12], 32);
                                    reply_queue_e2e_peer_valid = true;
                                    
                                    ESP_LOGI(TAG, "   ╔═══════════════════════════════════════════════════════╗");
                                    ESP_LOGI(TAG, "   ║  🎉 REPLY QUEUE E2E KEY EXTRACTED!                    ║");
                                    ESP_LOGI(TAG, "   ╚═══════════════════════════════════════════════════════╝");
                                    ESP_LOGI(TAG, "      Key: %02x%02x%02x%02x%02x%02x%02x%02x...",
                                            reply_queue_e2e_peer_public[0], reply_queue_e2e_peer_public[1],
                                            reply_queue_e2e_peer_public[2], reply_queue_e2e_peer_public[3],
                                            reply_queue_e2e_peer_public[4], reply_queue_e2e_peer_public[5],
                                            reply_queue_e2e_peer_public[6], reply_queue_e2e_peer_public[7]);
                                }
                            }
                        }
                        // Parse agent message
                        parse_agent_message(contact, plain, plain_len);
                        
                        // Send ACK
                        ESP_LOGI(TAG, "   Sending ACK...");
                        
                        uint8_t ack_body[64];
                        int ap = 0;
                        ack_body[ap++] = 1;
                        ack_body[ap++] = 'A';
                        ack_body[ap++] = contact->recipient_id_len;
                        memcpy(&ack_body[ap], contact->recipient_id, contact->recipient_id_len);
                        ap += contact->recipient_id_len;
                        ack_body[ap++] = 'A';
                        ack_body[ap++] = 'C';
                        ack_body[ap++] = 'K';
                        ack_body[ap++] = ' ';
                        ack_body[ap++] = msgIdLen;
                        memcpy(&ack_body[ap], msg_id, msgIdLen);
                        ap += msgIdLen;
                        
                        uint8_t ack_to_sign[128];
                        int ack_sign_pos = 0;
                        ack_to_sign[ack_sign_pos++] = 32;
                        memcpy(&ack_to_sign[ack_sign_pos], session_id, 32);
                        ack_sign_pos += 32;
                        memcpy(&ack_to_sign[ack_sign_pos], ack_body, ap);
                        ack_sign_pos += ap;
                        
                        uint8_t ack_sig[crypto_sign_BYTES];
                        crypto_sign_detached(ack_sig, NULL, ack_to_sign, ack_sign_pos, contact->rcv_auth_secret);
                        
                        uint8_t ack_trans[192];
                        int atp = 0;
                        ack_trans[atp++] = crypto_sign_BYTES;
                        memcpy(&ack_trans[atp], ack_sig, crypto_sign_BYTES);
                        atp += crypto_sign_BYTES;
                        ack_trans[atp++] = 32;
                        memcpy(&ack_trans[atp], session_id, 32);
                        atp += 32;
                        memcpy(&ack_trans[atp], ack_body, ap);
                        atp += ap;
                        
                        smp_write_command_block(&ssl, block, ack_trans, atp);
                    } else {
                        ESP_LOGE(TAG, "   Decryption failed!");
                    }
                    free(plain);
                }
            } else {
                ESP_LOGW(TAG, "      Cannot decrypt - no contact keys and ratchet not initialized");
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
    
    // Initialize libsodium
    if (sodium_init() < 0) {
        ESP_LOGE(TAG, "libsodium init failed!");
        return;
    }
    ESP_LOGI(TAG, "libsodium initialized");

    // Initialize X448 crypto
    if (!x448_init()) {
        ESP_LOGE(TAG, "X448 init failed!");
        return;
    }
    ESP_LOGI(TAG, "X448 initialized (wolfSSL)");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ========== Display + LVGL Init ==========
    ESP_LOGI(TAG, "");
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
            // Initialize Touch
            ESP_LOGI(TAG, "Initializing Touch...");
            ret = tdeck_touch_init();
            if (ret == ESP_OK) {
                tdeck_touch_register_lvgl();
                ESP_LOGI(TAG, "Touch input enabled!");
            } else {
                ESP_LOGW(TAG, "Touch init failed - continuing without touch");
            }
            
            // Initialize UI system
            ESP_LOGI(TAG, "Initializing UI...");
            ui_manager_init();
            tdeck_lvgl_start();  // Start rendering AFTER splash is loaded
            
            // Wait briefly until splash is rendered, THEN turn on backlight
            vTaskDelay(pdMS_TO_TICKS(50));
            tdeck_display_backlight(100);
        }
    }
    ESP_LOGI(TAG, "");
    wifi_init();

    // Wait for WiFi connection
    ESP_LOGI(TAG, "Waiting for WiFi...");
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // ========== Step 0: Create our Reply Queue ==========
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "=================================================================");
    ESP_LOGI(TAG, "  STEP 0: Creating our reply queue on %s:%d", SMP_HOST, SMP_PORT);
    ESP_LOGI(TAG, "=================================================================");
    
    if (!queue_create(SMP_HOST, SMP_PORT)) {
        ESP_LOGE(TAG, "Failed to create reply queue!");
        ESP_LOGW(TAG, "  Continuing without reply queue...");
    } else {
        ESP_LOGI(TAG, "Reply queue created!");
        ESP_LOGI(TAG, "   sndId: %02x%02x%02x%02x... (%d bytes)",
                 our_queue.snd_id[0], our_queue.snd_id[1],
                 our_queue.snd_id[2], our_queue.snd_id[3],
                 our_queue.snd_id_len);
        // ADD DEBUG HERE: After queue_create() return in main.c
        ESP_LOGI(TAG, "DEBUG shared_secret check: %02x%02x%02x%02x", 
                 our_queue.shared_secret[0], our_queue.shared_secret[1],
                 our_queue.shared_secret[2], our_queue.shared_secret[3]);
    }
    
    // Close queue connection - main connection will be separate
    queue_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500));

    // Main SMP connection
    smp_connect();

    ESP_LOGI(TAG, "Done!");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}