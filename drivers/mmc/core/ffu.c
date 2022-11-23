/*
 *create in 2021/1/20
 */
#include <linux/mmc/ffu.h>
#include "mmc_ops.h"
#include "core.h"

#define CID_MANFID_SANDISK	0x2
#define CID_MANFID_TOSHIBA	0x11
#define CID_MANFID_MICRON	0x13
#define CID_MANFID_SAMSUNG	0x15
#define CID_MANFID_SANDISK_NEW	0x45
#define CID_MANFID_KSI		0x70
#define CID_MANFID_HYNIX	0x90
#define OPERATION_CODES_TIMEOUT_MAX 0x17
/*
 * Turn the cache ON/OFF.
 * Turning the cache OFF shall trigger flushing of the data
 * to the non-volatile storage.
 * This function should be called with host claimed
 */
int mmc_ffu_cache_ctrl(struct mmc_host *host, u8 enable)
{
	struct mmc_card *card = host->card;
	unsigned int timeout;
	int err = 0;

	if (card && mmc_card_mmc(card) &&
			(card->ext_csd.cache_size > 0)) {
		enable = !!enable;

		if (card->ext_csd.cache_ctrl ^ enable) {
			timeout = enable ? card->ext_csd.generic_cmd6_time : 0;
			err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_CACHE_CTRL, enable, timeout);
			if (err)
				pr_notice("%s: cache %s error %d\n",
						mmc_hostname(card->host),
						enable ? "on" : "off",
						err);
			else
				card->ext_csd.cache_ctrl = enable;
		}
	}

	return err;
}
static int mmc_ffu_restart(struct mmc_card *card)
{
	struct mmc_host *host = card->host;
	int err = 0;
	card->host->ios.timing = MMC_TIMING_LEGACY;
	mmc_set_clock(card->host, 400000);
	mmc_set_bus_width(card->host, MMC_BUS_WIDTH_1);

	card->state |= MMC_QUIRK_FFUED;

	err = mmc_init_card(host, host->card->ocr, host->card);
	if (!err)
		card->state &= ~MMC_QUIRK_FFUED;

	return err;
}
static int mmc_ffu_install(struct mmc_card *card, u8 *ext_csd)
{
	u32 timeout;
	u8 set = 1;
	u8 retry = 10;
	int err;
	u8 *ext_csd_new = NULL;
	if ((card->cid.manfid == CID_MANFID_HYNIX) &&
				(card->cid.prv == 0x03)) {
		set = 0;
	}
	/*MODE_OPERATION_CODES not supported,Host sets MODE_CONFIG to
	 *NORMAL and performs CMD0/HW Reset/Power cycle to install the new firmware
	 */
	if (!(ext_csd[EXT_CSD_FFU_FEATURES] & MMC_FFU_FEATURES)) {
		err = mmc_switch(card, set,
				EXT_CSD_MODE_CONFIG,
				MMC_FFU_MODE_NORMAL, card->ext_csd.generic_cmd6_time);
		if (err) {
			pr_err("%sFFU:error %d:exit FFU mode\n",
				mmc_hostname(card->host), err);
			mmc_ffu_restart(card);
			goto exit;
		}
	}

	/*MODE_OPERATION_CODES
	 *supported
	 */
	if (ext_csd[EXT_CSD_FFU_FEATURES] & MMC_FFU_FEATURES) {
		timeout = ext_csd[EXT_CSD_OPERATION_CODE_TIMEOUT];
		if (timeout == 0 || timeout > OPERATION_CODES_TIMEOUT_MAX) {
			timeout = OPERATION_CODES_TIMEOUT_MAX;
			pr_err("%sFFU:use default OPERATION_CODES_TIMEOUT value",
					mmc_hostname(card->host));
		}
		timeout = (100*(1<<timeout))/1000 + 1;   //unit:ms
		/* Host sets MODE_OPERATION_CODES to FFU_INSTALL
		 * which automatically sets MODE_CONFIG to NORMAL after install
		 */
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_MODE_OPERATION_CODES,
					MMC_FFU_INSTALL_SET, timeout);
		if (err) {
			pr_err("%sFFU:Host sets MODE_OPERATION_CODES to FFU_INSTALL failed",
					mmc_hostname(card->host));
			mmc_ffu_restart(card);
			goto exit;
		}
	}
	err = mmc_ffu_restart(card);
	if (err) {
			pr_err("%sFFU: error %d:mmc ffu restart failed\n",
				mmc_hostname(card->host), err);
			goto exit;
		}

	while (retry--) {
		err = mmc_get_ext_csd(card, &ext_csd_new);
		if (err)
			pr_err("%sFFU:read ext_csd_new failed %d times",
					mmc_hostname(card->host), retry);
		else
			break;
	}
	if (err) {
		pr_err("%sFFU:read ext_csd_new failed 10 times",
				mmc_hostname(card->host));
		goto exit;
	}
	if (!ext_csd_new[EXT_CSD_FFU_STATUS]) {
		/*INSTALL SUCCESSD
		 *check FIRMWARE_VERSION[261-254]
		 */
		pr_err("FFU:install success!!,%x", ext_csd_new[EXT_CSD_FFU_STATUS]);
	} else {
		pr_err("%sFFU:install failed!!,%x",
				mmc_hostname(card->host), ext_csd_new[EXT_CSD_FFU_STATUS]);
		err = -EINVAL;
	}
exit:
	kfree(ext_csd_new);
	return err;
}


