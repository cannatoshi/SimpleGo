# Changelog

All notable changes to SimpleGo are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [0.1.16-alpha] - 2026-01-25

### Added

- Hardware Abstraction Layer (HAL) architecture for multi-device support
- Seven HAL interface definitions:
  - hal_common.h: Base types, error codes, logging macros
  - hal_display.h: Display initialization, flush, backlight, LVGL integration
  - hal_input.h: Keyboard, touch, trackball, encoder abstraction
  - hal_storage.h: NVS key-value store and filesystem operations
  - hal_network.h: WiFi, Ethernet, network events
  - hal_audio.h: Speaker, buzzer, microphone, tone generation
  - hal_system.h: Power management, battery, watchdog, system info
- T-Deck Plus device configuration with complete pin mappings
- T-Deck Plus display HAL implementation (ST7789V driver with DMA)
- Kconfig-based device selection and build configuration
- Comprehensive documentation overhaul:
  - README.md: Complete rewrite for multi-platform ecosystem
  - ARCHITECTURE.md: Detailed HAL and system architecture
  - BUILD_SYSTEM.md: ESP-IDF build system explanation
  - ADDING_NEW_DEVICE.md: Guide for porting to new hardware
  - ROADMAP.md: Complete development roadmap

### Changed

- Project positioning from ESP32-only to multi-platform ecosystem
- Build system updated for HAL integration
- main/CMakeLists.txt: Added HAL include paths and dependencies

### Removed

- T-Embed CC1101 support (was temporary development device)

### Supported Devices

| Device | Status |
|--------|--------|
| LilyGo T-Deck Plus | Active Development |
| LilyGo T-Deck Pro | Planned |
| LilyGo T-Lora Pager | Planned |
| SimpleGo Secure (Tier 2) | Design Phase |
| SimpleGo Vault (Tier 3) | Planning |

---

## [0.1.15-alpha] - 2026-01-24

### Added

- Complete Double Ratchet encryption implementation
- X3DH key agreement protocol
- X448 key generation and Diffie-Hellman operations
- wolfSSL integration for Curve448 cryptography
- Root KDF function (HKDF-SHA512, info: SimpleXRootRatchet)
- Chain KDF function (HKDF-SHA512, info: SimpleXChainRatchet)
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

| Bug | Description | Fix |
|-----|-------------|-----|
| #1 | E2E key used Word16 | Changed to 1-byte prefix |
| #2 | prevMsgHash used 1-byte | Changed to Word16 BE |
| #3 | MsgHeader DH key used Word16 | Changed to 1-byte prefix |
| #4 | ehBody used Word16 prefix | Changed to 1-byte prefix |
| #5 | emHeader was 124 bytes | Corrected to 123 bytes |
| #6 | Payload AAD was 236 bytes | Corrected to 235 bytes |
| #7 | Root KDF output order wrong | Fixed split order |
| #8 | Chain KDF IV order swapped | Fixed: header_iv first |
| #9 | wolfSSL X448 keys reversed | Added byte reversal |
| #10 | SMPQueueInfo port used space | Changed to length prefix |
| #11 | smpQueues count was 1-byte | Changed to Word16 BE |
| #12 | queueMode Nothing sent 0 | Changed to send nothing |

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

---

## [0.1.12-alpha] - 2026-01-17

### Added

- Message sending functionality
- SEND command implementation

---

## [0.1.11-alpha] - 2026-01-15

### Added

- Queue subscription (SUB command)
- Message receiving capability

---

## [0.1.10-alpha] - 2026-01-13

### Added

- Queue creation (NEW command)
- Queue ID handling

---

## [0.1.9-alpha] - 2026-01-11

### Added

- X25519 key exchange for per-queue encryption

---

## [0.1.8-alpha] - 2026-01-09

### Added

- Ed25519 signature verification

---

## [0.1.7-alpha] - 2026-01-07

### Added

- Ed25519 signature generation
- libsodium integration

---

## [0.1.6-alpha] - 2026-01-05

### Added

- SMP response parsing
- Protocol state machine

---

## [0.1.5-alpha] - 2026-01-03

### Added

- SMP command encoding

---

## [0.1.4-alpha] - 2026-01-01

### Added

- Basic SMP handshake
- Protocol version negotiation

---

## [0.1.3-alpha] - 2025-12-30

### Added

- TLS 1.3 connection to SMP servers
- mbedTLS integration

---

## [0.1.2-alpha] - 2025-12-28

### Added

- TCP socket implementation

---

## [0.1.1-alpha] - 2025-12-26

### Added

- WiFi connectivity

---

## [0.1.0-alpha] - 2025-12-24

### Added

- Initial project setup
- ESP-IDF project structure

---

## Version Summary

| Version | Date | Highlights |
|---------|------|------------|
| 0.1.16-alpha | 2026-01-25 | HAL architecture, multi-device support, documentation |
| 0.1.15-alpha | 2026-01-24 | Double Ratchet, X3DH, 12 bugs fixed |
| 0.1.14-alpha | 2026-01-21 | Modular architecture |
| 0.1.13-alpha | 2026-01-19 | Command handling |
| 0.1.12-alpha | 2026-01-17 | Message sending |
| 0.1.11-alpha | 2026-01-15 | Queue subscription |
| 0.1.10-alpha | 2026-01-13 | Queue creation |
| 0.1.9-alpha | 2026-01-11 | X25519 |
| 0.1.8-alpha | 2026-01-09 | Signature verification |
| 0.1.7-alpha | 2026-01-07 | Ed25519 |
| 0.1.6-alpha | 2026-01-05 | Response parsing |
| 0.1.5-alpha | 2026-01-03 | Command encoding |
| 0.1.4-alpha | 2026-01-01 | SMP handshake |
| 0.1.3-alpha | 2025-12-30 | TLS 1.3 |
| 0.1.2-alpha | 2025-12-28 | TCP sockets |
| 0.1.1-alpha | 2025-12-26 | WiFi |
| 0.1.0-alpha | 2025-12-24 | Project init |

---

## Links

- [ROADMAP.md](ROADMAP.md) - Development plan
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) - System architecture
