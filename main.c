/**
 * SimpleGo - Native SimpleX SMP Client for ESP32
 * 
 * Version: 0.4.1 (internal v4.1)
 * Status: NEW + SUB commands working
 * 
 * Copyright (C) 2025 Sascha (cannatoshi)
 * Part of the Sentinel Secure Messenger Suite
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 * 
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * 
 * ============================================================================
 * IMPLEMENTATION NOTES
 * ============================================================================
 * 
 * This is the first known native SMP (SimpleX Messaging Protocol) client
 * implementation outside of the official Haskell codebase. Key discoveries:
 * 
 * 1. keyHash must be computed from CA certificate (2nd in chain)
 * 2. libsodium required (Monocypher Ed25519 incompatible with SimpleX)
 * 3. Command blocks differ from handshake blocks (different headers)
 * 4. SMP v6+ requires subMode parameter on NEW command
 * 5. Signed data format: [0x20][sessionId][body] (length-prefixed sessionId)
 * 
 * See docs/PROTOCOL.md and docs/TECHNICAL.md for detailed documentation.
 * ============================================================================
 */

#include <string.h>
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

#include "sodium.h"  // libsodium for Ed25519 (CRITICAL: not Monocypher!)

static const char *TAG = "SMP";

/* ============================================================================
 * CONFIGURATION
 * ============================================================================
 * Modify these values for your setup:
 * - WIFI_SSID/WIFI_PASS: Your WiFi credentials
 * - SMP_HOST: SimpleX server (any SMP server works)
 * - SMP_PORT: Usually 5223 for SMP
 * ============================================================================ */

#define WIFI_SSID     "YourNetworkSSID"      // TODO: Set your WiFi SSID
#define WIFI_PASS     "YourNetworkPassword"  // TODO: Set your WiFi password
#define SMP_HOST      "smp3.simplexonflux.com"
#define SMP_PORT      5223
#define SMP_BLOCK_SIZE 16384  // Fixed 16KB block size per SMP spec

/* ============================================================================
 * SPKI (Subject Public Key Info) Headers
 * ============================================================================
 * SimpleX uses SPKI encoding for public keys. Format:
 *   [12-byte ASN.1 header][32-byte public key] = 44 bytes total
 * 
 * The headers encode the algorithm OID:
 * - Ed25519: 1.3.101.112
 * - X25519:  1.3.101.110
 * ============================================================================ */

static const uint8_t ED25519_SPKI_HEADER[12] = {
    0x30, 0x2a,              // SEQUENCE, 42 bytes total
    0x30, 0x05,              // SEQUENCE, 5 bytes (AlgorithmIdentifier)
    0x06, 0x03,              // OID, 3 bytes
    0x2b, 0x65, 0x70,        // 1.3.101.112 (id-Ed25519)
    0x03, 0x21, 0x00         // BIT STRING, 33 bytes
};

static const uint8_t X25519_SPKI_HEADER[12] = {
    0x30, 0x2a,              // SEQUENCE, 42 bytes total
    0x30, 0x05,              // SEQUENCE, 5 bytes (AlgorithmIdentifier)
    0x06, 0x03,              // OID, 3 bytes
    0x2b, 0x65, 0x6e,        // 1.3.101.110 (id-X25519)
    0x03, 0x21, 0x00         // BIT STRING, 33 bytes
};

#define SPKI_KEY_SIZE 44  // 12 header + 32 key bytes

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================ */

static volatile bool wifi_connected = false;

// TLS 1.3 cipher suite - SimpleX requires ChaCha20-Poly1305
static const int ciphersuites[] = {
    MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
    0  // Null-terminated
};

/* ============================================================================
 * TCP HELPERS
 * ============================================================================ */

/**
 * Establish TCP connection to host:port
 * Returns socket fd on success, -1 on failure
 */
