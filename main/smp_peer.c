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
#include "smp_x448.h"      // NEU: X448 E2E Ratchet
#include "smp_ratchet.h"   // NEU
#include "smp_queue.h"     // NEU: Für unsere Queue
#include "smp_handshake.h"

static const char *TAG = "SMP_PEER";

// Externs (defined elsewhere in project)
extern const int ciphersuites[];
extern int my_send_cb(void *ctx, const unsigned char *buf, size_t len);
extern int my_recv_cb(void *ctx, unsigned char *buf, size_t len);
extern int smp_tcp_connect(const char *host, int port);
extern int smp_read_block(mbedtls_ssl_context *ssl, uint8_t *block, int timeout_ms);

// SPKI headers from crypto module
extern const uint8_t X25519_SPKI_HEADER[12];
extern const uint8_t ED25519_SPKI_HEADER[12];

// ============== Peer Connection State ==============

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

    // NOTE: SKEY removed - it's for Fast Duplex (v9+ SMP protocol).
    // Standard flow: SEND without auth works on unsecured queues.

    // ========== Build connInfo (ChatMessage JSON) ==========
    const char *conn_info_json =
        "{\"v\":\"1-16\",\"event\":\"x.info\",\"params\":{\"profile\":{\"displayName\":\"ESP32\",\"fullName\":\"\"}}}";
    int json_len = (int)strlen(conn_info_json);

    ESP_LOGI(TAG, "   📋 ConnInfo JSON (%d bytes):", json_len);
    ESP_LOGI(TAG, "      %s", conn_info_json);

    // ========== Build AgentConnInfoReply ==========
    // Format: 'D' + smpQueues (NonEmpty SMPQueueInfo) + connInfo (Tail)
    if (!our_queue.valid) {
        ESP_LOGE(TAG, "❌ Our queue not created! Call queue_create() first!");
        free(block);
        return false;
    }

    uint8_t agent_conn_info[512];
    int aci_len = 0;

    // Tag = 'D' for AgentConnInfoReply (joining party sends this)
    agent_conn_info[aci_len++] = 'D';

    // smpQueues = NonEmpty list with 1 element
    agent_conn_info[aci_len++] = 0x00;  // smpQueues length high byte
    agent_conn_info[aci_len++] = 0x01;  // smpQueues length low byte (1 queue)

    int queue_info_len = queue_encode_info(&agent_conn_info[aci_len], sizeof(agent_conn_info) - (size_t)aci_len);
    if (queue_info_len < 0) {
        ESP_LOGE(TAG, "❌ Failed to encode queue info!");
        free(block);
        return false;
    }
    aci_len += queue_info_len;

    // connInfo = JSON tail (no extra length prefix here)
    memcpy(&agent_conn_info[aci_len], conn_info_json, (size_t)json_len);
    aci_len += json_len;

    ESP_LOGI(TAG, "   📦 AgentConnInfoReply: %d bytes (tag='D' + queue + JSON)", aci_len);
    ESP_LOGI(TAG, "      Queue: %s, sndId: %02x%02x%02x%02x...",
             our_queue.server_host,
             our_queue.snd_id[0], our_queue.snd_id[1],
             our_queue.snd_id[2], our_queue.snd_id[3]);

    printf("      Raw AgentConnInfoReply (%d bytes):\n", aci_len);
    for (int i = 0; i < aci_len; i += 16) {
        printf("      ");
        for (int j = 0; j < 16 && (i+j) < aci_len; j++) {
            printf("%02x ", agent_conn_info[i+j]);
        }
        printf("\n");
    }

    // ========== Generate E2E Ratchet Parameters ==========
    e2e_params_t *e2e_params = (e2e_params_t *)heap_caps_malloc(sizeof(e2e_params_t), MALLOC_CAP_8BIT);
    if (!e2e_params) {
        ESP_LOGE(TAG, "❌ Failed to allocate e2e_params!");
        free(block);
        return false;
    }

    if (!e2e_generate_params(e2e_params)) {
        ESP_LOGE(TAG, "❌ Failed to generate E2E params!");
        free(e2e_params);
        free(block);
        return false;
    }

    // Encode E2E params (SndE2ERatchetParams format)
    uint8_t e2e_encoded[1800];
    int e2e_len = e2e_encode_params(e2e_params, e2e_encoded);
    ESP_LOGI(TAG, "   🔐 E2E params: %d bytes", e2e_len);

    // ========== X3DH + Ratchet Encryption ==========
    if (!pending_peer.has_e2e) {
        ESP_LOGE(TAG, "❌ No E2E keys from peer!");
        free(e2e_params);
        free(block);
        return false;
    }

    // X3DH sender
    if (!ratchet_x3dh_sender(pending_peer.e2e_key1, pending_peer.e2e_key2, &e2e_params->key1, &e2e_params->key2)) {
        ESP_LOGE(TAG, "❌ X3DH failed!");
        free(e2e_params);
        free(block);
        return false;
    }

    // Ratchet init (global state)
    if (!ratchet_init_sender(pending_peer.e2e_key2, &e2e_params->key2)) {
        ESP_LOGE(TAG, "❌ Ratchet init failed!");
        free(e2e_params);
        free(block);
        return false;
    }

    // Encrypt AgentConnInfo with ratchet
    #define ENC_CONN_INFO_SIZE (1 + 123 + 16 + 14832 + 100)  // ~15072 bytes
    uint8_t *enc_conn_info = malloc(ENC_CONN_INFO_SIZE);
    if (!enc_conn_info) {
        ESP_LOGE(TAG, "❌ Failed to allocate enc_conn_info!");
        free(e2e_params);
        free(block);
        return false;
    }
    size_t enc_conn_info_len = 0;

    if (ratchet_encrypt(agent_conn_info, (size_t)aci_len, enc_conn_info, &enc_conn_info_len, 14832) != 0) {
        ESP_LOGE(TAG, "❌ Ratchet encrypt failed!");
        free(enc_conn_info);
        free(e2e_params);
        free(block);
        return false;
    }
    
    // DEBUG: Show encConnInfo after successful encrypt
    ESP_LOGI(TAG, "📋 encConnInfo FULL (first 64 bytes):");
    printf("   ");
    for (int i = 0; i < 64 && i < (int)enc_conn_info_len; i++) {
        printf("%02x ", enc_conn_info[i]);
        if ((i+1) % 32 == 0) printf("\n   ");
    }
    printf("\n");
    
    ESP_LOGI(TAG, "   🔒 encConnInfo encrypted: %d bytes", (int)enc_conn_info_len);

    // ========== Build AgentConfirmation ==========
    uint8_t *agent_msg = malloc(20000);
    if (!agent_msg) {
        ESP_LOGE(TAG, "❌ Failed to allocate agent_msg!");
        free(enc_conn_info);
        free(e2e_params);
        free(block);
        return false;
}
    int amp = 0;

    // agentVersion = 7 (2 bytes Big Endian)
    agent_msg[amp++] = 0x00;
    agent_msg[amp++] = 0x07;

    // Type = 'C' (Confirmation)
    agent_msg[amp++] = 'C';

    // Maybe tag for E2E params: ASCII '1' or '0'
    agent_msg[amp++] = '1';

    // E2E params first
    memcpy(&agent_msg[amp], e2e_encoded, (size_t)e2e_len);
    amp += e2e_len;

    // encConnInfo after params (Tail = no length prefix!)
    memcpy(&agent_msg[amp], enc_conn_info, enc_conn_info_len);
    amp += (int)enc_conn_info_len;

    ESP_LOGI(TAG, "    📨 AgentConfirmation: %d bytes", amp);
    ESP_LOGI(TAG, "      Header: %02x %02x %c (v7, 'C')",
             agent_msg[0], agent_msg[1], agent_msg[2]);

    printf("      Raw: ");
    for (int i = 0; i < amp && i < 32; i++) {
        printf("%02x ", agent_msg[i]);
    }
    printf("...\n");

    // ========== Build ClientMessage Plaintext ==========
    // PrivHeader for Confirmation = 'K' + Ed25519 SPKI (KEIN length prefix!)
    uint8_t *plaintext = malloc(20000);
    if (!plaintext) {
        ESP_LOGE(TAG, "❌ Failed to allocate plaintext!");
        free(agent_msg);
        free(enc_conn_info);
        free(e2e_params);
        free(block);
        return false;
    }
    int pp = 0;

    // 'K' = PHConfirmation tag
    plaintext[pp++] = 'K';

    // Ed25519 SPKI direkt (12 header + 32 key = 44 bytes, KEIN length prefix!)
    memcpy(&plaintext[pp], ED25519_SPKI_HEADER, 12);
    pp += 12;
    memcpy(&plaintext[pp], our_queue.rcv_auth_public, 32);
    pp += 32;

    // AgentMsgEnvelope (AgentConfirmation)
    memcpy(&plaintext[pp], agent_msg, (size_t)amp);
    pp += amp;

    ESP_LOGI(TAG, "   📝 ClientMessage plaintext: %d bytes (PrivHeader + AgentMsg)", pp);

    // ========== Encrypt with Peer's DH Key (crypto_box) ==========
    uint8_t nonce[24];
    esp_fill_random(nonce, 24);

    // DEBUG: Show plaintext BEFORE encryption
    ESP_LOGI(TAG, "📋 Plaintext BEFORE crypto_box (%d bytes):", pp);
    printf("      ");
    for (int i = 0; i < 64 && i < pp; i++) {
        printf("%02x ", plaintext[i]);
        if ((i + 1) % 32 == 0) printf("\n      ");
    }
    printf("\n");

    // Build PADDED plaintext with SimpleX padding scheme
    // e2eEncConfirmationLength = 15904 bytes
    #define E2E_ENC_CONFIRMATION_LENGTH 15904
    
    uint8_t *padded = malloc(E2E_ENC_CONFIRMATION_LENGTH);
    if (!padded) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate padding buffer!");
        free(e2e_params);
        free(block);
        return false;
    }
    
    // 2-Byte Length Prefix (Big Endian) - Nachrichtenlänge
    uint16_t msg_len = pp;  // pp = plaintext length
    padded[0] = (msg_len >> 8) & 0xFF;  // High byte
    padded[1] = msg_len & 0xFF;         // Low byte
    
    // Copy actual plaintext
    memcpy(&padded[2], plaintext, pp);
    
    // Fill rest with '#' (0x23)
    int pad_start = 2 + pp;
    memset(&padded[pad_start], '#', E2E_ENC_CONFIRMATION_LENGTH - pad_start);
    
    int padded_len = E2E_ENC_CONFIRMATION_LENGTH;

    // DEBUG: Show padded plaintext
    ESP_LOGI(TAG, "📋 PADDED Plaintext (%d bytes = 2 + %d + %d padding):", 
             padded_len, pp, E2E_ENC_CONFIRMATION_LENGTH - 2 - pp);
    printf("      ");
    for (int i = 0; i < 64 && i < padded_len; i++) {
        printf("%02x ", padded[i]);
        if ((i + 1) % 32 == 0) printf("\n      ");
    }
    printf("\n");

