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

#include <linux/notifier.h>
#include <linux/ktime.h>

#include <gpex_tsg.h>
#include <gpex_dvfs.h>
#include <gpex_utils.h>
#include <gpex_pm.h>
#include <gpex_ifpo.h>
#include <gpex_clock.h>
#include <gpexbe_pm.h>

#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/exynos-profiler.h>

#include "gpex_tsg_internal.h"

#define DVFS_TABLE_ROW_MAX 20
#define TABLE_MAX                      (32)
#define RENDERINGTIME_MAX_TIME         (999999)

static struct _tsg_info *tsg_info;
static struct amigo_interframe_data interframe[TABLE_MAX];

//static unsigned int rtp_head;
static unsigned int rtp_tail;
static unsigned int rtp_nrq;
//static unsigned int rtp_lastshowidx;
//static ktime_t rtp_prev_swaptimestamp;
//static ktime_t rtp_lasthw_starttime;
//static ktime_t rtp_lasthw_endtime;
//static atomic_t rtp_lasthw_totaltime;
//static atomic_t rtp_lasthw_read;
//static ktime_t rtp_curhw_starttime;
//static ktime_t rtp_curhw_endtime;
//static ktime_t rtp_curhw_totaltime;
static ktime_t rtp_sum_pre;
static ktime_t rtp_sum_cpu;
static ktime_t rtp_sum_v2s;
static ktime_t rtp_sum_gpu;
static ktime_t rtp_sum_v2f;
static ktime_t rtp_max_pre;
static ktime_t rtp_max_cpu;
static ktime_t rtp_max_v2s;
static ktime_t rtp_max_gpu;
static ktime_t rtp_max_v2f;
//static int rtp_last_cputime;
//static int rtp_last_gputime;

static ktime_t rtp_vsync_lastsw;
static ktime_t rtp_vsync_curhw;
//static ktime_t rtp_vsync_lasthw;
static ktime_t rtp_vsync_prev;
static ktime_t rtp_vsync_interval;
static int rtp_vsync_swapcall_counter;
static int rtp_vsync_frame_counter;
static int rtp_vsync_counter;
static unsigned int rtp_target_frametime;

static int migov_vsync_frame_counter;
static unsigned int migov_vsync_counter;
static ktime_t migov_last_updated_vsync_time;

static unsigned int rtp_targettime_margin;
static unsigned int rtp_workload_margin;
//static unsigned int rtp_decon_time;
static int rtp_shortterm_comb_ctrl;
//static int rtp_shortterm_comb_ctrl_manual;

static int rtp_powertable_size[NUM_OF_DOMAIN];
static struct amigo_freq_estpower *rtp_powertable[NUM_OF_DOMAIN];
//static int rtp_busy_cpuid_prev;
//static int rtp_busy_cpuid;
static int rtp_busy_cpuid_next;
static int rtp_cur_freqlv[NUM_OF_DOMAIN];
//static int rtp_max_minlock_freq;
//static int rtp_prev_minlock_cpu_freq;
//static int rtp_prev_minlock_gpu_freq;
static int rtp_last_minlock_cpu_maxfreq;
static int rtp_last_minlock_gpu_maxfreq;

unsigned long exynos_stats_get_job_state_cnt(void)
{
	return tsg_info->input_job_nr_acc;
}
EXPORT_SYMBOL(exynos_stats_get_job_state_cnt);

int exynos_stats_get_gpu_cur_idx(void)
{
	int i;
	int level = -1;
	int clock = 0;

	if (gpexbe_pm_get_exynos_pm_domain()) {
		if (gpexbe_pm_get_status() == 1) {
			clock = gpex_clock_get_cur_clock();
		} else {
			GPU_LOG(MALI_EXYNOS_ERROR, "%s: can't get dvfs cur clock\n", __func__);
			clock = 0;
		}
	} else {
		if (gpex_pm_get_status(true) == 1) {
			if (gpex_ifpo_get_mode() && !gpex_ifpo_get_status()) {
				GPU_LOG(MALI_EXYNOS_ERROR, "%s: can't get dvfs cur clock\n",
					__func__);
				clock = 0;
			} else {
				clock = gpex_clock_get_cur_clock();
			}
		}
	}

	if (clock == 0)
		return (gpex_clock_get_table_idx(gpex_clock_get_min_clock()) -
			gpex_clock_get_table_idx(gpex_clock_get_max_clock()));

	for (i = gpex_clock_get_table_idx(gpex_clock_get_max_clock());
	     i <= gpex_clock_get_table_idx(gpex_clock_get_min_clock()); i++) {
		if (gpex_clock_get_clock(i) == clock) {
			level = i;
			break;
		}
	}

	return (level - gpex_clock_get_table_idx(gpex_clock_get_max_clock()));
}
EXPORT_SYMBOL(exynos_stats_get_gpu_cur_idx);

