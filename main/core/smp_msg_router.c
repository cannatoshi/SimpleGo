/**
 * @file smp_msg_router.c
 * @brief Central message router for the App Task
 *
 * Skeleton implementation of the event loop. Checks ring buffer
 * and queue for incoming events and logs them. Real protocol
 * logic will be migrated here in Phase 3.
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#include "smp_msg_router.h"
#include "smp_tasks.h"
#include "smp_events.h"
#include "smp_frame_pool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "smp_msg_router";

void msg_router_init(void)
{
    ESP_LOGI(TAG, "Message Router initialized");
}

void msg_router_run(void)
{
    ESP_LOGI(TAG, "Message Router event loop started");

    while (1) {
        bool had_event = false;

        /* 1. Check ring buffer for frames from Network Task */
        size_t item_size = 0;
        void *item = xRingbufferReceive(net_to_app_rb, &item_size, 0);
        if (item != NULL) {
            ESP_LOGI(TAG, "Received frame from network (%zu bytes)", item_size);
            /* TODO Phase 3: Process SMP frame */
            vRingbufferReturnItem(net_to_app_rb, item);
            had_event = true;
        }

        /* 2. Check queue for events from UI Task */
        app_event_t ui_event;
        if (xQueueReceive(ui_to_app_queue, &ui_event, 0) == pdTRUE) {
            ESP_LOGI(TAG, "Received UI event (type: %d)", ui_event.type);
            /* TODO Phase 3: Handle UI event */
            had_event = true;
        }

        /* 3. Sleep if no events to avoid busy-waiting */
        if (!had_event) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
