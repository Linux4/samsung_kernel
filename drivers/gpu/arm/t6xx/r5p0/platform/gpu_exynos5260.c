/* drivers/gpu/arm/.../platform/gpu_exynos5433.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_exynos5433.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>
#include <linux/pm_qos.h>

#include <mach/asv-exynos.h>
#include <mach/pm_domains_v2.h>
#include <mach/regs-clock-exynos5260.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_control.h"

extern struct kbase_device *pkbdev;

#ifdef GPU_MO_RESTRICTION
#define L2CONFIG_MO_1BY8		0b0101
#define L2CONFIG_MO_1BY4		0b1010
#define L2CONFIG_MO_1BY2		0b1111
#define L2CONFIG_MO_NO_RESTRICT		0
#endif /* GPU_MO_RESTRICTION */

#define CPU_MAX PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE

/*  clk,vol,abb,min,max,down stay,time_in_state,pm_qos mem,pm_qos int,pm_qos cpu_kfc_min,pm_qos cpu_egl_max */
static gpu_dvfs_info gpu_dvfs_table_default[] = {
#if defined(CONFIG_MACH_M2ALTE) || defined(CONFIG_MACH_M2A3G)
	{667, 1175000, ABB_BYPASS, 98, 100, 1, 0, 667000, 333000, 1000000, CPU_MAX},
	{560, 1075000, ABB_BYPASS, 78, 100, 1, 0, 667000, 333000, 1000000, CPU_MAX},
	{450, 1000000, ABB_BYPASS, 70,  90, 2, 0, 275000, 160000,       0, CPU_MAX},
	{350,  950000, ABB_BYPASS, 60,  90, 3, 0, 275000, 160000,       0, CPU_MAX},
	{266,  900000, ABB_BYPASS, 53,  90, 3, 0, 275000, 100000,       0, CPU_MAX},
	{160,  900000, ABB_BYPASS,  0,  90, 3, 0, 275000, 100000,       0, CPU_MAX},
#else
	{667, 1075000, ABB_BYPASS, 98, 100, 1, 0, 667000, 333000, 1000000, CPU_MAX},
	{560, 1075000, ABB_BYPASS, 78, 100, 1, 0, 667000, 333000, 1000000, CPU_MAX},
	{450, 1000000, ABB_BYPASS, 70,  90, 2, 0, 413000, 160000,       0, CPU_MAX},
	{350,  950000, ABB_BYPASS, 60,  90, 3, 0, 413000, 160000,       0, CPU_MAX},
	{266,  900000, ABB_BYPASS, 53,  90, 3, 0, 413000, 100000,       0, CPU_MAX},
	{160,  900000, ABB_BYPASS,  0,  90, 3, 0, 275000, 100000,       0, CPU_MAX},
#endif
};

static gpu_attribute gpu_config_attributes[] = {
	{GPU_MAX_CLOCK, 560},
	{GPU_MAX_CLOCK_LIMIT, 560},
	{GPU_MIN_CLOCK, 160},
#if defined(CONFIG_LCD_MIPI_D6EA8061) || defined(CONFIG_LCD_MIPI_HX8394)
	{GPU_DVFS_START_CLOCK, 160},
	{GPU_DVFS_BL_CONFIG_CLOCK, 160},
#else
	{GPU_DVFS_START_CLOCK, 350},
	{GPU_DVFS_BL_CONFIG_CLOCK, 350},
#endif
	{GPU_GOVERNOR_START_CLOCK_DEFAULT, 266},
	{GPU_GOVERNOR_START_CLOCK_STATIC, 266},
	{GPU_GOVERNOR_START_CLOCK_BOOSTER, 266},
	{GPU_GOVERNOR_TABLE_DEFAULT, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_STATIC, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_BOOSTER, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_SIZE_DEFAULT, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_STATIC, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_BOOSTER, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_DEFAULT_VOLTAGE, 900000},
	{GPU_COLD_MINIMUM_VOL, 950000},
	{GPU_VOLTAGE_OFFSET_MARGIN, 50000},
	{GPU_TMU_CONTROL, 1},
	{GPU_TEMP_THROTTLING1, 266},
	{GPU_TEMP_THROTTLING2, 266},
	{GPU_TEMP_THROTTLING3, 160},
	{GPU_TEMP_THROTTLING4, 160},
	{GPU_TEMP_TRIPPING, 160},
	{GPU_BOOST_MIN_LOCK, 0},
	{GPU_BOOST_EGL_MIN_LOCK, 0},
	{GPU_POWER_COEFF, 64}, /* all core on param */
	{GPU_DVFS_TIME_INTERVAL, 5},
	{GPU_DEFAULT_WAKEUP_LOCK, 1},
	{GPU_BUS_DEVFREQ, 1},
	{GPU_DYNAMIC_ABB, 1},
	{GPU_EARLY_CLK_GATING, 0},
	{GPU_PERF_GATHERING, 0},
	{GPU_HWCNT_GATHERING, 0},
	{GPU_HWCNT_GPR, 0},
	{GPU_RUNTIME_PM_DELAY_TIME, 50},
	{GPU_DVFS_POLLING_TIME, 30},
	{GPU_DEBUG_LEVEL, DVFS_WARNING},
	{GPU_TRACE_LEVEL, TRACE_ALL},
};

