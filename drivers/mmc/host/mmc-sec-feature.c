// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/device.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/card.h>
#include <linux/sec_class.h>
#include <trace/hooks/mmc_core.h>

#include "dw_mmc-exynos.h"
#include "mmc-sec-feature.h"
#include "mmc-sec-sysfs.h"

struct mmc_sd_sec_device_info mdi;
struct mmc_sd_sec_device_info sdi;
struct device *sec_sdcard_cmd_dev;

bool check_reg_vh;

static int sd_sec_sdcard_uevent(struct device *dev,
				struct kobj_uevent_env *env)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	int retval;
	bool card_exist = false;
	struct mmc_sd_sec_status_err_info *status_err = &sdi.status_err;

	add_uevent_var(env, "DEVNAME=%s", dev->kobj.name);

	if (host->card)
		card_exist = true;

	retval = add_uevent_var(env, "IOERROR=%s", card_exist ? (
			((status_err->ge_cnt && !(status_err->ge_cnt % 1000)) ||
			 (status_err->ecc_cnt && !(status_err->ecc_cnt % 1000)) ||
			 (status_err->wp_cnt && !(status_err->wp_cnt % 100)) ||
			 (status_err->oor_cnt && !(status_err->oor_cnt % 100)))
			? "YES" : "NO")	: "NoCard");

	return retval;
}

static void sd_sec_sdcard_noti_work(struct work_struct *work)
{
	struct mmc_sd_sec_device_info *cdi;
	struct mmc_sd_sec_status_err_info *status_err = &sdi.status_err;
	int ret;

	cdi = container_of(work, struct mmc_sd_sec_device_info, noti_work);
	if (!cdi->mmc->card)
		return;

	pr_info("%s: Send notification for SD card IO error\n", mmc_hostname(cdi->mmc));

	status_err->noti_cnt++;

	ret = kobject_uevent(&sec_sdcard_cmd_dev->kobj, KOBJ_CHANGE);
	if (ret)
		pr_err("%s: Failed to send uevent with err %d\n", __func__, ret);
}

static struct device_type sdcard_type = {
	.uevent = sd_sec_sdcard_uevent,
};

