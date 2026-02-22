/**
 * @file ui_chat.c
 * @brief Chat Screen - WhatsApp-Style Cyberpunk Messenger
 *
 * Layout (320x240):
 *   [Status Bar 16px]  GO menu | time | wifi | battery
 *   [Glow Line 1px]    cyan accent
 *   [Chat Header 20px] < back | contact name | encryption badge
 *   [Dim Line 1px]     separator
 *   [Messages 166px]   scrollable flex column with styled bubbles
 *   [Input Bar 36px]   textarea + file/voice/send buttons
 *
 * SimpleGo UI - v2 Redesign
 * Copyright (c) 2025-2026 Sascha
 * SPDX-License-Identifier: AGPL-3.0
 */

#include "ui_chat.h"
#include "ui_theme.h"
#include "ui_manager.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

static const char *TAG = "UI_CHAT";

/* ============== Layout Constants ============== */

#define CHAT_HDR_Y      (UI_STATUS_H + 1)                  /* 17 */
#define CHAT_HDR_H      20
#define MSG_AREA_Y      (CHAT_HDR_Y + CHAT_HDR_H + 1)      /* 38 */
#define INPUT_BAR_H     36
#define INPUT_BAR_Y     (UI_SCREEN_H - INPUT_BAR_H)         /* 204 */
#define MSG_AREA_H      (INPUT_BAR_Y - MSG_AREA_Y)          /* 166 */

/* Bubble styling */
#define BUBBLE_RADIUS       10
#define BUBBLE_PAD_H        6
#define BUBBLE_PAD_V        4
#define BUBBLE_WIDTH_PCT    72
#define BUBBLE_GAP          4

/* Colors */
#define BUBBLE_BG_OUT       lv_color_hex(0x001a0d)
#define BUBBLE_BG_IN        lv_color_hex(0x0a0f1a)

/* Action button size */
#define ACT_BTN_SIZE        24

/* ============== State ============== */

static lv_obj_t *screen        = NULL;
static lv_obj_t *header_label  = NULL;
static lv_obj_t *msg_container = NULL;
static lv_obj_t *input_area    = NULL;
static lv_group_t *input_group = NULL;

static ui_chat_send_cb_t send_cb    = NULL;
static lv_indev_t *pending_kb_indev = NULL;

/* Delivery status tracking */
#define MAX_TRACKED_MSGS 16

typedef struct {
    lv_obj_t *status_label;
    uint32_t  msg_seq;
    bool      active;
} tracked_msg_t;

static tracked_msg_t tracked_msgs[MAX_TRACKED_MSGS] = {0};
static uint32_t msg_seq_counter = 0;

/* ============== Forward Declarations ============== */

static void on_back(lv_event_t *e);
static void on_input_ready(lv_event_t *e);
static void on_send_click(lv_event_t *e);
static void do_send_message(void);

/* ============== Helpers ============== */

/**
 * Get current time as string for bubble timestamps.
 * If system time is not set (no NTP), shows placeholder.
 */
static void get_timestamp(char *buf, size_t len)
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year > 70) {
        strftime(buf, len, "%a %H:%M", &timeinfo);
    } else {
        snprintf(buf, len, "--:--");
    }
}

/**
 * Create a round action button for the input bar.
 */
static lv_obj_t *create_action_btn(lv_obj_t *parent, const char *icon,
                                    lv_color_t color)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, ACT_BTN_SIZE, ACT_BTN_SIZE);
    lv_obj_set_style_radius(btn, ACT_BTN_SIZE / 2, 0);
    lv_obj_set_style_bg_color(btn, UI_COLOR_INPUT_BG, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    /* Pressed state */
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, icon);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    return btn;
}

/* ============== Chat Header ============== */

/**
 * Custom chat header: back arrow + contact name + encryption badge.
 * Returns the contact name label for ui_chat_set_contact().
 */
