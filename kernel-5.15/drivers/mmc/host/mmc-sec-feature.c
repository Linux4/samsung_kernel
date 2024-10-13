// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *  Storage Driver <storage.sec@samsung.com>
 */

#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <trace/hooks/mmc.h>

#include "mtk-mmc.h"
#include "mmc-sec-feature.h"
#include "mmc-sec-sysfs.h"

struct sd_sec_vendor_info sdi;
struct device *sec_sdcard_cmd_dev;

static int sd_sec_sdcard_uevent(struct device *dev,
		struct kobj_uevent_env *env)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	int retval;
	bool card_exist = false;
	struct sd_sec_status_err_info *status_err = &sdi.status_err;

	add_uevent_var(env, "DEVNAME=%s", dev->kobj.name);

	if (host->card)
		card_exist = true;

	retval = add_uevent_var(env, "IOERROR=%s", card_exist ? (
			((status_err->ge_cnt && !(status_err->ge_cnt % 1000)) ||
			 (status_err->ecc_cnt && !(status_err->ecc_cnt % 1000)) ||
			 (status_err->wp_cnt && !(status_err->wp_cnt % 100)) ||
			 (status_err->oor_cnt && !(status_err->oor_cnt % 100)))
			? "YES" : "NO") : "NoCard");

	return retval;
}

static void sd_sec_sdcard_noti_work(struct work_struct *work)
{
	struct sd_sec_vendor_info *cdi;
	struct sd_sec_status_err_info *status_err = &sdi.status_err;
	int ret;

	cdi = container_of(work, struct sd_sec_vendor_info, noti_work);
	if (!cdi->mmc->card)
		return;

	status_err->noti_cnt++;
	pr_info("%s: Send notification for SD card IO error. cnt(%d)\n",
			mmc_hostname(cdi->mmc), status_err->noti_cnt);

	ret = kobject_uevent(&sec_sdcard_cmd_dev->kobj, KOBJ_CHANGE);
	if (ret)
		pr_err("%s: Failed to send uevent with err %d\n", __func__, ret);
}

static struct device_type sdcard_type = {
	.uevent = sd_sec_sdcard_uevent,
};

static void sd_sec_inc_err_count(int index, int error, u32 status)
{
	struct sd_sec_err_info *err_log = &sdi.err_info[0];
	int i;
	int cpu = raw_smp_processor_id();

	if (!error)
		return;

	/*
	 * -EIO (-5) : SDMMC_INT_RESP_ERR error case. So log as CRC.
	 * -ENOMEDIUM (-123), etc : SW timeout and other error. So log as TIMEOUT.
	 */
	switch (error) {
	case -EIO:
		error = -EILSEQ;
		break;
	case -EILSEQ:
		break;
	default:
		error = -ETIMEDOUT;
		break;
	}

	for (i = 0; i < MAX_ERR_TYPE_INDEX; i++) {
		if (err_log[index + i].err_type == error) {
			index += i;
			break;
		}
	}
	if (i >= MAX_ERR_TYPE_INDEX)
		return;

	/* log device status and time if this is the first error  */
	if (!err_log[index].status || !(R1_CURRENT_STATE(status) & R1_STATE_TRAN))
		err_log[index].status = status;
	if (!err_log[index].first_issue_time)
		err_log[index].first_issue_time = cpu_clock(cpu);
	err_log[index].last_issue_time = cpu_clock(cpu);
	err_log[index].count++;
}


static void sd_sec_inc_status_err(struct mmc_card *card, u32 status)
{
	struct sd_sec_status_err_info *status_err = &sdi.status_err;
	bool noti = false;

	if (status & R1_ERROR) {
		status_err->ge_cnt++;
		if (!(status_err->ge_cnt % 1000))
			noti = true;
	}

	if (status & R1_CC_ERROR)
		status_err->cc_cnt++;

	if (status & R1_CARD_ECC_FAILED) {
		status_err->ecc_cnt++;
		if (!(status_err->ecc_cnt % 1000))
			noti = true;
	}

	if (status & R1_WP_VIOLATION) {
		status_err->wp_cnt++;
		if (!(status_err->wp_cnt % 100))
			noti = true;
	}

	if (status & R1_OUT_OF_RANGE) {
		status_err->oor_cnt++;
		if (!(status_err->oor_cnt % 100))
			noti = true;
	}

	/*
	 * Make notification for SD card errors
	 *
	 * Condition :
	 *	 GE, ECC : Every 1000 error
	 *	 WP, OOR : Every  100 error
	 */
	if (noti && mmc_card_sd(card) && sec_sdcard_cmd_dev)
		schedule_work(&sdi.noti_work);
}

