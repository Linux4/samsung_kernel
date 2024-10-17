#ifndef __EXYNOS_CPU_PROFILER_H__
#define __EXYNOS_CPU_PROFILER_H__

#include <trace/hooks/cpuidle.h>
#include <trace/hooks/cpufreq.h>

#include <exynos-profiler-if.h>

/* Result during profile time */
struct cpu_profile_result {
	struct freq_cstate_result	fc_result;

	s32				cur_temp;
	s32				avg_temp;
};

struct cpu_profiler {
	raw_spinlock_t			lock;
	struct domain_profiler		*dom;

	int				enabled;

	s32				cur_cstate;		/* this cpu cstate */
	ktime_t				last_update_time;

	struct freq_cstate		fc;
	struct freq_cstate_snapshot	fc_snap[NUM_OF_USER];
	struct cpu_profile_result		result[NUM_OF_USER];
};

struct domain_profiler {
	struct list_head		list;
	struct cpumask			cpus;		/* cpus in this domain */
	struct cpumask			online_cpus;	/* online cpus in this domain */

	int				enabled;

	s32				profiler_id;
	u32				cal_id;

	/* DVFS Data */
	struct freq_table		*table;
	u32				table_cnt;
	u32				cur_freq_idx;	/* current freq_idx */
	u32				max_freq_idx;	/* current max_freq_idx */
	u32				min_freq_idx;	/* current min_freq_idx */

	/* Power Data */
	u32				dyn_pwr_coeff;
	u32				st_pwr_coeff;

	/* Thermal Data */
	const char			*tz_name;
	struct thermal_zone_device	*tz;

	/* Profile Result */
	struct cpu_profile_result		result[NUM_OF_USER];
};

static struct profiler {
	struct device_node		*root;

	struct list_head		list;	/* list for domain */
	struct cpu_profiler __percpu	*cpus;	/* cpu data for all cpus */

	struct mutex			mode_change_lock;

	struct kobject			*kobj;
} profiler;
#endif /* __EXYNOS_CPU_PROFILER_H__ */
