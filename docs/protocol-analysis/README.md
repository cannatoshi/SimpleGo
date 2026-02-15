![SimpleGo](docs/gfx/sg_multi_agent_ft_header.png)

# SimpleGo Protocol Analysis

## Complete Development Documentation

This directory contains the complete, unabridged documentation of SimpleGo's development journey - the **world's first native SMP protocol implementation** outside the official SimpleX Haskell codebase.

---

## 🗄️ LATEST: Persistence! (2026-02-14 Session 26)

```
═══════════════════════════════════════════════════════════════════════════════

  🗄️🗄️🗄️ RATCHET STATE PERSISTENCE! 🗄️🗄️🗄️

  MILESTONE 6: ESP32 survives reboot without losing crypto state!
    • Ratchet state restored from NVS flash
    • Queue credentials persisted
    • Delivery receipts work after reboot
    • Write-Before-Send: 7.5ms verified

  Date: February 14, 2026 (Valentine's Day Part 2)

═══════════════════════════════════════════════════════════════════════════════
```

## 🎯 SESSION 25: Bidirectional Chat + Receipts! (2026-02-14)

```
═══════════════════════════════════════════════════════════════════════════════

  🎯🎯🎯 BIDIRECTIONAL ENCRYPTED CHAT + DELIVERY RECEIPTS! 🎯🎯🎯

  THREE MILESTONES in ONE Valentine's Day Session:
    • Milestone 3: First App message decrypted on ESP32
    • Milestone 4: Bidirectional encrypted chat
    • Milestone 5: Delivery receipts (✓✓)

  Refactoring: main.c 2440 → 611 lines (−75%)
  Date: February 14, 2026

═══════════════════════════════════════════════════════════════════════════════
```

## 🎉 HISTORIC MILESTONE: CONNECTED! (2026-02-08 Session 23)

```
═══════════════════════════════════════════════════════════════════════════════

  🎉🎉🎉 FIRST SIMPLEX CONNECTION ON A MICROCONTROLLER! 🎉🎉🎉

  SimpleX App shows: "ESP32 — Connected"
  Date: February 8, 2026 ~17:36 UTC

═══════════════════════════════════════════════════════════════════════════════
```

## 🏆 MILESTONE #2: First Chat Message! (2026-02-11 Session 24)

```
═══════════════════════════════════════════════════════════════════════════════

  🏆🏆🏆 FIRST CHAT MESSAGE FROM A MICROCONTROLLER! 🏆🏆🏆

  SimpleX App shows: "Hello from ESP32!"
  Date: February 11, 2026
  Stack: Double Ratchet → AgentMsgEnvelope → E2E → SEND → App

═══════════════════════════════════════════════════════════════════════════════
```

## Current Status (2026-02-14 Session 25)

```
SESSION 25 - 🎯 BIDIRECTIONAL CHAT + RECEIPTS!
===================================================

THREE MILESTONES ACHIEVED:
  • Milestone 3: First App message decrypted on ESP32
  • Milestone 4: Bidirectional encrypted chat ESP32 ↔ App
  • Milestone 5: Delivery receipts (✓✓) working!

Session 25 Achievements:
  - 8 bugs fixed (5 critical, 3 high)
  - Nonce offset corrected (13, not 14)
  - Ratchet state persistence fixed
  - Receipt wire format corrected
  - main.c refactored: 2440 → 611 lines (−75%)
  - 4 new modules: smp_ack, smp_wifi, smp_e2e, smp_agent

NEXT: Message persistence, UI integration, multiple contacts
```

---

## Acknowledgments and Respect

**This project would not be possible without the incredible work of the SimpleX team.**

SimpleX Chat represents a groundbreaking achievement in privacy-preserving communication technology. The protocol design is elegant, well-thought-out, and prioritizes user privacy above all else. We have the deepest respect for:

- **Evgeny Poberezkin** and the entire SimpleX Chat team
- The brilliant cryptographic design combining X3DH, Double Ratchet, and post-quantum algorithms
- The commitment to open source (AGPL-3.0) that made this project possible
- The comprehensive Haskell implementation that served as our reference

**Links:**
- SimpleX Chat: https://simplex.chat
- SimpleX GitHub: https://github.com/simplex-chat

