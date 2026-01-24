# SimpleGo Bug Tracker

Documentation of all bugs discovered and fixed during SimpleGo development.

---

## Summary

| Category | Count | Status |
|----------|-------|--------|
| Length Encoding | 5 | All Fixed |
| Size Calculation | 2 | All Fixed |
| KDF Output Order | 2 | All Fixed |
| Byte Order | 1 | Fixed |
| Format Encoding | 2 | All Fixed |
| **Total** | **12** | **All Fixed** |

---

## Bug #1: E2E Key Length Prefix

**Version:** v0.1.15-alpha
**Category:** Length Encoding
**Severity:** Critical
**Status:** Fixed

### Description

E2E encryption keys in AgentConfirmation were encoded with Word16 BE length prefix instead of 1-byte length prefix.

### Incorrect Implementation
`c
// Wrong: Word16 BE prefix
buffer[offset++] = 0x00;
buffer[offset++] = 0x44;  // 68 as Word16 BE
memcpy(buffer + offset, spki_key, 68);
`

### Correct Implementation
`c
// Correct: 1-byte prefix
buffer[offset++] = 0x44;  // 68 as single byte
memcpy(buffer + offset, spki_key, 68);
`

### Impact

Server accepted message but app failed to parse E2E parameters.

---

## Bug #2: prevMsgHash Length Prefix

**Version:** v0.1.15-alpha
**Category:** Length Encoding
**Severity:** Critical
**Status:** Fixed

### Description

The prevMsgHash field in HELLO messages used 1-byte length prefix instead of Word16 BE.

### Incorrect Implementation
`c
// Wrong: 1-byte prefix
buffer[offset++] = 0x20;  // 32 as single byte
memcpy(buffer + offset, prev_hash, 32);
`

### Correct Implementation
`c
// Correct: Word16 BE prefix
buffer[offset++] = 0x00;
buffer[offset++] = 0x20;  // 32 as Word16 BE
memcpy(buffer + offset, prev_hash, 32);
`

### Impact

HELLO message parsing failed on receiving side.

---

## Bug #3: MsgHeader DH Key Length

**Version:** v0.1.15-alpha
**Category:** Length Encoding
**Severity:** Critical
**Status:** Fixed

### Description

The DH key in MsgHeader (msgDHRs) used Word16 BE length prefix instead of 1-byte.

### Incorrect Implementation
`c
// Wrong: Word16 BE prefix
buffer[offset++] = 0x00;
buffer[offset++] = 0x44;  // 68 as Word16 BE
`

### Correct Implementation
`c
// Correct: 1-byte prefix
buffer[offset++] = 0x44;  // 68 as single byte
`

### Impact

MsgHeader size was 89 bytes instead of 88 bytes, causing encryption to fail.

---

## Bug #4: ehBody Length Prefix

**Version:** v0.1.15-alpha
**Category:** Length Encoding
**Severity:** Critical
**Status:** Fixed

### Description

The ehBody field in EncMessageHeader used Word16 BE length prefix instead of 1-byte.

### Incorrect Implementation
`c
// Wrong: Word16 BE prefix for ehBodyLen
buffer[offset++] = 0x00;
buffer[offset++] = 0x58;  // 88 as Word16 BE
`

### Correct Implementation
`c
// Correct: 1-byte prefix for ehBodyLen
buffer[offset++] = 0x58;  // 88 as single byte
`

### Impact

EncMessageHeader was 124 bytes instead of 123 bytes.

---

## Bug #5: emHeader Size Calculation

**Version:** v0.1.15-alpha
**Category:** Size Calculation
**Severity:** Critical
**Status:** Fixed

### Description

EncMessageHeader size was calculated as 124 bytes instead of correct 123 bytes.

### Incorrect Calculation
`
ehVersion (2) + ehIV (16) + ehAuthTag (16) + ehBodyLen (2) + ehBody (88) = 124
`

### Correct Calculation
`
ehVersion (2) + ehIV (16) + ehAuthTag (16) + ehBodyLen (1) + ehBody (88) = 123
`

### Impact

All downstream size calculations were wrong, causing parser errors.

---

## Bug #6: Payload AAD Size

**Version:** v0.1.15-alpha
**Category:** Size Calculation
**Severity:** Critical
**Status:** Fixed

### Description

Payload AAD was calculated as 236 bytes instead of correct 235 bytes.

### Incorrect Calculation
`c
// Wrong: Used 124 for emHeader size
size_t payload_aad_size = 112 + 124;  // = 236
`

### Correct Calculation
`c
// Correct: emHeader is 123 bytes
size_t payload_aad_size = 112 + 123;  // = 235
`

### Impact

AES-GCM authentication tag verification failed on receiving side.

---

## Bug #7: Root KDF Output Order

**Version:** v0.1.15-alpha
**Category:** KDF Output Order
**Severity:** Critical
**Status:** Fixed

### Description

The Root KDF output bytes were split in wrong order.

### Incorrect Implementation
`c
// Wrong order
memcpy(chain_key, output, 32);
memcpy(new_root_key, output + 32, 32);
memcpy(next_header_key, output + 64, 32);
`

### Correct Implementation
`c
// Correct order
memcpy(new_root_key, output, 32);        // [0:32]
memcpy(chain_key, output + 32, 32);      // [32:64]
memcpy(next_header_key, output + 64, 32); // [64:96]
`

### Impact

All derived keys were wrong, causing complete encryption failure.

---

## Bug #8: Chain KDF IV Order

**Version:** v0.1.15-alpha
**Category:** KDF Output Order
**Severity:** Critical
**Status:** Fixed

### Description

The header_iv and message_iv from Chain KDF output were swapped.

