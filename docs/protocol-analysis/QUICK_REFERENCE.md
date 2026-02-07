# Quick Reference

## Constants, Wire Formats, Verified Values

**Updated: 2026-02-07 - Session 21 (v3 Format Implemented, HELLO Debugging)**

---

## Current Status

```
SESSION 21 - v3 FORMAT IMPLEMENTED, HELLO DEBUGGING
=====================================================

v3 EncRatchetMessage format byte-correct verified, Server accepts with OK.
App still shows "Connecting..." — RSYNC crypto error on HELLO decrypt.

7 new bugs fixed (#20-#26):
  - PrivHeader: HELLO = 0x00 (not '_')
  - AgentVersion: AgentMessage = 1 (not 2)
  - prevMsgHash: Word16 prefix required
  - cbEncrypt: pad BEFORE encrypt
  - DH Key: snd_dh for HELLO (not rcv_dh)
  - PubHeader Nothing: '0' required
  - v2/v3 format: encodeLarge switches at v≥3

New architecture:
  - 4 Header Keys: HKs/NHKs/HKr/NHKr with promotion
  - SameRatchet vs AdvanceRatchet modes
  - KEY Command (optional for unsecured queues)

Top suspects for Session 22:
  1. HKs/NHKs Promotion after AdvanceRatchet
  2. E2E Version in our Confirmation (v2 vs v3)
  3. DH Key encoding in v3

Next: HKs/NHKs Promotion, E2E Version Clarification
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
13. [Session 21 Key Insights Summary](#13-session-21-key-insights-summary)

---

## 1. Version Numbers (VERIFIED)

| Protocol | Our Value | Hex | Notes |
|----------|-----------|-----|-------|
| SMP Client | 4 | 0x00 0x04 | |
| Agent (Confirmation) | 7 | 0x00 0x07 | AgentConfirmation |
| Agent (Message) | 1 | 0x00 0x01 | AgentMessage (HELLO etc.) — S21 |
| E2E | 2 | 0x00 0x02 | |
| RATCHET_VERSION | **3** | **0x00 0x03** | **Changed v2→v3 in S21!** |

---

## 2. Size Constants (VERIFIED)

| Structure | v2 Size | v3 Size | Notes |
|-----------|---------|---------|-------|
| EncMessageHeader | 123 | **124** | v3: 2-byte prefixes add 1 byte — S21 |
| MsgHeader | 88 | 88 | Same (KEM replaces 1 padding byte) |
| MsgHeader content | 79 | **80** | v3: KEM Nothing adds 1 byte — S21 |
| MsgHeader padding | 7 | **6** | v3: 1 less padding — S21 |
| X448 SPKI | 68 | 68 | 12 header + 56 raw |
| X25519 SPKI | 44 | 44 | 12 header + 32 raw |
| cmNonce | 24 | 24 | In ClientMsgEnvelope |
| Poly1305 MAC | 16 | 16 | Authentication tag |
| AES-GCM AuthTag | 16 | 16 | Authentication tag |
| AES-GCM IV | **16** | **16** | NOT 12! SimpleX uses 16-byte IV |
| Payload AAD | 235 | **236** | v3: 112 + 124 = 236 — S21 |
| rcAD | 112 | 112 | our_key1 \|\| peer_key1 (raw X448, no ASN.1) |

---

## 3. Encoding Reference (from Haskell Source, Verified Session 19-21)

| Primitive | Encoding | Source |
|-----------|----------|--------|
| Word16 | 2 Bytes Big-Endian | Encoding.hs:70-74 |
| Word32 | 4 Bytes Big-Endian | Encoding.hs |
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

### PrivHeader Encoding (Updated Session 21)

| Value | Hex | When Used |
|-------|-----|-----------|
| PHConfirmation 'K' | 0x4B | AgentConfirmation with sender auth key |
| PHEmpty '_' | 0x5F | AgentConfirmation without key |
| No PrivHeader | 0x00 | Regular messages (HELLO, chat messages) |

**NOT a standard Maybe encoding!** Custom scheme with 3 values.

### encodeLarge Version Switch (Session 21 — NEW!)

```haskell
encodeLarge v bs
  | v < VersionE2E 3 = smpEncode (Str.length bs :: Word8) <> bs    -- 1 byte max 255
  | otherwise        = smpEncode (Str.length bs :: Word16) <> bs   -- 2 bytes max 65535
