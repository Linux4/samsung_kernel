// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * gpio and clock configurations
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
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
#include <linux/io.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/clk-provider.h>
#include <linux/clkdev.h>

#include <exynos-is.h>
#include <is-config.h>
#include "is-resourcemgr.h"
#include "pablo-debug.h"

/*------------------------------------------------------*/
/*		Common control				*/
/*------------------------------------------------------*/

#define REGISTER_CLK(name) {name, NULL}

#define NUM_CIS_CLK		7
#define NUM_LINK_CLK		7
#define NUM_CSIS_DMA_CLK	1
#define CHAIN_CLK_OFS		((NUM_CIS_CLK * 2) + NUM_LINK_CLK + NUM_CSIS_DMA_CLK)

static struct is_clk is_clk_list[] = {
	/* Sensor MCLKs */
	REGISTER_CLK("DOUT_DIV_CLKCMU_CIS_CLK0"),
	REGISTER_CLK("DOUT_DIV_CLKCMU_CIS_CLK1"),
	REGISTER_CLK("DOUT_DIV_CLKCMU_CIS_CLK2"),
	REGISTER_CLK("DOUT_DIV_CLKCMU_CIS_CLK3"),
	REGISTER_CLK("DOUT_DIV_CLKCMU_CIS_CLK4"),
	REGISTER_CLK("DOUT_DIV_CLKCMU_CIS_CLK5"),
	REGISTER_CLK("DOUT_DIV_CLKCMU_CIS_CLK6"),

	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CIS_CLK0"),
	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CIS_CLK1"),
	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CIS_CLK2"),
	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CIS_CLK3"),
	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CIS_CLK4"),
	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CIS_CLK5"),
	REGISTER_CLK("GATE_DFTMUX_CMU_QCH_CIS_CLK6"),

	/* MIPI-PHYs & CSIS Links */
	REGISTER_CLK("GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS0"),
	REGISTER_CLK("GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS1"),
	REGISTER_CLK("GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS2"),
	REGISTER_CLK("GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS3"),
	REGISTER_CLK("GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS4"),
	REGISTER_CLK("GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS5"),
	REGISTER_CLK("GATE_MIPI_PHY_LINK_WRAP_QCH_CSIS6"),

	/* CSIS WDMA */
	REGISTER_CLK("GATE_CSIS_PDP_QCH_DMA"),

	/* PDP */
	REGISTER_CLK("GATE_CSIS_PDP_QCH_PDP"),

	/* CSTATs */
	REGISTER_CLK("GATE_OTF0_CSIS_CSTAT_QCH"),
	REGISTER_CLK("GATE_OTF1_CSIS_CSTAT_QCH"),
	REGISTER_CLK("GATE_OTF2_CSIS_CSTAT_QCH"),
	REGISTER_CLK("GATE_OTF3_CSIS_CSTAT_QCH"),

	/* ISCHAIN */
	REGISTER_CLK("GATE_BRP_QCH"),
	REGISTER_CLK("GATE_MCFP_QCH"),
	REGISTER_CLK("GATE_IP_DLFE_QCH"),
	REGISTER_CLK("GATE_YUVP_QCH"),
	REGISTER_CLK("GATE_MCSC_QCH"),
	REGISTER_CLK("GATE_LME_QCH_0"),
};


int is_set_rate(struct device *dev,
	const char *name, ulong frequency)
{
	int ret = 0;
	size_t index;
	struct clk *clk = NULL;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name))
			clk = is_clk_list[index].clk;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return -EINVAL;
	}

	ret = clk_set_rate(clk, frequency);
	if (ret) {
		pr_err("[@][ERR] %s: clk_set_rate is fail(%s)(ret: %d)\n", __func__, name, ret);
		return ret;
	}

	/* is_get_rate_dt(dev, name); */

	return ret;
}

/* utility function to get rate with DT */
ulong is_get_rate(struct device *dev,
	const char *name)
{
	ulong frequency;
	size_t index;
	struct clk *clk = NULL;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name))
			clk = is_clk_list[index].clk;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return 0;
	}

	frequency = clk_get_rate(clk);

	pr_info("[@] %s : %ldMhz (enabled:%d)\n", name, frequency/1000000, __clk_is_enabled(clk));

	return frequency;
}

/**
 * is_get_hw_rate - Return the lately calculated clock rate
 *		that is stored in clk_core structure.
 * @dev: device this lookup is bound
 * @name: format string describing clock name
 *
 * Just return the lately calculated clock rate that is stored in clk_hw structure.
 * It does not guarantee that has currently working clock rate.
 * To get it, call 'is_get_rate()' or 'is_get_rate_dt()' in advance.
 */
