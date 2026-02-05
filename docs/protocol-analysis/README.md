# SimpleGo Protocol Analysis

## Complete Development Documentation

This directory contains the complete, unabridged documentation of SimpleGo's development journey - the **world's first native SMP protocol implementation** outside the official SimpleX Haskell codebase.

---

## Current Status (2026-02-04 Session 17)

```
SESSION 17 - KEY CONSISTENCY INVESTIGATION
==========================================

CRITICAL REALIZATION:
  Evgeny ALREADY ANSWERED our question (Jan 28, 2026)!
  - Key is in "confirmation header" (SPKI in message header)
  - "outside of AgentConnInfoReply but in the same message"
  - TWO crypto_box layers with different keys and nonces

NEW DISCOVERIES:
  - Reply Queue has 2-byte length prefix (Contact Queue doesn't)
  - cmNonce is RANDOM (directly in message, not calculated)
  - Both keypairs generated at queue creation, NEVER changed
  - Key mismatch observed in logs (under investigation)

STILL VERIFIED CORRECT:
  ✅ Wire-Format, AAD, Keys, Custom XSalsa20
  
INVESTIGATING:
  ❓ e2e_private key consistency between creation and decrypt
  ❓ sender_pub extraction from message

Bug #18: Key Consistency Investigation - Debug test pending
```

---

## Acknowledgments and Respect

**This project would not be possible without the incredible work of the SimpleX team.**

SimpleX Chat represents a groundbreaking achievement in privacy-preserving communication technology. The protocol design is elegant, well-thought-out, and prioritizes user privacy above all else. We have the deepest respect for:

- **Evgeny Poberezkin** and the entire SimpleX Chat team
- The brilliant cryptographic design combining X3DH, Double Ratchet, and post-quantum algorithms
- The commitment to open source (AGPL-3.0) that made this project possible
- The comprehensive Haskell implementation that served as our reference

**Links:**
- SimpleX Chat: https://simplex.chat
- SimpleX GitHub: https://github.com/simplex-chat

---

## Document Structure

| Document | Lines | Description |
|----------|-------|-------------|
| [01_SIMPLEX_PROTOCOL_INDEX.md](01_SIMPLEX_PROTOCOL_INDEX.md) | ~100 | Navigation index |
| [02_SIMPLEX_STATUS.md](02_SIMPLEX_STATUS.md) | ~250 | Current status summary |
| [03_PART1_INTRO_SESSIONS_1-2.md](03_PART1_INTRO_SESSIONS_1-2.md) | ~2300 | Foundation, TLS 1.3, basic SMP |
| [04_PART2_SESSIONS_3-4.md](04_PART2_SESSIONS_3-4.md) | ~1000 | Wire format, bugs #1-8 |
| [05_PART3_SESSIONS_5-6.md](05_PART3_SESSIONS_5-6.md) | ~800 | X448 breakthrough, SMPQueueInfo |
| [06_PART4_SESSION_7.md](06_PART4_SESSION_7.md) | ~3200 | AES-GCM verification, Tail encoding |
| [07_PART5_SESSION_8_BREAKTHROUGH.md](07_PART5_SESSION_8_BREAKTHROUGH.md) | ~400 | AgentConfirmation works! |
| [08_PART6_SESSION_9.md](08_PART6_SESSION_9.md) | ~450 | Reply Queue HSalsa20 fix |
| [09_PART7_SESSION_10.md](09_PART7_SESSION_10.md) | ~400 | cmNonce fix, app "connecting" |
| [10_PART8_SESSION_11.md](10_PART8_SESSION_11.md) | ~400 | Regression & Recovery |
| [11_PART9_SESSION_12.md](11_PART9_SESSION_12.md) | ~400 | E2E Keypair Fix Attempt |
| [12_PART10_SESSION_13.md](12_PART10_SESSION_13.md) | ~700 | E2E Crypto Deep Analysis |
| [13_PART11_SESSION_14.md](13_PART11_SESSION_14.md) | ~900 | DH SECRET VERIFIED! |
| [14_PART12_SESSION_15.md](14_PART12_SESSION_15.md) | ~650 | Root Cause Found |
| [15_PART13_SESSION_16.md](15_PART13_SESSION_16.md) | ~900 | Custom XSalsa20 + Double Ratchet |
| [16_PART14_SESSION_17.md](16_PART14_SESSION_17.md) | ~500 | **Key Consistency Debug** |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~1050 | Complete bug documentation (18 bugs) |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~500 | Constants, wire formats, verified values |

**Total: ~21,000+ lines of detailed protocol analysis**

---

## Project Timeline

| Session | Date | Milestone | Bugs Fixed |
|---------|------|-----------|------------|
| 1-3 | Dec 2025 | Foundation, TLS 1.3, Basic SMP | - |
| 4 | Jan 23, 2026 | Wire format analysis | #1-8 |
| 5 | Jan 24, 2026 | X448 byte-order breakthrough | #9 |
| 6 | Jan 24, 2026 | SMPQueueInfo encoding | #10-12 |
| 7 | Jan 24-25, 2026 | Crypto verification, SimpleX contact | - |
| 8 | Jan 27, 2026 | AgentConfirmation WORKS! | #13-14 |
| 9 | Jan 27, 2026 | Reply Queue HSalsa20 fix | #15-16 |
| 10C | Jan 28, 2026 | cmNonce fix, app "connecting" | #17 |
| 11 | Jan 30, 2026 | Regression & Recovery | - |
| 12 | Jan 30, 2026 | E2E Keypair Analysis | - |
| 13 | Jan 30, 2026 | E2E Crypto Deep Analysis | - |
| 14 | Jan 31 - Feb 1 | DH SECRET VERIFIED! | #18 (partial) |
| 15 | Feb 1 | Root Cause Found | #18 (root cause) |
| 16 | Feb 1-3 | Double Ratchet Investigation | #18 (narrowed) |
| **17** | **Feb 4** | **Key Consistency Debug** | **#18 (investigating)** |

