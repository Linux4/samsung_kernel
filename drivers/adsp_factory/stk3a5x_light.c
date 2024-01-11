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
#define VENDOR "SensorTek"
#define CHIP_ID "STK33512"

enum {
	OPTION_TYPE_COPR_ENABLE,
	OPTION_TYPE_BOLED_ENABLE,
	OPTION_TYPE_LCD_ONOFF,
	OPTION_TYPE_GET_COPR,
	OPTION_TYPE_GET_CHIP_ID,
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
	OPTION_TYPE_MAX
};

#if defined(CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR) && defined(CONFIG_DISPLAY_SAMSUNG)
#include "../../../techpack/display/msm/samsung/ss_panel_notify.h"
#endif
#ifdef CONFIG_SUPPORT_LIGHT_CALIBRATION
int light_load_ub_cell_id_from_file(char *path, char *data_str);
int light_save_ub_cell_id_to_efs(char *data_str, bool first_booting);
#define LIGHT_FACTORY_CAL_PATH "/efs/FactoryApp/light_factory_cal_v4"
#define LIGHT_UB_CELL_ID_PATH "/efs/FactoryApp/light_ub_cell_id_v3"
#define LIGHT_CAL_DATA_LENGTH 4
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
	case FSTATE_FAC_INACTIVE_2:
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

static ssize_t light_register_read_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	uint16_t light_idx = get_light_sidx(data);
	int cnt = 0;
	int32_t msg_buf[1];

	msg_buf[0] = data->temp_reg;

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

	data->temp_reg = reg;
	pr_info("[SSC_FAC] %s: [0x%x]\n", __func__, data->temp_reg);

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

#if defined(CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR) && defined(CONFIG_DISPLAY_SAMSUNG)
void light_brightness_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct work_struct *)work,
		struct adsp_data, light_br_work);
	int cnt = 0;

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(data->brightness_info, sizeof(data->brightness_info),
		MSG_LIGHT, 0, MSG_TYPE_SET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_SET_CAL_DATA] & 1 << MSG_LIGHT) &&
		cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);

	data->ready_flag[MSG_TYPE_SET_CAL_DATA] &= ~(1 << MSG_LIGHT);

	if (cnt >= TIMEOUT_CNT)
		pr_err("[SSC_FAC] %s: Timeout!!! br: %d\n", __func__,
			data->brightness_info[0]);
	else
		pr_info("[SSC_FAC] %s: set br: %d, lux: %d(lcd:%d)\n", __func__,
			data->brightness_info[0], data->msg_buf[MSG_LIGHT][0],
			data->brightness_info[5]);

	mutex_unlock(&data->light_factory_mutex);
}

