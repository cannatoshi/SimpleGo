# =============================================================================
# SimpleGo Docs-Site Setup Script
# Creates docs-site/ folder with Docusaurus + GitHub Pages auto-deploy
# Does NOT modify any existing files in docs/
# =============================================================================
# Run from: C:\Espressif\projects\simplex_client

$ErrorActionPreference = "Stop"
$root = Get-Location

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  SimpleGo Docs-Site Setup" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# =============================================================================
# 1. CREATE DIRECTORY STRUCTURE
# =============================================================================
Write-Host "[1/6] Creating directory structure..." -ForegroundColor Yellow

$dirs = @(
    "docs-site",
    "docs-site/src/css",
    "docs-site/src/pages",
    "docs-site/src/components",
    "docs-site/static/img",
    "docs-site/content",
    "docs-site/content/spec",
    "docs-site/content/spec/encryption",
    "docs-site/content/spec/appendices",
    "docs-site/content/protocol-analysis",
    "docs-site/content/protocol-analysis/sessions",
    "docs-site/content/project",
    "docs-site/content/project/hardware",
    "docs-site/content/releases",
    "docs-site/content/legal",
    ".github/workflows"
)

foreach ($dir in $dirs) {
    $path = Join-Path $root $dir
    if (-not (Test-Path $path)) {
        New-Item -ItemType Directory -Path $path -Force | Out-Null
        Write-Host "  + $dir" -ForegroundColor Green
    } else {
        Write-Host "  = $dir (exists)" -ForegroundColor DarkGray
    }
}

# =============================================================================
# 2. DOCUSAURUS CONFIG FILES
# =============================================================================
Write-Host "`n[2/6] Creating Docusaurus config files..." -ForegroundColor Yellow

# --- package.json ---
@'
{
  "name": "simplego-docs",
  "version": "0.0.0",
  "private": true,
  "scripts": {
    "docusaurus": "docusaurus",
    "start": "docusaurus start",
    "build": "docusaurus build",
    "swizzle": "docusaurus swizzle",
    "deploy": "docusaurus deploy",
    "clear": "docusaurus clear",
    "serve": "docusaurus serve"
  },
  "dependencies": {
    "@docusaurus/core": "^3.9.2",
    "@docusaurus/preset-classic": "^3.9.2",
    "prism-react-renderer": "^2.3.0",
    "react": "^19.0.0",
    "react-dom": "^19.0.0"
  },
  "devDependencies": {
    "@docusaurus/module-type-aliases": "^3.9.2",
    "@docusaurus/tsconfig": "^3.9.2",
    "typescript": "~5.7.0"
  },
  "browserslist": {
    "production": [">0.5%", "not dead", "not op_mini all"],
    "development": ["last 3 chrome version", "last 3 firefox version", "last 5 safari version"]
  },
  "engines": {
    "node": ">=18.0"
  }
}
'@ | Set-Content -Path (Join-Path $root "docs-site/package.json") -Encoding UTF8
Write-Host "  + package.json" -ForegroundColor Green

# --- tsconfig.json ---
@'
{
  "extends": "@docusaurus/tsconfig",
  "compilerOptions": {
    "baseUrl": "."
  }
}
'@ | Set-Content -Path (Join-Path $root "docs-site/tsconfig.json") -Encoding UTF8
Write-Host "  + tsconfig.json" -ForegroundColor Green

# --- docusaurus.config.ts ---
@'
import {themes as prismThemes} from 'prism-react-renderer';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