---

## Session 17 Key Achievements

### 1. Evgeny Already Answered!

We asked a question he had already answered on Jan 28, 2026:
- Key is in **confirmation header** (SPKI in message header)
- "outside of AgentConnInfoReply but in the **same message**"
- TWO crypto_box layers with different keys and nonces

**Rule added: ALWAYS search past Evgeny conversations first!**

### 2. New Discoveries

| Discovery | Detail |
|-----------|--------|
| Length prefix | Reply Queue has 2-byte prefix, Contact Queue doesn't |
| cmNonce | Is RANDOM (in message), not calculated |
| Keypairs | Both generated at queue creation, NEVER changed |
| Key mismatch | Different e2e_private in logs (under investigation) |

---

## Session 16 Key Achievements

### 1. Session 15 Theory DISPROVEN

Evgeny confirmed: **"in the same message"**
- NO second message needed!
- Key IS in the message header
- Session 15 was chasing a non-existent problem

### 2. SimpleX NON-STANDARD XSalsa20 Discovered

```
Standard libsodium:  HSalsa20(key, nonce[0:16])
SimpleX:             HSalsa20(key, zeros[16])  <- ZEROS!

Subkeys are COMPLETELY DIFFERENT!
All previous crypto attempts were DOOMED!
```

### 3. Custom XSalsa20 Implemented and VERIFIED

```c
// simplex_crypto.c - Works!
simplex_secretbox_open() - Round-trip SUCCESS ✅
```

### 4. Problem Narrowed to Double Ratchet

| Component | Status |
|-----------|--------|
| Wire-Format | ✅ CORRECT |
| AAD | ✅ CORRECT |
| Keys | ✅ CORRECT |
| Custom XSalsa20 | ✅ VERIFIED |
| **Double Ratchet** | ❌ **BROKEN** |

Suspects: rcAD order, X3DH DH order, HKDF params

---

## Session 15 Key Achievements

### 1. ROOT CAUSE Identified!

```
maybe_e2e = ',' (Nothing) in message header
  -> No ephemeral e2ePubKey in message
  -> Uses pre-computed e2eDhSecret
  -> Need app.sndQueue.e2ePubKey to calculate it
  -> Key is in App's AgentConfirmation
  -> We don't receive that message!
```

### 2. Protocol Flow Analyzed

```
✅ Step 1: INVITATION received
✅ Step 2: AgentConfirmation sent -> OK
✅ Step 3: HELLO sent -> OK
❌ Step 4: App's AgentConfirmation NOT received!
❌ Step 5: Cannot decrypt Reply Queue (missing key)
```

### 3. All Available Keys Tested

| Key Source | Result |
|------------|--------|
| URL dh= key | FAILED |
| Message corrId | FAILED |
| All offsets 48-80 | FAILED |
| X25519 search | 0 found |

**Conclusion:** The needed key is NOT in data we have.

---

## Session 14 Key Achievements

### 1. DH Secret VERIFIED with Python!

```python
from nacl.bindings import crypto_scalarmult

our_private = bytes.fromhex('83473153de033039...')
peer_public = bytes.fromhex('9140e10e9fdee92e...')

dh_secret = crypto_scalarmult(our_private, peer_public)
# Result: d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810
# MATCHES ESP32!
```

### 2. Bugs Fixed

| Bug | Problem | Fix |
|-----|---------|-----|
| Wrong Key | Used SMP DH key from INVITATION | Extract e2ePubKey from message header [28-59] |
| Wrong DH Function | crypto_box_beforenm (adds HSalsa20) | crypto_scalarmult (raw DH) |

### 3. Handoff Theory DISPROVEN

| Statement | Handoff Document | Reality (Source Code) |
|-----------|------------------|----------------------|
| 2 MSGs on Contact Queue | Claimed | FALSE |
| HELLO on Reply Queue | Not mentioned | TRUE (confirmed) |
| E2E Key in PHConfirmation | Claimed | FALSE |

### 4. Correct Message Flow Documented

```
Contact Queue: 1 message (INVITATION)
Reply Queue: 1 message (HELLO)
NO second message on Contact Queue!
```

---

## Quick Navigation

### By Bug Number

| Bug | Description | Document |
|-----|-------------|----------|
| #1-8 | Wire format bugs | [Part 2](04_PART2_SESSIONS_3-4.md) |
| #9 | wolfSSL X448 byte-order | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #10-12 | SMPQueueInfo encoding | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #13-14 | AAD prefix, IV order | [Part 5](07_PART5_SESSION_8_BREAKTHROUGH.md) |
| #15-16 | HSalsa20, A_CRYPTO | [Part 6](08_PART6_SESSION_9.md) |
| #17 | cmNonce instead of msgId | [Part 7](09_PART7_SESSION_10.md) |
| **#18** | **Reply Queue E2E** | [**Part 14**](16_PART14_SESSION_17.md) |

---

## License

This documentation is part of SimpleGo, licensed under AGPL-3.0.

---

*Last updated: February 4, 2026 - Session 17 (Key Consistency Debug)*
