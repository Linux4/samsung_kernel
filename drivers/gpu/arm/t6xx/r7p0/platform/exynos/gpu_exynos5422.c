/* drivers/gpu/t6xx/kbase/src/platform/gpu_exynos7580.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_exynos7580.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>

#include <mach/asv-exynos.h>
#ifdef MALI64_PORTING
#include <mach/asv-exynos7_cal.h>
#endif
#include <mach/pm_domains.h>
#include <mach/regs-pmu.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"
#include "../mali_midg_regmap.h"

#define L2CONFIG_MO_1BY8			(0b0101)
#define L2CONFIG_MO_1BY4			(0b1010)
#define L2CONFIG_MO_1BY2			(0b1111)
#define L2CONFIG_MO_NO_RESTRICT			(0)

#define CPU_MAX PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE

/*  clk,vol,abb,min,max,down stay, pm_qos mem,
	pm_qos int, pm_qos cpu_kfc_min, pm_qos cpu_egl_max */
static gpu_dvfs_info gpu_dvfs_table_default[] = {
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{543, 1037500, 0,  99, 100, 1, 0, 825000, 400000, 1000000, 1600000},
#else
	{533, 1037500, 0,  99, 100, 1, 0, 825000, 400000, 1000000, 1600000},
#endif
	{480, 1000000, 0,  98,  99, 1, 0, 825000, 400000, 1000000, 1600000},
	{420, 962500,  0,  78,  99, 1, 0, 825000, 400000, 	0, CPU_MAX},
	{350, 912500,  0,  70,  90, 1, 0, 728000, 333000, 	0, CPU_MAX},
	{266, 862500,  0,  60,  90, 1, 0, 413000, 222000,       0, CPU_MAX},
	{177, 812500,  0,   0,  90, 2, 0, 165000, 83000,        0, CPU_MAX},
};

static int mif_min_table[] = {
	200000, 273000, 228000,
	416000, 559000, 667000,
	728000, 741000, 825000,
};

static gpu_attribute gpu_config_attributes[] = {
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{GPU_MAX_CLOCK, 543},
	{GPU_MAX_CLOCK_LIMIT, 543},
#else
	{GPU_MAX_CLOCK, 533},
	{GPU_MAX_CLOCK_LIMIT, 533},
#endif
	{GPU_MIN_CLOCK, 177},
	{GPU_DVFS_START_CLOCK, 177},
	{GPU_DVFS_BL_CONFIG_CLOCK, 177},
	{GPU_GOVERNOR_TYPE, G3D_DVFS_GOVERNOR_INTERACTIVE},
	{GPU_GOVERNOR_START_CLOCK_DEFAULT, 177},
	{GPU_GOVERNOR_START_CLOCK_INTERACTIVE, 177},
	{GPU_GOVERNOR_START_CLOCK_STATIC, 177},
	{GPU_GOVERNOR_START_CLOCK_BOOSTER, 177},
	{GPU_GOVERNOR_TABLE_DEFAULT, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_INTERACTIVE, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_STATIC, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_BOOSTER, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_SIZE_DEFAULT, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_INTERACTIVE, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_STATIC, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_BOOSTER, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_CLOCK, 420},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_LOAD, 95},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_DELAY, 0},
	{GPU_DEFAULT_VOLTAGE, 812500},
	{GPU_COLD_MINIMUM_VOL, 0},
	{GPU_VOLTAGE_OFFSET_MARGIN, 37500},
	{GPU_TMU_CONTROL, 1},
#ifdef CONFIG_SOC_EXYNOS5422_REV_0
	{GPU_TEMP_THROTTLING1, 543},
#else
	{GPU_TEMP_THROTTLING1, 533},
#endif
	{GPU_TEMP_THROTTLING2, 480},
	{GPU_TEMP_THROTTLING3, 420},
	{GPU_TEMP_THROTTLING4, 350},
	{GPU_TEMP_TRIPPING, 177},
	{GPU_BOOST_MIN_LOCK, 0},
	{GPU_BOOST_EGL_MIN_LOCK, 1300000},
	{GPU_POWER_COEFF, 46}, /* all core on param */
	{GPU_DVFS_TIME_INTERVAL, 5},
	{GPU_DEFAULT_WAKEUP_LOCK, 1},
	{GPU_BUS_DEVFREQ, 1},
	{GPU_DYNAMIC_ABB, 0},
	{GPU_EARLY_CLK_GATING, 0},
	{GPU_DVS, 0},
	{GPU_PERF_GATHERING, 0},
