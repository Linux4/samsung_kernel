// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#include <linux/mmc/slot-gpio.h>
#include <linux/of.h>
#include <linux/mmc/mmc.h>
#include <trace/hooks/mmc_core.h>

#include "mmc-sec-feature.h"
#include "mmc-sec-sysfs.h"

struct mmc_sec_op_info sd_op_info;

/*
 * These are for SDcard error counting.
 *
 * sd_err_info
 *   : accumulated err sum while SDcard is presented.
 * sd_status_err_info
 *   : accumulated status err sum while SDcard is presented.
 *
 * sd_err_info_backup
 *   : backup for sd_err_info after HQM reads sd_err_info.
 * sd_status_err_info_backup
 *   : backup for sd_status_err_info after HQM reads sd_status_err_info.
 */
struct mmc_sec_err_info sd_err_info[MAX_MMC_HOST_IDX][MAX_ERR_LOG_INDEX];
struct mmc_sec_status_err_info sd_status_err_info[MAX_MMC_HOST_IDX];
struct mmc_sec_err_info sd_err_info_backup[MAX_MMC_HOST_IDX][MAX_ERR_LOG_INDEX];
struct mmc_sec_status_err_info sd_status_err_info_backup[MAX_MMC_HOST_IDX];

/* SYSFS about SD Card Detection */
struct device *sd_card_dev;
/* SYSFS about SD Card Information */
struct device *sd_info_dev;
/* SYSFS about SD Card error Information */
struct device *sd_data_dev;

static int mmc_sec_sdcard_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	int host_idx = host->index;
	struct mmc_sec_status_err_info *err_log = &sd_status_err_info[host_idx];
	int retval = 0;
	bool card_exist = false;

	add_uevent_var(env, "DEVNAME=%s", dev->kobj.name);

	if (host->card)
		card_exist = true;

	retval = add_uevent_var(env, "IOERROR=%s", card_exist ? (
			((err_log->ge_cnt && !(err_log->ge_cnt % 1000)) ||
			 (err_log->ecc_cnt && !(err_log->ecc_cnt % 1000)) ||
			 (err_log->wp_cnt && !(err_log->wp_cnt % 100)) ||
			 (err_log->oor_cnt && !(err_log->oor_cnt % 100)))
			? "YES" : "NO")	: "NoCard");

	return retval;
}

static void mmc_sec_sdcard_noti_work(struct work_struct *work)
{
	struct mmc_sec_op_info *op_info = container_of(work, struct mmc_sec_op_info, noti_work);
	int host_idx = op_info->host->index;
	int ret = 0;

	if (!op_info->host->card)
		return;

	pr_info("%s: Send Notification about SD Card IO Error\n", mmc_hostname(op_info->host));

	sd_status_err_info[host_idx].noti_cnt++;

	ret = kobject_uevent(&sd_card_dev->kobj, KOBJ_CHANGE);
	if (ret)
		pr_err("%s: Failed to Send Uevent with err %d\n", __func__, ret);
}

static struct device_type sdcard_type = {
	.uevent = mmc_sec_sdcard_uevent,
};

#if IS_ENABLED(CONFIG_MMC_SDHCI_MSM)
static void mmc_sec_error_count_log(int host_idx, int index, int error, u32 status)
{
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
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

	// 1st status logging or not trans
	if (!err_log[index].status || !(R1_CURRENT_STATE(status) & R1_STATE_TRAN))
		err_log[index].status = status;
	if (!err_log[index].first_issue_time)
		err_log[index].first_issue_time = cpu_clock(cpu);
	err_log[index].last_issue_time = cpu_clock(cpu);
	err_log[index].count++;
}

