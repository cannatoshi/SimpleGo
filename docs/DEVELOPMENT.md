# SimpleGo Development Guide

> Complete setup guide and development documentation

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Environment Setup](#environment-setup)
3. [Building & Flashing](#building--flashing)
4. [Architecture Overview](#architecture-overview)
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

### System Stack (v0.1.12)

```
┌─────────────────────────────────────────────────────────────────┐
│           Application Layer                                     │
├─────────────────────────────────────────────────────────────────┤
│  Agent Protocol Layer                           ✅ NEW!         │
│  ├── parse_agent_message()                                      │
│  ├── AgentInvitation Parser (Type 'I')                          │
│  ├── Reply Queue URI Extraction                                 │
│  └── Peer Profile Parsing                                       │
├─────────────────────────────────────────────────────────────────┤
│  Message Decryption Stack                                       │
│  ├── Layer 3: SMP E2E (server DH)                               │
│  ├── Layer 5: Contact DH (decrypt_client_msg())   ✅ NEW!       │
│  └── Layer 6: Agent Protocol Parsing              ✅ NEW!       │
├─────────────────────────────────────────────────────────────────┤
│  Invitation Links                                               │
│  ├── Base64URL Encoding                                         │
│  └── Double-encoded = padding (%253D)                           │
├─────────────────────────────────────────────────────────────────┤
│  Contact Management                                             │
│  ├── contacts_db_t (10 slots)                                   │
│  ├── add/remove/list_contacts()                                 │
│  └── NVS Blob Persistence                                       │
├─────────────────────────────────────────────────────────────────┤
│  Crypto Stack                                                   │
│  ├── Ed25519 (libsodium)                                        │
│  ├── X25519 (libsodium)                                         │
│  ├── crypto_box (XSalsa20-Poly1305)                             │
│  └── SHA-256 (mbedTLS HW)                                       │
├─────────────────────────────────────────────────────────────────┤
│  SMP Protocol Layer                                             │
│  ├── NEW, SUB, SEND, MSG, ACK, DEL                              │
│  ├── 16KB Block Framing                                         │
│  └── Multi-Contact over one TLS                                 │
├─────────────────────────────────────────────────────────────────┤
│  Network Stack                                                  │
│  ├── TLS 1.3 (mbedTLS)                                          │
│  ├── WiFi (ESP32)                                               │
│  └── TCP/IP                                                     │
└─────────────────────────────────────────────────────────────────┘
```

### New Functions (v0.1.12)

```c
// Layer 5: Contact DH Decryption
static int decrypt_client_msg(
    const uint8_t *enc, int enc_len,
    const uint8_t *sender_dh_pub,   // 32 bytes raw X25519
    const uint8_t *our_dh_secret,   // 32 bytes
    uint8_t *plain
);

// Layer 6: Agent Protocol Parser
static void parse_agent_message(contact_t *contact, const uint8_t *plain, int plain_len);
```

### Removed Functions (v0.1.12)

```c
// Replaced/Refactored:
- base64_pre_encode()
- base64_std_encode()
- parse_smp_client_header()
- parse_agent_envelope()
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

#### ERR AUTH

```
Server response: ERR AUTH
```
**Fix**:
- Using libsodium (not Monocypher)?
- Correct entityId? (ACK/DEL use recipientId!)

#### Invalid Link

```
SimpleX App shows: "Invalid link"
```
**Fix**:
- Use Base64URL for DH key (not Standard Base64!)
- Double-encode `=` padding: `=` → `%3D` → `%253D`

#### Layer 3 Decryption Produces Garbage

**Check**: Is this an initial message (AgentInvitation)?  
**Fix**: Apply Layer 5 Contact DH decryption first!

```c
// Look for SPKI header at offset 14
if (memcmp(&decrypted[14], SPKI_HEADER, 12) == 0) {
    // This is Layer 5 encrypted! Extract sender's DH and decrypt again
}
```

#### Agent Message Type Unknown

**Fix**: Check 2-byte BE version at offset 0, then type at offset 2.

```c
uint16_t agent_version = (plain[0] << 8) | plain[1];
char agent_type = plain[2];  // 'C', 'I', 'M', or 'R'
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

### Basic Connection Test

1. Build & Flash
2. Watch for "TLS OK! ALPN: smp/1"
3. Watch for "Subscriptions complete"

### Invitation Link Test (v0.1.11+)

1. Create a contact with `add_contact("Test")`
2. Copy the 🌐 Web Link from output
3. Open in browser → Should show SimpleX landing page
4. Open link in SimpleX Desktop/Mobile App
5. Click "Connect" in SimpleX App

### Agent Protocol Test (v0.1.12+)

After SimpleX App sends connection request:

1. Watch for `💬 MESSAGE for [Test]!`
2. Watch for `🔓 Layer 3 Decrypted: XXXXX bytes`
3. Watch for `🔓 Layer 5 Decrypted: XXX bytes` ← **NEW!**
4. Watch for `📋 Agent: Version=X, Type='I'` ← **NEW!**
5. Watch for `🔗 Reply Queue: ...` ← **NEW!**
6. Watch for `👤 Peer: <username>` ← **NEW!**

**Expected Output:**
```
💬 MESSAGE for [Test]!
🔓 Layer 3 Decrypted: 16106 bytes (SMP E2E)
🔓 Layer 5 Decrypted: 847 bytes (Client DH)
📋 Agent Message: Version=7, Type='I' (Invitation)
🔗 Reply Queue: simplex:/invitation#/?v=2-7&smp=...@smp10.simplex.im/...
👤 Peer Profile: {"displayName":"Alice",...}
✅ ACK OK
```

### Message Layer Verification

| Layer | Expected Output |
|-------|-----------------|
| Layer 1 | `TLS OK! ALPN: smp/1` |
| Layer 2 | `Received 16384 bytes` |
| Layer 3 | `Layer 3 Decrypted: XXXXX bytes` |
| Layer 4 | (Implicit in Layer 3 output) |
| Layer 5 | `Layer 5 Decrypted: XXX bytes` |
| Layer 6 | `Agent: Version=X, Type='X'` |

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
git commit -m "feat(agent): implement Layer 5 Contact DH decryption"
git commit -m "fix(url): use Base64URL encoding for DH key"
git commit -m "feat(agent): parse AgentInvitation and extract reply queue"
```

---

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [libsodium Documentation](https://doc.libsodium.org/)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [SimpleX Agent Protocol](https://github.com/simplex-chat/simplexmq/tree/stable/src/Simplex/Messaging/Agent)

---

*Last updated: January 21, 2026 — v0.1.12-alpha*
