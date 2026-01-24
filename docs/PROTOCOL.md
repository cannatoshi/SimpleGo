# SimpleGo Protocol Documentation

Complete documentation of the SimpleX SMP protocol implementation in SimpleGo.

---

## Overview

SimpleGo implements the SimpleX Messaging Protocol (SMP) for secure, decentralized messaging. This document describes the protocol layers, message flow, and implementation details.

---

## Protocol Stack

| Layer | Protocol | Description |
|-------|----------|-------------|
| 5 | Application | User messages, contacts |
| 4 | Agent Protocol | E2E encryption, connection management |
| 3 | SMP | Queue-based message delivery |
| 2 | TLS 1.3 | Transport security |
| 1 | TCP | Network transport |

---

## SMP Protocol

The SimpleX Messaging Protocol provides queue-based message delivery without user identifiers.

### Key Concepts

| Concept | Description |
|---------|-------------|
| Queue | Unidirectional message channel |
| Sender | Party that sends messages to queue |
| Recipient | Party that receives messages from queue |
| Relay | Server that hosts queues |

### Queue Addresses

Each queue has two addresses:

| Address Type | Used By | Contains |
|--------------|---------|----------|
| Recipient Address | Recipient | Queue ID, recipient keys |
| Sender Address | Sender | Queue ID, sender keys |

### SMP Commands

| Command | Direction | Description |
|---------|-----------|-------------|
| NEW | Client → Server | Create new queue |
| SUB | Client → Server | Subscribe to queue |
| SEND | Client → Server | Send message to queue |
| ACK | Client → Server | Acknowledge message receipt |
| OFF | Client → Server | Suspend queue |
| DEL | Client → Server | Delete queue |

### SMP Responses

| Response | Description |
|----------|-------------|
| OK | Command succeeded |
| ERR | Command failed with error code |
| MSG | Message delivered |
| NMSG | New message notification |

### SMP Command Format
`
<corrId> <queueId> <command> [parameters]
`

| Field | Size | Description |
|-------|------|-------------|
| corrId | 24 bytes | Correlation ID (base64) |
| queueId | 24 bytes | Queue ID (base64) |
| command | Variable | Command name |
| parameters | Variable | Command-specific data |

---

## Agent Protocol

The Agent Protocol provides end-to-end encryption and connection management on top of SMP.

### Connection States

| State | Description |
|-------|-------------|
| NEW | Connection created, not confirmed |
| PENDING | Invitation sent, waiting for confirmation |
| CONFIRMED | Connection confirmed, ready for messaging |
| ESTABLISHED | Fully established, bidirectional |
| DELETED | Connection deleted |

### Agent Messages

| Message | Direction | Description |
|---------|-----------|-------------|
| CONF | A → B | Connection confirmation |
| INFO | A ↔ B | Connection information |
| HELLO | A → B | Initial greeting |
| MSG | A ↔ B | User message |
| ACK | A ↔ B | Message acknowledgment |

### AgentConfirmation (CONF)

Sent to confirm a connection request.

| Field | Type | Description |
|-------|------|-------------|
| agentVersion | Word16 | Protocol version (7) |
| connType | Char | 'C' for confirmation |
| e2eEnabled | Char | '1' if E2E enabled |
| e2eVersion | Word16 | E2E protocol version (2) |
| e2eKeys | Keys | X448 public keys for E2E |
| encConnInfo | Encrypted | Encrypted connection info |

### HELLO Message

First message sent after connection confirmation.

| Field | Type | Description |
|-------|------|-------------|
| prevMsgHash | ByteString | Hash of previous message |
| messageBody | ByteString | Message content |

---

## E2E Encryption Protocol

End-to-end encryption uses the Double Ratchet algorithm with X3DH key agreement.

### Key Exchange (X3DH)

Initial key exchange establishes shared secrets.

| Step | Operation |
|------|-----------|
| 1 | Sender generates ephemeral X448 key pair |
| 2 | Sender performs 3 DH operations |
| 3 | Sender derives initial keys via HKDF |
| 4 | Sender sends public keys to recipient |
| 5 | Recipient performs same DH operations |
| 6 | Both parties have same shared secrets |

### Double Ratchet

Provides forward secrecy through continuous key evolution.

| Ratchet | Trigger | Derives |
|---------|---------|---------|
| Root | DH ratchet step | New root key, chain key, header key |
| Chain | Each message | Message key, new chain key, IVs |

### Message Encryption

Each message is encrypted in two layers.

