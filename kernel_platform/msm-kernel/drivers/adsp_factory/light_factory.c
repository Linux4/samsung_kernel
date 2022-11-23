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

#define UNKNOWN_INDEX      0
#define DEVICE_LIST_NUM    10
#define DEVICE_INFO_LENGTH 10

struct device_id_t {
	uint8_t device_id;
	char device_vendor[DEVICE_INFO_LENGTH];
	char device_name[DEVICE_INFO_LENGTH];
};

static const struct device_id_t device_list[DEVICE_LIST_NUM] = {
	/* ID, Vendor,      Name */
	{0x00, "Unknown",   "Unknown"},
	{0x18, "AMS", "TCS3701"},
	{0x61, "SensorTek", "STK33911"},
	{0x62, "SensorTek", "STK33917"},
	{0x63, "SensorTek", "STK33910"},
	{0x65, "SensorTek", "STK33915"},
	{0x66, "SensorTek", "STK31610"},
	{0x70, "Capella", "VEML3235"},
	{0x71, "Capella", "VEML3328"},
};

#define ASCII_TO_DEC(x) (x - 48)
int brightness;

enum {
	OPTION_TYPE_COPR_ENABLE,
	OPTION_TYPE_BOLED_ENABLE,
	OPTION_TYPE_LCD_ONOFF,
	OPTION_TYPE_GET_COPR,
	OPTION_TYPE_GET_DDI_DEVICE_ID,
	OPTION_TYPE_SET_HALLIC_INFO,
	OPTION_TYPE_GET_LIGHT_CAL,
	OPTION_TYPE_SET_LIGHT_CAL,
	OPTION_TYPE_SET_LCD_VERSION,
	OPTION_TYPE_SET_UB_DISCONNECT,
	OPTION_TYPE_GET_LIGHT_DEBUG_INFO,
	OPTION_TYPE_SET_DEVICE_MODE,
	OPTION_TYPE_SET_PANEL_STATE,
	OPTION_TYPE_SET_PANEL_TEST_STATE,
	OPTION_TYPE_SET_AUTO_BRIGHTNESS_HYST,
	OPTION_TYPE_SET_PANEL_SCREEN_MODE,
	OPTION_TYPE_GET_LIGHT_CIRCLE_COORDINATES,
	OPTION_TYPE_SAVE_LIGHT_CAL,
	OPTION_TYPE_LOAD_LIGHT_CAL,
	OPTION_TYPE_GET_LIGHT_DEVICE_ID,
	OPTION_TYPE_MAX
};

#if IS_ENABLED(CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR) && IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
#include "../sec_panel_notifier/sec_panel_notifier.h"
#endif

#define LIGHT_CAL_PASS 1
#define LIGHT_CAL_FAIL 0

/*************************************************************************/
/* factory Sysfs							 */
/*************************************************************************/

int get_light_sidx(struct adsp_data *data)
{
	int ret = MSG_LIGHT;
#if IS_ENABLED(CONFIG_SUPPORT_DUAL_OPTIC)
	switch (data->fac_fstate) {
	case FSTATE_INACTIVE:
	case FSTATE_FAC_INACTIVE:
		ret = MSG_LIGHT;
		break;
	case FSTATE_ACTIVE:
	case FSTATE_FAC_ACTIVE:
	case FSTATE_FAC_INACTIVE_2:
		ret = MSG_LIGHT_SUB;
		break;
	default:
		break;
	}
#endif
	return ret;
}

int get_light_display_sidx(int32_t idx)
{
	int ret = MSG_LIGHT;
#if IS_ENABLED(CONFIG_SUPPORT_DUAL_OPTIC)
	ret = idx == 0 ? MSG_LIGHT : MSG_LIGHT_SUB;
#endif
	return ret;
}

static void light_get_device_id(struct adsp_data *data)
{
	int32_t cmd = OPTION_TYPE_GET_LIGHT_DEVICE_ID, i;
	int32_t device_index = UNKNOWN_INDEX;
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0, device_id = 0;

	mutex_lock(&data->light_factory_mutex);

	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA]
		& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1100);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);
	mutex_unlock(&data->light_factory_mutex);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
	else
		device_id = (uint8_t)data->msg_buf[light_idx][0];
	pr_err("[SSC_FAC] %s: device_id : %d\n", __func__, device_id);

	if (device_id == 0) {
		pr_err("[SSC_FAC] %s: No information\n", __func__);
	} else {
		for (i = 0; i < DEVICE_LIST_NUM; i++)
			if (device_id == device_list[i].device_id)
				break;
		if (i >= DEVICE_LIST_NUM)
			pr_err("[SSC_FAC] %s: Unknown ID - (0x%x)\n",
				__func__, device_id);
		else
			device_index = i;
	}

	memcpy(data->light_device_vendor,
		device_list[device_index].device_vendor,
		sizeof(char) * DEVICE_INFO_LENGTH);
	memcpy(data->light_device_name,
		device_list[device_index].device_name,
		sizeof(char) * DEVICE_INFO_LENGTH);

	pr_info("[SSC_FAC] %s: Device ID - %s(%s)\n", __func__,
		data->light_device_name, data->light_device_vendor);
}

