# SimpleGo Cryptography Documentation

Complete cryptographic specification for the SimpleGo Double Ratchet implementation.

---

## Overview

SimpleGo implements end-to-end encryption using the Double Ratchet algorithm with X3DH key agreement, following the SimpleX protocol specification.

### Cryptographic Components

| Component | Algorithm | Library | Purpose |
|-----------|-----------|---------|---------|
| Key Agreement | X3DH | Custom | Initial shared secret |
| DH Ratchet | X448 (Curve448) | wolfSSL | Forward secrecy |
| Key Derivation | HKDF-SHA512 | mbedTLS | Key expansion |
| Symmetric Encryption | AES-256-GCM | mbedTLS | Message encryption |
| Per-Queue Encryption | X25519 | libsodium | Queue-level encryption |
| Signatures | Ed25519 | libsodium | Authentication |

---

## X3DH Key Agreement

Extended Triple Diffie-Hellman establishes the initial shared secret between two parties.

### Keys Involved

| Key | Name | Owner | Type | Size | Purpose |
|-----|------|-------|------|------|---------|
| spk1 | Semi-permanent Key | Peer (App) | X448 Public | 56 bytes | Long-term identity |
| rk1 | Ratchet Key | Peer (App) | X448 Public | 56 bytes | Initial ratchet |
| sk1 | Ephemeral Secret | Us (ESP32) | X448 Private | 56 bytes | One-time secret |
| pk1 | Ephemeral Public | Us (ESP32) | X448 Public | 56 bytes | Sent to peer |
| rpk1 | Our Ratchet Key | Us (ESP32) | X448 Key Pair | 56+56 bytes | Our ratchet |

### DH Calculations

The sender (ESP32) performs three Diffie-Hellman operations:
`
dh1 = X448_DH(sk1, spk1)      // Our ephemeral × Peer's semi-permanent
dh2 = X448_DH(sk1, rk1)       // Our ephemeral × Peer's ratchet
dh3 = X448_DH(rpk1_priv, spk1) // Our ratchet × Peer's semi-permanent

ikm = dh1 || dh2 || dh3       // Concatenate: 168 bytes total
`

### HKDF Derivation
`
Input:
  salt = 0x00 repeated 64 times (64 zero bytes)
  ikm  = dh1 || dh2 || dh3 (168 bytes)
  info = "SimpleXX3DH" (11 ASCII bytes)
  
Output: 96 bytes
  [0:32]   = header_key (hk)
  [32:64]  = next_header_key (nhk)
  [64:96]  = root_key (rk)
`

### Code Example
`c
int smp_x3dh_derive_keys(
    const uint8_t *dh1,      // 56 bytes
    const uint8_t *dh2,      // 56 bytes
    const uint8_t *dh3,      // 56 bytes
    uint8_t *header_key,     // 32 bytes out
    uint8_t *next_header_key,// 32 bytes out
    uint8_t *root_key        // 32 bytes out
) {
    uint8_t salt[64] = {0};
    uint8_t ikm[168];
    uint8_t output[96];
    
    // Concatenate DH outputs
    memcpy(ikm, dh1, 56);
    memcpy(ikm + 56, dh2, 56);
    memcpy(ikm + 112, dh3, 56);
    
    // HKDF-SHA512
    mbedtls_hkdf(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA512),
        salt, 64,
        ikm, 168,
        (uint8_t*)"SimpleXX3DH", 11,
        output, 96
    );
    
    // Split output
    memcpy(header_key, output, 32);
    memcpy(next_header_key, output + 32, 32);
    memcpy(root_key, output + 64, 32);
    
    return 0;
}
`

---

## Double Ratchet

The Double Ratchet algorithm provides forward secrecy and break-in recovery.

### Ratchet State
`c
typedef struct {
    // Key material
    uint8_t root_key[32];
    uint8_t chain_key[32];
    uint8_t header_key[32];
    uint8_t next_header_key[32];
    
    // DH ratchet
    x448_keypair_t dh_keypair;     // Our current DH key pair
    uint8_t peer_dh_public[56];    // Peer's current DH public key
    
    // Message counters
    uint32_t msg_number_send;      // Messages sent in current chain
    uint32_t msg_number_recv;      // Messages received in current chain
    uint32_t prev_chain_length;    // Length of previous sending chain
} ratchet_state_t;
`

### Root KDF

