/**
 * SimpleGo - smp_ratchet.h
 * Double Ratchet Encryption with CORRECT Wire Format
 * v0.1.22-alpha - Updated 2026-02-03
 * 
 * CRITICAL: rcAD = initiator_pub || responder_pub (ABSOLUTE order!)
 * We are RESPONDER, peer is INITIATOR
 * Therefore: rcAD = peer_key1 || our_key1
 */

#ifndef SMP_RATCHET_H
#define SMP_RATCHET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "smp_x448.h"

// ============== Constants ==============

#define RATCHET_KEY_LEN         32
#define RATCHET_IV_LEN          16
#define RATCHET_TAG_LEN         16
#define RATCHET_HEADER_LEN      88
#define X448_PUBLIC_KEY_LEN     56

// ============== Ratchet State ==============

typedef struct {
    // Root key (shared secret from X3DH)
    uint8_t root_key[RATCHET_KEY_LEN];
    
    // Header keys (for encrypting/decrypting message headers)
    uint8_t header_key_send[RATCHET_KEY_LEN];
    uint8_t header_key_recv[RATCHET_KEY_LEN];
    
    // Chain keys (for deriving message keys)
    uint8_t chain_key_send[RATCHET_KEY_LEN];
    uint8_t chain_key_recv[RATCHET_KEY_LEN];
    
    // DH keypairs
    x448_keypair_t dh_self;                // Our current DH keypair
    uint8_t        dh_peer[X448_PUBLIC_KEY_LEN];  // Peer's current DH public key
    
    // Message counters
    uint32_t msg_num_send;           // Messages sent in current chain
    uint32_t msg_num_recv;           // Messages received in current chain
    uint32_t prev_chain_len;         // Length of previous sending chain
    
    // State flags
    bool initialized;
    
    // Associated Data for AEAD
    // CRITICAL FIX v0.1.22: rcAD = initiator_key1 || responder_key1
    // Since we are RESPONDER and peer is INITIATOR:
    //   assoc_data = peer_key1 (56 bytes) || our_key1 (56 bytes)
    // This is ABSOLUTE (role-based), NOT relative (our/peer)!
    uint8_t assoc_data[112];         // 56 + 56 bytes = initiator || responder

} ratchet_state_t;

// ============== X3DH Key Agreement ==============

/**
 * Perform X3DH key agreement as sender (initiator)
 * This establishes the initial root key for the ratchet
 * 
 * @param peer_key1           Peer's X448 identity key (56 bytes) - INITIATOR
 * @param peer_key2           Peer's X448 signed prekey (56 bytes)
 * @param our_key1            Our X448 identity keypair - RESPONDER
 * @param our_key2            Our ephemeral X448 keypair (used for DH)
 * @return true on success
 */
bool ratchet_x3dh_sender(const uint8_t *peer_key1,
                         const uint8_t *peer_key2,
                         const x448_keypair_t *our_key1,
                         const x448_keypair_t *our_key2);

// ============== Ratchet Initialization ==============

/**
 * Initialize the ratchet for sending (after X3DH)
 * 
 * @param peer_dh_public  Peer's initial DH public key (56 bytes)
 * @param our_key2        Our ephemeral keypair used in X3DH
 * @return true on success
 */
bool ratchet_init_sender(const uint8_t *peer_dh_public, 
                        const x448_keypair_t *our_key2);

// ============== Encryption/Decryption ==============

/**
 * Encrypt a message using the Double Ratchet
 * 
 * Output format (Version 2, non-PQ):
 *   [0]       emHeader-len = 0x7B (123)
 *   [1-123]   emHeader (123 bytes)
 *   [124-139] payload AuthTag (16 bytes)
 *   [140-...] encrypted payload
 * 
 * emHeader structure:
 *   [0-1]     ehVersion (Word16 BE) = 2
 *   [2-17]    ehIV (16 bytes)
 *   [18-33]   ehAuthTag (16 bytes)
 *   [34]      ehBody-len = 0x58 (88)
 *   [35-122]  ehBody (88 bytes encrypted MsgHeader)
 * 
 * @param plaintext       Input data to encrypt
 * @param pt_len          Length of plaintext
 * @param output          Output buffer (must be large enough: pt_len + ~150 bytes)
 * @param out_len         Output: actual length written
 * @param padded_msg_len  Target padded message length
 * @return 0 on success, negative on error
 */
int ratchet_encrypt(const uint8_t *plaintext, size_t pt_len,
                    uint8_t *output, size_t *out_len,
                    size_t padded_msg_len);

/**
 * Decrypt a message using the Double Ratchet
 * 
 * @param ciphertext  Encrypted data (EncRatchetMessage format)
 * @param ct_len      Length of ciphertext
 * @param plaintext   Output buffer for decrypted message
 * @param pt_len      Output: actual length written
 * @return 0 on success, negative on error
 */
int ratchet_decrypt(const uint8_t *ciphertext, size_t ct_len,
                    uint8_t *plaintext, size_t *pt_len);

/**
 * Self-decrypt test (for debugging)
 * Attempts to decrypt a message we just encrypted (header only)
 * Note: This is expected to fail because sender uses HKs but receiver needs HKr
 */
int ratchet_self_decrypt_test(const uint8_t *ciphertext, size_t ct_len,
                              uint8_t *plaintext, size_t *pt_len);

// ============== State Access / Debug ==============

/**
 * Get pointer to global ratchet state (for debugging/inspection/persistence)
 */
ratchet_state_t *ratchet_get_state(void);

/**
 * Check if ratchet has been initialized
 */
bool ratchet_is_initialized(void);

#endif // SMP_RATCHET_H
