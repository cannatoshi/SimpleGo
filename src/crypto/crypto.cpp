/**
 * SimpleGo - Crypto Implementation
 * wolfSSL backend for ESP32
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "crypto.h"
#include "config.h"

#include <wolfssl/wolfcrypt/settings.h>
#include <wolfssl/wolfcrypt/curve448.h>
#include <wolfssl/wolfcrypt/ed25519.h>
#include <wolfssl/wolfcrypt/chacha20_poly1305.h>
#include <wolfssl/wolfcrypt/random.h>
#include <string.h>

// Global RNG instance
static WC_RNG rng;
static int rng_initialized = 0;

// ==================== INIT ====================

int crypto_init(void) {
    if (rng_initialized) return CRYPTO_OK;
    
    if (wc_InitRng(&rng) != 0) {
        return CRYPTO_ERR_RNG;
    }
    
    rng_initialized = 1;
    return CRYPTO_OK;
}

void crypto_cleanup(void) {
    if (rng_initialized) {
        wc_FreeRng(&rng);
        rng_initialized = 0;
    }
}

// ==================== X448 ====================

int x448_keypair(uint8_t pk[X448_KEY_SIZE], uint8_t sk[X448_KEY_SIZE]) {
    curve448_key key;
    int ret;
    
    ret = wc_curve448_init(&key);
    if (ret != 0) return CRYPTO_ERR_KEYGEN;
    
    ret = wc_curve448_make_key(&rng, X448_KEY_SIZE, &key);
    if (ret != 0) {
        wc_curve448_free(&key);
        return CRYPTO_ERR_KEYGEN;
    }
    
    word32 pk_len = X448_KEY_SIZE;
    word32 sk_len = X448_KEY_SIZE;
    
    ret = wc_curve448_export_key_raw(&key, sk, &sk_len, pk, &pk_len);
    wc_curve448_free(&key);
    
    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERR_KEYGEN;
}

int x448_shared_secret(uint8_t shared[X448_SHARED_SIZE],
                       const uint8_t sk[X448_KEY_SIZE],
                       const uint8_t peer_pk[X448_KEY_SIZE]) {
    curve448_key priv, pub;
    int ret;
    
    // Import private key
    ret = wc_curve448_init(&priv);
    if (ret != 0) return CRYPTO_ERR_DH;
    
    ret = wc_curve448_import_private_raw(sk, X448_KEY_SIZE, NULL, 0, &priv);
    if (ret != 0) {
        wc_curve448_free(&priv);
        return CRYPTO_ERR_DH;
    }
    
    // Import peer public key
    ret = wc_curve448_init(&pub);
    if (ret != 0) {
        wc_curve448_free(&priv);
        return CRYPTO_ERR_DH;
    }
    
    ret = wc_curve448_import_public_raw(peer_pk, X448_KEY_SIZE, &pub);
    if (ret != 0) {
        wc_curve448_free(&priv);
        wc_curve448_free(&pub);
        return CRYPTO_ERR_DH;
    }
    
    // Compute shared secret
    word32 shared_len = X448_SHARED_SIZE;
    ret = wc_curve448_shared_secret(&priv, &pub, shared, &shared_len);
    
    wc_curve448_free(&priv);
    wc_curve448_free(&pub);
    
    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERR_DH;
}

// ==================== ED25519 ====================

int ed25519_keypair(uint8_t pk[ED25519_PK_SIZE], uint8_t sk[ED25519_SK_SIZE]) {
    ed25519_key key;
    int ret;
    
    ret = wc_ed25519_init(&key);
    if (ret != 0) return CRYPTO_ERR_KEYGEN;
    
    ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE, &key);
    if (ret != 0) {
        wc_ed25519_free(&key);
        return CRYPTO_ERR_KEYGEN;
    }
    
    word32 pk_len = ED25519_PK_SIZE;
    word32 sk_len = ED25519_SK_SIZE;
    
    ret = wc_ed25519_export_key(&key, sk, &sk_len, pk, &pk_len);
    wc_ed25519_free(&key);
    
    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERR_KEYGEN;
}

int ed25519_sign(uint8_t sig[ED25519_SIG_SIZE],
                 const uint8_t sk[ED25519_SK_SIZE],
                 const uint8_t *msg, size_t msg_len) {
    ed25519_key key;
    int ret;
    
    ret = wc_ed25519_init(&key);
    if (ret != 0) return CRYPTO_ERR_SIGN;
    
    ret = wc_ed25519_import_private_key(sk, ED25519_KEY_SIZE,
                                        sk + ED25519_KEY_SIZE, ED25519_PK_SIZE,
                                        &key);
    if (ret != 0) {
        wc_ed25519_free(&key);
        return CRYPTO_ERR_SIGN;
    }
    
    word32 sig_len = ED25519_SIG_SIZE;
    ret = wc_ed25519_sign_msg(msg, msg_len, sig, &sig_len, &key);
    wc_ed25519_free(&key);
    
    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERR_SIGN;
}

int ed25519_verify(const uint8_t sig[ED25519_SIG_SIZE],
                   const uint8_t pk[ED25519_PK_SIZE],
                   const uint8_t *msg, size_t msg_len) {
    ed25519_key key;
    int ret, verified = 0;
    
    ret = wc_ed25519_init(&key);
    if (ret != 0) return CRYPTO_ERR_VERIFY;
    
    ret = wc_ed25519_import_public(pk, ED25519_PK_SIZE, &key);
    if (ret != 0) {
        wc_ed25519_free(&key);
        return CRYPTO_ERR_VERIFY;
    }
    
    ret = wc_ed25519_verify_msg(sig, ED25519_SIG_SIZE, msg, msg_len,
                                 &verified, &key);
    wc_ed25519_free(&key);
    
    if (ret != 0) return CRYPTO_ERR_VERIFY;
    return verified ? CRYPTO_OK : CRYPTO_ERR_VERIFY;
}

// ==================== CHACHA20-POLY1305 ====================

int chacha20poly1305_encrypt(uint8_t *ciphertext,
                             uint8_t tag[POLY1305_TAG_SIZE],
                             const uint8_t key[CHACHA20_KEY_SIZE],
                             const uint8_t nonce[CHACHA20_NONCE_SIZE],
                             const uint8_t *plaintext, size_t pt_len,
                             const uint8_t *aad, size_t aad_len) {
    int ret = wc_ChaCha20Poly1305_Encrypt(
        key, nonce,
        aad, aad_len,
        plaintext, pt_len,
        ciphertext, tag
    );
    
    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERR_ENCRYPT;
}

int chacha20poly1305_decrypt(uint8_t *plaintext,
                             const uint8_t key[CHACHA20_KEY_SIZE],
                             const uint8_t nonce[CHACHA20_NONCE_SIZE],
                             const uint8_t *ciphertext, size_t ct_len,
                             const uint8_t tag[POLY1305_TAG_SIZE],
                             const uint8_t *aad, size_t aad_len) {
    int ret = wc_ChaCha20Poly1305_Decrypt(
        key, nonce,
        aad, aad_len,
        ciphertext, ct_len,
        tag, plaintext
    );
    
    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERR_DECRYPT;
}

// ==================== UTILITIES ====================

int crypto_random(uint8_t *buf, size_t len) {
    int ret = wc_RNG_GenerateBlock(&rng, buf, len);
    return (ret == 0) ? CRYPTO_OK : CRYPTO_ERR_RNG;
}

void crypto_wipe(void *buf, size_t len) {
    volatile uint8_t *p = (volatile uint8_t *)buf;
    while (len--) {
        *p++ = 0;
    }
}
