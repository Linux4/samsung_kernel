/* drivers/gpu/mali400/mali/platform/exynos4270/exynos4_pmm.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali400 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file exynos4_pmm.c
 * Platform specific Mali driver functions for the exynos 4XXX based platforms
 */
#include "mali_kernel_common.h"
#include "mali_osk.h"
#include "exynos4_pmm.h"
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <mach/regs-pmu.h>

#if defined(CONFIG_MALI400_PROFILING)
#include "mali_osk_profiling.h"
#endif

/* Some defines changed names in later Odroid-A kernels. Make sure it works for both. */
#ifndef S5P_G3D_CONFIGURATION
#define S5P_G3D_CONFIGURATION EXYNOS4_G3D_CONFIGURATION
#endif
#ifndef S5P_G3D_STATUS
#define S5P_G3D_STATUS (EXYNOS4_G3D_CONFIGURATION + 0x4)
#endif
#ifndef S5P_INT_LOCAL_PWR_EN
#define S5P_INT_LOCAL_PWR_EN EXYNOS_INT_LOCAL_PWR_EN
#endif

#define EXTXTALCLK_NAME  "ext_xtal"
#define VPLLSRCCLK_NAME  "mout_vpllsrc"
#define FOUTVPLLCLK_NAME "fout_vpll"
#define MOUTEPLLCLK_NAME "mout_epll"
#define SCLVPLLCLK_NAME  "mout_vpll"
#define GPUMOUT1CLK_NAME "mout_g3d1"
#define MPLLCLK_NAME     "mout_mpll"
#define SCLK_MPLL_PRE_DIV_CLK_NAME "sclk_mpll_pre_div"
#define MPLLCLK_NAME     "mout_mpll"
#define GPUMOUT0CLK_NAME "mout_g3d0"
#define GPUCLK_NAME      "sclk_g3d"
#define CLK_DIV_STAT_G3D 0x1003C62C
#define CLK_DESC         "clk-divider-status"

static struct clk *ext_xtal_clock = NULL;
static struct clk *vpll_src_clock = NULL;
static struct clk *mpll_clock = NULL;
static struct clk *sclk_mpll_pre_div_clock;
static struct clk *mali_mout0_clock;
static struct clk *fout_vpll_clock = NULL;
static struct clk *sclk_vpll_clock = NULL;
static struct clk *mali_mout1_clock;
static struct clk *mali_clock = NULL;

/* PegaW1 */
#ifndef CONFIG_EXYNOS3_VPLL_ENABLE
/* desired 133Mhz but need to specify 134 since 400/3 is 133.3 and is greater than 133 */
int mali_gpu_clk = 134;
#else
int mali_gpu_clk = 160;
#endif
static unsigned int GPU_MHZ	= 1000000;
static int nPowermode;
static atomic_t clk_active;

_mali_osk_lock_t *mali_freq_lock = 0;

/* export GPU frequency as a read-only parameter so that it can be read in /sys */
module_param(mali_gpu_clk, int, S_IRUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mali_gpu_clk, "Mali Current Clock");

