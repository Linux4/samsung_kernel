#ifndef __EXYNOS_MAIN_PROFILER_H__
#define __EXYNOS_MAIN_PROFILER_H__

#include "../../../../../../kernel/sched/ems/ems.h"
#include "../../../../../../kernel/sched/sched.h"

#include <exynos-profiler-if.h>

#include <v1/exynos-profiler-shared.h>

/******************************************************************************/
/*                            Macro for profiler                             */
/******************************************************************************/
#define for_each_mdomain(dom, id)	\
	for (id = -1; (dom) = next_domain(&id), id < NUM_OF_DOMAIN;)

/******************************************************************************/
/*                            Structure for profiler                             */
/******************************************************************************/

/* Structure for IP's Private Data */
struct private_data_cpu {
	struct private_fn_cpu *fn;

	s32	pm_qos_cpu;
	struct freq_qos_request pm_qos_min_req;
	s32	pm_qos_min_freq;

	s32	local_min_freq[EPOLL_MAX_PID];

	s32	pid_util_max;
	s32	pid_util_min;

	s32	num_of_cpu;
};

struct private_data_gpu {
	struct private_fn_gpu *fn;

	struct exynos_pm_qos_request pm_qos_min_req;
	s32	pm_qos_min_class;
	s32	pm_qos_min_freq;

	s32	local_min_freq[EPOLL_MAX_PID];
};

struct private_data_mif {
	struct private_fn_mif *fn;

	struct exynos_pm_qos_request pm_qos_min_req;
	s32	pm_qos_min_class;
	s32	pm_qos_min_freq;

	s32	local_min_freq[EPOLL_MAX_PID];

	s32	pid_util_max;
	s32	pid_util_min;
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
	int				forced_running;
	int				disable_gfx;
	int				profiler_gov;
	int				gpu_profiler_gov;

	int				bts_idx;
	int				bts_added;
	int				len_mo_id;
	int				*mo_id;

	int				llc_config;

	bool	dm_dynamic;
	bool	disable_llc_way;

	struct domain_data		domains[NUM_OF_DOMAIN];
	const char	*domain_name[NUM_OF_DOMAIN];

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

	struct device_node		*dn;
	struct kobject			*kobj;

	struct device_file_operation 	gov_fops;
	wait_queue_head_t		wq;

	int				profiler_state;
	atomic_t			profiler_running;
	struct work_struct		profiler_wq;
	int				profiler_stay_counter;

	int				perf_mainbw_thr;
	int				perf_gpubw_up_ratio;
	int				perf_gpubw_dn_ratio;
	int				perf_max_llc_way;
	int				perf_light_llc_way;
} profiler;

struct fps_profile {
	/* store last profile average fps */
	int		start;
	ktime_t		profile_time;	/* sec */
	u64		fps;
} fps_profile;

struct profiler_fops_priv {
	int pid;
	int id;
	int poll_mask;
};

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
