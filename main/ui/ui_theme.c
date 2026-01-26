/**
 * @file ui_theme.c
 * @brief Cyberpunk Theme
 */

#include "ui_theme.h"
#include "esp_log.h"

static const char *TAG = "THEME";

void ui_theme_init(void) {
    ESP_LOGI(TAG, "Cyberpunk theme");
}

void ui_theme_apply(lv_obj_t *obj) {
    lv_obj_set_style_bg_color(obj, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *ui_create_header(lv_obj_t *parent, const char *title, const char *right_text) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(lbl, 0, 2);
    
    if (right_text) {
        lv_obj_t *r = lv_label_create(parent);
        lv_label_set_text(r, right_text);
        lv_obj_set_style_text_color(r, UI_COLOR_TEXT_DIM, 0);
        lv_obj_align(r, LV_ALIGN_TOP_RIGHT, -2, 2);
    }
    
    ui_create_line(parent, UI_HEADER_H);
    return lbl;
}

lv_obj_t *ui_create_back_btn(lv_obj_t *parent) {
    // Position: unten links, mit Abstand zum Rand und zur Nav-Linie
    int x = 4;  // Abstand links
    int y = UI_SCREEN_H - UI_NAV_H + 4;  // Abstand nach Linie
    
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 50, 18);
    lv_obj_set_pos(btn, x, y);
    
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "BACK");
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_center(lbl);
    
    return btn;
}

lv_obj_t *ui_create_nav_bar(lv_obj_t *parent) {
    ui_create_line(parent, UI_SCREEN_H - UI_NAV_H);
    return NULL;
}

lv_obj_t *ui_create_nav_btn(lv_obj_t *parent, const char *text, int index) {
    int btn_w = 79;
    int x = index * 80;
    int y = UI_SCREEN_H - UI_NAV_H + 4;
    
    if (index > 0) {
        lv_obj_t *sep = lv_obj_create(parent);
        lv_obj_set_size(sep, 1, 18);
        lv_obj_set_pos(sep, x, y);
        lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);
    }
    
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, btn_w, 20);
    lv_obj_set_pos(btn, x + 1, y - 1);
    
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_center(lbl);
    
    return btn;
}

lv_obj_t *ui_create_line(lv_obj_t *parent, lv_coord_t y) {
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, UI_SCREEN_W, 1);
    lv_obj_set_pos(line, 0, y);
    lv_obj_set_style_bg_color(line, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
}

lv_obj_t *ui_create_btn(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t w) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, 22);
    lv_obj_set_pos(btn, x, y);
    
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_center(lbl);
    
    return btn;
}
