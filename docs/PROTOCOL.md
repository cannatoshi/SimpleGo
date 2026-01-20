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
10. [Implementation Notes](#implementation-notes)
11. [Performance & Memory](#performance--memory)

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

Length: Big-endian uint16, content length (excluding padding)
Content: Actual data
Padding: '#' (0x23) characters to fill 16384 bytes
```

#### Block Writing (Handshake)

```c
int smp_write_handshake_block(mbedtls_ssl_context *ssl, uint8_t *block,
                               const uint8_t *content, size_t content_len) {
    memset(block, '#', SMP_BLOCK_SIZE);  // Fill with padding
    block[0] = (content_len >> 8) & 0xFF;  // Length high byte
    block[1] = content_len & 0xFF;          // Length low byte
    memcpy(block + 2, content, content_len);
    
    // Write entire block
    return mbedtls_ssl_write(ssl, block, SMP_BLOCK_SIZE);
}
```

#### Block Writing (Commands)

Commands use a different format with transmission headers:

```c
int smp_write_command_block(mbedtls_ssl_context *ssl, uint8_t *block,
                             const uint8_t *transmission, size_t trans_len) {
    memset(block, '#', SMP_BLOCK_SIZE);
    
    // originalLength = 1 (txCount) + 2 (txLen) + trans_len
    uint16_t orig_len = 1 + 2 + trans_len;
    block[0] = (orig_len >> 8) & 0xFF;
    block[1] = orig_len & 0xFF;
    
    // transmissionCount = 1
    block[2] = 1;
    
    // transmissionLength
    block[3] = (trans_len >> 8) & 0xFF;
    block[4] = trans_len & 0xFF;
    
    // transmission data
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
   │                               │
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

#### Parsing ServerHello

```c
uint8_t *hello = block + 2;  // Skip length prefix

// Protocol versions
uint16_t minVer = (hello[0] << 8) | hello[1];  // e.g., 6
uint16_t maxVer = (hello[2] << 8) | hello[3];  // e.g., 8

// Session ID
uint8_t sessIdLen = hello[4];  // Should be 32
memcpy(session_id, &hello[5], 32);

// Certificate chain starts at offset 37
```

### Certificate Chain Parsing

The ServerHello contains a certificate chain in DER format. **Critical**: The keyHash must be computed from the **CA certificate** (second certificate), not the server certificate.

```c
int parse_cert_chain(const uint8_t *data, int len,
                     int *cert1_off, int *cert1_len,
                     int *cert2_off, int *cert2_len) {
    // Find first certificate (0x30 0x82 = SEQUENCE with 2-byte length)
    for (int i = 0; i < len - 4; i++) {
        if (data[i] == 0x30 && data[i+1] == 0x82) {
            *cert1_off = i;
            *cert1_len = ((data[i+2] << 8) | data[i+3]) + 4;
            break;
        }
    }
    
    // Find second certificate (CA cert)
    int search_start = *cert1_off + *cert1_len;
    for (int i = search_start; i < len - 4; i++) {
        if (data[i] == 0x30 && data[i+1] == 0x82) {
            *cert2_off = i;
            *cert2_len = ((data[i+2] << 8) | data[i+3]) + 4;
            break;
        }
    }
    
    return 0;
}
```

### keyHash Computation

```c
// keyHash = SHA256(full DER of CA certificate)
uint8_t ca_hash[32];
if (cert2_off >= 0) {
    mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
} else {
    // Fallback to first cert (shouldn't happen normally)
    mbedtls_sha256(hello + cert1_off, cert1_len, ca_hash, 0);
}
```

### ClientHello Format (v6)

```
┌────────────────────────────────────────────┐
│ ClientHello                                │
├───────────┬───────────┬────────────────────┤
│ version   │ keyHashLen│ keyHash            │
│ (2 bytes) │ (1 byte)  │ (32 bytes)         │
└───────────┴───────────┴────────────────────┘
```

```c
uint8_t client_hello[35];
int pos = 0;

// Protocol version (big-endian)
client_hello[pos++] = 0x00;
client_hello[pos++] = 0x06;  // Version 6

// keyHash with length prefix
client_hello[pos++] = 32;  // keyHash length
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
│ (1 byte)  │ (variable)    │ (1 byte)  │ (variable)  │            │
└───────────┴───────────────┴───────────┴──────────────────────────┘
```

### Signature Computation (v6)

**Critical**: The signature covers `smpEncode(sessionId) + transmission_body`.

`smpEncode` adds a length prefix, so the signed data is:

```
[0x20][sessionId 32 bytes][transmission_body]
```

```c
// Build data to sign
uint8_t to_sign[256];
int sign_pos = 0;

to_sign[sign_pos++] = 32;  // Length prefix for sessionId
memcpy(&to_sign[sign_pos], session_id, 32);
sign_pos += 32;
memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
sign_pos += trans_body_len;

// Sign with Ed25519 (libsodium)
uint8_t signature[64];
crypto_sign_detached(signature, NULL, to_sign, sign_pos, secret_key);
```

### Building Complete Transmission

```c
uint8_t transmission[256];
int tpos = 0;

// 1. Signature with length prefix
transmission[tpos++] = 64;  // Ed25519 signature is 64 bytes
memcpy(&transmission[tpos], signature, 64);
tpos += 64;

// 2. SessionId with length prefix
transmission[tpos++] = 32;
memcpy(&transmission[tpos], session_id, 32);
tpos += 32;

// 3. Transmission body (corrId + entityId + command)
memcpy(&transmission[tpos], trans_body, trans_body_len);
tpos += trans_body_len;

// Send using command block format
smp_write_command_block(&ssl, block, transmission, tpos);
```

---

## Cryptographic Operations

### Ed25519 Key Generation

Using libsodium (ESP-IDF component):

```c
#include "sodium.h"

uint8_t secret_key[crypto_sign_SECRETKEYBYTES];  // 64 bytes
uint8_t public_key[crypto_sign_PUBLICKEYBYTES];  // 32 bytes

// Generate from random seed
uint8_t seed[32];
esp_fill_random(seed, 32);
crypto_sign_seed_keypair(public_key, secret_key, seed);
```

### X25519 Key Generation

```c
uint8_t dh_secret[32];
uint8_t dh_public[32];

// Generate random secret
esp_fill_random(dh_secret, 32);

// Derive public key
crypto_scalarmult_base(dh_public, dh_secret);
```

### SPKI Key Encoding

SimpleX uses SPKI (Subject Public Key Info) format for public keys:

```
SPKI = [12-byte header][32-byte public key]
Total: 44 bytes
```

#### Ed25519 SPKI Header

```c
static const uint8_t ED25519_SPKI_HEADER[12] = {
    0x30, 0x2a,              // SEQUENCE, 42 bytes
    0x30, 0x05,              // SEQUENCE, 5 bytes (AlgorithmIdentifier)
    0x06, 0x03,              // OID, 3 bytes
    0x2b, 0x65, 0x70,        // 1.3.101.112 (Ed25519)
    0x03, 0x21, 0x00         // BIT STRING, 33 bytes (0x00 + 32 key bytes)
};
```

#### X25519 SPKI Header

```c
static const uint8_t X25519_SPKI_HEADER[12] = {
    0x30, 0x2a,              // SEQUENCE, 42 bytes
    0x30, 0x05,              // SEQUENCE, 5 bytes (AlgorithmIdentifier)
    0x06, 0x03,              // OID, 3 bytes
    0x2b, 0x65, 0x6e,        // 1.3.101.110 (X25519)
    0x03, 0x21, 0x00         // BIT STRING, 33 bytes
};
```

#### Encoding Keys

```c
#define SPKI_KEY_SIZE 44

uint8_t ed25519_spki[SPKI_KEY_SIZE];
memcpy(ed25519_spki, ED25519_SPKI_HEADER, 12);
memcpy(ed25519_spki + 12, ed25519_public, 32);

uint8_t x25519_spki[SPKI_KEY_SIZE];
memcpy(x25519_spki, X25519_SPKI_HEADER, 12);
memcpy(x25519_spki + 12, x25519_public, 32);
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

### Signature Verification

Always verify signatures locally before sending:

```c
int verify_result = crypto_sign_verify_detached(
    signature, to_sign, to_sign_len, public_key);

if (verify_result == 0) {
    ESP_LOGI(TAG, "Signature OK");
} else {
    ESP_LOGE(TAG, "Signature FAILED!");
}
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

Create a new message queue on the server.

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

recipientId: 24 bytes, used for SUB command
senderId: 24 bytes, for sender identification
serverDhKey: 44 bytes SPKI, for key exchange
```

### SUB - Subscribe to Queue

Subscribe to receive messages from a queue.

**Request:**
```
"SUB"

EntityId: recipientId from IDS response
```

**Response:**
```
"OK" - Success
"ERR [code]" - Error
```

### SEND - Send Message

Send a message to a queue.

**Request:**
```
"SEND " [msgFlags] " " [msgBody]

EntityId: senderId
msgFlags: 'T' or 'F' (ASCII, NOT binary!)
msgBody: Encrypted message content
```

**Response:**
```
"OK" - Message queued
"ERR [code]" - Error
```

### ACK - Acknowledge Message

Acknowledge receipt of a message.

**Request:**
```
"ACK " [msgIdLen][msgId]

EntityId: recipientId (NOT senderId!)
Auth: Signed with rcvAuthKey
```

**Response:**
```
"OK" - Message deleted from queue
"ERR NO_MSG" - Message not found
```

### DEL - Delete Queue

Delete a queue from the server.

**Request:**
```
"DEL"

EntityId: recipientId (Recipient Command!)
Auth: Signed with rcvAuthKey
No parameters!
```

**Response:**
```
"OK" - Queue + all messages deleted from server
```

**Haskell Source:**
```haskell
DEL :: Command Recipient    -- Recipient Command
DEL -> e DEL_               -- Format: just "DEL", no params
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

### DEL + NVS Clear (v0.1.9)

When DEL command succeeds, local NVS keys are automatically cleared:

```c
// After successful DEL
if (response == OK) {
    clear_saved_keys();  // Wipe NVS
}
```

---

## Error Handling

### Error Response Format

```
"ERR " [errorCode] [details]
```

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `ERR BLOCK` | Invalid block format | Check 16KB padding, length prefix |
| `ERR CMD SYNTAX` | Malformed command | Check parameter encoding, subMode |
| `ERR AUTH` | Signature verification failed | Use libsodium, check signed data format |
| `ERR SESSION` | Invalid sessionId | Include sessionId in transmission |
| `ERR NO_QUEUE` | Queue doesn't exist | Check recipientId |
| `ERR QUOTA` | Server quota exceeded | Try different server |
| `ERR NO_MSG` | Message not found | Already ACK'd |

### Error Debugging Flow

```
ERR BLOCK
  └─> Fix block format (16KB, padding, length)
      └─> ERR CMD SYNTAX
          └─> Fix command format (subMode, parameters)
              └─> ERR AUTH
                  └─> Fix signature (libsodium, signed data format)
                      └─> SUCCESS!
```

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

#### 1. keyHash Source

**Problem**: Connection rejected with invalid keyHash.

**Discovery**: keyHash must be computed from the **CA certificate** (second in chain), not the server certificate.

#### 2. Ed25519 Library Compatibility

**Problem**: `ERR AUTH` despite correct signature format.

**Discovery**: Monocypher and libsodium produce **different Ed25519 signatures** for identical input! SimpleX servers use crypton (libsodium-compatible).

**Solution**: Use ESP-IDF's libsodium component.

#### 3. Block Format Differentiation

**Problem**: `ERR BLOCK` after successful handshake.

**Discovery**: Handshake messages and commands use different block formats.

#### 4. SubMode Requirement

**Problem**: `ERR CMD SYNTAX` on NEW command.

**Discovery**: SMP v6+ requires a `subMode` parameter on NEW command.

#### 5. MsgFlags Encoding

**Problem**: `ERR CMD SYNTAX` on SEND command.

**Discovery**: msgFlags must be ASCII 'T' or 'F', NOT binary 0x00/0x01!

#### 6. ACK/DEL EntityId

**Problem**: `ERR AUTH` on ACK/DEL commands.

**Discovery**: ACK and DEL are **Recipient commands** — entityId must be recipientId, not senderId.

---

## Performance & Memory

### Performance (ESP32-S3 @ 240MHz)

| Operation | Time |
|-----------|------|
| Ed25519 keygen | ~8ms |
| Ed25519 sign | ~8ms |
| Ed25519 verify | ~21ms |
| X25519 keygen | ~8ms |
| X25519 DH | ~8ms |
| SHA-256 | <1ms (HW) |
| crypto_box decrypt | ~1ms |
| TLS handshake | ~800ms |
| NVS read/write | ~5ms |

### Memory Usage

| Component | RAM Usage |
|-----------|-----------|
| TLS context | ~40KB |
| Block buffer | 16KB |
| Key storage | ~256 bytes |
| Total | ~60KB |

---

## Quick Reference

### Block Sizes

| Constant | Value | Usage |
|----------|-------|-------|
| SMP_BLOCK_SIZE | 16384 | All blocks |
| MAX_CONTENT | 16382 | Block - 2 byte header |
| SESSION_ID_LEN | 32 | Session identifier |
| ED25519_SIG_LEN | 64 | Signature size |
| SPKI_KEY_SIZE | 44 | 12 header + 32 key |

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
- [RFC 8032 - Ed25519](https://tools.ietf.org/html/rfc8032)
- [RFC 7748 - X25519](https://tools.ietf.org/html/rfc7748)
- [libsodium Documentation](https://doc.libsodium.org/)
- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)

---

*Last updated: January 20, 2026 — v0.1.9-alpha*
