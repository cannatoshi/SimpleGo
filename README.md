# SimpleGo

Native SimpleX SMP Protocol Implementation for ESP32

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.1.15--alpha-green.svg)](CHANGELOG.md)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-orange.svg)](https://www.espressif.com/)

---

## Overview

SimpleGo is a native C implementation of the SimpleX Messaging Protocol (SMP) for ESP32 microcontrollers. The project enables private, decentralized messaging on dedicated hardware without requiring a smartphone.

The implementation includes the complete cryptographic stack: X3DH key agreement, Double Ratchet encryption, and the SMP wire protocol. All cryptographic operations have been verified against reference implementations.

---

## Project Goals

- Native SMP protocol implementation in C (not a CLI wrapper)
- End-to-end encrypted messaging using Double Ratchet algorithm
- Smartphone-free secure communication on embedded hardware
- Compatible with existing SimpleX infrastructure and clients

---

## Current Status: v0.1.15-alpha

### Completed Components

| Component | Description | Library |
|-----------|-------------|---------|
| TLS 1.3 | Secure connection to SMP servers | mbedTLS |
| SMP Handshake | Protocol negotiation and authentication | Custom |
| X3DH Key Agreement | Initial key establishment | wolfSSL (X448) |
| Double Ratchet | Forward-secure message encryption | Custom |
| Root KDF | Key derivation for ratchet steps | HKDF-SHA512 |
| Chain KDF | Per-message key derivation | HKDF-SHA512 |
| AES-256-GCM | Message encryption with 16-byte IV | mbedTLS |
| AgentConfirmation | Connection confirmation messages | Custom |
| HELLO Message | Initial contact message | Custom |

### In Progress

| Component | Status | Notes |
|-----------|--------|-------|
| App Compatibility | 40% | Investigating A_MESSAGE parsing error |

### Cryptographic Verification

All cryptographic operations verified against Python reference implementations:

| Operation | Verification Status |
|-----------|---------------------|
| X448 Diffie-Hellman | Verified |
| HKDF-SHA512 (X3DH) | Verified |
| HKDF-SHA512 (Root KDF) | Verified |
| HKDF-SHA512 (Chain KDF) | Verified |
| AES-256-GCM Encryption | Verified |
| Wire Format Encoding | Verified against Haskell source |

---

## Architecture

### Module Structure

| Module | Purpose | Lines |
|--------|---------|-------|
| smp_x448.c | X448 key generation and DH operations | ~200 |
| smp_ratchet.c | Double Ratchet state and KDF functions | ~500 |
| smp_handshake.c | E2E handshake and AgentConfirmation | ~300 |
| smp_queue.c | SMPQueueInfo encoding | ~250 |
| smp_peer.c | Peer connection management | ~400 |
| smp_parser.c | Protocol message parsing | ~350 |
| smp_network.c | TLS/TCP networking | ~300 |
| smp_crypto.c | Ed25519 signatures, X25519 | ~250 |
| smp_contacts.c | Contact address handling | ~200 |
| smp_utils.c | Encoding utilities | ~150 |

### Dependencies

| Component | Library | Purpose |
|-----------|---------|---------|
| TLS 1.3 | mbedTLS | Secure transport |
| X448/Curve448 | wolfSSL | DH key exchange |
| Ed25519 | libsodium | Digital signatures |
| X25519 | libsodium | Per-queue encryption |
| AES-GCM | mbedTLS | Symmetric encryption |

---

## Hardware Targets

| Device | Status | Description |
|--------|--------|-------------|
| LilyGo T-Deck | Primary | ESP32-S3 with keyboard and display |
| LilyGo T-Embed | Secondary | Compact form factor |
| Generic ESP32-S3 | Supported | PSRAM recommended |

---

## Building

### Prerequisites

- ESP-IDF 5.5.2 or newer
- Python 3.8+
- USB cable for flashing

### Build Commands

| Command | Description |
|---------|-------------|
| idf.py build | Compile project |
| idf.py flash -p COM5 | Flash to device |
| idf.py monitor -p COM5 | Serial monitor |
| idf.py build flash monitor -p COM5 | All combined |

---

## Project Structure

| Path | Description |
|------|-------------|
| main/ | Application source code |
| main/main.c | Entry point |
| main/smp_*.c | Protocol modules |
| include/ | Header files |
| include/smp_*.h | Module headers |
| components/wolfssl/ | X448 cryptography |
| components/kyber/ | Post-quantum (future) |
| docs/ | Documentation |
| CMakeLists.txt | Project build file |
| partitions.csv | Flash partition table |
| sdkconfig | ESP-IDF configuration |

---

## Documentation

| Document | Description |
|----------|-------------|
| [CHANGELOG.md](CHANGELOG.md) | Version history and release notes |
| [ROADMAP.md](ROADMAP.md) | Development plan and milestones |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Module structure and data flow |
| [docs/CRYPTO.md](docs/CRYPTO.md) | Cryptographic implementation details |
| [docs/WIRE_FORMAT.md](docs/WIRE_FORMAT.md) | Protocol encoding specification |
| [docs/BUGS.md](docs/BUGS.md) | Known issues and fixes |
| [docs/PROTOCOL.md](docs/PROTOCOL.md) | Protocol documentation |
| [docs/TECHNICAL.md](docs/TECHNICAL.md) | Technical details |
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | Development guide |

---

## Version History

| Version | Date | Highlights |
|---------|------|------------|
| v0.1.15-alpha | Jan 24, 2026 | Double Ratchet, X3DH, 12 bugs fixed |
| v0.1.14-alpha | Jan 21, 2026 | Modular architecture refactoring |
| v0.1.0-v0.1.13 | Dec 2025 | Initial development |

---

## Contributing

Contributions are welcome. Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## Security

For security vulnerabilities, please see [SECURITY.md](SECURITY.md).

---

## License

This project is licensed under the GNU Affero General Public License v3.0. See [LICENSE](LICENSE) for details.

---

## Disclaimer

SimpleGo is an independent project. It is not affiliated with, endorsed by, or connected with SimpleX Chat Ltd.

For trademark information, see [docs/TRADEMARK.md](docs/TRADEMARK.md).
