/*
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
#include <linux/sec-common.h>

static int board_id;
static int recovery_mode;

struct class *sec_class;
EXPORT_SYMBOL(sec_class);
unsigned int system_rev;
EXPORT_SYMBOL(system_rev);

static int __init board_id_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	system_rev = board_id = n;
	board_id = n;

	return 1;
}
__setup("board_id=", board_id_setup);

int get_board_id()
{
	return board_id;
}

static int __init recovery_mode_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;
	recovery_mode = n;
	return 1;
}
__setup("recovery_mode=", recovery_mode_setup);

int get_recoverymode()
{
	return recovery_mode;
}

void sec_common_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	if (IS_ERR(sec_class))
		pr_err("Class(sec) Creating Fail!!!\n");
}