---

## Document Structure

| Document | Lines | Description |
|----------|-------|-------------|
| [01_SIMPLEX_PROTOCOL_INDEX.md](01_SIMPLEX_PROTOCOL_INDEX.md) | ~310 | Navigation index |
| [02_SIMPLEX_STATUS.md](02_SIMPLEX_STATUS.md) | ~330 | Current status summary |
| [03_PART1_INTRO_SESSIONS_1-2.md](03_PART1_INTRO_SESSIONS_1-2.md) | ~2300 | Foundation, TLS 1.3, basic SMP |
| [04_PART2_SESSIONS_3-4.md](04_PART2_SESSIONS_3-4.md) | ~1000 | Wire format, bugs #1-8 |
| [05_PART3_SESSIONS_5-6.md](05_PART3_SESSIONS_5-6.md) | ~800 | X448 breakthrough, SMPQueueInfo |
| [06_PART4_SESSION_7.md](06_PART4_SESSION_7.md) | ~3200 | AES-GCM verification, Tail encoding |
| [07_PART5_SESSION_8_BREAKTHROUGH.md](07_PART5_SESSION_8_BREAKTHROUGH.md) | ~400 | AgentConfirmation works! |
| [08_PART6_SESSION_9.md](08_PART6_SESSION_9.md) | ~450 | Reply Queue HSalsa20 fix |
| [09_PART7_SESSION_10.md](09_PART7_SESSION_10.md) | ~400 | cmNonce fix, app "connecting" |
| [10_PART8_SESSION_11.md](10_PART8_SESSION_11.md) | ~400 | Regression & Recovery |
| [11_PART9_SESSION_12.md](11_PART9_SESSION_12.md) | ~400 | E2E Keypair Fix Attempt |
| [12_PART10_SESSION_13.md](12_PART10_SESSION_13.md) | ~700 | E2E Crypto Deep Analysis |
| [13_PART11_SESSION_14.md](13_PART11_SESSION_14.md) | ~900 | DH SECRET VERIFIED! |
| [14_PART12_SESSION_15.md](14_PART12_SESSION_15.md) | ~650 | Root Cause Found |
| [15_PART13_SESSION_16.md](15_PART13_SESSION_16.md) | ~900 | Custom XSalsa20 + Double Ratchet |
| [16_PART14_SESSION_17.md](16_PART14_SESSION_17.md) | ~500 | Key Consistency Debug |
| [17_PART15_SESSION_18.md](17_PART15_SESSION_18.md) | ~600 | 🎉 BUG #18 SOLVED! E2E Decrypt SUCCESS |
| [18_PART16_SESSION_19.md](18_PART16_SESSION_19.md) | ~550 | Header Decrypt SUCCESS! |
| [19_PART17_SESSION_20.md](19_PART17_SESSION_20.md) | ~600 | Body Decrypt SUCCESS! Peer Profile! |
| [20_PART18_SESSION_21.md](20_PART18_SESSION_21.md) | ~700 | v3 Format + HELLO Debugging |
| [21_PART19_SESSION_22.md](21_PART19_SESSION_22.md) | ~600 | Reply Queue Flow Discovery |
| [22_PART20_SESSION_23.md](22_PART20_SESSION_23.md) | ~570 | 🎉 CONNECTED! Historic Milestone! |
| [23_PART21_SESSION_24.md](23_PART21_SESSION_24.md) | ~600 | 🏆 First Chat Message! Milestone #2! |
| [24_PART22_SESSION_25.md](24_PART22_SESSION_25.md) | ~480 | 🎯 Bidirectional + Receipts! M3,4,5! |
| [25_PART23_SESSION_26.md](25_PART23_SESSION_26.md) | ~600 | **🗄️ Persistence! Milestone 6!** |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~1350 | Complete bug documentation (39 bugs, 120 lessons) |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~1130 | Constants, wire formats, verified values |

**Total: ~20,000+ lines of detailed protocol analysis (Session docs + reference docs)**

---

## Project Timeline

