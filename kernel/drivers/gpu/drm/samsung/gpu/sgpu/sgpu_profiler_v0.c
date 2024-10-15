// SPDX-License-Identifier: GPL-2.0
/*
 * @file sgpu_profiler_v0.c
 * @copyright 2022 Samsung Electronics
 */
#include "sgpu_profiler.h"

struct amdgpu_device *profiler_adev;

static struct exynos_pm_qos_request sgpu_profiler_gpu_min_qos;
static struct exynos_pm_qos_request sgpu_profiler_gpu_max_qos;
static struct freq_qos_request sgpu_profiler_cpu_min_qos[NUM_OF_CPU_DOMAIN];

#define LIT_BASE_ID 0
#define MIDL_BASE_ID 4
#define MIDH_BASE_ID 7
#define BIG_BASE_ID 9
static struct profiler_interframe_data interframe[PROFILER_TABLE_MAX];
static struct profiler_rtp_data rtp;
static struct profiler_pid_data pid;
static struct profiler_info_data pinfo;
static struct profiler_cputracer_data cputracer;

/* Start current DVFS level, find next DVFS lv met feasible region */
int profiler_get_freq_margin(void)
{
	struct devfreq *df = profiler_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	return data->freq_margin;
}

int profiler_set_freq_margin(int freq_margin)
{
	struct devfreq *df = profiler_adev->devfreq;
	struct sgpu_governor_data *data = df->data;

	if (freq_margin > 1000 || freq_margin < -1000) {
		dev_err(profiler_adev->dev, "freq_margin range : (-1000 ~ 1000)\n");
		return -EINVAL;
	}

	data->freq_margin = freq_margin;

	return 0;
}

void profiler_set_cputracer_size(int queue_size)
{
	int i;

	cputracer.qsz = queue_size;
	for (i = 0; i < NUM_OF_CPU_DOMAIN; i++) {
		cputracer.bitmap[i] = 0UL;
		cputracer.hitcnt[i] = 0;
	}
}

static int profiler_get_current_cpuid(void)
{
	int i, id;
	u64 eoq = 1 << cputracer.qsz;
	unsigned int core = smp_processor_id();

	for (i = 0; i < NUM_OF_CPU_DOMAIN; i++)
		cputracer.bitmap[i] <<= 1;

	if (core >= BIG_BASE_ID)
		id = PROFILER_CL2;
	else if (core >= MIDH_BASE_ID)
		id = PROFILER_CL1H;
	else if (core >= MIDL_BASE_ID)
		id = PROFILER_CL1L;
	else
		id = PROFILER_CL0;

	for (i = 0; i < NUM_OF_CPU_DOMAIN; i++) {
		if ((eoq & cputracer.bitmap[i]) > 0 && cputracer.hitcnt[i] > 0)
			cputracer.hitcnt[i]--;
	}

	cputracer.bitmap[id] |= 1;
	cputracer.hitcnt[id]++;
	return id;
}

static void profiler_init_pmqos(void)
{
	struct cpufreq_policy *policy;

	if (pinfo.cpufreq_pm_qos_added  == 0) {

		pinfo.cpufreq_pm_qos_added  = 1;

		if (!freq_qos_request_active(&sgpu_profiler_cpu_min_qos[0])) {
			policy = cpufreq_cpu_get(LIT_BASE_ID);
			if (policy != NULL)
				freq_qos_tracer_add_request(&policy->constraints,
						&sgpu_profiler_cpu_min_qos[0],
						FREQ_QOS_MIN, PM_QOS_DEFAULT_VALUE);
		}

		if (!freq_qos_request_active(&sgpu_profiler_cpu_min_qos[1])) {
			policy = cpufreq_cpu_get(MIDL_BASE_ID);
			if (policy != NULL)
				freq_qos_tracer_add_request(&policy->constraints,
						&sgpu_profiler_cpu_min_qos[1],
						FREQ_QOS_MIN, PM_QOS_DEFAULT_VALUE);
		}

		if (!freq_qos_request_active(&sgpu_profiler_cpu_min_qos[2])) {
			policy = cpufreq_cpu_get(MIDH_BASE_ID);
			if (policy != NULL)
				freq_qos_tracer_add_request(&policy->constraints,
						&sgpu_profiler_cpu_min_qos[2],
						FREQ_QOS_MIN, PM_QOS_DEFAULT_VALUE);
		}
		if (!freq_qos_request_active(&sgpu_profiler_cpu_min_qos[3])) {
			policy = cpufreq_cpu_get(BIG_BASE_ID);
			if (policy != NULL)
				freq_qos_tracer_add_request(&policy->constraints,
						&sgpu_profiler_cpu_min_qos[3],
						FREQ_QOS_MIN, PM_QOS_DEFAULT_VALUE);
		}

	}

	memset(&rtp.pmqos_minlock_prev[0], 0, sizeof(rtp.pmqos_minlock_prev[0]) * NUM_OF_DOMAIN);
	memset(&rtp.pmqos_minlock_next[0], 0, sizeof(rtp.pmqos_minlock_next[0]) * NUM_OF_DOMAIN);
}

