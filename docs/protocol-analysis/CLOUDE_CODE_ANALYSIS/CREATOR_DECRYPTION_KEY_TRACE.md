# Creator Decryption Key Trace: Reply Queue E2E Decryption
 
## Overview
 
When the **CREATOR** receives a message on the Reply Queue, there are **two layers** of
decryption, each using a **different DH shared secret** stored in the `RcvQueue`.
 
---
 
## 1. Which Private Key Does the Creator Use for E2E Decryption?
 
### Answer: TWO different keys for TWO different layers
 
#### Layer 1 — SMP transport encryption: `rcvDhSecret`
 
| Property | Value |
|----------|-------|
| **Field** | `rcvDhSecret :: RcvDhSecret` (alias for `C.DhSecretX25519`) |
| **Location** | `RcvQueue` record, `Store.hs:77` |
| **Purpose** | Decrypt the outer SMP envelope (server-to-recipient) |
| **Called in** | `decryptSMPMessage` at `Client.hs:1682` |
 
```haskell
-- Client.hs:1682-1686
decryptSMPMessage :: RcvQueue -> SMP.RcvMessage -> AM SMP.ClientRcvMsgBody
decryptSMPMessage rq SMP.RcvMessage {msgId, msgBody = SMP.EncRcvMsgBody body} =
  liftEither $ parse SMP.clientRcvMsgBodyP (AGENT A_MESSAGE) =<< decrypt body
  where
    decrypt = agentCbDecrypt (rcvDhSecret rq) (C.cbNonce msgId)
```
 
This secret is computed at queue creation time using the **server's public DH key**:
 
```haskell
-- Client.hs:1400
rcvDhSecret = C.dh' rcvPublicDhKey privDhKey
```
 
where `rcvPublicDhKey` comes from the server's CREATE response and `privDhKey` is the
recipient's ephemeral DH private key generated for that queue. This key is **not** sent
in the invitation — it's strictly between the recipient and the SMP server.
 
#### Layer 2 — Per-queue E2E encryption: `e2ePrivKey` → `e2eDhSecret`
 
| Property | Value |
|----------|-------|
| **Private key field** | `e2ePrivKey :: C.PrivateKeyX25519` |
| **Location** | `RcvQueue` record, `Store.hs:79` |
| **Computed secret field** | `e2eDhSecret :: Maybe C.DhSecretX25519` |
| **Location** | `RcvQueue` record, `Store.hs:81` |
| **Purpose** | Decrypt the inner client message envelope (sender-to-recipient) |
| **Called in** | `decryptClientMessage` at `Agent.hs:2885` |
 
**This IS related to the key sent in the invitation.** The **public half** of `e2ePrivKey`
is included in the `SMPQueueUri` that forms part of the invitation:
 
```haskell
-- Client.hs:1416
qUri = SMPQueueUri vRange $ SMPQueueAddress srv sndId e2eDhKey queueMode
--                                                     ^^^^^^^^
-- e2eDhKey is the PUBLIC key corresponding to e2ePrivKey
```
 
---
 
## 2. RcvQueue E2E Key Fields (Store.hs:68-104)
 
```haskell
data StoredRcvQueue (q :: DBStored) = RcvQueue
  { ...
    -- | shared DH secret used to encrypt/decrypt message bodies from server to recipient
    rcvDhSecret :: RcvDhSecret,              -- Line 77: SMP transport layer
    -- | private DH key related to public sent to sender out-of-band (to agree simple per-queue e2e)
    e2ePrivKey :: C.PrivateKeyX25519,         -- Line 79: Recipient's E2E private key
    -- | public sender's DH key and agreed shared DH secret for simple per-queue e2e
    e2eDhSecret :: Maybe C.DhSecretX25519,    -- Line 81: Agreed E2E shared secret (Nothing until confirmation)
    ...
  }
```
 
### Summary of the three fields:
 
| Field | Type | Layer | When Set |
|-------|------|-------|----------|
| `rcvDhSecret` | `RcvDhSecret` (`C.DhSecretX25519`) | SMP server↔recipient | At queue creation (`Client.hs:1400`) |
| `e2ePrivKey` | `C.PrivateKeyX25519` | Sender↔recipient E2E | At queue creation (`Client.hs:1401`) |
| `e2eDhSecret` | `Maybe C.DhSecretX25519` | Sender↔recipient E2E (cached) | Starts `Nothing`, set on confirmation (`AgentStore.hs:545`) |
 
---
 
## 3. Queue Creation: Which Keypairs Are Generated?
 
