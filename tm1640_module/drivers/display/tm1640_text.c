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

/* Safely mirrors the standard 7 rows (bits 0-6) to fix the font orientation, 
 * but preserves the special Row -1 LED (bit 7) */
static uint8_t flip_logical_rows(uint8_t col_data)
{
	uint8_t flipped = 0;
	if (col_data & 0x01) flipped |= 0x40; // Font Top -> Hardware Top (Row 7)
	if (col_data & 0x02) flipped |= 0x20;
	if (col_data & 0x04) flipped |= 0x10;
	if (col_data & 0x08) flipped |= 0x08; // Center stays center
	if (col_data & 0x10) flipped |= 0x04;
	if (col_data & 0x20) flipped |= 0x02;
	if (col_data & 0x40) flipped |= 0x01; // Font Bottom -> Hardware Bottom (Row 1)
	
	/* Preserve the special bit 7 (Row -1) exactly as is */
	flipped |= (col_data & 0x80); 
	
	return flipped;
}

/* Maps 17 logical columns to 16 physical grids for the Frankenstein board */
static void tm1640_map_logical_to_physical(const uint8_t *logical, uint8_t *physical)
{
	memset(physical, 0, TM1640_NUM_GRIDS);

	/* Create a row-flipped version of the logical buffer first */
	uint8_t flipped_logical[TM1640_LOGICAL_COLUMNS];
	for (int i = 0; i < TM1640_LOGICAL_COLUMNS; i++) {
		flipped_logical[i] = flip_logical_rows(logical[i]);
	}

	for (int i = 0; i < TM1640_LOGICAL_COLUMNS; i++) {
		if (i < 5) {
			physical[15 - i] = flipped_logical[i] & 0x7F;
		} else if (i > 5) {
			physical[16 - i] = flipped_logical[i] & 0x7F;
		}
	}

	/* Col 12 (logical[11]) Row -1 (bit 7) */
	if (flipped_logical[11] & 0x80) {
		physical[5] |= 0x80;
	}

	/* Col 6 (logical[5]) mapping to Grid 0x08-0x0F */
	/* Note: Because we flipped the bits above, Font Top (0x40) correctly routes to physical[15] (Row 7) */
	if (flipped_logical[5] & 0x80) physical[8] |= 0x80;  // Row -1
	if (flipped_logical[5] & 0x01) physical[9] |= 0x80;  // Row 1 (Bottom)
	if (flipped_logical[5] & 0x02) physical[10] |= 0x80; // Row 2
	if (flipped_logical[5] & 0x04) physical[11] |= 0x80; // Row 3
	if (flipped_logical[5] & 0x08) physical[12] |= 0x80; // Row 4
	if (flipped_logical[5] & 0x10) physical[13] |= 0x80; // Row 5
	if (flipped_logical[5] & 0x20) physical[14] |= 0x80; // Row 6
	if (flipped_logical[5] & 0x40) physical[15] |= 0x80; // Row 7 (Top)
}

int tm1640_print(const struct device *dev, const char *str, int offset)
{
	uint8_t logical[TM1640_LOGICAL_COLUMNS] = {0};
	uint8_t physical[TM1640_NUM_GRIDS] = {0};

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
		if (col >= TM1640_LOGICAL_COLUMNS) {
			break;
		}

		/* Off the left edge, skip */
		if (col + TM1640_FONT_WIDTH <= 0) {
			continue;
		}

		for (int j = 0; j < TM1640_FONT_WIDTH; j++) {
			int idx = col + j;

			if (idx < 0 || idx >= TM1640_LOGICAL_COLUMNS) {
				continue;
			}
			logical[idx] = glyph[j];
		}
	}

	LOG_DBG("print \"%s\" offset=%d", str, offset);

	tm1640_map_logical_to_physical(logical, physical);

	return tm1640_write(dev, physical, TM1640_NUM_GRIDS);
}
