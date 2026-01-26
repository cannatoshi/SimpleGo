/**
 * SimpleGo - smp_contacts.h
 * Contact management and NVS persistence
 */

#ifndef SMP_CONTACTS_H
#define SMP_CONTACTS_H

#include <stdbool.h>
#include "smp_types.h"
#include "mbedtls/ssl.h"

// NVS operations
bool load_contacts_from_nvs(void);
bool save_contacts_to_nvs(void);
void clear_all_contacts(void);

// Contact lookup
int find_contact_by_recipient_id(const uint8_t *recipient_id, uint8_t len);

// Contact display
void list_contacts(void);
void print_invitation_links(const uint8_t *ca_hash, const char *host, int port);

// Contact operations (require server connection)
int add_contact(mbedtls_ssl_context *ssl, uint8_t *block,
                const uint8_t *session_id, const char *name);
bool remove_contact(mbedtls_ssl_context *ssl, uint8_t *block,
                    const uint8_t *session_id, int index);

// Get invite link for UI
bool get_invite_link(const uint8_t *ca_hash, const char *host, int port, char *out_link, size_t out_len);

// Subscribe to all contacts
void subscribe_all_contacts(mbedtls_ssl_context *ssl, uint8_t *block,
                            const uint8_t *session_id);

#endif // SMP_CONTACTS_H
