// SPDX-License-Identifier: GPL-2.0
/*
 * @file sgpu_profiler_v2.c
 * @copyright 2023 Samsung Electronics
 */
#include "sgpu_profiler.h"

struct amdgpu_device *profiler_adev;

static int profiler_dvfs_interval;
static struct exynos_pm_qos_request sgpu_profiler_gpu_min_qos;
static struct freq_qos_request sgpu_profiler_cpu_min_qos[NUM_OF_CPU_DOMAIN];
static struct profiler_interframe_data interframe[PROFILER_TABLE_MAX];
static struct profiler_rtp_data rtp;
static struct profiler_pid_data pid;
static struct profiler_info_data pinfo;
static u32 profiler_fps_calc_average_us;
static int profiler_margin[NUM_OF_DOMAIN];
static int profiler_margin_applied[NUM_OF_DOMAIN];
static struct mutex profiler_pmqos_lock;

/* Start current DVFS level, find next DVFS lv met feasible region */
int profiler_get_freq_margin(void)
{
	struct devfreq *df = profiler_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	return data->freq_margin;
}

int profiler_set_freq_margin(int id, int freq_margin)
{
	if (freq_margin > 1000 || freq_margin < -1000) {
		dev_err(profiler_adev->dev, "freq_margin range : (-1000 ~ 1000)\n");
		return -EINVAL;
	}
	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return -EINVAL;
	}

	if (profiler_margin[id] != freq_margin) {
		profiler_margin[id] = freq_margin;

		if (id < NUM_OF_CPU_DOMAIN) {
			profiler_margin_applied[id] = freq_margin;
			emstune_set_pboost(3, 0, PROFILER_GET_CPU_BASE_ID(id), freq_margin / 10);
		}
	}

	return 0;
}

static int profiler_get_current_cpuid(void)
{
	unsigned int core;
	int id;

	core = get_cpu();
	id = PROFILER_GET_CL_ID(core);
	put_cpu();

	return id;
}

static void profiler_init_pmqos(void)
{
	int id;
	struct cpufreq_policy *policy;

	if (pinfo.cpufreq_pm_qos_added == 0) {
		pinfo.cpufreq_pm_qos_added = 1;

		for (id = 0; id < NUM_OF_CPU_DOMAIN ; id++) {
			policy = cpufreq_cpu_get(PROFILER_GET_CPU_BASE_ID(id));
			if (policy != NULL)
				freq_qos_tracer_add_request(&policy->constraints,
						&sgpu_profiler_cpu_min_qos[id],
						FREQ_QOS_MIN, FREQ_QOS_MIN_DEFAULT_VALUE);
		}
	}

	memset(&rtp.pmqos_minlock_prev[0], 0, sizeof(rtp.pmqos_minlock_prev[0]) * NUM_OF_DOMAIN);
	memset(&rtp.pmqos_minlock_next[0], 0, sizeof(rtp.pmqos_minlock_next[0]) * NUM_OF_DOMAIN);
}

static void profiler_set_pmqos_next(void)
{
	int id;

	if (pinfo.cpufreq_pm_qos_added > 0) {
		for (id = 0; id < NUM_OF_CPU_DOMAIN; id++) {
			if (rtp.pmqos_minlock_prev[id] != rtp.pmqos_minlock_next[id]) {
				if (freq_qos_request_active(&sgpu_profiler_cpu_min_qos[id]))
					freq_qos_update_request(&sgpu_profiler_cpu_min_qos[id], rtp.pmqos_minlock_next[id]);

				rtp.pmqos_minlock_prev[id] = rtp.pmqos_minlock_next[id];
			}
		}
	}

	id = PROFILER_GPU;
	if (rtp.pmqos_minlock_prev[id] != rtp.pmqos_minlock_next[id]) {
		/* If pbooster runs in GPU DVFS Governor, pmqos request will produce deadlock
		exynos_pm_qos_update_request(&sgpu_profiler_gpu_min_qos, rtp.pmqos_minlock_next[id]);*/
		rtp.pmqos_minlock_prev[id] = rtp.pmqos_minlock_next[id];
	}
}

static void profiler_reset_next_minlock(void)
{
	memset(&rtp.pmqos_minlock_next[0], 0, sizeof(rtp.pmqos_minlock_next[0]) * NUM_OF_DOMAIN);
}

void profiler_set_profiler_governor(int mode)
{
	char *profiler_governor = "profiler";
	static char saved_governor[DEVFREQ_NAME_LEN + 1] = {0};
	static int saved_polling_interval;
	char *cur_gpu_governor;
	int ret = 0;

	if ((mode == 1) && (saved_polling_interval == 0)) {
		cur_gpu_governor = gpu_dvfs_get_governor();
		/* change gpu governor to profiler if not set */
		if (strncmp(cur_gpu_governor, profiler_governor, strlen(profiler_governor))) {
			strncpy(saved_governor, cur_gpu_governor, DEVFREQ_NAME_LEN);
			ret = gpu_dvfs_set_governor(profiler_governor);
			if (!ret) {
				saved_polling_interval = gpu_dvfs_get_polling_interval();
				gpu_dvfs_set_polling_interval(profiler_dvfs_interval);
				gpu_dvfs_set_autosuspend_delay(1000);
				profiler_init_pmqos();
			} else {
				SGPU_LOG(profiler_adev, DMSG_WARNING, DMSG_ETC,
					"cannot change GPU governor for PROFILER, err:%d", ret);
			}
		}
	} else if ((mode == 0) && (saved_polling_interval > 0) && saved_governor[0] != 0) {
		cur_gpu_governor = gpu_dvfs_get_governor();
		/* change gpu governor to previous value if profiler */
		if (!strncmp(cur_gpu_governor, profiler_governor, strlen(profiler_governor))) {
			ret = gpu_dvfs_set_governor(saved_governor);
			if (!ret) {
				gpu_dvfs_set_polling_interval(saved_polling_interval);
				saved_polling_interval = 0;
				gpu_dvfs_set_autosuspend_delay(50);
				mutex_lock(&profiler_pmqos_lock);
				profiler_reset_next_minlock();
				profiler_set_pmqos_next();
				mutex_unlock(&profiler_pmqos_lock);
				memset(saved_governor, 0, DEVFREQ_NAME_LEN + 1);
			} else {
				SGPU_LOG(profiler_adev, DMSG_WARNING, DMSG_ETC,
					"cannot change GPU governor back to %s, err:%d", saved_governor, ret);
			}
		}
	} else {
		SGPU_LOG(profiler_adev, DMSG_WARNING, DMSG_ETC,
				"cannot change GPU governor from:%s, mode=%d, saved_polling_interval=%d",
				gpu_dvfs_get_governor(), mode, saved_polling_interval);
	}
}

