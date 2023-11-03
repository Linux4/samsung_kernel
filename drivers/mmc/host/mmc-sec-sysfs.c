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
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/sec_class.h>

#include "dw_mmc-exynos.h"
#include "mmc-sec-feature.h"
#include "mmc-sec-sysfs.h"

static struct device *sd_detection_cmd_dev;
static struct device *sd_data_cmd_dev;
static struct device *sd_info_cmd_dev;

extern struct mmc_sec_sdcard_err_info sd_err_info[2][10];
extern struct mmc_sec_status_err_info sd_status_err_info[2];

static ssize_t sd_detection_cmd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (priv->sec_sd_slot_type > SEC_NO_DET_SD_SLOT && !gpio_is_valid(priv->cd_gpio))
		goto gpio_error;

	if (gpio_get_value(priv->cd_gpio) ^ (host->pdata->use_gpio_invert)
			&& (priv->sec_sd_slot_type == SEC_HYBRID_SD_SLOT)) {
		dev_info(host->dev, "SD slot tray Removed.\n");
		return sprintf(buf, "Notray\n");
	}

	if (host->slot && host->slot->mmc && host->slot->mmc->card) {
		dev_info(host->dev, "SD card inserted.\n");
		return sprintf(buf, "Insert\n");
	}
	dev_info(host->dev, "SD card removed.\n");
	return sprintf(buf, "Remove\n");

gpio_error:
	dev_info(host->dev, "%s : External SD detect pin Error\n", __func__);
	return  sprintf(buf, "Error\n");
}

static ssize_t sd_detection_cnt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_info(dev, "%s : CD count is = %u\n", __func__, sd_op_info.card_detect_cnt);
	return sprintf(buf, "%u\n", sd_op_info.card_detect_cnt);
}

static ssize_t sd_detection_maxmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	const char *uhs_bus_speed_mode = "";
	struct device_node *np = host->dev->of_node;

	if (of_find_property(np, "sd-uhs-sdr104", NULL))
		uhs_bus_speed_mode = "SDR104";
	else if (of_find_property(np, "sd-uhs-ddr50", NULL))
		uhs_bus_speed_mode = "DDR50";
	else if (of_find_property(np, "sd-uhs-sdr50", NULL))
		uhs_bus_speed_mode = "SDR50";
	else if (of_find_property(np, "sd-uhs-sdr25", NULL))
		uhs_bus_speed_mode = "SDR25";
	else if (of_find_property(np, "sd-uhs-sdr12", NULL))
		uhs_bus_speed_mode = "SDR12";
	else
		uhs_bus_speed_mode = "HS";

	dev_info(host->dev, "%s : Max supported Host Speed Mode = %s\n", __func__, uhs_bus_speed_mode);
	return  sprintf(buf, "%s\n", uhs_bus_speed_mode);
}

static ssize_t sd_detection_curmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);

	const char *uhs_bus_speed_mode = "";
	static const char *const uhs_speeds[] = {
		[UHS_SDR12_BUS_SPEED] = "SDR12",
		[UHS_SDR25_BUS_SPEED] = "SDR25",
		[UHS_SDR50_BUS_SPEED] = "SDR50",
		[UHS_SDR104_BUS_SPEED] = "SDR104",
		[UHS_DDR50_BUS_SPEED] = "DDR50",
	};

	if (host->slot && host->slot->mmc && host->slot->mmc->card) {
		if (mmc_card_uhs(host->slot->mmc->card))
			uhs_bus_speed_mode = uhs_speeds[host->slot->mmc->card->sd_bus_speed];
		else
			uhs_bus_speed_mode = "HS";
	} else
		uhs_bus_speed_mode = "No Card";

	dev_info(host->dev, "%s : Current SD Card Speed = %s\n", __func__, uhs_bus_speed_mode);
	return  sprintf(buf, "%s\n", uhs_bus_speed_mode);
}

