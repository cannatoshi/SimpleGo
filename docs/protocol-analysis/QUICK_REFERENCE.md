# Quick Reference

## Constants, Wire Formats, Verified Values

**Updated: 2026-02-05 - Session 18 (🎉 BUG #18 SOLVED!)**

---

## Current Status

```
SESSION 18 - 🎉 BUG #18 SOLVED! E2E LAYER 2 DECRYPT SUCCESS!
================================================================

Root Cause: envelope_len included 102 bytes SMP block-padding
  plain_len = 16106, raw_len_prefix = 16002, padding = 102 (0x23)
  
Fix: ONE LINE — envelope_len = raw_len_prefix

Result: Method 0 (decrypt_client_msg) SUCCESS! 15904 bytes!
  Content: PrivHeader ':' + Ed25519 SPKI + AgentConfirmation + EncRatchetMessage

All Layers Through Layer 2: ✅
  Layer 0: TLS 1.3 ✅
  Layer 1: SMP Transport ✅
  Layer 2: E2E Decrypt ✅ (FIXED Session 18!)
  Layer 3: AgentMsgEnvelope parsing ⏳
  Layer 4: Double Ratchet ⏳
  Layer 5: Application Data ⏳

Next: Parse AgentConfirmation, identify PrivHeader ':', decrypt EncRatchetMessage
```

---

## Table of Contents

1. [Version Numbers](#1-version-numbers)
2. [Size Constants](#2-size-constants)
3. [Message Structure (Verified)](#3-message-structure-verified)
4. [Verified Test Data (Session 14)](#4-verified-test-data-session-14)
5. [Crypto Functions](#5-crypto-functions)
6. [Working Code State](#6-working-code-state)
7. [Message Flow](#7-message-flow)
8. [Queue Architecture](#8-queue-architecture)
9. [SMP Block-Padding](#9-smp-block-padding)
10. [Decryption Chain](#10-complete-decryption-chain)
11. [Important Source Locations](#11-important-source-locations)
12. [Evgeny Quotes](#12-evgeny-quotes-authoritative)

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
| Payload AAD | **235** | NO prefix! |

---

## 3a. Maybe Encoding (Session 15 Discovery)

### 3a.1 Maybe Tags

```
'0' (0x30) = Nothing (no value)
'1' (0x31) = Just (value follows)
',' (0x2C) = Nothing (alternative marker)
```

### 3a.2 Reply Queue HELLO Structure

```
[14] = '1' (0x31) = maybe_corrId = Just (corrId follows)
[15] = ',' (0x2C) = maybe_e2e = Nothing (NO e2ePubKey!)
```

### 3a.3 When maybe_e2e = Nothing

- Message has NO ephemeral e2ePubKey
- Uses pre-computed e2eDhSecret
- Secret was created during connection setup
- We need `app.sndQueue.e2ePubKey` to calculate it
- This key is in App's AgentConfirmation!

---

## 3. Message Structure (Verified Session 14)

### 3.1 Reply Queue After Server-Decrypt

```
Offset  Bytes                              Meaning
------  -----                              -------
[0-1]   3e 82                              Length prefix: 16002
[2-9]   00 00 00 00 69 7e 97 10            Padding/Timestamp
[10-13] 54 20 00 04                        PubHeader: Version, Flags
[14]    31 ('1')                           Maybe tag = Just (key present!)
[15]    2c (44)                            SPKI Length
[16-27] 30 2a 30 05 06 03 2b 65 6e 03 21 00  X25519 SPKI Header
[28-59] 91 40 e1 0e ...                    Peer's E2E public key (32 bytes)
[60-83] b2 1f a2 bc ...                    cmNonce (24 bytes)
[84-99] cc 3e ec 54 ...                    MAC (16 bytes)
[100+]  5b f2 2e fa ...                    Ciphertext
```

### 3.2 ClientMsgEnvelope Wire-Format (CORRECTED Session 18!)

**CRITICAL: corrId does NOT exist in ClientMsgEnvelope!**
corrId is SMP Transport Layer, parsed BEFORE the envelope.

```
ClientMsgEnvelope = (PubHeader, cmNonce, Tail cmEncBody)
PubHeader = (phVersion, Maybe phE2ePubDhKey)

WITH e2ePubKey (first message):
[0-1]   = phVersion (00 04)
[2]     = '1' (0x31) = Just
[3-46]  = X25519 SPKI (44 bytes)
[47-70] = cmNonce (24 bytes, raw)
[71+]   = cmEncBody (rest)

WITHOUT e2ePubKey (subsequent messages):
[0-1]   = phVersion (00 04)
[2]     = '0' (0x30) = Nothing
[3-26]  = cmNonce (24 bytes, raw)
[27+]   = cmEncBody (rest)
```

### 3.3 Key Extraction

```c
// Peer's E2E public key at offset 28
uint8_t peer_e2e_pub[32];
memcpy(peer_e2e_pub, &server_plain[28], 32);

// Nonce at offset 60
uint8_t cm_nonce[24];
memcpy(cm_nonce, &server_plain[60], 24);

// MAC at offset 84
const uint8_t *mac = &server_plain[84];

// Ciphertext at offset 100
const uint8_t *ciphertext = &server_plain[100];
```

---

## 4. Verified Test Data (Session 14)

### 4.1 Keys (VERIFIED MATCH!)

```python
# Our E2E private key
our_e2e_private = "83473153de033039edec9c5db7591cacfa42b6dd89a0618a00806732d01a96fa"

# Peer's E2E public key (from message header)
peer_e2e_pub = "9140e10e9fdee92ebb801ae8694435b5e9f06c4e0077dfa98d39b0f1bf0c0300"

# DH Secret (VERIFIED - Python matches ESP32!)
dh_secret = "d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810"
```

### 4.2 Nonce and MAC (VERIFIED)

```python
# cmNonce (24 bytes)
cm_nonce = "b21fa2bc0dbb5cb02d674dedfd65b0e6ff0fcf793791fd3b"

# MAC (16 bytes)
mac = "cc3eec548b0440cf0222466a79a00c0c"

# Ciphertext length
ciphertext_len = 16006
```

### 4.3 Session 18 Verified Values

```
plain_len:        16106 (total decrypted from Layer 1)
raw_len_prefix:   16002 (actual ClientMsgEnvelope length)
prefix_bytes:     2     (length prefix itself)
padding:          102   (SMP block-padding 0x23)
decrypted_result: 15904 bytes (AgentConfirmation + EncRatchetMessage)
```

---

## 5. Crypto Functions

### 5.1 DH Calculation (CORRECT)

```c
// Use crypto_scalarmult for raw DH (NOT crypto_box_beforenm!)
uint8_t dh_secret[32];
crypto_scalarmult(dh_secret, our_queue.e2e_private, peer_e2e_pub);
```

**Why NOT crypto_box_beforenm?**
- `crypto_box_beforenm` applies HSalsa20 key derivation
- Haskell uses raw DH output directly
- `crypto_scalarmult` gives raw DH output

### 5.2 Decrypt (Current Implementation)

```c
// Haskell format: [MAC 16][Ciphertext]
const uint8_t *mac = &server_plain[84];
const uint8_t *ciphertext = &server_plain[100];

int ret = crypto_secretbox_open_detached(
    plain,          // output
    ciphertext,     // input (after MAC)
    mac,            // MAC (first 16 bytes)
    ciphertext_len, // only ciphertext length
    cm_nonce,       // 24 bytes
    dh_secret       // raw DH output
);
```

### 5.3 Haskell vs libsodium

| Aspect | Haskell | libsodium | Match? |
|--------|---------|-----------|--------|
| Algorithm | XSalsa20-Poly1305 | crypto_secretbox | YES |
| Key | Raw DH (32 bytes) | Raw DH | YES |
| DH Function | X25519.dh | crypto_scalarmult | YES |
| Format | [MAC][Cipher] | detached | YES |

### 5.4 SimpleX Custom XSalsa20 (Session 16 Discovery!)

**CRITICAL:** SimpleX uses NON-STANDARD XSalsa20!

```
Standard libsodium crypto_secretbox:
  HSalsa20(dh_secret, nonce[0:16])

SimpleX xSalsa20 (Crypto.hs):
  HSalsa20(dh_secret, zeros[16])    <- ZEROS not nonce!
  HSalsa20(subkey1, nonce[8:24])
  Salsa20(subkey2, nonce[0:8])
```

**Subkeys are COMPLETELY DIFFERENT!**
```
Standard:  2d4b4528855228d0abf137ea...
SimpleX:   ce1b436c8b333a5ff881d4c0...
```

**Implementation (simplex_crypto.c):**
```c
int simplex_secretbox_open(...) {
    uint8_t subkey1[32], subkey2[32];
    uint8_t zeros[16] = {0};
    
    // Step 1: HSalsa20(dh_secret, zeros[16])
    crypto_core_hsalsa20(subkey1, zeros, dh_secret, NULL);
    
    // Step 2: HSalsa20(subkey1, nonce[8:24])
    crypto_core_hsalsa20(subkey2, &nonce[8], subkey1, NULL);
    
    // Step 3: Salsa20 decrypt + Poly1305 verify
}
```

---

## 6. Working Code State

### 6.1 smp_ratchet.c (DO NOT CHANGE!)

```c
#define RATCHET_VERSION         2
uint8_t em_header[123];         // 123 bytes!
em_header[hp++] = 0x58;         // ehBody-len = 88 (1 BYTE!)
output[p++] = 0x7B;             // emHeader len = 123
```

### 6.2 smp_queue.h

```c
typedef struct {
    uint8_t rcv_dh_public[32];    // Server DH
    uint8_t rcv_dh_private[32];
    
    uint8_t e2e_public[32];       // E2E DH (separate!)
    uint8_t e2e_private[32];
    
    uint8_t shared_secret[32];
    // ...
} our_queue_t;
```

### 6.3 Reply Queue E2E Decrypt (FIXED Session 18!)

```c
// CRITICAL: Use length prefix, NOT buffer size!
size_t envelope_len = raw_len_prefix;  // NOT plain_len - rq_prefix_len!
```

---

## 7. Message Flow (VERIFIED Session 14)

### 7.1 Correct Flow (from Haskell Source)

```
Contact Queue: 1 message
  - INVITATION (Type 'I')

Reply Queue: 1 message
  - HELLO (AgentMsgEnvelope)

NO SECOND MESSAGE ON CONTACT QUEUE!
```

### 7.2 Handoff Theory Was WRONG

| Handoff Document | Reality |
|------------------|---------|
| 2 MSGs on Contact Queue | FALSE |
| PHConfirmation has key | FALSE |
| HELLO on Reply Queue | TRUE |

---

## 8. Queue Architecture (Clarified Session 18!)

### 8.1 Contact Queue (ONLY Layer 1!)

```
Incoming MSG on Contact Queue:
  1. SMP Transport decrypt (Server→Recipient) → Layer 1
  2. Direct parse_agent_message() — NO E2E Layer!
  
decrypt_smp_message() → parse_agent_message()
```

**Contact Queue NEVER had a separate E2E layer!**

### 8.2 Reply Queue (TWO Layers)

```
Incoming MSG on Reply Queue:
  1. SMP Transport decrypt (Server→Recipient) → Layer 1
  2. ClientMsgEnvelope parse + E2E decrypt → Layer 2
  
decrypt_smp_message() → parse ClientMsgEnvelope → E2E decrypt → parse_agent_message()
```

### 8.3 Comparison

| Queue | Layer 1 (Server) | Layer 2 (E2E) | Status |
|-------|-------------------|---------------|--------|
| Contact Queue | ✅ decrypt_smp_message | ❌ NO E2E Layer | Working |
| Reply Queue | ✅ decrypt_smp_message | ✅ E2E decrypt | **FIXED S18!** |

---

## 9. SMP Block-Padding (Session 18 Discovery!)

### 9.1 The Padding

```
plain_len:        16106 (total decrypted from Layer 1)
raw_len_prefix:   16002 (actual ClientMsgEnvelope length)
prefix_bytes:     2     (length prefix itself)
padding:          102   (16106 - 16002 - 2)
padding_value:    0x23  (SMP block-padding character)
```

### 9.2 Why Padding Exists

SMP protocol pads messages to fixed block sizes for traffic analysis resistance. Padding is added AFTER the ClientMsgEnvelope but BEFORE Layer 1 encryption.

### 9.3 CRITICAL Rule

```
ALWAYS use length prefix for content boundaries!
NEVER use buffer_size - header_size!

WRONG:  envelope_len = plain_len - header_len    // Includes padding!
CORRECT: envelope_len = raw_len_prefix            // Exact content length!
```

---

## 10. Complete Decryption Chain (Verified Session 18!)

```
Layer 0: TLS 1.3 (mbedTLS)                                    ✅ Working
  ↓
Layer 1: SMP Transport (rcvDhSecret + cbNonce(msgId))          ✅ Working
  ↓ Output: [2B len prefix][ClientMsgEnvelope][padding 0x23...]
  ↓ CRITICAL: Use len prefix, NOT buffer size!
  ↓
Layer 2: E2E (e2eDhSecret + cmNonce from envelope)             ✅ FIXED S18!
  ↓ Input: ClientMsgEnvelope = [PubHeader][cmNonce][cmEncBody]
  ↓ PubHeader = [version 2B][maybe 1B][opt. SPKI 44B]
  ↓ Output: 15904 bytes
  ↓
Layer 3: AgentMsgEnvelope / ClientMessage                      ⏳ Next
  ↓ Contains: PrivHeader + AgentMessage
  ↓
Layer 4: Double Ratchet (EncRatchetMessage)                    ⏳ After L3
  ↓ Inside AgentConfirmation
  ↓
Layer 5: Application Data (ConnInfo, etc.)                     ⏳ After L4
```

### Wrapper Chain (Session 18 Discovery)

```
EncRcvMsgBody (encrypted blob from server)
  ↓ decrypt with rcvDhSecret
ClientRcvMsgBody {msgTs :: SysTime, msgFlags :: Word8, msgBody :: Tail ByteString}
  ↓ extract msgBody
ClientMsgEnvelope (PubHeader, cmNonce, Tail cmEncBody)
  ↓ decrypt cmEncBody with e2eDhSecret + cmNonce
ClientMessage / AgentMsgEnvelope
```

**No comma separators!** `smpEncode a <> smpEncode b` — direct concatenation.

---

## 11. Important Source Locations

### 11.1 Haskell

| Function | File | Lines |
|----------|------|-------|
| agentCbEncrypt | Agent/Client.hs | 1925-1933 |
| cryptoBox | Crypto.hs | 1295-1298 |
| xSalsa20 | Crypto.hs | 1449-1456 |
| sbDecryptNoPad_ | Crypto.hs | 1325-1333 |
| e2eDhSecret | Agent.hs | 3379 |
| ICDuplexSecure | Agent.hs | 1549-1551 |

### 11.2 SimpleGo

| Function | File |
|----------|------|
| E2E Decrypt | main.c:780-850 |
| Queue Create | smp_queue.c:210 |
| Queue Encode | smp_queue.c:455 |
| Peer Connect | smp_peer.c:50 |

---

## 12. Evgeny Quotes (Authoritative)

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

## 13. Decrypted Content Preview (Session 18)

### 13.1 First Bytes After E2E Decrypt

```
[0]     = 0x3a ':' → PrivHeader Type (NEW — must identify!)
[1]     = 0xae     → Length byte?
[2-14]  = Ed25519 SPKI (4b 2c 30 2a 30 05 06 03 2b 65 70 03 21 00)
[...]   = 00 07 43 → Agent Version 7, 'C' = AgentConfirmation
[...]   = 30 7b 00 02 → '0' (Nothing) + 0x7b = EncRatchetMessage Start
```

### 13.2 Open Questions (Session 19)

1. What is PrivHeader ':' (0x3a)? — Not PHConfirmation='K', not PHEmpty='_'
2. Parse AgentConfirmation → e2eEncryption + encConnInfo
3. Decrypt EncRatchetMessage with Double Ratchet

---

## 14. Session 15 Theory (DISPROVEN in Session 16)

### 14.1 Session 15 Claimed

```
App's HELLO on Reply Queue has maybe_e2e = Nothing
-> Uses pre-computed e2eDhSecret
-> We need app.sndQueue.e2ePubKey
-> Key is in App's AgentConfirmation
-> WE DON'T RECEIVE THIS MESSAGE!
```

### 14.2 Evgeny's Response (Session 16)

> "sender's public DH key sent in confirmation header - this is
> **outside of AgentConnInfoReply but in the same message**"

**The key IS in the message header! NO second message needed!**

---

## 15. The Real Problem (Sessions 16-17, SOLVED in Session 18!)

### 15.1 Session 16-17: Double Ratchet Theory

```
Session 16: Peer CANNOT decrypt our AgentConfirmation
  - Android: "Request to connect"
  - Desktop: "Connecting"
  Suspected: rcAD order, X3DH DH order, HKDF params

Session 17: Key Consistency Investigation
  - Key mismatch in logs (later: different test runs)
  - rcAD order tested (staying OUR||PEER)
```

### 15.2 Session 18: ACTUAL Root Cause

```
NOT a crypto problem!
NOT a Double Ratchet problem!
NOT a key problem!

ACTUAL: envelope_len included 102 bytes SMP block-padding
FIX: envelope_len = raw_len_prefix (ONE LINE!)
RESULT: 15904 bytes decrypted successfully!
```

---

*Quick Reference v12.0*  
*Last updated: February 5, 2026 - Session 18*  
*Status: 🎉 BUG #18 SOLVED! E2E Layer 2 Decrypt SUCCESS!*  
*Next: Parse AgentConfirmation, decrypt EncRatchetMessage*
