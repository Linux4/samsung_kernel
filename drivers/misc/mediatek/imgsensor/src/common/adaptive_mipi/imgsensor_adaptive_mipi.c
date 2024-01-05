/*
 * Copyright (C) 2011 Samsung Electronics, Co. Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define PFX "Adaptive_mipi D/D"

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
#include <linux/dev_ril_bridge.h>
#include <linux/bsearch.h>

static struct cam_cp_noti_info g_cp_noti_info;
static struct cam_cp_noti_info g_cp_noti_5g_info;
static struct notifier_block g_ril_notifier_block;
static struct mutex g_mipi_mutex;
static bool g_init_notifier;

static void imgsensor_get_rf_channel(struct cam_cp_noti_info *ch);
static void imgsensor_get_5g_rf_channel(struct cam_cp_noti_info *ch);
static int imgsensor_compare_rf_channel(const void *key, const void *element);

#define LOG_DBG(format, args...) pr_debug(PFX format, ##args)
#define LOG_INF(format, args...) pr_info(PFX format, ##args)
#define LOG_ERR(format, args...) pr_err(PFX format, ##args)

int imgsensor_register_ril_notifier(void)
{
	int ret = -1;

	LOG_INF("%s: register ril notifier - E\n", __func__);
	if (!g_init_notifier) {
		LOG_INF("%s: register ril notifier\n", __func__);

		mutex_init(&g_mipi_mutex);
		memset(&g_cp_noti_info, 0, sizeof(struct cam_cp_noti_info));
		memset(&g_cp_noti_5g_info, 0, sizeof(struct cam_cp_noti_info));

		ret = register_dev_ril_bridge_event_notifier(&g_ril_notifier_block);
		if (ret < 0)
			g_init_notifier = false;
		else
			g_init_notifier = true;
	}
	LOG_INF("%s: register ril notifier - X\n", __func__);
	return 0;
}

int imgsensor_print_cp_info(void)
{
	LOG_INF("%s, CP info [rat:%d, band:%d, channel:%d]\n",
		__func__, g_cp_noti_info.rat, g_cp_noti_info.band, g_cp_noti_info.channel);
	return 0;
}

int imgsensor_select_mipi_by_rf_channel(const struct cam_mipi_channel *channel_list, const int size)
{
	struct cam_mipi_channel *result = NULL;
	struct cam_mipi_channel key;
	struct cam_cp_noti_info input_ch;

	imgsensor_get_rf_channel(&input_ch);

	key.rat_band = CAM_RAT_BAND(input_ch.rat, input_ch.band);
	key.channel_min = input_ch.channel;
	key.channel_max = input_ch.channel;

	LOG_INF("%s: searching rf channel s [%d,%d,%d]\n",
		__func__, input_ch.rat, input_ch.band, input_ch.channel);

	result = bsearch(&key,
			channel_list,
			size,
			sizeof(struct cam_mipi_channel),
			imgsensor_compare_rf_channel);

	if (result == NULL) {
		if (input_ch.rat == CAM_RAT_7_NR5G) {		/* EN-DC case */
			LOG_INF("%s: not found for NR, retry for 5G RAT\n", __func__);
			imgsensor_get_5g_rf_channel(&input_ch);

			key.rat_band = CAM_RAT_BAND(input_ch.rat, input_ch.band);
			key.channel_min = input_ch.channel;
			key.channel_max = input_ch.channel;

			LOG_INF("%s: searching 5G rf channel s [%d,%d,%d]\n",
				__func__, input_ch.rat, input_ch.band, input_ch.channel);

			result = bsearch(&key,
					channel_list,
					size,
					sizeof(struct cam_mipi_channel),
					imgsensor_compare_rf_channel);
			if (result == NULL) {
				LOG_INF("%s: searching result : not found, use default mipi clock\n", __func__);
				return -1;
			}
		} else {
			LOG_INF("%s: searching result : not found, use default mipi clock\n", __func__);
			return -1;
		}
	}

	LOG_INF("%s: searching result : [0x%x,(%d-%d)]->(%d)\n", __func__,
		result->rat_band, result->channel_min, result->channel_max, result->setting_index);

	return result->setting_index;
}

/* CP notify format (HEX raw format)
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
static int imgsensor_ril_notifier(struct notifier_block *nb, unsigned long size, void *buf)
{
	struct dev_ril_bridge_msg *msg;
	struct cam_cp_noti_info *cp_noti_info;

	if (!g_init_notifier) {
		pr_warn("%s: not init ril notifier\n", __func__);
		return NOTIFY_DONE;
	}

	LOG_INF("%s: ril notification size [%ld]\n", __func__, size);

	msg = (struct dev_ril_bridge_msg *)buf;
	if (size == sizeof(struct dev_ril_bridge_msg)
			&& msg->dev_id == IPC_SYSTEM_CP_CHANNEL_INFO
			&& msg->data_len == sizeof(struct cam_cp_noti_info)) {
		cp_noti_info = (struct cam_cp_noti_info *)msg->data;

		mutex_lock(&g_mipi_mutex);
		memcpy(&g_cp_noti_info, msg->data, sizeof(struct cam_cp_noti_info));
		if (g_cp_noti_info.rat != CAM_RAT_7_NR5G)
			memcpy(&g_cp_noti_5g_info, &g_cp_noti_info, sizeof(struct cam_cp_noti_info));
		mutex_unlock(&g_mipi_mutex);

		LOG_INF("%s: update mipi channel [%d,%d,%d]\n",
			__func__, g_cp_noti_info.rat, g_cp_noti_info.band, g_cp_noti_info.channel);

		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static struct notifier_block g_ril_notifier_block = {
	.notifier_call = imgsensor_ril_notifier,
};

static void imgsensor_get_rf_channel(struct cam_cp_noti_info *ch)
{
	if (!g_init_notifier) {
		pr_warn("%s: not init ril notifier\n", __func__);
		memset(ch, 0, sizeof(struct cam_cp_noti_info));
		return;
	}

	mutex_lock(&g_mipi_mutex);
	memcpy(ch, &g_cp_noti_info, sizeof(struct cam_cp_noti_info));
	mutex_unlock(&g_mipi_mutex);
}

static void imgsensor_get_5g_rf_channel(struct cam_cp_noti_info *ch)
{
	if (!g_init_notifier) {
		pr_warn("%s: not init ril notifier\n", __func__);
		memset(ch, 0, sizeof(struct cam_cp_noti_info));
		return;
	}

	mutex_lock(&g_mipi_mutex);
	memcpy(ch, &g_cp_noti_5g_info, sizeof(struct cam_cp_noti_info));
	mutex_unlock(&g_mipi_mutex);
}

static int imgsensor_compare_rf_channel(const void *key, const void *element)
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