int light_panel_data_notify(struct notifier_block *nb,
	unsigned long val, void *v)
{
	struct adsp_data *data = adsp_get_struct_data();
	static int32_t pre_ub_con_state = -1;
#ifdef CONFIG_SUPPORT_PANEL_STATE_NOTIFY_FOR_LIGHT_SENSOR
	static int32_t pre_finger_mask_hbm_on = -1;
	static int32_t pre_test_state = -1;
	static int32_t pre_acl_status = -1;
#endif
	uint16_t light_idx = get_light_sidx(data);

	if (val == PANEL_EVENT_BL_CHANGED) {
		struct panel_bl_event_data *panel_data = v;

		if (panel_data->display_idx > 0)
			return 0;

		data->brightness_info[0] = panel_data->bl_level;
		data->brightness_info[1] = panel_data->aor_data;
#ifdef CONFIG_SUPPORT_PANEL_STATE_NOTIFY_FOR_LIGHT_SENSOR
		data->brightness_info[2] = panel_data->display_idx;
		data->brightness_info[3] = panel_data->finger_mask_hbm_on;
		data->brightness_info[4] = panel_data->acl_status;

		if ((data->brightness_info[0] == data->pre_bl_level) &&
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

		schedule_work(&data->light_br_work);
		pr_info("[SSC_FAC] %s: %d, %d, %d, %d, %d, %d\n", __func__,
			data->brightness_info[0], data->brightness_info[1],
			data->brightness_info[2], data->brightness_info[3],
			data->brightness_info[4], data->brightness_info[5]);
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
#ifdef CONFIG_SUPPORT_PANEL_STATE_NOTIFY_FOR_LIGHT_SENSOR
	} else if (val == PANEL_EVENT_STATE_CHANGED) {
		struct panel_state_data *evdata = (struct panel_state_data *)v;
		int32_t panel_state = (int32_t)evdata->state;
		int32_t msg_buf[4];

		if ((evdata->display_idx > 0) ||
			(panel_state >= MAX_PANEL_STATE) ||
			(data->pre_panel_state == panel_state))
			return 0;

		data->brightness_info[5] = data->pre_panel_state = panel_state;
		msg_buf[0] = OPTION_TYPE_SET_PANEL_STATE;
		msg_buf[1] = panel_state;
		msg_buf[2] = evdata->display_idx;
		msg_buf[3] = data->pre_screen_mode;

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

		if ((test_data->display_idx > 0) ||
			(pre_test_state == (int32_t)test_data->state))
			return 0;

		pre_test_state = (int32_t)test_data->state;
		msg_buf[0] = OPTION_TYPE_SET_PANEL_TEST_STATE;
		msg_buf[1] = (int32_t)test_data->state;

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

#ifdef CONFIG_SUPPORT_UNDER_PANEL_WITH_LIGHT_SENSOR
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
	int new_value;
#if defined(CONFIG_SEC_B2Q_PROJECT)
	int cnt = 0;
#endif
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
#ifdef CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR
		schedule_delayed_work(&data->light_copr_debug_work,
			msecs_to_jiffies(1000));
#endif
#ifdef CONFIG_SUPPORT_FIFO_DEBUG_FOR_LIGHT_SENSOR
		schedule_delayed_work(&data->light_fifo_debug_work,
			msecs_to_jiffies(10 * 1000));
#endif
		data->light_copr_debug_count = 0;
	} else {
#ifdef CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR
		cancel_delayed_work_sync(&data->light_copr_debug_work);
#endif
#ifdef CONFIG_SUPPORT_FIFO_DEBUG_FOR_LIGHT_SENSOR
		cancel_delayed_work_sync(&data->light_fifo_debug_work);
#endif
		data->light_copr_debug_count = 5;
	}
	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(msg_buf, sizeof(msg_buf),
		light_idx, 0, MSG_TYPE_OPTION_DEFINE);
#if defined(CONFIG_SEC_B2Q_PROJECT)
	while (!(data->ready_flag[MSG_TYPE_OPTION_DEFINE] & 1 << light_idx)
		&& cnt++ < TIMEOUT_CNT)
		usleep_range(500, 550);
	data->ready_flag[MSG_TYPE_OPTION_DEFINE] &= ~(1 << light_idx);
	if (cnt >= TIMEOUT_CNT)
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);

	pr_info("[SSC_FAC] %s: done(%d)\n", __func__, new_value);
#endif
	adsp_unicast(msg_buf, sizeof(msg_buf),
		MSG_SSC_CORE, 0, MSG_TYPE_OPTION_DEFINE);
#ifdef CONFIG_SUPPORT_DUAL_DDI_COPR_FOR_LIGHT_SENSOR
	adsp_unicast(msg_buf, sizeof(msg_buf),
		MSG_DDI, 0, MSG_TYPE_OPTION_DEFINE);
#endif
	mutex_unlock(&data->light_factory_mutex);

	return size;
}

static ssize_t light_circle_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SEC_M44X_PROJECT)
	return snprintf(buf, PAGE_SIZE, "50.499 -5.0 3.0\n");
#else
	return snprintf(buf, PAGE_SIZE, "0 0 0\n");
#endif
}
#endif /* CONFIG_SUPPORT_UNDER_PANEL_WITH_LIGHT_SENSOR */

#ifdef CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR
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
	int32_t cmd = OPTION_TYPE_GET_CHIP_ID;
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;

#ifdef CONFIG_SUPPORT_DUAL_DDI_COPR_FOR_LIGHT_SENSOR
	light_idx = MSG_DDI;
#endif
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

	if (data->brightness_info[5] == 0)
		return;

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
#ifdef CONFIG_SUPPORT_FIFO_DEBUG_FOR_LIGHT_SENSOR
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

