# Bug Tracker

## Complete Documentation of All 14 Bugs

This document provides detailed documentation of all bugs discovered during SimpleGo development, including the incorrect code, correct code, and root cause analysis.

---

## 🎉 ALL CRITICAL BUGS FIXED! (2026-01-27)

**Session 8 achieved the breakthrough - AgentConfirmation now works!**

---

## Summary

| Bug # | Component | Session | Status |
|-------|-----------|---------|--------|
| 1 | E2E key length | 4 | ✅ FIXED |
| 2 | prevMsgHash length | 4 | ✅ FIXED |
| 3 | MsgHeader DH key | 4 | ✅ FIXED |
| 4 | ehBody length | 4 | ✅ FIXED |
| 5 | emHeader size | 4 | ✅ FIXED |
| 6 | Payload AAD size | 4 | ✅ FIXED |
| 7 | Root KDF output order | 4 | ✅ FIXED |
| 8 | Chain KDF IV order | 4 | ✅ FIXED |
| 9 | wolfSSL X448 byte order | 5 | ✅ FIXED |
| 10 | Port encoding | 6 | ✅ FIXED |
| 11 | smpQueues count | 6 | ✅ FIXED |
| 12 | queueMode Nothing | 6 | ✅ FIXED |
| **13** | **Payload AAD length prefix** | **8** | ✅ **FIXED** |
| **14** | **chainKdf IV assignment** | **8** | ✅ **FIXED** |

---

## Bug #1: E2E Key Length Prefix

**Session:** 4  
**Component:** E2ERatchetParams encoding  
**Impact:** Critical - causes parsing failure

### Incorrect Code
```c
// Word16 BE length prefix (WRONG!)
buf[p++] = 0x00;
buf[p++] = 0x44;  // 68 as Word16
memcpy(&buf[p], spki_key, 68);
```

### Correct Code
```c
// 1-byte length prefix (CORRECT!)
buf[p++] = 0x44;  // 68 as single byte
memcpy(&buf[p], spki_key, 68);
```

### Root Cause

E2ERatchetParams keys are encoded as ByteString (1-byte prefix), not Large (Word16 prefix).

---

## Bug #2: prevMsgHash Length Prefix

**Session:** 4  
**Component:** AgentMessage encoding  
**Impact:** Critical - causes parsing failure

### Incorrect Code
```c
// 1-byte length prefix (WRONG!)
buf[p++] = 0x00;  // Empty hash
```

### Correct Code
```c
// Word16 BE length prefix (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x00;  // Empty hash as Word16
```

### Root Cause

AgentMessage uses Large wrapper for prevMsgHash, requiring Word16 prefix.

---

## Bug #3: MsgHeader DH Key Length

**Session:** 4  
**Component:** MsgHeader encoding  
**Impact:** Critical - causes parsing failure

### Incorrect Code
```c
// Word16 BE length prefix (WRONG!)
buf[p++] = 0x00;
buf[p++] = 0x44;
memcpy(&buf[p], dh_key_spki, 68);
```

### Correct Code
```c
// 1-byte length prefix (CORRECT!)
buf[p++] = 0x44;
memcpy(&buf[p], dh_key_spki, 68);
```

### Root Cause

MsgHeader msgDHRs is PublicKey, encoded as ByteString with 1-byte prefix.

---

## Bug #4: ehBody Length Prefix

**Session:** 4  
**Component:** EncMessageHeader encoding  
**Impact:** Critical - cascades to bugs #5 and #6

### Incorrect Code
```c
// Word16 BE length prefix (WRONG!)
em_header[hp++] = 0x00;
em_header[hp++] = 0x58;  // 88 as Word16
```

### Correct Code
```c
// 1-byte length prefix (CORRECT!)
em_header[hp++] = 0x58;  // 88 as single byte
```

### Root Cause

ehBody is ByteString, not Large.

---

## Bug #5: emHeader Size

**Session:** 4  
**Component:** EncMessageHeader structure  
**Impact:** Critical - cascades to bug #6

### Incorrect Code
```c
#define EM_HEADER_SIZE 124
uint8_t em_header[124];
```

### Correct Code
```c
#define EM_HEADER_SIZE 123
uint8_t em_header[123];
```

### Root Cause

Cascaded from Bug #4 - with 1-byte prefix, size is 123 not 124.

---

## Bug #6: Payload AAD Size

**Session:** 4  
**Component:** AES-GCM AAD  
**Impact:** Critical - auth tag mismatch

