# Changelog

All notable changes to SimpleGo are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Planned
- Reply Queue URI Parser
- Multi-Server Support
- AgentConfirmation Builder
- Double Ratchet Implementation

---

## [0.1.12-alpha] - 2026-01-21

### 🔐 Agent Protocol Parsing + Client Message Decryption!

Full message layer stack decoded — ESP32 now sees peer's profile and reply queue!

### Added
- **Client Message Decryption (Layer 5)** — Second crypto_box layer with contact's DH key
- **Agent Protocol Parsing (Layer 6)** — Version + Type + Body structure
- **AgentInvitation Detection** — Type 'I' messages recognized
- **Reply Queue URI Extraction** — Peer's SMP server + queue visible
- **Peer Profile Visibility** — ConnInfo with username extracted
- **`decrypt_client_msg()`** — DH decryption for initial messages
- **`parse_agent_message()`** — Agent protocol parser

### Fixed
- **CRITICAL: Contact Link URL Encoding** — DH Key must be Base64URL (not Standard!)
- **Double Encoding for `=`** — Padding `=` → `%3D` → `%253D`

### Removed
- `base64_pre_encode()` — No longer needed
- `base64_std_encode()` — Replaced with Base64URL
- `parse_smp_client_header()` — Refactored
- `parse_agent_envelope()` — Merged into parse_agent_message

### Technical Details

**Message Layer Stack (Complete):**
```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 1: TLS 1.3 Transport                                     │
│  └── ALPN: "smp/1", ChaCha20-Poly1305                          │
├─────────────────────────────────────────────────────────────────┤
│  Layer 2: SMP Transport Block                                   │
│  └── [2-byte transmissionLength] [content] [padding to 16KB]   │
├─────────────────────────────────────────────────────────────────┤
│  Layer 3: SMP E2E Encryption                                    │
│  └── crypto_box(msg, nonce, server_dh_pub, our_dh_secret)      │
│  └── Nonce: 24 bytes, Tag: 16 bytes                            │
├─────────────────────────────────────────────────────────────────┤
│  Layer 4: SMP Client Message                                    │
│  └── [2-byte length prefix] [encrypted_content] [padding]      │
├─────────────────────────────────────────────────────────────────┤
│  Layer 5: Contact DH Encryption (Initial Messages)              │
│  └── [X25519 SPKI key (44 bytes)] [crypto_box encrypted body]  │
│  └── crypto_box(body, nonce, sender_dh_pub, contact_dh_secret) │
├─────────────────────────────────────────────────────────────────┤
│  Layer 6: Agent Protocol Message                                │
│  └── [2-byte version BE] [type: 'C'/'I'/'M'/'R'] [body]        │
└─────────────────────────────────────────────────────────────────┘
```

**Message Format after Layer 3 Decryption:**
```
┌──────────────────────────────────────────────────────────────────┐
│ Offset 0-1:   Length Prefix (BE)        │ z.B. 0x3E82 = 16002   │
│ Offset 2-5:   Unknown Header            │ 00 00 00 00           │
│ Offset 6-9:   "ip" + 2 bytes            │ 69 70 xx xx           │
│ Offset 10-13: "T " + version "1,"       │ 54 20 00 04 31 2c     │
│ Offset 14-57: X25519 SPKI (44 bytes)    │ 30 2a 30 05 06 03...  │
│ Offset 58+:   crypto_box encrypted body │ [nonce][ciphertext]   │
└──────────────────────────────────────────────────────────────────┘
```

**Agent Message Types:**
| Type | Name | Description |
|------|------|-------------|
| `'C'` | AgentConfirmation | Connection confirmation with encrypted connInfo |
| `'I'` | AgentInvitation | Invitation with reply queue URI + profile |
| `'M'` | AgentMsgEnvelope | Double Ratchet encrypted message |
| `'R'` | AgentRatchetKey | Ratchet key exchange |

**AgentInvitation Format (Type 'I'):**
```
AgentInvitation = [version:2][type:'I'][connReqLen:2][connReq][connInfo]

connReq = URL-encoded simplex:/invitation#/?v=2-7&smp=...
connInfo = Peer's profile (JSON or binary)
```

**URL Encoding Fix:**
```
FALSCH: dh%3DMCowBQYDK2VuAyEA5nPWbPZTKmf3NdwGzYOq...%2Bv24%3D
                                                    ^^^  ^^^
                                                 Einfach encoded

RICHTIG: dh%3DMCowBQYDK2VuAyEABo11ArKXGHb9zoz_76yz...%253D
                                              ^       ^^^^^
                                           Base64URL  Doppelt encoded
```

**X25519 SPKI Header (12 bytes):**
```
30 2a 30 05 06 03 2b 65 6e 03 21 00
│  │  │  │  │  │  │  │  │  │  │  └─ 0x00
│  │  │  │  │  │  │  │  │  │  └─── BIT STRING length (33)
│  │  │  │  │  │  │  │  │  └────── BIT STRING tag
│  │  │  │  │  │  └──┴──┴───────── OID 1.3.101.110 (X25519)
│  │  │  └──┴──┴────────────────── OID container
│  │  └─────────────────────────── AlgorithmIdentifier
│  └────────────────────────────── Total length (42)
└───────────────────────────────── SEQUENCE tag
```

