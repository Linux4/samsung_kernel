// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Authors:
 *	Stanley Chu <stanley.chu@mediatek.com>
 *	Peter Wang <peter.wang@mediatek.com>
 */

#include <asm-generic/errno-base.h>
#include <asm/unaligned.h>
#include <linux/async.h>
#include <linux/container_of.h>
#include <linux/delay.h>
#include <linux/dev_printk.h>
#include <linux/kern_levels.h>
#include <linux/rpmb.h>
#include <linux/soc/mediatek/mtk_ise_lpm.h>
#include <linux/soc/mediatek/mtk-ise-mbox.h>
#include <linux/stddef.h>
#include <linux/timer.h>
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_dbg.h>
#include <ufs/ufshcd.h>
#include "ufs-mediatek-ise.h"
#include "ufs-mediatek.h"
#include "ufshcd-priv.h"

static struct rpmb_dev *rawdev_ufs_rpmb;
static int ufs_mtk_rpmb_cmd_seq(struct device *dev,
				struct rpmb_cmd *cmds,
				u32 ncmds, u8 region);

static struct rpmb_ops ufs_mtk_rpmb_dev_ops = {
	.cmd_seq = ufs_mtk_rpmb_cmd_seq,
	.type = RPMB_TYPE_UFS,
};

#define SEC_PROTOCOL_UFS  0xEC
#define SEC_SPECIFIC_UFS_RPMB 0x0001

#define SEC_PROTOCOL_CMD_SIZE 12
#define SEC_PROTOCOL_RETRIES 3
#define SEC_PROTOCOL_RETRIES_ON_RESET 10
#define SEC_PROTOCOL_TIMEOUT msecs_to_jiffies(30000)

