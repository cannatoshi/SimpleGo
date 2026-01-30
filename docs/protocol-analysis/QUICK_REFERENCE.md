# Quick Reference

## Constants, Wire Formats, and KDF Parameters

**Updated: 2026-01-30 - Session 12 (E2E Keypair Analysis)**

---

## Current Status

```
Session 12 - E2E Keypair Analysis:
- Discovered: Haskell uses TWO separate X25519 keypairs
- Implemented: e2e_public/e2e_private in our_queue_t
- Problem: App sends phE2ePubDhKey = Nothing
- App pre-computes e2eDhSecret, never sends key to us!
```

---

## Table of Contents

1. [Version Numbers](#1-version-numbers)
2. [Size Constants](#2-size-constants)
3. [Two Keypair Architecture](#3-two-keypair-architecture)
4. [ClientMsgEnvelope Structure](#4-clientmsgenvelope-structure)
5. [Maybe Encoding](#5-maybe-encoding)
6. [Working Code State](#6-working-code-state)

---

## 1. Version Numbers (VERIFIED)

| Protocol | Our Value | Hex |
|----------|-----------|-----|
| SMP Client | 4 | 0x00 0x04 |
| Agent | 7 | 0x00 0x07 |
| E2E | 2 | 0x00 0x02 |
| RATCHET_VERSION | **2** | **DO NOT CHANGE!** |

---

## 2. Size Constants (VERIFIED)

| Structure | Size | Notes |
|-----------|------|-------|
| EncMessageHeader | **123** | NOT 124! |
| MsgHeader | 88 | With padding |
| X448 SPKI | 68 | 12 header + 56 raw |
| X25519 SPKI | 44 | 12 header + 32 raw |
| cmNonce | 24 | In ClientMsgEnvelope |
| Payload AAD | **235** | NO prefix! |

---

## 3. Two Keypair Architecture (Session 12 Discovery)

### 3.1 Haskell Uses TWO Separate Keypairs!

| Keypair | Purpose | Used For |
|---------|---------|----------|
| **Server DH** | NEW command, server-level encrypt | rcvDhSecret, shared_secret |
| **E2E DH** | Peer-to-peer encrypt | e2eDhSecret, SMPQueueAddress |

### 3.2 our_queue_t Structure (Updated)

```c
typedef struct {
    // Server-level DH (for NEW command)
    uint8_t rcv_dh_public[32];
    uint8_t rcv_dh_private[32];
    
    // E2E-level DH (for peer encryption) - NEW!
    uint8_t e2e_public[32];
    uint8_t e2e_private[32];
    
    // Server-level shared secret
    uint8_t shared_secret[32];
    // ...
} our_queue_t;
```

### 3.3 Key Usage

```
Server Level (shared_secret):
  DH(srv_dh_public, rcv_dh_private) = shared_secret
  Used for: Server MSG decrypt (XSalsa20-Poly1305)

E2E Level (e2eDhSecret):
  DH(peer_e2e_public, our_e2e_private) = e2eDhSecret
  Used for: Per-queue E2E decrypt (crypto_box)
  
  PROBLEM: App doesn't send peer_e2e_public!
```

### 3.4 The E2E Problem

**What App Does:**
```haskell
-- App receives our e2e_public from SMPQueueInfo
-- App generates its own keypair
(e2ePubKey, e2ePrivKey) <- generateKeyPair
-- App pre-computes shared secret
e2eDhSecret = DH(our_e2e_public, app_e2e_private)
-- App NEVER sends e2ePubKey to us!
```

**What We Need:**
```c
// We need app's e2e_public to compute:
e2eDhSecret = DH(app_e2e_public, our_e2e_private)
// But app sends phE2ePubDhKey = Nothing!
```

---

## 4. ClientMsgEnvelope Structure

### 4.1 Full Layout

```
Offset  Size  Content
------  ----  -------
[0-1]   2     length prefix (16002 = 0x3e82)
[2-5]   4     unknown (00 00 00 00)
[6-9]   4     timestamp
[10-13] 4     unknown
[14]    1     maybe_corrId ('1' = Just, '0' = Nothing)
[15]    1     maybe_e2e ('1' = Just, ',' = Nothing) <- PROBLEM!
[16-59] 44    corrId X25519 SPKI (if maybe_corrId = '1')
[60-83] 24    cmNonce
[84+]   var   cmEncBody
```

### 4.2 The Nothing Problem

```
[15] = ',' (0x2c) means phE2ePubDhKey = Nothing

When maybe_e2e = ',':
  - No e2e_public key in message
  - App already pre-computed e2eDhSecret
  - We cannot compute the same secret!
```

---

## 5. Maybe Encoding (CRITICAL!)

### 5.1 Standard Maybe - ASCII!

```c
// CORRECT:
'1' (0x31) = Just (has value)
'0' (0x30) = Nothing (no value)
',' (0x2c) = Nothing (alternative encoding!)

// WRONG:
0x01 = Binary 1 - FAILS!
```

### 5.2 When ',' Appears

In ClientMsgEnvelope, `maybe_e2e = ','` means:
- phE2ePubDhKey = Nothing
- App didn't include its E2E public key
- E2E secret was pre-computed on app side

---

## 6. Working Code State (Session 11/12)

### 6.1 smp_ratchet.c (DO NOT CHANGE!)
```c
#define RATCHET_VERSION         2
uint8_t em_header[123];         // 123 bytes!
em_header[hp++] = 0x58;         // ehBody-len = 88 (1 BYTE!)
output[p++] = 0x7B;             // emHeader len = 123
```

### 6.2 smp_queue.c (Updated Session 12)
```c
// Generate BOTH keypairs
crypto_box_keypair(our_queue.rcv_dh_public, our_queue.rcv_dh_private);
crypto_box_keypair(our_queue.e2e_public, our_queue.e2e_private);

// Send e2e_public in SMPQueueInfo (not rcv_dh_public!)
memcpy(&buf[p], our_queue.e2e_public, 32);
```

### 6.3 main.c (cmNonce Fix - Session 10C)
```c
// Extract cmNonce from ClientMsgEnvelope
int cm_nonce_offset = spki_offset + 44;  // [60-83]
memcpy(cm_nonce, &server_plain[cm_nonce_offset], 24);
```

---

## Session 12 Open Questions

1. **Where does app's E2E public key come from?**
   - Not in message (phE2ePubDhKey = Nothing)
   - In AgentConfirmation?
   - Derived from X3DH?

2. **Is E2E derived from X3DH?**
   - X3DH produces root_key, header_key
   - Maybe e2e_key is also derived?

3. **Queue Mode difference?**
   - QMMessaging vs QMContact
   - Different E2E behavior?

---

*Quick Reference v6.0*  
*Last updated: January 30, 2026 - Session 12*
