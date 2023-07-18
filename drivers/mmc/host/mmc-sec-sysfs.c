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
#include <linux/mmc/card.h>
#include <linux/sec_class.h>
#include <linux/mmc/slot-gpio.h>

#include "dw_mmc-exynos.h"
#include "../core/host.h"
#include "mmc-sec-feature.h"
#include "mmc-sec-sysfs.h"

static const char *const uhs_speeds[] = {
	[UHS_SDR12_BUS_SPEED] = "SDR12",
	[UHS_SDR25_BUS_SPEED] = "SDR25",
	[UHS_SDR50_BUS_SPEED] = "SDR50",
	[UHS_SDR104_BUS_SPEED] = "SDR104",
	[UHS_DDR50_BUS_SPEED] = "DDR50",
};

struct device *sec_sdinfo_cmd_dev;
struct device *sec_sddata_cmd_dev;
struct device *sec_mmc_cmd_dev;

#define UNSTUFF_BITS(resp, start, size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

#define MMC_SD_SEC_CALC_STATUS_ERR(member) ({	\
			cur_status_err->member = status_err->member - saved_status_err->member; })

static inline void mmc_sd_sec_get_curr_err_info(struct mmc_sd_sec_device_info *cdi,
		struct mmc_card *card, unsigned long long *crc_cnt,
		unsigned long long *tmo_cnt, struct mmc_sd_sec_status_err_info *cur_status_err)
{
	struct mmc_sd_sec_err_info *err_log = &cdi->err_info[0];
	struct mmc_sd_sec_err_info *saved_err_log = &cdi->saved_err_info[0];
	struct mmc_sd_sec_status_err_info *status_err = &cdi->status_err;
	struct mmc_sd_sec_status_err_info *saved_status_err = &cdi->saved_status_err;

	int i;

	/* Only sbc(0,1)/cmd(2,3)/data(4,5) is checked. */
	for (i = 0; i < 6; i++) {
		if (err_log[i].err_type == -EILSEQ && *crc_cnt < U64_MAX)
			*crc_cnt += (err_log[i].count - saved_err_log[i].count);
		if (err_log[i].err_type == -ETIMEDOUT && *tmo_cnt < U64_MAX)
			*tmo_cnt += (err_log[i].count - saved_err_log[i].count);
	}

	MMC_SD_SEC_CALC_STATUS_ERR(ge_cnt);
	MMC_SD_SEC_CALC_STATUS_ERR(cc_cnt);
	MMC_SD_SEC_CALC_STATUS_ERR(ecc_cnt);
	MMC_SD_SEC_CALC_STATUS_ERR(wp_cnt);
	MMC_SD_SEC_CALC_STATUS_ERR(oor_cnt);
	MMC_SD_SEC_CALC_STATUS_ERR(halt_cnt);
	MMC_SD_SEC_CALC_STATUS_ERR(cq_cnt);
	MMC_SD_SEC_CALC_STATUS_ERR(rpmb_cnt);
	MMC_SD_SEC_CALC_STATUS_ERR(hw_rst_cnt);
	MMC_SD_SEC_CALC_STATUS_ERR(noti_cnt);
}

#define MMC_SD_SEC_SAVE_STATUS_ERR(member) ({		\
		saved_status_err->member = status_err->member;	})