### Incorrect Implementation
`c
// Wrong: IVs swapped
memcpy(message_iv, output + 64, 16);   // [64:80]
memcpy(header_iv, output + 80, 16);    // [80:96]
`

### Correct Implementation
`c
// Correct: header_iv FIRST, then message_iv
memcpy(header_iv, output + 64, 16);    // [64:80]
memcpy(message_iv, output + 80, 16);   // [80:96]
`

### Impact

Header encrypted with wrong IV, decryption failed.

---

## Bug #9: wolfSSL X448 Byte Order

**Version:** v0.1.15-alpha
**Category:** Byte Order
**Severity:** Critical
**Status:** Fixed

### Description

wolfSSL exports X448 keys in reversed byte order compared to SimpleX expectation.

### Problem
`
wolfSSL output:  [byte_55][byte_54]...[byte_1][byte_0]
SimpleX expects: [byte_0][byte_1]...[byte_54][byte_55]
`

### Solution
`c
void smp_x448_reverse_bytes(uint8_t *data, size_t len) {
    for (size_t i = 0; i < len / 2; i++) {
        uint8_t tmp = data[i];
        data[i] = data[len - 1 - i];
        data[len - 1 - i] = tmp;
    }
}

// Apply after key generation and DH operations
smp_x448_reverse_bytes(public_key, 56);
smp_x448_reverse_bytes(private_key, 56);
smp_x448_reverse_bytes(shared_secret, 56);
`

### Impact

All DH calculations produced wrong shared secrets.

---

## Bug #10: SMPQueueInfo Port Encoding

**Version:** v0.1.15-alpha
**Category:** Format Encoding
**Severity:** Critical
**Status:** Fixed

### Description

Port field in SMPQueueInfo was separated with space character instead of length prefix.

### Incorrect Implementation
`c
// Wrong: Space separator
buffer[offset++] = ' ';  // 0x20
memcpy(buffer + offset, port_str, port_len);
`

### Correct Implementation
`c
// Correct: Length prefix
buffer[offset++] = port_len;
memcpy(buffer + offset, port_str, port_len);
`

### Impact

SMPQueueInfo parsing failed, connection could not be established.

---

## Bug #11: smpQueues List Count

**Version:** v0.1.15-alpha
**Category:** Format Encoding
**Severity:** Critical
**Status:** Fixed

### Description

The smpQueues list count was encoded as 1 byte instead of Word16 BE.

### Incorrect Implementation
`c
// Wrong: 1-byte count
buffer[offset++] = 0x01;  // Count as single byte
`

### Correct Implementation
`c
// Correct: Word16 BE count
buffer[offset++] = 0x00;
buffer[offset++] = 0x01;  // Count as Word16 BE
`

### Impact

Queue list parsing failed when count > 0.

---

## Bug #12: queueMode Nothing Encoding

**Version:** v0.1.15-alpha
**Category:** Format Encoding
**Severity:** Medium
**Status:** Fixed

### Description

queueMode with value Nothing was encoded as '0' byte instead of empty (no bytes).

### Incorrect Implementation
`c
// Wrong: Send '0' for Nothing
if (queue_mode == QUEUE_MODE_NOTHING) {
    buffer[offset++] = '0';
}
`

### Correct Implementation
`c
// Correct: Send nothing for Nothing
if (queue_mode == QUEUE_MODE_NOTHING) {
    // Don't add any bytes
}
`

### queueMode Encoding Table

| Value | Encoding |
|-------|----------|
| Nothing | (empty - 0 bytes) |
| Just QMMessaging | 'M' (1 byte) |
| Just QMSubscription | 'S' (1 byte) |

### Impact

Parser encountered unexpected byte, causing parse error.

---

## Current Investigation

### Potential Bug #13: Tail Field Length Prefix

**Status:** Investigating
**Severity:** Unknown

### Hypothesis

The A_MESSAGE parsing error may be caused by adding length prefixes to Tail-encoded fields.

### Fields Under Investigation

| Structure | Field | Current | Suspected Correct |
|-----------|-------|---------|-------------------|
| AgentConfirmation | encConnInfo | With length? | Tail (no prefix) |
| EncRatchetMessage | emBody | With length? | Tail (no prefix) |

### Evidence

- All cryptographic operations verified correct
- Server accepts messages (OK response)
- App fails with A_MESSAGE parsing error
- Error indicates parsing issue, not crypto issue

### Next Steps

1. Review Haskell source for Tail encoding
2. Verify encConnInfo encoding
3. Verify emBody encoding
4. Test without length prefixes

---

## Bug Discovery Timeline

| Date | Bug # | Description |
|------|-------|-------------|
| Jan 20, 2026 | #1-#6 | Initial wire format analysis |
| Jan 21, 2026 | #7-#8 | KDF output verification |
| Jan 22, 2026 | #9 | wolfSSL byte order discovery |
| Jan 23, 2026 | #10-#12 | SMPQueueInfo encoding fixes |
| Jan 24, 2026 | #13? | Tail encoding investigation |

---

## Verification Methods

### Python Reference Implementation

All KDF and encryption operations verified using Python cryptography library.

### Haskell Source Comparison

Wire format verified by comparing with SimpleX Haskell source code.

### Live Server Testing

Messages sent to production SimpleX servers to verify acceptance.

---

## Lessons Learned

1. **Length encoding varies by context** - Don't assume Word16 or 1-byte universally
2. **KDF output order matters** - Always verify against specification
3. **Library byte order may differ** - Test with known vectors
4. **Tail encoding has no prefix** - Last field in structure needs no length
5. **Verify each layer separately** - Isolate crypto from encoding issues

---

## References

- SimpleX Protocol: https://github.com/simplex-chat/simplexmq
- Agent Protocol: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/agent-protocol.md

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
