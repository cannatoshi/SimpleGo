/**
 * @file smp_msg_router.h
 * @brief Central message router for the App Task
 *
 * Provides the main event loop that processes events from
 * both the Network Task (via ring buffer) and the UI Task
 * (via queue), routing them to appropriate handlers.
 *
 * @copyright Copyright (c) 2026 SimpleGo Project
 * @license AGPL-3.0
 */

#ifndef SMP_MSG_ROUTER_H
#define SMP_MSG_ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the message router
 *
 * Sets up internal state for event processing.
 * Must be called before msg_router_run().
 */
void msg_router_init(void);

/**
 * @brief Run the message router event loop
 *
 * This function blocks and runs the central event loop.
 * It is called from the App Task and never returns.
 *
 * Checks:
 *   1. net_to_app_rb ring buffer for incoming frames
 *   2. ui_to_app_queue for UI events
 *   3. Sleeps briefly if no events pending
 */
void msg_router_run(void);

#ifdef __cplusplus
}
#endif

#endif /* SMP_MSG_ROUTER_H */
