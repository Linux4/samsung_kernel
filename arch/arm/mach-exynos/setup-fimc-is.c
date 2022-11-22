/* linux/arch/arm/mach-exynos/setup-fimc-is.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * FIMC-IS gpio and clock configuration
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <plat/cpu.h>
#include <mach/exynos-fimc-is.h>
#include <mach/exynos-clock.h>
#include <mach/regs-gpio.h>
#include <mach/map.h>
#include <mach/regs-clock.h>

#undef USE_UART_DEBUG

#if defined(CONFIG_SOC_EXYNOS5260)
#include <mach/regs-clock-exynos5260.h>
/* Clock Gate func for Exynos5260 */
int exynos5260_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	int cfg = 0;
	u32 value = 0;

	if (clk_gate_id == 0)
		return 0;

	/* ISP 1 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_ISP_IP))
		value |= (1 << 4);
	if (clk_gate_id & (1 << FIMC_IS_GATE_DRC_IP))
		value |= (1 << 2);
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCC_IP))
		value |= (1 << 5);
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCP_IP))
		value |= (1 << 6);
	if (clk_gate_id & (1 << FIMC_IS_GATE_FD_IP))
		value |= (1 << 3);

	if (value > 0) {
		cfg = readl(EXYNOS5260_CLKGATE_IP_ISP1);
		if (is_on)
			writel(cfg | value, EXYNOS5260_CLKGATE_IP_ISP1);
		else
			writel(cfg & ~(value), EXYNOS5260_CLKGATE_IP_ISP1);
		pr_debug("%s :2 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	return 0;
}

int exynos5260_fimc_is_set_user_clk_gate(u32 group_id,
		bool is_on,
		u32 user_scenario_id,
		unsigned long msk_state,
		struct exynos_fimc_is_clk_gate_info *gate_info) {
	/* if you want to skip clock on/off, let this func return -1 */
	int ret = -1;

	switch (user_scenario_id) {
	default:
		ret = 0;
		break;
	}

	return ret;
}

/* Exynos5260 */
int exynos5260_fimc_is_cfg_clk(struct platform_device *pdev)
{
	/* top */
	struct clk *sclk_bustop_pll = NULL;
	struct clk *mout_isp1_media_266 = NULL;
	struct clk *sclk_memtop_pll = NULL;

	/* isp */
	struct clk *aclk_isp_266 = NULL;
	struct clk *aclk_isp_266_user = NULL;
	struct clk *pclk_isp_133 = NULL;
	struct clk *pclk_isp_66 = NULL;
	struct clk *sclk_mpwm_isp = NULL;

	/* mcuctl */
	struct clk *aclk_isp_400 = NULL;
	struct clk *aclk_ca5_clkin = NULL;
	struct clk *aclk_ca5_atclkin = NULL;
	struct clk *pclk_ca5_pclkdbg = NULL;
#ifdef USE_UART_DEBUG
	struct clk *sclk_uart_isp = NULL;
	struct clk *sclk_uart_isp_div = NULL;
	unsigned long isp_uart;
#endif
	pr_info("%s\n", __func__);

	/* 0. TOP */
	sclk_memtop_pll = clk_get(&pdev->dev, "sclk_memtop_pll");
	if (IS_ERR(sclk_memtop_pll))
		return PTR_ERR(sclk_memtop_pll);

	mout_isp1_media_266 = clk_get(&pdev->dev, "mout_isp1_media_266");
	if (IS_ERR(mout_isp1_media_266)) {
		clk_put(sclk_memtop_pll);
		return PTR_ERR(mout_isp1_media_266);
	}

	aclk_isp_266 = clk_get(&pdev->dev, "aclk_isp_266");
	if (IS_ERR(aclk_isp_266)) {
		clk_put(sclk_memtop_pll);
		clk_put(mout_isp1_media_266);
		return PTR_ERR(aclk_isp_266);
	}

	clk_set_parent(mout_isp1_media_266, sclk_memtop_pll);
	clk_set_parent(aclk_isp_266, mout_isp1_media_266);
	clk_set_rate(aclk_isp_266, 300 * 1000000);

	clk_put(sclk_memtop_pll);
	clk_put(mout_isp1_media_266);

	sclk_bustop_pll = clk_get(&pdev->dev, "sclk_bustop_pll");
	if (IS_ERR(sclk_bustop_pll))
		return PTR_ERR(sclk_bustop_pll);

	aclk_isp_400 = clk_get(&pdev->dev, "aclk_isp_400");
	if (IS_ERR(aclk_isp_400)) {
		clk_put(sclk_bustop_pll);
		return PTR_ERR(aclk_isp_400);
	}

	clk_set_parent(aclk_isp_400, sclk_bustop_pll);
	clk_set_rate(aclk_isp_400, 400 * 1000000);

	/* 1. MCUISP: aclk_isp_400 */
	aclk_ca5_clkin = clk_get(&pdev->dev, "aclk_ca5_clkin");
	if (IS_ERR(aclk_ca5_clkin)) {
		clk_put(aclk_isp_400);
		return PTR_ERR(aclk_ca5_clkin);
	}

	aclk_ca5_atclkin = clk_get(&pdev->dev, "aclk_ca5_atclkin");
	if (IS_ERR(aclk_ca5_atclkin)) {
		clk_put(aclk_isp_400);
		clk_put(aclk_ca5_clkin);
		return PTR_ERR(aclk_ca5_atclkin);
	}

	pclk_ca5_pclkdbg = clk_get(&pdev->dev, "pclk_ca5_pclkdbg");
	if (IS_ERR(pclk_ca5_pclkdbg)) {
		clk_put(aclk_isp_400);
		clk_put(aclk_ca5_clkin);
		clk_put(aclk_ca5_atclkin);
		return PTR_ERR(pclk_ca5_pclkdbg);
	}

	clk_set_parent(aclk_ca5_clkin, aclk_isp_400);
	clk_set_rate(aclk_ca5_atclkin, 200 * 1000000);
	clk_set_rate(pclk_ca5_pclkdbg, 100 * 1000000);

	clk_put(aclk_isp_400);
	clk_put(aclk_ca5_clkin);
	clk_put(aclk_ca5_atclkin);
	clk_put(pclk_ca5_pclkdbg);

	/* 2. ACLK_ISP: aclk_isp_266 */
	aclk_isp_266_user = clk_get(&pdev->dev, "aclk_isp_266_user");
	if (IS_ERR(aclk_isp_266_user)) {
		clk_put(aclk_isp_266);
		return PTR_ERR(aclk_isp_266_user);
	}

	pclk_isp_133 = clk_get(&pdev->dev, "pclk_isp_133");
	if (IS_ERR(pclk_isp_133)) {
		clk_put(aclk_isp_266);
		clk_put(aclk_isp_266_user);
		return PTR_ERR(pclk_isp_133);
	}

	pclk_isp_66 = clk_get(&pdev->dev, "pclk_isp_66");
	if (IS_ERR(pclk_isp_66)) {
		clk_put(aclk_isp_266);
		clk_put(aclk_isp_266_user);
		clk_put(pclk_isp_133);
		return PTR_ERR(pclk_isp_66);
	}

	sclk_mpwm_isp = clk_get(&pdev->dev, "sclk_mpwm_isp");
	if (IS_ERR(sclk_mpwm_isp)) {
		clk_put(aclk_isp_266);
		clk_put(aclk_isp_266_user);
		clk_put(pclk_isp_133);
		clk_put(pclk_isp_66);
		return PTR_ERR(sclk_mpwm_isp);
	}

	clk_set_parent(aclk_isp_266_user, aclk_isp_266);
	clk_set_rate(pclk_isp_133 , 150 * 1000000);
	clk_set_rate(pclk_isp_66 ,  75 * 1000000);
	clk_set_rate(sclk_mpwm_isp, 75 * 1000000);

	clk_put(aclk_isp_266);
	clk_put(aclk_isp_266_user);
	clk_put(pclk_isp_133);
	clk_put(pclk_isp_66);
	clk_put(sclk_mpwm_isp);

	return 0;
}

int exynos5260_fimc_is_cfg_gpio(struct platform_device *pdev, int channel, bool flag_on)
{
	return 0;
}

int exynos5260_fimc_is_clk_on(struct platform_device *pdev)
{
	struct clk *isp_ctrl = NULL;

	pr_info("%s\n", __func__);

	isp_ctrl = clk_get(&pdev->dev, "isp1");
	if (IS_ERR(isp_ctrl)) {
		pr_err("%s : clk_get(isp1) failed\n", __func__);
		return PTR_ERR(isp_ctrl);
	}

	clk_enable(isp_ctrl);
	clk_put(isp_ctrl);

	return 0;
}

