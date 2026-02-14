/**
 * @file smp_frame_pool.c
 * @brief Static memory pool for SMP frame buffers
 *
 * Manages a fixed pool of 8 × 4096-byte buffers using a FreeRTOS
 * queue as a free-list. Prevents heap fragmentation during
 * high-frequency SMP frame processing.
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#include "smp_frame_pool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "smp_frame_pool";

/* Static frame buffer pool — 32 KB total */
static uint8_t frame_pool[FRAME_POOL_COUNT][FRAME_POOL_BUF_SIZE];

/* Free-list: queue of pointers to available buffers */
static QueueHandle_t free_frames = NULL;

void frame_pool_init(void)
{
    free_frames = xQueueCreate(FRAME_POOL_COUNT, sizeof(uint8_t *));
    if (free_frames == NULL) {
        ESP_LOGE(TAG, "Failed to create free-list queue");
        return;
    }

    for (int i = 0; i < FRAME_POOL_COUNT; i++) {
        uint8_t *ptr = frame_pool[i];
        if (xQueueSend(free_frames, &ptr, 0) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to enqueue buffer %d", i);
        }
    }

    ESP_LOGI(TAG, "Frame Pool initialized (%d x %d bytes = %d KB)",
             FRAME_POOL_COUNT, FRAME_POOL_BUF_SIZE,
             (FRAME_POOL_COUNT * FRAME_POOL_BUF_SIZE) / 1024);
}

uint8_t *frame_pool_alloc(TickType_t timeout)
{
    uint8_t *buf = NULL;

    if (free_frames == NULL) {
        ESP_LOGE(TAG, "Frame pool not initialized");
        return NULL;
    }

    if (xQueueReceive(free_frames, &buf, timeout) != pdTRUE) {
        ESP_LOGW(TAG, "Frame pool exhausted (timeout)");
        return NULL;
    }

    return buf;
}

void frame_pool_free(uint8_t *buf)
{
    if (buf == NULL) {
        ESP_LOGW(TAG, "Attempted to free NULL buffer");
        return;
    }

    if (free_frames == NULL) {
        ESP_LOGE(TAG, "Frame pool not initialized");
        return;
    }

    /* Validate pointer belongs to our pool */
    uintptr_t pool_start = (uintptr_t)&frame_pool[0][0];
    uintptr_t pool_end   = (uintptr_t)&frame_pool[FRAME_POOL_COUNT - 1][FRAME_POOL_BUF_SIZE - 1];
    uintptr_t ptr_val    = (uintptr_t)buf;

    if (ptr_val < pool_start || ptr_val > pool_end) {
        ESP_LOGE(TAG, "Attempted to free buffer outside pool (ptr=%p, pool=%p-%p)",
                 buf, (void *)pool_start, (void *)pool_end);
        return;
    }

    /* Verify alignment: pointer must be at the start of a buffer slot */
    size_t offset = ptr_val - pool_start;
    if (offset % FRAME_POOL_BUF_SIZE != 0) {
        ESP_LOGE(TAG, "Attempted to free misaligned buffer (ptr=%p, offset=%zu)",
                 buf, offset);
        return;
    }

    if (xQueueSend(free_frames, &buf, 0) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to return buffer to pool (double free?)");
    }
}
