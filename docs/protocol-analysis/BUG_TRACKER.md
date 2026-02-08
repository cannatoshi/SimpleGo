# Bug Tracker

## Complete Documentation of All 31 Bugs

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
| 18 | Reply Queue E2E | 12-18 | FIXED |
| 19 | header_key_recv overwritten | 19-20 | FIXED |
| 20 | PrivHeader for HELLO | 21 | FIXED |
| 21 | AgentVersion for AgentMessage | 21 | FIXED |
| 22 | prevMsgHash encoding | 21 | FIXED |
| 23 | cbEncrypt padding | 21 | FIXED |
| 24 | DH Key for HELLO | 21 | FIXED |
| 25 | PubHeader Nothing encoding | 21 | FIXED |
| 26 | v2/v3 EncRatchetMessage format | 21 | FIXED |
| **27** | **E2E Version Mismatch** | **22** | **FIXED** |
| **28** | **KEM Parser Crash** | **22** | **FIXED** |
| **29** | **Body Decrypt Pointer-Arithmetik** | **22** | **FIXED** |
| **30** | **HKs/NHKs Init + Promotion** | **22** | **FIXED** |
| **31** | **Phase 2a Try-Order** | **22** | **FIXED** |

**Total: 31 bugs documented, 31 FIXED**

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

---

## Bug #18: Reply Queue E2E Decryption — ✅ SOLVED!

**Sessions:** 12, 13, 14, 15, 16, 17, 18  
**Component:** Reply Queue Per-Queue E2E Layer 2 → envelope_len calculation  
**Impact:** Cannot decrypt Reply Queue messages  
**Status:** ✅ **SOLVED in Session 18!**

### Root Cause & Fix

```
ROOT CAUSE:
  envelope_len = plain_len - 2 = 16104       ← WRONG! Includes 102B SMP padding
  envelope_len = raw_len_prefix = 16002      ← CORRECT! Exact content length

FIX — ONE LINE:
  envelope_len = raw_len_prefix;

RESULT:
  Method 0 (decrypt_client_msg): SUCCESS!
  Decrypted: 15904 bytes AgentConfirmation + EncRatchetMessage
```

See Session 18 documentation for full 7-session debugging history.

---

## Bug #19: header_key_recv Gets Overwritten — ✅ SOLVED!

**Sessions:** 19, 20  
**Component:** Double Ratchet key management → debug self-decrypt test  
**Impact:** Medium - header decrypt fails without workaround  
**Status:** ✅ **SOLVED in Session 20!**

### 19.1 Symptom

```
header_key_recv after X3DH = 1c08e86e... (saved_nhk, correct)
header_key_recv at receipt = cf0c74d2... (wrong, overwritten)
```

### 19.2 Root Cause — FOUND (Session 20)

**`smp_peer.c:347`** — Debug self-decrypt test calling `ratchet_decrypt()`.

After encrypting the AgentConfirmation, a debug self-test called `ratchet_decrypt()`
on our own encrypted message. `ratchet_decrypt()` has **side effects**: it performs
a DH ratchet step when it detects a "new" DH key in the decrypted header.

Corrupted: `header_key_recv`, `root_key`, `chain_key_recv`, `dh_peer`, `msg_num_recv`.

### 19.3 Fix Applied (Session 20)

Removed the debug self-decrypt test from `smp_peer.c:343-359`.
Branch: `claude/fix-header-key-recv-bug-DNYeF` → merged to main.

---

## Bug #20: PrivHeader for HELLO (SESSION 21)

**Session:** 21  
**Component:** ClientMessage encoding for HELLO  
**Impact:** Critical - wrong message type indicator

### Incorrect Code
```c
// Used PHEmpty tag (WRONG!)
buf[p++] = '_';  // 0x5F = PHEmpty (Confirmation without key)
```

### Correct Code
```c
// No PrivHeader for regular messages (CORRECT!)
buf[p++] = 0x00;  // No PrivHeader
```

### Root Cause

PrivHeader encoding is NOT a standard Maybe:
- `'K'` (0x4B) = PHConfirmation (with sender auth key)
- `'_'` (0x5F) = PHEmpty (confirmation without key)
- `0x00` = No PrivHeader (regular messages like HELLO)

HELLO is a regular AgentMessage, not a Confirmation.

---

## Bug #21: AgentVersion for AgentMessage (SESSION 21)

