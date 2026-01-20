# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (January 20, 2026)

### Version: v0.1.10-alpha

### 🏆 Multi-Contact + E2E Encryption Working!

Multiple contacts over ONE TLS connection with full E2E decryption!

**Latest Output:**
```
I (5765) SMP: ========================================
I (5765) SMP:   SimpleGo v0.1.10 - Multi-Contact E2E
I (5765) SMP:   Part of Sentinel Secure Messenger Suite
I (5765) SMP: ========================================
I (6088) SMP: [1/7] Establishing TCP + TLS connection...
I (6288) SMP:       TLS OK! ALPN: smp/1
I (6488) SMP: [2/7] Waiting for ServerHello...
I (6598) SMP: [3/7] Sending ClientHello...
I (6708) SMP: [4/7] Loading contacts from NVS...
I (6708) SMP:       Loaded 2 contacts
I (6718) SMP: [5/7] Subscribing all contacts...
I (6958) SMP:       📡 Subscriptions complete: 2/2
I (6968) SMP: 🧪 SELF-TEST: Sending to [0] Test...
I (7108) SMP:       📤 SEND command sent!
I (7348) SMP:       💬 MESSAGE for [Test]!
I (7358) SMP:       🔓 DECRYPTED: Hello from ESP32!
I (7368) SMP:       ✅ ACK OK
```

---

## Working Features

- ✅ Multi-Contact Database (10 slots)
- ✅ NVS Persistence (contacts_db as blob)
- ✅ Batch SUB (all contacts, one connection)
- ✅ Message Routing (by recipientId)
- ✅ E2E Decryption (crypto_box)
- ✅ Self-Test (SEND → MSG → DECRYPT → ACK)
- ✅ All SMP Commands (NEW, SUB, SEND, MSG, ACK, DEL)
- ✅ TLS 1.3 + ALPN "smp/1"
- ✅ Ed25519 + X25519 + XSalsa20-Poly1305

---

## Data Structures (v0.1.10)

```c
#define MAX_CONTACTS 10

typedef struct {
    char name[32];
    uint8_t rcv_auth_secret[64];  // Ed25519 secret
    uint8_t rcv_auth_public[32];  // Ed25519 public
    uint8_t rcv_dh_secret[32];    // X25519 secret
    uint8_t rcv_dh_public[32];    // X25519 public
    uint8_t recipient_id[24];
    uint8_t recipient_id_len;
    uint8_t sender_id[24];
    uint8_t sender_id_len;
    uint8_t srv_dh_public[32];
    uint8_t have_srv_dh;
    uint8_t active;
} contact_t;

typedef struct {
    uint8_t num_contacts;
    contact_t contacts[MAX_CONTACTS];
} contacts_db_t;
```

---

## Key Functions (v0.1.10)

| Function | Description |
|----------|-------------|
| `add_contact(name)` | NEW → IDS → save to NVS |
| `remove_contact(idx)` | DEL → remove from NVS |
| `list_contacts()` | Show all active contacts |
| `subscribe_all_contacts()` | SUB for each contact |
| `find_contact_by_recipient_id()` | MSG routing |
| `decrypt_message(contact, cipher, plain)` | E2E decryption |
| `self_test_send()` | Full round-trip test |

---

## Build Environment

**Windows (ESP-IDF 5.5 PowerShell):**
```powershell
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5
```

**WSL (Haskell source analysis):**
```bash
cd ~/simplexmq
grep -r "pattern" src --include="*.hs"
```

---

## Key Discoveries (v0.1.10)

### E2E Decryption Fix

```c
// WRONG: Raw X25519 output is NOT a valid crypto_box key!
crypto_scalarmult(shared, secret, public);
crypto_secretbox_open_easy(plain, cipher, len, nonce, shared);

// CORRECT: crypto_box_beforenm does HSalsa20 key derivation
crypto_box_beforenm(shared, public, secret);
crypto_box_open_easy_afternm(plain, cipher, len, nonce, shared);
```

**Why?** `crypto_box` uses HSalsa20 to derive the encryption key from the X25519 shared secret. Raw `crypto_scalarmult` skips this step!

### SEND Command Format

```
SEND ' ' flags ' ' msgBody
     ↑    ↑     ↑
    0x20 ASCII 0x20

flags = 'T' (notification) or 'F' (no notification)
NOT binary 0x00/0x01!
```

**Haskell Source:**
```haskell
-- Protocol.hs line 1697
SEND flags msg -> e (SEND_, ' ', flags, ' ', Tail msg)
```

### Server-Side Encryption