int exynos5260_fimc_is_clk_off(struct platform_device *pdev)
{
	struct clk *isp_ctrl = NULL;

	pr_info("%s\n", __func__);

	isp_ctrl = clk_get(&pdev->dev, "isp1");
	if (IS_ERR(isp_ctrl)) {
		pr_err("%s : clk_get(isp1) failed\n", __func__);
		return PTR_ERR(isp_ctrl);
	}

	clk_disable(isp_ctrl);
	clk_put(isp_ctrl);

	return 0;
}

/* utility function to get rate */
unsigned int  fimc_is_get_rate(struct platform_device *pdev,
	const char *conid)
{
	struct clk *target;
	unsigned int rate_target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	rate_target = clk_get_rate(target);
	clk_put(target);
	pr_info("%s : %d\n", conid, rate_target);

	return rate_target;
}

int exynos5260_fimc_is_print_clk(struct platform_device *pdev)
{

	fimc_is_get_rate(pdev, "sclk_memtop_pll");
	fimc_is_get_rate(pdev, "mout_isp1_media_266");
	fimc_is_get_rate(pdev, "sclk_bustop_pll");

	fimc_is_get_rate(pdev, "aclk_isp_400");
	fimc_is_get_rate(pdev, "aclk_isp_266");

	fimc_is_get_rate(pdev, "aclk_ca5_clkin");
	fimc_is_get_rate(pdev, "aclk_ca5_atclkin");
	fimc_is_get_rate(pdev, "pclk_ca5_pclkdbg");

	fimc_is_get_rate(pdev, "aclk_isp_266_user");
	fimc_is_get_rate(pdev, "pclk_isp_133");
	fimc_is_get_rate(pdev, "pclk_isp_66");
	fimc_is_get_rate(pdev, "sclk_mpwm_isp");

	fimc_is_get_rate(pdev, "sclk_mediatop_pll");
	fimc_is_get_rate(pdev, "aclk_gscl_fimc");
	fimc_is_get_rate(pdev, "aclk_gscl_fimc_user");
	fimc_is_get_rate(pdev, "sclK_csis");

	fimc_is_get_rate(pdev, "ext_xtal");
	fimc_is_get_rate(pdev, "dout_isp1_sensor0_a");
	fimc_is_get_rate(pdev, "dout_isp1_sensor1_a");
	fimc_is_get_rate(pdev, "dout_isp1_sensor2_a");

	return 0;
}

int exynos5260_fimc_is_print_cfg(struct platform_device *pdev, u32 channel)
{
	return 0;
}

