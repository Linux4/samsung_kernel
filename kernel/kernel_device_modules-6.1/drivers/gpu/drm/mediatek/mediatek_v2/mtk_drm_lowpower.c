// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2021 MediaTek Inc.
*/

#include <linux/fs.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <uapi/linux/sched/types.h>
#include <linux/delay.h>
#include <uapi/drm/mediatek_drm.h>
#include <drm/drm_vblank.h>
#include "mtk_drm_lowpower.h"
#include "mtk_drm_assert.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_trace.h"
#include "mtk_disp_vidle.h"
#include "mtk_dsi.h"

#define MAX_ENTER_IDLE_RSZ_RATIO 300
#define MTK_DRM_CPU_MAX_COUNT 8

static void mtk_drm_idlemgr_get_private_data(struct drm_crtc *crtc,
		struct mtk_idle_private_data *data)
{
	struct mtk_drm_private *priv = NULL;

	if (data == NULL || crtc == NULL)
		return;

	priv = crtc->dev->dev_private;
	if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC)) {
		data->cpu_mask = 0;
		data->cpu_freq = 0;
		data->cpu_dma_latency = PM_QOS_DEFAULT_VALUE;
		data->vblank_async = false;
		data->hw_async = false;
		data->sram_sleep = false;
		return;
	}

	switch (priv->data->mmsys_id) {
	case MMSYS_MT6989:
		data->cpu_mask = 0xf; //cpu0~3
		data->cpu_freq = 1000000; // 1Ghz
		data->cpu_dma_latency = PM_QOS_DEFAULT_VALUE;
		data->vblank_async = false;
		data->hw_async = true;
		data->sram_sleep = false;
		break;
	case MMSYS_MT6985:
		data->cpu_mask = 0x80;     //cpu7
		data->cpu_freq = 0;        // cpu7 default 1.2Ghz
		data->cpu_dma_latency = PM_QOS_DEFAULT_VALUE;
		data->vblank_async = false;
		data->hw_async = true;     // enable gce/ulps async
		data->sram_sleep = false;
		break;
	case MMSYS_MT6897:
		data->cpu_mask = 0xf; //cpu0~3
		data->cpu_freq = 1000000; // 1Ghz
		data->cpu_dma_latency = PM_QOS_DEFAULT_VALUE;
		data->vblank_async = false;
		data->hw_async = true;
		data->sram_sleep = false;
		break;
	case MMSYS_MT6878:
		data->cpu_mask = 0xf; //cpu0~3
		data->cpu_freq = 1000000; // 1Ghz
		data->cpu_dma_latency = PM_QOS_DEFAULT_VALUE;
		data->vblank_async = false;
		data->hw_async = true;
		data->sram_sleep = false;
		break;
	default:
		data->cpu_mask = 0;
		data->cpu_freq = 0;
		data->cpu_dma_latency = PM_QOS_DEFAULT_VALUE;
		data->vblank_async = false;
		data->hw_async = false;
		data->sram_sleep = false;
		break;
	}
}

static void mtk_drm_idlemgr_bind_cpu(struct task_struct *task, struct drm_crtc *crtc, bool bind)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	unsigned int mask = bind ? idlemgr_ctx->priv.cpu_mask : 0xff;
	cpumask_var_t cm;
	int i = 0;

	if (task == NULL || mask == 0)
		return;

	if (!zalloc_cpumask_var(&cm, GFP_KERNEL)) {
		DDPPR_ERR("%s: failed to init cm\n", __func__);
		return;
	}

	for (i = 0; i < MTK_DRM_CPU_MAX_COUNT; i++) {
		if (mask & (0x1 << i))
			cpumask_set_cpu(i, cm);
	}
	for_each_cpu(i, cm)
		DDPMSG("%s, cpu:%u, mask:0x%x\n", __func__, i, mask);

	set_cpus_allowed_ptr(task, cm);
	free_cpumask_var(cm);
}

static int mtk_drm_enhance_cpu_freq(struct freq_qos_request *req,
		unsigned int cpu_id, unsigned int cpu_freq, int *cpu_first, int *cpu_last)
{
	struct cpufreq_policy *policy = NULL;
	int ret = 0;

	if (req == NULL) {
		DDPMSG("%s, invalid req\n", __func__);
		return -EINVAL;
	}

	policy = cpufreq_cpu_get(cpu_id);
	if (policy == NULL) {
		DDPMSG("%s, failed to get cpu %u policy\n", __func__, cpu_id);
		return -EFAULT;
	}

	ret = freq_qos_add_request(&policy->constraints, req, FREQ_QOS_MIN, cpu_freq);
	if (cpu_first != NULL)
		*cpu_first = cpumask_first(policy->related_cpus);
	if (cpu_last != NULL)
		*cpu_last = cpumask_last(policy->related_cpus);
	cpufreq_cpu_put(policy);

	if (ret < 0) {
		DDPMSG("%s, failed to enhance cpu%u freq%u,first:%d,last:%d,ret:%d\n",
			__func__, cpu_id, cpu_freq,  *cpu_first, *cpu_last, ret);
		return ret;
	}

	return 0;
}

void mtk_drm_idlemgr_cpu_control(struct drm_crtc *crtc, int cmd, unsigned int data)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = NULL;
	unsigned int crtc_id = 0;

	if (crtc == NULL)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL)
		return;

	idlemgr = mtk_crtc->idlemgr;
	if (idlemgr == NULL)
		return;

	crtc_id = drm_crtc_index(crtc);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	idlemgr_ctx = idlemgr->idlemgr_ctx;
	switch (cmd) {
	case MTK_DRM_CPU_CMD_FREQ:
		idlemgr_ctx->priv.cpu_freq = data;
		break;
	case MTK_DRM_CPU_CMD_MASK:
		idlemgr_ctx->priv.cpu_mask = data & ((0x1 << MTK_DRM_CPU_MAX_COUNT) - 1);
		mtk_drm_idlemgr_bind_cpu(idlemgr->idlemgr_task, crtc, true);
		mtk_drm_idlemgr_bind_cpu(idlemgr->kick_task, crtc, true);
		break;
	case MTK_DRM_CPU_CMD_LATENCY:
		idlemgr_ctx->priv.cpu_dma_latency = data;
		break;
	default:
		DDPMSG("%s: invalid cmd:%d\n", __func__, cmd);
		break;
	}

	DDPMSG("%s,crtc:%u mask:0x%x, freq:%uMhz, latency:%dus\n", __func__,
		crtc_id, idlemgr_ctx->priv.cpu_mask,
		idlemgr_ctx->priv.cpu_freq / 1000,
		idlemgr_ctx->priv.cpu_dma_latency);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

static unsigned int mtk_drm_get_cpu_data(struct drm_crtc *crtc, unsigned int *cpus)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	unsigned int mask = 0, count = 0, i = 0;

	if (idlemgr_ctx->priv.cpu_mask == 0 ||
		idlemgr_ctx->priv.cpu_freq == 0)
		return 0;

	mask = idlemgr_ctx->priv.cpu_mask;
	while (mask > 0 && i < MTK_DRM_CPU_MAX_COUNT) {
		if ((mask & 0x1) == 0x1) {
			cpus[count] = i;
			count++;
		}
		mask = mask >> 1;
		i++;
	}

	return count;
}

static void mtk_drm_adjust_cpu_latency(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	struct mtk_drm_private *priv = NULL;

	priv = crtc->dev->dev_private;
	if (priv == NULL ||
		!mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_IDLEMGR_ASYNC))
		return;

	if (idlemgr_ctx->priv.cpu_dma_latency == PM_QOS_DEFAULT_VALUE)
		return;

	if (enable)
		cpu_latency_qos_update_request(&idlemgr->cpu_qos_req,
				idlemgr_ctx->priv.cpu_dma_latency);
	else
		cpu_latency_qos_update_request(&idlemgr->cpu_qos_req,
				PM_QOS_DEFAULT_VALUE);
}

static void mtk_drm_idlemgr_perf_reset(struct drm_crtc *crtc);
static void mtk_drm_adjust_cpu_freq(struct drm_crtc *crtc, bool bind,
		struct freq_qos_request **req, unsigned int *cpus, unsigned int count)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	struct mtk_drm_idlemgr_perf *perf = idlemgr->perf;
	unsigned long long start, end, period;
	unsigned int freq = 0;
	static int cpu_first = -1, cpu_last = -1;
	int ret = 0, i = 0, detail = 0;

	if (count == 0 || cpus == NULL ||
		idlemgr_ctx->priv.cpu_freq == 0)
		return;

	if (perf != NULL) {
		start = sched_clock();
		detail = atomic_read(&perf->detail);
		if (detail != 0)
			mtk_drm_trace_begin("adjust_cpu_freq:%d", bind);
	}

	if (bind == true) {
		freq = idlemgr_ctx->priv.cpu_freq;
		for (i = 0; i < count; i++) {
			if ((int)cpus[i] >= cpu_first && (int)cpus[i] <= cpu_last)
				continue;

			req[i] = kzalloc(sizeof(struct freq_qos_request), GFP_KERNEL);
			if (req[i] == NULL) {
				DDPPR_ERR("%s, alloc req:%d failed\n", __func__, i);
				continue;
			}

			ret = mtk_drm_enhance_cpu_freq(req[i], cpus[i], freq, &cpu_first, &cpu_last);
			if (ret < 0) {
				kfree(req[i]);
				req[i] = NULL;
				continue;
			}
		}
		usleep_range(10, 30); //make freq update
	} else {
		for (i = 0; i < count; i++) {
			if (req[i] == NULL)
				continue;
			ret = freq_qos_remove_request(req[i]);
			if (ret < 0)
				DDPMSG("%s, failed to rollback freq, id:%d, ret:%d, cluster:%d~%d\n",
					__func__, i, ret, cpu_first, cpu_last);
			kfree(req[i]);
			req[i] = NULL;
		}
		cpu_first = -1;
		cpu_last = -1;
	}

	if (perf != NULL) {
		if (detail != 0)
			mtk_drm_trace_end();

		end = sched_clock();
		if (bind == true) {
			period = (end - start) / 1000;
			if (perf->cpu_bind_count + 1 == 0 ||
				perf->cpu_total_bind + period < perf->cpu_total_bind)
				mtk_drm_idlemgr_perf_reset(crtc);

			perf->cpu_bind_count++;
			perf->cpu_total_bind += period;
		} else if (perf->cpu_bind_count > 0) {
			period = (end - start) / 1000;
			if (perf->cpu_total_unbind + period < perf->cpu_total_unbind)
				mtk_drm_idlemgr_perf_reset(crtc);
			else
				perf->cpu_total_unbind += period;
		}
	}

	return;
}

void mtk_drm_idlemgr_sram_control(struct drm_crtc *crtc, bool sleep)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = NULL;
	unsigned int crtc_id = 0;

	if (crtc == NULL)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL)
		return;

	idlemgr = mtk_crtc->idlemgr;
	if (idlemgr == NULL)
		return;

	crtc_id = drm_crtc_index(crtc);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	idlemgr_ctx = idlemgr->idlemgr_ctx;
	idlemgr_ctx->priv.sram_sleep = sleep;
	DDPMSG("%s,crtc:%u sram_sleep:%d\n",
		__func__, crtc_id, idlemgr_ctx->priv.sram_sleep);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

void mtk_drm_idlemgr_async_control(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = NULL;
	unsigned int crtc_id = 0;

	if (crtc == NULL)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL)
		return;

	idlemgr = mtk_crtc->idlemgr;
	if (idlemgr == NULL)
		return;

	crtc_id = drm_crtc_index(crtc);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	idlemgr_ctx = idlemgr->idlemgr_ctx;
	idlemgr_ctx->priv.hw_async = enable;
	DDPMSG("%s,crtc:%u hw_async:%d\n",
		__func__, crtc_id, idlemgr_ctx->priv.hw_async);
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