void profiler_set_targetframetime(int us)
{
	if ((us < RENDERTIME_MAX && us > RENDERTIME_MIN) || us == 0) {
		rtp.target_frametime = (u32)us;
		if (us > 0)
			rtp.target_fps = 10000000 / (u32)us;
		else
			rtp.target_fps = 0;
	}
}

int profiler_get_pb_params(int idx)
{
	int ret = -1;
	switch(idx) {
		case PB_CONTROL_USER_TARGET_PID:
			ret = rtp.user_target_pid;
			break;
		case PB_CONTROL_GPU_FORCED_BOOST_ACTIVE:
			ret = rtp.gpu_forced_boost_activepct;
			break;
		case PB_CONTROL_CPUTIME_BOOST_PM:
			ret = rtp.cputime_boost_pm;
			break;
		case PB_CONTROL_GPUTIME_BOOST_PM:
			ret = rtp.gputime_boost_pm;
			break;
		case PB_CONTROL_CPU_MGN_MAX:
			ret = rtp.pb_cpu_mgn_margin_max;
			break;
		case PB_CONTROL_CPU_MGN_FRAMEDROP_PM:
			ret = rtp.framedrop_detection_pm_mgn;
			break;
		case PB_CONTROL_CPU_MGN_FPSDROP_PM:
			ret = rtp.fpsdrop_detection_pm_mgn;
			break;
		case PB_CONTROL_CPU_MGN_ALL_FPSDROP_PM:
			ret = rtp.target_clusters_bitmap_fpsdrop_detection_pm;
			break;
		case PB_CONTROL_CPU_MGN_ALL_CL_BITMAP:
			ret = (int)rtp.target_clusters_bitmap;
			break;
		case PB_CONTROL_CPU_MINLOCK_BOOST_PM_MAX:
			ret = rtp.pb_cpu_minlock_margin_max;
			break;
		case PB_CONTROL_CPU_MINLOCK_FRAMEDROP_PM:
			ret = rtp.framedrop_detection_pm_minlock;
			break;
		case PB_CONTROL_CPU_MINLOCK_FPSDROP_PM:
			ret = rtp.fpsdrop_detection_pm_minlock;
			break;
		case PB_CONTROL_DVFS_INTERVAL:
			ret = profiler_dvfs_interval;
			break;
		case PB_CONTROL_FPS_AVERAGE_US:
			ret = profiler_fps_calc_average_us;
			break;
		default:
			break;
	}

	return ret;
}

void profiler_set_pb_params(int idx, int value)
{
	switch(idx) {
		case PB_CONTROL_USER_TARGET_PID:
			rtp.user_target_pid = value;
			break;
		case PB_CONTROL_GPU_FORCED_BOOST_ACTIVE:
			rtp.gpu_forced_boost_activepct = value;
			break;
		case PB_CONTROL_CPUTIME_BOOST_PM:
			rtp.cputime_boost_pm = value;
			break;
		case PB_CONTROL_GPUTIME_BOOST_PM:
			rtp.gputime_boost_pm = value;
			break;
		case PB_CONTROL_CPU_MGN_MAX:
			rtp.pb_cpu_mgn_margin_max = value;
			break;
		case PB_CONTROL_CPU_MGN_FRAMEDROP_PM:
			rtp.framedrop_detection_pm_mgn = value;
			break;
		case PB_CONTROL_CPU_MGN_FPSDROP_PM:
			rtp.fpsdrop_detection_pm_mgn = value;
			break;
		case PB_CONTROL_CPU_MGN_ALL_FPSDROP_PM:
			rtp.target_clusters_bitmap_fpsdrop_detection_pm = value;
			break;
		case PB_CONTROL_CPU_MGN_ALL_CL_BITMAP:
			rtp.target_clusters_bitmap = (u32)value;
			break;
		case PB_CONTROL_CPU_MINLOCK_BOOST_PM_MAX:
			rtp.pb_cpu_minlock_margin_max = value;
			break;
		case PB_CONTROL_CPU_MINLOCK_FRAMEDROP_PM:
			rtp.framedrop_detection_pm_minlock = value;
			break;
		case PB_CONTROL_CPU_MINLOCK_FPSDROP_PM:
			rtp.fpsdrop_detection_pm_minlock = value;
			break;
		case PB_CONTROL_DVFS_INTERVAL:
			if (value >= 4 && value <= 32)
				profiler_dvfs_interval = value;
			break;
		case PB_CONTROL_FPS_AVERAGE_US:
			if (value >= 32000 && value <= 1000000)
				profiler_fps_calc_average_us = value;
			break;
		default:
			break;
	}
}

void profiler_set_vsync(ktime_t time_us)
{
	rtp.vsync_interval = time_us - rtp.vsync_prev;
	rtp.vsync_prev = time_us;
	if (rtp.vsync_lastsw == 0)
		rtp.vsync_lastsw = time_us;
	if (rtp.vsync_curhw == 0)
		rtp.vsync_curhw = time_us;
	if (atomic_read(&rtp.vsync_swapcall_counter) > 0)
		rtp.vsync_frame_counter++;
	atomic_set(&rtp.vsync_swapcall_counter, 0);
	rtp.vsync_counter++;
	pinfo.vsync_counter++;
}

void profiler_get_frame_info(s32 *nrframe, u64 *nrvsync, u64 *delta_ms)
{
	*nrframe = pinfo.frame_counter;
	pinfo.frame_counter = 0;
	*nrvsync = pinfo.vsync_counter;
	pinfo.vsync_counter = 0;
	*delta_ms = (u64)(rtp.vsync_prev - (ktime_t)(pinfo.last_updated_vsync_time)) / 1000;
	pinfo.last_updated_vsync_time = (u64)rtp.vsync_prev;
}

