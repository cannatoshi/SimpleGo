# Changelog

All notable changes to SimpleGo are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- DH Key Extraction (multi-encoded URLs)
- CONF Response Builder
- Connect to Peer Server
- Double Ratchet Implementation

---

## [0.1.13-alpha] - 2026-01-21

### 🔧 Message Type Fix + Peer Queue Parsing!

AgentInvitation properly detected — ESP32 extracts peer server and queue ID!

### Added
- **`peer_queue_t` Structure** — Stores extracted invitation data (host, port, queue_id, dh_key)
- **`url_decode_inplace()`** — Handles multi-encoded URLs (2-3x encoding common)
- **SMP URI Parsing** — Extracts peer server + queue from invitation
- **"READY TO SEND CONFIRMATION"** — Status when invitation fully parsed

### Fixed
- **CRITICAL: Message Type Position** — Type is at `_` delimiter + 3, not fixed offset 2
- **Agent Version Parsing** — Now correctly reads 2-byte BE at delimiter + 1

### Technical Details

**Message Format After DH Decryption:**
```
2a a5 5f 00 07 49 ...
*  ?  _  ver   I
0  1  2  3  4  5

Position 2: '_' (Delimiter)
Position 3-4: Version (Big Endian, 0x0007 = Version 7)
Position 5: Message Type ('I' = Invitation)
```

**Old Code (WRONG):**
```c
char type = decrypted[2];  // Found '_' instead of type!
```

**New Code (CORRECT):**
```c
int toff = -1;
for (int i = 0; i < 10 && i < dec_len - 3; i++) {
    if (decrypted[i] == '_') { toff = i; break; }
}
uint16_t ver = (decrypted[toff + 1] << 8) | decrypted[toff + 2];
char type = decrypted[toff + 3];  // 'C', 'I', 'M', or 'R'
```

**peer_queue_t Structure:**
```c
typedef struct {
    char host[64];           // Peer Server (e.g., smp15.simplex.im)
    int port;                // Port (default 5223)
    uint8_t key_hash[32];    // Server Key Hash
    uint8_t queue_id[32];    // Queue ID (24 bytes typical)
    int queue_id_len;
    uint8_t dh_public[32];   // Peer's DH Public Key
    int has_dh;
    int valid;
} peer_queue_t;
```

**URL Decoding (Multi-Pass Required!):**
```
%253D → %3D → =
%2526 → %26 → &
%252F → %2F → /
```

SimpleX URIs are often 2-3x URL-encoded. Must decode repeatedly until no changes.

**Extracted from Invitation:**
```
📡 Peer Server: smp15.simplex.im:5223
📮 Queue ID: ahjPk2jlNZz53yh5RJ-sBCIu_vZQeWdK
🔑 DH Key: (extraction in progress)
✅ READY TO SEND CONFIRMATION
```

### Key Discoveries

| Discovery | Details |
|-----------|---------|
| `_` Delimiter | Agent messages start with prefix bytes, then `_` |
| Type at +3 | After `_`, skip 2-byte version, then type byte |
| Multi-encoded URLs | SimpleX URIs may be 2-3x URL encoded |
| smp:// format | `smp://keyHash@host:port/queueId#/?...&dh=...` |

### Status

| Feature | Status |
|---------|--------|
| Message Type 'I' Detection | ✅ Working |
| Peer Server Extraction | ✅ Working |
| Queue ID Extraction | ✅ Working |
| "READY TO SEND CONFIRMATION" | ✅ Working |
| DH Key Extraction | 🔧 In Progress |
| CONF Response | ⏳ Next |
| Connect to Peer Server | ⏳ Next |

---

## [0.1.12-alpha] - 2026-01-21

### 🔐 Agent Protocol Parsing + Client Message Decryption!

Full message layer stack decoded — ESP32 now sees peer's profile and reply queue!

### Added
- **Client Message Decryption (Layer 5)** — Second crypto_box layer with contact's DH key
- **Agent Protocol Parsing (Layer 6)** — Version + Type + Body structure
- **AgentInvitation Detection** — Type 'I' messages recognized
- **Reply Queue URI Extraction** — Peer's SMP server + queue visible
- **Peer Profile Visibility** — ConnInfo with username extracted

### Fixed
- **CRITICAL: Contact Link URL Encoding** — DH Key must be Base64URL (not Standard!)
- **Double Encoding for `=`** — Padding `=` → `%3D` → `%253D`

---

## [0.1.11-alpha] - 2026-01-20

### 🔗 Invitation Links Working!

SimpleX Desktop/Mobile Apps can now connect directly to ESP32!

---

## [0.1.10-alpha] - 2026-01-20

### 🏆 Multi-Contact + E2E Decryption Working!

---

## [0.1.9-alpha] - 2026-01-20

### 🗑️ DEL Command + Full SMP Client!

---

## [0.1.8-alpha] - 2026-01-20

### 🔑 NVS Key Persistence!

---

## [0.1.7-alpha] - 2026-01-20

### ✅ ACK Command Complete!

---

## [0.1.6-alpha] - 2026-01-20

### 🔐 E2E Encryption (Single Queue)

---

## [0.1.5-alpha] - 2026-01-20

### 📨 SEND + MSG Receive

---

## [0.1.4-alpha] - 2026-01-20

### 📡 SUB Command

---

## [0.1.3-alpha] - 2026-01-19

### 🎉 NEW Command Working!

---

## [0.1.2-alpha] - 2026-01-18

### 🤝 Handshake Complete!

---

## [0.1.1-alpha] - 2026-01-17

### 🔒 TLS 1.3 Working!

---

## [0.1.0-alpha] - 2026-01-16

### Initial Release

---

## Version History Summary

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.13-alpha** | **2026-01-21** | **🔧 Message Type Fix + Peer Queue!** |
| v0.1.12-alpha | 2026-01-21 | 🔐 Agent Protocol + Layer 5 |
| v0.1.11-alpha | 2026-01-20 | 🔗 Invitation Links |
| v0.1.10-alpha | 2026-01-20 | 🏆 Multi-Contact + E2E |
| v0.1.9-alpha | 2026-01-20 | 🗑️ DEL + Full SMP Client |
| v0.1.8-alpha | 2026-01-20 | 🔑 NVS Persistence |
| v0.1.7-alpha | 2026-01-20 | ✅ ACK Command |
| v0.1.6-alpha | 2026-01-20 | 🔐 E2E Decryption |
| v0.1.5-alpha | 2026-01-20 | 📨 SEND + MSG |
| v0.1.4-alpha | 2026-01-20 | 📡 SUB Command |
| v0.1.3-alpha | 2026-01-19 | 🎉 NEW Command |
| v0.1.2-alpha | 2026-01-18 | 🤝 Handshake |
| v0.1.1-alpha | 2026-01-17 | 🔒 TLS 1.3 |
| v0.1.0-alpha | 2026-01-16 | Initial |

---

## 🏆 Progress Update

**"First Native ESP32 SimpleX Client — Ready to Send Confirmation!"**

- ✅ TLS 1.3 + SMP Handshake
- ✅ Queue Management (NEW, SUB, DEL)
- ✅ Message Lifecycle (SEND, MSG, ACK)
- ✅ SMP E2E Decryption (Layer 3)
- ✅ Client Message Decryption (Layer 5)
- ✅ Agent Protocol Parsing (Layer 6)
- ✅ **AgentInvitation Type 'I' Detection**
- ✅ **Peer Server + Queue ID Extraction**
- 🔧 DH Key Extraction (in progress)
- ⏳ CONF Response + Connection Complete

---

## Links

- [GitHub Repository](https://github.com/cannatoshi/SimpleGo)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
