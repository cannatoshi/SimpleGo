# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (20. Januar 2026)

### Version: v0.1.7-alpha

### ✅ Full Message Lifecycle Complete!

```
NEW → IDS (queue created)
SUB → OK (subscribed)
SEND → MSG (echo back) + OK
MSG → Decrypt → ACK → OK (message confirmed & deleted)
```

**Complete SMP message handling operational!** 🎉

---

## Working Features

- ✅ TLS 1.3 connection with ChaCha20-Poly1305
- ✅ ALPN negotiation "smp/1"
- ✅ SMP handshake (ServerHello/ClientHello)
- ✅ Certificate chain parsing
- ✅ keyHash computation from CA certificate
- ✅ Ed25519 key generation (libsodium)
- ✅ X25519 key generation
- ✅ SPKI key encoding
- ✅ NEW command with IDS response
- ✅ SUB command with OK response
- ✅ SEND command with message transmission
- ✅ MSG receive with parsing
- ✅ X25519 DH Shared Secret
- ✅ XSalsa20-Poly1305 Decryption
- ✅ **ACK command with OK response** ← v0.1.7
- ✅ **Full message lifecycle!** ← v0.1.7

---

## Today's Addition: ACK Command (v0.1.7)

### ACK Format (from Protocol.hs)

```haskell
ACK :: MsgId -> Command Recipient
ACK msgId -> e (ACK_, ' ', msgId)
```

### Implementation

```c
// ACK is a Recipient command (like SUB)
// EntityId = recipientId (NOT senderId!)
// Requires signature with rcv_auth_secret

uint8_t ack_body[64];
int ap = 0;

// CorrId
ack_body[ap++] = 1;
ack_body[ap++] = '4';

// EntityId = recipientId (NICHT senderId!)
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

// Signature covers: [0x20][sessionId] + ack_body
```

### Server Response

