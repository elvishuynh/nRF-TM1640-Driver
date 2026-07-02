/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Test app for the TM1640 display driver on XIAO BLE.
 * Cycles through a few patterns to make sure it works (writes, brightness, etc.).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <string.h>

/* Logging */
#include <zephyr/logging/log.h>

#include <drivers/display/tm1640.h>

/* Register logging */
LOG_MODULE_REGISTER(tm1640_test, LOG_LEVEL_DBG);


#define PATTERN_DELAY_MS  500

int main(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(tm1640));
    uint8_t buf[TM1640_NUM_GRIDS];

    if (!device_is_ready(dev)) {
        LOG_ERR("TM1640 device not ready");
        return -1;
    }

    LOG_INF("TM1640 test application started");

    tm1640_clear(dev);
    tm1640_set_brightness(dev, TM1640_BRIGHTNESS_MAX);
    tm1640_display_on(dev);

    while (1) {
        LOG_INF("Pattern: Walking column");
        for (int i = 0; i < TM1640_NUM_GRIDS; i++) {
            memset(buf, 0x00, sizeof(buf));
            buf[i] = 0xFF;
            tm1640_write(dev, buf, TM1640_NUM_GRIDS);
            k_msleep(PATTERN_DELAY_MS);
        }

        LOG_INF("Pattern: Progressive fill");
        memset(buf, 0x00, sizeof(buf));
        for (int i = 0; i < TM1640_NUM_GRIDS; i++) {
            buf[i] = 0xFF;
            tm1640_write(dev, buf, TM1640_NUM_GRIDS);
            k_msleep(PATTERN_DELAY_MS);
        }

        LOG_INF("Pattern: Brightness ramp");
        memset(buf, 0xFF, sizeof(buf));
        tm1640_write(dev, buf, TM1640_NUM_GRIDS);
        for (uint8_t b = 0; b <= TM1640_BRIGHTNESS_MAX; b++) {
            LOG_DBG("Setting brightness to %d", b);
            tm1640_set_brightness(dev, b);
            k_msleep(PATTERN_DELAY_MS);
        }

        LOG_INF("Pattern: Checkerboard");
        for (int t = 0; t < 6; t++) {
            for (int i = 0; i < TM1640_NUM_GRIDS; i++) {
                if (t % 2 == 0) {
                    buf[i] = (i % 2 == 0) ? 0xAA : 0x55;
                } else {
                    buf[i] = (i % 2 == 0) ? 0x55 : 0xAA;
                }
            }
            tm1640_write(dev, buf, TM1640_NUM_GRIDS);
            k_msleep(PATTERN_DELAY_MS);
        }

        LOG_INF("Cycle complete — restarting");
        tm1640_clear(dev);
        k_msleep(1000);
    }

    return 0;
}