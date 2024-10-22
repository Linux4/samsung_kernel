// SPDX-License-Identifier: GPL-2.0
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*       Tiffany Lin <tiffany.lin@mediatek.com>
*/

#include <linux/module.h>
#include <linux/dma-resv.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <uapi/linux/dma-heap.h>
#include <mtk_heap.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <uapi/linux/sched/types.h>

#include "mtk_vcodec_fence.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_enc.h"
#include "vdec_drv_if.h"
#include "venc_drv_if.h"
#include "group.h"

#define LOG_PARAM_INFO_SIZE 64
#define MAX_SUPPORTED_LOG_PARAMS_COUNT 12
#define VCODEC_MAX_GROUP_SIZE 1024

char mtk_vdec_tmp_log[LOG_PROPERTY_SIZE];
char mtk_venc_tmp_log[LOG_PROPERTY_SIZE];
char mtk_vdec_tmp_prop[LOG_PROPERTY_SIZE];
char mtk_venc_tmp_prop[LOG_PROPERTY_SIZE];


static int group_list[VCODEC_MAX_GROUP_SIZE];
static unsigned int group_list_size;
static spinlock_t group_lock;
struct freq_qos_request freq_min_request[8];


void mtk_vcodec_check_alive(struct timer_list *t)
{
	struct dvfs_params *params;
	struct mtk_vcodec_dev *dev;

	/* Only support vdec check alive now */
	if (mtk_vcodec_is_vcp(MTK_INST_DECODER)) {
		params = from_timer(params, t, vdec_active_checker);
		dev = container_of(params, struct mtk_vcodec_dev, vdec_dvfs_params);
		dev->check_alive_work.dev = dev;
		dev->check_alive_work.ctx = NULL;
		queue_work(dev->check_alive_workqueue, &dev->check_alive_work.work);

		/*retrigger timer for next check*/
		params->vdec_active_checker.expires =
			jiffies + msecs_to_jiffies(MTK_VDEC_CHECK_ACTIVE_INTERVAL);
		add_timer(&params->vdec_active_checker);
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_check_alive);

void mtk_vcodec_alive_checker_init(struct mtk_vcodec_ctx *ctx, bool is_first)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#ifdef VDEC_CHECK_ALIVE
	struct mtk_vcodec_dev *dev = ctx->dev;

	/* Only support vdec check alive now */
	if (mtk_vcodec_is_vcp(MTK_INST_DECODER) && is_first && ctx->type == MTK_INST_DECODER) {
		if (!dev->vdec_dvfs_params.has_timer) {
			mtk_v4l2_debug(0, "[%d][VDVFS][VDEC] init vdec alive checker...", ctx->id);
			timer_setup(&dev->vdec_dvfs_params.vdec_active_checker,
				mtk_vcodec_check_alive, 0);
			dev->vdec_dvfs_params.vdec_active_checker.expires =
			jiffies + msecs_to_jiffies(MTK_VDEC_CHECK_ACTIVE_INTERVAL);
			add_timer(&dev->vdec_dvfs_params.vdec_active_checker);
			dev->vdec_dvfs_params.has_timer = 1;
		}
	}
#endif
#endif
}

void mtk_vcodec_alive_checker_deinit(struct mtk_vcodec_ctx *ctx, bool is_last)
{
#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#ifdef VDEC_CHECK_ALIVE
	struct mtk_vcodec_dev *dev = ctx->dev;

	/* Only support vdec check alive now */
	if (mtk_vcodec_is_vcp(MTK_INST_DECODER) && is_last && ctx->type == MTK_INST_DECODER) {
		if (dev->vdec_dvfs_params.has_timer) {
			del_timer_sync(&dev->vdec_dvfs_params.vdec_active_checker);
			flush_workqueue(dev->check_alive_workqueue);
			dev->vdec_dvfs_params.has_timer = 0;
			mtk_v4l2_debug(0, "[%d][VDVFS][VDEC] deinit vdec alive checker...",
				ctx->id);
		}
	}
#endif
#endif
}

/* For encoder, this will enable logs in venc/*/
bool mtk_vcodec_dbg;
EXPORT_SYMBOL_GPL(mtk_vcodec_dbg);

/* For vcodec performance measure */
bool mtk_vcodec_perf;
EXPORT_SYMBOL_GPL(mtk_vcodec_perf);

/* The log level of v4l2 encoder or decoder driver.
 * That is, files under mtk-vcodec/.
 */
int mtk_v4l2_dbg_level;
EXPORT_SYMBOL_GPL(mtk_v4l2_dbg_level);

/* The log level of decoder low power mode related log.
 */
int mtk_vdec_lpw_level;
EXPORT_SYMBOL_GPL(mtk_vdec_lpw_level);

/* For vdec kernel driver trace enable */
bool mtk_vdec_trace_enable;
EXPORT_SYMBOL_GPL(mtk_vdec_trace_enable);

/* For vcodec vcp debug */
int mtk_vcodec_vcp;
EXPORT_SYMBOL_GPL(mtk_vcodec_vcp);

/* For vdec set property */
char *mtk_vdec_property = "";
EXPORT_SYMBOL(mtk_vdec_property);

/* For venc set property */
char *mtk_venc_property = "";
EXPORT_SYMBOL(mtk_venc_property);

/* For vdec vcp log info */
char *mtk_vdec_vcp_log = "";
EXPORT_SYMBOL(mtk_vdec_vcp_log);

/* For venc vcp log info */
char *mtk_venc_vcp_log = "";
EXPORT_SYMBOL(mtk_venc_vcp_log);

/* For venc slb cb info */
struct VENC_SLB_CB_T mtk_venc_slb_cb = {0};
EXPORT_SYMBOL(mtk_venc_slb_cb);

/* For vcp vdec low power mode force setting limit grouping count */
int mtk_vdec_lpw_limit = MTK_VDEC_GROUP_CNT;
EXPORT_SYMBOL_GPL(mtk_vdec_lpw_limit);

/* For vcp vdec low power mode force setting timer default timeout (ms) */
int mtk_vdec_lpw_timeout = MTK_VDEC_WAIT_GROUP_MS;
EXPORT_SYMBOL_GPL(mtk_vdec_lpw_timeout);

/* For vdec slc switch on/off */
bool mtk_vdec_slc_enable = true;
EXPORT_SYMBOL_GPL(mtk_vdec_slc_enable);

struct vcu_v4l2_func vcu_func = { NULL };
EXPORT_SYMBOL_GPL(vcu_func);

int support_svp_region;
EXPORT_SYMBOL_GPL(support_svp_region);

int support_wfd_region;
EXPORT_SYMBOL_GPL(support_wfd_region);

bool mtk_vcodec_is_vcp(int type)
{
	if (type > MTK_INST_ENCODER || type < MTK_INST_DECODER)
		return false;
	return (mtk_vcodec_vcp & (1 << type));
}
EXPORT_SYMBOL_GPL(mtk_vcodec_is_vcp);

/* for check if ctx state is in specific state range, params means:
 * state_a & state_b != MTK_STATE_NULL: check if state_a <= ctx state = state_b,
 * state_a == MTK_STATE_NULL: check if ctx state <= state_b,
 * state_b == MTK_STATE_NULL: check if state_a <= ctx state,
 * state_a == state_b: check if ctx state == state_a/b
 */
bool mtk_vcodec_state_in_range(struct mtk_vcodec_ctx *ctx, int state_a, int state_b)
{
	unsigned long flags;

	if (!ctx)
		return false;

	spin_lock_irqsave(&ctx->state_lock, flags);
	if ((state_a == MTK_STATE_NULL || state_a <= ctx->state) &&
	    (state_b == MTK_STATE_NULL || ctx->state <= state_b)) {
		spin_unlock_irqrestore(&ctx->state_lock, flags);
		return true;
	}
	spin_unlock_irqrestore(&ctx->state_lock, flags);
	return false;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_state_in_range);