static int exynos5260_fimc_is_print_pwr(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pr_info("ISP power state(0x%08x)\n", readl(EXYNOS5260_ISP_STATUS));
	pr_info("CA5 power state(0x%08x)\n", readl(EXYNOS5260_A5IS_STATUS));

	return 0;
}
#elif defined(CONFIG_ARCH_EXYNOS3)
int exynos3_fimc_is_cfg_clk(struct platform_device *pdev)
{
	//int cfg;
	/*parent*/
	struct clk *sclk_mpll_pre_div = NULL;
	struct clk *xusbxti = NULL;

	/*mcuisp*/
	struct clk *dout_aclk_400 = NULL;
	struct clk *aclk_400_mcuisp = NULL;

	struct clk *aclk_mcuisp_div0 = NULL;
	struct clk *aclk_mcuisp_div1 = NULL;

	/*ack_266*/
	struct clk *mout_aclk_266_0 = NULL;
	struct clk *dout_aclk_266	= NULL;
	struct clk *aclk_266 = NULL;

	struct clk *aclk_div0 = NULL;
	struct clk *aclk_div1 = NULL;
	struct clk *aclk_div2 = NULL;

	/*function sclk*/
//	struct clk *sclk_pwm_isp = NULL;
	struct clk *sclk_uart_isp = NULL;
	struct clk *sclk_spi1_isp = NULL;
	struct clk *dout_sclk_spi1_isp = NULL;
	struct clk *sclk_spi0_isp = NULL;
	struct clk *dout_sclk_spi0_isp = NULL;

	/*MCLK*/
	struct clk *sclk_cam1 = NULL;

	unsigned long mcu_isp_400;
	unsigned long isp_266;
	unsigned long isp_uart;
//	unsigned long isp_pwm;
	unsigned long isp_spi1;
	unsigned long isp_spi0;
	unsigned long isp_cam1;


	pr_info(" %s\n",__func__);

	/* initialize Clocks */

	/*
	 * HACK: hard clock setting to preventing
	 * ISP init fail problem
	 */
	#if 0
	writel(0x31, EXYNOS5_CLKDIV_ISP0);
	writel(0x31, EXYNOS5_CLKDIV_ISP1);
	writel(0x1, EXYNOS5_CLKDIV_ISP2);
	cfg = readl(EXYNOS5_CLKDIV2_RATIO0);
	cfg |= (0x1 < 6);
	writel(0x1, EXYNOS5_CLKDIV2_RATIO0);
	#endif
	/* 0. Parent*/
	sclk_mpll_pre_div = clk_get(&pdev->dev, "sclk_mpll_pre_div");
	if (IS_ERR(sclk_mpll_pre_div)) {
		pr_err("%s : clk_get(sclk_mpll_pre_div) failed\n", __func__);
		return PTR_ERR(sclk_mpll_pre_div);
	}

	//clk_set_rate(sclk_mpll_pre_div, 800 * 1000000);

	pr_info("sclk_mpll_pre_div : %ld\n", clk_get_rate(sclk_mpll_pre_div));

	xusbxti = clk_get(&pdev->dev, "xusbxti");
	if (IS_ERR(xusbxti)) {
		pr_err("%s : clk_get(xxti) failed\n", __func__);
		return PTR_ERR(xusbxti);
	}

	pr_info("xusbxti : %ld\n", clk_get_rate(xusbxti));


	/* 1. MCUISP  and DIV*/
	/*
	mout_mpll
	sclk_mpll_mif
	sclk_mpll_pre_div
	dout_aclk_400
	aclk_400_mcuisp
	*/

	dout_aclk_400 = clk_get(&pdev->dev, "dout_aclk_400");
	if (IS_ERR(dout_aclk_400)) {
		pr_err("%s : clk_get(dout_aclk_400) failed\n", __func__);
		return PTR_ERR(dout_aclk_400);
	}

	clk_set_parent(dout_aclk_400, sclk_mpll_pre_div);
	clk_set_rate(dout_aclk_400, 400 * 1000000);

	aclk_400_mcuisp = clk_get(&pdev->dev, "aclk_400_mcuisp");
	if (IS_ERR(aclk_400_mcuisp)) {
		pr_err("%s : clk_get(aclk_400_mcuisp) failed\n", __func__);
		return PTR_ERR(aclk_400_mcuisp);
	}

	clk_set_parent(aclk_400_mcuisp, dout_aclk_400);

	/*mcuisp div*/
	aclk_mcuisp_div0 = clk_get(&pdev->dev, "aclk_mcuisp_div0");
	if (IS_ERR(aclk_mcuisp_div0)) {
		pr_err("%s : clk_get(aclk_mcuisp_div0) failed\n", __func__);
		return PTR_ERR(aclk_mcuisp_div0);
	}

	aclk_mcuisp_div1 = clk_get(&pdev->dev, "aclk_mcuisp_div1");
	if (IS_ERR(aclk_mcuisp_div1)) {
		pr_err("%s : clk_get(aclk_mcuisp_div1) failed\n", __func__);
		return PTR_ERR(aclk_mcuisp_div1);
	}

	clk_set_rate(aclk_mcuisp_div0, 200 * 1000000);
	clk_set_rate(aclk_mcuisp_div1, 100 * 1000000);

	mcu_isp_400 = clk_get_rate(aclk_400_mcuisp);
	pr_info("mcu_isp_400 : %ld\n", mcu_isp_400);

	mcu_isp_400 = clk_get_rate(aclk_mcuisp_div0);
	pr_info("mcu_isp_400_div0 : %ld\n", mcu_isp_400);

	mcu_isp_400 = clk_get_rate(aclk_mcuisp_div1);
	pr_info("aclk_mcuisp_div1 : %ld\n", mcu_isp_400);

	//clk_put(sclk_mpll_pre_div);
	clk_put(dout_aclk_400);
	clk_put(aclk_400_mcuisp);
	clk_put(aclk_mcuisp_div0);
	clk_put(aclk_mcuisp_div1);

	/* 2. ACLK_266_ISP */
	/*
	sclk_mpll_pre_div/mout_vpll : TODO. if we use vpll for 300M later, we will select this clk.
	mout_aclk_266_0
	dout_aclk_266
	aclk_266
	*/
	mout_aclk_266_0 = clk_get(&pdev->dev, "mout_aclk_266_0");
	if (IS_ERR(mout_aclk_266_0)) {
		pr_err("%s : clk_get(mout_aclk_266_0) failed\n", __func__);
		return PTR_ERR(mout_aclk_266_0);
	}
	dout_aclk_266 = clk_get(&pdev->dev, "dout_aclk_266");
	if (IS_ERR(dout_aclk_266)) {
		pr_err("%s : clk_get(dout_aclk_266) failed\n", __func__);
		return PTR_ERR(dout_aclk_266);
	}

	aclk_266 = clk_get(&pdev->dev, "aclk_266");
	if (IS_ERR(aclk_266)) {
		pr_err("%s : clk_get(aclk_266) failed\n", __func__);
		return PTR_ERR(aclk_266);
	}

	clk_set_parent(dout_aclk_266, mout_aclk_266_0);
	clk_set_parent(aclk_266, dout_aclk_266);
//	clk_set_rate(dout_aclk_266, 134 * 1000000);
	clk_set_rate(dout_aclk_266, 266 * 1000000);

	isp_266 = clk_get_rate(aclk_266);
	pr_info("isp_266 : %ld\n", isp_266);

	clk_put(mout_aclk_266_0);
	clk_put(dout_aclk_266);
	clk_put(aclk_266);

	/* ISP_DIV0 */
	aclk_div0 = clk_get(&pdev->dev, "aclk_div0");
	if (IS_ERR(aclk_div0)) {
		pr_err("%s : clk_get(isp_div0) failed\n", __func__);
		return PTR_ERR(aclk_div0);
	}

	//clk_set_rate(aclk_div0, 135 * 1000000);
	clk_set_rate(aclk_div0, 50 * 1000000);
	pr_info("aclk_div0 : %ld\n", clk_get_rate(aclk_div0));

	/* ISP_DIV1 */
	aclk_div1 = clk_get(&pdev->dev, "aclk_div1");
	if (IS_ERR(aclk_div1)) {
		pr_err("%s : clk_get(isp_div1) failed\n", __func__);
		return PTR_ERR(aclk_div1);
	}

	clk_set_rate(aclk_div1, 50 * 1000000);
	pr_info("aclk_div1 : %ld\n", clk_get_rate(aclk_div1));

	/* ISP_DIV2*/
	aclk_div2 = clk_get(&pdev->dev, "aclk_div2");
	if (IS_ERR(aclk_div2)) {
		pr_err("%s : clk_get(mpwm_div) failed\n", __func__);
		return PTR_ERR(aclk_div2);
	}

	clk_set_rate(aclk_div2, 50 * 1000000);
	pr_info("aclk_div2 : %ld\n", clk_get_rate(aclk_div2));

	clk_put(aclk_div0);
	clk_put(aclk_div1);
	clk_put(aclk_div2);


	/* 3. SCLK_ISP_BLK */
	/*UART, SPI, PWM, all from sclk_mpll_pre_div*/

	/* UART-ISP */
	sclk_uart_isp = clk_get(&pdev->dev, "sclk_uart_isp");
	if (IS_ERR(sclk_uart_isp)) {
		pr_err("%s : clk_get(sclk_uart_isp) failed\n", __func__);
		return PTR_ERR(sclk_uart_isp);
	}

	clk_set_parent(sclk_uart_isp, sclk_mpll_pre_div);
	clk_set_rate(sclk_uart_isp, 67 * 1000000);

	isp_uart = clk_get_rate(sclk_uart_isp);
	pr_info("isp_uart : %ld\n", isp_uart);

	//clk_enable(sclk_uart_isp);//jmq.enable uart when config
	clk_put(sclk_uart_isp);

	/* SPI1-ISP */
	sclk_spi1_isp = clk_get(&pdev->dev, "sclk_spi1_isp");
	if (IS_ERR(sclk_spi1_isp)) {
		pr_err("%s : clk_get(sclk_spi1_isp) failed\n", __func__);
		return PTR_ERR(sclk_spi1_isp);
	}

	dout_sclk_spi1_isp = clk_get(&pdev->dev, "dout_sclk_spi1_isp");
	if (IS_ERR(dout_sclk_spi1_isp)) {
		pr_err("%s : clk_get(dout_sclk_spi1_isp) failed\n", __func__);
		return PTR_ERR(dout_sclk_spi1_isp);
	}

	clk_set_parent(dout_sclk_spi1_isp, sclk_mpll_pre_div);
	clk_set_parent(sclk_spi1_isp, dout_sclk_spi1_isp);
	clk_set_rate(sclk_spi1_isp, 100 * 1000000);

	isp_spi1 = clk_get_rate(sclk_spi1_isp);
	pr_info("isp_spi1 : %ld\n", isp_spi1);

	clk_put(sclk_spi1_isp);
	clk_put(dout_sclk_spi1_isp);

	/* SPI0-ISP */
	sclk_spi0_isp = clk_get(&pdev->dev, "sclk_spi0_isp");
	if (IS_ERR(sclk_spi0_isp)) {
		pr_err("%s : clk_get(sclk_spi0_isp) failed\n", __func__);
		return PTR_ERR(sclk_spi0_isp);
	}

	dout_sclk_spi0_isp = clk_get(&pdev->dev, "dout_sclk_spi0_isp");
	if (IS_ERR(dout_sclk_spi0_isp)) {
		pr_err("%s : clk_get(dout_sclk_spi0_isp) failed\n", __func__);
		return PTR_ERR(dout_sclk_spi0_isp);
	}

	clk_set_parent(dout_sclk_spi0_isp, sclk_mpll_pre_div);
	clk_set_parent(sclk_spi0_isp, dout_sclk_spi0_isp);
	clk_set_rate(sclk_spi0_isp, 100 * 1000000);

	isp_spi0 = clk_get_rate(sclk_spi0_isp);
	pr_info("isp_spi0 : %ld\n", isp_spi0);

	clk_put(sclk_spi0_isp);
	clk_put(dout_sclk_spi0_isp);


	/* CAM1 */
	sclk_cam1 = clk_get(&pdev->dev, "sclk_cam1");
	if (IS_ERR(sclk_cam1)) {
		pr_err("%s : clk_get(sclk_cam1) failed\n", __func__);
		return PTR_ERR(sclk_cam1);
	}


	clk_set_parent(sclk_cam1, xusbxti);//from where?
	clk_set_rate(sclk_cam1, 24 * 1000000);
	//clk_set_rate(sclk_cam1, 26 * 1000000);

	isp_cam1 = clk_get_rate(sclk_cam1);
	pr_info("isp_cam1 : %ld\n", isp_cam1);

	//put the parent
	clk_put(sclk_mpll_pre_div);
	clk_put(xusbxti);

	/*FLite*/
	/*MIPI*/
	return 0;
}

int exynos3_fimc_is_cfg_gpio(struct platform_device *pdev, int channel, bool flag_on)
{
	return 0;
}

int exynos3_fimc_is_clk_on(struct platform_device *pdev)
{
	struct clk *isp0_ctrl = NULL;
	struct clk *isp1_ctrl = NULL;
	struct clk *mcuisp = NULL;
	struct clk *csis0 = NULL;
	struct clk *csis1 = NULL;
	struct clk *lite0 = NULL;
	struct clk *lite1 = NULL;
	struct clk *sclk_cam1 = NULL;

	pr_info("%s\n",__func__);

	isp0_ctrl = clk_get(&pdev->dev, "isp0_ctrl");
	if (IS_ERR(isp0_ctrl)) {
		pr_err("%s : clk_get(isp0_ctrl) failed\n", __func__);
		return PTR_ERR(isp0_ctrl);
	}

	clk_enable(isp0_ctrl);
	clk_put(isp0_ctrl);

	isp1_ctrl = clk_get(&pdev->dev, "isp1_ctrl");
	if (IS_ERR(isp1_ctrl)) {
		pr_err("%s : clk_get(isp1_ctrl) failed\n", __func__);
		return PTR_ERR(isp1_ctrl);
	}

	clk_enable(isp1_ctrl);
	clk_put(isp1_ctrl);

	mcuisp = clk_get(&pdev->dev, "mcuisp");
	if (IS_ERR(mcuisp)) {
		pr_err("%s : clk_get(mcuisp) failed\n", __func__);
		return PTR_ERR(mcuisp);
	}

	clk_enable(mcuisp);
	clk_put(mcuisp);

	csis0 = clk_get(&pdev->dev, "csis0");
	if (IS_ERR(csis0)) {
		pr_err("%s : clk_get(csis0) failed\n", __func__);
		return PTR_ERR(csis0);
	}

	clk_enable(csis0);
	clk_put(csis0);

	csis1 = clk_get(&pdev->dev, "csis1");
	if (IS_ERR(csis1)) {
		pr_err("%s : clk_get(csis1) failed\n", __func__);
		return PTR_ERR(csis1);
	}

	clk_enable(csis1);
	clk_put(csis1);

	lite0 = clk_get(&pdev->dev, "lite0");
	if (IS_ERR(lite0)) {
		pr_err("%s : clk_get(lite0) failed\n", __func__);
		return PTR_ERR(lite0);
	}

	clk_enable(lite0);
	clk_put(lite0);

	lite1 = clk_get(&pdev->dev, "lite1");
	if (IS_ERR(lite1)) {
		pr_err("%s : clk_get(lite1) failed\n", __func__);
		return PTR_ERR(lite1);
	}

	clk_enable(lite1);
	clk_put(lite1);

	sclk_cam1 = clk_get(&pdev->dev, "sclk_cam1");
	if (IS_ERR(sclk_cam1)) {
		pr_err("%s : clk_get(sclk_cam1) failed\n", __func__);
		return PTR_ERR(sclk_cam1);
	}
	clk_enable(sclk_cam1);
	clk_put(sclk_cam1);

	return 0;
}

