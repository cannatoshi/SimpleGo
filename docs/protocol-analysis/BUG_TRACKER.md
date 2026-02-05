# Bug Tracker

## Complete Documentation of All 18 Bugs

This document provides detailed documentation of all bugs discovered during SimpleGo development, including the incorrect code, correct code, and root cause analysis.

---

## Summary

| Bug # | Component | Session | Status |
|-------|-----------|---------|--------|
| 1 | E2E key length | 4 | FIXED |
| 2 | prevMsgHash length | 4 | FIXED |
| 3 | MsgHeader DH key | 4 | FIXED |
| 4 | ehBody length | 4 | FIXED |
| 5 | emHeader size | 4 | FIXED |
| 6 | Payload AAD size | 4 | FIXED |
| 7 | Root KDF output order | 4 | FIXED |
| 8 | Chain KDF IV order | 4 | FIXED |
| 9 | wolfSSL X448 byte order | 5 | FIXED |
| 10 | Port encoding | 6 | FIXED |
| 11 | smpQueues count | 6 | FIXED |
| 12 | queueMode Nothing | 6 | FIXED |
| 13 | Payload AAD length prefix | 8 | FIXED |
| 14 | chainKdf IV assignment | 8 | FIXED |
| 15 | Reply Queue HSalsa20 | 9 | FIXED |
| 16 | A_CRYPTO header AAD | 9 | FIXED |
| 17 | cmNonce instead of msgId | 10C | FIXED |
| **18** | **Reply Queue E2E** | **12-18** | **✅ SOLVED** |

**Total: 18 bugs documented, 18 FIXED! 🎉**

---

## Bug #1: E2E Key Length Prefix

**Session:** 4  
**Component:** E2ERatchetParams encoding  
**Impact:** Critical - causes parsing failure

### Incorrect Code
```c
// Word16 BE length prefix (WRONG!)
buf[p++] = 0x00;
buf[p++] = 0x44;  // 68 as Word16
memcpy(&buf[p], spki_key, 68);
```

### Correct Code
```c
// 1-byte length prefix (CORRECT!)
buf[p++] = 0x44;  // 68 as single byte
memcpy(&buf[p], spki_key, 68);
```

### Root Cause

E2ERatchetParams keys are encoded as ByteString (1-byte prefix), not Large (Word16 prefix).

---

## Bug #2: prevMsgHash Length Prefix

**Session:** 4  
**Component:** AgentMessage encoding  
**Impact:** Critical - causes parsing failure

### Incorrect Code
```c
// 1-byte length prefix (WRONG!)
buf[p++] = 0x00;  // Empty hash
```

### Correct Code
```c
// Word16 BE length prefix (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x00;  // Empty hash as Word16
```

### Root Cause

AgentMessage uses Large wrapper for prevMsgHash, requiring Word16 prefix.

---

## Bug #3: MsgHeader DH Key Length

**Session:** 4  
**Component:** MsgHeader encoding  
**Impact:** Critical - causes parsing failure

### Incorrect Code
```c
// Word16 BE length prefix (WRONG!)
buf[p++] = 0x00;
buf[p++] = 0x44;
memcpy(&buf[p], dh_key_spki, 68);
```

### Correct Code
```c
// 1-byte length prefix (CORRECT!)
buf[p++] = 0x44;
memcpy(&buf[p], dh_key_spki, 68);
```

### Root Cause

MsgHeader msgDHRs is PublicKey, encoded as ByteString with 1-byte prefix.

---

## Bug #4: ehBody Length Prefix

**Session:** 4  
**Component:** EncMessageHeader encoding  
**Impact:** Critical - cascades to bugs #5 and #6

### Incorrect Code
```c
// Word16 BE length prefix (WRONG!)
em_header[hp++] = 0x00;
em_header[hp++] = 0x58;  // 88 as Word16
```

### Correct Code
```c
// 1-byte length prefix (CORRECT!)
em_header[hp++] = 0x58;  // 88 as single byte
```

### Root Cause

ehBody is ByteString, not Large.

---

## Bug #5: emHeader Size

**Session:** 4  
**Component:** EncMessageHeader structure  
**Impact:** Critical - cascades to bug #6

### Incorrect Code
```c
#define EM_HEADER_SIZE 124
uint8_t em_header[124];
```

### Correct Code
```c
#define EM_HEADER_SIZE 123
uint8_t em_header[123];
```

### Root Cause

Cascaded from Bug #4 - with 1-byte prefix, size is 123 not 124.

---

## Bug #6: Payload AAD Size

**Session:** 4  
**Component:** AES-GCM AAD  
**Impact:** Critical - auth tag mismatch

### Incorrect Code
```c
uint8_t payload_aad[236];  // WRONG!
aes_gcm_encrypt(..., payload_aad, 236, ...);
```

### Correct Code
```c
uint8_t payload_aad[235];  // CORRECT!
aes_gcm_encrypt(..., payload_aad, 235, ...);
```

### Root Cause

Cascaded from Bug #5 - AAD = 112 + 123 = 235, not 236.

---

## Bug #7: Root KDF Output Order