const config: Config = {
  title: 'SimpleGo Docs',
  tagline: 'Native SimpleX Protocol Implementation for ESP32',
  favicon: 'img/favicon.ico',

  future: {
    v4: true,
  },

  url: 'https://docs.simplego.dev',
  baseUrl: '/',

  organizationName: 'cannatoshi',
  projectName: 'SimpleGo',
  deploymentBranch: 'gh-pages',
  trailingSlash: false,

  onBrokenLinks: 'throw',
  onBrokenMarkdownLinks: 'warn',

  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          path: 'content',
          sidebarPath: './sidebars.ts',
          editUrl: 'https://github.com/cannatoshi/SimpleGo/tree/main/docs-site/',
          routeBasePath: '/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  themeConfig: {
    image: 'img/simplex-spec-social.png',
    colorMode: {
      defaultMode: 'dark',
      respectPrefersColorScheme: true,
    },
    navbar: {
      title: 'SimpleGo',
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'specSidebar',
          position: 'left',
          label: 'Specification',
        },
        {
          type: 'docSidebar',
          sidebarId: 'analysisSidebar',
          position: 'left',
          label: 'Protocol Analysis',
        },
        {
          type: 'docSidebar',
          sidebarId: 'projectSidebar',
          position: 'left',
          label: 'Project Docs',
        },
        {
          type: 'docSidebar',
          sidebarId: 'releasesSidebar',
          position: 'left',
          label: 'Releases',
        },
        {
          href: 'https://github.com/cannatoshi/SimpleGo',
          label: 'GitHub',
          position: 'right',
        },
        {
          href: 'https://simplex.chat',
          label: 'SimpleX Chat',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Specification',
          items: [
            { label: 'Architecture', to: '/spec/architecture' },
            { label: 'Wire Format', to: '/spec/wire-format' },
            { label: 'Double Ratchet', to: '/spec/encryption/double-ratchet' },
          ],
        },
        {
          title: 'Project',
          items: [
            { label: 'SimpleGo on GitHub', href: 'https://github.com/cannatoshi/SimpleGo' },
            { label: 'SimpleX Chat', href: 'https://simplex.chat' },
            { label: 'S.D - IT and More Systems', href: 'https://it-and-more-systems.de' },
          ],
        },
        {
          title: 'Legal',
          items: [
            { label: 'Legal Notice (Impressum)', to: '/legal/impressum' },
            { label: 'Privacy Policy', to: '/legal/privacy' },
            { label: 'Spec License: CC BY-SA 4.0', href: 'https://creativecommons.org/licenses/by-sa/4.0/' },
            { label: 'Software License: AGPL-3.0', href: 'https://github.com/cannatoshi/SimpleGo/blob/main/LICENSE' },
          ],
        },
      ],
      copyright: `© ${new Date().getFullYear()} S.D - IT and More Systems. All rights reserved.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.dracula,
      additionalLanguages: ['c', 'haskell', 'bash', 'json'],
    },
    tableOfContents: {
      minHeadingLevel: 2,
      maxHeadingLevel: 4,
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
'@ | Set-Content -Path (Join-Path $root "docs-site/docusaurus.config.ts") -Encoding UTF8
Write-Host "  + docusaurus.config.ts" -ForegroundColor Green

# --- sidebars.ts ---
@'
import type {SidebarsConfig} from '@docusaurus/plugin-content-docs';

const sidebars: SidebarsConfig = {

  // =============================================
  // SPECIFICATION - SimpleX Protocol for ESP32
  // =============================================
  specSidebar: [
    'spec/index',
    {
      type: 'category',
      label: 'Fundamentals',
      collapsed: false,
      items: [
        'spec/terminology',
        'spec/architecture',
      ],
    },
    {
      type: 'category',
      label: 'Transport & Commands',
      collapsed: false,
      items: [
        'spec/transport',
        'spec/smp-commands',
      ],
    },
    {
      type: 'category',
      label: 'Encryption Layers',
      collapsed: false,
      items: [
        'spec/encryption/server-transport',
        'spec/encryption/e2e-encryption',
        'spec/encryption/double-ratchet',
      ],
    },
    {
      type: 'category',
      label: 'Protocol Details',
      collapsed: false,
      items: [
        'spec/agent-protocol',
        'spec/wire-format',
        'spec/message-types',
        'spec/versioning',
      ],
    },
    'spec/security',
    {
      type: 'category',
      label: 'Appendices',
      collapsed: true,
      items: [
        'spec/appendices/test-vectors',
        'spec/appendices/implementation-notes',
        'spec/appendices/haskell-reference',
        'spec/appendices/changelog',
      ],
    },
  ],

  // =============================================
  // PROTOCOL ANALYSIS - Session-by-session journey
  // =============================================
  analysisSidebar: [
    'protocol-analysis/index',
    'protocol-analysis/quick-reference',
    'protocol-analysis/bug-tracker',
    {
      type: 'category',
      label: 'Sessions',
      collapsed: false,
      items: [
        'protocol-analysis/sessions/sessions-1-2',
        'protocol-analysis/sessions/sessions-3-4',
        'protocol-analysis/sessions/sessions-5-6',
        'protocol-analysis/sessions/session-7',
        'protocol-analysis/sessions/session-8',
        'protocol-analysis/sessions/session-9',
        'protocol-analysis/sessions/session-10',
        'protocol-analysis/sessions/session-11',
        'protocol-analysis/sessions/session-12',
        'protocol-analysis/sessions/session-13',
        'protocol-analysis/sessions/session-14',
        'protocol-analysis/sessions/session-15',
        'protocol-analysis/sessions/session-16',
        'protocol-analysis/sessions/session-17',
        'protocol-analysis/sessions/session-18',
        'protocol-analysis/sessions/session-19',
        'protocol-analysis/sessions/session-20',
        'protocol-analysis/sessions/session-21',
        'protocol-analysis/sessions/session-22',
      ],
    },
  ],

  // =============================================
  // PROJECT DOCS - Architecture, Hardware, Dev
  // =============================================
  projectSidebar: [
    'project/index',
    {
      type: 'category',
      label: 'Software',
      collapsed: false,
      items: [
        'project/architecture',
        'project/build-system',
        'project/development',
        'project/technical',
        'project/crypto',
        'project/protocol',
        'project/wire-format',
        'project/security-model',
        'project/bugs',
        'project/devnotes',
      ],
    },
    {
      type: 'category',
      label: 'Hardware',
      collapsed: false,
      items: [
        'project/hardware/overview',
        'project/hardware/hal-architecture',
        'project/hardware/hardware-tiers',
        'project/hardware/component-selection',
        'project/hardware/pcb-design',
        'project/hardware/enclosure-design',
        'project/hardware/security-architecture',
      ],
    },
    {
      type: 'category',
      label: 'Other',
      collapsed: true,
      items: [
        'project/adding-new-device',
        'project/simplex-vs-matrix',
        'project/sponsors',
        'project/disclaimer',
        'project/trademark',
      ],
    },
  ],

  // =============================================
  // RELEASES
  // =============================================
  releasesSidebar: [
    'releases/index',
    'releases/v0-1-16-alpha',
    'releases/v0-1-15-alpha',
    'releases/v0-1-14-alpha',
  ],
};

export default sidebars;
'@ | Set-Content -Path (Join-Path $root "docs-site/sidebars.ts") -Encoding UTF8
Write-Host "  + sidebars.ts" -ForegroundColor Green

# --- custom.css ---
@'
:root {
  --ifm-color-primary: #6366f1;
  --ifm-color-primary-dark: #4f46e5;
  --ifm-color-primary-darker: #4338ca;
  --ifm-color-primary-darkest: #3730a3;
  --ifm-color-primary-light: #818cf8;
  --ifm-color-primary-lighter: #a5b4fc;
  --ifm-color-primary-lightest: #c7d2fe;
  --ifm-font-family-base: 'Inter', system-ui, -apple-system, sans-serif;
  --ifm-font-family-monospace: 'JetBrains Mono', 'Fira Code', 'Cascadia Code', monospace;
  --ifm-code-font-size: 87.5%;
  --ifm-heading-font-weight: 600;
}

[data-theme='dark'] {
  --ifm-color-primary: #818cf8;
  --ifm-color-primary-dark: #6366f1;
  --ifm-color-primary-darker: #4f46e5;
  --ifm-color-primary-darkest: #4338ca;
  --ifm-color-primary-light: #a5b4fc;
  --ifm-color-primary-lighter: #c7d2fe;
  --ifm-color-primary-lightest: #e0e7ff;
  --ifm-background-color: #0f172a;
  --ifm-background-surface-color: #1e293b;
}

table { font-size: 0.95rem; }
table code { background: transparent; padding: 0; }
pre { border-radius: 8px; }
.alert { border-radius: 8px; }
.anchor { scroll-margin-top: 80px; }
pre code { line-height: 1.6; }
.badge--warning { font-size: 0.75rem; }

.footer { border-top: 1px solid var(--ifm-color-emphasis-300); }
[data-theme='dark'] .footer { border-top-color: var(--ifm-color-emphasis-200); }
.footer__title { font-size: 0.85rem; text-transform: uppercase; letter-spacing: 0.05em; opacity: 0.8; }
.footer__copyright { margin-top: 1.5rem; padding-top: 1rem; border-top: 1px solid var(--ifm-color-emphasis-200); font-size: 0.85rem; opacity: 0.7; }
'@ | Set-Content -Path (Join-Path $root "docs-site/src/css/custom.css") -Encoding UTF8
Write-Host "  + src/css/custom.css" -ForegroundColor Green

# --- static/CNAME ---
"docs.simplego.dev" | Set-Content -Path (Join-Path $root "docs-site/static/CNAME") -Encoding UTF8 -NoNewline
Write-Host "  + static/CNAME" -ForegroundColor Green

# --- .gitignore for docs-site ---
@'
node_modules/
build/
.docusaurus/
.cache-loader/
'@ | Set-Content -Path (Join-Path $root "docs-site/.gitignore") -Encoding UTF8
Write-Host "  + .gitignore" -ForegroundColor Green

# =============================================================================
# 3. GITHUB ACTIONS WORKFLOW
# =============================================================================
Write-Host "`n[3/6] Creating GitHub Actions workflow..." -ForegroundColor Yellow

