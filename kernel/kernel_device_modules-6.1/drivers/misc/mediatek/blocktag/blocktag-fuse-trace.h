/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _BLOCKTAG_FUSE_TRACE_H
#define _BLOCKTAG_FUSE_TRACE_H

#include <linux/types.h>
#include "blocktag-internal.h"

#define FUSE_MAX_LOG (512)

struct fuse_log_s {
	uint64_t ns_time;
	int pid;
	int event_type;		/* trace event type */
	const char *func;
	int line;
	struct inode *inode;
	struct inode *backing;
	u64 nodeid;
	u64 nlookup;
	u64 nlookup_after;
};

struct fuse_logs_s {
	long max;				/* max number of logs */
	long nr;				/* current number of logs */
	struct fuse_log_s trace[FUSE_MAX_LOG];
	int head;
	int tail;
	spinlock_t lock;
};

enum btag_fuse_event {
	MTK_FUSE_NLOOKUP = 0,
	MTK_FUSE_QUEUE_FORGET,
	MTK_FUSE_FORCE_FORGET,
	MTK_FUSE_IGET_BACKING,
	NR_BTAG_FUSE_EVENT,
};

void btag_fuse_nlookup(void *data, const char *func, int line,
			   struct inode *inode, u64 nodeid,
			   u64 nlookup, u64 nlookup_after);
void btag_fuse_queue_forget(void *data, const char *func, int line,
				u64 nodeid, u64 nlookup);
void btag_fuse_force_forget(void *data, const char *func, int line,
				struct inode *inode, u64 nodeid, u64 nlookup);
void btag_fuse_iget_backing(void *data, const char *func, int line,
			    struct inode *inode, u64 nodeid, struct inode *b);

void mtk_btag_fuse_show(char **buff, unsigned long *size,
		struct seq_file *seq);

void mtk_btag_fuse_init(void);

#endif /* _BLOCKTAG_FUSE_TRACE_H */
