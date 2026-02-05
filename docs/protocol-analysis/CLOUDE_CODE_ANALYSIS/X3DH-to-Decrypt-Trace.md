# X3DH → First EncRatchetMessage Decrypt: Complete Trace
 
**Perspektive:** Invitation Creator (Alice)
**Kontext:** Alice hat die Invitation erstellt. Bob (Joiner) hat mit `AgentConfirmation` geantwortet. `e2eEncryption_ = Nothing` (keine neuen X448 Keys in der Nachricht). Die AgentConfirmation enthält ein verschlüsseltes `EncRatchetMessage`.
 
---
 
## 1. Key Setup: Was Alice und Bob vor dem X3DH haben
 
### Alice (Invitation Creator) generiert bei Connection-Erstellung:
- `rpk1` (PrivateKey X448) — Alice's erster X3DH-Key ("identity key")
- `rpk2` (PrivateKey X448) — Alice's zweiter X3DH-Key ("ephemeral key"), wird auch als DH-Ratchet-Key wiederverwendet
- Optional: `rpKem` (NTRU-Prime KEM Key Pair)
 
Alice sendet `(publicKey rpk1, publicKey rpk2, kem_public)` als `RcvE2ERatchetParams` in der Invitation.
Speichert `(rpk1, rpk2, rpKem)` in der DB via `createRatchetX3dhKeys`.
 
### Bob (Joiner) generiert beim Join:
- `spk1` (PrivateKey X448) — Bob's erster X3DH-Key
- `spk2` (PrivateKey X448) — Bob's zweiter X3DH-Key
- `rcDHRs_bob` (PrivateKey X448) — Bob's **separater** DH-Ratchet-Key (frisch generiert, NICHT spk1/spk2!)
- Optional: `spKem` (KEM Key Pair + KEM Encapsulation)
 
Bob sendet `(publicKey spk1, publicKey spk2, kem_params)` als `SndE2ERatchetParams` in der AgentConfirmation.
 
---
 
## 2. X3DH Shared Secret (identisch auf beiden Seiten)
 
### DH-Operationen
 
Beide Seiten berechnen drei DH-Secrets (X448):
 
```
dh1 = DH(Alice_pk1, Bob_sk2) = DH(Bob_pk2, Alice_sk1)
dh2 = DH(Alice_pk2, Bob_sk1) = DH(Bob_pk1, Alice_sk2)
dh3 = DH(Alice_pk2, Bob_sk2) = DH(Bob_pk2, Alice_sk2)
```
 
**Konkret in `pqX3dhRcv` (Alice)** — `Ratchet.hs:485-487`:
```haskell
pqX3dhRcv rpk1 rpk2 rpKem_ (E2ERatchetParams v sk1 sk2 sKem_) = do
  kem_ <- rcvPq
  let initParams = pqX3dh (sk1, publicKey rpk1)
                          (dh' sk2 rpk1)      -- dh1 = DH(Bob_pk2, Alice_sk1)
                          (dh' sk1 rpk2)      -- dh2 = DH(Bob_pk1, Alice_sk2)
                          (dh' sk2 rpk2)      -- dh3 = DH(Bob_pk2, Alice_sk2)
                          (snd <$> kem_)
```
 
### HKDF #1: X3DH → Initiale Keys
 
**Funktion: `pqX3dh`** — `Ratchet.hs:499-508`:
 
```
Input Key Material (IKM):
    dhs = dhBytes(dh1) || dhBytes(dh2) || dhBytes(dh3) [|| kemSharedSecret]
    Länge: 3 × 56 Bytes = 168 Bytes (X448) [+ optional KEM]
 
Salt:   64 Zero-Bytes (B.replicate 64 '\0')
Info:   "SimpleXX3DH"
Hash:   SHA-512
Output: 96 Bytes → Split in 3 × 32 Bytes
```
 
```
┌─────────────────────────────────────────────────┐
│ HKDF-SHA512(salt=zeros(64), ikm=dhs,            │
│             info="SimpleXX3DH", len=96)          │
├────────────┬────────────┬────────────────────────┤
│ Bytes 0-31 │ Bytes 32-63│ Bytes 64-95            │
│    hk      │    nhk     │    sk                  │
│ = sndHK    │ = rcvNextHK│ = ratchetKey           │
└────────────┴────────────┴────────────────────────┘
```
 
