# SimpleGo Technical Documentation

Low-level technical details for SimpleGo development and debugging.

---

## Build Environment

### Required Tools

| Tool | Version | Purpose |
|------|---------|---------|
| ESP-IDF | 5.5.2+ | Development framework |
| Python | 3.8+ | Build scripts, verification |
| CMake | 3.16+ | Build system |
| Ninja | 1.10+ | Build backend |

### Environment Setup (Windows)
`powershell
# Install ESP-IDF
cd C:\Espressif
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
install.ps1

# Activate environment
C:\Espressif\esp-idf\export.ps1

# Navigate to project
cd C:\Espressif\projects\simplex_client
`

### Environment Setup (Linux)
`ash
# Install ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh

# Activate environment
source ~/esp/esp-idf/export.sh

# Navigate to project
cd ~/projects/simplex_client
`

---

## Build Commands

### Full Build and Flash
`powershell
idf.py build flash monitor -p COM5
`

### Individual Commands

| Command | Description |
|---------|-------------|
| idf.py build | Compile project |
| idf.py flash -p COM5 | Flash to device |
| idf.py monitor -p COM5 | Serial monitor |
| idf.py clean | Clean build artifacts |
| idf.py fullclean | Full clean including config |
| idf.py menuconfig | Configuration menu |
| idf.py size | Binary size analysis |
| idf.py size-components | Component size breakdown |

### Build Configuration
`powershell
# Open configuration menu
idf.py menuconfig

# Key settings:
# - Component config → mbedTLS → TLS 1.3 support
# - Component config → ESP-TLS → Certificate verification
# - Serial flasher config → Flash size (4MB recommended)
`

---

## Memory Configuration

### Partition Table (partitions.csv)
`csv
# Name,   Type, SubType, Offset,  Size,    Flags
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 0x180000,
storage,  data, spiffs,  0x190000,0x70000,
`

### Memory Usage

| Region | Size | Usage |
|--------|------|-------|
| IRAM | 128 KB | Code, interrupt handlers |
| DRAM | 320 KB | Data, heap, stack |
| PSRAM | 2-8 MB | Extended heap (optional) |
| Flash | 4-16 MB | Code, read-only data |

### Heap Analysis
`c
// Print heap info
ESP_LOGI(TAG, "Free heap: %lu", esp_get_free_heap_size());
ESP_LOGI(TAG, "Min free heap: %lu", esp_get_minimum_free_heap_size());

// Detailed heap info
heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
`

### Stack Configuration

| Task | Stack Size | Notes |
|------|------------|-------|
| Main task | 8192 bytes | Application logic |
| Network task | 4096 bytes | TLS operations need more |
| Crypto task | 4096 bytes | Key operations |

---

## Component Configuration

### wolfSSL Configuration

Location: components/wolfssl/wolfssl_config/user_settings.h
`c
// Key settings for X448 support
#define HAVE_CURVE448
#define CURVE448_SMALL
#define HAVE_ED448
#define WOLFSSL_SHA512

// Disable unused features for size
#define NO_RSA
#define NO_DSA
#define NO_DH
#define NO_RC4
#define NO_MD4
#define NO_MD5
#define NO_SHA

// ESP32 optimizations
#define FREERTOS
#define WOLFSSL_ESPIDF
#define WOLFSSL_ESP32
`

### mbedTLS Configuration

Via menuconfig or sdkconfig:
`
CONFIG_MBEDTLS_SSL_PROTO_TLS1_3=y
CONFIG_MBEDTLS_HKDF_C=y
CONFIG_MBEDTLS_GCM_C=y
CONFIG_MBEDTLS_SHA512_C=y
CONFIG_MBEDTLS_ECDH_C=y
CONFIG_MBEDTLS_ECP_C=y
`

### libsodium Configuration
`
CONFIG_LIBSODIUM_USE_MBEDTLS_SHA=y
`

---

## Debugging

### Serial Monitor
`powershell
idf.py monitor -p COM5
`

### Log Levels
`c
// Set log level per tag
esp_log_level_set("SMP_NETWORK", ESP_LOG_DEBUG);
esp_log_level_set("SMP_CRYPTO", ESP_LOG_VERBOSE);

// Log macros
ESP_LOGE(TAG, "Error: %s", message);    // Error
ESP_LOGW(TAG, "Warning: %s", message);  // Warning
ESP_LOGI(TAG, "Info: %s", message);     // Info
ESP_LOGD(TAG, "Debug: %s", message);    // Debug
ESP_LOGV(TAG, "Verbose: %s", message);  // Verbose
`

