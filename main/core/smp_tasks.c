/**
 * SimpleGo - smp_tasks.c
 * FreeRTOS task management for multi-task architecture
 *
 * All heavy allocations use PSRAM (SPIRAM) to preserve internal
 * SRAM for TLS/WiFi operations (~40KB needed).
 */

#include "smp_tasks.h"
#include "smp_frame_pool.h"
#include "smp_events.h"
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "SMP_TASKS";

// Task handles
TaskHandle_t network_task_handle = NULL;
TaskHandle_t app_task_handle = NULL;
TaskHandle_t ui_task_handle = NULL;

// Ring buffer handles
RingbufHandle_t net_to_app_buf = NULL;
RingbufHandle_t app_to_net_buf = NULL;

// Stored SSL context (set by smp_tasks_start, used by network task later)
static mbedtls_ssl_context *s_ssl = NULL;

// Helper: log both internal and PSRAM heap
static void log_heap(const char *label)
{
    ESP_LOGI(TAG, "  [%s] Internal: %lu, PSRAM: %lu",
             label,
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

// --- Task functions (empty loops for Phase 2 proof-of-concept) ---

static void network_task(void *arg)
{
    ESP_LOGI(TAG, "Network task running on core %d", xPortGetCoreID());
    log_heap("net_task");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void app_task(void *arg)
{
    ESP_LOGI(TAG, "App task running on core %d", xPortGetCoreID());
    log_heap("app_task");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void ui_task(void *arg)
{
    ESP_LOGI(TAG, "UI task running on core %d", xPortGetCoreID());
    log_heap("ui_task");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// --- Public API ---

int smp_tasks_init(void)
{
    ESP_LOGI(TAG, "Initializing task infrastructure...");
    log_heap("before_init");

    // Initialize frame pool (4 x 4KB = 16KB in PSRAM)
    smp_frame_pool_init();
    ESP_LOGI(TAG, "  Frame pool: %d frames available (PSRAM)",
             smp_frame_pool_available());
    log_heap("after_pool");

    // Create ring buffers in PSRAM (NOSPLIT = items not split across wrap)
    net_to_app_buf = xRingbufferCreateWithCaps(NET_TO_APP_BUF_SIZE,
                                                RINGBUF_TYPE_NOSPLIT,
                                                MALLOC_CAP_SPIRAM);
    if (!net_to_app_buf) {
        ESP_LOGE(TAG, "Failed to create net_to_app ring buffer");
        return -1;
    }

    app_to_net_buf = xRingbufferCreateWithCaps(APP_TO_NET_BUF_SIZE,
                                                RINGBUF_TYPE_NOSPLIT,
                                                MALLOC_CAP_SPIRAM);
    if (!app_to_net_buf) {
        ESP_LOGE(TAG, "Failed to create app_to_net ring buffer");
        vRingbufferDeleteWithCaps(net_to_app_buf);
        net_to_app_buf = NULL;
        return -1;
    }

    ESP_LOGI(TAG, "  Ring buffers (PSRAM): net->app %dB, app->net %dB",
             NET_TO_APP_BUF_SIZE, APP_TO_NET_BUF_SIZE);
    log_heap("after_init");

    return 0;
}

int smp_tasks_start(mbedtls_ssl_context *ssl_context)
{
    if (!ssl_context) {
        ESP_LOGE(TAG, "SSL context is NULL");
        return -1;
    }

    s_ssl = ssl_context;

    ESP_LOGI(TAG, "Starting tasks (all in PSRAM)...");
    log_heap("before_tasks");

    // Network task on Core 0 (stack in PSRAM)
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(
        network_task, "net_task",
        NETWORK_TASK_STACK, NULL,
        NETWORK_TASK_PRIO, &network_task_handle, 0,
        MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create network task");
        return -1;
    }

    // App task on Core 1 (stack in PSRAM)
    ret = xTaskCreatePinnedToCoreWithCaps(
        app_task, "app_task",
        APP_TASK_STACK, NULL,
        APP_TASK_PRIO, &app_task_handle, 1,
        MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create app task");
        smp_tasks_stop();
        return -1;
    }

    // UI task on Core 1 (stack in PSRAM)
    ret = xTaskCreatePinnedToCoreWithCaps(
        ui_task, "ui_task",
        UI_TASK_STACK, NULL,
        UI_TASK_PRIO, &ui_task_handle, 1,
        MALLOC_CAP_SPIRAM);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UI task");
        smp_tasks_stop();
        return -1;
    }

    ESP_LOGI(TAG, "All tasks started successfully (all PSRAM)");
    log_heap("after_tasks");

    return 0;
}

void smp_tasks_stop(void)
{
    ESP_LOGW(TAG, "Stopping tasks...");

    if (network_task_handle) {
        vTaskDeleteWithCaps(network_task_handle);
        network_task_handle = NULL;
    }
    if (app_task_handle) {
        vTaskDeleteWithCaps(app_task_handle);
        app_task_handle = NULL;
    }
    if (ui_task_handle) {
        vTaskDeleteWithCaps(ui_task_handle);
        ui_task_handle = NULL;
    }

    if (net_to_app_buf) {
        vRingbufferDeleteWithCaps(net_to_app_buf);
        net_to_app_buf = NULL;
    }
    if (app_to_net_buf) {
        vRingbufferDeleteWithCaps(app_to_net_buf);
        app_to_net_buf = NULL;
    }

    s_ssl = NULL;

    ESP_LOGW(TAG, "All tasks stopped");
}
