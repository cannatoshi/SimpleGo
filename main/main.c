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
#include "mbedtls/gcm.h"  // Phase 2a: Header decrypt test

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

// Auftrag 24: Forward declaration for HELLO after KEY
extern bool peer_send_hello(contact_t *contact);
extern bool peer_send_chat_message(contact_t *contact, const char *message);  // Auftrag 44a
#include "smp_x448.h"
#include "smp_queue.h"
#include "simplex_crypto.h"  // SimpleX custom XSalsa20-Poly1305
#include "zstd.h"            // Zstd decompression for ConnInfo

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

    // Auftrag 23: Peer's sender auth key from received Confirmation
    uint8_t peer_sender_auth_key[44];  // Ed25519 SPKI from App's PrivHeader 'K'
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
    } else {
        ESP_LOGI(TAG, "      %d contact(s) loaded from NVS", contacts_db.num_contacts);
    }
    
    list_contacts();
    
    // ========== Step 5: Subscribe All Contacts ==========
    ESP_LOGI(TAG, "[5/5] Subscribing to queues...");
    subscribe_all_contacts(&ssl, block, session_id);
    
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
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "   MESSAGE (unknown contact)!");
            }
            ESP_LOGI(TAG, "   MsgId: %02x%02x%02x%02x...", msg_id[0], msg_id[1], msg_id[2], msg_id[3]);
            ESP_LOGI(TAG, "   Encrypted: %d bytes", enc_len);

            // ============================================================================
            // REPLY QUEUE E2E DECRYPTION (Session 16 - Cleaned)
            // ============================================================================
            if (is_reply_queue && our_queue.valid && enc_len > crypto_box_MACBYTES) {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "   Decrypting REPLY QUEUE message...");
                
                // Layer 1: Server-level decrypt (mit msg_id als Nonce)
                uint8_t server_nonce[24];
                memset(server_nonce, 0, 24);
                memcpy(server_nonce, msg_id, msgIdLen);
                
                uint8_t *server_plain = malloc(enc_len);
                if (!server_plain) {
                    ESP_LOGE(TAG, "      malloc failed!");
                    continue;
                }
                
                if (crypto_box_open_easy_afternm(server_plain, &resp[p], enc_len, 
                                                  server_nonce, our_queue.shared_secret) != 0) {
                    ESP_LOGE(TAG, "      Server-level decrypt FAILED!");
                    free(server_plain);
                    continue;
                }
                
                int plain_len = enc_len - crypto_box_MACBYTES;
                ESP_LOGI(TAG, "      Server-level decrypt SUCCESS! (%d bytes)", plain_len);
                
                // ================================================================
                // 🐰 CRITICAL: Raw Wire Format Analysis
                // ================================================================
                static int reply_msg_counter = 0;
                reply_msg_counter++;
                
                ESP_LOGE(TAG, "");
                ESP_LOGE(TAG, "      ╔═══════════════════════════════════════════════════════╗");
                ESP_LOGE(TAG, "      ║  🔬 REPLY QUEUE MESSAGE #%d RAW ANALYSIS              ║", reply_msg_counter);
                ESP_LOGE(TAG, "      ╚═══════════════════════════════════════════════════════╝");
                
                // Show raw bytes WITHOUT any offset adjustments first
                ESP_LOGE(TAG, "      📏 Total decrypted length: %d bytes", plain_len);
                
                ESP_LOGE(TAG, "      📦 RAW server_plain[0-31] (NO offset adjustment):");
                printf("         ");
                for (int i = 0; i < 32 && i < plain_len; i++) {
                    printf("%02x ", server_plain[i]);
                    if ((i + 1) % 16 == 0) printf("\n         ");
                }
                printf("\n");
                printf("         ASCII: ");
                for (int i = 0; i < 32 && i < plain_len; i++) {
                    char c = server_plain[i];
                    printf("%c", (c >= 32 && c < 127) ? c : '.');
                }
                printf("\n");
                
                // Check for length prefix
                uint16_t raw_len_prefix = (server_plain[0] << 8) | server_plain[1];
                ESP_LOGE(TAG, "      📏 Bytes [0-1] as BE uint16: 0x%04x = %d", raw_len_prefix, raw_len_prefix);
                
                // Now check different offset interpretations
                ESP_LOGE(TAG, "");
                ESP_LOGE(TAG, "      🔍 MAYBE ENCODING CHECK (Haskell: '0'=Nothing, '1'=Just):");
                
                // Without length prefix skip (raw)
                ESP_LOGE(TAG, "      [RAW] server_plain[12-17]: %02x %02x %02x %02x %02x %02x  ('%c' '%c' '%c' '%c' '%c' '%c')",
                         server_plain[12], server_plain[13], server_plain[14], server_plain[15], server_plain[16], server_plain[17],
                         (server_plain[12] >= 32 && server_plain[12] < 127) ? server_plain[12] : '.',
                         (server_plain[13] >= 32 && server_plain[13] < 127) ? server_plain[13] : '.',
                         (server_plain[14] >= 32 && server_plain[14] < 127) ? server_plain[14] : '.',
                         (server_plain[15] >= 32 && server_plain[15] < 127) ? server_plain[15] : '.',
                         (server_plain[16] >= 32 && server_plain[16] < 127) ? server_plain[16] : '.',
                         (server_plain[17] >= 32 && server_plain[17] < 127) ? server_plain[17] : '.');
                
                // With +2 offset (after length prefix)
                ESP_LOGE(TAG, "      [+2]  server_plain[14-19]: %02x %02x %02x %02x %02x %02x  ('%c' '%c' '%c' '%c' '%c' '%c')",
                         server_plain[14], server_plain[15], server_plain[16], server_plain[17], server_plain[18], server_plain[19],
                         (server_plain[14] >= 32 && server_plain[14] < 127) ? server_plain[14] : '.',
                         (server_plain[15] >= 32 && server_plain[15] < 127) ? server_plain[15] : '.',
                         (server_plain[16] >= 32 && server_plain[16] < 127) ? server_plain[16] : '.',
                         (server_plain[17] >= 32 && server_plain[17] < 127) ? server_plain[17] : '.',
                         (server_plain[18] >= 32 && server_plain[18] < 127) ? server_plain[18] : '.',
                         (server_plain[19] >= 32 && server_plain[19] < 127) ? server_plain[19] : '.');
                
                // Look for '0' or '1' anywhere in first 25 bytes
                ESP_LOGE(TAG, "");
                ESP_LOGE(TAG, "      🎯 SEARCHING FOR MAYBE MARKERS ('0'=0x30, '1'=0x31):");
                for (int i = 0; i < 25 && i < plain_len; i++) {
                    if (server_plain[i] == '0') {
                        ESP_LOGW(TAG, "         Found '0' (Nothing) at RAW offset %d", i);
                    }
                    if (server_plain[i] == '1') {
                        ESP_LOGW(TAG, "         Found '1' (Just) at RAW offset %d", i);
                    }
                }
                
                // Look for SPKI pattern (30 2a 30 05 06 03 2b 65 6e)
                ESP_LOGE(TAG, "");
                ESP_LOGE(TAG, "      🔍 SEARCHING FOR X25519 SPKI PATTERN (30 2a 30 05 06 03 2b 65 6e):");
                for (int i = 0; i < plain_len - 9 && i < 100; i++) {
                    if (server_plain[i] == 0x30 && server_plain[i+1] == 0x2a && 
                        server_plain[i+2] == 0x30 && server_plain[i+3] == 0x05 &&
                        server_plain[i+4] == 0x06 && server_plain[i+5] == 0x03 &&
                        server_plain[i+6] == 0x2b && server_plain[i+7] == 0x65) {
                        ESP_LOGW(TAG, "         Found X25519 SPKI at RAW offset %d", i);
                        if (i >= 4) {
                            ESP_LOGW(TAG, "         Bytes BEFORE SPKI [%d-%d]: %02x %02x %02x %02x  ('%c' '%c' '%c' '%c')",
                                     i-4, i-1, server_plain[i-4], server_plain[i-3], server_plain[i-2], server_plain[i-1],
                                     (server_plain[i-4] >= 32 && server_plain[i-4] < 127) ? server_plain[i-4] : '.',
                                     (server_plain[i-3] >= 32 && server_plain[i-3] < 127) ? server_plain[i-3] : '.',
                                     (server_plain[i-2] >= 32 && server_plain[i-2] < 127) ? server_plain[i-2] : '.',
                                     (server_plain[i-1] >= 32 && server_plain[i-1] < 127) ? server_plain[i-1] : '.');
                        }
                        // Show the raw key after SPKI header
                        ESP_LOGW(TAG, "         Raw key at offset %d: %02x%02x%02x%02x%02x%02x%02x%02x...",
                                 i + 12,
                                 server_plain[i+12], server_plain[i+13], server_plain[i+14], server_plain[i+15],
                                 server_plain[i+16], server_plain[i+17], server_plain[i+18], server_plain[i+19]);
                    }
                }
                
                ESP_LOGE(TAG, "");
                ESP_LOGE(TAG, "      ⚠️  NOTE: ',' (0x2C) is NOT a valid Maybe marker!");
                ESP_LOGE(TAG, "      ⚠️  Valid Haskell Maybe: '0' (0x30)=Nothing, '1' (0x31)=Just");
                ESP_LOGE(TAG, "");
                
                // Debug: First 64 bytes
                ESP_LOGI(TAG, "      First 64 bytes:");
                printf("         ");
                for (int i = 0; i < 64 && i < plain_len; i++) {
                    printf("%02x ", server_plain[i]);
                    if ((i + 1) % 16 == 0) printf("\n         ");
                }
                printf("\n");
                
                // === LAYER 2: Per-Queue E2E Decrypt ===
                // ================================================================
                // IMPORTANT: Reply Queue messages have a 2-byte LENGTH PREFIX!
                // Contact Queue: ClientMsgEnvelope starts at offset 0
                // Reply Queue:   [0-1] = Length prefix, ClientMsgEnvelope at offset 2
                //
                // ClientMsgEnvelope Layout (after length prefix):
                // [0-7]   = Message Header (timestamps, etc.)
                // [8-9]   = "T " (PubHeader Tag + Space)
                // [10-11] = Version (0x0004 = ClientMsg v4)
                // [12]    = maybe_corrId tag ('0' = Nothing, '1' = Just)
                // [13]    = maybe_e2e tag ('0' = Nothing, '1' = Just)
                // 
                // NOTE: We're seeing ',' (0x2C) which is NOT a valid Maybe marker!
                // Haskell Maybe encoding: '0' (0x30) = Nothing, '1' (0x31) = Just
                // This suggests our offset might be wrong or format is different!
                //
                // If maybe_corrId='1' and maybe_e2e='0' (Nothing = no separate e2e key):
                //   [14-57] = corrId SPKI (44 bytes) - this IS the E2E key!
                //   [58-81] = cmNonce (24 bytes RANDOM - read directly!)
                //   [82+]   = cmEncBody
                //
                // CRITICAL: cmNonce is NOT derived - it's IN the message!
                // ================================================================
                
                // Check for 2-byte length prefix (Reply Queue specific!)
                int rq_prefix_len = 0;
                if (plain_len > 2 && (server_plain[0] != 0x00 || server_plain[1] != 0x00)) {
                    // First 2 bytes are non-zero = length prefix present
                    uint16_t len_prefix = (server_plain[0] << 8) | server_plain[1];
                    ESP_LOGI(TAG, "      📏 Reply Queue length prefix: %u (0x%02x%02x)",
                             len_prefix, server_plain[0], server_plain[1]);
                    rq_prefix_len = 2;  // Skip the length prefix
                }
                
                // Point to the actual ClientMsgEnvelope (after any length prefix)
                const uint8_t *envelope = server_plain + rq_prefix_len;
                size_t envelope_len = raw_len_prefix;  // Use actual data length, not buffer length!
                
                // ================================================================
                // 🐰 DEBUG: Exakte Byte-Extraktion für E2E Decrypt
                // ================================================================
                ESP_LOGE(TAG, "");
                ESP_LOGE(TAG, "      ╔═══════════════════════════════════════════════════════╗");
                ESP_LOGE(TAG, "      ║  📐 BYTE EXTRACTION DEBUG                             ║");
                ESP_LOGE(TAG, "      ╚═══════════════════════════════════════════════════════╝");
                
                // Show raw envelope bytes
                ESP_LOGE(TAG, "      📏 envelope starts at server_plain + %d", rq_prefix_len);
                ESP_LOGE(TAG, "      📏 envelope_len = %zu", envelope_len);
                ESP_LOGE(TAG, "      Raw envelope [0-31]:");
                printf("         ");
                for (int i = 0; i < 32 && i < (int)envelope_len; i++) {
                    printf("%02x ", envelope[i]);
                    if ((i + 1) % 16 == 0) printf("\n         ");
                }
                printf("\n");
                
                // Show critical offsets
                ESP_LOGE(TAG, "      Critical bytes:");
                ESP_LOGE(TAG, "         envelope[12] (maybe_corrId): 0x%02x '%c'", 
                         envelope[12], (envelope[12] >= 0x20 && envelope[12] < 0x7f) ? envelope[12] : '?');
                ESP_LOGE(TAG, "         envelope[13] (maybe_e2e):    0x%02x '%c'", 
                         envelope[13], (envelope[13] >= 0x20 && envelope[13] < 0x7f) ? envelope[13] : '?');
                ESP_LOGE(TAG, "         envelope[14-19] (SPKI start): %02x %02x %02x %02x %02x %02x",
                         envelope[14], envelope[15], envelope[16], envelope[17], envelope[18], envelope[19]);
                ESP_LOGE(TAG, "         envelope[26-33] (raw key):    %02x %02x %02x %02x %02x %02x %02x %02x",
                         envelope[26], envelope[27], envelope[28], envelope[29],
                         envelope[30], envelope[31], envelope[32], envelope[33]);
                ESP_LOGE(TAG, "         envelope[58-65] (cmNonce):    %02x %02x %02x %02x %02x %02x %02x %02x",
                         envelope[58], envelope[59], envelope[60], envelope[61],
                         envelope[62], envelope[63], envelope[64], envelope[65]);
                ESP_LOGE(TAG, "         envelope[82-89] (ciphertext): %02x %02x %02x %02x %02x %02x %02x %02x",
                         envelope[82], envelope[83], envelope[84], envelope[85],
                         envelope[86], envelope[87], envelope[88], envelope[89]);
                ESP_LOGE(TAG, "");
                
                int offset = 12;
                
                // [12] = maybe_corrId
                uint8_t maybe_corrId = envelope[offset];
                ESP_LOGI(TAG, "      [%d] maybe_corrId = '%c' (0x%02x)", offset + rq_prefix_len, maybe_corrId, maybe_corrId);
                offset++;  // Now at 13
                
                // [13] = maybe_e2e (NOT corrId_len!)
                uint8_t maybe_e2e = envelope[offset];
                ESP_LOGI(TAG, "      [%d] maybe_e2e = '%c' (0x%02x)", offset + rq_prefix_len, 
                         (maybe_e2e >= 0x20 && maybe_e2e < 0x7f) ? maybe_e2e : '?', maybe_e2e);
                offset++;  // Now at 14
                
                const uint8_t x25519_spki_header[] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 
                                                       0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00};
                
                uint8_t sender_pub[32];
                bool have_sender_key = false;
                
                if (maybe_corrId == '1' && (maybe_e2e == ',' || maybe_e2e == '0')) {
                    // maybe_e2e = ',' (0x2C) or '0' (0x30) = Nothing = corrId SPKI IS the E2E key!
                    // NOTE: ',' is not standard Haskell Maybe encoding, but we see it in logs
                    ESP_LOGI(TAG, "      maybe_e2e = '%c' -> corrId SPKI doubles as E2E key!", maybe_e2e);
                    
                    // Verify SPKI header at offset 14
                    if (memcmp(&envelope[offset], x25519_spki_header, 12) == 0) {
                        // Extract raw key (after 12-byte SPKI header)
                        memcpy(sender_pub, &envelope[offset + 12], 32);
                        have_sender_key = true;
                        
                        ESP_LOGI(TAG, "      ✅ Found E2E Key (from corrId SPKI) at offset %d!", offset + rq_prefix_len);
                        
                        // 🐰 Full sender_pub debug
                        ESP_LOGE(TAG, "      📦 SENDER_PUB FULL (from envelope[%d + 12 = %d]):", offset, offset + 12);
                        printf("         ");
                        for (int i = 0; i < 32; i++) printf("%02x", sender_pub[i]);
                        printf("\n");
                        
                        // Save for future messages
                        memcpy(reply_queue_e2e_peer_public, sender_pub, 32);
                        reply_queue_e2e_peer_valid = true;
                    } else {
                        ESP_LOGE(TAG, "      ❌ SPKI header mismatch at offset %d", offset + rq_prefix_len);
                        ESP_LOGE(TAG, "         Expected: 30 2a 30 05 06 03 2b 65 6e 03 21 00");
                        ESP_LOGE(TAG, "         Got:      %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
                                 envelope[offset], envelope[offset+1], envelope[offset+2], envelope[offset+3],
                                 envelope[offset+4], envelope[offset+5], envelope[offset+6], envelope[offset+7],
                                 envelope[offset+8], envelope[offset+9], envelope[offset+10], envelope[offset+11]);
                    }
                    offset += 44;  // Skip past SPKI, now at cmNonce (offset 58)
                    
                } else if (maybe_corrId == '1' && maybe_e2e == '1') {
                    // maybe_e2e = '1' means: separate E2E key follows after corrId
                    ESP_LOGI(TAG, "      maybe_e2e = '1' -> separate E2E key after corrId");
                    
                    // Skip corrId SPKI first
                    uint8_t corrId_len = 44;  // Standard SPKI length
                    offset += corrId_len;  // Now past corrId
                    
                    // Now read e2e key length and key
                    uint8_t e2e_len = envelope[offset];
                    offset++;
                    
                    if (e2e_len == 44 && memcmp(&envelope[offset], x25519_spki_header, 12) == 0) {
                        memcpy(sender_pub, &envelope[offset + 12], 32);
                        have_sender_key = true;
                        ESP_LOGI(TAG, "      ✅ Found separate E2E Key at offset %d!", offset + rq_prefix_len);
                    }
                    offset += 44;  // Skip E2E SPKI
                    
                } else if (maybe_corrId == ',' || maybe_corrId == '0') {
                    // No corrId - use pre-shared key
                    ESP_LOGI(TAG, "      maybe_corrId = '%c' -> using PRE-SHARED key!", maybe_corrId);
                    
                    if (reply_queue_e2e_peer_valid) {
                        memcpy(sender_pub, reply_queue_e2e_peer_public, 32);
                        have_sender_key = true;
                        ESP_LOGI(TAG, "      ✅ Using pre-shared key: %02x%02x%02x%02x...",
                                 sender_pub[0], sender_pub[1], sender_pub[2], sender_pub[3]);
                    } else {
                        ESP_LOGE(TAG, "      ❌ No pre-shared E2E key available!");
                    }
                } else {
                    ESP_LOGE(TAG, "      ❌ Unknown format: maybe_corrId=0x%02x ('%c'), maybe_e2e=0x%02x ('%c')", 
                             maybe_corrId, (maybe_corrId >= 32 && maybe_corrId < 127) ? maybe_corrId : '?',
                             maybe_e2e, (maybe_e2e >= 32 && maybe_e2e < 127) ? maybe_e2e : '?');
                    ESP_LOGE(TAG, "         Haskell Maybe: '0' (0x30) = Nothing, '1' (0x31) = Just");
                    ESP_LOGE(TAG, "         Try looking at the RAW ANALYSIS output above!");
                    
                    // Try to find SPKI anyway at different offsets
                    ESP_LOGE(TAG, "         🔍 Attempting SPKI search at nearby offsets...");
                    for (int try_off = 10; try_off <= 20; try_off++) {
                        if (memcmp(&envelope[try_off], x25519_spki_header, 12) == 0) {
                            ESP_LOGW(TAG, "         Found SPKI at envelope[%d]! Maybe offset is %d?", try_off, try_off);
                            // Try extracting key from this offset
                            memcpy(sender_pub, &envelope[try_off + 12], 32);
                            have_sender_key = true;
                            ESP_LOGW(TAG, "         Extracted key: %02x%02x%02x%02x%02x%02x%02x%02x...",
                                     sender_pub[0], sender_pub[1], sender_pub[2], sender_pub[3],
                                     sender_pub[4], sender_pub[5], sender_pub[6], sender_pub[7]);
                            
                            // Adjust offset to continue parsing
                            offset = try_off + 44;  // After SPKI
                            ESP_LOGW(TAG, "         Adjusted offset to %d for nonce/cipher", offset);
                            break;
                        }
                    }
                }
                
                if (!have_sender_key) {
                    ESP_LOGE(TAG, "      ❌ No sender key available!");
                    free(server_plain);
                    continue;
                }
                
                // Now offset points to cmNonce (24 bytes)
                if (envelope_len < (size_t)offset + 24 + 16) {
                    ESP_LOGE(TAG, "      Message too short for E2E decrypt");
                    free(server_plain);
                    continue;
                }
                
                uint8_t cm_nonce[24];
                memcpy(cm_nonce, &envelope[offset], 24);
                ESP_LOGI(TAG, "      cmNonce (at offset %d): %02x%02x%02x%02x...",
                         offset + rq_prefix_len, cm_nonce[0], cm_nonce[1], cm_nonce[2], cm_nonce[3]);
                
                // 🐰 Full nonce debug
                ESP_LOGE(TAG, "      📦 CM_NONCE FULL (from envelope[%d]):", offset);
                printf("         ");
                for (int i = 0; i < 24; i++) printf("%02x", cm_nonce[i]);
                printf("\n");
                
                offset += 24;
                
                const uint8_t *e2e_encrypted = &envelope[offset];
                size_t e2e_encrypted_len = envelope_len - offset;
                
                ESP_LOGI(TAG, "      E2E encrypted at offset %d, len: %zu", offset + rq_prefix_len, e2e_encrypted_len);
                
                // 🐰 Ciphertext debug
                ESP_LOGE(TAG, "      📦 CIPHERTEXT (from envelope[%d], len=%zu):", offset, e2e_encrypted_len);
                ESP_LOGE(TAG, "         First 32 bytes:");
                printf("            ");
                for (int i = 0; i < 32 && i < (int)e2e_encrypted_len; i++) printf("%02x", e2e_encrypted[i]);
                printf("\n");
                if (e2e_encrypted_len > 16) {
                    ESP_LOGE(TAG, "         Last 16 bytes (Poly1305 MAC):");
                    printf("            ");
                    for (size_t i = e2e_encrypted_len - 16; i < e2e_encrypted_len; i++) printf("%02x", e2e_encrypted[i]);
                    printf("\n");
                }
                ESP_LOGE(TAG, "");
                
                uint8_t *e2e_plain = malloc(e2e_encrypted_len);
                if (e2e_plain) {
                    // ================================================================
                    // CRITICAL DEBUG: Verify our keypair is valid!
                    // ================================================================
                    uint8_t derived_pub[32];
                    crypto_scalarmult_base(derived_pub, our_queue.e2e_private);
                    
                    bool keypair_valid = (memcmp(derived_pub, our_queue.e2e_public, 32) == 0);
                    
                    ESP_LOGW(TAG, "");
                    ESP_LOGW(TAG, "      ╔═══════════════════════════════════════════════════════╗");
                    ESP_LOGW(TAG, "      ║  🔑 DECRYPT: KEY VERIFICATION                        ║");
                    ESP_LOGW(TAG, "      ╚═══════════════════════════════════════════════════════╝");
                    ESP_LOGW(TAG, "      our_queue.e2e_public:  %02x%02x%02x%02x %02x%02x%02x%02x...",
                             our_queue.e2e_public[0], our_queue.e2e_public[1],
                             our_queue.e2e_public[2], our_queue.e2e_public[3],
                             our_queue.e2e_public[4], our_queue.e2e_public[5],
                             our_queue.e2e_public[6], our_queue.e2e_public[7]);
                    ESP_LOGW(TAG, "      derived from private:  %02x%02x%02x%02x %02x%02x%02x%02x...",
                             derived_pub[0], derived_pub[1],
                             derived_pub[2], derived_pub[3],
                             derived_pub[4], derived_pub[5],
                             derived_pub[6], derived_pub[7]);
                    ESP_LOGW(TAG, "      our_queue.e2e_private: %02x%02x%02x%02x %02x%02x%02x%02x...",
                             our_queue.e2e_private[0], our_queue.e2e_private[1],
                             our_queue.e2e_private[2], our_queue.e2e_private[3],
                             our_queue.e2e_private[4], our_queue.e2e_private[5],
                             our_queue.e2e_private[6], our_queue.e2e_private[7]);
                    ESP_LOGW(TAG, "      sender_pub (from msg): %02x%02x%02x%02x %02x%02x%02x%02x...",
                             sender_pub[0], sender_pub[1],
                             sender_pub[2], sender_pub[3],
                             sender_pub[4], sender_pub[5],
                             sender_pub[6], sender_pub[7]);
                    
                    if (!keypair_valid) {
                        ESP_LOGE(TAG, "      ❌ KEYPAIR MISMATCH! Private key doesn't match public key!");
                    } else {
                        ESP_LOGI(TAG, "      ✅ Keypair valid (private derives to stored public)");
                    }
                    
                    // ================================================================
                    // 🐰 CHECK: Ist sender_pub unser eigener Key? (wäre Bug!)
                    // ================================================================
                    bool sender_is_us = (memcmp(sender_pub, our_queue.e2e_public, 32) == 0);
                    if (sender_is_us) {
                        ESP_LOGE(TAG, "      ⚠️ WARNING: sender_pub == our_queue.e2e_public!");
                        ESP_LOGE(TAG, "      This means the App encrypted TO our own key - WRONG!");
                    } else {
                        ESP_LOGI(TAG, "      ✅ sender_pub is different from our key (correct)");
                    }
                    ESP_LOGW(TAG, "");
                    
                    // ================================================================
                    // DEBUG: Key Identity Check - ist der Key noch derselbe?
                    // ================================================================
                    ESP_LOGE(TAG, "");
                    ESP_LOGE(TAG, "      🔵 REPLY_DECRYPT e2e_private FULL:");
                    printf("         ");
                    for (int i = 0; i < 32; i++) printf("%02x", our_queue.e2e_private[i]);
                    printf("\n");
                    ESP_LOGE(TAG, "      🔵 REPLY_DECRYPT e2e_public FULL:");
                    printf("         ");
                    for (int i = 0; i < 32; i++) printf("%02x", our_queue.e2e_public[i]);
                    printf("\n");
                    ESP_LOGE(TAG, "      🟣 sender_pub (from msg) FULL:");
                    printf("         ");
                    for (int i = 0; i < 32; i++) printf("%02x", sender_pub[i]);
                    printf("\n");
                    ESP_LOGE(TAG, "");
                    
                    // Compute DH secret
                    uint8_t dh_secret[32];
                    if (crypto_scalarmult(dh_secret, our_queue.e2e_private, sender_pub) != 0) {
                        ESP_LOGE(TAG, "      ❌ DH computation failed!");
                        free(e2e_plain);
                        free(server_plain);
                        continue;
                    }
                    
                    ESP_LOGI(TAG, "      DH secret: %02x%02x%02x%02x...",
                             dh_secret[0], dh_secret[1], dh_secret[2], dh_secret[3]);
                    
                    // ================================================================
                    // 🐰 SUMMARY: All Extracted Values
                    // ================================================================
                    ESP_LOGE(TAG, "");
                    ESP_LOGE(TAG, "      ╔═══════════════════════════════════════════════════════╗");
                    ESP_LOGE(TAG, "      ║  📊 CRYPTO INPUTS SUMMARY                             ║");
                    ESP_LOGE(TAG, "      ╚═══════════════════════════════════════════════════════╝");
                    ESP_LOGE(TAG, "      OUR e2e_private (for DH):");
                    printf("         ");
                    for (int i = 0; i < 32; i++) printf("%02x", our_queue.e2e_private[i]);
                    printf("\n");
                    ESP_LOGE(TAG, "      THEIR sender_pub (from msg):");
                    printf("         ");
                    for (int i = 0; i < 32; i++) printf("%02x", sender_pub[i]);
                    printf("\n");
                    ESP_LOGE(TAG, "      DH_SECRET (e2e_private * sender_pub):");
                    printf("         ");
                    for (int i = 0; i < 32; i++) printf("%02x", dh_secret[i]);
                    printf("\n");
                    ESP_LOGE(TAG, "      CM_NONCE (24 bytes from msg):");
                    printf("         ");
                    for (int i = 0; i < 24; i++) printf("%02x", cm_nonce[i]);
                    printf("\n");
                    ESP_LOGE(TAG, "      CIPHERTEXT len: %zu bytes", e2e_encrypted_len);
                    ESP_LOGE(TAG, "");
                    
                    // Full debug dump
                    printf("\n      📋 FULL CRYPTO DEBUG:\n");
                    printf("      sender_pub (32): ");
                    for(int i=0; i<32; i++) printf("%02x", sender_pub[i]);
                    printf("\n");
                    printf("      our_e2e_priv (32): ");
                    for(int i=0; i<32; i++) printf("%02x", our_queue.e2e_private[i]);
                    printf("\n");
                    printf("      dh_secret (32): ");
                    for(int i=0; i<32; i++) printf("%02x", dh_secret[i]);
                    printf("\n");
                    printf("      cm_nonce (24): ");
                    for(int i=0; i<24; i++) printf("%02x", cm_nonce[i]);
                    printf("\n");
                    printf("      e2e_cipher first 48: ");
                    for(int i=0; i<48 && i<(int)e2e_encrypted_len; i++) printf("%02x", e2e_encrypted[i]);
                    printf("\n\n");
                    
                    // ================================================================
                    // FIX v0.1.22: Try STANDARD crypto functions instead of custom!
                    // According to Haskell analysis: e2eDhSecret is simple X25519 DH
                    // used directly with NaCl crypto_box, NO custom XSalsa20 needed!
                    // ================================================================
                    
                    int decrypt_ret = -1;
                    
                    // Method 0: Use decrypt_client_msg (SAME function that works for Contact Queue!)
                    // This expects [24 nonce][ciphertext+MAC] format
                    // We pass data starting right after the SPKI key
                    ESP_LOGI(TAG, "      Trying Method 0: decrypt_client_msg (Contact Queue style)...");
                    int after_key_offset = offset - 44 + 44;  // Right after SPKI = nonce start
                    // Actually offset already points past nonce to cipher, so go back
                    int nonce_start = offset - 24;  // Back to where nonce starts (relative to envelope)
                    int enc_block_len = envelope_len - nonce_start;
                    ESP_LOGI(TAG, "      enc_block starts at %d, len=%d", nonce_start + rq_prefix_len, enc_block_len);
                    
                    uint8_t *method0_plain = malloc(enc_block_len);
                    if (method0_plain) {
                        int dec_len = decrypt_client_msg(&envelope[nonce_start], enc_block_len,
                                                         sender_pub,
                                                         our_queue.e2e_private,
                                                         method0_plain);
                        if (dec_len > 0) {
                            decrypt_ret = 0;
                            memcpy(e2e_plain, method0_plain, dec_len < (int)e2e_encrypted_len ? dec_len : e2e_encrypted_len);
                            ESP_LOGI(TAG, "      ✅ Method 0 SUCCESS! Decrypted %d bytes", dec_len);
                        } else {
                            ESP_LOGI(TAG, "      Method 0 failed");
                        }
                        free(method0_plain);
                    }
                    
                    if (decrypt_ret != 0) {
                        // Method 1: crypto_box_open_easy (full NaCl box)
                        // This does: DH internally + HSalsa20 + XSalsa20-Poly1305
                        ESP_LOGI(TAG, "      Trying Method 1: crypto_box_open_easy...");
                        decrypt_ret = crypto_box_open_easy(
                            e2e_plain,
                            e2e_encrypted,
                            e2e_encrypted_len,
                            cm_nonce,
                            sender_pub,
                            our_queue.e2e_private
                        );
                    }
                    
                    if (decrypt_ret != 0) {
                        // Method 2: crypto_secretbox_open_easy with DH secret
                        // This does: HSalsa20(dh_secret, nonce[0:16]) + XSalsa20-Poly1305
                        ESP_LOGI(TAG, "      Method 1 failed, trying Method 2: crypto_secretbox_open_easy...");
                        decrypt_ret = crypto_secretbox_open_easy(
                            e2e_plain,
                            e2e_encrypted,
                            e2e_encrypted_len,
                            cm_nonce,
                            dh_secret
                        );
                    }
                    
                    if (decrypt_ret != 0) {
                        // Method 3: Original custom simplex_secretbox_open_debug
                        ESP_LOGI(TAG, "      Method 2 failed, trying Method 3: simplex_secretbox_open_debug...");
                        decrypt_ret = simplex_secretbox_open_debug(
                            e2e_plain,
                            e2e_encrypted,
                            e2e_encrypted_len,
                            cm_nonce,
                            dh_secret,
                            "REPLY_E2E"
                        );
                    }
                    
                    sodium_memzero(dh_secret, 32);
                    
                    if (decrypt_ret == 0) {
                        int e2e_plain_len = e2e_encrypted_len - 16;
                        ESP_LOGI(TAG, "");
                        ESP_LOGI(TAG, "      +----------------------------------------------+");
                        ESP_LOGI(TAG, "      |  🎉 E2E LAYER 2 DECRYPT SUCCESS!            |");
                        ESP_LOGI(TAG, "      +----------------------------------------------+");
                        ESP_LOGI(TAG, "      Decrypted %d bytes!", e2e_plain_len);
                        // Auftrag 38b: Hex+ASCII dump after E2E decrypt (256 bytes)
                        {
                            size_t dump_len = e2e_plain_len < 256 ? e2e_plain_len : 256;
                            ESP_LOGI(TAG, "      📋 E2E decrypted dump (%zu of %d bytes):", dump_len, e2e_plain_len);
                            for (size_t di = 0; di < dump_len; di += 16) {
                                char hex[64] = {0};
                                char asc[20] = {0};
                                int hx = 0;
                                for (size_t dj = 0; dj < 16 && (di+dj) < dump_len; dj++) {
                                    uint8_t b = e2e_plain[di+dj];
                                    hx += sprintf(&hex[hx], "%02x ", b);
                                    asc[dj] = (b >= 0x20 && b < 0x7f) ? (char)b : '.';
                                }
                                ESP_LOGI(TAG, "        +%04zu: %-48s |%s|", di, hex, asc);
                            }
                        }
                        
                        // ================================================================
                        // 🐰 SESSION 19: AgentConfirmation Full Parse
                        // ================================================================
                        
                        // === SCHRITT 1: unPad ===
                        uint16_t original_len = (e2e_plain[0] << 8) | e2e_plain[1];
                        uint8_t *client_msg = e2e_plain + 2;  // Skip 2-byte length prefix
                        ESP_LOGI(TAG, "");
                        ESP_LOGI(TAG, "      ╔═══════════════════════════════════════════════════════╗");
                        ESP_LOGI(TAG, "      ║  📦 SESSION 19: AgentConfirmation Full Parse           ║");
                        ESP_LOGI(TAG, "      ╚═══════════════════════════════════════════════════════╝");
                        ESP_LOGI(TAG, "      unPad: originalLength=%u, totalDecrypted=%d, padding=%d",
                                 original_len, e2e_plain_len, e2e_plain_len - 2 - original_len);
                        
                        // === SCHRITT 2: PrivHeader parsen ===
                        uint8_t priv_tag = client_msg[0];
                        ESP_LOGI(TAG, "      PrivHeader tag: 0x%02x '%c'", priv_tag, priv_tag);
                        
                        if (priv_tag == 'K') {  // PHConfirmation
                            uint8_t key_len = client_msg[1];
                            ESP_LOGI(TAG, "      PHConfirmation: Auth Key len=%u", key_len);
                            ESP_LOG_BUFFER_HEX("Auth Key (Ed25519 SPKI)", &client_msg[2], key_len);
                            
                            // Auftrag 23: SAVE sender auth key for KEY command
                            if (key_len == 44) {
                                memcpy(peer_sender_auth_key, &client_msg[2], 44);
                                has_peer_sender_auth = true;
                                ESP_LOGI(TAG, "      ✅ Sender auth key SAVED for KEY command!");
                            } else {
                                ESP_LOGW(TAG, "      ⚠️ Unexpected key_len=%u (expected 44)", key_len);
                            }
                            
                            size_t cm_offset = 2 + key_len;  // Nach dem Auth Key
                            
                            // === SCHRITT 3: AgentConfirmation parsen ===
                            uint16_t agent_version = (client_msg[cm_offset] << 8) | client_msg[cm_offset+1];
                            cm_offset += 2;
                            
                            char agent_tag = client_msg[cm_offset];
                            cm_offset += 1;
                            
                            char maybe_tag = client_msg[cm_offset];
                            cm_offset += 1;
                            
                            ESP_LOGI(TAG, "      === AgentConfirmation ===");
                            ESP_LOGI(TAG, "      agentVersion: %u", agent_version);
                            ESP_LOGI(TAG, "      Tag: '%c' (0x%02x) — erwartet 'C' (0x43)", agent_tag, agent_tag);
                            ESP_LOGI(TAG, "      e2eEncryption_: '%c' (0x%02x) — '0'=Nothing, '1'=Just",
                                     maybe_tag, maybe_tag);
                            
                            if (maybe_tag == '0') {
                                ESP_LOGI(TAG, "      e2eEncryption_ = Nothing (X3DH bereits ausgetauscht)");
                            } else if (maybe_tag == '1') {
                                ESP_LOGW(TAG, "      e2eEncryption_ = Just — X448 Keys folgen! Parsing TODO!");
                                // Für jetzt überspringen — wir loggen trotzdem die nächsten Bytes
                                ESP_LOG_BUFFER_HEX("Next 32 bytes after maybe_tag", &client_msg[cm_offset], 32);
                            } else {
                                ESP_LOGE(TAG, "      ⚠️ Unexpected maybe_tag: 0x%02x '%c'",
                                         maybe_tag, (maybe_tag >= 0x20 && maybe_tag < 0x7f) ? maybe_tag : '?');
                                ESP_LOG_BUFFER_HEX("Context around maybe_tag", &client_msg[cm_offset - 4], 32);
                            }
                            
                            // === SCHRITT 4: EncRatchetMessage parsen ===
                            // Auftrag 38b: Dump EncRatchetMessage area before parsing
                            {
                                size_t remaining = original_len - cm_offset;
                                size_t dump_len = remaining < 256 ? remaining : 256;
                                ESP_LOGI(TAG, "      📋 EncRatchetMessage raw at cm_offset=%d (%zu of %zu bytes):",
                                         cm_offset, dump_len, remaining);
                                for (size_t di = 0; di < dump_len; di += 16) {
                                    char hex[64] = {0};
                                    char asc[20] = {0};
                                    int hx = 0;
                                    for (size_t dj = 0; dj < 16 && (di+dj) < dump_len; dj++) {
                                        uint8_t b = client_msg[cm_offset + di + dj];
                                        hx += sprintf(&hex[hx], "%02x ", b);
                                        asc[dj] = (b >= 0x20 && b < 0x7f) ? (char)b : '.';
                                    }
                                    ESP_LOGI(TAG, "        +%04zu: %-48s |%s|", di, hex, asc);
                                }
                            }
                            // v3: 2-byte emHeader length prefix (Word16 BE, Large encoding)
                            uint16_t em_header_len = (client_msg[cm_offset] << 8) | client_msg[cm_offset + 1];
                            cm_offset += 2;
                            
                            ESP_LOGI(TAG, "      === EncRatchetMessage ===");
                            ESP_LOGI(TAG, "      emHeader Länge: %u (erwartet 124 = 0x007C für v3)", em_header_len);
                            
                            if (em_header_len != 124 && em_header_len != 123) {
                                ESP_LOGW(TAG, "      ⚠️ emHeader Länge weder 124 (v3) noch 123 (v2)!");
                            }
                            
                            // EncMessageHeader innerhalb emHeader:
                            uint8_t *em_header = &client_msg[cm_offset];
                            uint16_t eh_version = (em_header[0] << 8) | em_header[1];
                            ESP_LOGI(TAG, "      ehVersion (E2E Ratchet): %u", eh_version);
                            
                            if (eh_version >= 3) {
                                // v3: 2-byte ehBody length (Word16 BE, Large encoding)
                                ESP_LOGI(TAG, "      ✓ ehVersion = %u (v3 format)", eh_version);
                            } else {
                                ESP_LOGI(TAG, "      ✓ ehVersion = %u (v2 format)", eh_version);
                            }
                            {
                                ESP_LOG_BUFFER_HEX("ehIV (16 bytes)", &em_header[2], 16);
                                ESP_LOG_BUFFER_HEX("ehAuthTag (16 bytes)", &em_header[18], 16);
                                uint16_t eh_body_len;
                                int eh_body_offset;
                                if (eh_version >= 3) {
                                    eh_body_len = (em_header[34] << 8) | em_header[35];
                                    eh_body_offset = 36;  // After 2-byte length prefix
                                } else {
                                    eh_body_len = em_header[34];
                                    eh_body_offset = 35;  // After 1-byte length prefix
                                }
                                ESP_LOGI(TAG, "      ehBody Länge: %u (erwartet 88), offset=%d", eh_body_len, eh_body_offset);
                                ESP_LOG_BUFFER_HEX("ehBody (encrypted MsgHeader)", &em_header[eh_body_offset],
                                                    eh_body_len > 88 ? 88 : eh_body_len);
                                
                                cm_offset += em_header_len;  // Skip gesamten emHeader
                                
                                // emAuthTag (16 bytes, raw)
                                ESP_LOG_BUFFER_HEX("emAuthTag (16 bytes)", &client_msg[cm_offset], 16);
                                cm_offset += 16;
                                
                                // emBody (Tail = Rest)
                                size_t em_body_len = original_len - cm_offset;
                                ESP_LOGI(TAG, "      emBody (encrypted ConnInfo): %zu bytes", em_body_len);
                                ESP_LOG_BUFFER_HEX("emBody first 32 bytes", &client_msg[cm_offset], 32);
                                
                                ESP_LOGI(TAG, "");
                                ESP_LOGI(TAG, "      ╔═══════════════════════════════════════════════════════╗");
                                ESP_LOGI(TAG, "      ║  ✅ SESSION 19 PARSING COMPLETE                       ║");
                                ESP_LOGI(TAG, "      ╚═══════════════════════════════════════════════════════╝");
                                ESP_LOGI(TAG, "      Offset final: %zu / originalLength: %u",
                                         cm_offset + em_body_len, (unsigned)original_len);
                                ESP_LOGI(TAG, "      📋 SUMMARY:");
                                ESP_LOGI(TAG, "         PrivHeader: 'K' (PHConfirmation)");
                                ESP_LOGI(TAG, "         Auth Key: %u bytes Ed25519", key_len);
                                ESP_LOGI(TAG, "         Agent v%u, Tag='%c', e2eEnc=%c",
                                         agent_version, agent_tag, maybe_tag);
                                ESP_LOGI(TAG, "         EncRatchet: ehVer=%u, ehBody=%u, emBody=%zu",
                                         eh_version, eh_body_len, em_body_len);
                                ESP_LOGI(TAG, "         🎯 Next: Double Ratchet Decrypt!");
                                
                                // ================================================================
                                // 🐰 PHASE 2a: Header-Only Decrypt Test
                                // Verifies X3DH keys + rcAD before attempting full decrypt
                                // ================================================================
                                ESP_LOGI(TAG, "");
                                ESP_LOGI(TAG, "      ╔═══════════════════════════════════════════════════════╗");
                                ESP_LOGI(TAG, "      ║  🐰 PHASE 2a: Ratchet Header Decrypt Test             ║");
                                ESP_LOGI(TAG, "      ╚═══════════════════════════════════════════════════════╝");
                                
                                if (!ratchet_is_initialized()) {
                                    ESP_LOGE(TAG, "      ❌ Ratchet NOT initialized! X3DH hasn't run yet?");
                                    ESP_LOGE(TAG, "      (AgentConfirmation SEND must happen before we can decrypt)");
                                } else {
                                    ratchet_state_t *rs = ratchet_get_state();
                                    
                                    // === Schritt 1: rcAD Reihenfolge loggen ===
                                    ESP_LOGI(TAG, "");
                                    ESP_LOGI(TAG, "      === rcAD Construction ===");
                                    ESP_LOGI(TAG, "      assoc_data[0-55] (first key in rcAD):");
                                    printf("         ");
                                    for (int i = 0; i < 56; i++) {
                                        printf("%02x", rs->assoc_data[i]);
                                        if ((i+1) % 32 == 0) printf("\n         ");
                                    }
                                    printf("\n");
                                    ESP_LOGI(TAG, "      assoc_data[56-111] (second key in rcAD):");
                                    printf("         ");
                                    for (int i = 56; i < 112; i++) {
                                        printf("%02x", rs->assoc_data[i]);
                                        if ((i+1-56) % 32 == 0) printf("\n         ");
                                    }
                                    printf("\n");
                                    ESP_LOGI(TAG, "      NOTE: Current code sets rcAD = our_key1 || peer_key1");
                                    ESP_LOGI(TAG, "      Haskell expects: rcAD = joiner_key1 || creator_key1");
                                    
                                    // === Schritt 2: Header Key Zuordnung loggen ===
                                    ESP_LOGI(TAG, "");
                                    ESP_LOGI(TAG, "      === X3DH Output Keys ===");
                                    ESP_LOGI(TAG, "      header_key_send (hk = HKDF[0-31]):");
                                    printf("         ");
                                    for (int i = 0; i < 32; i++) printf("%02x", rs->header_key_send[i]);
                                    printf("\n");
                                    ESP_LOGI(TAG, "      header_key_recv (nhk = HKDF[32-63]):");
                                    printf("         ");
                                    for (int i = 0; i < 32; i++) printf("%02x", rs->header_key_recv[i]);
                                    printf("\n");
                                    ESP_LOGI(TAG, "      root_key (rk = HKDF[64-95]):");
                                    printf("         ");
                                    for (int i = 0; i < 32; i++) printf("%02x", rs->root_key[i]);
                                    printf("\n");
                                    ESP_LOGI(TAG, "      chain_key_send:");
                                    printf("         ");
                                    for (int i = 0; i < 32; i++) printf("%02x", rs->chain_key_send[i]);
                                    printf("\n");
                                    ESP_LOGI(TAG, "      chain_key_recv:");
                                    printf("         ");
                                    for (int i = 0; i < 32; i++) printf("%02x", rs->chain_key_recv[i]);
                                    printf("\n");
                                    ESP_LOGI(TAG, "      CRITICAL: For RECEIVING peer's first msg:");
                                    ESP_LOGI(TAG, "      Haskell says header_key for decrypt = hk (bytes 0-31)!");
                                    ESP_LOGI(TAG, "      Our code uses header_key_recv = nhk (bytes 32-63)!");
                                    
                                    // === Schritt 3: Header Decrypt — 4 Kombinationen ===
                                    ESP_LOGI(TAG, "");
                                    ESP_LOGI(TAG, "      === Header Decrypt Attempts ===");
                                    
                                    uint8_t *eh_iv_ptr     = &em_header[2];    // 16 bytes
                                    uint8_t *eh_tag_ptr    = &em_header[18];   // 16 bytes
                                    uint8_t *eh_body_ptr   = &em_header[eh_body_offset];   // eh_body_len bytes
                                    
                                    ESP_LOGI(TAG, "      ehIV:  %02x%02x%02x%02x%02x%02x%02x%02x...",
                                             eh_iv_ptr[0], eh_iv_ptr[1], eh_iv_ptr[2], eh_iv_ptr[3],
                                             eh_iv_ptr[4], eh_iv_ptr[5], eh_iv_ptr[6], eh_iv_ptr[7]);
                                    ESP_LOGI(TAG, "      ehTag: %02x%02x%02x%02x%02x%02x%02x%02x...",
                                             eh_tag_ptr[0], eh_tag_ptr[1], eh_tag_ptr[2], eh_tag_ptr[3],
                                             eh_tag_ptr[4], eh_tag_ptr[5], eh_tag_ptr[6], eh_tag_ptr[7]);
                                    ESP_LOGI(TAG, "      ehBody (%u bytes): %02x%02x%02x%02x...",
                                             eh_body_len,
                                             eh_body_ptr[0], eh_body_ptr[1], eh_body_ptr[2], eh_body_ptr[3]);
                                    
                                    // Prepare swapped rcAD (peer_key1 || our_key1)
                                    // rcAD_swapped moved into fallback block below
                                    
                                    // ================================================================
                                    // FIX 1: Dynamic buffer for header_plain
                                    // ehBody can be 88 (non-PQ) or 2310+ (PQ with Kyber1024)
                                    // ================================================================
                                    uint8_t *header_plain = malloc(eh_body_len + 16);  // +16 safety margin
                                    if (!header_plain) {
                                        ESP_LOGE(TAG, "      ❌ malloc header_plain (%u bytes) failed!", eh_body_len);
                                        // Don't use break here - we're in a deeply nested block
                                        // Just skip the header decrypt section
                                        goto skip_header_decrypt;
                                    }
                                    
                                    mbedtls_gcm_context gcm;
                                    int hdr_ret;
                                    bool header_decrypted = false;
                                    int decrypt_mode = -1;  // 0=SameRatchet(HKr), 1=AdvanceRatchet(NHKr)

                                    // ---- Try 1: header_key_recv + rcAD → SameRatchet ----
                                    ESP_LOGI(TAG, "");
                                    ESP_LOGI(TAG, "      [Try 1] header_key_recv (HKr) + rcAD → SameRatchet");
                                    mbedtls_gcm_init(&gcm);
                                    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, rs->header_key_recv, 256);
                                    hdr_ret = mbedtls_gcm_auth_decrypt(&gcm, eh_body_len,
                                                eh_iv_ptr, 16, rs->assoc_data, 112,
                                                eh_tag_ptr, 16, eh_body_ptr, header_plain);
                                    mbedtls_gcm_free(&gcm);
                                    ESP_LOGI(TAG, "      Result: %d %s", hdr_ret, hdr_ret == 0 ? "✅ SUCCESS!" : "❌ MAC mismatch");
                                    if (hdr_ret == 0) { header_decrypted = true; decrypt_mode = 0; }

                                    // ---- Try 2: next_header_key_recv + rcAD → AdvanceRatchet ----
                                    if (!header_decrypted) {
                                        ESP_LOGI(TAG, "      [Try 2] next_header_key_recv (NHKr) + rcAD → AdvanceRatchet");
                                        mbedtls_gcm_init(&gcm);
                                        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, rs->next_header_key_recv, 256);
                                        hdr_ret = mbedtls_gcm_auth_decrypt(&gcm, eh_body_len,
                                                    eh_iv_ptr, 16, rs->assoc_data, 112,
                                                    eh_tag_ptr, 16, eh_body_ptr, header_plain);
                                        mbedtls_gcm_free(&gcm);
                                        ESP_LOGI(TAG, "      Result: %d %s", hdr_ret, hdr_ret == 0 ? "✅ SUCCESS!" : "❌ MAC mismatch");
                                        if (hdr_ret == 0) { header_decrypted = true; decrypt_mode = 1; }
                                    }

                                    // ---- Debug Fallbacks (Tries 3-6) — should NOT be needed ----
                                    if (!header_decrypted) {
                                        ESP_LOGW(TAG, "      ⚠️ Normal keys failed — trying debug fallbacks...");

                                        // Try 3: header_key_send + normal rcAD
                                        ESP_LOGI(TAG, "      [Try 3] header_key_send + rcAD");
                                        mbedtls_gcm_init(&gcm);
                                        mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, rs->header_key_send, 256);
                                        hdr_ret = mbedtls_gcm_auth_decrypt(&gcm, eh_body_len,
                                                    eh_iv_ptr, 16, rs->assoc_data, 112,
                                                    eh_tag_ptr, 16, eh_body_ptr, header_plain);
                                        mbedtls_gcm_free(&gcm);
                                        ESP_LOGI(TAG, "      Result: %d %s", hdr_ret, hdr_ret == 0 ? "✅ SUCCESS!" : "❌");
                                        if (hdr_ret == 0) { header_decrypted = true; decrypt_mode = 1; }
                                    }

                                    if (!header_decrypted) {
                                        // Tries 4-6: SWAPPED rcAD variants
                                        uint8_t rcAD_swapped[112];
                                        memcpy(rcAD_swapped, rs->assoc_data + 56, 56);
                                        memcpy(rcAD_swapped + 56, rs->assoc_data, 56);

                                        const uint8_t *fallback_keys[] = { rs->header_key_recv, rs->next_header_key_recv, rs->header_key_send };
                                        const char *fallback_names[] = { "HKr+swapped", "NHKr+swapped", "HKs+swapped" };
                                        for (int fi = 0; fi < 3 && !header_decrypted; fi++) {
                                            ESP_LOGI(TAG, "      [Try %d] %s", fi + 4, fallback_names[fi]);
                                            mbedtls_gcm_init(&gcm);
                                            mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, fallback_keys[fi], 256);
                                            hdr_ret = mbedtls_gcm_auth_decrypt(&gcm, eh_body_len,
                                                        eh_iv_ptr, 16, rcAD_swapped, 112,
                                                        eh_tag_ptr, 16, eh_body_ptr, header_plain);
                                            mbedtls_gcm_free(&gcm);
                                            ESP_LOGI(TAG, "      Result: %d %s", hdr_ret, hdr_ret == 0 ? "✅ SUCCESS!" : "❌");
                                            if (hdr_ret == 0) { header_decrypted = true; decrypt_mode = 1; }
                                        }
                                    }

                                    // Saved X3DH keys as last resort
                                    const uint8_t *orig_hk = ratchet_get_saved_hk();
                                    const uint8_t *orig_nhk = ratchet_get_saved_nhk();
                                    if (!header_decrypted && orig_hk && orig_nhk) {
                                        ESP_LOGW(TAG, "      === Last resort: SAVED X3DH keys ===");
                                        const uint8_t *saved_keys[] = { orig_nhk, orig_hk };
                                        const char *saved_names[] = { "saved_nhk+rcAD", "saved_hk+rcAD" };
                                        for (int si = 0; si < 2 && !header_decrypted; si++) {
                                            ESP_LOGI(TAG, "      [Try %d] %s", si + 7, saved_names[si]);
                                            mbedtls_gcm_init(&gcm);
                                            mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, saved_keys[si], 256);
                                            hdr_ret = mbedtls_gcm_auth_decrypt(&gcm, eh_body_len,
                                                        eh_iv_ptr, 16, rs->assoc_data, 112,
                                                        eh_tag_ptr, 16, eh_body_ptr, header_plain);
                                            mbedtls_gcm_free(&gcm);
                                            ESP_LOGI(TAG, "      Result: %d %s", hdr_ret, hdr_ret == 0 ? "✅ SUCCESS!" : "❌");
                                            if (hdr_ret == 0) { header_decrypted = true; decrypt_mode = 1; }
                                        }
                                    }

                                    if (!header_decrypted) {
                                        ESP_LOGE(TAG, "      ❌ Header decrypt FAILED with ALL keys!");
                                    }

                                    // Log which mode was used
                                    ESP_LOGI(TAG, "      decrypt_mode: %d (%s)", decrypt_mode,
                                             decrypt_mode == 0 ? "SameRatchet" :
                                             decrypt_mode == 1 ? "AdvanceRatchet" : "FAILED");
                                    
                                    // === Ergebnis ===
                                    ESP_LOGI(TAG, "");
                                    if (header_decrypted) {
                                        ESP_LOGI(TAG, "      ╔═══════════════════════════════════════════════════════╗");
                                        ESP_LOGI(TAG, "      ║  🎉 HEADER DECRYPT SUCCESS!                           ║");
                                        ESP_LOGI(TAG, "      ╚═══════════════════════════════════════════════════════╝");

                                        // DEBUG 32b: Hex dump decrypted MsgHeader
                                        ESP_LOGI(TAG, "      MsgHeader raw (first 128 of %u bytes):", eh_body_len);
                                        for (int di = 0; di < 128 && di < (int)eh_body_len; di += 16) {
                                            char hex[64] = {0};
                                            int hx = 0;
                                            for (int dj = 0; dj < 16 && (di+dj) < (int)eh_body_len; dj++) {
                                                hx += sprintf(&hex[hx], "%02x ", header_plain[di+dj]);
                                            }
                                            ESP_LOGI(TAG, "        +%04d: %s", di, hex);
                                        }
                                        
                                        // Parse MsgHeader
                                        uint16_t hdr_content_len = (header_plain[0] << 8) | header_plain[1];
                                        uint16_t hdr_msg_version = (header_plain[2] << 8) | header_plain[3];
                                        uint8_t hdr_key_len = header_plain[4];
                                        
                                        ESP_LOGI(TAG, "      MsgHeader contentLen: %u", hdr_content_len);
                                        ESP_LOGI(TAG, "      MsgHeader version: %u", hdr_msg_version);
                                        ESP_LOGI(TAG, "      MsgHeader DH key len: %u (expect 68 = SPKI)", hdr_key_len);
                                        
                                        if (hdr_key_len == 68) {
                                            // SPKI header at [5..16], raw X448 key at [17..72]
                                            ESP_LOGI(TAG, "      Peer DH SPKI: %02x%02x%02x%02x...",
                                                     header_plain[5], header_plain[6], header_plain[7], header_plain[8]);
                                            ESP_LOGI(TAG, "      Peer DH Key (X448): ");
                                            printf("         ");
                                            for (int i = 17; i < 73; i++) printf("%02x", header_plain[i]);
                                            printf("\n");
                                        }
                                        
                                        // === v3 MsgHeader Parser (dynamic offsets, PQ-aware) ===
                                        // Layout: [2B contentLen][2B version][1B keyLen][N key][KEM field][4B PN][4B Ns][padding]
                                        int mhp = 5 + hdr_key_len;  // Skip: 2B contentLen + 2B version + 1B keyLen + key
                                        ESP_LOGI(TAG, "      MsgHeader parse: after DH key, offset=%d", mhp);

                                        if (hdr_msg_version >= 3) {
                                            uint8_t kem_byte = header_plain[mhp];
                                            if (kem_byte == 0x30) {
                                                // '0' = Nothing (no PQ) — 1 byte only
                                                ESP_LOGI(TAG, "      KEM: 0x30 '0' (Nothing) at offset %d", mhp);
                                                mhp += 1;
                                            } else if (kem_byte == 0x31) {
                                                // '1' = Just — PQ KEM data follows
                                                mhp += 1;  // consume '1' tag
                                                uint8_t kem_state = header_plain[mhp];
                                                ESP_LOGI(TAG, "      KEM: Just, state=0x%02x '%c' at offset %d",
                                                         kem_state, kem_state, mhp);
                                                mhp += 1;  // consume state tag

                                                if (kem_state == 0x50) {
                                                    // 'P' = Proposed: 2B len + PK data
                                                    uint16_t pk_len = (header_plain[mhp] << 8) | header_plain[mhp+1];
                                                    mhp += 2;
                                                    ESP_LOGI(TAG, "      KEM Proposed: skip %u bytes PK", pk_len);
                                                    mhp += pk_len;
                                                } else if (kem_state == 0x41) {
                                                    // 'A' = Accepted: 2B len + CT, then 2B len + PK
                                                    uint16_t ct_len_kem = (header_plain[mhp] << 8) | header_plain[mhp+1];
                                                    mhp += 2;
                                                    ESP_LOGI(TAG, "      KEM Accepted: skip %u bytes CT", ct_len_kem);
                                                    mhp += ct_len_kem;
                                                    uint16_t pk_len = (header_plain[mhp] << 8) | header_plain[mhp+1];
                                                    mhp += 2;
                                                    ESP_LOGI(TAG, "      KEM Accepted: skip %u bytes PK", pk_len);
                                                    mhp += pk_len;
                                                } else {
                                                    ESP_LOGW(TAG, "      ⚠️ Unknown KEM state: 0x%02x — trying to continue", kem_state);
                                                }
                                            } else {
                                                ESP_LOGW(TAG, "      ⚠️ Unknown KEM tag: 0x%02x at offset %d", kem_byte, mhp);
                                                mhp += 1;  // skip unknown byte, hope for the best
                                            }
                                        }
                                        // else: v2 has no KEM field, mhp already correct

                                        ESP_LOGI(TAG, "      PN/Ns at offset %d", mhp);
                                        uint32_t hdr_pn = (header_plain[mhp] << 24) | (header_plain[mhp+1] << 16) |
                                                          (header_plain[mhp+2] << 8)  | header_plain[mhp+3];
                                        mhp += 4;
                                        uint32_t hdr_ns = (header_plain[mhp] << 24) | (header_plain[mhp+1] << 16) |
                                                          (header_plain[mhp+2] << 8)  | header_plain[mhp+3];
                                        mhp += 4;
                                        ESP_LOGI(TAG, "      PN (prev chain): %u", hdr_pn);
                                        ESP_LOGI(TAG, "      Ns (msg number): %u", hdr_ns);
                                        
                                        ESP_LOGI(TAG, "      Full MsgHeader (first 88 bytes):");
                                        printf("         ");
                                        for (int i = 0; i < 88 && i < (int)eh_body_len; i++) {
                                            printf("%02x", header_plain[i]);
                                            if ((i+1) % 32 == 0) printf("\n         ");
                                        }
                                        printf("\n");
                                        
                                        ESP_LOGI(TAG, "      🎯 Phase 2b — Body Decrypt + Zstd...");
                                        
                                        // ================================================================
                                        // 🐰 PHASE 2b: Body Decrypt (Session 20) + Zstd (Session 21)
                                        // peer DH key from MsgHeader + current ratchet state
                                        // ================================================================
                                        {
                                            // Peer's new DH key (raw X448, 56 bytes from header_plain)
                                            uint8_t peer_dh_from_header[56];
                                            memcpy(peer_dh_from_header, &header_plain[17], 56);
                                            
                                            // ================================================================
                                            // FIX 2: Reconstruct pointers dynamically with validation
                                            // EncRatchetMessage = [2B prefix][emHeader][16B authTag][encrypted_body]
                                            // em_header points AFTER 2B prefix, em_header_len = wire value
                                            // ================================================================
                                            const uint8_t *body_em_auth_tag = em_header + em_header_len;
                                            const uint8_t *body_em_body = em_header + em_header_len + 16;
                                            
                                            // Calculate offset and length relative to client_msg
                                            size_t auth_tag_offset_in_msg = (size_t)(body_em_auth_tag - client_msg);
                                            size_t body_offset_in_msg = (size_t)(body_em_body - client_msg);
                                            
                                            // Sanity check: body_offset must be within original_len
                                            size_t body_em_body_len = 0;
                                            if (body_offset_in_msg < original_len) {
                                                body_em_body_len = original_len - body_offset_in_msg;
                                            }
                                            
                                            ESP_LOGI(TAG, "");
                                            ESP_LOGI(TAG, "      📐 Body Decrypt Pointers:");
                                            ESP_LOGI(TAG, "         em_header at client_msg+%d, em_header_len=%u",
                                                     (int)(em_header - client_msg), em_header_len);
                                            ESP_LOGI(TAG, "         emAuthTag at client_msg+%zu",
                                                     auth_tag_offset_in_msg);
                                            ESP_LOGI(TAG, "         emBody at client_msg+%zu, len=%zu",
                                                     body_offset_in_msg, body_em_body_len);
                                            ESP_LOGI(TAG, "         original_len=%u", original_len);
                                            
                                            // Validate pointers are within bounds
                                            if (body_offset_in_msg >= original_len || body_em_body_len == 0) {
                                                ESP_LOGE(TAG, "      ❌ emBody exceeds message bounds!");
                                                ESP_LOGE(TAG, "         body_offset=%zu >= original_len=%u",
                                                         body_offset_in_msg, original_len);
                                                free(header_plain);
                                                goto skip_header_decrypt;
                                            }
                                            
                                            if (auth_tag_offset_in_msg + 16 > original_len) {
                                                ESP_LOGE(TAG, "      ❌ emAuthTag exceeds message bounds!");
                                                free(header_plain);
                                                goto skip_header_decrypt;
                                            }
                                            
                                            uint8_t *body_plain = malloc(body_em_body_len + 16);
                                            if (body_plain) {
                                                size_t body_plain_len = 0;
                                                int body_ret = ratchet_decrypt_body(
                                                    decrypt_mode == 0 ? RATCHET_MODE_SAME : RATCHET_MODE_ADVANCE,
                                                    peer_dh_from_header,
                                                    hdr_pn, hdr_ns,
                                                    em_header, (size_t)em_header_len,
                                                    body_em_auth_tag,
                                                    body_em_body, body_em_body_len,
                                                    body_plain, &body_plain_len
                                                );
                                                
                                                if (body_ret == 0) {
                                                    ESP_LOGI(TAG, "");
                                                    ESP_LOGI(TAG, "      ╔═══════════════════════════════════════════════╗");
                                                    ESP_LOGI(TAG, "      ║  🎉🎉🎉 ConnInfo DECRYPTED! 🎉🎉🎉          ║");
                                                    ESP_LOGI(TAG, "      ╚═══════════════════════════════════════════════╝");
                                                    ESP_LOGI(TAG, "      Plaintext: %zu bytes", body_plain_len);
                                                    ESP_LOGI(TAG, "      Tag byte: 0x%02X '%c'",
                                                             body_plain[0],
                                                             (body_plain[0] >= 0x20 && body_plain[0] < 0x7f) ? (char)body_plain[0] : '?');
                                                    
                                                    // Auftrag 37b: Hex+ASCII dump first 256 bytes
                                                    {
                                                        size_t dump_len = body_plain_len < 256 ? body_plain_len : 256;
                                                        ESP_LOGI(TAG, "      📋 Payload dump (%zu of %zu bytes):", dump_len, body_plain_len);
                                                        for (size_t di = 0; di < dump_len; di += 16) {
                                                            char hex[64] = {0};
                                                            char asc[20] = {0};
                                                            int hx = 0;
                                                            for (size_t dj = 0; dj < 16 && (di+dj) < dump_len; dj++) {
                                                                uint8_t b = body_plain[di+dj];
                                                                hx += sprintf(&hex[hx], "%02x ", b);
                                                                asc[dj] = (b >= 0x20 && b < 0x7f) ? (char)b : '.';
                                                            }
                                                            ESP_LOGI(TAG, "        +%04zu: %-48s |%s|", di, hex, asc);
                                                        }
                                                    }
                                                    
                                                    // === Tag-specific handling ===
                                                    if (body_plain[0] == 'D') {
                                                        // AgentConnInfoReply: 'D' + queues + JSON
                                                        ESP_LOGI(TAG, "      Tag: 'D' = AgentConnInfoReply");

                                                        // ======== Auftrag 41a: SMPQueueInfo boundary ========
                                                        {
                                                            size_t json_offset = 0;
                                                            for (size_t i = 1; i < body_plain_len && i < 300; i++) {
                                                                if (body_plain[i] == '{') { json_offset = i; break; }
                                                            }
                                                            ESP_LOGI(TAG, "      ======================================");
                                                            ESP_LOGI(TAG, "      📋 41a: JSON '{' found at offset %zu", json_offset);
                                                            ESP_LOGI(TAG, "      📋 41a: SMPQueueInfo = bytes [1..%zu] (%zu bytes)",
                                                                     json_offset > 0 ? json_offset - 1 : 0,
                                                                     json_offset > 0 ? json_offset - 1 : 0);
                                                            ESP_LOGI(TAG, "      ======================================");
                                                        }
                                                        // ======== Ende Auftrag 41a ========

                                                        for (size_t i = 1; i < body_plain_len && i < 200; i++) {
                                                            if (body_plain[i] == '{') {
                                                                int json_end = (body_plain_len - i > 200) ? 200 : (int)(body_plain_len - i);
                                                                ESP_LOGI(TAG, "      JSON at offset %zu: %.*s", i, json_end, &body_plain[i]);
                                                                break;
                                                            }
                                                        }
                                                    }
                                                    // ============================================================
                                                    // 🐰 SESSION 21: Zstd Decompression for 'I' Tag (ConnInfo)
                                                    // Structure: 'I' + framing bytes + Zstd_frame(28 b5 2f fd...)
                                                    // ============================================================
                                                    else if (body_plain_len > 6 && body_plain[0] == 'I') {
                                                        ESP_LOGI(TAG, "      Tag: 'I' = AgentMessage INFO (peer ConnInfo)");

                                                        // ======== Auftrag 42a: Peer Sender Auth Key Status ========
                                                        ESP_LOGI(TAG, "      ==========================================");
                                                        ESP_LOGI(TAG, "      📋 42a: PEER SENDER AUTH KEY (für KEY Command)");
                                                        ESP_LOGI(TAG, "      has_peer_sender_auth: %s", has_peer_sender_auth ? "YES" : "NO");
                                                        if (has_peer_sender_auth) {
                                                            ESP_LOG_BUFFER_HEX("      peer_sender_auth_key (44 bytes)", peer_sender_auth_key, 44);
                                                        }
                                                        ESP_LOGI(TAG, "      ==========================================");
                                                        // ======== Ende Auftrag 42a ========

                                                        ESP_LOGI(TAG, "      body_plain[0-7]: %02x %02x %02x %02x %02x %02x %02x %02x",
                                                                 body_plain[0], body_plain[1], body_plain[2], body_plain[3],
                                                                 body_plain[4], body_plain[5], body_plain[6],
                                                                 body_plain_len > 7 ? body_plain[7] : 0);
                                                        
                                                        // Search for Zstd magic bytes (28 b5 2f fd) in first 16 bytes
                                                        int zstd_offset = -1;
                                                        for (int si = 1; si < 16 && si + 3 < (int)body_plain_len; si++) {
                                                            if (body_plain[si]   == 0x28 && body_plain[si+1] == 0xb5 &&
                                                                body_plain[si+2] == 0x2f && body_plain[si+3] == 0xfd) {
                                                                zstd_offset = si;
                                                                break;
                                                            }
                                                        }
                                                        
                                                        if (zstd_offset >= 0) {
                                                            const uint8_t *zstd_frame = &body_plain[zstd_offset];
                                                            size_t zstd_frame_len = body_plain_len - zstd_offset;
                                                            
                                                            ESP_LOGI(TAG, "      🔧 Zstd frame found at offset %d, frame_len=%zu",
                                                                     zstd_offset, zstd_frame_len);
                                                            
                                                            // Get decompressed size from frame header
                                                            unsigned long long decomp_size = ZSTD_getFrameContentSize(
                                                                zstd_frame, zstd_frame_len);
                                                            
                                                            if (decomp_size == ZSTD_CONTENTSIZE_UNKNOWN) {
                                                                ESP_LOGW(TAG, "      Zstd: content size unknown, using 16KB buffer");
                                                                decomp_size = 16384;
                                                            } else if (decomp_size == ZSTD_CONTENTSIZE_ERROR) {
                                                                ESP_LOGE(TAG, "      ❌ Zstd: not valid compressed data!");
                                                            } else {
                                                                ESP_LOGI(TAG, "      Zstd: decompressed size = %llu bytes", decomp_size);
                                                            }
                                                            
                                                            if (decomp_size != ZSTD_CONTENTSIZE_ERROR && decomp_size < 65536) {
                                                                char *json_buf = (char *)malloc(decomp_size + 1);
                                                                if (json_buf) {
                                                                    size_t zresult = ZSTD_decompress(
                                                                        json_buf, decomp_size,
                                                                        zstd_frame, zstd_frame_len);
                                                                    
                                                                    if (ZSTD_isError(zresult)) {
                                                                        ESP_LOGE(TAG, "      ❌ Zstd decompress failed: %s",
                                                                                 ZSTD_getErrorName(zresult));
                                                                    } else {
                                                                        json_buf[zresult] = '\0';
                                                                        
                                                                        ESP_LOGI(TAG, "");
                                                                        ESP_LOGI(TAG, "      ╔═══════════════════════════════════════════════╗");
                                                                        ESP_LOGI(TAG, "      ║  🎉 ConnInfo JSON DECOMPRESSED!               ║");
                                                                        ESP_LOGI(TAG, "      ╚═══════════════════════════════════════════════╝");
                                                                        ESP_LOGI(TAG, "      Decompressed: %zu bytes", zresult);
                                                                        
                                                                        // Print JSON in chunks (may be long)
                                                                        size_t printed = 0;
                                                                        while (printed < zresult) {
                                                                            size_t chunk = zresult - printed;
                                                                            if (chunk > 200) chunk = 200;
                                                                            ESP_LOGI(TAG, "      %.*s", (int)chunk, &json_buf[printed]);
                                                                            printed += chunk;
                                                                        }
                                                                    }
                                                                    free(json_buf);
                                                                } else {
                                                                    ESP_LOGE(TAG, "      ❌ malloc json_buf failed! (%llu bytes)", decomp_size);
                                                                }
                                                            }
                                                        } else {
                                                            ESP_LOGW(TAG, "      No Zstd magic (28 b5 2f fd) found in first 16 bytes");
                                                            ESP_LOGW(TAG, "      Payload may be uncompressed or different format");
                                                        }
                                                    } else {
                                                        ESP_LOGW(TAG, "      Unknown tag: 0x%02X '%c'",
                                                                 body_plain[0],
                                                                 (body_plain[0] >= 0x20 && body_plain[0] < 0x7f) ? (char)body_plain[0] : '?');
                                                    }
                                                } else {
                                                    ESP_LOGE(TAG, "      ❌ Body decrypt failed (ret=%d)", body_ret);
                                                }
                                                free(body_plain);
                                            } else {
                                                ESP_LOGE(TAG, "      ❌ malloc for body_plain (%zu bytes) failed!", body_em_body_len);
                                            }
                                        }
                                        // ================================================================
                                        // END PHASE 2b
                                        // ================================================================
                                    } else {
                                        ESP_LOGE(TAG, "      ╔═══════════════════════════════════════════════════════╗");
                                        ESP_LOGE(TAG, "      ║  ❌ ALL 8 HEADER DECRYPT ATTEMPTS FAILED              ║");
                                        ESP_LOGE(TAG, "      ╚═══════════════════════════════════════════════════════╝");
                                        ESP_LOGE(TAG, "      Possible causes:");
                                        ESP_LOGE(TAG, "      1. X3DH DH computation wrong (byte order?)");
                                        ESP_LOGE(TAG, "      2. HKDF info string wrong");
                                        ESP_LOGE(TAG, "      3. HKDF output assignment wrong (hk/nhk/rk)");
                                        ESP_LOGE(TAG, "      4. rcAD key order wrong");
                                        ESP_LOGE(TAG, "      5. 16-byte IV not compatible (standard GCM uses 12)");
                                        ESP_LOGE(TAG, "      6. X3DH keys from PEER side differ (asymmetric X3DH)");
                                        ESP_LOGE(TAG, "      Need to compare X3DH intermediate values with Python!");
                                    }
                                    
                                    // ================================================================
                                    // FIX 1 continued: Free header_plain buffer
                                    // ================================================================
                                    free(header_plain);
                                }
                                
                                skip_header_decrypt:
                                // ================================================================
                                // END PHASE 2a
                                // ================================================================
                            }
                            
                            // ================================================================
                            // ======== Auftrag 42c+42d: Reconnect + SUB + KEY + HELLO + Read Reply ========
                            {
                                ESP_LOGI(TAG, "");
                                if (!has_peer_sender_auth) {
                                    ESP_LOGW(TAG, "      ⚠️ 42c: No peer auth key!");
                                    goto skip_42d;
                                }
                                
                                // === Step 1: Reconnect to Reply Queue ===
                                ESP_LOGI(TAG, "      📋 42c: Reconnecting to Reply Queue for KEY...");
                                if (!queue_reconnect()) {
                                    ESP_LOGE(TAG, "      ❌ 42c: Reconnect failed!");
                                    goto skip_42d;
                                }
                                ESP_LOGI(TAG, "      ✅ 42c: Reconnected!");
                                
                                // === Step 2: SUB on Reply Queue ===
                                if (!queue_subscribe()) {
                                    ESP_LOGE(TAG, "      ❌ 42c: SUB failed!");
                                    goto skip_42d;
                                }
                                ESP_LOGI(TAG, "      ✅ 42c: SUB OK!");
                                
                                // === Step 3: KEY command ===
                                if (!queue_send_key(peer_sender_auth_key, 44)) {
                                    ESP_LOGE(TAG, "      ❌ 42c: KEY failed!");
                                    goto skip_42d;
                                }
                                ESP_LOGI(TAG, "      ✅ 42c: KEY accepted!");
                                
                                // === Step 4: Send HELLO on Contact Queue Q_A ===
                                ESP_LOGI(TAG, "");
                                ESP_LOGI(TAG, "      📋 42d: Sending HELLO on Contact Queue Q_A...");
                                {
                                    contact_t *hello_contact = &contacts_db.contacts[0];
                                    if (peer_send_hello(hello_contact)) {
                                        ESP_LOGI(TAG, "      ✅ 42d: HELLO sent!");
                                    } else {
                                        ESP_LOGE(TAG, "      ❌ 42d: HELLO send failed!");
                                    }
                                }
                                
                                // === Step 5: Read + Decrypt Reply Queue message ===
                                ESP_LOGI(TAG, "");
                                ESP_LOGI(TAG, "      📋 42d: Reading Reply Queue message...");
                                {
                                    uint8_t *rq_block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
                                    if (!rq_block) {
                                        ESP_LOGE(TAG, "      ❌ malloc failed!");
                                        goto skip_42d;
                                    }
                                    
                                    // Read up to 3 blocks (might get OK, END, MSG)
                                    for (int rq_try = 0; rq_try < 3; rq_try++) {
                                        int rq_content_len = queue_read_raw(rq_block, SMP_BLOCK_SIZE, 15000);
                                        if (rq_content_len < 0) {
                                            ESP_LOGW(TAG, "      📋 42d: No more data from Reply Queue (try %d)", rq_try);
                                            break;
                                        }
                                        
                                        uint8_t *rq_resp = rq_block + 2;
                                        
                                        // Parse SMP transport
                                        int rp = 0;
                                        if (rq_resp[rp] == 1) rp++;  // txCount
                                        rp += 2;  // txLen
                                        
                                        int rq_authLen = rq_resp[rp++]; rp += rq_authLen;
                                        int rq_sessLen = rq_resp[rp++]; rp += rq_sessLen;
                                        int rq_corrLen = rq_resp[rp++]; rp += rq_corrLen;
                                        int rq_entLen = rq_resp[rp++]; rp += rq_entLen;
                                        
                                        // Check command type
                                        if (rp + 1 < rq_content_len && rq_resp[rp] == 'O' && rq_resp[rp+1] == 'K') {
                                            ESP_LOGI(TAG, "      📋 42d [%d]: OK", rq_try);
                                            continue;
                                        }
                                        if (rp + 2 < rq_content_len && rq_resp[rp] == 'E' && rq_resp[rp+1] == 'N' && rq_resp[rp+2] == 'D') {
                                            ESP_LOGI(TAG, "      📋 42d [%d]: END", rq_try);
                                            continue;
                                        }
                                        if (!(rp + 3 < rq_content_len && rq_resp[rp] == 'M' && rq_resp[rp+1] == 'S' && rq_resp[rp+2] == 'G')) {
                                            ESP_LOGW(TAG, "      📋 42d [%d]: Unknown command: %c%c%c", rq_try,
                                                     rq_resp[rp], rq_resp[rp+1], rq_resp[rp+2]);
                                            continue;
                                        }
                                        
                                        // === MSG received! ===
                                        rp += 4;  // Skip "MSG "
                                        uint8_t rq_msgIdLen = rq_resp[rp++];
                                        uint8_t rq_msg_id[24] = {0};
                                        if (rq_msgIdLen > 24) rq_msgIdLen = 24;
                                        memcpy(rq_msg_id, &rq_resp[rp], rq_msgIdLen);
                                        rp += rq_msgIdLen;
                                        
                                        int rq_enc_len = rq_content_len - rp;
                                        
                                        ESP_LOGI(TAG, "");
                                        ESP_LOGI(TAG, "      ╔══════════════════════════════════════════════════════╗");
                                        ESP_LOGI(TAG, "      ║  📋 42d: REPLY QUEUE MSG RECEIVED!                    ║");
                                        ESP_LOGI(TAG, "      ╚══════════════════════════════════════════════════════╝");
                                        ESP_LOGI(TAG, "      MsgId: %02x%02x%02x%02x...", rq_msg_id[0], rq_msg_id[1], rq_msg_id[2], rq_msg_id[3]);
                                        ESP_LOGI(TAG, "      Encrypted: %d bytes", rq_enc_len);
                                        
                                        // --- Layer 1: Server-level decrypt ---
                                        uint8_t rq_server_nonce[24] = {0};
                                        memcpy(rq_server_nonce, rq_msg_id, rq_msgIdLen);
                                        
                                        uint8_t *rq_server_plain = malloc(rq_enc_len);
                                        if (!rq_server_plain) break;
                                        
                                        if (crypto_box_open_easy_afternm(rq_server_plain, &rq_resp[rp], rq_enc_len,
                                                                          rq_server_nonce, our_queue.shared_secret) != 0) {
                                            ESP_LOGE(TAG, "      ❌ Server-level decrypt FAILED!");
                                            free(rq_server_plain);
                                            break;
                                        }
                                        int rq_plain_len = rq_enc_len - crypto_box_MACBYTES;
                                        ESP_LOGI(TAG, "      ✅ Server decrypt OK! (%d bytes)", rq_plain_len);
                                        
                                        // --- Layer 2: Parse ClientMsgEnvelope ---
                                        // Reply Queue has 2-byte length prefix
                                        int rq_prefix = 0;
                                        uint16_t rq_data_len = (rq_server_plain[0] << 8) | rq_server_plain[1];
                                        if (rq_data_len > 0 && rq_data_len < rq_plain_len) {
                                            rq_prefix = 2;
                                        }
                                        const uint8_t *rq_env = rq_server_plain + rq_prefix;
                                        size_t rq_env_len = rq_prefix ? rq_data_len : rq_plain_len;
                                        
                                        ESP_LOGI(TAG, "      Envelope: prefix=%d, len=%zu", rq_prefix, rq_env_len);
                                        ESP_LOGI(TAG, "      First 32 bytes:");
                                        printf("         ");
                                        for (int i = 0; i < 32 && i < (int)rq_env_len; i++) printf("%02x ", rq_env[i]);
                                        printf("\n");
                                        
                                        // Parse envelope: [0-11]=header, [12]=maybe_corrId, [13]=maybe_e2e
                                        int rq_off = 12;
                                        uint8_t rq_mcorr = rq_env[rq_off++];
                                        uint8_t rq_me2e = rq_env[rq_off++];
                                        
                                        ESP_LOGI(TAG, "      maybe_corrId=0x%02x '%c', maybe_e2e=0x%02x '%c'",
                                                 rq_mcorr, (rq_mcorr >= 0x20 && rq_mcorr < 0x7f) ? rq_mcorr : '?',
                                                 rq_me2e, (rq_me2e >= 0x20 && rq_me2e < 0x7f) ? rq_me2e : '?');
                                        
                                        // Extract E2E sender key
                                        const uint8_t rq_spki[] = {0x30,0x2a,0x30,0x05,0x06,0x03,0x2b,0x65,0x6e,0x03,0x21,0x00};
                                        uint8_t rq_sender_pub[32] = {0};
                                        bool rq_have_key = false;
                                        
                                        if (rq_mcorr == '1' && (rq_me2e == ',' || rq_me2e == '0')) {
                                            // corrId SPKI doubles as E2E key
                                            if (memcmp(&rq_env[rq_off], rq_spki, 12) == 0) {
                                                memcpy(rq_sender_pub, &rq_env[rq_off + 12], 32);
                                                rq_have_key = true;
                                                ESP_LOGI(TAG, "      ✅ E2E key from corrId SPKI");
                                            }
                                            rq_off += 44;
                                        } else if (rq_mcorr == ',' || rq_mcorr == '0') {
                                            if (reply_queue_e2e_peer_valid) {
                                                memcpy(rq_sender_pub, reply_queue_e2e_peer_public, 32);
                                                rq_have_key = true;
                                                ESP_LOGI(TAG, "      ✅ Using pre-shared E2E key");
                                            }
                                        } else if (rq_mcorr == '1' && rq_me2e == '1') {
                                            rq_off += 44;  // skip corrId SPKI
                                            uint8_t e2e_len = rq_env[rq_off++];
                                            if (e2e_len == 44 && memcmp(&rq_env[rq_off], rq_spki, 12) == 0) {
                                                memcpy(rq_sender_pub, &rq_env[rq_off + 12], 32);
                                                rq_have_key = true;
                                                ESP_LOGI(TAG, "      ✅ Separate E2E key");
                                            }
                                            rq_off += 44;
                                        }
                                        
                                        if (!rq_have_key) {
                                            ESP_LOGE(TAG, "      ❌ No E2E key found!");
                                            // Dump first 80 bytes for analysis
                                            printf("         Full envelope hex:\n         ");
                                            for (int i = 0; i < 80 && i < (int)rq_env_len; i++) {
                                                printf("%02x ", rq_env[i]);
                                                if ((i+1) % 16 == 0) printf("\n         ");
                                            }
                                            printf("\n");
                                            free(rq_server_plain);
                                            break;
                                        }
                                        
                                        ESP_LOGI(TAG, "      Sender pub: %02x%02x%02x%02x...",
                                                 rq_sender_pub[0], rq_sender_pub[1], rq_sender_pub[2], rq_sender_pub[3]);
                                        
                                        // Extract nonce + ciphertext
                                        uint8_t rq_cm_nonce[24];
                                        memcpy(rq_cm_nonce, &rq_env[rq_off], 24);
                                        rq_off += 24;
                                        
                                        const uint8_t *rq_e2e_enc = &rq_env[rq_off];
                                        size_t rq_e2e_enc_len = rq_env_len - rq_off;
                                        
                                        ESP_LOGI(TAG, "      cmNonce: %02x%02x%02x%02x...", rq_cm_nonce[0], rq_cm_nonce[1], rq_cm_nonce[2], rq_cm_nonce[3]);
                                        ESP_LOGI(TAG, "      E2E cipher: %zu bytes", rq_e2e_enc_len);
                                        
                                        // --- E2E Decrypt ---
                                        uint8_t *rq_e2e_plain = malloc(rq_e2e_enc_len);
                                        if (!rq_e2e_plain) { free(rq_server_plain); break; }
                                        
                                        int rq_dec_ret = -1;
                                        
                                        // Try Method 0: decrypt_client_msg
                                        {
                                            int nonce_start = rq_off - 24;  // back to nonce in envelope
                                            int enc_block_len = rq_env_len - nonce_start;
                                            uint8_t *m0_plain = malloc(enc_block_len);
                                            if (m0_plain) {
                                                int dec_len = decrypt_client_msg(&rq_env[nonce_start], enc_block_len,
                                                                                  rq_sender_pub, our_queue.e2e_private, m0_plain);
                                                if (dec_len > 0) {
                                                    rq_dec_ret = 0;
                                                    memcpy(rq_e2e_plain, m0_plain, dec_len);
                                                    ESP_LOGI(TAG, "      ✅ E2E decrypt OK (Method 0, %d bytes)", dec_len);
                                                }
                                                free(m0_plain);
                                            }
                                        }
                                        
                                        if (rq_dec_ret != 0) {
                                            rq_dec_ret = crypto_box_open_easy(rq_e2e_plain, rq_e2e_enc, rq_e2e_enc_len,
                                                                               rq_cm_nonce, rq_sender_pub, our_queue.e2e_private);
                                            if (rq_dec_ret == 0) ESP_LOGI(TAG, "      ✅ E2E decrypt OK (Method 1)");
                                        }
                                        
                                        if (rq_dec_ret != 0) {
                                            uint8_t rq_dh[32];
                                            crypto_scalarmult(rq_dh, our_queue.e2e_private, rq_sender_pub);
                                            rq_dec_ret = crypto_secretbox_open_easy(rq_e2e_plain, rq_e2e_enc, rq_e2e_enc_len,
                                                                                     rq_cm_nonce, rq_dh);
                                            if (rq_dec_ret == 0) ESP_LOGI(TAG, "      ✅ E2E decrypt OK (Method 2)");
                                            sodium_memzero(rq_dh, 32);
                                        }
                                        
                                        if (rq_dec_ret != 0) {
                                            ESP_LOGE(TAG, "      ❌ E2E decrypt FAILED (all methods)!");
                                            printf("         sender_pub: "); for(int i=0;i<32;i++) printf("%02x",rq_sender_pub[i]); printf("\n");
                                            printf("         our_e2e_priv: "); for(int i=0;i<32;i++) printf("%02x",our_queue.e2e_private[i]); printf("\n");
                                            printf("         cm_nonce: "); for(int i=0;i<24;i++) printf("%02x",rq_cm_nonce[i]); printf("\n");
                                            free(rq_e2e_plain);
                                            free(rq_server_plain);
                                            break;
                                        }
                                        
                                        int rq_e2e_plain_len = rq_e2e_enc_len - 16;
                                        
                                        // --- Layer 3: unPad + Parse PrivHeader + Agent Tag ---
                                        uint16_t rq_orig_len = (rq_e2e_plain[0] << 8) | rq_e2e_plain[1];
                                        uint8_t *rq_client_msg = rq_e2e_plain + 2;
                                        uint8_t rq_priv_tag = rq_client_msg[0];
                                        
                                        ESP_LOGI(TAG, "      unPad: origLen=%u, total=%d", rq_orig_len, rq_e2e_plain_len);
                                        ESP_LOGI(TAG, "      PrivHeader: 0x%02x '%c'", rq_priv_tag,
                                                 (rq_priv_tag >= 0x20 && rq_priv_tag < 0x7f) ? rq_priv_tag : '?');
                                        
                                        // === Auftrag 43b: Q_B POST-E2E ANALYSIS ===
                                        // Verify: Real HELLO or False Positive?
                                        ESP_LOGI(TAG, "");
                                        ESP_LOGI(TAG, "      === Q_B POST-E2E ANALYSIS (43b) ===");
                                        ESP_LOGI(TAG, "      rq_orig_len: %u", rq_orig_len);
                                        ESP_LOGI(TAG, "      rq_e2e_plain_len: %d", rq_e2e_plain_len);
                                        ESP_LOGI(TAG, "      Plausibility: origLen %s",
                                                 (rq_orig_len > 0 && rq_orig_len < 100) ? "SMALL → likely real HELLO" :
                                                 (rq_orig_len > 15000) ? "LARGE → likely ratchet-encrypted!" :
                                                 "MEDIUM → unclear, check hex");
                                        ESP_LOGI(TAG, "      First 32 bytes after unPad (rq_client_msg):");
                                        printf("         HEX: ");
                                        for (int i = 0; i < 32 && i < (int)rq_orig_len; i++) printf("%02x ", rq_client_msg[i]);
                                        printf("\n");
                                        printf("         ASC: ");
                                        for (int i = 0; i < 32 && i < (int)rq_orig_len; i++) {
                                            char c = rq_client_msg[i];
                                            printf("%c", (c >= 32 && c < 127) ? c : '.');
                                        }
                                        printf("\n");
                                        // === Ende 43b Analysis ===
                                        
                                        if (rq_priv_tag == '_') {
                                            // PHEmpty — AgentMsgEnvelope starts at offset 1
                                            ESP_LOGI(TAG, "      PHEmpty — parsing AgentMsgEnvelope...");
                                            uint8_t *rq_agent = rq_client_msg + 1;
                                            int rq_agent_len = rq_orig_len - 1;
                                            
                                            // Auftrag 37b style: version + tag
                                            if (rq_agent_len >= 3) {
                                                uint16_t rq_agent_ver = (rq_agent[0] << 8) | rq_agent[1];
                                                char rq_agent_tag = rq_agent[2];
                                                ESP_LOGI(TAG, "");
                                                ESP_LOGI(TAG, "      ╔══════════════════════════════════════════════════════╗");
                                                ESP_LOGI(TAG, "      ║  🎯 42d: AGENT MESSAGE TAG                           ║");
                                                ESP_LOGI(TAG, "      ╚══════════════════════════════════════════════════════╝");
                                                ESP_LOGI(TAG, "      Agent version: %u", rq_agent_ver);
                                                ESP_LOGI(TAG, "      Agent tag: 0x%02x '%c'", rq_agent_tag,
                                                         (rq_agent_tag >= 0x20 && rq_agent_tag < 0x7f) ? rq_agent_tag : '?');
                                                
                                                if (rq_agent_tag == 'H') {
                                                    ESP_LOGI(TAG, "      🎉🎉🎉 A_HELLO received from peer! 🎉🎉🎉");
                                                    ESP_LOGI(TAG, "      This means: CONNECTED! (CON)");
                                                } else if (rq_agent_tag == 'M') {
                                                    ESP_LOGI(TAG, "      📨 AgentMsgEnvelope Tag 'M' — EncRatchetMessage follows");
                                                    
                                                    // === Auftrag 43b: Ratchet Decrypt Q_B Message ===
                                                    uint8_t *enc_rm = rq_agent + 3;  // After version(2B) + tag(1B)
                                                    int enc_rm_len = rq_agent_len - 3;
                                                    
                                                    ESP_LOGI(TAG, "");
                                                    ESP_LOGI(TAG, "      ╔══════════════════════════════════════════════════════╗");
                                                    ESP_LOGI(TAG, "      ║  43b: Q_B RATCHET DECRYPT                            ║");
                                                    ESP_LOGI(TAG, "      ╚══════════════════════════════════════════════════════╝");
                                                    ESP_LOGI(TAG, "      EncRatchetMessage: %d bytes", enc_rm_len);
                                                    
                                                    if (enc_rm_len < 20) {
                                                        ESP_LOGE(TAG, "      ❌ Too short for EncRatchetMessage!");
                                                    } else if (!ratchet_is_initialized()) {
                                                        ESP_LOGE(TAG, "      ❌ Ratchet not initialized!");
                                                    } else {
                                                        // Step 1: Parse EncRatchetMessage
                                                        uint16_t rm_hdr_len = (enc_rm[0] << 8) | enc_rm[1];
                                                        const uint8_t *rm_header = &enc_rm[2];
                                                        const uint8_t *rm_auth_tag = &enc_rm[2 + rm_hdr_len];
                                                        const uint8_t *rm_body = &enc_rm[2 + rm_hdr_len + 16];
                                                        size_t rm_body_len = enc_rm_len - 2 - rm_hdr_len - 16;
                                                        
                                                        ESP_LOGI(TAG, "      emHeaderLen: %u, emBodyLen: %zu", rm_hdr_len, rm_body_len);
                                                        
                                                        // Step 2: Parse emHeader structure
                                                        uint16_t eh_ver = (rm_header[0] << 8) | rm_header[1];
                                                        const uint8_t *eh_iv = &rm_header[2];
                                                        const uint8_t *eh_tag = &rm_header[18];
                                                        
                                                        int eh_boff;
                                                        uint16_t eh_blen;
                                                        if (rm_hdr_len > 123) {
                                                            eh_blen = (rm_header[34] << 8) | rm_header[35];
                                                            eh_boff = 36;
                                                        } else {
                                                            eh_blen = rm_header[34];
                                                            eh_boff = 35;
                                                        }
                                                        const uint8_t *eh_body = &rm_header[eh_boff];
                                                        
                                                        ESP_LOGI(TAG, "      ehVer: %u, ehBodyLen: %u, ehBodyOff: %d", eh_ver, eh_blen, eh_boff);
                                                        
                                                        // Step 3: Header decrypt — try HKr then NHKr
                                                        ratchet_state_t *rs43 = ratchet_get_state();
                                                        uint8_t *hdr_plain = malloc(eh_blen + 16);
                                                        
                                                        if (!hdr_plain) {
                                                            ESP_LOGE(TAG, "      ❌ malloc header_plain failed!");
                                                        } else {
                                                            mbedtls_gcm_context gcm43;
                                                            int hr;
                                                            bool hdr_ok = false;
                                                            int dec_mode = -1;
                                                            
                                                            // Try 1: HKr + rcAD (SameRatchet)
                                                            mbedtls_gcm_init(&gcm43);
                                                            mbedtls_gcm_setkey(&gcm43, MBEDTLS_CIPHER_ID_AES, rs43->header_key_recv, 256);
                                                            hr = mbedtls_gcm_auth_decrypt(&gcm43, eh_blen,
                                                                        eh_iv, 16, rs43->assoc_data, 112,
                                                                        eh_tag, 16, eh_body, hdr_plain);
                                                            mbedtls_gcm_free(&gcm43);
                                                            ESP_LOGI(TAG, "      [Try 1] HKr + rcAD: %s", hr == 0 ? "✅" : "❌");
                                                            if (hr == 0) { hdr_ok = true; dec_mode = 0; }
                                                            
                                                            // Try 2: NHKr + rcAD (AdvanceRatchet)
                                                            if (!hdr_ok) {
                                                                mbedtls_gcm_init(&gcm43);
                                                                mbedtls_gcm_setkey(&gcm43, MBEDTLS_CIPHER_ID_AES, rs43->next_header_key_recv, 256);
                                                                hr = mbedtls_gcm_auth_decrypt(&gcm43, eh_blen,
                                                                            eh_iv, 16, rs43->assoc_data, 112,
                                                                            eh_tag, 16, eh_body, hdr_plain);
                                                                mbedtls_gcm_free(&gcm43);
                                                                ESP_LOGI(TAG, "      [Try 2] NHKr + rcAD: %s", hr == 0 ? "✅" : "❌");
                                                                if (hr == 0) { hdr_ok = true; dec_mode = 1; }
                                                            }
                                                            
                                                            // Try 3+4: Swapped rcAD
                                                            if (!hdr_ok) {
                                                                uint8_t rcAD_sw[112];
                                                                memcpy(rcAD_sw, rs43->assoc_data + 56, 56);
                                                                memcpy(rcAD_sw + 56, rs43->assoc_data, 56);
                                                                
                                                                mbedtls_gcm_init(&gcm43);
                                                                mbedtls_gcm_setkey(&gcm43, MBEDTLS_CIPHER_ID_AES, rs43->header_key_recv, 256);
                                                                hr = mbedtls_gcm_auth_decrypt(&gcm43, eh_blen,
                                                                            eh_iv, 16, rcAD_sw, 112,
                                                                            eh_tag, 16, eh_body, hdr_plain);
                                                                mbedtls_gcm_free(&gcm43);
                                                                ESP_LOGI(TAG, "      [Try 3] HKr + swapped: %s", hr == 0 ? "✅" : "❌");
                                                                if (hr == 0) { hdr_ok = true; dec_mode = 0; }
                                                                
                                                                if (!hdr_ok) {
                                                                    mbedtls_gcm_init(&gcm43);
                                                                    mbedtls_gcm_setkey(&gcm43, MBEDTLS_CIPHER_ID_AES, rs43->next_header_key_recv, 256);
                                                                    hr = mbedtls_gcm_auth_decrypt(&gcm43, eh_blen,
                                                                                eh_iv, 16, rcAD_sw, 112,
                                                                                eh_tag, 16, eh_body, hdr_plain);
                                                                    mbedtls_gcm_free(&gcm43);
                                                                    ESP_LOGI(TAG, "      [Try 4] NHKr + swapped: %s", hr == 0 ? "✅" : "❌");
                                                                    if (hr == 0) { hdr_ok = true; dec_mode = 1; }
                                                                }
                                                            }
                                                            
                                                            if (!hdr_ok) {
                                                                ESP_LOGE(TAG, "      ❌ All 4 header decrypt attempts FAILED!");
                                                                ESP_LOGI(TAG, "      HKr:  %02x%02x%02x%02x...", rs43->header_key_recv[0], rs43->header_key_recv[1], rs43->header_key_recv[2], rs43->header_key_recv[3]);
                                                                ESP_LOGI(TAG, "      NHKr: %02x%02x%02x%02x...", rs43->next_header_key_recv[0], rs43->next_header_key_recv[1], rs43->next_header_key_recv[2], rs43->next_header_key_recv[3]);
                                                            } else {
                                                                ESP_LOGI(TAG, "      ✅ Header decrypt OK! Mode=%s",
                                                                         dec_mode == 0 ? "SameRatchet" : "AdvanceRatchet");
                                                                
                                                                // Step 4: Parse MsgHeader
                                                                uint16_t mh_ver = (hdr_plain[2] << 8) | hdr_plain[3];
                                                                uint8_t mh_klen = hdr_plain[4];
                                                                ESP_LOGI(TAG, "      MsgHeader: ver=%u, keyLen=%u", mh_ver, mh_klen);
                                                                
                                                                if (mh_klen != 68) {
                                                                    ESP_LOGE(TAG, "      ❌ Unexpected DH key len: %u (expected 68)", mh_klen);
                                                                } else {
                                                                    // Extract raw X448 key (skip 12-byte SPKI header)
                                                                    uint8_t peer_dh43[56];
                                                                    memcpy(peer_dh43, &hdr_plain[17], 56);
                                                                    ESP_LOGI(TAG, "      Peer DH: %02x%02x%02x%02x%02x%02x%02x%02x...",
                                                                             peer_dh43[0], peer_dh43[1], peer_dh43[2], peer_dh43[3],
                                                                             peer_dh43[4], peer_dh43[5], peer_dh43[6], peer_dh43[7]);
                                                                    
                                                                    // Parse PN, Ns (skip KEM for v3)
                                                                    int mhp43 = 5 + mh_klen;
                                                                    if (mh_ver >= 3) {
                                                                        uint8_t kem = hdr_plain[mhp43];
                                                                        if (kem == 0x30) {
                                                                            mhp43 += 1;  // Nothing
                                                                        } else if (kem == 0x31) {
                                                                            mhp43 += 1;
                                                                            uint8_t ks = hdr_plain[mhp43++];
                                                                            if (ks == 0x50) {
                                                                                uint16_t pk_l = (hdr_plain[mhp43] << 8) | hdr_plain[mhp43+1];
                                                                                mhp43 += 2 + pk_l;
                                                                            } else if (ks == 0x41) {
                                                                                uint16_t ct_l = (hdr_plain[mhp43] << 8) | hdr_plain[mhp43+1];
                                                                                mhp43 += 2 + ct_l;
                                                                                uint16_t pk_l = (hdr_plain[mhp43] << 8) | hdr_plain[mhp43+1];
                                                                                mhp43 += 2 + pk_l;
                                                                            }
                                                                        }
                                                                    }
                                                                    
                                                                    uint32_t pn43 = (hdr_plain[mhp43] << 24) | (hdr_plain[mhp43+1] << 16) |
                                                                                    (hdr_plain[mhp43+2] << 8)  | hdr_plain[mhp43+3];
                                                                    mhp43 += 4;
                                                                    uint32_t ns43 = (hdr_plain[mhp43] << 24) | (hdr_plain[mhp43+1] << 16) |
                                                                                    (hdr_plain[mhp43+2] << 8)  | hdr_plain[mhp43+3];
                                                                    
                                                                    ESP_LOGI(TAG, "      PN=%u, Ns=%u", pn43, ns43);
                                                                    
                                                                    // Step 5: Ratchet Body Decrypt
                                                                    uint8_t *body_pt = malloc(rm_body_len + 16);
                                                                    if (body_pt) {
                                                                        size_t body_pt_len = 0;
                                                                        int bret = ratchet_decrypt_body(
                                                                            dec_mode == 0 ? RATCHET_MODE_SAME : RATCHET_MODE_ADVANCE,
                                                                            peer_dh43, pn43, ns43,
                                                                            rm_header, (size_t)rm_hdr_len,
                                                                            rm_auth_tag,
                                                                            rm_body, rm_body_len,
                                                                            body_pt, &body_pt_len);
                                                                        
                                                                        if (bret == 0) {
                                                                            ESP_LOGI(TAG, "");
                                                                            ESP_LOGI(TAG, "      ╔══════════════════════════════════════════════════════╗");
                                                                            ESP_LOGI(TAG, "      ║  🎉 43b: Q_B RATCHET DECRYPT SUCCESS!                ║");
                                                                            ESP_LOGI(TAG, "      ╚══════════════════════════════════════════════════════╝");
                                                                            ESP_LOGI(TAG, "      Plaintext: %zu bytes", body_pt_len);
                                                                            
                                                                            // Hex+ASCII dump first 64 bytes
                                                                            for (int di = 0; di < 64 && di < (int)body_pt_len; di += 16) {
                                                                                char hx43[64] = {0}; char as43[20] = {0}; int hp = 0;
                                                                                for (int dj = 0; dj < 16 && (di+dj) < (int)body_pt_len; dj++) {
                                                                                    uint8_t b = body_pt[di+dj];
                                                                                    hp += sprintf(&hx43[hp], "%02x ", b);
                                                                                    as43[dj] = (b >= 0x20 && b < 0x7f) ? (char)b : '.';
                                                                                }
                                                                                ESP_LOGI(TAG, "        +%04d: %-48s |%s|", di, hx43, as43);
                                                                            }
                                                                            
                                                                            // Step 6: Parse AgentMessage
                                                                            if (body_pt_len >= 11) {
                                                                                uint8_t am_tag = body_pt[0];
                                                                                ESP_LOGI(TAG, "      AgentMsg tag: 0x%02x '%c'", am_tag,
                                                                                         (am_tag >= 0x20 && am_tag < 0x7f) ? (char)am_tag : '?');
                                                                                
                                                                                if (am_tag == 'M') {
                                                                                    // 'M' + sndMsgId(8B) + prevMsgHashLen(1B) + [hash] + innerTag
                                                                                    uint8_t pmh_len = body_pt[9];
                                                                                    int inner_off = 10 + pmh_len;
                                                                                    if (inner_off < (int)body_pt_len) {
                                                                                        uint8_t inner_tag = body_pt[inner_off];
                                                                                        ESP_LOGI(TAG, "      Inner tag: 0x%02x '%c'", inner_tag,
                                                                                                 (inner_tag >= 0x20 && inner_tag < 0x7f) ? (char)inner_tag : '?');
                                                                                        if (inner_tag == 'H') {
                                                                                            ESP_LOGI(TAG, "");
                                                                                            ESP_LOGI(TAG, "      🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉");
                                                                                            ESP_LOGI(TAG, "      🎉  A_HELLO ON REPLY QUEUE!       🎉");
                                                                                            ESP_LOGI(TAG, "      🎉  BIDIRECTIONAL COMMS VERIFIED!  🎉");
                                                                                            ESP_LOGI(TAG, "      🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉🎉");
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        } else {
                                                                            ESP_LOGE(TAG, "      ❌ Ratchet body decrypt FAILED (ret=%d)", bret);
                                                                        }
                                                                        free(body_pt);
                                                                    }
                                                                }
                                                            }
                                                            free(hdr_plain);
                                                        }
                                                    }
                                                    // === Ende Auftrag 43b Ratchet ===
                                                } else {
                                                    ESP_LOGI(TAG, "      Tag '%c' — see agent-protocol.md for meaning", rq_agent_tag);
                                                }
                                                
                                                // Dump first 32 bytes of agent payload
                                                ESP_LOGI(TAG, "      Agent payload (%d bytes):", rq_agent_len);
                                                printf("         ");
                                                for (int i = 0; i < 32 && i < rq_agent_len; i++) printf("%02x ", rq_agent[i]);
                                                printf("\n");
                                                printf("         ASCII: ");
                                                for (int i = 0; i < 32 && i < rq_agent_len; i++) {
                                                    char c = rq_agent[i];
                                                    printf("%c", (c >= 32 && c < 127) ? c : '.');
                                                }
                                                printf("\n");
                                            }
                                        } else if (rq_priv_tag == 'K') {
                                            ESP_LOGI(TAG, "      PHConfirmation — unexpected on Reply Queue!");
                                        } else {
                                            ESP_LOGW(TAG, "      Unknown PrivHeader: 0x%02x", rq_priv_tag);
                                        }
                                        
                                        // Hex+ASCII dump of full decrypted content
                                        ESP_LOGI(TAG, "      📋 Full decrypted (%d bytes):", rq_e2e_plain_len);
                                        for (int di = 0; di < rq_e2e_plain_len && di < 128; di += 16) {
                                            char hex[64] = {0}; char asc[20] = {0}; int hx = 0;
                                            for (int dj = 0; dj < 16 && (di+dj) < rq_e2e_plain_len; dj++) {
                                                uint8_t b = rq_e2e_plain[di+dj];
                                                hx += sprintf(&hex[hx], "%02x ", b);
                                                asc[dj] = (b >= 0x20 && b < 0x7f) ? (char)b : '.';
                                            }
                                            ESP_LOGI(TAG, "        +%04d: %-48s |%s|", di, hex, asc);
                                        }
                                        
                                        free(rq_e2e_plain);
                                        free(rq_server_plain);
                                        break;  // Processed one MSG, done
                                    }
                                    free(rq_block);
                                }
                                
                                
                                // === Auftrag 44a: Send first chat message ===
                                ESP_LOGI(TAG, "");
                                ESP_LOGI(TAG, "      📋 44a: Sending first chat message in 3 seconds...");
                                vTaskDelay(pdMS_TO_TICKS(3000));
                                {
                                    contact_t *msg_contact = &contacts_db.contacts[0];
                                    if (peer_send_chat_message(msg_contact, "Hello from ESP32!")) {
                                        ESP_LOGI(TAG, "      ✅ 44a: Chat message sent!");
                                    } else {
                                        ESP_LOGE(TAG, "      ❌ 44a: Chat message send failed!");
                                    }
                                }
                                
                                skip_42d: ;
                            }
                            // ======== Ende Auftrag 42c+42d ========
                            // ================================================================
                            
                        } else if (priv_tag == '_') {  // PHEmpty
                            ESP_LOGI(TAG, "      PHEmpty — kein Auth Key, AgentMsgEnvelope folgt ab offset 1");
                        } else {
                            ESP_LOGE(TAG, "      UNBEKANNTER PrivHeader: 0x%02x '%c'",
                                     priv_tag, (priv_tag >= 0x20 && priv_tag < 0x7f) ? priv_tag : '?');
                        }
                        // ================================================================
                        // END SESSION 19 PARSING
                        // ================================================================
                        
                    } else {
                        ESP_LOGE(TAG, "      ❌ E2E DECRYPT FAILED (ret=%d)", decrypt_ret);
                        ESP_LOGI(TAG, "      Our e2e_public: %02x%02x%02x%02x...",
                                 our_queue.e2e_public[0], our_queue.e2e_public[1],
                                 our_queue.e2e_public[2], our_queue.e2e_public[3]);
                    }
                    free(e2e_plain);
                }
                
                free(server_plain);
            }
            // ============================================================================
            // END REPLY QUEUE DECRYPTION
            // ============================================================================

            // Contact Queue Decryption (existing code)
            if (contact && contact->have_srv_dh && enc_len > crypto_box_MACBYTES) {
                uint8_t *plain = malloc(enc_len);
                if (plain) {
                    int plain_len = 0;
                    if (decrypt_smp_message(contact, &resp[p], enc_len, msg_id, msgIdLen, plain, &plain_len)) {
                        ESP_LOGI(TAG, "   SMP-Level Decryption OK! (%d bytes)", plain_len);
                        
                        // === CONTACT QUEUE: Extract e2ePubKey for Reply Queue ===
                        // MUST run BEFORE parse_agent_message() which triggers peer handshake!
                        // During handshake the plain buffer might get corrupted.
                        if (contact && plain_len > 60) {
                            // Debug: show bytes around offset 12-15
                            ESP_LOGI(TAG, "   DEBUG Contact Queue [10-17]: %02x %02x %02x %02x %02x %02x %02x %02x",
                                     plain[10], plain[11], plain[12], plain[13], 
                                     plain[14], plain[15], plain[16], plain[17]);
                            
                            // Look for X25519 SPKI header (30 2a 30 05 06 03 2b 65 6e 03 21 00)
                            // Based on raw dump: SPKI starts at offset 14
                            const uint8_t x25519_spki[] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 
                                                           0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00};
                            
                            // Check at offset 14 (confirmed from hex dump)
                            if (memcmp(&plain[14], x25519_spki, 12) == 0) {
                                // Raw key is at offset 14 + 12 = 26
                                memcpy(reply_queue_e2e_peer_public, &plain[26], 32);
                                reply_queue_e2e_peer_valid = true;
                                
                                ESP_LOGI(TAG, "   +-------------------------------------------------------+");
                                ESP_LOGI(TAG, "   |  ✅ E2E KEY EXTRACTED FROM CONTACT QUEUE!            |");
                                ESP_LOGI(TAG, "   +-------------------------------------------------------+");
                                ESP_LOGI(TAG, "      Key: %02x%02x%02x%02x%02x%02x%02x%02x...",
                                        reply_queue_e2e_peer_public[0], reply_queue_e2e_peer_public[1],
                                        reply_queue_e2e_peer_public[2], reply_queue_e2e_peer_public[3],
                                        reply_queue_e2e_peer_public[4], reply_queue_e2e_peer_public[5],
                                        reply_queue_e2e_peer_public[6], reply_queue_e2e_peer_public[7]);
                            } else {
                                ESP_LOGW(TAG, "   SPKI not at offset 14, searching...");
                                // Search for SPKI in first 100 bytes
                                for (int i = 0; i < 100 && i < plain_len - 44; i++) {
                                    if (memcmp(&plain[i], x25519_spki, 12) == 0) {
                                        ESP_LOGI(TAG, "   Found SPKI at offset %d!", i);
                                        memcpy(reply_queue_e2e_peer_public, &plain[i + 12], 32);
                                        reply_queue_e2e_peer_valid = true;
                                        ESP_LOGI(TAG, "   Key: %02x%02x%02x%02x...",
                                                reply_queue_e2e_peer_public[0], reply_queue_e2e_peer_public[1],
                                                reply_queue_e2e_peer_public[2], reply_queue_e2e_peer_public[3]);
                                        break;
                                    }
                                }
                            }
                        }
                        
                        // Parse agent message (this triggers peer connection!)
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