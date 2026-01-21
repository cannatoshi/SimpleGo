# SimpleGo Architecture

> Module structure and code organization (v0.1.14+)

---

## Overview

SimpleGo v0.1.14 introduced a modular architecture, splitting the monolithic ~1800 line `main.c` into 8 dedicated modules.

```
Before (v0.1.13):          After (v0.1.14):
┌──────────────────┐       ┌──────────────────┐
│  main.c          │       │  main.c (~350)   │
│  (~1800 lines)   │  ──>  ├──────────────────┤
│  Everything!     │       │  7 modules       │
└──────────────────┘       │  7 headers       │
                           └──────────────────┘
```

---

## Directory Structure

```
simplex_client/
├── main/
│   ├── main.c              # Application entry, WiFi, main loop
│   ├── smp_globals.c       # Global variable definitions
│   ├── smp_utils.c         # Encoding utilities
│   ├── smp_crypto.c        # Cryptographic operations
│   ├── smp_network.c       # TLS/TCP I/O
│   ├── smp_contacts.c      # Contact management + NVS
│   ├── smp_parser.c        # Agent Protocol parsing
│   ├── smp_peer.c          # Peer server connection (NEW!)
│   ├── include/
│   │   ├── smp_types.h     # All structures and constants
│   │   ├── smp_utils.h     # Encoding function headers
│   │   ├── smp_crypto.h    # Crypto function headers
│   │   ├── smp_network.h   # Network function headers
│   │   ├── smp_contacts.h  # Contact function headers
│   │   ├── smp_parser.h    # Parser function headers
│   │   └── smp_peer.h      # Peer function headers (NEW!)
│   ├── CMakeLists.txt
│   └── idf_component.yml
├── docs/
│   ├── ARCHITECTURE.md     # This file
│   ├── PROTOCOL.md
│   ├── TECHNICAL.md
│   ├── DEVELOPMENT.md
│   ├── DEVNOTES.md
│   └── release-info/
│       └── v0.1.14-alpha.md
├── CHANGELOG.md
├── README.md
├── ROADMAP.md
├── .gitignore              # NEW!
└── CMakeLists.txt
```

---

## Module Details

### smp_types.h (~80 lines)

Central header with all structures and constants.

```c
// Structures
typedef struct { ... } contact_t;
typedef struct { ... } contacts_db_t;
typedef struct { ... } peer_queue_t;

// Constants
#define MAX_CONTACTS 10
#define SMP_BLOCK_SIZE 16384
#define SMP_PORT 5223

// Extern declarations
extern mbedtls_ssl_context ssl;
extern contacts_db_t contacts_db;
extern peer_queue_t pending_peer;
```

---

### smp_globals.c (~25 lines)

Global variable definitions (declared extern in smp_types.h).

```c
#include "smp_types.h"

mbedtls_ssl_context ssl;
mbedtls_ssl_config conf;
mbedtls_net_context server_fd;
contacts_db_t contacts_db;
peer_queue_t pending_peer;
// ... etc
```

---

### smp_utils.c (~100 lines)

Encoding and utility functions.

```c
// Base64
int base64_encode(const uint8_t *in, int len, char *out);
int base64_decode(const char *in, uint8_t *out);
int base64url_encode(const uint8_t *in, int len, char *out);
int base64url_decode(const char *in, uint8_t *out);

// URL encoding
void url_encode(const char *in, char *out, int out_size);
void url_decode_inplace(char *str);

// Hex
void hex_dump(const char *label, const uint8_t *data, size_t len);
```

---

### smp_crypto.c (~80 lines)

Cryptographic operations using libsodium.

```c
// Key generation
void generate_ed25519_keypair(uint8_t *secret, uint8_t *public);
void generate_x25519_keypair(uint8_t *secret, uint8_t *public);

// Signing
int ed25519_sign(const uint8_t *msg, size_t len, 
                 const uint8_t *secret, uint8_t *sig);

// Encryption
int crypto_box_decrypt(const uint8_t *cipher, size_t len,
                       const uint8_t *nonce,
                       const uint8_t *pub, const uint8_t *sec,
                       uint8_t *plain);
```

---

### smp_network.c (~160 lines)

TLS and TCP I/O operations.

