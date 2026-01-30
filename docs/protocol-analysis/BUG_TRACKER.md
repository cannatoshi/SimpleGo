# Bug Tracker

## Complete Documentation of All 18 Bugs

This document provides detailed documentation of all bugs discovered during SimpleGo development, including the incorrect code, correct code, and root cause analysis.

---

## Summary

| Bug # | Component | Session | Status |
|-------|-----------|---------|--------|
| 1-8 | Wire format | 4 | FIXED |
| 9 | wolfSSL X448 byte order | 5 | FIXED |
| 10-12 | SMPQueueInfo encoding | 6 | FIXED |
| 13-14 | Payload AAD, IV order | 8 | FIXED |
| 15-16 | HSalsa20, A_CRYPTO | 9 | FIXED |
| 17 | cmNonce instead of msgId | 10C | FIXED |
| **18** | **Reply Queue E2E** | **13** | **OPEN** |

---

## Bug #18: Reply Queue E2E Decryption (OPEN)

**Sessions:** 12, 13  
**Component:** Reply Queue Per-Queue E2E Layer  
**Impact:** Cannot decrypt Reply Queue messages from app  
**Status:** OPEN - Extensive analysis, multiple approaches tested

### 18.1 The Problem

Reply Queue messages fail decryption despite correct parsing. App shows "A_CRYPTO" error for our messages, and we cannot decrypt App's messages.

### 18.2 Session 12 Discoveries

**Two Separate X25519 Keypairs:**

| Keypair | Purpose | Used in |
|---------|---------|---------|
| dhKey / privDhKey | Server-level DH (NEW command) | rcvDhSecret |
| e2eDhKey / e2ePrivKey | E2E-level DH (Peer encryption) | SMPQueueAddress |

**Changes Implemented:**
```c
// smp_queue.h - Added E2E keys
uint8_t e2e_public[32];
uint8_t e2e_private[32];

// smp_queue.c - Generate E2E keypair
crypto_box_keypair(our_queue.e2e_public, our_queue.e2e_private);

// smp_queue.c - Send e2e_public in SMPQueueInfo
memcpy(&buf[p], our_queue.e2e_public, 32);
```

### 18.3 Session 13 Discoveries

#### 18.3.1 Parsing Bug Fixed

**Old (Wrong):**
```c
uint8_t maybe_corrId = plain[14];  // Wrong name!
uint8_t maybe_e2e = plain[15];     // This is SPKI length, not a tag!
```

**New (Correct):**
```c
uint8_t maybe_e2e = plain[14];     // '1' = Just, '0'/',' = Nothing
// [15] = 0x2c = 44 = SPKI length
// [16-59] = X25519 SPKI (e2ePubKey!)
// [60-83] = cmNonce
// [84+] = cmEncBody
```

#### 18.3.2 HSalsa20 Difference Discovered

| Step | Haskell | libsodium |
|------|---------|-----------|
| 1 | DH(pub, priv) -> secret | DH(pub, priv) -> secret |
| 2 | XSalsa20(secret, nonce, msg) | **HSalsa20(secret)** -> key |
| 3 | - | XSalsa20(key, nonce, msg) |

**Haskell xSalsa20 (Direct):**
```haskell
cryptoBox secret nonce s = BA.convert tag <> c
  where
    (rs, c) = xSalsa20 secret nonce s  -- Direct!
```

**libsodium (Extra HSalsa20):**
```c
crypto_box_beforenm(k, peer_pub, our_priv);  // HSalsa20 applied here!
crypto_box_open_easy_afternm(..., k);
```

#### 18.3.3 MAC Position Difference Discovered

**Haskell cbDecrypt:**
```haskell
(tag', c) = B.splitAt 16 packet  -- TAG = first 16 bytes!
```

| Format | Layout |
|--------|--------|
| **Haskell** | `[MAC 16 bytes][Ciphertext]` |
| **libsodium** | `[Ciphertext][MAC 16 bytes]` |

### 18.4 All Crypto Tests (Session 13)

| Test | Method | MAC | Private Key | Result |
|------|--------|-----|-------------|--------|
| 1 | crypto_box_open_easy | Auto | e2e_private | FAILED |
| 2 | crypto_box_open_easy | Auto | rcv_dh_private | FAILED |
| 3 | crypto_secretbox_open_easy | None | e2e_private | FAILED |
| 4 | crypto_secretbox_open_easy | Reordered | e2e_private | FAILED |
| 5 | crypto_secretbox_open_detached | Separate | e2e_private | FAILED |

