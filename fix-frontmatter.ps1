# =============================================================================
# Fix broken frontmatter in docs-site/content
# Run from: C:\Espressif\projects\simplex_client
# =============================================================================

$ErrorActionPreference = "Stop"
$root = Get-Location
$contentDir = Join-Path $root "docs-site/content"

Write-Host "`nFixing all content files..." -ForegroundColor Cyan

function Fix-File($relPath, $content) {
    $fullPath = Join-Path $contentDir $relPath
    $dir = Split-Path $fullPath -Parent
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    [System.IO.File]::WriteAllText($fullPath, $content, [System.Text.Encoding]::UTF8)
    Write-Host "  Fixed: $relPath" -ForegroundColor Green
}

# --- SPEC ---

Fix-File "spec/index.md" @"
---
title: "SimpleX Protocol Specification"
sidebar_position: 1
---

# SimpleX Protocol Specification for ESP32

A complete protocol specification derived from reverse-engineering the SimpleX Messaging Protocol (simplexmq Haskell codebase) for native implementation on ESP32 microcontrollers.

:::caution Work in Progress
This specification is actively being developed alongside the SimpleGo implementation. Content will be expanded as more protocol details are verified through implementation.
:::
"@

Fix-File "spec/terminology.md" @"
---
title: "Terminology"
sidebar_position: 2
---

# Terminology

:::note Placeholder
Glossary of SimpleX protocol terms. Will be expanded.
:::
"@

Fix-File "spec/architecture.md" @"
---
title: "Architecture"
sidebar_position: 3
---

# Architecture

:::note Placeholder
Protocol architecture overview. Will be expanded.
:::
"@

Fix-File "spec/transport.md" @"
---
title: "Transport Layer"
sidebar_position: 4
---

# Transport Layer (TLS 1.3)

:::note Placeholder
TLS 1.3 transport layer details. Will be expanded.
:::
"@

Fix-File "spec/smp-commands.md" @"
---
title: "SMP Commands"
sidebar_position: 5
---

# SMP Commands

:::note Placeholder
SMP command reference. Will be expanded.
:::
"@

Fix-File "spec/encryption/server-transport.md" @"
---
title: "Layer 3: Server Transport Encryption"
sidebar_position: 1
---

# Layer 3: Server Transport Encryption

:::note Placeholder
Server transport encryption (NaCl crypto_box). Will be expanded.
:::
"@

Fix-File "spec/encryption/e2e-encryption.md" @"
---
title: "Layer 2: E2E Encryption"
sidebar_position: 2
---

# Layer 2: End-to-End Encryption

:::note Placeholder
End-to-end encryption layer. Will be expanded.
:::
"@

Fix-File "spec/encryption/double-ratchet.md" @"
---
title: "Layer 1: Double Ratchet"
sidebar_position: 3
---

# Layer 1: Double Ratchet

:::note Placeholder
Double Ratchet algorithm implementation. Will be expanded.
:::
"@

Fix-File "spec/agent-protocol.md" @"
---
title: "Agent Protocol"
sidebar_position: 6
---

# Agent Protocol

:::note Placeholder
Agent protocol and connection management. Will be expanded.
:::
"@

Fix-File "spec/wire-format.md" @"
---
title: "Wire Format"
sidebar_position: 7
---

# Wire Format

:::note Placeholder
Binary wire format specification. Will be expanded.
:::
"@

Fix-File "spec/message-types.md" @"
---
title: "Message Types"
sidebar_position: 8
---

# Message Types

:::note Placeholder
Message type definitions. Will be expanded.
:::
"@

Fix-File "spec/versioning.md" @"
---
title: "Versioning"
sidebar_position: 9
---

# Versioning

:::note Placeholder
Version negotiation and compatibility. Will be expanded.
:::
"@

Fix-File "spec/security.md" @"
---
title: "Security Model"
sidebar_position: 10
---

# Security Model

:::note Placeholder
Security model and threat analysis. Will be expanded.
:::
"@

Fix-File "spec/appendices/test-vectors.md" @"
---
title: "Test Vectors"
sidebar_position: 1
---

# Test Vectors

:::note Placeholder
Cryptographic test vectors for verification.
:::
"@

Fix-File "spec/appendices/implementation-notes.md" @"
---
title: "Implementation Notes"
sidebar_position: 2
---

