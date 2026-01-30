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
| **17** | **cmNonce instead of msgId** | **10C** | **FIXED** |
| **18** | **Reply Queue Double Ratchet** | **11** | **OPEN** |

---

## Bug #1-#16: See Previous Documentation

Bugs #1-16 are documented in detail in earlier sessions. Summary:
- #1-8: Wire format length prefix issues
- #9: wolfSSL X448 byte order reversal
- #10-12: SMPQueueInfo encoding
- #13-14: Payload AAD and chainKdf IV order (Session 8 Breakthrough)
- #15-16: HSalsa20 key derivation and A_CRYPTO header (Session 9)

---

## Bug #17: cmNonce instead of msgId (FIXED)

**Session:** 10C  
**Component:** Per-Queue E2E Decryption  
**Impact:** Critical - All Reply Queue messages fail decryption  
**Status:** FIXED

### The Problem
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

## Bug #18: Reply Queue Double Ratchet (OPEN)

**Session:** 11  
**Component:** Double Ratchet Receiver Side  
**Impact:** Cannot decrypt Reply Queue messages from app  
**Status:** OPEN

### The Problem
Reply Queue messages have `Maybe = ','` (Nothing), meaning no e2ePubKey layer.
These messages go directly into Double Ratchet, requiring receiver-side implementation.

### Observation
```
Maybe tag = ',' (Nothing)
```

### Difference: Contact Queue vs Reply Queue

| Queue | Maybe Tag | Meaning |
|-------|-----------|---------|
| Contact Queue | '1' (Just) | Has e2ePubKey, per-queue E2E layer |
| Reply Queue | ',' (Nothing) | No e2ePubKey, direct Double Ratchet |

### Required Implementation

1. **Header Decryption** with `header_key_recv`
2. **Message Decryption** with derived message key from chain KDF
3. **Ratchet State Update** for next message

### Current Status
Contact Queue decrypt works (TEST4 SUCCESS).
Reply Queue decrypt pending - needs Double Ratchet receiver implementation.

---

## Session 11: Format Experiments (ALL REVERTED)

Session 11 documented several **incorrect** format experiments that caused regression:

| Experiment | Change | Result |
|------------|--------|--------|
| RATCHET_VERSION | 2 -> 3 | REGRESSION |
| em_header size | 123 -> 124 | REGRESSION |
| Maybe tag | '1' -> 0x01 | REGRESSION |
| Version encoding | 00 02 -> 02 02 | REGRESSION |
| DH order swap | Swapped | REGRESSION |

**All experiments were reverted via `git checkout -- main/`**

The only correct change was re-applying the cmNonce fix (Bug #17).

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
| **Jan 30, 2026** | **S11** | **#18** |

---

## Bug Categories

```
18 Bugs Total:
- 7x Length Prefix issues
- 3x KDF/IV Order issues
- 1x Byte Order issue (wolfSSL)
- 1x Separator issue (Space vs Length)
- 1x Maybe encoding issue
- 1x AAD construction issue
- 1x NaCl crypto layer issue (HSalsa20)
- 1x Header encryption issue
- 1x Nonce source issue (cmNonce)
- 1x Double Ratchet receiver issue (OPEN)
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
13. **If it works, don't touch it!** - Format experiments caused regression
14. **Git is your friend** - Commit at working state, reset when needed

---

*Bug Tracker v5.0*  
*Last updated: January 30, 2026 - Session 11*  
*Total bugs documented: 18 (17 fixed, 1 open)*