static int tcp_connect(const char *host, int port) {
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed for %s: %d", host, err);
        return -1;
    }
    
    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket creation failed: %d", errno);
        freeaddrinfo(res);
        return -1;
    }
    
    // Set receive timeout
    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "TCP connect failed: %d", errno);
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    
    freeaddrinfo(res);
    return sock;
}

/* ============================================================================
 * MBEDTLS I/O CALLBACKS
 * ============================================================================ */

static int my_send_cb(void *ctx, const unsigned char *buf, size_t len) {
    int sock = *(int *)ctx;
    int ret = send(sock, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) 
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        return -1;
    }
    return ret;
}

static int my_recv_cb(void *ctx, unsigned char *buf, size_t len) {
    int sock = *(int *)ctx;
    int ret = recv(sock, buf, len, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) 
            return MBEDTLS_ERR_SSL_WANT_READ;
        return -1;
    }
    return ret;
}

/* ============================================================================
 * SMP BLOCK I/O
 * ============================================================================
 * SMP uses fixed 16KB blocks, padded with '#' characters.
 * Two formats exist:
 * 
 * HANDSHAKE BLOCK (ServerHello, ClientHello):
 *   [contentLen:2][content:N][padding:'#']
 * 
 * COMMAND BLOCK (NEW, SUB, SEND, etc.):
 *   [origLen:2][txCount:1][txLen:2][transmission:N][padding:'#']
 *   where origLen = 1 + 2 + txLen
 * ============================================================================ */

static TickType_t get_tick_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/**
 * Read exact number of bytes with timeout
 * Returns bytes read on success, negative on error
 */
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
            ESP_LOGW(TAG, "Connection closed by peer");
            return -1;
        } else {
            ESP_LOGE(TAG, "TLS read error: -0x%04X", -ret);
            return -1;
        }
        
        if ((get_tick_ms() - start) > (TickType_t)timeout_ms) {
            if (received > 0) return received;
            return -2;  // Timeout
        }
    }
    return received;
}

/**
 * Read one 16KB SMP block
 * Returns content length on success, negative on error
 */
static int smp_read_block(mbedtls_ssl_context *ssl, uint8_t *block, int timeout_ms) {
    int ret = read_exact(ssl, block, SMP_BLOCK_SIZE, timeout_ms);
    if (ret < 0) return ret;
    
    // Extract content length (big-endian)
    uint16_t content_len = (block[0] << 8) | block[1];
    if (content_len > SMP_BLOCK_SIZE - 2) {
        ESP_LOGE(TAG, "Invalid content length: %d", content_len);
        return -3;
    }
    return content_len;
}

/**
 * Write a HANDSHAKE block (simple format)
 * Used for: ServerHello, ClientHello
 */
static int smp_write_handshake_block(mbedtls_ssl_context *ssl, uint8_t *block, 
                                      const uint8_t *content, size_t content_len) {
    if (content_len > SMP_BLOCK_SIZE - 2) return -1;
    
    // Fill with padding
    memset(block, '#', SMP_BLOCK_SIZE);
    
    // Set content length (big-endian)
    block[0] = (content_len >> 8) & 0xFF;
    block[1] = content_len & 0xFF;
    
    // Copy content
    memcpy(block + 2, content, content_len);
    
    // Write entire block
    int written = 0;
    while (written < SMP_BLOCK_SIZE) {
        int ret = mbedtls_ssl_write(ssl, block + written, SMP_BLOCK_SIZE - written);
        if (ret > 0) {
            written += ret;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGE(TAG, "TLS write error: -0x%04X", -ret);
            return ret;
        }
    }
    return 0;
}

/**
 * Write a COMMAND block (transport format)
 * Used for: NEW, SUB, SEND, ACK, etc.
 * 
 * Format:
 *   [origLen:2][txCount:1][txLen:2][transmission:N][padding]
 */
