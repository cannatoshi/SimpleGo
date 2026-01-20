# Changelog

All notable changes to SimpleGo are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- T-Embed UI (Display + Encoder)
- Double Ratchet (Curve448)
- Contact naming via UI

---

## [0.1.10-alpha] - 2026-01-20

### 🏆 Multi-Contact + E2E Decryption Working!

Multiple contacts over ONE TLS connection with full E2E encryption!

### Added
- **Multi-Contact System** — Up to 10 contacts per connection
- **Contact Database** — `contacts_db_t` with persistent NVS storage
- **Contact Management** — `add_contact()`, `remove_contact()`, `list_contacts()`
- **Batch Subscribe** — `subscribe_all_contacts()` for all queues
- **Message Routing** — `find_contact_by_recipient_id()` for MSG dispatch
- **Self-Test** — `self_test_send()` verifies full E2E round-trip

### Fixed
- **CRITICAL: E2E Decryption** — `crypto_box_beforenm()` instead of raw `crypto_scalarmult()`
- **SEND Format** — `SEND ' ' flags ' ' body` (two spaces, ASCII flags!)

### Technical Details

**Data Structures:**
```c
typedef struct {
    char name[32];
    uint8_t rcv_auth_secret[64];  // Ed25519
    uint8_t rcv_auth_public[32];
    uint8_t rcv_dh_secret[32];    // X25519
    uint8_t rcv_dh_public[32];
    uint8_t recipient_id[24];
    uint8_t sender_id[24];
    uint8_t srv_dh_public[32];
    // ... lengths and flags
} contact_t;

typedef struct {
    uint8_t num_contacts;
    contact_t contacts[MAX_CONTACTS];  // 10 slots
} contacts_db_t;
```

**E2E Decryption Fix:**
```c
// WRONG: Raw X25519 produces wrong key format
crypto_scalarmult(shared, secret, public);
crypto_secretbox_open_easy(...);

// CORRECT: crypto_box does HSalsa20 key derivation
crypto_box_beforenm(shared, public, secret);
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

**SEND Command Format:**
```
SEND ' ' 'F' ' ' msgBody
     ↑    ↑   ↑
    0x20 ASCII 0x20
```

### Proof - Self-Test Output
```
📡 Subscriptions complete: 2/2
🧪 SELF-TEST: Sending message to [0] Test...
📤 SEND command sent!
💬 MESSAGE for [Test]!
🔓 DECRYPTED: Hello from ESP32!
📨 Sending ACK...
✅ OK
```

### Key Discoveries

| Discovery | Details |
|-----------|---------|
| MsgFlags | ASCII 'T'/'F', NOT binary 0x00/0x01 |
| SEND Format | Two spaces: after SEND, after flags |
| Encryption | Server encrypts for recipient using `rcvDhSecret` |
| crypto_box | HSalsa20 key derivation, not raw X25519 |
| Nonce | msgId zero-padded to 24 bytes |

---

## [0.1.9-alpha] - 2026-01-20

### 🗑️ DEL Command + Full SMP Client!

Queue deletion implemented! All base SMP commands now complete.

### Added
- **DEL Command** — Delete queues from server
- **NVS Auto-Clear** — Local keys cleared after successful DEL

### Technical Details
```
DEL Format:
  [signature][sessionId][corrId][recipientId]"DEL"
  ← Recipient Command, no parameters!
