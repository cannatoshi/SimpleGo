# SimpleGo Architecture

Technical documentation of the SimpleGo module structure and design.

---

## Overview

SimpleGo is organized in a layered architecture with clear separation of concerns. The codebase consists of approximately 3,200 lines of C code distributed across 11 modules.

---

## Directory Structure

| Path | Description |
|------|-------------|
| main/ | Application source code |
| main/main.c | Application entry point |
| main/smp_x448.c | X448 cryptographic operations |
| main/smp_ratchet.c | Double Ratchet implementation |
| main/smp_handshake.c | E2E handshake protocol |
| main/smp_queue.c | SMPQueueInfo encoding |
| main/smp_peer.c | Peer connection management |
| main/smp_parser.c | Protocol message parsing |
| main/smp_network.c | TLS/TCP networking |
| main/smp_crypto.c | Ed25519, X25519 operations |
| main/smp_contacts.c | Contact address handling |
| main/smp_utils.c | Encoding utilities |
| include/ | Header files |
| components/wolfssl/ | X448/Curve448 library |
| components/kyber/ | Post-quantum crypto (future) |
| docs/ | Documentation |
| CMakeLists.txt | Project build file |
| partitions.csv | Flash partition table |
| sdkconfig | ESP-IDF configuration |

---

## Layer Architecture

### Layer 1: Crypto Layer

Low-level cryptographic operations.

| Module | File | Lines | Description |
|--------|------|-------|-------------|
| X448 Operations | smp_x448.c | ~200 | Key generation, DH, byte-order |
| Double Ratchet | smp_ratchet.c | ~500 | State, KDFs, AES-GCM |
| E2E Handshake | smp_handshake.c | ~300 | X3DH, AgentConfirmation |
| Queue Encoding | smp_queue.c | ~250 | SMPQueueInfo encoding |

### Layer 2: Protocol Layer

SMP protocol implementation.

| Module | File | Lines | Description |
|--------|------|-------|-------------|
| Peer Connection | smp_peer.c | ~400 | Connection lifecycle |
| Message Parser | smp_parser.c | ~350 | Protocol parsing |
| Network Layer | smp_network.c | ~300 | TLS/TCP |

### Layer 3: Application Layer

High-level application logic.

| Module | File | Lines | Description |
|--------|------|-------|-------------|
| Contact Management | smp_contacts.c | ~200 | Address handling |
| Legacy Crypto | smp_crypto.c | ~250 | Ed25519, X25519 |
| Utilities | smp_utils.c | ~150 | Encoding helpers |

---

## Module Details

### smp_x448.c - X448 Cryptographic Operations

Handles X448/Curve448 elliptic curve operations using wolfSSL.

Key Functions:
- smp_x448_generate_keypair(): Generate X448 key pair
- smp_x448_dh(): Perform Diffie-Hellman key exchange
- smp_x448_reverse_bytes(): Handle wolfSSL byte-order conversion

Notes:
- wolfSSL exports X448 keys in reversed byte order
- All public keys must be reversed before use

---

### smp_ratchet.c - Double Ratchet Implementation

Implements the Double Ratchet algorithm.

Key Functions:
- smp_ratchet_init(): Initialize ratchet state
- smp_ratchet_root_kdf(): Derive new root key, chain key, header key
- smp_ratchet_chain_kdf(): Derive message key, IVs
- smp_ratchet_encrypt_header(): Encrypt MsgHeader
- smp_ratchet_encrypt_body(): Encrypt message body

KDF Parameters:

| KDF | Salt | IKM | Info | Output |
|-----|------|-----|------|--------|
| X3DH | 64 zero bytes | dh1+dh2+dh3 | SimpleXX3DH | 96 bytes |
| Root | root_key | dh_output | SimpleXRootRatchet | 96 bytes |
| Chain | empty | chain_key | SimpleXChainRatchet | 96 bytes |

---

### smp_handshake.c - E2E Handshake Protocol

Handles end-to-end encryption handshake.

Key Functions:
- smp_handshake_x3dh(): Perform X3DH key agreement
- smp_handshake_build_agent_confirmation(): Build AgentConfirmation
- smp_handshake_build_hello(): Build HELLO message

---

### smp_queue.c - SMPQueueInfo Encoding

Handles encoding of SMPQueueInfo structures.

Key Functions:
- smp_queue_encode_info(): Encode complete SMPQueueInfo
- smp_queue_encode_version(): Encode smpClientVersion
- smp_queue_encode_queue_mode(): Encode queueMode

queueMode Encoding:
- Nothing = empty (0 bytes)
- Just QMMessaging = M (1 byte)

---

### smp_peer.c - Peer Connection Management

Manages peer connection lifecycle.

Key Functions:
- smp_peer_connect(): Establish connection
- smp_peer_send_confirmation(): Send AgentConfirmation
- smp_peer_send_hello(): Send HELLO message
- smp_peer_send_message(): Send encrypted message

---

### smp_parser.c - Protocol Message Parsing

Parses incoming messages.

Key Functions:
- smp_parser_parse_response(): Parse SMP response
- smp_parser_parse_agent_message(): Parse Agent message
- smp_parser_parse_queue_info(): Parse SMPQueueInfo

---

### smp_network.c - TLS/TCP Networking

Handles network communication.

Key Functions:
- smp_network_init(): Initialize networking
- smp_network_connect(): Establish TLS connection
- smp_network_send(): Send data
- smp_network_receive(): Receive data

TLS Configuration:
- Protocol: TLS 1.3
- Library: mbedTLS
- Certificate Verification: Enabled

---

## Component Dependencies

### wolfSSL

Provides X448/Curve448 cryptography.

Location: components/wolfssl/
Usage: smp_x448.c

### mbedTLS

Provides TLS 1.3, HKDF, AES-GCM.

Usage: smp_network.c, smp_ratchet.c

### libsodium

Provides Ed25519 and X25519.

Usage: smp_crypto.c

---

## Memory Layout

### Static Allocations

| Component | Size |
|-----------|------|
| Ratchet State | ~300 bytes |
| TLS Context | ~40 KB |
| Network Buffers | ~16 KB |

### Recommended Hardware

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| Flash | 4 MB | 8 MB |
| PSRAM | None | 2 MB |
| RAM | 320 KB | 512 KB |

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