static unsigned int perf_aee_timeout;
void mtk_drm_idlegmr_perf_aee_control(unsigned int timeout)
{
	perf_aee_timeout = timeout;
	DDPMSG("%s: timeout:%ums\n", __func__, perf_aee_timeout);
}

void mtk_drm_idlemgr_async_perf_detail_control(bool enable, struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_idlemgr_perf *perf = NULL;

	if (crtc == NULL)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL)
		return;

	idlemgr = mtk_crtc->idlemgr;
	if (idlemgr == NULL)
		return;

	perf = idlemgr->perf;
	if (perf == NULL) {
		DDPMSG("%s, crtc:%u has not started yet, try \"idle_perf:on\"\n",
			__func__, drm_crtc_index(crtc));
		return;
	}

	if (enable)
		atomic_set(&perf->detail, 1);
	else
		atomic_set(&perf->detail, 0);
	DDPMSG("%s, crtc%u, idle perf detail debug:%d\n",
		__func__, drm_crtc_index(crtc), atomic_read(&perf->detail));
}

void mtk_drm_idlemgr_perf_dump_func(struct drm_crtc *crtc, bool lock)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_idlemgr_perf *perf = NULL;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = NULL;
	unsigned int crtc_id = 0;

	if (crtc == NULL)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL)
		return;

	idlemgr = mtk_crtc->idlemgr;
	if (idlemgr == NULL)
		return;

	crtc_id = drm_crtc_index(crtc);
	if (lock)
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	perf = idlemgr->perf;
	if (perf == NULL) {
		DDPMSG("%s, crtc:%u has not started yet, try \"idle_perf:on\"\n",
			__func__, crtc_id);
		if (lock)
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		return;
	}

	if (perf->count > 0) {
		idlemgr_ctx = idlemgr->idlemgr_ctx;
		DDPMSG(
			"%s:crtc:%u,async:%d/%d,sram_sleep:%d,cpu:(0x%x,%uMhz,%dus),count:%llu,max:%llu-%llu,min:%llu-%llu,avg:%llu-%llu,cpu_avg:%llu-%llu,cpu_cnt:%llu\n",
			__func__, crtc_id, idlemgr_ctx->priv.hw_async,
			idlemgr_ctx->priv.vblank_async,
			idlemgr_ctx->priv.sram_sleep,
			idlemgr_ctx->priv.cpu_mask,
			idlemgr_ctx->priv.cpu_freq / 1000,
			idlemgr_ctx->priv.cpu_dma_latency, perf->count,
			perf->enter_max_cost, perf->leave_max_cost,
			perf->enter_min_cost, perf->leave_min_cost,
			perf->enter_total_cost/perf->count,
			perf->leave_total_cost/perf->count,
			perf->cpu_total_bind/perf->cpu_bind_count,
			perf->cpu_total_unbind/perf->cpu_bind_count,
			perf->cpu_bind_count);
	} else {
		DDPMSG("%s: crtc:%u, perf monitor is started w/o update\n",
				__func__, crtc_id);
	}

	if (lock)
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

void mtk_drm_idlemgr_perf_dump(struct drm_crtc *crtc)
{
	mtk_drm_idlemgr_perf_dump_func(crtc, true);
}

static void mtk_drm_idlemgr_perf_reset(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	struct mtk_drm_idlemgr_perf *perf = idlemgr->perf;
	unsigned int crtc_id;

	if (perf == NULL)
		return;

	if (perf->count > 0) {
		crtc_id = drm_crtc_index(crtc);
		DDPMSG(
			"%s:crtc:%u,async:%d/%d,sram_sleep:%d,cpu:(0x%x,%uMhz),count:%llu,max:%llu-%llu,min:%llu-%llu,avg:%llu-%llu,cpu_avg:%llu-%llu,cpu_cnt:%llu\n",
			__func__, crtc_id, idlemgr_ctx->priv.hw_async,
			idlemgr_ctx->priv.vblank_async,
			idlemgr_ctx->priv.sram_sleep,
			idlemgr_ctx->priv.cpu_mask,
			idlemgr_ctx->priv.cpu_freq / 1000, perf->count,
			perf->enter_max_cost, perf->leave_max_cost,
			perf->enter_min_cost, perf->leave_min_cost,
			perf->enter_total_cost/perf->count,
			perf->leave_total_cost/perf->count,
			perf->cpu_total_bind/perf->cpu_bind_count,
			perf->cpu_total_unbind/perf->cpu_bind_count,
			perf->cpu_bind_count);
	}
	perf->enter_max_cost = 0;
	perf->enter_min_cost = 0xffffffff;
	perf->enter_total_cost = 0;
	perf->leave_max_cost = 0;
	perf->leave_min_cost = 0xffffffff;
	perf->leave_total_cost = 0;
	perf->count = 0;
	perf->cpu_total_bind = 0;
	perf->cpu_bind_count = 0;
	perf->cpu_total_unbind = 0;
}

static void mtk_drm_idlemgr_perf_update(struct drm_crtc *crtc,
	bool enter, unsigned long long cost)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_perf *perf = idlemgr->perf;

	if (perf == NULL)
		return;

	if (enter == true) {
		if (perf->count + 1 == 0 ||
			perf->enter_total_cost + cost < perf->enter_total_cost)
			mtk_drm_idlemgr_perf_reset(crtc);

		perf->count++;
		if (perf->enter_max_cost < cost)
			perf->enter_max_cost = cost;
		if (perf->enter_min_cost > cost)
			perf->enter_min_cost = cost;
		perf->enter_total_cost += cost;
	} else if (perf->count > 0) {
		if (perf->leave_total_cost + cost < perf->leave_total_cost)
			mtk_drm_idlemgr_perf_reset(crtc);
		else {
			if (perf->leave_max_cost < cost)
				perf->leave_max_cost = cost;
			if (perf->leave_min_cost > cost)
				perf->leave_min_cost = cost;
			perf->leave_total_cost += cost;

			if (perf->count % 50 == 0)
				mtk_drm_idlemgr_perf_dump_func(crtc, false);
		}
	}
}

#define MTK_IDLE_PERF_DETAIL_DEBUG
#define MTK_IDLE_PERF_DETAIL_LENGTH  1024
void mtk_drm_idlemgr_perf_detail_check(bool detail, struct drm_crtc *crtc,
	char *next_event, int next_id, char *perf_string, bool kick)
{
#ifdef MTK_IDLE_PERF_DETAIL_DEBUG
	static unsigned long long tl, tc;
	char label[128] = { 0 };
	static char last_event[128] = { 0 };
	bool async = false;
	int len = 0;

	if (next_event == NULL) {
		DDPMSG("%s: invalid event\n", __func__);
		return;
	}

	//mmprofile dump of next event
	async = mtk_drm_idlemgr_get_async_status(crtc);
	if (async == true && next_id >= 0) {
		if (kick == true)
			CRTC_MMP_MARK((int)drm_crtc_index(crtc),
					leave_idle, detail, next_id);
		else
			CRTC_MMP_MARK((int)drm_crtc_index(crtc),
					enter_idle, detail, next_id);
	}

	if (detail == false)
		return;

	/* check and init of the first event*/
	if (tl == 0 ||
		!strncmp(next_event, "START", 5)) {
		tl = sched_clock();
		mtk_drm_trace_begin("start");
		memset(last_event, 0x0, sizeof(last_event));
		strncpy(last_event, "start", sizeof(last_event));
		return;
	}

	/* stop systrace dump of last event*/
	mtk_drm_trace_end();

	/* record performance data of last event*/
	tc = sched_clock();
	len = snprintf(label, 128, "[%s]:%lluus,", last_event,
			(tc - tl) / 1000);
	if (len > 0 &&
		MTK_IDLE_PERF_DETAIL_LENGTH - len > strlen(perf_string))
		strncat(perf_string, label, (sizeof(perf_string) - len - 1));

	/* update next event*/
	tl = tc;
	len = snprintf(last_event, sizeof(last_event),
			"%s-%d", next_event, next_id);
	if (len < 0) {
		DDPMSG("%s: snprintf failed, %d\n", __func__, len);
		last_event[0] = '\0';
	}

	/* start systrace dump of next event */
	if (strncmp(next_event, "STOP", 4)) {
		mtk_drm_trace_begin("%s", next_event);
		return;
	}
#endif
}

void mtk_drm_idlemgr_monitor(bool enable, struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_idlemgr *idlemgr = NULL;
	unsigned int crtc_id = 0;

	if (crtc == NULL)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc == NULL)
		return;

	idlemgr = mtk_crtc->idlemgr;
	if (idlemgr == NULL)
		return;

	crtc_id = drm_crtc_index(crtc);
	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
	if (enable == true) {
		if (idlemgr->perf == NULL) {
			idlemgr->perf = kzalloc(sizeof(struct mtk_drm_idlemgr_perf), GFP_KERNEL);
			if (idlemgr->perf == NULL) {
				DDPPR_ERR("%s, failed to allocate perf\n", __func__);
				goto out;
			}
		}
		mtk_drm_idlemgr_perf_reset(crtc);
		DDPMSG("%s: crtc:%u start idle perf monitor done\n", __func__, crtc_id);
	} else {
		if (idlemgr->perf == NULL) {
			DDPMSG("%s, crtc:%u already stopped idle perf monitor\n",
				__func__, crtc_id);
			goto out;
		}
		kfree(idlemgr->perf);
		idlemgr->perf = NULL;
		DDPMSG("%s: crtc%u stop idle perf monitor done\n", __func__, crtc_id);
	}

out:
	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

static void mtk_drm_idlemgr_enable_crtc(struct drm_crtc *crtc);
static void mtk_drm_idlemgr_disable_crtc(struct drm_crtc *crtc);


static void mtk_drm_vidle_control(struct drm_crtc *crtc, bool enable)
{
	struct mtk_drm_private *priv = NULL;
	static bool vidle_status;

	if (crtc == NULL || crtc->dev == NULL)
		return;

	priv = crtc->dev->dev_private;
	if (priv== NULL)
		return;

	if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_VDO_PANEL) ||
		!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_VIDLE_HOME_SCREEN_IDLE))
		return;

	DDPINFO("%s, enable:%d, vidle:%d, ff:%d\n", __func__, enable, vidle_status, mtk_vidle_is_ff_enabled());
	if (enable && !mtk_vidle_is_ff_enabled() && !vidle_status) {
		CRTC_MMP_MARK((int)drm_crtc_index(crtc), enter_vidle, 0x1d1e, enable);
		mtk_vidle_enable(true, priv);
		mtk_vidle_force_enable_mml(true);
		mtk_vidle_config_ff(true);
		mtk_vidle_force_enable_mml(false);
		vidle_status = true;
	} else if (!enable && vidle_status) {
		CRTC_MMP_MARK((int)drm_crtc_index(crtc), leave_vidle, 0x1d1e, enable);
		mtk_vidle_force_enable_mml(true);
		mtk_vidle_config_ff(false);
		mtk_vidle_enable(false, priv);
		vidle_status = false;
	}
}

static void mtk_drm_vdo_mode_enter_idle(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_state *state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i, j;
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	struct mtk_ddp_comp *comp;

	mtk_crtc_pkt_create(&handle, crtc, client);

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_IDLEMGR_BY_REPAINT) &&
	    atomic_read(&state->plane_enabled_num) > 1) {
		atomic_set(&priv->idle_need_repaint, 1);
		drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, crtc->dev);
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_IDLEMGR_BY_WB) &&
	    !mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_IDLEMGR_BY_REPAINT)) {
		if (atomic_read(&state->plane_enabled_num) > 1 &&
		    !mtk_drm_idlemgr_wb_is_entered(mtk_crtc) &&
		    !mtk_drm_dal_enable() &&
		    !msync_is_on(priv, mtk_crtc->panel_ext->params,
				 drm_crtc_index(crtc), state, state)) {
			mtk_drm_idlemgr_wb_enter(mtk_crtc, NULL);
		}
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
		mtk_disp_mutex_inten_disable_cmdq(mtk_crtc->mutex[0], handle);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, handle, IRQ_LEVEL_IDLE, NULL);
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp) {
		int en = 0;
		mtk_ddp_comp_io_cmd(comp, handle, DSI_VFP_IDLE_MODE, NULL);
		mtk_ddp_comp_io_cmd(comp, handle, DSI_LFR_SET, &en);
		mtk_drm_vidle_control(crtc, true);
	}

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
}

