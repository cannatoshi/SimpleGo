# Quick Reference

## Constants, Wire Formats, Verified Values

**Updated: 2026-02-06 - Session 20 (Body Decrypt SUCCESS! Peer Profile Read!)**

---

## Current Status

```
SESSION 20 - BODY DECRYPT SUCCESS! PEER PROFILE READ ON ESP32!
================================================================

Complete crypto chain working end-to-end:
  TLS 1.3 → SMP Transport → Server Decrypt → E2E Decrypt → unPad
  → ClientMessage → EncRatchetMessage → Header Decrypt (AES-GCM)
  → DH Ratchet Step (2× rootKdf) → Chain KDF → Body Decrypt (AES-GCM)
  → unPad → AgentConnInfo 'I' → Zstd Decompress → Peer Profile JSON

Peer profile read: "displayName": "cannatoshi" on an ESP32!

Bug #19: FIXED! Root cause: debug self-decrypt in smp_peer.c:347

New discoveries:
  - DH Ratchet Step = 2× rootKdf (recv chain + send chain)
  - iv1 = Body IV, iv2 = Header IV (correction!)
  - ConnInfo: 'I' = AgentConnInfo (profile), 'D' = AgentConnInfoReply
  - Zstd compression: 'X' marker, '1'=compressed, '0'=passthrough
  - XInfo Profile JSON: event "x.info", version range "1-16"

Next: HELLO processing, Ratchet State Persistence, Bidirectional Messaging
```

---

## Table of Contents

