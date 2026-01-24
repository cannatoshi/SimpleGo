# SimpleGo 🔐

**Native implementation of the SimpleX Messaging Protocol (SMP) for embedded systems.**

Secure messaging without smartphones.

[![License: AGPL-3.0](https://img.shields.io/badge/License-AGPL--3.0-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.1.29--alpha-green.svg)](CHANGELOG.md)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-orange.svg)](https://www.espressif.com/)

---

## Vision

What if secure messaging didn't require a smartphone?

SimpleGo brings the SimpleX protocol to ESP32 microcontrollers, enabling truly private communication on minimal, auditable hardware. No app stores. No telemetry. No bloat. Just cryptography.

---

## Why Dedicated Hardware?

Modern smartphones contain ~50 million lines of code, hundreds of background processes, and a closed-source baseband processor that can never be fully audited or disabled.

SimpleGo takes a different approach: **minimal trusted computing base**.

| Aspect | Smartphone | SimpleGo |
|--------|------------|----------|
| Lines of Code | ~50 million | ~50,000 |
| Baseband Processor | ✅ Closed-source black box | ❌ WiFi +4G only, open source |
| Background Processes | Hundreds | One |
| Telemetry | Google/Apple services | None |
| Attack Surface | Massive | Minimal |
| Cost | $500+ | ~$15-50 |
| Disposable | No | Yes |

**The most secure system is often the simplest one.**

For a detailed analysis, see [docs/SECURITY_MODEL.md](docs/SECURITY_MODEL.md).

---

## Current Status: v0.1.15-alpha

🏆 **First native SMP protocol implementation outside the official Haskell codebase.**

### What Works

| Component | Status | Verification |
|-----------|--------|--------------|
| TLS 1.3 Connection | ✅ | ALPN "smp/1" |
| SMP Handshake | ✅ | Version negotiation |
| Queue Creation | ✅ | Server accepts |
| X3DH Key Agreement | ✅ | Python-verified |
| Double Ratchet | ✅ | Python-verified |
| AES-256-GCM | ✅ | Python-verified |
| HKDF-SHA512 | ✅ | Python-verified |
| Wire Format | ✅ | Haskell source verified |
| Server Response | ✅ | "OK" |

### In Progress

| Component | Status | Notes |
|-----------|--------|-------|
| App Compatibility | 90% | Final message parsing |

### Cryptographic Verification

All cryptographic operations have been verified byte-for-byte against Python reference implementations:

- X448 Diffie-Hellman (with wolfSSL byte-order handling)
- HKDF-SHA512 for X3DH, Root KDF, and Chain KDF
- AES-256-GCM with 16-byte IV (GHASH transformation verified)
- Complete wire format encoding

---

## Hardware Security

The ESP32-S3 provides hardware-level security features:

| Feature | Description |
|---------|-------------|
| **Secure Boot** | RSA-3072/ECDSA firmware signature verification |
| **Flash Encryption** | AES-256-XTS encrypted storage |
| **eFuse Protection** | One-time programmable security bits |
| **JTAG Disable** | Permanently disable debug access |
| **Hardware Crypto** | Accelerated AES, SHA, RSA, TRNG |

Combined with a minimal codebase and no baseband processor, this creates a security model that eliminates entire categories of attacks that smartphone users must accept.

---

## Architecture

### Module Structure

| Module | Purpose |
|--------|---------|
| `smp_network.c` | TLS 1.3 transport |
| `smp_handshake.c` | SMP protocol handshake |
| `smp_x448.c` | X448 key exchange |
| `smp_ratchet.c` | Double Ratchet state machine |
| `smp_crypto.c` | Ed25519, X25519, AES-GCM |
| `smp_peer.c` | Peer connection management |
| `smp_parser.c` | Protocol message parsing |
| `smp_queue.c` | Queue information encoding |
| `smp_contacts.c` | Contact address handling |

### Dependencies

| Library | Purpose |
|---------|---------|
| mbedTLS | TLS 1.3, AES-GCM |
| wolfSSL | X448/Curve448 |
| libsodium | Ed25519, X25519 |

---

## Hardware Targets

| Device | Status | Description |
|--------|--------|-------------|
| LilyGo T-Deck | Primary | ESP32-S3 with keyboard and display |
| LilyGo T-Embed | Planned | Compact form factor |
| Generic ESP32-S3 | Supported | PSRAM recommended |

---

## Building

### Prerequisites

- ESP-IDF 5.5.2 or newer
- Python 3.8+

### Build Commands

```bash
cd simplex_client
idf.py build flash monitor -p COM5
```

---

## Documentation

| Document | Description |
|----------|-------------|
| [CHANGELOG.md](CHANGELOG.md) | Version history |
| [ROADMAP.md](ROADMAP.md) | Development plan |
| [docs/SECURITY_MODEL.md](docs/SECURITY_MODEL.md) | Security architecture |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Module structure |
| [docs/CRYPTO.md](docs/CRYPTO.md) | Cryptographic details |
| [docs/WIRE_FORMAT.md](docs/WIRE_FORMAT.md) | Protocol encoding |
| [docs/BUGS.md](docs/BUGS.md) | Known issues |

---

## Contributing

Contributions are welcome. Please see [CONTRIBUTING.md](CONTRIBUTING.md).

---

## Security

For security vulnerabilities, please see [SECURITY.md](SECURITY.md).

---

## License

AGPL-3.0 - See [LICENSE](LICENSE).

---

## Disclaimer

SimpleGo is an independent project not affiliated with SimpleX Chat Ltd. See [docs/TRADEMARK.md](docs/TRADEMARK.md).