@'
name: Deploy Docs to GitHub Pages

on:
  push:
    branches: [main]
    paths:
      - 'docs-site/**'
      - 'docs/**'
      - '.github/workflows/deploy-docs.yml'
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: "pages"
  cancel-in-progress: false

jobs:
  build:
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: docs-site
    steps:
      - uses: actions/checkout@v4
        with:
          fetch-depth: 0

      - uses: actions/setup-node@v4
        with:
          node-version: 22
          cache: npm
          cache-dependency-path: docs-site/package-lock.json

      - name: Install dependencies
        run: npm ci

      - name: Build website
        run: npm run build

      - name: Upload artifact
        uses: actions/upload-pages-artifact@v3
        with:
          path: docs-site/build

  deploy:
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    runs-on: ubuntu-latest
    needs: build
    steps:
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
'@ | Set-Content -Path (Join-Path $root ".github/workflows/deploy-docs.yml") -Encoding UTF8
Write-Host "  + .github/workflows/deploy-docs.yml" -ForegroundColor Green

# =============================================================================
# 4. CONTENT - Homepage
# =============================================================================
Write-Host "`n[4/6] Creating content pages..." -ForegroundColor Yellow

@'
---
title: SimpleGo Documentation
sidebar_position: 1
slug: /
---

# SimpleGo Documentation