static void mtk_drm_cmd_mode_enter_idle(struct drm_crtc *crtc)
{
	struct freq_qos_request *req[MTK_DRM_CPU_MAX_COUNT] = {0};
	unsigned int cpus[MTK_DRM_CPU_MAX_COUNT] = { 0 };
	unsigned int count = 0;

	mtk_drm_adjust_cpu_latency(crtc, true);
	count = mtk_drm_get_cpu_data(crtc, cpus);
	if (count > 0)
		mtk_drm_adjust_cpu_freq(crtc, true, req, cpus, count);

	mtk_drm_idlemgr_disable_crtc(crtc);
	lcm_fps_ctx_reset(crtc);

	mtk_drm_adjust_cpu_latency(crtc, false);
	if (count > 0)
		mtk_drm_adjust_cpu_freq(crtc, false, req, cpus, count);
}

static void mtk_drm_vdo_mode_leave_idle(struct drm_crtc *crtc)
{
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i, j;
	struct cmdq_pkt *handle;
	struct cmdq_client *client = mtk_crtc->gce_obj.client[CLIENT_CFG];
	struct mtk_ddp_comp *comp;

	mtk_crtc_pkt_create(&handle, crtc, client);

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				   MTK_DRM_OPT_IDLEMGR_DISABLE_ROUTINE_IRQ)) {
		mtk_disp_mutex_inten_enable_cmdq(mtk_crtc->mutex[0], handle);
		for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
			mtk_ddp_comp_io_cmd(comp, handle, IRQ_LEVEL_NORMAL, NULL);
	}

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (comp) {
		int en = 1;

		mtk_drm_vidle_control(crtc, false);
		mtk_ddp_comp_io_cmd(comp, handle, DSI_VFP_DEFAULT_MODE, NULL);
		mtk_ddp_comp_io_cmd(comp, handle, DSI_LFR_SET, &en);
	}

	cmdq_pkt_flush(handle);
	cmdq_pkt_destroy(handle);
}

static void mtk_drm_cmd_mode_leave_idle(struct drm_crtc *crtc)
{
	struct freq_qos_request *req[MTK_DRM_CPU_MAX_COUNT] = {0};
	unsigned int cpus[MTK_DRM_CPU_MAX_COUNT] = { 0 };
	unsigned int count = 0;

	mtk_drm_adjust_cpu_latency(crtc, true);
	count = mtk_drm_get_cpu_data(crtc, cpus);
	if (count > 0)
		mtk_drm_adjust_cpu_freq(crtc, true, req, cpus, count);

	mtk_drm_idlemgr_enable_crtc(crtc);
	lcm_fps_ctx_reset(crtc);

	mtk_drm_adjust_cpu_latency(crtc, false);
	if (count > 0)
		mtk_drm_adjust_cpu_freq(crtc, false, req, cpus, count);
}

static void mtk_drm_idlemgr_enter_idle_nolock(struct drm_crtc *crtc)
{
	struct mtk_ddp_comp *output_comp;
	int index = drm_crtc_index(crtc);
	unsigned int idle_interval;
	bool mode;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!output_comp)
		return;

	mode = mtk_dsi_is_cmd_mode(output_comp);
	idle_interval = mtk_drm_get_idle_check_interval(crtc);
	CRTC_MMP_EVENT_START(index, enter_idle, mode, idle_interval);

	if (mode)
		mtk_drm_cmd_mode_enter_idle(crtc);
	else
		mtk_drm_vdo_mode_enter_idle(crtc);

	CRTC_MMP_EVENT_END(index, enter_idle, mode, idle_interval);
}

static void mtk_drm_idlemgr_leave_idle_nolock(struct drm_crtc *crtc)
{
	struct mtk_ddp_comp *output_comp;
	int index = drm_crtc_index(crtc);
	bool mode;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);

	if (!output_comp)
		return;

	mode = mtk_dsi_is_cmd_mode(output_comp);
	CRTC_MMP_EVENT_START(index, leave_idle, mode, 0);
	drm_trace_tag_start("Kick idle");


	if (mode)
		mtk_drm_cmd_mode_leave_idle(crtc);
	else
		mtk_drm_vdo_mode_leave_idle(crtc);

	CRTC_MMP_EVENT_END(index, leave_idle, mode, 0);
	drm_trace_tag_end("Kick idle");
}

bool mtk_drm_is_idle(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;

	if (!idlemgr)
		return false;

	return idlemgr->idlemgr_ctx->is_idle;
}

bool mtk_drm_idlemgr_get_async_status(struct drm_crtc *crtc)
{
	struct mtk_drm_idlemgr *idlemgr;
	struct mtk_drm_private *priv = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	if (crtc == NULL)
		return false;

	priv = crtc->dev->dev_private;
	if (priv == NULL ||
		!mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_IDLEMGR_ASYNC))
		return false;

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc && mtk_crtc->idlemgr)
		idlemgr = mtk_crtc->idlemgr;
	else
		return false;

	if (atomic_read(&idlemgr->async_enabled) != 0)
		return true;

	return false;
}

bool mtk_drm_idlemgr_get_sram_status(struct drm_crtc *crtc)
{
	struct mtk_drm_idlemgr *idlemgr;
	struct mtk_drm_private *priv = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_idlemgr_context *idlemgr_ctx;

	if (crtc == NULL)
		return false;

	priv = crtc->dev->dev_private;
	if (priv == NULL ||
		!mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_IDLEMGR_ASYNC))
		return false;

	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc && mtk_crtc->idlemgr)
		idlemgr = mtk_crtc->idlemgr;
	else
		return false;

	idlemgr_ctx = idlemgr->idlemgr_ctx;
	if (atomic_read(&idlemgr->async_enabled) != 0)
		return idlemgr_ctx->priv.sram_sleep;

	return false;
}

void mtk_drm_idlemgr_async_get(struct drm_crtc *crtc, unsigned int user_id)
{
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	if (mtk_drm_idlemgr_get_async_status(crtc) == false)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	idlemgr = mtk_crtc->idlemgr;
	atomic_inc(&idlemgr->async_ref);
	CRTC_MMP_MARK((int)drm_crtc_index(crtc), idle_async,
		user_id, atomic_read(&idlemgr->async_ref));

	DDPINFO("%s, active:%d count:%d user:0x%x\n", __func__,
		atomic_read(&idlemgr->async_enabled),
		atomic_read(&idlemgr->async_ref), user_id);
}

// gce irq handler will do async put to let idle task go
void mtk_drm_idlemgr_async_put(struct drm_crtc *crtc, unsigned int user_id)
{
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;

	if (mtk_drm_idlemgr_get_async_status(crtc) == false)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	idlemgr = mtk_crtc->idlemgr;
	if (atomic_read(&idlemgr->async_ref) == 0) {
		DDPMSG("%s: invalid put w/o get\n", __func__);
		return;
	}

	if (atomic_dec_return(&idlemgr->async_ref) == 0)
		wake_up_interruptible(&idlemgr->async_event_wq);

	CRTC_MMP_MARK((int)drm_crtc_index(crtc), idle_async,
		user_id, atomic_read(&idlemgr->async_ref));

	DDPINFO("%s, active:%d count:%d user:0x%x\n", __func__,
		atomic_read(&idlemgr->async_enabled),
		atomic_read(&idlemgr->async_ref), user_id);
}

// cmdq pkt is wait and destroyed in the async handler thread
void mtk_drm_idlemgr_async_complete(struct drm_crtc *crtc, unsigned int user_id,
	struct mtk_drm_async_cb_data *cb_data)
{
	struct mtk_drm_async_cb *cb = NULL;
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned long flags = 0;

	if (mtk_drm_idlemgr_get_async_status(crtc) == false)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	idlemgr = mtk_crtc->idlemgr;
	if (cb_data != NULL) {
		cb = kmalloc(sizeof(struct mtk_drm_async_cb), GFP_KERNEL);
		if (cb == NULL) {
			DDPPR_ERR("%s, failed to allocate cb node\n", __func__);
			return;
		}

		cb->data = cb_data;
		spin_lock_irqsave(&idlemgr->async_lock, flags);
		list_add_tail(&cb->link, &idlemgr->async_cb_list);
		atomic_inc(&idlemgr->async_cb_count);
		if (cb_data->free_handle == false)
			atomic_inc(&idlemgr->async_cb_pending);
		CRTC_MMP_MARK((int)drm_crtc_index(crtc), idle_async_cb,
			(unsigned long)cb_data->handle,
			atomic_read(&idlemgr->async_cb_count) | (user_id << 16));
		spin_unlock_irqrestore(&idlemgr->async_lock, flags);
		wake_up_interruptible(&idlemgr->async_handler_wq);
	}
}

void mtk_drm_idle_async_cb(struct cmdq_cb_data data)
{
	struct mtk_drm_async_cb_data *cb_data = data.data;
	struct drm_crtc *crtc = NULL;

	if (cb_data == NULL) {
		DDPMSG("%s: invalid cb data\n", __func__);
		return;
	}

	crtc = cb_data->crtc;
	mtk_drm_idlemgr_async_put(crtc, cb_data->user_id);
}

int mtk_drm_idle_async_flush_func(struct drm_crtc *crtc,
	unsigned int user_id, struct cmdq_pkt *cmdq_handle,
	bool free_handle, cmdq_async_flush_cb cb)
{
	struct mtk_drm_async_cb_data *cb_data = NULL;
	bool async = mtk_drm_idlemgr_get_async_status(crtc);

	if (async == false)
		return -EFAULT;

	cb_data = kmalloc(sizeof(struct mtk_drm_async_cb_data), GFP_KERNEL);
	if (cb_data == NULL)
		return -ENOMEM;

	cb_data->crtc = crtc;
	cb_data->handle = cmdq_handle;
	cb_data->free_handle = free_handle;
	cb_data->user_id = user_id;
	mtk_drm_idlemgr_async_get(crtc, user_id);
	cmdq_pkt_flush_async(cmdq_handle, cb, cb_data);
	mtk_drm_idlemgr_async_complete(crtc, user_id, cb_data);

	return 0;
}

/*free cmdq handle after job done*/
int mtk_drm_idle_async_flush(struct drm_crtc *crtc,
	unsigned int user_id, struct cmdq_pkt *cmdq_handle)
{
	int ret = 0;

	if (cmdq_handle == NULL || crtc == NULL)
		return -EFAULT;

	ret = mtk_drm_idle_async_flush_func(crtc, user_id,
				cmdq_handle, true, mtk_drm_idle_async_cb);
	if (ret < 0) {
		cmdq_pkt_flush(cmdq_handle);
		cmdq_pkt_destroy(cmdq_handle);
	}
	return ret;
}

int mtk_drm_idle_async_flush_cust(struct drm_crtc *crtc,
	unsigned int user_id, struct cmdq_pkt *cmdq_handle,
	bool free_handle, cmdq_async_flush_cb cb)
{
	if (cmdq_handle == NULL || crtc == NULL)
		return -EFAULT;

	if (cb == NULL)
		cb = mtk_drm_idle_async_cb;

	return mtk_drm_idle_async_flush_func(crtc, user_id,
				cmdq_handle, free_handle, cb);
}

