# SimpleGo Wire Format Specification

Complete wire format documentation for the SimpleX SMP protocol implementation.

---

## Overview

This document describes the binary encoding format used by SimpleGo to communicate with SimpleX SMP servers and other SimpleX clients.

---

## Length Encoding Strategies

SimpleX uses three different length encoding strategies depending on context.

### Standard Length Encoding

Used for most fields with length <= 254 bytes.

| Length | Encoding |
|--------|----------|
| 0-254 | 1 byte containing the length |

### Large Length Encoding

Used for fields with length > 254 bytes.

| Length | Encoding |
|--------|----------|
| 255+ | 0xFF followed by Word16 BE |

Example: Length 300 = 0xFF 0x01 0x2C

### Tail Encoding

Used for the **last field** in a structure. No length prefix is added.

| Field Position | Encoding |
|----------------|----------|
| Last field | Raw bytes, no prefix |

**Important:** This is a critical encoding rule. Adding a length prefix to a Tail field will cause parsing errors.

---

## Numeric Encodings

### Word16 (Big Endian)

2 bytes, most significant byte first.

| Value | Encoding |
|-------|----------|
| 0 | 0x00 0x00 |
| 1 | 0x00 0x01 |
| 256 | 0x01 0x00 |
| 65535 | 0xFF 0xFF |

### Word32 (Big Endian)

4 bytes, most significant byte first.

| Value | Encoding |
|-------|----------|
| 0 | 0x00 0x00 0x00 0x00 |
| 1 | 0x00 0x00 0x00 0x01 |

---

## AgentConfirmation Message

The AgentConfirmation message confirms a connection request with E2E encryption parameters.

### Structure

| Offset | Size | Field | Encoding | Value/Description |
|--------|------|-------|----------|-------------------|
| 0 | 2 | agentVersion | Word16 BE | 7 |
| 2 | 1 | type | ASCII char | 'C' (0x43) |
| 3 | 1 | maybeE2E | ASCII char | '1' (0x31) |
| 4 | 2 | e2eVersion | Word16 BE | 2 |
| 6 | 1 | key1Len | 1 byte | 68 (0x44) |
| 7 | 68 | key1 | X448 SPKI | Our ratchet public key |
| 75 | 1 | key2Len | 1 byte | 68 (0x44) |
| 76 | 68 | key2 | X448 SPKI | Our ephemeral public key |
| 144 | REST | encConnInfo | **Tail** | Encrypted connection info |

**Total header size:** 144 bytes (before encConnInfo)

**Note:** encConnInfo uses Tail encoding - no length prefix!

### Padding

AgentConfirmation must be padded to exactly 14832 bytes:
`
[2 bytes: content length Word16 BE][content][padding with '#' characters]
`

---

## EncRatchetMessage

Encrypted message using the Double Ratchet protocol.

### Structure

| Offset | Size | Field | Encoding | Description |
|--------|------|-------|----------|-------------|
| 0 | 1 | emHeaderLen | 1 byte | 123 (0x7B) |
| 1 | 123 | emHeader | EncMessageHeader | Encrypted header |
| 124 | 16 | emAuthTag | Raw bytes | Body authentication tag |
| 140 | REST | emBody | **Tail** | Encrypted message body |

**Note:** emBody uses Tail encoding - no length prefix!

---

## EncMessageHeader

Encrypted message header (123 bytes total).

### Structure

| Offset | Size | Field | Encoding | Description |
|--------|------|-------|----------|-------------|
| 0 | 2 | ehVersion | Word16 BE | 2 |
| 2 | 16 | ehIV | Raw bytes | Header encryption IV |
| 18 | 16 | ehAuthTag | Raw bytes | Header authentication tag |
| 34 | 1 | ehBodyLen | 1 byte | 88 (0x58) |
| 35 | 88 | ehBody | Raw bytes | Encrypted MsgHeader |

**Total size:** 2 + 16 + 16 + 1 + 88 = 123 bytes

**Important:** ehBodyLen is 1 byte (not Word16), and total size is 123 (not 124).

---

## MsgHeader

Plaintext message header (88 bytes total), encrypted to produce ehBody.

