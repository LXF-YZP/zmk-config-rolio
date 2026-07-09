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
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/wpm.h>

#include "status.h"

LV_IMG_DECLARE(bolt);
LV_IMG_DECLARE(bt);
LV_IMG_DECLARE(bt_no_signal);
LV_IMG_DECLARE(bt_unbonded);
LV_IMG_DECLARE(usb);
LV_IMG_DECLARE(profile);
LV_IMG_DECLARE(profile_active);
LV_IMG_DECLARE(control_0);
LV_IMG_DECLARE(shift_0);
LV_IMG_DECLARE(opt_0);
LV_IMG_DECLARE(cmd_0);

LV_IMG_DECLARE(bongo_cat_double_tap1_03);
LV_IMG_DECLARE(bongo_cat_double_tap1_06);
LV_IMG_DECLARE(bongo_cat_double_tap2_02);
LV_IMG_DECLARE(bongo_cat_tap1_03);
LV_IMG_DECLARE(bongo_cat_tap2_03);

#define SRC(array) (const void **)array, (sizeof(array) / sizeof(array[0]))

enum bongo_anim_state {
    BONGO_ANIM_NONE,
    BONGO_ANIM_IDLE,
    BONGO_ANIM_SLOW,
    BONGO_ANIM_MID,
    BONGO_ANIM_FAST,
};

static const lv_img_dsc_t *bongo_idle_imgs[] = {
    &bongo_cat_double_tap1_06,
};
static const lv_img_dsc_t *bongo_slow_imgs[] = {
    &bongo_cat_tap1_03,
    &bongo_cat_tap2_03,
};
static const lv_img_dsc_t *bongo_mid_imgs[] = {
    &bongo_cat_tap1_03,
    &bongo_cat_tap2_03,
};
static const lv_img_dsc_t *bongo_fast_imgs[] = {
    &bongo_cat_double_tap2_02,
    &bongo_cat_double_tap1_03,
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

static void draw_text(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t width,
                      const lv_font_t *font, lv_text_align_t align, const char *text) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, font, align);
    lv_canvas_draw_text(canvas, x, y, width, &label_dsc, text);
}

static void draw_heading(lv_obj_t *canvas, lv_coord_t y, const char *label, const char *value) {
    draw_text(canvas, 8, y, 38, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, label);
    draw_text(canvas, 50, y, 42, &lv_font_montserrat_16, LV_TEXT_ALIGN_LEFT, value);
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
    draw_heading(canvas, 50, "WPM", text);
}

static void draw_modifiers(lv_obj_t *canvas) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);

    lv_canvas_draw_img(canvas, 98, 88, &control_0, &img_dsc);
    lv_canvas_draw_img(canvas, 114, 88, &shift_0, &img_dsc);
    lv_canvas_draw_img(canvas, 98, 104, &opt_0, &img_dsc);
    lv_canvas_draw_img(canvas, 114, 104, &cmd_0, &img_dsc);
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
    draw_modifiers(canvas);
    draw_profiles(canvas, state);
    draw_layer(canvas, state);
}

static void set_bongo_animation(struct zmk_widget_status *widget, uint8_t wpm) {
    enum bongo_anim_state next_state;
    uint16_t duration;

    if (widget->bongo == NULL) {
        return;
    }

    if (wpm < 5) {
        next_state = BONGO_ANIM_IDLE;
        duration = 10000;
    } else if (wpm < 30) {
        next_state = BONGO_ANIM_SLOW;
        duration = 2000;
    } else if (wpm < 70) {
        next_state = BONGO_ANIM_MID;
        duration = 500;
    } else {
        next_state = BONGO_ANIM_FAST;
        duration = 200;
    }

    if (widget->bongo_anim_state == next_state) {
        return;
    }

    switch (next_state) {
    case BONGO_ANIM_IDLE:
        lv_animimg_set_src(widget->bongo, SRC(bongo_idle_imgs));
        break;
    case BONGO_ANIM_SLOW:
        lv_animimg_set_src(widget->bongo, SRC(bongo_slow_imgs));
        break;
    case BONGO_ANIM_MID:
        lv_animimg_set_src(widget->bongo, SRC(bongo_mid_imgs));
        break;
    case BONGO_ANIM_FAST:
        lv_animimg_set_src(widget->bongo, SRC(bongo_fast_imgs));
        break;
    default:
        return;
    }

    lv_animimg_set_duration(widget->bongo, duration);
    lv_animimg_set_repeat_count(widget->bongo, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(widget->bongo);
    widget->bongo_anim_state = next_state;
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

static void set_wpm_status(struct zmk_widget_status *widget, struct wpm_status_state state) {
    for (int i = 0; i < WPM_SAMPLES - 1; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[WPM_SAMPLES - 1] = state.wpm;

    set_bongo_animation(widget, state.wpm);
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

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, VISTA508_DISPLAY_WIDTH, VISTA508_DISPLAY_HEIGHT);

    lv_obj_t *canvas = lv_canvas_create(widget->obj);
    lv_obj_align(canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(canvas, widget->cbuf, VISTA508_DISPLAY_WIDTH, VISTA508_DISPLAY_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);

    widget->bongo = lv_animimg_create(widget->obj);
    lv_obj_align(widget->bongo, LV_ALIGN_TOP_LEFT, 100, 32);
    widget->bongo_anim_state = BONGO_ANIM_NONE;
    set_bongo_animation(widget, 0);

    sys_slist_append(&widgets, &widget->node);
    widget_battery_status_init();
    widget_output_status_init();
    widget_layer_status_init();
    widget_wpm_status_init();

    draw_canvas(widget->obj, widget->cbuf, &widget->state);

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