static void mtk_drm_idle_async_wait(struct drm_crtc *crtc,
	unsigned int delay, char *name)
{
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_idlemgr_context *idlemgr_ctx;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned long jiffies = msecs_to_jiffies(1200);
	long ret = 0;

	if (mtk_drm_idlemgr_get_async_status(crtc) == false)
		return;

	mtk_crtc = to_mtk_crtc(crtc);
	idlemgr = mtk_crtc->idlemgr;

	if (atomic_read(&idlemgr->async_ref) == 0)
		return;

	//avoid of cpu schedule out by waiting last gce job done
	idlemgr_ctx = idlemgr->idlemgr_ctx;
	if (delay > 0 && (idlemgr_ctx == NULL ||
		idlemgr_ctx->priv.cpu_dma_latency != PM_QOS_DEFAULT_VALUE))
		udelay(delay);

	//wait gce job done by cpu schedule out
	ret = wait_event_interruptible_timeout(idlemgr->async_event_wq,
					 !atomic_read(&idlemgr->async_ref), jiffies);
	if (ret <= 0 && atomic_read(&idlemgr->async_ref) > 0) {
		DDPPR_ERR("%s, timeout, ret:%ld, clear ref:%u\n",
			__func__, ret, atomic_read(&idlemgr->async_ref));
		atomic_set(&idlemgr->async_ref, 0);
	}
}

void mtk_drm_idlemgr_kick_async(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_idlemgr *idlemgr;

	if (crtc)
		mtk_crtc = to_mtk_crtc(crtc);

	if (mtk_crtc && mtk_crtc->idlemgr)
		idlemgr = mtk_crtc->idlemgr;
	else
		return;

	atomic_set(&idlemgr->kick_task_active, 1);
	wake_up_interruptible(&idlemgr->kick_wq);
}

void mtk_drm_idlemgr_kick(const char *source, struct drm_crtc *crtc,
			  int need_lock)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	if (!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_IDLE_MGR))
		return;

	if (!mtk_crtc->idlemgr)
		return;
	idlemgr = mtk_crtc->idlemgr;
	idlemgr_ctx = idlemgr->idlemgr_ctx;

	/* get lock to protect idlemgr_last_kick_time and is_idle */
	if (need_lock)
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	if (idlemgr_ctx->is_idle) {
		DDPINFO("[LP] kick idle from [%s] time[%llu]\n", source,
			local_clock() - idlemgr_ctx->enter_idle_ts);
		if (mtk_crtc->esd_ctx)
			atomic_set(&mtk_crtc->esd_ctx->target_time, 0);
		mtk_drm_idlemgr_leave_idle_nolock(crtc);
		idlemgr_ctx->is_idle = 0;
		/* wake up idlemgr process to monitor next idle state */
		wake_up_interruptible(&idlemgr->idlemgr_wq);
	}

	/* update kick timestamp */
	idlemgr_ctx->idlemgr_last_kick_time = sched_clock();

	if (need_lock)
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
}

unsigned int mtk_drm_set_idlemgr(struct drm_crtc *crtc, unsigned int flag,
				 bool need_lock)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	unsigned int old_flag;

	if (!idlemgr)
		return 0;

	old_flag = atomic_read(&idlemgr->idlemgr_task_active);

	if (flag) {
		DDPINFO("[LP] enable idlemgr\n");
		atomic_set(&idlemgr->idlemgr_task_active, 1);
		wake_up_interruptible(&idlemgr->idlemgr_wq);
	} else {
		DDPINFO("[LP] disable idlemgr\n");
		atomic_set(&idlemgr->idlemgr_task_active, 0);
		mtk_drm_idlemgr_kick(__func__, crtc, need_lock);
	}

	return old_flag;
}

unsigned long long
mtk_drm_set_idle_check_interval(struct drm_crtc *crtc,
				unsigned long long new_interval)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned long long old_interval = 0;

	if (!(mtk_crtc && mtk_crtc->idlemgr && mtk_crtc->idlemgr->idlemgr_ctx))
		return 0;

	old_interval = mtk_crtc->idlemgr->idlemgr_ctx->idle_check_interval;
	mtk_crtc->idlemgr->idlemgr_ctx->idle_check_interval = new_interval;

	return old_interval;
}

unsigned long long
mtk_drm_get_idle_check_interval(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);

	if (!(mtk_crtc && mtk_crtc->idlemgr && mtk_crtc->idlemgr->idlemgr_ctx))
		return 0;

	return mtk_crtc->idlemgr->idlemgr_ctx->idle_check_interval;
}

static int mtk_drm_idlemgr_get_rsz_ratio(struct mtk_crtc_state *state)
{
	int src_w = state->rsz_src_roi.width;
	int src_h = state->rsz_src_roi.height;
	int dst_w = state->rsz_dst_roi.width;
	int dst_h = state->rsz_dst_roi.height;
	int ratio_w, ratio_h;

	if (src_w == 0 || src_h == 0)
		return 100;

	ratio_w = dst_w * 100 / src_w;
	ratio_h = dst_h * 100 / src_h;

	return ((ratio_w > ratio_h) ? ratio_w : ratio_h);
}

static bool is_yuv(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		return true;
	default:
		break;
	}

	return false;
}

static bool mtk_planes_is_yuv_fmt(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	int i;

	for (i = 0; i < mtk_crtc->layer_nr; i++) {
		struct drm_plane *plane = &mtk_crtc->planes[i].base;
		struct mtk_plane_state *plane_state =
			to_mtk_plane_state(plane->state);
		struct mtk_plane_pending_state *pending = &plane_state->pending;
		unsigned int fmt = pending->format;

		if (pending->enable && is_yuv(fmt))
			return true;

		if (plane_state->comp_state.layer_caps & MTK_DISP_SRC_YUV_LAYER)
			return true;
	}

	return false;
}

static void mtk_drm_destroy_async_cb(struct mtk_drm_async_cb *cb,
		struct drm_crtc *crtc, bool wait)
{
	struct mtk_drm_async_cb_data *cb_data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	unsigned long flags = 0;

	if (cb == NULL) {
		DDPMSG("%s, invalid async callback data\n", __func__);
		return;
	}
	cb_data = cb->data;

	if (cb_data != NULL) {
		DDPINFO("%s,async complete user:0x%x, cb_count:%d, wait:%d\n", __func__,
			cb_data->user_id, atomic_read(&idlemgr->async_cb_count), wait);
		if (wait)
			cmdq_pkt_wait_complete(cb_data->handle);

		spin_lock_irqsave(&idlemgr->async_lock, flags);
		list_del(&cb->link);
		atomic_dec(&idlemgr->async_cb_count);
		CRTC_MMP_MARK((int)drm_crtc_index(crtc), idle_async_cb,
			(unsigned long)cb_data->handle,
			atomic_read(&idlemgr->async_cb_count) | (cb_data->user_id << 16));
		spin_unlock_irqrestore(&idlemgr->async_lock, flags);

		if (cb_data->free_handle)
			cmdq_pkt_destroy(cb_data->handle);
		else
			atomic_dec(&idlemgr->async_cb_pending);
		cb_data->handle = NULL;
		kfree(cb_data);
	}

	cb->data = NULL;
	kfree(cb);
}

void mtk_drm_clear_async_cb_list(struct drm_crtc *crtc)
{
	struct sched_param hi_param = {.sched_priority = 87 };
	struct sched_param lo_param = {.sched_priority = 0 };
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_private *priv = NULL;
	bool adjusted = false;

	priv = crtc->dev->dev_private;
	if (priv == NULL || idlemgr == NULL ||
		!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC))
		return;

	while (atomic_read(&idlemgr->async_cb_pending) > 0) {
		if (adjusted == false) {
			sched_setscheduler(idlemgr->async_handler_task,
					SCHED_RR, &hi_param);
			adjusted = true;
			CRTC_MMP_MARK((int)drm_crtc_index(crtc), idle_async_cb,
				0xffffffff, atomic_read(&idlemgr->async_cb_pending));
			DDPINFO("%s: polling async count:%d, pending:%d\n",
				__func__, atomic_read(&idlemgr->async_cb_count),
				atomic_read(&idlemgr->async_cb_pending));
		}
		usleep_range(50, 100);
	}
	if (adjusted == true) {
		sched_setscheduler(idlemgr->async_handler_task,
				SCHED_NORMAL, &lo_param);
		CRTC_MMP_MARK((int)drm_crtc_index(crtc), idle_async_cb,
			0xffffffff, atomic_read(&idlemgr->async_cb_pending));
		DDPINFO("%s: async count:%d, pending:%d\n",
			__func__, atomic_read(&idlemgr->async_cb_count),
			atomic_read(&idlemgr->async_cb_pending));
	}
}

static int mtk_drm_async_handler_thread(void *data)
{
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_async_cb *cb = NULL;
	unsigned long flags = 0;
	int ret = 0;

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(idlemgr->async_handler_wq,
			atomic_read(&idlemgr->async_cb_count) > 0 || kthread_should_stop());

		spin_lock_irqsave(&idlemgr->async_lock, flags);
		if (list_empty(&idlemgr->async_cb_list)) {
			spin_unlock_irqrestore(&idlemgr->async_lock, flags);
			DDPPR_ERR("%s: async list is empty\n", __func__);
			break;
		}
		cb = list_first_entry(&idlemgr->async_cb_list, struct mtk_drm_async_cb, link);
		spin_unlock_irqrestore(&idlemgr->async_lock, flags);

		mtk_drm_destroy_async_cb(cb, crtc, true);
		cb = NULL;
	}

	return 0;
}

static int mtk_drm_async_vblank_thread(void *data)
{
	struct sched_param param = {.sched_priority = 87 };
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	int ret = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(
			idlemgr->async_vblank_wq,
			atomic_read(&idlemgr->async_vblank_active));

		atomic_set(&idlemgr->async_vblank_active, 0);
		drm_crtc_vblank_off(crtc);
		mtk_crtc_vblank_irq(&mtk_crtc->base);
		mtk_drm_idlemgr_async_put(crtc, USER_VBLANK_OFF);
	}

	return 0;
}

static int mtk_drm_async_kick_idlemgr_thread(void *data)
{
	struct sched_param param = {.sched_priority = 87 };
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	int ret = 0;

	sched_setscheduler(current, SCHED_RR, &param);

	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(
			idlemgr->kick_wq,
			atomic_read(&idlemgr->kick_task_active));

		atomic_set(&idlemgr->kick_task_active, 0);
		mtk_drm_idlemgr_kick(__func__, crtc, true);
	}

	return 0;
}

static int mtk_drm_idlemgr_monitor_thread(void *data)
{
	int ret = 0;
	unsigned long long t_idle;
	struct drm_crtc *crtc = (struct drm_crtc *)data;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_crtc_state *mtk_state = NULL;
	struct drm_vblank_crtc *vblank = NULL;
	int crtc_id = drm_crtc_index(crtc);

	msleep(16000);
	while (1) {
		ret = wait_event_interruptible(
			idlemgr->idlemgr_wq,
			atomic_read(&idlemgr->idlemgr_task_active));

		msleep_interruptible(idlemgr_ctx->idle_check_interval);

		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

		if (!mtk_crtc->enabled) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			mtk_crtc_wait_status(crtc, 1, MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		if (mtk_crtc_is_frame_trigger_mode(crtc) &&
				atomic_read(&priv->crtc_rel_present[crtc_id]) <
				atomic_read(&priv->crtc_present[crtc_id])) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			continue;
		}

		if (crtc->state) {
			mtk_state = to_mtk_crtc_state(crtc->state);
			if (mtk_state->prop_val[CRTC_PROP_DOZE_ACTIVE]) {
				DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__,
						__LINE__);
				continue;
			}
			/* do not enter VDO idle when rsz ratio >= 2.5;
			 * And When layer fmt is YUV in VP scenario, it
			 * will flicker into idle repaint, so let it not
			 * into idle repaint as workaround.
			 */
			if (mtk_crtc_is_frame_trigger_mode(crtc) == 0 &&
				((mtk_drm_idlemgr_get_rsz_ratio(mtk_state) >=
				MAX_ENTER_IDLE_RSZ_RATIO) ||
				mtk_planes_is_yuv_fmt(crtc))) {
				DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__,
						__LINE__);
				continue;
			}
		}

		if (idlemgr_ctx->is_idle
			|| mtk_crtc_is_dc_mode(crtc)
			|| mtk_crtc->sec_on
			|| !mtk_crtc->already_first_config
			|| !(priv->usage[crtc_id] == DISP_ENABLE)) {
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			continue;
		}

		t_idle = local_clock() - idlemgr_ctx->idlemgr_last_kick_time;
		if (t_idle < idlemgr_ctx->idle_check_interval * 1000 * 1000) {
			/* kicked in idle_check_interval msec, it's not idle */
			DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
			continue;
		}
		/* double check if dynamic switch on/off */
		if (atomic_read(&idlemgr->idlemgr_task_active)) {
			crtc_id = drm_crtc_index(crtc);
			vblank = &crtc->dev->vblank[crtc_id];

			/* enter idle state */
			if (!vblank || atomic_read(&vblank->refcount) == 0) {
				DDPINFO("[LP] enter idle\n");
				mtk_drm_idlemgr_enter_idle_nolock(crtc);
				idlemgr_ctx->is_idle = 1;
				idlemgr_ctx->enter_idle_ts = local_clock();
			} else {
				idlemgr_ctx->idlemgr_last_kick_time =
					sched_clock();
			}
		}

		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		wait_event_interruptible(idlemgr->idlemgr_wq,
					 !idlemgr_ctx->is_idle);

		if (kthread_should_stop())
			break;
	}

	return 0;
}

