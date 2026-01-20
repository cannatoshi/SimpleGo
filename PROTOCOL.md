# SimpleX Messaging Protocol (SMP) - Implementation Guide

> Deep technical documentation for implementing SMP on embedded systems

---

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [Transport Layer](#transport-layer)
3. [Handshake Sequence](#handshake-sequence)
4. [Command Format](#command-format)
5. [Cryptographic Operations](#cryptographic-operations)
6. [Command Reference](#command-reference)
7. [Error Handling](#error-handling)
8. [Implementation Notes](#implementation-notes)

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
| v7 | ✅ Compatible | Adds implySessId, authEncryptCmds |
| v8-v16 | ✅ Compatible | Incremental features |
| v17 | ✅ Compatible | Current server max |

SimpleGo uses **v6** as the client version for maximum compatibility.

#### Version Differences (v6 vs v7+)

| Feature | v6 | v7+ |
|---------|----|----|
| sessionId in transmission | ✅ Sent | ❌ Not sent (implied) |
| sessionId in signature | ❌ Not included | ✅ Included |
| Command encryption | ❌ Plain | ✅ Optional (authEncryptCmds) |

From Haskell source:
```haskell
authCmdsSMPVersion = VersionSMP 7
implySessId = v >= authCmdsSMPVersion
```

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

### Transmission Structure

After handshake, all commands follow this transmission format:

```
┌──────────────────────────────────────────────────────────────────┐
│ Signed Transmission                                              │
├───────────┬───────────────────┬───────────┬─────────────────────-┤
│ sigLen    │ signature         │ sessLen   │ sessionId            │
│ (1 byte)  │ (64 bytes)        │ (1 byte)  │ (32 bytes)           │
├───────────┴───────────────────┴───────────┴──────────────────────┤
│ Transmission Body                                                │
├───────────┬───────────────┬───────────┬──────────────────────────┤
│ corrIdLen │ corrId        │ entityLen │ entityId    │ command... │
│ (1 byte)  │ (variable)    │ (1 byte)  │ (variable)  │            │
└───────────┴───────────────┴───────────┴──────────────────────────┘
```

### Signature Computation

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

### Message Decryption (E2E)

```c
// 1. Compute DH Shared Secret
uint8_t shared[crypto_box_BEFORENMBYTES];  // 32 bytes
crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);

// 2. Nonce = msgId (24 bytes, zero-padded)
uint8_t nonce[24] = {0};
memcpy(nonce, msg_id, msgIdLen < 24 ? msgIdLen : 24);

// 3. Decrypt with NaCl crypto_box (XSalsa20-Poly1305)
int result = crypto_box_open_easy_afternm(
    plaintext, ciphertext, ciphertext_len, nonce, shared);
// result == 0 = success
```

---

## Command Reference

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

recipientId: 24 bytes, used for SUB/ACK commands
senderId: 24 bytes, used for SEND command
serverDhKey: 44 bytes SPKI, for key exchange
```

### SUB - Subscribe to Queue

Subscribe to receive messages from a queue.

**Request:**
```
"SUB"

EntityId: recipientId from IDS response
Auth: Signed with rcvAuthKey
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

EntityId: senderId from IDS response
msgFlags: 'T' or 'F' (ASCII, NOT binary!)
msgBody: Message content (encrypted)
Auth: None for unsecured queues (authLen = 0)
```

**Response:**
```
"OK" - Message delivered
"MSG " [msgId] [encrypted body] - Echo back to subscriber
```

### ACK - Acknowledge Message ✅ NEW in v0.1.7

Acknowledge receipt of a message, removing it from the queue.

**Request:**
```
"ACK " [msgIdLen][msgId]

EntityId: recipientId (NOT senderId!)
Auth: Signed with rcvAuthKey
```

**Response:**
```
"OK" - Message acknowledged and deleted
"ERR NO_MSG" - Message not found
```

**Implementation:**
```c
uint8_t ack_body[64];
int ap = 0;

// CorrId
ack_body[ap++] = 1;
ack_body[ap++] = '4';

// EntityId = recipientId (CRITICAL: NOT senderId!)
ack_body[ap++] = recipient_id_len;
memcpy(&ack_body[ap], recipient_id, recipient_id_len);
ap += recipient_id_len;

// Command: "ACK " + msgId
ack_body[ap++] = 'A';
ack_body[ap++] = 'C';
ack_body[ap++] = 'K';
ack_body[ap++] = ' ';

// msgId with length prefix
ack_body[ap++] = msgIdLen;
memcpy(&ack_body[ap], msg_id, msgIdLen);
ap += msgIdLen;

// Sign with rcvAuthKey
// ... (standard signature process)
```

### DEL - Delete Queue (Planned)

Delete a queue from the server.

**Request:**
```
"DEL"

EntityId: recipientId
Auth: Signed with rcvAuthKey
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
| `ERR NO_MSG` | Message not found | Already ACK'd or expired |
| `ERR QUOTA` | Server quota exceeded | Try different server |

---

## Implementation Notes

### Critical Discoveries

#### 1. keyHash Source

**Problem**: Connection rejected with invalid keyHash.

**Solution**: keyHash must be computed from the **CA certificate** (second in chain), not the server certificate.

#### 2. Ed25519 Library Compatibility

**Problem**: `ERR AUTH` despite correct signature format.

**Solution**: Use libsodium, not Monocypher. SimpleX servers use crypton (libsodium-compatible).

#### 3. Block Format Differentiation

**Problem**: `ERR BLOCK` after successful handshake.

**Solution**: Handshake messages and commands use different block formats.

#### 4. MsgFlags Encoding

**Problem**: `ERR CMD SYNTAX` on SEND.

**Solution**: msgFlags must be ASCII 'T' or 'F', NOT binary 0x00/0x01.

#### 5. ACK EntityId

**Problem**: `ERR AUTH` on ACK command.

**Solution**: ACK uses recipientId as EntityId, NOT senderId. ACK is a Recipient command.

### Performance Considerations

| Operation | Time (ESP32-S3) |
|-----------|-----------------|
| Ed25519 keygen | ~8ms |
| Ed25519 sign | ~8ms |
| Ed25519 verify | ~21ms |
| X25519 keygen | ~8ms |
| crypto_box decrypt | ~1ms |
| SHA-256 | <1ms |
| TLS handshake | ~800ms |

### Memory Usage

| Component | RAM Usage |
|-----------|-----------|
| TLS context | ~40KB |
| Block buffer | 16KB |
| Key storage | ~256 bytes |
| Total | ~60KB |

---

## References

- [SMP Protocol Specification](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [SimpleX Haskell Source](https://github.com/simplex-chat/simplexmq)
- [RFC 8032 - Ed25519](https://tools.ietf.org/html/rfc8032)
- [RFC 7748 - X25519](https://tools.ietf.org/html/rfc7748)
- [libsodium Documentation](https://doc.libsodium.org/)