/* check if ctx state is specific state */
bool mtk_vcodec_is_state(struct mtk_vcodec_ctx *ctx, int state)
{
	return mtk_vcodec_state_in_range(ctx, state, state);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_is_state);

int mtk_vcodec_get_state(struct mtk_vcodec_ctx *ctx)
{
	int state;
	unsigned long flags;

	if (!ctx)
		return MTK_STATE_FREE;

	spin_lock_irqsave(&ctx->state_lock, flags);
	state = ctx->state;
	spin_unlock_irqrestore(&ctx->state_lock, flags);
	return state;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_state);

/* if ctx state is specific state, then set state to target state */
int mtk_vcodec_set_state_from(struct mtk_vcodec_ctx *ctx, int target, int from)
{
	int state;
	unsigned long flags;

	if (!ctx)
		return MTK_STATE_FREE;

	spin_lock_irqsave(&ctx->state_lock, flags);
	state = ctx->state;
	if (ctx->state == from) {
		ctx->state = target;
		mtk_v4l2_debug(4, "[%d] set state %d from %d to %d",
			ctx->id, state, from, target);
	} else
		mtk_v4l2_debug(1, "[%d] set state %d from %d to %d fail",
			ctx->id, state, from, target);
	spin_unlock_irqrestore(&ctx->state_lock, flags);
	return state;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_state_from);

/* If ctx state is not except state, then set state to target state,
 * with checking for ABORT state only can be set to FREE state.
 * If except state is MTK_STATE_NULL, then will not check except state,
 * which means will only check if is ABORT state, otherwise just set to target state.
 */
int mtk_vcodec_set_state_except(struct mtk_vcodec_ctx *ctx, int target, int except_state)
{
	int state;
	unsigned long flags;

	if (!ctx)
		return MTK_STATE_FREE;

	spin_lock_irqsave(&ctx->state_lock, flags);
	state = ctx->state;
	if ((except_state == MTK_STATE_NULL || ctx->state != except_state) &&
	   (ctx->state != MTK_STATE_ABORT || target == MTK_STATE_FREE)) {
		ctx->state = target;
		mtk_v4l2_debug(4, "[%d] set state %d to %d (except %d)",
			ctx->id, state, target, except_state);
	} else
		mtk_v4l2_debug(1, "[%d] set state %d to %d fail (except %d)",
			ctx->id, state, target, except_state);
	spin_unlock_irqrestore(&ctx->state_lock, flags);
	return state;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_state_except);

int mtk_vcodec_set_state(struct mtk_vcodec_ctx *ctx, int target)
{
	return mtk_vcodec_set_state_except(ctx, target, MTK_STATE_NULL);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_state);

/* VCODEC FTRACE */
#if VCODEC_TRACE
void vcodec_trace(const char *fmt, ...)
{
	char buf[256] = {0};
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf[255] = '\0';

	trace_puts(buf);
}
EXPORT_SYMBOL(vcodec_trace);
#endif

void __iomem *mtk_vcodec_get_dec_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!data || reg_idx >= NUM_MAX_VDEC_REG_BASE) {
		mtk_v4l2_err("Invalid arguments, reg_idx=%d", reg_idx);
		return NULL;
	}
	return ctx->dev->dec_reg_base[reg_idx];
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_dec_reg_addr);

void __iomem *mtk_vcodec_get_enc_reg_addr(struct mtk_vcodec_ctx *data,
	unsigned int reg_idx)
{
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (!data || reg_idx >= NUM_MAX_VENC_REG_BASE) {
		mtk_v4l2_err("Invalid arguments, reg_idx=%d", reg_idx);
		return NULL;
	}
	return ctx->dev->enc_reg_base[reg_idx];
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_enc_reg_addr);


int mtk_vcodec_mem_alloc(struct mtk_vcodec_ctx *data,
						 struct mtk_vcodec_mem *mem)
{
	unsigned long size;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (data == NULL || mem == NULL) {
		mtk_v4l2_err("Invalid arguments, data=0x%lx, mem=0x%lx",
			(unsigned long)data, (unsigned long)mem);
		return -EINVAL;
	}
	size = mem->size;

	mem->va = dma_alloc_coherent(ctx->dev->smmu_dev, size, &mem->dma_addr, GFP_KERNEL);

	if (!mem->va) {
		mtk_v4l2_err("%s dma_alloc size=%ld failed!", dev_name(ctx->dev->smmu_dev),
					 size);
		return -ENOMEM;
	}

	memset(mem->va, 0, size);

	mtk_v4l2_debug(4, "[%d]  - va      = %p", ctx->id, mem->va);
	mtk_v4l2_debug(4, "[%d]  - dma     = 0x%lx", ctx->id,
				   (unsigned long)mem->dma_addr);
	mtk_v4l2_debug(4, "[%d]    size = 0x%lx", ctx->id, size);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_mem_alloc);

void mtk_vcodec_mem_free(struct mtk_vcodec_ctx *data,
						 struct mtk_vcodec_mem *mem)
{
	unsigned long size;
	struct mtk_vcodec_ctx *ctx = (struct mtk_vcodec_ctx *)data;

	if (data == NULL || mem == NULL) {
		mtk_v4l2_err("Invalid arguments, data=0x%lx, mem=0x%lx",
			(unsigned long)data, (unsigned long)mem);
		return;
	}
	size = mem->size;

	if (!mem->va) {
		mtk_v4l2_err("%s dma_free size=%ld failed!", dev_name(ctx->dev->smmu_dev),
					 size);
		return;
	}

	mtk_v4l2_debug(4, "[%d]  - va      = %p", ctx->id, mem->va);
	mtk_v4l2_debug(4, "[%d]  - dma     = 0x%lx", ctx->id,
				   (unsigned long)mem->dma_addr);
	mtk_v4l2_debug(4, "[%d]    size = 0x%lx", ctx->id, size);

	dma_free_coherent(ctx->dev->smmu_dev, size, mem->va, mem->dma_addr);
	mem->va = NULL;
	mem->dma_addr = 0;
	mem->size = 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_mem_free);

void mtk_vcodec_set_curr_ctx(struct mtk_vcodec_dev *dev,
	struct mtk_vcodec_ctx *ctx, unsigned int hw_id)
{
	unsigned long flags;

	if (dev == NULL || hw_id >= MTK_VDEC_HW_NUM) {
		mtk_v4l2_err("Invalid arguments, dev=0x%lx, ctx=0x%lx, hw_id=%d",
			(unsigned long)dev, (unsigned long)ctx, hw_id);
		return;
	}

	spin_lock_irqsave(&dev->irqlock, flags);
	dev->curr_dec_ctx[hw_id] = ctx;
	spin_unlock_irqrestore(&dev->irqlock, flags);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_curr_ctx);

struct mtk_vcodec_ctx *mtk_vcodec_get_curr_ctx(struct mtk_vcodec_dev *dev,
	unsigned int hw_id)
{
	unsigned long flags;
	struct mtk_vcodec_ctx *ctx;

	if (!dev || hw_id >= MTK_VDEC_HW_NUM) {
		mtk_v4l2_err("Invalid arguments, dev=0x%lx, hw_id=%d", (unsigned long)dev, hw_id);
		return NULL;
	}

	spin_lock_irqsave(&dev->irqlock, flags);
	ctx = dev->curr_dec_ctx[hw_id];
	spin_unlock_irqrestore(&dev->irqlock, flags);
	return ctx;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_curr_ctx);

