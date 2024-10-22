// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][mmc]" fmt

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched/clock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include "queue.h"
#include "blocktag-internal.h"
#include "blocktag-mmc.h"

/* ring trace for debugfs, eMMC & SD */
struct mtk_blocktag *mmc_mtk_btag;

static struct mmc_mtk_bio_context_task *mmc_mtk_bio_get_task(
	struct mmc_mtk_bio_context *ctx, __u16 task_id)
{
	struct mmc_mtk_bio_context_task *tsk = NULL;

	if (!ctx)
		return NULL;

	if (task_id >= MMC_BIOLOG_CONTEXT_TASKS) {
		pr_notice("invalid task id %d\n", task_id);
		return NULL;
	}

	tsk = &ctx->task[task_id];

	return tsk;
}

static struct mmc_mtk_bio_context *mmc_mtk_bio_curr_ctx(bool is_sd)
{
	struct mmc_mtk_bio_context *ctx = BTAG_CTX(mmc_mtk_btag);

	if (is_sd)
		return ctx ? &ctx[1] : NULL;
	else
		return ctx ? &ctx[0] : NULL;
}

static struct mmc_mtk_bio_context_task *mmc_mtk_bio_curr_task(
	__u16 task_id, struct mmc_mtk_bio_context **curr_ctx, bool is_sd)
{
	struct mmc_mtk_bio_context *ctx;

	ctx = mmc_mtk_bio_curr_ctx(is_sd);
	if (curr_ctx)
		*curr_ctx = ctx;
	return mmc_mtk_bio_get_task(ctx, task_id);
}

static void btag_mmc_pidlog_insert(struct request *rq, bool is_sd,
				   __u64 *tot_len, __u64 *top_len)
{
	struct mmc_mtk_bio_context *ctx;
	struct req_iterator rq_iter;
	struct bio_vec bvec;
	__u16 insert_pid[BTAG_PIDLOG_ENTRIES] = {0};
	__u32 insert_len[BTAG_PIDLOG_ENTRIES] = {0};
	__u32 insert_cnt = 0;
	enum mtk_btag_io_type io_type;
	unsigned long flags;

	*tot_len = 0;
	*top_len = 0;

	if (!rq)
		return;

	ctx = mmc_mtk_bio_curr_ctx(is_sd);
	if (!ctx)
		return;

	if (req_op(rq) == REQ_OP_READ)
		io_type = BTAG_IO_READ;
	else if (req_op(rq) == REQ_OP_WRITE)
		io_type = BTAG_IO_WRITE;
	else
		return;

	rq_for_each_segment(bvec, rq, rq_iter) {
		short pid;
		int idx;

		if (!bvec.bv_page)
			continue;

		pid = mtk_btag_page_pidlog_get(bvec.bv_page);
		mtk_btag_page_pidlog_set(bvec.bv_page, 0);

		*tot_len += bvec.bv_len;

		if (pid == 0)
			continue;

		if (pid < 0) {
			*top_len += bvec.bv_len;
			pid = -pid;
		}

		for (idx = 0; idx < BTAG_PIDLOG_ENTRIES; idx++) {
			if (insert_pid[idx] == 0) {
				insert_pid[idx] = pid;
				insert_len[idx] = bvec.bv_len;
				insert_cnt++;
				break;
			} else if (insert_pid[idx] == pid) {
				insert_len[idx] += bvec.bv_len;
				break;
			}
		}
	}

	spin_lock_irqsave(&ctx->lock, flags);
	mtk_btag_pidlog_insert(&ctx->pidlog, insert_pid, insert_len,
			       insert_cnt, io_type);
	spin_unlock_irqrestore(&ctx->lock, flags);
}

