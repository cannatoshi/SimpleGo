# SimpleGo Development Roadmap

> Strategic development plan for the first native SimpleX SMP client on ESP32

---

## Overview

SimpleGo development follows a phased approach, building from protocol fundamentals to a complete standalone messaging device. Each phase has clear deliverables and success criteria.

```
┌─────────────────────────────────────────────────────────────────────┐
│                        DEVELOPMENT PHASES                           │
├─────────────────────────────────────────────────────────────────────┤
│  Phase 1: Protocol Foundation     ████████████████████ 100% ✅      │
│  Phase 2: Full Messaging          ████████████████████ 100% ✅      │
│  Phase 3: E2E Encryption          ████████████████████ 100% ✅      │
│  Phase 4: User Interface          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 5: Advanced Features       ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Protocol Foundation ✅ COMPLETE

**Goal**: Establish reliable SMP server communication

**Status**: ✅ Complete (January 2026)

### Deliverables

| Task | Status | Description |
|------|--------|-------------|
| WiFi Connectivity | ✅ | ESP32 WiFi station mode |
| TCP Socket Layer | ✅ | Connection management, timeouts |
| TLS 1.3 | ✅ | ChaCha20-Poly1305, ALPN "smp/1" |
| SMP Handshake | ✅ | ServerHello/ClientHello exchange |
| Certificate Handling | ✅ | Chain parsing, keyHash computation |
| Transport Blocks | ✅ | 16KB padded blocks, content framing |
| Ed25519 Signatures | ✅ | libsodium integration, SPKI encoding |
| NEW Command | ✅ | Queue creation with IDS response |
| SUB Command | ✅ | Queue subscription |

### Key Achievements

- First working native SMP client outside Haskell
- Full TLS 1.3 compliance with SimpleX servers
- Correct cryptographic signature generation
- Protocol-compliant message framing

### Critical Discoveries

1. **keyHash Calculation**: Must use CA certificate (2nd in chain), not server certificate
2. **Ed25519 Compatibility**: Monocypher incompatible with SimpleX; must use libsodium
3. **Block Format**: Commands require different format than handshake messages
4. **SubMode Parameter**: Required for SMP v6 NEW command

---

## Phase 2: Full Messaging ✅ COMPLETE

**Goal**: Complete bidirectional message exchange

**Status**: ✅ Complete (January 20, 2026)

### Deliverables

| Task | Status | Description |
|------|--------|-------------|
| SEND Command | ✅ | Transmit messages to queues |
| Message Reception | ✅ | Receive MSG from subscribed queues |
| ACK Command | ✅ | Acknowledge message delivery |
| OK Response Handling | ✅ | Command confirmations |
| Error Handling | ✅ | Basic error recovery |
| Connection Keepalive | 📋 | Prevent timeout disconnects |
| Multiple Queues | 📋 | Manage multiple conversations |

### Technical Details

#### SEND Command Structure
```
SEND [msgFlags] [msgBody]
  - msgFlags: 'T' or 'F' (ASCII, not binary!)
  - msgBody: encrypted message content
```

#### ACK Command Structure (v0.1.7)
```
ACK [msgId]
  - EntityId: recipientId (NOT senderId!)
  - Requires signature with rcv_auth_secret
  - Server responds with OK
```

### Success Criteria ✅

- [x] Send plaintext message to queue
- [x] Receive message from subscribed queue
- [x] Acknowledge received messages
- [x] Handle OK responses
- [ ] Handle connection drops gracefully (partial)
- [ ] Maintain persistent session (partial)

---

## Phase 3: End-to-End Encryption ✅ COMPLETE

**Goal**: Implement transport-level E2E encryption

**Status**: ✅ Complete (January 20, 2026)

### Deliverables

| Task | Status | Description |
|------|--------|-------------|
| X25519 Key Exchange | ✅ | DH shared secret computation |
| XSalsa20-Poly1305 | ✅ | Message encryption/decryption |
| Nonce Handling | ✅ | msgId as nonce (zero-padded) |
| Server DH Key | ✅ | Extract from IDS response |
| Full Round-Trip | ✅ | SEND→MSG→Decrypt→ACK |

### Technical Implementation

```c
// DH Shared Secret
crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);

// Nonce = msgId (24 bytes, zero-padded)
uint8_t nonce[24] = {0};
memcpy(nonce, msg_id, msgIdLen);

// Decrypt
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

### Future: Double Ratchet (Phase 5)

The current implementation uses transport-level encryption. Full Agent-level E2E with Double Ratchet (Curve448) is planned for Phase 5.

---

## Phase 4: User Interface 📋 PLANNED

**Goal**: Complete messaging UI for T-Deck hardware

**Status**: Not started

**Target**: Q2 2026

### Deliverables