void mtk_vcodec_add_ctx_list(struct mtk_vcodec_ctx *ctx)
{
	bool is_first_ctx = false;

	if (ctx != NULL) {
		mutex_lock(&ctx->dev->ctx_mutex);
		is_first_ctx = list_empty(&ctx->dev->ctx_list);
		if (!ctx->add_list_cnt)
			list_add(&ctx->list, &ctx->dev->ctx_list);
		ctx->add_list_cnt++;
		mtk_vcodec_dump_ctx_list(ctx->dev, 4);
		mtk_vcodec_alive_checker_init(ctx, is_first_ctx);
		mutex_unlock(&ctx->dev->ctx_mutex);
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_add_ctx_list);

void mtk_vcodec_del_ctx_list(struct mtk_vcodec_ctx *ctx)
{
	if (ctx != NULL) {
		mutex_lock(&ctx->dev->ctx_mutex);
		mutex_lock(&ctx->ipi_use_lock);
		mtk_vcodec_dump_ctx_list(ctx->dev, 4);
		ctx->add_list_cnt--;
		if (!ctx->add_list_cnt)
			list_del_init(&ctx->list);
		mtk_vcodec_alive_checker_deinit(ctx, list_empty(&ctx->dev->ctx_list));
		mutex_unlock(&ctx->ipi_use_lock);
		mutex_unlock(&ctx->dev->ctx_mutex);
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_del_ctx_list);

void mtk_vcodec_dump_ctx_list(struct mtk_vcodec_dev *dev, unsigned int debug_level)
{
	struct list_head *p, *q;
	struct mtk_vcodec_ctx *ctx;

	if (dev == NULL)
		return;

	list_for_each_safe(p, q, &dev->ctx_list) {
		ctx = list_entry(p, struct mtk_vcodec_ctx, list);
		if (ctx == NULL)
			mtk_v4l2_err("ctx null in ctx list");
		else
			mtk_v4l2_debug(debug_level, "[%d] %s ctx 0x%08lx, drv_handle 0x%08lx %p, state %d, add_list_cnt %d",
				ctx->id, (ctx->type == MTK_INST_DECODER) ? "dec" : "enc",
				(unsigned long)ctx,
				ctx->drv_handle, (void *)ctx->drv_handle,
				mtk_vcodec_get_state(ctx), ctx->add_list_cnt);
	}
}
EXPORT_SYMBOL_GPL(mtk_vcodec_dump_ctx_list);

static void mtk_vcodec_set_uclamp(bool enable, int ctx_id, int pid)
{
	struct task_struct *p, *task_child;
	struct sched_attr attr = {};
	struct cpufreq_policy *policy;
	int cpu = 0, num = 0;

	int ret = -1;

	attr.sched_policy = -1;
	attr.sched_flags =
		SCHED_FLAG_KEEP_ALL |
		SCHED_FLAG_UTIL_CLAMP |
		SCHED_FLAG_RESET_ON_FORK;
	attr.sched_util_max = -1;

	if(enable) {
		attr.sched_util_min = 1024;
		for_each_possible_cpu(cpu) {
			policy = cpufreq_cpu_get(cpu);
			if (!policy)
				continue;
			if (policy->min == policy->max)
				continue;
			mtk_v4l2_debug(0, " [VDVFS][%d] cpu: %d policy: first:%d, min:%d, max:%d",
				ctx_id, cpu, policy->cpu, policy->min, policy->max);
			ret = freq_qos_add_request(&policy->constraints,
					&(freq_min_request[num]), FREQ_QOS_MIN, FREQ_QOS_MAX_DEFAULT_VALUE);
			if (ret < 0)
				mtk_v4l2_debug(0, " [VDVFS][%d] fail to add freq constraint (%d)",
				ctx_id, ret);
			num++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	} else {
		attr.sched_util_min = -1;
		for_each_possible_cpu(cpu) {
			policy = cpufreq_cpu_get(cpu);
			if (!policy)
				continue;
			if (policy->min == policy->max) {
				mtk_v4l2_debug(0, " [VDVFS][%d] cpu: %d policy: first:%d, min:%d, max:%d",
					ctx_id, cpu, policy->cpu, policy->min, policy->max);
				freq_qos_remove_request(&(freq_min_request[num]));
				num++;
				cpu = cpumask_last(policy->related_cpus);
				cpufreq_cpu_put(policy);
			}
		}
	}

	rcu_read_lock();
	p = find_task_by_vpid(pid);

	if (likely(p)) {
		get_task_struct(p);
		attr.sched_policy = p->policy;
		if(p->policy == SCHED_FIFO || p->policy == SCHED_RR)
			attr.sched_priority = p->rt_priority;
		ret = sched_setattr_nocheck(p, &attr);
		for_each_thread(p, task_child) {
			if(task_child) {
				get_task_struct(task_child);
				if(try_get_task_stack(task_child))
					ret = sched_setattr_nocheck(task_child, &attr);
				put_task_struct(task_child);
			}
		}
		put_task_struct(p);
		if(ret != 0)
			mtk_v4l2_err("[VDVFS][%d] set uclamp fail, pid: %d, ret: %d", ctx_id, pid, ret);
	}
	rcu_read_unlock();
};

void mtk_vcodec_set_cpu_hint(struct mtk_vcodec_dev *dev, bool enable,
	enum mtk_instance_type type, int ctx_id, int cpu_caller_pid, const char *debug_str)
{
	vcodec_trace_begin("%s(%d)(%s)", __func__, enable, debug_str);

	mutex_lock(&dev->cpu_hint_mutex);
	if (enable) {
		if (dev->cpu_hint_mode & (1 << MTK_GRP_AWARE_MODE)) { // cpu grp awr mode
			if (dev->cpu_hint_ref_cnt == 0) {
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
				set_top_grp_aware(1, 0);
				set_grp_awr_min_opp_margin(0, 0, 2560);
				set_grp_awr_thr(0, 0, 1680000);
				set_grp_awr_min_opp_margin(1, 0, 2560);
				set_grp_awr_thr(1, 0, 2240000);
#endif
			}
		}
		if (dev->cpu_hint_mode & (1 << MTK_UCLAMP_MODE)) // uclamp mode
			mtk_vcodec_set_uclamp(enable, ctx_id, cpu_caller_pid);

		dev->cpu_hint_ref_cnt++;
		mtk_v4l2_debug(0, " [VDVFS][%d][%s] enable CPU hint by %s (ref cnt %d, mode %d)",
			ctx_id, (type == MTK_INST_DECODER) ? "VDEC" : "VENC",
			debug_str, dev->cpu_hint_ref_cnt, dev->cpu_hint_mode);
	} else {
		dev->cpu_hint_ref_cnt--;
		if (dev->cpu_hint_mode & (1 << MTK_GRP_AWARE_MODE)) {
			if (dev->cpu_hint_ref_cnt == 0) {
#if IS_ENABLED(CONFIG_MTK_SCHED_FAST_LOAD_TRACKING)
				set_top_grp_aware(0, 0);
#endif
			}
		}
		if (dev->cpu_hint_mode & (1 << MTK_UCLAMP_MODE))
			mtk_vcodec_set_uclamp(enable, ctx_id, cpu_caller_pid);

		mtk_v4l2_debug(0, "[VDVFS][%d][%s] disable CPU hint by %s (ref cnt %d mode %d)",
			ctx_id, (type == MTK_INST_DECODER) ? "VDEC" : "VENC", debug_str,
			dev->cpu_hint_ref_cnt, dev->cpu_hint_mode);
	}
	mutex_unlock(&dev->cpu_hint_mutex);

	vcodec_trace_end();
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_cpu_hint);

void mtk_vcodec_init_slice_info(struct mtk_vcodec_ctx *ctx, struct mtk_video_dec_buf *dst_buf_info)
{
	struct vb2_v4l2_buffer *dst_buf;
	struct vdec_fb *pfb;
	struct dma_buf *dbuf;
	struct dma_fence *fence;

	if (ctx == NULL || dst_buf_info == NULL) {
		mtk_v4l2_err("Invalid arguments, ctx=0x%lx, dst_buf_info=0x%lx", (unsigned long)ctx, (unsigned long)dst_buf_info);
		return;
	}
	dst_buf = &dst_buf_info->vb;
	pfb = &dst_buf_info->frame_buffer;
	dbuf = dst_buf->vb2_buf.planes[0].dbuf;

	pfb->slice_done_count = 0;
	fence = mtk_vcodec_create_fence(ctx->dec_params.slice_count);
	if (fence) {
		dma_resv_lock(dbuf->resv, NULL);
		dma_resv_add_fence(dbuf->resv, fence, DMA_RESV_USAGE_KERNEL);
		dma_resv_unlock(dbuf->resv);
		dma_fence_put(fence); // make dbuf->resv the only owner
	}
}
EXPORT_SYMBOL(mtk_vcodec_init_slice_info);

struct vdec_fb *mtk_vcodec_get_fb(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *dst_buf = NULL;
	struct vdec_fb *pfb;
	struct mtk_video_dec_buf *dst_buf_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2;
	int i;

	if (!ctx) {
		mtk_v4l2_err("Ctx is NULL!");
		return NULL;
	}

	mtk_v4l2_debug_enter();

	mutex_lock(&ctx->buf_lock);
	dst_vb2_v4l2 = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	if (dst_vb2_v4l2 != NULL)
		dst_buf = &dst_vb2_v4l2->vb2_buf;
	if (dst_buf != NULL) {
		dst_buf_info = container_of(dst_vb2_v4l2, struct mtk_video_dec_buf, vb);
		pfb = &dst_buf_info->frame_buffer;
		pfb->num_planes = dst_vb2_v4l2->vb2_buf.num_planes;
		pfb->index = dst_buf->index;

		for (i = 0; i < dst_vb2_v4l2->vb2_buf.num_planes; i++) {
			if (mtk_v4l2_dbg_level > 0)
				pfb->fb_base[i].va = vb2_plane_vaddr(dst_buf, i);
			pfb->fb_base[i].dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, i);
			pfb->fb_base[i].size = ctx->picinfo.fb_sz[i];
			pfb->fb_base[i].length = dst_buf->planes[i].length;
			pfb->fb_base[i].dmabuf = dst_buf->planes[i].dbuf;
			if (dst_buf_info->used == false) {
				get_file(dst_buf->planes[i].dbuf->file);
				mtk_v4l2_debug(4, "[Ref cnt] id=%d Ref get dma %p", dst_buf->index,
					dst_buf->planes[i].dbuf);
			}
		}
		pfb->status = FB_ST_INIT;
		dst_buf_info->used = true;
		mtk_vcodec_init_slice_info(ctx, dst_buf_info);
		ctx->fb_list[pfb->index + 1] = (uintptr_t)pfb;

		mtk_v4l2_debug(1, "[%d] id=%d pfb=0x%p %llx VA=%p dma_addr[0]=%lx dma_addr[1]=%lx Size=%zx fd:%x, dma_general_buf = %p, general_buf_fd = %d, num_rdy_bufs=%d",
			ctx->id, dst_buf->index, pfb, (unsigned long long)pfb, pfb->fb_base[0].va,
			(unsigned long)pfb->fb_base[0].dma_addr,
			(unsigned long)pfb->fb_base[1].dma_addr,
			pfb->fb_base[0].size, dst_buf->planes[0].m.fd,
			pfb->dma_general_buf, pfb->general_buf_fd,
			v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx));
	} else {
		mtk_v4l2_debug(8, "[%d] No free framebuffer in v4l2!!\n", ctx->id);
		pfb = NULL;
	}
	mutex_unlock(&ctx->buf_lock);

	mtk_v4l2_debug_leave();

	return pfb;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_fb);

struct mtk_vcodec_mem *mtk_vcodec_get_bs(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *dst_buf;
	struct mtk_vcodec_mem *pbs_buf;
	struct mtk_video_enc_buf *dst_buf_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2;

	if (!ctx) {
		mtk_v4l2_err("Ctx is NULL!");
		return NULL;
	}

	mtk_v4l2_debug_enter();
	dst_vb2_v4l2 = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (dst_vb2_v4l2 == NULL) {
		mtk_v4l2_err("[%s][%d]dst_buf empty!!", __func__, ctx->id);
		return NULL;
	}
	dst_buf = &dst_vb2_v4l2->vb2_buf;
	if (dst_buf != NULL) {
		dst_buf_info = container_of(dst_vb2_v4l2, struct mtk_video_enc_buf, vb);
		pbs_buf = &dst_buf_info->bs_buf;

		if (!ctx->enc_params.svp_mode && mtk_v4l2_dbg_level > 0)
			pbs_buf->va = vb2_plane_vaddr(dst_buf, 0);
		pbs_buf->dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
		pbs_buf->size = (size_t)dst_buf->planes[0].length;
		pbs_buf->dmabuf = dst_buf->planes[0].dbuf;

		dst_vb2_v4l2 = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		dst_buf = &dst_vb2_v4l2->vb2_buf;
		if (dst_buf != NULL) {
			mtk_v4l2_debug(8, "[%d] index=%d, num_rdy_bufs=%d, dma_general_buf = %p, general_buf_fd = %d\n",
				ctx->id, dst_buf->index,
				v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx),
				pbs_buf->dma_general_buf,
				pbs_buf->general_buf_fd);
		}

	} else {
		mtk_v4l2_err("[%d] No free framebuffer in v4l2!!\n", ctx->id);
		pbs_buf = NULL;
	}
	mtk_v4l2_debug_leave();

	return pbs_buf;
}
EXPORT_SYMBOL(mtk_vcodec_get_bs);