**Ergebnis: `RatchetInitParams`**
 
| Feld         | Wert              | Bedeutung                          |
|-------------|-------------------|------------------------------------|
| `assocData` | `Bob_pk1 \|\| Alice_pk1` | Associated Data für AES-GCM       |
| `sndHK`     | `Key hk`          | Header Key (32 Bytes)              |
| `rcvNextHK` | `Key nhk`         | Next Header Key (32 Bytes)         |
| `ratchetKey`| `RatchetKey sk`    | Root Key für Ratchet-Init (32 Bytes)|
 
**Wichtig:** `assocData = pubKeyBytes sk1 <> pubKeyBytes rk1` wobei `sk1` = Bob's erster Public Key (aus den empfangenen Params) und `rk1` = Alice's erster Public Key. Dadurch ist die Reihenfolge auf beiden Seiten identisch: immer `Joiner_pk1 || Creator_pk1`.
 
---
 
## 3. Ratchet-Initialisierung: Alice (initRcvRatchet)
 
**Funktion: `initRcvRatchet`** — `Ratchet.hs:674-699`:
 
Alice ruft `initRcvRatchet` auf — `Agent.hs:2920`:
```haskell
let rc = CR.initRcvRatchet rcVs rcDHRs rcParams pqSupport'
```
 
**Ergebnis: Alice's Ratchet-State (BEVOR die erste Nachricht entschlüsselt wird)**
 
```
Ratchet {
    rcAD     = "Bob_pk1 || Alice_pk1"        -- Associated Data
    rcDHRs   = Alice_sk2                      -- Alice's DH-Ratchet Private Key (= rpk2)
    rcRK     = sk                             -- Root Key (direkt aus X3DH, KEIN rootKdf!)
    rcSnd    = Nothing                        -- Noch kein Sende-Ratchet
    rcRcv    = Nothing                        -- Noch kein Empfangs-Ratchet
    rcNs     = 0
    rcNr     = 0
    rcPN     = 0
    rcNHKs   = nhk                            -- Next Header Key für Senden
    rcNHKr   = hk                             -- Next Header Key für Empfangen ← KRITISCH
}
```
 
**Beachte:** Alice's `rcNHKr = hk` (sndHK aus X3DH) — das ist der Key, mit dem die ERSTE empfangene Nachricht-Header entschlüsselt wird.
 
---
 
## 4. Bob's Ratchet-Initialisierung (initSndRatchet) — zum Verständnis
 
**Funktion: `initSndRatchet`** — `Ratchet.hs:643-666`:
 
Bob macht bei Initialisierung bereits einen `rootKdf`-Schritt:
 
### HKDF #2: Root KDF bei Bob's initSndRatchet
 
```haskell
let (rcRK, rcCKs, rcNHKs) = rootKdf ratchetKey rcDHRr rcDHRs (rcPQRss <$> kemAccepted)
```
 
**Funktion: `rootKdf`** — `Ratchet.hs:1159-1166`:
 
```
DH-Operation:
    dh_out = DH(rcDHRr=Alice_pk2, rcDHRs=Bob_ratchet_priv)
    (Bob_ratchet_priv ist Bob's frisch generierter DH-Ratchet Key, NICHT spk1/spk2!)
 
Input Key Material (IKM):
    ss = dhBytes(dh_out) [|| kemSharedSecret]
 
Salt:   sk (der X3DH Root Key, 32 Bytes)
Info:   "SimpleXRootRatchet"
Hash:   SHA-512
Output: 96 Bytes → Split in 3 × 32 Bytes
```
 
```
┌──────────────────────────────────────────────────┐
│ HKDF-SHA512(salt=sk, ikm=DH(Alice_pk2,Bob_ratch),│
│             info="SimpleXRootRatchet", len=96)    │
├────────────┬────────────┬─────────────────────────┤
│ Bytes 0-31 │ Bytes 32-63│ Bytes 64-95             │
│    rk'     │    ck      │    nhk'                 │
│ = rcRK     │ = rcCKs    │ = rcNHKs                │
└────────────┴────────────┴─────────────────────────┘
```
 