void *gpu_get_config_attributes(void)
{
	return &gpu_config_attributes;
}

struct clk *fout_vpll;
struct clk *ext_xtal;
struct clk *aclk_g3d;
struct clk *clk_g3d;

#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator;
#endif /* CONFIG_REGULATOR */

int gpu_is_power_on(void)
{
	return ((__raw_readl(EXYNOS5260_G3D_STATUS) & EXYNOS5260_INT_LOCAL_PWR_EN) == EXYNOS5260_INT_LOCAL_PWR_EN) ? 1 : 0;
}

int gpu_power_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power initialized\n");

	return 0;
}

int gpu_get_cur_clock(struct exynos_context *platform)
{
	int clock;
	if (!platform)
		return -ENODEV;

	if (!aclk_g3d) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: clock is not initialized\n", __func__);
		return -1;
	}

	if (gpu_is_power_on())
		clock = clk_get_rate(aclk_g3d)/MHZ;
	else
		clock = platform->cur_clock;

	return clock;
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
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: can't set clock on in power off status\n", __func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 1) {
		ret = 0;
		goto err_return;
	}

	if (aclk_g3d) {
		(void) clk_prepare_enable(aclk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u, "aclk_g3d clock is enabled\n");
	}

	if (clk_g3d) {
		(void) clk_prepare_enable(clk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 1u, "clk_g3d clock is enabled\n");
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
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock off in power off status\n", __func__);
		ret = -1;
		goto err_return;
	}

	if (platform->clk_g3d_status == 0) {
		ret = 0;
		goto err_return;
	}

	if (aclk_g3d) {
		(void)clk_disable_unprepare(aclk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF, 0u, 0u, "aclk_g3d clock is disabled\n");
	}

	if (clk_g3d) {
		(void)clk_disable_unprepare(clk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF, 0u, 1u, "clk_g3d clock is disabled\n");
	}

	platform->clk_g3d_status = 0;

err_return:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}

#ifdef GPU_MO_RESTRICTION
static int gpu_set_multiple_outstanding_req(int val)
{
	volatile unsigned int reg;

	if(val > 0b1111)
		return -1;

	reg = kbase_os_reg_read(pkbdev, GPU_CONTROL_REG(L2_MMU_CONFIG));
	reg &= ~(0b1111 << 24);
	reg |= ((val & 0b1111) << 24);
	kbase_os_reg_write(pkbdev, GPU_CONTROL_REG(L2_MMU_CONFIG), reg);

	return 0;
}
#endif

static int gpu_set_clock(struct exynos_context *platform, int clk)
{
	long g3d_rate_prev = -1;
	unsigned long g3d_rate = clk * MHZ;
	int ret = 0;

	if (aclk_g3d == 0)
		return -1;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the power-off state!\n", __func__);
		goto err;
	}

	if (!platform->clk_g3d_status) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the clock-off state!\n", __func__);
		goto err;
	}

#ifdef GPU_MO_RESTRICTION
	gpu_set_multiple_outstanding_req(L2CONFIG_MO_1BY4);	/* Restrict M.O */
