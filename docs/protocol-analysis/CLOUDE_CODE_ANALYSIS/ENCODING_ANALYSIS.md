# AgentConfirmation Encoding Chain Analysis
 
Complete byte-level encoding chain from `AgentConfirmation` through all fields down to the `EncRatchetMessage` payload.
 
## 1. Maybe Encoding — `instance Encoding a => Encoding (Maybe a)`
 
**File:** `src/Simplex/Messaging/Encoding.hs:114-122`
 
```haskell
instance Encoding a => Encoding (Maybe a) where
  smpEncode = maybe "0" (('1' `B.cons`) . smpEncode)
  smpP = smpP >>= \case
    '0' -> pure Nothing
    '1' -> Just <$> smpP
    _ -> fail "invalid Maybe tag"
```
 
**Answer:** `Nothing` = `'0'` = **Byte 0x30** (ASCII character '0'), **NOT** `'\x00'` (byte 0).
 
| Value     | Encoding                          |
|-----------|-----------------------------------|
| `Nothing` | `0x30` (1 byte, ASCII '0')        |
| `Just a`  | `0x31` (ASCII '1') ++ smpEncode a |
 
---
 
## 2. AgentMsgEnvelope / AgentConfirmation Encoding
 
**File:** `src/Simplex/Messaging/Agent/Protocol.hs:772-819`
 
### Data Type
 
```haskell
data AgentMsgEnvelope
  = AgentConfirmation
      { agentVersion    :: VersionSMPA,                        -- Version Word16
        e2eEncryption_  :: Maybe (SndE2ERatchetParams 'C.X448), -- Maybe (AE2ERatchetParams 'X448)
        encConnInfo     :: ByteString                          -- EncRatchetMessage bytes
      }
  | AgentMsgEnvelope { ... }
  | AgentInvitation  { ... }
  | AgentRatchetKey  { ... }
```
 
### smpEncode
 
```haskell
AgentConfirmation {agentVersion, e2eEncryption_, encConnInfo} ->
  smpEncode (agentVersion, 'C', e2eEncryption_, Tail encConnInfo)
```
 
Tuple encoding is simple concatenation (`B.concat [smpEncode a, smpEncode b, smpEncode c, smpEncode d]`).
 
### Byte Layout of AgentConfirmation
 
```
Offset  Size     Field                Description
──────  ─────    ─────                ───────────
0       2        agentVersion         Word16 big-endian (VersionSMPA = Version Word16)
2       1        'C'                  Tag byte 0x43 (discriminator)
3       1+N      e2eEncryption_       Maybe (SndE2ERatchetParams 'X448), see §3
3+1+N   rest     encConnInfo          Tail (= remaining bytes, no length prefix)
```
 
---
 
## 3. e2eEncryption_ — Type and Encoding
 
**Type chain:**
```
e2eEncryption_ :: Maybe (SndE2ERatchetParams 'C.X448)
  where  type SndE2ERatchetParams a = AE2ERatchetParams a     -- Ratchet.hs:378
  where  data AE2ERatchetParams a = ∀s. AE2ERatchetParams (SRatchetKEMState s) (E2ERatchetParams s a)
  where  data E2ERatchetParams s a = E2ERatchetParams VersionE2E (PublicKey a) (PublicKey a) (Maybe (RKEMParams s))
```
 
**File:** `src/Simplex/Messaging/Crypto/Ratchet.hs:220-251`
 
### E2ERatchetParams Encoding (lines 238-241)
 
```haskell
smpEncode (E2ERatchetParams v k1 k2 kem_)
  | v >= pqRatchetE2EEncryptVersion = smpEncode (v, k1, k2, kem_)   -- v >= 3: includes KEM
  | otherwise                       = smpEncode (v, k1, k2)          -- v < 3: no KEM
```
 
### PublicKey X448 Encoding
 
`PublicKey a` is encoded via X.509 SubjectPublicKeyInfo DER:
```haskell
smpEncode = smpEncode . encodePubKey           -- Crypto.hs:567-568
encodePubKey = toPubKey $ encodeASNObj . publicToX509   -- Crypto.hs:605
```
 
This produces a DER-encoded ByteString, which is then length-prefixed with 1 byte (`smpEncode @ByteString`).
 