| Session | Date | Milestone | Bugs Fixed |
|---------|------|-----------|------------|
| 1-3 | Dec 2025 | Foundation, TLS 1.3, Basic SMP | - |
| 4 | Jan 23, 2026 | Wire format analysis | #1-8 |
| 5 | Jan 24, 2026 | X448 byte-order breakthrough | #9 |
| 6 | Jan 24, 2026 | SMPQueueInfo encoding | #10-12 |
| 7 | Jan 24-25, 2026 | AES-GCM verification, SimpleX contact | - |
| 8 | Jan 27, 2026 | AgentConfirmation WORKS! | #13-14 |
| 9 | Jan 27, 2026 | Reply Queue HSalsa20 fix | #15-16 |
| 10C | Jan 28, 2026 | cmNonce fix, app "connecting" | #17 |
| 11 | Jan 30, 2026 | Regression & Recovery | - |
| 12 | Jan 30, 2026 | E2E Keypair Analysis | - |
| 13 | Jan 30, 2026 | E2E Crypto Deep Analysis | - |
| 14 | Jan 31 - Feb 1 | DH SECRET VERIFIED! | #18 (partial) |
| 15 | Feb 1 | Root Cause Found (later disproven) | #18 (root cause) |
| 16 | Feb 1-3 | Custom XSalsa20 + Double Ratchet | #18 (narrowed) |
| 17 | Feb 4 | Key Consistency Debug | #18 (investigating) |
| 18 | Feb 5 | 🎉 BUG #18 SOLVED! E2E Decrypt SUCCESS! | #18 ✅ SOLVED |
| 19 | Feb 5 | Header Decrypt SUCCESS! MsgHeader Parsed | #19 found |
| 20 | Feb 6 | 🎉 Body Decrypt! Peer Profile on ESP32! | #19 ✅ SOLVED |
| 21 | Feb 6-7 | v3 Format + HELLO Debugging (7 bugs!) | #20-#26 |
| **22** | **Feb 7** | **Reply Queue Flow Discovery (5 bugs!)** | **#27-#31** |
| **23** | **Feb 7-8** | **🎉 CONNECTED! Historic Milestone!** | **ZERO new!** |
| **24** | **Feb 11-13** | **🏆 First Chat Message! Milestone #2!** | **ZERO new!** |
| **25** | **Feb 13-14** | **🎯 Bidirectional + Receipts! M3,4,5!** | **8 bugs!** |
| **26** | **Feb 14** | **🗄️ Persistence! Milestone 6!** | **0 bugs** |

---

## Session 26 Key Achievements — 🗄️ Persistence!

### 1. MILESTONE 6: Ratchet State Persistence!

```
ESP32 survives reboot without losing crypto state!

Write-Before-Send pattern (Evgeny's golden rule):
  Generate key → Persist to flash → THEN send

NVS Storage: 128KB partition, 150+ contacts supported
Write timing: 7.5ms verified (negligible vs network latency)
```

### 2. Storage Architecture

```
NVS (Internal Flash)     SD Card (External)
├── Ratchet States       ├── Message History
├── Queue Credentials    ├── Contact Profiles
├── Peer Connection      └── File Attachments
└── Device Config

Capacity: 256 million texts, 19 years mixed usage on 128GB
```

### 3. Delivery Receipts After Reboot

```
Test: Reboot ESP32 → App sends message → ESP32 decrypts → ✓✓ works!
Verified with multiple consecutive reboots.
```

### 4. Keyboard Integration (Partial)

```
Working: Keyboard → Serial → Message arrives in app ✅
Not working: Chat screen UI (architecture refactor needed)
```

---

## Session 25 Key Achievements — 🎯 Bidirectional + Receipts!

### 1. THREE MILESTONES IN ONE SESSION!

```
Milestone 3: First App message decrypted on ESP32
Milestone 4: Bidirectional encrypted chat ESP32 ↔ App
Milestone 5: Delivery receipts (✓✓) working!
```

### 2. Massive Refactoring

```
main.c: 2440 → 611 lines (−75%)

New modules:
  - smp_ack.c/h      ACK handling
  - smp_wifi.c/h     WiFi initialization
  - smp_e2e.c/h      E2E envelope decryption
  - smp_agent.c/h    Agent protocol layer
```

### 3. Critical Bug Fixes (8 total)