int exynos_stats_get_gpu_coeff(void)
{
	int coef = 6144;

	return coef;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_coeff);

uint32_t exynos_stats_get_gpu_table_size(void)
{
	return (gpex_clock_get_table_idx(gpex_clock_get_min_clock()) -
		gpex_clock_get_table_idx(gpex_clock_get_max_clock()) + 1);
}
EXPORT_SYMBOL(exynos_stats_get_gpu_table_size);

static uint32_t freqs[DVFS_TABLE_ROW_MAX];
uint32_t *gpu_dvfs_get_freq_table(void)
{
	int i;

	for (i = gpex_clock_get_table_idx(gpex_clock_get_max_clock());
	     i <= gpex_clock_get_table_idx(gpex_clock_get_min_clock()); i++) {
		freqs[i - gpex_clock_get_table_idx(gpex_clock_get_max_clock())] =
			(uint32_t)gpex_clock_get_clock(i);
	}

	return freqs;
}
EXPORT_SYMBOL(gpu_dvfs_get_freq_table);

static uint32_t volts[DVFS_TABLE_ROW_MAX];
uint32_t *exynos_stats_get_gpu_volt_table(void)
{
	int i;
	for (i = gpex_clock_get_table_idx(gpex_clock_get_max_clock());
	     i <= gpex_clock_get_table_idx(gpex_clock_get_min_clock()); i++) {
		volts[i - gpex_clock_get_table_idx(gpex_clock_get_max_clock())] =
			(uint32_t)gpex_clock_get_voltage(gpex_clock_get_clock(i));
	}

	return volts;
}
EXPORT_SYMBOL(exynos_stats_get_gpu_volt_table);

static ktime_t time_in_state[DVFS_TABLE_ROW_MAX];
ktime_t tis_last_update;
ktime_t *gpu_dvfs_get_time_in_state(void)
{
	int i;

	for (i = gpex_clock_get_table_idx(gpex_clock_get_max_clock());
	     i <= gpex_clock_get_table_idx(gpex_clock_get_min_clock()); i++) {
		time_in_state[i - gpex_clock_get_table_idx(gpex_clock_get_max_clock())] =
			ms_to_ktime((u64)(gpex_clock_get_time_busy(i) * 4) / 100);
	}

	return time_in_state;
}
EXPORT_SYMBOL(gpu_dvfs_get_time_in_state);

ktime_t gpu_dvfs_get_tis_last_update(void)
{
	return (ktime_t)(gpex_clock_get_time_in_state_last_update());
}
EXPORT_SYMBOL(gpu_dvfs_get_tis_last_update);

int gpu_dvfs_get_max_freq(void)
{
	unsigned long flags;
	int locked_clock = -1;

	gpex_dvfs_spin_lock(&flags);
	locked_clock = gpex_clock_get_max_lock();
	if (locked_clock <= 0)
		locked_clock = gpex_clock_get_max_clock();
	gpex_dvfs_spin_unlock(&flags);

	return locked_clock;
}
EXPORT_SYMBOL(gpu_dvfs_get_max_freq);

int gpu_dvfs_get_min_freq(void)
{
	unsigned long flags;
	int locked_clock = -1;

	gpex_dvfs_spin_lock(&flags);
	locked_clock = gpex_clock_get_min_lock();
	if (locked_clock <= 0)
		locked_clock = gpex_clock_get_min_clock();
	gpex_dvfs_spin_unlock(&flags);

	return locked_clock;
}
EXPORT_SYMBOL(gpu_dvfs_get_min_freq);

