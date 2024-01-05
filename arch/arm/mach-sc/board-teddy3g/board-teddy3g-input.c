/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * arch/arm/mach-sc/board-teddy3g/board-teddy3g-input.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <mach/board.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <mach/kpd.h>
#include <linux/input/matrix_keypad.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c-gpio.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/gpio_event.h>
#include <linux/i2c/mms_ts.h>
#include <linux/regulator/consumer.h>
#include <mach/regulator.h>
#include <linux/regulator/machine.h>
#include "board-teddy3g.h"

#define GPIO_HOME_KEY 113 /* PIN LCD_D[5] */

#define CUSTOM_KEYPAD_ROWS          (SCI_ROW0 | SCI_ROW1)
#define CUSTOM_KEYPAD_COLS          (SCI_COL0 | SCI_COL1)
#define ROWS	(2)
#define COLS	(2)

static const unsigned int board_keymap[] = {
	KEY(0, 0, KEY_VOLUMEDOWN),
	KEY(1, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_HOME),
};

static const struct matrix_keymap_data customize_keymap = {
	.keymap = board_keymap,
	.keymap_size = ARRAY_SIZE(board_keymap),
};

static struct sci_keypad_platform_data sci_teddy3g_keypad_data = {
	.rows_choose_hw = CUSTOM_KEYPAD_ROWS,
	.cols_choose_hw = CUSTOM_KEYPAD_COLS,
	.rows_number = ROWS,
	.cols_number = COLS,
	.keymap_data = &customize_keymap,
	.support_long_key = 1,
	.repeat = 0,
	.debounce_time = 5000,
};

static struct resource sci_teddy3g_keypad_resources[] = {
	{
		.start = IRQ_KPD_INT,
		.end = IRQ_KPD_INT,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device teddy3g_keypad_device = {
	.name = "sci-keypad",
	.id             = -1,
	.num_resources = ARRAY_SIZE(sci_teddy3g_keypad_resources),
	.resource = sci_teddy3g_keypad_resources,
};

static struct gpio_keys_button gpio_buttons[] = {
	{
		.gpio           = GPIO_HOME_KEY,
		.ds_irqflags	= IRQF_TRIGGER_LOW,
		.code           = KEY_HOMEPAGE,
		.desc           = "Home Key",
		.active_low     = 1,
		.debounce_interval = 2,
	},
};

static struct gpio_keys_platform_data gpio_button_data = {
	.buttons        = gpio_buttons,
	.nbuttons       = ARRAY_SIZE(gpio_buttons),
};

static struct platform_device gpio_button_device = {
	.name           = "gpio-keys",
	.id             = -1,
	.num_resources  = 0,
	.dev            = {
		.platform_data  = &gpio_button_data,
	}
};

static struct platform_device kb_backlight_device = {
	.name           = "keyboard-backlight",
	.id             =  -1,
};

static void teddy3g_keypad_init(void)
{
	platform_device_register(&gpio_button_device);
	platform_device_register(&kb_backlight_device);
	platform_device_register(&teddy3g_keypad_device);
	platform_device_add_data(&teddy3g_keypad_device, \
			(const void *)&sci_teddy3g_keypad_data, \
			sizeof(sci_teddy3g_keypad_data));
}

const u8	mms_ts_keycode[] = {KEY_MENU, KEY_BACK};

static void mms_ts_vdd_enable(bool on)
{
	static struct regulator *ts_vdd;

	if (ts_vdd == NULL) {
		ts_vdd = regulator_get(NULL, "vddsim2");

		if (IS_ERR(ts_vdd)) {
			pr_err("Get regulator of TSP error!\n");
			return;
		}
	}

	if (on) {
		regulator_set_voltage(ts_vdd, 3000000, 3000000);
		regulator_enable(ts_vdd);
	} else if (regulator_is_enabled(ts_vdd)) {
		regulator_disable(ts_vdd);
	}
}

static void touchkey_led_vdd_enable(bool on)
{
	static int ret;

	if (ret == 0) {
		ret = gpio_request(GPIO_TOUCHKEY_LED_EN, "touchkey_led_en");
		if (ret) {
			pr_err("%s: request gpio error\n", __func__);
			return -EIO;
		}
		gpio_direction_output(GPIO_TOUCHKEY_LED_EN, 0);
		ret = 1;
	}
	gpio_set_value(GPIO_TOUCHKEY_LED_EN, on);
}

static struct mms_ts_platform_data mms_ts_info = {
	.max_x = 480,
	.max_y = 800,
	.use_touchkey = 1,
	.touchkey_keycode = mms_ts_keycode,
	.gpio_sda = 74,
	.gpio_scl = 73,
	.gpio_int = 82,
	.vdd_on = mms_ts_vdd_enable,
	.tkey_led_vdd_on = touchkey_led_vdd_enable
};

static struct i2c_board_info i2c1_boardinfo[] = {
	{
		I2C_BOARD_INFO("mms_ts", 0x48),
		.platform_data = &mms_ts_info
	},
};

static void teddy3g_tsp_init(void)
{
	i2c_register_board_info(1, i2c1_boardinfo, ARRAY_SIZE(i2c1_boardinfo));
}

void teddy3g_input_init()
{
	teddy3g_keypad_init();
	teddy3g_tsp_init();
}
