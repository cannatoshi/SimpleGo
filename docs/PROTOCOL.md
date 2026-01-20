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
8. [Key Persistence](#key-persistence)
9. [Error Handling](#error-handling)

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

### v6 vs v7+ Technical Differences

| Feature | v6 | v7+ |
|---------|----|----|
| sessionId in transmission | ✅ Sent explicitly | ❌ Not sent (implied) |
| sessionId in signature | ❌ Not included | ✅ Included |
| Command encryption | ❌ Plain commands | ✅ Optional (`authEncryptCmds`) |
| Batch commands | ❌ Single | ✅ Multiple per block |

### Haskell Source Reference

```haskell
-- From Transport.hs
authCmdsSMPVersion = VersionSMP 7

-- Session ID handling differs by version:
implySessId = v >= authCmdsSMPVersion
-- v6: sessionId sent in transmission, NOT in signature
-- v7+: sessionId NOT sent, IS in signature
```

### Upgrade Strategy

```
v6 (current) ────────────────► v17 (future)
              skip v7-v16
```

When stable, upgrade directly to latest version for optimizations.
No intermediate versions needed — all are backwards compatible.

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
```

#### Handshake Block

```c
int smp_write_handshake_block(mbedtls_ssl_context *ssl, uint8_t *block,
                               const uint8_t *content, size_t content_len) {
    memset(block, '#', SMP_BLOCK_SIZE);
    block[0] = (content_len >> 8) & 0xFF;
    block[1] = content_len & 0xFF;
    memcpy(block + 2, content, content_len);
    return mbedtls_ssl_write(ssl, block, SMP_BLOCK_SIZE);
}
```

#### Command Block

```c
int smp_write_command_block(mbedtls_ssl_context *ssl, uint8_t *block,
                             const uint8_t *transmission, size_t trans_len) {
    memset(block, '#', SMP_BLOCK_SIZE);
    
    uint16_t orig_len = 1 + 2 + trans_len;
    block[0] = (orig_len >> 8) & 0xFF;
    block[1] = orig_len & 0xFF;
    block[2] = 1;  // txCount
    block[3] = (trans_len >> 8) & 0xFF;
    block[4] = trans_len & 0xFF;
    memcpy(&block[5], transmission, trans_len);
    
    return mbedtls_ssl_write(ssl, block, SMP_BLOCK_SIZE);
}
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
```

### ServerHello Format

```
┌────────────────────────────────────────────────────────┐
│ ServerHello                                            │
├───────────┬───────────┬───────────┬────────────────────┤
│ minVer    │ maxVer    │ sessIdLen │ sessionId          │
│ (2 bytes) │ (2 bytes) │ (1 byte)  │ (32 bytes)         │
├───────────┴───────────┴───────────┴────────────────────┤
│ Certificate Chain (DER encoded)                        │
│ [Server Certificate][CA Certificate]                   │
└────────────────────────────────────────────────────────┘
```

### Certificate Chain Parsing

**CRITICAL**: keyHash must use **CA certificate** (2nd in chain), NOT server certificate!

```c
// Parse certificate chain
parse_cert_chain(hello, content_len, 
    &cert1_off, &cert1_len,   // Server cert
    &cert2_off, &cert2_len);  // CA cert ← Use THIS!

// keyHash = SHA256(CA certificate)
mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
```

### ClientHello Format (v6)

```c
uint8_t client_hello[35];
int pos = 0;

client_hello[pos++] = 0x00;
client_hello[pos++] = 0x06;  // Version 6
client_hello[pos++] = 32;    // keyHash length
memcpy(&client_hello[pos], ca_hash, 32);
pos += 32;

smp_write_handshake_block(&ssl, block, client_hello, pos);
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
└───────────┴───────────────┴───────────┴──────────────────────────┘
```

### Signature Computation (v6)

**CRITICAL**: Signature covers `[0x20][sessionId] + transmission_body`

```c
uint8_t to_sign[256];
int sign_pos = 0;

to_sign[sign_pos++] = 32;  // Length prefix for sessionId!
memcpy(&to_sign[sign_pos], session_id, 32);
sign_pos += 32;
memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
sign_pos += trans_body_len;

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

```c
// Ed25519 SPKI Header (12 bytes)
static const uint8_t ED25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x70, 0x03, 0x21, 0x00
};

// X25519 SPKI Header (12 bytes)
static const uint8_t X25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03, 0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00
};

// SPKI = 12 byte header + 32 byte key = 44 bytes total
```

### Message Decryption (E2E)

```c
// 1. Compute DH Shared Secret
uint8_t shared[crypto_box_BEFORENMBYTES];
crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);

// 2. Nonce = msgId (24 bytes, zero-padded)
uint8_t nonce[24] = {0};
memcpy(nonce, msg_id, msgIdLen);

// 3. Decrypt with NaCl crypto_box (XSalsa20-Poly1305)
crypto_box_open_easy_afternm(plaintext, ciphertext, len, nonce, shared);
```

---

## Command Reference

### NEW - Create Queue

```
"NEW " [keyLen][rcvAuthKey SPKI] [keyLen][rcvDhKey SPKI] [subMode]

rcvAuthKey: Ed25519 public key (SPKI, 44 bytes)
rcvDhKey: X25519 public key (SPKI, 44 bytes)  
subMode: 'S' for SMSubscribe
```

**Response (IDS):**
```
"IDS " [len][recipientId] [len][senderId] [len][serverDhKey SPKI]
```

### SUB - Subscribe to Queue

```
"SUB"

EntityId: recipientId
Auth: Signed with rcvAuthKey
```

### SEND - Send Message

```
"SEND " [msgFlags] " " [msgBody]

EntityId: senderId
msgFlags: 'T' or 'F' (ASCII!)
Auth: None for unsecured queues
```

### ACK - Acknowledge Message

```
"ACK " [msgIdLen][msgId]

EntityId: recipientId (NOT senderId!)
Auth: Signed with rcvAuthKey
```

**Response:**
```
"OK"      - Message deleted from queue
"ERR NO_MSG" - Message not found
```

---

## Key Persistence

### NVS Storage (v0.1.8+)

Keys and queue IDs persist across reboots using ESP32's Non-Volatile Storage.

### Persisted Data

| NVS Key | Size | Description |
|---------|------|-------------|
| rcv_auth_sk | 64 bytes | Ed25519 Secret Key |
| rcv_auth_pk | 32 bytes | Ed25519 Public Key |
| rcv_dh_sk | 32 bytes | X25519 Secret Key |
| rcv_dh_pk | 32 bytes | X25519 Public Key |
| rcv_id | 24 bytes | Recipient ID |
| snd_id | 24 bytes | Sender ID |
| srv_dh_pk | 32 bytes | Server DH Key |

### Flow with Persistence

```
Start
  │
  ▼
TLS + Handshake
  │
  ▼
load_keys_from_nvs()
  │
  ├── Keys found? ──► Skip NEW ──► SUB directly
  │
  └── No keys? ──► NEW ──► save_keys_to_nvs() ──► SUB
```

---

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `ERR BLOCK` | Invalid block format | Check 16KB padding |
| `ERR CMD SYNTAX` | Malformed command | Check subMode, msgFlags |
| `ERR AUTH` | Signature failed | Use libsodium, check format |
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

## References

- [SMP Protocol Specification](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [SimpleX Haskell Source](https://github.com/simplex-chat/simplexmq)
- [libsodium Documentation](https://doc.libsodium.org/)
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)

---

*Last updated: January 20, 2026 — v0.1.8-alpha*
