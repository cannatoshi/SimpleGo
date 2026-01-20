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
│  Phase 3.5: Persistence           ████████████████████ 100% ✅      │
│  Phase 3.6: Queue Management      ████████████████████ 100% ✅      │
│  Phase 4: User Interface          ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
│  Phase 5: Advanced Features       ░░░░░░░░░░░░░░░░░░░░   0% 📋      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 🏆 MILESTONE: Full Single-Queue SMP Client Complete!

As of v0.1.9-alpha, all base SMP commands are implemented:

| Command | Function | Status |
|---------|----------|--------|
| NEW | Create queue | ✅ |
| SUB | Subscribe to queue | ✅ |
| SEND | Send message | ✅ |
| MSG | Receive + decrypt | ✅ |
| ACK | Acknowledge message | ✅ |
| DEL | Delete queue | ✅ |

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

### Technical Requirements Met

#### SEND Command Structure
```
SEND [msgFlags] [msgBody]
  - msgFlags: 'T' or 'F' (ASCII, NOT binary!)
  - msgBody: message content
```

#### Message Reception
```
MSG [msgId] [timestamp] [msgFlags] [msgBody]
  - Parse incoming MSG responses
  - Extract message content
  - Decrypt with XSalsa20-Poly1305
```

#### ACK Command
```
ACK [msgId]
  - EntityId = recipientId (NOT senderId!)
  - Server removes message from queue
```

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

---

## Phase 3.5: Persistence ✅ COMPLETE

**Goal**: Keys and queue IDs survive reboots

**Status**: ✅ Complete (January 20, 2026)

### Deliverables

| Task | Status | Description |
|------|--------|-------------|
| NVS Storage | ✅ | Non-volatile key persistence |
| Queue Reconnect | ✅ | SUB directly after reboot |
| Key Management | ✅ | have/load/save/clear functions |

### Persisted Data

| Key | Size | Description |
|-----|------|-------------|
| rcv_auth_sk | 64 bytes | Ed25519 Secret Key |
| rcv_auth_pk | 32 bytes | Ed25519 Public Key |
| rcv_dh_sk | 32 bytes | X25519 Secret Key |
| rcv_dh_pk | 32 bytes | X25519 Public Key |
| rcv_id | 24 bytes | Recipient ID |
| snd_id | 24 bytes | Sender ID |
| srv_dh_pk | 32 bytes | Server DH Key |

---

## Phase 3.6: Queue Management ✅ COMPLETE

**Goal**: Full queue lifecycle management

**Status**: ✅ Complete (January 20, 2026)

### Deliverables

| Task | Status | Description |
|------|--------|-------------|
| DEL Command | ✅ | Delete queue from server |
| NVS Auto-Clear | ✅ | Clear local keys after DEL |
| Full SMP Client | ✅ | All base commands implemented |

---

## Phase 4: User Interface 📋 PLANNED

**Goal**: Complete messaging UI for T-Embed/T-Deck hardware

**Status**: Not started

**Target**: Q1-Q2 2026

### Deliverables

| Task | Status | Priority | Description |
|------|--------|----------|-------------|
| Display Driver | 📋 | Critical | ST7789/ST7735 LCD initialization |
| LVGL Integration | 📋 | Critical | Graphics library setup |
| Rotary Encoder | 📋 | High | T-Embed input handling |
| Main Screen | 📋 | High | Connection status, message count |
| Conversation List | 📋 | High | Contact/queue list view |
| Message View | 📋 | High | Chat bubble interface |
| Compose Screen | 📋 | High | Text input with keyboard |
| Keyboard Driver | 📋 | High | T-Deck physical keyboard |
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

### T-Embed Hardware Specs

