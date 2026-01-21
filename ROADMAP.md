# SimpleGo Development Roadmap

> Strategic development plan for the first native SimpleX SMP client on ESP32

---

## Overview

```
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
│  Phase 3.11: encConnInfo Fix      ████████░░░░░░░░░░░░  40% 🔧      │
│  Phase 4: User Interface          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 5: Double Ratchet          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🏗️ MILESTONE: Modular Architecture + Peer Connection!

As of v0.1.14-alpha:

| Feature | Status |
|---------|--------|
| Modular Architecture (8 modules) | ✅ |
| Peer Server TLS Connection | ✅ |
| SMP Handshake with Peer | ✅ |
| AgentConfirmation Sent | ✅ Server OK |
| App Shows "Connected" | 🔧 Format Issue |

---

## Phase 3.10: Peer Connection ✅ COMPLETE

**Goal**: Connect to peer's SMP server and send confirmation

### Deliverables

| Task | Status |
|------|--------|
| Modular Refactor | ✅ |
| smp_peer.c Module | ✅ |
| peer_connect() | ✅ |
| send_agent_confirmation() | ✅ |
| Auto-Connect on Invitation | ✅ |
| Server Accepts with "OK" | ✅ |

---

## Phase 3.11: encConnInfo Fix 🔧 IN PROGRESS

**Goal**: Fix AgentConfirmation format so App shows "Connected"

### Current Issue

Server accepts Confirmation with "OK", but SimpleX App doesn't show "Connected".

### Analysis

From Haskell source:
```haskell
AgentConfirmation {agentVersion, e2eEncryption_, encConnInfo}
```

`encConnInfo` likely needs:
- Profile information
- Ratchet initialization data
- Proper encryption

### Deliverables

| Task | Status | Priority |
|------|--------|----------|
| Analyze encConnInfo format | 🔧 | Critical |
| Include profile data | ⏳ | Critical |
| Proper encryption | ⏳ | Critical |
| App Shows "Connected" | ⏳ | Goal |

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

---

## Phase 5: Double Ratchet 📋 FUTURE

**Target**: Q2-Q3 2026

| Task | Status |
|------|--------|
| X3DH Key Agreement | 📋 |
| Double Ratchet Algorithm | 📋 |
| AgentMsgEnvelope ('M') | 📋 |
| Curve448 Support | 📋 |

---

## Architecture Evolution

```
v0.1.0-v0.1.13:
┌─────────────────────────────────────┐
│  main.c (~1800 lines)               │
│  └── Everything in one file         │
└─────────────────────────────────────┘

v0.1.14+:
┌─────────────────────────────────────┐
│  main.c (~350 lines)                │
├─────────────────────────────────────┤
│  smp_peer.c    │  smp_parser.c      │
│  smp_contacts.c│  smp_network.c     │
│  smp_crypto.c  │  smp_utils.c       │
│  smp_globals.c │  include/*.h       │
└─────────────────────────────────────┘
```

---

## Current Priorities

### Immediate (v0.1.15)

1. **encConnInfo Format** — Analyze Haskell source
2. **Profile Data** — Include in confirmation
3. **App "Connected"** — Complete handshake

### Short-term

4. T-Embed UI
5. QR Code Display

### Medium-term

6. Double Ratchet
7. Group Messaging

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.14-alpha** | **2026-01-21** | **🏗️ Modular + Peer!** |
| v0.1.13-alpha | 2026-01-21 | 🔧 Message Type Fix |
| v0.1.12-alpha | 2026-01-21 | 🔐 Agent Protocol |
| v0.1.11-alpha | 2026-01-20 | 🔗 Invitation Links |
| v0.1.10-alpha | 2026-01-20 | 🏆 Multi-Contact |

---

## References

- [SimpleX Protocol](https://github.com/simplex-chat/simplexmq)
- [LVGL Documentation](https://docs.lvgl.io/)