ulong is_get_hw_rate(struct device *dev, const char *name)
{
	size_t index;
	struct clk *clk = NULL;
	struct clk_hw *clk_hw;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name)) {
			clk = is_clk_list[index].clk;
			break;
		}
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return 0;
	}

	clk_hw = __clk_get_hw(clk);
	if (IS_ERR_OR_NULL(clk_hw)) {
		pr_err("[@][ERR] %s: clk_hw is NULL : %s\n", __func__, name);
		return 0;
	}

	return clk_hw_get_rate(clk_hw);
}

/* utility function to eable with DT */
int  is_enable(struct device *dev,
	const char *name)
{
	int ret = 0;
	size_t index;
	struct clk *clk = NULL;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name))
			clk = is_clk_list[index].clk;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return -EINVAL;
	}

	if (is_get_debug_param(IS_DEBUG_PARAM_CLK) > 1)
		pr_info("[@][ENABLE] %s : (is_enabled : %d)\n", name, __clk_is_enabled(clk));

	ret = clk_prepare_enable(clk);
	if (ret) {
		pr_err("[@][ERR] %s: clk_prepare_enable is fail(%s)(ret: %d)\n", __func__, name, ret);
		return ret;
	}

	return ret;
}

/* utility function to disable with DT */
int is_disable(struct device *dev,
	const char *name)
{
	size_t index;
	struct clk *clk = NULL;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++) {
		if (!strcmp(name, is_clk_list[index].name))
			clk = is_clk_list[index].clk;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("[@][ERR] %s: clk_target_list is NULL : %s\n", __func__, name);
		return -EINVAL;
	}

	clk_disable_unprepare(clk);

	if (is_get_debug_param(IS_DEBUG_PARAM_CLK) > 1)
		pr_info("[@][DISABLE] %s : (is_enabled : %d)\n", name, __clk_is_enabled(clk));

	return 0;
}

int is_enabled_clk_disable(struct device *dev, const char *name)
{
	int i;
	struct clk *clk = NULL;
	unsigned int enable_count;

	for (i = 0; i < ARRAY_SIZE(is_clk_list); i++) {
		if (!strcmp(name, is_clk_list[i].name))
			clk = is_clk_list[i].clk;
	}

	if (IS_ERR_OR_NULL(clk)) {
		pr_err("%s: failed to find clk: %s\n", __func__, name);
		return -EINVAL;
	}

	enable_count = __clk_is_enabled(clk);
	if (enable_count) {
		pr_err("%s: abnormal clock state is detected: %s, (is_enabled: %d)\n",
				__func__, name, enable_count);

		for (i = 0; i < enable_count; i++)
			clk_disable_unprepare(clk);
	}

	return 0;
}

/* utility function to set parent with DT */
int is_set_parent_dt(struct device *dev,
	const char *child, const char *parent)
{
	int ret = 0;
	struct clk *p;
	struct clk *c;

	p = clk_get(dev, parent);
	if (IS_ERR_OR_NULL(p)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, parent);
		return -EINVAL;
	}

	c = clk_get(dev, child);
	if (IS_ERR_OR_NULL(c)) {
		clk_put(p);
		pr_err("%s: could not lookup clock : %s\n", __func__, child);
		return -EINVAL;
	}

	ret = clk_set_parent(c, p);
	if (ret) {
		clk_put(c);
		clk_put(p);
		pr_err("%s: clk_set_parent is fail(%s -> %s)(ret: %d)\n", __func__, child, parent, ret);
		return ret;
	}

	clk_put(c);
	clk_put(p);

	return 0;
}

/* utility function to set rate with DT */
int is_set_rate_dt(struct device *dev,
	const char *conid, unsigned int rate)
{
	int ret = 0;
	struct clk *target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_set_rate(target, rate);
	if (ret) {
		pr_err("%s: clk_set_rate is fail(%s)(ret: %d)\n", __func__, conid, ret);
		return ret;
	}

	is_get_rate_dt(dev, conid);

	clk_put(target);

	return 0;
}

/* utility function to get rate with DT */
ulong is_get_rate_dt(struct device *dev,
	const char *conid)
{
	struct clk *target;
	ulong rate_target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return 0;
	}

	rate_target = clk_get_rate(target);

	clk_put(target);

	pr_info("[@] %s : %ldMhz\n", conid, rate_target/1000000);

	return rate_target;
}

