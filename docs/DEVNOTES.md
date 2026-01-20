# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (January 21, 2026)

### Version: v0.1.12-alpha

### 🔐 Agent Protocol + Full Message Layer Decoding!

ESP32 now decodes the complete 6-layer message stack — seeing peer's profile and reply queue URI!

**Latest Output:**
```
I (5765) SMP: ========================================
I (5765) SMP:   SimpleGo v0.1.12 - Agent Protocol
I (5765) SMP:   Part of Sentinel Secure Messenger Suite
I (5765) SMP: ========================================

💬 MESSAGE for [Test]!
🔓 Layer 3 Decrypted: 16106 bytes (SMP E2E)
🔓 Layer 5 Decrypted: 847 bytes (Client DH)
📋 Agent Message: Version=7, Type='I' (Invitation)
🔗 Reply Queue: simplex:/invitation#/?v=2-7&smp=smp%3A%2F%2F...@smp10.simplex.im/...
👤 Peer Profile: {"displayName":"Alice",...}
✅ ACK OK
```

---

## Working Features

- ✅ **Agent Protocol Parsing (Layer 6)** ← NEU!
- ✅ **Client DH Decryption (Layer 5)** ← NEU!
- ✅ **Reply Queue URI Extraction** ← NEU!
- ✅ **Peer Profile Visibility** ← NEU!
- ✅ Contact Link URL Encoding Fixed (Base64URL)
- ✅ SMP E2E Decryption (Layer 3)
- ✅ Multi-Contact Database (10 slots)
- ✅ NVS Persistence
- ✅ All SMP Commands (NEW, SUB, SEND, MSG, ACK, DEL)
- ✅ TLS 1.3 + ALPN "smp/1"

---

## Message Layer Stack (Complete!)

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
│  └── Nonce: 24 bytes, Tag: 16 bytes                            │
├─────────────────────────────────────────────────────────────────┤
│  Layer 4: SMP Client Message                                    │
│  └── [2-byte length prefix] [encrypted_content] [padding]      │
├─────────────────────────────────────────────────────────────────┤
│  Layer 5: Contact DH Encryption (Initial Messages)              │
│  └── [X25519 SPKI key (44 bytes)] [crypto_box encrypted body]  │
│  └── crypto_box(body, nonce, sender_dh_pub, contact_dh_secret) │
├─────────────────────────────────────────────────────────────────┤
│  Layer 6: Agent Protocol Message                                │
│  └── [2-byte version BE] [type: 'C'/'I'/'M'/'R'] [body]        │
└─────────────────────────────────────────────────────────────────┘
```

---

## Key Discoveries (v0.1.12)

### 1. Contact Link URL Encoding (KRITISCH!)

**Problem:** Links mit `+` im DH Key wurden als "Invalid link" abgelehnt.

**Lösung:**
```
DH Key Encoding: Base64URL (nicht Standard Base64!)
- Verwende '-' statt '+'
- Verwende '_' statt '/'
- '=' Padding MUSS doppelt encoded werden: = → %3D → %253D

Vergleich:
FALSCH: dh%3DMCowBQYDK2VuAyEA5nPWbPZTKmf3NdwGzYOq...%2Bv24%3D
                                                    ^^^  ^^^
                                                 Einfach encoded (FALSCH)

RICHTIG: dh%3DMCowBQYDK2VuAyEABo11ArKXGHb9zoz_76yz...%253D
                                              ^       ^^^^^
                                           Base64URL  Doppelt encoded
```

### 2. Message Format nach Layer 3 Decryption

```
┌──────────────────────────────────────────────────────────────────┐
│ Offset 0-1:   Length Prefix (BE)        │ z.B. 0x3E82 = 16002   │
│ Offset 2-5:   Unknown Header            │ 00 00 00 00           │
│ Offset 6-9:   "ip" + 2 bytes            │ 69 70 xx xx           │
│ Offset 10-13: "T " + version "1,"       │ 54 20 00 04 31 2c     │
│ Offset 14-57: X25519 SPKI (44 bytes)    │ 30 2a 30 05 06 03...  │
│ Offset 58+:   crypto_box encrypted body │ [nonce][ciphertext]   │
└──────────────────────────────────────────────────────────────────┘
```

### 3. X25519 SPKI Header Format

```
SPKI Header (12 bytes): 30 2a 30 05 06 03 2b 65 6e 03 21 00
                        │  │  │  │  │  │  │  │  │  │  │  │
                        │  │  │  │  │  │  │  │  │  │  │  └─ 0x00
                        │  │  │  │  │  │  │  │  │  │  └─── BIT STRING length (33)
                        │  │  │  │  │  │  │  │  │  └────── BIT STRING tag (0x03)
                        │  │  │  │  │  │  └──┴──┴───────── OID 1.3.101.110 (X25519)
                        │  │  │  └──┴──┴────────────────── OID container
                        │  │  └─────────────────────────── AlgorithmIdentifier
                        │  └────────────────────────────── Total length (42)
                        └───────────────────────────────── SEQUENCE tag

Full SPKI (44 bytes) = Header (12 bytes) + Raw X25519 Key (32 bytes)
```

### 4. Agent Protocol Message Types

```c
'C' = AgentConfirmation  // Connection confirmation with encrypted connInfo
'I' = AgentInvitation    // Invitation with reply queue URI + profile
'M' = AgentMsgEnvelope   // Double Ratchet encrypted message
'R' = AgentRatchetKey    // Ratchet key exchange
```

### 5. AgentInvitation Format (Type 'I')

```
AgentInvitation = [version:2][type:'I'][connReqLen:2][connReq][connInfo]

