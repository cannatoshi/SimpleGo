# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (20. Januar 2026)

### Version: v0.1.9-alpha

### 🏆 Full Single-Queue SMP Client Complete!

All base SMP commands implemented:

| Command | Function | Status |
|---------|----------|--------|
| NEW | Create queue | ✅ |
| SUB | Subscribe | ✅ |
| SEND | Send message | ✅ |
| MSG | Receive + decrypt | ✅ |
| ACK | Acknowledge | ✅ |
| DEL | Delete queue | ✅ |

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
- ✅ ACK command with OK response
- ✅ NVS Key Persistence
- ✅ Queue Reconnect after Reboot
- ✅ **DEL command** ← v0.1.9
- ✅ **NVS Auto-Clear after DEL** ← v0.1.9

---

## Today's Addition: DEL Command (v0.1.9)

### New Function

```c
static void delete_queue(mbedtls_ssl_context *ssl, uint8_t *block,
                         const uint8_t *session_id,
                         const uint8_t *recipient_id, uint8_t recipient_id_len,
                         const uint8_t *rcv_auth_secret);
```

### DEL Command Format

```
[sigLen=64][signature]
[sessLen=32][sessionId]
[corrIdLen][corrId]
[entityIdLen][recipientId]    ← Recipient Command (like SUB, ACK)
"DEL"                         ← No parameters!
```

### From Haskell Source

```haskell
DEL :: Command Recipient    -- Recipient Command
DEL -> e DEL_               -- Format: just "DEL", no params
```

### What Happens

1. DEL command sent to server
2. Server deletes queue + all messages
3. Server responds with `OK`
4. Local NVS keys automatically cleared

### Proof - Log Output

```
I (187810) SMP:   🗑️ Deleting queue...
I (187930) SMP:   DEL sent!
I (188170) SMP:   ✅ Queue deleted from server!
I (188190) SMP:       NVS: Keys cleared!
I (188190) SMP:   ✅ NVS cleared!
```

---

## Previous Additions

### v0.1.8 - NVS Persistence

```c
bool have_saved_keys()      // Check if keys exist
bool load_keys_from_nvs()   // Load all keys
void save_keys_to_nvs()     // Save after IDS
void clear_saved_keys()     // Delete all (reset)
```

### v0.1.7 - ACK Command

```c
// ACK is a Recipient command (like SUB)
// EntityId = recipientId (NOT senderId!)
ack_body = [corrId] + [recipientId] + "ACK " + [msgIdLen][msgId]
signature = sign([0x20][sessionId] + ack_body)
```

### v0.1.6 - E2E Decryption

```c
// DH Shared Secret
crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);

// Nonce = msgId (24 bytes, zero-padded)
uint8_t nonce[24] = {0};
memcpy(nonce, msg_id, msgIdLen);

// Decrypt
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

---

## ESP-IDF Monitor Commands

| Key | Action |
|-----|--------|
| `Ctrl+]` | Exit monitor |
| `Ctrl+T, R` | Reboot device |
| `Ctrl+T, H` | Help menu |
| `Ctrl+T, P` | Pause output |

Sehr nützlich für Reboot-Tests! 🔄

---

## Next Steps

### Immediate

1. **Multiple Queues** — Handle multiple contacts
2. **T-Embed UI** — Display + Rotary Encoder
3. **Contact Management** — Save/load contacts

### Short-term

4. WiFi Config in NVS
5. Connection Recovery
6. T-Deck Keyboard Support

### Medium-term

7. Double Ratchet (Curve448)
8. Group Messaging

---

## Known Issues

### Resolved

| Issue | Solution | Date |
|-------|----------|------|
| DEL command | Recipient Command, no params | 20.01.2026 |
| Keys lost on reboot | NVS persistence | 20.01.2026 |
| ACK command | EntityId = recipientId | 20.01.2026 |
| MSG decryption | X25519 DH + XSalsa20-Poly1305 | 20.01.2026 |
| Nonce format | msgId zero-padded to 24 bytes | 20.01.2026 |
| MsgFlags binary | Use ASCII 'T'/'F' | 20.01.2026 |
| ERR AUTH | Switch to libsodium | 19.01.2026 |
| Wrong keyHash | Use CA cert (2nd in chain) | 18.01.2026 |

### Open

| Issue | Status | Notes |
|-------|--------|-------|
| Multiple queues | TODO | Contact management |
| Connection recovery | TODO | Auto-reconnect |
| T-Embed UI | TODO | Display integration |

---

## 🏆 Achievement Unlocked

**"First Complete Native ESP32 SimpleX SMP Client"**

- ✅ Queue Management (NEW, SUB, DEL)
- ✅ Message Lifecycle (SEND, MSG, ACK)
- ✅ SMP Protocol v6
- ✅ Ed25519 Signing
- ✅ X25519 Key Exchange
- ✅ NaCl crypto_box Encryption
- ✅ NVS Key Persistence
- ✅ **Full Single-Queue SMP Client!**

---

## Session Log

### 20. Januar 2026 (Heute)

**v0.1.9-alpha - DEL COMMAND + FULL SMP CLIENT! 🗑️**
- DEL command implementation
- Server-side queue deletion
- Auto NVS clear after DEL
- All base SMP commands complete!

**v0.1.8-alpha - NVS KEY PERSISTENCE! 🔑**
- NVS storage for all keys and queue IDs
- Queue reconnect after reboot (skip NEW, go to SUB)
- Key management functions: have_saved_keys(), load/save/clear
- ESP-IDF Monitor reboot shortcut: Ctrl+T, R

**v0.1.7-alpha - ACK COMMAND COMPLETE! 🎯**
- ACK command implementation
- EntityId = recipientId (NOT senderId!)
- OK response handling
- Full message lifecycle

**v0.1.6-alpha - E2E ENCRYPTION WORKING! 🏆**
- X25519 DH shared secret computation
- XSalsa20-Poly1305 decryption
- Full round-trip: SEND → MSG → Decrypt

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
