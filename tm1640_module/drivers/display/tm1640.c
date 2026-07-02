/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bit-banged driver for Titan Micro TM1640 (16-segment x 8-bit LED controller).
 * Implements CLK + DIN serial interface over Zephyr GPIOs.
 *
 * Protocol:
 * - Data latches on CLK rising edge.
 * - Change DIN only when CLK is LOW.
 * - START: CLK HIGH, DIN HIGH -> LOW.
 * - STOP/END: CLK HIGH, DIN LOW -> HIGH.
 * - LSB first.
 */

#define DT_DRV_COMPAT titanmec_tm1640

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <drivers/display/tm1640.h>

LOG_MODULE_REGISTER(tm1640, CONFIG_LOG_DEFAULT_LEVEL);

/* Command bytes */

/* Data write mode: auto-increment address */
#define TM1640_CMD_DATA       0x40
/* Start address at Grid 1 */
#define TM1640_CMD_ADDR_BASE  0xC0
/* Display ON command + brightness level */
#define TM1640_CMD_DISP_ON    0x88
/* Display OFF command */
#define TM1640_CMD_DISP_OFF   0x80

/* Device configs and state */

struct tm1640_config {
	struct gpio_dt_spec din;
	struct gpio_dt_spec clk;
};

struct tm1640_data {
	/* Brightness level (0-7) */
	uint8_t brightness;
	/* Is display on? */
	bool display_on;
};

/* Low-level bus protocol helpers */

/*
 * Timing delay. 1us wait is enough for all TM1640 constraints:
 * PWCLK >= 400ns, tSETUP >= 100ns, tHOLD >= 100ns, CLK gap >= 1us.
 */
static inline void tm1640_delay(void)
{
	k_busy_wait(1);
}

/*
 * Start condition: pull DIN low while CLK is high.
 */
static void tm1640_start(const struct tm1640_config *cfg)
{
	gpio_pin_set_dt(&cfg->din, 1);
	gpio_pin_set_dt(&cfg->clk, 1);
	tm1640_delay();
	gpio_pin_set_dt(&cfg->din, 0);  /* DIN HIGH -> LOW while CLK HIGH */
	tm1640_delay();
	gpio_pin_set_dt(&cfg->clk, 0);
	tm1640_delay();
}

/*
 * Stop condition: pull DIN low, raise CLK, then pull DIN high.
 */
static void tm1640_stop(const struct tm1640_config *cfg)
{
	gpio_pin_set_dt(&cfg->din, 0);
	gpio_pin_set_dt(&cfg->clk, 0);
	tm1640_delay();
	gpio_pin_set_dt(&cfg->clk, 1);
	tm1640_delay();
	gpio_pin_set_dt(&cfg->din, 1);  /* DIN LOW -> HIGH while CLK HIGH */
	tm1640_delay();
}

/*
 * Write one byte, LSB first.
 * Latches data on rising edge, leaves CLK low at the end.
 */
static void tm1640_write_byte(const struct tm1640_config *cfg, uint8_t byte)
{
	for (int i = 0; i < 8; i++) {
		gpio_pin_set_dt(&cfg->clk, 0);
		tm1640_delay();
		gpio_pin_set_dt(&cfg->din, (byte >> i) & 0x01);
		tm1640_delay();                    /* setup time */
		gpio_pin_set_dt(&cfg->clk, 1);
		tm1640_delay();                    /* clock width */
	}
	/* Leave CLK low so DIN can change without triggering start/stop conditions */
	gpio_pin_set_dt(&cfg->clk, 0);
	tm1640_delay();
}

/*
 * Send display control command (on/off and brightness).
 */
static void tm1640_send_display_ctrl(const struct tm1640_config *cfg,
				     bool on, uint8_t brightness)
{
	uint8_t cmd = on
		? (TM1640_CMD_DISP_ON | (brightness & TM1640_BRIGHTNESS_MAX))
		: TM1640_CMD_DISP_OFF;

	tm1640_start(cfg);
	tm1640_write_byte(cfg, cmd);
	tm1640_stop(cfg);
}

/* Driver API implementations */

int tm1640_display_on(const struct device *dev)
{
	const struct tm1640_config *cfg = dev->config;
	struct tm1640_data *data = dev->data;

	data->display_on = true;
	tm1640_send_display_ctrl(cfg, true, data->brightness);

	LOG_DBG("Display ON (brightness %u/7)", data->brightness);
	return 0;
}

