# SimpleGo Development Guide

> Complete setup guide and development documentation

---

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Environment Setup](#environment-setup)
3. [Building & Flashing](#building--flashing)
4. [Architecture Overview](#architecture-overview)
5. [Debugging](#debugging)
6. [Testing](#testing)

---

## Prerequisites

### Hardware

| Component | Recommended |
|-----------|-------------|
| **MCU** | ESP32-S3 |
| **Dev Board** | LilyGo T-Deck or T-Embed |
| **USB Cable** | USB-C data cable |

### Software

| Component | Version |
|-----------|---------|
| **ESP-IDF** | 5.5.2+ |
| **Python** | 3.8+ |
| **Git** | Any recent |

---

## Environment Setup

### Windows

```powershell
# Use ESP-IDF PowerShell from Start Menu
C:\Espressif\idf_cmd_init.ps1
```

### Linux / macOS

```bash
. ~/esp/esp-idf/export.sh
```

---

## Building & Flashing

```bash
idf.py set-target esp32s3
idf.py build flash monitor -p /dev/ttyUSB0  # or COM5 on Windows
```

### Monitor Shortcuts

| Key | Action |
|-----|--------|
| `Ctrl+]` | Exit monitor |
| `Ctrl+T, R` | Reboot device |

---

## Architecture Overview

### System Stack (v0.1.13)

```
┌─────────────────────────────────────────────────────────────────┐
│           Application Layer                                     │
├─────────────────────────────────────────────────────────────────┤
│  Connection Handler                             🔧 IN PROGRESS  │
│  ├── peer_queue_t Structure                     ✅ NEW!         │
│  ├── Peer Server Extraction                     ✅ NEW!         │
│  ├── Queue ID Extraction                        ✅ NEW!         │
│  └── DH Key Extraction                          🔧 In Progress  │
├─────────────────────────────────────────────────────────────────┤
│  Agent Protocol Layer                           ✅ FIXED!       │
│  ├── '_' Delimiter Search                       ✅ NEW!         │
│  ├── Version Parse (BE uint16)                  ✅ FIXED!       │
│  ├── Type Parse at +3                           ✅ FIXED!       │
│  └── url_decode_inplace()                       ✅ NEW!         │
├─────────────────────────────────────────────────────────────────┤
│  Message Decryption Stack                       ✅ COMPLETE     │
│  ├── Layer 3: SMP E2E (server DH)                               │
│  ├── Layer 5: Client DH (contact DH)                            │
│  └── Layer 6: Agent Protocol Parsing                            │
├─────────────────────────────────────────────────────────────────┤
│  Contact Management                             ✅ COMPLETE     │
│  └── Multi-Contact + NVS Persistence                            │
├─────────────────────────────────────────────────────────────────┤
│  Crypto + SMP Protocol                          ✅ COMPLETE     │
└─────────────────────────────────────────────────────────────────┘
```

### New Functions (v0.1.13)

```c
// URL decode (in-place, call repeatedly!)
static void url_decode_inplace(char *str);

// Message type finding with '_' delimiter
int toff = -1;
for (int i = 0; i < 10 && i < dec_len - 3; i++) {
    if (decrypted[i] == '_') { toff = i; break; }
}
uint16_t ver = (decrypted[toff + 1] << 8) | decrypted[toff + 2];
char type = decrypted[toff + 3];
```

### New Structure (v0.1.13)

```c
typedef struct {
    char host[64];           // Peer Server
    int port;                // Port (default 5223)
    uint8_t key_hash[32];    // Server Key Hash
    uint8_t queue_id[32];    // Queue ID
    int queue_id_len;
    uint8_t dh_public[32];   // Peer's DH Public Key
    int has_dh;
    int valid;
} peer_queue_t;
```

---

## Debugging

### Log Levels

```c
esp_log_level_set("*", ESP_LOG_INFO);
esp_log_level_set("SMP", ESP_LOG_DEBUG);
```

### Common Issues (v0.1.13)

#### Message Type Always Shows '_' or Wrong Character

**Cause**: Type searched at fixed offset instead of after '_' delimiter.

**Fix**: Search for '_' first, then read type at +3:

```c
int toff = -1;
for (int i = 0; i < 10 && i < dec_len - 3; i++) {
    if (decrypted[i] == '_') { toff = i; break; }
}
char type = decrypted[toff + 3];
```

#### URL Parameters Not Found (dh=, smp=, etc.)

**Cause**: Multi-level URL encoding.

**Fix**: Decode in loop until stable:

```c
size_t old_len;
do {
    old_len = strlen(uri);
    url_decode_inplace(uri);
} while (strlen(uri) < old_len);
```

#### DH Key Not Found

**Cause**: `dh=` may be encoded as `dh%3D` or `%26dh%3D`.

**Fix**: Search multiple patterns:

```c
char *dh_pos = strstr(uri, "dh=");
if (!dh_pos) dh_pos = strstr(uri, "dh%3D");
if (!dh_pos) dh_pos = strstr(uri, "%26dh%3D");
```

### Hex Dump Helper

```c
void hex_dump(const char *label, const uint8_t *data, size_t len) {
    ESP_LOGI("HEX", "%s (%d bytes):", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}
```

---

## Testing

### Message Type Test (v0.1.13)

1. Create a contact with `add_contact("Test")`
2. Copy the Web Link and scan with SimpleX App
3. Click "Connect" in SimpleX App
4. Watch ESP32 output:

**Expected:**
```
💬 MESSAGE for [Test]!
🔓 Layer 3 Decrypted: 16106 bytes
🔓 Layer 5 Decrypted: 847 bytes
📋 Agent: Version=7, Type='I' (Invitation)   ← Correct!
📡 Peer Server: smp15.simplex.im:5223
📮 Queue ID: ahjPk2jlNZz53yh5RJ-sBCIu_vZQeWdK
✅ READY TO SEND CONFIRMATION
```

**If Wrong:**
```
📋 Agent: Version=???, Type='_'   ← BUG! Not finding delimiter
```

### URL Decode Test

```c
// Test with multi-encoded string
char test[] = "%253D%2526";
url_decode_inplace(test);  // → "%3D%26"
url_decode_inplace(test);  // → "=&"
```

### Peer Queue Extraction Test

| Field | Expected |
|-------|----------|
| Type | `'I'` (Invitation) |
| Server | `smpXX.simplex.im` |
| Port | `5223` |
| Queue ID | 24+ character Base64URL |
| Status | "READY TO SEND CONFIRMATION" |

---

## Git Workflow

### Commit Style

```bash
git commit -m "type(scope): description"
```

Examples:
```bash
git commit -m "fix(agent): find message type after '_' delimiter"
git commit -m "feat(url): add multi-pass url_decode_inplace()"
git commit -m "feat(peer): add peer_queue_t structure"
```

---

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [libsodium Documentation](https://doc.libsodium.org/)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [SimpleX Agent Protocol](https://github.com/simplex-chat/simplexmq/tree/stable/src/Simplex/Messaging/Agent)

---

*Last updated: January 21, 2026 — v0.1.13-alpha*
