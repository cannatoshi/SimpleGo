# SimpleGo - Current Status (2026-02-13)

**Project:** Native SimpleX SMP Client for ESP32  
**Version:** v0.1.17-alpha  
**Archive:** See `01_SIMPLEX_PROTOCOL_INDEX.md` for complete documentation (424+ sections, 21 parts)

---

## 🏆 MILESTONE #2: First Chat Message! (2026-02-11 Session 24)

```
═══════════════════════════════════════════════════════════════════════════════

  🏆🏆🏆 FIRST CHAT MESSAGE FROM A MICROCONTROLLER! 🏆🏆🏆

  ┌─────────────────────────────────────────────────────────────────────────┐
  │                                                                         │
  │   SimpleX App shows: "Hello from ESP32!"                               │
  │                                                                         │
  │   The world's first chat message sent from a microcontroller           │
  │   through the complete SimpleX encryption stack.                       │
  │                                                                         │
  │   Date: February 11, 2026                                              │
  │   Platform: ESP32-S3 (LilyGo T-Deck)                                   │
  │   Stack: Double Ratchet → AgentMsgEnvelope → E2E → SEND → App         │
  │                                                                         │
  └─────────────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

---

## 🎯 Current Status

```
COMMUNICATION FLOW:
ESP32 ──► "Hello from ESP32!" ──► App ✅ WORKS!
ESP32 ◄── App messages ◄── App ❌ BLOCKED (server empty)
```

---

## ✅ What Works — Send Direction

| Component | Status | Session |
|-----------|--------|---------|
| TLS 1.3 + SMP | ✅ | S1-8 |
| E2E Encryption | ✅ | S18 |
| Double Ratchet | ✅ | S19-22 |
| AgentMsgEnvelope | ✅ | S24 |
| ChatMessage JSON | ✅ | S24 |
| SEND Command | ✅ | S24 |
| App Display | ✅ | S24 |

**Result:** "Hello from ESP32!" displays in SimpleX App! 🏆

---

## ❌ What's Blocked — Receive Direction

| Component | Status | Notes |
|-----------|--------|-------|
| Q_B Ratchet Decrypt | ✅ | Tag 'I' ConnInfo works |
| Q_B Listen Loop | ✅ | Connection alive |
| Server Messages | ❌ | Zero messages for Q_B |
| App → ESP32 | ❌ | App sends (1 checkmark) but not delivered |

**Root Cause (discovered late in session):** `subscribe_all_contacts()` SUBs on main `ssl`, but listen reads from `queue_conn.ssl`!

**Fix for Session 25:** Process Q_B in Main Receive Loop, or don't SUB Q_B in subscribe_all_contacts().

---

## 📊 Session 24 Achievements

### 1. First Chat Message! 🏆

```c
// ChatMessage JSON format (required!)
const char *msg = "{\"v\":\"1\",\"event\":\"x.msg.new\","
                  "\"params\":{\"content\":{\"type\":\"text\","
                  "\"text\":\"Hello from ESP32!\"}}}";
```

### 2. Session 23 Correction

```
Session 23: "HELLO received on Q_B" → FALSE POSITIVE!
Reality: Random 0x48 byte in Ratchet ciphertext matched 'H'
Actual Q_B content: Tag 'I' ConnInfo (after full Ratchet decrypt)
```

### 3. ACK Protocol Documented

```
SMP Flow Control:
  Server delivers MSG → blocks until ACK
  Missing ACK = queue backs up, no further delivery
  ACK is Recipient Command (rcv_private_auth_key)
  ACK response: OK (empty) or MSG (next message)
```

### 4. PQ-Kyber Graceful Degradation

```
App sends: emHeaderLen=2346 (Post-Quantum Kyber)
We send:   emHeaderLen=124 (pure DH, KEM Nothing)
Result:    Both directions decrypt successfully!
```

### 5. Queue IDs Verified

```
Aschenputtel byte-for-byte comparison:
  sndId    ✅ IDENTICAL
  rcvId    ✅ IDENTICAL  
  Server   ✅ IDENTICAL
  keyHash  ✅ IDENTICAL
  e2e_pub  ✅ IDENTICAL

