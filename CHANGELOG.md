# Changelog

All notable changes to SimpleGo are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- SEND command implementation
- Message reception handling
- ACK command for delivery confirmation
- Double Ratchet encryption layer

---

## [0.4.1] - 2025-01-20

### 🎉 Major Milestone: Full Queue Lifecycle Working!

This release marks a **historic achievement** — the first successful native SMP client implementation outside of the official Haskell codebase.

### Added
- **SUB command implementation** — Subscribe to created queues
- Queue subscription confirmation handling
- RecipientId storage for SUB command usage
- Complete NEW → IDS → SUB → OK flow

### Changed
- Version banner updated to v4.1

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

## [0.4.0] - 2025-01-19

### 🎉 Breakthrough: Queue Creation Working!

### Added
- **NEW command with successful IDS response** — First working queue creation!
- SPKI key encoding for Ed25519 and X25519 keys
- SubMode parameter ('S' for SMSubscribe) — Required for SMP v6
- Local signature verification before sending

### Fixed
- **Critical: Switched from Monocypher to libsodium** — Monocypher Ed25519 signatures are incompatible with SimpleX servers (crypton library)
- Signature format now matches server expectations

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

## [0.3.3] - 2025-01-19

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

## [0.3.2] - 2025-01-19

### Added
- Transport block format implementation
- 16KB block padding with '#' character
- Content length prefix (2 bytes, big-endian)

### Fixed
- Block size compliance (exactly 16384 bytes)
- Padding character (changed from 0x00 to '#')

---

## [0.3.1] - 2025-01-18

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

## [0.3.0] - 2025-01-18

### 🎉 Handshake Complete!

### Added
- **ClientHello with correct keyHash** — Handshake now succeeds!
- Certificate chain parsing (server cert + CA cert)
- SHA-256 hash of CA certificate for keyHash
- SMP v6 ClientHello format

### Fixed
- **Critical: keyHash must be computed from CA certificate (2nd in chain), not server certificate**

### Technical Discovery
```
Certificate chain in ServerHello:
  [0] Server certificate (online cert)
  [1] CA certificate ← Use this for keyHash!
  
keyHash = SHA256(full DER of CA certificate)
```

---

## [0.2.5] - 2025-01-18

### Added
- ServerHello parsing and validation
- Protocol version extraction (minVer, maxVer)
- SessionId extraction (32 bytes)
- Certificate data extraction from ServerHello

### Technical Details
```
ServerHello format:
  [minVer 2 bytes][maxVer 2 bytes]
  [sessIdLen 1 byte][sessionId 32 bytes]
  [certificate chain...]
```

---

## [0.2.0] - 2025-01-17

### 🎉 TLS 1.3 Working!

### Added
- **TLS 1.3 connection with ChaCha20-Poly1305**
- ALPN negotiation for "smp/1"
- Cipher suite restriction to TLS 1.3 only
- SNI (Server Name Indication) support

### Fixed
- TLS version enforcement (min and max set to TLS 1.3)
- Cipher suite configuration for SimpleX compatibility

### Technical Details
```
TLS Configuration:
  - Version: TLS 1.3 only
  - Cipher: TLS_CHACHA20_POLY1305_SHA256
  - ALPN: "smp/1"
  - SNI: smp3.simplexonflux.com
```

---

## [0.1.0] - 2025-01-16

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

---

## Links

- [GitHub Repository](https://github.com/cannatoshi/SimpleGo)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
