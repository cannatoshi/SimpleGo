# SimpleGo Protocol Analysis

## Complete Development Documentation

This directory contains the complete, unabridged documentation of SimpleGo's development journey - the **world's first native SMP protocol implementation** outside the official SimpleX Haskell codebase.

---

## Current Status (2026-02-07 Session 22)

```
SESSION 22 - REPLY QUEUE FLOW DISCOVERED
==========================================

5 BUGS FIXED (#27-#31):
  #27: E2E version_min: 2→3 + KEM Nothing (App breaks silence!)
  #28: KEM Parser: Dynamic for SNTRUP761 (up to 2346 bytes)
  #29: Body Decrypt: Dynamic emHeader size calculation
  #30: HKs/NHKs Init + Promotion: Three-part header key fix
  #31: Try-Order: HKr (SameRatchet), NHKr (AdvanceRatchet)

BREAKTHROUGH DISCOVERY:
  Modern SimpleX (v2 + senderCanSecure) needs NO HELLO!
  App expects AgentConnInfo on Reply Queue instead.
  Reply Queue Info is inside Tag 'D' AgentConnInfoReply.

POST-QUANTUM:
  SimpleX uses SNTRUP761 (not Kyber1024)
  1158B pubkey, 1039B ciphertext, 32B shared secret
  PQ-Graceful-Degradation: KEM Nothing → pure DH fallback

MISSING FOR "CONNECTED":
  1. Parse Reply Queue Info from Confirmation (Tag 'D')
  2. Second TLS connection to Reply Queue server
  3. SMP Handshake on Reply Queue
  4. SKEY on Reply Queue
  5. AgentConnInfo on Reply Queue
  6. App receives → CON → "Connected"

ALL LAYERS THROUGH LAYER 8 (receive):
  ✅ Layer 0: TLS 1.3
  ✅ Layer 1: SMP Transport
  ✅ Layer 2: E2E Decrypt (S18)
  ✅ Layer 2.5: unPad (S19)
  ✅ Layer 3: ClientMessage Parse (S19)
  ✅ Layer 4: EncRatchetMessage Parse (S19, dynamic KEM S22)
  ✅ Layer 5: Double Ratchet Header Decrypt (S19, Try-Order S22)
  ✅ Layer 6: Double Ratchet Body Decrypt (S20, dynamic offsets S22)
  ✅ Layer 7: ConnInfo Parse + Zstd (S20)
  ✅ Layer 8: Peer Profile JSON (S20)
  ❌ Layer 9b-9f: Reply Queue Flow (MISSING)
  ⏳ Layer 10-11: CON → Connected

NEXT: Reply Queue Implementation (Session 23)
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
| [17_PART15_SESSION_18.md](17_PART15_SESSION_18.md) | ~600 | 🎉 BUG #18 SOLVED! E2E Decrypt SUCCESS |
| [18_PART16_SESSION_19.md](18_PART16_SESSION_19.md) | ~550 | Header Decrypt SUCCESS! |
| [19_PART17_SESSION_20.md](19_PART17_SESSION_20.md) | ~600 | Body Decrypt SUCCESS! Peer Profile! |
| [20_PART18_SESSION_21.md](20_PART18_SESSION_21.md) | ~700 | v3 Format + HELLO Debugging |
| [21_PART19_SESSION_22.md](21_PART19_SESSION_22.md) | ~650 | **Reply Queue Flow Discovery** |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~1800 | Complete bug documentation (31 bugs, 83 lessons) |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~1000 | Constants, wire formats, verified values |

**Total: ~30,000+ lines of detailed protocol analysis**

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
| 18 | Feb 5 | 🎉 BUG #18 SOLVED! E2E Decrypt SUCCESS! | #18 ✅ SOLVED |
| 19 | Feb 5 | Header Decrypt SUCCESS! MsgHeader Parsed | #19 found |
| 20 | Feb 6 | 🎉 Body Decrypt! Peer Profile on ESP32! | #19 ✅ SOLVED |
| 21 | Feb 6-7 | v3 Format + HELLO Debugging (7 bugs!) | #20-#26 |
| **22** | **Feb 7** | **🎯 Reply Queue Flow Discovery (5 bugs!)** | **#27-#31** |

---

## Session 22 Key Achievements

### 1. Five Bugs Fixed (#27-#31)

| Bug | Component | Fix |
|-----|-----------|-----|
| #27 | E2E version_min | 2 → 3 + KEM Nothing (App breaks silence!) |
| #28 | KEM Parser | Dynamic for SNTRUP761 (up to 2346 bytes) |
| #29 | Body Decrypt Pointer | Dynamic emHeader size calculation |
| #30 | HKs/NHKs Init + Promotion | Three-part header key chain fix |
| #31 | Header Decrypt Try-Order | HKr first, NHKr second for AdvanceRatchet |

### 2. Breakthrough Protocol Discovery

**Modern SimpleX (v2 + `senderCanSecure = True`) does NOT need HELLO!**

The modern protocol expects AgentConnInfo on the Reply Queue instead.

```
Modern Protocol Flow:
  1. ESP32 creates Invitation           ✅ Working
  2. App sends AgentConfirmation        ✅ Working
  3. ESP32 extracts Reply Queue Info    ❌ MISSING
  4. ESP32 connects to Reply Queue      ❌ MISSING
  5. ESP32 sends SKEY on Reply Queue    ❌ MISSING
  6. ESP32 sends AgentConnInfo          ❌ MISSING
  7. App receives → CON → "Connected"   ❌ Blocked
