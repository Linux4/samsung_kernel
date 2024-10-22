// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 *	Peter Wang <peter.wang@mediatek.com>
 */

#include <linux/async.h>
#include <scsi/scsi_cmnd.h>
#include <ufs/ufshcd.h>
#include "ufs-mediatek.h"
#include "ufs-mediatek-sysfs.h"
#include <mt-plat/mtk_blocktag.h>
#if IS_ENABLED(CONFIG_SCSI_UFS_HPB)
#include "ufshpb.h"
#endif

static bool ufs_mtk_is_data_cmd(struct scsi_cmnd *cmd)
{
	char cmd_op = cmd->cmnd[0];

	if (cmd_op == WRITE_10 || cmd_op == READ_10 ||
	    cmd_op == WRITE_16 || cmd_op == READ_16 ||
	    cmd_op == WRITE_6 || cmd_op == READ_6
#if IS_ENABLED(CONFIG_SCSI_UFS_HPB)
	    || cmd_op == UFSHPB_READ
#endif
	    )
		return true;

	return false;
}

static struct ufs_hw_queue *ufs_mtk_mcq_req_to_hwq(struct ufs_hba *hba,
					    struct request *req)
{
	u32 utag = blk_mq_unique_tag(req);
	u32 hwq = blk_mq_unique_tag_to_hwq(utag);

	/* uhq[0] is used to serve device commands */
	return &hba->uhq[hwq];
}

void ufs_mtk_btag_send_command(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct ufs_hw_queue *hq;
	__u16 qid = 0;

	if (!atomic_read(&host->skip_btag) && ufs_mtk_is_data_cmd(cmd)) {
		if (is_mcq_enabled(hba)) {
			hq = ufs_mtk_mcq_req_to_hwq(hba, scsi_cmd_to_rq(cmd));
			qid = hq->id;
		}
		mtk_btag_ufs_send_command(lrbp->task_tag, qid, cmd);
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_btag_send_command);

void ufs_mtk_btag_compl_command(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct ufs_hw_queue *hq;
	__u16 qid = 0;

	if (!atomic_read(&host->skip_btag) && ufs_mtk_is_data_cmd(cmd)) {
		if (is_mcq_enabled(hba)) {
			hq = ufs_mtk_mcq_req_to_hwq(hba, scsi_cmd_to_rq(cmd));
			qid = hq->id;
		}
		mtk_btag_ufs_transfer_req_compl(lrbp->task_tag, qid);
	}
}
EXPORT_SYMBOL_GPL(ufs_mtk_btag_compl_command);

static void ufs_mtk_blocktag_add(void *data, async_cookie_t cookie)
{
	struct ufs_hba *hba = (struct ufs_hba *)data;
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	if (!wait_for_completion_timeout(&host->luns_added, 10 * HZ)) {
		dev_info(hba->dev, "%s: LUNs not ready before timeout. blocktag init failed",
			 __func__);
		return;
	}

	mtk_btag_ufs_init(host, hba->nr_hw_queues, hba->nutrs);
}

void ufs_mtk_btag_init(struct ufs_hba *hba)
{
	async_schedule(ufs_mtk_blocktag_add, hba);
}
EXPORT_SYMBOL_GPL(ufs_mtk_btag_init);

void ufs_mtk_btag_exit(struct ufs_hba *hba)
{
	mtk_btag_ufs_exit();
}
EXPORT_SYMBOL_GPL(ufs_mtk_btag_exit);

