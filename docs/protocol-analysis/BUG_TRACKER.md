# Bug Tracker

## Complete Documentation of All 19 Bugs

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

**Total: 19 bugs documented, 19 FIXED**

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

### 19.2 Discovery (Session 19)

During Session 19 Double Ratchet header decrypt implementation, we discovered that
`header_key_recv` (the key used to decrypt incoming message headers) is being
overwritten somewhere between X3DH initialization and message receipt.

### 19.3 Workaround (Session 19)

Saving `nhk` immediately after X3DH HKDF calculation as `saved_nhk`:
```c
// After X3DH HKDF:
memcpy(saved_nhk, &x3dh_output[32], 32);  // nhk = HKDF output bytes 32-63

// At header decrypt (instead of header_key_recv):
aes_gcm_decrypt(ehBody, saved_nhk, ehIV, rcAD, ...);  // SUCCESS!
```

### 19.4 Root Cause — FOUND (Session 20)

**`smp_peer.c:347`** — Debug self-decrypt test calling `ratchet_decrypt()`.

After encrypting the AgentConfirmation, a debug self-test called `ratchet_decrypt()`
on our own encrypted message. `ratchet_decrypt()` has **side effects**: it performs
a DH ratchet step when it detects a "new" DH key in the decrypted header.

When decrypting our **own** message, the DH key in the header is `dh_self.public_key`
(our key), which differs from `dh_peer` (the peer's key). So `dh_changed = true`
and the function overwrites:
- `ratchet_state.root_key` → corrupted
- `ratchet_state.chain_key_recv` → corrupted
- **`ratchet_state.header_key_recv`** → changed from `1c08e86e...` to `cf0c74d2...`
- `ratchet_state.dh_peer` → corrupted (set to our own key)
- `ratchet_state.msg_num_recv` → reset to 0

### 19.5 Fix Applied (Session 20)

Removed the debug self-decrypt test from `smp_peer.c:343-359`. The `saved_nhk`
workaround in `smp_ratchet.c` is no longer needed but kept as safety net.

Branch: `claude/fix-header-key-recv-bug-DNYeF` → merged to main.

### 19.6 Call Flow (for reference)

```
send_agent_confirmation():
  [309] ratchet_x3dh_sender()    → header_key_recv = 1c08e86e... ✅
  [317] ratchet_init_sender()    → no change to header_key_recv ✅
  [335] ratchet_encrypt()        → msg #0, no change to recv keys ✅
  [347] ratchet_decrypt() DEBUG  → header_key_recv = cf0c74d2... ❌ BUG!
  [689] complete_handshake()     → ratchet_encrypt() msg #1 (HELLO)
  ... later: ratchet_decrypt() on incoming msg → fails with wrong key
```

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
| **Feb 6** | **S20** | **#19 ✅ SOLVED! Root cause: debug self-decrypt** |

---

## Bug Categories

```
19 Bugs Total (19 FIXED):
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

---

*Bug Tracker v15.0*  
*Last updated: February 6, 2026 - Session 20*  
*Total bugs documented: 19 (19 FIXED)*  
*57 lessons learned!*