X448 raw key = 56 bytes. X.509 DER wrapping adds ~12 bytes → **~68 bytes** total.
Encoding: `[1 byte length] [DER bytes]` → **~69 bytes per key**.
 
### Full e2eEncryption_ Layout
 
#### Case: `Nothing`
```
Offset  Size  Content
0       1     0x30 ('0')
Total: 1 byte
```
 
#### Case: `Just (E2ERatchetParams v k1 k2 kem_)` with v < 3 (no PQ KEM)
```
Offset  Size   Content
0       1      0x31 ('1')                    -- Maybe tag: Just
1       2      VersionE2E                    -- Word16 big-endian
3       1+~68  PublicKey X448 #1 (k1)        -- 1-byte len + DER
~72     1+~68  PublicKey X448 #2 (k2)        -- 1-byte len + DER
Total: ~141 bytes
```
 
#### Case: `Just (E2ERatchetParams v k1 k2 kem_)` with v >= 3 (PQ KEM)
```
Offset  Size    Content
0       1       0x31 ('1')                    -- Maybe tag: Just
1       2       VersionE2E                    -- Word16 big-endian
3       1+~68   PublicKey X448 #1 (k1)        -- 1-byte len + DER
~72     1+~68   PublicKey X448 #2 (k2)        -- 1-byte len + DER
~141    varies  Maybe (RKEMParams s)          -- see below
```
 
**RKEMParams Encoding** (`Ratchet.hs:202-214`):
- `Nothing` → `0x30` (1 byte)
- `Just (RKParamsProposed pk)` → `0x31 'P'` + smpEncode KEMPublicKey (Large: 2-byte len + 1158 bytes)
- `Just (RKParamsAccepted ct pk)` → `0x31 'A'` + smpEncode KEMCiphertext (Large) + smpEncode KEMPublicKey (Large)
 
---
 
## 4. encConnInfo — Format and Relation to EncRatchetMessage
 
**Type:** `ByteString` (in the AgentConfirmation record)
 
**Content:** An encoded `EncRatchetMessage` — the output of `rcEncryptMsg` (`Ratchet.hs:970-975`):
 
```haskell
rcEncryptMsg MsgEncryptKey{..} paddedMsgLen msg = do
  (emAuthTag, emBody) <- encryptAEAD mk iv paddedMsgLen (msgRcAD <> msgEncHeader) msg
  let msg' = encodeEncRatchetMessage v EncRatchetMessage {emHeader = msgEncHeader, emBody, emAuthTag}
  pure msg'
```
 
**Production flow:**
1. Double Ratchet produces `MsgEncryptKey` with: message key, IV, AD, encrypted header
2. `encryptAEAD` encrypts the plaintext connection info (padded), producing `(AuthTag, ciphertext)`
3. `encodeEncRatchetMessage` serializes the triple `(emHeader, emAuthTag, emBody)` into a ByteString
4. This ByteString is stored in `encConnInfo`
 
**Padding length** (`Protocol.hs:316-320`):
```haskell
e2eEncConnInfoLength v = \case
  PQSupportOn | v >= pqdrSMPAgentVersion -> 11106
  _ -> 14832
```
 
**In AgentConfirmation encoding:** `encConnInfo` is wrapped in `Tail`, meaning the remaining bytes after parsing `e2eEncryption_` are all `encConnInfo` — **no length prefix**.
 
---
 
## 5. EncRatchetMessage — Definition and Encoding
 
**File:** `src/Simplex/Messaging/Crypto/Ratchet.hs:773-787`
 
### Data Type
 
```haskell
data EncRatchetMessage = EncRatchetMessage
  { emHeader  :: ByteString,   -- encoded EncMessageHeader
    emAuthTag :: AuthTag,      -- 16 bytes (AES-GCM auth tag, 128 bit)
    emBody    :: ByteString    -- encrypted message body (padded)
  }
```
 
### Encoding
 
```haskell
encodeEncRatchetMessage :: VersionE2E -> EncRatchetMessage -> ByteString
encodeEncRatchetMessage v EncRatchetMessage {emHeader, emBody, emAuthTag} =
  encodeLarge v emHeader <> smpEncode (emAuthTag, Tail emBody)
```
 
Where `encodeLarge` (`Ratchet.hs:759-762`):
```haskell
encodeLarge v s
  | v >= pqRatchetE2EEncryptVersion = smpEncode $ Large s  -- 2-byte Word16 length prefix
  | otherwise                       = smpEncode s          -- 1-byte length prefix
```
 
