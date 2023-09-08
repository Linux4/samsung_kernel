// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#include <linux/sysfs.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/mmc/mmc.h>

static inline void mmc_check_error_count(struct mmc_card_error_log *err_log,
		unsigned long long *total_c_cnt, unsigned long long *total_t_cnt)
{
	int i = 0;
	//Only sbc(0,1)/cmd(2,3)/data(4,5) is checked.
	for (i = 0; i < 6; i++) {
		if (err_log[i].err_type == -EILSEQ && *total_c_cnt < MAX_CNT_U64)
			*total_c_cnt += err_log[i].count;
		if (err_log[i].err_type == -ETIMEDOUT && *total_t_cnt < MAX_CNT_U64)
			*total_t_cnt += err_log[i].count;
	}
}

/* SYSFS about eMMC info */
static struct device *mmc_sec_dev;
/* SYSFS about SD Card Detection */
static struct device *sdcard_sec_dev;
/* SYSFS about SD Card Information */
static struct device *sdinfo_sec_dev;
/* SYSFS about SD Card error Information */
static struct device *sddata_sec_dev;

static ssize_t mmc_gen_unique_number_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	char gen_pnm[3];
	int i;

	switch (card->cid.manfid) {
	case 0x02:	/* Sandisk	-> [3][4] */
	case 0x45:
		sprintf(gen_pnm, "%.*s", 2, card->cid.prod_name + 3);
		break;
	case 0x11:	/* Toshiba	-> [1][2] */
	case 0x90:	/* Hynix */
		sprintf(gen_pnm, "%.*s", 2, card->cid.prod_name + 1);
		break;
	case 0x13:
	case 0xFE:	/* Micron	-> [4][5] */
		sprintf(gen_pnm, "%.*s", 2, card->cid.prod_name + 4);
		break;
	case 0x15:	/* Samsung	-> [0][1] */
	default:
		sprintf(gen_pnm, "%.*s", 2, card->cid.prod_name + 0);
		break;
	}

	/* Convert to Capital */
	for (i = 0 ; i < 2 ; i++) {
		if (gen_pnm[i] >= 'a' && gen_pnm[i] <= 'z')
			gen_pnm[i] -= ('a' - 'A');
	}

	return sprintf(buf, "C%s%02X%08X%02X\n",
			gen_pnm, card->cid.prv, card->cid.serial,
			UNSTUFF_BITS(card->raw_cid, 8, 8));
}

static ssize_t mmc_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_card_error_log *err_log;
	u64 total_c_cnt = 0;
	u64 total_t_cnt = 0;
	int len = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE,
				"\"GE\":\"0\",\"CC\":\"0\",\"ECC\":\"0\",\"WP\":\"0\","
				"\"OOR\":\"0\",\"CRC\":\"0\",\"TMO\":\"0\","
				"\"HALT\":\"0\",\"CQED\":\"0\",\"RPMB\":\"0\"\n");
		goto out;
	}

	err_log = card->err_log;

	mmc_check_error_count(err_log, &total_c_cnt, &total_t_cnt);

	len = snprintf(buf, PAGE_SIZE,
			"\"GE\":\"%d\",\"CC\":\"%d\",\"ECC\":\"%d\",\"WP\":\"%d\","
			"\"OOR\":\"%d\",\"CRC\":\"%lld\",\"TMO\":\"%lld\","
			"\"HALT\":\"%d\",\"CQED\":\"%d\",\"RPMB\":\"%d\"\n",
			err_log[0].ge_cnt, err_log[0].cc_cnt, err_log[0].ecc_cnt,
			err_log[0].wp_cnt, err_log[0].oor_cnt, total_c_cnt, total_t_cnt,
			err_log[0].halt_cnt, err_log[0].cq_cnt, err_log[0].rpmb_cnt);
out:
	return len;
}