int mtk_drm_idlemgr_init(struct drm_crtc *crtc, int index)
{
#define LEN 50
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_idlemgr *idlemgr = NULL;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = NULL;
	struct mtk_drm_private *priv =
				mtk_crtc->base.dev->dev_private;
	struct mtk_ddp_comp *output_comp = NULL;
	bool mode = false;
	char name[LEN] = {0};

	idlemgr = kzalloc(sizeof(*idlemgr), GFP_KERNEL);
	idlemgr_ctx = kzalloc(sizeof(*idlemgr_ctx), GFP_KERNEL);

	if (!idlemgr || !idlemgr_ctx) {
		DDPPR_ERR("idlemgr or idlemgr_ctx allocate fail\n");
		kfree(idlemgr);
		kfree(idlemgr_ctx);
		return -ENOMEM;
	}

	idlemgr->idlemgr_ctx = idlemgr_ctx;
	mtk_crtc->idlemgr = idlemgr;

	idlemgr_ctx->session_mode_before_enter_idle = MTK_DRM_SESSION_INVALID;
	idlemgr_ctx->is_idle = 0;
	idlemgr_ctx->enterulps = 0;
	idlemgr_ctx->idlemgr_last_kick_time = ~(0ULL);
	idlemgr_ctx->cur_lp_cust_mode = 0;
	idlemgr_ctx->idle_check_interval = 50;

	snprintf(name, LEN, "mtk_drm_disp_idlemgr-%d", index);
	idlemgr->idlemgr_task =
			kthread_create(mtk_drm_idlemgr_monitor_thread, crtc, name);
	init_waitqueue_head(&idlemgr->idlemgr_wq);
	atomic_set(&idlemgr->idlemgr_task_active, 1);
	wake_up_process(idlemgr->idlemgr_task);

	snprintf(name, LEN, "dis_ki-%d", index);
	idlemgr->kick_task =
			kthread_create(mtk_drm_async_kick_idlemgr_thread, crtc, name);
	init_waitqueue_head(&idlemgr->kick_wq);
	atomic_set(&idlemgr->kick_task_active, 0);
	wake_up_process(idlemgr->kick_task);

	atomic_set(&idlemgr->async_enabled, 0);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mode = mtk_dsi_is_cmd_mode(output_comp);
	if (mode && mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC)) {
		mtk_drm_idlemgr_get_private_data(crtc, &idlemgr_ctx->priv);
		DDPMSG("%s, %d, crtc:%u,cmd:%d,async:%d/%d,cpu:0x%x-%uMhz,sram:%d\n",
			__func__, __LINE__, drm_crtc_index(crtc), mode,
			idlemgr_ctx->priv.hw_async, idlemgr_ctx->priv.vblank_async,
			idlemgr_ctx->priv.cpu_mask, idlemgr_ctx->priv.cpu_freq / 1000,
			idlemgr_ctx->priv.sram_sleep);
		mtk_drm_idlemgr_bind_cpu(idlemgr->idlemgr_task, crtc, true);
		mtk_drm_idlemgr_bind_cpu(idlemgr->kick_task, crtc, true);

		init_waitqueue_head(&idlemgr->async_event_wq);
		atomic_set(&idlemgr->async_ref, 0);

		snprintf(name, LEN, "dis_async-%d", index);
		idlemgr->async_handler_task =
			kthread_create(mtk_drm_async_handler_thread, crtc, name);
		init_waitqueue_head(&idlemgr->async_handler_wq);
		atomic_set(&idlemgr->async_cb_count, 0);
		INIT_LIST_HEAD(&idlemgr->async_cb_list);
		spin_lock_init(&idlemgr->async_lock);
		wake_up_process(idlemgr->async_handler_task);

		if (idlemgr_ctx->priv.vblank_async == true) {
			DDPMSG("%s, %d, init vblank async\n", __func__, __LINE__);
			snprintf(name, LEN, "dis_vblank-%d", index);
			idlemgr->async_vblank_task =
				kthread_create(mtk_drm_async_vblank_thread, crtc, name);
			init_waitqueue_head(&idlemgr->async_vblank_wq);
			atomic_set(&idlemgr->async_vblank_active, 0);
			wake_up_process(idlemgr->async_vblank_task);
		}

		cpu_latency_qos_add_request(&idlemgr->cpu_qos_req, PM_QOS_DEFAULT_VALUE);
	}

	return 0;
}

static void mtk_drm_idlemgr_poweroff_connector(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_POWEROFF, NULL);
}

static void mtk_drm_idlemgr_disable_connector(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	bool async = false;

	async = mtk_drm_idlemgr_get_async_status(crtc);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_DISABLE, &async);
}

static void mtk_drm_idlemgr_enable_connector(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;
	bool async = false;

	async = mtk_drm_idlemgr_get_async_status(crtc);
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, CONNECTOR_ENABLE, &async);
}

static void mtk_drm_idlemgr_disable_crtc(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	unsigned int crtc_id = drm_crtc_index(&mtk_crtc->base);
	bool mode = mtk_crtc_is_dc_mode(crtc);
	struct mtk_drm_private *priv =
				mtk_crtc->base.dev->dev_private;
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_ddp_comp *output_comp = NULL;
	int en = 0, perf_detail = 0;
	struct cmdq_pkt *cmdq_handle1 = NULL, *cmdq_handle2 = NULL;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	unsigned long long start, end;
	char *perf_string = NULL;

	DDPINFO("%s, crtc%d+\n", __func__, crtc_id);

	if (mode) {
		DDPINFO("crtc%d mode:%d bypass enter idle\n", crtc_id, mode);
		DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
		return;
	}

	if (idlemgr->perf != NULL) {
		start = sched_clock();
		perf_detail = atomic_read(&idlemgr->perf->detail);
		if (perf_detail) {
			perf_string = kmalloc(MTK_IDLE_PERF_DETAIL_LENGTH, GFP_KERNEL);
			if (!perf_string) {
				DDPMSG("%s:%d, failed to allocate perf_string\n",
					__func__, __LINE__);
				perf_detail = 0;
			} else {
				perf_string[0] = '\0';
				mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
						"START", -1, perf_string, false);
			}
		}
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC))
		atomic_set(&idlemgr->async_enabled,
			idlemgr_ctx->priv.hw_async ? 1 : 0);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"polling_dsi", 1, perf_string, false);

	/* 0. Waiting CLIENT_DSI_CFG/CLIENT_CFG thread done */
	if (mtk_crtc->gce_obj.client[CLIENT_DSI_CFG])
		mtk_crtc_pkt_create(&cmdq_handle1, crtc,
			mtk_crtc->gce_obj.client[CLIENT_DSI_CFG]);

	if (cmdq_handle1) {
		cmdq_pkt_flush(cmdq_handle1);
		cmdq_pkt_destroy(cmdq_handle1);
		cmdq_handle1 = NULL;
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"polling_eof", 2, perf_string, false);

	mtk_crtc_pkt_create(&cmdq_handle2, crtc, mtk_crtc->gce_obj.client[CLIENT_CFG]);

	if (cmdq_handle2) {
		cmdq_pkt_clear_event(cmdq_handle2,
				     mtk_crtc->gce_obj.event[EVENT_STREAM_EOF]);
		cmdq_pkt_clear_event(cmdq_handle2,
				     mtk_crtc->gce_obj.event[EVENT_STREAM_BLOCK]);
		cmdq_pkt_wfe(cmdq_handle2,
				     mtk_crtc->gce_obj.event[EVENT_CABC_EOF]);

		cmdq_pkt_flush(cmdq_handle2);

		if (mtk_crtc->is_mml) {
			mtk_crtc->mml_link_state = MML_IR_IDLE;
			CRTC_MMP_MARK(0, mml_dbg, (unsigned long)cmdq_handle2, MMP_MML_IDLE);
		}

		cmdq_pkt_destroy(cmdq_handle2);
		cmdq_handle2 = NULL;
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"disable_conn", 3, perf_string, false);

	/* 1. stop connector */
	mtk_drm_idlemgr_disable_connector(crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"crtc_stop", 4, perf_string, false);
	/* 2. stop CRTC */
	mtk_crtc_stop(mtk_crtc, false);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"dis_addon", 5, perf_string, false);
	/* 3. disconnect addon module and recover config */
	mtk_crtc_disconnect_addon_module(crtc);
	if (crtc_state) {
		crtc_state->lye_state.mml_ir_lye = 0;
		crtc_state->lye_state.mml_dl_lye = 0;
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"async_wait1", 6, perf_string, false);
	mtk_drm_idle_async_wait(crtc, 50, "stop_crtc_async");

	if (atomic_read(&idlemgr->async_enabled) == 1) {
		mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
					"poweroff_conn", 7, perf_string, false);
		/* hrt bw has been cleared when mtk_crtc_stop,
		 * do dsi power off after enter ulps mode,
		 */
		mtk_drm_idlemgr_poweroff_connector(crtc);

		// trigger vblank async task
		if (idlemgr_ctx->priv.vblank_async == true &&
			atomic_read(&idlemgr->async_enabled) != 0) {
			mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
						"vblank_off", 8, perf_string, false);
			mtk_drm_idlemgr_async_get(crtc, USER_VBLANK_OFF);
			atomic_set(&idlemgr->async_vblank_active, 1);
			wake_up_interruptible(&idlemgr->async_vblank_wq);
		}
	} else {
		mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
					"update_hrt", 9, perf_string, false);
		/* 4. set HRT BW to 0 */
		if (mtk_drm_helper_get_opt(priv->helper_opt,
					MTK_DRM_OPT_MMQOS_SUPPORT))
			mtk_disp_set_hrt_bw(mtk_crtc, 0);
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"update_mmclk", 10, perf_string, false);
	/* 5. Release MMCLOCK request */
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, SET_MMCLK_BY_DATARATE,
				&en);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"dis_default_path", 11, perf_string, false);
	/* 6. disconnect path */
	mtk_crtc_disconnect_default_path(mtk_crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"unprepare", 12, perf_string, false);
	/* 7. power off all modules in this CRTC */
	mtk_crtc_ddp_unprepare(mtk_crtc);

	if (idlemgr_ctx->priv.vblank_async == false ||
		atomic_read(&idlemgr->async_enabled) == 0) {
		mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
					"vblank_off", 13, perf_string, false);
		drm_crtc_vblank_off(crtc);
		mtk_crtc_vblank_irq(&mtk_crtc->base);
	}

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
					"power_off", 14, perf_string, false);
		/* 8. power off MTCMOS */
		DDPFENCE("%s:%d power_state = false\n", __func__, __LINE__);
		mtk_drm_top_clk_disable_unprepare(crtc->dev);
	}

	if (idlemgr_ctx->priv.vblank_async == true &&
		atomic_read(&idlemgr->async_enabled) != 0) {
		mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
					"vblank_async", 15, perf_string, false);
		mtk_drm_idle_async_wait(crtc, 0, "vblank_async");
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"vsync_switch", -1, perf_string, false);
	/* 9. disable fake vsync if need */
	mtk_drm_fake_vsync_switch(crtc, false);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"dis_cmdq", 17, perf_string, false);

	/* 10. CMDQ power off */
	cmdq_mbox_disable(mtk_crtc->gce_obj.client[CLIENT_CFG]->chan);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"STOP", -1, perf_string, false);

	/* 11. idle manager performance monitor */
	if (idlemgr->perf != NULL) {
		unsigned long long cost;

		end = sched_clock();
		cost = (end - start) / 1000;
		mtk_drm_idlemgr_perf_update(crtc, true, cost);

		// dump detail performance data when exceed 10ms
		if (perf_detail) {
			if (cost > 10000)
				DDPMSG(
					"%s:async:%d,cpu:(0x%x,%uMhz,%dus),sram:%d,total:%lluus,detail:%s\n",
					__func__, atomic_read(&idlemgr->async_enabled),
					idlemgr_ctx->priv.cpu_mask,
					idlemgr_ctx->priv.cpu_freq / 1000,
					idlemgr_ctx->priv.cpu_dma_latency,
					idlemgr_ctx->priv.sram_sleep,
					cost, perf_string);
			kfree(perf_string);
		}
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC))
		atomic_set(&idlemgr->async_enabled, 0);
	DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
}