static void mmc_sec_check_status_err(struct mmc_card *card, int host_idx, u32 status)
{
	bool noti = false;

	if (status & R1_ERROR) {
		sd_status_err_info[host_idx].ge_cnt++;
		if (!(sd_status_err_info[host_idx].ge_cnt % 1000))
			noti = true;
	}
	if (status & R1_CC_ERROR)
		sd_status_err_info[host_idx].cc_cnt++;
	if (status & R1_CARD_ECC_FAILED) {
		sd_status_err_info[host_idx].ecc_cnt++;
		if (!(sd_status_err_info[host_idx].ecc_cnt % 1000))
			noti = true;
	}
	if (status & R1_WP_VIOLATION) {
		sd_status_err_info[host_idx].wp_cnt++;
		if (!(sd_status_err_info[host_idx].wp_cnt % 100))
			noti = true;
	}
	if (status & R1_OUT_OF_RANGE) {
		sd_status_err_info[host_idx].oor_cnt++;
		if (!(sd_status_err_info[host_idx].oor_cnt % 100))
			noti = true;
	}

	/*
	 * Make Notification about SD Card Errors
	 *
	 * Condition :
	 *   GE, ECC : Every 1000 errors
	 *   WP, OOR : Every  100 errors
	 */
	if (noti && mmc_card_sd(card) && sd_card_dev)
		schedule_work(&sd_op_info.noti_work);
}

static void mmc_sec_card_error_logging(struct mmc_host *mmc, struct mmc_request *mrq)
{
	int host_idx = mmc->index;
	u32 status = 0;

	status = (mrq->sbc ? mrq->sbc->resp[0] : 0) |
		(mrq->stop ? mrq->stop->resp[0] : 0) |
		(mrq->cmd ? mrq->cmd->resp[0] : 0);

	if (status & STATUS_MASK)
		mmc_sec_check_status_err(mmc->card, host_idx, status);

	if (mrq->sbc && mrq->sbc->error)
		mmc_sec_error_count_log(host_idx, MMC_LOG_SBC_OFFSET, mrq->sbc->error, status);
	if (mrq->cmd && mrq->cmd->error)
		mmc_sec_error_count_log(host_idx, MMC_LOG_CMD_OFFSET, mrq->cmd->error, status);
	if (mrq->data && mrq->data->error)
		mmc_sec_error_count_log(host_idx, MMC_LOG_DATA_OFFSET, mrq->data->error, status);
	if (mrq->stop && mrq->stop->error)
		mmc_sec_error_count_log(host_idx, MMC_LOG_STOP_OFFSET, mrq->stop->error, status);

	/*
	 * in block.c
	 *    #define MMC_BLK_TIMEOUT_MS  (20 * 1000)
	 *    refer to card_busy_detect()
	 * so, check CMD13's response(status)
	 *    if there is no other CMD for 18secs or more.
	 */
	if (mrq->cmd->opcode == MMC_SEND_STATUS &&
		time_after(jiffies, sd_op_info.tstamp_last_cmd + msecs_to_jiffies(18 * 1000))) {
		if (status && (!(status & R1_READY_FOR_DATA) ||
				(R1_CURRENT_STATE(status) == R1_STATE_PRG))) {
			// card stuck in prg state
			mmc_sec_error_count_log(host_idx, MMC_LOG_BUSY_OFFSET, -ETIMEDOUT, status);
			// not to check card busy again
			sd_op_info.tstamp_last_cmd = jiffies;
		}
	}
}

void mmc_sec_check_request_error(struct sdhci_host *host,
		struct mmc_request *mrq)
{
	struct mmc_host *mmc = host->mmc;

	if (!mmc->card || !mrq || !mrq->cmd)
		return;

	/* return if the cmd is tuning block */
	if (((mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK) ||
			(mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200)))
		return;

	/* set CMD(except CMD13) timestamp to check card stuck */
	if (mrq->cmd->opcode != MMC_SEND_STATUS)
		sd_op_info.tstamp_last_cmd = jiffies;