**SimpleGo** is a native ESP32-S3 implementation of the SimpleX Messaging Protocol — the first known third-party implementation outside the official Haskell codebase.

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
'@ | Set-Content -Path (Join-Path $root "docs-site/content/index.md") -Encoding UTF8
Write-Host "  + content/index.md" -ForegroundColor Green

# =============================================================================
# SPECIFICATION FILES
# =============================================================================

# Helper function
function Write-Placeholder($filePath, $title, $position, $note) {
    $fullPath = Join-Path $root "docs-site/content/$filePath"
    @"
---
title: $title
sidebar_position: $position
---

# $title

:::note Placeholder
$note
:::
"@ | Set-Content -Path $fullPath -Encoding UTF8
    Write-Host "  + content/$filePath" -ForegroundColor Green
}

# Spec index
@'
---
title: SimpleX Protocol Specification
sidebar_position: 1
---

# SimpleX Protocol Specification for ESP32

A complete protocol specification derived from reverse-engineering the SimpleX Messaging Protocol (simplexmq Haskell codebase) for native implementation on ESP32 microcontrollers.

:::caution Work in Progress
This specification is actively being developed alongside the SimpleGo implementation. Content will be expanded as more protocol details are verified through implementation.
:::

## Overview

The SimpleX protocol uses a layered architecture with three encryption layers, a custom wire format, and an agent protocol for managing connections. This specification documents each layer as understood through analysis of the Haskell source code and verified through ESP32 implementation.
'@ | Set-Content -Path (Join-Path $root "docs-site/content/spec/index.md") -Encoding UTF8
Write-Host "  + content/spec/index.md" -ForegroundColor Green

