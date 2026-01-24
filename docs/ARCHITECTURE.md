# SimpleGo Architecture

> Module structure and code organization (v0.1.15)

---

## Overview

SimpleGo v0.1.15 features a fully modular architecture with dedicated crypto modules for Double Ratchet implementation.
`
v0.1.13 (Monolithic):       v0.1.15 (Modular + Crypto):
┌──────────────────┐        ┌──────────────────────────────┐
│  main.c          │        │  main.c                      │
│  (~1800 lines)   │   ──>  ├──────────────────────────────┤
│  Everything!     │        │  12 modules + 4 crypto       │
└──────────────────┘        │  wolfssl + kyber components  │
                            └──────────────────────────────┘
`

---

## Directory Structure
`
simplex_client/
├── main/
│   ├── main.c                  # Application entry, WiFi, main loop
│   ├── smp_globals.c           # Global variable definitions
│   │
│   ├── # === CRYPTO LAYER (NEW in v0.1.15) ===
│   ├── smp_x448.c              # X448 DH with wolfSSL byte-order fix
│   ├── smp_ratchet.c           # Double Ratchet, KDFs, AES-GCM
│   ├── smp_handshake.c         # E2E handshake, AgentConfirmation
│   ├── smp_queue.c             # SMPQueueInfo encoding
│   │
│   ├── # === PROTOCOL LAYER ===
│   ├── smp_peer.c              # Peer server connection
│   ├── smp_parser.c            # Agent Protocol parsing
│   ├── smp_network.c           # TLS/TCP I/O
│   │
│   ├── # === APPLICATION LAYER ===
│   ├── smp_contacts.c          # Contact management + NVS
│   ├── smp_crypto.c            # Ed25519, X25519, crypto_box
│   ├── smp_utils.c             # Base64, URL encoding
│   │
│   ├── include/
│   │   ├── smp_types.h         # All structures and constants
│   │   ├── smp_x448.h          # X448 function headers
│   │   ├── smp_ratchet.h       # Ratchet function headers
│   │   ├── smp_handshake.h     # Handshake function headers
│   │   ├── smp_queue.h         # Queue function headers
│   │   ├── smp_peer.h          # Peer function headers
│   │   ├── smp_parser.h        # Parser function headers
│   │   ├── smp_network.h       # Network function headers
│   │   ├── smp_contacts.h      # Contact function headers
│   │   ├── smp_crypto.h        # Crypto function headers
│   │   └── smp_utils.h         # Utility function headers
│   │
│   ├── CMakeLists.txt
│   └── idf_component.yml
│
├── components/
│   ├── wolfssl__wolfssl/       # X448/Curve448 operations
│   ├── wolfssl_config/         # wolfSSL ESP32 configuration
│   ├── aes256ctr/              # Kyber AES component
│   ├── cbd/                    # Kyber CBD component
│   ├── fips202/                # Kyber FIPS202 component
│   ├── indcpa/                 # Kyber IndCPA component
│   ├── kem/                    # Kyber KEM component
│   ├── kex/                    # Kyber KEX component
│   ├── ntt/                    # Kyber NTT component
│   ├── poly/                   # Kyber Poly component
│   ├── polyvec/                # Kyber Polyvec component
│   ├── randombytes/            # Kyber RNG component
│   ├── reduce/                 # Kyber Reduce component
│   ├── sha2/                   # Kyber SHA2 component
│   ├── symmetric/              # Kyber Symmetric component
│   └── verify/                 # Kyber Verify component
│
├── docs/
│   ├── ARCHITECTURE.md         # This file
│   ├── PROTOCOL.md             # SMP protocol details
│   ├── CRYPTO.md               # Cryptography documentation
│   ├── WIRE_FORMAT.md          # Wire format specification
│   ├── BUGS.md                 # Bug tracker
│   ├── TECHNICAL.md            # Technical notes
│   ├── DEVELOPMENT.md          # Development guide
│   ├── DEVNOTES.md             # Session notes
│   └── release-info/
│       ├── v0.1.14-alpha.md
│       └── v0.1.15-alpha.md
│
├── .gitignore
├── CHANGELOG.md
├── README.md
├── ROADMAP.md
├── CMakeLists.txt
├── partitions.csv
└── sdkconfig
`

---

## Module Details

### Crypto Layer (NEW in v0.1.15)

#### smp_x448.c (~200 lines)

