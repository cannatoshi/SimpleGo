# SimpleGo Development Progress

> Detailed log of development milestones and sessions

---

## 📊 Overall Status

| Phase | Description | Status | Completion |
|-------|-------------|--------|------------|
| 1 | Environment Setup | ✅ Complete | 100% |
| 2 | Network Foundation | ✅ Complete | 100% |
| 3 | Crypto Foundation | ✅ Complete | 100% |
| 4 | SMP Protocol | 🔄 In Progress | 20% |
| 5 | Double Ratchet | 📋 Planned | 0% |
| 6 | Full UI (LVGL) | 📋 Planned | 0% |
| 7 | T-Deck Port | 📋 Planned | 0% |
| 8 | Tor Support | 📋 Planned | 0% |

---

## 🗓️ Session Log

### Session 1 - January 18/19, 2026

**Duration:** ~6 hours  
**Focus:** Complete foundation setup

#### Achievements

**Environment Setup:**
- [x] Arduino IDE 2.3.7 installed
- [x] Heltec ESP32 board support (manual Git install - Board Manager timeout issues)
- [x] CP2102 USB driver configured
- [x] Successful first upload to Heltec WiFi LoRa 32 V2

**Network Layer:**
- [x] WiFi connection working
- [x] TLS 1.3 connection to HTTPS servers
- [x] SMP server reachable (smp11.simplex.im)
- [x] Discovered: Port 5223 blocked on test network, Port 443 works
- [x] Direct IP connection (DNS issues bypassed)
- [x] 16KB block transmission successful (134ms)

**Crypto Implementation:**
- [x] Tested mbedTLS (built-in): SHA-256, SHA-512, AES-256-GCM ✅
- [x] Attempted libsodium (built-in): Linker errors with AEGIS
- [x] Attempted wolfSSL: Complex Arduino setup, abandoned
- [x] Installed Monocypher: Perfect fit! Simple, fast, working

**Crypto Benchmarks (ESP32 @ 240MHz):**
| Operation | Time |
|-----------|------|
| X25519 KeyGen | 8ms |
| X25519 DH | 8ms |
| Ed25519 KeyGen | 8ms |
| Ed25519 Sign | 8ms |
| Ed25519 Verify | 21ms |
| SHA-256 | <1ms |
| AES-256-GCM | 1ms |

**Display/UI:**
- [x] OLED display initialized (SSD1306)
- [x] SimpleX-style header with signal bars
- [x] Status screens (connecting, connected, error)
- [x] Basic message bubble layout

**SMP Protocol (Started):**
- [x] TLS connection to SMP server
- [x] 16KB block framing implemented
- [x] Basic NEW command structure
- [x] Ed25519 keypair generation for identity
- [x] X25519 keypair generation for queue DH
- [ ] Correct handshake sequence (needs research)
- [ ] Response parsing

#### Technical Discoveries

1. **Arduino Board Manager Issues**
   - Large packages (300MB+) timeout frequently
   - Solution: Manual Git clone + get.exe toolchain installer

2. **DNS Resolution**
   - ESP32 DNS unreliable for some domains
   - Router DNS returned 0.0.0.0 for smp.simplex.im
   - Solution: Use direct IP addresses

3. **Port Blocking**
   - Port 5223 blocked on some networks
   - Port 443 works (standard HTTPS)
   - SimpleX servers support both ports

4. **Crypto Library Selection**
   - libsodium (ESP32 built-in): Broken linker issues
   - wolfSSL: Too complex for Arduino, needs shell scripts
   - Monocypher: Perfect! 2 files, works immediately

5. **SMP Protocol**
   - Server keeps connection open even with invalid packets
   - No response without proper handshake
   - Need to study protocol spec more

#### Blockers & Solutions

| Blocker | Solution |
|---------|----------|
| Board Manager timeout | Manual Git installation |
| DNS resolution fails | Direct IP address |
| Port 5223 blocked | Use port 443 |
| libsodium linker errors | Use Monocypher instead |
| wolfSSL complex setup | Use Monocypher instead |
| Display variable conflict | Rename `display` to `oled` |
| HT_SSD1306Wire not found | Install Heltec_ESP32 library |

#### Files Created

- `src/arduino/simplex_dev_board/` - Main development sketches
- Multiple test sketches for WiFi, TLS, Crypto, SMP

#### Next Session TODO

1. [ ] Study SMP protocol specification in detail
2. [ ] Implement proper session handshake
3. [ ] Parse server responses
4. [ ] Get NEW command working
5. [ ] Receive queue ID from server

---

## 🔬 Technical Notes

### Working SMP Connection
```cpp
IPAddress smpServer(172, 236, 211, 32);  // smp11.simplex.im
const int smpPort = 443;

WiFiClientSecure client;
client.setInsecure();  // Skip cert validation for dev
client.connect(smpServer, smpPort, 15000);  // 15s timeout
```

### Monocypher Usage
```cpp
#include "monocypher.h"

// X25519 keypair
uint8_t secret[32], public_key[32];
esp_fill_random(secret, 32);  // Hardware RNG
crypto_x25519_public_key(public_key, secret);

// Ed25519 keypair
uint8_t ed_secret[64], ed_public[32], seed[32];
esp_fill_random(seed, 32);
crypto_eddsa_key_pair(ed_secret, ed_public, seed);

// Ed25519 sign
uint8_t signature[64];
crypto_eddsa_sign(signature, ed_secret, message, msg_len);

// Ed25519 verify
int valid = crypto_eddsa_check(signature, ed_public, message, msg_len);
// valid == 0 means success
```

### Display (Heltec OLED)
```cpp
#include "HT_SSD1306Wire.h"

SSD1306Wire oled(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

oled.init();
oled.flipScreenVertically();
oled.setFont(ArialMT_Plain_10);
oled.drawString(0, 0, "SimpleX");
oled.display();
```

---

## 📚 Resources Consulted

- [SimpleX Protocol Spec](https://github.com/simplex-chat/simplexmq/tree/stable/protocol)
- [Monocypher Manual](https://monocypher.org/manual/)
- [Heltec ESP32 Docs](https://docs.heltec.org/)
- [ESP32 mbedTLS](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/mbedtls.html)

---

## 🎯 Long-term Goals

1. **Functional SimpleX Client** - Send/receive messages
2. **T-Deck Native App** - Full UI with keyboard
3. **Offline Mesh** - LoRa fallback communication
4. **Tor Integration** - Anonymous connections
5. **Multi-device** - Sync across devices

---

*This document is updated after each development session.*