```
Nonce Offset:     14 → 13 (brute-force discovered)
Ratchet State:    Copy → Pointer (persistence bug)
Chain KDF:        Relative → Absolute skip
Receipt count:    Word16 → Word8
Receipt rcptInfo: Word32 → Word16
txCount Parser:   Hardcoded → Variable
Heap Overflow:    malloc(256) → dynamic
NULL Guard:       contact check for Reply Queue
```

### 4. Key Discovery: Nonce Offset 13

```
Session 24 believed: Byte [12] = corrId '0' → use cache
Session 25 discovered: Byte [12] = first nonce byte!

Brute-force scan: ✅ DECRYPT OK at nonce_offset=13!
```

### 5. Receipt Wire Format

```
A_RCVD ('V') payload:
  'M' + APrivHeader + 'V' + count(Word8) + [AMessageReceipt...]

AMessageReceipt:
  agentMsgId(8B) + msgHash(1+32B) + rcptInfo(Word16)
```

---

## Session 24 Key Achievements — 🏆 First Chat Message!

### 1. ZERO New Bugs — Again!

All 31 bugs from Sessions 4-22 remain sufficient. Session 23 and 24 achieved milestones with ZERO new bugs!

### 2. First Chat Message from Microcontroller

```
"Hello from ESP32!" displayed in SimpleX App!

Required: ChatMessage JSON format (not raw UTF-8)
  {"v":"1","event":"x.msg.new","params":{"content":{"type":"text","text":"Hello from ESP32!"}}}
```

### 3. Session 23 Correction

```
Session 23: "HELLO received on Q_B" → FALSE POSITIVE!
Reality: Random 0x48 byte in Ratchet ciphertext matched 'H'
Actual: Tag 'I' ConnInfo (after implementing Q_B Ratchet decrypt)
```

### 4. ACK Protocol Documented

```
SMP Flow Control:
  - Server delivers MSG → blocks until ACK
  - Missing ACK = queue backs up, no further delivery
  - ACK is Recipient Command (signed with rcv_private_auth_key)
  - ACK response can be OK (empty) or MSG (next message)
```

### 5. PQ-Kyber Graceful Degradation Verified

```
App sends: emHeaderLen=2346 (Post-Quantum Kyber)
Our sends: emHeaderLen=124 (pure DH, KEM Nothing)
Result: Both directions work! Graceful degradation successful.
```

### 6. Bug Fixed in Session 25

```
Session 24 Hypothesis: Format error in AgentConfirmation or HELLO
Session 25 Discovery: Nonce offset was 14 instead of 13!

Brute-force scan found the truth, bidirectional now works!
```

---

## Session 23 Key Achievements — 🎉 CONNECTED!

### 1. ZERO New Bugs!

All 31 bugs from Sessions 4-22 were sufficient. The crypto was already correct!

### 2. Role Clarification

| Role | Party | Creates | Sends Tag |
|------|-------|---------|-----------|
| Bob | ESP32 (Accepting) | Reply Queue (Q_B) | Tag 'D' (with Q_B info) |
| Alice | App (Initiating) | Contact Queue (Q_A) | Tag 'I' (profile only) |

**Session 22 assumed** App sends Reply Queue info in Tag 'D' — **WRONG!**  
**Session 23 discovered** WE send Tag 'D', App sends Tag 'I'.

### 3. Legacy vs Modern Path

```
PHConfirmation 'K' → Legacy Path:
  - Requires KEY command + HELLO exchange
  - Both parties must send HELLO
  - We use this path!

PHEmpty '_' → Modern Path (senderCanSecure):
  - Only ACK, CON immediate
  - No HELLO needed
  - NOT what the App uses with us!
```

### 4. KEY Command Discovery

```
KEY = Recipient Command:
  - Signed with: rcv_private_auth_key (OUR key)
  - Sent on: OUR queue (where we're recipient)
  - Authorizes: The SENDER (App) to send messages
  - Body: "KEY " + 0x2C + 44B peer_sender_auth_key SPKI
```

### 5. TLS Timeout + Reconnect

```
Problem: Reply Queue TLS connection times out during Confirmation processing

Solution:
  1. Reconnect TLS to Reply Queue server
  2. Send SUB (re-subscribe to queue)
  3. Send KEY command
  4. Send HELLO
```

