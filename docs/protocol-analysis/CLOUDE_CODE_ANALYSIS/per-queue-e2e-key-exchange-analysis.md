# Per-Queue E2E Key Exchange im SimpleX Protokoll
 
## Vollstaendige Analyse mit Code-Referenzen
 
---
 
## Inhaltsverzeichnis
 
1. [Architektur-Ueberblick: Zwei Schichten der E2E-Verschluesselung](#1-architektur-ueberblick)
2. [E2E Key Exchange Flow](#2-e2e-key-exchange-flow)
3. [Das "maybe_e2e"-Flag (PubHeader)](#3-das-maybe_e2e-flag-pubheader)
4. [ClientMsgEnvelope Wire-Format](#4-clientmsgenvelope-wire-format)
5. [Key Derivation: X3DH und Double Ratchet](#5-key-derivation-x3dh-und-double-ratchet)
6. [Separate Keys pro Queue vs. Shared Keys](#6-separate-keys-pro-queue)
 
---
 
## 1. Architektur-Ueberblick
 
SimpleX verwendet **zwei voneinander unabhaengige E2E-Verschluesselungsschichten**:
 
### Schicht 1: Per-Queue E2E (X25519 DH + NaCl crypto_box)
- **Zweck**: Verschluesselung aller Nachrichten auf einer SMP-Queue
- **Algorithmus**: X25519 DH -> XSalsa20-Poly1305 (NaCl crypto_box)
- **Key**: `e2eDhSecret` (ein `DhSecretX25519`, gespeichert pro Queue)
- **Lebensdauer**: Pro Queue, aendert sich nicht
 
### Schicht 2: Double Ratchet E2E (X448 + optional SNTRUP761 KEM)
- **Zweck**: Forward Secrecy und Post-Compromise Security fuer Nachrichteninhalte
- **Algorithmus**: X3DH Key Agreement -> Double Ratchet mit X448
- **Key**: Ratchet Keys (werden staendig rotiert)
- **Lebensdauer**: Pro Connection, staendig erneuert
 
**Die Queue-E2E (Schicht 1) verschluesselt die gesamte `ClientMsgEnvelope`, die wiederum
die Double-Ratchet-verschluesselten Nachrichteninhalte enthaelt.**
 
```
Transport-TLS
  -> Server-Auth (rcvDhSecret fuer Server->Recipient)
    -> Per-Queue E2E (e2eDhSecret, NaCl crypto_box)  <-- DIESE ANALYSE
      -> Double Ratchet E2E (X448/SNTRUP761)
        -> Nachrichteninhalt
```
 
---
 
## 2. E2E Key Exchange Flow
 
### 2.1 Wie wird `e2eDhSecret` berechnet?
 
`e2eDhSecret` ist ein X25519 Diffie-Hellman Shared Secret, berechnet mit:
 
```haskell
-- src/Simplex/Messaging/Crypto.hs:1262-1264
dh' :: DhAlgorithm a => PublicKey a -> PrivateKey a -> DhSecret a
dh' (PublicKeyX25519 k) (PrivateKeyX25519 pk _) = DhSecretX25519 $ X25519.dh k pk
dh' (PublicKeyX448 k) (PrivateKeyX448 pk _) = DhSecretX448 $ X448.dh k pk
```
 
Der Typ `DhSecret` ist ein Wrapper um das rohe DH-Ergebnis:
 
```haskell
-- src/Simplex/Messaging/Crypto.hs:416-418
data DhSecret (a :: Algorithm) where
  DhSecretX25519 :: X25519.DhSecret -> DhSecret X25519
  DhSecretX448 :: X448.DhSecret -> DhSecret X448
```
 
### 2.2 Invitation Queue (Initiator = "Bob"): Schritt fuer Schritt
 
#### Phase 1: Recipient erstellt Queue und generiert E2E-Keypair
 
```haskell
-- src/Simplex/Messaging/Agent.hs:947
e2eKeys <- atomically . C.generateKeyPair =<< asks random
```
 
Dieses Keypair wird an `newRcvQueue_` uebergeben:
 
```haskell
-- src/Simplex/Messaging/Agent/Client.hs:1379
newRcvQueue_ c nm userId connId ... (e2eDhKey, e2ePrivKey) = do
```
 
Die `RcvQueue` wird erstellt mit:
 
```haskell
-- src/Simplex/Messaging/Agent/Client.hs:1393-1402
let rq = RcvQueue
      { ...
        e2ePrivKey,           -- privater X25519-Key, gespeichert
        e2eDhSecret = Nothing, -- noch KEIN Shared Secret!
        ...
      }
```
 
Der **oeffentliche** Teil des E2E-Keys wird in die `SMPQueueUri` eingebettet:
 
```haskell
-- src/Simplex/Messaging/Agent/Client.hs:1416
qUri = SMPQueueUri vRange $ SMPQueueAddress srv sndId e2eDhKey queueMode
```
 
Die Queue-Adresse enthaelt also `dhPublicKey`:
 
```haskell
-- src/Simplex/Messaging/Agent/Protocol.hs:1310-1316
data SMPQueueAddress = SMPQueueAddress
  { smpServer :: SMPServer,
    senderId :: SMP.SenderId,
    dhPublicKey :: C.PublicKeyX25519,  -- <-- E2E Public Key des Empfaengers
    queueMode :: Maybe QueueMode
  }
```
 
Dies wird in der Connection-Request-URI weitergegeben:
 
```
smp://server#fingerprint/senderID?v=...&dh=<base64-e2ePubKey>&mode=...
```
 
#### Phase 2: Sender empfaengt URI und erstellt SndQueue
 
Wenn der Sender die Einladung annimmt (`joinConn`), wird `newSndQueue` aufgerufen:
 
```haskell
-- src/Simplex/Messaging/Agent.hs:3366-3391
newSndQueue userId connId
  (Compatible (SMPQueueInfo smpClientVersion
    SMPQueueAddress {smpServer, senderId, queueMode,
      dhPublicKey = rcvE2ePubDhKey}))  -- Public Key des Empfaengers
  sndKeys_ = do
  ...
  (e2ePubKey, e2ePrivKey) <- atomically $ C.generateKeyPair g  -- Neues Keypair!
  let sq = SndQueue
        { ...
          e2eDhSecret = C.dh' rcvE2ePubDhKey e2ePrivKey,
          -- ^ DH(empfaenger_pub, eigener_priv) = Shared Secret
          e2ePubKey = Just e2ePubKey,
          -- ^ eigener Public Key, wird mit Confirmation gesendet
          ...
        }
```
 
**Wichtig**: Der Sender berechnet `e2eDhSecret` SOFORT bei Queue-Erstellung,
weil er den Public Key des Empfaengers bereits aus der URI kennt.
 
#### Phase 3: Sender sendet Confirmation mit eigenem Public Key
 
```haskell
-- src/Simplex/Messaging/Agent/Client.hs:1646-1651
sendConfirmation c nm sq@SndQueue {... e2ePubKey = e2ePubKey@Just {} ...} agentConfirmation = do
  let (privHdr, spKey) = ...
      clientMsg = SMP.ClientMessage privHdr agentConfirmation
  msg <- agentCbEncrypt sq e2ePubKey $ smpEncode clientMsg
  -- e2ePubKey wird im PubHeader mitgesendet
```
 
In `agentCbEncrypt`:
 
```haskell
-- src/Simplex/Messaging/Agent/Client.hs:1925-1933
agentCbEncrypt SndQueue {e2eDhSecret, smpClientVersion} e2ePubKey msg = do
  cmNonce <- atomically . C.randomCbNonce =<< asks random
  let paddedLen = maybe SMP.e2eEncMessageLength (const SMP.e2eEncConfirmationLength) e2ePubKey
  cmEncBody <- liftEither . first cryptoError $
    C.cbEncrypt e2eDhSecret cmNonce msg paddedLen
  let cmHeader = SMP.PubHeader smpClientVersion e2ePubKey  -- Key im Header!
  pure $ smpEncode SMP.ClientMsgEnvelope {cmHeader, cmNonce, cmEncBody}
```
 
#### Phase 4: Empfaenger empfaengt Confirmation und berechnet DH Secret
 
```haskell
-- src/Simplex/Messaging/Agent.hs:2706-2715
clientMsg@SMP.ClientMsgEnvelope {cmHeader = SMP.PubHeader phVer e2ePubKey_} <-
  parseMessage msgBody
...
case (e2eDhSecret, e2ePubKey_) of
  (Nothing, Just e2ePubKey) -> do
    -- Erster Nachrichtenempfang: kein gespeichertes Secret,
    -- aber DH-Key im Header -> Berechne Secret
    let e2eDh = C.dh' e2ePubKey e2ePrivKey
    -- DH(sender_pub, eigener_priv) = gleiches Shared Secret!
    decryptClientMessage e2eDh clientMsg >>= \case
      (SMP.PHConfirmation senderKey, AgentConfirmation {...}) ->
        smpConfirmation ...
```
 
Danach wird der Secret persistent gespeichert:
 
```haskell
-- src/Simplex/Messaging/Agent.hs:2946-2949
let dhSecret = C.dh' e2ePubKey' e2ePrivKey'
setRcvQueueConfirmedE2E db rq dhSecret $ min v v'
```
 
```haskell
-- src/Simplex/Messaging/Agent/Store/AgentStore.hs:546-556
setRcvQueueConfirmedE2E db RcvQueue {rcvId, server = ProtocolServer {host, port}} e2eDhSecret smpClientVersion =
  DB.execute db ... (e2eDhSecret, Confirmed, smpClientVersion, host, port, rcvId)
  -- SET e2e_dh_secret = ?, status = ?, smp_client_version = ?
```
 
#### Phase 5: Nachfolgende Nachrichten
 
```haskell
-- src/Simplex/Messaging/Agent.hs:2722-2723
case (e2eDhSecret, e2ePubKey_) of
  (Just e2eDh, Nothing) -> do
    -- Gespeichertes Secret vorhanden, kein Key im Header
    -- -> Regulaere Nachricht
    decryptClientMessage e2eDh clientMsg >>= \case
      (SMP.PHEmpty, AgentMsgEnvelope {agentVersion, encAgentMessage}) -> ...
```
 
### 2.3 Contact Queue vs. Reply Queue
 
#### Contact Queue (Contact Address)
 
Bei einer Contact Queue (Dauerhafte Adresse fuer mehrere Verbindungsanfragen):
 
1. Der Recipient erstellt eine Queue mit `QMContact`
2. Mehrere Sender koennen an diese Queue senden
3. Jeder Sender verwendet `agentCbEncryptOnce` fuer einmalige Verschluesselung:
 
```haskell
-- src/Simplex/Messaging/Agent/Client.hs:1936-1946
agentCbEncryptOnce :: VersionSMPC -> C.PublicKeyX25519 -> ByteString -> AM ByteString
agentCbEncryptOnce clientVersion dhRcvPubKey msg = do
  g <- asks random
  (dhSndPubKey, dhSndPrivKey) <- atomically $ C.generateKeyPair g
  -- Ephemeres Keypair fuer diese eine Nachricht
  let e2eDhSecret = C.dh' dhRcvPubKey dhSndPrivKey
  cmNonce <- atomically $ C.randomCbNonce g
  cmEncBody <- liftEither . first cryptoError $
    C.cbEncrypt e2eDhSecret cmNonce msg SMP.e2eEncConfirmationLength
  let cmHeader = SMP.PubHeader clientVersion (Just dhSndPubKey)
  -- Public Key MUSS im Header sein (einmalige Verschluesselung)
  pure $ smpEncode SMP.ClientMsgEnvelope {cmHeader, cmNonce, cmEncBody}
```
 
**Verwendung**: Invitation-Nachrichten an Contact Queues:
 
```haskell
-- src/Simplex/Messaging/Agent/Client.hs:1654-1664
sendInvitation c nm userId connId (Compatible (SMPQueueInfo v SMPQueueAddress {..., dhPublicKey})) ... = do
  msg <- mkInvitation
  ...
  where
    mkInvitation = do
      let agentEnvelope = AgentInvitation {agentVersion, connReq, connInfo}
      agentCbEncryptOnce v dhPublicKey . smpEncode $
        SMP.ClientMessage SMP.PHEmpty (smpEncode agentEnvelope)
```
 
#### Reply Queue (Invitation Queue)
 
Bei einer Invitation Queue (1:1 Verbindung):
 
1. Der Sender berechnet `e2eDhSecret` einmalig bei `newSndQueue` und speichert es
2. Der Public Key wird nur in der **ersten** Nachricht (Confirmation) mitgesendet
3. Danach wird der gespeicherte Secret fuer alle Nachrichten verwendet
 
**Kernunterschied**:
| Aspekt | Contact Queue | Reply Queue |
|--------|--------------|-------------|
| Sender-Key | Ephemer (jede Nachricht neu) | Persistent (einmalig generiert) |
| DH-Key im Header | Immer `Just` | Nur bei Confirmation, dann `Nothing` |
| e2eDhSecret auf Sender | Nicht gespeichert | In SndQueue gespeichert |
| e2eDhSecret auf Empfaenger | Jedes Mal neu berechnet | Nach Confirmation gespeichert |
| Padding | `e2eEncConfirmationLength` (15904) | Confirmation: 15904, Messages: 16000 |
 
---
 
## 3. Das "maybe_e2e"-Flag (PubHeader)
 
### 3.1 Was das Flag bedeutet
 
Das `PubHeader` enthaelt ein `Maybe C.PublicKeyX25519` Feld:
 
```haskell
-- src/Simplex/Messaging/Protocol.hs:1074-1078
data PubHeader = PubHeader
  { phVersion :: VersionSMPC,
    phE2ePubDhKey :: Maybe C.PublicKeyX25519
  }
```
 
### 3.2 Wire-Encoding: '1' vs '0'
 
Die `Maybe`-Serialisierung ist in der `Encoding`-Instanz definiert:
 
```haskell
-- src/Simplex/Messaging/Encoding.hs:114-122
instance Encoding a => Encoding (Maybe a) where
  smpEncode = maybe "0" (('1' `B.cons`) . smpEncode)
  smpP =
    smpP >>= \case
      '0' -> pure Nothing       -- Kein DH Key (regulaere Nachricht)
      '1' -> Just <$> smpP      -- DH Key folgt (Confirmation)
      _ -> fail "invalid Maybe tag"
```
 
Da `PubHeader` als Tupel kodiert wird:
 
```haskell
-- src/Simplex/Messaging/Protocol.hs:1080-1082
instance Encoding PubHeader where
  smpEncode (PubHeader v k) = smpEncode (v, k)
  smpP = PubHeader <$> smpP <*> smpP
```
 
Ergibt sich:
 
| Wire-Format | Bedeutung | Beschreibung |
|---|---|---|
| `<version:2><'1'><key_len:1><key_bytes>` | `Just pubKey` | Confirmation: Sender uebertraegt DH-Key |
| `<version:2><'0'>` | `Nothing` | Regulaere Nachricht: DH-Key bereits bekannt |
 
### 3.3 Aus dem Protokoll-Dokument
 
```abnf
-- protocol/simplex-messaging.md:731-736
smpEncClientMessage = smpPubHeaderNoKey msgNonce sentClientMsgBody
smpPubHeaderNoKey = smpClientVersion "0"
 
smpEncConfirmation = smpPubHeaderWithKey msgNonce sentConfirmationBody
smpPubHeaderWithKey = smpClientVersion "1" senderPublicDhKey
```
 
### 3.4 Wo wird das Flag gesetzt?
 
**Beim Senden (Schreiben):**
 
1. **Confirmation mit Key** (`agentCbEncrypt` mit `Just pubKey`):
```haskell
-- src/Simplex/Messaging/Agent/Client.hs:1926-1932
agentCbEncrypt SndQueue {e2eDhSecret, smpClientVersion} e2ePubKey msg = do
  ...
  let cmHeader = SMP.PubHeader smpClientVersion e2ePubKey
  -- e2ePubKey = Just key -> "1" + key bytes
  -- e2ePubKey = Nothing  -> "0"
```
 
2. **Einmal-Verschluesselung** (immer mit Key):
```haskell
-- src/Simplex/Messaging/Agent/Client.hs:1945
let cmHeader = SMP.PubHeader clientVersion (Just dhSndPubKey)
-- Immer "1" + key
```
 
3. **Regulaere Nachricht senden** (`enqueueMessage` verwendet `agentCbEncrypt` mit
   `e2ePubKey = Nothing` aus der SndQueue, nachdem der Key einmal gesendet wurde)
 
**Beim Empfangen (Lesen):**
 
```haskell
-- src/Simplex/Messaging/Agent.hs:2706-2722
clientMsg@SMP.ClientMsgEnvelope {cmHeader = SMP.PubHeader phVer e2ePubKey_} <-
  parseMessage msgBody
...
case (e2eDhSecret, e2ePubKey_) of
  (Nothing, Just e2ePubKey) -> ...  -- Erste Nachricht: Berechne Secret
  (Just e2eDh, Nothing) -> ...      -- Regulaer: Verwende gespeichertes Secret
```
 
### 3.5 Welcher Key wird bei '0' (Nothing) verwendet?
 
Wenn `e2ePubKey_ = Nothing` (d.h. '0' im Header):
- Wird der **zuvor gespeicherte `e2eDhSecret`** aus der RcvQueue verwendet
- Dieser wurde bei Empfang der Confirmation berechnet und in der DB gespeichert
- Siehe `setRcvQueueConfirmedE2E` in `src/Simplex/Messaging/Agent/Store/AgentStore.hs:546`
 
---
 
## 4. ClientMsgEnvelope Wire-Format
 
### 4.1 Datenstruktur
 
```haskell
-- src/Simplex/Messaging/Protocol.hs:1067-1072
data ClientMsgEnvelope = ClientMsgEnvelope
  { cmHeader :: PubHeader,
    cmNonce :: C.CbNonce,
    cmEncBody :: ByteString
  }
```
 
### 4.2 Encoding
 
```haskell
-- src/Simplex/Messaging/Protocol.hs:1084-1089
instance Encoding ClientMsgEnvelope where
  smpEncode ClientMsgEnvelope {cmHeader, cmNonce, cmEncBody} =
    smpEncode (cmHeader, cmNonce, Tail cmEncBody)
  smpP = do
    (cmHeader, cmNonce, Tail cmEncBody) <- smpP
    pure ClientMsgEnvelope {cmHeader, cmNonce, cmEncBody}
```
 
### 4.3 Exaktes Wire-Format mit Byte-Offsets
 
#### Fall A: Regulaere Nachricht (kein DH Key)
 
```
Offset | Laenge | Feld              | Beschreibung
-------|--------|-------------------|------------------------------------------
  0    |   2    | phVersion         | SMP Client Version (Word16, Big Endian)
  2    |   1    | maybe_e2e = '0'   | Kein DH Key (0x30 = ASCII '0')
  3    |  24    | cmNonce           | Random 192-bit Nonce fuer NaCl crypto_box
 27    | 16016  | cmEncBody         | Verschluesselter + gepaddeter Body
       |        |                   |   = 16 bytes Poly1305 Auth Tag
       |        |                   |   + verschluesselter padded(smpClientMessage, 16000)
-------|--------|-------------------|------------------------------------------
Total  | 16043  |                   | = 3 + 24 + 16016
```
 
Padded Body-Laenge = `e2eEncMessageLength`:
```haskell
-- src/Simplex/Messaging/Protocol.hs:315-316
e2eEncMessageLength :: Int
e2eEncMessageLength = 16000
```
 
Die verschluesselte Laenge ist 16000 + 16 (Poly1305 Tag) = 16016 Bytes.
 
#### Fall B: Confirmation (mit DH Key)
 
```
Offset | Laenge | Feld              | Beschreibung
-------|--------|-------------------|------------------------------------------
  0    |   2    | phVersion         | SMP Client Version (Word16, Big Endian)
  2    |   1    | maybe_e2e = '1'   | DH Key folgt (0x31 = ASCII '1')
  3    |   1    | key_len           | Laenge des DH Keys (typisch: 32 = 0x20)
  4    |  32    | senderPublicDhKey | X25519 Public Key (X.509 DER encoded)
 36    |  24    | cmNonce           | Random 192-bit Nonce
 60    | 15920  | cmEncBody         | Verschluesselter + gepaddeter Body
       |        |                   |   = 16 bytes Poly1305 Auth Tag
       |        |                   |   + verschluesselter padded(smpConfirmation, 15904)
-------|--------|-------------------|------------------------------------------
Total  | 15980  |                   | = 36 + 24 + 15920
```
 
Padded Confirmation-Laenge = `e2eEncConfirmationLength`:
```haskell
-- src/Simplex/Messaging/Protocol.hs:312-313
e2eEncConfirmationLength :: Int
e2eEncConfirmationLength = 15904
```
 
**Hinweis**: Der Key wird mit SMP `Encoding ByteString` kodiert (1-Byte Laengen-Prefix).
X25519 Public Keys sind immer 32 Bytes, also ist key_len = 0x20.
 
### 4.4 Entschluesselter Body: ClientMessage
 
```haskell
-- src/Simplex/Messaging/Protocol.hs:1091
data ClientMessage = ClientMessage PrivHeader ByteString
 
-- src/Simplex/Messaging/Protocol.hs:1093-1096
data PrivHeader
  = PHConfirmation C.APublicAuthKey  -- 'K' + sender auth key
  | PHEmpty                          -- '_'
```
 
Encoding:
```haskell
-- src/Simplex/Messaging/Protocol.hs:1098-1110
instance Encoding PrivHeader where
  smpEncode = \case
    PHConfirmation k -> "K" <> smpEncode k  -- 'K' + key
    PHEmpty -> "_"                           -- '_'
 
instance Encoding ClientMessage where
  smpEncode (ClientMessage h msg) = smpEncode h <> msg
  smpP = ClientMessage <$> smpP <*> A.takeByteString
```
 
Nach Entschluesselung und Unpadding:
```
Offset | Laenge | Feld              | Beschreibung
-------|--------|-------------------|------------------------------------------
  0    |   2    | originalLength    | Laenge des Originals vor Padding (Word16)
  2    |  1+    | privHeader        | '_' (leer) oder 'K' + auth key
  3+   | Rest   | clientMsgBody     | AgentMsgEnvelope (smpEncode)
```
 
### 4.5 Verschluesselung (NaCl crypto_box)
 
Die E2E-Verschluesselung verwendet NaCl crypto_box (XSalsa20-Poly1305):
 
```haskell
-- src/Simplex/Messaging/Crypto.hs:1268-1269
cbEncrypt :: DhSecret X25519 -> CbNonce -> ByteString -> Int -> Either CryptoError ByteString
cbEncrypt (DhSecretX25519 secret) = sbEncrypt_ secret
 
-- src/Simplex/Messaging/Crypto.hs:1282-1283
sbEncrypt_ secret (CbNonce nonce) msg paddedLen = cryptoBox secret nonce <$> pad msg paddedLen
 
-- src/Simplex/Messaging/Crypto.hs:1295-1299
cryptoBox :: ByteArrayAccess key => key -> ByteString -> ByteString -> ByteString
cryptoBox secret nonce s = BA.convert tag <> c
  where
    (rs, c) = xSalsa20 secret nonce s      -- XSalsa20 Stream Cipher
    tag = Poly1305.auth rs c                -- Poly1305 MAC
 
-- src/Simplex/Messaging/Crypto.hs:1449-1457
xSalsa20 :: ByteArrayAccess key => key -> ByteString -> ByteString -> (ByteString, ByteString)
xSalsa20 secret nonce msg = (rs, msg')
  where
    zero = B.replicate 16 $ toEnum 0
    (iv0, iv1) = B.splitAt 8 nonce          -- 24-byte nonce split: 8+16
    state0 = XSalsa.initialize 20 secret (zero `B.append` iv0)
    state1 = XSalsa.derive state0 iv1
    (rs, state2) = XSalsa.generate state1 32 -- 32 bytes fuer Poly1305 key
    (msg', _) = XSalsa.combine state2 msg    -- XOR-Verschluesselung
```
 
**Entschluesselung**:
```haskell
-- src/Simplex/Messaging/Crypto.hs:1302-1303
cbDecrypt :: DhSecret X25519 -> CbNonce -> ByteString -> Either CryptoError ByteString
cbDecrypt (DhSecretX25519 secret) = sbDecrypt_ secret
 
-- src/Simplex/Messaging/Crypto.hs:1317-1318
sbDecrypt_ secret nonce = unPad <=< sbDecryptNoPad_ secret nonce
 
-- src/Simplex/Messaging/Crypto.hs:1326-1334
sbDecryptNoPad_ secret (CbNonce nonce) packet
  | B.length packet < 16 = Left CBDecryptError
  | BA.constEq tag' tag = Right msg     -- Constant-time Tag-Vergleich
  | otherwise = Left CBDecryptError
  where
    (tag', c) = B.splitAt 16 packet     -- Erste 16 Bytes = Auth Tag
    (rs, msg) = xSalsa20 secret nonce c
    tag = Poly1305.auth rs c            -- Neuberechnung des Tags
```
 
---
 
## 5. Key Derivation: X3DH und Double Ratchet
 
### 5.1 Per-Queue E2E Key (e2eDhSecret) - KEIN X3DH
 
Der per-Queue E2E Key (`e2eDhSecret`) wird **NICHT** aus X3DH abgeleitet.
Er ist ein einfaches X25519 DH Shared Secret:
 
```
e2eDhSecret = DH(peer_pub_X25519, own_priv_X25519)
```
 
Dieser Key wird **direkt als NaCl crypto_box Secret** verwendet (kein KDF).
 
### 5.2 Double Ratchet Key (Ratchet) - Verwendet X3DH
 
Der Double Ratchet (Schicht 2) verwendet ein modifiziertes X3DH fuer die Initialisierung,
aber mit **X448** (nicht X25519):
 
```haskell
-- src/Simplex/Messaging/Crypto/Ratchet.hs:499-508
pqX3dh :: DhAlgorithm a =>
  (PublicKey a, PublicKey a)   -- (sender_k1, receiver_k1) fuer assocData
  -> DhSecret a               -- dh1 = DH(rk1, spk2) oder DH(sk2, rpk1)
  -> DhSecret a               -- dh2 = DH(rk2, spk1) oder DH(sk1, rpk2)
  -> DhSecret a               -- dh3 = DH(rk2, spk2) oder DH(sk2, rpk2)
  -> Maybe RatchetKEMAccepted  -- PQ KEM Shared Secret (optional)
  -> RatchetInitParams
pqX3dh (sk1, rk1) dh1 dh2 dh3 kemAccepted =
  RatchetInitParams {assocData, ratchetKey = RatchetKey sk, sndHK = Key hk, rcvNextHK = Key nhk, kemAccepted}
  where
    assocData = Str $ pubKeyBytes sk1 <> pubKeyBytes rk1
    dhs = dhBytes' dh1 <> dhBytes' dh2 <> dhBytes' dh3 <> pq
    pq = maybe "" (\RatchetKEMAccepted {rcPQRss = KEMSharedKey ss} -> BA.convert ss) kemAccepted
    (hk, nhk, sk) =
      let salt = B.replicate 64 '\0'
       in hkdf3 salt dhs "SimpleXX3DH"
```
 
Die X3DH-Initialisierung produziert:
- `ratchetKey` (32 Bytes): Initialer Root Key fuer den Double Ratchet
- `sndHK` (32 Bytes): Header Key fuer Senden
- `rcvNextHK` (32 Bytes): Naechster Header Key fuer Empfangen
 
Die HKDF-Funktion:
```haskell
-- src/Simplex/Messaging/Crypto/Ratchet.hs:1174-1179
hkdf3 :: ByteString -> ByteString -> ByteString -> (ByteString, ByteString, ByteString)
hkdf3 salt ikm info = (s1, s2, s3)
  where
    out = hkdf salt ikm info 96    -- 96 Bytes Output
    (s1, rest) = B.splitAt 32 out  -- 3 x 32 Bytes
    (s2, s3) = B.splitAt 32 rest
```
 
### 5.3 Sender-Seite (pqX3dhSnd): "Alice" in der Spec
 
```haskell
-- src/Simplex/Messaging/Crypto/Ratchet.hs:467-480
pqX3dhSnd spk1 spk2 spKem_ (E2ERatchetParams v rk1 rk2 rKem_) = do
  (ks_, kem_) <- sndPq
  let initParams = pqX3dh
        (publicKey spk1, rk1)      -- Public Keys fuer assocData
        (dh' rk1 spk2)            -- dh1
        (dh' rk2 spk1)            -- dh2
        (dh' rk2 spk2)            -- dh3
        kem_
  pure (initParams, ks_)
```
 
### 5.4 Empfaenger-Seite (pqX3dhRcv): "Bob" in der Spec
 
```haskell
-- src/Simplex/Messaging/Crypto/Ratchet.hs:483-497
pqX3dhRcv rpk1 rpk2 rpKem_ (E2ERatchetParams v sk1 sk2 sKem_) = do
  kem_ <- rcvPq
  let initParams = pqX3dh
        (sk1, publicKey rpk1)      -- Public Keys fuer assocData
        (dh' sk2 rpk1)            -- dh1  (gleich wie sender dh1)
        (dh' sk1 rpk2)            -- dh2  (gleich wie sender dh2)
        (dh' sk2 rpk2)            -- dh3  (gleich wie sender dh3)
        (snd <$> kem_)
  pure (initParams, fst <$> kem_)
```
 
### 5.5 Zusammenfassung der Key-Arten
 
| Key | Algorithmus | Verwendung | Ableitung |
|-----|-----------|------------|-----------|
| `e2eDhSecret` | X25519 DH | Per-Queue crypto_box | Direkt: `DH(pub, priv)` |
| `rcvDhSecret` | X25519 DH | Server->Client Entschluesselung | Direkt: `DH(pub, priv)` |
| Ratchet Root Key | X448 + HKDF | Double Ratchet Init | X3DH: `HKDF(dh1||dh2||dh3[||pq])` |
| Chain Keys | HKDF | Nachrichten-Verschluesselung | Root KDF: `HKDF(RK, DH)` |
| Message Keys | HKDF | AES-256-GCM fuer Inhalt | Chain KDF: `HKDF(CK)` |
 
---
 
## 6. Separate Keys pro Queue
 
### 6.1 Per-Queue E2E Key: SEPARATE Keys pro Queue
 
Jede SMP-Queue hat ihren **eigenen, unabhaengigen** `e2eDhSecret`:
 
**RcvQueue** (Empfangs-Queue):
```haskell
-- src/Simplex/Messaging/Agent/Store.hs:68-81
data StoredRcvQueue (q :: DBStored) = RcvQueue
  { ...
    e2ePrivKey :: C.PrivateKeyX25519,           -- Eigener privater DH Key
    e2eDhSecret :: Maybe C.DhSecretX25519,      -- Shared Secret (Maybe, weil erst nach Confirmation)
    ...
  }
```
 
**SndQueue** (Sende-Queue):
```haskell
-- src/Simplex/Messaging/Agent/Store.hs:168-183
data StoredSndQueue (q :: DBStored) = SndQueue
  { ...
    e2ePubKey :: Maybe C.PublicKeyX25519,        -- Eigener Public Key (Just bei erster Nachricht)
    e2eDhSecret :: C.DhSecretX25519,             -- Shared Secret (immer vorhanden)
    ...
  }
```
 
**Jede Queue generiert ihr eigenes X25519-Keypair** bei der Erstellung:
 
- `newRcvQueue_` (Client.hs:1379): Empfaengt das Keypair als Parameter
- `newSndQueue` (Agent.hs:3371): `(e2ePubKey, e2ePrivKey) <- atomically $ C.generateKeyPair g`
 
### 6.2 Double Ratchet: Ein Ratchet pro Connection
 
Im Gegensatz dazu teilen sich alle Queues einer Connection **denselben** Double Ratchet:
 
```haskell
-- src/Simplex/Messaging/Agent.hs:2922
(agentMsgBody_, rc', skipped) <- liftError cryptoError $ CR.rcDecrypt g rc M.empty encConnInfo
-- `rc` wird pro `connId` geladen, nicht pro Queue
```
 
Der Ratchet wird in der DB pro `connId` gespeichert:
```haskell
-- src/Simplex/Messaging/Agent.hs:2945
createRatchet db connId rc'
-- Nur connId, kein Queue-Bezug
```
 
### 6.3 Warum separate per-Queue Keys?
 
1. **Queue-Rotation (Switching)**: Wenn Queues gewechselt werden (QADD/QKEY/QUSE),
   benoetigt jede neue Queue ein eigenes E2E-Secret
 
2. **Contact Queues**: Bei Contact Addresses koennen mehrere Sender unabhaengig
   verschluesseln, jeder mit einem eigenen ephemeren Key
 
3. **Server-Trennung**: Per-Queue E2E schuetzt vor dem SMP-Server selbst.
   Da verschiedene Queues auf verschiedenen Servern sein koennen,
   braucht jede ihren eigenen Key.
 
4. **Mehrere Queues pro Connection**: Eine DuplexConnection hat `NonEmpty RcvQueue`
   und `NonEmpty SndQueue`. Bei Queue-Switching existieren temporaer mehrere
   aktive Queues gleichzeitig.
 
### 6.4 Datenbank-Schema
 
```sql
-- src/Simplex/Messaging/Agent/Store/SQLite/Migrations/M20220101_initial.hs:38,56
-- rcv_queues:
  e2e_dh_secret BLOB,          -- NULL bis Confirmation empfangen
 
-- snd_queues:
  e2e_dh_secret BLOB NOT NULL, -- Immer vorhanden (sofort bei Erstellung berechnet)
```
 
---
 
## Zusammenfassung: Vollstaendiger E2E Key Exchange Ablauf
 
```
     RECIPIENT (Bob)                              SENDER (Alice)
     ==============                               ==============
 
1. Generiere X25519 Keypair:
   (e2eDhKey, e2ePrivKey)
 
2. Erstelle Queue beim Server
   e2eDhSecret = Nothing (noch kein Peer)
 
3. Teile Queue-URI (out-of-band):
   smp://server/sndId?dh=<e2eDhKey>
                                            4. Parse Queue-URI
                                               Extrahiere e2eDhKey (Recipient Pub)
 
                                            5. Generiere eigenes X25519 Keypair:
                                               (e2ePubKey, e2ePrivKey)
 
                                            6. Berechne Shared Secret:
                                               e2eDhSecret = DH(e2eDhKey, e2ePrivKey)
 
                                            7. Sende Confirmation:
                                               PubHeader = version + "1" + e2ePubKey
                                               Body = crypto_box(e2eDhSecret, nonce, msg)
 
8. Empfange Confirmation:
   Parse PubHeader -> e2ePubKey (Just)
 
9. Berechne Shared Secret:
   e2eDh = DH(e2ePubKey, e2ePrivKey)
   (= gleiches Secret wie Sender!)
 
10. Entschluessele Confirmation
    Speichere e2eDhSecret in DB
                                            11. Sende regulaere Nachrichten:
                                                PubHeader = version + "0" (kein Key)
                                                Body = crypto_box(e2eDhSecret, nonce, msg)
 
12. Empfange regulaere Nachrichten:
    e2eDhSecret aus DB laden
    Entschluesselung mit gespeichertem Secret
```