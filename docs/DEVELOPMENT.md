# SimpleGo Development Guide

> Setup, build, and development workflow for SimpleGo

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Environment Setup](#environment-setup)
3. [Project Setup](#project-setup)
4. [Build & Flash](#build--flash)
5. [Development Workflow](#development-workflow)
6. [Debugging](#debugging)
7. [Testing](#testing)

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

### Windows

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

### Linux / macOS

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

### Verify Installation

```bash
idf.py --version
# Should show: ESP-IDF v5.5.2
```

---

## Project Setup

### Clone Repository

```bash
cd ~/esp  # or C:\Espressif\projects on Windows
git clone https://github.com/cannatoshi/SimpleGo.git
cd SimpleGo
```

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

---

## Build & Flash

### Set Target

```bash
idf.py set-target esp32s3
```

### Build

```bash
idf.py build
```

### Flash

```bash
# Windows (check COM port in Device Manager)
idf.py flash -p COM5

# Linux
idf.py flash -p /dev/ttyUSB0

# macOS
idf.py flash -p /dev/cu.usbserial-*
```

### Monitor

```bash
idf.py monitor -p COM5  # or /dev/ttyUSB0
```

### All-in-One

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

---

## Development Workflow

### Typical Cycle

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
- Call `clear_saved_keys()` and restart to create new queue

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
| Your own | Local | Run simplexmq server |

---

## Dependencies

### libsodium

Managed via `idf_component.yml`:

```yaml
dependencies:
  libsodium:
    version: "^1.0.20"
```

Automatically downloaded on first build.

### mbedTLS

Built into ESP-IDF. Configured via menuconfig.

---

## Git Workflow

### Commit Style

```bash
git commit -m "type(scope): description"
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`

Examples:
```bash
git commit -m "feat(smp): implement ACK command"
git commit -m "fix(crypto): use libsodium for Ed25519"
git commit -m "docs(protocol): add version comparison"
git commit -m "feat(nvs): implement key persistence"
```

### Tagging Releases

```bash
git tag -a v0.1.8-alpha -m "NVS Key Persistence"
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

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [ESP-IDF API Reference](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/)
- [mbedTLS Documentation](https://mbed-tls.readthedocs.io/)
- [libsodium Documentation](https://doc.libsodium.org/)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)

---

*Last updated: January 20, 2026 — v0.1.8-alpha*