int exynos_stats_set_queued_threshold_0(uint32_t threshold)
{
	gpex_tsg_set_queued_threshold(0, threshold);
	return gpex_tsg_get_queued_threshold(0);
}
EXPORT_SYMBOL(exynos_stats_set_queued_threshold_0);

int exynos_stats_set_queued_threshold_1(uint32_t threshold)
{
	gpex_tsg_set_queued_threshold(1, threshold);

	return gpex_tsg_get_queued_threshold(1);
}
EXPORT_SYMBOL(exynos_stats_set_queued_threshold_1);

ktime_t *gpu_dvfs_get_job_queue_count(void)
{
	int i;
	for (i = 0; i < 2; i++) {
		gpex_tsg_set_queued_time(i, gpex_tsg_get_queued_time_tick(i));
	}
	return gpex_tsg_get_queued_time_array();
}
EXPORT_SYMBOL(gpu_dvfs_get_job_queue_count);

ktime_t gpu_dvfs_get_job_queue_last_updated(void)
{
	return gpex_tsg_get_queued_last_updated();
}
EXPORT_SYMBOL(gpu_dvfs_get_job_queue_last_updated);

void exynos_stats_set_gpu_polling_speed(int polling_speed)
{
	gpex_dvfs_set_polling_speed(polling_speed);
}
EXPORT_SYMBOL(exynos_stats_set_gpu_polling_speed);

int exynos_stats_get_gpu_polling_speed(void)
{
	return gpex_dvfs_get_polling_speed();
}
EXPORT_SYMBOL(exynos_stats_get_gpu_polling_speed);

void gpu_dvfs_set_amigo_governor(int mode)
{
	gpex_tsg_set_migov_mode(mode);
}
EXPORT_SYMBOL(gpu_dvfs_set_amigo_governor);

void gpu_dvfs_set_freq_margin(int margin)
{
	gpex_tsg_set_freq_margin(margin);
}
EXPORT_SYMBOL(gpu_dvfs_set_freq_margin);