static ssize_t sdcard_summary_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_card *card;
	const char *uhs_bus_speed_mode = "";
	static const char *const uhs_speeds[] = {
		[UHS_SDR12_BUS_SPEED] = "SDR12",
		[UHS_SDR25_BUS_SPEED] = "SDR25",
		[UHS_SDR50_BUS_SPEED] = "SDR50",
		[UHS_SDR104_BUS_SPEED] = "SDR104",
		[UHS_DDR50_BUS_SPEED] = "DDR50",
	};
	static const char *const unit[] = {"KB", "MB", "GB", "TB"};
	unsigned int size, serial;
	int	digit = 1;
	char ret_size[6];

	if (host->slot && host->slot->mmc && host->slot->mmc->card) {
		card = host->slot->mmc->card;

		/* MANID */
		/* SERIAL */
		serial = card->cid.serial & (0x0000FFFF);

		/*SIZE*/
		if (card->csd.read_blkbits == 9)			/* 1 Sector = 512 Bytes */
			size = (card->csd.capacity) >> 1;
		else if (card->csd.read_blkbits == 11)	/* 1 Sector = 2048 Bytes */
			size = (card->csd.capacity) << 1;
		else								/* 1 Sector = 1024 Bytes */
			size = card->csd.capacity;

		if (size >= 380000000 && size <= 410000000) {	/* QUIRK 400GB SD Card */
			sprintf(ret_size, "400GB");
		} else if (size >= 190000000 && size <= 210000000) {	/* QUIRK 200GB SD Card */
			sprintf(ret_size, "200GB");
		} else {
			while ((size >> 1) > 0) {
				size = size >> 1;
				digit++;
			}
			sprintf(ret_size, "%d%s", 1 << (digit%10), unit[digit/10]);
		}

		/* SPEEDMODE */
		if (mmc_card_uhs(card))
			uhs_bus_speed_mode = uhs_speeds[card->sd_bus_speed];
		else if (mmc_card_hs(card))
			uhs_bus_speed_mode = "HS";
		else
			uhs_bus_speed_mode = "DS";

		/* SUMMARY */
		dev_info(host->dev, "MANID : 0x%02X, SERIAL : %04X, SIZE : %s, SPEEDMODE : %s\n",
				card->cid.manfid, serial, ret_size, uhs_bus_speed_mode);
		return sprintf(buf, "\"MANID\":\"0x%02X\",\"SERIAL\":\"%04X\""\
				",\"SIZE\":\"%s\",\"SPEEDMODE\":\"%s\",\"NOTI\":\"%d\"\n",
				card->cid.manfid, serial, ret_size, uhs_bus_speed_mode,
				sd_status_err_info[host->slot->mmc->index].noti_cnt);
	} else {
		/* SUMMARY : No SD Card Case */
		dev_info(host->dev, "%s : No SD Card\n", __func__);
		return sprintf(buf, "\"MANID\":\"NoCard\",\"SERIAL\":\"NoCard\""\
			",\"SIZE\":\"NoCard\",\"SPEEDMODE\":\"NoCard\",\"NOTI\":\"NoCard\"\n");
	}
}

static ssize_t sd_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	u64 total_cnt = 0;
	int len = 0;
	int i = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE, "no card\n");
		goto out;
	}

	for (i = 0; i < 6; i++) {
		if (total_cnt < U64_MAX)
			total_cnt += sd_err_info[mmc->index][i].count;
	}
	len = snprintf(buf, PAGE_SIZE, "%lld\n", total_cnt);

out:
	return len;
}

static ssize_t sd_cid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	int len = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE, "no card\n");
		goto out;
	}

	len = snprintf(buf, PAGE_SIZE,
			"%08x%08x%08x%08x\n",
			card->raw_cid[0], card->raw_cid[1],
			card->raw_cid[2], card->raw_cid[3]);
out:
	return len;
}

static ssize_t sd_health_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	u64 total_c_cnt = 0;
	u64 total_t_cnt = 0;
	int len = 0;
	int i = 0;

	if (!card) {
		//There should be no spaces in 'No Card'(Vold Team).
		len = snprintf(buf, PAGE_SIZE, "NOCARD\n");
		goto out;
	}

	for (i = 0; i < 6; i++) {
		if (sd_err_info[mmc->index][i].err_type == -EILSEQ && total_c_cnt < U64_MAX)
			total_c_cnt += sd_err_info[mmc->index][i].count;
		if (sd_err_info[mmc->index][i].err_type == -ETIMEDOUT && total_t_cnt < U64_MAX)
			total_t_cnt += sd_err_info[mmc->index][i].count;
	}

	if (sd_status_err_info[mmc->index].ge_cnt > 100 ||
			sd_status_err_info[mmc->index].ecc_cnt > 0 ||
			sd_status_err_info[mmc->index].wp_cnt > 0 ||
			sd_status_err_info[mmc->index].oor_cnt > 10 ||
			total_t_cnt > 100 || total_c_cnt > 100)
		len = snprintf(buf, PAGE_SIZE, "BAD\n");
	else
		len = snprintf(buf, PAGE_SIZE, "GOOD\n");

out:
	return len;
}