/* TODO: we should restore the current setting rather than default setting */
static void mtk_drm_idlemgr_enable_crtc(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int crtc_id = drm_crtc_index(crtc);
	struct mtk_drm_private *priv =
			mtk_crtc->base.dev->dev_private;
	bool mode = mtk_crtc_is_dc_mode(crtc);
	struct mtk_ddp_comp *comp;
	unsigned int i, j;
	struct mtk_ddp_comp *output_comp = NULL;
	int en = 1, perf_detail = 0;
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct mtk_drm_idlemgr *idlemgr = mtk_crtc->idlemgr;
	struct mtk_drm_idlemgr_context *idlemgr_ctx = idlemgr->idlemgr_ctx;
	unsigned long long start, end;
	char *perf_string = NULL;

	DDPINFO("crtc%d do %s+\n", crtc_id, __func__);

	if (mode) {
		DDPINFO("crtc%d mode:%d bypass exit idle\n", crtc_id, mode);
		DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
		return;
	}

	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (output_comp && mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI) {
		struct mtk_dsi *dsi = container_of(output_comp, struct mtk_dsi, ddp_comp);

		if (dsi)
			dsi->force_resync_after_idle = 1;
	}

	if (idlemgr->perf != NULL) {
		start = sched_clock();
		perf_detail = atomic_read(&idlemgr->perf->detail);
		if (perf_detail) {
			perf_string = kmalloc(MTK_IDLE_PERF_DETAIL_LENGTH, GFP_KERNEL);
			if (!perf_string) {
				DDPMSG("%s:%d, failed to allocate perf_string\n",
					__func__, __LINE__);
				perf_detail = 0;
			} else {
				perf_string[0] = '\0';
				mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
						"START", -1, perf_string, true);
			}
		}
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC))
		atomic_set(&idlemgr->async_enabled,
			idlemgr_ctx->priv.hw_async ? 1 : 0);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"enable_cmdq", 1, perf_string, true);

	/* 0. CMDQ power on */
	cmdq_mbox_enable(mtk_crtc->gce_obj.client[CLIENT_CFG]->chan);

	if (disp_helper_get_stage() == DISP_HELPER_STAGE_NORMAL) {
		mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
					"power_on", 2, perf_string, true);
		/* 1. power on mtcmos & init apsrc*/
		mtk_drm_top_clk_prepare_enable(crtc->dev);

		mtk_crtc_rst_module(crtc);

		mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
					"apsrc_ctrl", 3, perf_string, true);
		mtk_crtc_v_idle_apsrc_control(crtc, NULL, true, true,
			MTK_APSRC_CRTC_DEFAULT, false);
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"update_mmclk", 4, perf_string, true);
	/* 2. Request MMClock before enabling connector*/
	mtk_crtc_attach_ddp_comp(crtc, mtk_crtc->ddp_mode, true);
	if (output_comp)
		mtk_ddp_comp_io_cmd(output_comp, NULL, SET_MMCLK_BY_DATARATE,
				&en);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
					"async_wait0", 41, perf_string, true);
	mtk_drm_idle_async_wait(crtc, 0, "reset_async");

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"clear_cb", 42, perf_string, true);
	mtk_drm_clear_async_cb_list(crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"enable_conn", 5, perf_string, true);
	/* 3. prepare modules would be used in this CRTC */
	mtk_drm_idlemgr_enable_connector(crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"start_event_loop", 6, perf_string, true);
	/* 4. start event loop first */
	if (crtc_id == 0) {
		if (mtk_crtc_with_event_loop(crtc) &&
			(mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_event_loop(crtc);
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"ddp_prepare", 7, perf_string, true);
	/* 5. prepare modules would be used in this CRTC */
	mtk_crtc_ddp_prepare(mtk_crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"async_wait1", 8, perf_string, true);
	mtk_drm_idle_async_wait(crtc, 50, "prepare_async");

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"atf_instr", 9, perf_string, true);
	mtk_gce_backup_slot_init(mtk_crtc);

#ifndef DRM_CMDQ_DISABLE
	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_USE_M4U))
		mtk_crtc_prepare_instr(crtc);
#endif

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"start_trig_loop", 10, perf_string, true);
	/* 6. start trigger loop first to keep gce alive */
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (!IS_ERR_OR_NULL(output_comp) &&
		mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI) {
		if (mtk_crtc_with_sodi_loop(crtc) &&
			(!mtk_crtc_is_frame_trigger_mode(crtc)))
			mtk_crtc_start_sodi_loop(crtc);

		mtk_crtc_start_trig_loop(crtc);
		mtk_crtc_hw_block_ready(crtc);
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"async_wait2", 11, perf_string, true);
	mtk_drm_idle_async_wait(crtc, 80, "gce_thread_async");

	if ((priv->data->mmsys_id == MMSYS_MT6878) &&
		(mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_IDLEMGR_ASYNC)) &&
		(mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_USE_M4U))) {
		DDPINFO("%s crtc:%d cmdq_prepare_instr -\n",
			__func__, drm_crtc_index(crtc));
		mutex_unlock(&priv->cmdq_prepare_instr_lock);
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"connect_default_path", 12, perf_string, true);
	/* 7. connect path */
	mtk_crtc_connect_default_path(mtk_crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"config_default_path", 13, perf_string, true);
	/* 8. config ddp engine & set dirty for cmd mode */
	mtk_crtc_config_default_path(mtk_crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"connect_addon", 14, perf_string, true);
	/* 9. conect addon module and config
	 *    skip mml addon connect if kick idle by atomic commit
	 */
	if (crtc_state->lye_state.mml_ir_lye || crtc_state->lye_state.mml_dl_lye ||
		((crtc_state->prop_val[CRTC_PROP_OUTPUT_ENABLE] == 1) && (crtc_id == 0))) {
		mtk_crtc_addon_connector_connect(crtc, NULL);
		DDPINFO("%s, id = %d, ir=%u, dl=%u, cwb=%llu\n", __func__,
			crtc_id, crtc_state->lye_state.mml_ir_lye,
			crtc_state->lye_state.mml_dl_lye,
			crtc_state->prop_val[CRTC_PROP_OUTPUT_ENABLE]);
	} else {
		DDPINFO("%s, addon module\n", __func__);
		mtk_crtc_connect_addon_module(crtc);
	}
	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"update_hrt", 15, perf_string, true);
	/* 10. restore HRT BW */
	if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT)) {
		mtk_crtc_init_hrt_usage(crtc);
		mtk_disp_set_hrt_bw(mtk_crtc,
			mtk_crtc->qos_ctx->last_hrt_req);
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_HRT_BY_LARB) && priv->data->mmsys_id == MMSYS_MT6989) {
		mtk_disp_set_per_larb_hrt_bw(mtk_crtc,
				mtk_crtc->qos_ctx->last_larb_hrt_max);
	}

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"restore_plane", 16, perf_string, true);
	/* 11. restore OVL setting */
	mtk_crtc_restore_plane_setting(mtk_crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"update_pmqos", 17, perf_string, true);
	/* 12. Set QOS BW */
	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
		mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_BW, NULL);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"async_wait3", 18, perf_string, true);
	mtk_drm_idle_async_wait(crtc, 0, "conifg_async");

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"vblank_on", 19, perf_string, true);
	/* 13. set vblank */
	drm_crtc_vblank_on(crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"vsync_switch", -1, perf_string, true);
	/* 14. enable fake vsync if need */
	mtk_drm_fake_vsync_switch(crtc, true);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"vidle", 21, perf_string, true);
	/* 15. v-idle enable */
	// mtk_vidle_enable(crtc);

	mtk_drm_idlemgr_perf_detail_check(perf_detail, crtc,
				"STOP", -1, perf_string, true);

	/* 16. idle manager performance monitor */
	if (idlemgr->perf != NULL) {
		unsigned long long cost;

		end = sched_clock();
		cost = (end - start) / 1000;
		CRTC_MMP_MARK((int)drm_crtc_index(crtc),
				leave_idle, cost, perf_aee_timeout * 1000);

		mtk_drm_idlemgr_perf_update(crtc, false, cost);

		// dump detail performance data when exceed 16ms
		if (perf_detail) {
			if (cost > 16000)
				DDPMSG(
					"%s:async:%d,cpu:(0x%x,%uMhz,%dus),sram:%d,total:%lluus,detail:%s\n",
					__func__, atomic_read(&idlemgr->async_enabled),
					idlemgr_ctx->priv.cpu_mask,
					idlemgr_ctx->priv.cpu_freq / 1000,
					idlemgr_ctx->priv.cpu_dma_latency,
					idlemgr_ctx->priv.sram_sleep,
					cost, perf_string);
			kfree(perf_string);
		}

		if (perf_aee_timeout > 0 && cost > (unsigned long long)perf_aee_timeout * 1000) {
			DDPAEE("[IDLE] perf drop:%lluus, timeout:%uus\n",
				cost, perf_aee_timeout * 1000);
			perf_aee_timeout = 0;
		}
	}

	if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_IDLEMGR_ASYNC))
		atomic_set(&idlemgr->async_enabled, 0);

	DDPINFO("crtc%d do %s-\n", crtc_id, __func__);
}

enum MTK_DRM_IDLEMGR_BY_WB_STATUS {
	MTK_DRM_IDLEMGR_BY_WB_NONE,
	MTK_DRM_IDLEMGR_BY_WB_ENTERING,
	MTK_DRM_IDLEMGR_BY_WB_ATTACHED,
	MTK_DRM_IDLEMGR_BY_WB_DETACHED,
	MTK_DRM_IDLEMGR_BY_WB_USING,
	MTK_DRM_IDLEMGR_BY_WB_SKIP,
	MTK_DRM_IDLEMGR_BY_WB_LEAVE
};
static int mtk_drm_idlemgr_wb_test_id;

