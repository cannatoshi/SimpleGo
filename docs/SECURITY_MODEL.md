# Security Model

Why dedicated hardware can offer security advantages over smartphones.

---

## The Smartphone Problem

Modern smartphones are powerful, but their complexity creates inherent security challenges:

| Aspect | Typical Smartphone |
|--------|-------------------|
| Lines of Code | ~50 million (Android/iOS) |
| Running Processes | Hundreds |
| Network Connections | Dozens (background) |
| Trusted Parties | Google/Apple + carriers + app developers |
| Update Control | Vendor-controlled |
| Baseband Processor | Closed-source, always running |

Even hardened mobile operating systems like GrapheneOS cannot fully mitigate these architectural limitations.

---

## The Dedicated Hardware Approach

SimpleGo takes a fundamentally different approach: **minimal trusted computing base (TCB)**.

### Attack Surface Comparison

```
Smartphone (Android/iOS):
├── Operating System (~20M lines)
├── System Services (~10M lines)
├── Browser Engine (~5M lines)
├── JavaScript Runtime
├── App Framework
├── Hundreds of Apps
├── Google/Apple Services
├── Baseband Processor (closed source)
├── Bluetooth Stack
├── NFC Stack
└── Telemetry Services

SimpleGo (ESP32):
├── FreeRTOS Kernel (~10K lines)
├── Network Stack (~20K lines)
├── Crypto Libraries (~15K lines)
└── SimpleGo Application (~5K lines)
    └── Total: ~50K lines (1000x smaller)
```

### What This Means

| Threat Vector | Smartphone | SimpleGo |
|---------------|------------|----------|
| Remote Code Execution | Large attack surface | Minimal attack surface |
| Supply Chain | Many dependencies | Few, auditable dependencies |
| Malicious Apps | Possible | No app installation |
| Browser Exploits | Major risk | No browser |
| JavaScript Attacks | Possible | No JavaScript engine |
| Telemetry/Tracking | Built-in | None |
| Forced Updates | Yes | User-controlled |
| Baseband Attacks | Always possible | No baseband processor |

---

## No Baseband Processor

This deserves special attention.

Every smartphone contains a **baseband processor** - a separate computer running its own operating system that handles cellular communication. This processor:

- Runs closed-source firmware
- Has direct memory access (DMA) on many devices
- Is always active when the phone has signal
- Cannot be audited or disabled
- Has known vulnerabilities (Qualcomm, MediaTek, etc.)

**SimpleGo has no baseband processor.** The ESP32 uses WiFi only, with an open-source network stack. When offline, no radio is active. When online, you know exactly what's transmitting.

---

## ESP32 Hardware Security Features

The ESP32-S3 provides hardware-level security:

### Secure Boot

```
Boot Process:
1. ROM bootloader (unchangeable)
2. Verify second-stage bootloader signature
3. Verify application signature
4. Execute only if all signatures valid
```

- RSA-3072 or ECDSA signature verification
- Public key burned into eFuse (one-time programmable)
- Prevents unsigned/modified firmware from running

### Flash Encryption

```
Storage:
┌─────────────────────────────────────┐
│     Encrypted Flash (AES-256)       │
│  ┌─────────────────────────────┐    │
│  │  Firmware + Keys + Data     │    │
│  └─────────────────────────────┘    │
└─────────────────────────────────────┘
         │
         ▼ Decrypted only in CPU
┌─────────────────────────────────────┐
│           ESP32 CPU                 │
│     (plaintext never leaves)        │
└─────────────────────────────────────┘
```

- AES-256-XTS encryption
- Key stored in eFuse, not readable by software
- Protects against physical flash reading

### eFuse Protection

One-time programmable bits that can:

- Disable JTAG debugging permanently
- Disable UART bootloader
- Lock flash encryption key
- Lock secure boot key
- Enable read/write protection on sensitive fuses

### Hardware Crypto Acceleration

- AES: Hardware accelerated
- SHA: Hardware accelerated
- RSA: Hardware accelerated
- Random Number Generator: Hardware TRNG

---

## Comparison with GrapheneOS

GrapheneOS is excellent - but it still runs on smartphone hardware.

