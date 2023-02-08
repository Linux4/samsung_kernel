/* drivers/gpu/mali400/mali/platform/exynos3470/exynos4_pmm.c
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

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#if defined(CONFIG_MALI400_PROFILING)
#include "mali_osk_profiling.h"
#endif

/* lock/unlock CPU freq by Mali */
#include <linux/types.h>
#include <mach/cpufreq.h>
#include <mach/regs-clock.h>
#include <mach/asv-exynos.h>
#ifdef CONFIG_CPU_FREQ
#define EXYNOS4_ASV_ENABLED
#endif

/* MALI_SEC_QOS */
#ifdef CONFIG_MALI_DVFS
#define BUSFREQ_QOS_LOCK
#endif

#ifdef BUSFREQ_QOS_LOCK
#include <linux/pm_qos.h>
static struct pm_qos_request mif_min_qos;
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

#include <asm/io.h>
#include <mach/regs-pmu.h>
#include <linux/workqueue.h>

#define MALI_DVFS_STEPS 5
#define MALI_DVFS_STEPS_ISP 4
#define MALI_DVFS_WATING 10 /* msec */
#define MALI_DVFS_DEFAULT_STEP 0
#define MALI_DVFS_DEFAULT_STEP_ISP 3

#define MALI_DVFS_CLK_DEBUG 0
#define CPUFREQ_LOCK_DURING_440 0

static int bMaliDvfsRun = 0;

typedef struct mali_dvfs_tableTag{
	unsigned int clock;
	unsigned int freq;
	unsigned int vol;
	unsigned int downthreshold;
	unsigned int upthreshold;
#ifdef BUSFREQ_QOS_LOCK
	unsigned int mif_min_lock_value;
#endif
}mali_dvfs_table;

typedef struct mali_dvfs_statusTag{
	unsigned int currentStep;
	mali_dvfs_table * pCurrentDvfs;
} mali_dvfs_status_t;

/*dvfs status*/
mali_dvfs_status_t maliDvfsStatus;
int mali_dvfs_control;
int mali_isp_current_clock;
static _mali_osk_atomic_t dvfslock_status;

typedef struct mali_runtime_resumeTag{
		int clk;
		int vol;
		unsigned int step;
}mali_runtime_resume_table;

mali_runtime_resume_table mali_runtime_resume = {160, 850000, 0};

/* dvfs table updated on 130520 */
mali_dvfs_table mali_dvfs[MALI_DVFS_STEPS] = {
	/* step 0 */{160, 1000000,  850000,   0,  70
#ifdef BUSFREQ_QOS_LOCK
		/* mif_min_lock_value */
		, 0
#endif
	},
	/* step 1 */{266, 1000000,  850000,  62,  90
#ifdef BUSFREQ_QOS_LOCK
		/* mif_min_lock_value */
		, 267000
#endif
	},
	/* step 2 */{340, 1000000,  875000,  85,  90
#ifdef BUSFREQ_QOS_LOCK
		/* mif_min_lock_value */
		, 267000
#endif
	},
	/* step 3 */{440, 1000000,  925000,  85,  90
#ifdef BUSFREQ_QOS_LOCK
		/* mif_min_lock_value */
		, 267000
#endif
	},
	/* step 4 */{533, 1000000, 1000000,  95, 100
#ifdef BUSFREQ_QOS_LOCK
		/* mif_min_lock_value */
		, 267000
#endif
	} };

/* dvfs table updated on 140314 */
mali_dvfs_table mali_dvfs_isp[MALI_DVFS_STEPS_ISP] = {
	/* step 0 */{150, 1000000,  850000,   0,  70
#ifdef BUSFREQ_QOS_LOCK
		/* mif_min_lock_value */
		, 0
#endif
	},
	/* step 1 */{225, 1000000,  850000,  62,  90
#ifdef BUSFREQ_QOS_LOCK
		/* mif_min_lock_value */
		, 267000
#endif
	},
	/* step 2 */{300, 1000000,  875000,  85,  90
#ifdef BUSFREQ_QOS_LOCK
		/* mif_min_lock_value */
		, 267000
#endif
	},
	/* step 3 */{450, 1000000,  925000,  95, 100
#ifdef BUSFREQ_QOS_LOCK
		/* mif_min_lock_value */
		, 267000
#endif
	} };

/* Exynos3470 */
int mali_gpu_clk = 160;
int mali_gpu_vol = 850000;
unsigned int mali_vpll_clk = 900;
int mali_min_freq;
int mali_max_freq;
char *mali_freq_table = "533 440 340 266 160";
#define EXTXTALCLK_NAME  "ext_xtal"
#define VPLLSRCCLK_NAME  "vpll_src"
#define FOUTVPLLCLK_NAME "fout_vpll"
#define MOUTEPLLCLK_NAME "mout_epll"
#define SCLVPLLCLK_NAME  "sclk_vpll"
#define GPUMOUT1CLK_NAME "mout_g3d1"
#define GPUMOUT0CLK_NAME "mout_g3d0"
#define GPUCLK_NAME      "sclk_g3d"
#define CLK_DIV_STAT_G3D 0x1003C62C
#define CLK_DESC         "clk-divider-status"
int ENABLE_LOCK_BY_ISP;
int mali_isp_current_level;