### Key Discoveries

| Discovery | Details |
|-----------|---------|
| Base64URL for DH | Use `-` and `_` instead of `+` and `/` |
| Double encode `=` | `=` → `%3D` → `%253D` in final URL |
| Layer 5 exists | Initial messages have extra DH encryption |
| SPKI at offset 14 | 44-byte X25519 key embedded in decrypted msg |
| Agent version | 2-byte BE integer (e.g., 0x0007 = v7) |
| Type 'I' | AgentInvitation contains reply queue URI |
| Version "1," | SMP Protocol version indicator at offset 12 |

### Haskell Source References

| File | Discovery |
|------|-----------|
| Agent/Protocol.hs | Agent message types 'C', 'I', 'M', 'R' |
| Agent/Client.hs | AgentInvitation format with connReq |
| Crypto.hs | Double crypto_box layers |

---

## [0.1.11-alpha] - 2026-01-20

### 🔗 Invitation Links Working!

SimpleX Desktop/Mobile Apps can now connect directly to ESP32!

### Added
- **SimpleX-Compatible Contact Links** — ESP32 generates working invitation links
- **Three Link Formats** — SMP Queue URI, Web Link, Direct App Link
- **Base64 Standard Encoding** — For SPKI X25519 public keys
- **URL Encoding** — With correct double-encoding for Base64 special chars
- **Link Generation Functions** — `base64_standard_encode()`, `url_encode()`, `print_invitation_links()`

---

## [0.1.10-alpha] - 2026-01-20

### 🏆 Multi-Contact + E2E Decryption Working!

Multiple contacts over ONE TLS connection with full E2E encryption!

### Added
- **Multi-Contact System** — Up to 10 contacts per connection
- **Contact Database** — `contacts_db_t` with persistent NVS storage
- **Batch Subscribe** — `subscribe_all_contacts()` for all queues
- **Self-Test** — `self_test_send()` verifies full E2E round-trip

### Fixed
- **CRITICAL: E2E Decryption** — `crypto_box_beforenm()` instead of raw `crypto_scalarmult()`
- **SEND Format** — `SEND ' ' flags ' ' body` (two spaces, ASCII flags!)

---

## [0.1.9-alpha] - 2026-01-20

### 🗑️ DEL Command + Full SMP Client!

---

## [0.1.8-alpha] - 2026-01-20

### 🔑 NVS Key Persistence!

---

## [0.1.7-alpha] - 2026-01-20

### ✅ ACK Command Complete!

---

## [0.1.6-alpha] - 2026-01-20

### 🔐 E2E Encryption (Single Queue)

---

## [0.1.5-alpha] - 2026-01-20

### 📨 SEND + MSG Receive

---

## [0.1.4-alpha] - 2026-01-20

### 📡 SUB Command

---

## [0.1.3-alpha] - 2026-01-19

### 🎉 NEW Command Working!

---

## [0.1.2-alpha] - 2026-01-18

### 🤝 Handshake Complete!

---

## [0.1.1-alpha] - 2026-01-17

### 🔒 TLS 1.3 Working!

---

## [0.1.0-alpha] - 2026-01-16

### Initial Release

---

## Version History Summary

| Version | Date | Milestone |
|---------|------|-----------|
| **v0.1.12-alpha** | **2026-01-21** | **🔐 Agent Protocol + Layer 5 Decryption!** |
| v0.1.11-alpha | 2026-01-20 | 🔗 Invitation Links Working |
| v0.1.10-alpha | 2026-01-20 | 🏆 Multi-Contact + E2E |
| v0.1.9-alpha | 2026-01-20 | 🗑️ DEL + Full SMP Client |
| v0.1.8-alpha | 2026-01-20 | 🔑 NVS Persistence |
| v0.1.7-alpha | 2026-01-20 | ✅ ACK Command |
| v0.1.6-alpha | 2026-01-20 | 🔐 E2E Decryption |
| v0.1.5-alpha | 2026-01-20 | 📨 SEND + MSG |
| v0.1.4-alpha | 2026-01-20 | 📡 SUB Command |
| v0.1.3-alpha | 2026-01-19 | 🎉 NEW Command |
| v0.1.2-alpha | 2026-01-18 | 🤝 Handshake |
| v0.1.1-alpha | 2026-01-17 | 🔒 TLS 1.3 |
| v0.1.0-alpha | 2026-01-16 | Initial |

---

## 🏆 Achievement Unlocked

**"First Native ESP32 SimpleX Client with Full Message Layer Decoding"**

- ✅ TLS 1.3 + SMP Handshake
- ✅ Queue Management (NEW, SUB, DEL)
- ✅ Message Lifecycle (SEND, MSG, ACK)
- ✅ SMP E2E Decryption (Layer 3)
- ✅ **Client Message Decryption (Layer 5)**
- ✅ **Agent Protocol Parsing (Layer 6)**
- ✅ **AgentInvitation + Reply Queue Extraction**

---

## Links

- [GitHub Repository](https://github.com/cannatoshi/SimpleGo)
- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/blob/stable/protocol/simplex-messaging.md)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
