#ifndef __EXYNOS_MIF_PROFILER_H__
#define __EXYNOS_MIF_PROFILER_H__

#include <exynos-profiler-if.h>

/* Result during profile time */
struct mif_profile_result {
	struct freq_cstate_result	fc_result;

	s32			cur_temp;
	s32			avg_temp;

	/* private data */
	u64			*tdata_in_state;
	u64			freq_stats0_sum;
	u64			freq_stats0_ratio;
	u64			freq_stats0_avg;
	u64			freq_stats1_sum;
	u64			freq_stats1_ratio;
	u64			freq_stats2_sum;
	u64			freq_stats2_ratio;
	int			llc_status;
};

static struct profiler {
	struct device_node	*root;

	int			enabled;

	s32			profiler_id;
	u32			cal_id;
	u32			devfreq_type;

	struct freq_table	*table;
	u32			table_cnt;
	u32			dyn_pwr_coeff;
	u32			st_pwr_coeff;

	const char			*tz_name;		/* currently do not use in MIF */
	struct thermal_zone_device	*tz;			/* currently do not use in MIF */

	struct freq_cstate		fc;			/* latest time_in_state info */
	struct freq_cstate_snapshot	fc_snap[NUM_OF_USER];	/* previous time_in_state info */

#if defined(CONFIG_SOC_S5E9925_EVT0) || defined(CONFIG_SOC_S5E8825)
	u64			*freq_stats[4];
	u64			*freq_stats_snap[4];
#else
	struct exynos_wow_profile	prev_wow_profile;
	u64			*prev_tdata_in_state;
#endif

	u32			cur_freq_idx;	/* current freq_idx */
	u32			max_freq_idx;	/* current max_freq_idx */
	u32			min_freq_idx;	/* current min_freq_idx */

	struct mif_profile_result	result[NUM_OF_USER];

	struct kobject		kobj;

	struct exynos_devfreq_freq_infos freq_infos;
} profiler;

#endif /* __EXYNOS_MIF_PROFILER_H__ */