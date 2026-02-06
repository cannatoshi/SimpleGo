# Ratchet Decrypt Flow Analysis — Auftrag 4b

**Date:** 2026-02-06
**Commit base:** `7cb6ba1 feat(ratchet): activate state update after body decrypt`
**Files:** `main/smp_ratchet.c`, `main/include/smp_ratchet.h`, `main/main.c`

---

## 1. Header Decrypt Flow (smp_ratchet.c:580-620 + main.c Phase 2a)

### 1.1 Which keys are tried?

**In `ratchet_decrypt()` (smp_ratchet.c:603-620) — 2 keys:**

| Try | Key | AAD | Purpose |
|-----|-----|-----|---------|
| 1 | `header_key_recv` | `assoc_data` (112B) | Normal receive key |
| 2 | `header_key_send` | `assoc_data` (112B) | Fallback with send key |

**In `main.c` Phase 2a (lines ~1092-1252) — 8 attempts:**

| Try | Key | AAD | Notes |
|-----|-----|-----|-------|
| 1 | `header_key_recv` | normal rcAD | Expected key for receiving |
| 2 | `header_key_send` | normal rcAD | hk from X3DH |
| 3 | `header_key_recv` | swapped rcAD | With rcAD byte-order swapped |
| 4 | `header_key_send` | swapped rcAD | With rcAD byte-order swapped |
| 5 | `saved_nhk` | normal rcAD | Original X3DH nhk (pre-ratchet_init_sender) |
| 6 | `saved_hk` | normal rcAD | Original X3DH hk (pre-ratchet_init_sender) |
| 7 | `saved_nhk` | swapped rcAD | |
| 8 | `saved_hk` | swapped rcAD | |

### 1.2 Is the result (which key worked) stored/returned?

**NO.** This is a **critical missing piece**.

- `main.c` sets a `header_decrypted` boolean (true/false), but does NOT record WHICH key succeeded.
- `ratchet_decrypt_body()` receives only the parsed MsgHeader fields (`peer_new_pub`, `msg_pn`, `msg_ns`) — no information about which key decrypted the header.
- In Haskell, the decrypt function returns a discriminated result:
  - `decryptCurrHeader` succeeds → **SameRatchet** (no DH advance)
  - `decryptNextHeader` succeeds → **AdvanceRatchet** (full DH ratchet step)
- **Our code ALWAYS does AdvanceRatchet** regardless of which key worked.

### 1.3 Bug Impact

For the FIRST incoming message, this doesn't matter (there's no "current" ratchet yet, so AdvanceRatchet is always correct). But once we have an established receive ratchet, a second message from the same ratchet epoch (same DH key) will incorrectly trigger a new DH ratchet step — corrupting state.

---

## 2. `next_header_key_recv` Field — Does it exist?

### Current `ratchet_state_t` (smp_ratchet.h:29-60):

```c
typedef struct {
    uint8_t root_key[32];
    uint8_t header_key_send[32];     // ← conflates HKs and NHKs
    uint8_t header_key_recv[32];     // ← conflates HKr and NHKr
    uint8_t chain_key_send[32];
    uint8_t chain_key_recv[32];
    x448_keypair_t dh_self;
    uint8_t dh_peer[56];
    uint32_t msg_num_send;
    uint32_t msg_num_recv;
    uint32_t prev_chain_len;
    bool initialized;
    uint8_t assoc_data[112];
} ratchet_state_t;
```

### **`next_header_key_recv` does NOT exist. Neither does `next_header_key_send`.**

### Haskell has 4 separate header key slots:

| Haskell Field | Location | Purpose |
|---------------|----------|---------|
| `rcHKr` | Inside `RcvRatchet` (rcRcv) | CURRENT header key for receiving (this ratchet epoch) |
| `rcNHKr` | Top-level Ratchet | NEXT header key for receiving (triggers AdvanceRatchet) |
| `rcHKs` | Inside `SndRatchet` (rcSnd) | CURRENT header key for sending |
| `rcNHKs` | Top-level Ratchet | NEXT header key for sending |

### Our code has only 2 slots:

| Our Field | Maps to | Problem |
|-----------|---------|---------|
| `header_key_recv` | `rcHKr` AND `rcNHKr` conflated | Can't distinguish SameRatchet vs AdvanceRatchet |
| `header_key_send` | `rcHKs` AND `rcNHKs` conflated | After advance, loses the current sending key |

### **Fix Required: Add 2 new fields**

```c
uint8_t next_header_key_recv[RATCHET_KEY_LEN];  // = Haskell rcNHKr
uint8_t next_header_key_send[RATCHET_KEY_LEN];  // = Haskell rcNHKs
```

