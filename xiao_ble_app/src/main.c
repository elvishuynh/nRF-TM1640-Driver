/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Scrolling "Hello World" demo for the TM1640.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <drivers/display/tm1640.h>
#include <drivers/display/tm1640_text.h>

LOG_MODULE_REGISTER(tm1640_demo, LOG_LEVEL_DBG);

#define SCROLL_DELAY_MS 80
#define SCROLL_MSG      "Hello World"

/* strlen * stride gives total pixel width; start offscreen right, end offscreen left */
#define SCROLL_START    TM1640_LOGICAL_COLUMNS
#define SCROLL_END      (-(int)(sizeof(SCROLL_MSG) - 1) * 6)

int main(void)
{
	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(tm1640));

	if (!device_is_ready(dev)) {
		LOG_ERR("TM1640 not ready");
		return -1;
	}

	LOG_INF("TM1640 scroll demo started");

	tm1640_clear(dev);
	tm1640_set_brightness(dev, TM1640_BRIGHTNESS_MAX);
	tm1640_display_on(dev);

	while (1) {
		for (int off = SCROLL_START; off >= SCROLL_END; off--) {
			tm1640_print(dev, SCROLL_MSG, off);
			k_msleep(SCROLL_DELAY_MS);
		}
	}

	return 0;
}