void profiler_get_run_times(u64 *times)
{
	struct profiler_interframe_data last_frame;
	u64 last_frame_nrq = 0;

	/* Transaction to transfer Rendering Time to profiler */
	if (rtp.readout != 0) {
		memset(&last_frame, 0, sizeof(struct profiler_interframe_data));
	} else {
		int tail = rtp.tail;
		int index = (tail == 0) ? PROFILER_TABLE_MAX - 1 : tail - 1;
		index = min(index, PROFILER_TABLE_MAX - 1);

		memcpy(&last_frame, &interframe[index], sizeof(struct profiler_interframe_data));
	}
	rtp.readout = 1;

	last_frame_nrq = last_frame.nrq;
	if (last_frame_nrq > 0) {
		u64 tmp;

		tmp = last_frame.sum_pre / last_frame_nrq;
		times[PREAVG] = min(tmp, RENDERTIME_MAX);
		tmp = last_frame.sum_cpu / last_frame_nrq;
		times[CPUAVG] = min(tmp, RENDERTIME_MAX);
		tmp = last_frame.sum_swap / last_frame_nrq;
		times[V2SAVG] = min(tmp, RENDERTIME_MAX);
		tmp = last_frame.sum_gpu / last_frame_nrq;
		times[GPUAVG] = min(tmp, RENDERTIME_MAX);
		tmp = last_frame.sum_v2f / last_frame_nrq;
		times[V2FAVG] = min(tmp, RENDERTIME_MAX);

		times[PREMAX] = min(last_frame.max_pre, RENDERTIME_MAX);
		times[CPUMAX] = min(last_frame.max_cpu, RENDERTIME_MAX);
		times[V2SMAX] = min(last_frame.max_swap, RENDERTIME_MAX);
		times[GPUMAX] = min(last_frame.max_gpu, RENDERTIME_MAX);
		times[V2FMAX] = min(last_frame.max_v2f, RENDERTIME_MAX);

		rtp.vsync_frame_counter = 0;
		rtp.vsync_counter = 0;
	} else {
		memset(&times[0], 0, sizeof(times[0]) * NUM_OF_TIMEINFO);
	}
}

static void sgpu_profiler_pid_init(void)
{
	atomic_set(&pid.list_readout, 0);
	atomic_set(&pid.list_top, 0);
	memset(&(pid.list[0]), 0, sizeof(pid.list[0]) * NUM_OF_PID);
	memset(&(pid.core_list[0]), 0, sizeof(pid.core_list[0]) * NUM_OF_PID);
}

void profiler_pid_get_list(u32 *list, u8 *core_list)
{
	int prev_top;

	if (list == NULL) {
		dev_err(profiler_adev->dev, "Null Pointer for list");
		return;
	}
	if (core_list == NULL) {
		dev_err(profiler_adev->dev, "Null Pointer for core_list");
		return;
	}

	if (atomic_read(&pid.list_readout) != 0) {
		memset(&list[0], 0, sizeof(list[0]) * NUM_OF_PID);
		memset(&core_list[0], 0, sizeof(core_list[0]) * NUM_OF_PID);
	} else {
		/* Transaction to transfer PID list to profiler */
		int limit_count = 2;
		do {
			prev_top = atomic_read(&pid.list_top);
			memcpy(list, &pid.list[0], sizeof(pid.list));
			memcpy(core_list, &pid.core_list[0], sizeof(pid.core_list));
		} while (prev_top != atomic_read(&pid.list_top) && limit_count-- > 0);
	}
	atomic_set(&pid.list_readout, 1);
}

void profiler_interframe_sw_update(ktime_t start, ktime_t end, ktime_t total)
{
	struct profiler_interframe_data *dst;
	ktime_t swaptime = end - rtp.prev_swaptimestamp;
	ktime_t v2ftime = 0;
	ktime_t pretime = (rtp.prev_swaptimestamp < start) ? start - rtp.prev_swaptimestamp : 0;
	ktime_t cputime = end - start;
	ktime_t gputime = (ktime_t)atomic_read(&rtp.lasthw_totaltime);
	int list_tail_pos = 0;
	int curidx = rtp.tail;
	int i;

	atomic_set(&rtp.lasthw_read, 1);

	rtp.prev_swaptimestamp = end;
	atomic_inc(&rtp.vsync_swapcall_counter);
	pinfo.frame_counter++;

	dst = &interframe[curidx];
	rtp.tail = (rtp.tail + 1) % PROFILER_TABLE_MAX;
	if (rtp.tail == rtp.head)
		rtp.head = (rtp.head + 1) % PROFILER_TABLE_MAX;
	if (rtp.tail == rtp.lastshowidx)
		rtp.lastshowidx = (rtp.lastshowidx + 1) % PROFILER_TABLE_MAX;

	dst->sw_vsync = (rtp.vsync_lastsw == 0) ? rtp.vsync_prev : rtp.vsync_lastsw;
	rtp.vsync_lastsw = 0;
	dst->sw_start = start;
	dst->sw_end = end;
	dst->sw_total = total;

	dst->hw_vsync = (rtp.vsync_lasthw == 0) ? rtp.vsync_prev : rtp.vsync_lasthw;
	dst->hw_start = rtp.lasthw_starttime;
	dst->hw_end = rtp.lasthw_endtime;
	dst->hw_total = gputime;
	v2ftime = (dst->hw_vsync < dst->hw_end) ?
		dst->hw_end - dst->hw_vsync : dst->hw_end - rtp.vsync_prev;

	if (rtp.nrq > 128 || rtp.readout != 0) {
		rtp.nrq = 0;
		rtp.sum_pre = 0;
		rtp.sum_cpu = 0;
		rtp.sum_swap = 0;
		rtp.sum_gpu = 0;
		rtp.sum_v2f = 0;

		rtp.max_pre = 0;
		rtp.max_cpu = 0;
		rtp.max_swap = 0;
		rtp.max_gpu = 0;
		rtp.max_v2f = 0;
	}
	rtp.readout = 0;
	rtp.nrq++;

	rtp.sum_pre += pretime;
	rtp.sum_cpu += cputime;
	rtp.sum_swap += swaptime;
	rtp.sum_gpu += gputime;
	rtp.sum_v2f += v2ftime;

	if (rtp.max_pre < pretime)
		rtp.max_pre = pretime;
	if (rtp.max_cpu < cputime)
		rtp.max_cpu = cputime;
	if (rtp.max_swap < swaptime)
		rtp.max_swap = swaptime;
	if (rtp.max_gpu < gputime)
		rtp.max_gpu = gputime;
	if (rtp.max_v2f < v2ftime)
		rtp.max_v2f = v2ftime;

	dst->nrq = rtp.nrq;
	dst->sum_pre = rtp.sum_pre;
	dst->sum_cpu = rtp.sum_cpu;
	dst->sum_swap = rtp.sum_swap;
	dst->sum_gpu = rtp.sum_gpu;
	dst->sum_v2f = rtp.sum_v2f;

	dst->max_pre = rtp.max_pre;
	dst->max_cpu = rtp.max_cpu;
	dst->max_swap = rtp.max_swap;
	dst->max_gpu = rtp.max_gpu;
	dst->max_v2f = rtp.max_v2f;
	dst->cputime = cputime;
	dst->gputime = gputime;

	dst->vsync_interval = rtp.vsync_interval;

	dst->timestamp = end;
	dst->pid = (u32)current->pid;
	dst->tgid = (u32)current->tgid;
	dst->coreid = (u32)get_cpu();
	put_cpu(); /* for enabling preemption */
	for (i = 0; i < 16; i++)
		dst->name[i] = current->comm[i];

	if (rtp.pfinfo.latest_targetpid > 0) {
		if (dst->tgid == rtp.pfinfo.latest_targetpid) {
			rtp.target_clusterid = PROFILER_GET_CL_ID(dst->coreid);
			rtp.last_cputime = cputime;
			rtp.last_swaptime = swaptime;
			rtp.last_gputime = gputime;
			rtp.last_cpufreqlv = rtp.cur_freqlv[rtp.target_clusterid];
			rtp.last_gpufreqlv = rtp.cur_freqlv[PROFILER_GPU];
		}
	} else {
		rtp.target_clusterid = PROFILER_GET_CL_ID(dst->coreid);
		rtp.last_cputime = cputime;
		rtp.last_swaptime = swaptime;
		rtp.last_gputime = gputime;
		rtp.last_cpufreqlv = rtp.cur_freqlv[rtp.target_clusterid];
		rtp.last_gpufreqlv = rtp.cur_freqlv[PROFILER_GPU];
	}

	if (atomic_read(&pid.list_readout) != 0)
		sgpu_profiler_pid_init();

	list_tail_pos = atomic_read(&pid.list_top);
	if (list_tail_pos < NUM_OF_PID) {
		/* coreid '0' means not recorded */
		pid.core_list[list_tail_pos] = dst->coreid + 1;
		/* tgid '0' => filtering of System UI */
		pid.list[list_tail_pos] = (dst->tgid == 0) ? 1 : dst->tgid;
		atomic_inc(&pid.list_top);
	}

	profiler_fps_profiler(curidx);
}

