/**
 * SimpleGo - smp_queue.c
 * SMP Queue Management (NEW, SUB, ACK commands)
 * v0.1.15-alpha - FIXED: NEW command now properly signed!
 */

#include "smp_queue.h"
#include "smp_types.h"
#include "smp_network.h"
#include "smp_crypto.h"
#include <string.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"
#include "sodium.h"

static const char *TAG = "SMP_QUEUE";

// Global queue instance
our_queue_t our_queue = {0};

// Internal connection state for our queue server
static struct {
    int sock;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    uint8_t session_id[32];
    bool connected;
    bool initialized;
} queue_conn = {0};

// ============== Queue Connection ==============

static bool queue_connect(const char *host, int port) {
    int ret;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—");
    ESP_LOGI(TAG, "â•‘  ðŸ“¦ CREATING OUR RECEIVE QUEUE                               â•‘");
    ESP_LOGI(TAG, "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "ðŸ”— CONNECTING TO OUR SMP SERVER...");
    ESP_LOGI(TAG, "   Host: %s:%d", host, port);
    
    // Store server info
    strncpy(our_queue.server_host, host, sizeof(our_queue.server_host) - 1);
    our_queue.server_port = port;
    
    // Initialize mbedTLS
    mbedtls_ssl_init(&queue_conn.ssl);
    mbedtls_ssl_config_init(&queue_conn.conf);
    mbedtls_entropy_init(&queue_conn.entropy);
    mbedtls_ctr_drbg_init(&queue_conn.ctr_drbg);
    queue_conn.initialized = true;
    
    ret = mbedtls_ctr_drbg_seed(&queue_conn.ctr_drbg, mbedtls_entropy_func, 
                                 &queue_conn.entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "   âŒ DRBG seed failed");
        return false;
    }
    
    // TCP connect
    ESP_LOGI(TAG, "   Attempting TCP connect...");
    queue_conn.sock = smp_tcp_connect(host, port);
    ESP_LOGI(TAG, "   TCP result: %d", queue_conn.sock);
    if (queue_conn.sock < 0) {
        ESP_LOGE(TAG, "   âŒ TCP connect failed");
        return false;
    }
    
    // TLS setup
    ESP_LOGI(TAG, "   Setting TLS defaults...");
    ret = mbedtls_ssl_config_defaults(&queue_conn.conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ mbedTLS failed: -0x%04X", -ret);
        close(queue_conn.sock);
        return false;
    }
    
    mbedtls_ssl_conf_min_tls_version(&queue_conn.conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&queue_conn.conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&queue_conn.conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&queue_conn.conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&queue_conn.conf, mbedtls_ctr_drbg_random, &queue_conn.ctr_drbg);
    
    static const char *alpn_list[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&queue_conn.conf, alpn_list);
    
    ESP_LOGI(TAG, "   SSL setup...");
    ret = mbedtls_ssl_setup(&queue_conn.ssl, &queue_conn.conf);
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ mbedTLS failed: -0x%04X", -ret);
        close(queue_conn.sock);
        return false;
    }
    
    mbedtls_ssl_set_hostname(&queue_conn.ssl, host);
    mbedtls_ssl_set_bio(&queue_conn.ssl, &queue_conn.sock, my_send_cb, my_recv_cb, NULL);
    
    // TLS handshake
    ESP_LOGI(TAG, "   Starting TLS handshake...");
    while ((ret = mbedtls_ssl_handshake(&queue_conn.ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "   âŒ TLS handshake failed: -0x%04X", -ret);
            close(queue_conn.sock);
            return false;
        }
    }
    ESP_LOGI(TAG, "   âœ… TLS OK!");
    
    // Allocate block buffer
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "   âŒ Block alloc failed");
        return false;
    }
    
    // Wait for ServerHello
    int content_len = smp_read_block(&queue_conn.ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   âŒ No ServerHello");
        free(block);
        return false;
    }
    
    uint8_t *hello = block + 2;
    uint8_t sess_id_len = hello[4];
    if (sess_id_len != 32) {
        ESP_LOGE(TAG, "   âŒ Bad sessionId length");
        free(block);
        return false;
    }
    memcpy(queue_conn.session_id, &hello[5], 32);
    ESP_LOGI(TAG, "   SessionId: %02x%02x%02x%02x...", 
             queue_conn.session_id[0], queue_conn.session_id[1],
             queue_conn.session_id[2], queue_conn.session_id[3]);
    
    // Parse CA cert for keyHash
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);
    
    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, our_queue.server_key_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, our_queue.server_key_hash, 0);
    }
    
    ESP_LOGI(TAG, "   KeyHash: %02x%02x%02x%02x...",
             our_queue.server_key_hash[0], our_queue.server_key_hash[1],
             our_queue.server_key_hash[2], our_queue.server_key_hash[3]);
    
    // Send ClientHello
    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;  // v6
    client_hello[pos++] = 32;
    memcpy(&client_hello[pos], our_queue.server_key_hash, 32);
    pos += 32;
    
    int ret2 = smp_write_handshake_block(&queue_conn.ssl, block, client_hello, pos);
    free(block);
    
    if (ret2 != 0) {
        ESP_LOGE(TAG, "   âŒ ClientHello failed");
        return false;
    }
    
    ESP_LOGI(TAG, "   âœ… SMP Handshake complete!");
    queue_conn.connected = true;
    
    return true;
}