void exynos_stats_get_run_times(u64 *times)
{
	struct amigo_interframe_data last_frame;
	int cur_nrq = rtp_nrq;

	rtp_nrq = 0;
	rtp_sum_pre = 0;
	rtp_sum_cpu = 0;
	rtp_sum_v2s = 0;
	rtp_sum_gpu = 0;
	rtp_sum_v2f = 0;

	rtp_max_pre = 0;
	rtp_max_cpu = 0;
	rtp_max_v2s = 0;
	rtp_max_gpu = 0;
	rtp_max_v2f = 0;

	if (rtp_tail == 0)
		memcpy(&last_frame, &interframe[TABLE_MAX - 1],
				sizeof(struct amigo_interframe_data));
	else
		memcpy(&last_frame, &interframe[rtp_tail - 1],
				sizeof(struct amigo_interframe_data));
	if (cur_nrq > 0 && last_frame.nrq > 0) {
		ktime_t tmp;

		tmp = last_frame.sum_pre / last_frame.nrq;
		times[PREAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;
		tmp = last_frame.sum_cpu / last_frame.nrq;
		times[CPUAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;
		tmp = last_frame.sum_v2s / last_frame.nrq;
		times[V2SAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;
		tmp = last_frame.sum_gpu / last_frame.nrq;
		times[GPUAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;
		tmp = last_frame.sum_v2f / last_frame.nrq;
		times[V2FAVG] = (tmp > RENDERINGTIME_MAX_TIME) ? RENDERINGTIME_MAX_TIME : tmp;

		times[PREMAX] = (last_frame.max_pre > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_pre;
		times[CPUMAX] = (last_frame.max_cpu > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_cpu;
		times[V2SMAX] = (last_frame.max_v2s > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_v2s;
		times[GPUMAX] = (last_frame.max_gpu > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_gpu;
		times[V2FMAX] = (last_frame.max_v2f > RENDERINGTIME_MAX_TIME) ?
			RENDERINGTIME_MAX_TIME : last_frame.max_v2f;

		times[NRSWAP] = last_frame.nrq;
		times[NRFRAME] = rtp_vsync_frame_counter;
		times[NRFRAMEDROP] = rtp_vsync_counter - rtp_vsync_frame_counter;
		rtp_vsync_frame_counter = 0;
		rtp_vsync_counter = 0;

		times[TARGETFRAMETIME] = rtp_target_frametime;
		times[STC_CONFIG] =	(u64)(rtp_targettime_margin & 0xffff) |
							((u64)(rtp_workload_margin & 0xff) << 16) |
							((u64)(rtp_shortterm_comb_ctrl & 0xff) << 24);
	} else {
		int i;

		for (i = 0; i < NUM_OF_TIMEINFO; i++)
			times[i] = 0;
	}

	rtp_last_minlock_cpu_maxfreq = 0;
	rtp_last_minlock_gpu_maxfreq = 0;
}
EXPORT_SYMBOL(exynos_stats_get_run_times);

void exynos_stats_set_vsync(ktime_t ktime_us)
{
	rtp_vsync_interval = ktime_us - rtp_vsync_prev;
	rtp_vsync_prev = ktime_us;

	if (rtp_vsync_lastsw == 0)
		rtp_vsync_lastsw = ktime_us;
	if (rtp_vsync_curhw == 0)
		rtp_vsync_curhw = ktime_us;

	if (rtp_vsync_swapcall_counter > 0) {
		rtp_vsync_swapcall_counter = 0;
		rtp_vsync_frame_counter++;
		migov_vsync_frame_counter++;
	}
	rtp_vsync_counter++;
	migov_vsync_counter++;
}
EXPORT_SYMBOL(exynos_stats_set_vsync);

void exynos_stats_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	*nrframe = (s32)migov_vsync_frame_counter;
	*nrvsync = (u64)migov_vsync_counter;
	*delta_ms = (u64)(rtp_vsync_prev - migov_last_updated_vsync_time) / 1000;
	migov_vsync_frame_counter = 0;
	migov_vsync_counter = 0;
	migov_last_updated_vsync_time = rtp_vsync_prev;
}
EXPORT_SYMBOL(exynos_stats_get_frame_info);

void gpex_tsg_inc_rtp_vsync_swapcall_counter(void)
{
	rtp_vsync_swapcall_counter++;
}
EXPORT_SYMBOL(gpex_tsg_inc_rtp_vsync_swapcall_counter);

void exynos_migov_set_targetframetime(int us)
{
	CSTD_UNUSED(us);
}
EXPORT_SYMBOL(exynos_migov_set_targetframetime);

void exynos_migov_set_targettime_margin(int us)
{
	CSTD_UNUSED(us);
}
EXPORT_SYMBOL(exynos_migov_set_targettime_margin);

void exynos_migov_set_util_margin(int percentage)
{
	CSTD_UNUSED(percentage);
}
EXPORT_SYMBOL(exynos_migov_set_util_margin);

void exynos_migov_set_decon_time(int us)
{
	CSTD_UNUSED(us);
}
EXPORT_SYMBOL(exynos_migov_set_decon_time);

void exynos_migov_set_comb_ctrl(int enable)
{
	CSTD_UNUSED(enable);
}
EXPORT_SYMBOL(exynos_migov_set_comb_ctrl);

void exynos_sdp_set_powertable(int id, int cnt, struct freq_table *table)
{
	int i;

	if (rtp_powertable[id] == NULL || rtp_powertable_size[id] < cnt) {
		if (rtp_powertable[id] != NULL)
			kfree(rtp_powertable[id]);
		rtp_powertable[id] = kcalloc(cnt, sizeof(struct amigo_freq_estpower), GFP_KERNEL);
		rtp_powertable_size[id] = cnt;
	}

	for (i = 0; rtp_powertable[id] != NULL && i < cnt; i++) {
		rtp_powertable[id][i].freq = table[i].freq;
		rtp_powertable[id][i].power = table[i].dyn_cost + table[i].st_cost;
	}
}
EXPORT_SYMBOL(exynos_sdp_set_powertable);

void exynos_sdp_set_busy_domain(int id)
{
	rtp_busy_cpuid_next = id;
}
EXPORT_SYMBOL(exynos_sdp_set_busy_domain);

void exynos_sdp_set_cur_freqlv(int id, int idx)
{
	rtp_cur_freqlv[id] = idx;
}
EXPORT_SYMBOL(exynos_sdp_set_cur_freqlv);

int exynos_gpu_stc_config_show(int page_size, char *buf)
{
	CSTD_UNUSED(page_size);
	CSTD_UNUSED(buf);
	return 0;
}
EXPORT_SYMBOL(exynos_gpu_stc_config_show);

int exynos_gpu_stc_config_store(const char *buf)
{
	CSTD_UNUSED(buf);
	return 0;
}
EXPORT_SYMBOL(exynos_gpu_stc_config_store);

int gpu_dvfs_register_utilization_notifier(struct notifier_block *nb)
{
	int ret = 0;
	ret = atomic_notifier_chain_register(gpex_tsg_get_frag_utils_change_notifier_list(), nb);
	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_register_utilization_notifier);

int gpu_dvfs_unregister_utilization_notifier(struct notifier_block *nb)
{
	int ret = 0;
	ret = atomic_notifier_chain_unregister(gpex_tsg_get_frag_utils_change_notifier_list(), nb);
	return ret;
}
EXPORT_SYMBOL(gpu_dvfs_unregister_utilization_notifier);

/* TODO: this sysfs function use external fucntion. */
/* Actually, Using external function in internal module is not ideal with the Refactoring rules */
/* So, if we can modify outer modules such as 'migov, cooling, ...' in the future, */
/* fix it following the rules*/
static ssize_t show_feedback_governor_impl(char *buf)
{
	ssize_t ret = 0;
	int i;
	uint32_t *freqs;
	uint32_t *volts;
	ktime_t *time_in_state;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "feedback governer implementation\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int exynos_stats_get_gpu_table_size(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "     +- int gpu_dvfs_get_step(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %u\n",
			exynos_stats_get_gpu_table_size());
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int exynos_stats_get_gpu_cur_idx(void)\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "     +- int gpu_dvfs_get_cur_level(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- clock=%u\n",
			gpex_clock_get_cur_clock());
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- level=%d\n",
			exynos_stats_get_gpu_cur_idx());
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int exynos_stats_get_gpu_coeff(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     +- int gpu_dvfs_get_coefficient_value(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %u\n",
			exynos_stats_get_gpu_coeff());
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int *gpu_dvfs_get_freq_table(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     +- uint32_t *gpu_dvfs_get_freqs(void)\n");
	freqs = gpu_dvfs_get_freq_table();
	for (i = 0; i < exynos_stats_get_gpu_table_size(); i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %u\n", freqs[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- unsigned int *exynos_stats_get_gpu_volt_table(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     +- unsigned int *gpu_dvfs_get_volts(void)\n");
	volts = exynos_stats_get_gpu_volt_table();
	for (i = 0; i < exynos_stats_get_gpu_table_size(); i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %u\n", volts[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- ktime_t *gpu_dvfs_get_time_in_state(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     +- ktime_t *gpu_dvfs_get_time_in_state(void)\n");
	time_in_state = gpu_dvfs_get_time_in_state();
	for (i = 0; i < exynos_stats_get_gpu_table_size(); i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %lld\n", time_in_state[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- ktime_t *gpu_dvfs_get_job_queue_count(void)\n");
	time_in_state = gpu_dvfs_get_job_queue_count();
	for (i = 0; i < 2; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %lld\n", time_in_state[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, " +- queued_threshold_check\n");
	for (i = 0; i < 2; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %lld\n",
				gpex_tsg_get_queued_threshold(i));
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" +- int exynos_stats_get_gpu_polling_speed(void)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "         +- %d\n",
			exynos_stats_get_gpu_polling_speed());

	return ret;
}
CREATE_SYSFS_DEVICE_READ_FUNCTION(show_feedback_governor_impl);

int gpex_tsg_external_init(struct _tsg_info *_tsg_info)
{
	tsg_info = _tsg_info;
	GPEX_UTILS_SYSFS_DEVICE_FILE_ADD_RO(feedback_governor_impl, show_feedback_governor_impl);
	return 0;
}

int gpex_tsg_external_term(void)
{
	tsg_info = (void *)0;

	return 0;
}
