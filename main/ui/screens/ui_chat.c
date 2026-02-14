/**
 * @file ui_chat.c
 * @brief Chat Screen - Message View with Keyboard Input
 *
 * Layout (320x240):
 *   [Header 20px]     contact name (ui_create_header)
 *   [Messages 152px]  scrollable container with bubbles
 *   [Input 34px]      textarea — type and press Enter to send
 *   [Nav 32px]        back button (ui_create_nav_bar)
 *
 * SimpleGo UI — Auftrag 50d Task 5
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_chat.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "UI_CHAT";

// ============== Layout Constants ==============

#define INPUT_H         34
#define MSG_AREA_Y      (UI_HEADER_H + 2)
#define MSG_AREA_H      (UI_SCREEN_H - UI_HEADER_H - INPUT_H - UI_NAV_H - 2)
#define INPUT_Y         (UI_SCREEN_H - UI_NAV_H - INPUT_H)

// ============== State ==============

static lv_obj_t *screen = NULL;
static lv_obj_t *header_label = NULL;
static lv_obj_t *msg_container = NULL;
static lv_obj_t *input_area = NULL;
static lv_group_t *input_group = NULL;

static ui_chat_send_cb_t send_cb = NULL;
static lv_indev_t *pending_kb_indev = NULL;  // Stored until chat screen is created

// ============== Forward Declarations ==============

static void on_back(lv_event_t *e);
static void on_input_ready(lv_event_t *e);

// ============== Screen Creation ==============

lv_obj_t *ui_chat_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);

    // ===== Header (theme standard) =====
    header_label = ui_create_header(screen, "Chat", NULL);

    // ===== Message Container (scrollable) =====
    msg_container = lv_obj_create(screen);
    lv_obj_set_size(msg_container, UI_SCREEN_W, MSG_AREA_H);
    lv_obj_set_pos(msg_container, 0, MSG_AREA_Y);
    lv_obj_set_style_bg_color(msg_container, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(msg_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(msg_container, 0, 0);
    lv_obj_set_style_radius(msg_container, 0, 0);
    lv_obj_set_style_pad_all(msg_container, 4, 0);
    lv_obj_set_style_pad_row(msg_container, 4, 0);
    lv_obj_set_flex_flow(msg_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(msg_container, LV_OBJ_FLAG_SCROLLABLE);
    // Hide scrollbar for clean look
    lv_obj_set_scrollbar_mode(msg_container, LV_SCROLLBAR_MODE_OFF);

    // ===== Input Bar =====
    lv_obj_t *input_bar = lv_obj_create(screen);
    lv_obj_set_size(input_bar, UI_SCREEN_W, INPUT_H);
    lv_obj_set_pos(input_bar, 0, INPUT_Y);
    lv_obj_set_style_bg_color(input_bar, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(input_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(input_bar, 0, 0);
    lv_obj_set_style_radius(input_bar, 0, 0);
    lv_obj_set_style_pad_all(input_bar, 3, 0);
    lv_obj_clear_flag(input_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Separator line above input
    ui_create_line(screen, INPUT_Y);

    // Text area
    input_area = lv_textarea_create(input_bar);
    lv_obj_set_size(input_area, UI_SCREEN_W - 8, INPUT_H - 6);
    lv_obj_align(input_area, LV_ALIGN_CENTER, 0, 0);
    lv_textarea_set_one_line(input_area, true);
    lv_textarea_set_placeholder_text(input_area, "Type message...");
    lv_obj_set_style_bg_color(input_area, lv_color_hex(0x001A22), 0);
    lv_obj_set_style_text_color(input_area, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(input_area, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(input_area, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_border_width(input_area, 1, 0);
    lv_obj_set_style_radius(input_area, 2, 0);
    // Cursor color
    lv_obj_set_style_bg_color(input_area, UI_COLOR_ACCENT, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(input_area, LV_OPA_COVER, LV_PART_CURSOR);
    // Placeholder color
    lv_obj_set_style_text_color(input_area, UI_COLOR_TEXT_DIM, LV_PART_TEXTAREA_PLACEHOLDER);

    // Enter key → send
    lv_obj_add_event_cb(input_area, on_input_ready, LV_EVENT_READY, NULL);

    // ===== Nav Bar + Back Button (theme standard) =====
    ui_create_nav_bar(screen);
    lv_obj_t *back = ui_create_back_btn(screen);
    lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, NULL);

    // Create input group for keyboard focus
    input_group = lv_group_create();
    lv_group_add_obj(input_group, input_area);

    // Apply deferred keyboard indev if set before screen creation
    if (pending_kb_indev) {
        lv_indev_set_group(pending_kb_indev, input_group);
        ESP_LOGI(TAG, "Keyboard linked to chat input (deferred) ⌨️");
    }

    ESP_LOGI(TAG, "Chat screen created ⌨️");
    return screen;
}

// ============== Event Handlers ==============

static void on_back(lv_event_t *e)
{
    (void)e;
    ui_manager_go_back();
}

static void on_input_ready(lv_event_t *e)
{
    (void)e;
    const char *text = lv_textarea_get_text(input_area);
    if (!text || text[0] == '\0') return;

    ESP_LOGI(TAG, "📤 Send: \"%s\"", text);

    // Show as outgoing bubble immediately
    ui_chat_add_message(text, true);

    // Fire send callback
    if (send_cb) {
        send_cb(text);
    }

    // Clear input
    lv_textarea_set_text(input_area, "");
}

// ============== Public API ==============

void ui_chat_set_contact(const char *name)
{
    if (header_label && name) {
        lv_label_set_text(header_label, name);
    }
}

void ui_chat_add_message(const char *text, bool is_outgoing)
{
    if (!msg_container || !text) return;

    // Create bubble container
    lv_obj_t *bubble = lv_obj_create(msg_container);
    lv_obj_set_style_radius(bubble, 4, 0);
    lv_obj_set_style_pad_all(bubble, 5, 0);
    lv_obj_set_style_border_width(bubble, 1, 0);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_width(bubble, LV_PCT(78));
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    if (is_outgoing) {
        // Our messages: green tint, right-aligned
        lv_obj_set_style_bg_color(bubble, lv_color_hex(0x002211), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(bubble, UI_COLOR_SECONDARY, 0);
        lv_obj_set_style_align(bubble, LV_ALIGN_RIGHT_MID, 0);
    } else {
        // Their messages: cyan tint, left-aligned
        lv_obj_set_style_bg_color(bubble, lv_color_hex(0x001122), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(bubble, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_align(bubble, LV_ALIGN_LEFT_MID, 0);
    }

    // Message text
    lv_obj_t *label = lv_label_create(bubble);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

    // Auto-scroll to bottom
    lv_obj_scroll_to_y(msg_container, LV_COORD_MAX, LV_ANIM_ON);

    ESP_LOGD(TAG, "%s: \"%s\"", is_outgoing ? "OUT" : "IN", text);
}

void ui_chat_set_send_callback(ui_chat_send_cb_t cb)
{
    send_cb = cb;
}

void ui_chat_set_keyboard_indev(lv_indev_t *kb_indev)
{
    pending_kb_indev = kb_indev;  // Always store for deferred use

    // If chat screen already exists, link immediately
    if (kb_indev && input_group) {
        lv_indev_set_group(kb_indev, input_group);
        ESP_LOGI(TAG, "Keyboard linked to chat input ⌨️");
    }
}
