/**
 * @file ui_connect.c
 */
#include "ui_connect.h"
#include "ui_theme.h"
#include "ui_manager.h"

static lv_obj_t *screen = NULL;
static void on_back(lv_event_t *e) { ui_manager_go_back(); }

lv_obj_t *ui_connect_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);
    ui_create_header(screen, "Connect", NULL);
    
    lv_obj_t *qr = lv_obj_create(screen);
    lv_obj_set_size(qr, 100, 100);
    lv_obj_align(qr, LV_ALIGN_CENTER, 0, -15);
    lv_obj_set_style_bg_opa(qr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(qr, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(qr, 1, 0);
    lv_obj_set_style_radius(qr, 0, 0);
    lv_obj_clear_flag(qr, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *txt = lv_label_create(qr);
    lv_label_set_text(txt, "QR");
    lv_obj_set_style_text_color(txt, UI_COLOR_TEXT_DIM, 0);
    lv_obj_center(txt);
    
    ui_create_btn(screen, "GENERATE", 110, 165, 100);
    
    ui_create_nav_bar(screen);
    
    lv_obj_t *back = ui_create_back_btn(screen);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);
    
    return screen;
}
void ui_connect_set_invite_link(const char *l) {}
