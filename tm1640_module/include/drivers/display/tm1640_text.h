/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Text rendering layer for the TM1640.
 */

#ifndef DRIVERS_DISPLAY_TM1640_TEXT_H_
#define DRIVERS_DISPLAY_TM1640_TEXT_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Write an ASCII string to the display using the 5x7 font.
 * Offset shifts the text by N grid columns (negative = shift left).
 * Out-of-bounds columns are clipped. Calls tm1640_write() internally.
 * Returns 0 on success, -EINVAL on bad args.
 */
int tm1640_print(const struct device *dev, const char *str, int offset);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_DISPLAY_TM1640_TEXT_H_ */
