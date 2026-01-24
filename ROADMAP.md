# SimpleGo Development Roadmap

> Strategic development plan for the first native SimpleX SMP client on ESP32

---

## Overview
`
┌─────────────────────────────────────────────────────────────────────┐
│                        DEVELOPMENT PHASES                           │
├─────────────────────────────────────────────────────────────────────┤
│  Phase 1: Protocol Foundation     ████████████████████ 100% ✅      │
│  Phase 2: Full Messaging          ████████████████████ 100% ✅      │
│  Phase 3: E2E Encryption          ████████████████████ 100% ✅      │
│  Phase 3.5: Persistence           ████████████████████ 100% ✅      │
│  Phase 3.6: Multi-Contact         ████████████████████ 100% ✅      │
│  Phase 3.7: Invitation Links      ████████████████████ 100% ✅      │
│  Phase 3.8: Agent Protocol        ████████████████████ 100% ✅      │
│  Phase 3.9: Peer Queue Parsing    ████████████████████ 100% ✅      │
│  Phase 3.10: Peer Connection      ████████████████████ 100% ✅      │
│  Phase 3.11: Double Ratchet       ████████████████████ 100% ✅      │
│  Phase 3.12: App Compatibility    ████████░░░░░░░░░░░░  40% 🔧      │
│  Phase 4: User Interface          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 5: Production Ready        ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
└─────────────────────────────────────────────────────────────────────┘
`

---

## 🏆 Historical Significance

**SimpleGo is the FIRST native SMP protocol implementation worldwide!**

All other implementations are WebSocket API wrappers. We implemented:
- Complete SMP binary protocol
- X3DH key agreement from scratch
- Double Ratchet algorithm
- All wire format encoding

---

## Phase 3.11: Double Ratchet ✅ COMPLETE

**Goal**: Implement complete Double Ratchet with X3DH key agreement

### Deliverables

| Task | Status |
|------|--------|
| X448 Key Generation | ✅ |
| X448 DH with byte-order fix | ✅ |
| X3DH Key Agreement | ✅ |
| HKDF-SHA512 | ✅ |
| Root Ratchet KDF | ✅ |
| Chain Ratchet KDF | ✅ |
| AES-GCM Encryption | ✅ |
| MsgHeader Encoding | ✅ |
| EncMessageHeader | ✅ |
| EncRatchetMessage | ✅ |
| AgentConfirmation Building | ✅ |
| HELLO Message | ✅ |
| Python Verification | ✅ 100% match |
| Server Acceptance | ✅ "OK" |

### New Modules Created

| Module | Lines | Purpose |
|--------|-------|---------|
| smp_x448.c | ~200 | X448 with wolfSSL byte-order fix |
| smp_ratchet.c | ~500 | Double Ratchet, KDFs, AES-GCM |
| smp_handshake.c | ~300 | E2E handshake, AgentConfirmation |
| smp_queue.c | ~250 | SMPQueueInfo encoding |

### Bugs Fixed (12 Total)

| Category | Count |
|----------|-------|
| Length Prefix | 7 |
| KDF Order | 2 |
| Crypto Library | 1 |
| Format | 2 |

---

## Phase 3.12: App Compatibility 🔧 IN PROGRESS

**Goal**: Fix remaining format issues so SimpleX App shows "Connected"

### Current Issue
`
Server: Accepts AgentConfirmation with "OK" ✅
Server: Accepts HELLO with "OK" ✅
App: Shows "error agent AGENT A_MESSAGE" ❌
`

**A_MESSAGE** = Parsing error (format wrong, crypto OK)
**A_CRYPTO** = Crypto error (decryption failed)

Our error is A_MESSAGE → Decryption works, format is wrong!

### Current Hypothesis: Tail Encoding
`haskell
-- Haskell uses "Tail" for last fields:
smpEncode (..., Tail encConnInfo)
--              ^^^^
--              NO LENGTH PREFIX!
`

If we add length prefix before Tail fields, parser fails.

### Deliverables

| Task | Status | Priority |
|------|--------|----------|
| Verify Tail encoding | 🔧 | Critical |
| Check encConnInfo format | 🔧 | Critical |
| Check emBody format | 🔧 | Critical |
| App shows "Connected" | ⏳ | Goal |

---

## Phase 4: User Interface 📋 PLANNED

**Target**: Q1-Q2 2026

| Task | Status |
|------|--------|
| Display Driver (ST7789) | 📋 |
| LVGL Integration | 📋 |
| QR Code Display | 📋 |
| Contact List View | 📋 |
| Message View | 📋 |
| Keyboard Input | 📋 |

---

## Phase 5: Production Ready 📋 FUTURE

**Target**: Q3-Q4 2026

| Task | Status |
|------|--------|
| Group Messaging | 📋 |
| File Transfer | 📋 |
| Battery Optimization | 📋 |
| OTA Updates | 📋 |
| Security Audit | 📋 |

---

## Architecture Evolution
`
v0.1.0-v0.1.13: Monolithic
┌─────────────────────────────────────┐
│  main.c (~1800 lines)               │
│  └── Everything in one file         │
└─────────────────────────────────────┘

v0.1.14: Modular
┌─────────────────────────────────────┐
│  main.c (~350 lines)                │
├─────────────────────────────────────┤
│  8 modules, 7 headers               │
└─────────────────────────────────────┘

v0.1.15: Crypto Layer Added
┌─────────────────────────────────────┐
│  main.c                             │
├─────────────────────────────────────┤
│  smp_x448    │  smp_ratchet         │
│  smp_handshake │ smp_queue          │
├─────────────────────────────────────┤
│  smp_peer    │  smp_parser          │
│  smp_network │  smp_contacts        │
├─────────────────────────────────────┤
│  wolfssl     │  kyber               │
└─────────────────────────────────────┘
`

---

## Verification Methods

| Method | Purpose |
|--------|---------|
| Python Comparison | Verify crypto output byte-by-byte |
| Haskell Source Analysis | Understand exact encoding |
| Hex Dump Analysis | Debug wire format |
| Server Response | Confirm message acceptance |

---

## Current Priorities

### Immediate (v0.1.16)

1. **Tail Encoding Fix** — Verify no length prefix on Tail fields
2. **App Compatibility** — Complete handshake with SimpleX App

### Short-term

3. T-Deck/T-Embed UI
4. QR Code Display

### Medium-term

5. Group Messaging
6. File Transfer

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.15-alpha** | **2026-01-24** | **🔐 Double Ratchet!** |
| v0.1.14-alpha | 2026-01-21 | 🏗️ Modular + Peer |
| v0.1.13-alpha | 2026-01-21 | 🔧 Message Type Fix |
| v0.1.12-alpha | 2026-01-21 | 🔐 Agent Protocol |
| v0.1.11-alpha | 2026-01-20 | 🔗 Invitation Links |
| v0.1.10-alpha | 2026-01-20 | 🏆 Multi-Contact |

---

## References

- [SimpleX Protocol](https://github.com/simplex-chat/simplexmq)
- [Signal Double Ratchet](https://signal.org/docs/specifications/doubleratchet/)
- [X3DH Specification](https://signal.org/docs/specifications/x3dh/)
- [LVGL Documentation](https://docs.lvgl.io/)

---

*Last updated: January 24, 2026 — v0.1.15-alpha*