static inline void mmc_sd_sec_save_err_info(struct mmc_sd_sec_device_info *cdi)
{
	struct mmc_sd_sec_err_info *err_log = &cdi->err_info[0];
	struct mmc_sd_sec_err_info *saved_err_log = &cdi->saved_err_info[0];
	struct mmc_sd_sec_status_err_info *status_err = &cdi->status_err;
	struct mmc_sd_sec_status_err_info *saved_status_err = &cdi->saved_status_err;
	int i;

	/* Save current error count */
	for (i = 0; i < MAX_LOG_INDEX; i++)
		saved_err_log[i].count = err_log[i].count;

	MMC_SD_SEC_SAVE_STATUS_ERR(ge_cnt);
	MMC_SD_SEC_SAVE_STATUS_ERR(cc_cnt);
	MMC_SD_SEC_SAVE_STATUS_ERR(ecc_cnt);
	MMC_SD_SEC_SAVE_STATUS_ERR(wp_cnt);
	MMC_SD_SEC_SAVE_STATUS_ERR(oor_cnt);
	MMC_SD_SEC_SAVE_STATUS_ERR(halt_cnt);
	MMC_SD_SEC_SAVE_STATUS_ERR(cq_cnt);
	MMC_SD_SEC_SAVE_STATUS_ERR(rpmb_cnt);
	MMC_SD_SEC_SAVE_STATUS_ERR(hw_rst_cnt);
	MMC_SD_SEC_SAVE_STATUS_ERR(noti_cnt);
}

static ssize_t mmc_sec_unique_number_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	char gen_pnm[3];
	int i;

	if (!card) {
		dev_info(dev, "%s : card is not exist!\n", __func__);
		return -EINVAL;
	}

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

static ssize_t mmc_sec_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_sd_sec_status_err_info status_err;
	u64 crc_cnt = 0;
	u64 tmo_cnt = 0;
	int len = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE,
				"\"GE\":\"0\",\"CC\":\"0\",\"ECC\":\"0\",\"WP\":\"0\","
				"\"OOR\":\"0\",\"CRC\":\"0\",\"TMO\":\"0\","
				"\"HALT\":\"0\",\"CQED\":\"0\",\"RPMB\":\"0\"\n");
		goto out;
	}

	memset(&status_err, 0, sizeof(struct mmc_sd_sec_status_err_info));

	mmc_sd_sec_get_curr_err_info(&mdi, card, &crc_cnt, &tmo_cnt, &status_err);

	len = snprintf(buf, PAGE_SIZE,
			"\"GE\":\"%d\",\"CC\":\"%d\",\"ECC\":\"%d\",\"WP\":\"%d\","
			"\"OOR\":\"%d\",\"CRC\":\"%lld\",\"TMO\":\"%lld\","
			"\"HALT\":\"%d\",\"CQED\":\"%d\",\"RPMB\":\"%d\"\n",
			status_err.ge_cnt, status_err.cc_cnt,
			status_err.ecc_cnt, status_err.wp_cnt,
			status_err.oor_cnt, crc_cnt, tmo_cnt,
			status_err.halt_cnt, status_err.cq_cnt,
			status_err.rpmb_cnt);
out:
	return len;
}

static ssize_t mmc_sec_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;

	if (!card)
		return -ENODEV;

	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	mmc_sd_sec_save_err_info(&mdi);

	return count;
}

static ssize_t mmc_sec_summary_show(struct device *dev,
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
		size = (uint64_t)card->ext_csd.sectors *
				card->ext_csd.data_sector_size;

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
		if (mmc_card_hs400(card) || mmc_card_hs400es(card))
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
		len = sprintf(buf, "\"MANID\":\"0x%02X\",\"PNM\":\"%s\","
			"\"REV\":\"%#x%x%x%x\",\"CQ\":\"%d\","
			"\"SIZE\":\"%s\",\"SPEEDMODE\":\"%s\","
			"\"LIFE\":\"%u\"\n",
			card->cid.manfid, card->cid.prod_name,
			(char)card->ext_csd.fwrev[4],
			(char)card->ext_csd.fwrev[5],
			(char)card->ext_csd.fwrev[6],
			(char)card->ext_csd.fwrev[7],
			((card->host->cqe_on) ? true : false),
			ret_size, bus_speed_mode,
			(card->ext_csd.device_life_time_est_typ_a >
			 card->ext_csd.device_life_time_est_typ_b ?
			 card->ext_csd.device_life_time_est_typ_a :
			 card->ext_csd.device_life_time_est_typ_b));
		dev_info(dev, "%s", buf);
		return len;
	} else {
		/* SUMMARY : No MMC Case */
		dev_info(dev, "%s : No eMMC Card\n", __func__);
		return sprintf(buf, "\"MANID\":\"NoCard\",\"PNM\":\"NoCard\""
				",\"REV\":\"NoCard\",\"CQ\":\"NoCard\",\"SIZE\":\"NoCard\""
				",\"SPEEDMODE\":\"NoCard\",\"LIFE\":\"NoCard\"\n");
	}
}

