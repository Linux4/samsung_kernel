// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[blocktag][fuse]" fmt

#include <linux/sched/clock.h>
#include "blocktag-fuse-trace.h"

struct fuse_logs_s fuse_traces;

static const char *event_name(int event)
{
	const char *ret;

	switch (event) {
	case MTK_FUSE_NLOOKUP:
		ret = "mtk_fuse_nlookup";
		break;
	case MTK_FUSE_QUEUE_FORGET:
		ret = "mtk_fuse_queue_forget";
		break;
	case MTK_FUSE_FORCE_FORGET:
		ret = "mtk_fuse_force_forget";
		break;
	case MTK_FUSE_IGET_BACKING:
		ret = "mtk_fuse_iget_backing";
		break;
	default:
		ret = "";
	}

	return ret;
}

static void fuse_add_log(int event, const char *func, int line,
				struct inode *inode, u64 nodeid,
				u64 nlookup, u64 nlookup_after,
				struct inode *backing)
{
	uint64_t ns_time = sched_clock();
	unsigned long flags;

	spin_lock_irqsave(&fuse_traces.lock, flags);
	fuse_traces.tail = (fuse_traces.tail + 1) % FUSE_MAX_LOG;
	if (fuse_traces.tail == fuse_traces.head)
		fuse_traces.head = (fuse_traces.head + 1) % FUSE_MAX_LOG;
	if (unlikely(fuse_traces.head == -1))
		fuse_traces.head++;

	fuse_traces.trace[fuse_traces.tail].ns_time = ns_time;
	fuse_traces.trace[fuse_traces.tail].pid = current->pid;
	fuse_traces.trace[fuse_traces.tail].event_type = event;
	fuse_traces.trace[fuse_traces.tail].func = func;
	fuse_traces.trace[fuse_traces.tail].line = line;
	fuse_traces.trace[fuse_traces.tail].inode = inode;
	fuse_traces.trace[fuse_traces.tail].nodeid = nodeid;
	fuse_traces.trace[fuse_traces.tail].nlookup = nlookup;
	fuse_traces.trace[fuse_traces.tail].nlookup_after = nlookup_after;
	fuse_traces.trace[fuse_traces.tail].backing = backing;
	spin_unlock_irqrestore(&fuse_traces.lock, flags);
}

void btag_fuse_nlookup(void *data, const char *func, int line,
			   struct inode *inode, u64 nodeid,
			   u64 nlookup, u64 nlookup_after)
{
	fuse_add_log(MTK_FUSE_NLOOKUP, func, line, inode, nodeid,
			nlookup, nlookup_after, NULL);
}

void btag_fuse_queue_forget(void *data, const char *func, int line,
				u64 nodeid, u64 nlookup)
{
	fuse_add_log(MTK_FUSE_QUEUE_FORGET, func, line, NULL, nodeid,
			nlookup, 0, NULL);
}

void btag_fuse_force_forget(void *data, const char *func, int line,
				struct inode *inode, u64 nodeid, u64 nlookup)
{
	fuse_add_log(MTK_FUSE_FORCE_FORGET, func, line, inode, nodeid,
			nlookup, 0, NULL);
}

void btag_fuse_iget_backing(void *data, const char *func, int line,
			    struct inode *inode, u64 nodeid, struct inode *b)
{
	fuse_add_log(MTK_FUSE_IGET_BACKING, func, line, inode, nodeid, 0, 0, b);
}

void mtk_btag_fuse_show(char **buff, unsigned long *size,
		struct seq_file *seq)
{
	struct fuse_log_s *tr;
	int idx;
	unsigned long flags;

	spin_lock_irqsave(&fuse_traces.lock, flags);
	idx = fuse_traces.tail;

	while (idx >= 0) {
		tr = &fuse_traces.trace[idx];
		switch (tr->event_type) {
		case MTK_FUSE_NLOOKUP:
			BTAG_PRINTF(buff, size, seq,
				    "[%lld]%d:%s,%s:%d,inode=0x%p,nodeid=%llu,nlookup=%llu,nlookup_after=%llu\n",
				    tr->ns_time,
				    tr->pid,
				    event_name(tr->event_type),
				    tr->func,
				    tr->line,
				    tr->inode,
				    tr->nodeid,
				    tr->nlookup,
				    tr->nlookup_after);
			break;
		case MTK_FUSE_QUEUE_FORGET:
			BTAG_PRINTF(buff, size, seq,
				    "[%lld]%d,%s,%s:%d,nodeid=%llu,nlookup=%llu\n",
				    tr->ns_time,
				    tr->pid,
				    event_name(tr->event_type),
				    tr->func,
				    tr->line,
				    tr->nodeid,
				    tr->nlookup);
			break;
		case MTK_FUSE_FORCE_FORGET:
			BTAG_PRINTF(buff, size, seq,
				    "[%lld]%d,%s,%s:%d,inode=0x%p,nodeid=%llu,nlookup=%llu\n",
				    tr->ns_time,
				    tr->pid,
				    event_name(tr->event_type),
				    tr->func,
				    tr->line,
				    tr->inode,
				    tr->nodeid,
				    tr->nlookup);
			break;
		case MTK_FUSE_IGET_BACKING:
			BTAG_PRINTF(buff, size, seq,
				    "[%lld]%d,%s,%s:%d,inode=0x%p,nodeid=%llu,backing=0x%p\n",
				    tr->ns_time,
				    tr->pid,
				    event_name(tr->event_type),
				    tr->func,
				    tr->line,
				    tr->inode,
				    tr->nodeid,
				    tr->backing);
			break;
		default:
			break;
		}

		if (idx == fuse_traces.head)
			break;
		idx = idx ? idx - 1 : FUSE_MAX_LOG - 1;
	}
	spin_unlock_irqrestore(&fuse_traces.lock, flags);
}

void mtk_btag_fuse_init(void)
{
	fuse_traces.head = -1;
	fuse_traces.tail = -1;
	spin_lock_init(&fuse_traces.lock);
}