static struct clk *ext_xtal_clock = NULL;
static struct clk *vpll_src_clock = NULL;
static struct clk *fout_vpll_clock = NULL;
static struct clk *mout_epll_clock = NULL;
static struct clk *sclk_vpll_clock = NULL;
static struct clk *mali_parent_clock = NULL;
static struct clk *mali_clock = NULL;

static unsigned int GPU_MHZ	= 1000000;
static unsigned int const GPU_ASV_VOLT = 1000;
static int nPowermode;
static atomic_t clk_active;

mali_io_address clk_register_map = 0;

/* export GPU frequency as a read-only parameter so that it can be read in /sys */
module_param(mali_gpu_clk, int, S_IRUSR | S_IRGRP | S_IROTH);
module_param(mali_gpu_vol, int, S_IRUSR | S_IRGRP | S_IROTH);
module_param(mali_min_freq, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
module_param(mali_max_freq, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
module_param(mali_freq_table, charp, S_IRUSR | S_IRGRP | S_IROTH);
#ifdef CONFIG_MALI_DVFS
module_param(mali_dvfs_control, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP| S_IROTH); /* rw-rw-r-- */
MODULE_PARM_DESC(mali_dvfs_control, "Mali Current DVFS");
DEVICE_ATTR(time_in_state, S_IRUGO|S_IWUSR, show_time_in_state, set_time_in_state);
MODULE_PARM_DESC(time_in_state, "Time-in-state of Mali DVFS");
#endif
MODULE_PARM_DESC(mali_gpu_clk, "Mali Current Clock");
MODULE_PARM_DESC(mali_gpu_vol, "Mali Current Voltage");
MODULE_PARM_DESC(mali_min_freq, "Mali Min frequency lock");
MODULE_PARM_DESC(mali_max_freq, "Mali Max frequency lock");
MODULE_PARM_DESC(mali_freq_table, "Mali frequency table");

#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator = NULL;
#endif
atomic_t mali_cpufreq_lock;

/* DVFS */
#ifdef CONFIG_MALI_DVFS
static unsigned int mali_dvfs_utilization = 255;
static void update_time_in_state(int level);
u64 mali_dvfs_time[MALI_DVFS_STEPS];
#endif

static void mali_dvfs_work_handler(struct work_struct *w);
static struct workqueue_struct *mali_dvfs_wq = 0;
_mali_osk_mutex_t *mali_dvfs_lock;
_mali_osk_mutex_t *mali_isp_lock;
static DECLARE_WORK(mali_dvfs_work, mali_dvfs_work_handler);

int cpufreq_lock_by_mali(unsigned int freq)
{
#ifdef CONFIG_EXYNOS4_CPUFREQ
	unsigned int level;

	if (atomic_read(&mali_cpufreq_lock) == 0) {
		if (exynos_cpufreq_get_level(freq * 1000, &level)) {
			printk(KERN_ERR "Mali: failed to get cpufreq level for %dMHz", freq);
			return -EINVAL;
		}

		if (exynos_cpufreq_lock(DVFS_LOCK_ID_G3D, level)) {
			printk(KERN_ERR "Mali: failed to cpufreq lock for L%d", level);
			return -EINVAL;
		}

		atomic_set(&mali_cpufreq_lock, 1);
		printk(KERN_DEBUG "Mali: cpufreq locked on <%d>%dMHz\n", level, freq);
	}
#endif
	return 0;
}

void cpufreq_unlock_by_mali(void)
{
#ifdef CONFIG_EXYNOS4_CPUFREQ
	if (atomic_read(&mali_cpufreq_lock) == 1) {
		exynos_cpufreq_lock_free(DVFS_LOCK_ID_G3D);
		atomic_set(&mali_cpufreq_lock, 0);
		printk(KERN_DEBUG "Mali: cpufreq locked off\n");
	}
#endif
}

#ifdef CONFIG_REGULATOR
void mali_regulator_disable(void)
{
	if(IS_ERR_OR_NULL(g3d_regulator))
	{
		MALI_DEBUG_PRINT(1, ("error on mali_regulator_disable : g3d_regulator is null\n"));
		return;
	}
	regulator_disable(g3d_regulator);
}

void mali_regulator_enable(void)
{
	if(IS_ERR_OR_NULL(g3d_regulator))
	{
		MALI_DEBUG_PRINT(1, ("error on mali_regulator_enable : g3d_regulator is null\n"));
		return;
	}
	regulator_enable(g3d_regulator);
}

void mali_regulator_set_voltage(int min_uV, int max_uV)
{
	_mali_osk_mutex_wait(mali_dvfs_lock);
	if(IS_ERR_OR_NULL(g3d_regulator))
	{
		MALI_DEBUG_PRINT(1, ("error on mali_regulator_set_voltage : g3d_regulator is null\n"));
		_mali_osk_mutex_signal(mali_dvfs_lock);
		return;
	}
	MALI_DEBUG_PRINT(1, ("= regulator_set_voltage: %d, %d \n", min_uV, max_uV));
	regulator_set_voltage(g3d_regulator, min_uV, max_uV);
	mali_gpu_vol = regulator_get_voltage(g3d_regulator);
	MALI_DEBUG_PRINT(1, ("Mali voltage: %d\n", mali_gpu_vol));
	_mali_osk_mutex_signal(mali_dvfs_lock);
}
#endif

#ifdef CONFIG_MALI_DVFS
static unsigned int get_mali_dvfs_status(void)
{
	return maliDvfsStatus.currentStep;
}
#endif

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

	if (mout_epll_clock == NULL) {
		mout_epll_clock = clk_get(NULL, MOUTEPLLCLK_NAME);
		if (IS_ERR(mout_epll_clock)) {
			MALI_PRINT(("MALI Error : failed to get source mout_epll_clock\n"));
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

	if (mali_parent_clock == NULL) {
		mali_parent_clock = clk_get(NULL, GPUMOUT1CLK_NAME);
		if (IS_ERR(mali_parent_clock)) {
			MALI_PRINT(("MALI Error : failed to get source mali parent clock\n"));
			return MALI_FALSE;
		}
	}

	/* mali clock get always. */
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
	if (mali_parent_clock)
	{
		clk_put(mali_parent_clock);
		mali_parent_clock = NULL;
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

	if (mout_epll_clock)
	{
		clk_put(mout_epll_clock);
		mout_epll_clock = NULL;
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

mali_bool set_mali_dvfs_current_step(unsigned int step)
{
	_mali_osk_mutex_wait(mali_dvfs_lock);
	if (ENABLE_LOCK_BY_ISP) {
		maliDvfsStatus.currentStep = step % MALI_DVFS_STEPS_ISP;
		mali_isp_current_clock = mali_dvfs_isp[maliDvfsStatus.currentStep].clock;
	} else {
		maliDvfsStatus.currentStep = step % MALI_DVFS_STEPS;
	}
	_mali_osk_mutex_signal(mali_dvfs_lock);
	return MALI_TRUE;
}

mali_bool mali_clk_set_rate_isp(u32 step)
{
#ifdef EXYNOS4_ASV_ENABLED
	int lock_vol;
#endif
	unsigned long rate = (unsigned long)mali_dvfs_isp[step].clock * (unsigned long)GPU_MHZ;
#ifdef CONFIG_REGULATOR
	if (IS_ERR_OR_NULL(g3d_regulator)) {
		MALI_DEBUG_PRINT(1, ("error on mali_regulator_set_voltage : g3d_regulator is null\n"));
		return MALI_FALSE;
	}
#ifdef EXYNOS4_ASV_ENABLED
	lock_vol = get_match_volt(ID_G3D, mali_dvfs_isp[step].clock * GPU_ASV_VOLT);
	regulator_set_voltage(g3d_regulator, lock_vol, lock_vol);
	exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_dvfs_isp[step].clock * GPU_ASV_VOLT));
#else
	regulator_set_voltage(g3d_regulator, mali_dvfs_isp[step].vol, mali_dvfs_isp[step].vol);
#endif
	mali_gpu_vol = regulator_get_voltage(g3d_regulator);
	MALI_DEBUG_PRINT(1, ("Mali voltage: %d\n", mali_gpu_vol));
#endif
	if (mali_clk_get() == MALI_FALSE)
		return MALI_FALSE;

	if (atomic_read(&clk_active) == 0) {
		if (clk_enable(mali_clock) < 0) {
			return MALI_FALSE;
		}
		atomic_set(&clk_active, 1);
	}

#ifdef BUSFREQ_QOS_LOCK
	if (pm_qos_request_active(&mif_min_qos))
		pm_qos_update_request(&mif_min_qos, mali_dvfs_isp[step].mif_min_lock_value);
#endif

	clk_set_rate(mali_clock, rate);
	rate = clk_get_rate(mali_clock);
	mali_gpu_clk = (int)(rate / GPU_MHZ);
	mali_isp_current_level = step;
	mali_clk_put(MALI_FALSE);
	set_mali_dvfs_current_step(step);
	return MALI_TRUE;
}

void mali_clk_set_rate(unsigned int clk, unsigned int mhz)
{
	int err;
	unsigned int read_val;
	unsigned long rate = (unsigned long)clk * (unsigned long)mhz;

	_mali_osk_mutex_wait(mali_dvfs_lock);

	MALI_DEBUG_PRINT(3, ("Mali platform: Setting frequency to %d mhz\n", clk));

	if (mali_clk_get() == MALI_FALSE) {
		_mali_osk_mutex_signal(mali_dvfs_lock);
		return;
	}
	clk_set_parent(mali_parent_clock, mout_epll_clock);

	do {
		cpu_relax();
		read_val = __raw_readl(EXYNOS4_CLKMUX_STAT_G3D0);
	} while (((read_val >> 4) & 0x7) != 0x1);

	MALI_DEBUG_PRINT(3, ("Mali platform: set to EPLL EXYNOS4_CLKMUX_STAT_G3D0 : 0x%08x\n", __raw_readl(EXYNOS4_CLKMUX_STAT_G3D0)));

	err = clk_set_parent(sclk_vpll_clock, ext_xtal_clock);

	if (err)
		MALI_PRINT_ERROR(("sclk_vpll set parent to ext_xtal failed\n"));

	MALI_DEBUG_PRINT(3, ("Mali platform: set_parent_vpll : %8.x \n", (__raw_readl(EXYNOS4_CLKSRC_TOP0) >> 8) & 0x1));

	clk_set_rate(fout_vpll_clock, (unsigned int)clk * GPU_MHZ);
	clk_set_parent(vpll_src_clock, ext_xtal_clock);

	err = clk_set_parent(sclk_vpll_clock, fout_vpll_clock);

	if (err)
		MALI_PRINT_ERROR(("sclk_vpll set parent to fout_vpll failed\n"));

	MALI_DEBUG_PRINT(3, ("Mali platform: set_parent_vpll : %8.x \n", (__raw_readl(EXYNOS4_CLKSRC_TOP0) >> 8) & 0x1));

	clk_set_parent(mali_parent_clock, sclk_vpll_clock);

	do {
		cpu_relax();
		read_val = __raw_readl(EXYNOS4_CLKMUX_STAT_G3D0);
	} while (((read_val >> 4) & 0x7) != 0x2);

	MALI_DEBUG_PRINT(3, ("SET to VPLL EXYNOS4_CLKMUX_STAT_G3D0 : 0x%08x\n", __raw_readl(EXYNOS4_CLKMUX_STAT_G3D0)));

	clk_set_parent(mali_clock, mali_parent_clock);

	if (atomic_read(&clk_active) == 0) {
		if (clk_enable(mali_clock) < 0) {
			_mali_osk_mutex_signal(mali_dvfs_lock);
			return;
		}
		atomic_set(&clk_active, 1);
	}

	err = clk_set_rate(mali_clock, rate);

	if (err > 0)
		MALI_PRINT_ERROR(("Failed to set Mali clock: %d\n", err));

	rate = clk_get_rate(mali_clock);

	MALI_DEBUG_PRINT(1, ("Mali frequency %d\n", rate / mhz));
	GPU_MHZ = mhz;

	mali_gpu_clk = (int)(rate / mhz);
	mali_clk_put(MALI_FALSE);

	_mali_osk_mutex_signal(mali_dvfs_lock);
}

#ifdef CONFIG_MALI_DVFS
static mali_bool set_mali_dvfs_status(u32 step, mali_bool boostup)
{
	_mali_osk_mutex_wait(mali_isp_lock);
	if (ENABLE_LOCK_BY_ISP) {
		MALI_DEBUG_PRINT(1, ("DVFS is already locked by ISP\n"));
		if (mali_clk_set_rate_isp(step) == MALI_FALSE) {
			_mali_osk_mutex_signal(mali_isp_lock);
			return MALI_FALSE;
		}
		_mali_osk_mutex_signal(mali_isp_lock);
		return MALI_TRUE;
	} else if (mali_isp_current_clock > 0) {
		mali_isp_current_clock = 0;
	}

#ifdef BUSFREQ_QOS_LOCK
	if (pm_qos_request_active(&mif_min_qos))
		pm_qos_update_request(&mif_min_qos, mali_dvfs[step].mif_min_lock_value);
#endif

	if(boostup) {
#ifdef CONFIG_REGULATOR
		/*change the voltage*/
#ifdef EXYNOS4_ASV_ENABLED
		mali_regulator_set_voltage(get_match_volt(ID_G3D, mali_dvfs[step].clock * GPU_ASV_VOLT), get_match_volt(ID_G3D, mali_dvfs[step].clock * GPU_ASV_VOLT));
		exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_dvfs[step].clock * GPU_ASV_VOLT));
#else
		mali_regulator_set_voltage(mali_dvfs[step].vol, mali_dvfs[step].vol);
#endif
#endif
		/*change the clock*/
		mali_clk_set_rate(mali_dvfs[step].clock, mali_dvfs[step].freq);
	} else {
		/*change the clock*/
		mali_clk_set_rate(mali_dvfs[step].clock, mali_dvfs[step].freq);
#ifdef CONFIG_REGULATOR
#ifdef EXYNOS4_ASV_ENABLED
		/*change the voltage*/
		mali_regulator_set_voltage(get_match_volt(ID_G3D, mali_dvfs[step].clock * GPU_ASV_VOLT), get_match_volt(ID_G3D, mali_dvfs[step].clock * GPU_ASV_VOLT));
		exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_dvfs[step].clock * GPU_ASV_VOLT));
#else
		mali_regulator_set_voltage(mali_dvfs[step].vol, mali_dvfs[step].vol);
#endif
#endif
	}

#if defined(CONFIG_MALI400_PROFILING)
	_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE|
			MALI_PROFILING_EVENT_CHANNEL_GPU|
			MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
			mali_gpu_clk, mali_gpu_vol/1000, 0, 0, 0);
