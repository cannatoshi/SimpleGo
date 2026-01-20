# SimpleGo

> **The First Native SimpleX SMP Client for ESP32 with Full Message Layer Decoding** — Part of the Sentinel Secure Messenger Suite

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework: ESP-IDF 5.5](https://img.shields.io/badge/Framework-ESP--IDF%205.5-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Version: v0.1.12-alpha](https://img.shields.io/badge/Version-v0.1.12--alpha-orange.svg)]()
[![Status: Agent Protocol Working](https://img.shields.io/badge/Status-Agent%20Protocol%20Working-brightgreen.svg)]()

---

## 🎯 Vision

SimpleGo brings [SimpleX Chat](https://simplex.chat/) — the first messaging platform without user identifiers — to standalone hardware devices. No smartphone required, no cloud dependency, complete privacy in your pocket.

---

## 🔐 MILESTONE: Full Message Layer Decoding!

**As of v0.1.12-alpha (January 21, 2026)**, SimpleGo decodes the complete 6-layer message stack!

```
🔓 Layer 3 Decrypted: 16106 bytes (SMP E2E)
🔓 Layer 5 Decrypted: 847 bytes (Client DH)
📋 Agent Message: Version=7, Type='I' (Invitation)
🔗 Reply Queue: simplex:/invitation#/?v=2-7&smp=smp%3A%2F%2F...@smp10.simplex.im/...
👤 Peer Profile: {"displayName":"Alice",...}
```

**ESP32 now sees peer's profile and reply queue URI!** 🎉

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
┌─────────────────────────────────────────────────────────────────┐
│                       SimpleGo Client                           │
├─────────────────────────────────────────────────────────────────┤
│  UI Layer                                       📋 PLANNED      │
│  └── OLED/LCD Display (LVGL planned)                            │
├─────────────────────────────────────────────────────────────────┤
│  Agent Protocol Layer                           ✅ NEW!         │
│  ├── AgentInvitation Parser (Type 'I')                          │
│  ├── Reply Queue URI Extraction                                 │
│  ├── Peer Profile Parsing (ConnInfo)                            │
│  └── AgentConfirmation Builder (planned)                        │
├─────────────────────────────────────────────────────────────────┤
│  Message Decryption Stack                       ✅ COMPLETE     │
│  ├── Layer 3: SMP E2E (server DH)                               │
│  ├── Layer 5: Client DH (contact DH)            ✅ NEW!         │
│  └── Layer 6: Agent Protocol Parsing            ✅ NEW!         │
├─────────────────────────────────────────────────────────────────┤
│  Invitation Links                               ✅ FIXED        │
│  ├── Base64URL Encoding (not Standard!)                         │
│  └── Double-encoded = padding (%253D)                           │
├─────────────────────────────────────────────────────────────────┤
│  Contact Management                             ✅ COMPLETE     │
│  ├── Multi-Contact Database (10 slots)                          │
│  ├── NVS Persistence                                            │
│  └── Message Routing                                            │
├─────────────────────────────────────────────────────────────────┤
│  Crypto Engine                                  ✅ COMPLETE     │
│  ├── Ed25519 (libsodium)                                        │
│  ├── X25519 (libsodium)                                         │
│  ├── crypto_box (XSalsa20-Poly1305)                             │
│  └── SHA-256 (mbedTLS)                                          │
├─────────────────────────────────────────────────────────────────┤
│  SMP Protocol Layer                             ✅ COMPLETE     │
│  ├── NEW, SUB, SEND, MSG, ACK, DEL                              │
│  ├── TLS 1.3 Transport                                          │
│  └── 16KB Block Framing                                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## ✅ What's Working

### Message Layer Stack (Complete!)

```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 1: TLS 1.3 Transport                      ✅ COMPLETE    │
│  └── ALPN: "smp/1", ChaCha20-Poly1305                          │
├─────────────────────────────────────────────────────────────────┤
│  Layer 2: SMP Transport Block                    ✅ COMPLETE    │
│  └── [2-byte transmissionLength] [content] [padding to 16KB]   │
├─────────────────────────────────────────────────────────────────┤
│  Layer 3: SMP E2E Encryption                     ✅ COMPLETE    │
│  └── crypto_box(msg, nonce, server_dh_pub, our_dh_secret)      │
├─────────────────────────────────────────────────────────────────┤
│  Layer 4: SMP Client Message                     ✅ COMPLETE    │
│  └── [2-byte length prefix] [encrypted_content] [padding]      │
├─────────────────────────────────────────────────────────────────┤
│  Layer 5: Contact DH Encryption                  ✅ NEW!        │
│  └── [X25519 SPKI 44 bytes] [crypto_box encrypted body]        │
├─────────────────────────────────────────────────────────────────┤
│  Layer 6: Agent Protocol Message                 ✅ NEW!        │
│  └── [2-byte version BE] [type: 'C'/'I'/'M'/'R'] [body]        │
└─────────────────────────────────────────────────────────────────┘
```

### Agent Message Types

| Type | Name | Status | Description |
|------|------|--------|-------------|
| `'I'` | AgentInvitation | ✅ Parsed | Reply queue URI + profile |
| `'C'` | AgentConfirmation | 📋 Planned | Connection confirmation |
| `'M'` | AgentMsgEnvelope | 📋 Planned | Double Ratchet encrypted |
| `'R'` | AgentRatchetKey | 📋 Planned | Ratchet key exchange |

### Features

| Feature | Status | Description |
|---------|--------|-------------|
| **Agent Protocol** | ✅ **NEW!** | Full message layer decoding |
| **Client DH Decrypt** | ✅ **NEW!** | Layer 5 decryption working |
| **Reply Queue URI** | ✅ **NEW!** | Extract peer's SMP server + queue |
| **Peer Profile** | ✅ **NEW!** | See username from connInfo |
| Invitation Links | ✅ Fixed | Base64URL + double-encoded = |
| Multi-Contact | ✅ Complete | 10 contacts, one TLS connection |
| E2E Encryption | ✅ Complete | crypto_box Layer 3 |
| NVS Persistence | ✅ Complete | Contacts survive reboots |
| All SMP Commands | ✅ Complete | NEW, SUB, SEND, MSG, ACK, DEL |

---

## 🔧 Hardware

### Target Hardware

| Device | Status | Features |
|--------|--------|----------|
| **LilyGo T-Deck** | 🎯 Primary | ESP32-S3, 2.8" LCD, Keyboard, 8MB PSRAM |
| **LilyGo T-Embed** | 🎯 Secondary | ESP32-S3, 1.9" LCD, Rotary Encoder |

---

## 📈 Performance (ESP32-S3 @ 240MHz)

| Operation | Time |
|-----------|------|
| Ed25519 Sign | ~8ms |
| X25519 DH | ~8ms |
| crypto_box decrypt (Layer 3) | ~1ms |
| crypto_box decrypt (Layer 5) | ~1ms |
| Agent message parse | <1ms |
| TLS Handshake | ~800ms |

---

## 🛠️ Technical Stack

| Component | Technology |
|-----------|------------|
| **MCU** | ESP32-S3 (Dual-core 240MHz, 8MB PSRAM) |
| **Framework** | ESP-IDF 5.5.2 |
| **TLS** | mbedTLS 3.x (TLS 1.3, ChaCha20-Poly1305) |
| **Cryptography** | libsodium (Ed25519, X25519, crypto_box) |
| **Storage** | NVS (Non-volatile key persistence) |
| **Protocol** | SMP v6 + Agent Protocol |

---

## 🚀 Quick Start

### Build & Flash

```bash
cd ~/SimpleGo
idf.py build flash monitor -p /dev/ttyUSB0
```

### Expected Output

```
🔗 SIMPLEX CONTACT LINKS ════════════════════════════════
📱 [0] Test ──────────────────────────────────────────────
🌐 https://simplex.chat/contact#/?v=2-7&smp=...

[SimpleX App scannt Link und sendet Invitation]

💬 MESSAGE for [Test]!
🔓 Layer 3 Decrypted: 16106 bytes
🔓 Layer 5 Decrypted: 847 bytes
📋 Agent: Version=7, Type='I'
🔗 Reply Queue: smp10.simplex.im/...
👤 Peer: Alice
```

---

## 🗺️ Roadmap

| Phase | Status |
|-------|--------|
| Phase 1: Protocol Foundation | ✅ Complete |
| Phase 2: Full Messaging | ✅ Complete |
| Phase 3: E2E Encryption | ✅ Complete |
| Phase 3.5: Persistence | ✅ Complete |
| Phase 3.6: Multi-Contact | ✅ Complete |
| Phase 3.7: Invitation Links | ✅ Complete |
| Phase 3.8: Agent Protocol | ✅ **Complete!** |
| Phase 3.9: Connection Complete | 📋 Next |
| Phase 4: User Interface | 📋 Planned |
| Phase 5: Double Ratchet | 📋 Future |

---

## 🔐 Security Model

SimpleGo inherits SimpleX's privacy-first design:

1. **No User Identifiers** — No phone numbers, usernames, or accounts
2. **No Central Directory** — No server stores your contact list
3. **Forward Secrecy** — Per-contact key isolation
4. **Metadata Protection** — Servers can't correlate senders and recipients
5. **Double Encryption** — Layer 3 (SMP) + Layer 5 (Contact DH)

---

## 📜 License

**GNU Affero General Public License v3.0 (AGPL-3.0)**

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.12-alpha** | **2026-01-21** | **🔐 Agent Protocol!** |
| v0.1.11-alpha | 2026-01-20 | 🔗 Invitation Links |
| v0.1.10-alpha | 2026-01-20 | 🏆 Multi-Contact + E2E |
| v0.1.9-alpha | 2026-01-20 | 🗑️ Full SMP Client |

---

<p align="center">
  <strong>🔐 First Native ESP32 SimpleX Client with Full Message Layer Decoding! 🔐</strong><br>
  <em>Privacy is not a privilege, it's a right.</em>
</p>

---

*Copyright (c) 2026 cannatoshi — Part of the Sentinel Secure Messenger Suite*