**Bob's Ratchet-State nach initSndRatchet:**
 
```
Ratchet {
    rcAD     = "Bob_pk1 || Alice_pk1"
    rcDHRs   = Bob_ratchet_priv               -- Bob's DH-Ratchet Key
    rcRK     = rk'                            -- Neuer Root Key (nach rootKdf)
    rcSnd    = Just SndRatchet {
        rcDHRr = Alice_pk2,                   -- Peer's Public Key
        rcCKs  = ck,                          -- Sending Chain Key
        rcHKs  = hk                           -- Sending Header Key (= sndHK aus X3DH)
    }
    rcRcv    = Nothing
    rcNHKs   = nhk'                           -- Next Header Key für Senden (aus rootKdf)
    rcNHKr   = nhk                            -- Next Header Key für Empfangen (aus X3DH)
}
```
 
**Kritisch:** Bob's `rcHKs = hk` = Alice's `rcNHKr = hk`. Das ist der Schlüssel, mit dem Bob den Header verschlüsselt und Alice ihn entschlüsselt.
 
---
 
## 5. Bob verschlüsselt die erste Nachricht (rcEncryptHeader + rcEncryptMsg)
 
**Funktion: `rcEncryptHeader`** — `Ratchet.hs:902-937`:
 
### 5a. Chain KDF: Ableitung des Message Keys
 
```haskell
let (ck', mk, iv, ehIV) = chainKdf rcCKs
```
 
### HKDF #3: Chain KDF
 
**Funktion: `chainKdf`** — `Ratchet.hs:1168-1172`:
 
```
Input Key Material (IKM): rcCKs (= ck aus HKDF #2, 32 Bytes)
Salt:   "" (leerer String)
Info:   "SimpleXChainRatchet"
Hash:   SHA-512
Output: 96 Bytes → Split:
```
 
```
┌──────────────────────────────────────────────────┐
│ HKDF-SHA512(salt="", ikm=rcCKs,                  │
│             info="SimpleXChainRatchet", len=96)   │
├────────────┬────────────┬─────────────────────────┤
│ Bytes 0-31 │ Bytes 32-63│ Bytes 64-95             │
│    ck'     │    mk      │    ivs                  │
│ = next CK  │ = MsgKey   │ (split 16+16)           │
│            │  (32 Bytes)│ iv1=hdrIV, iv2=msgIV    │
└────────────┴────────────┴─────────────────────────┘
```
 
| Output      | Bytes  | Verwendung                |
|------------|--------|---------------------------|
| `ck'`      | 0-31   | Nächster Chain Key         |
| `mk`       | 32-63  | Message Key (AES-256 Key)  |
| `iv1`/ehIV | 64-79  | IV für Header-Encryption   |
| `iv2`/iv   | 80-95  | IV für Message-Encryption  |
 
### 5b. Header verschlüsseln
 
```haskell
(ehAuthTag, ehBody) <- encryptAEAD rcHKs ehIV (paddedHeaderLen v rcSupportKEM') rcAD (msgHeader v maxSupported')
```
 
**AES-256-GCM Encrypt (Header):**
 
```
Key:      rcHKs = hk (32 Bytes, aus X3DH HKDF #1)
IV:       ehIV = iv1 (16 Bytes, aus Chain KDF HKDF #3)
AD:       rcAD = "Bob_pk1 || Alice_pk1"
Plaintext: encodeMsgHeader(MsgHeader {
              msgMaxVersion,
              msgDHRs = Bob_ratchet_pub,    ← Bob's DH-Ratchet Public Key
              msgKEM  = ...,                ← Optional KEM Params
              msgPN   = 0,                  ← Previous Chain Length
              msgNs   = 0                   ← Message Number in Chain
           })
Padding:  auf paddedHeaderLen aufgefüllt (PKCS-style mit 2-Byte Längenprefix)
 
Output:   (ehAuthTag: 16 Bytes, ehBody: verschlüsselter+gepadder Header)
```
 
