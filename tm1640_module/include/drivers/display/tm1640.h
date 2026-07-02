/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Header for the TM1640 LED Matrix driver.
 */

#ifndef DRIVERS_DISPLAY_TM1640_H_
#define DRIVERS_DISPLAY_TM1640_H_

#include <zephyr/device.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* TM1640 has 16 grids (segments) */
#define TM1640_NUM_GRIDS      16

/* Max brightness (0-7 scale) */
#define TM1640_BRIGHTNESS_MAX 7

/*
 * Turn on display at the currently set brightness level.
 * Returns 0 on success, or a negative error code on failure.
 */
int tm1640_display_on(const struct device *dev);

/*
 * Turn off the display. Preserves segment data in internal RAM.
 * Returns 0 on success, or a negative error code on failure.
 */
int tm1640_display_off(const struct device *dev);

/*
 * Set brightness (0 to 7).
 * Maps to duty cycle (0 = 1/16, up to 7 = 14/16 pulse width).
 * Takes effect immediately if display is on.
 * Returns 0 on success, -EINVAL if level > 7.
 */
int tm1640_set_brightness(const struct device *dev, uint8_t level);

/*
 * Write segment data using auto-increment starting at Grid 1 (0xC0).
 * Writes up to 16 bytes. If len < 16, it auto-fills the rest with 0s.
 * Returns 0 on success, or a negative error code on failure.
 */
int tm1640_write(const struct device *dev, const uint8_t *data, size_t len);

/*
 * Clear all grids (writes 0s).
 * Returns 0 on success, or a negative error code on failure.
 */
int tm1640_clear(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_DISPLAY_TM1640_H_ */
