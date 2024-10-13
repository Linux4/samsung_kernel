#ifndef __EXYNOS_MAIN_PROFILER_H__
#define __EXYNOS_MAIN_PROFILER_H__

#include "../../../../../../kernel/sched/ems/ems.h"
#include "../../../../../../kernel/sched/sched.h"

#include <exynos-profiler-if.h>

#include <legacy/exynos-profiler-shared.h>

/******************************************************************************/
/*                            Macro for profiler                             */
/******************************************************************************/
#define for_each_mdomain(dom, id)	\
	for (id = -1; (dom) = next_domain(&id), id < NUM_OF_DOMAIN;)
/******************************************************************************/
/*                            Structure for profiler                             */
/******************************************************************************/

enum profile_mode {
	PROFILE_FINISH,
	PROFILE_START_BY_GAME,
	PROFILE_START_BY_GPU,
	PROFILE_UPDATE,
	PROFILE_INVALID,
};

/* Structure for IP's Private Data */
struct private_data_cpu {
	struct private_fn_cpu *fn;

	s32	pm_qos_cpu;
	struct freq_qos_request pm_qos_min_req;
	struct freq_qos_request pm_qos_max_req;
	s32	pm_qos_min_freq;
	s32	pm_qos_max_freq;
	s32	hp_minlock_low_limit;
	s32	lp_minlock_low_limit;

	s32	pid_util_max;
	s32	pid_util_min;
	s32	pid_util_lpmax;
	s32	pid_util_lpmin;

	s32	num_of_cpu;
};

struct private_data_gpu {
	struct private_fn_gpu *fn;

	struct exynos_pm_qos_request pm_qos_max_req;
	struct exynos_pm_qos_request pm_qos_min_req;
	s32	pm_qos_max_class;
	s32	pm_qos_max_freq;
	s32	pm_qos_min_class;
	s32	pm_qos_min_freq;

	/* Tunables */
	u64	gpu_active_pct_thr;
};

struct private_data_mif {
	struct private_fn_mif *fn;

	struct exynos_pm_qos_request pm_qos_min_req;
	struct exynos_pm_qos_request pm_qos_max_req;
	s32	pm_qos_min_class;
	s32	pm_qos_max_class;
	s32	pm_qos_min_freq;
	s32	pm_qos_max_freq;
	s32	hp_minlock_low_limit;

	/* Tunables */
	s32	stats0_mode_min_freq;
	u64	stats0_sum_thr;
	u64	stats0_updown_delta_pct_thr;

	s32	pid_util_max;
	s32	pid_util_min;
	s32	pid_util_lpmax;
	s32	pid_util_lpmin;
};

struct domain_data {
	bool			enabled;
	s32			id;

	struct domain_fn	*fn;

	void			*private;

	struct attribute_group	attr_group;
};

struct device_file_operation {
	struct file_operations          fops;
	struct miscdevice               miscdev;
};

static struct profiler {
	bool				running;
	bool				forced_running;
	bool				disable;
	bool				game_mode;
	bool				heavy_gpu_mode;
	u32				cur_mode;

	int				bts_idx;
	int				len_mo_id;
	int				*mo_id;

	bool				llc_enable;

	struct domain_data		domains[NUM_OF_DOMAIN];

	ktime_t				start_time;
	ktime_t				end_time;

	u64				start_frame_cnt;
	u64				end_frame_cnt;
	u64				start_frame_vsync_cnt;
	u64				end_frame_vsync_cnt;
	u64				start_fence_cnt;
	u64				end_fence_cnt;
	void				(*get_frame_cnt)(u64 *cnt, ktime_t *time);
	void				(*get_vsync_cnt)(u64 *cnt, ktime_t *time);
	void				(*get_fence_cnt)(u64 *cnt, ktime_t *time);

	s32				max_fps;

	struct work_struct		fps_work;

	bool				was_gpu_heavy;
	ktime_t				heavy_gpu_start_time;
	ktime_t				heavy_gpu_end_time;
	s32				heavy_gpu_ms_thr;
	s32				gpu_freq_thr;
	s32				fragutil;
	s32				fragutil_upper_thr;
	s32				fragutil_lower_thr;
	struct work_struct		fragutil_work;

	struct device_node		*dn;
	struct kobject			*kobj;

	u32				running_event;
	struct device_file_operation 	gov_fops;
	wait_queue_head_t		wq;
} profiler;

struct fps_profile {
	/* store last profile average fps */
	int		start;
	ktime_t		profile_time;	/* sec */
	u64		fps;
} fps_profile;

/*  shared data with Platform */
struct profile_sharing_data psd;
struct delta_sharing_data dsd;
struct tunable_sharing_data tsd;

static struct mutex profiler_dsd_lock;	/* SHOULD: should check to merge profiler_lock and profiler_dsd_lock */

/******************************************************************************/
/*                                      fops                                  */
/******************************************************************************/
#define IOCTL_MAGIC	'M'
#define IOCTL_READ_TSD	_IOR(IOCTL_MAGIC, 0x50, struct tunable_sharing_data)
#define IOCTL_READ_PSD	_IOR(IOCTL_MAGIC, 0x51, struct profile_sharing_data)
#define IOCTL_WR_DSD	_IOWR(IOCTL_MAGIC, 0x52, struct delta_sharing_data)

#endif /* __EXYNOS_MAIN_PROFILER_H__ */