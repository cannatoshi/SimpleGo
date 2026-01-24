# SimpleGo Roadmap

Development plan and progress tracking for SimpleGo.

---

## Project Timeline

| Version | Date | Milestone |
|---------|------|-----------|
| v0.1.0 | Dec 2025 | Project initialization |
| v0.1.1-v0.1.13 | Dec 2025 | Initial protocol implementation |
| v0.1.14-alpha | Jan 21, 2026 | Modular architecture refactoring |
| v0.1.15-alpha | Jan 24, 2026 | Double Ratchet and X3DH implementation |

---

## Development Phases

### Phase 1: Foundation (Complete)

| Task | Status |
|------|--------|
| ESP-IDF project setup | Complete |
| WiFi connectivity | Complete |
| Basic TCP sockets | Complete |
| Project structure | Complete |

### Phase 2: TLS and Basic SMP (Complete)

| Task | Status |
|------|--------|
| TLS 1.3 with mbedTLS | Complete |
| SMP server connection | Complete |
| Basic protocol handshake | Complete |
| Server response parsing | Complete |

### Phase 3: Protocol Implementation

#### Phase 3.1-3.10: Core Protocol (Complete)

| Task | Status |
|------|--------|
| Ed25519 signature generation | Complete |
| Ed25519 signature verification | Complete |
| X25519 key exchange | Complete |
| SMP command encoding | Complete |
| SMP response parsing | Complete |
| Queue creation (NEW command) | Complete |
| Queue subscription (SUB command) | Complete |
| Message sending (SEND command) | Complete |
| Message receiving | Complete |
| Connection management | Complete |

#### Phase 3.11: Double Ratchet (Complete)

| Task | Status |
|------|--------|
| wolfSSL integration | Complete |
| X448 key generation | Complete |
| X448 Diffie-Hellman | Complete |
| wolfSSL byte-order fix | Complete |
| X3DH key agreement | Complete |
| X3DH HKDF derivation | Complete |
| HKDF-SHA512 implementation | Complete |
| Root KDF function | Complete |
| Chain KDF function | Complete |
| IV order correction | Complete |
| AES-256-GCM encryption | Complete |
| AES-256-GCM decryption | Complete |
| 16-byte IV implementation | Complete |
| Message header encryption | Complete |
| Message body encryption | Complete |
| EncMessageHeader encoding | Complete |
| EncRatchetMessage encoding | Complete |
| AgentConfirmation building | Complete |
| HELLO message building | Complete |
| Padding implementation (14832/15840 bytes) | Complete |
| SMPQueueInfo encoding | Complete |
| queueMode encoding fix | Complete |
| Cryptographic verification against Python | Complete |
| Wire format verification against Haskell | Complete |

#### Phase 3.12: App Compatibility (In Progress)

| Task | Status | Notes |
|------|--------|-------|
| Server acceptance | Complete | Server responds with OK |
| App message parsing | In Progress | A_MESSAGE error |
| Tail encoding investigation | In Progress | Hypothesis: length prefix on final fields |
| encConnInfo encoding | Investigating | May need to remove length prefix |
| emBody encoding | Investigating | May need to remove length prefix |
| Bidirectional messaging | Pending | Blocked by parsing issue |
| Full message flow | Pending | Blocked by parsing issue |
| Message delivery confirmation | Pending | Blocked by parsing issue |

### Phase 4: User Interface (Planned)

| Task | Status |
|------|--------|
| T-Deck display driver integration | Planned |
| ST7789 display initialization | Planned |
| Text rendering engine | Planned |
| Font support | Planned |
| Keyboard input handling | Planned |
| Key mapping configuration | Planned |
| Menu system implementation | Planned |
| Navigation controls | Planned |
| Contact list UI | Planned |
| Contact details view | Planned |
| Conversation list view | Planned |
| Conversation detail view | Planned |
| Message composition screen | Planned |
| Message input handling | Planned |
| Status indicators (connection, battery) | Planned |
| Notification system | Planned |
| Settings screen | Planned |
| WiFi configuration UI | Planned |

### Phase 5: Advanced Features (Planned)

| Task | Status |
|------|--------|
| Group messaging support | Planned |
| File transfer (XFTP) | Planned |
| Message history storage | Planned |
| NVS persistent storage | Planned |
| Contact backup/restore | Planned |
| QR code scanning | Planned |
| QR code generation | Planned |
| Multiple server support | Planned |

### Phase 6: Production Readiness (Future)

| Task | Status |
|------|--------|
| Security review | Future |
| External code audit | Future |
| Memory optimization | Future |
| Stack usage analysis | Future |
| Power management | Future |
| Deep sleep support | Future |
| Battery monitoring | Future |
| Error handling improvements | Future |
| Crash recovery | Future |
| OTA update support | Future |
| Production build configuration | Future |
| Beta testing program | Future |
| Documentation completion | Future |
| Release candidate | Future |
| v1.0.0 stable release | Future |

---

## Module Development Status

### Crypto Layer

