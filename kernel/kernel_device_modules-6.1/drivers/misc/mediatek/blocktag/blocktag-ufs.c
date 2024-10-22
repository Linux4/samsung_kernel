// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 * Authors:
 *	Perry Hsu <perry.hsu@mediatek.com>
 *	Stanley Chu <stanley.chu@mediatek.com>
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][ufs]" fmt

#define DEBUG 1
#define BTAG_UFS_TRACE_LATENCY ((unsigned long long)(1000000000))

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_proto.h>
#include "blocktag-trace.h"
#include "blocktag-internal.h"
#include "blocktag-ufs.h"

/* ring trace for debugfs */
struct mtk_blocktag *ufs_mtk_btag;
static int tag_per_queue;
static int nr_queue;
static int nr_tag;
struct workqueue_struct *ufs_mtk_btag_wq;
struct work_struct ufs_mtk_btag_worker;
#define tid_to_qid(tid) ((tag_per_queue) ? (tid) / (tag_per_queue) : -1)

static inline __u16 chbe16_to_u16(const char *str)
{
	__u16 ret;

	ret = str[0];
	ret = ret << 8 | str[1];
	return ret;
}

static inline __u32 chbe32_to_u32(const char *str)
{
	__u32 ret;

	ret = str[0];
	ret = ret << 8 | str[1];
	ret = ret << 8 | str[2];
	ret = ret << 8 | str[3];
	return ret;
}

#define scsi_cmnd_cmd(cmd)  (cmd->cmnd[0])

static enum mtk_btag_io_type cmd_to_io_type(__u16 cmd)
{
	switch (cmd) {
	case READ_6:
	case READ_10:
	case READ_16:
#if IS_ENABLED(CONFIG_SCSI_UFS_HPB)
	case UFSHPB_READ:
#endif
		return BTAG_IO_READ;

	case WRITE_6:
	case WRITE_10:
	case WRITE_16:
		return BTAG_IO_WRITE;

	default:
		return BTAG_IO_UNKNOWN;
	}
}

static __u32 scsi_cmnd_len(struct scsi_cmnd *cmd)
{
	__u32 len;

	switch (scsi_cmnd_cmd(cmd)) {
	case READ_6:
	case WRITE_6:
		len = cmd->cmnd[4];
		break;

	case READ_10:
	case WRITE_10:
		len = chbe16_to_u16(&cmd->cmnd[7]);
		break;

	case READ_16:
	case WRITE_16:
		len = chbe32_to_u32(&cmd->cmnd[10]);
		break;

#if IS_ENABLED(CONFIG_SCSI_UFS_HPB)
	case UFSHPB_READ:
		len = cmd->cmnd[14];
		break;
#endif

	default:
		return 0;
	}
	return len << UFS_LOGBLK_SHIFT;
}

static struct btag_ufs_ctx *btag_ufs_ctx(__u16 qid)
{
	struct btag_ufs_ctx *ctx = BTAG_CTX(ufs_mtk_btag);

	if (!ctx)
		return NULL;

	if (qid >= nr_queue) {
		pr_notice("invalid queue id %d\n", qid);
		return NULL;
	}
	return &ctx[qid];
}

static struct btag_ufs_ctx *btag_ufs_tid_to_ctx(__u16 tid)
{
	if (tid >= nr_tag) {
		pr_notice("%s: invalid tag id %d\n", __func__, tid);
		return NULL;
	}

	return btag_ufs_ctx(tid_to_qid(tid));
}

static struct btag_ufs_tag *btag_ufs_tag(struct btag_ufs_ctx_data *data,
					 __u16 tid)
{
	if (!data)
		return NULL;

	if (tid >= nr_tag) {
		pr_notice("%s: invalid tag id %d\n", __func__, tid);
		return NULL;
	}

	return &data->tags[tid % tag_per_queue];
}

static void btag_ufs_pidlog_insert(struct mtk_btag_proc_pidlogger *pidlog,
				   struct scsi_cmnd *cmd, __u32 *top_len)
{
	struct req_iterator rq_iter;
	struct bio_vec bvec;
	struct request *rq;
	__u16 insert_pid[BTAG_PIDLOG_ENTRIES] = {0};
	__u32 insert_len[BTAG_PIDLOG_ENTRIES] = {0};
	__u32 insert_cnt = 0;
	enum mtk_btag_io_type io_type;

	*top_len = 0;
	rq = scsi_cmd_to_rq(cmd);
	if (!rq)
		return;

	io_type = cmd_to_io_type(scsi_cmnd_cmd(cmd));
	if (io_type == BTAG_IO_UNKNOWN)
		return;

