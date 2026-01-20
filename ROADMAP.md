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
│  Phase 4: User Interface          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 5: Advanced Features       ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🏆 MILESTONE: Multi-Contact + E2E Complete!

As of v0.1.10-alpha, full multi-contact support with E2E encryption:

| Feature | Status |
|---------|--------|
| Multiple Contacts (10 slots) | ✅ |
| One TLS Connection | ✅ |
| NVS Persistence | ✅ |
| Batch Subscribe | ✅ |
| Message Routing | ✅ |
| E2E Decryption | ✅ |
| Self-Test | ✅ |

---

## Phase 1: Protocol Foundation ✅ COMPLETE

**Goal**: Establish reliable SMP server communication

### Deliverables

| Task | Status |
|------|--------|
| WiFi Connectivity | ✅ |
| TLS 1.3 (ChaCha20-Poly1305) | ✅ |
| ALPN "smp/1" | ✅ |
| SMP Handshake | ✅ |
| Certificate Chain Parsing | ✅ |
| keyHash from CA Certificate | ✅ |
| Ed25519 Signatures (libsodium) | ✅ |
| Transport Blocks (16KB) | ✅ |

---

## Phase 2: Full Messaging ✅ COMPLETE

**Goal**: Complete bidirectional message exchange

### Deliverables

| Task | Status |
|------|--------|
| NEW Command | ✅ |
| SUB Command | ✅ |
| SEND Command | ✅ |
| MSG Receive | ✅ |
| ACK Command | ✅ |
| DEL Command | ✅ |

---

## Phase 3: E2E Encryption ✅ COMPLETE

**Goal**: Transport-level E2E encryption

### Deliverables

| Task | Status |
|------|--------|
| X25519 Key Exchange | ✅ |
| Server DH Key Storage | ✅ |
| crypto_box Decryption | ✅ |
| HSalsa20 Key Derivation | ✅ |

### Key Discovery

```c
// WRONG: Raw X25519
crypto_scalarmult(shared, secret, public);

// CORRECT: crypto_box does HSalsa20 key derivation
crypto_box_beforenm(shared, public, secret);
```

---

## Phase 3.5: Persistence ✅ COMPLETE

**Goal**: Keys survive reboots

### Deliverables

| Task | Status |
|------|--------|
| NVS Storage | ✅ |
| Queue Reconnect | ✅ |
| Key Management Functions | ✅ |

---

## Phase 3.6: Multi-Contact ✅ COMPLETE

**Goal**: Multiple contacts over one connection

### Deliverables

| Task | Status |
|------|--------|
| contacts_db_t Structure | ✅ |
| add_contact() | ✅ |
| remove_contact() | ✅ |
| list_contacts() | ✅ |
| subscribe_all_contacts() | ✅ |
| find_contact_by_recipient_id() | ✅ |
| NVS Blob Storage | ✅ |
| Self-Test (E2E Round-Trip) | ✅ |

### Data Structure

```c
typedef struct {
    char name[32];
    uint8_t rcv_auth_secret[64];
    uint8_t rcv_auth_public[32];
    uint8_t rcv_dh_secret[32];
    uint8_t rcv_dh_public[32];
    uint8_t recipient_id[24];
    uint8_t sender_id[24];
    uint8_t srv_dh_public[32];
    // ... lengths and flags
} contact_t;

typedef struct {
    uint8_t num_contacts;
    contact_t contacts[MAX_CONTACTS];  // 10 slots
} contacts_db_t;
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
| Rotary Encoder (T-Embed) | 📋 | High |
| Main Screen | 📋 | High |
| Contact List View | 📋 | High |
| Message View | 📋 | High |
| Compose Screen | 📋 | High |
| Keyboard Driver (T-Deck) | 📋 | High |
| Settings Menu | 📋 | Medium |
| Status Bar | 📋 | Medium |

### T-Deck Hardware

```
Display: 2.8" IPS LCD (320x240), ST7789
Keyboard: Physical QWERTY (I2C)
Trackball: Navigation
```

### T-Embed Hardware

```
Display: 1.9" LCD (170x320), ST7789
Input: Rotary Encoder with button
```

---

## Phase 5: Advanced Features 📋 FUTURE

**Goal**: Extended functionality for production use

**Target**: Q3-Q4 2026

### 5.1 Double Ratchet (Agent-Level E2E)

| Component | Status |
|-----------|--------|
| X3DH Key Agreement | 📋 |
| Double Ratchet Algorithm | 📋 |
| Curve448 Support | 📋 |

### 5.2 Advanced Features

| Feature | Priority |
|---------|----------|
| Multiple Servers | High |
| Bidirectional Chat | High |
| Group Messaging | Medium |
| File Transfer (XFTP) | Medium |
| 4G/LTE Support | High |
| Tor Integration | Low |

### Prioritization Matrix

| Feature | Impact | Effort | Priority |
|---------|--------|--------|----------|
| T-Embed UI | High | Medium | **Critical** |
| Bidirectional Chat | High | Low | **High** |
| Multiple Servers | High | Medium | High |
| Double Ratchet | High | High | Medium |
| Group Messaging | High | High | Medium |
| 4G Connectivity | High | Medium | High |

---

## Timeline Summary

```
2026 Q1
├── January   ✅ Phase 1-3.6 Complete!
│             ├── Protocol Foundation
│             ├── Full Messaging
│             ├── E2E Encryption
│             ├── NVS Persistence
│             └── Multi-Contact (v0.1.10)
├── February  📋 Phase 4 Start (T-Embed UI)
└── March     📋 Phase 4 Continue

2026 Q2
├── April     📋 Phase 4 Continue
├── May       📋 Phase 4 Complete
└── June      📋 Beta Release

2026 Q3-Q4
├── July+     📋 Phase 5 (Advanced Features)
└── December  🎯 Version 1.0 Release
```

---

## Current Priorities

### Immediate (Next)

1. **T-Embed UI** — Display + Rotary Encoder
2. **Bidirectional Chat** — Two queues per contact
3. **Contact Naming UI** — User-friendly management

### Short-term

4. Multiple Servers — Contact on different SMP servers
5. Connection Recovery — Auto-reconnect
6. T-Deck Keyboard Support

### Medium-term

7. Double Ratchet (Curve448)
8. Group Messaging

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.10-alpha** | **2026-01-20** | **🏆 Multi-Contact + E2E!** |
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

## Contributing

### Current Priorities

1. **LVGL UI Development** — Embedded graphics experience
2. **Double Ratchet Port** — Cryptography expertise
3. **Documentation** — Protocol analysis

---

## References

- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [LVGL Documentation](https://docs.lvgl.io/)
- [LilyGo T-Embed](https://github.com/Xinyuan-LilyGO/T-Embed)
- [LilyGo T-Deck](https://github.com/Xinyuan-LilyGO/T-Deck)
