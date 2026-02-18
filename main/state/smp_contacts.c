/**
 * SimpleGo - smp_contacts.c
 * Contact management and NVS persistence
 */

#include "smp_contacts.h"
#include "smp_types.h"
#include "smp_utils.h"
#include "smp_network.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "sodium.h"
#include "smp_queue.h"
#include "freertos/ringbuf.h"

// Ring buffer for forwarding batched TX2 frames to App Task
extern RingbufHandle_t net_to_app_buf;

static const char *TAG = "SMP_CONT";

// ============== NVS Functions ==============

bool load_contacts_from_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "NVS: No saved contacts found");
        memset(&contacts_db, 0, sizeof(contacts_db));
        return false;
    }
    
    size_t required_size = sizeof(contacts_db_t);
    err = nvs_get_blob(handle, "contacts", &contacts_db, &required_size);
    nvs_close(handle);
    
    if (err == ESP_OK && contacts_db.num_contacts > 0) {
        ESP_LOGI(TAG, "NVS: Loaded %d contact(s)!", contacts_db.num_contacts);
        for (int i = 0; i < MAX_CONTACTS; i++) {
            if (contacts_db.contacts[i].active) {
                ESP_LOGI(TAG, "      [%d] %s (rcvId: %02x%02x%02x%02x...)", 
                         i, contacts_db.contacts[i].name,
                         contacts_db.contacts[i].recipient_id[0],
                         contacts_db.contacts[i].recipient_id[1],
                         contacts_db.contacts[i].recipient_id[2],
                         contacts_db.contacts[i].recipient_id[3]);
            }
        }
        return true;
    }
    
    ESP_LOGI(TAG, "NVS: No saved contacts found");
    memset(&contacts_db, 0, sizeof(contacts_db));
    return false;
}

bool save_contacts_to_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS: Failed to open for writing");
        return false;
    }
    
    err = nvs_set_blob(handle, "contacts", &contacts_db, sizeof(contacts_db_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS: Failed to save contacts");
        nvs_close(handle);
        return false;
    }
    
    err = nvs_commit(handle);
    nvs_close(handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "NVS: Contacts saved!");
        return true;
    }
    
    ESP_LOGE(TAG, "NVS: Commit failed");
    return false;
}

void clear_all_contacts(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        nvs_erase_key(handle, "contacts");
        nvs_commit(handle);
        nvs_close(handle);
        memset(&contacts_db, 0, sizeof(contacts_db));
        ESP_LOGI(TAG, "NVS: All contacts cleared!");
    }
}

// ============== Contact Lookup ==============

int find_contact_by_recipient_id(const uint8_t *recipient_id, uint8_t len) {
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (contacts_db.contacts[i].active && 
            contacts_db.contacts[i].recipient_id_len == len &&
            memcmp(contacts_db.contacts[i].recipient_id, recipient_id, len) == 0) {
            return i;
        }
    }
    return -1;
}

// ============== Contact Display ==============

void list_contacts(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "ðŸ“‹ Contact List (%d active):", contacts_db.num_contacts);
    ESP_LOGI(TAG, "â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€");
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (contacts_db.contacts[i].active) {
            contact_t *c = &contacts_db.contacts[i];
            ESP_LOGI(TAG, "  [%d] %s", i, c->name);
            ESP_LOGI(TAG, "      rcvId: %02x%02x%02x%02x%02x%02x...", 
                     c->recipient_id[0], c->recipient_id[1], 
                     c->recipient_id[2], c->recipient_id[3],
                     c->recipient_id[4], c->recipient_id[5]);
            ESP_LOGI(TAG, "      srvDH: %s", c->have_srv_dh ? "âœ…" : "âŒ");
        }
    }
    ESP_LOGI(TAG, "â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€");
    ESP_LOGI(TAG, "");
}

