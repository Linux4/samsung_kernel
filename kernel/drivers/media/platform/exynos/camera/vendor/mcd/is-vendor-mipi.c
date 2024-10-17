/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 mipi vendor functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <exynos-is-module.h>
#include "is-vendor-mipi.h"
#include "is-vendor.h"

#if defined(USE_CAMERA_ADAPTIVE_MIPI) && IS_ENABLED(CONFIG_DEV_RIL_BRIDGE)
#include <linux/dev_ril_bridge.h>
#endif

#include <linux/bsearch.h>

#include "is-device-sensor-peri.h"
#include "is-cis.h"

static struct mutex g_mipi_mutex;
static struct cam_cp_noti_cell_infos g_cp_noti_cell_infos;
static bool g_init_notifier;
static bool g_cp_test_noti_cell_flag;

/* CP notity format (HEX raw format)
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

#if defined(USE_CAMERA_ADAPTIVE_MIPI) && IS_ENABLED(CONFIG_DEV_RIL_BRIDGE)
static int is_vendor_ril_notifier(struct notifier_block *nb,
				unsigned long size, void *buf)
{
	struct dev_ril_bridge_msg *msg;
	int data_size;
	int msg_data_size;
	int i;

	if (!g_init_notifier) {
		warn("%s: not init ril notifier\n", __func__);
		return NOTIFY_DONE;
	}

	if (g_cp_test_noti_cell_flag) {
		info("%s: test flag is true, skip ril notifier", __func__);
		return NOTIFY_DONE;
	}

	info("%s: ril notification size [%ld]\n", __func__, size);

	msg = (struct dev_ril_bridge_msg *)buf;

	if (size == sizeof(struct dev_ril_bridge_msg)
				&& msg->dev_id == IPC_SYSTEM_CP_ADAPTIVE_MIPI_INFO) {
		data_size = (int)sizeof(struct cam_cp_cell_info);
		msg_data_size = msg->data_len - sizeof(g_cp_noti_cell_infos.num_cell);
		memcpy(&g_cp_noti_cell_infos, msg->data, sizeof(g_cp_noti_cell_infos.num_cell));

		if (msg_data_size == data_size * g_cp_noti_cell_infos.num_cell) {
			mutex_lock(&g_mipi_mutex);
			memset(&g_cp_noti_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));
			memcpy(&g_cp_noti_cell_infos, msg->data, msg->data_len);
			mutex_unlock(&g_mipi_mutex);

			info("%s: update mipi cell infos num_cell: %d\n",
				__func__, g_cp_noti_cell_infos.num_cell);

			for (i = 0; i < g_cp_noti_cell_infos.num_cell; i++) {
				info("%s: update mipi cell info %d [%d,%d,%d,%d,%d,%d]\n",
					__func__, i, g_cp_noti_cell_infos.cell_list[i].rat, g_cp_noti_cell_infos.cell_list[i].band,
					g_cp_noti_cell_infos.cell_list[i].channel, g_cp_noti_cell_infos.cell_list[i].connection_status,
					g_cp_noti_cell_infos.cell_list[i].bandwidth, g_cp_noti_cell_infos.cell_list[i].sinr);
			}

			return NOTIFY_OK;
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block g_ril_notifier_block = {
	.notifier_call = is_vendor_ril_notifier,
};

void is_vendor_register_ril_notifier(void)
{
	if (!g_init_notifier) {
		info("%s: register ril notifier\n", __func__);

		mutex_init(&g_mipi_mutex);
		memset(&g_cp_noti_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));

		register_dev_ril_bridge_event_notifier(&g_ril_notifier_block);
		g_init_notifier = true;
	}
}
#endif

int is_vendor_verify_mipi_cell_ratings(const struct cam_mipi_cell_ratings *channel_list,
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

int is_vendor_update_mipi_info(struct is_device_sensor *device)
{
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	int found = -1;
	struct is_cis *cis = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	int ret = 0;

	if (device == NULL) {
		err("device is NULL");
		return -1;
	}

	ret = is_sensor_g_module(device, &module);
	if (ret) {
		warn("%s sensor_g_module failed(%d)", __func__, ret);
		return -EINVAL;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	if (cis == NULL) {
		err("cis is NULL");
		return -1;
	}

	if (!cis->vendor_use_adaptive_mipi) {
		info("[pos:%d]vendor_use_adaptive_mipi false", device->position);
		return 0;
	}

	if (device->cfg->mode >= cis->mipi_sensor_mode_size) {
		err("sensor mode is out of bound");
		return -1;
	}

	cur_mipi_sensor_mode = &cis->mipi_sensor_mode[device->cfg->mode];

	if (cur_mipi_sensor_mode->mipi_cell_ratings_size == 0 ||
		cur_mipi_sensor_mode->mipi_cell_ratings == NULL) {
		dbg_sensor(1, "skip select mipi channel\n");
		return -1;
	}

	found = is_vendor_select_mipi_by_rf_cell_infos(device->position,
					cur_mipi_sensor_mode);

	if (found != -1) {
		if (found < cur_mipi_sensor_mode->sensor_setting_size) {
			device->cfg->mipi_speed = cur_mipi_sensor_mode->sensor_setting[found].mipi_rate;
			cis->mipi_clock_index_new = found;
			info("%s - [pos:%d]update mipi rate : [%d], [%d]\n", __func__,
				device->position,
				device->cfg->mipi_speed,
				cis->mipi_clock_index_new);
		} else {
			err("[pos:%d]sensor setting size is out of bound", device->position);
		}
	}

	return 0;
}
PST_KUNIT_EXPORT_SYMBOL(is_vendor_update_mipi_info);

int is_vendor_get_mipi_clock_string(struct is_device_sensor *device, char *cur_mipi_str)
{
	struct is_cis *cis = NULL;
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	int mode = 0;
	int ret = 0;

	cur_mipi_str[0] = '\0';

	if (device == NULL) {
		err("device is NULL");
		return -1;
	}

	ret = is_sensor_g_module(device, &module);
	if (ret) {
		warn("%s sensor_g_module failed(%d)", __func__, ret);
		return -EINVAL;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);
	if (cis == NULL) {
		err("[pos:%d]cis is NULL", device->position);
		return -1;
	}

	if (!cis->vendor_use_adaptive_mipi) {
		info("[pos:%d]vendor_use_adaptive_mipi false", device->position);
		return 0;
	}

	if (cis->cis_data->stream_on) {
		mode = cis->cis_data->sens_config_index_cur;

		if (mode >= cis->mipi_sensor_mode_size) {
			err("[pos:%d]sensor mode is out of bound", device->position);
			return -1;
		}

		cur_mipi_sensor_mode = &cis->mipi_sensor_mode[mode];

		if (cur_mipi_sensor_mode->sensor_setting_size == 0 ||
			cur_mipi_sensor_mode->sensor_setting == NULL) {
			err("[pos:%d]sensor_setting is not available", device->position);
			return -1;
		}

		if (cis->mipi_clock_index_new < 0 ||
			cur_mipi_sensor_mode->sensor_setting[cis->mipi_clock_index_new].str_mipi_clk == NULL) {
			err("[pos:%d]mipi_clock_index_new is not available", device->position);
			return -1;
		}

		sprintf(cur_mipi_str, "%s",
			cur_mipi_sensor_mode->sensor_setting[cis->mipi_clock_index_new].str_mipi_clk);
	}

	return 0;
}

int is_vendor_set_mipi_clock(struct is_device_sensor *device)
{
	int ret = 0;
	const struct cam_mipi_sensor_mode *cur_mipi_sensor_mode;
	int mode = 0;
	struct is_cis *cis = NULL;
	struct is_module_enum *module = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;

	if (device == NULL) {
		err("device is NULL");
		return -1;
	}

	ret = is_sensor_g_module(device, &module);
	if (ret) {
		warn("%s sensor_g_module failed(%d)", __func__, ret);
		return -EINVAL;
	}

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	cis = (struct is_cis *)v4l2_get_subdevdata(sensor_peri->subdev_cis);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	if (!cis->vendor_use_adaptive_mipi) {
		info("[pos:%d]vendor_use_adaptive_mipi false", device->position);
		return 0;
	}

	mode = cis->cis_data->sens_config_index_cur;

	dbg_sensor(1, "%s : [pos:%d]mipi_clock_index_cur(%d), new(%d)\n", __func__,
		device->position, cis->mipi_clock_index_cur, cis->mipi_clock_index_new);

	if (mode >= cis->mipi_sensor_mode_size) {
		err("[pos:%d]sensor mode is out of bound", device->position);
		return -1;
	}

	if (cis->mipi_clock_index_cur != cis->mipi_clock_index_new
		&& cis->mipi_clock_index_new >= 0) {
		cur_mipi_sensor_mode = &cis->mipi_sensor_mode[mode];

		if (cur_mipi_sensor_mode->sensor_setting == NULL) {
			dbg_sensor(1, "[pos:%d]no mipi setting for current sensor mode\n", device->position);
		} else if (cis->mipi_clock_index_new < cur_mipi_sensor_mode->sensor_setting_size) {
			info("%s: [pos:%d]change mipi clock [%d] : [%d], [%d]\n", __func__,
				device->position, mode,
				cur_mipi_sensor_mode->sensor_setting[cis->mipi_clock_index_new].mipi_rate,
				cis->mipi_clock_index_new);
			IXC_MUTEX_LOCK(cis->ixc_lock);
			sensor_cis_set_registers(cis->subdev,
				cur_mipi_sensor_mode->sensor_setting[cis->mipi_clock_index_new].setting,
				cur_mipi_sensor_mode->sensor_setting[cis->mipi_clock_index_new].setting_size);
			IXC_MUTEX_UNLOCK(cis->ixc_lock);
			cis->mipi_clock_index_cur = cis->mipi_clock_index_new;
		} else {
			err("[pos:%d]sensor setting index is out of bound %d %d",
				device->position, cis->mipi_clock_index_new, cur_mipi_sensor_mode->sensor_setting_size);
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(is_vendor_set_mipi_clock);

int is_vendor_set_mipi_mode(struct is_cis *cis)
{
	int ret = 0;
#if !defined(CONFIG_USE_SIGNED_BINARY)
	int i;
#endif

	if (!cis->vendor_use_adaptive_mipi) {
		info("[%s] vendor_use_adaptive_mipi is off", cis->sensor_info->name);
		return 0;
	}

	/* to use legacy mipi fucntions for backward compatibility */
	cis->mipi_sensor_mode = cis->sensor_info->mipi_mode;
	cis->mipi_sensor_mode_size = cis->sensor_info->mipi_mode_count;

