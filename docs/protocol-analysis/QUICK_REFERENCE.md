# Quick Reference

## Constants, Wire Formats, and KDF Parameters

This document provides a quick reference for all the technical details needed when working with SimpleGo.

**Updated: 2026-01-30 - Session 11 (Regression & Recovery)**

---

## Current Status

```
Session 11 - RECOVERED:
- App status: "connecting"
- Contact Queue decrypt: WORKING (TEST4)
- Reply Queue decrypt: PENDING (Double Ratchet receiver needed)
- All format experiments reverted
- cmNonce fix (Bug #17) re-applied
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
9. [ClientMsgEnvelope Structure](#9-clientmsgenvelope-structure)
10. [Working Code State](#10-working-code-state)

---

## 1. Version Numbers

### 1.1 Protocol Versions (VERIFIED WORKING)

| Protocol | Our Value | Valid Range | Hex |
|----------|-----------|-------------|-----|
| SMP Client (phVer) | 4 | 1-4 | 0x00 0x04 |
| Agent (agentVersion) | 7 | 2-7 | 0x00 0x07 |
| E2E (e2eVersion) | 2 | 2-3 | 0x00 0x02 |
| EncHeader (ehVersion) | 2 | - | 0x00 0x02 |
| MsgHeader (msgMaxVersion) | 2 | - | 0x00 0x02 |
| **RATCHET_VERSION** | **2** | - | **0x00 0x02** |

**WARNING:** RATCHET_VERSION must be 2, not 3! Session 11 proved version 3 causes regression.

### 1.2 Version Constants in Code
```c
#define SMP_CLIENT_VERSION      4
#define AGENT_VERSION           7
#define E2E_VERSION             2
#define EH_VERSION              2
#define MSG_HEADER_VERSION      2
#define RATCHET_VERSION         2   // DO NOT CHANGE!
```

---

## 2. Size Constants

### 2.1 Structure Sizes (VERIFIED WORKING)

| Structure | Size (bytes) | Notes |
|-----------|--------------|-------|
| **EncMessageHeader** | **123** | **NOT 124! Session 11 verified** |
| MsgHeader (plaintext) | 88 | With padding |
| MsgHeader (content) | 79 | Without padding |
| HELLO (plaintext) | 12 | Minimal message |
| E2E Params | 140 | 2 X448 keys |
| X448 SPKI Key | 68 | 12 header + 56 raw |
| X25519 SPKI Key | 44 | 12 header + 32 raw |
| cmNonce | 24 | In ClientMsgEnvelope |

### 2.2 AAD Sizes

| AAD Type | Size | Composition |
|----------|------|-------------|
| Header AAD (rcAD) | 112 | 56 + 56 raw keys |
| **Payload AAD** | **235** | **112 + 123 (NO prefix!)** |

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

### 3.2 EncMessageHeader (123 bytes) - VERSION 2
```
  +-----------+--------+------------+-------------+------------------+
  | ehVersion | ehIV   | ehAuthTag  | ehBody-len  | ehBody           |
  | (2 bytes) | (16 B) | (16 bytes) | (1 byte!)   | (88 bytes)       |
  | 00 02     | [iv]   | [tag]      | 58          | [encrypted hdr]  |
  +-----------+--------+------------+-------------+------------------+
  Total: 2 + 16 + 16 + 1 + 88 = 123 bytes

  NOTE: ehBody-len is 1 BYTE for version 2! (not 2 bytes)
```

---

## 4. KDF Parameters

### 4.1 Chain KDF (VERIFIED)
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

---

## 6. Encoding Rules

### 6.1 Length Encoding

| Size | Encoding |
|------|----------|
| <= 254 bytes | 1-byte length prefix |
| > 254 bytes | 0xFF + Word16 BE prefix |
| Tail field | NO prefix! |

### 6.2 Word16 (Big Endian) - CRITICAL!
```c
// CORRECT:
output[offset++] = 0x00;              // HIGH byte
output[offset++] = value;             // LOW byte
// Result: 00 02 = Version 2