int exynos3_fimc_is_clk_off(struct platform_device *pdev)
{
/*
	struct clk *isp0_ctrl = NULL;
	struct clk *isp1_ctrl = NULL;
	struct clk *mcuisp = NULL;
	struct clk *csis0 = NULL;
	struct clk *csis1 = NULL;
	struct clk *lite0 = NULL;
	struct clk *lite1 = NULL;
	struct clk *sclk_cam1 = NULL;
	pr_info("%s\n",__func__);

	isp0_ctrl = clk_get(&pdev->dev, "isp0_ctrl");
	if (IS_ERR(isp0_ctrl)) {
		pr_err("%s : clk_get(isp0_ctrl) failed\n", __func__);
		return PTR_ERR(isp0_ctrl);
	}

	clk_disable(isp0_ctrl);
	clk_put(isp0_ctrl);

	isp1_ctrl = clk_get(&pdev->dev, "isp1_ctrl");
	if (IS_ERR(isp1_ctrl)) {
		pr_err("%s : clk_get(isp1_ctrl) failed\n", __func__);
		return PTR_ERR(isp1_ctrl);
	}

	clk_disable(isp1_ctrl);
	clk_put(isp1_ctrl);

	mcuisp = clk_get(&pdev->dev, "mcuisp");
	if (IS_ERR(mcuisp)) {
		pr_err("%s : clk_get(mcuisp) failed\n", __func__);
		return PTR_ERR(mcuisp);
	}

	clk_disable(mcuisp);
	clk_put(mcuisp);

	csis0 = clk_get(&pdev->dev, "csis0");
	if (IS_ERR(csis0)) {
		pr_err("%s : clk_get(csis0) failed\n", __func__);
		return PTR_ERR(csis0);
	}

	clk_disable(csis0);
	clk_put(csis0);

	csis1 = clk_get(&pdev->dev, "csis1");
	if (IS_ERR(csis1)) {
		pr_err("%s : clk_get(csis1) failed\n", __func__);
		return PTR_ERR(csis1);
	}

	clk_disable(csis1);
	clk_put(csis1);

	lite0 = clk_get(&pdev->dev, "lite0");
	if (IS_ERR(lite0)) {
		pr_err("%s : clk_get(lite0) failed\n", __func__);
		return PTR_ERR(lite0);
	}

	clk_disable(lite0);
	clk_put(lite0);

	lite1 = clk_get(&pdev->dev, "lite1");
	if (IS_ERR(lite1)) {
		pr_err("%s : clk_get(lite1) failed\n", __func__);
		return PTR_ERR(lite1);
	}

	clk_disable(lite1);
	clk_put(lite1);

	sclk_cam1 = clk_get(&pdev->dev, "sclk_cam1");
	if (IS_ERR(sclk_cam1)) {
		pr_err("%s : clk_get(sclk_cam1) failed\n", __func__);
		return PTR_ERR(sclk_cam1);
	}
	clk_disable(sclk_cam1);
	clk_put(sclk_cam1);
*/
	return 0;
}

int exynos3_fimc_is_print_clk(struct platform_device *pdev)
{
	return 0;
}

int exynos3_fimc_is_print_cfg(struct platform_device *pdev, u32 channel)
{
	return 0;
}

#elif defined(CONFIG_SOC_EXYNOS3470)
extern int mali_dvfs_level_lock(void);
extern int mali_dvfs_level_unlock(void);

/* Clock Gate func for Exynos3470 */
int exynos3470_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	int cfg = 0;
	u32 value = 0;

	if (clk_gate_id == 0)
		return 0;

	/* ISP 0 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_ISP_IP))
		value |= (1 << 0);
	if (clk_gate_id & (1 << FIMC_IS_GATE_DRC_IP))
		value |= (1 << 1);
	if (clk_gate_id & (1 << FIMC_IS_GATE_FD_IP))
		value |= (1 << 2);

	if (value > 0) {
		cfg = readl(EXYNOS4_CLKGATE_IP_ISP0);
		if (is_on)
			writel(cfg | value, EXYNOS4_CLKGATE_IP_ISP0);
		else
			writel(cfg & ~(value), EXYNOS4_CLKGATE_IP_ISP0);
		pr_debug("%s :0 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	/* ISP 1 */
	value = 0;
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCC_IP))
		value |= (1 << 15);
	if (clk_gate_id & (1 << FIMC_IS_GATE_SCP_IP))
		value |= (1 << 16);

	if (value > 0) {
		cfg = readl(EXYNOS4_CLKGATE_IP_ISP1);
		if (is_on)
			writel(cfg | value, EXYNOS4_CLKGATE_IP_ISP1);
		else
			writel(cfg & ~(value), EXYNOS4_CLKGATE_IP_ISP1);
		pr_debug("%s :0 [%s] gate(%d) (0x%x) * (0x%x)\n", __func__,
				is_on ? "ON" : "OFF",
				clk_gate_id,
				cfg,
				value);
	}

	return 0;
}

int exynos3470_fimc_is_set_user_clk_gate(u32 group_id,
		bool is_on,
		u32 user_scenario_id,
		unsigned long msk_state,
		struct exynos_fimc_is_clk_gate_info *gate_info) {
	/* if you want to skip clock on/off, let this func return -1 */
	int ret = -1;

	switch (user_scenario_id) {
	default:
		ret = 0;
		break;
	}

	return ret;
}

