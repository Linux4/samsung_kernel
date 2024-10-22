// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Samsung Electronics Co., Ltd
 */

#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/types.h>

#include "kd_imgsensor_adaptive_mipi.h"

#if IS_ENABLED(CONFIG_CAMERA_ADAPTIVE_MIPI) && IS_ENABLED(CONFIG_DEV_RIL_BRIDGE)
#include <linux/dev_ril_bridge.h>
#endif

#include <linux/bsearch.h>

static struct mutex g_mipi_mutex;
static struct cam_cp_noti_cell_infos g_cp_noti_cell_infos;
static bool g_init_notifier;
static bool g_cp_test_noti_cell_flag;

/* CP notify format (HEX raw format)
 * 10 00 AA BB 27 05 03 CC CC CC CC
 * DD EE EE EE EE FF FF FF FF GG HH HH HH HH ... //cell 0
 * XX ... // cell 1
 * ...
 *
 * 00 10 (0x0010) - len
 * AA BB - not used
 * 27 - MAIN CMD (SYSTEM CMD : 0x27)
 * 05 - SUB CMD (CP Channel Info : 0x05)
 * 03 - NOTI CMD (0x03)
 * CC CC CC CC - NUM_CELL
 * --------- cell infos of NUM_CELL count
 * DD - RAT MODE
 * EE EE EE EE - BAND MODE
 * FF FF FF FF - CHANNEL NUMBER
 * GG - CELL_CONNECTION_STATUS
 * HH HH HH HH - BANDWIDTH_DOWNLINK
 * II II II II - ssSINR
 * .. .. .. .. - ssRSRP ssRSRQ CQI DL_MCS PUSCH_Power
 * --------- cell 0 end
 * XX -RAT MODE
 * ...
 */

