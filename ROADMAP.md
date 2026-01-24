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
| Padding implementation | Complete |
| SMPQueueInfo encoding | Complete |
| queueMode encoding fix | Complete |
| Cryptographic verification | Complete |
| Wire format verification | Complete |

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
| Contact list UI | Planned |
| Conversation view | Planned |
| Message composition | Planned |
| Status indicators | Planned |
| Settings screen | Planned |

### Phase 5: Advanced Features (Planned)

| Task | Status |
|------|--------|
| Group messaging support | Planned |
| File transfer (XFTP) | Planned |
| Message history storage | Planned |
| NVS persistent storage | Planned |
| Contact backup/restore | Planned |
| QR code scanning | Planned |
| Multiple server support | Planned |

### Phase 6: Production Readiness (Future)

| Task | Status |
|------|--------|
| Security review | Future |
| External code audit | Future |
| Memory optimization | Future |
| Power management | Future |
| OTA update support | Future |
| Beta testing program | Future |
| v1.0.0 stable release | Future |

---

## Module Development Status

### Crypto Layer

| Module | File | Lines | Status |
|--------|------|-------|--------|
| X448 Operations | smp_x448.c | ~200 | Complete |
| Double Ratchet | smp_ratchet.c | ~500 | Complete |
| E2E Handshake | smp_handshake.c | ~300 | Complete |
| Queue Encoding | smp_queue.c | ~250 | Complete |

### Protocol Layer

| Module | File | Lines | Status |
|--------|------|-------|--------|
| Peer Connection | smp_peer.c | ~400 | Complete |
| Message Parser | smp_parser.c | ~350 | Complete |
| Network Layer | smp_network.c | ~300 | Complete |

### Application Layer

| Module | File | Lines | Status |
|--------|------|-------|--------|
| Contact Management | smp_contacts.c | ~200 | Complete |
| Legacy Crypto | smp_crypto.c | ~250 | Complete |
| Utilities | smp_utils.c | ~150 | Complete |

---

## Bug Fixes by Version

### v0.1.15-alpha (12 bugs fixed)

| Bug | Description | Fix |
|-----|-------------|-----|
| #1 | E2E key used Word16 | Changed to 1-byte prefix |
| #2 | prevMsgHash used 1-byte | Changed to Word16 BE |
| #3 | MsgHeader DH key used Word16 | Changed to 1-byte prefix |
| #4 | ehBody used Word16 prefix | Changed to 1-byte prefix |
| #5 | emHeader was 124 bytes | Corrected to 123 bytes |
| #6 | Payload AAD was 236 bytes | Corrected to 235 bytes |
| #7 | Root KDF output order wrong | Fixed split order |
| #8 | Chain KDF IV order swapped | Fixed: header_iv first |
| #9 | wolfSSL X448 keys reversed | Added byte reversal |
| #10 | SMPQueueInfo port used space | Changed to length prefix |
| #11 | smpQueues count was 1-byte | Changed to Word16 BE |
| #12 | queueMode Nothing sent 0 | Changed to send nothing |

---

## Verification Milestones

| Milestone | Date | Result |
|-----------|------|--------|
| X448 DH verification | Jan 2026 | 100% match |
| X3DH HKDF verification | Jan 2026 | 100% match |
| Root KDF verification | Jan 2026 | 100% match |
| Chain KDF verification | Jan 2026 | 100% match |
| AES-GCM verification | Jan 2026 | 100% match |
| Wire format verification | Jan 2026 | Verified |
| Server acceptance test | Jan 2026 | OK response |

---

## Hardware Roadmap

### Supported (Current)

| Device | Chip | Status |
|--------|------|--------|
| LilyGo T-Deck | ESP32-S3 | Primary target |
| LilyGo T-Embed | ESP32-S3 | Secondary target |
| Generic ESP32-S3 | ESP32-S3 | Supported |

### Planned Support (Future)

| Device | Status |
|--------|--------|
| LilyGo T-Watch | Under consideration |
| M5Stack devices | Under consideration |
| Custom hardware | Community contributions |

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

## License

AGPL-3.0 - See [LICENSE](LICENSE)
