/**
 * @file ui_contacts.c
 */
#include "ui_contacts.h"
#include "ui_theme.h"
#include "ui_manager.h"

static lv_obj_t *screen = NULL;
static void on_back(lv_event_t *e) { ui_manager_go_back(); }

lv_obj_t *ui_contacts_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);
    ui_create_header(screen, "Contacts", NULL);
    
    lv_obj_t *msg = lv_label_create(screen);
    lv_label_set_text(msg, "NO CONTACTS");
    lv_obj_set_style_text_color(msg, UI_COLOR_TEXT_DIM, 0);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, 0);
    
    ui_create_nav_bar(screen);
    
    lv_obj_t *back = ui_create_back_btn(screen);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);
    
    return screen;
}
void ui_contacts_refresh(void) {}