int exynos3470_fimc_is_cfg_clk(struct platform_device *pdev)
{
	int ret = 0;
	struct clk *sclk_vpll = NULL;
	struct clk *mout_mpll = NULL;
	struct clk *pre_aclk_400_mcuisp = NULL;
	struct clk *aclk_400_mcuisp = NULL;
	struct clk *aclk_400_mcuisp_div0 = NULL;
	struct clk *aclk_400_mcuisp_div1 = NULL;
	struct clk *pre_aclk_266 = NULL;
	struct clk *aclk_266 = NULL;
	struct clk *aclk_266_div0 = NULL;
	struct clk *aclk_266_div1 = NULL;
	struct clk *aclk_266_div2 = NULL;
#ifdef USE_UART_DEBUG
	struct clk *sclk_uart_isp = NULL;
#endif
	pr_debug("(%s) ++++\n", __func__);

	/* VPLL should be lock to 900 MHz before configuring clock.
	 * Because ISP PLL is shared with G3D.
	 */
	ret = mali_dvfs_level_lock();
	if (ret < 0) {
		pr_err("mali_dvfs_level_lock was fail(%d)\n", ret);
		return -EINVAL;
	}

	/* 1. MCUISP */
	mout_mpll = clk_get(&pdev->dev, "mout_mpll");
	if (IS_ERR(mout_mpll)) {
		pr_err("fail to get 'mout_mpll'\n");
		return PTR_ERR(mout_mpll);
	}

	pre_aclk_400_mcuisp = clk_get(&pdev->dev, "pre_aclk_400_mcuisp");
	if (IS_ERR(pre_aclk_400_mcuisp)) {
		clk_put(mout_mpll);
		pr_err("fail to get 'pre_aclk_400_mcuisp'\n");
		return PTR_ERR(pre_aclk_400_mcuisp);
	}
	clk_set_parent(pre_aclk_400_mcuisp, mout_mpll);
	clk_set_rate(pre_aclk_400_mcuisp, 400 * 1000000);

	aclk_400_mcuisp = clk_get(&pdev->dev, "aclk_400_mcuisp");
	if (IS_ERR(aclk_400_mcuisp)) {
		clk_put(mout_mpll);
		clk_put(pre_aclk_400_mcuisp);
		pr_err("fail to get 'aclk_400_mcuisp'\n");
		return PTR_ERR(aclk_400_mcuisp);
	}
	clk_set_parent(aclk_400_mcuisp, pre_aclk_400_mcuisp);

	aclk_400_mcuisp_div0 = clk_get(&pdev->dev, "aclk_400_mcuisp_div0");
	if (IS_ERR(aclk_400_mcuisp_div0)) {
		clk_put(mout_mpll);
		clk_put(pre_aclk_400_mcuisp);
		clk_put(aclk_400_mcuisp);
		pr_err("fail to get 'aclk_400_mcuisp_div0'\n");
		return PTR_ERR(aclk_400_mcuisp_div0);
	}
	clk_set_rate(aclk_400_mcuisp_div0, 200 * 1000000);

	aclk_400_mcuisp_div1 = clk_get(&pdev->dev, "aclk_400_mcuisp_div1");
	if (IS_ERR(aclk_400_mcuisp_div1)) {
		clk_put(mout_mpll);
		clk_put(pre_aclk_400_mcuisp);
		clk_put(aclk_400_mcuisp);
		clk_put(aclk_400_mcuisp_div0);
		pr_err("fail to get 'aclk_400_mcuisp_div1'\n");
		return PTR_ERR(aclk_400_mcuisp_div1);
	}
	clk_set_rate(aclk_400_mcuisp_div1, 100 * 1000000);

	pr_info("aclk_400_mcuisp : %ld\n", clk_get_rate(aclk_400_mcuisp));
	pr_info("aclk_400_mcuisp_div0 : %ld\n", clk_get_rate(aclk_400_mcuisp_div0));
	pr_info("aclk_400_mcuisp_div1 : %ld\n", clk_get_rate(aclk_400_mcuisp_div1));

	clk_put(mout_mpll);
	clk_put(pre_aclk_400_mcuisp);
	clk_put(aclk_400_mcuisp);
	clk_put(aclk_400_mcuisp_div0);
	clk_put(aclk_400_mcuisp_div1);

	/* 2. ACLK_ISP */
	sclk_vpll = clk_get(&pdev->dev, "sclk_vpll");
	if (IS_ERR(sclk_vpll)) {
		pr_err("fail to get 'sclk_vpll'\n");
		return PTR_ERR(sclk_vpll);
	}

	pre_aclk_266 = clk_get(&pdev->dev, "pre_aclk_266");
	if (IS_ERR(pre_aclk_266)) {
		clk_put(sclk_vpll);
		pr_err("fail to get 'pre_aclk_266'\n");
		return PTR_ERR(pre_aclk_266);
	}
	clk_set_parent(pre_aclk_266, sclk_vpll);
	clk_set_rate(pre_aclk_266, 300 * 1000000);

	aclk_266 = clk_get(&pdev->dev, "aclk_266");
	if (IS_ERR(aclk_266)) {
		clk_put(sclk_vpll);
		clk_put(pre_aclk_266);
		pr_err("fail to get 'aclk_266'\n");
		return PTR_ERR(aclk_266);
	}
	clk_set_parent(aclk_266, pre_aclk_266);

	aclk_266_div0 = clk_get(&pdev->dev, "aclk_266_div0");
	if (IS_ERR(aclk_266_div0)) {
		clk_put(sclk_vpll);
		clk_put(pre_aclk_266);
		clk_put(aclk_266);
		pr_err("fail to get 'aclk_266_div0'\n");
		return PTR_ERR(aclk_266_div0);
	}
	clk_set_rate(aclk_266_div0, 150 * 1000000);

	aclk_266_div1 = clk_get(&pdev->dev, "aclk_266_div1");
	if (IS_ERR(aclk_266_div1)) {
		clk_put(sclk_vpll);
		clk_put(pre_aclk_266);
		clk_put(aclk_266);
		clk_put(aclk_266_div0);
		pr_err("fail to get 'aclk_266_div1'\n");
		return PTR_ERR(aclk_266_div1);
	}
	clk_set_rate(aclk_266_div1, 75 * 1000000);

	aclk_266_div2 = clk_get(&pdev->dev, "aclk_266_div2");
	if (IS_ERR(aclk_266_div2)) {
		clk_put(sclk_vpll);
		clk_put(pre_aclk_266);
		clk_put(aclk_266);
		clk_put(aclk_266_div0);
		clk_put(aclk_266_div1);
		pr_err("fail to get 'aclk_266_div2'\n");
		return PTR_ERR(aclk_266_div2);
	}
	clk_set_rate(aclk_266_div2, 75 * 1000000);

	pr_info("aclk_266 : %ld\n", clk_get_rate(aclk_266));
	pr_info("aclk_266_div0 : %ld\n", clk_get_rate(aclk_266_div0));
	pr_info("aclk_266_div1 : %ld\n", clk_get_rate(aclk_266_div1));
	pr_info("aclk_266_div2 : %ld\n", clk_get_rate(aclk_266_div2));

	clk_put(sclk_vpll);
	clk_put(pre_aclk_266);
	clk_put(aclk_266);
	clk_put(aclk_266_div0);
	clk_put(aclk_266_div1);
	clk_put(aclk_266_div2);


#ifdef USE_UART_DEBUG
	/* 3. UART-ISP */
	/* change special clock source to mout_mpll_user_top */
	mout_mpll = clk_get(&pdev->dev, "mout_mpll_user_top");
	if (IS_ERR(mout_mpll)) {
		pr_err("fail to get 'mout_mpll_user_top'\n");
		return PTR_ERR(mout_mpll);
	}
	sclk_uart_isp = clk_get(&pdev->dev, "sclk_uart_isp");
	if (IS_ERR(sclk_uart_isp)) {
		clk_put(mout_mpll);
		pr_err("fail to get 'sclk_uart_isp'\n");
		return PTR_ERR(sclk_uart_isp);
	}
	clk_set_parent(sclk_uart_isp, mout_mpll);
	clk_set_rate(sclk_uart_isp, 50 * 1000000);

	pr_debug("sclk_uart_isp : %ld\n", clk_get_rate(sclk_uart_isp));
	clk_put(mout_mpll);
	clk_put(sclk_uart_isp);
#endif
	pr_debug("(%s) ----\n", __func__);

	return 0;
}

int exynos3470_fimc_is_cfg_gpio(struct platform_device *pdev, int channel, bool flag_on)
{
	return 0;
}

int exynos3470_fimc_is_clk_on(struct platform_device *pdev)
{
	struct clk *isp_ctrl = NULL;
	struct clk *pre_aclk_400_mcuisp = NULL;
	struct clk *aclk_400_mcuisp = NULL;
	struct clk *pre_aclk_266 = NULL;
	struct clk *aclk_266 = NULL;

	pr_debug("(%s) ++++\n", __func__);

	/* ISP */
	isp_ctrl = clk_get(&pdev->dev, "gate_isp0");
	if (IS_ERR(isp_ctrl)) {
		pr_err("fail to get 'gate_isp0'\n");
		return PTR_ERR(isp_ctrl);
	}
	clk_enable(isp_ctrl);
	clk_put(isp_ctrl);

	isp_ctrl = clk_get(&pdev->dev, "gate_isp1");
	if (IS_ERR(isp_ctrl)) {
		pr_err("fail to get 'gate_isp1'\n");
		return PTR_ERR(isp_ctrl);
	}
	clk_enable(isp_ctrl);
	clk_put(isp_ctrl);

#ifdef USE_UART_DEBUG
	isp_ctrl = clk_get(&pdev->dev, "uart_isp");
	if (IS_ERR(isp_ctrl)) {
		pr_err("fail to get 'uart_isp'\n");
		return PTR_ERR(isp_ctrl);
	}
	clk_enable(isp_ctrl);
	clk_put(isp_ctrl);
#endif

	/* SUB selection */
	pre_aclk_400_mcuisp = clk_get(&pdev->dev, "pre_aclk_400_mcuisp");
	if (IS_ERR(pre_aclk_400_mcuisp))
		return PTR_ERR(pre_aclk_400_mcuisp);
	aclk_400_mcuisp = clk_get(&pdev->dev, "aclk_400_mcuisp");
	if (IS_ERR(aclk_400_mcuisp)) {
		clk_put(pre_aclk_400_mcuisp);
		return PTR_ERR(aclk_400_mcuisp);
	}
	clk_set_parent(aclk_400_mcuisp, pre_aclk_400_mcuisp);
	clk_put(pre_aclk_400_mcuisp);
	clk_put(aclk_400_mcuisp);

	pre_aclk_266 = clk_get(&pdev->dev, "pre_aclk_266");
	if (IS_ERR(pre_aclk_266))
		return PTR_ERR(pre_aclk_266);
	aclk_266 = clk_get(&pdev->dev, "aclk_266");
	if (IS_ERR(aclk_266)) {
		clk_put(pre_aclk_266);
		return PTR_ERR(aclk_266);
	}
	clk_set_parent(aclk_266, pre_aclk_266);
	clk_put(pre_aclk_266);
	clk_put(aclk_266);

	pr_debug("(%s) ----\n", __func__);

	return 0;
}

