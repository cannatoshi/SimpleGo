/**
 * @file hal_display_t_deck_plus.c
 * @brief T-Deck Plus Display HAL Implementation
 * 
 * Hardware: ST7789V 320x240 SPI Display
 * 
 * SimpleGo - Hardware Abstraction Layer
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "hal_display.h"
#include "device_config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"

#define TAG "HAL_DISP"

/*============================================================================
 * PRIVATE DATA
 *==========================================================================*/

static struct {
    bool initialized;
    spi_device_handle_t spi;
    hal_display_info_t info;
    SemaphoreHandle_t flush_sem;
    uint8_t *buffer1;
    uint8_t *buffer2;
    size_t buffer_size;
    hal_display_flush_cb_t flush_cb;
    void *flush_cb_user_data;
} s_disp = {0};

/*============================================================================
 * ST7789 COMMANDS
 *==========================================================================*/

#define ST7789_NOP          0x00
#define ST7789_SWRESET      0x01
#define ST7789_SLPIN        0x10
#define ST7789_SLPOUT       0x11
#define ST7789_PTLON        0x12
#define ST7789_NORON        0x13
#define ST7789_INVOFF       0x20
#define ST7789_INVON        0x21
#define ST7789_DISPOFF      0x28
#define ST7789_DISPON       0x29
#define ST7789_CASET        0x2A
#define ST7789_RASET        0x2B
#define ST7789_RAMWR        0x2C
#define ST7789_MADCTL       0x36
#define ST7789_COLMOD       0x3A

// MADCTL bits
#define MADCTL_MY           0x80
#define MADCTL_MX           0x40
#define MADCTL_MV           0x20
#define MADCTL_ML           0x10
#define MADCTL_RGB          0x00
#define MADCTL_BGR          0x08

/*============================================================================
 * PRIVATE FUNCTIONS
 *==========================================================================*/

/**
 * @brief Send command to display
 */
static void disp_cmd(uint8_t cmd) {
    gpio_set_level(DISPLAY_PIN_DC, 0);  // Command mode
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    spi_device_polling_transmit(s_disp.spi, &t);
}

/**
 * @brief Send data to display
 */
static void disp_data(const uint8_t *data, size_t len) {
    if (len == 0) return;
    
    gpio_set_level(DISPLAY_PIN_DC, 1);  // Data mode
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(s_disp.spi, &t);
}

/**
 * @brief Send single byte data
 */
static void disp_data8(uint8_t data) {
    disp_data(&data, 1);
}

/**
 * @brief Set address window
 */
static void disp_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t data[4];
    
    disp_cmd(ST7789_CASET);
    data[0] = x0 >> 8;
    data[1] = x0 & 0xFF;
    data[2] = x1 >> 8;
    data[3] = x1 & 0xFF;
    disp_data(data, 4);
    
    disp_cmd(ST7789_RASET);
    data[0] = y0 >> 8;
    data[1] = y0 & 0xFF;
    data[2] = y1 >> 8;
    data[3] = y1 & 0xFF;
    disp_data(data, 4);
    
    disp_cmd(ST7789_RAMWR);
}

/**
 * @brief Initialize display
 */
static void disp_init_seq(void) {
    // Hardware reset if available
    #if DISPLAY_PIN_RST >= 0
    gpio_set_level(DISPLAY_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(DISPLAY_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    #endif
    
    // Software reset
    disp_cmd(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    // Sleep out
    disp_cmd(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));
    
    // Pixel format: 16-bit RGB565
    disp_cmd(ST7789_COLMOD);
    disp_data8(0x55);
    
    // Memory access control (rotation)
    disp_cmd(ST7789_MADCTL);
    #if DISPLAY_ROTATION == 0
    disp_data8(MADCTL_RGB);
    #elif DISPLAY_ROTATION == 90
    disp_data8(MADCTL_MV | MADCTL_MY | MADCTL_RGB);
    #elif DISPLAY_ROTATION == 180
    disp_data8(MADCTL_MX | MADCTL_MY | MADCTL_RGB);
    #elif DISPLAY_ROTATION == 270
    disp_data8(MADCTL_MV | MADCTL_MX | MADCTL_RGB);
    #else
    disp_data8(MADCTL_MV | MADCTL_MY | MADCTL_RGB);  // Default landscape
    #endif
    
    // Inversion
    #if DISPLAY_INVERT
    disp_cmd(ST7789_INVON);
    #else
    disp_cmd(ST7789_INVOFF);
    #endif
    
    // Normal display mode
    disp_cmd(ST7789_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Display on
    disp_cmd(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));
}

/**
 * @brief Initialize backlight PWM
 */
static void backlight_init(void) {
    #if DISPLAY_PIN_BL >= 0
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = DISPLAY_BL_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);
    
    ledc_channel_config_t channel = {
        .gpio_num = DISPLAY_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = DISPLAY_BL_PWM_CHANNEL,
        .timer_sel = LEDC_TIMER_0,
        .duty = 255,  // Full brightness
        .hpoint = 0,
    };
    ledc_channel_config(&channel);
    #endif
}

/**
 * @brief DMA transfer complete callback
 */
static IRAM_ATTR void spi_pre_cb(spi_transaction_t *t) {
    gpio_set_level(DISPLAY_PIN_DC, 1);  // Data mode for pixel data
}

/*============================================================================
 * PUBLIC API
 *==========================================================================*/

hal_err_t hal_display_init(const hal_display_config_t *config) {
    if (s_disp.initialized) {
        return HAL_ERR_ALREADY;
    }
    
    HAL_LOGI(TAG, "Initializing display: %s", DEVICE_NAME);
    
    // Initialize GPIO
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << DISPLAY_PIN_DC),
    };
    #if DISPLAY_PIN_RST >= 0
    io_conf.pin_bit_mask |= (1ULL << DISPLAY_PIN_RST);
    #endif
    gpio_config(&io_conf);
    
    // Initialize SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = DISPLAY_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = DISPLAY_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_BUFFER_SIZE * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
    
    // Add display device
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = DISPLAY_SPI_FREQ,
        .mode = 0,
        .spics_io_num = DISPLAY_PIN_CS,
        .queue_size = 7,
        .pre_cb = spi_pre_cb,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(DISPLAY_SPI_HOST, &dev_cfg, &s_disp.spi));
    
    // Initialize display
    disp_init_seq();
    
    // Initialize backlight
    backlight_init();
    
    // Allocate buffers (prefer PSRAM)
    s_disp.buffer_size = DISPLAY_BUFFER_SIZE * sizeof(uint16_t);
    
    #if LVGL_USE_PSRAM
    s_disp.buffer1 = heap_caps_malloc(s_disp.buffer_size, MALLOC_CAP_SPIRAM);
    #if DISPLAY_DOUBLE_BUFFER
    s_disp.buffer2 = heap_caps_malloc(s_disp.buffer_size, MALLOC_CAP_SPIRAM);
    #endif
    #else
    s_disp.buffer1 = heap_caps_malloc(s_disp.buffer_size, MALLOC_CAP_DMA);
    #if DISPLAY_DOUBLE_BUFFER
    s_disp.buffer2 = heap_caps_malloc(s_disp.buffer_size, MALLOC_CAP_DMA);
    #endif
    #endif
    
    if (!s_disp.buffer1) {
        HAL_LOGE(TAG, "Failed to allocate display buffer");
        return HAL_ERR_NO_MEM;
    }
    
    // Create flush semaphore
    s_disp.flush_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(s_disp.flush_sem);
    
    // Fill info struct
    s_disp.info = (hal_display_info_t){
        .name = DEVICE_NAME,
        .native_size = {DISPLAY_WIDTH, DISPLAY_HEIGHT},
        .current_size = {DISPLAY_WIDTH, DISPLAY_HEIGHT},
        .orientation = DISPLAY_ROTATION,
        .color_depth = HAL_DISPLAY_COLOR_16BIT,
        .capabilities = HAL_DISPLAY_CAP_BACKLIGHT | HAL_DISPLAY_CAP_DMA,
        .backlight_level = 100,
    };
    
    #if TOUCH_ENABLED
    s_disp.info.capabilities |= HAL_DISPLAY_CAP_TOUCH;
    #endif
    
    s_disp.initialized = true;
    HAL_LOGI(TAG, "Display initialized: %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
    
    return HAL_OK;
}

