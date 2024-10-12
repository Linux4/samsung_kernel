/*
 *
 * FocalTech ftxxxx TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*****************************************************************************
 *
 * File Name: focaltech_ex_mode.c
 *
 * Author: Focaltech Driver Team
 *
 * Created: 2016-08-31
 *
 * Abstract:
 *
 * Reference:
 *
 *****************************************************************************/

/*****************************************************************************
 * 1.Included header files
 *****************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
 * 2.Private constant and macro definitions using #define
 *****************************************************************************/

/*****************************************************************************
 * 3.Private enumerations, structures and unions using typedef
 *****************************************************************************/

/*****************************************************************************
 * 4.Static variables
 *****************************************************************************/

/*****************************************************************************
 * 5.Global variable or extern global variabls/functions
 *****************************************************************************/

/*****************************************************************************
 * 6.Static function prototypes
 *******************************************************************************/

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
static ssize_t fts_glove_mode_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	fts_read_reg(FTS_REG_GLOVE_MODE_EN, &val);
	count = snprintf(buf + count, PAGE_SIZE, "Glove Mode:%s\n",
			ts_data->glove_mode ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Glove Reg(0xC0):%d\n", val);
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_glove_mode_store(
		struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	if (FTS_SYSFS_ECHO_ON(buf)) {
		if (!ts_data->glove_mode) {
			FTS_DEBUG("enter glove mode");
			ret = fts_write_reg(FTS_REG_GLOVE_MODE_EN, ENABLE);
			if (ret >= 0) {
				ts_data->glove_mode = ENABLE;
			}
		}
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		if (ts_data->glove_mode) {
			FTS_DEBUG("exit glove mode");
			ret = fts_write_reg(FTS_REG_GLOVE_MODE_EN, DISABLE);
			if (ret >= 0) {
				ts_data->glove_mode = DISABLE;
			}
		}
	}

	FTS_DEBUG("glove mode:%d", ts_data->glove_mode);
	return count;
}


static ssize_t fts_cover_mode_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	fts_read_reg(FTS_REG_COVER_MODE_EN, &val);
	count = snprintf(buf + count, PAGE_SIZE, "Cover Mode:%s\n",
			ts_data->cover_mode ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Cover Reg(0xC1):%d\n", val);
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_cover_mode_store(
		struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	if (FTS_SYSFS_ECHO_ON(buf)) {
		if (!ts_data->cover_mode) {
			FTS_DEBUG("enter cover mode");
			ret = fts_write_reg(FTS_REG_COVER_MODE_EN, ENABLE);
			if (ret >= 0) {
				ts_data->cover_mode = ENABLE;
			}
		}
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		if (ts_data->cover_mode) {
			FTS_DEBUG("exit cover mode");
			ret = fts_write_reg(FTS_REG_COVER_MODE_EN, DISABLE);
			if (ret >= 0) {
				ts_data->cover_mode = DISABLE;
			}
		}
	}

	FTS_DEBUG("cover mode:%d", ts_data->cover_mode);
	return count;
}

static ssize_t fts_charger_mode_show(
		struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct fts_ts_data *ts_data = fts_data;
	struct input_dev *input_dev = ts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	fts_read_reg(FTS_REG_CHARGER_MODE_EN, &val);
	count = snprintf(buf + count, PAGE_SIZE, "Charger Mode:%s\n",
			ts_data->charger_mode ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Charger Reg(0x8B):%d\n", val);
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_charger_mode_store(
		struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	if (FTS_SYSFS_ECHO_ON(buf)) {
		if (!ts_data->charger_mode) {
			FTS_DEBUG("enter charger mode");
			ret = fts_write_reg(FTS_REG_CHARGER_MODE_EN, ENABLE);
			if (ret >= 0) {
				ts_data->charger_mode = ENABLE;
			}
		}
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		if (ts_data->charger_mode) {
			FTS_DEBUG("exit charger mode");
			ret = fts_write_reg(FTS_REG_CHARGER_MODE_EN, DISABLE);
			if (ret >= 0) {
				ts_data->charger_mode = DISABLE;
			}
		}
	}

	FTS_DEBUG("charger mode:%d", ts_data->glove_mode);
	return count;
}


/* read and write charger mode
 * read example: cat fts_glove_mode        ---read  glove mode
 * write example:echo 1 > fts_glove_mode   ---write glove mode to 01
 */
static DEVICE_ATTR(fts_glove_mode, S_IRUGO | S_IWUSR,
		fts_glove_mode_show, fts_glove_mode_store);

static DEVICE_ATTR(fts_cover_mode, S_IRUGO | S_IWUSR,
		fts_cover_mode_show, fts_cover_mode_store);

static DEVICE_ATTR(fts_charger_mode, S_IRUGO | S_IWUSR,
		fts_charger_mode_show, fts_charger_mode_store);

static struct attribute *fts_touch_mode_attrs[] = {
	&dev_attr_fts_glove_mode.attr,
	&dev_attr_fts_cover_mode.attr,
	&dev_attr_fts_charger_mode.attr,
	NULL,
};

static struct attribute_group fts_touch_mode_group = {
	.attrs = fts_touch_mode_attrs,
};
#endif

int fts_ex_mode_recovery(struct fts_ts_data *ts_data)
{
	if (ts_data->glove_mode) {
		FTS_INFO("enable glove mode");
		fts_write_reg(FTS_REG_GLOVE_MODE_EN, ts_data->glove_mode);
	}

	if (!ts_data->pdata->scan_off_when_cover_closed && ts_data->cover_mode) {
		FTS_INFO("enable cover mode");
		fts_write_reg(FTS_REG_COVER_MODE_EN, ts_data->cover_mode);
	}

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
	if (ts_data->ta_status) {
		FTS_INFO("enable charger mode");
		fts_write_reg(FTS_REG_CHARGER_MODE_EN, ENABLE);
		ts_data->charger_mode = ENABLE;
	}
#endif
	if (ts_data->proximity_mode) {
		fts_write_reg(FTS_REG_PROXIMITY_MODE, ts_data->proximity_mode);
	}

	fts_set_edge_handler(ts_data);

	return 0;
}

int fts_ex_mode_init(struct fts_ts_data *ts_data)
{
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	int ret = 0;
#endif

	ts_data->glove_mode = DISABLE;
	ts_data->cover_mode = DISABLE;
	ts_data->charger_mode = DISABLE;

#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	ret = sysfs_create_group(&ts_data->dev->kobj, &fts_touch_mode_group);
	if (ret < 0) {
		FTS_ERROR("create sysfs(ex_mode) fail");
		sysfs_remove_group(&ts_data->dev->kobj, &fts_touch_mode_group);
		return ret;
	} else {
		FTS_DEBUG("create sysfs(ex_mode) succeedfully");
	}
#endif
	return 0;
}

int fts_ex_mode_exit(struct fts_ts_data *ts_data)
{
#if !IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	sysfs_remove_group(&ts_data->dev->kobj, &fts_touch_mode_group);
#endif
	return 0;
}