static ssize_t mmc_summary_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	char *bus_speed_mode = "";
	static const char *const unit[] = {"B", "KB", "MB", "GB", "TB"};
	uint64_t size;
	int digit = 0, pre_size = 1;
	int len = 0;
	char ret_size[6];

	if (card) {
		/* SIZE */
		size = (uint64_t)card->ext_csd.sectors * card->ext_csd.data_sector_size;

		/* SIZE - unit */
		while (size > 1024) {
			size /= 1024;
			digit++;
			if (digit == 4)
				break;
		}

		/* SIZE - capacity */
		while (size > pre_size) {
			if (pre_size > 1024)
				break;
			pre_size = pre_size << 1;
		}

		sprintf(ret_size, "%d%s", pre_size, unit[digit]);

		/* SPEED MODE */
		if (mmc_card_hs400(card))
			bus_speed_mode = "HS400";
		else if (mmc_card_hs200(card))
			bus_speed_mode = "HS200";
		else if (mmc_card_ddr52(card))
			bus_speed_mode = "DDR50";
		else if (mmc_card_hs(card))
			bus_speed_mode = "HS";
		else
			bus_speed_mode = "LEGACY";

		/* SUMMARY */
#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
		len = sprintf(buf, "\"MANID\":\"0x%02X\",\"PNM\":\"%s\","
				"\"REV\":\"%#x%x%x%x\",\"CQ\":\"%d\","
				"\"SIZE\":\"%s\",\"SPEEDMODE\":\"%s\","
				"\"LIFE\":\"%u\"\n",
				card->cid.manfid, card->cid.prod_name,
				(char)card->ext_csd.fwrev[4],
				(char)card->ext_csd.fwrev[5],
				(char)card->ext_csd.fwrev[6],
				(char)card->ext_csd.fwrev[7],
				(mmc_card_cmdq(card) ? true : false),
				ret_size, bus_speed_mode,
				(card->ext_csd.device_life_time_est_typ_a >
				 card->ext_csd.device_life_time_est_typ_b ?
				 card->ext_csd.device_life_time_est_typ_a :
				 card->ext_csd.device_life_time_est_typ_b));
#else
		len = sprintf(buf, "\"MANID\":\"0x%02X\",\"PNM\":\"%s\","\
				"\"REV\":\"%#x%x%x%x\","\
				"\"SIZE\":\"%s\",\"SPEEDMODE\":\"%s\","\
				"\"LIFE\":\"%u\"\n",
				card->cid.manfid, card->cid.prod_name,
				(char)card->ext_csd.fwrev[4],
				(char)card->ext_csd.fwrev[5],
				(char)card->ext_csd.fwrev[6],
				(char)card->ext_csd.fwrev[7],
				ret_size, bus_speed_mode,
				(card->ext_csd.device_life_time_est_typ_a >
				 card->ext_csd.device_life_time_est_typ_b ?
				 card->ext_csd.device_life_time_est_typ_a :
				 card->ext_csd.device_life_time_est_typ_b));
#endif
		dev_info(dev, "%s", buf);
		return len;
	} else {
		/* SUMMARY : No MMC Case */
		dev_info(dev, "%s : No eMMC Card\n", __func__);
#if defined(CONFIG_MTK_EMMC_CQ_SUPPORT) || defined(CONFIG_MTK_EMMC_HW_CQ)
		return sprintf(buf, "\"MANID\":\"NoCard\",\"PNM\":\"NoCard\",\"REV\":\"NoCard\""
				",\"CQ\":\"NoCard\",\"SIZE\":\"NoCard\",\"SPEEDMODE\":\"NoCard\""
				",\"LIFE\":\"NoCard\"\n");
#else
	    return sprintf(buf, "\"MANID\":\"NoCard\",\"PNM\":\"NoCard\",\"REV\":\"NoCard\""\
				",\"SIZE\":\"NoCard\",\"SPEEDMODE\":\"NoCard\""\
				",\"LIFE\":\"NoCard\"\n");
#endif
	}
}

#ifdef CONFIG_SEC_FACTORY
/* For checking reset pin of mmc*/
static ssize_t mmc_hwrst_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct mmc_card *card = mmc->card;

	if (card)
		return sprintf(buf, "%d\n", card->ext_csd.rst_n_function);
	else
		return sprintf(buf, "no card\n");
}
#endif

