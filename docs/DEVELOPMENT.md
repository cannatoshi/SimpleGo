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
`ash
git clone https://github.com/cannatoshi/SimpleGo.git
cd SimpleGo
`

### ESP-IDF Installation

#### Windows
`powershell
# Download ESP-IDF installer from espressif.com
# Or manual installation:
mkdir C:\Espressif
cd C:\Espressif
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
.\install.ps1 esp32s3

# Activate environment (run in each new terminal)
C:\Espressif\esp-idf\export.ps1
`

#### Linux
`ash
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3

# Activate environment
source ~/esp/esp-idf/export.sh
`

#### macOS
`ash
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3

# Activate environment
source ~/esp/esp-idf/export.sh
`

---

## Project Structure
`
simplex_client/
├── main/                    # Application source code
│   ├── main.c               # Entry point
│   ├── smp_*.c              # Protocol modules
│   └── CMakeLists.txt       # Main component build file
├── include/                 # Header files
│   └── smp_*.h
├── components/              # Third-party libraries
│   ├── wolfssl/             # X448 cryptography
│   └── kyber/               # Post-quantum (future)
├── docs/                    # Documentation
├── CMakeLists.txt           # Project build file
├── partitions.csv           # Flash partition table
└── sdkconfig                # ESP-IDF configuration
`

---

## Building

### Configure Target
`ash
idf.py set-target esp32s3
`

### Build Project
`ash
idf.py build
`

### Flash to Device
`ash
# Windows
idf.py flash -p COM5

# Linux/macOS
idf.py flash -p /dev/ttyUSB0
`

### Monitor Serial Output
`ash
# Windows
idf.py monitor -p COM5

# Linux/macOS
idf.py monitor -p /dev/ttyUSB0
`

### Combined Command
`ash
idf.py build flash monitor -p COM5
`

---

## Configuration

### WiFi Credentials

Edit main/main.c:
`c
#define WIFI_SSID "your_network_name"
#define WIFI_PASS "your_password"
`

### SimpleX Server

Edit main/smp_network.c:
`c
#define SMP_SERVER_HOST "smp.example.com"
#define SMP_SERVER_PORT 5223
`

### ESP-IDF Configuration
`ash
idf.py menuconfig
`

Key settings:

| Path | Setting | Recommended |
|------|---------|-------------|
| Serial flasher config | Flash size | 4 MB |
| Component config → mbedTLS | TLS 1.3 | Enabled |
| Component config → ESP-TLS | Certificate verification | Enabled |
| Component config → FreeRTOS | Tick rate | 1000 Hz |

---

## Adding New Modules

### Step 1: Create Source File

Create main/smp_newmodule.c:
`c
#include "smp_newmodule.h"
#include "esp_log.h"

static const char *TAG = "SMP_NEWMODULE";

int smp_newmodule_init(void) {
    ESP_LOGI(TAG, "Initializing new module");
    return 0;
}
`

### Step 2: Create Header File

Create include/smp_newmodule.h:
`c
#ifndef SMP_NEWMODULE_H
#define SMP_NEWMODULE_H

#include <stdint.h>

int smp_newmodule_init(void);

#endif // SMP_NEWMODULE_H
`

### Step 3: Update CMakeLists.txt

Edit main/CMakeLists.txt:
`cmake
idf_component_register(
    SRCS 
        "main.c"
        "smp_newmodule.c"   # Add new file
        # ... other files
    INCLUDE_DIRS 
        "../include"
    REQUIRES 
        nvs_flash esp_wifi esp_netif mbedtls libsodium
)
`

### Step 4: Build and Test
`ash
idf.py build
`

---

## Code Style

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Functions | snake_case with prefix | smp_ratchet_init() |
| Variables | snake_case | chain_key |
| Constants | UPPER_SNAKE_CASE | SMP_MAX_MSG_SIZE |
| Types | snake_case with _t suffix | ratchet_state_t |
| Macros | UPPER_SNAKE_CASE | ESP_LOGI() |

### File Header
`c
/**
 * @file smp_module.c
 * @brief Brief description of module
 *
 * Detailed description of module functionality.
 *
 * @author Your Name
 * @date 2026-01-24
 */
`

### Function Documentation
`c
/**
 * @brief Brief description
 *
 * Detailed description.
 *
 * @param param1 Description of parameter
 * @param param2 Description of parameter
 * @return Description of return value
 */
int smp_function(int param1, const uint8_t *param2);
`