X448 key operations with wolfSSL byte-order compatibility fix.
`c
// Key generation
int x448_generate_keypair(x448_keypair_t *keypair);

// Diffie-Hellman
int x448_dh(const uint8_t *their_public, 
            const uint8_t *my_private,
            uint8_t *shared_secret);

// CRITICAL: wolfSSL uses reversed byte order!
static void reverse_bytes(const uint8_t *src, uint8_t *dst, size_t len);
`

#### smp_ratchet.c (~500 lines)

Complete Double Ratchet implementation.
`c
// Ratchet state
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

// KDF functions
int root_kdf(const uint8_t *root_key, const uint8_t *dh_output,
             uint8_t *new_root, uint8_t *chain_key, uint8_t *next_header);

int chain_kdf(const uint8_t *chain_key,
              uint8_t *msg_key, uint8_t *new_chain,
              uint8_t *header_iv, uint8_t *msg_iv);

// Encryption
int encrypt_header(const ratchet_state_t *state,
                   const uint8_t *header, size_t header_len,
                   const uint8_t *aad, size_t aad_len,
                   uint8_t *ciphertext, uint8_t *tag);

int encrypt_payload(const uint8_t *key, const uint8_t *iv,
                    const uint8_t *aad, size_t aad_len,
                    const uint8_t *plaintext, size_t plain_len,
                    uint8_t *ciphertext, uint8_t *tag);
`

#### smp_handshake.c (~300 lines)

E2E handshake and message building.
`c
// X3DH key agreement
int x3dh_sender(const uint8_t *peer_spk, const uint8_t *peer_rk,
                const x448_keypair_t *our_ek, const x448_keypair_t *our_rk,
                uint8_t *header_key, uint8_t *next_header_key, uint8_t *root_key);

// Message building
int build_agent_confirmation(contact_t *contact, uint8_t *output, size_t *len);
int build_hello_message(contact_t *contact, uint8_t *output, size_t *len);
int build_msg_header(const ratchet_state_t *state, uint8_t *header);
int build_enc_message_header(const uint8_t *header, const uint8_t *key,
                              const uint8_t *iv, const uint8_t *aad,
                              uint8_t *output, size_t *len);
`

#### smp_queue.c (~250 lines)

SMPQueueInfo encoding for AgentConnInfo.
`c
// Queue info encoding
int encode_smp_queue_info(const smp_queue_info_t *info, 
                          uint8_t *output, size_t *len);

int encode_agent_conn_info(const agent_conn_info_t *info,
                           uint8_t *output, size_t *len);

// Wire format helpers
void write_word16_be(uint8_t *buf, uint16_t value);
void write_word32_be(uint8_t *buf, uint32_t value);
`

---

### Protocol Layer

#### smp_peer.c (~220 lines)

Peer server connection for bidirectional messaging.
`c
bool peer_connect(const char *host, int port);
void peer_disconnect(void);
bool peer_handshake(void);
bool send_agent_confirmation(contact_t *contact);
bool send_hello_message(contact_t *contact);
`

#### smp_parser.c (~260 lines)

Agent Protocol parsing with auto-connect.
`c
void parse_agent_message(contact_t *contact, const uint8_t *data, int len);
void handle_invitation(contact_t *contact, const uint8_t *data, int len);
void parse_smp_uri(const char *uri, peer_queue_t *peer);
`

#### smp_network.c (~160 lines)

TLS and TCP I/O operations.
`c
int smp_tcp_connect(const char *host, int port);
int tls_connect(const char *host, int port);
void tls_disconnect(void);
int send_block(const uint8_t *data, size_t len);
int receive_block(uint8_t *buf, size_t max_len);
int perform_smp_handshake(void);
`

---

### Application Layer

#### smp_contacts.c (~380 lines)

Contact management and NVS persistence.
`c
int add_contact(const char *name);
int remove_contact(int index);
void list_contacts(void);
contact_t *find_contact_by_recipient_id(const uint8_t *id);
void save_contacts_to_nvs(void);
void load_contacts_from_nvs(void);
int create_queue(contact_t *contact);
int subscribe_queue(contact_t *contact);
`

#### smp_crypto.c (~80 lines)

Basic cryptographic operations using libsodium.
`c
void generate_ed25519_keypair(uint8_t *secret, uint8_t *public);
void generate_x25519_keypair(uint8_t *secret, uint8_t *public);
int ed25519_sign(const uint8_t *msg, size_t len, 
                 const uint8_t *secret, uint8_t *sig);
int crypto_box_decrypt(const uint8_t *cipher, size_t len,
                       const uint8_t *nonce,
                       const uint8_t *pub, const uint8_t *sec,
                       uint8_t *plain);
`

#### smp_utils.c (~100 lines)

