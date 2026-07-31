/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <stdio.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/hid.h>
#include <zmk/usb.h>
#include <zmk/wpm.h>

#include <dt-bindings/zmk/hid_usage.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/modifiers.h>

#include "status.h"

LV_IMG_DECLARE(bolt);
LV_IMG_DECLARE(bt);
LV_IMG_DECLARE(bt_no_signal);
LV_IMG_DECLARE(bt_unbonded);
LV_IMG_DECLARE(usb);
LV_IMG_DECLARE(profile);
LV_IMG_DECLARE(profile_active);
LV_IMG_DECLARE(control_0);
LV_IMG_DECLARE(control_white_0);
LV_IMG_DECLARE(shift_0);
LV_IMG_DECLARE(shift_white_0);
LV_IMG_DECLARE(opt_0);
LV_IMG_DECLARE(opt_white_0);
LV_IMG_DECLARE(cmd_0);
LV_IMG_DECLARE(cmd_white_0);
LV_IMG_DECLARE(wizard_charging_0);
LV_IMG_DECLARE(wizard_charging_1);
LV_IMG_DECLARE(wizard_charging_2);
LV_IMG_DECLARE(wizard_charging_3);

#define WPM_CHART_X 8
#define WPM_CHART_Y 82
#define WPM_CHART_WIDTH 80
#define WPM_CHART_HEIGHT 40
#define WPM_CHART_PADDING 4
#define WPM_CHART_MAX 100
#define WPM_SAMPLE_INTERVAL_MS 1000
#define WIZARD_X 104
#define WIZARD_Y 4
#define WIZARD_ANIMATION_DURATION_MS 800

static const lv_img_dsc_t *wizard_charging_frames[] = {
    &wizard_charging_0,
    &wizard_charging_1,
    &wizard_charging_2,
    &wizard_charging_3,
};

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
};

struct layer_status_state {
    uint8_t index;
    const char *label;
};

struct wpm_status_state {
    uint8_t wpm;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
static bool wpm_status_started;
static uint8_t implicit_modifier_counts[8];

static void wpm_status_work_cb(struct k_work *work);
static void modifier_status_work_cb(struct k_work *work);

K_WORK_DELAYABLE_DEFINE(wpm_status_work, wpm_status_work_cb);
K_WORK_DELAYABLE_DEFINE(modifier_status_work, modifier_status_work_cb);

static void draw_text(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                      const lv_font_t *font, lv_text_align_t align, const char *text) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, font, align);
    lv_canvas_draw_text(canvas, x, y, width, &label_dsc, text);
}

static void draw_wpm_grid(lv_obj_t *canvas) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    for (int i = 0; i <= 4; i++) {
        lv_coord_t x = WPM_CHART_X + (WPM_CHART_WIDTH * i / 4);
        lv_point_t points[2] = {{x, WPM_CHART_Y}, {x, WPM_CHART_Y + WPM_CHART_HEIGHT}};
        lv_canvas_draw_line(canvas, points, 2, &line_dsc);
    }

    for (int i = 0; i <= 4; i++) {
        lv_coord_t y = WPM_CHART_Y + (WPM_CHART_HEIGHT * i / 4);
        lv_point_t points[2] = {{WPM_CHART_X, y}, {WPM_CHART_X + WPM_CHART_WIDTH, y}};
        lv_canvas_draw_line(canvas, points, 2, &line_dsc);
    }
}

static void draw_wpm_graph(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 2);

    lv_point_t points[WPM_SAMPLES];
    const int graph_width = WPM_CHART_WIDTH - (WPM_CHART_PADDING * 2);
    const int graph_height = WPM_CHART_HEIGHT - (WPM_CHART_PADDING * 2);
    const int baseline = WPM_CHART_Y + WPM_CHART_HEIGHT - WPM_CHART_PADDING;

    for (int i = 0; i < WPM_SAMPLES; i++) {
        int value = state->wpm[i];
        if (value > WPM_CHART_MAX) {
            value = WPM_CHART_MAX;
        }

        points[i].x = WPM_CHART_X + WPM_CHART_PADDING +
                      (graph_width * i / (WPM_SAMPLES - 1));
        points[i].y = baseline - (value * graph_height / WPM_CHART_MAX);
    }

    lv_canvas_draw_line(canvas, points, WPM_SAMPLES, &line_dsc);
}