static void profiler_set_pmqos_next(void)
{
	int id;

	if (pinfo.cpufreq_pm_qos_added > 0) {
		for (id = PROFILER_CL0; id <= PROFILER_CL2; id++) {
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
	char *profiler_gpu_governor = "profiler";
	const int gpu_profiler_polling_interval = 16;
	static char *saved_gpu_governor;
	static int saved_polling_interval;
	int ret = 0;

	if ((mode == 1) && (saved_polling_interval == 0)) {
		/* enable gpu profiler governor */
		saved_gpu_governor = gpu_dvfs_get_governor();
		ret = gpu_dvfs_set_governor(profiler_gpu_governor);
		if (ret == 0) {
			/* disable LLC control by GPU Driver */
			gpu_dvfs_set_disable_llc_way(1);
			saved_polling_interval = gpu_dvfs_get_polling_interval();
			gpu_dvfs_set_polling_interval(gpu_profiler_polling_interval);
			gpu_dvfs_set_autosuspend_delay(1000);
			profiler_init_pmqos();
		} else {
			SGPU_LOG(profiler_adev, DMSG_WARNING, DMSG_ETC,
				"cannot change GPU governor for PROFILER, err:%d",
				ret);
			saved_gpu_governor = NULL;
		}
	} else if ((mode == 0) && (saved_polling_interval > 0) && (saved_gpu_governor != NULL)) {
		ret = gpu_dvfs_set_governor(saved_gpu_governor);
		if (ret == 0) {
			/* enable LLC control by GPU Driver */
			gpu_dvfs_set_disable_llc_way(0);
			gpu_dvfs_set_polling_interval(saved_polling_interval);
			saved_polling_interval = 0;
			gpu_dvfs_set_autosuspend_delay(50);
			profiler_reset_next_minlock();
			profiler_set_pmqos_next();
		} else {
			SGPU_LOG(profiler_adev, DMSG_WARNING, DMSG_ETC,
				"cannot change GPU governor back to normal, err:%d",
				ret);
		}
	} else {
		SGPU_LOG(profiler_adev, DMSG_WARNING, DMSG_ETC,
				"cannot change GPU governor from:%s, mode=%d, saved_polling_interval=%d",
				gpu_dvfs_get_governor(), mode, saved_polling_interval);
	}
}

void profiler_set_targetframetime(int us)
{
	if (rtp.shortterm_comb_ctrl_manual == -2)
		return;
	if (us < RENDERINGTIME_MAX_TIME && us > RENDERINGTIME_MIN_FRAME)
		rtp.target_frametime = us;
}

void profiler_set_targettime_margin(int us)
{
	if (rtp.shortterm_comb_ctrl_manual == -2)
		return;
	if (us < RENDERINGTIME_MAX_TIME && us > RENDERINGTIME_MIN_FRAME)
		rtp.targettime_margin = us;
}

void profiler_set_util_margin(int percentage)
{
	if (rtp.shortterm_comb_ctrl_manual == -2)
		return;
	rtp.workload_margin = percentage;
}

void profiler_set_decon_time(int us)
{
	if (rtp.shortterm_comb_ctrl_manual == -2)
		return;
	rtp.decon_time = us;
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
	int cur_nrq;

	/* Transaction to transfer Rendering Time to profiler */
	if (rtp.readout != 0) {
		int i;

		memset(&last_frame, 0, sizeof(struct profiler_interframe_data));
		cur_nrq = rtp.nrq;
		for (i = 0; i < NUM_OF_CPU_DOMAIN; i++) {
			cputracer.bitmap[i] = 0UL;
			cputracer.hitcnt[i] = 0;
		}

		profiler_reset_next_minlock();
		profiler_set_pmqos_next();
	} else {
		do {
			cur_nrq = rtp.nrq;
			if (rtp.tail == 0)
				memcpy(&last_frame, &interframe[PROFILER_TABLE_MAX - 1],
						sizeof(struct profiler_interframe_data));
			else
				memcpy(&last_frame, &interframe[rtp.tail - 1],
						sizeof(struct profiler_interframe_data));
		} while (cur_nrq != rtp.nrq);
	}
	rtp.readout = 1;

	if (cur_nrq > 0 && last_frame.nrq > 0) {
		u64 tmp;

		tmp = (last_frame.sum_pre / last_frame.nrq);
		times[PREAVG] = min(tmp, RENDERINGTIME_MAX_TIME);
		tmp = last_frame.sum_cpu / last_frame.nrq;
		times[CPUAVG] = min(tmp, RENDERINGTIME_MAX_TIME);
		tmp = last_frame.sum_swap / last_frame.nrq;
		times[V2SAVG] = min(tmp, RENDERINGTIME_MAX_TIME);
		tmp = last_frame.sum_gpu / last_frame.nrq;
		times[GPUAVG] = min(tmp, RENDERINGTIME_MAX_TIME);
		tmp = last_frame.sum_v2f / last_frame.nrq;
		times[V2FAVG] = min(tmp, RENDERINGTIME_MAX_TIME);

		times[PREMAX] = min(last_frame.max_pre, RENDERINGTIME_MAX_TIME);
		times[CPUMAX] = min(last_frame.max_cpu, RENDERINGTIME_MAX_TIME);
		times[V2SMAX] = min(last_frame.max_swap, RENDERINGTIME_MAX_TIME);
		times[GPUMAX] = min(last_frame.max_gpu, RENDERINGTIME_MAX_TIME);
		times[V2FMAX] = min(last_frame.max_v2f, RENDERINGTIME_MAX_TIME);

		rtp.vsync_frame_counter = 0;
		rtp.vsync_counter = 0;
	} else {
		memset(&times[0], 0, sizeof(times[0]) * NUM_OF_TIMEINFO);
	}
}

u32 profiler_get_target_cpuid(u64 *cputrace_info)
{
	int i;
	u64 ret = 0;

	/* ret, ... HitCntCL2[8], HitCntCL1[8], HitCntCL0[8], QueueSize[8] */
	for (i = 0; i < NUM_OF_CPU_DOMAIN; i++)
		ret |= ((u64)(cputracer.hitcnt[i] & 0xff)) << (i << 3);
	ret <<= 8;
	ret |= (u64)(cputracer.qsz & 0xff);

	*cputrace_info = ret;
	return rtp.target_cpuid_cur;
}

void profiler_pid_get_list(u16 *list)
{
	int prev_top;

	if (pid.list_readout != 0)
		memset(&list[0], 0, sizeof(list[0]) * NUM_OF_PID);
	else {
		/* Transaction to transfer PID list to profiler */
		do {
			prev_top = pid.list_top;
			memcpy(list, &pid.list[0], sizeof(pid.list));
		} while (prev_top != pid.list_top);
	}
	pid.list_readout = 1;
}

void profiler_stc_set_comb_ctrl(int val)
{
	if (rtp.shortterm_comb_ctrl_manual <= -2)
		rtp.shortterm_comb_ctrl = -1;
	if (rtp.shortterm_comb_ctrl_manual == -1)
		rtp.shortterm_comb_ctrl = val;
	else
		rtp.shortterm_comb_ctrl = rtp.shortterm_comb_ctrl_manual;
}

int profiler_stc_set_powertable(int id, int cnt, struct freq_table *table)
{
	int i;

	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return -EINVAL;
	}

	rtp.powertable[id] = kzalloc(sizeof(struct profiler_freq_estpower_data) * cnt, GFP_KERNEL);
	rtp.powertable_size[id] = cnt;

	for (i = 0; i < cnt; i++) {
		rtp.powertable[id][i].freq = table[i].freq;
		rtp.powertable[id][i].power = table[i].dyn_cost + table[i].st_cost;
	}

	return 0;
}

void profiler_stc_set_busy_domain(int id)
{
	PROFILER_UNUSED(id);
}

void profiler_stc_set_cur_freqlv(int id, int idx)
{
	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return;
	}

	rtp.cur_freqlv[id] = idx;
}

