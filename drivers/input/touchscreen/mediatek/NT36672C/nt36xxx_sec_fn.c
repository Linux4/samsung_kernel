/*
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */


#include <linux/delay.h>

#include "nt36xxx.h"
#if SEC_TOUCH_CMD

#define SHOW_NOT_SUPPORT_CMD	0x00

#define NORMAL_MODE		0x00
#define TEST_MODE_1		0x21
#define TEST_MODE_2		0x22
#define MP_MODE_CC		0x41
#define FREQ_HOP_DISABLE	0x66
#define FREQ_HOP_ENABLE		0x65
#define HANDSHAKING_HOST_READY	0xBB

#define GLOVE_ENTER				0xB1
#define GLOVE_LEAVE				0xB2
#define HOLSTER_ENTER			0xB5
#define HOLSTER_LEAVE			0xB6
#define EDGE_REJ_VERTICLE_MODE	0xBA
#define EDGE_REJ_LEFT_UP_MODE	0xBB
#define EDGE_REJ_RIGHT_UP_MODE	0xBC
#define BLOCK_AREA_ENTER		0x71
#define BLOCK_AREA_LEAVE		0x72
#define EDGE_AREA_ENTER			0x73 //min:7
#define EDGE_AREA_LEAVE			0x74 //0~6
#define HOLE_AREA_ENTER			0x75 //0~120
#define HOLE_AREA_LEAVE			0x76 //no report
#define SPAY_SWIPE_ENTER		0x77
#define SPAY_SWIPE_LEAVE		0x78
#define DOUBLE_CLICK_ENTER		0x79
#define DOUBLE_CLICK_LEAVE		0x7A
#define SENSITIVITY_ENTER		0x7B
#define SENSITIVITY_LEAVE		0x7C
#define EXTENDED_CUSTOMIZED_CMD	0x7F
typedef enum {
	SET_GRIP_EXECPTION_ZONE = 1,
	SET_GRIP_PORTRAIT_MODE = 2,
	SET_GRIP_LANDSCAPE_MODE = 3,
	SET_TOUCH_DEBOUNCE = 5,	// for SIP
	SET_GAME_MODE = 6,
	SET_HIGH_SENSITIVITY_MODE = 7,
} EXTENDED_CUSTOMIZED_CMD_TYPE;

typedef enum {
	DEBOUNCE_NORMAL = 0,
	DEBOUNCE_LOWER = 1,	//to optimize tapping performance for SIP
} TOUCH_DEBOUNCE;

typedef enum {
	GAME_MODE_DISABLE = 0,
	GAME_MODE_ENABLE = 1,
} GAME_MODE;

typedef enum {
	HIGH_SENSITIVITY_DISABLE = 0,
	HIGH_SENSITIVITY_ENABLE = 1,
} HIGH_SENSITIVITY_MODE;

#define BUS_TRANSFER_LENGTH  256

#define XDATA_SECTOR_SIZE	256

#define CMD_RESULT_WORD_LEN	10

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))


/* function bit combination code */
#define EDGE_REJ_VERTICLE	1
#define EDGE_REJ_LEFT_UP	2
#define EDGE_REJ_RIGHT_UP	3

#define ORIENTATION_0		0
#define ORIENTATION_90		1
#define ORIENTATION_180		2
#define ORIENTATION_270		3

#define OPEN_SHORT_TEST		1
#define CHECK_ONLY_OPEN_TEST	1
#define CHECK_ONLY_SHORT_TEST	2

typedef enum {
	GLOVE = 1,
	HOLSTER = 4,
	EDGE_REJECT_L = 6,
	EDGE_REJECT_H,
	EDGE_PIXEL,
	HOLE_PIXEL,
	SPAY_SWIPE,
	DOUBLE_CLICK,
	SENSITIVITY,
	BLOCK_AREA,
	FUNCT_MAX,
} FUNCT_BIT;

typedef enum {
	GLOVE_MASK		= 0x0002,	// bit 1
	HOLSTER_MASK = 0x0010,	//bit 4
	EDGE_REJECT_MASK 	= 0x00C0,	// bit [6|7]
	EDGE_PIXEL_MASK		= 0x0100,	// bit 8
	HOLE_PIXEL_MASK		= 0x0200,	// bit 9
	SPAY_SWIPE_MASK		= 0x0400,	// bit 10
	DOUBLE_CLICK_MASK	= 0x0800,	// bit 11
	SENSITIVITY_MASK	= 0x4000,	// bit 14
	BLOCK_AREA_MASK		= 0x1000,	// bit 12
	NOISE_MASK			= 0x2000,
} FUNCT_MASK;

