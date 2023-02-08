/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS5260 - PLL support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/cpu.h>
#include "board-xyref5260.h"
#include <mach/setup-disp-clock.h>

#ifdef CONFIG_SOC_EXYNOS5260
int s3c_fb_clock_init(void)
{
	struct clk *mout_mediatop_bpll, *mout_cpll;
	struct clk *aclk_disp_333, *aclk_disp_222;
	struct clk *sclk_disp_pixel_user, *sclk_disp_pixel;
	struct clk *aclk_disp_333_nogate, *aclk_disp_222_nogate;
	struct clk *sclk_disp_128_extclkpll;

	mout_mediatop_bpll = clk_get(NULL, "sclk_mediatop_pll");
	mout_cpll = clk_get(NULL, "sclk_cpll");
	aclk_disp_333 = clk_get(NULL, "aclk_disp_333");
	aclk_disp_222 = clk_get(NULL, "aclk_disp_222");
	sclk_disp_pixel_user = clk_get(NULL, "sclk_disp_pixel_user");
	sclk_disp_pixel = clk_get(NULL, "sclk_disp_pixel");
	aclk_disp_333_nogate = clk_get(NULL, "aclk_disp_333_nogate");
	aclk_disp_222_nogate = clk_get(NULL, "aclk_disp_222_nogate");
	sclk_disp_128_extclkpll = clk_get(NULL, "sclk_fimd1_128_extclkpl");

	if (clk_set_parent(mout_mediatop_bpll, mout_cpll)) {
		clk_put(mout_cpll);
		clk_put(mout_mediatop_bpll);

		pr_err("Unable to set parent %s of clock %s.\n",
				mout_cpll->name, mout_mediatop_bpll->name);
		return -EINVAL;
	}

	if (clk_set_parent(aclk_disp_333, mout_mediatop_bpll)) {
		clk_put(mout_mediatop_bpll);
		clk_put(aclk_disp_333);

		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mediatop_bpll->name, aclk_disp_333->name);
		return -EINVAL;
	}

	if (clk_set_parent(aclk_disp_222, mout_mediatop_bpll)) {
		clk_put(mout_mediatop_bpll);
		clk_put(aclk_disp_222);

		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mediatop_bpll->name, aclk_disp_222->name);
		return -EINVAL;
	}

	if (clk_set_parent(sclk_disp_pixel_user, sclk_disp_pixel)) {
		clk_put(sclk_disp_pixel);
		clk_put(sclk_disp_pixel_user);

		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_disp_pixel->name, sclk_disp_pixel_user->name);
		return -EINVAL;
	}

	if (clk_set_parent(sclk_disp_128_extclkpll, sclk_disp_pixel_user)) {
		clk_put(sclk_disp_pixel_user);
		clk_put(sclk_disp_128_extclkpll);

		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_disp_pixel_user->name, sclk_disp_128_extclkpll->name);
		return -EINVAL;
	}

#ifdef CONFIG_S5P_DP
	clk_set_rate(sclk_disp_128_extclkpll, 267 * MHZ);
#endif
#ifdef CONFIG_FB_MIPI_DSIM
	clk_set_rate(sclk_disp_128_extclkpll, 266 * MHZ);
#endif
	if (clk_set_parent(aclk_disp_333_nogate, aclk_disp_333)) {
		clk_put(aclk_disp_333);
		clk_put(aclk_disp_333_nogate);

		pr_err("Unable to set parent %s of clock %s.\n",
				aclk_disp_333->name, aclk_disp_333_nogate->name);
		return -EINVAL;
	}

	if (clk_set_parent(aclk_disp_222_nogate, aclk_disp_222)) {
		clk_put(aclk_disp_222);
		clk_put(aclk_disp_222_nogate);

		pr_err("Unable to set parent %s of clock %s.\n",
				aclk_disp_222->name, aclk_disp_222_nogate->name);
		return -EINVAL;
	}

	clk_put(mout_mediatop_bpll);
	clk_put(mout_cpll);
	clk_put(aclk_disp_333);
	clk_put(aclk_disp_222);
	clk_put(sclk_disp_pixel_user);
	clk_put(sclk_disp_pixel);
	clk_put(aclk_disp_333_nogate);
	clk_put(aclk_disp_222_nogate);
	clk_put(sclk_disp_128_extclkpll);

	return 0;
}

