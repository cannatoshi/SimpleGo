# Changelog

All notable changes to SimpleGo are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- T-Embed UI (Display + Encoder)
- Multiple queue support
- Contact management
- Double Ratchet (Curve448)

---

## [0.1.9-alpha] - 2026-01-20

### 🗑️ DEL Command + Full SMP Client!

Queue deletion implemented! All base SMP commands now complete — SimpleGo is a **full single-queue SMP client**.

### Added
- **DEL Command** — Delete queues from server
- **NVS Auto-Clear** — Local keys cleared after successful DEL
- **Full SMP Client** — All base commands implemented!

### Technical Details

**DEL Command Format:**
```
[sigLen=64][signature]
[sessLen=32][sessionId]
[corrIdLen][corrId]
[entityIdLen][recipientId]    ← Recipient Command!
"DEL"                         ← No parameters
```

**Server Response:**
- `OK` = Queue + all messages deleted

### Proof - Log Output
```
I (187810) SMP:   🗑️ Deleting queue...
I (187930) SMP:   DEL sent!
I (188170) SMP:   ✅ Queue deleted from server!
I (188190) SMP:       NVS: Keys cleared!
I (188190) SMP:   ✅ NVS cleared!
```

### 🏆 Full SMP Command Set Complete!

| Command | Function | Status |
|---------|----------|--------|
| NEW | Create queue | ✅ |
| SUB | Subscribe to queue | ✅ |
| SEND | Send message | ✅ |
| MSG | Receive + decrypt | ✅ |
| ACK | Acknowledge message | ✅ |
| DEL | Delete queue | ✅ |

---

## [0.1.8-alpha] - 2026-01-20

### 🔑 NVS Key Persistence!

Keys and Queue-IDs now survive reboots! On restart, the existing queue is reused instead of creating a new one.

### Added
- **NVS Storage** — Keys persist across reboots
- **Queue Reconnect** — SUB directly on restart, skip NEW
- **Key Management Functions** — `have_saved_keys()`, `load_keys_from_nvs()`, `save_keys_to_nvs()`, `clear_saved_keys()`

### Technical Details

**Persisted Data (NVS Namespace: "simplego"):**

| Key | Size | Description |
|-----|------|-------------|
| rcv_auth_sk | 64 bytes | Ed25519 Secret Key |
| rcv_auth_pk | 32 bytes | Ed25519 Public Key |
| rcv_dh_sk | 32 bytes | X25519 Secret Key |
| rcv_dh_pk | 32 bytes | X25519 Public Key |
| rcv_id + rcv_id_len | 24+1 bytes | Recipient ID |
| snd_id + snd_id_len | 24+1 bytes | Sender ID |
| srv_dh_pk + have_srv_dh | 32+1 bytes | Server DH Key |

**New Flow:**
```
Start
  │
  ▼
TLS + ServerHello + ClientHello
  │
  ▼
load_keys_from_nvs()
  │
  ├── Keys found? ──► Skip NEW ──► SUB directly
  │
  └── No keys? ──► NEW ──► IDS ──► save_keys_to_nvs() ──► SUB
```

### Proof - Reboot Log

**First Start (NEW + Save):**
```
I (6769) SMP:   🎉🎉🎉 QUEUE CREATED! 🎉🎉🎉
I (6779) SMP:   📥 RecipientId (24 bytes): cb1ab7dfa04183e65fe52aeb7fa7118162b3c76e543284c3
I (6809) SMP:       NVS: Keys saved!
```

**After Reboot (Load + Skip NEW):**
```
I (6289) SMP:       NVS: Keys loaded!
I (6289) SMP:       rcvAuthKey: e92b3e5b...
I (6289) SMP:       recipientId (24 bytes): cb1ab7df...
I (6289) SMP:   [4-6] Skipping NEW - using saved queue!
I (6299) SMP:   [7/7] Sending SUB command...
I (6659) SMP:   ✅ SUBSCRIBED! Ready to receive messages.
```

---

## [0.1.7-alpha] - 2026-01-20

### ✅ ACK Command Complete!

Full message lifecycle now operational: NEW → SUB → SEND → MSG → ACK → OK

### Added
- **ACK command implementation** — Acknowledge received messages
- **Message deletion from queue** — Server confirms with OK
- **OK response handling** — Clean logging for command confirmations

### Technical Details
```
ACK Format:
  [sigLen=64][signature]
  [sessLen=32][sessionId]
  [corrIdLen][corrId]
  [entityIdLen][recipientId]    ← NOT senderId!
  "ACK " [msgIdLen][msgId]

Signature covers:
  [0x20][sessionId] + [corrId + entityId + "ACK " + msgId]
```

### Protocol Note: SMP Versions

| Version | Feature | Impact |
|---------|---------|--------|
| **v6** | Base protocol | ✅ What we use |
| **v7+** | `implySessId` | sessionId not sent, included in signature |
| **v7+** | `authEncryptCmds` | Commands encrypted with X25519 DH |
| **v17** | Latest features | Batch commands, optimizations |

