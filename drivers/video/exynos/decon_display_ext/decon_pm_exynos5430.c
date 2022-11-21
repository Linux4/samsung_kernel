/* linux/drivers/video/exynos/decon_display_ext/decon_pm_exynos5430.c
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
#include <linux/clk-private.h>
#include <linux/sec_batt.h>

#include <linux/platform_device.h>
#include <mach/map.h>

#if defined(CONFIG_SOC_EXYNOS5433)
#include <mach/regs-clock-exynos5433.h>
#else
#include <mach/regs-clock-exynos5430.h>
#define DECON_VCLK_ECLK_MUX_MASKING
#endif

#include <mach/regs-pmu.h>

#include "regs-decon.h"
#include "decon_display_driver.h"
#include "decon_fb.h"
#include "decon_mipi_dsi.h"
#include "decon_dt.h"

#define TEMPORARY_RECOVER_CMU(addr, mask, bits, value) do {\
	regs = addr; \
	data = readl(regs) & ~((mask) << (bits)); \
	data |= ((value & mask) << (bits)); \
	writel(data, regs); \
	} while (0)


/* temporary helper function to clock setup */
struct clk_cmu {
	unsigned int	pa;
	void __iomem	*va;
};

struct clk_cmu disp_cmu_list_ext[] = {
	{0x10030000,	EXYNOS5430_VA_CMU_TOP},
	{0x11800000,	EXYNOS5430_VA_CMU_EGL},
	{0x11900000,	EXYNOS5430_VA_CMU_KFC},
	{0x114C0000,	EXYNOS5430_VA_CMU_AUD},
	{0x14800000,	EXYNOS5430_VA_CMU_BUS1},
	{0x13400000,	EXYNOS5430_VA_CMU_BUS2},
	{0x120D0000,	EXYNOS5430_VA_CMU_CAM0},
	{0x121C0000,	EXYNOS5430_VA_CMU_CAM0_LOCAL},
	{0x145D0000,	EXYNOS5430_VA_CMU_CAM1},
	{0x142F0000,	EXYNOS5430_VA_CMU_CAM1_LOCAL},
	{0x10FC0000,	EXYNOS5430_VA_CMU_CPIF},
	{0x13B90000,	EXYNOS5430_VA_CMU_DISP},
	{0x156E0000,	EXYNOS5430_VA_CMU_FSYS},
	{0x12460000,	EXYNOS5430_VA_CMU_G2D},
	{0x14AA0000,	EXYNOS5430_VA_CMU_G3D},
	{0x13CF0000,	EXYNOS5430_VA_CMU_GSCL},
	{0x14F80000,	EXYNOS5430_VA_CMU_HEVC},
	{0x11060000,	EXYNOS5430_VA_CMU_IMEM},
	{0x146D0000,	EXYNOS5430_VA_CMU_ISP},
	{0x14360000,	EXYNOS5430_VA_CMU_ISP_LOCAL},
	{0x15280000,	EXYNOS5430_VA_CMU_MFC0},
	{0x15380000,	EXYNOS5430_VA_CMU_MFC1},
	{0x105B0000,	EXYNOS5430_VA_CMU_MIF},
	{0x150D0000,	EXYNOS5430_VA_CMU_MSCL},
	{0x14C80000,	EXYNOS5430_VA_CMU_PERIC},
	{0x10040000,	EXYNOS5430_VA_CMU_PERIS}
};

#ifdef DECON_VCLK_ECLK_MUX_MASKING
static int decon_vclk_eclk_mux_control_ext(bool enable)
{
	int ret = 0;
	void __iomem *regs;
	u32 data = 0x00;

	if (enable)
		TEMPORARY_RECOVER_CMU(EXYNOS5430_SRC_ENABLE_DISP3, 0x11, 0, 0x11);
	else
		TEMPORARY_RECOVER_CMU(EXYNOS5430_SRC_ENABLE_DISP3, 0x11, 0, 0x0);
	return ret;
}
#endif

