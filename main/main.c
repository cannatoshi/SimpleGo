/**
 * SimpleGo - Native SimpleX SMP Client for ESP32
 * v0.1.11-alpha - Multi-Contact Test
 * github.com/cannatoshi/SimpleGo
 * Autor: cannatoshi
 * 
 * Features:
 *   - TLS 1.3 with ALPN "smp/1"
 *   - Ed25519 signing (libsodium)
 *   - X25519 DH key exchange
 *   - SMP v6 protocol
 *   - NVS persistent contact storage
 *   - Multiple contacts over single connection
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

#include "sodium.h"  // libsodium for Ed25519 + X25519

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

// ============== Base64URL Encoding ==============

static const char base64url_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int base64url_encode(const uint8_t *input, int input_len, char *output, int output_max) {
    int i, j;
    for (i = 0, j = 0; i < input_len && j < output_max - 4; ) {
        uint32_t octet_a = i < input_len ? input[i++] : 0;
        uint32_t octet_b = i < input_len ? input[i++] : 0;
        uint32_t octet_c = i < input_len ? input[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        
        output[j++] = base64url_chars[(triple >> 18) & 0x3F];
        output[j++] = base64url_chars[(triple >> 12) & 0x3F];
        output[j++] = base64url_chars[(triple >> 6) & 0x3F];
        output[j++] = base64url_chars[triple & 0x3F];
    }
    
    // Remove padding
    int mod = input_len % 3;
    if (mod == 1) j -= 2;
    else if (mod == 2) j -= 1;
    
    output[j] = '\0';
    return j;
}

// ============== Standard Base64 Encoding (with padding, for dh param) ==============

static const char base64_std_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int base64_std_encode(const uint8_t *input, int input_len, char *output, int output_max) {
    int i, j;
    for (i = 0, j = 0; i < input_len && j < output_max - 4; ) {
        uint32_t octet_a = i < input_len ? input[i++] : 0;
        uint32_t octet_b = i < input_len ? input[i++] : 0;
        uint32_t octet_c = i < input_len ? input[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
        
        output[j++] = base64_std_chars[(triple >> 18) & 0x3F];
        output[j++] = base64_std_chars[(triple >> 12) & 0x3F];
        output[j++] = base64_std_chars[(triple >> 6) & 0x3F];
        output[j++] = base64_std_chars[triple & 0x3F];
    }
    
    // Add padding for standard base64
    int mod = input_len % 3;
    if (mod == 1) {
        output[j-2] = '=';
        output[j-1] = '=';
    } else if (mod == 2) {
        output[j-1] = '=';
    }
    
    output[j] = '\0';
    return j;
}

// ============== URL Encoding ==============

// Single URL encode - encodes all special characters
static int url_encode(const char *input, char *output, int output_max) {
    static const char *hex = "0123456789ABCDEF";
    int j = 0;
    for (int i = 0; input[i] && j < output_max - 4; i++) {
        unsigned char c = (unsigned char)input[i];
        // Only keep alphanumeric and - _ . ~ unreserved
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || 
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            output[j++] = c;
        } else {
            // Percent-encode everything else including : / # ? = & @ +
            output[j++] = '%';
            output[j++] = hex[(c >> 4) & 0x0F];
            output[j++] = hex[c & 0x0F];
        }
    }
    output[j] = '\0';
    return j;
}

// Pre-encode Base64 special chars (+ and =) so they become double-encoded after url_encode
// + → %2B, = → %3D (then url_encode makes %2B → %252B, %3D → %253D)
static int base64_pre_encode(const char *input, char *output, int output_max) {
    int j = 0;
    for (int i = 0; input[i] && j < output_max - 4; i++) {
        if (input[i] == '+') {
            output[j++] = '%';
            output[j++] = '2';
            output[j++] = 'B';
        } else if (input[i] == '=') {
            output[j++] = '%';
            output[j++] = '3';
            output[j++] = 'D';
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
    return j;
}

// ============== Contact Data Structures ==============

#define MAX_CONTACTS 10
#define NVS_NAMESPACE "simplego"

typedef struct {
    char name[32];                                    // Contact name
    uint8_t rcv_auth_secret[crypto_sign_SECRETKEYBYTES]; // 64 bytes
    uint8_t rcv_auth_public[crypto_sign_PUBLICKEYBYTES]; // 32 bytes
    uint8_t rcv_dh_secret[32];                        // X25519 secret
    uint8_t rcv_dh_public[32];                        // X25519 public
    uint8_t recipient_id[24];                         // Queue recipient ID
    uint8_t recipient_id_len;
    uint8_t sender_id[24];                            // Queue sender ID
    uint8_t sender_id_len;
    uint8_t srv_dh_public[32];                        // Server DH public key
    uint8_t have_srv_dh;                              // 1 if srv_dh_public is valid
    uint8_t active;                                   // 1=active, 0=slot free
} contact_t;

typedef struct {
    uint8_t num_contacts;
    contact_t contacts[MAX_CONTACTS];
} contacts_db_t;

static contacts_db_t contacts_db;

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
            return -2;  // Timeout
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

// Write a HANDSHAKE block (simple format: length + content + padding)
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

// Write a COMMAND block (transport format with txCount + txLen)
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
    
    // Find first certificate (0x30 0x82 = ASN.1 SEQUENCE)
    for (int i = 0; i < len - 4; i++) {
        if (data[i] == 0x30 && data[i+1] == 0x82) {
            *cert1_off = i;
            *cert1_len = ((data[i+2] << 8) | data[i+3]) + 4;
            break;
        }
    }
    
    if (*cert1_off < 0) return -1;
    
    // Find second certificate (CA cert for keyHash)
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

// ============== NVS Functions ==============

static bool load_contacts_from_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "NVS: No saved contacts found");
        memset(&contacts_db, 0, sizeof(contacts_db));
        return false;
    }
    
    size_t required_size = sizeof(contacts_db_t);
    err = nvs_get_blob(handle, "contacts", &contacts_db, &required_size);
    nvs_close(handle);
    
    if (err == ESP_OK && contacts_db.num_contacts > 0) {
        ESP_LOGI(TAG, "NVS: Loaded %d contact(s)!", contacts_db.num_contacts);
        for (int i = 0; i < MAX_CONTACTS; i++) {
            if (contacts_db.contacts[i].active) {
                ESP_LOGI(TAG, "      [%d] %s (rcvId: %02x%02x%02x%02x...)", 
                         i, contacts_db.contacts[i].name,
                         contacts_db.contacts[i].recipient_id[0],
                         contacts_db.contacts[i].recipient_id[1],
                         contacts_db.contacts[i].recipient_id[2],
                         contacts_db.contacts[i].recipient_id[3]);
            }
        }
        return true;
    }
    
    ESP_LOGI(TAG, "NVS: No saved contacts found");
    memset(&contacts_db, 0, sizeof(contacts_db));
    return false;
}

static bool save_contacts_to_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS: Failed to open for writing");
        return false;
    }
    
    err = nvs_set_blob(handle, "contacts", &contacts_db, sizeof(contacts_db_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS: Failed to save contacts");
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS: Contacts saved!");
        return true;
    }
    
    ESP_LOGE(TAG, "NVS: Commit failed");
    return false;
}

static void clear_all_contacts(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_key(handle, "contacts");
        nvs_commit(handle);
        nvs_close(handle);
        memset(&contacts_db, 0, sizeof(contacts_db));
        ESP_LOGI(TAG, "NVS: All contacts cleared!");
    }
}

// ============== Contact Management ==============

static int find_contact_by_recipient_id(const uint8_t *recipient_id, uint8_t len) {
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (contacts_db.contacts[i].active && 
            contacts_db.contacts[i].recipient_id_len == len &&
            memcmp(contacts_db.contacts[i].recipient_id, recipient_id, len) == 0) {
            return i;
        }
    }
    return -1;
}

static void list_contacts(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📋 Contact List (%d active):", contacts_db.num_contacts);
    ESP_LOGI(TAG, "────────────────────────────────────");
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (contacts_db.contacts[i].active) {
            contact_t *c = &contacts_db.contacts[i];
            ESP_LOGI(TAG, "  [%d] %s", i, c->name);
            ESP_LOGI(TAG, "      rcvId: %02x%02x%02x%02x%02x%02x...", 
                     c->recipient_id[0], c->recipient_id[1], 
                     c->recipient_id[2], c->recipient_id[3],
                     c->recipient_id[4], c->recipient_id[5]);
            ESP_LOGI(TAG, "      srvDH: %s", c->have_srv_dh ? "✅" : "❌");
        }
    }
    ESP_LOGI(TAG, "────────────────────────────────────");
    ESP_LOGI(TAG, "");
}

static void print_invitation_links(const uint8_t *ca_hash) {
    char hash_b64[64];           // keyHash (Base64URL, no padding)
    char snd_b64[64];            // senderId (Base64URL, no padding)
    char dh_b64[80];             // dhPublicKey (Standard Base64 with padding)
    char dh_preencoded[256];     // DH key with + and = pre-encoded
    char smp_uri[512];           // SMP URI with pre-encoded DH
    char smp_uri_encoded[2048];  // Final URL-encoded SMP URI
    
    // Encode keyHash as Base64URL (no padding)
    base64url_encode(ca_hash, 32, hash_b64, sizeof(hash_b64));
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🔗 SIMPLEX CONTACT LINKS");
    ESP_LOGI(TAG, "══════════════════════════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "Server keyHash: %s", hash_b64);
    ESP_LOGI(TAG, "");
    
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        
        contact_t *c = &contacts_db.contacts[i];
        
        // Encode senderId as Base64URL (no padding)
        base64url_encode(c->sender_id, c->sender_id_len, snd_b64, sizeof(snd_b64));
        
        // Encode dhPublicKey as SPKI + Standard Base64 (WITH padding, WITH +/=)
        // SPKI format: 12-byte header + 32-byte raw key = 44 bytes
        uint8_t dh_spki[44];
        memcpy(dh_spki, X25519_SPKI_HEADER, 12);
        memcpy(dh_spki + 12, c->rcv_dh_public, 32);
        base64_std_encode(dh_spki, 44, dh_b64, sizeof(dh_b64));
        
        // Pre-encode + and = in the Base64 DH key
        // This makes them double-encoded after the final url_encode()
        // + → %2B → %252B (after url_encode)
        // = → %3D → %253D (after url_encode)
        base64_pre_encode(dh_b64, dh_preencoded, sizeof(dh_preencoded));
        
        // Build SMP URI with pre-encoded DH key
        // - v=1-4 for SMP client version range  
        // - q=c for Contact queue mode
        snprintf(smp_uri, sizeof(smp_uri), 
                 "smp://%s@%s:%d/%s#/?v=1-4&dh=%s&q=c",
                 hash_b64, SMP_HOST, SMP_PORT, snd_b64, dh_preencoded);
        
        // Single URL-encode the entire SMP URI
        // : → %3A, / → %2F, @ → %40, # → %23, etc. (single)
        // %2B → %252B, %3D → %253D (double, because % → %25)
        url_encode(smp_uri, smp_uri_encoded, sizeof(smp_uri_encoded));
        
        ESP_LOGI(TAG, "📱 [%d] %s", i, c->name);
        ESP_LOGI(TAG, "──────────────────────────────────────────────────────────────────────────────");
        
        // Format 1: Raw SMP Queue URI (for reference/debugging)
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "   📋 SMP Queue URI (raw):");
        printf("   smp://%s@%s:%d/%s#/?v=1-4&dh=%s&q=c\n", 
               hash_b64, SMP_HOST, SMP_PORT, snd_b64, dh_b64);
        
        // Format 2: Web landing page (MAIN - copy this!)
        // v=2-7 is the agent version range
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "   🌐 SimpleX Contact Link (COPY THIS!):");
        printf("   https://simplex.chat/contact#/?v=2-7&smp=%s\n", smp_uri_encoded);
        
        // Format 3: simplex: URI scheme (for direct app opening)
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "   📲 Direct App Link:");
        printf("   simplex:/contact#/?v=2-7&smp=%s\n", smp_uri_encoded);
        
        // Debug info
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "   🔧 Debug Info:");
        ESP_LOGI(TAG, "      senderId (b64url): %s", snd_b64);
        ESP_LOGI(TAG, "      dhPubKey (b64std): %s", dh_b64);
        printf("      senderId (hex): ");
        for (int j = 0; j < c->sender_id_len; j++) {
            printf("%02x", c->sender_id[j]);
        }
        printf("\n");
        
        ESP_LOGI(TAG, "");
    }
    
    ESP_LOGI(TAG, "══════════════════════════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📝 ANLEITUNG:");
    ESP_LOGI(TAG, "   1. Den 🌐 Web Link kopieren");
    ESP_LOGI(TAG, "   2. In SimpleX Desktop/Mobile App öffnen");
    ESP_LOGI(TAG, "   3. 'Connect' klicken");
    ESP_LOGI(TAG, "   4. Nachricht senden");
    ESP_LOGI(TAG, "   5. ESP32 empfängt MSG!");
    ESP_LOGI(TAG, "");
}

// Self-test: Send a message to our own queue
static void self_test_send(mbedtls_ssl_context *ssl, uint8_t *block,
                           const uint8_t *session_id, int contact_idx) {
    if (contact_idx < 0 || contact_idx >= MAX_CONTACTS || !contacts_db.contacts[contact_idx].active) {
        ESP_LOGE(TAG, "❌ Invalid contact for self-test");
        return;
    }
    
    contact_t *c = &contacts_db.contacts[contact_idx];
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🧪 SELF-TEST: Sending message to [%d] %s...", contact_idx, c->name);
    
    // Create test message - server will encrypt for receiver!
    const char *test_msg = "Hello from ESP32!";
    int msg_len = strlen(test_msg);
    
    ESP_LOGI(TAG, "   Message: \"%s\" (%d bytes)", test_msg, msg_len);
    
    // Build SEND command
    // Format: corrId | entityId (senderId) | "SEND " | flags | msgBody
    // Server encrypts msgBody with receiver's DH key!
    uint8_t send_body[256];
    int pos = 0;
    
    // CorrId
    send_body[pos++] = 1;
    send_body[pos++] = 'S';
    
    // EntityId = senderId
    send_body[pos++] = c->sender_id_len;
    memcpy(&send_body[pos], c->sender_id, c->sender_id_len);
    pos += c->sender_id_len;
    
    // Command: "SEND "
    send_body[pos++] = 'S';
    send_body[pos++] = 'E';
    send_body[pos++] = 'N';
    send_body[pos++] = 'D';
    send_body[pos++] = ' ';
    
    // Flags = 'F' (no notification) - ASCII not binary!
    send_body[pos++] = 'F';
    
    // Space after flags
    send_body[pos++] = ' ';
    
    // MsgBody - plaintext! Server encrypts for us.
    memcpy(&send_body[pos], test_msg, msg_len);
    pos += msg_len;
    
    ESP_LOGI(TAG, "   SEND body: %d bytes", pos);
    
    // SEND uses senderId for auth (no signature)
    uint8_t transmission[256];
    int tpos = 0;
    
    // No signature (auth length = 0)
    transmission[tpos++] = 0;
    
    // SessionId
    transmission[tpos++] = 32;
    memcpy(&transmission[tpos], session_id, 32);
    tpos += 32;
    
    // Body
    memcpy(&transmission[tpos], send_body, pos);
    tpos += pos;
    
    ESP_LOGI(TAG, "   Transmission: %d bytes", tpos);
    
    // Send
    int ret = smp_write_command_block(ssl, block, transmission, tpos);
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ Failed to send SEND");
        return;
    }
    
    ESP_LOGI(TAG, "   📤 SEND command sent!");
    ESP_LOGI(TAG, "   Waiting for MSG echo...");
}

static int add_contact(mbedtls_ssl_context *ssl, uint8_t *block, 
                       const uint8_t *session_id, const char *name) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        ESP_LOGE(TAG, "❌ No free contact slot! Max %d contacts.", MAX_CONTACTS);
        return -1;
    }
    
    contact_t *c = &contacts_db.contacts[slot];
    memset(c, 0, sizeof(contact_t));
    strncpy(c->name, name, sizeof(c->name) - 1);
    
    ESP_LOGI(TAG, "➕ Creating contact '%s' in slot %d...", name, slot);
    
    // Generate Ed25519 keypair for authentication
    uint8_t seed[32];
    esp_fill_random(seed, 32);
    crypto_sign_seed_keypair(c->rcv_auth_public, c->rcv_auth_secret, seed);
    
    // Generate X25519 keypair for DH
    esp_fill_random(c->rcv_dh_secret, 32);
    crypto_scalarmult_base(c->rcv_dh_public, c->rcv_dh_secret);
    
    ESP_LOGI(TAG, "      Keys generated!");
    
    // Build SPKI-encoded keys
    uint8_t rcv_auth_spki[SPKI_KEY_SIZE];
    memcpy(rcv_auth_spki, ED25519_SPKI_HEADER, 12);
    memcpy(rcv_auth_spki + 12, c->rcv_auth_public, 32);
    
    uint8_t rcv_dh_spki[SPKI_KEY_SIZE];
    memcpy(rcv_dh_spki, X25519_SPKI_HEADER, 12);
    memcpy(rcv_dh_spki + 12, c->rcv_dh_public, 32);
    
    // Build NEW command body
    uint8_t trans_body[256];
    int pos = 0;
    
    // CorrId = slot number as single char
    trans_body[pos++] = 1;
    trans_body[pos++] = '0' + slot;
    
    // EntityId = empty for NEW
    trans_body[pos++] = 0;
    
    // Command: "NEW "
    trans_body[pos++] = 'N';
    trans_body[pos++] = 'E';
    trans_body[pos++] = 'W';
    trans_body[pos++] = ' ';
    
    // rcvAuthKey (SPKI encoded)
    trans_body[pos++] = SPKI_KEY_SIZE;
    memcpy(&trans_body[pos], rcv_auth_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;
    
    // rcvDhKey (SPKI encoded)
    trans_body[pos++] = SPKI_KEY_SIZE;
    memcpy(&trans_body[pos], rcv_dh_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;
    
    // subMode = 'S' (subscribe)
    trans_body[pos++] = 'S';
    
    int trans_body_len = pos;
    
    // Sign: sessionId + transBody
    uint8_t to_sign[1 + 32 + 256];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;  // sessionId length
    memcpy(&to_sign[sign_pos], session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
    sign_pos += trans_body_len;
    
    uint8_t signature[crypto_sign_BYTES];
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, c->rcv_auth_secret);
    
    // Build full transmission
    uint8_t transmission[256];
    int tpos = 0;
    
    // Signature
    transmission[tpos++] = crypto_sign_BYTES;
    memcpy(&transmission[tpos], signature, crypto_sign_BYTES);
    tpos += crypto_sign_BYTES;
    
    // SessionId
    transmission[tpos++] = 32;
    memcpy(&transmission[tpos], session_id, 32);
    tpos += 32;
    
    // TransBody
    memcpy(&transmission[tpos], trans_body, trans_body_len);
    tpos += trans_body_len;
    
    // Send NEW command
    ESP_LOGI(TAG, "      Sending NEW command...");
    int ret = smp_write_command_block(ssl, block, transmission, tpos);
    if (ret != 0) {
        ESP_LOGE(TAG, "      ❌ Failed to send NEW");
        return -1;
    }
    
    // Wait for IDS response
    int content_len = smp_read_block(ssl, block, 15000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "      ❌ No response to NEW");
        return -1;
    }
    
    uint8_t *resp = block + 2;
    
    // Parse response - look for IDS or ERR
    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'I' && resp[i+1] == 'D' && resp[i+2] == 'S' && resp[i+3] == ' ') {
            int p = i + 4;
            
            // rcvId (length-prefixed)
            if (p < content_len) {
                c->recipient_id_len = resp[p++];
                if (c->recipient_id_len > 24) c->recipient_id_len = 24;
                if (p + c->recipient_id_len <= content_len) {
                    memcpy(c->recipient_id, &resp[p], c->recipient_id_len);
                    p += c->recipient_id_len;
                }
            }
            
            // sndId (length-prefixed)
            if (p < content_len) {
                c->sender_id_len = resp[p++];
                if (c->sender_id_len > 24) c->sender_id_len = 24;
                if (p + c->sender_id_len <= content_len) {
                    memcpy(c->sender_id, &resp[p], c->sender_id_len);
                    p += c->sender_id_len;
                }
            }
            
            // srvDhKey (length-prefixed, SPKI encoded - 44 bytes)
            if (p < content_len) {
                uint8_t srv_dh_len = resp[p++];
                if (srv_dh_len == 44 && p + srv_dh_len <= content_len) {
                    // Extract raw 32-byte key from SPKI (skip 12-byte header)
                    memcpy(c->srv_dh_public, &resp[p + 12], 32);
                    c->have_srv_dh = 1;
                }
            }
            
            c->active = 1;
            contacts_db.num_contacts++;
            save_contacts_to_nvs();
            
            ESP_LOGI(TAG, "      ✅ Contact '%s' created!", name);
            ESP_LOGI(TAG, "      rcvId: %02x%02x%02x%02x%02x%02x...",
                     c->recipient_id[0], c->recipient_id[1], c->recipient_id[2],
                     c->recipient_id[3], c->recipient_id[4], c->recipient_id[5]);
            return slot;
        }
        
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R') {
            ESP_LOGE(TAG, "      ❌ Server error creating contact");
            // Log error details
            ESP_LOGE(TAG, "      Error: %.*s", 
                     (content_len - i > 20) ? 20 : content_len - i, &resp[i]);
            return -1;
        }
    }
    
    ESP_LOGE(TAG, "      ❌ Unexpected response");
    return -1;
}

static bool remove_contact(mbedtls_ssl_context *ssl, uint8_t *block,
                           const uint8_t *session_id, int index) {
    if (index < 0 || index >= MAX_CONTACTS || !contacts_db.contacts[index].active) {
        ESP_LOGE(TAG, "❌ Invalid contact index: %d", index);
        return false;
    }
    
    contact_t *c = &contacts_db.contacts[index];
    ESP_LOGI(TAG, "🗑️  Removing contact '%s' [%d]...", c->name, index);
    
    // Build DEL command body
    uint8_t del_body[64];
    int dp = 0;
    
    // CorrId
    del_body[dp++] = 1;
    del_body[dp++] = 'D';
    
    // EntityId = recipientId
    del_body[dp++] = c->recipient_id_len;
    memcpy(&del_body[dp], c->recipient_id, c->recipient_id_len);
    dp += c->recipient_id_len;
    
    // Command: "DEL"
    del_body[dp++] = 'D';
    del_body[dp++] = 'E';
    del_body[dp++] = 'L';
    
    // Sign
    uint8_t del_to_sign[128];
    int dsp = 0;
    del_to_sign[dsp++] = 32;
    memcpy(&del_to_sign[dsp], session_id, 32);
    dsp += 32;
    memcpy(&del_to_sign[dsp], del_body, dp);
    dsp += dp;
    
    uint8_t del_sig[crypto_sign_BYTES];
    crypto_sign_detached(del_sig, NULL, del_to_sign, dsp, c->rcv_auth_secret);
    
    // Build transmission
    uint8_t del_trans[192];
    int dtp = 0;
    
    del_trans[dtp++] = crypto_sign_BYTES;
    memcpy(&del_trans[dtp], del_sig, crypto_sign_BYTES);
    dtp += crypto_sign_BYTES;
    
    del_trans[dtp++] = 32;
    memcpy(&del_trans[dtp], session_id, 32);
    dtp += 32;
    
    memcpy(&del_trans[dtp], del_body, dp);
    dtp += dp;
    
    int ret = smp_write_command_block(ssl, block, del_trans, dtp);
    if (ret != 0) {
        ESP_LOGE(TAG, "      ❌ Failed to send DEL");
        return false;
    }
    
    int content_len = smp_read_block(ssl, block, 5000);
    if (content_len >= 0) {
        uint8_t *resp = block + 2;
        
        // Parse transport format to find OK
        int rp = 0;
        if (resp[rp] == 1) {
            rp++;  // txCount
            rp += 2;  // txLen
            int rauthLen = resp[rp++]; rp += rauthLen;  // auth
            int rsessLen = resp[rp++]; rp += rsessLen;  // sessId
            int rcorrLen = resp[rp++]; rp += rcorrLen;  // corrId
            int rentLen = resp[rp++]; rp += rentLen;    // entityId
            
            if (rp + 1 < content_len && resp[rp] == 'O' && resp[rp+1] == 'K') {
                c->active = 0;
                memset(c, 0, sizeof(contact_t));
                contacts_db.num_contacts--;
                save_contacts_to_nvs();
                ESP_LOGI(TAG, "      ✅ Contact removed from server and NVS!");
                return true;
            }
        }
    }
    
    ESP_LOGE(TAG, "      ❌ Failed to remove contact from server");
    return false;
}

// ============== Multi-SUB ==============

static void subscribe_all_contacts(mbedtls_ssl_context *ssl, uint8_t *block,
                                   const uint8_t *session_id) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📡 Subscribing to all contacts...");
    
    int success_count = 0;
    
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        
        contact_t *c = &contacts_db.contacts[i];
        ESP_LOGI(TAG, "   [%d] %s...", i, c->name);
        
        // Build SUB command body
        uint8_t sub_body[64];
        int pos = 0;
        
        // CorrId = slot number
        sub_body[pos++] = 1;
        sub_body[pos++] = '0' + i;
        
        // EntityId = recipientId
        sub_body[pos++] = c->recipient_id_len;
        memcpy(&sub_body[pos], c->recipient_id, c->recipient_id_len);
        pos += c->recipient_id_len;
        
        // Command: "SUB"
        sub_body[pos++] = 'S';
        sub_body[pos++] = 'U';
        sub_body[pos++] = 'B';
        
        int sub_body_len = pos;
        
        // Sign
        uint8_t sub_to_sign[1 + 32 + 64];
        int sub_sign_pos = 0;
        sub_to_sign[sub_sign_pos++] = 32;
        memcpy(&sub_to_sign[sub_sign_pos], session_id, 32);
        sub_sign_pos += 32;
        memcpy(&sub_to_sign[sub_sign_pos], sub_body, sub_body_len);
        sub_sign_pos += sub_body_len;
        
        uint8_t sub_sig[crypto_sign_BYTES];
        crypto_sign_detached(sub_sig, NULL, sub_to_sign, sub_sign_pos, c->rcv_auth_secret);
        
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
        
        int ret = smp_write_command_block(ssl, block, sub_trans, sub_tpos);
        if (ret != 0) {
            ESP_LOGE(TAG, "       ❌ Send failed");
            continue;
        }
        
        int content_len = smp_read_block(ssl, block, 5000);
        if (content_len >= 0) {
            uint8_t *resp = block + 2;
            int rp = 0;
            
            if (resp[rp] == 1) {
                rp++;  // txCount
                rp += 2;  // txLen
                int rauthLen = resp[rp++]; rp += rauthLen;
                int rsessLen = resp[rp++]; rp += rsessLen;
                int rcorrLen = resp[rp++]; rp += rcorrLen;
                int rentLen = resp[rp++]; rp += rentLen;
                
                if (rp + 1 < content_len && resp[rp] == 'O' && resp[rp+1] == 'K') {
                    ESP_LOGI(TAG, "       ✅ Subscribed!");
                    success_count++;
                } else if (rp + 2 < content_len && resp[rp] == 'E' && resp[rp+1] == 'R' && resp[rp+2] == 'R') {
                    ESP_LOGW(TAG, "       ⚠️ Server error");
                } else {
                    ESP_LOGW(TAG, "       ⚠️ Unknown response");
                }
            }
        } else {
            ESP_LOGW(TAG, "       ⚠️ No response");
        }
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📡 Subscriptions complete: %d/%d", success_count, contacts_db.num_contacts);
    ESP_LOGI(TAG, "");
}

// ============== Message Decryption ==============

static bool decrypt_message(contact_t *c, const uint8_t *encrypted, int enc_len,
                           const uint8_t *nonce, uint8_t nonce_len,
                           uint8_t *plain, int *plain_len) {
    if (!c || !c->have_srv_dh || enc_len <= crypto_box_MACBYTES) {
        return false;
    }
    
    // Compute shared secret using crypto_box_beforenm
    // This does X25519 + HSalsa20 key derivation (not just raw X25519!)
    uint8_t shared[crypto_box_BEFORENMBYTES];
    if (crypto_box_beforenm(shared, c->srv_dh_public, c->rcv_dh_secret) != 0) {
        ESP_LOGE(TAG, "      DH key computation failed");
        return false;
    }
    
    // Prepare nonce (24 bytes, zero-padded if needed)
    uint8_t full_nonce[crypto_box_NONCEBYTES];
    memset(full_nonce, 0, crypto_box_NONCEBYTES);
    int copy_len = (nonce_len < crypto_box_NONCEBYTES) ? nonce_len : crypto_box_NONCEBYTES;
    memcpy(full_nonce, nonce, copy_len);
    
    // Decrypt using crypto_box (XSalsa20 + Poly1305)
    if (crypto_box_open_easy_afternm(plain, encrypted, enc_len, full_nonce, shared) != 0) {
        ESP_LOGE(TAG, "      Decryption failed");
        return false;
    }
    
    *plain_len = enc_len - crypto_box_MACBYTES;
    return true;
}

// ============== Agent Message Parsing ==============

// Agent message types (after SMP-level decryption)
#define AGENT_MSG_CONFIRMATION  'C'  // Connection request from peer
#define AGENT_MSG_ENVELOPE      'M'  // Normal message (Double Ratchet encrypted)
#define AGENT_MSG_INVITATION    'I'  // Invitation
#define AGENT_MSG_RATCHET_KEY   'R'  // Ratchet key exchange

// Parse SMP client message header
// Returns pointer to body after header, or NULL on error
static const uint8_t* parse_smp_client_header(const uint8_t *data, int len, 
                                              uint8_t *sender_key, int *sender_key_len) {
    if (len < 1) return NULL;
    
    *sender_key_len = 0;
    
    if (data[0] == '_') {
        // Empty header - message from secured queue
        return data + 1;
    } else if (data[0] == 'K') {
        // Header with sender key - initial confirmation
        if (len < 2) return NULL;
        int key_len = data[1];
        if (len < 2 + key_len) return NULL;
        if (sender_key && key_len <= 64) {
            memcpy(sender_key, data + 2, key_len);
            *sender_key_len = key_len;
        }
        return data + 2 + key_len;
    }
    
    // Unknown header - might be raw agent message
    return data;
}

// Parse agent message envelope
// Returns the message type and fills in version
static char parse_agent_envelope(const uint8_t *data, int len, 
                                 uint16_t *agent_version,
                                 const uint8_t **body, int *body_len) {
    if (len < 3) return 0;
    
    // Agent version is 2 bytes, big-endian
    *agent_version = (data[0] << 8) | data[1];
    
    // Message type
    char msg_type = data[2];
    
    // Body starts after type
    *body = data + 3;
    *body_len = len - 3;
    
    return msg_type;
}

// Dump hex for debugging
static void dump_hex(const char *label, const uint8_t *data, int len, int max_bytes) {
    int show = (len > max_bytes) ? max_bytes : len;
    printf("   %s (%d bytes): ", label, len);
    for (int i = 0; i < show; i++) {
        printf("%02x ", data[i]);
    }
    if (len > max_bytes) printf("...");
    printf("\n");
}

// Parse and display agent message details
static void parse_agent_message(contact_t *contact, const uint8_t *plain, int plain_len) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "   🔬 AGENT MESSAGE ANALYSIS:");
    
    // First, check for SMP client header
    uint8_t sender_key[64];
    int sender_key_len = 0;
    const uint8_t *body = parse_smp_client_header(plain, plain_len, sender_key, &sender_key_len);
    
    if (!body) {
        ESP_LOGW(TAG, "      ⚠️ Failed to parse SMP client header");
        dump_hex("Raw data", plain, plain_len, 32);
        return;
    }
    
    int body_offset = body - plain;
    int body_len = plain_len - body_offset;
    
    if (sender_key_len > 0) {
        ESP_LOGI(TAG, "      📌 Header: 'K' (initial confirmation)");
        dump_hex("Sender key", sender_key, sender_key_len, 44);
    } else if (plain[0] == '_') {
        ESP_LOGI(TAG, "      📌 Header: '_' (secured queue)");
    } else {
        ESP_LOGI(TAG, "      📌 Header: none (raw agent msg)");
    }
    
    // Parse agent envelope
    if (body_len < 3) {
        ESP_LOGW(TAG, "      ⚠️ Body too short for agent envelope");
        dump_hex("Body", body, body_len, 32);
        return;
    }
    
    uint16_t agent_version;
    const uint8_t *agent_body;
    int agent_body_len;
    char msg_type = parse_agent_envelope(body, body_len, &agent_version, &agent_body, &agent_body_len);
    
    ESP_LOGI(TAG, "      📦 Agent Version: %d", agent_version);
    
    switch (msg_type) {
        case AGENT_MSG_CONFIRMATION:
            ESP_LOGI(TAG, "      📬 Type: CONFIRMATION ('C') - Connection Request!");
            ESP_LOGI(TAG, "      ────────────────────────────────────");
            ESP_LOGI(TAG, "      🎉 A SimpleX client wants to connect!");
            
            // Parse confirmation: "0" or "1" + e2eParams + encConnInfo
            if (agent_body_len > 0) {
                char e2e_flag = agent_body[0];
                ESP_LOGI(TAG, "      E2E flag: '%c' (%s)", e2e_flag,
                         e2e_flag == '0' ? "no e2e params" : "has e2e params");
                
                // The rest is the encrypted connection info
                // This contains the peer's profile, reply queue, etc.
                dump_hex("encConnInfo", agent_body + 1, agent_body_len - 1, 64);
                
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "      ⚡ NEXT STEPS:");
                ESP_LOGI(TAG, "         1. Parse e2e encryption params");
                ESP_LOGI(TAG, "         2. Decrypt connInfo with our DH key");
                ESP_LOGI(TAG, "         3. Extract peer's reply queue & profile");
                ESP_LOGI(TAG, "         4. Send our confirmation back");
                ESP_LOGI(TAG, "         5. Initialize Double Ratchet");
            }
            break;
            
        case AGENT_MSG_ENVELOPE:
            ESP_LOGI(TAG, "      📬 Type: MESSAGE ENVELOPE ('M')");
            ESP_LOGI(TAG, "      ⚠️ This is Double Ratchet encrypted!");
            ESP_LOGI(TAG, "      Need to implement ratchet to decrypt.");
            dump_hex("encAgentMessage", agent_body, agent_body_len, 64);
            break;
            
        case AGENT_MSG_INVITATION:
            ESP_LOGI(TAG, "      📬 Type: INVITATION ('I')");
            // Format: connReqLength (2 bytes) + connReq + connInfo
            if (agent_body_len >= 2) {
                uint16_t req_len = (agent_body[0] << 8) | agent_body[1];
                ESP_LOGI(TAG, "      ConnReq length: %d", req_len);
                dump_hex("connReq", agent_body + 2, (req_len < agent_body_len - 2) ? req_len : agent_body_len - 2, 64);
            }
            break;
            
        case AGENT_MSG_RATCHET_KEY:
            ESP_LOGI(TAG, "      📬 Type: RATCHET KEY ('R')");
            ESP_LOGI(TAG, "      Peer is sending ratchet key exchange.");
            dump_hex("ratchetInfo", agent_body, agent_body_len, 64);
            break;
            
        default:
            ESP_LOGW(TAG, "      📬 Type: UNKNOWN (0x%02x / '%c')", msg_type, 
                     (msg_type >= 32 && msg_type < 127) ? msg_type : '?');
            dump_hex("Body", body, body_len, 64);
            
            // Try to show as printable string
            ESP_LOGI(TAG, "      As text: ");
            printf("         \"");
            for (int i = 0; i < body_len && i < 100; i++) {
                char c = body[i];
                if (c >= 32 && c < 127) printf("%c", c);
                else printf(".");
            }
            printf("\"\n");
            break;
    }
    
    ESP_LOGI(TAG, "");
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

    uint8_t *block = (uint8_t *)heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "Failed to allocate buffer!");
        return;
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  SimpleGo v0.1.11-alpha Agent-Test ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════╝");
    ESP_LOGI(TAG, "");

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
    if (ret != 0) goto cleanup;

    // ========== Step 1: TCP + TLS ==========
    ESP_LOGI(TAG, "[1/5] Connecting to %s:%d...", SMP_HOST, SMP_PORT);
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
            ESP_LOGE(TAG, "      TLS failed: -0x%04X", -ret);
            goto cleanup;
        }
    }
    ESP_LOGI(TAG, "      ✅ TLS OK! ALPN: %s", mbedtls_ssl_get_alpn_protocol(&ssl));

    // ========== Step 2: ServerHello ==========
    ESP_LOGI(TAG, "[2/5] Waiting for ServerHello...");
    int content_len = smp_read_block(&ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "      ❌ No ServerHello");
        goto cleanup;
    }

    uint8_t *hello = block + 2;
    uint16_t minVer = (hello[0] << 8) | hello[1];
    uint16_t maxVer = (hello[2] << 8) | hello[3];
    uint8_t sessIdLen = hello[4];
    
    if (sessIdLen != 32) {
        ESP_LOGE(TAG, "      ❌ Unexpected sessionId length: %d", sessIdLen);
        goto cleanup;
    }
    memcpy(session_id, &hello[5], 32);
    
    ESP_LOGI(TAG, "      ✅ Versions: %d-%d", minVer, maxVer);
    ESP_LOGI(TAG, "      SessionId: %02x%02x%02x%02x...", 
             session_id[0], session_id[1], session_id[2], session_id[3]);

    // ========== Step 3: ClientHello ==========
    ESP_LOGI(TAG, "[3/5] Sending ClientHello...");
    
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);
    
    // keyHash = SHA256 of CA cert (second cert in chain)
    uint8_t ca_hash[32];
    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
    }
    
    // ClientHello: version + keyHash
    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;  // v6
    client_hello[pos++] = 32;    // keyHash length
    memcpy(&client_hello[pos], ca_hash, 32);
    pos += 32;
    
    ret = smp_write_handshake_block(&ssl, block, client_hello, pos);
    if (ret != 0) goto cleanup;
    ESP_LOGI(TAG, "      ✅ ClientHello sent!");

    // ========== Step 4: Load or Create Contacts ==========
    ESP_LOGI(TAG, "[4/5] Loading contacts...");
    load_contacts_from_nvs();
    
    if (contacts_db.num_contacts == 0) {
        ESP_LOGI(TAG, "      No contacts found - creating default...");
        int idx = add_contact(&ssl, block, session_id, "Test");
        if (idx < 0) {
            ESP_LOGE(TAG, "      ❌ Failed to create contact!");
            goto cleanup;
        }
    } else {
        ESP_LOGI(TAG, "      ✅ %d contact(s) loaded from NVS", contacts_db.num_contacts);
    }
    
    list_contacts();
    
    // ========== Step 5: Subscribe All Contacts ==========
    ESP_LOGI(TAG, "[5/5] Subscribing to queues...");
    subscribe_all_contacts(&ssl, block, session_id);
    
    // Print connection info
    print_invitation_links(ca_hash);
    
    // ========== Message Receive Loop ==========
    ESP_LOGI(TAG, "╔════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   📨 Waiting for messages...       ║");
    ESP_LOGI(TAG, "║   (Connect with SimpleX App!)      ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    while (1) {
        content_len = smp_read_block(&ssl, block, 60000);  // 60s timeout
        
        if (content_len == -2) {
            // Timeout - still waiting
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
        p++;  // txCount
        p += 2;  // txLen
        
        // Skip auth header
        int authLen = resp[p++]; p += authLen;
        int sessLen = resp[p++]; p += sessLen;
        int corrLen = resp[p++]; p += corrLen;
        
        // EntityId - tells us which contact this message is for
        int entLen = resp[p++];
        uint8_t entity_id[24];
        if (entLen > 24) entLen = 24;
        memcpy(entity_id, &resp[p], entLen);
        p += entLen;
        
        // Find which contact this is for
        int contact_idx = find_contact_by_recipient_id(entity_id, entLen);
        contact_t *contact = (contact_idx >= 0) ? &contacts_db.contacts[contact_idx] : NULL;
        
        // Parse command
        if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
            ESP_LOGI(TAG, "   ✅ OK");
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'N' && resp[p+2] == 'D') {
            if (contact) {
                ESP_LOGI(TAG, "   🔚 END [%s] - No more messages", contact->name);
            } else {
                ESP_LOGI(TAG, "   🔚 END - No more messages");
            }
        }
        else if (p + 3 < content_len && resp[p] == 'M' && resp[p+1] == 'S' && resp[p+2] == 'G' && resp[p+3] == ' ') {
            p += 4;  // Skip "MSG "
            
            // Parse msgId (nonce for decryption)
            uint8_t msgIdLen = resp[p++];
            uint8_t msg_id[24];
            memset(msg_id, 0, 24);
            if (msgIdLen > 24) msgIdLen = 24;
            memcpy(msg_id, &resp[p], msgIdLen);
            p += msgIdLen;
            
            // Rest is encrypted body
            int enc_len = content_len - p;
            
            if (contact) {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
                ESP_LOGI(TAG, "║   💬 MESSAGE RECEIVED for [%s]!", contact->name);
                ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
            } else {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "   💬 MESSAGE (unknown contact)!");
            }
            ESP_LOGI(TAG, "   MsgId: %02x%02x%02x%02x...", msg_id[0], msg_id[1], msg_id[2], msg_id[3]);
            ESP_LOGI(TAG, "   Encrypted: %d bytes", enc_len);
            
            // Try to decrypt
            if (contact && contact->have_srv_dh && enc_len > crypto_secretbox_MACBYTES) {
                uint8_t *plain = malloc(enc_len);
                if (plain) {
                    int plain_len = 0;
                    if (decrypt_message(contact, &resp[p], enc_len, msg_id, msgIdLen, plain, &plain_len)) {
                        ESP_LOGI(TAG, "   🔓 SMP-Level Decryption OK! (%d bytes)", plain_len);
                        
                        // Parse the agent message to understand what we got
                        parse_agent_message(contact, plain, plain_len);
                        
                        // Send ACK
                        ESP_LOGI(TAG, "   📨 Sending ACK...");
                        
                        uint8_t ack_body[64];
                        int ap = 0;
                        ack_body[ap++] = 1;  // corrId len
                        ack_body[ap++] = 'A';  // corrId
                        ack_body[ap++] = contact->recipient_id_len;  // entityId
                        memcpy(&ack_body[ap], contact->recipient_id, contact->recipient_id_len);
                        ap += contact->recipient_id_len;
                        ack_body[ap++] = 'A';
                        ack_body[ap++] = 'C';
                        ack_body[ap++] = 'K';
                        ack_body[ap++] = ' ';
                        ack_body[ap++] = msgIdLen;
                        memcpy(&ack_body[ap], msg_id, msgIdLen);
                        ap += msgIdLen;
                        
                        // Sign ACK
                        uint8_t ack_to_sign[128];
                        int ack_sign_pos = 0;
                        ack_to_sign[ack_sign_pos++] = 32;
                        memcpy(&ack_to_sign[ack_sign_pos], session_id, 32);
                        ack_sign_pos += 32;
                        memcpy(&ack_to_sign[ack_sign_pos], ack_body, ap);
                        ack_sign_pos += ap;
                        
                        uint8_t ack_sig[crypto_sign_BYTES];
                        crypto_sign_detached(ack_sig, NULL, ack_to_sign, ack_sign_pos, contact->rcv_auth_secret);
                        
                        // Build ACK transmission
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
                        ESP_LOGE(TAG, "   ❌ Decryption failed!");
                    }
                    free(plain);
                }
            } else {
                ESP_LOGW(TAG, "      ⚠️ Cannot decrypt - no contact keys");
            }
            ESP_LOGI(TAG, "");
        }
        else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
            ESP_LOGE(TAG, "   ❌ ERR: %.*s", 
                     (content_len - p > 20) ? 20 : content_len - p, &resp[p]);
        }
        else {
            // Unknown command
            ESP_LOGW(TAG, "   ❓ Unknown: %c%c%c", 
                     (p < content_len) ? resp[p] : '?',
                     (p+1 < content_len) ? resp[p+1] : '?',
                     (p+2 < content_len) ? resp[p+2] : '?');
        }
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔════════════════════════════════════╗");
    ESP_LOGI(TAG, "║       Session ended                ║");
    ESP_LOGI(TAG, "╚════════════════════════════════════╝");

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
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "SimpleGo starting...");
    
    // Initialize libsodium
    if (sodium_init() < 0) {
        ESP_LOGE(TAG, "libsodium init failed!");
        return;
    }
    ESP_LOGI(TAG, "libsodium initialized");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");

    // Initialize WiFi
    wifi_init();

    // Wait for WiFi connection
    ESP_LOGI(TAG, "Waiting for WiFi...");
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Connect to SMP server
    smp_connect();

    ESP_LOGI(TAG, "Done!");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}