// ============== Create Queue ==============

bool queue_create(const char *host, int port) {
    // Connect to server
    if (!queue_connect(host, port)) {
        return false;
    }
    
    // Generate our keypairs FIRST (needed for signing!)
    ESP_LOGI(TAG, "   ðŸ”‘ Generating keypairs...");
    
    // Ed25519 for command signing (rcvAuthKey)
    crypto_sign_keypair(our_queue.rcv_auth_public, our_queue.rcv_auth_private);
    
    // X25519 for DH (rcvDhKey)
    crypto_box_keypair(our_queue.rcv_dh_public, our_queue.rcv_dh_private);
    
    ESP_LOGI(TAG, "   Auth public: %02x%02x%02x%02x...",
             our_queue.rcv_auth_public[0], our_queue.rcv_auth_public[1],
             our_queue.rcv_auth_public[2], our_queue.rcv_auth_public[3]);
    ESP_LOGI(TAG, "   DH public: %02x%02x%02x%02x...",
             our_queue.rcv_dh_public[0], our_queue.rcv_dh_public[1],
             our_queue.rcv_dh_public[2], our_queue.rcv_dh_public[3]);
    
    /*
     * Build transmission body (what gets signed):
     * [corrIdLen][corrId][entityIdLen=0][command...]
     * 
     * NEW command format:
     * "NEW " + [len]rcvAuthKey(SPKI) + [len]rcvDhKey(SPKI) + subMode
     */
    
    uint8_t trans_body[256];
    int pos = 0;
    
    // CorrId (use simple "1" like smp_contacts.c does)
    trans_body[pos++] = 1;    // corrId length
    trans_body[pos++] = '1';  // corrId value
    
    // EntityId = empty for NEW (new queue has no ID yet)
    trans_body[pos++] = 0;
    
    // Command: "NEW "
    trans_body[pos++] = 'N';
    trans_body[pos++] = 'E';
    trans_body[pos++] = 'W';
    trans_body[pos++] = ' ';
    
    // rcvAuthKey = Ed25519 SPKI (44 bytes: 12 header + 32 key)
    trans_body[pos++] = 44;  // length prefix
    memcpy(&trans_body[pos], ED25519_SPKI_HEADER, 12);
    pos += 12;
    memcpy(&trans_body[pos], our_queue.rcv_auth_public, 32);
    pos += 32;
    
    // rcvDhKey = X25519 SPKI (44 bytes: 12 header + 32 key)
    trans_body[pos++] = 44;  // length prefix
    memcpy(&trans_body[pos], X25519_SPKI_HEADER, 12);
    pos += 12;
    memcpy(&trans_body[pos], our_queue.rcv_dh_public, 32);
    pos += 32;
    
    // subMode = SMSubscribe = 'S' (we want to subscribe immediately)
    trans_body[pos++] = 'S';
    
    int trans_body_len = pos;
    ESP_LOGI(TAG, "   ðŸ“¤ NEW command body: %d bytes", trans_body_len);
    
    /*
     * Sign: smpEncode(sessionId) + transmission_body
     * smpEncode adds length prefix: [0x20][sessionId 32 bytes]
     * 
     * THIS IS THE FIX! We must sign with rcv_auth_private!
     */
    uint8_t to_sign[1 + 32 + 256];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;  // Length prefix for sessionId
    memcpy(&to_sign[sign_pos], queue_conn.session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
    sign_pos += trans_body_len;
    
    // Sign with Ed25519 using our rcvAuthKey private part
    uint8_t signature[crypto_sign_BYTES];  // 64 bytes
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, our_queue.rcv_auth_private);
    
    ESP_LOGI(TAG, "   ðŸ” Signature: %02x%02x%02x%02x...%02x%02x%02x%02x",
             signature[0], signature[1], signature[2], signature[3],
             signature[60], signature[61], signature[62], signature[63]);
    
    // Verify signature locally (sanity check)
    int verify_result = crypto_sign_verify_detached(signature, to_sign, sign_pos, 
                                                     our_queue.rcv_auth_public);
    if (verify_result == 0) {
        ESP_LOGI(TAG, "   âœ… Signature verified locally!");
    } else {
        ESP_LOGE(TAG, "   âŒ Local signature verification FAILED!");
        return false;
    }
    
    /*
     * Build final transmission:
     *   [sigLen][signature 64 bytes]
     *   [sessLen][sessionId 32 bytes]
     *   [transmission_body]
     */
    uint8_t transmission[256];
    int tp = 0;
    
    // Signature WITH LENGTH PREFIX (64 bytes)
    transmission[tp++] = crypto_sign_BYTES;  // 64
    memcpy(&transmission[tp], signature, crypto_sign_BYTES);
    tp += crypto_sign_BYTES;
    
    // SessionId WITH LENGTH PREFIX (32 bytes)
    transmission[tp++] = 32;
    memcpy(&transmission[tp], queue_conn.session_id, 32);
    tp += 32;
    
    // Transmission body (corrId + entityId + command)
    memcpy(&transmission[tp], trans_body, trans_body_len);
    tp += trans_body_len;
    
    ESP_LOGI(TAG, "   ðŸ“¡ Full transmission: %d bytes", tp);
    
    // Debug: print first 20 bytes
    printf("      First 20 bytes: ");
    for (int i = 0; i < 20; i++) {
        printf("%02x ", transmission[i]);
    }
    printf("\n");
    
    // Send command
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) return false;
    
    int ret = smp_write_command_block(&queue_conn.ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   âŒ Send NEW failed!");
        free(block);
        return false;
    }
    
    ESP_LOGI(TAG, "   ðŸ“¤ NEW sent! Waiting for IDS...");
    
    // Wait for IDS response
    int content_len = smp_read_block(&queue_conn.ssl, block, 10000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   âŒ No response");
        free(block);
        return false;
    }
    
    uint8_t *resp = block + 2;
    
    // Debug
    ESP_LOGI(TAG, "   ðŸ“¥ Response (%d bytes):", content_len);
    printf("      ");
    for (int i = 0; i < content_len && i < 80; i++) {
        printf("%02x ", resp[i]);
    }
    printf("\n");
    
    /*
     * Parse response - same format as what we receive in smp_contacts.c
     * Format: [1][txLen:2][authLen][auth...][sessLen][sess...][corrLen][corr...][entLen][ent...][IDS ...]
     */
    
    int p = 0;
    
    // Skip transport wrapper
    if (resp[p] == 1) p++;  // txCount
    p += 2;  // txLen
    
    int authLen = resp[p++];
    p += authLen;
    
    int sessLen = resp[p++];
    p += sessLen;
    
    int corrLen = resp[p++];
    p += corrLen;
    
    int entLen = resp[p++];
    p += entLen;
    
    // Now we should be at the command
    ESP_LOGI(TAG, "   Command at %d: %c%c%c", p, resp[p], resp[p+1], resp[p+2]);
    
    if (resp[p] == 'I' && resp[p+1] == 'D' && resp[p+2] == 'S') {
        p += 3;  // Skip "IDS"
        if (resp[p] == ' ') p++;  // Skip space
        
        /*
         * Parse IDS response (same as smp_contacts.c):
         * [rcvIdLen][rcvId...][sndIdLen][sndId...][dhKeyLen][dhKey(SPKI)...]...
         */
        
        // rcvId (length-prefixed)
        our_queue.rcv_id_len = resp[p++];
        if (our_queue.rcv_id_len > QUEUE_ID_SIZE) {
            ESP_LOGE(TAG, "   âŒ rcvId too long: %d", our_queue.rcv_id_len);
            free(block);
            return false;
        }
        memcpy(our_queue.rcv_id, &resp[p], our_queue.rcv_id_len);
        p += our_queue.rcv_id_len;
        
        // sndId (length-prefixed)
        our_queue.snd_id_len = resp[p++];
        if (our_queue.snd_id_len > QUEUE_ID_SIZE) {
            ESP_LOGE(TAG, "   âŒ sndId too long: %d", our_queue.snd_id_len);
            free(block);
            return false;
        }
        memcpy(our_queue.snd_id, &resp[p], our_queue.snd_id_len);
        p += our_queue.snd_id_len;
        
        // rcvPublicDhKey = Server's X25519 SPKI
        int dhLen = resp[p++];
        if (dhLen != 44) {
            ESP_LOGE(TAG, "   âŒ Unexpected DH key length: %d", dhLen);
            free(block);
            return false;
        }
        // Skip SPKI header (12 bytes), copy raw key (32 bytes)
        memcpy(our_queue.srv_dh_public, &resp[p + 12], 32);
        p += dhLen;
        
        // Compute shared secret for message decryption
        if (crypto_scalarmult(our_queue.shared_secret, 
                              our_queue.rcv_dh_private, 
                              our_queue.srv_dh_public) != 0) {
            ESP_LOGE(TAG, "   âŒ DH failed");
            free(block);
            return false;
        }
        
        our_queue.valid = true;
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "â•”â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•—");
        ESP_LOGI(TAG, "â•‘   âœ… QUEUE CREATED SUCCESSFULLY!                             â•‘");
        ESP_LOGI(TAG, "â•šâ•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");
        ESP_LOGI(TAG, "   rcvId (%d): %02x%02x%02x%02x...", 
                 our_queue.rcv_id_len,
                 our_queue.rcv_id[0], our_queue.rcv_id[1],
                 our_queue.rcv_id[2], our_queue.rcv_id[3]);
        ESP_LOGI(TAG, "   sndId (%d): %02x%02x%02x%02x...", 
                 our_queue.snd_id_len,
                 our_queue.snd_id[0], our_queue.snd_id[1],
                 our_queue.snd_id[2], our_queue.snd_id[3]);
        ESP_LOGI(TAG, "   Server DH: %02x%02x%02x%02x...",
                 our_queue.srv_dh_public[0], our_queue.srv_dh_public[1],
                 our_queue.srv_dh_public[2], our_queue.srv_dh_public[3]);
        
        free(block);
        return true;
        
    } else if (resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
        // Print full error message
        printf("      Error: ");
        for (int i = p; i < content_len && i < p + 40; i++) {
            char c = resp[i];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
        ESP_LOGE(TAG, "   âŒ Server error!");
    } else {
        ESP_LOGE(TAG, "   âŒ Unexpected response");
    }
    
    free(block);
    return false;
}