### 6. Complete 7-Step Handshake Verified

```
Step   Queue   Direction      Content                           
──────────────────────────────────────────────────────────────
1.     —       App            NEW → Q_A, Invitation QR           
2a.    Q_A     ESP32→App      SKEY (Register Sender Auth)        
2b.    Q_A     ESP32→App      CONF Tag 'D' (Q_B + Profile)       
3.     —       App            processConf → CONF Event           
4.     —       App            LET/Accept Confirmation            
5a.    Q_A     App            KEY on Q_A (senderKey)             
5b.    Q_B     App→ESP32      SKEY on Q_B                        
5c.    Q_B     App→ESP32      Tag 'I' (App Profile)              
6a.    Q_B     ESP32          Reconnect + SUB + KEY              
6b.    Q_A     ESP32→App      HELLO                              
6c.    Q_B     App→ESP32      HELLO                              
7.     —       Both           CON — "CONNECTED" 🎉               
```

### 7. Evgeny Contact Restored

Evgeny reached out on Feb 8 — he wasn't upset about the deleted conversation, 
he had simply missed the file! Relationship restored, SimpleX team continues support.

---

## Session 22 Key Achievements

### 1. Five Bugs Fixed (#27-#31)

| Bug | Component | Fix |
|-----|-----------|-----|
| #27 | E2E version_min | 2 → 3 + KEM Nothing (App breaks silence!) |
| #28 | KEM Parser | Dynamic for SNTRUP761 (up to 2346 bytes) |
| #29 | Body Decrypt Pointer | Dynamic emHeader size calculation |
| #30 | HKs/NHKs Init + Promotion | Three-part header key chain fix |
| #31 | Header Decrypt Try-Order | HKr first, NHKr second for AdvanceRatchet |

### 2. Protocol Discovery (Later Corrected in S23!)

**Session 22 Theory:** Modern SimpleX (v2 + `senderCanSecure = True`) does NOT need HELLO!

**Session 23 Correction:** This is only true for Modern Path (PHEmpty '_').
We receive PHConfirmation 'K' = Legacy Path = KEY + HELLO required!

```
Session 22 Assumed Flow (WRONG for Legacy Path):
  1. ESP32 creates Invitation           ✅ Working
  2. App sends AgentConfirmation        ✅ Working
  3. ESP32 extracts Reply Queue Info    ← WRONG! App sends 'I', not 'D'!
  4-7. Modern Path flow                 ← WRONG! We use Legacy Path!

Session 23 Correct Flow (Legacy Path):
  See 7-step handshake above!
```

### 3. Post-Quantum KEM

SimpleX uses **SNTRUP761** (not Kyber1024):
- Public Key: 1158 bytes
- Ciphertext: 1039 bytes
- Shared Secret: 32 bytes

PQ-Graceful-Degradation: v3 + KEM Nothing → pure DH fallback (no error).

### 4. Reply Queue Info Location

The `smpReplyQueues` are inside Tag `'D'` (AgentConnInfoReply) at the innermost
ratchet-decrypted layer. **We send this, not receive it!** (Corrected in S23)

---

## Session 21 Key Achievements

### 1. Seven HELLO Format Bugs Fixed (#20-#26)

| Bug | Component | Fix |
|-----|-----------|-----|
| #20 | PrivHeader for HELLO | '_' → 0x00 (no PrivHeader) |
| #21 | AgentVersion | v2 → v1 for AgentMessage |
| #22 | prevMsgHash | Raw → Word16 prefix encoding |
| #23 | cbEncrypt padding | Pad BEFORE encrypt |
| #24 | DH Key selection | rcv_dh → snd_dh for HELLO |
| #25 | PubHeader Nothing | Missing → '0' (0x30) |
| #26 | v2/v3 format | 1-byte → 2-byte prefixes + KEM Nothing |

### 2. v3 EncRatchetMessage Format

```
v3 changes from v2:
  - emHeader prefix: 1 byte → 2 bytes Word16 BE
  - emHeader size: 123 → 124 bytes
  - ehBody prefix: 1 byte → 2 bytes Word16 BE
  - MsgHeader: +KEM Nothing ('0'), contentLen 79→80
  - Verified byte-correct, Server accepts with OK
```

