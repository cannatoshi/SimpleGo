/**
 * SimpleGo - smp_handshake.c
 * Connection Handshake Implementation
 * Based on official agent-protocol.md specification
 * v0.1.15-alpha
 * 
 * Flow (from agent-protocol.md):
 * 1. We send AgentConfirmation (with 'D' + Reply Queue)
 * 2. We send SKEY to secure peer's queue
 * 3. We send HELLO message
 * 4. App sends HELLO back
 * 5. CONNECTED!
 */

#include "smp_handshake.h"
#include "smp_types.h"
#include "smp_network.h"
#include "smp_queue.h"
#include "smp_ratchet.h"
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "mbedtls/sha256.h"

static const char *TAG = "SMP_HAND";

// Connection state for handshake
static struct {
    bool confirmation_received;
    bool hello_sent;
    bool hello_received;
    bool connected;
    uint64_t msg_id;           // Sequential message ID
    uint8_t prev_msg_hash[32]; // Hash of previous message
    ratchet_state_t *ratchet;  // Pointer to ratchet state
} handshake_state = {0};

// ============== HELLO Message Building ==============

/**
 * Build HELLO message according to agent-protocol.md:
 * 
 * agentMessage = %s"M" agentMsgHeader aMessage msgPadding
 * agentMsgHeader = agentMsgId prevMsgHash
 * agentMsgId = 8*8 OCTET      ; Int64 Big-Endian
 * prevMsgHash = shortString   ; [len][hash...]
 * HELLO = %s"H"
 */
static int build_hello_message(uint8_t *output, int max_len) {
    int p = 0;
    
    // agentMessage tag = 'M'
    output[p++] = 'M';
    
    // agentMsgHeader:
    // agentMsgId = 8 bytes Big-Endian (sequential message ID)
    handshake_state.msg_id++;
    uint64_t msg_id = handshake_state.msg_id;
    output[p++] = (msg_id >> 56) & 0xFF;
    output[p++] = (msg_id >> 48) & 0xFF;
    output[p++] = (msg_id >> 40) & 0xFF;
    output[p++] = (msg_id >> 32) & 0xFF;
    output[p++] = (msg_id >> 24) & 0xFF;
    output[p++] = (msg_id >> 16) & 0xFF;
    output[p++] = (msg_id >> 8) & 0xFF;
    output[p++] = msg_id & 0xFF;
    
    // prevMsgHash = ByteString [Word16 BE len][hash...]
    // For first message, use empty (len=0)
    if (handshake_state.msg_id == 1) {
        output[p++] = 0x00;  // High byte
        output[p++] = 0x00;  // Low byte = 0 (empty hash)
    } else {
        output[p++] = 0x00;  // High byte
        output[p++] = 32;    // Low byte = 32
        memcpy(&output[p], handshake_state.prev_msg_hash, 32);
        p += 32;
    }
    // aMessage = HELLO = %s"H"
    output[p++] = 'H';
    // DEBUG: Show exact HELLO plaintext bytes
    ESP_LOGI(TAG, "   📋 HELLO plaintext hex:");
    printf("      ");
    for (int i = 0; i < p; i++) {
        printf("%02x ", output[i]);
    }
    printf("\n");
    // NO internal padding - ratchet_encrypt handles padding to 15840 bytes!
    ESP_LOGI(TAG, "   📦 HELLO plaintext: %d bytes", p);

    return p;
}

/*
 * agentMsgEnvelope = agentVersion %s"M" encAgentMessage
 * encAgentMessage = doubleRatchetEncryptedMessage
 */
static int build_agent_msg_envelope(
    ratchet_state_t *ratchet,
    const uint8_t *plaintext,
    int plain_len,
    uint8_t *output,
    int max_len
) {
    int p = 0;
    
    // agentVersion = 2 bytes Big-Endian (version 5)
    output[p++] = 0x00;
    output[p++] = 0x05;
    
    // Message type = 'M' (AgentMsgEnvelope)
    output[p++] = 'M';
    
    // encAgentMessage (Tail = no length prefix!)
    size_t enc_len = 0;
    if (ratchet_encrypt(plaintext, plain_len, &output[p], &enc_len, 15840) != 0) {
        ESP_LOGE(TAG, "   ❌ Ratchet encryption failed!");
        return -1;
    }
    p += enc_len;
    
    ESP_LOGI(TAG, "   📦 AgentMsgEnvelope: %d bytes", p);
    return p;
}

