# SimpleGo Roadmap

This document outlines the development plan for SimpleGo, covering software implementation, hardware development, and community milestones.

---

## Table of Contents

1. [Project Timeline](#project-timeline)
2. [Current Release](#current-release)
3. [Development Phases](#development-phases)
4. [Software Roadmap](#software-roadmap)
5. [Hardware Roadmap](#hardware-roadmap)
6. [Platform Support](#platform-support)
7. [Community Roadmap](#community-roadmap)
8. [Release Schedule](#release-schedule)

---

## Project Timeline

```
2025                                    2026                                    2027
  |                                       |                                       |
  Q4          Q1          Q2          Q3          Q4          Q1          Q2
  |           |           |           |           |           |           |
  +--[v0.1]---+--[v0.2]---+--[v0.3]---+--[v0.4]---+--[v1.0]---+--[v1.1]---+-->
  |           |           |           |           |           |           |
  Protocol    HAL         UI          Hardware    Stable      Enterprise
  Foundation  Architecture Complete   Integration Release     Features
```

| Version | Target | Milestone |
|---------|--------|-----------|
| v0.1.x | Q4 2025 | Protocol implementation, cryptographic verification |
| v0.2.x | Q1 2026 | HAL architecture, multi-device support |
| v0.3.x | Q2 2026 | User interface, contact management |
| v0.4.x | Q3 2026 | Hardware integration, Secure Element support |
| v1.0.0 | Q4 2026 | Stable release, security audit complete |
| v1.1.x | Q1 2027 | Enterprise features, Tier 3 hardware |

---

## Current Release

### Version 0.2.0-alpha (January 2026)

**Focus:** Hardware Abstraction Layer and Multi-Device Architecture

#### Completed

| Component | Status | Description |
|-----------|--------|-------------|
| HAL Interface Definitions | Complete | Seven interfaces covering all hardware aspects |
| T-Deck Plus Configuration | Complete | Pin mappings, display driver, device config |
| Build System Updates | Complete | Kconfig integration, device selection |
| Architecture Documentation | Complete | Comprehensive technical documentation |
| Protocol Stack | Complete | Full SMP implementation with Double Ratchet |
| Cryptographic Verification | Complete | All algorithms verified against reference |

#### In Progress

| Component | Status | Notes |
|-----------|--------|-------|
| Application Compatibility | 90% | Message parsing investigation ongoing |
| T-Deck Plus HAL Implementation | 30% | Display driver complete, input pending |
| User Interface | 0% | LVGL integration planned |

#### Verified Functionality

| Test | Result | Date |
|------|--------|------|
| TLS 1.3 Connection | Pass | January 2026 |
| SMP Handshake | Pass | January 2026 |
| X3DH Key Agreement | Pass | January 2026 |
| Double Ratchet Encryption | Pass | January 2026 |
| Server Message Acceptance | Pass | January 2026 |

---

## Development Phases

### Phase 1: Protocol Foundation (Complete)

Establish core SimpleX protocol functionality.

| Task | Status | Version |
|------|--------|---------|
| ESP-IDF project setup | Complete | v0.1.0 |
| WiFi connectivity | Complete | v0.1.1 |
| TCP socket implementation | Complete | v0.1.2 |
| TLS 1.3 with mbedTLS | Complete | v0.1.3 |
| SMP handshake protocol | Complete | v0.1.4 |
| SMP command encoding | Complete | v0.1.5 |
| SMP response parsing | Complete | v0.1.6 |
| Ed25519 signatures | Complete | v0.1.7-8 |
| X25519 key exchange | Complete | v0.1.9 |
| Queue operations (NEW, SUB, SEND, ACK) | Complete | v0.1.10-12 |
| Connection management | Complete | v0.1.13 |
| Modular architecture refactoring | Complete | v0.1.14 |
| X448 Diffie-Hellman | Complete | v0.1.15 |
| X3DH key agreement | Complete | v0.1.15 |
| Double Ratchet implementation | Complete | v0.1.15 |
| HKDF-SHA512 key derivation | Complete | v0.1.15 |
| AES-256-GCM encryption | Complete | v0.1.15 |
| Wire format verification | Complete | v0.1.15 |

### Phase 2: Hardware Abstraction (Current)

Enable multi-device support through HAL architecture.

| Task | Status | Version |
|------|--------|---------|
| HAL interface design | Complete | v0.2.0 |
| hal_common.h | Complete | v0.2.0 |
| hal_display.h | Complete | v0.2.0 |
| hal_input.h | Complete | v0.2.0 |
| hal_storage.h | Complete | v0.2.0 |
| hal_network.h | Complete | v0.2.0 |
| hal_audio.h | Complete | v0.2.0 |
| hal_system.h | Complete | v0.2.0 |
| T-Deck Plus device configuration | Complete | v0.2.0 |
| Build system with Kconfig | Complete | v0.2.0 |
| T-Deck Plus display HAL | Complete | v0.2.0 |
| T-Deck Plus input HAL | Planned | v0.2.1 |
| T-Deck Plus audio HAL | Planned | v0.2.1 |
| T-Deck Pro configuration | Planned | v0.2.2 |
| T-Lora Pager configuration | Planned | v0.2.3 |
| Desktop (SDL2) HAL for testing | Planned | v0.2.4 |

### Phase 3: User Interface (Planned)

Create complete user interface with LVGL.

| Task | Status | Target |
|------|--------|--------|
| LVGL integration | Planned | v0.3.0 |
| Display driver integration | Planned | v0.3.0 |
| Splash screen | Planned | v0.3.0 |
| Main menu screen | Planned | v0.3.0 |
| Contact list screen | Planned | v0.3.1 |
| Chat conversation screen | Planned | v0.3.1 |
| Message composition | Planned | v0.3.1 |
| Settings screen | Planned | v0.3.2 |
| QR code display and scanning | Planned | v0.3.2 |
| Connection status indicators | Planned | v0.3.2 |
| Keyboard input handling | Planned | v0.3.3 |
| Touch input handling | Planned | v0.3.3 |
| Theme system (dark/light) | Planned | v0.3.4 |
| Notification system | Planned | v0.3.4 |

### Phase 4: Hardware Integration (Planned)

Integrate hardware security features and custom hardware.

| Task | Status | Target |
|------|--------|--------|
| Secure Element HAL interface | Planned | v0.4.0 |
| ATECC608B driver | Planned | v0.4.0 |
| Hardware key storage | Planned | v0.4.1 |
| Secure boot configuration | Planned | v0.4.1 |
| Flash encryption setup | Planned | v0.4.2 |
| SimpleGo DIY board design | Planned | v0.4.2 |
| SimpleGo DIY prototype | Planned | v0.4.3 |
| OPTIGA Trust M driver (Tier 2) | Planned | v0.4.4 |
| Tamper detection system | Planned | v0.4.4 |

### Phase 5: Production Readiness (Planned)

Prepare for stable release.

| Task | Status | Target |
|------|--------|--------|
| Security audit preparation | Planned | v0.5.0 |
| External security audit | Planned | v0.5.1 |
| Audit remediation | Planned | v0.5.2 |
| Performance optimization | Planned | v0.5.3 |
| Memory optimization | Planned | v0.5.3 |
| Power management | Planned | v0.5.4 |
| OTA update support | Planned | v0.5.4 |
| Documentation completion | Planned | v0.5.5 |
| Beta testing program | Planned | v0.5.5 |
| Release candidate | Planned | v0.9.0 |
| Stable release | Planned | v1.0.0 |

---

## Software Roadmap

### Protocol Layer

| Feature | Priority | Status | Target |
|---------|----------|--------|--------|
| SMP client | Critical | Complete | v0.1.x |
| Double Ratchet | Critical | Complete | v0.1.15 |
| X3DH key agreement | Critical | Complete | v0.1.15 |
| Agent Protocol | Critical | 90% | v0.2.x |
| Group messaging | High | Planned | v0.4.x |
| File transfer (XFTP) | Medium | Planned | v0.5.x |
| Message delivery receipts | Medium | Planned | v0.3.x |
| Message history sync | Low | Planned | v1.1.x |

### Storage Layer

| Feature | Priority | Status | Target |
|---------|----------|--------|--------|
| NVS settings storage | High | Planned | v0.3.x |
| Contact persistence | High | Planned | v0.3.x |
| Message history | High | Planned | v0.3.x |
| Encrypted storage | Critical | Planned | v0.4.x |
| SD card support | Medium | Planned | v0.4.x |
| Backup and restore | Medium | Planned | v0.5.x |

### Connectivity

| Feature | Priority | Status | Target |
|---------|----------|--------|--------|
| WiFi connectivity | Critical | Complete | v0.1.x |
| Multiple server support | High | Planned | v0.3.x |
| Connection resilience | High | Planned | v0.3.x |
| Tor integration | Medium | Research | v0.5.x |
| LoRa mesh networking | Medium | Research | v1.1.x |
| 4G/5G module support | Medium | Planned | v0.4.x |
| Satellite connectivity | Low | Research | v1.2.x |

---

## Hardware Roadmap

### Tier 1: Development Platforms

Off-the-shelf devices for development and DIY users.

| Device | MCU | Status | Target |
|--------|-----|--------|--------|
| LilyGo T-Deck Plus | ESP32-S3 | In Development | v0.2.x |
| LilyGo T-Deck Pro | ESP32-S3 | Planned | v0.3.x |
| LilyGo T-Lora Pager | ESP32-S3 | Planned | v0.3.x |
| Raspberry Pi (Desktop Testing) | ARM Linux | Planned | v0.2.x |

### Tier 2: SimpleGo Secure

Custom hardware with enhanced security features.

| Milestone | Description | Target |
|-----------|-------------|--------|
| Component selection | Finalize MCU, SE, and peripheral selection | Q2 2026 |
| Schematic design | Complete circuit design | Q2 2026 |
| PCB layout | Security-focused 6-layer PCB | Q3 2026 |
| Prototype fabrication | First prototype boards | Q3 2026 |
| Firmware adaptation | HAL implementation for custom board | Q3 2026 |
| Enclosure design | CNC aluminum enclosure | Q4 2026 |
| Security testing | Penetration testing and review | Q4 2026 |
| Limited production | Small batch manufacturing | Q1 2027 |

**Tier 2 Specifications:**

| Component | Selection |
|-----------|-----------|
| MCU | STM32U585 (Cortex-M33, TrustZone) |
| Secure Element Primary | ATECC608B |
| Secure Element Secondary | OPTIGA Trust M |
| Connectivity | WiFi 6, LTE Cat-M (isolated), LoRa |
| Display | 3.2" IPS LCD with touch |
| Input | Physical keyboard |
| Tamper Detection | Light sensor, PCB mesh |
| Enclosure | CNC aluminum, IP54 |

### Tier 3: SimpleGo Vault

Maximum security hardware for high-risk users.

| Milestone | Description | Target |
|-----------|-------------|--------|
| Architecture design | Triple-SE security model | Q1 2027 |
| Component sourcing | Authorized distributors only | Q1 2027 |
| Schematic design | 8-layer security PCB | Q2 2027 |
| Tamper system design | Full environmental monitoring | Q2 2027 |
| Prototype fabrication | First prototype | Q3 2027 |
| Security audit | External professional audit | Q4 2027 |
| Enclosure design | Potted CNC aluminum | Q4 2027 |
| Certification | Relevant security certifications | 2028 |

**Tier 3 Specifications:**

| Component | Selection |
|-----------|-----------|
| MCU | STM32U5A9 (Cortex-M33, 4MB Flash) |
| Secure Element 1 | ATECC608B (Microchip) |
| Secure Element 2 | OPTIGA Trust M (Infineon) |
| Secure Element 3 | SE050 (NXP) |
| Tamper Supervisor | Maxim DS3645 |
| Connectivity | WiFi 6, 4G/5G (isolated), LoRa, Satellite |
| Enclosure | Potted aluminum, active mesh wrap |

---

## Platform Support

### Supported Devices

| Device | Form Factor | Display | Input | Status |
|--------|-------------|---------|-------|--------|
| LilyGo T-Deck Plus | Handheld | 320x240 LCD | Keyboard, Trackball, Touch | Active Development |
| LilyGo T-Deck Pro | Handheld | 320x240 LCD | Keyboard, Trackball, Touch | Planned |
| LilyGo T-Lora Pager | Pager | 128x64 OLED | Buttons, Encoder | Planned |
| SimpleGo Secure | Handheld | 320x240 LCD | Keyboard, Touch | Design Phase |
| SimpleGo Vault | Handheld | 320x240 LCD | Keyboard, Touch | Planning |
| Desktop (SDL2) | Desktop | Configurable | Keyboard, Mouse | Development Tool |

### Platform Comparison

| Feature | T-Deck Plus | T-Deck Pro | T-Lora Pager | SimpleGo Secure | SimpleGo Vault |
|---------|-------------|------------|--------------|-----------------|----------------|
| MCU | ESP32-S3 | ESP32-S3 | ESP32-S3 | STM32U585 | STM32U5A9 |
| Secure Element | None | None | None | Dual | Triple |
| TrustZone | No | No | No | Yes | Yes |
| Tamper Detection | No | No | No | Basic | Full |
| Cellular | No | No | No | LTE Cat-M | 4G/5G |
| LoRa | Optional | Optional | Yes | Yes | Yes |
| Target Tier | Tier 1 | Tier 1 | Tier 1 | Tier 2 | Tier 3 |
| Target Price | ~$60 | ~$80 | ~$50 | ~$500 | ~$1200 |

---

## Community Roadmap

### Documentation

| Milestone | Status | Target |
|-----------|--------|--------|
| README overhaul | Complete | v0.2.0 |
| Architecture documentation | Complete | v0.2.0 |
| Build system documentation | Complete | v0.2.0 |
| API documentation | Planned | v0.3.x |
| Hardware documentation | Complete | v0.2.0 |
| Contribution guidelines | Partial | v0.2.x |
| Security documentation | Partial | v0.2.x |
| User manual | Planned | v1.0.0 |

### Community Infrastructure

| Milestone | Status | Target |
|-----------|--------|--------|
| GitHub repository | Complete | v0.1.0 |
| Issue templates | Planned | v0.2.x |
| Pull request templates | Planned | v0.2.x |
| Discussion forums | Planned | v0.3.x |
| Discord/Matrix server | Planned | v0.3.x |
| Project website | Planned | v0.5.x |
| Documentation site | Planned | v0.5.x |

### Collaboration

| Milestone | Description | Target |
|-----------|-------------|--------|
| SimpleX team coordination | Protocol clarification and compatibility | Ongoing |
| Security researcher outreach | Invite review and feedback | v0.4.x |
| Hardware partner identification | Manufacturing partners | v0.4.x |
| Beta tester program | Recruit early adopters | v0.5.x |
| Security audit firm selection | Professional audit | v0.5.x |

---

## Release Schedule

### Near-term Releases

| Version | Target Date | Focus |
|---------|-------------|-------|
| v0.2.0-alpha | January 2026 | HAL architecture, documentation |
| v0.2.1-alpha | February 2026 | T-Deck Plus HAL completion |
| v0.2.2-alpha | February 2026 | Protocol compatibility fix |
| v0.3.0-alpha | March 2026 | LVGL UI integration |
| v0.3.1-alpha | April 2026 | Chat and contacts UI |
| v0.3.2-alpha | May 2026 | Settings and QR codes |

### Long-term Releases

| Version | Target Date | Focus |
|---------|-------------|-------|
| v0.4.0-alpha | Q3 2026 | Secure Element integration |
| v0.5.0-beta | Q4 2026 | Security audit preparation |
| v1.0.0 | Q4 2026 | Stable release |
| v1.1.0 | Q1 2027 | Enterprise features |
| v2.0.0 | 2027 | Tier 3 hardware support |

---

## Contributing

Development priorities are determined by:

1. Security impact
2. Core functionality requirements
3. Community feedback
4. Resource availability

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to participate.

---

## References

- [SimpleX Protocol Specification](https://github.com/simplex-chat/simplexmq)
- [Hardware Documentation](docs/hardware/HARDWARE_OVERVIEW.md)
- [Architecture Documentation](docs/ARCHITECTURE.md)
- [Changelog](CHANGELOG.md)

---

## Document History

| Version | Date | Changes |
|---------|------|---------|
| 0.2.0 | January 2026 | Complete rewrite for multi-platform ecosystem |
| 0.1.0 | December 2025 | Initial roadmap |
