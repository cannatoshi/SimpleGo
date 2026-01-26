/**
 * @file ui_manager.c
 * @brief Screen Manager - Manual Fade Control
 */
#include "ui_manager.h"
#include "ui_theme.h"
#include "ui_splash.h"
#include "ui_main.h"
#include "ui_chat.h"
#include "ui_contacts.h"
#include "ui_connect.h"
#include "ui_settings.h"
#include "ui_developer.h"
#include "esp_log.h"

static const char *TAG = "UI_MGR";

static lv_obj_t *screens[UI_SCREEN_COUNT] = {NULL};
static ui_screen_t current_screen = UI_SCREEN_SPLASH;
static ui_screen_t prev_screen = UI_SCREEN_MAIN;

typedef lv_obj_t *(*screen_create_fn)(void);

static const screen_create_fn screen_creators[UI_SCREEN_COUNT] = {
    [UI_SCREEN_SPLASH]    = ui_splash_create,
    [UI_SCREEN_MAIN]      = ui_main_create,
    [UI_SCREEN_CHAT]      = ui_chat_create,
    [UI_SCREEN_CONTACTS]  = ui_contacts_create,
    [UI_SCREEN_CONNECT]   = ui_connect_create,
    [UI_SCREEN_SETTINGS]  = ui_settings_create,
    [UI_SCREEN_DEVELOPER] = ui_developer_create,
};

esp_err_t ui_manager_init(void)
{
    ESP_LOGI(TAG, "Init...");
    ui_theme_init();
    
    screens[UI_SCREEN_SPLASH] = ui_splash_create();
    lv_scr_load(screens[UI_SCREEN_SPLASH]);
    
    current_screen = UI_SCREEN_SPLASH;
    return ESP_OK;
}

void ui_manager_show_screen(ui_screen_t screen, lv_scr_load_anim_t anim)
{
    if (screen >= UI_SCREEN_COUNT) return;
    if (screen == current_screen) return;
    
    ESP_LOGI(TAG, "-> screen %d", screen);
    
    if (!screens[screen]) {
        screens[screen] = screen_creators[screen]();
    }
    
    prev_screen = current_screen;
    current_screen = screen;
    
    // Immer direkt laden - Animationen machen die Screens selbst
    lv_scr_load(screens[screen]);
    
    // Alten Splash löschen
    if (prev_screen == UI_SCREEN_SPLASH && screens[prev_screen]) {
        lv_obj_del(screens[prev_screen]);
        screens[prev_screen] = NULL;
    }
}

void ui_manager_go_back(void)
{
    if (prev_screen != current_screen && prev_screen != UI_SCREEN_SPLASH) {
        ui_manager_show_screen(prev_screen, LV_SCR_LOAD_ANIM_NONE);
    } else {
        ui_manager_show_screen(UI_SCREEN_MAIN, LV_SCR_LOAD_ANIM_NONE);
    }
}

ui_screen_t ui_manager_get_current(void)
{
    return current_screen;
}
