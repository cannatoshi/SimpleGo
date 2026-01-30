# Quick Reference

## Constants, Wire Formats, Crypto Differences

**Updated: 2026-01-30 - Session 13 (E2E Crypto Deep Analysis)**

---

## Current Status

```
Session 13 - E2E Crypto Deep Analysis:
- Fixed: Message parsing with correct offsets
- Discovered: HSalsa20 difference (Haskell vs libsodium)
- Discovered: MAC position [MAC][Cipher] vs [Cipher][MAC]
- Tested: 5 crypto approaches - ALL FAILED
- Found: SMPConfirmation contains e2ePubKey
- Next: Parse SMPConfirmation for App's key
```

---

## Table of Contents

1. [Version Numbers](#1-version-numbers)
2. [Size Constants](#2-size-constants)
3. [Message Structure (Verified)](#3-message-structure-verified)
4. [HSalsa20 Difference](#4-hsalsa20-difference)
5. [MAC Position Difference](#5-mac-position-difference)
6. [Crypto Tests Summary](#6-crypto-tests-summary)
7. [e2ePubKey Flow](#7-e2epubkey-flow)
8. [Working Code State](#8-working-code-state)

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
| Poly1305 MAC | 16 | Authentication tag |
| Payload AAD | **235** | NO prefix! |

---

## 3. Message Structure (Verified Session 13)

### 3.1 ClientRcvMsgBody (after Server-decrypt)

```
=== Full Layout ===
[0-1]    Length prefix (Word16 BE, e.g. 0x3e82 = 16002)
[2-9]    msgTs (SystemTime = Int64 BE, 8 bytes)
[10]     msgFlags (1 byte)
[11]     Space ' ' (0x20)

=== ClientMsgEnvelope starts at offset 12 ===
[12-13]  phVersion (Word16 BE, e.g. 00 04 = v4)
[14]     phE2ePubDhKey Maybe tag:
         - '1' (0x31) = Just (key present!)
         - '0' (0x30) = Nothing
         - ',' (0x2c) = Nothing (alternative)
```

### 3.2 When Maybe = '1' (Just)

```
[15]     SPKI length = 44 (0x2c)
[16-59]  X25519 SPKI (44 bytes)
  [16-27]  SPKI header: 30 2a 30 05 06 03 2b 65 6e 03 21 00
  [28-59]  Raw X25519 key (32 bytes) <- E2E PUBLIC KEY!
[60-83]  cmNonce (24 bytes)
[84+]    cmEncBody (encrypted data)
```

### 3.3 Log Verification

```
3e 82 00 00 00 00 69 7c e2 58 54 20 00 04 31 2c
^len  ^----msgTs (8 bytes)---- ^flg^sp^ver ^1 ^44

30 2a 30 05 06 03 2b 65 6e 03 21 00 42 60 ec a8
^-------SPKI header (12 bytes)------^--raw key--
```

All offsets verified correct!

---

## 4. HSalsa20 Difference (Critical!)

### 4.1 The Problem

| Step | Haskell | libsodium |
|------|---------|-----------|
| 1 | DH(pub, priv) -> secret | DH(pub, priv) -> secret |
| 2 | XSalsa20(secret, nonce) | **HSalsa20(secret)** -> key |
| 3 | - | XSalsa20(key, nonce) |

**libsodium adds an EXTRA HSalsa20 step!**

### 4.2 Haskell Implementation

```haskell
cryptoBox secret nonce s = BA.convert tag <> c
  where
    (rs, c) = xSalsa20 secret nonce s  -- DH secret DIRECT!
    tag = Poly1305.auth rs c
```

### 4.3 libsodium Implementation

```c
// crypto_box_beforenm applies HSalsa20!
crypto_box_beforenm(k, peer_pub, our_priv);  // k = HSalsa20(DH)
crypto_box_open_easy_afternm(..., k);         // Then XSalsa20
```

### 4.4 Double HSalsa20 Problem

**Haskell (Correct):**
```
1. subkey = HSalsa20(dh_secret, nonce[0:16])
2. Salsa20(subkey, nonce[16:24])
```

**libsodium with _beforenm (Wrong):**
```
1. k = HSalsa20(dh_secret, ZERO)        <- Extra!
2. subkey = HSalsa20(k, nonce[0:16])
3. Salsa20(subkey, nonce[16:24])
```

### 4.5 Solution Attempt

Use raw DH without HSalsa20:
```c
uint8_t dh_secret[32];
crypto_scalarmult(dh_secret, our_priv, peer_pub);  // Raw DH
crypto_secretbox_open_easy(plain, cipher, len, nonce, dh_secret);
```

**Result:** Still failed - other issues present.

---

## 5. MAC Position Difference (Critical!)

### 5.1 Haskell cbDecrypt

```haskell
sbDecryptNoPad_ secret nonce packet
  where
    (tag', c) = B.splitAt 16 packet  -- TAG FIRST!
```

### 5.2 Format Comparison

| Format | Layout |
|--------|--------|
| **Haskell** | `[MAC 16 bytes][Ciphertext]` |
| **libsodium** | `[Ciphertext][MAC 16 bytes]` |

### 5.3 Reordering Code

```c
// Haskell: [MAC][Cipher] -> libsodium: [Cipher][MAC]
uint8_t *reordered = malloc(enc_len);
memcpy(reordered, &cipher[16], enc_len - 16);    // Cipher first
memcpy(&reordered[enc_len - 16], cipher, 16);    // MAC last
crypto_secretbox_open_easy(plain, reordered, enc_len, nonce, key);
```

**Result:** Still failed - other issues present.

---

## 6. Crypto Tests Summary (Session 13)

### 6.1 All Tests

| # | Method | MAC | Key | Result |
|---|--------|-----|-----|--------|
| 1 | crypto_box_open_easy | Auto | e2e_private | FAILED |
| 2 | crypto_box_open_easy | Auto | rcv_dh_private | FAILED |
| 3 | crypto_secretbox_open_easy | Direct | e2e_private | FAILED |
| 4 | crypto_secretbox_open_easy | Reordered | e2e_private | FAILED |
| 5 | crypto_secretbox_open_detached | Separate | e2e_private | FAILED |

### 6.2 Test Data (from logs)

```
e2ePubKey:     88159398... (from [28-59])
our_e2e_priv:  f3944334... (verified)
cmNonce:       59c05b9e... (24 bytes)
DH secret:     dea3d892...
MAC:           143b0d95... (16 bytes)
Ciphertext:    16006 bytes
```

---

## 7. e2ePubKey Flow

### 7.1 SMPConfirmation Contains Key!

```haskell
data SMPConfirmation = SMPConfirmation
  { senderKey :: Maybe SndPublicAuthKey,
    e2ePubKey :: C.PublicKeyX25519,      -- THE KEY!
    connInfo :: ConnInfo,
    smpReplyQueues :: [SMPQueueInfo],
    smpClientVersion :: VersionSMPC
  }
```

### 7.2 App's Key Generation

```haskell
newSndQueue ... {dhPublicKey = rcvE2ePubDhKey} = do
  (e2ePubKey, e2ePrivKey) <- generateKeyPair
  let sq = SndQueue
        { e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey,
          e2ePubKey = Just e2ePubKey,
        }
```

### 7.3 Key Transmission

| Message # | e2ePubKey | Meaning |
|-----------|-----------|---------|
| First (Confirmation) | Just key | Key in header |
| Subsequent | Nothing | Pre-computed secret |

**Problem:** Reply Queue uses subsequent message format!

---

## 8. Working Code State

### 8.1 smp_ratchet.c (DO NOT CHANGE!)

```c
#define RATCHET_VERSION         2
uint8_t em_header[123];         // 123 bytes!
em_header[hp++] = 0x58;         // ehBody-len = 88 (1 BYTE!)
output[p++] = 0x7B;             // emHeader len = 123
```

### 8.2 smp_queue.h (Session 12)

```c
typedef struct {
    uint8_t rcv_dh_public[32];    // Server DH
    uint8_t rcv_dh_private[32];
    
    uint8_t e2e_public[32];       // E2E DH (separate!)
    uint8_t e2e_private[32];
    
    uint8_t shared_secret[32];
    // ...
} our_queue_t;
```

### 8.3 Correct Parsing (Session 13)

```c
// After server-decrypt, ClientMsgEnvelope at offset 12
int offset = 14;
uint8_t maybe_e2e = plain[offset];  // '1' = Just

if (maybe_e2e == '1') {
    // SPKI at offset 16-59 (44 bytes)
    // Raw key at offset 28-59 (32 bytes)
    memcpy(peer_e2e_public, &plain[28], 32);
    
    // Nonce at offset 60-83 (24 bytes)
    memcpy(cm_nonce, &plain[60], 24);
    
    // Encrypted body at offset 84+
    int enc_len = total_len - 84;
}
```

---

## 9. Open Questions

1. **Is key at [28-59] really e2ePubKey or corrId?**
2. **Do we need to parse SMPConfirmation first?**
3. **When maybe_e2e = ',' - is it direct Double Ratchet?**
4. **Why do Android and Desktop apps behave differently?**

---

## 10. Next Steps

1. Parse SMPConfirmation to extract App's e2ePubKey
2. Try Double Ratchet receiver (skip per-queue E2E)
3. Python verification with exact log values
4. Analyze Android parsing failure

---

*Quick Reference v7.0*  
*Last updated: January 30, 2026 - Session 13*
