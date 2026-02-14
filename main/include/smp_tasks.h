/**
 * @file smp_tasks.h
 * @brief FreeRTOS task manager for SimpleGo
 *
 * Creates and manages three core tasks (Network, App, UI),
 * their inter-task communication channels (ring buffers and queues),
 * and provides task handles for external access.
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#ifndef SMP_TASKS_H
#define SMP_TASKS_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Task handles */
extern TaskHandle_t network_task_handle;
extern TaskHandle_t app_task_handle;
extern TaskHandle_t ui_task_handle;

/* Ring buffers (Network <-> App) */
extern RingbufHandle_t net_to_app_rb;
extern RingbufHandle_t app_to_net_rb;

/* Queues (App <-> UI) */
extern QueueHandle_t app_to_ui_queue;
extern QueueHandle_t ui_to_app_queue;

/**
 * @brief Initialize queues and ring buffers
 *
 * Must be called before smp_tasks_start(). Creates all
 * inter-task communication channels.
 *
 * @return ESP_OK on success, ESP_FAIL on allocation failure
 */
esp_err_t smp_tasks_init(void);

/**
 * @brief Start all FreeRTOS tasks
 *
 * Creates Network Task (Core 0), App Task (Core 1),
 * and UI Task (Core 1) with appropriate priorities.
 *
 * @return ESP_OK on success, ESP_FAIL if task creation fails
 */
esp_err_t smp_tasks_start(void);

#ifdef __cplusplus
}
#endif

#endif /* SMP_TASKS_H */
