/**
 * @file ui_connect.h
 * @brief Connect Screen - Add Contact / Show QR
 */

#ifndef UI_CONNECT_H
#define UI_CONNECT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *ui_connect_create(void);
void ui_connect_set_invite_link(const char *link);

#ifdef __cplusplus
}
#endif

#endif /* UI_CONNECT_H */