#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
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

#ifdef CONFIG_SUPPORT_LIGHT_CALIBRATION
int light_get_cal_data(int32_t *cal_data)
{
	struct file *factory_cal_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	factory_cal_filp = filp_open(LIGHT_FACTORY_CAL_PATH, O_RDONLY, 0440);

	if (IS_ERR(factory_cal_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(factory_cal_filp);
		pr_err("[SSC_FAC] %s: open fail light_factory_cal:%d\n",
			__func__, ret);
		return ret;
	}

	ret = vfs_read(factory_cal_filp, (char *)cal_data,
		sizeof(int32_t) * LIGHT_CAL_DATA_LENGTH,
		&factory_cal_filp->f_pos);
	if (ret < 0)
		pr_err("[SSC_FAC] %s: fd read fail:%d\n", __func__, ret);

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
		pr_err("[SSC_FAC] %s: cal value error: %d, %d\n",
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
		pr_err("[SSC_FAC] %s: open fail light_factory_cal:%d\n",
			__func__, ret);
		return ret;
	}

	ret = vfs_write(factory_cal_filp, (char *)cal_data,
		sizeof(int32_t) * LIGHT_CAL_DATA_LENGTH,
		&factory_cal_filp->f_pos);
	if (ret < 0)
		pr_err("[SSC_FAC] %s: fd write %d\n", __func__, ret);

	filp_close(factory_cal_filp, current->files);
	set_fs(old_fs);

	return ret;
}

int light_load_ub_cell_id_from_file(char *path, char *data_str)
{
	struct file *file_filp = NULL;
	mm_segment_t old_fs;
	int ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	file_filp = filp_open(path, O_RDONLY, 0440);

	if (IS_ERR(file_filp)) {
		set_fs(old_fs);
		ret = PTR_ERR(file_filp);
		pr_err("[SSC_FAC] %s: open fail(%s):%d\n", __func__, path, ret);
		return ret;
	}

	ret = vfs_read(file_filp, (char *)data_str,
		sizeof(char) * (LIGHT_UB_CELL_ID_INFO_STRING_LENGTH - 1),
		&file_filp->f_pos);
	data_str[LIGHT_UB_CELL_ID_INFO_STRING_LENGTH - 1] = '\0';

	if (ret < 0)
		pr_err("[SSC_FAC] %s: fd read fail(%s): %d\n",
			__func__, path, ret);
	else
		pr_info("[SSC_FAC] %s: data(%s): %s\n",
			__func__, path, data_str);

	filp_close(file_filp, current->files);
	set_fs(old_fs);

	return ret;
}

int light_save_ub_cell_id_to_efs(char *data_str, bool first_booting)
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
		pr_err("[SSC_FAC] %s: open fail light_factory_cal(%s): %d\n",
			__func__, LIGHT_UB_CELL_ID_PATH, ret);
		return ret;
	}

	ret = vfs_write(file_filp, (char *)data_str,
		sizeof(char) * LIGHT_UB_CELL_ID_INFO_STRING_LENGTH,
		&file_filp->f_pos);
	if (ret < 0)
		pr_err("[SSC_FAC] %s: fd write fail(%s): %d\n",
			__func__, LIGHT_UB_CELL_ID_PATH, ret);

	filp_close(file_filp, current->files);
	set_fs(old_fs);

	pr_info("[SSC_FAC] %s(%s): %s\n", __func__,
		LIGHT_UB_CELL_ID_PATH, data_str);

	return ret;
}