Write-Placeholder "spec/terminology.md" "Terminology" 2 "Glossary of SimpleX protocol terms. Will be expanded."
Write-Placeholder "spec/architecture.md" "Architecture" 3 "Protocol architecture overview. Will be expanded."
Write-Placeholder "spec/transport.md" "Transport Layer" 4 "TLS 1.3 transport layer details. Will be expanded."
Write-Placeholder "spec/smp-commands.md" "SMP Commands" 5 "SMP command reference. Will be expanded."
Write-Placeholder "spec/encryption/server-transport.md" "Layer 3: Server Transport Encryption" 1 "Server transport encryption (NaCl crypto_box). Will be expanded."
Write-Placeholder "spec/encryption/e2e-encryption.md" "Layer 2: E2E Encryption" 2 "End-to-end encryption layer. Will be expanded."
Write-Placeholder "spec/encryption/double-ratchet.md" "Layer 1: Double Ratchet" 3 "Double Ratchet algorithm implementation. Will be expanded."
Write-Placeholder "spec/agent-protocol.md" "Agent Protocol" 6 "Agent protocol and connection management. Will be expanded."
Write-Placeholder "spec/wire-format.md" "Wire Format" 7 "Binary wire format specification. Will be expanded."
Write-Placeholder "spec/message-types.md" "Message Types" 8 "Message type definitions. Will be expanded."
Write-Placeholder "spec/versioning.md" "Versioning" 9 "Version negotiation and compatibility. Will be expanded."
Write-Placeholder "spec/security.md" "Security Model" 10 "Security model and threat analysis. Will be expanded."
Write-Placeholder "spec/appendices/test-vectors.md" "Test Vectors" 1 "Cryptographic test vectors for verification."
Write-Placeholder "spec/appendices/implementation-notes.md" "Implementation Notes" 2 "ESP32-specific implementation notes."
Write-Placeholder "spec/appendices/haskell-reference.md" "Haskell Reference" 3 "Key Haskell source code references."
Write-Placeholder "spec/appendices/changelog.md" "Changelog" 4 "Specification changelog."

# =============================================================================
# PROTOCOL ANALYSIS FILES
# =============================================================================

@'
---
title: Protocol Analysis
sidebar_position: 1
---

# Protocol Analysis

Session-by-session documentation of reverse-engineering the SimpleX Messaging Protocol for native ESP32 implementation.

> **22 sessions** · **31+ bugs found and fixed** · **83 key learnings**

## Sessions Overview

| Sessions | Topic | Key Achievement |
|----------|-------|-----------------|
| [1-2](/protocol-analysis/sessions/sessions-1-2) | Foundation & TLS | First SMP connection established |
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

- [Quick Reference](/protocol-analysis/quick-reference) — Constants, versions, key learnings
- [Bug Tracker](/protocol-analysis/bug-tracker) — All 31+ bugs documented
'@ | Set-Content -Path (Join-Path $root "docs-site/content/protocol-analysis/index.md") -Encoding UTF8
Write-Host "  + content/protocol-analysis/index.md" -ForegroundColor Green

Write-Placeholder "protocol-analysis/quick-reference.md" "Quick Reference" 2 "Content will be migrated from docs/protocol-analysis/QUICK_REFERENCE.md"
Write-Placeholder "protocol-analysis/bug-tracker.md" "Bug Tracker" 3 "Content will be migrated from docs/protocol-analysis/BUG_TRACKER.md"

# Session files
$sessions = @(
    @("sessions-1-2", "Sessions 1-2: Foundation and TLS", 1)
    @("sessions-3-4", "Sessions 3-4: Wire Format Deep Dive", 2)
    @("sessions-5-6", "Sessions 5-6: Encryption Layers", 3)
    @("session-7",    "Session 7: Agent Protocol", 4)
    @("session-8",    "Session 8: The Breakthrough", 5)
    @("session-9",    "Session 9: X3DH Key Agreement", 6)
    @("session-10",   "Session 10: Double Ratchet", 7)
    @("session-11",   "Session 11: Message Decryption", 8)
    @("session-12",   "Session 12: Full Chain", 9)
    @("session-13",   "Session 13: Stabilization", 10)
    @("session-14",   "Session 14: Edge Cases", 11)
    @("session-15",   "Session 15: Nonce Handling", 12)
    @("session-16",   "Session 16: Key Management", 13)
    @("session-17",   "Session 17: Refinement", 14)
    @("session-18",   "Session 18: Robustness", 15)
    @("session-19",   "Session 19: Post-Quantum Crypto", 16)
    @("session-20",   "Session 20: Reply Queue", 17)
    @("session-21",   "Session 21: Major Bug Fixes", 18)
    @("session-22",   "Session 22: Modern Protocol Discovery", 19)
)