#ifdef MALI_SEC_HWCNT
	{GPU_HWCNT_GATHERING, 1},
	{GPU_HWCNT_POLLING_TIME, 90},
	{GPU_HWCNT_UP_STEP, 3},
	{GPU_HWCNT_DOWN_STEP, 2},
	{GPU_HWCNT_GPR, 1},
	{GPU_HWCNT_DUMP_PERIOD, 50}, /* ms */
	{GPU_HWCNT_CHOOSE_JM , 0},
	{GPU_HWCNT_CHOOSE_SHADER , 0x5FF}, /* since TEX_ISSUES offset is 29 so we need non-zero values from 27-30 offset
					      of each shader core and hence 0x560 is changed to 0x5E0 */
	{GPU_HWCNT_CHOOSE_TILER , 0},
	{GPU_HWCNT_CHOOSE_L3_CACHE , 0},
	{GPU_HWCNT_CHOOSE_MMU_L2 , 0},
#endif
	{GPU_RUNTIME_PM_DELAY_TIME, 50},
	{GPU_DVFS_POLLING_TIME, 10},
	{GPU_PMQOS_INT_DISABLE, 0},
	{GPU_PMQOS_MIF_MAX_CLOCK, 825000},
	{GPU_PMQOS_MIF_MAX_CLOCK_BASE, 420},
	{GPU_CL_DVFS_START_BASE, 600},
	{GPU_DEBUG_LEVEL, DVFS_WARNING},
	{GPU_TRACE_LEVEL, TRACE_ALL},
};

#define G3D_CORE_MASK_EFUSE_OFFSET	0x1DC
#define G3D_CORE_MASK_EFUSE_OFFSET2	0x1D4
#define G3D_CORE_MASK_EFUSE_OFFSET3	0x16C

#ifdef CONFIG_SOC_EXYNOS7420
#ifdef CONFIG_OF_RESERVED_MEM
#include <linux/of_reserved_mem.h>

static int __init iram_vector_reserved_mem_setup(struct reserved_mem *rmem)
{
	pr_debug("%s: reserved-mem\n", __func__);
	return 0;
}

RESERVEDMEM_OF_DECLARE(iram_vector, "mali,iram-vector",
			iram_vector_reserved_mem_setup);
#endif
#endif

int gpu_dvfs_decide_max_clock(struct exynos_context *platform)
{
	if (!platform)
		return -1;

	return 0;
}

void *gpu_get_config_attributes(void)
{
	return &gpu_config_attributes;
}

uintptr_t gpu_get_max_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MAX_CLOCK) * 1000;
}

uintptr_t gpu_get_min_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MIN_CLOCK) * 1000;
}
/*
struct clk *fin_pll;
struct clk *fout_g3d_pll;
struct clk *aclk_g3d;
struct clk *mout_g3d;
struct clk *dout_aclk_g3d;
static struct clk *aclk_g3d_400;
*/
struct clk *fout_vpll;
struct clk *mout_vpll_ctrl;
struct clk *mout_dpll_ctrl;
struct clk *mout_aclk_g3d;
struct clk *dout_aclk_g3d;
struct clk *mout_aclk_g3d_sw;
struct clk *mout_aclk_g3d_user;
struct clk *clk_g3d_ip;

#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator;
struct regulator *dvs_en_regulator;
#endif /* CONFIG_REGULATOR */

#ifdef CONFIG_MALI_EXYNOS_TRACE
	extern struct kbase_device *pkbdev;
#endif

int gpu_is_power_on(void)
{
	return ((__raw_readl(EXYNOS5422_G3D_STATUS) & EXYNOS_INT_LOCAL_PWR_EN) == EXYNOS_INT_LOCAL_PWR_EN) ? 1 : 0;
}

void gpu_power_set_reg(void)
{
	//__raw_writel(LOCAL_PWR_CFG, EXYNOS_PMU_G3D_CONFIGURATION);

	while (1) {
		if (gpu_is_power_on())
			break;
	}
}