#ifdef CONFIG_SEC_FACTORY
static ssize_t mmc_sec_hwrst_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;

	if (card)
		return sprintf(buf, "%d\n", card->ext_csd.rst_n_function);
	else
		return sprintf(buf, "no card\n");

}
#endif

static ssize_t mmc_sec_lc_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", mdi.lc);
}

static ssize_t mmc_sec_lc_info_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	mdi.lc = value;

	return count;
}

static inline void sd_sec_get_size(struct mmc_card *card, char *buf, int len)
{
	static const char *const unit[] = {"KB", "MB", "GB", "TB"};
	int capacity;
	int digit = 1;

	if (card->csd.read_blkbits == 9) /* 1 Sector = 512 Bytes */
		capacity = (card->csd.capacity) >> 1;
	else if (card->csd.read_blkbits == 11) /* 1 Sector = 2048 Bytes */
		capacity = (card->csd.capacity) << 1;
	else /* 1 Sector = 1024 Bytes */
		capacity = card->csd.capacity;

	if (capacity >= 380000000 && capacity <= 410000000)
		snprintf(buf, len, "400GB");
	else if (capacity >= 190000000 && capacity <= 210000000)
		snprintf(buf, len, "200GB");
	else {
		while ((capacity >> 1) > 0) {
			capacity >>= 1;
			digit++;
		}
		snprintf(buf, len, "%d%s", 1 << (digit % 10), unit[digit / 10]);
	}
}

static inline void sd_sec_get_speedmode(struct mmc_card *card,
		const char **speedmode)
{
	if (mmc_card_uhs(card))
		*speedmode = uhs_speeds[card->sd_bus_speed];
	else if (mmc_card_hs(card))
		*speedmode = "HS";
	else
		*speedmode = "DS";
}

static ssize_t sd_sec_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);

	if (sdi.sd_slot_type > SEC_NO_DET_SD_SLOT && !gpio_is_valid(sdi.det_gpio))
		goto gpio_error;

	if (!sdi.det_gpio) {
		if (sdi.sd_slot_type == SEC_HYBRID_SD_SLOT) {
			dev_info(dev, "External SD slot tray Removed.\n");
			return sprintf(buf, "Notray\n");
		}
	}

	if (host->card) {
		dev_info(dev, "External SD: card inserted.\n");
		return sprintf(buf, "Insert\n");
	}

	dev_info(dev, "External SD: card removed.\n");
	return sprintf(buf, "Remove\n");

gpio_error:
	dev_info(dev, "%s : External SD detect pin Error\n", __func__);
	return	sprintf(buf, "Error\n");
}

static ssize_t sd_sec_maxmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	const char *bus_speed_mode = "";

	if (host->caps & MMC_CAP_UHS_SDR104)
		bus_speed_mode = "SDR104";
	else if (host->caps & MMC_CAP_UHS_DDR50)
		bus_speed_mode = "DDR50";
	else if (host->caps & MMC_CAP_UHS_SDR50)
		bus_speed_mode = "SDR50";
	else if (host->caps & MMC_CAP_UHS_SDR25)
		bus_speed_mode = "SDR25";
	else if (host->caps & MMC_CAP_UHS_SDR12)
		bus_speed_mode = "SDR12";
	else if (host->caps & MMC_CAP_SD_HIGHSPEED)
		bus_speed_mode = "HS";
	else
		bus_speed_mode = "DS";

	dev_info(dev, "%s : Max supported host speed mode = %s\n",
				__func__, bus_speed_mode);
	return  sprintf(buf, "%s\n", bus_speed_mode);
}

