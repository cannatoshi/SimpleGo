# SimpleX Connection Establishment: Complete Analysis for ESP32 Client
 
## Table of Contents
1. [Connection Establishment Flow](#1-connection-establishment-flow)
2. [Bob's E2E Key for Alice's Reply Queue](#2-bobs-e2e-key-for-alices-reply-queue)
3. [Client Message Envelope Byte Structure](#3-client-message-envelope-byte-structure)
4. [e2eDhSecret Calculation](#4-e2edhsecret-calculation)
5. [What maybe_e2e = ',' Means](#5-what-maybe_e2e--means)
6. [THE ANSWER: Which Key for Reply Queue](#6-the-answer-which-key-for-reply-queue)
7. [Root Cause of MAC Mismatch](#7-root-cause-of-mac-mismatch)
 
---
 
## 1. Connection Establishment Flow
 
### Full Sequence: Alice Creates Invite, Bob Scans It
 
**Conventions**: Alice = connection creator (you/ESP32), Bob = joiner (peer/phone app).
 
```
Alice (ESP32)                         SMP Server                    Bob (Phone)
     |                                    |                              |
     |--- CREATE RcvQueue (NEW) --------->|                              |
     |<-- queue URI + rcvId --------------|                              |
     |                                    |                              |
     | [Generate X25519 e2e keypair for Contact Queue]                   |
     | [Generate X448 ratchet keypairs (pk1, pk2)]                       |
     | [Build invitation URI with queue + e2e + ratchet params]          |
     |                                    |                              |
     |====== Invitation URI (QR/link) ===========================>|     |
     |                                    |                              |
     |                                    |<-- Bob creates SndQueue -----|
     |                                    |    (computes e2eDhSecret     |
     |                                    |     for Contact Queue)       |
     |                                    |                              |
     |                                    |<-- Bob creates his RcvQueue--|
     |                                    |    (Reply Queue, new e2e     |
     |                                    |     keypair)                 |
     |                                    |                              |
     |                                    |<-- Bob SEND confirmation ----|
     |                                    |    (AgentConfirmation with   |
     |                                    |     reply queue info +       |
     |                                    |     e2eSndParams +           |
     |                                    |     Bob's e2ePubKey in hdr)  |
     |                                    |                              |
     |<-- MSG (confirmation) -------------|                              |
     | [e2ePubKey_ = Just bobPubKey]      |                              |
     | [Compute e2eDh for Contact Queue]  |                              |
     | [Decrypt -> AgentConfirmation]     |                              |
     | [Init recv ratchet from e2eSndParams]                             |
     | [Extract Bob's Reply Queue URI]    |                              |
     | [Store CONF]                       |                              |
     |                                    |                              |
     | [User calls allowConnection/LET]   |                              |
     |                                    |                              |
     |--- SECURE Contact Queue (KEY) ---->|                              |
     |                                    |                              |
     | [Create SndQueue for Bob's Reply Queue]                           |
     | [Generate NEW X25519 e2e keypair for Reply Queue sending]         |
     | [Compute e2eDhSecret for Reply Queue]                             |
     |                                    |                              |
     |--- SEND confirmation to Reply Q -->|                              |
     |    (AgentConnInfo + Alice's        |                              |
     |     e2ePubKey in PubHeader)        |                              |
     |                                    |--- MSG to Bob's Reply Q ---->|
     |                                    |    [e2ePubKey_ = Just]       |
     |                                    |    Bob computes e2eDhSecret  |
     |                                    |    for his Reply Queue       |
     |                                    |                              |
     |--- SEND HELLO to Reply Q --------->|--- MSG HELLO to Bob -------->|
     |    [e2ePubKey_ = Nothing]          |   [uses stored e2eDhSecret]  |
     |    [ratchet-encrypted]             |                              |
     |                                    |                              |
     |                                    |<-- Bob SEND HELLO -----------|
     |<-- MSG HELLO ----------------------|   [e2ePubKey_ = Nothing]     |
     |    [uses stored e2eDhSecret]       |   [ratchet-encrypted]        |
     |                                    |                              |
     |===== CONNECTION ESTABLISHED (CON) =========================|     |
```
 
### Code References for Each Step
 
#### Step 1: Alice creates invitation
- **Entry**: `createConnection` at `Agent.hs:371`
- **Internal**: `newConn` at `Agent.hs:832` -> `newRcvConnSrv` at `Agent.hs:942`
- **E2E keypair for Contact Queue**: generated at `Agent.hs:947`
- **Ratchet params**: `CR.generateRcvE2EParams` at `Ratchet.hs:439`
- **RcvQueue created with `e2eDhSecret = Nothing`**: `Client.hs:1402`
 
#### Step 2: Bob joins connection
- **Entry**: `joinConnection` at `Agent.hs:414`
- **Internal**: `joinConn` at `Agent.hs:1060` -> `startJoinInvitation` at `Agent.hs:1070`
- **Bob's SndQueue created**: `newSndQueue` at `Agent.hs:3366`
  - **e2eDhSecret computed**: `Agent.hs:3381` -- `C.dh' rcvE2ePubDhKey e2ePrivKey`
  - **e2ePubKey stored**: `Agent.hs:3382` -- `Just e2ePubKey`
- **Bob's Reply Queue created**: `createReplyQueue` at `Agent.hs:1212`
- **Ratchet init**: `CR.initSndRatchet` at `Ratchet.hs:643`
 
#### Step 3: Bob sends confirmation to Alice's Contact Queue
- **Function**: `sendConfirmation` at `Client.hs:1646`
- **Encrypted with**: `agentCbEncrypt sq e2ePubKey` at `Client.hs:1650`
  - Uses `SndQueue.e2eDhSecret` (DH of Alice's Contact Queue pubkey + Bob's privkey)
  - Includes `SndQueue.e2ePubKey` in PubHeader (Bob's fresh X25519 pubkey)
- **Payload**: `AgentConfirmation` containing:
  - `e2eEncryption_` = Bob's `SndE2ERatchetParams` (his ratchet DH public keys)
  - `encConnInfo` = ratchet-encrypted `AgentConnInfoReply` with Bob's Reply Queue info
 
#### Step 4: Alice receives confirmation on Contact Queue
- **Processing**: `processSMP` at `Agent.hs:2686`
- **Dispatch**: `Agent.hs:2710-2711` -- `(Nothing, Just e2ePubKey)` branch
- **DH computed on the fly**: `Agent.hs:2712` -- `C.dh' e2ePubKey e2ePrivKey`
- **Ratchet initialized**: `smpConfirmation` at `Agent.hs:2901`, case `RcvConnection` at `Agent.hs:2912`
  - `CR.pqX3dhRcv` at `Ratchet.hs:482` -- X3DH key agreement
  - `CR.initRcvRatchet` at `Ratchet.hs:679`
- **e2eDhSecret stored**: `setRcvQueueConfirmedE2E` at `Agent.hs:2949`, `AgentStore.hs:545`
- **Notification**: `CONF confId pqSupport srvs connInfo`
 
#### Step 5: Alice allows connection (LET)
- **Entry**: `allowConnection'` at `Agent.hs:1224`
- **Enqueues**: `ICAllowSecure` internal command
- **Processed at**: `Agent.hs:1527`
  - Secures Contact Queue (KEY command)
  - Calls `connectReplyQueues` at `Agent.hs:3227`
 
#### Step 6: Alice creates SndQueue for Bob's Reply Queue
- **In `connectReplyQueues`**: `Agent.hs:3236` -> `newSndQueue` at `Agent.hs:3366`
- **CRITICAL**: A **NEW X25519 keypair** is generated (`Agent.hs:3371`)
- **e2eDhSecret**: `C.dh' rcvE2ePubDhKey e2ePrivKey` (`Agent.hs:3381`)
  - `rcvE2ePubDhKey` = Bob's Reply Queue DH public key (from the queue URI in his confirmation)
  - `e2ePrivKey` = Alice's freshly generated private key for this SndQueue
- **e2ePubKey**: `Just e2ePubKey` (`Agent.hs:3382`) -- Alice's fresh public key
 
#### Step 7: Alice sends confirmation to Bob's Reply Queue
- **Via**: `enqueueConfirmation` at `Agent.hs:3290` -> `storeConfirmation` -> `sendConfirmation`
- **PubHeader includes**: `e2ePubKey = Just` (Alice's fresh X25519 pubkey for Reply Queue)
- **Body encrypted with**: `e2eDhSecret` = DH(Bob's Reply Queue pubkey, Alice's fresh privkey)
 
#### Step 8: Bob receives Alice's confirmation on his Reply Queue
- **Processing**: `Agent.hs:2710-2711` -- `(Nothing, Just e2ePubKey)` branch
- **Bob computes**: `e2eDh = C.dh' e2ePubKey e2ePrivKey` at `Agent.hs:2712`
  - `e2ePubKey` = Alice's fresh pubkey from the PubHeader
  - `e2ePrivKey` = Bob's Reply Queue private key
- **Stored**: `setRcvQueueConfirmedE2E` at `Agent.hs:2964`
 
#### Step 9: HELLO exchange
- **Alice sends HELLO**: after `ICDuplexSecure` at `Agent.hs:1549-1551`
  - `enqueueMessage c cData sq SMP.MsgFlags {notification = True} HELLO`
  - Encrypted with ratchet, then per-queue E2E with **stored** e2eDhSecret
  - **PubHeader**: `e2ePubKey = Nothing` (no fresh key; uses stored secret)
- **Bob receives HELLO**: `helloMsg` at `Agent.hs:2975`
  - Decrypted with stored `e2eDhSecret` (the `(Just e2eDh, Nothing)` branch at `Agent.hs:2722`)
  - Bob sends HELLO back
- **Alice receives HELLO**: same `(Just e2eDh, Nothing)` branch
- **CON notification**: `Agent.hs:2990`
 
---
 
## 2. Bob's E2E Key for Alice's Reply Queue
 
### What Does Bob Send When Connecting to Alice's Contact Queue?
 
Bob sends a `ClientMsgEnvelope` with:
1. **PubHeader**: Contains Bob's `e2ePubKey` (X25519 public key, fresh per-SndQueue)
2. **CbNonce**: Random 24-byte nonce
3. **Encrypted body**: NaCl crypto_box encrypted with `e2eDhSecret`
 
The encrypted body, once decrypted, contains a `ClientMessage`:
1. **PrivHeader**: Either `PHConfirmation sndPublicKey` or `PHEmpty` (depends on queue mode)
2. **AgentMsgEnvelope body**: An `AgentConfirmation` envelope
 
### AgentConfirmation Structure
 
Defined at `Agent/Protocol.hs:772`:
```haskell
AgentConfirmation
  { agentVersion :: VersionSMPA,           -- 2 bytes
    e2eEncryption_ :: Maybe (SndE2ERatchetParams 'C.X448),  -- ratchet DH keys
    encConnInfo :: ByteString              -- ratchet-encrypted inner message
  }
```
 
The `encConnInfo`, once decrypted with the double ratchet, contains an `AgentConnInfoReply`:
```haskell
AgentConnInfoReply (NonEmpty SMPQueueInfo) ConnInfo
```
 
The `SMPQueueInfo` contains Bob's Reply Queue URI, which includes:
- SMP server address
- Queue send ID
- Queue mode
- **`dhPublicKey` = Bob's Reply Queue X25519 public key** (used for per-queue E2E)
 
### Is This Double Ratchet Encrypted or Plain E2E?
 
**Both layers are applied:**
1. **Outer layer (per-queue E2E)**: The entire `ClientMessage` (PrivHeader + AgentConfirmation) is encrypted with NaCl crypto_box using `SndQueue.e2eDhSecret`
2. **Inner layer (double ratchet)**: The `encConnInfo` field inside `AgentConfirmation` is additionally encrypted with the double ratchet (AES-256-GCM after X3DH key agreement)
 
### Where Is Bob's e2e_public Key?
 
**For the Contact Queue per-queue E2E**: Bob's X25519 public key is in `ClientMsgEnvelope.cmHeader.phE2ePubDhKey` (the `PubHeader`). This is **outside** the encryption -- it's plaintext in the envelope header.
 
**For Bob's Reply Queue per-queue E2E**: Bob's Reply Queue X25519 DH public key is inside `SMPQueueAddress.dhPublicKey` within the `SMPQueueInfo` in the `AgentConnInfoReply`. This is buried inside two layers of encryption (per-queue E2E + double ratchet).
 
---
 
## 3. Client Message Envelope Byte Structure
 
### Three Layers of Encryption
 
```
Layer 1: Server transport encryption (rcvDhSecret)
  |
  v
Layer 2: Per-queue E2E encryption (e2eDhSecret, NaCl crypto_box)
  |
  v
Layer 3: Double ratchet encryption (AES-256-GCM)
  |
  v
Plaintext AgentMessage
```
 
### After Server-Level Decrypt (Layer 1)
 
`decryptSMPMessage` at `Client.hs:1682` decrypts with `rcvDhSecret` and `cbNonce(msgId)`.
After decrypt + unPad, the result is parsed by `clientRcvMsgBodyP` (`Protocol.hs:808`):
 
```
Offset  Size  Field
------  ----  -----
0       8     msgTs.seconds (Int64 big-endian, two Word32s)
8       1     msgFlags.notification (Bool: 'T'=0x54 or 'F'=0x46)
9       ~7    future flags (consumed by takeTill ' ')
~10     1     ' ' space separator (0x20)
~11+    var   ClientMsgEnvelope (the per-queue E2E encrypted message)
```
 
Note: Currently MsgFlags only uses 1 byte, so offsets 9 onward are `' '` followed by the ClientMsgEnvelope starting at offset 10.
 
### ClientMsgEnvelope Structure
 
Encoded at `Protocol.hs:1084`:
```haskell
smpEncode (cmHeader, cmNonce, Tail cmEncBody)
```
 
```
Offset  Size  Field                    Encoding
------  ----  -----                    --------
0       2     phVersion                Word16 big-endian (e.g., 0x0004 = v4)
2       1     Maybe tag                '0' (0x30) = Nothing, '1' (0x31) = Just
                                       [If Just, followed by encoded key:]
3       1       key DER length         byte (0x2C = 44 for X25519)
4       44      X.509 DER X25519 key   ASN.1 DER encoding of X25519 public key
        --- then CbNonce follows ---
?       24    cmNonce                  Raw 24 bytes (NO length prefix)
?       rest  cmEncBody               All remaining bytes (Tail, no length prefix)
```
 
**For normal messages (maybe_e2e = Nothing):**
```
[00][04]            Version = 4
[30]                Nothing (no e2e key)
[24 bytes nonce]    CbNonce
[rest]              cmEncBody (NaCl crypto_box ciphertext)
                    = [16-byte Poly1305 MAC][encrypted padded body]
```
Total header: 2 + 1 + 24 = **27 bytes** before encrypted body.
 
**For confirmation messages (maybe_e2e = Just key):**
```
[00][04]            Version = 4
[31]                Just (e2e key present)
[2C]                Key length = 44 bytes
[44 bytes]          X.509 DER-encoded X25519 public key
[24 bytes nonce]    CbNonce
[rest]              cmEncBody
```
Total header: 2 + 1 + 1 + 44 + 24 = **72 bytes** before encrypted body.
 
### X.509 DER Encoding of X25519 Public Key (44 bytes)
 
```
30 2a                    SEQUENCE (42 bytes)
  30 05                  SEQUENCE (5 bytes) - algorithm identifier
    06 03 2b 65 6e       OID 1.3.101.110 (id-X25519)
  03 21 00               BIT STRING (33 bytes, 0 unused bits)
    [32 bytes]           Raw X25519 public key bytes
```
 
### After Per-Queue E2E Decrypt (Layer 2)
 
`decryptClientMessage` at `Agent.hs:2885` decrypts `cmEncBody` with `e2eDhSecret` and `cmNonce`.
 
The `cmEncBody` format (NaCl crypto_box):
```
[16 bytes]    Poly1305 authentication tag
[rest]        XSalsa20-encrypted padded body
```
 
After decrypt + unPad, the result is a `ClientMessage`:
```
Offset  Size  Field
------  ----  -----
0       1+    PrivHeader: '_' (0x5F) for PHEmpty
              or 'K' (0x4B) + encoded auth public key for PHConfirmation
rest    var   AgentMsgEnvelope body (raw bytes, parsed by takeByteString)
```
 
### AgentMsgEnvelope Structure
 
Encoded at `Agent/Protocol.hs:794`:
 
**For AgentConfirmation (tag 'C' = 0x43):**
```
[2 bytes]     agentVersion (Word16 BE)
[43]          'C' tag
[30 or 31]    Maybe tag for e2eEncryption_
              [If '1': SndE2ERatchetParams follows]
[rest]        encConnInfo (Tail, ratchet-encrypted)
```
 
**For AgentMsgEnvelope (tag 'M' = 0x4D) -- regular messages:**
```
[2 bytes]     agentVersion (Word16 BE)
[4D]          'M' tag
[rest]        encAgentMessage (Tail, ratchet-encrypted)
```
 
### About Your Hex Dump `3e 82 00 00 00 00 69 81`
 
If this is after server-level decrypt (after unPad), the first 8 bytes are `msgTs.seconds` (Int64 big-endian). The value `0x3E82000000006981` does not look like a valid Unix timestamp (~1.7 billion for 2025). This suggests either:
 
1. You may be looking at the raw padded data (before unPad) where the first 2 bytes are the length prefix: `3e 82` = 16002. This is plausible since `e2eEncMessageLength = 16000` and the actual message + 2-byte header would be close to this.
2. Or you're at a different offset than expected.
 
**If `3e 82` = length prefix (16002)**: After the 2-byte length, the next bytes are the actual ClientRcvMsgBody starting with the timestamp. The `00 00 00 00 69 81...` would be the beginning of the timestamp (the high bytes of the seconds).
 
---
 
## 4. e2eDhSecret Calculation
 
### The Core DH Function
 
Defined at `Crypto.hs:1262`:
```haskell
dh' :: DhAlgorithm a => PublicKey a -> PrivateKey a -> DhSecret a
dh' (PublicKeyX25519 k) (PrivateKeyX25519 pk _) = DhSecretX25519 $ X25519.dh k pk
```
 
This performs **Curve25519 ECDH**: `shared_secret = X25519(private_key, public_key)`.
The result is a raw 32-byte shared secret, used **directly** as the XSalsa20 key (no additional KDF).
 
### For Contact Queue (Alice's RcvQueue)
 
**When Bob sends** (creating his SndQueue at `Agent.hs:3366`):
```
Bob's e2eDhSecret = DH(Alice_contact_queue_dhPubKey, Bob_fresh_e2ePrivKey)
```
- `Alice_contact_queue_dhPubKey` = from the invitation URI (the `dhPublicKey` in `SMPQueueAddress`)
- `Bob_fresh_e2ePrivKey` = freshly generated at `Agent.hs:3371`
 
**When Alice receives** (at `Agent.hs:2711-2712`):
```
Alice's e2eDh = DH(Bob_e2ePubKey_from_header, Alice_contact_queue_e2ePrivKey)
```
- `Bob_e2ePubKey_from_header` = from `ClientMsgEnvelope.cmHeader.phE2ePubDhKey`
- `Alice_contact_queue_e2ePrivKey` = the private key Alice generated when creating her Contact Queue
 
**These produce the same shared secret** (DH commutativity).
 
### For Reply Queue (Bob's RcvQueue) -- THIS IS YOUR PROBLEM
 
**When Alice sends** (creating her SndQueue for Bob's Reply Queue at `Agent.hs:3366`):
```
Alice's e2eDhSecret = DH(Bob_reply_queue_dhPubKey, Alice_fresh_e2ePrivKey)
```
- `Bob_reply_queue_dhPubKey` = from `SMPQueueAddress.dhPublicKey` in the `SMPQueueInfo` that Bob included in his `AgentConnInfoReply`
- `Alice_fresh_e2ePrivKey` = **NEW** keypair generated specifically for this SndQueue (`Agent.hs:3371`)
 
**When Bob receives the FIRST message on his Reply Queue** (at `Agent.hs:2711-2712`):
```
Bob's e2eDh = DH(Alice_e2ePubKey_from_header, Bob_reply_queue_e2ePrivKey)
```
- `Alice_e2ePubKey_from_header` = Alice's fresh public key, included in the PubHeader of the first message
- `Bob_reply_queue_e2ePrivKey` = Bob's private key for his Reply Queue (generated when he created the Reply Queue at `Agent.hs:1212`)
 
**For ALL subsequent messages on Bob's Reply Queue** (HELLO, A_MSG, etc.):
```
Use the STORED e2eDhSecret (computed from the first message)
```
No new DH computation. The stored secret is used with `(Just e2eDh, Nothing)` branch at `Agent.hs:2722`.
 
### Where Does peer_e2e_public Come From?
 
**For Contact Queue**: Sender's `e2ePubKey` is in `ClientMsgEnvelope.cmHeader.phE2ePubDhKey` of the **first** message received on that queue.
 
**For Reply Queue**: Sender's `e2ePubKey` is in `ClientMsgEnvelope.cmHeader.phE2ePubDhKey` of the **first** message received on that queue (the confirmation).
 
**CRITICAL**: These are **different keys** for different queues. Each SndQueue generates its own X25519 keypair. The Contact Queue and Reply Queue have completely independent e2eDhSecret values.
 
---
 
## 5. What maybe_e2e = ',' Means
 
### The Encoding
 
The `Maybe C.PublicKeyX25519` field in `PubHeader` (`Protocol.hs:1076`) is encoded using the standard `Maybe` instance from `Encoding.hs:114`:
 
```haskell
instance Encoding a => Encoding (Maybe a) where
  smpEncode = maybe "0" (('1' `B.cons`) . smpEncode)
```
 
- **Nothing**: encoded as single byte `'0'` (0x30)
- **Just key**: encoded as `'1'` (0x31) followed by the encoded key
 
### What You're Seeing
 
You describe `[14] = '1'` and `[15] = ','`. There are two interpretations:
 
**Interpretation A**: If byte 14 is `0x31` ('1') and byte 15 is `0x2C` (','= 44 decimal), then:
- Byte 14 = `'1'` = `Just` tag -- **there IS a fresh e2e key**
- Byte 15 = `0x2C` = 44 = the length of the X.509 DER-encoded X25519 key
 
This would mean the message IS a confirmation (first message) with a fresh e2e key. The key follows for 44 bytes after the length byte.
 
**Interpretation B**: If you're looking at the SMP transmission framing (not the ClientMsgEnvelope body), the `','` might be the separator between `corrId` and the entity ID in the SMP frame. In that case, `'1'` could be the corrId value, and `','` is a literal field separator.
 
### When maybe_e2e = Nothing ('0'), Which Key Is Used?
 
When `phE2ePubDhKey = Nothing`:
- This is the `(Just e2eDh, Nothing)` branch at `Agent.hs:2722`
- The **stored** `RcvQueue.e2eDhSecret` is used
- This secret was computed and stored when the FIRST message on this queue was received
- The first message had `phE2ePubDhKey = Just` and the DH secret was computed then stored via `setRcvQueueConfirmedE2E` at `AgentStore.hs:545`
 
### Code That Reads This Flag
 
At `Agent.hs:2706`:
```haskell
clientMsg@SMP.ClientMsgEnvelope {cmHeader = SMP.PubHeader phVer e2ePubKey_} <-
  parseMessage msgBody
```
 
Then the dispatch at `Agent.hs:2710`:
```haskell
case (e2eDhSecret, e2ePubKey_) of
  (Nothing, Just e2ePubKey) -> ...   -- First message, compute DH
  (Just e2eDh, Nothing) -> ...       -- Subsequent message, use stored
  (Just e2eDh, Just _) -> ...        -- Repeated confirmation delivery
  (Nothing, Nothing) -> prohibited   -- Error
```
 
---
 
## 6. THE ANSWER: Which Key for Reply Queue E2E Decrypt
 
### For Reply Queue E2E Decrypt with maybe_e2e = Nothing
 
**You need**: `e2eDhSecret = DH(peer_e2ePubKey, your_reply_queue_e2ePrivKey)`
 
Where:
- **`peer_e2ePubKey`**: The X25519 public key that was in the `PubHeader.phE2ePubDhKey` of the **FIRST** message you received on your Reply Queue. This was the peer's confirmation message.
- **`your_reply_queue_e2ePrivKey`**: The X25519 private key you generated when you created your Reply Queue.
 
### WHERE Did You Receive This Key From the Peer?
 
The peer's e2e public key for your Reply Queue arrives in this message:
 
```
YOUR Reply Queue <- Peer sends confirmation:
  ClientMsgEnvelope {
    cmHeader = PubHeader {
      phVersion = ...,
      phE2ePubDhKey = Just <PEER'S FRESH X25519 PUBKEY>  <-- THIS KEY
    },
    cmNonce = ...,
    cmEncBody = ... (encrypted with DH of your Reply Queue dhPubKey + peer's privkey)
  }
```
 
**This is the FIRST message on your Reply Queue.** It contains `AgentConfirmation` or `AgentConnInfo` inside.
 
### Step-by-Step for ESP32
 
1. **When you create your Reply Queue**, you generate `(reply_e2ePubKey, reply_e2ePrivKey)`. You include `reply_e2ePubKey` in the `SMPQueueAddress.dhPublicKey` field of the queue URI you send to the peer.
 
2. **The FIRST message you receive on your Reply Queue** will have `cmHeader.phE2ePubDhKey = Just peer_pubkey`. Extract this key.
 
3. **Compute**: `e2eDhSecret = X25519_DH(peer_pubkey, reply_e2ePrivKey)`. This is a raw Curve25519 scalar multiplication producing 32 bytes. **No additional KDF** is applied.
 
4. **Store** this `e2eDhSecret`.
 
5. **For ALL subsequent messages** on this Reply Queue (where `phE2ePubDhKey = Nothing`), use this stored `e2eDhSecret` with the per-message `cmNonce` to `cbDecrypt`.
 
### The Decryption Itself (cbDecrypt)
 
```
Input:  e2eDhSecret (32 bytes), cmNonce (24 bytes), cmEncBody
Output: padded ClientMessage bytes
 
Algorithm (NaCl crypto_box = XSalsa20-Poly1305):
1. Split cmEncBody into: tag (first 16 bytes) + ciphertext (rest)
2. XSalsa20 init:
   a. iv0 = first 8 bytes of nonce, iv1 = last 16 bytes of nonce
   b. state0 = HSalsa20(e2eDhSecret, zero16 || iv0)
   c. state1 = Salsa20_derive(state0, iv1)
3. Generate 32 bytes of keystream -> poly_key
4. Decrypt ciphertext with remaining keystream -> plaintext
5. Compute Poly1305(poly_key, ciphertext) -> computed_tag
6. CONSTANT-TIME compare computed_tag with tag
   - If match: return plaintext
   - If mismatch: MAC ERROR  <-- YOUR PROBLEM
7. Unpad: read first 2 bytes as big-endian length, extract that many bytes
```
 
---
 
## 7. Root Cause of MAC Mismatch
 
If you're getting MAC mismatch on Reply Queue messages, the issue is almost certainly one of:
 
### Cause 1: Wrong e2eDhSecret (MOST LIKELY)
 
You may be using the **Contact Queue's e2eDhSecret** instead of the **Reply Queue's e2eDhSecret**. These are completely different:
 
| Queue | Your Private Key | Peer's Public Key | Source of Peer Key |
|-------|-----------------|-------------------|-------------------|
| Contact Queue | Generated when creating Contact Queue | From peer's first message PubHeader | Received in confirmation on Contact Queue |
| Reply Queue | Generated when creating Reply Queue | From peer's first message PubHeader | Received in confirmation on Reply Queue |
 
**Each queue has its own independent X25519 keypair and DH secret.**
 
### Cause 2: Never Processed the First Message on Reply Queue
 
The first message on your Reply Queue is a **confirmation** with `phE2ePubDhKey = Just`. You MUST:
1. Extract the peer's public key from this first message's PubHeader
2. Compute the DH secret
3. Decrypt this first message using the freshly computed DH secret
4. Store the DH secret for subsequent messages
 
If you skipped/failed this step, you have no valid e2eDhSecret for subsequent messages.
 
### Cause 3: Wrong Key Used in DH Computation
 
The DH for Reply Queue must use:
- **Your Reply Queue's e2ePrivKey** (NOT your Contact Queue's e2ePrivKey)
- **Peer's e2ePubKey from the Reply Queue PubHeader** (NOT the Contact Queue one)
 
### Cause 4: Nonce Handling
 
The nonce for per-queue E2E decrypt is `cmNonce` from the `ClientMsgEnvelope`, NOT the `msgId`-derived nonce (that's for the server-level decrypt).
 
### Debugging Checklist
 
```
1. [ ] Do you generate a SEPARATE X25519 keypair for your Reply Queue?
2. [ ] Is the Reply Queue's dhPublicKey correctly included in the SMPQueueAddress
       you send to the peer in your AgentConfirmation?
3. [ ] When the FIRST message arrives on your Reply Queue, do you extract
       peer's e2ePubKey from ClientMsgEnvelope.cmHeader.phE2ePubDhKey?
4. [ ] Do you compute: DH(peer_pubkey, your_REPLY_queue_privkey)?
       NOT: DH(peer_pubkey, your_CONTACT_queue_privkey)?
5. [ ] Do you store this secret and use it for subsequent messages?
6. [ ] For subsequent messages, are you using cmNonce (from the ClientMsgEnvelope)
       as the nonce, NOT the msgId-derived nonce?
7. [ ] After cbDecrypt, do you unPad (read 2-byte BE length prefix, extract)?
```
 
### Summary of All E2E Keys in a Connection
 
```
Connection between Alice (you) and Bob (peer):
 
Alice's Contact Queue (RcvQueue):
  Alice's e2ePrivKey: generated at queue creation
  Alice's e2ePubKey:  included in invitation URI
  e2eDhSecret:        DH(Bob's pubkey from his confirmation, Alice's privkey)
  Used to decrypt:    Bob's messages TO Alice's Contact Queue
 
Bob's Reply Queue (Alice's SndQueue):
  Alice's e2ePrivKey: generated when creating SndQueue for Reply Queue
  Alice's e2ePubKey:  included in PubHeader of first message to Reply Queue
  e2eDhSecret:        DH(Bob's Reply Queue pubkey from URI, Alice's privkey)
  Used to encrypt:    Alice's messages TO Bob's Reply Queue
 
Alice's Reply Queue (RcvQueue, created by Bob in his confirmation):
  Alice's e2ePrivKey: generated at Reply Queue creation
  Alice's e2ePubKey:  included in Reply Queue URI (sent in AgentConnInfo to Bob)
  e2eDhSecret:        DH(Bob's pubkey from his first msg to this queue, Alice's privkey)
  Used to decrypt:    Bob's messages TO Alice's Reply Queue
 
Alice's Contact Queue (Bob's SndQueue):
  Bob's e2ePrivKey:   generated when Bob created his SndQueue
  Bob's e2ePubKey:    included in PubHeader of his confirmation
  e2eDhSecret:        DH(Alice's Contact Queue pubkey from URI, Bob's privkey)
  Used to encrypt:    Bob's messages TO Alice's Contact Queue
```
 
**There are 4 different e2eDhSecret values in a duplex connection. Each queue direction has its own.**
 
---
 
## Appendix: Encryption Constants
 
From `Protocol.hs`:
```
e2eEncConfirmationLength = 15904  (padded length for confirmation messages)
e2eEncMessageLength      = 16000  (padded length for regular messages)
maxMessageLength         = 16048  (max message in SMP block, v8+)
MaxRcvMessageLen         = 16104  (includes 16 bytes timestamp + flags metadata)
```
 
From `Crypto.hs`:
```
CbNonce size     = 24 bytes (XSalsa20 nonce)
Poly1305 tag     = 16 bytes (MAC)
XSalsa20 rounds  = 20
Padding char     = '#' (0x23)
Length prefix     = 2 bytes big-endian (in pad/unPad)
AES-256-GCM tag  = 16 bytes (for ratchet layer)
AES-256-GCM IV   = 16 bytes (for ratchet layer)
```