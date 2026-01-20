# SimpleGo Development Guide

> Complete setup, build, and development instructions for SimpleGo

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Environment Setup](#environment-setup)
3. [Project Setup](#project-setup)
4. [Building & Flashing](#building--flashing)
5. [Development Workflow](#development-workflow)
6. [Debugging](#debugging)
7. [Code Structure](#code-structure)
8. [Testing](#testing)
9. [Troubleshooting](#troubleshooting)

---

## Prerequisites

### Hardware Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| **MCU Board** | Any ESP32-S3 | LilyGo T-Deck |
| **RAM** | 320KB SRAM | 8MB PSRAM |
| **Flash** | 4MB | 16MB |
| **USB** | USB-Serial | USB-OTG (native) |

#### Supported Boards

| Board | Status | Notes |
|-------|--------|-------|
| **LilyGo T-Deck** | ✅ Primary Target | Full feature support planned |
| **ESP32-S3-DevKitC** | ✅ Tested | Development and testing |
| **ESP32-S3-WROOM** | ✅ Compatible | Generic module |
| **ESP32 (original)** | ⚠️ Limited | Less RAM, no USB-OTG |

### Software Requirements

| Software | Version | Purpose |
|----------|---------|---------|
| **ESP-IDF** | 5.5.2+ | Build framework |
| **Python** | 3.8+ | ESP-IDF tools |
| **Git** | 2.x | Version control |
| **CMake** | 3.16+ | Build system |
| **Ninja** | 1.10+ | Build backend |

---

## Environment Setup

### Windows (Recommended)

#### 1. Install ESP-IDF

Download and run the [ESP-IDF Tools Installer](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/windows-setup.html):

```
https://dl.espressif.com/dl/esp-idf/
```

Select:
- ESP-IDF version: **5.5.2** (or latest 5.x)
- Installation directory: `C:\Espressif`
- Targets: **ESP32-S3** (required), ESP32 (optional)

#### 2. Verify Installation

Open **ESP-IDF 5.5 PowerShell** from Start Menu:

```powershell
# Check IDF version
idf.py --version
# Expected: ESP-IDF v5.5.2

# Check Python
python --version
# Expected: Python 3.x

# Check target support
idf.py --list-targets
# Should include: esp32s3
```

### Linux / macOS

#### 1. Install Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install git wget flex bison gperf python3 python3-pip \
    python3-venv cmake ninja-build ccache libffi-dev libssl-dev \
    dfu-util libusb-1.0-0

# macOS (Homebrew)
brew install cmake ninja dfu-util python3
```

#### 2. Clone ESP-IDF

```bash
mkdir -p ~/esp
cd ~/esp
git clone -b v5.5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
```

#### 3. Set Environment

```bash
# Add to ~/.bashrc or ~/.zshrc
alias get_idf='. $HOME/esp/esp-idf/export.sh'

# Activate for current session
source ~/esp/esp-idf/export.sh
```

---

## Project Setup

### 1. Create Project Directory

```powershell
# Windows
cd C:\Espressif\projects
mkdir simplex_client
cd simplex_client

# Linux/macOS
cd ~/esp
mkdir simplex_client
cd simplex_client
```

### 2. Initialize Project Structure

```
simplex_client/
├── CMakeLists.txt
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.c
├── sdkconfig.defaults
└── sdkconfig (generated)
```

### 3. Create CMakeLists.txt (Project Root)

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(simplex_client)
```

### 4. Create main/CMakeLists.txt

```cmake
# main/CMakeLists.txt
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        mbedtls
        libsodium
)
```

### 5. Create main/idf_component.yml

```yaml
# main/idf_component.yml
dependencies:
  espressif/libsodium: "^1.0.20"
```

### 6. Create sdkconfig.defaults

```ini
# sdkconfig.defaults

# Target
CONFIG_IDF_TARGET="esp32s3"

# Flash size
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y

# PSRAM (if available)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y

# WiFi
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=16
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=64
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=64

# mbedTLS for TLS 1.3
CONFIG_MBEDTLS_SSL_PROTO_TLS1_3=y
CONFIG_MBEDTLS_SSL_TLS1_3_COMPATIBILITY_MODE=y
CONFIG_MBEDTLS_TLS_CLIENT_ONLY=y

# Larger stack for crypto operations
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192

# Logging
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

### 7. Configure WiFi Credentials

Edit `main.c` and set your WiFi credentials:

```c
#define WIFI_SSID     "YourNetworkName"
#define WIFI_PASS     "YourPassword"
```

---

## Building & Flashing

### Standard Build Command

```powershell
# Windows (ESP-IDF PowerShell)
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5

# Linux/macOS
cd ~/esp/simplex_client
idf.py build flash monitor -p /dev/ttyUSB0
```

### Separate Commands

```powershell
# Build only
idf.py build

# Flash only
idf.py flash -p COM5

# Monitor only
idf.py monitor -p COM5

# Clean build
idf.py fullclean
idf.py build
```

### Finding Your COM Port

```powershell
# Windows - Device Manager or:
mode

# Linux
ls /dev/ttyUSB* /dev/ttyACM*

# macOS
ls /dev/cu.usb*
```

---

## Development Workflow

### Recommended Workflow

```
1. Edit main.c
2. Build: idf.py build
3. Flash + Monitor: idf.py flash monitor -p COM5
4. Observe output
5. Iterate
```

### Quick Iteration

For faster development, use the combined command:

```powershell
# Single command - builds, flashes, and monitors
idf.py build flash monitor -p COM5
```

### Monitor Controls

| Key | Action |
|-----|--------|
| `Ctrl+]` | Exit monitor |
| `Ctrl+T` | Menu |
| `Ctrl+T, H` | Help |
| `Ctrl+T, R` | Reset device |

---

## Debugging

### Serial Output

All debug output goes to serial console (USB):

```c
ESP_LOGI(TAG, "Info message");
ESP_LOGW(TAG, "Warning message");
ESP_LOGE(TAG, "Error message");
ESP_LOGD(TAG, "Debug message");  // Requires DEBUG level
```

### Enable Debug Logging

```powershell
idf.py menuconfig
# Navigate to: Component config → Log output → Default log verbosity
# Set to: Debug
```

### Hex Dump Helper

```c
// Print hex dump of buffer
void hex_dump(const char *label, const uint8_t *data, size_t len) {
    printf("%s (%d bytes): ", label, len);
    for (size_t i = 0; i < len && i < 64; i++) {
        printf("%02x", data[i]);
    }
    if (len > 64) printf("...");
    printf("\n");
}
```

---

## Troubleshooting

### Build Errors

#### "Component not found: libsodium"

```powershell
# Ensure idf_component.yml exists with dependency
# Run component manager
idf.py reconfigure
```

#### "mbedtls/ssl.h not found"

```powershell
# Check ESP-IDF installation
idf.py --version

# Reconfigure
idf.py fullclean
idf.py reconfigure
idf.py build
```

### Connection Errors

#### "DNS failed"

- Check WiFi credentials
- Verify internet connectivity
- Check server hostname

#### "TLS failed: -0x7780"

- TLS 1.3 not supported by server
- Check ALPN configuration
- Verify cipher suite

#### "ERR AUTH"

- Signature verification failed
- Ensure using libsodium (not Monocypher)
- Check sessionId in signed data
- For ACK: ensure using recipientId, not senderId

#### "ERR BLOCK"

- Block format incorrect
- Check 16KB padding
- Verify content length prefix

#### "ERR CMD SYNTAX"

- Command format incorrect
- Check parameter encoding
- For SEND: msgFlags must be ASCII 'T'/'F'

### Hardware Issues

#### "Failed to connect to ESP32-S3"

- Check USB cable (data, not charge-only)
- Try different USB port
- Hold BOOT button while connecting
- Check driver installation

#### "No serial output"

- Verify baud rate (115200)
- Check COM port selection
- Try `idf.py monitor` separately

---

## Additional Resources

### Documentation

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/)
- [mbedTLS Documentation](https://mbed-tls.readthedocs.io/)
- [libsodium Documentation](https://doc.libsodium.org/)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)

### Source Code References

- [simplexmq (Haskell)](https://github.com/simplex-chat/simplexmq) — Protocol reference
- [ESP-IDF Examples](https://github.com/espressif/esp-idf/tree/master/examples) — ESP-IDF patterns

---

*Last updated: January 20, 2026 — v0.1.7-alpha*