Encoding and utility functions.
`c
int base64_encode(const uint8_t *in, int len, char *out);
int base64_decode(const char *in, uint8_t *out);
int base64url_encode(const uint8_t *in, int len, char *out);
int base64url_decode(const char *in, uint8_t *out);
void url_encode(const char *in, char *out, int out_size);
void url_decode_inplace(char *str);
`

---

## Component Dependencies

### wolfSSL

Provides X448/Curve448 elliptic curve operations.
`
components/wolfssl__wolfssl/
├── wolfcrypt/src/
│   ├── curve448.c      # X448 implementation
│   ├── fe_448.c        # Field elements
│   ├── ge_448.c        # Group elements
│   └── ...
└── include/
    └── user_settings.h # ESP32 configuration
`

**Critical:** wolfSSL uses reversed byte order for X448 keys compared to OpenSSL/cryptonite!

### Kyber (Post-Quantum)

Prepared for future PQ crypto support (SimpleX v3+).
`
components/
├── kem/        # Key Encapsulation Mechanism
├── indcpa/     # IND-CPA secure encryption
├── poly/       # Polynomial operations
└── ...
`

---

## Build Configuration

### CMakeLists.txt
`cmake
idf_component_register(
    SRCS 
        "main.c"
        "smp_globals.c"
        "smp_utils.c"
        "smp_crypto.c"
        "smp_network.c"
        "smp_contacts.c"
        "smp_parser.c"
        "smp_peer.c"
        "smp_x448.c"
        "smp_ratchet.c"
        "smp_handshake.c"
        "smp_queue.c"
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        nvs_flash 
        esp_wifi 
        esp_netif 
        mbedtls
)
`

### idf_component.yml
`yaml
dependencies:
  espressif/libsodium: "^1.0"
  wolfssl/wolfssl: "^5.6"
`

---

## Data Flow

### Outgoing Message (AgentConfirmation)
`
┌─────────────────────────────────────────────────────────────────┐
│  1. build_agent_confirmation()                                  │
│     └── Creates AgentConfirmation structure                     │
├─────────────────────────────────────────────────────────────────┤
│  2. x3dh_sender()                                               │
│     └── Derives header_key, root_key from DH                    │
├─────────────────────────────────────────────────────────────────┤
│  3. build_msg_header()                                          │
│     └── Creates MsgHeader with DH key, counters                 │
├─────────────────────────────────────────────────────────────────┤
│  4. chain_kdf()                                                 │
│     └── Derives msg_key, header_iv, msg_iv                      │
├─────────────────────────────────────────────────────────────────┤
│  5. encrypt_header() + encrypt_payload()                        │
│     └── AES-GCM encryption with AAD                             │
├─────────────────────────────────────────────────────────────────┤
│  6. send_block()                                                │
│     └── Sends via TLS to peer server                            │
└─────────────────────────────────────────────────────────────────┘
`

### Incoming Message (AgentInvitation)
`
┌─────────────────────────────────────────────────────────────────┐
│  1. receive_block()                                             │
│     └── Receives via TLS                                        │
├─────────────────────────────────────────────────────────────────┤
│  2. Layer 3-5 decryption                                        │
│     └── E2E decrypt, crypto_box                                 │
├─────────────────────────────────────────────────────────────────┤
│  3. parse_agent_message()                                       │
│     └── Identifies message type                                 │
├─────────────────────────────────────────────────────────────────┤
│  4. handle_invitation()                                         │
│     └── Extracts peer info, triggers auto-connect               │
├─────────────────────────────────────────────────────────────────┤
│  5. peer_connect() + send_agent_confirmation()                  │
│     └── Sends confirmation to peer                              │
└─────────────────────────────────────────────────────────────────┘
`

---

## Memory Layout

### Stack Usage (approximate)

| Function | Stack |
|----------|-------|
| x3dh_sender | ~500 bytes |
| encrypt_header | ~300 bytes |
| encrypt_payload | ~300 bytes |
| build_agent_confirmation | ~2KB |
| Total peak | ~4KB |

### Heap Usage

| Allocation | Size |
|------------|------|
| TLS context | ~40KB |
| Ratchet state | ~300 bytes |
| Contact DB | ~5KB |
| Message buffer | 16KB |

---

## Performance (ESP32-S3)

| Operation | Time |
|-----------|------|
| X448 keygen | ~15ms |
| X448 DH | ~15ms |
| X3DH (3x DH + HKDF) | ~50ms |
| AES-GCM encrypt | ~1ms |
| Full AgentConfirmation | ~100ms |
| TLS handshake | ~800ms |

---

*Last updated: January 24, 2026 — v0.1.15-alpha*