### Incorrect Code
```c
uint8_t payload_aad[236];  // WRONG!
aes_gcm_encrypt(..., payload_aad, 236, ...);
```

### Correct Code
```c
uint8_t payload_aad[235];  // CORRECT!
aes_gcm_encrypt(..., payload_aad, 235, ...);
```

### Root Cause

Cascaded from Bug #5 - AAD = 112 + 123 = 235, not 236.

---

## Bug #7: Root KDF Output Order

**Session:** 4  
**Component:** Root KDF implementation  
**Impact:** Critical - all keys wrong

### Incorrect Code
```c
// Wrong order!
memcpy(chain_key, kdf_output, 32);
memcpy(new_root_key, kdf_output + 32, 32);
```

### Correct Code
```c
// Correct order per Haskell
memcpy(new_root_key, kdf_output, 32);
memcpy(chain_key, kdf_output + 32, 32);
memcpy(next_header_key, kdf_output + 64, 32);
```

### Root Cause

Misread Haskell source - output order is root, chain, header.

---

## Bug #8: Chain KDF IV Order

**Session:** 4  
**Component:** Chain KDF implementation  
**Impact:** Critical - encryption uses wrong IVs

### Incorrect Code
```c
// Swapped! (WRONG!)
memcpy(msg_iv, kdf_output + 64, 16);
memcpy(header_iv, kdf_output + 80, 16);
```

### Correct Code
```c
// Correct order!
memcpy(header_iv, kdf_output + 64, 16);  // iv1 = header
memcpy(msg_iv, kdf_output + 80, 16);     // iv2 = message
```

### Root Cause

iv1 (bytes 64-79) is header IV, iv2 (bytes 80-95) is message IV.

---

## Bug #9: wolfSSL X448 Byte Order

**Session:** 5  
**Component:** X448 cryptography  
**Impact:** Critical - all DH computations wrong

### The Problem

wolfSSL X448 uses little-endian, SimpleX expects big-endian.

### The Fix
```c
static void reverse_bytes(const uint8_t *src, uint8_t *dst, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[len - 1 - i];
    }
}

// After key generation:
reverse_bytes(pub_tmp, keypair->public_key, 56);
reverse_bytes(priv_tmp, keypair->private_key, 56);

// Before DH:
reverse_bytes(their_public, their_public_rev, 56);
reverse_bytes(my_private, my_private_rev, 56);

// After DH:
reverse_bytes(secret_tmp, shared_secret, 56);
```

### Root Cause

wolfSSL defines EC448_LITTLE_ENDIAN internally.

---

## Bug #10: Port Encoding

**Session:** 6  
**Component:** SMPQueueInfo encoding  
**Impact:** Critical - parser fails

### Incorrect Code
```c
// Length prefix (WRONG!)
buf[p++] = (uint8_t)strlen(port_str);
memcpy(&buf[p], port_str, strlen(port_str));
```

### Correct Code
```c
// Space separator (CORRECT!)
buf[p++] = ' ';  // 0x20
memcpy(&buf[p], port_str, strlen(port_str));
```

### Root Cause

SMPServer encoding uses space separator, not length prefix.

---

## Bug #11: smpQueues Count

**Session:** 6  
**Component:** NonEmpty list encoding  
**Impact:** Critical - parser fails

### Incorrect Code
```c
// 1-byte count (WRONG!)
buf[p++] = 0x01;
```

### Correct Code
```c
// Word16 BE count (CORRECT!)
buf[p++] = 0x00;
buf[p++] = 0x01;
```

### Root Cause

NonEmpty list uses Word16 for count.

---

## Bug #12: queueMode Nothing

**Session:** 6  
**Component:** SMPQueueInfo encoding  
**Impact:** Medium - parser might fail

### Incorrect Code
```c
// Send '0' byte (WRONG!)
buf[p++] = '0';  // 0x30
```

### Correct Code
```c
// Send NOTHING (CORRECT!)
// (no code - just don't write anything)
```

### Root Cause

queueMode uses "maybe empty" not standard Maybe encoding.

---

## Bug #13: Payload AAD Length Prefix 🎉

**Session:** 8 - **THE BREAKTHROUGH!**  
**Component:** Payload AAD construction  
**Impact:** Critical - auth tag mismatch, A_MESSAGE error

### The Problem

The AAD for payload encryption incorrectly included a length prefix before emHeader:

