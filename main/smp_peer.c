/**
 * SimpleGo - smp_peer.c
 * Peer server connection for AgentConfirmation
 * v0.1.14-alpha - Corrected AgentConfirmation format
 */

#include "smp_peer.h"
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

static const char *TAG = "SMP_PEER";

// Internal peer connection state (full types)
static struct {
    int sock;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    uint8_t session_id[32];
    uint8_t server_key_hash[32];
    bool connected;
    bool initialized;
} peer_state = {0};

// ============== Peer Connection ==============

bool peer_connect(const char *host, int port) {
    int ret;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🔗 CONNECTING TO PEER SERVER...");
    ESP_LOGI(TAG, "   Host: %s:%d", host, port);
    
    // Initialize mbedTLS
    mbedtls_ssl_init(&peer_state.ssl);
    mbedtls_ssl_config_init(&peer_state.conf);
    mbedtls_entropy_init(&peer_state.entropy);
    mbedtls_ctr_drbg_init(&peer_state.ctr_drbg);
    peer_state.initialized = true;
    
    ret = mbedtls_ctr_drbg_seed(&peer_state.ctr_drbg, mbedtls_entropy_func, 
                                 &peer_state.entropy, NULL, 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ DRBG seed failed");
        return false;
    }
    
    // TCP connect
    peer_state.sock = smp_tcp_connect(host, port);
    if (peer_state.sock < 0) {
        ESP_LOGE(TAG, "   ❌ TCP connect failed");
        return false;
    }
    
    // TLS setup
    ret = mbedtls_ssl_config_defaults(&peer_state.conf, MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        close(peer_state.sock);
        return false;
    }
    
    mbedtls_ssl_conf_min_tls_version(&peer_state.conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_max_tls_version(&peer_state.conf, MBEDTLS_SSL_VERSION_TLS1_3);
    mbedtls_ssl_conf_ciphersuites(&peer_state.conf, ciphersuites);
    mbedtls_ssl_conf_authmode(&peer_state.conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&peer_state.conf, mbedtls_ctr_drbg_random, &peer_state.ctr_drbg);
    
    static const char *alpn_list[] = {"smp/1", NULL};
    mbedtls_ssl_conf_alpn_protocols(&peer_state.conf, alpn_list);
    
    ret = mbedtls_ssl_setup(&peer_state.ssl, &peer_state.conf);
    if (ret != 0) {
        close(peer_state.sock);
        return false;
    }
    
    mbedtls_ssl_set_hostname(&peer_state.ssl, host);
    mbedtls_ssl_set_bio(&peer_state.ssl, &peer_state.sock, my_send_cb, my_recv_cb, NULL);
    
    // TLS handshake
    while ((ret = mbedtls_ssl_handshake(&peer_state.ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "   ❌ TLS handshake failed: -0x%04X", -ret);
            close(peer_state.sock);
            return false;
        }
    }
    ESP_LOGI(TAG, "   ✅ TLS OK!");
    
    // Allocate block buffer
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) {
        ESP_LOGE(TAG, "   ❌ Block alloc failed");
        return false;
    }
    
    // Wait for ServerHello
    int content_len = smp_read_block(&peer_state.ssl, block, 30000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   ❌ No ServerHello");
        free(block);
        return false;
    }
    
    uint8_t *hello = block + 2;
    uint8_t sess_id_len = hello[4];
    if (sess_id_len != 32) {
        ESP_LOGE(TAG, "   ❌ Bad sessionId length");
        free(block);
        return false;
    }
    memcpy(peer_state.session_id, &hello[5], 32);
    ESP_LOGI(TAG, "   SessionId: %02x%02x%02x%02x...", 
             peer_state.session_id[0], peer_state.session_id[1],
             peer_state.session_id[2], peer_state.session_id[3]);
    
    // Parse CA cert for keyHash
    int cert1_off, cert1_len, cert2_off, cert2_len;
    parse_cert_chain(hello, content_len, &cert1_off, &cert1_len, &cert2_off, &cert2_len);
    
    if (cert2_off >= 0) {
        mbedtls_sha256(hello + cert2_off, cert2_len, peer_state.server_key_hash, 0);
    } else {
        mbedtls_sha256(hello + cert1_off, cert1_len, peer_state.server_key_hash, 0);
    }
    
    // Send ClientHello
    uint8_t client_hello[35];
    int pos = 0;
    client_hello[pos++] = 0x00;
    client_hello[pos++] = 0x06;  // v6
    client_hello[pos++] = 32;
    memcpy(&client_hello[pos], peer_state.server_key_hash, 32);
    pos += 32;
    
    int ret2 = smp_write_handshake_block(&peer_state.ssl, block, client_hello, pos);
    free(block);
    
    if (ret2 != 0) {
        ESP_LOGE(TAG, "   ❌ ClientHello failed");
        return false;
    }
    
    ESP_LOGI(TAG, "   ✅ SMP Handshake complete!");
    peer_state.connected = true;
    
    // Update global peer_conn for compatibility
    peer_conn.connected = true;
    memcpy(peer_conn.session_id, peer_state.session_id, 32);
    
    return true;
}