### Structure

| Offset | Size | Field | Encoding | Description |
|--------|------|-------|----------|-------------|
| 0 | 2 | msgMaxVersion | Word16 BE | 2 |
| 2 | 1 | dhKeyLen | 1 byte | 68 (0x44) |
| 3 | 68 | msgDHRs | X448 SPKI | Sender's ratchet public key |
| 71 | 4 | msgPN | Word32 BE | Previous chain message count |
| 75 | 4 | msgNs | Word32 BE | Current message number |
| 79 | 9 | padding | Zero bytes | Padding to 88 bytes |

**Total size:** 2 + 1 + 68 + 4 + 4 + 9 = 88 bytes

**Important:** dhKeyLen is 1 byte (not Word16).

---

## SMPQueueInfo

Queue information for establishing message queues.

### Structure

| Field | Encoding | Description |
|-------|----------|-------------|
| clientVersion | Word16 BE | 8 |
| hostCount | 1 byte | Number of hosts (usually 1) |
| host | Length-prefixed string | Server hostname |
| port | Length-prefixed string | Server port |
| keyHash | 1-byte length + 32 bytes | Server key hash |
| senderId | 1-byte length + N bytes | Queue sender ID (base64) |
| dhPublicKey | 1-byte length (44) + key | X25519 SPKI public key |
| queueMode | Optional | Queue mode flag |

### Host Encoding
`
[1 byte: host length][host string bytes]
`

### Port Encoding
`
[1 byte: port length][port string bytes]
`

**Important:** Port uses a length byte, not a space character!

### queueMode Encoding

| Value | Encoding |
|-------|----------|
| Nothing | Empty (0 bytes) |
| Just QMMessaging | "M" (1 byte) |
| Just QMSubscription | "S" (1 byte) |

**Important:** queueMode Nothing means send nothing at all, not '0' or any other byte!

---

## smpQueues List

List of SMPQueueInfo structures.

### Structure

| Field | Encoding | Description |
|-------|----------|-------------|
| count | Word16 BE | Number of queues |
| queues | Repeated SMPQueueInfo | Queue info structures |

**Important:** Count is Word16 BE (2 bytes), not 1 byte!

---

## SPKI Key Formats

### X448 SPKI (68 bytes)
`
Bytes 0-11:  30 42 30 05 06 03 2b 65 6f 03 39 00  (SPKI header)
Bytes 12-67: [56 bytes raw X448 public key]
`

### X25519 SPKI (44 bytes)
`
Bytes 0-11:  30 2a 30 05 06 03 2b 65 6e 03 21 00  (SPKI header)
Bytes 12-43: [32 bytes raw X25519 public key]
`

---

## HELLO Message

Initial message sent after connection establishment.

### Structure

The HELLO message body contains:

| Field | Encoding | Description |
|-------|----------|-------------|
| prevMsgHash | Word16 BE length + 32 bytes | Previous message hash |
| messageBody | Tail | Actual message content |

**Important:** prevMsgHash uses Word16 BE length prefix (not 1 byte).

### Padding

HELLO messages must be padded to exactly 15840 bytes.

---

## Padding Format

All padded messages use this format:
`
[2 bytes: content length Word16 BE][content bytes][padding]
`

Padding character: '#' (0x23)

### Padding Sizes

| Message Type | Total Padded Size |
|--------------|-------------------|
| AgentConfirmation | 14832 bytes |
| HELLO | 15840 bytes |

### Padding Code Example
`c
int smp_add_padding(uint8_t *buffer, size_t content_len, size_t target_size) {
    // Write content length as Word16 BE
    buffer[0] = (content_len >> 8) & 0xFF;
    buffer[1] = content_len & 0xFF;
    
    // Content is already at buffer + 2
    
    // Fill remaining space with '#'
    size_t padding_start = 2 + content_len;
    size_t padding_len = target_size - padding_start;
    memset(buffer + padding_start, '#', padding_len);
    
    return target_size;
}
`

---

## AAD (Additional Authenticated Data)

### Header AAD (rcAD) - 112 bytes