**Session:** 4  
**Component:** Root KDF implementation  
**Impact:** Critical - all keys wrong

### Incorrect Code
```c
// Wrong order!
memcpy(chain_key, kdf_output, 32);
memcpy(new_root_key, kdf_output + 32, 32);
```

### Correct Code
```c
// Correct order per Haskell
memcpy(new_root_key, kdf_output, 32);
memcpy(chain_key, kdf_output + 32, 32);
memcpy(next_header_key, kdf_output + 64, 32);
```

### Root Cause

Misread Haskell source - output order is root, chain, header.

---

## Bug #8: Chain KDF IV Order

**Session:** 4  
**Component:** Chain KDF implementation  
**Impact:** Critical - encryption uses wrong IVs

### Incorrect Code
```c
// Swapped! (WRONG!)
memcpy(msg_iv, kdf_output + 64, 16);
memcpy(header_iv, kdf_output + 80, 16);
```

### Correct Code
```c
// Correct order!
memcpy(header_iv, kdf_output + 64, 16);  // iv1 = header
memcpy(msg_iv, kdf_output + 80, 16);     // iv2 = message
```

### Root Cause

iv1 (bytes 64-79) is header IV, iv2 (bytes 80-95) is message IV.

---

## Bug #9: wolfSSL X448 Byte Order

**Session:** 5  
**Component:** X448 cryptography  
**Impact:** Critical - all DH computations wrong

### The Problem

wolfSSL X448 uses little-endian, SimpleX expects big-endian.

### The Fix
```c
static void reverse_bytes(const uint8_t *src, uint8_t *dst, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
}

// After key generation:
reverse_bytes(pub_tmp, keypair->public_key, 56);
reverse_bytes(priv_tmp, keypair->private_key, 56);

// Before DH:
reverse_bytes(their_public, their_public_rev, 56);
reverse_bytes(my_private, my_private_rev, 56);

// After DH:
reverse_bytes(secret_tmp, shared_secret, 56);
```

### Root Cause

wolfSSL defines EC448_LITTLE_ENDIAN internally.

---

## Bug #10: Port Encoding

**Session:** 6  
**Component:** SMPQueueInfo encoding  
**Impact:** Critical - parser fails

### Incorrect Code
```c
// Length prefix (WRONG!)
buf[p++] = (uint8_t)strlen(port_str);
memcpy(&buf[p], port_str, strlen(port_str));
```

### Correct Code
```c
// Space separator (CORRECT!)
buf[p++] = ' ';  // 0x20
memcpy(&buf[p], port_str, strlen(port_str));
```

### Root Cause

SMPServer encoding uses space separator, not length prefix.

---

## Bug #11: smpQueues Count

**Session:** 6  
**Component:** NonEmpty list encoding  
**Impact:** Critical - parser fails

### Incorrect Code
```c
// 1-byte count (WRONG!)
buf[p++] = 0x01;
```

### Correct Code
```c
// Word16 BE count (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x01;
```

### Root Cause

NonEmpty list uses Word16 for count.

---

## Bug #12: queueMode Nothing

**Session:** 6  
**Component:** SMPQueueInfo encoding  
**Impact:** Medium - parser might fail

### Incorrect Code
```c
// Send '0' byte (WRONG!)
buf[p++] = '0';  // 0x30
```

### Correct Code
```c
// Send NOTHING (CORRECT!)
// (no code - just don't write anything)
```

### Root Cause

queueMode uses "maybe empty" not standard Maybe encoding.

---

## Bug #13: Payload AAD Length Prefix (SESSION 8 BREAKTHROUGH!)

**Session:** 8  
**Component:** Payload AAD construction  
**Impact:** Critical - AgentConfirmation rejected

### The Discovery

Haskell `largeP` parser removes length prefix from parsed object:
```haskell
largeP :: Parser a -> Parser a
largeP p = smpP >>= \len -> A.take (fromIntegral (len :: Word16)) >>= parseAll p
```

### Incorrect Code
```c
// AAD with length prefix (WRONG!)
uint8_t payload_aad[237];  // 2 + 112 + 123
payload_aad[0] = (total_len >> 8) & 0xFF;  // Length prefix
payload_aad[1] = total_len & 0xFF;
memcpy(&payload_aad[2], header_aad, 112);
memcpy(&payload_aad[114], em_header, 123);
```

### Correct Code
```c
// AAD WITHOUT length prefix (CORRECT!)
uint8_t payload_aad[235];  // 112 + 123
memcpy(&payload_aad[0], header_aad, 112);
memcpy(&payload_aad[112], em_header, 123);
```

### Root Cause

The length prefix is consumed by the parser, not included in AAD.

---

## Bug #14: chainKdf IV Assignment (SESSION 8)

**Session:** 8  
**Component:** Chain KDF IV handling  
**Impact:** Critical - wrong IVs used for encryption

### The Discovery

Session 4 found the order but assignment was still swapped later.

### Incorrect Code
```c
// Assignments swapped (WRONG!)
uint8_t *header_iv = &chain_kdf_output[80];  // iv2
uint8_t *msg_iv = &chain_kdf_output[64];     // iv1
```

