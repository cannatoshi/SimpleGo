# SimpleGo Technical Documentation

> Key learnings, discoveries, and implementation decisions from developing the first native SMP client

---

## Table of Contents

1. [Project Background](#project-background)
2. [Critical Discoveries](#critical-discoveries)
3. [Debugging Journey](#debugging-journey)
4. [Architecture Decisions](#architecture-decisions)
5. [Message Layer Analysis](#message-layer-analysis)
6. [Lessons Learned](#lessons-learned)

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

---

### Discovery #6: SEND Format Has Two Spaces

**Problem**: `ERR CMD SYNTAX` on SEND even with ASCII flags.

**Finding**: SEND command format is `SEND ' ' flags ' ' body` — two spaces!

---

### Discovery #7: crypto_box vs raw X25519

**Problem**: E2E decryption failed despite correct keys.

**Finding**: NaCl's `crypto_box` uses HSalsa20 to derive the encryption key. Raw `crypto_scalarmult` output is NOT a valid encryption key!

```c
// ❌ WRONG
crypto_scalarmult(shared, secret, public);
crypto_secretbox_open_easy(plain, cipher, len, nonce, shared);

// ✅ CORRECT
crypto_box_beforenm(shared, public, secret);
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

---

### Discovery #8: Server-Side Encryption

**Finding**: The **server** encrypts messages for the recipient. The sending client does NOT encrypt — it sends plaintext.

---

### Discovery #9: ACK/DEL Use recipientId

**Problem**: `ERR AUTH` on ACK and DEL commands.

**Finding**: ACK and DEL are **Recipient commands** — entityId must be recipientId, not senderId.

---

### Discovery #10: Base64URL for DH Key (v0.1.11)

**Problem**: Invitation links rejected as "Invalid link"

**Finding**: Initial implementation used `+` character which broke URL parsing.

**Solution**: Use Base64URL with `-` and `_` instead of `+` and `/`.

---

### Discovery #11: Double Encoding for `=` (v0.1.11)

**Problem**: Even with Base64URL, links with padding failed.

**Finding**: The `=` padding must be **double URL-encoded**:

```
=  →  %3D  →  %253D
```

---

### Discovery #12: Layer 5 Contact DH Encryption (v0.1.12)

**Problem**: Layer 3 decryption produced garbage instead of readable messages.

**Finding**: Initial messages (AgentInvitation) have a **second encryption layer** using contact's DH key exchange!

```
After Layer 3 decrypt:
┌──────────────────────────────────────────────────────────────────┐
│ Offset 14-57: X25519 SPKI (44 bytes)    │ Sender's DH key       │
│ Offset 58+:   crypto_box encrypted body │ Needs Layer 5 decrypt │
└──────────────────────────────────────────────────────────────────┘
```

**Solution**:
```c
// Extract sender's DH public key from SPKI
uint8_t sender_dh_pub[32];
memcpy(sender_dh_pub, &msg[14 + 12], 32);  // Skip 12-byte header

// Decrypt with OUR DH secret
crypto_box_beforenm(shared, sender_dh_pub, contact_dh_secret);
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

---

### Discovery #13: Agent Protocol Format (v0.1.12)

**Finding**: After Layer 5 decryption, messages follow Agent Protocol format:

```
┌──────────────────────────────────────────────────────────────────┐
│ Offset 0-1:   Version (2-byte BE)       │ e.g., 0x0007 = v7     │
│ Offset 2:     Type                      │ 'C', 'I', 'M', 'R'    │
│ Offset 3+:    Body                      │ Type-specific         │
└──────────────────────────────────────────────────────────────────┘
```

---

### Discovery #14: AgentInvitation Contains Reply Queue (v0.1.12)

**Finding**: Type 'I' messages contain:
- `connReq`: URL-encoded reply queue URI (simplex:/invitation#/...)
- `connInfo`: Peer's profile (JSON)

This is how we learn **where to send AgentConfirmation**!

---

### Discovery #15: SPKI Offset Bug (v0.1.12)

**Problem**: SPKI detection failed intermittently.

**Finding**: Byte-offset check was wrong (`i+5` instead of `i+4` for OID tag 0x06).

---

### Discovery #16: Version String "1," (v0.1.12)

**Finding**: SMP protocol version appears as string "1," at offset 12 in decrypted message, distinct from Agent version which is 2-byte BE integer.

---

## Debugging Journey

### Error State Progression

```
Timeline:
──────────────────────────────────────────────────────────────────────────────────────────────────>

[TLS] [BLOCK] [CMD] [AUTH] [DECRYPT] [SEND] [E2E FIX] [MULTI] [LINKS] [URL FIX] [LAYER5] [AGENT]
  │      │      │      │       │        │       │        │       │        │         │        │
  ▼      ▼      ▼      ▼       ▼        ▼       ▼        ▼       ▼        ▼         ▼        ▼
TLS    Block  SubMode libsodium First  ASCII  HSalsa20 10     Links  Base64URL   Contact  Parse
1.3    format added   works    decrypt flags   key    contacts work   + =enc     DH dec   'I'
```

### Detailed Error Analysis

| Version | Error | Root Cause | Fix |
|---------|-------|------------|-----|
| v0.1.1 | TLS fail | Version mismatch | Force TLS 1.3 |
| v0.1.2 | ERR BLOCK | Wrong block format | Command block format |
| v0.1.3 | ERR CMD SYNTAX | Missing subMode | Add 'S' parameter |
| v0.1.3 | ERR AUTH | Wrong crypto lib | Switch to libsodium |
| v0.1.5 | ERR CMD SYNTAX | Binary flags | Use ASCII 'T'/'F' |
| v0.1.5 | ERR CMD SYNTAX | SEND format | Two spaces |
| v0.1.6 | Decrypt fail | Raw X25519 | crypto_box_beforenm |
| v0.1.7 | ERR AUTH | Wrong entityId | recipientId for ACK/DEL |
| v0.1.11 | Invalid link | `+` in URL | Base64URL encoding |
| v0.1.11 | Invalid link | `=` encoding | Double encode |
| v0.1.12 | Garbage output | Layer 5 | Contact DH decrypt |
| v0.1.12 | Unknown format | Agent Protocol | Parse version + type |

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

---

## Message Layer Analysis

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
│  └── [2-byte version BE] [type: 'C'/'I'/'M'/'R'] [body]        │
└─────────────────────────────────────────────────────────────────┘
```

### X25519 SPKI Header

```
Header (12 bytes): 30 2a 30 05 06 03 2b 65 6e 03 21 00
                   │  │  │  │  │  │  │  │  │  │  │  │
                   │  │  │  │  │  │  │  │  │  │  │  └─ 0x00
                   │  │  │  │  │  │  │  │  │  │  └─── BIT STRING len (33)
                   │  │  │  │  │  │  │  │  │  └────── 0x03 BIT STRING
                   │  │  │  │  │  │  └──┴──┴───────── OID 1.3.101.110
                   │  │  │  └──┴──┴────────────────── OID container
                   │  │  └─────────────────────────── AlgorithmId
                   │  └────────────────────────────── Length (42)
                   └───────────────────────────────── SEQUENCE
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
Invalid link → URL encoding
```

### 4. Multiple Encryption Layers

Don't assume one decryption is enough. Check if the output makes sense.

### 5. URL Encoding is Tricky

Different Base64 variants exist. Double encoding is sometimes required.

### 6. Systematic Debugging

Work through one layer at a time. Verify each layer before moving to the next.

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

### Agent Message Types

| Type | Name | Description |
|------|------|-------------|
| `'C'` | AgentConfirmation | Connection confirmation |
| `'I'` | AgentInvitation | Reply queue + profile |
| `'M'` | AgentMsgEnvelope | Double Ratchet message |
| `'R'` | AgentRatchetKey | Key exchange |

### URL Encoding

| Character | Single Encode | Double Encode |
|-----------|---------------|---------------|
| `=` | `%3D` | `%253D` |
| `:` | `%3A` | `%253A` |
| `/` | `%2F` | `%252F` |
| `@` | `%40` | `%2540` |

---

## 🏆 Achievement: Full Message Layer Decoding

As of v0.1.12-alpha:

- ✅ Layer 1-4: SMP Transport + E2E
- ✅ Layer 5: Contact DH Decryption
- ✅ Layer 6: Agent Protocol Parsing
- ✅ AgentInvitation + Reply Queue Extraction

**"First Native ESP32 SimpleX Client with Full Message Layer Decoding"**

---

*Last updated: January 21, 2026 — v0.1.12-alpha*
