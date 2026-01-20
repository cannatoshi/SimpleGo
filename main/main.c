/**
 * SimpleGo - Native SimpleX SMP Client for ESP32
 * v4.2 - Full queue lifecycle: NEW, SUB, SEND, MSG receive
 * 
 * Features:
 *   - TLS 1.3 with ALPN "smp/1"
 *   - Ed25519 signing (libsodium)
 *   - X25519 DH key exchange
 *   - SMP v6 protocol
 */

#include <string.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/sha256.h"

#include "sodium.h"  // libsodium for Ed25519

static const char *TAG = "SMP";

// ============== CONFIG ==============
#define WIFI_SSID     "SONCLOUD"
#define WIFI_PASS     "MKwlan1d250e-mured5"
#define SMP_HOST      "smp3.simplexonflux.com"
#define SMP_PORT      5223
#define SMP_BLOCK_SIZE 16384

// SPKI headers for key encoding
static const uint8_t ED25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00
};
static const uint8_t X25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00
};
#define SPKI_KEY_SIZE 44  // 12 header + 32 key

static volatile bool wifi_connected = false;

static const int ciphersuites[] = {
    MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
    0
};

// ============== TCP Helpers ==============

static int tcp_connect(const char *host, int port) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS failed: %d", err);
        return -1;
    }
    
    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket failed");
        freeaddrinfo(res);
        return -1;
    }
    
    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "Connect failed");
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    
    freeaddrinfo(res);
    return sock;
}

// ============== mbedTLS I/O ==============

static int my_send_cb(void *ctx, const unsigned char *buf, size_t len) {
    int sock = *(int *)ctx;
    int ret = send(sock, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_WRITE;
        return -1;
    }
    return ret;
}

static int my_recv_cb(void *ctx, unsigned char *buf, size_t len) {
    int sock = *(int *)ctx;
    int ret = recv(sock, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return MBEDTLS_ERR_SSL_WANT_READ;
        return -1;
    }
    return ret;
}

// ============== SMP Block I/O ==============

static TickType_t get_tick_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static int read_exact(mbedtls_ssl_context *ssl, uint8_t *buf, size_t len, int timeout_ms) {
    size_t received = 0;
    TickType_t start = get_tick_ms();
    
    while (received < len) {
        int ret = mbedtls_ssl_read(ssl, buf + received, len - received);
        
        if (ret > 0) {
            received += ret;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) {
            ESP_LOGW(TAG, "Connection closed");
            return -1;
        } else {
            ESP_LOGE(TAG, "Read error: -0x%04X", -ret);
            return -1;
        }
        
        if ((get_tick_ms() - start) > (TickType_t)timeout_ms) {
            if (received > 0) return received;
            return -2;
        }
    }
    return received;
}

static int smp_read_block(mbedtls_ssl_context *ssl, uint8_t *block, int timeout_ms) {
    int ret = read_exact(ssl, block, SMP_BLOCK_SIZE, timeout_ms);
    if (ret < 0) return ret;
    
    uint16_t content_len = (block[0] << 8) | block[1];
    if (content_len > SMP_BLOCK_SIZE - 2) {
        ESP_LOGE(TAG, "Invalid content length: %d", content_len);
        return -3;
    }
    return content_len;
}

// Write a HANDSHAKE block (simple format)
static int smp_write_handshake_block(mbedtls_ssl_context *ssl, uint8_t *block, 
                                      const uint8_t *content, size_t content_len) {
    if (content_len > SMP_BLOCK_SIZE - 2) return -1;
    
    memset(block, '#', SMP_BLOCK_SIZE);
    block[0] = (content_len >> 8) & 0xFF;
    block[1] = content_len & 0xFF;
    memcpy(block + 2, content, content_len);
    
    int written = 0;
    while (written < SMP_BLOCK_SIZE) {
        int ret = mbedtls_ssl_write(ssl, block + written, SMP_BLOCK_SIZE - written);
        if (ret > 0) {
            written += ret;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGE(TAG, "Write error: -0x%04X", -ret);
            return ret;
        }
    }
    return 0;
}

