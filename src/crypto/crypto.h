/**
 * SimpleGo - Crypto Interface
 * Unified API for all cryptographic operations
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SIMPLEGO_CRYPTO_H
#define SIMPLEGO_CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// ===== Key Sizes =====
#define X448_KEY_SIZE       56
#define X448_SHARED_SIZE    56
#define ED25519_SK_SIZE     64
#define ED25519_PK_SIZE     32
#define ED25519_SIG_SIZE    64
#define CHACHA20_KEY_SIZE   32
#define CHACHA20_NONCE_SIZE 12
#define POLY1305_TAG_SIZE   16

// ===== Error Codes =====
#define CRYPTO_OK           0
#define CRYPTO_ERR_KEYGEN   -1
#define CRYPTO_ERR_DH       -2
#define CRYPTO_ERR_SIGN     -3
#define CRYPTO_ERR_VERIFY   -4
#define CRYPTO_ERR_ENCRYPT  -5
#define CRYPTO_ERR_DECRYPT  -6
#define CRYPTO_ERR_RNG      -7

#ifdef __cplusplus
extern "C" {
#endif

// ===== Initialization =====
int crypto_init(void);
void crypto_cleanup(void);

// ===== X448 Key Exchange =====
int x448_keypair(uint8_t pk[X448_KEY_SIZE], uint8_t sk[X448_KEY_SIZE]);
int x448_shared_secret(uint8_t shared[X448_SHARED_SIZE],
                       const uint8_t sk[X448_KEY_SIZE],
                       const uint8_t peer_pk[X448_KEY_SIZE]);

// ===== Ed25519 Signatures =====
int ed25519_keypair(uint8_t pk[ED25519_PK_SIZE], uint8_t sk[ED25519_SK_SIZE]);
int ed25519_sign(uint8_t sig[ED25519_SIG_SIZE],
                 const uint8_t sk[ED25519_SK_SIZE],
                 const uint8_t *msg, size_t msg_len);
int ed25519_verify(const uint8_t sig[ED25519_SIG_SIZE],
                   const uint8_t pk[ED25519_PK_SIZE],
                   const uint8_t *msg, size_t msg_len);

// ===== ChaCha20-Poly1305 AEAD =====
int chacha20poly1305_encrypt(uint8_t *ciphertext,
                             uint8_t tag[POLY1305_TAG_SIZE],
                             const uint8_t key[CHACHA20_KEY_SIZE],
                             const uint8_t nonce[CHACHA20_NONCE_SIZE],
                             const uint8_t *plaintext, size_t pt_len,
                             const uint8_t *aad, size_t aad_len);

int chacha20poly1305_decrypt(uint8_t *plaintext,
                             const uint8_t key[CHACHA20_KEY_SIZE],
                             const uint8_t nonce[CHACHA20_NONCE_SIZE],
                             const uint8_t *ciphertext, size_t ct_len,
                             const uint8_t tag[POLY1305_TAG_SIZE],
                             const uint8_t *aad, size_t aad_len);

// ===== Utilities =====
int crypto_random(uint8_t *buf, size_t len);
void crypto_wipe(void *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // SIMPLEGO_CRYPTO_H
