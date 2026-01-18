# SimpleGo

> Native SimpleX Chat client for LilyGo T-Deck & ESP32 hardware

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL%203.0-blue.svg)](LICENSE)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)
[![Status: Active Development](https://img.shields.io/badge/Status-Active%20Development-brightgreen.svg)]()

## 🎯 Vision

SimpleGo brings [SimpleX Chat](https://simplex.chat/) - the first messaging platform without user identifiers - to standalone hardware devices. No smartphone required, no cloud dependency, complete privacy in your pocket.

## 📊 Current Status

**Development Phase: Crypto Foundation ✅**

| Component | Status | Notes |
|-----------|--------|-------|
| WiFi + TLS | ✅ Working | TLS 1.3 via WiFiClientSecure |
| SMP Server Connection | ✅ Working | Port 443, TLS verified |
| X25519 Key Exchange | ✅ Working | 8ms via Monocypher |
| Ed25519 Signatures | ✅ Working | 8ms sign, 21ms verify |
| SHA-256/512 | ✅ Working | Hardware accelerated |
| AES-256-GCM | ✅ Working | 1ms, hardware accelerated |
| OLED Display UI | ✅ Working | SimpleX-style interface |
| SMP Protocol | 🔄 In Progress | Command structure built |
| Double Ratchet | 📋 Planned | Next milestone |
| LVGL Full UI | 📋 Planned | After protocol complete |

## 🔧 Hardware

### Currently Testing On
- **Heltec WiFi LoRa 32 V2** - ESP32 + OLED + LoRa (Development board)

### Target Hardware
| Device | Status | Features |
|--------|--------|----------|
| **LilyGo T-Deck** | 🎯 Primary Target | ESP32-S3, 2.8" LCD, Keyboard, 8MB PSRAM |
| **T-Deck Plus** | 🎯 Planned | + GPS, 2000mAh Battery |
| **Heltec LoRa 32** | ✅ Dev Board | ESP32, 0.96" OLED, LoRa |

## 🏗️ Architecture
```
┌─────────────────────────────────────────────────────────┐
│                    SimpleGo Client                      │
├─────────────────────────────────────────────────────────┤
│  UI Layer                                               │
│  └── OLED/LCD Display (LVGL planned)                    │
├─────────────────────────────────────────────────────────┤
│  Crypto Engine                     ✅ IMPLEMENTED       │
│  ├── X25519 (Monocypher)          - Key Exchange        │
│  ├── Ed25519 (Monocypher)         - Signatures          │
│  ├── AES-256-GCM (mbedTLS)        - Message Encryption  │
│  └── SHA-256/512 (mbedTLS)        - Hashing/HKDF        │
├─────────────────────────────────────────────────────────┤
│  SMP Protocol Layer                🔄 IN PROGRESS       │
│  ├── TLS 1.3 Transport            ✅ Working            │
│  ├── 16KB Block Framing           ✅ Working            │
│  └── Command Parser               🔄 Building           │
├─────────────────────────────────────────────────────────┤
│  Network Layer                     ✅ IMPLEMENTED       │
│  ├── WiFi (ESP32)                                       │
│  ├── TLS 1.3 (WiFiClientSecure)                         │
│  └── Tor (planned)                                      │
└─────────────────────────────────────────────────────────┘
```

## 🚀 Quick Start

### Prerequisites
- Arduino IDE 2.x
- Heltec ESP32 Board Support (manual install)
- Libraries: Monocypher, Heltec_ESP32, Adafruit_GFX

### Setup Guide
See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for detailed setup instructions.

### Build & Flash
```bash
# Open Arduino IDE
# Select Board: "WiFi LoRa 32(V2)" or your target
# Select Port: COMx / /dev/ttyUSBx
# Upload!
```

## 📁 Project Structure
```
SimpleGo/
├── src/
│   └── arduino/
│       └── simplex_dev_board/    # Main development sketches
├── docs/
│   ├── DEVELOPMENT.md            # Setup & build guide
│   ├── PROGRESS.md               # Development progress log
│   ├── dev-log/                  # Session notes
│   ├── architecture/             # System design docs
│   ├── protocol/                 # SMP protocol analysis
│   └── hardware/                 # Hardware specs & pinouts
├── tests/                        # Test sketches
└── tools/                        # Helper scripts
```

## 📈 Performance Benchmarks (ESP32 @ 240MHz)

| Operation | Time | Library |
|-----------|------|---------|
| X25519 KeyGen | ~8ms | Monocypher |
| X25519 DH | ~8ms | Monocypher |
| Ed25519 KeyGen | ~8ms | Monocypher |
| Ed25519 Sign | ~8ms | Monocypher |
| Ed25519 Verify | ~21ms | Monocypher |
| SHA-256 | <1ms | mbedTLS (HW) |
| AES-256-GCM | ~1ms | mbedTLS (HW) |
| TLS Connect | ~800ms | WiFiClientSecure |

## 🗺️ Roadmap

- [x] **Phase 1: Environment Setup** - Arduino IDE, Board Support
- [x] **Phase 2: Network Foundation** - WiFi, TLS, SMP Server Connection
- [x] **Phase 3: Crypto Foundation** - X25519, Ed25519, AES-GCM
- [ ] **Phase 4: SMP Protocol** - Commands, Queue Management
- [ ] **Phase 5: Double Ratchet** - E2E Encryption
- [ ] **Phase 6: Full UI** - LVGL, Touch, Keyboard
- [ ] **Phase 7: T-Deck Port** - Hardware migration
- [ ] **Phase 8: Tor Support** - Optional anonymity layer

## 🤝 Contributing

Contributions welcome! Please check [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for setup instructions.

## 📜 License

**GNU Affero General Public License v3.0** - See [LICENSE](LICENSE)

## 🙏 Acknowledgments

- [SimpleX Chat](https://simplex.chat/) - Protocol inspiration
- [Monocypher](https://monocypher.org/) - Excellent crypto library
- [LilyGo](https://lilygo.cc/) - T-Deck hardware
- [Heltec](https://heltec.org/) - Development boards
- [Espressif](https://www.espressif.com/) - ESP32 platform

---

<p align="center">
  <b>Privacy is not a privilege, it's a right.</b><br>
  <sub>Building the future of private communication.</sub>
</p>
