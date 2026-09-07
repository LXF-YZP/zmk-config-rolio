#include <errno.h>

#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_central_status_changed.h>

#define PORTRAIT_CONTENT_WIDTH 224
#define PORTRAIT_ROLLER_HEIGHT 160
#define PORTRAIT_MODIFIER_HEIGHT 48
#define PORTRAIT_BATTERY_HEIGHT 48

#define OPERATOR_SCREEN_CHILD_COUNT 5
#define OPERATOR_WPM_BAR_COUNT 26
#define OPERATOR_WPM_CHILD_COUNT (OPERATOR_WPM_BAR_COUNT + 3)
#define OPERATOR_CONTENT_WIDTH 220
#define OPERATOR_WPM_HEIGHT 80
#define OPERATOR_WPM_BAR_WIDTH 6
#define OPERATOR_WPM_BAR_GAP 2
#define OPERATOR_LAYER_DOT_GAP 3
#define PORTRAIT_LAYOUT_INITIAL_DELAY_MS 100
#define PORTRAIT_LAYOUT_RETRY_DELAY_MS 50
#define PORTRAIT_LAYOUT_MAX_RETRIES 40

static uint8_t portrait_layout_retries;
static struct k_work_delayable prospector_portrait_layout_work;

static int set_prospector_portrait_orientation(void) {
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display)) {
        return -ENODEV;
    }

    /* The current 90-degree landscape orientation plus 90 degrees clockwise. */
    return display_set_orientation(display, DISPLAY_ORIENTATION_ROTATED_180);
}

/* Run after Prospector's orientation initializer at APPLICATION priority 60. */
SYS_INIT(set_prospector_portrait_orientation, APPLICATION, 61);

#if defined(CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR)

LV_FONT_DECLARE(FG_Medium_20);

struct dual_battery_state {
    uint8_t local_level;
    uint8_t peripheral_level;
    bool peripheral_connected;
};

static uint8_t dual_battery_peripheral_level;
static bool dual_battery_peripheral_connected;
static lv_obj_t *dual_battery_arcs[2];
static lv_obj_t *dual_battery_labels[2];

static void dual_battery_set_arc(lv_obj_t *arc, lv_obj_t *label, uint8_t level,
                                 bool connected) {
    bool low_battery = connected && level > 0 && level <= 20;

    if (low_battery) {
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x584028), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0xC08040), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(label, lv_color_hex(0xC08040), LV_PART_MAIN);
    } else if (connected) {
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x2a4036), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x54806c), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    } else {
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x282c30), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x383c42), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(label, lv_color_hex(0x909090), LV_PART_MAIN);
    }

    lv_arc_set_value(arc, connected ? level : 0);
    if (connected && level > 0) {
        lv_label_set_text_fmt(label, "%d", (int)level);
    } else {
        lv_label_set_text(label, "--");
    }
}

static void dual_battery_update_cb(struct dual_battery_state state) {
    if (dual_battery_arcs[0] == NULL || dual_battery_arcs[1] == NULL ||
        dual_battery_labels[0] == NULL || dual_battery_labels[1] == NULL) {
        return;
    }

    /* The central half is always the local keyboard battery. */
    dual_battery_set_arc(dual_battery_arcs[0], dual_battery_labels[0], state.local_level, true);
    dual_battery_set_arc(dual_battery_arcs[1], dual_battery_labels[1], state.peripheral_level,
                         state.peripheral_connected);
}

static struct dual_battery_state dual_battery_get_state(const zmk_event_t *eh) {
    if (eh != NULL) {
        const struct zmk_peripheral_battery_state_changed *battery_event =
            as_zmk_peripheral_battery_state_changed(eh);
        if (battery_event != NULL && battery_event->source == 0) {
            dual_battery_peripheral_level = battery_event->state_of_charge;
            dual_battery_peripheral_connected = battery_event->state_of_charge > 0;
        }

        const struct zmk_split_central_status_changed *connection_event =
            as_zmk_split_central_status_changed(eh);
        if (connection_event != NULL && connection_event->slot == 0) {
            dual_battery_peripheral_connected = connection_event->connected;
            if (!connection_event->connected) {
                dual_battery_peripheral_level = 0;
            }
        }
    }

