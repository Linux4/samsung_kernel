/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-huskytd/board-huskytd-battery.c
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
#include <linux/gpio.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/board.h>

static struct resource sprd_battery_resources[] = {
		[0] = {
				.start = EIC_CHARGER_DETECT,
				.end = EIC_CHARGER_DETECT,
				.name = "charger_detect",
				.flags = IORESOURCE_IO,
		},
		[1] = {
				.start = EIC_CHG_CV_STATE,
				.end = EIC_CHG_CV_STATE,
				.name = "chg_cv_state",
				.flags = IORESOURCE_IO,
		},
		[2] = {
				.start = EIC_VCHG_OVI,
				.end = EIC_VCHG_OVI,
				.name = "vchg_ovi",
				.flags = IORESOURCE_IO,
		},

};

static struct platform_device sprd_battery_device = {
		.name           = "sprd-battery",
		.id             =  0,
		.num_resources  = ARRAY_SIZE(sprd_battery_resources),
		.resource       = sprd_battery_resources,
};

void huskytd_battery_init(void)
{
	platform_device_register(&sprd_battery_device);
}
