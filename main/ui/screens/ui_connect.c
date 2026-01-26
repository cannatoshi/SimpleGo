/**
 * @file ui_connect.c
 * @brief Connect Screen with QR Code - Full Screen
 */
#include "ui_connect.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "lvgl.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_CONNECT";

static lv_obj_t *screen = NULL;
static lv_obj_t *qr_code = NULL;
static lv_obj_t *placeholder = NULL;
static lv_obj_t *status_lbl = NULL;

static void on_back(lv_event_t *e) { ui_manager_go_back(); }

lv_obj_t *ui_connect_create(void)
{
    ESP_LOGI(TAG, "Creating connect screen...");

    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);
    ui_create_header(screen, "Connect", NULL);

    // QR Code - gross und zentriert (180x180)
    qr_code = lv_qrcode_create(screen);
    lv_qrcode_set_size(qr_code, 160);
    lv_qrcode_set_dark_color(qr_code, UI_COLOR_PRIMARY);
    lv_qrcode_set_light_color(qr_code, UI_COLOR_BG);
    lv_obj_align(qr_code, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_flag(qr_code, LV_OBJ_FLAG_HIDDEN);

    // Placeholder
    placeholder = lv_label_create(screen);
    lv_label_set_text(placeholder, "Generating...");
    lv_obj_set_style_text_color(placeholder, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(placeholder, LV_ALIGN_CENTER, 0, -10);

    // Status Label unter QR
    status_lbl = lv_label_create(screen);
    lv_label_set_text(status_lbl, "");
    lv_obj_set_style_text_color(status_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(status_lbl, LV_ALIGN_CENTER, 0, 85);

    // Nav Bar
    ui_create_nav_bar(screen);
    lv_obj_t *back = ui_create_nav_btn(screen, "BACK", 0);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);

    return screen;
}

void ui_connect_set_invite_link(const char *link) {
    if (!link || !qr_code) return;

    ESP_LOGI(TAG, "Setting QR code for link (%d chars)", strlen(link));

    lv_result_t res = lv_qrcode_update(qr_code, link, strlen(link));
    
    if (res == LV_RESULT_OK) {
        ESP_LOGI(TAG, "QR code updated successfully");
        lv_obj_clear_flag(qr_code, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(placeholder, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(status_lbl, "Scan with SimpleX");
    } else {
        ESP_LOGE(TAG, "QR code update failed: %d", res);
        lv_label_set_text(placeholder, "QR Error!");
        lv_label_set_text(status_lbl, "Link too long?");
    }
}