static void decon_clock_on_ext(struct display_driver *dispdrv)
{
#ifndef DECON_VCLK_ECLK_MUX_MASKING
	if (!dispdrv->decon_driver.clk) {
		dispdrv->decon_driver.clk = clk_get(dispdrv->display_driver, "gate_decon_tv");
		if (IS_ERR(dispdrv->decon_driver.clk)) {
			pr_err("Failed to clk_get - gate_decon_tv\n");
			return;
		}
	}
	clk_prepare(dispdrv->decon_driver.clk);
	clk_enable(dispdrv->decon_driver.clk);
#else
	decon_vclk_eclk_mux_control_ext(true);
#endif
}

static void mic_clock_on_ext(struct display_driver *dispdrv)
{
#ifdef CONFIG_DECON_MIC
	struct decon_lcd *lcd = decon_get_lcd_info_ext();

	if (!lcd->mic)
		return;

	if (!dispdrv->mic_driver.clk) {
		dispdrv->mic_driver.clk = clk_get(dispdrv->display_driver, "gate_mic1");
		if (IS_ERR(dispdrv->mic_driver.clk)) {
			pr_err("Failed to clk_get - gate_mic1\n");
			return;
		}
	}
	clk_prepare(dispdrv->mic_driver.clk);
	clk_enable(dispdrv->mic_driver.clk);
#endif
}

static void dsi_clock_on_ext(struct display_driver *dispdrv)
{
	if (!dispdrv->dsi_driver.clk) {
		dispdrv->dsi_driver.clk = clk_get(dispdrv->display_driver, "gate_dsim1");
		if (IS_ERR(dispdrv->dsi_driver.clk)) {
			pr_err("Failed to clk_get - gate_dsim1\n");
			return;
		}
	}
	clk_prepare(dispdrv->dsi_driver.clk);
	clk_enable(dispdrv->dsi_driver.clk);
}

static void decon_clock_off_ext(struct display_driver *dispdrv)
{
#ifndef DECON_VCLK_ECLK_MUX_MASKING
	clk_disable(dispdrv->decon_driver.clk);
	clk_unprepare(dispdrv->decon_driver.clk);
#else
	decon_vclk_eclk_mux_control_ext(false);
#endif
}

static void dsi_clock_off_ext(struct display_driver *dispdrv)
{
	clk_disable(dispdrv->dsi_driver.clk);
	clk_unprepare(dispdrv->dsi_driver.clk);
}

static void mic_clock_off_ext(struct display_driver *dispdrv)
{
#ifdef CONFIG_DECON_MIC
	struct decon_lcd *lcd = decon_get_lcd_info_ext();

	if (!lcd->mic)
		return;

	clk_disable(dispdrv->mic_driver.clk);
	clk_unprepare(dispdrv->mic_driver.clk);
#endif
}

static unsigned int find_cmu_ext(void __iomem *va)
{
	unsigned int i, reg;

	reg = (unsigned int)va;

	for (i = 0; i < ARRAY_SIZE(disp_cmu_list_ext); i++) {
		if ((reg & 0xfffff000) == (unsigned int)disp_cmu_list_ext[i].va)
			break;
	}

	i = (i >= ARRAY_SIZE(disp_cmu_list_ext)) ? 0 : disp_cmu_list_ext[i].pa;

	i += (reg & 0xfff);

	return i;
}

static int exynos_display_set_parent(struct device *dev, const char *child, const char *parent)
{
	struct clk *p;
	struct clk *c;
	struct clk_mux *mux;
	unsigned int val;
	int r = 0;

	p = clk_get(dev, parent);
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s: can't get clock: %s\n", __func__, parent);
		return -EINVAL;
	}

	c = clk_get(dev, child);
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: can't get clock: %s\n", __func__, child);
		return -EINVAL;
	}

	r = clk_set_parent(c, p);
	if (r < 0)
		pr_info("failed %s: %s, %s, %d, %08x\n", __func__, child, parent, r, __raw_readl(EXYNOS5430_SRC_ENABLE_DISP3));
	else {
		mux = container_of(c->hw, struct clk_mux, hw);
		val = readl(mux->reg) >> mux->shift;
		val &= mux->mask;
		pr_debug("0x%08X[%2d], %8d, 0x%08x, %30s, %30s\n",
			find_cmu_ext(mux->reg), mux->shift, val, readl(mux->reg), child, parent);
	}

	return r;
}