Derives new keys when the DH ratchet advances.
`
Input:
  salt = root_key (32 bytes)
  ikm  = dh_output (56 bytes from X448 DH)
  info = "SimpleXRootRatchet" (19 ASCII bytes)

Output: 96 bytes
  [0:32]   = new_root_key
  [32:64]  = chain_key
  [64:96]  = next_header_key
`

### Root KDF Code
`c
int smp_ratchet_root_kdf(
    const uint8_t *root_key,      // 32 bytes
    const uint8_t *dh_output,     // 56 bytes
    uint8_t *new_root_key,        // 32 bytes out
    uint8_t *chain_key,           // 32 bytes out
    uint8_t *next_header_key      // 32 bytes out
) {
    uint8_t output[96];
    
    mbedtls_hkdf(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA512),
        root_key, 32,             // salt
        dh_output, 56,            // ikm
        (uint8_t*)"SimpleXRootRatchet", 19,  // info
        output, 96
    );
    
    memcpy(new_root_key, output, 32);
    memcpy(chain_key, output + 32, 32);
    memcpy(next_header_key, output + 64, 32);
    
    return 0;
}
`

### Chain KDF

Derives per-message keys and IVs.
`
Input:
  salt = empty (0 bytes)
  ikm  = chain_key (32 bytes)
  info = "SimpleXChainRatchet" (20 ASCII bytes)

Output: 96 bytes
  [0:32]   = message_key
  [32:64]  = new_chain_key
  [64:80]  = header_iv      // 16 bytes - FIRST!
  [80:96]  = message_iv     // 16 bytes - SECOND!
`

**Important:** The IV order is critical. header_iv comes before message_iv in the output.

### Chain KDF Code
`c
int smp_ratchet_chain_kdf(
    const uint8_t *chain_key,     // 32 bytes
    uint8_t *message_key,         // 32 bytes out
    uint8_t *new_chain_key,       // 32 bytes out
    uint8_t *header_iv,           // 16 bytes out
    uint8_t *message_iv           // 16 bytes out
) {
    uint8_t output[96];
    
    mbedtls_hkdf(
        mbedtls_md_info_from_type(MBEDTLS_MD_SHA512),
        NULL, 0,                  // salt (empty)
        chain_key, 32,            // ikm
        (uint8_t*)"SimpleXChainRatchet", 20,  // info
        output, 96
    );
    
    memcpy(message_key, output, 32);
    memcpy(new_chain_key, output + 32, 32);
    memcpy(header_iv, output + 64, 16);   // FIRST
    memcpy(message_iv, output + 80, 16);  // SECOND
    
    return 0;
}
`

---

## KDF Parameters Summary

| KDF | Salt | IKM | Info | Output |
|-----|------|-----|------|--------|
| X3DH | 64 × 0x00 | dh1+dh2+dh3 (168B) | "SimpleXX3DH" (11B) | 96 bytes |
| Root | root_key (32B) | dh_output (56B) | "SimpleXRootRatchet" (19B) | 96 bytes |
| Chain | empty (0B) | chain_key (32B) | "SimpleXChainRatchet" (20B) | 96 bytes |

---

## AES-256-GCM Encryption

SimpleX uses AES-256-GCM with 16-byte IVs (not 12-byte).

### Parameters

| Parameter | Size | Notes |
|-----------|------|-------|
| Key | 32 bytes | From message_key or header_key |
| IV | 16 bytes | From KDF output |
| Auth Tag | 16 bytes | GCM authentication tag |
| AAD | Variable | Additional authenticated data |

### Header Encryption

Encrypts the MsgHeader (88 bytes) to produce encrypted header body.
`
Input:
  key = header_key (32 bytes)
  iv  = header_iv (16 bytes)
  aad = rcAD (112 bytes)
  plaintext = MsgHeader (88 bytes)

Output:
  ciphertext = 88 bytes
  auth_tag = 16 bytes
`

### Header AAD (rcAD)

The Additional Authenticated Data for header encryption:
`
rcAD = our_ratchet_key_raw || peer_ratchet_key_raw
     = 56 bytes || 56 bytes
     = 112 bytes total

Note: Use RAW keys (56 bytes), not SPKI-encoded keys (68 bytes)
`

### Body Encryption

Encrypts the message body.
`
Input:
  key = message_key (32 bytes)
  iv  = message_iv (16 bytes)
  aad = payload_aad (235 bytes)
  plaintext = message body (variable)

Output:
  ciphertext = same length as plaintext
  auth_tag = 16 bytes
`

