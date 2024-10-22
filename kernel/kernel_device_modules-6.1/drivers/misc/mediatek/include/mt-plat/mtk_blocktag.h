/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _MTK_BLOCKTAG_H
#define _MTK_BLOCKTAG_H

#ifndef pr_fmt
#define pr_fmt(fmt) "[blocktag]" fmt
#endif

#include <linux/blk_types.h>
#include <linux/mmc/core.h>
#include <ufs/ufshcd.h>
#include "ufs-mediatek.h"

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)

enum mtk_btag_storage_type {
	BTAG_STORAGE_UFS     = 0,
	BTAG_STORAGE_MMC     = 1,
	BTAG_STORAGE_UNKNOWN = 2
};

#define BTAG_NAME_LEN           16
struct mtk_btag_mictx_id {
	enum mtk_btag_storage_type storage;
	char name[BTAG_NAME_LEN];
	__s8 id;
};

/*
 * public structure to provide IO statistics
 * in a period of time.
 *
 * Make sure MTK_BTAG_FEATURE_MICTX_IOSTAT is
 * defined alone with mictx series.
 */
struct mtk_btag_mictx_iostat_struct {
	__u64 duration;  /* duration time for below performance data (ns) */
	__u32 tp_req_r;  /* throughput (per-request): read  (KB/s) */
	__u32 tp_req_w;  /* throughput (per-request): write (KB/s) */
	__u32 tp_all_r;  /* throughput (overlapped) : read  (KB/s) */
	__u32 tp_all_w;  /* throughput (overlapped) : write (KB/s) */
	__u32 reqsize_r; /* request size : read  (Bytes) */
	__u32 reqsize_w; /* request size : write (Bytes) */
	__u32 reqcnt_r;  /* request count: read */
	__u32 reqcnt_w;  /* request count: write */
	__u16 wl;	/* storage device workload (%) */
	__u16 top;       /* ratio of request (size) by top-app */
	__u16 q_depth;   /* storage cmdq queue depth */
};

int mtk_btag_mictx_get_data(
	struct mtk_btag_mictx_id mictx_id,
	struct mtk_btag_mictx_iostat_struct *iostat);
void mtk_btag_mictx_enable(struct mtk_btag_mictx_id *mictx_id, bool enable);

int mtk_btag_ufs_init(struct ufs_mtk_host *host, __u32 ufs_nr_queue,
		      __u32 ufs_nutrs);
int mtk_btag_ufs_exit(void);
void mtk_btag_ufs_send_command(__u16 tid, __u16 qid, struct scsi_cmnd *cmd);
void mtk_btag_ufs_transfer_req_compl(__u16 tid, __u16 qid);

int mmc_mtk_biolog_init(struct mmc_host *mmc);
int mmc_mtk_biolog_exit(void);
void mmc_mtk_biolog_send_command(__u16 task_id, struct mmc_request *mrq);
void mmc_mtk_biolog_transfer_req_compl(struct mmc_host *mmc,
				       __u16 task_id,
				       unsigned long req_mask);
void mmc_mtk_biolog_check(struct mmc_host *mmc, unsigned long req_mask);

#else

#define mtk_btag_mictx_get_data(...)
#define mtk_btag_mictx_enable(...)

#define mtk_btag_ufs_init(...)
#define mtk_btag_ufs_exit(...)
#define mtk_btag_ufs_send_command(...)
#define mtk_btag_ufs_transfer_req_compl(...)

#define mmc_mtk_biolog_send_command(...)
#define mmc_mtk_biolog_transfer_req_compl(...)
#define mmc_mtk_biolog_init(...)
#define mmc_mtk_biolog_exit(...)
#define mmc_mtk_biolog_check(...)

#endif
#endif