int mipi_dsi_clock_init(void)
{
	struct clk *phyclk_dsim1_rxclkesc0, *phyclk_mipi_rxclkesc0;
	struct clk *sclk_mipi_txclkescclk, *sclk_dsim1_txclkescclk;
	struct clk *phyclk_dsim1_bitclkdiv8, *phyclk_mipi_txbyteclkhs;
	struct clk *sclk_mipi_txclkesc0, *sclk_dsim1_txclkesc0;
	struct clk *sclk_mipi_txclkesc1, *sclk_dsim1_txclkesc1;
	struct clk *sclk_mipi_txclkesc2, *sclk_dsim1_txclkesc2;
	struct clk *sclk_mipi_txclkesc3, *sclk_dsim1_txclkesc3;

	phyclk_dsim1_rxclkesc0 = clk_get(NULL, "phyclk_dsim1_rxclkesc0");
	phyclk_mipi_rxclkesc0 = clk_get(NULL, "phyclk_mipi_dphy_4l_m_rxclkesc0");
	sclk_mipi_txclkescclk = clk_get(NULL, "sclk_mipi_dphy_4l_m_txclkescclk");
	sclk_dsim1_txclkescclk = clk_get(NULL, "sclk_dsim1_txclkescclk");
	phyclk_dsim1_bitclkdiv8 = clk_get(NULL, "phyclk_dsim1_bitclkdiv8");
	phyclk_mipi_txbyteclkhs = clk_get(NULL, "phyclk_mipi_dphy_4l_m_txbyteclkhs");
	sclk_mipi_txclkesc0 = clk_get(NULL, "sclk_mipi_dphy_4l_m_txclkesc0");
	sclk_dsim1_txclkesc0 = clk_get(NULL, "sclk_dsim1_txclkesc0");
	sclk_mipi_txclkesc1 = clk_get(NULL, "sclk_mipi_dphy_4l_m_txclkesc1");
	sclk_dsim1_txclkesc1 = clk_get(NULL, "sclk_dsim1_txclkesc1");
	sclk_mipi_txclkesc2 = clk_get(NULL, "sclk_mipi_dphy_4l_m_txclkesc2");
	sclk_dsim1_txclkesc2 = clk_get(NULL, "sclk_dsim1_txclkesc2");
	sclk_mipi_txclkesc3 = clk_get(NULL, "sclk_mipi_dphy_4l_m_txclkesc3");
	sclk_dsim1_txclkesc3 = clk_get(NULL, "sclk_dsim1_txclkesc3");

	if (clk_set_parent(phyclk_dsim1_rxclkesc0, phyclk_mipi_rxclkesc0)) {
		clk_put(phyclk_mipi_rxclkesc0);
		clk_put(phyclk_dsim1_rxclkesc0);

		pr_err("Unable to set parent %s of clock %s.\n",
				phyclk_dsim1_rxclkesc0->name, phyclk_mipi_rxclkesc0->name);
		return -EINVAL;
	}

	if (clk_set_parent(sclk_mipi_txclkescclk, sclk_dsim1_txclkescclk)) {
		clk_put(sclk_dsim1_txclkescclk);
		clk_put(sclk_mipi_txclkescclk);

		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mipi_txclkescclk->name, sclk_dsim1_txclkescclk->name);
		return -EINVAL;
	}

	if (clk_set_parent(phyclk_dsim1_bitclkdiv8, phyclk_mipi_txbyteclkhs)) {
		clk_put(phyclk_mipi_txbyteclkhs);
		clk_put(phyclk_dsim1_bitclkdiv8);

		pr_err("Unable to set parent %s of clock %s.\n",
				phyclk_dsim1_bitclkdiv8->name, phyclk_mipi_txbyteclkhs->name);
		return -EINVAL;
	}

	if (clk_set_parent(sclk_mipi_txclkesc0, sclk_dsim1_txclkesc0)) {
		clk_put(sclk_dsim1_txclkesc0);
		clk_put(sclk_mipi_txclkesc0);

		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mipi_txclkesc0->name, sclk_dsim1_txclkesc0->name);
		return -EINVAL;
	}

	if (clk_set_parent(sclk_mipi_txclkesc1, sclk_dsim1_txclkesc1)) {
		clk_put(sclk_dsim1_txclkesc1);
		clk_put(sclk_mipi_txclkesc1);

		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mipi_txclkesc1->name, sclk_dsim1_txclkesc1->name);
		return -EINVAL;
	}

	if (clk_set_parent(sclk_mipi_txclkesc2, sclk_dsim1_txclkesc2)) {
		clk_put(sclk_dsim1_txclkesc2);
		clk_put(sclk_mipi_txclkesc2);

		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mipi_txclkesc2->name, sclk_dsim1_txclkesc2->name);
		return -EINVAL;
	}

	if (clk_set_parent(sclk_mipi_txclkesc3, sclk_dsim1_txclkesc3)) {
		clk_put(sclk_dsim1_txclkesc3);
		clk_put(sclk_mipi_txclkesc3);

		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mipi_txclkesc3->name, sclk_dsim1_txclkesc3->name);
		return -EINVAL;
	}

	clk_put(phyclk_dsim1_rxclkesc0);
	clk_put(phyclk_mipi_rxclkesc0);
	clk_put(sclk_mipi_txclkescclk);
	clk_put(sclk_dsim1_txclkescclk);
	clk_put(phyclk_dsim1_bitclkdiv8);
	clk_put(phyclk_mipi_txbyteclkhs);
	clk_put(sclk_mipi_txclkesc0);
	clk_put(sclk_dsim1_txclkesc0);
	clk_put(sclk_mipi_txclkesc1);
	clk_put(sclk_dsim1_txclkesc1);
	clk_put(sclk_mipi_txclkesc2);
	clk_put(sclk_dsim1_txclkesc2);
	clk_put(sclk_mipi_txclkesc3);
	clk_put(sclk_dsim1_txclkesc3);
	return 0;
}

