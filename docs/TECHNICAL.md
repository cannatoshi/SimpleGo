# SimpleGo Technical Documentation

Low-level technical details for SimpleGo development and debugging.

---

## Build Environment

### Required Tools

| Tool | Version | Purpose |
|------|---------|---------|
| ESP-IDF | 5.5.2+ | Development framework |
| Python | 3.8+ | Build scripts |
| CMake | 3.16+ | Build system |

### Build Commands

| Command | Description |
|---------|-------------|
| idf.py build | Compile project |
| idf.py flash -p COM5 | Flash to device |
| idf.py monitor -p COM5 | Serial monitor |
| idf.py build flash monitor -p COM5 | All combined |
| idf.py clean | Clean build |
| idf.py fullclean | Full clean |
| idf.py menuconfig | Configuration menu |
| idf.py size | Binary size analysis |

---

## Memory Configuration

### Partition Table

| Name | Type | Size |
|------|------|------|
| nvs | data | 24 KB |
| phy_init | data | 4 KB |
| factory | app | 1.5 MB |
| storage | data | 1 MB |

### Memory Usage

| Region | Size |
|--------|------|
| IRAM | 128 KB |
| DRAM | 320 KB |
| PSRAM | 2-8 MB (optional) |
| Flash | 4-16 MB |

---

## Component Configuration

### wolfSSL Settings

Key settings for X448 support:
- HAVE_CURVE448: Enabled
- CURVE448_SMALL: Enabled
- WOLFSSL_SHA512: Enabled

### mbedTLS Settings

- CONFIG_MBEDTLS_SSL_PROTO_TLS1_3=y
- CONFIG_MBEDTLS_HKDF_C=y
- CONFIG_MBEDTLS_GCM_C=y
- CONFIG_MBEDTLS_SHA512_C=y

---

## Debugging

### Log Levels

| Level | Macro | Use Case |
|-------|-------|----------|
| Error | ESP_LOGE | Critical failures |
| Warning | ESP_LOGW | Unexpected but handled |
| Info | ESP_LOGI | Normal operation |
| Debug | ESP_LOGD | Development info |
| Verbose | ESP_LOGV | Detailed tracing |

### Set Log Level Per Module

In code: esp_log_level_set("SMP_NETWORK", ESP_LOG_DEBUG);

---

## Performance

### Operation Timing (ESP32-S3 @ 240MHz)

| Operation | Time |
|-----------|------|
| WiFi connect | ~3 sec |
| TLS handshake | ~500 ms |
| X448 keygen | ~15 ms |
| X448 DH | ~15 ms |
| HKDF (96 bytes) | ~0.5 ms |
| AES-GCM (1KB) | ~0.3 ms |

### Memory Usage

| Component | RAM |
|-----------|-----|
| WiFi stack | ~40 KB |
| TLS context | ~40 KB |
| Application | ~20 KB |
| Free heap | ~180 KB |

---

## Error Codes

### ESP-IDF Errors

| Code | Name |
|------|------|
| 0 | ESP_OK |
| 0x101 | ESP_ERR_NO_MEM |
| 0x102 | ESP_ERR_INVALID_ARG |
| 0x103 | ESP_ERR_INVALID_STATE |

### mbedTLS Errors

| Code | Description |
|------|-------------|
| -0x7780 | SSL handshake failure |
| -0x7200 | Certificate verification failed |
| -0x0010 | GCM auth failed |

---

## Troubleshooting

### Build Failures

| Error | Solution |
|-------|----------|
| Component not found | Run idf.py reconfigure |
| Out of IRAM | Reduce log levels |
| Linker errors | Check dependencies |

### Runtime Crashes

| Symptom | Solution |
|---------|----------|
| Stack overflow | Increase task stack |
| Heap corruption | Check array bounds |
| LoadProhibited | Add null checks |

### Network Issues

| Symptom | Solution |
|---------|----------|
| WiFi wont connect | Check credentials |
| TLS fails | Verify certificate |
| Connection timeout | Check port 5223 |

### Cryptographic Issues

| Symptom | Solution |
|---------|----------|
| DH mismatch | Reverse wolfSSL output |
| Auth tag fails | Check AAD construction |
| Parse error | Verify wire format |

---

## References

### ESP-IDF

- Docs: https://docs.espressif.com/projects/esp-idf/en/latest/
- GitHub: https://github.com/espressif/esp-idf

### Cryptography

- wolfSSL: https://www.wolfssl.com/documentation/
- mbedTLS: https://tls.mbed.org/api/
- libsodium: https://doc.libsodium.org/

### SimpleX

- simplexmq: https://github.com/simplex-chat/simplexmq
- simplex-chat: https://github.com/simplex-chat/simplex-chat

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