int tm1640_display_off(const struct device *dev)
{
	const struct tm1640_config *cfg = dev->config;
	struct tm1640_data *data = dev->data;

	data->display_on = false;
	tm1640_send_display_ctrl(cfg, false, 0);

	LOG_DBG("Display OFF");
	return 0;
}

int tm1640_set_brightness(const struct device *dev, uint8_t level)
{
	const struct tm1640_config *cfg = dev->config;
	struct tm1640_data *data = dev->data;

	if (level > TM1640_BRIGHTNESS_MAX) {
		LOG_ERR("Brightness %u exceeds maximum (%u)",
			level, TM1640_BRIGHTNESS_MAX);
		return -EINVAL;
	}

	data->brightness = level;

	/* Update display settings immediately if it's already on */
	if (data->display_on) {
		tm1640_send_display_ctrl(cfg, true, data->brightness);
	}

	LOG_DBG("Brightness set to %u/7", level);
	return 0;
}

int tm1640_write(const struct device *dev, const uint8_t *buf, size_t len)
{
	const struct tm1640_config *cfg = dev->config;
	struct tm1640_data *data = dev->data;

	if (buf == NULL) {
		return -EINVAL;
	}

	if (len > TM1640_NUM_GRIDS) {
		len = TM1640_NUM_GRIDS;
	}

	/* Set write mode to auto-increment */
	tm1640_start(cfg);
	tm1640_write_byte(cfg, TM1640_CMD_DATA);
	tm1640_stop(cfg);

	/* Set starting address and write data bytes */
	tm1640_start(cfg);
	tm1640_write_byte(cfg, TM1640_CMD_ADDR_BASE);

	for (size_t i = 0; i < len; i++) {
		tm1640_write_byte(cfg, buf[i]);
	}
	/* Fill remaining grids with zeros to send full 16 bytes */
	for (size_t i = len; i < TM1640_NUM_GRIDS; i++) {
		tm1640_write_byte(cfg, 0x00);
	}

	tm1640_stop(cfg);

	/* Update display state and brightness */
	tm1640_send_display_ctrl(cfg, data->display_on, data->brightness);

	return 0;
}

int tm1640_clear(const struct device *dev)
{
	uint8_t zeros[TM1640_NUM_GRIDS] = {0};

	return tm1640_write(dev, zeros, TM1640_NUM_GRIDS);
}

/* Zephyr device initialization and driver binding */

static int tm1640_init(const struct device *dev)
{
	const struct tm1640_config *cfg = dev->config;
	struct tm1640_data *data = dev->data;
	int ret;

	/* Setup DIN GPIO */
	if (!gpio_is_ready_dt(&cfg->din)) {
		LOG_ERR("DIN GPIO device not ready");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&cfg->din, GPIO_OUTPUT_LOW);
	if (ret < 0) {
		LOG_ERR("Failed to configure DIN GPIO: %d", ret);
		return ret;
	}

	/* Setup CLK GPIO */
	if (!gpio_is_ready_dt(&cfg->clk)) {
		LOG_ERR("CLK GPIO device not ready");
		return -ENODEV;
	}
	ret = gpio_pin_configure_dt(&cfg->clk, GPIO_OUTPUT_LOW);
	if (ret < 0) {
		LOG_ERR("Failed to configure CLK GPIO: %d", ret);
		return ret;
	}

	/* Defaults: full brightness, display off to start */
	data->brightness = TM1640_BRIGHTNESS_MAX;
	data->display_on = false;

	/* Clear display memory */
	tm1640_clear(dev);

	/* Enable display */
	tm1640_display_on(dev);

	LOG_INF("TM1640 initialized (DIN pin=%u, CLK pin=%u)",
		cfg->din.pin, cfg->clk.pin);

	return 0;
}

#define TM1640_DEFINE(inst)                                                    \
	static struct tm1640_data tm1640_data_##inst;                           \
                                                                               \
	static const struct tm1640_config tm1640_config_##inst = {              \
		.din = GPIO_DT_SPEC_INST_GET(inst, din_gpios),                 \
		.clk = GPIO_DT_SPEC_INST_GET(inst, clk_gpios),                \
	};                                                                     \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst,                                            \
			      tm1640_init,                                     \
			      NULL,              /* pm */                      \
			      &tm1640_data_##inst,                             \
			      &tm1640_config_##inst,                           \
			      POST_KERNEL,                                     \
			      90,                /* init priority */           \
			      NULL);             /* api (custom, not subsys) */

DT_INST_FOREACH_STATUS_OKAY(TM1640_DEFINE)
