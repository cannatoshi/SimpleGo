# SimpleGo

> **The First Complete Native SimpleX SMP Client for ESP32** — Part of the Sentinel Secure Messenger Suite

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework: ESP-IDF 5.5](https://img.shields.io/badge/Framework-ESP--IDF%205.5-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Version: v0.1.9-alpha](https://img.shields.io/badge/Version-v0.1.9--alpha-orange.svg)]()
[![Status: Full SMP Client](https://img.shields.io/badge/Status-Full%20SMP%20Client-brightgreen.svg)]()

---

## 🎯 Vision

SimpleGo brings [SimpleX Chat](https://simplex.chat/) — the first messaging platform without user identifiers — to standalone hardware devices. No smartphone required, no cloud dependency, complete privacy in your pocket.

---

## 🏆 MILESTONE: Full Single-Queue SMP Client!

**As of v0.1.9-alpha (January 20, 2026)**, SimpleGo implements all base SMP commands:

| Command | Function | Status |
|---------|----------|--------|
| NEW | Create queue | ✅ |
| SUB | Subscribe to queue | ✅ |
| SEND | Send message | ✅ |
| MSG | Receive + decrypt | ✅ |
| ACK | Acknowledge message | ✅ |
| DEL | Delete queue | ✅ |

```
🗑️ Queue Deletion:
I (187810) SMP:   🗑️ Deleting queue...
I (188170) SMP:   ✅ Queue deleted from server!
I (188190) SMP:   ✅ NVS cleared!
```

**First complete native ESP32 SimpleX client!** 🎉

---

## 🎯 What is SimpleGo?

SimpleGo is a **groundbreaking open-source project** that implements a native [SimpleX Messaging Protocol (SMP)](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md) client for ESP32 microcontrollers. This is the **first known implementation** of the SimpleX protocol outside of the official Haskell codebase.

**Why is this significant?**

All existing SimpleX clients (mobile apps, desktop, CLI) use the Haskell core library via FFI (Foreign Function Interface). SimpleGo implements the protocol **from scratch in C**, enabling:

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
│  Crypto Engine                         ✅ COMPLETE      │
│  ├── Ed25519 (libsodium)              - Signatures      │
│  ├── X25519 (libsodium)               - Key Exchange    │
│  ├── XSalsa20-Poly1305 (libsodium)    - E2E Encryption  │
│  └── SHA-256 (mbedTLS)                - Hashing         │
├─────────────────────────────────────────────────────────┤
│  SMP Protocol Layer                    ✅ COMPLETE      │
│  ├── NEW, SUB, SEND, MSG, ACK, DEL    ✅ All Commands   │
│  ├── TLS 1.3 Transport                ✅ Working        │
│  ├── 16KB Block Framing               ✅ Working        │
│  └── NVS Key Persistence              ✅ Working        │
├─────────────────────────────────────────────────────────┤
│  Network Layer                         ✅ COMPLETE      │
│  ├── WiFi (ESP32)                                       │
│  ├── TLS 1.3 (mbedTLS)                                  │
│  └── Tor (planned)                                      │
└─────────────────────────────────────────────────────────┘
```

---

## 🔧 Hardware

### Currently Testing On
- **ESP32-S3 DevKit** — Development board

### Target Hardware

| Device | Status | Features |
|--------|--------|----------|
| **LilyGo T-Deck** | 🎯 Primary Target | ESP32-S3, 2.8" LCD, Keyboard, 8MB PSRAM |
| **LilyGo T-Embed** | 🎯 Secondary | ESP32-S3, 1.9" LCD, Rotary Encoder |
| **T-Deck Plus** | 📋 Planned | + GPS, 2000mAh Battery |
| **Heltec LoRa 32** | ✅ Tested | ESP32, 0.96" OLED, LoRa |

---

## ✅ What's Working

### SMP Commands

| Command | Status | Description |
|---------|--------|-------------|
| NEW | ✅ Complete | Queue creation with IDS response |
| SUB | ✅ Complete | Queue subscription |
| SEND | ✅ Complete | Message transmission |
| MSG | ✅ Complete | Message receive + decrypt |
| ACK | ✅ Complete | Message acknowledgment |
| DEL | ✅ Complete | Queue deletion |

### Features

| Feature | Status | Description |
|---------|--------|-------------|
| TLS 1.3 Connection | ✅ Complete | ChaCha20-Poly1305, ALPN "smp/1" |
| SMP Handshake | ✅ Complete | ServerHello/ClientHello exchange |
| E2E Encryption | ✅ Complete | X25519 DH + XSalsa20-Poly1305 |
| NVS Persistence | ✅ Complete | Keys survive reboots |
| Queue Reconnect | ✅ Complete | Skip NEW on restart |
| Multiple Queues | 📋 Planned | Contact management |

### Cryptography

| Feature | Status | Description |
|---------|--------|-------------|
| Ed25519 Signatures | ✅ Complete | libsodium, SPKI encoding |
| X25519 Key Exchange | ✅ Complete | DH shared secret |
| SHA-256 Hashing | ✅ Complete | Certificate fingerprints |
| XSalsa20-Poly1305 | ✅ Complete | Message encryption/decryption |
| Double Ratchet | 📋 Planned | Full E2E (Agent-level) |

---

## 📈 Performance Benchmarks (ESP32-S3 @ 240MHz)

| Operation | Time | Library |
|-----------|------|---------|
| Ed25519 KeyGen | ~8ms | libsodium |
| Ed25519 Sign | ~8ms | libsodium |
| Ed25519 Verify | ~21ms | libsodium |
| X25519 KeyGen | ~8ms | libsodium |
| X25519 DH | ~8ms | libsodium |
| SHA-256 | <1ms | mbedTLS (HW) |
| crypto_box decrypt | ~1ms | libsodium |
| TLS Handshake | ~800ms | mbedTLS |
| NVS read/write | ~5ms | ESP-IDF |

---

## 🛠️ Technical Stack

| Component | Technology | Notes |
|-----------|------------|-------|
| **MCU** | ESP32-S3 | Dual-core 240MHz, 8MB PSRAM |
| **Target Hardware** | LilyGo T-Deck / T-Embed | Display + Input |
| **Framework** | ESP-IDF 5.5.2 | Official Espressif IoT Development Framework |
| **TLS** | mbedTLS 3.x | TLS 1.3, ChaCha20-Poly1305 |
| **Cryptography** | libsodium | Ed25519, X25519, crypto_box |
| **Storage** | NVS | Non-volatile key persistence |
| **Protocol** | SMP v6 | SimpleX Messaging Protocol |

---

## 📁 Project Structure

```
SimpleGo/
├── README.md                 # This file
├── CHANGELOG.md              # Version history
├── ROADMAP.md                # Development roadmap
├── LICENSE                   # AGPL-3.0
│
├── main/
│   ├── main.c                # Main application (SMP client)
│   ├── CMakeLists.txt        # Component build config
│   └── idf_component.yml     # Component dependencies
│
├── docs/
│   ├── DEVELOPMENT.md        # Build & setup guide
│   ├── PROTOCOL.md           # SMP protocol deep dive
│   ├── TECHNICAL.md          # Key learnings & discoveries
│   └── DEVNOTES.md           # Development session notes
│
├── CMakeLists.txt            # Project build config
├── sdkconfig.defaults        # Default configuration
└── .gitignore                # Git ignore rules
```

---

## 🚀 Quick Start

### Prerequisites

- **Windows**: ESP-IDF 5.5.2 with PowerShell integration
- **Linux/macOS**: ESP-IDF 5.5.2 via install script
- **Hardware**: ESP32-S3 board (T-Deck or T-Embed recommended)

### Build & Flash

```powershell
# Windows (ESP-IDF PowerShell)
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5

# Linux/macOS
cd ~/esp/simplex_client
idf.py build flash monitor -p /dev/ttyUSB0
```

### Useful Monitor Commands

| Key | Action |
|-----|--------|
| `Ctrl+]` | Exit monitor |
| `Ctrl+T, R` | Reboot device |
| `Ctrl+T, H` | Help |

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for detailed setup instructions.

---

## 🗺️ Roadmap

See [ROADMAP.md](ROADMAP.md) for detailed plans.

**Phase 1: Protocol Foundation** ✅ Complete  
**Phase 2: Full Messaging** ✅ Complete  
**Phase 3: E2E Encryption** ✅ Complete  
**Phase 3.5: Persistence** ✅ Complete  
**Phase 3.6: Queue Management** ✅ **Complete!**  
**Phase 4: User Interface** 📋 Planned  
**Phase 5: Advanced Features** 📋 Future  

---

## 🔐 Security Model

SimpleGo inherits SimpleX's privacy-first design:

1. **No User Identifiers** — No phone numbers, usernames, or accounts
2. **No Central Directory** — No server stores your contact list
3. **Forward Secrecy** — Compromised keys don't expose past messages
4. **Metadata Protection** — Servers can't correlate senders and recipients

### Hardware Security Advantages

- **Physical Isolation** — No app store, no background processes
- **No Cloud Sync** — Keys never leave the device
- **Tamper Evidence** — Physical access required for compromise
- **Air-Gap Capable** — Can operate without persistent internet

---

## 🤝 Contributing

SimpleGo is part of the **Sentinel Secure Messenger Suite** and welcomes contributions!

1. **Read the docs** — Start with [DEVELOPMENT.md](docs/DEVELOPMENT.md) and [PROTOCOL.md](docs/PROTOCOL.md)
2. **Check the issues** — Look for `good first issue` labels
3. **Fork & PR** — Standard GitHub workflow

### Current Priorities

Looking for contributors in these areas:

1. **Multiple Queue Support** — Contact management
2. **Double Ratchet Port** — Cryptography expertise needed
3. **LVGL UI Development** — Embedded graphics experience
4. **Documentation** — Protocol analysis and guides

---

## 📜 License

**GNU Affero General Public License v3.0 (AGPL-3.0)**

See [LICENSE](LICENSE) for full terms.

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
| **v0.1.9-alpha** | **2026-01-20** | **🗑️ DEL + Full SMP Client!** |
| v0.1.8-alpha | 2026-01-20 | 🔑 NVS Persistence |
| v0.1.7-alpha | 2026-01-20 | 🎯 ACK Command |
| v0.1.6-alpha | 2026-01-20 | 🏆 E2E Decryption! |
| v0.1.5-alpha | 2026-01-20 | SEND + MSG receive |
| v0.4.1 | 2026-01-20 | SUB command |
| v0.4.0 | 2026-01-19 | NEW command (libsodium fix) |
| v0.3.0 | 2026-01-18 | Handshake (keyHash fix) |
| v0.2.0 | 2026-01-17 | TLS 1.3 |
| v0.1.0 | 2026-01-16 | Initial |

---

<p align="center">
  <strong>🏆 First Complete Native ESP32 SimpleX SMP Client! 🏆</strong><br>
  <em>Privacy is not a privilege, it's a right.</em>
</p>

---

*Copyright (c) 2026 cannatoshi — Part of the Sentinel Secure Messenger Suite*
