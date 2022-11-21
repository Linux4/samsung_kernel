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
#include <linux/dirent.h>
#include "adsp.h"
#define VENDOR "AMS"
#ifdef CONFIG_SUPPORT_TMD4907_FACTORY
#define CHIP_ID "TMD4907"
#else
#define CHIP_ID "TMD4910"
#endif

#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_COMPENSATION_LUX) || \
	defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
#include <linux/panel_notify.h>

#if defined(CONFIG_SEC_BEYOND0QLTE_PROJECT) || \
	defined(CONFIG_SEC_BEYOND1QLTE_PROJECT) || \
	defined(CONFIG_SEC_BEYOND2QLTE_PROJECT) || \
	(defined(CONFIG_SEC_R5Q_PROJECT) && \
	!defined(CONFIG_MACH_R5Q_USA_OPEN))
static unsigned int system_rev __read_mostly;

static int __init sec_hw_rev_setup(char *p)
{
	int ret;

	ret = kstrtouint(p, 0, &system_rev);
	if (unlikely(ret < 0)) {
		pr_warn("androidboot.revision is malformed (%s)\n", p);
		return -EINVAL;
	}

	pr_info("androidboot.revision %x\n", system_rev);

	return 0;
}
early_param("androidboot.revision", sec_hw_rev_setup);

static unsigned int sec_hw_rev(void)
{
	return system_rev;
}
#endif
#define LIGHT_FACTORY_CAL_PATH "/efs/FactoryApp/light_factory_cal"
#define LIGHT_UB_CELL_ID_PATH "/efs/FactoryApp/light_ub_cell_id"
#define UB_CELL_ID_PATH "/sys/class/lcd/panel/SVC_OCTA"
#define UB_CELL_ID_LENGTH 50

#define LIGHT_CAL_PASS 1
#define LIGHT_CAL_FAIL 0
#endif

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/
static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR);
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_ID);
}

int get_light_sidx(struct adsp_data *data)
{
	int ret = MSG_LIGHT;
#ifdef CONFIG_SUPPORT_DUAL_OPTIC
	switch (data->fac_fstate) {
	case FSTATE_INACTIVE:
	case FSTATE_FAC_INACTIVE:
		ret = MSG_LIGHT;
		break;
	case FSTATE_ACTIVE:
	case FSTATE_FAC_ACTIVE:
		ret = MSG_LIGHT_SUB;
		break;
	default:
		break;
	}
#endif
//	pr_info("[FACTORY] %s: idx:%d\n", __func__, ret);
	return ret;
}

static ssize_t light_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(NULL, 0, light_idx, 0, MSG_TYPE_GET_RAW_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_RAW_DATA] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_RAW_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0\n");
	}

	mutex_unlock(&data->light_factory_mutex);
	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d\n",
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2], data->msg_buf[light_idx][3],
		data->msg_buf[light_idx][4], data->msg_buf[light_idx][5]);
}

static ssize_t light_get_dhr_sensor_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	pr_info("[FACTORY] %s: start\n", __func__);
	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(NULL, 0, light_idx, 0, MSG_TYPE_GET_DHR_INFO);
	while (!(data->ready_flag[MSG_TYPE_GET_DHR_INFO] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_DHR_INFO] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	mutex_unlock(&data->light_factory_mutex);
	return data->msg_buf[light_idx][0];
}

#if defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
static ssize_t light_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int brightness = 0;

	if (sscanf(buf, "%3d", &brightness) != 1) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&brightness, sizeof(brightness), light_idx, 0, MSG_TYPE_SET_CAL_DATA);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}
#endif

#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
static ssize_t light_set_aod_rect_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int32_t msg_buf[5] = {OPTION_TYPE_SSC_AOD_RECT, 0, 0, 0, 0};

	if (sscanf(buf, "%3d,%3d,%3d,%3d",
		&msg_buf[1], &msg_buf[2], &msg_buf[3], &msg_buf[4]) != 4) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	pr_info("[FACTORY] %s: rect:%d,%d,%d,%d \n", __func__,
		msg_buf[1], msg_buf[2], msg_buf[3], msg_buf[4]);
	adsp_unicast(msg_buf, sizeof(msg_buf),
			MSG_SSC_CORE, 0, MSG_TYPE_OPTION_DEFINE);
	return size;
}
#endif

