// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Perry Hsu <perry.hsu@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#define DEBUG 1

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][mictx]" fmt

#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/spinlock.h>
#include <linux/math64.h>
#define CREATE_TRACE_POINTS
#include "blocktag-internal.h"
#include "blocktag-trace.h"
#include "mtk_blocktag.h"

#define MICTX_RESET_NS 1000000000

static struct mtk_btag_mictx *mictx_find(struct mtk_blocktag *btag, __s8 id)
{
	struct mtk_btag_mictx *mictx;

	list_for_each_entry_rcu(mictx, &btag->ctx.mictx.list, list)
		if (mictx->id == id)
			return mictx;
	return NULL;
}

/* TODO:
 * this is only used in mtk_btag_mictx_check_window. check if this is necessary.
 */
static void mictx_reset(struct mtk_btag_mictx *mictx)
{
	unsigned long flags;
	int qid;

	/* clear throughput, request data */
	for (qid = 0; qid < mictx->queue_nr; qid++) {
		struct mtk_btag_mictx_queue *q = &mictx->q[qid];

		spin_lock_irqsave(&q->lock, flags);
		q->rq_size[BTAG_IO_READ] = 0;
		q->rq_size[BTAG_IO_WRITE] = 0;
		q->rq_cnt[BTAG_IO_READ] = 0;
		q->rq_cnt[BTAG_IO_WRITE] = 0;
		q->top_len = 0;
		q->tp_size[BTAG_IO_READ] = 0;
		q->tp_size[BTAG_IO_WRITE] = 0;
		q->tp_time[BTAG_IO_READ] = 0;
		q->tp_time[BTAG_IO_WRITE] = 0;
		spin_unlock_irqrestore(&q->lock, flags);
	}

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
	/* clear average queue depth */
	spin_lock_irqsave(&mictx->avg_qd.lock, flags);
	mictx->avg_qd.latency = 0;
	mictx->avg_qd.last_depth_chg = sched_clock();
	spin_unlock_irqrestore(&mictx->avg_qd.lock, flags);
#endif

	/* clear workload */
	spin_lock_irqsave(&mictx->wl.lock, flags);
	mictx->wl.idle_total = 0;
	mictx->wl.window_begin = sched_clock();
	if (mictx->wl.idle_begin)
		mictx->wl.idle_begin = mictx->wl.window_begin;
	spin_unlock_irqrestore(&mictx->wl.lock, flags);
}

void mtk_btag_mictx_check_window(struct mtk_btag_mictx_id mictx_id, bool force)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;

	btag = mtk_btag_find_by_type(mictx_id.storage);
	if (!btag)
		return;

	rcu_read_lock();
	mictx = mictx_find(btag, mictx_id.id);
	if (!mictx) {
		rcu_read_unlock();
		return;
	}

	if (force) {
		mictx_reset(mictx);
	} else if  (sched_clock() - READ_ONCE(mictx->wl.window_begin) > MICTX_RESET_NS) {
		mictx_reset(mictx);
		mtk_btag_earaio_clear_data();
	}

	rcu_read_unlock();
}

void mtk_btag_mictx_send_command(struct mtk_blocktag *btag, __u64 start_t,
				 enum mtk_btag_io_type io_type, __u64 tot_len,
				 __u64 top_len, __u32 tid, __u16 qid)
{
	struct mtk_btag_mictx *mictx;

	if (!btag || tid >= BTAG_MAX_TAG || io_type == BTAG_IO_UNKNOWN)
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(mictx, &btag->ctx.mictx.list, list) {
		struct mtk_btag_mictx_queue *q = &mictx->q[qid];
		unsigned long flags;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
		__u64 time;
#endif

		/* workload */
		spin_lock_irqsave(&mictx->wl.lock, flags);
		if (!mictx->wl.depth++) {
			mictx->wl.idle_total += start_t - mictx->wl.idle_begin;
			mictx->wl.idle_begin = 0;
		}
		spin_unlock_irqrestore(&mictx->wl.lock, flags);

		/* request size & count */
		spin_lock_irqsave(&q->lock, flags);
		q->rq_cnt[io_type]++;
		q->rq_size[io_type] += tot_len;
		q->top_len += top_len;
		spin_unlock_irqrestore(&q->lock, flags);

		/* tags */
		mictx->tags[tid].start_t = start_t;

		/* throughput */
		if (mictx->full_logging) {
			mictx->tags[tid].io_type = io_type;
			mictx->tags[tid].len = tot_len;
		}

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
		/* average queue depth
		 * NOTE: see the calculation in mictx_evaluate_avg_qd()
		 */
		spin_lock_irqsave(&mictx->avg_qd.lock, flags);
		time = sched_clock();
		if (mictx->full_logging) {
			mictx->avg_qd.latency += mictx->avg_qd.depth *
					(time - mictx->avg_qd.last_depth_chg);
		}
		mictx->avg_qd.last_depth_chg = time;
		mictx->avg_qd.depth++;
		spin_unlock_irqrestore(&mictx->avg_qd.lock, flags);
#endif
	}
	rcu_read_unlock();

	if (top_len && btag->vops->earaio_enabled)
		mtk_btag_earaio_update_pwd(io_type, top_len);
}