### Correct Code
```c
// Correct assignments!
uint8_t *header_iv = &chain_kdf_output[64];  // iv1 = header
uint8_t *msg_iv = &chain_kdf_output[80];     // iv2 = message
```

### Root Cause

Chain KDF output layout:
```
[0:32]   next_chain_key
[32:64]  message_key
[64:80]  iv1 = HEADER_IV
[80:96]  iv2 = MESSAGE_IV
```

---

## Bug #15: Reply Queue HSalsa20 (SESSION 9)

**Session:** 9  
**Component:** Reply Queue E2E decryption  
**Impact:** Critical - Reply Queue decrypt fails

### The Discovery

NaCl `crypto_box` includes HSalsa20 key derivation internally.

### Incorrect Code
```c
// crypto_scalarmult only does raw X25519 (WRONG!)
crypto_scalarmult(dh_secret, rcv_dh_private, srv_dh_public);
// dh_secret is RAW, not ready for XSalsa20-Poly1305!
```

### Correct Code
```c
// crypto_box_beforenm does X25519 + HSalsa20 (CORRECT!)
crypto_box_beforenm(dh_secret, srv_dh_public, rcv_dh_private);
// dh_secret is NOW ready for crypto_box_open_easy_afternm!
```

### NaCl Crypto Layers
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

### Root Cause

Must use same crypto primitive chain as sender.

---

## Bug #16: A_CRYPTO Header AAD (SESSION 9)

**Session:** 9  
**Component:** Header encryption AAD  
**Impact:** Critical - A_CRYPTO error in app

### The Problem

Header encryption AAD format was incorrect.

### Root Cause

Incorrect AAD construction for header encryption causing authentication failure.

---

## Bug #17: cmNonce instead of msgId (SESSION 10C)

**Session:** 10C  
**Component:** Per-Queue E2E Decryption  
**Impact:** Critical - All Contact Queue messages fail decryption

### The Discovery

Used `msgId` as nonce for per-queue E2E decryption, but the correct nonce is `cmNonce` from the ClientMsgEnvelope structure.

### ClientMsgEnvelope Structure
```
Offset  Size  Content
------  ----  -------
[0-1]   2     length prefix
[12-13] 2     version
[14]    1     maybe tag
[15]    1     maybe tag for e2ePubKey
[16-59] 44    X25519 SPKI
[60-83] 24    cmNonce <- CORRECT NONCE!
[84+]   var   cmEncBody
```

### Incorrect Code
```c
// WRONG - used msgId as nonce
memcpy(nonce, msg_id, msgIdLen);  // msgId from MSG header
```

### Correct Code
```c
// CORRECT - extract cmNonce from ClientMsgEnvelope
int cm_nonce_offset = spki_offset + 44;  // [60-83]
memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);

// Then decrypt with cmNonce
crypto_box_open_easy_afternm(plain, &data[cm_enc_body_offset], 
                              enc_len, cm_nonce, dh_shared);
```

### Result After Fix
```
TEST4 SUCCESS! Per-queue E2E decrypt worked!
   Decrypted 15904 bytes (ClientMessage)
   PrivHeader tag: 'K' (PHConfirmation)

App status: "connecting"
```

---

## Bug #18: Reply Queue E2E Decryption — ✅ SOLVED!

**Sessions:** 12, 13, 14, 15, 16, 17, 18  
**Component:** Reply Queue Per-Queue E2E Layer 2 → envelope_len calculation  
**Impact:** Cannot decrypt Reply Queue messages  
**Status:** ✅ **SOLVED in Session 18!**

### 18.0 FINAL ROOT CAUSE & FIX (Session 18, 2026-02-05) 🎉

#### Root Cause

```
envelope_len = plain_len - 2 = 16104       ← WRONG! Includes 102B SMP padding
envelope_len = raw_len_prefix = 16002      ← CORRECT! Exact content length

plain_len:      16106 (total decrypted from Layer 1)
raw_len_prefix: 16002 (actual ClientMsgEnvelope length from 2-byte prefix)
prefix_bytes:   2     (length prefix itself)
padding:        102   (16106 - 16002 - 2 = SMP block-padding 0x23)
```

#### The Fix — ONE LINE!

```c
// BEFORE (WRONG):
size_t envelope_len = plain_len - rq_prefix_len;  // Includes 102B SMP padding!

// AFTER (CORRECT):
size_t envelope_len = raw_len_prefix;  // Use actual length from 2-byte prefix
```

#### Result

```
Method 0 (decrypt_client_msg): SUCCESS!
Decrypted: 15904 bytes
Content: AgentConfirmation + EncRatchetMessage

Decrypted plaintext:
  [0]     = 0x3a ':' → PrivHeader Type (new, must identify)
  [2-14]  = Ed25519 SPKI (OID 1.3.101.112)
  [...]   = 00 07 43 → Agent Version 7, 'C' = AgentConfirmation
  [...]   = 30 7b 00 02 → '0' (Nothing) + 0x7b = EncRatchetMessage
```