#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_COMPENSATION_LUX)
int light_panel_data_notify(struct notifier_block *nb,
	unsigned long val, void *v)
{
	static int32_t pre_bl_level = -1;
	int32_t brightness_data[2] = {0, };
	struct panel_bl_event_data *panel_data = v;

	if (val == PANEL_EVENT_BL_CHANGED) {
		brightness_data[0] = panel_data->bl_level / 100;
		brightness_data[1] = panel_data->aor_data;

		if (brightness_data[0] == pre_bl_level)
			return 0;

		pre_bl_level = brightness_data[0];
#ifdef CONFIG_SUPPORT_DUAL_OPTIC
		adsp_unicast(brightness_data, sizeof(brightness_data),
			MSG_VIR_OPTIC, 0, MSG_TYPE_SET_CAL_DATA);
#else
		adsp_unicast(brightness_data, sizeof(brightness_data),
			MSG_LIGHT, 0, MSG_TYPE_SET_CAL_DATA);
#endif
	        pr_info("[FACTORY] %s: %d, %d\n", __func__,
			brightness_data[0], brightness_data[1]);
	}

	return 0;
}

static struct notifier_block light_panel_data_notifier = {
	.notifier_call = light_panel_data_notify,
	.priority = 1,
};
#endif

#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
static ssize_t light_hallic_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int32_t msg_buf[2];
	int new_value;

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else if (sysfs_streq(buf, "1"))
		new_value = 1;
	else
		return size;

	pr_info("[FACTORY] %s: new_value %d\n", __func__, new_value);
	msg_buf[0] = OPTION_TYPE_SET_HALLIC_INFO;
	msg_buf[1] = new_value;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

static ssize_t light_circle_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SEC_BEYOND0QLTE_PROJECT)
	if (sec_hw_rev() < 14)
		return snprintf(buf, PAGE_SIZE, "47.3 1.1 2.3\n");
	else
		return snprintf(buf, PAGE_SIZE, "47.3 8.8 2.2\n");
#elif defined(CONFIG_SEC_BEYOND1QLTE_PROJECT)
	if (sec_hw_rev() < 14)
		return snprintf(buf, PAGE_SIZE, "49.5 1.2 2.2\n");
	else
		return snprintf(buf, PAGE_SIZE, "49.8 8.6 2.2\n");
#elif defined(CONFIG_SEC_BEYOND2QLTE_PROJECT)
	if (sec_hw_rev() < 14)
		return snprintf(buf, PAGE_SIZE, "48.1 1.0 2.2\n");
	else
		return snprintf(buf, PAGE_SIZE, "48.0 8.8 2.2\n");
#elif defined(CONFIG_SEC_BEYONDXQ_PROJECT)
	return snprintf(buf, PAGE_SIZE, "26.8 7.3 2.2\n");
#elif defined(CONFIG_SEC_D1Q_PROJECT) || defined(CONFIG_SEC_D1XQ_PROJECT)
	return snprintf(buf, PAGE_SIZE, "41.3 7.1 2.4\n");
#elif defined(CONFIG_SEC_D2Q_PROJECT) || defined(CONFIG_SEC_D2XQ_PROJECT) || defined(CONFIG_SEC_D2XQ2_PROJECT)
	return snprintf(buf, PAGE_SIZE, "43.8 6.7 2.4\n");
#elif defined(CONFIG_SEC_R3Q_PROJECT)
	return snprintf(buf, PAGE_SIZE, "27.1 6.2 2.4\n");
#elif defined(CONFIG_SEC_R5Q_PROJECT)
#if defined(CONFIG_MACH_R5Q_USA_OPEN)
	return snprintf(buf, PAGE_SIZE, "45.4 5.1 2.4\n");