**Session:** 21  
**Component:** AgentMsgEnvelope encoding  
**Impact:** Critical - parser version mismatch

### Incorrect Code
```c
// Used Agent protocol version (WRONG!)
buf[p++] = 0x00;
buf[p++] = 0x02;  // agentVersion = 2
```

### Correct Code
```c
// AgentMessage uses version 1 (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x01;  // agentVersion = 1
```

### Root Cause

AgentConfirmation uses agentVersion=7 (protocol version), but AgentMessage (HELLO)
uses agentVersion=1 (message format version). Different fields, different values.

---

## Bug #22: prevMsgHash Encoding (SESSION 21)

**Session:** 21  
**Component:** AgentMessage encoding  
**Impact:** Critical - parser fails on hash field

### Incorrect Code
```c
// Raw empty bytes or missing (WRONG!)
```

### Correct Code
```c
// smpEncode(ByteString) with Word16 prefix (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x00;  // Word16 BE length = 0 (empty hash)
```

### Root Cause

prevMsgHash field uses Large encoding (Word16 prefix). For empty hash: `[0x00][0x00]`.
Related to Bug #2 (same encoding pattern).

---

## Bug #23: cbEncrypt Padding (SESSION 21)

**Session:** 21  
**Component:** Server-level encryption (cbEncrypt)  
**Impact:** Critical - server rejects or app can't decrypt

### Incorrect Code
```c
// Encrypt raw plaintext (WRONG!)
cbEncrypt(key, nonce, raw_plaintext, raw_len, ...);
```

### Correct Code
```c
// Pad BEFORE encrypt (CORRECT!)
pad(raw_plaintext, raw_len, padded_buf, &padded_len);
cbEncrypt(key, nonce, padded_buf, padded_len, ...);
```

### Root Cause

The `pad` function adds a 2-byte length prefix and 0x23 padding BEFORE encryption.
Receiver does: decrypt → unPad. Sender must: pad → encrypt.

---

## Bug #24: DH Key for HELLO (SESSION 21)

**Session:** 21  
**Component:** Per-queue E2E encryption key selection  
**Impact:** Critical - E2E layer fails

### Incorrect Code
```c
// Used receiver's DH key (WRONG!)
compute_e2e_secret(rcv_dh_public, our_private, ...);
```

### Correct Code
```c
// Use sender's DH key for HELLO (CORRECT!)
compute_e2e_secret(snd_dh_public, our_private, ...);
```

### Root Cause

