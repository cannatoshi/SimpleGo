# rcAD (Ratchet Associated Data) Ordering in SimpleX Double Ratchet
 
## 1. Where is rcAD Initially Created?
 
rcAD is created in the `pqX3dh` function:
 
**File:** `src/Simplex/Messaging/Crypto/Ratchet.hs:499-508`
 
```haskell
pqX3dh :: DhAlgorithm a => (PublicKey a, PublicKey a) -> DhSecret a -> DhSecret a -> DhSecret a -> Maybe RatchetKEMAccepted -> RatchetInitParams
pqX3dh (sk1, rk1) dh1 dh2 dh3 kemAccepted =
  RatchetInitParams {assocData, ratchetKey = RatchetKey sk, sndHK = Key hk, rcvNextHK = Key nhk, kemAccepted}
  where
    assocData = Str $ pubKeyBytes sk1 <> pubKeyBytes rk1   -- LINE 503
```
 
It is then stored into the `Ratchet` record's `rcAD` field (line 516) by both
`initSndRatchet` (line 651) and `initRcvRatchet` (line 679).
 
## 2. What Do sk1 and rk1 Represent?
 
**sk1 and rk1 are NOT "sender" and "receiver" of individual messages.** They refer
to the two parties' roles during the X3DH key agreement, which maps to connection
setup roles:
 
| Variable | Meaning | Connection Role |
|----------|---------|-----------------|
| `sk1` | "Sender key 1" — the public key of the party who **joins/accepts** the connection | Invitation scanner (= "Alice" in the spec comments, line 447) |
| `rk1` | "Receiver key 1" — the public key of the party who **creates/initiates** the connection | Invitation creator (= "Bob" in the spec comments, line 438) |
 
The naming `s`/`r` in `pqX3dh` refers to the X3DH send/receive roles:
- **"Snd" (Alice in spec)** = the party that accepts/joins the connection
- **"Rcv" (Bob in spec)** = the party that created/initiated the connection
 
This is explicitly documented in the code:
- Line 438: `-- used by party initiating connection, Bob in double-ratchet spec`
  → `generateRcvE2EParams`
- Line 447: `-- used by party accepting connection, Alice in double-ratchet spec`
  → `generateSndE2EParams`
- Line 466: `-- this is used by the peer joining the connection`
  → `pqX3dhSnd`
- Line 482: `-- this is used by the peer that created new connection, after receiving the reply`
  → `pqX3dhRcv`
 
## 3. Is sk1 Determined by Who INITIATED or Who Is SENDING?
 
**sk1 is determined by connection role (who initiated vs who joined), NOT by who
is sending the current message.**
 
sk1 is always the public key of the party who **joined** the connection (scanned
the invitation / accepted the connection request). This is fixed at connection
setup time and never changes.
 
### Proof: Tracing the Two Call Paths
 
**Path A — The joining party calls `pqX3dhSnd`** (line 469):
```haskell
pqX3dhSnd spk1 spk2 spKem_ (E2ERatchetParams v rk1 rk2 rKem_) = do
  let initParams = pqX3dh (publicKey spk1, rk1) ...
```
Here `sk1 = publicKey spk1` = the joiner's own public key,
and `rk1` = the initiator's public key (received in E2ERatchetParams).
 
**Path B — The initiating party calls `pqX3dhRcv`** (line 485):
```haskell
pqX3dhRcv rpk1 rpk2 rpKem_ (E2ERatchetParams v sk1 sk2 sKem_) = do
  let initParams = pqX3dh (sk1, publicKey rpk1) ...
```
Here `sk1` = the joiner's public key (received in E2ERatchetParams),
and `rk1 = publicKey rpk1` = the initiator's own public key.
 
**Both paths produce identical `(sk1, rk1)` ordering:** joiner's key first, initiator's key second.
 
## 4. Does rcAD Byte Order Stay FIXED or SWAP?
 
**rcAD is FIXED for the entire lifetime of the connection. It NEVER swaps.**
 
Once set at connection creation, `rcAD` is stored in the `Ratchet` record (line 516)
and used identically for ALL subsequent encrypt and decrypt operations regardless
of message direction:
 
- **Header encryption** (line 918): `encryptAEAD rcHKs ehIV ... rcAD (msgHeader ...)`
- **Header decryption** (line 1152): `decryptAEAD k ehIV rcAD ehBody ehAuthTag`
- **Message encryption** (line 973): `encryptAEAD mk iv ... (msgRcAD <> msgEncHeader) msg`
- **Message decryption** (line 1157): `decryptAEAD mk iv (rcAD <> emHeader) emBody emAuthTag`
 
Both parties store the same `rcAD` value. This is verified in the test at lines
386-388 and 464-468 where `paramsAlice` and `paramsBob` must have equal `assocData`.
 
The byte order is always: **joiner's key ‖ initiator's key** (regardless of who
sends a particular message).
 
## 5. In pqX3dh / initRcState: Which Party Provides Which Key?
 
### Key Generation
 
| Function | Who calls it | Line | Comment |
|----------|-------------|------|---------|
| `generateRcvE2EParams` | Invitation creator | 439 | "used by party initiating connection, Bob in double-ratchet spec" |
| `generateSndE2EParams` | Invitation scanner | 448 | "used by party accepting connection, Alice in double-ratchet spec" |
 