static int exynos_display_set_divide(struct device *dev, const char *conid, unsigned int divider)
{
	struct clk *p;
	struct clk *c;
	struct clk_divider *div;
	unsigned int val;
	unsigned long rate;
	int r = 0;

	c = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: can't get clock: %s\n", __func__, conid);
		return -EINVAL;
	}

	p = clk_get_parent(c);
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s: can't get parent clock: %s\n", __func__, conid);
		return -EINVAL;
	}

	rate = DIV_ROUND_UP(clk_get_rate(p), (divider + 1));

	r = clk_set_rate(c, rate);
	if (r < 0)
		pr_info("failed to %s, %s, %d\n", __func__, conid, r);
	else {
		div = container_of(c->hw, struct clk_divider, hw);
		val = readl(div->reg) >> div->shift;
		val &= (BIT(div->width) - 1);
		WARN_ON(divider != val);
		pr_debug("0x%08X[%2d], %8d, 0x%08x, %30s, %30s, %ld, %ld\n",
			find_cmu_ext(div->reg), div->shift, val, readl(div->reg), conid,
			__clk_get_name(p), clk_get_rate(c), clk_get_rate(p));
	}

	return r;
}

static int exynos_display_set_rate(struct device *dev, const char *conid, unsigned long rate)
{
	struct clk *p;
	struct clk *c;
	struct clk_divider *div;
	unsigned int val;
	int r = 0;

	c = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: can't get clock: %s\n", __func__, conid);
		return -EINVAL;
	}

	p = clk_get_parent(c);
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s: can't get parent clock: %s\n", __func__, conid);
		return -EINVAL;
	}

	r = clk_set_rate(c, rate);
	if (r < 0)
		pr_info("failed to %s, %s, %d\n", __func__, conid, r);
	else {
		div = container_of(c->hw, struct clk_divider, hw);
		val = readl(div->reg) >> div->shift;
		val &= (BIT(div->width) - 1);
		pr_debug("0x%08X[%2d], %8d, 0x%08x, %30s, %30s, %ld, %ld\n",
			find_cmu_ext(div->reg), div->shift, val, readl(div->reg), conid,
			__clk_get_name(p), clk_get_rate(c), clk_get_rate(p));
	}

	return r;
}

