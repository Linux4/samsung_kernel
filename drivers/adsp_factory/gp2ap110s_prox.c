/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include "adsp.h"
#define VENDOR "SHARP"
#define CHIP_ID "GP2AP110S"

#define PROX_AVG_COUNT           40
#define PROX_ALERT_THRESHOLD     200
#define PROX_TH_READ             0
#define PROX_TH_WRITE            1
#define BUFFER_MAX               128
#define PROX_REG_START           0x80
#define PROX_SETTINGS_THD_HIGH   550
#define PROX_SETTINGS_THD_LOW    130
#define FORCE_CLOSE_HIGH_THD     750
#define FORCE_CLOSE_LOW_THD      600

#define PROX_SETTINGS_FILE_PATH    "/efs/FactoryApp/prox_settings"

extern unsigned int system_rev;

struct prox_data {
	struct hrtimer prox_timer;
	struct work_struct work_prox;
	struct workqueue_struct *prox_wq;
	struct adsp_data *dev_data;
	int min;
	int max;
	int avg;
	int val;
	int offset;
	int bytes;
	int prox_settings;
	int ps_high_th;
	int ps_low_th;
	int led_reg_val;
	int reg_backup[2];
	int settings_thd_low;
	int settings_thd_high;
	short avgwork_check;
	short avgtimer_enabled;
	bool init_status;
};

enum {
	PRX_THRESHOLD_DETECT_H,
	PRX_THRESHOLD_HIGH_DETECT_L,
	PRX_THRESHOLD_HIGH_DETECT_H,
	PRX_THRESHOLD_RELEASE_L,
};

static struct prox_data *pdata;

static int get_prox_sidx(struct adsp_data *data)
{
	return MSG_PROX;
}

static ssize_t prox_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t prox_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

static ssize_t prox_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	if (pdata->avgwork_check == 0) {
		if (get_prox_sidx(data) == MSG_PROX)
			get_prox_raw_data(&pdata->val, &pdata->offset);
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->val);
}

static ssize_t prox_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n", pdata->min,
		pdata->avg, pdata->max);
}

static ssize_t prox_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int new_value;

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else
		new_value = 1;

	if (new_value == pdata->avgtimer_enabled)
		return size;

	if (new_value == 0) {
		pdata->avgtimer_enabled = 0;
		hrtimer_cancel(&pdata->prox_timer);
		cancel_work_sync(&pdata->work_prox);
	} else {
		pdata->avgtimer_enabled = 1;
		pdata->dev_data = data;
		hrtimer_start(&pdata->prox_timer,
			ns_to_ktime(2000 * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}

	return size;
}

static void prox_work_func(struct work_struct *work)
{
	int min = 0, max = 0, avg = 0;
	int i;

	pdata->avgwork_check = 1;
	for (i = 0; i < PROX_AVG_COUNT; i++) {
		msleep(20);

		if (get_prox_sidx(pdata->dev_data) == MSG_PROX)
			get_prox_raw_data(&pdata->val, &pdata->offset);

		avg += pdata->val;

		if (!i)
			min = pdata->val;
		else if (pdata->val < min)
			min = pdata->val;

		if (pdata->val > max)
			max = pdata->val;
	}
	avg /= PROX_AVG_COUNT;

	pdata->min = min;
	pdata->avg = avg;
	pdata->max = max;
	pdata->avgwork_check = 0;
}

static enum hrtimer_restart prox_timer_func(struct hrtimer *timer)
{
	queue_work(pdata->prox_wq, &pdata->work_prox);
	hrtimer_forward_now(&pdata->prox_timer,
		ns_to_ktime(2000 * NSEC_PER_MSEC));
	return HRTIMER_RESTART;
}

int get_prox_threshold(struct adsp_data *data, int type)
{
	uint8_t cnt = 0;
	uint16_t prox_idx = get_prox_sidx(data);
	int32_t msg_buf[2];
	int ret = 0;

	msg_buf[0] = type;
	msg_buf[1] = 0;

	mutex_lock(&data->prox_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		prox_idx, 0, MSG_TYPE_GET_THRESHOLD);

	while (!(data->ready_flag[MSG_TYPE_GET_THRESHOLD] & 1 << prox_idx) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->ready_flag[MSG_TYPE_GET_THRESHOLD] &= ~(1 << prox_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->prox_factory_mutex);
		return ret;
	}

	ret = data->msg_buf[prox_idx][0];
	mutex_unlock(&data->prox_factory_mutex);

	return ret;
}

void set_prox_threshold(struct adsp_data *data, int type, int val)
{
	uint8_t cnt = 0;
	uint16_t prox_idx = get_prox_sidx(data);
	int32_t msg_buf[2];

	msg_buf[0] = type;
	msg_buf[1] = val;

	mutex_lock(&data->prox_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		prox_idx, 0, MSG_TYPE_SET_THRESHOLD);

	while (!(data->ready_flag[MSG_TYPE_SET_THRESHOLD] & 1 << prox_idx) &&
		cnt++ < TIMEOUT_CNT)
		msleep(20);

	data->ready_flag[MSG_TYPE_SET_THRESHOLD] &= ~(1 << prox_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	mutex_unlock(&data->prox_factory_mutex);
}

static ssize_t prox_cancel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int hi_thd, low_thd;

	hi_thd = get_prox_threshold(data, PRX_THRESHOLD_DETECT_H);
	low_thd = get_prox_threshold(data, PRX_THRESHOLD_RELEASE_L);

	if (pdata->avgwork_check == 0)
		get_prox_raw_data(&pdata->val, &pdata->offset);

	pr_info("[FACTORY] %s: offset: %d, hi thd: %d, lo thd: %d\n", __func__,
		pdata->offset, hi_thd, low_thd);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
			pdata->offset, hi_thd, low_thd);
}

