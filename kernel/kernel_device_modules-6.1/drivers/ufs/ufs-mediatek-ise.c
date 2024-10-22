// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Authors:
 *	Po-Wen Kao <powen.kao@mediatek.com>
 */
#include <asm-generic/errno-base.h>
#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/soc/mediatek/mtk_ise_lpm.h>
#include <linux/soc/mediatek/mtk-ise-mbox.h>
#include <ufs/ufshcd.h>
#include <ufshcd-priv.h>

#include "ufs-mediatek-dbg.h"
#include "ufs-mediatek-mimic.h"
#include "ufs-mediatek-priv.h"
#include "ufs-mediatek.h"

extern struct ufs_hba *g_hba;

static inline bool ufshcd_is_host_ready(struct ufs_hba *hba)
{
	return ufshcd_readl(hba, REG_CONTROLLER_CAPABILITIES) != 0;
}

#define BLOCK_IO_TIMEOUT_US (1 * 1000 * 1000)

static inline int ufshcd_unblock_io(struct ufs_hba *hba)
{
	ufsm_scsi_unblock_requests(hba);
	mutex_unlock(&hba->dev_cmd.lock);

	return 0;
}

static inline int ufshcd_block_io(struct ufs_hba *hba)
{
	int ret = 0;
	/* block both dev command and IO request */
	mutex_lock(&hba->dev_cmd.lock);
	ufsm_scsi_block_requests(hba);
	ret = ufsm_wait_for_doorbell_clr(hba, BLOCK_IO_TIMEOUT_US);

	if (ret) {
		dev_err(hba->dev, "%s: wait cmd clear failed (%d)", __func__, ret);

		/* unblock IO*/
		ufshcd_unblock_io(hba);
	}

	return ret;
}


static int ise_rpmb_open(struct inode *node, struct file *file)
{
	return 0;
}

static int ise_rpmb_release(struct inode *node, struct file *file)
{
	return 0;
}

enum ise_rpmb_key_srv_cmd {
	ISE_RPMB_KEY_CMD_PROGRAM_KEY = 0x1,
};

enum ise_rpmb_mailbox_status {
	ISE_RPMB_MB_STA_NONE = 0,
	ISE_RPMB_MB_STA_SUCCESS,
	ISE_RPMB_MB_STA_FAILED,
};

enum rpmb_protocol_id {
	RPMB_SR_ID_KEY = 1,
};

#define ISE_RPMB_SRV_VER 1
static int ise_rpmb_program_key(struct ufs_hba *hba)
{
#if IS_ENABLED(CONFIG_MTK_ISE_MBOX) && IS_ENABLED(CONFIG_MTK_ISE_LPM_SUPPORT)
	mailbox_request_t request = {0};
	mailbox_payload_t payload = {0};
	mailbox_reply_t reply = {0};
	uint32_t result = ISE_RPMB_MB_STA_FAILED;

	if (mtk_ise_awake_lock(ISE_REE)) {
		dev_err(hba->dev, "%s: ise power on failed\n", __func__);
		goto out;
	}

	payload.size = 1;
	payload.fields[0] = ISE_RPMB_KEY_CMD_PROGRAM_KEY;

	reply = ise_mailbox_request(&request, &payload, REQUEST_RPMB,
		RPMB_SR_ID_KEY, ISE_RPMB_SRV_VER);
	if (reply.status.error != MAILBOX_SUCCESS)
		dev_err(hba->dev, "%s: request mailbox failed 0x%x\n", __func__, reply.status.error);
	else
		result = reply.payload.fields[0];

	if (mtk_ise_awake_unlock(ISE_REE))
		dev_err(hba->dev, "%s: ise power off failed\n", __func__);

out:
	if (result == ISE_RPMB_MB_STA_SUCCESS) {
		dev_info(hba->dev, "Key programming success");
		return 0;
	}

	dev_err(hba->dev, "Key programming failed");
	return -EIO;

#else
	pr_info("iSE program RPMB key service not available. \
		 Please enable config MTK_ISE_MBOX, MTK_ISE_LPM_SUPPORT");
	return -EINVAL;
#endif
}

enum {
	UFS_REG_CDACFG   = 0xd0,
	UFS_REG_CDATX1   = 0xd4,
	UFS_REG_CDATX2   = 0xd8,
	UFS_REG_CDARX1   = 0xdc,
	UFS_REG_CDARX2   = 0xe0,
	UFS_REG_CDASTA   = 0xe4,
};

enum ise_rpmb_cmd {
	ISE_RPMB_PROGRAM_KEY,
};

/**
 * @brief Prepare for iSE key programming
 *
 * @return int
 */