#if IS_ENABLED(CONFIG_CAMERA_ADAPTIVE_MIPI) && IS_ENABLED(CONFIG_DEV_RIL_BRIDGE)
static int imgsensor_ril_notifier(struct notifier_block *nb,
				unsigned long size, void *buf)
{
	struct dev_ril_bridge_msg *msg;
	int data_size;
	int msg_data_size;
	int i;

	if (!g_init_notifier) {
		pr_warn("%s:init ril notifier not done\n", __func__);
		return NOTIFY_DONE;
	}

	if (g_cp_test_noti_cell_flag) {
		pr_info("%s: test flag is true, skip ril notifier", __func__);
		return NOTIFY_DONE;
	}

	pr_info("%s: ril notification size [%ld]\n", __func__, size);

	msg = (struct dev_ril_bridge_msg *)buf;

	pr_info("%s: msg size [%ld], msg->dev_id: %d\n", __func__, sizeof(struct dev_ril_bridge_msg), msg->dev_id);

	if (size == sizeof(struct dev_ril_bridge_msg)
				&& msg->dev_id == IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO) {
		data_size = (int)sizeof(struct cam_cp_cell_info);
		msg_data_size = msg->data_len - sizeof(g_cp_noti_cell_infos.num_cell);
		memcpy(&g_cp_noti_cell_infos, msg->data, sizeof(g_cp_noti_cell_infos.num_cell));
		pr_info("%s: g_cp_noti_cell_infos.num_cell: [%d]\n", __func__, g_cp_noti_cell_infos.num_cell);
		if (msg_data_size == data_size * g_cp_noti_cell_infos.num_cell) {
			mutex_lock(&g_mipi_mutex);
			memset(&g_cp_noti_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));
			memcpy(&g_cp_noti_cell_infos, msg->data, msg->data_len);
			mutex_unlock(&g_mipi_mutex);

			pr_info("%s: update mipi cell infos num_cell: %d\n",
				__func__, g_cp_noti_cell_infos.num_cell);

			for (i = 0; i < g_cp_noti_cell_infos.num_cell; i++) {
				pr_info("%s: update mipi cell info %d [%d,%d,%d,%d,%d,%d]\n",
					__func__, i, g_cp_noti_cell_infos.cell_list[i].rat,
					g_cp_noti_cell_infos.cell_list[i].band,
					g_cp_noti_cell_infos.cell_list[i].channel,
					g_cp_noti_cell_infos.cell_list[i].connection_status,
					g_cp_noti_cell_infos.cell_list[i].bandwidth,
					g_cp_noti_cell_infos.cell_list[i].sinr);
			}

			return NOTIFY_OK;
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block g_ril_notifier_block = {
	.notifier_call = imgsensor_ril_notifier,
};

void imgsensor_register_ril_notifier(void)
{
	if (!g_init_notifier) {
		pr_info("%s: register ril notifier\n", __func__);

		mutex_init(&g_mipi_mutex);
		memset(&g_cp_noti_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));

		register_dev_ril_bridge_event_notifier(&g_ril_notifier_block);
		g_init_notifier = true;
	}
}
#endif

int imgsensor_verify_mipi_cell_ratings(const struct cam_mipi_cell_ratings *channel_list,
										const int size, const int freq_size)
{
	int i, j;
	u16 pre_rat;
	u16 pre_band;
	u32 pre_channel_min;
	u32 pre_channel_max;
	u16 cur_rat;
	u16 cur_band;
	u32 cur_channel_min;
	u32 cur_channel_max;

	if (channel_list == NULL || size < 2)
		return 0;

	pre_rat = CAM_GET_RAT(channel_list[0].rat_band);
	pre_band = CAM_GET_BAND(channel_list[0].rat_band);
	pre_channel_min = channel_list[0].channel_min;
	pre_channel_max = channel_list[0].channel_max;

	if (pre_channel_min > pre_channel_max) {
		panic("min is bigger than max : index=%d, min=%d, max=%d", 0, pre_channel_min, pre_channel_max);
		return -EINVAL;
	}

	for (i = 1; i < size; i++) {
		cur_rat = CAM_GET_RAT(channel_list[i].rat_band);
		cur_band = CAM_GET_BAND(channel_list[i].rat_band);
		cur_channel_min = channel_list[i].channel_min;
		cur_channel_max = channel_list[i].channel_max;

		if (cur_channel_min > cur_channel_max) {
			panic("min is bigger than max : index=%d, min=%d, max=%d", 0, cur_channel_min, cur_channel_max);
			return -EINVAL;
		}

		if (pre_rat > cur_rat) {
			panic("not sorted rat : index=%d, pre_rat=%d, cur_rat=%d", i, pre_rat, cur_rat);
			return -EINVAL;
		}

		if (pre_rat == cur_rat) {
			if (pre_band > cur_band) {
				panic("not sorted band : index=%d, pre_band=%d, cur_band=%d", i, pre_band, cur_band);
				return -EINVAL;
			}

			if (pre_band == cur_band) {
				if (pre_channel_max >= cur_channel_min) {
					panic("overlaped channel range : index=%d, pre_ch=[%d-%d], cur_ch=[%d-%d]",
						i, pre_channel_min, pre_channel_max, cur_channel_min, cur_channel_max);
					return -EINVAL;
				}
			}
		}

		pre_rat = cur_rat;
		pre_band = cur_band;
		pre_channel_min = cur_channel_min;
		pre_channel_max = cur_channel_max;
	}

	for (i = 0; i < size; i++) {
		for (j = freq_size; j < CAM_MIPI_MAX_FREQ; j++) {
			if (channel_list[i].freq_ratings[j] != 0) {
				panic("write freq_ratings over freq_size=%d : index=%d, freq_ratings_index=%d, freq_ratings=%d",
					freq_size, i, j, channel_list[i].freq_ratings[j]);
				return -EINVAL;
			}
		}
	}

	return 0;
}

void imgsensor_get_rf_cell_infos(struct cam_cp_noti_cell_infos *cell_infos)
{
	if (!g_init_notifier) {
		pr_info("%s: not init ril notifier\n", __func__);
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

int imgsensor_select_mipi_by_rf_cell_infos(const struct cam_mipi_sensor_mode *mipi_sensor_mode)
{
	const struct cam_mipi_cell_ratings *channel_list = mipi_sensor_mode->mipi_cell_ratings;
	const int size = mipi_sensor_mode->mipi_cell_ratings_size;
	const int freq_size = mipi_sensor_mode->sensor_setting_size;
	struct cam_mipi_cell_ratings *result = NULL;
	struct cam_mipi_cell_ratings key;
	struct cam_cp_noti_cell_infos cell_infos;
	int i, j;
	int freq_ratings_sums[CAM_MIPI_MAX_FREQ] = {0,};
	int min = 0x7fffffff;
	int min_freq_idx = -1;
	char print_buf[128] = {0,};
	int print_buf_size = (int)sizeof(print_buf);
	int print_buf_cnt = 0;
	int freq_rating;

	imgsensor_get_rf_cell_infos(&cell_infos);

	pr_info("%s:adaptive mipi cell number %d", __func__, cell_infos.num_cell);

	for (i = 0; i < cell_infos.num_cell; i++) {
		key.rat_band = CAM_RAT_BAND(cell_infos.cell_list[i].rat, cell_infos.cell_list[i].band);
		key.channel_min = cell_infos.cell_list[i].channel;
		key.channel_max = cell_infos.cell_list[i].channel;

		pr_info("%s: searching rf channel s [%d,%d,%d]\n",
			__func__, cell_infos.cell_list[i].rat,
			cell_infos.cell_list[i].band, cell_infos.cell_list[i].channel);

		result = bsearch(&key,
				channel_list,
				size,
				sizeof(struct cam_mipi_cell_ratings),
				compare_rf_cell_ratings);

		if (result == NULL) {
			pr_info("%s: searching result : not found, skip this\n", __func__);
			continue;
		}

		memset(print_buf, print_buf_size, 0);
		print_buf_cnt = 0;

		for (j = 0; j < freq_size; j++) {
			if (cell_infos.cell_list[i].connection_status == CAM_CON_STATUS_PRIMARY_SERVING)
				freq_rating = result->freq_ratings[j] * CAM_MIPI_PCC_WEIGHT;
			else
				freq_rating = result->freq_ratings[j];

			freq_ratings_sums[j] += freq_rating;
			print_buf_cnt += snprintf(print_buf + print_buf_cnt, print_buf_size - print_buf_cnt,
					"%d : [%d], ", j, freq_rating);
		}

		pr_info("%s: searching result : [0x%x,(%d-%d)]-> %s\n", __func__,
			result->rat_band, result->channel_min, result->channel_max, print_buf);

	}

	memset(print_buf, print_buf_size, 0);
	print_buf_cnt = 0;
	for (i = 0; i < freq_size; i++) {
		if (min > freq_ratings_sums[i]) {
			min = freq_ratings_sums[i];
			min_freq_idx = i;
		}

		print_buf_cnt += snprintf(print_buf + print_buf_cnt, print_buf_size - print_buf_cnt,
				"%d : [%d], ", i, freq_ratings_sums[i]);
	}

	pr_info(" mode:%s final result: [%d], [%d], mipi ratings result : %s\n", mipi_sensor_mode->name,
		mipi_sensor_mode->sensor_setting[min_freq_idx].mipi_rate, min_freq_idx,
		print_buf);

	return min_freq_idx;
}
EXPORT_SYMBOL_GPL(imgsensor_select_mipi_by_rf_cell_infos);

static struct cam_cp_noti_cell_infos rf_cell_infos_on_error;

void imgsensor_save_rf_cell_infos_on_error(void)
{
	int i;

	imgsensor_get_rf_cell_infos(&rf_cell_infos_on_error);
	pr_info("[RF_MIPI] num_cell %d", rf_cell_infos_on_error.num_cell);

	for (i = 0; i < rf_cell_infos_on_error.num_cell; i++) {
		pr_info("[RF_MIPI] channel %d [%d,%d,%d,%d,%d]\n",
			i, rf_cell_infos_on_error.cell_list[i].rat, rf_cell_infos_on_error.cell_list[i].band,
			rf_cell_infos_on_error.cell_list[i].channel,
			rf_cell_infos_on_error.cell_list[i].connection_status,
			rf_cell_infos_on_error.cell_list[i].bandwidth);
	}
}
void imgsensor_get_rf_cell_infos_on_error(struct cam_cp_noti_cell_infos *rf_cell_infos)
{
	memcpy(rf_cell_infos, &rf_cell_infos_on_error, sizeof(struct cam_cp_noti_cell_infos));
}

void imgsensor_set_cp_test_cell(bool on)
{
	pr_info("[%s] set test flag :  %d", __func__, on);
	g_cp_test_noti_cell_flag = on;
}

void imgsensor_set_cp_test_cell_infos(const struct cam_cp_noti_cell_infos *test_cell_infos)
{
	int i;

	mutex_lock(&g_mipi_mutex);
	memset(&g_cp_noti_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));
	memcpy(&g_cp_noti_cell_infos, test_cell_infos, sizeof(struct cam_cp_noti_cell_infos));
	mutex_unlock(&g_mipi_mutex);

	pr_info("%s: update test mipi cell infos num_cell: %d\n",
		__func__, g_cp_noti_cell_infos.num_cell);

	for (i = 0; i < g_cp_noti_cell_infos.num_cell; i++) {
		pr_info("%s: update test mipi cell info %d [%d,%d,%d,%d,%d,%d]\n",
			__func__, i, g_cp_noti_cell_infos.cell_list[i].rat, g_cp_noti_cell_infos.cell_list[i].band,
			g_cp_noti_cell_infos.cell_list[i].channel, g_cp_noti_cell_infos.cell_list[i].connection_status,
			g_cp_noti_cell_infos.cell_list[i].bandwidth, g_cp_noti_cell_infos.cell_list[i].sinr);
	}
}