foreach ($s in $sessions) {
    Write-Placeholder "protocol-analysis/sessions/$($s[0]).md" $s[1] $s[2] "Content will be migrated from the corresponding analysis document."
}

# =============================================================================
# PROJECT DOCS FILES
# =============================================================================
Write-Host "`n[5/6] Creating Project Docs & Releases placeholders..." -ForegroundColor Yellow

@'
---
title: Project Documentation
sidebar_position: 1
---

# SimpleGo Project Documentation

Technical documentation for the SimpleGo project — a native ESP32 implementation of the SimpleX messaging protocol.

## Software

Architecture, build system, cryptography implementation, and development guides.

## Hardware

Hardware abstraction layer, device tiers, PCB designs, and enclosure specifications for dedicated SimpleX messaging devices.
'@ | Set-Content -Path (Join-Path $root "docs-site/content/project/index.md") -Encoding UTF8
Write-Host "  + content/project/index.md" -ForegroundColor Green

# Software docs (map to existing docs/ files)
$softwareDocs = @(
    @("project/architecture.md", "Software Architecture", 2, "Source: docs/ARCHITECTURE.md")
    @("project/build-system.md", "Build System", 3, "Source: docs/BUILD_SYSTEM.md")
    @("project/development.md", "Development Guide", 4, "Source: docs/DEVELOPMENT.md")
    @("project/technical.md", "Technical Details", 5, "Source: docs/TECHNICAL.md")
    @("project/crypto.md", "Cryptography", 6, "Source: docs/CRYPTO.md")
    @("project/protocol.md", "Protocol Implementation", 7, "Source: docs/PROTOCOL.md")
    @("project/wire-format.md", "Wire Format Implementation", 8, "Source: docs/WIRE_FORMAT.md")
    @("project/security-model.md", "Security Model", 9, "Source: docs/SECURITY_MODEL.md")
    @("project/bugs.md", "Known Bugs", 10, "Source: docs/BUGS.md")
    @("project/devnotes.md", "Developer Notes", 11, "Source: docs/DEVNOTES.md")
    @("project/adding-new-device.md", "Adding New Devices", 20, "Source: docs/ADDING_NEW_DEVICE.md")
    @("project/simplex-vs-matrix.md", "SimpleX vs Matrix", 21, "Source: docs/SIMPLEX_VS_MATRIX.md")
    @("project/sponsors.md", "Sponsors", 22, "Source: docs/SPONSORS.md")
    @("project/disclaimer.md", "Disclaimer", 23, "Source: docs/DISCLAIMER.md")
    @("project/trademark.md", "Trademark", 24, "Source: docs/TRADEMARK.md")
)

foreach ($doc in $softwareDocs) {
    Write-Placeholder $doc[0] $doc[1] $doc[2] $doc[3]
}

# Hardware docs
$hardwareDocs = @(
    @("project/hardware/overview.md", "Hardware Overview", 1, "Source: docs/hardware/HARDWARE_OVERVIEW.md")
    @("project/hardware/hal-architecture.md", "HAL Architecture", 2, "Source: docs/hardware/HAL_ARCHITECTURE.md")
    @("project/hardware/hardware-tiers.md", "Hardware Tiers", 3, "Source: docs/hardware/HARDWARE_TIERS.md")
    @("project/hardware/component-selection.md", "Component Selection", 4, "Source: docs/hardware/COMPONENT_SELECTION.md")
    @("project/hardware/pcb-design.md", "PCB Design", 5, "Source: docs/hardware/PCB_DESIGN.md")
    @("project/hardware/enclosure-design.md", "Enclosure Design", 6, "Source: docs/hardware/ENCLOSURE_DESIGN.md")
    @("project/hardware/security-architecture.md", "Security Architecture", 7, "Source: docs/hardware/SECURITY_ARCHITECTURE.md")
)

foreach ($doc in $hardwareDocs) {
    Write-Placeholder $doc[0] $doc[1] $doc[2] $doc[3]
}

# =============================================================================
# RELEASES
# =============================================================================

@'
---
title: Releases
sidebar_position: 1
slug: /releases
---

# SimpleGo Releases