# Implementation Notes

:::note Placeholder
ESP32-specific implementation notes.
:::
"@

Fix-File "spec/appendices/haskell-reference.md" @"
---
title: "Haskell Reference"
sidebar_position: 3
---

# Haskell Source Reference

:::note Placeholder
Key Haskell source code references.
:::
"@

Fix-File "spec/appendices/changelog.md" @"
---
title: "Changelog"
sidebar_position: 4
---

# Specification Changelog

:::note Placeholder
Specification changelog.
:::
"@

# --- PROTOCOL ANALYSIS ---

Fix-File "protocol-analysis/index.md" @"
---
title: "Protocol Analysis"
sidebar_position: 1
---

# Protocol Analysis

Session-by-session documentation of reverse-engineering the SimpleX Messaging Protocol for native ESP32 implementation.

> **22 sessions** - **31+ bugs found and fixed** - **83 key learnings**

## Sessions Overview

| Sessions | Topic | Key Achievement |
|----------|-------|-----------------|
| [1-2](/protocol-analysis/sessions/sessions-1-2) | Foundation and TLS | First SMP connection established |
| [3-4](/protocol-analysis/sessions/sessions-3-4) | Wire Format | Binary parsing working |
| [5-6](/protocol-analysis/sessions/sessions-5-6) | Encryption Layers | NaCl decryption working |
| [7](/protocol-analysis/sessions/session-7) | Agent Protocol | Agent message parsing |
| [8](/protocol-analysis/sessions/session-8) | Breakthrough | First end-to-end decryption |
| [9](/protocol-analysis/sessions/session-9) | X3DH Key Agreement | Shared secret derivation |
| [10](/protocol-analysis/sessions/session-10) | Double Ratchet | Ratchet state machine |
| [11](/protocol-analysis/sessions/session-11) | Message Decryption | Full message chain |
| [12](/protocol-analysis/sessions/session-12) | Full Chain | Complete receive path |
| [13](/protocol-analysis/sessions/session-13) | Stabilization | Edge case handling |
| [14](/protocol-analysis/sessions/session-14) | Edge Cases | Robustness improvements |
| [15](/protocol-analysis/sessions/session-15) | Nonce Handling | Nonce management fixes |
| [16](/protocol-analysis/sessions/session-16) | Key Management | Key lifecycle |
| [17](/protocol-analysis/sessions/session-17) | Refinement | Protocol refinement |
| [18](/protocol-analysis/sessions/session-18) | Robustness | Error handling |
| [19](/protocol-analysis/sessions/session-19) | PQ Crypto | Post-quantum KEM |
| [20](/protocol-analysis/sessions/session-20) | Reply Queue | Bidirectional messaging |
| [21](/protocol-analysis/sessions/session-21) | Major Bugs | Critical bug fixes |
| [22](/protocol-analysis/sessions/session-22) | Modern Protocol | v2 senderCanSecure |

## Quick Links

- [Quick Reference](/protocol-analysis/quick-reference) - Constants, versions, key learnings
- [Bug Tracker](/protocol-analysis/bug-tracker) - All 31+ bugs documented
"@

Fix-File "protocol-analysis/quick-reference.md" @"
---
title: "Quick Reference"
sidebar_position: 2
---

# Quick Reference

:::note Placeholder
Content will be migrated from docs/protocol-analysis/QUICK_REFERENCE.md
:::
"@

Fix-File "protocol-analysis/bug-tracker.md" @"
---
title: "Bug Tracker"
sidebar_position: 3
---

# Bug Tracker

:::note Placeholder
Content will be migrated from docs/protocol-analysis/BUG_TRACKER.md
:::
"@

