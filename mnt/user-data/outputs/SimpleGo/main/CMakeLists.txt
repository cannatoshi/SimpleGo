# SimpleGo - Main component
# SPDX-License-Identifier: AGPL-3.0-or-later

idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        nvs_flash      # Non-volatile storage (WiFi config)
        esp_wifi       # WiFi driver
        esp_event      # Event loop
        esp_netif      # Network interface
        mbedtls        # TLS and crypto
        libsodium      # Ed25519, X25519 (CRITICAL: not Monocypher!)
)