#### Wire-Format Discovery (Claude Code Analysis #1)

Our code assumed WRONG offsets for ClientMsgEnvelope:

```
WRONG (our assumption):
  [0-7]   = Message Header (Timestamps)
  [8-9]   = "T " (PubHeader Tag + Space)
  [10-11] = Version
  [12]    = maybe_corrId        ← DOES NOT EXIST IN ENVELOPE!
  [13]    = maybe_e2e           ← WRONG OFFSET!
  
CORRECT (Haskell reality):
  ClientMsgEnvelope = (PubHeader, cmNonce, Tail cmEncBody)
  PubHeader = (phVersion, Maybe phE2ePubDhKey)

  WITH e2ePubKey:
  [0-1]   = phVersion (00 04)
  [2]     = '1' (0x31) = Just
  [3-46]  = X25519 SPKI (44 bytes)
  [47-70] = cmNonce (24 bytes, raw)
  [71+]   = cmEncBody (rest)
  
  WITHOUT e2ePubKey:
  [0-1]   = phVersion (00 04)
  [2]     = '0' (0x30) = Nothing
  [3-26]  = cmNonce (24 bytes, raw)
  [27+]   = cmEncBody (rest)
```

#### Key Insight: corrId is NOT in ClientMsgEnvelope

corrId is SMP Transport Layer — parsed BEFORE the envelope, not inside it.

#### Contact Queue vs Reply Queue Architecture

| Queue | Layer 1 (Server) | Layer 2 (E2E) |
|-------|-------------------|---------------|
| Contact Queue | ✅ decrypt_smp_message | ❌ NO E2E Layer! |
| Reply Queue | ✅ decrypt_smp_message | ✅ E2E decrypt |

**Contact Queue never had a separate E2E layer!** The assumption "Contact Queue E2E works" was FALSE — it only has Layer 1 (server decryption).

### 18.1 Session 12 Discoveries

**Two Separate X25519 Keypairs:**

| Keypair | Purpose | Used in |
|---------|---------|---------|
| dhKey / privDhKey | Server-level DH (NEW command) | rcvDhSecret |
| e2eDhKey / e2ePrivKey | E2E-level DH (Peer encryption) | SMPQueueAddress |

**Changes Implemented:**
```c
// smp_queue.h - Added E2E keys
uint8_t e2e_public[32];
uint8_t e2e_private[32];

// smp_queue.c - Generate E2E keypair
crypto_box_keypair(our_queue.e2e_public, our_queue.e2e_private);

// smp_queue.c - Send e2e_public in SMPQueueInfo
memcpy(&buf[p], our_queue.e2e_public, 32);
```

### 18.2 Session 13 Discoveries

#### 18.2.1 Parsing Bug Fixed

**Old (Wrong):**
```c
uint8_t maybe_corrId = plain[14];  // Wrong name!
uint8_t maybe_e2e = plain[15];     // This is SPKI length, not a tag!
```

**New (Correct):**
```c
uint8_t maybe_e2e = plain[14];     // '1' = Just, '0'/',' = Nothing
// [15] = 0x2c = 44 = SPKI length
// [16-59] = X25519 SPKI (e2ePubKey!)
// [60-83] = cmNonce
// [84+] = cmEncBody
```

#### 18.2.2 HSalsa20 Difference Discovered

| Step | Haskell | libsodium |
|------|---------|-----------|
| 1 | DH(pub, priv) -> secret | DH(pub, priv) -> secret |
| 2 | XSalsa20(secret, nonce, msg) | **HSalsa20(secret)** -> key |
| 3 | - | XSalsa20(key, nonce, msg) |

**Haskell xSalsa20 (Direct):**
```haskell
cryptoBox secret nonce s = BA.convert tag <> c
  where
    (rs, c) = xSalsa20 secret nonce s  -- Direct!
```

**libsodium (Extra HSalsa20):**
```c
crypto_box_beforenm(k, peer_pub, our_priv);  // HSalsa20 applied here!
crypto_box_open_easy_afternm(..., k);
```

#### 18.2.3 MAC Position Difference Discovered

**Haskell cbDecrypt:**
```haskell
(tag', c) = B.splitAt 16 packet  -- TAG = first 16 bytes!
```

| Format | Layout |
|--------|--------|
| **Haskell** | `[MAC 16 bytes][Ciphertext]` |
| **libsodium** | `[Ciphertext][MAC 16 bytes]` |

### 18.3 Session 14 Discoveries - DH SECRET VERIFIED!

#### 18.3.1 Handoff Theory DISPROVEN

| Statement | Handoff Document | Reality (Source Code) |
|-----------|------------------|----------------------|
| 2 MSGs on Contact Queue | Claimed | FALSE |
| HELLO on Reply Queue | Not mentioned | TRUE (confirmed) |
| E2E Key in PHConfirmation | Claimed | FALSE |
| E2E Key in PubHeader | Not mentioned | TRUE (confirmed) |

#### 18.3.2 Bug Fixed: Wrong Key Used

**Before (WRONG):**
```c
// Used SMP DH key from INVITATION
crypto_box_beforenm(e2e_dh_secret, pending_peer.dh_public, our_queue.e2e_private);
```