static ssize_t prox_cancel_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	//for LCiA ADC Check sequence
	pr_info("[FACTORY] %s\n", __func__);
	return size;
}

void prox_factory_init_work(void)
{
	pr_info("[FACTORY] %s: Done!\n", __func__);
}

static ssize_t prox_thresh_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int thd;

	thd = get_prox_threshold(data, PRX_THRESHOLD_DETECT_H);
	pr_info("[FACTORY] %s: %d\n", __func__, thd);

	return snprintf(buf, PAGE_SIZE, "%d\n",	thd);
}

static ssize_t prox_thresh_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int thd = 0;

	if (kstrtoint(buf, 10, &thd)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return size;
	}

	set_prox_threshold(data, PRX_THRESHOLD_DETECT_H, thd);
	pr_info("[FACTORY] %s: %d\n", __func__, thd);

	return size;
}

static ssize_t prox_thresh_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int thd;

	thd = get_prox_threshold(data, PRX_THRESHOLD_RELEASE_L);
	pr_info("[FACTORY] %s: %d\n", __func__, thd);

	return snprintf(buf, PAGE_SIZE, "%d\n",	thd);
}

static ssize_t prox_thresh_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int thd = 0;

	if (kstrtoint(buf, 10, &thd)) {
		pr_err("[FACTORY] %s: kstrtoint fail\n", __func__);
		return size;
	}

	set_prox_threshold(data, PRX_THRESHOLD_RELEASE_L, thd);
	pr_info("[FACTORY] %s: %d\n", __func__, thd);

	return size;
}

static ssize_t prox_cancel_pass_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1\n");
}

static ssize_t prox_default_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->offset);
}

static ssize_t prox_alert_thresh_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", PROX_ALERT_THRESHOLD);
}

static ssize_t prox_register_read_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t prox_idx = get_prox_sidx(data);
	int cnt = 0;
	int32_t msg_buf[1];

	msg_buf[0] = pdata->reg_backup[0];

	mutex_lock(&data->prox_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		prox_idx, 0, MSG_TYPE_GET_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_GET_REGISTER] & 1 << prox_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_REGISTER] &= ~(1 << prox_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	pdata->reg_backup[1] = data->msg_buf[prox_idx][0];
	pr_info("[FACTORY] %s: [0x%x]: 0x%x\n",
		__func__, pdata->reg_backup[0], pdata->reg_backup[1]);

	mutex_unlock(&data->prox_factory_mutex);

	return snprintf(buf, PAGE_SIZE, "[0x%x]: 0x%x\n",
		pdata->reg_backup[0], pdata->reg_backup[1]);
}

static ssize_t prox_register_read_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int reg = 0;

	if (sscanf(buf, "%3x", &reg) != 1) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	pdata->reg_backup[0] = reg;
	pr_info("[FACTORY] %s: [0x%x]\n", __func__, pdata->reg_backup[0]);

	return size;
}

