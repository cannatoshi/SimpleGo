/**
 * SimpleGo - Reply Queue E2E Decryption
 * Extracted from main.c (Auftrag 46c)
 *
 * Handles the full Reply Queue message decrypt pipeline:
 * Server-level decrypt → Envelope parse → E2E decrypt
 *
 * Consolidates duplicate code from:
 * - Main Reply Queue message handler
 * - 42d secondary Reply Queue read
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * Decrypt a Reply Queue message (both server + E2E layers).
 *
 * Pipeline:
 * 1. Server-level decrypt (crypto_box with our_queue.shared_secret)
 * 2. Length prefix handling (Reply Queue specific)
 * 3. Envelope parse: maybe_corrId, maybe_e2e, sender key extraction
 * 4. Multi-method E2E decrypt (decrypt_client_msg, crypto_box, secretbox, simplex)
 *
 * Updates reply_queue_e2e_peer_public/valid when key extracted from SPKI.
 *
 * @param encrypted      Server-encrypted data (from MSG command)
 * @param encrypted_len  Length of encrypted data
 * @param msg_id         SMP message ID (used as server nonce)
 * @param msg_id_len     Length of msg_id
 * @param out_plain      OUT: allocated plaintext buffer (caller must free!)
 * @param out_plain_len  OUT: length of decrypted data (includes 2-byte unPad prefix)
 * @return 0 on success, negative on error:
 *         -1 = malloc failed
 *         -2 = server decrypt failed
 *         -3 = no sender key found
 *         -4 = message too short
 *         -5 = E2E decrypt failed (all methods)
 */
int smp_e2e_decrypt_reply_message(
    const uint8_t *encrypted, int encrypted_len,
    const uint8_t *msg_id, int msg_id_len,
    uint8_t **out_plain, size_t *out_plain_len);
