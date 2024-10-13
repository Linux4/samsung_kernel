/* drivers/gpu/t6xx/kbase/src/platform/gpu_exynos3475.c
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
 * @file gpu_exynos3475.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#ifdef CONFIG_EXYNOS_APM
#include <mach/apm-exynos.h>
#endif
#include <mach/asv-exynos.h>
#include <mach/asv-exynos_cal.h>
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

#include <mach/regs-clock-exynos3475.h>

extern struct kbase_device *pkbdev;

#define CPU_MAX PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE

#ifdef CONFIG_EXYNOS_BUSMONITOR
void __iomem *g3d0_outstanding_regs;
void __iomem *g3d1_outstanding_regs;
#endif /* CONFIG_EXYNOS_BUSMONITOR */

/*  clk,vol,abb,min,max,down stay, time, pm_qos mem,
	pm_qos int, pm_qos cpu_kfc_min, pm_qos cpu_egl_max */
static gpu_dvfs_info gpu_dvfs_table_default[] = {
	{667, 1168750, 0,  99, 100, 1, 0, 666000, 275000,  897000, 1500000},
	{533, 1062500, 0,  98,  99, 1, 0, 559000, 275000,  897000, 1500000},
	{350,  900000, 0,  90,  98, 1, 0, 413000, 275000,  507000, 1500000},
	{266,  900000, 0,  78,  95, 1, 0, 413000, 275000,  507000, 1500000},
	{160,  900000, 0,  10,  20, 1, 0, 413000, 275000,  507000, 1500000},
};

static int mif_min_table[] = {
	200000, 273000, 338000, 413000, 559000, 666000,
};

static gpu_attribute gpu_config_attributes[] = {
	{GPU_MAX_CLOCK, 667},
	{GPU_MAX_CLOCK_LIMIT, 667},
	{GPU_MIN_CLOCK, 160},
	{GPU_DVFS_START_CLOCK, 160},
	{GPU_DVFS_BL_CONFIG_CLOCK, 160},
	{GPU_GOVERNOR_TYPE, G3D_DVFS_GOVERNOR_INTERACTIVE},
	{GPU_GOVERNOR_START_CLOCK_DEFAULT, 160},
	{GPU_GOVERNOR_START_CLOCK_INTERACTIVE, 160},
	{GPU_GOVERNOR_START_CLOCK_STATIC, 160},
	{GPU_GOVERNOR_START_CLOCK_BOOSTER, 160},
	{GPU_GOVERNOR_TABLE_DEFAULT, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_INTERACTIVE, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_STATIC, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_BOOSTER, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_SIZE_DEFAULT, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_INTERACTIVE, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_STATIC, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_BOOSTER, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_CLOCK, 440},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_LOAD, 95},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_DELAY, 0},
	{GPU_DEFAULT_VOLTAGE, 975000},
	{GPU_COLD_MINIMUM_VOL, 0},
	{GPU_VOLTAGE_OFFSET_MARGIN, 37500},
	{GPU_TMU_CONTROL, 0},
	{GPU_TEMP_THROTTLING1, 600},
	{GPU_TEMP_THROTTLING2, 533},
	{GPU_TEMP_THROTTLING3, 440},
	{GPU_TEMP_THROTTLING4, 350},
	{GPU_TEMP_TRIPPING, 266},
	{GPU_POWER_COEFF, 46}, /* all core on param */
	{GPU_DVFS_TIME_INTERVAL, 5},
	{GPU_DEFAULT_WAKEUP_LOCK, 1},
	{GPU_BUS_DEVFREQ, 0},
	{GPU_DYNAMIC_ABB, 0},
	{GPU_EARLY_CLK_GATING, 0},
	{GPU_DVS, 0},
#ifdef MALI_SEC_HWCNT
	{GPU_HWCNT_GATHERING, 1},
	{GPU_HWCNT_GPR, 1},
	{GPU_HWCNT_DUMP_PERIOD, 50}, /* ms */
	{GPU_HWCNT_CHOOSE_JM , 0},
	{GPU_HWCNT_CHOOSE_SHADER , 0xF8},
	{GPU_HWCNT_CHOOSE_TILER , 0},
	{GPU_HWCNT_CHOOSE_L3_CACHE , 0},
	{GPU_HWCNT_CHOOSE_MMU_L2 , 0},
