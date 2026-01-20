# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (20. Januar 2026)

### Version: v0.1.5-alpha

**Working Features:**
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
- ✅ Message receive loop

**Latest Output:**
```
I (xxxx) SMP: ========================================
I (xxxx) SMP:   SimpleGo v0.1.5-alpha
I (xxxx) SMP:   Native SMP Client for ESP32
I (xxxx) SMP:   Part of Sentinel Secure Messenger Suite
I (xxxx) SMP: ========================================
I (xxxx) SMP: [1/8] TCP + TLS...
I (xxxx) SMP:       TLS OK! ALPN: smp/1
I (xxxx) SMP: [5/8] Sending NEW command...
I (xxxx) SMP:   🎉🎉🎉 QUEUE CREATED! 🎉🎉🎉
I (xxxx) SMP: [7/8] Sending SUB command...
I (xxxx) SMP:   ✅ SUBSCRIBED! Ready to receive messages.
I (xxxx) SMP: [8/8] Testing SEND command...
I (xxxx) SMP:       SEND command sent!
I (xxxx) SMP:   📨 Waiting for messages...
I (xxxx) SMP:   💬 MESSAGE received!
I (xxxx) SMP:   ✅ OK - SEND confirmed
```

---

## Today's Discoveries (20. Januar 2026)

### SUB Response Transport Format

The SUB response comes in transport format, not simple format:

```
[01]           = txCount
[00 3f]        = txLen (63 bytes)
[00]           = authLen (no signature from server)
[20][32 bytes] = sessionId
[01][corrId]   = corrId
[18][24 bytes] = entityId (recipientId echo)
[O][K]         = "OK" ← at position 64!
```

**Key insight:** Must parse through transport headers to find actual command!

### SEND Command - Critical Findings

**1. MsgFlags is ASCII, NOT binary!**

From Haskell `Encoding.hs`:
```haskell
instance Encoding Bool where
  smpEncode = \case
    True -> "T"
    False -> "F"
```

- ❌ WRONG: `0x00` or `0x01` (binary)
- ✅ CORRECT: `'F'` (0x46) or `'T'` (0x54)

**2. Space required after msgFlags**

From `Protocol.hs:1697`:
```haskell
e (SEND_, ' ', flags, ' ', Tail msg)
```

Format: `"SEND " + flag + " " + msgBody`

**3. Unsecured queue = No signature needed**

From `Server.hs:1241`:
```haskell
vc SSender SEND {} = verifyQueue $ \q -> 
  if maybe (isNothing tAuth) verify (senderKey $ snd q) 
  then VRVerified q_ else VRFailed AUTH
```

Queue without SKEY command accepts SEND with `authLen = 0`.

### SEND Body Construction

```c
// CorrId
send_body[sp++] = 1;      // corrIdLen
send_body[sp++] = '3';    // corrId = "3"

// EntityId = senderId (NOT recipientId!)
send_body[sp++] = sender_id_len;  // 24
memcpy(&send_body[sp], sender_id, sender_id_len);
sp += sender_id_len;

// Command
send_body[sp++] = 'S';
send_body[sp++] = 'E';
send_body[sp++] = 'N';
send_body[sp++] = 'D';
send_body[sp++] = ' ';

// MsgFlags + Space (CRITICAL!)
send_body[sp++] = 'F';    // notification = False (ASCII!)
send_body[sp++] = ' ';    // Space before msgBody!

// MsgBody
memcpy(&send_body[sp], msg, msg_len);
```

### Self-Echo Behavior

When you're both sender AND subscriber on same queue:
1. **MSG** (large, ~16KB) — Your message echoed back as `EncRcvMsgBody`
2. **OK** (small, ~66 bytes) — Confirmation that SEND succeeded

The MSG body is encrypted with:
- X25519 DH shared secret (rcvDhSecret ↔ srvDhPubKey)
- XSalsa20-Poly1305 (NaCl `crypto_box`)

---

## Next Steps

### Immediate (This Week)