int gpu_power_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform =
		(struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power initialized\n");

	return 0;
}

int gpu_get_cur_clock(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	if (!clk_g3d_ip) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: clock is not initialized\n", __func__);
		return -1;
	}

	return clk_get_rate(clk_g3d_ip)/MHZ;
}

int gpu_is_clock_on(void)
{
	return __clk_is_enabled(clk_g3d_ip);
}

static int gpu_clock_on(struct exynos_context *platform)
{
	int ret = 0;
	if (!platform)
		return -ENODEV;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u,
			"%s: can't set clock on in power off status\n",
			__func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 1) {
		ret = 0;
		goto err_return;
	}

	if (clk_g3d_ip) {
		(void) clk_prepare_enable(clk_g3d_ip);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u,
			"clock is enabled\n");
	}

	platform->clk_g3d_status = 1;

err_return:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}

static int gpu_clock_off(struct exynos_context *platform)
{
	int ret = 0;

	if (!platform)
		return -ENODEV;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set clock off in power off status\n",
			__func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 0)
	{
		ret = 0;
		goto err_return;
	}

	if (clk_g3d_ip) {
		(void)clk_disable_unprepare(clk_g3d_ip);
	#ifdef CONFIG_MALI_EXYNOS_TRACE
		KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_CLOCK_OFF, NULL, NULL, 0u, 0u);
	#endif
	}

	platform->clk_g3d_status = 0;

err_return:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}

int gpu_register_dump(void)
{
#ifdef CONFIG_MALI_EXYNOS_TRACE
	if (gpu_is_power_on()) {

		/* G3D PMU */
		KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_REGISTER_DUMP, NULL, NULL, 0x10044084, __raw_readl(EXYNOS5422_G3D_STATUS));

		/* G3D SRC */
		KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_REGISTER_DUMP, NULL, NULL, 0x10020208, __raw_readl(EXYNOS5_CLK_SRC_TOP2));
		KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_REGISTER_DUMP, NULL, NULL, 0x10020214, __raw_readl(EXYNOS5_CLK_SRC_TOP5));
		KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_REGISTER_DUMP, NULL, NULL, 0x10020288, __raw_readl(EXYNOS5_CLK_SRC_TOP12));

		/* G3D DIV */
		KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_REGISTER_DUMP, NULL, NULL, 0x10020608, __raw_readl(EXYNOS5_CLK_DIV_STAT_TOP2));

		/* G3D MUX */
		KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_REGISTER_DUMP, NULL, NULL, 0x10020414, __raw_readl(EXYNOS5_CLK_MUX_STAT_TOP5));
	}
#endif /* CONFIG_MALI_EXYNOS_TRACE */

	return 0;
}

static int gpu_update_clock(struct exynos_context *platform)
{
	if (!clk_g3d_ip) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"clk_g3d_ip is not initialized\n");
		return -1;
	}

	platform->cur_clock = clk_get_rate(clk_g3d_ip)/MHZ;
	return 0;
}

unsigned long get_dpll_freq(int curr, int targ)
{
	unsigned long dpll_clk;
	int divider;

	switch(targ)
	{
		case 480: case 420: case 350:
			divider = 2;
			break;
		case 266:
			divider = 2 + (targ < curr ? 0:1);
			break;
		case 177:
			divider = 3 + (targ < curr ? 0:1);
			break;
		case 100:
			divider = 4;
			break;
		default:
			divider = 1;
		 break;
	}
	dpll_clk = curr / divider + 5;

	return (dpll_clk*1000000);
}

