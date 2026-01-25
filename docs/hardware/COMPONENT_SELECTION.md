# Component Selection Guide

> **Parent Document:** [HARDWARE_OVERVIEW.md](./HARDWARE_OVERVIEW.md)  
> **Version:** 0.1.0-draft

## Microcontrollers

### Tier 1: ESP32-S3-WROOM-1-N8R8 (~€4.50)

| Feature | Value |
|---------|-------|
| Core | Dual Xtensa LX7 @ 240 MHz |
| Memory | 8 MB Flash, 8 MB PSRAM |
| Security | Secure Boot v2, Flash Encryption |
| Wireless | WiFi 4 + BLE 5.0 |

**Pros:** Low cost, large community, hardware crypto  
**Cons:** No TrustZone, limited side-channel resistance

### Tier 2: STM32U585CIU6 (~€12)

| Feature | Value |
|---------|-------|
| Core | Cortex-M33 @ 160 MHz |
| Memory | 2 MB Flash, 786 KB SRAM |
| Security | TrustZone, OTFDEC, HUK, tamper pins |
| Crypto | AES, SHA, PKA (DPA resistant) |

**Pros:** TrustZone, PSA L3 ready, tamper inputs  
**Cons:** Higher cost, needs external WiFi

### Tier 3: STM32U5A9NJH6Q (~€18)

Same security as U585 with 4 MB Flash, 2.5 MB SRAM, GPU.

---

## Secure Elements

### ATECC608B (~€0.90) - All Tiers

| Feature | Value |
|---------|-------|
| Algorithms | ECC-P256, SHA-256, AES-128 |
| Key Slots | 16 |
| Interface | I²C |

**Use:** Identity key, TLS client, secure boot verification

### OPTIGA Trust M (~€3) - Tier 2+

| Feature | Value |
|---------|-------|
| Algorithms | ECC P-256/384/521, RSA-2048 |
| Certification | CC EAL6+ |
| Interface | Shielded I²C |

**Use:** Message keys, session management (different vendor)

### NXP SE050 (~€5) - Tier 3

| Feature | Value |
|---------|-------|
| Storage | 50 KB secure |
| Certification | CC EAL6+, FIPS 140-2 L3 (variant) |
| Interface | I²C + NFC provisioning |

**Use:** Backup/recovery, third vendor redundancy

---

## Displays

### LCD Options

| Part | Resolution | Price | Use Case |
|------|------------|-------|----------|
| ST7789V 2.4" | 320×240 | €8 | Tier 1 handheld |
| ILI9341 3.2" + Touch | 320×240 | €15 | Tier 2 handheld |
| 3.5" IPS | 480×320 | €20 | Tier 3 premium |

### E-Ink Options

| Part | Resolution | Price | Use Case |
|------|------------|-------|----------|
| Waveshare 2.9" | 296×128 | €18 | Tier 1 pager |
| GDEY042T81 4.2" | 400×300 | €28 | Tier 2-3 pager |

---

## Connectivity

| Module | Type | Price | Notes |
|--------|------|-------|-------|
| ESP32-C6-MINI-1 | WiFi 6 | €4 | Coprocessor for STM32 |
| Quectel BG95-M3 | LTE Cat-M | €18 | Global cellular |
| nRF9160 SiP | LTE-M + MCU | €22 | Integrated security |
| E22-900M30S | LoRa (SX1262) | €8 | Mesh, 10+ km range |

---

## Tamper Detection

| Component | Function | Price |
|-----------|----------|-------|
| DS3645 | Tamper supervisor (8 inputs) | €22 |
| APDS-9960 | Light + proximity sensor | €3 |
| 23LC1024 | Battery-backed SRAM | €2 |
| DS1321 | SRAM supervisor | €4 |

---

## Power

| Component | Function | Price |
|-----------|----------|-------|
| TP4056 | Basic Li-Ion charger | €0.50 |
| AXP2101 | Full PMIC + fuel gauge | €3 |
| Tadiran TL-5902 | 20-year backup battery | €8 |

---

## Sourcing Guidelines

### Authorized Distributors Only

- DigiKey, Mouser, Arrow, Farnell, RS Components

### Never Source From

- AliExpress (for production)
- eBay, unknown brokers

### Counterfeit Mitigation (Tier 3)

1. Visual inspection
2. Electrical test
3. X-ray inspection
4. Decap analysis (if critical)

---

## Quick Reference

```
MCUs:
  ESP32-S3-WROOM-1-N8R8      Tier 1
  STM32U585CIU6              Tier 2
  STM32U5A9NJH6Q             Tier 3

Secure Elements:
  ATECC608B-SSHDA-B          All tiers (primary)
  SLS32AIA010MH              Tier 2+ (OPTIGA)
  SE050E                     Tier 3 (NXP)

Tamper:
  DS3645+                    Tier 3 supervisor
  APDS-9960                  Light sensor
  23LC1024-I/SN              Battery-backed SRAM
```
