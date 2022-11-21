/* drivers/misc/sec-debug/sec-common.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/reboot.h>

#include <asm/system_info.h>

static int board_id;

static int __init board_id_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	system_rev = board_id = n;

	/* Hack: For rev 0 with U-Boot, board id is always coming 0xE10 */
	if (system_rev == 0xE10)
		system_rev = board_id = 0;

	return 1;
}
__setup("hw_revision=", board_id_setup);

int get_board_id()
{
	return board_id;
}

