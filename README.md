# SimpleGo

> **The First Native Multi-Contact SimpleX SMP Client for ESP32** — Part of the Sentinel Secure Messenger Suite

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework: ESP-IDF 5.5](https://img.shields.io/badge/Framework-ESP--IDF%205.5-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Version: v0.1.10-alpha](https://img.shields.io/badge/Version-v0.1.10--alpha-orange.svg)]()
[![Status: Multi-Contact E2E](https://img.shields.io/badge/Status-Multi--Contact%20E2E-brightgreen.svg)]()

---

## 🎯 Vision

SimpleGo brings [SimpleX Chat](https://simplex.chat/) — the first messaging platform without user identifiers — to standalone hardware devices. No smartphone required, no cloud dependency, complete privacy in your pocket.

---

## 🏆 MILESTONE: Multi-Contact + E2E Encryption!

**As of v0.1.10-alpha (January 20, 2026)**, SimpleGo supports multiple contacts with full E2E encryption:

```
📡 Subscriptions complete: 2/2
🧪 SELF-TEST: Sending message to [0] Test...
📤 SEND command sent!
💬 MESSAGE for [Test]!
🔓 DECRYPTED: Hello from ESP32!
✅ OK
```

**First native ESP32 multi-contact SimpleX client with working E2E encryption!** 🎉

---

## 🎯 What is SimpleGo?

SimpleGo is a **groundbreaking open-source project** that implements a native [SimpleX Messaging Protocol (SMP)](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md) client for ESP32 microcontrollers. This is the **first known implementation** of the SimpleX protocol outside of the official Haskell codebase.

**Why is this significant?**

All existing SimpleX clients (mobile apps, desktop, CLI) use the Haskell core library via FFI. SimpleGo implements the protocol **from scratch in C**, enabling:

- 📱 **Smartphone-free messaging** — No dependency on mobile devices
- 🔒 **Hardware-level privacy** — Dedicated secure communication device
- 🌐 **Offline-first design** — Store-and-forward with local encryption
- 🔧 **Full protocol control** — No black-box dependencies

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    SimpleGo Client                      │
├─────────────────────────────────────────────────────────┤
│  UI Layer                              📋 PLANNED       │
│  └── OLED/LCD Display (LVGL planned)                    │
├─────────────────────────────────────────────────────────┤
│  Contact Management                    ✅ COMPLETE      │
│  ├── Multi-Contact Database           - 10 slots        │
│  ├── NVS Persistence                  - Survives reboot │
│  └── Message Routing                  - By recipientId  │
├─────────────────────────────────────────────────────────┤
│  Crypto Engine                         ✅ COMPLETE      │
│  ├── Ed25519 (libsodium)              - Signatures      │
│  ├── X25519 (libsodium)               - Key Exchange    │
│  ├── XSalsa20-Poly1305 (libsodium)    - E2E Encryption  │
│  └── SHA-256 (mbedTLS)                - Hashing         │
├─────────────────────────────────────────────────────────┤
│  SMP Protocol Layer                    ✅ COMPLETE      │
│  ├── NEW, SUB, SEND, MSG, ACK, DEL    ✅ All Commands   │
│  ├── TLS 1.3 Transport                ✅ Working        │
│  └── 16KB Block Framing               ✅ Working        │
├─────────────────────────────────────────────────────────┤
│  Network Layer                         ✅ COMPLETE      │
│  ├── WiFi (ESP32)                                       │
│  ├── TLS 1.3 (mbedTLS)                                  │
│  └── Tor (planned)                                      │
└─────────────────────────────────────────────────────────┘
```

---

## ✅ What's Working

### SMP Commands

| Command | Status | Description |
|---------|--------|-------------|
| NEW | ✅ Complete | Queue creation with IDS response |
| SUB | ✅ Complete | Queue subscription (batch for all contacts) |
| SEND | ✅ Complete | Message transmission |
| MSG | ✅ Complete | Message receive + E2E decrypt |
| ACK | ✅ Complete | Message acknowledgment |
| DEL | ✅ Complete | Queue deletion |

### Features

| Feature | Status | Description |
|---------|--------|-------------|
| Multi-Contact | ✅ Complete | Up to 10 contacts, one TLS connection |
| E2E Encryption | ✅ Complete | X25519 DH + XSalsa20-Poly1305 |
| NVS Persistence | ✅ Complete | Contacts survive reboots |
| Message Routing | ✅ Complete | Dispatch by recipientId |
| Self-Test | ✅ Complete | Verify full E2E round-trip |
| TLS 1.3 | ✅ Complete | ChaCha20-Poly1305, ALPN "smp/1" |

### Cryptography

| Feature | Status | Description |
|---------|--------|-------------|
| Ed25519 Signatures | ✅ Complete | libsodium, SPKI encoding |
| X25519 Key Exchange | ✅ Complete | Per-contact DH keys |
| crypto_box | ✅ Complete | HSalsa20 key derivation + XSalsa20-Poly1305 |
| SHA-256 | ✅ Complete | Certificate fingerprints |
| Double Ratchet | 📋 Planned | Full Agent-level E2E |

---

## 🔧 Hardware

### Currently Testing On
- **ESP32-S3 DevKit** — Development board

### Target Hardware

| Device | Status | Features |
|--------|--------|----------|
| **LilyGo T-Deck** | 🎯 Primary | ESP32-S3, 2.8" LCD, Keyboard, 8MB PSRAM |
| **LilyGo T-Embed** | 🎯 Secondary | ESP32-S3, 1.9" LCD, Rotary Encoder |
| **T-Deck Plus** | 📋 Planned | + GPS, 2000mAh Battery |

---

## 📈 Performance (ESP32-S3 @ 240MHz)

| Operation | Time | Library |
|-----------|------|---------|
| Ed25519 Sign | ~8ms | libsodium |
| X25519 DH | ~8ms | libsodium |
| crypto_box decrypt | ~1ms | libsodium |
| TLS Handshake | ~800ms | mbedTLS |
| NVS read/write | ~5ms | ESP-IDF |

---

## 🛠️ Technical Stack

| Component | Technology |
|-----------|------------|
| **MCU** | ESP32-S3 (Dual-core 240MHz, 8MB PSRAM) |
| **Framework** | ESP-IDF 5.5.2 |
| **TLS** | mbedTLS 3.x (TLS 1.3, ChaCha20-Poly1305) |
| **Cryptography** | libsodium (Ed25519, X25519, crypto_box) |
| **Storage** | NVS (Non-volatile key persistence) |
| **Protocol** | SMP v6 |

---

## 📁 Project Structure

```
SimpleGo/
├── README.md                 # This file
├── CHANGELOG.md              # Version history
├── ROADMAP.md                # Development roadmap
├── LICENSE                   # AGPL-3.0
├── main/
│   ├── main.c                # Main application
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── docs/
│   ├── DEVELOPMENT.md        # Build & setup guide
│   ├── PROTOCOL.md           # SMP protocol deep dive
│   ├── TECHNICAL.md          # Key learnings
│   └── DEVNOTES.md           # Session notes
├── CMakeLists.txt
└── sdkconfig.defaults
```

---

## 🚀 Quick Start

### Build & Flash

```powershell
# Windows (ESP-IDF PowerShell)
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5

# Linux/macOS
cd ~/esp/simplex_client
idf.py build flash monitor -p /dev/ttyUSB0
```

### Monitor Commands

| Key | Action |
|-----|--------|
| `Ctrl+]` | Exit monitor |
| `Ctrl+T, R` | Reboot device |

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for detailed setup.

---

## 🗺️ Roadmap

See [ROADMAP.md](ROADMAP.md) for detailed plans.

| Phase | Status |
|-------|--------|
| Phase 1: Protocol Foundation | ✅ Complete |
| Phase 2: Full Messaging | ✅ Complete |
| Phase 3: E2E Encryption | ✅ Complete |
| Phase 3.5: Persistence | ✅ Complete |
| Phase 3.6: Multi-Contact | ✅ **Complete!** |
| Phase 4: User Interface | 📋 Planned |
| Phase 5: Advanced Features | 📋 Future |

---

## 🔐 Security Model

SimpleGo inherits SimpleX's privacy-first design:

1. **No User Identifiers** — No phone numbers, usernames, or accounts
2. **No Central Directory** — No server stores your contact list
3. **Forward Secrecy** — Per-contact key isolation
4. **Metadata Protection** — Servers can't correlate senders and recipients

---

## 🤝 Contributing

1. **Read the docs** — [DEVELOPMENT.md](docs/DEVELOPMENT.md), [PROTOCOL.md](docs/PROTOCOL.md)
2. **Check issues** — Look for `good first issue` labels
3. **Fork & PR** — Standard GitHub workflow

### Current Priorities

1. **T-Embed UI** — Display + Rotary Encoder
2. **Double Ratchet** — Full Agent-level E2E
3. **Contact Naming UI** — User-friendly management

---

## 📜 License

**GNU Affero General Public License v3.0 (AGPL-3.0)**

---

## 🙏 Acknowledgments

- **[SimpleX Chat](https://simplex.chat/)** — Protocol design and inspiration
- **[Espressif](https://www.espressif.com/)** — ESP32 platform and ESP-IDF
- **[LilyGo](https://lilygo.cc/)** — T-Deck / T-Embed hardware
- **[libsodium](https://libsodium.org/)** — Cryptographic primitives

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.10-alpha** | **2026-01-20** | **🏆 Multi-Contact + E2E!** |
| v0.1.9-alpha | 2026-01-20 | 🗑️ DEL + Full SMP Client |
| v0.1.8-alpha | 2026-01-20 | 🔑 NVS Persistence |
| v0.1.7-alpha | 2026-01-20 | ✅ ACK Command |
| v0.1.6-alpha | 2026-01-20 | 🔐 E2E (Single) |
| v0.1.5-alpha | 2026-01-20 | 📨 SEND + MSG |

---

<p align="center">
  <strong>🏆 First Native ESP32 Multi-Contact SimpleX Client! 🏆</strong><br>
  <em>Privacy is not a privilege, it's a right.</em>
</p>

---

*Copyright (c) 2026 cannatoshi — Part of the Sentinel Secure Messenger Suite*