void mtk_btag_mictx_complete_command(struct mtk_blocktag *btag, __u64 end_t,
				     __u32 tid, __u16 qid)
{
	struct mtk_btag_mictx *mictx;

	if (!btag || tid >= BTAG_MAX_TAG)
		return;

	rcu_read_lock();
	list_for_each_entry_rcu(mictx, &btag->ctx.mictx.list, list) {
		struct mtk_btag_mictx_queue *q = &mictx->q[qid];
		unsigned long flags;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
		__u64 time;
#endif

		if (!mictx->tags[tid].start_t)
			continue;

		/* workload */
		spin_lock_irqsave(&mictx->wl.lock, flags);
		if (!--mictx->wl.depth)
			mictx->wl.idle_begin = end_t;
		spin_unlock_irqrestore(&mictx->wl.lock, flags);

		/* throughput */
		if (mictx->full_logging) {
			spin_lock_irqsave(&q->lock, flags);
			q->tp_size[mictx->tags[tid].io_type] +=
				mictx->tags[tid].len;
			q->tp_time[mictx->tags[tid].io_type] +=
				end_t - mictx->tags[tid].start_t;
			spin_unlock_irqrestore(&q->lock, flags);
		}

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
		/* average queue depth
		 * NOTE: see the calculation in mictx_evaluate_avg_qd()
		 */
		spin_lock_irqsave(&mictx->avg_qd.lock, flags);
		time = sched_clock();
		if (mictx->full_logging) {
			mictx->avg_qd.latency += mictx->avg_qd.depth *
					(time - mictx->avg_qd.last_depth_chg);
		}
		mictx->avg_qd.last_depth_chg = time;
		mictx->avg_qd.depth--;
		spin_unlock_irqrestore(&mictx->avg_qd.lock, flags);
#endif

		/* clear tags */
		mictx->tags[tid].start_t = 0;
		mictx->tags[tid].len = 0;
	}
	rcu_read_unlock();
}

static void mictx_evaluate_workload(struct mtk_btag_mictx *mictx,
				    struct mtk_btag_mictx_iostat_struct *iostat)
{
	unsigned long flags;
	__u64 cur_time, idle_total, window_begin, idle_begin = 0;

	spin_lock_irqsave(&mictx->wl.lock, flags);
	cur_time = sched_clock();
	idle_total = mictx->wl.idle_total;
	window_begin = mictx->wl.window_begin;
	mictx->wl.idle_total = 0;
	mictx->wl.window_begin = cur_time;
	if (mictx->wl.idle_begin) {
		idle_begin = mictx->wl.idle_begin;
		mictx->wl.idle_begin = cur_time;
	}
	spin_unlock_irqrestore(&mictx->wl.lock, flags);

	if (idle_begin)
		idle_total += cur_time - idle_begin;
	iostat->wl = (__u16)(100 - div64_u64(idle_total * 100,
					     cur_time - window_begin));
	iostat->duration = cur_time - window_begin;
}

static __u32 calculate_throughput(__u64 bytes, __u64 time)
{
	__u64 tp;

	if (!bytes)
		return 0;

	do_div(time, 1000000);       /* convert ns to ms */
	if (!time)
		return 0;

	tp = div64_u64(bytes, time); /* byte/ms */
	tp = (tp * 1000) >> 10;      /* KB/s */

	return (__u32)tp;
}

static void mictx_evaluate_queue(struct mtk_btag_mictx *mictx,
				 struct mtk_btag_mictx_iostat_struct *iostat)
{
	__u64 tp_size[BTAG_IO_TYPE_NR] = {0};
	__u64 tp_time[BTAG_IO_TYPE_NR] = {0};
	__u64 tot_len, top_len = 0;
	unsigned long flags;
	int qid;