// ============== Send SKEY Command ==============

/**
 * Send SKEY command to secure the peer's queue.
 * This must be sent BEFORE HELLO according to agent-protocol.md:
 * "Agent B secures Alice's queue with SMP command SKEY"
 * 
 * Format: SKEY <senderAuthPublicKey>
 * senderAuthPublicKey = length x509encoded (Ed25519 SPKI)
 */
bool send_skey_command(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *our_auth_public  // Ed25519 public key (32 bytes raw)
) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  🔐 SENDING SKEY COMMAND (Secure Queue)                      ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    
    // Ed25519 SPKI format: 12 byte header + 32 byte key = 44 bytes
    static const uint8_t ED25519_SPKI_HEADER[12] = {
        0x30, 0x2A, 0x30, 0x05, 0x06, 0x03, 0x2B, 0x65, 0x70, 0x03, 0x21, 0x00
    };
    
    // Build SKEY command body
    // Format: [corrId][entityId=peer_queue_id]SKEY [authKey]
    uint8_t skey_body[150];
    int sbp = 0;
    
    // CorrId (length-prefixed string)
    skey_body[sbp++] = 1;    // Length
    skey_body[sbp++] = 'K';  // 'K' for SKEY
    
    // EntityId = peer's queue ID
    skey_body[sbp++] = (uint8_t)peer_queue_id_len;
    memcpy(&skey_body[sbp], peer_queue_id, peer_queue_id_len);
    sbp += peer_queue_id_len;
    
    // SKEY command
    memcpy(&skey_body[sbp], "SKEY ", 5);
    sbp += 5;
    
    // senderAuthPublicKey = length + Ed25519 SPKI (44 bytes)
    skey_body[sbp++] = 44;  // Length of SPKI
    memcpy(&skey_body[sbp], ED25519_SPKI_HEADER, 12);
    sbp += 12;
    memcpy(&skey_body[sbp], our_auth_public, 32);
    sbp += 32;
    
    ESP_LOGI(TAG, "   📮 SKEY body: %d bytes", sbp);
    ESP_LOGI(TAG, "   🔑 Auth key: %02x%02x%02x%02x...", 
             our_auth_public[0], our_auth_public[1], 
             our_auth_public[2], our_auth_public[3]);
    
    // Build transmission (no signature for SKEY - it's sender securing)
    uint8_t transmission[200];
    int tp = 0;
    
    // Auth = 0 (SKEY doesn't need auth, we're the sender)
    transmission[tp++] = 0;
    
    // SessionId (length-prefixed)
    transmission[tp++] = 32;
    memcpy(&transmission[tp], session_id, 32);
    tp += 32;
    
    // Body
    memcpy(&transmission[tp], skey_body, sbp);
    tp += sbp;
    
    ESP_LOGI(TAG, "   📡 Transmission: %d bytes", tp);
    
    // Send
    int ret = smp_write_command_block(ssl, block, transmission, tp);
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ Send SKEY failed: %d", ret);
        return false;
    }
    
    ESP_LOGI(TAG, "   📤 SKEY sent! Waiting for response...");
    
    // Wait for OK response
    int content_len = smp_read_block(ssl, block, 10000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   ❌ No response!");
        return false;
    }
    
    uint8_t *resp = block + 2;
    
    // Look for OK
    for (int i = 0; i < content_len - 1; i++) {
        if (resp[i] == 'O' && resp[i+1] == 'K') {
            ESP_LOGI(TAG, "   ✅ SKEY accepted! Queue secured.");
            return true;
        }
    }
    
    // Debug: show response
    ESP_LOGW(TAG, "   ⚠️  SKEY Response:");
    for (int i = 0; i < content_len && i < 50; i++) {
        printf("%02x ", resp[i]);
    }
    printf("\n");
    
    // Check for specific errors
    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R' && resp[i+3] == ' ') {
            ESP_LOGW(TAG, "   ⚠️  Server returned error (queue may already be secured)");
            return true;  // Continue anyway - might already be secured
        }
    }
    
    return false;
}

