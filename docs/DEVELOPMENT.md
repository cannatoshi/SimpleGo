# SimpleGo Development Guide

> Complete setup guide and development documentation

## Table of Contents

1. [Environment Setup](#environment-setup)
2. [Hardware Setup](#hardware-setup)
3. [Library Installation](#library-installation)
4. [Building & Flashing](#building--flashing)
5. [Architecture Overview](#architecture-overview)
6. [Current Implementation Status](#current-implementation-status)
7. [Next Steps](#next-steps)

---

## Environment Setup

### Arduino IDE Installation

1. Download Arduino IDE 2.x from [arduino.cc](https://www.arduino.cc/en/software)
2. Install for your platform (Windows/Mac/Linux)

### ESP32 Board Support (Manual Installation)

The Arduino Board Manager often times out downloading large ESP32 packages. Manual installation is more reliable:

#### Windows (PowerShell)
```powershell
# Create hardware directory
mkdir "$env:USERPROFILE\Documents\Arduino\hardware"

# Clone Heltec ESP32 framework
cd "$env:USERPROFILE\Documents\Arduino\hardware"
git clone https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series.git

# Restructure for Arduino
mkdir heltec
move WiFi_Kit_series heltec\esp32

# Install toolchain
cd "$env:USERPROFILE\Documents\Arduino\hardware\heltec\esp32\tools"
.\get.exe
```

#### Linux/Mac (Bash)
```bash
# Create hardware directory
mkdir -p ~/Arduino/hardware/heltec

# Clone and setup
cd ~/Arduino/hardware/heltec
git clone https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series.git esp32

# Install toolchain
cd ~/Arduino/hardware/heltec/esp32/tools
python3 get.py
```

### USB Driver (CP2102)

Download from Silicon Labs: https://www.silabs.com/documents/public/software/CP210x_Windows_Drivers.zip

---

## Hardware Setup

### Heltec WiFi LoRa 32 V2 (Development Board)

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-D0WDQ6 Dual Core @ 240MHz |
| Flash | 4MB |
| SRAM | 520KB |
| Display | 0.96" OLED 128x64 (SSD1306) |
| LoRa | SX1276 (433/868/915MHz) |
| WiFi | 802.11 b/g/n 2.4GHz |
| USB | CP2102 USB-Serial |

**Pin Configuration:**
- OLED SDA: GPIO 4
- OLED SCL: GPIO 15
- OLED RST: GPIO 16
- LED: GPIO 25
- LoRa NSS: GPIO 18
- LoRa RST: GPIO 14
- LoRa DIO0: GPIO 26

### LilyGo T-Deck (Target Hardware)

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-S3FN16R8 Dual Core @ 240MHz |
| Flash | 16MB |
| PSRAM | 8MB |
| Display | 2.8" IPS LCD 320x240 (ST7789) |
| Keyboard | Integrated (ESP32-C3 controller) |
| Trackball | Yes |
| Touch | Capacitive |
| LoRa | Optional SX1262 |

---

## Library Installation

### Required Libraries

#### 1. Heltec ESP32 Library
```bash
cd ~/Arduino/libraries  # or Documents\Arduino\libraries on Windows
git clone https://github.com/HelTecAutomation/Heltec_ESP32.git
```

#### 2. Adafruit GFX (Dependency)

Install via Arduino Library Manager:
- Sketch → Include Library → Manage Libraries
- Search "Adafruit GFX" → Install

#### 3. Monocypher (Crypto)
```bash
cd ~/Arduino/libraries
git clone https://github.com/LoupVaillant/Monocypher.git

# Copy source files to root for Arduino compatibility
cp Monocypher/src/monocypher.h Monocypher/
cp Monocypher/src/monocypher.c Monocypher/

# Create library.properties
cat > Monocypher/library.properties << 'PROPS'
name=Monocypher
version=4.0.2
author=Loup Vaillant
sentence=Fast, secure crypto library
architectures=*
PROPS
```

---

## Building & Flashing

### Arduino IDE Settings

1. **Board:** "WiFi LoRa 32(V2)" (under Heltec ESP32 Series)
2. **Upload Speed:** 921600
3. **Flash Frequency:** 80MHz
4. **Port:** Select your COM port

### Upload Issues

If upload fails, enter bootloader mode manually:
1. Hold **PRG/BOOT** button
2. Press **RST** button once
3. Release **PRG/BOOT**
4. Upload again

### Serial Monitor

- Baud Rate: **115200**
- Press RST button to see boot messages

---

## Architecture Overview

### Crypto Stack
```
┌─────────────────────────────────────────┐
│           Application Layer             │
├─────────────────────────────────────────┤
│  Monocypher (Public Domain)             │
│  ├── X25519 - ECDH Key Exchange         │
│  ├── Ed25519 - Digital Signatures       │
│  └── ChaCha20-Poly1305 (available)      │
├─────────────────────────────────────────┤
│  mbedTLS (Built into ESP32)             │
│  ├── SHA-256/512 - Hashing              │
│  ├── AES-256-GCM - Symmetric Encryption │
│  └── HKDF - Key Derivation              │
├─────────────────────────────────────────┤
│  ESP32 Hardware                         │
│  ├── Hardware RNG (esp_random)          │
│  ├── SHA Hardware Acceleration          │
│  └── AES Hardware Acceleration          │
└─────────────────────────────────────────┘
```

### Network Stack
```
┌─────────────────────────────────────────┐
│         SMP Protocol Layer              │
│  ├── 16KB Block Framing                 │
│  ├── Command Serialization              │
│  └── Queue Management                   │
├─────────────────────────────────────────┤
│         TLS 1.3 Layer                   │
│  └── WiFiClientSecure (mbedTLS)         │
├─────────────────────────────────────────┤
│         TCP/IP Layer                    │
│  └── ESP32 WiFi Stack                   │
└─────────────────────────────────────────┘
```

### SimpleX Protocol Requirements

| Primitive | SimpleX Usage | Our Implementation |
|-----------|---------------|-------------------|
| X25519 | SMP crypto_box | ✅ Monocypher |
| Ed25519 | Command signatures | ✅ Monocypher |
| X448 | Double Ratchet DH | ⚠️ Needs wolfSSL |
| AES-256-GCM | Message encryption | ✅ mbedTLS |
| SHA-512 | HKDF | ✅ mbedTLS |
| XSalsa20-Poly1305 | SMP encryption | 📋 Planned |

**Note:** SimpleX uses Curve448 (X448) for Double Ratchet, not Curve25519. This requires wolfSSL with `HAVE_CURVE448` enabled, which needs custom compilation for ESP32.

---

## Current Implementation Status

### ✅ Completed (Phase 1-3)

1. **Development Environment**
   - Arduino IDE 2.3.7
   - Heltec ESP32 board support (manual install)
   - All required libraries

2. **Network Layer**
   - WiFi connection
   - TLS 1.3 (WiFiClientSecure)
   - SMP server connection (smp11.simplex.im:443)
   - 16KB block transmission

3. **Crypto Foundation**
   - X25519 key generation (~8ms)
   - X25519 Diffie-Hellman (~8ms)
   - Ed25519 key generation (~8ms)
   - Ed25519 sign (~8ms)
   - Ed25519 verify (~21ms)
   - SHA-256/512 (<1ms, HW accelerated)
   - AES-256-GCM (~1ms, HW accelerated)
   - Hardware RNG

4. **Display/UI**
   - OLED initialization
   - SimpleX-style interface
   - Signal strength bars
   - Status screens

### 🔄 In Progress (Phase 4)

1. **SMP Protocol**
   - Command structure defined
   - NEW command builder started
   - Need: Proper handshake sequence
   - Need: Response parsing

### 📋 Planned

1. **Phase 5: Double Ratchet**
   - X3DH key agreement
   - Ratchet state management
   - Message encryption/decryption

2. **Phase 6: Full UI (LVGL)**
   - Chat view
   - Contact list
   - Message input
   - QR code scanner

3. **Phase 7: T-Deck Port**
   - LCD driver (ST7789)
   - Keyboard input
   - Trackball navigation
   - Touch support

4. **Phase 8: Tor Support**
   - toresp32 integration
   - .onion address resolution

---

## Next Steps

### Immediate TODO

1. [ ] Study SMP protocol handshake sequence
2. [ ] Implement session establishment
3. [ ] Parse server responses
4. [ ] Create working NEW command
5. [ ] Receive queue confirmation

### Research Needed

- [ ] SMP binary protocol specification
- [ ] Session key derivation
- [ ] Transport encryption layer (post-TLS)
- [ ] Curve448 for Double Ratchet (wolfSSL compilation)

---

## Troubleshooting

### Board Not Recognized

1. Check USB cable (data cable, not charge-only)
2. Install CP2102 driver
3. Try different USB port

### Upload Timeout

1. Enter bootloader mode (PRG + RST)
2. Reduce upload speed to 115200
3. Check if another program uses the port

### Library Not Found

1. Restart Arduino IDE after installing libraries
2. Check library is in correct path
3. Verify library.properties exists

### DNS Resolution Fails

ESP32 DNS can be unreliable. Use direct IP addresses:
```cpp
IPAddress smpServer(172, 236, 211, 32);  // smp11.simplex.im
```

### Port 5223 Blocked

Some networks block non-standard ports. Use port 443:
```cpp
const int smpPort = 443;  // Works on most networks
```

---

## References

- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/tree/stable/protocol)
- [Monocypher Documentation](https://monocypher.org/manual/)
- [ESP32 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/)
- [Heltec ESP32 Docs](https://docs.heltec.org/en/node/esp32/)
- [LVGL Documentation](https://docs.lvgl.io/)

---

*Last Updated: January 19, 2026*