# Session files
$sessionData = @(
    @("sessions-1-2", "Sessions 1-2: Foundation and TLS", 1),
    @("sessions-3-4", "Sessions 3-4: Wire Format Deep Dive", 2),
    @("sessions-5-6", "Sessions 5-6: Encryption Layers", 3),
    @("session-7",    "Session 7: Agent Protocol", 4),
    @("session-8",    "Session 8: The Breakthrough", 5),
    @("session-9",    "Session 9: X3DH Key Agreement", 6),
    @("session-10",   "Session 10: Double Ratchet", 7),
    @("session-11",   "Session 11: Message Decryption", 8),
    @("session-12",   "Session 12: Full Chain", 9),
    @("session-13",   "Session 13: Stabilization", 10),
    @("session-14",   "Session 14: Edge Cases", 11),
    @("session-15",   "Session 15: Nonce Handling", 12),
    @("session-16",   "Session 16: Key Management", 13),
    @("session-17",   "Session 17: Refinement", 14),
    @("session-18",   "Session 18: Robustness", 15),
    @("session-19",   "Session 19: Post-Quantum Crypto", 16),
    @("session-20",   "Session 20: Reply Queue", 17),
    @("session-21",   "Session 21: Major Bug Fixes", 18),
    @("session-22",   "Session 22: Modern Protocol Discovery", 19)
)

foreach ($s in $sessionData) {
    $slug = $s[0]
    $title = $s[1]
    $pos = $s[2]
    $fileContent = "---`ntitle: `"$title`"`nsidebar_position: $pos`n---`n`n# $title`n`n:::note Placeholder`nContent will be migrated from the corresponding analysis document.`n:::`n"
    Fix-File "protocol-analysis/sessions/$slug.md" $fileContent
}

# --- PROJECT DOCS ---

Fix-File "project/index.md" @"
---
title: "Project Documentation"
sidebar_position: 1
---

# SimpleGo Project Documentation

Technical documentation for the SimpleGo project - a native ESP32 implementation of the SimpleX messaging protocol.

## Software

Architecture, build system, cryptography implementation, and development guides.

## Hardware

Hardware abstraction layer, device tiers, PCB designs, and enclosure specifications for dedicated SimpleX messaging devices.
"@

$projectData = @(
    @("project/architecture.md",      "Software Architecture",    2,  "Source: docs/ARCHITECTURE.md"),
    @("project/build-system.md",      "Build System",             3,  "Source: docs/BUILD_SYSTEM.md"),
    @("project/development.md",       "Development Guide",        4,  "Source: docs/DEVELOPMENT.md"),
    @("project/technical.md",         "Technical Details",         5,  "Source: docs/TECHNICAL.md"),
    @("project/crypto.md",            "Cryptography",             6,  "Source: docs/CRYPTO.md"),
    @("project/protocol.md",          "Protocol Implementation",  7,  "Source: docs/PROTOCOL.md"),
    @("project/wire-format.md",       "Wire Format Implementation", 8, "Source: docs/WIRE_FORMAT.md"),
    @("project/security-model.md",    "Security Model",           9,  "Source: docs/SECURITY_MODEL.md"),
    @("project/bugs.md",              "Known Bugs",               10, "Source: docs/BUGS.md"),
    @("project/devnotes.md",          "Developer Notes",          11, "Source: docs/DEVNOTES.md"),
    @("project/adding-new-device.md", "Adding New Devices",       20, "Source: docs/ADDING_NEW_DEVICE.md"),
    @("project/simplex-vs-matrix.md", "SimpleX vs Matrix",        21, "Source: docs/SIMPLEX_VS_MATRIX.md"),
    @("project/sponsors.md",          "Sponsors",                 22, "Source: docs/SPONSORS.md"),
    @("project/disclaimer.md",        "Disclaimer",               23, "Source: docs/DISCLAIMER.md"),
    @("project/trademark.md",         "Trademark",                24, "Source: docs/TRADEMARK.md")
)

foreach ($d in $projectData) {
    $path = $d[0]; $title = $d[1]; $pos = $d[2]; $note = $d[3]
    $fileContent = "---`ntitle: `"$title`"`nsidebar_position: $pos`n---`n`n# $title`n`n:::note Placeholder`n$note`n:::`n"
    Fix-File $path $fileContent
}

$hwData = @(
    @("project/hardware/overview.md",              "Hardware Overview",       1, "Source: docs/hardware/HARDWARE_OVERVIEW.md"),
    @("project/hardware/hal-architecture.md",      "HAL Architecture",       2, "Source: docs/hardware/HAL_ARCHITECTURE.md"),
    @("project/hardware/hardware-tiers.md",        "Hardware Tiers",         3, "Source: docs/hardware/HARDWARE_TIERS.md"),
    @("project/hardware/component-selection.md",   "Component Selection",    4, "Source: docs/hardware/COMPONENT_SELECTION.md"),
    @("project/hardware/pcb-design.md",            "PCB Design",             5, "Source: docs/hardware/PCB_DESIGN.md"),
    @("project/hardware/enclosure-design.md",      "Enclosure Design",       6, "Source: docs/hardware/ENCLOSURE_DESIGN.md"),
    @("project/hardware/security-architecture.md", "Security Architecture",  7, "Source: docs/hardware/SECURITY_ARCHITECTURE.md")
)

