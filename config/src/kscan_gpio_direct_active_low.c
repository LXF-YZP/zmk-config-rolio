/*
 * Direct GPIO KSCAN controller that reads logical GPIO levels.
 *
 * The ZMK fork's direct scanner reads raw port values and therefore cannot
 * interpret GPIO_ACTIVE_LOW buttons. gpio_pin_get_dt() applies the polarity
 * flags from each input-gpios entry before reporting a key event.
 */
#define DT_DRV_COMPAT zmk_kscan_gpio_direct_active_low

#include <errno.h>
#include <stddef.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <zmk/debounce.h>

#define INPUT_COUNT(n) DT_INST_PROP_LEN(n, input_gpios)
#define INPUT_SPEC_INIT(idx, n) GPIO_DT_SPEC_GET_BY_IDX(DT_DRV_INST(n), input_gpios, idx),

struct direct_active_low_config {
    const struct gpio_dt_spec *inputs;
    size_t input_count;
    struct zmk_debounce_config debounce;
    int poll_period_ms;
};

struct direct_active_low_data {
    const struct device *dev;
    kscan_callback_t callback;
    struct k_work_delayable work;
    struct zmk_debounce_state *states;
};

static int direct_active_low_configure(const struct device *dev, kscan_callback_t callback) {
    struct direct_active_low_data *data = dev->data;

    if (callback == NULL) {
        return -EINVAL;
    }

    data->callback = callback;
    return 0;
}

static void direct_active_low_scan(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct direct_active_low_data *data =
        CONTAINER_OF(delayable, struct direct_active_low_data, work);
    const struct direct_active_low_config *config = data->dev->config;

    for (size_t i = 0; i < config->input_count; i++) {
        const int active = gpio_pin_get_dt(&config->inputs[i]);
        if (active < 0) {
            continue;
        }

        zmk_debounce_update(&data->states[i], active != 0, config->poll_period_ms,
                            &config->debounce);
        if (zmk_debounce_get_changed(&data->states[i])) {
            data->callback(data->dev, 0, i, zmk_debounce_is_pressed(&data->states[i]));
        }
    }

    k_work_reschedule(&data->work, K_MSEC(config->poll_period_ms));
}

static int direct_active_low_enable(const struct device *dev) {
    struct direct_active_low_data *data = dev->data;
    k_work_reschedule(&data->work, K_NO_WAIT);
    return 0;
}

static int direct_active_low_disable(const struct device *dev) {
    struct direct_active_low_data *data = dev->data;
    k_work_cancel_delayable(&data->work);
    return 0;
}

static int direct_active_low_init(const struct device *dev) {
    const struct direct_active_low_config *config = dev->config;
    struct direct_active_low_data *data = dev->data;

    data->dev = dev;
    for (size_t i = 0; i < config->input_count; i++) {
        const struct gpio_dt_spec *gpio = &config->inputs[i];
        if (!device_is_ready(gpio->port)) {
            return -ENODEV;
        }

        const int err = gpio_pin_configure_dt(gpio, GPIO_INPUT);
        if (err < 0) {
            return err;
        }
    }

    k_work_init_delayable(&data->work, direct_active_low_scan);
    return 0;
}

static const struct kscan_driver_api direct_active_low_api = {
    .config = direct_active_low_configure,
    .enable_callback = direct_active_low_enable,
    .disable_callback = direct_active_low_disable,
};

#define DIRECT_ACTIVE_LOW_INIT(n)                                                                  \
    BUILD_ASSERT(DT_INST_PROP(n, debounce_press_ms) <= DEBOUNCE_COUNTER_MAX,                       \
                 "debounce-press-ms is too large");                                               \
    BUILD_ASSERT(DT_INST_PROP(n, debounce_release_ms) <= DEBOUNCE_COUNTER_MAX,                     \
                 "debounce-release-ms is too large");                                             \
    static const struct gpio_dt_spec direct_active_low_inputs_##n[] = {                            \
        LISTIFY(INPUT_COUNT(n), INPUT_SPEC_INIT, (, ), n)};                                         \
    static struct zmk_debounce_state direct_active_low_states_##n[INPUT_COUNT(n)];                 \
    static struct direct_active_low_data direct_active_low_data_##n = {                            \
        .states = direct_active_low_states_##n,                                                     \
    };                                                                                             \
    static const struct direct_active_low_config direct_active_low_config_##n = {                  \
        .inputs = direct_active_low_inputs_##n,                                                     \
        .input_count = INPUT_COUNT(n),                                                             \
        .debounce = {                                                                              \
            .debounce_press_ms = DT_INST_PROP(n, debounce_press_ms),                               \
            .debounce_release_ms = DT_INST_PROP(n, debounce_release_ms),                           \
        },                                                                                         \
        .poll_period_ms = DT_INST_PROP(n, poll_period_ms),                                         \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, direct_active_low_init, NULL, &direct_active_low_data_##n,            \
                          &direct_active_low_config_##n, POST_KERNEL, CONFIG_KSCAN_INIT_PRIORITY,   \
                          &direct_active_low_api);

DT_INST_FOREACH_STATUS_OKAY(DIRECT_ACTIVE_LOW_INIT)
