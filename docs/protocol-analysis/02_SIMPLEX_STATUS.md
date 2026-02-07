# SimpleGo - Current Status (2026-02-07)

**Project:** Native SimpleX SMP Client for ESP32  
**Version:** v0.1.17-alpha  
**Archive:** See `01_SIMPLEX_PROTOCOL_INDEX.md` for complete documentation (390+ sections, 19 parts)

---

## 🎯 CURRENT STATUS: Reply Queue Flow Discovered!

```
═══════════════════════════════════════════════════════════════════

🎯 SESSION 22 DISCOVERY (2026-02-07)
├── ✅ Complete receive chain: TLS → Ratchet → JSON Profile
├── ✅ 31 bugs found and fixed
├── ✅ 83 lessons learned
├── 🔍 BREAKTHROUGH: Modern SimpleX needs NO HELLO!
├── 🔍 App expects AgentConnInfo on Reply Queue
└── ❌ Missing: Reply Queue flow for "Connected"

═══════════════════════════════════════════════════════════════════
```

---

## 📊 Connection Flow

```
RECEIVE (ALL WORKING ✅):
ESP32 ◄── TLS 1.3 ◄── SMP Server ◄── SimpleX App
          ✅           ✅              ✅

SEND (MISSING ❌):
ESP32 ──► Reply Queue Server ──► SimpleX App ──► CON ──► "Connected"
          ❌ Not connected      ❌                ❌
```

---

## ✅ What Works (Receive Chain)

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
| 8 | **Peer Profile JSON** | ✅ | S20 |

**Result:** `"displayName": "cannatoshi"` on ESP32! 🎉

---

## ❌ What's Missing (Reply Queue Flow)

| Step | Component | Status | Priority |
|------|-----------|--------|----------|
| 9b | Parse Reply Queue Info from Tag 'D' | ❌ | ★★★★★ |
| 9c | Second TLS to Reply Queue Server | ❌ | ★★★★★ |
| 9d | SMP Handshake on Reply Queue | ❌ | ★★★★★ |
| 9e | SKEY on Reply Queue | ❌ | ★★★★★ |
| 9f | AgentConnInfo on Reply Queue | ❌ | ★★★★★ |
| 10 | App receives → CON | ❌ | Result |
| 11 | **"Connected" status** | ❌ | **GOAL** |

---

## 🔍 Session 22 Key Discoveries

### Discovery 1: Modern Protocol Flow
```
Modern SimpleX (v2 + senderCanSecure = True):
  ✗ Does NOT need HELLO on Contact Queue
  ✓ Expects AgentConnInfo on Reply Queue
  ✓ Reply Queue Info in Tag 'D' AgentConnInfoReply
```

### Discovery 2: Post-Quantum KEM
```
SimpleX uses SNTRUP761 (not Kyber1024):
  - Public Key:    1158 bytes
  - Ciphertext:    1039 bytes
  - Shared Secret: 32 bytes
  
PQ-Graceful-Degradation:
  v3 + KEM Nothing → pure DH fallback (no error)
```

### Discovery 3: Dynamic Header Sizes
```
v2:     123 bytes (1-byte prefix)
v3:     124 bytes (2-byte prefix)
v3+PQ:  ~2346 bytes (with SNTRUP761)

→ All offset calculations must be dynamic!
```

---

## 📊 Session 22 Fixes (5 Bugs)

| Bug | Component | Fix |
|-----|-----------|-----|
| #27 | E2E version_min | 2 → 3 + KEM Nothing (App breaks silence!) |
| #28 | KEM Parser | Dynamic for SNTRUP761 (up to 2346 bytes) |
| #29 | Body Decrypt Pointer | Dynamic emHeader size calculation |
| #30 | HKs/NHKs Init + Promotion | Three-part header key chain fix |
| #31 | Header Decrypt Try-Order | HKr (SameRatchet), NHKr (AdvanceRatchet) |

---

## 📋 Complete Bug Summary (31 Bugs - ALL FIXED!)

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
| **S22** | **#27-31** | **E2E v3, KEM parser, NHK promotion** |

