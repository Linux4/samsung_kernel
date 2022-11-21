/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#if defined(CONFIG_CAMERA_ADAPTIVE_MIPI)
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/dev_ril_bridge.h>
#include "cam_sensor_mipi.h"
#include "cam_sensor_adaptive_mipi_wide.h"
#include "cam_sensor_adaptive_mipi_uw.h"
#include "cam_sensor_adaptive_mipi_tele.h"
#include "cam_sensor_adaptive_mipi_front.h"
#include "cam_sensor_dev.h"

static int adaptive_mipi_mode;
module_param(adaptive_mipi_mode, int, 0644);

static struct cam_cp_noti_info g_cp_noti_info;
static struct mutex g_mipi_mutex;
static bool g_init_notifier;
extern char mipi_string[20];

/* CP notity format (HEX raw format)
 * 10 00 AA BB 27 01 03 XX YY YY YY YY ZZ ZZ ZZ ZZ
 *
 * 00 10 (0x0010) - len
 * AA BB - not used
 * 27 - MAIN CMD (SYSTEM CMD : 0x27)
 * 01 - SUB CMD (CP Channel Info : 0x01)
 * 03 - NOTI CMD (0x03)
 * XX - RAT MODE
 * YY YY YY YY - BAND MODE
 * ZZ ZZ ZZ ZZ - FREQ INFO
 */

void *bsearch(const void *key, const void *base, size_t num, size_t size, cmp_func_t cmp)
{
	const char *pivot;
	int result;

	while (num > 0) {
		pivot = base + (num >> 1) * size;
		result = cmp(key, pivot);

		if (result == 0)
			return (void *)pivot;

		if (result > 0) {
			base = pivot + size;
			num--;
		}
		num >>= 1;
	}

	return NULL;
}