// ============== Encode Queue Info ==============

int queue_encode_info(uint8_t *buf, int max_len) {
    if (!our_queue.valid) {
        ESP_LOGE(TAG, "queue_encode_info: queue not valid!");
        return -1;
    }

    int p = 0;

    // clientVersion = 4 (2 bytes BE)
    buf[p++] = 0x00;
    buf[p++] = 0x04;

    // smpServer: [host_count] [host_len] [host] [port_len] [port] [keyhash_len] [keyhash]
    int host_len = strlen(our_queue.server_host);

    buf[p++] = 0x01;  // 1 host
    buf[p++] = (uint8_t)host_len;
    memcpy(&buf[p], our_queue.server_host, host_len);
    p += host_len;

    // Port as string with LENGTH PREFIX (not space!)
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", our_queue.server_port);
    int port_len = strlen(port_str);
    buf[p++] = (uint8_t)port_len;  // â† FIX: Length prefix!
    memcpy(&buf[p], port_str, port_len);
    p += port_len;

    // KeyHash (32 bytes)
    buf[p++] = 32;
    memcpy(&buf[p], our_queue.server_key_hash, 32);
    p += 32;

    // senderId
    buf[p++] = (uint8_t)our_queue.snd_id_len;
    memcpy(&buf[p], our_queue.snd_id, our_queue.snd_id_len);
    p += our_queue.snd_id_len;

    // dhPublicKey (X25519 SPKI, 44 bytes)
    buf[p++] = 44;
    memcpy(&buf[p], X25519_SPKI_HEADER, 12);
    p += 12;
    memcpy(&buf[p], our_queue.rcv_dh_public, 32);
    p += 32;

    // queueMode = Nothing (Maybe encoding)
    // FÃ¼r clientVersion >= shortLinksSMPClientVersion wird queueMode optional geparst
    // Nothing = kein extra byte ODER 0x00
    // PrÃ¼fe was shortLinksSMPClientVersion ist...

    ESP_LOGI(TAG, "   Encoded SMPQueueInfo: %d bytes", p);
    return p;
}

