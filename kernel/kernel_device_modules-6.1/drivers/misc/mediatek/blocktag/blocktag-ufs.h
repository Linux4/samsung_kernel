/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_TRACER_H
#define _UFS_MEDIATEK_TRACER_H

#include <linux/types.h>
#include "blocktag-internal.h"

#if IS_ENABLED(CONFIG_SCSI_UFS_HPB)
#include "ufshpb.h"
#endif

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)

#define BTAG_UFS_RINGBUF_MAX            1000
#define UFS_LOGBLK_SHIFT                12
#define BTAG_UFS_MAX_TAG_PER_QUEUE      64
#define BTAG_UFS_MAX_QUEUE              8
#define BTAG_UFS_MAX_TAG \
	((BTAG_UFS_MAX_TAG_PER_QUEUE) *	(BTAG_UFS_MAX_QUEUE))

struct btag_ufs_tag {
	__u64 start_t;
	__u32 len;
	__u16 cmd;
};

struct btag_ufs_throughput {
	spinlock_t lock;
	__u64 usage[BTAG_IO_TYPE_NR];
	__u64 size[BTAG_IO_TYPE_NR];
};

struct btag_ufs_workload {
	spinlock_t lock;
	__u64 window_begin;
	__u64 idle_begin;
	__u64 idle_total;
	__u64 req_cnt;
	__u16 depth;
};

struct btag_ufs_ctx_data {
	struct btag_ufs_tag tags[BTAG_UFS_MAX_TAG_PER_QUEUE];
	struct btag_ufs_throughput tp;
	struct btag_ufs_workload wl;
	struct mtk_btag_proc_pidlogger pidlog;
};

struct btag_ufs_ctx {
	struct btag_ufs_ctx_data __rcu *cur_data;
	struct btag_ufs_ctx_data data[2];
};

#endif

#endif