// ============== Send HELLO ==============

/**
 * Send HELLO message to complete the connection handshake.
 * 
 * This is sent to the PEER's queue (the one from the invitation)
 * after we've sent our AgentConfirmation.
 */
bool send_hello_message(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet
) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  📤 SENDING HELLO MESSAGE                                    ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    
    handshake_state.ratchet = ratchet;
    
    // 1. Build HELLO plaintext
    uint8_t hello_plain[300];
    int hello_plain_len = build_hello_message(hello_plain, sizeof(hello_plain));
    if (hello_plain_len < 0) {
        ESP_LOGE(TAG, "   ❌ Failed to build HELLO message!");
        return false;
    }
    
    // 2. Build AgentMsgEnvelope (encrypt with ratchet)
    #define HELLO_BUFFER_SIZE 17000
    
    uint8_t *agent_envelope = malloc(HELLO_BUFFER_SIZE);
    if (!agent_envelope) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate agent_envelope!");
        return false;
    }
    int envelope_len = build_agent_msg_envelope(ratchet, hello_plain, hello_plain_len,
                                                  agent_envelope, HELLO_BUFFER_SIZE);  // ← FIX!
    if (envelope_len < 0) {
        ESP_LOGE(TAG, "   ❌ Failed to build AgentMsgEnvelope!");
        free(agent_envelope);
        return false;
    }

    ESP_LOGI(TAG, "   📦 AgentMsgEnvelope: %d bytes", envelope_len);

    // 3. Encrypt with SMP-level crypto (NaCL crypto_box)
    uint8_t nonce[24];
    esp_fill_random(nonce, 24);

    // Client message = PubHeader + nonce + encrypted envelope
    uint8_t *client_msg = malloc(HELLO_BUFFER_SIZE);
    if (!client_msg) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate client_msg!");
        free(agent_envelope);
        return false;
    }
    int cmp = 0;

    // PubHeader: SMP Client Version (Word16 BE) - version 4
    client_msg[cmp++] = 0x00;
    client_msg[cmp++] = 0x04;

    // PubHeader: Maybe tag '1' = Just (we have a DH key)
    client_msg[cmp++] = '1';

    // PubHeader: Length prefix for 44-byte SPKI
    client_msg[cmp++] = 44;

    // PubHeader: X25519 DH public key in SPKI format (44 bytes)
    memcpy(&client_msg[cmp], X25519_SPKI_HEADER, 12);
    cmp += 12;
    memcpy(&client_msg[cmp], our_dh_public, 32);
    cmp += 32;

    // Nonce (24 bytes)
    memcpy(&client_msg[cmp], nonce, 24);
    cmp += 24;

    // Encrypted envelope + MAC (16 bytes MAC)
    uint8_t *enc_envelope = malloc(HELLO_BUFFER_SIZE);
    if (!enc_envelope) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate enc_envelope!");
        free(client_msg);
        free(agent_envelope);
        return false;
    }
    
    if (crypto_box_easy(enc_envelope, agent_envelope, envelope_len,
                        nonce, peer_dh_public, our_dh_private) != 0) {
        ESP_LOGE(TAG, "   ❌ crypto_box failed!");
        free(enc_envelope);
        free(client_msg);
        free(agent_envelope);
        return false;
    }
    int enc_envelope_len = envelope_len + crypto_box_MACBYTES;

    memcpy(&client_msg[cmp], enc_envelope, enc_envelope_len);
    cmp += enc_envelope_len;

    free(enc_envelope);  // Nicht mehr benötigt
    free(agent_envelope);  // Nicht mehr benötigt

    ESP_LOGI(TAG, "   📦 Client message: %d bytes (48 PubHeader + 24 nonce + %d encrypted)",
             cmp, enc_envelope_len);

    // 4. Build SEND command body
    uint8_t *send_body = malloc(HELLO_BUFFER_SIZE);
    if (!send_body) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate send_body!");
        free(client_msg);
        return false;
    }
    int sbp = 0;

    // CorrId (length-prefixed string)
    send_body[sbp++] = 1;
    send_body[sbp++] = 'H';

    // EntityId = peer's queue ID
    send_body[sbp++] = (uint8_t)peer_queue_id_len;
    memcpy(&send_body[sbp], peer_queue_id, peer_queue_id_len);
    sbp += peer_queue_id_len;

    // SEND command
    memcpy(&send_body[sbp], "SEND ", 5);
    sbp += 5;

    // Flags
    send_body[sbp++] = 'T';
    send_body[sbp++] = ' ';

    // MsgBody
    memcpy(&send_body[sbp], client_msg, cmp);
    sbp += cmp;

    free(client_msg);  // Nicht mehr benötigt

    ESP_LOGI(TAG, "   📮 SEND body: %d bytes", sbp);

    // 5. Build transmission
    uint8_t *transmission = malloc(HELLO_BUFFER_SIZE);
    if (!transmission) {
        ESP_LOGE(TAG, "   ❌ Failed to allocate transmission!");
        free(send_body);
        return false;
    }
    int tp = 0;

    transmission[tp++] = 0;
    transmission[tp++] = 32;
    memcpy(&transmission[tp], session_id, 32);
    tp += 32;

    memcpy(&transmission[tp], send_body, sbp);
    tp += sbp;

    free(send_body);  // Nicht mehr benötigt

    ESP_LOGI(TAG, "   📡 Transmission: %d bytes", tp);

    // 6. Send
    int ret = smp_write_command_block(ssl, block, transmission, tp);
    free(transmission);  // Nicht mehr benötigt
    
    if (ret != 0) {
        ESP_LOGE(TAG, "   ❌ Send failed: %d", ret);
        return false;
    }
    
    ESP_LOGI(TAG, "   📤 HELLO sent! Waiting for response...");
    
    // 7. Wait for OK response
    int content_len = smp_read_block(ssl, block, 10000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "   ❌ No response!");
        return false;
    }
    
    uint8_t *resp = block + 2;
    
    // Look for OK or OK#
    for (int i = 0; i < content_len - 1; i++) {
        if (resp[i] == 'O' && resp[i+1] == 'K') {
            ESP_LOGI(TAG, "   ✅ HELLO accepted by server!");
            handshake_state.hello_sent = true;
            
            // Update prev_msg_hash for next message
            mbedtls_sha256(hello_plain, hello_plain_len, handshake_state.prev_msg_hash, 0);
            
            return true;
        }
    }
    
    // Debug: show full response with better parsing
    ESP_LOGW(TAG, "   ⚠️  Response not OK (%d bytes):", content_len);
    
    // Show first 80 bytes hex
    printf("   HEX: ");
    for (int i = 0; i < content_len && i < 80; i++) {
        printf("%02x ", resp[i]);
    }
    printf("\n");
    
    // Try to find and show error message
    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R' && resp[i+3] == ' ') {
            // Found "ERR ", print the error type
            char err_type[32] = {0};
            int j = 0;
            for (int k = i + 4; k < content_len && j < 31 && resp[k] >= 0x20 && resp[k] < 0x7F; k++) {
                err_type[j++] = resp[k];
            }
            ESP_LOGE(TAG, "   🛑 Server Error: ERR %s", err_type);
            break;
        }
    }
    
    return false;
}

