/* linux/drivers/video/decon_display/decon_display_driver.c
 *
 * Copyright (c) 2013 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/fs.h>
#include <linux/fb.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/lcd.h>
#include <linux/gpio.h>

#if defined(CONFIG_FB_EXYNOS_FIMD_MC) || defined(CONFIG_FB_EXYNOS_FIMD_MC_WB)
#include <media/v4l2-subdev.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/exynos_mc.h>
#include <plat/map-base.h>
#endif

#include "decon_display_driver.h"
#include "decon_pm.h"

#include "fimd_fb.h"

static struct display_driver g_display_driver;

int dsi_display_driver_init(struct platform_device *pdev)
{
	int ret = 0;


#ifdef CONFIG_FB_HIBERNATION_DISPLAY
	/* FIXME: If dsi_driver probe are faster more then fimd_driver */
	init_display_pm(&g_display_driver);
	/* Clocks are enabled by each driver during boot-time */
	g_display_driver.pm_status.clock_enabled = 1;
#endif

	/* IMPORTANT: MIPI-DSI component should be 1'st created. */
	ret = create_mipi_dsi_controller(pdev);
	if (ret < 0) {
		pr_err("display error: mipi-dsi controller create failed.");
		return ret;
	}

	return ret;
}

int decon_display_driver_init(struct platform_device *pdev)
{
	int ret = 0;

	ret = create_decon_display_controller(pdev);
	if (ret < 0) {
		pr_err("display error: display controller create failed.");
		return ret;
	}

	return ret;
}

/* get_display_driver - for returning reference of display
 * driver context */
struct display_driver *get_display_driver(void)
{
	return &g_display_driver;
}

MODULE_DESCRIPTION("Samusung DECON-DISP driver");
MODULE_LICENSE("GPL");