For Confirmation: use `rcv_dh` (receiver's DH key from the queue).
For HELLO: use `snd_dh` (sender's DH key for the reply queue).

---

## Bug #25: PubHeader Nothing Encoding (SESSION 21)

**Session:** 21  
**Component:** ClientMsgEnvelope PubHeader field  
**Impact:** Medium - parser may fail

### Incorrect Code
```c
// Field missing entirely (WRONG!)
```

### Correct Code
```c
// Maybe Nothing = '0' (CORRECT!)
buf[p++] = '0';  // 0x30 = Nothing
```

### Root Cause

PubHeader in ClientMsgEnvelope is a Maybe type. When Nothing, must be encoded
as `'0'` (0x30), not omitted.

---

## Bug #26: v2/v3 EncRatchetMessage Format (SESSION 21)

**Session:** 21  
**Component:** EncRatchetMessage encoding  
**Impact:** Critical - App can't decrypt HELLO (RSYNC error)

### The Discovery

App initialized ratchet with `currentE2EEncryptVersion = 3` (v3), but our
EncRatchetMessage was encoded in v2 format.

### Incorrect Code (v2)
```c
#define RATCHET_VERSION 2
em_header[hp++] = 0x7B;         // emHeader len = 123 (1 byte)
em_header[hp++] = 0x58;         // ehBody len = 88 (1 byte)
#define EM_HEADER_SIZE 123
// No KEM field in MsgHeader
```

### Correct Code (v3)
```c
#define RATCHET_VERSION 3
em_header[hp++] = 0x00;
em_header[hp++] = 0x7C;         // emHeader len = 124 (2 bytes Word16 BE)
em_header[hp++] = 0x00;
em_header[hp++] = 0x58;         // ehBody len = 88 (2 bytes Word16 BE)
#define EM_HEADER_SIZE 124
// KEM Nothing: msg_header[p++] = '0';
```

### Root Cause

`encodeLarge` switches at v≥3: 1-byte (Word8) → 2-byte (Word16 BE) prefix.
Also MsgHeader must include KEM Nothing field in v3.

---

## Bug #27: E2E Version Mismatch (SESSION 22)

**Session:** 22  
**Component:** `smp_x448.c` E2ERatchetParams encoding  
**Impact:** Critical - App breaks silence after fix!

### The Discovery

`smp_x448.c` sent `version_min = 2` in the AgentConfirmation, but `smp_ratchet.c`
encrypted HELLO in v3 format. The version mismatch caused the App to expect v2
format but receive v3.

### Incorrect Code
```c
// In e2e_encode_params():
buf[p++] = 0x00;
buf[p++] = 0x02;  // version_min = 2
// No KEM Nothing-Byte after key2
```

### Correct Code
```c
// In e2e_encode_params():
buf[p++] = 0x00;
buf[p++] = 0x03;  // version_min = 3
// After key2:
buf[p++] = 0x30;  // KEM Nothing ('0' = 0x30)
```

### Root Cause

`smp_x448.c` was not updated in Session 21 when v3 was implemented in `smp_ratchet.c`.
The `version_min` in E2ERatchetParams must match `RATCHET_VERSION` used for encryption.

---

## Bug #28: KEM Parser Crash (SESSION 22)

**Session:** 22  
**Component:** `smp_ratchet.c` MsgHeader parser  
**Impact:** Critical - Parser crash on PQ responses

### The Discovery

App responds with v3 + SNTRUP761 KEM (2310 bytes) instead of 88-byte header.
Parser had fixed offsets → read garbage → crash.

### Incorrect Code
```c
// Fixed offset calculation
int dh_key_offset = 4;  // contentLen(2) + msgMaxVersion(2)
int pn_offset = dh_key_offset + 1 + dh_key_len;  // No KEM handling
```

### Correct Code
```c
// Dynamic KEM handling
int kem_offset = dh_key_offset + 1 + dh_key_len;
uint8_t kem_tag = decrypted_header[kem_offset];
if (kem_tag == '0') {  // Nothing
    pn_offset = kem_offset + 1;
} else if (kem_tag == '1') {  // Just
    uint8_t state_tag = decrypted_header[kem_offset + 1];
    if (state_tag == 'P' || state_tag == 'A') {
        // Read length prefix, skip KEM data
        uint16_t kem_len = (decrypted_header[kem_offset + 2] << 8) | 
                            decrypted_header[kem_offset + 3];
        pn_offset = kem_offset + 4 + kem_len;
    }
}
```

### Root Cause

MsgHeader parser expected 88-byte header without KEM field. v3+PQ headers can be
2346 bytes with SNTRUP761 (1158B pubkey + 1039B ciphertext + overhead).

---

## Bug #29: Body Decrypt Pointer-Arithmetik (SESSION 22)

**Session:** 22  
**Component:** `main.c` body decrypt offset calculation  
**Impact:** Critical - 2GB malloc fail on body decrypt

### The Discovery

emHeader is now 2346 bytes (v3+PQ) instead of 123 bytes (v2), but pointer
calculation for emAuthTag/emBody was hardcoded → garbage offsets → 2GB malloc fail.

### Incorrect Code
```c
#define EM_HEADER_SIZE 124  // Hardcoded
uint8_t *emAuthTag = &encrypted[EM_HEADER_SIZE];
uint8_t *emBody = &encrypted[EM_HEADER_SIZE + 16];
```

### Correct Code
```c
// Read ehVersion to determine size
uint16_t ehVersion = (encrypted[0] << 8) | encrypted[1];
size_t emHeader_size;
if (ehVersion >= 3) {
    // v3: 2-byte length prefix
    emHeader_size = (encrypted[2] << 8) | encrypted[3];
    emHeader_size += 4;  // Include prefix itself
} else {
    // v2: 1-byte length prefix
    emHeader_size = encrypted[2] + 3;
}
uint8_t *emAuthTag = &encrypted[emHeader_size];
uint8_t *emBody = &encrypted[emHeader_size + 16];
```

### Root Cause

Header sizes vary dramatically:
- v2: 123 bytes
- v3: 124 bytes
- v3+PQ: 2346 bytes (with SNTRUP761)

All offset calculations must be dynamic based on actual header content.

---

## Bug #30: HKs/NHKs Init + Promotion (SESSION 22)

**Session:** 22  
**Component:** `smp_ratchet.c` header key management  
**Impact:** Critical - Header key chain broken from init to promotion

### The Discovery

Three connected problems in header key handling:

**Problem 30a:** `next_header_key_send` was never stored in ratchet state (local variable only).

**Problem 30b:** `ratchet_x3dh_sender()` stored `nhk` (= rcvNextHK = NHKr) incorrectly
in `header_key_recv` instead of `next_header_key_recv`.

**Problem 30c:** After DH Ratchet Step, KDF output was set directly as HKs instead
of proper NHKs→HKs promotion.

### Incorrect Code
```c
// In ratchet_init_sender():
uint8_t next_header_key_send[32];  // Local variable, never saved!
// ...
// In ratchet_x3dh_sender():
memcpy(ratchet_state.header_key_recv, nhk, 32);  // WRONG! nhk is NHKr
// ...
// After DH Ratchet Step:
memcpy(ratchet_state.header_key_send, kdf_output + 64, 32);  // Direct, no promotion
```

### Correct Code
```c
// In ratchet_init_sender():
memcpy(ratchet_state.next_header_key_send, hkdf_output + 64, 32);  // SAVE to state!
// ...
// In ratchet_x3dh_sender():
memcpy(ratchet_state.next_header_key_recv, nhk, 32);  // NHKr, will promote to HKr
// ...
// After DH Ratchet Step - PROMOTION:
memcpy(ratchet_state.header_key_send, ratchet_state.next_header_key_send, 32);  // NHKs→HKs
memcpy(ratchet_state.next_header_key_send, kdf_output + 64, 32);  // New NHKs from KDF
```

### Root Cause

The 4 Header Key architecture requires:
- HKs/NHKs for sending (current/next)
- HKr/NHKr for receiving (current/next)

Promotion: `HKs ← NHKs` then `NHKs ← KDF output` (not direct assignment).
Initial: `nhk` from X3DH is NHKr, promotes to HKr on first AdvanceRatchet.

---

## Bug #31: Phase 2a Try-Order (SESSION 22)

**Session:** 22  
**Component:** `main.c` header decrypt try sequence  
**Impact:** Critical - AdvanceRatchet never triggered

### The Discovery

Header decrypt tried `next_header_key_recv` only via debug fallback (`saved_nhk`),
not as a regular try → AdvanceRatchet was never triggered → ratchet state stuck.

### Incorrect Code
```c
// Only tried HKr
if (try_header_decrypt(header_key_recv, ...)) {
    // SameRatchet
} else {
    // Debug fallback using saved_nhk (not proper flow)
    if (try_header_decrypt(saved_nhk, ...)) {
        // This worked but didn't trigger AdvanceRatchet!
    }
}
```

### Correct Code
```c
// Try HKr first (SameRatchet)
if (try_header_decrypt(header_key_recv, ...)) {
    decrypt_mode = SAME_RATCHET;
}
// Try NHKr second (AdvanceRatchet)
else if (try_header_decrypt(next_header_key_recv, ...)) {
    decrypt_mode = ADVANCE_RATCHET;
    // Promote: HKr ← NHKr
    memcpy(ratchet_state.header_key_recv, ratchet_state.next_header_key_recv, 32);
    // Trigger full DH ratchet step...
}
```

### Root Cause

Double Ratchet requires trying keys in order:
1. HKr (SameRatchet) — same DH key, just chain forward
2. NHKr (AdvanceRatchet) — new DH key, full ratchet step

If NHKr succeeds, it triggers AdvanceRatchet and promotes NHKr→HKr.

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
| Feb 5 | S18 | #18 ✅ SOLVED! One-line fix! |
| Feb 5 | S19 | #19 header_key_recv overwritten (workaround) |
| Feb 6 | S20 | #19 ✅ SOLVED! Root cause: debug self-decrypt |
| Feb 6-7 | S21 | #20-#26 HELLO format + v3 format (7 bugs!) |
| **Feb 7** | **S22** | **#27-#31 E2E version, KEM parser, NHK promotion (5 bugs!)** |
| **Feb 7-8** | **S23** | **🎉 ZERO new bugs — CONNECTED!** |

---

## Bug Categories

```
31 Bugs Total (31 FIXED):
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
- 1x Key management issue (#19 - debug self-decrypt side effects)
- 1x Message type indicator issue (#20 - PrivHeader for HELLO)
- 1x Version field issue (#21 - AgentVersion)
- 1x Hash encoding issue (#22 - prevMsgHash)
- 1x Encryption order issue (#23 - pad before encrypt)
- 1x Key selection issue (#24 - rcv_dh vs snd_dh)
- 1x Maybe field issue (#25 - PubHeader Nothing)
- 1x Format version issue (#26 - v2/v3 encodeLarge)
- 1x Version mismatch issue (#27 - E2E version_min vs RATCHET_VERSION)
- 1x Dynamic parsing issue (#28 - KEM parser for variable header sizes)
- 1x Pointer arithmetic issue (#29 - dynamic emHeader size calculation)
- 1x Key storage/promotion issue (#30 - HKs/NHKs init and promotion chain)
- 1x Try-order issue (#31 - header decrypt sequence for AdvanceRatchet)

🎉 Session 23: CONNECTED with ZERO new bugs! The crypto was already correct!
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
44. **unPad layer exists between crypto_box and ClientMessage** - [2B len][content][padding 0x23...] (Session 19)
45. **PrivHeader tags: 'K'=PHConfirmation, '_'=PHEmpty** - Check Protocol.hs for encoding! (Session 19)
46. **Maybe encoding is ASCII '0'/'1', NOT binary 0x00/0x01** - Check Encoding.hs! (Session 19)
47. **nhk (HKDF[32-63]) = header_key_recv** - Second block of X3DH HKDF output! (Session 19)
48. **AES-GCM uses 16-byte IV in SimpleX** - Not standard 12-byte! (Session 19)
49. **Save keys immediately after derivation** - Prevents overwrite bugs like #19! (Session 19)
50. **Always account for ALL wrapper layers when parsing** - 0x3a wasn't PrivHeader, it was unPad length! (Session 19)
51. **Analysis first, implementation second** - Don't code until you understand the wire format! (Session 19)
52. **Tests must NEVER modify production state** - Debug self-decrypt corrupted ratchet state! (Session 20)
53. **Understand roles: Initiator='I', Joiner='D'** - ConnInfo tags differ by role in handshake! (Session 20)
54. **Check for Zstd compression** - 'X'=0x58 marker, magic 28 b5 2f fd, '1'=compressed! (Session 20)
55. **DH Ratchet Step = TWO rootKdf calls** - recv chain + send chain, new keypair in between! (Session 20)
56. **iv1 = Body IV, iv2 = Header IV** - During decrypt, header IV comes from ehIV, not chainKdf! (Session 20)
57. **Body AAD = rcAD || emHeader (raw bytes)** - Use exact wire bytes, don't re-serialize! (Session 20)
58. **ESP32 = Accepting Party, App = Joining Party** - Roles determine key/queue usage! (Session 21)
59. **PrivHeader: HELLO=0x00, CONF='K'** - Regular messages have NO PrivHeader, not PHEmpty! (Session 21)
60. **AgentMessage uses agentVersion=1, not v2/v7** - Different from AgentConfirmation! (Session 21)
61. **prevMsgHash must be smpEncoded** - Word16 prefix even when empty: [0x00][0x00]! (Session 21)
62. **DH Keys: rcv_dh for Confirmation, snd_dh for HELLO** - Different keys for different msg types! (Session 21)
63. **PubHeader Nothing = '0' (0x30), not missing** - Standard Maybe encoding, must be present! (Session 21)
64. **NOT_AVAILABLE = AUTH error on App side** - App can't SEND because queue not secured! (Session 21)
65. **KEY Command timing: after Confirmation, before HELLO** - Authorize sender before they can send! (Session 21)
66. **Reply Queues are unsecured** - SEND works without KEY auth! (Session 21)
67. **chatItemNotFoundByContactId = RSYNC Crypto Error** - Not HELLO parsing, but decrypt failure! (Session 21)
68. **RSYNC = Ratchet Sync Event** - Triggered on decrypt failure, not protocol error! (Session 21)
69. **v2/v3 encodeLarge switch at v≥3** - 1-byte → 2-byte prefix, affects header sizes! (Session 21)
70. **Version from E2ERatchetParams, not hardcoded** - Confirmation determines peer's expected format! (Session 21)
71. **Confirmation can work v2, HELLO expected v3** - Version mismatch between message types! (Session 21)
72. **Modern SimpleX (v2 + senderCanSecure) needs NO HELLO** - Use Reply Queue flow instead! (Session 22)
73. **AgentConnInfo on Reply Queue, not HELLO on Contact Queue** - Different protocol flow! (Session 22)
74. **smpReplyQueues in Tag 'D' AgentConnInfoReply** - Innermost layer of AgentConfirmation! (Session 22)
75. **SNTRUP761 for PQ KEM, not Kyber1024** - 1158B pubkey, 1039B ciphertext, 32B secret! (Session 22)
76. **PQ-Graceful-Degradation: KEM Nothing → pure DH** - No error on fallback! (Session 22)
77. **E2E version_min MUST match RATCHET_VERSION** - Mismatch causes format confusion! (Session 22)
78. **KEM Parser must be dynamic** - v3+PQ headers up to 2346 bytes! (Session 22)
79. **emHeader size dynamic based on ehVersion** - Don't hardcode offset calculations! (Session 22)
80. **NHKs must be stored in state at init** - Local variable loses value! (Session 22)
81. **nhk from X3DH = NHKr, not HKr directly** - Promotes to HKr on first AdvanceRatchet! (Session 22)
82. **NHKs→HKs promotion THEN KDF→NHKs** - Two-step promotion, not direct assignment! (Session 22)
83. **Header decrypt try-order: HKr, then NHKr** - Wrong order prevents AdvanceRatchet! (Session 22)
84. **ESP32 = Bob (Accepting), App = Alice (Initiating)** - Clear role names! (Session 23)
85. **Tag 'D' sent BY US, Tag 'I' received FROM App** - We send Reply Queue info, App doesn't! (Session 23)
86. **Legacy Path (PHConfirmation 'K') requires KEY + HELLO** - Not Modern/senderCanSecure Path! (Session 23)
87. **KEY is a RECIPIENT command** - Signed with rcv_private_auth_key, authorizes the SENDER! (Session 23)
88. **TLS timeout during Confirmation processing** - Reply Queue connection drops, must reconnect! (Session 23)
89. **Sequence: KEY BEFORE HELLO** - Can't send HELLO before authorizing the sender! (Session 23)
90. **Reconnect sequence: TLS → SUB → KEY** - Must re-subscribe to queue after reconnect! (Session 23)
91. **Padding: 14832B for ConnInfo, 15840B for HELLO** - Different message types, different sizes! (Session 23)
92. **Session 22's "No HELLO" theory was WRONG** - Legacy Path still requires HELLO exchange! (Session 23)
93. **Assumptions must be verified with logs** - Tag 'D' branch never triggered = wrong assumption! (Session 23)
94. **Complete handshake is 7 steps** - Not 3, not 5, exactly 7 steps for Legacy Path! (Session 23)
95. **CONNECTED requires BOTH HELLOs** - We send on Q_A, App sends on Q_B, then CON! (Session 23)

---

## Session 23: CONNECTED with ZERO New Bugs! 🎉

Session 23 achieved the historic milestone of **CONNECTED** status without introducing 
any new bugs. All 31 existing bugs were already fixed, and the complete 7-step 
handshake was successfully implemented.

### The Complete 7-Step Handshake (Verified Working)

```
Step   Queue   Direction      Content                           
──────────────────────────────────────────────────────────────
1.     —       App            NEW → Q_A, creates Invitation      
2a.    Q_A     ESP32→App      SKEY (Register Sender Auth)        
2b.    Q_A     ESP32→App      CONF Tag 'D' (Q_B + Profile)       
3.     —       App            processConf → CONF Event           
4.     —       App            LET/Accept Confirmation            
5a.    Q_A     App            KEY on Q_A (senderKey)             
5b.    Q_B     App→ESP32      SKEY on Q_B                        
5c.    Q_B     App→ESP32      Tag 'I' (App Profile)              
6a.    Q_B     ESP32          Reconnect + SUB + KEY              
6b.    Q_A     ESP32→App      HELLO                              
6c.    Q_B     App→ESP32      HELLO                              
7.     —       Both           CON — "CONNECTED" 🎉               
```

### Key Corrections from Session 22

- **Session 22 assumed:** "Modern SimpleX needs no HELLO, App sends Reply Queue in Tag 'D'"
- **Session 23 discovered:** App sends Tag 'I' (no Queue info), WE send Tag 'D', Legacy Path needs HELLO

---

*Bug Tracker v18.0*  
*Last updated: February 8, 2026 - Session 23*  
*Total bugs documented: 31 (31 FIXED)*  
*95 lessons learned!*  
*🎉 CONNECTED with ZERO new bugs!*
