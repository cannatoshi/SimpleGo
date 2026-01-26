/**
 * @file ui_developer.c
 */
#include "ui_developer.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_wifi.h"

static lv_obj_t *screen = NULL;
static lv_obj_t *heap_lbl, *up_lbl, *wifi_lbl, *log_ta;
static lv_timer_t *tmr;

static void on_back(lv_event_t *e) { ui_manager_go_back(); }

static void update(lv_timer_t *t) {
    char b[48];
    snprintf(b, 48, "HEAP %luK", (unsigned long)(esp_get_free_heap_size()/1024));
    lv_label_set_text(heap_lbl, b);
    
    int s = (int)(esp_timer_get_time()/1000000);
    snprintf(b, 48, "%02d:%02d:%02d", s/3600, (s%3600)/60, s%60);
    lv_label_set_text(up_lbl, b);
    
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK)
        snprintf(b, 48, "%ddBm", ap.rssi);
    else
        snprintf(b, 48, "--");
    lv_label_set_text(wifi_lbl, b);
}

lv_obj_t *ui_developer_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);
    ui_create_header(screen, "Developer", NULL);
    
    heap_lbl = lv_label_create(screen);
    lv_obj_set_style_text_color(heap_lbl, UI_COLOR_SECONDARY, 0);
    lv_obj_set_pos(heap_lbl, 5, 24);
    
    up_lbl = lv_label_create(screen);
    lv_obj_set_style_text_color(up_lbl, UI_COLOR_PRIMARY, 0);
    lv_obj_set_pos(up_lbl, 120, 24);
    
    wifi_lbl = lv_label_create(screen);
    lv_obj_set_style_text_color(wifi_lbl, UI_COLOR_WARNING, 0);
    lv_obj_set_pos(wifi_lbl, 220, 24);
    
    ui_create_line(screen, 42);
    
    log_ta = lv_textarea_create(screen);
    lv_obj_set_size(log_ta, 316, 120);
    lv_obj_set_pos(log_ta, 2, 44);
    lv_textarea_set_text(log_ta, "> READY\n");
    lv_obj_set_style_bg_color(log_ta, UI_COLOR_BG, 0);
    lv_obj_set_style_border_color(log_ta, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_border_width(log_ta, 1, 0);
    lv_obj_set_style_text_color(log_ta, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_radius(log_ta, 0, 0);
    
    ui_create_nav_bar(screen);
    
    lv_obj_t *back = ui_create_back_btn(screen);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);
    
    tmr = lv_timer_create(update, 1000, NULL);
    update(NULL);
    
    return screen;
}

void ui_developer_log(const char *m) {
    if (log_ta) {
        lv_textarea_add_text(log_ta, "> ");
        lv_textarea_add_text(log_ta, m);
        lv_textarea_add_text(log_ta, "\n");
    }
}
void ui_developer_update_stats(void) { update(NULL); }
