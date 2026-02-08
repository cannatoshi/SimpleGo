# SimpleGo - Current Status (2026-02-08)

**Project:** Native SimpleX SMP Client for ESP32  
**Version:** v0.1.17-alpha  
**Archive:** See `01_SIMPLEX_PROTOCOL_INDEX.md` for complete documentation (410+ sections, 20 parts)

---

## 🎉 HISTORIC MILESTONE: CONNECTED! (2026-02-08 Session 23)

```
═══════════════════════════════════════════════════════════════════════════════

  🎉🎉🎉 FIRST SIMPLEX CONNECTION ON A MICROCONTROLLER! 🎉🎉🎉

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   SimpleX App shows: "ESP32 — Connected"                               │
  │                                                                         │
  │   The world's first native third-party implementation of the           │
  │   SimpleX protocol has successfully established a complete             │
  │   bidirectional connection.                                            │
  │                                                                         │
  │   Date: February 8, 2026 ~17:36 UTC                                    │
  │   Platform: ESP32-S3 (LilyGo T-Deck)                                   │
  │   Protocol: SimpleX Messaging Protocol (SMP)                           │
  │   Encryption: X3DH + Double Ratchet + X448 + AES-256-GCM               │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🎯 Current Status

```
COMPLETE CONNECTION FLOW:
ESP32 ◄──► TLS 1.3 ◄──► SMP Server ◄──► SimpleX App ◄──► "CONNECTED" 🎉
          ✅            ✅              ✅                 ✅