After this, the existing fields map cleanly:
- `header_key_recv` = `rcHKr` (current, used for SameRatchet decrypt)
- `next_header_key_recv` = `rcNHKr` (next, used for AdvanceRatchet decrypt)
- `header_key_send` = `rcHKs` (current, used for encrypt)
- `next_header_key_send` = `rcNHKs` (next, promoted on advance)

---

## 3. State Update Mapping — Our Code vs. Haskell

### 3.1 Initial State After X3DH (BEFORE first message)

| Field | Our Code | Haskell (Responder) | Match? |
|-------|----------|-------------------|--------|
| `root_key` | `kdf_output[64-95]` = sk | `rcRK = sk` | ✅ |
| `header_key_send` | `kdf_output[0-31]` = hk | `rcNHKs = hk` (responder swaps) | ✅ name mismatch (ours is NHKs, not HKs) |
| `header_key_recv` | `kdf_output[32-63]` = nhk | `rcNHKr = nhk` (responder swaps) | ✅ name mismatch (ours is NHKr, not HKr) |
| `next_header_key_recv` | **MISSING** | `rcNHKr` (separate field) | ❌ N/A |
| `next_header_key_send` | **MISSING** | `rcNHKs` (separate field) | ❌ N/A |
| `chain_key_recv` | `0` (not set) | `rcRcv = Nothing` | ✅ |
| `chain_key_send` | From `ratchet_init_sender` | Established later | ✅ |
| `dh_self` | `our_key2` (from ratchet_init) | `rcDHRs` | ✅ |
| `dh_peer` | `peer_dh_public` | set later | ✅ |
| `assoc_data` | `our_key1 \|\| peer_key1` | `sk1 \|\| rk1` | ⚠️ order under review |

### 3.2 State Update in `ratchet_decrypt_body()` (lines 980-991) vs. Haskell `advanceRatchet`

**Context:** After rootKdf #1 (recv) and rootKdf #2 (send):