	/*
	 * cmd->flags info
	 * #define MMC_CMD_MASK    (3 << 5) : non-SPI command type
	 * #define MMC_CMD_AC      (0 << 5) : Addressed (point-to-point) commands
	 * #define MMC_CMD_ADTC    (1 << 5) : Addressed (point-to-point) data transfer commands
	 * #define MMC_CMD_BC      (2 << 5) : Broadcast commands
	 * #define MMC_CMD_BCR     (3 << 5) : Broadcast commands with response
	 *
	 * We should log the errors in case of AC or ADTC type
	 */
	if ((mrq->cmd->flags & MMC_RSP_PRESENT) &&
			!(mrq->cmd->flags & MMC_CMD_BC)) {
		/* no need to check cmd flag has MMC_RSP_136 set */
		/*
		 * return immediately if the cmd->opcode is MMC_APP_CMD
		 *    in SDcard case, CMD55 is sent with MMC_CMD_AC flag
		 *    in eMMC case, CMD55 is not used
		 */
		if ((mrq->cmd->flags & MMC_RSP_136) ||
				(mrq->cmd->opcode == MMC_APP_CMD))
			return;

		mmc_sec_card_error_logging(mmc, mrq);
	}
}
#endif

static void mmc_sec_clear_err_count(struct mmc_host *host)
{
	int host_idx = host->index;
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
	int i = 0;

	for (i = 0; i < MAX_ERR_LOG_INDEX; i++) {
		err_log[i].status = 0;
		err_log[i].first_issue_time = 0;
		err_log[i].last_issue_time = 0;
		err_log[i].count = 0;
	}

	memset(sd_err_info_backup[host_idx], 0,
			sizeof(struct mmc_sec_err_info) * MAX_ERR_LOG_INDEX);
	memset(&sd_status_err_info[host_idx], 0, sizeof(struct mmc_sec_status_err_info));
	memset(&sd_status_err_info_backup[host_idx], 0, sizeof(struct mmc_sec_status_err_info));
}

static void mmc_sec_init_err_count(struct mmc_host *host)
{
	int host_idx = host->index;
	struct mmc_sec_err_info *err_log = sd_err_info[host_idx];
	static const char *const req_types[] = {
		"sbc  ", "cmd  ", "data ", "stop ", "busy "
	};
	int i = 0;

	/*
	 * Init err_log[]
	 * //sbc
	 * err_log[0].err_type = -EILSEQ;
	 * err_log[1].err_type = -ETIMEDOUT;
	 * ...
	 */
	for (i = 0; i < MAX_ERR_LOG_INDEX; i++) {
		snprintf(err_log[i].type, sizeof(char) * 5, "%s",
				req_types[i / MAX_ERR_TYPE_INDEX]);

		err_log[i].err_type =
			(i % MAX_ERR_TYPE_INDEX == 0) ?	-EILSEQ : -ETIMEDOUT;
	}
}

/*
 * vendor hooks
 * check include/trace/hooks/mmc_core.h
 */
static void sec_android_vh_mmc_blk_reset(void *data, struct mmc_host *host, int err, bool *allow)
{
	if (err)
		pr_err("%s : mmc_blk_reset is failed with %d.\n", mmc_hostname(host), err);
	else
		pr_info("%s : mmc_blk_reset is done with %d.\n", mmc_hostname(host), err);
}

static void sec_android_vh_mmc_blk_mq_rw_recovery(void *data, struct mmc_card *card)
{
}

static void sec_android_vh_sd_update_bus_speed_mode(void *data, struct mmc_card *card)
{
	/* save speed mode value when updating bus speed */
	sd_op_info.sd_bus_speed = card->sd_bus_speed;
}

static void sec_android_vh_mmc_attach_sd(void *data, struct mmc_host *host, u32 ocr, int err)
{
	/* It can handle only err case */
	if (err) {
		/* ST_LOG */
		return;
	}
}

static void sec_android_vh_sdhci_get_cd(void *data, struct sdhci_host *host, bool *allow)
{
}

