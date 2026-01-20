# SimpleX Messaging Protocol (SMP) - Implementation Guide

> Deep technical documentation for implementing SMP on embedded systems

---

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [Message Layer Stack](#message-layer-stack)
3. [Transport Layer](#transport-layer)
4. [Handshake Sequence](#handshake-sequence)
5. [Command Format](#command-format)
6. [Cryptographic Operations](#cryptographic-operations)
7. [Command Reference](#command-reference)
8. [Agent Protocol](#agent-protocol)
9. [Invitation Links](#invitation-links)
10. [Multi-Contact Architecture](#multi-contact-architecture)
11. [Error Handling](#error-handling)

---

## Protocol Overview

### What is SMP?

The **SimpleX Messaging Protocol (SMP)** is a transport-agnostic protocol for private, asynchronous messaging. It uses unidirectional message queues where:

- **Sender** can only send messages
- **Recipient** can only receive messages
- **Server** cannot correlate sender and recipient

### Protocol Properties

| Property | Description |
|----------|-------------|
| **No User IDs** | Queues identified by random IDs, not user identities |
| **Unidirectional** | Each queue is one-way; bidirectional needs two queues |
| **Asynchronous** | Store-and-forward; parties don't need to be online simultaneously |
| **Metadata Protection** | Server can't link sender to recipient |

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
│  └── Nonce: 24 bytes, Tag: 16 bytes                            │
├─────────────────────────────────────────────────────────────────┤
│  Layer 4: SMP Client Message                                    │
│  └── [2-byte length prefix] [encrypted_content] [padding]      │
├─────────────────────────────────────────────────────────────────┤
│  Layer 5: Contact DH Encryption (Initial Messages)              │
│  └── [X25519 SPKI key (44 bytes)] [crypto_box encrypted body]  │
│  └── crypto_box(body, nonce, sender_dh_pub, contact_dh_secret) │
├─────────────────────────────────────────────────────────────────┤
│  Layer 6: Agent Protocol Message                                │
│  └── [2-byte version BE] [type: 'C'/'I'/'M'/'R'] [body]        │
└─────────────────────────────────────────────────────────────────┘
```

### Layer Details

#### Layer 1: TLS 1.3 Transport

```
Host: smp3.simplexonflux.com (or any SMP server)
Port: 5223
Protocol: TLS 1.3
ALPN: "smp/1"
Cipher: TLS_CHACHA20_POLY1305_SHA256
```

#### Layer 2: SMP Transport Block

All SMP communication uses **16KB (16384 byte) blocks**, padded with `#` characters.

```
┌─────────────────────────────────────────────────────┐
│ Block (16384 bytes)                                 │
├──────────┬──────────────────────────────────────────┤
│ 2 bytes  │ Content (up to 16382 bytes)              │
│ Length   │ + Padding (# characters)                 │
└──────────┴──────────────────────────────────────────┘
```

#### Layer 3: SMP E2E Encryption

Server-side encryption using recipient's DH key.

```c
// Decrypt incoming messages
crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

#### Layer 4: SMP Client Message

```
┌──────────────────────────────────────────────────────────────────┐
│ Offset 0-1:   Length Prefix (BE)        │ z.B. 0x3E82 = 16002   │
│ Offset 2+:    Encrypted Content         │                       │
│ Padding:      To fill block             │                       │
└──────────────────────────────────────────────────────────────────┘
```

#### Layer 5: Contact DH Encryption (CRITICAL!)

**Initial messages** (AgentInvitation, AgentConfirmation) have an additional encryption layer using the contact's DH key exchange.

```
┌──────────────────────────────────────────────────────────────────┐
│ Offset 0-1:   Length Prefix (BE)        │                       │
│ Offset 2-5:   Unknown Header            │ 00 00 00 00           │
│ Offset 6-9:   "ip" + 2 bytes            │ 69 70 xx xx           │
│ Offset 10-13: "T " + version "1,"       │ 54 20 00 04 31 2c     │
│ Offset 14-57: X25519 SPKI (44 bytes)    │ 30 2a 30 05 06 03...  │
│ Offset 58+:   crypto_box encrypted body │ [nonce][ciphertext]   │
└──────────────────────────────────────────────────────────────────┘
```

**Decryption:**
```c
// Extract sender's DH public key from SPKI at offset 14
uint8_t sender_dh_pub[32];
memcpy(sender_dh_pub, &msg[14 + 12], 32);  // Skip 12-byte SPKI header

// Decrypt with contact's DH secret
crypto_box_beforenm(shared, sender_dh_pub, contact_dh_secret);
crypto_box_open_easy_afternm(plain, &msg[58], cipher_len, nonce, shared);
```

#### Layer 6: Agent Protocol Message

```
┌──────────────────────────────────────────────────────────────────┐
│ Offset 0-1:   Version (2-byte BE)       │ e.g., 0x0007 = v7     │
│ Offset 2:     Type                      │ 'C', 'I', 'M', 'R'    │
│ Offset 3+:    Body                      │ Type-specific         │
└──────────────────────────────────────────────────────────────────┘
```

---

## Handshake Sequence

### Overview

```
Client                          Server
   │                               │
   │──── TLS Handshake ───────────>│
   │<─── TLS Established ──────────│
   │                               │
   │<─── ServerHello ──────────────│
   │                               │
   │──── ClientHello ─────────────>│
   │                               │
   │     (Session Established)     │
   │                               │
```

### keyHash Computation

**CRITICAL**: keyHash must be computed from the **CA certificate** (second in chain), not the server certificate.

```c
uint8_t ca_hash[32];
mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
```

---

## Cryptographic Operations

### X25519 SPKI Header Format

```
SPKI Header (12 bytes): 30 2a 30 05 06 03 2b 65 6e 03 21 00
                        │  │  │  │  │  │  │  │  │  │  │  │
                        │  │  │  │  │  │  │  │  │  │  │  └─ 0x00
                        │  │  │  │  │  │  │  │  │  │  └─── BIT STRING length (33)
                        │  │  │  │  │  │  │  │  │  └────── BIT STRING tag (0x03)
                        │  │  │  │  │  │  └──┴──┴───────── OID 1.3.101.110 (X25519)
                        │  │  │  └──┴──┴────────────────── OID container
                        │  │  └─────────────────────────── AlgorithmIdentifier
                        │  └────────────────────────────── Total length (42)
                        └───────────────────────────────── SEQUENCE tag

Full SPKI (44 bytes) = Header (12 bytes) + Raw X25519 Key (32 bytes)
```

### Ed25519 SPKI Header Format

```c
static const uint8_t ED25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03,
    0x2b, 0x65, 0x70,        // OID 1.3.101.112 (Ed25519)
    0x03, 0x21, 0x00
};
```

### crypto_box vs raw X25519 (CRITICAL!)

```c
// ❌ WRONG: Raw X25519 shared secret is NOT a valid encryption key!
crypto_scalarmult(shared, secret, public);
crypto_secretbox_open_easy(plain, cipher, len, nonce, shared);

// ✅ CORRECT: crypto_box does HSalsa20 key derivation
crypto_box_beforenm(shared, public, secret);
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

---

## Agent Protocol

### Message Types

| Type | Name | Description |
|------|------|-------------|
| `'C'` | AgentConfirmation | Connection confirmation with encrypted connInfo |
| `'I'` | AgentInvitation | Invitation with reply queue URI + profile |
| `'M'` | AgentMsgEnvelope | Double Ratchet encrypted message |
| `'R'` | AgentRatchetKey | Ratchet key exchange |

### AgentInvitation Format (Type 'I')

```
AgentInvitation = [version:2][type:'I'][connReqLen:2][connReq][connInfo]

connReq = URL-encoded simplex:/invitation#/?v=2-7&smp=...
connInfo = Peer's profile (JSON or binary)
```

**Example decoded connReq:**
```
simplex:/invitation#/?v=2-7&smp=smp%3A%2F%2F6iIcWT_dF2zN_w5xzZEY7HI2Prbh3ldP07YTyDexPjE%3D%40smp10.simplex.im%2FzeKFSKNA_xTcbWniJn-gB4m9V2RIWZ...
```

### Agent Version vs SMP Version

| Version Type | Location | Format | Example |
|--------------|----------|--------|---------|
| SMP Version | Message header offset 12 | String | "1," |
| Agent Version | Layer 6 offset 0-1 | 2-byte BE integer | 0x0007 = v7 |

### Connection Flow (Contact Address q=c)

```
┌──────────┐                              ┌──────────┐
│ SimpleX  │                              │  ESP32   │
│   App    │                              │(Contact) │
└────┬─────┘                              └────┬─────┘
     │  1. Scannt Contact Link                 │
     │  2. SEND AgentInvitation ───────────────>  (Reply Queue + Profile)
     │  3. Wartet auf Accept...                │
     │     <───────────── AgentConfirmation    │  (Zu App's Reply Queue!)
     │  4. "Connected!"                        │
```

---

## Invitation Links

### URL Encoding (CRITICAL!)

**DH Key must use Base64URL encoding**, not Standard Base64!

```
Base64 Standard: A-Za-z0-9+/=
Base64URL:       A-Za-z0-9-_=
                           ^^
                         Different!
```

**Double encoding for `=` padding:**
```
=  →  %3D  →  %253D
```

**Comparison:**
```
FALSCH: dh%3DMCowBQYDK2VuAyEA5nPWbPZTKmf3NdwGzYOq...%2Bv24%3D
                                                    ^^^  ^^^
                                                 + and = single encoded (WRONG!)

RICHTIG: dh%3DMCowBQYDK2VuAyEABo11ArKXGHb9zoz_76yz...%253D
                                              ^       ^^^^^
                                           _ (Base64URL)  = double encoded
```

### Link Formats

```
📋 SMP Queue URI:
smp://keyHash@server:5223/senderId#/?v=1-4&dh=<base64url>&q=c

🌐 Web Link:
https://simplex.chat/contact#/?v=2-7&smp=<URL-ENCODED-SMP-URI>

📲 App Link:
simplex:/contact#/?v=2-7&smp=<URL-ENCODED-SMP-URI>
```

### Version Ranges

| Layer | Parameter | Value | Meaning |
|-------|-----------|-------|---------|
| Contact URI (outer) | `v` | `2-7` | Agent Version Range |
| SMP Queue (inner) | `v` | `1-4` | SMP Client Version Range |

---

## Command Reference

### Command Types

| Command | Type | EntityId | Description |
|---------|------|----------|-------------|
| NEW | - | empty | Create queue |
| SUB | Recipient | recipientId | Subscribe |
| SEND | Sender | senderId | Send message |
| ACK | Recipient | recipientId | Acknowledge |
| DEL | Recipient | recipientId | Delete queue |

### SEND Command Format (CRITICAL!)

```
"SEND" ' ' [msgFlags] ' ' [msgBody]
       ↑              ↑
      0x20           0x20

msgFlags: ASCII 'T' or 'F' (NOT binary 0x00/0x01!)
```

---

## Multi-Contact Architecture

### Data Structures

```c
#define MAX_CONTACTS 10

typedef struct {
    char name[32];
    uint8_t rcv_auth_secret[64];  // Ed25519 secret
    uint8_t rcv_auth_public[32];  // Ed25519 public
    uint8_t rcv_dh_secret[32];    // X25519 secret
    uint8_t rcv_dh_public[32];    // X25519 public
    uint8_t recipient_id[24];
    uint8_t sender_id[24];
    uint8_t srv_dh_public[32];
    uint8_t active;
} contact_t;

typedef struct {
    uint8_t num_contacts;
    contact_t contacts[MAX_CONTACTS];
} contacts_db_t;
```

---

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `ERR BLOCK` | Invalid block format | Check 16KB padding |
| `ERR CMD SYNTAX` | Malformed command | Check SEND format |
| `ERR AUTH` | Signature failed | Use libsodium |
| `Invalid link` | Wrong URL encoding | Use Base64URL + double-encode = |

---

## Implementation Notes

### Critical Discoveries (v0.1.12)

| Discovery | Details |
|-----------|---------|
| Base64URL for DH | Use `-` and `_` instead of `+` and `/` |
| Double encode `=` | `=` → `%3D` → `%253D` in final URL |
| Layer 5 exists | Initial messages have extra DH encryption |
| SPKI at offset 14 | 44-byte X25519 key embedded in decrypted msg |
| Agent version | 2-byte BE integer (e.g., 0x0007 = v7) |
| Type 'I' | AgentInvitation contains reply queue URI |

### Haskell Source References

| File | Discovery |
|------|-----------|
| Agent/Protocol.hs | Agent message types 'C', 'I', 'M', 'R' |
| Agent/Client.hs | AgentInvitation format with connReq |
| Crypto.hs | Double crypto_box layers |

---

## Performance (ESP32-S3 @ 240MHz)

| Operation | Time |
|-----------|------|
| Ed25519 sign | ~8ms |
| X25519 DH | ~8ms |
| crypto_box decrypt (Layer 3) | ~1ms |
| crypto_box decrypt (Layer 5) | ~1ms |
| Agent message parse | <1ms |
| TLS handshake | ~800ms |

---

## References

- [SMP Protocol Specification](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [SimpleX Agent Protocol](https://github.com/simplex-chat/simplexmq/tree/stable/src/Simplex/Messaging/Agent)
- [libsodium Documentation](https://doc.libsodium.org/)

---

*Last updated: January 21, 2026 — v0.1.12-alpha*
