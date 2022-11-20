/*
 * MELFAS MIP4 Touchscreen
 *
 * Copyright (C) 2000-2018 MELFAS Inc.
 *
 *
 * mip4_ts_sec.c : Custom functions for SEC
 *
 * Version : 2018.10.18
 *
 * Tested models : SM-G950F
 */

#include "mip4_ts.h"

#define CMD_RES_X 1440
#define CMD_RES_Y 2960

#define USE_SEC_TOUCHKEY 0

#define USE_SEC_CMD_DEVICE 1
#define USE_SEC_CLASS 1

#if USE_SEC_CLASS
extern struct class *sec_class;
#endif

/*
 * Sponge
 */
#if USE_SPONGE

/* Sponge Registers for AP access */
#define D_SPONGE_AOD_ENABLE_OFFSET 0x02 /**/
#define D_SPONGE_TOUCHBOX_W_OFFSET 0x02 /**/
#define D_SPONGE_TOUCHBOX_H_OFFSET 0x04
#define D_SPONGE_TOUCHBOX_X_OFFSET 0x06
#define D_SPONGE_TOUCHBOX_Y_OFFSET 0x08
#define D_SPONGE_UTC_OFFSET        0x10
#define D_BASE_LP_DUMP_REG_ADDR            0xF0
#define D_SPONGE_DUMP_FORMAT_REG_OFFSET    (D_BASE_LP_DUMP_REG_ADDR + 0x00)
#define D_SPONGE_DUMP_NUM_REG_OFFSET       (D_BASE_LP_DUMP_REG_ADDR + 0x01)
#define D_SPONGE_DUMP_CUR_INDEX_REG_OFFSET (D_BASE_LP_DUMP_REG_ADDR + 0x02)
#define D_SPONGE_DUMP_START                (D_BASE_LP_DUMP_REG_ADDR + 0x04)

/* write UTC code */
void mip4_ts_sponge_write_time(struct mip4_ts_info *info, u32 val)
{
	int ret = 0;
	u8 data[4];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - val[%u]\n", __func__, val);

	data[0] = (val >> 24) & 0xFF; /* Data */
	data[1] = (val >> 16) & 0xFF; /* Data */
	data[2] = (val >> 8) & 0xFF; /* Data */
	data[3] = (val >> 0) & 0xFF; /* Data */

#if USE_SPONGE
	ret = mip4_ts_sponge_write(info, D_SPONGE_UTC_OFFSET, data, 4);
	if (ret < 0) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_sponge_write\n", __func__);
		goto error;
	}
#endif

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;

error:
	dev_dbg(&info->client->dev, "%s [ERROR]\n", __func__);
	return;
}

#if 0
/* Set AOD rect for sponge */
static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct irq_desc *desc = irq_to_desc(info->client->irq);
	int irq_gpio_status;
	u8 data[10] = {0x02, 0};
	u8 wbuf[16] = {0}
	int ret, i;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	irq_gpio_status = gpio_get_value(info->gpio_intr);

	dev_info(&info->client->dev, "%s: w:%d, h:%d, x:%d, y:%d, (%d,%d,%d)\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3],
			irq_gpio_status, desc->depth, desc->irq_count);

	for (i = 0; i < 4; i++) {
		data[i * 2 + 2] = sec->cmd_param[i] & 0xFF;
		data[i * 2 + 3] = (sec->cmd_param[i] >> 8) & 0xFF;
		ts->rect_data[i] = sec->cmd_param[i];
	}

#if USE_SPONGE
	disable_irq(info->client->irq);

	ret = mip4_ts_i2c_write(info, SEC_TS_CMD_SPONGE_WRITE_PARAM, &data[0], 10);
	if (ret < 0) {
		dev_err(&info->client->dev, "%s: Failed to write offset\n", __func__);
		goto NG;
	}

	enable_irq(info->client->irq);

	/* optional reg : SEC_TS_CMD_LPM_AOD_OFF_ON(0x9B)			*/
	/* 0 : aod off : change tsp scan freq , avoid wacom scan noise 	*/
	/* 1 : aod on->off : change tsp scan freq , avoid wacom scan noise in lpm mode*/
	/* 2 : aod on : scan based on vsync/hsync					*/
	if (ts->power_status == SEC_TS_STATE_LPM) {
		if (sec->cmd_param[0] == 0 && sec->cmd_param[1] == 0 &&
			sec->cmd_param[2] == 0 && sec->cmd_param[3] == 0 ) {

			data[0] = SEC_TS_CMD_LPM_AOD_ON_OFF;
		} else {
			data[0] = SEC_TS_CMD_LPM_AOD_ON;
		}
		ret = mip4_ts_i2c_write(info, SEC_TS_CMD_LPM_AOD_OFF_ON, &data[0], 1);
		if (ret < 0) {
			dev_err(&info->client->dev, "%s: Failed to send aod off_on cmd\n", __func__);
			goto NG;
		}
	}
#endif

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return;

error:
	enable_irq(info->client->irq);
	snprintf(buf, sizeof(buf), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return;
}
#endif