int ufs_mtk_rpmb_security_out(struct scsi_device *sdev,
			 struct rpmb_frame *frames, u32 cnt, u8 region)
{
	struct scsi_sense_hdr sshdr = {0};
	u32 trans_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_OUT;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16((region << 8) | SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                              /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(trans_len, cmd + 6);  /* transfer length */

retry:
	ret = scsi_execute_cmd(sdev, cmd, REQ_OP_DRV_OUT, frames,
				trans_len, SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
				&(struct scsi_exec_args) {
					.sshdr = &sshdr,
					.resid = NULL,
				});

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_info(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (scsi_sense_valid(&sshdr) && sshdr.sense_key)
		scsi_print_sense_hdr(sdev, "rpmb: security out", &sshdr);

	return ret;
}

int ufs_mtk_rpmb_security_in(struct scsi_device *sdev,
			struct rpmb_frame *frames, u32 cnt, u8 region)
{
	struct scsi_sense_hdr sshdr = {0};
	u32 alloc_len = cnt * sizeof(struct rpmb_frame);
	int reset_retries = SEC_PROTOCOL_RETRIES_ON_RESET;
	int ret;
	u8 cmd[SEC_PROTOCOL_CMD_SIZE];

	memset(cmd, 0, SEC_PROTOCOL_CMD_SIZE);
	cmd[0] = SECURITY_PROTOCOL_IN;
	cmd[1] = SEC_PROTOCOL_UFS;
	put_unaligned_be16((region << 8) | SEC_SPECIFIC_UFS_RPMB, cmd + 2);
	cmd[4] = 0;                             /* inc_512 bit 7 set to 0 */
	put_unaligned_be32(alloc_len, cmd + 6); /* allocation length */

retry:
	ret = scsi_execute_cmd(sdev, cmd, REQ_OP_DRV_IN, frames,
				alloc_len, SEC_PROTOCOL_TIMEOUT, SEC_PROTOCOL_RETRIES,
				&(struct scsi_exec_args) {
					.sshdr = &sshdr,
					.resid = NULL,
				});

	if (ret && scsi_sense_valid(&sshdr) &&
	    sshdr.sense_key == UNIT_ATTENTION)
		/*
		 * Device reset might occur several times,
		 * give it one more chance
		 */
		if (--reset_retries > 0)
			goto retry;

	if (ret)
		dev_info(&sdev->sdev_gendev, "%s: failed with err %0x\n",
			__func__, ret);

	if (scsi_sense_valid(&sshdr) && sshdr.sense_key)
		scsi_print_sense_hdr(sdev, "rpmb: security in", &sshdr);

	return ret;
}

static int ufs_mtk_rpmb_cmd_seq(struct device *dev,
			       struct rpmb_cmd *cmds, u32 ncmds, u8 region)
{
	unsigned long flags;
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_mtk_host *host;
	struct scsi_device *sdev;
	struct rpmb_cmd *cmd;
	int i;
	int ret;

	host = ufshcd_get_variant(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	sdev = host->sdev_rpmb;
	if (sdev) {
		ret = scsi_device_get(sdev);
		if (!ret && !scsi_device_online(sdev)) {
			ret = -ENODEV;
			scsi_device_put(sdev);
		}
	} else {
		ret = -ENODEV;
	}
	spin_unlock_irqrestore(hba->host->host_lock, flags);
	if (ret)
		return ret;

	/*
	 * Send all command one by one.
	 * Use rpmb lock to prevent other rpmb read/write threads cut in line.
	 * Use mutex not spin lock because in/out function might sleep.
	 */
	down(&host->rpmb_sem);
	/* Ensure device is resumed before RPMB operation */
	scsi_autopm_get_device(sdev);

	for (ret = 0, i = 0; i < ncmds && !ret; i++) {
		cmd = &cmds[i];
		if (cmd->flags & RPMB_F_WRITE)
			ret = ufs_mtk_rpmb_security_out(sdev, cmd->frames,
						       cmd->nframes, region);
		else
			ret = ufs_mtk_rpmb_security_in(sdev, cmd->frames,
						      cmd->nframes, region);
	}

	/* Allow device to be runtime suspended */
	scsi_autopm_put_device(sdev);
	up(&host->rpmb_sem);

	scsi_device_put(sdev);
	return ret;
}

/**
 * @brief PURGE_TIMEOUT_MS: In worst case, purge is not completed and no
 * status polling command is sent in last PURGE_TIMEOUT_MS. We should
 * release host RPM and let device sleep.
 */
#define PURGE_TIMEOUT_MS 5000
static void ufs_mtk_rpmb_purge_tmr_update(struct ufs_hba *hba, bool activate)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);

	spin_lock(&host->purge_lock);
	if (activate && !host->purge_active) {
		ufshcd_rpm_get_sync(hba);
		host->purge_active = true;
		mod_timer(&host->purge_timer, jiffies + msecs_to_jiffies(PURGE_TIMEOUT_MS));
		dev_info(hba->dev, "wl rpm get +");
	} else if (!activate && host->purge_active) {
		host->purge_active = false;
		ufshcd_rpm_put(hba);
		del_timer(&host->purge_timer);
		dev_info(hba->dev, "wl rpm put -");
	} else if (activate && host->purge_active) {
		/* Simply refresh timer */
		mod_timer(&host->purge_timer, jiffies + msecs_to_jiffies(PURGE_TIMEOUT_MS));
		dev_info(hba->dev, "update timer");
	}
	spin_unlock(&host->purge_lock);
}

void ufs_rpmb_vh_compl_command(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct request *rq = scsi_cmd_to_rq(cmd);
	struct bio_vec bvec;
	struct bvec_iter iter;
	size_t bytes = 0;
	size_t len = 0;
	u8 *_buf = NULL;
	struct rpmb_frame *rframe = NULL;

	char buf[sizeof(struct rpmb_frame)];
	const size_t buf_sz = sizeof(buf);

	if (cmd->cmnd[0] == SECURITY_PROTOCOL_IN) {

		dev_dbg(hba->dev, "SECURITY_PROTOCOL_IN");
		if (!rq->bio)
			return;

		scsi_dma_unmap(cmd);

		bio_for_each_segment(bvec, rq->bio, iter) {

			_buf = bvec_kmap_local(&bvec);
			if (!_buf) {
				dev_warn(hba->dev, "empty page");
				continue;
			}

			len =  min(buf_sz - bytes,	(size_t) bvec.bv_len);
			memcpy(buf + bytes, _buf, len);

			kunmap_local(_buf);
			_buf = NULL;

			bytes += len;
			if (bytes >= buf_sz){
				/* First RPMB frame has enough info */
				break;
			}
		}

		if (bytes < buf_sz) {
			dev_warn(hba->dev, "%s: unexpected scsi data size %zu less than %zu", __func__, bytes, buf_sz);
			WARN_ON_ONCE(true);
			return;
		}

		rframe = (void *)buf;

		dev_dbg(hba->dev, "req_resp=0x%x, result=0x%x",
			be16_to_cpu(rframe->req_resp), be16_to_cpu(rframe->result));

		if (be16_to_cpu(rframe->req_resp) == RPMB_RESP_PURGE_ENABLE) {
			dev_dbg(hba->dev, "RPMB_RESP_PURGE_ENABLE rsp found");
			ufs_mtk_rpmb_purge_tmr_update(hba, true);
		}

		if (be16_to_cpu(rframe->req_resp) == RPMB_RESP_PURGE_STATUS_READ) {
			dev_dbg(hba->dev, "RPMB_RESP_PURGE_STATUS_READ rsp found, status=%d", rframe->data[0]);
			if (rframe->data[0] != RPMB_PURGE_STA_IN_PROGRESS) {
				ufs_mtk_rpmb_purge_tmr_update(hba, false);
			} else {
				/* refresh timer */
				ufs_mtk_rpmb_purge_tmr_update(hba, true);
			}
		}

		/* Dump RPMB frame start from nonce to the end */
		print_hex_dump(KERN_DEBUG, "tail", DUMP_PREFIX_OFFSET,
			16, 8, buf + 484, 28, false);
	}

}

static void ufs_mtk_rpmb_purge_timer_cb(struct timer_list *list)
{
	struct ufs_mtk_host *host = container_of(list, struct ufs_mtk_host, purge_timer);
	struct ufs_hba *hba = host->hba;

	/* In case user did not query status until purge finish */
	ufs_mtk_rpmb_purge_tmr_update(hba, false);
	dev_info(hba->dev, "purge timer force deactivate");
}

struct ufs_hba *g_hba;
/**
 * ufs_mtk_rpmb_ddd - add mtk rpmb cdev
 * @data: host controller instance (hba)
 *
 * Read max ufs device read/write rpmb size support and
 * set to reliable_wr_cnt for rpmb cdev read/write reference.
 *
 * Register raw cdve device in rawdev_ufs_rpmb
 */
static void ufs_mtk_rpmb_add(void *data, async_cookie_t cookie)
{
	int err;
	u8 *desc_buf;
	struct rpmb_dev *rdev;
	u8 rw_size;
	struct ufs_mtk_host *host;
	struct ufs_hba *hba = (struct ufs_hba *)data;
	struct scsi_device *sdev = NULL;

	host = ufshcd_get_variant(hba);
	g_hba = hba;

	sema_init(&host->rpmb_sem, 0);

	err = wait_for_completion_timeout(&host->luns_added, 10 * HZ);
	if (err == 0) {
		dev_info(hba->dev, "%s: LUNs not ready before timeout. RPMB init failed", __func__);
		goto out;
	}

	/* add sdev_rpmb */
	shost_for_each_device(sdev, hba->host) {
		if (sdev->lun == ufshcd_upiu_wlun_to_scsi_wlun(UFS_UPIU_RPMB_WLUN)) { /* rpmb lun */
			host->sdev_rpmb = sdev;
			/* break out shost_for_each_device should call scsi_device_put(sdev) */
			scsi_device_put(sdev);
			goto find_exit;
		}
	}

	dev_info(hba->dev, "%s: scsi rpmb device cannot found\n", __func__);
		goto out;

find_exit:

	desc_buf = kmalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf)
		goto out;

	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_GEOMETRY, 0, 0,
				     desc_buf, QUERY_DESC_MAX_SIZE);
	if (err) {
		dev_info(hba->dev, "%s: cannot get rpmb rw limit %d\n",
			 dev_name(hba->dev), err);
		/* fallback to singel frame write */
		rw_size = 1;
	} else {
		rw_size = desc_buf[GEOMETRY_DESC_PARAM_RPMB_RW_SIZE];
	}

	kfree(desc_buf);
	dev_info(hba->dev, "rpmb rw_size: %d\n", rw_size);

	ufs_mtk_rpmb_dev_ops.reliable_wr_cnt = rw_size;

	if (unlikely(scsi_device_get(host->sdev_rpmb)))
		goto out;

	rdev = rpmb_dev_register(hba->dev, &ufs_mtk_rpmb_dev_ops);
	if (IS_ERR(rdev)) {
		dev_info(hba->dev, "%s: cannot register to rpmb %ld\n",
			 dev_name(hba->dev), PTR_ERR(rdev));
		goto out_put_dev;
	}

	/*
	 * Preserve rpmb_dev to globals for connection of legacy
	 * rpmb ioctl solution.
	 */
	rawdev_ufs_rpmb = rdev;

	/*
	 *  Replace default rpm_autosuspend_delay
	 */
	if (host->sdev_rpmb->rpm_autosuspend)
		pm_runtime_set_autosuspend_delay(&host->sdev_rpmb->sdev_gendev,
						 MTK_RPM_AUTOSUSPEND_DELAY_MS);
out_put_dev:
	scsi_device_put(host->sdev_rpmb);

out:
	up(&host->rpmb_sem);
}


struct rpmb_dev *ufs_mtk_rpmb_get_raw_dev(void)
{
	return rawdev_ufs_rpmb;
}
EXPORT_SYMBOL_GPL(ufs_mtk_rpmb_get_raw_dev);

void ufs_mtk_rpmb_init(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host = ufshcd_get_variant(hba);
	ufs_mtk_ise_rpmb_probe(hba);
	async_schedule(ufs_mtk_rpmb_add, hba);

	spin_lock_init(&host->purge_lock);
	timer_setup(&host->purge_timer, ufs_mtk_rpmb_purge_timer_cb, 0);

}
EXPORT_SYMBOL_GPL(ufs_mtk_rpmb_init);