int init_display_decon_clocks_ext(struct device *dev)
{
	int ret = 0;
	struct decon_lcd *lcd = decon_get_lcd_info_ext();
	struct display_driver *dispdrv = get_display_driver_ext();

	if (lcd->xres * lcd->yres == 720 * 1280)
		exynos_display_set_rate(dev, "disp_pll", 63 * MHZ);
	else if (lcd->xres * lcd->yres == 1080 * 1920)
		exynos_display_set_rate(dev, "disp_pll", 142 * MHZ);
	else if (lcd->xres * lcd->yres == 1440 * 2560)
		exynos_display_set_rate(dev, "disp_pll", 250 * MHZ);
	else if (lcd->xres * lcd->yres == 2560 * 1600)
		exynos_display_set_rate(dev, "disp_pll", 278 * MHZ);
	else
		dev_err(dev, "%s: resolution %d:%d is missing\n", __func__, lcd->xres, lcd->yres);

	exynos_display_set_divide(dev, "dout_aclk_disp_333", 1);
	exynos_display_set_parent(dev, "mout_aclk_disp_333_a", "mout_mfc_pll_div2");
	exynos_display_set_parent(dev, "mout_aclk_disp_333_b", "mout_aclk_disp_333_a");
	exynos_display_set_divide(dev, "dout_pclk_disp", 2);
	exynos_display_set_parent(dev, "mout_aclk_disp_333_user", "aclk_disp_333");

	if (lcd->xres * lcd->yres == 720 * 1280) {
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_eclk", 11);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_c", "mout_sclk_decon_tv_eclk_b");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_b", "mout_sclk_decon_tv_eclk_a");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_a", "mout_bus_pll_div2");
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_eclk_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk", "mout_sclk_decon_tv_eclk_user");
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_vclk", 0);
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_vclk_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_c_disp", "mout_sclk_decon_tv_vclk_b_disp");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_b_disp", "mout_sclk_decon_tv_vclk_a_disp");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_a_disp", "mout_disp_pll");
		exynos_display_set_parent(dev, "mout_disp_pll", "disp_pll");
		exynos_display_set_divide(dev, "dout_sclk_dsim1", 11);
		exynos_display_set_parent(dev, "mout_sclk_dsim1_c", "mout_sclk_dsim1_b");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_b", "mout_sclk_dsim1_a");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_a", "mout_bus_pll_div2");
		exynos_display_set_divide(dev, "dout_sclk_dsim1_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_dsim1_b_disp", "mout_sclk_dsim1_user");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_user", "sclk_dsim1_disp");
	} else if (lcd->xres * lcd->yres == 1080 * 1920) {
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_eclk", 4);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_c", "mout_sclk_decon_tv_eclk_b");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_b", "mout_sclk_decon_tv_eclk_a");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_a", "mout_bus_pll_div2");
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_eclk_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk", "mout_sclk_decon_tv_eclk_user");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_user", "sclk_decon_tv_eclk_disp");
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_vclk", 0);
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_vclk_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_c_disp", "mout_sclk_decon_tv_vclk_b_disp");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_b_disp", "mout_sclk_decon_tv_vclk_a_disp");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_a_disp", "mout_disp_pll");
		exynos_display_set_parent(dev, "mout_disp_pll", "disp_pll");
		exynos_display_set_divide(dev, "dout_sclk_dsim1", 4);
		exynos_display_set_parent(dev, "mout_sclk_dsim1_c", "mout_sclk_dsim1_b");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_b", "mout_sclk_dsim1_a");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_a", "mout_bus_pll_div2");
		exynos_display_set_divide(dev, "dout_sclk_dsim1_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_dsim1_b_disp", "mout_sclk_dsim1_user");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_user", "sclk_dsim1_disp");
	} else if (lcd->xres * lcd->yres == 1440 * 2560) {
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_eclk", 2);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_c", "mout_sclk_decon_tv_eclk_b");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_b", "mout_sclk_decon_tv_eclk_a");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_a", "mout_bus_pll_div2");
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_eclk_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk", "mout_sclk_decon_tv_eclk_user");
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_vclk", 0);
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_vclk_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_c_disp", "mout_sclk_decon_tv_vclk_b_disp");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_b_disp", "mout_sclk_decon_tv_vclk_a_disp");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_a_disp", "mout_disp_pll");
		exynos_display_set_parent(dev, "mout_disp_pll", "disp_pll");
		exynos_display_set_divide(dev, "dout_sclk_dsim1", 2);
		exynos_display_set_parent(dev, "mout_sclk_dsim1_c", "mout_sclk_dsim1_b");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_b", "mout_sclk_dsim1_a");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_a", "mout_bus_pll_div2");
		exynos_display_set_divide(dev, "dout_sclk_dsim1_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_dsim1_b_disp", "mout_sclk_dsim1_user");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_user", "sclk_dsim1_disp");
	} else if (lcd->xres * lcd->yres == 2560 * 1600) {
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_eclk", 1);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_c", "mout_sclk_decon_tv_eclk_b");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_b", "mout_sclk_decon_tv_eclk_a");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk_a", "mout_bus_pll_div2");
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_eclk_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_eclk", "mout_sclk_decon_tv_eclk_user");
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_vclk", 0);
		exynos_display_set_divide(dev, "dout_sclk_decon_tv_vclk_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_c_disp", "mout_sclk_decon_tv_vclk_b_disp");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_b_disp", "mout_sclk_decon_tv_vclk_a_disp");
		exynos_display_set_parent(dev, "mout_sclk_decon_tv_vclk_a_disp", "mout_disp_pll");
		exynos_display_set_parent(dev, "mout_disp_pll", "disp_pll");
		exynos_display_set_divide(dev, "dout_sclk_dsim1", 1);
		exynos_display_set_parent(dev, "mout_sclk_dsim1_c", "mout_sclk_dsim1_b");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_b", "mout_sclk_dsim1_a");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_a", "mout_bus_pll_div2");
		exynos_display_set_divide(dev, "dout_sclk_dsim1_disp", 0);
		exynos_display_set_parent(dev, "mout_sclk_dsim1_b_disp", "mout_sclk_dsim1_user");
		exynos_display_set_parent(dev, "mout_sclk_dsim1_user", "sclk_dsim1_disp");
	} else
		dev_err(dev, "%s: resolution %d:%d is missing\n", __func__, lcd->xres, lcd->yres);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY2_CLOCK_GATING
	dispdrv->pm_status.ops->clk_on(dispdrv);