static int mtk_drm_idlemgr_wb_get_fb(struct mtk_drm_crtc *mtk_crtc, unsigned int fmt,
					 struct mtk_rect *roi, struct mtk_rect *roi_r)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct drm_mode_fb_cmd2 mode = {0};
	struct mtk_drm_gem_obj *mtk_gem;
	unsigned int w, h, subblock_nr;
	size_t size_header, size_body;

	if (!mtk_crtc->idlemgr->wb_fb) {
		w = ALIGN(roi->width, 32);
		h = ALIGN(roi->height, 8);
		subblock_nr = w * h / 256;
		size_header = ALIGN(subblock_nr * 16, 1024);
		size_body = subblock_nr * 1024;
		mode.pixel_format = fmt;
		mode.modifier[0] = DRM_FORMAT_MOD_ARM_AFBC(AFBC_FORMAT_MOD_BLOCK_SIZE_32x8 |
							   AFBC_FORMAT_MOD_SPARSE);
		mode.width = roi->width;
		mode.height = roi->height;
		mode.pitches[0] = w * 4;
		mtk_gem = mtk_drm_gem_create(
			crtc->dev, size_header + size_body, true);
		mtk_crtc->idlemgr->wb_fb = mtk_drm_framebuffer_create(
			crtc->dev, &mode, &mtk_gem->base);
		mtk_crtc->idlemgr->wb_buffer_iova = mtk_gem->dma_addr;
	}
	if (!mtk_crtc->idlemgr->wb_fb)
		return -ENOMEM;

	if (!mtk_crtc->is_dual_pipe)
		return 0;

	if (!mtk_crtc->idlemgr->wb_fb_r){
		w = ALIGN(roi_r->width, 32);
		h = ALIGN(roi_r->height, 8);
		subblock_nr = w * h / 256;
		size_header = ALIGN(subblock_nr * 16, 1024);
		size_body = subblock_nr * 1024;
		mode.pixel_format = fmt;
		mode.width = roi_r->width;
		mode.height = roi_r->height;
		mode.pitches[0] = w * 4;
		mtk_gem = mtk_drm_gem_create(
			crtc->dev, size_header + size_body, true);
		mtk_crtc->idlemgr->wb_fb_r = mtk_drm_framebuffer_create(
			crtc->dev, &mode, &mtk_gem->base);
		mtk_crtc->idlemgr->wb_buffer_r_iova = mtk_gem->dma_addr;
	}
	if (!mtk_crtc->idlemgr->wb_fb_r)
		return -ENOMEM;
	return 0;
}

static void mtk_drm_idlemgr_wb_cmdq_cb(struct cmdq_cb_data data)
{
	struct mtk_cmdq_cb_data *cb_data = data.data;
	struct drm_crtc *crtc = cb_data->crtc;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	unsigned int *trace;
	unsigned int bw_base;

	trace = mtk_get_gce_backup_slot_va(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE);
	DDPINFO("after enter IDLEMGR_BY_WB_TRACE:0x%x\n", *trace);

	if (*trace & BIT(4)) {
		bw_base = mtk_drm_primary_frame_bw(crtc);
		mtk_disp_set_hrt_bw(mtk_crtc, bw_base);
		mtk_crtc->qos_ctx->last_hrt_req = bw_base;
	}

	cmdq_pkt_destroy(cb_data->cmdq_handle);
	kfree(cb_data);

	if (mtk_drm_idlemgr_wb_test_id & BIT(20)) {
		mtk_drm_crtc_analysis(&mtk_crtc->base);
		mtk_drm_crtc_dump(&mtk_crtc->base);
	}
}

bool mtk_drm_idlemgr_wb_is_entered(struct mtk_drm_crtc *mtk_crtc)
{
	if (!mtk_crtc->idlemgr)
		return false;
	return mtk_crtc->idlemgr->idlemgr_ctx->wb_entered;
}

void mtk_drm_idlemgr_wb_capture(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	const struct mtk_addon_scenario_data *addon_data;
	const struct mtk_addon_scenario_data *addon_data_dual;
	const struct mtk_addon_module_data *addon_module;
	const struct mtk_addon_path_data *path_data;
	union mtk_addon_config addon_config;
	struct total_tile_overhead to_info;
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	dma_addr_t status_pad;
	int gce_event, i;
	unsigned int fmt = DRM_FORMAT_XBGR2101010;//DRM_FORMAT_XBGR2101010,DRM_FORMAT_XBGR8888;
	unsigned int flag = DISP_BW_FORCE_UPDATE;

	DDPFUNC();

	addon_data = mtk_addon_get_scenario_data(__func__, crtc, IDLE_WDMA_WRITE_BACK);
	if (!addon_data)
		return;
	if (mtk_crtc->is_dual_pipe) {
		addon_data_dual = mtk_addon_get_scenario_data_dual(__func__, crtc, IDLE_WDMA_WRITE_BACK);
		if (!addon_data_dual)
			return;
	}
	status_pad = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_STATUS);
	/* 1. attach wdma */
	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 1);
	cmdq_pkt_write_value_addr(cmdq_handle,
		 mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE),
		 BIT(2), BIT(2));

	to_info = mtk_crtc_get_total_overhead(mtk_crtc);

	for (i = 0; i < addon_data->module_num; i++) {
		struct mtk_rect src_roi = {0}, src_roi_l = {0}, src_roi_r = {0};
		struct mtk_rect dst_roi = {0}, dst_roi_l = {0}, dst_roi_r = {0};

		addon_module = &addon_data->module_data[i];
		addon_config.config_type.module = addon_module->module;
		addon_config.config_type.type = addon_module->type;
		addon_config.addon_wdma_config.p_golden_setting_context =
				__get_golden_setting_context(mtk_crtc);

		src_roi.width = crtc->state->adjusted_mode.hdisplay;
		src_roi.height = crtc->state->adjusted_mode.vdisplay;

		if (mtk_crtc->is_dual_pipe) {
			if (to_info.is_support) {
				src_roi_l.width = to_info.left_in_width;
				src_roi_r.width = to_info.right_in_width;
			} else {
				src_roi_l.width = src_roi.width / 2;
				src_roi_r.width = src_roi.width / 2;
			}
			src_roi_r.height = src_roi_l.height = src_roi.height;
			dst_roi_l = src_roi_l;
			dst_roi_r = src_roi_r;
			mtk_drm_idlemgr_wb_get_fb(mtk_crtc, fmt, &dst_roi_l, &dst_roi_r);

			addon_config.addon_wdma_config.wdma_src_roi = src_roi_l;
			addon_config.addon_wdma_config.wdma_dst_roi = dst_roi_l;
			addon_config.addon_wdma_config.addr = mtk_crtc->idlemgr->wb_buffer_iova;
			addon_config.addon_wdma_config.fb = mtk_crtc->idlemgr->wb_fb;
			addon_config.addon_wdma_config.p_golden_setting_context->dst_width = dst_roi_l.width;
			addon_config.addon_wdma_config.p_golden_setting_context->dst_height = dst_roi_l.height;
			/* connect left pipe */
			mtk_addon_connect_after(crtc, mtk_crtc->ddp_mode, addon_module,
						&addon_config, cmdq_handle);

			addon_module = &addon_data_dual->module_data[i];
			addon_config.config_type.module = addon_module->module;
			addon_config.config_type.type = addon_module->type;
			addon_config.addon_wdma_config.wdma_src_roi = src_roi_r;
			addon_config.addon_wdma_config.wdma_dst_roi = dst_roi_r;
			addon_config.addon_wdma_config.addr = mtk_crtc->idlemgr->wb_buffer_r_iova;
			addon_config.addon_wdma_config.fb = mtk_crtc->idlemgr->wb_fb_r;
			addon_config.addon_wdma_config.p_golden_setting_context->dst_width = dst_roi_r.width;
			addon_config.addon_wdma_config.p_golden_setting_context->dst_height = dst_roi_r.height;
			/* connect right pipe */
			mtk_addon_connect_after(crtc, mtk_crtc->ddp_mode, addon_module,
						&addon_config, cmdq_handle);
		} else {
			dst_roi = src_roi;
			mtk_drm_idlemgr_wb_get_fb(mtk_crtc, fmt, &dst_roi, NULL);
			addon_config.addon_wdma_config.wdma_src_roi = src_roi;
			addon_config.addon_wdma_config.wdma_dst_roi = dst_roi;
			addon_config.addon_wdma_config.fb = mtk_crtc->idlemgr->wb_fb;
			addon_config.addon_wdma_config.addr = mtk_crtc->idlemgr->wb_buffer_iova;
			mtk_addon_connect_after(crtc, mtk_crtc->ddp_mode, addon_module,
						&addon_config, cmdq_handle);
		}
	}
	cmdq_pkt_write_value_addr(cmdq_handle, status_pad, MTK_DRM_IDLEMGR_BY_WB_ATTACHED, ~0);

	/* 2.1 wait WDMA done */
	addon_module = &addon_data->module_data[0];
	path_data = mtk_addon_module_get_path(addon_module->module);
	gce_event = get_comp_wait_event(mtk_crtc, priv->ddp_comp[path_data->path[path_data->path_len - 1]]);

	mtk_ddp_comp_io_cmd(priv->ddp_comp[path_data->path[path_data->path_len - 1]], NULL, PMQOS_UPDATE_BW, &flag);
	cmdq_pkt_wfe(cmdq_handle, gce_event);
	if (mtk_crtc->is_dual_pipe) {
		addon_module = &addon_data_dual->module_data[0];
		path_data = mtk_addon_module_get_path(addon_module->module);
		gce_event = get_comp_wait_event(mtk_crtc, priv->ddp_comp[path_data->path[path_data->path_len - 1]]);

		mtk_ddp_comp_io_cmd(priv->ddp_comp[path_data->path[path_data->path_len - 1]],
				    NULL, PMQOS_UPDATE_BW, &flag);
		cmdq_pkt_wfe(cmdq_handle, gce_event);
	}
	mtk_crtc_wait_frame_done(mtk_crtc, cmdq_handle, DDP_FIRST_PATH, 1);

	cmdq_pkt_write_value_addr(cmdq_handle,
		 mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE),
		 BIT(3), BIT(3));

	/* 2.2 detach wdma */
	for (i = 0; i < addon_data->module_num; i++) {
		addon_module = &addon_data->module_data[i];
		addon_config.config_type.module = addon_module->module;
		addon_config.config_type.type = addon_module->type;

		if (mtk_crtc->is_dual_pipe) {
			/* disconnect left pipe */
			mtk_addon_disconnect_after(crtc, mtk_crtc->ddp_mode, addon_module,
						   &addon_config, cmdq_handle);
			/* disconnect right pipe */
			addon_module = &addon_data_dual->module_data[i];
			addon_config.config_type.module = addon_module->module;
			addon_config.config_type.type = addon_module->type;
			mtk_addon_disconnect_after(crtc, mtk_crtc->ddp_mode, addon_module,
						   &addon_config, cmdq_handle);
		} else {
			mtk_addon_disconnect_after(crtc, mtk_crtc->ddp_mode, addon_module,
						   &addon_config, cmdq_handle);
		}
	}
	cmdq_pkt_write_value_addr(cmdq_handle, status_pad, MTK_DRM_IDLEMGR_BY_WB_DETACHED, ~0);
}


