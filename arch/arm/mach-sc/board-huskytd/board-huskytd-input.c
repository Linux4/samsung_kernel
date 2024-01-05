/* arch/arm/mach-sc/board-huskytd/board-huskytd-input.c
 *
 * Copyright (C) 2013 Samsung Electronics Co, Ltd.
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
#include <linux/i2c/focaltech.h>
#include <mach/kpd.h>
#include <linux/input/matrix_keypad.h>
#include <mach/irqs.h>

#include "board-huskytd.h"

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

static struct sci_keypad_platform_data sprd_keypad_data = {
	.rows_choose_hw = CUSTOM_KEYPAD_ROWS,
	.cols_choose_hw = CUSTOM_KEYPAD_COLS,
	.rows_number = ROWS,
	.cols_number = COLS,
	.keymap_data = &customize_keymap,
	.support_long_key = 1,
	.repeat = 0,
	.debounce_time = 5000,
};

static struct resource sprd_keypad_resources[] = {
	{
		.start = IRQ_KPD_INT,
		.end = IRQ_KPD_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device sprd_keypad_device = {
	.name = "sci-keypad",
	.id             = -1,
	.num_resources = ARRAY_SIZE(sprd_keypad_resources),
	.resource = sprd_keypad_resources,
};

static void huskytd_keypad_init(void)
{
	platform_device_add_data(&sprd_keypad_device, \
					(const void *)&sprd_keypad_data, \
					sizeof(sprd_keypad_data));
	platform_device_register(&sprd_keypad_device);
}

static struct ft5x0x_ts_platform_data ft5x0x_ts_info = {
	.irq_gpio_number	= GPIO_TOUCH_IRQ,
	.reset_gpio_number	= GPIO_TOUCH_RESET,
	.vdd_name			= "vdd28",
	.virtualkeys = {
			100, 1020, 80, 65,
			280, 1020, 80, 65,
			470, 1020, 80, 65,
			},
	 .TP_MAX_X = 720,
	 .TP_MAX_Y = 1280,
};

static struct i2c_board_info i2c1_boardinfo[] = {
	{
		I2C_BOARD_INFO(FOCALTECH_TS_NAME, FOCALTECH_TS_ADDR),
		.platform_data = &ft5x0x_ts_info,
	},
};

static void huskytd_tsp_init(void)
{
	i2c_register_board_info(1, i2c1_boardinfo, ARRAY_SIZE(i2c1_boardinfo));
}

void huskytd_input_init()
{
	huskytd_tsp_init();
	huskytd_keypad_init();
}
