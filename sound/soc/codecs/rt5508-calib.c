/*
 *  sound/soc/codecs/rt5508-calib.c
 *
 *  Copyright (C) 2016 Richtek Technology Corp.
 *  cy_huang <cy_huang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/delay.h>
/* vfs */
#include <linux/fs.h>
#include <asm/segment.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
/* alsa sound header */
#include <sound/soc.h>

#include "rt5508.h"

#define rt5508_calib_path "/data/local/tmp/"
#define RT5508_CALIB_MAGIC (5526789)

enum {
	RT5508_CALIB_CTRL_START = 0,
	RT5508_CALIB_CTRL_DCROFFSET,
	RT5508_CALIB_CTRL_N20DB,
	RT5508_CALIB_CTRL_N15DB,
	RT5508_CALIB_CTRL_N10DB,
	RT5508_CALIB_CTRL_READOTP,
	RT5508_CALIB_CTRL_WRITEOTP,
	RT5508_CALIB_CTRL_WRITEFILE,
	RT5508_CALIB_CTRL_END,
	RT5508_CALIB_CTRL_MAX,
};

static struct class *rt5508_cal_class;
static int calib_status;

static struct file *file_open(const char *path, int flags, int rights)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, rights);
	set_fs(oldfs);
	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		return NULL;
	}
	return filp;
}

static int file_read(struct file *file, unsigned long long offset,
		     unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());
	ret = vfs_read(file, data, size, &offset);
	set_fs(oldfs);
	return ret;
}

static int file_size(struct file *file)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());
	ret = (file->f_path.dentry)->d_inode->i_size;
	set_fs(oldfs);
	return ret;
}

static int file_write(struct file *file, unsigned long long offset,
		     const unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());
	ret = vfs_write(file, data, size, &offset);
	set_fs(oldfs);
	return ret;
}

static void file_close(struct file *file)
{
	filp_close(file, NULL);
}

static int rt5508_calib_get_rspk_times(struct rt5508_chip *chip)
{
	struct snd_soc_codec *codec = chip->codec;
	int i = 0, j = 0;
	int ret = 0;

	dev_dbg(chip->dev, "%s\n", __func__);
	/* RSPK 0x268 ~ 0x26f */
	for (i = 0; i < 8; i++) {
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR,
				    0x26f - i);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x80);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
		if (ret < 0)
			return ret;
		for (j = 7; j >= 0; j--) {
			if (!(ret & (0x01 << j)))
				break;
		}
		if (j >= 0)
			break;
	}
	dev_dbg(chip->dev, "%s: i = %d, j = %d\n", __func__, i, j);
	if (i >= 8) {
		dev_info(chip->dev, "rspk otp empty\n");
		chip->calib_times = 0;
	} else if (i == 0 && j == 7) {
		dev_info(chip->dev, "rspk otp full\n");
		chip->calib_times = 64;
	} else
		chip->calib_times = 8 * (7 - i) + j + 1;
	return 0;
}

static int rt5508_calib_get_dcroffset(struct rt5508_chip *chip)
{
	struct snd_soc_codec *codec = chip->codec;
	int i = 0, j = 0, index = 0;
	uint32_t delta_v = 0, vtemp = 0;
	int ret = 0;

	dev_dbg(codec->dev, "%s\n", __func__);
	for (i = 0; i < 8; i++) {
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR, 0x97 - i);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x80);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
		if (ret < 0)
			return ret;
		for (j = 7; j >= 0; j--) {
			if (!(ret & (0x01 << j)))
				break;
		}
		if (j >= 0)
			break;
	}
	dev_dbg(codec->dev, "i = %d, j= %d\n", i, j);
	if (i >= 8) {
		ret = 0x8000;
		goto bypass_vtemp_read;
	}
	index = ((7 - i) * 8 + j) * 2 + 0x98;
	ret = snd_soc_write(codec, RT5508_REG_OTPADDR, index);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5508_REG_OTPCONF , 0x88);
	if (ret < 0)
		return ret;
	ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
	if (ret < 0)
		return ret;
	/* prevent devide by zero */
	if (ret == 0)
		ret = 0x8000;