void light_cal_read_work_func(struct work_struct *work)
{
	struct adsp_data *data = container_of((struct delayed_work *)work,
		struct adsp_data, light_cal_work);
	int32_t msg_buf[LIGHT_CAL_DATA_LENGTH];
	char *cur_data_str = kzalloc(LIGHT_UB_CELL_ID_INFO_STRING_LENGTH,
				GFP_KERNEL);

	light_load_ub_cell_id_from_file(UB_CELL_ID_PATH, cur_data_str);
	if (strcmp(data->light_ub_id, cur_data_str) == 0) {
		pr_info("[SSC_FAC] %s : UB is matched\n", __func__);
	} else {
		pr_info("[SSC_FAC] %s : UB is not matched\n", __func__);
#if defined(CONFIG_SUPPORT_PROX_CALIBRATION) && !defined (CONFIG_SEC_FACTORY)
		prox_send_cal_data(data, false);
#endif
		kfree(cur_data_str);
		return;
	}
	kfree(cur_data_str);

#ifdef CONFIG_SUPPORT_PROX_CALIBRATION
	prox_send_cal_data(data, true);
#endif

	if ((data->light_cal_result == LIGHT_CAL_PASS) &&
		(data->light_cal1 >= 0) && (data->light_cal2 >= 0)) {
		mutex_lock(&data->light_factory_mutex);
		msg_buf[0] = OPTION_TYPE_SET_LIGHT_CAL;
		msg_buf[1] = data->light_cal1;
		msg_buf[2] = data->light_cal2;
		msg_buf[3] = data->copr_w;
		adsp_unicast(msg_buf, sizeof(msg_buf),
				MSG_LIGHT, 0, MSG_TYPE_OPTION_DEFINE);
		mutex_unlock(&data->light_factory_mutex);
		pr_info("[SSC_FAC] %s: Cal1: %d, Cal2: %d, COPR_W: %d)\n",
			__func__, msg_buf[1], msg_buf[2], msg_buf[3]);
	}
}

void light_cal_init_work(struct adsp_data *data)
{
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;
	int32_t init_data[LIGHT_CAL_DATA_LENGTH] = {LIGHT_CAL_FAIL, -1, -1, -1};
	int ret = 0;
	char *temp_str;

	data->pre_bl_level = -1;
	data->pre_panel_state = -1;
	data->light_cal_result = LIGHT_CAL_FAIL;
	data->light_cal1 = -1;
	data->light_cal2 = -1;
	data->copr_w = -1;
	data->light_debug_info_cmd = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(LIGHT_FACTORY_CAL_PATH, O_RDONLY, 0440);
	if (PTR_ERR(cal_filp) == -ENOENT || PTR_ERR(cal_filp) == -ENXIO) {
		pr_info("[SSC_FAC] %s : no light_factory_cal file\n", __func__);
		light_set_cal_data(init_data, true);
	} else if (IS_ERR(cal_filp)) {
		pr_err("[SSC_FAC]: %s - filp_open error\n", __func__);
	} else {
		ret = vfs_read(cal_filp, (char *)init_data,
			sizeof(int32_t) * LIGHT_CAL_DATA_LENGTH,
			&cal_filp->f_pos);
		if (ret < 0) {
			pr_err("[SSC_FAC] %s: fd read fail:%d\n",
				__func__, ret);
		} else {
			pr_info("[SSC_FAC] %s : already exist (P/F: %d, Cal1: %d, Cal2: %d, COPR_W: %d)\n",
				__func__, init_data[0], init_data[1],
				init_data[2], init_data[3]);
			data->light_cal_result = init_data[0];
			data->light_cal1 = init_data[1];
			data->light_cal2 = init_data[2];
			data->copr_w = init_data[3];
		}
		filp_close(cal_filp, current->files);
	}

	cal_filp = filp_open(LIGHT_UB_CELL_ID_PATH, O_RDONLY, 0440);
	if (PTR_ERR(cal_filp) == -ENOENT || PTR_ERR(cal_filp) == -ENXIO) {
		pr_info("[SSC_FAC] %s : no light_ub_efs file\n", __func__);
		temp_str = kzalloc(LIGHT_UB_CELL_ID_INFO_STRING_LENGTH,
			GFP_KERNEL);
		light_save_ub_cell_id_to_efs(temp_str, true);
		kfree(temp_str);
	} else if (IS_ERR(cal_filp)) {
		pr_err("[SSC_FAC]: %s - ub filp_open error\n", __func__);
	} else {
		ret = vfs_read(cal_filp, (char *)data->light_ub_id,
			sizeof(char) * LIGHT_UB_CELL_ID_INFO_STRING_LENGTH,
			&cal_filp->f_pos);
		if (ret < 0)
			pr_err("[SSC_FAC] %s: ub read fail:%d\n",
				__func__, ret);
		else
			pr_info("[SSC_FAC] %s : already ub efs exist %s\n",
				__func__, data->light_ub_id);

		filp_close(cal_filp, current->files);
	}

	set_fs(old_fs);
	schedule_delayed_work(&data->light_cal_work, msecs_to_jiffies(8000));
}

