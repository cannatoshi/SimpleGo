# SimpleGo Development Roadmap

> Strategic development plan for the first native SimpleX SMP client on ESP32

---

## Overview

SimpleGo development follows a phased approach, building from protocol fundamentals to a complete standalone messaging device.

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
│  Phase 3.10: Connection Complete  ████████░░░░░░░░░░░░  40% 🔧      │
│  Phase 4: User Interface          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 5: Double Ratchet          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🔧 MILESTONE: Peer Queue Parsing Complete!

As of v0.1.13-alpha, AgentInvitation properly parsed!

| Feature | Status |
|---------|--------|
| Message Type Fix ('_' + 3) | ✅ |
| `peer_queue_t` Structure | ✅ |
| `url_decode_inplace()` | ✅ |
| Peer Server Extraction | ✅ |
| Queue ID Extraction | ✅ |
| "READY TO SEND CONFIRMATION" | ✅ |
| DH Key Extraction | 🔧 In Progress |

---

## Phase 3.9: Peer Queue Parsing ✅ COMPLETE

**Goal**: Extract peer connection info from AgentInvitation

### Deliverables

| Task | Status |
|------|--------|
| Message Type Parsing Fix | ✅ |
| Find '_' Delimiter | ✅ |
| Read Version (BE uint16) | ✅ |
| Read Type ('C'/'I'/'M'/'R') | ✅ |
| `peer_queue_t` Structure | ✅ |
| URL Decode (multi-pass) | ✅ |
| Extract Peer Server | ✅ |
| Extract Queue ID | ✅ |

### Message Format Discovery

```
After DH Decryption:

2a a5 5f 00 07 49 ...
*  ?  _  ver   I
0  1  2  3  4  5

Position 2: '_' (Delimiter)
Position 3-4: Version (Big Endian, 0x0007 = v7)
Position 5: Message Type ('I' = Invitation)
```

---

## Phase 3.10: Connection Complete 🔧 IN PROGRESS

**Goal**: Complete bidirectional connection with SimpleX apps

**Target**: January 2026

### Deliverables

| Task | Status | Priority |
|------|--------|----------|
| DH Key Extraction | 🔧 In Progress | Critical |
| Connect to Peer Server | ⏳ Next | Critical |
| AgentConfirmation Builder | ⏳ Next | Critical |
| SEND CONF to Peer Queue | ⏳ Next | Critical |
| SimpleX App shows "Connected" | ⏳ Goal | Critical |

### DH Key Search Patterns

The `dh=` parameter is deeply nested and multi-encoded:

```
Raw: %26dh%3DMCowBQYDK2VuAyEAWjdWg-4cHabdeVsdhOtIvEZXxaHZKtQlZeXrBj0Z7EU%253D

Search patterns:
- dh=          (direct)
- dh%3D        (once encoded)
- %26dh%3D     (twice encoded, &dh=)
```

### Connection Flow

```
┌──────────┐                              ┌──────────┐
│ SimpleX  │                              │  ESP32   │
│   App    │                              │          │
└────┬─────┘                              └────┬─────┘
     │                                         │
     │  1. Scans Contact Link                  │
     │  2. SEND AgentInvitation ───────────────>
     │     (Reply Queue + Profile)             │
     │                                         │
     │  3. ESP32 extracts:                     │
     │     - Peer Server ✅                    │
     │     - Queue ID ✅                       │
     │     - DH Key 🔧                         │
     │                                         │
     │  4. ESP32 connects to Peer Server       │  ⏳
     │  5. SEND AgentConfirmation              │  ⏳
     │     <─────────────────────────────────────
     │                                         │
     │  6. "Connected!"                        │
```

---

## Phase 4: User Interface 📋 PLANNED

**Goal**: Complete messaging UI for T-Embed/T-Deck hardware

**Target**: Q1-Q2 2026

### Deliverables

| Task | Status | Priority |
|------|--------|----------|
| Display Driver (ST7789) | 📋 | Critical |
| LVGL Integration | 📋 | Critical |
| QR Code Display | 📋 | High |
| Contact List View | 📋 | High |
| Message View | 📋 | High |
| Keyboard Driver (T-Deck) | 📋 | High |

---

## Phase 5: Double Ratchet 📋 FUTURE

**Goal**: Full end-to-end encryption with forward secrecy

**Target**: Q2-Q3 2026

### Agent Message Types to Implement

| Type | Name | Priority |
|------|------|----------|
| `'C'` | AgentConfirmation | Critical (Phase 3.10) |
| `'M'` | AgentMsgEnvelope | High (Phase 5) |
| `'R'` | AgentRatchetKey | High (Phase 5) |

---

## Timeline Summary

```
2026 Q1
├── January   ✅ Phase 1-3.9 Complete!
│             ├── Protocol Foundation
│             ├── Full Messaging + E2E
│             ├── Multi-Contact + Persistence
│             ├── Invitation Links
│             ├── Agent Protocol (v0.1.12)
│             └── Peer Queue Parsing (v0.1.13)
├── January   🔧 Phase 3.10 (Connection Complete)
├── February  📋 Phase 4 Start (UI)
└── March     📋 Phase 4 Continue

2026 Q2-Q4
├── April     📋 Phase 4 Complete
├── May       📋 Phase 5 Start (Double Ratchet)
└── December  🎯 Version 1.0 Release
```

---

## Current Priorities

### Immediate (v0.1.14)

1. **DH Key Extraction** — Handle multi-encoded URLs
2. **Connect to Peer Server** — TLS to smp15.simplex.im etc.
3. **AgentConfirmation Builder** — Create response message
4. **SEND to Peer** — Complete connection handshake

### Short-term

5. SimpleX App shows "Connected"
6. T-Embed UI — Display + Rotary Encoder
7. QR Code Display

### Medium-term

8. Double Ratchet (Curve448)
9. Group Messaging

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.13-alpha** | **2026-01-21** | **🔧 Message Type Fix + Peer Queue!** |
| v0.1.12-alpha | 2026-01-21 | 🔐 Agent Protocol |
| v0.1.11-alpha | 2026-01-20 | 🔗 Invitation Links |
| v0.1.10-alpha | 2026-01-20 | 🏆 Multi-Contact + E2E |
| v0.1.9-alpha | 2026-01-20 | DEL + Full SMP Client |
| v0.1.8-alpha | 2026-01-20 | NVS Persistence |
| v0.1.7-alpha | 2026-01-20 | ACK Command |
| v0.1.6-alpha | 2026-01-20 | E2E Decryption |
| v0.1.5-alpha | 2026-01-20 | SEND + MSG |
| v0.1.4-alpha | 2026-01-20 | SUB Command |
| v0.1.3-alpha | 2026-01-19 | NEW Command |
| v0.1.2-alpha | 2026-01-18 | Handshake |
| v0.1.1-alpha | 2026-01-17 | TLS 1.3 |
| v0.1.0-alpha | 2026-01-16 | Initial |

---

## References

- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [SimpleX Agent Protocol](https://github.com/simplex-chat/simplexmq/tree/stable/src/Simplex/Messaging/Agent)
- [LVGL Documentation](https://docs.lvgl.io/)