Conclusion: Queue IDs are NOT the problem.
```

---

## 📐 Quick Reference - A_MSG Format (S24)

```
AgentMessage for A_MSG:
Offset  Size   Field               Value
──────────────────────────────────────────────────────────
0       1      AgentMessage tag    'M' (0x4D)
1       8      sndMsgId            Int64 BE (starts at 1)
9       1      prevMsgHash len     0x00 (first) or 0x20
10      0|32   prevMsgHash data    empty or SHA-256
10|42   1      AMessage tag        'M' (0x4D) for A_MSG
11|43   N      msgBody             ChatMessage JSON (Tail)
```

---

## 📐 Quick Reference - ChatMessage JSON (S24)

```json
{
  "v": "1",
  "event": "x.msg.new",
  "params": {
    "content": {
      "type": "text",
      "text": "Hello from ESP32!"
    }
  }
}
```

Event types: `x.msg.new`, `x.msg.update`, `x.msg.del`, `x.file`, `x.info`
Content types: `text`, `file`, `image`, `voice`

---

## 📐 Quick Reference - ACK Protocol (S24)

```
ACK Wire Format:
  "ACK " + [1B len][N bytes msgId]
  Signed with: rcv_private_auth_key (Recipient Command)

ACK Response:
  "OK"      — Queue empty
  "MSG ..." — Next message delivered immediately!

Agent-Level Timing:
  Confirmation  → ACK immediately (auto)
  HELLO         → ACK immediately + Delete
  A_MSG         → ACK deferred (app decides)
```

---

## 📋 Bug Summary

**Total: 31 bugs found and fixed (ALL IN SESSIONS 4-22)**

**Sessions 23 & 24: ZERO new bugs — milestones achieved with solid crypto!**

**Lessons Learned: 110** (documented in BUG_TRACKER.md)

---

## 📝 Key Learnings Session 24

1. **msgBody must be ChatMessage JSON** — Raw UTF-8 fails!
2. **Session 23 "HELLO on Q_B" was FALSE POSITIVE** — Random byte match
3. **SMP ACK is critical flow control** — Missing ACK blocks delivery
4. **ACK is Recipient Command** — Signed with rcv_private_auth_key
5. **Response multiplexing** — OK/MSG/END can interleave anytime
6. **pending_msg buffer needed** — Catch MSG during ACK/SUB
7. **PQ-Kyber graceful degradation works** — App sends 2346B headers
8. **Scan-based > Parser-based** — Simple "find OK/MSG" wins
9. **One checkmark ≠ delivered** — Server accepted, not delivered
10. **App may not fully activate** — Shows "Connected" but Q_B empty

---

## 📁 Documentation Files

| File | Description |
|------|-------------|
| `01_SIMPLEX_PROTOCOL_INDEX.md` | Navigation index |
| `02_SIMPLEX_STATUS.md` | This file - quick status |
| `README.md` | Project overview |
| `BUG_TRACKER.md` | All 31 bugs, 109 lessons |
| `QUICK_REFERENCE.md` | Constants, wire formats |
| `03-22_PART*.md` | Sessions 1-23 documentation |
| `23_PART21_SESSION_24.md` | **🏆 First Chat Message!** |

---

## 🎯 Next Steps (Session 25)

### Phase 1: Code Refactoring
```
main.c from 2400 lines → ~150 lines
Extract: msg_handler, agent_handler, chat, listen
```

### Phase 2: Bidirectional Bug Fix
```
Haskell analysis:
  - What does App validate?
  - What triggers full activation?
  - What causes silent discard?
  
(Note: Late-session discovery suggests socket routing issue)
```

### Phase 3: Full Bidirectional
```
Third milestone: Receive messages from App
  - Fix format/socket issue
  - Receive A_MSG on Q_B
  - Display on T-Deck
```

---

*Status updated: 2026-02-13 Session 24 — 🏆 FIRST CHAT MESSAGE FROM A MICROCONTROLLER!*