static int gpu_set_clock(struct exynos_context *platform, int clk)
{
	long g3d_rate_prev = -1;
	unsigned long g3d_rate = clk * MHZ;
	unsigned long tmp = 0;
	int ret;

	if (clk_g3d_ip == 0)
		return -1;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"%s: can't set clock in the power-off state!\n",__func__);
		goto err;
	}

	if (!gpu_is_clock_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"%s: can't set clock in the clock-off state!\n",__func__);
		goto err;
	}

	g3d_rate_prev = clk_get_rate(fout_vpll)/MHZ;

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (clk != g3d_rate_prev) 
	{
		/*for stable clock input.*/
		ret = clk_set_rate(dout_aclk_g3d, get_dpll_freq(clk_get_rate(clk_g3d_ip)/1000000, clk));
		if (ret < 0) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"failed to clk_set_rate [dout_aclk_g3d]\n");
			goto err;
		}

		/*change here for future stable clock changing*/
		ret = clk_set_parent(mout_aclk_g3d, mout_dpll_ctrl);
		if (ret < 0) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"failed to clk_set_parent [mout_aclk_g3d]\n");
			goto err;
		}

		/*change g3d pll*/
		ret = clk_set_rate(fout_vpll, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"failed to clk_set_rate [fout_vpll]\n");
			goto err;
		}

		/*restore parent*/
		ret = clk_set_parent(mout_aclk_g3d, mout_vpll_ctrl);
		if (ret < 0) {
			GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"failed to clk_set_parent [mout_aclk_g3d]\n");
			goto err;
		}

		g3d_rate_prev = g3d_rate;
	}


	ret = clk_set_rate(dout_aclk_g3d, g3d_rate);
	if (ret < 0) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"failed to clk_set_rate [dout_aclk_g3d]\n");
		goto err;
	}

	/* Waiting for clock is stable */
	do {
		tmp = __raw_readl(EXYNOS5_CLK_DIV_STAT_TOP2);
	} while (tmp & 0x10000);

	gpu_update_clock(platform);

#ifdef CONFIG_MALI_EXYNOS_TRACE
	KBASE_TRACE_ADD_EXYNOS(pkbdev, LSI_CLOCK_VALUE, NULL, NULL, 0u, g3d_rate/MHZ);
#endif /* CONFIG_MALI_EXYNOS_TRACE */

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"[G3D] clock set: %ld\n", g3d_rate / MHZ);
	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,"[G3D] clock get: %d\n", platform->cur_clock);
err:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}
static int gpu_get_clock(struct kbase_device *kbdev)
{

	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	/*
	 * EXYNOS5422 3D clock description
	 * normal usage: mux(vpll) -> divider -> mux_sw -> mux_user -> aclk_g3d
	 * on clock changing: mux(dpll) -> divider(3) -> mux_sw -> mux_user -> aclk_g3d
	 */

	fout_vpll = clk_get(NULL, "fout_vpll");
	if (IS_ERR(fout_vpll)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "failed to clk_get [fout_vpll]\n");
		return -1;
	}

	mout_vpll_ctrl = clk_get(kbdev->dev, "mout_vpll_ctrl"); /* same as sclk_vpll */
	if (IS_ERR(mout_vpll_ctrl)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "failed to clk_set_parent [mout_aclk_g3d]\n");
		return -1;
	}

	mout_dpll_ctrl = clk_get(kbdev->dev, "mout_dpll_ctrl"); /* same as sclk_dpll */
	if (IS_ERR(mout_dpll_ctrl)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "failed to clk_get [mout_dpll_ctrl]\n");
		return -1;
	}

	mout_aclk_g3d = clk_get(kbdev->dev, "mout_aclk_g3d"); /* set parents v or d pll */
	if (IS_ERR(mout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "failed to clk_set_parent [mout_aclk_g3d]\n");
		return -1;
	}

	dout_aclk_g3d = clk_get(kbdev->dev, "dout_aclk_g3d"); /* divider usage */
	if (IS_ERR(dout_aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "failed to clk_get [dout_aclk_g3d]\n");
		return -1;
	}

	mout_aclk_g3d_sw = clk_get(kbdev->dev, "mout_aclk_g3d_sw");
	if (IS_ERR(mout_aclk_g3d_sw)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "failed to clk_get [mout_aclk_g3d_sw]\n");
		return -1;
	}

	mout_aclk_g3d_user = clk_get(kbdev->dev, "mout_aclk_g3d_user");
	if (IS_ERR(mout_aclk_g3d_user)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "failed to clk_get [mout_aclk_g3d_user]\n");
		return -1;
	}

	clk_g3d_ip = clk_get(kbdev->dev, "clk_g3d_ip");
	clk_prepare_enable(clk_g3d_ip);
	if (IS_ERR(clk_g3d_ip)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "failed to clk_get [clk_g3d_ip]\n");
		return -1;
	} else {
		platform->clk_g3d_status = 1;
	}

	//gpu_clock_on(platform);//check this nithin
	//__raw_writel(0x1, EXYNOS7580_MUX_SEL_G3D0);

	return 0;
}
int gpu_clock_init(struct kbase_device *kbdev)
{
	int ret;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;
	KBASE_DEBUG_ASSERT(kbdev != NULL);
	ret = gpu_get_clock(kbdev);
	if (ret < 0)	
		return -1;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "g3d clock initialized\n");
	return 0;
}