bypass_vtemp_read:
	vtemp = ret & 0xffff;
	ret = snd_soc_read(codec, RT5508_REG_VTHRMDATA);
	if (ret < 0)
		return ret;
	delta_v = 2500 * (ret - vtemp) / vtemp;
	return delta_v;
}

static int rt5508_calib_choosen_db(struct rt5508_chip *chip, int choose)
{
	struct snd_soc_codec *codec = chip->codec;
	u32 data = 0;
	int ret = 0;

	dev_info(chip->dev, "%s\n", __func__);
	data = 0x0080;
	ret = snd_soc_write(codec, RT5508_REG_CALIB_REQ, data);
	if (ret < 0)
		return ret;
	switch (choose) {
	case RT5508_CALIB_CTRL_N20DB:
		dev_dbg(chip->dev, "-20db\n");
		data = 0x0ccc;
		break;
	case RT5508_CALIB_CTRL_N15DB:
		dev_dbg(chip->dev, "-15db\n");
		data = 0x16c3;
		break;
	case RT5508_CALIB_CTRL_N10DB:
		dev_dbg(chip->dev, "-10db\n");
		data = 0x287a;
		break;
	default:
		return -EINVAL;
	}
	ret = snd_soc_write(codec, RT5508_REG_CALIB_GAIN, data);
	if (ret < 0)
		return ret;
	ret = snd_soc_read(codec, RT5508_REG_CALIB_CTRL);
	if (ret < 0)
		return ret;
	data = ret;
	data |= 0x80;
	ret = snd_soc_write(codec, RT5508_REG_CALIB_CTRL, data);
	if (ret < 0)
		return ret;
	msleep(120);
	data &= ~(0x80);
	ret = snd_soc_write(codec, RT5508_REG_CALIB_CTRL, data);
	if (ret < 0)
		return ret;
	return snd_soc_read(codec, RT5508_REG_CALIB_OUT0);
}

static int rt5508_calib_read_otp(struct rt5508_chip *chip)
{
	struct snd_soc_codec *codec = chip->codec;
	int i = 0, j = 0, index = 0;
	int ret = 0;

	/* Gsense 0x1a0 ~ 0x1a7 */
	for (i = 0; i < 8; i++) {
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR,
				    0x1a7 - i);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x80);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
		if (ret < 0)
			return ret;
		for (j = 7; j >= 0; j--) {
			if (!(ret & (0x01 << j)))
				break;
		}
		if (j >= 0)
			break;
	}
	dev_info(chip->dev, "i = %d, j= %d\n", i, j);
	if (i >= 8) {
		ret = 0x800000;
		return ret;
	}
	/* Gsense 0x1a8 ~ 0x267 */
	index = ((7 - i) * 8 + j) * 3 + 0x1a8;
	ret = snd_soc_write(codec, RT5508_REG_OTPADDR, index);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5508_REG_OTPCONF , 0x90);
	if (ret < 0)
		return ret;
	return snd_soc_read(codec, RT5508_REG_OTPDIN);
}