#endif
	mali_clk_put(MALI_FALSE);

	set_mali_dvfs_current_step(step);
	/*for future use*/
	maliDvfsStatus.pCurrentDvfs = &mali_dvfs[step];

#if CPUFREQ_LOCK_DURING_440
	/* lock/unlock CPU freq by Mali */
	if (mali_dvfs[step].clock >= 440)
		cpufreq_lock_by_mali(400);
	else
		cpufreq_unlock_by_mali();
#endif
	_mali_osk_mutex_signal(mali_isp_lock);
	return MALI_TRUE;
}

static void mali_platform_wating(u32 msec)
{
	/*
	* sample wating
	* change this in the future with proper check routine.
	*/
	unsigned int read_val;
	while(1) {
		read_val = _mali_osk_mem_ioread32(clk_register_map, 0x00);
		if ((read_val & 0x8000)==0x0000) break;

		_mali_osk_time_ubusydelay(100); /* 1000 -> 100 : 20101218 */
	}
}

static mali_bool change_mali_dvfs_status(u32 step, mali_bool boostup )
{
	MALI_DEBUG_PRINT(4, ("> change_mali_dvfs_status: %d, %d \n", step, boostup));

	if (!set_mali_dvfs_status(step, boostup)) {
		MALI_DEBUG_PRINT(1, ("error on set_mali_dvfs_status: %d, %d \n",step, boostup));
		return MALI_FALSE;
	}

	/* wait until clock and voltage is stablized */
	mali_platform_wating(MALI_DVFS_WATING); /* msec */
	return MALI_TRUE;
}

