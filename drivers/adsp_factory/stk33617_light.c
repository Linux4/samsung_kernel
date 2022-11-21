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

#define VENDOR "Sensortek"
#define CHIP_ID "STK33617"

#define ASCII_TO_DEC(x) (x - 48)
#define BATT_TEMP_SYSFS_PATH "/sys/class/power_supply/battery/temp"

#define UB_CELL_ID_PATH "/sys/class/lcd/panel/window_type"
#define UB_CELL_ID_LENGTH 50

int brightness;
int batt_temp;
struct delayed_work batt_check_work;

enum {
	OPTION_TYPE_COPR_ENABLE,
	OPTION_TYPE_BOLED_ENABLE,
	OPTION_TYPE_LCD_ONOFF,
	OPTION_TYPE_GET_COPR,
	OPTION_TYPE_GET_CHIP_ID,
	OPTION_TYPE_SET_LCD_VERSION,
	OPTION_TYPE_MAX
};

void read_batt_temperature(void)
{
	struct file *batt_temp_filp = NULL;
	int ret = 0;
	mm_segment_t old_fs;
	char buf[4] = {0,};
	int temp = 0;
	int32_t msg_buf[1] = {0};

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	batt_temp_filp = filp_open(BATT_TEMP_SYSFS_PATH, O_RDONLY, 0440);
	if (IS_ERR(batt_temp_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(batt_temp_filp);
		pr_err("[FACTORY] %s: Can't open batt temp sysfs(%d)\n", __func__, ret);
		return;
	}  

	ret = vfs_read(batt_temp_filp, (char *)buf,
		4 * sizeof(char), &batt_temp_filp->f_pos);
	if (ret < 0) {
		pr_err("[FACTORY] %s: fd read fail:%d\n", __func__, ret);
		filp_close(batt_temp_filp, current->files);
		set_fs(old_fs);
		return;
	}

	filp_close(batt_temp_filp, current->files);
	set_fs(old_fs);

	sscanf(buf, "%4d", &temp);

	pr_info("[FACTORY] %s: battery temperature cur = %d, prev = %d\n", __func__, temp, batt_temp);

	if ((temp > 50 && batt_temp <= 50) || (temp <= 50 && batt_temp > 50)) {
		msg_buf[0] = temp;
		adsp_unicast(msg_buf, sizeof(msg_buf), MSG_PROX, 0, MSG_TYPE_SET_SETTINGS);
	}

	batt_temp = temp;
}

static void batt_check_work_func(struct work_struct *work)
{
	read_batt_temperature();

	schedule_delayed_work(&batt_check_work, msecs_to_jiffies(600000));
}

#ifdef CONFIG_SUPPORT_LIGHT_READ_UBID
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

void light_ub_read_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct delayed_work *)work,
		struct adsp_data, light_work);
	int32_t msg_buf[2];
	char *cur_id_str = kzalloc(UB_CELL_ID_LENGTH, GFP_KERNEL);
	int i;
	int val = 0;

	pr_info("[FACTORY] %s \n", __func__);
	light_get_ub_cell_id(cur_id_str);

	for (i = 0; i < 2; i++)
	{
	    val = val * 10 + cur_id_str[i] - '0';
	}
	kfree(cur_id_str);

	mutex_lock(&data->light_factory_mutex);
	msg_buf[0] = OPTION_TYPE_SET_LCD_VERSION;
	msg_buf[1] = val;
	adsp_unicast(msg_buf, sizeof(msg_buf),
		MSG_LIGHT, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	pr_info("[FACTORY] %s: UB ID: %d\n", __func__, val);
}

void light_ub_read_init_work(struct adsp_data *data)
{
	schedule_delayed_work(&data->light_work, msecs_to_jiffies(8000));
}
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
	return MSG_LIGHT;
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
		pr_err("[FACTORY] %s: Timeout\n", __func__);
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
		pr_err("[FACTORY] %s: Timeout\n", __func__);

	mutex_unlock(&data->light_factory_mutex);
	return data->msg_buf[light_idx][0];
}

