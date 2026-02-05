# Quick Reference

## Constants, Wire Formats, Verified Values

**Updated: 2026-02-04 - Session 17 (Key Consistency Debug)**

---

## Current Status

```
SESSION 17 - KEY CONSISTENCY INVESTIGATION
==========================================

Evgeny ALREADY ANSWERED (Jan 28, 2026):
  - Key in "confirmation header" (SPKI in message header)
  - "outside of AgentConnInfoReply but in the same message"
  - TWO crypto_box layers with different keys/nonces

New Discoveries:
  - Reply Queue: 2-byte length prefix (Contact Queue: none)
  - cmNonce: RANDOM (directly in message, not calculated)
  - Both keypairs at queue creation, NEVER changed
  - Key mismatch in logs (under investigation)

Debug test pending: e2e_private consistency
```

---

## Table of Contents

1. [Version Numbers](#1-version-numbers)
2. [Size Constants](#2-size-constants)
3. [Message Structure (Verified)](#3-message-structure-verified)
4. [Verified Test Data (Session 14)](#4-verified-test-data-session-14)
5. [Crypto Functions](#5-crypto-functions)
6. [Working Code State](#6-working-code-state)
7. [Message Flow](#7-message-flow)

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

## 3a. Maybe Encoding (Session 15 Discovery)

### 3a.1 Maybe Tags

```
'0' (0x30) = Nothing (no value)
'1' (0x31) = Just (value follows)
',' (0x2C) = Nothing (alternative marker)
```

### 3a.2 Reply Queue HELLO Structure

```
[14] = '1' (0x31) = maybe_corrId = Just (corrId follows)
[15] = ',' (0x2C) = maybe_e2e = Nothing (NO e2ePubKey!)
```

### 3a.3 When maybe_e2e = Nothing

- Message has NO ephemeral e2ePubKey
- Uses pre-computed e2eDhSecret
- Secret was created during connection setup
- We need `app.sndQueue.e2ePubKey` to calculate it
- This key is in App's AgentConfirmation!

---

## 3. Message Structure (Verified Session 14)

### 3.1 Reply Queue After Server-Decrypt

```
Offset  Bytes                              Meaning
------  -----                              -------
[0-1]   3e 82                              Length prefix: 16002
[2-9]   00 00 00 00 69 7e 97 10            Padding/Timestamp
[10-13] 54 20 00 04                        PubHeader: Version, Flags
[14]    31 ('1')                           Maybe tag = Just (key present!)
[15]    2c (44)                            SPKI Length
[16-27] 30 2a 30 05 06 03 2b 65 6e 03 21 00  X25519 SPKI Header
[28-59] 91 40 e1 0e ...                    Peer's E2E public key (32 bytes)
[60-83] b2 1f a2 bc ...                    cmNonce (24 bytes)
[84-99] cc 3e ec 54 ...                    MAC (16 bytes)
[100+]  5b f2 2e fa ...                    Ciphertext
```

### 3.2 Key Extraction

```c
// Peer's E2E public key at offset 28
uint8_t peer_e2e_pub[32];
memcpy(peer_e2e_pub, &server_plain[28], 32);

// Nonce at offset 60
uint8_t cm_nonce[24];
memcpy(cm_nonce, &server_plain[60], 24);

// MAC at offset 84
const uint8_t *mac = &server_plain[84];

// Ciphertext at offset 100
const uint8_t *ciphertext = &server_plain[100];
```

---

## 4. Verified Test Data (Session 14)

### 4.1 Keys (VERIFIED MATCH!)

```python
# Our E2E private key
our_e2e_private = "83473153de033039edec9c5db7591cacfa42b6dd89a0618a00806732d01a96fa"

# Peer's E2E public key (from message header)
peer_e2e_pub = "9140e10e9fdee92ebb801ae8694435b5e9f06c4e0077dfa98d39b0f1bf0c0300"

# DH Secret (VERIFIED - Python matches ESP32!)
dh_secret = "d0b7b55cbcfacd540e399ab41346e1267a8100ca7e37f9748f59b95ec4291810"
```

### 4.2 Nonce and MAC (VERIFIED)

```python
# cmNonce (24 bytes)
cm_nonce = "b21fa2bc0dbb5cb02d674dedfd65b0e6ff0fcf793791fd3b"

# MAC (16 bytes)
mac = "cc3eec548b0440cf0222466a79a00c0c"

# Ciphertext length
ciphertext_len = 16006
```

---

## 5. Crypto Functions

### 5.1 DH Calculation (CORRECT)

```c
// Use crypto_scalarmult for raw DH (NOT crypto_box_beforenm!)
uint8_t dh_secret[32];
crypto_scalarmult(dh_secret, our_queue.e2e_private, peer_e2e_pub);
```

**Why NOT crypto_box_beforenm?**
- `crypto_box_beforenm` applies HSalsa20 key derivation
- Haskell uses raw DH output directly
- `crypto_scalarmult` gives raw DH output

### 5.2 Decrypt (Current Implementation)

```c
// Haskell format: [MAC 16][Ciphertext]
const uint8_t *mac = &server_plain[84];
const uint8_t *ciphertext = &server_plain[100];

int ret = crypto_secretbox_open_detached(
    plain,          // output
    ciphertext,     // input (after MAC)
    mac,            // MAC (first 16 bytes)
    ciphertext_len, // only ciphertext length
    cm_nonce,       // 24 bytes
    dh_secret       // raw DH output
);
```

### 5.3 Haskell vs libsodium

| Aspect | Haskell | libsodium | Match? |
|--------|---------|-----------|--------|
| Algorithm | XSalsa20-Poly1305 | crypto_secretbox | YES |
| Key | Raw DH (32 bytes) | Raw DH | YES |
| DH Function | X25519.dh | crypto_scalarmult | YES |
| Format | [MAC][Cipher] | detached | YES |

### 5.4 SimpleX Custom XSalsa20 (Session 16 Discovery!)

**CRITICAL:** SimpleX uses NON-STANDARD XSalsa20!

```
Standard libsodium crypto_secretbox:
  HSalsa20(dh_secret, nonce[0:16])

SimpleX xSalsa20 (Crypto.hs):
  HSalsa20(dh_secret, zeros[16])    <- ZEROS not nonce!
  HSalsa20(subkey1, nonce[8:24])
  Salsa20(subkey2, nonce[0:8])
```

**Subkeys are COMPLETELY DIFFERENT!**
```
Standard:  2d4b4528855228d0abf137ea...
SimpleX:   ce1b436c8b333a5ff881d4c0...
```

**Implementation (simplex_crypto.c):**
```c
int simplex_secretbox_open(...) {
    uint8_t subkey1[32], subkey2[32];
    uint8_t zeros[16] = {0};
    
    // Step 1: HSalsa20(dh_secret, zeros[16])
    crypto_core_hsalsa20(subkey1, zeros, dh_secret, NULL);
    
    // Step 2: HSalsa20(subkey1, nonce[8:24])
    crypto_core_hsalsa20(subkey2, &nonce[8], subkey1, NULL);
    
    // Step 3: Salsa20 decrypt + Poly1305 verify
}
```

---

## 6. Working Code State

### 6.1 smp_ratchet.c (DO NOT CHANGE!)

```c
#define RATCHET_VERSION         2
uint8_t em_header[123];         // 123 bytes!
em_header[hp++] = 0x58;         // ehBody-len = 88 (1 BYTE!)
output[p++] = 0x7B;             // emHeader len = 123
```

### 6.2 smp_queue.h

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

---

## 7. Message Flow (VERIFIED Session 14)

### 7.1 Correct Flow (from Haskell Source)

```
Contact Queue: 1 message
  - INVITATION (Type 'I')

Reply Queue: 1 message
  - HELLO (AgentMsgEnvelope)

NO SECOND MESSAGE ON CONTACT QUEUE!
```

### 7.2 Handoff Theory Was WRONG

| Handoff Document | Reality |
|------------------|---------|
| 2 MSGs on Contact Queue | FALSE |
| PHConfirmation has key | FALSE |
| HELLO on Reply Queue | TRUE |

---

## 8. Open Questions (Session 15)

### 8.1 Offset Problem?

```
server_plain[0-1] = 3e 82 (Length Prefix)

Question: Do offsets need +2 shift?
- peer_e2e_pub: [28-59] or [30-61]?
- cm_nonce: [60-83] or [62-85]?
- MAC: [84-99] or [86-101]?
```

### 8.2 To Verify

1. Add debug output for raw offsets
2. Export full ciphertext (16006 bytes) for Python test
3. Check libsodium parameter order

---

## 9. Important Source Locations

### 9.1 Haskell

| Function | File | Lines |
|----------|------|-------|
| agentCbEncrypt | Agent/Client.hs | 1925-1933 |
| cryptoBox | Crypto.hs | 1295-1298 |
| xSalsa20 | Crypto.hs | 1449-1456 |
| sbDecryptNoPad_ | Crypto.hs | 1325-1333 |
| e2eDhSecret | Agent.hs | 3379 |
| ICDuplexSecure | Agent.hs | 1549-1551 |

### 9.2 SimpleGo

| Function | File |
|----------|------|
| E2E Decrypt | main.c:780-850 |
| Queue Create | smp_queue.c:210 |
| Queue Encode | smp_queue.c:455 |
| Peer Connect | smp_peer.c:50 |

---

## 9. Session 15 Theory (DISPROVEN in Session 16)

### 9.1 Session 15 Claimed

```
App's HELLO on Reply Queue has maybe_e2e = Nothing
-> Uses pre-computed e2eDhSecret
-> We need app.sndQueue.e2ePubKey
-> Key is in App's AgentConfirmation
-> WE DON'T RECEIVE THIS MESSAGE!
```

### 9.2 Evgeny's Response (Session 16)

> "sender's public DH key sent in confirmation header - this is
> **outside of AgentConnInfoReply but in the same message**"

**The key IS in the message header! NO second message needed!**

---

## 10. The Real Problem (Session 16-17)

### 10.1 Double Ratchet Problem

```
The Peer CANNOT decrypt our AgentConfirmation!

Evidence:
- Android shows "Request to connect" (not "Connecting")
- Header decrypt OK, Payload decrypt FAILED
- 4 different DH keys all fail

Root Cause:
- NOT missing keys
- Probably rcAD order wrong
- Or X3DH DH order wrong
- Or HKDF parameters wrong
```

### 10.2 Length Prefix Difference (Session 17)

```
Contact Queue: No length prefix before ClientMsgEnvelope
Reply Queue:   2-byte length prefix (e.g. 0x3E82 = 16002)
```

### 10.3 cmNonce (Session 17)

```
cmNonce is RANDOM - directly stored in the message!
NOT calculated from any other value.
Extract from message at correct offset.
```

### 10.4 Layer 2 Decrypt Flow (Haskell Reference)

```
1. Parse ClientMsgEnvelope from Layer 1 output
2. Extract e2ePubKey_ (sender's ephemeral) from PubHeader
3. Get e2ePrivKey from RcvQueue (key from queue creation!)
4. e2eDh = DH(sender_ephemeral_pub, our_e2e_private)
5. plaintext = crypto_box_open(cmEncBody, cmNonce, e2eDh)
```

---

*Quick Reference v11.0*  
*Last updated: February 4, 2026 - Session 17*  
*Status: Key Consistency Investigation*