#else
	if (sec_hw_rev() < 6)
		return snprintf(buf, PAGE_SIZE, "31.9 13.8 2.4\n");
	else
		return snprintf(buf, PAGE_SIZE, "45.4 5.1 2.4\n");
#endif
#elif defined(CONFIG_SEC_BLOOMQ_PROJECT)
	return snprintf(buf, PAGE_SIZE, "34.1 11.6 2.4\n");
#else
	return snprintf(buf, PAGE_SIZE, "0 0 0\n");
#endif
}
#endif

#ifdef CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR
static ssize_t light_read_copr_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int32_t msg_buf[2];
	int new_value;

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else if (sysfs_streq(buf, "1"))
		new_value = 1;
	else
		return size;

	pr_info("[FACTORY] %s: new_value %d\n", __func__, new_value);
	msg_buf[0] = OPTION_TYPE_COPR_ENABLE;
	msg_buf[1] = new_value;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

static ssize_t light_read_copr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_COPR;
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	pr_info("[FACTORY] %s: %d\n", __func__, data->msg_buf[light_idx][4]);
	mutex_unlock(&data->light_factory_mutex);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->msg_buf[light_idx][4]);
}

static ssize_t light_copr_roix_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(NULL, 0, light_idx, 0, MSG_TYPE_GET_DUMP_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_GET_DUMP_REGISTER] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_DUMP_REGISTER] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "-1,-1,-1,-1\n");
	}

	pr_info("[FACTORY] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2], data->msg_buf[light_idx][3],
		data->msg_buf[light_idx][4], data->msg_buf[light_idx][5],
		data->msg_buf[light_idx][6], data->msg_buf[light_idx][7],
		data->msg_buf[light_idx][8], data->msg_buf[light_idx][9],
		data->msg_buf[light_idx][10], data->msg_buf[light_idx][11]);

	mutex_unlock(&data->light_factory_mutex);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2], data->msg_buf[light_idx][3],
		data->msg_buf[light_idx][4], data->msg_buf[light_idx][5],
		data->msg_buf[light_idx][6], data->msg_buf[light_idx][7],
		data->msg_buf[light_idx][8], data->msg_buf[light_idx][9],
		data->msg_buf[light_idx][10], data->msg_buf[light_idx][11]);
}

static ssize_t light_test_copr_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_COPR;
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "-1,-1,-1,-1\n");
	}

	pr_info("[FACTORY] %s: %d,%d,%d,%d\n", __func__,
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2], data->msg_buf[light_idx][3]);

	mutex_unlock(&data->light_factory_mutex);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d\n",
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2], data->msg_buf[light_idx][3]);
}

static ssize_t light_ddi_spi_check_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_CHIP_ID;
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	pr_info("[FACTORY] %s: %d\n", __func__, data->msg_buf[light_idx][0]);

	mutex_unlock(&data->light_factory_mutex);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->msg_buf[light_idx][0]);
}

static ssize_t light_boled_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int32_t msg_buf[2];
	int new_value;

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else if (sysfs_streq(buf, "1"))
		new_value = 1;
	else
		return size;

	pr_info("[FACTORY] %s: new_value %d\n", __func__, new_value);
	msg_buf[0] = OPTION_TYPE_BOLED_ENABLE;
	msg_buf[1] = new_value;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

static ssize_t light_register_read_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int cnt = 0;
	int32_t msg_buf[1];

	msg_buf[0] = data->msg_buf[light_idx][MSG_LIGHT_MAX - 1];

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_GET_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_GET_REGISTER] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_REGISTER] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	pr_info("[FACTORY] %s: [0x%x]: 0x%x\n",
		__func__, data->msg_buf[light_idx][MSG_LIGHT_MAX - 1],
		data->msg_buf[light_idx][0]);

	mutex_unlock(&data->light_factory_mutex);

	return snprintf(buf, PAGE_SIZE, "[0x%x]: 0x%x\n",
		data->msg_buf[light_idx][MSG_LIGHT_MAX - 1],
		data->msg_buf[light_idx][0]);
}