- `OK` = Message acknowledged and deleted from queue
- `ERR NO_MSG` = Message not found (already ACK'd or expired)

---

## SMP Version Analysis

### Why v6?

v6 has **everything** needed for a complete messenger:
- ✅ Queue management (NEW, SUB, DEL)
- ✅ Message sending (SEND)
- ✅ Message receiving (MSG)
- ✅ Acknowledgment (ACK)
- ✅ E2E encryption (X25519 + XSalsa20-Poly1305)

### What v7+ adds (not critical):

- `implySessId` — sessionId not sent (optimization)
- `authEncryptCmds` — Command encryption (extra security)
- Batch commands — Performance optimization

### Upgrade Path

```
v6 (now) ────────────────► v17 (future)
          skip v7-v16
```

From Haskell source:
```haskell
authCmdsSMPVersion = VersionSMP 7
implySessId = v >= authCmdsSMPVersion
-- v6: sessionId sent in transmission, NOT in signature
-- v7+: sessionId NOT sent, IS in signature
```

---

## Previous Discoveries (v0.1.6)

### MSG Encryption Schema

From Haskell `Server.hs:2024`:
```haskell
encrypt body = RcvMessage msgId' . EncRcvMsgBody $ 
  C.cbEncryptMaxLenBS (rcvDhSecret qr) (C.cbNonce msgId') body
```

### Decryption Steps

**1. Compute DH Shared Secret:**
```c
uint8_t shared[crypto_box_BEFORENMBYTES];  // 32 bytes
crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);
```

**2. Nonce = msgId (24 bytes, zero-padded):**
```c
uint8_t nonce[24];
memset(nonce, 0, 24);  // Pad with zeros
memcpy(nonce, msg_id, msgIdLen < 24 ? msgIdLen : 24);
```

**3. Decrypt with NaCl crypto_box:**
```c
int result = crypto_box_open_easy_afternm(
    plaintext, ciphertext, ciphertext_len, nonce, shared);
```

---

## Next Steps

### Immediate

1. ~~ACK Command~~ ✅ DONE (v0.1.7)
2. **Key Persistence** — NVS Storage for rcvAuthKey, rcvDhKey
3. **Queue Reconnect** — SUB after reboot with stored keys

### Short-term

4. **DEL Command** — Delete queue when done
5. **Multiple Queues** — Handle multiple contacts
6. **Error Recovery** — Reconnect on connection loss

### Medium-term

7. **T-Deck UI** — LCD + Keyboard integration
8. **Double Ratchet** — Curve448 for Agent-level E2E

---

## Known Issues

### Resolved

| Issue | Solution | Date |
|-------|----------|------|
| ACK command | EntityId = recipientId, not senderId | 20.01.2026 |
| MSG decryption | X25519 DH + XSalsa20-Poly1305 | 20.01.2026 |
| Nonce format | msgId zero-padded to 24 bytes | 20.01.2026 |
| srv_dh_public | Extract from IDS, skip SPKI header | 20.01.2026 |
| MsgFlags binary | Use ASCII 'T'/'F' | 20.01.2026 |
| ERR AUTH | Switch to libsodium | 19.01.2026 |
| Wrong keyHash | Use CA cert (2nd in chain) | 18.01.2026 |

### Open

| Issue | Status | Notes |
|-------|--------|-------|
| Key persistence | TODO | NVS storage |
| Queue reconnect | TODO | After reboot |
| Double Ratchet | TODO | Curve448 needed |
| Connection keepalive | TODO | Prevent timeouts |

---

## Haskell Source References

| File | Line | Discovery |
|------|------|-----------|
| `Protocol.hs` | - | `ACK msgId -> e (ACK_, ' ', msgId)` |
| `Server.hs` | 2024 | `C.cbEncryptMaxLenBS (rcvDhSecret qr) (C.cbNonce msgId')` |
| `Crypto.hs` | 1372-1381 | `cbNonce` paddet auf 24 Bytes mit 0x00 |
| `Transport.hs` | - | `authCmdsSMPVersion = VersionSMP 7` |

### Useful grep Commands

```bash
# Find ACK handling
grep -rn "ACK_\|pattern ACK" src/Simplex/Messaging/Protocol.hs

# Find version differences
grep -rn "implySessId\|authCmdsSMPVersion" src/Simplex/Messaging/

# Find encryption
grep -rn "cbEncrypt\|cbDecrypt" src/Simplex/Messaging/Crypto.hs
```

---

## 🏆 Achievement Unlocked

**"First Native ESP32 SimpleX E2E Client — Full Lifecycle!"**

- ✅ Queue Management (NEW, SUB)
- ✅ SMP Protocol v6
- ✅ Ed25519 Signing
- ✅ X25519 Key Exchange
- ✅ NaCl crypto_box Encryption
- ✅ Full Message Round-Trip
- ✅ **ACK Command — Messages Confirmed & Deleted!**

---

## Session Log

### 20. Januar 2026 (Heute)

**v0.1.7-alpha - ACK COMMAND COMPLETE! 🎯**
- ACK command implementation
- EntityId = recipientId (NOT senderId!)
- OK response handling
- Full message lifecycle: NEW→SUB→SEND→MSG→ACK→OK
- SMP version analysis (v6 vs v7-v17)

**v0.1.6-alpha - E2E ENCRYPTION WORKING! 🏆**
- X25519 DH shared secret computation
- XSalsa20-Poly1305 decryption via libsodium
- Server DH key extraction from IDS response
- Nonce = msgId (zero-padded to 24 bytes)
- Full round-trip: SEND → MSG → Decrypt → "Hello from ESP32!"

**v0.1.5-alpha**
- SEND command implementation
- MSG receive with parsing
- Message receive loop

**v0.1.4-alpha**
- SUB command working

### 19. Januar 2026

- NEW command working (v0.1.3-alpha)
- libsodium fix for Ed25519

### 18. Januar 2026

- Handshake complete (v0.1.2-alpha)
- keyHash from CA certificate

### 17. Januar 2026

- TLS 1.3 working (v0.1.1-alpha)

### 16. Januar 2026

- Initial structure (v0.1.0-alpha)

---

*Last updated: 20. Januar 2026*