/* utility function to eable with DT */
int is_enable_dt(struct device *dev,
	const char *conid)
{
	int ret;
	struct clk *target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	ret = clk_prepare(target);
	if (ret) {
		pr_err("%s: clk_prepare is fail(%s)(ret: %d)\n", __func__, conid, ret);
		return ret;
	}

	ret = clk_enable(target);
	if (ret) {
		pr_err("%s: clk_enable is fail(%s)(ret: %d)\n", __func__, conid, ret);
		return ret;
	}

	clk_put(target);

	return 0;
}

/* utility function to disable with DT */
int is_disable_dt(struct device *dev,
	const char *conid)
{
	struct clk *target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return -EINVAL;
	}

	clk_disable(target);
	clk_unprepare(target);
	clk_put(target);

	return 0;
}

static int exynos_is_print_clk(struct device *dev)
{
	u32 i;

	pr_info("PABLO-IS CLOCK DUMP\n");

	for (i = 0; i < ARRAY_SIZE(is_clk_list); i++)
		is_get_rate_dt(dev, is_clk_list[i].name);

	return 0;
}

/* utility function to get rate with DT */
void is_dump_rate_dt(struct device *dev,
	const char *conid)
{
	struct clk *target;
	ulong rate_target;

	target = clk_get(dev, conid);
	if (IS_ERR_OR_NULL(target)) {
		pr_err("%s: could not lookup clock : %s\n", __func__, conid);
		return;
	}

	rate_target = clk_get_rate(target);

	clk_put(target);

	cinfo("[@] %s : %ldMhz\n", conid, rate_target/1000000);
	return;
}

static int exynos_is_dump_clk(struct device *dev)
{
#if defined(ENABLE_DBG_CLK_PRINT)
	u32 i;

	if (!IS_ENABLED(CLOGGING))
		return 0;

	cinfo("### PABLO-IS CLOCK DUMP ###\n");

	for (i = 0; i < ARRAY_SIZE(is_clk_list); i++)
		is_dump_rate_dt(dev, is_clk_list[i].name);
#endif

	return 0;
}

static int exynos_is_clk_get(struct device *dev)
{
	struct clk *clk;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(is_clk_list); i++) {
		clk = devm_clk_get(dev, is_clk_list[i].name);
		if (IS_ERR_OR_NULL(clk)) {
			pr_err("[@][ERR] %s: could not lookup clock : %s\n",
				__func__, is_clk_list[i].name);
			return -EINVAL;
		}

		is_clk_list[i].clk = clk;
	}

	return 0;
}

static int exynos_is_clk_cfg(struct device *dev)
{
	return 0;
}

static int exynos_is_clk_on(struct device *dev)
{
	int ret;
	u32 i;
	struct exynos_platform_is *pdata;

	pdata = dev_get_platdata(dev);
	if (pdata->clock_on) {
		ret = pdata->clk_off(dev);
		if (ret) {
			pr_err("clk_off is fail(%d)\n", ret);
			goto p_err;
		}
	}

	/* Enable from PDP to MCSC & LME */
	for (i = CHAIN_CLK_OFS; i < ARRAY_SIZE(is_clk_list); i++)
		is_enable(dev, is_clk_list[i].name);

	if (is_get_debug_param(IS_DEBUG_PARAM_CLK) > 1)
		exynos_is_print_clk(dev);

	pdata->clock_on = true;

p_err:
	return 0;
}

static int exynos_is_clk_off(struct device *dev)
{
	int ret;
	u32 i;
	struct exynos_platform_is *pdata;

	pdata = dev_get_platdata(dev);
	if (!pdata->clock_on) {
		pr_err("clk_off is fail(already off)\n");
		ret = -EINVAL;
		goto p_err;
	}

	/* Enable from PDP to MCSC & LME */
	for (i = CHAIN_CLK_OFS; i < ARRAY_SIZE(is_clk_list); i++)
		is_disable(dev, is_clk_list[i].name);

	pdata->clock_on = false;

p_err:
	return 0;
}

/**
 * is_clk_get_rate - Call 'clk_get_rate()' for every clock
 * @dev: device that has clocks
 *
 * By calling 'clk_get_rate()' for each clock source,
 * it get rate changes via clock tree traversal.
 */
static int exynos_is_clk_get_rate(struct device *dev)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(is_clk_list); index++)
		clk_get_rate(is_clk_list[index].clk);

	return 0;
}

void is_get_clk_ops(struct exynos_platform_is *pdata)
{
	pdata->clk_get = exynos_is_clk_get;
	pdata->clk_cfg = exynos_is_clk_cfg;
	pdata->clk_on = exynos_is_clk_on;
	pdata->clk_off = exynos_is_clk_off;
	pdata->clk_get_rate = exynos_is_clk_get_rate;
	pdata->clk_print = exynos_is_print_clk;
	pdata->clk_dump = exynos_is_dump_clk;
}