```

### 3. Post-Quantum KEM

SimpleX uses **SNTRUP761** (not Kyber1024):
- Public Key: 1158 bytes
- Ciphertext: 1039 bytes
- Shared Secret: 32 bytes

PQ-Graceful-Degradation: v3 + KEM Nothing → pure DH fallback (no error).

### 4. Reply Queue Info Location

The `smpReplyQueues` are inside Tag `'D'` (AgentConnInfoReply) at the innermost
ratchet-decrypted layer. We already decrypt them but only parse the profile.

---

## Session 21 Key Achievements

### 1. Seven HELLO Format Bugs Fixed (#20-#26)

| Bug | Component | Fix |
|-----|-----------|-----|
| #20 | PrivHeader for HELLO | '_' → 0x00 (no PrivHeader) |
| #21 | AgentVersion | v2 → v1 for AgentMessage |
| #22 | prevMsgHash | Raw → Word16 prefix encoding |
| #23 | cbEncrypt padding | Pad BEFORE encrypt |
| #24 | DH Key selection | rcv_dh → snd_dh for HELLO |
| #25 | PubHeader Nothing | Missing → '0' (0x30) |
| #26 | v2/v3 format | 1-byte → 2-byte prefixes + KEM Nothing |

### 2. v3 EncRatchetMessage Format

```
v3 changes from v2:
  - emHeader prefix: 1 byte → 2 bytes Word16 BE
  - emHeader size: 123 → 124 bytes
  - ehBody prefix: 1 byte → 2 bytes Word16 BE
  - MsgHeader: +KEM Nothing ('0'), contentLen 79→80
  - Verified byte-correct, Server accepts with OK
```

### 3. New Architecture

- **4 Header Keys:** HKs/NHKs/HKr/NHKr with promotion
- **SameRatchet vs AdvanceRatchet** modes
- **KEY Command** implemented (optional for unsecured queues)

---

## Session 20 Key Achievements

### 1. Bug #19 FIXED

Root cause: Debug self-decrypt test corrupted ratchet state.

### 2. Complete Crypto Chain

```
TLS 1.3 → SMP Transport → Server Decrypt → E2E Decrypt → unPad
→ ClientMessage → EncRatchetMessage → Header Decrypt
→ DH Ratchet Step → Chain KDF → Body Decrypt
→ unPad → ConnInfo 'I' → Zstd → Peer Profile JSON
```

### 3. Peer Profile Read

`"displayName": "cannatoshi"` — first SimpleX profile read on ESP32!

---

## Session 19 Key Achievements

### 1. Three New Layers + Header Decrypt SUCCESS

- unPad Layer, ClientMessage Layer, EncRatchetMessage Layer
- MsgHeader fully parsed (msgMaxVersion=3, PN=0, Ns=0)

---

## Session 18 Key Achievements

### 1. 🎉 BUG #18 SOLVED After Weeks of Debugging!

**Root Cause:** `envelope_len = plain_len - 2` included SMP padding  
**Fix:** `envelope_len = raw_len_prefix` — ONE LINE!

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
| #18 | Reply Queue E2E — SOLVED! | [Part 15](17_PART15_SESSION_18.md) |
| #19 | header_key_recv — SOLVED! | [Part 16](18_PART16_SESSION_19.md) + [Part 17](19_PART17_SESSION_20.md) |
| #20-#26 | HELLO format + v3 | [Part 18](20_PART18_SESSION_21.md) |
| **#27-#31** | **E2E v3, KEM, NHK, Try-Order** | [**Part 19**](21_PART19_SESSION_22.md) |

### By Topic

| Topic | Document |
|-------|----------|
| TLS 1.3, Basic SMP | [Part 1](03_PART1_INTRO_SESSIONS_1-2.md) |
| Wire format, smpEncode | [Part 2](04_PART2_SESSIONS_3-4.md), [Part 4](06_PART4_SESSION_7.md) |
| X448 Cryptography | [Part 3](05_PART3_SESSIONS_5-6.md) |
| AgentConfirmation | [Part 5](07_PART5_SESSION_8_BREAKTHROUGH.md) |
| Reply Queue E2E | [Part 6](08_PART6_SESSION_9.md) - [Part 15](17_PART15_SESSION_18.md) |
| Double Ratchet Header | [Part 16](18_PART16_SESSION_19.md) |
| Double Ratchet Body | [Part 17](19_PART17_SESSION_20.md) |
| ConnInfo + Zstd | [Part 17](19_PART17_SESSION_20.md) |
| HELLO + v3 Format | [Part 18](20_PART18_SESSION_21.md) |
| Reply Queue Flow | [**Part 19**](21_PART19_SESSION_22.md) |
| All Bugs | [BUG_TRACKER](BUG_TRACKER.md) |
| Quick Reference | [QUICK_REFERENCE](QUICK_REFERENCE.md) |

---

## License

This documentation is part of SimpleGo, licensed under AGPL-3.0.

---

*Last updated: February 7, 2026 - Session 22 (Reply Queue Flow Discovery)*