### 3. New Architecture

- **4 Header Keys:** HKs/NHKs/HKr/NHKr with promotion
- **SameRatchet vs AdvanceRatchet** modes
- **KEY Command** implemented (optional for unsecured queues)

---

## Session 20 Key Achievements

### 1. Bug #19 FIXED

Root cause: Debug self-decrypt test corrupted ratchet state.

### 2. Complete Crypto Chain

```
TLS 1.3 → SMP Transport → Server Decrypt → E2E Decrypt → unPad
→ ClientMessage → EncRatchetMessage → Header Decrypt
→ DH Ratchet Step → Chain KDF → Body Decrypt
→ unPad → ConnInfo 'I' → Zstd → Peer Profile JSON
```

### 3. Peer Profile Read

`"displayName": "cannatoshi"` — first SimpleX profile read on ESP32!

---

## Session 19 Key Achievements

### 1. Three New Layers + Header Decrypt SUCCESS

- unPad Layer, ClientMessage Layer, EncRatchetMessage Layer
- MsgHeader fully parsed (msgMaxVersion=3, PN=0, Ns=0)

---

## Session 18 Key Achievements

### 1. 🎉 BUG #18 SOLVED After Weeks of Debugging!

**Root Cause:** `envelope_len = plain_len - 2` included SMP padding  
**Fix:** `envelope_len = raw_len_prefix` — ONE LINE!

---

## Quick Navigation

### By Bug Number

| Bug | Description | Document |
|-----|-------------|----------|
| #1-8 | Wire format bugs | [Part 2](04_PART2_SESSIONS_3-4.md) |
| #9 | wolfSSL X448 byte-order | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #10-12 | SMPQueueInfo encoding | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #13-14 | AAD prefix, IV order | [Part 5](07_PART5_SESSION_8_BREAKTHROUGH.md) |
| #15-16 | HSalsa20, A_CRYPTO | [Part 6](08_PART6_SESSION_9.md) |
| #17 | cmNonce instead of msgId | [Part 7](09_PART7_SESSION_10.md) |
| #18 | Reply Queue E2E — SOLVED! | [Part 15](17_PART15_SESSION_18.md) |
| #19 | header_key_recv — SOLVED! | [Part 16](18_PART16_SESSION_19.md) + [Part 17](19_PART17_SESSION_20.md) |
| #20-#26 | HELLO format + v3 | [Part 18](20_PART18_SESSION_21.md) |
| **#27-#31** | **E2E v3, KEM, NHK, Try-Order** | [**Part 19**](21_PART19_SESSION_22.md) |

### By Topic

| Topic | Document |
|-------|----------|
| TLS 1.3, Basic SMP | [Part 1](03_PART1_INTRO_SESSIONS_1-2.md) |
| Wire format, smpEncode | [Part 2](04_PART2_SESSIONS_3-4.md), [Part 4](06_PART4_SESSION_7.md) |
| X448 Cryptography | [Part 3](05_PART3_SESSIONS_5-6.md) |
| AgentConfirmation | [Part 5](07_PART5_SESSION_8_BREAKTHROUGH.md) |
| Reply Queue E2E | [Part 6](08_PART6_SESSION_9.md) - [Part 15](17_PART15_SESSION_18.md) |
| Double Ratchet Header | [Part 16](18_PART16_SESSION_19.md) |
| Double Ratchet Body | [Part 17](19_PART17_SESSION_20.md) |
| ConnInfo + Zstd | [Part 17](19_PART17_SESSION_20.md) |
| HELLO + v3 Format | [Part 18](20_PART18_SESSION_21.md) |
| Reply Queue Flow | [**Part 19**](21_PART19_SESSION_22.md) |
| All Bugs | [BUG_TRACKER](BUG_TRACKER.md) |
| Quick Reference | [QUICK_REFERENCE](QUICK_REFERENCE.md) |

---

## License

This documentation is part of SimpleGo, licensed under AGPL-3.0.

---

*Last updated: February 7, 2026 - Session 22 (Reply Queue Flow Discovery)*