connReq = URL-encoded simplex:/invitation#/?v=2-7&smp=...
connInfo = Peer's profile (JSON or binary)

Beispiel decoded:
"simplex:/invitation#/?v=2-7&smp=smp%3A%2F%2F6iIcWT_dF2zN_w5xzZEY7HI2Prbh3ldP07YTyDexPjE%3D%40smp10.simplex.im%2FzeKFSKNA_xTcbWniJn-gB4m9V2RIWZ..."
```

### 6. Agent Version vs SMP Version

```
- SMP Version: "1," in Header (String bei Offset 12)
- Agent Version: 2-byte BE Integer (z.B. 0x0007 = Version 7)
```

---

## Connection Flow (Contact Address q=c)

```
┌──────────┐                              ┌──────────┐
│ SimpleX  │                              │  ESP32   │
│   App    │                              │(Contact) │
└────┬─────┘                              └────┬─────┘
     │  1. Scannt Contact Link                 │
     │  2. SEND AgentInvitation ───────────────>  (Reply Queue + Profile)
     │  3. Wartet auf Accept...                │
     │     <───────────── AgentConfirmation    │  (Zu App's Reply Queue!)
     │  4. "Connected!"                        │
```

---

## Code-Änderungen v0.1.12

### Neue Funktionen

```c
// DH Decryption für Client Messages (Layer 5)
static int decrypt_client_msg(
    const uint8_t *enc, int enc_len,
    const uint8_t *sender_dh_pub,   // 32 bytes raw X25519
    const uint8_t *our_dh_secret,   // 32 bytes
    uint8_t *plain
);

// Verbesserter Agent Message Parser (Layer 6)
static void parse_agent_message(contact_t *contact, const uint8_t *plain, int plain_len);
```

### Entfernte Funktionen

```c
// Nicht mehr benötigt:
- base64_pre_encode()
- base64_std_encode()
- parse_smp_client_header()
- parse_agent_envelope()
```

### URL Encoding Fix

```c
// DH Key jetzt als Base64URL mit Padding
// = wird pre-encoded zu %3D vor dem URL-encode
// Ergebnis: %253D (doppelt encoded)
```

---

## Debugging-Erkenntnisse

| Issue | Solution |
|-------|----------|
| SPKI Detection Bug | Byte-Offsets waren falsch (`i+5` statt `i+4` für 0x06) |
| Kein 'K' Header | Initial Messages haben kein explizites 'K' Prefix - SPKI ist direkt embedded |
| Version String "1," | Zeigt SMP Protocol Version an, bei Offset 12 |
| Agent Version | 2-byte BE Integer, nicht String |

---

## Aktueller Feature-Stand

```
SimpleGo v0.1.12-alpha Feature Matrix:
═══════════════════════════════════════════════════════════════
✅ TLS 1.3 Connection (ALPN: smp/1)
✅ SMP Handshake (ServerHello/ClientHello)
✅ Queue Creation (NEW → IDS)
✅ Queue Subscribe (SUB → OK)
✅ Contact Link Generation (Base64URL + double-encoded =)
✅ Message Receive (MSG)
✅ SMP E2E Decryption (Layer 3)
✅ Client Message Decryption (Layer 5) ← NEU!
✅ Agent Protocol Parsing (Layer 6) ← NEU!
✅ AgentInvitation Detection ('I') ← NEU!
✅ Reply Queue URI Extraction ← NEU!
✅ Peer Profile Visibility ← NEU!
═══════════════════════════════════════════════════════════════
🔜 Parse Reply Queue URI
🔜 Connect to Peer's SMP Server
🔜 Send AgentConfirmation
🔜 Connection Established
🔜 Double Ratchet Implementation
🔜 Send/Receive Chat Messages
═══════════════════════════════════════════════════════════════
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
grep -r "AgentInvitation\|AgentConfirmation" src --include="*.hs"
```

---

## Haskell Source References

| File | Discovery |
|------|-----------|
| Agent/Protocol.hs | Agent message types 'C', 'I', 'M', 'R' |
| Agent/Client.hs | AgentInvitation format with connReq |
| Crypto.hs | Double crypto_box layers |

---

## Next Steps (v0.1.13)

1. **Reply Queue URI Parser** — Extrahiere Server, Queue ID, DH Key
2. **Multi-Server Support** — Verbinde zu smp10.simplex.im (Peer's Server)
3. **AgentConfirmation Builder** — Erstelle Antwort-Nachricht
4. **SEND to Peer** — Sende Confirmation an Peer's Queue
5. **Connection Complete!** — SimpleX App zeigt "Connected"

---

## 🏆 Achievement Unlocked

**"First Native ESP32 SimpleX Client with Full Message Layer Decoding"**

- ✅ TLS 1.3 + SMP Handshake
- ✅ Queue Management (NEW, SUB, DEL)
- ✅ Message Lifecycle (SEND, MSG, ACK)
- ✅ SMP E2E Decryption (Layer 3)
- ✅ **Client Message Decryption (Layer 5)**
- ✅ **Agent Protocol Parsing (Layer 6)**
- ✅ **AgentInvitation + Reply Queue Extraction**

---

*Last updated: January 21, 2026 — v0.1.12-alpha*