static ssize_t prox_register_write_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t prox_idx = get_prox_sidx(data);
	int cnt = 0;
	int32_t msg_buf[2];

	if (sscanf(buf, "%3x,%3x", &msg_buf[0], &msg_buf[1]) != 2) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&data->prox_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		prox_idx, 0, MSG_TYPE_SET_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_SET_REGISTER] & 1 << prox_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_SET_REGISTER] &= ~(1 << prox_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	pdata->reg_backup[0] = msg_buf[0];
	pr_info("[FACTORY] %s: 0x%x - 0x%x\n",
		__func__, msg_buf[0], data->msg_buf[prox_idx][0]);
	mutex_unlock(&data->prox_factory_mutex);

	return size;
}

static ssize_t prox_light_get_dhr_sensor_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t prox_idx = get_prox_sidx(data);
	uint8_t cnt = 0;
	int offset = 0;
	int32_t *info = data->msg_buf[prox_idx];

	mutex_lock(&data->prox_factory_mutex);
	adsp_unicast(NULL, 0, prox_idx, 0, MSG_TYPE_GET_DHR_INFO);
	while (!(data->ready_flag[MSG_TYPE_GET_DHR_INFO] & 1 << prox_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_DHR_INFO] &= ~(1 << prox_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	pr_info("[FACTORY] %d,%d,%d,%d,%02x,%02x,%02x,%02x,%02x,%02x,%02x,%d\n",
		info[0], info[1], info[2], info[3], info[4], info[5],
		info[6], info[7], info[8], info[9], info[10], info[11]);

	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"\"THD\":\"%d %d %d %d\",", info[0], info[1], info[2], info[3]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"\"PDRIVE_CURRENT\":\"%02x\",", info[4]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"\"PERSIST_TIME\":\"%02x\",", info[5]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"\"PPULSE\":\"%02x\",", info[6]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"\"PGAIN\":\"%02x\",", info[7]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"\"PTIME\":\"%02x\",", info[8]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"\"PPLUSE_LEN\":\"%02x\",", info[9]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"\"ATIME\":\"%02x\",", info[10]);
	offset += snprintf(buf + offset, PAGE_SIZE - offset,
		"\"POFFSET\":\"%d\"\n", info[11]);

	mutex_unlock(&data->prox_factory_mutex);
	return offset;
}