	/* get and clear mictx queue data */
	for (qid = 0; qid < mictx->queue_nr; qid++) {
		struct mtk_btag_mictx_queue tmp, *q = &mictx->q[qid];
		int io_type;

		spin_lock_irqsave(&q->lock, flags);
		tmp = *q;
		for (io_type = 0; io_type < BTAG_IO_TYPE_NR; io_type++) {
			q->rq_size[io_type] = 0;
			q->rq_cnt[io_type] = 0;
			q->tp_size[io_type] = 0;
			q->tp_time[io_type] = 0;
		}
		q->top_len = 0;
		spin_unlock_irqrestore(&q->lock, flags);

		iostat->reqsize_r += tmp.rq_size[BTAG_IO_READ];
		iostat->reqsize_w += tmp.rq_size[BTAG_IO_WRITE];
		iostat->reqcnt_r += tmp.rq_cnt[BTAG_IO_READ];
		iostat->reqcnt_w += tmp.rq_cnt[BTAG_IO_WRITE];
		top_len += tmp.top_len;

		if (!mictx->full_logging)
			continue;

		tp_size[BTAG_IO_READ] += tmp.tp_size[BTAG_IO_READ];
		tp_size[BTAG_IO_WRITE] += tmp.tp_size[BTAG_IO_WRITE];
		tp_time[BTAG_IO_READ] += tmp.tp_time[BTAG_IO_READ];
		tp_time[BTAG_IO_WRITE] += tmp.tp_time[BTAG_IO_WRITE];
	}

	/* top rate */
	tot_len = iostat->reqsize_r + iostat->reqsize_w;
	iostat->top = tot_len ? (__u32)div64_u64(top_len * 100, tot_len) : 0;

	if (!mictx->full_logging)
		return;

	/* throughput (per-request) */
	iostat->tp_req_r = calculate_throughput(tp_size[BTAG_IO_READ],
						tp_time[BTAG_IO_READ]);
	iostat->tp_req_w = calculate_throughput(tp_size[BTAG_IO_WRITE],
						tp_time[BTAG_IO_WRITE]);

	/* throughput (overlapped, not 100% precise) */
	iostat->tp_all_r = calculate_throughput(tp_size[BTAG_IO_READ],
						iostat->duration);
	iostat->tp_all_w = calculate_throughput(tp_size[BTAG_IO_WRITE],
						iostat->duration);
}

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
/* Average Queue Depth
 * NOTE:
 *                 |<----------------- Window Time (WT) ----------------->|
 *                 |<---------- cmd1 ---------->                          |
 *                 |                 <---------- cmd2 ---------->         |
 *    time          t0               t1        t2               t3
 *    Depth Before  0                1         2                1
 *    Depth After   1                2         1                0
 *
 *    The Average Queue Depth can be calculate as:
 *      = Sum of CMD Latency / Window Time
 *      = [ (t1-t0)*1 + (t2-t1)*2 + (t3-t2)*1 ] / WT
 *      = Sum{[Depth_Change_Time(n) - Depth_Change_Time(n-1)] * Depth(n-1)} / WT
 */
static void mictx_evaluate_avg_qd(struct mtk_btag_mictx *mictx,
				  struct mtk_btag_mictx_iostat_struct *iostat)
{
	unsigned long flags;
	__u64 cur_time, latency, last_depth_chg, depth;

	spin_lock_irqsave(&mictx->avg_qd.lock, flags);
	cur_time = sched_clock();
	latency = mictx->avg_qd.latency;
	last_depth_chg = mictx->avg_qd.last_depth_chg;
	depth = mictx->avg_qd.depth;
	mictx->avg_qd.latency = 0;
	mictx->avg_qd.last_depth_chg = cur_time;
	spin_unlock_irqrestore(&mictx->avg_qd.lock, flags);

	latency += depth * (cur_time - last_depth_chg);
	iostat->q_depth = (__u16)div64_u64(latency, iostat->duration);
}
#endif

int mtk_btag_mictx_get_data(struct mtk_btag_mictx_id mictx_id,
			    struct mtk_btag_mictx_iostat_struct *iostat)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;

	if (!iostat)
		return -1;

	btag = mtk_btag_find_by_type(mictx_id.storage);
	if (!btag)
		return -1;

	rcu_read_lock();
	mictx = mictx_find(btag, mictx_id.id);
	if (!mictx) {
		rcu_read_unlock();
		return -1;
	}

	memset(iostat, 0, sizeof(struct mtk_btag_mictx_iostat_struct));
	mictx_evaluate_workload(mictx, iostat);
	mictx_evaluate_queue(mictx, iostat);
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
	if (mictx->full_logging)
		mictx_evaluate_avg_qd(mictx, iostat);
#endif
	rcu_read_unlock();

	trace_blocktag_mictx_get_data(mictx_id.name, iostat);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_get_data);

