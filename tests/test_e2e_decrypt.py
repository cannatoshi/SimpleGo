#!/usr/bin/env python3
"""
BUG #18: Reply Queue E2E Decrypt - Python Verification (E.5)
============================================================
Test mit ECHTEN Werten aus dem ESP32 Log vom 2026-01-31

Wenn dieser Test FUNKTIONIERT → ESP32 Implementierung ist falsch
Wenn dieser Test FEHLSCHLÄGT → Key oder Struktur ist falsch
"""

from nacl.bindings import (
    crypto_scalarmult,
    crypto_secretbox_open,
    crypto_secretbox_MACBYTES,
    crypto_secretbox_NONCEBYTES,
)
from nacl.secret import SecretBox
from nacl.public import PrivateKey, PublicKey, Box
import hashlib

# ============================================================
# ECHTE WERTE AUS DEM ESP32 LOG (2026-01-31)
# ============================================================

our_e2e_private_hex = "30cdb61c34466225586d40a7a3a6b9010dc2ec691bd2eb5779a456491d956ff1"
peer_public_hex = "8731c502883d3924672bf3f527b43d71545647bfd007622423f908fed9884f3e"
nonce_hex = "de81a0cc1da348bf426b2a4985ea236329d0e24d4038ed00"

# Ciphertext (erste 200 bytes aus Log, vollständig wäre 16022 bytes)
ciphertext_hex = "7c14495d3459f85fbc22840ae4bc7db92f9498886c56a8476faa5dbfa9de60c7eedf682aedf15d7328a11d4bd71947b076357d8d1a377d11884586daa1a21f8e5ad420c42d327e278d2c2ab99b71a9a55afa705cca74588753fd08c122de836e9358400f8934a7c4efbed2736b515f55209ad09bc3e8df2e8e20fb5932b8d5d6bbc9e0c5823eba5069b4bc44d9eedde32a35b40394b5881e941e5d7af9a0f7ffc16c5ce5c295e5f4f9a7e38f78cedf1b7eb5b8fcd38225c5d47d518cbe463318870292a2a633dbd6"

ciphertext_full_len = 16022

# Convert to bytes
our_private = bytes.fromhex(our_e2e_private_hex)
peer_public = bytes.fromhex(peer_public_hex)
nonce = bytes.fromhex(nonce_hex)
ciphertext = bytes.fromhex(ciphertext_hex)

print("=" * 70)
print("BUG #18: Reply Queue E2E Decrypt - Python Verification")
print("=" * 70)
print()

print("📋 Input Data:")
print(f"   our_private:  {our_private.hex()[:16]}...")
print(f"   peer_public:  {peer_public.hex()[:16]}...")
print(f"   nonce:        {nonce.hex()}")
print(f"   ciphertext:   {len(ciphertext)} bytes (full: {ciphertext_full_len})")
print(f"   MAC (first 16): {ciphertext[:16].hex()}")
print()

# ============================================================
# TEST 1: Raw DH + crypto_secretbox (wie Haskell)
# ============================================================
print("=" * 70)
print("TEST 1: Raw DH + crypto_secretbox (Haskell-Stil)")
print("=" * 70)

try:
    # Raw DH (ohne HSalsa20)
    dh_secret = crypto_scalarmult(our_private, peer_public)
    print(f"   DH Secret: {dh_secret.hex()[:16]}...")
    
    # Haskell Format: [MAC 16][Ciphertext]
    # libsodium erwartet: [Ciphertext][MAC 16]
    # Also umordnen!
    mac = ciphertext[:16]
    cipher_only = ciphertext[16:]
    reordered = cipher_only + mac
    
    print(f"   MAC: {mac.hex()}")
    print(f"   Cipher (after MAC): {cipher_only[:16].hex()}...")
    print(f"   Reordered: [Cipher {len(cipher_only)}][MAC 16]")
    
    # Decrypt mit SecretBox (XSalsa20-Poly1305)
    box = SecretBox(dh_secret)
    plaintext = box.decrypt(reordered, nonce)
    
    print()
    print("   ✅ TEST 1 SUCCESS!")
    print(f"   Plaintext ({len(plaintext)} bytes): {plaintext[:50]}...")
    print(f"   Hex: {plaintext[:32].hex()}")
    
except Exception as e:
    print()
    print(f"   ❌ TEST 1 FAILED: {e}")

print()

# ============================================================
# TEST 2: crypto_box (mit HSalsa20)
# ============================================================
print("=" * 70)
print("TEST 2: crypto_box (mit HSalsa20 - Standard NaCl)")
print("=" * 70)

try:
    # crypto_box macht intern:
    # 1. DH(pub, priv) → secret
    # 2. HSalsa20(secret) → key
    # 3. XSalsa20-Poly1305(key, nonce, msg)
    
    priv_key = PrivateKey(our_private)
    pub_key = PublicKey(peer_public)
    box = Box(priv_key, pub_key)
    
    # Auch hier: Haskell=[MAC][Cipher], NaCl=[Cipher][MAC]
    mac = ciphertext[:16]
    cipher_only = ciphertext[16:]
    reordered = cipher_only + mac
    
    plaintext = box.decrypt(reordered, nonce)
    
    print()
    print("   ✅ TEST 2 SUCCESS!")
    print(f"   Plaintext ({len(plaintext)} bytes): {plaintext[:50]}...")
    
except Exception as e:
    print()
    print(f"   ❌ TEST 2 FAILED: {e}")

print()

# ============================================================
# TEST 3: Ohne MAC-Umordnung (falls Haskell doch [Cipher][MAC] ist)
# ============================================================
print("=" * 70)
print("TEST 3: Raw DH + SecretBox OHNE MAC-Umordnung")
print("=" * 70)

try:
    dh_secret = crypto_scalarmult(our_private, peer_public)
    box = SecretBox(dh_secret)
    
    # Direkt ohne Umordnung
    plaintext = box.decrypt(ciphertext, nonce)
    
    print()
    print("   ✅ TEST 3 SUCCESS!")
    print(f"   Plaintext ({len(plaintext)} bytes): {plaintext[:50]}...")
    
except Exception as e:
    print()
    print(f"   ❌ TEST 3 FAILED: {e}")

print()

# ============================================================
# TEST 4: crypto_box OHNE MAC-Umordnung
# ============================================================
print("=" * 70)
print("TEST 4: crypto_box OHNE MAC-Umordnung")
print("=" * 70)

try:
    priv_key = PrivateKey(our_private)
    pub_key = PublicKey(peer_public)
    box = Box(priv_key, pub_key)
    
    plaintext = box.decrypt(ciphertext, nonce)
    
    print()
    print("   ✅ TEST 4 SUCCESS!")
    print(f"   Plaintext ({len(plaintext)} bytes): {plaintext[:50]}...")
    
except Exception as e:
    print()
    print(f"   ❌ TEST 4 FAILED: {e}")

print()

# ============================================================
# FAZIT
# ============================================================
print("=" * 70)
print("FAZIT")
print("=" * 70)
print()
print("Wenn ALLE Tests fehlschlagen:")
print("   → Problem liegt bei Key oder Struktur, NICHT bei Crypto!")
print("   → Mögliche Ursachen:")
print("      1. Der Key bei [28-59] ist NICHT der e2ePubKey")
print("      2. Die Nonce ist an falscher Position")
print("      3. Der Ciphertext beginnt an falscher Position")
print("      4. Unser e2e_private passt nicht zum gesendeten e2e_public")
print()
print("Wenn EIN Test funktioniert:")
print("   → ESP32 Implementierung ist falsch")
print("   → Der funktionierende Test zeigt die richtige Methode")
print()
