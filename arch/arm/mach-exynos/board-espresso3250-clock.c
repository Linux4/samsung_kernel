/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * EXYNOS3250 - PLL support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <plat/clock.h>
#include <plat/cpu.h>
#include <media/exynos_camera.h>
#include "board-espresso3250.h"


#define CLK_PREFIX        "CLK: "

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#ifdef CLK_DEBUG
#define DEBUG_PRINT_INFO(fmt, ...) printk(CLK_PREFIX pr_fmt(fmt), ##__VA_ARGS__)
#else
#define DEBUG_PRINT_INFO(fmt, ...)
#endif

/*config core_r clock here*/
static int exynos3250_acp_clock_init(void)
{
	struct clk *sclk_mpll_pre_div, *mout_mpll_user_acp, *core_bus;
	struct clk *dout_acp_dmc, *aclk_cored, *aclk_corep;
	struct clk *aclk_acp, *pclk_acp;
	struct clk *xusbxti, *sclk_pwi;

	sclk_mpll_pre_div = clk_get(NULL, "sclk_mpll_pre_div");
	mout_mpll_user_acp = clk_get(NULL, "mout_mpll_user_acp");
	core_bus = clk_get(NULL, "mout_core_r_bus");
	xusbxti = clk_get(NULL, "xusbxti");
	sclk_pwi = clk_get(NULL, "sclk_pwi");

	if (clk_set_parent(mout_mpll_user_acp, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
			sclk_mpll_pre_div->name, mout_mpll_user_acp->name);

	if (clk_set_parent(core_bus, mout_mpll_user_acp))
		pr_err("Unable to set parent %s of clock %s.\n",
			mout_mpll_user_acp->name, core_bus->name);

	if (clk_set_parent(sclk_pwi, xusbxti))
		pr_err("Unable to set parent %s of clock %s.\n",
			xusbxti->name, sclk_pwi->name);

	clk_set_rate(sclk_pwi, 26*1000*1000);

	clk_put(sclk_mpll_pre_div);
	clk_put(mout_mpll_user_acp);
	clk_put(core_bus);
	clk_put(xusbxti);
	clk_put(sclk_pwi);

	dout_acp_dmc = clk_get(NULL, "dout_acp_dmc");
	clk_set_rate(dout_acp_dmc, 400 * MHZ);

	aclk_cored = clk_get(NULL, "aclk_cored");
	clk_set_rate(aclk_cored, 200 * MHZ);

	aclk_corep = clk_get(NULL, "aclk_corep");
	clk_set_rate(aclk_corep, 100 * MHZ);

	aclk_acp = clk_get(NULL, "aclk_acp");
	clk_set_rate(aclk_acp, 200 * MHZ);

	pclk_acp = clk_get(NULL, "pclk_acp");
	clk_set_rate(pclk_acp, 100 * MHZ);

	DEBUG_PRINT_INFO("dout_acp_dmc is %lu, aclk_cored is %lu,"\
		"aclk_corep is %lu, aclk_acp is %lu, pclk_acp is %lu\n",
					clk_get_rate(dout_acp_dmc),
					clk_get_rate(aclk_cored),
					clk_get_rate(aclk_corep),
					clk_get_rate(aclk_acp),
					clk_get_rate(pclk_acp));
	clk_put(dout_acp_dmc);
	clk_put(aclk_cored);
	clk_put(aclk_corep);
	clk_put(aclk_acp);
	clk_put(pclk_acp);

	return 0;

}

static int exynos3250_peril_clock_init(void)
{
	struct clk *aclk_gpr;

	aclk_gpr = clk_get(NULL, "aclk_gpr");

	clk_set_rate(aclk_gpr, 100*MHZ);

	DEBUG_PRINT_INFO("aclk_gdr rate is %lu\n", clk_get_rate(aclk_gpr));

	clk_put(aclk_gpr);
	return 0;
}

static int exynos3250_spi_clock_init(void)
{
	struct clk *child_clk = NULL;
	struct clk *parent_clk = NULL;
	char clk_name[16];
	int i;

	for (i = 0; i < 2; i++) {
		snprintf(clk_name, sizeof(clk_name), "dout_sclk_spi%d", i);

		child_clk = clk_get(NULL, clk_name);
		if (IS_ERR(child_clk)) {
			pr_err("Failed to get %s clk\n", clk_name);
			return PTR_ERR(child_clk);
		}

		parent_clk = clk_get(NULL, "sclk_mpll_pre_div");
		if (IS_ERR(parent_clk)) {
			clk_put(child_clk);
			pr_err("Failed to get sclk_mpll_pre_div clk\n");
			return PTR_ERR(parent_clk);
		}

		if (clk_set_parent(child_clk, parent_clk)) {
			clk_put(child_clk);
			clk_put(parent_clk);
			pr_err("Unable to set parent %s of clock %s\n",
					parent_clk->name, child_clk->name);
			return PTR_ERR(child_clk);
		}

		clk_set_rate(child_clk, 100 * MHZ);

		clk_put(parent_clk);
		clk_put(child_clk);
	}

	return 0;
}

static int exynos3250_mmc_clock_init(void)
{
	struct clk *sclk_mmc0, *sclk_mmc1, *sclk_mmc2, *sclk_mpll_pre_div;
	struct clk *dw_mmc0, *dw_mmc1, *dw_mmc2;

	sclk_mmc0 = clk_get(NULL, "dout_sclk_mmc0");
	sclk_mmc1 = clk_get(NULL, "dout_sclk_mmc1");
	sclk_mmc2 = clk_get(NULL, "dout_sclk_mmc2");
	sclk_mpll_pre_div = clk_get(NULL, "sclk_mpll_pre_div");
	dw_mmc0 = clk_get_sys("dw_mmc.0", "sclk_dwmci");
	dw_mmc1 = clk_get_sys("dw_mmc.1", "sclk_dwmci");
	dw_mmc2 = clk_get_sys("dw_mmc.2", "sclk_dwmci");

	if (clk_set_parent(sclk_mmc0, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll_pre_div->name, sclk_mmc0->name);
	if (clk_set_parent(sclk_mmc1, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll_pre_div->name, sclk_mmc1->name);
	if (clk_set_parent(sclk_mmc2, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll_pre_div->name, sclk_mmc2->name);

	clk_set_rate(sclk_mmc0, 400 * MHZ);
	clk_set_rate(sclk_mmc1, 400 * MHZ);
	clk_set_rate(sclk_mmc2, 400 * MHZ);
	clk_set_rate(dw_mmc0, 400 * MHZ);
	clk_set_rate(dw_mmc1, 400 * MHZ);
	clk_set_rate(dw_mmc2, 400 * MHZ);

	DEBUG_PRINT_INFO("dw_mmc0 is %lu, dw_mmc1 is %lu, dw_mmc2 is %lu\n",
						clk_get_rate(dw_mmc0),
						clk_get_rate(dw_mmc1),
						clk_get_rate(dw_mmc2));

	clk_put(sclk_mmc0);
	clk_put(sclk_mmc1);
	clk_put(sclk_mmc2);
	clk_put(sclk_mpll_pre_div);
	clk_put(dw_mmc0);
	clk_put(dw_mmc1);
	clk_put(dw_mmc2);

	return 0;
}

static int exynos3250_mipi_clock_init(void)
{
	struct clk *sclk_mipidphy, *sclk_mipi0;
	struct clk *sclk_mpll_pre_div;

	sclk_mipidphy = clk_get(NULL, "sclk_mipidphy4l");
	sclk_mipi0 = clk_get(NULL, "sclk_mipi0");
	sclk_mpll_pre_div = clk_get(NULL, "sclk_mpll_pre_div");

	if (clk_set_parent(sclk_mipidphy, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll_pre_div->name, sclk_mipidphy->name);

	clk_set_rate(sclk_mipidphy, 800 * MHZ);
	clk_set_rate(sclk_mipi0, 100 * MHZ);

	DEBUG_PRINT_INFO("sclk_mipidphy is %lu, sclk_mipi0 is %lu,"\
					"sclk_mpll_pre_div is %lu\n",
					clk_get_rate(sclk_mipidphy),
					clk_get_rate(sclk_mipi0),
					clk_get_rate(sclk_mpll_pre_div));

	clk_put(sclk_mipidphy);
	clk_put(sclk_mipi0);
	clk_put(sclk_mpll_pre_div);

	return 0;
}

static int exynos3250_uart_clock_init(void)
{
	struct clk *uart0, *uart1, *uart2, *uart3;
	struct clk *sclk_mpll_pre_div;

	uart0 = clk_get_sys("s5pv210-uart.0", "uclk1");
	uart1 = clk_get_sys("s5pv210-uart.1", "uclk1");
	uart2 = clk_get_sys("s5pv210-uart.2", "uclk1");
	uart3 = clk_get_sys("s5pv210-uart.3", "uclk1");

	sclk_mpll_pre_div = clk_get(NULL, "sclk_mpll_pre_div");

	if (clk_set_parent(uart0, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll_pre_div->name, uart0->name);
	else
		clk_set_rate(uart0, 100 * MHZ);

	if (clk_set_parent(uart1, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll_pre_div->name, uart1->name);
	else
		clk_set_rate(uart1, 100 * MHZ);

	if (clk_set_parent(uart2, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll_pre_div->name, uart2->name);
	else
		clk_set_rate(uart2, 100 * MHZ);

	if (clk_set_parent(uart3, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll_pre_div->name, uart3->name);
	else
		clk_set_rate(uart3, 100 * MHZ);

	DEBUG_PRINT_INFO("uart0 rate is %lu\n", clk_get_rate(uart0));
	DEBUG_PRINT_INFO("uart1 rate is %lu\n", clk_get_rate(uart1));
	DEBUG_PRINT_INFO("uart2 rate is %lu\n", clk_get_rate(uart2));
	DEBUG_PRINT_INFO("uart3 rate is %lu\n", clk_get_rate(uart3));

	clk_put(uart0);
	clk_put(uart1);
	clk_put(uart2);
	clk_put(uart3);

	clk_put(sclk_mpll_pre_div);

	return 0;
}

static int exynos3250_func_clk_cam_init(void)
{
	struct clk *clk_cam_blk_320, *clk_cam;

	clk_cam_blk_320 = clk_get(NULL, "cam_blk_320");
	clk_cam = clk_get(NULL, "sclk_cam_blk");

	if (clk_set_parent(clk_cam, clk_cam_blk_320))
		pr_err("Unable to set parent %s of clock %s.\n",
				clk_cam_blk_320->name, clk_cam->name);

	clk_set_rate(clk_cam, 320 * MHZ);

	DEBUG_PRINT_INFO("cal_cam rate is %lu\n", clk_get_rate(clk_cam));

	clk_put(clk_cam_blk_320);
	clk_put(clk_cam);

	return 0;
}

static int exynos3250_mfc_clock_init(void)
{
	struct clk *mout_mfc0, *sclk_mfc;
	struct clk *sclk_mpll_pre_div;

	sclk_mpll_pre_div = clk_get(NULL, "sclk_mpll_pre_div");
	mout_mfc0 = clk_get(NULL, "mout_mfc0");

	if (clk_set_parent(mout_mfc0, sclk_mpll_pre_div))
		pr_err("Unable to set parent %s of clock %s.\n",
				sclk_mpll_pre_div->name, mout_mfc0->name);

	sclk_mfc = clk_get_sys("s5p-mfc", "sclk_mfc");

	if (clk_set_parent(sclk_mfc, mout_mfc0))
		pr_err("Unable to set parent %s of clock %s.\n",
				mout_mfc0->name, sclk_mfc->name);

	clk_set_rate(sclk_mfc, 200 * MHZ);

	clk_put(sclk_mfc);

	clk_put(mout_mfc0);
	clk_put(sclk_mpll_pre_div);
	return 0;
}

static int exynos3250_disp_clock_init(void)
{
	struct clk *clk_child = NULL;
	struct clk *clk_parent = NULL;

	clk_child = clk_get(NULL, "aclk_160");
	if (IS_ERR(clk_child)) {
		pr_err("failed to get %s clock\n", "aclk_160");
		return PTR_ERR(clk_child);
	}

	clk_parent = clk_get(NULL, "sclk_mpll_pre_div");
	if (IS_ERR(clk_parent)) {
		clk_put(clk_child);
		pr_err("failed to get %s clock\n", "sclk_mpll_pre_div");
		return PTR_ERR(clk_child);
	}

	if (clk_set_parent(clk_child, clk_parent)) {
		clk_put(clk_child);
		clk_put(clk_parent);
		pr_err("Unable to set parent %s of clock %s.\n",
			"sclk_mpll_pre_div", "aclk_160");
		return PTR_ERR(clk_child);
	}

	clk_put(clk_child);
	clk_put(clk_parent);

	return 0;
}

static int exynos3250_adc_clock_setup(void)
{
	struct clk* sclk_tsadc;
	struct clk* mpll_pre_div;
	int ret = 0;

	sclk_tsadc = clk_get(NULL,"dout_sclk_tsadc");
	if (IS_ERR(sclk_tsadc)) {
		pr_err("Failed to get sclk_tsadc clk\n");
		ret = PTR_ERR(sclk_tsadc);
		goto err;
	}

	mpll_pre_div = clk_get(NULL,"sclk_mpll_pre_div");
	if (IS_ERR(mpll_pre_div)) {
		pr_err("Failed to get mpll_pre_div clk\n");
		ret = PTR_ERR(mpll_pre_div);
		goto err;
	}

	ret = clk_set_parent(sclk_tsadc,mpll_pre_div);
	if (ret){
		pr_err("Failed to set mpll_pre_div as parent clock of sclk_tsadc \n");
		goto err;
	}

err:
	return ret;
}

void __init exynos3_espresso3250_clock_init(void)
{
	if (exynos3250_acp_clock_init())
		pr_err("failed to init acp clock\n");

	if (exynos3250_peril_clock_init())
		pr_err("failed to init peril clock\n");

	if (exynos3250_uart_clock_init())
		pr_err("failed to init uart clock\n");

	if (exynos3250_mmc_clock_init())
		pr_err("failed to init emmc clock\n");

	if (exynos3250_mipi_clock_init())
		pr_err("failed to init mipi clock\n");

	if (exynos3250_spi_clock_init())
		pr_err("failed to init spi clock\n");

	if (exynos3250_func_clk_cam_init())
		pr_err("failed to init cam block clock\n");

	if (exynos3250_mfc_clock_init())
		pr_err("failed to init mfc clock\n");

	if (exynos3250_disp_clock_init())
		pr_err("failed to init display clock\n");

	if (exynos3250_adc_clock_setup())
		pr_err("failed to init adc clock\n");

	return;
}