    return (struct dual_battery_state){
        .local_level = zmk_battery_state_of_charge(),
        .peripheral_level = dual_battery_peripheral_level,
        .peripheral_connected = dual_battery_peripheral_connected,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(prospector_dual_battery, struct dual_battery_state,
                            dual_battery_update_cb, dual_battery_get_state)
ZMK_SUBSCRIPTION(prospector_dual_battery, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(prospector_dual_battery, zmk_peripheral_battery_state_changed);
ZMK_SUBSCRIPTION(prospector_dual_battery, zmk_split_central_status_changed);

static void create_dual_battery_panel(lv_obj_t *screen, lv_obj_t *upstream_battery) {
    lv_obj_add_flag(upstream_battery, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *panel = lv_obj_create(screen);
    lv_obj_set_size(panel, 132, 62);
    lv_obj_set_pos(panel, 54, 142);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *arc = lv_arc_create(panel);
        lv_obj_set_size(arc, 54, 54);
        lv_obj_set_pos(arc, i * 68, 4);
        lv_arc_set_range(arc, 0, 100);
        lv_arc_set_value(arc, 0);
        lv_arc_set_bg_angles(arc, 0, 360);
        lv_arc_set_rotation(arc, 270);
        lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 5, LV_PART_INDICATOR);
        lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
        lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x282c30), LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0x383c42), LV_PART_INDICATOR);

        lv_obj_t *label = lv_label_create(panel);
        lv_obj_set_style_text_font(label, &FG_Medium_20, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_hex(0x909090), LV_PART_MAIN);
        lv_label_set_text(label, "--");
        lv_obj_align_to(label, arc, LV_ALIGN_CENTER, 0, 0);

        dual_battery_arcs[i] = arc;
        dual_battery_labels[i] = label;
    }

    prospector_dual_battery_init();
}

static bool apply_operator_portrait_layout(lv_obj_t *screen) {
    if (lv_obj_get_child_cnt(screen) < OPERATOR_SCREEN_CHILD_COUNT) {
        return false;
    }

    /* Operator creates modifier, WPM, layer dots, battery, and output in this order. */
    lv_obj_t *modifier = lv_obj_get_child(screen, 0);
    lv_obj_t *wpm = lv_obj_get_child(screen, 1);
    lv_obj_t *layer = lv_obj_get_child(screen, 2);
    lv_obj_t *battery = lv_obj_get_child(screen, 3);
    lv_obj_t *output = lv_obj_get_child(screen, 4);

    static lv_obj_t *dual_battery_screen;
    if (dual_battery_screen != screen) {
        create_dual_battery_panel(screen, battery);
        dual_battery_screen = screen;
    }

    lv_obj_set_size(modifier, 230, 24);
    lv_obj_set_pos(modifier, 5, 4);

    lv_obj_set_size(wpm, OPERATOR_CONTENT_WIDTH, OPERATOR_WPM_HEIGHT);
    lv_obj_set_pos(wpm, 10, 34);

    if (lv_obj_get_child_cnt(wpm) >= OPERATOR_WPM_CHILD_COUNT) {
        const int bars_width =
            OPERATOR_WPM_BAR_COUNT * OPERATOR_WPM_BAR_WIDTH +
            (OPERATOR_WPM_BAR_COUNT - 1) * OPERATOR_WPM_BAR_GAP;
        const int bars_x = (OPERATOR_CONTENT_WIDTH - bars_width) / 2;

        for (int i = 0; i < OPERATOR_WPM_BAR_COUNT; i++) {
            lv_obj_t *bar = lv_obj_get_child(wpm, i);
            lv_obj_set_size(bar, OPERATOR_WPM_BAR_WIDTH, OPERATOR_WPM_HEIGHT);
            lv_obj_set_pos(bar, bars_x + i * (OPERATOR_WPM_BAR_WIDTH + OPERATOR_WPM_BAR_GAP),
                           0);
        }

        /* Upstream positions the peak marker using its fixed 260 px landscape width. */
        lv_obj_t *peak = lv_obj_get_child(wpm, OPERATOR_WPM_BAR_COUNT);
        lv_obj_set_style_opa(peak, LV_OPA_TRANSP, LV_PART_MAIN);

        lv_obj_t *wpm_label = lv_obj_get_child(wpm, OPERATOR_WPM_BAR_COUNT + 1);
        lv_obj_align(wpm_label, LV_ALIGN_TOP_LEFT, -3, -9);

        lv_obj_t *layer_label = lv_obj_get_child(wpm, OPERATOR_WPM_BAR_COUNT + 2);
        lv_obj_align(layer_label, LV_ALIGN_BOTTOM_RIGHT, -2, 7);
    }

    lv_obj_set_size(layer, OPERATOR_CONTENT_WIDTH, 6);
    lv_obj_set_pos(layer, 10, 126);

    uint32_t layer_dot_count = lv_obj_get_child_cnt(layer);
    if (layer_dot_count > 0) {
        int dot_width =
            (OPERATOR_CONTENT_WIDTH - (layer_dot_count - 1) * OPERATOR_LAYER_DOT_GAP) /
            layer_dot_count;

        for (uint32_t i = 0; i < layer_dot_count; i++) {
            lv_obj_t *dot = lv_obj_get_child(layer, i);
            lv_obj_set_size(dot, dot_width, 6);
            lv_obj_set_pos(dot, i * (dot_width + OPERATOR_LAYER_DOT_GAP), 0);
        }
    }

    /* Stack the two 62 px information blocks to fit the narrower portrait screen. */
    lv_obj_set_pos(battery, 54, 142);
    lv_obj_set_pos(output, 62, 212);

    return true;
}

