# Changelog

All notable changes to SimpleGo are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [0.1.15-alpha] - 2026-01-24

### Added

- Complete Double Ratchet encryption implementation
- X3DH key agreement protocol
- X448 key generation and Diffie-Hellman operations
- wolfSSL integration for Curve448 cryptography
- Root KDF function (HKDF-SHA512, info: "SimpleXRootRatchet")
- Chain KDF function (HKDF-SHA512, info: "SimpleXChainRatchet")
- AES-256-GCM encryption with 16-byte IV
- EncMessageHeader encoding (123 bytes)
- EncRatchetMessage encoding
- AgentConfirmation message building
- HELLO message building
- SMPQueueInfo encoding with proper queueMode handling
- Padding implementation (14832 bytes for AgentConfirmation, 15840 bytes for HELLO)
- New modules: smp_x448.c, smp_ratchet.c, smp_handshake.c, smp_queue.c
- Comprehensive cryptographic verification against Python reference
- Wire format verification against Haskell source code

### Fixed

- Bug #1: E2E key length encoding (Word16 to 1-byte)
- Bug #2: prevMsgHash length encoding (1-byte to Word16)
- Bug #3: MsgHeader DH key length encoding (Word16 to 1-byte)
- Bug #4: ehBody length encoding (Word16 to 1-byte)
- Bug #5: emHeader size (124 to 123 bytes)
- Bug #6: Payload AAD size (236 to 235 bytes)
- Bug #7: Root KDF output order (root, chain, next_header)
- Bug #8: Chain KDF IV order (header_iv before msg_iv)
- Bug #9: wolfSSL X448 byte-order reversal
- Bug #10: SMPQueueInfo port encoding (space to length prefix)
- Bug #11: smpQueues list count encoding (1-byte to Word16 BE)
- Bug #12: queueMode Nothing encoding (removed '0' byte)

### Changed

- Refactored cryptographic operations into dedicated modules
- Improved separation of concerns in protocol handling
- Enhanced error handling in encryption functions

### Verified

- X448 Diffie-Hellman: 100% match with Python reference
- HKDF-SHA512 (X3DH): 100% match with Python reference
- HKDF-SHA512 (Root KDF): 100% match with Python reference
- HKDF-SHA512 (Chain KDF): 100% match with Python reference
- AES-256-GCM encryption: 100% match with Python reference
- Server acceptance: OK response received

### Known Issues

- App compatibility: A_MESSAGE parsing error under investigation
- Hypothesis: Tail encoding issue with encConnInfo and emBody fields

---

## [0.1.14-alpha] - 2026-01-21

### Added

- Modular architecture refactoring
- Separated protocol logic into distinct modules
- New module structure with clear responsibilities
- Improved code organization

### Changed

- Moved from monolithic main.c to modular design
- Created separate files for peer, parser, network, crypto, contacts, utils
- Improved maintainability and testability

### Modules Created

| Module | Purpose |
|--------|---------|
| smp_peer.c | Peer connection management |
| smp_parser.c | Protocol message parsing |
| smp_network.c | TLS/TCP networking |
| smp_crypto.c | Ed25519, X25519 operations |
| smp_contacts.c | Contact address handling |
| smp_utils.c | Encoding utilities |

---

## [0.1.13-alpha] - 2026-01-19

### Added

- Improved SMP command handling
- Better error reporting
- Connection state management

### Fixed

- Various protocol encoding issues
- Connection stability improvements

---

## [0.1.12-alpha] - 2026-01-17

### Added

- Message sending functionality
- SEND command implementation
- Basic message encryption

### Fixed

- Queue subscription handling
- Server response parsing

---

## [0.1.11-alpha] - 2026-01-15

### Added

- Queue subscription (SUB command)
- Message receiving capability
- Basic notification handling

---

## [0.1.10-alpha] - 2026-01-13

### Added

- Queue creation (NEW command)
- Queue ID handling
- Server queue management

---

## [0.1.9-alpha] - 2026-01-11

### Added

- X25519 key exchange for per-queue encryption
- Improved key management
- Session key derivation

---

## [0.1.8-alpha] - 2026-01-09

### Added

- Ed25519 signature verification
- Server authentication
- Signature validation

---

## [0.1.7-alpha] - 2026-01-07

### Added

- Ed25519 signature generation
- libsodium integration
- Key pair management

---

## [0.1.6-alpha] - 2026-01-05

### Added

- SMP response parsing
- Protocol state machine
- Error handling for server responses

---

## [0.1.5-alpha] - 2026-01-03

### Added

- SMP command encoding
- Protocol message formatting
- Command serialization

---

## [0.1.4-alpha] - 2026-01-01

### Added

- Basic SMP handshake
- Protocol version negotiation
- Initial server communication

---

## [0.1.3-alpha] - 2025-12-30

### Added

- TLS 1.3 connection to SMP servers
- mbedTLS integration
- Certificate handling

---

## [0.1.2-alpha] - 2025-12-28

### Added

- TCP socket implementation
- Basic networking layer
- Connection management

---

## [0.1.1-alpha] - 2025-12-26

### Added

- WiFi connectivity
- Network initialization
- ESP-IDF networking stack

---

## [0.1.0-alpha] - 2025-12-24

### Added

- Initial project setup
- ESP-IDF project structure
- Basic build configuration
- CMakeLists.txt configuration
- Partition table setup

---

## Version Summary

| Version | Date | Highlights |
|---------|------|------------|
| 0.1.15-alpha | 2026-01-24 | Double Ratchet, X3DH, 12 bugs fixed |
| 0.1.14-alpha | 2026-01-21 | Modular architecture |
| 0.1.13-alpha | 2026-01-19 | Command handling improvements |
| 0.1.12-alpha | 2026-01-17 | Message sending |
| 0.1.11-alpha | 2026-01-15 | Queue subscription |
| 0.1.10-alpha | 2026-01-13 | Queue creation |
| 0.1.9-alpha | 2026-01-11 | X25519 key exchange |
| 0.1.8-alpha | 2026-01-09 | Signature verification |
| 0.1.7-alpha | 2026-01-07 | Ed25519 signatures |
| 0.1.6-alpha | 2026-01-05 | Response parsing |
| 0.1.5-alpha | 2026-01-03 | Command encoding |
| 0.1.4-alpha | 2026-01-01 | SMP handshake |
| 0.1.3-alpha | 2025-12-30 | TLS 1.3 |
| 0.1.2-alpha | 2025-12-28 | TCP sockets |
| 0.1.1-alpha | 2025-12-26 | WiFi connectivity |
| 0.1.0-alpha | 2025-12-24 | Project initialization |

---

## Links

- [ROADMAP.md](ROADMAP.md) - Development plan
- [docs/BUGS.md](docs/BUGS.md) - Detailed bug documentation
- [docs/release-info/](docs/release-info/) - Detailed release notes