static void draw_output(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);

    draw_text(canvas, 8, 6, 38, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, "SIG");

    switch (state->selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        lv_canvas_draw_img(canvas, 56, 8, &usb, &img_dsc);
        break;
    case ZMK_TRANSPORT_BLE:
        if (!state->active_profile_bonded) {
            lv_canvas_draw_img(canvas, 55, 6, &bt_unbonded, &img_dsc);
        } else if (state->active_profile_connected) {
            lv_canvas_draw_img(canvas, 59, 6, &bt, &img_dsc);
        } else {
            lv_canvas_draw_img(canvas, 59, 6, &bt_no_signal, &img_dsc);
        }
        break;
    }
}

static void draw_battery_level(lv_obj_t *canvas, const struct status_state *state) {
    char text[8] = {};

    draw_text(canvas, 8, 28, 38, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, "BAT");

    if (state->charging) {
        snprintf(text, sizeof(text), "%d", state->battery);
        draw_text(canvas, 50, 28, 32, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, text);

        lv_draw_img_dsc_t img_dsc;
        lv_draw_img_dsc_init(&img_dsc);
        lv_canvas_draw_img(canvas, 79, 32, &bolt, &img_dsc);
    } else {
        snprintf(text, sizeof(text), "%d%%", state->battery);
        draw_text(canvas, 50, 28, 44, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, text);
    }
}

static void draw_wpm(lv_obj_t *canvas, const struct status_state *state) {
    char text[8] = {};
    uint8_t current_wpm = state->wpm[WPM_SAMPLES - 1];

    snprintf(text, sizeof(text), "%d", current_wpm);
    draw_text(canvas, 8, 50, 48, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, "WPM");
    draw_text(canvas, 62, 50, 30, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, text);
    draw_wpm_grid(canvas);
    draw_wpm_graph(canvas, state);
}

static void draw_modifiers(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);

    lv_canvas_draw_img(canvas, 98, 88,
                       state->modifiers & (MOD_LCTL | MOD_RCTL) ? &control_white_0 : &control_0,
                       &img_dsc);
    lv_canvas_draw_img(canvas, 114, 88,
                       state->modifiers & (MOD_LSFT | MOD_RSFT) ? &shift_white_0 : &shift_0,
                       &img_dsc);
    lv_canvas_draw_img(canvas, 98, 104,
                       state->modifiers & (MOD_LALT | MOD_RALT) ? &opt_white_0 : &opt_0,
                       &img_dsc);
    lv_canvas_draw_img(canvas, 114, 104,
                       state->modifiers & (MOD_LGUI | MOD_RGUI) ? &cmd_white_0 : &cmd_0,
                       &img_dsc);
}

static void draw_profiles(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);

    for (int i = 0; i < 5; i++) {
        lv_canvas_draw_img(canvas, 36 + i * 14, 128,
                           i == state->active_profile_index ? &profile_active : &profile,
                           &img_dsc);
    }
}

static void draw_layer(lv_obj_t *canvas, const struct status_state *state) {
    char text[12] = {};

    if (state->layer_label == NULL) {
        snprintf(text, sizeof(text), "LAYER %d", state->layer_index);
    } else {
        snprintf(text, sizeof(text), "%s", state->layer_label);
    }

    draw_text(canvas, 8, 148, VISTA508_DISPLAY_WIDTH - 16, &lv_font_montserrat_16,
              LV_TEXT_ALIGN_CENTER, text);
}

static void draw_canvas(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    ARG_UNUSED(cbuf);

    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    lv_draw_rect_dsc_t bg_dsc;
    init_rect_dsc(&bg_dsc, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, VISTA508_DISPLAY_WIDTH, VISTA508_DISPLAY_HEIGHT, &bg_dsc);

    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);
    lv_point_t top_rule[2] = {{8, 74}, {88, 74}};
    lv_canvas_draw_line(canvas, top_rule, 2, &line_dsc);
    lv_point_t bottom_rule[2] = {{8, 144}, {136, 144}};
    lv_canvas_draw_line(canvas, bottom_rule, 2, &line_dsc);

    draw_output(canvas, state);
    draw_battery_level(canvas, state);
    draw_wpm(canvas, state);
    draw_modifiers(canvas, state);
    draw_profiles(canvas, state);
    draw_layer(canvas, state);
}