void mmc_mtk_biolog_send_command(__u16 task_id, struct mmc_request *mrq)
{
	struct mmc_queue_req *mqrq;
	struct request *req;
	unsigned long flags;
	struct mmc_host *mmc;
	struct mmc_mtk_bio_context *ctx;
	struct mmc_mtk_bio_context_task *tsk;
	bool is_sd;
	__u64 tot_len, top_len;

	if (!mrq)
		return;

	mmc = mrq->host;
	if (!mmc)
		return;

	req = NULL;

	if (!(mmc->caps2 & MMC_CAP2_NO_MMC))
		is_sd = false;
	else if (!(mmc->caps2 & MMC_CAP2_NO_SD))
		is_sd = true;
	else
		return;

	tsk = mmc_mtk_bio_curr_task(task_id, &ctx, is_sd);
	if (!tsk)
		return;

	if (!is_sd && !mrq->cmd) { /* eMMC CQHCI */
		mqrq = container_of(mrq, struct mmc_queue_req, brq.mrq);
		req = blk_mq_rq_from_pdu(mqrq);
	/* SD non-cqhci */
	} else if (is_sd &&
		(mrq->cmd->opcode == MMC_READ_SINGLE_BLOCK ||
		mrq->cmd->opcode == MMC_READ_MULTIPLE_BLOCK ||
		mrq->cmd->opcode == MMC_WRITE_BLOCK ||
		mrq->cmd->opcode == MMC_WRITE_MULTIPLE_BLOCK)) {
		/* skip ioctl path such as RPMB test */
		if (PTR_ERR(mrq->completion.wait.task_list.next)
			&& PTR_ERR(mrq->completion.wait.task_list.prev))
			return;
		mqrq = container_of(mrq, struct mmc_queue_req, brq.mrq);
		req = blk_mq_rq_from_pdu(mqrq);
	} else
		return;

	btag_mmc_pidlog_insert(req, is_sd, &tot_len, &top_len);

	if (is_sd && mrq->cmd->data) {
		tsk->len = mrq->cmd->data->blksz * mrq->cmd->data->blocks;
		tsk->dir = MMC_DATA_DIR(!!(mrq->cmd->data->flags & MMC_DATA_READ));
	} else if (!is_sd && mrq->data) {
		tsk->len = mrq->data->blksz * mrq->data->blocks;
		tsk->dir = MMC_DATA_DIR(!!(mrq->data->flags & MMC_DATA_READ));
	}

	spin_lock_irqsave(&ctx->lock, flags);
	tsk->t[tsk_send_cmd] = sched_clock();
	tsk->t[tsk_req_compl] = 0;

	if (!ctx->period_start_t)
		ctx->period_start_t = tsk->t[tsk_send_cmd];

	ctx->q_depth++;
	spin_unlock_irqrestore(&ctx->lock, flags);

	/* mictx send logging */
	mtk_btag_mictx_send_command(mmc_mtk_btag, tsk->t[tsk_send_cmd],
				    tsk->dir ? BTAG_IO_WRITE : BTAG_IO_READ,
				    tot_len, top_len, task_id, 0);
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_send_command);

void mmc_mtk_biolog_transfer_req_compl(struct mmc_host *mmc,
	__u16 task_id, unsigned long req_mask)
{
	struct mmc_mtk_bio_context *ctx;
	struct mmc_mtk_bio_context_task *tsk;
	unsigned long flags;
	enum mtk_btag_io_type io_type;
	__u64 busy_time, cur_time = sched_clock();
	__u32 size;
	bool is_sd;

	if (!(mmc->caps2 & MMC_CAP2_NO_MMC))
		is_sd = false;
	else if (!(mmc->caps2 & MMC_CAP2_NO_SD))
		is_sd = true;
	else
		return;

	tsk = mmc_mtk_bio_curr_task(task_id, &ctx, is_sd);
	if (!tsk)
		return;

	/* return if there's no on-going request  */
	if (!tsk->t[tsk_send_cmd])
		return;

	/* mictx complete logging */
	mtk_btag_mictx_complete_command(mmc_mtk_btag, cur_time, task_id, 0);

	spin_lock_irqsave(&ctx->lock, flags);

	tsk->t[tsk_req_compl] = cur_time;

	io_type = tsk->dir ? BTAG_IO_WRITE : BTAG_IO_READ;

	/* throughput usage := duration of handling this request */
	busy_time = tsk->t[tsk_req_compl] - tsk->t[tsk_send_cmd];

	/* workload statistics */
	ctx->workload.count++;

	if (io_type < BTAG_IO_TYPE_NR) {
		size = tsk->len;
		ctx->throughput[io_type].usage += busy_time;
		ctx->throughput[io_type].size += size;
	}

	if (!req_mask)
		ctx->q_depth = 0;
	else
		ctx->q_depth--;

	/* clear this task */
	tsk->t[tsk_send_cmd] = tsk->t[tsk_req_compl] = 0;

	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_transfer_req_compl);

/* evaluate throughput and workload of given context */
static void mmc_mtk_bio_context_eval(struct mmc_mtk_bio_context *ctx)
{
	uint64_t period;

	ctx->workload.usage = ctx->period_usage;

	if (ctx->workload.period > (ctx->workload.usage * 100)) {
		ctx->workload.percent = 1;
	} else {
		period = ctx->workload.period;
		do_div(period, 100);
		ctx->workload.percent =
			(__u32)ctx->workload.usage / (__u32)period;
	}
	mtk_btag_throughput_eval(ctx->throughput);
}

/* print context to trace ring buffer */
static struct mtk_btag_trace *mmc_mtk_bio_print_trace(
	struct mmc_mtk_bio_context *ctx)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(mmc_mtk_btag);
	struct mtk_btag_trace *tr;
	unsigned long flags;

	if (!rt)
		return NULL;

	spin_lock_irqsave(&rt->lock, flags);
	tr = mtk_btag_curr_trace(rt);

	if (!tr)
		goto out;

	memset(tr, 0, sizeof(struct mtk_btag_trace));
	tr->pid = ctx->pid;
	tr->qid = ctx->qid;
	mtk_btag_pidlog_eval(&tr->pidlog, &ctx->pidlog);
	mtk_btag_vmstat_eval(&tr->vmstat);
	mtk_btag_cpu_eval(&tr->cpu);
	memcpy(tr->throughput, ctx->throughput,
	       sizeof(struct mtk_btag_throughput) * BTAG_IO_TYPE_NR);
	memcpy(&tr->workload, &ctx->workload, sizeof(struct mtk_btag_workload));

	tr->time = sched_clock();
	mtk_btag_next_trace(rt);