The **server** encrypts messages for the recipient — the sending client does NOT encrypt:

```haskell
-- Server.hs line 2024
C.cbEncryptMaxLenBS (rcvDhSecret qr) (C.cbNonce msgId')
```

So when we SEND plaintext, the server encrypts it using the recipient's DH key.

---

## Haskell Source References

| File | Line | Discovery |
|------|------|-----------|
| Protocol.hs | 1697 | SEND format: `SEND_, ' ', flags, ' ', Tail msg` |
| Protocol.hs | 563 | MsgFlags: `notification :: Bool` |
| Server.hs | 2024 | Server encrypts with `rcvDhSecret` |
| Crypto.hs | 1372 | `cbNonce` pads msgId to 24 bytes |

---

## Session Updates

### v0.1.10-alpha - Multi-Contact + E2E (Today!)

**Major Changes:**
- Multi-contact database with NVS persistence
- Batch subscribe for all contacts
- E2E decryption working (crypto_box fix!)
- Self-test proves full round-trip

**Critical Fix:**
```c
// HSalsa20 key derivation is essential!
crypto_box_beforenm(shared, srv_dh_public, rcv_dh_secret);
```

### v0.1.9-alpha - DEL Command

- Queue deletion from server
- NVS auto-clear after DEL

### v0.1.8-alpha - NVS Persistence

- Keys survive reboots
- Skip NEW on restart → go to SUB

### v0.1.7-alpha - ACK Command

- EntityId = recipientId (not senderId!)
- Full message lifecycle complete

### v0.1.6-alpha - E2E (Single Queue)

- First working E2E decryption
- X25519 DH + XSalsa20-Poly1305

---

## Full E2E Flow (Proven!)

```
┌──────────┐                              ┌──────────┐
│  Client  │                              │  Server  │
└────┬─────┘                              └────┬─────┘
     │                                         │
     │  NEW (rcvAuthKey, rcvDhKey)            │
     │────────────────────────────────────────>│
     │                                         │
     │  IDS (recipientId, senderId, srvDhKey) │
     │<────────────────────────────────────────│
     │                                         │
     │  SUB (recipientId)                      │
     │────────────────────────────────────────>│
     │                                         │
     │  OK                                     │
     │<────────────────────────────────────────│
     │                                         │
     │  SEND ' ' 'F' ' ' "Hello!"             │
     │────────────────────────────────────────>│
     │                                         │
     │  (Server encrypts with rcvDhSecret)    │
     │                                         │
     │  MSG [msgId][ts][flags][encrypted]     │
     │<────────────────────────────────────────│
     │                                         │
     │  Client decrypts with:                 │
     │  crypto_box_beforenm(shared, srvDh, rcvDh)
     │  crypto_box_open_easy_afternm(...)     │
     │                                         │
     │  ACK (msgId)                            │
     │────────────────────────────────────────>│
     │                                         │
     │  OK                                     │
     │<────────────────────────────────────────│
     │                                         │
```

---

## Next Steps

### Immediate

1. **T-Embed UI** — Display + Rotary Encoder
2. **Contact Naming** — Better UX for management
3. **Connection Recovery** — Auto-reconnect

### Short-term

4. **Multiple Servers** — Contact on different SMP servers
5. **Bidirectional** — Two queues per contact

### Medium-term

6. **Double Ratchet** — Full Agent-level E2E
7. **Group Messaging**

---

## Known Issues

### Resolved (v0.1.10)

| Issue | Solution |
|-------|----------|
| E2E decryption fails | Use `crypto_box_beforenm()` not `crypto_scalarmult()` |
| SEND syntax error | ASCII 'T'/'F', two spaces |
| Wrong encryption key | HSalsa20 key derivation required |

### Open

| Issue | Status | Notes |
|-------|--------|-------|
| Multi-server | TODO | All contacts on same server |
| Bidirectional chat | TODO | Need two queues per contact |
| T-Embed UI | TODO | Display integration |

---

## 🏆 Achievement Unlocked

**"First Native ESP32 Multi-Contact SimpleX Client with E2E Encryption"**

- ✅ Multiple Queues (10 contacts, one connection)
- ✅ Contact Management (Add/Remove/List)
- ✅ Full Message Lifecycle (NEW→SUB→SEND→MSG→DECRYPT→ACK)
- ✅ XSalsa20-Poly1305 E2E Encryption
- ✅ Ed25519 Signing + X25519 Key Exchange
- ✅ NVS Persistent Storage

---

*Last updated: January 20, 2026 — v0.1.10-alpha*