```
Display:
  - 1.9" LCD (170x320)
  - ST7789 controller

Input:
  - Rotary Encoder with button
  - Compact form factor
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
- [ ] Keyboard/Encoder input works reliably
- [ ] Navigate between screens
- [ ] Compose and send message via UI
- [ ] View received messages
- [ ] Responsive to user input (<100ms)

---

## Phase 5: Advanced Features 📋 FUTURE

**Goal**: Extended functionality for production use

**Status**: Planning

**Target**: Q3-Q4 2026

### 5.1 Double Ratchet (Agent-Level E2E)

```
Components:
  - Identity Key (IK): Long-term Ed25519/X25519
  - Signed Pre-Key (SPK): Medium-term, signed by IK
  - One-Time Pre-Key (OPK): Single-use keys
  
X3DH Key Agreement:
  - Initial key exchange protocol
  - Output: Shared secret for Double Ratchet initialization

Double Ratchet Algorithm:
  1. DH Ratchet: New key exchange per message chain
  2. Symmetric Ratchet: Derive new keys per message
  
Properties:
  - Forward Secrecy: Past messages secure if key compromised
  - Break-in Recovery: Future messages secure after compromise
```

### 5.2 Multiple Queues / Contact Management

```
- Multiple queue handling
- Contact storage in NVS
- Queue-to-contact mapping
- Contact list UI
```

### 5.3 Group Messaging

```
- Group queue management
- Member key distribution
- Group admin functions
```

### 5.4 File Transfer

```
- XFTP protocol integration
- Chunked file transfer
- Progress indication
```

### 5.5 Connectivity Options

```
- 4G/LTE modem support (SIM7600)
- WiFi mesh networking
- LoRa peer-to-peer (local)
```

### 5.6 Tor Integration

```
- Optional Tor proxy
- .onion SMP servers
- Enhanced metadata protection
```

### 5.7 Multi-Device Sync

```
- Linked device protocol
- Message synchronization
- Key sharing mechanism
```

### 5.8 Hardware Security

```
- Secure boot
- Flash encryption
- Hardware key storage (if available)
```

### Prioritization Matrix

| Feature | Impact | Effort | Priority |
|---------|--------|--------|----------|
| Multiple Queues | High | Medium | **High** |
| Double Ratchet | High | High | Medium |
| Group Messaging | High | High | Medium |
| File Transfer | Medium | Medium | Medium |
| 4G Connectivity | High | Medium | High |
| Tor Support | Medium | High | Low |
| Multi-Device | High | Very High | Low |
| Hardware Security | High | Medium | High |

---

## Timeline Summary

```
2026 Q1
├── January   ✅ Phase 1-3.6 Complete!
│             ├── Protocol Foundation
│             ├── Full Messaging (SEND, MSG, ACK)
│             ├── E2E Encryption
│             ├── NVS Persistence
│             └── Queue Management (DEL)
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

1. **Multiple Queues** — Handle multiple contacts
2. **T-Embed UI** — Display + Rotary Encoder
3. **Contact Management** — Save/load contacts

### Short-term

4. WiFi Config in NVS
5. Connection Recovery
6. T-Deck Keyboard Support

### Medium-term

7. Double Ratchet (Curve448)
8. Group Messaging

---

## Contributing to Roadmap

### How to Propose Features

1. **Open an Issue**: Describe the feature and use case
2. **Discussion**: Community feedback and prioritization
3. **RFC (if major)**: Formal proposal for significant changes
4. **Implementation**: PR with tests and documentation

### Current Priorities

Looking for contributors in these areas:

1. **Multiple Queue Support** — Immediate need
2. **Double Ratchet Port** — Cryptography expertise needed
3. **LVGL UI Development** — Embedded graphics experience
4. **Documentation** — Protocol analysis and guides

---

## References

- [SimpleX Messaging Protocol](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [Double Ratchet Algorithm](https://signal.org/docs/specifications/doubleratchet/)
- [X3DH Key Agreement](https://signal.org/docs/specifications/x3dh/)
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [LVGL Documentation](https://docs.lvgl.io/)
- [LilyGo T-Embed](https://github.com/Xinyuan-LilyGO/T-Embed)
- [LilyGo T-Deck](https://github.com/Xinyuan-LilyGO/T-Deck)
