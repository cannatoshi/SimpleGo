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

**Investigation**:
```bash
# In WSL, analyzing Haskell source
grep -r "keyHash" ~/simplexmq/src --include="*.hs"
```

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

**Impact**: Handshake started succeeding after this fix.

---

### Discovery #2: Monocypher vs libsodium Incompatibility

**Date**: January 19, 2026

**Problem**: Persistent `ERR AUTH` errors despite correct signature format.

**Investigation**:

Created test to verify signature locally:
```c
// Sign data
crypto_sign_detached(signature, NULL, data, data_len, secret_key);

// Verify locally
int result = crypto_sign_verify_detached(signature, data, data_len, public_key);
// Result: PASS - but server still rejects!
```

**Hypothesis**: Different Ed25519 implementations produce different signatures?

**Testing**:
```c
// Same seed, same data
// Monocypher: signature = 0x1a2b3c4d...
// libsodium:  signature = 0x5e6f7g8h... (DIFFERENT!)
```

**Root Cause**: Ed25519 has implementation variations:
- Different handling of scalar clamping
- Different internal reduction methods
- Both produce "valid" signatures, but only one matches what the server expects

**SimpleX Server Uses**: `crypton` library (Haskell), which is libsodium-compatible.

**Solution**: Switch from Monocypher to ESP-IDF's libsodium component:

```yaml
# idf_component.yml
dependencies:
  espressif/libsodium: "^1.0.20"
```

**Impact**: `ERR AUTH` → `QUEUE CREATED!` 🎉

---

### Discovery #3: Command vs Handshake Block Format

**Date**: January 19, 2026

**Problem**: `ERR BLOCK` when sending NEW command after successful handshake.

**Investigation**:
```bash
grep -r "tPutBlock\|TransportBlock" ~/simplexmq/src --include="*.hs"
```

**Finding**: SMP uses two different block formats:

**Handshake Block** (ServerHello, ClientHello):
```
┌─────────┬─────────────────────────┬─────────┐
│ Length  │ Content                 │ Padding │
│ 2 bytes │ variable                │ '#'     │
└─────────┴─────────────────────────┴─────────┘
```

**Command Block** (NEW, SUB, SEND, etc.):
```
┌─────────┬─────────┬─────────┬──────────────┬─────────┐
│ OrigLen │ TxCount │ TxLen   │ Transmission │ Padding │
│ 2 bytes │ 1 byte  │ 2 bytes │ variable     │ '#'     │
└─────────┴─────────┴─────────┴──────────────┴─────────┘

OrigLen = 1 + 2 + TxLen (total of TxCount + TxLen + Transmission)
TxCount = Number of transmissions (always 1 for client)
TxLen = Length of transmission data
```

**Solution**: Implement separate functions:
```c
smp_write_handshake_block()  // For handshake messages
smp_write_command_block()    // For commands
```

---

### Discovery #4: SubMode Parameter Required

**Date**: January 19, 2026

**Problem**: `ERR CMD SYNTAX` on NEW command.

**Investigation**:
```bash
grep -r "subMode\|SMSubscribe" ~/simplexmq/src --include="*.hs"
```

**Finding**: SMP v6+ requires a `subMode` parameter after the DH key:

```haskell
-- From Protocol.hs
data SMPSubscribeMode = SMSubscribe | SMOnlyCreate
```

**Solution**: Append `'S'` (SMSubscribe) to NEW command:
```c
// After rcvDhKey SPKI
trans_body[pos++] = 'S';  // subMode = SMSubscribe
```

---

### Discovery #5: Signed Data Format

**Date**: January 19, 2026

**Problem**: `ERR AUTH` with correct signature algorithm.

**Investigation**:
```bash
grep -r "signSMP\|smpEncode" ~/simplexmq/src --include="*.hs"
```

**Finding**: The signed data isn't just `sessionId + body`. It's `smpEncode(sessionId) + body`:

```haskell
signSMP sk sessId body = sign sk (smpEncode sessId <> body)
-- smpEncode adds length prefix!
```

**Correct signed data format**:
```
[0x20]           ← Length prefix (32 in decimal)
[sessionId]      ← 32 bytes
[trans_body]     ← Variable length
```

**Solution**:
```c
uint8_t to_sign[256];
int pos = 0;

to_sign[pos++] = 32;  // LENGTH PREFIX - this was missing!
memcpy(&to_sign[pos], session_id, 32);
pos += 32;
memcpy(&to_sign[pos], trans_body, trans_body_len);
pos += trans_body_len;

crypto_sign_detached(signature, NULL, to_sign, pos, secret_key);
```

---

### Discovery #6: MsgFlags Must Be ASCII

**Date**: January 20, 2026

**Problem**: `ERR CMD SYNTAX` on SEND command.

