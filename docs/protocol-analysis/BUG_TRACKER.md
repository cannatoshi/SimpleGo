# Bug Tracker

## Complete Documentation of All 18 Bugs

This document provides detailed documentation of all bugs discovered during SimpleGo development, including the incorrect code, correct code, and root cause analysis.

---

## Summary

| Bug # | Component | Session | Status |
|-------|-----------|---------|--------|
| 1-8 | Wire format | 4 | FIXED |
| 9 | wolfSSL X448 byte order | 5 | FIXED |
| 10-12 | SMPQueueInfo encoding | 6 | FIXED |
| 13-14 | Payload AAD, IV order | 8 | FIXED |
| 15-16 | HSalsa20, A_CRYPTO | 9 | FIXED |
| 17 | cmNonce instead of msgId | 10C | FIXED |
| **18** | **Reply Queue E2E** | **12** | **OPEN** |

---

## Bug #18: Reply Queue E2E Decryption (OPEN)

**Session:** 12  
**Component:** Reply Queue Per-Queue E2E Layer  
**Impact:** Cannot decrypt Reply Queue messages from app  
**Status:** OPEN - Root cause identified, solution unclear

### 18.1 The Discovery

Haskell uses **TWO separate X25519 keypairs**:

| Keypair | Purpose | Used in |
|---------|---------|---------|
| dhKey / privDhKey | Server-level DH (NEW command) | rcvDhSecret |
| e2eDhKey / e2ePrivKey | E2E-level DH (Peer encryption) | SMPQueueAddress |

### 18.2 Changes Implemented

**smp_queue.h - Added E2E keys:**
```c
typedef struct {
    uint8_t rcv_dh_public[32];    // Server DH
    uint8_t rcv_dh_private[32];
    
    // E2E keys (separate from server DH!)
    uint8_t e2e_public[32];       // NEW!
    uint8_t e2e_private[32];      // NEW!
    
    uint8_t shared_secret[32];
    // ...
} our_queue_t;
```

**smp_queue.c - Generate E2E keypair:**
```c
crypto_box_keypair(our_queue.rcv_dh_public, our_queue.rcv_dh_private);
crypto_box_keypair(our_queue.e2e_public, our_queue.e2e_private);  // NEW!
```

**smp_queue.c - Send e2e_public in SMPQueueInfo:**
```c
// Changed from rcv_dh_public to e2e_public
memcpy(&buf[p], our_queue.e2e_public, 32);
```

### 18.3 The Problem Found

Message structure analysis:
```
[14]    maybe_corrId = '1' (0x31) - Just (has corrId)
[15]    maybe_e2e = ',' (0x2c) - Nothing! <- PROBLEM!
```

**phE2ePubDhKey = Nothing** - App sends NO E2E public key!

### 18.4 Why App Doesn't Send E2E Key

From Haskell `newSndQueue`:
```haskell
newSndQueue userId connId (SMPQueueInfo ... dhPublicKey = rcvE2ePubDhKey) = do
  (e2ePubKey, e2ePrivKey) <- generateKeyPair
  e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey  -- Pre-computed!
```

The app:
1. Receives our `e2e_public` from SMPQueueInfo
2. Generates its own E2E keypair
3. Pre-computes `e2eDhSecret = DH(our_pub, app_priv)`
4. **Never sends its e2ePubKey to us!**

### 18.5 The Dilemma

| Side | Has | Needs |
|------|-----|-------|
| App | our_e2e_public, app_e2e_private | Can compute DH |
| Us | our_e2e_private, ??? | Need app_e2e_public! |

### 18.6 Sub-Issues

| Sub-Issue | Description | Status |
|-----------|-------------|--------|
| #18a | Separate E2E Keypair implemented | DONE |
| #18b | E2E public sent in SMPQueueInfo | DONE |
| #18c | App sends phE2ePubDhKey = Nothing | DISCOVERED |
| #18d | Where does app_e2e_public come from? | UNKNOWN |

### 18.7 Hypotheses

1. **Protocol Version:** Different E2E behavior in newer versions
2. **Queue Mode:** QMMessaging vs QMContact might differ
3. **X3DH Derived:** E2E key from X3DH key agreement
4. **In AgentConfirmation:** Key might be in app's confirmation

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
| **Jan 30, 2026** | **S12** | **#18 (deep analysis)** |

---

## Bug Categories

```
18 Bugs Total:
- 7x Length Prefix issues
- 3x KDF/IV Order issues
- 1x Byte Order issue (wolfSSL)
- 1x Separator issue
- 1x Maybe encoding issue
- 1x AAD construction issue
- 1x NaCl crypto layer issue
- 1x Header encryption issue
- 1x Nonce source issue (cmNonce)
- 1x E2E keypair exchange issue (OPEN)
```

---

## Lessons Learned

1. **Length encoding varies by context** - always check Haskell source
2. **Crypto libraries differ** - verify against reference implementations
3. **If it works, don't touch it!** - Session 11 regression
4. **cmNonce != msgId** - Different nonces for different layers
5. **Two keypairs exist** - Server DH vs E2E DH are separate!
6. **Pre-computed secrets** - App may pre-compute and not send keys

---

*Bug Tracker v6.0*  
*Last updated: January 30, 2026 - Session 12*  
*Total bugs documented: 18 (17 fixed, 1 open)*
