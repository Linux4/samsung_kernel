// SPDX-License-Identifier: GPL-2.0
/*
 * @file sgpu_profiler_v1.c
 * @copyright 2022 Samsung Electronics
 */
#include "sgpu_profiler.h"

struct amdgpu_device *profiler_adev;

static struct exynos_pm_qos_request sgpu_profiler_gpu_min_qos;
static struct freq_qos_request sgpu_profiler_cpu_min_qos[NUM_OF_CPU_DOMAIN];
static struct profiler_interframe_data interframe[PROFILER_TABLE_MAX];
static struct profiler_rtp_data rtp;
static struct profiler_pid_data pid;
static struct profiler_info_data pinfo;
static int profiler_pbooster_margin[NUM_OF_DOMAIN];

/* Start current DVFS level, find next DVFS lv met feasible region */
int profiler_get_freq_margin(void)
{
	struct devfreq *df = profiler_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	return data->freq_margin;
}

int profiler_set_freq_margin(int id, int freq_margin)
{
	struct devfreq *df = profiler_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	if (freq_margin > 1000 || freq_margin < -1000) {
		dev_err(profiler_adev->dev, "freq_margin range : (-1000 ~ 1000)\n");
		return -EINVAL;
	}
	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return -EINVAL;
	}

	profiler_pbooster_margin[id] = freq_margin;

	/* Apply GPU margin */
	if (id == PROFILER_GPU)
		data->freq_margin = freq_margin;

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
		exynos_pm_qos_update_request(&sgpu_profiler_gpu_min_qos, rtp.pmqos_minlock_next[id]);
		rtp.pmqos_minlock_prev[id] = rtp.pmqos_minlock_next[id];
	}
}

static void profiler_reset_next_minlock(void)
{
	memset(&rtp.pmqos_minlock_next[0], 0, sizeof(rtp.pmqos_minlock_next[0]) * NUM_OF_DOMAIN);
}

static bool profiler_get_is_profiler_governor(struct devfreq *df)
{
	struct sgpu_governor_data *gdata;

	gdata = df->data;
	return (gdata->governor->id == SGPU_DVFS_GOVERNOR_PROFILER);
}

void profiler_set_profiler_governor(int mode)
{
	char *profiler_governor = "profiler";
	const int gpu_profiler_polling_interval = 16;
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
				gpu_dvfs_set_polling_interval(gpu_profiler_polling_interval);
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
				profiler_reset_next_minlock();
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
	if ((us < RENDERTIME_MAX && us > RENDERTIME_MIN) || us == 0)
		rtp.target_frametime = us;
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

		profiler_reset_next_minlock();
		profiler_set_pmqos_next();
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

static int profiler_pb_get_next_feasible_freq(int id)
{
	long fnext = 0;

	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return -EINVAL;
	}

	if (!rtp.powertable[id] || rtp.powertable_size[id] == 0) {
		dev_err(profiler_adev->dev, "FreqPowerTable was not set, id(%d)", id);
		return -EINVAL;
	}

	if (id == PROFILER_GPU || id == rtp.target_cpuid_cur) {
		int dvfslv = rtp.cur_freqlv[id];
		long est_rendertime = (id == PROFILER_GPU) ? (long)rtp.last_gputime : (long)rtp.last_cputime;
		long ticks = ((long)rtp.powertable[id][dvfslv].freq * est_rendertime);

		/* Move non-feasible to feasible region */
		while (dvfslv > 0 && est_rendertime > rtp.target_frametime) {
			dvfslv--;
			fnext = (long)rtp.powertable[id][dvfslv].freq * (1000 - profiler_pbooster_margin[id]) / 1000;
			est_rendertime = ticks / fnext;
		}

		/* Move to edge of feasible region */
		while ((dvfslv + 1) < rtp.powertable_size[id]) {
			fnext = (long)rtp.powertable[id][dvfslv + 1].freq * (1000 - profiler_pbooster_margin[id]) / 1000;
			est_rendertime = ticks / fnext;
			if (est_rendertime < rtp.target_frametime)
				dvfslv++;
			else
				break;
		}
	}
	return fnext;
}

static void profiler_pbooster(struct profiler_interframe_data *dst)
{
	struct devfreq *df = profiler_adev->devfreq;
	int cur_cpu_id = rtp.target_cpuid_cur;

	if (dst == NULL) {
		dev_err(profiler_adev->dev, "NULL data for interframe");
		return;
	}

	if ((!rtp.powertable[cur_cpu_id]) || (rtp.powertable_size[cur_cpu_id] <= 0)) {
		dev_err(profiler_adev->dev, "FreqPowerTable was not set, cpuid(%d)", cur_cpu_id);
		return;
	}

	if (profiler_get_is_profiler_governor(df) && rtp.target_frametime > 0) {
		int id, fnext;
		int cur_fcpulv = rtp.cur_freqlv[cur_cpu_id];
		int cur_fgpulv = rtp.cur_freqlv[PROFILER_GPU];

		dst->sdp_next_cpuid = cur_cpu_id;
		dst->sdp_cur_fcpu = rtp.powertable[cur_cpu_id][cur_fcpulv].freq;
		dst->sdp_cur_fgpu = rtp.powertable[PROFILER_GPU][cur_fgpulv].freq;

		fnext = profiler_pb_get_next_feasible_freq(PROFILER_GPU);
		rtp.pmqos_minlock_next[PROFILER_GPU] = fnext;
		dst->sdp_next_fgpu = fnext;

		for (id = 0; id < NUM_OF_CPU_DOMAIN; id++) {
			fnext = profiler_pb_get_next_feasible_freq(id);

			rtp.pmqos_minlock_next[id] = fnext;
			if (id == cur_cpu_id)
				dst->sdp_next_fcpu = fnext;
		}

	} else {
		profiler_reset_next_minlock();
	}

	profiler_set_pmqos_next();
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

	atomic_set(&rtp.lasthw_read, 1);

	rtp.target_cpuid_cur = profiler_get_current_cpuid();
	rtp.prev_swaptimestamp = end;
	atomic_inc(&rtp.vsync_swapcall_counter);
	pinfo.frame_counter++;

	dst = &interframe[rtp.tail];
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
	rtp.last_cputime = cputime;
	rtp.last_gputime = gputime;

	dst->vsync_interval = rtp.vsync_interval;

	if (atomic_read(&pid.list_readout) != 0)
		sgpu_profiler_pid_init();

	list_tail_pos = atomic_read(&pid.list_top);
	if (list_tail_pos < NUM_OF_PID) {
		u8 coreid = (u8)get_cpu();
		u64 mpid = (u64)current->tgid;
		u64 hash = ((mpid >> 32) ^ mpid) & 0xffff;

		if (hash == 0)
			hash = 1; /* Filtering System UI */
		coreid++; /* '0' means not recorded */

		pid.core_list[list_tail_pos] = coreid;
		pid.list[list_tail_pos] = (u32)hash;
		atomic_inc(&pid.list_top);
		put_cpu();
	}

	profiler_pbooster(dst);
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

	rtp.target_cpuid_cur = profiler_get_current_cpuid();
	rtp.target_frametime = RENDERTIME_DEFAULT_FRAMETIME;
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
