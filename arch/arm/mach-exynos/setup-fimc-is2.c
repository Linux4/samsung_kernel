/* linux/arch/arm/mach-exynos/setup-fimc-is2.c
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
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <mach/map.h>
#include <mach/regs-clock.h>
#include <plat/map-s5p.h>
#include <plat/cpu.h>
#include <mach/exynos-fimc-is2.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

struct platform_device; /* don't need the contents */

/*------------------------------------------------------*/
/*		Common control				*/
/*------------------------------------------------------*/

#define PRINT_CLK(c, n) printk(KERN_DEBUG "%s : 0x%08X\n", n, readl(c));

int exynos_fimc_is_print_cfg(struct platform_device *pdev, u32 channel)
{
	pr_debug("%s\n", __func__);

	return 0;
}

/* utility function to set rate with DT */
#if 0
int fimc_is_set_rate(struct platform_device *pdev,
	const char *conid, unsigned int rate)
{
	int ret = 0;
	int id;
	struct clk *target;

	for ( id = 0; id < CLK_NUM; id++ ) {
		if (!strcmp(conid, clk_g_list[id]))
			target = clk_target_list[id];
	}

	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: clk_target_list is NULL : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_set_rate(target, rate);
	if (ret) {
		pr_err("%s: clk_set_rate is fail(%s)\n", __func__, conid);
		return ret;
	}

	/* fimc_is_get_rate_dt(pdev, conid); */

	return 0;
}

/* utility function to get rate with DT */
int  fimc_is_get_rate(struct platform_device *pdev,
	const char *conid)
{
	int id;
	struct clk *target;
	unsigned int rate_target;

	for ( id = 0; id < CLK_NUM; id++ ) {
		if (!strcmp(conid, clk_g_list[id]))
			target = clk_target_list[id];
	}

	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: clk_target_list is NULL : %s\n", __func__, conid);
		return -EINVAL;
	}

	rate_target = clk_get_rate(target);
	pr_info("%s : %dMhz\n", conid, rate_target/1000000);

	return rate_target;
}
#endif
/* utility function to eable with DT */
int  fimc_is_enable(struct platform_device *pdev,
	const char *conid)
{
	int ret;
	int id;
	struct clk *target = NULL;

	for ( id = 0; id < CLK_NUM; id++ ) {
		if (!strcmp(conid, clk_g_list[id]))
			target = clk_target_list[id];
	}

	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: clk_target_list is NULL : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_prepare(target);
	if (ret) {
		pr_err("%s: clk_prepare is fail(%s)\n", __func__, conid);
		return ret;
	}

	ret = clk_enable(target);
	if (ret) {
		pr_err("%s: clk_enable is fail(%s)\n", __func__, conid);
		return ret;
	}

	return 0;
}

/* utility function to disable with DT */
int fimc_is_disable(struct platform_device *pdev,
	const char *conid)
{
	int id;
	struct clk *target = NULL;

	for ( id = 0; id < CLK_NUM; id++ ) {
		if (!strcmp(conid, clk_g_list[id]))
			target = clk_target_list[id];
	}

	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: clk_target_list is NULL : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_disable(target);
	clk_unprepare(target);

	return 0;
}

/* utility function to set parent with DT */
int fimc_is_set_parent_dt(struct platform_device *pdev,
	const char *child, const char *parent)
{
	int ret = 0;
	struct clk *p;
	struct clk *c;

	p = clk_get(&pdev->dev, parent);
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, parent);
		return -EINVAL;
	}

	c = clk_get(&pdev->dev, child);
	if (IS_ERR_OR_NULL(c)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return -EINVAL;
	}

	ret = clk_set_parent(c, p);
	if (ret) {
		pr_err("%s: clk_set_parent is fail(%s -> %s)\n", __func__, child, parent);
		return ret;
	}

	return 0;
}

/* utility function to set rate with DT */
int fimc_is_set_rate_dt(struct platform_device *pdev,
	const char *conid, unsigned int rate)
{
	int ret = 0;
	struct clk *target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_set_rate(target, rate);
	if (ret) {
		pr_err("%s: clk_set_rate is fail(%s)\n", __func__, conid);
		return ret;
	}

	/* fimc_is_get_rate_dt(pdev, conid); */

	return 0;
}

/* utility function to get rate with DT */
int  fimc_is_get_rate_dt(struct platform_device *pdev,
	const char *conid)
{
	struct clk *target;
	unsigned int rate_target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	rate_target = clk_get_rate(target);
	printk(KERN_INFO "%s : %dMhz\n", conid, rate_target/1000000);

	return rate_target;
}