---

## 📐 Quick Reference - Constants (Updated S22)

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

// SNTRUP761 Post-Quantum KEM (S22)
#define SNTRUP761_PUBKEY_SIZE       1158
#define SNTRUP761_CIPHERTEXT_SIZE   1039
#define SNTRUP761_SECRET_SIZE       32
```

---

## 📐 Quick Reference - 4 Header Keys (S21-22)

```
Key    Full Name              Usage
────────────────────────────────────────────────────────────
HKs    header_key_send        Current: encrypt our outgoing headers
NHKs   next_header_key_send   Next: becomes HKs after our DH ratchet
HKr    header_key_recv        Current: decrypt incoming headers
NHKr   next_header_key_recv   Next: becomes HKr after peer's DH ratchet

Initial from X3DH:
  HKs  = hk  (HKDF[0-31])   — first send
  NHKr = nhk (HKDF[32-63])  — promotes to HKr on first recv

Promotion (AdvanceRatchet):
  Recv: HKr ← NHKr, then rootKdf → new NHKr
  Send: HKs ← NHKs, then rootKdf → new NHKs

Header Decrypt Try-Order (S22 Bug #31):
  1. Try HKr (SameRatchet)
  2. Try NHKr (AdvanceRatchet) — if success, promote!
```

---

## 📐 Quick Reference - SMPQueueInfo Wire Format (S22)

```
Reply Queue Info location: Tag 'D' AgentConnInfoReply (innermost layer)

[1B count] [SMPQueueInfo:]
  [2B clientVersion] 
  [SMPServer:]
    [1B host count] [1B+N hostname] [space] [port] [1B+N keyHash]
  [1B+N senderId] 
  [1B+44 DH X25519 SPKI] 
  [1B QueueMode 'M']
```

---

## 📐 Quick Reference - KEM Maybe Encoding (S22)

```
KEM in MsgHeader v3:
  Nothing  → '0' (0x30) — No PQ KEM active
  Just Proposed → '1' + 'P' + [2B len] + pubkey_data (1158B)
  Just Accepted → '1' + 'A' + [2B len] + ciphertext_data (1039B)
```

---

## 📝 Key Learnings (83 Total)

### Latest (S22):
- Modern SimpleX (v2 + senderCanSecure) needs NO HELLO
- AgentConnInfo on Reply Queue, not HELLO on Contact Queue
- smpReplyQueues in Tag 'D' AgentConnInfoReply
- SNTRUP761 for PQ KEM (not Kyber1024)
- version_min MUST match RATCHET_VERSION
- KEM Parser must be dynamic (up to 2346 bytes)
- NHKs→HKs promotion chain (two-step, not direct)
- Header decrypt try-order: HKr first, NHKr second

### Classic:
- Wire Format ≠ Crypto Format
- Haskell `largeP` removes length prefix
- SimpleX uses NON-STANDARD XSalsa20 (zeros, not nonce)
- Tests must NEVER modify production state
- One line can block weeks of progress

---

## 📁 Documentation Files

| File | Description |
|------|-------------|
| `01_SIMPLEX_PROTOCOL_INDEX.md` | Navigation index |
| `02_SIMPLEX_STATUS.md` | This file - quick status |
| `README.md` | Project overview |
| `BUG_TRACKER.md` | All 31 bugs, 83 lessons |
| `QUICK_REFERENCE.md` | Constants, wire formats |
| `03-21_PART*.md` | Sessions 1-22 documentation |

---

## 🎯 Next Steps (Session 23)

1. **Parse Reply Queue Info** from Tag 'D'
2. **Second TLS Connection** to Reply Queue server
3. **SMP Handshake** on Reply Queue
4. **SKEY Command** on Reply Queue
5. **AgentConnInfo** (our profile) on Reply Queue
6. **App receives CON** → "Connected" 🎉

---

*Status updated: 2026-02-07 Session 22 — Reply Queue Flow Discovered*