static lv_obj_t *create_chat_header(lv_obj_t *parent)
{
    /* Header background */
    lv_obj_t *hdr = lv_obj_create(parent);
    lv_obj_set_size(hdr, UI_SCREEN_W, CHAT_HDR_H);
    lv_obj_set_pos(hdr, 0, CHAT_HDR_Y);
    lv_obj_set_style_bg_color(hdr, UI_COLOR_HDR_BG, 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    /* Back arrow button "<" */
    lv_obj_t *back_btn = lv_btn_create(hdr);
    lv_obj_set_size(back_btn, 24, CHAT_HDR_H);
    lv_obj_set_pos(back_btn, 0, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(back_btn, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_radius(back_btn, 0, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);
    lv_obj_set_style_pad_all(back_btn, 0, 0);

    lv_obj_t *arrow = lv_label_create(back_btn);
    lv_label_set_text(arrow, "<");
    lv_obj_set_style_text_color(arrow, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_color(arrow, UI_COLOR_ACCENT, LV_STATE_PRESSED);
    lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
    lv_obj_center(arrow);

    lv_obj_add_event_cb(back_btn, on_back, LV_EVENT_CLICKED, NULL);

    /* Contact name */
    lv_obj_t *name = lv_label_create(hdr);
    lv_label_set_text(name, "Chat");
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name, 160);
    lv_obj_set_style_text_color(name, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_14, 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 26, 0);

    /* Encryption badge */
    lv_obj_t *enc = lv_label_create(hdr);
    lv_label_set_text(enc, "Post-Quantum E2E");
    lv_obj_set_style_text_color(enc, UI_COLOR_ENCRYPT, 0);
    lv_obj_set_style_text_font(enc, &lv_font_montserrat_14, 0);
    lv_obj_align(enc, LV_ALIGN_RIGHT_MID, -4, 0);

    /* Dim separator line below header */
    ui_create_line(parent, CHAT_HDR_Y + CHAT_HDR_H);

    return name;
}

/* ============== Screen Creation ============== */

lv_obj_t *ui_chat_create(void)
{
    screen = lv_obj_create(NULL);
    ui_theme_apply(screen);

    /* ---- Status Bar (GO + time + wifi + battery) ---- */
    lv_obj_t *go_btn = ui_create_status_bar(screen);
    lv_obj_add_event_cb(go_btn, on_back, LV_EVENT_CLICKED, NULL);

    /* ---- Chat Header (back + name + encryption) ---- */
    header_label = create_chat_header(screen);

    /* ---- Message Container (scrollable flex column) ---- */
    msg_container = lv_obj_create(screen);
    lv_obj_set_size(msg_container, UI_SCREEN_W, MSG_AREA_H);
    lv_obj_set_pos(msg_container, 0, MSG_AREA_Y);
    lv_obj_set_style_bg_color(msg_container, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(msg_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(msg_container, 0, 0);
    lv_obj_set_style_radius(msg_container, 0, 0);
    lv_obj_set_style_pad_left(msg_container, 6, 0);
    lv_obj_set_style_pad_right(msg_container, 6, 0);
    lv_obj_set_style_pad_top(msg_container, 6, 0);
    lv_obj_set_style_pad_bottom(msg_container, 4, 0);
    lv_obj_set_style_pad_row(msg_container, BUBBLE_GAP, 0);
    lv_obj_set_flex_flow(msg_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(msg_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(msg_container, LV_SCROLLBAR_MODE_OFF);

    /* ---- Input Bar ---- */

    /* Separator line above input */
    lv_obj_t *sep = lv_obj_create(screen);
    lv_obj_set_size(sep, UI_SCREEN_W, 1);
    lv_obj_set_pos(sep, 0, INPUT_BAR_Y);
    lv_obj_set_style_bg_color(sep, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_20, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_CLICKABLE);

    /* Input bar container (flex row) */
    lv_obj_t *input_bar = lv_obj_create(screen);
    lv_obj_set_size(input_bar, UI_SCREEN_W, INPUT_BAR_H);
    lv_obj_set_pos(input_bar, 0, INPUT_BAR_Y);
    lv_obj_set_style_bg_color(input_bar, UI_COLOR_STATUS_BG, 0);
    lv_obj_set_style_bg_opa(input_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(input_bar, 0, 0);
    lv_obj_set_style_radius(input_bar, 0, 0);
    lv_obj_set_style_pad_left(input_bar, 3, 0);
    lv_obj_set_style_pad_right(input_bar, 3, 0);
    lv_obj_set_style_pad_top(input_bar, 5, 0);
    lv_obj_set_style_pad_bottom(input_bar, 5, 0);
    lv_obj_set_style_pad_column(input_bar, 2, 0);
    lv_obj_set_flex_flow(input_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(input_bar, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(input_bar, LV_OBJ_FLAG_SCROLLABLE);

    /* Textarea (flex grow takes remaining space) */
    input_area = lv_textarea_create(input_bar);
    lv_obj_set_flex_grow(input_area, 1);
    lv_obj_set_height(input_area, ACT_BTN_SIZE);
    lv_textarea_set_one_line(input_area, true);
    lv_textarea_set_placeholder_text(input_area, "> message...");

    lv_obj_set_style_bg_color(input_area, UI_COLOR_INPUT_BG, 0);
    lv_obj_set_style_bg_opa(input_area, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(input_area, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(input_area, &lv_font_montserrat_14, 0);
    lv_obj_set_style_border_color(input_area, UI_COLOR_LINE_DIM, 0);
    lv_obj_set_style_border_width(input_area, 1, 0);
    lv_obj_set_style_radius(input_area, 12, 0);
    lv_obj_set_style_pad_left(input_area, 8, 0);
    lv_obj_set_style_pad_right(input_area, 8, 0);

    /* Focus glow */
    lv_obj_set_style_border_color(input_area, UI_COLOR_PRIMARY, LV_STATE_FOCUSED);

    /* Magenta cursor */
    lv_obj_set_style_bg_color(input_area, UI_COLOR_ACCENT, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(input_area, LV_OPA_COVER, LV_PART_CURSOR);

    /* Placeholder dim */
    lv_obj_set_style_text_color(input_area, UI_COLOR_TEXT_DIM,
                                LV_PART_TEXTAREA_PLACEHOLDER);

    /* Enter key sends message */
    lv_obj_add_event_cb(input_area, on_input_ready, LV_EVENT_READY, NULL);

    /* Action buttons: File, Voice, Send */
    create_action_btn(input_bar, "+", UI_COLOR_TEXT_DIM);
    create_action_btn(input_bar, "~", UI_COLOR_TEXT_DIM);
    lv_obj_t *send_btn = create_action_btn(input_bar, ">",
                                            UI_COLOR_SECONDARY);
    lv_obj_add_event_cb(send_btn, on_send_click, LV_EVENT_CLICKED, NULL);

    /* ---- Keyboard input group ---- */
    input_group = lv_group_create();
    lv_group_add_obj(input_group, input_area);

    if (pending_kb_indev) {
        lv_indev_set_group(pending_kb_indev, input_group);
        ESP_LOGI(TAG, "Keyboard linked (deferred)");
    }

    ESP_LOGI(TAG, "Chat screen created");
    return screen;
}

/* ============== Event Handlers ============== */

static void on_back(lv_event_t *e)
{
    (void)e;
    ui_manager_go_back();
}

static void do_send_message(void)
{
    const char *text = lv_textarea_get_text(input_area);
    if (!text || text[0] == '\0') return;

    ESP_LOGI(TAG, "Send: \"%s\"", text);

    /* Assign sequence BEFORE creating bubble */
    uint32_t seq = ui_chat_next_seq();
    (void)seq;

    /* Show outgoing bubble with "..." status */
    ui_chat_add_message(text, true);

    /* Fire send callback (protocol layer updates status) */
    if (send_cb) {
        send_cb(text);
    }

    lv_textarea_set_text(input_area, "");
}

static void on_input_ready(lv_event_t *e)
{
    (void)e;
    do_send_message();
}

static void on_send_click(lv_event_t *e)
{
    (void)e;
    do_send_message();
}

/* ============== Public API ============== */

void ui_chat_set_contact(const char *name)
{
    if (header_label && name) {
        lv_label_set_text(header_label, name);
    }
}

void ui_chat_add_message(const char *text, bool is_outgoing)
{
    if (!msg_container || !text) return;

    /* Get timestamp */
    char time_buf[16];
    get_timestamp(time_buf, sizeof(time_buf));

    /* ---- Bubble container (flex column: text + meta) ---- */
    lv_obj_t *bubble = lv_obj_create(msg_container);
    lv_obj_set_width(bubble, LV_PCT(BUBBLE_WIDTH_PCT));
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(bubble, BUBBLE_RADIUS, 0);
    lv_obj_set_style_pad_left(bubble, BUBBLE_PAD_H, 0);
    lv_obj_set_style_pad_right(bubble, BUBBLE_PAD_H, 0);
    lv_obj_set_style_pad_top(bubble, BUBBLE_PAD_V, 0);
    lv_obj_set_style_pad_bottom(bubble, 2, 0);
    lv_obj_set_style_border_width(bubble, 1, 0);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    /* Flex column for text + meta stacking */
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(bubble, 1, 0);

    if (is_outgoing) {
        /* Outgoing: dark green tint, soft green border */
        lv_obj_set_style_bg_color(bubble, BUBBLE_BG_OUT, 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(bubble, UI_COLOR_SECONDARY, 0);
        lv_obj_set_style_border_opa(bubble, LV_OPA_30, 0);
        lv_obj_set_style_align(bubble, LV_ALIGN_RIGHT_MID, 0);
    } else {
        /* Incoming: dark blue tint, soft cyan border */
        lv_obj_set_style_bg_color(bubble, BUBBLE_BG_IN, 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(bubble, UI_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_opa(bubble, LV_OPA_30, 0);
        lv_obj_set_style_align(bubble, LV_ALIGN_LEFT_MID, 0);
    }

    /* ---- Message text ---- */
    lv_obj_t *label = lv_label_create(bubble);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);

    /* ---- Meta row (timestamp + delivery status) ---- */
    lv_obj_t *meta = lv_obj_create(bubble);
    lv_obj_set_width(meta, LV_PCT(100));
    lv_obj_set_height(meta, 16);
    lv_obj_set_style_bg_opa(meta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(meta, 0, 0);
    lv_obj_set_style_pad_all(meta, 0, 0);
    lv_obj_clear_flag(meta, LV_OBJ_FLAG_SCROLLABLE);

    /* Timestamp (left side of meta) */
    lv_obj_t *time_lbl = lv_label_create(meta);
    lv_label_set_text(time_lbl, time_buf);
    lv_obj_set_style_text_color(time_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(time_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(time_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    /* Delivery status (outgoing only, right side of meta) */
    if (is_outgoing) {
        lv_obj_t *st = lv_label_create(meta);
        lv_label_set_text(st, "...");
        lv_obj_set_style_text_color(st, UI_COLOR_TEXT_DIM, 0);
        lv_obj_set_style_text_font(st, &lv_font_montserrat_14, 0);
        lv_obj_align(st, LV_ALIGN_RIGHT_MID, 0, 0);

        /* Track for async status updates */
        uint32_t seq = msg_seq_counter;
        for (int i = 0; i < MAX_TRACKED_MSGS; i++) {
            if (!tracked_msgs[i].active) {
                tracked_msgs[i].status_label = st;
                tracked_msgs[i].msg_seq = seq;
                tracked_msgs[i].active = true;
                break;
            }
        }
    }

    /* Session 32 Fix: Force layout recalc + invalidate + scroll
     * Required when called from timer callback (ui_poll_timer_cb) */
    lv_obj_update_layout(msg_container);
    lv_obj_scroll_to_y(msg_container, LV_COORD_MAX, LV_ANIM_ON);
    lv_obj_invalidate(lv_scr_act());

    /* Session 33 Fix: Force immediate render when called from timer context.
     * Without this, incoming messages only appear after next user interaction
     * because LVGL's internal refresh timer already ran before our poll timer. */
    lv_refr_now(NULL);

    ESP_LOGD(TAG, "%s: \"%s\"", is_outgoing ? "OUT" : "IN", text);
}

void ui_chat_set_send_callback(ui_chat_send_cb_t cb)
{
    send_cb = cb;
}

void ui_chat_set_keyboard_indev(lv_indev_t *kb_indev)
{
    pending_kb_indev = kb_indev;

    if (kb_indev && input_group) {
        lv_indev_set_group(kb_indev, input_group);
        ESP_LOGI(TAG, "Keyboard linked");
    }
}

/* ============== Delivery Status API ============== */

void ui_chat_update_status(uint32_t msg_seq, int status)
{
    for (int i = 0; i < MAX_TRACKED_MSGS; i++) {
        if (tracked_msgs[i].active && tracked_msgs[i].msg_seq == msg_seq) {
            if (status >= 0 && status <= 3) {
                static const char *st_text[] = { "...", "v", "vv", "x" };
                lv_label_set_text(tracked_msgs[i].status_label, st_text[status]);

                lv_color_t color;
                switch (status) {
                    case 2:  color = UI_COLOR_SECONDARY; break;
                    case 3:  color = UI_COLOR_ERROR;     break;
                    default: color = UI_COLOR_TEXT_DIM;   break;
                }
                lv_obj_set_style_text_color(
                    tracked_msgs[i].status_label, color, 0);
            }

            if (status == 2 || status == 3) {
                tracked_msgs[i].active = false;
            }
            return;
        }
    }
}

uint32_t ui_chat_next_seq(void)
{
    return ++msg_seq_counter;
}

uint32_t ui_chat_get_last_seq(void)
{
    return msg_seq_counter;
}
