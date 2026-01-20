# SimpleGo Development Guide

> Complete setup guide and development documentation

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Environment Setup](#environment-setup)
3. [Hardware Setup](#hardware-setup)
4. [Library Installation](#library-installation)
5. [Building & Flashing](#building--flashing)
6. [Architecture Overview](#architecture-overview)
7. [Development Workflow](#development-workflow)
8. [Debugging](#debugging)
9. [Testing](#testing)

---

## Prerequisites

### Hardware

| Component | Recommended | Notes |
|-----------|-------------|-------|
| **MCU** | ESP32-S3 | Dual-core, 8MB PSRAM preferred |
| **Dev Board** | LilyGo T-Deck or T-Embed | Display + input included |
| **USB Cable** | USB-C data cable | Not charge-only! |

### Software

| Component | Version | Notes |
|-----------|---------|-------|
| **ESP-IDF** | 5.5.2+ | Official Espressif framework |
| **Python** | 3.8+ | Required by ESP-IDF |
| **Git** | Any recent | Version control |
| **VS Code** | Optional | Recommended IDE |

---

## Environment Setup

### ESP-IDF Installation (Recommended)

#### Windows

1. **Download ESP-IDF Installer**
   - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html
   - Run `esp-idf-tools-setup-x.x.exe`

2. **Select Components**
   - ESP-IDF v5.5.2
   - All chip targets (or at least ESP32-S3)
   - Python installation

3. **Launch Environment**
   ```powershell
   # Use ESP-IDF PowerShell from Start Menu
   # Or run:
   C:\Espressif\idf_cmd_init.ps1
   ```

#### Linux / macOS

```bash
# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone -b v5.5.2 --recursive https://github.com/espressif/esp-idf.git

# Install tools
cd esp-idf
./install.sh esp32s3

# Set up environment (add to .bashrc/.zshrc)
. ~/esp/esp-idf/export.sh
```

#### Verify Installation

```bash
idf.py --version
# Should show: ESP-IDF v5.5.2
```

### Arduino IDE (Alternative - Legacy)

> **Note**: ESP-IDF is recommended for SimpleGo. Arduino setup documented for reference.

1. Download Arduino IDE 2.x from [arduino.cc](https://www.arduino.cc/en/software)
2. Install for your platform (Windows/Mac/Linux)

#### ESP32 Board Support (Manual Installation)

The Arduino Board Manager often times out downloading large ESP32 packages. Manual installation is more reliable:

**Windows (PowerShell):**
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

**Linux/Mac (Bash):**
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

### LilyGo T-Embed (Target Hardware)

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-S3 Dual Core @ 240MHz |
| Flash | 16MB |
| PSRAM | 8MB |
| Display | 1.9" LCD 170x320 (ST7789) |
| Input | Rotary Encoder with button |
| Form Factor | Compact |

### USB Driver (CP2102)

Download from Silicon Labs: https://www.silabs.com/documents/public/software/CP210x_Windows_Drivers.zip

---

## Library Installation

### ESP-IDF Dependencies

Managed via `idf_component.yml`:

```yaml
dependencies:
  espressif/libsodium: "^1.0.20"
```

Automatically downloaded on first build.

### Arduino Libraries (if using Arduino)

#### 1. Heltec ESP32 Library
```bash
cd ~/Arduino/libraries  # or Documents\Arduino\libraries on Windows
git clone https://github.com/HelTecAutomation/Heltec_ESP32.git
```

#### 2. Adafruit GFX (Dependency)

Install via Arduino Library Manager:
- Sketch → Include Library → Manage Libraries
- Search "Adafruit GFX" → Install

---

## Building & Flashing

### ESP-IDF (Recommended)

#### Set Target

```bash
idf.py set-target esp32s3
```

#### Build

```bash
idf.py build
```

#### Flash

```bash
# Windows (check COM port in Device Manager)
idf.py flash -p COM5

# Linux
idf.py flash -p /dev/ttyUSB0

# macOS
idf.py flash -p /dev/cu.usbserial-*
```

#### Monitor

```bash
idf.py monitor -p COM5  # or /dev/ttyUSB0
```

#### All-in-One

```bash
idf.py build flash monitor -p COM5
```

### Monitor Shortcuts

| Key | Action |
|-----|--------|
| `Ctrl+]` | Exit monitor |
| `Ctrl+T, R` | Reboot device |
| `Ctrl+T, H` | Help menu |
| `Ctrl+T, P` | Pause output |

### Arduino IDE (Alternative)

1. **Board:** "WiFi LoRa 32(V2)" (under Heltec ESP32 Series)
2. **Upload Speed:** 921600
3. **Flash Frequency:** 80MHz
4. **Port:** Select your COM port

#### Upload Issues

If upload fails, enter bootloader mode manually:
1. Hold **PRG/BOOT** button
2. Press **RST** button once
3. Release **PRG/BOOT**
4. Upload again

---

## Architecture Overview

### Crypto Stack

```
┌─────────────────────────────────────────┐
│           Application Layer             │
├─────────────────────────────────────────┤
│  libsodium (ESP-IDF Component)          │
│  ├── Ed25519 - Digital Signatures       │
│  ├── X25519 - ECDH Key Exchange         │
│  └── XSalsa20-Poly1305 - crypto_box     │
├─────────────────────────────────────────┤
│  mbedTLS (Built into ESP-IDF)           │
│  ├── SHA-256/512 - Hashing              │
│  ├── TLS 1.3 - Transport Security       │
│  └── ChaCha20-Poly1305 - TLS Cipher     │
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
│  ├── NEW, SUB, SEND, MSG, ACK, DEL      │
│  ├── 16KB Block Framing                 │
│  ├── Command Serialization              │
│  └── Queue Management                   │
├─────────────────────────────────────────┤
│         TLS 1.3 Layer                   │
│  └── mbedTLS (ChaCha20-Poly1305)        │
├─────────────────────────────────────────┤
│         TCP/IP Layer                    │
│  └── ESP32 WiFi Stack                   │
└─────────────────────────────────────────┘
```

### SimpleX Protocol Requirements

| Primitive | SimpleX Usage | Our Implementation |
|-----------|---------------|-------------------|
| Ed25519 | Command signatures | ✅ libsodium |
| X25519 | SMP key exchange | ✅ libsodium |
| XSalsa20-Poly1305 | SMP encryption | ✅ libsodium |
| SHA-256 | keyHash | ✅ mbedTLS |
| X448 | Double Ratchet DH | 📋 Planned |

---

## Development Workflow

### Project Structure

```
SimpleGo/
├── main/
│   ├── main.c              # Main application
│   ├── CMakeLists.txt      # Component config
│   └── idf_component.yml   # Dependencies (libsodium)
├── docs/
│   ├── DEVELOPMENT.md      # This file
│   ├── PROTOCOL.md         # SMP protocol details
│   ├── TECHNICAL.md        # Implementation notes
│   └── DEVNOTES.md         # Session notes
├── CMakeLists.txt          # Project config
├── sdkconfig.defaults      # Default settings
├── CHANGELOG.md
├── README.md
└── ROADMAP.md
```

### Configure WiFi

Edit `main/main.c`:

```c
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASS "YourPassword"
```

### Configure SMP Server (Optional)

Default server is `smp3.simplexonflux.com`. To change:

```c
#define SMP_HOST "your-server.com"
#define SMP_PORT "5223"
```

### Typical Development Cycle

```
1. Edit main/main.c
2. idf.py build
3. idf.py flash monitor -p COM5
4. Test
5. Repeat
```

### Clean Build

```bash
idf.py fullclean
idf.py build
```

### Menuconfig (SDK Settings)

```bash
idf.py menuconfig
```

Important settings:
- **Component config → mbedTLS** → Enable TLS 1.3
- **Component config → ESP-TLS** → Certificate bundle
- **Partition Table** → Custom (if needed)

---

## Debugging

### Log Levels

In `main.c`:
```c
esp_log_level_set("*", ESP_LOG_INFO);        // Default
esp_log_level_set("SMP", ESP_LOG_DEBUG);     // Verbose SMP
esp_log_level_set("mbedtls", ESP_LOG_WARN);  // Quiet TLS
```

### Common Issues

#### TLS Handshake Fails

```
E (1234) esp-tls-mbedtls: mbedtls_ssl_handshake returned -0x7780
```

**Fix**: Check WiFi connection, server hostname, or certificate issues.

#### ERR BLOCK

```
Server response: ERR BLOCK
```

**Fix**: Check block format. Commands need transmission headers.

#### ERR AUTH

```
Server response: ERR AUTH
```

**Fix**: 
- Using libsodium (not Monocypher)?
- Correct entityId for command?
- Signature includes `[0x20][sessionId]` prefix?

#### ERR NO_QUEUE

```
Server response: ERR NO_QUEUE
```

**Fix**: Queue doesn't exist. Either:
- No keys saved in NVS → need NEW command
- Server restarted → queue lost
- Queue was DEL'd
- Call `clear_saved_keys()` and restart to create new queue

#### DNS Resolution Fails

ESP32 DNS can be unreliable. Use direct IP addresses:
```c
IPAddress smpServer(172, 236, 211, 32);  // smp11.simplex.im
```

#### Port 5223 Blocked

Some networks block non-standard ports. Use port 443:
```c
const int smpPort = 443;  // Works on most networks
```

### Hex Dump Helper

```c
void hex_dump(const char *label, const uint8_t *data, size_t len) {
    ESP_LOGI("HEX", "%s (%d bytes):", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", data[i]);
        if ((i + 1) % 32 == 0) printf("\n");
    }
    printf("\n");
}
```

---

## Testing

### Manual Testing

1. **Build & Flash** — `idf.py build flash monitor`
2. **Watch for** — "QUEUE CREATED!" or "Keys loaded!"
3. **Send test message** — Use SimpleX mobile app to scan QR/link
4. **Check** — Message received and decrypted

### Reboot Test (NVS Persistence)

1. Start fresh (no keys saved)
2. Watch for "QUEUE CREATED!" and "NVS: Keys saved!"
3. Press `Ctrl+T, R` to reboot
4. Watch for "NVS: Keys loaded!" and "Skipping NEW"
5. Should go directly to SUB

### DEL Test (Queue Deletion)

1. Have an active queue (subscribed)
2. Call `delete_queue()` function
3. Watch for "Queue deleted from server!" and "NVS cleared!"
4. Reboot — should go to NEW (no saved keys)

### Reset Keys

To clear saved keys and start fresh:

```c
// In main.c, temporarily add at start:
clear_saved_keys();
```

Or via NVS erase:

```bash
idf.py erase-flash
idf.py flash monitor
```

### Test Servers

| Server | Location | Notes |
|--------|----------|-------|
| smp3.simplexonflux.com | EU | Default, reliable |
| smp1.simplexonflux.com | US | Alternative |
| smp4.simplexonflux.com | EU | Untested |
| Your own | Local | Run simplexmq server |

---

## Git Workflow

### Commit Style

```bash
git commit -m "type(scope): description"
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`

Examples:
```bash
git commit -m "feat(smp): implement DEL command"
git commit -m "fix(nvs): clear keys after DEL"
git commit -m "docs(protocol): add DEL command reference"
```

### Tagging Releases

```bash
git tag -a v0.1.9-alpha -m "DEL Command + Full SMP Client"
git push origin main --tags
```

---

## Useful Commands

```bash
# Check flash size
idf.py size

# Component sizes
idf.py size-components

# Open menuconfig
idf.py menuconfig

# Clean build
idf.py fullclean

# Erase all flash (including NVS!)
idf.py erase-flash

# Just erase NVS partition
parttool.py --port COM5 erase_partition --partition-name nvs
```

---

## VS Code Setup (Optional)

### Extensions

- ESP-IDF Extension (official)
- C/C++ Extension (Microsoft)

### settings.json

```json
{
    "idf.espIdfPath": "C:/Espressif/frameworks/esp-idf-v5.5.2",
    "idf.toolsPath": "C:/Espressif",
    "idf.port": "COM5"
}
```

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

---

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [ESP-IDF API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/)
- [mbedTLS Documentation](https://mbed-tls.readthedocs.io/)
- [libsodium Documentation](https://doc.libsodium.org/)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [Monocypher Documentation](https://monocypher.org/manual/)
- [Heltec ESP32 Docs](https://docs.heltec.org/en/node/esp32/)
- [LVGL Documentation](https://docs.lvgl.io/)

---

*Last updated: January 20, 2026 — v0.1.9-alpha*