void print_invitation_links(const uint8_t *ca_hash, const char *host, int port) {
    char hash_b64[64];
    char snd_b64[64];
    char dh_b64[80];
    char smp_uri[512];
    char smp_uri_encoded[2048];
    
    base64url_encode(ca_hash, 32, hash_b64, sizeof(hash_b64));
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "ðŸ”— SIMPLEX CONTACT LINKS");
    ESP_LOGI(TAG, "â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");
    ESP_LOGI(TAG, "Server keyHash: %s", hash_b64);
    ESP_LOGI(TAG, "");
    
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        
        contact_t *c = &contacts_db.contacts[i];
        
        base64url_encode(c->sender_id, c->sender_id_len, snd_b64, sizeof(snd_b64));
        
        // Encode dhPublicKey as SPKI + Base64URL with padding
        uint8_t dh_spki[44];
        memcpy(dh_spki, X25519_SPKI_HEADER, 12);
        memcpy(dh_spki + 12, c->rcv_dh_public, 32);
        
        {
            int in_len = 44;
            int j = 0;
            for (int k = 0; k < in_len; ) {
                uint32_t octet_a = k < in_len ? dh_spki[k++] : 0;
                uint32_t octet_b = k < in_len ? dh_spki[k++] : 0;
                uint32_t octet_c = k < in_len ? dh_spki[k++] : 0;
                uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
                
                dh_b64[j++] = base64url_chars[(triple >> 18) & 0x3F];
                dh_b64[j++] = base64url_chars[(triple >> 12) & 0x3F];
                dh_b64[j++] = base64url_chars[(triple >> 6) & 0x3F];
                dh_b64[j++] = base64url_chars[triple & 0x3F];
            }
            int mod = in_len % 3;
            if (mod == 1) {
                dh_b64[j-2] = '=';
                dh_b64[j-1] = '=';
            } else if (mod == 2) {
                dh_b64[j-1] = '=';
            }
            dh_b64[j] = '\0';
        }
        
        // Pre-encode = as %3D for double-encoding
        char dh_with_encoded_padding[100];
        {
            int j = 0;
            for (int k = 0; dh_b64[k] && j < 95; k++) {
                if (dh_b64[k] == '=') {
                    dh_with_encoded_padding[j++] = '%';
                    dh_with_encoded_padding[j++] = '3';
                    dh_with_encoded_padding[j++] = 'D';
                } else {
                    dh_with_encoded_padding[j++] = dh_b64[k];
                }
            }
            dh_with_encoded_padding[j] = '\0';
        }
        
        snprintf(smp_uri, sizeof(smp_uri), 
                 "smp://%s@%s:%d/%s#/?v=1-4&dh=%s&q=c",
                 hash_b64, host, port, snd_b64, dh_with_encoded_padding);
        
        url_encode(smp_uri, smp_uri_encoded, sizeof(smp_uri_encoded));
        
        ESP_LOGI(TAG, "ðŸ“± [%d] %s", i, c->name);
        ESP_LOGI(TAG, "â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€");
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "   ðŸŒ SimpleX Contact Link (COPY THIS!):");
        printf("   https://simplex.chat/contact#/?v=2-7&smp=%s\n", smp_uri_encoded);
        
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "   ðŸ“² Direct App Link:");
        printf("   simplex:/contact#/?v=2-7&smp=%s\n", smp_uri_encoded);
        
        ESP_LOGI(TAG, "");
    }
    
    ESP_LOGI(TAG, "â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•");
    ESP_LOGI(TAG, "");
}

// ============== Contact Operations ==============