int v4l2_m2m_buf_queue_check(struct v4l2_m2m_ctx *m2m_ctx,
		void *vbuf)
{
	struct v4l2_m2m_buffer *b;

	if (vbuf == NULL) {
		mtk_v4l2_err("Invalid arguments, m2m_ctx=0x%lx, vbuf=0x%lx",
			(unsigned long)m2m_ctx, (unsigned long)vbuf);
		return -1;
	}
	b = container_of(vbuf, struct v4l2_m2m_buffer, vb);
	mtk_v4l2_debug(8, "[Debug] b %p b->list.next %p prev %p %p %p\n",
		b, b->list.next, b->list.prev,
		LIST_POISON1, LIST_POISON2);

	if (WARN_ON(IS_ERR_OR_NULL(m2m_ctx) ||
		(b->list.next != LIST_POISON1 && b->list.next) ||
		(b->list.prev != LIST_POISON2 && b->list.prev))) {
		v4l2_aee_print("b %p next %p prev %p already in rdyq %p %p\n",
			b, b->list.next, b->list.prev,
			LIST_POISON1, LIST_POISON2);
		return -1;
	}
	vcodec_trace_begin_func();
	v4l2_m2m_buf_queue(m2m_ctx, vbuf);
	vcodec_trace_end();
	return 0;
}
EXPORT_SYMBOL_GPL(v4l2_m2m_buf_queue_check);

int mtk_dma_sync_sg_range(const struct sg_table *sgt,
	struct device *dev, unsigned int size,
	enum dma_data_direction direction)
{
	struct sg_table *sgt_tmp;
	struct scatterlist *s_sgl, *d_sgl;
	unsigned int contig_size = 0;
	int ret, i;

	if (sgt == NULL || dev == NULL) {
		mtk_v4l2_err("sgt or dev is invalid");
		return -1;
	}
	if (size == 0) {
		mtk_v4l2_debug(1, "size %d no need to cache sync", size);
		return 0;
	}

	sgt_tmp = kzalloc(sizeof(*sgt_tmp), GFP_KERNEL);
	if (!sgt_tmp)
		return -1;

	ret = sg_alloc_table(sgt_tmp, sgt->orig_nents, GFP_KERNEL);
	if (ret) {
		mtk_v4l2_debug(0, "sg alloc table failed %d.\n", ret);
		kfree(sgt_tmp);
		return -1;
	}
	sgt_tmp->nents = 0;
	d_sgl = sgt_tmp->sgl;

