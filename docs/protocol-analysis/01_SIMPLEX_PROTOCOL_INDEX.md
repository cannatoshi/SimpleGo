# SimpleX Protocol Analysis - Documentation Index

**Project:** SimpleGo - Native ESP32 SMP Implementation  
**Version:** v0.1.17-alpha  
**Last Updated:** 2026-02-07 (Session 22)

---

## 🎯 CURRENT STATUS: Reply Queue Flow Discovered!

On February 7, 2026, Session 22 discovered the **fundamental protocol insight**: Modern SimpleX (v2 + `senderCanSecure`) does NOT need HELLO! The App expects AgentConnInfo on the Reply Queue instead.

**Complete receive chain working:** TLS → SMP → E2E → Ratchet → Zstd → JSON Profile ✅  
**Missing:** Reply Queue flow for "Connected" status

---

## Documentation Structure

The complete protocol analysis (~30,000+ lines, 390+ sections) is split into 19 parts:

| Part | File | Lines | Content |
|------|------|-------|---------|
| 1 | [03_PART1_INTRO_SESSIONS_1-2.md](03_PART1_INTRO_SESSIONS_1-2.md) | ~2,300 | Introduction, Foundation, TLS 1.3 |
| 2 | [04_PART2_SESSIONS_3-4.md](04_PART2_SESSIONS_3-4.md) | ~1,000 | Wire format, Bugs #1-8 |
| 3 | [05_PART3_SESSIONS_5-6.md](05_PART3_SESSIONS_5-6.md) | ~800 | X448 breakthrough, SMPQueueInfo |
| 4 | [06_PART4_SESSION_7.md](06_PART4_SESSION_7.md) | ~3,200 | AES-GCM verification, Tail encoding |
| 5 | [07_PART5_SESSION_8_BREAKTHROUGH.md](07_PART5_SESSION_8_BREAKTHROUGH.md) | ~400 | 🎉 AgentConfirmation works! |
| 6 | [08_PART6_SESSION_9.md](08_PART6_SESSION_9.md) | ~450 | Reply Queue HSalsa20 fix |
| 7 | [09_PART7_SESSION_10.md](09_PART7_SESSION_10.md) | ~400 | cmNonce fix, app "connecting" |
| 8 | [10_PART8_SESSION_11.md](10_PART8_SESSION_11.md) | ~400 | Regression & Recovery |
| 9 | [11_PART9_SESSION_12.md](11_PART9_SESSION_12.md) | ~400 | E2E Keypair Fix Attempt |
| 10 | [12_PART10_SESSION_13.md](12_PART10_SESSION_13.md) | ~700 | E2E Crypto Deep Analysis |
| 11 | [13_PART11_SESSION_14.md](13_PART11_SESSION_14.md) | ~900 | DH SECRET VERIFIED! |
| 12 | [14_PART12_SESSION_15.md](14_PART12_SESSION_15.md) | ~650 | Root Cause Found |
| 13 | [15_PART13_SESSION_16.md](15_PART13_SESSION_16.md) | ~900 | Custom XSalsa20 + Double Ratchet |
| 14 | [16_PART14_SESSION_17.md](16_PART14_SESSION_17.md) | ~500 | Key Consistency Debug |
| 15 | [17_PART15_SESSION_18.md](17_PART15_SESSION_18.md) | ~600 | 🎉 BUG #18 SOLVED! E2E SUCCESS |
| 16 | [18_PART16_SESSION_19.md](18_PART16_SESSION_19.md) | ~550 | Header Decrypt SUCCESS! |
| 17 | [19_PART17_SESSION_20.md](19_PART17_SESSION_20.md) | ~600 | 🎉 Body Decrypt! Peer Profile! |
| 18 | [20_PART18_SESSION_21.md](20_PART18_SESSION_21.md) | ~700 | v3 Format + HELLO Debugging |
| **19** | [**21_PART19_SESSION_22.md**](21_PART19_SESSION_22.md) | **~650** | **🎯 Reply Queue Flow Discovery** |
| **Total** | | **~30,000+** | **390+ Sections** |

---

## Quick Reference Documents

| Document | Lines | Description |
|----------|-------|-------------|
| [README.md](README.md) | ~500 | Project overview and navigation |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~1,800 | All 31 bugs documented, 83 lessons |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~1,000 | Constants, wire formats, verified values |

---

## Session Overview