out:
	spin_unlock_irqrestore(&rt->lock, flags);
	return tr;
}

static void mmc_mtk_bio_ctx_count_usage(struct mmc_mtk_bio_context *ctx,
	__u64 start, __u64 end)
{
	__u64 busy_in_period;

	if (start < ctx->period_start_t)
		busy_in_period = end - ctx->period_start_t;
	else
		busy_in_period = end - start;

	ctx->period_usage += busy_in_period;
}

/* Check requests after set/clear mask. */
void mmc_mtk_biolog_check(struct mmc_host *mmc, unsigned long req_mask)
{
	struct mmc_mtk_bio_context *ctx;
	struct mtk_btag_trace *tr = NULL;
	__u64 end_time, period_time;
	unsigned long flags;
	bool is_sd;

	if (!(mmc->caps2 & MMC_CAP2_NO_MMC))
		is_sd = false;
	else if (!(mmc->caps2 & MMC_CAP2_NO_SD))
		is_sd = true;
	else
		return;

	ctx = mmc_mtk_bio_curr_ctx(is_sd);
	if (!ctx)
		return;

	end_time = sched_clock();

	spin_lock_irqsave(&ctx->lock, flags);

	if (ctx->busy_start_t)
		mmc_mtk_bio_ctx_count_usage(ctx, ctx->busy_start_t, end_time);

	ctx->busy_start_t = (req_mask) ? end_time : 0;

	period_time = end_time - ctx->period_start_t;

	if (period_time >= MMC_MTK_BIO_TRACE_LATENCY) {
		ctx->period_end_t = end_time;
		ctx->workload.period = period_time;
		mmc_mtk_bio_context_eval(ctx);
		tr = mmc_mtk_bio_print_trace(ctx);
		ctx->period_start_t = end_time;
		ctx->period_end_t = 0;
		ctx->period_usage = 0;
		memset(ctx->throughput, 0,
		       sizeof(struct mtk_btag_throughput) * BTAG_IO_TYPE_NR);
		memset(&ctx->workload, 0, sizeof(struct mtk_btag_workload));
	}
	spin_unlock_irqrestore(&ctx->lock, flags);
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_check);

static size_t mmc_mtk_bio_seq_debug_show_info(char **buff, unsigned long *size,
	struct seq_file *seq)
{
	int i;
	struct mmc_mtk_bio_context *ctx = BTAG_CTX(mmc_mtk_btag);

	if (!ctx)
		return 0;

	for (i = 0; i < MMC_BIOLOG_CONTEXTS; i++)	{
		if (ctx[i].pid == 0)
			continue;
		BTAG_PRINTF(buff, size, seq,
			    "ctx[%d]=ctx_map[%d],pid:%4d,q:%d\n",
			    i,
			    ctx[i].id,
			    ctx[i].pid,
			    ctx[i].qid);
	}

	return 0;
}

static void mmc_mtk_bio_init_ctx(struct mmc_mtk_bio_context *ctx)
{
	int i;

	for (i = 0; i < MMC_BIOLOG_CONTEXTS; i++) {
		memset(ctx + i, 0, sizeof(struct mmc_mtk_bio_context));
		spin_lock_init(&(ctx + i)->lock);
		spin_lock_init(&(ctx + i)->pidlog.lock);
		(ctx + i)->period_start_t = sched_clock();
		(ctx + i)->qid = i;
	}
}

static struct mtk_btag_vops mmc_mtk_btag_vops = {
	.seq_show       = mmc_mtk_bio_seq_debug_show_info,
};

int mmc_mtk_biolog_init(struct mmc_host *mmc)
{
	struct mtk_blocktag *btag;
	struct mmc_mtk_bio_context *ctx;
	struct device_node *np;
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	if (!mmc)
		return -EINVAL;

	np = mmc->class_dev.of_node;
	boot_node = of_parse_phandle(np, "bootmode", 0);

	if (boot_node) {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (tag) {
			if (tag->boottype == 1)
				mmc_mtk_btag_vops.boot_device = true;
		}
	}

	btag = mtk_btag_alloc("mmc",
				BTAG_STORAGE_MMC,
				MMC_BIOLOG_RINGBUF_MAX,
				sizeof(struct mmc_mtk_bio_context),
				MMC_BIOLOG_CONTEXTS,
				&mmc_mtk_btag_vops);

	if (btag) {
		mmc_mtk_btag = btag;
		ctx = BTAG_CTX(mmc_mtk_btag);
		mmc_mtk_bio_init_ctx(ctx);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_init);

int mmc_mtk_biolog_exit(void)
{
	mtk_btag_free(mmc_mtk_btag);
	return 0;
}
EXPORT_SYMBOL_GPL(mmc_mtk_biolog_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek MMC Block IO Tracer");
