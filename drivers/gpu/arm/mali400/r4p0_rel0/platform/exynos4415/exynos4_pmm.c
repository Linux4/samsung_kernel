/* drivers/gpu/mali400/mali/platform/exynos4415/exynos4_pmm.c
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
#include <mach/regs-clock-exynos4415.h>
#include <mach/asv-exynos.h>
#ifdef CONFIG_CPU_FREQ
#ifndef CONFIG_SOC_EXYNOS4415
#define EXYNOS4_ASV_ENABLED
#endif
#endif

#ifdef CONFIG_MALI_DVFS_FULL_LEVEL
#define MALI_DVFS_FULL_LEVEL
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

#ifdef MALI_DVFS_FULL_LEVEL
#define MALI_DVFS_STEPS 7
#else
#define MALI_DVFS_STEPS 4
#endif

#define MALI_DVFS_WATING 10 /* msec */
#define MALI_DVFS_DEFAULT_STEP 1
#define PD_G3D_LOCK_FLAG 2

#define MALI_DVFS_CLK_DEBUG 0
#define CPUFREQ_LOCK_DURING_440 1

static int bMaliDvfsRun = 0;

typedef struct mali_dvfs_tableTag{
	unsigned int clock;
	unsigned int freq;
	unsigned int vol;
	unsigned int downthreshold;
	unsigned int upthreshold;
}mali_dvfs_table;

typedef struct mali_dvfs_statusTag{
	unsigned int currentStep;
	mali_dvfs_table * pCurrentDvfs;

} mali_dvfs_status_t;

/*dvfs status*/
mali_dvfs_status_t maliDvfsStatus;
int mali_dvfs_control;

typedef struct mali_runtime_resumeTag{
		int clk;
		int vol;
		unsigned int step;
}mali_runtime_resume_table;

mali_runtime_resume_table mali_runtime_resume = {266, 875000, 1};

#ifdef MALI_DVFS_FULL_LEVEL
/*dvfs table updated on 131203*/
mali_dvfs_table mali_dvfs[MALI_DVFS_STEPS]={
	/*step 0*/{160, 1000000,  850000,  0,  60},
	/*step 1*/{266, 1000000,  875000, 52,  70},
	/*step 2*/{350, 1000000,  950000, 62,  80},
	/*step 3*/{440, 1000000, 1025000, 75,  80},
	/*step 4*/{550, 1000000, 1125000, 85,  90},
	/*step 5*/{667, 1000000, 1225000, 85,  95},
	/*step 6*/{733, 1000000, 1300000, 95, 100} };
#else
/*dvfs table updated on 131203*/
mali_dvfs_table mali_dvfs[MALI_DVFS_STEPS]={
	/*step 0*/{160, 1000000,  850000,  0,  70},
	/*step 1*/{266, 1000000,  875000, 62,  90},
	/*step 2*/{350, 1000000,  950000, 85,  90},
	/*step 3*/{440, 1000000, 1025000, 85, 100} };
#endif

/* Exynos4415 */
int mali_gpu_clk = 266;
int mali_gpu_vol = 875000;
#ifdef MALI_DVFS_FULL_LEVEL
char *mali_freq_table = "733 667 550 440 350 266 160";
#else
char *mali_freq_table = "440 350 266 160";
#endif

#define EXTXTALCLK_NAME  "ext_xtal"
#define VPLLSRCCLK_NAME  "vpll_src"
#define FOUTVPLLCLK_NAME "fout_vpll"
#define SCLKEPLLCLK_NAME "sclk_epll"
#define SCLVPLLCLK_NAME  "sclk_vpll"
#define GPUMOUT1CLK_NAME "mout_g3d1"
#define GPUMOUT0CLK_NAME "mout_g3d0"
#define GPUCLK_NAME      "sclk_g3d"
#define CLK_DIV_STAT_G3D 0x1003C62C
#define CLK_DESC         "clk-divider-status"