void profiler_interframe_hw_update(ktime_t start, ktime_t end, bool end_of_frame)
{
	if (end_of_frame) {
		ktime_t diff = ktime_get_real() - ktime_get();
		ktime_t term = 0;
		int time = atomic_read(&rtp.lasthw_totaltime);
		int read = atomic_read(&rtp.lasthw_read);

		atomic_set(&rtp.lasthw_read, 0);

		rtp.lasthw_starttime = (diff + rtp.curhw_starttime)/1000;
		rtp.lasthw_endtime = (diff + end)/1000;
		if (start < rtp.curhw_endtime)
			term = end - rtp.curhw_endtime;
		else
			term = end - start;
		rtp.curhw_totaltime += term;
		if (read == 1 || time < (int)(rtp.curhw_totaltime / 1000)) {
			time = (int)(rtp.curhw_totaltime / 1000);
			atomic_set(&rtp.lasthw_totaltime, time);
		}

		rtp.curhw_starttime = 0;
		rtp.curhw_endtime = end;
		rtp.curhw_totaltime = 0;

		rtp.vsync_lasthw = (rtp.vsync_curhw == 0) ? rtp.vsync_prev : rtp.vsync_curhw;
		rtp.vsync_curhw = 0;
	} else {
		ktime_t term = 0;

		if (rtp.curhw_starttime == 0)
			rtp.curhw_starttime = start;
		if (start < rtp.curhw_endtime)
			term = end - rtp.curhw_endtime;
		else
			term = end - start;
		rtp.curhw_totaltime += term;
		rtp.curhw_endtime = end;
	}
}

struct profiler_interframe_data *profiler_get_next_frameinfo(void)
{
	struct profiler_interframe_data *dst;

	if (rtp.head == rtp.tail)
		return NULL;
	dst = &interframe[rtp.head];
	rtp.head = (rtp.head + 1) % PROFILER_TABLE_MAX;
	return dst;
}

int profiler_get_target_frametime(void)
{
	return rtp.target_frametime;
}

unsigned int profiler_get_weight_table_idx_0(void)
{
	struct devfreq *df = profiler_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	return data->weight_table_idx[0];
}

int profiler_set_weight_table_idx_0(unsigned int value)
{
	struct devfreq *df = profiler_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	if (value >= WEIGHT_TABLE_MAX_SIZE) {
		dev_err(profiler_adev->dev, "weight table idx range : (0 ~%u)\n",
				WEIGHT_TABLE_MAX_SIZE);
		return -EINVAL;
	}
	data->weight_table_idx[0] = value;

	return 0;
}

unsigned int profiler_get_weight_table_idx_1(void)
{
	struct devfreq *df = profiler_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	return data->weight_table_idx[1];
}

int profiler_set_weight_table_idx_1(unsigned int value)
{
	struct devfreq *df = profiler_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	if (value >= WEIGHT_TABLE_MAX_SIZE) {
		dev_err(profiler_adev->dev, "weight table idx range : (0 ~%u)\n",
				WEIGHT_TABLE_MAX_SIZE);
		return -EINVAL;
	}
	data->weight_table_idx[1] = value;

	return 0;
}

static void (*profiler_wakeup_func)(void);
void profiler_register_wakeup_func(void (*target_func)(void))
{
	profiler_wakeup_func = target_func;
}

void profiler_wakeup(void)
{
	if (profiler_wakeup_func != NULL)
		profiler_wakeup_func();
}

