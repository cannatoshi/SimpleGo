/**
 * @file smp_tasks.c
 * @brief FreeRTOS task manager for SimpleGo
 *
 * Creates three core tasks with dedicated core pinning:
 *   - Network Task (Core 0, Prio 7) — handles TLS socket I/O
 *   - App Task     (Core 1, Prio 6) — protocol logic & message routing
 *   - UI Task      (Core 1, Prio 5) — LVGL display & user input
 *
 * Inter-task communication uses ring buffers (Network <-> App)
 * and FreeRTOS queues (App <-> UI).
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#include "smp_tasks.h"
#include "smp_events.h"
#include "esp_log.h"

static const char *TAG = "smp_tasks";

/* ========================================================================
 * Task Configuration
 * ======================================================================== */

#define NETWORK_TASK_STACK  16384
#define NETWORK_TASK_PRIO   7
#define NETWORK_TASK_CORE   0

#define APP_TASK_STACK      12288
#define APP_TASK_PRIO       6
#define APP_TASK_CORE       1

#define UI_TASK_STACK       10240
#define UI_TASK_PRIO        5
#define UI_TASK_CORE        1

/* Ring buffer sizes */
#define NET_TO_APP_RB_SIZE  8192
#define APP_TO_NET_RB_SIZE  4096

/* Queue depths */
#define APP_TO_UI_DEPTH     10
#define UI_TO_APP_DEPTH     10

/* ========================================================================
 * Exported Handles
 * ======================================================================== */

TaskHandle_t network_task_handle = NULL;
TaskHandle_t app_task_handle     = NULL;
TaskHandle_t ui_task_handle      = NULL;

RingbufHandle_t net_to_app_rb = NULL;
RingbufHandle_t app_to_net_rb = NULL;

QueueHandle_t app_to_ui_queue = NULL;
QueueHandle_t ui_to_app_queue = NULL;

/* ========================================================================
 * Task Functions (Stubs — real logic comes in Phase 3)
 * ======================================================================== */

static void network_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Network Task started on Core %d", xPortGetCoreID());
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void app_task(void *pvParameters)
{
    ESP_LOGI(TAG, "App Task started on Core %d", xPortGetCoreID());
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void ui_task(void *pvParameters)
{
    ESP_LOGI(TAG, "UI Task started on Core %d", xPortGetCoreID());
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ========================================================================
 * Public API
 * ======================================================================== */

esp_err_t smp_tasks_init(void)
{
    /* Create ring buffers for Network <-> App communication */
    net_to_app_rb = xRingbufferCreate(NET_TO_APP_RB_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (net_to_app_rb == NULL) {
        ESP_LOGE(TAG, "Failed to create net_to_app ring buffer");
        return ESP_FAIL;
    }

    app_to_net_rb = xRingbufferCreate(APP_TO_NET_RB_SIZE, RINGBUF_TYPE_NOSPLIT);
    if (app_to_net_rb == NULL) {
        ESP_LOGE(TAG, "Failed to create app_to_net ring buffer");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Ring Buffers initialized (net->app: %d, app->net: %d)",
             NET_TO_APP_RB_SIZE, APP_TO_NET_RB_SIZE);

    /* Create queues for App <-> UI communication */
    app_to_ui_queue = xQueueCreate(APP_TO_UI_DEPTH, sizeof(ui_event_t));
    if (app_to_ui_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create app_to_ui queue");
        return ESP_FAIL;
    }

    ui_to_app_queue = xQueueCreate(UI_TO_APP_DEPTH, sizeof(app_event_t));
    if (ui_to_app_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create ui_to_app queue");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Queues initialized (app->ui: %d, ui->app: %d)",
             APP_TO_UI_DEPTH, UI_TO_APP_DEPTH);

    return ESP_OK;
}

esp_err_t smp_tasks_start(void)
{
    BaseType_t ret;

    ret = xTaskCreatePinnedToCore(
        network_task,
        "network_task",
        NETWORK_TASK_STACK,
        NULL,
        NETWORK_TASK_PRIO,
        &network_task_handle,
        NETWORK_TASK_CORE
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Network Task");
        return ESP_FAIL;
    }

    ret = xTaskCreatePinnedToCore(
        app_task,
        "app_task",
        APP_TASK_STACK,
        NULL,
        APP_TASK_PRIO,
        &app_task_handle,
        APP_TASK_CORE
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create App Task");
        return ESP_FAIL;
    }

    ret = xTaskCreatePinnedToCore(
        ui_task,
        "ui_task",
        UI_TASK_STACK,
        NULL,
        UI_TASK_PRIO,
        &ui_task_handle,
        UI_TASK_CORE
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI Task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "All tasks started successfully");
    return ESP_OK;
}