### EncRatchetMessage Byte Layout
 
#### Version < 3 (legacy, 1-byte length prefix):
```
Offset   Size        Content
0        1           emHeader length (1 byte) ← THIS IS THE 0x7b BYTE
1        N           emHeader (encoded EncMessageHeader)
1+N      16          emAuthTag (raw 16 bytes, no length prefix)
1+N+16   rest        emBody (Tail, remaining bytes)
```
 
#### Version >= 3 (PQ, 2-byte length prefix):
```
Offset   Size        Content
0        2           emHeader length (Word16 big-endian)
2        N           emHeader (encoded EncMessageHeader)
2+N      16          emAuthTag (raw 16 bytes, no length prefix)
2+N+16   rest        emBody (Tail, remaining bytes)
```
 
### The 0x7b (123) Byte Explained
 
`0x7b` = 123 decimal = `'{'` in ASCII.
 
In legacy mode (v < 3), this is the **1-byte length prefix** of `emHeader`. It means the `EncMessageHeader` is **123 bytes long**.
 
This is consistent with `fullHeaderLen v pq`:
```haskell
fullHeaderLen v pq = 2 + 1 + paddedHeaderLen v pq + authTagSize + ivSize @AES256
-- = 2 (VersionE2E) + 1 (ehBody length) + 88 (paddedHeaderLen for non-PQ) + 16 (AuthTag) + 16 (IV)
-- = 123 bytes
```
 
So `0x7b` is NOT a magic marker or '{' character — it's the **length 123** of the encrypted header.
 
---
 
## 6. EncMessageHeader — Inner Structure of emHeader
 
**File:** `src/Simplex/Messaging/Crypto/Ratchet.hs:742-756`
 
```haskell
data EncMessageHeader = EncMessageHeader
  { ehVersion :: VersionE2E,  -- current ratchet version
    ehIV      :: IV,          -- initialization vector
    ehAuthTag :: AuthTag,     -- authentication tag for header encryption
    ehBody    :: ByteString   -- encrypted MsgHeader (padded)
  }
 
smpEncode EncMessageHeader {ehVersion, ehIV, ehAuthTag, ehBody} =
  smpEncode (ehVersion, ehIV, ehAuthTag) <> encodeLarge ehVersion ehBody
```
 
### EncMessageHeader Byte Layout (v < 3)
 
```
Offset  Size   Content
0       2      ehVersion (Word16 BE)
2       16     ehIV (raw bytes, ivSize @AES256 = 16 for AES-256)
18      16     ehAuthTag (raw 16 bytes)
34      1+N    ehBody (1-byte length + N bytes of encrypted+padded MsgHeader)
```
 
Where `paddedHeaderLen _ _ = 88` (non-PQ), so:
```
34      1      ehBody length = 88 = 0x58
35      88     ehBody (encrypted MsgHeader)
Total: 123 bytes  ← matches 0x7b!
```
 
### EncMessageHeader Byte Layout (v >= 3, PQ)
 
```
Offset  Size   Content
0       2      ehVersion (Word16 BE)
2       16     ehIV
18      16     ehAuthTag
34      2+N    ehBody (2-byte Word16 length + N bytes, paddedHeaderLen = 2310)
Total: 2 + 16 + 16 + 2 + 2310 = 2346 bytes
```
 
---
 
## 7. Complete Encoding Chain — AgentConfirmation (Non-PQ, v < 3)
 
