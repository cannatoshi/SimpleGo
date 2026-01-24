# SimpleGo Cryptography Documentation

## Overview

| Component | Algorithm | Library |
|-----------|-----------|---------|
| Key Agreement | X3DH | Custom |
| DH Ratchet | X448 | wolfSSL |
| KDF | HKDF-SHA512 | mbedTLS |
| Encryption | AES-256-GCM | mbedTLS |

## X3DH Key Agreement

dh1 = X448_DH(sk1, spk1)
dh2 = X448_DH(sk1, rk1)
dh3 = X448_DH(rpk1_priv, spk1)
ikm = dh1 || dh2 || dh3 (168 bytes)

HKDF: salt=64x0x00, info="SimpleXX3DH", output=96 bytes
- header_key = output[0:32]
- next_header_key = output[32:64]
- root_key = output[64:96]

## Double Ratchet KDFs

Root KDF: salt=root_key, ikm=dh_output, info="SimpleXRootRatchet"
Chain KDF: salt=empty, ikm=chain_key, info="SimpleXChainRatchet"
- msg_key[0:32], new_chain[32:64], header_iv[64:80], msg_iv[80:96]

## Verification: 100% Python Match

*Last updated: January 24, 2026*