static unsigned int decideNextStatus(unsigned int utilization)
{
	unsigned int level = get_mali_dvfs_status();
	unsigned int max_level = MALI_DVFS_STEPS - 1;
	unsigned int min_level = 0;
	int iStepCount = 0;

	if (ENABLE_LOCK_BY_ISP) {
		if (level != mali_isp_current_level) {
			level = mali_isp_current_level;
			set_mali_dvfs_current_step(mali_isp_current_level);
		}

		if (utilization > (int)(256 * mali_dvfs_isp[mali_isp_current_level].upthreshold / 100) &&
				level < MALI_DVFS_STEPS_ISP - 1) {
			level++;
		} else if (utilization < (int)(256 * mali_dvfs_isp[mali_isp_current_level].downthreshold / 100) &&
			level > 0) {
			level--;
		}
		return level;
	} else if (mali_dvfs_control == 0) {
		if (mali_min_freq != 0 && mali_max_freq != 0) {
			if (mali_min_freq > mali_max_freq) {
				for (iStepCount = MALI_DVFS_STEPS - 1; iStepCount >= 0; iStepCount--) {
					if (mali_max_freq >= mali_dvfs[iStepCount].clock) {
						max_level = iStepCount;
						min_level = iStepCount;
						level = max_level;
						break;
					}
				}
			} else {
				for (iStepCount = MALI_DVFS_STEPS - 1; iStepCount >= 0; iStepCount--) {
					if (mali_min_freq >= mali_dvfs[iStepCount].clock) {
						min_level = iStepCount;
						if (level < min_level)
							level = min_level;
						break;
					}
				}
				for (iStepCount = MALI_DVFS_STEPS - 1; iStepCount >= 0; iStepCount--) {
					if (mali_max_freq >= mali_dvfs[iStepCount].clock) {
						max_level = iStepCount;
						if (level > max_level)
							level = max_level;
						break;
					}
				}
			}
		} else if (mali_min_freq != 0) {
			for (iStepCount = MALI_DVFS_STEPS - 1; iStepCount >= 0; iStepCount--) {
				if (mali_min_freq >= mali_dvfs[iStepCount].clock) {
					min_level = iStepCount;
					if (level < min_level)
						level = min_level;
					break;
				}
			}
		} else if (mali_max_freq != 0) {
			for (iStepCount = MALI_DVFS_STEPS - 1; iStepCount >= 0; iStepCount--) {
				if (mali_max_freq >= mali_dvfs[iStepCount].clock) {
					max_level = iStepCount;
					if (level > max_level)
						level = max_level;
					break;
				}
			}
		}

		if (utilization > (int)(256 * mali_dvfs[maliDvfsStatus.currentStep].upthreshold / 100) &&
			level < max_level) {
			level++;
		} else if (utilization < (int)(256 * mali_dvfs[maliDvfsStatus.currentStep].downthreshold / 100) &&
			level > min_level) {
			level--;
		}
	} else {
		for (iStepCount = MALI_DVFS_STEPS - 1; iStepCount >= 0; iStepCount--) {
			if (mali_dvfs_control >= mali_dvfs[iStepCount].clock) {
				level = iStepCount;
				break;
			}
		}
	}
	return level;
}

