---
title: "SimpleGo Documentation"
sidebar_position: 0
slug: /
hide_table_of_contents: true
---

# SimpleGo Documentation

**The first native SimpleX protocol implementation for embedded hardware.**

SimpleGo implements the [SimpleX Messaging Protocol](https://simplex.chat) in C on ESP32-S3 microcontrollers, enabling secure, smartphone-free messaging on dedicated hardware devices.

---

## 🎉 Historic Milestone: CONNECTED!

On February 8, 2026, SimpleGo achieved **the world's first SimpleX connection on a microcontroller**. The complete protocol chain is working: TLS → SMP → E2E → Ratchet → Zstd → JSON → KEY → HELLO → CON ✅

---

## Quick Navigation

| Section | Description |
|---------|-------------|
| **[Learn](/learn)** | Understand SimpleX: architecture, encryption layers, and security model |
| **[Specification](/spec)** | Formal protocol spec: SMP, cryptography, agent, XFTP, chat protocol |
| **[Implement](/implement)** | C/ESP32 implementation guide: mbedTLS, libsodium, state machines |
| **[Protocol Analysis](/protocol-analysis/SIMPLEX_PROTOCOL_INDEX)** | 23 sessions of reverse-engineering the SimpleX protocol from Haskell |
| **[Project](/ARCHITECTURE)** | Software architecture, hardware design, and development docs |
| **[Releases](/releases)** | Release notes from v0.1.14-alpha to current |

---

## Project Overview

| | |
|---|---|
| **Platform** | ESP32-S3 (LilyGo T-Deck) |
| **Language** | C (ESP-IDF 5.5) |
| **Crypto** | mbedTLS + libsodium |
| **Protocol** | SimpleX SMP v7+ with Double Ratchet |
| **License** | AGPL-3.0 (code) · CC BY-SA 4.0 (spec) |
| **Repository** | [github.com/cannatoshi/SimpleGo](https://github.com/cannatoshi/SimpleGo) |

---

:::info About this project
SimpleGo is developed by [S.D - IT and More Systems](https://it-and-more-systems.de) and represents the first known third-party implementation of the SimpleX protocol outside the official Haskell codebase.
:::
