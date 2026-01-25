# Session 4: Wire Format Analysis

## The Great Bug Hunt - Finding 8 Critical Encoding Bugs

**Date:** January 23-24, 2026
**Version:** v0.1.14 to v0.1.24-alpha
**Bugs Fixed:** #1 through #8

---

## 🎯 Session Overview

This session marked the transition from "it compiles" to "it almost works". Through meticulous byte-level analysis of the Haskell source code, we discovered and fixed 8 critical encoding bugs that were preventing the SimpleX app from parsing our messages.

**The Challenge:** Server accepted our messages (OK response), but the SimpleX app showed "error agent AGENT A_MESSAGE" - a parsing error.

---

## 📋 Table of Contents

1. [The A_MESSAGE Error](#1-the-a_message-error)
2. [Protocol Stack Deep Dive](#2-protocol-stack-deep-dive)
3. [Version Checks Analysis](#3-version-checks-analysis)
4. [Encoding Rules Discovery](#4-encoding-rules-discovery)
5. [Bug #1: E2E Key Length](#5-bug-1-e2e-key-length)
6. [Bug #2: prevMsgHash Length](#6-bug-2-prevmsghash-length)
7. [Bug #3: MsgHeader DH Key Length](#7-bug-3-msgheader-dh-key-length)
8. [Bug #4: ehBody Length](#8-bug-4-ehbody-length)
9. [Bug #5: emHeader Size](#9-bug-5-emheader-size)
10. [Bug #6: Payload AAD Size](#10-bug-6-payload-aad-size)
11. [Bug #7: Root KDF Output Order](#11-bug-7-root-kdf-output-order)
12. [Bug #8: Chain KDF IV Order](#12-bug-8-chain-kdf-iv-order)
13. [Wire Format Specifications](#13-wire-format-specifications)
14. [Session Summary](#14-session-summary)
15. [Changelog](#15-changelog)

---

## 1. The A_MESSAGE Error

### 1.1 The Paradox
`
The Mysterious Situation:
═══════════════════════════════════════════════════════════════════

✅ TLS 1.3 connection: Working
✅ SMP handshake: Working
✅ Queue creation: Working
✅ Message sending: Server returns "OK"
❌ SimpleX App: "error agent AGENT A_MESSAGE"

The server accepts our message, but the app can't parse it!

═══════════════════════════════════════════════════════════════════
`

### 1.2 Error Location in Haskell Source

**Source:** grep -rn "A_MESSAGE" ~/simplexmq/src/

| File | Line | Context |
|------|------|---------|
| Agent.hs | 2780 | Message parsing |
| Agent.hs | 2813 | AgentMessage decode |
| Agent.hs | 2897 | Ratchet message decode |
| Protocol.hs | 1913 | Error definition |

### 1.3 Error Definition
`haskell
-- From Protocol.hs:1913
data AgentErrorType
  = -- | message parsing error
    A_MESSAGE
  | -- | crypto error
    A_CRYPTO
  | -- | version incompatible
    A_VERSION
`

**Key Insight:** A_MESSAGE means parsing failed, NOT crypto failed. The message format is wrong, not the encryption.

---

## 2. Protocol Stack Deep Dive

### 2.1 Complete Message Structure
`
SimpleX Protocol Stack (Detailed):
═══════════════════════════════════════════════════════════════════

📡 Transport Layer (TLS 1.3)
│   └── ALPN: "smp/1"
│
📦 SMP Protocol Layer
│   ├── Commands: NEW, KEY, SUB, SEND, ACK, OFF, DEL
│   ├── Server Responses: IDS, MSG, OK, ERR, END
│   └── ClientMsgEnvelope (for SEND command)
│       ├── PubHeader
│       │   ├── phVersion (Word16 BE) ← SMP Client Version 1-4
│       │   └── phE2ePubDhKey (Maybe X25519)
│       ├── cmNonce (24 bytes)
│       └── cmEncBody (encrypted)
│
🔐 Encryption Layer (crypto_box)
│   └── ClientMessage (after decryption)
│       ├── PrivHeader
│       │   ├── PHConfirmation = 'K' + Ed25519 SPKI (NO length prefix!)
│       │   └── PHEmpty = '_'
│       └── Body (AgentMsgEnvelope)
│
🤝 Agent Protocol Layer
│   ├── AgentMsgEnvelope Types:
│   │   ├── AgentConfirmation (Tag 'C') ← WE ARE HERE!
│   │   ├── AgentInvitation
│   │   ├── AgentMsgEnvelope (Tag 'M')
│   │   └── AgentRatchetKey
│   │
│   └── AgentConfirmation Structure:
│       ├── agentVersion (Word16 BE) ← Agent Version 2-7
│       ├── 'C' Tag
│       ├── e2eEncryption_ (Maybe E2ERatchetParams)
│       │   └── E2ERatchetParams
│       │       ├── e2eVersion (Word16 BE) ← E2E Version 2-3
│       │       ├── e2ePubKey1 (X448 SPKI)
│       │       ├── e2ePubKey2 (X448 SPKI)
│       │       └── e2eKEM (optional, v3+)
│       └── encConnInfo (Ratchet-encrypted)
│           └── After decryption: AgentMessage
│               ├── 'I' = AgentConnInfo (initiating party)
│               └── 'D' = AgentConnInfoReply (joining party) ← US!
│
🔄 Double Ratchet Layer
│   ├── X3DH Key Agreement
│   ├── Header Encryption
│   └── Message Encryption (AES-256-GCM)
│
💬 Chat Protocol Layer (JSON)
    └── {"event":"x.info","params":{...}}

═══════════════════════════════════════════════════════════════════
`

### 2.2 Our Position in the Stack

We are implementing the **joining party** in a contact address handshake:

1. App creates QR code/link with contact address
2. We scan/receive the invitation
3. We send AgentConfirmation with our E2E keys
4. We send HELLO message
5. Connection established!

---

## 3. Version Checks Analysis

### 3.1 Three Version Checks

The SimpleX agent performs three version compatibility checks:

| Check | Location | What | Valid Range |
|-------|----------|------|-------------|
| 1 | Line 2707 | phVer (SMP Client) | 1-4 |
| 2 | Line 2908 | agentVersion | 2-7 |
| 3 | Line 2913 | e2eVersion | 2-3 |

### 3.2 Check 1: SMP Client Version (Line 2707)
`haskell
processClientMsg srvTs msgFlags msgBody = do
  clientMsg@SMP.ClientMsgEnvelope {cmHeader = SMP.PubHeader phVer e2ePubKey_} <-
    parseMessage msgBody
  clientVRange <- asks $ smpClientVRange . config
  unless (phVer isCompatible clientVRange) . throwE $ AGENT A_VERSION
`

**Our value:** 4 (0x00 0x04) ✅

### 3.3 Check 2: Agent Version (Line 2908)
`haskell
smpConfirmation ... agentVersion = do
  AgentConfig {smpAgentVRange} <- asks config
  let compatible = agentVersion isCompatible smpAgentVRange
  unless compatible $ throwE $ AGENT A_VERSION
`

**Our value:** 7 (0x00 0x07) ✅

### 3.4 Check 3: E2E Version (Line 2913)
`haskell
case e2eEncryption of
  Just (CR.E2ERatchetParams e2eVersion _ _ _) -> do
    unless (e2eVersion isCompatible e2eEncryptVRange) (throwE $ AGENT A_VERSION)
`

**Our value:** 2 (0x00 0x02) ✅

**All version checks pass!** The problem must be in the message format.

---

## 4. Encoding Rules Discovery

### 4.1 Maybe Type Encoding

**Source:** Encoding.hs:114-115
`haskell
instance Encoding a => Encoding (Maybe a) where
  smpEncode = maybe "0" (('1' B.cons) . smpEncode)
`

**CRITICAL:** Uses ASCII characters, NOT binary!
- `Nothing` = ASCII '0' (0x30)
- `Just x` = ASCII '1' (0x31) + encoded value

### 4.2 ByteString Length Encoding

**Source:** Encoding.hs:100-104
`haskell
instance Encoding ByteString where
  smpEncode s = B.cons (lenEncode $ B.length s) s

lenEncode :: Int -> Char
lenEncode = w2c . fromIntegral  -- Single byte for lengths <= 254
`

**For lengths <= 254:** 1-byte length prefix

### 4.3 Large ByteString Encoding

**Source:** Encoding.hs:136
`haskell
instance Encoding Large where
  smpEncode (Large s) = smpEncode @Word16 (fromIntegral $ B.length s) <> s
`

**For Large wrapper:** 2-byte (Word16 BE) length prefix

### 4.4 Word16 Encoding
`haskell
instance Encoding Word16 where
  smpEncode w = B.pack [fromIntegral (w shiftR 8), fromIntegral w]
`

**Always Big Endian!**

---

## 5. Bug #1: E2E Key Length

### 5.1 Discovery

The E2E ratchet keys (X448 SPKI) were encoded with Word16 length prefix instead of 1-byte.

### 5.2 Haskell Reference
`haskell
-- E2ERatchetParams encoding (Crypto/Ratchet.hs)
smpEncode (E2ERatchetParams v k1 k2 kem_)
  | v >= pqRatchetE2EEncryptVersion = smpEncode (v, k1, k2, kem_)
  | otherwise = smpEncode (v, k1, k2)  -- v2: no KEM
`

The keys are encoded as regular ByteStrings, which use 1-byte length prefix.

### 5.3 The Fix

**Before (WRONG):**
`c
// Word16 BE length prefix
buf[p++] = 0x00;
buf[p++] = 0x44;  // 68 as Word16
memcpy(&buf[p], spki_key1, 68);
`

**After (CORRECT):**
`c
// 1-byte length prefix
buf[p++] = 0x44;  // 68 as single byte
memcpy(&buf[p], spki_key1, 68);
`

### 5.4 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | Word16 instead of 1-byte |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 6. Bug #2: prevMsgHash Length

### 6.1 Discovery

In the HELLO message, the prevMsgHash field used 1-byte length instead of Word16.

### 6.2 Haskell Reference
`haskell
-- AgentMessage encoding (Agent/Protocol.hs)
data AgentMessage = AgentMessage
  { agentMsgId :: Word64
  , agentMsgPrev :: ByteString  -- prevMsgHash - uses Large encoding!
  , agentMsgBody :: AMessage
  }

smpEncode AgentMessage {..} = smpEncode (agentMsgId, Large agentMsgPrev, agentMsgBody)
`

The `Large` wrapper means Word16 length prefix!

### 6.3 The Fix

**Before (WRONG):**
`c
// 1-byte length prefix
buf[p++] = 0x00;  // Empty hash length (1 byte)
`

**After (CORRECT):**
`c
// Word16 BE length prefix
buf[p++] = 0x00;
buf[p++] = 0x00;  // Empty hash length (Word16 BE)
`

### 6.4 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | 1-byte instead of Word16 |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 7. Bug #3: MsgHeader DH Key Length

### 7.1 Discovery

The DH ratchet key in MsgHeader used Word16 length instead of 1-byte.

### 7.2 Haskell Reference
`haskell
-- MsgHeader encoding (Crypto/Ratchet.hs)
data MsgHeader = MsgHeader
  { msgMaxVersion :: Version
  , msgDHRs :: PublicKey X448
  , msgPN :: Word32
  , msgNs :: Word32
  }

smpEncode MsgHeader {..} = smpEncode (msgMaxVersion, msgDHRs, msgPN, msgNs)
`

PublicKey is encoded as ByteString with 1-byte prefix.

### 7.3 The Fix

**Before (WRONG):**
`c
// Word16 BE length prefix
buf[p++] = 0x00;
buf[p++] = 0x44;  // 68 as Word16
memcpy(&buf[p], dh_key_spki, 68);
`

**After (CORRECT):**
`c
// 1-byte length prefix
buf[p++] = 0x44;  // 68 as single byte
memcpy(&buf[p], dh_key_spki, 68);
`

### 7.4 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | Word16 instead of 1-byte |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 8. Bug #4: ehBody Length

### 8.1 Discovery

The ehBody (encrypted header body) length in EncMessageHeader used Word16 instead of 1-byte.

### 8.2 Haskell Reference
`haskell
-- EncMessageHeader encoding (Crypto/Ratchet.hs)
data EncMessageHeader = EncMessageHeader
  { ehVersion :: Version
  , ehIV :: IV
  , ehAuthTag :: AuthTag
  , ehBody :: ByteString  -- Regular ByteString = 1-byte length!
  }
`

### 8.3 The Fix

**Before (WRONG):**
`c
// Word16 BE length prefix
em_header[hp++] = 0x00;
em_header[hp++] = 0x58;  // 88 as Word16
`

**After (CORRECT):**
`c
// 1-byte length prefix
em_header[hp++] = 0x58;  // 88 as single byte
`

### 8.4 Impact on emHeader Size

This bug caused a cascade effect:
- Old emHeader size: 124 bytes (with 2-byte length)
- New emHeader size: 123 bytes (with 1-byte length)

### 8.5 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | Word16 instead of 1-byte |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 9. Bug #5: emHeader Size

### 9.1 Discovery

The emHeader (EncMessageHeader) size calculation was wrong due to Bug #4.

### 9.2 Size Calculation
`
EncMessageHeader Structure:
┌───────────┬──────────┬────────────┬─────────────┬────────────────────┐
│ ehVersion │ ehIV     │ ehAuthTag  │ ehBody-len  │ ehBody             │
│ (2 bytes) │ (16 B)   │ (16 bytes) │ (1 byte!)   │ (88 bytes)         │
└───────────┴──────────┴────────────┴─────────────┴────────────────────┘

Total: 2 + 16 + 16 + 1 + 88 = 123 bytes ✅
NOT: 2 + 16 + 16 + 2 + 88 = 124 bytes ❌
`

### 9.3 The Fix

**Before (WRONG):**
`c
#define EM_HEADER_SIZE 124
uint8_t em_header[124];
`

**After (CORRECT):**
`c
#define EM_HEADER_SIZE 123
uint8_t em_header[123];
`

### 9.4 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | Cascaded from Bug #4 |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 10. Bug #6: Payload AAD Size

### 10.1 Discovery

The payload AAD (Additional Authenticated Data) size was wrong due to Bug #5.

### 10.2 AAD Structure
`
Payload AAD = rcAD (112 bytes) + emHeader (123 bytes)

┌─────────────────────────────────┬──────────────────────────────────┐
│ rcAD (112 bytes)                │ emHeader (123 bytes)             │
├─────────────────────────────────┼──────────────────────────────────┤
│ our_key1 (56) | peer_key1 (56)  │ [complete EncMessageHeader]      │
└─────────────────────────────────┴──────────────────────────────────┘

Total: 112 + 123 = 235 bytes ✅
NOT: 112 + 124 = 236 bytes ❌
`

### 10.3 The Fix

**Before (WRONG):**
`c
uint8_t payload_aad[236];
// ...
aes_gcm_encrypt(..., payload_aad, 236, ...);
`

**After (CORRECT):**
`c
uint8_t payload_aad[235];
// ...
aes_gcm_encrypt(..., payload_aad, 235, ...);
`

### 10.4 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | Cascaded from Bug #5 |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 11. Bug #7: Root KDF Output Order

### 11.1 Discovery

The Root KDF output bytes were split in the wrong order.

### 11.2 Haskell Reference
`haskell
-- Root KDF (Crypto/Ratchet.hs)
rootKdf :: RootKey -> DHOutput -> (RootKey, ChainKey, HeaderKey)
rootKdf rk dh = (rk', ck, nhk)
  where
    out = hkdf rk dh "SimpleXRootRatchet"  -- 96 bytes
    rk' = take 32 out                       -- bytes 0-31: new root key
    ck  = take 32 (drop 32 out)             -- bytes 32-63: chain key
    nhk = drop 64 out                       -- bytes 64-95: next header key
`

### 11.3 The Fix

**Before (WRONG):**
`c
// Wrong order!
memcpy(chain_key, kdf_output, 32);      // bytes 0-31
memcpy(new_root_key, kdf_output + 32, 32);  // bytes 32-63
`

**After (CORRECT):**
`c
// Correct order per Haskell
memcpy(new_root_key, kdf_output, 32);       // bytes 0-31: root key
memcpy(chain_key, kdf_output + 32, 32);     // bytes 32-63: chain key
memcpy(next_header_key, kdf_output + 64, 32); // bytes 64-95: next header key
`

### 11.4 Visualization
`
Root KDF Output (96 bytes):
═══════════════════════════════════════════════════════════════════

Position:    0-31           32-63          64-95
             ─────────────────────────────────────
HKDF Output: [new_root_key] [chain_key]    [next_header_key]

OLD CODE (WRONG):
             [chain_key❌]  [root_key❌]   [ignored❌]

NEW CODE (CORRECT):
             [root_key✅]   [chain_key✅]  [next_header✅]

═══════════════════════════════════════════════════════════════════
`

### 11.5 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | Output order reversed |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 12. Bug #8: Chain KDF IV Order

### 12.1 Discovery

The Chain KDF output bytes for IVs were swapped - header_iv and msg_iv were reversed!

### 12.2 Haskell Reference
`haskell
-- Chain KDF (Crypto/Ratchet.hs)
chainKdf :: ChainKey -> (ChainKey, MessageKey, IV, IV)
chainKdf ck = (ck', mk, iv1, iv2)
  where
    out = hkdf "" ck "SimpleXChainRatchet"  -- 96 bytes, empty salt!
    ck' = take 32 out                        -- bytes 0-31: next chain key
    mk  = take 32 (drop 32 out)              -- bytes 32-63: message key
    iv1 = take 16 (drop 64 out)              -- bytes 64-79: HEADER IV!
    iv2 = drop 80 out                        -- bytes 80-95: MESSAGE IV!
`

### 12.3 The Fix

**Before (WRONG - swapped!):**
`c
// IVs from ChainKDF - WRONG!
memcpy(msg_iv, kdf_output + 64, 16);     // ❌ This is iv1 = header_iv!
memcpy(header_iv, kdf_output + 80, 16);  // ❌ This is iv2 = msg_iv!
`

**After (CORRECT):**
`c
// IVs from ChainKDF - CORRECT ORDER
memcpy(header_iv, kdf_output + 64, 16);  // ✅ iv1 = Header IV (bytes 64-79)
memcpy(msg_iv, kdf_output + 80, 16);     // ✅ iv2 = Message IV (bytes 80-95)
`

### 12.4 Why This Was Critical
`
Encryption Flow:
1. Header Encryption: AES-GCM(header_key, header_iv, rcAD, MsgHeader)
2. Payload Encryption: AES-GCM(msg_key, msg_iv, rcAD+emHeader, payload)

With swapped IVs:
- Header encrypted with msg_iv → wrong auth tag
- Payload encrypted with header_iv → wrong auth tag
- Recipient can decrypt NOTHING → A_MESSAGE!
`

### 12.5 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | IVs swapped |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 13. Wire Format Specifications

### 13.1 EncRatchetMessage
`
┌─────────────────┬──────────────────┬────────────────┬─────────────────────┐
│ emHeader-len    │ emHeader         │ Payload AuthTag│ Encrypted Payload   │
│ (1 byte)        │ (123 bytes)      │ (16 bytes)     │ (Tail - no prefix)  │
│ 0x7B            │ [EncMsgHeader]   │ [tag]          │ [encrypted]         │
└─────────────────┴──────────────────┴────────────────┴─────────────────────┘
`

### 13.2 EncMessageHeader (123 bytes)
`
┌───────────┬──────────┬────────────┬─────────────┬────────────────────┐
│ ehVersion │ ehIV     │ ehAuthTag  │ ehBody-len  │ ehBody             │
│ (2 bytes) │ (16 B)   │ (16 bytes) │ (1 byte)    │ (88 bytes)         │
│ 00 02     │ [iv]     │ [tag]      │ 58          │ [encrypted header] │
└───────────┴──────────┴────────────┴─────────────┴────────────────────┘
Total: 2 + 16 + 16 + 1 + 88 = 123 bytes ✅
`

### 13.3 MsgHeader (88 bytes, plaintext)
`
┌───────────┬─────────────┬────────────────────┬──────────┬──────────┬─────────┐
│ msgMaxVer │ msgDHRs-len │ msgDHRs (SPKI)     │ msgPN    │ msgNs    │ Padding │
│ (2 bytes) │ (1 byte)    │ (68 bytes)         │ (4 bytes)│ (4 bytes)│ (9 B)   │
│ 00 02     │ 44          │ [12B hdr + 56B key]│ [PN BE]  │ [Ns BE]  │ [zeros] │
└───────────┴─────────────┴────────────────────┴──────────┴──────────┴─────────┘
Total: 2 + 1 + 68 + 4 + 4 + 9 = 88 bytes ✅
`

### 13.4 HELLO Plaintext (12 bytes)
`
┌─────┬─────────────────────────┬───────────┬─────┐
│ 'M' │ sndMsgId (8 bytes BE)   │ prevHash  │ 'H' │
│ 4D  │ 00 00 00 00 00 00 00 01 │ len (2B)  │ 48  │
│     │                         │ 00 00     │     │
└─────┴─────────────────────────┴───────────┴─────┘
Total: 1 + 8 + 2 + 1 = 12 bytes ✅
`

### 13.5 E2E Params / SndE2ERatchetParams (140 bytes)
`
┌───────────┬─────────────┬────────────────┬─────────────┬────────────────┐
│ e2eVer    │ key1-len    │ key1 (SPKI)    │ key2-len    │ key2 (SPKI)    │
│ (2 bytes) │ (1 byte)    │ (68 bytes)     │ (1 byte)    │ (68 bytes)     │
│ 00 02     │ 44          │ [SPKI key1]    │ 44          │ [SPKI key2]    │
└───────────┴─────────────┴────────────────┴─────────────┴────────────────┘
Total: 2 + 1 + 68 + 1 + 68 = 140 bytes ✅
`

---

## 14. Session Summary

### 14.1 Bugs Fixed in Session 4

| Bug # | Component | Issue | Fix |
|-------|-----------|-------|-----|
| 1 | E2E key length | Word16 → 1-byte | Use single byte |
| 2 | prevMsgHash | 1-byte → Word16 | Use Word16 BE |
| 3 | MsgHeader DH | Word16 → 1-byte | Use single byte |
| 4 | ehBody length | Word16 → 1-byte | Use single byte |
| 5 | emHeader size | 124 → 123 | Cascade from #4 |
| 6 | Payload AAD | 236 → 235 | Cascade from #5 |
| 7 | Root KDF order | Swapped | Correct order |
| 8 | Chain KDF IVs | Swapped | header=64-79, msg=80-95 |

### 14.2 Key Learnings

1. **Length encoding is context-dependent** - ByteString uses 1-byte, Large uses Word16
2. **The Haskell source is the definitive reference** - Always verify against it
3. **Cascade effects are real** - One wrong byte can break everything
4. **Systematic debugging pays off** - Check each field individually

### 14.3 Status After Session 4
`
After Session 4:
═══════════════════════════════════════════════════════════════════

✅ 8 bugs fixed
✅ Wire format verified against Haskell source
✅ Server accepts messages
❌ App still shows A_MESSAGE

Next hypothesis: X448 cryptographic values might be wrong

═══════════════════════════════════════════════════════════════════
`

---

## 15. Changelog

| Date | Change |
|------|--------|
| 2026-01-23 S4 | Session 4 started - Wire format analysis |
| 2026-01-23 S4 | Bug #1 identified: E2E key length |
| 2026-01-23 S4 | Bug #2 identified: prevMsgHash length |
| 2026-01-23 S4 | Bug #3 identified: MsgHeader DH key |
| 2026-01-23 S4 | Bug #4 identified: ehBody length |
| 2026-01-24 S4 | Bug #5 identified: emHeader size cascade |
| 2026-01-24 S4 | Bug #6 identified: Payload AAD cascade |
| 2026-01-24 S4 | Bug #7 identified: Root KDF output order |
| 2026-01-24 S4 | Bug #8 identified: Chain KDF IV order |
| 2026-01-24 S4 | All 8 bugs fixed, A_MESSAGE persists |
| 2026-01-24 S4 | Documentation v10 created |

---

*Document version: Session 4 Complete*
*Last updated: January 24, 2026*
*Total bugs fixed this session: 8*