static ssize_t sd_sec_curmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card;
	const char *bus_speed_mode = "";

	if (host && host->card) {
		card = host->card;
		sd_sec_get_speedmode(card, &bus_speed_mode);
	} else
		bus_speed_mode = "No Card";

	dev_info(dev, "%s : Current SD card speed = %s\n",
				__func__, bus_speed_mode);
	return  sprintf(buf, "%s\n", bus_speed_mode);
}

static ssize_t sd_sec_detect_cnt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	dev_info(dev, "%s : CD count is = %u\n",
					__func__, sdi.card_detect_cnt);
	return sprintf(buf, "%u\n", sdi.card_detect_cnt);
}

static ssize_t sd_sec_summary_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card;
	struct mmc_sd_sec_status_err_info *status_err = &sdi.status_err;
	unsigned int serial;
	char size[6];
	const char *bus_speed_mode = "";

	if (host && host->card) {
		card = host->card;

		/* MANID */
		/* SERIAL */
		serial = card->cid.serial & (0x0000FFFF);

		/*SIZE*/
		sd_sec_get_size(card, size, sizeof(size));

		/* SPEEDMODE */
		sd_sec_get_speedmode(card, &bus_speed_mode);

		/* SUMMARY */
		dev_info(dev, "MANID : 0x%02X, SERIAL : %04X, SIZE : %s, SPEEDMODE : %s\n",
				card->cid.manfid, serial, size, bus_speed_mode);
		return sprintf(buf, "\"MANID\":\"0x%02X\",\"SERIAL\":\"%04X\""\
				",\"SIZE\":\"%s\",\"SPEEDMODE\":\"%s\",\"NOTI\":\"%d\"\n",
				card->cid.manfid, serial, size, bus_speed_mode,
				status_err->noti_cnt);
	} else {
		/* SUMMARY : No SD Card Case */
		dev_info(dev, "%s : No SD Card\n", __func__);
		return sprintf(buf, "\"MANID\":\"NoCard\",\"SERIAL\":\"NoCard\""\
			",\"SIZE\":\"NoCard\",\"SPEEDMODE\":\"NoCard\",\"NOTI\":\"NoCard\"\n");
	}
}

static inline void sd_sec_calc_error_count(struct mmc_sd_sec_err_info *err_log,
		unsigned long long *crc_cnt, unsigned long long *tmo_cnt)
{
	int i = 0;

	/* Only sbc(0,1)/cmd(2,3)/data(4,5) is checked. */
	for (i = 0; i < 6; i++) {
		if (err_log[i].err_type == -EILSEQ && *crc_cnt < U64_MAX)
			*crc_cnt += err_log[i].count;
		if (err_log[i].err_type == -ETIMEDOUT && *tmo_cnt < U64_MAX)
			*tmo_cnt += err_log[i].count;
	}
}

static ssize_t mmc_sd_sec_error_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_sd_sec_device_info *cdi;
	struct mmc_sd_sec_err_info *err_log;
	struct mmc_sd_sec_status_err_info *status_err;
	u64 crc_cnt = 0;
	u64 tmo_cnt = 0;
	int len = 0;
	int i;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE, "No card\n");
		goto out;
	}

	cdi = get_device_info(host);
	err_log = &cdi->err_info[0];
	status_err = &cdi->status_err;

	len += snprintf(buf, PAGE_SIZE,
				"type : err    status: first_issue_time:  last_issue_time:      count\n");

	for (i = 0; i < MAX_LOG_INDEX; i++) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"%5s:%4d 0x%08x %16llu, %16llu, %10d\n",
				err_log[i].type, err_log[i].err_type,
				err_log[i].status,
				err_log[i].first_issue_time,
				err_log[i].last_issue_time,
				err_log[i].count);
	}

	sd_sec_calc_error_count(err_log, &crc_cnt, &tmo_cnt);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"GE:%d,CC:%d,ECC:%d,WP:%d,OOR:%d,CRC:%lld,TMO:%lld,HALT:%d,CQED:%d,RPMB:%d,HWRST:%d\n",
			status_err->ge_cnt, status_err->cc_cnt,
			status_err->ecc_cnt, status_err->wp_cnt,
			status_err->oor_cnt, crc_cnt, tmo_cnt,
			status_err->halt_cnt, status_err->cq_cnt,
			status_err->rpmb_cnt, status_err->hw_rst_cnt);

