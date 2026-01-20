# SimpleGo

> **The First Native SimpleX SMP Client for ESP32** — Part of the Sentinel Secure Messenger Suite

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework: ESP-IDF 5.5](https://img.shields.io/badge/Framework-ESP--IDF%205.5-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Status: Working](https://img.shields.io/badge/Status-Queue%20Creation%20Working-brightgreen.svg)]()

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

## 🏆 Current Achievement: Working SMP Client

**As of v4.1, SimpleGo successfully:**

✅ Establishes TLS 1.3 connections with ChaCha20-Poly1305  
✅ Completes SMP handshake (ServerHello/ClientHello)  
✅ Creates message queues on SimpleX servers (NEW command)  
✅ Subscribes to queues for message reception (SUB command)  
✅ Generates Ed25519 signatures compatible with SimpleX servers  
✅ Implements correct SPKI key encoding  
✅ Handles SMP v6 protocol format  

```
I (6688) SMP:   🎉🎉🎉 QUEUE CREATED! 🎉🎉🎉
I (6688) SMP:   📥 RecipientId (24 bytes): e1c77e711e254cab7de8fa5db27b433922c9227f5abcd298
I (6698) SMP:   📤 SenderId (24 bytes): 6ce4d1233896d0243871b897f1657d84d0a2601bf306f365
I (7158) SMP:   ✅ SUBSCRIBED! Ready to receive messages.
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
├── src/
│   └── main/
│       ├── main.c            # Main application (SMP client)
│       ├── CMakeLists.txt    # Component build config
│       └── idf_component.yml # Component dependencies
│
├── docs/
│   ├── DEVELOPMENT.md        # Build & setup guide
│   ├── PROTOCOL.md           # SMP protocol deep dive
│   ├── TECHNICAL.md          # Key learnings & discoveries
│   └── DEVNOTES.md           # Development session notes
│
├── CMakeLists.txt            # Project build config
├── sdkconfig                 # ESP-IDF configuration
└── sdkconfig.defaults        # Default configuration
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

### Expected Output

```
I (5765) SMP: ========================================
I (5765) SMP:   SimpleGo v4.1 - NEW + SUB!
I (5765) SMP: ========================================
I (5865) SMP: [1/6] TCP + TLS...
I (6088) SMP:       TLS OK! ALPN: smp/1
I (6088) SMP: [2/6] Waiting for ServerHello...
I (6288) SMP:       Versions: 6-8, SessionId: a1b2c3d4...
I (6288) SMP: [3/6] Sending ClientHello...
I (6398) SMP: [4/6] Generating keypairs...
I (6398) SMP: [5/6] Sending NEW command...
I (6688) SMP:   🎉🎉🎉 QUEUE CREATED! 🎉🎉🎉
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
| SEND Command | 🔄 Next | Message transmission |
| ACK Command | 📋 Planned | Message acknowledgment |
| OFF/DEL Commands | 📋 Planned | Queue management |

### Cryptography

| Feature | Status | Description |
|---------|--------|-------------|
| Ed25519 Signatures | ✅ Complete | libsodium, SPKI encoding |
| X25519 Key Exchange | ✅ Complete | DH key generation |
| SHA-256 Hashing | ✅ Complete | Certificate fingerprints |
| Double Ratchet | 📋 Planned | E2E message encryption |
| Curve448 | 📋 Planned | Extended key exchange |

### User Interface

| Feature | Status | Description |
|---------|--------|-------------|
| Serial Console | ✅ Working | Debug output via USB |
| OLED Status | 📋 Planned | Connection/message indicators |
| T-Deck LCD | 📋 Planned | Full messaging UI |
| Physical Keyboard | 📋 Planned | Message composition |

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
- TLS 1.3, SMP handshake, NEW/SUB commands

**Phase 2: Full Messaging** 🔄 In Progress
- SEND command, message reception, ACK handling

**Phase 3: End-to-End Encryption** 📋 Planned
- Double Ratchet implementation, key management

**Phase 4: User Interface** 📋 Planned
- T-Deck display, keyboard input, contact management

**Phase 5: Advanced Features** 📋 Future
- Groups, file transfer, 4G connectivity, Tor support

---

## 🤝 Contributing

SimpleGo is part of the **Sentinel Secure Messenger Suite** and welcomes contributions!

### How to Contribute

1. **Read the docs** — Start with [DEVELOPMENT.md](docs/DEVELOPMENT.md) and [PROTOCOL.md](docs/PROTOCOL.md)
2. **Check the issues** — Look for `good first issue` labels
3. **Fork & PR** — Standard GitHub workflow
4. **Test thoroughly** — Protocol bugs can be subtle

### Development Environment

- **Windows**: Recommended for ESP-IDF (native toolchain)
- **WSL**: Useful for Haskell source analysis
- **Hardware**: Any ESP32-S3 board for testing

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

## 📞 Contact & Community

- **GitHub Issues** — Bug reports and feature requests
- **Sentinel Suite** — Part of the broader secure communication ecosystem

---

<p align="center">
  <strong>Privacy is not a privilege, it's a right.</strong><br>
  <em>Building the future of hardware-based private communication.</em>
</p>