static int gp2ap_write_settings(struct adsp_data *data)
{
	struct file *filp = NULL;
	mm_segment_t old_fs;
	int ret = -1;
	char tmp_buf[14] = "";
	char *buf = NULL;
	uint16_t prox_idx = get_prox_sidx(data);
	int cnt = 0;
	int led_reg_val;
	int32_t msg_buf[1];

	if (pdata->prox_settings == 1) {
		led_reg_val = 0x14;	
	} else if (pdata->prox_settings == 2) {
		led_reg_val = 0x24;
	}
	msg_buf[0] = pdata->prox_settings;

	mutex_lock(&data->prox_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		prox_idx, 0, MSG_TYPE_SET_SETTINGS);

	if(pdata->init_status)
	{
		while (!(data->ready_flag[MSG_TYPE_SET_SETTINGS] & 1 << prox_idx) &&
			cnt++ < TIMEOUT_CNT)
			usleep_range(500, 550);

		data->ready_flag[MSG_TYPE_SET_SETTINGS] &= ~(1 << prox_idx);

		if (cnt >= TIMEOUT_CNT)
			pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
	}

	mutex_unlock(&data->prox_factory_mutex);
	
	pdata->led_reg_val = led_reg_val;

	pdata->bytes = snprintf(tmp_buf, PAGE_SIZE, "%d",led_reg_val);

	buf = kzalloc(sizeof(char) * (pdata->bytes), GFP_KERNEL);
	pdata->bytes = snprintf(buf, PAGE_SIZE, "%d",led_reg_val);

	pr_info("[FACTORY] %s: tmp_buf=%s, buf=%s, bytes=%d\n", __func__, tmp_buf, buf, pdata->bytes);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(PROX_SETTINGS_FILE_PATH,
			O_CREAT | O_TRUNC | O_RDWR | O_SYNC, 0666);

	if (filp == NULL) {
		pr_info("[FACTORY] %s: filp is NULL\n", __func__);
		return ret;
	}

	if (IS_ERR(filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(filp);
		pr_err("[FACTORY] %s: Can't open prox settings file (%d)\n", __func__, ret);
		return ret;
	}

	ret = vfs_write(filp, buf, pdata->bytes, &filp->f_pos);
	if (ret != pdata->bytes) {
		pr_err("[FACTORY] %s: Can't write the prox settings data to file, ret=%d\n", __func__, ret);
		ret = -EIO;
	}

	filp_close(filp, current->files);
	set_fs(old_fs);

	msleep(150);

	pr_info("[FACTORY] %s: Done, ret=%d\n", __func__, ret);

	return ret;
}

static int gp2ap_read_settings(struct adsp_data *data)
{
	struct file *filp = NULL;
	mm_segment_t old_fs;
	int ret = -1;

	char *buf = kzalloc(sizeof(char) * (pdata->bytes), GFP_KERNEL);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filp = filp_open(PROX_SETTINGS_FILE_PATH, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(filp);
		pr_err("[FACTORY] %s: Can't open prox settings file (%d)\n", __func__, ret);
		return ret;
	}

	ret = vfs_read(filp, buf, pdata->bytes, &filp->f_pos);
	if (ret <= 0) {
		pr_err("[FACTORY] %s: Can't read the prox settings data from file, bytes=%d\n", __func__, ret);
		ret = -EIO;
	} else {
		sscanf(buf, "%d", &pdata->led_reg_val);
		pr_info("[FACTORY] %s: led_reg_val=%d\n", __func__, pdata->led_reg_val);
	}

	pr_info("[FACTORY] %s: buf=%s\n", __func__, buf);

	filp_close(filp, current->files);
	set_fs(old_fs);

	pr_info("[FACTORY] %s: Done, ret=%d\n", __func__, ret);

	return ret;
}

static ssize_t modify_settings_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data =  dev_get_drvdata(dev);

	pdata->ps_high_th = get_prox_threshold(data, PRX_THRESHOLD_DETECT_H);
	pdata->ps_low_th = get_prox_threshold(data, PRX_THRESHOLD_RELEASE_L);

	if (pdata->ps_high_th == FORCE_CLOSE_HIGH_THD && pdata->ps_low_th == FORCE_CLOSE_LOW_THD) {
		pr_info("[FACTORY] %s: Skip changing proximity settings (%d, %d)\n", __func__, pdata->ps_high_th,pdata->ps_low_th);		
		return size;
	}

	if (sysfs_streq(buf, "1"))
		pdata->prox_settings = 1;
	else if (sysfs_streq(buf, "2"))
		pdata->prox_settings = 2;
	else {
		pr_err("[FACTORY] %s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	pr_info("[FACTORY] %s: prox_settings = %d\n", __func__, pdata->prox_settings);

	gp2ap_write_settings(data);

	return size;
}

static ssize_t modify_settings_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	gp2ap_read_settings(data);

	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->prox_settings);
}

static ssize_t settings_thd_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 value = 0;
	int ret;

	ret = kstrtou16(buf, 10, &value);
	if (ret < 0) {
		pr_err("[FACTORY] %s: kstrtoul failed, ret=0x%x\n", __func__, ret);
		return ret;
	}

	pr_info("[FACTORY] %s: settings_thd_high: %d\n", __func__, value);

	pdata->settings_thd_high = value;

	return size;
}

static ssize_t settings_thd_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->settings_thd_high);
}

static ssize_t settings_thd_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	u16 value = 0;
	int ret;

	ret = kstrtou16(buf, 10, &value);
	if (ret < 0) {
		pr_err("[FACTORY] %s: kstrtoul failed, ret=0x%x\n", __func__, ret);
		return ret;
	}

	pr_info("[FACTORY] %s: settings_thd_low: %d\n", __func__, value);

	pdata->settings_thd_low = value;

	return size;
}

static ssize_t settings_thd_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", pdata->settings_thd_low);

}