static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    widget->state.battery = state.level;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);

    return (struct battery_status_state){
        .level = zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

static void set_output_status(struct zmk_widget_status *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, &state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    ARG_UNUSED(_eh);

    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

static void set_layer_status(struct zmk_widget_status *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);

    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

static void update_implicit_modifiers(uint8_t modifiers, bool pressed) {
    for (uint8_t bit = 0; bit < 8; bit++) {
        uint8_t flag = BIT(bit);
        if (!(modifiers & flag)) {
            continue;
        }

        if (pressed) {
            implicit_modifier_counts[bit]++;
        } else if (implicit_modifier_counts[bit] > 0) {
            implicit_modifier_counts[bit]--;
        }
    }
}

static uint8_t get_implicit_modifiers(void) {
    uint8_t modifiers = 0;

    for (uint8_t bit = 0; bit < 8; bit++) {
        if (implicit_modifier_counts[bit] > 0) {
            modifiers |= BIT(bit);
        }
    }

    return modifiers;
}

static void refresh_modifier_status(void) {
    struct zmk_widget_status *widget;
    uint8_t modifiers = zmk_hid_get_explicit_mods() | get_implicit_modifiers();

    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.modifiers = modifiers;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}

static void modifier_status_work_cb(struct k_work *work) {
    ARG_UNUSED(work);

    refresh_modifier_status();
}

static int modifier_status_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    bool is_modifier_key;

    if (ev == NULL || ev->usage_page != HID_USAGE_KEY) {
        return 0;
    }

    is_modifier_key = ev->keycode >= HID_USAGE_KEY_KEYBOARD_LEFTCONTROL &&
                      ev->keycode <= HID_USAGE_KEY_KEYBOARD_RIGHT_GUI;
    if (!is_modifier_key && ev->implicit_modifiers == 0) {
        return 0;
    }

    if (!is_modifier_key) {
        update_implicit_modifiers(ev->implicit_modifiers, ev->state);
    }

    k_work_reschedule_for_queue(zmk_display_work_q(), &modifier_status_work, K_MSEC(1));

    return 0;
}

ZMK_LISTENER(widget_modifier_status, modifier_status_listener);
ZMK_SUBSCRIPTION(widget_modifier_status, zmk_keycode_state_changed);

static void append_wpm_sample(struct zmk_widget_status *widget, uint8_t wpm) {
    for (int i = 0; i < WPM_SAMPLES - 1; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[WPM_SAMPLES - 1] = wpm;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void set_wpm_status(struct zmk_widget_status *widget, struct wpm_status_state state) {
    widget->state.wpm[WPM_SAMPLES - 1] = state.wpm;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

static void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_wpm_status(widget, state); }
}

struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh) {
    ARG_UNUSED(eh);

    return (struct wpm_status_state){.wpm = zmk_wpm_get_state()};
};

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state, wpm_status_update_cb,
                            wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

static void wpm_status_work_cb(struct k_work *work) {
    ARG_UNUSED(work);

    struct zmk_widget_status *widget;
    uint8_t wpm = zmk_wpm_get_state();
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { append_wpm_sample(widget, wpm); }

    k_work_schedule_for_queue(zmk_display_work_q(), &wpm_status_work,
                              K_MSEC(WPM_SAMPLE_INTERVAL_MS));
}

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, VISTA508_DISPLAY_WIDTH, VISTA508_DISPLAY_HEIGHT);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, VISTA508_DISPLAY_WIDTH, VISTA508_DISPLAY_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);

    widget->wizard = lv_animimg_create(widget->obj);
    lv_obj_align(widget->wizard, LV_ALIGN_TOP_LEFT, WIZARD_X, WIZARD_Y);
    lv_animimg_set_src(widget->wizard, (const void **)wizard_charging_frames,
                       ARRAY_SIZE(wizard_charging_frames));
    lv_animimg_set_duration(widget->wizard, WIZARD_ANIMATION_DURATION_MS);
    lv_animimg_set_repeat_count(widget->wizard, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(widget->wizard);

    sys_slist_append(&widgets, &widget->node);
    widget->state.modifiers = zmk_hid_get_explicit_mods() | get_implicit_modifiers();
    widget_battery_status_init();
    widget_output_status_init();
    widget_layer_status_init();
    widget_wpm_status_init();

    if (!wpm_status_started) {
        wpm_status_started = true;
        k_work_schedule_for_queue(zmk_display_work_q(), &wpm_status_work,
                                  K_MSEC(WPM_SAMPLE_INTERVAL_MS));
    }

    draw_canvas(widget->obj, widget->cbuf, &widget->state);

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
