/* SPDX-License-Identifier: MIT */
#pragma once

#include <stdbool.h>
#include <stdint.h>

struct a320_motion {
    int16_t x;
    int16_t y;
};

/* Convert each sensor byte to a signed delta before swapping or scaling. */
static inline struct a320_motion a320_decode(uint8_t raw_x, uint8_t raw_y, bool swap_xy) {
    int16_t x = raw_x < 128 ? raw_x : (int16_t)raw_x - 256;
    int16_t y = raw_y < 128 ? raw_y : (int16_t)raw_y - 256;
    return swap_xy ? (struct a320_motion){y, x} : (struct a320_motion){x, y};
}

static inline int16_t a320_scale(int16_t delta, int percentage) {
    return (int32_t)delta * 3 * percentage / 200;
}

static inline bool a320_toggle_scroll_mode(bool enabled) {
    return !enabled;
}

static inline struct a320_motion a320_scroll(struct a320_motion motion) {
    int magnitude = motion.y < 0 ? -motion.y : motion.y;
    int divisor = magnitude >= 128 ? 24 : magnitude >= 64 ? 16 : magnitude >= 32 ? 12
                                                        : magnitude >= 21   ? 8
                                                                            : 0;
    if (divisor) {
        return (struct a320_motion){-motion.x / divisor, -motion.y / divisor};
    }
    return (struct a320_motion){-(motion.x > 0) + (motion.x < 0),
                               magnitude >= 3 ? -(motion.y > 0) + (motion.y < 0) : 0};
}
