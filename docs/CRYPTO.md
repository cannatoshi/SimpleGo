# SimpleGo Cryptography Documentation

> Complete cryptography specification for SimpleX Double Ratchet implementation

---

## Overview

SimpleX uses the Double Ratchet algorithm with X3DH key agreement:

| Component | Algorithm | Library (ESP32) |
|-----------|-----------|-----------------|
| Key Agreement | X3DH | Custom implementation |
| DH Ratchet | X448 (Curve448) | wolfSSL |
| KDF | HKDF-SHA512 | mbedTLS |
| Encryption | AES-256-GCM | mbedTLS |
| Per-Queue E2E | X25519 | libsodium |
| Signatures | Ed25519 | libsodium |

---

## X3DH Key Agreement

Extended Triple Diffie-Hellman for initial key establishment.

### Keys Involved

| Key | Owner | Type | Size | Purpose |
|-----|-------|------|------|---------|
| spk1 | Peer (App) | X448 Public | 56 bytes | Semi-permanent key |
| rk1 | Peer (App) | X448 Public | 56 bytes | Ratchet public key |
| sk1 | Us (ESP32) | X448 Private | 56 bytes | Our ephemeral secret |
| rpk1 | Us (ESP32) | X448 Public | 56 bytes | Our ratchet public |

### DH Calculations (Sender Side)
```
// Three DH operations for X3DH
dh1 = X448_DH(sk1, spk1);   // Our ephemeral x Peer's semi-permanent
dh2 = X448_DH(sk1, rk1);    // Our ephemeral x Peer's ratchet
dh3 = X448_DH(rpk1_priv, spk1);  // Our ratchet x Peer's semi-permanent

// Concatenate for HKDF input
ikm = dh1 || dh2 || dh3;    // 168 bytes (56 x 3)
```

### HKDF Derivation
```
// HKDF parameters
salt = 0x00 x 64;           // 64 zero bytes
ikm = dh1 || dh2 || dh3;    // 168 bytes
info = "SimpleXX3DH";       // 11 ASCII bytes

// Derive 96 bytes
output = HKDF-SHA512(salt, ikm, info, 96);

// Split output
hk  = output[0:32];         // Header key (encrypt headers)
nhk = output[32:64];        // Next header key
rk  = output[64:96];        // Root key (for ratchet)
```