static int rt5508_calib_write_otp(struct rt5508_chip *chip)
{
	struct snd_soc_codec *codec = chip->codec;
	int i = 0, j = 0, index = 0;
	int first = 1, cnt = 0;
	uint32_t param = chip->calib_dev.rspk;
	int ret = 0;

	ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
				  RT5508_SPKAMP_ENMASK, 0);
	if (ret < 0)
		return ret;
	if (chip->chip_rev >= RT5508_CHIP_REVG) {
		/* force change to fix mode */
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_MODE,
					  0x03, 0x02);
		if (ret < 0)
			return ret;
		ret = snd_soc_update_bits(codec, RT5508_REG_SAMPCONF,
					  0x40, 0x40);
		if (ret < 0)
			return ret;
	}
	ret = snd_soc_update_bits(codec, RT5508_REG_OVPUVPCTRL, 0x80, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_write(codec, RT5508_REG_IDACTSTEN, 0x03);
	if (ret < 0)
		return ret;
	for (i = 0; i < 22; i++) {
		msleep(2);
		ret = snd_soc_write(codec, RT5508_REG_IDAC2TST,
				    0x21 + i);
		if (ret < 0)
			return ret;
	}
	/* RSPK 0x268 ~ 0x26f */
	for (i = 0; i < 8; i++) {
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR,
				    0x26f - i);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x80);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
		if (ret < 0)
			return ret;
		for (j = 7; j >= 0; j--) {
			if (!(ret & (0x01 << j)))
				break;
		}
		if (j >= 0)
			break;
	}
	if (i >= 8) {
		i = 7;
		j = 0;
		dev_warn(chip->dev, "rspk otp empty\n");
	} else if (i == 0 && j == 7) {
		dev_err(chip->dev, "rspk otp full\n");
		return -EFAULT;
	}
	/* RSPK 0x270 ~ 0x32f */
	index = ((7 - i) * 8 + j) * 3 + 0x270;
WRITE_NOT_EQUAL:
	if (index < 0x32F) {
		if (!first) {
			if (++j > 7) {
				i--;
				j = 0;
			}
		}
		dev_info(chip->dev, "i = %d, j= %d,\n", i, j);
		dev_info(chip->dev, "write valid data\n");
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR, index);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPDIN, param);
		if (ret < 0)
			return ret;
		cnt = 0;
		while (cnt++ < 100) {
			ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x94);
			if (ret < 0)
				return ret;
			mdelay(1);
		}
		msleep(10);
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR, index);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x92);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
		if (ret < 0)
			return ret;
		dev_info(chip->dev, "current data = 0x%08x\n", ret);
		if (ret != param) {
			index += 3;
			first = 0;
			goto WRITE_NOT_EQUAL;
		}
		dev_info(chip->dev, "write valid bit\n");
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR, 0x26f - i);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPDIN, ~(1 << j));
		if (ret < 0)
			return ret;
		cnt = 0;
		while (cnt++ < 100) {
			ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x8c);
			if (ret < 0)
				return ret;
			mdelay(1);
		}
		msleep(10);
		ret = snd_soc_write(codec, RT5508_REG_OTPADDR, 0x26f - i);
		if (ret < 0)
			return ret;
		ret = snd_soc_write(codec, RT5508_REG_OTPCONF, 0x8a);
		if (ret < 0)
			return ret;
		ret = snd_soc_read(codec, RT5508_REG_OTPDIN);
		if (ret < 0)
			return ret;
		if (ret & (1 << j)) {
			index += 3;
			first = 0;
			goto WRITE_NOT_EQUAL;
		}
		dev_info(chip->dev, "otp successfully write\n");
	} else {
		dev_err(chip->dev, "data is full\n");
	}
	for (i = 0; i < 22; i++) {
		msleep(2);
		ret = snd_soc_write(codec, RT5508_REG_IDAC2TST,
				    0x36 - i);
		if (ret < 0)
			return ret;
	}
	ret = snd_soc_write(codec, RT5508_REG_IDACTSTEN, 0x00);
	if (ret < 0)
		return ret;
	ret = snd_soc_update_bits(codec, RT5508_REG_OVPUVPCTRL, 0x80,
				  0x80);
	if (ret < 0)
		return ret;
	if (chip->chip_rev >= RT5508_CHIP_REVG) {
		ret = snd_soc_update_bits(codec, RT5508_REG_SAMPCONF, 0x40, 0);
		if (ret < 0)
			return ret;
		/* revert change to adaptive mode */
		ret = snd_soc_update_bits(codec, RT5508_REG_BST_MODE,
					  0x03, 0x03);
		if (ret < 0)
			return ret;
	}
	ret = snd_soc_update_bits(codec, RT5508_REG_CHIPEN,
				  RT5508_SPKAMP_ENMASK,
				  RT5508_SPKAMP_ENMASK);
	if (ret < 0)
		return ret;
	return 0;
}