static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	if(data->light_device_vendor[0] == 0) {
		light_get_device_id(data);
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", data->light_device_vendor);
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	if(data->light_device_name[0] == 0) {
		light_get_device_id(data);
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", data->light_device_name);
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
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
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

	pr_info("[SSC_FAC] %s: start\n", __func__);
	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(NULL, 0, light_idx, 0, MSG_TYPE_GET_DHR_INFO);
	while (!(data->ready_flag[MSG_TYPE_GET_DHR_INFO] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_DHR_INFO] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);

	mutex_unlock(&data->light_factory_mutex);
	return data->msg_buf[light_idx][0];
}

static ssize_t light_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("[SSC_FAC] %s: %d\n", __func__, brightness);
	return snprintf(buf, PAGE_SIZE, "%d\n", brightness);
}

static ssize_t light_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);

	brightness = ASCII_TO_DEC(buf[0]) * 100 + ASCII_TO_DEC(buf[1]) * 10 + ASCII_TO_DEC(buf[2]);
	pr_info("[SSC_FAC]%s: %d\n", __func__, brightness);

	adsp_unicast(&brightness, sizeof(brightness), light_idx, 0, MSG_TYPE_SET_CAL_DATA);

	pr_info("[SSC_FAC]%s: done\n", __func__);

	return size;
}

static ssize_t light_register_read_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int cnt = 0;
	int32_t msg_buf[1];

	msg_buf[0] = data->light_temp_reg;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_GET_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_GET_REGISTER] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_REGISTER] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);

	pr_info("[SSC_FAC] %s: [0x%x]: 0x%x\n",
		__func__, msg_buf[0], data->msg_buf[light_idx][0]);

	mutex_unlock(&data->light_factory_mutex);

	return snprintf(buf, PAGE_SIZE, "[0x%x]: 0x%x\n",
		msg_buf[0], data->msg_buf[light_idx][0]);
}

static ssize_t light_register_read_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int reg = 0;
	struct adsp_data *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%3x", &reg) != 1) {
		pr_err("[SSC_FAC]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	data->light_temp_reg = reg;
	pr_info("[SSC_FAC] %s: [0x%x]\n", __func__, data->light_temp_reg);

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
		pr_err("[SSC_FAC]: %s - The number of data are wrong\n",
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
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);

	data->msg_buf[light_idx][MSG_LIGHT_MAX - 1] = msg_buf[0];
	pr_info("[SSC_FAC] %s: 0x%x - 0x%x\n",
		__func__, msg_buf[0], data->msg_buf[light_idx][0]);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

static ssize_t light_hyst_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);

	pr_info("[SSC_FAC] %s: %d,%d,%d,%d\n", __func__,
		data->hyst[0], data->hyst[1], data->hyst[2], data->hyst[3]);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d\n",
		data->hyst[0], data->hyst[1], data->hyst[2], data->hyst[3]);
}

static ssize_t light_hyst_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int32_t msg_buf[5];

	if (sscanf(buf, "%11d,%11d,%11d,%11d", &data->hyst[0], &data->hyst[1],
		&data->hyst[2], &data->hyst[3]) != 4) {
		pr_err("[SSC_FAC]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	pr_info("[SSC_FAC] %s: (%d) %d < %d < %d\n", __func__,
		data->hyst[0], data->hyst[1], data->hyst[2], data->hyst[3]);

	msg_buf[0] = OPTION_TYPE_SET_AUTO_BRIGHTNESS_HYST;
	msg_buf[1] = data->hyst[0];
	msg_buf[2] = data->hyst[1];
	msg_buf[3] = data->hyst[2];
	msg_buf[4] = data->hyst[3];

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

#if IS_ENABLED(CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR) && IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
void light_brightness_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct work_struct *)work,
		struct adsp_data, light_br_work);
	uint16_t light_idx = get_light_display_sidx(data->brightness_info[2]);
	int cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(data->brightness_info, sizeof(data->brightness_info),
		light_idx, 0, MSG_TYPE_SET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_SET_CAL_DATA] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_SET_CAL_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[SSC_FAC] %s: Timeout!!! br: %d\n", __func__,
			data->brightness_info[0]);

	mutex_unlock(&data->light_factory_mutex);
}