```
AGENT CONFIRMATION ENCODING (total structure)
═══════════════════════════════════════════════
 
Offset  Bytes  Field                        Hex Example    Source
──────  ─────  ─────                        ───────────    ──────
0       2      agentVersion                 00 XX          Word16 BE
2       1      Tag 'C'                      43             discriminator
3       1      Maybe tag                    30 or 31       '0'=Nothing, '1'=Just
 
── If e2eEncryption_ = Nothing (0x30): ──
4       ...    encConnInfo starts here (Tail)
 
── If e2eEncryption_ = Just (E2ERatchetParams v k1 k2 kem_): ──
4       2      VersionE2E                   00 02          e.g. version 2
6       1      k1 DER length               ~44            (68 bytes → 0x44)
7       ~68    k1 (PublicKey X448, DER)     ...            X.509 SubjectPublicKeyInfo
~75     1      k2 DER length               ~44
~76     ~68    k2 (PublicKey X448, DER)     ...
~144    ...    encConnInfo starts here (Tail)
 
── encConnInfo = encoded EncRatchetMessage: ──
 
~144    1      emHeader length              7B             = 123 bytes (v<3)
~145    123    emHeader (EncMessageHeader):
  ~145    2      ehVersion (VersionE2E)     00 02
  ~147    16     ehIV (AES256 IV)           ...
  ~163    16     ehAuthTag (header auth)    ...
  ~179    1      ehBody length             58              = 88 (paddedHeaderLen)
  ~180    88     ehBody (encrypted MsgHeader) ...
~268    16     emAuthTag (message auth tag)  ...
~284    rest   emBody (Tail, encrypted+padded connInfo, ~14832 or ~11106 bytes)
```
 
---
 
## 8. Complete Encoding Chain — AgentConfirmation (PQ, v >= 3)
 
```
Offset  Bytes   Field                        Source
──────  ─────   ─────                        ──────
0       2       agentVersion                 Word16 BE
2       1       Tag 'C'                      0x43
3       1       Maybe tag '1'                0x31 (Just)
4       2       VersionE2E                   00 03 (v=3)
6       1+~68   k1 (PublicKey X448)          1-byte len + DER
~75     1+~68   k2 (PublicKey X448)
~144    varies  Maybe (RKEMParams s)         see §3
        ...     encConnInfo (Tail):
 
── encConnInfo = encoded EncRatchetMessage (v >= 3): ──
+0      2       emHeader length (Large)      Word16 BE (~2346)
+2      ~2346   emHeader (EncMessageHeader):
  +0      2       ehVersion                 00 03
  +2      16      ehIV
  +18     16      ehAuthTag
  +34     2       ehBody length (Large)     Word16 BE (2310)
  +36     2310    ehBody (encrypted MsgHeader with PQ KEM keys)
+2348   16      emAuthTag
+2364   rest    emBody (Tail, encrypted+padded connInfo)
```
 
---
 
## Key Reference Summary
 
| Primitive          | Encoding Rule                                        | File:Line                          |
|--------------------|------------------------------------------------------|------------------------------------|
| `Word16`           | 2 bytes big-endian                                   | `Encoding.hs:70-74`                |
| `Char`             | 1 byte (B.singleton)                                 | `Encoding.hs:52-56`                |
| `ByteString`       | 1-byte length prefix + data                          | `Encoding.hs:100-104`              |
| `Large`            | 2-byte Word16 length prefix + data                   | `Encoding.hs:132-141`              |
| `Tail`             | Remaining bytes (no length prefix)                   | `Encoding.hs:124-130`              |
| `Maybe a`          | `'0'` (0x30) = Nothing, `'1'` (0x31) + data = Just   | `Encoding.hs:114-122`              |
| `Version v`        | Word16 (delegates to smpEncode @Word16)              | `Version/Internal.hs:12-14`        |
| `AuthTag`          | 16 raw bytes (128 bit, no length prefix)             | `Crypto.hs:956-958`                |
| `IV`               | 16 raw bytes (ivSize @AES256 = blockSize = 16)       | `Crypto.hs:935-937`                |
| `PublicKey a`      | ByteString-encoded (1-byte len + X.509 DER)          | `Crypto.hs:567-568`                |
| `KEMPublicKey`     | Large-encoded (2-byte len + 1158 raw bytes)          | `SNTRUP761/Bindings.hs:74-76`      |
| `KEMCiphertext`    | Large-encoded (2-byte len + 1039 raw bytes)          | `SNTRUP761/Bindings.hs:82-83`      |
| `E2ERatchetParams` | v + k1 + k2 [+ kem_ if v≥3]                          | `Ratchet.hs:238-241`               |
| `EncRatchetMessage`| encodeLarge(emHeader) + AuthTag + Tail(emBody)       | `Ratchet.hs:779-781`               |
| `EncMessageHeader` | (v, IV, AuthTag) + encodeLarge(ehBody)               | `Ratchet.hs:750-752`               |
| `Tuples`           | Simple concatenation of smpEncode of each element    | `Encoding.hs:184-212`              |