# SimpleGo Architecture

Technical documentation of the SimpleGo module structure and design.

---

## Overview

SimpleGo is organized in a layered architecture with clear separation of concerns. The codebase consists of approximately 3,200 lines of C code distributed across 11 modules.

---

## Directory Structure
`
simplex_client/
├── main/
│   ├── main.c                 # Application entry point
│   ├── smp_x448.c             # X448 cryptographic operations
│   ├── smp_ratchet.c          # Double Ratchet implementation
│   ├── smp_handshake.c        # E2E handshake protocol
│   ├── smp_queue.c            # SMPQueueInfo encoding
│   ├── smp_peer.c             # Peer connection management
│   ├── smp_parser.c           # Protocol message parsing
│   ├── smp_network.c          # TLS/TCP networking
│   ├── smp_crypto.c           # Ed25519, X25519 operations
│   ├── smp_contacts.c         # Contact address handling
│   └── smp_utils.c            # Encoding utilities
├── include/
│   ├── smp_x448.h
│   ├── smp_ratchet.h
│   ├── smp_handshake.h
│   ├── smp_queue.h
│   ├── smp_peer.h
│   ├── smp_parser.h
│   ├── smp_network.h
│   ├── smp_crypto.h
│   ├── smp_contacts.h
│   └── smp_utils.h
├── components/
│   ├── wolfssl/               # X448/Curve448 library
│   │   ├── wolfssl/
│   │   └── wolfssl_config/
│   └── kyber/                 # Post-quantum crypto (future)
│       └── kem/
├── docs/
│   ├── ARCHITECTURE.md
│   ├── CRYPTO.md
│   ├── WIRE_FORMAT.md
│   ├── BUGS.md
│   ├── PROTOCOL.md
│   ├── TECHNICAL.md
│   ├── DEVELOPMENT.md
│   ├── DEVNOTES.md
│   ├── DISCLAIMER.md
│   ├── LEGAL.md
│   ├── SPONSORS.md
│   ├── TRADEMARK.md
│   └── release-info/
│       ├── v0.1.14-alpha.md
│       └── v0.1.15-alpha.md
├── CMakeLists.txt
├── partitions.csv
├── sdkconfig
├── CHANGELOG.md
├── README.md
├── ROADMAP.md
├── CONTRIBUTING.md
├── CODE_OF_CONDUCT.md
├── SECURITY.md
├── SUPPORT.md
└── LICENSE
`

---

## Layer Architecture

### Layer 1: Crypto Layer

Low-level cryptographic operations. These modules have no dependencies on protocol logic.

| Module | File | Lines | Description |
|--------|------|-------|-------------|
| X448 Operations | smp_x448.c | ~200 | X448 key generation, Diffie-Hellman, byte-order handling |
| Double Ratchet | smp_ratchet.c | ~500 | Ratchet state, Root KDF, Chain KDF, AES-GCM |
| E2E Handshake | smp_handshake.c | ~300 | X3DH key agreement, AgentConfirmation, HELLO |
| Queue Encoding | smp_queue.c | ~250 | SMPQueueInfo structure encoding |

### Layer 2: Protocol Layer

SMP protocol implementation. Depends on Crypto Layer.

| Module | File | Lines | Description |
|--------|------|-------|-------------|
| Peer Connection | smp_peer.c | ~400 | Connection lifecycle, message sending/receiving |
| Message Parser | smp_parser.c | ~350 | Agent protocol parsing, response handling |
| Network Layer | smp_network.c | ~300 | TLS 1.3 connection, TCP socket management |

### Layer 3: Application Layer

High-level application logic. Depends on Protocol Layer.

| Module | File | Lines | Description |
|--------|------|-------|-------------|
| Contact Management | smp_contacts.c | ~200 | Contact address parsing, storage |
| Legacy Crypto | smp_crypto.c | ~250 | Ed25519 signatures, X25519 key exchange |
| Utilities | smp_utils.c | ~150 | Hex encoding, length prefixes, helpers |

### Layer 4: Entry Point

| Module | File | Lines | Description |
|--------|------|-------|-------------|
| Main Application | main.c | ~350 | WiFi init, application loop, user interface |

---

## Module Details

### smp_x448.c - X448 Cryptographic Operations

Handles X448/Curve448 elliptic curve operations using wolfSSL.

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_x448_generate_keypair() | Generate X448 key pair |
| smp_x448_dh() | Perform Diffie-Hellman key exchange |
| smp_x448_reverse_bytes() | Handle wolfSSL byte-order conversion |