int light_panel_data_notify(struct notifier_block *nb,
	unsigned long val, void *v)
{
	struct adsp_data *data = adsp_get_struct_data();
	static int32_t pre_ub_con_state = -1;
#if IS_ENABLED(CONFIG_SUPPORT_PANEL_STATE_NOTIFY_FOR_LIGHT_SENSOR)
	static int32_t pre_finger_mask_hbm_on = -1;
	static int32_t pre_test_state = -1;
	static int32_t pre_acl_status = -1;
#endif
	uint16_t light_idx = get_light_sidx(data);

	if (val == PANEL_EVENT_BL_CHANGED) {
		struct panel_bl_event_data *panel_data = v;

		if (panel_data->display_idx > 1)
			return 0;

		data->brightness_info[0] = panel_data->bl_level;
		data->brightness_info[1] = panel_data->aor_data;
#if IS_ENABLED(CONFIG_SUPPORT_PANEL_STATE_NOTIFY_FOR_LIGHT_SENSOR)
		data->brightness_info[2] = panel_data->display_idx;
		data->brightness_info[3] = panel_data->finger_mask_hbm_on;
		data->brightness_info[4] = panel_data->acl_status;

		if ((data->brightness_info[0] == data->pre_bl_level) &&
			(data->brightness_info[2] == data->pre_display_idx) &&
			(data->brightness_info[3] == pre_finger_mask_hbm_on) &&
			(data->brightness_info[4] == pre_acl_status))
			return 0;

		pre_finger_mask_hbm_on = data->brightness_info[3];
		pre_acl_status = data->brightness_info[4];
#else
		if (data->brightness_info[0] == data->pre_bl_level)
			return 0;
#endif
		data->pre_bl_level = data->brightness_info[0];
		data->pre_display_idx = data->brightness_info[2];

		schedule_work(&data->light_br_work);
	} else if (val == PANEL_EVENT_UB_CON_CHANGED) {
		struct panel_ub_con_event_data *panel_data = v;
		int32_t msg_buf[2];

		if ((int32_t)panel_data->state == pre_ub_con_state)
			return 0;

		pre_ub_con_state = (int32_t)panel_data->state;
		msg_buf[0] = OPTION_TYPE_SET_UB_DISCONNECT;
		msg_buf[1] = (int32_t)panel_data->state;

		mutex_lock(&data->light_factory_mutex);
		pr_info("[SSC_FAC] %s: ub disconnected %d\n",
			__func__, msg_buf[1]);
		adsp_unicast(msg_buf, sizeof(msg_buf),
			light_idx, 0, MSG_TYPE_OPTION_DEFINE);
		mutex_unlock(&data->light_factory_mutex);
#if IS_ENABLED(CONFIG_SUPPORT_PANEL_STATE_NOTIFY_FOR_LIGHT_SENSOR)
	} else if (val == PANEL_EVENT_STATE_CHANGED) {
		struct panel_state_data *evdata = (struct panel_state_data *)v;
		int32_t panel_state = (int32_t)evdata->state;
		int32_t msg_buf[4];

		if ((evdata->display_idx > 1) ||
			(panel_state >= MAX_PANEL_STATE) ||
			((data->pre_panel_state == panel_state) &&
			(data->pre_panel_idx == evdata->display_idx)))
			return 0;

		data->brightness_info[5] = data->pre_panel_state = panel_state;
		data->pre_panel_idx = evdata->display_idx;
		msg_buf[0] = OPTION_TYPE_SET_PANEL_STATE;
		msg_buf[1] = panel_state;
		msg_buf[2] = evdata->display_idx;
		msg_buf[3] = data->pre_screen_mode;

		light_idx = get_light_display_sidx(evdata->display_idx);

		mutex_lock(&data->light_factory_mutex);
		pr_info("[SSC_FAC] %s: panel_state %d(inx: %d, mode: %d)\n",
			__func__, (int)evdata->state, evdata->display_idx,
			data->pre_screen_mode);
		adsp_unicast(msg_buf, sizeof(msg_buf),
			light_idx, 0, MSG_TYPE_OPTION_DEFINE);
		mutex_unlock(&data->light_factory_mutex);
	} else if (val == PANEL_EVENT_TEST_MODE_CHANGED) {
		struct panel_test_mode_data *test_data = v;
		int32_t msg_buf[2];

		if ((test_data->display_idx > 1) ||
			(pre_test_state == (int32_t)test_data->state))
			return 0;

		pre_test_state = (int32_t)test_data->state;
		msg_buf[0] = OPTION_TYPE_SET_PANEL_TEST_STATE;
		msg_buf[1] = (int32_t)test_data->state;

		light_idx = get_light_display_sidx(test_data->display_idx);

		mutex_lock(&data->light_factory_mutex);
		pr_info("[SSC_FAC] %s: panel test state %d\n",
			__func__, (int)test_data->state);

		adsp_unicast(msg_buf, sizeof(msg_buf),
			light_idx, 0, MSG_TYPE_OPTION_DEFINE);
		mutex_unlock(&data->light_factory_mutex);
	} else if (val == PANEL_EVENT_SCREEN_MODE_CHANGED) {
		struct panel_screen_mode_data *screen_data = v;
		int32_t msg_buf[3];

		if (data->pre_screen_mode == (int32_t)screen_data->mode)
			return 0;

		data->pre_screen_mode = (int32_t)screen_data->mode;
		msg_buf[0] = OPTION_TYPE_SET_PANEL_SCREEN_MODE;
		msg_buf[1] = (int32_t)screen_data->mode;
		msg_buf[2] = (int32_t)screen_data->display_idx;

		light_idx = get_light_display_sidx(screen_data->display_idx);

		mutex_lock(&data->light_factory_mutex);
		pr_info("[SSC_FAC] %s: panel screen mode %d %d\n",
			__func__, screen_data->mode, screen_data->display_idx);

		adsp_unicast(msg_buf, sizeof(msg_buf),
			light_idx, 0, MSG_TYPE_OPTION_DEFINE);
		mutex_unlock(&data->light_factory_mutex);
#endif /* CONFIG_SUPPORT_PANEL_STATE_NOTIFY_FOR_LIGHT_SENSOR */
	}

	return 0;
}

