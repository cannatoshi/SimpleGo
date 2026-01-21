# SimpleX Messaging Protocol (SMP) - Implementation Guide

> Deep technical documentation for implementing SMP on embedded systems

---

## Table of Contents

1. [Protocol Overview](#protocol-overview)
2. [Message Layer Stack](#message-layer-stack)
3. [Agent Protocol](#agent-protocol)
4. [Peer Connection](#peer-connection)
5. [Base64 Encoding Variants](#base64-encoding-variants)
6. [Command Reference](#command-reference)

---

## Protocol Overview

The **SimpleX Messaging Protocol (SMP)** uses unidirectional message queues:

- **Sender** can only send messages
- **Recipient** can only receive messages
- **Server** cannot correlate sender and recipient

---

## Message Layer Stack

```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 1: TLS 1.3 Transport (ALPN: "smp/1")                     │
├─────────────────────────────────────────────────────────────────┤
│  Layer 2: SMP Transport Block (16KB padded)                     │
├─────────────────────────────────────────────────────────────────┤
│  Layer 3: SMP E2E Encryption (server DH)                        │
├─────────────────────────────────────────────────────────────────┤
│  Layer 4: SMP Client Message                                    │
├─────────────────────────────────────────────────────────────────┤
│  Layer 5: Contact DH Encryption (initial messages)              │
├─────────────────────────────────────────────────────────────────┤
│  Layer 6: Agent Protocol Message                                │
└─────────────────────────────────────────────────────────────────┘
```

---

## Agent Protocol

### Message Format (After Layer 5 Decryption)

```
[prefix]['_'][version:2 BE][type][body]
        ^^^^
        Find this delimiter first!
```

### Agent Message Types

| Type | Name | Description |
|------|------|-------------|
| `'C'` | AgentConfirmation | Connection confirmation |
| `'I'` | AgentInvitation | Reply queue + profile |
| `'M'` | AgentMsgEnvelope | Double Ratchet message |
| `'R'` | AgentRatchetKey | Key exchange |

### AgentConfirmation Format (Type 'C')

From Haskell source:
```haskell
AgentConfirmation {
    agentVersion :: Version,           -- 2 bytes BE
    e2eEncryption_ :: Maybe E2EParams, -- '0' or '1' + data
    encConnInfo :: ByteString          -- Encrypted profile/conn info
}
```

**Maybe Encoding:**
- `'0'` = Nothing (no data)
- `'1'` + data = Just (has data)

---

## Peer Connection

### Why Peer Connection?

Each SimpleX user has their own SMP server. To complete a connection:

1. **Our Server** — Where we receive messages
2. **Peer's Server** — Where we send confirmations/messages

```
┌──────────┐        ┌────────────────┐        ┌──────────┐
│  ESP32   │──TLS──>│ smp3 (our)     │<──TLS──│ SimpleX  │
│          │        └────────────────┘        │   App    │
│          │        ┌────────────────┐        │          │
│          │──TLS──>│ smp15 (peer's) │<───────│          │
└──────────┘        └────────────────┘        └──────────┘
```

### Peer Connection Flow

```c
// 1. Parse invitation to get peer server info
parse_smp_uri(invitation, &pending_peer);
// → pending_peer.host = "smp15.simplex.im"
// → pending_peer.port = 5223
// → pending_peer.queue_id = [24 bytes]
// → pending_peer.dh_public = [32 bytes]

// 2. Connect to peer's server
peer_connect("smp15.simplex.im", 5223);
// → TLS handshake
// → SMP handshake (ServerHello/ClientHello)

// 3. Send AgentConfirmation
send_agent_confirmation(contact);
// → SEND command to peer's queue_id
// → Server responds "OK"

// 4. Disconnect
peer_disconnect();
```

### SEND to Peer's Queue

```
SEND ' ' [flags] ' ' [encrypted_confirmation]
```

**Key Differences from Receiving:**
- **entityId** = peer's queue_id (NOT our queue!)
- **No signature** needed (we're sender, not recipient)
- **Encrypted** with peer's DH public key

---

## Base64 Encoding Variants

### CRITICAL: Different Contexts Use Different Encodings!

| Context | Encoding | Alphabet |
|---------|----------|----------|
| **Our Contact Links** | Base64URL | `-_` (no padding) |
| **Invitation URIs** | Standard Base64 | `+/=` |
| **Queue IDs** | Base64URL | `-_` |
| **DH Keys in Invitations** | Standard Base64 | `+/=` |

### DH Key Extraction Fix (v0.1.14)

**Problem:** DH Keys from Invitation URIs use Standard Base64!

```c
// Extract DH key from invitation
char *dh_start = strstr(uri, "dh=");
// dh= MCowBQYDK2VuAyEA+abc/xyz==
//                     ^   ^   ^^
//                     Standard Base64 chars!

// Convert to Base64URL before decoding
char dh_clean[64];
strcpy(dh_clean, dh_value);
int len = strlen(dh_clean);

// Strip '=' padding
while (len > 0 && dh_clean[len - 1] == '=') {
    dh_clean[--len] = '\0';
}

// Convert +/ to -_
for (int x = 0; x < len; x++) {
    if (dh_clean[x] == '+') dh_clean[x] = '-';
    if (dh_clean[x] == '/') dh_clean[x] = '_';
}

// Now decode as Base64URL
base64url_decode(dh_clean, peer->dh_public);
```

---

## Command Reference

### SMP Commands

| Command | EntityId | Signed? | Description |
|---------|----------|---------|-------------|
| NEW | empty | Yes | Create queue |
| SUB | recipientId | Yes | Subscribe |
| SEND | senderId | No | Send message |
| ACK | recipientId | Yes | Acknowledge |
| DEL | recipientId | Yes | Delete |

### SEND Command Format

```
"SEND" ' ' [flags] ' ' [body]
```

- **flags:** ASCII `'T'` or `'F'`
- **body:** Encrypted message content

---

## Connection Sequences

### Contact Address Flow (q=c)

```
SimpleX App                                ESP32
     │                                        │
     │  1. Scans https://simplex.chat/...     │
     │                                        │
     │  2. Connects to ESP32's server         │
     │     (smp3.simplexonflux.com)           │
     │                                        │
     │  3. SEND AgentInvitation ──────────────>
     │     [Type 'I'][Reply Queue][Profile]   │
     │                                        │
     │  4. ESP32 parses invitation            │
     │     - Peer server: smp15.simplex.im    │
     │     - Queue ID: ahjPk2jl...            │
     │     - DH Key: MCowBQ...                │
     │                                        │
     │  5. ESP32 connects to Peer's server    │
     │     (smp15.simplex.im)                 │
     │                                        │
     │  6. SEND AgentConfirmation             │
     │     <────────────────────────────────────
     │     Server: "OK"                       │
     │                                        │
     │  7. App shows "Connected" (pending)    │
```

---

## Performance (ESP32-S3)

| Operation | Time |
|-----------|------|
| TLS handshake | ~800ms |
| SMP handshake | ~100ms |
| Peer connect total | ~1000ms |
| SEND command | ~50ms |

---

## References

- [SMP Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [SimpleX Agent](https://github.com/simplex-chat/simplexmq/tree/stable/src/Simplex/Messaging/Agent)

---

*Last updated: January 21, 2026 — v0.1.14-alpha*