int exynos3470_fimc_is_clk_off(struct platform_device *pdev)
{
	int ret = 0;
	struct clk *isp_ctrl = NULL;
	struct clk *ext_xtal = NULL;
	struct clk *aclk_400_mcuisp = NULL;
	struct clk *aclk_266 = NULL;

	pr_debug("(%s) ++++\n", __func__);

	/* 3. ISP */
	isp_ctrl = clk_get(&pdev->dev, "gate_isp0");
	if (IS_ERR(isp_ctrl))
		return PTR_ERR(isp_ctrl);

	clk_disable(isp_ctrl);
	clk_put(isp_ctrl);

	isp_ctrl = clk_get(&pdev->dev, "gate_isp1");
	if (IS_ERR(isp_ctrl))
		return PTR_ERR(isp_ctrl);

	clk_disable(isp_ctrl);
	clk_put(isp_ctrl);

#ifdef USE_UART_DEBUG
	isp_ctrl = clk_get(&pdev->dev, "uart_isp");
	if (IS_ERR(isp_ctrl)) {
		pr_err("fail to get 'uart_isp'\n");
		return PTR_ERR(isp_ctrl);
	}
	clk_disable(isp_ctrl);
	clk_put(isp_ctrl);
#endif
	/* SUB selection */
	ext_xtal = clk_get(&pdev->dev, "ext_xtal");
	if (IS_ERR(ext_xtal))
		return PTR_ERR(ext_xtal);

	aclk_400_mcuisp = clk_get(&pdev->dev, "aclk_400_mcuisp");
	if (IS_ERR(aclk_400_mcuisp)) {
		clk_put(ext_xtal);
		return PTR_ERR(aclk_400_mcuisp);
	}
	clk_set_parent(aclk_400_mcuisp, ext_xtal);
	clk_put(aclk_400_mcuisp);

	aclk_266 = clk_get(&pdev->dev, "aclk_266");
	if (IS_ERR(aclk_266)) {
		clk_put(ext_xtal);
		return PTR_ERR(aclk_266);
	}
	clk_set_parent(aclk_266, ext_xtal);
	clk_put(aclk_266);

	/* mali_dvfs_level_unlock() function should be paired with
	 * mali_dvfs_level_lock() function.
	 */
	ret = mali_dvfs_level_unlock();
	if (ret < 0) {
		pr_err("mali_dvfs_level_unlock was fail(%d)\n", ret);
		return -EINVAL;
	}

	pr_debug("(%s) ----\n", __func__);
	return 0;
}

int exynos3470_fimc_is_print_clk(struct platform_device *pdev)
{
	return 0;
}

int exynos3470_fimc_is_print_cfg(struct platform_device *pdev, u32 channel)
{
	return 0;
}
#elif defined(CONFIG_SOC_EXYNOS4415)
#include <mach/regs-clock-exynos4415.h>
/* #define CLOCK_OUT_DEBUG */