static struct notifier_block light_panel_data_notifier = {
	.notifier_call = light_panel_data_notify,
	.priority = 1,
};
#endif /* CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR */

static ssize_t light_hallic_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
#ifndef CONFIG_SEC_FACTORY
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int32_t msg_buf[2];
#endif
	int new_value;

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else if (sysfs_streq(buf, "1"))
		new_value = 1;
	else
		return size;

	pr_info("[SSC_FAC] %s: new_value %d\n", __func__, new_value);

#ifndef CONFIG_SEC_FACTORY
	msg_buf[0] = OPTION_TYPE_SET_HALLIC_INFO;
	msg_buf[1] = new_value;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);
#endif
	return size;
}

static ssize_t light_lcd_onoff_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int32_t msg_buf[3];
	int new_value, cnt = 0;

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else if (sysfs_streq(buf, "1"))
		new_value = 1;
	else
		return size;

	pr_info("[SSC_FAC] %s: new_value %d\n", __func__, new_value);
	data->pre_bl_level = -1;
	msg_buf[0] = OPTION_TYPE_LCD_ONOFF;
	msg_buf[1] = new_value;
	msg_buf[2] = data->brightness_info[5];

	if (new_value == 1) {
#if IS_ENABLED(CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR)
		schedule_delayed_work(&data->light_copr_debug_work,
			msecs_to_jiffies(1000));
		data->light_copr_debug_count = 0;
#endif
#if IS_ENABLED(CONFIG_SUPPORT_FIFO_DEBUG_FOR_LIGHT_SENSOR)
		schedule_delayed_work(&data->light_fifo_debug_work,
			msecs_to_jiffies(10 * 1000));
#endif
	} else {
#if IS_ENABLED(CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR)
		cancel_delayed_work_sync(&data->light_copr_debug_work);
		data->light_copr_debug_count = 5;
#endif
#if IS_ENABLED(CONFIG_SUPPORT_FIFO_DEBUG_FOR_LIGHT_SENSOR)
		cancel_delayed_work_sync(&data->light_fifo_debug_work);
#endif
	}
	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	while (!(data->ready_flag[MSG_TYPE_OPTION_DEFINE] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);
	data->ready_flag[MSG_TYPE_OPTION_DEFINE] &= ~(1 << light_idx);
	if (cnt >= TIMEOUT_CNT)
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		MSG_SSC_CORE, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

static ssize_t light_circle_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_LIGHT_CIRCLE_COORDINATES;
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA]
		& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1100);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);
	mutex_unlock(&data->light_factory_mutex);

