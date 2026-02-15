/**
 * SimpleGo - Native SimpleX SMP Client for ESP32
 * v0.1.17-alpha - Phase 3: FreeRTOS Task Architecture
 * github.com/cannatoshi/SimpleGo
 * Autor: cannatoshi
 *
 * Phase 3 Architecture:
 * - main.c: Init sequence only (no blocking loop)
 * - Network Task (Core 0): TLS read/write via ring buffers
 * - App Task (Core 1): Protocol processing, decrypt, keyboard
 * - UI Task (Core 1): LVGL display (stub)
 *
 * Modules:
 * - smp_tasks.c     (Task manager, TLS init, task functions)
 * - smp_msg_router.c (Frame processing, decrypt pipeline)
 * - smp_ack.c       (ACK build + send)
 * - smp_e2e.c       (Reply Queue E2E decrypt pipeline)
 * - smp_agent.c     (Agent protocol: PrivHeader dispatch, ratchet, Zstd)
 */

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"

#include "sodium.h"

// SimpleGo modules
#include "smp_types.h"
#include "smp_network.h"
#include "smp_ratchet.h"
#include "smp_contacts.h"
#include "smp_peer.h"
#include "smp_queue.h"
#include "smp_handshake.h"
#include "smp_wifi.h"
#include "smp_storage.h"
#include "smp_frame_pool.h"
#include "smp_tasks.h"
#include "smp_x448.h"

// T-Deck Display Driver
#include "tdeck_display.h"
#include "tdeck_lvgl.h"
#include "tdeck_touch.h"
#include "tdeck_keyboard.h"

// UI System
#include "ui_manager.h"
#include "ui/screens/ui_connect.h"
#include "ui_theme.h"

static const char *TAG = "SMP";

// ============== CONFIG ==============
#define SMP_HOST      "smp1.simplexonflux.com"
#define SMP_PORT      5223

// ============== Keyboard Task ==============

static void keyboard_task(void *arg)
{
    (void)arg;
    static char buf[256] = {0};
    int pos = 0;

    ESP_LOGI("KBD_TASK", "⌨️ Keyboard task started (polling every 50ms)");

    while (1) {
        char key = tdeck_keyboard_read();

        if (key != 0) {
            if (key == '\r' || key == '\n') {
                if (pos > 0) {
                    buf[pos] = '\0';
                    ESP_LOGI("KBD_TASK", "⌨️ ENTER → \"%s\"", buf);
                    xQueueSend(kbd_msg_queue, buf, 0);
                    pos = 0;
                    buf[0] = '\0';
                }
            } else if (key == 0x08) {
                if (pos > 0) {
                    pos--;
                    buf[pos] = '\0';
                }
                ESP_LOGI("KBD_TASK", "⌨️ Buffer: \"%s\"", buf);
            } else if (key >= 0x20 && key < 0x7F && pos < 254) {
                buf[pos++] = key;
                buf[pos] = '\0';
                ESP_LOGI("KBD_TASK", "⌨️ Buffer: \"%s\"", buf);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ============== App Main ==============

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "SimpleGo v0.1.17-alpha starting...");

    if (sodium_init() < 0) {
        ESP_LOGE(TAG, "libsodium init failed!");
        return;
    }
    ESP_LOGI(TAG, "libsodium initialized");

    if (!x448_init()) {
        ESP_LOGE(TAG, "X448 init failed!");
        return;
    }
    ESP_LOGI(TAG, "X448 initialized (wolfSSL)");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Storage Phase 1: NVS only (before display, no SPI conflict)
    smp_storage_init();
    smp_storage_print_info();
    smp_storage_self_test();

    // Auftrag 51b: Frame Pool + Task infrastructure
    frame_pool_init();
    smp_tasks_init();

    // Display + LVGL Init
    ESP_LOGI(TAG, "Initializing Display...");
    ret = tdeck_display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Initializing LVGL...");
        ret = tdeck_lvgl_init();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "LVGL init failed: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(TAG, "Initializing Touch...");
            ret = tdeck_touch_init();
            if (ret == ESP_OK) {
                tdeck_touch_register_lvgl();
                ESP_LOGI(TAG, "Touch input enabled!");
            } else {
                ESP_LOGW(TAG, "Touch init failed - continuing without touch");
            }

            // Keyboard init (I2C bus shared with touch)
            ESP_LOGI(TAG, "Initializing Keyboard...");
            ret = tdeck_keyboard_init();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "Keyboard input enabled! ⌨️");
                // Create keyboard message queue (consumed by App Task)
                kbd_msg_queue = xQueueCreate(4, 256);
                xTaskCreate(keyboard_task, "kbd_task", 4096, NULL, 5, NULL);
            } else {
                ESP_LOGW(TAG, "Keyboard init failed - continuing without keyboard");
            }

            ESP_LOGI(TAG, "Initializing UI...");
            ui_manager_init();

            tdeck_lvgl_start();

            vTaskDelay(pdMS_TO_TICKS(50));
            tdeck_display_backlight(100);
        }
    }

    // Storage Phase 2: SD card (after display owns SPI bus)
    smp_storage_init_sd();

    smp_wifi_init();

    ESP_LOGI(TAG, "Waiting for WiFi...");
    while (!wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // ========== Session Restoration or Fresh Start ==========
    bool session_restored = false;

    if (smp_storage_exists("rat_00") && smp_storage_exists("queue_our")) {
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "╔══════════════════════════════════════════════════════════════╗");
        ESP_LOGI(TAG, "║  📂 RESTORING PREVIOUS SESSION                               ║");
        ESP_LOGI(TAG, "╚══════════════════════════════════════════════════════════════╝");
        ESP_LOGI(TAG, "");

        bool ratchet_ok = ratchet_load_state(0);
        bool queue_ok = queue_load_credentials();
        bool peer_ok = peer_load_state();
        bool hand_ok = handshake_load_state();

        if (ratchet_ok && queue_ok) {
            session_restored = true;
            ESP_LOGI(TAG, "✅ Session restored! Skipping queue creation. (peer=%s, hand=%s)",
                     peer_ok ? "✅" : "⚠️",
                     hand_ok ? "✅" : "⚠️");
        } else {
            ESP_LOGW(TAG, "⚠️ Partial restore failed (ratchet=%d, queue=%d) — fresh start",
                     ratchet_ok, queue_ok);
        }
    } else {
        ESP_LOGI(TAG, "No previous session found — fresh start");
    }

    if (!session_restored) {
        // Create Reply Queue (fresh start only)
        ESP_LOGI(TAG, "Creating reply queue on %s:%d...", SMP_HOST, SMP_PORT);

        if (!queue_create(SMP_HOST, SMP_PORT)) {
            ESP_LOGE(TAG, "Failed to create reply queue!");
            ESP_LOGW(TAG, "  Continuing without reply queue...");
        } else {
            ESP_LOGI(TAG, "Reply queue created! sndId: %02x%02x%02x%02x... (%d bytes)",
                     our_queue.snd_id[0], our_queue.snd_id[1],
                     our_queue.snd_id[2], our_queue.snd_id[3],
                     our_queue.snd_id_len);
        }

        queue_disconnect();
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // ========== Phase 3: Initialize network + start tasks ==========
    if (smp_net_init(SMP_HOST, SMP_PORT, session_restored) != ESP_OK) {
        ESP_LOGE(TAG, "Network init failed! Halting.");
        while (1) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    smp_tasks_start();
    ESP_LOGI(TAG, "All tasks started — SimpleGo v0.1.17-alpha ready");

    // Main thread is done — tasks handle everything now
    ESP_LOGI(TAG, "Main thread idle (tasks running)");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
