/*
* @file sgpu_governor.h
* @copyright 2020 Samsung Electronics
*/

#ifndef _SGPU_GOVERNOR_H_
#define _SGPU_GOVERNOR_H_

#include <linux/pm_qos.h>
#include <governor.h>

#define HZ_PER_KHZ			1000

#if IS_ENABLED(CONFIG_EXYNOS_GPU_PROFILER)
#define WINDOW_MAX_SIZE			12
#define WEIGHT_TABLE_MAX_SIZE		11
#define WEIGHT_TABLE_IDX_NUM		2
#endif /* CONFIG_EXYNOS_GPU_PROFILER */

typedef enum {
	SGPU_DVFS_GOVERNOR_STATIC = 0,
	SGPU_DVFS_GOVERNOR_CONSERVATIVE,
	SGPU_DVFS_GOVERNOR_INTERACTIVE,
#if IS_ENABLED(CONFIG_EXYNOS_GPU_PROFILER)
	SGPU_DVFS_GOVERNOR_PROFILER,
#endif /* CONFIG_EXYNOS_GPU_PROFILER */
	SGPU_MAX_GOVERNOR_NUM,
} gpu_governor_type;

struct sgpu_governor_info {
	uint64_t	id;
	char		*name;
	int (*get_target)(struct devfreq *df, uint32_t *level);
	int (*clear)(struct devfreq *df, uint32_t level);
};

struct sgpu_governor_data {
	struct devfreq			*devfreq;
	struct amdgpu_device		*adev;
	struct notifier_block           nb_trans;
#ifdef CONFIG_DRM_SGPU_EXYNOS
	struct notifier_block		nb_tmu;
#endif
	struct dev_pm_qos_request	sys_pm_qos_min;
	struct dev_pm_qos_request	sys_pm_qos_max;
	unsigned long			sys_max_freq;
	unsigned long			sys_min_freq;

	struct timer_list		task_timer;
	struct task_struct		*update_task;
	/* msec */
	unsigned int			valid_time;
	bool				wakeup_lock;

	unsigned long			highspeed_freq;
	uint32_t			highspeed_load;
	uint32_t			highspeed_delay;
	uint32_t			highspeed_level;
	uint32_t			*downstay_times;
	uint32_t			*min_thresholds;
	uint32_t			*max_thresholds;

	/* current state */
	bool				in_suspend;
	struct sgpu_governor_info	*governor;
	unsigned long			min_freq;
	unsigned long			max_freq;
	int				current_level;
	unsigned long			expire_jiffies;
	unsigned long			expire_highspeed_delay;
	uint32_t			power_ratio;
	struct mutex			lock;

#if IS_ENABLED(CONFIG_EXYNOS_GPU_PROFILER)
	/* utilization window table */
	uint32_t			window[WINDOW_MAX_SIZE];
	uint32_t			window_idx;
	uint32_t			weight_table_idx[WEIGHT_TABLE_IDX_NUM];
	int				freq_margin;
#endif /* CONFIG_EXYNOS_GPU_PROFILER */

	/* %, additional weight for compute scenario */
	int				compute_weight;

	/* cl_boost */
	bool				cl_boost_disable;
	bool				cl_boost_status;
	uint32_t			cl_boost_level;
	unsigned long			cl_boost_freq;
	unsigned long			mm_min_clock;

#if IS_ENABLED(CONFIG_EXYNOS_ESCA_DVFS_MANAGER)
	int				dm_type;
	unsigned long			old_freq;
#endif /* CONFIG_EXYNOS_ESCA_DVFS_MANAGER */
};

ssize_t sgpu_governor_all_info_show(struct devfreq *df, char *buf);
ssize_t sgpu_governor_current_info_show(struct devfreq *df, char *buf,
					size_t size);
int sgpu_governor_change(struct devfreq *df, char *str_governor);
int sgpu_governor_init(struct device *dev, struct devfreq_dev_profile *dp,
		       struct sgpu_governor_data **governor_data);
void sgpu_governor_deinit(struct devfreq *df);
unsigned int *sgpu_get_array_data(struct devfreq_dev_profile *dp, const char *buf);

#endif
