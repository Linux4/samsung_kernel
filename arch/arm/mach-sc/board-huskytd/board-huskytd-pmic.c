/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-huskytd/board-huskytd-pmic.c
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
#include "board-huskytd.h"


static struct platform_device regulator_device = {
	.name = "sc2713-regulator",
	.id = -1,
};

void huskytd_pmic_init(void)
{
	platform_device_register(&regulator_device);
}