	for_each_sg(sgt->sgl, s_sgl, sgt->orig_nents, i) {
		mtk_v4l2_debug(4, "%d contig_size %d bytesused %d.\n",
			i, contig_size, size);
		if (contig_size >= size)
			break;
		memcpy(d_sgl, s_sgl, sizeof(*s_sgl));
		contig_size += s_sgl->length;
		d_sgl = sg_next(d_sgl);
		sgt_tmp->nents++;
	}
	if (direction == DMA_TO_DEVICE) {
		dma_sync_sg_for_device(dev, sgt_tmp->sgl, sgt_tmp->nents, direction);
	} else if (direction == DMA_FROM_DEVICE) {
		dma_sync_sg_for_cpu(dev, sgt_tmp->sgl, sgt_tmp->nents, direction);
	} else {
		mtk_v4l2_debug(0, "direction %d not correct\n", direction);
		return -1;
	}
	mtk_v4l2_debug(4, "flush nents %d total nents %d\n",
		sgt_tmp->nents, sgt->orig_nents);
	sg_free_table(sgt_tmp);
	kfree(sgt_tmp);

	return 0;
}
EXPORT_SYMBOL(mtk_dma_sync_sg_range);

void v4l_fill_mtk_fmtdesc(struct v4l2_fmtdesc *fmt)
{
	const char *descr = NULL;

	if (fmt == NULL) {
		mtk_v4l2_err("Invalid arguments, fmt=0x%lx", (unsigned long)fmt);
		return;
	}

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_H265:
	    descr = "H.265"; break;
	case V4L2_PIX_FMT_HEIF:
	    descr = "HEIF"; break;
	case V4L2_PIX_FMT_WMV1:
	    descr = "WMV1"; break;
	case V4L2_PIX_FMT_WMV2:
	    descr = "WMV2"; break;
	case V4L2_PIX_FMT_WMV3:
	    descr = "WMV3"; break;
	case V4L2_PIX_FMT_WVC1:
	    descr = "WVC1"; break;
	case V4L2_PIX_FMT_WMVA:
	    descr = "WMVA"; break;
	case V4L2_PIX_FMT_RV30:
	    descr = "RealVideo 8"; break;
	case V4L2_PIX_FMT_RV40:
	    descr = "RealVideo 9/10"; break;
	case V4L2_PIX_FMT_AV1:
	    descr = "AV1"; break;
	case V4L2_PIX_FMT_MT10S:
	    descr = "MTK 10-bit compressed single"; break;
	case V4L2_PIX_FMT_MT10:
	    descr = "MTK 10-bit compressed"; break;
	case V4L2_PIX_FMT_P010S:
	    descr = "10-bit P010 LSB 6-bit single"; break;
	case V4L2_PIX_FMT_P010M:
	    descr = "10-bit P010 LSB 6-bit"; break;
	case V4L2_PIX_FMT_NV12_AFBC:
	    descr = "AFBC NV12"; break;
	case V4L2_PIX_FMT_NV21_AFBC:
	    descr = "AFBC NV21"; break;
	case V4L2_PIX_FMT_NV12_10B_AFBC:
	    descr = "10-bit AFBC NV12"; break;
	case V4L2_PIX_FMT_RGB32_AFBC:
	    descr = "32-bit AFBC A/XRGB 8-8-8-8"; break;
	case V4L2_PIX_FMT_BGR32_AFBC:
	    descr = "32-bit AFBC A/XBGR 8-8-8-8"; break;
	case V4L2_PIX_FMT_RGBA1010102_AFBC:
	    descr = "10-bit AFBC RGB 2-bit for A"; break;
	case V4L2_PIX_FMT_BGRA1010102_AFBC:
	    descr = "10-bit AFBC BGR 2-bit for A"; break;
	case V4L2_PIX_FMT_ARGB1010102:
	case V4L2_PIX_FMT_ABGR1010102:
	case V4L2_PIX_FMT_RGBA1010102:
	case V4L2_PIX_FMT_BGRA1010102:
	    descr = "10-bit for RGB, 2-bit for A"; break;
	case V4L2_PIX_FMT_NV12_HYFBC:
		descr = "8-bit yuv 420 HyFBC"; break;
	case V4L2_PIX_FMT_P010_HYFBC:
		descr = "10-bit yuv 420 HyFBC"; break;
	case V4L2_PIX_FMT_MT21:
	case V4L2_PIX_FMT_MT2110T:
	case V4L2_PIX_FMT_MT2110R:
	case V4L2_PIX_FMT_MT21C10T:
	case V4L2_PIX_FMT_MT21C10R:
	case V4L2_PIX_FMT_MT21CS:
	case V4L2_PIX_FMT_MT21S:
	case V4L2_PIX_FMT_MT21S10T:
	case V4L2_PIX_FMT_MT21S10R:
	case V4L2_PIX_FMT_MT21CS10T:
	case V4L2_PIX_FMT_MT21CS10R:
	case V4L2_PIX_FMT_MT21CSA:
	case V4L2_PIX_FMT_MT21S10TJ:
	case V4L2_PIX_FMT_MT21S10RJ:
	case V4L2_PIX_FMT_MT21CS10TJ:
	case V4L2_PIX_FMT_MT21CS10RJ:
		descr = "Mediatek Video Block Format"; break;
	default:
		mtk_v4l2_debug(8, "MTK Unknown pixelformat 0x%08x\n", fmt->pixelformat);
		break;
	}

	if (descr)
		WARN_ON(strscpy(fmt->description, descr, sizeof(fmt->description)) < 0);
}
EXPORT_SYMBOL_GPL(v4l_fill_mtk_fmtdesc);

#if IS_ENABLED(CONFIG_MTK_TINYSYS_VCP_SUPPORT)
#define VCP_CACHE_LINE 128
int mtk_vcodec_alloc_mem(struct vcodec_mem_obj *mem, struct device *dev,
	struct dma_buf_attachment **attach, struct sg_table **sgt, enum mtk_instance_type fmt)
{
	struct dma_heap *dma_heap;
	struct dma_buf *dbuf;
	dma_addr_t dma_addr;
	__u32 alloc_len;
	int ret = 0;
	void *ret_ptr = NULL;

	if (mem == NULL || dev == NULL || attach == NULL || sgt == NULL) {
		mtk_v4l2_err("Invalid arguments, mem=0x%lx, dev=0x%lx, attach=0x%lx, sgt=0x%lx",
			(unsigned long)mem, (unsigned long)dev, (unsigned long)attach, (unsigned long)sgt);
		return -EINVAL;
	}
	alloc_len = mem->len;

	mem->iova = 0;
	if (dev == NULL) {
		mtk_v4l2_err("dev null when type %u", mem->type);
		return -EPERM;
	}
	if (mem->len > CODEC_ALLOCATE_MAX_BUFFER_SIZE || mem->len == 0U) {
		mtk_v4l2_err("buffer len = %u invalid", mem->len);
		return -EPERM;
	}

	if (mem->type == MEM_TYPE_FOR_SW ||
	    mem->type == MEM_TYPE_FOR_HW ||
	    mem->type == MEM_TYPE_FOR_UBE_HW) {
		dma_heap = dma_heap_find("mtk_mm");
	} else if (mem->type == MEM_TYPE_FOR_SEC_SW ||
		   mem->type == MEM_TYPE_FOR_SEC_HW ||
		   mem->type == MEM_TYPE_FOR_SEC_UBE_HW) {
		if (support_svp_region)
			dma_heap = dma_heap_find("mtk_svp_region-aligned");
		else
			dma_heap = dma_heap_find("mtk_svp_page-uncached");
	} else if (mem->type == MEM_TYPE_FOR_SEC_WFD_HW) {
		if (support_wfd_region)
			dma_heap = dma_heap_find("mtk_wfd_region-aligned");
		else
			dma_heap = dma_heap_find("mtk_wfd_page-uncached");
	} else {
		mtk_v4l2_err("wrong type %u", mem->type);
		return -EPERM;
	}

