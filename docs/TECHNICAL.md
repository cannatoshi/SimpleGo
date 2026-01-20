# SimpleGo Technical Documentation

> Key learnings, discoveries, and implementation decisions

---

## Table of Contents

1. [Project Background](#project-background)
2. [Critical Discoveries](#critical-discoveries)
3. [Architecture Decisions](#architecture-decisions)
4. [SMP Version Analysis](#smp-version-analysis)
5. [NVS Persistence](#nvs-persistence)
6. [Debugging Journey](#debugging-journey)
7. [Lessons Learned](#lessons-learned)

---

## Project Background

### The Challenge

All existing SimpleX clients use the same architecture:

```
┌─────────────────┐     ┌─────────────────┐
│  Native UI      │────>│  Haskell Core   │
│  (Swift/Kotlin/ │ FFI │  (simplexmq)    │
│   Electron)     │<────│                 │
└─────────────────┘     └─────────────────┘
```

**Problems**:
- Heavy runtime (~50MB+ for Haskell)
- Complex FFI bindings
- Not portable to embedded systems
- No standalone implementation documentation

### The Goal

**Native C implementation** for resource-constrained hardware:

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
3. **Documentation** — Reference for future implementations
4. **Independence** — No Haskell ecosystem dependency

---

## Critical Discoveries

### Discovery #1: keyHash Must Use CA Certificate

**Date**: January 18, 2026

**Problem**: ClientHello rejected despite correct format.

**Finding**: keyHash in SMP URLs = **CA certificate** fingerprint, NOT server certificate.

```
ServerHello certificates:
  [Server Certificate] ← NOT this
  [CA Certificate]     ← Use THIS for keyHash!
```

**Solution**:
```c
// Hash the CA certificate (2nd in chain)
mbedtls_sha256(hello + cert2_off, cert2_len, ca_hash, 0);
```

---

### Discovery #2: Monocypher vs libsodium Incompatibility

**Date**: January 19, 2026

**Problem**: Persistent `ERR AUTH` with correct signature format.

**Root Cause**: Ed25519 has implementation variations. Monocypher and libsodium produce **different signatures** for same input!

**SimpleX Server Uses**: `crypton` library (Haskell) = libsodium-compatible

**Solution**: Switch to ESP-IDF's libsodium component.

---

### Discovery #3: Block Format Differentiation

**Date**: January 19, 2026

**Problem**: `ERR BLOCK` when sending NEW command.

**Finding**: SMP uses two different block formats:

| Type | Format |
|------|--------|
| Handshake | `[Len 2B][Content][Padding]` |
| Command | `[OrigLen 2B][TxCount 1B][TxLen 2B][Transmission][Padding]` |

---

### Discovery #4: MsgFlags Must Be ASCII

**Date**: January 20, 2026

**Problem**: `ERR CMD SYNTAX` on SEND command.

**Finding**: msgFlags = ASCII 'T' (0x54) or 'F' (0x46), NOT binary!

```haskell
-- Encoding.hs
True  = "T" (ASCII 0x54)
False = "F" (ASCII 0x46)
```

---

### Discovery #5: ACK Uses recipientId

**Date**: January 20, 2026

**Problem**: `ERR AUTH` on ACK command.

**Finding**: ACK is a **Recipient command** (like SUB), not Sender command.

| Command | Type | EntityId |
|---------|------|----------|
| SUB | Recipient | recipientId |
| SEND | Sender | senderId |
| ACK | Recipient | recipientId ← NOT senderId! |

---

### Discovery #6: DEL is Parameter-less

**Date**: January 20, 2026

**Problem**: Understanding DEL command format.

**Finding**: DEL is a **Recipient command** with NO parameters.

```haskell
-- From Haskell source
DEL :: Command Recipient    -- Recipient Command
DEL -> e DEL_               -- Format: just "DEL", no params
```

**Key Points**:
- EntityId = recipientId (like SUB, ACK)
- Command = "DEL" (no space, no parameters)
- Response = "OK" (queue + messages deleted)
- After OK: Clear local NVS keys

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

### Why libsodium?

| Aspect | mbedTLS | libsodium |
|--------|---------|-----------|
| Ed25519 | ⚠️ Optional | ✅ Native |
| X25519 | ⚠️ Limited | ✅ Native |
| crypto_box | ❌ No | ✅ Yes |
| SimpleX compatible | Unknown | ✅ Yes |

---

## SMP Version Analysis

### Version Comparison

| Version | Key Feature | Impact |
|---------|-------------|--------|
| **v6** | Base protocol | ✅ What SimpleGo uses |
| **v7+** | `implySessId` | sessionId not sent, included in signature |
| **v7+** | `authEncryptCmds` | Commands encrypted with X25519 DH |
| **v17** | Batch commands | Multiple commands per block |

### Why v6?

v6 has **everything** needed:
- ✅ Queue management (NEW, SUB, DEL)
- ✅ Message sending (SEND)
- ✅ Message receiving (MSG)
- ✅ Acknowledgment (ACK)
- ✅ E2E encryption

v7+ adds **optimizations**, not critical features.

### Haskell Source Reference

```haskell
-- Transport.hs
authCmdsSMPVersion = VersionSMP 7

implySessId = v >= authCmdsSMPVersion
-- v6: sessionId sent in transmission, NOT in signature
-- v7+: sessionId NOT sent, IS in signature
```

### Upgrade Path

```
v6 (current) ────────► v17 (when stable)
             direct upgrade, skip v7-v16
```

---

## NVS Persistence

### Overview (v0.1.8)

Keys and queue IDs survive reboots using ESP32's Non-Volatile Storage.

### Why NVS?

| Storage | Pros | Cons |
|---------|------|------|
| **NVS** | Simple API, wear-leveling | ~16KB limit |
| SPIFFS | Larger files | Overkill for keys |
| SD Card | Large storage | Hardware needed |

### Persisted Data

| Key | Size | Description |
|-----|------|-------------|
| rcv_auth_sk | 64 bytes | Ed25519 Secret Key |
| rcv_auth_pk | 32 bytes | Ed25519 Public Key |
| rcv_dh_sk | 32 bytes | X25519 Secret Key |
| rcv_dh_pk | 32 bytes | X25519 Public Key |
| rcv_id | 24 bytes | Recipient ID |
| rcv_id_len | 1 byte | Length |
| snd_id | 24 bytes | Sender ID |
| snd_id_len | 1 byte | Length |
| srv_dh_pk | 32 bytes | Server DH Key |
| have_srv_dh | 1 byte | Flag |

### API Functions

```c
bool have_saved_keys()      // Check if keys exist
bool load_keys_from_nvs()   // Load all keys
void save_keys_to_nvs()     // Save after IDS
void clear_saved_keys()     // Delete all (reset)
```

### Flow

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

When DEL command succeeds:
1. Server deletes queue + messages
2. Server responds with OK
3. Client calls `clear_saved_keys()` to wipe local NVS

---

## Debugging Journey

### Error Progression

```
Timeline:
─────────────────────────────────────────────────────────────────────────>

[TLS FAIL] [ERR BLOCK] [ERR CMD] [ERR AUTH] [DECRYPT] [ACK OK] [NVS] [DEL]
    │          │           │         │          │         │       │     │
    ▼          ▼           ▼         ▼          ▼         ▼       ▼     ▼
  TLS 1.3   Block      SubMode   libsodium   E2E      Full    Keys  Full
  + ALPN    format     + flags   signatures  works    cycle   save  SMP!
```

### Error Analysis

| Error | Symptom | Root Cause | Fix |
|-------|---------|------------|-----|
| TLS fail | -0x7780 | TLS version | Force TLS 1.3 |
| No ServerHello | Timeout | Wrong ALPN | Set "smp/1" |
| ERR BLOCK | After ClientHello | Wrong block format | Command block format |
| ERR CMD SYNTAX | After NEW | Missing subMode | Add 'S' |
| ERR CMD SYNTAX | SEND | Binary msgFlags | ASCII 'T'/'F' |
| ERR AUTH | NEW | Wrong signature | libsodium |
| ERR AUTH | ACK | Wrong entityId | Use recipientId |

---

## Lessons Learned

### 1. Test Assumptions Early

**Assumption**: "Ed25519 is Ed25519"  
**Reality**: Implementation differences exist  
**Lesson**: Verify crypto library compatibility first

### 2. Incremental Debugging

```
1. TLS working (isolated)
2. Handshake working (isolated)
3. NEW working (full auth)
4. SUB working
5. SEND working
6. MSG decrypt working
7. ACK working
8. NVS persistence working
9. DEL working ← Full SMP Client!
```

### 3. The Source is Truth

When in doubt, read the Haskell source. Complex but correct.

### 4. Error Messages are Clues

```
ERR BLOCK   → Block format
ERR CMD     → Command format
ERR AUTH    → Signature or entityId
ERR NO_MSG  → Already ACK'd
ERR NO_QUEUE → Queue deleted or doesn't exist
```

### 5. Persist Early, Clear on Delete

- Save keys immediately after IDS response
- Clear keys immediately after successful DEL
- Power can fail anytime

---

## Quick Reference

### Block Sizes

| Constant | Value |
|----------|-------|
| SMP_BLOCK_SIZE | 16384 |
| MAX_CONTENT | 16382 |
| SESSION_ID_LEN | 32 |
| ED25519_SIG_LEN | 64 |
| SPKI_KEY_SIZE | 44 |

### EntityId per Command

| Command | EntityId |
|---------|----------|
| NEW | empty |
| SUB | recipientId |
| SEND | senderId |
| ACK | recipientId |
| DEL | recipientId |

### Performance (ESP32-S3)

| Operation | Time |
|-----------|------|
| Ed25519 keygen | ~8ms |
| Ed25519 sign | ~8ms |
| X25519 keygen | ~8ms |
| crypto_box decrypt | ~1ms |
| TLS handshake | ~800ms |
| NVS read/write | ~5ms |

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

*Last updated: January 20, 2026 — v0.1.9-alpha*
