# Quick Reference

## Constants, Wire Formats, Verified Values

**Updated: 2026-02-05 - Session 19 (Double Ratchet Header Decrypt SUCCESS!)**

---

## Current Status

```
SESSION 19 - DOUBLE RATCHET HEADER DECRYPT SUCCESS!
====================================================

Three new layers discovered:
  1. unPad Layer — [2B len][content][padding 0x23...]
  2. ClientMessage Layer — PrivHeader + AgentMsgEnvelope
  3. EncRatchetMessage Layer — Double Ratchet Header-Decrypt

MsgHeader fully parsed:
  - msgMaxVersion: 3 (Peer supports PQ)
  - DH Key: 68 bytes X448 SPKI
  - PN: 0, Ns: 0 (first message)

Key insight: nhk (HKDF[32-63]) = header_key_recv

Bug #19: header_key_recv overwritten (workaround: saved_nhk)

Next: Fix Bug #19, DH Ratchet Step, Body Decrypt
```

---

## Table of Contents

1. [Version Numbers](#1-version-numbers)
2. [Size Constants](#2-size-constants)
3. [Encoding Reference](#3-encoding-reference)
4. [Wire Formats](#4-wire-formats)
5. [HKDF Chain](#5-hkdf-chain)
6. [Verified Byte-Map](#6-verified-byte-map)
7. [Decryption Chain](#7-complete-decryption-chain)
8. [Crypto Functions](#8-crypto-functions)
9. [Working Code State](#9-working-code-state)
10. [Evgeny Quotes](#10-evgeny-quotes)

---

## 1. Version Numbers (VERIFIED)

| Protocol | Our Value | Hex |
|----------|-----------|-----|
| SMP Client | 4 | 0x00 0x04 |
| Agent | 7 | 0x00 0x07 |
| E2E | 2 | 0x00 0x02 |
| RATCHET_VERSION | **2** | **DO NOT CHANGE!** |

---

## 2. Size Constants (VERIFIED)

| Structure | Size | Notes |
|-----------|------|-------|
| EncMessageHeader | **123** | NOT 124! |
| MsgHeader | 88 | With padding |
| X448 SPKI | 68 | 12 header + 56 raw |
| X25519 SPKI | 44 | 12 header + 32 raw |
| cmNonce | 24 | In ClientMsgEnvelope |
| Poly1305 MAC | 16 | Authentication tag |
| AES-GCM AuthTag | 16 | Authentication tag |
| AES-GCM IV | **16** | NOT 12! SimpleX uses 16-byte IV |
| Payload AAD | **235** | NO prefix! |
| rcAD | 112 | our_key1 \|\| peer_key1 |

---

## 3. Encoding Reference (from Haskell Source, Verified Session 19)

| Primitive | Encoding | Source |
|-----------|----------|--------|
| Word16 | 2 Bytes Big-Endian | Encoding.hs:70-74 |
| Char | 1 Byte (B.singleton) | Encoding.hs:52-56 |
| ByteString | 1-Byte Len + Data | Encoding.hs:100-104 |
| Large | 2-Byte Word16 Len + Data | Encoding.hs:132-141 |
| Tail | Rest without length prefix | Encoding.hs:124-130 |
| **Maybe a** | **'0'=Nothing, '1'+data=Just** | Encoding.hs:114-122 |
| AuthTag | 16 Bytes raw (no prefix) | Crypto.hs:956-958 |
| IV | 16 Bytes raw | Crypto.hs:935-937 |
| PublicKey a | ByteString (1-Byte Len + X.509 DER) | Crypto.hs:567-568 |
| Tuple | Simple concatenation | Encoding.hs:184-212 |

### Maybe Encoding (CRITICAL - Session 19)

```
Maybe a:
  Nothing → 0x30 (ASCII '0') — 1 byte only!
  Just a  → 0x31 (ASCII '1') + smpEncode a

NOT binary 0x00/0x01!
```

### PrivHeader Encoding (Session 19)

From Protocol.hs:1093-1098:

| Tag | Hex | Constructor | Content After Tag |
|-----|-----|-------------|-------------------|
| `'K'` | 0x4B | PHConfirmation | 1-byte Len + SPKI Key |
| `'_'` | 0x5F | PHEmpty | (nothing) |

---

## 4. Wire Formats (Verified Session 19)

### 4.1 unPad Layer (NEW!)

```
[0..1]           originalLength (Word16 Big-Endian)
[2..1+origLen]   ClientMessage (actual content)
[2+origLen..]    Padding (0x23 = '#')
```

### 4.2 ClientMessage

```
ClientMessage = PrivHeader ++ Body (simple concatenation)
smpEncode (ClientMessage h msg) = smpEncode h <> msg
```

### 4.3 AgentConfirmation

```
smpEncode = (agentVersion, 'C', e2eEncryption_, Tail encConnInfo)

Fields:
  agentVersion    Word16 BE        2 bytes
  Tag 'C'         Char             1 byte
  e2eEncryption_  Maybe (...)      1+ bytes
  encConnInfo     Tail             rest
```

### 4.4 EncRatchetMessage

```
encodeEncRatchetMessage v msg =
  encodeLarge v emHeader <> smpEncode (emAuthTag, Tail emBody)

For v < 3 (legacy): encodeLarge = 1-byte length prefix
For v >= 3 (PQ):    encodeLarge = 2-byte Word16 length prefix

Structure (v < 3):
  emHeader Len    1 byte           = 123 (0x7B)
  emHeader        123 bytes        EncMessageHeader
  emAuthTag       16 bytes raw     AES-GCM Auth Tag
  emBody          Tail             rest (encrypted payload)
```

### 4.5 EncMessageHeader

```
smpEncode = (ehVersion, ehIV, ehAuthTag) <> encodeLarge ehVersion ehBody

Structure (v < 3):
  ehVersion       2 bytes          Word16 BE
  ehIV            16 bytes raw     AES-256-GCM IV
  ehAuthTag       16 bytes raw     Header Auth Tag
  ehBody Len      1 byte           = 88 (0x58)
  ehBody          88 bytes         encrypted MsgHeader
```

### 4.6 MsgHeader (Decrypted)

```
contentLen        variable         msgMaxVersion + DH Key + counters
msgMaxVersion     2 bytes          Word16 BE
DH Key Len        1 byte           = 68 (X448 SPKI)
DH Key            68 bytes         X448 SPKI
PN                4 bytes          Word32 BE (previous chain count)
Ns                4 bytes          Word32 BE (message number in chain)
Padding           fill to 88       0x23 ('#')
```

---

## 5. HKDF Chain (Verified Session 19)

### 5.1 HKDF #1: X3DH Initial

```
Salt:   64 × 0x00
IKM:    DH1 || DH2 || DH3 (168 bytes for X448)
Info:   "SimpleXX3DH"
Output: 96 bytes
  [0-31]   hk  = header_key_send (peer decrypts our headers)
  [32-63]  nhk = header_key_recv (WE decrypt peer's headers) ← THE KEY!
  [64-95]  sk  = root_key (input for Root KDF)
```

### 5.2 HKDF #2/#4: Root KDF

```
Salt:   sk (32 bytes, Root Key)
IKM:    DH(Peer_ratchet_pub, Our_sk2) [56 bytes X448]
Info:   "SimpleXRootRatchet"
Output: 96 bytes
  [0-31]   rk'  = new root_key
  [32-63]  ck   = chain_key_recv
  [64-95]  nhk' = next_header_key_recv
```

### 5.3 HKDF #3/#6: Chain KDF

```
Salt:   "" (empty!)
IKM:    ck (32 bytes, Chain Key)
Info:   "SimpleXChainRatchet"
Output: 96 bytes
  [0-31]   ck'  = next chain_key
  [32-63]  mk   = message_key (for body decrypt)
  [64-79]  iv1  = header_iv
  [80-95]  iv2  = message_iv
```

### 5.4 Key Assignment Summary

| HKDF | Output | Bytes | Name | Usage |
|------|--------|-------|------|-------|
| X3DH | Block 1 | [0-31] | hk | Peer decrypts our headers |
| X3DH | Block 2 | [32-63] | **nhk** | **WE decrypt peer's headers** |
| X3DH | Block 3 | [64-95] | sk | Input for Root KDF |
| Root | Block 1 | [0-31] | rk' | New root key |
| Root | Block 2 | [32-63] | ck | Chain key for receive |
| Root | Block 3 | [64-95] | nhk' | Next header key |
| Chain | Block 1 | [0-31] | ck' | Next chain key |
| Chain | Block 2 | [32-63] | mk | Message key |
| Chain | Block 3a | [64-79] | iv1 | Header IV |
| Chain | Block 3b | [80-95] | iv2 | Message IV |

---

## 6. Verified Byte-Map (Reply Queue AgentConfirmation, Session 19)

### 6.1 Level 1: E2E Plaintext (15904 Bytes)

```
Offset  Hex         Field                         Status
[0-1]   3a ae       unPad originalLength: 15022   ✅
[2]     4B          PrivHeader 'K' (PHConfirm)    ✅
[3]     2C          Auth Key Length: 44           ✅
[4-47]  30 2a 30..  Ed25519 SPKI Auth Key         ✅
[48-49] 00 07       agentVersion: 7               ✅
[50]    43          'C' = AgentConfirmation       ✅
[51]    30          e2eEncryption_ = Nothing      ✅
```

### 6.2 Level 2: EncRatchetMessage (from Offset 52)

```
Offset  Hex         Field                         Status
[52]    7B          emHeader Length: 123          ✅
[53-175]            emHeader (EncMessageHeader):
  [53-54] XX XX       ehVersion: 2                ✅
  [55-70] ...         ehIV (16 Bytes)             ✅
  [71-86] ...         ehAuthTag (16 Bytes)        ✅
  [87]    58          ehBody Length: 88           ✅
  [88-175] ...        ehBody (encrypted MsgHeader) ✅
[176-191]           emAuthTag (16 Bytes)          ✅
[192-15023]         emBody (14832 Bytes)          ✅
```

### 6.3 Level 3: MsgHeader (after Header-Decrypt)

```
Field             Value                           Status
contentLen        79                              ✅
msgMaxVersion     3 (Peer supports PQ)            ✅
DH Key Len        68 (X448 SPKI)                  ✅
Peer DH Key       c3d0cb637a26c2c8... (56B raw)   ✅
PN                0 (first message)               ✅
Ns                0 (Message #0)                  ✅
Padding           0x23 ('#')                      ✅
```

---

## 7. Complete Decryption Chain (Updated Session 19)

```
Layer 0: TLS 1.3 (mbedTLS)                                    ✅ Working
  ↓
Layer 1: SMP Transport (rcvDhSecret + cbNonce(msgId))          ✅ Working
  ↓ Output: [2B len prefix][ClientMsgEnvelope][padding 0x23...]
  ↓
Layer 2: E2E (e2eDhSecret + cmNonce from envelope)             ✅ Working (S18)
  ↓ Output: 15904 bytes (padded)
  ↓
Layer 2.5: unPad                                               ✅ Working (S19)
  ↓ Input: [2B originalLen][ClientMessage][padding 0x23...]
  ↓ Output: 15022 bytes ClientMessage
  ↓
Layer 3: ClientMessage Parse                                   ✅ Working (S19)
  ↓ Input: [PrivHeader][AgentMsgEnvelope]
  ↓ PrivHeader: 'K' + 44B Ed25519 SPKI
  ↓ AgentMsgEnvelope: version + 'C' + e2eEncryption_ + Tail encConnInfo
  ↓
Layer 4: EncRatchetMessage Parse                               ✅ Working (S19)
  ↓ Input: [1B emHeader len=123][emHeader 123B][emAuthTag 16B][Tail emBody]
  ↓ emHeader: [version 2B][ehIV 16B][ehAuthTag 16B][ehBody len 1B][ehBody 88B]
  ↓
Layer 5: Double Ratchet Header Decrypt                         ✅ Working (S19)
  ↓ Key: saved_nhk (HKDF[32-63] from X3DH)
  ↓ IV: ehIV (16 bytes)
  ↓ AAD: rcAD (112 bytes = our_key1 || peer_key1)
  ↓ Output: MsgHeader (79 bytes content + 9 bytes header/padding)
  ↓
Layer 6: Double Ratchet Body Decrypt                           ⏳ Next Step
  ↓ Need: DH Ratchet Step → Root KDF → Chain KDF → message_key
  ↓ Input: emBody (14832 bytes)
  ↓ AAD: rcAD || emHeader (112 + 123 = 235 bytes)
  ↓
Layer 7: ConnInfo Parse                                        ⏳ After L6
  ↓ AgentConnInfoReply with peer's SMP Queues
  ↓
Layer 8: Connection Established                                ⏳ Final Goal
```

---

## 8. Crypto Functions

### 8.1 Header Decrypt (Verified Session 19)

```c
// Key: saved_nhk (HKDF[32-63] from X3DH)
// IV: ehIV (16 bytes from EncMessageHeader)
// AAD: rcAD (112 bytes = our_key1 || peer_key1)
// Ciphertext: ehBody (88 bytes)
// AuthTag: ehAuthTag (16 bytes)

int ret = mbedtls_gcm_auth_decrypt(
    &gcm_ctx,
    88,                    // ehBody length
    ehIV,                  // 16-byte IV
    16,                    // IV length
    rcAD,                  // 112-byte AAD
    112,                   // AAD length
    ehAuthTag,             // 16-byte auth tag
    16,                    // tag length
    ehBody,                // ciphertext
    msg_header             // output plaintext
);
```

### 8.2 Body Decrypt (Next Step)

```c
// Key: message_key (from Chain KDF)
// IV: iv2 (Chain KDF output [80-95])
// AAD: rcAD || emHeader (112 + 123 = 235 bytes)
// Ciphertext: emBody (14832 bytes)
// AuthTag: emAuthTag (16 bytes)
```

### 8.3 SimpleX Custom XSalsa20 (Session 16)

```
Standard libsodium: HSalsa20(dh_secret, nonce[0:16])
SimpleX:            HSalsa20(dh_secret, zeros[16])  ← ZEROS!
```

---

## 9. Working Code State

### 9.1 smp_ratchet.c (DO NOT CHANGE!)

```c
#define RATCHET_VERSION         2
uint8_t em_header[123];         // 123 bytes!
em_header[hp++] = 0x58;         // ehBody-len = 88 (1 BYTE!)
output[p++] = 0x7B;             // emHeader len = 123
```

### 9.2 Key Preservation (Bug #19 Workaround)

```c
// Save nhk immediately after X3DH HKDF
uint8_t saved_nhk[32];
memcpy(saved_nhk, &x3dh_output[32], 32);

// Use saved_nhk for header decrypt instead of header_key_recv
```

---

## 10. Evgeny Quotes (Authoritative)

**ALWAYS read these before asking Evgeny new questions!**

| # | Date | Quote | Topic |
|---|------|-------|-------|
| 1 | 28.01 | "To your question, most likely A" | Reply Queue E2E Key |
| 2 | 28.01 | "combine your private DH key...with sender's public DH key sent in confirmation header - outside of AgentConnInfoReply but in the same message" | Key Location |
| 3 | 28.01 | "TWO separate crypto_box decryption layers...different keys and different nonces" | Two Layers |
| 4 | 28.01 | "it does seem like you're indeed missing server to client encryption layer" | Missing Layer |
| 5 | 28.01 | "I think the key would be in PHConfirmation, no?" | PHConfirmation |
| 6 | 26.01 | "A_MESSAGE is a bit too broad error" | Error Types |
| 7 | 26.01 | "claude is surprisingly good...Opus 4.5 specifically" | Claude Recommendation |
| 8 | 26.01 | "I'd make an automatic test that tests it against haskell implementation" | Testing |
| 9 | 26.01 | "what you did is impressive...first third-party SMP implementation" | Impressed |

---

## 11. Session 19 Key Insights Summary

1. **unPad Layer** — [2B len][content][padding 0x23...]
2. **PrivHeader** — 'K'=PHConfirmation, '_'=PHEmpty
3. **ClientMessage** — Simple concatenation, no length prefix
4. **Maybe encoding** — '0'=Nothing, '1'=Just (NOT 0x00/0x01!)
5. **AgentConfirmation** — (version, 'C', e2eEncryption_, Tail encConnInfo)
6. **EncRatchetMessage** — v<3: 1-byte len prefix
7. **EncMessageHeader** — [version][IV 16B][AuthTag 16B][len 1B][body 88B]
8. **AES-GCM IV** — 16 bytes (not standard 12!)
9. **X3DH HKDF** — hk[0-31], nhk[32-63], sk[64-95]
10. **rcAD** — our_key1 || peer_key1 (112 bytes)
11. **nhk = header_key_recv** — THE key for header decrypt!

---

*Quick Reference v13.0*  
*Last updated: February 5, 2026 - Session 19*  
*Status: Double Ratchet Header Decrypt SUCCESS!*  
*Next: Fix Bug #19, DH Ratchet Step, Body Decrypt*
