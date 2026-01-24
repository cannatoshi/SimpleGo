# Changelog

All notable changes to SimpleGo are documented in this file.

For detailed release information, see the [Release Notes](docs/release-info/).

---

## [Unreleased]

- App compatibility (A_MESSAGE parsing)
- UI Components

---

## [0.1.15-alpha] - 2026-01-24

### 🔐 Double Ratchet + X3DH Implementation!

**[Full Release Notes →](docs/release-info/v0.1.15-alpha.md)**

This release represents a major cryptographic milestone - implementing the complete Double Ratchet algorithm with X3DH key agreement from scratch.

#### 🏆 Historical Significance

**SimpleGo is the FIRST native SMP protocol implementation worldwide!**  
All other implementations are WebSocket API wrappers. SimpleGo speaks the real binary-level protocol.

#### Added

- **smp_x448.c** — X448 key generation and DH with wolfSSL byte-order fix
- **smp_ratchet.c** — Complete Double Ratchet with root/chain KDF, AES-GCM
- **smp_handshake.c** — E2E handshake and AgentConfirmation building
- **smp_queue.c** — SMPQueueInfo encoding and queue management
- **wolfSSL component** — X448/Curve448 cryptographic operations
- **Kyber KEM components** — Post-quantum crypto preparation

#### Fixed

- **12 encoding bugs** discovered through protocol analysis:
  - Bug #1-5: Length prefix corrections (Word16 BE vs 1-byte)
  - Bug #6: Payload AAD size (236 → 235 bytes)
  - Bug #7: KDF root output byte order
  - Bug #8: Chain KDF IV order (header_iv before msg_iv)
  - Bug #9: wolfSSL X448 byte-order reversal
  - Bug #10: SMPQueueInfo port encoding (space → length)
  - Bug #11: smpQueues count (Word16 BE)
  - Bug #12: queueMode Nothing encoding

#### Verified

- ✅ All cryptography verified against Python reference (100% match)
- ✅ AES-GCM with 16-byte IV verified
- ✅ Wire format verified against Haskell source
- ✅ Server accepts all messages with "OK"

#### Status

- Server: Accepts AgentConfirmation and HELLO ✅
- App: Shows "error agent A_MESSAGE" (parsing issue) 🔧

---

## [0.1.14-alpha] - 2026-01-21

### 🏗️ Modular Architecture + Peer Connection!

**[Full Release Notes →](docs/release-info/v0.1.14-alpha.md)**

- **Added:** Modular architecture (8 modules from ~1800 line monolith)
- **Added:** `smp_peer.c` — Peer server connection module
- **Added:** Auto-Connect on Invitation receive
- **Added:** `docs/ARCHITECTURE.md`, `.gitignore`
- **Fixed:** tcp_connect renamed to `smp_tcp_connect()` (lwip collision)
- **Fixed:** DH Key uses Standard Base64 in Invitation URIs
- **Status:** Server accepts CONF with "OK", App "Connected" pending

---

## [0.1.13-alpha] - 2026-01-21

### 🔧 Message Type Fix + Peer Queue Parsing

- **Fixed:** Message type at `_` delimiter + 3, not fixed offset
- **Added:** `peer_queue_t` structure, `url_decode_inplace()`
- **Added:** Peer server + Queue ID extraction
- **Status:** "READY TO SEND CONFIRMATION"

---

## [0.1.12-alpha] - 2026-01-21

### 🔐 Agent Protocol + Layer 5 Decryption

- **Added:** Layer 5 Contact DH decryption
- **Added:** Layer 6 Agent Protocol parsing
- **Added:** AgentInvitation detection, Reply Queue extraction
- **Fixed:** URL Encoding (Base64URL + double-encoded `=`)

---

## [0.1.11-alpha] - 2026-01-20

### 🔗 Invitation Links Working

- **Added:** SimpleX-compatible contact links (3 formats)
- **Status:** SimpleX Apps can connect to ESP32

---

## [0.1.10-alpha] - 2026-01-20

### 🏆 Multi-Contact + E2E Decryption

- **Added:** 10 contacts over one TLS connection
- **Fixed:** `crypto_box_beforenm()` for E2E, SEND format (ASCII flags)

---

## [0.1.9-alpha] - 2026-01-20

### 🗑️ Full SMP Client

- **Added:** DEL command, NVS auto-clear

---

## [0.1.8-alpha] - 2026-01-20

### 🔑 NVS Persistence

---

## [0.1.7-alpha] - 2026-01-20

### ✅ ACK Command

---

## [0.1.6-alpha] - 2026-01-20

### 🔐 E2E Encryption

---

## [0.1.5-alpha] - 2026-01-20

### 📨 SEND + MSG

---

## [0.1.4-alpha] - 2026-01-20

### 📡 SUB Command

---

## [0.1.3-alpha] - 2026-01-19

### 🎉 NEW Command

---

## [0.1.2-alpha] - 2026-01-18

### 🤝 Handshake

---

## [0.1.1-alpha] - 2026-01-17

### 🔒 TLS 1.3

---

## [0.1.0-alpha] - 2026-01-16

### Initial Release

---

## Version Summary

| Version | Milestone | Details |
|---------|-----------|---------|
| **v0.1.15** | 🔐 Double Ratchet | [Release Notes](docs/release-info/v0.1.15-alpha.md) |
| v0.1.14 | 🏗️ Modular + Peer | [Release Notes](docs/release-info/v0.1.14-alpha.md) |
| v0.1.13 | 🔧 Type Fix + Queue | Message parsing fixed |
| v0.1.12 | 🔐 Agent Protocol | 6-layer decryption |
| v0.1.11 | 🔗 Invitation Links | Apps can connect |
| v0.1.10 | 🏆 Multi-Contact | 10 contacts + E2E |
| v0.1.9 | 🗑️ Full SMP | All commands |
| v0.1.6 | 🔐 E2E | First encryption |
| v0.1.3 | 🎉 NEW | First command |
| v0.1.1 | 🔒 TLS | Connection works |

---

## Links

- [GitHub Repository](https://github.com/cannatoshi/SimpleGo)
- [SimpleX Protocol](https://github.com/simplex-chat/simplexmq)
