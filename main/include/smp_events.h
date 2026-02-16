/**
 * SimpleGo - smp_events.h
 * Event types for inter-task communication
 */
#ifndef SMP_EVENTS_H
#define SMP_EVENTS_H

#include <stdint.h>
#include <stddef.h>

// Event types for task communication
typedef enum {
    SMP_EVENT_FRAME_RECEIVED,   // Network → App: raw frame from server
    SMP_EVENT_FRAME_SEND,       // App → Network: frame to send to server
    SMP_EVENT_MSG_DECODED,      // App → UI: decrypted message ready
    SMP_EVENT_ACK_READY,        // App → Network: ACK/receipt to send
    SMP_EVENT_RECONNECT,        // Any → Network: trigger reconnection
    SMP_EVENT_SHUTDOWN          // Any → All: graceful shutdown
} smp_event_type_t;

// Frame source identifier
typedef enum {
    SMP_SOURCE_NETWORK,
    SMP_SOURCE_APP,
    SMP_SOURCE_UI
} smp_source_t;

// Frame container (4KB data buffer)
typedef struct {
    uint8_t data[4096];
    size_t len;
    smp_source_t source;
} smp_frame_t;

// Event container for inter-task queues
typedef struct {
    smp_event_type_t event_type;
    void *payload;
    size_t payload_len;
    uint32_t timestamp;
} smp_event_t;

// === Network Task Command Protocol (App → Network via Ring Buffer) ===
// Phase 3 T4: Commands sent from App Task to Network Task

typedef enum {
    NET_CMD_SEND_ACK,          // Send ACK for received message
    NET_CMD_SUBSCRIBE_ALL,     // Re-subscribe all contacts after handshake
} net_cmd_type_t;

typedef struct {
    net_cmd_type_t cmd;
    // For NET_CMD_SEND_ACK:
    uint8_t recipient_id[24];
    int recipient_id_len;
    uint8_t msg_id[24];
    int msg_id_len;
    uint8_t rcv_auth_secret[64];
    // NET_CMD_SUBSCRIBE_ALL needs no extra data
} net_cmd_t;

#endif // SMP_EVENTS_H