In `newRcvQueue_` (`Client.hs:1378-1417`), **two independent DH keypairs** are generated:
 
### Keypair A — SMP transport (server↔recipient)
```haskell
-- Client.hs:1383
(dhKey, privDhKey) <- atomically $ C.generateKeyPair g
```
- `dhKey` is sent to the server in the CREATE command
- `privDhKey` + server's response `rcvPublicDhKey` → `rcvDhSecret` (line 1400)
 
### Keypair B — Per-queue E2E (sender↔recipient)
```haskell
-- Generated BEFORE newRcvQueue_ is called, passed in as parameter:
-- Client.hs:1360
e2eKeys <- atomically . C.generateKeyPair =<< asks random
-- Also: Agent.hs:947
e2eKeys <- atomically . C.generateKeyPair =<< asks random
```
- `e2ePrivKey` is stored in the `RcvQueue` (line 1401)
- `e2eDhKey` (the public half) goes into the `SMPQueueUri` / invitation (line 1416)
- `e2eDhSecret` starts as `Nothing` (line 1402), computed later
 
**They are different keypairs for different purposes.**
 
---
 
## 4. Full Decrypt Path: MSG on Reply Queue
 
### Step-by-step trace:
 
```
MSG arrives on SMP server
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│ Agent.hs:2694  SMP.MSG msg@SMP.RcvMessage{msgId=srvMsgId}  │
│                                                             │
│ RcvQueue is pattern-matched at line 2688:                   │
│   rq@RcvQueue { e2ePrivKey, e2eDhSecret, ... }             │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│ LAYER 1: SMP Transport Decryption                          │
│                                                             │
│ Agent.hs:2697                                               │
│   msg' <- decryptSMPMessage rq msg                          │
│                                                             │
│ Client.hs:1682-1686                                         │
│   decrypt = agentCbDecrypt (rcvDhSecret rq) (C.cbNonce msgId│)
│                              ^^^^^^^^^^^^^^                 │
│                   KEY: rcvDhSecret from RcvQueue            │
│                                                             │
│ Strips SMP server encryption → ClientRcvMsgBody             │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│ Agent.hs:2706  Parse ClientMsgEnvelope from msgBody         │
│   SMP.PubHeader phVer e2ePubKey_                            │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│ LAYER 2: Per-Queue E2E Decryption                          │
│                                                             │
│ Agent.hs:2710  case (e2eDhSecret, e2ePubKey_) of            │
│                                                             │
│ ┌─ CASE A (first message, handshake): ──────────────────┐   │
│ │ (Nothing, Just e2ePubKey) →                           │   │
│ │   let e2eDh = C.dh' e2ePubKey e2ePrivKey              │   │
│ │                      ^^^^^^^^^ ^^^^^^^^^^             │   │
│ │                      from msg   from RcvQueue:79      │   │
│ │   decryptClientMessage e2eDh clientMsg                │   │
│ └───────────────────────────────────────────────────────┘   │
│                                                             │
│ ┌─ CASE B (established connection): ────────────────────┐   │
│ │ (Just e2eDh, Nothing) →                               │   │
│ │   decryptClientMessage e2eDh clientMsg                │   │
│ │                        ^^^^^                          │   │
│ │                 stored in RcvQueue.e2eDhSecret:81     │   │
│ └───────────────────────────────────────────────────────┘   │
│                                                             │
│ Agent.hs:2885-2896  decryptClientMessage                    │
│   agentCbDecrypt e2eDh cmNonce cmEncBody                    │
│   → (PrivHeader, AgentMsgEnvelope)                          │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────────┐
│ LAYER 3: Double Ratchet Decryption (for data messages)     │
│                                                             │
│ Agent.hs:2727  AgentMsgEnvelope { encAgentMessage }         │
│                                                             │
│ Agent.hs:2814 (inside agentClientMsg)                       │
│   agentRatchetDecrypt' g db connId rc encAgentMessage       │
│                                                             │
│ Agent.hs:3354-3364                                          │
│   rc  ← getRatchet db connId       -- from DB, per-conn    │
│   CR.rcDecrypt g rc skipped encAgentMessage                 │
│                                                             │
│ → A_MSG body (the actual plaintext message)                 │
└─────────────────────────────────────────────────────────────┘
```
 
### When is `e2eDhSecret` stored?
 
After the CREATOR processes the initial confirmation (CONF), the computed DH secret is
persisted to the database via `setRcvQueueConfirmedE2E`:
 