**Dependencies:**
- wolfSSL library
- smp_utils.h

**Notes:**
- wolfSSL exports X448 keys in reversed byte order
- All public keys must be reversed before use
- All DH outputs must be reversed after computation

---

### smp_ratchet.c - Double Ratchet Implementation

Implements the Double Ratchet algorithm for forward-secure encryption.

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_ratchet_init() | Initialize ratchet state |
| smp_ratchet_root_kdf() | Derive new root key, chain key, next header key |
| smp_ratchet_chain_kdf() | Derive message key, new chain key, IVs |
| smp_ratchet_encrypt_header() | Encrypt MsgHeader with AES-GCM |
| smp_ratchet_encrypt_body() | Encrypt message body with AES-GCM |
| smp_ratchet_build_enc_message() | Build complete EncRatchetMessage |

**Data Structures:**
`c
typedef struct {
    uint8_t root_key[32];
    uint8_t chain_key[32];
    uint8_t header_key[32];
    uint8_t next_header_key[32];
    x448_keypair_t dh_keypair;
    uint8_t peer_dh_public[56];
    uint32_t msg_number_send;
    uint32_t msg_number_recv;
    uint32_t prev_chain_length;
} ratchet_state_t;
`

**KDF Parameters:**

| KDF | Salt | IKM | Info | Output |
|-----|------|-----|------|--------|
| X3DH | 64 zero bytes | dh1+dh2+dh3 (168B) | "SimpleXX3DH" | 96 bytes |
| Root | root_key (32B) | dh_output (56B) | "SimpleXRootRatchet" | 96 bytes |
| Chain | empty | chain_key (32B) | "SimpleXChainRatchet" | 96 bytes |

**Dependencies:**
- mbedTLS (HKDF, AES-GCM)
- smp_x448.h

---

### smp_handshake.c - E2E Handshake Protocol

Handles the end-to-end encryption handshake using X3DH.

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_handshake_x3dh() | Perform X3DH key agreement |
| smp_handshake_build_agent_confirmation() | Build AgentConfirmation message |
| smp_handshake_build_hello() | Build HELLO message |
| smp_handshake_build_enc_conn_info() | Encode encrypted connection info |

**AgentConfirmation Structure:**

| Offset | Size | Field |
|--------|------|-------|
| 0 | 2 | agentVersion (Word16 BE = 7) |
| 2 | 1 | type ('C' = 0x43) |
| 3 | 1 | maybeE2E ('1' = 0x31) |
| 4 | 2 | e2eVersion (Word16 BE = 2) |
| 6 | 1 | key1Len (0x44 = 68) |
| 7 | 68 | key1 (X448 SPKI) |
| 75 | 1 | key2Len (0x44 = 68) |
| 76 | 68 | key2 (X448 SPKI) |
| 144 | REST | encConnInfo (Tail encoded) |

**Dependencies:**
- smp_x448.h
- smp_ratchet.h
- smp_queue.h

---

### smp_queue.c - SMPQueueInfo Encoding

Handles encoding of SMPQueueInfo structures for the Agent Protocol.

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_queue_encode_info() | Encode complete SMPQueueInfo |
| smp_queue_encode_version() | Encode smpClientVersion |
| smp_queue_encode_server() | Encode server host and port |
| smp_queue_encode_queue_mode() | Encode queueMode (Nothing = empty) |

**SMPQueueInfo Structure:**

| Field | Encoding |
|-------|----------|
| clientVersion | Word16 BE (= 8) |
| hostCount | 1 byte (= 1) |
| host | Length-prefixed string |
| port | Length-prefixed string |
| keyHash | 1-byte length + 32 bytes |
| senderId | 1-byte length + base64 bytes |
| dhPublicKey | 1-byte length (44) + X25519 SPKI |
| queueMode | Nothing = empty, Just QMMessaging = "M" |

**Dependencies:**
- smp_utils.h

---

### smp_peer.c - Peer Connection Management

Manages the lifecycle of peer connections and message flow.

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_peer_connect() | Establish connection to peer |
| smp_peer_send_confirmation() | Send AgentConfirmation |
| smp_peer_send_hello() | Send HELLO message |
| smp_peer_send_message() | Send encrypted message |
| smp_peer_receive() | Receive and process messages |
| smp_peer_disconnect() | Close connection |