### X3DH Agreement
 
| Function | Who calls it | Line | Comment |
|----------|-------------|------|---------|
| `pqX3dhSnd` | Invitation scanner (joiner) | 466 | "this is used by the peer joining the connection" |
| `pqX3dhRcv` | Invitation creator (initiator) | 482 | "this is used by the peer that created new connection, after receiving the reply" |
 
### Ratchet Initialization
 
| Function | Who calls it | Line | Comment |
|----------|-------------|------|---------|
| `initSndRatchet` | Invitation scanner | 643 | Initializes with a sending ratchet (rcSnd = Just ..., rcRcv = Nothing) |
| `initRcvRatchet` | Invitation creator | 674 | Initializes with NO sending ratchet (rcSnd = Nothing, rcRcv = Nothing) |
 
### Summary Table
 
| | Invitation Creator (Bob) | Invitation Scanner (Alice) |
|---|---|---|
| Generates params with | `generateRcvE2EParams` (line 439) | `generateSndE2EParams` (line 448) |
| X3DH function | `pqX3dhRcv` (line 482) | `pqX3dhSnd` (line 466) |
| Ratchet init | `initRcvRatchet` (line 674) | `initSndRatchet` (line 643) |
| Provides to rcAD | `rk1` (second position) | `sk1` (first position) |
| Initial send capability | No (rcSnd = Nothing) | Yes (rcSnd = Just ...) |
| Sends first message | Waits to receive first | Sends the first encrypted message |
 
**The invitation creator provides rk1 (second in rcAD).**
**The invitation scanner provides sk1 (first in rcAD).**
 
## 6. Exact Code Paths with Line Numbers
 
### rcAD Construction
 
| Location | Line | Code |
|----------|------|------|
| `pqX3dh` definition | 499-508 | Core function that builds `RatchetInitParams` |
| `assocData` construction | 503 | `assocData = Str $ pubKeyBytes sk1 <> pubKeyBytes rk1` |
 
### rcAD Storage
 
| Location | Line | Code |
|----------|------|------|
| `Ratchet` record field | 516 | `rcAD :: Str` with comment "must be the same in both parties ratchets" |
| `initSndRatchet` assignment | 651 | `rcAD = assocData` |
| `initRcvRatchet` assignment | 679 | `rcAD = assocData` |
 
### rcAD Usage in Encryption
 
| Location | Line | Purpose |
|----------|------|---------|
| `rcEncryptHeader` pattern | 904 | Destructures `rcAD = Str rcAD` from Ratchet |
| Header AEAD encryption | 918 | `encryptAEAD rcHKs ehIV ... rcAD (msgHeader ...)` |
| `MsgEncryptKey` storage | 925 | `msgRcAD = rcAD` |
| `rcEncryptMsg` body AEAD | 973 | `encryptAEAD mk iv ... (msgRcAD <> msgEncHeader) msg` |
 
### rcAD Usage in Decryption
 
| Location | Line | Purpose |
|----------|------|---------|
| `rcDecrypt` pattern | 998 | Destructures `rcAD = Str rcAD` from Ratchet |
| Header AEAD decryption | 1152 | `decryptAEAD k ehIV rcAD ehBody ehAuthTag` |
| Body AEAD decryption | 1157 | `decryptAEAD mk iv (rcAD <> emHeader) emBody emAuthTag` |
 
### Agent Integration (Agent.hs)
 
| Location | Line | Purpose |
|----------|------|---------|
| Joiner calls `pqX3dhSnd` | 1104 | `CR.pqX3dhSnd pk1 pk2 pKem e2eRcvParams` |
| Joiner calls `initSndRatchet` | 1106 | `CR.initSndRatchet rcVs rcDHRr rcDHRs rcParams` |
| Creator calls `pqX3dhRcv` | 2917 | `CR.pqX3dhRcv pk1 rcDHRs pKem e2eSndParams` |
| Creator calls `initRcvRatchet` | 2920 | `CR.initRcvRatchet rcVs rcDHRs rcParams pqSupport'` |
| rcAD hash extraction | 2233 | `CR.Ratchet {rcAD = Str rcAD} <- withStore c (\`getRatchet\` connId)` |
| Ratchet re-negotiation | 3189-3197 | Role determined by comparing key hashes |
 
### Ratchet Re-negotiation (Agent.hs:3189-3197)
 
During ratchet re-sync, the roles are **re-determined** by comparing public key hashes:
```haskell
initRatchet rcVs (pk1, pk2, pKem)
  | rkHash (C.publicKey pk1) (C.publicKey pk2) <= rkHashRcv = do
      -- This party becomes "Rcv" (creator role) → calls pqX3dhRcv + initRcvRatchet
  | otherwise = do
      -- This party becomes "Snd" (joiner role) → calls pqX3dhSnd + initSndRatchet
```
This means the rcAD ordering is deterministic even after re-negotiation: it is
determined by the key hash comparison, not the original connection roles.
 
## Key Insight
 
The `s` prefix in `sk1`/`spk1` stands for the X3DH **"sender"** role (the party
who sends the first ratchet message = the joiner), NOT the sender of any particular
message. The `r` prefix in `rk1`/`rpk1` stands for the X3DH **"receiver"** role
(the party who receives the first ratchet message = the initiator). These roles
are fixed at connection setup and determine the permanent rcAD byte ordering.