#endif
	{GPU_RUNTIME_PM_DELAY_TIME, 50},
	{GPU_DVFS_POLLING_TIME, 30},
	{GPU_PERF_GATHERING, 0},
	{GPU_PMQOS_INT_DISABLE, 0},
	{GPU_PMQOS_MIF_MAX_CLOCK, 666000},
	{GPU_PMQOS_MIF_MAX_CLOCK_BASE, 667},
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	{GPU_CL_DVFS_START_BASE, 600},
#endif
	{GPU_DEBUG_LEVEL, DVFS_WARNING},
	{GPU_TRACE_LEVEL, TRACE_ALL},
};

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

struct clk *fin_pll;
struct clk *fout_g3d_pll;
struct clk *aclk_g3d;
struct clk *mout_aclk_g3d_400_user;
struct clk *mout_aclk_g3d_667;
static struct clk *aclk_g3d_400;
#ifdef MALI_DVFS_USING_SWITCHING_PLL
static struct clk *dout_aclk_g3d_400;
#define ACLK_G3D_400_START_CLOCK_MHZ 276
#endif
#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator;
static int regulator_max_support_volt;
#endif /* CONFIG_REGULATOR */


int gpu_is_power_on(void)
{
	return ((__raw_readl(EXYNOS_PMU_G3D_STATUS) &
			LOCAL_PWR_CFG) == LOCAL_PWR_CFG) ? 1 : 0;
}

void gpu_power_set_reg(void)
{
	__raw_writel(LOCAL_PWR_CFG, EXYNOS_PMU_G3D_CONFIGURATION);

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

	if (!aclk_g3d) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: clock is not initialized\n", __func__);
		return -1;
	}

	return clk_get_rate(aclk_g3d)/MHZ;
}

