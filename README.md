# SimpleGo

> **The First Native SimpleX SMP Client for ESP32 — Modular Architecture** — Part of the Sentinel Secure Messenger Suite

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-green.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework: ESP-IDF 5.5](https://img.shields.io/badge/Framework-ESP--IDF%205.5-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Version: v0.1.14-alpha](https://img.shields.io/badge/Version-v0.1.14--alpha-orange.svg)]()
[![Status: Peer Connection Working](https://img.shields.io/badge/Status-Peer%20Connection%20Working-brightgreen.svg)]()

---

## 🎯 Vision

SimpleGo brings [SimpleX Chat](https://simplex.chat/) — the first messaging platform without user identifiers — to standalone hardware devices. No smartphone required, no cloud dependency, complete privacy in your pocket.

---

## 🏗️ MILESTONE: Modular Architecture + Peer Connection!

**As of v0.1.14-alpha (January 21, 2026)**, SimpleGo has been refactored into a clean modular architecture and can connect to peer SMP servers!

```
📦 Monolithic main.c (~1800 lines) → 8 Modules (~350 lines main.c)
🔌 Peer Connection: TLS + Handshake ✅
📤 AgentConfirmation: Server OK ✅
📱 App "Connected": Format pending 🔧
```

---

## 🏗️ Architecture

### Module Structure (v0.1.14)

```
┌─────────────────────────────────────────────────────────────────┐
│                        SimpleGo Client                          │
├─────────────────────────────────────────────────────────────────┤
│  main.c (~350 lines)                                            │
│  └── Application flow, WiFi init, main loop                     │
├─────────────────────────────────────────────────────────────────┤
│  smp_peer.c        ← NEW!        │  smp_parser.c                │
│  ├── peer_connect()              │  ├── parse_agent_message()   │
│  ├── peer_disconnect()           │  ├── handle_invitation()     │
│  └── send_agent_confirmation()   │  └── Auto-Connect trigger    │
├──────────────────────────────────┼──────────────────────────────┤
│  smp_contacts.c                  │  smp_network.c               │
│  ├── add/remove/list_contacts()  │  ├── smp_tcp_connect()       │
│  ├── NVS persistence             │  ├── tls_connect()           │
│  └── Message routing             │  └── send/receive blocks     │
├──────────────────────────────────┼──────────────────────────────┤
│  smp_crypto.c                    │  smp_utils.c                 │
│  ├── Ed25519 signatures          │  ├── base64_encode/decode    │
│  ├── X25519 DH                   │  ├── url_encode/decode       │
│  └── crypto_box                  │  └── hex utilities           │
├─────────────────────────────────────────────────────────────────┤
│  smp_globals.c                   │  smp_types.h                 │
│  └── Global variables            │  └── All structures/consts   │
└─────────────────────────────────────────────────────────────────┘
```

### Header Files

```
include/
├── smp_types.h      # Structures, constants, externs
├── smp_utils.h      # Encoding functions
├── smp_crypto.h     # Crypto functions
├── smp_network.h    # Network I/O
├── smp_contacts.h   # Contact management
├── smp_parser.h     # Agent Protocol
└── smp_peer.h       # Peer connection (NEW!)
```

---

## ✅ What's Working

### Connection Flow

```
┌──────────┐                              ┌──────────┐
│ SimpleX  │                              │  ESP32   │
│   App    │                              │          │
└────┬─────┘                              └────┬─────┘
     │  1. Scans Contact Link                  │
     │  2. SEND AgentInvitation ───────────────>  ✅
     │                                         │
     │  3. ESP32 parses invitation             │  ✅
     │  4. ESP32 connects to Peer Server       │  ✅
     │  5. ESP32 sends AgentConfirmation       │  ✅
     │     <─────────────────────────────────────
     │     Server: "OK"                        │  ✅
     │                                         │
     │  6. App shows "Connected"               │  🔧
```

### Features

| Feature | Status |
|---------|--------|
| **Modular Architecture** | ✅ 8 modules |
| **Peer Server Connection** | ✅ TLS working |
| **AgentConfirmation Sent** | ✅ Server OK |
| Agent Protocol (Layer 6) | ✅ Complete |
| 6-Layer Decryption | ✅ Complete |
| Multi-Contact (10 slots) | ✅ Complete |
| All SMP Commands | ✅ Complete |
| App "Connected" | 🔧 Format issue |

---

## 🔧 Hardware

| Device | Status |
|--------|--------|
| **LilyGo T-Deck** | 🎯 Primary |
| **LilyGo T-Embed** | 🎯 Secondary |

---

## 🚀 Quick Start

### Build & Flash

```powershell
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5
```

### Expected Output (v0.1.14)

```
🔗 SIMPLEX CONTACT LINKS ════════════════════════════════
📱 [0] Test
🌐 https://simplex.chat/contact#/?v=2-7&smp=...

[SimpleX App scans and sends Invitation]

💬 MESSAGE for [Test]!
📋 Agent: Version=7, Type='I'
📡 Peer: smp15.simplex.im:5223
🔌 Connecting to peer server...
✅ Peer TLS OK
✅ Peer Handshake OK
📤 Sending AgentConfirmation...
✅ Server: OK
```

---

## 🗺️ Roadmap

| Phase | Status |
|-------|--------|
| Phase 1-3.9: Foundation | ✅ Complete |
| Phase 3.10: Peer Connection | ✅ **Complete!** |
| Phase 3.11: encConnInfo Fix | 🔧 In Progress |
| Phase 4: User Interface | 📋 Planned |
| Phase 5: Double Ratchet | 📋 Future |

---

## 📜 License

**GNU Affero General Public License v3.0 (AGPL-3.0)**

---

## Version History

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.14-alpha** | **2026-01-21** | **🏗️ Modular + Peer!** |
| v0.1.13-alpha | 2026-01-21 | 🔧 Message Type Fix |
| v0.1.12-alpha | 2026-01-21 | 🔐 Agent Protocol |
| v0.1.11-alpha | 2026-01-20 | 🔗 Invitation Links |

---

<p align="center">
  <strong>🏗️ First Native ESP32 SimpleX Client — Modular Architecture! 🏗️</strong><br>
  <em>Privacy is not a privilege, it's a right.</em>
</p>

---

*Copyright (c) 2026 cannatoshi — Part of the Sentinel Secure Messenger Suite*
