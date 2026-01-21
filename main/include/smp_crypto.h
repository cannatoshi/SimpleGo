/**
 * SimpleGo - smp_crypto.h
 * Cryptographic functions for SMP protocol
 */

#ifndef SMP_CRYPTO_H
#define SMP_CRYPTO_H

#include <stdint.h>
#include <stdbool.h>
#include "smp_types.h"

// Decrypt SMP message (Layer 3 - server DH)
bool decrypt_smp_message(contact_t *c, const uint8_t *encrypted, int enc_len,
                         const uint8_t *nonce, uint8_t nonce_len,
                         uint8_t *plain, int *plain_len);

// Decrypt client message (Layer 5 - contact DH)
int decrypt_client_msg(const uint8_t *enc, int enc_len,
                       const uint8_t *sender_dh_pub,
                       const uint8_t *our_dh_priv,
                       uint8_t *plain);

// Encrypt message for peer
int encrypt_for_peer(const uint8_t *plain, int plain_len,
                     const uint8_t *peer_dh_pub,
                     const uint8_t *our_dh_priv,
                     uint8_t *encrypted);

#endif // SMP_CRYPTO_H