static int exynos4415_fimc_is_cfg_clk(struct platform_device *pdev)
{
	struct clk *mout_isp_pll = NULL;
	struct clk *mout_aclk_isp0_300 = NULL;
	struct clk *dout_aclk_isp0_300 = NULL;
	struct clk *mout_aclk_isp1_300 = NULL;
	struct clk *dout_aclk_isp1_300 = NULL;
	struct clk *aclk_isp0_300 = NULL;
	struct clk *aclk_isp1_300 = NULL;
	struct clk *dout_sclk_csis1 = NULL;
	struct clk *dout_aclk_3aa1 = NULL;
	struct clk *dout_aclk_lite_b = NULL;
	struct clk *dout_pclk_isp0_a_150 = NULL;
	struct clk *dout_pclk_isp0_b_150 = NULL;
	struct clk *dout_pclk_isp0_b_75 = NULL;
	struct clk *dout_pclk_isp1_150 = NULL;
	struct clk *dout_pclk_isp1_75 = NULL;
	struct clk *aclk_isp0_400 = NULL;
	struct clk *aclk_isp0_400_pre = NULL;
	struct clk *mout_mpll_user_top = NULL;
	struct clk *sclk_pwm_isp = NULL;
	struct clk *dout_spi0_isp = NULL;
	struct clk *dout_spi1_isp = NULL;

	struct clk *sclk_csis0 = NULL;
	struct clk *sclk_csis1 = NULL;

	struct clk *sclk_pxlasync_csis0_fimc = NULL;
	struct clk *sclk_pxlasync_csis1_fimc = NULL;

	struct clk *phyclk_rxbyte_clkhs0_s2a = NULL;
	struct clk *mout_phyclk_rxbyte_clkhs0_s2a = NULL;
	struct clk *phyclk_rxbyte_clkhs0_s4 = NULL;
	struct clk *mout_phyclk_rxbyte_clkhs0_s4 = NULL;

	/* front DPHY */
	phyclk_rxbyte_clkhs0_s2a = clk_get(&pdev->dev, "phyclk_rxbyte_clkhs0_s2a");
	if (IS_ERR(phyclk_rxbyte_clkhs0_s2a)) {
		pr_err("%s : clk_get(phyclk_rxbyte_clkhs0_s2a) failed\n", __func__);
		return PTR_ERR(phyclk_rxbyte_clkhs0_s2a);
	}
	mout_phyclk_rxbyte_clkhs0_s2a = clk_get(&pdev->dev, "mout_phyclk_rxbyte_clkhs0_s2a");
	if (IS_ERR(mout_phyclk_rxbyte_clkhs0_s2a)) {
		pr_err("%s : clk_get(mout_phyclk_rxbyte_clkhs0_s2a) failed\n", __func__);
		return PTR_ERR(mout_phyclk_rxbyte_clkhs0_s2a);
	}
	clk_set_parent(mout_phyclk_rxbyte_clkhs0_s2a, phyclk_rxbyte_clkhs0_s2a);
	clk_put(mout_phyclk_rxbyte_clkhs0_s2a);
	clk_put(phyclk_rxbyte_clkhs0_s2a);

	/* main DPHY */
	phyclk_rxbyte_clkhs0_s4 = clk_get(&pdev->dev, "phyclk_rxbyte_clkhs0_s4");
	if (IS_ERR(phyclk_rxbyte_clkhs0_s4)) {
		pr_err("%s : clk_get(phyclk_rxbyte_clkhs0_s4) failed\n", __func__);
		return PTR_ERR(phyclk_rxbyte_clkhs0_s4);
	}
	mout_phyclk_rxbyte_clkhs0_s4 = clk_get(&pdev->dev, "mout_phyclk_rxbyte_clkhs0_s4");
	if (IS_ERR(mout_phyclk_rxbyte_clkhs0_s4)) {
		pr_err("%s : clk_get(mout_phyclk_rxbyte_clkhs0_s4) failed\n", __func__);
		return PTR_ERR(mout_phyclk_rxbyte_clkhs0_s4);
	}
	clk_set_parent(mout_phyclk_rxbyte_clkhs0_s4, phyclk_rxbyte_clkhs0_s4);
	clk_put(mout_phyclk_rxbyte_clkhs0_s4);
	clk_put(phyclk_rxbyte_clkhs0_s4);

	mout_isp_pll = clk_get(&pdev->dev, "mout_isp_pll");
	if (IS_ERR(mout_isp_pll)) {
		pr_err("%s : clk_get(mout_isp_pll) failed\n", __func__);
		return PTR_ERR(mout_isp_pll);
	}

	/* ack_isp0_300 */
	mout_aclk_isp0_300 = clk_get(&pdev->dev, "mout_aclk_isp0_300");
	if (IS_ERR(mout_aclk_isp0_300)) {
		pr_err("%s : clk_get(mout_aclk_isp0_300) failed\n", __func__);
		clk_put(mout_isp_pll);
		return PTR_ERR(mout_aclk_isp0_300);
	}
	dout_aclk_isp0_300 = clk_get(&pdev->dev, "dout_aclk_isp0_300");
	if (IS_ERR(dout_aclk_isp0_300)) {
		pr_err("%s : clk_get(dout_aclk_isp0_300) failed\n", __func__);
		clk_put(mout_isp_pll);
		clk_put(mout_aclk_isp0_300);
		return PTR_ERR(dout_aclk_isp0_300);
	}
	aclk_isp0_300 = clk_get(&pdev->dev, "aclk_isp0_300");
	if (IS_ERR(aclk_isp0_300)) {
		pr_err("%s : clk_get(aclk_isp0_300) failed\n", __func__);
		clk_put(mout_isp_pll);
		clk_put(mout_aclk_isp0_300);
		clk_put(dout_aclk_isp0_300);
		return PTR_ERR(aclk_isp0_300);
	}
	clk_set_parent(mout_aclk_isp0_300, mout_isp_pll);
	clk_set_parent(aclk_isp0_300, dout_aclk_isp0_300);
	clk_set_rate(dout_aclk_isp0_300, 300 * 1000000);
	pr_info("dout_aclk_isp0_300 : %ld\n", clk_get_rate(dout_aclk_isp0_300));
	clk_put(mout_aclk_isp0_300);
	clk_put(dout_aclk_isp0_300);
	clk_put(aclk_isp0_300);

	/* ack_isp1_300 */
	mout_aclk_isp1_300 = clk_get(&pdev->dev, "mout_aclk_isp1_300");
	if (IS_ERR(mout_aclk_isp1_300)) {
		pr_err("%s : clk_get(mout_aclk_isp1_300) failed\n", __func__);
		clk_put(mout_isp_pll);
		return PTR_ERR(mout_aclk_isp1_300);
	}
	dout_aclk_isp1_300 = clk_get(&pdev->dev, "dout_aclk_isp1_300");
	if (IS_ERR(dout_aclk_isp1_300)) {
		pr_err("%s : clk_get(dout_aclk_isp1_300) failed\n", __func__);
		clk_put(mout_isp_pll);
		clk_put(mout_aclk_isp1_300);
		return PTR_ERR(dout_aclk_isp1_300);
	}
	aclk_isp1_300 = clk_get(&pdev->dev, "aclk_isp1_300");
	if (IS_ERR(aclk_isp1_300)) {
		pr_err("%s : clk_get(aclk_isp1_300) failed\n", __func__);
		clk_put(mout_isp_pll);
		clk_put(mout_aclk_isp1_300);
		clk_put(dout_aclk_isp1_300);
		return PTR_ERR(aclk_isp1_300);
	}
	clk_set_parent(mout_aclk_isp1_300, mout_isp_pll);
	clk_set_parent(aclk_isp1_300, dout_aclk_isp1_300);
	clk_set_rate(dout_aclk_isp1_300, 300 * 1000000);
	pr_info("dout_aclk_isp1_300 : %ld\n", clk_get_rate(dout_aclk_isp1_300));
	clk_put(mout_aclk_isp1_300);
	clk_put(dout_aclk_isp1_300);
	clk_put(aclk_isp1_300);

	/* PWM */
	sclk_pwm_isp = clk_get(&pdev->dev, "sclk_pwm_isp");
	if (IS_ERR(sclk_pwm_isp)) {
		pr_err("%s : clk_get(sclk_pwm_isp) failed\n", __func__);
		return PTR_ERR(sclk_pwm_isp);
	}
	clk_set_parent(sclk_pwm_isp, mout_isp_pll);
	clk_set_rate(sclk_pwm_isp, 75* 1000000);
	pr_info("sclk_pwm_isp : %ld\n", clk_get_rate(sclk_pwm_isp));
	clk_put(sclk_pwm_isp);

	/* SPI0 */
	dout_spi0_isp = clk_get(&pdev->dev, "dout_spi0_isp");
	if (IS_ERR(dout_spi0_isp)) {
		pr_err("%s : clk_get(dout_spi0_isp) failed\n", __func__);
		return PTR_ERR(dout_spi0_isp);
	}
	clk_set_parent(dout_spi0_isp, mout_isp_pll);
	clk_set_rate(dout_spi0_isp, 50* 1000000);
	pr_info("dout_spi0_isp : %ld\n", clk_get_rate(dout_spi0_isp));
	clk_put(dout_spi0_isp);

	/* SPI1 */
	dout_spi1_isp = clk_get(&pdev->dev, "dout_spi1_isp");
	if (IS_ERR(dout_spi1_isp)) {
		pr_err("%s : clk_get(dout_spi1_isp) failed\n", __func__);
		return PTR_ERR(dout_spi1_isp);
	}
	clk_set_parent(dout_spi1_isp, mout_isp_pll);
	clk_set_rate(dout_spi1_isp, 50* 1000000);
	pr_info("dout_spi1_isp : %ld\n", clk_get_rate(dout_spi1_isp));
	clk_put(dout_spi1_isp);

	/* CSIS0 */
	sclk_csis0 = clk_get(&pdev->dev, "sclk_csis0");
	if (IS_ERR(sclk_csis0)) {
		pr_err("%s : clk_get(sclk_csis0) failed\n", __func__);
		return PTR_ERR(sclk_csis0);
	}
	clk_set_parent(sclk_csis0, mout_isp_pll);
	clk_set_rate(sclk_csis0, 266 * 1000000);
	pr_info("sclk_csis0 : %ld\n", clk_get_rate(sclk_csis0));
	clk_put(sclk_csis0);

	/* CSIS1 */
	sclk_csis1 = clk_get(&pdev->dev, "sclk_csis1");
	if (IS_ERR(sclk_csis1)) {
		pr_err("%s : clk_get(sclk_csis1) failed\n", __func__);
		return PTR_ERR(sclk_csis1);
	}
	clk_set_parent(sclk_csis1, mout_isp_pll);
	clk_set_rate(sclk_csis1, 266 * 1000000);
	pr_info("sclk_csis1 : %ld\n", clk_get_rate(sclk_csis1));
	clk_put(sclk_csis1);

	sclk_pxlasync_csis0_fimc = clk_get(&pdev->dev, "sclk_pxlasync_csis0_fimc");
	if (IS_ERR(sclk_pxlasync_csis0_fimc)) {
		pr_err("%s : clk_get(sclk_pxlasync_csis0_fimc) failed\n", __func__);
		return PTR_ERR(sclk_pxlasync_csis0_fimc);
	}
	clk_set_parent(sclk_pxlasync_csis0_fimc, mout_isp_pll);
	clk_set_rate(sclk_pxlasync_csis0_fimc, 160 * 1000000);
	pr_info("sclk_pxlasync_csis0_fimc : %ld\n", clk_get_rate(sclk_pxlasync_csis0_fimc));
	clk_put(sclk_pxlasync_csis0_fimc);

	sclk_pxlasync_csis1_fimc = clk_get(&pdev->dev, "sclk_pxlasync_csis1_fimc");
	if (IS_ERR(sclk_pxlasync_csis1_fimc)) {
		pr_err("%s : clk_get(sclk_pxlasync_csis1_fimc) failed\n", __func__);
		return PTR_ERR(sclk_pxlasync_csis1_fimc);
	}
	clk_set_parent(sclk_pxlasync_csis1_fimc, mout_isp_pll);
	clk_set_rate(sclk_pxlasync_csis1_fimc, 160 * 1000000);
	pr_info("sclk_pxlasync_csis1_fimc : %ld\n", clk_get_rate(sclk_pxlasync_csis1_fimc));
	clk_put(sclk_pxlasync_csis1_fimc);

	clk_put(mout_isp_pll);

	dout_sclk_csis1 = clk_get(&pdev->dev, "dout_sclk_csis1");
	if (IS_ERR(dout_sclk_csis1)) {
		pr_err("%s : clk_get(dout_sclk_csis1) failed\n", __func__);
		return PTR_ERR(dout_sclk_csis1);
	}
	clk_set_rate(dout_sclk_csis1, 300 * 1000000);
	pr_info("dout_sclk_csis1 : %ld\n", clk_get_rate(dout_sclk_csis1));
	clk_put(dout_sclk_csis1);

	dout_aclk_3aa1 = clk_get(&pdev->dev, "dout_aclk_3aa1");
	if (IS_ERR(dout_aclk_3aa1)) {
		pr_err("%s : clk_get(dout_aclk_3aa1) failed\n", __func__);
		return PTR_ERR(dout_aclk_3aa1);
	}
	clk_set_rate(dout_aclk_3aa1, 300 * 1000000);
	pr_info("dout_aclk_3aa1 : %ld\n", clk_get_rate(dout_aclk_3aa1));
	clk_put(dout_aclk_3aa1);

	dout_aclk_lite_b = clk_get(&pdev->dev, "dout_aclk_lite_b");
	if (IS_ERR(dout_aclk_lite_b)) {
		pr_err("%s : clk_get(dout_aclk_lite_b) failed\n", __func__);
		return PTR_ERR(dout_aclk_lite_b);
	}
	clk_set_rate(dout_aclk_lite_b, 300 * 1000000);
	pr_info("dout_aclk_lite_b : %ld\n", clk_get_rate(dout_aclk_lite_b));
	clk_put(dout_aclk_lite_b);

	dout_pclk_isp0_a_150 = clk_get(&pdev->dev, "dout_pclk_isp0_a_150");
	if (IS_ERR(dout_pclk_isp0_a_150)) {
		pr_err("%s : clk_get(dout_pclk_isp0_a_150) failed\n", __func__);
		return PTR_ERR(dout_pclk_isp0_a_150);
	}
	clk_set_rate(dout_pclk_isp0_a_150, 150* 1000000);
	pr_info("dout_pclk_isp0_a_150 : %ld\n", clk_get_rate(dout_pclk_isp0_a_150));
	clk_put(dout_pclk_isp0_a_150);

	dout_pclk_isp0_b_150 = clk_get(&pdev->dev, "dout_pclk_isp0_b_150");
	if (IS_ERR(dout_pclk_isp0_b_150)) {
		pr_err("%s : clk_get(dout_pclk_isp0_b_150) failed\n", __func__);
		return PTR_ERR(dout_pclk_isp0_b_150);
	}
	clk_set_rate(dout_pclk_isp0_b_150, 150* 1000000);
	pr_info("dout_pclk_isp0_b_150 : %ld\n", clk_get_rate(dout_pclk_isp0_b_150));
	clk_put(dout_pclk_isp0_b_150);

	dout_pclk_isp0_b_75 = clk_get(&pdev->dev, "dout_pclk_isp0_b_75");
	if (IS_ERR(dout_pclk_isp0_b_75)) {
		pr_err("%s : clk_get(dout_pclk_isp0_b_75) failed\n", __func__);
		return PTR_ERR(dout_pclk_isp0_b_75);
	}
	clk_set_rate(dout_pclk_isp0_b_75, 75* 1000000);
	pr_info("dout_pclk_isp0_b_75 : %ld\n", clk_get_rate(dout_pclk_isp0_b_75));
	clk_put(dout_pclk_isp0_b_75);

	/* aclk_isp_400 */
	aclk_isp0_400 = clk_get(&pdev->dev, "aclk_isp0_400");
	if (IS_ERR(aclk_isp0_400)) {
		pr_err("%s : clk_get(aclk_isp0_400) failed\n", __func__);
		return PTR_ERR(aclk_isp0_400);
	}
	aclk_isp0_400_pre = clk_get(&pdev->dev, "aclk_isp0_400_pre");
	if (IS_ERR(aclk_isp0_400_pre)) {
		pr_err("%s : clk_get(aclk_isp0_400_pre) failed\n", __func__);
		clk_put(aclk_isp0_400);
		return PTR_ERR(aclk_isp0_400_pre);
	}
	mout_mpll_user_top = clk_get(&pdev->dev, "mout_mpll_user_top");
	if (IS_ERR(mout_mpll_user_top)) {
		pr_err("%s : clk_get(mout_mpll_user_top) failed\n", __func__);
		clk_put(aclk_isp0_400);
		clk_put(aclk_isp0_400_pre);
		return PTR_ERR(mout_mpll_user_top);
	}
	clk_set_parent(aclk_isp0_400, aclk_isp0_400_pre);
	clk_set_parent(aclk_isp0_400_pre, mout_mpll_user_top);
	clk_set_rate(aclk_isp0_400_pre, 400 * 1000000);
	pr_info("aclk_isp0_400_pre : %ld\n", clk_get_rate(aclk_isp0_400_pre));
	clk_put(aclk_isp0_400);
	clk_put(aclk_isp0_400_pre);
	clk_put(mout_mpll_user_top);

	dout_pclk_isp1_150 = clk_get(&pdev->dev, "dout_pclk_isp1_150");
	if (IS_ERR(dout_pclk_isp1_150)) {
		pr_err("%s : clk_get(dout_pclk_isp1_150) failed\n", __func__);
		return PTR_ERR(dout_pclk_isp1_150);
	}
	clk_set_rate(dout_pclk_isp1_150, 150* 1000000);
	pr_info("dout_pclk_isp1_150 : %ld\n", clk_get_rate(dout_pclk_isp1_150));
	clk_put(dout_pclk_isp1_150);

	dout_pclk_isp1_75 = clk_get(&pdev->dev, "dout_pclk_isp1_75");
	if (IS_ERR(dout_pclk_isp1_75)) {
		pr_err("%s : clk_get(dout_pclk_isp1_75) failed\n", __func__);
		return PTR_ERR(dout_pclk_isp1_75);
	}
	clk_set_rate(dout_pclk_isp1_75, 75* 1000000);
	pr_info("dout_pclk_isp1_75 : %ld\n", clk_get_rate(dout_pclk_isp1_75));
	clk_put(dout_pclk_isp1_75);

#ifdef CLOCK_OUT_DEBUG
	/*
	 * clock out top debug
	 * writel(((1 << 16) | 0x18), EXYNOS4415_CLKOUT_CMU_TOP);
	 * writel((0x1 << 8), EXYNOS_PMU_DEBUG);
	 */

	/* clock out isp sub debug */
	writel(((1 << 16) | 0x5), EXYNOS4415_CLKOUT_CMU_ISP);
	writel((0x5 << 8), EXYNOS_PMU_DEBUG);
	while(1) {
		pr_info(".");
		msleep(1000);
	}
#endif

	return 0;
}