```

---

## ✅ What Works — EVERYTHING!

| Layer | Component | Status | Session |
|-------|-----------|--------|---------|
| 0 | TLS 1.3 Handshake | ✅ | S1-3 |
| 1 | SMP Transport (rcvDhSecret) | ✅ | S4-8 |
| 2 | E2E Decrypt (e2eDhSecret + cmNonce) | ✅ | S18 |
| 2.5 | unPad Layer | ✅ | S19 |
| 3 | ClientMessage Parse | ✅ | S19 |
| 4 | EncRatchetMessage Parse (dynamic KEM) | ✅ | S19, S22 |
| 5 | Double Ratchet Header Decrypt | ✅ | S19, S22 |
| 6 | Double Ratchet Body Decrypt | ✅ | S20, S22 |
| 7 | ConnInfo Parse + Zstd | ✅ | S20 |
| 8 | Peer Profile JSON | ✅ | S20 |
| 9a | TLS Reconnect to Reply Queue | ✅ | S23 |
| 9b | SUB Command (re-subscribe) | ✅ | S23 |
| 9c | KEY Command (authorize sender) | ✅ | S23 |
| 10 | HELLO (ESP32 → App on Q_A) | ✅ | S23 |
| 11 | HELLO (App → ESP32 on Q_B) | ✅ | S23 |
| **12** | **CON — "CONNECTED"** | **✅** | **S23** |

**Result:** `"displayName": "cannatoshi"` on ESP32 + **CONNECTED** in App! 🎉

---

## 📊 Session 23 — The Journey to CONNECTED

### Phase 1: Codebase Analysis
- Claude Code documented 4-layer model (SMP → Server Encrypt → E2E → Ratchet)
- SMPQueueInfo wire format fully documented

### Phase 2: Role Correction — Tag 'I' not 'D'!
- **CRITICAL:** App sends Tag `'I'` (AgentConnInfo), NOT Tag `'D'`!
- We send `'D'` (with Reply Queue), App sends `'I'` (profile only)
- Session 22's assumption was WRONG

### Phase 3: Handshake Flow Clarification
- 7-step flow identified and verified
- Legacy Path (PHConfirmation 'K') requires KEY + HELLO
- Modern Path (PHEmpty '_') would skip HELLO

### Phase 4: PrivHeader Identification
- Log showed: `PrivHeader tag: 0x4B 'K' = PHConfirmation`
- Confirms Legacy Path — KEY + HELLO needed!

### Phase 5: KEY Command
- TLS connection timed out during processing
- Solution: Reconnect → SUB → KEY
- Server responded: OK ✅

### Phase 6: HELLO + CONNECTED!
- ESP32 sends HELLO on Q_A
- App sends HELLO on Q_B
- **Both sides: CON → "CONNECTED"!** 🎉

---

## 📊 Complete 7-Step Handshake (Verified!)

```
Step   Queue   Direction      Content                           Status
──────────────────────────────────────────────────────────────────────────
1.     —       App            NEW → Q_A, Invitation QR           ✅
2a.    Q_A     ESP32→App      SKEY (Register Sender Auth)        ✅
2b.    Q_A     ESP32→App      CONF Tag 'D' (Q_B + Profile)       ✅
3.     —       App            processConf → CONF Event           ✅
4.     —       App            LET/Accept Confirmation            ✅
5a.    Q_A     App            KEY on Q_A (senderKey)             ✅
5b.    Q_B     App→ESP32      SKEY on Q_B                        ✅
5c.    Q_B     App→ESP32      Tag 'I' (App Profile)              ✅
6a.    Q_B     ESP32          Reconnect + SUB + KEY              ✅
6b.    Q_A     ESP32→App      HELLO                              ✅
6c.    Q_B     App→ESP32      HELLO                              ✅
7.     —       Both           CON — "CONNECTED" 🎉               ✅
```

---

## 📋 Bug Summary

**Total: 31 bugs found and fixed (ALL IN SESSIONS 4-22)**

**🎉 Session 23: ZERO new bugs! The crypto was already correct!**

| Sessions | Bugs | Category |
|----------|------|----------|
| S4 | #1-8 | Wire format, length prefixes, KDF order |
| S5 | #9 | wolfSSL X448 byte order |
| S6 | #10-12 | SMPQueueInfo encoding |
| S8 | #13-14 | AAD prefix, IV assignment |
| S9 | #15-16 | HSalsa20, A_CRYPTO |
| S10C | #17 | cmNonce vs msgId |
| S12-18 | #18 | Reply Queue E2E (ONE LINE FIX!) |
| S19-20 | #19 | header_key_recv overwritten |
| S21 | #20-26 | HELLO format + v3 EncRatchetMessage |
| S22 | #27-31 | E2E v3, KEM parser, NHK promotion |
| **S23** | **ZERO** | **🎉 CONNECTED!** |

**Lessons Learned: 95** (documented in BUG_TRACKER.md)

---

## 📐 Quick Reference - Constants (Updated S23)

```c
// Version Numbers
#define AGENT_VERSION               7      // AgentConfirmation
#define AGENT_MSG_VERSION           1      // AgentMessage (HELLO etc.)
#define E2E_VERSION                 2
#define RATCHET_VERSION             3      // Changed v2→v3 in S21!
#define VERSION_MIN_CONFIRMATION    3      // Must match RATCHET_VERSION! S22

// Header Sizes (DYNAMIC in v3+PQ!)
#define EM_HEADER_SIZE_V2           123    // v2: 1-byte prefix
#define EM_HEADER_SIZE_V3           124    // v3: 2-byte prefix
// v3+PQ: ~2346 bytes (variable, calculate dynamically!)

// Other Sizes
#define MSG_HEADER_SIZE             88     // MsgHeader (padded, without PQ)
#define RCAD_SIZE                   112    // Associated Data (rcAD)
#define PAYLOAD_AAD_SIZE_V2         235    // rcAD + emHeader v2
#define PAYLOAD_AAD_SIZE_V3         236    // rcAD + emHeader v3

// Padding
#define PADDING_CONNINFO            14832  // ConnInfo (Tag 'D' or 'I')
#define PADDING_HELLO               15840  // HELLO / A_MSG (non-PQ)