static int rt5508_calib_rwotp(struct rt5508_chip *chip, int choose)
{
	int ret = 0;

	dev_info(chip->dev, "%s\n", __func__);
	switch (choose) {
	case RT5508_CALIB_CTRL_READOTP:
		ret = rt5508_calib_read_otp(chip);
		break;
	case RT5508_CALIB_CTRL_WRITEOTP:
		ret = rt5508_calib_write_otp(chip);
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int rt5508_calib_write_file(struct rt5508_chip *chip)
{
	struct rt5508_calib_classdev *pcalib_dev = &chip->calib_dev;
	struct file *calib_file;
	char tmp[100] = {0};
	char file_name[100] = {0};

	snprintf(file_name, 100, rt5508_calib_path "rt5508_calib");
	calib_file = file_open(file_name, O_WRONLY | O_CREAT | O_APPEND,
			       S_IRUGO | S_IWUSR);
	if (!calib_file) {
		dev_err(chip->dev, "open file fail\n");
		return -EFAULT;
	}
	snprintf(tmp, 100, "================================\n");
	file_write(calib_file, 0, tmp, strlen(tmp));
	snprintf(tmp, 100, "index = %d\n", chip->pdev->id);
	file_write(calib_file, 0, tmp, strlen(tmp));
	snprintf(tmp, 100, "dcr_offset -> 0x%08x\n", pcalib_dev->dcr_offset);
	file_write(calib_file, 0, tmp, strlen(tmp));
	snprintf(tmp, 100,  "n20db -> 0x%08x\n", pcalib_dev->n20db);
	file_write(calib_file, 0, tmp, strlen(tmp));
	snprintf(tmp, 100, "n15db -> 0x%08x\n", pcalib_dev->n15db);
	file_write(calib_file, 0, tmp, strlen(tmp));
	snprintf(tmp, 100, "n10db -> 0x%08x\n", pcalib_dev->n10db);
	file_write(calib_file, 0, tmp, strlen(tmp));
	snprintf(tmp, 100, "gsense_otp -> 0x%08x\n", pcalib_dev->gsense_otp);
	file_write(calib_file, 0, tmp, strlen(tmp));
	snprintf(tmp, 100, "rspk -> 0x%08x\n", pcalib_dev->rspk);
	file_write(calib_file, 0, tmp, strlen(tmp));
	snprintf(tmp, 100, "================================\n");
	file_write(calib_file, 0, tmp, strlen(tmp));
	file_close(calib_file);
	return 0;
}

static int rt5508_calib_trigger_read(struct rt5508_calib_classdev *cdev)
{
	struct rt5508_chip *chip = dev_get_drvdata(cdev->dev->parent);
	int ret = 0;

	dev_dbg(chip->dev, "%s\n", __func__);
	ret = rt5508_calib_get_dcroffset(chip);
	if (ret < 0) {
		cdev->dcr_offset = 0xffffffff;
		goto out_trigger_read;
	}
	cdev->dcr_offset = ret;
	dev_dbg(chip->dev, "dcr_offset -> %d\n", cdev->dcr_offset);
	ret = rt5508_calib_choosen_db(chip, RT5508_CALIB_CTRL_N20DB);
	if (ret < 0) {
		cdev->n20db = 0xffffffff;
		goto out_trigger_read;
	}
	cdev->n20db = ret;
	dev_dbg(chip->dev, "n20db -> 0x%08x\n", cdev->n20db);
	ret = rt5508_calib_choosen_db(chip, RT5508_CALIB_CTRL_N15DB);
	if (ret < 0) {
		cdev->n15db = 0xffffffff;
		goto out_trigger_read;
	}
	cdev->n15db = ret;
	dev_dbg(chip->dev, "n15db -> 0x%08x\n", cdev->n15db);
	ret = rt5508_calib_choosen_db(chip, RT5508_CALIB_CTRL_N10DB);
	if (ret < 0) {
		cdev->n10db = 0xffffffff;
		goto out_trigger_read;
	}
	cdev->n10db = ret;
	dev_dbg(chip->dev, "n10db -> 0x%08x\n", cdev->n10db);
	ret = rt5508_calib_rwotp(chip, RT5508_CALIB_CTRL_READOTP);
	if (ret < 0) {
		cdev->gsense_otp = 0xffffffff;
		goto out_trigger_read;
	}
	cdev->gsense_otp = ret;
	return 0;
out_trigger_read:
	return ret;
}

static int rt5508_calib_trigger_write(struct rt5508_calib_classdev *cdev)
{
	struct rt5508_chip *chip = dev_get_drvdata(cdev->dev->parent);
	int ret = 0;

	dev_dbg(chip->dev, "%s\n", __func__);
	ret = rt5508_calib_rwotp(chip, RT5508_CALIB_CTRL_WRITEOTP);
	if (ret < 0)
		goto out_trigger_write;
	ret = rt5508_calib_get_rspk_times(chip);
	if (ret < 0)
		goto out_trigger_write;
	ret = rt5508_calib_trigger_reset(chip);
	if (ret < 0)
		goto out_trigger_write;
	ret = rt5508_calib_write_file(chip);
	if (ret < 0)
		goto out_trigger_write;
	return 0;
out_trigger_write:
	return ret;
}

int rt5508_calib_create(struct rt5508_chip *chip)
{
	struct rt5508_calib_classdev *pcalib_dev = &chip->calib_dev;

	dev_dbg(chip->dev, "%s\n", __func__);
	if (rt5508_calib_get_rspk_times(chip) < 0)
		return -EINVAL;
	pcalib_dev->trigger_read = rt5508_calib_trigger_read;
	pcalib_dev->trigger_write = rt5508_calib_trigger_write;
	pcalib_dev->dev = device_create(rt5508_cal_class, chip->dev, 0,
				pcalib_dev, "rt5508.%d", chip->pdev->id);
	if (IS_ERR(pcalib_dev->dev))
		return -EINVAL;
	return 0;
}
EXPORT_SYMBOL_GPL(rt5508_calib_create);

void rt5508_calib_destroy(struct rt5508_chip *chip)
{
	dev_dbg(chip->dev, "%s\n", __func__);
	device_unregister(chip->calib_dev.dev);
}
EXPORT_SYMBOL_GPL(rt5508_calib_destroy);

static ssize_t rt_calib_dev_attr_show(struct device *,
		struct device_attribute *, char *);
static ssize_t rt_calib_dev_attr_store(struct device *,
		struct device_attribute *, const char *, size_t);
static struct device_attribute rt5508_dev_attrs[] = {
	__ATTR(n20db, 0444, rt_calib_dev_attr_show, rt_calib_dev_attr_store),
	__ATTR(n15db, 0444, rt_calib_dev_attr_show, rt_calib_dev_attr_store),
	__ATTR(n10db, 0444, rt_calib_dev_attr_show, rt_calib_dev_attr_store),
	__ATTR(gsense_otp, 0444, rt_calib_dev_attr_show,
	       rt_calib_dev_attr_store),
	__ATTR(rspk, 0664, rt_calib_dev_attr_show, rt_calib_dev_attr_store),
	__ATTR(calib_data, 0444, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR(calib_times, 0444, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR(dcr_offset, 0444, rt_calib_dev_attr_show,
		rt_calib_dev_attr_store),
	__ATTR_NULL,
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
static struct attribute *rt5508_cal_dev_attrs[] = {
	&rt5508_dev_attrs[0].attr,
	&rt5508_dev_attrs[1].attr,
	&rt5508_dev_attrs[2].attr,
	&rt5508_dev_attrs[3].attr,
	&rt5508_dev_attrs[4].attr,
	&rt5508_dev_attrs[5].attr,
	&rt5508_dev_attrs[6].attr,
	&rt5508_dev_attrs[7].attr,
	NULL,
};

static const struct attribute_group rt5508_cal_group = {
	.attrs = rt5508_cal_dev_attrs,
};

static const struct attribute_group *rt5508_cal_groups[] = {
	&rt5508_cal_group,
	NULL,
};
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)) */

enum {
	RT5508_CALIB_DEV_N20DB = 0,
	RT5508_CALIB_DEV_N15DB,
	RT5508_CALIB_DEV_N10DB,
	RT5508_CALIB_DEV_GSENSE_OTP,
	RT5508_CALIB_DEV_RSPK,
	RT5508_CALIB_DEV_CALIB_DATA,
	RT5508_CALIB_DEV_CALIB_TIMES,
	RT5508_CALIB_DEV_DCROFFSET,
	RT5508_CALIB_DEV_MAX,
};

static int calib_data_file_read(struct rt5508_chip *chip, char *buf)
{
	struct file *calib_file;
	char file_name[100] = {0};
	int len;

	snprintf(file_name, 100, rt5508_calib_path "rt5508_calib");
	calib_file = file_open(file_name,
			       O_RDONLY, S_IRUGO);
	if (!calib_file) {
		dev_err(chip->dev, "open file fail\n");
		return -EIO;
	}
	len = (file_size(calib_file) > PAGE_SIZE ?
			PAGE_SIZE : file_size(calib_file));
	file_read(calib_file, 0, buf, len);
	file_close(calib_file);
	return len;
}

static ssize_t rt_calib_dev_attr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct rt5508_chip *chip = dev_get_drvdata(dev->parent);
	struct rt5508_calib_classdev *calib_dev = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - rt5508_dev_attrs;
	int ret = 0;

	switch (offset) {
	case RT5508_CALIB_DEV_N20DB:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->n20db);
		break;
	case RT5508_CALIB_DEV_N15DB:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->n15db);
		break;
	case RT5508_CALIB_DEV_N10DB:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->n10db);
		break;
	case RT5508_CALIB_DEV_GSENSE_OTP:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n",
				calib_dev->gsense_otp);
		break;
	case RT5508_CALIB_DEV_RSPK:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->rspk);
		break;
	case RT5508_CALIB_DEV_CALIB_DATA:
		ret = calib_data_file_read(chip, buf);
		break;
	case RT5508_CALIB_DEV_CALIB_TIMES:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", chip->calib_times);
		break;
	case RT5508_CALIB_DEV_DCROFFSET:
		ret = scnprintf(buf, PAGE_SIZE, "0x%08x\n", calib_dev->dcr_offset);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static ssize_t rt_calib_dev_attr_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct rt5508_calib_classdev *calib_dev = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - rt5508_dev_attrs;
	uint32_t tmp = 0;

	switch (offset) {
	case RT5508_CALIB_DEV_RSPK:
		if (sscanf(buf, "0x%08x", &tmp) != 1)
			return -EINVAL;
		calib_dev->rspk = tmp;
		break;
	case RT5508_CALIB_DEV_N20DB:
	case RT5508_CALIB_DEV_N15DB:
	case RT5508_CALIB_DEV_N10DB:
	case RT5508_CALIB_DEV_GSENSE_OTP:
	case RT5508_CALIB_DEV_CALIB_DATA:
	case RT5508_CALIB_DEV_CALIB_TIMES:
	case RT5508_CALIB_DEV_DCROFFSET:
	default:
		return -EINVAL;
	}
	return count;
}

