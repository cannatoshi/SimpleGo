# SimpleX Messaging Protocol (SMP) - Implementation Guide

> Deep technical documentation for implementing SMP on embedded systems

---

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [SMP Version Differences](#smp-version-differences)
3. [Transport Layer](#transport-layer)
4. [Handshake Sequence](#handshake-sequence)
5. [Command Format](#command-format)
6. [Cryptographic Operations](#cryptographic-operations)
7. [Command Reference](#command-reference)
8. [Invitation Links](#invitation-links)
9. [Multi-Contact Architecture](#multi-contact-architecture)
10. [Key Persistence](#key-persistence)
11. [Error Handling](#error-handling)
12. [Implementation Notes](#implementation-notes)
13. [Performance & Memory](#performance--memory)

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

### Protocol Versions

| Version | Status | Notes |
|---------|--------|-------|
| v6 | ✅ Supported | Current minimum for SimpleGo |
| v7 | ✅ Compatible | implySessId, authEncryptCmds |
| v8 | ✅ Compatible | Current server max |
| v17 | Latest | Batch commands |

SimpleGo uses **v6** as the client version for maximum compatibility.

---

## SMP Version Differences

### Version Overview

| Version | Status | Key Features |
|---------|--------|--------------|
| **v6** | ✅ SimpleGo uses | Base protocol, full functionality |
| **v7** | Compatible | `implySessId`, `authEncryptCmds` |
| **v8-v16** | Compatible | Incremental improvements |
| **v17** | Latest | Batch commands, optimizations |

### Why SimpleGo Uses v6

v6 has **everything** needed for a complete messenger:
- ✅ Queue management (NEW, SUB, DEL)
- ✅ Message sending (SEND)
- ✅ Message receiving (MSG)
- ✅ Acknowledgment (ACK)
- ✅ E2E encryption (X25519 + XSalsa20-Poly1305)

---

## Transport Layer

### Connection Parameters

```
Host: smp3.simplexonflux.com (or any SMP server)
Port: 5223
Protocol: TLS 1.3
ALPN: "smp/1"
Cipher: TLS_CHACHA20_POLY1305_SHA256
```

### TLS Configuration (mbedTLS)

```c
// Force TLS 1.3 only
mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);
mbedtls_ssl_conf_max_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_3);

// Cipher suite
static const int ciphersuites[] = {
    MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
    0
};
mbedtls_ssl_conf_ciphersuites(&conf, ciphersuites);

// ALPN protocol
static const char *alpn_list[] = {"smp/1", NULL};
mbedtls_ssl_conf_alpn_protocols(&conf, alpn_list);

// SNI (Server Name Indication)
mbedtls_ssl_set_hostname(&ssl, "smp3.simplexonflux.com");
```

### Block Format

All SMP communication uses **16KB (16384 byte) blocks**, padded with `#` characters.

```
┌─────────────────────────────────────────────────┐
│ Block (16384 bytes)                             │
├──────────┬──────────────────────────────────────┤
│ 2 bytes  │ Content (up to 16382 bytes)          │
│ Length   │ + Padding (# characters)             │
└──────────┴──────────────────────────────────────┘

Length: Big-endian uint16, content length (excluding padding)
Content: Actual data
Padding: '#' (0x23) characters to fill 16384 bytes
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
// keyHash = SHA256(full DER of CA certificate)
uint8_t ca_hash[32];
mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
```

---

## Command Format

### Transmission Structure (v6)

```
┌──────────────────────────────────────────────────────────────────┐
│ Signed Transmission                                              │
├───────────┬───────────────────┬───────────┬──────────────────────┤
│ sigLen    │ signature         │ sessLen   │ sessionId            │
│ (1 byte)  │ (64 bytes)        │ (1 byte)  │ (32 bytes)           │
├───────────┴───────────────────┴───────────┴──────────────────────┤
│ Transmission Body                                                │
├───────────┬───────────────┬───────────┬──────────────────────────┤
│ corrIdLen │ corrId        │ entityLen │ entityId    │ command... │
│ (1 byte)  │ (variable)    │ (1 byte)  │ (variable)  │            │
└───────────┴───────────────┴───────────┴──────────────────────────┘
```

### Signature Computation (v6)

**Critical**: The signature covers `smpEncode(sessionId) + transmission_body`.

```c
// Build data to sign
uint8_t to_sign[256];
int sign_pos = 0;

to_sign[sign_pos++] = 32;  // Length prefix for sessionId (smpEncode)
memcpy(&to_sign[sign_pos], session_id, 32);
sign_pos += 32;
memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
sign_pos += trans_body_len;

// Sign with Ed25519 (libsodium)
uint8_t signature[64];
crypto_sign_detached(signature, NULL, to_sign, sign_pos, secret_key);
```

---

## Cryptographic Operations

### Ed25519 Key Generation

```c
#include "sodium.h"

uint8_t secret_key[crypto_sign_SECRETKEYBYTES];  // 64 bytes
uint8_t public_key[crypto_sign_PUBLICKEYBYTES];  // 32 bytes

uint8_t seed[32];
esp_fill_random(seed, 32);
crypto_sign_seed_keypair(public_key, secret_key, seed);
```

### X25519 Key Generation

```c
uint8_t dh_secret[32];
uint8_t dh_public[32];

esp_fill_random(dh_secret, 32);
crypto_scalarmult_base(dh_public, dh_secret);
```

### SPKI Key Encoding

SimpleX uses SPKI (Subject Public Key Info) format for public keys (44 bytes = 12-byte header + 32-byte key):

```c
static const uint8_t ED25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03,
    0x2b, 0x65, 0x70,        // OID 1.3.101.112 (Ed25519)
    0x03, 0x21, 0x00
};

static const uint8_t X25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03,
    0x2b, 0x65, 0x6e,        // OID 1.3.101.110 (X25519)
    0x03, 0x21, 0x00
};
```

### Message Decryption (E2E)

**CRITICAL**: The **server** encrypts messages for the recipient. The sending client does NOT encrypt — it sends plaintext, and the server uses the recipient's DH key to encrypt.

```c
// 1. Compute DH Shared Secret with HSalsa20 key derivation
uint8_t shared[crypto_box_BEFORENMBYTES];
crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);

// 2. Nonce = msgId (24 bytes, zero-padded)
uint8_t nonce[24] = {0};
memcpy(nonce, msg_id, msgIdLen);

// 3. Decrypt with NaCl crypto_box (XSalsa20-Poly1305)
crypto_box_open_easy_afternm(plain, cipher, cipher_len, nonce, shared);
```

**CRITICAL: crypto_box vs raw X25519**

```c
// ❌ WRONG: Raw X25519 shared secret is NOT a valid encryption key!
crypto_scalarmult(shared, secret, public);
crypto_secretbox_open_easy(plain, cipher, len, nonce, shared);

// ✅ CORRECT: crypto_box_beforenm does HSalsa20 key derivation
crypto_box_beforenm(shared, public, secret);
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

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

### NEW - Create Queue

**Request:**
```
"NEW " [keyLen][rcvAuthKey SPKI] [keyLen][rcvDhKey SPKI] [subMode]

rcvAuthKey: Ed25519 public key (SPKI, 44 bytes)
rcvDhKey: X25519 public key (SPKI, 44 bytes)  
subMode: 'S' for SMSubscribe (required for v6+)
```

**Response (IDS):**
```
"IDS " [len][recipientId] [len][senderId] [len][serverDhKey SPKI]

recipientId: 24 bytes, used for SUB/ACK/DEL commands
senderId: 24 bytes, used for SEND command and invitation links
serverDhKey: 44 bytes SPKI, for E2E decryption
```

### SUB - Subscribe to Queue

**Request:** `"SUB"` with EntityId = recipientId

**Response:** `OK` or `ERR [code]`

### SEND - Send Message

**CRITICAL FORMAT:**

```
"SEND" ' ' [msgFlags] ' ' [msgBody]
       ↑              ↑
      0x20           0x20

msgFlags: ASCII 'T' or 'F' (NOT binary 0x00/0x01!)
```

### ACK - Acknowledge Message

**Request:** `"ACK " [msgIdLen][msgId]` with EntityId = recipientId

**Response:** `OK` or `ERR NO_MSG`

### DEL - Delete Queue

**Request:** `"DEL"` with EntityId = recipientId (no parameters!)

**Response:** `OK`

---

## Invitation Links

### Overview

SimpleGo generates SimpleX-compatible invitation links that work with SimpleX Desktop/Mobile apps.

### Link Formats

```
📋 SMP Queue URI (raw):
smp://keyHash@server:5223/senderId#/?v=1-4&dh=<base64>&q=c

🌐 SimpleX Contact Link:
https://simplex.chat/contact#/?v=2-7&smp=<URL-ENCODED-SMP-URI>

📲 Direct App Link:
simplex:/contact#/?v=2-7&smp=<URL-ENCODED-SMP-URI>
```

### SMP Queue URI Format

```
smp://[keyHash]@[server]:[port]/[senderId]#/?v=[smpVersionRange]&dh=[dhKey]&q=[queueMode]
```

| Component | Description | Example |
|-----------|-------------|---------|
| `keyHash` | Base64url SHA256 of CA cert | `1jne379u7IDJSxAvXbWb_JgoE7iabcslX0LBF22Rej0` |
| `server` | SMP server hostname | `smp3.simplexonflux.com` |
| `port` | SMP port (default 5223) | `5223` |
| `senderId` | Base64url sender ID from IDS | `XLEVCxbNocUkdcmSuQJMHQ_efzha0W_R` |
| `v` | SMP client version range | `1-4` |
| `dh` | Base64 Standard SPKI X25519 key | `MCowBQYDK2VuAyEA...` |
| `q` | Queue mode | `c` (contact) |

### Contact URI Format

```
https://simplex.chat/contact#/?v=[agentVersionRange]&smp=[urlEncodedSmpUri]
```

| Component | Description | Example |
|-----------|-------------|---------|
| `v` | Agent version range | `2-7` |
| `smp` | URL-encoded SMP Queue URI | `smp%3A%2F%2F...` |

### URL-Encoding Rules

**Single encoded (standard URL encoding):**
```
:  →  %3A
/  →  %2F
@  →  %40
#  →  %23
?  →  %3F
&  →  %26
=  →  %3D
```

**Double encoded (ONLY for + and = in Base64 DH-Key):**
```
+  →  %2B  →  %252B
=  →  %3D  →  %253D
```

**Why double encoding?** The SMP URI is embedded inside the Contact URI. Special characters in the Base64 DH-key would break parsing, so they must be encoded twice.

### Base64 Encoding

**CRITICAL**: Use Base64 Standard encoding (NOT base64url!) for the DH key:
- Alphabet: `A-Za-z0-9+/`
- Padding: `=`

```c
void base64_standard_encode(const uint8_t *input, size_t len, char *output) {
    static const char table[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    // ... standard base64 encoding with + / = characters
}
```

### Version Ranges

| Layer | Parameter | Range | Meaning |
|-------|-----------|-------|---------|
| Contact URI (outer) | `v` | `2-7` | Agent Version Range |
| SMP Queue (inner) | `v` | `1-4` | SMP Client Version Range |

### Queue Mode

| Value | Meaning |
|-------|---------|
| `q=c` | Contact Queue (for contact links) |
| `q=m` | Message Queue (for groups, etc.) |

### Implementation Example

```c
void print_invitation_links(contact_t *c) {
    char dh_base64[64];
    char smp_uri[512];
    char contact_link[1024];
    
    // 1. Base64 encode SPKI DH key
    uint8_t spki[44];
    memcpy(spki, X25519_SPKI_HEADER, 12);
    memcpy(spki + 12, c->rcv_dh_public, 32);
    base64_standard_encode(spki, 44, dh_base64);
    
    // 2. Build SMP Queue URI
    snprintf(smp_uri, sizeof(smp_uri),
        "smp://%s@%s:%s/%s#/?v=1-4&dh=%s&q=c",
        key_hash_base64url,
        SMP_HOST,
        SMP_PORT,
        sender_id_base64url,
        dh_base64);
    
    // 3. URL-encode SMP URI (with double encoding for +/=)
    char encoded_smp[1024];
    url_encode_with_double(smp_uri, encoded_smp, sizeof(encoded_smp));
    
    // 4. Build Contact Link
    snprintf(contact_link, sizeof(contact_link),
        "https://simplex.chat/contact#/?v=2-7&smp=%s",
        encoded_smp);
    
    printf("🌐 SimpleX Contact Link:\n%s\n", contact_link);
}
```

### Haskell Source References

| File | Line | Content |
|------|------|---------|
| Protocol.hs | 1078-1085 | `crEncode` Contact URI format |
| Protocol.hs | SMPQueueUri | `v=1-4&dh=<key>&q=c` format |
| ConnectionRequestTests.hs | - | `simplex:/contact#/?v=2-7&smp=` |

---

## Multi-Contact Architecture

### Data Structures (v0.1.10+)

```c
#define MAX_CONTACTS 10

typedef struct {
    char name[32];
    uint8_t rcv_auth_secret[64];  // Ed25519 secret key
    uint8_t rcv_auth_public[32];  // Ed25519 public key
    uint8_t rcv_dh_secret[32];    // X25519 secret key
    uint8_t rcv_dh_public[32];    // X25519 public key
    uint8_t recipient_id[24];
    uint8_t recipient_id_len;
    uint8_t sender_id[24];
    uint8_t sender_id_len;
    uint8_t srv_dh_public[32];
    uint8_t have_srv_dh;
    uint8_t active;
} contact_t;

typedef struct {
    uint8_t num_contacts;
    contact_t contacts[MAX_CONTACTS];
} contacts_db_t;
```

### Message Routing

When MSG arrives, find contact by recipientId:

```c
int find_contact_by_recipient_id(const uint8_t *rcv_id, uint8_t len) {
    for (int i = 0; i < db.num_contacts; i++) {
        if (db.contacts[i].active &&
            db.contacts[i].recipient_id_len == len &&
            memcmp(db.contacts[i].recipient_id, rcv_id, len) == 0) {
            return i;
        }
    }
    return -1;
}
```

---

## Key Persistence

### NVS Storage (v0.1.10+)

The entire contacts database is stored as a single NVS blob:

```c
// Save
nvs_set_blob(handle, "contacts_db", &db, sizeof(contacts_db_t));

// Load
nvs_get_blob(handle, "contacts_db", &db, &size);
```

---

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `ERR BLOCK` | Invalid block format | Check 16KB padding, length prefix |
| `ERR CMD SYNTAX` | Malformed command | Check SEND format (two spaces, ASCII flags!) |
| `ERR AUTH` | Signature failed | Use libsodium, check signed data format |
| `ERR NO_QUEUE` | Queue doesn't exist | Check recipientId |
| `ERR NO_MSG` | Message not found | Already ACK'd |

### EntityId per Command

| Command | EntityId |
|---------|----------|
| NEW | empty |
| SUB | recipientId |
| SEND | senderId |
| ACK | recipientId |
| DEL | recipientId |

---

## Implementation Notes

### Critical Discoveries

| Discovery | Details |
|-----------|---------|
| keyHash | Must use CA certificate (2nd in chain) |
| Ed25519 | libsodium required (Monocypher incompatible) |
| Block Format | Commands ≠ Handshake format |
| SubMode | Required for SMP v6 NEW command |
| MsgFlags | ASCII 'T'/'F', NOT binary |
| SEND Format | Two spaces: `SEND ' ' flags ' ' body` |
| crypto_box | HSalsa20 key derivation required |
| Server Encryption | Server encrypts for recipient, not sender |
| Double Encoding | + and = in Base64 DH-key need double URL encoding |
| Base64 Standard | Use + / = characters, NOT base64url |

---

## Performance & Memory

### Performance (ESP32-S3 @ 240MHz)

| Operation | Time |
|-----------|------|
| Ed25519 sign | ~8ms |
| X25519 DH | ~8ms |
| crypto_box decrypt | ~1ms |
| Base64 encode | <1ms |
| URL encode | <1ms |
| TLS handshake | ~800ms |
| NVS read/write | ~5ms |

### Memory Usage

| Component | RAM Usage |
|-----------|-----------|
| TLS context | ~40KB |
| Block buffer | 16KB |
| contacts_db (10) | ~3KB |
| Link buffers | ~2KB |
| Total | ~62KB |

---

## Quick Reference

### Key Lengths

| Key Type | Secret | Public | SPKI |
|----------|--------|--------|------|
| Ed25519 | 64* | 32 | 44 |
| X25519 | 32 | 32 | 44 |

*libsodium Ed25519 secret key is 64 bytes (seed + public key)

---

## References

- [SMP Protocol Specification](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [SimpleX Haskell Source](https://github.com/simplex-chat/simplexmq)
- [libsodium Documentation](https://doc.libsodium.org/)
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)

---

*Last updated: January 20, 2026 — v0.1.11-alpha*