#define MMC_BLK_TIMEOUT_MS (9 * 1000)
static bool sd_sec_check_busy_stuck(u32 status)
{
	if (time_before(jiffies,
		sdi.tstamp_last_cmd + msecs_to_jiffies(MMC_BLK_TIMEOUT_MS)))
		return false;

	if (status && (!(status & R1_READY_FOR_DATA) ||
			(R1_CURRENT_STATE(status) == R1_STATE_PRG)))
		return true;

	return false;
}

static void sd_sec_log_err_count(struct mmc_host *mmc, struct mmc_request *mrq)
{
	u32 status = (mrq->sbc ? mrq->sbc->resp[0] : 0) |
				(mrq->stop ? mrq->stop->resp[0] : 0) |
				(mrq->cmd ? mrq->cmd->resp[0] : 0);

	if (status & STATUS_MASK)
		sd_sec_inc_status_err(mmc->card, status);

	if (mrq->cmd->error)
		sd_sec_inc_err_count(SD_CMD_OFFSET, mrq->cmd->error, status);
	if (mrq->sbc && mrq->sbc->error)
		sd_sec_inc_err_count(SD_SBC_OFFSET, mrq->sbc->error, status);
	if (mrq->data && mrq->data->error)
		sd_sec_inc_err_count(SD_DATA_OFFSET, mrq->data->error, status);
	if (mrq->stop && mrq->stop->error)
		sd_sec_inc_err_count(SD_STOP_OFFSET, mrq->stop->error, status);

	/*
	 * Core driver polls card busy for 10s, MMC_BLK_TIMEOUT_MS.
	 * If card status is still in prog state after 9s by cmd13
	 * and tstamp_last_cmd has not been updated by next cmd,
	 * log as write busy timeout.
	 */
	if (mrq->cmd->opcode != MMC_SEND_STATUS)
		return;

	if (sd_sec_check_busy_stuck(status)) {
		/* card stuck in prg state */
		sd_sec_inc_err_count(SD_BUSY_OFFSET, -ETIMEDOUT, status);
		/* not to check card busy again */
		sdi.tstamp_last_cmd = jiffies;
	}
}

static bool sd_sec_check_cmd_type(struct mmc_request *mrq)
{
	/*
	 * cmd->flags info
	 * MMC_CMD_AC	 (0b00 << 5) : Addressed commands
	 * MMC_CMD_ADTC  (0b01 << 5) : Addressed data transfer commands
	 * MMC_CMD_BC	 (0b10 << 5) : Broadcast commands
	 * MMC_CMD_BCR	 (0b11 << 5) : Broadcast commands with response
	 *
	 * Log the errors only for AC or ADTC type
	 */

	if (!(mrq->cmd->flags & MMC_RSP_PRESENT))
		return false;

	if (mrq->cmd->flags & MMC_CMD_BC)
		return false;

	/*
	 * No need to check if MMC_RSP_136 set or cmd MMC_APP_CMD.
	 * CMD55 is sent with MMC_CMD_AC flag but no need to log.
	 */
	if ((mrq->cmd->flags & MMC_RSP_136) ||
			(mrq->cmd->opcode == MMC_APP_CMD))
		return false;

	return true;
}

void sd_sec_check_req_err(struct msdc_host *host, struct mmc_request *mrq)
{
	struct mmc_host *mmc = mmc_from_priv(host);

	if (!mmc->card || !mrq || !mrq->cmd)
		return;

	/* Return if the cmd is tuning block */
	if ((mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK) ||
			(mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200))
		return;

	/* Set CMD(except CMD13) timestamp to check card stuck */
	if (mrq->cmd->opcode != MMC_SEND_STATUS)
		sdi.tstamp_last_cmd = jiffies;

	if (sd_sec_check_cmd_type(mrq))
		sd_sec_log_err_count(mmc, mrq);
}