// Jetzt crypto_box mit PADDED!
    uint8_t *encrypted = malloc(24 + E2E_ENC_CONFIRMATION_LENGTH + crypto_box_MACBYTES);
    if (!encrypted) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate encrypted buffer!");
        free(padded);
        free(e2e_params);
        free(block);
        return false;
    }
    memcpy(encrypted, nonce, 24);
    if (crypto_box_easy(&encrypted[24], padded, (unsigned long long)padded_len, nonce,
                        pending_peer.dh_public, contact->rcv_dh_secret) != 0) {
        ESP_LOGE(TAG, "   ❌ Encryption failed!");
        free(padded);     // ← HIER AUCH FREIGEBEN!
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }
    
    free(padded);  // ← Nach erfolgreicher Verschlüsselung freigeben

    // WICHTIG: encrypted_len muss jetzt padded_len nutzen!
    int encrypted_len = 24 + padded_len + crypto_box_MACBYTES;
    ESP_LOGI(TAG, "   🔐 Encrypted: %d bytes (nonce + ciphertext + MAC)", encrypted_len);

    // ========== Wrap in SMP ClientMsgEnvelope ==========
    // PubHeader = [Version (2B BE)][Maybe '1'][len=44][X25519 SPKI]
    // Body = [nonce+ciphertext+mac]
    uint8_t *client_msg = malloc(24 + E2E_ENC_CONFIRMATION_LENGTH + crypto_box_MACBYTES + 100);
    if (!client_msg) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate client_msg buffer!");
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }
    int cmp = 0;

    // SMP client version 4
    client_msg[cmp++] = 0x00;
    client_msg[cmp++] = 0x04;

    // Maybe tag: ASCII '1'
    client_msg[cmp++] = '1';

    // LENGTH PREFIX for 44-byte X25519 SPKI - FIX
    client_msg[cmp++] = 44;  // 0x2C

    // X25519 DH public key SPKI
    memcpy(&client_msg[cmp], X25519_SPKI_HEADER, 12);
    cmp += 12;
    memcpy(&client_msg[cmp], contact->rcv_dh_public, 32);
    cmp += 32;

    // Encrypted body
    // Nonce SEPARAT (24 bytes) - nicht Teil des encrypted body!
    memcpy(&client_msg[cmp], nonce, 24);
    cmp += 24;

    // Ciphertext + MAC (OHNE die ersten 24 Nonce-Bytes!)
    memcpy(&client_msg[cmp], &encrypted[24], (size_t)(encrypted_len - 24));
    cmp += (encrypted_len - 24);

    ESP_LOGI(TAG, "   📦 Client message: %d bytes (PubHeader + encrypted)", cmp);

    // ========== Build SEND Command ==========
    #define SEND_BUFFER_SIZE (E2E_ENC_CONFIRMATION_LENGTH + 200)
    uint8_t *send_body = malloc(SEND_BUFFER_SIZE);
    if (!send_body) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate send_body!");
        free(client_msg);
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }
    int sbp = 0;

    // CorrId (length-prefixed)
    send_body[sbp++] = 1;
    send_body[sbp++] = 'C';

    // EntityId = peer's queue ID (length-prefixed)
    send_body[sbp++] = pending_peer.queue_id_len;
    memcpy(&send_body[sbp], pending_peer.queue_id, (size_t)pending_peer.queue_id_len);
    sbp += pending_peer.queue_id_len;

    // Command: "SEND "
    send_body[sbp++] = 'S';
    send_body[sbp++] = 'E';
    send_body[sbp++] = 'N';
    send_body[sbp++] = 'D';
    send_body[sbp++] = ' ';

    // Flags: "T "
    send_body[sbp++] = 'T';
    send_body[sbp++] = ' ';

    // MsgBody (no length prefix for SEND)
    memcpy(&send_body[sbp], client_msg, (size_t)cmp);
    sbp += cmp;

    ESP_LOGI(TAG, "   📮 SEND body: %d bytes", sbp);

    // ========== Build Transmission ==========
    uint8_t *transmission = malloc(SEND_BUFFER_SIZE + 100);
    if (!transmission) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate transmission!");
        free(send_body);
        free(client_msg);
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }
    int tp = 0;

    // No auth signature for SEND
    transmission[tp++] = 0;

    // SessionId (length-prefixed)
    transmission[tp++] = 32;
    memcpy(&transmission[tp], peer_state.session_id, 32);
    tp += 32;

    // Body
    memcpy(&transmission[tp], send_body, (size_t)sbp);
    tp += sbp;

    ESP_LOGI(TAG, "   📡 Transmission: %d bytes", tp);

    // ========== Send ==========
    int ret = smp_write_command_block(&peer_state.ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ Send failed!");
        free(transmission);
        free(send_body);
        free(client_msg);
        free(encrypted);
        free(e2e_params);
        free(block);
        return false;
    }

    ESP_LOGI(TAG, "   📤 SEND command sent! Waiting for response...");

    // Wait for response
    int content_len = smp_read_block(&peer_state.ssl, block, 10000);
    if (content_len >= 0) {
        uint8_t *resp = block + 2;

        ESP_LOGI(TAG, "   📥 Response (%d bytes):", content_len);
        printf("      ");
        for (int i = 0; i < content_len && i < 40; i++) {
            printf("%02x ", resp[i]);
        }
        printf("\n");

        // Parse response (lightweight, best-effort)
        int p = 0;
        if (resp[p] == 1) {
            p++;
            p += 2;  // skip next byte + something (your existing framing)

            int authLen = resp[p++]; p += authLen;
            int sessLen = resp[p++]; p += sessLen;
            int corrLen = resp[p++]; p += corrLen;
            int entLen  = resp[p++]; p += entLen;

            ESP_LOGI(TAG, "   Response command at offset %d: %c%c%c",
                     p, resp[p], resp[p + 1], resp[p + 2]);

            if (p + 1 < content_len && resp[p] == 'O' && resp[p + 1] == 'K') {
                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
                ESP_LOGI(TAG, "║   🎉🎉🎉 CONFIRMATION ACCEPTED BY SERVER! 🎉🎉🎉            ║");
                ESP_LOGI(TAG, "║                                                              ║");
                ESP_LOGI(TAG, "║   Server accepted our message.                               ║");
                ESP_LOGI(TAG, "║   Now waiting for SimpleX App to process it...               ║");
                ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");

                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "   📤 Starting HELLO handshake...");

                if (pending_peer.has_dh && pending_peer.valid) {
                    uint8_t our_dh_private[32];
                    uint8_t our_dh_public[32];
                    memcpy(our_dh_private, our_queue.rcv_dh_private, 32);
                    memcpy(our_dh_public,  our_queue.rcv_dh_public,  32);

                    bool hello_ok = complete_handshake(
                        &peer_state.ssl,
                        block,
                        peer_state.session_id,
                        pending_peer.queue_id,
                        pending_peer.queue_id_len,
                        pending_peer.dh_public,
                        our_dh_private,
                        our_dh_public,
                        ratchet_get_state()
                    );

                    if (hello_ok) {
                        ESP_LOGI(TAG, "   ✅ HELLO handshake complete!");
                    } else {
                        ESP_LOGW(TAG, "   ⚠️  HELLO handshake failed (connection may still work)");
                    }
                } else {
                    ESP_LOGW(TAG, "   ⚠️  No peer DH key available, skipping HELLO");
                }

                free(transmission);
                free(send_body);
                free(client_msg);
                free(encrypted);
                free(e2e_params);
                free(block);
                return true;

            } else if (p + 2 < content_len && resp[p] == 'E' && resp[p + 1] == 'R' && resp[p + 2] == 'R') {
                ESP_LOGE(TAG, "   ❌ Server error: %.*s", 30, &resp[p]);
            }
        }
    } else {
        ESP_LOGW(TAG, "   ⚠️ No response received (timeout or error)");
    }

    free(transmission);
    free(send_body);
    free(client_msg);
    free(encrypted);
    free(e2e_params);
    free(block);
    return false;
}
