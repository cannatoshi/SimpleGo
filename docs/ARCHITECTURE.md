# SimpleGo Architecture

## Overview

SimpleGo is designed as a multi-device native SimpleX client using a Hardware Abstraction Layer (HAL) architecture. This enables support for different hardware platforms while maintaining a single, unified codebase for the protocol implementation and UI.

```
┌─────────────────────────────────────────────────────────────────┐
│                        APPLICATION                               │
│                  (SimpleX Protocol Logic)                        │
│                    Device-Independent                            │
├─────────────────────────────────────────────────────────────────┤
│                         UI LAYER                                 │
│              (Screens, Widgets, Themes)                          │
│                    Device-Independent                            │
├─────────────────────────────────────────────────────────────────┤
│                   HAL (Interfaces)                               │
│     hal_display | hal_input | hal_storage | hal_audio | ...     │
├──────────┬──────────┬──────────┬──────────┬────────────────────┤
│ T-Deck   │ T-Embed  │ RPi      │ Custom   │  Device-Specific   │
│ Plus     │ CC1101   │          │          │  Implementations   │
└──────────┴──────────┴──────────┴──────────┴────────────────────┘
```

## Directory Structure

```
simplex_client/
├── main/                           # Main application code
│   ├── main.c                      # Entry point
│   ├── core/                       # Protocol layer (device-independent)
│   │   ├── smp_network.c           # TLS/TCP connections
│   │   ├── smp_handshake.c         # SMP handshake
│   │   ├── smp_transport.c         # Transport encoding
│   │   ├── smp_commands.c          # SMP commands
│   │   ├── smp_ratchet.c           # Double Ratchet
│   │   ├── smp_x448.c              # X448 curve operations
│   │   ├── smp_x3dh.c              # X3DH key agreement
│   │   ├── smp_crypto.c            # AES-GCM, HKDF
│   │   ├── smp_agent.c             # Agent protocol
│   │   ├── smp_queue.c             # Queue management
│   │   └── smp_message.c           # Message parsing
│   │
│   ├── hal/                        # HAL interfaces (headers only)
│   │   ├── hal_common.h            # Common types & errors
│   │   ├── hal_display.h           # Display interface
│   │   ├── hal_input.h             # Input interface
│   │   ├── hal_storage.h           # Storage interface
│   │   ├── hal_network.h           # Network interface
│   │   ├── hal_audio.h             # Audio interface
│   │   └── hal_system.h            # System interface
│   │
│   └── ui/                         # UI layer (device-independent)
│       ├── ui_manager.c            # Screen management
│       ├── ui_events.c             # Event handling
│       ├── screens/                # Individual screens
│       ├── widgets/                # Reusable widgets
│       └── themes/                 # Visual themes
│
├── devices/                        # Device-specific implementations
│   ├── t_deck_plus/
│   │   ├── hal_impl/               # HAL implementations
│   │   └── config/                 # Device configuration
│   ├── t_embed_cc1101/
│   │   ├── hal_impl/
│   │   └── config/
│   └── template/                   # Template for new devices
│
├── components/                     # External libraries
├── tools/                          # Build scripts
├── docs/                           # Documentation
├── CMakeLists.txt                  # Build system
└── Kconfig                         # Configuration menu
```

## Layer Descriptions

### Core Layer (`main/core/`)

The protocol layer implements the complete SimpleX Messaging Protocol (SMP):

- **Network**: TLS connections to SMP servers
- **Handshake**: Server authentication and session setup
- **Commands**: SMP command encoding/decoding (NEW, KEY, SUB, SEND, etc.)
- **Crypto**: All cryptographic operations (X448, AES-GCM, HKDF)
- **Ratchet**: Double Ratchet protocol for forward secrecy
- **X3DH**: Extended Triple Diffie-Hellman key agreement
- **Agent**: SimpleX Agent protocol layer

This layer is **100% device-independent**.

### HAL Layer (`main/hal/`)

Hardware Abstraction Layer defines interfaces for:

| Interface | Purpose |
|-----------|---------|
| `hal_display` | Display initialization, drawing, backlight |
| `hal_input` | Keyboard, touch, encoder, buttons |
| `hal_storage` | NVS key-value store, file system |
| `hal_network` | WiFi, Ethernet configuration |
| `hal_audio` | Speaker, buzzer, microphone |
| `hal_system` | Power management, sleep, battery |

### UI Layer (`main/ui/`)

The UI layer uses LVGL and the HAL interfaces:

- **Screens**: Individual UI screens (chat, contacts, settings)
- **Widgets**: Reusable components (message bubbles, status bar)
- **Themes**: Visual styles (dark, light)

The UI adapts automatically to device capabilities via HAL queries.

### Device Layer (`devices/*/`)

Each supported device has:

- `hal_impl/`: HAL interface implementations
- `config/device_config.h`: Hardware-specific constants

## Adding a New Device

1. Copy `devices/template/` to `devices/your_device/`
2. Fill in `config/device_config.h` with your hardware specs
3. Implement required HAL functions in `hal_impl/`
4. Add device to `Kconfig` and `CMakeLists.txt`
5. Build with `SIMPLEGO_DEVICE=your_device`

See `docs/ADDING_NEW_DEVICE.md` for detailed instructions.

## Build System

SimpleGo uses the standard ESP-IDF build system with Kconfig for device selection.

**IMPORTANT:** ESP-IDF has strict rules about CMakeLists.txt files:
- ROOT `CMakeLists.txt`: Only project setup (no `idf_component_register`!)
- Component `CMakeLists.txt` (in `main/`): Source registration

See `docs/BUILD_SYSTEM.md` for detailed explanation of why this matters.

```bash
# Configure device via menuconfig
idf.py menuconfig
# → SimpleGo Configuration → Target Device

# Build
idf.py build

# Flash
idf.py flash monitor -p COM5
```

## Device Comparison

| Feature | T-Deck Plus | T-Embed CC1101 |
|---------|-------------|----------------|
| Display | 320×240 | 170×320 |
| Keyboard | QWERTY | On-screen |
| Touch | Yes | No |
| Trackball | Yes | No |
| Encoder | No | Yes |
| Audio | I2S Speaker | Buzzer |
| Battery | AXP2101 PMU | ADC only |
| Radio | Optional LoRa | CC1101 Sub-GHz |

## Design Principles

1. **Single Codebase**: Core protocol and UI code shared across devices
2. **HAL Abstraction**: All hardware access through HAL interfaces
3. **Capability Queries**: UI adapts based on available features
4. **Minimal Dependencies**: Only essential libraries per device
5. **Memory Efficiency**: Careful buffer management for ESP32

## Memory Layout (ESP32-S3)

```
┌─────────────────────────────────────┐
│ Flash (16MB)                        │
│ ├── Bootloader (64KB)               │
│ ├── Partition Table                 │
│ ├── NVS (16KB) - Keys & Settings    │
│ ├── Application (~4MB)              │
│ └── SPIFFS/LittleFS - Contacts/Msgs │
├─────────────────────────────────────┤
│ Internal SRAM (512KB)               │
│ ├── Heap (~280KB)                   │
│ └── Stack (16KB per task)           │
├─────────────────────────────────────┤
│ PSRAM (8MB)                         │
│ ├── LVGL Buffers                    │
│ ├── Message Cache                   │
│ └── Crypto Buffers                  │
└─────────────────────────────────────┘
```

## Security Considerations

- Private keys stored in NVS with encryption
- Ratchet keys never leave device
- Memory wiped after use (`explicit_bzero`)
- Optional PIN/password protection
- No telemetry or analytics

## License

AGPL-3.0 (to comply with SimpleX protocol requirements)