static ssize_t error_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_card_error_log *err_log;
	u64 total_c_cnt = 0;
	u64 total_t_cnt = 0;
	int total_len = 0;
	int i = 0;
	static const char *const req_types[] = {
		"sbc  ", "cmd  ", "data ", "stop ", "busy "
	};

	if (!card) {
		total_len = snprintf(buf, PAGE_SIZE, "no card\n");
		goto out;
	}

	err_log = card->err_log;

	total_len += snprintf(buf, PAGE_SIZE,
			"type: err    status: first_issue_time:  last_issue_time:      count\n");

	/*
	 * Init err_log[]
	 * //sbc
	 * err_log[0].err_type = -EILSEQ;
	 * err_log[1].err_type = -ETIMEDOUT;
	 * ...
	 */
	for (i = 0; i < MAX_ERR_LOG_INDEX; i++) {
		strncpy(card->err_log[i].type,
			req_types[i / MAX_ERR_TYPE_INDEX], sizeof(char) * 5);
		card->err_log[i].err_type =
			(i % MAX_ERR_TYPE_INDEX == 0) ?	-EILSEQ : -ETIMEDOUT;

		total_len += snprintf(buf + total_len, PAGE_SIZE - total_len,
				"%5s:%4d 0x%08x %16llu, %16llu, %10d\n",
				err_log[i].type, err_log[i].err_type,
				err_log[i].status,
				err_log[i].first_issue_time,
				err_log[i].last_issue_time,
				err_log[i].count);
	}

	mmc_check_error_count(err_log, &total_c_cnt, &total_t_cnt);

	total_len += snprintf(buf + total_len, PAGE_SIZE - total_len,
			"GE:%d,CC:%d,ECC:%d,WP:%d,OOR:%d,CRC:%lld,TMO:%lld,"
			"HALT:%d,CQEN:%d,RPMB:%d,RST:%d\n",
			err_log[0].ge_cnt, err_log[0].cc_cnt, err_log[0].ecc_cnt,
			err_log[0].wp_cnt, err_log[0].oor_cnt, total_c_cnt, total_t_cnt,
			err_log[0].halt_cnt, err_log[0].cq_cnt, err_log[0].rpmb_cnt,
			err_log[0].hw_rst_cnt);

out:
	return total_len;
}

/* SYSFS for SD card detection */
static ssize_t sdcard_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct msdc_host *host = mmc_priv(mmc);
	bool level;

	if (cd_gpio) {
		// level - inserted : true, removed: false
		level = (host->hw->cd_level == __gpio_get_value(cd_gpio)) ? true : false;
		if (level && mmc->card) {
			pr_err("SD card inserted.\n");
			return sprintf(buf, "Insert\n");
		} else if (level && !mmc->card) {
			pr_err("SD card removed.\n");
			return sprintf(buf, "Remove\n");
		} else {
			pr_err("SD slot tray Removed.\n");
			return sprintf(buf, "Notray\n");
		}
	} else {
		if (mmc->card) {
			pr_err("SD card inserted.\n");
			return sprintf(buf, "Insert\n");
		} else {
			pr_err("SD card removed.\n");
			return sprintf(buf, "Remove\n");
		}
	}
}

static ssize_t sdcard_detect_cnt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n", mmc->card_detect_cnt);
}

static ssize_t sdcard_detect_maxmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	const char *uhs_bus_speed_mode = "";

	if (mmc->caps & MMC_CAP_UHS_SDR104)
		uhs_bus_speed_mode = "SDR104";
	else if (mmc->caps & MMC_CAP_UHS_DDR50)
		uhs_bus_speed_mode = "DDR50";
	else if (mmc->caps & MMC_CAP_UHS_SDR50)
		uhs_bus_speed_mode = "SDR50";
	else if (mmc->caps & MMC_CAP_UHS_SDR25)
		uhs_bus_speed_mode = "SDR25";
	else if (mmc->caps & MMC_CAP_UHS_SDR12)
		uhs_bus_speed_mode = "SDR12";
	else
		uhs_bus_speed_mode = "HS";

	return sprintf(buf, "%s\n", uhs_bus_speed_mode);
}

static ssize_t sdcard_detect_curmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	const char *uhs_bus_speed_mode = "";
	static const char *const uhs_speed[] = {
		[UHS_SDR12_BUS_SPEED]	= "SDR12",
		[UHS_SDR25_BUS_SPEED]	= "SDR25",
		[UHS_SDR50_BUS_SPEED]	= "SDR50",
		[UHS_SDR104_BUS_SPEED]	= "SDR104",
		[UHS_DDR50_BUS_SPEED]	= "DDR50",
	};

	if (mmc && mmc->card) {
		if (mmc_card_uhs(mmc->card))
			uhs_bus_speed_mode = uhs_speed[mmc->card->sd_bus_speed];
		else
			uhs_bus_speed_mode = "HS";
	} else
		uhs_bus_speed_mode = "No Card";

	return sprintf(buf, "%s\n", uhs_bus_speed_mode);
}

