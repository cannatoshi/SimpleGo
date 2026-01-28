# SimpleGo Protocol Analysis

## Complete Development Documentation

This directory contains the complete, unabridged documentation of SimpleGo's development journey - the **world's first native SMP protocol implementation** outside the official SimpleX Haskell codebase.

---

## Current Status (2026-01-28 Session 10C)

```
Session 10C - Reply Queue Three-Layer Analysis:
- Reply Queue Server-Level Decrypt: WORKING
- Reply Queue Per-Queue DH: ALL TESTS FAIL
- Contact Queue: Fully working (3-layer)
- Developer question required for Per-Queue DH key

Tested Key Combinations (all failed):
- peer_ephemeral_pub + rcv_dh_private + msgId
- srv_dh_public + rcv_dh_private + msgId  
- shared_secret (direct) + message_nonce
- Direct on raw data
```

---

## Acknowledgments and Respect

**This project would not be possible without the incredible work of the SimpleX team.**

SimpleX Chat represents a groundbreaking achievement in privacy-preserving communication technology. The protocol design is elegant, well-thought-out, and prioritizes user privacy above all else. We have the deepest respect for:

- **Evgeny Poberezkin** and the entire SimpleX Chat team
- The brilliant cryptographic design combining X3DH, Double Ratchet, and post-quantum algorithms
- The commitment to open source (AGPL-3.0) that made this project possible
- The comprehensive Haskell implementation that served as our reference

**SimpleGo is a tribute to their work**, not a replacement. Our goal is to expand the SimpleX ecosystem by bringing the protocol to embedded hardware, enabling new use cases while maintaining the same security guarantees.

**Links:**
- SimpleX Chat: https://simplex.chat
- SimpleX GitHub: https://github.com/simplex-chat
- Protocol Documentation: https://github.com/simplex-chat/simplexmq

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
| [09_PART7_SESSION_10.md](09_PART7_SESSION_10.md) | ~400 | Reply Queue Per-Queue DH analysis |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~1100 | Complete bug documentation (17 bugs) |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~650 | Constants, wire formats, KDF parameters |

**Total: ~13,000 lines of detailed protocol analysis**

---

## Historical Significance

```
SimpleGo Achievement:

  FIRST native SMP protocol implementation WORLDWIDE!
  - Outside the official Haskell codebase
  - Direct binary-level protocol communication
  - No WebSocket wrapper - true SMP protocol!

  All other "implementations" are wrappers around the JSON WebSocket API.
  SimpleGo speaks the REAL SMP protocol at the binary level.
```

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
| **10C** | **Jan 28, 2026** | **Reply Queue Per-Queue DH analysis** | **#17 (active)** |

---

## Quick Navigation

### By Topic

- **TLS Connection**: [Part 1](03_PART1_INTRO_SESSIONS_1-2.md)
- **SMP Handshake**: [Part 1](03_PART1_INTRO_SESSIONS_1-2.md)
- **Double Ratchet**: [Part 2](04_PART2_SESSIONS_3-4.md)
- **X448 Cryptography**: [Part 3](05_PART3_SESSIONS_5-6.md)
- **Wire Format**: [Quick Reference](QUICK_REFERENCE.md)
- **All Bugs**: [Bug Tracker](BUG_TRACKER.md)
- **Breakthrough**: [Part 5 - Session 8](07_PART5_SESSION_8_BREAKTHROUGH.md)
- **Reply Queue HSalsa20**: [Part 6 - Session 9](08_PART6_SESSION_9.md)
- **Reply Queue Per-Queue DH**: [Part 7 - Session 10](09_PART7_SESSION_10.md)

### By Bug Number

| Bug | Description | Document |
|-----|-------------|----------|
| #1-8 | Wire format bugs | [Part 2](04_PART2_SESSIONS_3-4.md) |
| #9 | wolfSSL X448 byte-order | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #10-12 | SMPQueueInfo encoding | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #13-14 | AAD prefix, IV order | [Part 5](07_PART5_SESSION_8_BREAKTHROUGH.md) |
| #15-16 | HSalsa20, A_CRYPTO | [Part 6](08_PART6_SESSION_9.md) |
| **#17** | **Reply Queue Per-Queue DH (active)** | [**Part 7**](09_PART7_SESSION_10.md) |

---

## Document Rules

**These documents follow strict rules to preserve knowledge:**

1. **No Deletions** - Even incorrect theories remain, marked as disproven
2. **Every Change is Logged** - Changelog at the end of each session
3. **Code Changes Documented** - Every fix is recorded with before/after
4. **For Chat Continuity** - These documents travel between chat sessions
5. **Capture Everything** - Theories, hypotheses, discoveries, ALL of it

---

## Related Documentation

| Document | Description |
|----------|-------------|
| [../ARCHITECTURE.md](../ARCHITECTURE.md) | Module structure overview |
| [../CRYPTO.md](../CRYPTO.md) | Cryptographic implementation summary |
| [../WIRE_FORMAT.md](../WIRE_FORMAT.md) | Protocol encoding summary |
| [../../CHANGELOG.md](../../CHANGELOG.md) | Version history |

---

## License

This documentation is part of SimpleGo, licensed under AGPL-3.0.

The SimpleX protocol is the intellectual property of SimpleX Chat Ltd, used here under AGPL-3.0 for interoperability purposes.

---

*Last updated: January 28, 2026 - Session 10C (Reply Queue Per-Queue DH Analysis)*
