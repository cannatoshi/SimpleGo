# SimpleGo Technical Documentation

> Key learnings, discoveries, and implementation decisions from developing the first native SMP client

---

## Table of Contents

1. [Project Background](#project-background)
2. [Critical Discoveries](#critical-discoveries)
3. [Debugging Journey](#debugging-journey)
4. [Architecture Decisions](#architecture-decisions)
5. [Cryptographic Considerations](#cryptographic-considerations)
6. [Protocol Reverse Engineering](#protocol-reverse-engineering)
7. [Lessons Learned](#lessons-learned)

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

This means:
- Heavy runtime dependency (~50MB+ for Haskell runtime)
- Complex FFI bindings
- Not portable to embedded systems

### The Goal

Build a **native C implementation** for resource-constrained hardware:

```
┌─────────────────┐
│  SimpleGo       │
│  (Pure C)       │
│  ESP32-S3       │
│  320KB RAM      │
└─────────────────┘
```

---

## Critical Discoveries

### Discovery #1: keyHash Must Use CA Certificate

**Problem**: ClientHello rejected despite correct format.

**Finding**: keyHash must be computed from the **CA certificate** (2nd in chain), not the server certificate.

**Solution**:
```c
// Hash the CA certificate, not server cert!
mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
```

---

### Discovery #2: Monocypher vs libsodium Incompatibility

**Problem**: Persistent `ERR AUTH` errors.

**Finding**: Monocypher and libsodium produce **different Ed25519 signatures** for identical input! SimpleX servers use crypton (libsodium-compatible).

**Solution**: Use ESP-IDF's libsodium component.

---

### Discovery #3: Command vs Handshake Block Format

**Problem**: `ERR BLOCK` when sending commands.

**Finding**: SMP uses two different block formats — handshake blocks vs command blocks with transmission headers.

---

### Discovery #4: SubMode Parameter Required

**Problem**: `ERR CMD SYNTAX` on NEW command.

**Finding**: SMP v6+ requires `subMode` parameter ('S' for SMSubscribe).

---

### Discovery #5: MsgFlags Must Be ASCII

**Problem**: `ERR CMD SYNTAX` on SEND command.

**Finding**: msgFlags = ASCII 'T' or 'F', NOT binary 0x00/0x01!

```c
// ❌ WRONG
body[pos++] = 0x00;

// ✅ CORRECT
body[pos++] = 'F';  // ASCII 0x46
```

---

### Discovery #6: SEND Format Has Two Spaces

**Problem**: `ERR CMD SYNTAX` on SEND even with ASCII flags.

**Finding**: SEND command format is `SEND ' ' flags ' ' body` — two spaces!

**Haskell Source:**
```haskell
-- Protocol.hs line 1697
SEND flags msg -> e (SEND_, ' ', flags, ' ', Tail msg)
```

---

### Discovery #7: crypto_box vs raw X25519

**Problem**: E2E decryption failed despite correct keys.

**Finding**: NaCl's `crypto_box` uses HSalsa20 to derive the encryption key from the X25519 shared secret. Raw `crypto_scalarmult` output is NOT a valid encryption key!

```c
// ❌ WRONG: Raw X25519 shared secret
crypto_scalarmult(shared, secret, public);
crypto_secretbox_open_easy(plain, cipher, len, nonce, shared);

// ✅ CORRECT: crypto_box does HSalsa20 key derivation
crypto_box_beforenm(shared, public, secret);
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

---

### Discovery #8: Server-Side Encryption

**Finding**: The **server** encrypts messages for the recipient. The sending client does NOT encrypt — it sends plaintext.

**Haskell Source:**
```haskell
-- Server.hs line 2024
C.cbEncryptMaxLenBS (rcvDhSecret qr) (C.cbNonce msgId')
```

The server uses the recipient's DH key (`rcvDhSecret`) to encrypt messages.

---

### Discovery #9: ACK/DEL Use recipientId

**Problem**: `ERR AUTH` on ACK and DEL commands.

**Finding**: ACK and DEL are **Recipient commands** — entityId must be recipientId, not senderId.

| Command | Type | EntityId |
|---------|------|----------|
| SUB | Recipient | recipientId |
| SEND | Sender | senderId |
| ACK | Recipient | recipientId |
| DEL | Recipient | recipientId |

---

## Debugging Journey

### Error State Progression

```
Timeline:
──────────────────────────────────────────────────────────────────────────────────>

[TLS] [BLOCK] [CMD] [AUTH] [DECRYPT] [SEND] [E2E FIX] [MULTI]
  │      │      │      │       │        │       │        │
  ▼      ▼      ▼      ▼       ▼        ▼       ▼        ▼
TLS    Block  SubMode libsodium First  ASCII  HSalsa20 10 contacts
1.3    format added   works    decrypt flags   key      working!
```

### Detailed Error Analysis

| Error | Root Cause | Fix |
|-------|------------|-----|
| TLS fail | Version mismatch | Force TLS 1.3 |
| ERR BLOCK | Wrong block format | Use command block format |
| ERR CMD SYNTAX | Missing subMode | Add 'S' parameter |
| ERR CMD SYNTAX | Binary flags | Use ASCII 'T'/'F' |
| ERR CMD SYNTAX | SEND format | Two spaces! |
| ERR AUTH | Wrong crypto lib | Switch to libsodium |
| ERR AUTH | Wrong entityId | recipientId for ACK/DEL |
| Decrypt fail | Raw X25519 | Use crypto_box_beforenm |

---

## Architecture Decisions

### Why ESP-IDF over Arduino?

| Aspect | Arduino | ESP-IDF |
|--------|---------|---------|
| TLS 1.3 | Limited | Full support |
| mbedTLS access | Wrapped | Direct |
| NVS access | Limited | Full |
| Production ready | Hobby | Yes |

### Why libsodium over mbedTLS Crypto?

| Aspect | mbedTLS | libsodium |
|--------|---------|-----------|
| Ed25519 | Optional | Native |
| crypto_box | No | Yes |
| SimpleX compatible | Unknown | ✅ Yes |

### Multi-Contact Architecture

```c
typedef struct {
    char name[32];
    uint8_t rcv_auth_secret[64];
    uint8_t rcv_auth_public[32];
    uint8_t rcv_dh_secret[32];
    uint8_t rcv_dh_public[32];
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

All contacts share ONE TLS connection but have separate crypto keys.

---

## Cryptographic Considerations

### Key Management

```
Per-Contact Keys:
├── rcvAuthKey (Ed25519) ─── Signs commands
├── rcvDhKey (X25519)    ─── Key exchange
└── srvDhKey (X25519)    ─── From server (IDS response)

Shared:
└── sessionId ─── From ServerHello (one per TLS session)
```

### E2E Encryption Flow

```
SEND (plaintext)
     │
     ▼
   Server encrypts with rcvDhSecret + msgId as nonce
     │
     ▼
MSG (ciphertext)
     │
     ▼
Client decrypts:
  1. crypto_box_beforenm(shared, srvDhPub, rcvDhSecret)
  2. nonce = msgId (zero-padded to 24 bytes)
  3. crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared)
```

---

## Protocol Reverse Engineering

### Key Source Files

| File | Purpose |
|------|---------|
| Protocol.hs | Command definitions |
| Transport.hs | Block framing |
| Server.hs | Server-side logic |
| Crypto.hs | Cryptographic operations |

### Useful grep Commands

```bash
# Find SEND format
grep -r "pattern SEND\|SEND_" --include="*.hs"

# Find encryption
grep -r "cbEncrypt\|rcvDhSecret" --include="*.hs"

# Find nonce handling
grep -r "cbNonce" --include="*.hs"
```

---

## Lessons Learned

### 1. Test Assumptions Early

**Assumption**: "Ed25519 is Ed25519"  
**Reality**: Implementation differences exist  
**Lesson**: Verify crypto library compatibility first

### 2. Read the Source

When in doubt, read the Haskell source. It's complex but correct.

### 3. Error Messages are Clues

```
ERR BLOCK   → Block format
ERR CMD     → Command format
ERR AUTH    → Signature/EntityId
```

### 4. crypto_box ≠ crypto_scalarmult + crypto_secretbox

The NaCl `crypto_box` functions do HSalsa20 key derivation internally. You can't replicate them with raw primitives!

---

## Quick Reference

### Command EntityIds

| Command | EntityId |
|---------|----------|
| NEW | empty |
| SUB | recipientId |
| SEND | senderId |
| ACK | recipientId |
| DEL | recipientId |

### Key Lengths

| Key | Secret | Public | SPKI |
|-----|--------|--------|------|
| Ed25519 | 64 | 32 | 44 |
| X25519 | 32 | 32 | 44 |

---

## 🏆 Milestone: Multi-Contact E2E Client

As of v0.1.10-alpha:

- ✅ Multiple Contacts (10 slots, one connection)
- ✅ Full Message Lifecycle (NEW→SUB→SEND→MSG→DECRYPT→ACK)
- ✅ NVS Persistent Storage
- ✅ E2E Encryption (crypto_box)

**Achievement: "First Native ESP32 Multi-Contact SimpleX Client with E2E Encryption"**

---

*Last updated: January 20, 2026 — v0.1.10-alpha*