### Payload AAD

The Additional Authenticated Data for body encryption:
`
payload_aad = rcAD || emHeader
            = 112 bytes || 123 bytes
            = 235 bytes total
`

### AES-GCM Code Example
`c
int smp_aes_gcm_encrypt(
    const uint8_t *key,           // 32 bytes
    const uint8_t *iv,            // 16 bytes
    const uint8_t *aad,           // variable
    size_t aad_len,
    const uint8_t *plaintext,
    size_t plaintext_len,
    uint8_t *ciphertext,          // same length as plaintext
    uint8_t *tag                  // 16 bytes
) {
    mbedtls_gcm_context ctx;
    mbedtls_gcm_init(&ctx);
    
    mbedtls_gcm_setkey(&ctx, MBEDTLS_CIPHER_ID_AES, key, 256);
    
    mbedtls_gcm_crypt_and_tag(
        &ctx,
        MBEDTLS_GCM_ENCRYPT,
        plaintext_len,
        iv, 16,                   // 16-byte IV
        aad, aad_len,
        plaintext,
        ciphertext,
        16, tag                   // 16-byte tag
    );
    
    mbedtls_gcm_free(&ctx);
    return 0;
}
`

---

## SPKI Key Format

SimpleX transmits public keys in SPKI (Subject Public Key Info) format.

### X448 SPKI (68 bytes)
`
Offset  Size  Content
0       12    SPKI Header
12      56    Raw X448 public key

Header bytes: 30 42 30 05 06 03 2b 65 6f 03 39 00
`

### X25519 SPKI (44 bytes)
`
Offset  Size  Content
0       12    SPKI Header
12      32    Raw X25519 public key

Header bytes: 30 2a 30 05 06 03 2b 65 6e 03 21 00
`

### SPKI Encoding Code
`c
// X448 SPKI header
const uint8_t X448_SPKI_HEADER[12] = {
    0x30, 0x42, 0x30, 0x05, 0x06, 0x03,
    0x2b, 0x65, 0x6f, 0x03, 0x39, 0x00
};

// X25519 SPKI header
const uint8_t X25519_SPKI_HEADER[12] = {
    0x30, 0x2a, 0x30, 0x05, 0x06, 0x03,
    0x2b, 0x65, 0x6e, 0x03, 0x21, 0x00
};

int smp_encode_x448_spki(const uint8_t *raw_key, uint8_t *spki_key) {
    memcpy(spki_key, X448_SPKI_HEADER, 12);
    memcpy(spki_key + 12, raw_key, 56);
    return 68;
}
`

---

## wolfSSL Byte-Order Issue

wolfSSL exports X448 keys in reversed byte order compared to the SimpleX expectation.

### Problem
`
wolfSSL output:  [byte_55][byte_54]...[byte_1][byte_0]
SimpleX expects: [byte_0][byte_1]...[byte_54][byte_55]
`

### Solution

Reverse all bytes after key generation and before DH operations:
`c
void smp_x448_reverse_bytes(uint8_t *data, size_t len) {
    for (size_t i = 0; i < len / 2; i++) {
        uint8_t tmp = data[i];
        data[i] = data[len - 1 - i];
        data[len - 1 - i] = tmp;
    }
}

int smp_x448_generate_keypair(uint8_t *public_key, uint8_t *private_key) {
    // Generate with wolfSSL
    wc_curve448_make_key(&rng, 56, &key);
    
    // Export keys
    wc_curve448_export_public(&key, public_key);
    wc_curve448_export_private(&key, private_key);
    
    // Reverse byte order for SimpleX compatibility
    smp_x448_reverse_bytes(public_key, 56);
    smp_x448_reverse_bytes(private_key, 56);
    
    return 0;
}
`

---

## Verification Results

All cryptographic operations have been verified against Python reference implementations.

### Test Vectors Used

| Test | Input | Expected Output | Result |
|------|-------|-----------------|--------|
| X448 DH | Known key pairs | Known shared secret | Match |
| X3DH HKDF | Known DH outputs | Known derived keys | Match |
| Root KDF | Known inputs | Known outputs | Match |
| Chain KDF | Known inputs | Known outputs | Match |
| AES-GCM | Known plaintext | Known ciphertext | Match |

