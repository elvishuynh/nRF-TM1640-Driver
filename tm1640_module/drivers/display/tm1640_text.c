/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * TM1640 text rendering. Rasterizes strings into grid buffers.
 */

#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <drivers/display/tm1640.h>
#include <drivers/display/tm1640_font.h>
#include <drivers/display/tm1640_text.h>

LOG_MODULE_REGISTER(tm1640_text, CONFIG_LOG_DEFAULT_LEVEL);

int tm1640_print(const struct device *dev, const char *str, int offset)
{
	uint8_t buffer[TM1640_NUM_GRIDS] = {0};

	if (dev == NULL || str == NULL) {
		return -EINVAL;
	}

	for (int i = 0; str[i] != '\0'; i++) {
		uint8_t ch = (uint8_t)str[i];
		const uint8_t *glyph;

		/* Solid block for unprintable chars */
		if (ch < TM1640_FONT_FIRST_CHAR || ch > TM1640_FONT_LAST_CHAR) {
			static const uint8_t placeholder[TM1640_FONT_WIDTH] = {
				0x7F, 0x7F, 0x7F, 0x7F, 0x7F
			};
			glyph = placeholder;
		} else {
			glyph = tm1640_font_data[ch - TM1640_FONT_FIRST_CHAR];
		}

		int col = offset + (i * TM1640_FONT_STRIDE);

		/* Past the right edge, done */
		if (col >= TM1640_NUM_GRIDS) {
			break;
		}

		/* Off the left edge, skip */
		if (col + TM1640_FONT_WIDTH <= 0) {
			continue;
		}

		for (int j = 0; j < TM1640_FONT_WIDTH; j++) {
			int idx = col + j;

			if (idx < 0 || idx >= TM1640_NUM_GRIDS) {
				continue;
			}
			buffer[idx] = glyph[j];
		}
	}

	LOG_DBG("print \"%s\" offset=%d", str, offset);

	return tm1640_write(dev, buffer, TM1640_NUM_GRIDS);
}