int add_contact(mbedtls_ssl_context *ssl, uint8_t *block,
                const uint8_t *session_id, const char *name) {
    int slot = -1;
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) {
        ESP_LOGE(TAG, "âŒ No free contact slot! Max %d contacts.", MAX_CONTACTS);
        return -1;
    }
    
    contact_t *c = &contacts_db.contacts[slot];
    memset(c, 0, sizeof(contact_t));
    strncpy(c->name, name, sizeof(c->name) - 1);
    
    ESP_LOGI(TAG, "âž• Creating contact '%s' in slot %d...", name, slot);
    
    // Generate Ed25519 keypair
    uint8_t seed[32];
    esp_fill_random(seed, 32);
    crypto_sign_seed_keypair(c->rcv_auth_public, c->rcv_auth_secret, seed);
    
    // Generate X25519 keypair
    esp_fill_random(c->rcv_dh_secret, 32);
    crypto_scalarmult_base(c->rcv_dh_public, c->rcv_dh_secret);
    
    ESP_LOGI(TAG, "      Keys generated!");
    
    // Build SPKI-encoded keys
    uint8_t rcv_auth_spki[SPKI_KEY_SIZE];
    memcpy(rcv_auth_spki, ED25519_SPKI_HEADER, 12);
    memcpy(rcv_auth_spki + 12, c->rcv_auth_public, 32);
    
    uint8_t rcv_dh_spki[SPKI_KEY_SIZE];
    memcpy(rcv_dh_spki, X25519_SPKI_HEADER, 12);
    memcpy(rcv_dh_spki + 12, c->rcv_dh_public, 32);
    
    // Build NEW command
    uint8_t trans_body[256];
    int pos = 0;
    
    trans_body[pos++] = 1;
    trans_body[pos++] = '0' + slot;
    trans_body[pos++] = 0;  // Empty entityId
    
    trans_body[pos++] = 'N';
    trans_body[pos++] = 'E';
    trans_body[pos++] = 'W';
    trans_body[pos++] = ' ';
    
    trans_body[pos++] = SPKI_KEY_SIZE;
    memcpy(&trans_body[pos], rcv_auth_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;
    
    trans_body[pos++] = SPKI_KEY_SIZE;
    memcpy(&trans_body[pos], rcv_dh_spki, SPKI_KEY_SIZE);
    pos += SPKI_KEY_SIZE;
    
    trans_body[pos++] = 'S';  // subMode
    
    int trans_body_len = pos;
    
    // Sign
    uint8_t to_sign[1 + 32 + 256];
    int sign_pos = 0;
    to_sign[sign_pos++] = 32;
    memcpy(&to_sign[sign_pos], session_id, 32);
    sign_pos += 32;
    memcpy(&to_sign[sign_pos], trans_body, trans_body_len);
    sign_pos += trans_body_len;
    
    uint8_t signature[crypto_sign_BYTES];
    crypto_sign_detached(signature, NULL, to_sign, sign_pos, c->rcv_auth_secret);
    
    // Build transmission
    uint8_t transmission[256];
    int tpos = 0;
    
    transmission[tpos++] = crypto_sign_BYTES;
    memcpy(&transmission[tpos], signature, crypto_sign_BYTES);
    tpos += crypto_sign_BYTES;
    
    // v7: no sessionId on wire (only in signature)
    memcpy(&transmission[tpos], trans_body, trans_body_len);
    tpos += trans_body_len;
    
    ESP_LOGI(TAG, "      Sending NEW command...");
    int ret = smp_write_command_block(ssl, block, transmission, tpos);
    if (ret != 0) {
        ESP_LOGE(TAG, "      âŒ Failed to send NEW");
        return -1;
    }
    
    // Wait for IDS response
    int content_len = smp_read_block(ssl, block, 15000);
    if (content_len < 0) {
        ESP_LOGE(TAG, "      âŒ No response to NEW");
        return -1;
    }
    
    uint8_t *resp = block + 2;
    
    for (int i = 0; i < content_len - 3; i++) {
        if (resp[i] == 'I' && resp[i+1] == 'D' && resp[i+2] == 'S' && resp[i+3] == ' ') {
            int p = i + 4;
            
            if (p < content_len) {
                c->recipient_id_len = resp[p++];
                if (c->recipient_id_len > 24) c->recipient_id_len = 24;
                if (p + c->recipient_id_len <= content_len) {
                    memcpy(c->recipient_id, &resp[p], c->recipient_id_len);
                    p += c->recipient_id_len;
                }
            }
            
            if (p < content_len) {
                c->sender_id_len = resp[p++];
                if (c->sender_id_len > 24) c->sender_id_len = 24;
                if (p + c->sender_id_len <= content_len) {
                    memcpy(c->sender_id, &resp[p], c->sender_id_len);
                    p += c->sender_id_len;
                }
            }
            
            if (p < content_len) {
                uint8_t srv_dh_len = resp[p++];
                if (srv_dh_len == 44 && p + srv_dh_len <= content_len) {
                    memcpy(c->srv_dh_public, &resp[p + 12], 32);
                    c->have_srv_dh = 1;
                }
            }
            
            c->active = 1;
            contacts_db.num_contacts++;
            save_contacts_to_nvs();
            
            ESP_LOGI(TAG, "      âœ… Contact '%s' created!", name);
            return slot;
        }
        
        if (resp[i] == 'E' && resp[i+1] == 'R' && resp[i+2] == 'R') {
            ESP_LOGE(TAG, "      âŒ Server error creating contact");
            return -1;
        }
    }
    
    ESP_LOGE(TAG, "      âŒ Unexpected response");
    return -1;
}

