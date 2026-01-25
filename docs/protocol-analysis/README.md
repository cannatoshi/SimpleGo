# SimpleGo Protocol Analysis

## Complete Development Documentation

This directory contains the complete, unabridged documentation of SimpleGo's development journey - the **world's first native SMP protocol implementation** outside the official SimpleX Haskell codebase.

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
| [SESSION_1_3_FOUNDATION.md](SESSION_1_3_FOUNDATION.md) | ~1200 | Project setup, TLS 1.3, basic SMP protocol |
| [SESSION_4_WIRE_FORMAT.md](SESSION_4_WIRE_FORMAT.md) | ~2500 | Wire format analysis, bugs #1-8 |
| [SESSION_5_X448_BREAKTHROUGH.md](SESSION_5_X448_BREAKTHROUGH.md) | ~1800 | The wolfSSL byte-order discovery, bug #9 |
| [SESSION_6_SMPQUEUEINFO.md](SESSION_6_SMPQUEUEINFO.md) | ~1500 | SMPQueueInfo encoding, bugs #10-12 |
| [SESSION_7_DEEP_RESEARCH.md](SESSION_7_DEEP_RESEARCH.md) | ~2000 | AES-GCM verification, Tail encoding, SimpleX team contact |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~500 | Constants, wire formats, KDF parameters |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~800 | Complete bug documentation with code |

**Total: ~10,000 lines of detailed protocol analysis**

---

## Historical Significance
`
SimpleGo Achievement:

  FIRST native SMP protocol implementation WORLDWIDE!
  - Outside the official Haskell codebase
  - Direct binary-level protocol communication
  - No WebSocket wrapper - true SMP protocol!

  All other "implementations" are wrappers around the JSON WebSocket API.
  SimpleGo speaks the REAL SMP protocol at the binary level.

  January 25, 2026: Message forwarded to Evgeny Poberezkin (SimpleX founder)!
`

---

## Project Timeline

| Session | Date | Milestone | Bugs Fixed |
|---------|------|-----------|------------|
| 1-3 | Dec 2025 | Foundation, TLS 1.3, Basic SMP | - |
| 4 | Jan 23, 2026 | Wire format analysis | #1-8 |
| 5 | Jan 24, 2026 | X448 byte-order breakthrough | #9 |
| 6 | Jan 24, 2026 | SMPQueueInfo encoding | #10-12 |
| 7 | Jan 24-25, 2026 | Crypto verification, SimpleX contact | - |

---

## Current Status
`
Verification Status (Session 7):

  [OK] X448 Diffie-Hellman        (Python match - 100%)
  [OK] X3DH HKDF                  (Python match - 100%)
  [OK] Root KDF                   (Python match - 100%)
  [OK] Chain KDF                  (Python match - 100%)
  [OK] AES-256-GCM                (Python match - 100%)
  [OK] 16-byte IV GHASH           (Python match - 100%)
  [OK] Wire Format                (Haskell source verified)
  [OK] 12 Encoding Bugs           (All fixed)
  [OK] Server Acceptance          ("OK" response)
  [OK] Tail Encoding              (Verified correct)
  [OK] MsgHeader Padding          (Word16 + '#')

  [??] App Parsing                (A_MESSAGE error persists)

  Status: Awaiting response from Evgeny Poberezkin
`

---

## Quick Navigation

### By Topic

- **TLS Connection**: [Session 1-3](SESSION_1_3_FOUNDATION.md#6-tls-13-connection)
- **SMP Handshake**: [Session 1-3](SESSION_1_3_FOUNDATION.md#8-smp-handshake)
- **Double Ratchet**: [Session 4](SESSION_4_WIRE_FORMAT.md#2-protocol-stack-deep-dive)
- **X448 Cryptography**: [Session 5](SESSION_5_X448_BREAKTHROUGH.md)
- **Wire Format**: [Quick Reference](QUICK_REFERENCE.md#3-wire-formats)
- **All Bugs**: [Bug Tracker](BUG_TRACKER.md)

### By Bug Number

| Bug | Description | Document |
|-----|-------------|----------|
| #1 | E2E key length | [Session 4](SESSION_4_WIRE_FORMAT.md#5-bug-1-e2e-key-length) |
| #2 | prevMsgHash length | [Session 4](SESSION_4_WIRE_FORMAT.md#6-bug-2-prevmsghash-length) |
| #3 | MsgHeader DH key | [Session 4](SESSION_4_WIRE_FORMAT.md#7-bug-3-msgheader-dh-key-length) |
| #4 | ehBody length | [Session 4](SESSION_4_WIRE_FORMAT.md#8-bug-4-ehbody-length) |
| #5 | emHeader size | [Session 4](SESSION_4_WIRE_FORMAT.md#9-bug-5-emheader-size) |
| #6 | Payload AAD size | [Session 4](SESSION_4_WIRE_FORMAT.md#10-bug-6-payload-aad-size) |
| #7 | Root KDF order | [Session 4](SESSION_4_WIRE_FORMAT.md#11-bug-7-root-kdf-output-order) |
| #8 | Chain KDF IV order | [Session 4](SESSION_4_WIRE_FORMAT.md#12-bug-8-chain-kdf-iv-order) |
| #9 | wolfSSL X448 byte-order | [Session 5](SESSION_5_X448_BREAKTHROUGH.md#3-bug-9-wolfssl-x448-byte-order) |
| #10 | Port encoding | [Session 6](SESSION_6_SMPQUEUEINFO.md#3-bug-10-port-encoding) |
| #11 | smpQueues count | [Session 6](SESSION_6_SMPQUEUEINFO.md#4-bug-11-smpqueues-count) |
| #12 | queueMode Nothing | [Session 6](SESSION_6_SMPQUEUEINFO.md#5-bug-12-queuemode-nothing) |

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

*Last updated: January 25, 2026 - Session 7 (SimpleX Team Contact)*
