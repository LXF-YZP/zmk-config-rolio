/*
 * SPDX-License-Identifier: MIT
 *
 * Q20 ADBS-A320 input driver. Register addresses and legacy scroll behavior
 * follow ZitaoTech/zmk, bbkeyboard_tp (6bb7b0244fa644368cbce7a4790c5edff7ecb4f5).
 * Uses Zephyr input events so official ZMK owns mouse reports and Studio RPC.
 */
#define DT_DRV_COMPAT avago_a320

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zmk/hid_indicators.h>

#include "a320_motion.h"

LOG_MODULE_REGISTER(q20_a320, CONFIG_ZMK_LOG_LEVEL);

#define A320_REG_MOTION 0x02
#define A320_REG_DELTA_X 0x03
#define A320_REG_DELTA_Y 0x04
#define A320_MOTION BIT(7)
#define A320_OVERFLOW BIT(4)
#define A320_STACK_SIZE 1536

struct a320_config {
    struct i2c_dt_spec bus;
    bool swap_xy;
};

struct a320_data {
    struct k_thread thread;
    K_KERNEL_STACK_MEMBER(stack, A320_STACK_SIZE);
};

static int a320_read_motion(const struct device *dev, struct a320_motion *motion) {
    const struct a320_config *cfg = dev->config;
    uint8_t status, x, y;
    int ret = i2c_reg_read_byte_dt(&cfg->bus, A320_REG_MOTION, &status);
    if (ret < 0) {
        return ret;
    }
    if (!(status & A320_MOTION)) {
        return -EAGAIN;
    }

    /* Read Motion once, then both delta registers, including on overflow. */
    ret = i2c_reg_read_byte_dt(&cfg->bus, A320_REG_DELTA_X, &x);
    if (ret < 0) {
        return ret;
    }
    ret = i2c_reg_read_byte_dt(&cfg->bus, A320_REG_DELTA_Y, &y);
    if (ret < 0) {
        return ret;
    }
    if (status & A320_OVERFLOW) {
        return -EOVERFLOW;
    }
    *motion = a320_decode(x, y, cfg->swap_xy);
    return 0;
}

static void a320_poll(void *device, void *unused1, void *unused2) {
    const struct device *dev = device;
    bool io_error = false;

    while (true) {
        struct a320_motion motion;
        int ret = a320_read_motion(dev, &motion);
        bool scrolling = a320_scroll_mode(zmk_hid_indicators_get_current_profile());
        if (ret == 0) {
            io_error = false;
            if (scrolling) {
                motion = a320_scroll(motion);
            } else {
                motion.x = a320_scale(motion.x, CONFIG_TRACKPAD_SPEEDMULTIPLIER_HORIZONTAL);
                motion.y = a320_scale(motion.y, CONFIG_TRACKPAD_SPEEDMULTIPLIER_VERTICAL);
            }
            if (motion.x || motion.y) {
                input_report_rel(dev, scrolling ? INPUT_REL_HWHEEL : INPUT_REL_X, motion.x,
                                 false, K_FOREVER);
                input_report_rel(dev, scrolling ? INPUT_REL_WHEEL : INPUT_REL_Y, motion.y,
                                 true, K_FOREVER);
            }
        } else if (ret != -EAGAIN && ret != -EOVERFLOW && !io_error) {
            LOG_WRN("Trackpad I2C read failed: %d", ret);
            io_error = true;
        }
        k_sleep(K_MSEC(1000 / CONFIG_INPUT_A320_POLLINGRATE +
                       (scrolling ? CONFIG_TRACKPAD_SCROLL_INTERVAL : 0)));
    }
}

static int a320_init(const struct device *dev) {
    const struct a320_config *cfg = dev->config;
    struct a320_data *data = dev->data;
    if (!i2c_is_ready_dt(&cfg->bus)) {
        LOG_ERR("Trackpad I2C bus is not ready");
        return -ENODEV;
    }

    /* Drain stale motion without changing the sensor's power-on settings. */
    struct a320_motion discarded;
    int ret = a320_read_motion(dev, &discarded);
    if (ret < 0 && ret != -EAGAIN && ret != -EOVERFLOW) {
        LOG_WRN("Trackpad not responding yet: %d; polling will retry", ret);
    }
    k_thread_create(&data->thread, data->stack, K_KERNEL_STACK_SIZEOF(data->stack), a320_poll,
                    (void *)dev, NULL, NULL, K_PRIO_PREEMPT(10), 0, K_MSEC(100));
    LOG_INF("A320 input ready; swap XY: %d", cfg->swap_xy);
    return 0;
}

#define A320_DEFINE(inst)                                                                           \
    static struct a320_data a320_data_##inst;                                                        \
    static const struct a320_config a320_config_##inst = {                                          \
        .bus = I2C_DT_SPEC_INST_GET(inst),                                                          \
        .swap_xy = DT_INST_PROP(inst, swap_xy),                                                     \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(inst, a320_init, NULL, &a320_data_##inst, &a320_config_##inst, POST_KERNEL,   \
                          CONFIG_KERNEL_INIT_PRIORITY_DEVICE, NULL);

DT_INST_FOREACH_STATUS_OKAY(A320_DEFINE)