---

## [0.1.6-alpha] - 2026-01-20

### 🏆 MEGA-MILESTONE: E2E Encryption Working!

First native ESP32 SimpleX client with working end-to-end encryption!
Successfully sent, received, and **decrypted** "Hello from ESP32!" 🎉

### Added
- **MSG Decryption** — XSalsa20-Poly1305 via libsodium
- **X25519 DH Shared Secret** — `crypto_box_beforenm()`
- **Server DH Key Storage** — Extract from IDS response
- **Full E2E Round-Trip** — NEW→SUB→SEND→MSG→Decrypt

### Technical Implementation
```c
// 1. Compute DH Shared Secret
uint8_t shared[crypto_box_BEFORENMBYTES];
crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);

// 2. Nonce = msgId (24 bytes, zero-padded)
uint8_t nonce[24] = {0};
memcpy(nonce, msg_id, msgIdLen);

// 3. Decrypt with NaCl crypto_box
crypto_box_open_easy_afternm(plain, cipher, cipher_len, nonce, shared);
```

---

## [0.1.5-alpha] - 2026-01-20

### 🎉 Major Milestone: Full Message Lifecycle!

SEND command working, MSG receive implemented, complete message loop operational.

### Added
- **SEND command implementation** — Send messages to queues
- **MSG receive parsing** — Parse incoming messages with msgId, timestamp, flags
- **Message receive loop** — Continuous listening for incoming messages
- **OK confirmation handling** — SEND success confirmation

### Fixed
- **CRITICAL: MsgFlags encoding** — Must be ASCII 'T'/'F', NOT binary 0x00/0x01!
- **Space after msgFlags** — Required separator before msgBody
- **Unsecured queue auth** — authLen = 0 for queues without SKEY

---

## [0.1.4-alpha] - 2026-01-20

### Added
- **SUB command implementation** — Subscribe to created queues
- SUB response parsing with transport format handling
- Queue subscription confirmation

---

## [0.1.3-alpha] - 2026-01-19

### 🎉 Breakthrough: Queue Creation Working!

### Added
- **NEW command with successful IDS response** — First working queue creation!
- SPKI key encoding for Ed25519 and X25519 keys
- SubMode parameter ('S' for SMSubscribe)
- Local signature verification before sending

### Fixed
- **CRITICAL: Switched from Monocypher to libsodium** — Monocypher Ed25519 signatures incompatible with SimpleX servers

---

## [0.1.2-alpha] - 2026-01-18

### 🎉 Handshake Complete!

### Added
- **ClientHello with correct keyHash** — Handshake succeeds!
- Certificate chain parsing (server cert + CA cert)
- SHA-256 hash computation for keyHash

### Fixed
- **CRITICAL: keyHash must use CA certificate (2nd in chain)**, not server certificate

---

## [0.1.1-alpha] - 2026-01-17

### 🎉 TLS 1.3 Working!

### Added
- **TLS 1.3 connection with ChaCha20-Poly1305**
- ALPN negotiation for "smp/1"
- Cipher suite restriction to TLS 1.3 only
- SNI support

---

## [0.1.0-alpha] - 2026-01-16

### Initial Release

### Added
- Project structure for ESP-IDF
- WiFi connection handling
- Basic TCP socket connection
- Initial mbedTLS integration

---

## Version History Summary

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.9-alpha** | **2026-01-20** | **🗑️ DEL + Full SMP Client!** |
| v0.1.8-alpha | 2026-01-20 | 🔑 NVS Persistence |
| v0.1.7-alpha | 2026-01-20 | 🎯 ACK Command |
| v0.1.6-alpha | 2026-01-20 | 🏆 E2E Decryption! |
| v0.1.5-alpha | 2026-01-20 | SEND + MSG receive |
| v0.1.4-alpha | 2026-01-20 | SUB command |
| v0.1.3-alpha | 2026-01-19 | NEW command (libsodium fix) |
| v0.1.2-alpha | 2026-01-18 | Handshake (keyHash fix) |
| v0.1.1-alpha | 2026-01-17 | TLS 1.3 |
| v0.1.0-alpha | 2026-01-16 | Initial |

---

## 🏆 Achievement Unlocked

**"First Complete Native ESP32 SimpleX SMP Client"**

- ✅ Queue Management (NEW, SUB, DEL)
- ✅ Message Lifecycle (SEND, MSG, ACK)
- ✅ SMP Protocol v6
- ✅ Ed25519 Signing
- ✅ X25519 Key Exchange
- ✅ NaCl crypto_box Encryption
- ✅ NVS Key Persistence
- ✅ **Full Single-Queue SMP Client!**

---

## Links

- [GitHub Repository](https://github.com/cannatoshi/SimpleGo)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