#else
	dsi_clock_on_ext(dispdrv);
	mic_clock_on_ext(dispdrv);
	decon_clock_on_ext(dispdrv);
#endif

	return ret;
}

int init_display_dsi_clocks_ext(struct device *dev)
{
	int ret = 0;

	exynos_display_set_parent(dev, "mout_phyclk_mipidphy1_rxclkesc0_user", "phyclk_mipidphy1_rxclkesc0_phy");
	exynos_display_set_parent(dev, "mout_phyclk_mipidphy1_bitclkdiv8_user", "phyclk_mipidphy1_bitclkdiv8_phy");

	return ret;
}

int enable_display_decon_clocks_ext(struct device *dev)
{
	return 0;
}

int disable_display_decon_ext_clocks(struct device *dev)
{
	struct display_driver *dispdrv;
	dispdrv = get_display_driver_ext();
#ifdef CONFIG_FB_HIBERNATION_DISPLAY2_CLOCK_GATING
	dispdrv->pm_status.ops->clk_off(dispdrv);
#else
	dsi_clock_off_ext(dispdrv);
	mic_clock_off_ext(dispdrv);
	decon_clock_off_ext(dispdrv);
#endif

	return 0;
}

/* should be moved to dt file */
static struct regulator_bulk_data *supplies;
static unsigned int num_supplies;

static void get_supplies(struct device *dev)
{
	int ret = 0;
	unsigned int i;

	num_supplies = of_property_count_strings(dev->of_node, "display,regulator-names");

	if (!num_supplies)
		return;

	supplies = kzalloc(num_supplies * sizeof(struct regulator_bulk_data), GFP_KERNEL);

	for (i = 0; i < num_supplies; i++) {
		of_property_read_string_index(dev->of_node, "display,regulator-names", i, &supplies[i].supply);
		pr_info("%s: %s\n", __func__, supplies[i].supply);
	}

	if (num_supplies) {
		ret = regulator_bulk_get(NULL, num_supplies, supplies);
		if (ret < 0)
			pr_err("%s: failed to get regulators: %d\n", __func__, ret);
	}
}

static struct pinctrl		*pins;
static struct pinctrl_state	*state_default;
#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
static struct pinctrl_state	*state_turnon_tes;
static struct pinctrl_state	*state_turnoff_tes;
#endif

static void get_pintctrl(struct device *dev)
{
	if (!pins) {
		pins = devm_pinctrl_get(dev);
		if (!pins)
			pr_err("%s: failed to get pinctrl\n", __func__);
	}

	if (!state_default) {
		state_default = pinctrl_lookup_state(pins, "default");
		if (!state_default)
			pr_err("%s: failed to get default pinctrl\n", __func__);
	}

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
	if (!state_turnon_tes) {
		state_turnon_tes = pinctrl_lookup_state(pins, "turnon_tes");
		if (!state_turnon_tes)
			pr_err("%s: failed to get default pinctrl\n", __func__);
	}

	if (!state_turnoff_tes) {
		state_turnoff_tes = pinctrl_lookup_state(pins, "turnoff_tes");
		if (!state_turnoff_tes)
			pr_err("%s: failed to get default pinctrl\n", __func__);
	}
#endif
}

static int set_pinctrl(struct device *dev, int enable)
{
	int ret = 0;

	if (enable) {
		if (!pins)
			get_pintctrl(dev);

		if (!IS_ERR(pins) && !IS_ERR(state_default)) {
			ret = pinctrl_select_state(pins, state_default);
			if (ret) {
				pr_err("%s: failed to select state for default", __func__);
				return ret;
			}
		}

#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
		if (!IS_ERR(pins) && !IS_ERR(state_turnon_tes)) {
			ret = pinctrl_select_state(pins, state_turnon_tes);
			if (ret) {
				pr_err("%s: failed to select state for turn on tes", __func__);
				return ret;
			}
		}
#endif
	} else {
#if defined(CONFIG_FB_I80_COMMAND_MODE) && !defined(CONFIG_FB_I80_SW_TRIGGER)
		if (!IS_ERR(pins) && !IS_ERR(state_turnoff_tes)) {
			ret = pinctrl_select_state(pins, state_turnoff_tes);
			if (ret) {
				pr_err("%s: failed to select state for turn off tes", __func__);
				return ret;
			}
		}
#endif
	}

	return ret;
}