#endif

	g3d_rate_prev = clk_get_rate(aclk_g3d)/MHZ;

	ret = clk_set_parent(aclk_g3d, ext_xtal);
	if (ret < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [mout_g3d_pll]\n", __func__);
		goto err;
	}
	ret = clk_set_rate(fout_vpll, g3d_rate);
	if (ret < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_rate [fout_vpll]\n", __func__);
		goto err;
	}
	ret = clk_set_parent(aclk_g3d, fout_vpll);
	if (ret < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [aclk_g3d]\n", __func__);
		goto err;
	}

#ifdef GPU_MO_RESTRICTION
	gpu_set_multiple_outstanding_req(L2CONFIG_MO_NO_RESTRICT);	/* Set to M.O by maximum */
#endif		

	platform->cur_clock = gpu_get_cur_clock(platform);

	if (g3d_rate != g3d_rate_prev)
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, g3d_rate/MHZ, platform->cur_clock, "clock set: %d, clock get: %d\n", (int) g3d_rate/MHZ, platform->cur_clock);
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

	fout_vpll = clk_get(kbdev->dev, "fout_vpll");
	if (IS_ERR(fout_vpll) || (fout_vpll == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [fout_vpll]\n", __func__);
		return -1;
	}

	ext_xtal = clk_get(NULL, "ext_xtal");
	if (IS_ERR(ext_xtal)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [ext_xtal]\n", __func__);
		return -1;
	}

	aclk_g3d = clk_get(kbdev->dev, "aclk_g3d");
	if (IS_ERR(aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [aclk_g3d]\n", __func__);
		return -1;
	}

	clk_g3d = clk_get(kbdev->dev, "g3d");
	if (IS_ERR(clk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [clk_g3d]\n", __func__);
		return -1;
	}

	return 0;
}

int gpu_clock_init(struct kbase_device *kbdev)
{
	int ret;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	ret = gpu_get_clock(kbdev);
	if (ret < 0)
		return -1;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "clock initialized\n");

	return 0;
}

int gpu_get_cur_voltage(struct exynos_context *platform)
{
	int ret = 0;
#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
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
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set voltage in the power-off state!\n", __func__);
		return -1;
	}

#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

	if (regulator_set_voltage(g3d_regulator, vol, vol) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage, voltage: %d\n", __func__, vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	platform->cur_voltage = gpu_get_cur_voltage(platform);

	GPU_LOG(DVFS_DEBUG, LSI_VOL_VALUE, vol, platform->cur_voltage, "voltage set: %d, voltage get:%d\n", vol, platform->cur_voltage);

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

int gpu_control_ops_init(struct exynos_context *platform)
{
	if (!platform)
		return -1;

	platform->ctr_ops.is_power_on = gpu_is_power_on;
	platform->ctr_ops.set_voltage = gpu_set_voltage;
	platform->ctr_ops.set_voltage_pre = gpu_set_voltage_pre;
	platform->ctr_ops.set_voltage_post = gpu_set_voltage_post;
	platform->ctr_ops.set_clock = gpu_set_clock;
	platform->ctr_ops.set_clock_pre = NULL;
	platform->ctr_ops.set_clock_post = NULL;
	platform->ctr_ops.set_clock_to_osc = NULL;
	platform->ctr_ops.enable_clock = gpu_clock_on;
	platform->ctr_ops.disable_clock = gpu_clock_off;

	return 0;
}

#ifdef CONFIG_REGULATOR
int gpu_regulator_init(struct exynos_context *platform)
{
	int gpu_voltage = 0;

	g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR(g3d_regulator)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get vdd_g3d regulator, 0x%p\n", __func__, g3d_regulator);
		g3d_regulator = NULL;
		return -1;
	}

	if (regulator_enable(g3d_regulator) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to enable mali t6xx regulator%p\n", __func__, g3d_regulator);
		g3d_regulator = NULL;
		return -1;
	}

	gpu_voltage = get_match_volt(ID_G3D, platform->gpu_dvfs_config_clock*1000);

	if (gpu_voltage == 0)
		gpu_voltage = platform->gpu_default_vol;

	if (gpu_set_voltage(platform, gpu_voltage) != 0) {
		regulator_disable(g3d_regulator);
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage [%d]\n", __func__, gpu_voltage);
		return -1;
	}

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "regulator initialized\n");

	return 0;
}
#endif /* CONFIG_REGULATOR */