static void sec_android_vh_mmc_gpio_cd_irqt(void *data, struct mmc_host *host, bool *allow)
{
	bool status;

	if (!host) {
		*allow = false;
		return;
	}

	status = mmc_gpio_get_cd(host) ? true : false;
	pr_debug("%s: cd gpio irq, gpio state %d (CARD_%s)\n",
			mmc_hostname(host), status,
			status ? "INSERT" : "REMOVAL");

	if (status ^ sd_op_info.tray_status) {
		pr_err("%s: slot status change detected (%d -> %d), GPIO_ACTIVE_%s\n",
				mmc_hostname(host), sd_op_info.tray_status, status,
				(host->caps2 & MMC_CAP2_CD_ACTIVE_HIGH) ?
				"HIGH" : "LOW");

		sd_op_info.tray_status = status;
		if (sd_op_info.card_detect_cnt < UINT_MAX)
			sd_op_info.card_detect_cnt++;

		host->trigger_card_event = true;

		if (!status)
			sd_op_info.sd_bus_speed = 0;

		mmc_sec_clear_err_count(host);
	} else
		*allow = false;
}

static void mmc_sec_register_vendor_hooks(void)
{
	register_trace_android_vh_mmc_blk_reset(sec_android_vh_mmc_blk_reset, NULL);
	register_trace_android_vh_mmc_blk_mq_rw_recovery(sec_android_vh_mmc_blk_mq_rw_recovery, NULL);
	register_trace_android_vh_sd_update_bus_speed_mode(sec_android_vh_sd_update_bus_speed_mode, NULL);
	register_trace_android_vh_mmc_attach_sd(sec_android_vh_mmc_attach_sd, NULL);
	register_trace_android_vh_sdhci_get_cd(sec_android_vh_sdhci_get_cd, NULL);
	register_trace_android_vh_mmc_gpio_cd_irqt(sec_android_vh_mmc_gpio_cd_irqt, NULL);
}

void mmc_sec_save_tuning_phase(u8 phase)
{
	sd_op_info.saved_tuning_phase = phase;
}

void mmc_set_sec_features(struct mmc_host *host, struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	pr_err("%s calling.\n", __func__);

	/* disable mmc core's runtime PM */
	host->caps &= ~MMC_CAP_AGGRESSIVE_PM;
	/* enable no prescan powerup */
	host->caps2 |= MMC_CAP2_NO_PRESCAN_POWERUP;

	/* set trigger_card_event to true */
	host->trigger_card_event = true;

	sd_op_info.tray_status = mmc_gpio_get_cd(host) ? true : false;
	if (!of_property_read_u32(np, "sec-sd-slot-type", &sd_op_info.sd_slot_type)) {
		if (mmc_gpio_get_cd(host) < 0) {
			if (sd_op_info.sd_slot_type != SEC_NO_DET_SD_SLOT) {
				pr_warn("mmc-sec : check sd_slot_type and cd_gpio settings for no det.\n");
				sd_op_info.sd_slot_type = SEC_NO_DET_SD_SLOT;
			}
		} else {
			if (sd_op_info.sd_slot_type == SEC_NO_DET_SD_SLOT) {
				pr_warn("mmc-sec : check sd_slot_type setting with cd_gpio.\n");
				sd_op_info.sd_slot_type = SEC_HYBRID_SD_SLOT;
			}
		}
	} else { /* default */
		sd_op_info.sd_slot_type = SEC_HYBRID_SD_SLOT;
	}

	mmc_sec_init_err_count(host);

	mmc_sec_init_sysfs(host);

	mmc_sec_register_vendor_hooks();

	sd_op_info.host = host;

	/* SDcard uevent register. */
	if (sd_card_dev) {
		sd_card_dev->type = &sdcard_type;
		INIT_WORK(&sd_op_info.noti_work, mmc_sec_sdcard_noti_work);
	}
}