	rq_for_each_segment(bvec, rq, rq_iter) {
		short pid;
		int idx;

		if (!bvec.bv_page)
			continue;

		pid = mtk_btag_page_pidlog_get(bvec.bv_page);
		mtk_btag_page_pidlog_set(bvec.bv_page, 0);

		if (!pid)
			continue;

		if (pid < 0) {
			*top_len += bvec.bv_len;
			pid = -pid;
		}

		/* only calculate the top len and return */
		if (!pidlog)
			continue;

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

	if (pidlog)
		mtk_btag_pidlog_insert(pidlog, insert_pid, insert_len,
				       insert_cnt, io_type);
}

void mtk_btag_ufs_send_command(__u16 tid, __u16 qid, struct scsi_cmnd *cmd)
{
	struct btag_ufs_ctx *ctx;
	struct btag_ufs_ctx_data *data;
	struct btag_ufs_tag *tag;
	unsigned long flags;
	__u64 cur_time = sched_clock();
	__u64 window_t = 0;
	__u32 top_len = 0;

	if (!cmd)
		return;

	tid += qid * tag_per_queue;

	ctx = btag_ufs_tid_to_ctx(tid);
	if (!ctx)
		return;

	if (!ufs_mtk_btag->ctx_enable) {
		btag_ufs_pidlog_insert(NULL, cmd, &top_len);
		goto mictx;
	}

	rcu_read_lock();
	data = rcu_dereference(ctx->cur_data);

	/* tag */
	tag = btag_ufs_tag(data, tid);
	if (!tag)
		goto rcu_unlock;
	tag->len = scsi_cmnd_len(cmd);
	tag->cmd = scsi_cmnd_cmd(cmd);
	tag->start_t = cur_time;

	/* workload */
	spin_lock_irqsave(&data->wl.lock, flags);
	if (!data->wl.depth++) {
		data->wl.idle_total += cur_time - data->wl.idle_begin;
		data->wl.idle_begin = 0;
	}
	window_t = cur_time - data->wl.window_begin;
	spin_unlock_irqrestore(&data->wl.lock, flags);

	/* pidlog */
	btag_ufs_pidlog_insert(&data->pidlog, cmd, &top_len);

rcu_unlock:
	rcu_read_unlock();
	if (window_t > BTAG_UFS_TRACE_LATENCY)
		queue_work(ufs_mtk_btag_wq, &ufs_mtk_btag_worker);

mictx:
	/* mictx send logging */
	mtk_btag_mictx_send_command(ufs_mtk_btag, cur_time,
				    cmd_to_io_type(scsi_cmnd_cmd(cmd)),
				    scsi_cmnd_len(cmd), top_len, tid,
				    tid_to_qid(tid));

	trace_blocktag_ufs_send(sched_clock() - cur_time, tid, tid_to_qid(tid),
				scsi_cmnd_cmd(cmd), scsi_cmnd_len(cmd));
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_send_command);

void mtk_btag_ufs_transfer_req_compl(__u16 tid, __u16 qid)
{
	struct btag_ufs_ctx *ctx;
	struct btag_ufs_ctx_data *data;
	struct btag_ufs_tag *tag;
	unsigned long flags;
	enum mtk_btag_io_type io_type;
	__u64 cur_time = sched_clock();
	__u64 window_t = 0;

	tid += qid * tag_per_queue;

	ctx = btag_ufs_tid_to_ctx(tid);
	if (!ctx)
		return;

	if (!ufs_mtk_btag->ctx_enable)
		goto mictx;

	rcu_read_lock();
	data = rcu_dereference(ctx->cur_data);

	tag = btag_ufs_tag(data, tid);
	if (!tag)
		goto rcu_unlock;

	/* throughput */
	io_type = cmd_to_io_type(tag->cmd);
	if (io_type < BTAG_IO_TYPE_NR) {
		spin_lock_irqsave(&data->tp.lock, flags);
		data->tp.usage[io_type] += (cur_time - tag->start_t);
		data->tp.size[io_type] += tag->len;
		spin_unlock_irqrestore(&data->tp.lock, flags);
	}

	/* workload
	 * If a tag is transferred before ctx data switching, the start_t will
	 * be 0. Do not count the depth and set the time from the start of the
	 * window to now as busy (clear idle_total).
	 */
	spin_lock_irqsave(&data->wl.lock, flags);
	data->wl.req_cnt++;
	if (!tag->start_t) {
		data->wl.idle_total = 0;
		data->wl.idle_begin = data->wl.depth ? 0 : cur_time;
	} else if (!--data->wl.depth) {
		data->wl.idle_begin = cur_time;
	}
	window_t = cur_time - data->wl.window_begin;
	spin_unlock_irqrestore(&data->wl.lock, flags);

	/* clear tag */
	tag->start_t = 0;
	tag->cmd = 0;
	tag->len = 0;

rcu_unlock:
	rcu_read_unlock();
	if (window_t > BTAG_UFS_TRACE_LATENCY)
		queue_work(ufs_mtk_btag_wq, &ufs_mtk_btag_worker);

mictx:
	/* mictx complete logging */
	mtk_btag_mictx_complete_command(ufs_mtk_btag, cur_time, tid,
					tid_to_qid(tid));

	trace_blocktag_ufs_complete(sched_clock() - cur_time, tid,
				    tid_to_qid(tid));
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_transfer_req_compl);

/* evaluate throughput, workload and pidlog of given context data */
static void btag_ufs_data_eval(struct btag_ufs_ctx_data *data,
			       struct mtk_btag_trace *tr)
{
	struct mtk_btag_throughput *tp = tr->throughput;
	struct mtk_btag_workload *wl = &tr->workload;
	enum mtk_btag_io_type io_type;
	__u64 cur_time = sched_clock();

