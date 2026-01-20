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

## [0.4.1] - 2026-01-20

### 🎉 Major Milestone: Full Queue Lifecycle Working!

This release marks a **historic achievement** — the first successful native SMP client implementation outside of the official Haskell codebase.

### Added
- **SUB command implementation** — Subscribe to created queues
- Queue subscription confirmation handling
- RecipientId storage for SUB command usage
- Complete NEW → IDS → SUB → OK flow

### Technical Details
```
NEW command → Server returns IDS with:
  - RecipientId (24 bytes) — Used for SUB command
  - SenderId (24 bytes) — For sender identification  
  - ServerDhKey (44 bytes SPKI) — For key exchange

SUB command → Server returns OK
  - Queue now active for message reception
```

---

## [0.4.0] - 2026-01-19

### 🎉 Breakthrough: Queue Creation Working!

### Added
- **NEW command with successful IDS response** — First working queue creation!
- SPKI key encoding for Ed25519 and X25519 keys
- SubMode parameter ('S' for SMSubscribe) — Required for SMP v6
- Local signature verification before sending

### Fixed
- **Critical: Switched from Monocypher to libsodium** — Monocypher Ed25519 signatures are incompatible with SimpleX servers (crypton library)

### Technical Discovery
```
Monocypher vs libsodium Ed25519 signatures:
- Same input data
- Same key material  
- DIFFERENT signatures!
- SimpleX uses crypton (libsodium-compatible)
- Solution: Use ESP-IDF's libsodium component
```

---

## [0.3.3] - 2026-01-19

### Added
- PING command test implementation
- Command block format (vs handshake block format)
- TransmissionCount and TransmissionLength headers

### Fixed
- Block format differentiation between handshake and commands
- Proper transport block structure with padding

### Error Progression
```
ERR BLOCK → Fixed block format
ERR CMD SYNTAX → Added subMode parameter
ERR AUTH → Switched to libsodium (next version)
```

---

## [0.3.2] - 2026-01-19

### Added
- Transport block format implementation
- 16KB block padding with '#' character
- Content length prefix (2 bytes, big-endian)

---

## [0.3.1] - 2026-01-18

### Added
- Ed25519 signature generation for transmission authentication
- SessionId integration in signed data
- Transmission body structure (corrId, entityId, command)

### Technical Details
```
Signed data format:
  [0x20][sessionId 32 bytes][transmission_body]
  
Transmission format:
  [sigLen][signature 64 bytes]
  [sessLen][sessionId 32 bytes]
  [corrIdLen][corrId]
  [entityIdLen][entityId]
  [command...]
```

---

## [0.3.0] - 2026-01-18

### 🎉 Handshake Complete!

### Added
- **ClientHello with correct keyHash** — Handshake now succeeds!
- Certificate chain parsing (server cert + CA cert)
- SHA-256 hash of CA certificate for keyHash

### Fixed
- **Critical: keyHash must be computed from CA certificate (2nd in chain), not server certificate**

---

## [0.2.5] - 2026-01-18

### Added
- ServerHello parsing and validation
- Protocol version extraction (minVer, maxVer)
- SessionId extraction (32 bytes)
- Certificate data extraction from ServerHello

---

## [0.2.0] - 2026-01-17

### 🎉 TLS 1.3 Working!

### Added
- **TLS 1.3 connection with ChaCha20-Poly1305**
- ALPN negotiation for "smp/1"
- Cipher suite restriction to TLS 1.3 only
- SNI (Server Name Indication) support

---

## [0.1.0] - 2026-01-16

### Initial Release

### Added
- Project structure for ESP-IDF
- WiFi connection handling
- Basic TCP socket connection
- Initial mbedTLS integration

---

## Version Numbering

- **0.x.x** — Pre-release development
- **Major.Minor.Patch** — Standard semver after 1.0.0
- Internal versions (v3.3, v4.1) map to semver releases

### Internal to Semver Mapping

| Internal | Semver | Milestone |
|----------|--------|-----------|
| v1.0 | 0.1.0 | Initial structure |
| v2.0 | 0.2.0 | TLS 1.3 working |
| v3.0 | 0.3.0 | Handshake complete |
| v3.3 | 0.3.3 | PING test |
| v4.0 | 0.4.0 | NEW command working |
| v4.1 | 0.4.1 | SUB command working |
| - | 0.1.5-alpha | SEND + MSG |
| - | 0.1.6-alpha | E2E Encryption |
| - | 0.1.7-alpha | ACK command |
| - | 0.1.8-alpha | NVS Persistence |
| - | 0.1.9-alpha | DEL + Full SMP Client |

---

## Version History Summary

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.9-alpha** | **2026-01-20** | **🗑️ DEL + Full SMP Client!** |
| v0.1.8-alpha | 2026-01-20 | 🔑 NVS Persistence |
| v0.1.7-alpha | 2026-01-20 | 🎯 ACK Command |
| v0.1.6-alpha | 2026-01-20 | 🏆 E2E Decryption! |
| v0.1.5-alpha | 2026-01-20 | SEND + MSG receive |
| v0.4.1 | 2026-01-20 | SUB command |
| v0.4.0 | 2026-01-19 | NEW command (libsodium fix) |
| v0.3.0 | 2026-01-18 | Handshake (keyHash fix) |
| v0.2.0 | 2026-01-17 | TLS 1.3 |
| v0.1.0 | 2026-01-16 | Initial |

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