### Incorrect Code
```c
// WRONG - includes length prefix!
uint8_t payload_aad[236];  // 112 + 1 + 123 = 236
memcpy(payload_aad, ratchet_state.assoc_data, 112);
payload_aad[112] = 0x7B;  // Length prefix (123) ← ERROR!
memcpy(payload_aad + 113, em_header, 123);
```

### Correct Code
```c
// CORRECT - NO length prefix!
uint8_t payload_aad[235];  // 112 + 123 = 235
memcpy(payload_aad, ratchet_state.assoc_data, 112);
memcpy(payload_aad + 112, em_header, 123);  // Direct concatenation!
```

### Root Cause

**Haskell Reference (Ratchet.hs:1155-1157):**
```haskell
decryptMessage (MessageKey mk iv) EncRatchetMessage {emHeader, emBody, emAuthTag} =
  tryE $ decryptAEAD mk iv (rcAD <> emHeader) emBody emAuthTag
```

The `emHeader` in the AAD is the **parsed** header (after `largeP`) - meaning WITHOUT the length prefix! The length prefix is only used for wire transmission, NOT for AAD calculation.

---

## Bug #14: chainKdf IV Assignment 🎉

**Session:** 8 - **THE BREAKTHROUGH!**  
**Component:** Chain KDF IV extraction  
**Impact:** Critical - wrong IVs used for encryption

### The Problem

The IV assignment from chainKdf output was swapped:

### Incorrect Code
```c
// WRONG - IVs swapped!
memcpy(header_iv, kdf_output + 64, 16);  // iv1 used for header
memcpy(msg_iv, kdf_output + 80, 16);     // iv2 used for message
```

### Correct Code
```c
// CORRECT - per Haskell source!
memcpy(msg_iv, kdf_output + 64, 16);     // iv1 = Message IV
memcpy(header_iv, kdf_output + 80, 16);  // iv2 = Header IV
```

### Root Cause

**Haskell Reference (Ratchet.hs:1168-1172 and line 906):**
```haskell
chainKdf (RatchetKey ck) =
  let (ck', mk, ivs) = hkdf3 "" ck "SimpleXChainRatchet"
      (iv1, iv2) = B.splitAt 16 ivs
   in (RatchetKey ck', Key mk, IV iv1, IV iv2)

-- Usage in line 906:
let (ck', mk, iv, ehIV) = chainKdf rcCKs
-- iv = iv1 = for payload (message)
-- ehIV = iv2 = for header
```

---

## Bug Discovery Timeline

| Date | Session | Bugs Found |
|------|---------|------------|
| Jan 23, 2026 | S4 | #1-#6 |
| Jan 24, 2026 | S4 | #7-#8 |
| Jan 24, 2026 | S5 | #9 |
| Jan 24, 2026 | S6 | #10-#12 |
| **Jan 27, 2026** | **S8** | **#13-#14** 🎉 |

---

## Bug Categories

```
14 Bugs Total:
├── 7× Length Prefix issues
├── 3× KDF/IV Order issues
├── 1× Byte Order issue (wolfSSL)
├── 1× Separator issue (Space vs Length)
├── 1× Maybe encoding issue
└── 1× AAD construction issue
```

---

## Lessons Learned

1. **Length encoding varies by context** - always check Haskell source
2. **Crypto libraries differ** - verify against reference implementations
3. **Cascade effects are real** - one bug can cause multiple symptoms
4. **A_MESSAGE != A_CRYPTO** - parsing error vs crypto error
5. **Tail means no prefix** - last fields don't need length
6. **Two pad() functions exist** - Lazy.hs (Int64) vs Crypto.hs (Word16)
7. **Wire format ≠ Crypto format** - length prefixes for serialization, not always for AAD
8. **Haskell parser awareness** - `largeP` removes length prefix from parsed object
9. **Python verification essential** - systematically verify all crypto operations
10. **Community support helps** - SimpleX developers are responsive and helpful

---

## Current Status

### 🎉 BREAKTHROUGH ACHIEVED!

- ✅ All 14 bugs FIXED
- ✅ AgentConfirmation ACCEPTED by app
- ✅ Double Ratchet E2E encryption WORKING
- ✅ Contact "ESP32" visible in SimpleX App
- ✅ Connection status: JOINED

### Next Priorities

- 🔥 HELLO Handshake (ERR AUTH)
- ⏳ Incoming message decryption
- ⏳ Bidirectional chat

---

*Bug Tracker v2.0*  
*Last updated: January 27, 2026 - Session 8 (🎉 BREAKTHROUGH!)*  
*Total bugs documented: 14*