int mip4_ts_sponge_read(struct mip4_ts_info *info, u16 addr, u8 *buf, u8 len)
{
	int ret = 0;
	u8 wbuf[4] = {0, };
	u16 mip4_addr = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mip4_addr = MIP4_LIB_ADDR_START + addr;
	if (mip4_addr > MIP4_LIB_ADDR_END) {
		dev_err(&info->client->dev, "%s [ERROR] sponge addr range\n", __func__);
		ret = -1;
		goto exit;
	}

	wbuf[0] = (u8)((mip4_addr >> 8) & 0xFF);
	wbuf[1] = (u8)(mip4_addr & 0xFF);
	if (mip4_ts_i2c_read(info, wbuf, 2, buf, len)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_read\n", __func__);
		ret = -1;
	}

exit:
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

int mip4_ts_sponge_write(struct mip4_ts_info *info, u16 addr, u8 *buf, u8 len)
{
	int ret = 0;
	u8 *wbuf;
	u16 mip4_addr = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	mip4_addr = MIP4_LIB_ADDR_START + addr;
	if (mip4_addr > MIP4_LIB_ADDR_END) {
		dev_err(&info->client->dev, "%s [ERROR] sponge addr range\n", __func__);
		ret = -1;
		goto exit;
	}

	wbuf = kzalloc(sizeof(u8) * (2 + len), GFP_KERNEL);

	wbuf[0] = (u8)((mip4_addr >> 8) & 0xFF);
	wbuf[1] = (u8)(mip4_addr & 0xFF);
	memcpy(&wbuf[2], buf, len);

	if (mip4_ts_i2c_write(info, wbuf, 2 + len)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		ret = -1;
	}

	kfree(wbuf);

exit:
	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

/*
 * Event handler
 */
int mip4_ts_custom_event_handler(struct mip4_ts_info *info, u8 *rbuf, u8 size)
{
	int ret = 0;
	u8 s_feature = 0;
	u8 event_id = 0;
	u8 gesture_type = 0;
	u8 gesture_id = 0;
	u8 gesture_data[4] = {0, };
	u8 left_event = 0;
	//int x = 0;
	//int y = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	//print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " aod event : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, size, false);

	s_feature = (rbuf[2] >> 6) & 0x03;
	gesture_type = (rbuf[2] >> 2) & 0x0F;
	event_id = rbuf[2] & 0x03;
	gesture_id = rbuf[3];
	gesture_data[0] = rbuf[4];
	gesture_data[1] = rbuf[5];
	gesture_data[2] = rbuf[6];
	gesture_data[3] = rbuf[7];
	left_event = rbuf[9] & 0x3F;

	dev_dbg(&info->client->dev, "%s - sf[%u] eid[%u] left[%u]\n", __func__, s_feature, event_id, left_event);
	dev_dbg(&info->client->dev, "%s - gesture type[%u] id[%u] data[0x%02X 0x%02X 0x%02X 0x%02X]\n", __func__, gesture_type, gesture_id, gesture_data[0], gesture_data[1], gesture_data[2], gesture_data[3]);

	if (s_feature) {
		/* Samsung */
		if (gesture_type == 0) {
			/* Wake-up */
			if (gesture_id == 0) {
				/* Swipe up */
			} else if (gesture_id == 1) {
				/* Double tap */

				//////////////////////////
				// PLEASE MODIFY HERE !!!
				//

				info->scrub_id = 11;
				//info->scrub_x = x / info->max_x * CMD_RES_X;
				//info->scrub_y = y / info->max_y * CMD_RES_Y;
				dev_dbg(&info->client->dev, "%s - scrub : id[%d] x[%d] y[%d]\n", __func__, info->scrub_id, info->scrub_x, info->scrub_y);
#if 0
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(info->input_dev);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
				input_sync(info->input_dev);
#endif
				//
				//////////////////////////
			} else if (gesture_id == 2) {
				/* Edge swipe */
			}
		} else if (gesture_type == 7) {
			/* Vendor */
		}
	}
	else {
		/* Melfas */
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

#if USE_AOD
/*
 * AOD config
 */
int mip4_ts_aod_config(struct mip4_ts_info *info)
{
	int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}
#endif /* USE_AOD */

#else /* USE_SPONGE */

/* Register address */
#define MIP4_R1_SEC_AOD_CTRL_APPLY    0x10
#define MIP4_R1_SEC_AOD_CTRL_TYPE     0x12
#define MIP4_R1_SEC_AOD_CTRL_GESTURE  0x13
#define MIP4_R1_SEC_AOD_CTRL_WIDTH    0x14
#define MIP4_R1_SEC_AOD_CTRL_HEIGHT   0x16
#define MIP4_R1_SEC_AOD_CTRL_OFFSET_X 0x18
#define MIP4_R1_SEC_AOD_CTRL_OFFSET_Y 0x1A

#define MIP4_R1_SEC_AOD_EVENT_TYPE    0x13
#define MIP4_R1_SEC_AOD_EVENT_X       0x14
#define MIP4_R1_SEC_AOD_EVENT_Y       0x16

/*
 * Event handler
 */
int mip4_ts_custom_event_handler(struct mip4_ts_info *info, u8 *rbuf, u8 size)
{
	int ret = 0;
	u8 aod;
	u8 spay;
	u16 x;
	u16 y;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	print_hex_dump(KERN_ERR, MIP4_TS_DEVICE_NAME " aod event : ", DUMP_PREFIX_OFFSET, 16, 1, rbuf, size, false);

	aod = (rbuf[2] >> 2) & 0x01;
	spay = (rbuf[2] >> 1) & 0x01;
	x = (rbuf[4] << 8) | rbuf[3];
	y = (rbuf[6] << 8) | rbuf[5];

	dev_dbg(&info->client->dev, "%s - aod[%u] spay[%u] x[%d] y[%d]\n", __func__, aod, spay, x, y);

	if (aod || spay) {
		//////////////////////////
		// PLEASE MODIFY HERE !!!
		//

		info->scrub_id = 11;
		info->scrub_x = x / info->max_x * CMD_RES_X;
		info->scrub_y = y / info->max_y * CMD_RES_Y;
		dev_dbg(&info->client->dev, "%s - scrub : id[%d] x[%d] y[%d]\n", __func__, info->scrub_id, info->scrub_x, info->scrub_y);
#if 0
		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
		input_sync(info->input_dev);
#endif
#if 1
		input_report_key(info->input_dev, KEY_HOMEPAGE, 1);
		input_sync(info->input_dev);
		input_report_key(info->input_dev, KEY_HOMEPAGE, 0);
		input_sync(info->input_dev);
#endif

		//
		//////////////////////////
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return ret;
}

#if USE_AOD
/*
 * AOD config
 */
int mip4_ts_aod_config(struct mip4_ts_info *info)
{
	u8 wbuf[12];
	u8 aod, spay, double_tap;
	u16 width, height, offset_x, offset_y;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	aod = info->aod.aod_enable;
	spay = info->aod.spay_enable;
	double_tap = info->aod.doubletap_enable;
	width = info->aod.width / CMD_RES_X * info->max_x;
	height = info->aod.height / CMD_RES_Y * info->max_y;
	offset_x = info->aod.x / CMD_RES_X * info->max_x;
	offset_y = info->aod.y / CMD_RES_Y * info->max_y;

	dev_dbg(&info->client->dev, "%s - AOD[%u] Spay[%u] DoubleTap[%u] Width[%u] Height[%u] X[%u] Y[%u]\n", __func__, aod, spay, double_tap, width, height, offset_x, offset_y);

	/* Config */
	wbuf[0] = MIP4_R0_CUSTOM;
	wbuf[1] = MIP4_R1_SEC_AOD_CTRL_TYPE;
	wbuf[2] = ((aod << 2) & 0x04) | ((spay << 1) & 0x02);
	wbuf[3] = (double_tap << 5) & 0x20;
	wbuf[4] = width & 0xFF;
	wbuf[5] = (width >> 8) & 0xFF;
	wbuf[6] = height & 0xFF;
	wbuf[7] = (height >> 8) & 0xFF;
	wbuf[8] = offset_x & 0xFF;
	wbuf[9] = (offset_x >> 8) & 0xFF;
	wbuf[10] = offset_y & 0xFF;
	wbuf[11] = (offset_y >> 8) & 0xFF;
	if (mip4_ts_i2c_write(info, wbuf, 12)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		goto error;
	}

	/* Apply */
	wbuf[0] = MIP4_R0_CUSTOM;
	wbuf[1] = MIP4_R1_SEC_AOD_CTRL_APPLY;
	wbuf[2] = 1;
	if (mip4_ts_i2c_write(info, wbuf, 3)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}
#endif /* USE_AOD */

#endif /* USE_SPONGE */

/*
 * Factory command functions
 */
#if USE_CMD

/*
 * Command : Update firmware
 */
static void fw_update(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	int fw_location = cmd->cmd_param[0];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	switch (fw_location) {
	case 0:
		if (mip4_ts_fw_update_from_kernel(info)) {
			goto error;
		}
		break;
	case 1:
		if (mip4_ts_fw_update_from_storage(info, FW_PATH_EXTERNAL, true)) {
			goto error;
		}
		break;
	default:
		goto error;
		break;
	}

	sprintf(buf, "%s", "OK");
	cmd->cmd_state = SEC_CMD_STATUS_OK;
	goto exit;

error:
	sprintf(buf, "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Get firmware version from file
 */
static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 version[VERSION_LENGTH];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	if (mip4_ts_get_fw_version_from_bin(info, version)) {
		sprintf(buf, "%s", "NG");
		cmd->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	/* Version format = "ME" + Panel revision + Test version + Release version */
	sprintf(buf, "ME%02X%02X%02X\n", 0x00, version[7], version[6]);
	cmd->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Get firmware version from chip
 */
static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 version[VERSION_LENGTH];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	if (mip4_ts_get_fw_version(info, version)) {
		sprintf(buf, "%s", "NG");
		cmd->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	/* Version format = "ME" + Panel revision + Test version + Release version */
	sprintf(buf, "ME%02X%02X%02X\n", 0x00, version[7], version[6]);
	cmd->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Get chip vendor
 */
static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	sprintf(buf, "ME");
	cmd->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Get chip name
 */
static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	sprintf(buf, CHIP_NAME);
	cmd->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Get X axis node num
 */
static void get_x_num(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 rbuf[4];
	u8 wbuf[4];
	int val;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	wbuf[0] = MIP4_R0_INFO;
	wbuf[1] = MIP4_R1_INFO_NODE_NUM_Y; /* Factory App X = Node Num Y */
	if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, 1)) {
		cmd->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	val = rbuf[0];

	sprintf(buf, "%d", val);
	cmd->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Get Y axis node num
 */
static void get_y_num(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 rbuf[4];
	u8 wbuf[4];
	int val;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	wbuf[0] = MIP4_R0_INFO;
	wbuf[1] = MIP4_R1_INFO_NODE_NUM_X; /* Factory App Y = Node Num X */
	if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, 1)) {
		cmd->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	val = rbuf[0];

	sprintf(buf, "%d", val);
	cmd->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Power off
 */
static void module_off_master(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	if (mip4_ts_power_off(info, POWER_OFF_DELAY)) {
		sprintf(buf, "%s", "NG");
		cmd->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	sprintf(buf, "%s", "OK");
	cmd->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Power on
 */
static void module_on_master(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	if (mip4_ts_power_on(info, POWER_ON_DELAY)) {
		sprintf(buf, "%s", "NG");
		cmd->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	sprintf(buf, "%s", "OK");
	cmd->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Get screen threshold
 */
static void get_threshold(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 rbuf[4];
	u8 wbuf[4];
	int val;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	wbuf[0] = MIP4_R0_INFO;
	wbuf[1] = MIP4_R1_INFO_CONTACT_THD_SCR;
	if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, 1)) {
		cmd->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	val = rbuf[0];

	sprintf(buf, "%d", val);
	cmd->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Set glove mode
 */
static void glove_mode(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 wbuf[4];
	u8 val = cmd->cmd_param[0];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

 	if ((val == 0) || (val == 1)) {
		wbuf[0] = MIP4_R0_CTRL;
		wbuf[1] = MIP4_R1_CTRL_HIGH_SENS_MODE;
		wbuf[2] = val;
		if (mip4_ts_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
			goto error;
		}

		sprintf(buf, "%s", "OK");
		cmd->cmd_state = SEC_CMD_STATUS_OK;
		goto exit;
	}

error:
	sprintf(buf, "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Set clear cover mode
 */
static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 wbuf[4];
	u8 val = cmd->cmd_param[0];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

 	if ((val == 0) || (val == 1)) {
		wbuf[0] = MIP4_R0_CTRL;
		wbuf[1] = MIP4_R1_CTRL_WINDOW_MODE;
		wbuf[2] = val;
		if (mip4_ts_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
			goto error;
		}

		sprintf(buf, "%s", "OK");
		cmd->cmd_state = SEC_CMD_STATUS_OK;
		goto exit;
	}

error:
	sprintf(buf, "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Set wireless charger mode
 */
static void wireless_charger_mode(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 wbuf[4];
	u8 val = cmd->cmd_param[0];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

 	if ((val == 0) || (val == 1)) {
		wbuf[0] = MIP4_R0_CTRL;
		wbuf[1] = MIP4_R1_CTRL_WIRELESS_CHARGER_MODE;
		wbuf[2] = val;
		if (mip4_ts_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
			goto error;
		}

		sprintf(buf, "%s", "OK");
		cmd->cmd_state = SEC_CMD_STATUS_OK;
		goto exit;
	}

error:
	sprintf(buf, "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Grip control register
 */
#define MIP4_R1_SEC_GRIP_CONTROL_ENABLE                0x30
#define MIP4_R1_SEC_GRIP_PORTRAIT_DEADZONE_LOWER_Y     0x32
#define MIP4_R1_SEC_GRIP_PORTRAIT_DEADZONE_UPPER_WIDTH 0x34
#define MIP4_R1_SEC_GRIP_PORTRAIT_DEADZONE_LOWER_WIDTH 0x36
#define MIP4_R1_SEC_GRIP_PORTRAIT_EDGE_WIDTH           0x38
#define MIP4_R1_SEC_GRIP_PORTRAIT_APPLY                0x3A
#define MIP4_R1_SEC_GRIP_LANDSCAPE_DEADZONE_WIDTH      0x3C
#define MIP4_R1_SEC_GRIP_LANDSCAPE_EDGE_WIDTH          0x3E
#define MIP4_R1_SEC_GRIP_LANDSCAPE_APPLY               0x40
#define MIP4_R1_SEC_GRIP_EDGE_HANDLER_Y_START          0x42
#define MIP4_R1_SEC_GRIP_EDGE_HANDLER_Y_END            0x44
#define MIP4_R1_SEC_GRIP_EDGE_HANDLER_DIR              0x46
#define MIP4_R1_SEC_GRIP_EDGE_HANDLER_APPLY            0x47

/*
 * Grip config portrait
 */
static int grip_config_portrait(struct mip4_ts_info *info, bool enable, u16 edge_w, u16 deadzone_upper_w, u16 deadzone_lower_w, u16 deadzone_lower_y)
{
	u8 wbuf[11] = {0};
	u16 actual_edge_w = edge_w / CMD_RES_X * info->max_x;
	u16 actual_deadzone_upper_w = deadzone_upper_w / CMD_RES_X * info->max_x ;
	u16 actual_deadzone_lower_w = deadzone_lower_w / CMD_RES_X * info->max_x ;
	u16 actual_deadzone_lower_y = deadzone_lower_y / CMD_RES_Y * info->max_y;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - enable[%d]\n", __func__, enable);
	dev_dbg(&info->client->dev, "%s - edge_w[%d] actual_edge_w[%d]\n", __func__, edge_w, actual_edge_w);
	dev_dbg(&info->client->dev, "%s - deadzone_upper_w[%d] actual_deadzone_upper_w[%d]\n", __func__, deadzone_upper_w, actual_deadzone_upper_w);
	dev_dbg(&info->client->dev, "%s - deadzone_lower_w[%d] actual_deadzone_lower_w[%d]\n", __func__, deadzone_lower_w, actual_deadzone_lower_w);
	dev_dbg(&info->client->dev, "%s - deadzone_lower_y[%d] actual_deadzone_lower_y[%d]\n", __func__, deadzone_lower_y, actual_deadzone_lower_y);

	wbuf[0] = MIP4_R0_CUSTOM;
	wbuf[1] = MIP4_R1_SEC_GRIP_PORTRAIT_DEADZONE_LOWER_Y;

	if (enable) {
		wbuf[2] = actual_deadzone_lower_y & 0xFF;
		wbuf[3] = (actual_deadzone_lower_y >> 8) & 0xFF;
		wbuf[4] = actual_deadzone_upper_w & 0xFF;
		wbuf[5] = (actual_deadzone_upper_w >> 8) & 0xFF;
		wbuf[6] = actual_deadzone_lower_w & 0xFF;
		wbuf[7] = (actual_deadzone_lower_w >> 8) & 0xFF;
		wbuf[8] = actual_edge_w & 0xFF;
		wbuf[9] = (actual_edge_w >> 8) & 0xFF;
		wbuf[10] = 1;
	}

	print_hex_dump(KERN_DEBUG, MIP4_TS_DEVICE_NAME " grip_config_portrait : ", DUMP_PREFIX_OFFSET, 16, 1, wbuf, 11, false);

	if (mip4_ts_i2c_write(info, wbuf, 11)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
 * Grip config landscape
 */
static int grip_config_landscape(struct mip4_ts_info *info, bool enable, u8 edge_w, u8 deadzone_w)
{
	u8 wbuf[7] = {0};
	u16 actual_edge_w = edge_w / CMD_RES_X * info->max_x;
	u16 actual_deadzone_w = deadzone_w / CMD_RES_X * info->max_x;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - enable[%d]\n", __func__, enable);
	dev_dbg(&info->client->dev, "%s - edge_w[%d] actual_edge_w[%d]\n", __func__, edge_w, actual_edge_w);
	dev_dbg(&info->client->dev, "%s - deadzone_w[%d] actual_deadzone_w[%d]\n", __func__, deadzone_w, actual_deadzone_w);

	wbuf[0] = MIP4_R0_CUSTOM;
	wbuf[1] = MIP4_R1_SEC_GRIP_LANDSCAPE_DEADZONE_WIDTH;

	if (enable) {
		wbuf[2] = actual_deadzone_w & 0xFF;
		wbuf[3] = (actual_deadzone_w >> 8) & 0xFF;
		wbuf[4] = actual_edge_w & 0xFF;
		wbuf[5] = (actual_edge_w >> 8) & 0xFF;
		wbuf[6] = 1;
	}

	print_hex_dump(KERN_DEBUG, MIP4_TS_DEVICE_NAME " grip_config_landscape : ", DUMP_PREFIX_OFFSET, 16, 1, wbuf, 7, false);

	if (mip4_ts_i2c_write(info, wbuf, 7)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
 * Grip config edge handler
 */
static int __maybe_unused grip_config_edge_handler(struct mip4_ts_info *info, u8 dir, u16 y_start, u16 y_end)
{
	u8 wbuf[8];
	u16 actual_y_start = y_start / CMD_RES_Y * info->max_y;
	u16 actual_y_end = y_end / CMD_RES_Y * info->max_y;
	
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - dir[%d] y[%d ~ %d] actual_y[%d ~ %d]\n", __func__, dir, y_start, y_end, actual_y_start, actual_y_end);

	wbuf[0] = MIP4_R0_CUSTOM;
	wbuf[1] = MIP4_R1_SEC_GRIP_EDGE_HANDLER_Y_START;
	wbuf[2] = actual_y_start & 0xFF;
	wbuf[3] = (actual_y_start >> 8) & 0xFF;
	wbuf[4] = actual_y_end & 0xFF;
	wbuf[5] = (actual_y_end >> 8) & 0xFF;
	wbuf[6] = dir;
	if (dir > 0) {
		wbuf[7] = 1;
	} else {
		wbuf[7] = 0;
	}

	print_hex_dump(KERN_DEBUG, MIP4_TS_DEVICE_NAME " grip_config_edge_handler : ", DUMP_PREFIX_OFFSET, 16, 1, wbuf, 8, false);

	if (mip4_ts_i2c_write(info, wbuf, 8)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
 * Grip enable
 */
static int grip_enable(struct mip4_ts_info *info, bool enable)
{
	u8 wbuf[3];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - enable[%d]\n", __func__, enable);

	wbuf[0] = MIP4_R0_CUSTOM;
	wbuf[1] = MIP4_R1_SEC_GRIP_CONTROL_ENABLE;
	wbuf[2] = enable;
	if (mip4_ts_i2c_write(info, wbuf, 3)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return 1;
}

/*
 * Command : Set grip data
 */
static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	if (cmd->cmd_param[0] == 0) {
		/* Edge handler */
		if (cmd->cmd_param[1] == 0) {
			/* Disable */
			dev_dbg(&info->client->dev, "%s - edge handler : disable\n", __func__);
			if (grip_config_edge_handler(info, 0, 0, 0)) {
				dev_err(&info->client->dev, "%s [ERROR] grip_config_edge_handler\n", __func__);
				goto error;
			}
		} else if (cmd->cmd_param[1] == 1) {
			/* Left */
			dev_dbg(&info->client->dev, "%s - edge handler : left[%d ~ %d]\n", __func__, cmd->cmd_param[2], cmd->cmd_param[3]);
			if (grip_config_edge_handler(info, 1, cmd->cmd_param[2], cmd->cmd_param[3])) {
				dev_err(&info->client->dev, "%s [ERROR] grip_config_edge_handler\n", __func__);
				goto error;
			}
		} else if (cmd->cmd_param[1] == 2) {
			/* Right */
			dev_dbg(&info->client->dev, "%s - edge handler : right[%d ~ %d]\n", __func__, cmd->cmd_param[2], cmd->cmd_param[3]);
			if (grip_config_edge_handler(info, 2, cmd->cmd_param[2], cmd->cmd_param[3])) {
				dev_err(&info->client->dev, "%s [ERROR] grip_config_edge_handler\n", __func__);
				goto error;
			}
		} else {
			dev_err(&info->client->dev, "%s [ERROR] edge handler\n", __func__);
			goto error;
		}
	} else if (cmd->cmd_param[0] == 1) {
		/* Portrait (normal) */
		dev_dbg(&info->client->dev, "%s - portrait : edge[%d] deadzone_upper[%d] deadzone_lower[%d] y[%d]\n", __func__, cmd->cmd_param[1], cmd->cmd_param[2], cmd->cmd_param[3], cmd->cmd_param[4]);
		if (grip_config_portrait(info, true, cmd->cmd_param[1], cmd->cmd_param[2], cmd->cmd_param[3], cmd->cmd_param[4])) {
			dev_err(&info->client->dev, "%s [ERROR] grip_config_portrait\n", __func__);
			goto error;
		}
		if (grip_config_landscape(info, false, 0, 0)) {
			dev_err(&info->client->dev, "%s [ERROR] grip_config_landscape\n", __func__);
			goto error;
		}
	} else if (cmd->cmd_param[0] == 2) {
		/* Landscape */
		if (cmd->cmd_param[1] == 0) {
			/* Portrait */
			dev_dbg(&info->client->dev, "%s - landscape : portrait\n", __func__);
			if (grip_config_landscape(info, false, 0, 0)) {
				dev_err(&info->client->dev, "%s [ERROR] grip_config_landscape\n", __func__);
				goto error;
			}
		} else if (cmd->cmd_param[1] == 1) {
			/* Landscape */
			dev_dbg(&info->client->dev, "%s - landscape : edge[%d] deadzone[%d]\n", __func__, cmd->cmd_param[2], cmd->cmd_param[3]);
			if (grip_config_landscape(info, true, cmd->cmd_param[2], cmd->cmd_param[3])) {
				dev_err(&info->client->dev, "%s [ERROR] grip_config_landscape\n", __func__);
				goto error;
			}
		} else {
			dev_err(&info->client->dev, "%s [ERROR] landscape\n", __func__);
			goto error;
		}
	} else {
		dev_err(&info->client->dev, "%s - [ERROR] command\n", __func__);
		goto error;
	}

	if (grip_enable(info, true)) {
		dev_err(&info->client->dev, "%s [ERROR] grip_enable\n", __func__);
		goto error;
	}

	snprintf(buf, sizeof(buf), "%s", "OK");
	cmd->cmd_state = SEC_CMD_STATUS_OK;
	goto exit;

error:
	snprintf(buf, sizeof(buf), "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/*
 * Command : Set dead zone enable
 */
static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 wbuf[4];
	u8 val = cmd->cmd_param[0];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

 	if ((val == 0) || (val == 1)) {
		wbuf[0] = MIP4_R0_CUSTOM;
		wbuf[1] = MIP4_R1_SEC_GRIP_CONTROL_ENABLE;
		wbuf[2] = val;
		if (mip4_ts_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
			goto error;
		}

		sprintf(buf, "%s", "OK");
		cmd->cmd_state = SEC_CMD_STATUS_OK;
		goto exit;
	}

error:
	sprintf(buf, "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Set power mode
 */
static void set_lowpower_mode(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 wbuf[4];
	u8 val = cmd->cmd_param[0];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

 	if ((val == 0) || (val == 1)) {
		wbuf[0] = MIP4_R0_CTRL;
		wbuf[1] = MIP4_R1_CTRL_POWER_STATE;
		wbuf[2] = val;
		if (mip4_ts_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
			goto error;
		}

		sprintf(buf, "%s", "OK");
		cmd->cmd_state = SEC_CMD_STATUS_OK;
		goto exit;
	}

error:
	sprintf(buf, "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Set proximity enable
 */
static void proximity_enable(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 wbuf[4];
	u8 val = cmd->cmd_param[0];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

 	if ((val == 0) || (val == 1)) {
		wbuf[0] = MIP4_R0_CTRL;
		wbuf[1] = MIP4_R1_CTRL_PROXIMITY_SENSING;
		wbuf[2] = val;
		if (mip4_ts_i2c_write(info, wbuf, 3)) {
			dev_err(&info->client->dev, "%s [ERROR] mip4_ts_i2c_write\n", __func__);
			goto error;
		}

		sprintf(buf, "%s", "OK");
		cmd->cmd_state = SEC_CMD_STATUS_OK;
		goto exit;
	}

error:
	sprintf(buf, "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

/*
 * Command : Run proximity calibration
 */
static void run_proximity_calibration(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 result = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	result = mip4_ts_run_calibration(info, MIP4_CAL_TYPE_RUN_AUTO);
	if (result != 0) {
		goto error;
	}

	sprintf(buf, "%s", "OK");
	cmd->cmd_state = SEC_CMD_STATUS_OK;
	goto exit;

error:
	sprintf(buf, "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE] cmd[%s] len[%zu] state[%d]\n", __func__, buf, strnlen(buf, sizeof(buf)), cmd->cmd_state);
}

#if USE_AOD
/*
 * Command : Set AOD rect
 */
static void __maybe_unused set_aod_rect(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u16 w = cmd->cmd_param[0];
	u16 h = cmd->cmd_param[1];
	u16 x = cmd->cmd_param[2];
	u16 y = cmd->cmd_param[3];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	dev_dbg(&info->client->dev, "%s - w[%d] h[%d] x[%d] y[%d]\n", __func__, w, h, x, y);

	info->aod.doubletap_enable = 1;
	info->aod.width = w;
	info->aod.height = h;
	info->aod.x = x;
	info->aod.y = y;

	if (mip4_ts_aod_config(info)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		cmd->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buf, sizeof(buf), "%s", "OK");
		cmd->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/*
 * Command : Set AOD enable
 */
static void __maybe_unused aod_enable(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	//int ret = 0;
	u8 enable = cmd->cmd_param[0];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	dev_dbg(&info->client->dev, "%s - enable[%u]\n", __func__, enable);

	if ((enable == 0) || (enable == 1)) {
		info->aod.aod_enable = enable;

		snprintf(buf, sizeof(buf), "%s", "OK");
		cmd->cmd_state = SEC_CMD_STATUS_OK;
		goto exit;
	}

	snprintf(buf, sizeof(buf), "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/*
 * Command : Set spay enable
 */
static void __maybe_unused spay_enable(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u8 enable = cmd->cmd_param[0];

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);

	dev_dbg(&info->client->dev, "%s - enable[%u]\n", __func__, enable);

	if ((enable == 0) || (enable == 1)) {
		info->aod.spay_enable = enable;

		snprintf(buf, sizeof(buf), "%s", "OK");
		cmd->cmd_state = SEC_CMD_STATUS_OK;
		goto exit;
	}

	snprintf(buf, sizeof(buf), "%s", "NG");
	cmd->cmd_state = SEC_CMD_STATUS_FAIL;

exit:
	sec_cmd_set_cmd_result(cmd, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}
#endif /* USE_AOD */

/*
 * Command : Not supported command
 */
static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *cmd = (struct sec_cmd_data *)device_data;
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sec_cmd_set_default_result(cmd);
	snprintf(buf, sizeof(buf), "%s", "NA");

	sec_cmd_set_cmd_result(cmd, buf, strlen(buf));
	cmd->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

	sec_cmd_set_cmd_exit(cmd);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

/*
 * Command list
 */
static struct sec_cmd cmd_list[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("module_off_master", module_off_master),},
	{SEC_CMD("module_on_master", module_on_master),},
	{SEC_CMD("get_threshold", get_threshold),},
	//{SEC_CMD("run_rawdata_read", run_rawdata_read),},
	//{SEC_CMD("run_rawdata_read_all", run_rawdata_read_all),},
	//{SEC_CMD("get_rawdata", get_rawdata),},
	{SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD("set_lowpower_mode", set_lowpower_mode),},
	{SEC_CMD("set_wirelesscharger_mode", wireless_charger_mode),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD("proximity_enable", proximity_enable),},
	{SEC_CMD("run_proximity_calibration", run_proximity_calibration),},
	//{SEC_CMD("get_proximity_calibration", get_proximity_calibration),},
#ifdef USE_PRESSURE_SENSOR
	//{SEC_CMD("run_force_calibration", run_force_calibration),},
	//{SEC_CMD("get_force_calibration", get_force_calibration),},
	//{SEC_CMD("run_pressure_strength_read_all", run_pressure_strength_read_all),},
	//{SEC_CMD("set_pressure_strength", set_pressure_strength),},
	//{SEC_CMD("get_pressure_strength", get_pressure_strength),},
	//{SEC_CMD("run_pressure_rawdata_read_all", run_pressure_rawdata_read_all),},
	//{SEC_CMD("set_pressure_rawdata", set_pressure_rawdata),},
	//{SEC_CMD("get_pressure_rawdata", get_pressure_rawdata),},
	//{SEC_CMD("get_pressure_threshold", get_pressure_threshold),},
#endif
#if USE_AOD
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	//{SEC_CMD("get_aod_rect", get_aod_rect),},
	{SEC_CMD("aod_enable", aod_enable),},
	{SEC_CMD("spay_enable", spay_enable),},
#endif /* USE_AOD */
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

/*
 * Sysfs - show scrub pos
 */
static ssize_t scrub_pos_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *cmd = dev_get_drvdata(dev);
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	u8 output[128] = {0};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	dev_dbg(&info->client->dev, "%s - scrub : id[%d] x[%d] y[%d]\n", __func__, info->scrub_id, info->scrub_x, info->scrub_y);
	snprintf(output, sizeof(output), "%d %d %d", info->scrub_id, info->scrub_x, info->scrub_y);

	info->scrub_x = 0;
	info->scrub_y = 0;

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return snprintf(buf, PAGE_SIZE, "%s", output);
}

static DEVICE_ATTR(scrub_pos, S_IRUGO, scrub_pos_show, NULL);

/*
 * Sysfs - show vendor
 */
static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *cmd = dev_get_drvdata(dev);
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	u8 output[16] = {0};

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	snprintf(output, 16, CHIP_NAME);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return snprintf(buf, PAGE_SIZE, "ME_%s", output);
}

static DEVICE_ATTR(vendor, S_IRUGO, vendor_show, NULL);

#if USE_SPONGE
/*
 * Sysfs - show LP dump
 */
static ssize_t get_lp_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *cmd = dev_get_drvdata(dev);
	struct mip4_ts_info *info = container_of(cmd, struct mip4_ts_info, cmd);
	u8 string_data[10] = {0, };
	u16 current_index;
	u8 dump_format, dump_num;
	u16 dump_start, dump_end;
	int i, ret;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	disable_irq(info->client->irq);

	ret = mip4_ts_sponge_read(info, D_BASE_LP_DUMP_REG_ADDR, string_data, 4);
	if (ret < 0) {
		dev_err(&info->client->dev, "%s: Failed to read rect\n", __func__);
		snprintf(buf, SEC_CMD_STR_LEN, "NG, Failed to read rect");
		goto exit;
	}
	dump_format = string_data[0];
	dump_num = string_data[1];
	dump_start = D_BASE_LP_DUMP_REG_ADDR + 4;
	dump_end = dump_start + (dump_format * (dump_num - 1));

	current_index = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
	if (current_index > dump_end || current_index < dump_start) {
		dev_err(&info->client->dev,
				"Failed to Sponge LP log %d\n", current_index);
		snprintf(buf, SEC_CMD_STR_LEN,
				"NG, Failed to Sponge LP log, current_index=%d",
				current_index);
		goto exit;
	}

	dev_info(&info->client->dev, "%s: DEBUG format=%d, num=%d, start=%d, end=%d, current_index=%d\n",
				__func__, dump_format, dump_num, dump_start, dump_end, current_index);

	for (i = dump_num - 1 ; i >= 0 ; i--) {
		u16 data0, data1, data2, data3, data4;
		char buff[30] = {0, };
		u16 string_addr;

		if (current_index < (dump_format * i))
			string_addr = (dump_format * dump_num) + current_index - (dump_format * i);
		else
			string_addr = current_index - (dump_format * i);

		if (string_addr < dump_start)
			string_addr += (dump_format * dump_num);

		//ret = mip4_ts_sponge_read(info, string_addr, string_data, dump_format);
		ret = mip4_ts_sponge_read(info, string_addr, string_data, 10);
		if (ret < 0) {
			dev_err(&info->client->dev,
					"%s: Failed to read rect\n", __func__);
			snprintf(buf, SEC_CMD_STR_LEN,
					"NG, Failed to read rect, addr=%d",
					string_addr);
			goto exit;
		}

		data0 = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
		data1 = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
		data2 = (string_data[5] & 0xFF) << 8 | (string_data[4] & 0xFF);
		data3 = (string_data[7] & 0xFF) << 8 | (string_data[6] & 0xFF);
		data4 = (string_data[9] & 0xFF) << 8 | (string_data[8] & 0xFF);

		if (data0 || data1 || data2 || data3 || data4) {
			if (dump_format == 10) {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x%04x\n",
						string_addr, data0, data1, data2, data3, data4);
			} else {
				snprintf(buff, sizeof(buff),
						"%d: %04x%04x%04x%04x\n",
						string_addr, data0, data1, data2, data3);
			}
			strncat(buf, buff, sizeof(buff));
		}
	}

exit:
	enable_irq(info->client->irq);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);

	return strlen(buf);
}

static DEVICE_ATTR(get_lp_dump, S_IRUGO, get_lp_dump_show, NULL);
#endif

/*
 * Sysfs - cmd attribute list
 */
static struct attribute *mip4_ts_cmd_attr[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_vendor.attr,
	//&dev_attr_pressure_enable.attr,
	//&dev_attr_proximity_enable.attr,
#if USE_SPONGE
	&dev_attr_get_lp_dump.attr,
#endif
	NULL,
};

/*
 * Sysfs - cmd attribute group
 */
static const struct attribute_group mip4_ts_cmd_attr_group = {
	.attrs = mip4_ts_cmd_attr,
};

#if USE_SEC_TOUCHKEY
/*
 * Sysfs - touchkey intensity
 */
static ssize_t touchkey_intensity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip4_ts_info *info = dev_get_drvdata(dev);
	int key_offset = info->node_x * info->node_y;
	int key_idx = -1;
	int retry = 3;

	if (!strcmp(attr->attr.name, "touchkey_recent")) {
		key_idx = 0;
	} else if (!strcmp(attr->attr.name, "touchkey_back")) {
		key_idx = 1;
	} else {
		dev_err(&info->client->dev, "%s [ERROR] Invalid attribute\n", __func__);
		goto error;
	}

	while (retry--) {
		if (info->test_busy == false) {
			break;
		}
		usleep_range(5 * 1000, 10 * 1000);
	}
	if (retry <= 0) {
		dev_err(&info->client->dev, "%s [ERROR] skip\n", __func__);
		goto exit;
	}

	if (mip4_ts_get_image(info, MIP4_IMG_TYPE_INTENSITY)) {
		dev_err(&info->client->dev, "%s [ERROR] mip4_ts_get_image\n", __func__);
		goto error;
	}

exit:
	dev_dbg(&info->client->dev, "%s - %s [%d]\n", __func__, attr->attr.name, info->image_buf[key_offset + key_idx]);
	return snprintf(buf, PAGE_SIZE, "%d", info->image_buf[key_offset + key_idx]);

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return snprintf(buf, PAGE_SIZE, "NG");
}

static DEVICE_ATTR(touchkey_recent, S_IRUGO, touchkey_intensity_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_intensity_show, NULL);

/*
 * Sysfs - touchkey threshold
 */
static ssize_t touchkey_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mip4_ts_info *info = dev_get_drvdata(dev);
	u8 wbuf[4];
	u8 rbuf[4];
	int threshold;

	wbuf[0] = MIP4_R0_INFO;
	wbuf[1] = MIP4_R1_INFO_CONTACT_THD_KEY;
	if (mip4_ts_i2c_read(info, wbuf, 2, rbuf, 2)) {
		goto error;
	}

	threshold = (rbuf[1] << 8) | rbuf[0];

	dev_dbg(&info->client->dev, "%s - threshold [%d]\n", __func__, threshold);
	return snprintf(buf, PAGE_SIZE, "%d", threshold);

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return snprintf(buf, PAGE_SIZE, "NG");
}

static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold_show, NULL);

/*
 * Sysfs - touchkey attr info
 */
static struct attribute *mip4_ts_cmd_key_attr[] = {
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_threshold.attr,
	NULL,
};

/*
 * Sysfs - touchkey attr group info
 */
static const struct attribute_group mip4_ts_cmd_key_attr_group = {
	.attrs = mip4_ts_cmd_key_attr,
};
#endif /* USE_SEC_TOUCHKEY */

#if USE_SEC_CMD_DEVICE
/*
 * Create command sysfs
 */
int mip4_ts_sysfs_cmd_create(struct mip4_ts_info *info)
{
	int ret = 0;

	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	ret = sec_cmd_init(&info->cmd, cmd_list, ARRAY_SIZE(cmd_list), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		dev_err(&info->client->dev, "%s [ERROR] sec_cmd_init\n", __func__);
		goto error;
	}

	ret = sysfs_create_group(&info->cmd.fac_dev->kobj, &mip4_ts_cmd_attr_group);
	if (ret < 0) {
		dev_err(&info->client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		goto error;
	}

	ret = sysfs_create_link(&info->cmd.fac_dev->kobj, &info->input_dev->dev.kobj, "input");
	if (ret < 0) {
		dev_err(&info->client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
		goto error;
	}

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
	return 0;

error:
	dev_err(&info->client->dev, "%s [ERROR]\n", __func__);
	return -1;
}

/*
 * Remove command sysfs
 */
void mip4_ts_sysfs_cmd_remove(struct mip4_ts_info *info)
{
	dev_dbg(&info->client->dev, "%s [START]\n", __func__);

	sysfs_delete_link(&info->cmd.fac_dev->kobj, &info->client->dev.kobj, "input");

	sysfs_remove_group(&info->cmd.fac_dev->kobj, &mip4_ts_cmd_attr_group);

	sec_cmd_exit(&info->cmd, SEC_CLASS_DEVT_TSP);

	dev_dbg(&info->client->dev, "%s [DONE]\n", __func__);
}

#else /* USE_SEC_CMD_DEVICE */
/*
 * Create command sysfs
 */
int mip4_ts_sysfs_cmd_create(struct mip4_ts_info *info)
{
	int i = 0;

	/* init */
	INIT_LIST_HEAD(&info->cmd_list_head);
	info->cmd_buffer_size = 0;

	for (i = 0; i < ARRAY_SIZE(mip4_ts_commands); i++) {
		list_add_tail(&mip4_ts_commands[i].list, &info->cmd_list_head);
		if (mip4_ts_commands[i].cmd_name) {
			info->cmd_buffer_size += strlen(mip4_ts_commands[i].cmd_name) + 1;
		}
	}

	info->cmd_busy = false;

	if (info->print_buf == NULL) {
		info->print_buf = devm_kzalloc(&info->client->dev, sizeof(u8) * 4096, GFP_KERNEL);
	}
	if (info->image_buf == NULL) {
		info->image_buf = devm_kzalloc(&info->client->dev, sizeof(int) * ((info->node_x * info->node_y) + info->node_key), GFP_KERNEL);
	}

	/* create sysfs */
	if (sysfs_create_group(&info->client->dev.kobj, &mip4_ts_cmd_attr_group)) {
		dev_err(&info->client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}

	/* create sec class */
#if USE_SEC_CLASS
	info->cmd_dev = device_create(sec_class, NULL, 0, info, "tsp");
#else
	info->cmd_class = class_create(THIS_MODULE, "sec");
	info->cmd_dev = device_create(info->cmd_class, NULL, 0, info, "tsp");
#endif
	if (sysfs_create_group(&info->cmd_dev->kobj, &mip4_ts_cmd_attr_group)) {
		dev_err(&info->client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
		return -EAGAIN;
	}

#if USE_SEC_TOUCHKEY
	if (info->key_enable == true) {
		/* create sysfs for touchkey */
		if (sysfs_create_group(&info->client->dev.kobj, &mip4_ts_cmd_key_attr_group)) {
			dev_err(&info->client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
			return -EAGAIN;
		}

		/* create sec class for touchkey */
#if USE_SEC_CLASS
		info->key_dev = device_create(sec_class, NULL, 0, info, "sec_touchkey");
#else
		info->key_dev = device_create(info->cmd_class, NULL, 0, info, "sec_touchkey");
#endif
		if (sysfs_create_group(&info->key_dev->kobj, &mip4_ts_cmd_key_attr_group)) {
			dev_err(&info->client->dev, "%s [ERROR] sysfs_create_group\n", __func__);
			return -EAGAIN;
		}
	}
#endif /* USE_SEC_TOUCHKEY */

	return 0;
}

/*
 * Remove command sysfs
 */
void mip4_ts_sysfs_cmd_remove(struct mip4_ts_info *info)
{
	sysfs_remove_group(&info->client->dev.kobj, &mip4_ts_cmd_attr_group);

	devm_kfree(&info->client->dev, info->print_buf);
}
#endif /* USE_SEC_CMD_DEVICE */
#endif /* USE_CMD */

