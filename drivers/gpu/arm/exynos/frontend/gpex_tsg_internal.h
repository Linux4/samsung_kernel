/* SPDX-License-Identifier: GPL-2.0 */

/*
 * (C) COPYRIGHT 2021 Samsung Electronics Inc. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 */

#ifndef _GPEX_TSG_INTERNAL_H_
#define _GPEX_TSG_INTERNAL_H_
#include <linux/ktime.h>

struct _tsg_info {
	struct kbase_device *kbdev;
	struct {
		int util_history[2][WINDOW];
		int freq_history[WINDOW];
		int average_util;
		int average_freq;
		int diff_util;
		int diff_freq;
		int weight_util[2];
		int weight_freq;
		int next_util;
		int next_freq;
		int pre_util;
		int pre_freq;
		bool en_signal;
	} prediction;

	struct {
		/* job queued time, index represents queued_time each threshold */
		uint32_t queued_threshold[2];
		ktime_t queued_time_tick[2];
		ktime_t queued_time[2];
		ktime_t last_updated;
		atomic_t nr[3]; /* Legacy: job_nr, in_nr, out_nr */
	} queue;

	uint64_t input_job_nr_acc;
	int input_job_nr;

	int freq_margin;
	int migov_mode;
	int weight_table_idx_0;
	int weight_table_idx_1;
	int migov_saved_polling_speed;
	bool is_pm_qos_tsg;
	int amigo_flags;

	int governor_type_init;
	int is_gov_set;

	/* GPU Profiler */
	int nr_q;
	int egphead;
	int egptail;
	int lastshowidx;
	u64 prev_swap_timestamp;
	u64 first_job_timestamp;
	u64 gpu_timestamps[3];
	u32 js_occupy;
	u64 lastjob_starttimestamp;
	u64 sum_jobs_time;
	u64 last_jobs_time;
	struct egp_profile {
		u32 nr_q;
		u64 cpu_timestamps[3];
		u64 gpu_timestamps[3];
		u64 pre_time;
		u64 cpurun_time;
		u64 gpurun_time;
		u64 sum_pre_time;
		u64 sum_cpurun_time;
		u64 sum_vsync2swap_time;
		u64 sum_gpurun_time;
		u64 sum_vsync2fence_time;
		u64 max_pre_time;
		u64 max_cpurun_time;
		u64 max_vsync2swap_time;
		u64 max_gpurun_time;
		u64 max_vsync2fence_time;
	} egp[EGP_QUEUE_SIZE];
	u64 sum_pre_time;
	u64 sum_cpurun_time;
	u64 sum_vsync2swap_time;
	u64 sum_gpurun_time;
	u64 sum_vsync2fence_time;
	u64 max_pre_time;
	u64 max_cpurun_time;
	u64 max_vsync2swap_time;
	u64 max_gpurun_time;
	u64 max_vsync2fence_time;

	int vqtail;
	int vsync_interval;
	u64 vsyncq[VSYNC_HIST_SIZE];
	/* End for GPU Profiler */

	raw_spinlock_t spinlock;
	struct atomic_notifier_head frag_utils_change_notifier_list;
};

struct amigo_interframe_data {
	unsigned int nrq;
	ktime_t sw_vsync;
	ktime_t sw_start;
	ktime_t sw_end;
	ktime_t sw_total;
	ktime_t hw_vsync;
	ktime_t hw_start;
	ktime_t hw_end;
	ktime_t hw_total;
	ktime_t sum_pre;
	ktime_t sum_cpu;
	ktime_t sum_v2s;
	ktime_t sum_gpu;
	ktime_t sum_v2f;
	ktime_t max_pre;
	ktime_t max_cpu;
	ktime_t max_v2s;
	ktime_t max_gpu;
	ktime_t max_v2f;
	ktime_t vsync_interval;
	int sdp_next_cpuid;
	int sdp_cur_fcpu;
	int sdp_cur_fgpu;
	int sdp_next_fcpu;
	int sdp_next_fgpu;
	ktime_t cputime, gputime;
};

struct amigo_freq_estpower {
	int freq;
	int power;
};

int gpex_tsg_external_init(struct _tsg_info *_tsg_info);
int gpex_tsg_sysfs_init(struct _tsg_info *_tsg_info);

int gpex_tsg_external_term(void);
int gpex_tsg_sysfs_term(void);

void gpex_tsg_inc_rtp_vsync_swapcall_counter(void);

#endif
