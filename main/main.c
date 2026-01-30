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
                        
                        // Layer 2: Per-queue DH decrypt
                        // Find X25519 SPKI key in decrypted data
                        int dh_offset = -1;
                        for (int i = 0; i < plain_len - 44; i++) {
                            if (server_plain[i] == 0x30 && server_plain[i+1] == 0x2a &&
                                server_plain[i+2] == 0x30 && server_plain[i+3] == 0x05 &&
                                server_plain[i+4] == 0x06 && server_plain[i+5] == 0x03 &&
                                server_plain[i+6] == 0x2b && server_plain[i+7] == 0x65 &&
                                server_plain[i+8] == 0x6e) {
                                dh_offset = i;
                                break;
                            }
                        }
                        
                        if (dh_offset >= 0) {
                            // TEST: Try different nonce sources
                            ESP_LOGI(TAG, "      🔬 EXTENDED TESTS...");
                            
                            // Get the ciphertext AFTER SPKI (offset 60)
                            int data_offset = dh_offset + 44;  // After SPKI
                            int data_len = plain_len - data_offset;
                            
                            // TEST 1: Use msgId as nonce (like Contact Queue!)
                            uint8_t msg_nonce[24];
                            memset(msg_nonce, 0, 24);
                            memcpy(msg_nonce, msg_id, msgIdLen);
                            
                            // Compute DH with peer's ephemeral key
                            uint8_t peer_pub[32];
                            memcpy(peer_pub, &server_plain[dh_offset + 12], 32);
                            uint8_t test_dh[32];
                            crypto_box_beforenm(test_dh, peer_pub, our_queue.rcv_dh_private);
                            
                            uint8_t *test_plain = malloc(data_len);
                            if (test_plain && data_len > 16) {
                                ESP_LOGI(TAG, "      TEST1: peer_dh + msgId nonce");
                                if (crypto_box_open_easy_afternm(test_plain, &server_plain[data_offset], data_len,
                                                                  msg_nonce, test_dh) == 0) {
                                    ESP_LOGI(TAG, "      ✅ TEST1 SUCCESS!");
                                    ESP_LOGI(TAG, "      First 16: %02x %02x %02x %02x %02x %02x %02x %02x",
                                             test_plain[0], test_plain[1], test_plain[2], test_plain[3],
                                             test_plain[4], test_plain[5], test_plain[6], test_plain[7]);
                                } else {
                                    ESP_LOGE(TAG, "      ❌ TEST1 FAILED");
                                }
                                
                                // TEST 2: Use srv_dh_public instead of peer ephemeral
                                ESP_LOGI(TAG, "      TEST2: srv_dh_public + msgId nonce");
                                uint8_t srv_dh[32];
                                crypto_box_beforenm(srv_dh, our_queue.srv_dh_public, our_queue.rcv_dh_private);
                                if (crypto_box_open_easy_afternm(test_plain, &server_plain[data_offset], data_len,
                                                                  msg_nonce, srv_dh) == 0) {
                                    ESP_LOGI(TAG, "      ✅ TEST2 SUCCESS!");
                                    ESP_LOGI(TAG, "      First 16: %02x %02x %02x %02x %02x %02x %02x %02x",
                                             test_plain[0], test_plain[1], test_plain[2], test_plain[3],
                                             test_plain[4], test_plain[5], test_plain[6], test_plain[7]);
                                } else {
                                    ESP_LOGE(TAG, "      ❌ TEST2 FAILED");
                                }
                                
                                // TEST 3: Skip server-level, try DIRECT on raw data
                                ESP_LOGI(TAG, "      TEST3: Direct on raw (no server decrypt first)");
                                // Find SPKI in RAW resp[p] data
                                int raw_spki = -1;
                                for (int i = 0; i < enc_len - 44; i++) {
                                    if (resp[p+i] == 0x30 && resp[p+i+1] == 0x2a) {
                                        raw_spki = i;
                                        break;
                                    }
                                }
                                if (raw_spki >= 0) {
                                    ESP_LOGI(TAG, "         Raw SPKI at offset %d", raw_spki);
                                    uint8_t raw_peer[32];
                                    memcpy(raw_peer, &resp[p + raw_spki + 12], 32);
                                    uint8_t raw_dh[32];
                                    crypto_box_beforenm(raw_dh, raw_peer, our_queue.rcv_dh_private);
                                    int raw_data_off = raw_spki + 44;
                                    int raw_data_len = enc_len - raw_data_off;
                                    if (crypto_box_open_easy_afternm(test_plain, &resp[p + raw_data_off], raw_data_len,
                                                                      msg_nonce, raw_dh) == 0) {
                                        ESP_LOGI(TAG, "      ✅ TEST3 SUCCESS!");
                                    } else {
                                        ESP_LOGE(TAG, "      ❌ TEST3 FAILED");
                                    }
                                } else {
                                    ESP_LOGI(TAG, "         No SPKI in raw data");
                                }
                                
                                free(test_plain);
                            }

                            // ========== TEST 4: CORRECT - Use cmNonce from ClientMsgEnvelope ==========
                            ESP_LOGI(TAG, "      TEST4: CORRECT - peer_e2e + cmNonce from envelope");

                            int version_offset = 12;
                            int maybe_tag_offset = 14;
                            int spki_offset = 16;
                            int cm_nonce_offset = spki_offset + 44;        // [60-83]
                            int cm_enc_body_offset = cm_nonce_offset + 24; // [84+]

                            ESP_LOGI(TAG, "         Structure offsets: version=%d, maybe=%d, spki=%d, cmNonce=%d, cmEncBody=%d",
                                     version_offset, maybe_tag_offset, spki_offset, cm_nonce_offset, cm_enc_body_offset);

                            uint8_t maybe_e2e = server_plain[maybe_tag_offset + 1];
                            ESP_LOGI(TAG, "         ✓ Maybe tag = '%c' (%s)", maybe_e2e, 
                                     maybe_e2e == '1' ? "Just - has e2ePubKey" : "Nothing");

                            if (maybe_e2e == '1' && plain_len > cm_enc_body_offset + 16) {
                                uint8_t cm_nonce[24];
                                memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);
                                ESP_LOGI(TAG, "         cmNonce: %02x %02x %02x %02x %02x %02x %02x %02x...",
                                         cm_nonce[0], cm_nonce[1], cm_nonce[2], cm_nonce[3],
                                         cm_nonce[4], cm_nonce[5], cm_nonce[6], cm_nonce[7]);

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
                                        ESP_LOGI(TAG, "      ✅ TEST4 SUCCESS! Per-queue E2E decrypt worked!");
                                        ESP_LOGI(TAG, "         Decrypted %d bytes (ClientMessage)", cm_plain_len);
                                        printf("            ");
                                        for (int i = 0; i < 32 && i < cm_plain_len; i++) printf("%02x ", cm_plain[i]);
                                        printf("\n");

                                        if (cm_plain_len > 4) {
                                            char priv_tag = (char)cm_plain[2];
                                            ESP_LOGI(TAG, "         PrivHeader tag: '%c' (0x%02x)", priv_tag, (uint8_t)priv_tag);
                                            if (priv_tag == 'K') {
                                                ESP_LOGI(TAG, "      🎉 Received PEER'S AgentConfirmation!");
                                            }
                                        }
                                    } else {
                                        ESP_LOGE(TAG, "      ❌ TEST4 FAILED");
                                    }
                                    free(cm_plain);
                                }
                            }

                            // END EXTENDED TESTS
                            
                            // Extract peer's ephemeral X25519 public key (skip 12-byte SPKI header)
                            uint8_t peer_dh_pub[32];
                            memcpy(peer_dh_pub, &server_plain[dh_offset + 12], 32);
                            
                            ESP_LOGI(TAG, "      DEBUG: peer_dh_pub: %02x%02x%02x%02x...",
                                     peer_dh_pub[0], peer_dh_pub[1], peer_dh_pub[2], peer_dh_pub[3]);
                            ESP_LOGI(TAG, "      DEBUG: our rcv_dh_public: %02x%02x%02x%02x...",
                                     our_queue.rcv_dh_public[0], our_queue.rcv_dh_public[1],
                                     our_queue.rcv_dh_public[2], our_queue.rcv_dh_public[3]);
                            
                            // Data after SPKI key = nonce + ciphertext + MAC
                            int after_key_offset = dh_offset + 44;
                            int after_key_len = plain_len - after_key_offset;
                            
                            if (after_key_len > 40) {
                                // Compute shared secret with our reply queue DH private key
                                uint8_t dh_shared[32];
                                if (crypto_box_beforenm(dh_shared, peer_dh_pub, our_queue.rcv_dh_private) == 0) {
                                    ESP_LOGI(TAG, "      DEBUG: dh_shared: %02x%02x%02x%02x...",
                                             dh_shared[0], dh_shared[1], dh_shared[2], dh_shared[3]);
                                    
                                    // Nonce is first 24 bytes after SPKI
                                    uint8_t *dh_nonce = &server_plain[after_key_offset];
                                    uint8_t *dh_ciphertext = &server_plain[after_key_offset + 24];
                                    int dh_ct_len = after_key_len - 24;
                                    
                                    ESP_LOGI(TAG, "      DEBUG: nonce: %02x%02x%02x%02x...",
                                             dh_nonce[0], dh_nonce[1], dh_nonce[2], dh_nonce[3]);
                                    ESP_LOGI(TAG, "      DEBUG: dh_ct_len: %d", dh_ct_len);
                                    
                                    uint8_t *dh_plain = malloc(dh_ct_len);
                                    if (dh_plain) {
                                        if (crypto_box_open_easy_afternm(dh_plain, dh_ciphertext, dh_ct_len,
                                                                          dh_nonce, dh_shared) == 0) {
                                            int dh_plain_len = dh_ct_len - crypto_box_MACBYTES;
                                            ESP_LOGI(TAG, "      ✅ Per-queue DH decrypt SUCCESS! (%d bytes)", dh_plain_len);
                                            ESP_LOGI(TAG, "      First 32 bytes:");
                                            printf("         ");
                                            for (int i = 0; i < 32 && i < dh_plain_len; i++) printf("%02x ", dh_plain[i]);
                                            printf("\n");
                                            
                                            // Now parse AgentMsgEnvelope and Double Ratchet
                                            if (dh_plain_len > 5 && ratchet_is_initialized()) {
                                                int ap = 2;  // Skip length prefix
                                                uint16_t agent_ver = (dh_plain[ap] << 8) | dh_plain[ap + 1];
                                                ap += 2;
                                                char msg_type = (char)dh_plain[ap++];
                                                
                                                ESP_LOGI(TAG, "      📦 AgentMsgEnvelope: version=%d, type='%c' (0x%02x)", 
                                                         agent_ver, msg_type >= 0x20 ? msg_type : '?', (uint8_t)msg_type);
                                                
                                                if (msg_type == 'M') {
                                                    ESP_LOGI(TAG, "      🔐 Double Ratchet decrypt...");
                                                    uint8_t *dr_plain = malloc(dh_plain_len);
                                                    if (dr_plain) {
                                                        size_t dr_len = 0;
                                                        if (ratchet_decrypt_incoming(&dh_plain[ap], dh_plain_len - ap, dr_plain, &dr_len) == 0) {
                                                            ESP_LOGI(TAG, "      ✅ Double Ratchet SUCCESS! (%zu bytes)", dr_len);
                                                            printf("         ");
                                                            for (size_t i = 0; i < 32 && i < dr_len; i++) printf("%02x ", dr_plain[i]);
                                                            printf("\n");
                                                        } else {
                                                            ESP_LOGE(TAG, "      ❌ Double Ratchet decrypt FAILED");
                                                        }
                                                        free(dr_plain);
                                                    }
                                                }
                                            }
                                        } else {
                                            ESP_LOGE(TAG, "      ❌ Per-queue DH decrypt FAILED!");
                                        }
                                        free(dh_plain);
                                    }
                                }
                            }
                        } else {
                            ESP_LOGE(TAG, "      ❌ No X25519 SPKI found in reply!");
                        }
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
            } else if (ratchet_is_initialized()) {
                // Try Double Ratchet decrypt for incoming messages
                ESP_LOGI(TAG, "   🔐 Attempting Double Ratchet decrypt...");
                uint8_t *plain = malloc(enc_len + 100);
                if (plain) {
                    size_t plain_len = 0;
                    if (ratchet_decrypt_incoming(&resp[p], enc_len, plain, &plain_len) == 0) {
                        ESP_LOGI(TAG, "   ✅ Double Ratchet decrypt SUCCESS! (%zu bytes)", plain_len);
                        ESP_LOGI(TAG, "   First 16 bytes:");
                        printf("      ");
                        for (size_t i = 0; i < 16 && i < plain_len; i++) {
                            printf("%02x ", plain[i]);
                        }
                        printf("\n");
                        // Parse the decrypted agent message
                        // TODO: parse_agent_message(NULL, plain, plain_len);
                    } else {
                        ESP_LOGE(TAG, "   ❌ Double Ratchet decrypt FAILED");
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