| Field | Our Code (line) | Our Value | Haskell | Correct? | Notes |
|-------|----------------|-----------|---------|----------|-------|
| `root_key` | 981 | `new_root_key_2` | `rcRK''` (after 2× rootKdf) | ✅ | |
| `chain_key_recv` | 982 | `next_chain_key` (after chainKdf) | `rcCKr` (BEFORE chainKdf!) | ❌ | See §3.3 |
| `chain_key_send` | 983 | `send_chain_key` (from rootKdf #2) | `rcCKs` (from rootKdf #2) | ✅ | |
| `header_key_recv` | 984 | `new_nhk_recv` (rootKdf #1 output) | `rcHKr = OLD rcNHKr` | ❌ | Should be old NHKr, not new NHK |
| `header_key_send` | 985 | `new_nhk_send` (rootKdf #2 output) | `rcHKs = OLD rcNHKs` | ❌ | Should be old NHKs, not new NHK |
| `next_header_key_recv` | N/A (missing) | — | `rcNHKr = nhk from rootKdf #1` | ❌ | Field doesn't exist |
| `next_header_key_send` | N/A (missing) | — | `rcNHKs = nhk from rootKdf #2` | ❌ | Field doesn't exist |
| `dh_self` | 986-987 | `new_dh_self` (fresh keypair) | `rcDHRs = new keypair` | ✅ | |
| `dh_peer` | 988 | `peer_new_pub` | `rcDHRr = peer pub` | ✅ | |
| `msg_num_recv` | 989 | `msg_ns + 1` | `rcNr = 0` (reset) then advanced | ⚠️ | See §3.4 |
| `msg_num_send` | 991 | `0` | `rcNs = 0` (reset) | ✅ | |
| `prev_chain_len` | 990 | `msg_num_send` (old) | `rcPN = old rcNs` | ✅ | |

### 3.3 Bug: `chain_key_recv` — Pre-consumed

**Our code (lines 852-865):**
```c
// SCHRITT 3: Chain KDF
memcpy(temp_ck, recv_chain_key, 32);         // recv_chain_key from rootKdf #1
for (i = 0; i < msg_ns; i++) { skip... }     // Skip ahead
kdf_chain(temp_ck, next_chain_key, ...);      // Consume one more for this message
// ...
memcpy(ratchet_state.chain_key_recv, next_chain_key, 32);  // Line 982: AFTER chainKdf
```

**Haskell:**
```haskell
-- advanceRatchet stores the INITIAL chain key
rcRcv = RcvRatchet { rcCKr = ck_from_rootKdf, ... }
-- Then decryptSkipped + decryptMessage consume from it separately
```

**Impact:** Our `chain_key_recv` is already advanced past this message. For the first message after a DH ratchet (msg_ns=0), this means `chain_key_recv` is set to the key for message #1 — which is correct behavior for the next message. **This is actually OK** because we increment `msg_num_recv` to `msg_ns + 1`, so the next decrypt will start from the right position. However, it deviates from Haskell's architecture where the chain key and message counter are tracked together more cleanly.

### 3.4 `msg_num_recv` Assignment

Our code sets `msg_num_recv = msg_ns + 1`. In Haskell, after `advanceRatchet`:
- `rcNr` is reset to 0
- Then `decryptSkipped` advances it for skipped messages
- Then `decryptMessage` consumes one more

Since our code processes the message inline (skip loop + one chainKdf), setting `msg_num_recv = msg_ns + 1` is functionally equivalent. **This is OK.**

---

## 4. Detailed Bug List

### Bug A: Missing NHK fields (CRITICAL)

**Problem:** `ratchet_state_t` has no `next_header_key_recv` / `next_header_key_send`.

**Effect:** After the first AdvanceRatchet, we can't distinguish:
- "Same ratchet" messages (decrypt with current HKr, no DH step)
- "New ratchet" messages (decrypt with NHKr, full DH step)

**Fix:** Add two fields to `ratchet_state_t`.

### Bug B: header_key_recv gets wrong value (CRITICAL)

**Problem (line 984):** `header_key_recv = new_nhk_recv` (rootKdf output)

**Should be:** `header_key_recv = OLD next_header_key_recv` (the key that just successfully decrypted the header becomes the "current" key for this epoch)

**Same issue for header_key_send (line 985).**

### Bug C: No SameRatchet path (HIGH)

**Problem:** `ratchet_decrypt_body()` ALWAYS does a full DH ratchet step (2× rootKdf + new keypair). There's no "SameRatchet" path for when the peer sends multiple messages in the same epoch.

**Effect:** Second message from the same ratchet epoch will fail to decrypt because:
1. We generate a new DH keypair unnecessarily
2. Root key advances incorrectly
3. Chain key is overwritten

### Bug D: Header decrypt result not propagated (MEDIUM)

**Problem:** `main.c` tries 8 key/AD combinations but doesn't record which succeeded. `ratchet_decrypt_body()` can't determine if it should do SameRatchet or AdvanceRatchet.

**Fix:** Return an enum/int indicating which key worked, or pass a flag to `ratchet_decrypt_body()`.

---

## 5. Plan: Split `ratchet_decrypt_body()` into SameRatchet / AdvanceRatchet

### 5.1 New API Design

```c
// Header decrypt result — pass from main.c to ratchet layer
typedef enum {
    RATCHET_SAME,       // Decrypted with current HKr → no DH advance
    RATCHET_ADVANCE     // Decrypted with NHKr → full DH ratchet step
} ratchet_decrypt_mode_t;

int ratchet_decrypt_body(ratchet_decrypt_mode_t mode,   // NEW parameter
                         const uint8_t *peer_new_pub,
                         uint32_t msg_pn, uint32_t msg_ns,
                         const uint8_t *em_header_raw, size_t em_header_len,
                         const uint8_t *em_auth_tag,
                         const uint8_t *em_body, size_t em_body_len,
                         uint8_t *plaintext, size_t *pt_len);
```

### 5.2 Header Decrypt in main.c — Determine Mode

```
Phase 2a (main.c):
  1. Try header_key_recv (= HKr) → if success: mode = RATCHET_SAME
  2. Try next_header_key_recv (= NHKr) → if success: mode = RATCHET_ADVANCE
  3. (Fallback attempts with swapped rcAD etc.)
  4. Pass mode to ratchet_decrypt_body()
```

### 5.3 SameRatchet Path (mode == RATCHET_SAME)

```
ratchet_decrypt_body(RATCHET_SAME, ...):
  1. NO DH ratchet step
  2. NO new keypair generation
  3. Use existing chain_key_recv
  4. Skip forward: for i in [msg_num_recv .. msg_ns-1]: chainKdf(skip)
  5. chainKdf(temp_ck) → message_key, iv_body
  6. AES-GCM decrypt body
  7. unPad
  8. State update:
     - chain_key_recv = next_chain_key
     - msg_num_recv = msg_ns + 1
     (Nothing else changes)
```

### 5.4 AdvanceRatchet Path (mode == RATCHET_ADVANCE)

```
ratchet_decrypt_body(RATCHET_ADVANCE, ...):
  1. Save skipped message keys from OLD chain (msg_num_recv .. msg_pn)  [future]
  2. DH Ratchet — Receiving Half:
     - DH: peer_new_pub × dh_self.priv → dh_recv
     - rootKdf(root_key, dh_recv) → new_rk_1, recv_ck, nhk_recv_new
  3. DH Ratchet — Sending Half:
     - Generate new DH keypair
     - DH: peer_new_pub × new_priv → dh_send
     - rootKdf(new_rk_1, dh_send) → new_rk_2, send_ck, nhk_send_new
  4. Chain KDF for this message:
     - Skip forward for msg_ns
     - chainKdf → message_key, iv_body
  5. AES-GCM decrypt body
  6. unPad
  7. State update:
     - root_key = new_rk_2
     - header_key_recv = OLD next_header_key_recv   ← NOT rootKdf output!
     - next_header_key_recv = nhk_recv_new           ← rootKdf #1 output
     - header_key_send = OLD next_header_key_send   ← NOT rootKdf output!
     - next_header_key_send = nhk_send_new           ← rootKdf #2 output
     - chain_key_recv = next_chain_key (after chainKdf)
     - chain_key_send = send_ck
     - dh_self = new keypair
     - dh_peer = peer_new_pub
     - prev_chain_len = old msg_num_send
     - msg_num_send = 0
     - msg_num_recv = msg_ns + 1
```

### 5.5 Implementation Order

1. **Step 1:** Add `next_header_key_recv` and `next_header_key_send` to `ratchet_state_t`
2. **Step 2:** Update `ratchet_x3dh_sender()` to populate the new fields correctly:
   - In initial state (before first message), we only have NHK fields:
     - `next_header_key_recv = nhk` (bytes 32-63 for responder)
     - `next_header_key_send = hk` (bytes 0-31 for responder)
     - `header_key_recv = 0` (no current receive ratchet yet)
     - `header_key_send = 0` (no current send ratchet yet)
3. **Step 3:** Update `main.c` Phase 2a to try keys in correct order and pass mode
4. **Step 4:** Split `ratchet_decrypt_body()` into two paths
5. **Step 5:** Fix state update assignments (Bug B)
6. **Step 6:** Test with first message (AdvanceRatchet) — should still work
7. **Step 7:** Test with second message from same epoch (SameRatchet) — new capability

### 5.6 State Diagram

```
                    ┌─────────────────────────────┐
                    │     Initial (after X3DH)     │
                    │                              │
                    │  NHKr = nhk                  │
                    │  NHKs = hk                   │
                    │  HKr  = ∅ (no rcRcv)        │
                    │  HKs  = ∅ (no rcSnd)        │
                    │  rcRcv = Nothing             │
                    │  rcSnd = Nothing             │
                    └──────────┬──────────────────┘
                               │
                    1st message arrives
                    Header decrypts with NHKr
                    → mode = RATCHET_ADVANCE
                               │
                    ┌──────────▼──────────────────┐
                    │  After 1st AdvanceRatchet    │
                    │                              │
                    │  HKr  = OLD NHKr             │
                    │  NHKr = rootKdf#1 output     │
                    │  HKs  = OLD NHKs             │
                    │  NHKs = rootKdf#2 output     │
                    │  CKr  = from rootKdf#1       │
                    │  CKs  = from rootKdf#2       │
                    │  rcRcv = established         │
                    │  rcSnd = established         │
                    └──┬───────────┬──────────────┘
                       │           │
          2nd msg      │           │  msg with new DH
          same DH      │           │
                       │           │
              ┌────────▼──┐  ┌─────▼──────────────┐
              │SameRatchet│  │   AdvanceRatchet    │
              │           │  │                     │
              │ Use HKr   │  │ Use NHKr            │
              │ Use CKr   │  │ 2× rootKdf          │
              │ No new DH │  │ New keypair          │
              │           │  │ Promote NHK→HK      │
              └───────────┘  └─────────────────────┘
```

---

## 6. Summary of Required Changes

| Priority | Change | Files |
|----------|--------|-------|
| **P0** | Add `next_header_key_recv[32]` and `next_header_key_send[32]` to `ratchet_state_t` | smp_ratchet.h |
| **P0** | Fix X3DH init: set NHK fields, leave HK fields zeroed | smp_ratchet.c |
| **P0** | Fix state update: `HKr = OLD NHKr`, not rootKdf output | smp_ratchet.c |
| **P1** | Add `ratchet_decrypt_mode_t` enum, pass mode from main.c | smp_ratchet.h, main.c |
| **P1** | Split `ratchet_decrypt_body()` into SameRatchet/AdvanceRatchet | smp_ratchet.c |
| **P1** | Update main.c header decrypt to try HKr first, then NHKr, record which | main.c |
| **P2** | Add skipped message key storage (for out-of-order messages) | smp_ratchet.h/c |