static ssize_t light_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[FACTORY] %s: %d\n", __func__, brightness);
	return snprintf(buf, PAGE_SIZE, "%d\n", brightness);
}

static ssize_t light_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);

	brightness = ASCII_TO_DEC(buf[0]) * 100 + ASCII_TO_DEC(buf[1]) * 10 + ASCII_TO_DEC(buf[2]);
	pr_info("[FACTORY] %s: %d\n", __func__, brightness);

	adsp_unicast(&brightness, sizeof(brightness), light_idx, 0, MSG_TYPE_SET_CAL_DATA);

	pr_info("[FACTORY] %s: done\n", __func__);

	return size;
}

static ssize_t light_lcd_onoff_store(struct device *dev,
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
	msg_buf[0] = OPTION_TYPE_LCD_ONOFF;
	msg_buf[1] = new_value;

	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);

	if (new_value)
		read_batt_temperature();

	pr_info("[FACTORY] %s: done\n", __func__);

	return size;
}

#if defined(CONFIG_SEC_A71_PROJECT) 
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

static ssize_t light_circle_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SEC_A71_PROJECT)
	if (sec_hw_rev() >= 4)
		return snprintf(buf, PAGE_SIZE, "19.87 5.08 2.4\n");
	else
		return snprintf(buf, PAGE_SIZE, "53.20 2.41 1.8\n");
#elif defined(CONFIG_SEC_M51_PROJECT)
	return snprintf(buf, PAGE_SIZE, "23.05 6.08 2.8\n");
#else
	return snprintf(buf, PAGE_SIZE, "0 0 0\n");
#endif
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
		pr_err("[FACTORY] %s: Timeout\n", __func__);

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
		pr_err("[FACTORY] %s: Timeout\n", __func__);

	data->msg_buf[light_idx][MSG_LIGHT_MAX - 1] = msg_buf[0];
	pr_info("[FACTORY] %s: 0x%x - 0x%x\n",
		__func__, msg_buf[0], data->msg_buf[light_idx][0]);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

static DEVICE_ATTR(lcd_onoff, 0220, NULL, light_lcd_onoff_store);
static DEVICE_ATTR(light_circle, 0444, light_circle_show, NULL);
static DEVICE_ATTR(register_write, 0220, NULL, light_register_write_store);
static DEVICE_ATTR(register_read, 0664,
		light_register_read_show, light_register_read_store);
static DEVICE_ATTR(vendor, 0444, light_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, light_name_show, NULL);
static DEVICE_ATTR(lux, 0444, light_raw_data_show, NULL);
static DEVICE_ATTR(raw_data, 0444, light_raw_data_show, NULL);
static DEVICE_ATTR(dhr_sensor_info, 0444, light_get_dhr_sensor_info_show, NULL);
static DEVICE_ATTR(brightness, 0664, light_brightness_show, light_brightness_store);

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	&dev_attr_dhr_sensor_info,
	&dev_attr_brightness,
	&dev_attr_lcd_onoff,
	&dev_attr_light_circle,
	&dev_attr_register_write,
	&dev_attr_register_read,
	NULL,
};

static int __init stk33617_light_factory_init(void)
{
	adsp_factory_register(MSG_LIGHT, light_attrs);
	INIT_DELAYED_WORK(&batt_check_work, batt_check_work_func);
	schedule_delayed_work(&batt_check_work, msecs_to_jiffies(600000));

	pr_info("[FACTORY] %s\n", __func__);

	brightness = 0;
	batt_temp = 10000;

	return 0;
}

static void __exit stk33617_light_factory_exit(void)
{
	cancel_delayed_work_sync(&batt_check_work);
	adsp_factory_unregister(MSG_LIGHT);

	pr_info("[FACTORY] %s\n", __func__);
}
module_init(stk33617_light_factory_init);
module_exit(stk33617_light_factory_exit);
