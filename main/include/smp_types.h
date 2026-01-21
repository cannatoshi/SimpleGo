/**
 * SimpleGo - Native SimpleX SMP Client for ESP32
 * smp_types.h - Data structures and constants
 * v0.1.13-alpha
 */

#ifndef SMP_TYPES_H
#define SMP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include "sodium.h"

// ============== Constants ==============

#define SMP_BLOCK_SIZE      16384
#define MAX_CONTACTS        10
#define NVS_NAMESPACE       "simplego"
#define SPKI_KEY_SIZE       44  // 12 header + 32 key

// SPKI headers for key encoding
extern const uint8_t ED25519_SPKI_HEADER[12];
extern const uint8_t X25519_SPKI_HEADER[12];

// Base64URL character set
extern const char base64url_chars[];

// ============== Contact Structure ==============

typedef struct {
    char name[32];                                    // Contact name
    uint8_t rcv_auth_secret[crypto_sign_SECRETKEYBYTES]; // 64 bytes
    uint8_t rcv_auth_public[crypto_sign_PUBLICKEYBYTES]; // 32 bytes
    uint8_t rcv_dh_secret[32];                        // X25519 secret
    uint8_t rcv_dh_public[32];                        // X25519 public
    uint8_t recipient_id[24];                         // Queue recipient ID
    uint8_t recipient_id_len;
    uint8_t sender_id[24];                            // Queue sender ID
    uint8_t sender_id_len;
    uint8_t srv_dh_public[32];                        // Server DH public key
    uint8_t have_srv_dh;                              // 1 if srv_dh_public is valid
    uint8_t active;                                   // 1=active, 0=slot free
} contact_t;

typedef struct {
    uint8_t num_contacts;
    contact_t contacts[MAX_CONTACTS];
} contacts_db_t;

// ============== Peer Queue Structure ==============

typedef struct {
    char host[64];
    int port;
    uint8_t key_hash[32];
    char key_hash_b64[48];
    uint8_t queue_id[32];
    int queue_id_len;
    uint8_t dh_public[32];
    int has_dh;
    int valid;
} peer_queue_t;

// ============== Peer Connection Structure ==============

typedef struct {
    int sock;
    void *ssl;           // mbedtls_ssl_context*
    void *conf;          // mbedtls_ssl_config*
    void *entropy;       // mbedtls_entropy_context*
    void *ctr_drbg;      // mbedtls_ctr_drbg_context*
    uint8_t session_id[32];
    uint8_t server_key_hash[32];
    bool connected;
} peer_connection_t;

// ============== Global State ==============

extern contacts_db_t contacts_db;
extern peer_queue_t pending_peer;
extern peer_connection_t peer_conn;
extern volatile bool wifi_connected;

#endif // SMP_TYPES_H