**Connection States:**

| State | Description |
|-------|-------------|
| DISCONNECTED | No active connection |
| CONNECTING | TLS handshake in progress |
| CONNECTED | TLS established, SMP handshake pending |
| HANDSHAKING | SMP handshake in progress |
| READY | Ready for messaging |
| ERROR | Connection error |

**Dependencies:**
- smp_network.h
- smp_handshake.h
- smp_parser.h

---

### smp_parser.c - Protocol Message Parsing

Parses incoming SMP and Agent Protocol messages.

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_parser_parse_response() | Parse SMP server response |
| smp_parser_parse_agent_message() | Parse Agent Protocol message |
| smp_parser_parse_queue_info() | Parse SMPQueueInfo from invitation |
| smp_parser_extract_keys() | Extract public keys from messages |

**Supported Message Types:**

| Type | Description |
|------|-------------|
| OK | Command accepted |
| ERR | Error response |
| MSG | Incoming message |
| NMSG | New message notification |

**Dependencies:**
- smp_utils.h

---

### smp_network.c - TLS/TCP Networking

Handles low-level network communication.

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_network_init() | Initialize networking stack |
| smp_network_connect() | Establish TLS connection |
| smp_network_send() | Send data over TLS |
| smp_network_receive() | Receive data over TLS |
| smp_network_disconnect() | Close TLS connection |
| smp_network_cleanup() | Free network resources |

**TLS Configuration:**

| Parameter | Value |
|-----------|-------|
| Protocol | TLS 1.3 |
| Library | mbedTLS |
| Certificate Verification | Enabled |
| ALPN | None |

**Dependencies:**
- mbedTLS
- ESP-IDF networking

---

### smp_crypto.c - Ed25519 and X25519 Operations

Handles Ed25519 signatures and X25519 key exchange (non-ratchet).

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_crypto_ed25519_keypair() | Generate Ed25519 key pair |
| smp_crypto_ed25519_sign() | Sign data with Ed25519 |
| smp_crypto_ed25519_verify() | Verify Ed25519 signature |
| smp_crypto_x25519_keypair() | Generate X25519 key pair |
| smp_crypto_x25519_dh() | X25519 Diffie-Hellman |

**Dependencies:**
- libsodium

---

### smp_contacts.c - Contact Address Handling

Manages contact addresses and invitation links.

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_contacts_parse_address() | Parse SimpleX contact address |
| smp_contacts_parse_invitation() | Parse invitation link |
| smp_contacts_store() | Store contact in NVS |
| smp_contacts_load() | Load contact from NVS |

**Address Format:**
`
simplex:/invitation#/?v=1&smp=<server>&e2e=<keys>
`

**Dependencies:**
- smp_parser.h
- ESP-IDF NVS

---

### smp_utils.c - Encoding Utilities

Provides common encoding and utility functions.

**Key Functions:**

| Function | Description |
|----------|-------------|
| smp_utils_encode_length() | Encode length prefix (1-byte or Large) |
| smp_utils_encode_word16() | Encode Word16 big-endian |
| smp_utils_encode_word32() | Encode Word32 big-endian |
| smp_utils_hex_encode() | Convert bytes to hex string |
| smp_utils_hex_decode() | Convert hex string to bytes |
| smp_utils_base64_encode() | Base64 encoding |
| smp_utils_base64_decode() | Base64 decoding |

**Length Encoding:**

| Type | Condition | Format |
|------|-----------|--------|
| Standard | length <= 254 | 1 byte |
| Large | length > 254 | 0xFF + Word16 BE |
| Tail | Last field in structure | No prefix |

**Dependencies:**
- None (standalone)

---

## Component Dependencies

### wolfSSL

Provides X448/Curve448 elliptic curve cryptography.

**Configuration:**
- Location: components/wolfssl/
- Custom user_settings.h for ESP32
- Curve448 enabled
- Other curves disabled for size optimization

**Usage:**
- smp_x448.c

### Kyber KEM

Post-quantum key encapsulation mechanism (prepared for future use).

**Configuration:**
- Location: components/kyber/
- Not yet integrated into message flow
- Planned for hybrid encryption

**Usage:**
- Future implementation

### mbedTLS

Provides TLS 1.3, HKDF, and AES-GCM.

**Usage:**
- smp_network.c (TLS)
- smp_ratchet.c (HKDF, AES-GCM)