int gpu_is_clock_on(void)
{
	return __clk_is_enabled(aclk_g3d);
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

	if (aclk_g3d) {
		(void) clk_prepare_enable(aclk_g3d);
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

	if (platform->clk_g3d_status == 0) {
		ret = 0;
		goto err_return;
	}

	if (aclk_g3d) {
		(void)clk_disable_unprepare(aclk_g3d);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF,
			0u, 0u, "clock is disabled\n");
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
	if (gpu_is_power_on()) {
		/* G3D PMU */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x10464060,
			__raw_readl(EXYNOS_PMU_G3D_STATUS),
			"REG_DUMP: EXYNOS_PMU_G3D_STATUS %x\n",
			__raw_readl(EXYNOS_PMU_G3D_STATUS));

		/* G3D PLL */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460000,
			__raw_readl(EXYNOS3475_G3D_PLL_LOCK),
			"REG_DUMP: EXYNOS3475_G3D_PLL_LOCK %x\n",
			__raw_readl(EXYNOS3475_G3D_PLL_LOCK));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460100,
			__raw_readl(EXYNOS3475_G3D_PLL_CON0),
			"REG_DUMP: EXYNOS3475_G3D_PLL_CON0 %x\n",
			__raw_readl(EXYNOS3475_G3D_PLL_CON0));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460104,
			__raw_readl(EXYNOS3475_G3D_PLL_CON1),
			"REG_DUMP: EXYNOS3475_G3D_PLL_CON1 %x\n",
			__raw_readl(EXYNOS3475_G3D_PLL_CON1));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460108,
			__raw_readl(EXYNOS3475_G3D_PLL_FREQ_DET),
			"REG_DUMP: EXYNOS3475_G3D_PLL_FREQ_DET %x\n",
			__raw_readl(EXYNOS3475_G3D_PLL_FREQ_DET));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460200,
			__raw_readl(EXYNOS3475_CLK_CON_MUX_G3D_PLL),
			"REG_DUMP: EXYNOS3475_CLK_CON_MUX_G3D_PLL %x\n",
			__raw_readl(EXYNOS3475_CLK_CON_MUX_G3D_PLL));

		/* G3D SRC - ACLK_G3D(ACLK_G3D_667) */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460208,
			__raw_readl(EXYNOS3475_CLK_CON_MUX_ACLK_G3D_667),
			"REG_DUMP: EXYNOS3475_CLK_CON_MUX_ACLK_G3D_667 %x\n",
			__raw_readl(EXYNOS3475_CLK_CON_MUX_ACLK_G3D_667));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460608,
			__raw_readl(EXYNOS3475_CLK_STAT_MUX_ACLK_G3D_667),
			"REG_DUMP: EXYNOS3475_CLK_STAT_MUX_ACLK_G3D_667 %x\n",
			__raw_readl(EXYNOS3475_CLK_STAT_MUX_ACLK_G3D_667));

		/* G3D SRC - ACLK_G3D_400 */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460204,
			__raw_readl(EXYNOS3475_CLK_CON_MUX_ACLK_G3D_400_USER),
			"REG_DUMP: EXYNOS3475_CLK_CON_MUX_ACLK_G3D_400_USER %x\n",
			__raw_readl(EXYNOS3475_CLK_CON_MUX_ACLK_G3D_400_USER));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460604,
			__raw_readl(EXYNOS3475_CLK_STAT_MUX_ACLK_G3D_400_USER),
			"REG_DUMP: EXYNOS3475_CLK_STAT_MUX_ACLK_G3D_400_USER %x\n",
			__raw_readl(EXYNOS3475_CLK_STAT_MUX_ACLK_G3D_400_USER));

		/* G3D DIV */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460400,
			__raw_readl(EXYNOS3475_CLK_CON_DIV_ACLK_G3D_667),
			"REG_DUMP: EXYNOS3475_CLK_CON_DIV_ACLK_G3D_667 %x\n",
			__raw_readl(EXYNOS3475_CLK_CON_DIV_ACLK_G3D_667));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x10010418,
			__raw_readl(EXYNOS3475_CLK_CON_DIV_ACLK_G3D_400),
			"REG_DUMP: EXYNOS3475_CLK_CON_DIV_ACLK_G3D_400 %x\n",
			__raw_readl(EXYNOS3475_CLK_CON_DIV_ACLK_G3D_400));

		/* G3D ENABLE */
		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x11460800,
			__raw_readl(EXYNOS3475_CLK_ENABLE_ACLK_G3D_667),
			"REG_DUMP: EXYNOS3475_CLK_ENABLE_ACLK_G3D_667 %x\n",
			__raw_readl(EXYNOS3475_CLK_ENABLE_ACLK_G3D_667));

		GPU_LOG(DVFS_DEBUG, LSI_REGISTER_DUMP, 0x10010818,
			__raw_readl(EXYNOS3475_CLK_ENABLE_ACLK_G3D_400),
			"REG_DUMP: EXYNOS3475_CLK_ENABLE_ACLK_G3D_400 %x\n",
			__raw_readl(EXYNOS3475_CLK_ENABLE_ACLK_G3D_400));
	}

	return 0;
}

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
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set clock in the power-off state!\n",
			__func__);
		goto err;
	}

	if (!gpu_is_clock_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u,
			"%s: can't set clock in the clock-off state!\n",
			__func__);
		goto err;
	}

	g3d_rate_prev = clk_get_rate(aclk_g3d);

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (g3d_rate != g3d_rate_prev) {

		if (aclk_g3d_400) {
			(void) clk_prepare_enable(aclk_g3d_400);
			GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u,
				"[switching PLL : aclk_g3d_400] clock is enabled\n");
		}

		ret = clk_set_parent(mout_aclk_g3d_667, mout_aclk_g3d_400_user);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: failed to clk_set_parent [aclk_g3d_400]\n",
				__func__);
			goto err;
		}

		/*change g3d pll*/
		ret = clk_set_rate(fout_g3d_pll, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: failed to clk_set_rate"
				"[fout_g3d_pll]\n", __func__);
			goto err;
		}

		ret = clk_set_parent(mout_aclk_g3d_667, fout_g3d_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
				"%s: failed to clk_set_parent [fout_g3d_pll]\n",
				__func__);
			goto err;
		}

		if (aclk_g3d_400) {
			(void)clk_disable_unprepare(aclk_g3d_400);
			GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF,
				0u, 0u, "[switching PLL : aclk_g3d_400] clock is disabled\n");
		}

		g3d_rate_prev = g3d_rate;
	}

	platform->cur_clock = gpu_get_cur_clock(platform);

	if (platform->cur_clock != clk_get_rate(fout_g3d_pll)/MHZ)
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"clock value is wrong (aclk_g3d: %d,"
			" fout_g3d_pll: %d)\n",
			platform->cur_clock,
			(int) clk_get_rate(fout_g3d_pll)/MHZ);

	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, 0u, g3d_rate/MHZ,
		"clock set: %ld\n", g3d_rate/MHZ);
	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, 0u, platform->cur_clock,
		"clock get: %d\n", platform->cur_clock);
err:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	return ret;
}

