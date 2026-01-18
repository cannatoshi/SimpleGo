/**
 * SimpleGo - Crypto Configuration
 * wolfSSL settings for ESP32
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef SIMPLEGO_CRYPTO_CONFIG_H
#define SIMPLEGO_CRYPTO_CONFIG_H

// ===== wolfSSL ESP32 Settings =====
#define WOLFSSL_ESP32
#define WOLFSSL_ESPIDF

// ===== Enable required algorithms =====
#define HAVE_CURVE448          // X448 DH - SimpleX requirement!
#define HAVE_ED448             // Ed448 signatures (optional)
#define HAVE_CURVE25519        // X25519 DH (fallback)
#define HAVE_ED25519           // Ed25519 signatures
#define HAVE_CHACHA            // ChaCha20 stream cipher
#define HAVE_POLY1305          // Poly1305 MAC
#define HAVE_AEAD              // AEAD modes

// ===== Performance tuning =====
#define WOLFSSL_SMALL_STACK    // Save stack space
#define CURVED448_SMALL        // Smaller Curve448 implementation
#define ED448_SMALL

// ===== Disable unused features =====
#define NO_RSA
#define NO_DSA
#define NO_DH                  // We use X448, not classic DH
#define NO_RC4
#define NO_MD4
#define NO_MD5
#define NO_SHA                 // We only need SHA-512

// ===== TLS for SMP connection =====
#define WOLFSSL_TLS13
#define HAVE_TLS_EXTENSIONS
#define HAVE_SNI

// ===== Debug (disable in production) =====
// #define DEBUG_WOLFSSL

#endif // SIMPLEGO_CRYPTO_CONFIG_H