static int ise_rpmb_prepare_kp(struct ufs_hba *hba)
{
	int ret;

	ret = ufshcd_rpm_get_sync(hba);
	if (ret < 0) {
		dev_err(hba->dev, "pm get failed (%d)", ret);
		return ret;
	}

	ufshcd_hold(hba, false);

	ret = ufshcd_block_io(hba);
	if (ret) {
		dev_err(hba->dev, "%s: block io faialed (%d)", __func__, ret);
		ufs_mtk_dbg_dump(10);
		goto pm_put;
	}

	ret = ufs_mtk_auto_hibern8_disable(hba);
	if (ret) {
		dev_err(hba->dev, "%s: disable AH8 failed (%d)", __func__, ret);
		goto io_unblock;
	}
	return 0;

io_unblock:
	ufshcd_unblock_io(hba);
pm_put:
	ufshcd_release(hba);
	ufshcd_rpm_put(hba);
	return ret;
}

static int ise_rpmb_unprepare_kp(struct ufs_hba *hba)
{
	/* unprepare for key programming */
	ufshcd_unblock_io(hba);
	ufshcd_release(hba);
	/* AH8 timer will be restored in next runtime pm cycle */
	return ufshcd_rpm_put(hba);
}

#define IOR_ISE_RPMB_PROGRAM_KEY _IOR(ISE_RPMB_PROGRAM_KEY,'a', int *)
static long ise_rpmb_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	struct ufs_hba *hba = g_hba;
	struct ufs_mtk_host *host;
	int ret;

	host = ufshcd_get_variant(hba);

	switch (cmd) {
	case IOR_ISE_RPMB_PROGRAM_KEY:
		ret = ise_rpmb_prepare_kp(hba);
		if (ret) {
			dev_err(hba->dev, "%s: prepare failed (%d)", __func__, ret);
			break;
		}

		/* send program key command to iSE through mailbox*/
		ret = ise_rpmb_program_key(hba);
		if (ret)
			dev_err(hba->dev, "%s: ise program key failed (%d)", __func__, ret);


		ise_rpmb_unprepare_kp(hba);
	break;
	default:
		ret = -EINVAL;
		dev_err(hba->dev, "%s: unknown command (%d)", __func__, cmd);
	}

	ret = copy_to_user((int *)arg, &ret, sizeof(ret));
	if (ret)
		dev_err(hba->dev, "%s: result copy_to_user failed (%d)", __func__, ret);

	return ret;
}

#ifdef CONFIG_COMPAT
static long ise_rpmb_compat_ioctl(struct file *fp, unsigned int cmd,
			      unsigned long arg)
{
	return ise_rpmb_ioctl(fp, cmd, (unsigned long)compat_ptr(arg));
}
#endif /* CONFIG_COMPAT */


static const struct file_operations ise_rpmb_fops = {
	.open           = ise_rpmb_open,
	.release        = ise_rpmb_release,
	.unlocked_ioctl = ise_rpmb_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = ise_rpmb_compat_ioctl,
#endif
	.owner          = THIS_MODULE,
	.llseek         = noop_llseek,
};

#define ISE_RPMB_NAME "ise-rpmb"
static struct class *ise_rpmb_class;
int ufs_mtk_ise_rpmb_probe(struct ufs_hba *hba)
{
	struct ufs_mtk_host *host;
	unsigned int major;
	dev_t dev = 0;
	struct device *device = NULL;
	int ret = 0;
	struct tag_ufs *atag;

	ise_rpmb_class = NULL;
	host = ufshcd_get_variant(hba);
	atag = host->atag;

	if (!atag) {
		dev_err(hba->dev, "%s: atag not found", __func__);
		return -EINVAL;
	}

	dev_info(hba->dev, "rpmb region 2 key state=%d", atag->rpmb_r2_kst);

	if (atag->rpmb_r2_kst != RPMB_KEY_ST_NOT_PROGRAMMED) {
		dev_err(hba->dev, "ise-rpmb node skipped");
		return -EPERM;
	}

	/* Only create node if ise RPMB key is not yet programmed */

	ret = alloc_chrdev_region(&dev, 0, 1, ISE_RPMB_NAME);
	if (ret) {
		pr_err( "%s, init alloc_chrdev_region failed!", __func__);
		goto unregister;
	}

	cdev_init(&host->ise_rpmb_cdev, &ise_rpmb_fops);
	host->ise_rpmb_cdev.owner = THIS_MODULE;
	major = MAJOR(dev);

	ret = cdev_add(&host->ise_rpmb_cdev, MKDEV(major, 0U), 1);
	if (ret) {
		pr_err("%s, init cdev_add failed!", __func__);
		goto unregister;
	}

	ise_rpmb_class = class_create(THIS_MODULE, ISE_RPMB_NAME);

	if (IS_ERR(ise_rpmb_class)) {
		pr_err( "%s, init class_create failed!", __func__);
		goto del_cdev;
	}

	device = device_create(ise_rpmb_class, NULL, MKDEV(major, 0), NULL,
		ISE_RPMB_NAME "%d", 0);
	if (IS_ERR(device)) {
		pr_err("%s, init device_create failed!", __func__);
		goto destroy_class;
	}
	return 0;

destroy_class:
	if (ise_rpmb_class)
		class_destroy(ise_rpmb_class);
del_cdev:
	cdev_del(&host->ise_rpmb_cdev);
unregister:
	unregister_chrdev_region(dev, 1);
	return -EPERM;
}
