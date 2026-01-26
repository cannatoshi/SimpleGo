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
    ESP_LOGI(TAG, "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—");
    ESP_LOGI(TAG, "â•‘  SimpleGo v0.1.17-alpha Connection!    â•‘");
    ESP_LOGI(TAG, "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");
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
    ESP_LOGI(TAG, "      âœ… TLS OK! ALPN: %s", mbedtls_ssl_get_alpn_protocol(&ssl));

    // ========== Step 2: ServerHello ==========
    ESP_LOGI(TAG, "[2/5] Waiting for ServerHello...");
    int content_len = smp_read_block(&ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "      âŒ No ServerHello");
        goto cleanup;
    }

    uint8_t *hello = block + 2;
    uint16_t minVer = (hello[0] << 8) | hello[1];
    uint16_t maxVer = (hello[2] << 8) | hello[3];
    uint8_t sessIdLen = hello[4];
    
    if (sessIdLen != 32) {
        ESP_LOGE(TAG, "      âŒ Unexpected sessionId length: %d", sessIdLen);
        goto cleanup;
    }
    memcpy(session_id, &hello[5], 32);
    
    ESP_LOGI(TAG, "      âœ… Versions: %d-%d", minVer, maxVer);
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
    ESP_LOGI(TAG, "      âœ… ClientHello sent!");

    // ========== Step 4: Load or Create Contacts ==========
    ESP_LOGI(TAG, "[4/5] Loading contacts...");
    
    // Fresh start for testing - comment out in production!
    ESP_LOGW(TAG, "      ðŸ§¹ Clearing old contacts for fresh test...");
    clear_all_contacts();
    
    load_contacts_from_nvs();
    
    if (contacts_db.num_contacts == 0) {
        ESP_LOGI(TAG, "      No contacts found - creating 'ESP32'...");
        int idx = add_contact(&ssl, block, session_id, "ESP32");
        if (idx < 0) {
            ESP_LOGE(TAG, "      âŒ Failed to create contact!");
            goto cleanup;
        }
    } else {
        ESP_LOGI(TAG, "      âœ… %d contact(s) loaded from NVS", contacts_db.num_contacts);
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
    ESP_LOGI(TAG, "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—");
    ESP_LOGI(TAG, "â•‘   ðŸ“¨ Waiting for messages...       â•‘");
    ESP_LOGI(TAG, "â•‘   (Connect with SimpleX App!)      â•‘");
    ESP_LOGI(TAG, "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");
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
            ESP_LOGI(TAG, "   ðŸ“¬ Message on REPLY QUEUE from peer!");
        }
        
        // Parse command
        if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
            ESP_LOGI(TAG, "   âœ… OK");
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'N' && resp[p+2] == 'D') {
            if (contact) {
                ESP_LOGI(TAG, "   ðŸ”š END [%s] - No more messages", contact->name);
            } else {
                ESP_LOGI(TAG, "   ðŸ”š END - No more messages");
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
                ESP_LOGI(TAG, "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—");
                ESP_LOGI(TAG, "â•‘   ðŸ’¬ MESSAGE RECEIVED for [%s]!", contact->name);
                ESP_LOGI(TAG, "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");
            } else {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "   ðŸ’¬ MESSAGE (unknown contact)!");
            }
            ESP_LOGI(TAG, "   MsgId: %02x%02x%02x%02x...", msg_id[0], msg_id[1], msg_id[2], msg_id[3]);
            ESP_LOGI(TAG, "   Encrypted: %d bytes", enc_len);
            
            // Decrypt
            if (contact && contact->have_srv_dh && enc_len > crypto_box_MACBYTES) {
                uint8_t *plain = malloc(enc_len);
                if (plain) {
                    int plain_len = 0;
                    if (decrypt_smp_message(contact, &resp[p], enc_len, msg_id, msgIdLen, plain, &plain_len)) {
                        ESP_LOGI(TAG, "   ðŸ”“ SMP-Level Decryption OK! (%d bytes)", plain_len);
                        
                        // === DEBUG: HEX-DUMP der entschlÃ¼sselten Agent-Nachricht ===
                        ESP_LOGI(TAG, "");
                        ESP_LOGI(TAG, "   ðŸ“¦ RAW AGENT MESSAGE (first 60 bytes):");
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
                        ESP_LOGI(TAG, "   ðŸ“¨ Sending ACK...");
                        
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
                        ESP_LOGE(TAG, "   âŒ Decryption failed!");
                    }
                    free(plain);
                }
            } else {
                ESP_LOGW(TAG, "      âš ï¸ Cannot decrypt - no contact keys");
            }
            ESP_LOGI(TAG, "");
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
            ESP_LOGE(TAG, "   âŒ ERR: %.*s", 
                     (content_len - p > 20) ? 20 : content_len - p, &resp[p]);
        }
        else {
            ESP_LOGW(TAG, "   â“ Unknown: %c%c%c", 
                     (p < content_len) ? resp[p] : '?',
                     (p+1 < content_len) ? resp[p+1] : '?',
                     (p+2 < content_len) ? resp[p+2] : '?');
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—");
    ESP_LOGI(TAG, "â•‘       Session ended                â•‘");
    ESP_LOGI(TAG, "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");

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
            
            // Kurz warten bis Splash gerendert ist, DANN Backlight an
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
    ESP_LOGI(TAG, "â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");
    ESP_LOGI(TAG, "  STEP 0: Creating our reply queue on %s:%d", SMP_HOST, SMP_PORT);
    ESP_LOGI(TAG, "â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");
    
    if (!queue_create(SMP_HOST, SMP_PORT)) {
        ESP_LOGE(TAG, "âŒ Failed to create reply queue!");
        ESP_LOGW(TAG, "âš ï¸  Continuing without reply queue...");
    } else {
        ESP_LOGI(TAG, "âœ… Reply queue created!");
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
