/**
 * @file ui_contacts.c
 * @brief Contacts Screen - Shows real contacts from contacts_db
 *
 * Session 32: Replaced "NO CONTACTS" stub with live data
 *
 * SimpleGo UI
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */
#include "ui_contacts.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "ui_chat.h"
#include "smp_types.h"
#include "smp_tasks.h"    // Session 33: smp_set_active_contact()
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_CONTACTS";

static lv_obj_t *screen = NULL;
static lv_obj_t *list_container = NULL;
static lv_obj_t *empty_label = NULL;

static void on_back(lv_event_t *e)
{
    (void)e;
    ui_manager_go_back();
}

static void on_contact_click(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx >= 0 && idx < MAX_CONTACTS && contacts_db.contacts[idx].active) {
        contact_t *c = &contacts_db.contacts[idx];
        ESP_LOGI(TAG, "Contact tapped: [%d] %s", idx, c->name);

        // Session 33: Tell protocol layer which contact to send to
        smp_set_active_contact(idx);

        // Session 33: Set chat header
        ui_chat_set_contact(c->name);

        ui_manager_show_screen(UI_SCREEN_CHAT, LV_SCR_LOAD_ANIM_NONE);
    }
}

lv_obj_t *ui_contacts_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);
    ui_create_header(screen, "Contacts", NULL);

    // Scrollable list area
    list_container = lv_obj_create(screen);
    lv_obj_set_size(list_container, UI_SCREEN_W, UI_SCREEN_H - UI_HEADER_H - UI_NAV_H - 2);
    lv_obj_set_pos(list_container, 0, UI_HEADER_H + 2);
    lv_obj_set_style_bg_color(list_container, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(list_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list_container, 0, 0);
    lv_obj_set_style_radius(list_container, 0, 0);
    lv_obj_set_style_pad_all(list_container, 4, 0);
    lv_obj_set_style_pad_row(list_container, 2, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(list_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_OFF);

    // Empty state label (hidden when contacts exist)
    empty_label = lv_label_create(list_container);
    lv_label_set_text(empty_label, "No contacts yet");
    lv_obj_set_style_text_color(empty_label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(empty_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_top(empty_label, 20, 0);
    lv_obj_set_style_align(empty_label, LV_ALIGN_TOP_MID, 0);

    // Nav Bar
    ui_create_nav_bar(screen);
    lv_obj_t *back = ui_create_back_btn(screen);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);

    // Populate with real contacts
    ui_contacts_refresh();

    ESP_LOGI(TAG, "Contacts screen created");
    return screen;
}

void ui_contacts_refresh(void)
{
    if (!list_container) return;

    // Remove all children except empty_label
    uint32_t child_count = lv_obj_get_child_count(list_container);
    for (int i = child_count - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(list_container, i);
        if (child != empty_label) {
            lv_obj_delete(child);
        }
    }

    // Count active contacts
    int active_count = 0;
    for (int i = 0; i < contacts_db.num_contacts && i < MAX_CONTACTS; i++) {
        if (contacts_db.contacts[i].active) {
            active_count++;
        }
    }

    if (active_count == 0) {
        lv_obj_clear_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);

    // Create a row for each active contact
    for (int i = 0; i < contacts_db.num_contacts && i < MAX_CONTACTS; i++) {
        contact_t *c = &contacts_db.contacts[i];
        if (!c->active) continue;

        lv_obj_t *row = lv_obj_create(list_container);
        lv_obj_set_size(row, LV_PCT(100), 36);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x001A22), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, UI_COLOR_LINE_DIM, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_radius(row, 2, 0);
        lv_obj_set_style_pad_left(row, 8, 0);
        lv_obj_set_style_pad_right(row, 8, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        // Contact name
        lv_obj_t *name_label = lv_label_create(row);
        lv_label_set_text(name_label, c->name);
        lv_obj_set_style_text_color(name_label, UI_COLOR_SECONDARY, 0);
        lv_obj_set_style_text_font(name_label, &lv_font_montserrat_14, 0);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 0, 0);

        // Status indicator (connected = has server DH)
        lv_obj_t *status = lv_label_create(row);
        if (c->have_srv_dh) {
            lv_label_set_text(status, "online");
            lv_obj_set_style_text_color(status, UI_COLOR_SECONDARY, 0);
        } else {
            lv_label_set_text(status, "pending");
            lv_obj_set_style_text_color(status, UI_COLOR_TEXT_DIM, 0);
        }
        lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
        lv_obj_align(status, LV_ALIGN_RIGHT_MID, 0, 0);

        // Session 33: Tap to open chat (pass contact INDEX as user data)
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(row, on_contact_click, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        ESP_LOGI(TAG, "  [%d] %s (srv_dh=%d)", i, c->name, c->have_srv_dh);
    }

    ESP_LOGI(TAG, "Contacts refreshed: %d active", active_count);
}
