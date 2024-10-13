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
#include "cam_sensor_adaptive_mipi_wide_2_0.h"
#include "cam_sensor_adaptive_mipi_uw_2_0.h"
#include "cam_sensor_adaptive_mipi_tele_2_0.h"
#include "cam_sensor_adaptive_mipi_tele2_2_0.h"
#include "cam_sensor_adaptive_mipi_front_2_0.h"
#include "cam_sensor_adaptive_mipi_front_top_2_0.h"
#include "cam_sensor_dev.h"

static int adaptive_mipi_mode;
module_param(adaptive_mipi_mode, int, 0644);

/*
adb shell "echo 1,56,61 > /sys/module/camera/parameters/am_auto_test"
*/
static int am_auto_test_count;
static unsigned int am_auto_test[10];
module_param_array(am_auto_test, uint, &am_auto_test_count, 0644);

static struct cam_cp_noti_cell_infos g_cp_noti_cell_infos;
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
	int data_size;
	int msg_data_size;
	int i;

	if (!g_init_notifier) {
		CAM_ERR(CAM_SENSOR, "[AM_DBG] not init ril notifier");
		return NOTIFY_DONE;
	}

	CAM_INFO(CAM_SENSOR, "[AM_DBG] ril notification size [%ld]", size);

	msg = (struct dev_ril_bridge_msg *)buf;
	CAM_INFO(CAM_SENSOR, "[AM_DBG] dev_id : %d, data_len : %d",
		msg->dev_id, msg->data_len);

	if (size == sizeof(struct dev_ril_bridge_msg)
			&& msg->dev_id == IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO) {
		data_size = sizeof(struct cam_cp_cell_info);
		msg_data_size = msg->data_len - sizeof(g_cp_noti_cell_infos.num_cell);
		memcpy(&g_cp_noti_cell_infos, msg->data, sizeof(g_cp_noti_cell_infos.num_cell)); // get number of cell

		CAM_INFO(CAM_SENSOR, "[AM_DBG] num_cell: %d, data_size : %d, msg_data_size : %d",
			g_cp_noti_cell_infos.num_cell, data_size, msg_data_size);

		if (msg_data_size == data_size * CAM_MIPI_MAX_BAND) {
			mutex_lock(&g_mipi_mutex);
			memset(&g_cp_noti_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));
			memcpy(&g_cp_noti_cell_infos, msg->data, msg->data_len);
			mutex_unlock(&g_mipi_mutex);

			for (i = 0; i < g_cp_noti_cell_infos.num_cell; i++) {
				CAM_INFO(CAM_SENSOR, "[AM_DBG] update mipi cell info %d : [%d,%d,%d,%d,%d,%d]",
					i, g_cp_noti_cell_infos.cell_list[i].rat, g_cp_noti_cell_infos.cell_list[i].band,
					g_cp_noti_cell_infos.cell_list[i].channel, g_cp_noti_cell_infos.cell_list[i].connection_status,
					g_cp_noti_cell_infos.cell_list[i].bandwidth, g_cp_noti_cell_infos.cell_list[i].sinr);
			}

			return NOTIFY_OK;
		}
		else
		{
			CAM_ERR(CAM_SENSOR, "[AM_DBG] mismatching msg data size : %d", data_size * g_cp_noti_cell_infos.num_cell);
		}
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
		memset(&g_cp_noti_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));

		register_dev_ril_bridge_event_notifier(&g_ril_notifier_block);
		g_init_notifier = true;
	}
}

static void cam_mipi_get_rf_cell_infos(struct cam_cp_noti_cell_infos *cell_infos)
{
	if (am_auto_test_count > 0) {
		int i = 0;
		memset(cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));

		cell_infos->num_cell = am_auto_test_count;

		for (i = 0; i < am_auto_test_count; i++) {
			cell_infos->cell_list[i].rat = test_cp_cell_infos[am_auto_test[i] - 1].rat;
			cell_infos->cell_list[i].band = test_cp_cell_infos[am_auto_test[i] - 1].band;
			cell_infos->cell_list[i].channel = test_cp_cell_infos[am_auto_test[i] - 1].channel;
			cell_infos->cell_list[i].connection_status = test_cp_cell_infos[am_auto_test[i] - 1].connection_status;
			cell_infos->cell_list[i].bandwidth = test_cp_cell_infos[am_auto_test[i] - 1].bandwidth;
			cell_infos->cell_list[i].sinr = test_cp_cell_infos[am_auto_test[i] - 1].sinr;

			CAM_INFO(CAM_SENSOR, "[AM_DBG] update test mipi cell info %d : [%d,%d,%d,%d,%d,%d]",
				am_auto_test[i], cell_infos->cell_list[i].rat, cell_infos->cell_list[i].band,
				cell_infos->cell_list[i].channel, cell_infos->cell_list[i].connection_status,
				cell_infos->cell_list[i].bandwidth, cell_infos->cell_list[i].sinr);
		}

		return;
	}

	if (!g_init_notifier) {
		CAM_ERR(CAM_SENSOR, "[AM_DBG] not init ril notifier\n");
		memset(cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));
		return;
	}

	mutex_lock(&g_mipi_mutex);
	memcpy(cell_infos, &g_cp_noti_cell_infos, sizeof(struct cam_cp_noti_cell_infos));
	mutex_unlock(&g_mipi_mutex);
}

