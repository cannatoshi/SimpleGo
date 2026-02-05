# SimpleGo Protocol Analysis

## Complete Development Documentation

This directory contains the complete, unabridged documentation of SimpleGo's development journey - the **world's first native SMP protocol implementation** outside the official SimpleX Haskell codebase.

---

## Current Status (2026-02-05 Session 18)

```
SESSION 18 - 🎉 BUG #18 SOLVED! E2E LAYER 2 DECRYPT SUCCESS!
================================================================

BREAKTHROUGH:
  Root Cause: envelope_len included 102 bytes SMP block-padding
  Fix: ONE LINE — envelope_len = raw_len_prefix
  Result: Method 0 (decrypt_client_msg) SUCCESS! 15904 bytes!
  Content: AgentConfirmation + EncRatchetMessage

BUG #18 TIMELINE:
  7 sessions (S12-S18), ~30 sub-issues, weeks of debugging
  Final fix: ONE LINE OF CODE

DECRYPTED CONTENT:
  [0]     = PrivHeader ':' (0x3a) — new type, must be identified
  [2-14]  = Ed25519 SPKI (OID 1.3.101.112)
  [...]   = Agent Version 7, Tag 'C' = AgentConfirmation
  [...]   = EncRatchetMessage — Double Ratchet payload

ALL LAYERS THROUGH LAYER 2:
  ✅ Layer 0: TLS 1.3
  ✅ Layer 1: SMP Transport (Server→Recipient)
  ✅ Layer 2: E2E (Sender→Recipient) — FIXED SESSION 18!
  ⏳ Layer 3: AgentMsgEnvelope parsing
  ⏳ Layer 4: Double Ratchet
  ⏳ Layer 5: Application Data

NEXT: Parse AgentConfirmation, decrypt EncRatchetMessage
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
| [16_PART14_SESSION_17.md](16_PART14_SESSION_17.md) | ~500 | Key Consistency Debug |
| [17_PART15_SESSION_18.md](17_PART15_SESSION_18.md) | ~600 | **🎉 BUG #18 SOLVED! E2E Decrypt SUCCESS** |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~1200 | Complete bug documentation (18 bugs) |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~600 | Constants, wire formats, verified values |

**Total: ~22,000+ lines of detailed protocol analysis**

---

## Project Timeline

| Session | Date | Milestone | Bugs Fixed |
|---------|------|-----------|------------|
| 1-3 | Dec 2025 | Foundation, TLS 1.3, Basic SMP | - |
| 4 | Jan 23, 2026 | Wire format analysis | #1-8 |
| 5 | Jan 24, 2026 | X448 byte-order breakthrough | #9 |
| 6 | Jan 24, 2026 | SMPQueueInfo encoding | #10-12 |
| 7 | Jan 24-25, 2026 | AES-GCM verification, SimpleX contact | - |
| 8 | Jan 27, 2026 | AgentConfirmation WORKS! | #13-14 |
| 9 | Jan 27, 2026 | Reply Queue HSalsa20 fix | #15-16 |
| 10C | Jan 28, 2026 | cmNonce fix, app "connecting" | #17 |
| 11 | Jan 30, 2026 | Regression & Recovery | - |
| 12 | Jan 30, 2026 | E2E Keypair Analysis | - |
| 13 | Jan 30, 2026 | E2E Crypto Deep Analysis | - |
| 14 | Jan 31 - Feb 1 | DH SECRET VERIFIED! | #18 (partial) |
| 15 | Feb 1 | Root Cause Found (later disproven) | #18 (root cause) |
| 16 | Feb 1-3 | Custom XSalsa20 + Double Ratchet | #18 (narrowed) |
| 17 | Feb 4 | Key Consistency Debug | #18 (investigating) |
| **18** | **Feb 5** | **🎉 BUG #18 SOLVED! E2E Decrypt SUCCESS!** | **#18 ✅ SOLVED** |

---

## Session 18 Key Achievements

### 1. 🎉 BUG #18 SOLVED After Weeks of Debugging!

**Root Cause:** `envelope_len = plain_len - 2` included 102 bytes SMP block-padding  
**Fix:** `envelope_len = raw_len_prefix` — ONE LINE!  
**Result:** Method 0 decrypt SUCCESS → 15904 bytes AgentConfirmation

### 2. Wire-Format Fully Analyzed

| Discovery | Detail |
|-----------|--------|
| corrId NOT in envelope | corrId is SMP Transport Layer, parsed before envelope |
| All offsets were WRONG | Code assumed header+corrId+maybe, reality is version+maybe+body |
| No comma separators | `smpEncode a <> smpEncode b` — direct concatenation |
| Wrapper chain | EncRcvMsgBody → ClientRcvMsgBody → ClientMsgEnvelope |
| 102 bytes padding | SMP block-padding (0x23) for traffic analysis resistance |

### 3. Architecture Clarified

| Queue | Layer 1 (Server) | Layer 2 (E2E) |
|-------|-------------------|---------------|
| Contact Queue | ✅ decrypt_smp_message | ❌ NO E2E Layer! |
| Reply Queue | ✅ decrypt_smp_message | ✅ **FIXED Session 18!** |

### 4. Decrypted Content Preview

```
PrivHeader ':' (0x3a) — new type to identify
Ed25519 SPKI — OID 1.3.101.112  
Agent Version 7, Tag 'C' — AgentConfirmation
EncRatchetMessage — Double Ratchet payload (next step)
```

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
| Key mismatch | Different e2e_private in logs (resolved in S18: different test runs) |

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
| **#18** | **Reply Queue E2E — SOLVED!** | [**Part 14**](16_PART14_SESSION_17.md), [**Part 15**](17_PART15_SESSION_18.md) |

---

## License

This documentation is part of SimpleGo, licensed under AGPL-3.0.

---

*Last updated: February 5, 2026 - Session 18 (🎉 BUG #18 SOLVED! E2E Decrypt SUCCESS!)*