static ssize_t rt_calib_class_attr_show(struct class *,
		struct class_attribute *, char *);
static ssize_t rt_calib_class_attr_store(struct class *,
		struct class_attribute *, const char *, size_t);
static struct class_attribute rt5508_class_attrs[] = {
	__ATTR(trigger, 0220, rt_calib_class_attr_show,
	       rt_calib_class_attr_store),
	__ATTR(status, 0444, rt_calib_class_attr_show,
	       rt_calib_class_attr_store),
	__ATTR_NULL,
};

enum {
	RT5508_CALIB_CLASS_TRIGGER = 0,
	RT5508_CALIB_CLASS_STATUS,
	RT5508_CALIB_CLASS_MAX,
};

static ssize_t rt_calib_class_attr_show(struct class *cls,
		struct class_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - rt5508_class_attrs;
	int ret = 0;

	switch (offset) {
	case RT5508_CALIB_CLASS_STATUS:
		ret = scnprintf(buf, PAGE_SIZE, "%d\n", calib_status);
		break;
	case RT5508_CALIB_CLASS_TRIGGER:
	default:
		return -EINVAL;
	}
	return ret;
}

static int rt_calib_trigger_read(struct device *dev, void *data)
{
	struct rt5508_calib_classdev *calib_dev = dev_get_drvdata(dev);

	return calib_dev->trigger_read ?
			calib_dev->trigger_read(calib_dev) : -EINVAL;
}