static int gpu_get_clock(struct kbase_device *kbdev)
{
	struct exynos_context *platform =
		(struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	fin_pll = clk_get(kbdev->dev, "fin_pll");
	if (IS_ERR(fin_pll) || (fin_pll == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [fin_pll]\n",
			__func__);
		return -1;
	}

	fout_g3d_pll = clk_get(kbdev->dev, "fout_g3d_pll");
	if (IS_ERR(fout_g3d_pll) || (fout_g3d_pll == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [fout_g3d_pll]\n",
			__func__);
		goto clock_error1;
	}

	aclk_g3d = clk_get(kbdev->dev, "aclk_g3d");
	if ((IS_ERR(aclk_g3d)) || (aclk_g3d == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [aclk_g3d]\n",
			__func__);
		goto clock_error2;
	}

	mout_aclk_g3d_667 = clk_get(kbdev->dev, "mout_aclk_g3d_667");
	if (IS_ERR(mout_aclk_g3d_667) || (mout_aclk_g3d_667 == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [mout_aclk_g3d_667]\n",
			__func__);
		goto clock_error3;
	}

	mout_aclk_g3d_400_user = clk_get(kbdev->dev, "mout_aclk_g3d_400_user");
	if (IS_ERR(mout_aclk_g3d_400_user) || (mout_aclk_g3d_400_user == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [mout_aclk_g3d_400_user]\n",
			__func__);
		goto clock_error4;
	}

	aclk_g3d_400 = clk_get(kbdev->dev, "aclk_g3d_400");
	if (IS_ERR(aclk_g3d_400) || (aclk_g3d_400 == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [aclk_g3d_400]\n",
			__func__);
		goto clock_error5;
	}

#ifdef MALI_DVFS_USING_SWITCHING_PLL
	dout_aclk_g3d_400 = clk_get(kbdev->dev, "dout_aclk_g3d_400");
	if (IS_ERR(dout_aclk_g3d_400) || (dout_aclk_g3d_400 == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to clk_get [dout_aclk_g3d_400]\n",
			__func__);
		goto clock_error6;
	}

#endif

	if (gpu_clock_on(platform) < 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: clock is not enabled\n", __func__);
		return -1;
	}

	return 0;
#ifdef MALI_DVFS_USING_SWITCHING_PLL
clock_error6:
	clk_put(aclk_g3d_400);
#endif
clock_error5:
	clk_put(mout_aclk_g3d_400_user);
clock_error4:
	clk_put(mout_aclk_g3d_667);
clock_error3:
	clk_put(aclk_g3d);
clock_error2:
	clk_put(fout_g3d_pll);
clock_error1:
	clk_put(fin_pll);
	return -1;
}

int gpu_clock_init(struct kbase_device *kbdev)
{
	int ret = 0;
#ifdef MALI_DVFS_USING_SWITCHING_PLL
	unsigned long aclk_g3d_400_rate = ACLK_G3D_400_START_CLOCK_MHZ * MHZ;
#endif

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	ret = gpu_get_clock(kbdev);

	if (ret < 0)
		return -1;

#ifdef MALI_DVFS_USING_SWITCHING_PLL
	if (aclk_g3d_400) {
		(void) clk_prepare_enable(aclk_g3d_400);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_ON, 0u, 0u,
			"alck_g3d_400 clock is enabled\n");

		ret = clk_set_rate(dout_aclk_g3d_400, aclk_g3d_400_rate);
		if (ret < 0) {
			(void)clk_disable_unprepare(aclk_g3d_400);
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
					"%s: failed to clk_set_rate"
					"[dout_aclk_g3d_400]\n", __func__);
			return -1;
		}

		(void)clk_disable_unprepare(aclk_g3d_400);
		GPU_LOG(DVFS_DEBUG, LSI_CLOCK_OFF,
			0u, 0u, "aclk_g3d_400 clock is disabled\n");
	}
#endif

#ifdef CONFIG_EXYNOS_BUSMONITOR
	g3d0_outstanding_regs = ioremap(0x14A00000, SZ_1K);
	g3d1_outstanding_regs = ioremap(0x14A20000, SZ_1K);
#endif /* CONFIG_EXYNOS_BUSMONITOR */

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "clock initialized\n");

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

	if (regulator_set_voltage(g3d_regulator, vol, regulator_max_support_volt) != 0) {
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

	return 0;
}

static int gpu_set_voltage_post(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

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

int gpu_regulator_init(struct exynos_context *platform)
{
	g3d_regulator = regulator_get(NULL, "vdd_core");
	if (IS_ERR(g3d_regulator) || (g3d_regulator == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u,
			"%s: failed to get vdd_core regulator, 0x%p\n", __func__,
			g3d_regulator);
		g3d_regulator = NULL;
		return -1;
	}

	regulator_max_support_volt = regulator_get_max_support_voltage(g3d_regulator);

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "regulator initialized\n");

	return 0;
}
#endif /* CONFIG_REGULATOR */

int *get_mif_table(int *size)
{
	*size = ARRAY_SIZE(mif_min_table);
	return mif_min_table;
}

void exynos3475_set_ema (unsigned int volt)
{
	cal_set_ema(SYSC_DVFS_G3D, volt);
}