// WRONG (Session 11 mistake):
output[offset++] = value;             // First byte
output[offset++] = value;             // Second byte
// Result: 02 02 = Version 514!
```

---

## 7. Maybe Encoding (CRITICAL!)

### 7.1 Standard Maybe - ASCII Characters!
```c
// CORRECT:
agent_msg[amp++] = '1';   // ASCII 0x31 for Just
agent_msg[amp++] = '0';   // ASCII 0x30 for Nothing

// WRONG (Session 11 mistake):
agent_msg[amp++] = 0x01;  // Binary 1 - FAILS!
```

### 7.2 Haskell Reference
```haskell
instance Encoding a => Encoding (Maybe a) where
  smpEncode = maybe "0" (('1' `B.cons`) . smpEncode)
  -- Nothing = '0' (0x30)
  -- Just x  = '1' (0x31) + encoded x
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

---

## 9. ClientMsgEnvelope Structure (Bug #17 Fix)

### 9.1 Structure Layout
```
Offset  Size  Content
------  ----  -------
[0-1]   2     length prefix
[2-11]  10    padding/unknown
[12-13] 2     version
[14]    1     maybe tag
[15]    1     maybe tag for e2ePubKey
[16-59] 44    X25519 SPKI (e2ePubKey)
[60-83] 24    cmNonce <- CORRECT NONCE FOR DECRYPT!
[84+]   var   cmEncBody
```

### 9.2 Nonce Extraction (CRITICAL!)
```c
// WRONG - used msgId from MSG header
memcpy(nonce, msg_id, msgIdLen);  // This is server-level nonce!

// CORRECT - extract cmNonce from ClientMsgEnvelope
int spki_offset = 16;
int cm_nonce_offset = spki_offset + 44;  // = 60
memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);
```

---

## 10. Working Code State (Session 11 Verified)

### 10.1 smp_ratchet.c
```c
#define RATCHET_VERSION         2       // DO NOT CHANGE!
uint8_t em_header[123];                 // 123 bytes, NOT 124!
em_header[hp++] = 0x00; 
em_header[hp++] = RATCHET_VERSION;      // ehVersion (Word16 BE)
em_header[hp++] = 0x58;                 // ehBody-len = 88 (1 BYTE!)
output[p++] = 0x7B;                     // emHeader len = 123
```

### 10.2 smp_peer.c
```c
agent_msg[amp++] = '1';                 // ASCII '1' (0x31) NOT 0x01!
```

### 10.3 smp_x448.c
```c
output[offset++] = 0x00;                // HIGH byte first!
output[offset++] = params->version_min; // LOW byte = 0x02
// Result: 00 02 = Version 2
```

### 10.4 main.c (TEST4 - cmNonce Fix)
```c
// Extract cmNonce for per-queue E2E decryption
int cm_nonce_offset = spki_offset + 44;        // [60-83]
int cm_enc_body_offset = cm_nonce_offset + 24; // [84+]
memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);

// Decrypt with cmNonce (NOT msgId!)
crypto_box_open_easy_afternm(plain, &data[cm_enc_body_offset], 
                              enc_len, cm_nonce, dh_shared);
```

---

## Session 11 Anti-Patterns

**DO NOT repeat these mistakes:**

| Mistake | What Happened | Correct |
|---------|---------------|---------|
| RATCHET_VERSION = 3 | Regression | Keep at 2 |
| em_header[124] | Regression | Keep at 123 |
| Maybe = 0x01 | Regression | Use '1' (0x31) |
| Version 02 02 | Regression | Use 00 02 |
| DH order swap | Regression | Keep original |

**If app shows "connecting" - DON'T EXPERIMENT!**

---

*Quick Reference v5.0*  
*Last updated: January 30, 2026 - Session 11*