**Ergebnis: `EncMessageHeader`**
```
EncMessageHeader {
    ehVersion = current ratchet version,
    ehIV      = iv1,
    ehAuthTag = 16-Byte Auth Tag,
    ehBody    = AES-GCM-Encrypt(hk, iv1, rcAD, padded_header)
}
```
 
Der vollständige `emHeader` = `smpEncode(EncMessageHeader)` (serialisiert mit Version, IV, AuthTag, Body).
 
### 5c. Nachricht verschlüsseln
 
**Funktion: `rcEncryptMsg`** — `Ratchet.hs:970-975`:
 
```haskell
(emAuthTag, emBody) <- encryptAEAD mk iv paddedMsgLen (msgRcAD <> msgEncHeader) msg
```
 
**AES-256-GCM Encrypt (Message Body):**
 
```
Key:      mk (32 Bytes, aus Chain KDF HKDF #3)
IV:       iv = iv2 (16 Bytes, aus Chain KDF HKDF #3)
AD:       rcAD || emHeader
          = "Bob_pk1 || Alice_pk1" || smpEncode(EncMessageHeader)
Plaintext: Die AgentConfirmation-Payload (AgentConnInfoReply)
Padding:  auf paddedMsgLen aufgefüllt
 
Output:   (emAuthTag: 16 Bytes, emBody: verschlüsselter+gepadder Body)
```
 
**Ergebnis: `EncRatchetMessage`** (das finale verschlüsselte Paket):
```
EncRatchetMessage {
    emHeader  = smpEncode(EncMessageHeader{ehVersion, ehIV, ehAuthTag, ehBody}),
    emAuthTag = 16-Byte Auth Tag (für Message Body),
    emBody    = AES-GCM-Encrypt(mk, iv2, rcAD||emHeader, padded_msg)
}
```
 
---
 
## 6. Alice entschlüsselt: `rcDecrypt` — der vollständige Pfad
 
**Funktion: `rcDecrypt`** — `Ratchet.hs:990-1157`
 
**Aufruf in `smpConfirmation`** — `Agent.hs:2922`:
```haskell
(agentMsgBody_, rc', skipped) <- liftError cryptoError $ CR.rcDecrypt g rc M.empty encConnInfo
```
 
Hier ist `rc` der gerade eben mit `initRcvRatchet` erstellte Ratchet (siehe Schritt 3).
`M.empty` = keine übersprungenen Keys (erste Nachricht).
 
### Schritt 6.1: Parsing
 
```haskell
encMsg@EncRatchetMessage {emHeader} <- parseE CryptoHeaderError encRatchetMessageP msg'
encHdr <- parseE CryptoHeaderError smpP emHeader
```
 
- `msg'` wird zu `EncRatchetMessage { emHeader, emAuthTag, emBody }` geparst
- `emHeader` wird zu `EncMessageHeader { ehVersion, ehIV, ehAuthTag, ehBody }` geparst
 
### Schritt 6.2: Skipped Keys prüfen
 
```haskell
decryptSkipped encHdr encMsg >>= \case
    SMNone -> ...
```
 
Da `rcMKSkipped = M.empty`, gibt es keine übersprungenen Keys → `SMNone`.
 
### Schritt 6.3: Header entschlüsseln
 
```haskell
(rcStep, hdr) <- decryptRcHeader rcRcv encHdr
```
 
Da `rcRcv = Nothing` (initRcvRatchet hat keinen RcvRatchet erstellt):
 
```haskell
decryptRcHeader Nothing hdr = decryptNextHeader hdr
decryptNextHeader hdr = (AdvanceRatchet,) <$> decryptHeader (rcNHKr rc) hdr
```
 