static void sd_sec_clear_err_count(void)
{
	struct sd_sec_err_info *err_log = &sdi.err_info[0];
	struct sd_sec_status_err_info *status_err = &sdi.status_err;
	int i;

	for (i = 0; i < MAX_LOG_INDEX; i++) {
		err_log[i].status = 0;
		err_log[i].first_issue_time = 0;
		err_log[i].last_issue_time = 0;
		err_log[i].count = 0;
	}

	memset(status_err, 0,
			sizeof(struct sd_sec_status_err_info));
}

static void sd_sec_init_err_count(void)
{
	static const char *const req_types[] = {
		"sbc  ", "cmd  ", "data ", "stop ", "busy "
	};
	struct sd_sec_err_info *err_log = &sdi.err_info[0];
	int i;

	/*
	 * err_log[0].type = "sbc  "
	 * err_log[0].err_type = -EILSEQ;
	 * err_log[1].type = "sbc  "
	 * err_log[1].err_type = -ETIMEDOUT;
	 * ...
	 */
	for (i = 0; i < MAX_LOG_INDEX; i++) {
		snprintf(err_log[i].type, sizeof(char) * 5, "%s",
				req_types[i / MAX_ERR_TYPE_INDEX]);

		err_log[i].err_type =
			(i % MAX_ERR_TYPE_INDEX == 0) ?	-EILSEQ : -ETIMEDOUT;
	}
}

/*
 * vendor hooks
 * check include/trace/hooks/mmc.h
 */
static void sec_android_vh_mmc_gpio_cd_irqt(void *data,
		struct mmc_host *host, bool *allow)
{
	bool status;

	if (sdi.card_detect_cnt < UINT_MAX)
		sdi.card_detect_cnt++;

	if (!host)
		return;

	status = mmc_gpio_get_cd(host) ? true : false;

	if (!status)
		sdi.failed_init = false;

	sd_sec_clear_err_count();
}

static void sec_android_vh_mmc_blk_reset(void *data,
		struct mmc_host *host, int err)
{
	if (err) {
		sdi.failed_init = true;
		pr_err("%s: mmc_blk_reset is failed with %d.\n",
				mmc_hostname(host), err);
	} else {
		sdi.failed_init = false;
		pr_info("%s: mmc_blk_reset is done with %d.\n",
				mmc_hostname(host), err);
	}
}

static void sec_android_vh_mmc_attach_sd(void *data,
		struct mmc_host *host, u32 ocr, int err)
{
	/* It can handle only err case */
	if (err) {
		sdi.failed_init = true;
		pr_info("%s: init failed: err(%d)\n", __func__, err);
	}
}

void sd_sec_register_vendor_hooks(void)
{
	register_trace_android_vh_mmc_gpio_cd_irqt(sec_android_vh_mmc_gpio_cd_irqt, NULL);
	register_trace_android_vh_mmc_blk_reset(sec_android_vh_mmc_blk_reset, NULL);
	register_trace_android_vh_mmc_attach_sd(sec_android_vh_mmc_attach_sd, NULL);
}

void sd_sec_set_features(struct platform_device *pdev)
{
	struct mmc_host *mmc;
	struct device_node *np = pdev->dev.of_node;

	mmc = platform_get_drvdata(pdev);

	if (of_property_read_u32(np, "sec-sd-slot-type", &sdi.slot_type)) {
		/* Set hotplug slot type if cd_gpio is defined as default */
		if (mmc_can_gpio_cd(mmc))
			sdi.slot_type = SEC_HOTPLUG_SD_SLOT;
		else
			sdi.slot_type = SEC_INVALID_SD_SLOT;
	}

	/* Initialize cd_cnt to 0. */
	sdi.card_detect_cnt = 0;

	sd_sec_init_err_count();

	sd_sec_register_vendor_hooks();
	sd_sec_init_sysfs(mmc);

	/* Register sd uevent . */
	sdi.mmc = mmc;
	sec_sdcard_cmd_dev->type = &sdcard_type;
	INIT_WORK(&sdi.noti_work, sd_sec_sdcard_noti_work);
}

MODULE_LICENSE("GPL v2");
