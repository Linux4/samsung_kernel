#ifndef __EXYNOS_PROFILER_DEFS_H__
#define __EXYNOS_PROFILER_DEFS_H__

#include <linux/types.h>
#include <linux/ktime.h>

#if IS_ENABLED(CONFIG_SOC_S5E8845)
#include <dt-bindings/soc/samsung/s5e8845-profiler-dt.h>
#elif IS_ENABLED(CONFIG_SOC_S5E9935)
#include <dt-bindings/soc/samsung/s5e9935-profiler-dt.h>
#else
#include <dt-bindings/soc/samsung/s5e9945-profiler-dt.h>
#endif

#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_LEGACY)
#define PROFILER_VERSION 0
#else
#define PROFILER_VERSION 1
#endif

#ifndef PROFILER_UNUSED
#define PROFILER_UNUSED(x)  ((void)(x))
#endif

/* Common Defs */

#define EPOLL_MAX_PID 4

#define PROFILER_STATE_SLEEP 0
#define PROFILER_STATE_GFX 1
#define PROFILER_STATE_JOB_SUBMIT 2
#define PROFILER_STATE_MONITOR 4

#define REQ_PROFILE 1

#define CONTROL_CMD_PERF_DN_RATIO 1
#define CONTROL_CMD_PERF_UP_RATIO 2
#define CONTROL_CMD_PERF_MAINBW_THR 3
#define CONTROL_CMD_DISABLE_LLC_WAY 4
#define CONTROL_CMD_ENABLE_PROFILER_GOVERNOR 5
#define CONTROL_CMD_MAX_LLC_WAY 6
#define CONTROL_CMD_LIGHT_LLC_WAY 7

#define DEF_PERF_MAINBW_THR 2000
#define DEF_PERF_GPUBW_UP_RATIO 30
#define DEF_PERF_GPUBW_DN_RATIO 25

/* Orgin from exynos-profiler.h */
#define RATIO_UNIT		1000

#define nsec_to_usec(time)	((time) / 1000)
#define khz_to_mhz(freq)	((freq) / 1000)
#define ST_COST_BASE_TEMP_WEIGHT	(70 * 70)

/*
 * Input table should be DECENSING-ORDER
 * RELATION_LOW : return idx of freq lower than or same with input freq
 * RELATOIN_HIGH: return idx of freq higher thant or same with input freq
 */
#define RELATION_LOW	0
#define RELATION_HIGH	1

#define STATE_SCALE_CNT			(1 << 0)	/* ramainnig cnt even if freq changed */
#define STATE_SCALE_TIME		(1 << 1)	/* scaling up/down time with changed freq */
#define	STATE_SCALE_WITH_SPARE		(1 << 2)	/* freq boost with spare cap */
#define	STATE_SCALE_WO_SPARE		(1 << 3)	/* freq boost without spare cap */
#define	STATE_SCALE_WITH_ORG_CAP	(1 << 4)	/* freq boost with original capacity */

#define PWR_COST_CFVV	0
#define PWR_COST_CVV	1

/* Shared variables between profiler & external devices */
#define NUM_OF_PID 32

enum gpu_timeinfo {
	PREAVG,
	PREMAX,

	CPUAVG,
	CPUMAX,

	V2SAVG,
	V2SMAX,

	GPUAVG,
	GPUMAX,

	V2FAVG,
	V2FMAX,

	NUM_OF_TIMEINFO,
};

enum user_type {
	SYSFS,
	PROFILER,
	NUM_OF_USER,
};

enum service_type {
	SVC_GFX,
#if IS_ENABLED(CONFIG_EXYNOS_PROFILER_GFLOW)
	SVC_JOB_SUBMIT,
#endif
	SVC_RESERVED,
	NUM_OF_PROFILER_SVC,
};

enum cstate {
	CS_ACTIVE,
	CLK_OFF,
	PWR_OFF,
	NUM_OF_CSTATE,
};

/* Structure for FREQ */
struct freq_table {
	u32	freq;		/* KHz */
	u32	volt;		/* uV */

	/*
	 * Equation for cost
	 * CPU/GPU : Dyn_Coeff/St_Coeff * F(MHz) * V(mV)^2
	 * MIF	   : Dyn_Coeff/St_Coeff * V(mV)^2
	 */
	u64	dyn_cost;
	u64	st_cost;
};

/*
 * It is free-run count
 * NOTICE: MUST guarantee no overflow
 */
struct freq_cstate {
	ktime_t	*time[NUM_OF_CSTATE];
};
struct freq_cstate_snapshot {
	ktime_t	last_snap_time;
	ktime_t	*time[NUM_OF_CSTATE];
};
struct freq_cstate_result {
	ktime_t	profile_time;

	ktime_t	*time[NUM_OF_CSTATE];
	u32	ratio[NUM_OF_CSTATE];
	u32	freq[NUM_OF_CSTATE];

	u64	dyn_power;
	u64	st_power;
};

struct private_fn_cpu {
	s32 (*cpu_active_pct)(s32 id, s32 *cpu_active_pct);
	s32 (*cpu_asv_ids)(s32 id);
};

struct private_fn_gpu {
#if (PROFILER_VERSION == 0)
	u64 (*get_timeinfo)(int idx);
	u16 (*get_pidinfo)(int idx);
	void (*set_targettime_margin)(int);
	void (*set_util_margin)(int);
	void (*set_decon_time)(int);
	void (*set_comb_ctrl)(int);
	u32 (*get_target_cpuid)(u64 *info);
	void (*set_cputracer_size)(int);
#else
	void (*get_timeinfo)(u64 *info);
	void (*get_pidinfo)(u32 *pid_list, u8 *core_list);
#endif
	void (*set_targetframetime)(int);
	u64 (*get_gpu_hw_status)(void);
};

struct private_fn_mif {
	u64 (*get_stats0_sum)(void);
	u64 (*get_stats0_ratio)(void);
	u64 (*get_stats0_avg)(void);
	u64 (*get_stats1_sum)(void);
	u64 (*get_stats1_ratio)(void);
	u64 (*get_stats2_sum)(void);
	u64 (*get_stats2_ratio)(void);
	u64 (*get_llc_status)(void);
};

struct domain_fn {
	u32 (*get_table_cnt)(s32 id);
	u32 (*get_freq_table)(s32 id, u32 *table);
	u32 (*get_max_freq)(s32 id);
	u32 (*get_min_freq)(s32 id);
	u32 (*get_freq)(s32 id);
	void (*get_power)(s32 id, u64 *dyn_power, u64 *st_power);
	void (*get_power_change)(s32 id, s32 freq_delta_ratio,
		u32 *freq, u64 *dyn_power, u64 *st_power);
	u32 (*get_active_pct)(s32 id);
	s32 (*get_temp)(s32 id);
	void (*set_margin)(s32, s32 margin);
	u32 (*update_mode)(s32 id, int mode);
};

#define for_each_cstate(state)		for (state = 0; state < NUM_OF_CSTATE; state++)

#endif /*__EXYNOS_PROFILER_DEFS_H__*/