### Verification Status

| Component | Verification Method | Status |
|-----------|---------------------|--------|
| X448 Key Generation | Python cryptography library | Verified |
| X448 Diffie-Hellman | Python cryptography library | Verified |
| HKDF-SHA512 (X3DH) | Python cryptography library | Verified |
| HKDF-SHA512 (Root KDF) | Python cryptography library | Verified |
| HKDF-SHA512 (Chain KDF) | Python cryptography library | Verified |
| AES-256-GCM | Python cryptography library | Verified |
| Wire Format | Haskell source comparison | Verified |

---

## Python Verification Script
`python
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.asymmetric.x448 import X448PrivateKey
from cryptography.hazmat.backends import default_backend

def verify_chain_kdf(chain_key_hex):
    """Verify Chain KDF output matches ESP32 implementation."""
    chain_key = bytes.fromhex(chain_key_hex)
    
    hkdf = HKDF(
        algorithm=hashes.SHA512(),
        length=96,
        salt=b'',  # Empty salt
        info=b'SimpleXChainRatchet',
        backend=default_backend()
    )
    
    output = hkdf.derive(chain_key)
    
    message_key = output[0:32]
    new_chain_key = output[32:64]
    header_iv = output[64:80]
    message_iv = output[80:96]
    
    print(f"message_key:   {message_key.hex()}")
    print(f"new_chain_key: {new_chain_key.hex()}")
    print(f"header_iv:     {header_iv.hex()}")
    print(f"message_iv:    {message_iv.hex()}")
    
    return message_key, new_chain_key, header_iv, message_iv

def verify_aes_gcm(key_hex, iv_hex, aad_hex, plaintext_hex):
    """Verify AES-GCM encryption matches ESP32 implementation."""
    key = bytes.fromhex(key_hex)
    iv = bytes.fromhex(iv_hex)
    aad = bytes.fromhex(aad_hex)
    plaintext = bytes.fromhex(plaintext_hex)
    
    aesgcm = AESGCM(key)
    ciphertext_with_tag = aesgcm.encrypt(iv, plaintext, aad)
    
    ciphertext = ciphertext_with_tag[:-16]
    tag = ciphertext_with_tag[-16:]
    
    print(f"ciphertext: {ciphertext.hex()}")
    print(f"tag:        {tag.hex()}")
    
    return ciphertext, tag
`

---

## Common Issues and Solutions

### Issue #1: wolfSSL Byte Order

**Symptom:** DH shared secret doesn't match
**Cause:** wolfSSL uses reversed byte order
**Solution:** Reverse all key bytes after generation and before DH

### Issue #2: IV Order in Chain KDF

**Symptom:** Decryption fails with auth error
**Cause:** header_iv and message_iv swapped
**Solution:** header_iv is bytes [64:80], message_iv is bytes [80:96]

### Issue #3: SPKI vs Raw Keys in AAD

**Symptom:** Auth tag mismatch
**Cause:** Using 68-byte SPKI keys instead of 56-byte raw keys
**Solution:** Use raw keys (without SPKI header) for AAD construction

### Issue #4: Wrong IV Size

**Symptom:** Encryption/decryption produces garbage
**Cause:** Using 12-byte IV instead of 16-byte
**Solution:** SimpleX uses 16-byte IVs for AES-GCM

### Issue #5: Payload AAD Size

**Symptom:** Auth tag verification fails
**Cause:** Incorrect payload_aad size calculation
**Solution:** payload_aad = rcAD (112) + emHeader (123) = 235 bytes

---

## Security Considerations

### Forward Secrecy

- New keys derived for each message via Chain KDF
- DH ratchet advances periodically via Root KDF
- Compromise of current keys doesn't reveal past messages

### Break-in Recovery

- DH ratchet advances restore security after compromise
- New DH key pairs generated regularly
- Attacker loses access when ratchet advances

### Authentication

- AES-GCM provides authenticated encryption
- All messages include authentication tags
- Tampering is detected and rejected

---

## References

- Signal Protocol Specification: https://signal.org/docs/
- SimpleX Protocol Documentation: https://github.com/simplex-chat/simplexmq
- Double Ratchet Algorithm: https://signal.org/docs/specifications/doubleratchet/
- X3DH Key Agreement: https://signal.org/docs/specifications/x3dh/

---

## License

AGPL-3.0 - See [LICENSE](../LICENSE)
