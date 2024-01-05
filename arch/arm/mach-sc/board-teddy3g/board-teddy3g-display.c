/* Copyright (C) 2013 Samsung Electronics Co, Ltd.
 *
 * Based on mach-sc/board-teddy3g/board-teddy3g-display.c
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

static struct resource sprd_teddy3g_lcd_resources[] = {
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

struct platform_device sprd_teddy3g_lcd_device0 = {
        .name           = "sprd_fb",
        .id             =  0,
        .num_resources  = ARRAY_SIZE(sprd_teddy3g_lcd_resources),
        .resource       = sprd_teddy3g_lcd_resources,
};

struct platform_device sprd_teddy3g_backlight_device = {
        .name           = "sprd_backlight",
        .id             =  -1,
};

void teddy3g_display_init(void)
{
        if(platform_device_register(&sprd_teddy3g_lcd_device0) < 0)
		pr_err("unable to register LCD device");

        if(platform_device_register(&sprd_teddy3g_backlight_device) < 0)
		pr_err("unable to register back light device");
}