1. **MSG Decryption**
   - Compute DH shared secret: `crypto_scalarmult(shared, rcvDhSecret, srvDhPubKey)`
   - Decrypt with `crypto_box_open_easy` or `crypto_secretbox_open_easy`

2. **ACK Command**
   - Send ACK with msgId after receiving MSG
   - Removes message from server queue

### Short-term (This Month)

3. **Key Persistence**
   - Store rcvAuthKey, rcvDhKey in NVS
   - Restore queue subscriptions after reboot

4. **Multiple Queues**
   - Manage multiple conversations
   - Queue state tracking

### Medium-term (Next Month)

5. **Double Ratchet**
   - X3DH key agreement
   - Curve448 support (may need wolfSSL)
   - Proper E2E encryption

---

## Build Environment

**Windows (ESP-IDF 5.5 PowerShell):**
```powershell
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5
```

**WSL (for Haskell source analysis):**
```bash
cd ~/simplexmq
grep -r "pattern" src --include="*.hs"
```

---

## Key Haskell Source References

| File | Line | Discovery |
|------|------|-----------|
| `Protocol.hs` | 1697 | SEND format: `e (SEND_, ' ', flags, ' ', Tail msg)` |
| `Encoding.hs` | - | Bool encoding: "T"/"F", `_smpP = space *> smpP` |
| `Server.hs` | 1241 | Unsecured queue accepts SEND without auth |
| `Crypto.hs` | 1267-1274 | NaCl crypto_box with X25519 DH shared secret |

### Useful grep Commands

```bash
# Find SEND format
grep -r "SEND_\|pattern SEND" --include="*.hs"

# Find Bool encoding
grep -r "instance Encoding Bool" --include="*.hs"

# Find crypto_box usage
grep -r "crypto_box\|secretbox" --include="*.hs"

# Find MSG format
grep -r "pattern MSG\|MSG_" --include="*.hs"
```

---

## Known Issues

### Resolved

| Issue | Solution | Date |
|-------|----------|------|
| MsgFlags binary | Use ASCII 'T'/'F' | 20.01.2026 |
| Missing space | Add ' ' after msgFlags | 20.01.2026 |
| SUB response parsing | Parse transport format | 20.01.2026 |
| ERR AUTH on SEND | Use authLen=0 for unsecured | 20.01.2026 |
| ERR AUTH | Switch to libsodium | 19.01.2026 |
| Wrong keyHash | Use CA cert (2nd in chain) | 18.01.2026 |

### Open

| Issue | Status | Notes |
|-------|--------|-------|
| MSG decryption | TODO | Need DH + XSalsa20 |
| ACK command | TODO | After decryption works |
| Key persistence | TODO | NVS storage |
| Certificate verification | TODO | Currently VERIFY_NONE |

---

## Test Servers

| Server | Host | Port | Status |
|--------|------|------|--------|
| SimpleX Flux 3 | smp3.simplexonflux.com | 5223 | ✅ Working |
| SimpleX Flux 4 | smp4.simplexonflux.com | 5223 | Untested |
| SimpleX Official | smp.simplex.im | 5223 | Untested |

---

## Session Log

### 20. Januar 2026 (Heute)

- SUB response transport format parsing
- SEND command implementation
- MsgFlags ASCII encoding fix ('F'/'T')
- Space after msgFlags required
- Unsecured queue auth (authLen=0)
- MSG receive working
- Message loop implemented
- Version bump to v0.1.5-alpha

### 19. Januar 2026

- Major breakthrough: NEW command working!
- Fixed Ed25519 signature issue (Monocypher → libsodium)
- Implemented SUB command
- v0.1.3-alpha, v0.1.4-alpha

### 18. Januar 2026

- Handshake working (v0.1.2-alpha)
- Discovered keyHash must use CA certificate
- Implemented certificate chain parsing

### 17. Januar 2026

- TLS 1.3 connection established (v0.1.1-alpha)
- ALPN negotiation working
- Block format implemented

### 16. Januar 2026

- Initial project structure (v0.1.0-alpha)
- WiFi and TCP connection

---

*Last updated: 20. Januar 2026*
