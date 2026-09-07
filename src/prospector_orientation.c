#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/init.h>

static int set_prospector_orientation(void) {
    const struct device *display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display)) {
        return -ENODEV;
    }

    /* Prospector defaults to 270 degrees; normal is 90 degrees clockwise from it. */
    return display_set_orientation(display, DISPLAY_ORIENTATION_NORMAL);
}

/* Run after Prospector's orientation initializer at APPLICATION priority 60. */
SYS_INIT(set_prospector_orientation, APPLICATION, 61);
