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
│  Phase 2: Full Messaging          ████████░░░░░░░░░░░░  40% 🔄      │
│  Phase 3: E2E Encryption          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 4: User Interface          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 5: Advanced Features       ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Phase 1: Protocol Foundation ✅ COMPLETE

**Goal**: Establish reliable SMP server communication

**Status**: ✅ Complete (January 2025)

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

## Phase 2: Full Messaging 🔄 IN PROGRESS

**Goal**: Complete bidirectional message exchange

**Status**: 40% complete

**Target**: February 2025

### Deliverables

| Task | Status | Priority | Description |
|------|--------|----------|-------------|
| SEND Command | 🔄 | High | Transmit messages to queues |
| Message Reception | 📋 | High | Receive MSG from subscribed queues |
| ACK Command | 📋 | High | Acknowledge message delivery |
| Error Handling | 📋 | Medium | Graceful error recovery |
| Connection Keepalive | 📋 | Medium | Prevent timeout disconnects |
| Multiple Queues | 📋 | Low | Manage multiple conversations |

### Technical Requirements

#### SEND Command Structure
```
SEND [msgFlags] [msgBody]
  - msgFlags: notification flags
  - msgBody: encrypted message content
```

#### Message Reception
```
MSG [msgId] [timestamp] [msgFlags] [msgBody]
  - Parse incoming MSG responses
  - Extract message content
  - Queue for processing
```

#### ACK Command
```
ACK [msgId]
  - Confirm message receipt
  - Server removes from queue
```

### Success Criteria

- [ ] Send plaintext message to queue
- [ ] Receive message from subscribed queue
- [ ] Acknowledge received messages
- [ ] Handle connection drops gracefully
- [ ] Maintain persistent session

---

## Phase 3: End-to-End Encryption 📋 PLANNED

**Goal**: Implement Double Ratchet for message encryption

**Status**: Not started

**Target**: March 2025

### Deliverables

| Task | Status | Priority | Description |
|------|--------|----------|-------------|
| X3DH Key Agreement | 📋 | Critical | Initial key exchange |
| Double Ratchet | 📋 | Critical | Message encryption/decryption |
| Key Storage | 📋 | Critical | Secure NVS key management |
| Ratchet State | 📋 | High | Persistent ratchet state |
| Forward Secrecy | 📋 | High | Key rotation after each message |

### Technical Requirements

#### X3DH (Extended Triple Diffie-Hellman)
```
Components:
  - Identity Key (IK): Long-term Ed25519/X25519
  - Signed Pre-Key (SPK): Medium-term, signed by IK
  - One-Time Pre-Key (OPK): Single-use keys
  
Output:
  - Shared secret for Double Ratchet initialization
```

#### Double Ratchet Algorithm
```
Ratchet Types:
  1. DH Ratchet: New key exchange per message chain
  2. Symmetric Ratchet: Derive new keys per message
  
Properties:
  - Forward Secrecy: Past messages secure if key compromised
  - Break-in Recovery: Future messages secure after compromise
```

#### Key Storage (NVS)
```
Stored Keys:
  - Identity keypair
  - Current ratchet state
  - Message keys (limited window)
  - Contact public keys
```

### Dependencies

- Phase 2 complete (messaging working)
- Curve448 support (optional, for stronger security)
- Secure random number generation (ESP32 hardware RNG)

### Success Criteria

- [ ] Generate and exchange X3DH keys
- [ ] Initialize Double Ratchet from shared secret
- [ ] Encrypt outgoing messages
- [ ] Decrypt incoming messages
- [ ] Persist ratchet state across reboots
- [ ] Handle out-of-order messages

---

## Phase 4: User Interface 📋 PLANNED

**Goal**: Complete messaging UI for T-Deck hardware

**Status**: Not started

**Target**: April-May 2025

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

### Screen Mockups

```
┌────────────────────────┐
│ ◉ SimpleGo    ▂▄▆█ 85%│  ← Status bar
├────────────────────────┤
│                        │
│   ┌──────────────┐     │
│   │ 🔒 Connected │     │  ← Main status
│   │   to SMP3    │     │
│   └──────────────┘     │
│                        │
│  Conversations: 3      │
│  Unread: 2             │
│                        │
│  [Enter] Open          │
│  [Menu] Settings       │
│                        │
└────────────────────────┘
```

### Success Criteria

- [ ] Display initializes correctly
- [ ] LVGL renders without artifacts
- [ ] Keyboard input works reliably
- [ ] Navigate between screens
- [ ] Compose and send message via UI
- [ ] View received messages
- [ ] Responsive to user input (<100ms)

---

## Phase 5: Advanced Features 📋 FUTURE

**Goal**: Extended functionality for production use

**Status**: Planning

**Target**: Q3-Q4 2025

### Feature Set

#### 5.1 Group Messaging
```
- Group queue management
- Member key distribution
- Group admin functions
```

#### 5.2 File Transfer
```
- XFTP protocol integration
- Chunked file transfer
- Progress indication
```

#### 5.3 Connectivity Options
```
- 4G/LTE modem support (SIM7600)
- WiFi mesh networking
- LoRa peer-to-peer (local)
```

#### 5.4 Tor Integration
```
- Optional Tor proxy
- .onion SMP servers
- Enhanced metadata protection
```

#### 5.5 Multi-Device Sync
```
- Linked device protocol
- Message synchronization
- Key sharing mechanism
```

#### 5.6 Hardware Security
```
- Secure boot
- Flash encryption
- Hardware key storage (if available)
```

### Prioritization Matrix

| Feature | Impact | Effort | Priority |
|---------|--------|--------|----------|
| Group Messaging | High | High | Medium |
| File Transfer | Medium | Medium | Medium |
| 4G Connectivity | High | Medium | High |
| Tor Support | Medium | High | Low |
| Multi-Device | High | Very High | Low |
| Hardware Security | High | Medium | High |

---

## Timeline Summary

```
2025 Q1
├── January   ✅ Phase 1 Complete (Protocol Foundation)
├── February  🔄 Phase 2 (Full Messaging)
└── March     📋 Phase 3 (E2E Encryption)

2025 Q2
├── April     📋 Phase 4 Start (UI Development)
├── May       📋 Phase 4 Continue
└── June      📋 Phase 4 Complete + Beta Release

2025 Q3-Q4
├── July+     📋 Phase 5 (Advanced Features)
└── December  🎯 Version 1.0 Release
```

---

## Contributing to Roadmap

### How to Propose Features

1. **Open an Issue**: Describe the feature and use case
2. **Discussion**: Community feedback and prioritization
3. **RFC (if major)**: Formal proposal for significant changes
4. **Implementation**: PR with tests and documentation

### Current Priorities

Looking for contributors in these areas:

1. **SEND Command Implementation** — Immediate need
2. **Double Ratchet Port** — Cryptography expertise needed
3. **LVGL UI Development** — Embedded graphics experience
4. **Documentation** — Protocol analysis and guides

---

## References

- [SimpleX Messaging Protocol](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [Double Ratchet Algorithm](https://signal.org/docs/specifications/doubleratchet/)
- [X3DH Key Agreement](https://signal.org/docs/specifications/x3dh/)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [LVGL Documentation](https://docs.lvgl.io/)