| Module | File | Lines | Status | Description |
|--------|------|-------|--------|-------------|
| X448 Operations | smp_x448.c | ~200 | Complete | Key generation, DH, byte-order handling |
| Double Ratchet | smp_ratchet.c | ~500 | Complete | State management, KDFs, encryption |
| E2E Handshake | smp_handshake.c | ~300 | Complete | X3DH, AgentConfirmation, HELLO |
| Queue Encoding | smp_queue.c | ~250 | Complete | SMPQueueInfo, smpClientVersion |

### Protocol Layer

| Module | File | Lines | Status | Description |
|--------|------|-------|--------|-------------|
| Peer Connection | smp_peer.c | ~400 | Complete | Connection lifecycle, message flow |
| Message Parser | smp_parser.c | ~350 | Complete | Agent protocol parsing |
| Network Layer | smp_network.c | ~300 | Complete | TLS, TCP, connection management |

### Application Layer

| Module | File | Lines | Status | Description |
|--------|------|-------|--------|-------------|
| Contact Management | smp_contacts.c | ~200 | Complete | Address handling, contact storage |
| Legacy Crypto | smp_crypto.c | ~250 | Complete | Ed25519, X25519, basic operations |
| Utilities | smp_utils.c | ~150 | Complete | Encoding helpers, hex conversion |

### Components

| Component | Status | Description |
|-----------|--------|-------------|
| wolfSSL | Integrated | X448/Curve448 cryptographic operations |
| Kyber KEM | Prepared | Post-quantum cryptography (future use) |

---

## Bug Fixes by Version

### v0.1.15-alpha (12 bugs fixed)

| Bug | Category | Description | Fix |
|-----|----------|-------------|-----|
| #1 | Length Encoding | E2E key used Word16 instead of 1-byte | Changed to 1-byte prefix |
| #2 | Length Encoding | prevMsgHash used 1-byte instead of Word16 | Changed to Word16 BE |
| #3 | Length Encoding | MsgHeader DH key used Word16 | Changed to 1-byte prefix |
| #4 | Length Encoding | ehBody used Word16 prefix | Changed to 1-byte prefix |
| #5 | Length Encoding | emHeader was 124 bytes | Corrected to 123 bytes |
| #6 | AAD Size | Payload AAD was 236 bytes | Corrected to 235 bytes |
| #7 | KDF Output | Root KDF output order wrong | Fixed: root, chain, next_header |
| #8 | KDF Output | Chain KDF IV order swapped | Fixed: header_iv before msg_iv |
| #9 | Byte Order | wolfSSL X448 keys reversed | Added byte reversal function |
| #10 | Format | SMPQueueInfo port used space | Changed to length prefix |
| #11 | Format | smpQueues count was 1-byte | Changed to Word16 BE |
| #12 | Format | queueMode Nothing sent '0' | Changed to send nothing |

---

## Verification Milestones

| Milestone | Date | Method | Result |
|-----------|------|--------|--------|
| X448 DH verification | Jan 2026 | Python comparison | 100% match |
| X3DH HKDF verification | Jan 2026 | Python comparison | 100% match |
| Root KDF verification | Jan 2026 | Python comparison | 100% match |
| Chain KDF verification | Jan 2026 | Python comparison | 100% match |
| AES-GCM verification | Jan 2026 | Python comparison | 100% match |
| Wire format verification | Jan 2026 | Haskell source comparison | Verified |
| Server acceptance test | Jan 2026 | Live server test | OK response |

---

## Current Investigation: Tail Encoding

### Hypothesis

The A_MESSAGE parsing error may be caused by incorrect handling of Tail-encoded fields. In SimpleX protocol, certain fields at the end of structures use Tail encoding, which means no length prefix is added.

### Fields Under Investigation

| Structure | Field | Current Encoding | Suspected Correct |
|-----------|-------|------------------|-------------------|
| AgentConfirmation | encConnInfo | With length prefix | Tail (no prefix) |
| EncRatchetMessage | emBody | With length prefix | Tail (no prefix) |

### Evidence

- All cryptographic operations verified correct
- Server accepts messages with OK response
- App fails to parse with A_MESSAGE error
- Error suggests parsing issue, not crypto issue

### Next Steps

1. Review Haskell source for Tail encoding patterns
2. Remove length prefixes from suspected Tail fields
3. Test with SimpleX mobile app
4. Verify bidirectional message flow

---

## Hardware Roadmap

### Supported (Current)

| Device | Chip | Status |
|--------|------|--------|
| LilyGo T-Deck | ESP32-S3 | Primary development target |
| LilyGo T-Embed | ESP32-S3 | Secondary target |
| Generic ESP32-S3 | ESP32-S3 | Supported with PSRAM |

### Planned Support (Future)

| Device | Chip | Status |
|--------|------|--------|
| LilyGo T-Watch | ESP32 | Under consideration |
| M5Stack devices | Various | Under consideration |
| Custom hardware | ESP32-S3 | Community contributions welcome |

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to contribute to the roadmap and development.

---

## License

This project is licensed under AGPL-3.0. See [LICENSE](LICENSE) for details.