int s5p_dp_clock_init(void)
{
	struct clk *phyclk_dptx_clk_div2, *phyclk_dptx_phy_div2;
	struct clk *phyclk_dptx_ch0, *phyclk_dptx_phy_ch0;
	struct clk *phyclk_dptx_ch1, *phyclk_dptx_phy_ch1;
	struct clk *phyclk_dptx_ch2, *phyclk_dptx_phy_ch2;
	struct clk *phyclk_dptx_ch3, *phyclk_dptx_phy_ch3;
	struct clk *phyclk_dptx_clk_24m, *phyclk_dptx_phy_24m;

	phyclk_dptx_clk_div2 = clk_get(NULL, "phyclk_dptx_link_i_clk_div2");
	phyclk_dptx_phy_div2 = clk_get(NULL, "phyclk_dptx_phy_clk_div2");
	phyclk_dptx_ch0 = clk_get(NULL, "phyclk_dptx_link_i_ch0_txd_clk");
	phyclk_dptx_phy_ch0 = clk_get(NULL, "phyclk_dptx_phy_ch0_txd_clk");
	phyclk_dptx_ch1 = clk_get(NULL, "phyclk_dptx_link_i_ch1_txd_clk");
	phyclk_dptx_phy_ch1 = clk_get(NULL, "phyclk_dptx_phy_ch1_txd_clk");
	phyclk_dptx_ch2 = clk_get(NULL, "phyclk_dptx_link_i_ch2_txd_clk");
	phyclk_dptx_phy_ch2 = clk_get(NULL, "phyclk_dptx_phy_ch2_txd_clk");
	phyclk_dptx_ch3 = clk_get(NULL, "phyclk_dptx_link_i_ch3_txd_clk");
	phyclk_dptx_phy_ch3 = clk_get(NULL, "phyclk_dptx_phy_ch3_txd_clk");
	phyclk_dptx_clk_24m = clk_get(NULL, "phyclk_dptx_link_i_clk_24m");
	phyclk_dptx_phy_24m = clk_get(NULL, "phyclk_dptx_phy_o_ref_clk_24m");

	if (clk_set_parent(phyclk_dptx_clk_div2, phyclk_dptx_phy_div2)) {
		clk_put(phyclk_dptx_clk_div2);
		clk_put(phyclk_dptx_phy_div2);

		pr_err("Unable to set parent %s of clock %s.\n",
				phyclk_dptx_phy_div2->name, phyclk_dptx_clk_div2->name);
		return -EINVAL;
	}

	if (clk_set_parent(phyclk_dptx_ch0, phyclk_dptx_phy_ch0)) {
		clk_put(phyclk_dptx_ch0);
		clk_put(phyclk_dptx_phy_ch0);

		pr_err("Unable to set parent %s of clock %s.\n",
				phyclk_dptx_phy_ch0->name, phyclk_dptx_ch0->name);
		return -EINVAL;
	}

	if (clk_set_parent(phyclk_dptx_ch1, phyclk_dptx_phy_ch1)) {
		clk_put(phyclk_dptx_ch1);
		clk_put(phyclk_dptx_phy_ch1);

		pr_err("Unable to set parent %s of clock %s.\n",
				phyclk_dptx_phy_ch1->name, phyclk_dptx_ch1->name);
		return -EINVAL;
	}

	if (clk_set_parent(phyclk_dptx_ch2, phyclk_dptx_phy_ch2)) {
		clk_put(phyclk_dptx_ch2);
		clk_put(phyclk_dptx_phy_ch2);

		pr_err("Unable to set parent %s of clock %s.\n",
				phyclk_dptx_phy_ch2->name, phyclk_dptx_ch2->name);
		return -EINVAL;
	}

	if (clk_set_parent(phyclk_dptx_ch3, phyclk_dptx_phy_ch3)) {
		clk_put(phyclk_dptx_ch3);
		clk_put(phyclk_dptx_phy_ch3);

		pr_err("Unable to set parent %s of clock %s.\n",
				phyclk_dptx_phy_ch3->name, phyclk_dptx_ch3->name);
		return -EINVAL;
	}

	if (clk_set_parent(phyclk_dptx_clk_24m, phyclk_dptx_phy_24m)) {
		clk_put(phyclk_dptx_phy_24m);
		clk_put(phyclk_dptx_clk_24m);

		pr_err("Unable to set parent %s of clock %s.\n",
				phyclk_dptx_phy_24m->name, phyclk_dptx_clk_24m->name);
		return -EINVAL;
	}

	clk_put(phyclk_dptx_clk_div2);
	clk_put(phyclk_dptx_phy_div2);
	clk_put(phyclk_dptx_ch0);
	clk_put(phyclk_dptx_phy_ch0);
	clk_put(phyclk_dptx_ch1);
	clk_put(phyclk_dptx_phy_ch1);
	clk_put(phyclk_dptx_ch2);
	clk_put(phyclk_dptx_phy_ch2);
	clk_put(phyclk_dptx_ch3);
	clk_put(phyclk_dptx_phy_ch3);
	clk_put(phyclk_dptx_clk_24m);
	clk_put(phyclk_dptx_phy_24m);

	return 0;
}
#else
int s3c_fb_clock_init(void)
{
	return 0;
}

int mipi_dsi_clock_init(void)
{
	return 0;
}

int s5p_dp_clock_init(void)
{
	return 0;
}
#endif