### libsodium

Provides Ed25519 and X25519 operations.

**Usage:**
- smp_crypto.c

---

## Data Flow

### Outgoing Message Flow
`
1. User composes message
       |
       v
2. main.c: smp_peer_send_message()
       |
       v
3. smp_peer.c: Prepare message content
       |
       v
4. smp_ratchet.c: Chain KDF for keys and IVs
       |
       v
5. smp_ratchet.c: Encrypt header (AES-GCM)
       |
       v
6. smp_ratchet.c: Encrypt body (AES-GCM)
       |
       v
7. smp_ratchet.c: Build EncRatchetMessage
       |
       v
8. smp_peer.c: Add padding
       |
       v
9. smp_network.c: Send over TLS
       |
       v
10. SMP Server
`

### Incoming Message Flow
`
1. SMP Server sends message
       |
       v
2. smp_network.c: Receive over TLS
       |
       v
3. smp_parser.c: Parse SMP response
       |
       v
4. smp_peer.c: Extract encrypted message
       |
       v
5. smp_ratchet.c: Decrypt header
       |
       v
6. smp_ratchet.c: Verify header auth tag
       |
       v
7. smp_ratchet.c: Decrypt body
       |
       v
8. smp_ratchet.c: Verify body auth tag
       |
       v
9. smp_peer.c: Process decrypted message
       |
       v
10. main.c: Display to user
`

### Connection Establishment Flow
`
1. Parse contact invitation
       |
       v
2. Extract server address and keys
       |
       v
3. smp_network.c: TLS connect to server
       |
       v
4. smp_peer.c: SMP handshake
       |
       v
5. smp_handshake.c: X3DH key agreement
       |
       v
6. smp_ratchet.c: Initialize ratchet state
       |
       v
7. smp_handshake.c: Build AgentConfirmation
       |
       v
8. smp_peer.c: Send AgentConfirmation
       |
       v
9. Server: OK response
       |
       v
10. smp_handshake.c: Build HELLO message
       |
       v
11. smp_peer.c: Send HELLO
       |
       v
12. Server: OK response
       |
       v
13. Connection ready for messaging
`

---

## Memory Layout

### Static Allocations

| Component | Size | Description |
|-----------|------|-------------|
| Ratchet State | ~300 bytes | Per-connection ratchet |
| TLS Context | ~40 KB | mbedTLS structures |
| Network Buffers | ~16 KB | Send/receive buffers |
| Key Storage | ~1 KB | Temporary key material |

### Heap Usage

| Operation | Peak Usage |
|-----------|------------|
| TLS Handshake | ~60 KB |
| Message Encryption | ~10 KB |
| Message Decryption | ~10 KB |
| Idle | ~45 KB |

### Recommended Hardware

| Requirement | Minimum | Recommended |
|-------------|---------|-------------|
| Flash | 4 MB | 8 MB |
| PSRAM | None | 2 MB |
| RAM | 320 KB | 512 KB |

---

## Build Configuration

### CMakeLists.txt
`cmake
idf_component_register(
    SRCS 
        "main.c"
        "smp_x448.c"
        "smp_ratchet.c"
        "smp_handshake.c"
        "smp_queue.c"
        "smp_peer.c"
        "smp_parser.c"
        "smp_network.c"
        "smp_crypto.c"
        "smp_contacts.c"
        "smp_utils.c"
    INCLUDE_DIRS 
        "../include"
    REQUIRES 
        nvs_flash 
        esp_wifi 
        esp_netif 
        mbedtls 
        libsodium
)
`

### Partition Table

| Name | Type | Size | Description |
|------|------|------|-------------|
| nvs | data | 24 KB | Non-volatile storage |
| phy_init | data | 4 KB | PHY calibration |
| factory | app | 1.5 MB | Application |
| storage | data | 1 MB | Contact storage |

---

## Future Architecture Changes

### Planned Improvements

| Change | Priority | Description |
|--------|----------|-------------|
| Message Queue | High | Async message handling |
| Connection Pool | Medium | Multiple simultaneous connections |
| Storage Abstraction | Medium | Pluggable storage backends |
| UI Layer | High | Display and input handling |

### Post-Quantum Migration

Preparation for hybrid encryption:

1. Kyber KEM already included in components
2. Will be integrated alongside X448
3. Hybrid key exchange: X448 + Kyber768

---

## Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md) for architecture contribution guidelines.

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