| Feature | GrapheneOS | SimpleGo |
|---------|------------|----------|
| Verified Boot | ✅ | ✅ |
| Full Storage Encryption | ✅ | ✅ |
| Hardened Memory Allocator | ✅ | Possible |
| ASLR | ✅ | Limited (RAM constraints) |
| Sandboxing | ✅ | ✅ (only one app exists) |
| No Google Services | ✅ | ✅ |
| No Baseband | ❌ | ✅ |
| Minimal TCB | ~50M lines | ~50K lines |
| Physical Deniability | ❌ (it's a phone) | ✅ (looks like dev board) |
| Disposable | ❌ (~$500+) | ✅ (~$15-50) |
| Air-Gap Capable | Difficult | Easy |

---

## Operational Security Advantages

### Physical Characteristics

**Smartphones:**
- Recognizable as communication devices
- Expensive, not easily disposed
- Biometric data (face/fingerprint) stored
- Location history embedded

**SimpleGo Device:**
- Looks like generic development board
- Cheap enough to be disposable
- No biometrics
- No location history
- Can be hidden easily

### Network Behavior

**Smartphones:**
- Constantly connected
- Background sync to multiple services
- Push notifications require persistent connection
- Carrier can track location via cell towers

**SimpleGo:**
- Online only when you choose
- Single connection (to SMP server via Tor possible)
- No background activity
- No carrier relationship

### Plausible Deniability

A smartphone is obviously a communication device. An ESP32 board could be:
- A weather station
- A home automation controller
- A learning project
- An IoT sensor
- A development prototype

---

## Threat Model

### What SimpleGo Protects Against

| Threat | Protection |
|--------|------------|
| Mass surveillance | E2E encryption, no metadata to provider |
| Network monitoring | TLS 1.3, encrypted payload |
| Server compromise | Server never sees plaintext |
| Device theft (locked) | Flash encryption, secure boot |
| Physical flash extraction | AES-256 encrypted |
| Supply chain attacks | Auditable codebase, reproducible builds |
| Carrier tracking | No cellular, WiFi only |
| App-based attacks | No app installation possible |
| Browser exploits | No browser |
| Telemetry | None exists |

### What SimpleGo Does NOT Protect Against

| Threat | Limitation |
|--------|------------|
| Physical access (unlocked) | Device in use is vulnerable |
| Nation-state with physical access | eFuse can potentially be read with equipment |
| WiFi network monitoring | Connection timing visible (use Tor) |
| Implementation bugs | Code must be audited |
| Side-channel attacks | Not hardened against power analysis etc. |
| Social engineering | User must verify contacts |

### Recommended Practices

1. **Enable Secure Boot and Flash Encryption** before deployment
2. **Disable JTAG** via eFuse for production devices
3. **Use Tor** if network-level privacy required
4. **Verify contacts** through secondary channel
5. **Store device securely** when not in use
6. **Consider device disposable** - replace if compromised

---

## Code Quality and Stability

### Why C?

SimpleGo is written in C, which requires careful memory management but offers:

| Aspect | C | Managed Languages |
|--------|---|-------------------|
| Garbage Collector | None (deterministic) | Unpredictable pauses |
| Runtime Size | Minimal | Large |
| Memory Control | Complete | Abstracted |
| Binary Size | Small | Large |
| Timing Attacks | Easier to prevent | GC complicates |
| Audit Surface | Code only | Code + runtime + JIT |

### Memory Safety Mitigations

```c
// Compiler hardening flags
-fstack-protector-strong    // Stack canaries
-D_FORTIFY_SOURCE=2         // Buffer overflow detection
-Wformat-security           // Format string checks

// Coding practices
- Bounds checking on all buffer operations
- No sprintf, only snprintf
- Input validation on all external data
- Static analysis with cppcheck
- Valgrind testing where possible
```

### Deterministic Behavior

No garbage collector means:
- Predictable timing (important for crypto)
- No memory allocation during critical operations
- Consistent performance under load
- Easier to audit for timing leaks

---

## Future Security Enhancements

| Enhancement | Status | Description |
|-------------|--------|-------------|
| Secure Boot Integration | Planned | Document and test secure boot setup |
| Flash Encryption Guide | Planned | Step-by-step encryption setup |
| eFuse Configuration | Planned | Production lockdown guide |
| Memory Sanitizers | Planned | ASAN/MSAN testing |
| Fuzzing | Planned | Protocol parser fuzzing |
| Tor Integration | Research | Onion routing for traffic analysis resistance |
| External Secure Element | Research | Hardware key storage (ATECC608) |

---

## Summary

SimpleGo's security model is based on **simplicity and control**:

1. **Minimal code** = fewer bugs = smaller attack surface
2. **No baseband** = no cellular black box
3. **No apps** = no malware vector
4. **No browser** = no web exploits
5. **No telemetry** = no data leakage
6. **Hardware security** = protection at rest
7. **User control** = you decide when it's online
8. **Disposable** = compromise is recoverable

This approach cannot match all security properties of a hardened smartphone (particularly ASLR and advanced sandboxing), but it eliminates entire categories of attacks that smartphone users must accept.

**The most secure system is often the simplest one.**

---

*Document Version: 1.0*
*Last Updated: January 2026*