```

---

## 4. Wire Formats (Verified Session 19-21)

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
  agentVersion    Word16 BE = 7    2 bytes
  Tag 'C'         Char             1 byte
  e2eEncryption_  Maybe (...)      1+ bytes
  encConnInfo     Tail             rest
```

### 4.4 AgentMessage / HELLO (Session 21 — NEW!)

```
smpEncode = (agentVersion, smpVersion, prevMsgHash, Tail body)

Fields:
  agentVersion    Word16 BE = 1    2 bytes  ← NOT 2 or 7!
  smpVersion      Word16 BE        2 bytes
  prevMsgHash     Large (Word16)   2+ bytes (empty = [0x00][0x00])
  body            Tail             rest

HELLO Body:
  'H'             HELLO tag        1 byte
  '0'             AckMode_Off      1 byte (0x30, ASCII '0')
```

### 4.5 EncRatchetMessage v3 (Session 21 — UPDATED!)

```
encodeEncRatchetMessage v msg =
  encodeLarge v emHeader <> smpEncode (emAuthTag, Tail emBody)

Structure (v3, v >= 3):
  emHeader Len    2 bytes Word16 BE   = 124 (0x00 0x7C)
  emHeader        124 bytes           EncMessageHeader
  emAuthTag       16 bytes raw        AES-GCM Auth Tag
  emBody          Tail                rest (encrypted payload)

Structure (v2, v < 3):
  emHeader Len    1 byte              = 123 (0x7B)
  emHeader        123 bytes           EncMessageHeader
  emAuthTag       16 bytes raw        AES-GCM Auth Tag
  emBody          Tail                rest (encrypted payload)
```

### 4.6 EncMessageHeader v3 (Session 21 — UPDATED!)

```
Structure (v3, 124 bytes):
  ehVersion       2 bytes          Word16 BE = 3
  ehIV            16 bytes raw     AES-256-GCM IV
  ehAuthTag       16 bytes raw     Header Auth Tag
  ehBody Len      2 bytes Word16   = 88 (0x00 0x58)
  ehBody          88 bytes         encrypted MsgHeader

Structure (v2, 123 bytes):
  ehVersion       2 bytes          Word16 BE = 2
  ehIV            16 bytes raw     AES-256-GCM IV
  ehAuthTag       16 bytes raw     Header Auth Tag
  ehBody Len      1 byte           = 88 (0x58)
  ehBody          88 bytes         encrypted MsgHeader
```

### 4.7 MsgHeader v3 (Session 21 — UPDATED!)

```
v3 MsgHeader (padded to 88 bytes):
  [Word16 BE]     contentLen = 80
  [Word16 BE]     msgMaxVersion = 2
  [1 byte]        DH key length = 68
  [68 bytes]      msgDHRs SPKI (12 header + 56 raw X448)
  [1 byte]        KEM Nothing = '0' (0x30)     ← NEW in v3!
  [Word32 BE]     msgPN
  [Word32 BE]     msgNs
  [6 bytes]       '#' padding (6× instead of 7× in v2)

v2 MsgHeader (padded to 88 bytes):
  [Word16 BE]     contentLen = 79
  [Word16 BE]     msgMaxVersion
  [1 byte]        DH key length = 68
  [68 bytes]      msgDHRs SPKI
  [Word32 BE]     msgPN
  [Word32 BE]     msgNs
  [7 bytes]       '#' padding
```

### 4.8 ConnInfo Tags (Session 20)