// SNTRUP761 Post-Quantum KEM (S22)
#define SNTRUP761_PUBKEY_SIZE       1158
#define SNTRUP761_CIPHERTEXT_SIZE   1039
#define SNTRUP761_SECRET_SIZE       32
```

---

## 📐 Quick Reference - KEY Command (S23)

```
KEY Body: "KEY " + senderKey

senderKey:
  [1B len=0x2C] + [44B Ed25519 X.509 SPKI DER]

Full body: "KEY " + 0x2C + peer_sender_auth_key[44]
Total: 4 + 1 + 44 = 49 bytes

Signed with: rcv_private_auth_key (OUR recipient private key!)
This is a RECIPIENT command — we authorize senders on OUR queue.

Server Response:
  OK    → Sender authorized successfully
  ERR   → Authorization failed
```

---

## 📐 Quick Reference - Roles (S23)

```
Role          Party    Creates          Sends Tag
────────────────────────────────────────────────────────
Accepting     ESP32    Reply Queue (B)  'D' (with Q_B)
Initiating    App      Contact Queue    'I' (profile only)

Bob (ESP32):
  - Creates Reply Queue → sends in Tag 'D'
  - Sends HELLO on Contact Queue (Q_A)
  - Authorizes App with KEY command

Alice (App):
  - Creates Invitation → Contact Queue (Q_A)
  - Sends Tag 'I' (profile only, no queue)
  - Sends HELLO on Reply Queue (Q_B)
```

---

## 📐 Quick Reference - Legacy vs Modern Path (S23)

```
PHConfirmation 'K' (0x4B) → Legacy Path:
  ✓ Requires KEY command on Reply Queue
  ✓ Requires HELLO exchange (both directions)
  ✓ This is what we use!

PHEmpty '_' (0x5F) → Modern Path (senderCanSecure):
  ✗ Only ACK, CON immediate
  ✗ No HELLO needed
  ✗ NOT what the App uses with us!

Session 22's "No HELLO needed" theory was WRONG for Legacy Path!
```

---

## 📝 Key Learnings Session 23

1. **Role clarity:** ESP32 = Bob (Accepting), App = Alice (Initiating)
2. **Tag 'D' vs 'I':** We send 'D' (with Reply Queue), App sends 'I' (profile only)
3. **Legacy Path:** PHConfirmation 'K' → KEY + HELLO required
4. **KEY is Recipient Command:** Signed with rcv_private_auth_key
5. **TLS timeout matters:** Reconnect before KEY command
6. **Sequence critical:** KEY before HELLO
7. **Reconnect sequence:** TLS → SUB → KEY
8. **7-step handshake:** Exactly 7 steps for Legacy Path
9. **CONNECTED needs BOTH HELLOs:** We send on Q_A, App sends on Q_B
10. **Session 22 assumption was WRONG:** Legacy Path needs HELLO!

---

## 📁 Documentation Files

| File | Description |
|------|-------------|
| `01_SIMPLEX_PROTOCOL_INDEX.md` | Navigation index |
| `02_SIMPLEX_STATUS.md` | This file - quick status |
| `README.md` | Project overview |
| `BUG_TRACKER.md` | All 31 bugs, 95 lessons |
| `QUICK_REFERENCE.md` | Constants, wire formats |
| `03-21_PART*.md` | Sessions 1-22 documentation |
| `22_PART20_SESSION_23.md` | **🎉 CONNECTED!** |

---

## 🎯 Next Steps (Post-Connection)

1. **Bidirectional Chat Messages** — Send and receive actual chat messages
2. **Message Persistence** — Store messages on ESP32 flash
3. **UI Integration** — Display on LilyGo T-Deck screen
4. **Multiple Contacts** — Handle more than one connection
5. **Reconnection Logic** — Handle connection drops gracefully
6. **Post-Quantum Upgrade** — Implement SNTRUP761 KEM exchange

---

*Status updated: 2026-02-08 Session 23 — 🎉 FIRST SIMPLEX CONNECTION ON A MICROCONTROLLER!*