foreach ($d in $hwData) {
    $path = $d[0]; $title = $d[1]; $pos = $d[2]; $note = $d[3]
    $fileContent = "---`ntitle: `"$title`"`nsidebar_position: $pos`n---`n`n# $title`n`n:::note Placeholder`n$note`n:::`n"
    Fix-File $path $fileContent
}

# --- RELEASES ---

Fix-File "releases/index.md" @"
---
title: "Releases"
sidebar_position: 1
slug: /releases
---

# SimpleGo Releases

| Version | Highlights |
|---------|------------|
| [v0.1.16-alpha](/releases/v0-1-16-alpha) | HAL Architecture, Multi-Device |
| [v0.1.15-alpha](/releases/v0-1-15-alpha) | Double Ratchet, X3DH |
| [v0.1.14-alpha](/releases/v0-1-14-alpha) | Modular Architecture |
"@

Fix-File "releases/v0-1-16-alpha.md" @"
---
title: "v0.1.16-alpha"
sidebar_position: 2
---

# Release v0.1.16-alpha

:::note Placeholder
Source: docs/release-info/v0.1.16-alpha.md
:::
"@

Fix-File "releases/v0-1-15-alpha.md" @"
---
title: "v0.1.15-alpha"
sidebar_position: 3
---

# Release v0.1.15-alpha

:::note Placeholder
Source: docs/release-info/v0.1.15-alpha.md
:::
"@

Fix-File "releases/v0-1-14-alpha.md" @"
---
title: "v0.1.14-alpha"
sidebar_position: 4
---

# Release v0.1.14-alpha

:::note Placeholder
Source: docs/release-info/v0.1.14-alpha.md
:::
"@

# --- LEGAL ---

Fix-File "legal/impressum.md" @"
---
title: "Legal Notice (Impressum)"
sidebar_position: 99
unlisted: true
---

# Legal Notice (Impressum)

Required under German law (TMG, MStV).

**S.D - IT and More Systems**

*Full legal details will be added here.*
"@

Fix-File "legal/privacy.md" @"
---
title: "Privacy Policy"
sidebar_position: 100
unlisted: true
---

# Privacy Policy

*Full privacy policy will be added here.*
"@

# --- HOMEPAGE ---

Fix-File "index.md" @"
---
title: "SimpleGo Documentation"
sidebar_position: 1
slug: /
---

# SimpleGo Documentation

**SimpleGo** is a native ESP32-S3 implementation of the SimpleX Messaging Protocol - the first known third-party implementation outside the official Haskell codebase.

## Documentation Sections

### [Specification](/spec)
Complete protocol specification for implementing SimpleX on embedded hardware. Covers transport, encryption layers, wire format, and agent protocol.

### [Protocol Analysis](/protocol-analysis)
Session-by-session journey of reverse-engineering the SimpleX protocol. 22 sessions documenting discoveries, bugs, and breakthroughs.

### [Project Documentation](/project)
Software architecture, hardware designs, build system, and development guides for the SimpleGo project.

### [Releases](/releases)
Release notes for all SimpleGo versions.

---

## Quick Links

| Resource | Description |
|----------|-------------|
| [GitHub Repository](https://github.com/cannatoshi/SimpleGo) | Source code (AGPL-3.0) |
| [SimpleX Chat](https://simplex.chat) | Official SimpleX project |
| [Bug Tracker](/protocol-analysis/bug-tracker) | All 31+ protocol bugs found and fixed |
| [Quick Reference](/protocol-analysis/quick-reference) | Constants, versions, key learnings |

---

:::info License
The SimpleGo specification is licensed under [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).
The SimpleGo software is licensed under [AGPL-3.0](https://github.com/cannatoshi/SimpleGo/blob/main/LICENSE).
:::
"@

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  All files fixed!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "`n  Now run:  npm run start" -ForegroundColor Green
Write-Host ""