static mali_bool mali_dvfs_status(unsigned int utilization)
{
	unsigned int nextStatus = 0;
	unsigned int curStatus = 0;
	mali_bool boostup = MALI_FALSE;
	static int stay_count = 5;

	MALI_DEBUG_PRINT(4, ("> mali_dvfs_status: %d \n", utilization));

	/* decide next step */
	nextStatus = decideNextStatus(utilization);
	curStatus = get_mali_dvfs_status();

	MALI_DEBUG_PRINT(4, ("= curStatus %d, nextStatus %d\n", curStatus, nextStatus));
	/* if next status is same with current status, don't change anything */
	if (curStatus != nextStatus) {
		/*check if boost up or not*/
		if (curStatus < nextStatus) {
			boostup = 1;
			stay_count = 5;
		} else if (curStatus > nextStatus) {
			stay_count--;
		}

		if (boostup == 1 || stay_count <= 0 || ENABLE_LOCK_BY_ISP) {
			/*change mali dvfs status*/
			update_time_in_state(curStatus);
			if (!change_mali_dvfs_status(nextStatus, boostup)) {
				MALI_DEBUG_PRINT(1, ("error on change_mali_dvfs_status \n"));
				return MALI_FALSE;
			}
			boostup = 0;
			stay_count = 5;
		}
	} else {
		stay_count = 5;
	}

	return MALI_TRUE;
}
#endif

static void mali_dvfs_work_handler(struct work_struct *w)
{
	bMaliDvfsRun = 1;
	MALI_DEBUG_PRINT(3, ("=== mali_dvfs_work_handler\n"));

#ifdef CONFIG_MALI_DVFS
	if(!mali_dvfs_status(mali_dvfs_utilization))
		MALI_DEBUG_PRINT(1, ( "error on mali dvfs status in mali_dvfs_work_handler"));
#endif

	bMaliDvfsRun = 0;
}

