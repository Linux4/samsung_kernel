/* linux/drivers/video/decon_display/decon_pm_exynos5430.c
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
#include <linux/of.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/sec_batt.h>
#include <linux/platform_device.h>

#include <mach/map.h>

#include <mach/regs-pmu.h>

#include "regs-decon.h"
#include "decon_display_driver.h"
#include "decon_fb.h"
#include "decon_mipi_dsi.h"
#include "decon_dt.h"
#include "decon_clock.h"
#include "decon_board.h"


int init_display_decon_clocks(struct device *dev)
{
	int ret = 0;
	struct decon_lcd *lcd = decon_get_lcd_info();
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	struct display_driver *dispdrv = get_display_driver();
#endif

	if (lcd->xres * lcd->yres == 720 * 1280)
		exynos_display_set_rate(dev, disp_pll, 67 * 1000000);
	else if (lcd->xres * lcd->yres == 1080 * 1920)
		exynos_display_set_rate(dev, disp_pll, 142 * 1000000);
	else if (lcd->xres * lcd->yres == 1536 * 2048)
		exynos_display_set_rate(dev, disp_pll, 214 * 1000000);
	else if (lcd->xres * lcd->yres == 1440 * 2560)
		exynos_display_set_rate(dev, disp_pll, 250 * 1000000);
	else if (lcd->xres * lcd->yres == 2560 * 1600)
		exynos_display_set_rate(dev, disp_pll, 278 * 1000000);
	else
		dev_err(dev, "%s: resolution %d:%d is missing\n", __func__, lcd->xres, lcd->yres);

	exynos_display_set_parent(dev, mout_aclk_disp_333_a, "mout_mfc_pll_div2");
	exynos_display_set_parent(dev, mout_aclk_disp_333_b, "mout_aclk_disp_333_a");
	exynos_display_set_divide(dev, dout_pclk_disp, 2);
	exynos_display_set_parent(dev, mout_aclk_disp_333_user, "aclk_disp_333");

	exynos_display_set_divide(dev, dout_sclk_dsd, 1);
	exynos_display_set_parent(dev, mout_sclk_dsd_c, "mout_sclk_dsd_b");
	exynos_display_set_parent(dev, mout_sclk_dsd_b, "mout_sclk_dsd_a");
	exynos_display_set_parent(dev, mout_sclk_dsd_a, "mout_mfc_pll_div2");
	exynos_display_set_parent(dev, mout_sclk_dsd_user, "sclk_dsd_disp");

	if (lcd->xres * lcd->yres == 720 * 1280) {
		exynos_display_set_divide(dev, dout_sclk_decon_eclk, 10);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_c, "mout_sclk_decon_eclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_b, "mout_sclk_decon_eclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_decon_eclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk, "mout_sclk_decon_eclk_user");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_user, "sclk_decon_eclk_disp");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_c, "mout_sclk_decon_vclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_b, "mout_sclk_decon_vclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk, "mout_disp_pll");
		exynos_display_set_parent(dev, mout_disp_pll, "disp_pll");
		exynos_display_set_divide(dev, dout_sclk_dsim0, 10);
		exynos_display_set_parent(dev, mout_sclk_dsim0_c, "mout_sclk_dsim0_b");
		exynos_display_set_parent(dev, mout_sclk_dsim0_b, "mout_sclk_dsim0_a");
		exynos_display_set_parent(dev, mout_sclk_dsim0_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_dsim0_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_dsim0, "mout_sclk_dsim0_user");
		exynos_display_set_parent(dev, mout_sclk_dsim0_user, "sclk_dsim0_disp");
	} else if (lcd->xres * lcd->yres == 1080 * 1920) {
		exynos_display_set_divide(dev, dout_sclk_decon_eclk, 4);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_c, "mout_sclk_decon_eclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_b, "mout_sclk_decon_eclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_decon_eclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk, "mout_sclk_decon_eclk_user");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_user, "sclk_decon_eclk_disp");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_c, "mout_sclk_decon_vclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_b, "mout_sclk_decon_vclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk, "mout_disp_pll");
		exynos_display_set_parent(dev, mout_disp_pll, "disp_pll");
		exynos_display_set_divide(dev, dout_sclk_dsim0, 4);
		exynos_display_set_parent(dev, mout_sclk_dsim0_c, "mout_sclk_dsim0_b");
		exynos_display_set_parent(dev, mout_sclk_dsim0_b, "mout_sclk_dsim0_a");
		exynos_display_set_parent(dev, mout_sclk_dsim0_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_dsim0_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_dsim0, "mout_sclk_dsim0_user");
		exynos_display_set_parent(dev, mout_sclk_dsim0_user, "sclk_dsim0_disp");
	} else if (lcd->xres * lcd->yres == 1440 * 2560 || lcd->xres * lcd->yres == 1536 * 2048) {
		exynos_display_set_divide(dev, dout_sclk_decon_eclk, 2);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_c, "mout_sclk_decon_eclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_b, "mout_sclk_decon_eclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_decon_eclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk, "mout_sclk_decon_eclk_user");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_user, "sclk_decon_eclk_disp");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_c, "mout_sclk_decon_vclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_b, "mout_sclk_decon_vclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk, "mout_disp_pll");
		exynos_display_set_parent(dev, mout_disp_pll, "disp_pll");
		exynos_display_set_divide(dev, dout_sclk_dsim0, 2);
		exynos_display_set_parent(dev, mout_sclk_dsim0_c, "mout_sclk_dsim0_b");
		exynos_display_set_parent(dev, mout_sclk_dsim0_b, "mout_sclk_dsim0_a");
		exynos_display_set_parent(dev, mout_sclk_dsim0_a, "mout_bus_pll_div2");
		exynos_display_set_divide(dev, dout_sclk_dsim0_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_dsim0, "mout_sclk_dsim0_user");
		exynos_display_set_parent(dev, mout_sclk_dsim0_user, "sclk_dsim0_disp");
	} else if (lcd->xres * lcd->yres == 2560 * 1600) {
		exynos_display_set_divide(dev, dout_sclk_decon_eclk, 1);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_c, "mout_sclk_decon_eclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_b, "mout_mfc_pll_div2");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_eclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_eclk, "mout_sclk_decon_eclk_user");
		exynos_display_set_parent(dev, mout_sclk_decon_eclk_user, "sclk_decon_eclk_disp");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_c, "mout_sclk_decon_vclk_b");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_b, "mout_sclk_decon_vclk_a");
		exynos_display_set_parent(dev, mout_sclk_decon_vclk_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_decon_vclk_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_decon_vclk, "mout_disp_pll");
		exynos_display_set_parent(dev, mout_disp_pll, "disp_pll");
		exynos_display_set_divide(dev, dout_sclk_dsim0, 1);
		exynos_display_set_parent(dev, mout_sclk_dsim0_c, "mout_sclk_dsim0_b");
		exynos_display_set_parent(dev, mout_sclk_dsim0_b, "mout_mfc_pll_div2");
		exynos_display_set_parent(dev, mout_sclk_dsim0_a, "oscclk");
		exynos_display_set_divide(dev, dout_sclk_dsim0_disp, 0);
		exynos_display_set_parent(dev, mout_sclk_dsim0, "mout_sclk_dsim0_user");
		exynos_display_set_parent(dev, mout_sclk_dsim0_user, "sclk_dsim0_disp");
	} else
		dev_err(dev, "%s: resolution %d:%d is missing\n", __func__, lcd->xres, lcd->yres);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	dispdrv->pm_status.ops->clk_on(dispdrv);
#endif

	return ret;
}

int init_display_dsi_clocks(struct device *dev)
{
	int ret = 0;

	exynos_display_set_parent(dev, mout_phyclk_mipidphy_rxclkesc0_user, "phyclk_mipidphy_rxclkesc0_phy");
	exynos_display_set_parent(dev, mout_phyclk_mipidphy_bitclkdiv8_user, "phyclk_mipidphy_bitclkdiv8_phy");

	return ret;
}

int enable_display_decon_clocks(struct device *dev)
{
	return 0;
}

int disable_display_decon_clocks(struct device *dev)
{
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	dispdrv->pm_status.ops->clk_off(dispdrv);
#endif

	return 0;
}

int enable_display_driver_power(struct device *dev)
{
#ifdef CONFIG_LCD_ALPM
	struct display_driver *dispdrv = get_display_driver();

	if (dispdrv->dsi_driver.dsim->lcd_alpm)
		return 0;
#endif
	run_list(dev, __func__);

	return 0;
}

int disable_display_driver_power(struct device *dev)
{
#ifdef CONFIG_LCD_ALPM
	struct display_driver *dispdrv = get_display_driver();

	if (dispdrv->dsi_driver.dsim->lcd_alpm)
		return 0;
#endif
	run_list(dev, __func__);

	return 0;
}

#if defined(CONFIG_DECON_LCD_S6TNMR7)
static int wait_tcon_rdy(struct device *dev)
{
	int timeout = 10;
	int gpio;

	gpio = of_get_named_gpio(dev->of_node, "gpio-tcon-rdy", 0);
	if (gpio < 0) {
		dev_err(dev, "failed to get proper gpio number\n");
		return 0;
	}

	do {
		if (gpio_get_value(gpio))
			break;
		msleep(30);
	} while (timeout--);
	if (timeout < 0)
		pr_err("%s timeout...\n", __func__);
	else
		pr_info("%s duration: %d\n", __func__, (10 - timeout) * 30);
	return 0;
}
#elif defined(CONFIG_DECON_LCD_ANA38401)
static int wait_tcon_rdy(struct device *dev)
{
	int timeout = 30;
	int gpio, ret = 0;
	static int retry_count = 1;

	gpio = of_get_named_gpio(dev->of_node, "gpio-tcon-rdy", 0);
	if (gpio < 0) {
		dev_err(dev, "failed to get proper gpio number\n");
		return ret;
	}

	while (timeout) {
		if (gpio_get_value(gpio))
			break;
		usleep_range(10000, 11000);
		timeout--;
	}

	pr_info("%s time is %d: %d\n", __func__, (30 - timeout) * 10, retry_count);

	if (timeout <= 22 && timeout >= 0) {
		ret = retry_count ? -ETIMEDOUT : 0;
		retry_count = !retry_count;
	} else {
		ret = 0;
		retry_count = 1;
	}

	return ret;
}
#endif

int reset_display_driver_panel(struct device *dev)
{
	int ret = 0;
#ifdef CONFIG_LCD_ALPM
	struct display_driver *dispdrv = get_display_driver();

	if (dispdrv->dsi_driver.dsim->lcd_alpm)
		return 0;

#endif

	run_list(dev, __func__);

#if defined(CONFIG_DECON_LCD_S6TNMR7) || defined(CONFIG_DECON_LCD_ANA38401)
	if (of_find_property(dev->of_node, "gpio-tcon-rdy", NULL))
		ret = wait_tcon_rdy(dev);
#endif

	return ret;
}

int enable_display_decon_runtimepm(struct device *dev)
{
	return 0;
}

int disable_display_decon_runtimepm(struct device *dev)
{
	return 0;
}

int enable_display_dsd_clocks(struct device *dev, bool enable)
{
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	if (!dispdrv->decon_driver.dsd_clk) {
		dispdrv->decon_driver.dsd_clk = clk_get(dev, "gate_dsd");
		if (IS_ERR(dispdrv->decon_driver.dsd_clk)) {
			pr_err("Failed to clk_get - gate_dsd\n");
			return -EBUSY;
		}
	}
#if defined(CONFIG_SOC_EXYNOS5433) && defined(CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING)
	if (!dispdrv->decon_driver.gate_dsd_clk) {
		dispdrv->decon_driver.gate_dsd_clk = clk_get(dev, "gate_dsd_clk");
		if (IS_ERR(dispdrv->decon_driver.gate_dsd_clk)) {
			pr_err("Failed to clk_get - gate_dsd_clk\n");
			return -EBUSY;
		}
	}
	clk_prepare_enable(dispdrv->decon_driver.gate_dsd_clk);
#endif
	clk_prepare_enable(dispdrv->decon_driver.dsd_clk);
	return 0;
}

int disable_display_dsd_clocks(struct device *dev, bool enable)
{
	struct display_driver *dispdrv;
	dispdrv = get_display_driver();

	if (!dispdrv->decon_driver.dsd_clk)
		return -EBUSY;

	clk_disable_unprepare(dispdrv->decon_driver.dsd_clk);
#if defined(CONFIG_SOC_EXYNOS5433) && defined(CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING)
	if (dispdrv->decon_driver.gate_dsd_clk)
		clk_disable_unprepare(dispdrv->decon_driver.gate_dsd_clk);
#endif
	return 0;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
bool check_camera_is_running(void)
{
#if defined(CONFIG_SOC_EXYNOS5433)
	if (readl(EXYNOS5433_CAM0_STATUS) & 0x1)
#else
	if (readl(EXYNOS5430_CAM0_STATUS) & 0x1)
#endif
		return true;
	else
		return false;
}

bool get_display_power_status(void)
{
#if defined(CONFIG_SOC_EXYNOS5433)
	if (readl(EXYNOS5433_DISP_STATUS) & 0x1)
#else
	if (readl(EXYNOS5430_DISP_STATUS) & 0x1)
#endif
		return true;
	else
		return false;
}

void set_hw_trigger_mask(struct s3c_fb *sfb, bool mask)
{
	unsigned int val;

	val = readl(sfb->regs + TRIGCON);
	if (mask)
		val &= ~(TRIGCON_HWTRIGMASK_I80_RGB);
	else
		val |= (TRIGCON_HWTRIGMASK_I80_RGB);

	writel(val, sfb->regs + TRIGCON);
}

int get_display_line_count(struct display_driver *dispdrv)
{
	struct s3c_fb *sfb = dispdrv->decon_driver.sfb;

	return readl(sfb->regs + VIDCON1) >> VIDCON1_LINECNT_SHIFT;
}
#endif

#ifdef CONFIG_FB_HIBERNATION_DISPLAY
void set_default_hibernation_mode(struct display_driver *dispdrv)
{
	bool clock_gating = false;
	bool power_gating = false;
	bool hotplug_gating = false;

	if (lpcharge)
		pr_info("%s: off, Low power charging mode: %d\n", __func__, lpcharge);
	else {
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_CLOCK_GATING
		clock_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING
		power_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY_POWER_GATING_DEEPSTOP
		hotplug_gating = true;
#endif
	}
	dispdrv->pm_status.clock_gating_on = clock_gating;
	dispdrv->pm_status.power_gating_on = power_gating;
	dispdrv->pm_status.hotplug_gating_on = hotplug_gating;
}

void decon_clock_on(struct display_driver *dispdrv)
{
#if !defined(CONFIG_SOC_EXYNOS5430)
	int ret = 0;

	if (!dispdrv->decon_driver.clk) {
		dispdrv->decon_driver.clk = clk_get(dispdrv->display_driver, "gate_decon");
		if (IS_ERR(dispdrv->decon_driver.clk)) {
			pr_err("Failed to clk_get - gate_decon\n");
			return;
		}
	}
	clk_prepare(dispdrv->decon_driver.clk);
	ret = clk_enable(dispdrv->decon_driver.clk);
#endif
}

void mic_clock_on(struct display_driver *dispdrv)
{
#ifdef CONFIG_DECON_MIC
	struct decon_lcd *lcd = decon_get_lcd_info();
	int ret = 0;

	if (!lcd->mic)
		return;

	if (!dispdrv->mic_driver.clk) {
		dispdrv->mic_driver.clk = clk_get(dispdrv->display_driver, "gate_mic");
		if (IS_ERR(dispdrv->mic_driver.clk)) {
			pr_err("Failed to clk_get - gate_mic\n");
			return;
		}
	}
	clk_prepare(dispdrv->mic_driver.clk);
	ret = clk_enable(dispdrv->mic_driver.clk);
#endif
}

void dsi_clock_on(struct display_driver *dispdrv)
{
	int ret = 0;

	if (!dispdrv->dsi_driver.clk) {
		dispdrv->dsi_driver.clk = clk_get(dispdrv->display_driver, "gate_dsim0");
		if (IS_ERR(dispdrv->dsi_driver.clk)) {
			pr_err("Failed to clk_get - gate_dsi\n");
			return;
		}
	}
	clk_prepare(dispdrv->dsi_driver.clk);
	ret = clk_enable(dispdrv->dsi_driver.clk);
}

void decon_clock_off(struct display_driver *dispdrv)
{
#if !defined(CONFIG_SOC_EXYNOS5430)
	clk_disable(dispdrv->decon_driver.clk);
	clk_unprepare(dispdrv->decon_driver.clk);
#endif
}

void dsi_clock_off(struct display_driver *dispdrv)
{
	clk_disable(dispdrv->dsi_driver.clk);
	clk_unprepare(dispdrv->dsi_driver.clk);
}

void mic_clock_off(struct display_driver *dispdrv)
{
#ifdef CONFIG_DECON_MIC
	struct decon_lcd *lcd = decon_get_lcd_info();

	if (!lcd->mic)
		return;

	clk_disable(dispdrv->mic_driver.clk);
	clk_unprepare(dispdrv->mic_driver.clk);
#endif
}

struct pm_ops decon_pm_ops = {
	.clk_on		= decon_clock_on,
	.clk_off	= decon_clock_off,
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