static int exynos4415_fimc_is_clk_on(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int exynos4415_fimc_is_clk_off(struct platform_device *pdev)
{
	struct clk *aclk_isp0_300 = NULL;
	struct clk *aclk_isp1_300 = NULL;
	struct clk *ext_xtal = NULL;

	pr_debug("%s\n", __func__);

	ext_xtal = clk_get(&pdev->dev, "ext_xtal");
	if (IS_ERR(ext_xtal)) {
		pr_err("%s : clk_get(ext_xtal) failed\n", __func__);
		return PTR_ERR(ext_xtal);
	}

	aclk_isp0_300 = clk_get(&pdev->dev, "aclk_isp0_300");
	if (IS_ERR(aclk_isp0_300)) {
		pr_err("%s : clk_get(aclk_isp0_300) failed\n", __func__);
		return PTR_ERR(aclk_isp0_300);
	}

	clk_set_parent(aclk_isp0_300, ext_xtal);
	clk_put(aclk_isp0_300);

	aclk_isp1_300 = clk_get(&pdev->dev, "aclk_isp1_300");
	if (IS_ERR(aclk_isp1_300)) {
		pr_err("%s : clk_get(aclk_isp1_300) failed\n", __func__);
		return PTR_ERR(aclk_isp1_300);
	}

	clk_set_parent(aclk_isp1_300, ext_xtal);
	clk_put(aclk_isp1_300);

	clk_put(ext_xtal);

	return 0;
}

static int exynos4415_fimc_is_print_cfg(struct platform_device *pdev, u32 channel)
{
	return 0;
}

static int exynos4415_fimc_is_print_pwr(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	return 0;
}
#endif

int exynos_fimc_is_cfg_clk(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS4415)
	return exynos4415_fimc_is_cfg_clk(pdev);
#elif defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_cfg_clk(pdev);
#else
	pr_err("%s : can't find!! \n", __func__);
	return -ENODEV;
#endif
}

int exynos_fimc_is_clk_on(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS4415)
	return exynos4415_fimc_is_clk_on(pdev);
#elif defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_clk_on(pdev);
#else
	pr_err("%s : can't find!! \n", __func__);
	return -ENODEV;
#endif
}

int exynos_fimc_is_clk_off(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS4415)
	return exynos4415_fimc_is_clk_off(pdev);
#elif defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_clk_off(pdev);
#else
	pr_err("%s : can't find!! \n", __func__);
	return -ENODEV;
#endif
}

int exynos_fimc_is_print_cfg(struct platform_device *pdev, u32 channel)
{
#if defined(CONFIG_SOC_EXYNOS4415)
	return exynos4415_fimc_is_print_cfg(pdev, channel);
#elif defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_print_cfg(pdev, channel);
#else
	pr_err("%s : can't find!! \n", __func__);
	return -ENODEV;
#endif
}

int exynos_fimc_is_print_pwr(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS4415)
	return exynos4415_fimc_is_print_pwr(pdev);
#elif defined(CONFIG_SOC_EXYNOS5260)
	return exynos5260_fimc_is_print_pwr(pdev);
#else
	pr_err("%s : can't find!! \n", __func__);
	return -ENODEV;
#endif
}

int exynos_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	return 0;
}

int exynos_fimc_is_set_user_clk_gate(u32 group_id,
		bool is_on,
		u32 user_scenario_id,
		unsigned long msk_state,
		struct exynos_fimc_is_clk_gate_info *gate_info) {
	return 0;
}