#if IS_ENABLED(CONFIG_SUPPORT_DUAL_OPTIC)
	if (cnt >= TIMEOUT_CNT)
		return snprintf(buf, PAGE_SIZE, "0 0 0 0 0 0\n");

	return snprintf(buf, PAGE_SIZE, "%d.%d %d.%d %d.%d %d.%d %d.%d %d.%d\n",
		data->msg_buf[light_idx][0] / 10,
		data->msg_buf[light_idx][0] % 10,
		data->msg_buf[light_idx][1] / 10,
		data->msg_buf[light_idx][1] % 10,
		data->msg_buf[light_idx][4] / 10,
		data->msg_buf[light_idx][4] % 10,
		data->msg_buf[light_idx][2] / 10,
		data->msg_buf[light_idx][2] % 10,
		data->msg_buf[light_idx][3] / 10,
		data->msg_buf[light_idx][3] % 10,
		data->msg_buf[light_idx][4] / 10,
		data->msg_buf[light_idx][4] % 10);
#else
	if (cnt >= TIMEOUT_CNT)
		return snprintf(buf, PAGE_SIZE, "0 0 0\n");
	
	return snprintf(buf, PAGE_SIZE, "%d.%d %d.%d %d.%d\n",
		data->msg_buf[light_idx][0] / 10,
		data->msg_buf[light_idx][0] % 10,
		data->msg_buf[light_idx][1] / 10,
		data->msg_buf[light_idx][1] % 10,
		data->msg_buf[light_idx][2] / 10,
		data->msg_buf[light_idx][2] % 10);
#endif
}

#if IS_ENABLED(CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR)
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

	pr_info("[SSC_FAC] %s: new_value %d\n", __func__, new_value);
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
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	pr_info("[SSC_FAC] %s: %d\n", __func__, data->msg_buf[light_idx][4]);
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

	while (!(data->ready_flag[MSG_TYPE_GET_DUMP_REGISTER] & 1 << light_idx)
		&& cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_DUMP_REGISTER] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "-1,-1,-1,-1\n");
	}

	pr_info("[SSC_FAC] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
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
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "-1,-1,-1,-1\n");
	}

	pr_info("[SSC_FAC] %s: %d,%d,%d,%d\n", __func__,
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
	int32_t cmd = OPTION_TYPE_GET_DDI_DEVICE_ID;
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return snprintf(buf, PAGE_SIZE, "-1\n");
	}

	pr_info("[SSC_FAC] %s: %d\n", __func__, data->msg_buf[light_idx][0]);

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

	pr_info("[SSC_FAC] %s: new_value %d\n", __func__, new_value);
	msg_buf[0] = OPTION_TYPE_BOLED_ENABLE;
	msg_buf[1] = new_value;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

void light_copr_debug_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct delayed_work *)work,
		struct adsp_data, light_copr_debug_work);
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

#if IS_ENABLED(CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR) && IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
	if (data->brightness_info[5] == 0)
		return;
#endif
	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(NULL, 0, light_idx, 0, MSG_TYPE_GET_DUMP_REGISTER);

	while (!(data->ready_flag[MSG_TYPE_GET_DUMP_REGISTER] & 1 << light_idx)
		&& cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_DUMP_REGISTER] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return;
	}

	pr_info("[SSC_FAC] %s: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", __func__,
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2], data->msg_buf[light_idx][3],
		data->msg_buf[light_idx][4], data->msg_buf[light_idx][5],
		data->msg_buf[light_idx][6], data->msg_buf[light_idx][7],
		data->msg_buf[light_idx][8], data->msg_buf[light_idx][9],
		data->msg_buf[light_idx][10], data->msg_buf[light_idx][11]);

	mutex_unlock(&data->light_factory_mutex);

	if (data->light_copr_debug_count++ < 5)
		schedule_delayed_work(&data->light_copr_debug_work,
			msecs_to_jiffies(1000));
}
#endif /* CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR */
#if IS_ENABLED(CONFIG_SUPPORT_FIFO_DEBUG_FOR_LIGHT_SENSOR)
void light_fifo_debug_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct delayed_work *)work,
		struct adsp_data, light_fifo_debug_work);
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(NULL, 0, light_idx, 0, MSG_TYPE_SET_THRESHOLD);

	while (!(data->ready_flag[MSG_TYPE_SET_THRESHOLD] & 1 << light_idx)
		&& cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_SET_THRESHOLD] &= ~(1 << light_idx);
	mutex_unlock(&data->light_factory_mutex);

	schedule_delayed_work(&data->light_fifo_debug_work,
		msecs_to_jiffies(2 * 60 * 1000));
}
#endif