void mtk_btag_mictx_set_full_logging(struct mtk_btag_mictx_id mictx_id,
				     bool enable)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;

	btag = mtk_btag_find_by_type(mictx_id.storage);
	if (!btag)
		return;

	rcu_read_lock();
	mictx = mictx_find(btag, mictx_id.id);
	if (!mictx) {
		rcu_read_unlock();
		return;
	}
	mictx->full_logging = enable;
	rcu_read_unlock();
}

int mtk_btag_mictx_full_logging(struct mtk_btag_mictx_id mictx_id)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;
	int ret;

	btag = mtk_btag_find_by_type(mictx_id.storage);
	if (!btag)
		return -1;

	rcu_read_lock();
	mictx = mictx_find(btag, mictx_id.id);
	if (!mictx) {
		rcu_read_unlock();
		return -1;
	}
	ret = mictx->full_logging;
	rcu_read_unlock();

	return ret;
}

static int mictx_alloc(enum mtk_btag_storage_type type)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;
	struct mtk_btag_mictx_queue *q;
	unsigned long flags;
	__u64 cur_time = sched_clock();
	int qid;

	btag = mtk_btag_find_by_type(type);
	if (!btag)
		return -1;

	mictx = kzalloc(sizeof(struct mtk_btag_mictx), GFP_NOFS);
	if (!mictx)
		return -1;

	q = kcalloc(btag->ctx.count, sizeof(struct mtk_btag_mictx_queue),
		    GFP_NOFS);
	if (!q) {
		kfree(mictx);
		return -1;
	}

	mictx->queue_nr = btag->ctx.count;
	spin_lock_init(&mictx->wl.lock);
	mictx->wl.window_begin = cur_time;
	mictx->wl.idle_begin = cur_time;
#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD)
	spin_lock_init(&mictx->avg_qd.lock);
	mictx->avg_qd.last_depth_chg = cur_time;
#endif
	for (qid = 0; qid < btag->ctx.count; qid++)
		spin_lock_init(&q[qid].lock);
	mictx->q = q;

	spin_lock_irqsave(&btag->ctx.mictx.list_lock, flags);
	mictx->id = btag->ctx.mictx.last_unused_id;
	mictx->full_logging = true;
	btag->ctx.mictx.nr_list++;
	btag->ctx.mictx.last_unused_id++;
	list_add_tail_rcu(&mictx->list, &btag->ctx.mictx.list);
	spin_unlock_irqrestore(&btag->ctx.mictx.list_lock, flags);

	return mictx->id;
}

static void mictx_free(struct mtk_btag_mictx_id *mictx_id)
{
	struct mtk_blocktag *btag;
	struct mtk_btag_mictx *mictx;
	unsigned long flags;

	btag = mtk_btag_find_by_type(mictx_id->storage);
	if (!btag)
		return;

	spin_lock_irqsave(&btag->ctx.mictx.list_lock, flags);
	list_for_each_entry(mictx, &btag->ctx.mictx.list, list)
		if (mictx->id == mictx_id->id)
			goto found;
	spin_unlock_irqrestore(&btag->ctx.mictx.list_lock, flags);
	return;

found:
	list_del_rcu(&mictx->list);
	btag->ctx.mictx.nr_list--;
	spin_unlock_irqrestore(&btag->ctx.mictx.list_lock, flags);

	synchronize_rcu();
	kfree(mictx->q);
	kfree(mictx);
}

void mtk_btag_mictx_free_all(struct mtk_blocktag *btag)
{
	struct mtk_btag_mictx *mictx, *n;
	unsigned long flags;
	LIST_HEAD(free_list);

	spin_lock_irqsave(&btag->ctx.mictx.list_lock, flags);
	list_splice_init(&btag->ctx.mictx.list, &free_list);
	btag->ctx.mictx.nr_list = 0;
	spin_unlock_irqrestore(&btag->ctx.mictx.list_lock, flags);

	synchronize_rcu();
	list_for_each_entry_safe(mictx, n, &free_list, list) {
		list_del(&mictx->list);
		kfree(mictx->q);
		kfree(mictx);
	}
}

void mtk_btag_mictx_enable(struct mtk_btag_mictx_id *mictx_id, bool enable)
{
	if (enable)
		mictx_id->id = mictx_alloc(mictx_id->storage);
	else
		mictx_free(mictx_id);
}
EXPORT_SYMBOL_GPL(mtk_btag_mictx_enable);

void mtk_btag_mictx_init(struct mtk_blocktag *btag)
{
	spin_lock_init(&btag->ctx.mictx.list_lock);
	btag->ctx.mictx.nr_list = 0;
	INIT_LIST_HEAD(&btag->ctx.mictx.list);
}