	if (!dma_heap) {
		mtk_v4l2_err("heap find fail");
		return -EPERM;
	}

	if (mem->type == MEM_TYPE_FOR_SW ||
		mem->type == MEM_TYPE_FOR_SEC_SW) {
		alloc_len = (ROUND_N(mem->len, VCP_CACHE_LINE) + VCP_CACHE_LINE);
	}

	dbuf = dma_heap_buffer_alloc(dma_heap, alloc_len,
		O_RDWR | O_CLOEXEC, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR_OR_NULL(dbuf)) {
		mtk_v4l2_err("buffer alloc fail");
		ret_ptr = (void *)dbuf;
		goto alloc_mem_err;
	}

	if (fmt == MTK_INST_DECODER)
		mtk_dma_buf_set_name(dbuf, "vdec_working");
	else
		mtk_dma_buf_set_name(dbuf, "venc_working");

	*attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR_OR_NULL(*attach)) {
		mtk_v4l2_err("attach fail, return");
		ret_ptr = (void *)*attach;
		goto alloc_mem_err_attach_fail;
	}
	*sgt = dma_buf_map_attachment(*attach, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(*sgt)) {
		mtk_v4l2_err("map failed, detach and return");
		ret_ptr = (void *)*sgt;
		goto alloc_mem_err_map_fail;
	}

	dma_addr = sg_dma_address((*sgt)->sgl);
	if (dbuf == NULL || dma_addr == 0) {
		mtk_v4l2_err("alloc failed, va 0x%llx pa 0x%llx iova 0x%llx len %d type %u",
			(__u64)dbuf, (__u64)dma_addr, (__u64)dma_addr, mem->len, mem->type);
		ret = -EPERM;
		goto alloc_mem_err_after_map_done;
	}
	mem->va = (__u64)dbuf;
	mem->pa = (__u64)dma_addr;
	mem->iova = mem->pa;
	mtk_v4l2_debug(8, "va 0x%llx pa 0x%llx iova 0x%llx len %d type %u",
		mem->va, mem->pa, mem->iova, mem->len, mem->type);

	return 0;

alloc_mem_err_after_map_done:
	dma_buf_unmap_attachment(*attach, *sgt, DMA_BIDIRECTIONAL);
alloc_mem_err_map_fail:
	dma_buf_detach(dbuf, *attach);
alloc_mem_err_attach_fail:
	dma_heap_buffer_free(dbuf);
alloc_mem_err:
	if (ret_ptr && IS_ERR(ret_ptr))
		ret = PTR_ERR(ret_ptr);
	else if (ret == 0)
		ret = -EPERM;
	return ret;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_alloc_mem);

int mtk_vcodec_free_mem(struct vcodec_mem_obj *mem, struct device *dev,
	struct dma_buf_attachment *attach, struct sg_table *sgt)
{
	if (mem == NULL || dev == NULL || attach == NULL || sgt == NULL) {
		mtk_v4l2_err("Invalid arguments, mem=0x%lx, dev=0x%lx, attach=0x%lx, sgt=0x%lx",
			(unsigned long)mem, (unsigned long)dev, (unsigned long)attach, (unsigned long)sgt);
		return -EINVAL;
	}

