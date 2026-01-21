# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (January 21, 2026)

### Version: v0.1.13-alpha

### 🔧 Message Type Fix + Peer Queue Parsing!

AgentInvitation properly detected — ESP32 extracts peer server and queue ID!

**Latest Output:**
```
I (5765) SMP: ========================================
I (5765) SMP:   SimpleGo v0.1.13 - Peer Queue Parsing
I (5765) SMP:   Part of Sentinel Secure Messenger Suite
I (5765) SMP: ========================================

💬 MESSAGE for [Test]!
🔓 Layer 3 Decrypted: 16106 bytes (SMP E2E)
🔓 Layer 5 Decrypted: 847 bytes (Client DH)
📋 Agent: Version=7, Type='I' (Invitation)
📡 Peer Server: smp15.simplex.im:5223
📮 Queue ID: ahjPk2jlNZz53yh5RJ-sBCIu_vZQeWdK
✅ READY TO SEND CONFIRMATION
```

---

## Working Features

- ✅ **Message Type Parsing Fix** ← FIXED!
- ✅ **Peer Server Extraction** ← NEU!
- ✅ **Queue ID Extraction** ← NEU!
- ✅ **url_decode_inplace()** ← NEU!
- ✅ **peer_queue_t Structure** ← NEU!
- ✅ Agent Protocol Parsing (Layer 6)
- ✅ Client DH Decryption (Layer 5)
- ✅ SMP E2E Decryption (Layer 3)
- ✅ Multi-Contact Database (10 slots)
- ✅ All SMP Commands

---

## Key Discovery: Message Type Position

### The Problem

After DH decryption, the message type was searched at the wrong position:

```c
// OLD CODE (WRONG!)
char type = decrypted[2];  // Found '_' instead of 'I'!
```

### The Discovery

The message format after DH decryption:

```
2a a5 5f 00 07 49 ...
*  ?  _  ver   I
0  1  2  3  4  5

Position 2: '_' (Delimiter)
Position 3-4: Version (Big Endian, 0x0007 = Version 7)
Position 5: Message Type ('I' = Invitation)
```

### The Fix

```c
// NEW CODE (CORRECT!)
int toff = -1;
for (int i = 0; i < 10 && i < dec_len - 3; i++) {
    if (decrypted[i] == '_') { toff = i; break; }
}
if (toff >= 0) {
    uint16_t ver = (decrypted[toff + 1] << 8) | decrypted[toff + 2];
    char type = decrypted[toff + 3];  // 'C', 'I', 'M', or 'R'
}
```

---

## New Structure: peer_queue_t

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

---

## URL Decoding (Multi-Pass Required!)

### The Problem

SimpleX URIs are often 2-3x URL-encoded:

```
Original:    %253D
After 1st:   %3D
After 2nd:   =
```

### Common Patterns

| Encoded | Once Decoded | Twice Decoded |
|---------|--------------|---------------|
| `%253D` | `%3D` | `=` |
| `%2526` | `%26` | `&` |
| `%252F` | `%2F` | `/` |
| `%253A` | `%3A` | `:` |
| `%2540` | `%40` | `@` |

### Implementation

```c
static void url_decode_inplace(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src == '%' && src[1] && src[2]) {
            int val;
            if (sscanf(src + 1, "%2x", &val) == 1) {
                *dst++ = (char)val;
                src += 3;
                continue;
            }
        }
        *dst++ = *src++;
    }
    *dst = '\0';
}

// Must call multiple times!
do {
    old_len = strlen(uri);
    url_decode_inplace(uri);
} while (strlen(uri) < old_len);
```

---

## SMP URI Format

### Structure

```
smp://keyHash@host:port/queueId#/?v=1-4&dh=base64Key
```

### Extracted from Invitation

