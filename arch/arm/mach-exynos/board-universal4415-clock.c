/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS4415 - Clock setup support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/cpu.h>
#include "board-universal4415.h"

static int exynos4415_mmc_clock_init(void)
{
	struct clk *sclk_epll;
	struct clk *sclk_mmc0_a, *sclk_mmc1_a, *sclk_mmc2_a;
	struct clk *dw_mmc0, *dw_mmc1, *dw_mmc2;
	int ret = 0;

	sclk_epll = clk_get(NULL, "sclk_epll");
	if (IS_ERR(sclk_epll)) {
		pr_err("failed to get %s clock\n", "sclk_epll");
		ret = PTR_ERR(sclk_epll);
		goto err1;
	}

	sclk_mmc0_a = clk_get(NULL, "dout_mmc0");
	if (IS_ERR(sclk_mmc0_a)) {
		pr_err("failed to get %s clock\n", "dout_mmc0");
		ret = PTR_ERR(sclk_mmc0_a);
		goto err2;
	}

	sclk_mmc1_a = clk_get(NULL, "dout_mmc1");
	if (IS_ERR(sclk_mmc1_a)) {
		pr_err("failed to get %s clock\n", "dout_mmc1");
		ret = PTR_ERR(sclk_mmc1_a);
		goto err3;
	}

	sclk_mmc2_a = clk_get(NULL, "dout_mmc2");
	if (IS_ERR(sclk_mmc2_a)) {
		pr_err("failed to get %s clock\n", "dout_mmc2");
		ret = PTR_ERR(sclk_mmc2_a);
		goto err4;
	}

	dw_mmc0 = clk_get_sys("dw_mmc.0", "sclk_mmc");
	if (IS_ERR(dw_mmc0)) {
		pr_err("%s: %s clock not found\n", "dw_mmc.0", "sclk_mmc");
		ret = PTR_ERR(dw_mmc0);
		goto err5;
	}

	dw_mmc1 = clk_get_sys("dw_mmc.1", "sclk_mmc");
	if (IS_ERR(dw_mmc1)) {
		pr_err("%s: %s clock not found\n", "dw_mmc.1", "sclk_mmc");
		ret = PTR_ERR(dw_mmc1);
		goto err6;
	}

	dw_mmc2 = clk_get_sys("dw_mmc.2", "sclk_mmc");
	if (IS_ERR(dw_mmc2)) {
		pr_err("%s: %s clock not found\n", "dw_mmc.2", "sclk_mmc");
		ret = PTR_ERR(dw_mmc2);
		goto err7;
	}

	ret = clk_set_parent(sclk_mmc0_a, sclk_epll);
	if (ret) {
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_epll->name, sclk_mmc0_a->name);
		goto err8;
	}

	ret = clk_set_parent(sclk_mmc1_a, sclk_epll);
	if (ret) {
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_epll->name, sclk_mmc1_a->name);
		goto err8;
	}

	ret = clk_set_parent(sclk_mmc2_a, sclk_epll);
	if (ret) {
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_epll->name, sclk_mmc2_a->name);
		goto err8;
	}

	clk_set_rate(sclk_mmc0_a, 786432000);
	clk_set_rate(sclk_mmc1_a, 786432000);
	clk_set_rate(sclk_mmc2_a, 786432000);
	clk_set_rate(dw_mmc0, 786432000);
	clk_set_rate(dw_mmc1, 786432000);
	clk_set_rate(dw_mmc2, 786432000);

err8:
	clk_put(dw_mmc2);
err7:
	clk_put(dw_mmc1);
err6:
	clk_put(dw_mmc0);
err5:
	clk_put(sclk_mmc2_a);
err4:
	clk_put(sclk_mmc1_a);
err3:
	clk_put(sclk_mmc0_a);
err2:
	clk_put(sclk_epll);
err1:
	return ret;
}


#include "board-universal4415.h"

static int exynos4415_set_parent(char *child_name, char *parent_name)
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

static int exynos4_disp_clock_init(void)
{

	if (exynos4415_set_parent("aclk_266", "aclk_266_pre"))
		pr_err("failed to init aclk_266");

	return 0;
}