mali_bool init_mali_dvfs_status(void)
{
	/*
	* default status
	* add here with the right function to get initilization value.
	*/
	if (!mali_dvfs_wq)
		mali_dvfs_wq = create_singlethread_workqueue("mali_dvfs");

	_mali_osk_atomic_init(&dvfslock_status, 0);

	/* add a error handling here */
	maliDvfsStatus.currentStep = MALI_DVFS_DEFAULT_STEP;

	return MALI_TRUE;
}

void deinit_mali_dvfs_status(void)
{
	if (mali_dvfs_wq)
		destroy_workqueue(mali_dvfs_wq);

	_mali_osk_atomic_term(&dvfslock_status);

	mali_dvfs_wq = NULL;
}

#ifdef CONFIG_MALI_DVFS
mali_bool mali_dvfs_handler(unsigned int utilization)
{
	mali_dvfs_utilization = utilization;
	queue_work_on(0, mali_dvfs_wq, &mali_dvfs_work);

	return MALI_TRUE;
}
#endif

static mali_bool init_mali_clock(void)
{
	mali_bool ret = MALI_TRUE;
	nPowermode = MALI_POWER_MODE_DEEP_SLEEP;

	if (mali_clock != 0)
		return ret; /* already initialized */

	mali_dvfs_lock = _mali_osk_mutex_init(_MALI_OSK_LOCKFLAG_ORDERED, 0);
	mali_isp_lock = _mali_osk_mutex_init(_MALI_OSK_LOCKFLAG_ORDERED, 0);

	if (mali_dvfs_lock == NULL || mali_isp_lock == NULL)
		return _MALI_OSK_ERR_FAULT;

	if (!mali_clk_get())
	{
		MALI_PRINT(("Error: Failed to get Mali clock\n"));
		goto err_clk;
	}

	clk_set_parent(vpll_src_clock, ext_xtal_clock);
	clk_set_parent(sclk_vpll_clock, fout_vpll_clock);
	clk_set_parent(mali_parent_clock, sclk_vpll_clock);
	clk_set_parent(mali_clock, mali_parent_clock);

	if (!atomic_read(&clk_active)) {
		if (clk_enable(mali_clock) < 0)	{
			MALI_PRINT(("Error: Failed to enable clock\n"));
			goto err_clk;
		}
		atomic_set(&clk_active, 1);
	}

	mali_clk_set_rate((unsigned int)mali_gpu_clk, GPU_MHZ);

	MALI_PRINT(("init_mali_clock mali_clock %x\n", mali_clock));

#ifdef CONFIG_REGULATOR
	g3d_regulator = regulator_get(NULL, "vdd_g3d");

	if (IS_ERR(g3d_regulator))
	{
		MALI_PRINT( ("MALI Error : failed to get vdd_g3d\n"));
		ret = MALI_FALSE;
		goto err_regulator;
	}

	mali_gpu_vol = mali_runtime_resume.vol;
#ifdef EXYNOS4_ASV_ENABLED
	mali_gpu_vol = get_match_volt(ID_G3D, mali_gpu_clk * GPU_ASV_VOLT);
	mali_runtime_resume.vol = get_match_volt(ID_G3D, mali_runtime_resume.clk * GPU_ASV_VOLT);
#endif

	regulator_enable(g3d_regulator);
	mali_regulator_set_voltage(mali_gpu_vol, mali_gpu_vol);

#ifdef EXYNOS4_ASV_ENABLED
	exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_runtime_resume.clk * GPU_ASV_VOLT));
#endif
#endif

#if defined(CONFIG_MALI400_PROFILING)
	_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE|
			MALI_PROFILING_EVENT_CHANNEL_GPU|
			MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
			mali_gpu_clk, mali_gpu_vol/1000, 0, 0, 0);
#endif
	mali_clk_put(MALI_FALSE);

	return MALI_TRUE;

#ifdef CONFIG_REGULATOR
err_regulator:
	regulator_put(g3d_regulator);
#endif
err_clk:
	mali_clk_put(MALI_TRUE);

	return ret;
}

static mali_bool deinit_mali_clock(void)
{
	if (mali_clock == 0)
		return MALI_TRUE;

#ifdef CONFIG_REGULATOR
	if (g3d_regulator)
	{
		regulator_put(g3d_regulator);
		g3d_regulator = NULL;
	}
#endif
	mali_clk_put(MALI_TRUE);

	return MALI_TRUE;
}

static _mali_osk_errcode_t enable_mali_clocks(void)
{
	int err;

	_mali_osk_mutex_wait(mali_isp_lock);
	if (atomic_read(&clk_active) == 0) {
		err = clk_enable(mali_clock);
		MALI_DEBUG_PRINT(3,("enable_mali_clocks mali_clock %p error %d \n", mali_clock, err));
		atomic_set(&clk_active, 1);
	}

	if (ENABLE_LOCK_BY_ISP) {
		MALI_DEBUG_PRINT(1, ("DVFS is already locked by ISP\n"));
#ifdef EXYNOS4_ASV_ENABLED
		if (samsung_rev() >= EXYNOS3470_REV_2_0)
			exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_dvfs_isp[mali_isp_current_level].clock * GPU_ASV_VOLT));
#endif
		mali_clk_set_rate_isp(mali_isp_current_level);
		_mali_osk_mutex_signal(mali_isp_lock);
		MALI_SUCCESS;
	}

	/* set clock rate */