// ============== Subscribe ==============

bool queue_subscribe(void) {
    if (!queue_conn.connected || !our_queue.valid) {
        ESP_LOGE(TAG, "queue_subscribe: not ready");
        return false;
    }
    
    ESP_LOGI(TAG, "   ðŸ“¥ Subscribing to queue...");
    
    /*
     * Build SUB command with proper signing (same pattern as queue_create)
     */
    
    // Build the unsigned body: corrId + entityId + command
    uint8_t body[128];
    int bp = 0;
    
    // CorrId
    body[bp++] = 1;
    body[bp++] = '2';  // Use different corrId
    
    // EntityId = our rcvId (for SUB we identify the queue)
    body[bp++] = (uint8_t)our_queue.rcv_id_len;
    memcpy(&body[bp], our_queue.rcv_id, our_queue.rcv_id_len);
    bp += our_queue.rcv_id_len;
    
    // Command: "SUB"
    body[bp++] = 'S';
    body[bp++] = 'U';
    body[bp++] = 'B';
    
    // Sign: [sessIdLen][sessId] + body
    uint8_t to_sign[1 + 32 + 128];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;
    memcpy(&to_sign[sign_pos], queue_conn.session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], body, bp);
    sign_pos += bp;
    
    uint8_t signature[crypto_sign_BYTES];
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, our_queue.rcv_auth_private);
    
    // Build transmission with signature
    uint8_t transmission[256];
    int tp = 0;
    
    // Auth = Ed25519 signature (length-prefixed)
    transmission[tp++] = crypto_sign_BYTES;  // 64
    memcpy(&transmission[tp], signature, crypto_sign_BYTES);
    tp += crypto_sign_BYTES;
    
    // SessionId
    transmission[tp++] = 32;
    memcpy(&transmission[tp], queue_conn.session_id, 32);
    tp += 32;
    
    // Body
    memcpy(&transmission[tp], body, bp);
    tp += bp;
    
    // Send
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) return false;
    
    int ret = smp_write_command_block(&queue_conn.ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   âŒ Send SUB failed!");
        free(block);
        return false;
    }
    
    // Wait for response
    int content_len = smp_read_block(&queue_conn.ssl, block, 5000);
    if (content_len >= 0) {
        uint8_t *resp = block + 2;
        
        // Look for OK
        for (int i = 0; i < content_len - 1; i++) {
            if (resp[i] == 'O' && resp[i+1] == 'K') {
                ESP_LOGI(TAG, "   âœ… Subscribed!");
                free(block);
                return true;
            }
        }
        
        ESP_LOGW(TAG, "   âš ï¸ SUB response: %.20s", resp);
    }
    
    free(block);
    return false;
}

// ============== Disconnect ==============

void queue_disconnect(void) {
    if (queue_conn.connected || queue_conn.initialized) {
        mbedtls_ssl_close_notify(&queue_conn.ssl);
        mbedtls_ssl_free(&queue_conn.ssl);
        mbedtls_ssl_config_free(&queue_conn.conf);
        mbedtls_ctr_drbg_free(&queue_conn.ctr_drbg);
        mbedtls_entropy_free(&queue_conn.entropy);
        if (queue_conn.sock >= 0) close(queue_conn.sock);
        queue_conn.connected = false;
        queue_conn.initialized = false;
        ESP_LOGI(TAG, "   ðŸ”Œ Queue connection closed");
    }
}