hal_err_t hal_display_deinit(void) {
    if (!s_disp.initialized) {
        return HAL_ERR_NOT_INIT;
    }
    
    hal_display_power(false);
    
    spi_bus_remove_device(s_disp.spi);
    spi_bus_free(DISPLAY_SPI_HOST);
    
    free(s_disp.buffer1);
    free(s_disp.buffer2);
    vSemaphoreDelete(s_disp.flush_sem);
    
    s_disp.initialized = false;
    return HAL_OK;
}

const hal_display_info_t *hal_display_get_info(void) {
    return &s_disp.info;
}

hal_err_t hal_display_set_backlight(uint8_t level) {
    #if DISPLAY_PIN_BL >= 0
    uint32_t duty = (level * 255) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, DISPLAY_BL_PWM_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, DISPLAY_BL_PWM_CHANNEL);
    s_disp.info.backlight_level = level;
    return HAL_OK;
    #else
    return HAL_ERR_NOT_SUPPORTED;
    #endif
}

uint8_t hal_display_get_backlight(void) {
    return s_disp.info.backlight_level;
}

hal_err_t hal_display_flush(const hal_rect_t *area, const uint8_t *color_data) {
    if (!s_disp.initialized) {
        return HAL_ERR_NOT_INIT;
    }
    
    // Wait for previous flush
    xSemaphoreTake(s_disp.flush_sem, portMAX_DELAY);
    
    // Set window
    disp_set_window(area->x, area->y, 
                    area->x + area->width - 1, 
                    area->y + area->height - 1);
    
    // Send pixel data
    size_t size = area->width * area->height * 2;
    gpio_set_level(DISPLAY_PIN_DC, 1);
    
    spi_transaction_t t = {
        .length = size * 8,
        .tx_buffer = color_data,
    };
    spi_device_polling_transmit(s_disp.spi, &t);
    
    xSemaphoreGive(s_disp.flush_sem);
    
    // Notify callback
    if (s_disp.flush_cb) {
        s_disp.flush_cb(NULL, area, (uint8_t*)color_data);
    }
    
    return HAL_OK;
}

void hal_display_flush_ready(void) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_disp.flush_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

hal_err_t hal_display_power(bool on) {
    if (on) {
        disp_cmd(ST7789_SLPOUT);
        vTaskDelay(pdMS_TO_TICKS(120));
        disp_cmd(ST7789_DISPON);
    } else {
        disp_cmd(ST7789_DISPOFF);
        disp_cmd(ST7789_SLPIN);
    }
    return HAL_OK;
}

hal_err_t hal_display_invert(bool invert) {
    disp_cmd(invert ? ST7789_INVON : ST7789_INVOFF);
    return HAL_OK;
}

void hal_display_set_flush_cb(hal_display_flush_cb_t cb, void *user_data) {
    s_disp.flush_cb = cb;
    s_disp.flush_cb_user_data = user_data;
}

hal_err_t hal_display_get_buffers(void **buf1, void **buf2, size_t *size) {
    *buf1 = s_disp.buffer1;
    *buf2 = s_disp.buffer2;
    *size = DISPLAY_BUFFER_SIZE;
    return HAL_OK;
}
