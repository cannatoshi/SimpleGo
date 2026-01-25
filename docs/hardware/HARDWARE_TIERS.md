# SimpleGo Hardware Tiers

> **Parent Document:** [HARDWARE_OVERVIEW.md](./HARDWARE_OVERVIEW.md)  
> **Version:** 0.1.0-draft

This document provides detailed specifications for each SimpleGo hardware tier.

---

## Tier 1: SimpleGo DIY (€100-200)

### Overview

Entry-level configuration for makers, developers, and enthusiasts. Basic security with software protections and single Secure Element.

### Core Components

| Component | Part | Price |
|-----------|------|-------|
| MCU | ESP32-S3-WROOM-1-N8R8 | €4.50 |
| Secure Element | ATECC608B-SSHDA-B | €0.90 |
| Display | ST7789V 2.4" IPS (320×240) | €8 |
| Input | BBQ20KBD compatible keyboard | €15 |
| Battery | 103450 LiPo 2000mAh | €8 |
| Charger | TP4056 module | €1 |

### Security Features

- ESP32-S3 Secure Boot v2 (RSA-3072)
- Flash Encryption (AES-256-XTS)
- ATECC608B for identity key storage
- JTAG disabled via eFuse
- No tamper detection (physical security only)

### E-Ink Pager Variant

| Component | Part | Price |
|-----------|------|-------|
| Display | Waveshare 2.9" E-Ink (296×128) | €18 |
| Input | Rotary encoder + 2 buttons | €3 |

**Total BOM:** €45-95 + PCB + Enclosure

---

## Tier 2: SimpleGo Secure (€400-600)

### Overview

Enhanced security for journalists, activists, and privacy-focused users. Features dual-vendor Secure Elements and active tamper detection.

### Core Components

| Component | Part | Price |
|-----------|------|-------|
| MCU | STM32U585CIU6 (Cortex-M33, TrustZone) | €12 |
| SE Primary | ATECC608B-SSHDA-B | €0.90 |
| SE Secondary | OPTIGA Trust M (CC EAL6+) | €3 |
| WiFi | ESP32-C6-MINI-1 (WiFi 6) | €4 |
| LoRa | E22-900M30S (SX1262) | €8 |
| Display | ILI9341 3.2" IPS + touch | €15 |
| PMIC | AXP2101 | €3 |

### Security Features

- STM32 TrustZone isolation
- DPA-resistant crypto accelerator
- Dual-vendor Secure Elements
- 6-layer PCB with security mesh
- Light sensor tamper detection
- Battery-backed SRAM for key zeroization
- Air-gap mode via QR codes

### Tamper System

| Component | Part | Price |
|-----------|------|-------|
| Light Sensor | APDS-9960 | €3 |
| BB-SRAM | 23LC1024 | €2 |
| Supervisor | DS1321 | €4 |

**Total BOM:** €190-250 + PCB Assembly + CNC Enclosure

---

## Tier 3: SimpleGo Vault (€1,000+)

### Overview

Maximum security for enterprise, government, and high-value targets. Designed to resist state-level adversaries with physical access.

### Core Components

| Component | Part | Price |
|-----------|------|-------|
| MCU | STM32U5A9NJH6Q (4MB Flash) | €18 |
| SE Primary | ATECC608B-TNGTLSS | €1.50 |
| SE Secondary | OPTIGA Trust M (EAL6+) | €4 |
| SE Tertiary | SE050E (FIPS 140-2 L3 capable) | €5 |
| Tamper Supervisor | Maxim DS3645 | €22 |

### Security Features

- Triple-vendor Secure Elements
- Full environmental monitoring (voltage, temperature, clock)
- Sub-microsecond key zeroization
- Active tamper mesh (flex PCB wrap)
- Potted enclosure (aluminum-filled epoxy)
- Duress mode and "Brick Me" PIN

### Connectivity Stack

| Component | Part | Price |
|-----------|------|-------|
| WiFi | ESP32-S3-MINI-1 | €4 |
| Cellular | nRF9160 (LTE-M, PSA L2) | €22 |
| LoRa | E22-900M30S | €8 |
| Satellite (optional) | Swarm M138 | €100 |

**Total BOM:** €560-1,070 + PCB Assembly + Potted CNC Enclosure

---

## Comparison Matrix

| Feature | Tier 1 | Tier 2 | Tier 3 |
|---------|--------|--------|--------|
| MCU | ESP32-S3 | STM32U585 | STM32U5A9 |
| TrustZone | No | Yes | Yes |
| Secure Elements | 1 | 2 | 3 |
| Tamper Detection | None | Basic | Full |
| PCB Layers | 4 | 6 | 8 |
| Enclosure | 3D Printed | CNC Aluminum | Potted CNC |
| Target Price | €100-200 | €400-600 | €1,000+ |
| Threat Model | Casual | Skilled | State-level |
