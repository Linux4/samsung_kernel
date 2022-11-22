/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 * contact <steve.zhan@spreadtrum.com>
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

#ifndef __ARCH_KPD_H
#define __ARCH_KPD_H 

/**
	For example, if you using chip row0 and (col0,col1) to define some keys
	pls set :
		rows_choose_hw = row0
		cols_choose_hw = col0 | col1
*/
struct sci_keypad_platform_data {
	int rows_choose_hw;	/* choose chip keypad controler rows */
	int cols_choose_hw;	/* choose chip keypad controler cols */
	int rows_number; /*How many rows are there in board. */
	int cols_number; /*How many cols are there in board. */
	const struct matrix_keymap_data *keymap_data;
	int support_long_key;
	unsigned short repeat;
	unsigned int debounce_time;	/* in ns */
	int wakeup;
};

/* chip define begin */
#define SCI_COL7	(0x01 << 15)
#define SCI_COL6	(0x01 << 14)
#define SCI_COL5	(0x01 << 13)
#define SCI_COL4	(0x01 << 12)
#define SCI_COL3	(0x01 << 11)
#define SCI_COL2	(0x01 << 10)
#define SCI_COL1	(0x01 << 9)
#define SCI_COL0	(0x01 << 8)

#define SCI_ROW7	(0x01 << 23)
#define SCI_ROW6	(0x01 << 22)
#define SCI_ROW5	(0x01 << 21)
#define SCI_ROW4	(0x01 << 20)
#define SCI_ROW3	(0x01 << 19)
#define SCI_ROW2	(0x01 << 18)
#define SCI_ROW1	(0x01 << 17)
#define SCI_ROW0	(0x01 << 16)

/*using example */
/*
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

static struct sci_keypad_platform_data sci_keypad_data = {
	.rows_choose_hw = CUSTOM_KEYPAD_ROWS,
	.cols_choose_hw = CUSTOM_KEYPAD_COLS,
	.rows_number = ROWS,
	.cols_number = COLS,
	.keymap_data = &customize_keymap,
	.support_long_key = 1,
	.repeat = 0,
	.debounce_time = 5000,
};
platform_device_add_data(&sprd_keypad_device,(const void*)&sci_keypad_data,sizeof(sci_keypad_data));
platform_add_devices(sprd_keypad_device, sizeof(sprd_keypad_device));
*/


#endif