```
simplex:/invitation#/?v=2-7&smp=smp%3A%2F%2F...

After URL decode:
smp://6iIcWT_dF2zN_w5xzZEY7HI2Prbh3ldP07YTyDexPjE=@smp15.simplex.im:5223/ahjPk2jlNZz53yh5RJ-sBCIu_vZQeWdK#/?v=1-4&dh=MCow...
      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^    ^^^^^^^^^^^^^^^^^      ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^        ^^^^^^^^
      Key Hash (Base64URL)                         Host:Port              Queue ID                                DH Key
```

---

## DH Key Search (In Progress)

### The Challenge

The `dh=` parameter is deeply nested and multi-encoded:

```
%26dh%3DMCowBQYDK2VuAyEAWjdWg-4cHabdeVsdhOtIvEZXxaHZKtQlZeXrBj0Z7EU%253D
```

### Search Patterns

```c
// Try multiple patterns
const char *patterns[] = {
    "dh=",           // Direct
    "dh%3D",         // Once encoded
    "%26dh%3D",      // Twice encoded (&dh=)
    NULL
};
```

### Status

- ✅ Pattern search implemented
- 🔧 Base64URL decode needed
- 🔧 Handle trailing `%253D` (double-encoded `=`)

---

## Agent Message Types

| Type | Name | Description | Status |
|------|------|-------------|--------|
| `'C'` | AgentConfirmation | Confirmation response | ⏳ To Send |
| `'I'` | AgentInvitation | Invitation with reply queue | ✅ Parsed |
| `'M'` | AgentMsgEnvelope | Double Ratchet message | 📋 Planned |
| `'R'` | AgentRatchetKey | Ratchet key exchange | 📋 Planned |

---

## Connection Flow (Current State)

```
┌──────────┐                              ┌──────────┐
│ SimpleX  │                              │  ESP32   │
│   App    │                              │          │
└────┬─────┘                              └────┬─────┘
     │  1. Scans Contact Link                  │
     │  2. SEND AgentInvitation ───────────────>
     │                                         │
     │  3. ESP32 parses:                       │
     │     ✅ Type = 'I' (Invitation)          │
     │     ✅ Peer Server = smp15.simplex.im   │
     │     ✅ Queue ID = ahjPk2jl...           │
     │     🔧 DH Key = (in progress)           │
     │                                         │
     │  READY TO SEND CONFIRMATION             │
     │                                         │
     │  4. Connect to Peer Server              │  ⏳ NEXT
     │  5. SEND AgentConfirmation              │  ⏳ NEXT
     │     <─────────────────────────────────────
     │  6. "Connected!"                        │
```

---

## Build Environment

**Windows (ESP-IDF 5.5 PowerShell):**
```powershell
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5
```

---

## Feature Matrix v0.1.13

```
═══════════════════════════════════════════════════════════════
✅ TLS 1.3 Connection (ALPN: smp/1)
✅ SMP Handshake (ServerHello/ClientHello)
✅ Queue Creation (NEW → IDS)
✅ Queue Subscribe (SUB → OK)
✅ Contact Link Generation
✅ Message Receive (MSG)
✅ SMP E2E Decryption (Layer 3)
✅ Client Message Decryption (Layer 5)
✅ Agent Protocol Parsing (Layer 6)
✅ Message Type Fix ('_' + 3)
✅ Peer Server Extraction
✅ Queue ID Extraction
✅ url_decode_inplace() (multi-pass)
✅ "READY TO SEND CONFIRMATION"
═══════════════════════════════════════════════════════════════
🔧 DH Key Extraction (multi-encoded)
⏳ Connect to Peer Server
⏳ AgentConfirmation Builder
⏳ SEND CONF to Peer Queue
⏳ Connection Established!
═══════════════════════════════════════════════════════════════
```

---

## Next Steps (v0.1.14)

1. **DH Key Extraction** — Handle all encoding variants
2. **Connect to Peer Server** — TLS to peer's SMP server
3. **AgentConfirmation Builder** — Create CONF message
4. **SEND to Peer** — Complete handshake
5. **"Connected!"** — SimpleX App shows connection

---

*Last updated: January 21, 2026 — v0.1.13-alpha*
