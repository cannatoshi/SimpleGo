# Double Ratchet Implementation Analysis
 
Analysis of `src/Simplex/Messaging/Crypto/Ratchet.hs` — the SimpleX Double Ratchet with
Post-Quantum KEM Hybrid and Header Encryption (PQ2HE variant).
 
## 1. rcAD (Associated Data) Computation
 
**Location**: `pqX3dh` at `Ratchet.hs:503`
 
```haskell
assocData = Str $ pubKeyBytes sk1 <> pubKeyBytes rk1
```
 
`rcAD` is the raw byte concatenation of two public DH keys:
 
- `sk1` — the sender's (Alice's) first public DH key
- `rk1` — the receiver's (Bob's) first public DH key
 
For X448 keys this yields `56 + 56 = 112 bytes`. Both parties compute identical `rcAD`
values because:
 
- **Sender** (`pqX3dhSnd:470`): calls `pqX3dh (publicKey spk1, rk1) ...`
- **Receiver** (`pqX3dhRcv:487`): calls `pqX3dh (sk1, publicKey rpk1) ...`
 
The sender's `publicKey spk1` corresponds to what the receiver sees as `sk1`, and vice versa.
 
`rcAD` is subsequently used as AAD in **every** AEAD operation — both header encryption
and message encryption — for the lifetime of the ratchet.
 
## 2. X3DH DH Operations Order
 
### Sender Side (`pqX3dhSnd:471`)
 
```haskell
pqX3dh (publicKey spk1, rk1) (dh' rk1 spk2) (dh' rk2 spk1) (dh' rk2 spk2) kem_
--                              ^^^DH1^^^       ^^^DH2^^^       ^^^DH3^^^
```
 
### Receiver Side (`pqX3dhRcv:487`)
 
```haskell
pqX3dh (sk1, publicKey rpk1) (dh' sk2 rpk1) (dh' sk1 rpk2) (dh' sk2 rpk2) ...
--                             ^^^DH1^^^       ^^^DH2^^^       ^^^DH3^^^
```
 
### DH Operation Semantics
 
| DH | Computation (Sender) | Computation (Receiver) | Semantics |
|----|---------------------|----------------------|-----------|
| DH1 | `dh'(rk1, spk2)` | `dh'(sk2, rpk1)` | Bob Identity × Alice Ephemeral |
| DH2 | `dh'(rk2, spk1)` | `dh'(sk1, rpk2)` | Bob Ephemeral × Alice Identity |
| DH3 | `dh'(rk2, spk2)` | `dh'(sk2, rpk2)` | Bob Ephemeral × Alice Ephemeral |
 
### Concatenation (`pqX3dh:504`)
 
```haskell
dhs = dhBytes' dh1 <> dhBytes' dh2 <> dhBytes' dh3 <> pq
```
 
The IKM for the initial HKDF is `DH1 || DH2 || DH3 [|| KEM_SS]`, where the optional
SNTRUP761 shared secret is appended only when post-quantum KEM is active:
 
```haskell
pq = maybe "" (\RatchetKEMAccepted {rcPQRss = KEMSharedKey ss} -> BA.convert ss) kemAccepted
```
 
## 3. Derivation of hk and nhk from KDF
 
All KDF derivations use HKDF-SHA512 via the `hkdf3` helper (`Ratchet.hs:1174-1179`):
 
```haskell
hkdf3 salt ikm info = (s1, s2, s3)
  where
    out = hkdf salt ikm info 96      -- HKDF-SHA512, 96 bytes output
    (s1, rest) = B.splitAt 32 out    -- s1 = bytes [0..31]
    (s2, s3)   = B.splitAt 32 rest   -- s2 = bytes [32..63], s3 = bytes [64..95]
```
 
### Initial X3DH KDF (`pqX3dh:506-508`)
 
```haskell
(hk, nhk, sk) =
  let salt = B.replicate 64 '\0'
   in hkdf3 salt dhs "SimpleXX3DH"
```
 
| Byte Range | Variable | Purpose |
|-----------|----------|---------|
| `[0..31]`  | `hk`  | Sender Header Key (`sndHK = Key hk`) |
| `[32..63]` | `nhk` | Receiver Next Header Key (`rcvNextHK = Key nhk`) |
| `[64..95]` | `sk`  | Root Key (`ratchetKey = RatchetKey sk`) |
 
- **Salt**: 64 zero bytes
- **IKM**: `DH1 \|\| DH2 \|\| DH3 [\|\| KEM_SS]`
- **Info**: `"SimpleXX3DH"`
 
### Root Ratchet KDF (`rootKdf:1159-1166`)
 
Called at each ratchet step to derive new keys:
 