void peer_disconnect(void) {
    if (peer_state.connected || peer_state.initialized) {
        mbedtls_ssl_close_notify(&peer_state.ssl);
        mbedtls_ssl_free(&peer_state.ssl);
        mbedtls_ssl_config_free(&peer_state.conf);
        mbedtls_ctr_drbg_free(&peer_state.ctr_drbg);
        mbedtls_entropy_free(&peer_state.entropy);
        if (peer_state.sock >= 0) close(peer_state.sock);
        peer_state.connected = false;
        peer_state.initialized = false;
        peer_conn.connected = false;
        ESP_LOGI(TAG, "   🔌 Peer connection closed");
    }
}

// ============== AgentConfirmation ==============

bool send_agent_confirmation(contact_t *contact) {
    if (!peer_state.connected || !pending_peer.valid || !pending_peer.has_dh) {
        ESP_LOGE(TAG, "❌ Cannot send CONF: not ready");
        return false;
    }
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  📤 SENDING AGENT CONFIRMATION (v0.1.14 Format)              ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "   To queue: %02x%02x%02x%02x... (%d bytes)",
             pending_peer.queue_id[0], pending_peer.queue_id[1],
             pending_peer.queue_id[2], pending_peer.queue_id[3],
             pending_peer.queue_id_len);
    
    uint8_t *block = heap_caps_malloc(SMP_BLOCK_SIZE, MALLOC_CAP_8BIT);
    if (!block) return false;
    
    // ========== Build connInfo (ChatMessage JSON) ==========
    // Format: {"v":"1-16","event":"x.info","params":{"profile":{"displayName":"ESP32","fullName":""}}}
    
    const char *conn_info_json = "{\"v\":\"1-16\",\"event\":\"x.info\",\"params\":{\"profile\":{\"displayName\":\"ESP32\",\"fullName\":\"\"}}}";
    int json_len = strlen(conn_info_json);
    
    ESP_LOGI(TAG, "   📋 ConnInfo JSON (%d bytes):", json_len);
    ESP_LOGI(TAG, "      %s", conn_info_json);
    
    // ========== Build AgentConnInfo ==========
    // Format: 'I' + connInfo (Tail encoding = raw bytes)
    
    uint8_t agent_conn_info[256];
    int aci_len = 0;
    agent_conn_info[aci_len++] = 'I';  // AgentConnInfo tag
    memcpy(&agent_conn_info[aci_len], conn_info_json, json_len);
    aci_len += json_len;
    
    ESP_LOGI(TAG, "   📦 AgentConnInfo: %d bytes (tag='I' + JSON)", aci_len);
    
    // ========== Build AgentConfirmation ==========
    // Format: version (2 BE) + 'C' + e2eEncryption_ (Maybe) + encConnInfo (Tail)
    // smpEncode (agentVersion, 'C', e2eEncryption_, Tail encConnInfo)
    
    uint8_t agent_msg[512];
    int amp = 0;
    
    // agentVersion = 7 (2 bytes Big Endian)
    agent_msg[amp++] = 0x00;
    agent_msg[amp++] = 0x07;
    
    // Type = 'C' (Confirmation)
    agent_msg[amp++] = 'C';
    
    // e2eEncryption_ = Nothing (Maybe encoding: '0' = Nothing)
    agent_msg[amp++] = '0';
    
    // encConnInfo = AgentConnInfo (Tail encoding = raw bytes, no length prefix)
    memcpy(&agent_msg[amp], agent_conn_info, aci_len);
    amp += aci_len;
    
    ESP_LOGI(TAG, "   📨 AgentConfirmation: %d bytes", amp);
    ESP_LOGI(TAG, "      Header: %02x %02x %c %c (v7, 'C', Nothing)", 
             agent_msg[0], agent_msg[1], agent_msg[2], agent_msg[3]);
    
    // Debug: show first bytes
    printf("      Raw: ");
    for (int i = 0; i < amp && i < 32; i++) {
        printf("%02x ", agent_msg[i]);
    }
    printf("...\n");
    
    // ========== Encrypt with Peer's DH Key (crypto_box) ==========
    // Output: [24-byte nonce][ciphertext + 16-byte MAC]
    
    uint8_t nonce[24];
    esp_fill_random(nonce, 24);
    
    uint8_t encrypted[600];
    memcpy(encrypted, nonce, 24);
    
    if (crypto_box_easy(&encrypted[24], agent_msg, amp, nonce,
                        pending_peer.dh_public, contact->rcv_dh_secret) != 0) {
        ESP_LOGE(TAG, "   ❌ Encryption failed!");
        free(block);
        return false;
    }
    
    int encrypted_len = 24 + amp + crypto_box_MACBYTES;
    ESP_LOGI(TAG, "   🔐 Encrypted: %d bytes (nonce + ciphertext + MAC)", encrypted_len);
    
    // ========== Wrap in SMP Client Message ==========
    // Format: [SPKI our public key (44 bytes)][encrypted body]
    
    uint8_t client_msg[700];
    int cmp = 0;
    
    // Our X25519 SPKI public key (so peer can decrypt)
    memcpy(&client_msg[cmp], X25519_SPKI_HEADER, 12);
    cmp += 12;
    memcpy(&client_msg[cmp], contact->rcv_dh_public, 32);
    cmp += 32;
    
    // Encrypted body
    memcpy(&client_msg[cmp], encrypted, encrypted_len);
    cmp += encrypted_len;
    
    ESP_LOGI(TAG, "   📦 Client message: %d bytes (SPKI + encrypted)", cmp);
    
    // ========== Build SEND Command ==========
    // Format: CorrId + EntityId + "SEND " + flags + body
    
    uint8_t send_body[800];
    int sbp = 0;
    
    // CorrId (length-prefixed)
    send_body[sbp++] = 1;   // length
    send_body[sbp++] = 'C'; // correlator
    
    // EntityId = peer's queue ID (length-prefixed)
    send_body[sbp++] = pending_peer.queue_id_len;
    memcpy(&send_body[sbp], pending_peer.queue_id, pending_peer.queue_id_len);
    sbp += pending_peer.queue_id_len;
    
    // Command: "SEND "
    send_body[sbp++] = 'S';
    send_body[sbp++] = 'E';
    send_body[sbp++] = 'N';
    send_body[sbp++] = 'D';
    send_body[sbp++] = ' ';
    
    // Flags = "T " (with notification) - ASCII not binary!
    send_body[sbp++] = 'T';
    send_body[sbp++] = ' ';
    
    // MsgBody (no length prefix for SEND)
    memcpy(&send_body[sbp], client_msg, cmp);
    sbp += cmp;
    
    ESP_LOGI(TAG, "   📮 SEND body: %d bytes", sbp);
    
    // ========== Build Transmission (no signature needed for SEND) ==========
    // Format: authLen + sessionId + body
    
    uint8_t transmission[1024];
    int tp = 0;
    
    // No auth signature for SEND (length = 0)
    transmission[tp++] = 0;
    
    // SessionId (length-prefixed)
    transmission[tp++] = 32;
    memcpy(&transmission[tp], peer_state.session_id, 32);
    tp += 32;
    
    // Body
    memcpy(&transmission[tp], send_body, sbp);
    tp += sbp;
    
    ESP_LOGI(TAG, "   📡 Transmission: %d bytes", tp);
    
    // ========== Send! ==========
    int ret = smp_write_command_block(&peer_state.ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ Send failed!");
        free(block);
        return false;
    }
    
    ESP_LOGI(TAG, "   📤 SEND command sent! Waiting for response...");
    
    // Wait for response
    int content_len = smp_read_block(&peer_state.ssl, block, 10000);
    if (content_len >= 0) {
        uint8_t *resp = block + 2;
        
        // Debug: show response
        ESP_LOGI(TAG, "   📥 Response (%d bytes):", content_len);
        printf("      ");
        for (int i = 0; i < content_len && i < 40; i++) {
            printf("%02x ", resp[i]);
        }
        printf("\n");
        
        // Parse response
        int p = 0;
        if (resp[p] == 1) {
            p++;
            p += 2;  // skip next byte + something
            int authLen = resp[p++]; p += authLen;
            int sessLen = resp[p++]; p += sessLen;
            int corrLen = resp[p++]; p += corrLen;
            int entLen = resp[p++]; p += entLen;
            
            ESP_LOGI(TAG, "   Response command at offset %d: %c%c%c", 
                     p, resp[p], resp[p+1], resp[p+2]);
            
            if (p + 1 < content_len && resp[p] == 'O' && resp[p+1] == 'K') {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
                ESP_LOGI(TAG, "║   🎉🎉🎉 CONFIRMATION ACCEPTED BY SERVER! 🎉🎉🎉            ║");
                ESP_LOGI(TAG, "║                                                              ║");
                ESP_LOGI(TAG, "║   Server accepted our message.                               ║");
                ESP_LOGI(TAG, "║   Now waiting for SimpleX App to process it...              ║");
                ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
                free(block);
                return true;
            } else if (p + 2 < content_len && resp[p] == 'E' && resp[p+1] == 'R' && resp[p+2] == 'R') {
                ESP_LOGE(TAG, "   ❌ Server error: %.*s", 30, &resp[p]);
            }
        }
    } else {
        ESP_LOGW(TAG, "   ⚠️ No response received (timeout or error)");
    }
    
    free(block);
    return false;
}