/* linux/drivers/video/exynos/decon_display/decon_clock.c
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/clk-private.h>

#if defined(CONFIG_SOC_EXYNOS5433)
#include <mach/regs-clock-exynos5433.h>
#else
#include <mach/regs-clock-exynos5430.h>
#endif

#include "decon_clock.h"

struct clk_list_t {
	struct clk *c;
	struct clk *p;
	const char *c_name;
	const char *p_name;
};

#if defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS5430)
static struct clk_list_t clk_list[disp_clks_max] = {
	{ .c_name = "disp_pll", },
	{ .c_name = "mout_aclk_disp_333_a", },
	{ .c_name = "mout_aclk_disp_333_b", },
	{ .c_name = "dout_pclk_disp", },
	{ .c_name = "mout_aclk_disp_333_user", },
	{ .c_name = "dout_sclk_dsd", },
	{ .c_name = "mout_sclk_dsd_c", },
	{ .c_name = "mout_sclk_dsd_b", },
	{ .c_name = "mout_sclk_dsd_a", },
	{ .c_name = "mout_sclk_dsd_user", },
	{ .c_name = "dout_sclk_decon_eclk", },
	{ .c_name = "mout_sclk_decon_eclk_c", },
	{ .c_name = "mout_sclk_decon_eclk_b", },
	{ .c_name = "mout_sclk_decon_eclk_a", },
	{ .c_name = "dout_sclk_decon_eclk_disp", },
	{ .c_name = "mout_sclk_decon_eclk", },
	{ .c_name = "mout_sclk_decon_eclk_user", },
	{ .c_name = "dout_sclk_decon_vclk", },
	{ .c_name = "mout_sclk_decon_vclk_c", },
	{ .c_name = "mout_sclk_decon_vclk_b", },
	{ .c_name = "mout_sclk_decon_vclk_a", },
	{ .c_name = "dout_sclk_decon_vclk_disp", },
	{ .c_name = "mout_sclk_decon_vclk", },
	{ .c_name = "mout_disp_pll", },
	{ .c_name = "dout_sclk_dsim0", },
	{ .c_name = "mout_sclk_dsim0_c", },
	{ .c_name = "mout_sclk_dsim0_b", },
	{ .c_name = "mout_sclk_dsim0_a", },
	{ .c_name = "dout_sclk_dsim0_disp", },
	{ .c_name = "mout_sclk_dsim0", },
	{ .c_name = "mout_sclk_dsim0_user", },
	{ .c_name = "mout_phyclk_mipidphy_rxclkesc0_user", },
	{ .c_name = "mout_phyclk_mipidphy_bitclkdiv8_user", },
};
#else
#error __FILE__ "clock list is missing"
#endif

#if defined(CONFIG_DISP_CLK_DEBUG)
struct cmu_list_t {
	unsigned int	pa;
	void __iomem	*va;
};

#define EXYNOS543X_CMU_LIST(name)	\
	{EXYNOS5430_PA_CMU_##name,	EXYNOS5430_VA_CMU_##name}

struct cmu_list_t cmu_list[] = {
	EXYNOS543X_CMU_LIST(TOP),
	EXYNOS543X_CMU_LIST(EGL),
	EXYNOS543X_CMU_LIST(KFC),
	EXYNOS543X_CMU_LIST(AUD),
	EXYNOS543X_CMU_LIST(BUS1),
	EXYNOS543X_CMU_LIST(BUS2),
	EXYNOS543X_CMU_LIST(CAM0),
	EXYNOS543X_CMU_LIST(CAM0_LOCAL),
	EXYNOS543X_CMU_LIST(CAM1),
	EXYNOS543X_CMU_LIST(CAM1_LOCAL),
	EXYNOS543X_CMU_LIST(CPIF),
	EXYNOS543X_CMU_LIST(DISP),
	EXYNOS543X_CMU_LIST(FSYS),
	EXYNOS543X_CMU_LIST(G2D),
	EXYNOS543X_CMU_LIST(G3D),
	EXYNOS543X_CMU_LIST(GSCL),
	EXYNOS543X_CMU_LIST(HEVC),
	EXYNOS543X_CMU_LIST(IMEM),
	EXYNOS543X_CMU_LIST(ISP),
	EXYNOS543X_CMU_LIST(ISP_LOCAL),
	EXYNOS543X_CMU_LIST(MFC0),
	EXYNOS543X_CMU_LIST(MFC1),
	EXYNOS543X_CMU_LIST(MIF),
	EXYNOS543X_CMU_LIST(MSCL),
	EXYNOS543X_CMU_LIST(PERIC),
	EXYNOS543X_CMU_LIST(PERIS),
};

unsigned int get_pa(void __iomem *va)
{
	unsigned int i, reg;

	reg = (unsigned int)va;

	for (i = 0; i < ARRAY_SIZE(cmu_list); i++) {
		if ((reg & 0xfffff000) == (unsigned int)cmu_list[i].va)
			break;
	}

	i = (i >= ARRAY_SIZE(cmu_list)) ? 0 : cmu_list[i].pa;

	i += (reg & 0xfff);

	return i;
}
#endif

static int exynos_display_clk_get(struct device *dev, enum disp_clks idx, const char *parent)
{
	struct clk *p;
	struct clk *c;
	int ret = 0;
	const char *conid = clk_list[idx].c_name;

	if (IS_ERR_OR_NULL(clk_list[idx].c)) {
		c = clk_get(dev, conid);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("%s: can't get clock: %s\n", __func__, conid);
			return -EINVAL;
		} else
			clk_list[idx].c = c;
	}

	if (IS_ERR_OR_NULL(parent)) {
		if (IS_ERR_OR_NULL(clk_list[idx].p)) {
			p = clk_get_parent(clk_list[idx].c);
			if (IS_ERR_OR_NULL(p)) {
				pr_err("%s: can't get clock parent: %s\n", __func__, conid);
				return -EINVAL;
			} else
				clk_list[idx].p = p;
		}
	} else {
		if (IS_ERR_OR_NULL(clk_list[idx].p)) {
			p = clk_get(dev, parent);
			if (IS_ERR_OR_NULL(p)) {
				pr_err("%s: can't get clock: %s\n", __func__, parent);
				return -EINVAL;
			} else
				clk_list[idx].p = p;
		}
	}

	return ret;
}

int exynos_display_set_parent(struct device *dev, enum disp_clks idx, const char *parent)
{
	struct clk *p;
	struct clk *c;
	int ret = 0;
	const char *conid = clk_list[idx].c_name;

	if (unlikely(IS_ERR_OR_NULL(clk_list[idx].c))) {
		ret = exynos_display_clk_get(dev, idx, parent);
		if (ret < 0) {
			pr_err("%s: can't get clock: %s\n", __func__, conid);
			return ret;
		}
	}

	p = clk_list[idx].p;
	c = clk_list[idx].c;

	ret = clk_set_parent(c, p);
	if (ret < 0)
		pr_info("failed %s: %s, %s, %d\n", __func__, conid, parent, ret);
#if defined(CONFIG_DISP_CLK_DEBUG)
	else {
		struct clk_mux *mux = container_of(c->hw, struct clk_mux, hw);
		unsigned int val = readl(mux->reg) >> mux->shift;
		val &= mux->mask;
		pr_info("%08X[%2d], %8d, 0x%08x, %30s, %30s\n",
			get_pa(mux->reg), mux->shift, val, readl(mux->reg), conid, parent);
	}
#endif

	return ret;
}

int exynos_display_set_divide(struct device *dev, enum disp_clks idx, unsigned int divider)
{
	struct clk *p;
	struct clk *c;
	unsigned long rate;
	int ret = 0;
	const char *conid = clk_list[idx].c_name;

	if (unlikely(IS_ERR_OR_NULL(clk_list[idx].c))) {
		ret = exynos_display_clk_get(dev, idx, NULL);
		if (ret < 0) {
			pr_err("%s: can't get clock: %s\n", __func__, conid);
			return ret;
		}
	}

	p = clk_list[idx].p;
	c = clk_list[idx].c;

	rate = DIV_ROUND_UP(clk_get_rate(p), (divider + 1));

	ret = clk_set_rate(c, rate);
	if (ret < 0)
		pr_info("failed to %s, %s, %d\n", __func__, conid, ret);
#if defined(CONFIG_DISP_CLK_DEBUG)
	else {
		struct clk_divider *div = container_of(c->hw, struct clk_divider, hw);
		unsigned int val = readl(div->reg) >> div->shift;
		val &= (BIT(div->width) - 1);
		WARN_ON(divider != val);
		pr_info("%08X[%2d], %8d, 0x%08x, %30s, %30s\n",
			get_pa(div->reg), div->shift, val, readl(div->reg), conid, __clk_get_name(p));
	}
#endif

	return ret;
}

int exynos_display_set_rate(struct device *dev, enum disp_clks idx, unsigned long rate)
{
	struct clk *p;
	struct clk *c;
	int ret = 0;
	const char *conid = clk_list[idx].c_name;

	if (unlikely(IS_ERR_OR_NULL(clk_list[idx].c))) {
		ret = exynos_display_clk_get(dev, idx, NULL);
		if (ret < 0) {
			pr_err("%s: can't get clock: %s\n", __func__, conid);
			return ret;
		}
	}

	p = clk_list[idx].p;
	c = clk_list[idx].c;

	ret = clk_set_rate(c, rate);
	if (ret < 0)
		pr_info("failed to %s, %s, %d\n", __func__, conid, ret);
#if defined(CONFIG_DISP_CLK_DEBUG)
	else {
		struct clk_divider *div = container_of(c->hw, struct clk_divider, hw);
		pr_info("%08X[%2s], %8s, 0x%08x, %30s, %30s\n",
			get_pa(div->reg), "", "", readl(div->reg), conid, __clk_get_name(p));
	}
#endif

	return ret;
}