int gpu_get_cur_voltage(struct exynos_context *platform)
{
	int ret = 0;
#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: regulator is not initialized\n",
			__func__);
		return -1;
	}

	ret = regulator_get_voltage(g3d_regulator);
#endif /* CONFIG_REGULATOR */
	return ret;
}

static int gpu_set_voltage(struct exynos_context *platform, int vol)
{
	if (gpu_get_cur_voltage(platform) == vol)
		return 0;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set voltage in the power-off state!\n",
			__func__);
		return -1;
	}

#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: regulator is not initialized\n", __func__);
		return -1;
	}

#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	regulator_sync_voltage(g3d_regulator);
#endif /* CONFIG_EXYNOS_CL_DVFS_G3D */

	if (regulator_set_voltage(g3d_regulator, vol, vol) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to set voltage, voltage: %d\n", __func__,
			vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	platform->cur_voltage = gpu_get_cur_voltage(platform);

	GPU_LOG(DVFS_DEBUG, LSI_VOL_VALUE, vol, platform->cur_voltage,
		"voltage set: %d, voltage get:%d\n",
		vol, platform->cur_voltage);

	return 0;
}

static int gpu_set_voltage_pre(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (!is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}

static int gpu_set_voltage_post(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}

static struct gpu_control_ops ctr_ops = {
	.is_power_on = gpu_is_power_on,
	.set_voltage = gpu_set_voltage,
	.set_voltage_pre = gpu_set_voltage_pre,
	.set_voltage_post = gpu_set_voltage_post,
	.set_clock_to_osc = NULL,
	.set_clock = gpu_set_clock,
	.set_clock_pre = NULL,
	.set_clock_post = NULL,
	.enable_clock = gpu_clock_on,
	.disable_clock = gpu_clock_off,
};

struct gpu_control_ops *gpu_get_control_ops(void)
{
	return &ctr_ops;
}

#ifdef CONFIG_REGULATOR
int gpu_enable_dvs(struct exynos_context *platform) {
	return 0;
}

int gpu_disable_dvs(struct exynos_context *platform) {
	return 0;
}

int gpu_regulator_enable(struct exynos_context *platform) {
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s g3d_regulator is not initialized\n", __func__);
		return -1;
	}

	if (regulator_enable(g3d_regulator) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s failed to enable g3d regulator\n", __func__);
		return -1;
	}
	return 0;
}

int gpu_regulator_disable(struct exynos_context *platform) {
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s g3d_regulator is not initialized\n", __func__);
		return -1;
	}

	if (regulator_disable(g3d_regulator) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s failed to disable g3d regulator\n", __func__);
		return -1;
	}
	return 0;
}

int gpu_regulator_init(struct exynos_context *platform) {

	g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR(g3d_regulator) || (g3d_regulator == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to get vdd_g3d regulator, 0x%p\n", __func__,
			g3d_regulator);
		g3d_regulator = NULL;
		return -1;
	}

	if (gpu_regulator_enable(platform) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s failed to enable regulator\n", __func__);
		g3d_regulator = NULL;
		return -1;
	}

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "regulator initialized\n");

	return 0;
}

int gpu_regulator_term(struct exynos_context *platform) {
	if (gpu_regulator_disable(platform) != 0) {
		return -1;
	}

	regulator_put(g3d_regulator);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "regulator terminated\n");
	return 0;
}
#endif /* CONFIG_REGULATOR */

int *get_mif_table(int *size)
{
	*size = ARRAY_SIZE(mif_min_table);
	return mif_min_table;
}