static int smp_write_command_block(mbedtls_ssl_context *ssl, uint8_t *block,
                                    const uint8_t *transmission, size_t trans_len) {
    memset(block, '#', SMP_BLOCK_SIZE);
    
    int pos = 0;
    
    // originalLength = 1 (txCount) + 2 (txLen) + trans_len
    uint16_t orig_len = 1 + 2 + trans_len;
    block[pos++] = (orig_len >> 8) & 0xFF;
    block[pos++] = orig_len & 0xFF;
    
    // transmissionCount = 1 (always 1 for client)
    block[pos++] = 1;
    
    // transmissionLength
    block[pos++] = (trans_len >> 8) & 0xFF;
    block[pos++] = trans_len & 0xFF;
    
    // transmission data
    memcpy(&block[pos], transmission, trans_len);
    
    // Write entire block
    int written = 0;
    while (written < SMP_BLOCK_SIZE) {
        int ret = mbedtls_ssl_write(ssl, block + written, SMP_BLOCK_SIZE - written);
        if (ret > 0) {
            written += ret;
        } else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(10));
        } else {
            ESP_LOGE(TAG, "TLS write error: -0x%04X", -ret);
            return ret;
        }
    }
    return 0;
}

/* ============================================================================
 * CERTIFICATE PARSING
 * ============================================================================
 * ServerHello contains a certificate chain (DER format):
 *   [Server Certificate][CA Certificate]
 * 
 * CRITICAL: keyHash must be computed from CA certificate (2nd), not server cert!
 * ============================================================================ */

/**
 * Find certificate boundaries in DER-encoded chain
 * Certificates start with 0x30 0x82 (SEQUENCE with 2-byte length)
 */