static void sgpu_profiler_rtp_init(void)
{
	memset(&rtp, 0, sizeof(struct profiler_rtp_data));

	/* GPU Profiler Governor */
	mutex_init(&profiler_pmqos_lock);
	profiler_dvfs_interval = PROFILER_DVFS_INTERVAL_MS;

	/* default PB input params */
	profiler_fps_calc_average_us = PROFILER_FPS_CALC_AVERAGE_US;
	rtp.outinfolevel = 0;
	rtp.target_clusterid = profiler_get_current_cpuid();
	rtp.target_frametime = RENDERTIME_DEFAULT_FRAMETIME;
	rtp.gpu_forced_boost_activepct = 98;
	rtp.user_target_pid = 0;

	rtp.pb_cpu_mgn_margin_max = 400;
	rtp.framedrop_detection_pm_mgn = 100;
	rtp.fpsdrop_detection_pm_mgn = 50; /* 57fps/60fps, 114fps/120fps */
	rtp.target_clusters_bitmap = 0xff - 1;
	rtp.target_clusters_bitmap_fpsdrop_detection_pm = 100;

	rtp.pb_cpu_minlock_margin_max = 4000;
	rtp.framedrop_detection_pm_minlock = 200; /* expected rendertime / latest rendertime */
	rtp.fpsdrop_detection_pm_minlock = 100; /* 54fps/60fps, 108fps/120fps */
}

void sgpu_profiler_init(struct amdgpu_device *adev)
{
	profiler_wakeup_func = NULL;
	pinfo.cpufreq_pm_qos_added = 0;
	profiler_adev = adev;
	exynos_pm_qos_add_request(&sgpu_profiler_gpu_min_qos,
				  PM_QOS_GPU_THROUGHPUT_MIN, 0);
	sgpu_profiler_rtp_init();
	sgpu_profiler_pid_init();
}

void sgpu_profiler_deinit(void)
{
	exynos_pm_qos_remove_request(&sgpu_profiler_gpu_min_qos);
}

ssize_t profiler_show_status(char *buf)
{
	struct profiler_interframe_data *dst;
	ssize_t count = 0;
	int id = 0;
	int target_frametime = profiler_get_target_frametime();

	while ((dst = profiler_get_next_frameinfo()) != NULL){
		if (dst->nrq > 0) {
			ktime_t avg_pre = dst->sum_pre / dst->nrq;
			ktime_t avg_cpu = dst->sum_cpu / dst->nrq;
			ktime_t avg_swap = dst->sum_swap / dst->nrq;
			ktime_t avg_gpu = dst->sum_gpu / dst->nrq;
			ktime_t avg_v2f = dst->sum_v2f / dst->nrq;

			count += scnprintf(buf + count, PAGE_SIZE - count,"%4d, %6llu, %3u, %6llu, %6llu, %6llu, %6llu, %6llu"
				, id++
				, dst->vsync_interval
				, dst->nrq
				, avg_pre
				, avg_cpu
				, avg_swap
				, avg_gpu
				, avg_v2f);
			count += scnprintf(buf + count, PAGE_SIZE - count,",|, %6d, %6llu, %6llu"
				, target_frametime
				, dst->cputime
				, dst->gputime);
			count += scnprintf(buf + count, PAGE_SIZE - count,",|, %2d, %6d, %6d, %6d, %6d\n"
				, dst->sdp_next_cpuid
				, dst->sdp_cur_fcpu
				, dst->sdp_cur_fgpu
			        , dst->sdp_next_fcpu
				, dst->sdp_next_fgpu);

		}
	}

	return count;
}

/* Performance Booster */
void profiler_set_outinfo(int value)
{
	rtp.outinfolevel = value;
}

int profiler_pb_set_powertable(int id, int cnt, struct freq_table *table)
{
	int i;

	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return -EINVAL;
	}

	if (table == NULL) {
		dev_err(profiler_adev->dev, "Null Pointer for table");
		return -EINVAL;
	}

	if (cnt == 0) {
		dev_err(profiler_adev->dev, "invalid freq_table count");
		return -EINVAL;
	}

	if (rtp.powertable[id] != NULL) {
		dev_err(profiler_adev->dev, "Already allocated by first device for id(%d)", id);
		return -EINVAL;
	}

	rtp.powertable[id] = kzalloc(sizeof(struct profiler_freq_estpower_data) * cnt, GFP_KERNEL);
	if (!rtp.powertable[id]) {
		dev_err(profiler_adev->dev, "failed to allocate for estpower talbe id(%d)", id);
		return -ENOMEM;
	}

	rtp.powertable_size[id] = cnt;

	for (i = 0; i < cnt; i++) {
		rtp.powertable[id][i].freq = table[i].freq;
		rtp.powertable[id][i].power = table[i].dyn_cost + table[i].st_cost;
	}

	return 0;
}

void profiler_pb_set_cur_freqlv(int id, int idx)
{
	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return;
	}
	rtp.cur_freqlv[id] = idx;
}

void profiler_pb_set_cur_freq(int id, int freq)
{
	int i;

	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return;
	}

	if (!rtp.powertable[id] || rtp.powertable_size[id] == 0) {
		dev_err(profiler_adev->dev, "FreqPowerTable was not set, id(%d)", id);
		return;
	}

	for (i = rtp.cur_freqlv[id]; i > 0; i--)
		if (i < rtp.powertable_size[id] && freq <= rtp.powertable[id][i].freq)
			break;

	while (i < rtp.powertable_size[id]) {
		if (freq >= rtp.powertable[id][i].freq) {
			rtp.cur_freqlv[id] = i;
			return;
		}
		i++;
	}

	if (rtp.powertable_size[id] > 0)
		rtp.cur_freqlv[id] = rtp.powertable_size[id] - 1;
}

