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
#include "board-xyref4415.h"

static int exynos4415_mmc_clock_init(void)
{
	struct clk *sclk_mpll;
	struct clk *sclk_mmc0_a, *sclk_mmc1_a, *sclk_mmc2_a;
	struct clk *dw_mmc0, *dw_mmc1, *dw_mmc2;
	int ret = 0;

	sclk_mpll = clk_get(NULL, "sclk_mpll");
	if (IS_ERR(sclk_mpll)) {
		pr_err("failed to get %s clock\n", "sclk_mpll");
		ret = PTR_ERR(sclk_mpll);
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

	ret = clk_set_parent(dw_mmc0, sclk_mpll);
	if (ret) {
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll->name, dw_mmc0->name);
		goto err8;
	}

	ret = clk_set_parent(dw_mmc1, sclk_mpll);
	if (ret) {
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll->name, dw_mmc1->name);
		goto err8;
	}

	ret = clk_set_parent(dw_mmc2, sclk_mpll);
	if (ret) {
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll->name, dw_mmc2->name);
		goto err8;
	}

	clk_set_rate(sclk_mmc0_a, 800 * MHZ);
	clk_set_rate(sclk_mmc1_a, 800 * MHZ);
	clk_set_rate(sclk_mmc2_a, 800 * MHZ);
	clk_set_rate(dw_mmc0, 800 * MHZ);
	clk_set_rate(dw_mmc1, 800 * MHZ);
	clk_set_rate(dw_mmc2, 800 * MHZ);

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
	clk_put(sclk_mpll);
err1:
	return ret;
}


#include "board-xyref4415.h"

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

void __init exynos4415_xyref4415_clock_init(void)
{
	/* Nothing here yet */
	if (exynos4415_mmc_clock_init())
		pr_err("failed to init emmc clock init\n");

	if (exynos4_disp_clock_init())
		pr_err("failed to init disp clock init\n");

	if (exynos4_jpeg_clock_init())
		pr_err("failed to init jpeg clock init\n");
};
