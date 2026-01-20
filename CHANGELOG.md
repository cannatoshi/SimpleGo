# Changelog

All notable changes to SimpleGo are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- MSG decryption (X25519 DH + XSalsa20-Poly1305)
- ACK command implementation
- Key persistence in NVS
- Double Ratchet encryption

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

### Technical Discoveries
```
SEND Format (from Haskell Protocol.hs:1697):
  e (SEND_, ' ', flags, ' ', Tail msg)

MsgFlags Encoding (from Encoding.hs):
  True  = "T" (ASCII 0x54)
  False = "F" (ASCII 0x46)

Unsecured Queue (from Server.hs:1241):
  Queue without SKEY accepts SEND with authLen = 0
```

---

## [0.1.4-alpha] - 2026-01-20

### Added
- **SUB command implementation** — Subscribe to created queues
- SUB response parsing with transport format handling
- Queue subscription confirmation

### Technical Details
```
SUB Response Transport Format:
  [01]           = txCount
  [00 3f]        = txLen (63 bytes)
  [00]           = authLen (no signature)
  [20][32 bytes] = sessionId
  [01][corrId]   = corrId
  [18][24 bytes] = entityId (recipientId)
  [O][K]         = "OK" at position 64
```

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

### Technical Discovery
```
Ed25519 Library Incompatibility:
  - Monocypher and libsodium produce DIFFERENT signatures
  - Same input, same keys, different output!
  - SimpleX servers use crypton (libsodium-compatible)
  - Solution: Use ESP-IDF's libsodium component
```

---

## [0.1.2-alpha] - 2026-01-18

### 🎉 Handshake Complete!

### Added
- **ClientHello with correct keyHash** — Handshake succeeds!
- Certificate chain parsing (server cert + CA cert)
- SHA-256 hash computation for keyHash

### Fixed
- **CRITICAL: keyHash must use CA certificate (2nd in chain)**, not server certificate

### Technical Discovery
```
Certificate Chain in ServerHello:
  [0] Server certificate (online cert)
  [1] CA certificate ← Use THIS for keyHash!

keyHash = SHA256(full DER of CA certificate)
```

---

## [0.1.1-alpha] - 2026-01-17

### 🎉 TLS 1.3 Working!

### Added
- **TLS 1.3 connection with ChaCha20-Poly1305**
- ALPN negotiation for "smp/1"
- Cipher suite restriction to TLS 1.3 only
- SNI support

### Configuration
```
TLS Settings:
  - Version: TLS 1.3 only
  - Cipher: TLS_CHACHA20_POLY1305_SHA256
  - ALPN: "smp/1"
  - SNI: enabled
```

---

## [0.1.0-alpha] - 2026-01-16

### Initial Release

### Added
- Project structure for ESP-IDF
- WiFi connection handling
- Basic TCP socket connection
- Initial mbedTLS integration

### Technical Stack
- ESP-IDF 5.5.2
- ESP32-S3 target
- mbedTLS for TLS
- FreeRTOS for task management

---

## Version History Summary

| Version | Date | Milestone |
|---------|------|-----------|
| v0.1.5-alpha | 2026-01-20 | SEND + MSG receive |
| v0.1.4-alpha | 2026-01-20 | SUB command |
| v0.1.3-alpha | 2026-01-19 | NEW command (libsodium fix) |
| v0.1.2-alpha | 2026-01-18 | Handshake (keyHash fix) |
| v0.1.1-alpha | 2026-01-17 | TLS 1.3 |
| v0.1.0-alpha | 2026-01-16 | Initial |

---

## Links

- [GitHub Repository](https://github.com/cannatoshi/SimpleGo)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