static ssize_t sd_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	struct mmc_host *mmc = host->slot->mmc;
	struct mmc_card *card = mmc->card;
	u64 total_c_cnt = 0;
	u64 total_t_cnt = 0;
	int len = 0;
	int i = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE,
			"\"GE\":\"0\",\"CC\":\"0\",\"ECC\":\"0\",\"WP\":\"0\"," \
			"\"OOR\":\"0\",\"CRC\":\"0\",\"TMO\":\"0\"\n");
		goto out;
	}

	for (i = 0; i < 6; i++) {
		if (sd_err_info[mmc->index][i].err_type == -EILSEQ && total_c_cnt < U64_MAX)
			total_c_cnt += sd_err_info[mmc->index][i].count;
		if (sd_err_info[mmc->index][i].err_type == -ETIMEDOUT && total_t_cnt < U64_MAX)
			total_t_cnt += sd_err_info[mmc->index][i].count;
	}

	len = snprintf(buf, PAGE_SIZE,
		"\"GE\":\"%d\",\"CC\":\"%d\",\"ECC\":\"%d\",\"WP\":\"%d\"," \
		"\"OOR\":\"%d\",\"CRC\":\"%lld\",\"TMO\":\"%lld\"\n",
		sd_status_err_info[mmc->index].ge_cnt,
		sd_status_err_info[mmc->index].cc_cnt,
		sd_status_err_info[mmc->index].ecc_cnt,
		sd_status_err_info[mmc->index].wp_cnt,
		sd_status_err_info[mmc->index].oor_cnt,
		total_c_cnt, total_t_cnt);
out:
	return len;
}

static DEVICE_ATTR(status, 0444, sd_detection_cmd_show, NULL);
static DEVICE_ATTR(cd_cnt, 0444, sd_detection_cnt_show, NULL);
static DEVICE_ATTR(max_mode, 0444, sd_detection_maxmode_show, NULL);
static DEVICE_ATTR(current_mode, 0444, sd_detection_curmode_show, NULL);
static DEVICE_ATTR(sdcard_summary, 0444, sdcard_summary_show, NULL);
static DEVICE_ATTR(sd_count, 0444, sd_count_show, NULL);
static DEVICE_ATTR(data, 0444, sd_cid_show, NULL);
static DEVICE_ATTR(fc, 0444, sd_health_show, NULL);
static DEVICE_ATTR(sd_data, 0444, sd_data_show, NULL);

void mmc_sec_init_sysfs(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	/* Initialize cd_cnt to 0. */
	sd_op_info.card_detect_cnt = 0;

	if ((priv->sec_sd_slot_type) >= SEC_NO_DET_SD_SLOT) {
		if (!sd_detection_cmd_dev) {
			sd_detection_cmd_dev = sec_device_create(host, "sdcard");
			if (IS_ERR(sd_detection_cmd_dev))
				pr_err("Fail to create sysfs dev\n");

			if (device_create_file(sd_detection_cmd_dev,
						&dev_attr_status) < 0)
				pr_err("Fail to create status sysfs file\n");

			if (device_create_file(sd_detection_cmd_dev,
						&dev_attr_max_mode) < 0)
				pr_err("Fail to create max_mode sysfs file\n");

			if (device_create_file(sd_detection_cmd_dev,
						&dev_attr_current_mode) < 0)
				pr_err("Fail to create current_mode sysfs file\n");

			if (device_create_file(sd_detection_cmd_dev,
						&dev_attr_cd_cnt) < 0)
				pr_err("Fail to create cd_cnt sysfs file\n");

			if (device_create_file(sd_detection_cmd_dev,
						&dev_attr_sdcard_summary) < 0)
				pr_err("Fail to create sdcard_summary sysfs file\n");
		}

		if (!sd_info_cmd_dev) {
			sd_info_cmd_dev = sec_device_create(host, "sdinfo");
			if (IS_ERR(sd_info_cmd_dev))
				pr_err("Fail to create sysfs dev\n");

			if (device_create_file(sd_info_cmd_dev,
						&dev_attr_sd_count) < 0)
				pr_err("Fail to create sd_count sysfs file\n");

			if (device_create_file(sd_info_cmd_dev,
						&dev_attr_data) < 0)
				pr_err("Fail to create data sysfs file\n");

			if (device_create_file(sd_info_cmd_dev,
						&dev_attr_fc) < 0)
				pr_err("Fail to create fc sysfs file\n");
		}

		if (!sd_data_cmd_dev) {
			sd_data_cmd_dev = sec_device_create(host, "sddata");
			if (IS_ERR(sd_data_cmd_dev))
				pr_err("Fail to create sysfs dev\n");

			if (device_create_file(sd_data_cmd_dev,
						&dev_attr_sd_data) < 0)
				pr_err("Fail to create sd_data sysfs file\n");
		}
	}
}