#ifdef CONFIG_MALI_DVFS
#ifdef EXYNOS4_ASV_ENABLED
	if (samsung_rev() >= EXYNOS3470_REV_2_0)
		exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_runtime_resume.clk * GPU_ASV_VOLT));
#endif

	if (mali_dvfs_control != 0 || mali_gpu_clk >= mali_runtime_resume.clk) {
		mali_clk_set_rate(mali_gpu_clk, GPU_MHZ);
	} else {
#ifdef CONFIG_REGULATOR
		mali_regulator_set_voltage(mali_runtime_resume.vol, mali_runtime_resume.vol);
#ifdef EXYNOS4_ASV_ENABLED
		exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_runtime_resume.clk * GPU_ASV_VOLT));
#endif
#endif
		mali_clk_set_rate(mali_runtime_resume.clk, GPU_MHZ);
		set_mali_dvfs_current_step(mali_runtime_resume.step);
	}
#else
	mali_clk_set_rate((unsigned int)mali_gpu_clk, GPU_MHZ);
	maliDvfsStatus.currentStep = MALI_DVFS_DEFAULT_STEP;
#endif

#ifdef BUSFREQ_QOS_LOCK
	if (pm_qos_request_active(&mif_min_qos))
		pm_qos_update_request(&mif_min_qos, mali_dvfs[maliDvfsStatus.currentStep].mif_min_lock_value);
#endif

	_mali_osk_mutex_signal(mali_isp_lock);
	MALI_SUCCESS;
}

static _mali_osk_errcode_t disable_mali_clocks(void)
{
	_mali_osk_mutex_wait(mali_isp_lock);
#ifdef EXYNOS4_ASV_ENABLED
	if (samsung_rev() >= EXYNOS3470_REV_2_0)
		exynos_set_abb(ID_G3D, ABB_BYPASS);
#endif

	if (atomic_read(&clk_active)) {
		clk_disable(mali_clock);
		MALI_DEBUG_PRINT(3, ("disable_mali_clocks mali_clock %p\n", mali_clock));
		atomic_set(&clk_active, 0);
#ifdef BUSFREQ_QOS_LOCK
		pm_qos_update_request(&mif_min_qos, 0);
#endif
	}
	_mali_osk_mutex_signal(mali_isp_lock);
	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_init(struct device *dev)
{
	atomic_set(&clk_active, 0);
	MALI_CHECK(init_mali_clock(), _MALI_OSK_ERR_FAULT);

#ifdef CONFIG_MALI_DVFS

#ifdef BUSFREQ_QOS_LOCK
	pm_qos_add_request(&mif_min_qos, PM_QOS_BUS_THROUGHPUT, 0);
#endif

	/* Create sysfs for time-in-state */
	if (device_create_file(dev, &dev_attr_time_in_state)) {
		dev_err(dev, "Couldn't create sysfs file [time_in_state]\n");
	}

	if (!clk_register_map)
		clk_register_map = _mali_osk_mem_mapioregion(CLK_DIV_STAT_G3D, 0x20, CLK_DESC);

	if (!init_mali_dvfs_status())
		MALI_DEBUG_PRINT(1, ("mali_platform_init failed\n"));

	maliDvfsStatus.currentStep = MALI_DVFS_DEFAULT_STEP;
#endif
	mali_platform_power_mode_change(dev, MALI_POWER_MODE_ON);

	MALI_SUCCESS;
}

_mali_osk_errcode_t mali_platform_deinit(struct device *dev)
{
	mali_platform_power_mode_change(dev, MALI_POWER_MODE_DEEP_SLEEP);
	deinit_mali_clock();

#ifdef CONFIG_MALI_DVFS
	deinit_mali_dvfs_status();
	if (clk_register_map)
	{
		_mali_osk_mem_unmapioregion(CLK_DIV_STAT_G3D, 0x20, clk_register_map);
		clk_register_map = NULL;
	}
#endif

#ifdef BUSFREQ_QOS_LOCK
	pm_qos_remove_request(&mif_min_qos);
#endif
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
			enable_mali_clocks();

#if defined(CONFIG_MALI400_PROFILING)
			_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
			MALI_PROFILING_EVENT_CHANNEL_GPU |
			MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
			mali_gpu_clk, mali_gpu_vol/1000, 0, 0, 0);
#endif
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

#if defined(CONFIG_MALI400_PROFILING)
			_mali_osk_profiling_add_event(MALI_PROFILING_EVENT_TYPE_SINGLE |
			MALI_PROFILING_EVENT_CHANNEL_GPU |
			MALI_PROFILING_EVENT_REASON_SINGLE_GPU_FREQ_VOLT_CHANGE,
			0, 0, 0, 0, 0);
#endif
			nPowermode = power_mode;
		}
		break;
	}
	MALI_SUCCESS;
}

void mali_gpu_utilization_handler(struct mali_gpu_utilization_data *data)
{
	if (nPowermode == MALI_POWER_MODE_ON)
	{
#ifdef CONFIG_MALI_DVFS
		if(!mali_dvfs_handler(data->utilization_gpu))
			MALI_DEBUG_PRINT(1, ("error on mali dvfs status in utilization\n"));
#endif
	}
}

#ifdef CONFIG_MALI_DVFS
static void update_time_in_state(int level)
{
	u64 current_time;
	static u64 prev_time = 0;

	if (prev_time == 0)
		prev_time = get_jiffies_64();

	current_time = get_jiffies_64();
	mali_dvfs_time[level] += current_time - prev_time;
	prev_time = current_time;
}