| Tag | Hex | Constructor | Who Sends | Content |
|-----|-----|-------------|-----------|---------|
| `'I'` | 0x49 | AgentConnInfo | Any sender on Reply Queue | Profile only |
| `'D'` | 0x44 | AgentConnInfoReply | Joiner on Contact Queue | SMP Queues + Profile |

### 4.9 Compressed ConnInfo Format (Session 20)

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

### 4.10 KEY Command (Session 21 — NEW!)

```
KEY Body:  [corrId][recipientId] KEY [peer_sender_auth_key 44B SPKI]
Signed:    Ed25519 with rcv_auth_private
Server:    Main SSL connection (not peer server)
Response:  OK | ERR AUTH

Source of sender_auth_key:
  PHConfirmation in received AgentConfirmation
  44 bytes Ed25519 SPKI

Status: Functional but NOT REQUIRED (Reply Queues unsecured)
```

---

## 5. HKDF Chain (Verified Session 19-20)

### 5.1 HKDF #1: X3DH Initial

```
Salt:   64 × 0x00
IKM:    DH1 || DH2 || DH3 (168 bytes for X448)
Info:   "SimpleXX3DH"
Output: 96 bytes
  [0-31]   hk  = HKs (encrypt our first headers)
  [32-63]  nhk = NHKr (promotes to HKr on first recv)
  [64-95]  sk  = root_key (input for Root KDF)
```

### 5.2 HKDF #2: Root KDF Recv

```
Salt:   sk (32 bytes, Root Key from X3DH)
IKM:    DH(peer_new_pub, our_old_priv) [56 bytes X448]
Info:   "SimpleXRootRatchet"
Output: 96 bytes
  [0-31]   rk1  = new_root_key_1 (input for Root KDF Send)
  [32-63]  ck   = recv_chain_key
  [64-95]  nhk' = new NHKr (next_header_key_recv)
```

### 5.3 HKDF #3: Root KDF Send

```
Salt:   rk1 (32 bytes, from HKDF #2)
IKM:    DH(peer_new_pub, our_NEW_priv) [56 bytes X448]
Info:   "SimpleXRootRatchet"
Output: 96 bytes
  [0-31]   rk2  = new_root_key_2 (final root key)
  [32-63]  ck   = send_chain_key
  [64-95]  nhk' = new NHKs (next_header_key_send)
```

### 5.4 HKDF #4: Chain KDF Recv

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

### 5.5 4 Header Key Architecture (Session 21 — NEW!)

| Key | Full Name | Usage |
|-----|-----------|-------|
| HKs | header_key_send | Current: encrypt our outgoing headers |
| NHKs | next_header_key_send | Next: will become HKs after our DH ratchet |
| HKr | header_key_recv | Current: decrypt incoming headers |
| NHKr | next_header_key_recv | Next: will become HKr after peer's DH ratchet |

**Initial Assignment from X3DH:**
```
HKs  = hk     (HKDF[0-31])   — used for our first send
NHKs = (none, set after first recv AdvanceRatchet)
HKr  = (none, NHKr promotes on first recv)
NHKr = nhk    (HKDF[32-63])  — promotes to HKr on first recv
```

**Promotion on AdvanceRatchet:**
```
Receiving: HKr ← NHKr, then rootKdf → new NHKr
Sending:   HKs ← NHKs, then rootKdf → new NHKs
```

### 5.6 SameRatchet vs AdvanceRatchet (Session 21 — NEW!)

| Mode | Trigger | DH Step? | Operations |
|------|---------|----------|------------|
| SameRatchet | Same DH key (dh_changed=false) | NO | chainKdf only → mk, ivs |
| AdvanceRatchet | New DH key (dh_changed=true) | YES | 2× rootKdf + chainKdf |

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
[52]    7B          emHeader Length: 123          ✅ (v2, v3=00 7C)
[53-175]            emHeader (EncMessageHeader):
  [53-54] XX XX       ehVersion: 2                ✅
  [55-70] ...         ehIV (16 Bytes)             ✅
  [71-86] ...         ehAuthTag (16 Bytes)        ✅
  [87]    58          ehBody Length: 88           ✅ (v2, v3=00 58)
  [88-175] ...        ehBody (encrypted MsgHeader) ✅