static ssize_t light_debug_info_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int reg = 0;

	if (sscanf(buf, "%3d", &reg) != 1) {
		pr_err("[SSC_FAC]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	data->light_debug_info_cmd = reg;

	return size;
}

static ssize_t light_debug_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd[2];
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	cmd[0] = OPTION_TYPE_GET_LIGHT_DEBUG_INFO;
	cmd[1] = data->light_debug_info_cmd;
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA] & 1 << light_idx) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);
	mutex_unlock(&data->light_factory_mutex);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[FACTORY] %s: Timeout!!!\n", __func__);
		return snprintf(buf, PAGE_SIZE, "0,0,0,0,0,0\n");
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d,%d,%d\n",
		data->msg_buf[light_idx][0], data->msg_buf[light_idx][1],
		data->msg_buf[light_idx][2], data->msg_buf[light_idx][3],
		data->msg_buf[light_idx][4] >> 16,
		data->msg_buf[light_idx][4] & 0xffff);
}

#if IS_ENABLED(CONFIG_SUPPORT_SSC_AOD_RECT)
static ssize_t light_set_aod_rect_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int32_t msg_buf[5] = {OPTION_TYPE_SSC_AOD_RECT, 0, 0, 0, 0};

	if (sscanf(buf, "%3d,%3d,%3d,%3d",
		&msg_buf[1], &msg_buf[2], &msg_buf[3], &msg_buf[4]) != 4) {
		pr_err("[SSC_FAC]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	pr_info("[SSC_FAC] %s: rect:%d,%d,%d,%d\n", __func__,
		msg_buf[1], msg_buf[2], msg_buf[3], msg_buf[4]);
	adsp_unicast(msg_buf, sizeof(msg_buf),
			MSG_SSC_CORE, 0, MSG_TYPE_OPTION_DEFINE);
	return size;
}

void light_rect_init_work(void)
{
	int32_t rect_msg[5] = {OPTION_TYPE_SSC_AOD_LIGHT_CIRCLE, 546, 170, 576, 200};

	adsp_unicast(rect_msg, sizeof(rect_msg),
		MSG_SSC_CORE, 0, MSG_TYPE_OPTION_DEFINE);
}
#endif

void light_init_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct delayed_work *)work,
		struct adsp_data, light_init_work);

	light_get_device_id(data);
}

void light_init_work(struct adsp_data *data)
{
	data->pre_bl_level = -1;
	data->pre_panel_state = -1;
	data->pre_panel_idx = -1;
	data->pre_display_idx = -1;
	data->light_debug_info_cmd = 0;

	schedule_delayed_work(&data->light_init_work, msecs_to_jiffies(1000));
}

#if IS_ENABLED(CONFIG_SUPPORT_LIGHT_CALIBRATION)
void light_cal_read_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct delayed_work *)work,
		struct adsp_data, light_cal_work);
	uint16_t light_idx = get_light_sidx(data);
	int32_t msg_buf[5] = {0, }, cmd, cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	cmd = OPTION_TYPE_LOAD_LIGHT_CAL;
	adsp_unicast(&cmd, sizeof(int32_t),
		light_idx, 0, MSG_TYPE_SET_TEMPORARY_MSG);

	while (!(data->ready_flag[MSG_TYPE_SET_TEMPORARY_MSG]
		& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1100);

	data->ready_flag[MSG_TYPE_SET_TEMPORARY_MSG] &= ~(1 << light_idx);
	mutex_unlock(&data->light_factory_mutex);
	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		return;
	} else if (data->msg_buf[light_idx][0] < 0) {
		pr_err("[SSC_FAC] %s: UB is not matched!!!(%d)\n", __func__,
			data->msg_buf[light_idx][0]);
#if IS_ENABLED(CONFIG_SUPPORT_PROX_CALIBRATION)
		prox_send_cal_data(data, false);
#endif
		return;
	}

	msg_buf[0] = OPTION_TYPE_SET_LIGHT_CAL;
	msg_buf[1] = data->light_cal_result = data->msg_buf[light_idx][0];
	msg_buf[2] = data->light_cal1 = data->msg_buf[light_idx][1];
	msg_buf[3] = data->light_cal2 = data->msg_buf[light_idx][2];
	msg_buf[4] = data->copr_w = data->msg_buf[light_idx][3];

#if IS_ENABLED(CONFIG_SUPPORT_PROX_CALIBRATION)
	data->prox_cal = data->msg_buf[light_idx][4];
	prox_send_cal_data(data, true);
