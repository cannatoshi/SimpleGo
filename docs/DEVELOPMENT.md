# SimpleGo Development Guide

> Complete setup guide and development documentation

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Environment Setup](#environment-setup)
3. [Hardware Setup](#hardware-setup)
4. [Building & Flashing](#building--flashing)
5. [Architecture Overview](#architecture-overview)
6. [Development Workflow](#development-workflow)
7. [Debugging](#debugging)
8. [Testing](#testing)

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

---

## Environment Setup

### ESP-IDF Installation

#### Windows

1. **Download ESP-IDF Installer**
   - https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/windows-setup.html

2. **Launch Environment**
   ```powershell
   # Use ESP-IDF PowerShell from Start Menu
   C:\Espressif\idf_cmd_init.ps1
   ```

#### Linux / macOS

```bash
mkdir -p ~/esp
cd ~/esp
git clone -b v5.5.2 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3

# Add to .bashrc/.zshrc
. ~/esp/esp-idf/export.sh
```

#### Verify Installation

```bash
idf.py --version
# Should show: ESP-IDF v5.5.2
```

---

## Hardware Setup

### LilyGo T-Deck (Target)

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-S3FN16R8 @ 240MHz |
| Flash | 16MB |
| PSRAM | 8MB |
| Display | 2.8" IPS LCD 320x240 (ST7789) |
| Keyboard | Physical QWERTY (I2C) |

### LilyGo T-Embed (Target)

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-S3 @ 240MHz |
| Display | 1.9" LCD 170x320 (ST7789) |
| Input | Rotary Encoder with button |

---

## Building & Flashing

### Set Target

```bash
idf.py set-target esp32s3
```

### Build

```bash
idf.py build
```

### Flash & Monitor

```bash
# Windows
idf.py build flash monitor -p COM5

# Linux
idf.py build flash monitor -p /dev/ttyUSB0
```

### Monitor Shortcuts

| Key | Action |
|-----|--------|
| `Ctrl+]` | Exit monitor |
| `Ctrl+T, R` | Reboot device |
| `Ctrl+T, H` | Help menu |

---

## Architecture Overview

### System Stack

```
┌─────────────────────────────────────────┐
│           Application Layer             │
├─────────────────────────────────────────┤
│  Invitation Links (v0.1.11+)            │
│  ├── SMP Queue URI Generation           │
│  ├── SimpleX Contact Link (Web)         │
│  ├── Direct App Link (simplex:/)        │
│  ├── Base64 Standard Encoding           │
│  └── URL Encoding (double for +/=)      │
├─────────────────────────────────────────┤
│  Contact Management (v0.1.10+)          │
│  ├── contacts_db_t (10 slots)           │
│  ├── add/remove/list_contacts()         │
│  ├── Message Routing (by recipientId)   │
│  └── NVS Blob Persistence               │
├─────────────────────────────────────────┤
│  Crypto Stack                           │
│  ├── Ed25519 (libsodium)                │
│  ├── X25519 (libsodium)                 │
│  ├── crypto_box (XSalsa20-Poly1305)     │
│  └── SHA-256 (mbedTLS HW)               │
├─────────────────────────────────────────┤
│  SMP Protocol Layer                     │
│  ├── NEW, SUB, SEND, MSG, ACK, DEL      │
│  ├── 16KB Block Framing                 │
│  └── Multi-Contact over one TLS         │
├─────────────────────────────────────────┤
│  Network Stack                          │
│  ├── TLS 1.3 (mbedTLS)                  │
│  ├── WiFi (ESP32)                       │
│  └── TCP/IP                             │
└─────────────────────────────────────────┘
```

### New Functions (v0.1.11)

```c
// Base64 Standard encoding (+ / = characters)
void base64_standard_encode(const uint8_t *input, size_t len, char *output);

// URL encoding with proper escaping
void url_encode(const char *input, char *output, size_t max_len);

// Generate and print all invitation link formats
void print_invitation_links(void);
```

---

## Development Workflow

### Project Structure

```
SimpleGo/
├── main/
│   ├── main.c              # Main application
│   ├── CMakeLists.txt
│   └── idf_component.yml   # Dependencies (libsodium)
├── docs/
│   ├── DEVELOPMENT.md      # This file
│   ├── PROTOCOL.md         # SMP protocol details
│   ├── TECHNICAL.md        # Implementation notes
│   └── DEVNOTES.md         # Session notes
├── CMakeLists.txt
├── sdkconfig.defaults
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

---

## Debugging

### Log Levels

```c
esp_log_level_set("*", ESP_LOG_INFO);
esp_log_level_set("SMP", ESP_LOG_DEBUG);
```

### Common Issues

#### TLS Handshake Fails

```
E (1234) esp-tls-mbedtls: mbedtls_ssl_handshake returned -0x7780
```
**Fix**: Check WiFi, server hostname, TLS 1.3 config.

#### ERR BLOCK

```
Server response: ERR BLOCK
```
**Fix**: Check block format — commands need transmission headers.

#### ERR CMD SYNTAX

```
Server response: ERR CMD SYNTAX
```
**Fix**: Check command format:
- NEW: Missing subMode? Add 'S'
- SEND: Binary flags? Use ASCII 'T'/'F'
- SEND: Missing space? Format is `SEND ' ' flags ' ' body`

#### ERR AUTH

```
Server response: ERR AUTH
```
**Fix**:
- Using libsodium (not Monocypher)?
- Correct entityId? (ACK/DEL use recipientId!)
- Signature includes `[0x20][sessionId]` prefix?

#### ERR NO_QUEUE

```
Server response: ERR NO_QUEUE
```
**Fix**: Queue doesn't exist. Clear NVS and create new queue.

#### E2E Decryption Fails

**Fix**: Use `crypto_box_beforenm()`, NOT raw `crypto_scalarmult()`!

```c
// ❌ WRONG
crypto_scalarmult(shared, secret, public);

// ✅ CORRECT (HSalsa20 key derivation)
crypto_box_beforenm(shared, public, secret);
```

#### Invitation Links Not Working

**Fix**: Check encoding:
- Base64 Standard (NOT base64url!) for DH key
- Double URL encode `+` and `=` in Base64 DH key
- Include `q=c` parameter for queue mode
- Version ranges: outer `v=2-7`, inner `v=1-4`

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

### Basic Connection Test

1. Build & Flash
2. Watch for "TLS OK! ALPN: smp/1"
3. Watch for "Subscriptions complete"

### Multi-Contact Test (v0.1.10+)

1. Start fresh (no contacts)
2. `add_contact("Test")` → Watch for "QUEUE CREATED!"
3. `add_contact("Test2")` → Second queue
4. Reboot (`Ctrl+T, R`)
5. Watch for "Loaded 2 contacts"
6. Watch for "Subscriptions complete: 2/2"

### Invitation Link Test (v0.1.11+)

1. Create a contact with `add_contact("Test")`
2. Watch for invitation links in output:
   ```
   🔗 SIMPLEX CONTACT LINKS ════════════════════════════════
   📱 [0] Test ──────────────────────────────────────────────
   📋 SMP Queue URI (raw):
      smp://1jne...@smp3.simplexonflux.com:5223/XLEV...#/?v=1-4&dh=MCow...&q=c
   
   🌐 SimpleX Contact Link (COPY THIS!):
      https://simplex.chat/contact#/?v=2-7&smp=smp%3A%2F%2F...
   ```
3. Copy the 🌐 Web Link
4. Open in browser → Should show SimpleX landing page
5. Open link in SimpleX Desktop/Mobile App
6. Click "Connect" in SimpleX App
7. Send a message from SimpleX App
8. Watch ESP32 output for:
   ```
   💬 MESSAGE for [Test]!
   🔓 DECRYPTED: <your message>
   ✅ ACK OK
   ```

### Self-Test (E2E Round-Trip)

The self-test sends a message to your own queue and verifies decryption:

```
🧪 SELF-TEST: Sending to [0] Test...
📤 SEND command sent!
💬 MESSAGE for [Test]!
🔓 DECRYPTED: Hello from ESP32!
✅ ACK OK
```

### NVS Tests

**Save Test:**
1. Add contacts
2. Watch for "NVS: Saved contacts_db"
3. Reboot
4. Watch for "NVS: Loaded X contacts"

**Clear Test:**
```c
// Temporarily add at start of main():
nvs_flash_erase();
```

Or:
```bash
idf.py erase-flash
```

### Test Servers

| Server | Location |
|--------|----------|
| smp3.simplexonflux.com | EU (default) |
| smp1.simplexonflux.com | US |

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
```

---

## Git Workflow

### Commit Style

```bash
git commit -m "type(scope): description"
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`

Examples:
```bash
git commit -m "feat(links): add SimpleX-compatible invitation links"
git commit -m "fix(encoding): double URL encode Base64 special chars"
```

### Tagging Releases

```bash
git tag -a v0.1.11-alpha -m "Invitation Links Working"
git push origin main --tags
```

---

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [libsodium Documentation](https://doc.libsodium.org/)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [LVGL Documentation](https://docs.lvgl.io/)

---

*Last updated: January 20, 2026 — v0.1.11-alpha*