	if (mem->type == MEM_TYPE_FOR_SW ||
		mem->type == MEM_TYPE_FOR_HW ||
		mem->type == MEM_TYPE_FOR_UBE_HW ||
		mem->type == MEM_TYPE_FOR_SEC_SW ||
		mem->type == MEM_TYPE_FOR_SEC_HW ||
		mem->type == MEM_TYPE_FOR_SEC_UBE_HW ||
		mem->type == MEM_TYPE_FOR_SEC_WFD_HW) {
		dma_buf_unmap_attachment(attach, sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach((struct dma_buf *)mem->va, attach);
		dma_heap_buffer_free((struct dma_buf *)mem->va);
	} else {
		mtk_v4l2_err("wrong type %d\n", mem->type);
		return -EPERM;
	}

	mtk_v4l2_debug(8, "va 0x%llx pa 0x%llx iova 0x%llx len %d\n",
		mem->va, mem->pa, mem->iova, mem->len);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_vcodec_free_mem);

unsigned char hyfbc_10bit_black_pattern_y[192] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

unsigned char hyfbc_10bit_black_pattern_c[192] = {
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void mtk_vcodec_vp_mode_buf_prepare(struct mtk_vcodec_dev *dev)
{
	unsigned int j = 0;
	int i = 0;
	struct vdec_vp_mode_buf *src_buf;
	struct iosys_map map;
	void *va = NULL;
	struct dma_buf *dmabuf = NULL;
	struct device *io_dev = NULL;
	int ret;
	unsigned char *pattern;

	if (dev == NULL) {
		mtk_v4l2_err("Invalid argument");
		return;
	}

	mutex_lock(&dev->vp_mode_buf_mutex);
	if (dev->vp_mode_used_cnt > 0) {
		dev->vp_mode_used_cnt++;
		mtk_v4l2_debug(4, "vp mode y buf iova: 0x%llx, c buf iova: 0x%llx, used_cnt %d",
			dev->vp_mode_buf[0].mem.iova, dev->vp_mode_buf[1].mem.iova,
			dev->vp_mode_used_cnt);
		mutex_unlock(&dev->vp_mode_buf_mutex);
		return;
	}

	io_dev = dev->smmu_dev;
	for (i = 0; i < 2; i++) {
		switch (i) {
		case 0:
			pattern = hyfbc_10bit_black_pattern_y;
			break;
		case 1:
			pattern = hyfbc_10bit_black_pattern_c;
			break;
		}

		src_buf = &dev->vp_mode_buf[i];
		src_buf->mem.len = 192 * 16 * 1024 + 16;
		src_buf->mem.type = MEM_TYPE_FOR_HW;

		ret = mtk_vcodec_alloc_mem(&src_buf->mem, io_dev, &src_buf->attach, &src_buf->sgt, MTK_INST_DECODER);
		if (ret) {
			mtk_v4l2_err("vp mode buf %d alloc failed", i);
			goto err_out;
		}

		/* mapping va */
		memset(&map, 0, sizeof(struct iosys_map));
		dmabuf = (struct dma_buf *)src_buf->mem.va;
		ret = dma_buf_vmap(dmabuf, &map);
		va = ret ? NULL : map.vaddr;
		if (va == NULL) {
			mtk_v4l2_err("vp mode buf %d dma vmap failed", i);
			goto err_out;
		}

		/* fill working buffer */
		for (j = 0; j < src_buf->mem.len; j += 192)
			memcpy((va + j), pattern, 192);
		dma_sync_sg_for_device(io_dev, src_buf->sgt->sgl,
			src_buf->sgt->nents, DMA_TO_DEVICE);
		dma_buf_vunmap(dmabuf, &map);
	}
	dev->vp_mode_used_cnt = 1;
	mutex_unlock(&dev->vp_mode_buf_mutex);
	return;

err_out:
	for (i = 0; i < 2; i++) {
		src_buf = &dev->vp_mode_buf[i];
		if (!IS_ERR_OR_NULL(ERR_PTR((long)src_buf->mem.va)))
			mtk_vcodec_free_mem(&src_buf->mem, io_dev, src_buf->attach, src_buf->sgt);
		memset(src_buf, 0, sizeof(struct vdec_vp_mode_buf));
	}
	mutex_unlock(&dev->vp_mode_buf_mutex);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_vp_mode_buf_prepare);

void mtk_vcodec_vp_mode_buf_unprepare(struct mtk_vcodec_dev *dev)
{
	struct vdec_vp_mode_buf *src_buf;
	struct device *io_dev;
	int i = 0;

	if (dev == NULL) {
		mtk_v4l2_err("Invalid argument");
		return;
	}

	mutex_lock(&dev->vp_mode_buf_mutex);
	if (dev->vp_mode_used_cnt > 1) {
		dev->vp_mode_used_cnt--;
		mtk_v4l2_debug(4, "vp mode buf remaining used_cnt %d", dev->vp_mode_used_cnt);
		mutex_unlock(&dev->vp_mode_buf_mutex);
		return;
	}

	mtk_v4l2_debug(2, "vp mode buf free y 0x%llx, c 0x%llx",
		dev->vp_mode_buf[0].mem.iova, dev->vp_mode_buf[1].mem.iova);
	io_dev = dev->smmu_dev;
	for (i = 0; i < 2; i++) {
		src_buf = &dev->vp_mode_buf[i];
		if (!IS_ERR_OR_NULL(ERR_PTR((long)src_buf->mem.va)))
			mtk_vcodec_free_mem(&src_buf->mem, io_dev, src_buf->attach, src_buf->sgt);
		memset(src_buf, 0, sizeof(struct vdec_vp_mode_buf));
	}

	dev->vp_mode_used_cnt = 0;
	mutex_unlock(&dev->vp_mode_buf_mutex);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_vp_mode_buf_unprepare);
#endif

static void mtk_vcodec_sync_log(struct mtk_vcodec_dev *dev,
	const char *param_key, const char *param_val, enum mtk_vcodec_log_index index)
{
	struct mtk_vcodec_log_param *pram, *tmp;
	struct list_head *plist;
	struct mutex *plist_mutex;

	if (index == MTK_VCODEC_LOG_INDEX_LOG) {
		plist = &dev->log_param_list;
		plist_mutex = &dev->log_param_mutex;
	} else if (index == MTK_VCODEC_LOG_INDEX_PROP) {
		plist = &dev->prop_param_list;
		plist_mutex = &dev->prop_param_mutex;
	} else {
		mtk_v4l2_err("invalid index: %d", index);
		return;
	}

	mutex_lock(plist_mutex);

	list_for_each_entry(pram, plist, list) {
		// find existed param, replace its value
		if (strcmp(pram->param_key, param_key) == 0) {
			mtk_v4l2_debug(8, "replace old key: %s, value: %s -> %s\n",
				pram->param_key, pram->param_val, param_val);
			SNPRINTF(pram->param_val, LOG_PARAM_INFO_SIZE, "%s", param_val);
			// move to list head
			list_del(&pram->list);
			list_add(&pram->list, plist);
			mutex_unlock(plist_mutex);
			return;
		}
	}

	// cannot find, add new
	pram = kzalloc(sizeof(*pram), GFP_KERNEL);
	SNPRINTF(pram->param_key, LOG_PARAM_INFO_SIZE, "%s", param_key);
	SNPRINTF(pram->param_val, LOG_PARAM_INFO_SIZE, "%s", param_val);
	mtk_v4l2_debug(8, "add new key: %s, value: %s\n",
		pram->param_key, pram->param_val);
	list_add(&pram->list, plist);

	// remove disabled log param from list if value is empty
	list_for_each_entry_safe(pram, tmp, plist, list) {
		pram->param_val[LOG_PARAM_INFO_SIZE - 1] = 0;
		if (strlen(pram->param_val) == 0) {
			mtk_v4l2_debug(8, "remove deprecated key: %s, value: %s\n",
				pram->param_key, pram->param_val);
			list_del_init(&pram->list);
			kfree(pram);
		}
	}
	mutex_unlock(plist_mutex);
}

static void mtk_vcodec_build_log_string(struct mtk_vcodec_dev *dev,
	enum mtk_vcodec_log_index log_index)
{
	struct mtk_vcodec_log_param *pram;
	struct list_head *plist;
	struct mutex *plist_mutex;
	char *temp_str;
	char temp_info[LOG_PARAM_INFO_SIZE * 2 + 2] = {0};
	int str_len = 0, temp_info_len = 0, temp_len, ret;

	if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
		plist = &dev->log_param_list;
		plist_mutex = &dev->log_param_mutex;
		if (dev->vfd_dec)
			temp_str = mtk_vdec_tmp_log;
		else
			temp_str = mtk_venc_tmp_log;
	} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
		plist = &dev->prop_param_list;
		plist_mutex = &dev->prop_param_mutex;
		if (dev->vfd_dec)
			temp_str = mtk_vdec_tmp_prop;
		else
			temp_str = mtk_venc_tmp_prop;
	} else {
		mtk_v4l2_err("invalid log_index: %d", log_index);
		return;
	}

	mutex_lock(plist_mutex);

	memset(temp_str, 0x00, LOG_PROPERTY_SIZE);
	str_len = 0;
	list_for_each_entry(pram, plist, list) {
		mtk_v4l2_debug(8, "existed log param: %s %s\n",
				pram->param_key, pram->param_val);
		ret = snprintf(temp_info, LOG_PARAM_INFO_SIZE * 2 + 2,
			"%s %s", pram->param_key, pram->param_val);
		if (ret <= 0) {
			mtk_v4l2_err("temp_info err usage: snprintf ret %d", ret);
			continue;
		}
		temp_info_len = strlen(temp_info);
		if (temp_info_len > 0 && str_len + 1 + temp_info_len < LOG_PROPERTY_SIZE) {
			if (str_len == 0)
				temp_len = snprintf(temp_str, LOG_PROPERTY_SIZE, "%s", temp_info);
			else
				temp_len = snprintf(temp_str + str_len, LOG_PROPERTY_SIZE - str_len,
					" %s", temp_info);
			if (temp_len > 0)
				str_len += temp_len;
			else
				mtk_v4l2_err("temp_str err usage: temp_len: %d", temp_len);
		} else
			mtk_v4l2_err("temp_str err usage: str_len: %d, temp_info_len: %d",
				str_len, temp_info_len);
	}

	if (dev->vfd_dec) {
		if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
			mtk_vdec_vcp_log = temp_str;
			mtk_v4l2_debug(8, "build mtk_vdec_vcp_log: %s\n", mtk_vdec_vcp_log);
		} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
			mtk_vdec_property = temp_str;
			mtk_v4l2_debug(8, "build mtk_vdec_property: %s\n", mtk_vdec_property);
		}
	} else {
		if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
			mtk_venc_vcp_log = temp_str;
			mtk_v4l2_debug(8, "build mtk_venc_vcp_log: %s\n", mtk_venc_vcp_log);
		} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
			mtk_venc_property = temp_str;
			mtk_v4l2_debug(8, "build mtk_venc_property: %s\n", mtk_venc_property);
		}
	}
	mutex_unlock(plist_mutex);
}