static void mmc_sd_sec_inc_err_count(struct mmc_sd_sec_err_info *err_log,
		int index, int error, u32 status)
{
	int i = 0;
	int cpu = raw_smp_processor_id();

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

static void mmc_sd_sec_inc_status_err(struct mmc_sd_sec_status_err_info *status_err,
		struct mmc_card *card, u32 status)
{
	bool noti = false;

	if (status & HALT_UNHALT_ERR)
		status_err->halt_cnt++;
	if (status & CQ_EN_DIS_ERR)
		status_err->cq_cnt++;
	if (status & RPMB_SWITCH_ERR)
		status_err->rpmb_cnt++;
	if (status & HW_RST) {
		status_err->hw_rst_cnt++;
#if defined(CONFIG_SEC_ABC)
		if ((status_err->hw_rst_cnt % 10) == 0)
			sec_abc_send_event("MODULE=storage@WARN=mmc_hwreset_err");
#endif
	}

	if (!card)
		return;

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

static void mmc_sd_sec_log_err_count(struct mmc_sd_sec_device_info *cdi,
		struct mmc_host *mmc, struct mmc_request *mrq)
{
	u32 status = (mrq->sbc ? mrq->sbc->resp[0] : 0) |
				(mrq->stop ? mrq->stop->resp[0] : 0) |
				(mrq->cmd ? mrq->cmd->resp[0] : 0);

	if (status & STATUS_MASK)
		mmc_sd_sec_inc_status_err(&cdi->status_err, mmc->card, status);

	if (mrq->cmd->error)
		mmc_sd_sec_inc_err_count(&cdi->err_info[0],
			SD_CMD_OFFSET, mrq->cmd->error, status);
	if (mrq->sbc && mrq->sbc->error)
		mmc_sd_sec_inc_err_count(&cdi->err_info[0],
			SD_SBC_OFFSET, mrq->sbc->error, status);
	if (mrq->data && mrq->data->error)
		mmc_sd_sec_inc_err_count(&cdi->err_info[0],
			SD_DATA_OFFSET, mrq->data->error, status);
	if (mrq->stop && mrq->stop->error)
		mmc_sd_sec_inc_err_count(&cdi->err_info[0],
			SD_STOP_OFFSET, mrq->stop->error, status);

	/*
	 * Core driver polls card busy for 20s, MMC_BLK_TIMEOUT_MS.
	 * If card status is still in prog state after 18s by cmd13
	 * and tstamp_last_cmd has not been updated by next cmd,
	 * log as write busy timeout.
	 */
	if (mrq->cmd->opcode == MMC_SEND_STATUS &&
		time_after(jiffies, cdi->tstamp_last_cmd +
								msecs_to_jiffies(18 * 1000))) {
		if (status && (!(status & R1_READY_FOR_DATA) ||
				(R1_CURRENT_STATE(status) == R1_STATE_PRG))) {
			/* card stuck in prg state */
			mmc_sd_sec_inc_err_count(&cdi->err_info[0], SD_BUSY_OFFSET, -ETIMEDOUT, status);
			/* not to check card busy again */
			cdi->tstamp_last_cmd = jiffies;
		}
	}
}

void mmc_sec_log_err_count(u32 status)
{
	if (status & MMC_ERR_MASK)
		mmc_sd_sec_inc_status_err(&mdi.status_err, NULL, status);
}

static bool mmc_sd_sec_check_cmd_type(struct mmc_request *mrq)
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
	if ((mrq->cmd->flags & MMC_RSP_PRESENT) &&
			!(mrq->cmd->flags & MMC_CMD_BC)) {
		/*
		 * No need to check if MMC_RSP_136 set or cmd MMC_APP_CMD.
		 * CMD55 is sent with MMC_CMD_AC flag but no need to log.
		 */
		if ((mrq->cmd->flags & MMC_RSP_136) ||
				(mrq->cmd->opcode == MMC_APP_CMD))
			return false;

		return true;
	} else
		return false;
}

void mmc_sd_sec_check_req_err(struct mmc_host *host, struct mmc_request *mrq)
{
	struct mmc_sd_sec_device_info *cdi;

	if (!host->card || !mrq || !mrq->cmd)
		return;

	cdi = get_device_info(host);

	/* Return if the cmd is tuning block */
	if ((mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK) ||
			(mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200))
		return;

	/* Set CMD(except CMD13) timestamp to check card stuck */
	if (mrq->cmd->opcode != MMC_SEND_STATUS)
		cdi->tstamp_last_cmd = jiffies;

	if (mmc_sd_sec_check_cmd_type(mrq))
		mmc_sd_sec_log_err_count(cdi, host, mrq);
}

static void mmc_sd_sec_clear_err_count(void)
{
	struct mmc_sd_sec_err_info *err_log = &sdi.err_info[0];
	struct mmc_sd_sec_status_err_info *status_err = &sdi.status_err;

	int i = 0;

	for (i = 0; i < MAX_LOG_INDEX; i++) {
		err_log[i].status = 0;
		err_log[i].first_issue_time = 0;
		err_log[i].last_issue_time = 0;
		err_log[i].count = 0;
	}

	memset(status_err, 0,
			sizeof(struct mmc_sd_sec_status_err_info));
}

static void mmc_sd_sec_init_err_count(struct mmc_sd_sec_err_info *err_log)
{
	static const char *const req_types[] = {
		"sbc  ", "cmd  ", "data ", "stop ", "busy "
	};
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

void sd_sec_check_cd_gpio(int det_status)
{
	sdi.det_gpio = det_status;
}

void sd_sec_detect_interrupt(struct mmc_host *host)
{
	if (sdi.card_detect_cnt < UINT_MAX)
		sdi.card_detect_cnt++;

	host->trigger_card_event = true;
	mmc_sd_sec_clear_err_count();
}

static void mmc_sd_sec_register_vendor_hooks(void)
{
	if (check_reg_vh)
		return;

	check_reg_vh = true;
}

void sd_sec_set_features(struct mmc_host *host, struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	if (of_get_property(np, "sec-sd-slot-type", NULL))
		of_property_read_u32(np,
				"sec-sd-slot-type", &sdi.sd_slot_type);
	else {
		/* Set hotplug slot type if cd_gpio is defined as default */
		if (sdi.det_gpio < 0) {
			sdi.sd_slot_type = SEC_INVALID_SD_SLOT;
			pr_err("%s set a invalid sd slot\n", __func__);
		} else
			sdi.sd_slot_type = SEC_HOTPLUG_SD_SLOT;
	}

	/* Initialize cd_cnt to 0. */
	sdi.card_detect_cnt = 0;

	/* disable mmc core's runtime PM */
	host->caps &= ~MMC_CAP_AGGRESSIVE_PM;

	mmc_sd_sec_init_err_count(&sdi.err_info[0]);
	sd_sec_init_sysfs(host);

	/* Register sd uevent . */
	sdi.mmc = host;
	sec_sdcard_cmd_dev->type = &sdcard_type;
	INIT_WORK(&sdi.noti_work, sd_sec_sdcard_noti_work);

	mmc_sd_sec_register_vendor_hooks();
}

void mmc_sec_set_features(struct mmc_host *host)
{
	mmc_sd_sec_init_err_count(&mdi.err_info[0]);
	mmc_sec_init_sysfs(host);

	mmc_sd_sec_register_vendor_hooks();
}