int profiler_pb_get_next_feasible_freq(int id)
{
	long fnext = 0;
	long rendertime, est_rendertime, fcur, ticks, in_mgn, out_mgn;
	int dvfslv;

	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return -EINVAL;
	}

	if (!rtp.powertable[id] || rtp.powertable_size[id] == 0) {
		dev_err(profiler_adev->dev, "FreqPowerTable was not set, id(%d)", id);
		return -EINVAL;
	}

	in_mgn = profiler_margin[id];
	if (id == PROFILER_GPU) {
		dvfslv = rtp.last_gpufreqlv;
		out_mgn = in_mgn + (long)rtp.gputime_boost_pm;
		rendertime = (long)rtp.last_gputime + ((long)rtp.last_gputime * out_mgn / 1000L);
	} else if (id == rtp.target_clusterid) {
		dvfslv = rtp.last_cpufreqlv;
		if (rtp.pb_next_cpu_minlock_margin > in_mgn)
			out_mgn = rtp.pb_next_cpu_minlock_margin;
		else
			out_mgn = in_mgn;
		out_mgn += (long)rtp.cputime_boost_pm;
		rendertime = (long)rtp.last_cputime + ((long)rtp.last_cputime * out_mgn / 1000L);
	} else
		return 0;

	est_rendertime = rendertime;
	fcur = (long)rtp.powertable[id][dvfslv].freq;
	ticks = est_rendertime * fcur;

	/* Move non-feasible to feasible region */
	while (dvfslv > 0 && est_rendertime > rtp.target_frametime) {
		dvfslv--;
		fnext = (long)rtp.powertable[id][dvfslv].freq;
		est_rendertime = ticks / fnext;
	}

	/* Move to edge of feasible region */
	while ((dvfslv + 1) < rtp.powertable_size[id]) {
		fnext = (long)rtp.powertable[id][dvfslv + 1].freq;
		est_rendertime = ticks / fnext;
		if (est_rendertime < rtp.target_frametime)
			dvfslv++;
		else
			break;
	}

	if ((rtp.outinfolevel & 8) == 8)
		profiler_info("EGP:PB:%d: mgn(i,o)=%4ld,%4ld, f(cur,nxt)=%5ld,%5ld, rtime(prev,next)=%8ld,%8ld\n"
				, id, in_mgn, out_mgn, fcur/1000, fnext/1000, rendertime, est_rendertime);
	return fnext;
}

void profiler_pb_update_minlock(u64 nowtimeus, int rtp_tail)
{
	struct profiler_interframe_data *dst;
	int cur_clid = rtp.target_clusterid;
	int id, fnext;

	if (rtp_tail >= PROFILER_TABLE_MAX) {
		dev_err(profiler_adev->dev, "Index for interframe is over MAX");
		return;
	}

	dst = &interframe[rtp_tail];

	if ((!rtp.powertable[cur_clid]) || (rtp.powertable_size[cur_clid] <= 0)) {
		dev_err(profiler_adev->dev, "FreqPowerTable was not set, cpuid(%d)", cur_clid);
		return;
	}

	if (rtp.pb_next_cpu_minlock_boosting_fpsdrop > 0) {
		fnext = profiler_pb_get_next_feasible_freq(PROFILER_GPU);
		rtp.pmqos_minlock_next[PROFILER_GPU] = fnext;
		dst->sdp_next_fgpu = fnext;
	}

	for (id = 0; id < NUM_OF_CPU_DOMAIN; id++) {
		if (rtp.pb_next_cpu_minlock_margin > 0)
			fnext = profiler_pb_get_next_feasible_freq(id);
		else
			fnext = 0;

		rtp.pmqos_minlock_next[id] = fnext;
		if (id == cur_clid)
			dst->sdp_next_fcpu = fnext;
	}
}

void profiler_pidframeinfo_reset(void)
{
	int i;

	for (i = 0; i < PROFILER_PID_FRAMEINFO_TABLE_SIZE; i++)
		rtp.pfinfo.pid[i] = 0;
}

int profiler_pidframeinfo_pididx(u32 pid)
{
	int i;

	for (i = 0; i < PROFILER_PID_FRAMEINFO_TABLE_SIZE; i++)
		if (rtp.pfinfo.pid[i] != 0 && rtp.pfinfo.pid[i] == pid)
			return i;
	return -1;
}

int profiler_pidframeinfo_getnopids(void)
{
	int i;
	int no = 0;

	for (i = 0; i < PROFILER_PID_FRAMEINFO_TABLE_SIZE; i++)
		if (rtp.pfinfo.pid[i] != 0)
			no++;

	return no;
}

int profiler_pidframeinfo_getempty(void)
{
	int i;
	u64 lru_ts = 0;
	int lruidx = -1;

	for (i = 0; i < PROFILER_PID_FRAMEINFO_TABLE_SIZE; i++) {
		if (rtp.pfinfo.pid[i] == 0)
			return i;

		/* if there is not an empty slot, find lru policy */
		if (lru_ts == 0 || rtp.pfinfo.latest_ts[i] < lru_ts) {
			lru_ts = rtp.pfinfo.latest_ts[i];
			lruidx = i;
		}
	}

	return lruidx;
}

int profiler_pidframeinfo_getmaxhitidx(void)
{
	int i;
	int maxhit = 0;
	int maxidx = -1;

	for (i = 0; i < PROFILER_PID_FRAMEINFO_TABLE_SIZE; i++) {
		if (rtp.pfinfo.pid[i] > 0 && maxhit < rtp.pfinfo.hit[i]) {
			maxidx = i;
			maxhit = rtp.pfinfo.hit[i];
		}
	}

	return maxidx;
}

s32 profiler_get_pb_margin_info(int id, u16 *no_boost)
{
	s32 avg_margin = 0;

	if (rtp.pb_margin_info[id].no_value > 0)
		avg_margin = rtp.pb_margin_info[id].sum_margin / (s32)rtp.pb_margin_info[id].no_value;

	*no_boost = rtp.pb_margin_info[id].no_boost;

	rtp.pb_margin_info[id].sum_margin = 0;
	rtp.pb_margin_info[id].no_value = 0;
	rtp.pb_margin_info[id].no_boost = 0;
	return avg_margin;
}

