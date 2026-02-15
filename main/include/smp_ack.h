/**
 * SimpleGo - SMP ACK Command
 * Consolidated ACK sending for all queue types
 *
 * Phase 3: Split into build + send for task architecture.
 * - smp_build_ack()  → App Task builds the signed ACK frame
 * - smp_send_ack()   → Legacy: builds AND sends (for non-task code)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "mbedtls/ssl.h"

/**
 * Build ACK transport frame WITHOUT sending.
 * Used by App Task to prepare ACK for Network Task.
 *
 * @param out           Output buffer (must be >= 192 bytes)
 * @param out_size      Size of output buffer
 * @param session_id    32-byte TLS session ID
 * @param recipient_id  Queue recipient ID
 * @param recipient_id_len  Length of recipient_id
 * @param msg_id        Message ID to acknowledge
 * @param msg_id_len    Length of msg_id
 * @param rcv_auth_secret  64-byte Ed25519 secret key for signing
 * @return Length of built transport frame, or -1 on error
 */
int smp_build_ack(uint8_t *out, size_t out_size,
                  const uint8_t *session_id,
                  const uint8_t *recipient_id, int recipient_id_len,
                  const uint8_t *msg_id, int msg_id_len,
                  const uint8_t *rcv_auth_secret);

/**
 * Send ACK for a received message on any queue (legacy).
 * Builds and sends in one call — used by code that owns ssl directly.
 */
bool smp_send_ack(mbedtls_ssl_context *ssl, uint8_t *block,
                  const uint8_t *session_id,
                  const uint8_t *recipient_id, int recipient_id_len,
                  const uint8_t *msg_id, int msg_id_len,
                  const uint8_t *rcv_auth_secret);