static ssize_t sdcard_summary_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	const char *uhs_bus_speed_mode = "";
	static const char *const uhs_speeds[] = {
		[UHS_SDR12_BUS_SPEED]   = "SDR12",
		[UHS_SDR25_BUS_SPEED]   = "SDR25",
		[UHS_SDR50_BUS_SPEED]   = "SDR50",
		[UHS_SDR104_BUS_SPEED]  = "SDR104",
		[UHS_DDR50_BUS_SPEED]   = "DDR50",
	};
	static const char *const unit[] = {"KB", "MB", "GB", "TB"};
	unsigned int size, serial;
	int digit = 1;
	int len = 0;
	char ret_size[6];

	if (card) {
		/* MANID */
		/* SERIAL */
		serial = card->cid.serial & (0x0000FFFF);

		/*SIZE*/
		if (card->csd.read_blkbits == 9)        /* 1 Sector = 512 Bytes */
			size = (card->csd.capacity) >> 1;
		else if (card->csd.read_blkbits == 11)  /* 1 Sector = 2048 Bytes */
			size = (card->csd.capacity) << 1;
		else                                    /* 1 Sector = 1024 Bytes */
			size = card->csd.capacity;

		if (size >= 380000000 && size <= 410000000) {   /* QUIRK 400GB SD Card */
			sprintf(ret_size, "400GB");
		} else if (size >= 190000000 && size <= 210000000) {   /* QUIRK 200GB SD Card */
			sprintf(ret_size, "200GB");
		} else {
			while ((size >> 1) > 0) {
				size = size >> 1;
				digit++;
			}
			sprintf(ret_size, "%d%s", 1 << (digit % 10), unit[digit / 10]);
		}

		/* SPEEDMODE */
		if (mmc_card_uhs(card))
			uhs_bus_speed_mode = uhs_speeds[card->sd_bus_speed];
		else if (mmc_card_hs(card))
			uhs_bus_speed_mode = "HS";
		else
			uhs_bus_speed_mode = "DS";

		/* SUMMARY */
		len = sprintf(buf, "\"MANID\":\"0x%02X\",\"SERIAL\":\"%04X\""
				",\"SIZE\":\"%s\",\"SPEEDMODE\":\"%s\",\"NOTI\":\"%d\"\n",
				card->cid.manfid, serial, ret_size, uhs_bus_speed_mode,
				card->err_log[0].noti_cnt);
		dev_info(dev, "%s", buf);
		return len;
	} else {
		/* SUMMARY : No SD Card Case */
		dev_info(dev, "%s : No SD Card\n", __func__);
		return sprintf(buf, "\"MANID\":\"NoCard\",\"SERIAL\":\"NoCard\""
				",\"SIZE\":\"NoCard\",\"SPEEDMODE\":\"NoCard\",\"NOTI\":\"NoCard\"\n");
	}
}

static ssize_t sd_cid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
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
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_card_error_log *err_log;
	u64 total_c_cnt = 0;
	u64 total_t_cnt = 0;
	int len = 0;

	if (!card) {
		//There should be no spaces in 'No Card'(Vold Team).
		len = snprintf(buf, PAGE_SIZE, "NOCARD\n");
		goto out;
	}

	err_log = card->err_log;

	mmc_check_error_count(err_log, &total_c_cnt, &total_t_cnt);

	if (err_log[0].ge_cnt > 100 || err_log[0].ecc_cnt > 0 ||
		err_log[0].wp_cnt > 0 || err_log[0].oor_cnt > 10 ||
		total_t_cnt > 100 || total_c_cnt > 100)
		len = snprintf(buf, PAGE_SIZE, "BAD\n");
	else
		len = snprintf(buf, PAGE_SIZE, "GOOD\n");

out:
	return len;
}

/* SYSFS for service center support */
static ssize_t sd_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_card_error_log *err_log;
	u64 total_cnt = 0;
	int len = 0;
	int i = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE, "no card\n");
		goto out;
	}

	err_log = card->err_log;

	//Only sbc(0,1)/cmd(2,3)/data(4,5) is checked.
	for (i = 0; i < 6; i++) {
		if (total_cnt < MAX_CNT_U64)
			total_cnt += err_log[i].count;
	}
	len = snprintf(buf, PAGE_SIZE, "%lld\n", total_cnt);

out:
	return len;
}

#define SEC_MMC_STATUS_ERR_INFO_GET_VALUE(member) ({	\
		cur_err_log->member = err_log->member - err_log_backup->member; })