static DEVICE_ATTR(vendor, 0444, prox_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, prox_name_show, NULL);
static DEVICE_ATTR(state, 0444, prox_raw_data_show, NULL);
static DEVICE_ATTR(raw_data, 0444, prox_raw_data_show, NULL);
static DEVICE_ATTR(prox_avg, 0664, prox_avg_show, prox_avg_store);
static DEVICE_ATTR(prox_cal, 0664, prox_cancel_show, prox_cancel_store);
static DEVICE_ATTR(thresh_high, 0664,
	prox_thresh_high_show, prox_thresh_high_store);
static DEVICE_ATTR(thresh_low, 0664,
	prox_thresh_low_show, prox_thresh_low_store);
static DEVICE_ATTR(register_write, 0220,
	NULL, prox_register_write_store);
static DEVICE_ATTR(register_read, 0664,
	prox_register_read_show, prox_register_read_store);
static DEVICE_ATTR(prox_offset_pass, 0444, prox_cancel_pass_show, NULL);
static DEVICE_ATTR(prox_trim, 0444, prox_default_trim_show, NULL);
static DEVICE_ATTR(prox_alert_thresh, 0444, prox_alert_thresh_show, NULL);
static DEVICE_ATTR(dhr_sensor_info, 0440,
	prox_light_get_dhr_sensor_info_show, NULL);
static DEVICE_ATTR(modify_settings, 0664, modify_settings_show, modify_settings_store);
static DEVICE_ATTR(settings_thd_high, 0664, settings_thd_high_show, settings_thd_high_store);
static DEVICE_ATTR(settings_thd_low, 0664, settings_thd_low_show, settings_thd_low_store);

static struct device_attribute *prox_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_state,
	&dev_attr_raw_data,
	&dev_attr_prox_avg,
	&dev_attr_prox_cal,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_prox_offset_pass,
	&dev_attr_prox_trim,
	&dev_attr_prox_alert_thresh,
	&dev_attr_dhr_sensor_info,
	&dev_attr_register_write,
	&dev_attr_register_read,
	&dev_attr_modify_settings,
	&dev_attr_settings_thd_high,
	&dev_attr_settings_thd_low,
	NULL,
};

void prox_gp2ap110s_init_settings(struct adsp_data *data)
{
	int ret = -1;

	pr_info("[FACTORY] %s\n", __func__);

	if(0 == pdata->prox_settings)
	{
		ret = gp2ap_read_settings(data);
		if (ret > 0) {
			if (pdata->led_reg_val == 0x24)
				pdata->prox_settings = 2;
			else
				pdata->prox_settings = 1;
			pr_info("[FACTORY] %s: Applied File prox_settings=%d\n", __func__, pdata->prox_settings);
		} else {
			pdata->prox_settings = 1;
			pr_info("[FACTORY] %s: Applied prox_settings=%d\n", __func__, pdata->prox_settings);
		}
		gp2ap_write_settings(data);
	}
	pdata->init_status = true;
}

static int __init prox_factory_init(void)
{
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	adsp_factory_register(MSG_PROX, prox_attrs);
	pr_info("[FACTORY] %s\n", __func__);

	hrtimer_init(&pdata->prox_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pdata->prox_timer.function = prox_timer_func;
	pdata->prox_wq = create_singlethread_workqueue("prox_wq");

	/* this is the thread function we run on the work queue */
	INIT_WORK(&pdata->work_prox, prox_work_func);

	pdata->avgwork_check = 0;
	pdata->avgtimer_enabled = 0;
	pdata->avg = 0;
	pdata->min = 0;
	pdata->max = 0;
	pdata->offset = 0;
	pdata->bytes = 2;
	pdata->prox_settings = 0;
	pdata->ps_high_th = 0;
	pdata->ps_low_th = 0;
	pdata->led_reg_val = 0;
	pdata->init_status = false;
	pdata->settings_thd_high =PROX_SETTINGS_THD_HIGH;
	pdata->settings_thd_low =PROX_SETTINGS_THD_LOW;
	
	return 0;
}

static void __exit prox_factory_exit(void)
{
	if (pdata->avgtimer_enabled == 1) {
		hrtimer_cancel(&pdata->prox_timer);
		cancel_work_sync(&pdata->work_prox);
	}
	destroy_workqueue(pdata->prox_wq);
	adsp_factory_unregister(MSG_PROX);
	kfree(pdata);
	pr_info("[FACTORY] %s\n", __func__);
}

module_init(prox_factory_init);
module_exit(prox_factory_exit);
