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
#include "simplex_crypto.h"  // SimpleX custom XSalsa20-Poly1305

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
                // FINAL CORRECT Wire Format:
                // 
                // [14]    = maybe_corrId tag ('1' = Just)
                // [15]    = maybe_e2e tag (',' = 0x2C = Nothing = corrId IS the E2E key!)
                // [16-59] = SPKI (44 bytes) = BOTH corrId AND E2E key!
                // [60-83] = cmNonce (24 bytes)
                // [84+]   = cmEncBody
                //
                // CRITICAL: When maybe_e2e = ',' (0x2C), the corrId SPKI doubles as E2E key!
                // The 0x2C byte is NOT a length - it's the "Nothing" tag for maybe_e2e!
                // ================================================================
                
                int offset = 14;
                
                // [14] = maybe_corrId
                uint8_t maybe_corrId = server_plain[offset];
                ESP_LOGI(TAG, "      [%d] maybe_corrId = '%c' (0x%02x)", offset, maybe_corrId, maybe_corrId);
                offset++;  // Now at 15
                
                // [15] = maybe_e2e (NOT corrId_len!)
                uint8_t maybe_e2e = server_plain[offset];
                ESP_LOGI(TAG, "      [%d] maybe_e2e = '%c' (0x%02x)", offset, 
                         (maybe_e2e >= 0x20 && maybe_e2e < 0x7f) ? maybe_e2e : '?', maybe_e2e);
                offset++;  // Now at 16
                
                const uint8_t x25519_spki_header[] = {0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 
                                                       0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00};
                
                uint8_t sender_pub[32];
                bool have_sender_key = false;
                
                if (maybe_corrId == '1' && maybe_e2e == ',') {
                    // maybe_e2e = ',' (0x2C) means: corrId SPKI IS the E2E key!
                    ESP_LOGI(TAG, "      maybe_e2e = ',' -> corrId SPKI doubles as E2E key!");
                    
                    // Verify SPKI header at offset 16
                    if (memcmp(&server_plain[offset], x25519_spki_header, 12) == 0) {
                        // Extract raw key (after 12-byte SPKI header)
                        memcpy(sender_pub, &server_plain[offset + 12], 32);
                        have_sender_key = true;
                        
                        ESP_LOGI(TAG, "      ✅ Found E2E Key (from corrId SPKI) at offset %d!", offset);
                        ESP_LOGI(TAG, "      sender_pub: %02x%02x%02x%02x%02x%02x%02x%02x...",
                                 sender_pub[0], sender_pub[1], sender_pub[2], sender_pub[3],
                                 sender_pub[4], sender_pub[5], sender_pub[6], sender_pub[7]);
                        
                        // Save for future messages
                        memcpy(reply_queue_e2e_peer_public, sender_pub, 32);
                        reply_queue_e2e_peer_valid = true;
                    } else {
                        ESP_LOGE(TAG, "      ❌ SPKI header mismatch at offset %d", offset);
                    }
                    offset += 44;  // Skip past SPKI, now at cmNonce (offset 60)
                    
                } else if (maybe_corrId == '1' && maybe_e2e == '1') {
                    // maybe_e2e = '1' means: separate E2E key follows after corrId
                    ESP_LOGI(TAG, "      maybe_e2e = '1' -> separate E2E key after corrId");
                    
                    // Skip corrId SPKI first
                    uint8_t corrId_len = 44;  // Standard SPKI length
                    offset += corrId_len;  // Now past corrId
                    
                    // Now read e2e key length and key
                    uint8_t e2e_len = server_plain[offset];
                    offset++;
                    
                    if (e2e_len == 44 && memcmp(&server_plain[offset], x25519_spki_header, 12) == 0) {
                        memcpy(sender_pub, &server_plain[offset + 12], 32);
                        have_sender_key = true;
                        ESP_LOGI(TAG, "      ✅ Found separate E2E Key at offset %d!", offset);
                    }
                    offset += 44;  // Skip E2E SPKI
                    
                } else if (maybe_corrId == ',') {
                    // No corrId - use pre-shared key
                    ESP_LOGI(TAG, "      maybe_corrId = ',' -> using PRE-SHARED key!");
                    
                    if (reply_queue_e2e_peer_valid) {
                        memcpy(sender_pub, reply_queue_e2e_peer_public, 32);
                        have_sender_key = true;
                        ESP_LOGI(TAG, "      ✅ Using pre-shared key: %02x%02x%02x%02x...",
                                 sender_pub[0], sender_pub[1], sender_pub[2], sender_pub[3]);
                    } else {
                        ESP_LOGE(TAG, "      ❌ No pre-shared E2E key available!");
                    }
                } else {
                    ESP_LOGE(TAG, "      ❌ Unknown format: maybe_corrId=0x%02x, maybe_e2e=0x%02x", 
                             maybe_corrId, maybe_e2e);
                }
                
                if (!have_sender_key) {
                    ESP_LOGE(TAG, "      ❌ No sender key available!");
                    free(server_plain);
                    continue;
                }
                
                // Now offset points to cmNonce (24 bytes)
                if (plain_len < (size_t)offset + 24 + 16) {
                    ESP_LOGE(TAG, "      Message too short for E2E decrypt");
                    free(server_plain);
                    continue;
                }
                
                uint8_t cm_nonce[24];
                memcpy(cm_nonce, &server_plain[offset], 24);
                ESP_LOGI(TAG, "      cmNonce (at offset %d): %02x%02x%02x%02x...",
                         offset, cm_nonce[0], cm_nonce[1], cm_nonce[2], cm_nonce[3]);
                offset += 24;
                
                const uint8_t *e2e_encrypted = &server_plain[offset];
                size_t e2e_encrypted_len = plain_len - offset;
                
                ESP_LOGI(TAG, "      E2E encrypted at offset %d, len: %zu", offset, e2e_encrypted_len);
                
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
                    ESP_LOGW(TAG, "");
                    
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
                    int nonce_start = offset - 24;  // Back to where nonce starts
                    int enc_block_len = plain_len - nonce_start;
                    ESP_LOGI(TAG, "      enc_block starts at %d, len=%d", nonce_start, enc_block_len);
                    
                    uint8_t *method0_plain = malloc(enc_block_len);
                    if (method0_plain) {
                        int dec_len = decrypt_client_msg(&server_plain[nonce_start], enc_block_len,
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
                        ESP_LOGI(TAG, "      First 64 bytes:");
                        printf("         ");
                        for (int i = 0; i < 64 && i < e2e_plain_len; i++) {
                            printf("%02x ", e2e_plain[i]);
                            if ((i + 1) % 16 == 0) printf("\n         ");
                        }
                        printf("\n");
                        
                        // Parse PrivHeader
                        char priv_header = e2e_plain[0];
                        ESP_LOGI(TAG, "      PrivHeader: '%c' (0x%02x)", priv_header, priv_header);
                        
                        if (priv_header == '_') {
                            ESP_LOGI(TAG, "      PHEmpty - AgentMsgEnvelope follows at offset 1");
                        } else if (priv_header == 'K') {
                            ESP_LOGI(TAG, "      PHConfirmation - sender auth key present");
                        }
                        
                        // Check for EncRatchetMessage (0x7b prefix)
                        int msg_offset = 1;
                        if (e2e_plain[msg_offset] == 0x7b) {
                            ESP_LOGI(TAG, "      Found EncRatchetMessage at offset %d!", msg_offset);
                            ESP_LOGI(TAG, "      (Double Ratchet decrypt would go here)");
                        }
                        
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