# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (January 20, 2025)

### Version: 0.4.1 (internal v4.1)

**Working Features:**
- ✅ TLS 1.3 connection with ChaCha20-Poly1305
- ✅ ALPN negotiation "smp/1"
- ✅ SMP handshake (ServerHello/ClientHello)
- ✅ Certificate chain parsing
- ✅ keyHash computation from CA certificate
- ✅ Ed25519 key generation (libsodium)
- ✅ X25519 key generation
- ✅ SPKI key encoding
- ✅ NEW command with IDS response
- ✅ SUB command with OK response

**Latest Output:**
```
I (5765) SMP: ========================================
I (5765) SMP:   SimpleGo v0.4.1 - Native SMP Client
I (5765) SMP:   Part of Sentinel Secure Messenger Suite
I (5765) SMP: ========================================
I (6088) SMP: [1/7] Establishing TCP + TLS connection...
I (6288) SMP:       TLS OK! ALPN: smp/1
I (6288) SMP: [2/7] Waiting for ServerHello...
I (6488) SMP:       Server versions: 6-8
I (6488) SMP: [3/7] Sending ClientHello...
I (6598) SMP: [4/7] Generating keypairs...
I (6598) SMP: [5/7] Sending NEW command...
I (6688) SMP:       ✅ Signature verified locally
I (6688) SMP:       NEW command sent! (196 bytes)
I (6688) SMP: [6/7] Waiting for IDS response...
I (6888) SMP:   🎉🎉🎉 QUEUE CREATED! 🎉🎉🎉
I (6888) SMP:   📥 RecipientId (24 bytes): e1c77e711e254cab...
I (6898) SMP:   📤 SenderId (24 bytes): 6ce4d1233896d024...
I (6908) SMP:   🔑 ServerDhKey (44 bytes SPKI)
I (6908) SMP: [7/7] Sending SUB command...
I (7158) SMP:   ✅ SUBSCRIBED! Ready to receive messages.
```

---

## Next Steps

### Immediate (This Week)

1. **SEND Command Implementation**
   - Build SEND message format
   - Handle msgFlags parameter
   - Test message delivery

2. **Message Reception Loop**
   - Keep connection alive
   - Parse incoming MSG responses
   - Implement ACK command

### Short-term (This Month)

3. **Connection Management**
   - Reconnection on disconnect
   - Keepalive mechanism
   - Multiple queue support

4. **Key Persistence**
   - Store keys in NVS
   - Restore queue subscriptions after reboot

### Medium-term (Next Month)

5. **Double Ratchet**
   - X3DH key agreement
   - Message encryption/decryption
   - Ratchet state management

---

## Build Environment

**Windows (ESP-IDF 5.5 PowerShell):**
```powershell
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5
```

**WSL (for Haskell source analysis):**
```bash
cd ~/simplexmq
grep -r "pattern" src --include="*.hs"
```

---

## Key Technical References

### Haskell Source Files

| File | Purpose | Key Functions |
|------|---------|---------------|
| Protocol.hs | Command definitions | NEW, SUB, SEND patterns |
| Transport.hs | Block framing | tPutBlock, tGetBlock |
| Client.hs | Client logic | createSMPQueue |
| Crypto.hs | Signatures | signSMP, verifySMP |

### Useful grep Commands

```bash
# Find signature encoding
grep -r "signSMP\|smpEncode" --include="*.hs"

# Find command structure
grep -r "pattern NEW\|pattern SUB" --include="*.hs"

# Find transmission format
grep -r "tEncodeAuth" --include="*.hs"
```

---

## Known Issues

### Resolved

| Issue | Solution | Date |
|-------|----------|------|
| ERR BLOCK | Use command block format for commands | Jan 19 |
| ERR CMD SYNTAX | Add subMode parameter | Jan 19 |
| ERR AUTH | Switch to libsodium | Jan 19 |
| Wrong keyHash | Use CA cert (2nd in chain) | Jan 18 |

### Open

| Issue | Status | Notes |
|-------|--------|-------|
| Certificate verification | TODO | Currently VERIFY_NONE |
| Message encryption | TODO | Double Ratchet needed |
| UI | TODO | T-Deck display integration |

---

## Test Servers

| Server | Host | Port | Status |
|--------|------|------|--------|
| SimpleX Flux 3 | smp3.simplexonflux.com | 5223 | ✅ Working |
| SimpleX Flux 4 | smp4.simplexonflux.com | 5223 | Untested |
| SimpleX Official | smp.simplex.im | 5223 | Untested |

---

## Session Log

### January 20, 2025

- Documentation overhaul
- Created comprehensive README, CHANGELOG, ROADMAP
- Added PROTOCOL.md with full protocol documentation
- Added TECHNICAL.md with key learnings
- Restructured project for GitHub

### January 19, 2025

- Major breakthrough: NEW command working!
- Fixed Ed25519 signature issue (Monocypher → libsodium)
- Implemented SUB command
- Full queue lifecycle now working

### January 18, 2025

- Handshake working
- Discovered keyHash must use CA certificate
- Implemented certificate chain parsing

### January 17, 2025

- TLS 1.3 connection established
- ALPN negotiation working
- Block format implemented

---

*Last updated: January 20, 2025*
