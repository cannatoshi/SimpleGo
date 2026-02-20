/**
 * SimpleGo - smp_tasks.h
 * FreeRTOS task management for multi-task architecture
 */
#ifndef SMP_TASKS_H
#define SMP_TASKS_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "mbedtls/ssl.h"

// Task stack sizes (all allocated in PSRAM)
#define NETWORK_TASK_STACK  (12 * 1024)
// App logic runs on main task (64KB Internal SRAM stack, needed for NVS writes)
#define UI_TASK_STACK       (8 * 1024)

// Task priorities (higher = more important)
#define NETWORK_TASK_PRIO   7
// App logic: main task priority (not configurable here)
#define UI_TASK_PRIO        5

// Ring buffer sizes
// Phase 3: net->app carries full SMP blocks (SMP_BLOCK_SIZE = 16384)
// NOSPLIT ring buffers have significant internal overhead (headers, alignment, free-block)
// 2x block size gives room for at least 1 frame plus management overhead
#define NET_TO_APP_BUF_SIZE (2 * 16384 + 4096)
#define APP_TO_NET_BUF_SIZE 1024

// Task handles (extern for status queries)
extern TaskHandle_t network_task_handle;
extern TaskHandle_t ui_task_handle;

// Ring buffer handles
extern RingbufHandle_t net_to_app_buf;
extern RingbufHandle_t app_to_net_buf;

/**
 * Initialize task infrastructure (queues, ring buffers, frame pool).
 * Call BEFORE smp_tasks_start().
 *
 * @return 0 on success, -1 on error
 */
int smp_tasks_init(void);

/**
 * Start all tasks after successful smp_connect().
 * Network task on Core 0, App + UI on Core 1.
 *
 * @param ssl_context  Active SSL context from smp_connect()
 * @param session_id   32-byte TLS session ID for ACK signing
 * @return 0 on success, -1 on error
 */
int smp_tasks_start(mbedtls_ssl_context *ssl_context, const uint8_t *session_id, int sock_fd);

/**
 * Graceful shutdown of all tasks.
 */
void smp_tasks_stop(void);

/**
 * Run app logic (ring buffer read, parse, decrypt, ACK).
 * MUST be called from main task (needs Internal SRAM stack for NVS writes).
 * This function blocks (infinite loop) until connection error.
 */
void smp_app_run(QueueHandle_t kbd_queue);

/*============================================================================
 * UI EVENT BRIDGE (Session 32: Demonstration Mode)
 *==========================================================================*/

/** UI event types for cross-task communication */
typedef enum {
    UI_EVT_MESSAGE = 0,     // Chat message (incoming or outgoing status)
    UI_EVT_NAVIGATE,        // Navigate to screen
    UI_EVT_SET_CONTACT,     // Set contact name in chat header
    UI_EVT_DELIVERY_STATUS, // Update delivery status on outgoing message
} ui_event_type_t;

/** Delivery status for outgoing messages */
typedef enum {
    MSG_STATUS_SENDING = 0, // Queued, not yet sent
    MSG_STATUS_SENT,        // Server accepted
    MSG_STATUS_DELIVERED,   // Peer received (delivery receipt)
    MSG_STATUS_FAILED,      // Send failed
} msg_delivery_status_t;

/** UI event structure (fits in FreeRTOS queue) */
typedef struct {
    ui_event_type_t type;
    bool is_outgoing;                // UI_EVT_MESSAGE: true=ours, false=theirs
    uint8_t screen;                  // UI_EVT_NAVIGATE: ui_screen_t value
    msg_delivery_status_t status;    // UI_EVT_DELIVERY_STATUS
    uint32_t msg_seq;                // Sequence number for status updates
    char text[256];                  // Message text or contact name
} ui_event_t;

/** Global queue: protocol tasks -> LVGL timer */
extern QueueHandle_t app_to_ui_queue;

/** Push functions (safe to call from any task on any core) */
void smp_notify_ui_message(const char *text, bool is_outgoing, uint32_t msg_seq);
void smp_notify_ui_navigate(uint8_t screen);
void smp_notify_ui_contact(const char *name);
void smp_notify_ui_delivery_status(uint32_t msg_seq, msg_delivery_status_t status);

/**
 * @brief Register mapping between UI sequence and protocol msg_id
 * Called after successful send to enable receipt matching
 */
void smp_register_msg_mapping(uint32_t ui_seq, uint64_t protocol_msg_id);

/**
 * @brief Notify UI that a delivery receipt was received
 * Looks up the seq by msg_id and pushes DELIVERED status
 */
void smp_notify_receipt_received(uint64_t protocol_msg_id);

/**
 * @brief Set which contact is currently active for sending
 * Called by UI when user selects a contact from the list.
 * @param idx  Index into contacts_db.contacts[]
 */
void smp_set_active_contact(int idx);

/**
 * @brief Get currently active contact index
 * @return Index into contacts_db.contacts[], or 0 if none set
 */
int smp_get_active_contact(void);

#endif // SMP_TASKS_H