enum {
	BUILT_IN = 0,
	UMS,
	NONE,
	SPU,
};

u16 landscape_deadzone[2] = { 0 };

int nvt_get_fw_info(void);

int nvt_ts_fw_update_from_bin(struct nvt_ts_data *ts);
int nvt_ts_fw_update_from_mp_bin(struct nvt_ts_data *ts, bool is_start);
int nvt_ts_fw_update_from_ums(struct nvt_ts_data *ts);


//int nvt_ts_resume_pd(struct nvt_ts_data *ts);
int nvt_get_checksum(struct nvt_ts_data *ts, u8 *csum_result, u8 csum_size);

int32_t nvt_set_page(uint32_t addr);

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct nvt_ts_data *ts = container_of(sec, struct nvt_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode;

	sec_cmd_set_default_result(sec);

	if (!ts->platdata->enable_settings_aot) {
		input_err(true, &ts->client->dev, "%s: aot feature not support %d\n",
			__func__, ts->platdata->enable_settings_aot);
		goto out;
	} else if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		goto out;
	} else {
		mode = sec->cmd_param[0] ? DOUBLE_CLICK_ENTER : DOUBLE_CLICK_LEAVE;
		if (sec->cmd_param[0] == 1) {
			ts->aot_enable |= LPM_DC;
			gesture_flag = 1;
		} else {
			ts->aot_enable &= (u8)(~LPM_DC);
			gesture_flag = 0;
		}
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s %s\n", __func__, (ts->aot_enable & LPM_DC)?"ON":"OFF", buff);

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s %s\n", __func__, (ts->aot_enable & LPM_DC)?"ON":"OFF", buff);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 feature = 0;

	//if (ts->platdata->enable_settings_aot)
	feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	//input_info(true, &ts->client->dev, "%s: %d%s\n",
	//			__func__, feature,
	//			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d\n", feature);
}

static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);

static struct attribute *cmd_attributes[] = {
	&dev_attr_support_feature.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

int nvt_sec_mp_parse_dt(struct nvt_ts_data *ts, const char *node_compatible)
{
	struct device_node *np = ts->client->dev.of_node;
	struct device_node *child = NULL;

	/* find each MP sub-nodes */
	for_each_child_of_node(np, child) {
		/* find the specified node */
		if (of_device_is_compatible(child, node_compatible)) {
			np = child;
			break;
		}
	}

	if (!child) {
		input_err(true, &ts->client->dev, "%s: do not find mp criteria for %s\n",
			  __func__, node_compatible);
		return -EINVAL;
	}

	/* MP Criteria */
	if (of_property_read_u32_array(np, "open_test_spec", ts->platdata->open_test_spec, 2))
		return -EINVAL;

	if (of_property_read_u32_array(np, "short_test_spec", ts->platdata->short_test_spec, 2))
		return -EINVAL;

	if (of_property_read_u32(np, "diff_test_frame", &ts->platdata->diff_test_frame))
		return -EINVAL;

	input_info(true, &ts->client->dev, "%s: parse mp criteria for %s\n", __func__, node_compatible);

	return 0;
}

int nvt_ts_sec_fn_init(struct nvt_ts_data *ts)
{
	int ret;

	ret = sec_cmd_init(&ts->sec, sec_cmds, ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to sec cmd init\n",
			__func__);
		return ret;
	}

	ret = sysfs_create_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: failed to create sysfs attributes\n",
			__func__);
			goto out;
	}

	ret = sysfs_create_link(&ts->sec.fac_dev->kobj, &ts->input_dev->dev.kobj, "input");
	if (ret) {
		input_err(true, &ts->client->dev, "%s: failed to creat sysfs link\n",
			__func__);
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		goto out;
	}

	input_info(true, &ts->client->dev, "%s\n", __func__);

	return 0;

out:
	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);

	return ret;
}

void nvt_ts_sec_fn_remove(struct nvt_ts_data *ts)
{
	sysfs_delete_link(&ts->sec.fac_dev->kobj, &ts->input_dev->dev.kobj, "input");

	sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);

	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
}
#endif
