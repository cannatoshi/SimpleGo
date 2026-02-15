# SimpleGo - Current Status (2026-02-14)

**Project:** Native SimpleX SMP Client for ESP32  
**Version:** v0.1.18-alpha  
**Archive:** See `01_SIMPLEX_PROTOCOL_INDEX.md` for complete documentation (457+ sections, 23 parts)

---

## 🗄️ LATEST: MILESTONE 6! (2026-02-14 Session 26)

```
═══════════════════════════════════════════════════════════════════════════════

  🗄️🗄️🗄️ RATCHET STATE PERSISTENCE! 🗄️🗄️🗄️

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   MILESTONE 6: ESP32 survives reboot without losing crypto state!      │
  │                                                                         │
  │   - Ratchet state restored from NVS flash                              │
  │   - Queue credentials persisted                                        │
  │   - Delivery receipts work after reboot                                │
  │   - Write-Before-Send: 7.5ms verified                                  │
  │                                                                         │
  │   Date: February 14, 2026 (Valentine's Day Part 2)                     │
  │   Platform: ESP32-S3 (LilyGo T-Deck)                                   │
  │   NVS Capacity: 150+ contacts                                          │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🎯 SESSION 25: Bidirectional Chat + Receipts! (2026-02-14)

```
═══════════════════════════════════════════════════════════════════════════════

  🎯🎯🎯 BIDIRECTIONAL ENCRYPTED CHAT + DELIVERY RECEIPTS! 🎯🎯🎯

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   MILESTONE 3: First App message decrypted on ESP32                    │
  │   MILESTONE 4: Bidirectional encrypted chat ESP32 ↔ SimpleX App        │
  │   MILESTONE 5: Delivery receipts (✓✓) working!                         │
  │                                                                         │
  │   Date: February 14, 2026 (Valentine's Day!)                           │
  │   Platform: ESP32-S3 (LilyGo T-Deck)                                   │
  │   Refactoring: main.c 2440 → 611 lines (−75%)                          │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🏆 MILESTONE #2: First Chat Message! (2026-02-11 Session 24)

```
═══════════════════════════════════════════════════════════════════════════════

  🏆🏆🏆 FIRST CHAT MESSAGE FROM A MICROCONTROLLER! 🏆🏆🏆

  SimpleX App shows: "Hello from ESP32!"
  Date: February 11, 2026
  Stack: Double Ratchet → AgentMsgEnvelope → E2E → SEND → App

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🎉 MILESTONE #1: CONNECTED! (2026-02-08 Session 23)

```
═══════════════════════════════════════════════════════════════════════════════

  🎉🎉🎉 FIRST SIMPLEX CONNECTION ON A MICROCONTROLLER! 🎉🎉🎉

  SimpleX App shows: "ESP32 — Connected"
  Date: February 8, 2026 ~17:36 UTC

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🎯 Current Status

```
BIDIRECTIONAL COMMUNICATION:
ESP32 ──► "Hello from ESP32!" ──► App ✅ WORKS!
ESP32 ◄── "Hello?" ◄── App ✅ WORKS!
App shows ✓✓ for received messages ✅ WORKS!
```

---

## ✅ What Works — EVERYTHING!

| Component | Status | Session |
|-----------|--------|---------|
| TLS 1.3 | ✅ | S1-3 |
| SMP Handshake | ✅ | S4-8 |
| Queue Creation | ✅ | S4-8 |
| Invitation Parsing | ✅ | S4-8 |
| X3DH Key Agreement | ✅ | S4-8 |
| Double Ratchet Init | ✅ | S4-8 |
| X448 DH | ✅ | S5 |
| HKDF-SHA512 | ✅ | S4-8 |
| AES-GCM Encryption | ✅ | S4-8 |
| Wire Format | ✅ | S4 |
| Padding | ✅ | S4-8 |
| AAD | ✅ | S8 |
| IV Order | ✅ | S8 |
| AgentConfirmation | ✅ | S8 |
| E2E Encryption | ✅ | S18 |
| Double Ratchet Header Decrypt | ✅ | S19 |
| Double Ratchet Body Decrypt | ✅ | S20 |
| Peer Profile Parsing | ✅ | S20 |
| v3 EncRatchetMessage | ✅ | S21 |
| HELLO Exchange | ✅ | S21-23 |
| KEY Command | ✅ | S23 |
| Reply Queue Setup | ✅ | S22-23 |
| **CONNECTED** | ✅ | **S23** |
| A_MSG Send | ✅ | S24 |
| ChatMessage JSON | ✅ | S24 |
| ACK Protocol | ✅ | S24 |
| **First Chat Message** | ✅ | **S24** |
| A_MSG Receive | ✅ | S25 |
| Ratchet State Persistence | ✅ | S25 |
| **Bidirectional Chat** | ✅ | **S25** |
| **Delivery Receipts (✓✓)** | ✅ | **S25** |

**Result:** Full bidirectional encrypted chat with receipts! 🎯

---

## 📊 Session 25 — The Valentine's Day Session

### Phase 1: Massive Refactoring
```
main.c: 2440 → 611 lines (−75%)

New modules:
  - smp_ack.c/h      ACK handling
  - smp_wifi.c/h     WiFi initialization
  - smp_e2e.c/h      E2E envelope decryption
  - smp_agent.c/h    Agent protocol layer
```

### Phase 2: Bidirectional Bug Fixes (8 bugs)
- Nonce offset: 14 → 13 (brute-force discovered!)
- Ratchet state: Copy → Pointer (persistence)
- Chain KDF: Relative → Absolute skip
- txCount parser: Hardcoded → Variable
- Heap overflow: malloc(256) → dynamic
- Receipt count: Word16 → Word8
- Receipt rcptInfo: Word32 → Word16
- NULL guard: contact check for Reply Queue

### Phase 3: Delivery Receipts
- Receipt wire format documented
- count=Word8, rcptInfo=Word16 (corrected)
- App shows ✓✓ for ESP32-received messages!

---

## 📊 Session 24 — First Chat Message

### Key Achievements
- First A_MSG sent: "Hello from ESP32!"
- ChatMessage JSON format discovered
- Q_B Ratchet decrypt working
- ACK protocol documented
- PQ-Kyber graceful degradation verified

---

## 📊 Session 23 — CONNECTED

### Key Achievements
- ZERO new bugs (31 total sufficient)
- Complete 7-step handshake verified
- Role clarification: ESP32=Bob, App=Alice
- KEY command on Reply Queue
- TLS reconnect + SUB + KEY sequence

---

## 📋 Complete Bug List (39 Bugs - ALL FIXED!)

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
| S23 | ZERO | CONNECTED! |
| S24 | ZERO | First Chat Message! |
| S25 | #32-39 | Bidirectional + Receipts |

**All 39 bugs FIXED!**

---

## 📐 Quick Reference - Constants

```c
// Padding sizes
#define E2E_ENC_CONN_INFO_LENGTH    14832  // AgentConfirmation
#define E2E_ENC_AGENT_MSG_LENGTH    15840  // HELLO, A_MSG, etc.
#define E2E_ENC_CONFIRMATION_LENGTH 15904  // Outer ClientMessage

// Structure sizes
#define EM_HEADER_SIZE_V2           123    // EncMessageHeader (v2)
#define EM_HEADER_SIZE_V3           124    // EncMessageHeader (v3)
#define MSG_HEADER_SIZE             88     // MsgHeader (padded)
#define HELLO_SIZE                  12     // HELLO Plaintext
#define E2E_PARAMS_SIZE             140    // SndE2ERatchetParams
#define RCAD_SIZE                   112    // Associated Data (rcAD)
#define PAYLOAD_AAD_SIZE_V2         235    // rcAD + emHeader (v2)
#define PAYLOAD_AAD_SIZE_V3         236    // rcAD + emHeader (v3)

// Versions
#define AGENT_VERSION               7      // 0x0007
#define E2E_VERSION                 2      // 0x0002
#define RATCHET_VERSION             3      // v3 format
```

---

## 📐 Quick Reference - Session 25 Discoveries

### Nonce Offset for Reply Queue Regular Messages
```
WRONG: Offset 14 (Session 24 assumption)
RIGHT: Offset 13 (Brute-force discovered)

Message format: [12B header][nonce@13][ciphertext]
```

### Ratchet State Persistence
```c
// WRONG — changes lost:
ratchet_state_t rs = *ratchet_get_state();

// CORRECT — changes persist:
ratchet_state_t *rs = ratchet_get_state();
```

### Receipt Wire Format
```
A_RCVD ('V') payload:
  'M' + APrivHeader + 'V' + count(Word8) + [AMessageReceipt...]

AMessageReceipt:
  agentMsgId(8B Int64 BE) + msgHash(1+32B SHA256) + rcptInfo(Word16)
```

---

## 📐 Quick Reference - Wire Formats (Historical)

### AgentConfirmation (S8 Breakthrough!)
```
[2B version=7][1B 'C'][1B '1'][140B E2EParams][Tail encConnInfo]
```

### EncRatchetMessage
```
v2: [1B len=123][123B emHeader][16B authTag][Tail payload]
v3: [2B len=124][124B emHeader][16B authTag][Tail payload]
```

### Payload AAD - CORRECTED in S8!
```
[112B rcAD][emHeader]  ← NO length prefix before emHeader!
```

---

## 📐 Quick Reference - KDF

### Chain KDF Output (96 bytes)
```
Bytes 0-31:  next_chain_key
Bytes 32-63: message_key
Bytes 64-79: MESSAGE_IV (iv1)  ← FOR PAYLOAD!
Bytes 80-95: HEADER_IV (iv2)   ← FOR HEADER!
```

---

## 📝 Key Learnings (Selection)

1. **Wire Format ≠ Crypto Format** - Length prefixes for serialization, not always for AAD (S8)
2. **Haskell Parser Awareness** - `largeP` removes length prefix from parsed object (S8)
3. **Python Verification** - Essential for debugging crypto operations (S4-8)
4. **Community Support** - SimpleX developers are helpful and responsive (S7)
5. **SimpleX uses NON-STANDARD XSalsa20** - HSalsa20(key, zeros[16]) not nonce[0:16] (S16)
6. **Self-decrypt failure is BY DESIGN** - Asymmetric header keys (S16)
7. **Nonce offset varies by message type** - Contact Queue vs Reply Queue (S25)
8. **Ratchet state must persist** - Use pointer, not copy (S25)
9. **App's own messages are best protocol reference** - Byte comparison beats source analysis (S25)

---

## 📁 Documentation Files

| File | Description |
|------|-------------|
| `01_SIMPLEX_PROTOCOL_INDEX.md` | Navigation index |
| `02_SIMPLEX_STATUS.md` | This file - quick status |
| `README.md` | Project overview |
| `BUG_TRACKER.md` | All 39 bugs, 112 lessons |
| `QUICK_REFERENCE.md` | Constants, wire formats |
| `03-24_PART*.md` | Sessions 1-25 documentation |

---

## 🎯 Milestone Overview

| # | Milestone | Date | Session |
|---|-----------|------|---------|
| 0 | 🎉 AgentConfirmation | 2026-01-27 | 8 |
| 1 | 🎉 CONNECTED | 2026-02-08 | 23 |
| 2 | 🏆 First A_MSG | 2026-02-11 | 24 |
| 3 | 📥 App→ESP32 Decrypt | 2026-02-14 | 25 |
| 4 | 🔄 Bidirectional Chat | 2026-02-14 | 25 |
| 5 | ✓✓ Delivery Receipts | 2026-02-14 | 25 |
| **6** | **🗄️ Ratchet Persistence** | **2026-02-14** | **26** |

---

## 🎯 Next Steps (Session 27)

1. **Fix "bad message ID"** — Persist send counter in NVS
2. **Multi-Task Architecture** — SMP Receive, SMP Send, LVGL UI tasks
3. **Complete Chat UI** — Connect keyboard to chat screen
4. **Message bridge** — SMP ↔ LVGL task communication
5. **Post-quantum upgrade** — Full SNTRUP761 KEM implementation

---

*Status updated: 2026-02-14 Session 26 — 🗄️ RATCHET STATE PERSISTENCE!*  
*History: S8 Breakthrough → S23 CONNECTED → S24 First MSG → S25 Bidirectional → S26 Persistence!*
