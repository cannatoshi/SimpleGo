# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (20. Januar 2026)

### Version: v0.1.8-alpha

### 🔑 NVS Key Persistence Complete!

Keys and Queue-IDs now survive reboots!

```
First Start:
I (6769) SMP:   🎉🎉🎉 QUEUE CREATED! 🎉🎉🎉
I (6809) SMP:       NVS: Keys saved!

After Reboot:
I (6289) SMP:       NVS: Keys loaded!
I (6289) SMP:   [4-6] Skipping NEW - using saved queue!
I (6659) SMP:   ✅ SUBSCRIBED! Ready to receive messages.
```

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
- ✅ **NVS Key Persistence** ← v0.1.8
- ✅ **Queue Reconnect after Reboot** ← v0.1.8

---

## Today's Addition: NVS Persistence (v0.1.8)

### New Functions

```c
bool have_saved_keys()      // Check if keys exist in NVS
bool load_keys_from_nvs()   // Load all keys from NVS
void save_keys_to_nvs()     // Save after IDS response
void clear_saved_keys()     // Delete all keys (reset)
```

### Persisted Data (NVS Namespace: "simplego")

| Key | Size | Description |
|-----|------|-------------|
| rcv_auth_sk | 64 bytes | Ed25519 Secret Key |
| rcv_auth_pk | 32 bytes | Ed25519 Public Key |
| rcv_dh_sk | 32 bytes | X25519 Secret Key |
| rcv_dh_pk | 32 bytes | X25519 Public Key |
| rcv_id | 24 bytes | Recipient ID |
| rcv_id_len | 1 byte | Recipient ID length |
| snd_id | 24 bytes | Sender ID |
| snd_id_len | 1 byte | Sender ID length |
| srv_dh_pk | 32 bytes | Server DH Key |
| have_srv_dh | 1 byte | Flag if server DH exists |

### New Flow

```
Start
  │
  ▼
TLS + ServerHello + ClientHello
  │
  ▼
load_keys_from_nvs()
  │
  ├── Keys found? ──► [4-6] Skip NEW ──► SUB directly
  │
  └── No keys? ──► NEW ──► IDS ──► save_keys_to_nvs() ──► SUB
```

### Implementation Notes

```c
// NVS Namespace
#define NVS_NAMESPACE "simplego"

// Check for saved keys
bool have_saved_keys() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    
    size_t len = 0;
    esp_err_t err = nvs_get_blob(handle, "rcv_auth_sk", NULL, &len);
    nvs_close(handle);
    
    return (err == ESP_OK && len == crypto_sign_SECRETKEYBYTES);
}

// Load all keys
bool load_keys_from_nvs() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    
    size_t len;
    
    // Load Ed25519 keys
    len = crypto_sign_SECRETKEYBYTES;
    nvs_get_blob(handle, "rcv_auth_sk", rcv_auth_secret, &len);
    len = crypto_sign_PUBLICKEYBYTES;
    nvs_get_blob(handle, "rcv_auth_pk", rcv_auth_public, &len);
    
    // Load X25519 keys
    len = 32;
    nvs_get_blob(handle, "rcv_dh_sk", rcv_dh_secret, &len);
    nvs_get_blob(handle, "rcv_dh_pk", rcv_dh_public, &len);
    
    // Load queue IDs
    len = 24;
    nvs_get_blob(handle, "rcv_id", recipient_id, &len);
    nvs_get_u8(handle, "rcv_id_len", &recipient_id_len);
    
    // ... etc
    
    nvs_close(handle);
    return true;
}
```

---

## Bonus: ESP-IDF Monitor Commands

| Key | Action |
|-----|--------|
| `Ctrl+]` | Exit monitor |
| `Ctrl+T, R` | Reboot device |
| `Ctrl+T, H` | Help menu |
| `Ctrl+T, P` | Pause output |

Sehr nützlich für Reboot-Tests! 🔄

---

## Previous Additions

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

## Next Steps

### Immediate

1. ~~NVS Key Persistence~~ ✅ DONE (v0.1.8)
2. **T-Embed UI** — Display + Rotary Encoder
3. **DEL Command** — Delete queue on reset

### Short-term

4. **Multiple Queues** — Handle multiple contacts
5. **Error Recovery** — Reconnect on connection loss
6. **Config Storage** — WiFi credentials in NVS

### Medium-term

7. **T-Deck UI** — LCD + Physical Keyboard
8. **Double Ratchet** — Curve448 for Agent-level E2E

---

## Known Issues

### Resolved

| Issue | Solution | Date |
|-------|----------|------|
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
| DEL command | TODO | For queue cleanup |
| Multiple queues | TODO | Contact management |
| Connection recovery | TODO | Auto-reconnect |

---

## 🏆 Achievement Unlocked

**"First Native ESP32 SimpleX E2E Client — Persistent!"**

- ✅ Queue Management (NEW, SUB)
- ✅ SMP Protocol v6
- ✅ Ed25519 Signing
- ✅ X25519 Key Exchange
- ✅ NaCl crypto_box Encryption
- ✅ Full Message Round-Trip
- ✅ ACK Command
- ✅ **NVS Key Persistence — Survives Reboots!**

---

## Session Log

### 20. Januar 2026 (Heute)

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
