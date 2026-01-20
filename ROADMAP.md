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

### New Flow

```
Start
  │
  ▼
TLS + Handshake
  │
  ▼
load_keys_from_nvs()
  │
  ├── Keys found? ──► Skip NEW ──► SUB directly
  │
  └── No keys? ──► NEW ──► save_keys_to_nvs() ──► SUB
```

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
| Message View | 📋 | High | Chat interface |
| Keyboard Support | 📋 | Medium | T-Deck physical keyboard |
| Settings Menu | 📋 | Medium | WiFi, server config |

### Target Hardware

**T-Embed (Primary):**
- 1.9" LCD (170x320)
- Rotary Encoder
- Compact form factor

**T-Deck (Secondary):**
- 2.8" LCD (320x240)
- Physical QWERTY keyboard
- LoRa module

---

## Phase 5: Advanced Features 📋 FUTURE

**Goal**: Extended functionality for production use

**Status**: Planning

**Target**: Q3-Q4 2026

### Feature Set

| Feature | Priority | Description |
|---------|----------|-------------|
| DEL Command | High | Delete queues |
| Multiple Queues | High | Contact management |
| Double Ratchet | Medium | Agent-level E2E (Curve448) |
| WiFi Config | Medium | Credentials in NVS |
| Connection Recovery | Medium | Auto-reconnect |
| Group Messaging | Low | Group queues |
| File Transfer | Low | XFTP integration |

---

## Timeline Summary

```
2026 Q1
├── January   ✅ Phase 1-3.5 Complete!
│             ├── Protocol Foundation
│             ├── Full Messaging (SEND, MSG, ACK)
│             ├── E2E Encryption
│             └── NVS Persistence
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
2. **DEL Command** — Queue cleanup
3. **WiFi Config** — Store credentials in NVS

### Short-term

4. Multiple Queues
5. Connection Recovery
6. T-Deck Keyboard Support

---

## References

- [SimpleX Messaging Protocol](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [LVGL Documentation](https://docs.lvgl.io/)
- [LilyGo T-Embed](https://github.com/Xinyuan-LilyGO/T-Embed)