#else

static bool apply_classic_portrait_layout(lv_obj_t *screen) {
    if (lv_obj_get_child_cnt(screen) < 3) {
        return false;
    }

    /* Prospector Classic creates modifier, battery, and layer widgets in this order. */
    lv_obj_t *modifier = lv_obj_get_child(screen, 0);
    lv_obj_t *battery = lv_obj_get_child(screen, 1);
    lv_obj_t *roller = lv_obj_get_child(screen, 2);

    lv_obj_set_size(roller, PORTRAIT_CONTENT_WIDTH, PORTRAIT_ROLLER_HEIGHT);
    lv_obj_align(roller, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_set_size(modifier, PORTRAIT_CONTENT_WIDTH, PORTRAIT_MODIFIER_HEIGHT);
    lv_obj_set_flex_flow(modifier, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(modifier, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(modifier, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(modifier, 8, LV_PART_MAIN);
    lv_obj_align(modifier, LV_ALIGN_BOTTOM_MID, 0, -(PORTRAIT_BATTERY_HEIGHT + 4));

    lv_obj_set_size(battery, lv_pct(100), PORTRAIT_BATTERY_HEIGHT);
    lv_obj_align(battery, LV_ALIGN_BOTTOM_MID, 0, 0);

    return true;
}

#endif

static void apply_prospector_portrait_layout(struct k_work *work) {
    ARG_UNUSED(work);

    lv_obj_t *screen = lv_scr_act();
    bool applied = false;

    if (screen != NULL) {
#if defined(CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR)
        applied = apply_operator_portrait_layout(screen);
#else
        applied = apply_classic_portrait_layout(screen);
#endif
    }

    /* ZMK creates and activates the status screen asynchronously. */
    if (!applied && portrait_layout_retries++ < PORTRAIT_LAYOUT_MAX_RETRIES) {
        k_work_reschedule_for_queue(zmk_display_work_q(), &prospector_portrait_layout_work,
                                    K_MSEC(PORTRAIT_LAYOUT_RETRY_DELAY_MS));
    }
}

static K_WORK_DELAYABLE_DEFINE(prospector_portrait_layout_work, apply_prospector_portrait_layout);

static int schedule_prospector_portrait_layout(void) {
    k_work_reschedule_for_queue(zmk_display_work_q(), &prospector_portrait_layout_work,
                                K_MSEC(PORTRAIT_LAYOUT_INITIAL_DELAY_MS));
    return 0;
}

SYS_INIT(schedule_prospector_portrait_layout, APPLICATION, 99);
