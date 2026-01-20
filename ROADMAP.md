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
│  Phase 3.9: Connection Complete   ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 4: User Interface          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 5: Double Ratchet          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🔐 MILESTONE: Agent Protocol Complete!

As of v0.1.12-alpha, full 6-layer message stack decoded!

| Feature | Status |
|---------|--------|
| Contact Link URL Encoding Fix | ✅ Base64URL + double-encoded = |
| SMP E2E Decryption (Layer 3) | ✅ |
| Client Message Decryption (Layer 5) | ✅ |
| Agent Protocol Parsing (Layer 6) | ✅ |
| AgentInvitation Detection ('I') | ✅ |
| Reply Queue URI Extraction | ✅ |
| Peer Profile Visibility | ✅ |

---

## Phase 1-3.7: Foundation ✅ COMPLETE

All base protocol work completed in previous versions.

---

## Phase 3.8: Agent Protocol ✅ COMPLETE

**Goal**: Decode full message layer stack

### Deliverables

| Task | Status |
|------|--------|
| URL Encoding Fix (Base64URL) | ✅ |
| Layer 3 Decryption (SMP E2E) | ✅ |
| Layer 5 Decryption (Client DH) | ✅ |
| Layer 6 Parsing (Agent Protocol) | ✅ |
| AgentInvitation ('I') Parser | ✅ |
| Reply Queue URI Extraction | ✅ |
| Peer Profile (ConnInfo) | ✅ |

### Message Layer Stack (Complete)

```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 1: TLS 1.3 Transport                                     │
│  └── ALPN: "smp/1", ChaCha20-Poly1305                          │
├─────────────────────────────────────────────────────────────────┤
│  Layer 2: SMP Transport Block                                   │
│  └── [2-byte transmissionLength] [content] [padding to 16KB]   │
├─────────────────────────────────────────────────────────────────┤
│  Layer 3: SMP E2E Encryption                                    │
│  └── crypto_box(msg, nonce, server_dh_pub, our_dh_secret)      │
│  └── Nonce: 24 bytes, Tag: 16 bytes                            │
├─────────────────────────────────────────────────────────────────┤
│  Layer 4: SMP Client Message                                    │
│  └── [2-byte length prefix] [encrypted_content] [padding]      │
├─────────────────────────────────────────────────────────────────┤
│  Layer 5: Contact DH Encryption (Initial Messages)              │
│  └── [X25519 SPKI key (44 bytes)] [crypto_box encrypted body]  │
│  └── crypto_box(body, nonce, sender_dh_pub, contact_dh_secret) │
├─────────────────────────────────────────────────────────────────┤
│  Layer 6: Agent Protocol Message                                │
│  └── [2-byte version BE] [type: 'C'/'I'/'M'/'R'] [body]        │
└─────────────────────────────────────────────────────────────────┘
```

---

## Phase 3.9: Connection Complete 📋 NEXT

**Goal**: Complete bidirectional connection with SimpleX apps

**Target**: January 2026

### Deliverables

| Task | Status | Priority |
|------|--------|----------|
| Reply Queue URI Parser | 📋 | Critical |
| Multi-Server Support | 📋 | Critical |
| AgentConfirmation Builder | 📋 | Critical |
| SEND to Peer's Queue | 📋 | Critical |
| Connection Established | 📋 | Critical |
| SimpleX App shows "Connected" | 📋 | Critical |

### Connection Flow (Contact Address q=c)

```
┌──────────┐                              ┌──────────┐
│ SimpleX  │                              │  ESP32   │
│   App    │                              │(Contact) │
└────┬─────┘                              └────┬─────┘
     │  1. Scannt Contact Link                 │
     │  2. SEND AgentInvitation ───────────────>  (Reply Queue + Profile)
     │  3. Wartet auf Accept...                │
     │     <───────────── AgentConfirmation    │  (Zu App's Reply Queue!)
     │  4. "Connected!"                        │
```

### Alternative Flow (Invitation Link q=i)

```
┌──────────┐                              ┌──────────┐
│ SimpleX  │                              │  ESP32   │
│   App    │                              │(Inviter)│
└────┬─────┘                              └────┬─────┘
     │  1. Scannt Invitation Link              │
     │  2. SEND AgentConfirmation ─────────────>  (Direct confirmation)
     │  3. "Connected!"                        │
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
| `'C'` | AgentConfirmation | Critical (Phase 3.9) |
| `'M'` | AgentMsgEnvelope | High (Phase 5) |
| `'R'` | AgentRatchetKey | High (Phase 5) |

### Deliverables

| Task | Status |
|------|--------|
| X3DH Key Agreement | 📋 |
| Double Ratchet Algorithm | 📋 |
| AgentMsgEnvelope ('M') Decrypt | 📋 |
| AgentRatchetKey ('R') Handle | 📋 |
| Curve448 Support | 📋 |

---

## Timeline Summary

```
2026 Q1
├── January   ✅ Phase 1-3.8 Complete!
│             ├── Protocol Foundation
│             ├── Full Messaging
│             ├── E2E Encryption
│             ├── Multi-Contact
│             ├── Invitation Links
│             └── Agent Protocol (v0.1.12)
├── January   📋 Phase 3.9 (Connection Complete)
├── February  📋 Phase 4 Start (UI)
└── March     📋 Phase 4 Continue

2026 Q2
├── April     📋 Phase 4 Complete
├── May       📋 Phase 5 Start (Double Ratchet)
└── June      📋 Beta Release

2026 Q3-Q4
└── December  🎯 Version 1.0 Release
```

---

## Current Priorities

### Immediate (v0.1.13)

1. **Reply Queue URI Parser** — Extract server, queue ID, DH key
2. **Multi-Server Support** — Connect to peer's SMP server (e.g., smp10.simplex.im)
3. **AgentConfirmation Builder** — Create response message
4. **SEND to Peer** — Complete connection handshake

### Short-term

5. T-Embed UI — Display + Rotary Encoder
6. QR Code Display — Show invitation as QR
7. Connection Recovery — Auto-reconnect

### Medium-term

8. Double Ratchet (Curve448)
9. Group Messaging

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.12-alpha** | **2026-01-21** | **🔐 Agent Protocol!** |
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
- [LilyGo T-Embed](https://github.com/Xinyuan-LilyGO/T-Embed)
