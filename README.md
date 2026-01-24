# SimpleGo

> **The First Native SimpleX SMP Client for ESP32** — Part of the Sentinel Secure Messenger Suite

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework: ESP-IDF 5.5](https://img.shields.io/badge/Framework-ESP--IDF%205.5-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Version: v0.1.15-alpha](https://img.shields.io/badge/Version-v0.1.15--alpha-orange.svg)]()
[![Status: Double Ratchet Implemented](https://img.shields.io/badge/Status-Double%20Ratchet%20Implemented-brightgreen.svg)]()

---

## 🏆 Historical Significance

**SimpleGo is the FIRST native SMP protocol implementation worldwide!**

All other SimpleX "implementations" are WebSocket API wrappers:
- simplex-python → WebSocket wrapper
- SimplOxide (Rust) → WebSocket SDK  
- TypeScript SDK → WebSocket API

**SimpleGo speaks the real binary-level SMP protocol directly on embedded hardware.**

---

## 🎯 Vision

SimpleGo brings [SimpleX Chat](https://simplex.chat/) — the first messaging platform without user identifiers — to standalone hardware devices. No smartphone required, no cloud dependency, complete privacy in your pocket.

---

## 🔐 MILESTONE: Double Ratchet + X3DH Implementation!

**As of v0.1.15-alpha (January 24, 2026)**, SimpleGo implements the complete Double Ratchet algorithm with X3DH key agreement:
`
📦 New Crypto Modules:
├── smp_x448.c      — X448 key generation with wolfSSL byte-order fix
├── smp_ratchet.c   — Complete Double Ratchet (root/chain KDF, AES-GCM)
├── smp_handshake.c — E2E handshake, AgentConfirmation building
└── smp_queue.c     — SMPQueueInfo encoding

🔬 Verification:
├── ✅ All crypto verified against Python (100% match)
├── ✅ AES-GCM with 16-byte IV verified
├── ✅ Wire format verified against Haskell source
└── ✅ Server accepts all messages ("OK")

🔧 Current Status:
├── ✅ Server accepts AgentConfirmation
├── ✅ Server accepts HELLO message
└── 🔧 App compatibility (A_MESSAGE parsing)
`

---

## 🏗️ Architecture

### Module Structure (v0.1.15)
`
┌─────────────────────────────────────────────────────────────────┐
│                        SimpleGo Client                          │
├─────────────────────────────────────────────────────────────────┤
│  main.c                          Application flow, main loop    │
├─────────────────────────────────────────────────────────────────┤
│  CRYPTO LAYER (NEW!)                                            │
│  ├── smp_x448.c      X448 DH with wolfSSL byte-order fix       │
│  ├── smp_ratchet.c   Double Ratchet, KDFs, AES-GCM             │
│  ├── smp_handshake.c E2E handshake, AgentConfirmation          │
│  └── smp_queue.c     SMPQueueInfo encoding                     │
├─────────────────────────────────────────────────────────────────┤
│  PROTOCOL LAYER                                                 │
│  ├── smp_peer.c      Peer server connection                    │
│  ├── smp_parser.c    Agent Protocol parsing                    │
│  └── smp_network.c   TLS/TCP I/O                               │
├─────────────────────────────────────────────────────────────────┤
│  APPLICATION LAYER                                              │
│  ├── smp_contacts.c  Contact management, NVS                   │
│  ├── smp_crypto.c    Ed25519, X25519, crypto_box               │
│  └── smp_utils.c     Base64, URL encoding                      │
├─────────────────────────────────────────────────────────────────┤
│  COMPONENTS                                                     │
│  ├── wolfssl         X448/Curve448 operations                  │
│  └── kyber           Post-quantum KEM (preparation)            │
└─────────────────────────────────────────────────────────────────┘
`

---

## ✅ What's Working

### Cryptography (100% Verified)

| Component | Status | Verification |
|-----------|--------|--------------|
| X448 DH | ✅ | Python match |
| X3DH Key Agreement | ✅ | Python match |
| HKDF-SHA512 | ✅ | Python match |
| Root KDF | ✅ | Python match |
| Chain KDF | ✅ | Python match |
| AES-GCM 256 | ✅ | Python match |
| 16-byte IV GHASH | ✅ | Python match |

### Protocol

| Feature | Status |
|---------|--------|
| TLS 1.3 Connection | ✅ |
| SMP Handshake | ✅ |
| Queue Creation (NEW) | ✅ |
| Queue Subscription (SUB) | ✅ |
| Message Send (SEND) | ✅ |
| Message Receive (MSG) | ✅ |
| Acknowledge (ACK) | ✅ |
| Delete Queue (DEL) | ✅ |
| Peer Server Connection | ✅ |
| AgentConfirmation | ✅ Server OK |
| HELLO Message | ✅ Server OK |
| **App Compatibility** | 🔧 In Progress |

### Application

| Feature | Status |
|---------|--------|
| Multi-Contact (10 slots) | ✅ |
| NVS Persistence | ✅ |
| SimpleX-compatible Links | ✅ |
| Invitation Parsing | ✅ |
| Auto-Connect | ✅ |

---

## 🐛 Bugs Fixed (12 Total)

| # | Bug | Fix |
|---|-----|-----|
| 1 | E2E key length | 1-byte prefix |
| 2 | HELLO prevMsgHash | Word16 BE |
| 3 | MsgHeader DH key | 1-byte prefix |
| 4 | ehBody length | 1-byte prefix |
| 5 | emHeader length | 1-byte prefix |
| 6 | Payload AAD size | 235 bytes |
| 7 | KDF root output order | Corrected |
| 8 | Chain KDF IV order | header_iv first |
| 9 | wolfSSL X448 byte-order | reverse_bytes() |
| 10 | SMPQueueInfo port | Length prefix |
| 11 | smpQueues count | Word16 BE |
| 12 | queueMode Nothing | Don't send '0' |

---

## 🔧 Hardware

| Device | Status |
|--------|--------|
| **LilyGo T-Deck** | 🎯 Primary Target |
| **LilyGo T-Embed** | 🎯 Secondary Target |
| Generic ESP32-S3 | ✅ Supported |

---

## 🚀 Quick Start

### Build & Flash
`powershell
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5
`

### Expected Output (v0.1.15)
`
🔗 SIMPLEX CONTACT LINKS ════════════════════════════════
📱 [0] Test
🌐 https://simplex.chat/contact#/?v=2-7&smp=...

[SimpleX App scans link and sends Invitation]

💬 MESSAGE for [Test]!
📋 Agent: Version=7, Type='I'
🔐 X3DH Key Agreement...
  ├── dh1: 3b270d17...
  ├── dh2: 407ee5f7...
  └── dh3: 133af800...
🔑 Double Ratchet initialized
📤 Sending AgentConfirmation...
✅ Server: OK
📤 Sending HELLO...
✅ Server: OK
`

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [CHANGELOG.md](CHANGELOG.md) | Version history |
| [ROADMAP.md](ROADMAP.md) | Development plan |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Module structure |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | SMP protocol details |
| [docs/CRYPTO.md](docs/CRYPTO.md) | Cryptography documentation |
| [docs/WIRE_FORMAT.md](docs/WIRE_FORMAT.md) | Wire format specification |
| [docs/BUGS.md](docs/BUGS.md) | Bug tracker |

---

## 🗺️ Roadmap

| Phase | Status |
|-------|--------|
| Phase 1-3.10: Foundation + Peer | ✅ Complete |
| Phase 3.11: Double Ratchet | ✅ **Complete!** |
| Phase 3.12: App Compatibility | 🔧 In Progress |
| Phase 4: User Interface | 📋 Planned |
| Phase 5: Production Ready | 📋 Future |

---

## 📜 License

**GNU Affero General Public License v3.0 (AGPL-3.0)**

Required by SimpleX protocol compatibility.

---

## 🙏 Acknowledgments

- [SimpleX Chat](https://simplex.chat/) — Protocol specification and Haskell reference
- [wolfSSL](https://www.wolfssl.com/) — X448/Curve448 implementation
- [Espressif](https://www.espressif.com/) — ESP-IDF framework

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.15-alpha** | **2026-01-24** | **🔐 Double Ratchet!** |
| v0.1.14-alpha | 2026-01-21 | 🏗️ Modular + Peer |
| v0.1.13-alpha | 2026-01-21 | 🔧 Message Type Fix |
| v0.1.12-alpha | 2026-01-21 | 🔐 Agent Protocol |
| v0.1.11-alpha | 2026-01-20 | 🔗 Invitation Links |

---

<p align="center">
  <strong>🏆 First Native ESP32 SimpleX Client — Double Ratchet Implemented! 🔐</strong><br>
  <em>Privacy is not a privilege, it's a right.</em>
</p>

---

*Copyright (c) 2026 cannatoshi — Part of the Sentinel Secure Messenger Suite*