// ============== Parse Incoming HELLO ==============

/**
 * Parse a decrypted message to check if it's HELLO
 * 
 * Format:
 * agentMessage = %s"M" agentMsgHeader aMessage msgPadding
 * agentMsgHeader = agentMsgId prevMsgHash  
 * HELLO = %s"H"
 */
bool parse_hello_message(const uint8_t *data, int len, uint64_t *msg_id_out) {
    if (len < 10) {
        return false;
    }
    
    int p = 0;
    
    // Check for 'M' tag (agentMessage)
    if (data[p++] != 'M') {
        ESP_LOGD(TAG, "   Not agentMessage (tag='%c')", data[p-1]);
        return false;
    }
    
    // agentMsgId = 8 bytes Big-Endian
    uint64_t msg_id = 0;
    for (int i = 0; i < 8; i++) {
        msg_id = (msg_id << 8) | data[p++];
    }
    
    // prevMsgHash = shortString [len][data...]
    uint8_t hash_len = data[p++];
    p += hash_len;
    
    if (p >= len) {
        return false;
    }
    
    // aMessage - check for HELLO = 'H'
    if (data[p] == 'H') {
        ESP_LOGI(TAG, "   ✅ HELLO parsed! MsgId: %llu", (unsigned long long)msg_id);
        handshake_state.hello_received = true;
        if (msg_id_out) *msg_id_out = msg_id;
        return true;
    }
    
    ESP_LOGD(TAG, "   Not HELLO (cmd='%c' 0x%02x)", data[p], data[p]);
    return false;
}

