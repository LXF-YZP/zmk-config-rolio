/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include "../drivers/input/a320_motion.h"

static void expect_motion(struct a320_motion actual, int x, int y) {
    assert(actual.x == x);
    assert(actual.y == y);
}

int main(void) {
    /* Sensor bytes at the signed boundary must not reverse direction. */
    expect_motion(a320_decode(0x7f, 0x80, false), 127, -128);
    expect_motion(a320_decode(0x7f, 0x80, true), -128, 127);
    expect_motion(a320_decode(0xff, 0x01, true), 1, -1);
    expect_motion(a320_decode(0x00, 0x00, true), 0, 0);
    assert(a320_scale(127, 100) == 190);
    assert(a320_scale(-128, 100) == -192);
    assert(a320_scale(127, 400) == 762);
    assert(a320_scale(-128, 400) == -768);
    for (int percentage = 1; percentage <= 400; ++percentage) {
        int previous = a320_scale(-128, percentage);
        for (int delta = -127; delta <= 127; ++delta) {
            int scaled = a320_scale(delta, percentage);
            assert(scaled >= previous);
            assert(delta < 0 ? scaled <= 0 : scaled >= 0);
            previous = scaled;
        }
    }

    expect_motion(a320_scroll((struct a320_motion){0, 0}), 0, 0);
    expect_motion(a320_scroll((struct a320_motion){1, 3}), -1, -1);
    expect_motion(a320_scroll((struct a320_motion){-1, -3}), 1, 1);
    expect_motion(a320_scroll((struct a320_motion){0, 2}), 0, 0);
    expect_motion(a320_scroll((struct a320_motion){0, 20}), 0, -1);
    expect_motion(a320_scroll((struct a320_motion){0, -20}), 0, 1);
    expect_motion(a320_scroll((struct a320_motion){0, 127}), 0, -7);
    expect_motion(a320_scroll((struct a320_motion){0, -128}), 0, 5);
    bool scrolling = false;
    scrolling = a320_toggle_scroll_mode(scrolling);
    assert(scrolling);
    scrolling = a320_toggle_scroll_mode(scrolling);
    assert(!scrolling);
    puts("A320 signed movement, axis swap, gain and scroll checks passed");
    return 0;
}