void mtk_vcodec_set_log(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_dev *dev,
	const char *val, enum mtk_vcodec_log_index log_index,
	void (*set_vcu_vpud_log)(struct mtk_vcodec_ctx *ctx, void *in))
{
	int i, argc = 0;
	char (*argv)[LOG_PARAM_INFO_SIZE] = NULL;
	char *temp = NULL;
	char *token = NULL;
	long temp_val = 0;
	char *log = NULL;
	char vcu_log[128] = {0};

	if (dev == NULL || val == NULL || strlen(val) == 0)
		return;

	mtk_v4l2_debug(0, "val: %s, log_index: %d", val, log_index);

	argv = kzalloc(MAX_SUPPORTED_LOG_PARAMS_COUNT * 2 * LOG_PARAM_INFO_SIZE, GFP_KERNEL);
	if (!argv)
		return;
	log = kzalloc(LOG_PROPERTY_SIZE, GFP_KERNEL);
	if (!log) {
		kfree(argv);
		return;
	}

	SNPRINTF(log, LOG_PROPERTY_SIZE, "%s", val);
	temp = log;
	for (token = strsep(&temp, "\n\r ");
	     token != NULL && argc < MAX_SUPPORTED_LOG_PARAMS_COUNT * 2;
	     token = strsep(&temp, "\n\r ")) {
		if (strlen(token) == 0)
			continue;
		SNPRINTF(argv[argc], LOG_PARAM_INFO_SIZE, "%s", token);
		argc++;
	}

	for (i = 0; i < argc-1; i += 2) {
		if (strlen(argv[i]) == 0)
			continue;
		if (strcmp("-mtk_vcodec_dbg", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				mtk_vcodec_dbg = temp_val;
				mtk_v4l2_debug(0, "mtk_vcodec_dbg: %d\n", mtk_vcodec_dbg);
			}
		} else if (strcmp("-mtk_vcodec_perf", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				mtk_vcodec_perf = temp_val;
				mtk_v4l2_debug(0, "mtk_vcodec_perf: %d\n", mtk_vcodec_perf);
			}
		} else if (strcmp("-mtk_v4l2_dbg_level", argv[i]) == 0) {
			if (kstrtol(argv[i+1], 0, &temp_val) == 0) {
				mtk_v4l2_dbg_level = temp_val;
				mtk_v4l2_debug(0, "mtk_v4l2_dbg_level: %d\n", mtk_v4l2_dbg_level);
			}
		} else {
			// uP path
			if ((dev->vfd_dec && mtk_vcodec_is_vcp(MTK_INST_DECODER))
				|| (dev->vfd_enc && mtk_vcodec_is_vcp(MTK_INST_ENCODER))) {
				mtk_vcodec_sync_log(dev, argv[i], argv[i+1], log_index);
			} else { // vcu path
				if (ctx == NULL) {
					mtk_v4l2_err("ctx is null, cannot set log to vpud");
					kfree(argv);
					kfree(log);
					return;
				}
				if (log_index != MTK_VCODEC_LOG_INDEX_LOG) {
					mtk_v4l2_err(
						"invalid index: %d, only support set log on vcu path",
						log_index);
					kfree(argv);
					kfree(log);
					return;
				}
				memset(vcu_log, 0x00, sizeof(vcu_log));
				SNPRINTF(vcu_log, sizeof(vcu_log), "%s %s", argv[i], argv[i+1]);
				if (set_vcu_vpud_log != NULL)
					set_vcu_vpud_log(ctx, vcu_log);
			}
		}
	}

	// uP path
	if (mtk_vcodec_is_vcp(MTK_INST_DECODER) || mtk_vcodec_is_vcp(MTK_INST_ENCODER))
		mtk_vcodec_build_log_string(dev, log_index);

	kfree(argv);
	kfree(log);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_set_log);

void mtk_vcodec_get_log(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_dev *dev,
	char *val, enum mtk_vcodec_log_index log_index,
	void (*get_vcu_vpud_log)(struct mtk_vcodec_ctx *ctx, void *out))
{
	int len = 0;

	if (!dev || !val) {
		mtk_v4l2_err("Invalid arguments, dev=0x%lx, val=0x%lx",
			(unsigned long)dev, (unsigned long)val);
		return;
	}

	memset(val, 0x00, LOG_PROPERTY_SIZE);

	if ((dev->vfd_dec && mtk_vcodec_is_vcp(MTK_INST_DECODER))
		|| (dev->vfd_enc && mtk_vcodec_is_vcp(MTK_INST_ENCODER))) { // uP path
		if (dev->vfd_dec) {
			if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
				SNPRINTF(val, LOG_PROPERTY_SIZE, "%s", mtk_vdec_vcp_log);
			} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
				SNPRINTF(val, LOG_PROPERTY_SIZE, "%s", mtk_vdec_property);
			} else {
				mtk_v4l2_err("invalid index: %d", log_index);
				return;
			}
		} else {
			if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
				SNPRINTF(val, LOG_PROPERTY_SIZE, "%s", mtk_venc_vcp_log);
			} else if (log_index == MTK_VCODEC_LOG_INDEX_PROP) {
				SNPRINTF(val, LOG_PROPERTY_SIZE, "%s", mtk_venc_property);
			} else {
				mtk_v4l2_err("invalid index: %d", log_index);
				return;
			}
		}
	} else { // vcu path
		if (ctx == NULL) {
			mtk_v4l2_err("ctx is null, cannot set log to vpud");
			return;
		}
		if (log_index != MTK_VCODEC_LOG_INDEX_LOG) {
			mtk_v4l2_err(
				"invalid index: %d, only support get log on vcu path", log_index);
			return;
		}

		if (get_vcu_vpud_log != NULL)
			get_vcu_vpud_log(ctx, val);
	}

	// join kernel log level
	if (log_index == MTK_VCODEC_LOG_INDEX_LOG) {
		len = strlen(val);
		if (len < LOG_PROPERTY_SIZE)
			SNPRINTF(val + len, LOG_PROPERTY_SIZE - len,
				" %s %d", "-mtk_vcodec_dbg", mtk_vcodec_dbg);

		len = strlen(val);
		if (len < LOG_PROPERTY_SIZE)
			SNPRINTF(val + len, LOG_PROPERTY_SIZE - len,
				" %s %d", "-mtk_vcodec_perf", mtk_vcodec_perf);

		len = strlen(val);
		if (len < LOG_PROPERTY_SIZE)
			SNPRINTF(val + len, LOG_PROPERTY_SIZE - len,
				" %s %d", "-mtk_v4l2_dbg_level", mtk_v4l2_dbg_level);
	}

	mtk_v4l2_debug(0, "val: %s, log_index: %d", val, log_index);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_get_log);

static void mtk_vcodec_add_list(struct task_struct *task)
{
	struct task_struct *task_child;
	int ret = 0;

	for_each_thread(task, task_child) {
		if (task_child != NULL) {
			ret = set_task_to_group(task_child->pid, GROUP_ID_1);
			if (ret == -1)
				mtk_v4l2_err("put tid %d fail", task_child->pid);
			else {
				if (group_list_size < VCODEC_MAX_GROUP_SIZE) {
					group_list[group_list_size] = task_child->pid;
					group_list_size ++;
				} else
					mtk_v4l2_err("invalid grouplist size %d", group_list_size);
			}
		}
	}
	//pr_info("update size group_list_size %d\n", group_list_size);

}

static void mtk_vcodec_make_group_list(void)
{
	struct task_struct *task;

	rcu_read_lock();
	for_each_process(task) {
		if (task != NULL && (
		    !strcmp(task->comm, "c2@1.2-mediatek") ||
		    !strcmp(task->comm, "mtk-vcodec-dec") ||
		    !strcmp(task->comm, "vdec_ipi_recv") ||
		    !strcmp(task->comm, "mtk-vcodec-enc") ||
		    !strcmp(task->comm, "venc_ipi_recv")))
			mtk_vcodec_add_list(task);
	}
	rcu_read_unlock();
}

void mtk_vcodec_config_group_list(void)
{
	int i = 0;

	if (unlikely(group_get_mode() == GP_MODE_0))
		return;

	spin_lock(&group_lock);
	if (group_list_size != 0) {
		//pr_info("clean group_list_size %d\n", group_list_size);
		for (i = 0; i < group_list_size; i++) {
			set_task_to_group( group_list[i], -1);
			group_list[i] = 0;
		}
		group_list_size = 0;
	}
	mtk_vcodec_make_group_list();
	spin_unlock(&group_lock);
}
EXPORT_SYMBOL_GPL(mtk_vcodec_config_group_list);

void mtk_vcodec_init_group_list_lock(void)
{
	static int init_flag;

	if (init_flag == 0) {
		init_flag = 1;
		spin_lock_init(&group_lock);
		group_list_size = 0;
	}

}
EXPORT_SYMBOL_GPL(mtk_vcodec_init_group_list_lock);




MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");