bool remove_contact(mbedtls_ssl_context *ssl, uint8_t *block,
                    const uint8_t *session_id, int index) {
    if (index < 0 || index >= MAX_CONTACTS || !contacts_db.contacts[index].active) {
        ESP_LOGE(TAG, "âŒ Invalid contact index: %d", index);
        return false;
    }
    
    contact_t *c = &contacts_db.contacts[index];
    ESP_LOGI(TAG, "ðŸ—‘ï¸  Removing contact '%s' [%d]...", c->name, index);
    
    uint8_t del_body[64];
    int dp = 0;
    
    del_body[dp++] = 1;
    del_body[dp++] = 'D';
    del_body[dp++] = c->recipient_id_len;
    memcpy(&del_body[dp], c->recipient_id, c->recipient_id_len);
    dp += c->recipient_id_len;
    del_body[dp++] = 'D';
    del_body[dp++] = 'E';
    del_body[dp++] = 'L';
    
    uint8_t del_to_sign[128];
    int dsp = 0;
    del_to_sign[dsp++] = 32;
    memcpy(&del_to_sign[dsp], session_id, 32);
    dsp += 32;
    memcpy(&del_to_sign[dsp], del_body, dp);
    dsp += dp;
    
    uint8_t del_sig[crypto_sign_BYTES];
    crypto_sign_detached(del_sig, NULL, del_to_sign, dsp, c->rcv_auth_secret);
    
    uint8_t del_trans[192];
    int dtp = 0;
    
    del_trans[dtp++] = crypto_sign_BYTES;
    memcpy(&del_trans[dtp], del_sig, crypto_sign_BYTES);
    dtp += crypto_sign_BYTES;
    
    // v7: no sessionId on wire (only in signature)
    memcpy(&del_trans[dtp], del_body, dp);
    dtp += dp;
    
    int ret = smp_write_command_block(ssl, block, del_trans, dtp);
    if (ret != 0) {
        ESP_LOGE(TAG, "      âŒ Failed to send DEL");
        return false;
    }
    
    int content_len = smp_read_block(ssl, block, 5000);
    if (content_len >= 0) {
        uint8_t *resp = block + 2;
        int rp = 0;
        if (resp[rp] == 1) {
            rp++;
            rp += 2;
            int rauthLen = resp[rp++]; rp += rauthLen;
            // v7: no sessLen in response
            int rcorrLen = resp[rp++]; rp += rcorrLen;
            int rentLen = resp[rp++]; rp += rentLen;
            
            if (rp + 1 < content_len && resp[rp] == 'O' && resp[rp+1] == 'K') {
                c->active = 0;
                memset(c, 0, sizeof(contact_t));
                contacts_db.num_contacts--;
                save_contacts_to_nvs();
                ESP_LOGI(TAG, "      âœ… Contact removed!");
                return true;
            }
        }
    }
    
    ESP_LOGE(TAG, "      âŒ Failed to remove contact");
    return false;
}

// ============== Subscribe All ==============