ssize_t show_time_in_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;

	update_time_in_state(maliDvfsStatus.currentStep);

	for (i = 0; i < MALI_DVFS_STEPS; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d %llu\n",
						mali_dvfs[i].clock,
						mali_dvfs_time[i]);
	}

	if (ret < PAGE_SIZE - 1) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	} else {
		buf[PAGE_SIZE - 2] = '\n';
		buf[PAGE_SIZE - 1] = '\0';
		ret = PAGE_SIZE - 1;
	}
	return ret;
}

ssize_t set_time_in_state(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int i;

	for (i = 0; i < MALI_DVFS_STEPS; i++) {
		mali_dvfs_time[i] = 0;
	}
	return count;
}
#endif

int mali_dvfs_level_lock(void)
{
	int prev_status = _mali_osk_atomic_read(&dvfslock_status);
	unsigned long rate = (mali_dvfs_isp[MALI_DVFS_DEFAULT_STEP_ISP].clock * mali_dvfs_isp[MALI_DVFS_DEFAULT_STEP_ISP].freq);
	unsigned int read_val;
#ifdef EXYNOS4_ASV_ENABLED
    int lock_vol;
#endif

	if (prev_status < 0) {
		MALI_PRINT(("DVFS lock status is not valid for lock\n"));
		return -1;
	} else if (prev_status > 0) {
		MALI_PRINT(("DVFS lock already enabled\n"));
		return -1;
	}

	_mali_osk_mutex_wait(mali_isp_lock);

	ENABLE_LOCK_BY_ISP = 1;
	mali_isp_current_clock = mali_dvfs_isp[MALI_DVFS_DEFAULT_STEP_ISP].clock;
	mali_isp_current_level = MALI_DVFS_DEFAULT_STEP_ISP;
#ifdef CONFIG_REGULATOR
	if (IS_ERR_OR_NULL(g3d_regulator)) {
		MALI_DEBUG_PRINT(1, ("error on mali_regulator_set_voltage : g3d_regulator is null\n"));
		_mali_osk_mutex_signal(mali_isp_lock);
		return -1;
	}

#ifdef EXYNOS4_ASV_ENABLED
	lock_vol = get_match_volt(ID_G3D, mali_dvfs_isp[MALI_DVFS_DEFAULT_STEP_ISP].clock * GPU_ASV_VOLT);
	regulator_set_voltage(g3d_regulator, lock_vol, lock_vol);
	exynos_set_abb(ID_G3D, get_match_abb(ID_G3D, mali_dvfs_isp[MALI_DVFS_DEFAULT_STEP_ISP].clock * GPU_ASV_VOLT));
#else
	regulator_set_voltage(g3d_regulator, mali_dvfs_isp[MALI_DVFS_DEFAULT_STEP_ISP].vol, mali_dvfs_isp[MALI_DVFS_DEFAULT_STEP_ISP].vol);
#endif
	mali_gpu_vol = regulator_get_voltage(g3d_regulator);
	MALI_DEBUG_PRINT(1, ("Mali voltage: %d\n", mali_gpu_vol));
#endif
	if (mali_clk_get() == MALI_FALSE) {
		_mali_osk_mutex_signal(mali_isp_lock);
		return -1;
	}
	clk_set_rate(mali_clock, (clk_get_rate(mali_clock) / 2));
	clk_set_parent(mali_parent_clock, mout_epll_clock);

	do {
		cpu_relax();
		read_val = __raw_readl(EXYNOS4_CLKMUX_STAT_G3D0);
	} while (((read_val >> 4) & 0x7) != 0x1);

	clk_set_rate(fout_vpll_clock, mali_vpll_clk * GPU_MHZ);
	clk_set_parent(mali_parent_clock, sclk_vpll_clock);
	clk_set_parent(mali_clock, mali_parent_clock);

	do {
		cpu_relax();
		read_val = __raw_readl(EXYNOS4_CLKMUX_STAT_G3D0);
	} while (((read_val >> 4) & 0x7) != 0x2);

	if (atomic_read(&clk_active) == 0) {
		if (clk_enable(mali_clock) < 0) {
			return MALI_FALSE;
		}
		atomic_set(&clk_active, 1);
	}

	clk_set_rate(mali_clock, rate);
	rate = clk_get_rate(mali_clock);
	mali_gpu_clk = (int)(rate / GPU_MHZ);
	mali_clk_put(MALI_FALSE);
	_mali_osk_mutex_signal(mali_isp_lock);
	MALI_DEBUG_PRINT(1, ("DVFS is locked by ISP\n"));

	return _mali_osk_atomic_inc_return(&dvfslock_status);
}

int mali_dvfs_level_unlock(void)
{
	int prev_status = _mali_osk_atomic_read(&dvfslock_status);
	if (prev_status <= 0) {
		MALI_PRINT(("DVFS lock status is not valid for unlock\n"));
		return -1;
	} else if (prev_status >= 1) {
		_mali_osk_mutex_wait(mali_isp_lock);
		ENABLE_LOCK_BY_ISP = 0;
		mali_isp_current_level = 0;
		mali_gpu_clk = 160;
		MALI_DEBUG_PRINT(1, ("DVFS lock is released ISP\n"));
		_mali_osk_mutex_signal(mali_isp_lock);
	}
	return _mali_osk_atomic_dec_return(&dvfslock_status);
}