Used for encrypting the MsgHeader.
`
rcAD = our_ratchet_key_raw (56 bytes) || peer_ratchet_key_raw (56 bytes)
`

**Important:** Use RAW keys (56 bytes each), not SPKI-encoded keys (68 bytes).

### Payload AAD - 235 bytes

Used for encrypting the message body.
`
payload_aad = rcAD (112 bytes) || emHeader (123 bytes)
`

**Important:** Size is 235 bytes (112 + 123), not 236.

---

## Common Encoding Mistakes

| Mistake | Incorrect | Correct |
|---------|-----------|---------|
| E2E key length | Word16 BE | 1 byte |
| MsgHeader DH key length | Word16 BE | 1 byte |
| ehBody length | Word16 BE | 1 byte |
| emHeader length | Word16 BE (124) | 1 byte (123) |
| prevMsgHash length | 1 byte | Word16 BE |
| Port encoding | Space character | Length byte |
| smpQueues count | 1 byte | Word16 BE |
| queueMode Nothing | '0' byte | Empty (nothing) |
| Tail field | With length prefix | No prefix |
| rcAD keys | SPKI (68 bytes) | Raw (56 bytes) |
| Payload AAD size | 236 bytes | 235 bytes |

---

## Byte Order Summary

| Field Type | Byte Order |
|------------|------------|
| Word16 | Big Endian |
| Word32 | Big Endian |
| X448 keys | Little Endian (reversed from wolfSSL) |
| Strings | As-is (UTF-8) |
| Raw bytes | As-is |

---

## Message Flow Example

### Sending AgentConfirmation
`
1. Build header (144 bytes):
   - agentVersion: 0x00 0x07
   - type: 0x43
   - maybeE2E: 0x31
   - e2eVersion: 0x00 0x02
   - key1Len: 0x44
   - key1: [68 bytes X448 SPKI]
   - key2Len: 0x44
   - key2: [68 bytes X448 SPKI]

2. Build encConnInfo (Tail, no length prefix):
   - EncRatchetMessage containing encrypted SMPQueueInfo

3. Concatenate:
   - header || encConnInfo

4. Add padding to 14832 bytes:
   - [Word16 BE length][content][### padding]

5. Send over TLS
`

### Sending Encrypted Message
`
1. Derive keys using Chain KDF

2. Build MsgHeader (88 bytes):
   - msgMaxVersion: 0x00 0x02
   - dhKeyLen: 0x44
   - msgDHRs: [68 bytes X448 SPKI]
   - msgPN: [4 bytes Word32 BE]
   - msgNs: [4 bytes Word32 BE]
   - padding: [9 zero bytes]

3. Encrypt MsgHeader with header_key and header_iv
   - AAD: rcAD (112 bytes)
   - Output: ehBody (88 bytes) + ehAuthTag (16 bytes)

4. Build EncMessageHeader (123 bytes):
   - ehVersion: 0x00 0x02
   - ehIV: [16 bytes]
   - ehAuthTag: [16 bytes]
   - ehBodyLen: 0x58
   - ehBody: [88 bytes]

5. Encrypt message body with message_key and message_iv
   - AAD: payload_aad (235 bytes)
   - Output: emBody + emAuthTag

6. Build EncRatchetMessage:
   - emHeaderLen: 0x7B
   - emHeader: [123 bytes]
   - emAuthTag: [16 bytes]
   - emBody: [Tail, no length prefix]
`

---

## Verification Checklist

Before sending a message, verify:

- [ ] All Word16 values are Big Endian
- [ ] All Word32 values are Big Endian
- [ ] Key lengths use 1-byte prefix (not Word16)
- [ ] emHeader size is 123 bytes
- [ ] ehBody size is 88 bytes
- [ ] Tail fields have no length prefix
- [ ] rcAD uses raw keys (56 bytes each)
- [ ] Payload AAD is exactly 235 bytes
- [ ] Padding reaches target size
- [ ] queueMode Nothing sends no bytes

---

## References

- SimpleX Protocol: https://github.com/simplex-chat/simplexmq
- Agent Protocol: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/agent-protocol.md
- SMP Protocol: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
