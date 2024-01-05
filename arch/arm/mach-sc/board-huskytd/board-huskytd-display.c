/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-huskytd/board-huskytd-display.c
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

#include <mach/hardware.h>
#include <mach/irqs.h>

static struct resource sprd_lcd_resources[] = {
	[0] = {
		.start = SPRD_LCDC_BASE,
		.end = SPRD_LCDC_BASE + SPRD_LCDC_SIZE - 1,
		.name = "lcd_res",
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_DISPC0_INT,
		.end = IRQ_DISPC0_INT,
		.flags = IORESOURCE_IRQ,
	},

	[2] = {
		.start = IRQ_DISPC1_INT,
		.end = IRQ_DISPC1_INT,
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device sprd_lcd0_device = {
	.name           = "sprd_fb",
	.id             =  0,
	.num_resources  = ARRAY_SIZE(sprd_lcd_resources),
	.resource       = sprd_lcd_resources,
};

/*
 * If the backlight is driven by PWM, config the PWM info
 * else driven by PWMD, the PWM index is 3
 */
static struct resource sprd_bl_resource[] = {
	[0] = {
		.start	= 3,
		.end	= 3,
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device sprd_backlight_device = {
	.name           = "sprd_backlight",
	.id             =  -1,
	.num_resources	= ARRAY_SIZE(sprd_bl_resource),
	.resource	= sprd_bl_resource,
};

void huskytd_display_init(void)
{
	platform_device_register(&sprd_lcd0_device);
	platform_device_register(&sprd_backlight_device);
}
