/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS4270 - PLL support and clcck setup
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/err.h>
#include <linux/clk.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include "board-universal222ap.h"

static int exynos4_acp_clock_init(void)
{
	struct clk *aclk_acp, *pclk_acp;

	aclk_acp = clk_get(NULL, "aclk_acp");
	pclk_acp = clk_get(NULL, "pclk_acp");

	clk_set_rate(aclk_acp, 200000000);
	clk_set_rate(pclk_acp, 100000000);

	clk_put(aclk_acp);
	clk_put(pclk_acp);

	return 0;
}

static int exynos4_universal222ap_g2d_clock_init(void)
{
	struct clk *clk_child;
	struct clk *clk_parent = NULL;

	/* Set Set sclk_epll */
	clk_child = clk_get(NULL, "mout_g2d_acp_1");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "mout_g2d_acp_1");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "mout_epll");

	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "sclk_epll");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
			"sclk_epll", "mout_g2d_acp_1");
		return PTR_ERR(clk_child);
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	/* Set Set mout_g2d_acp_1 */
	clk_child = clk_get(NULL, "mout_g2d_acp");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "mout_g2d_acp");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "mout_g2d_acp_1");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "mout_g2d_acp_1");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
			"mout_g2d_acp_1", "mout_g2d_acp");
		return PTR_ERR(clk_child);
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	/* Set Set mout_g2d_acp */
	clk_child = clk_get(NULL, "sclk_fimg2d"); /* sclk_g2d_acp */
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "dout_g2d_acp");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "mout_g2d_acp");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "mout_g2d_acp");
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

	pr_info("FIMG2D clk : %s, clk rate : %ld\n", clk_child->name, clk_get_rate(clk_child));

	clk_put(clk_child);
	clk_put(clk_parent);

	return 0;
}

static int exynos4_uart_clock_init(void)
{
	struct clk *uart0, *uart1, *uart2, *uart3;
	struct clk *mout_mpll = NULL;
	int ret = 0;

	uart0 = clk_get_sys("exynos4210-uart.0", "uclk1");
	if (IS_ERR(uart0)) {
		pr_err("failed to get %s clock\n", "uart0");
		ret = PTR_ERR(uart0);
		goto err0;
	}

	uart1 = clk_get_sys("exynos4210-uart.1", "uclk1");
	if (IS_ERR(uart1)) {
		pr_err("failed to get %s clock\n", "uart1");
		ret = PTR_ERR(uart1);
		goto err1;
	}

	uart2 = clk_get_sys("exynos4210-uart.2", "uclk1");
	if (IS_ERR(uart2)) {
		pr_err("failed to get %s clock\n", "uart2");
		ret = PTR_ERR(uart2);
		goto err2;
	}

	uart3 = clk_get_sys("exynos4210-uart.3", "uclk1");
	if (IS_ERR(uart3)) {
		pr_err("failed to get %s clock\n", "uart3");
		ret = PTR_ERR(uart3);
		goto err3;
	}

	mout_mpll = clk_get(NULL, "mout_mpll_user_top");
	if (IS_ERR(mout_mpll)) {
		pr_err("failed to get %s clock\n", "mout_mpll");
		ret = PTR_ERR(mout_mpll);
		goto err4;
	}

	if (clk_set_parent(uart0, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll->name, uart0->name);
	else
		clk_set_rate(uart0, 200 * MHZ);

	if (clk_set_parent(uart1, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll->name, uart1->name);
	else
		clk_set_rate(uart1, 200 * MHZ);

	if (clk_set_parent(uart2, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll->name, uart2->name);
	else
		clk_set_rate(uart2, 200 * MHZ);

	if (clk_set_parent(uart3, mout_mpll))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mpll->name, uart3->name);
	else
		clk_set_rate(uart3, 200 * MHZ);

err4:
	clk_put(uart3);
err3:
	clk_put(uart2);
err2:
	clk_put(uart1);
err1:
	clk_put(uart0);
err0:
	return ret;
}

static int exynos4_spi_clock_init(void)
{
	struct clk *child_clk = NULL;
	struct clk *parent_clk = NULL;
	char clk_name[16];
	int i;

	for (i = 0; i < 3; i++) {
		snprintf(clk_name, sizeof(clk_name), "dout_spi%d", i);

		child_clk = clk_get(NULL, clk_name);
		if (IS_ERR(child_clk)) {
			pr_err("Failed to get %s clk\n", clk_name);
			return PTR_ERR(child_clk);
		}

		parent_clk = clk_get(NULL, "mout_mpll_user_top");
		if (IS_ERR(parent_clk)) {
			clk_put(child_clk);
			pr_err("Failed to get mout_mpll clk\n");
			return PTR_ERR(parent_clk);
		}

		if (clk_set_parent(child_clk, parent_clk)) {
			clk_put(child_clk);
			clk_put(parent_clk);
			pr_err("Unable to set parent %s of clock %s\n",
					parent_clk->name, child_clk->name);
			return PTR_ERR(child_clk);
		}

		clk_set_rate(child_clk, 100 * 1000 * 1000);

		clk_put(parent_clk);
		clk_put(child_clk);
	}

	return 0;
}

static int exynos4_adc_clock_init(void)
{
	struct clk *clk_child = NULL;
	struct clk *clk_parent = NULL;

	clk_child = clk_get(NULL, "dout_tsadc");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "dout_tsadc");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "mout_mpll_user_top");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "mout_mpll_user_top");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
			"mout_mpll_user_top", "dout_tsadc");
		return PTR_ERR(clk_child);
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	return 0;
}
void __init exynos4_universal222ap_clock_init(void)
{
	if (exynos4_uart_clock_init())
		pr_err("failed to init uart clock init\n");
	if (exynos4_acp_clock_init())
		pr_err("failed to init acp clock init\n");
	if (exynos4_universal222ap_g2d_clock_init())
		pr_err("failed to g2d clock init\n");
	if (exynos4_spi_clock_init())
		pr_err("failed to spi clock init\n");
	if (exynos4_adc_clock_init())
		pr_err("failed to init adc clock init\n");
}