| Session | Date | Focus | Result |
|---------|------|-------|--------|
| 1-3 | Dec 2025 | Foundation | TLS 1.3, Basic SMP |
| 4 | Jan 23, 2026 | Wire Format | Bugs #1-8 fixed |
| 5 | Jan 24, 2026 | Crypto | X448 byte-order (Bug #9) |
| 6 | Jan 24, 2026 | Handshake | SMPQueueInfo (Bugs #10-12) |
| 7 | Jan 24-25, 2026 | Research | First native SMP confirmed! |
| 8 | Jan 27, 2026 | **BREAKTHROUGH** | 🎉 AgentConfirmation works! (Bugs #13-14) |
| 9 | Jan 27, 2026 | Reply Queue | HSalsa20 fix (Bugs #15-16) |
| 10C | Jan 28, 2026 | E2E Layer | cmNonce fix (Bug #17) |
| 11 | Jan 30, 2026 | Recovery | Regression fixed |
| 12-13 | Jan 30, 2026 | Analysis | E2E Crypto Deep Dive |
| 14 | Jan 31 - Feb 1 | Verification | DH SECRET VERIFIED! |
| 15 | Feb 1, 2026 | Root Cause | Theory (later disproven) |
| 16 | Feb 1-3, 2026 | Custom Crypto | XSalsa20 + Double Ratchet |
| 17 | Feb 4, 2026 | Debug | Key Consistency |
| 18 | Feb 5, 2026 | **SOLVED** | 🎉 Bug #18 ONE LINE FIX! |
| 19 | Feb 5, 2026 | Header | Header Decrypt SUCCESS! (Bug #19) |
| 20 | Feb 6, 2026 | **PROFILE** | 🎉 Body Decrypt! Peer Profile! |
| 21 | Feb 6-7, 2026 | HELLO | v3 Format (Bugs #20-26) |
| **22** | **Feb 7, 2026** | **DISCOVERY** | **🎯 Reply Queue Flow (Bugs #27-31)** |

---

## Key Achievements

### ✅ Fully Working (Receive Chain)
- TLS 1.3 Handshake
- SMP Protocol (Contact + Reply Queues)
- X3DH Key Agreement
- Double Ratchet Header Decrypt (with PQ KEM skip)
- Double Ratchet Body Decrypt (dynamic offsets)
- Zstd Decompression
- ConnInfo JSON Parsing
- Peer Profile on ESP32: `"displayName": "cannatoshi"` 🎉

### ⚠️ Working but Not Needed
- HELLO send (Server OK) — Modern protocol doesn't use HELLO!

### ❌ Missing for "Connected"
1. Parse Reply Queue Info from Tag 'D' AgentConnInfoReply
2. Second TLS connection to Reply Queue server
3. SMP Handshake on Reply Queue
4. SKEY on Reply Queue
5. AgentConnInfo on Reply Queue
6. App receives → CON → "Connected"

---

## Bug Summary

**Total bugs found and fixed: 31**

| Category | Count | Sessions |
|----------|-------|----------|
| Length Prefix bugs | 7 | S4, S8 |
| KDF/IV Order bugs | 3 | S4, S8 |
| Byte Order bugs | 1 | S5 |
| Encoding bugs | 3 | S6 |
| NaCl Crypto bugs | 2 | S9 |
| Nonce bugs | 1 | S10C |
| Envelope Length bugs | 1 | S12-18 |
| Key Management bugs | 1 | S19-20 |
| HELLO Format bugs | 7 | S21 |
| E2E Version/KEM/NHK bugs | 5 | S22 |

**Lessons Learned: 83** (documented in BUG_TRACKER.md)

---

## Protocol Discoveries

### Session 22: Modern Protocol Flow
```
Modern SimpleX (v2 + senderCanSecure = True):
  - Does NOT need HELLO on Contact Queue
  - Expects AgentConnInfo on Reply Queue
  - Reply Queue Info in Tag 'D' (innermost ratchet layer)
```

### Session 22: Post-Quantum KEM
```
SimpleX uses SNTRUP761 (not Kyber1024):
  - Public Key: 1158 bytes
  - Ciphertext: 1039 bytes
  - Shared Secret: 32 bytes
  - PQ-Graceful-Degradation: KEM Nothing → pure DH
```

### Session 21: v3 EncRatchetMessage
```
v3 changes from v2:
  - encodeLarge switches at v≥3: 1-byte → 2-byte prefix
  - MsgHeader includes KEM Nothing ('0')
  - 4 Header Keys: HKs/NHKs/HKr/NHKr with promotion
```

### Session 16: Custom XSalsa20
```
SimpleX uses NON-STANDARD XSalsa20:
  - Standard: HSalsa20(key, nonce[0:16])
  - SimpleX:  HSalsa20(key, zeros[16])  ← ZEROS!
```

---

## Community Recognition

> *"Amazing project!"* - **Evgeny Poberezkin**, SimpleX Chat Founder

> *"what you did is impressive...first third-party SMP implementation"* - Evgeny

SimpleGo is confirmed as the **FIRST native SMP protocol implementation** outside the official Haskell codebase.

---

## Next Steps (Session 23)

1. **Parse Reply Queue Info** from AgentConfirmation Tag 'D'
2. **Second TLS Connection** to Reply Queue server
3. **SMP Handshake** on Reply Queue
4. **SKEY Command** on Reply Queue
5. **AgentConnInfo** (our profile) on Reply Queue
6. **App receives CON** → "Connected" 🎉

---

*Index updated: 2026-02-07 Session 22*