void profiler_fps_profiler(int qidx)
{
	ktime_t nowtimeus = ktime_get_real() / 1000L;
	u64 latest_ts;
	int previdx = (qidx + PROFILER_TABLE_MAX - 1) % PROFILER_TABLE_MAX;
	int hitpfi = -1;
	int i;

	latest_ts = interframe[qidx].timestamp;
	if ((nowtimeus - interframe[previdx].timestamp) > 1000000L) {
		/* If timestamp of prev is older than 1sec, reset */
		profiler_pidframeinfo_reset();
	} else {
		/* Move 1sec index */
		for (i = 0; i < PROFILER_TABLE_MAX; i++) {
			int idx = (rtp.pfinfo.onesecidx + i) % PROFILER_TABLE_MAX;
			u64 ts = interframe[idx].timestamp;
			u64 interval = latest_ts - ts;
			int pfi;

			/* If current timestamp is in 1sec, finish this loop */
			if (interval <= profiler_fps_calc_average_us) {
				rtp.pfinfo.onesecidx = idx;
				break;
			}

			/* Find index in pidframeinfo which matched to pid */
			pfi = profiler_pidframeinfo_pididx(interframe[idx].tgid);
			if (pfi >= 0) {
				rtp.pfinfo.hit[pfi]--;
				/* if not hit any more, delete it */
				if (rtp.pfinfo.hit[pfi] == 0)
					rtp.pfinfo.pid[pfi] = 0;
				else {
					/* update earliest timestamp */
					rtp.pfinfo.earliest_ts[pfi] = ts;
				}
			}
		}

		/* find idx for new pid */
		hitpfi = profiler_pidframeinfo_pididx(interframe[qidx].tgid);
	}

	if (hitpfi >= 0) {
		/* Update if hit */
		rtp.pfinfo.hit[hitpfi]++;
		rtp.pfinfo.latest_interval[hitpfi] = latest_ts - rtp.pfinfo.latest_ts[hitpfi];
		rtp.pfinfo.latest_ts[hitpfi] = latest_ts;
	} else {
		/* Miss, then add it */
		int pfi = profiler_pidframeinfo_getempty();

		if (pfi >= 0) {
			rtp.pfinfo.pid[pfi] = interframe[qidx].tgid;
			rtp.pfinfo.hit[pfi] = 1;
			rtp.pfinfo.latest_ts[pfi] = latest_ts;
			rtp.pfinfo.earliest_ts[pfi] = latest_ts;
			rtp.pfinfo.latest_interval[pfi] = 0;
		}
	}

	if ((rtp.outinfolevel & 4) == 4)
		for(i = 0; i < PROFILER_PID_FRAMEINFO_TABLE_SIZE; i++) {
			profiler_info("EGP: %d: %6u, %3u, %8llu, %8llu, %8llu, 1sidx=%d /%d"
				, i, rtp.pfinfo.pid[i], rtp.pfinfo.hit[i]
				, rtp.pfinfo.earliest_ts[i], rtp.pfinfo.latest_ts[i], rtp.pfinfo.latest_interval[i]
				, rtp.pfinfo.onesecidx, qidx);
		}
}

void profiler_fps_calculator(u64 nowtimeus, int rtp_tail)
{
	int dstidx = -1;

	const u64 max_interval = 99999999L;
	const u32 max_fps = 99999;
	u64 sum_interval;
	u32 avg_fps = 0;
	u64 latest_interval;
	u32 latest_fps = 0;
	u64 nowsum_interval;
	u32 expected_afps = 0;
	u64 now_interval;
	u32 expected_fps = 0;

	u32 tft = rtp.target_frametime;
	u32 tfps = rtp.target_fps;
	u32 fps_drop_pm = 0;
	u32 frame_drop_pm = 0;

	if (rtp.user_target_pid > 0)
		dstidx = profiler_pidframeinfo_pididx(rtp.user_target_pid);

	if (dstidx < 0)
		dstidx = profiler_pidframeinfo_getmaxhitidx();

	if (dstidx < 0) {
		if ((rtp.outinfolevel & 1) == 1)
			profiler_info("EGP: FPS Calc: dstidx < 0\n");
		return;
	}

	rtp.pfinfo.latest_targetpid = rtp.pfinfo.pid[dstidx];

	sum_interval = rtp.pfinfo.latest_ts[dstidx] - rtp.pfinfo.earliest_ts[dstidx];
	latest_interval = rtp.pfinfo.latest_interval[dstidx];
	nowsum_interval = nowtimeus - rtp.pfinfo.earliest_ts[dstidx];
	now_interval = nowtimeus - rtp.pfinfo.latest_ts[dstidx];

	if (nowsum_interval > max_interval)
		nowsum_interval = max_interval;
	if (now_interval > max_interval)
		now_interval = max_interval;

	if (sum_interval > 0)
		avg_fps = 10000000L * (u64)rtp.pfinfo.hit[dstidx] / sum_interval;
	if (latest_interval > 0)
		latest_fps = (u32)(10000000L / latest_interval);
	if (nowsum_interval > 0)
		expected_afps = (u32)(10000000L * (u64)(rtp.pfinfo.hit[dstidx] + 1) / nowsum_interval);
	if (now_interval > 0)
		expected_fps = (u32)(10000000L / now_interval);
	if (expected_fps > max_fps)
		expected_fps = max_fps;

	if (tft > 0) {
		if (expected_afps < tfps)
			fps_drop_pm = (tfps - expected_afps) * 1000 / tfps;

		if ((now_interval >> 3) > tft)
			frame_drop_pm = 9999;
		else if (now_interval > tft)
			frame_drop_pm = (now_interval - tft) * 1000 / tft;
	}

	rtp.pfinfo.expected_swaptime = now_interval;
	rtp.pfinfo.avg_fps = avg_fps;
	rtp.pfinfo.exp_afps = expected_fps;
	rtp.pfinfo.fps_drop_pm = fps_drop_pm;
	rtp.pfinfo.frame_drop_pm = frame_drop_pm;

	if ((rtp.outinfolevel & 1) == 1)
		profiler_info(
			"EGP: #PID=%2d, Interval(t,l,et,e)=%8llu,%8llu,%8llu,%8llu, fps(t/a/l/ea/e)=%5u,%5u,%5u,%5u,%5u, drop(fps,frame)=%5u,%5u\n"
			, profiler_pidframeinfo_getnopids()
			, sum_interval, latest_interval, nowsum_interval, now_interval
			, tfps, avg_fps, latest_fps, expected_afps, expected_fps
			, fps_drop_pm, frame_drop_pm);
}

