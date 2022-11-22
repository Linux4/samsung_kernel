/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2015 MELFAS Inc.
 *
 *
 * mip4_sec.c : Command functions (Optional)
 *
 */

#include "mip4.h"

#if MIP_USE_CMD

#define NAME_OF_UNKNOWN_CMD "not_support_cmd"

enum CMD_STATUS {
	CMD_STATUS_WAITING = 0,
	CMD_STATUS_RUNNING,
	CMD_STATUS_OK,
	CMD_STATUS_FAIL,
	CMD_STATUS_NONE,
};

/**
* Clear command result
*/
static void cmd_clear_result(struct mip_ts_info *info)
{
	char delim = ':';
	memset(info->cmd_result, 0x00, ARRAY_SIZE(info->cmd_result));
	memcpy(info->cmd_result, info->cmd, strnlen(info->cmd, CMD_LEN));
	strncat(info->cmd_result, &delim, 1);
}

/**
* Set command result
*/
static void cmd_set_result(struct mip_ts_info *info, char *buf, int len)
{
	strncat(info->cmd_result, buf, len);
}

/**
* Command : Update firmware
*/
static void cmd_fw_update(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int fw_location = info->cmd_param[0];

	cmd_clear_result(info);

	switch (fw_location) {
	case 0:
		if (mip_fw_update_from_kernel(info, true))
			goto ERROR;
		break;
	case 1:
		if (mip_fw_update_from_storage(info, info->pdata->ext_fw_name, true))
			goto ERROR;
		break;
	default:
		goto ERROR;
		break;
	}

	mip_init_config(info);

	sprintf(buf, "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;
	goto EXIT;

ERROR:
	sprintf(buf, "%s", "NG");
	info->cmd_state = CMD_STATUS_FAIL;
	goto EXIT;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get firmware version from file
*/
static void cmd_get_fw_ver_bin(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	const struct firmware *fw;
	char buf[64] = { 0 };

	cmd_clear_result(info);

	request_firmware(&fw, info->pdata->fw_name, &info->client->dev);
	if (!fw) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	sprintf(buf, "ME%02X%04X\n", fw->data[fw->size - 31], *(u16 *)&fw->data[fw->size - 42]);
	info->cmd_state = CMD_STATUS_OK;

	release_firmware(fw);

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get firmware version from chip
*/
static void cmd_get_fw_ver_ic(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };
	char *offset;
	u8 vendor_id[2];
	u8 hw_category[2];
	u8 param_ver[2];

	cmd_clear_result(info);

	offset = buf;

	if (mip_get_vendor_id(info, vendor_id)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(offset, sizeof(buf), "%s", vendor_id);
	offset += sizeof(vendor_id);

	if (mip_get_hw_category(info, hw_category)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;

	}

	sprintf(offset, "%02X", *hw_category);
	offset += sizeof(hw_category);

	if (mip_get_parameter_version(info, param_ver)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	sprintf(offset, "%02X%02X\n",  param_ver[0], param_ver[1]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get chip vendor
*/
static void cmd_get_chip_vendor(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	cmd_clear_result(info);

	sprintf(buf, "MELFAS");
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get chip name
*/
static void cmd_get_chip_name(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	cmd_clear_result(info);

	sprintf(buf, CHIP_NAME);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get X axis line num
*/
static void cmd_get_x_num(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_NODE_NUM_Y;	/* Factory App X = Node Num Y */
	if (mip_i2c_read(info, wbuf, 2, rbuf, 1)) {
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

EXIT:
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get Y axis line num
*/
static void cmd_get_y_num(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_NODE_NUM_X;	/* Factory App Y = Node Num X */
	if (mip_i2c_read(info, wbuf, 2, rbuf, 1)) {
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

EXIT:
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get X resolution
*/
static void cmd_get_max_x(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_X;
	if (mip_i2c_read(info, wbuf, 2, rbuf, 2)) {
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (rbuf[1] << 8) | rbuf[0];

	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

EXIT:
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get Y resolution
*/
static void cmd_get_max_y(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_RESOLUTION_Y;
	if (mip_i2c_read(info, wbuf, 2, rbuf, 2)) {
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (rbuf[1] << 8) | rbuf[0];

	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

EXIT:
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Power off
*/
static void cmd_module_off_master(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	cmd_clear_result(info);

	if (mip_power_off(info)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	sprintf(buf, "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Power on
*/
static void cmd_module_on_master(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	cmd_clear_result(info);

	if (mip_power_on(info)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	sprintf(buf, "%s", "OK");
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Read intensity image
*/
static void cmd_read_intensity(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int min = 999999;
	int max = -999999;
	int i = 0;

	cmd_clear_result(info);

	if (mip_get_image(info, MIP_IMG_TYPE_INTENSITY)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	for (i = 0; i < (info->node_x * info->node_y); i++) {
		if (info->image_buf[i] > max)
			max = info->image_buf[i];
		if (info->image_buf[i] < min)
			min = info->image_buf[i];
	}

	sprintf(buf, "%d,%d", min, max);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get intensity data
*/
static void cmd_get_intensity(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int app_x = info->cmd_param[0];
	int app_y = info->cmd_param[1];

	int tsp_x = app_y;
	int tsp_y = app_x;

	int tsp_x_num = info->node_x;
	int tsp_y_num = info->node_y;

	int idx = 0;

	cmd_clear_result(info);

	if ((tsp_x < 0) || (tsp_x >= tsp_x_num) || (tsp_y < 0) || (tsp_y >= tsp_y_num)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = (tsp_x_num * tsp_y) + tsp_x;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Read rawdata image
*/
static void cmd_read_rawdata(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int min = 999999;
	int max = -999999;
	int i = 0;

	cmd_clear_result(info);

	if (mip_get_image(info, MIP_IMG_TYPE_RAWDATA)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	for (i = 0; i < (info->node_x * info->node_y); i++) {
		if (info->image_buf[i] > max)
			max = info->image_buf[i];
		if (info->image_buf[i] < min)
			min = info->image_buf[i];
	}

	sprintf(buf, "%d,%d", min, max);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get rawdata
*/
static void cmd_get_rawdata(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int app_x = info->cmd_param[0];
	int app_y = info->cmd_param[1];

	int tsp_x = app_y;
	int tsp_y = app_x;

	int tsp_x_num = info->node_x;
	int tsp_y_num = info->node_y;

	int idx = 0;

	cmd_clear_result(info);

	if ((tsp_x < 0) || (tsp_x >= tsp_x_num) || (tsp_y < 0) || (tsp_y >= tsp_y_num)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = (tsp_x_num * tsp_y) + tsp_x;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Run cm delta test
*/
static void cmd_run_test_cm_delta(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int min = 999999;
	int max = -999999;
	int i = 0;

	cmd_clear_result(info);

	if (mip_run_test(info, MIP_TEST_TYPE_CM_DELTA)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	for (i = 0; i < (info->node_x * info->node_y); i++) {
		if (info->image_buf[i] > max)
			max = info->image_buf[i];
		if (info->image_buf[i] < min)
			min = info->image_buf[i];
	}

	sprintf(buf, "%d,%d", min, max);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_info(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get result of cm delta test
*/
static void cmd_get_cm_delta(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int app_x = info->cmd_param[0];
	int app_y = info->cmd_param[1];

	int tsp_x = app_y;
	int tsp_y = app_x;

	int tsp_x_num = info->node_x;
	int tsp_y_num = info->node_y;

	int idx = 0;

	cmd_clear_result(info);

	if ((tsp_x < 0) || (tsp_x >= tsp_x_num) || (tsp_y < 0) || (tsp_y >= tsp_y_num)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = (tsp_x_num * tsp_y) + tsp_x;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_info(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get result of dummy cm delta test
*/
static void cmd_get_dummy_cm_delta(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };
	int idx = info->node_x * info->node_y + 2;

	cmd_clear_result(info);

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_info(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get result of cm delta test (key)
*/
static void cmd_get_cm_delta_key(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int key_idx = info->cmd_param[0];
	int idx = 0;

	cmd_clear_result(info);

	if ((key_idx < 0) || (key_idx >= info->key_num)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = info->node_x * info->node_y + key_idx;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Run cm abs test
*/
static void cmd_run_test_cm_abs(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int min = 999999;
	int max = -999999;
	int i = 0;

	cmd_clear_result(info);

	if (mip_run_test(info, MIP_TEST_TYPE_CM_ABS)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	for (i = 0; i < (info->node_x * info->node_y); i++) {
		if (info->image_buf[i] > max)
			max = info->image_buf[i];
		if (info->image_buf[i] < min)
			min = info->image_buf[i];
	}

	sprintf(buf, "%d,%d", min, max);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get result of cm abs test
*/
static void cmd_get_cm_abs(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int app_x = info->cmd_param[0];
	int app_y = info->cmd_param[1];

	int tsp_x = app_y;
	int tsp_y = app_x;

	int tsp_x_num = info->node_x;
	int tsp_y_num = info->node_y;

	int idx = 0;

	cmd_clear_result(info);

	if ((tsp_x < 0) || (tsp_x >= tsp_x_num) || (tsp_y < 0) || (tsp_y >= tsp_y_num)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = (tsp_x_num * tsp_y) + tsp_x;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get result of cm abs test (key)
*/
static void cmd_get_cm_abs_key(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int key_idx = info->cmd_param[0];
	int idx = 0;

	cmd_clear_result(info);

	if ((key_idx < 0) || (key_idx >= info->key_num)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = info->node_x * info->node_y + key_idx;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Run cm jitter test
*/
static void cmd_run_test_cm_jitter(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int min = 999999;
	int max = -999999;
	int i = 0;

	cmd_clear_result(info);

	if (mip_run_test(info, MIP_TEST_TYPE_CM_JITTER)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	for (i = 0; i < (info->node_x * info->node_y); i++) {
		if (info->image_buf[i] > max)
			max = info->image_buf[i];
		if (info->image_buf[i] < min)
			min = info->image_buf[i];
	}

	sprintf(buf, "%d,%d", min, max);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get result of cm jitter test
*/
static void cmd_get_cm_jitter(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };

	int app_x = info->cmd_param[0];
	int app_y = info->cmd_param[1];

	int tsp_x = app_y;
	int tsp_y = app_x;

	int tsp_x_num = info->node_x;
	int tsp_y_num = info->node_y;

	int idx = 0;

	cmd_clear_result(info);

	if ((tsp_x < 0) || (tsp_x >= tsp_x_num) || (tsp_y < 0) || (tsp_y >= tsp_y_num)) {
		sprintf(buf, "%s", "NG");
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	idx = (tsp_x_num * tsp_y) + tsp_x;

	sprintf(buf, "%d", info->image_buf[idx]);
	info->cmd_state = CMD_STATUS_OK;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Glove mode
*/
static void cmd_glove_mode(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 wbuf[8];
	u8 val = info->cmd_param[0];

	cmd_clear_result(info);

	if ((val == 0) || (val == 1)) {
		wbuf[0] = MIP_R0_CTRL;
		wbuf[1] = MIP_R1_CTRL_HIGH_SENS_MODE;
		wbuf[2] = val;
		if (mip_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
			goto ERROR;
		}

		sprintf(buf, "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;

		goto EXIT;
	}

ERROR:
	sprintf(buf, "%s", "NG");
	info->cmd_state = CMD_STATUS_FAIL;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Edge (extension) mode
*/
static void cmd_edge_mode(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 wbuf[8];
	u8 val = info->cmd_param[0];

	cmd_clear_result(info);

	if ((val == 0) || (val == 1)) {
		wbuf[0] = MIP_R0_CTRL;
		wbuf[1] = MIP_R1_CTRL_EDGE_CORRECTION;
		wbuf[2] = !(val) ? 2 : 0;
		if (mip_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip_i2c_write\n", __func__);
			goto ERROR;
		}

		sprintf(buf, "%s", "OK");
		info->cmd_state = CMD_STATUS_OK;

		goto EXIT;
	}

ERROR:
	sprintf(buf, "%s", "NG");
	info->cmd_state = CMD_STATUS_FAIL;

EXIT:
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Get contact threshold (screen)
*/
static void cmd_get_threshold(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;

	cmd_clear_result(info);

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_CONTACT_THD_SCR;
	if (mip_i2c_read(info, wbuf, 2, rbuf, 2)) {
		info->cmd_state = CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (rbuf[1] << 8) | rbuf[0];

	sprintf(buf, "%d", val);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	info->cmd_state = CMD_STATUS_OK;

EXIT:
	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

/**
* Command : Unknown cmd
*/
static void cmd_unknown_cmd(void *device_data)
{
	struct mip_ts_info *info = (struct mip_ts_info *)device_data;
	char buf[16] = { 0 };

	cmd_clear_result(info);

	snprintf(buf, sizeof(buf), "%s", NAME_OF_UNKNOWN_CMD);
	cmd_set_result(info, buf, strnlen(buf, sizeof(buf)));

	mutex_lock(&info->lock);
	info->cmd_state = CMD_STATUS_WAITING;
	info->cmd_busy = false;
	mutex_unlock(&info->lock);

	dev_dbg(&info->client->dev, "%s - cmd[%s] len[%d] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), info->cmd_state);
}

#define MIP_CMD(name, func)	.cmd_name = name, .cmd_func = func

/**
* Info of command function
*/
struct mip_cmd {
	struct list_head list;
	const char *cmd_name;
	void (*cmd_func) (void *device_data);
};

/**
* List of command functions
*/
static struct mip_cmd mip_commands[] = {
	{MIP_CMD("fw_update", cmd_fw_update),},
	{MIP_CMD("get_fw_ver_bin", cmd_get_fw_ver_bin),},
	{MIP_CMD("get_fw_ver_ic", cmd_get_fw_ver_ic),},
	{MIP_CMD("get_chip_vendor", cmd_get_chip_vendor),},
	{MIP_CMD("get_chip_name", cmd_get_chip_name),},
	{MIP_CMD("get_x_num", cmd_get_x_num),},
	{MIP_CMD("get_y_num", cmd_get_y_num),},
	{MIP_CMD("get_max_x", cmd_get_max_x),},
	{MIP_CMD("get_max_y", cmd_get_max_y),},
	{MIP_CMD("module_off_master", cmd_module_off_master),},
	{MIP_CMD("module_on_master", cmd_module_on_master),},
	{MIP_CMD("run_intensity_read", cmd_read_intensity),},
	{MIP_CMD("get_intensity", cmd_get_intensity),},
	{MIP_CMD("run_rawdata_read", cmd_read_rawdata),},
	{MIP_CMD("get_rawdata", cmd_get_rawdata),},
	{MIP_CMD("run_inspection_read", cmd_run_test_cm_delta),},
	{MIP_CMD("get_inspection", cmd_get_cm_delta),},
	{MIP_CMD("run_cm_delta_read", cmd_run_test_cm_delta),},
	{MIP_CMD("get_cm_delta", cmd_get_cm_delta),},
	{MIP_CMD("run_cp_read", cmd_run_test_cm_delta),},
	{MIP_CMD("get_cp", cmd_get_cm_delta),},
	{MIP_CMD("get_dummy_cp", cmd_get_dummy_cm_delta),},
	{MIP_CMD("run_cm_abs_read", cmd_run_test_cm_abs),},
	{MIP_CMD("get_cm_abs", cmd_get_cm_abs),},
	{MIP_CMD("run_cm_jitter_read", cmd_run_test_cm_jitter),},
	{MIP_CMD("get_cm_jitter", cmd_get_cm_jitter),},
	{MIP_CMD("run_cm_delta_read_tsk", cmd_run_test_cm_delta),},
	{MIP_CMD("get_cm_delta_tsk", cmd_get_cm_delta_key),},
	{MIP_CMD("run_cm_abs_read_tsk", cmd_run_test_cm_abs),},
	{MIP_CMD("get_cm_abs_tsk", cmd_get_cm_abs_key),},
	{MIP_CMD("glove_mode", cmd_glove_mode),},
	{MIP_CMD("dead_zone_enable", cmd_edge_mode),},
	{MIP_CMD("get_threshold", cmd_get_threshold),},
	{MIP_CMD(NAME_OF_UNKNOWN_CMD, cmd_unknown_cmd),},
};

/**
* Sysfs - recv command
*/
static ssize_t mip_sys_cmd(struct device *dev, struct device_attribute *devattr, const char *buf, size_t count)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	int ret;
	char *cur, *start, *end;
	char cbuf[CMD_LEN] = { 0 };
	int len, i;
	struct mip_cmd *mip_cmd_ptr = NULL;
	char delim = ',';
	bool cmd_found = false;
	int param_cnt = 0;

	if (!info) {
		pr_err("%s [ERROR] mip_ts_info not found\n", __func__);
		ret = -EINVAL;
		goto ERROR;
	}

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);
	dev_dbg(&info->client->dev, "%s - input [%s]\n", __func__, buf);

	if (!info->input_dev) {
		dev_err(&info->client->dev, "%s [ERROR] input_dev not found\n", __func__);
		ret = -EINVAL;
		goto ERROR;
	}

	if (!info->enabled) {
		dev_err(&info->client->dev, "%s [ERROR] device is not enabled\n", __func__);
		ret = -EINVAL;
		goto ERROR;
	}

    if (strlen(buf) >= CMD_LEN) {
		dev_err(&info->client->dev, "%s: cmd length is over (%s,%d)!!\n", __func__, buf, (int)strlen(buf));
		return -EINVAL;
	}

	if (info->cmd_busy == true) {
		dev_err(&info->client->dev, "%s [ERROR] previous command is not ended\n", __func__);
		ret = -1;
		goto ERROR;
	}

	mutex_lock(&info->lock);
	info->cmd_busy = true;
	info->cmd_state = CMD_STATUS_RUNNING;
	mutex_unlock(&info->lock);

	info->cmd_state = 1;
	for (i = 0; i < ARRAY_SIZE(info->cmd_param); i++)
		info->cmd_param[i] = 0;

	len = (int)count;
	if (*(buf + len - 1) == '\n')
		len--;

	memset(info->cmd, 0x00, ARRAY_SIZE(info->cmd));
	memcpy(info->cmd, buf, len);
	cur = strchr(buf, (int)delim);
	if (cur)
		memcpy(cbuf, buf, cur - buf);
	else
		memcpy(cbuf, buf, len);

	dev_dbg(&info->client->dev, "%s - command [%s]\n", __func__, cbuf);

	/* command */
	list_for_each_entry(mip_cmd_ptr, &info->cmd_list_head, list) {
		if (!strncmp(cbuf, mip_cmd_ptr->cmd_name, CMD_LEN)) {
			cmd_found = true;
			break;
		}
	}
	if (!cmd_found) {
		list_for_each_entry(mip_cmd_ptr, &info->cmd_list_head, list) {
			if (!strncmp(NAME_OF_UNKNOWN_CMD, mip_cmd_ptr->cmd_name, CMD_LEN))
				break;
		}
	}

	/* parameter */
	if (cur && cmd_found) {
		cur++;
		start = cur;
		memset(cbuf, 0x00, ARRAY_SIZE(cbuf));

		do {
			if (*cur == delim || cur - buf == len) {
				end = cur;
				memcpy(cbuf, start, end - start);
				*(cbuf + strnlen(cbuf, ARRAY_SIZE(cbuf))) = '\0';
				if (kstrtoint(cbuf, 10, info->cmd_param + param_cnt) < 0)
					goto ERROR;
				start = cur + 1;
				memset(cbuf, 0x00, ARRAY_SIZE(cbuf));
				param_cnt++;
			}
			cur++;
		} while ((cur - buf <= len) && (param_cnt < CMD_PARAM_NUM));
	}

	dev_dbg(&info->client->dev, "%s - cmd [%s]\n", __func__, mip_cmd_ptr->cmd_name);
	for (i = 0; i < param_cnt; i++)
		dev_dbg(&info->client->dev, "%s - param #%d [%d]\n", __func__, i, info->cmd_param[i]);

	/* execute */
	mip_cmd_ptr->cmd_func(info);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return count;

ERROR:
	if (!info)
		pr_err("%s [ERROR]\n", __func__);
	else
		dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return count;
}
static DEVICE_ATTR(cmd, S_IWUSR | S_IWGRP, NULL, mip_sys_cmd);

/**
* Sysfs - print command status
*/
static ssize_t mip_sys_cmd_status(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	int ret;
	char cbuf[32] = {0};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - status [%d]\n", __func__, info->cmd_state);

	if (info->cmd_state == CMD_STATUS_WAITING)
		snprintf(cbuf, sizeof(cbuf), "WAITING");
	else if (info->cmd_state == CMD_STATUS_RUNNING)
		snprintf(cbuf, sizeof(cbuf), "RUNNING");
	else if (info->cmd_state == CMD_STATUS_OK)
		snprintf(cbuf, sizeof(cbuf), "OK");
	else if (info->cmd_state == CMD_STATUS_FAIL)
		snprintf(cbuf, sizeof(cbuf), "FAIL");
	else if (info->cmd_state == CMD_STATUS_NONE)
		snprintf(cbuf, sizeof(cbuf), "NOT_APPLICABLE");

	ret = snprintf(buf, PAGE_SIZE, "%s\n", cbuf);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}
static DEVICE_ATTR(cmd_status, S_IRUGO, mip_sys_cmd_status, NULL);

/**
* Sysfs - print command result
*/
static ssize_t mip_sys_cmd_result(struct device *dev, struct device_attribute *devattr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	int ret;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - result [%s]\n", __func__, info->cmd_result);

	mutex_lock(&info->lock);
	info->cmd_busy = false;
	mutex_unlock(&info->lock);

	info->cmd_state = CMD_STATUS_WAITING;

	ret = snprintf(buf, PAGE_SIZE, "%s\n", info->cmd_result);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}
static DEVICE_ATTR(cmd_result, S_IRUGO, mip_sys_cmd_result, NULL);

/**
* Sysfs - print command list
*/
static ssize_t mip_sys_cmd_list(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	int ret;
	int i = 0;
	char buffer[info->cmd_buffer_size + 30];
	char buffer_name[CMD_LEN];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	snprintf(buffer, 30, "== Command list ==\n");
	while (strncmp(mip_commands[i].cmd_name, NAME_OF_UNKNOWN_CMD, CMD_LEN) != 0) {
		snprintf(buffer_name, CMD_LEN, "%s\n", mip_commands[i].cmd_name);
		strncat(buffer, buffer_name, CMD_LEN);
		i++;
	}

	dev_dbg(&info->client->dev, "%s - length [%u / %d]\n", __func__, strlen(buffer), info->cmd_buffer_size);

	ret = snprintf(buf, PAGE_SIZE, "%s\n", buffer);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return ret;
}
static DEVICE_ATTR(cmd_list, S_IRUGO, mip_sys_cmd_list, NULL);

/**
* Sysfs - send proximity intensity
*/
static ssize_t mip_sys_intensity_proximity(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	int key_offset = info->node_x * info->node_y;

	if (mip_get_image(info, MIP_IMG_TYPE_INTENSITY)) {
		dev_err(&info->client->dev, "%s [ERROR] mip_get_image\n", __func__);
		goto ERROR;
	}

EXIT:
	dev_info(&info->client->dev, "%s - %s [%d]\n", __func__, attr->attr.name, info->image_buf[key_offset + 3]);

	return snprintf(buf, PAGE_SIZE, "%d", info->image_buf[key_offset + 3]);

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);

	return snprintf(buf, PAGE_SIZE, "NG");
}
static DEVICE_ATTR(proximity, S_IRUGO, mip_sys_intensity_proximity, NULL);

/**
* Sysfs - cmd attr info
*/
static struct attribute *mip_cmd_attr[] = {
	&dev_attr_cmd.attr,
	&dev_attr_cmd_status.attr,
	&dev_attr_cmd_result.attr,
	&dev_attr_cmd_list.attr,
	&dev_attr_proximity.attr,
	NULL,
};

/**
* Sysfs - cmd attr group info
*/
static const struct attribute_group mip_cmd_attr_group = {
	.attrs = mip_cmd_attr,
};

/**
* Sysfs print key intensity
*/
static ssize_t mip_sys_intensity_key(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	int key_offset = info->node_x * info->node_y;
	int key_idx = -1;
	int retry = 3;

	if (!strcmp(attr->attr.name, "touchkey_recent")) {
		key_idx = 0;
	} else if (!strcmp(attr->attr.name, "touchkey_back")) {
		key_idx = 1;
	} else {
		dev_err(&info->client->dev, "%s [ERROR] Invalid attribute\n", __func__);
		goto ERROR;
	}

	while (retry--) {
		if (info->test_busy == false)
			break;
		msleep(10);
	}
	if (retry <= 0) {
		dev_err(&info->client->dev, "%s [ERROR] skip\n", __func__);
		goto EXIT;
	}

	if (mip_get_image(info, MIP_IMG_TYPE_INTENSITY)) {
		dev_err(&info->client->dev, "%s [ERROR] mip_get_image\n", __func__);
		goto ERROR;
	}

EXIT:
	dev_dbg(&info->client->dev, "%s - %s [%d]\n", __func__, attr->attr.name, info->image_buf[key_offset + key_idx]);

	return snprintf(buf, PAGE_SIZE, "%d", info->image_buf[key_offset + key_idx]);

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);

	return snprintf(buf, PAGE_SIZE, "NG");
}
static DEVICE_ATTR(touchkey_recent, S_IRUGO, mip_sys_intensity_key, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, mip_sys_intensity_key, NULL);

/**
* Sysfs print key raw data
*/
static ssize_t mip_sys_raw_key(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	int key_offset = info->node_x * info->node_y;
	int key_idx = -1;
	int retry = 3;

	if (!strcmp(attr->attr.name, "touchkey_recent_raw")) {
		key_idx = 0;
	} else if (!strcmp(attr->attr.name, "touchkey_back_raw")) {
		key_idx = 1;
	} else {
		dev_err(&info->client->dev, "%s [ERROR] Invalid attribute\n", __func__);
		goto ERROR;
	}

	if (mip_run_test(info, MIP_TEST_TYPE_CM_DELTA)) {
		sprintf(buf, "%s", "NG");
		goto EXIT;
	}

EXIT:
	dev_info(&info->client->dev, "%s - %s [%d]\n", __func__, attr->attr.name, info->image_buf[key_offset + key_idx]);

	return snprintf(buf, PAGE_SIZE, "%d", info->image_buf[key_offset + key_idx]);

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);

	return snprintf(buf, PAGE_SIZE, "NG");
}
static DEVICE_ATTR(touchkey_recent_raw, S_IRUGO, mip_sys_raw_key, NULL);
static DEVICE_ATTR(touchkey_back_raw, S_IRUGO, mip_sys_raw_key, NULL);

/**
* Sysfs print key threshold
*/
static ssize_t mip_sys_threshold_key(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip_ts_info *info = dev_get_drvdata(dev);
	u8 wbuf[4];
	u8 rbuf[4];
	int threshold;

	wbuf[0] = MIP_R0_INFO;
	wbuf[1] = MIP_R1_INFO_CONTACT_THD_KEY;
	if (mip_i2c_read(info, wbuf, 2, rbuf, 2))
		goto ERROR;

	threshold = (rbuf[1] << 8) | rbuf[0];

	dev_dbg(&info->client->dev, "%s - threshold [%d]\n", __func__, threshold);
	return snprintf(buf, PAGE_SIZE, "%d", threshold);

ERROR:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return snprintf(buf, PAGE_SIZE, "NG");
}
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, mip_sys_threshold_key, NULL);

/**
* Sysfs - touchkey attr info
*/
static struct attribute *mip_cmd_key_attr[] = {
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent_raw.attr,
	&dev_attr_touchkey_back_raw.attr,
	&dev_attr_touchkey_threshold.attr,
	NULL,
};

/**
* Sysfs - touchkey attr group info
*/
static const struct attribute_group mip_cmd_key_attr_group = {
	.attrs = mip_cmd_key_attr,
};

extern struct class *sec_class;

/**
* Create sysfs command functions
*/
int mip_sysfs_cmd_create(struct mip_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i = 0;

	/* init cmd list */
	INIT_LIST_HEAD(&info->cmd_list_head);
	info->cmd_buffer_size = 0;

	for (i = 0; i < ARRAY_SIZE(mip_commands); i++) {
		list_add_tail(&mip_commands[i].list, &info->cmd_list_head);
		if (mip_commands[i].cmd_name)
			info->cmd_buffer_size += strlen(mip_commands[i].cmd_name) + 1;
	}
	mutex_lock(&info->lock);
	info->cmd_busy = false;
	mutex_unlock(&info->lock);

	if (info->print_buf == NULL)
		info->print_buf = kzalloc(sizeof(u8) * 4096, GFP_KERNEL);
	if (info->image_buf == NULL)
		info->image_buf = kzalloc(sizeof(int) * ((info->node_x * info->node_y) + info->node_key), GFP_KERNEL);
	/* create sysfs */
	if (sysfs_create_group(&client->dev.kobj, &mip_cmd_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}

	/* create sec class */
	/*
	info->cmd_class = class_create(THIS_MODULE, "sec");
	info->cmd_dev = device_create(info->cmd_class, NULL, 0, info, "tsp");
	*/
	info->cmd_dev = device_create(sec_class, NULL, 0, info, "tsp");
	if (sysfs_create_group(&info->cmd_dev->kobj, &mip_cmd_attr_group)) {
		dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}

	if (info->key_enable == true) {
		/* create sysfs for touchkey */
		if (sysfs_create_group(&client->dev.kobj, &mip_cmd_key_attr_group)) {
			dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
			return -EAGAIN;
		}

		/* create sec class for touchkey */
		info->key_dev = device_create(sec_class, NULL, 0, info, "sec_touchkey");
		if (sysfs_create_group(&info->key_dev->kobj, &mip_cmd_key_attr_group)) {
			dev_err(&client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
			return -EAGAIN;
		}
	}

	return 0;
}

/**
* Remove sysfs command functions
*/
void mip_sysfs_cmd_remove(struct mip_ts_info *info)
{
	sysfs_remove_group(&info->client->dev.kobj, &mip_cmd_attr_group);

	return;
}

#endif

