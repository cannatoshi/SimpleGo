# SimpleGo Bug Tracker

> All bugs discovered and fixed during SimpleGo development

---

## Summary

| Category | Count | Status |
|----------|-------|--------|
| Length Prefix Bugs | 7 | All fixed |
| KDF Order Bugs | 2 | All fixed |
| Crypto Library Bugs | 1 | Fixed |
| Format Bugs | 2 | All fixed |
| **Total** | **12** | **All fixed** |

---

## Bug #1: E2E Key Length Prefix

**Session:** 4 | **Severity:** Critical

Wrong: Word16 BE (0x00 0x44)
Correct: 1 byte (0x44)

---

## Bug #2: HELLO prevMsgHash Length

**Session:** 4 | **Severity:** Critical

Wrong: 1 byte
Correct: Word16 BE

---

## Bug #3: MsgHeader DH Key Length

**Session:** 4 | **Severity:** Critical

Wrong: Word16 BE
Correct: 1 byte (0x44)

---

## Bug #4: ehBody Length Prefix

**Session:** 4 | **Severity:** Critical

Wrong: Word16 BE (0x00 0x58)
Correct: 1 byte (0x58)

---

## Bug #5: emHeader Length Prefix

**Session:** 4 | **Severity:** Critical

Wrong: Word16 BE (0x00 0x7C = 124)
Correct: 1 byte (0x7B = 123)

---

## Bug #6: Payload AAD Size

**Session:** 4 | **Severity:** Critical

Wrong: 236 bytes
Correct: 235 bytes (112 + 123)

---

## Bug #7: KDF Root Output Order

**Session:** 4 | **Severity:** Critical
```
HKDF output (96 bytes):
[0-31]  = new_root_key
[32-63] = chain_key
[64-95] = next_header_key
```

---

## Bug #8: Chain KDF IV Order

**Session:** 4 | **Severity:** Critical
```
[64-79] = header_iv (FIRST!)
[80-95] = msg_iv (SECOND!)
```

---

## Bug #9: wolfSSL X448 Byte Order

**Session:** 4-5 | **Severity:** Critical

wolfSSL exports X448 keys in reversed byte order.
Solution: reverse_bytes() on all key operations.

---

## Bug #10: SMPQueueInfo Port Encoding

**Session:** 6 | **Severity:** Critical

Wrong: Space character (0x20)
Correct: Length prefix byte

---

## Bug #11: smpQueues List Count

**Session:** 6 | **Severity:** Critical

Wrong: 1 byte (0x01)
Correct: Word16 BE (0x00 0x01)

---

## Bug #12: queueMode for Nothing

**Session:** 6 | **Severity:** Medium

Wrong: Send '0' character
Correct: Send nothing (empty)

---

## Current Investigation

### Potential Bug #13: Tail Length Prefix

**Status:** Investigating

Hypothesis: We may be adding length prefixes before Tail fields.

Fields to check:
- encConnInfo in AgentConfirmation
- emBody in EncRatchetMessage

---

*Last updated: January 24, 2026 - v0.1.15-alpha*