static ssize_t light_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t msg_buf[LIGHT_CAL_DATA_LENGTH], cnt = 0;
	int32_t cal_data[LIGHT_CAL_DATA_LENGTH] = {LIGHT_CAL_FAIL, -1, -1, -1};
	int32_t cmd = OPTION_TYPE_GET_LIGHT_CAL, cur_lux, ret;
	uint16_t light_idx = get_light_sidx(data);

	ret = light_get_cal_data(cal_data);

	mutex_lock(&data->light_factory_mutex);
	adsp_unicast(&cmd, sizeof(cmd), light_idx, 0, MSG_TYPE_GET_CAL_DATA);

	while (!(data->ready_flag[MSG_TYPE_GET_CAL_DATA]
		& 1 << light_idx) && cnt++ < TIMEOUT_CNT)
		usleep_range(1000, 1100);

	data->ready_flag[MSG_TYPE_GET_CAL_DATA] &= ~(1 << light_idx);

	if (cnt >= TIMEOUT_CNT) {
		pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		cur_lux = -1;
	} else {
		cur_lux = data->msg_buf[light_idx][4];
	}

	msg_buf[0] = OPTION_TYPE_SET_LIGHT_CAL;
	msg_buf[1] = cal_data[1];
	msg_buf[2] = cal_data[2];
	msg_buf[3] = cal_data[3];

	if ((ret >= 0) && (cal_data[0] == LIGHT_CAL_PASS) && (cal_data[1] >= 0)
		&& (cal_data[2] >= 0))
		adsp_unicast(msg_buf, sizeof(msg_buf),
			light_idx, 0, MSG_TYPE_OPTION_DEFINE);
	mutex_unlock(&data->light_factory_mutex);

	pr_info("[SSC_FAC] %s: cal_data (P/F: %d, Cal1: %d, Cal2: %d, COPR_W: %d, cur lux: %d)\n",
		__func__, cal_data[0], cal_data[1],
		cal_data[2], cal_data[3], cur_lux);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		LIGHT_CAL_PASS, cal_data[2], cur_lux);
}

static ssize_t light_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct adsp_data *data = dev_get_drvdata(dev);
	int32_t cmd = OPTION_TYPE_GET_LIGHT_CAL, new_value;
	int32_t cal_data[LIGHT_CAL_DATA_LENGTH] = {LIGHT_CAL_FAIL, -1, -1, -1};
	int32_t msg_buf[LIGHT_CAL_DATA_LENGTH] = {0, 0, 0, 0};
	uint16_t light_idx = get_light_sidx(data);
	uint8_t cnt = 0;
	char *cur_id_str;

	if (sysfs_streq(buf, "0"))
		new_value = 0;
	else if (sysfs_streq(buf, "1"))
		new_value = 1;
	else
		return size;

	pr_info("[SSC_FAC] %s: cmd: %d\n", __func__, new_value);

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
			pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
			return size;
		}

		cal_data[0] = data->msg_buf[light_idx][0];
		cal_data[1] = data->msg_buf[light_idx][1];
		cal_data[2] = data->msg_buf[light_idx][2];
		cal_data[3] = data->msg_buf[light_idx][3];

		pr_info("[SSC_FAC] %s: (P/F: %d, Cal1: %d, Cal2: %d, COPR_W: %d)\n",
			__func__, data->msg_buf[light_idx][0],
			data->msg_buf[light_idx][1],
			data->msg_buf[light_idx][2],
			data->msg_buf[light_idx][3]);

		if (cal_data[0] == LIGHT_CAL_PASS) {
			data->light_cal_result = LIGHT_CAL_PASS;
			data->light_cal1 = msg_buf[1] = cal_data[1];
			data->light_cal2 = msg_buf[2] = cal_data[2];
			data->copr_w = msg_buf[3] = cal_data[3];
		} else {
			return size;
		}

		cur_id_str = kzalloc(LIGHT_UB_CELL_ID_INFO_STRING_LENGTH,
			GFP_KERNEL);
		light_load_ub_cell_id_from_file(UB_CELL_ID_PATH, cur_id_str);
		light_save_ub_cell_id_to_efs(cur_id_str, false);
		kfree(cur_id_str);
	} else {
		mutex_lock(&data->light_factory_mutex);
		adsp_unicast(data->brightness_info,
			sizeof(data->brightness_info),
			MSG_LIGHT, 0, MSG_TYPE_SET_CAL_DATA);

		while (!(data->ready_flag[MSG_TYPE_SET_CAL_DATA] & 1 << MSG_LIGHT) &&
			cnt++ < TIMEOUT_CNT)
			usleep_range(500, 550);

		data->ready_flag[MSG_TYPE_SET_CAL_DATA] &= ~(1 << MSG_LIGHT);

		if (cnt >= TIMEOUT_CNT)
			pr_err("[SSC_FAC] %s: Timeout!!!\n", __func__);
		else
			pr_info("[SSC_FAC] %s: set br %d\n",
				__func__, data->msg_buf[MSG_LIGHT][0]);

		mutex_unlock(&data->light_factory_mutex);

		data->light_cal_result = cal_data[0] = LIGHT_CAL_FAIL;
		data->light_cal1 = cal_data[1] = msg_buf[1] = 0;
		data->light_cal2 = cal_data[2] = msg_buf[2] = 0;
		data->copr_w = cal_data[3] = msg_buf[3] = 0;
	}

	light_set_cal_data(cal_data, false);

	mutex_lock(&data->light_factory_mutex);
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