**Finding**: msgFlags = ASCII 'T' (0x54) or 'F' (0x46), NOT binary!

```haskell
-- Encoding.hs
True  = "T" (ASCII 0x54)
False = "F" (ASCII 0x46)
```

---

### Discovery #7: ACK/DEL Use recipientId

**Date**: January 20, 2026

**Problem**: `ERR AUTH` on ACK and DEL commands.

**Finding**: ACK and DEL are **Recipient commands** (like SUB), not Sender commands.

| Command | Type | EntityId |
|---------|------|----------|
| SUB | Recipient | recipientId |
| SEND | Sender | senderId |
| ACK | Recipient | recipientId ← NOT senderId! |
| DEL | Recipient | recipientId ← NOT senderId! |

---

### Discovery #8: DEL is Parameter-less

**Date**: January 20, 2026

**Finding**: DEL command has NO parameters.

```haskell
-- From Haskell source
DEL :: Command Recipient    -- Recipient Command
DEL -> e DEL_               -- Format: just "DEL", no params
```

---

## Debugging Journey

### Error State Progression

The path from "nothing works" to "full SMP client":

```
Timeline:
──────────────────────────────────────────────────────────────────────────────────>

[TLS FAIL] [ERR BLOCK] [ERR CMD] [ERR AUTH] [DECRYPT] [ACK OK] [NVS] [DEL] [FULL!]
    │          │          │          │          │         │       │     │      │
    ▼          ▼          ▼          ▼          ▼         ▼       ▼     ▼      ▼
  TLS 1.3   Block     SubMode   libsodium    E2E      Full    Keys  Queue  🏆
  + ALPN    format    + flags   signatures   works    cycle   save  delete
  fixed     fixed     added     working
```

### Detailed Error Analysis

| Error | Symptom | Root Cause | Fix |
|-------|---------|------------|-----|
| TLS handshake fail | -0x7780 | TLS version mismatch | Force TLS 1.3 only |
| No ServerHello | Timeout | Wrong ALPN | Set ALPN to "smp/1" |
| ERR BLOCK | After ClientHello | Wrong block format for commands | Use command block format |
| ERR CMD SYNTAX | After NEW | Missing subMode | Add 'S' parameter |
| ERR CMD SYNTAX | SEND | Binary msgFlags | ASCII 'T'/'F' |
| ERR AUTH | After adding subMode | Wrong signature | Switch to libsodium |
| ERR AUTH | ACK/DEL | Wrong entityId | Use recipientId |
| ERR SESSION | Testing variant | Missing sessionId | Add with length prefix |

### Debugging Techniques Used

1. **Haskell Source Analysis**
   ```bash
   # WSL terminal
   grep -r "pattern" ~/simplexmq/src --include="*.hs"
   ```

2. **Hex Dump Everything**
   ```c
   void hex_dump(const char *label, uint8_t *data, int len) {
       printf("%s: ", label);
       for (int i = 0; i < len; i++) printf("%02x", data[i]);
       printf("\n");
   }
   ```

3. **Local Signature Verification**
   ```c
   // Verify signature works locally before blaming server
   int result = crypto_sign_verify_detached(sig, data, len, pubkey);
   ```

4. **PING Test Isolation**
   - When authentication failed, tested with PING (no auth required)
   - Confirmed block format was correct
   - Isolated problem to signature generation

---

## Architecture Decisions

### Why ESP-IDF over Arduino?

| Aspect | Arduino | ESP-IDF |
|--------|---------|---------|
| TLS 1.3 | ⚠️ Limited | ✅ Full support |
| mbedTLS access | Wrapped | Direct |
| Memory control | Limited | Full |
| NVS access | Limited | Full |
| RTOS features | Hidden | Exposed |
| Production ready | Hobby | Yes |

**Decision**: ESP-IDF provides the control needed for protocol implementation.

### Why libsodium over mbedTLS Crypto?

| Aspect | mbedTLS | libsodium |
|--------|---------|-----------|
| Ed25519 | ⚠️ Optional | ✅ Native |
| X25519 | ⚠️ Limited | ✅ Native |
| crypto_box | ❌ No | ✅ Yes |
| API simplicity | Complex | Simple |
| SimpleX compatible | Unknown | ✅ Yes |

**Decision**: libsodium matches SimpleX server's crypton library.

### Why SMP v6?

| Version | Support | Risk |
|---------|---------|------|
| v6 | All servers | Low |
| v7 | Most servers | Low |
| v8 | Newest only | Higher |

