# Quick Reference

## Constants, Wire Formats, and KDF Parameters

This document provides a quick reference for all the technical details needed when working with SimpleGo.

**Updated: 2026-01-28 - Session 10C (Reply Queue Per-Queue DH Analysis)**

---

## Current Status

```
Session 10C:
- Contact Queue Decrypt: WORKING (3-layer)
- Reply Queue Server-Level: WORKING
- Reply Queue Per-Queue DH: ALL TESTS FAIL (Bug #17)
- Developer question required
```

---

## Table of Contents

1. [Version Numbers](#1-version-numbers)
2. [Size Constants](#2-size-constants)
3. [Wire Formats](#3-wire-formats)
4. [KDF Parameters](#4-kdf-parameters)
5. [SPKI Key Formats](#5-spki-key-formats)
6. [Encoding Rules](#6-encoding-rules)
7. [Maybe Encoding](#7-maybe-encoding)
8. [NaCl Crypto Layers](#8-nacl-crypto-layers)
9. [Reply Queue Structure](#9-reply-queue-structure)

---

## 1. Version Numbers

### 1.1 Protocol Versions

| Protocol | Our Value | Valid Range | Hex |
|----------|-----------|-------------|-----|
| SMP Client (phVer) | 4 | 1-4 | 0x00 0x04 |
| Agent (agentVersion) | 7 | 2-7 | 0x00 0x07 |
| E2E (e2eVersion) | 2 | 2-3 | 0x00 0x02 |
| EncHeader (ehVersion) | 2 | - | 0x00 0x02 |
| MsgHeader (msgMaxVersion) | 2 | - | 0x00 0x02 |

### 1.2 Version Constants in Code
```c
#define SMP_CLIENT_VERSION      4
#define AGENT_VERSION           7
#define E2E_VERSION             2
#define EH_VERSION              2
#define MSG_HEADER_VERSION      2
```

---

## 2. Size Constants

### 2.1 Structure Sizes

| Structure | Size (bytes) | Notes |
|-----------|--------------|-------|
| EncMessageHeader | 123 | NOT 124! |
| MsgHeader (plaintext) | 88 | With padding |
| MsgHeader (content) | 79 | Without padding |
| HELLO (plaintext) | 12 | Minimal message |
| E2E Params | 140 | 2 X448 keys |
| X448 SPKI Key | 68 | 12 header + 56 raw |
| X25519 SPKI Key | 44 | 12 header + 32 raw |
| Ed25519 SPKI Key | 44 | 12 header + 32 raw |
| X448 Raw Key | 56 | |
| X25519 Raw Key | 32 | |
| AES-GCM IV | 16 | |
| AES-GCM Tag | 16 | |
| Poly1305 Tag | 16 | crypto_box_MACBYTES |

### 2.2 Padding Sizes

| Message Type | Padded Size |
|--------------|-------------|
| AgentConfirmation (encConnInfo) | 14,832 bytes |
| HELLO / A_MSG | 15,840 bytes |
| ClientMessage (outer) | 15,904 bytes |

### 2.3 AAD Sizes

| AAD Type | Size | Composition |
|----------|------|-------------|
| Header AAD (rcAD) | 112 | 56 + 56 raw keys |
| **Payload AAD** | **235** | **112 + 123 (rcAD + emHeader, NO prefix!)** |

---

## 3. Wire Formats

### 3.1 EncRatchetMessage
```
  +---------------+-----------------+----------------+-------------------+
  | emHeader-len  | emHeader        | Payload AuthTag| Encrypted Payload |
  | (1 byte)      | (123 bytes)     | (16 bytes)     | (Tail-no prefix)  |
  | 0x7B          | [EncMsgHeader]  | [tag]          | [encrypted]       |
  +---------------+-----------------+----------------+-------------------+
```

### 3.2 EncMessageHeader (123 bytes)
```
  +-----------+--------+------------+-------------+------------------+
  | ehVersion | ehIV   | ehAuthTag  | ehBody-len  | ehBody           |
  | (2 bytes) | (16 B) | (16 bytes) | (1 byte)    | (88 bytes)       |
  | 00 02     | [iv]   | [tag]      | 58          | [encrypted hdr]  |
  +-----------+--------+------------+-------------+------------------+
  Total: 2 + 16 + 16 + 1 + 88 = 123 bytes
```

### 3.3 MsgHeader (88 bytes, plaintext)
```
  +------------+-------------+------------------+----------+----------+---------+
  | Word16 len | msgMaxVer   | msgDHRs          | msgPN    | msgNs    | Padding |
  | (2 bytes)  | (2 bytes)   | (1+68 bytes)     | (4 bytes)| (4 bytes)| (7 B)   |
  | 00 4F      | 00 02       | 44 [SPKI]        | [PN BE]  | [Ns BE]  | '#####' |
  +------------+-------------+------------------+----------+----------+---------+
  Total: 2 + 2 + 1 + 68 + 4 + 4 + 7 = 88 bytes
```

### 3.4 crypto_box Wire Format
```
  +------------------+------------------+
  | Poly1305 Tag     | Ciphertext       |
  | (16 bytes)       | (variable)       |
  +------------------+------------------+
```

---

## 4. KDF Parameters

### 4.1 X3DH Key Derivation
```
X3DH:
  Hash:   SHA512
  Salt:   64 zero bytes
  IKM:    DH1 || DH2 || DH3 (168 bytes)
  Info:   "SimpleXX3DH" (11 bytes)
  Output: 96 bytes
    [0:32]   header_key (hk)
    [32:64]  next_header_key (nhk)
    [64:96]  root_key (sk)
```

### 4.2 Root KDF
```
Root KDF:
  Hash:   SHA512
  Salt:   current root_key (32 bytes)
  IKM:    DH output (56 bytes)
  Info:   "SimpleXRootRatchet" (18 bytes)
  Output: 96 bytes
    [0:32]   new_root_key
    [32:64]  chain_key
    [64:96]  next_header_key
```

### 4.3 Chain KDF
```
Chain KDF:
  Hash:   SHA512
  Salt:   EMPTY (0 bytes!)
  IKM:    chain_key (32 bytes)
  Info:   "SimpleXChainRatchet" (19 bytes)
  Output: 96 bytes
    [0:32]   next_chain_key
    [32:64]  message_key
    [64:80]  MESSAGE_IV (iv1)  <-- FOR PAYLOAD!
    [80:96]  HEADER_IV (iv2)   <-- FOR HEADER!
```

---

## 5. SPKI Key Formats

### 5.1 X448 SPKI (68 bytes)
```
Header (12 bytes):
  30 42 30 05 06 03 2b 65 6f 03 39 00

+ Raw key (56 bytes)
```

### 5.2 X25519 SPKI (44 bytes)
```
Header (12 bytes):
  30 2a 30 05 06 03 2b 65 6e 03 21 00

+ Raw key (32 bytes)
```

### 5.3 Ed25519 SPKI (44 bytes)
```
Header (12 bytes):
  30 2a 30 05 06 03 2b 65 70 03 21 00

+ Raw key (32 bytes)
```

---

## 6. Encoding Rules

### 6.1 Length Encoding

| Size | Encoding |
|------|----------|
| <= 254 bytes | 1-byte length prefix |
| > 254 bytes | 0xFF + Word16 BE prefix |
| Tail field | NO prefix! |

### 6.2 Word16 (Big Endian)
```
Value 68:  0x00 0x44
Value 79:  0x00 0x4F
Value 88:  0x00 0x58
Value 123: 0x00 0x7B
```

---

## 7. Maybe Encoding

### 7.1 Standard Maybe
```
Nothing:  '0' (0x30)
Just x:   '1' (0x31) + encoded x
```

### 7.2 Special Cases
```
queueMode (NOT standard Maybe!):
  Nothing:              (empty - zero bytes!)
  Just QMMessaging:     "M"
  Just QMSubscription:  "S"
```

---

## 8. NaCl Crypto Layers

### 8.1 The Three Layers

```
+-------------------------------------------------------------+
|                     NaCl crypto_box                         |
+-------------------------------------------------------------+
|  1. X25519 DH:       scalarmult(sk, pk) -> raw_secret       |
|  2. HSalsa20:        derive(raw_secret) -> box_key          |
|  3. XSalsa20-Poly1305: encrypt(box_key, nonce, msg)         |
+-------------------------------------------------------------+
|  crypto_scalarmult:    Only step 1                          |
|  crypto_box_beforenm:  Steps 1 + 2 (returns box_key)        |
|  crypto_box_easy:      All steps in one call                |
+-------------------------------------------------------------+
```

### 8.2 Critical Rule
**Always use the same crypto primitive chain as the sender!**

---

## 9. Reply Queue Structure (Session 10C)

### 9.1 Three-Layer Encryption Model

```
+-------------------------------------------------------------+
|  Layer 1: Server-Level XSalsa20-Poly1305                    |
|           Key: shared_secret (srv_dh_public + rcv_dh_private)|
|           Nonce: msgId (24 bytes)                           |
|           Status: WORKING                                   |
+-------------------------------------------------------------+
|  Layer 2: Per-Queue DH (NaCl crypto_box)                    |
|           Key: ??? (all tested combinations FAIL)           |
|           Status: BLOCKED                                   |
+-------------------------------------------------------------+
|  Layer 3: Double Ratchet (AES-GCM)                          |
|           Status: Blocked by Layer 2                        |
+-------------------------------------------------------------+
```

### 9.2 Structure After Server-Level Decrypt

```
Offset  Bytes           Meaning
------  -----           -------
0-1     3e 82           Length prefix: 16002
2-5     00 00 00 00     Unknown (Padding?)
6-9     69 7a 0c 8d     Timestamp
10-13   54 20 00 04     Unknown
14-15   31 2c           Version "1," (ASCII)
16-59   30 2a 30 05...  X25519 SPKI (44 bytes)
60+     ???             Ciphertext (Layer 2 or Double Ratchet?)
```

### 9.3 Tested Key Combinations (ALL FAILED)

| Test | Key | Nonce | Result |
|------|-----|-------|--------|
| 1 | peer_ephemeral + rcv_dh_private | msgId | FAILED |
| 2 | srv_dh_public + rcv_dh_private | msgId | FAILED |
| 3 | shared_secret (direct) | message_nonce | FAILED |
| 4 | Direct on raw data | - | FAILED |

### 9.4 our_queue_t Structure

```c
typedef struct {
    bool valid;
    uint8_t rcv_id[24];
    uint8_t snd_id[24];
    uint8_t srv_dh_public[32];    // From server IDS
    uint8_t rcv_dh_public[32];    // Our public key
    uint8_t rcv_dh_private[32];   // Our private key
    uint8_t shared_secret[32];    // Precomputed
    uint8_t rcv_auth_public[32];
    uint8_t rcv_auth_secret[64];
} our_queue_t;
```

---

*Quick Reference v4.0*  
*Last updated: January 28, 2026 - Session 10C*