1. [Version Numbers](#1-version-numbers)
2. [Size Constants](#2-size-constants)
3. [Encoding Reference](#3-encoding-reference)
4. [Wire Formats](#4-wire-formats)
5. [HKDF Chain](#5-hkdf-chain)
6. [Verified Byte-Map](#6-verified-byte-map)
7. [Complete Decryption Chain](#7-complete-decryption-chain)
8. [Crypto Functions](#8-crypto-functions)
9. [Working Code State](#9-working-code-state)
10. [Evgeny Quotes](#10-evgeny-quotes)
11. [Session 19 Key Insights Summary](#11-session-19-key-insights-summary)
12. [Session 20 Key Insights Summary](#12-session-20-key-insights-summary)

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
| Payload AAD | **235** | NO prefix! (112 + 123) |
| rcAD | 112 | our_key1 \|\| peer_key1 (raw X448, no ASN.1) |

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

## 4. Wire Formats (Verified Session 19-20)

### 4.1 unPad Layer (Session 19)

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

### 4.7 ConnInfo Tags (Session 20 — NEW!)

| Tag | Hex | Constructor | Who Sends | Content |
|-----|-----|-------------|-----------|---------|
| `'I'` | 0x49 | AgentConnInfo | Any sender on Reply Queue | Profile only |
| `'D'` | 0x44 | AgentConnInfoReply | Joiner on Contact Queue | SMP Queues + Profile |

```
AgentConnInfo:      'I' <Tail connInfo>
AgentConnInfoReply: 'D' <smpQueues> <Tail connInfo>
```

### 4.8 Compressed ConnInfo Format (Session 20 — NEW!)

```
ConnInfo = 'I' <compressed_batch>

compressed_batch:
  'X' (0x58)                    — Compressed marker
  <Word16 BE count>             — NonEmpty list count
  For each item:
    '0' <Tail data>             — Passthrough (≤180 bytes, no compression)
    '1' <Word16 BE len> <data>  — Zstd compressed

Zstd Frame Magic: 28 b5 2f fd (little-endian: 0xFD2FB528)
Max decompressed: 65,536 bytes
Standard Zstd Level 3, no dictionary
```

### 4.9 XInfo Profile JSON (Session 20 — NEW!)

```json
{
  "v": "1-16",           // Chat protocol version range
  "event": "x.info",     // XInfo Profile event type
  "params": {
    "profile": {
      "displayName": "...",
      "fullName": "...",
      "shortDescr": "...",
      "image": "data:image/jpg;base64,...",
      "contactLink": "https://...",
      "preferences": { ... }
    }
  }
}
```

---

## 5. HKDF Chain (Verified Session 19-20)

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

### 5.2 HKDF #2: Root KDF Recv (Session 20 — VERIFIED!)

```
Salt:   sk (32 bytes, Root Key from X3DH)
IKM:    DH(peer_new_pub, our_old_priv) [56 bytes X448]
Info:   "SimpleXRootRatchet"
Output: 96 bytes
  [0-31]   rk1  = new_root_key_1 (input for Root KDF Send)
  [32-63]  ck   = recv_chain_key
  [64-95]  nhk' = next_header_key_recv
```

### 5.3 HKDF #3: Root KDF Send (Session 20 — NEW!)

```
Salt:   rk1 (32 bytes, from HKDF #2)
IKM:    DH(peer_new_pub, our_NEW_priv) [56 bytes X448]
Info:   "SimpleXRootRatchet"
Output: 96 bytes
  [0-31]   rk2  = new_root_key_2 (final root key)
  [32-63]  ck   = send_chain_key
  [64-95]  nhk' = next_header_key_send
```

### 5.4 HKDF #4: Chain KDF Recv (Session 20 — VERIFIED!)

```
Salt:   "" (empty!)
IKM:    ck (32 bytes, recv_chain_key from HKDF #2)
Info:   "SimpleXChainRatchet"
Output: 96 bytes
  [0-31]   ck'  = next chain_key
  [32-63]  mk   = message_key (for body decrypt)
  [64-79]  iv1  = BODY IV (NOT header!)
  [80-95]  iv2  = header IV (ignored during decrypt)
```

### 5.5 Key Assignment Summary (Updated Session 20)

| HKDF # | Output | Bytes | Name | Usage | Verified |
|--------|--------|-------|------|-------|----------|
| X3DH #1 | Block 1 | [0-31] | hk | Peer decrypts our headers | S19 |
| X3DH #1 | Block 2 | [32-63] | nhk | WE decrypt peer's headers | S19 |
| X3DH #1 | Block 3 | [64-95] | sk | Input for Root KDF | S19 |
| Root #2 | Block 1 | [0-31] | rk1 | Input for Root KDF Send | S20 |
| Root #2 | Block 2 | [32-63] | ck_recv | Recv chain key | S20 |
| Root #2 | Block 3 | [64-95] | nhk_recv | Next header key recv | S20 |
| Root #3 | Block 1 | [0-31] | rk2 | New root key (final) | S20 |
| Root #3 | Block 2 | [32-63] | ck_send | Send chain key | S20 |
| Root #3 | Block 3 | [64-95] | nhk_send | Next header key send | S20 |
| Chain #4 | Block 1 | [0-31] | ck' | Next recv chain key | S20 |
| Chain #4 | Block 2 | [32-63] | mk | Message key (body decrypt) | S20 |
| Chain #4 | Block 3a | [64-79] | iv1 | **Body IV** (NOT header!) | S20 |
| Chain #4 | Block 3b | [80-95] | iv2 | Header IV (ignored in decrypt) | S20 |

### 5.6 iv1/iv2 Clarification (Session 20 CORRECTION)

```
ENCRYPT uses both:
  iv1 → body encryption IV
  iv2 → header encryption IV

DECRYPT uses only iv1:
  iv1 → body decryption IV
  Header IV comes from ehIV in the wire format (EncMessageHeader)
  iv2 is IGNORED during decrypt
```

---

## 6. Verified Byte-Map (Updated Session 20)

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

### 6.4 Level 4: Body Decrypt Intermediate Values (Session 20 — NEW!)

```
root_key:        b0d3fd0e76379553d10718617a973bc69a289c8381ff608f7d1057f292df90dd
dh_secret_recv:  9a66056fff2882bb4690a098ca000b8ac69a0283790ffbfbbb630c20ba3061b1...
new_root_key_1:  82190a059a10b8097355b6a612a1ef21a18b0f46c5ed4c8e066f9c97b90d1e97
recv_chain_key:  747dcc01aa665f0d85295950fdbc4b2fa398cd90615a8f9259efd62ba6318ef5
message_key:     ea8461db5d92ce9f70474bae4d241bca2a99d87cac4ccd48d0af177019b8d44d
iv_body:         a187e7d0636a7e54902a607b05dfbdd8
```

### 6.5 Level 5: ConnInfo Parse (Session 20 — NEW!)

```
Offset  Hex    Field                         Status
[0]     49     'I' — AgentConnInfo Tag       ✅
[1]     58     'X' — Compressed marker       ✅
[2]     01     NonEmpty count: 1             ✅
[3]     31     '1' — Zstd compressed         ✅
[4-5]   22 b1  Zstd data length: 8881       ✅
[6-8886]       Zstd compressed data          ✅

After Zstd decompress: 12268 bytes JSON     ✅
```

---

## 7. Complete Decryption Chain (Updated Session 20)

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
  ↓ Key: header_key_recv (nhk from X3DH HKDF[32-63])
  ↓ IV: ehIV (16 bytes)
  ↓ AAD: rcAD (112 bytes = our_key1 || peer_key1)
  ↓ Output: MsgHeader (79 bytes content + 9 bytes header/padding)
  ↓
Layer 6: Double Ratchet Body Decrypt                           ✅ Working (S20)
  ↓ DH Ratchet Step: 2× rootKdf → recv_chain_key
  ↓ Chain KDF: → message_key + iv_body
  ↓ AES-256-GCM: key=message_key, iv=iv_body, AAD=rcAD||emHeader
  ↓ Output: 8889 bytes → unPad → 8887 bytes plaintext
  ↓
Layer 7: ConnInfo Parse                                        ✅ Working (S20)
  ↓ Tag: 'I' = AgentConnInfo (peer profile)
  ↓ 'X' compressed batch → Zstd decompress
  ↓ Output: 12268 bytes JSON
  ↓
Layer 8: Peer Profile                                          ✅ Working (S20)
  ↓ event: "x.info" — XInfo Profile
  ↓ displayName: "cannatoshi"
  ↓ Full profile with image, preferences, contact link
  ↓
Layer 9: Connection Established                                ⏳ Next Step
  ↓ Need: Process HELLO message
  ↓ Need: Send HELLO back
  ↓ Need: Bidirectional messaging
```

---

## 8. Crypto Functions

### 8.1 Header Decrypt (Verified Session 19)

```c
// Key: header_key_recv (nhk from X3DH HKDF[32-63])
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

### 8.2 Body Decrypt (Verified Session 20)

```c
// Step 1: DH Ratchet Recv
// dh_secret = X448_DH(peer_new_pub, our_old_priv)
// HKDF(salt=root_key, ikm=dh_secret, info="SimpleXRootRatchet")
// → new_root_key_1, recv_chain_key, new_nhk_recv

// Step 2: DH Ratchet Send
// Generate new keypair (our_new_priv, our_new_pub)
// dh_secret = X448_DH(peer_new_pub, our_new_priv)
// HKDF(salt=new_root_key_1, ikm=dh_secret, info="SimpleXRootRatchet")
// → new_root_key_2, send_chain_key, new_nhk_send

// Step 3: Chain KDF
// HKDF(salt="", ikm=recv_chain_key, info="SimpleXChainRatchet")
// → next_recv_ck, message_key, iv_body, (iv_header ignored)

// Step 4: AES-GCM Decrypt
// Key: message_key (32 bytes from Chain KDF [32-63])
// IV: iv_body (16 bytes from Chain KDF [64-79])
// AAD: rcAD[112] || emHeader[123] = 235 bytes
// Ciphertext: emBody (14832 bytes)
// AuthTag: emAuthTag (16 bytes)

int ret = mbedtls_gcm_auth_decrypt(
    &gcm_ctx,
    emBody_len,            // 14832 bytes
    iv_body,               // 16-byte IV from Chain KDF
    16,                    // IV length
    payload_aad,           // 235-byte AAD (rcAD || emHeader)
    235,                   // AAD length
    emAuthTag,             // 16-byte auth tag
    16,                    // tag length
    emBody,                // ciphertext
    decrypted              // output plaintext
);

// Step 5: unPad
// msg_len = BE_uint16(decrypted[0..1])
// plaintext = decrypted[2 .. 2+msg_len-1]
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

### 9.2 Bug #19 Fix (Session 20)

```c
// REMOVED from smp_peer.c:343-359:
// Debug self-decrypt test that called ratchet_decrypt() on own message
// This corrupted header_key_recv, root_key, chain_key_recv, dh_peer

// saved_nhk workaround in smp_ratchet.c kept as safety net but no longer needed
```

### 9.3 ratchet_decrypt_body() (Session 20 — NEW!)

```c
// ~300 lines in smp_ratchet.c
// Implements: DH Ratchet Step (2× rootKdf) + Chain KDF + AES-GCM Decrypt + unPad
// State update: LOG ONLY (not yet activated for production)
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

## 12. Session 20 Key Insights Summary

1. **Bug #19 Root Cause** — Debug self-decrypt test in smp_peer.c:347 corrupted ratchet state
2. **DH Ratchet Step = TWO rootKdf calls** — recv chain + send chain, new keypair in between
3. **iv1 = Body IV, iv2 = Header IV** — During decrypt, header IV comes from ehIV, not chainKdf
4. **Body AAD = rcAD || emHeader (raw)** — 112 + 123 = 235 bytes, use exact wire bytes
5. **ConnInfo tag 'I' = AgentConnInfo** — Profile only (Initiator/Reply Queue sender)
6. **ConnInfo tag 'D' = AgentConnInfoReply** — SMP Queues + Profile (Joiner on Contact Queue)
7. **Zstd compression** — 'X' marker, '1'=compressed, '0'=passthrough, max 65536 bytes
8. **Zstd magic** — `28 b5 2f fd` (little-endian: 0xFD2FB528)
9. **XInfo Profile** — event "x.info", JSON with displayName, image, preferences
10. **Complete chain verified** — TLS → SMP → E2E → Ratchet → Zstd → JSON on ESP32

---

*Quick Reference v14.0*  
*Last updated: February 6, 2026 - Session 20*  
*Status: Body Decrypt SUCCESS! Peer Profile Read on ESP32!*  
*Next: HELLO processing, Ratchet State Persistence, Bidirectional Messaging*