| Version | Highlights |
|---------|------------|
| [v0.1.16-alpha](/releases/v0-1-16-alpha) | HAL Architecture, Multi-Device |
| [v0.1.15-alpha](/releases/v0-1-15-alpha) | Double Ratchet, X3DH |
| [v0.1.14-alpha](/releases/v0-1-14-alpha) | Modular Architecture |
'@ | Set-Content -Path (Join-Path $root "docs-site/content/releases/index.md") -Encoding UTF8
Write-Host "  + content/releases/index.md" -ForegroundColor Green

Write-Placeholder "releases/v0-1-16-alpha.md" "v0.1.16-alpha" 2 "Source: docs/release-info/v0.1.16-alpha.md"
Write-Placeholder "releases/v0-1-15-alpha.md" "v0.1.15-alpha" 3 "Source: docs/release-info/v0.1.15-alpha.md"
Write-Placeholder "releases/v0-1-14-alpha.md" "v0.1.14-alpha" 4 "Source: docs/release-info/v0.1.14-alpha.md"

# =============================================================================
# LEGAL PAGES
# =============================================================================

@"
---
title: Legal Notice (Impressum)
sidebar_position: 99
unlisted: true
---

# Legal Notice (Impressum)

Required under German law (TMG `$5, MStV `$18 Abs. 2).

**S.D - IT and More Systems**

*Full legal details will be added here.*
"@ | Set-Content -Path (Join-Path $root "docs-site/content/legal/impressum.md") -Encoding UTF8
Write-Host "  + content/legal/impressum.md" -ForegroundColor Green

@"
---
title: Privacy Policy
sidebar_position: 100
unlisted: true
---

# Privacy Policy (Datenschutzerklaerung)

*Full privacy policy will be added here.*
"@ | Set-Content -Path (Join-Path $root "docs-site/content/legal/privacy.md") -Encoding UTF8
Write-Host "  + content/legal/privacy.md" -ForegroundColor Green

# =============================================================================
# 6. SUMMARY
# =============================================================================
Write-Host "`n[6/6] Done!" -ForegroundColor Yellow

$totalFiles = (Get-ChildItem -Path (Join-Path $root "docs-site") -Recurse -File).Count
$contentFiles = (Get-ChildItem -Path (Join-Path $root "docs-site/content") -Recurse -File -Filter "*.md").Count

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "  Setup Complete!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Total files:      $totalFiles" -ForegroundColor White
Write-Host "  Content pages:    $contentFiles" -ForegroundColor White
Write-Host "  Existing docs/:   UNTOUCHED" -ForegroundColor Green
Write-Host ""
Write-Host "  Structure:" -ForegroundColor White
Write-Host "    simplex_client/" -ForegroundColor DarkGray
Write-Host "    +-- docs/                    (your existing docs - UNTOUCHED)" -ForegroundColor DarkGray
Write-Host "    +-- docs-site/               (Docusaurus website)" -ForegroundColor White
Write-Host "    |   +-- content/spec/        (protocol specification)" -ForegroundColor White
Write-Host "    |   +-- content/protocol-analysis/  (session analysis)" -ForegroundColor White
Write-Host "    |   +-- content/project/     (project docs for website)" -ForegroundColor White
Write-Host "    |   +-- content/releases/    (release notes)" -ForegroundColor White
Write-Host "    |   +-- content/legal/       (impressum, privacy)" -ForegroundColor White
Write-Host "    +-- .github/workflows/       (auto-deploy on push)" -ForegroundColor White
Write-Host ""
Write-Host "  Next steps:" -ForegroundColor Yellow
Write-Host "    1.  cd docs-site" -ForegroundColor White
Write-Host "    2.  npm install" -ForegroundColor White
Write-Host "    3.  npm run start             (local preview at localhost:3000)" -ForegroundColor White
Write-Host "    4.  cd .." -ForegroundColor White
Write-Host "    5.  git add -A" -ForegroundColor White
Write-Host '    6.  git commit -m "ci(docs): add Docusaurus site with GitHub Pages auto-deploy"' -ForegroundColor White
Write-Host "    7.  git push" -ForegroundColor White
Write-Host ""
Write-Host "  GitHub Settings (once):" -ForegroundColor Yellow
Write-Host "    -> Settings > Pages > Source: GitHub Actions" -ForegroundColor White
Write-Host "    -> Custom domain: docs.simplego.dev" -ForegroundColor White
Write-Host ""
Write-Host "  After that: edit .md, push, site updates in ~2min!" -ForegroundColor Green
Write-Host ""
