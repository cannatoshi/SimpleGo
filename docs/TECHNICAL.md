# SimpleGo Technical Documentation

> Key learnings, discoveries, and implementation decisions from developing the first native SMP client

---

## Table of Contents

1. [Project Background](#project-background)
2. [Critical Discoveries](#critical-discoveries)
3. [Debugging Journey](#debugging-journey)
4. [Architecture Decisions](#architecture-decisions)
5. [Lessons Learned](#lessons-learned)

---

## Project Background

### The Challenge

All existing SimpleX clients share a common architecture:

```
┌─────────────────┐     ┌─────────────────┐
│  Native UI      │────>│  Haskell Core   │
│  (Swift/Kotlin/ │ FFI │  (simplexmq)    │
│   Electron)     │<────│                 │
└─────────────────┘     └─────────────────┘
```

SimpleGo implements the protocol **from scratch in C** for ESP32.

---

## Critical Discoveries

### Discovery #1: keyHash Must Use CA Certificate

**Finding**: keyHash computed from **CA certificate** (2nd in chain), not server cert.

---

### Discovery #2: Monocypher vs libsodium Incompatibility

**Finding**: Different Ed25519 signatures! Use libsodium for SimpleX compatibility.

---

### Discovery #3-6: Command Format Issues

- SubMode 'S' required for SMP v6
- MsgFlags must be ASCII 'T'/'F'
- SEND needs two spaces
- Different block formats for handshake vs commands

---

### Discovery #7: crypto_box vs raw X25519

**Finding**: `crypto_box_beforenm()` does HSalsa20 key derivation, raw `crypto_scalarmult()` doesn't work.

---

### Discovery #8-11: URL Encoding Issues

- Server encrypts for recipient
- ACK/DEL use recipientId
- Base64URL for DH key (not Standard)
- Double encoding for `=` padding

---

### Discovery #12-16: Agent Protocol (v0.1.12)

- Layer 5: Contact DH encryption exists
- Layer 6: Agent Protocol format
- SPKI at offset 14 in decrypted message
- Version string "1," vs BE integer

---

### Discovery #17: '_' Delimiter for Message Type (v0.1.13)

**Problem**: Message type was searched at wrong position.

**Finding**: After DH decryption, the message has a prefix, then `_` delimiter:

```
2a a5 5f 00 07 49 ...
*  ?  _  ver   I
0  1  2  3  4  5

Position 2: '_' (Delimiter) ← SEARCH FOR THIS FIRST!
Position 3-4: Version (Big Endian)
Position 5: Type ('C', 'I', 'M', 'R')
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
char type = decrypted[toff + 3];
```

---

### Discovery #18: Multi-Encoded URLs (v0.1.13)

**Problem**: SMP URI parameters couldn't be found.

**Finding**: SimpleX URIs are often 2-3x URL-encoded:

```
%253D → %3D → =
%2526 → %26 → &
```

**Solution**: Decode repeatedly until no changes:

```c
do {
    old_len = strlen(uri);
    url_decode_inplace(uri);
} while (strlen(uri) < old_len);
```

---

### Discovery #19: DH Key Search Patterns (v0.1.13)

**Problem**: `dh=` parameter hidden in nested encoding.

**Finding**: Search multiple patterns:

```c
"dh="           // Direct
"dh%3D"         // Once encoded
"%26dh%3D"      // Twice encoded (&dh=)
```

---

## Debugging Journey

### Error State Progression

```
Timeline:
──────────────────────────────────────────────────────────────────────────────────────────────────────>

[TLS] [BLOCK] [CMD] [AUTH] [E2E] [LAYER5] [AGENT] [TYPE FIX] [URL DEC] [PEER Q]
  │      │      │      │      │      │       │        │          │         │
  ▼      ▼      ▼      ▼      ▼      ▼       ▼        ▼          ▼         ▼
TLS    Block  SubMode libsodium E2E  Contact Agent   Find '_'  Multi    Extract
1.3    format  'S'    works   decrypt  DH   Protocol  then+3   decode   server
                                                                         +queue
```

### Detailed Error Analysis

| Version | Error | Root Cause | Fix |
|---------|-------|------------|-----|
| v0.1.1 | TLS fail | Version mismatch | Force TLS 1.3 |
| v0.1.3 | ERR AUTH | Wrong crypto lib | libsodium |
| v0.1.5 | ERR CMD | Binary flags | ASCII 'T'/'F' |
| v0.1.6 | Decrypt fail | Raw X25519 | crypto_box_beforenm |
| v0.1.11 | Invalid link | `+` in URL | Base64URL |
| v0.1.12 | Garbage output | Layer 5 | Contact DH decrypt |
| v0.1.12 | Unknown format | Agent Protocol | Parse version + type |
| **v0.1.13** | **Wrong type** | **Fixed offset** | **Find '_' + 3** |
| **v0.1.13** | **Param not found** | **Multi-encoded** | **Decode loop** |

---

## Architecture Decisions

### peer_queue_t Structure

New structure for extracted invitation data:

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

### URL Decode Function

In-place decoder for memory efficiency:

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
```

---

## Lessons Learned

### 1. Don't Assume Fixed Offsets

Agent messages have variable-length prefixes. Search for delimiters first.

### 2. URL Decoding is Nested

SimpleX URIs can be 2-3x encoded. Always decode in a loop until stable.

### 3. Search Multiple Patterns

When looking for parameters, try all encoding variants:
- Direct: `dh=`
- Once encoded: `dh%3D`
- Twice encoded: `%26dh%3D`

### 4. Test with Real Data

The `'_'` delimiter issue only appeared with actual SimpleX App messages.

### 5. Log Hex Dumps

Raw byte inspection revealed the true message format.

---

## Quick Reference

### Message Type Position

```
[prefix bytes] ['_'] [ver:2 BE] [type] [body]
               ^^^^
               Search for this, then +3 for type
```

### Agent Message Types

| Type | Name | Description |
|------|------|-------------|
| `'C'` | AgentConfirmation | Confirmation response |
| `'I'` | AgentInvitation | Reply queue + profile |
| `'M'` | AgentMsgEnvelope | Double Ratchet message |
| `'R'` | AgentRatchetKey | Key exchange |

### URL Encoding Levels

| Original | 1x | 2x | 3x |
|----------|----|----|-----|
| `=` | `%3D` | `%253D` | `%25253D` |
| `&` | `%26` | `%2526` | `%252526` |
| `/` | `%2F` | `%252F` | `%25252F` |

---

## 🏆 Progress: Ready to Send Confirmation!

As of v0.1.13-alpha:

- ✅ Layer 1-6 Decryption Working
- ✅ Message Type 'I' Properly Detected
- ✅ Peer Server Extracted
- ✅ Queue ID Extracted
- ✅ "READY TO SEND CONFIRMATION"
- 🔧 DH Key Extraction (in progress)
- ⏳ Connect to Peer Server
- ⏳ Send AgentConfirmation

**"First Native ESP32 SimpleX Client — Ready to Complete Connection!"**

---

*Last updated: January 21, 2026 — v0.1.13-alpha*