	tr->time = cur_time;

	/* throughput */
	for (io_type = 0; io_type < BTAG_IO_TYPE_NR; io_type++) {
		tp[io_type].usage = data->tp.usage[io_type];
		tp[io_type].size = data->tp.size[io_type];
	}
	mtk_btag_throughput_eval(tp);

	/* workload */
	wl->period = cur_time - data->wl.window_begin;
	wl->usage = wl->period - data->wl.idle_total;
	wl->count = data->wl.req_cnt;
	if (data->wl.idle_begin)
		wl->usage -= cur_time - data->wl.idle_begin;
	if (!wl->usage)
		wl->percent = 0;
	else if (wl->period > wl->usage * 100)
		wl->percent = 1;
	else
		wl->percent = (__u32)div64_u64(wl->usage * 100, wl->period);

	/* pidlog */
	memcpy(&tr->pidlog.info, &data->pidlog.info,
	       sizeof(struct mtk_btag_proc_pidlogger_entry) *
	       BTAG_PIDLOG_ENTRIES);
}

/* Switch the ufs context data to another
 * This function is only used by ufs worker, therefore we can switch the ctx
 * data without any write lock.
 */
static struct btag_ufs_ctx_data *btag_switch_ctx_data(struct btag_ufs_ctx *ctx)
{
	struct btag_ufs_ctx_data *next, *prev;

	rcu_read_lock();
	prev = rcu_dereference(ctx->cur_data);
	if (sched_clock() - prev->wl.window_begin < BTAG_UFS_TRACE_LATENCY) {
		rcu_read_unlock();
		return NULL;
	}

	next = (prev == ctx->data) ? &ctx->data[1] : &ctx->data[0];
	next->wl.window_begin = sched_clock();
	next->wl.idle_begin = next->wl.window_begin;
	rcu_assign_pointer(ctx->cur_data, next);
	rcu_read_unlock();
	synchronize_rcu();

	return prev;
}

/* evaluate context to trace ring buffer */
static void btag_ufs_work(struct work_struct *work)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(ufs_mtk_btag);
	struct mtk_btag_trace *tr;
	struct btag_ufs_ctx *ctx;
	struct btag_ufs_ctx_data *data;
	unsigned long flags;
	__u32 qid;

	if (!rt)
		return;

	for (qid = 0; qid < ufs_mtk_btag->ctx.count; qid++) {
		ctx = btag_ufs_ctx(qid);
		if (!ctx)
			break;

		data = btag_switch_ctx_data(ctx);
		if (!data)
			continue;

		spin_lock_irqsave(&rt->lock, flags);
		tr = mtk_btag_curr_trace(rt);
		if (!tr) {
			pr_notice("Get current ringbuffer failed\n");
			goto unlock;
		}

		memset(tr, 0, sizeof(struct mtk_btag_trace));
		tr->pid = 0;
		tr->qid = qid;
		btag_ufs_data_eval(data, tr);
		mtk_btag_vmstat_eval(&tr->vmstat);
		mtk_btag_cpu_eval(&tr->cpu);
		mtk_btag_next_trace(rt);

unlock:
		spin_unlock_irqrestore(&rt->lock, flags);
		memset(data, 0, sizeof(struct btag_ufs_ctx_data));
		spin_lock_init(&data->tp.lock);
		spin_lock_init(&data->wl.lock);
		spin_lock_init(&data->pidlog.lock);
	}
}

static size_t btag_ufs_seq_debug_show_info(char **buff, unsigned long *size,
					   struct seq_file *seq)
{
	BTAG_PRINTF(buff, size, seq, "CTX Status: %s\n",
		    ufs_mtk_btag->ctx_enable ? "Enable" : "Disable");
	BTAG_PRINTF(buff, size, seq,
		    "echo n > /proc/blocktag/ufs/blockio, n presents:\n");
	BTAG_PRINTF(buff, size, seq, "  Clear all trace   : 0\n");
	BTAG_PRINTF(buff, size, seq, "  Enable CTX trace  : 1\n");
	BTAG_PRINTF(buff, size, seq, "  Disable CTX trace : 2\n");
	return 0;
}