static int cam_mipi_ril_notifier(struct notifier_block *nb,
				unsigned long size, void *buf)
{
	struct dev_ril_bridge_msg *msg;
	struct cam_cp_noti_info *cp_noti_info;

	if (!g_init_notifier) {
		CAM_ERR(CAM_SENSOR, "[AM_DBG] not init ril notifier");
		return NOTIFY_DONE;
	}

	CAM_INFO(CAM_SENSOR, "[AM_DBG] ril notification size [%ld]", size);

	msg = (struct dev_ril_bridge_msg *)buf;
	CAM_INFO(CAM_SENSOR, "[AM_DBG] dev_id : %d, data_len : %d",
		msg->dev_id, msg->data_len);

	if (msg->dev_id == IPC_SYSTEM_CP_CHANNEL_INFO
				&& msg->data_len == sizeof(struct cam_cp_noti_info)) {
	   cp_noti_info = (struct cam_cp_noti_info *)msg->data;
	   mutex_lock(&g_mipi_mutex);
	   memcpy(&g_cp_noti_info, cp_noti_info, sizeof(struct cam_cp_noti_info));
	   mutex_unlock(&g_mipi_mutex);

	   CAM_INFO(CAM_SENSOR, "[AM_DBG] update mipi channel [%d,%d,%d]",
		   g_cp_noti_info.rat, g_cp_noti_info.band, g_cp_noti_info.channel);
	   return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block g_ril_notifier_block = {
	.notifier_call = cam_mipi_ril_notifier,
};

void cam_mipi_register_ril_notifier(void)
{
	if (!g_init_notifier) {
		CAM_INFO(CAM_SENSOR, "[AM_DBG] register ril notifier");

		mutex_init(&g_mipi_mutex);
		memset(&g_cp_noti_info, 0, sizeof(struct cam_cp_noti_info));

		register_dev_ril_bridge_event_notifier(&g_ril_notifier_block);
		g_init_notifier = true;
	}
}

static void cam_mipi_get_rf_channel(struct cam_cp_noti_info *ch)
{
	if (!g_init_notifier) {
		CAM_ERR(CAM_SENSOR, "[AM_DBG] not init ril notifier");
		memset(ch, 0, sizeof(struct cam_cp_noti_info));
		return;
	}

	mutex_lock(&g_mipi_mutex);
	memcpy(ch, &g_cp_noti_info, sizeof(struct cam_cp_noti_info));
	mutex_unlock(&g_mipi_mutex);
}

static int compare_rf_channel(const void *key, const void *element)
{
	struct cam_mipi_channel *k = ((struct cam_mipi_channel *)key);
	struct cam_mipi_channel *e = ((struct cam_mipi_channel *)element);

	if (k->rat_band < e->rat_band)
		return -1;
	else if (k->rat_band > e->rat_band)
		return 1;

	if (k->channel_max < e->channel_min)
		return -1;
	else if (k->channel_min > e->channel_max)
		return 1;

	return 0;
}

int cam_mipi_select_mipi_by_rf_channel(const struct cam_mipi_channel *channel_list, const int size)
{
	struct cam_mipi_channel *result = NULL;
	struct cam_mipi_channel key;
	struct cam_cp_noti_info input_ch;

	cam_mipi_get_rf_channel(&input_ch);

	key.rat_band = CAM_RAT_BAND(input_ch.rat, input_ch.band);
	key.channel_min = input_ch.channel;
	key.channel_max = input_ch.channel;

	CAM_INFO(CAM_SENSOR, "[AM_DBG] searching rf channel s [%d,%d,%d]",
		input_ch.rat, input_ch.band, input_ch.channel);

	result = bsearch(&key,
			channel_list,
			size,
			sizeof(struct cam_mipi_channel),
			compare_rf_channel);

	if (result == NULL) {
		CAM_INFO(CAM_SENSOR, "[AM_DBG] searching result : not found");
		return -1;
	}

	CAM_DBG(CAM_SENSOR, "[AM_DBG] searching result : [0x%x,(%d-%d)]->(%d)",
		result->rat_band, result->channel_min, result->channel_max, result->setting_index);

	return result->setting_index;
}

int32_t cam_check_sensor_type(uint16_t sensor_id)
{
	int32_t sensor_type = INVALID;

	switch (sensor_id) {
		case SENSOR_ID_S5KGN3:
		case SENSOR_ID_S5KHM3:
			sensor_type = WIDE;
			break;

		case SENSOR_ID_IMX374:
		case SENSOR_ID_S5KGH1:
 			sensor_type = FRONT;
			break;

		case SENSOR_ID_IMX563:
			sensor_type = UW;
			break;

		case SENSOR_ID_S5K3K1:
		case SENSOR_ID_IMX754:
			sensor_type = TELE;
			break;

		default:
			sensor_type = INVALID;
			break;
	}
	CAM_INFO(CAM_SENSOR, "[AM_DBG] sensor_type : %d, 0x%x", sensor_type, sensor_id);

	return sensor_type;
}

void cam_mipi_init_setting(struct cam_sensor_ctrl_t *s_ctrl)
{
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	int32_t sensor_type = cam_check_sensor_type(s_ctrl->sensordata->slave_info.sensor_id);

#if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
	extern long rear_frs_test_mode;

	if (rear_frs_test_mode == 0) {
#endif
	if (sensor_type == WIDE) {
		CAM_INFO(CAM_SENSOR, "[AM_DBG] Wide sensor_mode : %d / %d", s_ctrl->sensor_mode, num_wide_mipi_setting);
		if (s_ctrl->sensor_mode == 0) {
			s_ctrl->mipi_info = sensor_wide_mipi_A_mode;
		} else if (s_ctrl->sensor_mode == 1 && s_ctrl->sensor_mode <= num_wide_mipi_setting) {
			s_ctrl->mipi_info = sensor_wide_mipi_B_mode;
		} else if (s_ctrl->sensor_mode == 2 && s_ctrl->sensor_mode <= num_wide_mipi_setting) {
			s_ctrl->mipi_info = sensor_wide_mipi_C_mode;
		} else if (s_ctrl->sensor_mode == 3 && s_ctrl->sensor_mode <= num_wide_mipi_setting) {
			s_ctrl->mipi_info = sensor_wide_mipi_D_mode;
		} else {
			s_ctrl->mipi_info = sensor_wide_mipi_A_mode;
		}
	}
	else if (sensor_type == FRONT) {
		CAM_INFO(CAM_SENSOR, "[AM_DBG] Front sensor_mode : %d / %d", s_ctrl->sensor_mode, num_front_mipi_setting);
		if (s_ctrl->sensor_mode == 0) {
			s_ctrl->mipi_info = sensor_front_mipi_A_mode;
		} else if (s_ctrl->sensor_mode == 1 && s_ctrl->sensor_mode <= num_front_mipi_setting) {
			s_ctrl->mipi_info = sensor_front_mipi_B_mode;
		} else if (s_ctrl->sensor_mode == 2 && s_ctrl->sensor_mode <= num_front_mipi_setting) {
			s_ctrl->mipi_info = sensor_front_mipi_C_mode;
		} else if (s_ctrl->sensor_mode == 3 && s_ctrl->sensor_mode <= num_front_mipi_setting) {
			s_ctrl->mipi_info = sensor_front_mipi_D_mode;
		} else {
			s_ctrl->mipi_info = sensor_front_mipi_A_mode;
		}
	}
	else if (sensor_type == UW) {
		CAM_INFO(CAM_SENSOR, "[AM_DBG] UW sensor_mode : %d / %d", s_ctrl->sensor_mode, num_uw_mipi_setting);
		if (s_ctrl->sensor_mode == 0) {
			s_ctrl->mipi_info = sensor_uw_mipi_A_mode;
		} else if (s_ctrl->sensor_mode == 1 && s_ctrl->sensor_mode <= num_uw_mipi_setting) {
			s_ctrl->mipi_info = sensor_uw_mipi_B_mode;
		} else if (s_ctrl->sensor_mode == 2 && s_ctrl->sensor_mode <= num_uw_mipi_setting) {
			s_ctrl->mipi_info = sensor_uw_mipi_C_mode;
		} else if (s_ctrl->sensor_mode == 3 && s_ctrl->sensor_mode <= num_uw_mipi_setting) {
			s_ctrl->mipi_info = sensor_uw_mipi_D_mode;
		} else {
			s_ctrl->mipi_info = sensor_uw_mipi_A_mode;
		}
	}
	else if (sensor_type == TELE) {
		CAM_INFO(CAM_SENSOR, "[AM_DBG] Tele sensor_mode : %d / %d", s_ctrl->sensor_mode, num_tele_mipi_setting);
		if (s_ctrl->sensor_mode == 0) {
			s_ctrl->mipi_info = sensor_tele_mipi_A_mode;
		} else if (s_ctrl->sensor_mode == 1 && s_ctrl->sensor_mode <= num_tele_mipi_setting) {
			s_ctrl->mipi_info = sensor_tele_mipi_B_mode;
		} else if (s_ctrl->sensor_mode == 2 && s_ctrl->sensor_mode <= num_tele_mipi_setting) {
			s_ctrl->mipi_info = sensor_tele_mipi_C_mode;
		} else if (s_ctrl->sensor_mode == 3 && s_ctrl->sensor_mode <= num_tele_mipi_setting) {
			s_ctrl->mipi_info = sensor_tele_mipi_D_mode;
		} else {
			s_ctrl->mipi_info = sensor_tele_mipi_A_mode;
		}
	}
	else {
		CAM_ERR(CAM_SENSOR, "[AM_DBG] Not support sensor_type : %d", sensor_type);
		s_ctrl->mipi_info = sensor_wide_mipi_A_mode;
	}
	cur_mipi_sensor_mode = &(s_ctrl->mipi_info[0]);
 #if defined(CONFIG_CAMERA_FRS_DRAM_TEST)
	}
#endif

	s_ctrl->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	s_ctrl->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
}

void cam_mipi_update_info(struct cam_sensor_ctrl_t *s_ctrl)
{
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	int found = -1;

	cur_mipi_sensor_mode = &(s_ctrl->mipi_info[0]);

	CAM_DBG(CAM_SENSOR, "[AM_DBG] cur rat : %d", cur_mipi_sensor_mode->mipi_channel->rat_band);
	CAM_DBG(CAM_SENSOR, "[AM_DBG] cur channel_min : %d", cur_mipi_sensor_mode->mipi_channel->channel_min);
	CAM_DBG(CAM_SENSOR, "[AM_DBG] cur channel_max : %d", cur_mipi_sensor_mode->mipi_channel->channel_max);
	CAM_DBG(CAM_SENSOR, "[AM_DBG] cur setting_index : %d", cur_mipi_sensor_mode->mipi_channel->setting_index);

	found = cam_mipi_select_mipi_by_rf_channel(cur_mipi_sensor_mode->mipi_channel,
				cur_mipi_sensor_mode->mipi_channel_size);
	if (found != -1) {
		if (found < cur_mipi_sensor_mode->sensor_setting_size) {
			s_ctrl->mipi_clock_index_new = found;

			CAM_DBG(CAM_SENSOR, "[AM_DBG] mipi_clock_index_new : %d",
				s_ctrl->mipi_clock_index_new);
		} else {
			CAM_ERR(CAM_SENSOR, "sensor setting size is out of bound");
		}
	}
 	else {
		CAM_INFO(CAM_SENSOR, "not found rf channel, use default mipi clock");
		s_ctrl->mipi_clock_index_new = 0;
	}

#if defined(CONFIG_SEC_FACTORY)
	s_ctrl->mipi_clock_index_new = 0;//only for factory
#endif

	if (adaptive_mipi_mode > 0) {
		s_ctrl->mipi_clock_index_new = adaptive_mipi_mode - 10;
		CAM_INFO(CAM_SENSOR, "[AM_DBG] test adaptive mode : %d", s_ctrl->mipi_clock_index_new);
	}
}

void cam_mipi_get_clock_string(struct cam_sensor_ctrl_t *s_ctrl)
{
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;

	cur_mipi_sensor_mode = &(s_ctrl->mipi_info[0]);

	sprintf(mipi_string, "%s",
		cur_mipi_sensor_mode->mipi_setting[s_ctrl->mipi_clock_index_new].str_mipi_clk);

	CAM_DBG(CAM_SENSOR, "[AM_DBG] cam_mipi_get_clock_string : %d", s_ctrl->mipi_clock_index_new);
	CAM_DBG(CAM_SENSOR, "[AM_DBG] mipi_string : %s", mipi_string);
 }
#endif