| Layer | Key | Content |
|-------|-----|---------|
| Header | header_key | MsgHeader (sender's DH key, counters) |
| Body | message_key | Actual message content |

---

## Connection Establishment Flow

### Step 1: Create Invitation
`
Recipient:
1. Generate X448 key pairs (semi-permanent, ratchet)
2. Create queue on SMP server
3. Build invitation with server address and keys
4. Share invitation (QR code, link, etc.)
`

### Step 2: Accept Invitation
`
Sender:
1. Parse invitation
2. Extract server address and recipient keys
3. Connect to SMP server
4. Create sender queue
5. Perform X3DH key agreement
6. Send AgentConfirmation with encrypted queue info
`

### Step 3: Confirm Connection
`
Recipient:
1. Receive AgentConfirmation
2. Decrypt connection info
3. Extract sender's queue address
4. Perform X3DH (recipient side)
5. Initialize Double Ratchet state
6. Connection ready
`

### Step 4: Exchange Messages
`
Both parties:
1. Derive message keys via Chain KDF
2. Encrypt message header
3. Encrypt message body
4. Send via SMP queue
5. Receive and decrypt incoming messages
`

---

## Message Format

### Outgoing Message Structure
`
[SMP Header]
  [Correlation ID: 24 bytes]
  [Queue ID: 24 bytes]
  [Command: SEND]
[Agent Message]
  [Agent Header]
  [Encrypted Content]
    [EncRatchetMessage]
      [EncMessageHeader: 123 bytes]
      [Auth Tag: 16 bytes]
      [Encrypted Body: variable]
[Padding]
`

### EncRatchetMessage Structure
`
Offset  Size   Field
0       1      emHeaderLen (123)
1       123    emHeader (EncMessageHeader)
124     16     emAuthTag
140     REST   emBody (Tail encoded)
`

### EncMessageHeader Structure
`
Offset  Size   Field
0       2      ehVersion (2)
2       16     ehIV
18      16     ehAuthTag
34      1      ehBodyLen (88)
35      88     ehBody (encrypted MsgHeader)
`

### MsgHeader Structure
`
Offset  Size   Field
0       2      msgMaxVersion (2)
2       1      dhKeyLen (68)
3       68     msgDHRs (X448 SPKI)
71      4      msgPN
75      4      msgNs
79      9      padding
`

---

## Server Communication

### Connection Setup
`
1. DNS resolution of server hostname
2. TCP connection to server port (typically 5223)
3. TLS 1.3 handshake
4. SMP protocol version negotiation
`

### TLS Configuration

| Parameter | Value |
|-----------|-------|
| Protocol | TLS 1.3 |
| Certificate Verification | Enabled |
| Server Name Indication | Enabled |

### Keep-Alive

SMP connections use periodic keep-alive messages to maintain connection state.

| Parameter | Value |
|-----------|-------|
| Interval | 30 seconds |
| Timeout | 60 seconds |

---

## Error Handling

### SMP Errors

| Error | Code | Description |
|-------|------|-------------|
| AUTH | 1 | Authentication failed |
| NO_QUEUE | 2 | Queue does not exist |
| QUOTA | 3 | Queue quota exceeded |
| NO_MSG | 4 | No message available |
| LARGE_MSG | 5 | Message too large |
| INTERNAL | 6 | Server internal error |

### Agent Errors

| Error | Description |
|-------|-------------|
| A_DUPLICATE | Duplicate connection |
| A_PROHIBITED | Operation not allowed |
| A_MESSAGE | Message parsing error |
| A_CRYPTO | Cryptographic error |

### Recovery Strategies

| Error Type | Strategy |
|------------|----------|
| Network error | Reconnect with exponential backoff |
| Auth error | Re-authenticate or create new queue |
| Parse error | Log and skip message |
| Crypto error | Request message resend |

---

## Security Properties

### Confidentiality

- All messages encrypted with AES-256-GCM
- Keys derived from DH shared secrets
- Forward secrecy via Double Ratchet

### Integrity

- GCM authentication tags on all messages
- Message counters prevent replay
- Hash chains link messages

### Authentication

- X3DH provides mutual authentication
- Ed25519 signatures on queue operations
- TLS server authentication

### Privacy

- No user identifiers in protocol
- Queue IDs are random
- Servers cannot read message content
- Metadata minimization

---

## Protocol Versions

### SMP Versions

| Version | Features |
|---------|----------|
| 1-3 | Legacy versions |
| 4 | Current stable |
| 5+ | Reserved |

### Agent Versions

| Version | Features |
|---------|----------|
| 1-6 | Legacy versions |
| 7 | Current (E2E v2) |
| 8+ | Reserved |

### E2E Versions

| Version | Features |
|---------|----------|
| 1 | X25519 + AES-GCM |
| 2 | X448 + Double Ratchet |

---

## Implementation Notes

### Threading Model
`
Main Thread:
  - User interface
  - Message composition
  
Network Thread:
  - TLS connection management
  - Send/receive operations
  
Crypto Thread:
  - Key derivation
  - Encryption/decryption
`

### Buffer Management

| Buffer | Size | Purpose |
|--------|------|---------|
| Send buffer | 16 KB | Outgoing messages |
| Receive buffer | 16 KB | Incoming messages |
| Crypto buffer | 4 KB | Encryption workspace |

### Memory Considerations

- Ratchet state: ~300 bytes per connection
- TLS context: ~40 KB
- Message buffers: ~32 KB total

---

## Testing

### Unit Tests

| Component | Test Coverage |
|-----------|---------------|
| KDF functions | Input/output verification |
| Encryption | Known-answer tests |
| Encoding | Round-trip tests |

### Integration Tests

| Test | Description |
|------|-------------|
| Server connection | Connect to test server |
| Queue creation | Create and verify queue |
| Message delivery | Send and receive message |

### Verification Tools

| Tool | Purpose |
|------|---------|
| Python scripts | Crypto verification |
| Wireshark | Protocol analysis |
| Test server | Local testing |

---

## References

### Specifications

- SMP Protocol: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md
- Agent Protocol: https://github.com/simplex-chat/simplexmq/blob/stable/protocol/agent-protocol.md
- Double Ratchet: https://signal.org/docs/specifications/doubleratchet/
- X3DH: https://signal.org/docs/specifications/x3dh/

### Source Code

- SimpleX Haskell: https://github.com/simplex-chat/simplexmq
- SimpleX Chat: https://github.com/simplex-chat/simplex-chat

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
