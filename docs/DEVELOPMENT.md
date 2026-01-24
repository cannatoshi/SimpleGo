# SimpleGo Development Guide

Guide for developers who want to contribute to or build upon SimpleGo.

---

## Getting Started

### Prerequisites

| Requirement | Version | Notes |
|-------------|---------|-------|
| ESP-IDF | 5.5.2+ | Espressif IoT Development Framework |
| Python | 3.8+ | Build tools and verification scripts |
| Git | 2.30+ | Version control |
| Hardware | ESP32-S3 | T-Deck recommended for full testing |

### Clone Repository

Run: git clone https://github.com/cannatoshi/SimpleGo.git

---

## ESP-IDF Installation

### Windows

1. Create directory: C:\Espressif
2. Clone ESP-IDF: git clone --recursive https://github.com/espressif/esp-idf.git
3. Run installer: .\install.ps1 esp32s3
4. Activate: C:\Espressif\esp-idf\export.ps1

### Linux / macOS

1. Create directory: mkdir -p ~/esp
2. Clone ESP-IDF: git clone --recursive https://github.com/espressif/esp-idf.git
3. Run installer: ./install.sh esp32s3
4. Activate: source ~/esp/esp-idf/export.sh

---

## Project Structure

| Path | Description |
|------|-------------|
| main/ | Application source code |
| main/main.c | Entry point |
| main/smp_*.c | Protocol modules |
| main/CMakeLists.txt | Component build file |
| include/ | Header files |
| include/smp_*.h | Module headers |
| components/wolfssl/ | X448 cryptography library |
| components/kyber/ | Post-quantum crypto (future) |
| docs/ | Documentation |
| CMakeLists.txt | Project build file |
| partitions.csv | Flash partition table |
| sdkconfig | ESP-IDF configuration |

---

## Build Commands

| Command | Description |
|---------|-------------|
| idf.py set-target esp32s3 | Set target chip |
| idf.py build | Compile project |
| idf.py flash -p COM5 | Flash to device |
| idf.py monitor -p COM5 | Serial monitor |
| idf.py build flash monitor -p COM5 | All in one |
| idf.py clean | Clean build |
| idf.py fullclean | Full clean |
| idf.py menuconfig | Configuration menu |
| idf.py size | Binary size analysis |
| idf.py size-components | Component sizes |

---

## Configuration

### WiFi Credentials

Edit main/main.c and set:
- WIFI_SSID: Your network name
- WIFI_PASS: Your password

### SimpleX Server

Edit main/smp_network.c and set:
- SMP_SERVER_HOST: Server hostname
- SMP_SERVER_PORT: Server port (default 5223)

### ESP-IDF Settings

Run idf.py menuconfig to configure:

| Path | Setting | Value |
|------|---------|-------|
| Serial flasher config | Flash size | 4 MB |
| Component config - mbedTLS | TLS 1.3 | Enabled |
| Component config - ESP-TLS | Certificate verification | Enabled |
| Component config - FreeRTOS | Tick rate | 1000 Hz |

---

## Adding New Modules

### Step 1: Create Source File

Create main/smp_newmodule.c with your implementation.

### Step 2: Create Header File

Create include/smp_newmodule.h with function declarations.

### Step 3: Update CMakeLists.txt

Add the new .c file to SRCS in main/CMakeLists.txt.

### Step 4: Build and Test

Run: idf.py build

---

## Code Style

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Functions | snake_case with prefix | smp_ratchet_init() |
| Variables | snake_case | chain_key |
| Constants | UPPER_SNAKE_CASE | SMP_MAX_MSG_SIZE |
| Types | snake_case with _t | ratchet_state_t |

---

## Debugging

### Log Levels

| Level | Macro | Use Case |
|-------|-------|----------|
| Error | ESP_LOGE | Critical failures |
| Warning | ESP_LOGW | Unexpected but handled |
| Info | ESP_LOGI | Normal operation |
| Debug | ESP_LOGD | Development info |
| Verbose | ESP_LOGV | Detailed tracing |

### Set Log Level Per Module

In code: esp_log_level_set("SMP_NETWORK", ESP_LOG_DEBUG);

---

## Git Workflow

### Branch Naming

| Type | Format | Example |
|------|--------|---------|
| Feature | feat/description | feat/group-messaging |
| Bugfix | fix/description | fix/kdf-order |
| Documentation | docs/description | docs/crypto-guide |

### Commit Messages

Format: type(scope): description

Types: feat, fix, docs, refactor, test, chore

Example: feat(crypto): add X448 key generation

---

## Troubleshooting

### Build Errors

| Error | Solution |
|-------|----------|
| Component not found | Run idf.py reconfigure |
| Header not found | Check include paths |
| Undefined reference | Add source to CMakeLists.txt |

### Flash Errors

| Error | Solution |
|-------|----------|
| Failed to connect | Check USB cable and port |
| Wrong chip | Run idf.py set-target esp32s3 |
| Timeout | Hold BOOT button while flashing |

### Runtime Errors

| Error | Solution |
|-------|----------|
| Stack overflow | Increase task stack size |
| Heap exhausted | Check for memory leaks |

---

## Resources

### Documentation

- ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/latest/
- wolfSSL: https://www.wolfssl.com/documentation/
- mbedTLS: https://tls.mbed.org/api/
- SimpleX: https://github.com/simplex-chat/simplexmq

### Community

- Issues: https://github.com/cannatoshi/SimpleGo/issues
- Discussions: https://github.com/cannatoshi/SimpleGo/discussions

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