### Error Handling
`c
int smp_some_function(void) {
    int ret;
    
    ret = some_operation();
    if (ret != 0) {
        ESP_LOGE(TAG, "Operation failed: %d", ret);
        return ret;
    }
    
    return 0;
}
`

---

## Testing

### Manual Testing

1. Build and flash the firmware
2. Open serial monitor
3. Observe connection and message flow
4. Test with SimpleX mobile app

### Crypto Verification

Use Python scripts to verify cryptographic output:
`ash
cd tools
python3 verify_kdf.py
python3 verify_encryption.py
`

### Test with Local Server
`ash
# Run local SimpleX server (Docker)
docker run -d -p 5223:5223 simplexchat/smp-server

# Configure ESP32 to connect to local server
# Change SMP_SERVER_HOST to your local IP
`

---

## Debugging

### Log Levels
`c
// Set per-module log level
esp_log_level_set("SMP_NETWORK", ESP_LOG_DEBUG);
esp_log_level_set("SMP_RATCHET", ESP_LOG_VERBOSE);
`

| Level | Macro | Use Case |
|-------|-------|----------|
| Error | ESP_LOGE | Critical failures |
| Warning | ESP_LOGW | Unexpected but handled |
| Info | ESP_LOGI | Normal operation |
| Debug | ESP_LOGD | Development info |
| Verbose | ESP_LOGV | Detailed tracing |

### Hex Dump
`c
void hex_dump(const char *label, const uint8_t *data, size_t len) {
    ESP_LOGI(TAG, "%s (%d bytes):", label, (int)len);
    for (size_t i = 0; i < len; i += 16) {
        char line[64] = {0};
        for (size_t j = i; j < i + 16 && j < len; j++) {
            sprintf(line + strlen(line), "%02x ", data[j]);
        }
        ESP_LOGI(TAG, "  %04x: %s", (int)i, line);
    }
}
`

### GDB Debugging
`ash
# Terminal 1: Start OpenOCD
idf.py openocd

# Terminal 2: Start GDB
idf.py gdb
`

---

## Common Tasks

### Update wolfSSL
`ash
cd components/wolfssl
git pull origin master
# Reconfigure if needed
`

### Clean Build
`ash
idf.py fullclean
idf.py build
`

### Check Binary Size
`ash
idf.py size
idf.py size-components
`

### Generate Documentation
`ash
# If using Doxygen
doxygen Doxyfile
`

---

## Git Workflow

### Branch Naming

| Type | Format | Example |
|------|--------|---------|
| Feature | feat/description | feat/group-messaging |
| Bugfix | fix/description | fix/kdf-order |
| Documentation | docs/description | docs/crypto-guide |

### Commit Messages

Follow Conventional Commits:
`
type(scope): description

[optional body]

[optional footer]
`

Types: feat, fix, docs, refactor, test, chore

### Example Workflow
`ash
# Create feature branch
git checkout -b feat/new-feature

# Make changes
# ...

# Commit
git add .
git commit -s -m "feat(module): add new feature"

# Push
git push origin feat/new-feature

# Create pull request on GitHub
`

---

## Release Process

### Version Numbering

Format: 0.X.Y-alpha

| Component | Meaning |
|-----------|---------|
| 0 | Major version (pre-1.0) |
| X | Feature version |
| Y | Patch version |
| alpha | Pre-release status |

### Creating a Release

1. Update CHANGELOG.md
2. Update version in code
3. Create release commit
4. Tag release
5. Push to GitHub
`ash
# Update changelog and code
git add CHANGELOG.md main/main.c
git commit -m "chore(release): v0.1.16-alpha"

# Tag
git tag v0.1.16-alpha

# Push
git push origin main
git push origin v0.1.16-alpha
`

---

## Troubleshooting Development Issues

### Build Errors

| Error | Solution |
|-------|----------|
| Component not found | Run idf.py reconfigure |
| Header not found | Check include paths in CMakeLists.txt |
| Undefined reference | Add source file to CMakeLists.txt |
| Out of memory | Increase partition size or optimize code |

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
| Assertion failed | Check preconditions |

---

## Resources

### Documentation

- ESP-IDF: https://docs.espressif.com/projects/esp-idf/en/latest/
- wolfSSL: https://www.wolfssl.com/documentation/
- mbedTLS: https://tls.mbed.org/api/
- SimpleX: https://github.com/simplex-chat/simplexmq

### Community

- GitHub Issues: https://github.com/cannatoshi/SimpleGo/issues
- GitHub Discussions: https://github.com/cannatoshi/SimpleGo/discussions

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