static int exynos4415_camera_clock_init(void)
{
	struct clk *mout_isp_pll = NULL;
	struct clk *v_isppll = NULL;
	struct clk *ext_xtal = NULL;

	v_isppll = clk_get(NULL, "v_isppll");
	if (IS_ERR(v_isppll)) {
		pr_err("fail to get 'v_isppll'\n");
		return PTR_ERR(v_isppll);
	}

	ext_xtal = clk_get(NULL, "ext_xtal");
	if (IS_ERR(ext_xtal)) {
		clk_put(v_isppll);
		pr_err("fail to get 'ext_xtal'\n");
		return PTR_ERR(ext_xtal);
	}

	mout_isp_pll = clk_get(NULL, "mout_isp_pll");
	if (IS_ERR(mout_isp_pll)) {
		pr_err("fail to get 'mout_isp_pll'\n");
		clk_put(v_isppll);
		clk_put(ext_xtal);
		return PTR_ERR(mout_isp_pll);
	}

	clk_enable(v_isppll);
	clk_enable(mout_isp_pll);

	clk_set_parent(mout_isp_pll, ext_xtal);

	clk_disable(mout_isp_pll);
	clk_disable(v_isppll);

	clk_put(ext_xtal);
	clk_put(mout_isp_pll);
	clk_put(v_isppll);

	return 0;
}

static int exynos4415_g2d_clock_init(void)
{
	struct clk *clk_child;
	struct clk *clk_parent = NULL;

	/* Set mout_mpll_user_acp */
	clk_child = clk_get(NULL, "mout_g2d_acp_0");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock for %s\n",
				"mout_g2d_acp_0", "mout_mpll_user_acp");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "mout_mpll_user_acp");

	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock for %s\n",
				"mout_mpll_user_acp", "mout_g2d_acp_0");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
				"sclk_epll", "mout_g2d_acp_0");
		return PTR_ERR(clk_child);
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	/* Set mout_g2d_acp_0 */
	clk_child = clk_get(NULL, "mout_g2d_acp");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock for %s\n",
				"mout_g2d_acp", "mout_g2d_acp_0");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "mout_g2d_acp_0");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock for %s\n",
				"mout_g2d_acp_0", "mout_g2d_acp");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
				"mout_g2d_acp_0", "mout_g2d_acp");
		return PTR_ERR(clk_child);
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	/* Set mout_g2d_acp */
	clk_child = clk_get(NULL, "sclk_fimg2d"); /* sclk_g2d_acp */
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock for %s\n",
				"sclk_fimg2d", "mout_g2d_acp");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "mout_g2d_acp");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock for %s\n",
				"mout_g2d_acp", "sclk_fimg2d");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
				"mout_g2d_acp", "dout_g2d_acp");
		return PTR_ERR(clk_child);
	}
	clk_set_rate(clk_child, 200000000);

	pr_info("FIMG2D clk : %s, clk rate : %ld\n",
			clk_child->name, clk_get_rate(clk_child));

	clk_put(clk_child);
	clk_put(clk_parent);

	return 0;
}

int exynos4_jpeg_clock_init(void)
{
	struct clk *mout_jpeg;

	if (exynos4415_set_parent("mout_jpeg_0", "mout_mpll_user_top"))
		pr_err("failed to init mout_jpeg0");

	if (exynos4415_set_parent("mout_jpeg", "mout_jpeg_0"))
		pr_err("failed to init mout_jpeg");

	mout_jpeg = clk_get(NULL, "mout_jpeg");
	if (IS_ERR(mout_jpeg)) {
		pr_err("failed to get %s clock\n", mout_jpeg->name);
		return PTR_ERR(mout_jpeg);
	}
	clk_set_rate(mout_jpeg, 160 * MHZ);

	return 0;
}

void __init exynos4415_universal4415_clock_init(void)
{
	/* Nothing here yet */
	if (exynos4415_mmc_clock_init())
		pr_err("failed to init emmc clock init\n");

	if (exynos4_disp_clock_init())
		pr_err("failed to init disp clock init\n");

	if (exynos4_jpeg_clock_init())
		pr_err("failed to init jpeg clock init\n");

	if (exynos4415_g2d_clock_init())
		pr_err("failed to init g2d clock init\n");

	if (exynos4415_camera_clock_init())
		pr_err("failed to init g2d clock init\n");
};