#endif

	if (data->light_cal_result == LIGHT_CAL_PASS) {
		mutex_lock(&data->light_factory_mutex);
		adsp_unicast(msg_buf, sizeof(msg_buf),
			light_idx, 0, MSG_TYPE_OPTION_DEFINE);
		mutex_unlock(&data->light_factory_mutex);
	}
}

void light_cal_init_work(struct adsp_data *data)
{
	data->light_cal_result = LIGHT_CAL_FAIL;
	data->light_cal1 = -1;
	data->light_cal2 = -1;
	data->copr_w = -1;

	schedule_delayed_work(&data->light_cal_work, msecs_to_jiffies(8000));
}

static ssize_t light_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_LIGHT_CAL, cur_lux, cnt = 0;
	uint16_t light_idx = get_light_sidx(data);

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(int32_t), light_idx,
		0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA]
		& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1100);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	mutex_unlock(&data->light_factory_mutex);
	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		cur_lux = -1;
	} else {
		cur_lux = data->msg_buf[light_idx][4];
	}

	pr_info("[SSC_FAC] %s: cal_data (P/F: %d, Cal1: %d, Cal2: %d, COPR_W: %d, cur lux: %d)\n",
		__func__, data->light_cal_result, data->light_cal1,
		data->light_cal2, data->copr_w, cur_lux);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		data->light_cal_result, data->light_cal2, cur_lux);
}

static ssize_t light_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_LIGHT_CAL, new_value, cnt = 0;
	int32_t msg_buf[5] = {0, };
	uint16_t light_idx = get_light_sidx(data);

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else if (sysfs_streq(buf, "1"))
		new_value = 1;
	else
		return size;

	pr_info("[SSC_FAC] %s: cmd: %d\n", __func__, new_value);
	mutex_lock(&data->light_factory_mutex);

	if (new_value == 1) {
		adsp_unicast(&cmd, sizeof(cmd), light_idx, 0,
			MSG_TYPE_GET_CAL_DATA);

		while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA]
			& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
			usleep_range(1000, 1100);

		data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

		if (cnt >= TIMEOUT_CNT) {
			pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
			mutex_unlock(&data->light_factory_mutex);
			return size;
		}

		pr_info("[SSC_FAC] %s: (P/F: %d, Cal1: %d, Cal2: %d, COPR_W: %d)\n",
			__func__, data->msg_buf[light_idx][0],
			data->msg_buf[light_idx][1],
			data->msg_buf[light_idx][2],
			data->msg_buf[light_idx][3]);

		if (data->msg_buf[light_idx][0] == LIGHT_CAL_PASS) {
			data->light_cal_result = data->msg_buf[light_idx][0];
			data->light_cal1 = data->msg_buf[light_idx][1];
			data->light_cal2 = data->msg_buf[light_idx][2];
			data->copr_w = data->msg_buf[light_idx][3];
		} else {
			mutex_unlock(&data->light_factory_mutex);
			return size;
		}
	} else {
		data->light_cal_result = LIGHT_CAL_FAIL;
		data->light_cal1 = 0;
		data->light_cal2 = 0;
		data->copr_w = 0;
	}

	cnt = 0;
	msg_buf[0] = OPTION_TYPE_SAVE_LIGHT_CAL;
	msg_buf[1] = data->light_cal_result;
	msg_buf[2] = data->light_cal1;
	msg_buf[3] = data->light_cal2;
	msg_buf[4] = data->copr_w;
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_SET_TEMPORARY_MSG);

	while (!(data->ready_flag[MSG_TYPE_SET_TEMPORARY_MSG]
		& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1100);

	data->ready_flag[MSG_TYPE_SET_TEMPORARY_MSG] &= ~(1 << light_idx);
	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		mutex_unlock(&data->light_factory_mutex);
		return size;
	}

	msg_buf[0] = OPTION_TYPE_SET_LIGHT_CAL;
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
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		test_value = -1;
	} else {
		test_value = data->msg_buf[light_idx][2];
	}

	mutex_unlock(&data->light_factory_mutex);

	pr_info("[SSC_FAC] %s: test_data (Cal1: %d, Cal2: %d, COPR_W: %d, 16ms lux: %d)\n",
		__func__, data->light_cal1, data->light_cal2,
		data->copr_w, test_value);

	return snprintf(buf, PAGE_SIZE, "%d, %d, %d, %d\n",
		data->light_cal1, data->light_cal2, data->copr_w, test_value);
}

