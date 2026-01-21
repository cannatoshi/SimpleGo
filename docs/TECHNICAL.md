# SimpleGo Technical Documentation

> Key learnings, discoveries, and implementation decisions

---

## Critical Discoveries

### Discovery #1-15: Previous Versions

See earlier documentation for discoveries about TLS, handshake, crypto_box, URL encoding, and Agent Protocol parsing.

---

### Discovery #16: Standard Base64 in Invitation URIs (v0.1.14)

**Problem:** DH Keys from Invitation URIs not decoding properly.

**Finding:** Invitation URIs use **Standard Base64** (`+`, `/`, `=`), NOT Base64URL (`-`, `_`)!

**Our Contact Links:** Base64URL (no `+`, `/`, `=`)
**Peer's Invitation:** Standard Base64 (has `+`, `/`, `=`)

**Solution:**
```c
// Convert Standard Base64 → Base64URL before decoding
while (len > 0 && dh[len - 1] == '=') dh[--len] = '\0';
for (int i = 0; i < len; i++) {
    if (dh[i] == '+') dh[i] = '-';
    if (dh[i] == '/') dh[i] = '_';
}
```

---

### Discovery #17: AgentConfirmation Format (v0.1.14)

**Finding:** AgentConfirmation structure from Haskell:

```haskell
AgentConfirmation {
    agentVersion :: Version,           -- 2 bytes Big Endian
    e2eEncryption_ :: Maybe E2EParams, -- Maybe encoded
    encConnInfo :: ByteString          -- Encrypted connection info
}
```

**Wire Format:**
```
[version:2 BE]['C'][maybe_e2e][encConnInfo]
```

---

### Discovery #18: Maybe Encoding (v0.1.14)

**Finding:** Haskell's `Maybe` type is encoded as:

| Value | Encoding |
|-------|----------|
| `Nothing` | `'0'` (single byte) |
| `Just x` | `'1'` + encoded x |

**Example:**
```c
// Nothing (no E2E params)
buf[0] = '0';

// Just params
buf[0] = '1';
memcpy(&buf[1], e2e_params, params_len);
```

---

### Discovery #19: Separate Peer Connection (v0.1.14)

**Finding:** Each peer has their own SMP server. To complete connection:

1. **Receive** invitation on OUR server
2. **Send** confirmation to PEER's server (different host!)

```
ESP32 ──TLS──> smp3.simplexonflux.com (our server)
      ──TLS──> smp15.simplex.im (peer's server)
```

**Implication:** Need separate TLS connection for peer communication!

---

### Discovery #20: SEND to Peer (v0.1.14)

**Finding:** Sending to peer's queue:

| Aspect | Receiving (SUB) | Sending to Peer |
|--------|-----------------|-----------------|
| Server | Our server | Peer's server |
| EntityId | recipientId | peer's queue_id |
| Signature | Required | Not needed |
| Auth | Our keys | None |

```c
// SEND to peer - no signature!
snprintf(cmd, sizeof(cmd), "SEND T ");
// entityId = peer's queue_id
// No Ed25519 signature needed
```

---

## Debugging Journey (Updated)

```
Timeline:
────────────────────────────────────────────────────────────────────────────────>

[TLS] [AUTH] [E2E] [LINKS] [AGENT] [TYPE] [PEER] [CONFIRM] [MODULAR]
  │      │     │      │       │      │      │       │         │
  ▼      ▼     ▼      ▼       ▼      ▼      ▼       ▼         ▼
TLS  libsodium E2E   Links  Agent  Type  Peer   CONF      8 modules
1.3   works  decrypt work  parse  fix   TLS    sent      main.c
                                               "OK"       ~350
```

---

## Architecture Evolution

### Before (v0.1.0 - v0.1.13)

```
┌────────────────────────────────────┐
│  main.c (~1800 lines)              │
│  ├── All structures                │
│  ├── All crypto                    │
│  ├── All network                   │
│  ├── All contacts                  │
│  ├── All parsing                   │
│  └── Everything else               │
└────────────────────────────────────┘
```

### After (v0.1.14+)

```
┌────────────────────────────────────┐
│  main.c (~350 lines)               │
│  └── App flow only                 │
├────────────────────────────────────┤
│  smp_types.h    │  smp_globals.c   │
│  smp_utils.c    │  smp_crypto.c    │
│  smp_network.c  │  smp_contacts.c  │
│  smp_parser.c   │  smp_peer.c      │
└────────────────────────────────────┘
```

---

## Lessons Learned (v0.1.14)

### 1. Base64 Variants Matter

Different parts of SimpleX use different Base64 variants:
- Our links: Base64URL
- Peer invitations: Standard Base64

Always check which variant is expected!

### 2. Peer = Different Server

Don't assume all communication goes through one server. Each user has their own server.

### 3. SEND Doesn't Need Signature

When sending to peer's queue, we're the sender (not recipient). No auth needed.

### 4. Modular Code = Faster Development

After refactoring:
- Easier to find code
- Faster compilation
- Better testing

---

## Quick Reference

### Base64 Conversion

```c
// Standard → URL
'+' → '-'
'/' → '_'
'=' → strip

// URL → Standard (if needed)
'-' → '+'
'_' → '/'
// add '=' padding
```

### Agent Message Types

| Type | Purpose |
|------|---------|
| `'C'` | Confirmation (we send) |
| `'I'` | Invitation (we receive) |
| `'M'` | Message (future) |
| `'R'` | Ratchet (future) |

### Maybe Encoding

```c
Nothing → '0'
Just x  → '1' + x
```

---

## Current Challenge: encConnInfo

Server accepts our Confirmation with "OK", but App doesn't show "Connected".

**Hypothesis:** `encConnInfo` needs proper content:
- Encrypted with peer's DH?
- Contains profile data?
- Ratchet initialization?

**Next:** Analyze Haskell `encConnInfo` encoding.

---

*Last updated: January 21, 2026 — v0.1.14-alpha*
