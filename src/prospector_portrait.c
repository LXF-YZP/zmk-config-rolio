#include <errno.h>

#include <lvgl.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zmk/display.h>

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
