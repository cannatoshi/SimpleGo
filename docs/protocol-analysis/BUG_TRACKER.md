# Bug Tracker

## Complete Documentation of All 17 Bugs

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
| 16 | A_CRYPTO in App | 9 | FIXED |
| **17** | **Reply Queue Per-Queue DH** | **10C** | **ACTIVE** |

---

## Bug #1-#14: See Previous Documentation

Bugs #1-14 are documented in detail in earlier sessions. Summary:
- #1-8: Wire format length prefix issues
- #9: wolfSSL X448 byte order reversal
- #10-12: SMPQueueInfo encoding
- #13-14: Payload AAD and chainKdf IV order (Session 8 Breakthrough)

---

## Bug #15: Reply Queue HSalsa20 Key Derivation

**Session:** 9  
**Component:** Reply Queue shared secret computation  
**Impact:** Critical - Poly1305 tag verification always fails  
**Status:** FIXED

### The Problem
Reply Queue decryption used raw X25519 output instead of NaCl-derived key.

### Incorrect Code
```c
// Uses crypto_scalarmult - ONLY raw X25519, NO HSalsa20!
crypto_scalarmult(our_queue.shared_secret,
                  our_queue.rcv_dh_private,
                  our_queue.srv_dh_public);
```

### Correct Code
```c
// Use crypto_box_beforenm for X25519 + HSalsa20 key derivation!
crypto_box_beforenm(our_queue.shared_secret,
                    our_queue.srv_dh_public,
                    our_queue.rcv_dh_private);
```

### Root Cause
NaCl crypto_box includes HSalsa20 derivation layer. `crypto_scalarmult` only does raw X25519.

---

## Bug #16: A_CRYPTO Error in App

**Session:** 9  
**Component:** Double Ratchet header encryption  
**Impact:** Critical - App cannot decrypt AgentConfirmation  
**Status:** FIXED

### The Problem
Header encryption AAD format issue causing decryption failure.

### Resolution
Fixed through proper AAD construction matching Haskell reference implementation.

---

## Bug #17: Reply Queue Per-Queue DH (ACTIVE)

**Session:** 10C  
**Component:** Reply Queue second encryption layer  
**Impact:** Critical - Cannot decrypt peer messages on Reply Queue  
**Status:** ACTIVE - All key combinations fail

### The Problem
After server-level decrypt (Layer 1) succeeds, the per-queue DH decrypt (Layer 2) fails with ALL tested key combinations.

### Structure After Server-Level Decrypt
```
Offset  Bytes           Meaning
0-1     3e 82           Length prefix: 16002
2-5     00 00 00 00     Unknown (Padding?)
6-9     69 7a 0c 8d     Timestamp
10-13   54 20 00 04     Unknown
14-15   31 2c           Version "1," (ASCII)
16-59   30 2a 30 05...  X25519 SPKI (44 bytes)
60+     be 27 85 4d...  Ciphertext?
```

### Tested Key Combinations (ALL FAILED)

```c
// TEST 1: peer_ephemeral + rcv_dh_private + msgId nonce
crypto_box_beforenm(test_dh, peer_pub, our_queue.rcv_dh_private);
crypto_box_open_easy_afternm(test_plain, &server_plain[data_offset], 
                              data_len, msg_nonce, test_dh);
// RESULT: FAILED

// TEST 2: srv_dh_public + rcv_dh_private + msgId nonce  
crypto_box_beforenm(srv_dh, our_queue.srv_dh_public, our_queue.rcv_dh_private);
crypto_box_open_easy_afternm(test_plain, &server_plain[data_offset],
                              data_len, msg_nonce, srv_dh);
// RESULT: FAILED

// TEST 3: shared_secret (direct) + message nonce
// RESULT: FAILED

// TEST 4: Direct on raw data (without server decrypt)
// RESULT: FAILED
```

### Current Hypotheses

1. **No Per-Queue Layer:** Maybe Reply Queue has NO second crypto_box layer - Double Ratchet starts directly after SPKI?
2. **Different Key Source:** Key might come from a different source than assumed
3. **Different Nonce:** Nonce might be extracted differently

### Problem with No-Layer Hypothesis
Double Ratchet Decrypt also fails:
```
Decrypting incoming message...
   emHeader length: 189
Invalid emHeader length: 189
```
emHeader length 189 is unrealistic - should be ~123-127.

### Questions for Developers
```
For Reply Queue decryption:

After server-level decrypt (using shared_secret derived from 
srv_dh_public + rcv_dh_private, with msgId as nonce), I see 
an X25519 SPKI at offset 16 in the decrypted data.

When I try to decrypt the data after SPKI using 
`crypto_box_beforenm(peer_ephemeral_pub, our_rcv_dh_private)`, 
it fails.

Questions:
1. Is there a second per-queue crypto_box layer for Reply Queue?
2. If yes, which key should be used?
3. If no, does Double Ratchet data start directly after SPKI?
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
| **Jan 28, 2026** | **S10C** | **#17** |

---

## Bug Categories

```
17 Bugs Total:
- 7x Length Prefix issues
- 3x KDF/IV Order issues
- 1x Byte Order issue (wolfSSL)
- 1x Separator issue (Space vs Length)
- 1x Maybe encoding issue
- 1x AAD construction issue
- 1x NaCl crypto layer issue (HSalsa20)
- 1x Header encryption issue
- 1x Per-Queue DH key issue (ACTIVE)
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
12. **Test ALL key combinations** - systematic testing reveals unexpected behaviors

---

*Bug Tracker v4.0*  
*Last updated: January 28, 2026 - Session 10C*  
*Total bugs documented: 17 (16 fixed, 1 active)*