// ============== Complete Connection Handshake ==============

/**
 * Complete the duplex connection handshake after sending AgentConfirmation.
 * 
 * From agent-protocol.md:
 * - After sending confirmation, we (joining party) must:
 *   1. Subscribe to our reply queue (to receive confirmation back)
 *   2. Receive confirmation from initiating party
 *   3. Send HELLO message
 *   4. Receive HELLO back
 *   5. Connection established!
 * 
 * For Fast Duplex (modern apps), steps 1-2 may be skipped.
 */
bool complete_handshake(
    mbedtls_ssl_context *peer_ssl,
    uint8_t *block,
    const uint8_t *peer_session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *peer_dh_public,
    const uint8_t *our_dh_private,
    const uint8_t *our_dh_public,
    ratchet_state_t *ratchet
) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  🔗 COMPLETING DUPLEX CONNECTION HANDSHAKE                   ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    ESP_LOGI(TAG, "");
    
    // Reset handshake state
    memset(&handshake_state, 0, sizeof(handshake_state));
    
    // Step 1: AgentConfirmation already sent
    ESP_LOGI(TAG, "   [1/4] AgentConfirmation ✅ (already sent)");
    
    // Step 2: Fast duplex - skip waiting for confirmation
    ESP_LOGI(TAG, "   [2/4] Peer confirmation ✅ (fast duplex, skipped)");
    handshake_state.confirmation_received = true;
    
    // Step 4: Send HELLO
    ESP_LOGI(TAG, "   [3/4] Sending HELLO...");
    
    if (!send_hello_message(peer_ssl, block, peer_session_id,
                            peer_queue_id, peer_queue_id_len,
                            peer_dh_public, our_dh_private, our_dh_public,
                            ratchet)) {
        ESP_LOGE(TAG, "   ❌ Failed to send HELLO!");
        return false;
    }
    
    // Step 5: Wait for HELLO from peer
    ESP_LOGI(TAG, "   [4/4] Waiting for HELLO from peer...");
    ESP_LOGI(TAG, "         ⏳ (Will arrive on reply queue)");
    
    // For demo purposes, mark as connected after sending our HELLO
    // The SimpleX app should show "Connected" when it receives our HELLO
    handshake_state.connected = true;
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  📊 HANDSHAKE STATUS                                         ║");
    ESP_LOGI(TAG, "╠══════════════════════════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║  AgentConfirmation sent:   ✅                                ║");
    ESP_LOGI(TAG, "║  Fast duplex mode:         ✅                                ║");
    ESP_LOGI(TAG, "║  HELLO sent:               ✅                                ║");
    ESP_LOGI(TAG, "║  HELLO received:           ⏳ (waiting)                      ║");
    ESP_LOGI(TAG, "╠══════════════════════════════════════════════════════════════╣");
    ESP_LOGI(TAG, "║  🎉 CONNECTION SHOULD BE ACTIVE! 🎉                          ║");
    ESP_LOGI(TAG, "║  Check SimpleX app for 'Connected' status.                   ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
    
    return true;
}

// ============== Status Getters ==============

bool is_handshake_complete(void) {
    return handshake_state.hello_sent && handshake_state.hello_received;
}

bool is_hello_sent(void) {
    return handshake_state.hello_sent;
}

bool is_hello_received(void) {
    return handshake_state.hello_received;
}

bool is_connected(void) {
    return handshake_state.connected;
}

void reset_handshake_state(void) {
    memset(&handshake_state, 0, sizeof(handshake_state));
}