static int compare_rf_cell_ratings(const void *key, const void *element)
{
	struct cam_mipi_cell_ratings *k = ((struct cam_mipi_cell_ratings *)key);
	struct cam_mipi_cell_ratings *e = ((struct cam_mipi_cell_ratings *)element);

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

int cam_mipi_select_mipi_by_rf_cell_infos(struct cam_sensor_ctrl_t *s_ctrl,
	const struct cam_mipi_cell_ratings *channel_list,
	const int size, const int freq_size)
{
	struct cam_mipi_cell_ratings *result = NULL;
	struct cam_mipi_cell_ratings key;
	struct cam_cp_noti_cell_infos cell_infos;
	int i, j;
	int freq_ratings_sums[CAM_MIPI_MAX_FREQ] = {0,};
	int min = 0x7fffffff;
	int min_freq_idx = -1;
	char print_buf[128] = {0,};
	int print_buf_size = sizeof(print_buf);
	int print_buf_cnt = 0;
	int freq_rating;
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	int32_t sensor_type = 0;

	cam_mipi_get_rf_cell_infos(&cell_infos);

	CAM_INFO(CAM_SENSOR, "[AM_DBG] cell number %d", cell_infos.num_cell);

	for (i = 0; i < cell_infos.num_cell; i++) {
		key.rat_band = CAM_RAT_BAND(cell_infos.cell_list[i].rat, cell_infos.cell_list[i].band);
		key.channel_min = cell_infos.cell_list[i].channel;
		key.channel_max = cell_infos.cell_list[i].channel;

		CAM_INFO(CAM_SENSOR, "[AM_DBG] searching rf channel s [%d,%d,%d]\n",
			cell_infos.cell_list[i].rat,
			cell_infos.cell_list[i].band, cell_infos.cell_list[i].channel);

		result = bsearch(&key,
				channel_list,
				size,
				sizeof(struct cam_mipi_cell_ratings),
				compare_rf_cell_ratings);

		if (result == NULL) {
			CAM_INFO(CAM_SENSOR, "[AM_DBG] searching result : not found, skip this\n");
			continue;
		}

		memset(print_buf, print_buf_size, 0);
		print_buf_cnt = 0;

		for (j = 0; j < freq_size; j++) {
			if (cell_infos.cell_list[i].connection_status == CAM_CON_STATUS_PRIMARY_SERVING)
				 freq_rating = result->freq_ratings[j] * 10;
			else
				freq_rating = result->freq_ratings[j];

			freq_ratings_sums[j] += freq_rating;
			print_buf_cnt += snprintf(print_buf + print_buf_cnt, print_buf_size - print_buf_cnt, "%d : [%d], ", j, freq_rating);
		}

		CAM_INFO(CAM_SENSOR, "[AM_DBG] searching result : [0x%x,(%d-%d)]-> %s\n",
			result->rat_band, result->channel_min, result->channel_max, print_buf);

	}

	memset(print_buf, print_buf_size, 0);
	print_buf_cnt = 0;
	for (i = 0; i < freq_size; i++) {
		if (min > freq_ratings_sums[i]) {
			min = freq_ratings_sums[i];
			min_freq_idx = i;
		}

		print_buf_cnt += snprintf(print_buf + print_buf_cnt, print_buf_size - print_buf_cnt, "%d : [%d], ", i, freq_ratings_sums[i]);
	}

	cur_mipi_sensor_mode = &(s_ctrl->mipi_info[0]);
	sensor_type = cam_check_sensor_type(s_ctrl->sensordata->slave_info.sensor_id);

	CAM_INFO(CAM_SENSOR, "[AM_DBG] [Pos:%d, Mode:%d] final result: [%d], [%d], mipi ratings result : %s",
		sensor_type - 1, s_ctrl->sensor_mode,
		cur_mipi_sensor_mode->mipi_setting[min_freq_idx].mipi_rate,
		min_freq_idx,
		print_buf);

	CAM_DBG(CAM_SENSOR, "[AM_DBG] selected index : %d", min_freq_idx);

	return min_freq_idx;
}

int32_t cam_check_sensor_type(uint16_t sensor_id)
{
	int32_t sensor_type = INVALID;

	switch (sensor_id) {
		case SENSOR_ID_S5KGN3:
		case SENSOR_ID_S5KHP2:
		case SENSOR_ID_S5K2LD:
			sensor_type = WIDE;
			break;

		case SENSOR_ID_IMX374:
		case SENSOR_ID_S5K3J1:
#if !defined(CONFIG_SEC_Q6Q_PROJECT) && !defined(CONFIG_SEC_B6Q_PROJECT)
		case SENSOR_ID_S5K3LU:
#endif
 			sensor_type = FRONT;
			break;

		case SENSOR_ID_IMX564:
		case SENSOR_ID_IMX258:
#if defined(CONFIG_SEC_Q6Q_PROJECT) || defined(CONFIG_SEC_B6Q_PROJECT)
		case SENSOR_ID_S5K3LU:
#endif
			sensor_type = UW;
			break;

		case SENSOR_ID_S5K3K1:
		case SENSOR_ID_IMX754:
			sensor_type = TELE;
			break;

		case SENSOR_ID_IMX854:
			sensor_type = TELE2;
			break;

		case SENSOR_ID_IMX471:
		case SENSOR_ID_IMX596:
			sensor_type = FRONT_TOP;
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
	else if (sensor_type == TELE2) {
		CAM_INFO(CAM_SENSOR, "[AM_DBG] Tele2 sensor_mode : %d / %d", s_ctrl->sensor_mode, num_tele2_mipi_setting);
		if (s_ctrl->sensor_mode == 0) {
			s_ctrl->mipi_info = sensor_tele2_mipi_A_mode;
		} else if (s_ctrl->sensor_mode == 1 && s_ctrl->sensor_mode <= num_tele2_mipi_setting) {
			s_ctrl->mipi_info = sensor_tele2_mipi_B_mode;
		} else if (s_ctrl->sensor_mode == 2 && s_ctrl->sensor_mode <= num_tele2_mipi_setting) {
			s_ctrl->mipi_info = sensor_tele2_mipi_C_mode;
		} else if (s_ctrl->sensor_mode == 3 && s_ctrl->sensor_mode <= num_tele2_mipi_setting) {
			s_ctrl->mipi_info = sensor_tele2_mipi_D_mode;
		} else {
			s_ctrl->mipi_info = sensor_tele2_mipi_A_mode;
		}
	}
	else if (sensor_type == FRONT_TOP) {
		CAM_INFO(CAM_SENSOR, "[AM_DBG] Front_TOP sensor_mode : %d / %d", s_ctrl->sensor_mode, num_front_top_mipi_setting);
		if (s_ctrl->sensor_mode == 0) {
			s_ctrl->mipi_info = sensor_front_top_mipi_A_mode;
		} else if (s_ctrl->sensor_mode == 1 && s_ctrl->sensor_mode <= num_front_top_mipi_setting) {
			s_ctrl->mipi_info = sensor_front_top_mipi_B_mode;
		} else if (s_ctrl->sensor_mode == 2 && s_ctrl->sensor_mode <= num_front_top_mipi_setting) {
			s_ctrl->mipi_info = sensor_front_top_mipi_C_mode;
		} else if (s_ctrl->sensor_mode == 3 && s_ctrl->sensor_mode <= num_front_top_mipi_setting) {
			s_ctrl->mipi_info = sensor_front_top_mipi_D_mode;
		} else {
			s_ctrl->mipi_info = sensor_front_top_mipi_A_mode;
		}
	}
	else {
		CAM_ERR(CAM_SENSOR, "[AM_DBG] Not support sensor_type : %d", sensor_type);
		s_ctrl->mipi_info = sensor_wide_mipi_A_mode;
	}
	cur_mipi_sensor_mode = &(s_ctrl->mipi_info[0]);

	s_ctrl->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	s_ctrl->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
}

void cam_mipi_update_info(struct cam_sensor_ctrl_t *s_ctrl)
{
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	int found = -1;

	cur_mipi_sensor_mode = &(s_ctrl->mipi_info[0]);

	CAM_DBG(CAM_SENSOR, "[AM_DBG] cur rat : %d", cur_mipi_sensor_mode->mipi_cell_ratings->rat_band);
	CAM_DBG(CAM_SENSOR, "[AM_DBG] cur channel_min : %d", cur_mipi_sensor_mode->mipi_cell_ratings->channel_min);
	CAM_DBG(CAM_SENSOR, "[AM_DBG] cur channel_max : %d", cur_mipi_sensor_mode->mipi_cell_ratings->channel_max);

	found = cam_mipi_select_mipi_by_rf_cell_infos(s_ctrl,
				cur_mipi_sensor_mode->mipi_cell_ratings,
				cur_mipi_sensor_mode->mipi_cell_ratings_size,
				cur_mipi_sensor_mode->mipi_setting_size);

	if (found != -1) {
		if (found < cur_mipi_sensor_mode->mipi_setting_size) {
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

#if defined(CONFIG_CAMERA_RF_MIPI)
void get_rf_info(struct cam_cp_noti_cell_infos *cell_infos)
{
	cam_mipi_get_rf_cell_infos(cell_infos);

	for (int i = 0; i < cell_infos->num_cell; i++) {
		CAM_INFO(CAM_SENSOR, "[AM_DBG] get rf info [%d] [%d,%d,%d]",
			i,
			cell_infos->cell_list[i].rat,
			cell_infos->cell_list[i].band,
			cell_infos->cell_list[i].channel);
	}
}
#endif
#endif
