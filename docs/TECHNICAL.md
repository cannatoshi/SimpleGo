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

All existing SimpleX clients (iOS, Android, Desktop, CLI) share a common architecture:

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
- No existing documentation for standalone implementation

### The Goal

Build a **native C implementation** that can run on resource-constrained hardware:

```
┌─────────────────┐
│  SimpleGo       │
│  (Pure C)       │
│                 │
│  ESP32-S3       │
│  320KB RAM      │
│  240MHz         │
└─────────────────┘
```

### Why This Matters

1. **First of its kind** — No known native SMP implementation exists
2. **Embedded privacy** — Secure messaging on dedicated hardware
3. **Documentation** — Creates reference for future implementations
4. **Independence** — No reliance on Haskell ecosystem

---

## Critical Discoveries

### Discovery #1: keyHash Must Use CA Certificate

**Date**: January 18, 2026

**Problem**: ClientHello was rejected despite correct format.

**Finding**: The keyHash in SMP URLs refers to the **CA certificate** fingerprint, not the server certificate.

**ServerHello certificate structure**:
```
[Server Certificate (online cert)]
[CA Certificate] ← Use THIS for keyHash!
```

**Solution**:
```c
// Parse both certificates
parse_cert_chain(hello, content_len, 
    &cert1_off, &cert1_len,   // Server cert
    &cert2_off, &cert2_len);  // CA cert

// Hash the CA certificate
mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
```

---

### Discovery #2: Monocypher vs libsodium Incompatibility

**Date**: January 19, 2026

**Problem**: Persistent `ERR AUTH` errors despite correct signature format.

**Root Cause**: Ed25519 has implementation variations. Monocypher and libsodium produce **different signatures** for identical input!

**SimpleX Server Uses**: `crypton` library (Haskell), which is libsodium-compatible.

**Solution**: Switch from Monocypher to ESP-IDF's libsodium component.

---

### Discovery #3: Command vs Handshake Block Format

**Date**: January 19, 2026

**Problem**: `ERR BLOCK` when sending NEW command after successful handshake.

**Finding**: SMP uses two different block formats:

**Handshake Block**:
```
[Length 2 bytes][Content][Padding '#']
```

**Command Block**:
```
[OrigLen 2 bytes][TxCount 1 byte][TxLen 2 bytes][Transmission][Padding '#']
```

---

### Discovery #4: MsgFlags Must Be ASCII

**Date**: January 20, 2026

**Problem**: `ERR CMD SYNTAX` on SEND command.

**Finding**: msgFlags must be ASCII 'T' (0x54) or 'F' (0x46), NOT binary 0x01/0x00!

From Haskell Encoding.hs:
```haskell
True  = "T" (ASCII 0x54)
False = "F" (ASCII 0x46)
```

---

### Discovery #5: ACK Uses recipientId, Not senderId

**Date**: January 20, 2026

**Problem**: `ERR AUTH` on ACK command.

**Finding**: ACK is a **Recipient command** (like SUB), not a Sender command (like SEND). Therefore:
- EntityId = recipientId
- Signed with rcv_auth_secret

---

## Debugging Journey

### Error State Progression

```
Timeline:
─────────────────────────────────────────────────────────────────────────>

[TLS FAIL]  [ERR BLOCK]  [ERR CMD]   [ERR AUTH]  [DECRYPT]   [ACK OK]
    │           │            │           │           │           │
    ▼           ▼            ▼           ▼           ▼           ▼
  TLS 1.3    Block        SubMode    libsodium   E2E works   Full
  + ALPN     format       + flags    signatures  v0.1.6      lifecycle
  fixed      fixed        fixed      working                 v0.1.7
```

### Detailed Error Analysis

| Error | Symptom | Root Cause | Fix |
|-------|---------|------------|-----|
| TLS handshake fail | -0x7780 | TLS version mismatch | Force TLS 1.3 only |
| No ServerHello | Timeout | Wrong ALPN | Set ALPN to "smp/1" |
| ERR BLOCK | After ClientHello | Wrong block format for commands | Use command block format |
| ERR CMD SYNTAX | After NEW | Missing subMode | Add 'S' parameter |
| ERR CMD SYNTAX | SEND command | Binary msgFlags | Use ASCII 'T'/'F' |
| ERR AUTH | After adding subMode | Wrong signature | Switch to libsodium |
| ERR AUTH | ACK command | Wrong entityId | Use recipientId, not senderId |

---

## Architecture Decisions

### Why ESP-IDF over Arduino?

| Aspect | Arduino | ESP-IDF |
|--------|---------|---------|
| TLS 1.3 | ⚠️ Limited | ✅ Full support |
| mbedTLS access | Wrapped | Direct |
| Memory control | Limited | Full |
| RTOS features | Hidden | Exposed |
| Production ready | Hobby | Yes |