**After (CORRECT):**
```c
// Extract e2ePubKey from message header (Offset 28)
uint8_t peer_e2e_pub[32];
memcpy(peer_e2e_pub, &server_plain[28], 32);
```

#### 18.3.3 Bug Fixed: Wrong DH Function

**Before (WRONG):**
```c
crypto_box_beforenm(e2e_dh_secret, peer_pub, our_priv);
// ^^^ applies HSalsa20 key derivation!
```

**After (CORRECT):**
```c
crypto_scalarmult(dh_secret, our_queue.e2e_private, peer_e2e_pub);
// ^^^ raw DH output - matches Haskell!
```

#### 18.3.4 DH Secret VERIFIED with Python!

```python
from nacl.bindings import crypto_scalarmult

our_private = bytes.fromhex('83473153de033039edec9c5db7591cacfa42b6dd89a0618a00806732d01a96fa')
peer_public = bytes.fromhex('9140e10e9fdee92ebb801ae8694435b5e9f06c4e0077dfa98d39b0f1bf0c0300')

dh_secret = crypto_scalarmult(our_private, peer_public)
```

**Result:**
```
Python DH:  d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810
ESP32 DH:   d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810
Match: TRUE!
```

### 18.5 Session 15 - ROOT CAUSE FOUND! (Later Disproven in S16)

#### 18.5.1 Critical Discovery: `maybe_e2e = ','` (Nothing)

The Reply Queue HELLO message has:
```
[14] = '1' (0x31) = maybe_corrId = Just
[15] = ',' (0x2C) = maybe_e2e = Nothing  <- CRITICAL!
```

When `maybe_e2e = Nothing`:
- **NO ephemeral e2ePubKey** in the message
- Message uses **pre-computed e2eDhSecret**
- We need `app.sndQueue.e2ePubKey` to calculate same secret
- This key is NOT in the message!

#### 18.5.2 The Missing Key

**App side calculates:**
```
e2eDhSecret = DH(our_queue.e2e_public, app.sndQueue.e2ePrivKey)
```

**ESP32 needs:**
```
dh_secret = DH(app.sndQueue.e2ePubKey, our_queue.e2e_private)
            ^^^^^^^^^^^^^^^^^^^^
            WE DON'T HAVE THIS KEY!
```

#### 18.5.3 Where is the Key?

According to SimpleX protocol, `sndQueue.e2ePubKey` is sent in:
- **App's AgentConfirmation** (Type 'C')
- Arrives on our **Contact Queue**
- As response to our connection request

**PROBLEM:** We don't receive this message!

#### 18.5.4 Protocol Flow Issue

```
✅ Step 1: INVITATION received on Contact Queue
✅ Step 2: AgentConfirmation sent -> Server: "OK"
✅ Step 3: HELLO sent -> Server: "OK"
❌ Step 4: App's AgentConfirmation NOT received!
❌ Step 5: App's HELLO received, cannot decrypt
```

#### 18.5.5 Tests Performed (All Failed)

| Test | Key Source | DH Result | Decrypt |
|------|------------|-----------|---------|
| 1 | URL dh= key | 685e7514... | FAILED |
| 2 | Message corrId | 3863509c... | FAILED |
| 3 | Offsets 48-80 | Various | All FAILED |
| 4 | X25519 search | 0 found | N/A |

**Conclusion:** The needed key is NOT in the data we have.

### 18.6 Session 16 - Correction Session

#### 18.6.1 Session 15 Theory DISPROVEN

Evgeny confirmed: **"in the same message"**

| Session 15 Said | Evgeny Says |
|-----------------|-------------|
| "2nd MSG missing" | "in the same message" |
| "Key never arrives" | "sender's public DH key sent in confirmation header" |

**The key IS in the message header!**

#### 18.6.2 SimpleX NON-STANDARD XSalsa20 Discovered

```
Standard libsodium crypto_secretbox:
  HSalsa20(dh_secret, nonce[0:16])

SimpleX xSalsa20:
  HSalsa20(dh_secret, zeros[16])  <- ZEROS not nonce!
```

**Python test proof:**
```
Standard subkey:  2d4b452885522...
SimpleX subkey:   ce1b436c8b333...
COMPLETELY DIFFERENT!
```

**All previous crypto attempts were DOOMED!**

#### 18.6.3 Custom XSalsa20 Implemented

New files created:
- `simplex_crypto.c` - Custom XSalsa20 for ESP32
- `simplex_crypto.h` - Header
- `test_simplex_crypto.c` - C test
- `verify_simplex_crypto.py` - Python verification

**Round-trip test: SUCCESS ✅**

#### 18.6.4 Key Race Condition Fixed

```c
// BUG: reply_queue_e2e_peer_public written from TWO places:
// 1. main.c:642 → Contact Queue PubHeader SPKI (CORRECT!)
// 2. smp_parser.c:746 → AgentConnInfoReply Parser (WRONG!)

// FIX: Removed overwrite in smp_parser.c
```

#### 18.6.5 Problem Shifted to Double Ratchet

