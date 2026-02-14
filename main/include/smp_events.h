/**
 * @file smp_events.h
 * @brief Event types for inter-task communication
 *
 * Defines all structs and enums used for communication between
 * Network Task, App Task, and UI Task via FreeRTOS queues
 * and ring buffers.
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#ifndef SMP_EVENTS_H
#define SMP_EVENTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * App Event Types (UI → App, Network → App)
 * ======================================================================== */

/**
 * @brief Events sent TO the App Task
 */
typedef enum {
    SMP_FRAME_RECEIVED = 0,   /**< Raw SMP frame from network */
    SEND_MESSAGE,              /**< User wants to send a message */
    CONTACT_SELECTED,          /**< User selected a contact in UI */
    CONNECTION_LOST,           /**< Network connection dropped */
    CONNECTION_READY,          /**< Network connection established */
} app_event_type_t;

/**
 * @brief Event payload for App Task
 */
typedef struct {
    app_event_type_t type;
    union {
        struct {
            uint8_t *data;     /**< Pointer to frame data (from frame pool) */
            size_t len;        /**< Length of frame data */
        } frame;               /**< For SMP_FRAME_RECEIVED */

        struct {
            uint8_t *data;     /**< Message plaintext (caller-allocated) */
            size_t len;        /**< Message length */
            int contact_idx;   /**< Target contact index */
        } send_msg;            /**< For SEND_MESSAGE */

        struct {
            int contact_idx;   /**< Selected contact index */
        } contact;             /**< For CONTACT_SELECTED */
    };
} app_event_t;

/* ========================================================================
 * UI Event Types (App → UI)
 * ======================================================================== */

/**
 * @brief Events sent TO the UI Task
 */
typedef enum {
    NEW_MESSAGE = 0,           /**< Decrypted message to display */
    MSG_DELIVERED,             /**< Delivery receipt confirmed */
    CONNECTION_STATUS,         /**< Connection state changed */
    UI_ERROR,                  /**< Error to display to user */
} ui_event_type_t;

/**
 * @brief Event payload for UI Task
 */
typedef struct {
    ui_event_type_t type;
    union {
        struct {
            char text[256];    /**< Decrypted message text */
            int contact_idx;   /**< Sender contact index */
        } message;             /**< For NEW_MESSAGE */

        struct {
            int contact_idx;   /**< Contact whose msg was delivered */
        } delivered;           /**< For MSG_DELIVERED */

        struct {
            bool connected;    /**< true = connected, false = disconnected */
        } conn_status;         /**< For CONNECTION_STATUS */

        struct {
            char text[128];    /**< Error description */
        } error;               /**< For UI_ERROR */
    };
} ui_event_t;

/* ========================================================================
 * Network Send Type (App → Network)
 * ======================================================================== */

/**
 * @brief Data to send over network (passed via ring buffer)
 */
typedef struct {
    uint8_t *data;             /**< Pointer to encrypted frame data */
    size_t len;                /**< Length of data */
} net_send_t;

#ifdef __cplusplus
}
#endif

#endif /* SMP_EVENTS_H */