void subscribe_all_contacts(mbedtls_ssl_context *ssl, uint8_t *block,
                            const uint8_t *session_id) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "ðŸ“¡ Subscribing to all contacts...");
    
    int success_count = 0;
    
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        
        contact_t *c = &contacts_db.contacts[i];
        ESP_LOGI(TAG, "   [%d] %s...", i, c->name);
        
        uint8_t sub_body[128];
        int pos = 0;
        
        // T6-Fix4: corrId must be 24 random bytes per SMP protocol spec
        sub_body[pos++] = 24;         // corrId length
        uint8_t corr_id[24];
        esp_fill_random(corr_id, 24);
        memcpy(&sub_body[pos], corr_id, 24);
        pos += 24;
        sub_body[pos++] = c->recipient_id_len;
        memcpy(&sub_body[pos], c->recipient_id, c->recipient_id_len);
        pos += c->recipient_id_len;
        sub_body[pos++] = 'S';
        sub_body[pos++] = 'U';
        sub_body[pos++] = 'B';
        
        int sub_body_len = pos;
        
        uint8_t sub_to_sign[1 + 32 + 128];
        int sub_sign_pos = 0;
        sub_to_sign[sub_sign_pos++] = 32;
        memcpy(&sub_to_sign[sub_sign_pos], session_id, 32);
        sub_sign_pos += 32;
        memcpy(&sub_to_sign[sub_sign_pos], sub_body, sub_body_len);
        sub_sign_pos += sub_body_len;
        
        uint8_t sub_sig[crypto_sign_BYTES];
        crypto_sign_detached(sub_sig, NULL, sub_to_sign, sub_sign_pos, c->rcv_auth_secret);
        
        uint8_t sub_trans[256];
        int sub_tpos = 0;
        
        sub_trans[sub_tpos++] = crypto_sign_BYTES;
        memcpy(&sub_trans[sub_tpos], sub_sig, crypto_sign_BYTES);
        sub_tpos += crypto_sign_BYTES;
        
        // v7: no sessionId on wire (only in signature)
        memcpy(&sub_trans[sub_tpos], sub_body, sub_body_len);
        sub_tpos += sub_body_len;
        
        // T6-Diag3b: Hex dump of SUB transmission
        ESP_LOGW(TAG, "SUB transmission (%d bytes):", sub_tpos);
        for (int d = 0; d < sub_tpos && d < 128; d += 16) {
            char hex[64] = {0}; int hx = 0;
            char asc[20] = {0}; int ax = 0;
            for (int j = 0; j < 16 && (d+j) < sub_tpos; j++) {
                hx += sprintf(&hex[hx], "%02x ", sub_trans[d+j]);
                asc[ax++] = (sub_trans[d+j] >= 0x20 && sub_trans[d+j] < 0x7F) 
                             ? sub_trans[d+j] : '.';
            }
            asc[ax] = '\0';
            ESP_LOGW(TAG, "  +%04d: %-48s %s", d, hex, asc);
        }
        
        int ret = smp_write_command_block(ssl, block, sub_trans, sub_tpos);
        if (ret != 0) {
            ESP_LOGE(TAG, "       âŒ Send failed");
            continue;
        }
        
        // T6-Fix3: Drain responses until we find our SUB OK
        // After 42d handshake, ACK/END responses may arrive before SUB OK
        bool sub_ok = false;
        for (int attempt = 0; attempt < 5; attempt++) {
            int content_len = smp_read_block(ssl, block, 5000);
            if (content_len < 0) {
                ESP_LOGW(TAG, "       read attempt %d: timeout/error (%d)", attempt, content_len);
                break;
            }
            
            uint8_t *resp = block + 2;
            int rp = 0;
            
            // T6-Diag3: Hex dump first 64 bytes of response
            int dump_len = (content_len < 64) ? content_len : 64;
            ESP_LOGW(TAG, "       [attempt %d] resp %d bytes, first %d hex:", attempt, content_len, dump_len);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG, resp, dump_len, ESP_LOG_WARN);
            
            uint8_t tx_count = resp[rp];
            if (content_len > 10) {
                ESP_LOGI("SUB", "Contact SUB response txCount=%d", tx_count);
                rp++;
                rp += 2;
                int rauthLen = resp[rp++]; rp += rauthLen;
                // v7: no sessLen in response
                int rcorrLen = resp[rp++];
                // T6-Diag3: Log corrId content
                ESP_LOGW(TAG, "       [attempt %d] corrLen=%d, entLen after corr", attempt, rcorrLen);
                if (rcorrLen > 0 && rcorrLen <= 24) {
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &resp[rp], rcorrLen, ESP_LOG_WARN);
                }
                rp += rcorrLen;
                int rentLen = resp[rp++];
                
                // T6-Diag3: Log entity id
                ESP_LOGW(TAG, "       [attempt %d] entLen=%d, our_rcpLen=%d", attempt, rentLen, c->recipient_id_len);
                if (rentLen > 0 && rentLen <= 32) {
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &resp[rp], rentLen, ESP_LOG_WARN);
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, c->recipient_id, c->recipient_id_len, ESP_LOG_WARN);
                }
                
                // Check if this response is for our contact
                bool is_our_response = (rentLen == c->recipient_id_len &&
                    memcmp(&resp[rp], c->recipient_id, rentLen) == 0);
                rp += rentLen;
                
                // T6-Diag3: Log the command bytes
                int cmd_bytes = content_len - rp;
                ESP_LOGW(TAG, "       [attempt %d] ent_match=%d, cmd_offset=%d, cmd_bytes=%d", 
                         attempt, is_our_response, rp, cmd_bytes);
                if (cmd_bytes > 0) {
                    int cmd_dump = (cmd_bytes < 16) ? cmd_bytes : 16;
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, &resp[rp], cmd_dump, ESP_LOG_WARN);
                }
                
                if (is_our_response && rp + 1 < content_len && 
                    resp[rp] == 'O' && resp[rp+1] == 'K') {
                    ESP_LOGI(TAG, "       ✅ Subscribed! (attempt %d)", attempt);
                    success_count++;
                    sub_ok = true;

                    // TX2 Forwarding: If batch block contains a second transmission, forward it
                    if (tx_count > 1) {
                        uint16_t tx1_len = (resp[1] << 8) | resp[2];
                        int tx2_start = 1 + 2 + tx1_len;
                        ESP_LOGW("BATCH", "Contact txCount=%d, tx1_len=%d, tx2_start=%d, content_len=%d",
                                 tx_count, tx1_len, tx2_start, content_len);

                        if (tx2_start + 2 < content_len) {
                            uint16_t tx2_data_len = (resp[tx2_start] << 8) | resp[tx2_start + 1];
                            uint8_t *tx2_ptr = &resp[tx2_start + 2];
                            int tx2_avail = content_len - tx2_start - 2;
                            ESP_LOGW("BATCH", "TX2: data_len=%d, avail=%d", tx2_data_len, tx2_avail);

                            if (tx2_data_len > 0 && tx2_data_len <= tx2_avail) {
                                uint8_t *fwd = block;
                                int tx2_total = 1 + 2 + tx2_data_len;
                                fwd[0] = (tx2_total >> 8) & 0xFF;
                                fwd[1] = tx2_total & 0xFF;
                                fwd[2] = 0x01;
                                fwd[3] = (tx2_data_len >> 8) & 0xFF;
                                fwd[4] = tx2_data_len & 0xFF;
                                memmove(&fwd[5], tx2_ptr, tx2_data_len);

                                ESP_LOGW("BATCH", "Forwarding TX2 MSG (%d bytes) to App Task", tx2_data_len);
                                BaseType_t sent = xRingbufferSend(net_to_app_buf, fwd,
                                                                   tx2_total + 2,
                                                                   pdMS_TO_TICKS(1000));
                                if (sent != pdTRUE) {
                                    ESP_LOGE("BATCH", "Ring buffer full, TX2 MSG lost!");
                                }
                            }
                        } else {
                            ESP_LOGW("BATCH", "TX2 offset %d beyond content_len %d!", tx2_start, content_len);
                        }
                    }

                    break;
                } else {
                    ESP_LOGW("DRAIN", "Non-matching frame DISCARDED! (Contact SUB, attempt %d)", attempt);
                    ESP_LOGW("DRAIN", "Expected entity: %02x%02x%02x%02x",
                             c->recipient_id[0], c->recipient_id[1],
                             c->recipient_id[2], c->recipient_id[3]);
                    if (rentLen >= 4) {
                        ESP_LOGW("DRAIN", "Got entity: %02x%02x%02x%02x",
                                 resp[rp - rentLen], resp[rp - rentLen + 1],
                                 resp[rp - rentLen + 2], resp[rp - rentLen + 3]);
                    }
                    if (rp + 2 < content_len) {
                        ESP_LOGW("DRAIN", "Command bytes: %02x %02x %02x (%c%c%c)",
                                 resp[rp], resp[rp+1], resp[rp+2],
                                 resp[rp], resp[rp+1], resp[rp+2]);
                    }
                    int drain_dump = (content_len < 32) ? content_len : 32;
                    ESP_LOG_BUFFER_HEX_LEVEL("DRAIN", resp, drain_dump, ESP_LOG_WARN);
                }
            } else {
                ESP_LOGW("DRAIN", "Non-matching frame DISCARDED! (Contact SUB, unexpected format, attempt %d)", attempt);
                ESP_LOGW("DRAIN", "first_byte=0x%02x (expected 0x01)", resp[0]);
                int drain_dump = (content_len < 32) ? content_len : 32;
                ESP_LOG_BUFFER_HEX_LEVEL("DRAIN", resp, drain_dump, ESP_LOG_WARN);
            }
        }
        if (!sub_ok) {
            ESP_LOGE(TAG, "       ❌ Subscribe failed after retries");
        }
    }
    
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "📡 Contact subscriptions: %d/%d", success_count, contacts_db.num_contacts);
    
    // ========== Reply Queue SUB (Root Cause Fix!) ==========
    // After 42d handshake, the App sends messages to our Reply Queue
    // (our_queue.rcv_id), NOT to Q_A (contact->recipient_id).
    // We MUST subscribe to Reply Queue on the main socket so the
    // Network Task receives those MSG frames.
    if (our_queue.valid && our_queue.rcv_id_len > 0) {
        ESP_LOGI(TAG, "   [R] Reply Queue (rcvId: %02x%02x%02x%02x...)...",
                 our_queue.rcv_id[0], our_queue.rcv_id[1],
                 our_queue.rcv_id[2], our_queue.rcv_id[3]);

        // Build SUB body: corrId + entityId(rcvId) + "SUB"
        uint8_t rq_body[128];
        int rqp = 0;

        // corrId = 24 random bytes per SMP protocol spec
        rq_body[rqp++] = 24;
        uint8_t rq_corr_id[24];
        esp_fill_random(rq_corr_id, 24);
        memcpy(&rq_body[rqp], rq_corr_id, 24);
        rqp += 24;

        // EntityId = our Reply Queue rcvId
        rq_body[rqp++] = (uint8_t)our_queue.rcv_id_len;
        memcpy(&rq_body[rqp], our_queue.rcv_id, our_queue.rcv_id_len);
        rqp += our_queue.rcv_id_len;

        // Command: "SUB"
        rq_body[rqp++] = 'S';
        rq_body[rqp++] = 'U';
        rq_body[rqp++] = 'B';

        int rq_body_len = rqp;

        // Sign: [sessIdLen=32][sessionId] + body
        uint8_t rq_to_sign[1 + 32 + 128];
        int rqs = 0;
        rq_to_sign[rqs++] = 32;
        memcpy(&rq_to_sign[rqs], session_id, 32);
        rqs += 32;
        memcpy(&rq_to_sign[rqs], rq_body, rq_body_len);
        rqs += rq_body_len;

        uint8_t rq_sig[crypto_sign_BYTES];
        crypto_sign_detached(rq_sig, NULL, rq_to_sign, rqs,
                             our_queue.rcv_auth_private);  // Ed25519 64-byte PRIVATE key

        // Build transmission: [sigLen][sig][body] (v7: no sessionId on wire)
        uint8_t rq_trans[256];
        int rqt = 0;

        rq_trans[rqt++] = crypto_sign_BYTES;  // 64
        memcpy(&rq_trans[rqt], rq_sig, crypto_sign_BYTES);
        rqt += crypto_sign_BYTES;

        // v7: no sessionId on wire (only in signature)
        memcpy(&rq_trans[rqt], rq_body, rq_body_len);
        rqt += rq_body_len;

        // Send SUB
        int ret = smp_write_command_block(ssl, block, rq_trans, rqt);
        if (ret != 0) {
            ESP_LOGE(TAG, "       ❌ Reply Queue SUB send failed!");
        } else {
            // Drain responses until we find our SUB OK (same pattern as Contact SUBs)
            bool rq_sub_ok = false;
            for (int attempt = 0; attempt < 5; attempt++) {
                int rq_content_len = smp_read_block(ssl, block, 5000);
                if (rq_content_len < 0) {
                    ESP_LOGW(TAG, "       RQ read attempt %d: timeout/error (%d)", attempt, rq_content_len);
                    break;
                }

                uint8_t *rq_resp = block + 2;
                int rrp = 0;

                // v7 response parsing: txCount(1) + skip(2) + auth + corr + entity + command
                uint8_t rq_tx_count = rq_resp[rrp];
                if (rq_content_len > 10) {
                    ESP_LOGI("SUB", "Reply Queue response txCount=%d", rq_tx_count);
                    rrp++;
                    rrp += 2;
                    int rq_authLen = rq_resp[rrp++]; rrp += rq_authLen;
                    // v7: no sessLen in response
                    int rq_corrLen = rq_resp[rrp++]; rrp += rq_corrLen;
                    int rq_entLen  = rq_resp[rrp++];

                    // Check if this response is for our Reply Queue
                    bool is_our_rq = (rq_entLen == our_queue.rcv_id_len &&
                        memcmp(&rq_resp[rrp], our_queue.rcv_id, rq_entLen) == 0);
                    rrp += rq_entLen;

                    if (is_our_rq && rrp + 1 < rq_content_len &&
                        rq_resp[rrp] == 'O' && rq_resp[rrp+1] == 'K') {
                        ESP_LOGI(TAG, "       ✅ Reply Queue subscribed on main socket! (attempt %d)", attempt);
                        rq_sub_ok = true;

                        // TX2 Forwarding: If batch block contains a second transmission, forward it
                        if (rq_tx_count > 1) {
                            uint16_t tx1_len = (rq_resp[1] << 8) | rq_resp[2];
                            int tx2_start = 1 + 2 + tx1_len;
                            ESP_LOGW("BATCH", "RQ txCount=%d, tx1_len=%d, tx2_start=%d, content_len=%d",
                                     rq_tx_count, tx1_len, tx2_start, rq_content_len);

                            if (tx2_start + 2 < rq_content_len) {
                                uint16_t tx2_data_len = (rq_resp[tx2_start] << 8) | rq_resp[tx2_start + 1];
                                uint8_t *tx2_ptr = &rq_resp[tx2_start + 2];
                                int tx2_avail = rq_content_len - tx2_start - 2;
                                ESP_LOGW("BATCH", "TX2: data_len=%d, avail=%d", tx2_data_len, tx2_avail);

                                if (tx2_data_len > 0 && tx2_data_len <= tx2_avail) {
                                    // Repack TX2 as single-transmission block in-place
                                    uint8_t *fwd = block;
                                    int tx2_total = 1 + 2 + tx2_data_len;  // txCount + Large-Len + Data
                                    fwd[0] = (tx2_total >> 8) & 0xFF;
                                    fwd[1] = tx2_total & 0xFF;
                                    fwd[2] = 0x01;  // txCount = 1
                                    fwd[3] = (tx2_data_len >> 8) & 0xFF;
                                    fwd[4] = tx2_data_len & 0xFF;
                                    memmove(&fwd[5], tx2_ptr, tx2_data_len);

                                    ESP_LOGW("BATCH", "Forwarding TX2 MSG (%d bytes) to App Task", tx2_data_len);
                                    BaseType_t sent = xRingbufferSend(net_to_app_buf, fwd,
                                                                       tx2_total + 2,
                                                                       pdMS_TO_TICKS(1000));
                                    if (sent != pdTRUE) {
                                        ESP_LOGE("BATCH", "Ring buffer full, TX2 MSG lost!");
                                    }
                                }
                            } else {
                                ESP_LOGW("BATCH", "TX2 offset %d beyond content_len %d!", tx2_start, rq_content_len);
                            }
                        }

                        break;
                    } else {
                        ESP_LOGW("DRAIN", "Non-matching frame DISCARDED! (RQ SUB, attempt %d)", attempt);
                        ESP_LOGW("DRAIN", "Expected entity: %02x%02x%02x%02x",
                                 our_queue.rcv_id[0], our_queue.rcv_id[1],
                                 our_queue.rcv_id[2], our_queue.rcv_id[3]);
                        if (rq_entLen >= 4) {
                            ESP_LOGW("DRAIN", "Got entity: %02x%02x%02x%02x",
                                     rq_resp[rrp - rq_entLen], rq_resp[rrp - rq_entLen + 1],
                                     rq_resp[rrp - rq_entLen + 2], rq_resp[rrp - rq_entLen + 3]);
                        }
                        if (rrp + 2 < rq_content_len) {
                            ESP_LOGW("DRAIN", "Command bytes: %02x %02x %02x (%c%c%c)",
                                     rq_resp[rrp], rq_resp[rrp+1], rq_resp[rrp+2],
                                     rq_resp[rrp], rq_resp[rrp+1], rq_resp[rrp+2]);
                        }
                        int drain_dump = (rq_content_len < 32) ? rq_content_len : 32;
                        ESP_LOG_BUFFER_HEX_LEVEL("DRAIN", rq_resp, drain_dump, ESP_LOG_WARN);
                    }
                } else {
                    ESP_LOGW("DRAIN", "Non-matching frame DISCARDED! (RQ SUB, unexpected format, attempt %d)", attempt);
                    ESP_LOGW("DRAIN", "first_byte=0x%02x (expected 0x01)", rq_resp[0]);
                    int drain_dump = (rq_content_len < 32) ? rq_content_len : 32;
                    ESP_LOG_BUFFER_HEX_LEVEL("DRAIN", rq_resp, drain_dump, ESP_LOG_WARN);
                }
            }
            if (!rq_sub_ok) {
                ESP_LOGE(TAG, "       ❌ Reply Queue SUB failed after retries!");
            }
        }
    } else {
        ESP_LOGW(TAG, "   [R] Reply Queue: not valid (valid=%d, rcv_id_len=%d), skipping SUB",
                 our_queue.valid, our_queue.rcv_id_len);
    }

    ESP_LOGI(TAG, "");
}