```
Layer 4 (E2E): FIXED ✅
Layer 5 (Double Ratchet): BROKEN ❌

Peer cannot decrypt our AgentConfirmation!
- Android: "Request to connect" (Confirmation NOT understood)
- Desktop: "Connecting" (trying but failing)
```

#### 18.6.6 Verified CORRECT

| Component | Status |
|-----------|--------|
| Wire-Format Parsing | ✅ CORRECT |
| Payload AAD (235 bytes) | ✅ CORRECT |
| Header AAD | ✅ CORRECT |
| emHeader Encoding | ✅ CORRECT |
| Key Consistency | ✅ CORRECT |
| Custom XSalsa20 | ✅ VERIFIED |

### 18.7 Session 17 - Key Consistency Debug

#### 18.7.1 Evgeny Already Answered!

On January 28, 2026 Evgeny already told us:
- Key is in **confirmation header** (SPKI in message header)
- "outside of AgentConnInfoReply but in the same message"
- TWO crypto_box layers with different keys and nonces

**Rule: ALWAYS search past Evgeny conversations before asking!**

#### 18.7.2 rcAD Order Analysis

```
rcAD = sk1 || rk1 = JOINER_KEY || INITIATOR_KEY
     = APP_KEY    || ESP32_KEY  = PEER || OUR
```

Test result: PEER||OUR was WORSE than OUR||PEER.
Staying with OUR||PEER for now.

#### 18.7.3 Length Prefix Discovery

```
Contact Queue: No length prefix before ClientMsgEnvelope
Reply Queue:   2-byte length prefix (e.g. 0x3E82 = 16002)
```

Fix applied: offset +2 for Reply Queue.

#### 18.7.4 Key Mismatch in Logs (Resolved in S18)

```
Queue Creation:      e2e_private = c4cd6fd7...
Reply Queue Decrypt: e2e_private = 6156a27f...
```

**Resolution:** Different test runs (not same session). 3-point logging in S18 confirmed keys are CONSISTENT.

### 18.8 Session 18 - 🎉 BUG #18 SOLVED!

#### 18.8.1 Wire-Format Root Cause