/* utility function to eable with DT */
int  fimc_is_enable_dt(struct platform_device *pdev,
	const char *conid)
{
	int ret;
	struct clk *target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_prepare(target);
	if (ret) {
		pr_err("%s: clk_prepare is fail(%s)\n", __func__, conid);
		return ret;
	}

	ret = clk_enable(target);
	if (ret) {
		pr_err("%s: clk_enable is fail(%s)\n", __func__, conid);
		return ret;
	}

	return 0;
}

/* utility function to disable with DT */
int fimc_is_disable_dt(struct platform_device *pdev,
	const char *conid)
{
	struct clk *target;

	target = clk_get(&pdev->dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_disable(target);
	clk_unprepare(target);

	return 0;
}

#if defined(CONFIG_SOC_EXYNOS3475)
int exynos3475_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
	pr_debug("%s\n", __func__);

	return 0;
}

int exynos3475_fimc_is_cfg_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	/* USER_MUX_SEL to OSCCLK */
	fimc_is_enable_dt(pdev, "mout_aclk_isp_300_user");
	fimc_is_disable_dt(pdev, "mout_aclk_isp_300_user");

	/* PCLK_ISP DIV MAX */
	fimc_is_set_rate_dt(pdev, "dout_pclk_isp_150", 1);

	/** CMU_ISP **/

	/* USER_MUX_SEL */
	fimc_is_enable_dt(pdev, "mout_aclk_isp_300_user");
	/* PCLK_ISP DIV SET */
	fimc_is_set_rate_dt(pdev, "dout_pclk_isp_150", 148 * 1000000);

	return 0;
}

int exynos3475_fimc_is_clk_on(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	/* GATE CLOCK ON */
	fimc_is_enable_dt(pdev, "aclk_isp_300_aclk_isp_d");
	fimc_is_enable_dt(pdev, "aclk_isp_300_aclk_ppmu");
	fimc_is_enable_dt(pdev, "aclk_isp_300_aclk_fd");
	fimc_is_enable_dt(pdev, "aclk_isp_300_aclk_is");

	return 0;
}

int exynos3475_fimc_is_clk_off(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	/* GATE CLOCK OFF */
	fimc_is_disable_dt(pdev, "aclk_isp_300_aclk_isp_d");
	fimc_is_disable_dt(pdev, "aclk_isp_300_aclk_ppmu");
	fimc_is_disable_dt(pdev, "aclk_isp_300_aclk_fd");
	fimc_is_disable_dt(pdev, "aclk_isp_300_aclk_is");

	return 0;
}

int exynos3475_fimc_is_print_clk(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	/** CMU_ISP **/
	fimc_is_get_rate_dt(pdev, "mout_aclk_isp_300_user");
	fimc_is_get_rate_dt(pdev, "aclk_isp_300_aclk_is");
	fimc_is_get_rate_dt(pdev, "aclk_isp_300_aclk_fd");
	fimc_is_get_rate_dt(pdev, "aclk_isp_300_aclk_ppmu");
	fimc_is_get_rate_dt(pdev, "aclk_isp_300_aclk_isp_d");

	fimc_is_get_rate_dt(pdev, "dout_pclk_isp_150");

	return 0;
}
#endif

/* Wrapper functions */
int exynos_fimc_is_cfg_clk(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_cfg_clk(pdev);
#endif
	return 0;
}

int exynos_fimc_is_cfg_cam_clk(struct platform_device *pdev)
{
	return 0;
}

int exynos_fimc_is_clk_on(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_clk_on(pdev);
#endif
	return 0;
}

int exynos_fimc_is_clk_off(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_clk_off(pdev);
#endif
	return 0;
}

int exynos_fimc_is_print_clk(struct platform_device *pdev)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_print_clk(pdev);
#endif
	return 0;
}

int exynos_fimc_is_set_user_clk_gate(u32 group_id, bool is_on,
	u32 user_scenario_id,
	unsigned long msk_state,
	struct exynos_fimc_is_clk_gate_info *gate_info)
{
	return 0;
}

int exynos_fimc_is_clk_gate(u32 clk_gate_id, bool is_on)
{
#if defined(CONFIG_SOC_EXYNOS3475)
	exynos3475_fimc_is_clk_gate(clk_gate_id, is_on);
#endif
	return 0;
}

#if !defined(CONFIG_SOC_EXYNOS7420)
int exynos_fimc_is_print_pwr(struct platform_device *pdev)
{
	return 0;
}
#endif