### Why libsodium over mbedTLS Crypto?

| Aspect | mbedTLS | libsodium |
|--------|---------|-----------|
| Ed25519 | ⚠️ Optional | ✅ Native |
| X25519 | ⚠️ Limited | ✅ Native |
| crypto_box | ❌ No | ✅ Yes |
| SimpleX compatible | Unknown | ✅ Yes |

### Why SMP v6?

v6 provides everything needed for a complete messenger:
- ✅ Queue management (NEW, SUB, DEL)
- ✅ Message sending (SEND)
- ✅ Message receiving (MSG)
- ✅ Acknowledgment (ACK)
- ✅ E2E encryption

v7+ adds optimizations (implySessId, authEncryptCmds) but no critical features.

---

## Cryptographic Considerations

### Key Management

```
┌─────────────────────────────────────────────────────────┐
│ Per-Queue Keys                                          │
├─────────────────────────────────────────────────────────┤
│ rcvAuthKey (Ed25519)  ─── Signs commands to server      │
│ rcvDhKey (X25519)     ─── Key exchange with sender      │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│ From Server (IDS Response)                              │
├─────────────────────────────────────────────────────────┤
│ recipientId (24 bytes) ─── For SUB, ACK commands        │
│ senderId (24 bytes)    ─── For SEND command             │
│ serverDhKey (X25519)   ─── Server's DH public key       │
└─────────────────────────────────────────────────────────┘
```

### E2E Encryption Flow

```
SEND:
  Client → [plaintext] → Server encrypts → [ciphertext] → Queue

MSG:
  Queue → [ciphertext] → Client decrypts with DH shared secret → [plaintext]

Decryption:
  shared_secret = X25519(srv_dh_public, rcv_dh_secret)
  nonce = msgId (zero-padded to 24 bytes)
  plaintext = crypto_box_open(ciphertext, nonce, shared_secret)
```

---

## Protocol Reverse Engineering

### Information Sources

| Source | Usefulness | Notes |
|--------|------------|-------|
| Protocol spec | 70% | Missing implementation details |
| Haskell source | 95% | Authoritative but complex |
| Trial & error | 100% | Essential for edge cases |

### Key Haskell Files

```
simplexmq/src/Simplex/Messaging/
├── Protocol.hs      ─── Command definitions
├── Transport.hs     ─── Block framing
├── Client.hs        ─── Client-side logic
├── Server.hs        ─── Server-side (for understanding errors)
├── Crypto.hs        ─── Cryptographic operations
└── Encoding.hs      ─── Binary encoding helpers
```

### Useful grep Patterns

```bash
# Find ACK handling
grep -rn "ACK_\|pattern ACK" src/Simplex/Messaging/Protocol.hs

# Find version differences
grep -rn "implySessId\|authCmdsSMPVersion" src/Simplex/Messaging/

# Find encoding logic
grep -r "smpEncode\|Encoding" --include="*.hs"
```

---

## Lessons Learned

### 1. Test Assumptions Early

**Assumption**: "Ed25519 is Ed25519"
**Reality**: Implementation differences exist
**Lesson**: Verify crypto library compatibility before committing

### 2. Incremental Debugging

**Approach that worked**:
```
1. Get TLS working (isolated test)
2. Get handshake working (isolated test)
3. Get NEW working (full auth)
4. Get SUB working
5. Get SEND working
6. Get MSG decrypt working
7. Get ACK working
```

### 3. The Source is Truth

When in doubt, read the Haskell source. It's complex but correct.

### 4. Error Messages are Clues

```
ERR BLOCK   → Block format problem
ERR CMD     → Command format problem
ERR AUTH    → Signature or entityId problem
ERR NO_MSG  → Message already ACK'd
```

### 5. EntityId Matters

Different commands use different entityIds:
- NEW: empty
- SUB: recipientId
- SEND: senderId
- ACK: recipientId (NOT senderId!)

---

## Appendix: Quick Reference

### Block Sizes

| Constant | Value | Usage |
|----------|-------|-------|
| SMP_BLOCK_SIZE | 16384 | All blocks |
| MAX_CONTENT | 16382 | Block - 2 byte header |
| SESSION_ID_LEN | 32 | Session identifier |
| ED25519_SIG_LEN | 64 | Signature size |
| SPKI_KEY_SIZE | 44 | 12 header + 32 key |

### Command EntityIds

| Command | EntityId | Auth Required |
|---------|----------|---------------|
| NEW | empty | Yes (rcvAuthKey) |
| SUB | recipientId | Yes (rcvAuthKey) |
| SEND | senderId | No (unsecured queue) |
| ACK | recipientId | Yes (rcvAuthKey) |
| DEL | recipientId | Yes (rcvAuthKey) |

---

*Last updated: January 20, 2026 — v0.1.7-alpha*
