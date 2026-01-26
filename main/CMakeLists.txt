# Use our own lv_conf.h
set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h)

# SimpleGo - Main Component
# Professional HAL Architecture
# SPDX-License-Identifier: AGPL-3.0

# Device selection (default: t_deck_plus)
set(SIMPLEGO_DEVICE "t_deck_plus" CACHE STRING "Target device")
set(DEVICE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../devices/${SIMPLEGO_DEVICE}")

message(STATUS "========================================")
message(STATUS "  SimpleGo - Building for: ${SIMPLEGO_DEVICE}")
message(STATUS "========================================")

# Core protocol sources (device-independent)
set(CORE_SOURCES
    "main.c"
    "smp_contacts.c"
    "smp_crypto.c"
    "smp_globals.c"
    "smp_network.c"
    "smp_parser.c"
    "smp_peer.c"
    "smp_queue.c"
    "smp_utils.c"
    "smp_x448.c"
    "smp_ratchet.c"
    "smp_handshake.c"
)

# UI sources (device-independent)
set(UI_SOURCES
    "ui/ui_manager.c"
    "ui/ui_theme.c"
    "ui/screens/ui_splash.c"
    "ui/screens/ui_main.c"
    "ui/screens/ui_chat.c"
    "ui/screens/ui_contacts.c"
    "ui/screens/ui_connect.c"
    "ui/screens/ui_settings.c"
    "ui/screens/ui_developer.c"
)

# HAL implementation sources (device-specific)
file(GLOB HAL_IMPL_SOURCES "${DEVICE_DIR}/hal_impl/*.c")

# Combine all sources
set(ALL_SOURCES
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${HAL_IMPL_SOURCES}
)

idf_component_register(
    SRCS
        ${ALL_SOURCES}
    INCLUDE_DIRS
        "include"
        "hal"
        "ui"
        "ui/screens"
        "ui/widgets"
        "ui/themes"
        "${DEVICE_DIR}/config"
        "${DEVICE_DIR}/hal_impl"
        "../components/kem"
        "../components/common"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        mbedtls
        libsodium
        wolfssl_config
        wolfssl__wolfssl
        driver
        esp_lcd
        esp_timer
        lvgl
)

# Log what we found
message(STATUS "Core sources: ${CORE_SOURCES}")
message(STATUS "UI sources: ${UI_SOURCES}")
message(STATUS "HAL sources: ${HAL_IMPL_SOURCES}")
