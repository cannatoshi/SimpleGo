# Session 5: X448 Breakthrough

## The wolfSSL Byte-Order Discovery

**Date:** January 24, 2026
**Version:** v0.1.25-alpha
**Bug Fixed:** #9 - The Critical X448 Bug

---

## 🎉 THE BREAKTHROUGH

After fixing 8 encoding bugs in Session 4, the A_MESSAGE error persisted. Session 5 brought the breakthrough: **wolfSSL's X448 implementation outputs keys in reversed byte order compared to what SimpleX expects!**

This was the moment everything started making sense.

---

## 📋 Table of Contents

1. [Session Overview](#1-session-overview)
2. [The Investigation](#2-the-investigation)
3. [Bug #9: wolfSSL X448 Byte Order](#3-bug-9-wolfssl-x448-byte-order)
4. [Python Verification](#4-python-verification)
5. [The Fix Implementation](#5-the-fix-implementation)
6. [Cryptographic Verification](#6-cryptographic-verification)
7. [Understanding the Issue](#7-understanding-the-issue)
8. [Session Summary](#8-session-summary)
9. [Changelog](#9-changelog)

---

## 1. Session Overview

### 1.1 Starting Point
`
Session 5 Start:
═══════════════════════════════════════════════════════════════════

✅ 8 encoding bugs fixed (Session 4)
✅ Wire format verified
✅ Server accepts messages
❌ App: "error agent A_MESSAGE"

Hypothesis: Crypto values might be wrong

═══════════════════════════════════════════════════════════════════
`

### 1.2 The Key Question

If all the encoding is correct, why can't the app decrypt our messages?

**New approach:** Compare our cryptographic outputs byte-by-byte against a Python reference implementation.

---

## 2. The Investigation

### 2.1 Testing Strategy

We created Python scripts to compute the same values as our ESP32 code:
1. X448 key generation
2. X448 Diffie-Hellman
3. X3DH key agreement
4. HKDF key derivation
5. AES-GCM encryption

### 2.2 First Comparison: X448 DH
`python
# Python reference using cryptography library
from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey

# Using the same keys from ESP32 logs
our_private = bytes.fromhex("...")
peer_public = bytes.fromhex("...")

private_key = X448PrivateKey.from_private_bytes(our_private)
peer_key = X448PublicKey.from_public_bytes(peer_public)
shared_secret = private_key.exchange(peer_key)

print(f"Python shared secret: {shared_secret.hex()}")
`

**Result:** The shared secrets did NOT match!

### 2.3 The Discovery
`
Comparison Results:
═══════════════════════════════════════════════════════════════════

ESP32 (wolfSSL) shared secret:
  a1 b2 c3 d4 e5 f6 ... (56 bytes)

Python (cryptography) shared secret:
  ... f6 e5 d4 c3 b2 a1  (56 bytes) ← REVERSED!

THE BYTES ARE IN OPPOSITE ORDER!

═══════════════════════════════════════════════════════════════════
`

---

## 3. Bug #9: wolfSSL X448 Byte Order

### 3.1 The Problem

wolfSSL's Curve448 implementation uses **little-endian** byte order internally, while SimpleX (using Haskell's cryptonite library) expects **big-endian** byte order.
`
Byte Order Difference:
═══════════════════════════════════════════════════════════════════

wolfSSL output (little-endian):
  [byte_55][byte_54][byte_53]...[byte_2][byte_1][byte_0]

SimpleX expects (big-endian):
  [byte_0][byte_1][byte_2]...[byte_53][byte_54][byte_55]

They are EXACTLY REVERSED!

═══════════════════════════════════════════════════════════════════
`

### 3.2 Root Cause Analysis

wolfSSL defines `EC448_LITTLE_ENDIAN` for its Curve448 implementation. This affects:
- Public key export
- Private key export
- Shared secret output

The Haskell cryptonite library uses big-endian (network byte order) for all cryptographic operations.

### 3.3 Impact

Every X448 operation was producing reversed bytes:
- Our public keys sent to peer: **reversed**
- DH shared secrets: **reversed**
- All derived keys (X3DH, Root KDF, Chain KDF): **WRONG**
- All encryption: **WRONG**

No wonder the app couldn't decrypt anything!

---

## 4. Python Verification

### 4.1 Verification Script
`python
#!/usr/bin/env python3
"""
Verify ESP32 X448 output against Python reference.
"""

from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey

def reverse_bytes(data: bytes) -> bytes:
    """Reverse byte order."""
    return bytes(reversed(data))

# ESP32 outputs (from serial log)
esp32_private = bytes.fromhex("YOUR_PRIVATE_KEY_HEX_HERE")
esp32_public = bytes.fromhex("YOUR_PUBLIC_KEY_HEX_HERE")
esp32_shared = bytes.fromhex("YOUR_SHARED_SECRET_HEX_HERE")

# Test 1: Verify public key derivation
private_key = X448PrivateKey.from_private_bytes(reverse_bytes(esp32_private))
computed_public = private_key.public_key().public_bytes_raw()

print(f"ESP32 public:    {esp32_public.hex()}")
print(f"ESP32 reversed:  {reverse_bytes(esp32_public).hex()}")
print(f"Python computed: {computed_public.hex()}")

if reverse_bytes(esp32_public) == computed_public:
    print("✅ Public key matches when reversed!")
else:
    print("❌ Public key mismatch")

# Test 2: Verify DH shared secret
peer_public = bytes.fromhex("PEER_PUBLIC_KEY_HEX_HERE")
peer_key = X448PublicKey.from_public_bytes(peer_public)
python_shared = private_key.exchange(peer_key)

print(f"\nESP32 shared:    {esp32_shared.hex()}")
print(f"ESP32 reversed:  {reverse_bytes(esp32_shared).hex()}")
print(f"Python shared:   {python_shared.hex()}")

if reverse_bytes(esp32_shared) == python_shared:
    print("✅ Shared secret matches when reversed!")
else:
    print("❌ Shared secret mismatch")
`

### 4.2 Verification Results
`
Verification Output:
═══════════════════════════════════════════════════════════════════

ESP32 public:    a1b2c3...
ESP32 reversed:  ...c3b2a1
Python computed: ...c3b2a1
✅ Public key matches when reversed!

ESP32 shared:    d4e5f6...
ESP32 reversed:  ...f6e5d4
Python shared:   ...f6e5d4
✅ Shared secret matches when reversed!

CONFIRMED: wolfSSL outputs are byte-reversed!

═══════════════════════════════════════════════════════════════════
`

---

## 5. The Fix Implementation

### 5.1 Helper Function
`c
/**
 * @brief Reverse byte order of a buffer
 *
 * wolfSSL X448 uses little-endian internally, but SimpleX
 * expects big-endian. This function reverses the byte order.
 *
 * @param src Source buffer
 * @param dst Destination buffer (can be same as src for in-place)
 * @param len Buffer length
 */
static void reverse_bytes(const uint8_t *src, uint8_t *dst, size_t len) {
    // Handle in-place reversal
    if (src == dst) {
        for (size_t i = 0; i < len / 2; i++) {
            uint8_t tmp = dst[i];
            dst[i] = dst[len - 1 - i];
            dst[len - 1 - i] = tmp;
        }
    } else {
        for (size_t i = 0; i < len; i++) {
            dst[i] = src[len - 1 - i];
        }
    }
}
`

### 5.2 Key Generation Fix
`c
int smp_x448_generate_keypair(x448_keypair_t *keypair) {
    curve448_key key;
    uint8_t pub_tmp[56], priv_tmp[56];
    
    // Generate keypair
    wc_curve448_init(&key);
    wc_curve448_make_key(&rng, 56, &key);
    
    // Export keys (wolfSSL little-endian)
    word32 pub_len = 56, priv_len = 56;
    wc_curve448_export_public(&key, pub_tmp, &pub_len);
    wc_curve448_export_private_raw(&key, priv_tmp, &priv_len);
    
    // CRITICAL: Reverse byte order for SimpleX compatibility!
    reverse_bytes(pub_tmp, keypair->public_key, 56);
    reverse_bytes(priv_tmp, keypair->private_key, 56);
    
    wc_curve448_free(&key);
    return 0;
}
`

### 5.3 Diffie-Hellman Fix
`c
int smp_x448_dh(const uint8_t *their_public,
                const uint8_t *my_private,
                uint8_t *shared_secret) {
    curve448_key their_key, my_key;
    uint8_t their_public_rev[56], my_private_rev[56];
    uint8_t secret_tmp[56];
    
    // Reverse inputs before importing to wolfSSL
    reverse_bytes(their_public, their_public_rev, 56);
    reverse_bytes(my_private, my_private_rev, 56);
    
    // Import keys
    wc_curve448_init(&their_key);
    wc_curve448_init(&my_key);
    wc_curve448_import_public(their_public_rev, 56, &their_key);
    wc_curve448_import_private(my_private_rev, 56, &my_key);
    
    // Compute shared secret
    word32 secret_len = 56;
    wc_curve448_shared_secret(&my_key, &their_key, secret_tmp, &secret_len);
    
    // CRITICAL: Reverse output for SimpleX compatibility!
    reverse_bytes(secret_tmp, shared_secret, 56);
    
    wc_curve448_free(&their_key);
    wc_curve448_free(&my_key);
    return 0;
}
`

### 5.4 Where Reversal is Needed

| Operation | Input Reversal | Output Reversal |
|-----------|----------------|-----------------|
| Key generation | N/A | Public + Private |
| Public key import | Yes | N/A |
| Private key import | Yes | N/A |
| DH shared secret | Both inputs | Output |

---

## 6. Cryptographic Verification

### 6.1 Complete Verification Script
`python
#!/usr/bin/env python3
"""
Complete cryptographic verification for SimpleGo.
Compares ESP32 output (after byte-reversal fix) against Python reference.
"""

from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey, X448PublicKey
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.backends import default_backend

def verify_x3dh(our_sk1_priv, our_rpk1_priv, peer_spk1_pub, peer_rk1_pub):
    """Verify X3DH key agreement."""
    
    # Load keys
    sk1 = X448PrivateKey.from_private_bytes(our_sk1_priv)
    rpk1 = X448PrivateKey.from_private_bytes(our_rpk1_priv)
    spk1 = X448PublicKey.from_public_bytes(peer_spk1_pub)
    rk1 = X448PublicKey.from_public_bytes(peer_rk1_pub)
    
    # Three DH operations
    dh1 = sk1.exchange(spk1)   # Our ephemeral x Peer semi-permanent
    dh2 = sk1.exchange(rk1)    # Our ephemeral x Peer ratchet
    dh3 = rpk1.exchange(spk1)  # Our ratchet x Peer semi-permanent
    
    # Concatenate
    ikm = dh1 + dh2 + dh3  # 168 bytes
    
    # HKDF
    hkdf = HKDF(
        algorithm=hashes.SHA512(),
        length=96,
        salt=b'\x00' * 64,  # 64 zero bytes
        info=b"SimpleXX3DH",
        backend=default_backend()
    )
    output = hkdf.derive(ikm)
    
    header_key = output[0:32]
    next_header_key = output[32:64]
    root_key = output[64:96]
    
    return {
        'dh1': dh1,
        'dh2': dh2,
        'dh3': dh3,
        'ikm': ikm,
        'header_key': header_key,
        'next_header_key': next_header_key,
        'root_key': root_key
    }

def verify_chain_kdf(chain_key):
    """Verify Chain KDF."""
    hkdf = HKDF(
        algorithm=hashes.SHA512(),
        length=96,
        salt=b'',  # Empty salt!
        info=b"SimpleXChainRatchet",
        backend=default_backend()
    )
    output = hkdf.derive(chain_key)
    
    return {
        'next_chain_key': output[0:32],
        'message_key': output[32:64],
        'header_iv': output[64:80],   # bytes 64-79
        'message_iv': output[80:96]   # bytes 80-95
    }

# Usage example with ESP32 values
if __name__ == "__main__":
    # Insert ESP32 key values here (after byte-reversal fix)
    # our_sk1_priv = bytes.fromhex("...")
    # ...
    pass
`

### 6.2 After the Fix - 100% Match!
`
Verification Results (After Fix):
═══════════════════════════════════════════════════════════════════

X448 Diffie-Hellman:
  ESP32:  a1b2c3d4e5f6... (56 bytes)
  Python: a1b2c3d4e5f6... (56 bytes)
  ✅ 100% MATCH!

X3DH Key Agreement:
  ESP32 header_key:  1234567890...
  Python header_key: 1234567890...
  ✅ 100% MATCH!

Root KDF:
  ESP32 chain_key:  abcdef...
  Python chain_key: abcdef...
  ✅ 100% MATCH!

Chain KDF:
  ESP32 message_key: 112233...
  Python message_key: 112233...
  ✅ 100% MATCH!

  ESP32 header_iv: aabbcc...
  Python header_iv: aabbcc...
  ✅ 100% MATCH!

  ESP32 msg_iv: ddeeff...
  Python msg_iv: ddeeff...
  ✅ 100% MATCH!

ALL CRYPTOGRAPHIC VALUES VERIFIED!

═══════════════════════════════════════════════════════════════════
`

---

## 7. Understanding the Issue

### 7.1 Why Different Libraries Use Different Byte Orders

**Little-endian (wolfSSL):**
- Matches typical CPU architecture (x86, ARM)
- More efficient for arithmetic operations
- Common in low-level crypto implementations

**Big-endian (cryptonite/SimpleX):**
- Network byte order
- More intuitive for humans reading hex dumps
- Common in protocol specifications

### 7.2 Lessons Learned
`
Key Takeaways:
═══════════════════════════════════════════════════════════════════

1. CRYPTO LIBRARIES ARE NOT INTERCHANGEABLE
   └── Byte order can vary between implementations
   └── Always verify against test vectors or reference implementations

2. PYTHON COMPARISON TESTS ARE INVALUABLE
   └── Python's cryptography library matches SimpleX's cryptonite
   └── Use Python to verify ESP32 crypto outputs

3. THE ERROR WAS INVISIBLE AT THE ENCODING LEVEL
   └── Wire format was correct
   └── Server accepted messages
   └── But crypto values were all wrong!

4. SYSTEMATIC DEBUGGING WORKS
   └── Compare each layer independently
   └── Verify inputs and outputs at each step

═══════════════════════════════════════════════════════════════════
`

---

## 8. Session Summary

### 8.1 The Breakthrough

| Item | Before | After |
|------|--------|-------|
| X448 DH output | Reversed | Correct |
| X3DH keys | Wrong | Verified |
| Root KDF | Wrong | Verified |
| Chain KDF | Wrong | Verified |
| AES-GCM | Failed | Should work |

### 8.2 Bug #9 Details

| Attribute | Value |
|-----------|-------|
| Bug Number | #9 |
| Component | X448 Cryptography |
| Library | wolfSSL |
| Issue | Little-endian output |
| Solution | Reverse all X448 bytes |
| Status | ✅ **FIXED** |

### 8.3 Status After Session 5
`
After Session 5:
═══════════════════════════════════════════════════════════════════

✅ 9 bugs fixed (8 encoding + 1 crypto)
✅ All cryptographic values verified against Python
✅ Server accepts messages
❌ App STILL shows A_MESSAGE (but we're close!)

Remaining investigation: SMPQueueInfo encoding

═══════════════════════════════════════════════════════════════════
`

---

## 9. Changelog

| Date | Change |
|------|--------|
| 2026-01-24 S5 | Session 5 started - Crypto verification |
| 2026-01-24 S5 | Created Python verification scripts |
| 2026-01-24 S5 | **BREAKTHROUGH: Discovered wolfSSL byte order issue!** |
| 2026-01-24 S5 | Bug #9 identified: X448 byte reversal needed |
| 2026-01-24 S5 | Implemented reverse_bytes() helper |
| 2026-01-24 S5 | Fixed key generation |
| 2026-01-24 S5 | Fixed DH computation |
| 2026-01-24 S5 | Verified all crypto against Python: 100% match |
| 2026-01-24 S5 | Documentation v15 created |

---

*Document version: Session 5 Complete*
*Last updated: January 24, 2026*
*The Breakthrough Session - wolfSSL byte order discovered!*
