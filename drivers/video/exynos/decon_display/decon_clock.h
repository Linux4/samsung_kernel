/* linux/drivers/video/exynos/decon_display/decon_board.c
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DECON_DISPLAY_CLOCK_HEADER__
#define __DECON_DISPLAY_CLOCK_HEADER__

#include <linux/device.h>

#if defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS5430)
/* this clk eum value is DIFFERENT with clk-exynos543x.c */
enum disp_clks {
	disp_pll,
	mout_aclk_disp_333_a,
	mout_aclk_disp_333_b,
	dout_pclk_disp,
	mout_aclk_disp_333_user,
	dout_sclk_dsd,
	mout_sclk_dsd_c,
	mout_sclk_dsd_b,
	mout_sclk_dsd_a,
	mout_sclk_dsd_user,
	dout_sclk_decon_eclk,
	mout_sclk_decon_eclk_c,
	mout_sclk_decon_eclk_b,
	mout_sclk_decon_eclk_a,
	dout_sclk_decon_eclk_disp,
	mout_sclk_decon_eclk,
	mout_sclk_decon_eclk_user,
	dout_sclk_decon_vclk,
	mout_sclk_decon_vclk_c,
	mout_sclk_decon_vclk_b,
	mout_sclk_decon_vclk_a,
	dout_sclk_decon_vclk_disp,
	mout_sclk_decon_vclk,
	mout_disp_pll,
	dout_sclk_dsim0,
	mout_sclk_dsim0_c,
	mout_sclk_dsim0_b,
	mout_sclk_dsim0_a,
	dout_sclk_dsim0_disp,
	mout_sclk_dsim0,
	mout_sclk_dsim0_user,
	mout_phyclk_mipidphy_rxclkesc0_user,
	mout_phyclk_mipidphy_bitclkdiv8_user,
	disp_clks_max,
};
#else
#error __FILE__ "clock enum is missing"
#endif

int exynos_display_set_rate(struct device *dev, enum disp_clks idx, unsigned long rate);
int exynos_display_set_parent(struct device *dev, enum disp_clks idx, const char *parent);
int exynos_display_set_divide(struct device *dev, enum disp_clks idx, unsigned int divider);

#endif
