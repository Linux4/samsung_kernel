/* linux/drivers/video/decon_display/fimd_pm_exynos.c
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
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#if defined(CONFIG_FB_EXYNOS_FIMD_MC) || defined(CONFIG_FB_EXYNOS_FIMD_MC_WB)
#include <media/v4l2-subdev.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-device.h>
#include <media/exynos_mc.h>
#include <plat/map-base.h>
#endif

#include <linux/platform_device.h>
#include "decon_display_driver.h"
#include "fimd_fb.h"
#include <mach/map.h>
#include <plat/regs-fb.h>
#include <plat/dsim.h>
#include <plat/clock.h>

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
static int get_clk_cnt(struct clk *clk)
{
	return clk->usage;
}

bool check_camera_is_running(void)
{
	/* CAM1 STATUS */
	if (readl(S5P_VA_PMU + 0x3C04) & 0x1)
		return true;
	else
		return false;
}

bool get_display_power_status(void)
{
	/* DISP_STATUS */
	if (readl(S5P_VA_PMU + 0x3C84) & 0x1)
		return true;
	else
		return false;
}

int get_display_line_count(struct display_driver *dispdrv)
{
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	/* FIXME */
	return 0;
	return (readl(sfb->regs + VIDCON1) >> VIDCON1_LINECNT_SHIFT);
}

void set_default_hibernation_mode(struct display_driver *dispdrv)
{
	bool clock_gating = false;
	bool power_gating = false;
	bool hotplug_gating = false;

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	clock_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING
	power_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING_DEEPSTOP
	hotplug_gating = true;
#endif
	dispdrv->pm_status.clock_gating_on = clock_gating;
	dispdrv->pm_status.power_gating_on = power_gating;
	dispdrv->pm_status.hotplug_gating_on = hotplug_gating;
}

void fimd_clock_on(struct display_driver *dispdrv)
{
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	if (!sfb->bus_clk) {
		pr_err("%s: Failed to clk_control\n", __func__);
		return;
	}

	clk_enable(sfb->bus_clk);

	/*TODO: Check FIMD H/W version */
	clk_enable(sfb->axi_disp1);

	if (!sfb->variant.has_clksel)
		clk_enable(sfb->lcd_clk);
	pr_debug("%s: clock_count = %d\n", __func__, get_clk_cnt(sfb->lcd_clk));
	pr_debug("%s: clock_count = %d\n", __func__, get_clk_cnt(sfb->axi_disp1));
	pr_debug("%s: clock_count = %d\n", __func__, get_clk_cnt(sfb->bus_clk));
}

void mic_clock_on(struct display_driver *dispdrv)
{
	return;
}

void dsi_clock_on(struct display_driver *dispdrv)
{
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;
	if (!dsim->clock) {
		pr_err("%s: Failed to clk_control\n", __func__);
		return;
	}
	clk_enable(dsim->clock);
	pr_debug("%s: clock_count = %d\n", __func__, get_clk_cnt(dsim->clock));
}

void fimd_clock_off(struct display_driver *dispdrv)
{
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	if (!sfb->bus_clk) {
		pr_err("%s: Failed to clk_control\n", __func__);
		return;
	}

	if (!sfb->variant.has_clksel)
		clk_disable(sfb->lcd_clk);

	/*TODO: Check FIMD H/W version */
	clk_disable(sfb->axi_disp1);

	clk_disable(sfb->bus_clk);
	pr_debug("%s: clock_count = %d\n", __func__, get_clk_cnt(sfb->lcd_clk));
	pr_debug("%s: clock_count = %d\n", __func__, get_clk_cnt(sfb->axi_disp1));
	pr_debug("%s: clock_count = %d\n", __func__, get_clk_cnt(sfb->bus_clk));
}

void dsi_clock_off(struct display_driver *dispdrv)
{
	struct mipi_dsim_device *dsim = dispdrv->dsi_driver.dsim;
	if (!dsim->clock) {
		pr_err("%s: Failed to clk_control\n", __func__);
		return;
	}
	clk_disable(dsim->clock);
	pr_debug("%s: clock_count = %d\n", __func__, get_clk_cnt(dsim->clock));
}

void mic_clock_off(struct display_driver *dispdrv)
{
}

struct pm_ops decon_pm_ops = {
	.clk_on		= fimd_clock_on,
	.clk_off	= fimd_clock_off,
};
#ifdef CONFIG_DECON_MIC
struct pm_ops mic_pm_ops = {
	.clk_on		= mic_clock_on,
	.clk_off	= mic_clock_off,
};
#endif
struct pm_ops dsi_pm_ops = {
	.clk_on		= dsi_clock_on,
	.clk_off	= dsi_clock_off,
};

#else
int disp_pm_runtime_get_sync(struct display_driver *dispdrv)
{
	pm_runtime_get_sync(dispdrv->display_driver);
	return 0;
}

int disp_pm_runtime_put_sync(struct display_driver *dispdrv)
{
	pm_runtime_put_sync(dispdrv->display_driver);
	return 0;
}
#endif
