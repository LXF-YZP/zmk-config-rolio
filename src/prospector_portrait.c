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

static void apply_prospector_portrait_layout(struct k_work *work) {
    ARG_UNUSED(work);

    lv_obj_t *screen = lv_scr_act();

    if (screen == NULL || lv_obj_get_child_cnt(screen) < 3) {
        return;
    }

    /* Prospector creates the modifier, battery, and layer widgets in this order. */
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
}

K_WORK_DEFINE(prospector_portrait_layout_work, apply_prospector_portrait_layout);

static int schedule_prospector_portrait_layout(void) {
    /* The display initializer queued the screen creation on this same work queue. */
    k_work_submit_to_queue(zmk_display_work_q(), &prospector_portrait_layout_work);
    return 0;
}

SYS_INIT(schedule_prospector_portrait_layout, APPLICATION, 99);