static int parse_cert_chain(const uint8_t *data, int len, 
                           int *cert1_off, int *cert1_len,
                           int *cert2_off, int *cert2_len) {
    *cert1_off = -1;
    *cert2_off = -1;
    
    // Find first certificate
    for (int i = 0; i < len - 4; i++) {
        if (data[i] == 0x30 && data[i+1] == 0x82) {
            *cert1_off = i;
            *cert1_len = ((data[i+2] << 8) | data[i+3]) + 4;  // +4 for header
            break;
        }
    }
    
    if (*cert1_off < 0) return -1;
    
    // Find second certificate (CA cert)
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

/* ============================================================================
 * WIFI HANDLING
 * ============================================================================ */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
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

/* ============================================================================
 * MAIN SMP CONNECTION
 * ============================================================================
 * This function implements the complete SMP protocol flow:
 *   1. TCP + TLS connection
 *   2. Receive ServerHello
 *   3. Send ClientHello
 *   4. Generate keypairs
 *   5. Send NEW command
 *   6. Receive IDS response
 *   7. Send SUB command
 *   8. Receive OK
 * ============================================================================ */

static void smp_connect(void) {
    int ret;
    int sock = -1;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    // Session state
    uint8_t session_id[32];
    
    // Recipient keys for queue
    uint8_t rcv_auth_secret[crypto_sign_SECRETKEYBYTES];  // 64 bytes (libsodium)
    uint8_t rcv_auth_public[crypto_sign_PUBLICKEYBYTES];  // 32 bytes
    uint8_t rcv_dh_secret[32];  // X25519 secret
    uint8_t rcv_dh_public[32];  // X25519 public
    
    // Queue IDs from server
    uint8_t recipient_id[24];
    uint8_t recipient_id_len = 0;

    // Allocate block buffer
    uint8_t *block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "Failed to allocate block buffer!");
        return;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  SimpleGo v0.4.1 - Native SMP Client");
    ESP_LOGI(TAG, "  Part of Sentinel Secure Messenger Suite");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    // Initialize mbedTLS contexts
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "DRBG seed failed: -0x%04X", -ret);
        goto cleanup;
    }

    /* ========== Step 1: TCP + TLS ========== */
    ESP_LOGI(TAG, "[1/7] Establishing TCP + TLS connection...");
    
    sock = tcp_connect(SMP_HOST, SMP_PORT);
    if (sock < 0) {
        ESP_LOGE(TAG, "TCP connection failed");
        goto cleanup;
    }

    // Configure TLS
    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) goto cleanup;

    // Force TLS 1.3 (required by SimpleX)
    mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);  // TODO: Implement cert verification
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

    // Set ALPN protocol (required)
    static const char *alpn_list[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&conf, alpn_list);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) goto cleanup;

    // Set SNI hostname
    mbedtls_ssl_set_hostname(&ssl, SMP_HOST);
    mbedtls_ssl_set_bio(&ssl, &sock, my_send_cb, my_recv_cb, NULL);

    // Perform TLS handshake
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "TLS handshake failed: -0x%04X", -ret);
            goto cleanup;
        }
    }
    ESP_LOGI(TAG, "      TLS OK! ALPN: %s", mbedtls_ssl_get_alpn_protocol(&ssl));

    /* ========== Step 2: ServerHello ========== */
    ESP_LOGI(TAG, "[2/7] Waiting for ServerHello...");
    
    int content_len = smp_read_block(&ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "No ServerHello received");
        goto cleanup;
    }

    uint8_t *hello = block + 2;  // Skip length prefix
    
    // Parse protocol versions
    uint16_t minVer = (hello[0] << 8) | hello[1];
    uint16_t maxVer = (hello[2] << 8) | hello[3];
    
    // Parse session ID
    uint8_t sessIdLen = hello[4];
    if (sessIdLen != 32) {
        ESP_LOGE(TAG, "Unexpected sessionId length: %d", sessIdLen);
        goto cleanup;
    }
    memcpy(session_id, &hello[5], 32);
    
    ESP_LOGI(TAG, "      Server versions: %d-%d", minVer, maxVer);
    ESP_LOGI(TAG, "      SessionId: %02x%02x%02x%02x...", 
             session_id[0], session_id[1], session_id[2], session_id[3]);

    /* ========== Step 3: ClientHello ========== */
    ESP_LOGI(TAG, "[3/7] Sending ClientHello...");
    
    // Parse certificate chain to find CA cert
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);
    
    // CRITICAL: Hash CA certificate (2nd cert), not server certificate!
    uint8_t ca_hash[32];
    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
        ESP_LOGI(TAG, "      Using CA certificate for keyHash");
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
        ESP_LOGW(TAG, "      Fallback to server certificate (no CA found)");
    }
    
    // Build ClientHello (v6 format)
    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;  // Protocol version 6
    client_hello[pos++] = 32;    // keyHash length
    memcpy(&client_hello[pos], ca_hash, 32);
    pos += 32;
    
    ret = smp_write_handshake_block(&ssl, block, client_hello, pos);
    if (ret != 0) goto cleanup;
    ESP_LOGI(TAG, "      ClientHello sent!");

    /* ========== Step 4: Generate Keypairs ========== */
    ESP_LOGI(TAG, "[4/7] Generating keypairs...");
    
    // Ed25519 keypair for rcvAuthKey (used for signing commands)
    uint8_t seed[32];
    esp_fill_random(seed, 32);
    crypto_sign_seed_keypair(rcv_auth_public, rcv_auth_secret, seed);
    
    // X25519 keypair for rcvDhKey (used for key exchange)
    esp_fill_random(rcv_dh_secret, 32);
    crypto_scalarmult_base(rcv_dh_public, rcv_dh_secret);
    
    ESP_LOGI(TAG, "      rcvAuthKey (Ed25519): %02x%02x%02x%02x...", 
             rcv_auth_public[0], rcv_auth_public[1], rcv_auth_public[2], rcv_auth_public[3]);
    ESP_LOGI(TAG, "      rcvDhKey (X25519):    %02x%02x%02x%02x...",
             rcv_dh_public[0], rcv_dh_public[1], rcv_dh_public[2], rcv_dh_public[3]);

    /* ========== Step 5: Send NEW Command ========== */
    ESP_LOGI(TAG, "[5/7] Sending NEW command...");
    
    // Build SPKI-encoded keys
    uint8_t rcv_auth_spki[SPKI_KEY_SIZE];
    memcpy(rcv_auth_spki, ED25519_SPKI_HEADER, 12);
    memcpy(rcv_auth_spki + 12, rcv_auth_public, 32);
    
    uint8_t rcv_dh_spki[SPKI_KEY_SIZE];
    memcpy(rcv_dh_spki, X25519_SPKI_HEADER, 12);
    memcpy(rcv_dh_spki + 12, rcv_dh_public, 32);
    
    /*
     * Build transmission body (what we sign):
     *   [corrIdLen][corrId][entityIdLen][entityId][command...]
     * 
     * For NEW command:
     *   "NEW " [keyLen][rcvAuthKey SPKI] [keyLen][rcvDhKey SPKI] [subMode]
     */
    uint8_t trans_body[256];
    pos = 0;
    
    // CorrId = "1"
    trans_body[pos++] = 1;    // corrId length
    trans_body[pos++] = '1';  // corrId value
    
    // EntityId = empty (for NEW command, queue doesn't exist yet)
    trans_body[pos++] = 0;    // entityId length = 0
    
    // Command: "NEW " + keys + subMode
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
    
    // subMode = 'S' (SMSubscribe) - REQUIRED for SMP v6+!
    trans_body[pos++] = 'S';
    
    int trans_body_len = pos;
    
    /*
     * Sign: smpEncode(sessionId) + transmission_body
     * smpEncode adds length prefix: [0x20][sessionId 32 bytes]
     */
    uint8_t to_sign[1 + 32 + 256];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;  // Length prefix for sessionId
    memcpy(&to_sign[sign_pos], session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
    sign_pos += trans_body_len;
    
    // Sign with Ed25519 (libsodium)
    uint8_t signature[crypto_sign_BYTES];  // 64 bytes
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, rcv_auth_secret);
    
    // Verify signature locally (sanity check)
    int verify_result = crypto_sign_verify_detached(signature, to_sign, sign_pos, rcv_auth_public);
    if (verify_result == 0) {
        ESP_LOGI(TAG, "      ✅ Signature verified locally");
    } else {
        ESP_LOGE(TAG, "      ❌ Local signature verification FAILED!");
        goto cleanup;
    }
    
    /*
     * Build final transmission:
     *   [sigLen][signature 64 bytes]
     *   [sessLen][sessionId 32 bytes]
     *   [transmission_body]
     */
    uint8_t transmission[256];
    int tpos = 0;
    
    transmission[tpos++] = crypto_sign_BYTES;  // 64
    memcpy(&transmission[tpos], signature, crypto_sign_BYTES);
    tpos += crypto_sign_BYTES;
    
    transmission[tpos++] = 32;  // sessionId length
    memcpy(&transmission[tpos], session_id, 32);
    tpos += 32;
    
    memcpy(&transmission[tpos], trans_body, trans_body_len);
    tpos += trans_body_len;
    
    // Send using command block format
    ret = smp_write_command_block(&ssl, block, transmission, tpos);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to send NEW command");
        goto cleanup;
    }
    ESP_LOGI(TAG, "      NEW command sent! (%d bytes)", tpos);

    /* ========== Step 6: Wait for IDS Response ========== */
    ESP_LOGI(TAG, "[6/7] Waiting for IDS response...");
    
    content_len = smp_read_block(&ssl, block, 15000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "No response received");
        goto cleanup;
    }
    
    uint8_t *resp = block + 2;
    
    // Look for IDS (success) or ERR in response
    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'I' && resp[i+1] == 'D' && resp[i+2] == 'S' && resp[i+3] == ' ') {
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "  🎉🎉🎉 QUEUE CREATED! 🎉🎉🎉");
            ESP_LOGI(TAG, "");
            
            // Parse IDS response: "IDS " [len][rcvId] [len][sndId] [len][srvDhKey]
            int p = i + 4;  // Skip "IDS "
            
            // RecipientId
            if (p < content_len) {
                uint8_t rcv_id_len = resp[p++];
                if (p + rcv_id_len <= content_len && rcv_id_len <= 24) {
                    ESP_LOGI(TAG, "  📥 RecipientId (%d bytes):", rcv_id_len);
                    printf("     ");
                    for (int j = 0; j < rcv_id_len; j++) {
                        printf("%02x", resp[p + j]);
                    }
                    printf("\n");
                    
                    // Store for SUB command
                    memcpy(recipient_id, &resp[p], rcv_id_len);
                    recipient_id_len = rcv_id_len;
                    
                    p += rcv_id_len;
                }
            }
            
            // SenderId
            if (p < content_len) {
                uint8_t snd_id_len = resp[p++];
                if (p + snd_id_len <= content_len) {
                    ESP_LOGI(TAG, "  📤 SenderId (%d bytes):", snd_id_len);
                    printf("     ");
                    for (int j = 0; j < snd_id_len; j++) {
                        printf("%02x", resp[p + j]);
                    }
                    printf("\n");
                    p += snd_id_len;
                }
            }
            
            // ServerDhKey
            if (p < content_len) {
                uint8_t srv_dh_len = resp[p++];
                if (p + srv_dh_len <= content_len) {
                    ESP_LOGI(TAG, "  🔑 ServerDhKey (%d bytes SPKI)", srv_dh_len);
                    if (srv_dh_len == 44) {
                        // Extract raw key (skip 12-byte header)
                        printf("     Raw key: ");
                        for (int j = 12; j < srv_dh_len; j++) {
                            printf("%02x", resp[p + j]);
                        }
                        printf("\n");
                    }
                }
            }
            ESP_LOGI(TAG, "");
            break;
        }
        
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R') {
            printf("  Error response: ");
            for (int j = i; j < content_len && j < i + 40; j++) {
                char c = resp[j];
                printf("%c", (c >= 32 && c < 127) ? c : '.');
            }
            printf("\n");
            ESP_LOGE(TAG, "  ❌ Server returned ERROR!");
            goto cleanup;
        }
    }

    /* ========== Step 7: Send SUB Command ========== */
    if (recipient_id_len > 0) {
        ESP_LOGI(TAG, "[7/7] Sending SUB command...");
        
        /*
         * SUB transmission body:
         *   [corrIdLen][corrId][entityIdLen][recipientId][command = "SUB"]
         * 
         * Note: EntityId is the recipientId (this is the key difference from NEW)
         */
        uint8_t sub_body[64];
        pos = 0;
        
        // CorrId = "2"
        sub_body[pos++] = 1;
        sub_body[pos++] = '2';
        
        // EntityId = recipientId
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
                resp = block + 2;
                
                for (int i = 0; i < content_len - 2; i++) {
                    if (resp[i] == 'O' && resp[i+1] == 'K') {
                        ESP_LOGI(TAG, "");
                        ESP_LOGI(TAG, "  ✅ SUBSCRIBED! Ready to receive messages.");
                        ESP_LOGI(TAG, "");
                        break;
                    }
                    if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R') {
                        printf("  SUB Error: ");
                        for (int j = i; j < content_len && j < i + 40; j++) {
                            char c = resp[j];
                            printf("%c", (c >= 32 && c < 127) ? c : '.');
                        }
                        printf("\n");
                        break;
                    }
                }
            }
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "    Connection test complete!");
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

/* ============================================================================
 * APPLICATION ENTRY POINT
 * ============================================================================ */

void app_main(void) {
    ESP_LOGI(TAG, "SimpleGo starting...");
    
    // Initialize libsodium
    if (sodium_init() < 0) {
        ESP_LOGE(TAG, "libsodium initialization failed!");
        return;
    }
    ESP_LOGI(TAG, "libsodium initialized");

    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    wifi_init();

    // Wait for connection
    ESP_LOGI(TAG, "Waiting for WiFi...");
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));  // Brief delay after connect

    // Run SMP connection
    smp_connect();

    ESP_LOGI(TAG, "SimpleGo complete!");
    
    // Keep running (for future: message receive loop)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