### Hex Dump Helper
`c
void hex_dump(const char *label, const uint8_t *data, size_t len) {
    ESP_LOGI(TAG, "%s (%d bytes):", label, len);
    for (size_t i = 0; i < len; i += 16) {
        char line[64];
        int pos = 0;
        for (size_t j = i; j < i + 16 && j < len; j++) {
            pos += sprintf(line + pos, "%02x ", data[j]);
        }
        ESP_LOGI(TAG, "  %04x: %s", i, line);
    }
}
`

### GDB Debugging
`powershell
# Start GDB server
idf.py openocd

# In another terminal
idf.py gdb
`

### Core Dump Analysis
`powershell
# Enable core dump in menuconfig
# Component config → ESP System Settings → Core dump → Enable

# Analyze core dump
idf.py coredump-info
idf.py coredump-debug
`

---

## Network Debugging

### WiFi Connection
`c
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started, connecting...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
                esp_wifi_connect();
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        }
    }
}
`

### TLS Debug Output
`c
// Enable mbedTLS debug
#include "mbedtls/debug.h"

void my_debug(void *ctx, int level, const char *file, 
              int line, const char *str) {
    ESP_LOGD("mbedTLS", "%s:%d: %s", file, line, str);
}

// In TLS setup
mbedtls_ssl_conf_dbg(&conf, my_debug, NULL);
mbedtls_debug_set_threshold(4);  // 0-4, higher = more verbose
`

### Network Packet Capture

For debugging protocol issues, capture traffic on the server side:
`ash
# On SimpleX server (Linux)
tcpdump -i any port 5223 -w capture.pcap

# Analyze with Wireshark
wireshark capture.pcap
`

---

## Cryptographic Debugging

### Key Verification
`c
void verify_x448_keypair(const uint8_t *public_key, 
                         const uint8_t *private_key) {
    ESP_LOGI(TAG, "X448 Public Key:");
    hex_dump("  ", public_key, 56);
    
    ESP_LOGI(TAG, "X448 Private Key:");
    hex_dump("  ", private_key, 56);
    
    // Verify by computing public from private
    uint8_t computed_public[56];
    wc_curve448_make_pub(56, computed_public, 56, private_key);
    smp_x448_reverse_bytes(computed_public, 56);
    
    if (memcmp(public_key, computed_public, 56) == 0) {
        ESP_LOGI(TAG, "Key pair VALID");
    } else {
        ESP_LOGE(TAG, "Key pair INVALID");
    }
}
`

### KDF Output Verification
`c
void verify_chain_kdf(const uint8_t *chain_key) {
    uint8_t message_key[32], new_chain_key[32];
    uint8_t header_iv[16], message_iv[16];
    
    smp_ratchet_chain_kdf(chain_key, message_key, new_chain_key,
                          header_iv, message_iv);
    
    ESP_LOGI(TAG, "Chain KDF Output:");
    hex_dump("  message_key", message_key, 32);
    hex_dump("  new_chain_key", new_chain_key, 32);
    hex_dump("  header_iv", header_iv, 16);
    hex_dump("  message_iv", message_iv, 16);
}
`

### Python Verification Script
`python
#!/usr/bin/env python3
"""Verify ESP32 cryptographic output against Python reference."""

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.backends import default_backend

def verify_chain_kdf(chain_key_hex: str):
    """Verify Chain KDF matches ESP32 output."""
    chain_key = bytes.fromhex(chain_key_hex)
    
    hkdf = HKDF(
        algorithm=hashes.SHA512(),
        length=96,
        salt=b'',
        info=b'SimpleXChainRatchet',
        backend=default_backend()
    )
    
    output = hkdf.derive(chain_key)
    
    print(f"message_key:   {output[0:32].hex()}")
    print(f"new_chain_key: {output[32:64].hex()}")
    print(f"header_iv:     {output[64:80].hex()}")
    print(f"message_iv:    {output[80:96].hex()}")

if __name__ == "__main__":
    # Replace with actual chain key from ESP32 output
    verify_chain_kdf("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef")
`

---

## Performance Optimization

### Timing Measurements
`c
#include "esp_timer.h"

int64_t start = esp_timer_get_time();
// ... operation ...
int64_t end = esp_timer_get_time();
ESP_LOGI(TAG, "Operation took %lld us", end - start);
`

### Typical Operation Times (ESP32-S3 @ 240MHz)

| Operation | Time |
|-----------|------|
| X448 key generation | ~15 ms |
| X448 DH | ~15 ms |
| HKDF-SHA512 (96 bytes) | ~0.5 ms |
| AES-GCM encrypt (1KB) | ~0.3 ms |
| TLS handshake | ~500 ms |