static int rt_calib_trigger_write(struct device *dev, void *data)
{
	struct rt5508_calib_classdev *calib_dev = dev_get_drvdata(dev);

	return calib_dev->trigger_write ?
			calib_dev->trigger_write(calib_dev) : -EINVAL;
}

static inline int rt_calib_trigger_sequence(struct class *cls, int seq)
{
	int ret = 0;

	switch (seq) {
	case RT5508_CALIB_CTRL_START:
		ret = class_for_each_device(cls, NULL, NULL,
					    rt_calib_trigger_read);
		break;
	case RT5508_CALIB_CTRL_END:
		ret = class_for_each_device(cls, NULL, NULL,
					    rt_calib_trigger_write);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static ssize_t rt_calib_class_attr_store(struct class *cls,
		struct class_attribute *attr, const char *buf, size_t cnt)
{
	const ptrdiff_t offset = attr - rt5508_class_attrs;
	int parse_val = 0;
	int ret = 0;

	switch (offset) {
	case RT5508_CALIB_CLASS_TRIGGER:
		if (sscanf(buf, "%d", &parse_val) != 1)
			return -EINVAL;
		parse_val -= RT5508_CALIB_MAGIC;
		ret = rt_calib_trigger_sequence(cls, parse_val);
		calib_status = ret;
		if (ret < 0)
			return ret;
		break;
	case RT5508_CALIB_CLASS_STATUS:
	default:
		return -EINVAL;
	}
	return cnt;
}

static int __init rt5508_cal_init(void)
{
	int i = 0, ret = 0;

	rt5508_cal_class = class_create(THIS_MODULE, "rt5508_cal");
	if (IS_ERR(rt5508_cal_class))
		return PTR_ERR(rt5508_cal_class);
	for (i = 0; rt5508_class_attrs[i].attr.name; i++) {
		ret = class_create_file(rt5508_cal_class,
					&rt5508_class_attrs[i]);
		if (ret < 0)
			goto out_cal_init;
	}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
	rt5508_cal_class->dev_groups = rt5508_cal_groups;
#else
	rt5508_cal_class->dev_attrs = rt5508_dev_attrs;
#endif /* #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)) */
	return 0;
out_cal_init:
	while (--i >= 0)
		class_remove_file(rt5508_cal_class, &rt5508_class_attrs[i]);
	class_destroy(rt5508_cal_class);
	return ret;
}

static void __exit rt5508_cal_exit(void)
{
	int i = 0;

	for (i = 0; rt5508_class_attrs[i].attr.name; i++)
		class_remove_file(rt5508_cal_class, &rt5508_class_attrs[i]);
	class_destroy(rt5508_cal_class);
}

subsys_initcall(rt5508_cal_init);
module_exit(rt5508_cal_exit);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("RT5508 SPKAMP calibration");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
