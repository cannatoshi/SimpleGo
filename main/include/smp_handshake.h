/**
 * SimpleGo - smp_handshake.h
 * Connection Handshake Implementation
 * Based on official agent-protocol.md specification
 */

#ifndef SMP_HANDSHAKE_H
#define SMP_HANDSHAKE_H

#include <stdint.h>
#include <stdbool.h>
#include "mbedtls/ssl.h"
#include "smp_ratchet.h"
#include "smp_types.h"

/**
 * Send SKEY command to secure peer's queue
 * (must be called BEFORE AgentConfirmation)
 */
bool send_skey_command(
    mbedtls_ssl_context *ssl,
    uint8_t *block,
    const uint8_t *session_id,
    const uint8_t *peer_queue_id,
    int peer_queue_id_len,
    const uint8_t *our_auth_public
);

/**
 * Send HELLO message to complete the connection handshake.
 *
 * @param ssl               TLS context for peer connection
 * @param block             Work buffer (SMP_BLOCK_SIZE)
 * @param session_id        Current session ID (32 bytes)
 * @param peer_queue_id     Peer's queue ID
 * @param peer_queue_id_len Length of peer's queue ID
 * @param peer_dh_public    Peer's X25519 DH public key (32 bytes)
 * @param our_dh_private    Our X25519 DH private key (32 bytes)
 * @param our_dh_public     Our X25519 DH public key (32 bytes)
 * @param ratchet           Ratchet state for encryption
 * @return true on success
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
);

/**
 * Parse a decrypted message to check if it's HELLO.
 *
 * @param data       Decrypted message data
 * @param len        Length of data
 * @param msg_id_out Output: message ID (optional, can be NULL)
 * @return true if message is HELLO
 */
bool parse_hello_message(const uint8_t *data, int len, uint64_t *msg_id_out);

/**
 * Complete the duplex connection handshake after sending AgentConfirmation.
 *
 * Flow:
 * 1. (AgentConfirmation already sent)
 * 2. (Fast duplex: skip waiting for confirmation)
 * 3. Send HELLO message
 * 4. Wait for HELLO back (optional in fast mode)
 *
 * @param peer_ssl          TLS context for peer connection
 * @param block             Work buffer
 * @param peer_session_id   Peer's session ID
 * @param peer_queue_id     Peer's queue ID
 * @param peer_queue_id_len Length of peer queue ID
 * @param peer_dh_public    Peer's DH public key
 * @param our_dh_private    Our DH private key
 * @param our_dh_public     Our DH public key
 * @param ratchet           Ratchet state
 * @return true if handshake initiated successfully
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
);

// Status getters
bool is_handshake_complete(void);
bool is_hello_sent(void);
bool is_hello_received(void);
bool is_connected(void);
void reset_handshake_state(void);

#endif // SMP_HANDSHAKE_H