void mtk_drm_idlemgr_wb_reconfig(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	struct mtk_plane_state *mtk_plane_state;
	struct mtk_ddp_comp *comp, *ovl_comp = NULL;
	dma_addr_t status_pad, break_pad;
	int i, j;

	GCE_COND_DECLARE;
	struct cmdq_operand lop, rop;

	status_pad = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_STATUS);
	break_pad = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_BREAK);

	if ((mtk_drm_idlemgr_wb_test_id & 0xffff) == 2)
		cmdq_pkt_write_value_addr(cmdq_handle, break_pad, 1, 1);

	GCE_COND_ASSIGN(cmdq_handle, CMDQ_THR_SPR_IDX3, CMDQ_GPR_R07);
	lop.reg = true;
	lop.idx = CMDQ_THR_SPR_IDX2;
	rop.reg = false;
	rop.idx = 0;
	cmdq_pkt_read(cmdq_handle, mtk_crtc->gce_obj.base, break_pad, lop.idx);

	GCE_IF(lop, R_CMDQ_EQUAL, rop);
	/* if the value of DISP_SLOT_IDLEMGR_BY_WB_BREAK is 0 */
	// off all ovl layers & config the top layer using wb
	cmdq_pkt_write_value_addr(cmdq_handle,
		 mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE),
		 BIT(4), BIT(4));

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		mtk_ddp_comp_config_begin(comp, cmdq_handle, j);
	}
	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
			mtk_ddp_comp_config_begin(comp, cmdq_handle, j);
		}
	}

	_mtk_crtc_atmoic_addon_module_disconnect(crtc, mtk_crtc->ddp_mode,
			&crtc_state->lye_state, cmdq_handle);
	mtk_crtc_all_layer_off(mtk_crtc, cmdq_handle);

	for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_OVL) {
			ovl_comp = comp;
			break;
		}
	}
	if (!ovl_comp) {
		DDPPR_ERR("%s L%d: No proper ovl module in CRTC %s\n", __func__, __LINE__,
			mtk_crtc->base.name);
		return;
	}

	mtk_plane_state = kzalloc(sizeof(*mtk_plane_state), GFP_KERNEL);

	mtk_plane_state->pending.addr = mtk_crtc->idlemgr->wb_buffer_iova;
	mtk_plane_state->pending.pitch = mtk_crtc->idlemgr->wb_fb->pitches[0];
	mtk_plane_state->pending.width = mtk_crtc->idlemgr->wb_fb->width;
	mtk_plane_state->pending.height = mtk_crtc->idlemgr->wb_fb->height;
	mtk_plane_state->pending.format = mtk_crtc->idlemgr->wb_fb->format->format;
	mtk_plane_state->pending.prop_val[PLANE_PROP_COMPRESS] = 1;
	mtk_plane_state->pending.prop_val[PLANE_PROP_VPITCH] = mtk_crtc->idlemgr->wb_fb->height;
	mtk_plane_state->pending.config = 1;
	mtk_plane_state->pending.dirty = 1;
	mtk_plane_state->pending.enable = true;
	mtk_plane_state->pending.is_sec = false;

	mtk_plane_state->comp_state.comp_id = ovl_comp->id;
	mtk_plane_state->comp_state.lye_id = 0;
	mtk_plane_state->comp_state.ext_lye_id = 0;
	mtk_plane_state->crtc = crtc;
	mtk_plane_state->base.crtc = crtc;
	mtk_plane_state->base.alpha = ((0xFF << 8) | 0xFF);

	mtk_ddp_comp_layer_config(ovl_comp, 0, mtk_plane_state, cmdq_handle);

	cmdq_pkt_write_value_addr(cmdq_handle, status_pad, MTK_DRM_IDLEMGR_BY_WB_USING, ~0);
	if (!mtk_crtc->is_dual_pipe)
		goto out;

	for_each_comp_in_dual_pipe(comp, mtk_crtc, i, j) {
		if (mtk_ddp_comp_get_type(comp->id) == MTK_DISP_OVL) {
			ovl_comp = comp;
			break;
		}
	}
	if (!ovl_comp) {
		DDPPR_ERR("%s L%d: No proper ovl module in CRTC %s\n", __func__, __LINE__,
			mtk_crtc->base.name);
		goto out;
	}

	mtk_plane_state->pending.addr = mtk_crtc->idlemgr->wb_buffer_r_iova;
	mtk_plane_state->pending.pitch = mtk_crtc->idlemgr->wb_fb_r->pitches[0];
	mtk_plane_state->pending.width = mtk_crtc->idlemgr->wb_fb_r->width;
	mtk_plane_state->pending.height = mtk_crtc->idlemgr->wb_fb_r->height;
	mtk_plane_state->pending.format = mtk_crtc->idlemgr->wb_fb_r->format->format;
	mtk_plane_state->comp_state.comp_id = ovl_comp->id;
	mtk_ddp_comp_layer_config(ovl_comp, 0, mtk_plane_state, cmdq_handle);

	GCE_ELSE;
	/* if the value of DISP_SLOT_IDLEMGR_BY_WB_BREAK is not 0 */
	cmdq_pkt_write_value_addr(cmdq_handle,
		 mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE),
		 BIT(5), BIT(5));
	cmdq_pkt_write_value_addr(cmdq_handle, break_pad, 0, ~0);

	cmdq_pkt_write_value_addr(cmdq_handle, status_pad, MTK_DRM_IDLEMGR_BY_WB_SKIP, ~0);
	GCE_FI;

out:
	kfree(mtk_plane_state);
}

void mtk_drm_idlemgr_wb_enter(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	unsigned int *status, *wb_break, *trace;
	dma_addr_t status_pad, break_pad;
	bool seprated_pkt = false;
	unsigned int bw_base;

	GCE_COND_DECLARE;
	struct cmdq_operand lop, rop;

	/* hrt bw add wdma part */
	bw_base = mtk_drm_primary_frame_bw(crtc);
	mtk_crtc->qos_ctx->last_hrt_req += bw_base;
	mtk_disp_set_hrt_bw(mtk_crtc, mtk_crtc->qos_ctx->last_hrt_req);

	if (!cmdq_handle) {
		seprated_pkt = true;
		mtk_crtc_pkt_create(&cmdq_handle, crtc, mtk_crtc->gce_obj.client[CLIENT_CFG]);
	}

	status = mtk_get_gce_backup_slot_va(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_STATUS);
	*status = MTK_DRM_IDLEMGR_BY_WB_ENTERING;

	wb_break = mtk_get_gce_backup_slot_va(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_BREAK);
	*wb_break = 0;

	trace = mtk_get_gce_backup_slot_va(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE);
	*trace = BIT(0);

	status_pad = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_STATUS);
	break_pad = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_BREAK);

	if ((mtk_drm_idlemgr_wb_test_id & 0xffff) == 1)
		cmdq_pkt_write_value_addr(cmdq_handle, break_pad, 1, 1);

	GCE_COND_ASSIGN(cmdq_handle, CMDQ_THR_SPR_IDX1, CMDQ_GPR_R07);
	lop.reg = true;
	lop.idx = CMDQ_THR_SPR_IDX2;
	rop.reg = false;
	rop.idx = 0;
	cmdq_pkt_read(cmdq_handle, mtk_crtc->gce_obj.base, break_pad, lop.idx);

	GCE_IF(lop, R_CMDQ_EQUAL, rop);
	/* if the value of DISP_SLOT_IDLEMGR_BY_WB_BREAK is 0 */
	cmdq_pkt_write_value_addr(cmdq_handle,
		 mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE),
		 BIT(1), BIT(1));

	mtk_drm_idlemgr_wb_capture(mtk_crtc, cmdq_handle);
	mtk_drm_idlemgr_wb_reconfig(mtk_crtc, cmdq_handle);

	GCE_ELSE;
	/* if the value of DISP_SLOT_IDLEMGR_BY_WB_BREAK is not 0 */
	cmdq_pkt_write_value_addr(cmdq_handle,
		 mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE),
		 BIT(15), BIT(15));
	cmdq_pkt_write_value_addr(cmdq_handle, break_pad, 0, ~0);
	cmdq_pkt_write_value_addr(cmdq_handle, status_pad, MTK_DRM_IDLEMGR_BY_WB_LEAVE, ~0);

	GCE_FI;

	if (seprated_pkt) {
		struct mtk_cmdq_cb_data *cb_data;

		cb_data = kmalloc(sizeof(*cb_data), GFP_KERNEL);
		if (!cb_data) {
			DDPPR_ERR("%s:%d, cb data creation failed\n", __func__, __LINE__);
			return;
		}
		cb_data->cmdq_handle = cmdq_handle;
		cb_data->crtc = crtc;
		if (cmdq_pkt_flush_threaded(cmdq_handle, mtk_drm_idlemgr_wb_cmdq_cb, cb_data) < 0)
			DDPPR_ERR("%s:%d, failed to flush cmdq pkt\n", __func__, __LINE__);
	}
	mtk_crtc->idlemgr->idlemgr_ctx->wb_entered = true;
}

void mtk_drm_idlemgr_wb_leave(struct mtk_drm_crtc *mtk_crtc, struct cmdq_pkt *cmdq_handle)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	dma_addr_t status_pad;
	unsigned int *wb_break =
		mtk_get_gce_backup_slot_va(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_BREAK);

	GCE_COND_DECLARE;
	struct cmdq_operand lop, rop;

	*wb_break = 1;
	mtk_crtc->idlemgr->idlemgr_ctx->wb_entered = false;

	cmdq_pkt_write_value_addr(cmdq_handle,
		 mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE),
		 BIT(16), BIT(16));
	status_pad = mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_STATUS);

	GCE_COND_ASSIGN(cmdq_handle, CMDQ_THR_SPR_IDX1, CMDQ_GPR_R07);
	lop.reg = true;
	lop.idx = CMDQ_THR_SPR_IDX2;
	rop.reg = false;
	rop.idx = MTK_DRM_IDLEMGR_BY_WB_USING;
	cmdq_pkt_read(cmdq_handle, mtk_crtc->gce_obj.base, status_pad, lop.idx);

	GCE_IF(lop, R_CMDQ_EQUAL, rop);
	/* if the value of DISP_SLOT_IDLEMGR_BY_WB_STATUS is USING */
	cmdq_pkt_write_value_addr(cmdq_handle,
		 mtk_get_gce_backup_slot_pa(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE),
		 BIT(17), BIT(17));
	mtk_crtc_all_layer_off(mtk_crtc, cmdq_handle);
	_mtk_crtc_atmoic_addon_module_connect(crtc, mtk_crtc->ddp_mode,
				&crtc_state->lye_state, cmdq_handle);
	__mtk_crtc_restore_plane_setting(mtk_crtc, cmdq_handle);
	GCE_FI;

	cmdq_pkt_write_value_addr(cmdq_handle, status_pad, MTK_DRM_IDLEMGR_BY_WB_LEAVE, ~0);
}

void mtk_drm_idlemgr_wb_leave_post(struct mtk_drm_crtc *mtk_crtc)
{
	unsigned int *status, *trace;

	if (!mtk_crtc->idlemgr)
		return;

	status = mtk_get_gce_backup_slot_va(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_STATUS);
	if (*status != MTK_DRM_IDLEMGR_BY_WB_LEAVE)
		return;

	trace = mtk_get_gce_backup_slot_va(mtk_crtc, DISP_SLOT_IDLEMGR_BY_WB_TRACE);
	DDPINFO("after leave IDLEMGR_BY_WB_TRACE:0x%x\n", *trace);
	*status = MTK_DRM_IDLEMGR_BY_WB_NONE;

	if (mtk_drm_idlemgr_wb_test_id & BIT(21)) {
		mtk_drm_crtc_analysis(&mtk_crtc->base);
		mtk_drm_crtc_dump(&mtk_crtc->base);
	}
}

void mtk_drm_idlemgr_wb_fill_buf(struct drm_crtc *crtc, int value)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_drm_gem_obj *mtk_gem;
	int i = 0;
	int *ptr;

	mtk_gem = (struct mtk_drm_gem_obj *)mtk_fb_get_gem_obj(mtk_crtc->idlemgr->wb_fb);
	ptr = mtk_gem->kvaddr;
	do {
		*(ptr + i) = value;
		i++;
	} while (i * 4 < mtk_gem->size);

	if (!mtk_crtc->idlemgr->wb_fb_r)
		return;

	mtk_gem = (struct mtk_drm_gem_obj *)mtk_fb_get_gem_obj(mtk_crtc->idlemgr->wb_fb_r);
	ptr = mtk_gem->kvaddr;
	i = 0;
	do {
		*(ptr + i) = value;
		i++;
	} while (i * 4 < mtk_gem->size);

}

void mtk_drm_idlemgr_wb_test(int value)
{
	mtk_drm_idlemgr_wb_test_id = value;
	DDPMSG("mtk_drm_idlemgr_wb_test_id set to:0x%x\n", mtk_drm_idlemgr_wb_test_id);
}