#if !defined(CONFIG_USE_SIGNED_BINARY)
	for (i = 0; i < cis->mipi_sensor_mode_size; i++) {
		if (is_vendor_verify_mipi_cell_ratings(cis->mipi_sensor_mode[i].mipi_cell_ratings,
					cis->mipi_sensor_mode[i].mipi_cell_ratings_size,
					cis->mipi_sensor_mode[i].sensor_setting_size)) {
			panic("[%s] wrong mipi channel", cis->sensor_info->name);
			break;
		}
	}
#endif

	return ret;
}
EXPORT_SYMBOL_GPL(is_vendor_set_mipi_mode);

void is_vendor_get_rf_cell_infos(struct cam_cp_noti_cell_infos *cell_infos)
{
	if (!g_init_notifier) {
		warn("%s: not init ril notifier\n", __func__);
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

int is_vendor_select_mipi_by_rf_cell_infos(const int position,
										const struct cam_mipi_sensor_mode *mipi_sensor_mode)
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

	is_vendor_get_rf_cell_infos(&cell_infos);

	info("%s: cell number %d", __func__, cell_infos.num_cell);

	for (i = 0; i < cell_infos.num_cell; i++) {
		key.rat_band = CAM_RAT_BAND(cell_infos.cell_list[i].rat, cell_infos.cell_list[i].band);
		key.channel_min = cell_infos.cell_list[i].channel;
		key.channel_max = cell_infos.cell_list[i].channel;

		info("%s: searching rf channel s [%d,%d,%d]\n",
			__func__, cell_infos.cell_list[i].rat,
			cell_infos.cell_list[i].band, cell_infos.cell_list[i].channel);

		result = bsearch(&key,
				channel_list,
				size,
				sizeof(struct cam_mipi_cell_ratings),
				compare_rf_cell_ratings);

		if (result == NULL) {
			info("%s: searching result : not found, skip this\n", __func__);
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
			print_buf_cnt += snprintf(print_buf + print_buf_cnt, print_buf_size - print_buf_cnt, "%d : [%d], ", j, freq_rating);
		}

		info("%s: searching result : [0x%x,(%d-%d)]-> %s\n", __func__,
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

	info("%s: [Pos:%d, mode:%s] final result: [%d], [%d], mipi ratings result : %s\n",
		__func__, position, mipi_sensor_mode->name,
		mipi_sensor_mode->sensor_setting[min_freq_idx].mipi_rate, min_freq_idx,
		print_buf);

	return min_freq_idx;
}

static struct cam_cp_noti_cell_infos rf_cell_infos_on_error;

void is_vendor_save_rf_cell_infos_on_error(void)
{
	int i;
	is_vendor_get_rf_cell_infos(&rf_cell_infos_on_error);
	info("[RF_MIPI] num_cell %d", rf_cell_infos_on_error.num_cell);

	for (i = 0; i < rf_cell_infos_on_error.num_cell; i++) {
		info("[RF_MIPI] channel %d [%d,%d,%d,%d,%d]\n",
			i, rf_cell_infos_on_error.cell_list[i].rat, rf_cell_infos_on_error.cell_list[i].band, rf_cell_infos_on_error.cell_list[i].channel,
			rf_cell_infos_on_error.cell_list[i].connection_status, rf_cell_infos_on_error.cell_list[i].bandwidth);
	}
}
void is_vendor_get_rf_cell_infos_on_error(struct cam_cp_noti_cell_infos *rf_cell_infos)
{
	memcpy(rf_cell_infos, &rf_cell_infos_on_error, sizeof(struct cam_cp_noti_cell_infos));
}

void is_vendor_set_cp_test_cell(bool on)
{
	info("[%s] set test flag :  %d", __func__, on);
	g_cp_test_noti_cell_flag = on;
}

void is_vendor_set_cp_test_cell_infos(const struct cam_cp_noti_cell_infos *test_cell_infos)
{
	int i;

	mutex_lock(&g_mipi_mutex);
	memset(&g_cp_noti_cell_infos, 0, sizeof(struct cam_cp_noti_cell_infos));
	memcpy(&g_cp_noti_cell_infos, test_cell_infos, sizeof(struct cam_cp_noti_cell_infos));
	mutex_unlock(&g_mipi_mutex);

	info("%s: update test mipi cell infos num_cell: %d\n",
		__func__, g_cp_noti_cell_infos.num_cell);

	for (i = 0; i < g_cp_noti_cell_infos.num_cell; i++) {
		info("%s: update test mipi cell info %d [%d,%d,%d,%d,%d,%d]\n",
			__func__, i, g_cp_noti_cell_infos.cell_list[i].rat, g_cp_noti_cell_infos.cell_list[i].band,
			g_cp_noti_cell_infos.cell_list[i].channel, g_cp_noti_cell_infos.cell_list[i].connection_status,
			g_cp_noti_cell_infos.cell_list[i].bandwidth, g_cp_noti_cell_infos.cell_list[i].sinr);
	}
}