// Write a COMMAND block (transport format)
static int smp_write_command_block(mbedtls_ssl_context *ssl, uint8_t *block,
                                    const uint8_t *transmission, size_t trans_len) {
    memset(block, '#', SMP_BLOCK_SIZE);
    
    int pos = 0;
    
    // originalLength = 1 (txCount) + 2 (txLen) + trans_len
    uint16_t orig_len = 1 + 2 + trans_len;
    block[pos++] = (orig_len >> 8) & 0xFF;
    block[pos++] = orig_len & 0xFF;
    
    // transmissionCount = 1
    block[pos++] = 1;
    
    // transmissionLength
    block[pos++] = (trans_len >> 8) & 0xFF;
    block[pos++] = trans_len & 0xFF;
    
    // transmission data
    memcpy(&block[pos], transmission, trans_len);
    
    // No extra logging needed
    
    int written = 0;
    while (written < SMP_BLOCK_SIZE) {
        int ret = mbedtls_ssl_write(ssl, block + written, SMP_BLOCK_SIZE - written);
        if (ret > 0) {
            written += ret;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGE(TAG, "Write error: -0x%04X", -ret);
            return ret;
        }
    }
    return 0;
}

// ============== Certificate Parsing ==============

static int parse_cert_chain(const uint8_t *data, int len, 
                           int *cert1_off, int *cert1_len,
                           int *cert2_off, int *cert2_len) {
    *cert1_off = -1;
    *cert2_off = -1;
    
    for (int i = 0; i < len - 4; i++) {
        if (data[i] == 0x30 && data[i+1] == 0x82) {
            *cert1_off = i;
            *cert1_len = ((data[i+2] << 8) | data[i+3]) + 4;
            break;
        }
    }
    
    if (*cert1_off < 0) return -1;
    
    int search_start = *cert1_off + *cert1_len;
    for (int i = search_start; i < len - 4; i++) {
        if (data[i] == 0x30 && data[i+1] == 0x82) {
            *cert2_off = i;
            *cert2_len = ((data[i+2] << 8) | data[i+3]) + 4;
            break;
        }
    }
    
    return 0;
}

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
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS },
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
    
    // Keys for NEW command
    uint8_t rcv_auth_secret[crypto_sign_SECRETKEYBYTES], rcv_auth_public[crypto_sign_PUBLICKEYBYTES];
    uint8_t rcv_dh_secret[32], rcv_dh_public[32];
    
    // Queue IDs from IDS response
    uint8_t recipient_id[24];
    uint8_t recipient_id_len = 0;
    uint8_t sender_id[24];
    uint8_t sender_id_len = 0;
    uint8_t srv_dh_public[32];  // Server's DH public key for encryption
    bool have_srv_dh = false;

    uint8_t *block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "Failed to allocate buffer!");
        return;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SimpleGo v4.2 - NEW + SUB + SEND!");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) goto cleanup;

    // ========== Step 1: TCP + TLS ==========
    ESP_LOGI(TAG, "[1/6] TCP + TLS...");
    sock = tcp_connect(SMP_HOST, SMP_PORT);
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
            ESP_LOGE(TAG, "TLS failed: -0x%04X", -ret);
            goto cleanup;
        }
    }
    ESP_LOGI(TAG, "      TLS OK! ALPN: %s", mbedtls_ssl_get_alpn_protocol(&ssl));

    // ========== Step 2: ServerHello ==========
    ESP_LOGI(TAG, "[2/6] Waiting for ServerHello...");
    int content_len = smp_read_block(&ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "No ServerHello");
        goto cleanup;
    }

    uint8_t *hello = block + 2;
    uint16_t minVer = (hello[0] << 8) | hello[1];
    uint16_t maxVer = (hello[2] << 8) | hello[3];
    uint8_t sessIdLen = hello[4];
    
    if (sessIdLen != 32) {
        ESP_LOGE(TAG, "Unexpected sessionId length: %d", sessIdLen);
        goto cleanup;
    }
    memcpy(session_id, &hello[5], 32);
    
    ESP_LOGI(TAG, "      Versions: %d-%d, SessionId: %02x%02x%02x%02x...", 
             minVer, maxVer, session_id[0], session_id[1], session_id[2], session_id[3]);

    // ========== Step 3: ClientHello ==========
    ESP_LOGI(TAG, "[3/6] Sending ClientHello...");
    
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);
    
    uint8_t ca_hash[32];
    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
    }
    
    // ClientHello v6
    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;  // v6
    client_hello[pos++] = 32;    // keyHash length
    memcpy(&client_hello[pos], ca_hash, 32);
    pos += 32;
    
    ret = smp_write_handshake_block(&ssl, block, client_hello, pos);
    if (ret != 0) goto cleanup;
    ESP_LOGI(TAG, "      ClientHello sent!");

    // ========== Step 4: Generate Keys ==========
    ESP_LOGI(TAG, "[4/6] Generating keypairs...");
    
    // Ed25519 keypair for rcvAuthKey (used for signing!)
    // libsodium: sk is 64 bytes (seed + pubkey), pk is 32 bytes
    uint8_t seed[32];
    esp_fill_random(seed, 32);
    crypto_sign_seed_keypair(rcv_auth_public, rcv_auth_secret, seed);
    
    // X25519 keypair for rcvDhKey
    esp_fill_random(rcv_dh_secret, 32);
    crypto_scalarmult_base(rcv_dh_public, rcv_dh_secret);
    
    ESP_LOGI(TAG, "      rcvAuthKey (Ed25519): %02x%02x%02x%02x...", 
             rcv_auth_public[0], rcv_auth_public[1], rcv_auth_public[2], rcv_auth_public[3]);
    ESP_LOGI(TAG, "      rcvDhKey (X25519):    %02x%02x%02x%02x...",
             rcv_dh_public[0], rcv_dh_public[1], rcv_dh_public[2], rcv_dh_public[3]);

    // ========== Step 5: Send NEW Command ==========
    ESP_LOGI(TAG, "[5/6] Sending NEW command...");
    
    // Build SPKI-encoded keys
    uint8_t rcv_auth_spki[SPKI_KEY_SIZE];
    memcpy(rcv_auth_spki, ED25519_SPKI_HEADER, 12);
    memcpy(rcv_auth_spki + 12, rcv_auth_public, 32);
    
    uint8_t rcv_dh_spki[SPKI_KEY_SIZE];
    memcpy(rcv_dh_spki, X25519_SPKI_HEADER, 12);
    memcpy(rcv_dh_spki + 12, rcv_dh_public, 32);
    
    /*
     * Build transmission body (what we sign):
     *   [corrIdLen][corrId][entityIdLen][command]
     * 
     * Command for NEW:
     *   "NEW " [keyLen][rcvAuthKey SPKI] [keyLen][rcvDhKey SPKI]
     */
    uint8_t trans_body[256];
    pos = 0;
    
    // CorrId = "1"
    trans_body[pos++] = 1;    // corrId length
    trans_body[pos++] = '1';  // corrId value
    
    // EntityId = empty (for NEW command)
    trans_body[pos++] = 0;    // entityId length = 0
    
    // Command: "NEW " + keys
    trans_body[pos++] = 'N';
    trans_body[pos++] = 'E';
    trans_body[pos++] = 'W';
    trans_body[pos++] = ' ';
    
    // rcvAuthKey (SPKI with length prefix)
    trans_body[pos++] = SPKI_KEY_SIZE;  // 44
    memcpy(&trans_body[pos], rcv_auth_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;
    
    // rcvDhKey (SPKI with length prefix)
    trans_body[pos++] = SPKI_KEY_SIZE;  // 44
    memcpy(&trans_body[pos], rcv_dh_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;
    
    // subMode = 'S' (SMSubscribe) - REQUIRED for v6!
    trans_body[pos++] = 'S';
    
    int trans_body_len = pos;
    ESP_LOGI(TAG, "      Command: NEW (%d bytes)", trans_body_len);
    
    /*
     * Signature is over: smpEncode(sessionId) <> transmission_body
     * smpEncode adds length prefix, so: [0x20][sessionId] + body
     */
    uint8_t to_sign[1 + 32 + 256];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;  // Length prefix for sessionId!
    memcpy(&to_sign[sign_pos], session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
    sign_pos += trans_body_len;
    int to_sign_len = sign_pos;
    
    ESP_LOGI(TAG, "      Signing %d bytes...", to_sign_len);
    
    // Sign with Ed25519 using rcvAuthKey (libsodium)
    uint8_t signature[crypto_sign_BYTES];  // 64 bytes
    crypto_sign_detached(signature, NULL, to_sign, to_sign_len, rcv_auth_secret);
    
    // LOCAL VERIFICATION - does our signature verify with our public key?
    int verify_result = crypto_sign_verify_detached(signature, to_sign, to_sign_len, rcv_auth_public);
    if (verify_result == 0) {
        ESP_LOGI(TAG, "      ✅ Signature OK");
    } else {
        ESP_LOGE(TAG, "      ❌ Signature verification FAILED!");
        goto cleanup;
    }
    
    /*
     * Final transmission format:
     *   [sigLen][signature 64 bytes]
     *   [sessLen][sessionId 32 bytes]
     *   [transmission_body]
     */
    uint8_t transmission[256];
    int tpos = 0;
    
    // Signature with length prefix
    transmission[tpos++] = crypto_sign_BYTES;  // 64
    memcpy(&transmission[tpos], signature, crypto_sign_BYTES);
    tpos += crypto_sign_BYTES;
    
    // SessionId with length prefix
    transmission[tpos++] = 32;  // sessionId length
    memcpy(&transmission[tpos], session_id, 32);
    tpos += 32;
    
    // Transmission body (corrId + entityId + command)
    memcpy(&transmission[tpos], trans_body, trans_body_len);
    tpos += trans_body_len;
    
    ESP_LOGI(TAG, "      Transmission: %d bytes", tpos);
    
    // Send using transport block format
    ret = smp_write_command_block(&ssl, block, transmission, tpos);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to send NEW command");
        goto cleanup;
    }
    ESP_LOGI(TAG, "      NEW command sent!");

    // ========== Step 6: Wait for Response ==========
    ESP_LOGI(TAG, "[6/6] Waiting for response...");
    
    content_len = smp_read_block(&ssl, block, 15000);
    if (content_len >= 0) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "  Response: %d bytes", content_len);
        
        uint8_t *resp = block + 2;
        
        // Check for IDS (success) or ERR
        for (int i = 0; i < content_len - 3; i++) {
            if (resp[i] == 'I' && resp[i+1] == 'D' && resp[i+2] == 'S' && resp[i+3] == ' ') {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "  🎉🎉🎉 QUEUE CREATED! 🎉🎉🎉");
                ESP_LOGI(TAG, "");
                
                // Parse IDS response: "IDS " [len][rcvId] [len][sndId] [len][srvDhKey]
                int p = i + 4;  // Skip "IDS "
                
                // rcvId (RecipientId)
                if (p < content_len) {
                    uint8_t rcv_id_len = resp[p++];
                    if (p + rcv_id_len <= content_len && rcv_id_len <= 24) {
                        ESP_LOGI(TAG, "  📥 RecipientId (%d bytes):", rcv_id_len);
                        printf("     ");
                        for (int j = 0; j < rcv_id_len; j++) {
                            printf("%02x", resp[p + j]);
                        }
                        printf("\n");
                        
                        // Store recipientId for SUB command
                        memcpy(recipient_id, &resp[p], rcv_id_len);
                        recipient_id_len = rcv_id_len;
                        
                        p += rcv_id_len;
                    }
                }
                
                // sndId (SenderId)
                if (p < content_len) {
                    sender_id_len = resp[p++];
                    if (p + sender_id_len <= content_len && sender_id_len <= 24) {
                        ESP_LOGI(TAG, "  📤 SenderId (%d bytes):", sender_id_len);
                        printf("     ");
                        for (int j = 0; j < sender_id_len; j++) {
                            printf("%02x", resp[p + j]);
                            sender_id[j] = resp[p + j];
                        }
                        printf("\n");
                        p += sender_id_len;
                    }
                }
                
                // srvDhKey (Server DH Public Key - SPKI format)
                if (p < content_len) {
                    uint8_t srv_dh_len = resp[p++];
                    if (p + srv_dh_len <= content_len) {
                        ESP_LOGI(TAG, "  🔑 ServerDhKey (%d bytes):", srv_dh_len);
                        printf("     ");
                        for (int j = 0; j < srv_dh_len && j < 20; j++) {
                            printf("%02x", resp[p + j]);
                        }
                        if (srv_dh_len > 20) printf("...");
                        printf("\n");
                        
                        // Extract raw key from SPKI (skip 12-byte header)
                        if (srv_dh_len == 44) {
                            ESP_LOGI(TAG, "  🔑 ServerDhKey (raw 32 bytes):");
                            printf("     ");
                            for (int j = 12; j < srv_dh_len; j++) {
                                printf("%02x", resp[p + j]);
                            }
                            printf("\n");
                            
                            // Store for decryption!
                            memcpy(srv_dh_public, &resp[p + 12], 32);
                            have_srv_dh = true;
                        }
                    }
                }
                
                ESP_LOGI(TAG, "");
                break;
            }
            if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R') {
                printf("  Error: ");
                for (int j = i; j < content_len && j < i + 30; j++) {
                    char c = resp[j];
                    printf("%c", (c >= 32 && c < 127) ? c : '.');
                }
                printf("\n");
                ESP_LOGE(TAG, "  ❌ Server returned ERROR!");
                break;
            }
        }
    } else if (content_len == -2) {
        ESP_LOGW(TAG, "      Timeout waiting for response");
    } else {
        ESP_LOGW(TAG, "      Connection closed: %d", content_len);
    }

    // ========== Step 7: Send SUB Command (if we got recipientId) ==========
    if (recipient_id_len > 0) {
        ESP_LOGI(TAG, "[7/7] Sending SUB command...");
        
        /*
         * SUB transmission body:
         *   [corrIdLen][corrId][entityIdLen][recipientId][command = "SUB"]
         */
        uint8_t sub_body[64];
        pos = 0;
        
        // CorrId = "2"
        sub_body[pos++] = 1;
        sub_body[pos++] = '2';
        
        // EntityId = recipientId (THIS IS THE KEY DIFFERENCE!)
        sub_body[pos++] = recipient_id_len;
        memcpy(&sub_body[pos], recipient_id, recipient_id_len);
        pos += recipient_id_len;
        
        // Command: "SUB"
        sub_body[pos++] = 'S';
        sub_body[pos++] = 'U';
        sub_body[pos++] = 'B';
        
        int sub_body_len = pos;
        
        // Sign: [0x20][sessionId] + sub_body
        uint8_t sub_to_sign[1 + 32 + 64];
        int sub_sign_pos = 0;
        sub_to_sign[sub_sign_pos++] = 32;
        memcpy(&sub_to_sign[sub_sign_pos], session_id, 32);
        sub_sign_pos += 32;
        memcpy(&sub_to_sign[sub_sign_pos], sub_body, sub_body_len);
        sub_sign_pos += sub_body_len;
        
        uint8_t sub_sig[crypto_sign_BYTES];
        crypto_sign_detached(sub_sig, NULL, sub_to_sign, sub_sign_pos, rcv_auth_secret);
        
        // Build transmission
        uint8_t sub_trans[256];
        int sub_tpos = 0;
        
        sub_trans[sub_tpos++] = crypto_sign_BYTES;
        memcpy(&sub_trans[sub_tpos], sub_sig, crypto_sign_BYTES);
        sub_tpos += crypto_sign_BYTES;
        
        sub_trans[sub_tpos++] = 32;
        memcpy(&sub_trans[sub_tpos], session_id, 32);
        sub_tpos += 32;
        
        memcpy(&sub_trans[sub_tpos], sub_body, sub_body_len);
        sub_tpos += sub_body_len;
        
        ret = smp_write_command_block(&ssl, block, sub_trans, sub_tpos);
        if (ret != 0) {
            ESP_LOGE(TAG, "      Failed to send SUB");
        } else {
            ESP_LOGI(TAG, "      SUB sent! Waiting for response...");
            
            content_len = smp_read_block(&ssl, block, 15000);
            if (content_len >= 0) {
                uint8_t *resp = block + 2;
                
                // DEBUG: Show what we actually received
                ESP_LOGI(TAG, "      SUB Response: %d bytes", content_len);
                printf("      Hex: ");
                for (int i = 0; i < content_len && i < 50; i++) {
                    printf("%02x ", resp[i]);
                }
                printf("\n");
                printf("      ASCII: ");
                for (int i = 0; i < content_len && i < 50; i++) {
                    char c = resp[i];
                    printf("%c", (c >= 32 && c < 127) ? c : '.');
                }
                printf("\n");
                
                // Parse transport format: [txCount][txLen][transmission]
                // Skip to actual response after header
                int p = 0;
                if (resp[p] == 1) {  // txCount = 1
                    p++;
                    p += 2;  // skip txLen
                    
                    // Now we're in the transmission
                    int authLen = resp[p++];
                    p += authLen;  // skip signature if any
                    
                    int sessLen = resp[p++];
                    p += sessLen;  // skip sessionId
                    
                    int corrLen = resp[p++];
                    p += corrLen;  // skip corrId
                    
                    int entLen = resp[p++];
                    p += entLen;  // skip entityId
                    
                    // NOW we're at the command!
                    ESP_LOGI(TAG, "      Command at position %d:", p);
                    printf("      -> ");
                    for (int i = p; i < content_len; i++) {
                        char c = resp[i];
                        printf("%c", (c >= 32 && c < 127) ? c : '.');
                    }
                    printf("\n");
                    
                    if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
                        ESP_LOGI(TAG, "");
                        ESP_LOGI(TAG, "  ✅ SUBSCRIBED! Ready to receive messages.");
                        ESP_LOGI(TAG, "");
                        
                        // Generate connection URI for external client
                        ESP_LOGI(TAG, "");
                        ESP_LOGI(TAG, "  📋 Connection Info for Desktop Client:");
                        ESP_LOGI(TAG, "  Server: %s:%d", SMP_HOST, SMP_PORT);
                        ESP_LOGI(TAG, "  SenderId (hex): ");
                        printf("     ");
                        for (int j = 0; j < 24; j++) printf("%02x", recipient_id[j]);
                        printf("\n");
                        ESP_LOGI(TAG, "  rcvDhPubKey (hex): ");
                        printf("     ");
                        for (int j = 0; j < 32; j++) printf("%02x", rcv_dh_public[j]);
                        printf("\n");
                        
                        // ========== SEND Test ==========
                        ESP_LOGI(TAG, "");
                        ESP_LOGI(TAG, "[8/8] Testing SEND command...");
                        
                        if (sender_id_len == 0) {
                            ESP_LOGE(TAG, "      No senderId available!");
                        } else {
                            ESP_LOGI(TAG, "      Using senderId (%d bytes)", sender_id_len);
                            
                            /*
                             * SEND transmission body:
                             *   [corrIdLen][corrId][entityIdLen][senderId][command]
                             * 
                             * Command: "SEND " [msgFlags 1 byte] [msgBody]
                             * For now: unencrypted test message (will fail but shows format)
                             */
                            uint8_t send_body[256];
                            int sp = 0;
                            
                            // CorrId = "3"
                            send_body[sp++] = 1;
                            send_body[sp++] = '3';
                            
                            // EntityId = senderId (NOT recipientId!)
                            send_body[sp++] = sender_id_len;
                            memcpy(&send_body[sp], sender_id, sender_id_len);
                            sp += sender_id_len;
                            
                            // Command: "SEND "
                            send_body[sp++] = 'S';
                            send_body[sp++] = 'E';
                            send_body[sp++] = 'N';
                            send_body[sp++] = 'D';
                            send_body[sp++] = ' ';
                            
                            // msgFlags = 'F' (False = no notification) - ASCII character!
                            send_body[sp++] = 'F';
                            send_body[sp++] = ' ';  // Space before message body!
                            
                            // msgBody = test message (should be encrypted ClientMsgEnvelope, but testing format first)
                            const char *test_msg = "Hello from ESP32!";
                            int msg_len = strlen(test_msg);
                            memcpy(&send_body[sp], test_msg, msg_len);
                            sp += msg_len;
                            
                            int send_body_len = sp;
                            ESP_LOGI(TAG, "      SEND body: %d bytes", send_body_len);
                            
                            // Sign: [0x20][sessionId] + send_body
                            // BUT WAIT - SEND doesn't need signature from us!
                            // Sender uses sndAuthKey (which server has from SKEY)
                            // For now, try without signature (authLen = 0)
                            
                            uint8_t send_trans[256];
                            int stp = 0;
                            
                            // No signature for SEND (sender authenticates differently)
                            send_trans[stp++] = 0;  // authLen = 0
                            
                            // SessionId
                            send_trans[stp++] = 32;
                            memcpy(&send_trans[stp], session_id, 32);
                            stp += 32;
                            
                            // Body
                            memcpy(&send_trans[stp], send_body, send_body_len);
                            stp += send_body_len;
                            
                            ESP_LOGI(TAG, "      Transmission: %d bytes", stp);
                            
                            // Debug: show what we're sending
                            printf("      Hex: ");
                            for (int j = 0; j < stp && j < 50; j++) {
                                printf("%02x ", send_trans[j]);
                            }
                            printf("...\n");
                            
                            ret = smp_write_command_block(&ssl, block, send_trans, stp);
                            if (ret != 0) {
                                ESP_LOGE(TAG, "      Failed to send SEND command");
                            } else {
                                ESP_LOGI(TAG, "      SEND command sent! Waiting for response...");
                                
                                content_len = smp_read_block(&ssl, block, 15000);
                                if (content_len >= 0) {
                                    uint8_t *sresp = block + 2;
                                    
                                    ESP_LOGI(TAG, "      SEND Response: %d bytes", content_len);
                                    printf("      Hex: ");
                                    for (int j = 0; j < content_len && j < 50; j++) {
                                        printf("%02x ", sresp[j]);
                                    }
                                    printf("\n");
                                    printf("      ASCII: ");
                                    for (int j = 0; j < content_len && j < 50; j++) {
                                        char c = sresp[j];
                                        printf("%c", (c >= 32 && c < 127) ? c : '.');
                                    }
                                    printf("\n");
                                    
                                    // Parse transport format to find actual response
                                    int rp = 0;
                                    if (sresp[rp] == 1) {
                                        rp++;
                                        rp += 2;  // skip txLen
                                        int rauthLen = sresp[rp++];
                                        rp += rauthLen;
                                        int rsessLen = sresp[rp++];
                                        rp += rsessLen;
                                        int rcorrLen = sresp[rp++];
                                        rp += rcorrLen;
                                        int rentLen = sresp[rp++];
                                        rp += rentLen;
                                        
                                    // Check if it's MSG (our own message back!)
                                        if (rp + 3 < content_len && sresp[rp] == 'M' && sresp[rp+1] == 'S' && sresp[rp+2] == 'G') {
                                            ESP_LOGI(TAG, "      📬 Got our MSG back!");
                                            rp += 4;  // Skip "MSG "
                                            
                                            // Parse msgId
                                            uint8_t msgIdLen = sresp[rp++];
                                            uint8_t msg_id[24];
                                            memset(msg_id, 0, 24);
                                            memcpy(msg_id, &sresp[rp], msgIdLen < 24 ? msgIdLen : 24);
                                            
                                            printf("      MsgId: ");
                                            for (int i = 0; i < msgIdLen; i++) printf("%02x", sresp[rp + i]);
                                            printf("\n");
                                            rp += msgIdLen;
                                            
                                            // Rest is encrypted body
                                            int enc_len = content_len - rp;
                                            ESP_LOGI(TAG, "      Encrypted: %d bytes", enc_len);
                                            
                                            if (have_srv_dh && enc_len > crypto_box_MACBYTES) {
                                                uint8_t shared[crypto_box_BEFORENMBYTES];
                                                if (crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret) == 0) {
                                                    uint8_t *plain = malloc(enc_len);
                                                    if (plain) {
                                                        if (crypto_box_open_easy_afternm(plain, &sresp[rp], enc_len, msg_id, shared) == 0) {
                                                            int plain_len = enc_len - crypto_box_MACBYTES;
                                                            ESP_LOGI(TAG, "  🔓 DECRYPTED (%d bytes):", plain_len);
                                                            printf("      ");
                                                            for (int i = 0; i < plain_len && i < 100; i++) {
                                                                char c = plain[i];
                                                                printf("%c", (c >= 32 && c < 127) ? c : '.');
                                                            }
                                                            printf("\n");
                                                        } else {
                                                            ESP_LOGE(TAG, "      ❌ Decryption failed");
                                                        }
                                                        free(plain);
                                                    }
                                                }
                                            }
                                        } else {
                                            printf("      Command: ");
                                            for (int j = rp; j < content_len && j < rp + 20; j++) {
                                                char c = sresp[j];
                                                printf("%c", (c >= 32 && c < 127) ? c : '.');
                                            }
                                            printf("\n");
                                        }
                                    }
                                }
                            }
                        }
                        
                        // ========== Message Receive Loop ==========
                        ESP_LOGI(TAG, "  📨 Waiting for messages... (Ctrl+C to exit)");
                        ESP_LOGI(TAG, "");
                        
                        while (1) {
                            content_len = smp_read_block(&ssl, block, 60000);  // 60s timeout
                            
                            if (content_len == -2) {
                                // Timeout - just keep waiting
                                ESP_LOGI(TAG, "      ... still waiting");
                                continue;
                            }
                            
                            if (content_len < 0) {
                                ESP_LOGW(TAG, "      Connection closed");
                                break;
                            }
                            
                            resp = block + 2;
                            ESP_LOGI(TAG, "  📩 Received %d bytes!", content_len);
                            
                            // Parse transport format
                            p = 0;
                            if (resp[p] != 1) {
                                ESP_LOGW(TAG, "      Unexpected txCount: %d", resp[p]);
                                continue;
                            }
                            p++;
                            p += 2;  // skip txLen
                            
                            // Skip to command
                            int msgAuthLen = resp[p++];
                            p += msgAuthLen;
                            int msgSessLen = resp[p++];
                            p += msgSessLen;
                            int msgCorrLen = resp[p++];
                            p += msgCorrLen;
                            int msgEntLen = resp[p++];
                            p += msgEntLen;
                            
                            // Check command type
                            if (p + 3 < content_len && resp[p] == 'M' && resp[p+1] == 'S' && resp[p+2] == 'G') {
                                ESP_LOGI(TAG, "  💬 MESSAGE received!");
                                p += 4;  // Skip "MSG "
                                
                                // Parse: [msgIdLen][msgId][encryptedBody...]
                                if (p < content_len) {
                                    uint8_t msgIdLen = resp[p++];
                                    
                                    // Store msgId for nonce
                                    uint8_t msg_id[24];
                                    memset(msg_id, 0, 24);  // Pad with zeros
                                    int copy_len = (msgIdLen < 24) ? msgIdLen : 24;
                                    memcpy(msg_id, &resp[p], copy_len);
                                    
                                    printf("      MsgId (%d bytes): ", msgIdLen);
                                    for (int i = 0; i < msgIdLen && p + i < content_len; i++) {
                                        printf("%02x", resp[p + i]);
                                    }
                                    printf("\n");
                                    p += msgIdLen;
                                    
                                    // Rest is encrypted body
                                    int enc_body_len = content_len - p;
                                    ESP_LOGI(TAG, "      Encrypted body: %d bytes", enc_body_len);
                                    
                                    if (have_srv_dh && enc_body_len > crypto_box_MACBYTES) {
                                        // Compute DH shared key
                                        uint8_t shared_key[crypto_box_BEFORENMBYTES];
                                        if (crypto_box_beforenm(shared_key, srv_dh_public, rcv_dh_secret) == 0) {
                                            ESP_LOGI(TAG, "      DH shared key computed!");
                                            
                                            // Decrypt
                                            uint8_t *decrypted = malloc(enc_body_len);
                                            if (decrypted) {
                                                // crypto_box_open_easy_afternm needs: [16-byte MAC][ciphertext]
                                                if (crypto_box_open_easy_afternm(
                                                        decrypted, 
                                                        &resp[p], 
                                                        enc_body_len, 
                                                        msg_id,  // nonce (24 bytes)
                                                        shared_key) == 0) {
                                                    
                                                    int plain_len = enc_body_len - crypto_box_MACBYTES;
                                                    ESP_LOGI(TAG, "  🔓 DECRYPTED! (%d bytes)", plain_len);
                                                    
                                                    // Show decrypted content
                                                    printf("      Content: ");
                                                    for (int i = 0; i < plain_len && i < 200; i++) {
                                                        char c = decrypted[i];
                                                        printf("%c", (c >= 32 && c < 127) ? c : '.');
                                                    }
                                                    printf("\n");
                                                    
                                                    // Also hex dump first 50 bytes
                                                    printf("      Hex: ");
                                                    for (int i = 0; i < plain_len && i < 50; i++) {
                                                        printf("%02x ", decrypted[i]);
                                                    }
                                                    printf("\n");
                                                    
                                                } else {
                                                    ESP_LOGE(TAG, "      ❌ Decryption failed!");
                                                }
                                                free(decrypted);
                                            }
                                        } else {
                                            ESP_LOGE(TAG, "      DH key computation failed!");
                                        }
                                    } else {
                                        ESP_LOGW(TAG, "      Cannot decrypt (no srv_dh_public or body too short)");
                                    }
                                }
                                ESP_LOGI(TAG, "");
                                
                            } else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'N' && resp[p+2] == 'D') {
                                ESP_LOGI(TAG, "  🔚 END - No more messages in queue");
                            } else {
                                printf("      Unknown command: ");
                                for (int i = p; i < content_len && i < p + 20; i++) {
                                    char c = resp[i];
                                    printf("%c", (c >= 32 && c < 127) ? c : '.');
                                }
                                printf("\n");
                            }
                        }
                        
                    } else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
                        printf("  SUB Error: ");
                        for (int j = p; j < content_len && j < p + 30; j++) {
                            char c = resp[j];
                            printf("%c", (c >= 32 && c < 127) ? c : '.');
                        }
                        printf("\n");
                    }
                } else {
                    ESP_LOGW(TAG, "      Unexpected txCount: %d", resp[0]);
                }
            }
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "    Test complete!");
    ESP_LOGI(TAG, "========================================");

cleanup:
    free(block);
    mbedtls_ssl_close_notify(&ssl);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    if (sock >= 0) close(sock);
}

void app_main(void) {
    ESP_LOGI(TAG, "SimpleGo starting...");
    
    // Initialize libsodium
    if (sodium_init() < 0) {
        ESP_LOGE(TAG, "libsodium init failed!");
        return;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();

    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    smp_connect();

    ESP_LOGI(TAG, "Done!");
    while (1) vTaskDelay(pdMS_TO_TICKS(10000));
}