int enable_display_driver_power_ext(struct device *dev)
{
#if !defined(CONFIG_MACH_XYREF5430) && !defined(CONFIG_MACH_ESPRESSO5433)
	struct display_driver *dispdrv = get_display_driver_ext();
	struct display_gpio *gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio_ext();
	int ret = 0, i;

	if (!num_supplies)
		get_supplies(dev);

	if (!num_supplies)
		goto pin_config;

	/* Turn on VDDI(VDD3): 1.8 */
	ret = regulator_enable(supplies[0].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
		goto pin_config;
	}

	if (gpio->num > 2) {
		/* 1. Turn on VDDR(VDDI_REG): 1.5 or 1.6 */
		/* 2. Turn on VCI: 3.0 */
		for (i = 1; i < gpio->num; i ++) {
			ret = gpio_request_one(gpio->id[i], GPIOF_OUT_INIT_HIGH, "lcd_power");
			if (ret < 0) {
				pr_err("Failed to get gpio number for the lcd power(%d)\n", i);
				return ret;
			}
			gpio_free(gpio->id[i]);
		}
	}
	else {
		/* Turn on VDDR(VDDI_REG): 1.5 or 1.6 */
		ret = gpio_request_one(gpio->id[1], GPIOF_OUT_INIT_HIGH, "lcd_power");
		if (ret < 0) {
			pr_err("%s: gpio lcd_power request fail\n", __func__);
			goto pin_config;
		}
		gpio_free(gpio->id[1]);

		/* Turn on VCI: 3.0 */
		ret = regulator_enable(supplies[1].consumer);
		if (ret < 0) {
			pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
			goto pin_config;
		}
	}
	/* Wait 10ms */
	usleep_range(10000, 11000);
#endif

pin_config:
	set_pinctrl(dev, 1);

	return 0;
}

int disable_display_driver_power_ext(struct device *dev)
{
	struct display_driver *dispdrv;
	struct display_gpio *gpio;
	int ret, i;

	dispdrv = get_display_driver_ext();
	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio_ext();

#ifdef CONFIG_LCD_ALPM
	if (dispdrv->dsi_driver.dsim->lcd_alpm)
		return 0;
#endif

	ret = gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_LOW, "lcd_reset");
	if (ret < 0) {
		pr_err("Failed to get gpio number for the lcd_reset\n");
		return -EINVAL;
	}
	gpio_free(gpio->id[0]);

	if (!num_supplies)
		get_supplies(dev);

	if (!num_supplies)
		goto pin_config;

	if (gpio->num > 2) {
		/* 1. Turn off VCI: 3.0 */
		/* 2. Turn on VDDR(VDDI_REG): 1.5 or 1.6 */
		for (i = gpio->num - 1; i > 0; i --) {
			ret = gpio_request_one(gpio->id[i], GPIOF_OUT_INIT_LOW, "lcd_power");
			if (ret < 0) {
				pr_err("Failed to get gpio number for the lcd power(%d)\n", i);
				return -EINVAL;
			}
			gpio_free(gpio->id[i]);
		}
	}
	else {
		/* Turn off VCI: 3.0 */
		ret = regulator_disable(supplies[1].consumer);
		if (ret < 0) {
			pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
			goto pin_config;
		}

#if !defined(CONFIG_MACH_XYREF5430) && !defined(CONFIG_MACH_ESPRESSO5433)
		/* Turn on VDDR(VDDI_REG): 1.5 or 1.6 */
		ret = gpio_request_one(gpio->id[1], GPIOF_OUT_INIT_LOW, "lcd_power");
		if (ret < 0) {
			pr_err("%s: gpio lcd_power request fail\n", __func__);
			goto pin_config;
		}
		gpio_free(gpio->id[1]);
#endif
	}
	/* Turn off VDDI(VDD3): 1.8 */
	ret = regulator_disable(supplies[0].consumer);
	if (ret < 0) {
		pr_err("%s: regulator %s enable fail\n", __func__, supplies[0].supply);
		goto pin_config;
	}