static inline void mmc_check_error_count_calc_current(struct mmc_card *card,
		unsigned long long *total_c_cnt, unsigned long long *total_t_cnt,
		struct mmc_card_error_log *cur_err_log)
{
	struct mmc_card_error_log *err_log = card->err_log;
	struct mmc_card_error_log *err_log_backup = card->err_log_backup;
	int i = 0;

	//Only sbc(0,1)/cmd(2,3)/data(4,5) is checked.
	for (i = 0; i < 6; i++) {
		if (err_log[i].err_type == -EILSEQ && *total_c_cnt < U64_MAX)
			*total_c_cnt += (err_log[i].count - err_log_backup[i].count);
		if (err_log[i].err_type == -ETIMEDOUT && *total_t_cnt < U64_MAX)
			*total_t_cnt += (err_log[i].count - err_log_backup[i].count);
	}

	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(ge_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(cc_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(ecc_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(wp_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(oor_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(halt_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(cq_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(rpmb_cnt);
	SEC_MMC_STATUS_ERR_INFO_GET_VALUE(noti_cnt);
}


#define SEC_MMC_STATUS_ERR_INFO_BACKUP(member) ({		\
		err_log_backup->member = err_log->member;	})

static inline void mmc_backup_err_info(struct mmc_card *card)
{
	struct mmc_card_error_log *err_log = card->err_log;
	struct mmc_card_error_log *err_log_backup = card->err_log_backup;
	int i = 0;

	// save current error count
	for (i = 0; i < MAX_ERR_LOG_INDEX; i++)
		err_log_backup[i].count = err_log[i].count;

	SEC_MMC_STATUS_ERR_INFO_BACKUP(ge_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(cc_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(ecc_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(wp_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(oor_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(halt_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(cq_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(rpmb_cnt);
	SEC_MMC_STATUS_ERR_INFO_BACKUP(noti_cnt);
}

/* SYSFS for big data support */
static ssize_t sd_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_card_error_log cur_err_log;
	u64 total_c_cnt = 0;
	u64 total_t_cnt = 0;
	int len = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE,
				"\"GE\":\"0\",\"CC\":\"0\",\"ECC\":\"0\",\"WP\":\"0\","
				"\"OOR\":\"0\",\"CRC\":\"0\",\"TMO\":\"0\"\n");
		goto out;
	}

	memset(&cur_err_log, 0, sizeof(struct mmc_card_error_log));
	mmc_check_error_count_calc_current(card, &total_c_cnt,
			&total_t_cnt, &cur_err_log);

	len = snprintf(buf, PAGE_SIZE,
			"\"GE\":\"%d\",\"CC\":\"%d\",\"ECC\":\"%d\",\"WP\":\"%d\","
			"\"OOR\":\"%d\",\"CRC\":\"%lld\",\"TMO\":\"%lld\"\n",
			cur_err_log.ge_cnt, cur_err_log.cc_cnt, cur_err_log.ecc_cnt,
			cur_err_log.wp_cnt, cur_err_log.oor_cnt, total_c_cnt, total_t_cnt);
out:
	return len;
}

static ssize_t sd_data_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;

	if (!card)
		return -ENODEV;

	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	mmc_backup_err_info(card);

	return count;
}

static DEVICE_ATTR(un, 0440, mmc_gen_unique_number_show, NULL);
static DEVICE_ATTR(mmc_summary, 0444, mmc_summary_show, NULL);
static DEVICE_ATTR(mmc_data, 0444, mmc_data_show, NULL);
#ifdef CONFIG_SEC_FACTORY
static DEVICE_ATTR(hwrst, 0444, mmc_hwrst_show, NULL);
#endif
static DEVICE_ATTR(err_count, 0444, error_count_show, NULL);

static DEVICE_ATTR(status, 0444, sdcard_status_show, NULL);
static DEVICE_ATTR(cd_cnt, 0444, sdcard_detect_cnt_show, NULL);
static DEVICE_ATTR(max_mode, 0444, sdcard_detect_maxmode_show, NULL);
static DEVICE_ATTR(current_mode, 0444, sdcard_detect_curmode_show, NULL);
static DEVICE_ATTR(sdcard_summary, 0444, sdcard_summary_show, NULL);

static DEVICE_ATTR(data, 0444, sd_cid_show, NULL);
static DEVICE_ATTR(fc, 0444, sd_health_show, NULL);
static DEVICE_ATTR(sd_count, 0444, sd_count_show, NULL);

static DEVICE_ATTR(sd_data, 0664, sd_data_show, sd_data_store);

static struct attribute *mmc_attributes[] = {
	&dev_attr_un.attr,
	&dev_attr_mmc_summary.attr,
	&dev_attr_mmc_data.attr,
#ifdef CONFIG_SEC_FACTORY
	&dev_attr_hwrst.attr,
#endif
	&dev_attr_err_count.attr,
	NULL,
};

static struct attribute_group mmc_attr_group = {
	.attrs = mmc_attributes,
};

static struct attribute *sdcard_attributes[] = {
	&dev_attr_status.attr,
	&dev_attr_cd_cnt.attr,
	&dev_attr_max_mode.attr,
	&dev_attr_current_mode.attr,
	&dev_attr_sdcard_summary.attr,
	&dev_attr_err_count.attr,
	NULL,
};

static struct attribute_group sdcard_attr_group = {
	.attrs = sdcard_attributes,
};

static struct attribute *sdinfo_attributes[] = {
	&dev_attr_data.attr,
	&dev_attr_fc.attr,
	&dev_attr_sd_count.attr,
	NULL,
};

static struct attribute_group sdinfo_attr_group = {
	.attrs = sdinfo_attributes,
};

static struct attribute *sddata_attributes[] = {
	&dev_attr_sd_data.attr,
	NULL,
};

static struct attribute_group sddata_attr_group = {
	.attrs = sddata_attributes,
};

/* Callback function for SD Card IO Error */
static int sdcard_uevent(struct mmc_card *card)
{
	pr_info("%s: Send Notification about SD Card IO Error\n", mmc_hostname(card->host));
	return kobject_uevent(&sdcard_sec_dev->kobj, KOBJ_CHANGE);
}

static int msdc_sdcard_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);
	struct mmc_card *card = NULL;
	struct mmc_card_error_log *err_log = NULL;
	int retval = 0;
	bool card_exist = false;

	add_uevent_var(env, "DEVNAME=%s", dev->kobj.name);

	if (mmc->card) {
		card_exist = true;
		card = mmc->card;
		err_log = card->err_log;
	}

	retval = add_uevent_var(env, "IOERROR=%s", card_exist ? (
				((err_log[0].ge_cnt && !(err_log[0].ge_cnt % 1000)) ||
				 (err_log[0].ecc_cnt && !(err_log[0].ecc_cnt % 1000)) ||
				 (err_log[0].wp_cnt && !(err_log[0].wp_cnt % 100)) ||
				 (err_log[0].oor_cnt && !(err_log[0].oor_cnt % 100)))
				? "YES" : "NO") : "NoCard");

	return retval;
}

static struct device_type sdcard_type = {
	.uevent = msdc_sdcard_uevent,
};

void msdc_sec_create_sysfs_group(struct mmc_host *mmc, struct device **dev,
		const struct attribute_group *dev_attr_group, const char *str)
{
	*dev = sec_device_create(NULL, str);
	if (IS_ERR(*dev))
		pr_err("%s: Failed to create device!\n", __func__);
	else {
		if (sysfs_create_group(&(*dev)->kobj, dev_attr_group))
			pr_err("%s: Failed to create %s sysfs group\n", __func__, str);
		else
			dev_set_drvdata(*dev, mmc);
	}
}

void mmc_sec_init_sysfs(struct mmc_host *mmc)
{
	struct msdc_host *host = mmc_priv(mmc);

	if (host->hw->host_function == MSDC_EMMC)
		msdc_sec_create_sysfs_group(mmc, &mmc_sec_dev,
				&mmc_attr_group, "mmc");

	if (host->hw->host_function == MSDC_SD) {
		msdc_sec_create_sysfs_group(mmc, &sdcard_sec_dev,
				&sdcard_attr_group, "sdcard");
		msdc_sec_create_sysfs_group(mmc, &sdinfo_sec_dev,
				&sdinfo_attr_group, "sdinfo");
		msdc_sec_create_sysfs_group(mmc, &sddata_sec_dev,
				&sddata_attr_group, "sddata");

		if (!IS_ERR(sdcard_sec_dev)) {
			sdcard_sec_dev->type = &sdcard_type;
			mmc->sdcard_uevent = sdcard_uevent;
		}
	}
}