static ssize_t btag_ufs_proc_write(const char __user *ubuf, size_t count)
{
	struct mtk_btag_ringtrace *rt = BTAG_RT(ufs_mtk_btag);
	unsigned long flags;
	char cmd[16] = {0};
	int ret;

	if (!count || !ufs_mtk_btag || !rt)
		goto err;

	if (count > 16) {
		pr_info("proc_write: command too long\n");
		goto err;
	}

	ret = copy_from_user(cmd, ubuf, count);
	if (ret < 0)
		goto err;

	/* remove line feed */
	cmd[count - 1] = 0;

	if (!strcmp(cmd, "0")) {
		spin_lock_irqsave(&rt->lock, flags);
		memset(rt->trace, 0, (sizeof(struct mtk_btag_trace) * rt->max));
		rt->index = 0;
		spin_unlock_irqrestore(&rt->lock, flags);
	} else if (!strcmp(cmd, "1")) {
		ufs_mtk_btag->ctx_enable = true;
		pr_info("UFS CTX Enable\n");
	} else if (!strcmp(cmd, "2")) {
		ufs_mtk_btag->ctx_enable = false;
		pr_info("UFS CTX Disable\n");
	} else {
		pr_info("proc_write: invalid cmd %s\n", cmd);
		goto err;
	}

	return count;
err:
	return -1;
}
static void btag_ufs_init_ctx(struct mtk_blocktag *btag)
{
	struct btag_ufs_ctx *ctx = BTAG_CTX(btag);
	__u64 time = sched_clock();
	int qid;

	if (!ctx)
		return;

	memset(ctx, 0, sizeof(struct btag_ufs_ctx) * btag->ctx.count);
	for (qid = 0; qid < btag->ctx.count; qid++) {
		spin_lock_init(&ctx[qid].data[0].tp.lock);
		spin_lock_init(&ctx[qid].data[1].tp.lock);
		spin_lock_init(&ctx[qid].data[0].wl.lock);
		spin_lock_init(&ctx[qid].data[1].wl.lock);
		spin_lock_init(&ctx[qid].data[0].pidlog.lock);
		spin_lock_init(&ctx[qid].data[1].pidlog.lock);
		ctx[qid].data[0].wl.window_begin = time;
		ctx[qid].data[0].wl.idle_begin = time;
		rcu_assign_pointer(ctx[qid].cur_data, &ctx[qid].data[0]);
	}

	if (IS_ENABLED(CONFIG_MTK_BLOCK_IO_DEBUG_BUILD) || btag->ctx.count == 1)
		btag->ctx_enable = true;
	else
		btag->ctx_enable = false;
}

static struct mtk_btag_vops btag_ufs_vops = {
	.seq_show = btag_ufs_seq_debug_show_info,
	.sub_write = btag_ufs_proc_write,
};

int mtk_btag_ufs_init(struct ufs_mtk_host *host, __u32 ufs_nr_queue,
		      __u32 ufs_nutrs)
{
	struct mtk_blocktag *btag;

	if (!host)
		return -1;

	if (host->qos_allowed)
		btag_ufs_vops.earaio_enabled = true;

	if (host->boot_device)
		btag_ufs_vops.boot_device = true;

	tag_per_queue = ufs_nutrs;
	nr_queue = ufs_nr_queue;
	nr_tag = nr_queue * tag_per_queue;

	if (tag_per_queue > BTAG_UFS_MAX_TAG_PER_QUEUE ||
	    nr_queue > BTAG_UFS_MAX_QUEUE ||
	    nr_tag > BTAG_UFS_MAX_TAG || nr_tag > BTAG_MAX_TAG) {
		tag_per_queue = 0;
		nr_queue = 0;
		nr_tag = 0;
		return -1;
	}

	ufs_mtk_btag_wq = alloc_workqueue("ufs_mtk_btag",
					  WQ_FREEZABLE | WQ_UNBOUND, 1);
	INIT_WORK(&ufs_mtk_btag_worker, btag_ufs_work);

	btag = mtk_btag_alloc("ufs",
			      BTAG_STORAGE_UFS,
			      BTAG_UFS_RINGBUF_MAX,
			      sizeof(struct btag_ufs_ctx),
			      nr_queue, &btag_ufs_vops);

	if (btag) {
		btag_ufs_init_ctx(btag);
		ufs_mtk_btag = btag;
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_init);

int mtk_btag_ufs_exit(void)
{
	mtk_btag_free(ufs_mtk_btag);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_btag_ufs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Mediatek UFS Block IO Tracer");
MODULE_AUTHOR("Perry Hsu <perry.hsu@mediatek.com>");
MODULE_AUTHOR("Stanley Chu <stanley chu@mediatek.com>");

