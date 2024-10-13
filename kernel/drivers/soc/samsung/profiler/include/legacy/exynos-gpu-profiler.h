#ifndef __EXYNOS_GPU_PROFILER_H__
#define __EXYNOS_GPU_PROFILER_H__

#include <exynos-profiler-if.h>

/* Result during profile time */
struct gpu_profile_result {
	struct freq_cstate_result	fc_result;

	s32			cur_temp;
	s32			avg_temp;

	/* Times Info. */
	u64                     rtimes[NUM_OF_TIMEINFO];
	u16                     pid_list[NUM_OF_PID];
};

static struct profiler {
	struct device_node	*root;

	int			enabled;

	s32			profiler_id;
	u32			cal_id;

	struct freq_table	*table;
	u32			table_cnt;
	u32			dyn_pwr_coeff;
	u32			st_pwr_coeff;

	const char			*tz_name;
	struct thermal_zone_device	*tz;

	struct freq_cstate		fc;
	struct freq_cstate_snapshot	fc_snap[NUM_OF_USER];

	u32			cur_freq_idx;	/* current freq_idx */
	u32			max_freq_idx;	/* current max_freq_idx */
	u32			min_freq_idx;	/* current min_freq_idx */

	struct gpu_profile_result	result[NUM_OF_USER];

	struct kobject		kobj;

	u64			gpu_hw_status;
} profiler;
#endif /* __EXYNOS_GPU_PROFILER_H__ */
