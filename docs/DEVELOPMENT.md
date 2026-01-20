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

#### 3. Set Default Target

```powershell
# Set ESP32-S3 as default (one-time)
idf.py set-target esp32s3
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

### WSL Setup (Optional - For Haskell Analysis)

WSL is useful for analyzing the SimpleX Haskell source code:

```bash
# Install WSL2 with Ubuntu
wsl --install -d Ubuntu

# Inside WSL
sudo apt update
sudo apt install git grep ripgrep

# Clone SimpleX for reference
git clone https://github.com/simplex-chat/simplexmq.git
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

### 7. Copy main.c

Copy the SimpleGo v4.1 `main.c` to `main/main.c`.

### 8. Configure WiFi Credentials

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

### Build Options

```powershell
# Verbose build
idf.py build -v

# Parallel build (faster)
idf.py build -j 8

# Specific target
idf.py set-target esp32s3
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

### Menuconfig

Configure project settings via interactive menu:

```powershell
idf.py menuconfig
```

Important settings:
- **Component config → mbedTLS** — TLS settings
- **Component config → ESP-TLS** — TLS wrapper
- **Component config → Wi-Fi** — WiFi settings
- **Serial flasher config** — Flash settings

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

### Network Debugging

#### Wireshark SSL Key Logging

For TLS traffic analysis (advanced):

1. Set environment variable for key logging
2. Capture with Wireshark
3. Configure SSL key log file in Wireshark

Note: SimpleX uses TLS 1.3, which requires different capture techniques than TLS 1.2.

### GDB Debugging (Advanced)

```powershell
# Start GDB server
idf.py openocd

# In another terminal
idf.py gdb
```

---

## Code Structure

### main.c Organization

```c
// ============== CONFIG ==============
// WiFi credentials, server settings, constants

// ============== TCP Helpers ==============
// tcp_connect() - Establish TCP connection

// ============== mbedTLS I/O ==============
// my_send_cb(), my_recv_cb() - TLS callbacks

// ============== SMP Block I/O ==============
// read_exact() - Read exact bytes with timeout
// smp_read_block() - Read 16KB SMP block
// smp_write_handshake_block() - Write handshake format
// smp_write_command_block() - Write command format

// ============== Certificate Parsing ==============
// parse_cert_chain() - Extract certs from ServerHello

// ============== WiFi ==============
// wifi_event_handler() - WiFi events
// wifi_init() - Initialize WiFi

// ============== Main SMP Connection ==============
// smp_connect() - Main protocol logic

// ============== Entry Point ==============
// app_main() - FreeRTOS entry point
```

### Key Data Structures

```c
// Session state
uint8_t session_id[32];          // From ServerHello

// Recipient keys (for queue)
uint8_t rcv_auth_secret[64];     // Ed25519 secret (libsodium format)
uint8_t rcv_auth_public[32];     // Ed25519 public
uint8_t rcv_dh_secret[32];       // X25519 secret
uint8_t rcv_dh_public[32];       // X25519 public

// Queue IDs (from IDS response)
uint8_t recipient_id[24];        // For SUB command
uint8_t sender_id[24];           // For sender reference
```

### Protocol Flow

```
1. TCP Connect
2. TLS Handshake (ALPN: smp/1)
3. Receive ServerHello
4. Send ClientHello (with keyHash)
5. Generate keypairs
6. Send NEW command
7. Receive IDS response
8. Send SUB command
9. Receive OK
10. Ready for messaging
```

---

## Testing

### Unit Testing (Future)

```bash
# Run unit tests
idf.py test
```

### Integration Testing

Current testing is manual via serial output:

1. **TLS Test**: Verify "TLS OK! ALPN: smp/1"
2. **Handshake Test**: Verify ServerHello/ClientHello exchange
3. **NEW Test**: Verify "QUEUE CREATED!" message
4. **SUB Test**: Verify "SUBSCRIBED!" message

### Test Servers

| Server | Host | Status |
|--------|------|--------|
| SimpleX Flux 3 | smp3.simplexonflux.com:5223 | ✅ Tested |
| SimpleX Official | smp.simplex.im:5223 | ✅ Compatible |

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

#### "ERR BLOCK"

- Block format incorrect
- Check 16KB padding
- Verify content length prefix

#### "ERR CMD SYNTAX"

- Command format incorrect
- Check parameter encoding
- Verify subMode parameter present

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

### Community

- GitHub Issues — Bug reports and questions
- SimpleX Chat — For protocol questions

---

## Appendix: Haskell Source Analysis

When debugging protocol issues, the Haskell source is the authoritative reference.

### Useful grep Commands (WSL)

```bash
cd ~/simplexmq

# Find transmission encoding
grep -r "tEncodeAuth" --include="*.hs"

# Find signature format
grep -r "signSMP\|verifySMP" --include="*.hs"

# Find block format
grep -r "tPutBlock\|tGetBlock" --include="*.hs"

# Find command parsing
grep -r "NEW\|SUB\|SEND" src/Simplex/Messaging/Protocol.hs
```

### Key Files

| File | Purpose |
|------|---------|
| `Protocol.hs` | Command definitions |
| `Transport.hs` | Block framing |
| `Client.hs` | Client-side logic |
| `Crypto.hs` | Cryptographic operations |