static ssize_t light_register_read_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int reg = 0;
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);

	if (sscanf(buf, "%3x", &reg) != 1) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	data->msg_buf[light_idx][MSG_LIGHT_MAX - 1] = reg;
	pr_info("[FACTORY] %s: [0x%x]\n", __func__,
		data->msg_buf[light_idx][MSG_LIGHT_MAX - 1]);

	return size;
}

static ssize_t light_register_write_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int cnt = 0;
	int32_t msg_buf[2];

	if (sscanf(buf, "%3x,%3x", &msg_buf[0], &msg_buf[1]) != 2) {
		pr_err("[FACTORY]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_SET_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_SET_REGISTER] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_SET_REGISTER] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);

	data->msg_buf[light_idx][MSG_LIGHT_MAX - 1] = msg_buf[0];
	pr_info("[FACTORY] %s: 0x%x - 0x%x\n",
		__func__, msg_buf[0], data->msg_buf[light_idx][0]);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

int light_get_cal_data(int32_t *cal_data)
{
	struct file *factory_cal_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	factory_cal_filp = filp_open(LIGHT_FACTORY_CAL_PATH,
			O_RDONLY, 0440);

	if (IS_ERR(factory_cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(factory_cal_filp);
		pr_err("[FACTORY] %s: open fail light_factory_cal:%d\n",
			__func__, ret);
		return ret;
	}

	ret = vfs_read(factory_cal_filp, (char *)cal_data,
		sizeof(int32_t) * 2, &factory_cal_filp->f_pos);
	if (ret < 0)
		pr_err("[FACTORY] %s: fd read fail:%d\n", __func__, ret);

	filp_close(factory_cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

int light_set_cal_data(int32_t *cal_data, bool first_booting)
{
	struct file *factory_cal_filp = NULL;
	mm_segment_t old_fs;
	int flag, ret = 0;
	umode_t mode = 0;

	if (cal_data[0] < 0) {
		pr_err("[FACTORY] %s: cal value error: %d, %d\n",
			__func__, cal_data[0], cal_data[1]);
		return -1;
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (first_booting) {
		flag = O_TRUNC | O_RDWR | O_CREAT;
		mode = 0600;
	} else {
		flag = O_RDWR;
		mode = 0660;
	}

	factory_cal_filp = filp_open(LIGHT_FACTORY_CAL_PATH, flag, mode);

	if (IS_ERR(factory_cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(factory_cal_filp);
		pr_err("[FACTORY] %s: open fail light_factory_cal:%d\n",
			__func__, ret);
		return ret;
	}

	ret = vfs_write(factory_cal_filp, (char *)cal_data,
		sizeof(int32_t) * 2, &factory_cal_filp->f_pos);
	if (ret < 0)
		pr_err("[FACTORY] %s: fd write %d\n", __func__, ret);

	filp_close(factory_cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

int light_get_ub_cell_id(char *id_str)
{
	struct file *file_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	file_filp = filp_open(UB_CELL_ID_PATH, O_RDONLY, 0440);

	if (IS_ERR(file_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(file_filp);
		pr_err("[FACTORY] %s: open fail:%d\n",
			__func__, ret);
		return ret;
	}

	ret = vfs_read(file_filp, (char *)id_str,
		sizeof(char) * UB_CELL_ID_LENGTH, &file_filp->f_pos);
	if (ret < 0)
		pr_err("[FACTORY] %s: fd read fail:%d\n",
			__func__, ret);
	else
		pr_info("[FACTORY] %s: UB cell ID: %s\n",
			__func__, id_str);

	filp_close(file_filp, current->files);
	set_fs(old_fs);

	return ret;
}

int light_set_ub_cell_id(char *id_str, bool first_booting)
{
	struct file *file_filp = NULL;
	mm_segment_t old_fs;
	int flag, ret = 0;
	umode_t mode = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (first_booting) {
		flag = O_TRUNC | O_RDWR | O_CREAT;
		mode = 0600;
	} else {
		flag = O_RDWR;
		mode = 0660;
	}

	file_filp = filp_open(LIGHT_UB_CELL_ID_PATH, flag, mode);

	if (IS_ERR(file_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(file_filp);
		pr_err("[FACTORY] %s: open fail light_factory_cal:%d\n",
			__func__, ret);
		return ret;
	}

	ret = vfs_write(file_filp, (char *)id_str,
		sizeof(char) * UB_CELL_ID_LENGTH, &file_filp->f_pos);
	if (ret < 0)
		pr_err("[FACTORY] %s: fd write %d\n", __func__, ret);

	filp_close(file_filp, current->files);
	set_fs(old_fs);

	pr_info("[FACTORY] %s: %s\n", __func__, id_str);

	return ret;
}

void light_factory_init_work(struct adsp_data *data)
{
#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
	int32_t rect_msg[5] = {OPTION_TYPE_SSC_AOD_LIGHT_CIRCLE, 546, 170, 576, 200};
#endif
#ifdef CONFIG_SUPPORT_TMD4907_FACTORY
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	int32_t init_data[2] = {LIGHT_CAL_FAIL, -1};
	int32_t msg_buf[2];
	int ret = 0;
	bool ub_check = false;
	char *efs_id_str = kzalloc(UB_CELL_ID_LENGTH, GFP_KERNEL);
	char *cur_id_str = kzalloc(UB_CELL_ID_LENGTH, GFP_KERNEL);

	data->light_cal = -1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(LIGHT_UB_CELL_ID_PATH, O_RDONLY, 0440);
	if (PTR_ERR(cal_filp) == -ENOENT || PTR_ERR(cal_filp) == -ENXIO) {
		pr_info("[FACTORY] %s : no light_ub_cell_id file\n", __func__);
		set_fs(old_fs);
		light_set_ub_cell_id(cur_id_str, true);
	} else if (IS_ERR(cal_filp)) {
		pr_err("[FACTORY]: %s - light_ub_cell_id filp_open error\n",
			__func__);
		set_fs(old_fs);
	} else {
		ret = vfs_read(cal_filp, (char *)efs_id_str,
			sizeof(char) * UB_CELL_ID_LENGTH, &cal_filp->f_pos);
		if (ret < 0) {
			pr_err("[FACTORY] %s: light_ub_cell_id read fail:%d\n",
				__func__, ret);
			filp_close(cal_filp, current->files);
			set_fs(old_fs);
		} else {
			pr_info("[FACTORY] %s : light_ub_cell_id exist %s\n",
				__func__, efs_id_str);

			filp_close(cal_filp, current->files);
			set_fs(old_fs);

			if (efs_id_str[0] != 0) {
				light_get_ub_cell_id(cur_id_str);
				if (strcmp(efs_id_str, cur_id_str) == 0) {
					ub_check = true;
					pr_info("[FACTORY] %s : UB is matched!\n",
						__func__);
				} else {
					pr_info("[FACTORY] %s : UB is not matched!\n",
						__func__);
				}
			} else {
				pr_info("[FACTORY] %s : UB efs is NULL\n",
					__func__);
			}
		}
	}
	kfree(efs_id_str);
	kfree(cur_id_str);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(LIGHT_FACTORY_CAL_PATH, O_RDONLY, 0440);
	if (PTR_ERR(cal_filp) == -ENOENT || PTR_ERR(cal_filp) == -ENXIO) {
		pr_info("[FACTORY] %s : no light_factory_cal file\n", __func__);
		set_fs(old_fs);
		light_set_cal_data(init_data, true);
	} else if (IS_ERR(cal_filp)) {
		pr_err("[FACTORY]: %s - filp_open error\n", __func__);
		set_fs(old_fs);
	} else {
		ret = vfs_read(cal_filp, (char *)init_data,
			sizeof(int32_t) * 2, &cal_filp->f_pos);
		if (ret < 0) {
			pr_err("[FACTORY] %s: fd read fail:%d\n", __func__, ret);
			filp_close(cal_filp, current->files);
			set_fs(old_fs);
			return;
		}

		pr_info("[FACTORY] %s : already exist %d, %d\n",
			__func__, init_data[0], init_data[1]);
		msg_buf[0] = OPTION_TYPE_SET_LIGHT_CAL;
		data->light_cal = msg_buf[1] = init_data[1];
#if 0
		mutex_lock(&data->light_factory_mutex);
		if ((init_data[0] >= LIGHT_CAL_PASS) &&
			(init_data[1] >= 0) && ub_check)
			adsp_unicast(msg_buf, sizeof(msg_buf),
				MSG_LIGHT, 0, MSG_TYPE_OPTION_DEFINE);
		mutex_unlock(&data->light_factory_mutex);
#endif
		filp_close(cal_filp, current->files);
		set_fs(old_fs);
	}
#else
	pr_info("[FACTORY] %s : not supported light cal\n", __func__);
#endif
#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
	adsp_unicast(rect_msg, sizeof(rect_msg),
		MSG_SSC_CORE, 0, MSG_TYPE_OPTION_DEFINE);
#endif
}

static ssize_t light_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t msg_buf[2], cnt = 0, cmd = OPTION_TYPE_GET_LIGHT_CAL;
	int32_t cal_data[2] = {LIGHT_CAL_FAIL, -1}, ret, cur_lux;
	uint16_t light_idx = get_light_sidx(data);

	ret = light_get_cal_data(cal_data);

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA]
		& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1100);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		cur_lux = -1;
	} else {
		cur_lux = data->msg_buf[light_idx][2];
	}

	msg_buf[0] = OPTION_TYPE_SET_LIGHT_CAL;
	msg_buf[1] = cal_data[1];

	if ((ret >= 0) && (cal_data[0] == LIGHT_CAL_PASS) && (cal_data[1] >= 0))
		adsp_unicast(msg_buf, sizeof(msg_buf),
			light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	pr_info("[FACTORY] %s: cal_data %d, %d, %d\n", __func__,
		cal_data[0], cal_data[1], cur_lux);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		cal_data[0], cal_data[1], cur_lux);
}

static ssize_t light_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_LIGHT_CAL, new_value;
	int32_t cal_data[2] = {LIGHT_CAL_FAIL, -1};
	int32_t msg_buf[2] = {0, 0};
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;
	char *cur_id_str;

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else if (sysfs_streq(buf, "1"))
		new_value = 1;
	else
		return size;

	pr_info("[FACTORY] %s: cmd: %d\n", __func__, new_value);
	cur_id_str = kzalloc(UB_CELL_ID_LENGTH, GFP_KERNEL);

	if (new_value == 1) {
		mutex_lock(&data->light_factory_mutex);
		adsp_unicast(&cmd, sizeof(cmd), light_idx, 0,
			MSG_TYPE_GET_CAL_DATA);

		while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA]
			& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
			usleep_range(1000, 1100);

		data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);
		mutex_unlock(&data->light_factory_mutex);

		if (cnt >= TIMEOUT_CNT) {
			pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
			kfree(cur_id_str);
			return size;
		}

		pr_info("[FACTORY] %s: %d, %d\n", __func__,
			data->msg_buf[light_idx][0],
			data->msg_buf[light_idx][1]);
		cal_data[0] = data->msg_buf[light_idx][0];
		cal_data[1] = data->msg_buf[light_idx][1];

		msg_buf[0] = OPTION_TYPE_SET_LIGHT_CAL;
		if (cal_data[0] == LIGHT_CAL_PASS)
			data->light_cal = msg_buf[1] = cal_data[1];
		else
			msg_buf[1] = 0;
		light_get_ub_cell_id(cur_id_str);
	} else {
		cal_data[0] = LIGHT_CAL_FAIL;
		cal_data[1] = 0;

		msg_buf[0] = OPTION_TYPE_SET_LIGHT_CAL;
		msg_buf[1] = 0;
	}

	light_set_cal_data(cal_data, false);
	light_set_ub_cell_id(cur_id_str, false);
	kfree(cur_id_str);

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

static ssize_t light_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cnt = 0, cmd = OPTION_TYPE_GET_LIGHT_CAL;
	int32_t test_value;
	uint16_t light_idx = get_light_sidx(data);

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA]
		& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1100);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		test_value = -1;
	} else {
		test_value = data->msg_buf[light_idx][1];
	}

	mutex_unlock(&data->light_factory_mutex);

	pr_info("[FACTORY] %s: test_data %d, %d\n", __func__,
		data->light_cal, test_value);

	return snprintf(buf, PAGE_SIZE, "%d, %d\n",data->light_cal, test_value);
}

static DEVICE_ATTR(read_copr, 0664, light_read_copr_show, light_read_copr_store);
static DEVICE_ATTR(test_copr, 0444, light_test_copr_show, NULL);
static DEVICE_ATTR(boled_enable, 0220, NULL, light_boled_enable_store);
static DEVICE_ATTR(copr_roix, 0444, light_copr_roix_show, NULL);
static DEVICE_ATTR(sensorhub_ddi_spi_check, 0444, light_ddi_spi_check_show, NULL);
static DEVICE_ATTR(register_write, 0220, NULL, light_register_write_store);
static DEVICE_ATTR(register_read, 0664,
		light_register_read_show, light_register_read_store);
static DEVICE_ATTR(light_cal, 0664, light_cal_show, light_cal_store);
static DEVICE_ATTR(light_test, 0444, light_test_show, NULL);
#endif

static DEVICE_ATTR(vendor, 0444, light_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, light_name_show, NULL);
static DEVICE_ATTR(lux, 0444, light_raw_data_show, NULL);
static DEVICE_ATTR(raw_data, 0444, light_raw_data_show, NULL);
static DEVICE_ATTR(dhr_sensor_info, 0444, light_get_dhr_sensor_info_show, NULL);
#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
static DEVICE_ATTR(hallic_info, 0220, NULL, light_hallic_info_store);
static DEVICE_ATTR(light_circle, 0444, light_circle_show, NULL);
#endif
#if defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
static DEVICE_ATTR(brightness, 0220, NULL, light_brightness_store);
#endif
#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
static DEVICE_ATTR(set_aod_rect, 0220, NULL, light_set_aod_rect_store);
#endif

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	&dev_attr_dhr_sensor_info,
#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
	&dev_attr_hallic_info,
	&dev_attr_light_circle,
#endif
#ifdef CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR
	&dev_attr_read_copr,
	&dev_attr_test_copr,
	&dev_attr_boled_enable,
	&dev_attr_copr_roix,
	&dev_attr_register_write,
	&dev_attr_register_read,
	&dev_attr_sensorhub_ddi_spi_check,
	&dev_attr_light_cal,
	&dev_attr_light_test,
#endif
#if defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
	&dev_attr_brightness,
#endif
#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
	&dev_attr_set_aod_rect,
#endif
	NULL,
};

static int __init tmd490x_light_factory_init(void)
{
	adsp_factory_register(MSG_LIGHT, light_attrs);
#if defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
	return 0;
#endif

#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_COMPENSATION_LUX)
        panel_notifier_register(&light_panel_data_notifier);
#endif
	pr_info("[FACTORY] %s\n", __func__);

	return 0;
}

static void __exit tmd490x_light_factory_exit(void)
{
	adsp_factory_unregister(MSG_LIGHT);
#if defined(CONFIG_SUPPORT_BRIGHT_SYSFS_COMPENSATION_LUX)
	return;
#endif

#if defined(CONFIG_SUPPORT_BHL_COMPENSATION_FOR_LIGHT_SENSOR) || \
	defined(CONFIG_SUPPORT_BRIGHT_COMPENSATION_LUX)
        panel_notifier_unregister(&light_panel_data_notifier);
#endif
	pr_info("[FACTORY] %s\n", __func__);
}
module_init(tmd490x_light_factory_init);
module_exit(tmd490x_light_factory_exit);
