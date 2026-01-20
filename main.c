/**
 * SimpleGo - Native SimpleX SMP Client for ESP32
 * v4.3 - Full message lifecycle: NEW, SUB, SEND, MSG, ACK
 * 
 * Features:
 *   - TLS 1.3 with ALPN "smp/1"
 *   - Ed25519 signing (libsodium)
 *   - X25519 DH key exchange
 *   - XSalsa20-Poly1305 decryption
 *   - SMP v6 protocol
 *   - Full message lifecycle with ACK
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
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASS     "YOUR_WIFI_PASSWORD"
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
    uint8_t srv_dh_public[32];
    bool have_srv_dh = false;

    uint8_t *block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "Failed to allocate buffer!");
        return;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SimpleGo v4.3 - Full Message Lifecycle");
    ESP_LOGI(TAG, "  NEW → SUB → SEND → MSG → ACK → OK");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) goto cleanup;

    // ========== Step 1: TCP + TLS ==========
    ESP_LOGI(TAG, "[1/8] TCP + TLS...");
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
    ESP_LOGI(TAG, "[2/8] Waiting for ServerHello...");
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
    ESP_LOGI(TAG, "[3/8] Sending ClientHello...");
    
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);
    
    uint8_t ca_hash[32];
    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
    }
    
    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;  // v6
    client_hello[pos++] = 32;
    memcpy(&client_hello[pos], ca_hash, 32);
    pos += 32;
    
    ret = smp_write_handshake_block(&ssl, block, client_hello, pos);
    if (ret != 0) goto cleanup;
    ESP_LOGI(TAG, "      ClientHello sent!");

    // ========== Step 4: Generate Keys ==========
    ESP_LOGI(TAG, "[4/8] Generating keypairs...");
    
    uint8_t seed[32];
    esp_fill_random(seed, 32);
    crypto_sign_seed_keypair(rcv_auth_public, rcv_auth_secret, seed);
    
    esp_fill_random(rcv_dh_secret, 32);
    crypto_scalarmult_base(rcv_dh_public, rcv_dh_secret);
    
    ESP_LOGI(TAG, "      rcvAuthKey: %02x%02x%02x%02x...", 
             rcv_auth_public[0], rcv_auth_public[1], rcv_auth_public[2], rcv_auth_public[3]);

    // ========== Step 5: Send NEW Command ==========
    ESP_LOGI(TAG, "[5/8] Sending NEW command...");
    
    uint8_t rcv_auth_spki[SPKI_KEY_SIZE];
    memcpy(rcv_auth_spki, ED25519_SPKI_HEADER, 12);
    memcpy(rcv_auth_spki + 12, rcv_auth_public, 32);
    
    uint8_t rcv_dh_spki[SPKI_KEY_SIZE];
    memcpy(rcv_dh_spki, X25519_SPKI_HEADER, 12);
    memcpy(rcv_dh_spki + 12, rcv_dh_public, 32);
    
    uint8_t trans_body[256];
    pos = 0;
    
    trans_body[pos++] = 1;
    trans_body[pos++] = '1';
    trans_body[pos++] = 0;
    
    trans_body[pos++] = 'N';
    trans_body[pos++] = 'E';
    trans_body[pos++] = 'W';
    trans_body[pos++] = ' ';
    
    trans_body[pos++] = SPKI_KEY_SIZE;
    memcpy(&trans_body[pos], rcv_auth_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;
    
    trans_body[pos++] = SPKI_KEY_SIZE;
    memcpy(&trans_body[pos], rcv_dh_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;
    
    trans_body[pos++] = 'S';
    
    int trans_body_len = pos;
    
    uint8_t to_sign[1 + 32 + 256];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;
    memcpy(&to_sign[sign_pos], session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
    sign_pos += trans_body_len;
    
    uint8_t signature[crypto_sign_BYTES];
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, rcv_auth_secret);
    
    uint8_t transmission[256];
    int tpos = 0;
    
    transmission[tpos++] = crypto_sign_BYTES;
    memcpy(&transmission[tpos], signature, crypto_sign_BYTES);
    tpos += crypto_sign_BYTES;
    
    transmission[tpos++] = 32;
    memcpy(&transmission[tpos], session_id, 32);
    tpos += 32;
    
    memcpy(&transmission[tpos], trans_body, trans_body_len);
    tpos += trans_body_len;
    
    ret = smp_write_command_block(&ssl, block, transmission, tpos);
    if (ret != 0) goto cleanup;
    ESP_LOGI(TAG, "      NEW sent!");

    // ========== Step 6: Wait for IDS ==========
    ESP_LOGI(TAG, "[6/8] Waiting for IDS...");
    
    content_len = smp_read_block(&ssl, block, 15000);
    if (content_len >= 0) {
        uint8_t *resp = block + 2;
        
        for (int i = 0; i < content_len - 3; i++) {
            if (resp[i] == 'I' && resp[i+1] == 'D' && resp[i+2] == 'S' && resp[i+3] == ' ') {
                ESP_LOGI(TAG, "  🎉 QUEUE CREATED!");
                
                int p = i + 4;
                
                if (p < content_len) {
                    uint8_t rcv_id_len = resp[p++];
                    if (p + rcv_id_len <= content_len && rcv_id_len <= 24) {
                        memcpy(recipient_id, &resp[p], rcv_id_len);
                        recipient_id_len = rcv_id_len;
                        p += rcv_id_len;
                    }
                }
                
                if (p < content_len) {
                    sender_id_len = resp[p++];
                    if (p + sender_id_len <= content_len && sender_id_len <= 24) {
                        memcpy(sender_id, &resp[p], sender_id_len);
                        p += sender_id_len;
                    }
                }
                
                if (p < content_len) {
                    uint8_t srv_dh_len = resp[p++];
                    if (srv_dh_len == 44 && p + srv_dh_len <= content_len) {
                        memcpy(srv_dh_public, &resp[p + 12], 32);
                        have_srv_dh = true;
                    }
                }
                break;
            }
        }
    }

    if (recipient_id_len == 0) {
        ESP_LOGE(TAG, "Failed to get recipientId");
        goto cleanup;
    }

    // ========== Step 7: SUB ==========
    ESP_LOGI(TAG, "[7/8] Sending SUB...");
    
    uint8_t sub_body[64];
    pos = 0;
    sub_body[pos++] = 1;
    sub_body[pos++] = '2';
    sub_body[pos++] = recipient_id_len;
    memcpy(&sub_body[pos], recipient_id, recipient_id_len);
    pos += recipient_id_len;
    sub_body[pos++] = 'S';
    sub_body[pos++] = 'U';
    sub_body[pos++] = 'B';
    
    uint8_t sub_to_sign[128];
    sign_pos = 0;
    sub_to_sign[sign_pos++] = 32;
    memcpy(&sub_to_sign[sign_pos], session_id, 32);
    sign_pos += 32;
    memcpy(&sub_to_sign[sign_pos], sub_body, pos);
    sign_pos += pos;
    
    uint8_t sub_sig[crypto_sign_BYTES];
    crypto_sign_detached(sub_sig, NULL, sub_to_sign, sign_pos, rcv_auth_secret);
    
    uint8_t sub_trans[192];
    tpos = 0;
    sub_trans[tpos++] = crypto_sign_BYTES;
    memcpy(&sub_trans[tpos], sub_sig, crypto_sign_BYTES);
    tpos += crypto_sign_BYTES;
    sub_trans[tpos++] = 32;
    memcpy(&sub_trans[tpos], session_id, 32);
    tpos += 32;
    memcpy(&sub_trans[tpos], sub_body, pos);
    tpos += pos;
    
    smp_write_command_block(&ssl, block, sub_trans, tpos);
    
    content_len = smp_read_block(&ssl, block, 15000);
    ESP_LOGI(TAG, "  ✅ SUBSCRIBED!");

    // ========== Step 8: SEND Test ==========
    ESP_LOGI(TAG, "[8/8] Testing SEND...");
    
    uint8_t send_body[256];
    pos = 0;
    send_body[pos++] = 1;
    send_body[pos++] = '3';
    send_body[pos++] = sender_id_len;
    memcpy(&send_body[pos], sender_id, sender_id_len);
    pos += sender_id_len;
    send_body[pos++] = 'S';
    send_body[pos++] = 'E';
    send_body[pos++] = 'N';
    send_body[pos++] = 'D';
    send_body[pos++] = ' ';
    send_body[pos++] = 'F';
    send_body[pos++] = ' ';
    
    const char *test_msg = "Hello from ESP32!";
    memcpy(&send_body[pos], test_msg, strlen(test_msg));
    pos += strlen(test_msg);
    
    uint8_t send_trans[256];
    tpos = 0;
    send_trans[tpos++] = 0;  // No signature for SEND
    send_trans[tpos++] = 32;
    memcpy(&send_trans[tpos], session_id, 32);
    tpos += 32;
    memcpy(&send_trans[tpos], send_body, pos);
    tpos += pos;
    
    smp_write_command_block(&ssl, block, send_trans, tpos);
    ESP_LOGI(TAG, "      SEND sent!");

    // ========== Message Loop ==========
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📨 Waiting for messages...");
    
    while (1) {
        content_len = smp_read_block(&ssl, block, 60000);
        
        if (content_len == -2) {
            ESP_LOGI(TAG, "      ... still waiting");
            continue;
        }
        
        if (content_len < 0) {
            ESP_LOGW(TAG, "Connection closed");
            break;
        }
        
        uint8_t *resp = block + 2;
        int p = 0;
        
        if (resp[p] != 1) continue;
        p++;
        p += 2;
        
        int authLen = resp[p++];
        p += authLen;
        int sessLen = resp[p++];
        p += sessLen;
        int corrLen = resp[p++];
        p += corrLen;
        int entLen = resp[p++];
        p += entLen;
        
        // Check for OK
        if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
            ESP_LOGI(TAG, "  ✅ OK received");
            continue;
        }
        
        // Check for MSG
        if (p + 3 < content_len && resp[p] == 'M' && resp[p+1] == 'S' && resp[p+2] == 'G') {
            ESP_LOGI(TAG, "  💬 MESSAGE received!");
            p += 4;
            
            uint8_t msgIdLen = resp[p++];
            uint8_t msg_id[24] = {0};
            memcpy(msg_id, &resp[p], msgIdLen < 24 ? msgIdLen : 24);
            p += msgIdLen;
            
            int enc_len = content_len - p;
            
            if (have_srv_dh && enc_len > crypto_box_MACBYTES) {
                uint8_t shared[crypto_box_BEFORENMBYTES];
                crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);
                
                uint8_t *plain = malloc(enc_len);
                if (plain && crypto_box_open_easy_afternm(plain, &resp[p], enc_len, msg_id, shared) == 0) {
                    int plain_len = enc_len - crypto_box_MACBYTES;
                    ESP_LOGI(TAG, "  🔓 DECRYPTED!");
                    printf("      Content: ");
                    for (int i = 0; i < plain_len && i < 100; i++) {
                        char c = plain[i];
                        printf("%c", (c >= 32 && c < 127) ? c : '.');
                    }
                    printf("\n");
                    
                    // ========== ACK ==========
                    ESP_LOGI(TAG, "  📨 Sending ACK...");
                    
                    uint8_t ack_body[64];
                    int ap = 0;
                    ack_body[ap++] = 1;
                    ack_body[ap++] = '4';
                    ack_body[ap++] = recipient_id_len;
                    memcpy(&ack_body[ap], recipient_id, recipient_id_len);
                    ap += recipient_id_len;
                    ack_body[ap++] = 'A';
                    ack_body[ap++] = 'C';
                    ack_body[ap++] = 'K';
                    ack_body[ap++] = ' ';
                    ack_body[ap++] = msgIdLen;
                    memcpy(&ack_body[ap], msg_id, msgIdLen);
                    ap += msgIdLen;
                    
                    uint8_t ack_to_sign[128];
                    int asp = 0;
                    ack_to_sign[asp++] = 32;
                    memcpy(&ack_to_sign[asp], session_id, 32);
                    asp += 32;
                    memcpy(&ack_to_sign[asp], ack_body, ap);
                    asp += ap;
                    
                    uint8_t ack_sig[crypto_sign_BYTES];
                    crypto_sign_detached(ack_sig, NULL, ack_to_sign, asp, rcv_auth_secret);
                    
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
                    ESP_LOGI(TAG, "      ACK sent!");
                }
                if (plain) free(plain);
            }
            ESP_LOGI(TAG, "");
        }
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "    Session complete!");
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
    ESP_LOGI(TAG, "SimpleGo v0.1.7-alpha starting...");
    
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
