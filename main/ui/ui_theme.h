/**
 * @file ui_theme.h
 * @brief SimpleGo Cyberpunk Theme
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_VERSION "v0.1.17-alpha"

#define UI_COLOR_BG           lv_color_hex(0x000000)
#define UI_COLOR_PRIMARY      lv_color_hex(0x00D4FF)
#define UI_COLOR_ACCENT       lv_color_hex(0xFF00FF)
#define UI_COLOR_SECONDARY    lv_color_hex(0x00FF66)
#define UI_COLOR_WARNING      lv_color_hex(0xFFCC00)
#define UI_COLOR_ERROR        lv_color_hex(0xFF3366)

#define UI_COLOR_TEXT         lv_color_hex(0x00D4FF)
#define UI_COLOR_TEXT_DIM     lv_color_hex(0x006680)
#define UI_COLOR_TEXT_WHITE   lv_color_hex(0xFFFFFF)

#define UI_COLOR_LINE         lv_color_hex(0x00D4FF)
#define UI_COLOR_LINE_DIM     lv_color_hex(0x003344)

#define UI_SCREEN_W           320
#define UI_SCREEN_H           240
#define UI_HEADER_H           18
#define UI_NAV_H              26
#define UI_CONTENT_H          (UI_SCREEN_H - UI_HEADER_H - UI_NAV_H)

#define UI_FONT               &lv_font_montserrat_14

void ui_theme_init(void);
void ui_theme_apply(lv_obj_t *obj);
lv_obj_t *ui_create_header(lv_obj_t *parent, const char *title, const char *right_text);
lv_obj_t *ui_create_back_btn(lv_obj_t *parent);
lv_obj_t *ui_create_nav_bar(lv_obj_t *parent);
lv_obj_t *ui_create_nav_btn(lv_obj_t *parent, const char *text, int index);
lv_obj_t *ui_create_line(lv_obj_t *parent, lv_coord_t y);
lv_obj_t *ui_create_btn(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t w);

#ifdef __cplusplus
}
#endif

#endif