static DEVICE_ATTR(light_cal, 0664, light_cal_show, light_cal_store);
static DEVICE_ATTR(light_test, 0444, light_test_show, NULL);
#endif /* CONFIG_SUPPORT_LIGHT_CALIBRATION */

#ifdef CONFIG_SUPPORT_UNDER_PANEL_WITH_LIGHT_SENSOR
static DEVICE_ATTR(lcd_onoff, 0220, NULL, light_lcd_onoff_store);
static DEVICE_ATTR(hallic_info, 0220, NULL, light_hallic_info_store);
static DEVICE_ATTR(light_circle, 0444, light_circle_show, NULL);
#endif
#ifdef CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR
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
#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
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
#ifdef CONFIG_SUPPORT_UNDER_PANEL_WITH_LIGHT_SENSOR
	&dev_attr_lcd_onoff,
	&dev_attr_hallic_info,
	&dev_attr_light_circle,
#endif
#ifdef CONFIG_SUPPORT_DDI_COPR_FOR_LIGHT_SENSOR
	&dev_attr_read_copr,
	&dev_attr_test_copr,
	&dev_attr_boled_enable,
	&dev_attr_copr_roix,
	&dev_attr_sensorhub_ddi_spi_check,
#endif
#ifdef CONFIG_SUPPORT_LIGHT_CALIBRATION
	&dev_attr_light_cal,
	&dev_attr_light_test,
#endif
	&dev_attr_debug_info,
	&dev_attr_hyst,
#ifdef CONFIG_SUPPORT_SSC_AOD_RECT
	&dev_attr_set_aod_rect,
#endif
	NULL,
};

static int __init stk3a5x_light_factory_init(void)
{
	adsp_factory_register(MSG_LIGHT, light_attrs);
#if defined(CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR) && defined(CONFIG_DISPLAY_SAMSUNG)
	ss_panel_notifier_register(&light_panel_data_notifier);
#endif
	pr_info("[SSC_FAC] %s\n", __func__);

	return 0;
}

static void __exit stk3a5x_light_factory_exit(void)
{
	adsp_factory_unregister(MSG_LIGHT);
#if defined(CONFIG_SUPPORT_BRIGHTNESS_NOTIFY_FOR_LIGHT_SENSOR) && defined(CONFIG_DISPLAY_SAMSUNG)
	ss_panel_notifier_unregister(&light_panel_data_notifier);
#endif
	pr_info("[SSC_FAC] %s\n", __func__);
}
module_init(stk3a5x_light_factory_init);
module_exit(stk3a5x_light_factory_exit);