static DEVICE_ATTR(light_test, 0444, light_test_show, NULL);
#elif defined (CONFIG_TABLET_MODEL_CONCEPT)
static ssize_t light_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_LIGHT_CAL, cur_lux, max_als = 0, cnt = 0;
	uint16_t light_idx = get_light_sidx(data);

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(int32_t), light_idx,
		0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA]
		& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1100);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	mutex_unlock(&data->light_factory_mutex);
	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		cur_lux = -1;
	} else {
		max_als = data->msg_buf[light_idx][1];
		cur_lux = data->msg_buf[light_idx][2];
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		1, max_als, cur_lux);
}

static ssize_t light_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("[SSC_FAC]%s: done\n", __func__);
	return size;
}
#endif

#if IS_ENABLED(CONFIG_SUPPORT_LIGHT_CALIBRATION) || IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
static DEVICE_ATTR(light_cal, 0664, light_cal_show, light_cal_store);
#endif
static DEVICE_ATTR(lcd_onoff, 0220, NULL, light_lcd_onoff_store);
static DEVICE_ATTR(hallic_info, 0220, NULL, light_hallic_info_store);
static DEVICE_ATTR(light_circle, 0444, light_circle_show, NULL);

#if IS_ENABLED(CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR)
static DEVICE_ATTR(read_copr, 0664,
		light_read_copr_show, light_read_copr_store);
static DEVICE_ATTR(test_copr, 0444, light_test_copr_show, NULL);
static DEVICE_ATTR(boled_enable, 0220, NULL, light_boled_enable_store);
static DEVICE_ATTR(copr_roix, 0444, light_copr_roix_show, NULL);
static DEVICE_ATTR(sensorhub_ddi_spi_check, 0444,
		light_ddi_spi_check_show, NULL);
#endif
static DEVICE_ATTR(register_write, 0220, NULL, light_register_write_store);
static DEVICE_ATTR(register_read, 0664,
		light_register_read_show, light_register_read_store);
static DEVICE_ATTR(vendor, 0444, light_vendor_show, NULL);
static DEVICE_ATTR(name, 0444, light_name_show, NULL);
static DEVICE_ATTR(lux, 0444, light_raw_data_show, NULL);
static DEVICE_ATTR(raw_data, 0444, light_raw_data_show, NULL);
static DEVICE_ATTR(dhr_sensor_info, 0444, light_get_dhr_sensor_info_show, NULL);
static DEVICE_ATTR(debug_info, 0664,
		light_debug_info_show, light_debug_info_store);
static DEVICE_ATTR(hyst, 0664, light_hyst_show, light_hyst_store);
static DEVICE_ATTR(brightness, 0664, light_brightness_show, light_brightness_store);
#if IS_ENABLED(CONFIG_SUPPORT_SSC_AOD_RECT)
static DEVICE_ATTR(set_aod_rect, 0220, NULL, light_set_aod_rect_store);
#endif

static struct device_attribute *light_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_lux,
	&dev_attr_raw_data,
	&dev_attr_dhr_sensor_info,
	&dev_attr_register_write,
	&dev_attr_register_read,
	&dev_attr_lcd_onoff,
	&dev_attr_hallic_info,
	&dev_attr_light_circle,
#if IS_ENABLED(CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR)
	&dev_attr_read_copr,
	&dev_attr_test_copr,
	&dev_attr_boled_enable,
	&dev_attr_copr_roix,
	&dev_attr_sensorhub_ddi_spi_check,
#endif
#if IS_ENABLED(CONFIG_SUPPORT_LIGHT_CALIBRATION) || IS_ENABLED(CONFIG_TABLET_MODEL_CONCEPT)
	&dev_attr_light_cal,
#endif
#if IS_ENABLED(CONFIG_SUPPORT_LIGHT_CALIBRATION)
	&dev_attr_light_test,
#endif
	&dev_attr_debug_info,
	&dev_attr_hyst,
	&dev_attr_brightness,
#if IS_ENABLED(CONFIG_SUPPORT_SSC_AOD_RECT)
	&dev_attr_set_aod_rect,
#endif
	NULL,
};

int light_factory_init(void)
{
	adsp_factory_register(MSG_LIGHT, light_attrs);
#if IS_ENABLED(CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR) && IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
	ss_panel_notifier_register(&light_panel_data_notifier);
#endif
	pr_info("[SSC_FAC] %s\n", __func__);

	return 0;
}

void light_factory_exit(void)
{
	adsp_factory_unregister(MSG_LIGHT);
#if IS_ENABLED(CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR) && IS_ENABLED(CONFIG_SEC_PANEL_NOTIFIER)
	ss_panel_notifier_unregister(&light_panel_data_notifier);
#endif
	pr_info("[SSC_FAC] %s\n", __func__);
}