static struct clk *ext_xtal_clock = NULL;
static struct clk *vpll_src_clock = NULL;
static struct clk *fout_vpll_clock = NULL;
static struct clk *sclk_epll_clock = NULL;
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
module_param(mali_freq_table, charp, S_IRUSR | S_IRGRP | S_IROTH);
#ifdef CONFIG_MALI_DVFS
module_param(mali_dvfs_control, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP| S_IROTH); /* rw-rw-r-- */
MODULE_PARM_DESC(mali_dvfs_control, "Mali Current DVFS");
DEVICE_ATTR(time_in_state, S_IRUGO|S_IWUSR, show_time_in_state, set_time_in_state);
MODULE_PARM_DESC(time_in_state, "Time-in-state of Mali DVFS");
#endif
MODULE_PARM_DESC(mali_gpu_clk, "Mali Current Clock");
MODULE_PARM_DESC(mali_gpu_vol, "Mali Current Voltage");
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
int mali_runtime_resumed = -1;
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

static unsigned int get_mali_dvfs_status(void)
{
	return maliDvfsStatus.currentStep;
}

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

	if (sclk_epll_clock == NULL) {
		sclk_epll_clock = clk_get(NULL, SCLKEPLLCLK_NAME);
		if (IS_ERR(fout_vpll_clock)) {
			MALI_PRINT(("MALI Error : failed to get source sclk_epll_clock\n"));
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

	if (sclk_epll_clock)
	{
		clk_put(sclk_epll_clock);
		sclk_epll_clock = NULL;
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
	unsigned int read_val;
	unsigned long rate = (unsigned long)clk * (unsigned long)mhz;

	_mali_osk_mutex_wait(mali_dvfs_lock);
	MALI_DEBUG_PRINT(3, ("Mali platform: Setting frequency to %d mhz\n", clk));

	if (mali_clk_get() == MALI_FALSE) {
		_mali_osk_mutex_signal(mali_dvfs_lock);
		return;
	}

	err = clk_set_parent(mali_parent_clock, sclk_epll_clock);

	if (err)
		MALI_PRINT_ERROR(("mali_parent set parent to sclk_epll failed\n"));

	do {
		cpu_relax();
		read_val = __raw_readl(EXYNOS4415_CLKMUX_STAT_G3D0);
	} while (((read_val >> 4) & 0x7) != 0x1);

	MALI_DEBUG_PRINT(3, ("Mali platform: set to EPLL EXYNOS4415_CLKMUX_STAT_G3D0: 0x%08x\n", __raw_readl(EXYNOS4415_CLKMUX_STAT_G3D0)));

	err = clk_set_rate(fout_vpll_clock, (unsigned int)clk * GPU_MHZ);

	if (err > 0)
		MALI_PRINT_ERROR(("Failed to set fout_vpll clock: %d\n", err));


	err = clk_set_parent(vpll_src_clock, ext_xtal_clock);

	if (err)
		MALI_PRINT_ERROR(("vpll_src set parent to ext_xtal failed\n"));

	err = clk_set_parent(sclk_vpll_clock, fout_vpll_clock);

	if (err)
		MALI_PRINT_ERROR(("sclk_vpll set parent to fout_vpll failed\n"));

	MALI_DEBUG_PRINT(3, ("Mali platform: set_parent_vpll : %8.x \n", (__raw_readl(EXYNOS4_CLKSRC_TOP0) >> 8) & 0x1));

	err = clk_set_parent(mali_parent_clock, sclk_vpll_clock);

	if (err)
		MALI_PRINT_ERROR(("mali_parent set parent to sclk_vpll failed\n"));

	do {
		cpu_relax();
		read_val = __raw_readl(EXYNOS4415_CLKMUX_STAT_G3D0);
	} while (((read_val >> 4) & 0x7) != 0x2);

	MALI_DEBUG_PRINT(3, ("SET to VPLL EXYNOS4_CLKMUX_STAT_G3D0 : 0x%08x\n", __raw_readl(EXYNOS4415_CLKMUX_STAT_G3D0)));

	err = clk_set_parent(mali_clock, mali_parent_clock);

	if (err)
		MALI_PRINT_ERROR(("mali_clock set parent to mali_parent failed\n"));

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

mali_bool set_mali_dvfs_current_step(unsigned int step)
{
	_mali_osk_mutex_wait(mali_dvfs_lock);
	maliDvfsStatus.currentStep = step % MALI_DVFS_STEPS;
	_mali_osk_mutex_signal(mali_dvfs_lock);
	return MALI_TRUE;
}

static mali_bool set_mali_dvfs_status(u32 step,mali_bool boostup)
{
	u32 validatedStep = step;

	if(boostup)	{
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

	set_mali_dvfs_current_step(validatedStep);
	/*for future use*/
	maliDvfsStatus.pCurrentDvfs = &mali_dvfs[validatedStep];

#if CPUFREQ_LOCK_DURING_440
	/* lock/unlock CPU freq by Mali */
	if (mali_dvfs[step].clock >= 440)
		cpufreq_lock_by_mali(400);
	else
		cpufreq_unlock_by_mali();
#endif
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
	static unsigned int level = 0;
	int iStepCount = 0;

	if (mali_runtime_resumed >= 0) {
		level = mali_runtime_resumed;
		mali_runtime_resumed = -1;
	}

	if (mali_dvfs_control == 0) {
		if (utilization > (int)(255 * mali_dvfs[maliDvfsStatus.currentStep].upthreshold / 100) &&
				level < MALI_DVFS_STEPS - 1) {
			level++;
		} else if (utilization < (int)(255 * mali_dvfs[maliDvfsStatus.currentStep].downthreshold / 100) &&
			level > 0) {
			level--;
		}
	} else {
		for (iStepCount = MALI_DVFS_STEPS-1; iStepCount >= 0; iStepCount--) {
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
	curStatus = get_mali_dvfs_status();
	nextStatus = decideNextStatus(utilization);

	MALI_DEBUG_PRINT(4, ("= curStatus %d, nextStatus %d, maliDvfsStatus.currentStep %d \n", curStatus, nextStatus, maliDvfsStatus.currentStep));
	/* if next status is same with current status, don't change anything */
	if(curStatus != nextStatus) {
		/*check if boost up or not*/
		if(maliDvfsStatus.currentStep < nextStatus) {
			boostup = 1;
			stay_count = 5;
		} else if (maliDvfsStatus.currentStep > nextStatus){
			stay_count--;
		}

		if( boostup == 1 || stay_count <= 0){
			/*change mali dvfs status*/
#ifdef CONFIG_MALI_DVFS
			update_time_in_state(curStatus);
#endif
			if (!change_mali_dvfs_status(nextStatus, boostup)) {
				MALI_DEBUG_PRINT(1, ("error on change_mali_dvfs_status \n"));
				return MALI_FALSE;
			}
			boostup = 0;
			stay_count = 5;
		}
	}
	else
		stay_count = 5;

	return MALI_TRUE;
}

static void mali_dvfs_work_handler(struct work_struct *w)
{
	bMaliDvfsRun = 1;
	MALI_DEBUG_PRINT(3, ("=== mali_dvfs_work_handler\n"));

	if(!mali_dvfs_status(mali_dvfs_utilization))
		MALI_DEBUG_PRINT(1, ( "error on mali dvfs status in mali_dvfs_work_handler"));

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

	/* add a error handling here */
	maliDvfsStatus.currentStep = MALI_DVFS_DEFAULT_STEP;

	return MALI_TRUE;
}

void deinit_mali_dvfs_status(void)
{
	if (mali_dvfs_wq)
		destroy_workqueue(mali_dvfs_wq);

	mali_dvfs_wq = NULL;
}

mali_bool mali_dvfs_handler(unsigned int utilization)
{
	mali_dvfs_utilization = utilization;
	queue_work_on(0, mali_dvfs_wq, &mali_dvfs_work);

	return MALI_TRUE;
}

static mali_bool init_mali_clock(void)
{
	mali_bool ret = MALI_TRUE;
	nPowermode = MALI_POWER_MODE_DEEP_SLEEP;

	if (mali_clock != 0)
		return ret; /* already initialized */

	mali_dvfs_lock = _mali_osk_mutex_init(_MALI_OSK_LOCKFLAG_ORDERED, 0);

	if (mali_dvfs_lock == NULL)
		return _MALI_OSK_ERR_FAULT;

	if (!mali_clk_get())
	{
		MALI_PRINT(("Error: Failed to get Mali clock\n"));
		goto err_clk;
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

	if (atomic_read(&clk_active) == 0) {
		err = clk_enable(mali_clock);
		MALI_DEBUG_PRINT(3,("enable_mali_clocks mali_clock %p error %d \n", mali_clock, err));
		atomic_set(&clk_active, 1);
	}

	/* set clock rate */
#ifdef CONFIG_MALI_DVFS
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
	atomic_set(&clk_active, 0);

#ifdef CONFIG_MALI_DVFS
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
