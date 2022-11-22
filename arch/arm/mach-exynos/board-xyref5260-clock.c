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

static int exynos5_bustop_pll_clock_init(void)
{
	struct clk *mout_bustop_bpll, *mout_mpll;

	mout_bustop_bpll = clk_get(NULL, "sclk_bustop_pll");
	mout_mpll = clk_get(NULL, "sclk_mpll");

	if (clk_set_parent(mout_bustop_bpll, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll->name, mout_bustop_bpll->name);

	clk_put(mout_bustop_bpll);
	clk_put(mout_mpll);

	return 0;
}

static int exynos5260_set_parent(char *child_name, char *parent_name);
static int exynos5_mediatop_pll_clock_init(void)
{
	struct clk *mout_mediatop_bpll, *mout_cpll;
	struct clk *aclk_disp_333, *aclk_disp_222;

	mout_mediatop_bpll = clk_get(NULL, "sclk_mediatop_pll");
	mout_cpll = clk_get(NULL, "sclk_cpll");

	aclk_disp_333 = clk_get(NULL, "aclk_disp_333");
	aclk_disp_222 = clk_get(NULL, "aclk_disp_222");

	clk_set_rate(aclk_disp_333, 334 * MHZ);
	clk_set_rate(aclk_disp_222, 223 * MHZ);

	if (clk_set_parent(mout_mediatop_bpll, mout_cpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_cpll->name, mout_mediatop_bpll->name);

	if (clk_set_parent(aclk_disp_333, mout_mediatop_bpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mediatop_bpll->name, aclk_disp_333->name);

	if (clk_set_parent(aclk_disp_222, mout_mediatop_bpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mediatop_bpll->name, aclk_disp_222->name);

	if (exynos5260_set_parent("sclk_disp_pixel_user","sclk_disp_pixel"))
		pr_err("failed to init aclk_disp_333\n");

	clk_set_rate(aclk_disp_333, 334 * MHZ);
	clk_set_rate(aclk_disp_222, 223 * MHZ);

	clk_put(mout_mediatop_bpll);
	clk_put(mout_cpll);
	clk_put(aclk_disp_333);
	clk_put(aclk_disp_222);

	return 0;
}

#if defined(CONFIG_S5P_DP)
static int exynos5_dp_clock_init(void)
{
	if (exynos5260_set_parent("phyclk_dptx_link_i_clk_div2",
				"phyclk_dptx_phy_clk_div2"))
		pr_err("failed to init phyclk_dptx_link_i_clk_div2");

	if (exynos5260_set_parent("phyclk_dptx_link_i_ch0_txd_clk",
				"phyclk_dptx_phy_ch0_txd_clk"))
		pr_err("failed to init phyclk_dptx_link_i_ch0_txd_clk");

	if (exynos5260_set_parent("phyclk_dptx_link_i_ch1_txd_clk",
				"phyclk_dptx_phy_ch1_txd_clk"))
		pr_err("failed to init phyclk_dptx_link_i_ch1_txd_clk");

	if (exynos5260_set_parent("phyclk_dptx_link_i_ch2_txd_clk",
				"phyclk_dptx_phy_ch2_txd_clk"))
		pr_err("failed to init phyclk_dptx_link_i_ch2_txd_clk");

	if (exynos5260_set_parent("phyclk_dptx_link_i_ch3_txd_clk",
				"phyclk_dptx_phy_ch3_txd_clk"))
		pr_err("failed to init phyclk_dptx_link_i_ch3_txd_clk");

	if (exynos5260_set_parent("phyclk_dptx_link_i_clk_24m",
				"phyclk_dptx_phy_o_ref_clk_24m"))
		pr_err("failed to init phyclk_dptx_link_i_clk_24m");

	return 0;
}
#elif defined(CONFIG_FB_MIPI_DSIM)
static int exynos5_mipi_dsi_clock_init(void)
{
	if (exynos5260_set_parent("phyclk_dsim1_rxclkesc0",
				"phyclk_mipi_dphy_4l_m_rxclkesc0"))
		pr_err("failed to init phyclk_dsim1_rxclkesc0");

	if (exynos5260_set_parent("sclk_mipi_dphy_4l_m_txclkescclk",
				"sclk_dsim1_txclkescclk"))
		pr_err("failed to init sclk_mipi_dphy_4l_m_txclkescclk");

	if (exynos5260_set_parent("phyclk_dsim1_bitclkdiv8",
				"phyclk_mipi_dphy_4l_m_txbyteclkhs"))
		pr_err("failed to init sclk_mipi_dphy_4l_m_txclkescclk");

	if (exynos5260_set_parent("sclk_mipi_dphy_4l_m_txclkesc0",
				"sclk_dsim1_txclkesc0"))
		pr_err("failed to init sclk_mipi_dphy_4l_m_txclkesc0");

	if (exynos5260_set_parent("sclk_mipi_dphy_4l_m_txclkesc1",
				"sclk_dsim1_txclkesc1"))
		pr_err("failed to init sclk_mipi_dphy_4l_m_txclkesc1");

	if (exynos5260_set_parent("sclk_mipi_dphy_4l_m_txclkesc2",
				"sclk_dsim1_txclkesc2"))
		pr_err("failed to init sclk_mipi_dphy_4l_m_txclkesc2");

	if (exynos5260_set_parent("sclk_mipi_dphy_4l_m_txclkesc3",
				"sclk_dsim1_txclkesc3"))
		pr_err("failed to init sclk_mipi_dphy_4l_m_txclkesc3");

	return 0;
}
#endif

static int exynos5_mmc_clock_init(void)
{
	struct clk *mout_fsys_pll;
	struct clk *mout_bustop_pll;
	struct clk *mout_mediatop_pll;
	struct clk *mout_mmc0_a, *mout_mmc1_a, *mout_mmc2_a;
	struct clk *sclk_mmc0_a, *sclk_mmc1_a, *sclk_mmc2_a;
	struct clk *dw_mmc0, *dw_mmc1, *dw_mmc2;

	mout_mmc0_a = clk_get(NULL, "mout_sclk_fsys_mmc0_sdclkin_a");
	mout_mmc1_a = clk_get(NULL, "mout_sclk_fsys_mmc1_sdclkin_a");
	mout_mmc2_a = clk_get(NULL, "mout_sclk_fsys_mmc2_sdclkin_a");
	sclk_mmc0_a = clk_get(NULL, "dout_sclk_fsys_mmc0_sdclkin_a");
	sclk_mmc1_a = clk_get(NULL, "dout_sclk_fsys_mmc1_sdclkin_a");
	sclk_mmc2_a = clk_get(NULL, "dout_sclk_fsys_mmc2_sdclkin_a");
	mout_bustop_pll = clk_get(NULL, "sclk_bustop_pll");
	mout_mediatop_pll = clk_get(NULL, "sclk_mediatop_pll");
	dw_mmc0 = clk_get_sys("dw_mmc.0", "sclk_dwmci");
	dw_mmc1 = clk_get_sys("dw_mmc.1", "sclk_dwmci");
	dw_mmc2 = clk_get_sys("dw_mmc.2", "sclk_dwmci");

	if (clk_set_parent(mout_mmc0_a, mout_bustop_pll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_bustop_pll->name, mout_mmc0_a->name);
	if (clk_set_parent(mout_mmc1_a, mout_bustop_pll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_bustop_pll->name, mout_mmc1_a->name);
	if (clk_set_parent(mout_mmc2_a, mout_bustop_pll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_bustop_pll->name, mout_mmc2_a->name);

	mout_fsys_pll = mout_bustop_pll;

	if (clk_set_parent(sclk_mmc0_a, mout_mmc0_a))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_fsys_pll->name, sclk_mmc0_a->name);
	if (clk_set_parent(sclk_mmc1_a, mout_mmc1_a))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_fsys_pll->name, sclk_mmc1_a->name);
	if (clk_set_parent(sclk_mmc2_a, mout_mmc2_a))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_fsys_pll->name, sclk_mmc2_a->name);

	clk_set_rate(sclk_mmc0_a, 800 * MHZ);
	clk_set_rate(sclk_mmc1_a, 800 * MHZ);
	clk_set_rate(sclk_mmc2_a, 800 * MHZ);
	clk_set_rate(dw_mmc0, 800 * MHZ);
	clk_set_rate(dw_mmc1, 800 * MHZ);
	clk_set_rate(dw_mmc2, 800 * MHZ);

	clk_put(mout_mmc0_a);
	clk_put(mout_mmc1_a);
	clk_put(mout_mmc2_a);
	clk_put(sclk_mmc0_a);
	clk_put(sclk_mmc1_a);
	clk_put(sclk_mmc2_a);
	clk_put(mout_bustop_pll);
	clk_put(mout_mediatop_pll);
	clk_put(dw_mmc0);
	clk_put(dw_mmc1);
	clk_put(dw_mmc2);

	return 0;
}

static int exynos5_usb_clock_init(void)
{
	struct clk *dout_sclk_usb;

	if (exynos5260_set_parent("dout_sclk_fsys_usbdrd30_suspend_clk",
				"ext_xtal"))
		pr_err("failed to init dout_sclk_fsys_usbdrd30_suspend_clk");

	if (exynos5260_set_parent("phyclk_usbdrd30_udrd30_phyclock_user",
				"phyclk_usbdrd30_udrd30_phyclock"))
		pr_err("failed to init phyclk_usbdrd30_udrd30_phyclock_user");

	if (exynos5260_set_parent("phyclk_usbdrd30_udrd30_pipe_pclk_user",
				"phyclk_usbdrd30_udrd30_pipe_pclk"))
		pr_err("failed to init phyclk_usbdrd30_udrd30_pipe_pclk_user");

	if (exynos5260_set_parent("phyclk_usbhost20_phy_freeclk_user",
				"phyclk_usbhost20_phy_freeclk"))
		pr_err("failed to init phyclk_usbhost20_phy_freeclk_user");

	if (exynos5260_set_parent("phyclk_usbhost20_phy_phyclock_user",
				"phyclk_usbhost20_phy_phyclock"))
		pr_err("failed to init phyclk_usbhost20_phy_phyclock_user");

	if (exynos5260_set_parent("phyclk_usbhost20_phy_clk48mohci_user",
				"phyclk_usbhost20_phy_clk48mohci"))
		pr_err("failed to init phyclk_usbhost20_phy_clk48mohci_user");

	dout_sclk_usb = clk_get(NULL, "dout_sclk_fsys_usbdrd30_suspend_clk");
	if (IS_ERR(dout_sclk_usb)) {
		pr_err("failed to get %s clock\n",
			"dout_sclk_fsys_usbdrd30_suspend_clk");
		return PTR_ERR(dout_sclk_usb);
	}
	clk_set_rate(dout_sclk_usb, 24 * MHZ);
	clk_put(dout_sclk_usb);

	return 0;
}

static int exynos5260_spi_clock_init(void)
{
	struct clk *child_clk = NULL;
	struct clk *parent_clk = NULL;
	char clk_name[16];
	int i;

	parent_clk = clk_get(NULL, "sclk_bustop_pll");
	if (IS_ERR(parent_clk)) {
		pr_err("Failed to get sclk_bustop_pll clk\n");
		return PTR_ERR(parent_clk);
	}

	for (i = 0; i < 3; i++) {
		snprintf(clk_name, sizeof(clk_name), "dout_sclk_spi%d", i);

		child_clk = clk_get(NULL, clk_name);
		if (IS_ERR(child_clk)) {
			pr_err("Failed to get %s clk\n", clk_name);
			goto err_clk0;
		}

		if (clk_set_parent(child_clk, parent_clk)) {
			pr_err("Unable to set parent %s of clock %s\n",
					parent_clk->name, child_clk->name);
			goto err_clk1;
		}

		clk_set_rate(child_clk, 100 * 1000 * 1000);
		clk_put(child_clk);
	}
	clk_put(parent_clk);
	return 0;

err_clk1:
	clk_put(child_clk);
err_clk0:
	clk_put(parent_clk);
	return PTR_ERR(child_clk);
}

static int exynos5260_g2d_clock_init(void)
{
	/* MEDIATOP_PLL : 666MHz */
	struct clk *sclk_cpll, *sclk_mediatop_pll;
	struct clk *aclk_g2d_333, *aclk_g2d_333_nogate;
	int ret = 0;

	sclk_cpll = clk_get(NULL, "sclk_cpll");
	if (IS_ERR(sclk_cpll)) {
		pr_err("failed to get %s clock\n", "sclk_cpll");
		return PTR_ERR(sclk_cpll);
	}

	sclk_mediatop_pll = clk_get(NULL, "sclk_mediatop_pll");
	if (IS_ERR(sclk_mediatop_pll)) {
		pr_err("failed to get %s clock\n", "sclk_mediatop_pll");
		ret = PTR_ERR(sclk_mediatop_pll);
		goto err_sclk_cpll;
	}

	aclk_g2d_333 = clk_get(NULL, "aclk_g2d_333");
	if (IS_ERR(aclk_g2d_333)) {
		pr_err("failed to get %s clock\n", "aclk_g2d_333");
		ret = PTR_ERR(aclk_g2d_333);
		goto err_sclk_mediatop_pll;
	}

	aclk_g2d_333_nogate = clk_get(NULL, "aclk_g2d_333_nogate"); /* sclk_fimg2d */
	if (IS_ERR(aclk_g2d_333_nogate)) {
		pr_err("failed to get %s clock\n", "aclk_g2d_333_nogate");
		ret = PTR_ERR(aclk_g2d_333_nogate);
		goto err_aclk_g2d_333;
	}

	if (clk_set_parent(sclk_mediatop_pll, sclk_cpll))
		pr_err("Unable to set parent %s of clock %s\n",
			sclk_cpll->name, sclk_mediatop_pll->name);

	if (clk_set_parent(aclk_g2d_333, sclk_mediatop_pll))
		pr_err("Unable to set parent %s of clock %s\n",
			sclk_mediatop_pll->name, aclk_g2d_333->name);

	if (clk_set_parent(aclk_g2d_333_nogate, aclk_g2d_333))
		pr_err("Unable to set parent %s of clock %s\n",
			aclk_g2d_333->name, aclk_g2d_333_nogate->name);

	/* CLKDIV_ACLK_333_G2D */
	if (clk_set_rate(aclk_g2d_333, 334 * MHZ))
		pr_err("Can't set %s clock rate\n", aclk_g2d_333->name);

	printk("[%s:%d] aclk_g2d_333: %lu Hz\n"
			, __func__, __LINE__, clk_get_rate(aclk_g2d_333));
	clk_put(aclk_g2d_333_nogate);
err_aclk_g2d_333:
	clk_put(aclk_g2d_333);
err_sclk_mediatop_pll:
	clk_put(sclk_mediatop_pll);
err_sclk_cpll:
	clk_put(sclk_cpll);

	return ret;
}
static int exynos5260_set_parent(char *child_name, char *parent_name)
{
	struct clk *clk_child;
	struct clk *clk_parent;

	clk_child = clk_get(NULL, child_name);
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", child_name);
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, parent_name);
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", parent_name);
		return PTR_ERR(clk_parent);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);

		pr_err("Unable to set parent %s of clock %s.\n",
				parent_name, child_name);
		return -EINVAL;
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	return 0;
}

static int exynos5260_gscl_clock_init(void)
{
	struct clk *aclk_gscl_333;
	int ret = 0;

	ret = exynos5260_set_parent("sclk_mediatop_pll", "sclk_cpll");
	if (ret) {
		pr_err("failed clock child(sclk_mediatop_pll),\
				parent(sclk_cpll)\n");
		return ret;
	}

	ret = exynos5260_set_parent("aclk_gscl_333", "sclk_mediatop_pll");
	if (ret) {
		pr_err("failed clock child(aclk_gscl_333),\
				parent(sclk_mediatop_pll)\n");
		return ret;
	}

	ret = exynos5260_set_parent("aclk_gscaler_333", "aclk_gscl_333");
	if (ret) {
		pr_err("failed clock child(aclk_gscaler_333),\
				parent(aclk_gscl_333)\n");
		return ret;
	}

	aclk_gscl_333 = clk_get(NULL, "aclk_gscl_333");
	if (IS_ERR(aclk_gscl_333)) {
		pr_err("failed to get %s clock\n", "aclk_gscl_333");
		ret = PTR_ERR(aclk_gscl_333);
		return ret;
	}
	if (clk_set_rate(aclk_gscl_333, 334 * MHZ))
		pr_err("Can't set %s clock rate\n", aclk_gscl_333->name);

	clk_put(aclk_gscl_333);

	return 0;
}

static int exynos5260_mfc_clock_init(void)
{
	struct clk *aclk_mfc_333 = NULL;

	if (exynos5260_set_parent("aclk_mfc_333", "sclk_mediatop_pll"))
		pr_err("failed to init aclk_mfc_333\n");

	if (exynos5260_set_parent("aclk_mfc_333_user", "aclk_mfc_333"))
		pr_err("failed to init aclk_mfc_333_user\n");

	aclk_mfc_333 = clk_get(NULL, "aclk_mfc_333");
	if (IS_ERR(aclk_mfc_333)) {
		pr_err("Failed to get aclk_mfc_333 clk\n");
		return PTR_ERR(aclk_mfc_333);
	}
	clk_set_rate(aclk_mfc_333, 334 * MHZ);
	return 0;
}

static int exynos5260_mscl_clock_init(void)
{
	struct clk *aclk;
	int ret = 0;

	if (exynos5260_set_parent("sclk_bustop_pll", "sclk_mpll"))
		return ret;

	if (exynos5260_set_parent("aclk_gscl_400", "sclk_bustop_pll"))
		return ret;

	if (exynos5260_set_parent("aclk_m2m_400", "aclk_gscl_400"))
		return ret;

	aclk = clk_get(NULL, "aclk_gscl_400");
	if (IS_ERR(aclk)) {
		pr_err("failed to get %s clock\n", aclk->name);
		return PTR_ERR(aclk);
	}
	clk_set_rate(aclk, 400 * MHZ);
	pr_info("++exynos mscaler clk rate %ld (%s)\n", clk_get_rate(aclk), aclk->name);
	clk_put(aclk);

	return 0;
}

static int exynos5260_sss_pclock_init(void)
{
	struct clk *pclk_g2d_83_nogate;

	pclk_g2d_83_nogate = clk_get(NULL, "pclk_g2d_83_nogate");
	if (IS_ERR(pclk_g2d_83_nogate)) {
		pr_err("failed to get %s clock\n", "pclk_g2d_83_nogate");
		return PTR_ERR(pclk_g2d_83_nogate);
	}

	if (clk_set_rate(pclk_g2d_83_nogate, 84 * MHZ))
		pr_err("Can't set %s clock rate\n", pclk_g2d_83_nogate->name);

	clk_put(pclk_g2d_83_nogate);

	return 0;
}

void __init exynos5_xyref5260_clock_init(void)
{
	if (exynos5_mediatop_pll_clock_init())
		pr_err("failed to init mediatop_pll clock init\n");

	if (exynos5_bustop_pll_clock_init())
		pr_err("failed to init bustop pll clock init\n");
#if defined(CONFIG_S5P_DP)
	if (exynos5_dp_clock_init())
		pr_err("failed to init dp clock init\n");
#elif defined(CONFIG_FB_MIPI_DSIM)
	if (exynos5_mipi_dsi_clock_init())
		pr_err("failed to init mipi_dsi clock init\n");
#endif
	if (exynos5_mmc_clock_init())
		pr_err("failed to init emmc clock init\n");

	if (exynos5_usb_clock_init())
		pr_err("failed to init usb clock init\n");

	if (exynos5260_spi_clock_init())
		pr_err("failed to init spi clock init\n");

	if (exynos5260_set_parent("aclk_disp_333_nogate", "aclk_disp_333"))
		pr_err("failed to init aclk_disp_333\n");

	if (exynos5260_set_parent("aclk_disp_222_nogate", "aclk_disp_222"))
		pr_err("failed to init aclk_disp_222\n");

	if (exynos5260_set_parent("aud_pll_user", "fout_epll"))
		pr_err("failed to init aud_pll_user\n");

	if (exynos5260_gscl_clock_init())
		pr_err("failed to init gscl clock init\n");

	if (exynos5260_g2d_clock_init())
		pr_err("failed to init g2d clock init\n");

	if (exynos5260_mfc_clock_init())
		pr_err("failed to init mfc clock init\n");

	if (exynos5260_mscl_clock_init())
		pr_err("failed to init mscl clock\n");

	if (exynos5260_sss_pclock_init())
		pr_err("failed to init sss pclock\n");
}
