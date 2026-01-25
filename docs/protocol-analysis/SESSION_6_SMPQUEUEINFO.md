# Session 6: SMPQueueInfo Encoding

## The Final Encoding Bugs

**Date:** January 24, 2026
**Version:** v0.1.26 to v0.1.28-alpha
**Bugs Fixed:** #10, #11, #12

---

## 📋 Table of Contents

1. [Session Overview](#1-session-overview)
2. [Contact Address Handshake Flow](#2-contact-address-handshake-flow)
3. [Bug #10: Port Encoding](#3-bug-10-port-encoding)
4. [Bug #11: smpQueues Count](#4-bug-11-smpqueues-count)
5. [Bug #12: queueMode Nothing](#5-bug-12-queuemode-nothing)
6. [SMPQueueInfo Complete Encoding](#6-smpqueueinfo-complete-encoding)
7. [Session Summary](#7-session-summary)
8. [Changelog](#8-changelog)

---

## 1. Session Overview

### 1.1 Starting Point

After Session 5's breakthrough with X448 byte order, all cryptographic values were verified. Yet A_MESSAGE persisted. This session focused on the SMPQueueInfo structure within the AgentConnInfoReply.

### 1.2 The Hunt Continues
`
Session 6 Start:
═══════════════════════════════════════════════════════════════════

✅ 9 bugs fixed (Sessions 4-5)
✅ All crypto verified (100% Python match)
✅ Server accepts messages
❌ App: A_MESSAGE persists

Focus: SMPQueueInfo encoding in AgentConnInfoReply

═══════════════════════════════════════════════════════════════════
`

---

## 2. Contact Address Handshake Flow

### 2.1 The Complete Flow
`
Contact Address Handshake (q=c):
═══════════════════════════════════════════════════════════════════

ESP32                    SMP Server              SimpleX App
  │                          │                        │
  │  QR/Link created         │                        │
  │◄─────────────────────────│                        │
  │                          │   App scans QR         │
  │                          │◄───────────────────────│
  │  agentInvitation         │                        │
  │◄─────────────────────────│  (E2E keys + KEM)     │
  │                          │                        │
  │  agentConfirmation (E2E) │                        │
  ├─────────────────────────►│  Server: OK ✅        │
  │                          │                        │
  │  HELLO (E2E)             │                        │
  ├─────────────────────────►│  Server: OK ✅        │
  │                          │                        │
  │                          │  App receives...       │
  │                          │  → error agent ❌      │

═══════════════════════════════════════════════════════════════════
`

### 2.2 What We Send

In AgentConnInfoReply, we include our SMPQueueInfo so the app knows how to reach us:
`
AgentConnInfoReply Structure:
├── 'D' tag (1 byte)
├── smpQueues (NonEmpty list of SMPQueueInfo)
│   ├── count (Word16 BE!)
│   └── SMPQueueInfo[]
└── connInfo (JSON)
`

---

## 3. Bug #10: Port Encoding

### 3.1 Discovery

The SMP server port was encoded with a length prefix instead of space separator.

### 3.2 Haskell Reference
`haskell
-- SMPServer encoding (Protocol.hs)
smpEncode SMPServer {host, port, keyHash} =
  smpEncode host <> " " <> port <> smpEncode keyHash
--                   ^^^
--                   Space separator, NOT length prefix!
`

### 3.3 Byte Analysis

**ESP32 sent (WRONG):**
`
63 6f 6d 04 35 32 32 33   = "com" + Length(4) + "5223"
        ^^
        Length prefix (WRONG!)
`

**Haskell expects (CORRECT):**
`
63 6f 6d 20 35 32 32 33   = "com" + Space(0x20) + "5223"
        ^^
        Space character (CORRECT!)
`

### 3.4 The Fix

**Before (WRONG):**
`c
// Port with length prefix
buf[p++] = (uint8_t)strlen(port_str);  // WRONG: Length prefix
memcpy(&buf[p], port_str, strlen(port_str));
p += strlen(port_str);
`

**After (CORRECT):**
`c
// Space + Port as string (no length prefix!)
buf[p++] = ' ';  // 0x20 Space separator
memcpy(&buf[p], port_str, strlen(port_str));
p += strlen(port_str);
`

### 3.5 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | Length prefix instead of space |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 4. Bug #11: smpQueues Count

### 4.1 Discovery

The smpQueues list count was encoded as 1 byte instead of Word16 BE.

### 4.2 Haskell Reference
`haskell
-- NonEmpty list encoding uses Word16 for count
instance Encoding a => Encoding (NonEmpty a) where
  smpEncode (x :| xs) = smpEncode @Word16 (fromIntegral $ 1 + length xs) <> ...
--                                ^^^^^^^
--                                Word16 BE for count!
`

### 4.3 Byte Analysis

**ESP32 sent (WRONG):**
`
01 ...   = Count 1 (1 byte)
^^
1-byte count (WRONG!)
`

**Haskell expects (CORRECT):**
`
00 01 ...   = Count 1 (Word16 BE)
^^^^^
2-byte count (CORRECT!)
`

### 4.4 The Fix

**Before (WRONG):**
`c
// List count as 1 byte
buf[p++] = 0x01;  // WRONG: 1-byte count
`

**After (CORRECT):**
`c
// List count as Word16 BE
buf[p++] = 0x00;  // High byte
buf[p++] = 0x01;  // Low byte
`

### 4.5 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | 1-byte instead of Word16 |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 5. Bug #12: queueMode Nothing

### 5.1 Discovery

For queueMode = Nothing, we were sending '0' (ASCII 0x30) instead of sending nothing at all.

### 5.2 Haskell Reference
`haskell
-- SMPQueueInfo encoding for clientVersion >= 4
smpEncode (SMPQueueInfo clientVersion ... queueMode)
  | clientVersion >= shortLinksSMPClientVersion = 
      addrEnc <> maybe "" smpEncode queueMode
--             ^^^^^^^^
--             Nothing = "" (empty string, zero bytes!)
`

### 5.3 Byte Analysis

**ESP32 sent (WRONG):**
`
... [dhPublicKey] 30 7b 22 76   = queueMode='0' + JSON start
                  ^^
                  '0' character (WRONG!)
`

**Haskell expects (CORRECT):**
`
... [dhPublicKey] 7b 22 76   = JSON start directly
                  ^^
                  No queueMode bytes (CORRECT!)
`

### 5.4 The Fix

**Before (WRONG):**
`c
// queueMode = Nothing (WRONG interpretation)
buf[p++] = '0';  // WRONG: '0' is for Maybe encoding, but we shouldn't send anything!
`

**After (CORRECT):**
`c
// queueMode = Nothing - send NOTHING!
// (no code here - just don't write anything)
// The "maybe" encoding means: Nothing = empty, Just x = encoded x
// We want Nothing, so we write zero bytes
`

### 5.5 Important Distinction
`
Maybe Encoding vs queueMode:
═══════════════════════════════════════════════════════════════════

Standard Maybe encoding (for e2eEncryption_, etc.):
  Nothing → '0' (ASCII 0x30)
  Just x  → '1' (ASCII 0x31) + encoded x

queueMode encoding (special case):
  Nothing → "" (empty, zero bytes!)
  Just QMMessaging → "M"
  Just QMSubscription → "S"

The "maybe" in the Haskell code means: encode if present, skip if absent.
It's NOT the standard Maybe '0'/'1' encoding!

═══════════════════════════════════════════════════════════════════
`

### 5.6 Status

| Item | Status |
|------|--------|
| Bug identified | ✅ |
| Root cause | Sent '0' instead of nothing |
| Fix implemented | ✅ |
| Status | ✅ **FIXED** |

---

## 6. SMPQueueInfo Complete Encoding

### 6.1 Full Structure
`
SMPQueueInfo (for clientVersion = 8):
═══════════════════════════════════════════════════════════════════

Offset  Size    Field               Encoding
──────────────────────────────────────────────────────────────────
0       2       smpClientVersion    Word16 BE (0x00 0x08 = 8)
2       1       hostCount           1 byte (0x01 = 1 host)
3       var     host                Length-prefixed string
var     1       space               0x20 (space separator!)
var     var     port                String (NO length prefix!)
var     1       keyHashLen          1 byte (0x20 = 32)
var     32      keyHash             Raw bytes
var     1       senderIdLen         1 byte
var     var     senderId            Raw bytes
var     1       dhPublicKeyLen      1 byte (0x2C = 44)
var     44      dhPublicKey         X25519 SPKI
var     0-1     queueMode           Nothing=empty, Just="M"/"S"

═══════════════════════════════════════════════════════════════════
`

### 6.2 Code Implementation
`c
int encode_smp_queue_info(uint8_t *buf, smp_queue_info_t *info) {
    int p = 0;
    
    // smpClientVersion (Word16 BE)
    buf[p++] = 0x00;
    buf[p++] = 0x08;  // Version 8
    
    // Host count (1 byte)
    buf[p++] = 0x01;  // 1 host
    
    // Host (length-prefixed)
    buf[p++] = strlen(info->host);
    memcpy(&buf[p], info->host, strlen(info->host));
    p += strlen(info->host);
    
    // SPACE separator (Bug #10 fix!)
    buf[p++] = ' ';  // 0x20
    
    // Port (NO length prefix!)
    memcpy(&buf[p], info->port, strlen(info->port));
    p += strlen(info->port);
    
    // keyHash (length + 32 bytes)
    buf[p++] = 32;
    memcpy(&buf[p], info->key_hash, 32);
    p += 32;
    
    // senderId (length + bytes)
    buf[p++] = info->sender_id_len;
    memcpy(&buf[p], info->sender_id, info->sender_id_len);
    p += info->sender_id_len;
    
    // dhPublicKey (length + X25519 SPKI)
    buf[p++] = 44;
    memcpy(&buf[p], info->dh_public_spki, 44);
    p += 44;
    
    // queueMode = Nothing (Bug #12 fix!)
    // Send NOTHING - don't write any bytes!
    
    return p;
}
`

---

## 7. Session Summary

### 7.1 Bugs Fixed in Session 6

| Bug # | Component | Issue | Fix |
|-------|-----------|-------|-----|
| 10 | Port encoding | Length prefix | Space separator |
| 11 | smpQueues count | 1-byte | Word16 BE |
| 12 | queueMode Nothing | '0' byte | Empty (nothing) |

### 7.2 Total Bug Count
`
Complete Bug List (12 bugs):
═══════════════════════════════════════════════════════════════════

Session 4: Bugs #1-8 (Wire format)
Session 5: Bug #9 (X448 byte order)
Session 6: Bugs #10-12 (SMPQueueInfo)

Total: 12 bugs found and fixed!

═══════════════════════════════════════════════════════════════════
`

### 7.3 Status After Session 6
`
After Session 6:
═══════════════════════════════════════════════════════════════════

✅ 12 bugs fixed
✅ All crypto verified (100% Python match)
✅ All wire format verified against Haskell source
✅ Server accepts messages
❌ App STILL shows A_MESSAGE

What remains? AES-GCM verification needed...

═══════════════════════════════════════════════════════════════════
`

---

## 8. Changelog

| Date | Change |
|------|--------|
| 2026-01-24 S6 | Session 6 started - SMPQueueInfo analysis |
| 2026-01-24 S6 | Bug #10 identified: Port encoding |
| 2026-01-24 S6 | Bug #11 identified: smpQueues count |
| 2026-01-24 S6 | Bug #12 identified: queueMode Nothing |
| 2026-01-24 S6 | All 3 bugs fixed |
| 2026-01-24 S6 | 12 total bugs fixed across Sessions 4-6 |
| 2026-01-24 S6 | Documentation v18 created |

---

*Document version: Session 6 Complete*
*Last updated: January 24, 2026*
*Total bugs fixed: 12*