```

---

## [0.1.8-alpha] - 2026-01-20

### 🔑 NVS Key Persistence!

Keys and Queue-IDs now survive reboots.

### Added
- **NVS Storage** — Keys persist across reboots
- **Queue Reconnect** — SUB directly on restart, skip NEW
- **Key Management** — `have_saved_keys()`, `load/save/clear_keys()`

---

## [0.1.7-alpha] - 2026-01-20

### ✅ ACK Command Complete!

Full message lifecycle: NEW → SUB → SEND → MSG → ACK → OK

### Added
- **ACK Command** — Acknowledge received messages
- **OK Response Handling** — Clean command confirmations

### Protocol Note
ACK is a **Recipient command** — entityId = recipientId, NOT senderId!

---

## [0.1.6-alpha] - 2026-01-20

### 🔐 E2E Encryption (Single Queue)

First native ESP32 SimpleX client with working E2E encryption!

### Added
- **MSG Decryption** — XSalsa20-Poly1305 via libsodium
- **X25519 DH Shared Secret** — `crypto_box_beforenm()`
- **Server DH Key Storage** — Extract from IDS response

---

## [0.1.5-alpha] - 2026-01-20

### 📨 SEND + MSG Receive

### Added
- **SEND Command** — Send messages to queues
- **MSG Parsing** — Parse incoming messages with msgId, timestamp, flags
- **Message Loop** — Continuous listening for incoming messages

### Fixed
- **MsgFlags encoding** — Must be ASCII 'T'/'F'!

---

## [0.1.4-alpha] - 2026-01-20

### 📡 SUB Command

### Added
- **SUB Command** — Subscribe to created queues
- **RecipientId Storage** — For SUB command usage
- Complete NEW → IDS → SUB → OK flow

---

## [0.1.3-alpha] - 2026-01-19

### 🎉 NEW Command Working!

### Added
- **NEW Command** — Queue creation with IDS response
- **SPKI Key Encoding** — Ed25519 and X25519 keys
- **SubMode Parameter** — Required for SMP v6

### Fixed
- **CRITICAL: Switched to libsodium** — Monocypher Ed25519 incompatible with SimpleX

---

## [0.1.2-alpha] - 2026-01-18

### 🤝 Handshake Complete!

### Added
- **ClientHello** — Correct keyHash from CA certificate
- **Certificate Chain Parsing** — Server cert + CA cert

### Fixed
- **keyHash** — Must use CA certificate (2nd in chain)!

---

## [0.1.1-alpha] - 2026-01-17

### 🔒 TLS 1.3 Working!

### Added
- **TLS 1.3** — ChaCha20-Poly1305
- **ALPN** — "smp/1" negotiation
- **SNI** — Server Name Indication

---

## [0.1.0-alpha] - 2026-01-16

### Initial Release

### Added
- Project structure for ESP-IDF
- WiFi connection handling
- Basic TCP socket connection

---

## Version History Summary

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.10-alpha** | **2026-01-20** | **🏆 Multi-Contact + E2E Working!** |
| v0.1.9-alpha | 2026-01-20 | 🗑️ DEL + Full SMP Client |
| v0.1.8-alpha | 2026-01-20 | 🔑 NVS Persistence |
| v0.1.7-alpha | 2026-01-20 | ✅ ACK Command |
| v0.1.6-alpha | 2026-01-20 | 🔐 E2E Decryption (Single) |
| v0.1.5-alpha | 2026-01-20 | 📨 SEND + MSG |
| v0.1.4-alpha | 2026-01-20 | 📡 SUB Command |
| v0.1.3-alpha | 2026-01-19 | 🎉 NEW Command |
| v0.1.2-alpha | 2026-01-18 | 🤝 Handshake |
| v0.1.1-alpha | 2026-01-17 | 🔒 TLS 1.3 |
| v0.1.0-alpha | 2026-01-16 | Initial |

---

## 🏆 Achievement Unlocked

**"First Native ESP32 Multi-Contact SimpleX Client with E2E Encryption"**

- ✅ Multiple Queues (10 contacts, one connection)
- ✅ Contact Management (Add/Remove/List)
- ✅ Full Message Lifecycle (NEW→SUB→SEND→MSG→DECRYPT→ACK)
- ✅ XSalsa20-Poly1305 E2E Encryption
- ✅ Ed25519 Signing + X25519 Key Exchange
- ✅ NVS Persistent Storage

---

## Links

- [GitHub Repository](https://github.com/cannatoshi/SimpleGo)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