### Optimization Techniques

| Technique | Benefit |
|-----------|---------|
| Pre-generate keys | Reduce latency on send |
| Buffer reuse | Reduce heap fragmentation |
| Async operations | Improve responsiveness |
| Hardware AES | Faster encryption |

---

## Error Codes

### ESP-IDF Errors

| Code | Name | Description |
|------|------|-------------|
| 0 | ESP_OK | Success |
| 0x101 | ESP_ERR_NO_MEM | Out of memory |
| 0x102 | ESP_ERR_INVALID_ARG | Invalid argument |
| 0x103 | ESP_ERR_INVALID_STATE | Invalid state |
| 0x104 | ESP_ERR_INVALID_SIZE | Invalid size |
| 0x105 | ESP_ERR_NOT_FOUND | Not found |

### mbedTLS Errors

| Code | Description |
|------|-------------|
| -0x7780 | SSL handshake failure |
| -0x7200 | Certificate verification failed |
| -0x6800 | Fatal alert received |
| -0x6280 | Connection reset |
| -0x0010 | GCM auth failed |

### wolfSSL Errors

| Code | Description |
|------|-------------|
| -170 | ECC_CURVE_OID_E (unsupported curve) |
| -173 | ECC_BAD_ARG_E (bad argument) |
| -180 | CURVE448_E (Curve448 error) |

---

## Test Infrastructure

### Unit Test Setup
`c
#include "unity.h"

void test_chain_kdf_output_size(void) {
    uint8_t chain_key[32] = {0};
    uint8_t message_key[32], new_chain[32];
    uint8_t header_iv[16], message_iv[16];
    
    int ret = smp_ratchet_chain_kdf(chain_key, message_key, 
                                     new_chain, header_iv, message_iv);
    
    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_NOT_NULL(message_key);
}

void app_main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_chain_kdf_output_size);
    UNITY_END();
}
`

### Test Vectors
`c
// Known X3DH test vector
const uint8_t test_dh1[56] = { /* ... */ };
const uint8_t test_dh2[56] = { /* ... */ };
const uint8_t test_dh3[56] = { /* ... */ };
const uint8_t expected_header_key[32] = { /* ... */ };
const uint8_t expected_next_header_key[32] = { /* ... */ };
const uint8_t expected_root_key[32] = { /* ... */ };
`

---

## Haskell Source Reference

### Key Files in simplexmq Repository

| File | Content |
|------|---------|
| src/Simplex/Messaging/Protocol.hs | SMP protocol definitions |
| src/Simplex/Messaging/Agent/Protocol.hs | Agent protocol |
| src/Simplex/Messaging/Crypto.hs | Cryptographic operations |
| src/Simplex/Messaging/Crypto/Ratchet.hs | Double Ratchet |
| src/Simplex/Messaging/Encoding.hs | Wire format encoding |

### Finding Encoding Details
`ash
# In WSL with simplexmq cloned
cd ~/simplexmq

# Search for specific encoding
grep -r "smpEncode" src/
grep -r "ByteString" src/Simplex/Messaging/Encoding.hs

# Find specific structure
grep -r "AgentConfirmation" src/
grep -r "EncRatchetMessage" src/
`

---

## Troubleshooting Guide

### Build Failures

| Error | Solution |
|-------|----------|
| Component not found | Run idf.py reconfigure |
| Out of IRAM | Reduce log levels, optimize code |
| Linker errors | Check component dependencies |

### Runtime Crashes

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| Stack overflow | Small stack size | Increase task stack |
| Heap corruption | Buffer overflow | Check array bounds |
| LoadProhibited | Null pointer | Add null checks |

### Network Issues

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| WiFi won't connect | Wrong credentials | Check SSID/password |
| TLS handshake fails | Certificate issue | Verify server cert |
| Connection timeout | Firewall | Check port 5223 |

### Cryptographic Issues

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| DH mismatch | Byte order | Reverse wolfSSL output |
| Auth tag fails | Wrong AAD | Check AAD construction |
| Parse error | Wrong encoding | Verify wire format |

---

## References

### ESP-IDF Documentation

- Programming Guide: https://docs.espressif.com/projects/esp-idf/en/latest/
- API Reference: https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/
- GitHub: https://github.com/espressif/esp-idf

### Cryptography Libraries

- wolfSSL: https://www.wolfssl.com/documentation/
- mbedTLS: https://tls.mbed.org/api/
- libsodium: https://doc.libsodium.org/

### SimpleX Protocol

- simplexmq: https://github.com/simplex-chat/simplexmq
- simplex-chat: https://github.com/simplex-chat/simplex-chat

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