**→ Entschlüsselung mit `rcNHKr = hk` (aus X3DH HKDF #1)**
 
```haskell
decryptHeader k EncMessageHeader {ehVersion, ehBody, ehAuthTag, ehIV} = do
    header <- decryptAEAD k ehIV rcAD ehBody ehAuthTag
    parseE' CryptoHeaderError (msgHeaderP ehVersion) header
```
 
**AES-256-GCM Decrypt (Header):**
 
```
Key:      rcNHKr = hk (32 Bytes) — direkt aus X3DH HKDF #1, Bytes 0-31
IV:       ehIV = iv1 (16 Bytes) — aus dem EncMessageHeader
AD:       rcAD = "Bob_pk1 || Alice_pk1" (aus pqX3dh assocData)
Ciphertext: ehBody
AuthTag:  ehAuthTag (16 Bytes)
 
→ decryptAEAD: initAEAD @AES256 mit 16-Byte IV
→ AES.aeadSimpleDecrypt(aead, AD, ciphertext, authTag)
→ unPad (entfernt PKCS-Padding)
→ Ergebnis: MsgHeader (Klartext)
```
 
**Ergebnis: `rcStep = AdvanceRatchet`** (weil `rcNHKr` benutzt wurde, nicht `rcHKr`).
 
### Schritt 6.4: MsgHeader Parsen
 
**Typ: `MsgHeader`** — `Ratchet.hs:703-720`:
 
```haskell
data MsgHeader a = MsgHeader
    { msgMaxVersion :: VersionE2E,
      msgDHRs       :: PublicKey a,     -- Bob's DH-Ratchet Public Key
      msgKEM        :: Maybe ARKEMParams,
      msgPN         :: Word32,          -- Previous Chain Length (= 0)
      msgNs         :: Word32           -- Message Number (= 0)
    }
```
 
Der entschlüsselte Header enthält:
- `msgDHRs` = **Bob's DH-Ratchet Public Key** (frisch generiert, NICHT spk1/spk2)
- `msgPN = 0` (kein vorheriger Chain)
- `msgNs = 0` (erste Nachricht)
 
### Schritt 6.5: Ratchet Step (DH-Ratchet vorwärts bewegen)
 
Da `rcStep = AdvanceRatchet`, wird `ratchetStep` ausgeführt — `Ratchet.hs:1043-1071`:
 
Zuerst: `skipMessageKeys msgPN=0` → da `rcRcv = Nothing`, passiert nichts.
 
Dann:
```haskell
ratchetStep rc' MsgHeader {msgDHRs, msgKEM} = do
    (kemSS, kemSS', rcKEM') <- pqRatchetStep rc' msgKEM
    (_, rcDHRs') <- atomically $ generateKeyPair @a g    -- Alice generiert neuen DH-Key
    let (rcRK', rcCKr', rcNHKr') = rootKdf rcRK msgDHRs rcDHRs kemSS        -- ← HKDF #4
        (rcRK'', rcCKs', rcNHKs') = rootKdf rcRK' msgDHRs rcDHRs' kemSS'    -- ← HKDF #5
```
 
### HKDF #4: Erster Root KDF (Empfangs-Chain ableiten)
 
```
DH-Operation:
    dh_out = DH(msgDHRs=Bob_ratchet_pub, rcDHRs=Alice_sk2)
 
    Dies ist IDENTISCH mit Bob's initSndRatchet rootKdf:
    DH(rcDHRr=Alice_pk2, rcDHRs=Bob_ratchet_priv) = DH(Bob_ratchet_pub, Alice_sk2)
 
Input Key Material (IKM): dhBytes(dh_out) [|| kemSharedSecret]
Salt:   rcRK = sk (X3DH Root Key, 32 Bytes)
Info:   "SimpleXRootRatchet"
Output: 96 Bytes → Split:
```
 
```
┌──────────────────────────────────────────────────────────┐
│ HKDF-SHA512(salt=sk, ikm=DH(Bob_ratch_pub, Alice_sk2),  │
│             info="SimpleXRootRatchet", len=96)            │
├────────────┬────────────┬─────────────────────────────────┤
│    rk'     │    ck_r    │    nhk_r                        │
│ = rcRK'    │ = rcCKr'   │ = rcNHKr'                       │
│ new root   │ recv chain │ next recv header key             │
└────────────┴────────────┴─────────────────────────────────┘
```
 
**Kritisch:** `rcCKr'` hier = Bob's `rcCKs` (aus HKDF #2). Dieselben Inputs → dieselben Outputs.
 
### HKDF #5: Zweiter Root KDF (Sende-Chain ableiten)
 
```
DH-Operation:
    dh_out' = DH(msgDHRs=Bob_ratchet_pub, rcDHRs'=Alice_new_priv)
    (Alice_new_priv ist Alice's frisch generierter DH-Ratchet Key)
 
Salt:   rcRK' (der neue Root Key aus HKDF #4)
Info:   "SimpleXRootRatchet"
Output: 96 Bytes → (rcRK'', rcCKs', rcNHKs')
```
 
**Ergebnis: Neuer Ratchet-State nach ratchetStep:**
 
```
Ratchet {
    rcDHRs   = Alice_new_priv               -- Frisch generiert
    rcRK     = rcRK''                       -- Root Key (nach 2× rootKdf)
    rcSnd    = Just SndRatchet {
        rcDHRr = Bob_ratchet_pub,           -- Peer's Ratchet Key (aus Header)
        rcCKs  = rcCKs',                    -- Sende Chain Key
        rcHKs  = rcNHKs                     -- Header Key = Alice's alter rcNHKs (= nhk aus X3DH)
    }
    rcRcv    = Just RcvRatchet {
        rcCKr  = rcCKr',                    -- Empfangs Chain Key (= Bob's rcCKs!)
        rcHKr  = rcNHKr                     -- Header Key = Alice's alter rcNHKr (= hk aus X3DH)
    }
    rcPN     = 0                            -- Alice hatte noch nichts gesendet
    rcNs     = 0
    rcNr     = 0
    rcNHKs   = rcNHKs'                      -- Aus HKDF #5
    rcNHKr   = rcNHKr'                      -- Aus HKDF #4
}
```
 
### Schritt 6.6: Skip Message Keys bis msgNs
 
```haskell
case skipMessageKeys msgNs rc' of ...
```
 
Da `msgNs = 0` und `rcNr = 0` → `rcNr == untilN` → keine Keys übersprungen.
 
### Schritt 6.7: Message Key ableiten und Body entschlüsseln
 
```haskell
let (rcCKr', mk, iv, _) = chainKdf rcCKr
msg <- decryptMessage (MessageKey mk iv) encMsg
```
 
### HKDF #6: Chain KDF für die erste Nachricht (auf Alice's Seite)
 
```
Input Key Material (IKM): rcCKr (= rcCKr' aus HKDF #4, 32 Bytes)
Salt:   "" (leerer String)
Info:   "SimpleXChainRatchet"
Output: 96 Bytes → Split:
```
 
```
┌──────────────────────────────────────────────────┐
│ HKDF-SHA512(salt="", ikm=rcCKr,                  │
│             info="SimpleXChainRatchet", len=96)   │
├────────────┬────────────┬─────────────────────────┤
│    ck'     │    mk      │    ivs                  │
│ next CK    │ = MsgKey   │ iv1 (16B) + iv2 (16B)   │
└────────────┴────────────┴─────────────────────────┘
```
 
**Da `rcCKr` (Alice) = `rcCKs` (Bob) aus HKDF #2, und `chainKdf` deterministisch ist:**
- Alice's `mk` = Bob's `mk` (identisch!)
- Alice's `iv` = Bob's `iv` (identisch!)
 
### Schritt 6.8: AES-GCM Decrypt (Message Body)
 
```haskell
decryptMessage (MessageKey mk iv) EncRatchetMessage {emHeader, emBody, emAuthTag} =
    tryE $ decryptAEAD mk iv (rcAD <> emHeader) emBody emAuthTag
```
 
**AES-256-GCM Decrypt (Message Body):**
 
```
Key:       mk (32 Bytes) — aus Chain KDF HKDF #6
IV:        iv = iv2 (16 Bytes) — Bytes 80-95 aus HKDF #6
AD:        rcAD || emHeader
           = "Bob_pk1 || Alice_pk1" || smpEncode(EncMessageHeader)
Ciphertext: emBody
AuthTag:   emAuthTag (16 Bytes)
 
→ decryptAEAD: initAEAD @AES256 (16-Byte IV, transformiert in cryptonite)
→ AES.aeadSimpleDecrypt(aead, AD, ciphertext, authTag)
→ unPad (entfernt Padding)
→ Ergebnis: Klartext der AgentConfirmation-Payload
```
 
**Ergebnis:** Der entschlüsselte Klartext wird als `AgentConnInfoReply` geparst.
 
---
 
## 7. Zusammenfassung: Die vollständige HKDF-Kette
 
```
X3DH DH-Secrets:
  dh1 = DH(Alice_pk1, Bob_sk2)           56 Bytes (X448)
  dh2 = DH(Alice_pk2, Bob_sk1)           56 Bytes (X448)
  dh3 = DH(Alice_pk2, Bob_sk2)           56 Bytes (X448)
  [pq  = KEM Shared Secret]              optional
 
                    ┌─────────────────┐
                    │     HKDF #1     │
                    │  "SimpleXX3DH"  │
                    │ salt=zeros(64)  │
                    │ ikm=dh1||dh2||  │
                    │     dh3[||pq]   │
                    └────┬───┬───┬────┘
                         │   │   │
                    hk   nhk  sk
                    (32B)(32B)(32B)
                    │     │    │
                    │     │    └──────────────────────┐
                    │     │                           │
            ┌───────┘     └──────┐                    │
            │                    │                    │
  Bob:rcHKs=hk          Bob:rcNHKr=nhk         Root Key = sk
  Alice:rcNHKr=hk       Alice:rcNHKs=nhk              │
            │                                         │
  (Header encrypt/decrypt                             │
   für erste Nachricht)                               │
                                                      │
                    ┌─────────────────────────────┐   │
                    │          HKDF #2 / #4       │   │
                    │     "SimpleXRootRatchet"     │   │
                    │ salt=sk                      │◄──┘
                    │ ikm=DH(Alice_pk2,            │
                    │        Bob_ratchet_priv)     │
                    └────┬───────┬────────┬────────┘
                         │       │        │
                       rk'    rcCKs    nhk'
                       (32B)  (32B)    (32B)
                         │       │        │
                         │       │    next header key
                         │       │    (for future ratchet)
                         │       │
                         │   ┌───┘
                         │   │
                         │   │  ┌─────────────────────────────┐
                         │   │  │          HKDF #3 / #6       │
                         │   └──│     "SimpleXChainRatchet"    │
                         │      │ salt=""                      │
                         │      │ ikm=rcCKs                   │
                         │      └────┬───────┬────────┬────────┘
                         │           │       │        │
                         │         ck'      mk      ivs
                         │         (32B)   (32B)  (16B+16B)
                         │                  │     iv1   iv2
                         │                  │      │     │
                         │                  │      │     │
                         │                  │      │     │
                    ┌────┘                  │      │     │
                    │                       │      │     │
               HKDF #5                      │      │     │
          (zweiter rootKdf                  │      │     │
           für Alice's Sende-Chain)         │      │     │
                                            │      │     │
                              ┌─────────────┘      │     │
                              │                    │     │
                    AES-256-GCM(mk, iv2,      AES-256-GCM(hk, iv1,
                      AD=rcAD||emHeader,        AD=rcAD,
                      emBody)                   ehBody)
                         │                        │
                         ▼                        ▼
                   Klartext-Body            Klartext-MsgHeader
                   (AgentConnInfo)          {msgDHRs, msgPN, msgNs}
```
 
---
 
## 8. Schlüsselübersicht für die erste Nachricht
 
| Schlüssel | Herkunft | Verwendung |
|-----------|----------|------------|
| `hk` (32B) | HKDF#1 Bytes 0-31 | Header Encrypt Key (Bob sendet) / Header Decrypt Key (Alice empfängt) |
| `nhk` (32B) | HKDF#1 Bytes 32-63 | Nicht für erste Nachricht verwendet — für zukünftigen Ratchet-Schritt |
| `sk` (32B) | HKDF#1 Bytes 64-95 | Root Key Input für HKDF#2/#4 |
| `rcCKs`/`rcCKr` (32B) | HKDF#2/#4 Bytes 32-63 | Chain Key — Input für HKDF#3/#6 |
| `mk` (32B) | HKDF#3/#6 Bytes 32-63 | AES-256 Message Key |
| `iv1` (16B) | HKDF#3/#6 Bytes 64-79 | Header Encryption IV |
| `iv2` (16B) | HKDF#3/#6 Bytes 80-95 | Message Body Encryption IV |
| `rcAD` | `Bob_pk1 \|\| Alice_pk1` | AES-GCM Associated Data |
 
---
 
## 9. rcAD: Associated Data im Detail
 
**Konstruktion** (`pqX3dh`, `Ratchet.hs:503`):
```haskell
assocData = Str $ pubKeyBytes sk1 <> pubKeyBytes rk1
```
 
- `sk1` = Joiner's erster Public Key (Bob_pk1)
- `rk1` = Creator's erster Public Key (Alice_pk1)
- Beide Seiten verwenden dieselbe Reihenfolge (Joiner zuerst, Creator zweiter)
 
**Verwendung bei Header-Decrypt:**
```
AD = rcAD = "Bob_pk1 || Alice_pk1"
```
 
**Verwendung bei Message-Body-Decrypt:**
```
AD = rcAD || emHeader = "Bob_pk1 || Alice_pk1" || smpEncode(EncMessageHeader)
```
 
Das `emHeader` im AD des Body-Decrypts bindet den verschlüsselten Header kryptographisch an den Body — ein Angreifer kann den Header nicht austauschen, ohne dass die Body-Entschlüsselung fehlschlägt.
 
---
 
## 10. AES-GCM IV-Handling
 
**Funktion `initAEAD`** — `Crypto.hs:1115-1120`:
```haskell
initAEAD :: forall c. AES.BlockCipher c => Key -> IV -> ExceptT CryptoError IO (AES.AEAD c)
initAEAD (Key aesKey) (IV ivBytes) = do
    iv <- makeIV @c ivBytes            -- 16 Bytes → AES.IV AES256
    cryptoFailable $ do
        cipher <- AES.cipherInit aesKey  -- AES-256 Key Init
        AES.aeadInit AES.AEAD_GCM cipher iv
```
 
- **IV-Länge: 16 Bytes** (nicht die Standard-12-Byte GCM IV!)
- Die 16-Byte IV wird intern von cryptonite in `cryptonite_aes_gcm_init` transformiert
- Dies ist ein Implementierungsdetail — die IV wird über den AES-BlockCipher-Mechanismus konvertiert
- Kommentar im Code: "To make it compatible with WebCrypto we will need to start using initAEADGCM" (12-Byte IV)
 
---
 
## 11. Relevante Quelldateien
 
| Datei | Zeilen | Funktion |
|-------|--------|----------|
| `Ratchet.hs` | 499-508 | `pqX3dh` — X3DH HKDF #1 |
| `Ratchet.hs` | 483-497 | `pqX3dhRcv` — Alice's X3DH |
| `Ratchet.hs` | 467-480 | `pqX3dhSnd` — Bob's X3DH |
| `Ratchet.hs` | 674-699 | `initRcvRatchet` — Alice's Ratchet-Init |
| `Ratchet.hs` | 643-666 | `initSndRatchet` — Bob's Ratchet-Init |
| `Ratchet.hs` | 990-1157 | `rcDecrypt` — Vollständiger Decrypt-Pfad |
| `Ratchet.hs` | 1043-1071 | `ratchetStep` — DH-Ratchet-Schritt |
| `Ratchet.hs` | 1159-1166 | `rootKdf` — Root KDF |
| `Ratchet.hs` | 1168-1172 | `chainKdf` — Chain KDF |
| `Ratchet.hs` | 1174-1179 | `hkdf3` — HKDF Wrapper (96 Bytes → 3×32) |
| `Ratchet.hs` | 703-740 | `MsgHeader` Datentyp + Encoding |
| `Ratchet.hs` | 742-756 | `EncMessageHeader` Datentyp |
| `Ratchet.hs` | 773-787 | `EncRatchetMessage` Datentyp |
| `Crypto.hs` | 1035-1038 | `decryptAEAD` — AES-256-GCM Decrypt |
| `Crypto.hs` | 1115-1120 | `initAEAD` — AEAD Initialisierung |
| `Crypto.hs` | 1443-1447 | `hkdf` — HKDF-SHA512 Wrapper |
| `Agent.hs` | 2901-2945 | `smpConfirmation` — Alice empfängt AgentConfirmation |
| `Agent.hs` | 1070-1108 | `startJoinInvitation` — Bob sendet Confirmation |