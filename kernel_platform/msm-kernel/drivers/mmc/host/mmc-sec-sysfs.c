// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#include <linux/sysfs.h>
#include <linux/mmc/slot-gpio.h>
#include <linux/mmc/mmc.h>

#include "mmc-sec-sysfs.h"
#include "../core/card.h"

struct device *sec_sdcard_cmd_dev;
struct device *sec_sdinfo_cmd_dev;

static ssize_t sd_sec_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);

	if (!mmc_gpio_get_cd(host)) {
		if (sdi.sd_slot_type == SEC_HYBRID_SD_SLOT) {
			pr_debug("SD slot tray Removed.\n");
			return sprintf(buf, "Notray\n");
		}
	}

	if (host->card) {
		pr_debug("External sd: card inserted.\n");
		return sprintf(buf, "Insert\n");
	}

	pr_debug("External sd: card removed.\n");
	return sprintf(buf, "Remove\n");
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

	err_log = &sdi.err_info[0];
	status_err = &sdi.status_err;

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
			"GE:%d,CC:%d,ECC:%d,WP:%d,OOR:%d,CRC:%lld,TMO:%lld\n",
			status_err->ge_cnt, status_err->cc_cnt,
			status_err->ecc_cnt, status_err->wp_cnt,
			status_err->oor_cnt, crc_cnt, tmo_cnt);
out:
	return len;
}

static ssize_t sd_sec_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	struct mmc_sd_sec_err_info *err_log;
	u64 total_cnt = 0;
	int i;

	if (!card) {
		return snprintf(buf, PAGE_SIZE, "no card\n");
	}

	err_log = &sdi.err_info[0];

	for (i = 0; i < 6; i++) {
		if (total_cnt < U64_MAX)
			total_cnt += err_log[i].count;
	}

	return snprintf(buf, PAGE_SIZE, "%lld\n", total_cnt);
}

static ssize_t sd_sec_cid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;

	if (!card) {
		return snprintf(buf, PAGE_SIZE, "no card\n");
	}

	return snprintf(buf, PAGE_SIZE,
			"%08x%08x%08x%08x\n",
			card->raw_cid[0], card->raw_cid[1],
			card->raw_cid[2], card->raw_cid[3]);
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

	if (!card) {
		/* There should be no spaces in 'No Card'(Vold Team). */
		return snprintf(buf, PAGE_SIZE, "NOCARD\n");
	}

	sd_sec_calc_error_count(err_log, &crc_cnt, &tmo_cnt);

	if (status_err->ge_cnt > 100 || status_err->ecc_cnt > 0 ||
			status_err->wp_cnt > 0 || status_err->oor_cnt > 10 ||
			tmo_cnt > 100 || crc_cnt > 100)
		return snprintf(buf, PAGE_SIZE, "BAD\n");
	else
		return snprintf(buf, PAGE_SIZE, "GOOD\n");
}

static ssize_t sd_sec_reason_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	char *reason = NULL;

	if (card)
		reason = mmc_card_readonly(card) ? "PERMWP" : "NORMAL";
	else
		reason = sdi.failed_init ? "INITFAIL" : "NOCARD";

	return snprintf(buf, PAGE_SIZE, "%s\n", reason);
}

static DEVICE_ATTR(status, 0444, sd_sec_status_show, NULL);
static DEVICE_ATTR(err_count, 0444, mmc_sd_sec_error_count_show, NULL);

static DEVICE_ATTR(sd_count, 0444, sd_sec_count_show, NULL);
static DEVICE_ATTR(data, 0444, sd_sec_cid_show, NULL);
static DEVICE_ATTR(fc, 0444, sd_sec_health_show, NULL);
static DEVICE_ATTR(reason, 0444, sd_sec_reason_show, NULL);

static struct attribute *sdcard_attributes[] = {
	&dev_attr_status.attr,
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
	&dev_attr_reason.attr,
	NULL,
};

static struct attribute_group sdinfo_attr_group = {
	.attrs = sdinfo_attributes,
};

void sd_sec_create_sysfs_group(struct mmc_host *host, struct device **dev,
		const struct attribute_group *dev_attr_group,
		const char *group_name)
{
	*dev = sec_device_create(host, group_name);
	if (IS_ERR(*dev)) {
		pr_err("%s: Failed to create device for %s!\n",
				__func__, group_name);
		return;
	}
	if (sysfs_create_group(&(*dev)->kobj, dev_attr_group))
		pr_err("%s: Failed to create %s sysfs group\n",
				__func__, group_name);
}

void sd_sec_init_sysfs(struct mmc_host *host)
{
	sd_sec_create_sysfs_group(host, &sec_sdcard_cmd_dev,
			&sdcard_attr_group, "sdcard");
	sd_sec_create_sysfs_group(host, &sec_sdinfo_cmd_dev,
			&sdinfo_attr_group, "sdinfo");
}