Our code parsed ClientMsgEnvelope with completely wrong assumptions:
- Assumed corrId exists in envelope (WRONG — it's SMP Transport Layer)
- Used `plain_len - 2` for envelope length (WRONG — includes 102B SMP padding)
- All offsets shifted, causing MAC mismatch

#### 18.8.2 Claude Code Analyses

**Analysis #1:** ClientMsgEnvelope wire-format
- corrId NOT in envelope
- Structure: `(PubHeader, cmNonce, Tail cmEncBody)`
- No comma separators

**Analysis #2:** Tuple-encoding + wrapper chain
- `smpEncode a <> smpEncode b` (direct concatenation)
- Wrapper: `ClientRcvMsgBody {msgTs, msgFlags, msgBody}`
- Chain: EncRcvMsgBody → ClientRcvMsgBody → ClientMsgEnvelope → ClientMessage

#### 18.8.3 Evidence from ESP Log

```
plain_len:      16106 (total decrypted from Layer 1)
raw_len_prefix: 16002 (actual content length)
Difference:     102 bytes = 0x23 SMP block-padding
```

Contact Queue parser correctly used prefix_len → works.
Reply Queue parser used plain_len - 2 → includes padding → fails.

#### 18.8.4 The Fix

```c
envelope_len = raw_len_prefix;  // ONE LINE!
```

#### 18.8.5 Result

```
Method 0 (decrypt_client_msg): SUCCESS!
15904 bytes decrypted
Content: PrivHeader ':' + Ed25519 SPKI + AgentConfirmation + EncRatchetMessage
```

#### 18.8.6 Excluded Theories (Final)

| Theory | Status | Evidence |
|--------|--------|----------|
| our_queue.e2e_private gets overwritten | WRONG | 3-point logging: identical |
| Wrong key in Invitation | WRONG | Consistency test: all 3 match |
| Byte offsets were correct | **WRONG** | Session 18: offsets completely wrong |
| corrId SPKI = E2E Key | WRONG | Claude Code: corrId is Transport Layer |
| Contact Queue has E2E Layer 2 | **WRONG** | Code analysis: only Layer 1! |
| corrId exists in Envelope | **WRONG** | Claude Code #1: corrId is SMP Transport |

### 18.9 All Crypto Tests (Session 13)

| Test | Method | MAC | Private Key | Result |
|------|--------|-----|-------------|--------|
| 1 | crypto_box_open_easy | Auto | e2e_private | FAILED |
| 2 | crypto_box_open_easy | Auto | rcv_dh_private | FAILED |
| 3 | crypto_secretbox_open_easy | None | e2e_private | FAILED |
| 4 | crypto_secretbox_open_easy | Reordered | e2e_private | FAILED |
| 5 | crypto_secretbox_open_detached | Separate | e2e_private | FAILED |

### 18.4 Verified Correct

- Key extraction: [28-59] = Raw X25519 key
- Nonce extraction: [60-83] = 24 bytes
- Body offset: [84+] = cmEncBody
- Keypair verification: e2e_public matches derived from e2e_private
- SPKI header: `30 2a 30 05 06 03 2b 65 6e 03 21 00`

### 18.5 SMPConfirmation Contains e2ePubKey!

**Found in Haskell:**
```haskell
data SMPConfirmation = SMPConfirmation
  { senderKey :: Maybe SndPublicAuthKey,
    e2ePubKey :: C.PublicKeyX25519,        -- THE KEY!
    connInfo :: ConnInfo,
    smpReplyQueues :: [SMPQueueInfo],
    smpClientVersion :: VersionSMPC
  }
```

### 18.6 App's e2ePubKey Flow

```haskell
newSndQueue ... {dhPublicKey = rcvE2ePubDhKey} = do
  (e2ePubKey, e2ePrivKey) <- generateKeyPair
  let sq = SndQueue
        { e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey,  -- Pre-computed!
          e2ePubKey = Just e2ePubKey,                     -- App's key
        }
```

1. App receives our `e2e_public` from SMPQueueInfo
2. App generates its own keypair
3. App **first** message: `e2ePubKey = Just` (sendConfirmation)
4. App **subsequent** messages: `e2ePubKey = Nothing` (sendAgentMessage)

### 18.11 Sub-Issues Status (FINAL — All Resolved)

| Sub-Issue | Description | Status |
|-----------|-------------|--------|
| #18a | Separate E2E Keypair implemented | DONE |
| #18b | E2E public sent in SMPQueueInfo | DONE |
| #18c | Parsing fix (correct offsets) | DONE |
| #18d | HSalsa20 difference identified | DONE |
| #18e | MAC position difference identified | DONE |
| #18f | 5 crypto approaches tested (S13) | DONE - All fail |
| #18g | SMPConfirmation contains e2ePubKey | FOUND |
| #18h | Handoff theory DISPROVEN (S14) | DONE |
| #18i | Wrong key bug fixed (S14) | DONE |
| #18j | Wrong DH function fixed (S14) | DONE |
| #18k | DH SECRET VERIFIED with Python! (S14) | DONE |
| #18l | maybe_e2e = Nothing discovered (S15) | DONE |
| #18m | Pre-computed secret required (S15) | DONE |
| #18n | Missing App's AgentConfirmation (S15) | DISPROVEN (S16) |
| #18o | app.sndQueue.e2ePubKey identified (S15) | DISPROVEN (S16) |
| #18p | ROOT CAUSE IDENTIFIED (S15) | WRONG (S16) |
| #18q | Session 15 theory DISPROVEN (S16) | DONE |
| #18r | SimpleX custom XSalsa20 discovered (S16) | DONE |
| #18s | simplex_crypto.c implemented (S16) | DONE |
| #18t | Custom XSalsa20 verified (S16) | DONE |
| #18u | Key race condition fixed (S16) | DONE |
| #18v | Wire-format verified correct (S16) | DONE |
| #18w | Problem is Double Ratchet (S16) | IDENTIFIED |
| #18x | rcAD order analyzed (S17) | DONE - staying OUR\|\|PEER |
| #18y | Length prefix fix (S17) | DONE |
| #18z | Key consistency check (S17) | DONE - keys consistent |
| **#18aa** | **Contact Queue has NO E2E Layer 2 (S18)** | **DONE** |
| **#18ab** | **ClientMsgEnvelope wire-format analyzed (S18)** | **DONE** |
| **#18ac** | **Tuple-encoding: no comma separators (S18)** | **DONE** |
| **#18ad** | **Wrapper chain documented (S18)** | **DONE** |
| **#18ae** | **Root cause: 102B SMP padding in envelope_len (S18)** | **DONE** |
| **#18af** | **Fix: envelope_len = raw_len_prefix (S18)** | **DONE** |
| **#18ag** | **E2E Layer 2 decrypt SUCCESS: 15904 bytes (S18)** | **✅ SOLVED** |

### 18.12 Android vs Desktop Difference

| Aspect | Desktop | Android |
|--------|---------|---------|
| URI extraction | SUCCESS (2090 chars) | FAILED |
| Peer-Connect | YES | NO |
| AgentConfirmation | Sent | Not sent |
| Display | "Connecting..." | No status |
| Padding prefix | `2a fc 5f...` | `09 e7 5f...` |

---

## Bug Discovery Timeline

| Date | Session | Bugs Found |
|------|---------|------------|
| Jan 23, 2026 | S4 | #1-#6 |
| Jan 24, 2026 | S4 | #7-#8 |
| Jan 24, 2026 | S5 | #9 |
| Jan 24, 2026 | S6 | #10-#12 |
| Jan 27, 2026 | S8 | #13-#14 |
| Jan 27, 2026 | S9 | #15-#16 |
| Jan 28, 2026 | S10C | #17 |
| Jan 30, 2026 | S12-S13 | #18 (deep analysis) |
| Jan 31-Feb 1 | S14 | #18 DH SECRET VERIFIED! |
| Feb 1 | S15 | #18 Root Cause (later disproven) |
| Feb 1-3 | S16 | #18 Custom XSalsa20! |
| Feb 4 | S17 | #18 Key Consistency Debug |
| **Feb 5** | **S18** | **#18 ✅ SOLVED! One-line fix!** |

---

## Bug Categories

```
18 Bugs Total (ALL FIXED! 🎉):
- 7x Length Prefix issues (#1-6, #13)
- 3x KDF/IV Order issues (#7, #8, #14)
- 1x Byte Order issue (#9 - wolfSSL)
- 1x Separator issue (#10)
- 1x Maybe encoding issue (#12)
- 1x AAD construction issue (#13)
- 1x NaCl crypto layer issue (#15 - HSalsa20)
- 1x Header encryption issue (#16)
- 1x Nonce source issue (#17 - cmNonce)
- 1x Envelope length calculation issue (#18 - SMP padding)
```

---

## Lessons Learned

1. **Length encoding varies by context** - always check Haskell source
2. **Crypto libraries differ** - verify against reference implementations
3. **Cascade effects are real** - one bug can cause multiple symptoms
4. **A_MESSAGE != A_CRYPTO** - parsing error vs crypto error
5. **Tail means no prefix** - last fields don't need length
6. **Two pad() functions exist** - Lazy.hs (Int64) vs Crypto.hs (Word16)
7. **Wire format != Crypto format** - length prefixes for serialization, not always for AAD
8. **Haskell parser awareness** - `largeP` removes length prefix from parsed object
9. **Python verification essential** - systematically verify all crypto operations
10. **Community support helps** - SimpleX developers are responsive and helpful
11. **NaCl crypto layers** - crypto_box includes HSalsa20, crypto_scalarmult does not
12. **cmNonce != msgId** - Different nonces for different layers
13. **If it works, don't touch it!** - Session 11 regression
14. **Git is your friend** - Commit at working state, reset when needed
15. **Two keypairs exist** - Server DH vs E2E DH are separate!
16. **HSalsa20 matters** - libsodium adds extra step vs Haskell
17. **MAC position matters** - [MAC][Cipher] vs [Cipher][MAC]
18. **Parse SMPConfirmation** - Contains App's e2ePubKey
19. **Verify theories against source code** - Handoff document was WRONG! (Session 14)
20. **crypto_scalarmult vs crypto_box_beforenm** - Use raw DH, not derived key! (Session 14)
21. **Python verification is proof** - DH Secret match proves crypto basis correct! (Session 14)
22. **maybe_e2e = Nothing means pre-computed** - No key in message, use stored secret! (Session 15)
23. **Two key types in protocol** - dh= for SMP, sndQueue.e2ePubKey for E2E (Session 15)
24. **Missing message = missing key** - App's AgentConfirmation has the e2ePubKey! (Session 15) **DISPROVEN S16**
25. **Protocol flow analysis essential** - Must understand full message sequence! (Session 15)
26. **Ask the developer!** - Evgeny's "in the same message" disproved Session 15 theory! (Session 16)
27. **SimpleX uses NON-STANDARD XSalsa20** - HSalsa20(key, zeros[16]) not nonce[0:16]! (Session 16)
28. **Custom crypto may be needed** - simplex_crypto.c for ESP32 (Session 16)
29. **Key race conditions** - Multiple writes to same variable = bugs! (Session 16)
30. **Self-decrypt failure is BY DESIGN** - Asymmetric header keys (Session 16)
31. **Problem can shift between layers** - L4 fixed, L5 broke (Session 16)
32. **Verify all layers before moving on** - Wire-format ✅, AAD ✅, Keys ✅ (Session 16)
33. **ALWAYS search past Evgeny conversations first!** - He already answered Jan 28 (Session 17)
34. **Length prefix differs per queue** - Reply Queue has 2-byte prefix, Contact Queue doesn't (Session 17)
35. **cmNonce is RANDOM** - Directly in message, not calculated (Session 17)
36. **ALWAYS use length prefix for content boundaries** - Never assume buffer_size - header = content_size! (Session 18)
37. **SMP block-padding exists** - 0x23 padding for traffic analysis resistance, must be excluded! (Session 18)
38. **corrId is SMP Transport, NOT in ClientMsgEnvelope** - Parsed before envelope, not inside it! (Session 18)
39. **Contact Queue has NO E2E Layer 2** - Only server-level decryption, no separate E2E! (Session 18)
40. **Compare working code with broken code** - Contact Queue parser used prefix_len correctly, Reply Queue didn't! (Session 18)
41. **No comma separators in smpEncode** - Direct concatenation: `smpEncode a <> smpEncode b`! (Session 18)
42. **Wrapper chain matters** - EncRcvMsgBody → ClientRcvMsgBody → ClientMsgEnvelope → ClientMessage! (Session 18)
43. **One line can block weeks of progress** - Bug #18 was ONE LINE: envelope_len = raw_len_prefix! (Session 18)

---

*Bug Tracker v13.0*  
*Last updated: February 5, 2026 - Session 18*  
*Total bugs documented: 18 (18 fixed! 🎉)*  
*43 lessons learned!*