out:
	return len;
}

static ssize_t sd_sec_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_sd_sec_device_info *cdi;
	struct mmc_sd_sec_err_info *err_log;
	u64 total_cnt = 0;
	int len = 0;
	int i;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE, "no card\n");
		goto out;
	}

	cdi = get_device_info(host);
	err_log = &cdi->err_info[0];

	for (i = 0; i < 6; i++) {
		if (total_cnt < U64_MAX)
			total_cnt += err_log[i].count;
	}
	len = snprintf(buf, PAGE_SIZE, "%lld\n", total_cnt);

out:
	return len;
}

static ssize_t sd_sec_cid_show(struct device *dev,
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

static ssize_t sd_sec_health_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_sd_sec_err_info *err_log = &sdi.err_info[0];
	struct mmc_sd_sec_status_err_info *status_err = &sdi.status_err;
	u64 crc_cnt = 0;
	u64 tmo_cnt = 0;
	int len = 0;

	if (!card) {
		/* There should be no spaces in 'No Card'(Vold Team). */
		len = snprintf(buf, PAGE_SIZE, "NOCARD\n");
		goto out;
	}

	sd_sec_calc_error_count(err_log, &crc_cnt, &tmo_cnt);

	if (status_err->ge_cnt > 100 || status_err->ecc_cnt > 0 ||
			status_err->wp_cnt > 0 || status_err->oor_cnt > 10 ||
			tmo_cnt > 100 || crc_cnt > 100)
		len = snprintf(buf, PAGE_SIZE, "BAD\n");
	else
		len = snprintf(buf, PAGE_SIZE, "GOOD\n");

out:
	return len;
}

static ssize_t sd_sec_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_sd_sec_status_err_info status_err;
	u64 crc_cnt = 0;
	u64 tmo_cnt = 0;
	int len = 0;

	if (!card) {
		len = snprintf(buf, PAGE_SIZE,
			"\"GE\":\"0\",\"CC\":\"0\",\"ECC\":\"0\",\"WP\":\"0\"," \
			"\"OOR\":\"0\",\"CRC\":\"0\",\"TMO\":\"0\"\n");
		goto out;
	}

	memset(&status_err, 0, sizeof(struct mmc_sd_sec_status_err_info));

	mmc_sd_sec_get_curr_err_info(&sdi, card, &crc_cnt, &tmo_cnt, &status_err);

	len = snprintf(buf, PAGE_SIZE,
		"\"GE\":\"%d\",\"CC\":\"%d\",\"ECC\":\"%d\",\"WP\":\"%d\"," \
		"\"OOR\":\"%d\",\"CRC\":\"%lld\",\"TMO\":\"%lld\"\n",
		status_err.ge_cnt,
		status_err.cc_cnt,
		status_err.ecc_cnt,
		status_err.wp_cnt,
		status_err.oor_cnt,
		crc_cnt, tmo_cnt);
out:
	return len;
}

static ssize_t sd_sec_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;

	if (!card)
		return -ENODEV;

	if ((buf[0] != 'C' && buf[0] != 'c') || (count != 1))
		return -EINVAL;

	mmc_sd_sec_save_err_info(&sdi);

	return count;
}

