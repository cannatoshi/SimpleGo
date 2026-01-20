# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (20. Januar 2026)

### Version: v0.1.6-alpha

### 🏆 MEGA-MILESTONE: E2E Encryption Working!

```
I (7789) SMP:       📬 Got our MSG back!
      MsgId: 354c3cd4a96d8510f1ac5965378e0f18edd2a73c662e1dff
I (7799) SMP:       Encrypted: 16122 bytes
I (7859) SMP:   🔓 DECRYPTED (16106 bytes):
      ......io..F Hello from ESP32!###############
```

**"Hello from ESP32!"** erfolgreich gesendet, empfangen und entschlüsselt! 🎉

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
- ✅ **X25519 DH Shared Secret**
- ✅ **XSalsa20-Poly1305 Decryption**
- ✅ **Full E2E Round-Trip!**

---

## Today's Discoveries (20. Januar 2026)

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

From `Crypto.hs:1372-1381`:
```haskell
cbNonce :: ByteString -> CbNonce
cbNonce s
  | len == 24 = CryptoBoxNonce s
  | len > 24 = CryptoBoxNonce . fst $ B.splitAt 24 s
  | otherwise = CryptoBoxNonce $ s <> B.replicate (24 - len) (toEnum 0)
```

**3. Decrypt with NaCl crypto_box:**
```c
int result = crypto_box_open_easy_afternm(
    plaintext,      // output buffer
    ciphertext,     // input (16-byte MAC + encrypted data)
    ciphertext_len, // length including MAC
    nonce,          // 24 bytes (msgId zero-padded)
    shared          // 32 bytes DH shared secret
);
// result == 0 = success
// result == -1 = failed (tampering or wrong key)
```

### MSG Format

```
MSG [msgIdLen][msgId][encryptedBody...]

encryptedBody = [16 bytes MAC][ciphertext][padding mit '#']
```

### Decrypted Body Structure

```
[Header: ~12 bytes][Timestamp][Flags]['F'][' '][Klartext][Padding mit '#']
```

### New Variables in main.c

```c
// Server's DH public key (from IDS response)
uint8_t srv_dh_public[32];
bool have_srv_dh = false;

// Save when parsing IDS response (skip 12-byte SPKI header):
memcpy(srv_dh_public, &resp[p + 12], 32);
have_srv_dh = true;
```

### libsodium API Reference

```c
#include "sodium.h"

// Constants
crypto_box_BEFORENMBYTES  // 32 - shared secret size
crypto_box_MACBYTES       // 16 - MAC size
crypto_box_NONCEBYTES     // 24 - nonce size

// DH Shared Secret (X25519)
crypto_box_beforenm(shared, their_public, my_secret);

// Decrypt (XSalsa20-Poly1305)
crypto_box_open_easy_afternm(plain, cipher, cipher_len, nonce, shared);
```

---

## Complete E2E Flow (Proven!)

```
1. NEW         → Queue erstellen      → IDS Response (get srv_dh_public)
2. SUB         → Subscribe            → OK
3. SEND        → Nachricht senden     → MSG (echo) + OK
4. MSG decrypt → Entschlüsseln        → "Hello from ESP32!" 🎉
```

---

## Next Steps

### Immediate

1. **ACK Command** — Nachricht als gelesen bestätigen
2. **Message Header Parsing** — Timestamp, Flags extrahieren
3. **Multiple Messages** — Queue mit mehreren MSG testen

### Short-term

4. **Key Persistence** — NVS Storage für rcvAuthKey, rcvDhKey
5. **Queue Reconnect** — Nach Reboot wieder subscriben

### Medium-term

6. **Double Ratchet** — Curve448 für Agent-Level E2E
7. **T-Deck UI** — LCD + Keyboard Integration

---

## Haskell Source References

| File | Line | Discovery |
|------|------|-----------|
| `Server.hs` | 2024 | `C.cbEncryptMaxLenBS (rcvDhSecret qr) (C.cbNonce msgId')` |
| `Crypto.hs` | 1372-1381 | `cbNonce` paddet auf 24 Bytes mit 0x00 |
| `Crypto.hs` | 1274 | `cbEncryptNoPad` = `cryptoBox secret nonce` |
| `Protocol.hs` | 1697 | SEND format: `e (SEND_, ' ', flags, ' ', Tail msg)` |
| `Server.hs` | 1241 | Unsecured queue accepts SEND without auth |

### Useful grep Commands

```bash
# Find encryption
grep -rn "cbEncrypt\|cbDecrypt" src/Simplex/Messaging/Crypto.hs

# Find nonce handling
grep -rn "cbNonce" src/Simplex/Messaging/

# Find MSG encryption
grep -rn "EncRcvMsgBody" src/Simplex/Messaging/
```

---

## Known Issues

### Resolved

| Issue | Solution | Date |
|-------|----------|------|
| MSG decryption | X25519 DH + XSalsa20-Poly1305 | 20.01.2026 |
| Nonce format | msgId zero-padded to 24 bytes | 20.01.2026 |
| srv_dh_public | Extract from IDS, skip SPKI header | 20.01.2026 |
| MsgFlags binary | Use ASCII 'T'/'F' | 20.01.2026 |
| ERR AUTH | Switch to libsodium | 19.01.2026 |
| Wrong keyHash | Use CA cert (2nd in chain) | 18.01.2026 |

### Open

| Issue | Status | Notes |
|-------|--------|-------|
| ACK command | TODO | After MSG processed |
| Key persistence | TODO | NVS storage |
| Double Ratchet | TODO | Curve448 needed |

---

## 🏆 Achievement Unlocked

**"First Native ESP32 SimpleX E2E Client"**

- ✅ Queue Management
- ✅ SMP Protocol v6
- ✅ Ed25519 Signing
- ✅ X25519 Key Exchange
- ✅ NaCl crypto_box Encryption
- ✅ Full Message Round-Trip

---

## Session Log

### 20. Januar 2026 (Heute)

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
