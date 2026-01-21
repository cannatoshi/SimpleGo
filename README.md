# SimpleGo

> **The First Native SimpleX SMP Client for ESP32 — Ready to Send Confirmation!** — Part of the Sentinel Secure Messenger Suite

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework: ESP-IDF 5.5](https://img.shields.io/badge/Framework-ESP--IDF%205.5-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Version: v0.1.13-alpha](https://img.shields.io/badge/Version-v0.1.13--alpha-orange.svg)]()
[![Status: Peer Queue Parsed](https://img.shields.io/badge/Status-Peer%20Queue%20Parsed-brightgreen.svg)]()

---

## 🎯 Vision

SimpleGo brings [SimpleX Chat](https://simplex.chat/) — the first messaging platform without user identifiers — to standalone hardware devices. No smartphone required, no cloud dependency, complete privacy in your pocket.

---

## 🔧 MILESTONE: Peer Queue Parsing!

**As of v0.1.13-alpha (January 21, 2026)**, SimpleGo correctly parses AgentInvitation and extracts peer server info!

```
💬 MESSAGE for [Test]!
🔓 Layer 3 Decrypted: 16106 bytes
🔓 Layer 5 Decrypted: 847 bytes
📋 Agent: Version=7, Type='I' (Invitation)
📡 Peer Server: smp15.simplex.im:5223
📮 Queue ID: ahjPk2jlNZz53yh5RJ-sBCIu_vZQeWdK
✅ READY TO SEND CONFIRMATION
```

**ESP32 knows where to send the confirmation response!** 🎉

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
│  Connection Handler                             🔧 IN PROGRESS  │
│  ├── peer_queue_t Structure                     ✅ NEW!         │
│  ├── Peer Server Extraction                     ✅ NEW!         │
│  ├── Queue ID Extraction                        ✅ NEW!         │
│  ├── DH Key Extraction                          🔧 In Progress  │
│  └── CONF Response Builder                      ⏳ Next         │
├─────────────────────────────────────────────────────────────────┤
│  Agent Protocol Layer                           ✅ COMPLETE     │
│  ├── Message Type Fix ('_' + 3)                 ✅ FIXED!       │
│  ├── AgentInvitation Parser (Type 'I')          ✅ Working      │
│  └── url_decode_inplace()                       ✅ NEW!         │
├─────────────────────────────────────────────────────────────────┤
│  Message Decryption Stack                       ✅ COMPLETE     │
│  ├── Layer 3: SMP E2E (server DH)                               │
│  ├── Layer 5: Client DH (contact DH)                            │
│  └── Layer 6: Agent Protocol Parsing                            │
├─────────────────────────────────────────────────────────────────┤
│  Contact Management                             ✅ COMPLETE     │
│  ├── Multi-Contact Database (10 slots)                          │
│  ├── NVS Persistence                                            │
│  └── Message Routing                                            │
├─────────────────────────────────────────────────────────────────┤
│  Crypto Engine                                  ✅ COMPLETE     │
│  ├── Ed25519 + X25519 (libsodium)                               │
│  └── crypto_box (XSalsa20-Poly1305)                             │
├─────────────────────────────────────────────────────────────────┤
│  SMP Protocol Layer                             ✅ COMPLETE     │
│  ├── NEW, SUB, SEND, MSG, ACK, DEL                              │
│  └── TLS 1.3 + 16KB Block Framing                               │
└─────────────────────────────────────────────────────────────────┘
```

---

## ✅ What's Working

### Message Type Parsing (FIXED in v0.1.13!)

```
Message Format After DH Decryption:

2a a5 5f 00 07 49 ...
*  ?  _  ver   I
0  1  2  3  4  5

✅ Find '_' delimiter (position 2)
✅ Read version at +1,+2 (Big Endian)
✅ Read type at +3 ('C', 'I', 'M', 'R')
```

### Peer Queue Extraction

| Data | Status | Example |
|------|--------|---------|
| Peer Server | ✅ Extracted | `smp15.simplex.im` |
| Port | ✅ Extracted | `5223` |
| Queue ID | ✅ Extracted | `ahjPk2jlNZz53yh5RJ-sBCIu_vZQeWdK` |
| Key Hash | ✅ Extracted | (32 bytes) |
| DH Public Key | 🔧 In Progress | (multi-encoded URL) |

### Agent Message Types

| Type | Name | Status |
|------|------|--------|
| `'I'` | AgentInvitation | ✅ Parsed |
| `'C'` | AgentConfirmation | ⏳ Next (to send) |
| `'M'` | AgentMsgEnvelope | 📋 Planned |
| `'R'` | AgentRatchetKey | 📋 Planned |

### Features Summary

| Feature | Status |
|---------|--------|
| **Message Type Fix** | ✅ **FIXED!** |
| **Peer Server Extraction** | ✅ **NEW!** |
| **Queue ID Extraction** | ✅ **NEW!** |
| **url_decode_inplace()** | ✅ **NEW!** |
| Agent Protocol (Layer 6) | ✅ Complete |
| Client DH Decrypt (Layer 5) | ✅ Complete |
| SMP E2E (Layer 3) | ✅ Complete |
| Multi-Contact | ✅ Complete |
| All SMP Commands | ✅ Complete |
| DH Key Extraction | 🔧 In Progress |
| CONF Response | ⏳ Next |

---

## 🔧 Hardware

### Target Hardware

| Device | Status | Features |
|--------|--------|----------|
| **LilyGo T-Deck** | 🎯 Primary | ESP32-S3, 2.8" LCD, Keyboard |
| **LilyGo T-Embed** | 🎯 Secondary | ESP32-S3, 1.9" LCD, Encoder |

---

## 🚀 Quick Start

### Build & Flash

```bash
cd ~/SimpleGo
idf.py build flash monitor -p /dev/ttyUSB0
```

### Expected Output (v0.1.13)

```
🔗 SIMPLEX CONTACT LINKS ════════════════════════════════
📱 [0] Test ──────────────────────────────────────────────
🌐 https://simplex.chat/contact#/?v=2-7&smp=...

[SimpleX App scans link and sends Invitation]

💬 MESSAGE for [Test]!
🔓 Layer 3 Decrypted: 16106 bytes
🔓 Layer 5 Decrypted: 847 bytes
📋 Agent: Version=7, Type='I' (Invitation)
📡 Peer Server: smp15.simplex.im:5223
📮 Queue ID: ahjPk2jlNZz53yh5RJ-sBCIu_vZQeWdK
✅ READY TO SEND CONFIRMATION
```

---

## 🗺️ Roadmap

| Phase | Status |
|-------|--------|
| Phase 1-3.7: Foundation | ✅ Complete |
| Phase 3.8: Agent Protocol | ✅ Complete |
| Phase 3.9: Peer Queue Parsing | ✅ **Complete!** |
| Phase 3.10: Connection Complete | 🔧 In Progress |
| Phase 4: User Interface | 📋 Planned |
| Phase 5: Double Ratchet | 📋 Future |

---

## 📜 License

**GNU Affero General Public License v3.0 (AGPL-3.0)**

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.13-alpha** | **2026-01-21** | **🔧 Message Type Fix + Peer Queue!** |
| v0.1.12-alpha | 2026-01-21 | 🔐 Agent Protocol |
| v0.1.11-alpha | 2026-01-20 | 🔗 Invitation Links |
| v0.1.10-alpha | 2026-01-20 | 🏆 Multi-Contact + E2E |

---

<p align="center">
  <strong>🔧 First Native ESP32 SimpleX Client — Ready to Send Confirmation! 🔧</strong><br>
  <em>Privacy is not a privilege, it's a right.</em>
</p>

---

*Copyright (c) 2026 cannatoshi — Part of the Sentinel Secure Messenger Suite*
