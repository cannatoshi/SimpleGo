# SimpleGo Protocol Analysis

## Complete Development Documentation

This directory contains the complete, unabridged documentation of SimpleGo's development journey - the **world's first native SMP protocol implementation** outside the official SimpleX Haskell codebase.

---

## Current Status (2026-02-06 Session 20)

```
SESSION 20 - BODY DECRYPT SUCCESS! PEER PROFILE READ ON ESP32!
================================================================

BREAKTHROUGH:
  Complete crypto chain working end-to-end:
    TLS 1.3 → SMP Transport → Server Decrypt → E2E Decrypt → unPad
    → ClientMessage → EncRatchetMessage → Header Decrypt (AES-GCM)
    → DH Ratchet Step (2× rootKdf) → Chain KDF → Body Decrypt (AES-GCM)
    → unPad → AgentConnInfo 'I' → Zstd Decompress → Peer Profile JSON

  Peer profile read: "displayName": "cannatoshi" on an ESP32!

BUG #19 FIXED:
  Root cause: Debug self-decrypt test in smp_peer.c:347
  Side effects of ratchet_decrypt() corrupted ratchet state
  Fix: Removed debug test, merged to main

NEW CAPABILITIES:
  - DH Ratchet Step (2× rootKdf: recv chain + send chain)
  - Body Decrypt via AES-256-GCM (14832 → 8887 bytes)
  - ConnInfo parsing ('I' = AgentConnInfo, 'D' = AgentConnInfoReply)
  - Zstd decompression (8881 → 12268 bytes JSON)
  - XInfo Profile JSON parsing (displayName, image, preferences)

10 KEY INSIGHTS:
  - DH Ratchet Step = TWO rootKdf calls (recv + send)
  - iv1 = Body IV, iv2 = Header IV (correction from earlier)
  - ConnInfo: 'I' = profile only, 'D' = queues + profile
  - Zstd: 'X' marker, '1'=compressed, '0'=passthrough
  - Body AAD = rcAD || emHeader (raw bytes, 235 total)

ALL LAYERS THROUGH LAYER 8:
  ✅ Layer 0: TLS 1.3
  ✅ Layer 1: SMP Transport
  ✅ Layer 2: E2E Decrypt (S18)
  ✅ Layer 2.5: unPad (S19)
  ✅ Layer 3: ClientMessage Parse (S19)
  ✅ Layer 4: EncRatchetMessage Parse (S19)
  ✅ Layer 5: Double Ratchet Header Decrypt (S19)
  ✅ Layer 6: Double Ratchet Body Decrypt (S20)
  ✅ Layer 7: ConnInfo Parse + Zstd (S20)
  ✅ Layer 8: Peer Profile JSON (S20)
  ⏳ Layer 9: Connection Established (HELLO processing)

NEXT: HELLO processing, Ratchet State Persistence, Bidirectional Messaging
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
| [19_PART17_SESSION_20.md](19_PART17_SESSION_20.md) | ~600 | **Body Decrypt SUCCESS! Peer Profile!** |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~1400 | Complete bug documentation (19 bugs, 57 lessons) |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~800 | Constants, wire formats, verified values |

**Total: ~25,000+ lines of detailed protocol analysis**

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
| **20** | **Feb 6** | **🎉 Body Decrypt! Peer Profile on ESP32!** | **#19 ✅ SOLVED** |

---

## Session 20 Key Achievements

### 1. Bug #19 FIXED

| Aspect | Detail |
|--------|--------|
| Root Cause | Debug self-decrypt test in smp_peer.c:347 |
| Problem | `ratchet_decrypt()` on own message triggered spurious DH ratchet step |
| Fix | Removed debug test, merged to main |
| Lesson | Tests must NEVER modify production state |

### 2. DH Ratchet Step Implemented

Two rootKdf calls per DH ratchet step:
- rootKdf #1: `peer_new_pub × our_old_priv` → recv chain
- rootKdf #2: `peer_new_pub × our_NEW_priv` → send chain

### 3. Body Decrypt SUCCESS

```
Chain KDF → message_key + iv_body
AES-256-GCM Decrypt: 14832 bytes → 8889 bytes → unPad → 8887 bytes
```

### 4. ConnInfo Parsed

| Tag | Constructor | Content |
|-----|------------|---------|
| 'I' | AgentConnInfo | Profile only (Reply Queue) |
| 'D' | AgentConnInfoReply | SMP Queues + Profile (Contact Queue) |

### 5. Zstd Decompression

- Integrated zstd v1.5.5 as ESP-IDF component (~117KB Flash)
- 8881 bytes compressed → 12268 bytes JSON

### 6. Peer Profile Read

```json
{
  "event": "x.info",
  "params": {
    "profile": {
      "displayName": "cannatoshi"
    }
  }
}
```

First time a peer's SimpleX profile has been read on an ESP32!

### 7. Complete Crypto Chain

```
TLS 1.3 → SMP Transport → Server Decrypt → E2E Decrypt → unPad
→ ClientMessage → EncRatchetMessage → Header Decrypt
→ DH Ratchet Step → Chain KDF → Body Decrypt
→ unPad → ConnInfo 'I' → Zstd → Peer Profile JSON
```

---

## Session 19 Key Achievements

### 1. Three New Layers Discovered

| Layer | Name | Content |
|-------|------|---------|
| 2.5 | unPad | [2B len][content][padding 0x23...] |
| 3 | ClientMessage | PrivHeader + AgentMsgEnvelope |
| 4 | EncRatchetMessage | [emHeader][emAuthTag][Tail emBody] |

### 2. Double Ratchet Header Decrypt SUCCESS!

```
Key: saved_nhk (HKDF[32-63] from X3DH)
IV: ehIV (16 bytes from EncMessageHeader)
AAD: rcAD (112 bytes = our_key1 || peer_key1)
Result: MsgHeader fully parsed!
```

### 3. MsgHeader Parsed

| Field | Value |
|-------|-------|
| msgMaxVersion | 3 (Peer supports PQ) |
| DH Key | 68 bytes X448 SPKI |
| PN | 0 (first message) |
| Ns | 0 (Message #0) |

### 4. 11 Key Insights

- unPad layer exists between E2E decrypt and ClientMessage
- PrivHeader: 'K'=PHConfirmation, '_'=PHEmpty
- Maybe encoding: '0'=Nothing, '1'=Just (NOT 0x00/0x01!)
- nhk from X3DH HKDF[32-63] = header_key_recv
- AES-GCM uses 16-byte IV in SimpleX

### 5. Bug #19 Found

`header_key_recv` gets overwritten somewhere. Workaround with `saved_nhk` works.

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
| 102 bytes padding | SMP block-padding (0x23) for traffic analysis resistance |

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
| #19 | header_key_recv — SOLVED! | [Part 16](18_PART16_SESSION_19.md) + [**Part 17**](19_PART17_SESSION_20.md) |

### By Topic

| Topic | Document |
|-------|----------|
| TLS 1.3, Basic SMP | [Part 1](03_PART1_INTRO_SESSIONS_1-2.md) |
| Wire format, smpEncode | [Part 2](04_PART2_SESSIONS_3-4.md), [Part 4](06_PART4_SESSION_7.md) |
| X448 Cryptography | [Part 3](05_PART3_SESSIONS_5-6.md) |
| AgentConfirmation | [Part 5](07_PART5_SESSION_8_BREAKTHROUGH.md) |
| Reply Queue E2E | [Part 6](08_PART6_SESSION_9.md) - [Part 15](17_PART15_SESSION_18.md) |
| Double Ratchet Header | [Part 16](18_PART16_SESSION_19.md) |
| Double Ratchet Body | [**Part 17**](19_PART17_SESSION_20.md) |
| ConnInfo + Zstd | [**Part 17**](19_PART17_SESSION_20.md) |
| All Bugs | [BUG_TRACKER](BUG_TRACKER.md) |
| Quick Reference | [QUICK_REFERENCE](QUICK_REFERENCE.md) |

---

## License

This documentation is part of SimpleGo, licensed under AGPL-3.0.

---

*Last updated: February 6, 2026 - Session 20 (Body Decrypt SUCCESS! Peer Profile Read!)*
