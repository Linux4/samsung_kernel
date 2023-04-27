// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *      Storage Driver <storage.sec@samsung.com>
 */

#include <linux/device.h>
#include <linux/sec_class.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/slot-gpio.h>

#include "mmc-sec-sysfs.h"

#define MSDC_EMMC          (0)
#define MSDC_SD            (1)

#define UNSTUFF_BITS(resp, start, size) \
({ \
	const int __size = size; \
	const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1; \
	const int __off = 3 - ((start) / 32); \
	const int __shft = (start) & 31; \
	u32 __res; \
	__res = resp[__off] >> __shft; \
	if (__size + __shft > 32) \
		__res |= resp[__off-1] << ((32 - __shft) % 32); \
	__res & __mask; \
})

/* SYSFS about eMMC info */
static struct device *mmc_sec_dev;
/* SYSFS about SD Card Detection */
static struct device *sdcard_sec_dev;

#define UN_LENGTH 20
static char un_buf[UN_LENGTH + 1];
static int __init un_boot_state_param(char *line)
{
	if (strlen(line) == UN_LENGTH)
		strncpy(un_buf, line, UN_LENGTH);
	else
		pr_err("%s: androidboot.un %s does not match the UN_LENGTH.\n", __func__, line);

	return 1;
}
__setup("androidboot.un=", un_boot_state_param);

static ssize_t mmc_gen_unique_number_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *host = dev_get_drvdata(dev);
	struct mmc_card *card = host->card;
	ssize_t n = 0;

	n = sprintf(buf, "H%02X%02X%02X%X%02X%08X%02X\n",
			card->cid.manfid, card->cid.prod_name[0], card->cid.prod_name[1],
			card->cid.prod_name[2] >> 4, card->cid.prv, card->cid.serial,
			UNSTUFF_BITS(card->raw_cid, 8, 8));

	if (strncmp(un_buf, buf, UN_LENGTH) != 0) {
		pr_info("%s: eMMC UN mismatch\n", __func__);
		BUG_ON(1);
	}

	return n;
}

static ssize_t sdcard_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct mmc_host *mmc = dev_get_drvdata(dev);

	if (mmc_can_gpio_cd(mmc)) {
		if (mmc_gpio_get_cd(mmc)) {
			if (mmc->card) {
				pr_err("SD card inserted.\n");
				return sprintf(buf, "Insert\n");
			} else {
				pr_err("SD card removed.\n");
				return sprintf(buf, "Remove\n");
			}
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

static DEVICE_ATTR(un, 0440, mmc_gen_unique_number_show, NULL);

static DEVICE_ATTR(status, 0444, sdcard_status_show, NULL);

static struct attribute *mmc_attributes[] = {
	&dev_attr_un.attr,
	NULL,
};

static struct attribute_group mmc_attr_group = {
	.attrs = mmc_attributes,
};

static struct attribute *sdcard_attributes[] = {
	&dev_attr_status.attr,
	NULL,
};

static struct attribute_group sdcard_attr_group = {
	.attrs = sdcard_attributes,
};

void msdc_sec_create_sysfs_group(struct mmc_host *mmc, struct device **dev,
		const struct attribute_group *dev_attr_group, const char *group_name)
{
	*dev = sec_device_create(NULL, group_name);
	if (IS_ERR(*dev))
		pr_err("%s: Failed to create device!\n", __func__);
	else {
		if (sysfs_create_group(&(*dev)->kobj, dev_attr_group))
			pr_err("%s: Failed to create %s sysfs group\n", __func__, group_name);
		else
			dev_set_drvdata(*dev, mmc);
	}
}

void mmc_sec_init_sysfs(struct mmc_host *mmc)
{
	if (mmc->host_function == MSDC_EMMC)
		msdc_sec_create_sysfs_group(mmc, &mmc_sec_dev,
				&mmc_attr_group, "mmc");

	if (mmc->host_function == MSDC_SD)
		msdc_sec_create_sysfs_group(mmc, &sdcard_sec_dev,
				&sdcard_attr_group, "sdcard");
}
