# SimpleX Messaging Protocol (SMP) - Implementation Guide

> Deep technical documentation for implementing SMP on embedded systems

---

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [Message Layer Stack](#message-layer-stack)
3. [Agent Message Format](#agent-message-format)
4. [SMP URI Format](#smp-uri-format)
5. [URL Encoding](#url-encoding)
6. [Cryptographic Operations](#cryptographic-operations)
7. [Command Reference](#command-reference)
8. [Connection Flow](#connection-flow)

---

## Protocol Overview

### What is SMP?

The **SimpleX Messaging Protocol (SMP)** is a transport-agnostic protocol for private, asynchronous messaging. It uses unidirectional message queues where:

- **Sender** can only send messages
- **Recipient** can only receive messages
- **Server** cannot correlate sender and recipient

---

## Message Layer Stack

### Complete 6-Layer Stack

```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 1: TLS 1.3 Transport                                     │
│  └── ALPN: "smp/1", ChaCha20-Poly1305                          │
├─────────────────────────────────────────────────────────────────┤
│  Layer 2: SMP Transport Block                                   │
│  └── [2-byte transmissionLength] [content] [padding to 16KB]   │
├─────────────────────────────────────────────────────────────────┤
│  Layer 3: SMP E2E Encryption                                    │
│  └── crypto_box(msg, nonce, server_dh_pub, our_dh_secret)      │
├─────────────────────────────────────────────────────────────────┤
│  Layer 4: SMP Client Message                                    │
│  └── [2-byte length prefix] [encrypted_content] [padding]      │
├─────────────────────────────────────────────────────────────────┤
│  Layer 5: Contact DH Encryption (Initial Messages)              │
│  └── [X25519 SPKI key (44 bytes)] [crypto_box encrypted body]  │
├─────────────────────────────────────────────────────────────────┤
│  Layer 6: Agent Protocol Message                                │
│  └── [prefix] ['_'] [version:2 BE] [type] [body]               │
└─────────────────────────────────────────────────────────────────┘
```

---

## Agent Message Format

### CRITICAL: Message Type Position (v0.1.13 Fix!)

After DH decryption (Layer 5), the message has this format:

```
2a a5 5f 00 07 49 ...
*  ?  _  ver   I
0  1  2  3  4  5

Position 0-1: Prefix bytes (variable)
Position 2:   '_' (Delimiter) ← SEARCH FOR THIS!
Position 3-4: Version (Big Endian, 0x0007 = v7)
Position 5:   Type ('C', 'I', 'M', 'R')
Position 6+:  Body (type-specific)
```

### Finding the Type (Correct Method)

```c
// Search for '_' delimiter first!
int toff = -1;
for (int i = 0; i < 10 && i < dec_len - 3; i++) {
    if (decrypted[i] == '_') { toff = i; break; }
}

if (toff >= 0) {
    uint16_t agent_version = (decrypted[toff + 1] << 8) | decrypted[toff + 2];
    char type = decrypted[toff + 3];
    
    switch (type) {
        case 'C': // AgentConfirmation
        case 'I': // AgentInvitation
        case 'M': // AgentMsgEnvelope
        case 'R': // AgentRatchetKey
    }
}
```

### Agent Message Types

| Type | Name | Description |
|------|------|-------------|
| `'C'` | AgentConfirmation | Connection confirmation response |
| `'I'` | AgentInvitation | Invitation with reply queue URI + profile |
| `'M'` | AgentMsgEnvelope | Double Ratchet encrypted message |
| `'R'` | AgentRatchetKey | Ratchet key exchange |

### AgentInvitation Format (Type 'I')

```
[prefix]['_'][version:2][type:'I'][connReqLen:2][connReq][connInfo]

connReq = URL-encoded simplex:/invitation#/?v=2-7&smp=...
connInfo = Peer's profile (JSON or binary)
```

---

## SMP URI Format

### Structure

```
smp://keyHash@host:port/queueId#/?v=1-4&dh=base64Key&q=c
      ^^^^^^^ ^^^^^^^^^ ^^^^^^^     ^^^ ^^^^^^^^^^^ ^^^
      Key Hash Host:Port Queue ID   Ver DH Key      Mode
```

### Components

| Component | Description | Example |
|-----------|-------------|---------|
| `keyHash` | Base64URL SHA256 of server CA cert | `6iIcWT_dF2zN_w5xzZEY...` |
| `host` | SMP server hostname | `smp15.simplex.im` |
| `port` | SMP port (default 5223) | `5223` |
| `queueId` | Base64URL queue identifier | `ahjPk2jlNZz53yh5RJ...` |
| `v` | SMP version range | `1-4` |
| `dh` | Base64 X25519 SPKI public key | `MCowBQYDK2VuAyEA...` |
| `q` | Queue mode (c=contact) | `c` |

### peer_queue_t Structure

```c
typedef struct {
    char host[64];           // Peer Server (e.g., smp15.simplex.im)
    int port;                // Port (default 5223)
    uint8_t key_hash[32];    // Server Key Hash (decoded)
    uint8_t queue_id[32];    // Queue ID (24 bytes typical)
    int queue_id_len;
    uint8_t dh_public[32];   // Peer's DH Public Key (decoded)
    int has_dh;
    int valid;
} peer_queue_t;
```

---

## URL Encoding

### Multi-Pass Decoding Required!

SimpleX URIs are often 2-3x URL-encoded:

```
Original:    %253D
After 1st:   %3D
After 2nd:   =

Original:    %2526
After 1st:   %26
After 2nd:   &
```

### Common Encoding Patterns

| Original | Once Encoded | Twice Encoded | Thrice Encoded |
|----------|--------------|---------------|----------------|
| `=` | `%3D` | `%253D` | `%25253D` |
| `&` | `%26` | `%2526` | `%252526` |
| `/` | `%2F` | `%252F` | `%25252F` |
| `:` | `%3A` | `%253A` | `%25253A` |
| `@` | `%40` | `%2540` | `%252540` |
| `#` | `%23` | `%2523` | `%252523` |
| `?` | `%3F` | `%253F` | `%25253F` |

### URL Decode Implementation

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

// Call repeatedly until no more changes!
void url_decode_full(char *str) {
    size_t old_len;
    do {
        old_len = strlen(str);
        url_decode_inplace(str);
    } while (strlen(str) < old_len);
}
```

### DH Key Search Patterns

The `dh=` parameter may appear in various encoded forms:

```c
const char *dh_patterns[] = {
    "dh=",           // Direct
    "dh%3D",         // Once encoded
    "%26dh%3D",      // Twice encoded (&dh=)
    "%2526dh%253D",  // Thrice encoded
    NULL
};

// Search for each pattern
for (int i = 0; dh_patterns[i]; i++) {
    char *pos = strstr(uri, dh_patterns[i]);
    if (pos) {
        // Found DH key location
        break;
    }
}
```

---

## Cryptographic Operations

### X25519 SPKI Header

```
Header (12 bytes): 30 2a 30 05 06 03 2b 65 6e 03 21 00
Full SPKI (44 bytes) = Header (12) + Raw Key (32)
```

### crypto_box (Layer 3 + Layer 5)

```c
// Decrypt with HSalsa20 key derivation
crypto_box_beforenm(shared, public, secret);
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

---

## Command Reference

### SMP Commands

| Command | EntityId | Description |
|---------|----------|-------------|
| NEW | empty | Create queue |
| SUB | recipientId | Subscribe |
| SEND | senderId | Send message |
| ACK | recipientId | Acknowledge |
| DEL | recipientId | Delete queue |

### SEND Command Format

```
"SEND" ' ' [flags] ' ' [body]
       ↑          ↑
      0x20       0x20

flags: ASCII 'T' or 'F' (NOT binary!)
```

---

## Connection Flow

### Contact Address Flow (q=c)

```
┌──────────┐                              ┌──────────┐
│ SimpleX  │                              │  ESP32   │
│   App    │                              │(Contact) │
└────┬─────┘                              └────┬─────┘
     │                                         │
     │  1. Scans Contact Link                  │
     │     (https://simplex.chat/contact#/...) │
     │                                         │
     │  2. SEND AgentInvitation ───────────────>
     │     [Type 'I'][Reply Queue][Profile]    │
     │                                         │
     │  3. ESP32 parses invitation:            │
     │     - Find '_' delimiter                │
     │     - Read version (BE uint16)          │
     │     - Read type = 'I'                   │
     │     - Extract peer server               │
     │     - Extract queue ID                  │
     │     - Extract DH key                    │
     │                                         │
     │  4. ESP32 connects to Peer Server       │
     │     (smp15.simplex.im:5223)             │
     │                                         │
     │  5. SEND AgentConfirmation              │
     │     <─────────────────────────────────────
     │     [Type 'C'][connInfo]                │
     │                                         │
     │  6. "Connected!"                        │
```

---

## Performance (ESP32-S3 @ 240MHz)

| Operation | Time |
|-----------|------|
| URL decode (per pass) | <1ms |
| Find '_' delimiter | <1ms |
| Extract peer queue | <1ms |
| crypto_box decrypt | ~1ms |
| TLS handshake | ~800ms |

---

## References

- [SMP Protocol Specification](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [SimpleX Agent Protocol](https://github.com/simplex-chat/simplexmq/tree/stable/src/Simplex/Messaging/Agent)
- [libsodium Documentation](https://doc.libsodium.org/)

---

*Last updated: January 21, 2026 — v0.1.13-alpha*