static int mmc_ffu_check_result(struct mmc_request *mrq)
{
	if (!mrq || !mrq->cmd || !mrq->data)
		return -EINVAL;

	if (mrq->cmd->error != 0)
		return -EINVAL;

	if (mrq->data->error != 0)
		return -EINVAL;

	if (mrq->stop != NULL && mrq->stop->error != 0)
		return -1;

	if (mrq->data->bytes_xfered != (mrq->data->blocks * mrq->data->blksz))
		return -EINVAL;

	return 0;
}

 static int mmc_ffu_busy(struct mmc_command *cmd)
{
	return !(cmd->resp[0] & R1_READY_FOR_DATA) ||
		(R1_CURRENT_STATE(cmd->resp[0]) == R1_STATE_PRG);
}

int mmc_ffu_wait_busy(struct mmc_card *card)
{
	int ret, busy = 0;
	struct mmc_command cmd = {0};

	memset(&cmd, 0, sizeof(struct mmc_command));
	cmd.opcode = MMC_SEND_STATUS;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	do {
		/* request device send status register */
		ret = mmc_wait_for_cmd(card->host, &cmd, 0);
		if (ret)
			break;

		if (!busy && mmc_ffu_busy(&cmd)) {
			busy = 1;
			if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY) {
				pr_err("%sFFU: Warning: Host did not wait for busy state to end.\n",
					mmc_hostname(card->host));
			}
		}

	} while (mmc_ffu_busy(&cmd));

	return ret;
}

static int host_download_fw(u8 *ext_csd, struct mmc_card *card, struct mmc_request *mrq)
{
	struct mmc_command *cmd = mrq->cmd;
	struct mmc_command stop = {0};
	int err;
	/*device reports to the host the value
	 * that the host should set as an argument
	 */
	pr_debug("ffu start download \r\n");
	cmd->arg = ext_csd[EXT_CSD_FFU_ARG] |
			ext_csd[EXT_CSD_FFU_ARG + 1] << 8 |
			ext_csd[EXT_CSD_FFU_ARG + 2] << 16 |
			ext_csd[EXT_CSD_FFU_ARG + 3] << 24;
	mrq->cmd->opcode = MMC_WRITE_MULTIPLE_BLOCK;
	mrq->stop = &stop;
	mrq->stop->opcode = MMC_STOP_TRANSMISSION;
	mrq->stop->arg = 0;
	mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	/* start MMC Firmware bin write request */
	mmc_wait_for_req(card->host, mrq);
	/* device finished Programming State */
	mmc_ffu_wait_busy(card);

	err = mmc_ffu_check_result(mrq);
	if (err) {
		pr_err("%sFFU: err:%d,write Firmware bin FAILED,",
				mmc_hostname(card->host), err);
	}
	return err;
}
void mmc_wait_for_ffu_req(struct mmc_host *host, struct mmc_request *mrq)
{
	struct mmc_card *card = host->card;
	//struct mmc_command *cmd = mrq->cmd;
	u8 *ext_csd = NULL;
	int err;

	/* check emmc device */
	if (!mmc_card_mmc(card)) {
		mrq->cmd->error = -EINVAL;
		return;
	}
	/* read ext_csd */
	err = mmc_get_ext_csd(card, &ext_csd);
	if (err) {
		mrq->cmd->error = -EINVAL;
		pr_err("%sFFU: err:%d,FFU read ext_csd failed",
				mmc_hostname(card->host), err);
		goto exit;
	}
	/* check if emmc support ffu */
	if (!(ext_csd[EXT_CSD_SUPPORTED_MODE] & MMC_FFU_SUPPORTED_MODES)) {
		mrq->cmd->error = -EINVAL;
		pr_err("%sFFU: err:%d,FFU is not supported",
				mmc_hostname(card->host), err);
		goto exit;
	}

	/* check if card is eMMC 5.0 or higher */
	if (ext_csd[EXT_CSD_REV] < 7) {
		mrq->cmd->error = -EINVAL;
		pr_err("%sFFU: err:%d,emmc version is elder than 5.0",
				mmc_hostname(card->host), err);
		goto exit;
	}

	/* make sure emmc enable ffu function */
	if (!(ext_csd[EXT_CSD_FW_CONFIG] & MMC_FFU_FW_CONFIG)) {
			/* printk("FFU: FW updates enabled"); */
	} else {
		err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				EXT_CSD_FW_CONFIG, MMC_FFU_ENABLE,
				card->ext_csd.generic_cmd6_time);
		if (err) {
			mrq->cmd->error = -EINVAL;
			pr_err("%sFFU: err:%d,FW update failed to enabled",
					mmc_hostname(card->host), err);
			goto exit;
		}
	}
	printk("eMMC cache originally %s -> %s\n",
			((card->ext_csd.cache_ctrl) ? "on" : "off"),
			((card->ext_csd.cache_ctrl) ? "turn off" : "keep"));
	if (card->ext_csd.cache_ctrl) {
		mmc_flush_cache(card);
		mmc_ffu_cache_ctrl(card->host, 0);
	}

	/* host sets MODE_CONFIG to ffu mode */
	err = mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_MODE_CONFIG,
			MMC_FFU_MODE_SET, card->ext_csd.generic_cmd6_time);
	if (err) {
		pr_err("%sFFU: err:%d,switch to FFU mode failed\n",
			mmc_hostname(card->host), err);
		err = -EINVAL;
		goto exit;
	}

	/* host download the firmware */
	err = host_download_fw(ext_csd, card, mrq);
	if (err) {
		pr_err("%sFFU: err:%d,download fw fail\n",
				mmc_hostname(card->host), err);
		mmc_ffu_restart(card);
		goto exit;
	}

	err = mmc_ffu_install(card, ext_csd);

exit:
	mrq->cmd->error = err;
	kfree(ext_csd);
}