mali_bool mali_clk_get(void)
{
	if (ext_xtal_clock == NULL)	{
		ext_xtal_clock = clk_get(NULL, EXTXTALCLK_NAME);
		if (IS_ERR(ext_xtal_clock)) {
			MALI_PRINT(("MALI Error : failed to get source ext_xtal_clock\n"));
			return MALI_FALSE;
		}
	}

	if (vpll_src_clock == NULL)	{
		vpll_src_clock = clk_get(NULL, VPLLSRCCLK_NAME);
		if (IS_ERR(vpll_src_clock)) {
			MALI_PRINT(("MALI Error : failed to get source vpll_src_clock\n"));
			return MALI_FALSE;
		}
	}

	if (fout_vpll_clock == NULL) {
		fout_vpll_clock = clk_get(NULL, FOUTVPLLCLK_NAME);
		if (IS_ERR(fout_vpll_clock)) {
			MALI_PRINT(("MALI Error : failed to get source fout_vpll_clock\n"));
			return MALI_FALSE;
		}
	}

	if (mpll_clock == NULL)
	{
		mpll_clock = clk_get(NULL,MPLLCLK_NAME);

		if (IS_ERR(mpll_clock)) {
			MALI_PRINT( ("MALI Error : failed to get source mpll clock\n"));
			return MALI_FALSE;
		}
	}

	if (sclk_mpll_pre_div_clock == NULL) {
		sclk_mpll_pre_div_clock = clk_get(NULL, SCLK_MPLL_PRE_DIV_CLK_NAME);

		if (IS_ERR(sclk_mpll_pre_div_clock)) {
			MALI_PRINT(("MALI Error : failed to get source sclk_mpll_pre_div clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_mout0_clock == NULL)
	{
		mali_mout0_clock = clk_get(NULL, GPUMOUT0CLK_NAME);

		if (IS_ERR(mali_mout0_clock)) {
			MALI_PRINT(("MALI Error : failed to get source mali mout0 clock\n"));
			return MALI_FALSE;
		}
	}

	if (sclk_vpll_clock == NULL) {
		sclk_vpll_clock = clk_get(NULL, SCLVPLLCLK_NAME);
		if (IS_ERR(sclk_vpll_clock)) {
			MALI_PRINT(("MALI Error : failed to get source sclk_vpll_clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_mout1_clock == NULL) {
		mali_mout1_clock = clk_get(NULL, GPUMOUT1CLK_NAME);

		if (IS_ERR(mali_mout1_clock)) {
			MALI_PRINT(("MALI Error : failed to get source mali mout1 clock\n"));
			return MALI_FALSE;
		}
	}

	if (mali_clock == NULL) {
		mali_clock = clk_get(NULL, GPUCLK_NAME);

		if (IS_ERR(mali_clock)) {
			MALI_PRINT(("MALI Error : failed to get source mali clock\n"));
			return MALI_FALSE;
		}
	}

	return MALI_TRUE;
}

void mali_clk_put(mali_bool binc_mali_clock)
{
	if (mali_mout1_clock)
	{
		clk_put(mali_mout1_clock);
		mali_mout1_clock = NULL;
	}

	if (sclk_vpll_clock)
	{
		clk_put(sclk_vpll_clock);
		sclk_vpll_clock = NULL;
	}

	if (binc_mali_clock && fout_vpll_clock)
	{
		clk_put(fout_vpll_clock);
		fout_vpll_clock = NULL;
	}

	if (mpll_clock)
	{
		clk_put(mpll_clock);
		mpll_clock = NULL;
	}

	if (sclk_mpll_pre_div_clock) {
		clk_put(sclk_mpll_pre_div_clock);
		sclk_mpll_pre_div_clock = NULL;
	}

	if (mali_mout0_clock)
	{
		clk_put(mali_mout0_clock);
		mali_mout0_clock = NULL;
	}

	if (vpll_src_clock)
	{
		clk_put(vpll_src_clock);
		vpll_src_clock = NULL;
	}

	if (ext_xtal_clock)
	{
		clk_put(ext_xtal_clock);
		ext_xtal_clock = NULL;
	}

	if (binc_mali_clock && mali_clock)
	{
		clk_put(mali_clock);
		mali_clock = NULL;
	}
}

void mali_clk_set_rate(unsigned int clk, unsigned int mhz)
{
	int err;
	unsigned long rate = (unsigned long)clk * (unsigned long)mhz;

	_mali_osk_lock_wait(mali_freq_lock, _MALI_OSK_LOCKMODE_RW);
	MALI_DEBUG_PRINT(3, ("Mali platform: Setting frequency to %d mhz\n", clk));

	if (mali_clk_get() == MALI_FALSE) {
		_mali_osk_lock_signal(mali_freq_lock, _MALI_OSK_LOCKMODE_RW);
		return;
	}

	err = clk_set_rate(mali_clock, rate);
	if (err)
		MALI_PRINT_ERROR(("Failed to set Mali clock: %d\n", err));

	rate = clk_get_rate(mali_clock);
	GPU_MHZ = mhz;

	mali_gpu_clk = rate / mhz;
	MALI_PRINT(("Mali freq %dMhz\n", rate / mhz));
	mali_clk_put(MALI_FALSE);

	_mali_osk_lock_signal(mali_freq_lock, _MALI_OSK_LOCKMODE_RW);
}

static mali_bool init_mali_clock(void)
{
	int err = 0;
	mali_bool ret = MALI_TRUE;
	nPowermode = MALI_POWER_MODE_DEEP_SLEEP;

	if (mali_clock != 0)
		return ret; /* already initialized */

	mali_freq_lock = _mali_osk_lock_init(_MALI_OSK_LOCKFLAG_NONINTERRUPTABLE
			| _MALI_OSK_LOCKFLAG_ONELOCK, 0, 0);

	if (mali_freq_lock == NULL)
		return _MALI_OSK_ERR_FAULT;

	if (!mali_clk_get())
	{
		MALI_PRINT(("Error: Failed to get Mali clock\n"));
		goto err_clk;
	}

#ifndef CONFIG_EXYNOS3_VPLL_ENABLE
	err = clk_set_parent(mali_mout0_clock, sclk_mpll_pre_div_clock);
	if (err)
		MALI_PRINT_ERROR(("mali_mout0_clock set parent to sclk_mpll_pre_div_clock failed\n"));

	err = clk_set_parent(mali_clock, mali_mout0_clock);
	if (err)
		MALI_PRINT_ERROR(("mali_clock set parent to mali_mout0_clock failed\n"));
#else
	err = clk_set_rate(fout_vpll_clock, (unsigned int)mali_gpu_clk * GPU_MHZ);
	if (err > 0)
		MALI_PRINT_ERROR(("Failed to set fout_vpll clock: %d\n", err));

	err = clk_set_parent(vpll_src_clock, ext_xtal_clock);
	if (err)
		MALI_PRINT_ERROR(("vpll_src_clock set parent to ext_xtal_clock failed\n"));

	err = clk_set_parent(sclk_vpll_clock, fout_vpll_clock);
	if (err)
		MALI_PRINT_ERROR(("sclk_vpll_clock set parent to fout_vpll_clock failed\n"));

	err = clk_set_parent(mali_mout1_clock, sclk_vpll_clock);
	if (err)
		MALI_PRINT_ERROR(("mali_mout1_clock set parent to sclk_vpll_clock failed\n"));

	err = clk_set_parent(mali_clock, mali_mout1_clock);
	if (err)
		MALI_PRINT_ERROR(("mali_clock set parent to mali_mout1_clock failed\n"));
#endif
	if (!atomic_read(&clk_active)) {
		if (clk_enable(mali_clock) < 0) {
			MALI_PRINT(("Error: Failed to enable clock\n"));
			goto err_clk;
		}
		atomic_set(&clk_active, 1);
	}

	mali_clk_set_rate((unsigned int)mali_gpu_clk, GPU_MHZ);
	mali_clk_put(MALI_FALSE);

	return MALI_TRUE;

err_clk:
	mali_clk_put(MALI_TRUE);

	return ret;
}

static mali_bool deinit_mali_clock(void)
{
	if (mali_clock == 0)
		return MALI_TRUE;

	mali_clk_put(MALI_TRUE);
	return MALI_TRUE;
}

static _mali_osk_errcode_t enable_mali_clocks(struct device *dev)
{
	int err;
	err = clk_enable(mali_clock);
	MALI_DEBUG_PRINT(3,("enable_mali_clocks mali_clock %p error %d \n", mali_clock, err));

	MALI_SUCCESS;
}

static _mali_osk_errcode_t disable_mali_clocks(void)
{
	if (atomic_read(&clk_active)) {
		clk_disable(mali_clock);
		MALI_DEBUG_PRINT(3, ("disable_mali_clocks mali_clock %p\n", mali_clock));
		atomic_set(&clk_active, 0);
	}

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_init(struct device *dev)
{
	MALI_CHECK(init_mali_clock(), _MALI_OSK_ERR_FAULT);
	mali_platform_power_mode_change(dev, MALI_POWER_MODE_ON);

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(struct device *dev)
{
	mali_platform_power_mode_change(dev, MALI_POWER_MODE_DEEP_SLEEP);
	deinit_mali_clock();

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_power_mode_change(struct device *dev, mali_power_mode power_mode)
{
	switch (power_mode)
	{
	case MALI_POWER_MODE_ON:
		MALI_DEBUG_PRINT(3, ("Mali platform: Got MALI_POWER_MODE_ON event, %s\n",
					nPowermode ? "powering on" : "already on"));
		if (nPowermode == MALI_POWER_MODE_LIGHT_SLEEP || nPowermode == MALI_POWER_MODE_DEEP_SLEEP)	{
			MALI_DEBUG_PRINT(4, ("enable clock\n"));
			enable_mali_clocks(dev);
			nPowermode = power_mode;
		}
		break;
	case MALI_POWER_MODE_DEEP_SLEEP:
	case MALI_POWER_MODE_LIGHT_SLEEP:
		MALI_DEBUG_PRINT(3, ("Mali platform: Got %s event, %s\n", power_mode == MALI_POWER_MODE_LIGHT_SLEEP ?
					"MALI_POWER_MODE_LIGHT_SLEEP" : "MALI_POWER_MODE_DEEP_SLEEP",
					nPowermode ? "already off" : "powering off"));
		if (nPowermode == MALI_POWER_MODE_ON)	{
			disable_mali_clocks();
			nPowermode = power_mode;
		}
		break;
	}
	MALI_SUCCESS;
}