```haskell
-- Agent.hs:2946-2949 (inside smpConfirmation, RcvConnection branch)
let RcvQueue {smpClientVersion = v, e2ePrivKey = e2ePrivKey'} = rq
    SMPConfirmation {smpClientVersion = v', e2ePubKey = e2ePubKey'} = senderConf
    dhSecret = C.dh' e2ePubKey' e2ePrivKey'
setRcvQueueConfirmedE2E db rq dhSecret $ min v v'
 
-- AgentStore.hs:545-556
setRcvQueueConfirmedE2E :: DB.Connection -> RcvQueue -> C.DhSecretX25519 -> VersionSMPC -> IO ()
setRcvQueueConfirmedE2E db RcvQueue {rcvId, ...} e2eDhSecret smpClientVersion =
  DB.execute db
    [sql| UPDATE rcv_queues
          SET e2e_dh_secret = ?, status = ?, smp_client_version = ?
          WHERE host = ? AND port = ? AND rcv_id = ? |]
    (e2eDhSecret, Confirmed, smpClientVersion, host, port, rcvId)
```
 
After this, subsequent messages hit **Case B** (`Just e2eDh, Nothing`) and use the
pre-computed secret directly.
 
---
 
## 5. Is the E2E Key the Same as What Was Sent in the Invitation?
 
**Yes and no:**
 
- The **public half** of `e2ePrivKey` is what goes into the invitation URI (`e2eDhKey` in
  `SMPQueueAddress`). The sender uses this public key to compute a shared DH secret.
- The **private half** (`e2ePrivKey`) stays in the `RcvQueue` and is never transmitted.
- The sender generates an **ephemeral keypair** and includes its public key in the message
  header (`PubHeader.phE2ePubDhKey`).
- The creator computes: `e2eDh = C.dh' senderEphemeralPubKey e2ePrivKey`
- The sender computed:  `e2eDh = C.dh' creatorPubKeyFromInvitation senderEphemeralPrivKey`
- Both sides arrive at the same `DhSecretX25519` via standard X25519 DH agreement.
 
### Sender-side proof (`agentCbEncryptOnce` in `Client.hs:1936-1946`):
 
```haskell
agentCbEncryptOnce clientVersion dhRcvPubKey msg = do
  g <- asks random
  (dhSndPubKey, dhSndPrivKey) <- atomically $ C.generateKeyPair g  -- ephemeral
  let e2eDhSecret = C.dh' dhRcvPubKey dhSndPrivKey
  --                       ^^^^^^^^^^^
  --           creator's public key from invitation
  cmNonce <- atomically $ C.randomCbNonce g
  cmEncBody <- liftEither . first cryptoError $
    C.cbEncrypt e2eDhSecret cmNonce msg SMP.e2eEncConfirmationLength
  let cmHeader = SMP.PubHeader clientVersion (Just dhSndPubKey)
  --                                               ^^^^^^^^^^^
  --                           ephemeral pub key sent in header
  pure $ smpEncode SMP.ClientMsgEnvelope {cmHeader, cmNonce, cmEncBody}
```
 
---
 
## Key File References
 
| File | Key Lines | What |
|------|-----------|------|
| `src/Simplex/Messaging/Agent/Store.hs` | 68-104 | `RcvQueue` definition with `rcvDhSecret`, `e2ePrivKey`, `e2eDhSecret` |
| `src/Simplex/Messaging/Agent/Client.hs` | 1378-1417 | `newRcvQueue_`: queue creation, both keypairs generated |
| `src/Simplex/Messaging/Agent/Client.hs` | 1682-1686 | `decryptSMPMessage`: Layer 1 decrypt using `rcvDhSecret` |
| `src/Simplex/Messaging/Agent/Client.hs` | 1936-1946 | `agentCbEncryptOnce`: sender's ephemeral DH for first message |
| `src/Simplex/Messaging/Agent.hs` | 2685-2740 | `processSMP`: MSG handling, Layer 2 case dispatch |
| `src/Simplex/Messaging/Agent.hs` | 2885-2896 | `decryptClientMessage`: Layer 2 decrypt |
| `src/Simplex/Messaging/Agent.hs` | 2946-2949 | `smpConfirmation`: computes and stores `e2eDhSecret` |
| `src/Simplex/Messaging/Agent.hs` | 3354-3364 | `agentRatchetDecrypt`: Layer 3 double ratchet |
| `src/Simplex/Messaging/Agent/Store/AgentStore.hs` | 545-556 | `setRcvQueueConfirmedE2E`: persists E2E secret to DB |
| `src/Simplex/Messaging/Protocol.hs` | 1417-1434 | Type aliases: `RcvDhSecret`, `RcvPrivateAuthKey`, etc. |