```haskell
rootKdf (RatchetKey rk) k pk kemSecret_ =
  let dhOut = dhBytes' (dh' k pk)
      ss = case kemSecret_ of
        Just (KEMSharedKey s) -> dhOut <> BA.convert s
        Nothing -> dhOut
      (rk', ck, nhk) = hkdf3 rk ss "SimpleXRootRatchet"
   in (RatchetKey rk', RatchetKey ck, Key nhk)
```
 
| Byte Range | Variable | Purpose |
|-----------|----------|---------|
| `[0..31]`  | `rk'` | New Root Key |
| `[32..63]` | `ck`  | New Chain Key |
| `[64..95]` | `nhk` | **Next Header Key** |
 
- **Salt**: current Root Key `rk`
- **IKM**: `DH_output [\|\| KEM_SS]`
- **Info**: `"SimpleXRootRatchet"`
 
### Chain KDF (`chainKdf:1168-1172`)
 
```haskell
chainKdf (RatchetKey ck) =
  let (ck', mk, ivs) = hkdf3 "" ck "SimpleXChainRatchet"
      (iv1, iv2) = B.splitAt 16 ivs
   in (RatchetKey ck', Key mk, IV iv1, IV iv2)
```
 
| Byte Range | Variable | Purpose |
|-----------|----------|---------|
| `[0..31]`  | `ck'` | New Chain Key |
| `[32..63]` | `mk`  | Message Key (AES-256-GCM) |
| `[64..79]` | `iv1` | IV for message encryption |
| `[80..95]` | `iv2` | IV for header encryption (`ehIV`) |
 
- **Salt**: empty string
- **IKM**: current Chain Key
- **Info**: `"SimpleXChainRatchet"`
 
## 4. AES-GCM AAD Format for Header Encryption
 
There are **two layers** of AEAD encryption, each with different AAD.
 
### Layer 1: Header Encryption (`rcEncryptHeader:918`)
 
```haskell
-- Encryption
(ehAuthTag, ehBody) <- encryptAEAD rcHKs ehIV (paddedHeaderLen v rcSupportKEM') rcAD (msgHeader v maxSupported')
 
-- Decryption (rcDecrypt:1152)
header <- decryptAEAD k ehIV rcAD ehBody ehAuthTag
```
 
**AAD = `rcAD`** — the raw 112 bytes (X448) of the two identity public keys.
 
The underlying primitive is `crypton`'s `aeadSimpleEncrypt` with AES-256-GCM
(`Crypto.hs:1012-1016`):
 
```haskell
encryptAEAD aesKey ivBytes paddedLen ad msg = do
  aead <- initAEAD @AES256 aesKey ivBytes
  msg' <- liftEither $ pad msg paddedLen
  pure . first AuthTag $ AES.aeadSimpleEncrypt aead ad msg' authTagSize
```
 
The `ad` (= `rcAD`) byte string is passed directly to GCM as associated data
without any additional encoding.
 
### Layer 2: Message Encryption (`rcEncryptMsg:973`)
 
```haskell
-- Encryption
(emAuthTag, emBody) <- encryptAEAD mk iv paddedMsgLen (msgRcAD <> msgEncHeader) msg
 
-- Decryption (rcDecrypt:1157)
tryE $ decryptAEAD mk iv (rcAD <> emHeader) emBody emAuthTag
```
 
**AAD = `rcAD` || `emHeader`** — the identity keys concatenated with the
**full serialized encrypted header** (including version, IV, AuthTag, ciphertext).
 
The `emHeader` is the `smpEncode` result of `EncMessageHeader` (`:920`):
 
```haskell
emHeader = smpEncode EncMessageHeader {ehVersion = v, ehBody, ehAuthTag, ehIV}
```
 
This yields the byte format:
`version(2B) || IV(16B) || AuthTag(16B) || length-prefixed(encrypted_header_body)`
 
### AAD Summary
 
```
Header AEAD:   AAD = pubKeyBytes(sk1) || pubKeyBytes(rk1)
                    = 112 bytes (X448)
 
Message AEAD:  AAD = pubKeyBytes(sk1) || pubKeyBytes(rk1) || smpEncode(EncMessageHeader)
                    = 112 bytes + variable (encrypted header)
```
 
### Security Properties
 
1. **Header encryption**: Headers can only be decrypted by parties knowing the correct
   Header Key AND having the correct identity binding (via rcAD as AAD).
 
2. **Message-to-header binding**: The message ciphertext is cryptographically bound to
   the exact encrypted header — any tampering with the header will cause message
   decryption to fail.
 
3. **Header padding**: Headers are padded to fixed lengths (`88` bytes without PQ,
   `2310` bytes with PQ) before encryption, preventing header-size-based traffic analysis.