| Task | Status | Priority | Description |
|------|--------|----------|-------------|
| Display Driver | 📋 | Critical | ST7789 LCD initialization |
| LVGL Integration | 📋 | Critical | Graphics library setup |
| Main Screen | 📋 | High | Connection status, message count |
| Conversation List | 📋 | High | Contact/queue list view |
| Message View | 📋 | High | Chat bubble interface |
| Compose Screen | 📋 | High | Text input with keyboard |
| Keyboard Driver | 📋 | High | Physical keyboard input |
| Settings Menu | 📋 | Medium | WiFi, server config |
| Status Bar | 📋 | Medium | Signal, battery, time |

### T-Deck Hardware Specs

```
Display:
  - 2.8" IPS LCD (320x240)
  - ST7789 controller
  - SPI interface

Keyboard:
  - Physical QWERTY
  - I2C interface
  - Backlight control

Additional:
  - Trackball navigation
  - Speaker/microphone
  - LoRa module (SX1262)
  - GPS module (optional)
```

### UI Design Principles

1. **SimpleX-Style Interface**: Familiar to SimpleX users
2. **High Contrast**: Readable in various lighting
3. **Minimal Animations**: Performance over polish
4. **Keyboard-First**: Optimized for physical input

---

## Phase 5: Advanced Features 📋 FUTURE

**Goal**: Extended functionality for production use

**Status**: Planning

**Target**: Q3-Q4 2026

### Feature Set

#### 5.1 Key Persistence & Queue Recovery
```
- NVS storage for keys
- Queue reconnect after reboot
- Contact management
```

#### 5.2 Double Ratchet (Agent-Level E2E)
```
- X3DH key agreement
- Curve448 support
- Forward secrecy per message
```

#### 5.3 Group Messaging
```
- Group queue management
- Member key distribution
- Group admin functions
```

#### 5.4 File Transfer
```
- XFTP protocol integration
- Chunked file transfer
- Progress indication
```

#### 5.5 Connectivity Options
```
- 4G/LTE modem support (SIM7600)
- WiFi mesh networking
- LoRa peer-to-peer (local)
```

#### 5.6 Tor Integration
```
- Optional Tor proxy
- .onion SMP servers
- Enhanced metadata protection
```

### Prioritization Matrix

| Feature | Impact | Effort | Priority |
|---------|--------|--------|----------|
| Key Persistence | High | Low | **Immediate** |
| Double Ratchet | High | High | Medium |
| Group Messaging | High | High | Medium |
| File Transfer | Medium | Medium | Medium |
| 4G Connectivity | High | Medium | High |
| Tor Support | Medium | High | Low |

---

## Timeline Summary

```
2026 Q1
├── January   ✅ Phase 1-3 Complete!
│             ├── Protocol Foundation
│             ├── Full Messaging (SEND, MSG, ACK)
│             └── E2E Encryption
├── February  📋 Key Persistence + Queue Recovery
└── March     📋 Phase 4 Start (UI Development)

2026 Q2
├── April     📋 Phase 4 Continue
├── May       📋 Phase 4 Continue
└── June      📋 Phase 4 Complete + Beta Release

2026 Q3-Q4
├── July+     📋 Phase 5 (Advanced Features)
└── December  🎯 Version 1.0 Release
```

---

## SMP Protocol Version Strategy

### Current: v6

v6 has **everything** needed for a complete messenger:
- ✅ Queue management (NEW, SUB, DEL)
- ✅ Message sending (SEND)
- ✅ Message receiving (MSG)
- ✅ Acknowledgment (ACK)
- ✅ E2E encryption (X25519 + XSalsa20-Poly1305)

### What v7+ adds (not critical for MVP):
- `implySessId` — sessionId not sent in transmission (optimization)
- `authEncryptCmds` — Commands encrypted with X25519 DH (extra security)
- Batch commands — Performance optimization

### Upgrade Path
```
v6 (now) ────────────────► v17 (future)
          skip v7-v16
```

When stable, upgrade directly to latest version for optimizations.

---

## Contributing to Roadmap

### How to Propose Features

1. **Open an Issue**: Describe the feature and use case
2. **Discussion**: Community feedback and prioritization
3. **RFC (if major)**: Formal proposal for significant changes
4. **Implementation**: PR with tests and documentation

### Current Priorities

Looking for contributors in these areas:

1. **Key Persistence (NVS)** — Immediate need
2. **LVGL UI Development** — Embedded graphics experience
3. **Double Ratchet Port** — Cryptography expertise needed
4. **Documentation** — Protocol analysis and guides

---

## References

- [SimpleX Messaging Protocol](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [Double Ratchet Algorithm](https://signal.org/docs/specifications/doubleratchet/)
- [X3DH Key Agreement](https://signal.org/docs/specifications/x3dh/)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [LVGL Documentation](https://docs.lvgl.io/)
