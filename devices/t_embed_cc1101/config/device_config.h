/**
 * @file device_config.h
 * @brief LILYGO T-Embed CC1101 Plus Device Configuration
 * 
 * Hardware Specifications:
 * - MCU: ESP32-S3 (N16R8 - 16MB Flash, 8MB PSRAM)
 * - Display: 1.9" IPS 170x320 ST7789V (Portrait)
 * - Input: Rotary Encoder + 2 Side Buttons
 * - Radio: CC1101 Sub-GHz Transceiver
 * - Battery: 3.7V 1100mAh LiPo with Charging IC
 * - Speaker: Built-in Buzzer
 * - NO GPS, NO LoRa (different from T-Embed Plus)
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

/*============================================================================
 * DEVICE IDENTIFICATION
 *==========================================================================*/

#define DEVICE_NAME             "T-Embed CC1101"
#define DEVICE_ID               "t_embed_cc1101"
#define DEVICE_MANUFACTURER     "LILYGO"
#define DEVICE_MCU              "ESP32-S3"
#define DEVICE_VERSION          "1.0"

/*============================================================================
 * DISPLAY CONFIGURATION
 *==========================================================================*/

#define DISPLAY_TYPE            "ST7789V"
#define DISPLAY_WIDTH           170
#define DISPLAY_HEIGHT          320
#define DISPLAY_COLOR_DEPTH     16          // RGB565
#define DISPLAY_ROTATION        0           // Portrait (natural orientation)
#define DISPLAY_INVERT          true        // This display needs inversion

// SPI Pins (T-Embed specific)
#define DISPLAY_SPI_HOST        SPI2_HOST
#define DISPLAY_PIN_MOSI        11
#define DISPLAY_PIN_SCLK        12
#define DISPLAY_PIN_CS          10
#define DISPLAY_PIN_DC          13
#define DISPLAY_PIN_RST         9
#define DISPLAY_PIN_BL          15

// SPI Configuration
#define DISPLAY_SPI_FREQ        40000000    // 40 MHz
#define DISPLAY_BL_PWM_CHANNEL  0
#define DISPLAY_BL_PWM_FREQ     5000

// DMA Buffer (smaller because narrower screen)
#define DISPLAY_BUFFER_SIZE     (DISPLAY_WIDTH * 20)  // 20 lines
#define DISPLAY_USE_DMA         true
#define DISPLAY_DOUBLE_BUFFER   true

/*============================================================================
 * TOUCH CONFIGURATION
 *==========================================================================*/

#define TOUCH_ENABLED           false       // No touch on T-Embed

/*============================================================================
 * KEYBOARD CONFIGURATION
 *==========================================================================*/

#define KEYBOARD_ENABLED        false       // No keyboard - uses encoder

/*============================================================================
 * ROTARY ENCODER CONFIGURATION
 *==========================================================================*/

#define ENCODER_ENABLED         true
#define ENCODER_PIN_A           4           // CLK
#define ENCODER_PIN_B           5           // DT
#define ENCODER_PIN_BTN         0           // SW (shared with BOOT)
#define ENCODER_STEPS_PER_NOTCH 4
#define ENCODER_REVERSE         false       // Swap CW/CCW

/*============================================================================
 * BUTTON CONFIGURATION
 *==========================================================================*/

#define BUTTON_COUNT            2

// Side buttons
#define BUTTON_A_PIN            47          // Left side button
#define BUTTON_B_PIN            48          // Right side button (or second left)
#define BUTTON_ACTIVE_LOW       true        // Buttons pull to GND

// Button mapping
#define BUTTON_A_FUNCTION       "BACK"      // Default function
#define BUTTON_B_FUNCTION       "MENU"      // Default function

/*============================================================================
 * I2C CONFIGURATION
 *==========================================================================*/

#define I2C_PORT_0_ENABLED      true
#define I2C_PORT_0_PIN_SDA      8
#define I2C_PORT_0_PIN_SCL      18
#define I2C_PORT_0_FREQ         400000      // 400 kHz

#define I2C_PORT_1_ENABLED      false

/*============================================================================
 * AUDIO CONFIGURATION
 *==========================================================================*/

#define AUDIO_ENABLED           true
#define AUDIO_TYPE              "BUZZER"    // Simple buzzer, not I2S speaker
#define AUDIO_BUZZER_PIN        46
#define AUDIO_BUZZER_CHANNEL    1           // LEDC channel

// No I2S audio
#define AUDIO_I2S_ENABLED       false

/*============================================================================
 * CC1101 SUB-GHZ RADIO CONFIGURATION
 *==========================================================================*/

