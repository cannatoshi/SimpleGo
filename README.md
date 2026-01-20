# SimpleGo

> **The First Native SimpleX SMP Client for ESP32** — Part of the Sentinel Secure Messenger Suite

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework: ESP-IDF 5.5](https://img.shields.io/badge/Framework-ESP--IDF%205.5-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Version: v0.1.5-alpha](https://img.shields.io/badge/Version-v0.1.5--alpha-orange.svg)]()
[![Status: SEND Working](https://img.shields.io/badge/Status-SEND%20Working-brightgreen.svg)]()

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

## 🏆 Current Achievement: Full Message Lifecycle

**As of v0.1.5-alpha (January 20, 2026), SimpleGo successfully:**

✅ Establishes TLS 1.3 connections with ChaCha20-Poly1305  
✅ Completes SMP handshake (ServerHello/ClientHello)  
✅ Creates message queues on SimpleX servers (NEW command)  
✅ Subscribes to queues for message reception (SUB command)  
✅ **Sends messages to queues (SEND command)**  
✅ **Receives and parses incoming messages (MSG)**  
✅ Generates Ed25519 signatures compatible with SimpleX servers  
✅ Implements correct SPKI key encoding  

```
I (xxxx) SMP: ========================================
I (xxxx) SMP:   SimpleGo v0.1.5-alpha
I (xxxx) SMP:   Native SMP Client for ESP32
I (xxxx) SMP: ========================================
I (xxxx) SMP: [5/8] Sending NEW command...
I (xxxx) SMP:   🎉🎉🎉 QUEUE CREATED! 🎉🎉🎉
I (xxxx) SMP: [7/8] Sending SUB command...
I (xxxx) SMP:   ✅ SUBSCRIBED! Ready to receive messages.
I (xxxx) SMP: [8/8] Testing SEND command...
I (xxxx) SMP:       SEND command sent!
I (xxxx) SMP:   💬 MESSAGE received!
I (xxxx) SMP:   ✅ OK - SEND confirmed
```

---

## 🛠️ Technical Stack

| Component | Technology | Notes |
|-----------|------------|-------|
| **MCU** | ESP32-S3 | Dual-core 240MHz, 8MB PSRAM |
| **Target Hardware** | LilyGo T-Deck | 2.8" LCD, Physical Keyboard, LoRa |
| **Framework** | ESP-IDF 5.5.2 | Official Espressif IoT Development Framework |
| **TLS** | mbedTLS 3.x | TLS 1.3, ChaCha20-Poly1305 |
| **Cryptography** | libsodium | Ed25519, X25519 (ESP-IDF component) |
| **Protocol** | SMP v6 | SimpleX Messaging Protocol |

### Why These Choices?

- **ESP-IDF over Arduino**: Full control over networking stack, proper TLS 1.3 support, production-ready
- **libsodium over Monocypher**: Critical discovery — Monocypher produces Ed25519 signatures incompatible with SimpleX servers (which use crypton/libsodium)
- **ESP32-S3 over ESP32**: More RAM, better crypto acceleration, USB-OTG for development

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
- **Hardware**: ESP32-S3 board (T-Deck recommended) or any ESP32-S3 DevKit

### Build & Flash

```powershell
# Windows (ESP-IDF PowerShell)
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5

# Linux/macOS
cd ~/esp/simplex_client
idf.py build flash monitor -p /dev/ttyUSB0
```

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for detailed setup instructions.

---

## 📊 Implementation Status

### Core Protocol

| Feature | Status | Description |
|---------|--------|-------------|
| TLS 1.3 Connection | ✅ Complete | ChaCha20-Poly1305, ALPN "smp/1" |
| SMP Handshake | ✅ Complete | ServerHello/ClientHello exchange |
| Transport Blocks | ✅ Complete | 16KB padded blocks |
| NEW Command | ✅ Complete | Queue creation with IDS response |
| SUB Command | ✅ Complete | Queue subscription |
| SEND Command | ✅ Complete | Message transmission |
| MSG Receive | ✅ Complete | Message parsing |
| ACK Command | 📋 Planned | Message acknowledgment |
| Message Decryption | 🔄 Next | XSalsa20-Poly1305 |

### Cryptography

| Feature | Status | Description |
|---------|--------|-------------|
| Ed25519 Signatures | ✅ Complete | libsodium, SPKI encoding |
| X25519 Key Exchange | ✅ Complete | DH key generation |
| SHA-256 Hashing | ✅ Complete | Certificate fingerprints |
| XSalsa20-Poly1305 | 🔄 Next | Message decryption |
| Double Ratchet | 📋 Planned | E2E message encryption |

---

## 🔐 Security Model

SimpleGo inherits SimpleX's privacy-first design:

1. **No User Identifiers** — No phone numbers, usernames, or accounts
2. **No Central Directory** — No server stores your contact list
3. **Forward Secrecy** — Compromised keys don't expose past messages
4. **Metadata Protection** — Servers can't correlate senders and recipients

### Hardware Security Advantages

Running on dedicated hardware adds:

- **Physical Isolation** — No app store, no background processes
- **No Cloud Sync** — Keys never leave the device
- **Tamper Evidence** — Physical access required for compromise
- **Air-Gap Capable** — Can operate without persistent internet

---

## 🗺️ Roadmap

See [ROADMAP.md](ROADMAP.md) for detailed plans.

**Phase 1: Protocol Foundation** ✅ Complete  
**Phase 2: Full Messaging** ✅ Complete  
**Phase 3: Message Encryption** 🔄 In Progress  
**Phase 4: User Interface** 📋 Planned  
**Phase 5: Advanced Features** 📋 Future  

---

## 🤝 Contributing

SimpleGo is part of the **Sentinel Secure Messenger Suite** and welcomes contributions!

### How to Contribute

1. **Read the docs** — Start with [DEVELOPMENT.md](docs/DEVELOPMENT.md) and [PROTOCOL.md](docs/PROTOCOL.md)
2. **Check the issues** — Look for `good first issue` labels
3. **Fork & PR** — Standard GitHub workflow
4. **Test thoroughly** — Protocol bugs can be subtle

---

## 📜 License

**GNU Affero General Public License v3.0 (AGPL-3.0)**

This license was chosen to align with SimpleX Chat's licensing and ensure that:
- Network service modifications remain open source
- The privacy community benefits from all improvements
- Commercial use requires source disclosure

See [LICENSE](LICENSE) for full terms.

---

## 🙏 Acknowledgments

- **[SimpleX Chat](https://simplex.chat/)** — Protocol design and inspiration
- **[Espressif](https://www.espressif.com/)** — ESP32 platform and ESP-IDF
- **[LilyGo](https://lilygo.cc/)** — T-Deck hardware
- **[libsodium](https://libsodium.org/)** — Cryptographic primitives

---

<p align="center">
  <strong>Privacy is not a privilege, it's a right.</strong><br>
  <em>Building the future of hardware-based private communication.</em>
</p>

---

*Copyright (c) 2026 cannatoshi — Part of the Sentinel Secure Messenger Suite*
