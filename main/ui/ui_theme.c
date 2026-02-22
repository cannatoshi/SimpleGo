/**
 * @file ui_theme.c
 * @brief Cyberpunk Theme - Touch Optimized
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
    lv_obj_set_pos(lbl, 4, 2);

    if (right_text) {
        lv_obj_t *r = lv_label_create(parent);
        lv_label_set_text(r, right_text);
        lv_obj_set_style_text_color(r, UI_COLOR_TEXT_DIM, 0);
        lv_obj_align(r, LV_ALIGN_TOP_RIGHT, -4, 2);
    }

    ui_create_line(parent, UI_HEADER_H);
    return lbl;
}

lv_obj_t *ui_create_back_btn(lv_obj_t *parent) {
    int y = UI_SCREEN_H - UI_NAV_H + 2;

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 56, UI_BTN_H);
    lv_obj_set_pos(btn, 2, y);

    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "< BACK");
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
    int y = UI_SCREEN_H - UI_NAV_H + 2;

    if (index > 0) {
        lv_obj_t *sep = lv_obj_create(parent);
        lv_obj_set_size(sep, 1, UI_BTN_H);
        lv_obj_set_pos(sep, x, y);
        lv_obj_set_style_bg_color(sep, UI_COLOR_LINE_DIM, 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);
    }

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, btn_w, UI_BTN_H);
    lv_obj_set_pos(btn, x + 1, y);

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

lv_obj_t *ui_create_tab_btn(lv_obj_t *parent, const char *text, int index, int total, bool active) {
    int tab_w = 60;
    int start_x = UI_SCREEN_W - (total * tab_w) - 4;
    int x = start_x + (index * tab_w);
    int y = UI_SCREEN_H - UI_NAV_H + 2;

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, tab_w - 2, UI_BTN_H);
    lv_obj_set_pos(btn, x, y);

    if (active) {
        lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_30, 0);
    } else {
        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    }
    lv_obj_set_style_bg_opa(btn, LV_OPA_40, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 0, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, active ? UI_COLOR_TEXT_WHITE : UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(lbl, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_center(lbl);

    return btn;
}

lv_obj_t *ui_create_switch(lv_obj_t *parent, const char *label, lv_coord_t x, lv_coord_t y, bool state) {
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 150, 28);
    lv_obj_set_pos(cont, x, y);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sw = lv_switch_create(cont);
    lv_obj_set_size(sw, 40, 20);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_set_style_bg_color(sw, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_bg_color(sw, UI_COLOR_PRIMARY, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, UI_COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, UI_COLOR_ACCENT, LV_PART_KNOB | LV_STATE_CHECKED);

    if (state) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    return sw;
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
    lv_obj_set_size(btn, w, UI_BTN_H);
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

/* ============================================================
 * Status Bar (v2) - GO button, time, WiFi bars, battery
 * ============================================================ */

/**
 * Helper: create a small rectangle (used for WiFi bars, battery parts)
 */
static lv_obj_t *create_indicator_rect(lv_obj_t *parent,
                                        lv_coord_t x, lv_coord_t y,
                                        lv_coord_t w, lv_coord_t h,
                                        lv_color_t color, lv_opa_t opa)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_size(r, w, h);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_style_bg_color(r, color, 0);
    lv_obj_set_style_bg_opa(r, opa, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

lv_obj_t *ui_create_status_bar(lv_obj_t *parent)
{
    /* Status bar background (16px) */
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, UI_SCREEN_W, UI_STATUS_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, UI_COLOR_STATUS_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    /* "GO" menu button (left side) */
    lv_obj_t *go_btn = lv_btn_create(bar);
    lv_obj_set_size(go_btn, 30, UI_STATUS_H);
    lv_obj_set_pos(go_btn, 0, 0);
    lv_obj_set_style_bg_opa(go_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(go_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(go_btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(go_btn, 0, 0);
    lv_obj_set_style_radius(go_btn, 0, 0);
    lv_obj_set_style_shadow_width(go_btn, 0, 0);
    lv_obj_set_style_pad_all(go_btn, 0, 0);

    lv_obj_t *go_lbl = lv_label_create(go_btn);
    lv_label_set_text(go_lbl, "GO");
    lv_obj_set_style_text_color(go_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(go_lbl, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_text_font(go_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(go_lbl);

    /*
     * Right side indicators: Time, WiFi bars, Battery
     *
     * Layout from right edge (5px padding):
     *   Battery tip:   x=312, w=2, h=3
     *   Battery body:  x=297, w=15, h=7
     *   WiFi bar 4:    x=289, w=2, h=7
     *   WiFi bar 3:    x=286, w=2, h=5
     *   WiFi bar 2:    x=283, w=2, h=3
     *   WiFi bar 1:    x=280, w=2, h=2
     *   Time label:    right-aligned ending ~x=275
     *
     * All indicators vertically centered, bars bottom-aligned to y=12
     */

    /* Time placeholder */
    lv_obj_t *time_lbl = lv_label_create(bar);
    lv_label_set_text(time_lbl, "12:00");
    lv_obj_set_style_text_color(time_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(time_lbl, 232, 1);

    /* WiFi bars (4 rectangles, bottom-aligned to y=12) */
    /* 3 of 4 bars active = good signal, bar 4 is dim */
    create_indicator_rect(bar, 280, 10, 2, 2, UI_COLOR_PRIMARY, LV_OPA_COVER);
    create_indicator_rect(bar, 283,  9, 2, 3, UI_COLOR_PRIMARY, LV_OPA_COVER);
    create_indicator_rect(bar, 286,  7, 2, 5, UI_COLOR_PRIMARY, LV_OPA_COVER);
    create_indicator_rect(bar, 289,  5, 2, 7, UI_COLOR_PRIMARY, LV_OPA_30);

    /* Battery body (outline) */
    lv_obj_t *bat_body = lv_obj_create(bar);
    lv_obj_set_size(bat_body, 15, 7);
    lv_obj_set_pos(bat_body, 297, 5);
    lv_obj_set_style_bg_opa(bat_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(bat_body, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_border_width(bat_body, 1, 0);
    lv_obj_set_style_radius(bat_body, 1, 0);
    lv_obj_set_style_pad_all(bat_body, 0, 0);
    lv_obj_clear_flag(bat_body, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Battery fill (70%) */
    create_indicator_rect(bat_body, 1, 1, 9, 3, UI_COLOR_SECONDARY, LV_OPA_COVER);

    /* Battery tip */
    create_indicator_rect(bar, 312, 7, 2, 3, UI_COLOR_TEXT_DIM, LV_OPA_COVER);

    /* Cyan glow line below status bar */
    lv_obj_t *glow = lv_obj_create(parent);
    lv_obj_set_size(glow, UI_SCREEN_W, 1);
    lv_obj_set_pos(glow, 0, UI_STATUS_H);
    lv_obj_set_style_bg_color(glow, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(glow, LV_OPA_30, 0);
    lv_obj_set_style_border_width(glow, 0, 0);
    lv_obj_clear_flag(glow, LV_OBJ_FLAG_CLICKABLE);

    ESP_LOGI(TAG, "Status bar created");
    return go_btn;
}
