/**
 * SimpleGo - smp_queue.h
 * SMP Queue Management (NEW, SUB, ACK commands)
 * v0.1.15-alpha
 */

#ifndef SMP_QUEUE_H
#define SMP_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

// Queue ID sizes
#define QUEUE_ID_SIZE       24
#define DH_PUBLIC_SIZE      32
#define AUTH_KEY_SIZE       32

// Our created queue info
typedef struct {
    // IDs from server
    uint8_t rcv_id[QUEUE_ID_SIZE];      // Our recipient ID (for receiving)
    int rcv_id_len;
    uint8_t snd_id[QUEUE_ID_SIZE];      // Sender ID (give to peer)
    int snd_id_len;
    
    // Server's DH key
    uint8_t srv_dh_public[DH_PUBLIC_SIZE];
    
    // Our keys for this queue
    uint8_t rcv_auth_public[AUTH_KEY_SIZE];   // Ed25519 public (signing)
    uint8_t rcv_auth_private[64];              // Ed25519 private
    uint8_t rcv_dh_public[DH_PUBLIC_SIZE];    // X25519 public
    uint8_t rcv_dh_private[DH_PUBLIC_SIZE];   // X25519 private
    
    // Shared secret (our_dh_private * srv_dh_public)
    uint8_t shared_secret[DH_PUBLIC_SIZE];
    
    // Server info (for SMPQueueInfo encoding)
    char server_host[64];
    int server_port;
    uint8_t server_key_hash[32];
    
    bool valid;
} our_queue_t;

// Global instance
extern our_queue_t our_queue;

/**
 * Create a new receive queue on our SMP server
 * This sends NEW command and parses IDS response
 * 
 * @param host SMP server hostname
 * @param port SMP server port (usually 443)
 * @return true if queue created successfully
 */
bool queue_create(const char *host, int port);

/**
 * Subscribe to our queue to receive messages
 * Sends SUB command
 * 
 * @return true if subscribed
 */
bool queue_subscribe(void);

/**
 * Encode our queue as SMPQueueInfo for AgentConnInfoReply
 * 
 * @param buf Output buffer
 * @param max_len Buffer size
 * @return Encoded length, or -1 on error
 */
int queue_encode_info(uint8_t *buf, int max_len);

/**
 * Disconnect from our queue server
 */
void queue_disconnect(void);

#endif // SMP_QUEUE_H