# SimpleGo Protocol Analysis

## Complete Development Documentation

This directory contains the complete, unabridged documentation of SimpleGo's development journey - the **world's first native SMP protocol implementation** outside the official SimpleX Haskell codebase.

---

## Current Status (2026-01-30 Session 13)

```
Session 13 - E2E Crypto Deep Analysis:
- Fixed: Message parsing with correct offsets
- Discovered: HSalsa20 difference (Haskell vs libsodium)
- Discovered: MAC position [MAC][Cipher] vs [Cipher][MAC]
- Tested: 5 different crypto approaches - ALL FAILED
- Found: SMPConfirmation contains e2ePubKey
- Observed: Android vs Desktop apps behave differently

Bug #18: Reply Queue E2E - Still failing, root cause investigation continues
Next: Parse SMPConfirmation to extract App's e2ePubKey
```

---

## Acknowledgments and Respect

**This project would not be possible without the incredible work of the SimpleX team.**

SimpleX Chat represents a groundbreaking achievement in privacy-preserving communication technology. The protocol design is elegant, well-thought-out, and prioritizes user privacy above all else. We have the deepest respect for:

- **Evgeny Poberezkin** and the entire SimpleX Chat team
- The brilliant cryptographic design combining X3DH, Double Ratchet, and post-quantum algorithms
- The commitment to open source (AGPL-3.0) that made this project possible
- The comprehensive Haskell implementation that served as our reference

**Links:**
- SimpleX Chat: https://simplex.chat
- SimpleX GitHub: https://github.com/simplex-chat

---

## Document Structure

| Document | Lines | Description |
|----------|-------|-------------|
| [01_SIMPLEX_PROTOCOL_INDEX.md](01_SIMPLEX_PROTOCOL_INDEX.md) | ~100 | Navigation index |
| [02_SIMPLEX_STATUS.md](02_SIMPLEX_STATUS.md) | ~250 | Current status summary |
| [03_PART1_INTRO_SESSIONS_1-2.md](03_PART1_INTRO_SESSIONS_1-2.md) | ~2300 | Foundation, TLS 1.3, basic SMP |
| [04_PART2_SESSIONS_3-4.md](04_PART2_SESSIONS_3-4.md) | ~1000 | Wire format, bugs #1-8 |
| [05_PART3_SESSIONS_5-6.md](05_PART3_SESSIONS_5-6.md) | ~800 | X448 breakthrough, SMPQueueInfo |
| [06_PART4_SESSION_7.md](06_PART4_SESSION_7.md) | ~3200 | AES-GCM verification, Tail encoding |
| [07_PART5_SESSION_8_BREAKTHROUGH.md](07_PART5_SESSION_8_BREAKTHROUGH.md) | ~400 | AgentConfirmation works! |
| [08_PART6_SESSION_9.md](08_PART6_SESSION_9.md) | ~450 | Reply Queue HSalsa20 fix |
| [09_PART7_SESSION_10.md](09_PART7_SESSION_10.md) | ~400 | cmNonce fix, app "connecting" |
| [10_PART8_SESSION_11.md](10_PART8_SESSION_11.md) | ~400 | Regression & Recovery |
| [11_PART9_SESSION_12.md](11_PART9_SESSION_12.md) | ~400 | E2E Keypair Fix Attempt |
| [12_PART10_SESSION_13.md](12_PART10_SESSION_13.md) | ~700 | E2E Crypto Deep Analysis |
| [BUG_TRACKER.md](BUG_TRACKER.md) | ~1500 | Complete bug documentation (18 bugs) |
| [QUICK_REFERENCE.md](QUICK_REFERENCE.md) | ~900 | Constants, wire formats, crypto differences |

**Total: ~16,000 lines of detailed protocol analysis**

---

## Project Timeline

| Session | Date | Milestone | Bugs Fixed |
|---------|------|-----------|------------|
| 1-3 | Dec 2025 | Foundation, TLS 1.3, Basic SMP | - |
| 4 | Jan 23, 2026 | Wire format analysis | #1-8 |
| 5 | Jan 24, 2026 | X448 byte-order breakthrough | #9 |
| 6 | Jan 24, 2026 | SMPQueueInfo encoding | #10-12 |
| 7 | Jan 24-25, 2026 | Crypto verification, SimpleX contact | - |
| 8 | Jan 27, 2026 | AgentConfirmation WORKS! | #13-14 |
| 9 | Jan 27, 2026 | Reply Queue HSalsa20 fix | #15-16 |
| 10C | Jan 28, 2026 | cmNonce fix, app "connecting" | #17 |
| 11 | Jan 30, 2026 | Regression & Recovery | - |
| 12 | Jan 30, 2026 | E2E Keypair Analysis | - |
| **13** | **Jan 30, 2026** | **E2E Crypto Deep Analysis** | **#18 (open)** |

---

## Session 13 Key Discoveries

### 1. HSalsa20 Difference

| Step | Haskell | libsodium |
|------|---------|-----------|
| 1 | DH(pub, priv) -> secret | DH(pub, priv) -> secret |
| 2 | XSalsa20(secret, nonce, msg) | **HSalsa20(secret)** -> key |
| 3 | - | XSalsa20(key, nonce, msg) |

**libsodium has an EXTRA HSalsa20 step!**

### 2. MAC Position Difference

| Format | Layout |
|--------|--------|
| **Haskell** | `[MAC 16 bytes][Ciphertext]` |
| **libsodium** | `[Ciphertext][MAC 16 bytes]` |

### 3. Correct Message Structure

```
[12-13]  phVersion (00 04)
[14]     Maybe tag: '1' = Just (key present!)
[15]     SPKI length = 44 (0x2c)
[16-59]  X25519 SPKI (44 bytes)
[60-83]  cmNonce (24 bytes)
[84+]    cmEncBody
```

### 4. All 5 Crypto Tests Failed

1. crypto_box_open_easy + e2e_private
2. crypto_box_open_easy + rcv_dh_private  
3. crypto_secretbox_open_easy (direct)
4. crypto_secretbox_open_easy (MAC reordered)
5. crypto_secretbox_open_detached (MAC separate)

---

## Quick Navigation

### By Bug Number

| Bug | Description | Document |
|-----|-------------|----------|
| #1-8 | Wire format bugs | [Part 2](04_PART2_SESSIONS_3-4.md) |
| #9 | wolfSSL X448 byte-order | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #10-12 | SMPQueueInfo encoding | [Part 3](05_PART3_SESSIONS_5-6.md) |
| #13-14 | AAD prefix, IV order | [Part 5](07_PART5_SESSION_8_BREAKTHROUGH.md) |
| #15-16 | HSalsa20, A_CRYPTO | [Part 6](08_PART6_SESSION_9.md) |
| #17 | cmNonce instead of msgId | [Part 7](09_PART7_SESSION_10.md) |
| **#18** | **Reply Queue E2E (open)** | [**Part 10**](12_PART10_SESSION_13.md) |

---

## License

This documentation is part of SimpleGo, licensed under AGPL-3.0.

---

*Last updated: January 30, 2026 - Session 13 (E2E Crypto Deep Analysis)*