#define CC1101_ENABLED          true
#define CC1101_SPI_HOST         SPI3_HOST
#define CC1101_PIN_MOSI         35
#define CC1101_PIN_MISO         37
#define CC1101_PIN_SCK          36
#define CC1101_PIN_CS           38          // CSn
#define CC1101_PIN_GDO0         3           // Digital output 0
#define CC1101_PIN_GDO2         1           // Digital output 2

// CC1101 Default Settings
#define CC1101_FREQUENCY        433.92      // MHz (EU ISM band)
#define CC1101_DATARATE         4800        // bps
#define CC1101_MODULATION       "2-FSK"

/*============================================================================
 * POWER MANAGEMENT CONFIGURATION
 *==========================================================================*/

#define PMU_ENABLED             false       // No separate PMU chip
#define PMU_TYPE                "NONE"

// Battery monitoring via ADC
#define BATTERY_ENABLED         true
#define BATTERY_ADC_PIN         14          // Battery voltage divider
#define BATTERY_ADC_CHANNEL     ADC_CHANNEL_3
#define BATTERY_CAPACITY_MAH    1100
#define BATTERY_LOW_THRESHOLD   15
#define BATTERY_CRITICAL        5

// Voltage divider calibration (check schematic)
#define BATTERY_DIVIDER_R1      100         // kOhm
#define BATTERY_DIVIDER_R2      100         // kOhm
#define BATTERY_FULL_MV         4200
#define BATTERY_EMPTY_MV        3300

// Charging detection
#define CHARGING_DETECT_PIN     -1          // No dedicated pin
#define USB_DETECT_PIN          -1          // No dedicated pin

/*============================================================================
 * SD CARD CONFIGURATION
 *==========================================================================*/

#define SD_CARD_ENABLED         false       // No SD card slot on T-Embed

/*============================================================================
 * WIFI/BLUETOOTH
 *==========================================================================*/

#define WIFI_ENABLED            true
#define BT_ENABLED              true
#define BT_BLE_ENABLED          true
#define BT_CLASSIC_ENABLED      false

/*============================================================================
 * MISC GPIO
 *==========================================================================*/

#define GPIO_BOOT_BUTTON        0           // Shared with encoder button
#define GPIO_LED                -1          // No dedicated LED (backlight only)

// Power control
#define GPIO_PERIPH_POWER       -1          // Some boards have peripheral power control

/*============================================================================
 * FEATURE FLAGS
 *==========================================================================*/

#define FEATURE_KEYBOARD        0
#define FEATURE_TRACKBALL       0
#define FEATURE_TOUCH           0
#define FEATURE_ENCODER         1
#define FEATURE_BUTTONS         1
#define FEATURE_AUDIO           1           // Buzzer only
#define FEATURE_AUDIO_I2S       0
#define FEATURE_BATTERY         1
#define FEATURE_SD_CARD         0
#define FEATURE_LORA            0
#define FEATURE_CC1101          1
#define FEATURE_GPS             0
#define FEATURE_BLE             1

/*============================================================================
 * UI CONFIGURATION
 *==========================================================================*/

// Smaller screen needs optimized UI
#define UI_THEME                "dark"
#define UI_FONT_DEFAULT         "lv_font_montserrat_12"  // Smaller default
#define UI_FONT_LARGE           "lv_font_montserrat_16"
#define UI_FONT_SMALL           "lv_font_montserrat_10"
#define UI_ANIMATION_SPEED      150         // Faster animations
#define UI_SCROLL_SPEED         8

// Portrait layout adjustments
#define UI_HEADER_HEIGHT        32
#define UI_FOOTER_HEIGHT        28
#define UI_LIST_ITEM_HEIGHT     40
#define UI_PADDING              4

/*============================================================================
 * LVGL BUFFER CONFIGURATION
 *==========================================================================*/

#define LVGL_TICK_PERIOD_MS     2
#define LVGL_BUFFER_LINES       20          // Narrower screen = smaller buffer
#define LVGL_USE_PSRAM          true
#define LVGL_DOUBLE_BUFFER      true

/*============================================================================
 * INPUT PRIORITIES
 *==========================================================================*/

// Encoder is primary input for T-Embed
#define INPUT_PRIMARY           "ENCODER"
#define INPUT_SECONDARY         "BUTTONS"

/*============================================================================
 * ON-SCREEN KEYBOARD CONFIG
 *==========================================================================*/

// Since no physical keyboard, need OSK for text entry
#define OSK_ENABLED             true
#define OSK_LAYOUT              "QWERTY"
#define OSK_HEIGHT_PERCENT      50          // % of screen when visible
#define OSK_KEY_WIDTH           16          // Minimum key width
#define OSK_KEY_HEIGHT          28          // Key height

#endif /* DEVICE_CONFIG_H */