pin_config:
	set_pinctrl(dev, 0);

	return 0;
}

int reset_display_driver_panel_ext(struct device *dev)
{
	struct display_driver *dispdrv;
	struct display_gpio *gpio;

	dispdrv = get_display_driver_ext();

#ifdef CONFIG_LCD_ALPM
	if (dispdrv->dsi_driver.dsim->lcd_alpm)
		return 0;
#endif

	gpio = dispdrv->dt_ops.get_display_dsi_reset_gpio_ext();
	gpio_request_one(gpio->id[0], GPIOF_OUT_INIT_HIGH, "lcd_reset");
	usleep_range(5000, 6000);
	gpio_set_value(gpio->id[0], 0);
	usleep_range(5000, 6000);
	gpio_set_value(gpio->id[0], 1);
	gpio_free(gpio->id[0]);

	/* Wait 10ms */
	usleep_range(10000, 11000);

	return 0;
}

int enable_display_decon_runtimepm_ext(struct device *dev)
{
	return 0;
}

int disable_display_decon_runtimepm_ext(struct device *dev)
{
	return 0;
}

int enable_display_dsd_clocks_ext(struct device *dev, bool enable)
{
	struct display_driver *dispdrv;
	dispdrv = get_display_driver_ext();

	if (!dispdrv->decon_driver.dsd_clk) {
		dispdrv->decon_driver.dsd_clk = clk_get(dev, "gate_dsd");
		if (IS_ERR(dispdrv->decon_driver.dsd_clk)) {
			pr_err("Failed to clk_get - gate_dsd\n");
			return -EBUSY;
		}
	}
#if defined(CONFIG_SOC_EXYNOS5433)
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

int disable_display_dsd_clocks_ext(struct device *dev, bool enable)
{
	struct display_driver *dispdrv;
	dispdrv = get_display_driver_ext();

	if (!dispdrv->decon_driver.dsd_clk)
		return -EBUSY;

	clk_disable_unprepare(dispdrv->decon_driver.dsd_clk);
#if defined(CONFIG_SOC_EXYNOS5433)
	if (dispdrv->decon_driver.gate_dsd_clk)
		clk_disable_unprepare(dispdrv->decon_driver.gate_dsd_clk);
#endif
	return 0;
}

#if 0 //def CONFIG_FB_HIBERNATION_DISPLAY2
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

	return (readl(sfb->regs + VIDCON1) >> VIDCON1_LINECNT_SHIFT);
}
#endif

#ifdef CONFIG_FB_HIBERNATION_DISPLAY2
void set_default_hibernation_mode(struct display_driver *dispdrv)
{
	bool clock_gating = false;
	bool power_gating = false;
	bool hotplug_gating = false;

	if (lpcharge)
		pr_info ("%s: off, Low power charging mode: %d\n", __func__, lpcharge);
	else {
#ifdef CONFIG_FB_HIBERNATION_DISPLAY2_CLOCK_GATING
		clock_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY2_POWER_GATING
		power_gating = true;
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY2_POWER_GATING_DEEPSTOP
		hotplug_gating = true;
#endif
	}
	dispdrv->pm_status.clock_gating_on = clock_gating;
	dispdrv->pm_status.power_gating_on = power_gating;
	dispdrv->pm_status.hotplug_gating_on = hotplug_gating;
}

struct pm_ops decon_pm_ops = {
	.clk_on		= decon_clock_on_ext,
	.clk_off	= decon_clock_off_ext,
};
#ifdef CONFIG_DECON_MIC
struct pm_ops mic_pm_ops = {
	.clk_on		= mic_clock_on_ext,
	.clk_off	= mic_clock_off_ext,
};
#endif
struct pm_ops dsi_pm_ops = {
	.clk_on		= dsi_clock_on_ext,
	.clk_off	= dsi_clock_off_ext,
};
#else
int disp_pm_runtime_get_sync_ext(struct display_driver *dispdrv)
{
	pm_runtime_get_sync(dispdrv->display_driver);
	return 0;
}

int disp_pm_runtime_put_sync_ext(struct display_driver *dispdrv)
{
	pm_runtime_put_sync(dispdrv->display_driver);
	return 0;
}
#endif