[176-191]           emAuthTag (16 Bytes)          ✅
[192-15023]         emBody (14832 Bytes)          ✅
```

### 6.3 Level 3: MsgHeader (after Header-Decrypt)

```
Field             Value                           Status
contentLen        79 (v2) / 80 (v3)              ✅
msgMaxVersion     3 (Peer supports PQ)            ✅
DH Key Len        68 (X448 SPKI)                  ✅
Peer DH Key       c3d0cb637a26c2c8... (56B raw)   ✅
KEM               Nothing ('0') — v3 only         ✅ S21
PN                0 (first message)               ✅
Ns                0 (Message #0)                  ✅
Padding           0x23 ('#')                      ✅
```

### 6.4 Level 4: Body Decrypt Intermediate Values (Session 20)

```
root_key:        b0d3fd0e76379553d10718617a973bc69a289c8381ff608f7d1057f292df90dd
dh_secret_recv:  9a66056fff2882bb4690a098ca000b8ac69a0283790ffbfbbb630c20ba3061b1...
new_root_key_1:  82190a059a10b8097355b6a612a1ef21a18b0f46c5ed4c8e066f9c97b90d1e97
recv_chain_key:  747dcc01aa665f0d85295950fdbc4b2fa398cd90615a8f9259efd62ba6318ef5
message_key:     ea8461db5d92ce9f70474bae4d241bca2a99d87cac4ccd48d0af177019b8d44d
iv_body:         a187e7d0636a7e54902a607b05dfbdd8
```

### 6.5 Level 5: ConnInfo Parse (Session 20)

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

## 7. Complete Decryption/Send Chain (Updated Session 21)

```
RECEIVE CHAIN (all working):
Layer 0: TLS 1.3 (mbedTLS)                                    ✅ Working
  ↓
Layer 1: SMP Transport (rcvDhSecret + cbNonce(msgId))          ✅ Working
  ↓
Layer 2: E2E (e2eDhSecret + cmNonce from envelope)             ✅ Working (S18)
  ↓
Layer 2.5: unPad                                               ✅ Working (S19)
  ↓
Layer 3: ClientMessage Parse                                   ✅ Working (S19)
  ↓
Layer 4: EncRatchetMessage Parse                               ✅ Working (S19, v3 S21)
  ↓
Layer 5: Double Ratchet Header Decrypt                         ✅ Working (S19)
  ↓
Layer 6: Double Ratchet Body Decrypt                           ✅ Working (S20)
  ↓
Layer 7: ConnInfo Parse + Zstd                                 ✅ Working (S20)
  ↓
Layer 8: Peer Profile JSON                                     ✅ Working (S20)

SEND CHAIN (HELLO):
Layer 9: HELLO Send                                            ⚠️ Server OK, App RSYNC
  ↓ v3 format implemented (S21)
  ↓ 7 format bugs fixed (#20-#26)
  ↓ KEY command optional
  ↓ App can't decrypt → RSYNC crypto error
  ↓
Layer 10: HELLO Receive                                        ⏳ Blocked
  ↓
Layer 11: Connection Established                               ⏳ Final Goal
```

---

## 8. Crypto Functions

### 8.1 Header Decrypt (Verified Session 19)

```c
// Key: HKr (promoted from NHKr on AdvanceRatchet)
// IV: ehIV (16 bytes from EncMessageHeader)
// AAD: rcAD (112 bytes = our_key1 || peer_key1)
// Ciphertext: ehBody (88 bytes)
// AuthTag: ehAuthTag (16 bytes)
```

### 8.2 Body Decrypt (Verified Session 20)

```c
// Key: message_key (32 bytes from Chain KDF [32-63])
// IV: iv_body (16 bytes from Chain KDF [64-79])
// AAD: rcAD[112] || emHeader[123 or 124] = 235 or 236 bytes
// Ciphertext: emBody
// AuthTag: emAuthTag (16 bytes)
```

### 8.3 SimpleX Custom XSalsa20 (Session 16)

```
Standard libsodium: HSalsa20(dh_secret, nonce[0:16])
SimpleX:            HSalsa20(dh_secret, zeros[16])  ← ZEROS!
```

---

## 9. Working Code State

### 9.1 smp_ratchet.c (Updated Session 21)

```c
#define RATCHET_VERSION         3              // Changed from 2 in S21!
uint8_t em_header[124];                        // 124 bytes in v3 (was 123)
em_header[hp++] = 0x00; em_header[hp++] = 0x58; // ehBody-len = 88 (2 BYTES in v3!)
output[p++] = 0x00; output[p++] = 0x7C;        // emHeader len = 124 (2 BYTES in v3!)
// MsgHeader includes KEM Nothing: msg_header[p++] = '0';
```

### 9.2 HELLO Format (Session 21 — NEW!)

```c
// PrivHeader: 0x00 (no PrivHeader for regular messages)
// AgentVersion: 0x00 0x01 (version 1, NOT 2 or 7)
// prevMsgHash: 0x00 0x00 (Word16 prefix, empty)
// Body: 'H' '0' (HELLO + AckMode_Off)
// PubHeader: '0' (Nothing, standard Maybe encoding)
// Pad BEFORE encrypt (pad → cbEncrypt)
// DH Key: snd_dh (not rcv_dh)
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

1. **Bug #19 Root Cause** — Debug self-decrypt test corrupted ratchet state
2. **DH Ratchet Step = TWO rootKdf calls** — recv chain + send chain
3. **iv1 = Body IV, iv2 = Header IV** — header IV from ehIV, not chainKdf
4. **Body AAD = rcAD || emHeader (raw)** — 112 + 123 = 235 bytes
5. **ConnInfo tag 'I' = AgentConnInfo** — Profile only
6. **ConnInfo tag 'D' = AgentConnInfoReply** — SMP Queues + Profile
7. **Zstd compression** — 'X' marker, '1'=compressed, '0'=passthrough
8. **Zstd magic** — `28 b5 2f fd` (little-endian: 0xFD2FB528)
9. **XInfo Profile** — event "x.info", JSON with displayName
10. **Complete chain verified** — TLS → SMP → E2E → Ratchet → Zstd → JSON

---

## 13. Session 21 Key Insights Summary

1. **ESP32 = Accepting Party, App = Joining Party** — affects key/queue usage
2. **PrivHeader: HELLO=0x00, CONF='K', empty='_'** — 3 values, not standard Maybe
3. **AgentMessage vs AgentConfirmation** — different agentVersion (1 vs 7)
4. **HELLO body** — 'H' + '0' (AckMode_Off), just 2 bytes
5. **prevMsgHash** — Word16 prefix, empty = [0x00][0x00]
6. **DH Keys differ by message type** — rcv_dh for Conf, snd_dh for HELLO
7. **PubHeader Nothing** — '0' (0x30), must be present
8. **KEY command** — optional for unsecured Reply Queues
9. **RSYNC = Ratchet Sync** — crypto decrypt failure indicator
10. **v2/v3 encodeLarge switch** — 1-byte → 2-byte prefix at v≥3
11. **4 Header Keys** — HKs/NHKs/HKr/NHKr with promotion
12. **SameRatchet vs AdvanceRatchet** — chain-only vs full DH ratchet step

---

*Quick Reference v15.0*  
*Last updated: February 7, 2026 - Session 21*  
*Status: v3 Format Implemented, App RSYNC on HELLO*  
*Next: HKs/NHKs Promotion, E2E Version Clarification*
