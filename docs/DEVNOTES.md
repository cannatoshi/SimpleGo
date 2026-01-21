# SimpleGo Development Notes

> Current development state and session notes

---

## Current Status (January 21, 2026)

### Version: v0.1.14-alpha

### 🏗️ Modular Architecture + Peer Connection!

Monolithic main.c refactored into 8 modules — Peer server connection working!

**Latest Output:**
```
🔗 SIMPLEX CONTACT LINKS ════════════════════════════════
📱 [0] Test
🌐 https://simplex.chat/contact#/?v=2-7&smp=...

[SimpleX App scans link]

💬 MESSAGE for [Test]!
📋 Agent: Version=7, Type='I'
📡 Peer: smp15.simplex.im:5223
🔑 DH Key extracted (32 bytes)
🔌 Connecting to peer server...
✅ Peer TLS OK (ALPN: smp/1)
✅ Peer Handshake OK
📤 Sending AgentConfirmation...
✅ Server: OK
```

---

## Working Features

- ✅ **Modular Architecture** — 8 modules, ~350 line main.c
- ✅ **smp_peer.c Module** — Peer connection functions
- ✅ **peer_connect()** — TLS to peer server
- ✅ **send_agent_confirmation()** — SEND to peer queue
- ✅ **Auto-Connect** — Parser triggers on Invitation
- ✅ **Server Accepts** — "OK" response
- 🔧 **App "Connected"** — Format issue pending

---

## Module Structure

```
main/
├── main.c              (~350 lines)
├── smp_globals.c       (~25 lines)
├── smp_utils.c         (~100 lines)
├── smp_crypto.c        (~80 lines)
├── smp_network.c       (~160 lines)
├── smp_contacts.c      (~380 lines)
├── smp_parser.c        (~260 lines)
├── smp_peer.c          (~220 lines) ← NEW!
└── include/
    ├── smp_types.h
    ├── smp_utils.h
    ├── smp_crypto.h
    ├── smp_network.h
    ├── smp_contacts.h
    ├── smp_parser.h
    └── smp_peer.h      ← NEW!
```

---

## Bug Fixes

### 1. tcp_connect Naming Conflict

**Problem:** `multiple definition of tcp_connect`

**Cause:** Collision with lwip's `tcp_connect`

**Solution:** Renamed to `smp_tcp_connect()` everywhere

### 2. DH Key Extraction from Invitation

**Problem:** DH Keys not decoded properly

**Cause:** Invitation URIs use **Standard Base64** (`+`, `/`, `=`), NOT Base64URL!

**Solution:**
```c
// Strip '=' padding
while (len > 0 && dh_clean[len - 1] == '=') dh_clean[--len] = '\0';

// Convert +/ to -_ (Standard → URL)
for (int x = 0; x < len; x++) {
    if (dh_clean[x] == '+') dh_clean[x] = '-';
    if (dh_clean[x] == '/') dh_clean[x] = '_';
}
```

---

## New Discoveries (v0.1.14)

| # | Discovery |
|---|-----------|
| 16 | DH Keys in Invitation URIs: Standard Base64, NOT Base64URL |
| 17 | AgentConfirmation format: `(agentVersion, 'C', e2eEncryption_, Tail encConnInfo)` |
| 18 | Maybe Encoding: `'0'` = Nothing, `'1'` + data = Just |
| 19 | Each peer has own SMP server → separate TLS connection required |
| 20 | SEND to Peer: queue_id as entityId, no signature needed |

---

## smp_peer.c Functions

```c
// Connect to peer's SMP server
bool peer_connect(const char *host, int port);

// Disconnect from peer
void peer_disconnect(void);

// Perform SMP handshake with peer
bool peer_handshake(void);

// Send AgentConfirmation to peer's queue
bool send_agent_confirmation(contact_t *contact);
```

---

## Auto-Connect Flow

```c
// In smp_parser.c after parsing Invitation:
if (pending_peer.valid && pending_peer.has_dh) {
    ESP_LOGI(TAG, "🔌 Auto-connecting to peer...");
    
    if (peer_connect(pending_peer.host, pending_peer.port)) {
        send_agent_confirmation(contact);
        peer_disconnect();
    }
}
```

---

## Current Issue: App Not "Connected"

### Symptom

Server accepts Confirmation with "OK", but SimpleX App doesn't show "Connected".

### Analysis

From Haskell source:
```haskell
data AMessage =
  ...
  | AgentConfirmation {
      agentVersion :: Version,
      e2eEncryption_ :: Maybe (E2ERatchetParams 'C448),
      encConnInfo :: ByteString
    }
```

### Hypothesis

`encConnInfo` needs more than just our DH Key:
- Profile information?
- Ratchet initialization?
- Proper encryption with peer's DH?

### Next Steps

1. Analyze `encConnInfo` encoding in Haskell
2. Check if profile data needed
3. May need Double Ratchet init

---

## Build Environment

```powershell
cd C:\Espressif\projects\simplex_client
idf.py build flash monitor -p COM5
```

---

## Feature Matrix v0.1.14

```
═══════════════════════════════════════════════════════════════
✅ Modular Architecture (8 modules)
✅ Peer Server TLS Connection
✅ SMP Handshake with Peer
✅ AgentConfirmation Sent
✅ Server Response: OK
═══════════════════════════════════════════════════════════════
🔧 App Shows "Connected" (encConnInfo format)
⏳ Double Ratchet Implementation
⏳ UI Components
═══════════════════════════════════════════════════════════════
```

---

## New Files

| File | Purpose |
|------|---------|
| `.gitignore` | build/, managed_components/, sdkconfig.old |
| `docs/ARCHITECTURE.md` | Module documentation |
| `docs/release-info/v0.1.14-alpha.md` | Detailed release notes |

---

*Last updated: January 21, 2026 — v0.1.14-alpha*
