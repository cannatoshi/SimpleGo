# SimpleGo Development Guide

> Complete setup guide and development documentation

---

## Prerequisites

### Hardware

| Component | Recommended |
|-----------|-------------|
| **MCU** | ESP32-S3 |
| **Dev Board** | LilyGo T-Deck or T-Embed |

### Software

| Component | Version |
|-----------|---------|
| **ESP-IDF** | 5.5.2+ |
| **Python** | 3.8+ |

---

## Building & Flashing

```powershell
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5
```

---

## Project Structure (v0.1.14)

```
simplex_client/
├── main/
│   ├── main.c              # Application entry (~350 lines)
│   ├── smp_globals.c       # Global variables
│   ├── smp_utils.c         # Encoding utilities
│   ├── smp_crypto.c        # Cryptography
│   ├── smp_network.c       # TLS/TCP I/O
│   ├── smp_contacts.c      # Contact management
│   ├── smp_parser.c        # Agent Protocol
│   ├── smp_peer.c          # Peer connection (NEW!)
│   ├── include/
│   │   ├── smp_types.h     # All structures
│   │   ├── smp_utils.h
│   │   ├── smp_crypto.h
│   │   ├── smp_network.h
│   │   ├── smp_contacts.h
│   │   ├── smp_parser.h
│   │   └── smp_peer.h      # NEW!
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── docs/
│   ├── ARCHITECTURE.md     # Module documentation
│   ├── PROTOCOL.md
│   ├── TECHNICAL.md
│   ├── DEVELOPMENT.md
│   ├── DEVNOTES.md
│   └── release-info/
│       └── v0.1.14-alpha.md
├── .gitignore              # NEW!
├── CHANGELOG.md
├── README.md
└── ROADMAP.md
```

---

## Module Overview

| Module | Lines | Purpose |
|--------|-------|---------|
| `main.c` | ~350 | App entry, WiFi, main loop |
| `smp_globals.c` | ~25 | Global variable definitions |
| `smp_utils.c` | ~100 | Base64, URL encoding |
| `smp_crypto.c` | ~80 | Ed25519, X25519, crypto_box |
| `smp_network.c` | ~160 | TLS, TCP, send/receive |
| `smp_contacts.c` | ~380 | Contact CRUD, NVS |
| `smp_parser.c` | ~260 | Agent Protocol, auto-connect |
| `smp_peer.c` | ~220 | Peer server connection |

**Total:** ~1575 lines (organized) vs ~1800 lines (monolithic)

---

## Adding New Code

### Adding a New Function

1. **Decide which module** it belongs to
2. **Add declaration** to appropriate header in `include/`
3. **Add implementation** to the `.c` file
4. **Include header** where needed

Example: Adding a new crypto function

```c
// 1. In include/smp_crypto.h
int my_new_crypto_function(const uint8_t *data, size_t len);

// 2. In smp_crypto.c
int my_new_crypto_function(const uint8_t *data, size_t len) {
    // Implementation
}

// 3. In other file that needs it
#include "smp_crypto.h"
my_new_crypto_function(data, len);
```

### Adding a New Module

1. Create `smp_newmodule.c` in `main/`
2. Create `include/smp_newmodule.h`
3. Add to `CMakeLists.txt`:

```cmake
idf_component_register(
    SRCS 
        "main.c"
        "smp_globals.c"
        ...
        "smp_newmodule.c"  # Add here
    INCLUDE_DIRS 
        "include"
    ...
)
```

---

## Debugging

### Common Issues (v0.1.14)

#### tcp_connect Undefined

**Cause:** Was renamed to `smp_tcp_connect()`

**Fix:** Use `smp_tcp_connect()` everywhere

#### DH Key Decode Fails

**Cause:** Invitation URIs use Standard Base64 (`+/=`)

**Fix:** Convert to Base64URL first:
```c
// Strip padding
while (len > 0 && dh[len-1] == '=') dh[--len] = '\0';
// Convert chars
for (int i = 0; i < len; i++) {
    if (dh[i] == '+') dh[i] = '-';
    if (dh[i] == '/') dh[i] = '_';
}
```

#### Peer Connection Fails

**Check:**
1. Host extracted correctly?
2. Port is 5223?
3. TLS handshake succeeds?
4. SMP handshake succeeds?

```c
ESP_LOGI(TAG, "Peer: %s:%d", pending_peer.host, pending_peer.port);
```

---

## Testing

### Peer Connection Test (v0.1.14)

1. Build and flash
2. Copy Web Link from ESP32 output
3. Scan with SimpleX App
4. Watch ESP32 output:

**Expected:**
```
💬 MESSAGE for [Test]!
📋 Agent: Version=7, Type='I'
📡 Peer: smp15.simplex.im:5223
🔑 DH Key extracted (32 bytes)
🔌 Connecting to peer server...
✅ Peer TLS OK
✅ Peer Handshake OK
📤 Sending AgentConfirmation...
✅ Server: OK
```

### Module Isolation Test

Each module should compile independently:

```bash
# Test single module compilation
idf.py build 2>&1 | grep -i error
```

---

## Git Workflow

### .gitignore (NEW!)

```gitignore
build/
managed_components/
sdkconfig.old
sdkconfig.defaults.old
*.pyc
__pycache__/
.vscode/
```

### Commit Style

```bash
git commit -m "type(module): description"
```

Types: `feat`, `fix`, `refactor`, `docs`

Modules: `peer`, `parser`, `contacts`, `network`, `crypto`, `utils`

Examples:
```bash
git commit -m "feat(peer): add peer_connect function"
git commit -m "fix(utils): convert Standard Base64 to URL"
git commit -m "refactor(all): split into 8 modules"
```

---

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [libsodium Documentation](https://doc.libsodium.org/)
- [SimpleX Protocol](https://github.com/simplex-chat/simplexmq)
- [Architecture Guide](ARCHITECTURE.md)

---

*Last updated: January 21, 2026 — v0.1.14-alpha*
