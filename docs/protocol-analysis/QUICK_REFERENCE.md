# Quick Reference

## Constants, Wire Formats, and KDF Parameters

This document provides a quick reference for all the technical details needed when working with SimpleGo.

---

## Table of Contents

1. [Version Numbers](#1-version-numbers)
2. [Size Constants](#2-size-constants)
3. [Wire Formats](#3-wire-formats)
4. [KDF Parameters](#4-kdf-parameters)
5. [SPKI Key Formats](#5-spki-key-formats)
6. [Encoding Rules](#6-encoding-rules)
7. [Maybe Encoding](#7-maybe-encoding)

---

## 1. Version Numbers

### 1.1 Protocol Versions

| Protocol | Our Value | Valid Range | Hex |
|----------|-----------|-------------|-----|
| SMP Client (phVer) | 4 | 1-4 | 0x00 0x04 |
| Agent (agentVersion) | 7 | 2-7 | 0x00 0x07 |
| E2E (e2eVersion) | 2 | 2-3 | 0x00 0x02 |
| EncHeader (ehVersion) | 2 | - | 0x00 0x02 |
| MsgHeader (msgMaxVersion) | 2 | - | 0x00 0x02 |

### 1.2 Version Constants in Code
`c
#define SMP_CLIENT_VERSION      4
#define AGENT_VERSION           7
#define E2E_VERSION             2
#define EH_VERSION              2
#define MSG_HEADER_VERSION      2
`

---

## 2. Size Constants

### 2.1 Structure Sizes

| Structure | Size (bytes) | Notes |
|-----------|--------------|-------|
| EncMessageHeader | 123 | NOT 124! |
| MsgHeader (plaintext) | 88 | With padding |
| MsgHeader (content) | 79 | Without padding |
| HELLO (plaintext) | 12 | Minimal message |
| E2E Params | 140 | 2 X448 keys |
| X448 SPKI Key | 68 | 12 header + 56 raw |
| X25519 SPKI Key | 44 | 12 header + 32 raw |
| Ed25519 SPKI Key | 44 | 12 header + 32 raw |
| X448 Raw Key | 56 | |
| X25519 Raw Key | 32 | |
| AES-GCM IV | 16 | |
| AES-GCM Tag | 16 | |

### 2.2 Padding Sizes

| Message Type | Padded Size |
|--------------|-------------|
| AgentConfirmation (encConnInfo) | 14,832 bytes |
| HELLO / A_MSG | 15,840 bytes |
| ClientMessage (outer) | 15,904 bytes |

### 2.3 AAD Sizes

| AAD Type | Size | Composition |
|----------|------|-------------|
| Header AAD (rcAD) | 112 | 56 + 56 raw keys |
| Payload AAD | 235 | 112 + 123 (rcAD + emHeader) |

---

## 3. Wire Formats

### 3.1 EncRatchetMessage
`
  +---------------+-----------------+----------------+-------------------+
  | emHeader-len  | emHeader        | Payload AuthTag| Encrypted Payload |
  | (1 byte)      | (123 bytes)     | (16 bytes)     | (Tail-no prefix)  |
  | 0x7B          | [EncMsgHeader]  | [tag]          | [encrypted]       |
  +---------------+-----------------+----------------+-------------------+
`

### 3.2 EncMessageHeader (123 bytes)
`
  +-----------+--------+------------+-------------+------------------+
  | ehVersion | ehIV   | ehAuthTag  | ehBody-len  | ehBody           |
  | (2 bytes) | (16 B) | (16 bytes) | (1 byte)    | (88 bytes)       |
  | 00 02     | [iv]   | [tag]      | 58          | [encrypted hdr]  |
  +-----------+--------+------------+-------------+------------------+
  Total: 2 + 16 + 16 + 1 + 88 = 123 bytes
`

### 3.3 MsgHeader (88 bytes, plaintext)
`
  +------------+-------------+------------------+----------+----------+---------+
  | Word16 len | msgMaxVer   | msgDHRs          | msgPN    | msgNs    | Padding |
  | (2 bytes)  | (2 bytes)   | (1+68 bytes)     | (4 bytes)| (4 bytes)| (7 B)   |
  | 00 4F      | 00 02       | 44 [SPKI]        | [PN BE]  | [Ns BE]  | '#####' |
  +------------+-------------+------------------+----------+----------+---------+
  Total: 2 + 2 + 1 + 68 + 4 + 4 + 7 = 88 bytes
`

### 3.4 HELLO Plaintext (12 bytes)
`
  +-----+---------------------------+-----------+-----+
  | 'M' | sndMsgId (8 bytes BE)     | prevHash  | 'H' |
  | 4D  | 00 00 00 00 00 00 00 01   | len (2B)  | 48  |
  |     |                           | 00 00     |     |
  +-----+---------------------------+-----------+-----+
  Total: 1 + 8 + 2 + 1 = 12 bytes
`

### 3.5 E2E Params (140 bytes)
`
  +-----------+-------------+----------------+-------------+----------------+
  | e2eVer    | key1-len    | key1 (SPKI)    | key2-len    | key2 (SPKI)    |
  | (2 bytes) | (1 byte)    | (68 bytes)     | (1 byte)    | (68 bytes)     |
  | 00 02     | 44          | [SPKI key1]    | 44          | [SPKI key2]    |
  +-----------+-------------+----------------+-------------+----------------+
  Total: 2 + 1 + 68 + 1 + 68 = 140 bytes
`

---

## 4. KDF Parameters

### 4.1 X3DH Key Derivation
`
X3DH:
  Hash:   SHA512
  Salt:   64 zero bytes
  IKM:    DH1 || DH2 || DH3 (168 bytes)
  Info:   "SimpleXX3DH" (11 bytes)
  Output: 96 bytes
    [0:32]   header_key (hk)
    [32:64]  next_header_key (nhk)
    [64:96]  root_key (sk)
`

### 4.2 Root KDF
`
Root KDF:
  Hash:   SHA512
  Salt:   current root_key (32 bytes)
  IKM:    DH output (56 bytes)
  Info:   "SimpleXRootRatchet" (18 bytes)
  Output: 96 bytes
    [0:32]   new_root_key
    [32:64]  chain_key
    [64:96]  next_header_key
`

### 4.3 Chain KDF
`
Chain KDF:
  Hash:   SHA512
  Salt:   EMPTY (0 bytes!) <-- Important!
  IKM:    chain_key (32 bytes)
  Info:   "SimpleXChainRatchet" (19 bytes)
  Output: 96 bytes
    [0:32]   next_chain_key
    [32:64]  message_key
    [64:80]  header_iv   <-- FIRST!
    [80:96]  message_iv  <-- SECOND!
`

---

## 5. SPKI Key Formats

### 5.1 X448 SPKI (68 bytes)
`
Header (12 bytes):
  30 42 30 05 06 03 2b 65 6f 03 39 00

+ Raw key (56 bytes)
`

### 5.2 X25519 SPKI (44 bytes)
`
Header (12 bytes):
  30 2a 30 05 06 03 2b 65 6e 03 21 00

+ Raw key (32 bytes)
`

### 5.3 Ed25519 SPKI (44 bytes)
`
Header (12 bytes):
  30 2a 30 05 06 03 2b 65 70 03 21 00

+ Raw key (32 bytes)
`

---

## 6. Encoding Rules

### 6.1 Length Encoding

| Size | Encoding |
|------|----------|
| <= 254 bytes | 1-byte length prefix |
| > 254 bytes | 0xFF + Word16 BE prefix |
| Tail field | NO prefix! |

### 6.2 Word16 (Big Endian)
`
Value 68:  0x00 0x44
Value 79:  0x00 0x4F
Value 88:  0x00 0x58
Value 123: 0x00 0x7B
`

### 6.3 Word32 (Big Endian)
`
Value 0:   0x00 0x00 0x00 0x00
Value 1:   0x00 0x00 0x00 0x01
`

---

## 7. Maybe Encoding

### 7.1 Standard Maybe
`
Nothing:  '0' (0x30)
Just x:   '1' (0x31) + encoded x
`

### 7.2 Special Cases
`
queueMode (NOT standard Maybe!):
  Nothing:              (empty - zero bytes!)
  Just QMMessaging:     "M"
  Just QMSubscription:  "S"
`

### 7.3 Padding Functions
`
TWO different pad() functions exist:

Crypto/Lazy.hs (LazyByteString):
  - 8-byte Int64 length prefix
  - Used for: sbEncrypt (SecretBox)

Crypto.hs (strict ByteString):
  - 2-byte Word16 length prefix
  - Used for: encryptAEAD (AES-GCM)
  - THIS IS WHAT DOUBLE RATCHET USES!
`

---

*Quick Reference v1.0*  
*Last updated: January 25, 2026*