static DEVICE_ATTR(un, 0440, mmc_sec_unique_number_show, NULL);
static DEVICE_ATTR(mmc_summary, 0444, mmc_sec_summary_show, NULL);
static DEVICE_ATTR(mmc_data, 0664, mmc_sec_data_show, mmc_sec_data_store);
#ifdef CONFIG_SEC_FACTORY
static DEVICE_ATTR(hwrst, 0444, mmc_sec_hwrst_show, NULL);
#endif
static DEVICE_ATTR(lc, 0664, mmc_sec_lc_info_show, mmc_sec_lc_info_store);
static DEVICE_ATTR(status, 0444, sd_sec_status_show, NULL);
static DEVICE_ATTR(max_mode, 0444, sd_sec_maxmode_show, NULL);
static DEVICE_ATTR(current_mode, 0444, sd_sec_curmode_show, NULL);
static DEVICE_ATTR(cd_cnt, 0444, sd_sec_detect_cnt_show, NULL);
static DEVICE_ATTR(sdcard_summary, 0444, sd_sec_summary_show, NULL);
static DEVICE_ATTR(err_count, 0444, mmc_sd_sec_error_count_show, NULL);
static DEVICE_ATTR(sd_count, 0444, sd_sec_count_show, NULL);
static DEVICE_ATTR(data, 0444, sd_sec_cid_show, NULL);
static DEVICE_ATTR(fc, 0444, sd_sec_health_show, NULL);
static DEVICE_ATTR(sd_data, 0664, sd_sec_data_show, sd_sec_data_store);

static struct attribute *sdcard_attributes[] = {
	&dev_attr_status.attr,
	&dev_attr_max_mode.attr,
	&dev_attr_current_mode.attr,
	&dev_attr_cd_cnt.attr,
	&dev_attr_sdcard_summary.attr,
	&dev_attr_err_count.attr,
	NULL,
};

static struct attribute_group sdcard_attr_group = {
	.attrs = sdcard_attributes,
};

static struct attribute *sdinfo_attributes[] = {
	&dev_attr_sd_count.attr,
	&dev_attr_data.attr,
	&dev_attr_fc.attr,
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

static struct attribute *mmc_attributes[] = {
	&dev_attr_un.attr,
	&dev_attr_mmc_data.attr,
	&dev_attr_mmc_summary.attr,
	&dev_attr_err_count.attr,
#ifdef CONFIG_SEC_FACTORY
	&dev_attr_hwrst.attr,
#endif
	&dev_attr_lc.attr,
	NULL,
};

static struct attribute_group mmc_attr_group = {
	.attrs = mmc_attributes,
};

void sd_sec_create_sysfs_group(struct mmc_host *host, struct device **dev,
		const struct attribute_group *dev_attr_group, const char *group_name)
{
	*dev = sec_device_create(host, group_name);
	if (IS_ERR(*dev)) {
		pr_err("%s: Failed to create device for %s!\n", __func__, group_name);
		return;
	}
	if (sysfs_create_group(&(*dev)->kobj, dev_attr_group))
		pr_err("%s: Failed to create %s sysfs group\n", __func__, group_name);
}

void sd_sec_init_sysfs(struct mmc_host *host)
{
	sd_sec_create_sysfs_group(host, &sec_sdcard_cmd_dev,
			&sdcard_attr_group, "sdcard");
	sd_sec_create_sysfs_group(host, &sec_sdinfo_cmd_dev,
			&sdinfo_attr_group, "sdinfo");
	sd_sec_create_sysfs_group(host, &sec_sddata_cmd_dev,
			&sddata_attr_group, "sddata");
}

void mmc_sec_init_sysfs(struct mmc_host *host)
{
	sd_sec_create_sysfs_group(host, &sec_mmc_cmd_dev,
			&mmc_attr_group, "mmc");
}