### 18.5 Verified Correct

- Key extraction: [28-59] = Raw X25519 key
- Nonce extraction: [60-83] = 24 bytes
- Body offset: [84+] = cmEncBody
- Keypair verification: e2e_public matches derived from e2e_private
- SPKI header: `30 2a 30 05 06 03 2b 65 6e 03 21 00`

### 18.6 SMPConfirmation Contains e2ePubKey!

**Found in Haskell:**
```haskell
data SMPConfirmation = SMPConfirmation
  { senderKey :: Maybe SndPublicAuthKey,
    e2ePubKey :: C.PublicKeyX25519,        -- THE KEY!
    connInfo :: ConnInfo,
    smpReplyQueues :: [SMPQueueInfo],
    smpClientVersion :: VersionSMPC
  }
```

### 18.7 App's e2ePubKey Flow

```haskell
newSndQueue ... {dhPublicKey = rcvE2ePubDhKey} = do
  (e2ePubKey, e2ePrivKey) <- generateKeyPair
  let sq = SndQueue
        { e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey,  -- Pre-computed!
          e2ePubKey = Just e2ePubKey,                     -- App's key
        }
```

1. App receives our `e2e_public` from SMPQueueInfo
2. App generates its own keypair
3. App **first** message: `e2ePubKey = Just` (sendConfirmation)
4. App **subsequent** messages: `e2ePubKey = Nothing` (sendAgentMessage)

### 18.8 Sub-Issues Status

| Sub-Issue | Description | Status |
|-----------|-------------|--------|
| #18a | Separate E2E Keypair implemented | DONE |
| #18b | E2E public sent in SMPQueueInfo | DONE |
| #18c | Parsing fix (correct offsets) | DONE |
| #18d | HSalsa20 difference identified | DONE |
| #18e | MAC position difference identified | DONE |
| #18f | 5 crypto approaches tested | DONE - All fail |
| #18g | SMPConfirmation contains e2ePubKey | FOUND |
| #18h | Parse SMPConfirmation | TODO |
| #18i | Find where App's key comes from | TODO |

### 18.9 Remaining Hypotheses

1. **H1:** Key at [28-59] is corrId, not e2ePubKey
2. **H2:** Need to parse SMPConfirmation first to get App's key
3. **H3:** maybe_e2e = ',' means direct Double Ratchet
4. **H4:** Subtle XSalsa20 implementation differences

### 18.10 Android vs Desktop Difference

| Aspect | Desktop | Android |
|--------|---------|---------|
| URI extraction | SUCCESS (2090 chars) | FAILED |
| Peer-Connect | YES | NO |
| AgentConfirmation | Sent | Not sent |
| Display | "Connecting..." | No status |
| Padding prefix | `2a fc 5f...` | `09 e7 5f...` |

---

## Bug Discovery Timeline

| Date | Session | Bugs |
|------|---------|------|
| Jan 23, 2026 | S4 | #1-#6 |
| Jan 24, 2026 | S4-S6 | #7-#12 |
| Jan 27, 2026 | S8-S9 | #13-#16 |
| Jan 28, 2026 | S10C | #17 |
| **Jan 30, 2026** | **S12-S13** | **#18 (deep analysis)** |

---

## Bug Categories

```
18 Bugs Total:
- 7x Length Prefix issues
- 3x KDF/IV Order issues
- 1x Byte Order issue (wolfSSL)
- 1x Separator issue
- 1x Maybe encoding issue
- 1x AAD construction issue
- 1x NaCl crypto layer issue
- 1x Header encryption issue
- 1x Nonce source issue (cmNonce)
- 1x E2E crypto compatibility issue (OPEN)
```

---

## Lessons Learned

1. **Length encoding varies by context** - always check Haskell source
2. **Crypto libraries differ** - verify against reference implementations
3. **If it works, don't touch it!** - Session 11 regression
4. **cmNonce != msgId** - Different nonces for different layers
5. **Two keypairs exist** - Server DH vs E2E DH are separate!
6. **HSalsa20 matters** - libsodium adds extra step
7. **MAC position matters** - [MAC][Cipher] vs [Cipher][MAC]
8. **Test all combinations** - Systematic approach required
9. **Parse SMPConfirmation** - Contains App's e2ePubKey

---

*Bug Tracker v7.0*  
*Last updated: January 30, 2026 - Session 13*  
*Total bugs documented: 18 (17 fixed, 1 open)*