```c
// Connection
int smp_tcp_connect(const char *host, int port);  // Renamed from tcp_connect!
int tls_connect(const char *host, int port);
void tls_disconnect(void);

// I/O
int send_block(const uint8_t *data, size_t len);
int receive_block(uint8_t *buf, size_t max_len);

// SMP Handshake
int perform_smp_handshake(void);
```

**Note:** `tcp_connect` was renamed to `smp_tcp_connect` to avoid collision with lwip's `tcp_connect`.

---

### smp_contacts.c (~380 lines)

Contact management and NVS persistence.

```c
// Contact CRUD
int add_contact(const char *name);
int remove_contact(int index);
void list_contacts(void);
contact_t *find_contact_by_recipient_id(const uint8_t *id);

// NVS
void save_contacts_to_nvs(void);
void load_contacts_from_nvs(void);

// Queue operations
int create_queue(contact_t *contact);
int subscribe_queue(contact_t *contact);
int subscribe_all_contacts(void);

// Message handling
void handle_incoming_message(contact_t *contact, const uint8_t *msg, int len);
```

---

### smp_parser.c (~260 lines)

Agent Protocol parsing with auto-connect.

```c
// Message parsing
void parse_agent_message(contact_t *contact, const uint8_t *data, int len);
void handle_invitation(contact_t *contact, const uint8_t *data, int len);

// URI parsing
void parse_smp_uri(const char *uri, peer_queue_t *peer);
void extract_peer_info(const char *invitation);

// Auto-connect trigger
void process_invitation(contact_t *contact);
```

**Auto-Connect Feature:**
```c
if (pending_peer.valid && pending_peer.has_dh) {
    peer_connect(pending_peer.host, pending_peer.port);
    send_agent_confirmation(contact);
    peer_disconnect();
}
```

---

### smp_peer.c (~220 lines) — NEW!

Peer server connection for bidirectional messaging.

```c
// Connection
bool peer_connect(const char *host, int port);
void peer_disconnect(void);

// Handshake
bool peer_handshake(void);

// Confirmation
bool send_agent_confirmation(contact_t *contact);
```

**Usage Flow:**
```
1. peer_connect("smp15.simplex.im", 5223)
2. peer_handshake()  // TLS + SMP handshake
3. send_agent_confirmation(contact)  // SEND to peer's queue
4. peer_disconnect()
```

---

### main.c (~350 lines)

Application entry point and main loop.

```c
void app_main(void) {
    // 1. Initialize NVS
    // 2. Initialize WiFi
    // 3. Wait for connection
    // 4. Connect to home SMP server
    // 5. Load/create contacts
    // 6. Subscribe to queues
    // 7. Print invitation links
    // 8. Main message loop
}
```

---

## Dependencies

### Include Order

```c
// 1. Standard libraries
#include <stdio.h>
#include <string.h>

// 2. ESP-IDF
#include "esp_log.h"
#include "nvs_flash.h"

// 3. mbedTLS
#include "mbedtls/ssl.h"
#include "mbedtls/net_sockets.h"

// 4. libsodium
#include "sodium.h"

// 5. SimpleGo headers
#include "smp_types.h"
#include "smp_utils.h"
#include "smp_crypto.h"
// ... etc
```

### Module Dependencies

```
main.c
  └── smp_types.h
  └── smp_network.h
  └── smp_contacts.h
  └── smp_parser.h
  └── smp_peer.h

smp_peer.c
  └── smp_types.h
  └── smp_network.h
  └── smp_crypto.h

smp_parser.c
  └── smp_types.h
  └── smp_utils.h
  └── smp_peer.h

smp_contacts.c
  └── smp_types.h
  └── smp_crypto.h
  └── smp_network.h
  └── smp_utils.h
```

---

## CMakeLists.txt

```cmake
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
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        nvs_flash 
        esp_wifi 
        esp_netif 
        mbedtls
)
```

---

## Benefits of Modular Architecture

| Aspect | Before | After |
|--------|--------|-------|
| **Compilation** | Full rebuild | Incremental |
| **Testing** | Difficult | Per-module |
| **Readability** | 1800 lines | ~350 max |
| **Collaboration** | Conflicts | Parallel |
| **Maintenance** | Find in 1800 | Find in ~200 |

---

*Last updated: January 21, 2026 — v0.1.14-alpha*
