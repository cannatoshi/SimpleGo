/**
 * @file ui_chat.h
 * @brief Chat Screen - Message View
 */

#ifndef UI_CHAT_H
#define UI_CHAT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_chat_create(void);
void ui_chat_set_contact(const char *name);
void ui_chat_add_message(const char *text, bool is_outgoing);

#ifdef __cplusplus
}
#endif

#endif /* UI_CHAT_H */
