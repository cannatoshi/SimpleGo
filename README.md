# SimpleGo

**Native SimpleX Protocol Implementation for Dedicated Secure Communication Devices**

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)
[![Hardware: CERN-OHL-S-2.0](https://img.shields.io/badge/Hardware-CERN--OHL--S--2.0-green.svg)](docs/hardware/LICENSE)
[![Version](https://img.shields.io/badge/version-0.1.16--alpha-orange.svg)](CHANGELOG.md)
[![Platform](https://img.shields.io/badge/platform-Multi--MCU-lightgrey.svg)](#supported-platforms)

---

## Overview

SimpleGo is the first native implementation of the [SimpleX Messaging Protocol](https://github.com/simplex-chat/simplexmq) (SMP) outside the official Haskell codebase. Written in C for embedded systems, it enables secure end-to-end encrypted communication on dedicated hardware devices that operate independently of smartphones.

The project encompasses three components:

1. **Protocol Stack** — Complete SMP implementation including Double Ratchet encryption
2. **Hardware Abstraction Layer** — Unified interface supporting multiple hardware platforms
3. **Hardware Designs** — Open-source schematics for three security tiers

This is not another messaging application. It is an ecosystem for building purpose-built secure communication devices.

---

## Why Dedicated Hardware

Modern smartphones present fundamental security challenges that cannot be fully mitigated through software alone.

### The Smartphone Problem

| Concern | Smartphone Reality |
|---------|-------------------|
| Attack Surface | Approximately 50 million lines of code in Android/iOS |
| Baseband Processor | Closed-source firmware with direct memory access, always active |
| Background Processes | Hundreds of services running, many with network access |
| Telemetry | Continuous data collection by OS vendor and applications |
| Update Control | Vendor-controlled, can introduce vulnerabilities or backdoors |
| Physical Security | No tamper detection, keys stored in software or TEE |

Even hardened mobile operating systems such as GrapheneOS, while significantly improving the security posture, cannot address these architectural limitations. GrapheneOS provides excellent sandboxing, verified boot, and hardened memory allocation. However, it still runs on hardware with a closed-source baseband processor, and the trusted computing base remains large by necessity.

### The Dedicated Hardware Approach

SimpleGo takes a fundamentally different approach: minimize the trusted computing base.

| Aspect | Smartphone | SimpleGo Device |
|--------|------------|-----------------|
| Lines of Code | ~50,000,000 | ~50,000 |
| Baseband Processor | Yes (closed-source, DMA access) | None |
| Cellular Connectivity | Integrated, always-on baseband | Isolated module, data-only (Tier 2+) |
| Background Services | Hundreds | One |
| Telemetry | Built-in | None |
| Key Storage | Software/TEE | Hardware Secure Element |
| Tamper Detection | None | Active monitoring (Tier 2+) |
| Physical Profile | Obviously a phone | Appears as generic electronics |
| Cost | $500+ | $50-200 (Tier 1) |
| Disposability | Impractical | Designed for it |

**Regarding cellular connectivity:** Tier 2 and Tier 3 devices can include 4G LTE or 5G modules for mobile data connectivity. Unlike smartphone baseband processors, these modules are architecturally isolated from the main processor with no direct memory access. They function purely as data modems with communication occurring through a defined serial interface. The cellular module can be physically disabled or removed without affecting device operation over WiFi.

This approach eliminates entire categories of attacks. No browser means no browser exploits. No app installation means no malware vector. No baseband with DMA means no cellular-based memory attacks.

---

## Project Status

SimpleGo is under active development. The protocol implementation has reached a functional state with server acceptance confirmed.

### Protocol Implementation

| Component | Status | Notes |
|-----------|--------|-------|
| TLS 1.3 Transport | Complete | ALPN "smp/1" negotiation |
| SMP Handshake | Complete | Version negotiation verified |
| Queue Operations | Complete | CREATE, SUBSCRIBE, SEND, ACK |
| X3DH Key Agreement | Complete | Cryptographically verified |
| Double Ratchet | Complete | Forward secrecy implemented |
| AES-256-GCM Encryption | Complete | 16-byte IV, verified against reference |
| Wire Format Encoding | Complete | Matches Haskell implementation |
| Server Acceptance | Complete | Servers respond with OK |
| Application Compatibility | In Progress | Message parsing under investigation |

### Hardware Abstraction Layer

| Component | Status |
|-----------|--------|
| HAL Interface Definitions | Complete |
| Display Abstraction | Complete |
| Input Abstraction | Complete |
| Storage Abstraction | Complete |
| Network Abstraction | Complete |
| T-Deck Plus Implementation | In Progress |
| T-Embed CC1101 Implementation | Planned |

### Cryptographic Verification

All cryptographic operations have been verified byte-for-byte against Python reference implementations:

- X448 Diffie-Hellman key exchange
- HKDF-SHA512 key derivation (X3DH, Root KDF, Chain KDF)
- AES-256-GCM authenticated encryption
- Wire format encoding against Haskell source analysis

---

## Hardware Tiers

SimpleGo defines three hardware security tiers to address different threat models and budgets.

### Tier 1: DIY

Entry-level configuration for developers, makers, and privacy enthusiasts.

| Specification | Value |
|---------------|-------|
| Microcontroller | ESP32-S3 (Dual Xtensa LX7, 240 MHz) |
| Secure Element | ATECC608B (single) |
| Security Features | Secure Boot, Flash Encryption, eFuse Protection |
| Connectivity | WiFi 4, Bluetooth 5.0 |
| Target Price | EUR 100-200 |
| Threat Model | Protection against casual adversaries |

Suitable for: Personal use, development, learning, privacy-conscious individuals

### Tier 2: Secure

Enhanced security for users facing sophisticated threats.

| Specification | Value |
|---------------|-------|
| Microcontroller | STM32U585 (Cortex-M33 with TrustZone) |
| Secure Elements | Dual-vendor (ATECC608B + OPTIGA Trust M) |
| Security Features | TrustZone isolation, DPA-resistant crypto, PCB tamper mesh |
| Tamper Detection | Light sensor, battery-backed SRAM |
| Connectivity | WiFi 6, LTE Cat-M/NB-IoT (isolated), LoRa |
| Enclosure | CNC aluminum with security screws |
| Target Price | EUR 400-600 |
| Threat Model | Protection against skilled adversaries with equipment |

Suitable for: Journalists, activists, researchers, legal professionals

### Tier 3: Vault

Maximum security for high-value targets facing state-level threats.

| Specification | Value |
|---------------|-------|
| Microcontroller | STM32U5A9 (Cortex-M33, 4MB Flash) |
| Secure Elements | Triple-vendor (ATECC608B + OPTIGA Trust M + NXP SE050) |
| Tamper Supervisor | Maxim DS3645 (8 inputs, sub-microsecond zeroization) |
| Security Features | Full environmental monitoring, active tamper mesh wrap |
| Connectivity | WiFi 6, 4G LTE / 5G NR (isolated), LoRa, Satellite (optional) |
| Enclosure | Potted CNC aluminum (aluminum-filled epoxy) |
| Target Price | EUR 1000+ |
| Threat Model | Protection against state-level adversaries with physical access |

Suitable for: Enterprise, government, high-risk individuals

For detailed specifications, see [Hardware Tiers Documentation](docs/hardware/HARDWARE_TIERS.md).

---

## Architecture

SimpleGo employs a layered architecture with strict separation between platform-independent and platform-specific code.

```
+-----------------------------------------------------------------------+
|                         APPLICATION LAYER                             |
|                    User Interface, Screen Management                  |
+-----------------------------------------------------------------------+
|                          PROTOCOL LAYER                               |
|        SimpleX SMP, Agent Protocol, Double Ratchet, X3DH              |
+-----------------------------------------------------------------------+
|                   HARDWARE ABSTRACTION LAYER                          |
|      hal_display | hal_input | hal_network | hal_storage | hal_audio  |
+-----------------------------------------------------------------------+
|                      DEVICE IMPLEMENTATIONS                           |
+---------------+----------------+-------------------+-------------------+
|  T-Deck Plus  |  T-Deck Pro    |  SimpleGo Secure  |  Desktop (SDL2)  |
|  ESP32-S3     |  ESP32-S3      |  STM32 + Dual SE  |  Linux Testing   |
+---------------+----------------+-------------------+-------------------+
```

The Protocol Layer and Application Layer are identical across all devices. Only the HAL implementations differ, enabling a single codebase to support diverse hardware platforms.

### Directory Structure

```
simplex_client/
├── main/
│   ├── core/                 # Protocol implementation (device-independent)
│   ├── hal/                  # HAL interface headers
│   ├── include/              # Protocol headers
│   └── ui/                   # User interface (device-independent)
│
├── devices/
│   ├── t_deck_plus/          # LilyGo T-Deck Plus implementation
│   ├── t_deck_pro/           # LilyGo T-Deck Pro implementation
│   ├── t_lora_pager/         # LilyGo T-Lora Pager implementation
│   ├── simplego_secure/      # SimpleGo Tier 2 hardware
│   └── template/             # Template for new devices
│
├── components/               # External libraries
├── docs/                     # Documentation
│   └── hardware/             # Hardware design documentation
└── tools/                    # Build and provisioning tools
```

For complete architecture documentation, see [Architecture](docs/ARCHITECTURE.md).

---

## Supported Platforms

### LilyGo Development Devices (Tier 1)

Off-the-shelf devices for development and DIY users.

| Device | MCU | Display | Input | Status |
|--------|-----|---------|-------|--------|
| LilyGo T-Deck Plus | ESP32-S3 | 320x240 LCD | Keyboard, trackball, touch | Active Development |
| LilyGo T-Deck Pro | ESP32-S3 | 320x240 LCD | Keyboard, trackball, touch | Planned |
| LilyGo T-Lora Pager | ESP32-S3 | 128x64 OLED | Buttons, encoder | Planned |

### SimpleGo Custom Hardware (Tier 2 and 3)

Purpose-built secure communication devices.

| Device | MCU | Security | Status |
|--------|-----|----------|--------|
| SimpleGo Secure | STM32U585 | Dual Secure Element, TrustZone | Design Phase |
| SimpleGo Vault | STM32U5A9 | Triple Secure Element, Tamper Detection | Planning |

### Development Tools

| Platform | Purpose |
|----------|---------|
| Raspberry Pi / Desktop Linux | SDL2-based testing without hardware |

Adding support for new hardware requires implementing the HAL interfaces. See [Adding New Devices](docs/ADDING_NEW_DEVICE.md).

---

## Building

### Prerequisites

- ESP-IDF 5.5.2 or later
- Python 3.8 or later
- CMake 3.16 or later

### Quick Start

```bash
# Clone the repository
git clone https://github.com/nicokuehn-dci/simplego.git
cd simplego

# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh    # Linux/macOS
# or
%IDF_PATH%\export.bat             # Windows

# Configure (required for WiFi credentials)
idf.py menuconfig

# Build
idf.py build

# Flash and monitor
idf.py flash monitor -p /dev/ttyUSB0    # Linux
idf.py flash monitor -p COM6            # Windows
```

### Configuration

All settings are managed through the ESP-IDF menuconfig system:

```bash
idf.py menuconfig
```

Navigate to **SimpleGo Configuration** to access all project-specific settings.

#### Required Settings

| Menu | Setting | Description |
|------|---------|-------------|
| Network Configuration | WiFi SSID | Your WiFi network name |
| Network Configuration | WiFi Password | Your WiFi password |

#### Available Configuration Menus

| Menu | Settings |
|------|----------|
| **Device Selection** | T-Deck Plus, T-Deck Pro, T-Lora Pager, Raspberry Pi, Custom |
| **SimpleGo Core** | Max contacts (50), max messages (100), message length, auto-reconnect |
| **Network Configuration** | WiFi SSID, WiFi password, SMP server, SMP port, TLS verification |
| **UI Configuration** | Theme (dark/light/auto), animations, typing indicator, read receipts |
| **Security Configuration** | PIN required, PIN length, auto-lock timeout, wipe on failed attempts |
| **Power Management** | Sleep enable, light/deep sleep timeout, battery saver level |
| **Debug Options** | Debug logging, log crypto operations, log network traffic |

Note: The `sdkconfig` file contains your personal settings (including WiFi credentials) and is excluded from version control via `.gitignore`.

For detailed build instructions, see [Build System Documentation](docs/BUILD_SYSTEM.md).

---

## Documentation

### Core Documentation

| Document | Description |
|----------|-------------|
| [Architecture](docs/ARCHITECTURE.md) | System architecture and HAL design |
| [Build System](docs/BUILD_SYSTEM.md) | Build configuration and device selection |
| [Security Model](docs/SECURITY_MODEL.md) | Threat model and security architecture |
| [Technical Reference](docs/TECHNICAL.md) | Low-level technical details |
| [Protocol](docs/PROTOCOL.md) | SimpleX protocol implementation details |
| [Cryptography](docs/CRYPTO.md) | Cryptographic implementation |
| [Wire Format](docs/WIRE_FORMAT.md) | Protocol message encoding |
| [Adding New Devices](docs/ADDING_NEW_DEVICE.md) | Guide for porting to new hardware |

### Hardware Documentation

| Document | Description |
|----------|-------------|
| [Hardware Overview](docs/hardware/HARDWARE_OVERVIEW.md) | Hardware design philosophy |
| [Hardware Tiers](docs/hardware/HARDWARE_TIERS.md) | Tier specifications |
| [Security Architecture](docs/hardware/SECURITY_ARCHITECTURE.md) | Hardware security model |
| [HAL Architecture](docs/hardware/HAL_ARCHITECTURE.md) | Hardware abstraction design |
| [Component Selection](docs/hardware/COMPONENT_SELECTION.md) | Component specifications |
| [PCB Design](docs/hardware/PCB_DESIGN.md) | PCB layout guidelines |
| [Enclosure Design](docs/hardware/ENCLOSURE_DESIGN.md) | Mechanical design |

### Project Management

| Document | Description |
|----------|-------------|
| [Roadmap](ROADMAP.md) | Development plan |
| [Changelog](CHANGELOG.md) | Version history |
| [Contributing](CONTRIBUTING.md) | Contribution guidelines |
| [Security Policy](SECURITY.md) | Vulnerability reporting |

---

## Contributing

SimpleGo is a community-driven project. Contributions are welcome in several areas:

- Protocol implementation and bug fixes
- HAL implementations for new hardware platforms
- Documentation improvements
- Security analysis and review
- Hardware design contributions
- Testing and verification

Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting pull requests.

### Code of Conduct

This project adheres to a code of conduct. By participating, you agree to uphold respectful and constructive communication. See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

---

## Security

### Reporting Vulnerabilities

Security vulnerabilities should be reported privately. Do not open public issues for security concerns.

See [SECURITY.md](SECURITY.md) for reporting procedures.

### Security Audits

The codebase has not yet undergone formal security audit. This is planned for post-v1.0 release. Users should consider this when evaluating the software for sensitive applications.

---

## License

This project uses dual licensing:

| Component | License |
|-----------|---------|
| Software | [AGPL-3.0](LICENSE) |
| Hardware Designs | [CERN-OHL-S-2.0](docs/hardware/LICENSE) |

The AGPL-3.0 license is required for compatibility with the SimpleX protocol libraries.

---

## Acknowledgments

- [SimpleX Chat](https://simplex.chat/) for creating the SimpleX protocol
- [Espressif](https://www.espressif.com/) for ESP-IDF and ESP32 platform
- [wolfSSL](https://www.wolfssl.com/) for embedded cryptography libraries
- [LVGL](https://lvgl.io/) for embedded graphics library

---

## Disclaimer

SimpleGo is an independent project and is not affiliated with, endorsed by, or connected to SimpleX Chat Ltd. The SimpleX name and protocol are used for interoperability purposes only.

This software is provided as-is, without warranty. Users are responsible for evaluating its suitability for their security requirements. See [docs/DISCLAIMER.md](docs/DISCLAIMER.md) for full legal notices.

---

## Contact

- GitHub Issues: Bug reports and feature requests
- GitHub Discussions: General questions and community discussion

---

*SimpleGo — Secure messaging without smartphones.*