void profiler_stc_set_cur_freq(int id, int freq)
{
	int i;

	if (id < 0 || id >= NUM_OF_DOMAIN) {
		dev_err(profiler_adev->dev, "Out of Range for id");
		return;
	}
	for (i = rtp.cur_freqlv[id]; i > 0; i--) {
		if (i < rtp.powertable_size[id] && freq <= rtp.powertable[id][i].freq)
			break;
	}

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

int profiler_stc_config_show(int page_size, char *buf)
{
	int ret;

	ret = snprintf(buf, page_size, "[0]comb = %d / cur=%d\n"
			, rtp.shortterm_comb_ctrl_manual, rtp.shortterm_comb_ctrl);
	ret += snprintf(buf + ret, page_size - ret, "[1]workload_margin = %d %%\n", rtp.workload_margin);
	ret += snprintf(buf + ret, page_size - ret, "[2]decon_time = %d ns\n", rtp.decon_time);
	ret += snprintf(buf + ret, page_size - ret, "[3]target_frametime_margin = %d ns\n", rtp.targettime_margin);
	ret += snprintf(buf + ret, page_size - ret, "[4]max min_lock = %d MHz\n", rtp.max_minlock_freq);
	ret += snprintf(buf + ret, page_size - ret, "target_frametime = %d ns\n", rtp.target_frametime);
	return ret;
}

int profiler_stc_config_store(const char *buf)
{
	int id, val;

	if (sscanf(buf, "%d %d", &id, &val) != 2)
		return -1;

	switch (id) {
	case 0:
		if (val >= -2) {
			rtp.shortterm_comb_ctrl_manual = val;
			if (val != -1)
				rtp.shortterm_comb_ctrl = val;
		}
		break;
	case 1:
		if (val >= 50 && val <= 150)
			rtp.workload_margin = val;
		break;
	case 2:
		if (val >= 0 && val <= 16000)
			rtp.decon_time = val;
		break;
	case 3:
		if (val >= 0 && val <= 16000)
			rtp.targettime_margin = val;
		break;
	case 4:
		if (val >= 0 && val <= 999000)
			rtp.max_minlock_freq = val;
		break;
	}
	return 0;
}

static void profiler_stc_get_next_feasible_freqlv(int *nextcpulv, int *nextgpulv)
{
	int targetframetime = rtp.target_frametime - rtp.targettime_margin;
	int tmax = targetframetime + targetframetime - rtp.decon_time;
	int lvsz_cpu = rtp.powertable_size[rtp.target_cpuid_cur];
	int lvsz_gpu = rtp.powertable_size[PROFILER_GPU];
	int fcpulv = rtp.cur_freqlv[rtp.target_cpuid_cur];
	int fgpulv = rtp.cur_freqlv[PROFILER_GPU];
	int fcpulvlimit = 0;
	int fgpulvlimit = 0;

	int next_cputime = rtp.last_cputime;
	int next_gputime = rtp.last_gputime;
	long cpu_workload = (long)rtp.powertable[rtp.target_cpuid_cur][fcpulv].freq
		* (long)rtp.last_cputime * (long)rtp.workload_margin / 100;
	long gpu_workload = (long)rtp.powertable[PROFILER_GPU][fgpulv].freq
		* (long)rtp.last_gputime * (long)rtp.workload_margin / 100;

	if (nextcpulv == NULL || nextgpulv == NULL) {
		dev_err(profiler_adev->dev, "NULL data for nextcpulv, nextgpulv");
		return;
	}

	/* Move to feasible region */
	while (fcpulv > 0 && next_cputime > targetframetime) {
		fcpulv--;
		next_cputime = (int)(cpu_workload /
				rtp.powertable[rtp.target_cpuid_cur][fcpulv].freq);
	}
	while (fgpulv > 0 && next_gputime > targetframetime) {
		fgpulv--;
		next_gputime = (int)(gpu_workload /
				rtp.powertable[PROFILER_GPU][fgpulv].freq);
	}
	if (rtp.shortterm_comb_ctrl == 0)
		tmax = COMB_CTRL_OFF_TMAX;
	else {
		if (rtp.shortterm_comb_ctrl < fcpulv)
			fcpulvlimit = fcpulv - rtp.shortterm_comb_ctrl;
		if (rtp.shortterm_comb_ctrl < fgpulv)
			fgpulvlimit = fgpulv - rtp.shortterm_comb_ctrl;
		while ((next_cputime + next_gputime) > tmax) {
			if (fcpulv > fcpulvlimit && next_cputime > next_gputime) {
				fcpulv--;
				next_cputime = (int)(cpu_workload /
						rtp.powertable[rtp.target_cpuid_cur][fcpulv].freq);
			} else if (fgpulv > fgpulvlimit) {
				fgpulv--;
				next_gputime = (int)(gpu_workload /
						rtp.powertable[PROFILER_GPU][fgpulv].freq);
			} else
				break;
		}
	}

	/* Find min power in feasible region */
	while ((fcpulv + 1) < lvsz_cpu && (fgpulv + 1) < lvsz_gpu) {
		int pcpudec = rtp.powertable[rtp.target_cpuid_cur][fcpulv + 1].power +
			rtp.powertable[PROFILER_GPU][fgpulv].power;
		int pgpudec = rtp.powertable[rtp.target_cpuid_cur][fcpulv].power +
			rtp.powertable[PROFILER_GPU][fgpulv + 1].power;
		int est_cputime = (int)(cpu_workload / rtp.powertable[rtp.target_cpuid_cur][fcpulv + 1].freq);
		int est_gputime = (int)(gpu_workload / rtp.powertable[PROFILER_GPU][fgpulv + 1].freq);

		if (pcpudec < pgpudec && est_cputime < targetframetime && (est_cputime + next_gputime) < tmax) {
			fcpulv++;
			next_cputime = est_cputime;
		} else if (est_gputime < targetframetime && (next_cputime + est_gputime) < tmax) {
			fgpulv++;
			next_gputime = est_gputime;
		} else
			break;
	}

	while ((fcpulv + 1) < lvsz_cpu) {
		int est_cputime = (int)(cpu_workload / rtp.powertable[rtp.target_cpuid_cur][fcpulv + 1].freq);

		if (est_cputime < targetframetime && (est_cputime + next_gputime) < tmax) {
			fcpulv++;
			next_cputime = est_cputime;
		} else
			break;
	}
	while ((fgpulv + 1) < lvsz_gpu) {
		int est_gputime = (int)(gpu_workload / rtp.powertable[PROFILER_GPU][fgpulv + 1].freq);

		if (est_gputime < targetframetime && (next_cputime + est_gputime) < tmax) {
			fgpulv++;
			next_gputime = est_gputime;
		} else
			break;
	}

	*nextcpulv = fcpulv;
	*nextgpulv = fgpulv;
}
static void profiler_stc_control(struct profiler_interframe_data *dst)
{
	struct devfreq *df = profiler_adev->devfreq;

	if (dst == NULL) {
		dev_err(profiler_adev->dev, "NULL data for interframe");
		return;
	}

	if (profiler_get_is_profiler_governor(df) && rtp.target_frametime > 0) {
		int cur_fcpulv = rtp.cur_freqlv[rtp.target_cpuid_cur];
		int cur_fgpulv = rtp.cur_freqlv[PROFILER_GPU];
		int next_fcpulv = cur_fcpulv;
		int next_fgpulv = cur_fgpulv;
		int id;

		dst->sdp_next_cpuid = rtp.target_cpuid_cur;
		dst->sdp_cur_fcpu = rtp.powertable[rtp.target_cpuid_cur][cur_fcpulv].freq;
		dst->sdp_cur_fgpu = rtp.powertable[PROFILER_GPU][cur_fgpulv].freq;

		if (rtp.shortterm_comb_ctrl >= 0) {
			profiler_stc_get_next_feasible_freqlv(&next_fcpulv, &next_fgpulv);
		} else {
			next_fcpulv = rtp.powertable_size[rtp.target_cpuid_cur] - 1;
			next_fgpulv = rtp.powertable_size[PROFILER_GPU] - 1;
		}

		dst->sdp_next_fcpu = rtp.powertable[rtp.target_cpuid_cur][next_fcpulv].freq;
		dst->sdp_next_fgpu = rtp.powertable[PROFILER_GPU][next_fgpulv].freq;

		if (dst->sdp_next_fgpu > rtp.max_minlock_freq)
			dst->sdp_next_fgpu = rtp.max_minlock_freq;

		/* Set next freq for CPU */
		for (id = PROFILER_CL0; id <= PROFILER_CL2; id++) {
			if (id == rtp.target_cpuid_cur)
				rtp.pmqos_minlock_next[id] = dst->sdp_next_fcpu;
			else
				rtp.pmqos_minlock_next[id] = 0;
		}
		/* Set next freq for GPU */
		rtp.pmqos_minlock_next[PROFILER_GPU] = dst->sdp_next_fgpu;
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

	atomic_set(&rtp.lasthw_read, 1);

	rtp.target_cpuid_cur = profiler_get_current_cpuid();
	rtp.prev_swaptimestamp = end;
	atomic_inc(&rtp.vsync_swapcall_counter);
	pinfo.frame_counter++;

	dst = &interframe[rtp.tail];
	rtp.tail = (rtp.tail + 1) % TABLE_MAX;
	if (rtp.tail == rtp.head)
		rtp.head = (rtp.head + 1) % TABLE_MAX;
	if (rtp.tail == rtp.lastshowidx)
		rtp.lastshowidx = (rtp.lastshowidx + 1) % TABLE_MAX;

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

	if (pid.list_readout != 0) {
		pid.list_top = 0;
		memset(&(pid.list[0]), 0, sizeof(pid.list[0]) * NUM_OF_PID);
		pid.list_readout = 0;
	}
	if (pid.list_top < NUM_OF_PID) {
		u64 mpid = (u64)current->tgid;
		u64 hash = ((mpid >> 16) ^ mpid) & 0xffff;

		if (hash == 0)
			hash = 1;
		pid.list[pid.list_top++] = (u16)hash;
	}

	profiler_stc_control(dst);
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
	rtp.head = (rtp.head + 1) % TABLE_MAX;
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

static void sgpu_profiler_rtp_init(void)
{
	rtp.readout = 0;
	memset(&rtp.powertable[0], 0, sizeof(rtp.powertable[0]) * NUM_OF_DOMAIN);
	memset(&rtp.powertable_size[0], 0, sizeof(rtp.powertable_size[0]) * NUM_OF_DOMAIN);
	rtp.target_cpuid_cur = profiler_get_current_cpuid();
	rtp.shortterm_comb_ctrl = 0;
	rtp.shortterm_comb_ctrl_manual = -1;
	rtp.target_frametime = RTP_DEFAULT_FRAMETIME;
	rtp.targettime_margin = 0; /* us */
	rtp.workload_margin = 100; /* +0% */
	rtp.decon_time = DEADLINE_DECON_INUS;
	rtp.max_minlock_freq = 711000;
}

static void sgpu_profiler_pid_init(void)
{
	pid.list_readout = 0;
	pid.list_top = 0;
	memset(&(pid.list[0]), 0, sizeof(pid.list[0]) * NUM_OF_PID);
}

static void sgpu_profiler_cputracer_init(void)
{
	profiler_set_cputracer_size(12);
}

void sgpu_profiler_init(struct amdgpu_device *adev)
{
	pinfo.cpufreq_pm_qos_added = 0;
	profiler_adev = adev;
	exynos_pm_qos_add_request(&sgpu_profiler_gpu_min_qos,
				  PM_QOS_GPU_THROUGHPUT_MIN, 0);
	exynos_pm_qos_add_request(&sgpu_profiler_gpu_max_qos,
				  PM_QOS_GPU_THROUGHPUT_MAX,
				  PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	sgpu_profiler_rtp_init();
	sgpu_profiler_pid_init();
	sgpu_profiler_cputracer_init();
}

void sgpu_profiler_deinit(void)
{
	exynos_pm_qos_remove_request(&sgpu_profiler_gpu_min_qos);
	exynos_pm_qos_remove_request(&sgpu_profiler_gpu_max_qos);
}
