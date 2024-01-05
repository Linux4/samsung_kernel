/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-huskytd/board-teddy3g-pmic.c
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


#include <linux/platform_device.h>

extern int __init sci_regulator_init(void);


void teddy3g_pmic_init()
{
	sci_regulator_init();
}
