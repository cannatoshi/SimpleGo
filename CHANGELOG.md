# Changelog

All notable changes to SimpleGo are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- T-Embed UI (Display + Encoder)
- Double Ratchet (Curve448)
- Bidirectional Chat (two queues per contact)

---

## [0.1.11-alpha] - 2026-01-20

### 🔗 Invitation Links Working!

SimpleX Desktop/Mobile Apps can now connect directly to ESP32!

### Added
- **SimpleX-Compatible Contact Links** — ESP32 generates working invitation links
- **Three Link Formats** — SMP Queue URI, Web Link, Direct App Link
- **Base64 Standard Encoding** — For SPKI X25519 public keys
- **URL Encoding** — With correct double-encoding for Base64 special chars
- **Link Generation Functions** — `base64_standard_encode()`, `url_encode()`, `print_invitation_links()`

### Technical Details

**Link Formats Generated:**
```
📋 SMP Queue URI (raw):
smp://keyHash@server:5223/senderId#/?v=1-4&dh=<base64>&q=c

🌐 SimpleX Contact Link:
https://simplex.chat/contact#/?v=2-7&smp=<URL-ENCODED-SMP-URI>

📲 Direct App Link:
simplex:/contact#/?v=2-7&smp=<URL-ENCODED-SMP-URI>
```

**URL-Encoding Rules:**
```
Single encoded: : / @ # ? & =
Double encoded (Base64 DH-Key only): + → %252B, = → %253D
```

**Version Ranges:**
- Contact URI (outer): `v=2-7` (Agent Version Range)
- SMP Queue (inner): `v=1-4` (SMP Client Version Range)

**Parameters:**
- `dh=` — Base64 Standard encoded SPKI X25519 Public Key
- `q=c` — Queue Mode: Contact

### Proof - Working Output
```
🔗 SIMPLEX CONTACT LINKS ════════════════════════════════════════════
📱 [0] Test ─────────────────────────────────────────────────────────
📋 SMP Queue URI (raw):
   smp://1jne...@smp3.simplexonflux.com:5223/XLEV...#/?v=1-4&dh=MCow...&q=c

🌐 SimpleX Contact Link (COPY THIS!):
   https://simplex.chat/contact#/?v=2-7&smp=smp%3A%2F%2F...

══════════════════════════════════════════════════════════════════════
📝 ANLEITUNG:
   1. Den 🌐 Web Link kopieren
   2. In SimpleX Desktop/Mobile App öffnen
   3. 'Connect' klicken
   4. Nachricht senden
   5. ESP32 empfängt MSG!
```

### Test Results
- ✅ Link in Browser → SimpleX Landing Page
- ✅ Link in SimpleX App → "Connect to Contact" Dialog
- ✅ Connect → Works!
- ✅ Send Message → ESP32 receives MSG!

### Key Discoveries

| Discovery | Details |
|-----------|---------|
| Double Encoding | Only `+` and `=` in Base64 DH-Key are double-encoded |
| Queue Mode | `q=c` for Contact Queue |
| Version Ranges | Outer: `v=2-7`, Inner: `v=1-4` |
| DH Key Format | Base64 Standard (NOT base64url!) with SPKI Header |

### Haskell Source References

| File | Line | Discovery |
|------|------|-----------|
| Protocol.hs | 1078-1085 | `crEncode` Contact URI Format |
| Protocol.hs | SMPQueueUri | `v=1-4&dh=<key>&q=c` Format |
| ConnectionRequestTests.hs | - | `simplex:/contact#/?v=2-7&smp=` |

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
// WRONG: Raw X25519 shared secret is NOT a valid encryption key!
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
| **v0.1.11-alpha** | **2026-01-20** | **🔗 Invitation Links Working!** |
| v0.1.10-alpha | 2026-01-20 | 🏆 Multi-Contact + E2E |
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

**"First Native ESP32 SimpleX Client with Working Invitation Links"**

- ✅ Multiple Queues (10 contacts, one connection)
- ✅ Contact Management (Add/Remove/List)
- ✅ Full Message Lifecycle (NEW→SUB→SEND→MSG→DECRYPT→ACK)
- ✅ XSalsa20-Poly1305 E2E Encryption
- ✅ Ed25519 Signing + X25519 Key Exchange
- ✅ NVS Persistent Storage
- ✅ **SimpleX-Compatible Invitation Links**

---

## Links

- [GitHub Repository](https://github.com/cannatoshi/SimpleGo)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