**Decision**: v6 provides broadest compatibility with minimal feature loss.

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
│ recipientId (24 bytes) ─── Queue identifier for SUB     │
│ senderId (24 bytes)    ─── For sender identification    │
│ serverDhKey (X25519)   ─── Server's DH public key       │
└─────────────────────────────────────────────────────────┘
```

### Security Properties

| Property | Implementation |
|----------|----------------|
| Forward secrecy | X25519 per-queue keys |
| Authentication | Ed25519 signatures |
| Key isolation | Separate keys per queue |
| Randomness | ESP32 hardware RNG |

### Future: Double Ratchet

```
Current (v0.1.9):
  Client ──[E2E encrypted]──> Queue ──[E2E encrypted]──> Recipient
          └── XSalsa20-Poly1305 ──┘

Future (with Double Ratchet):
  Client ──[DR encrypted]──> Queue ──[DR encrypted]──> Recipient
          └── X3DH + Double Ratchet ──┘
```

---

## Protocol Reverse Engineering

### Information Sources

| Source | Usefulness | Notes |
|--------|------------|-------|
| Protocol spec | 70% | Missing implementation details |
| Haskell source | 95% | Authoritative but complex |
| simplexmq-js | 30% | Outdated protocol version |
| Wireshark | 50% | TLS 1.3 makes inspection hard |
| Trial & error | 100% | Essential for edge cases |

### Haskell Code Navigation

Key files in `simplexmq/src/Simplex/Messaging/`:

```
Protocol.hs      ─── Command definitions, parsing
Transport.hs     ─── Block framing, TLS handling
Client.hs        ─── Client-side logic
Server.hs        ─── Server-side (for understanding errors)
Crypto.hs        ─── Cryptographic operations
Encoding.hs      ─── Binary encoding helpers
```

### Useful grep Patterns

```bash
# Find encoding logic
grep -r "smpEncode\|Encoding" --include="*.hs"

# Find signature handling
grep -r "signSMP\|verifySMP" --include="*.hs"

# Find command format
grep -r "pattern NEW\|pattern SUB\|pattern DEL" --include="*.hs"

# Find error types
grep -r "ErrorType\|ERR" --include="*.hs"

# Find version handling
grep -r "implySessId\|authCmdsSMPVersion" --include="*.hs"
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
3. Get PING working (no auth)
4. Get NEW working (full auth)
5. Get SUB working
6. Get SEND working
7. Get MSG decrypt working
8. Get ACK working
9. Get DEL working
→ Full SMP Client!
```

**Why**: Each step isolated one failure mode.

### 3. The Source is Truth

**Documentation**: Helpful but incomplete
**Source code**: Definitive

When in doubt, read the Haskell source. It's complex but correct.

### 4. Error Messages are Clues

```
ERR BLOCK   → Block format problem
ERR CMD     → Command format problem
ERR AUTH    → Signature problem
ERR SESSION → SessionId problem
ERR NO_MSG  → Already ACK'd
ERR NO_QUEUE → Queue deleted or doesn't exist
```

Each error pointed to a specific layer.

### 5. Preserve Debug Code

```c
#ifdef DEBUG_PROTOCOL
    hex_dump("Transmission", transmission, trans_len);
    hex_dump("Signature", signature, 64);
    hex_dump("Signed data", to_sign, to_sign_len);
#endif
```

Commented debug code is invaluable when issues resurface.

### 6. Persist Early, Clear on Delete

- Save keys immediately after IDS response
- Clear keys immediately after successful DEL
- Power can fail anytime

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

### Key Lengths

| Key Type | Secret | Public | SPKI |
|----------|--------|--------|------|
| Ed25519 | 64* | 32 | 44 |
| X25519 | 32 | 32 | 44 |

*libsodium Ed25519 secret key is 64 bytes (seed + public key)

### Command Identifiers

| Command | EntityId | Auth Required |
|---------|----------|---------------|
| NEW | empty | Yes (rcvAuthKey) |
| SUB | recipientId | Yes (rcvAuthKey) |
| SEND | senderId | Yes (sndAuthKey) |
| ACK | recipientId | Yes (rcvAuthKey) |
| DEL | recipientId | Yes (rcvAuthKey) |
| PING | empty | No |

---

## 🏆 Milestone: Full Single-Queue SMP Client

As of v0.1.9-alpha, all base SMP commands implemented:

| Command | Function | Status |
|---------|----------|--------|
| NEW | Create queue | ✅ |
| SUB | Subscribe | ✅ |
| SEND | Send message | ✅ |
| MSG | Receive + decrypt | ✅ |
| ACK | Acknowledge | ✅ |
| DEL | Delete queue | ✅ |

**Achievement Unlocked: "First Complete Native ESP32 SimpleX SMP Client"**

---

## Contributing to Documentation

Found something missing or incorrect? Please:

1. Open an issue with details
2. Reference the Haskell source if applicable
3. Include test results if available

This documentation exists because implementing SMP from scratch was hard. Let's make it easier for the next person.

---

*Last updated: January 20, 2026 — v0.1.9-alpha*
