// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#include <linux/mmc/slot-gpio.h>
#include <linux/of.h>
#include <linux/mmc/mmc.h>
#include <trace/hooks/mmc.h>

#include "mmc-sec-feature.h"
#include "mmc-sec-sysfs.h"

struct mmc_sd_sec_device_info sdi;

static void mmc_sd_sec_inc_err_count(struct mmc_sd_sec_err_info *err_log,
		int index, int error, u32 status)
{
	int i = 0;
	int cpu = raw_smp_processor_id();

	if (!error)
		return;

	/*
	 * need to store errors except both -EILSEQ and -ETIMEDOUT
	 */
	if (error != -EILSEQ)
		error = -ETIMEDOUT;

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
	if (!card)
		return;

	if (status & R1_ERROR)
		status_err->ge_cnt++;
	if (status & R1_CC_ERROR)
		status_err->cc_cnt++;
	if (status & R1_CARD_ECC_FAILED)
		status_err->ecc_cnt++;
	if (status & R1_WP_VIOLATION)
		status_err->wp_cnt++;
	if (status & R1_OUT_OF_RANGE)
		status_err->oor_cnt++;
}

#define MMC_BLK_TIMEOUT_MS (9 * 1000)
static bool mmc_sd_sec_check_busy_stuck(u32 status)
{
	if (time_before(jiffies,
		sdi.tstamp_last_cmd + msecs_to_jiffies(MMC_BLK_TIMEOUT_MS)))
		return false;

	if (status && (!(status & R1_READY_FOR_DATA) ||
			(R1_CURRENT_STATE(status) == R1_STATE_PRG)))
		return true;

	return false;
}

static void mmc_sd_sec_log_err_count(struct mmc_host *mmc,
		struct mmc_request *mrq)
{
	u32 status = (mrq->sbc ? mrq->sbc->resp[0] : 0) |
		(mrq->stop ? mrq->stop->resp[0] : 0) |
		(mrq->cmd ? mrq->cmd->resp[0] : 0);

	if (status & STATUS_MASK)
		mmc_sd_sec_inc_status_err(&sdi.status_err, mmc->card, status);

	if (mrq->cmd->error)
		mmc_sd_sec_inc_err_count(&sdi.err_info[0],
			SD_CMD_OFFSET, mrq->cmd->error, status);
	if (mrq->sbc && mrq->sbc->error)
		mmc_sd_sec_inc_err_count(&sdi.err_info[0],
			SD_SBC_OFFSET, mrq->sbc->error, status);
	if (mrq->data && mrq->data->error)
		mmc_sd_sec_inc_err_count(&sdi.err_info[0],
			SD_DATA_OFFSET, mrq->data->error, status);
	if (mrq->stop && mrq->stop->error)
		mmc_sd_sec_inc_err_count(&sdi.err_info[0],
			SD_STOP_OFFSET, mrq->stop->error, status);

	/*
	 * in block.c
	 *    #define MMC_BLK_TIMEOUT_MS  (10 * 1000)
	 *    refer to card_busy_detect()
	 * so, check CMD13's response(status)
	 *    if there is no other CMD for 9 secs or more.
	 */
	if (mrq->cmd->opcode != MMC_SEND_STATUS)
		return;

	if (mmc_sd_sec_check_busy_stuck(status)) {
		/* card stuck in prg state */
		mmc_sd_sec_inc_err_count(&sdi.err_info[0],
				SD_BUSY_OFFSET, -ETIMEDOUT, status);
		/* not to check card busy again */
		sdi.tstamp_last_cmd = jiffies;
	}
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

void mmc_sd_sec_check_req_err(struct mmc_host *host, struct mmc_request *mrq)
{
	if (!host->card || !mrq || !mrq->cmd)
		return;

	/* return if the cmd is tuning block */
	if ((mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK) ||
			(mrq->cmd->opcode == MMC_SEND_TUNING_BLOCK_HS200))
		return;

	/* set CMD(except CMD13) timestamp to check card stuck */
	if (mrq->cmd->opcode != MMC_SEND_STATUS)
		sdi.tstamp_last_cmd = jiffies;

	if (mmc_sd_sec_check_cmd_type(mrq))
		mmc_sd_sec_log_err_count(host, mrq);
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

	memset(status_err, 0, sizeof(struct mmc_sd_sec_status_err_info));
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

/*
 * vendor hooks
 * check include/trace/hooks/mmc.h
 */
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

static void sec_android_vh_mmc_blk_mq_rw_recovery(void *data,
		struct mmc_card *card)
{
}

static void sec_android_vh_mmc_attach_sd(void *data,
		struct mmc_host *host, u32 ocr, int err)
{
	/* It can handle only err case */
	if (err) {
		/* ST_LOG */
		sdi.failed_init = true;
		pr_info("%s: init failed: err(%d)\n", __func__, err);
	}
}

static void sec_android_vh_sdhci_get_cd(void *data,
		struct sdhci_host *host, bool *allow)
{
}

static void sec_android_vh_mmc_gpio_cd_irqt(void *data,
		struct mmc_host *host, bool *allow)
{
	bool status;

	if (!host)
		return;

	status = mmc_gpio_get_cd(host) ? true : false;
	pr_debug("%s: cd gpio irq, gpio state %d (CARD_%s)\n",
			mmc_hostname(host), status,
			status ? "INSERT" : "REMOVAL");

	if (!status)
		sdi.failed_init = false;

	mmc_sd_sec_clear_err_count();
}

static void mmc_sd_sec_register_vendor_hooks(void)
{
	register_trace_android_vh_mmc_blk_reset(sec_android_vh_mmc_blk_reset, NULL);
	register_trace_android_vh_mmc_blk_mq_rw_recovery(sec_android_vh_mmc_blk_mq_rw_recovery, NULL);
	register_trace_android_vh_mmc_attach_sd(sec_android_vh_mmc_attach_sd, NULL);
	register_trace_android_vh_sdhci_get_cd(sec_android_vh_sdhci_get_cd, NULL);
	register_trace_android_vh_mmc_gpio_cd_irqt(sec_android_vh_mmc_gpio_cd_irqt, NULL);
}

void sd_sec_set_features(struct mmc_host *host, struct platform_device *pdev)
{
	pr_err("%s calling.\n", __func__);

	sdi.sd_slot_type = SEC_HYBRID_SD_SLOT;

	sd_sec_init_sysfs(host);
	mmc_sd_sec_init_err_count(&sdi.err_info[0]);
	mmc_sd_sec_register_vendor_hooks();

	sdi.mmc = host;
}
