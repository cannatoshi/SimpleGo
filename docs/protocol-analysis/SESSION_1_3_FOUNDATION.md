# Session 1-3: Foundation

## Project Initialization and Basic Protocol Implementation

**Date Range:** December 24, 2025 - January 22, 2026
**Version:** v0.1.0 to v0.1.13-alpha

---

## 🚀 Project Vision

**SimpleGo: The First Native ESP32 Implementation of the SimpleX Messaging Protocol**

This project aims to bring truly private, decentralized messaging to dedicated hardware devices - no smartphone required. By implementing the SMP protocol natively on ESP32 microcontrollers, we enable a new class of secure communication devices.

---

## 📋 Table of Contents

1. [Project Goals](#1-project-goals)
2. [Development Environment](#2-development-environment)
3. [ESP-IDF Setup](#3-esp-idf-setup)
4. [WiFi Implementation](#4-wifi-implementation)
5. [TCP Socket Layer](#5-tcp-socket-layer)
6. [TLS 1.3 Connection](#6-tls-13-connection)
7. [SMP Protocol Overview](#7-smp-protocol-overview)
8. [SMP Handshake](#8-smp-handshake)
9. [Queue Management](#9-queue-management)
10. [Ed25519 Signatures](#10-ed25519-signatures)
11. [X25519 Key Exchange](#11-x25519-key-exchange)
12. [Session Summary](#12-session-summary)
13. [Changelog](#13-changelog)

---

## 1. Project Goals

### 1.1 Primary Objectives

| Objective | Description | Status |
|-----------|-------------|--------|
| Native SMP | Implement SMP protocol in C, not a wrapper | ✅ |
| ESP32 Target | Run on LilyGo T-Deck and similar devices | ✅ |
| No Smartphone | Operate independently without a phone | In Progress |
| Full Encryption | X3DH + Double Ratchet encryption | ✅ |
| App Compatible | Communicate with official SimpleX apps | In Progress |

### 1.2 Why Native Implementation?
`
Existing "SimpleX Clients":
├── simplex-python     → WebSocket wrapper around CLI
├── SimplOxide (Rust)  → WebSocket wrapper
├── TypeScript SDK     → WebSocket wrapper
└── All others         → WebSocket wrappers

SimpleGo:
└── NATIVE binary SMP protocol implementation!
    └── Direct TCP/TLS to SMP servers
    └── Full protocol stack in C
    └── True embedded system client
`

---

## 2. Development Environment

### 2.1 Hardware

| Device | Chip | RAM | Flash | Status |
|--------|------|-----|-------|--------|
| LilyGo T-Deck | ESP32-S3 | 8MB PSRAM | 16MB | Primary Target |
| LilyGo T-Embed | ESP32-S3 | 8MB PSRAM | 16MB | Secondary |
| Generic ESP32-S3 | ESP32-S3 | Varies | Varies | Supported |

### 2.2 Software Stack

| Component | Version | Purpose |
|-----------|---------|---------|
| ESP-IDF | 5.5.2 | Development framework |
| mbedTLS | 3.x | TLS 1.3, AES-GCM, HKDF |
| wolfSSL | 5.x | X448/Curve448 support |
| libsodium | 1.0.x | Ed25519, X25519 |

### 2.3 Project Path
`
Windows: C:\Espressif\projects\simplex_client
Linux:   ~/projects/simplex_client
`

---

## 3. ESP-IDF Setup

### 3.1 Installation (Windows)
`powershell
# Create directory
mkdir C:\Espressif
cd C:\Espressif

# Clone ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Install tools
.\install.ps1 esp32s3

# Activate environment (run in each new terminal)
C:\Espressif\esp-idf\export.ps1
`

### 3.2 Project Creation
`powershell
# Create project from template
idf.py create-project simplex_client
cd simplex_client

# Set target
idf.py set-target esp32s3

# Configure
idf.py menuconfig
`

### 3.3 Build Commands

| Command | Description |
|---------|-------------|
| idf.py build | Compile the project |
| idf.py flash -p COM5 | Flash to device |
| idf.py monitor -p COM5 | Open serial monitor |
| idf.py build flash monitor -p COM5 | All in one |

---

## 4. WiFi Implementation

### 4.1 WiFi Station Mode
`c
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#define WIFI_SSID "YourNetwork"
#define WIFI_PASS "YourPassword"

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init_sta(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Configure WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    // Set credentials
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
`

### 4.2 Connection Status
`
WiFi Connection Flow:
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│ WIFI_EVENT  │───►│ STA_START   │───►│ Connecting  │
│ STA_START   │    │ Connect()   │    │ ...         │
└─────────────┘    └─────────────┘    └──────┬──────┘
                                             │
                   ┌─────────────┐    ┌──────▼──────┐
                   │ Got IP!     │◄───│ IP_EVENT    │
                   │ Ready!      │    │ STA_GOT_IP  │
                   └─────────────┘    └─────────────┘
`

---

## 5. TCP Socket Layer

### 5.1 Socket Creation
`c
#include "lwip/sockets.h"
#include "lwip/netdb.h"

int create_tcp_socket(const char *host, int port) {
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res;
    
    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", port);
    
    int err = getaddrinfo(host, port_str, &hints, &res);
    if (err != 0 || res == NULL) {
        ESP_LOGE(TAG, "DNS lookup failed: %d", err);
        return -1;
    }
    
    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Socket creation failed");
        freeaddrinfo(res);
        return -1;
    }
    
    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "Socket connect failed");
        close(sock);
        freeaddrinfo(res);
        return -1;
    }
    
    freeaddrinfo(res);
    return sock;
}
`

---

## 6. TLS 1.3 Connection

### 6.1 Why TLS 1.3?

SimpleX SMP servers require TLS 1.3 with ALPN "smp/1". This provides:
- Perfect forward secrecy
- Reduced handshake latency
- Modern cipher suites

### 6.2 mbedTLS Configuration
`c
#include "esp_tls.h"

esp_tls_cfg_t tls_cfg = {
    .alpn_protos = (const char *[]){"smp/1", NULL},
    .skip_common_name = false,
    .use_global_ca_store = true,
    .timeout_ms = 10000,
};

esp_tls_t *tls = esp_tls_init();
if (esp_tls_conn_new_sync(host, strlen(host), port, &tls_cfg, tls) != 1) {
    ESP_LOGE(TAG, "TLS connection failed");
    return -1;
}
ESP_LOGI(TAG, "TLS 1.3 connected with ALPN: smp/1");
`

### 6.3 Certificate Handling
`c
// Using global CA store (recommended)
extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[] asm("_binary_server_root_cert_pem_end");

esp_err_t ret = esp_tls_init_global_ca_store();
ret = esp_tls_set_global_ca_store(server_root_cert_pem_start,
                                   server_root_cert_pem_end - server_root_cert_pem_start);
`

---

## 7. SMP Protocol Overview

### 7.1 Protocol Stack
`
SimpleX Messaging Protocol Stack:
═══════════════════════════════════════════════════════════════════

┌─────────────────────────────────────────────────────────────────┐
│                    Chat Protocol (JSON)                         │
│                 {"event":"x.msg","params":{...}}                │
├─────────────────────────────────────────────────────────────────┤
│                    Agent Protocol                               │
│           AgentConfirmation, AgentInvitation, etc.              │
├─────────────────────────────────────────────────────────────────┤
│                    Double Ratchet                               │
│              X3DH, Header Encryption, Body Encryption           │
├─────────────────────────────────────────────────────────────────┤
│                    SMP Protocol                                 │
│          Commands: NEW, KEY, SUB, SEND, ACK, OFF, DEL           │
├─────────────────────────────────────────────────────────────────┤
│                    TLS 1.3 (ALPN: smp/1)                        │
├─────────────────────────────────────────────────────────────────┤
│                    TCP/IP                                       │
└─────────────────────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════
`

### 7.2 SMP Commands

| Command | Direction | Description |
|---------|-----------|-------------|
| NEW | Client → Server | Create new queue |
| KEY | Client → Server | Secure queue with key |
| SUB | Client → Server | Subscribe to queue |
| SEND | Client → Server | Send message to queue |
| ACK | Client → Server | Acknowledge message |
| OFF | Client → Server | Suspend queue |
| DEL | Client → Server | Delete queue |

### 7.3 SMP Responses

| Response | Description |
|----------|-------------|
| IDS | Queue IDs (after NEW) |
| MSG | Message delivered |
| NMSG | New message notification |
| OK | Command succeeded |
| ERR | Command failed |
| END | Queue deleted/ended |

---

## 8. SMP Handshake

### 8.1 Initial Connection
`
SMP Handshake Flow:
═══════════════════════════════════════════════════════════════════

Client                              Server
   │                                   │
   │ ──── TLS 1.3 Handshake ─────────► │
   │ ◄─── TLS Established ──────────── │
   │                                   │
   │ ──── SMP Version Info ──────────► │
   │ ◄─── Server Version ───────────── │
   │                                   │
   │ ──── NEW (create queue) ────────► │
   │ ◄─── IDS (queue IDs) ───────────  │
   │                                   │

═══════════════════════════════════════════════════════════════════
`

### 8.2 SMP Transmission Block

Every SMP message is wrapped in a transmission block:
`
SMP Transmission Block:
┌─────────────┬────────────────────────────────────────┐
│ Length      │ Payload                                │
│ (2 bytes)   │ (variable)                             │
│ Big Endian  │                                        │
└─────────────┴────────────────────────────────────────┘
`

### 8.3 Session ID and Commands
`c
// SMP command structure
typedef struct {
    uint8_t session_id[32];  // Zeros for initial commands
    uint8_t command;         // Command byte
    uint8_t *payload;        // Command-specific data
    size_t payload_len;
} smp_command_t;
`

---

## 9. Queue Management

### 9.1 Queue Concept

In SimpleX, communication happens through **queues**:
- Each queue is unidirectional
- Sender and Recipient have different keys
- Queues are identified by random IDs
`
Simplex Queue Model:
═══════════════════════════════════════════════════════════════════

              ┌──────────────────────────────┐
              │        SMP Server            │
              │                              │
   Sender ───►│  Queue (senderId, rcvId)  ───│───► Recipient
              │                              │
              └──────────────────────────────┘

- Sender knows: senderId, sender private key
- Recipient knows: rcvId, recipient private key
- Server stores: encrypted messages

═══════════════════════════════════════════════════════════════════
`

### 9.2 NEW Command
`c
// Create new queue
int smp_create_queue(smp_connection_t *conn, smp_queue_t *queue) {
    uint8_t cmd[128];
    int p = 0;
    
    // Session ID (32 zeros for new)
    memset(cmd, 0, 32);
    p += 32;
    
    // Command: NEW
    cmd[p++] = 'N';
    
    // Recipient public key (X25519)
    cmd[p++] = 44;  // SPKI length
    memcpy(&cmd[p], queue->rcv_public_spki, 44);
    p += 44;
    
    // DH public key
    cmd[p++] = 44;
    memcpy(&cmd[p], queue->dh_public_spki, 44);
    p += 44;
    
    return smp_send_command(conn, cmd, p);
}
`

---

## 10. Ed25519 Signatures

### 10.1 Why Ed25519?

SimpleX uses Ed25519 for:
- Queue authentication (sender proves ownership)
- Message signing
- Preventing replay attacks

### 10.2 libsodium Integration
`c
#include "sodium.h"

// Generate Ed25519 keypair
int generate_ed25519_keypair(uint8_t *public_key, uint8_t *secret_key) {
    return crypto_sign_keypair(public_key, secret_key);
}

// Sign message
int sign_message(const uint8_t *message, size_t msg_len,
                 const uint8_t *secret_key,
                 uint8_t *signature) {
    unsigned long long sig_len;
    return crypto_sign_detached(signature, &sig_len, message, msg_len, secret_key);
}

// Verify signature
int verify_signature(const uint8_t *message, size_t msg_len,
                     const uint8_t *signature,
                     const uint8_t *public_key) {
    return crypto_sign_verify_detached(signature, message, msg_len, public_key);
}
`

### 10.3 SPKI Format

SimpleX transmits Ed25519 keys in SPKI (Subject Public Key Info) format:
`
Ed25519 SPKI (44 bytes):
┌──────────────────────────────────┬────────────────────────────────┐
│ Header (12 bytes)                │ Raw Key (32 bytes)             │
│ 30 2a 30 05 06 03 2b 65 70      │ [public key bytes]             │
│ 03 21 00                         │                                │
└──────────────────────────────────┴────────────────────────────────┘
`

---

## 11. X25519 Key Exchange

### 11.1 Per-Queue Encryption

Each SMP queue uses X25519 for encrypting messages between sender and server (not E2E - that's Double Ratchet).
`c
// Generate X25519 keypair
int generate_x25519_keypair(uint8_t *public_key, uint8_t *secret_key) {
    crypto_box_keypair(public_key, secret_key);
    return 0;
}

// Compute shared secret
int x25519_shared_secret(uint8_t *shared_secret,
                         const uint8_t *their_public,
                         const uint8_t *my_secret) {
    return crypto_scalarmult(shared_secret, my_secret, their_public);
}
`

### 11.2 X25519 SPKI Format
`
X25519 SPKI (44 bytes):
┌──────────────────────────────────┬────────────────────────────────┐
│ Header (12 bytes)                │ Raw Key (32 bytes)             │
│ 30 2a 30 05 06 03 2b 65 6e      │ [public key bytes]             │
│ 03 21 00                         │                                │
└──────────────────────────────────┴────────────────────────────────┘
`

---

## 12. Session Summary

### 12.1 Achievements (Sessions 1-3)

| Milestone | Status | Notes |
|-----------|--------|-------|
| ESP-IDF project setup | ✅ | v5.5.2 |
| WiFi connectivity | ✅ | Station mode |
| TCP socket layer | ✅ | lwIP sockets |
| TLS 1.3 connection | ✅ | mbedTLS, ALPN smp/1 |
| SMP version negotiation | ✅ | Version 1-4 |
| Queue creation (NEW) | ✅ | IDS response |
| Ed25519 signatures | ✅ | libsodium |
| X25519 key exchange | ✅ | libsodium |

### 12.2 Code Statistics

| Metric | Value |
|--------|-------|
| Total lines | ~1500 |
| Modules | 3 (main, network, crypto) |
| Libraries | mbedTLS, libsodium |

### 12.3 Next Steps (Leading to Session 4)

1. Implement Double Ratchet encryption
2. Parse invitation links
3. Build AgentConfirmation message
4. Achieve E2E encrypted communication

---

## 13. Changelog

| Date | Change |
|------|--------|
| 2025-12-24 | Project initialized |
| 2025-12-26 | WiFi connectivity implemented |
| 2025-12-28 | TCP socket layer working |
| 2025-12-30 | TLS 1.3 connection established |
| 2026-01-01 | SMP handshake implemented |
| 2026-01-03 | Queue creation working |
| 2026-01-05 | Ed25519 signatures integrated |
| 2026-01-07 | X25519 key exchange working |
| 2026-01-09 | Basic SMP protocol complete |

---

*Document version: Session 1-3 Complete*
*Last updated: January 22, 2026*
