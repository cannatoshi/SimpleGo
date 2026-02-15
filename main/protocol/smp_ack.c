/**
 * SimpleGo - SMP ACK Command
 * Consolidated ACK sending for all queue types
 *
 * Phase 3: Added smp_build_ack() for task architecture.
 */

#include "smp_ack.h"
#include <string.h>
#include "esp_log.h"
#include "sodium.h"
#include "smp_network.h"

static const char *TAG = "SMP_ACK";

/**
 * Internal: Build the signed ACK transport frame into a buffer.
 * Returns the length of the transport frame, or -1 on error.
 */
static int build_ack_internal(uint8_t *out, size_t out_size,
                               const uint8_t *session_id,
                               const uint8_t *recipient_id, int recipient_id_len,
                               const uint8_t *msg_id, int msg_id_len,
                               const uint8_t *rcv_auth_secret)
{
    if (!out || !session_id || !recipient_id || !msg_id || !rcv_auth_secret) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    if (out_size < 192) {
        ESP_LOGE(TAG, "Output buffer too small (%zu)", out_size);
        return -1;
    }

    // Build ACK body: [version=1]['A'][rcvIdLen][rcvId]["ACK "][msgIdLen][msgId]
    uint8_t ack_body[64];
    int ap = 0;

    ack_body[ap++] = 1;                                          // version
    ack_body[ap++] = 'A';                                        // entity type
    ack_body[ap++] = (uint8_t)recipient_id_len;                  // recipient ID length
    memcpy(&ack_body[ap], recipient_id, recipient_id_len);
    ap += recipient_id_len;
    ack_body[ap++] = 'A';                                        // ACK command
    ack_body[ap++] = 'C';
    ack_body[ap++] = 'K';
    ack_body[ap++] = ' ';
    ack_body[ap++] = (uint8_t)msg_id_len;                        // message ID length
    memcpy(&ack_body[ap], msg_id, msg_id_len);
    ap += msg_id_len;

    // Build data to sign: [sessLen=32][sessionId][body]
    uint8_t to_sign[128];
    int sp = 0;
    to_sign[sp++] = 32;
    memcpy(&to_sign[sp], session_id, 32);
    sp += 32;
    memcpy(&to_sign[sp], ack_body, ap);
    sp += ap;

    // Sign with Ed25519
    uint8_t sig[crypto_sign_BYTES];
    crypto_sign_detached(sig, NULL, to_sign, sp, rcv_auth_secret);

    // Build transport frame: [sigLen][signature][sessLen][sessionId][body]
    int tp = 0;
    out[tp++] = crypto_sign_BYTES;
    memcpy(&out[tp], sig, crypto_sign_BYTES);
    tp += crypto_sign_BYTES;
    out[tp++] = 32;
    memcpy(&out[tp], session_id, 32);
    tp += 32;
    memcpy(&out[tp], ack_body, ap);
    tp += ap;

    ESP_LOGD(TAG, "ACK built (%d bytes, rcvId=%02x%02x..., msgId=%02x%02x...)",
             tp, recipient_id[0], recipient_id[1], msg_id[0], msg_id[1]);

    return tp;
}

int smp_build_ack(uint8_t *out, size_t out_size,
                  const uint8_t *session_id,
                  const uint8_t *recipient_id, int recipient_id_len,
                  const uint8_t *msg_id, int msg_id_len,
                  const uint8_t *rcv_auth_secret)
{
    return build_ack_internal(out, out_size, session_id,
                              recipient_id, recipient_id_len,
                              msg_id, msg_id_len, rcv_auth_secret);
}

bool smp_send_ack(mbedtls_ssl_context *ssl, uint8_t *block,
                  const uint8_t *session_id,
                  const uint8_t *recipient_id, int recipient_id_len,
                  const uint8_t *msg_id, int msg_id_len,
                  const uint8_t *rcv_auth_secret)
{
    if (!ssl || !block) {
        ESP_LOGE(TAG, "Invalid ssl/block parameters");
        return false;
    }

    uint8_t transport[192];
    int tp = build_ack_internal(transport, sizeof(transport), session_id,
                                 recipient_id, recipient_id_len,
                                 msg_id, msg_id_len, rcv_auth_secret);
    if (tp < 0) return false;

    smp_write_command_block(ssl, block, transport, tp);
    return true;
}