// Generate invite link for first active contact (for QR code)
bool get_invite_link(const uint8_t *ca_hash, const char *host, int port, char *out_link, size_t out_len) {
    char hash_b64[64];
    char snd_b64[64];
    char dh_b64[80];
    char smp_uri[512];
    char smp_uri_encoded[1024];
    
    base64url_encode(ca_hash, 32, hash_b64, sizeof(hash_b64));
    
    // Find first active contact
    for (int i = 0; i < MAX_CONTACTS; i++) {
        if (!contacts_db.contacts[i].active) continue;
        contact_t *c = &contacts_db.contacts[i];
        
        base64url_encode(c->sender_id, c->sender_id_len, snd_b64, sizeof(snd_b64));
        
        // Encode dhPublicKey as SPKI + Base64URL with padding
        uint8_t dh_spki[44];
        memcpy(dh_spki, X25519_SPKI_HEADER, 12);
        memcpy(dh_spki + 12, c->rcv_dh_public, 32);
        
        {
            int in_len = 44;
            int j = 0;
            for (int k = 0; k < in_len; ) {
                uint32_t octet_a = k < in_len ? dh_spki[k++] : 0;
                uint32_t octet_b = k < in_len ? dh_spki[k++] : 0;
                uint32_t octet_c = k < in_len ? dh_spki[k++] : 0;
                uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
                dh_b64[j++] = base64url_chars[(triple >> 18) & 0x3F];
                dh_b64[j++] = base64url_chars[(triple >> 12) & 0x3F];
                dh_b64[j++] = base64url_chars[(triple >> 6) & 0x3F];
                dh_b64[j++] = base64url_chars[triple & 0x3F];
            }
            int mod = in_len % 3;
            if (mod == 1) {
                dh_b64[j-2] = '=';
                dh_b64[j-1] = '=';
            } else if (mod == 2) {
                dh_b64[j-1] = '=';
            }
            dh_b64[j] = '\0';
        }
        
        // Pre-encode = as %3D
        char dh_with_encoded_padding[100];
        {
            int j = 0;
            for (int k = 0; dh_b64[k] && j < 95; k++) {
                if (dh_b64[k] == '=') {
                    dh_with_encoded_padding[j++] = '%';
                    dh_with_encoded_padding[j++] = '3';
                    dh_with_encoded_padding[j++] = 'D';
                } else {
                    dh_with_encoded_padding[j++] = dh_b64[k];
                }
            }
            dh_with_encoded_padding[j] = '\0';
        }
        
        snprintf(smp_uri, sizeof(smp_uri),
                 "smp://%s@%s:%d/%s#/?v=1-4&dh=%s&q=c",
                 hash_b64, host, port, snd_b64, dh_with_encoded_padding);
        
        url_encode(smp_uri, smp_uri_encoded, sizeof(smp_uri_encoded));
        
        // Build final link
        snprintf(out_link, out_len, "simplex:/contact#/?v=2-7&smp=%s", smp_uri_encoded);
        
        ESP_LOGI(TAG, "Generated invite link (%d chars)", strlen(out_link));
        return true;
    }
    
    return false;
}