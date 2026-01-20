# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (January 20, 2026)

### Version: v0.1.11-alpha

### 🔗 Invitation Links Working!

SimpleX Desktop/Mobile Apps can now connect directly to ESP32!

**Latest Output:**
```
I (5765) SMP: ========================================
I (5765) SMP:   SimpleGo v0.1.11 - Invitation Links
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

🔗 SIMPLEX CONTACT LINKS ════════════════════════════════════════════
📱 [0] Test ─────────────────────────────────────────────────────────
📋 SMP Queue URI (raw):
   smp://1jne379u7IDJSxAvXbWb_JgoE7iabcslX0LBF22Rej0@smp3.simplexonflux.com:5223/XLEVCxbNocUkdcmSuQJMHQ_efzha0W_R#/?v=1-4&dh=MCowBQYDK2VuAyEA5tJkIGLCSx0fSehiUt5wmL7Pyq8H+VX2Km3ChgGaDRE=&q=c

🌐 SimpleX Contact Link (COPY THIS!):
   https://simplex.chat/contact#/?v=2-7&smp=smp%3A%2F%2F1jne379u7IDJSxAvXbWb_JgoE7iabcslX0LBF22Rej0%40smp3.simplexonflux.com%3A5223%2FXLEVCxbNocUkdcmSuQJMHQ_efzha0W_R%23%2F%3Fv%3D1-4%26dh%3DMCowBQYDK2VuAyEA5tJkIGLCSx0fSehiUt5wmL7Pyq8H%252BVX2Km3ChgGaDRE%253D%26q%3Dc

══════════════════════════════════════════════════════════════════════
📝 ANLEITUNG:
   1. Den 🌐 Web Link kopieren
   2. In SimpleX Desktop/Mobile App öffnen
   3. 'Connect' klicken
   4. Nachricht senden
   5. ESP32 empfängt MSG!
```

---

## Working Features

- ✅ **Invitation Links** ← NEU!
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

## Link Generation (v0.1.11)

### Three Link Formats

```
📋 SMP Queue URI (raw):
smp://keyHash@server:5223/senderId#/?v=1-4&dh=<base64>&q=c

🌐 SimpleX Contact Link:
https://simplex.chat/contact#/?v=2-7&smp=<URL-ENCODED-SMP-URI>

📲 Direct App Link:
simplex:/contact#/?v=2-7&smp=<URL-ENCODED-SMP-URI>
```

### URL-Encoding Rules

```
Single encoded:
  :  →  %3A
  /  →  %2F
  @  →  %40
  #  →  %23
  ?  →  %3F
  &  →  %26
  =  →  %3D

Double encoded (Base64 DH-Key only):
  +  →  %252B
  =  →  %253D
```

### Version Ranges

| Layer | Version | Meaning |
|-------|---------|---------|
| Contact URI (outer) | `v=2-7` | Agent Version Range |
| SMP Queue (inner) | `v=1-4` | SMP Client Version Range |

### New Functions

```c
// Base64 Standard encoding (+ / = characters)
void base64_standard_encode(const uint8_t *input, size_t len, char *output);

// URL encoding with proper escaping
void url_encode(const char *input, char *output, size_t max_len);

// Generate and print all link formats
void print_invitation_links(void);
```

---

## Key Discoveries (v0.1.11)

### Double Encoding for Base64 Special Characters

```c
// Im Base64-encoded DH-Key:
+  →  %2B   →  %252B  (doppelt encoded!)
=  →  %3D   →  %253D  (doppelt encoded!)

// Warum? Der SMP URI wird URL-encoded in den Contact URI eingebettet.
// Base64 special chars müssen zweimal encoded werden.
```

### DH Key Format

```
Base64 Standard (NICHT base64url!)
  - Mit + / = Zeichen
  - SPKI Header (44 bytes total)
  - X25519 Public Key
```

### Queue Mode Parameter

```
q=c  →  Contact Queue (für Contact Links)
q=m  →  Message Queue (für Gruppen, etc.)
```

### Haskell Source References

| File | Line | Discovery |
|------|------|-----------|
| Protocol.hs | 1078-1085 | `crEncode` Contact URI Format |
| Protocol.hs | SMPQueueUri | `v=1-4&dh=<key>&q=c` Format |
| ConnectionRequestTests.hs | - | `simplex:/contact#/?v=2-7&smp=` |

---

## Test Results (v0.1.11)

| Test | Result |
|------|--------|
| Link in Browser öffnen | ✅ SimpleX Landing Page |
| Link in SimpleX App öffnen | ✅ "Connect to Contact" Dialog |
| Connect klicken | ✅ Verbindung hergestellt |
| Nachricht senden | ✅ ESP32 empfängt MSG |
| E2E Decryption | ✅ Nachricht entschlüsselt |
| ACK senden | ✅ OK Response |

---

## Data Structures (v0.1.10+)

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

## Session Updates

### v0.1.11-alpha - Invitation Links (Today!)

**Major Achievement:**
- SimpleX-compatible contact links working!
- SimpleX Desktop/Mobile can connect to ESP32!

**New Functions:**
- `base64_standard_encode()` — Base64 mit + / =
- `url_encode()` — Standard URL encoding
- `print_invitation_links()` — Alle Link-Formate ausgeben

**Key Discovery:**
- Doppeltes Encoding für + und = im Base64 DH-Key

### v0.1.10-alpha - Multi-Contact + E2E

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
     │  ═══════════════════════════════════   │
     │  📋 Generate Invitation Link:          │
     │  smp://keyHash@server/senderId#/...    │
     │  ═══════════════════════════════════   │
     │                                         │
     │  (SimpleX App connects via Link)       │
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
2. **Bidirectional Chat** — Two queues per contact
3. **QR Code Generation** — Display invitation link as QR

### Short-term

4. Multiple Servers — Contact on different SMP servers
5. Connection Recovery — Auto-reconnect
6. T-Deck Keyboard Support

### Medium-term

7. Double Ratchet (Curve448)
8. Group Messaging

---

## Known Issues

### Resolved (v0.1.11)

| Issue | Solution |
|-------|----------|
| Links not working | Double encoding for Base64 + and = |
| Wrong DH key format | Base64 Standard, nicht base64url |
| Missing queue mode | `q=c` Parameter hinzugefügt |

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
| QR Code | TODO | Show link as scannable QR |

---

## 🏆 Achievement Unlocked

**"First Native ESP32 SimpleX Client with Working Invitation Links"**

- ✅ Multiple Queues (10 contacts, one connection)
- ✅ Contact Management (Add/Remove/List)
- ✅ Full Message Lifecycle (NEW→SUB→SEND→MSG→DECRYPT→ACK)
- ✅ XSalsa20-Poly1305 E2E Encryption
- ✅ Ed25519 Signing + X25519 Key Exchange
- ✅ NVS Persistent Storage
- ✅ **SimpleX-Compatible Invitation Links**

---

*Last updated: January 20, 2026 — v0.1.11-alpha*