void profiler_pb_update_boost_margin(void)
{
	int id, cpu_margin_prev, cpu_margin_next;
	u32 target_cl_msk;

	rtp.pb_next_cpu_minlock_margin = -1;
	rtp.pb_next_cpu_minlock_boosting_fpsdrop = -1;
	rtp.pb_next_cpu_minlock_boosting_framedrop = -1;

	if (rtp.pfinfo.avg_fps < 50)
		return;
	if (rtp.target_clusterid < 0 || rtp.target_clusterid >= NUM_OF_CPU_DOMAIN)
		return;

	id = rtp.target_clusterid;
	cpu_margin_prev = cpu_margin_next = profiler_margin[id];
	target_cl_msk = 1 << id;

	/* CPU minlock by frame */
	if (rtp.framedrop_detection_pm_minlock > 0 && rtp.pfinfo.frame_drop_pm >= rtp.framedrop_detection_pm_minlock) {
		int new_margin = rtp.pfinfo.frame_drop_pm;

		if (new_margin > rtp.pb_cpu_minlock_margin_max)
			new_margin = rtp.pb_cpu_minlock_margin_max;
		rtp.pb_next_cpu_minlock_boosting_framedrop = new_margin;
		if (new_margin > rtp.pb_next_cpu_minlock_margin)
			rtp.pb_next_cpu_minlock_margin = new_margin;
	}

	/* CPU minlock by fps */
	if (rtp.fpsdrop_detection_pm_minlock > 0 && rtp.pfinfo.fps_drop_pm >= rtp.fpsdrop_detection_pm_minlock) {
		int new_margin = rtp.pfinfo.fps_drop_pm;

		if (new_margin > rtp.pb_cpu_minlock_margin_max)
			new_margin = rtp.pb_cpu_minlock_margin_max;
		rtp.pb_next_cpu_minlock_boosting_fpsdrop = new_margin;
		if (new_margin > rtp.pb_next_cpu_minlock_margin)
			rtp.pb_next_cpu_minlock_margin = new_margin;
	}

	/* CPU margin by frame */
	if (rtp.framedrop_detection_pm_mgn > 0 && rtp.pfinfo.frame_drop_pm >= rtp.framedrop_detection_pm_mgn) {
		int new_margin = rtp.pfinfo.frame_drop_pm - rtp.framedrop_detection_pm_mgn;

		if (new_margin > cpu_margin_next)
			cpu_margin_next = new_margin;
	}

	/* CPU margin by fps */
	if (rtp.fpsdrop_detection_pm_mgn > 0 && rtp.pfinfo.fps_drop_pm >= rtp.fpsdrop_detection_pm_mgn) {
		int new_margin = rtp.pfinfo.fps_drop_pm;

		if (new_margin > cpu_margin_next)
			cpu_margin_next = new_margin;
	}

	/* CPU Bitmap */
	if (rtp.target_clusters_bitmap_fpsdrop_detection_pm > 0 &&
			rtp.pfinfo.fps_drop_pm >= rtp.target_clusters_bitmap_fpsdrop_detection_pm)
		target_cl_msk = rtp.target_clusters_bitmap;

	for (id = 0; id < NUM_OF_CPU_DOMAIN; id++) {
		u32 bitmsk = 1 << id;
		int new_margin = -1;

		if ((target_cl_msk & bitmsk) != 0 )
			new_margin = cpu_margin_next;
		else
			new_margin = profiler_margin[id];

		if (new_margin > rtp.pb_cpu_mgn_margin_max)
			new_margin = rtp.pb_cpu_mgn_margin_max;

		if (new_margin != profiler_margin_applied[id]) {
			profiler_margin_applied[id] = new_margin;
			emstune_set_pboost(3, 0, PROFILER_GET_CPU_BASE_ID(id), new_margin / 10);
			if (new_margin > profiler_margin[id])
				rtp.pb_margin_info[id].no_boost++;
		}

		if ((rtp.outinfolevel & 2) == 2)
			profiler_info("EGP: CPU margin(id, in, applied) = %d, %5d,%5d\n"
				, id, profiler_margin[id], profiler_margin_applied[id]);
	}

	if (rtp.last_gputime > rtp.target_frametime) {
		int gpumgn = profiler_margin[PROFILER_GPU] + rtp.gputime_boost_pm;
		u32 gputime = (u32)rtp.last_gputime * (u32)(1000 + gpumgn) / 1000;
		u32 diff = (gputime > rtp.target_frametime)? gputime - rtp.target_frametime : 0;

		if (diff > 0) {
			profiler_margin_applied[PROFILER_GPU] = gpumgn;
			rtp.pb_margin_info[PROFILER_GPU].no_boost++;
		}

		if ((rtp.outinfolevel & 2) == 2)
			profiler_info("EGP: GPU margin(in, applied) = %5d,%5d\n"
				, profiler_margin[PROFILER_GPU], profiler_margin_applied[PROFILER_GPU]);
	}
}

long profiler_pb_get_gpu_target(unsigned long freq, uint64_t activepct, uint64_t target_norm)
{
	ktime_t nowtimeus = ktime_get_real() / 1000L;
	int gpu_mgn = profiler_margin[PROFILER_GPU];
	int rtp_tail = rtp.tail;
	long fnext = 0;

	mutex_lock(&profiler_pmqos_lock);
	profiler_reset_next_minlock();

	if (rtp.target_frametime > 0) {
		profiler_fps_calculator(nowtimeus, rtp_tail);
		profiler_pb_update_boost_margin();
		profiler_pb_update_minlock(nowtimeus, rtp_tail);
		for (int i = 0; i < NUM_OF_DOMAIN; i++) {
			rtp.pb_margin_info[i].no_value++;
			rtp.pb_margin_info[i].sum_margin += profiler_margin_applied[i];
		}
	} else {
		for (int i = 0; i < NUM_OF_DOMAIN; i++) {
			rtp.pb_margin_info[i].no_value++;
			rtp.pb_margin_info[i].sum_margin += profiler_margin[i];
		}
	}

	profiler_set_pmqos_next();
	mutex_unlock(&profiler_pmqos_lock);

	fnext = (long)target_norm + ((long)target_norm * (long)gpu_mgn / 1000);
	if (activepct >= rtp.gpu_forced_boost_activepct && fnext < freq) {
		fnext = freq + 10000;
	}
	if (fnext < rtp.pmqos_minlock_next[PROFILER_GPU])
		fnext = rtp.pmqos_minlock_next[PROFILER_GPU];
	profiler_margin_applied[PROFILER_GPU] = gpu_mgn;

	return fnext;
}
/* End of Performance Booster */
