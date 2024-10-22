// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/input/sec_input/stm_fst1ba90a_i2c/stm_cmd.c
 *
 * Copyright (C) 2020 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "stm_ts.h"

enum ito_error_type {
	ITO_FORCE_SHRT_GND		= 0x60,
	ITO_SENSE_SHRT_GND		= 0x61,
	ITO_FORCE_SHRT_VDD		= 0x62,
	ITO_SENSE_SHRT_VDD		= 0x63,
	ITO_FORCE_SHRT_FORCE	= 0x64,
	ITO_SENSE_SHRT_SENSE	= 0x65,
	ITO_FORCE_OPEN			= 0x66,
	ITO_SENSE_OPEN			= 0x67,
	ITO_KEY_OPEN			= 0x68
};

#define STM_TS_COMP_DATA_HEADER_SIZE     16

enum stm_ts_nvm_data_type {		/* Write Command */
	STM_TS_NVM_OFFSET_FAC_RESULT = 1,
	STM_TS_NVM_OFFSET_CAL_COUNT,
	STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT,
	STM_TS_NVM_OFFSET_TUNE_VERSION,
	STM_TS_NVM_OFFSET_CAL_POSITION,
	STM_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT,
	STM_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP,
	STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO,
	STM_TS_NVM_OFFSET_CAL_FAIL_FLAG,
	STM_TS_NVM_OFFSET_CAL_FAIL_COUNT,
};

struct stm_ts_nvm_data_map {
	int type;
	int offset;
	int length;
};

#define NVM_CMD(mtype, moffset, mlength)		.type = mtype,	.offset = moffset,	.length = mlength

/* This Flash Meory Map is FIXED by STM firmware
 * Do not change MAP.
 */
struct stm_ts_nvm_data_map nvm_data[] = {
	{NVM_CMD(0,						0x00, 0),},
	{NVM_CMD(STM_TS_NVM_OFFSET_FAC_RESULT,			0x00, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_CAL_COUNT,			0x01, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT,		0x02, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_TUNE_VERSION,			0x03, 2),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_CAL_POSITION,			0x05, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT,		0x06, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP,		0x07, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO,		0x08, 20),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_CAL_FAIL_FLAG,			0x1C, 1),},	/* SEC */
	{NVM_CMD(STM_TS_NVM_OFFSET_CAL_FAIL_COUNT,			0x1D, 1),},	/* SEC */
};
#define STM_TS_NVM_OFFSET_ALL	31

int set_nvm_data_by_size(struct stm_ts_data *ts, u8 offset, int length, u8 *buf)
{
	u8 regAdd[256] = { 0 };
	u8 remaining, index, sendinglength;
	int ret;

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true); // Clear FIFO

	stm_ts_release_all_finger(ts);

	remaining = length;
	index = 0;
	sendinglength = 0;

	while (remaining) {
		regAdd[0] = 0xC7;
		regAdd[1] = 0x01;
		regAdd[2] = offset + index;

		// write data up to 12 bytes available
		if (remaining < 13) {
			memcpy(&regAdd[3], &buf[index], remaining);
			sendinglength = remaining;
		} else {
			memcpy(&regAdd[3], &buf[index], 12);
			index += 12;
			sendinglength = 12;
		}

		ret = ts->stm_ts_write(ts, &regAdd[0], sendinglength + 3);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: write failed. ret: %d\n", __func__, ret);
			return ret;
		}
		remaining -= sendinglength;
	}

	sec_input_irq_disable(ts->plat_data);

	// Save to flash
	regAdd[0] = 0xA4;
	regAdd[1] = 0x05;
	regAdd[2] = 0x04; // panel configuration area
	ret = stm_ts_wait_for_echo_event(ts, &regAdd[0], 3, 200);
	if (ret < 0)
		input_err(true, &ts->client->dev,
				"%s: save to flash failed. ret: %d\n", __func__, ret);

	sec_input_irq_enable(ts->plat_data);

	return ret;
}

int set_nvm_data(struct stm_ts_data *ts, u8 type, u8 *buf)
{
	return set_nvm_data_by_size(ts, nvm_data[type].offset, nvm_data[type].length, buf);
}

int get_nvm_data_by_size(struct stm_ts_data *ts, u8 offset, int length, u8 *nvdata)
{
	u8 regAdd[3] = {0};
	u8 data[128] = { 0 };
	int ret;

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true); // Clear FIFO

	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);

	// Request SEC factory debug data from flash
	regAdd[0] = 0xA4;
	regAdd[1] = 0x06;
	regAdd[2] = 0x90;
	ret = stm_ts_wait_for_echo_event(ts, &regAdd[0], 3, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed to request data. ret: %d\n", __func__, ret);
		sec_input_irq_enable(ts->plat_data);
		return ret;
	}

	sec_input_irq_enable(ts->plat_data);

	regAdd[0] = 0xA6;
	regAdd[1] = 0x00;
	regAdd[2] = offset;

	ret = ts->stm_ts_read(ts, &regAdd[0], 3, data, length + 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: read failed. ret: %d\n", __func__, ret);
		return ret;
	}

	memcpy(nvdata, &data[0], length);

	input_raw_info(true, &ts->client->dev, "%s: offset [%d], length [%d]\n",
			__func__, offset, length);

	return ret;
}

int get_nvm_data(struct stm_ts_data *ts, int type, u8 *nvdata)
{
	int size = sizeof(nvm_data) / sizeof(struct stm_ts_nvm_data_map);

	if (type >= size)
		return -EINVAL;

	return get_nvm_data_by_size(ts, nvm_data[type].offset, nvm_data[type].length, nvdata);
}

static ssize_t hw_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_INPUT_HW_PARAM_SIZE];
	char tbuff[SEC_CMD_STR_LEN];
	char mdev[SEC_CMD_STR_LEN];

	memset(mdev, 0x00, sizeof(mdev));
	if (GET_DEV_COUNT(ts->plat_data->multi_dev) == MULTI_DEV_SUB)
		snprintf(mdev, sizeof(mdev), "%s", "2");
	else
		snprintf(mdev, sizeof(mdev), "%s", "");

	memset(buff, 0x00, sizeof(buff));

	sec_input_get_common_hw_param(ts->plat_data, buff);

	/* module_id */
	memset(tbuff, 0x00, sizeof(tbuff));

	snprintf(tbuff, sizeof(tbuff), ",\"TMOD%s\":\"ST%02X%02X%02X%c%01X\"",
			mdev, ts->plat_data->img_version_of_bin[1], ts->plat_data->img_version_of_bin[2],
			ts->plat_data->img_version_of_bin[3],
#ifdef TCLM_CONCEPT
			ts->tdata->tclm_string[ts->tdata->nvdata.cal_position].s_name,
			ts->tdata->nvdata.cal_count & 0xF);
#else
			'0', 0);
#endif
	strlcat(buff, tbuff, sizeof(buff));

	/* vendor_id */
	memset(tbuff, 0x00, sizeof(tbuff));
	if (ts->plat_data->firmware_name) {
		char temp[11];

		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, 9, "%s", ts->plat_data->firmware_name + 8);
		snprintf(tbuff, sizeof(tbuff), "\"TVEN%s\":\"STM_%s\"", mdev, temp);
	} else {
		snprintf(tbuff, sizeof(tbuff), "\"TVEN%s\":\"STM\"", mdev);
	}
	strlcat(buff, tbuff, sizeof(buff));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t hw_param_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	sec_input_clear_common_hw_param(ts->plat_data);

	return count;
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	unsigned char wbuf[2] = { 0 };
	unsigned long value = 0;
	int ret = 0;

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return -EIO;

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return -EPERM;
	}

	wbuf[0] = STM_TS_CMD_SENSITIVITY_MODE;
	if (value)
		wbuf[1] = 0x24; /* enable */
	else
		wbuf[1] = 0xFF; /* disable */

	ret = ts->stm_ts_write(ts, wbuf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: write failed. ret: %d\n", __func__, ret);
		return ret;
	}

	sec_delay(30);

	input_info(true, &ts->client->dev, "%s: %lu\n", __func__, value);
	return count;
}

#define STM_TS_SENSITIVITY_POINT_NUM	9
static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	u8 rbuf[STM_TS_SENSITIVITY_POINT_NUM * 2] = { 0 };
	u8 reg_read = STM_TS_READ_SENSITIVITY_VALUE;
	int ret, i;
	s16 value[STM_TS_SENSITIVITY_POINT_NUM];
	char temp[10] = { 0 };

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n", __func__);
		return -EPERM;
	}

	ret = ts->stm_ts_read(ts, &reg_read, 1, rbuf, STM_TS_SENSITIVITY_POINT_NUM * 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read failed ret = %d\n", __func__, ret);
		return ret;
	}

	for (i = 0; i < STM_TS_SENSITIVITY_POINT_NUM; i++) {
		value[i] = rbuf[i * 2] | (rbuf[i * 2 + 1] << 8);
		if (i != 0)
			strlcat(buf, ",", SEC_CMD_BUF_SIZE);
		snprintf(temp, 10, "%d", value[i]);
		strlcat(buf, temp, SEC_CMD_BUF_SIZE);
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buf);

	return strlen(buf);
}

static ssize_t get_lp_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	u8 string_data[10] = {0, };
	u16 current_index;
	u8 dump_format, dump_num;
	u16 dump_start, dump_end;
	int i, ret;
	u16 addr;

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "TSP turned off");
	}

	mutex_lock(&ts->plat_data->enable_mutex);

	sec_input_irq_disable(ts->plat_data);

	addr = STM_TS_CMD_SPONGE_LP_DUMP;

	ret = ts->stm_ts_read_from_sponge(ts, addr, string_data, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to read from Sponge, addr=0x%X\n", __func__, addr);
		snprintf(buf, SEC_CMD_BUF_SIZE,
				"NG, Failed to read from Sponge, addr=0x%X", addr);
		goto out;
	}

	dump_format = string_data[0];
	dump_num = string_data[1];
	dump_start = STM_TS_CMD_SPONGE_LP_DUMP + 4;
	dump_end = dump_start + (dump_format * (dump_num - 1));

	current_index = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
	if (current_index > dump_end || current_index < dump_start) {
		input_err(true, &ts->client->dev,
				"Failed to Sponge LP log %d\n", current_index);
		snprintf(buf, SEC_CMD_BUF_SIZE,
				"NG, Failed to Sponge LP log, current_index=%d",
				current_index);
		goto out;
	}

	input_info(true, &ts->client->dev, "%s: DEBUG format=%d, num=%d, start=%d, end=%d, current_index=%d\n",
				__func__, dump_format, dump_num, dump_start, dump_end, current_index);

	for (i = dump_num - 1; i >= 0; i--) {
		u16 data0, data1, data2, data3, data4;
		char buff[30] = {0, };
		u16 string_addr;

		if (current_index < (dump_format * i))
			string_addr = (dump_format * dump_num) + current_index - (dump_format * i);
		else
			string_addr = current_index - (dump_format * i);

		if (string_addr < dump_start)
			string_addr += (dump_format * dump_num);

		addr = string_addr;

		ret = ts->stm_ts_read_from_sponge(ts, addr, string_data, dump_format);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: Failed to read from Sponge, addr=0x%X\n", __func__, addr);
			snprintf(buf, SEC_CMD_BUF_SIZE,
					"NG, Failed to read from Sponge, addr=0x%X", addr);
			goto out;
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
			strlcat(buf, buff, PAGE_SIZE);
		}
	}

out:
	sec_input_irq_enable(ts->plat_data);
	mutex_unlock(&ts->plat_data->enable_mutex);

	return strlen(buf);
}

static ssize_t fod_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	return sec_input_get_fod_info(&ts->client->dev, buf);
}

static ssize_t fod_pos_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[3] = { 0 };
	int ret, i;
	u8 data[255] = { 0 };

	if (!ts->plat_data->support_fod) {
		input_err(true, &ts->client->dev, "%s: fod is not supported\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NA");
	}

	if (!ts->plat_data->fod_data.vi_size) {
		input_err(true, &ts->client->dev, "%s: fod vi size is 0\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	ret = ts->stm_ts_read_from_sponge(ts, STM_TS_CMD_SPONGE_FOD_POSITION,
			data, ts->plat_data->fod_data.vi_size);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	for (i = 0; i < ts->plat_data->fod_data.vi_size; i++) {
		snprintf(buff, 3, "%02X", data[i]);
		strlcat(buf, buff, SEC_CMD_BUF_SIZE);
	}

	return strlen(buf);
}

static ssize_t debug_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[INPUT_DEBUG_INFO_SIZE];
	char tbuff[128];

	if (sizeof(tbuff) > INPUT_DEBUG_INFO_SIZE)
		input_info(true, &ts->client->dev, "%s: buff_size[%d], tbuff_size[%ld]\n",
				__func__, INPUT_DEBUG_INFO_SIZE, sizeof(tbuff));

	memset(buff, 0x00, sizeof(buff));
	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "FW: (I)%02X%02X%02X%02X / (B)%02X%02X%02X%02X\n",
			ts->plat_data->img_version_of_ic[0], ts->plat_data->img_version_of_ic[1],
			ts->plat_data->img_version_of_ic[2], ts->plat_data->img_version_of_ic[3],
			ts->plat_data->img_version_of_bin[0], ts->plat_data->img_version_of_bin[1],
			ts->plat_data->img_version_of_bin[2], ts->plat_data->img_version_of_bin[3]);
	strlcat(buff, tbuff, sizeof(buff));

#ifdef TCLM_CONCEPT
	if (ts->tdata->tclm_level != TCLM_LEVEL_NOT_SUPPORT) {
		memset(tbuff, 0x00, sizeof(tbuff));
		snprintf(tbuff, sizeof(tbuff), "C%02XT%04X.%4s%s Cal_flag:%d fail_cnt:%d\n",
			ts->tdata->nvdata.cal_count, ts->tdata->nvdata.tune_fix_ver,
			ts->tdata->tclm_string[ts->tdata->nvdata.cal_position].f_name,
			(ts->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L" : " ",
			ts->tdata->nvdata.cal_fail_flag, ts->tdata->nvdata.cal_fail_cnt);
		strlcat(buff, tbuff, sizeof(buff));
	}
#endif

	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), "ic_reset_count %d\nchecksum bit %d\n",
			ts->plat_data->hw_param.ic_reset_count,
			ts->plat_data->hw_param.checksum_result);
	strlcat(buff, tbuff, sizeof(buff));

	sec_input_get_multi_device_debug_info(ts->plat_data->multi_dev, buff, sizeof(buff));

	input_info(true, &ts->client->dev, "%s: size: %ld\n", __func__, strlen(buff));

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

DEVICE_ATTR_RW(hw_param);
DEVICE_ATTR_RW(sensitivity_mode);
DEVICE_ATTR_RO(get_lp_dump);
DEVICE_ATTR_RO(fod_info);
DEVICE_ATTR_RO(fod_pos);
DEVICE_ATTR_RO(debug_info);

static struct attribute *cmd_attributes[] = {
	&dev_attr_hw_param.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_get_lp_dump.attr,
	&dev_attr_fod_info.attr,
	&dev_attr_fod_pos.attr,
	&dev_attr_debug_info.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

static void enter_factory_mode(struct stm_ts_data *ts, bool fac_mode)
{
	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF))
		return;

	ts->stm_ts_systemreset(ts, 50);

	stm_ts_release_all_finger(ts);

	if (fac_mode) {
		// Auto-Tune without saving
		stm_ts_execute_autotune(ts, false);

		sec_delay(50);
	}

	stm_ts_set_scanmode(ts, ts->scan_mode);
}

int stm_ts_get_channel_info(struct stm_ts_data *ts)
{
	int rc = -1;
	u8 regAdd[1] = { STM_TS_READ_PANEL_INFO };
	u8 data[STM_TS_EVENT_SIZE] = { 0 };
	int ic_x_resolution, ic_y_resolution;

	memset(data, 0x0, STM_TS_EVENT_SIZE);

	rc = ts->stm_ts_read(ts, regAdd, 1, data, 11);
	if (rc < 0) {
		ts->plat_data->y_node_num = 0;
		ts->plat_data->x_node_num  = 0;
		input_err(true, &ts->client->dev, "%s: Get channel info Read Fail!!\n", __func__);
		return rc;
	}

	ts->plat_data->y_node_num = data[8]; // Number of TX CH
	ts->plat_data->x_node_num = data[9]; // Number of RX CH

	if (!(ts->plat_data->y_node_num > 0 && ts->plat_data->y_node_num <= STM_TS_MAX_NUM_FORCE &&
		ts->plat_data->x_node_num > 0 && ts->plat_data->x_node_num  <= STM_TS_MAX_NUM_SENSE)) {
		ts->plat_data->y_node_num = STM_TS_MAX_NUM_FORCE;
		ts->plat_data->x_node_num  = STM_TS_MAX_NUM_SENSE;
		input_err(true, &ts->client->dev, "%s: set channel num based on max value, check it!\n", __func__);
	}

	ic_x_resolution = (data[0] << 8) | data[1]; // X resolution of IC
	ic_y_resolution = (data[2] << 8) | data[3]; // Y resolution of IC

	input_info(true, &ts->client->dev, "%s: RX:Sense(%02d) TX:Force(%02d) resolution:(IC)x:%d y:%d, (DT)x:%d,y:%d\n",
		__func__, ts->plat_data->x_node_num, ts->plat_data->y_node_num,
		ic_x_resolution, ic_y_resolution, ts->plat_data->max_x, ts->plat_data->max_y);

	if (ic_x_resolution > 0 && ic_x_resolution <= STM_TS_MAX_X_RESOLUTION &&
		ic_y_resolution > 0 && ic_y_resolution <= STM_TS_MAX_Y_RESOLUTION &&
		ts->plat_data->max_x != ic_x_resolution && ts->plat_data->max_y != ic_y_resolution) {
		ts->plat_data->max_x = ic_x_resolution;
		ts->plat_data->max_y = ic_y_resolution;
		input_err(true, &ts->client->dev, "%s: set resolution based on ic value, check it!\n", __func__);
	}

	return rc;
}

static void stm_ts_print_channel(struct stm_ts_data *ts, s16 *tx_data, s16 *rx_data)
{
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int len, max_num  = 0;
	int i = 0, k = 0;
	int tx_count = ts->plat_data->y_node_num;
	int rx_count = ts->plat_data->x_node_num;

	if (tx_count > 20)
		max_num = 10;
	else
		max_num = tx_count;

	if (!max_num)
		return;

	len = 7 * (max_num + 1);
	pStr = vzalloc(len);
	if (!pStr)
		return;

	/* Print TX channel */
	memset(pStr, 0x0, len);
	snprintf(pTmp, sizeof(pTmp), " TX");
	strlcat(pStr, pTmp, len);

	for (k = 0; k < max_num; k++) {
		snprintf(pTmp, sizeof(pTmp), "    %02d", k);
		strlcat(pStr, pTmp, len);
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, len);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, len);

	for (k = 0; k < max_num; k++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strlcat(pStr, pTmp, len);
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, len);
	snprintf(pTmp, sizeof(pTmp), " | ");
	strlcat(pStr, pTmp, len);

	for (i = 0; i < tx_count; i++) {
		if (i && i % max_num == 0) {
			input_raw_info(true, &ts->client->dev, "%s\n", pStr);
			memset(pStr, 0x0, len);
			snprintf(pTmp, sizeof(pTmp), " | ");
			strlcat(pStr, pTmp, len);
		}

		snprintf(pTmp, sizeof(pTmp), " %5d", tx_data[i]);
		strlcat(pStr, pTmp, len);
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	input_raw_info(true, &ts->client->dev, "\n");


	/* Print RX channel */
	memset(pStr, 0x0, len);
	snprintf(pTmp, sizeof(pTmp), " RX");
	strlcat(pStr, pTmp, len);

	for (k = 0; k < max_num; k++) {
		snprintf(pTmp, sizeof(pTmp), "    %02d", k);
		strlcat(pStr, pTmp, len);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, len);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, len);

	for (k = 0; k < max_num; k++) {
		snprintf(pTmp, sizeof(pTmp), "------");
		strlcat(pStr, pTmp, len);
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, len);
	snprintf(pTmp, sizeof(pTmp), " | ");
	strlcat(pStr, pTmp, len);

	for (i = 0; i < rx_count; i++) {
		if (i && i % max_num == 0) {
			input_raw_info(true, &ts->client->dev, "%s\n", pStr);
			memset(pStr, 0x0, len);
			snprintf(pTmp, sizeof(pTmp), " | ");
			strlcat(pStr, pTmp, len);
		}

		snprintf(pTmp, sizeof(pTmp), " %5d", rx_data[i]);
		strlcat(pStr, pTmp, len);
	}
	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	vfree(pStr);
}

void stm_ts_print_frame(struct stm_ts_data *ts, short *frame, short *min, short *max)
{
	int i = 0;
	int j = 0;
	u8 *pStr = NULL;
	u8 pTmp[16] = { 0 };
	int lsize = 6 * (ts->plat_data->x_node_num  + 1);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (pStr == NULL)
		return;

	snprintf(pTmp, 4, "    ");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->plat_data->x_node_num ; i++) {
		snprintf(pTmp, 6, "Rx%02d  ", i);
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, 2, " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->plat_data->x_node_num ; i++) {
		snprintf(pTmp, 6, "------");
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, &ts->client->dev, "%s\n", pStr);

	for (i = 0; i < ts->plat_data->y_node_num; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, 7, "Tx%02d | ", i);
		strlcat(pStr, pTmp, lsize);

		for (j = 0; j < ts->plat_data->x_node_num ; j++) {
			snprintf(pTmp, 6, "%5d ", frame[(i * ts->plat_data->x_node_num) + j]);
			strlcat(pStr, pTmp, lsize);

			if (i > 0) { /* TX00 is intentionally excluded for preventing the defect judgment */
				if (frame[(i * ts->plat_data->x_node_num) + j] < *min)
					*min = frame[(i * ts->plat_data->x_node_num) + j];

				if (frame[(i * ts->plat_data->x_node_num) + j] > *max)
					*max = frame[(i * ts->plat_data->x_node_num) + j];
			}
		}
		input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	}

	input_raw_info(true, &ts->client->dev, "%s, min:%d, max:%d\n", __func__, *min, *max);

	kfree(pStr);
}

int stm_ts_read_frame(struct stm_ts_data *ts, u8 type, short *frame, short *min, short *max)
{
	struct stm_ts_syncframeheader *pSyncFrameHeader;
	u8 regAdd[8] = { 0 };
	unsigned int totalbytes = 0;
	u8 *pRead;
	int ret;
	int i = 0;
	int retry = 10;

	pRead = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 3 + 1, GFP_KERNEL);
	if (!pRead)
		return -ENOMEM;

	/* Request Data Type */
	regAdd[0] = 0xA4;
	regAdd[1] = 0x06;
	regAdd[2] = (u8)type;
	ret = ts->stm_ts_write(ts, &regAdd[0], 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to request data\n", __func__);
		kfree(pRead);
		return ret;
	}

	sec_delay(50);

	do {
		regAdd[0] = 0xA6;
		regAdd[1] = 0x00;
		regAdd[2] = 0x00;
		ret = ts->stm_ts_read(ts, &regAdd[0], 3, &pRead[0], STM_TS_COMP_DATA_HEADER_SIZE);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: read failed rc = %d\n", __func__, ret);
			kfree(pRead);
			return ret;
		}

		pSyncFrameHeader = (struct stm_ts_syncframeheader *) &pRead[0];

		if ((pSyncFrameHeader->header == 0xA5) && (pSyncFrameHeader->host_data_mem_id == type))
			break;

		sec_delay(100);
	} while (retry--);

	if (retry == 0) {
		input_err(true, &ts->client->dev,
				"%s: didn't match header or id. header = %02X, id = %02X\n",
				__func__, pSyncFrameHeader->header, pSyncFrameHeader->host_data_mem_id);
		kfree(pRead);
		return -EIO;
	}

	totalbytes = (ts->plat_data->y_node_num * ts->plat_data->x_node_num   * 2);

	regAdd[0] = 0xA6;
	regAdd[1] = 0x00;
	regAdd[2] = STM_TS_COMP_DATA_HEADER_SIZE + pSyncFrameHeader->dbg_frm_len;
	ret = ts->stm_ts_read(ts, &regAdd[0], 3, &pRead[0], totalbytes);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read failed rc = %d\n", __func__, ret);
		kfree(pRead);
		return ret;
	}

	for (i = 0; i < totalbytes / 2; i++)
		frame[i] = (short)(pRead[i * 2] + (pRead[i * 2 + 1] << 8));

	switch (type) {
	case TYPE_RAW_DATA:
		input_raw_info(true, &ts->client->dev, "%s: [Raw Data]\n", __func__);
		break;
	case TYPE_STRENGTH_DATA:
		input_raw_info(true, &ts->client->dev, "%s: [Strength Data]\n", __func__);
		break;
	case TYPE_BASELINE_DATA:
		input_raw_info(true, &ts->client->dev, "%s: [Baseline Data]\n", __func__);
		break;
	}

	stm_ts_print_frame(ts, frame, min, max);
	kfree(pRead);
	return 0;
}

int stm_ts_panel_ito_test(struct stm_ts_data *ts, int testmode)
{
	u8 cmd = STM_TS_READ_ONE_EVENT;
	u8 regAdd[4] = { 0 };
	u8 data[STM_TS_EVENT_SIZE];
	int i;
	bool matched = false;
	int retry = 0, ret;
	int result = 0;

	ts->stm_ts_systemreset(ts, 0);

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);

	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);
	mutex_lock(&ts->wait_for);

	regAdd[0] = 0xA4;
	regAdd[1] = 0x04;
	switch (testmode) {
	case OPEN_TEST:
		regAdd[2] = 0x00;
		regAdd[3] = 0x30;
		break;

	case OPEN_SHORT_CRACK_TEST:
	default:
		regAdd[2] = 0xFF;
		regAdd[3] = 0x01;
		break;
	}

	ret = ts->stm_ts_write(ts, &regAdd[0], 4); // ITO test command
	if (ret < 0) {
		mutex_unlock(&ts->wait_for);
		input_info(true, &ts->client->dev,
				"%s: failed to write ITO test cmd\n", __func__);
		result = -ITO_FAIL;
		goto out;
	}

	sec_delay(100);

	memset(ts->plat_data->hw_param.ito_test, 0x0, 4);

	memset(data, 0x0, STM_TS_EVENT_SIZE);
	while (ts->stm_ts_read(ts, &cmd, 1, (u8 *)data, STM_TS_EVENT_SIZE)) {
		if ((data[0] == STM_TS_EVENT_STATUS_REPORT) && (data[1] == 0x01)) {  // Check command ECHO - finished
			for (i = 0; i < 4; i++) {
				if (data[i + 2] != regAdd[i]) {
					matched = false;
					break;
				}
				matched = true;
			}

			if (matched == true)
				break;
		} else if (data[0] == STM_TS_EVENT_ERROR_REPORT) {
			ts->plat_data->hw_param.ito_test[0] = data[0];
			ts->plat_data->hw_param.ito_test[1] = data[1];
			ts->plat_data->hw_param.ito_test[2] = data[2];
			ts->plat_data->hw_param.ito_test[3] = 0x00;
			result = -ITO_FAIL;

			switch (data[1]) {
			case ITO_FORCE_SHRT_GND:
				input_info(true, &ts->client->dev, "%s: Force channel [%d] short to GND\n",
						__func__, data[2]);
				result = -ITO_FAIL_SHORT;
				break;

			case ITO_SENSE_SHRT_GND:
				input_info(true, &ts->client->dev, "%s: Sense channel [%d] short to GND\n",
						__func__, data[2]);
				result = -ITO_FAIL_SHORT;
				break;

			case ITO_FORCE_SHRT_VDD:
				input_info(true, &ts->client->dev, "%s: Force channel [%d] short to VDD\n",
						__func__, data[2]);
				result = -ITO_FAIL_SHORT;
				break;

			case ITO_SENSE_SHRT_VDD:
				input_info(true, &ts->client->dev, "%s: Sense channel [%d] short to VDD\n",
						__func__, data[2]);
				result = -ITO_FAIL_SHORT;
				break;

			case ITO_FORCE_SHRT_FORCE:
				input_info(true, &ts->client->dev, "%s: Force channel [%d] short to force\n",
						__func__, data[2]);
				result = -ITO_FAIL_SHORT;
				break;

			case ITO_SENSE_SHRT_SENSE:
				input_info(true, &ts->client->dev, "%s: Sennse channel [%d] short to sense\n",
						__func__, data[2]);
				result = -ITO_FAIL_SHORT;
				break;

			case ITO_FORCE_OPEN:
				input_info(true, &ts->client->dev, "%s: Force channel [%d] open\n",
						__func__, data[2]);
				result = -ITO_FAIL_OPEN;
				break;

			case ITO_SENSE_OPEN:
				input_info(true, &ts->client->dev, "%s: Sense channel [%d] open\n",
						__func__, data[2]);
				result = -ITO_FAIL_OPEN;
				break;

			case ITO_KEY_OPEN:
				input_info(true, &ts->client->dev, "%s: Key channel [%d] open\n",
						__func__, data[2]);
				result = -ITO_FAIL_OPEN;
				break;

			default:
				input_info(true, &ts->client->dev, "%s: unknown event %02x %02x %02x %02x %02x %02x %02x %02x\n",
						__func__, data[0], data[1], data[2], data[3],
						data[4], data[5], data[6], data[7]);
				break;
			}
		}

		if (retry++ > 50) {
			result = -ITO_FAIL;
			input_err(true, &ts->client->dev, "%s: Time over - wait for result of ITO test\n", __func__);
			break;
		}
		sec_delay(20);
	}

	mutex_unlock(&ts->wait_for);

out:
	ts->stm_ts_systemreset(ts, 0);

	stm_ts_set_cover_type(ts, ts->flip_enable);

	sec_delay(10);

	if (ts->charger_mode) {
		stm_ts_charger_mode(ts);
		sec_delay(10);
	}

	stm_ts_set_scanmode(ts, ts->scan_mode);

	input_raw_info(true, &ts->client->dev, "%s: mode:%d [%s] %02X %02X %02X %02X\n",
			__func__, testmode, result < 0 ? "FAIL" : "PASS",
			ts->plat_data->hw_param.ito_test[0], ts->plat_data->hw_param.ito_test[1],
			ts->plat_data->hw_param.ito_test[2], ts->plat_data->hw_param.ito_test[3]);

	return result;
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int retval = 0;

	mutex_lock(&ts->plat_data->enable_mutex);
	retval = stm_ts_fw_update_on_hidden_menu(ts, sec->cmd_param[0]);
	if (retval < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &ts->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &ts->client->dev, "%s: success [%d]\n", __func__, retval);
	}
	mutex_unlock(&ts->plat_data->enable_mutex);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[16] = { 0 };

	snprintf(buff, sizeof(buff), "%s%02X%02X%02X%02X",
			ts->plat_data->ic_vendor_name,
			ts->plat_data->img_version_of_bin[SEC_INPUT_FW_IC_VER],
			ts->plat_data->img_version_of_bin[SEC_INPUT_FW_VER_PROJECT_ID],
			ts->plat_data->img_version_of_bin[SEC_INPUT_FW_MODULE_VER],
			ts->plat_data->img_version_of_bin[SEC_INPUT_FW_VER]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[16] = { 0 };
	char model_ver[7] = { 0 };

	ts->stm_ts_get_version_info(ts);

	snprintf(buff, sizeof(buff), "%s%02X%02X%02X%02X",
			ts->plat_data->ic_vendor_name,
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_IC_VER],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER_PROJECT_ID],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_MODULE_VER],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER]);
	snprintf(model_ver, sizeof(model_ver), "%s%02X%02X",
			ts->plat_data->ic_vendor_name,
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_IC_VER],
			ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER_PROJECT_ID]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model_ver, strnlen(model_ver, sizeof(model_ver)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void module_off_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret = 0;

	ret = ts->plat_data->stop_device(ts);
	if (ret == 0)
		sec->cmd_state = SEC_CMD_STATUS_OK;
	else
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

static void module_on_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret = 0;

	ret = ts->plat_data->start_device(ts);
	if (ret == 0)
		sec->cmd_state = SEC_CMD_STATUS_OK;
	else
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[16] = { 0 };

	snprintf(buff, sizeof(buff), "STM");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[16] = { 0 };

	if (ts->plat_data->firmware_name)
		memcpy(buff, ts->plat_data->firmware_name + 8, 9);
	else
		snprintf(buff, 10, "FST");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_wet_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 regAdd[3] = { 0 };
	u8 data[2] = { 0 };
	int ret;

	regAdd[0] = 0xC7;
	regAdd[1] = 0x03;
	ret = ts->stm_ts_read(ts, &regAdd[0], 2, &data[0], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: [ERROR] failed to read\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "WET_MODE");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	snprintf(buff, sizeof(buff), "%d", data[0]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "WET_MODE");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[16] = { 0 };

	snprintf(buff, sizeof(buff), "%d", ts->plat_data->x_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[16] = { 0 };

	snprintf(buff, sizeof(buff), "%d", ts->plat_data->y_node_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[16] = { 0 };
	int rc;
	u32 checksum_data;

	rc = ts->stm_ts_systemreset(ts, 0);
	if (rc < 0)
		goto NG;

	rc = ts->stm_ts_get_sysinfo_data(ts, STM_TS_SI_CONFIG_CHECKSUM, 4, (u8 *)&checksum_data);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: Get checksum data Read Fail!! [Data : %08X]\n",
				__func__, checksum_data);
		goto NG;
	}
	ts->plat_data->init(ts);

	snprintf(buff, sizeof(buff), "%08X", checksum_data);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
	return;
NG:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

static void check_fw_corruption(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int rc = 0;

	if (!ts->fw_corruption) {
		rc = stm_ts_fw_corruption_check(ts);
		if (rc != -STM_TS_ERROR_FW_CORRUPTION)
			goto out;
		ts->stm_ts_wait_for_ready(ts);
	}

	input_err(true, &ts->client->dev,
			"%s: do autotune and retry check corruption\n", __func__);

	if (stm_ts_execute_autotune(ts, true) < 0) {
		if (rc == STM_TS_ERROR_FW_CORRUPTION)
			goto out;
	}

	rc = stm_ts_fw_corruption_check(ts);
out:
	if (rc == -STM_TS_ERROR_FW_CORRUPTION) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;

		ts->stm_ts_systemreset(ts, 0);
		if (ts->osc_trim_error)
			stm_ts_osc_trim_recovery(ts);

		ts->plat_data->init(ts);
	}
	ts->fw_corruption = false;
	ts->osc_trim_error = false;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

int stm_ts_fw_wait_for_jitter_result(struct stm_ts_data *ts, u8 *reg, u8 count, s16 *ret1, s16 *ret2)
{
	int rc = 0;
	u8 regAdd;
	u8 data[STM_TS_EVENT_SIZE];
	int retry = 0;

	mutex_lock(&ts->wait_for);

	rc = ts->stm_ts_write(ts, reg, count);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write command\n", __func__);
		mutex_unlock(&ts->wait_for);
		return rc;
	}

	memset(data, 0x0, STM_TS_EVENT_SIZE);

	regAdd = STM_TS_READ_ONE_EVENT;
	rc = -1;
	while (ts->stm_ts_read(ts, &regAdd, 1, (u8 *)data, STM_TS_EVENT_SIZE)) {
		if (data[0] != 0x00)
			input_info(true, &ts->client->dev,
					"%s: event %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
					__func__, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

		if ((data[0] == STM_TS_EVENT_JITTER_RESULT) && (data[1] == 0x03)) {// Check Jitter result
			if (data[2] == STM_TS_EVENT_JITTER_MUTUAL_MAX) {
				*ret2 = (s16)((data[3] << 8) + data[4]);
				input_info(true, &ts->client->dev, "%s: Mutual max jitter Strength : %d, RX:%d, TX:%d\n",
						__func__, *ret2, data[5], data[6]);
			} else if (data[2] == STM_TS_EVENT_JITTER_MUTUAL_MIN) {
				*ret1 = (s16)((data[3] << 8) + data[4]);
				input_info(true, &ts->client->dev, "%s: Mutual min jitter Strength : %d, RX:%d, TX:%d\n",
							__func__, *ret1, data[5], data[6]);
				rc = 0;
				break;
			} else if (data[2] == STM_TS_EVENT_JITTER_SELF_TX_P2P) {
				*ret1 = (s16)((data[3] << 8) + data[4]);
				input_info(true, &ts->client->dev, "%s: Self TX P2P jitter Strength : %d, TX:%d\n",
						__func__, *ret1, data[6]);
			} else if (data[2] == STM_TS_EVENT_JITTER_SELF_RX_P2P) {
				*ret2 = (s16)((data[3] << 8) + data[4]);
				input_info(true, &ts->client->dev, "%s: Self RX P2P jitter Strength : %d, RX:%d\n",
						__func__, *ret2, data[5]);
				rc = 0;
				break;
			}
		} else if (data[0] == STM_TS_EVENT_ERROR_REPORT) {
			input_info(true, &ts->client->dev, "%s: Error detected %02X,%02X,%02X,%02X,%02X,%02X\n",
				__func__, data[0], data[1], data[2], data[3], data[4], data[5]);
			break;
		}

		if (retry++ > STM_TS_RETRY_COUNT * 10) {
			rc = -1;
			input_err(true, &ts->client->dev, "%s: Time Over (%02X,%02X,%02X,%02X,%02X,%02X)\n",
				__func__, data[0], data[1], data[2], data[3], data[4], data[5]);
			break;
		}
		sec_delay(20);
	}

	mutex_unlock(&ts->wait_for);
	return rc;
}

static void run_jitter_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 regAdd[4] = { 0 };
	int ret;
	s16 mutual_min = 0, mutual_max = 0;

	ts->stm_ts_systemreset(ts, 0);
	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);

	/* lock active scan mode */
	stm_ts_fix_active_mode(ts, true);

	// Mutual jitter.
	regAdd[0] = 0xC7;
	regAdd[1] = 0x08;
	regAdd[2] = 0x64;	//100 frame
	regAdd[3] = 0x00;

	ret = stm_ts_fw_wait_for_jitter_result(ts, regAdd, 4, &mutual_min, &mutual_max);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to read Mutual jitter\n", __func__);
		goto ERROR;
	}

	ts->stm_ts_systemreset(ts, 0);
	stm_ts_set_scanmode(ts, ts->scan_mode);

	sec_input_irq_enable(ts->plat_data);

	snprintf(buff, sizeof(buff), "%d", mutual_max);
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MUTUAL_JITTER");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_raw_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;

ERROR:
	ts->stm_ts_systemreset(ts, 0);
	stm_ts_set_scanmode(ts, ts->scan_mode);

	sec_input_irq_enable(ts->plat_data);

	snprintf(buff, sizeof(buff), "NG");
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MUTUAL_JITTER");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_raw_info(true, &ts->client->dev, "%s: Fail %s\n", __func__, buff);

}

int stm_ts_get_hf_data(struct stm_ts_data *ts, short *frame)
{
	u8 regAdd[4] = { 0 };
	int ret;
	short min = 0x7FFF;
	short max = 0x8000;

	ts->stm_ts_systemreset(ts, 0);

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);

	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	regAdd[0] = 0xA4;
	regAdd[1] = 0x04;
	regAdd[2] = 0xFF;
	regAdd[3] = 0x01;
	ret = stm_ts_wait_for_echo_event(ts, &regAdd[0], 4, 100);
	if (ret < 0)
		goto out;

	ret = stm_ts_read_frame(ts, TYPE_RAW_DATA, frame, &min, &max);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to get rawdata\n", __func__);

out:
	ts->stm_ts_systemreset(ts, 0);

	stm_ts_set_cover_type(ts, ts->flip_enable);

	sec_delay(10);

	if (ts->charger_mode) {
		stm_ts_charger_mode(ts);
		sec_delay(10);
	}

	stm_ts_set_scanmode(ts, ts->scan_mode);

	return ret;
}

int stm_ts_get_miscal_data(struct stm_ts_data *ts, short *frame, short *min, short *max)
{
	u8 cmd = STM_TS_READ_ONE_EVENT;
	u8 regAdd[2] = { 0 };
	u8 data[STM_TS_EVENT_SIZE];
	int retry = 200, ret;
	int result = STM_TS_EVENT_ERROR_REPORT;
	short min_print = 0x7FFF;
	short max_print = 0x8000;

	ts->stm_ts_systemreset(ts, 0);

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true);

	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);

	mutex_lock(&ts->wait_for);

	regAdd[0] = 0xC7;
	regAdd[1] = 0x02;

	ret = ts->stm_ts_write(ts, &regAdd[0], 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"%s: failed to write miscal cmd\n", __func__);
		result = -EIO;
		mutex_unlock(&ts->wait_for);
		goto error;
	}

	memset(data, 0x0, STM_TS_EVENT_SIZE);

	while (retry-- >= 0) {
		memset(data, 0x00, sizeof(data));
		ret = ts->stm_ts_read(ts, &cmd, 1, data, STM_TS_EVENT_SIZE);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: read failed: %d\n", __func__, ret);
			result = -EIO;
			mutex_unlock(&ts->wait_for);
			goto error;
		}

		if (data[0] != 0x00)
			input_info(true, &ts->client->dev, "%s: %02X %02X %02X %02X %02X %02X %02X %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);

		sec_delay(10);

		if (data[0] == STM_TS_EVENT_PASS_REPORT || data[0] == STM_TS_EVENT_ERROR_REPORT) {
			result = data[0];
			*max = data[3] << 8 | data[2];
			*min = data[5] << 8 | data[4];
			break;
		}

		if (retry == 0) {
			result = -EIO;
			mutex_unlock(&ts->wait_for);
			goto error;
		}
	}
	mutex_unlock(&ts->wait_for);

	input_raw_info(true, &ts->client->dev, "%s: %s %d,%d\n",
			__func__, data[0] == STM_TS_EVENT_PASS_REPORT ? "pass" : "fail", *min, *max);

	ret = stm_ts_read_frame(ts, TYPE_RAW_DATA, frame, &min_print, &max_print);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to get rawdata\n", __func__);

error:
	ts->plat_data->init(ts);

	return result;
}

static void run_factory_miscalibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN];
	int ret;
	short min = 0x7FFF, max = 0x8000;
	short *frame;

	frame = kzalloc(ts->plat_data->x_node_num  * ts->plat_data->y_node_num * 2 + 1, GFP_KERNEL);
	if (!frame) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = stm_ts_get_miscal_data(ts, frame, &min, &max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to get miscal data\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		kfree(frame);
		return;
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", min, max);
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "MIS_CAL");
	} else {
		if (ret == STM_TS_EVENT_PASS_REPORT)
			snprintf(buff, sizeof(buff), "OK,%d,%d", min, max);
		else
			snprintf(buff, sizeof(buff), "NG,%d,%d", min, max);
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(frame);
}

static void run_factory_miscalibration_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN];
	int i, j, ret;
	short min = 0x7FFF, max = 0x8000;
	char *all_strbuff;
	short *frame;

	frame = kzalloc(ts->plat_data->x_node_num  * ts->plat_data->y_node_num * 2 + 1, GFP_KERNEL);
	if (!frame) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	all_strbuff = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 7 + 1, GFP_KERNEL);
	if (!all_strbuff)
		goto NG;

	input_raw_info(true, &ts->client->dev, "%s\n", __func__);

	ret = stm_ts_get_miscal_data(ts, frame, &min, &max);
	if (ret < 0)
		goto NG;

	for (j = 0; j < ts->plat_data->y_node_num; j++) {
		for (i = 0; i < ts->plat_data->x_node_num ; i++) {
			snprintf(buff, sizeof(buff), "%d,", frame[j * ts->plat_data->x_node_num  + i]);
			strlcat(all_strbuff, buff, ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 7);
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, all_strbuff, strlen(all_strbuff));
	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, strlen(all_strbuff));
	kfree(frame);
	kfree(all_strbuff);
	return;
NG:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	kfree(frame);
	kfree(all_strbuff);
}

static void run_miscalibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN];
	char data[STM_TS_EVENT_SIZE];
	char echo;
	int ret;
	int retry = 200;
	short min = SHRT_MIN, max = SHRT_MAX;

	sec_input_irq_disable(ts->plat_data);

	mutex_lock(&ts->wait_for);

	memset(data, 0x00, sizeof(data));
	memset(buff, 0x00, sizeof(buff));

	data[0] = 0xA4;
	data[1] = 0x0B;
	data[2] = 0x00;
	data[3] = 0xC0;

	ret = ts->stm_ts_write(ts, data, 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: write failed: %d\n", __func__, ret);
		goto error;
	}

	/* maximum timeout 2sec ? */
	while (retry-- >= 0) {
		memset(data, 0x00, sizeof(data));
		echo = STM_TS_READ_ONE_EVENT;
		ret = ts->stm_ts_read(ts, &echo, 1, data, STM_TS_EVENT_SIZE);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: read failed: %d\n", __func__, ret);
			goto error;
		}

		if (data[0] != 0)
			input_info(true, &ts->client->dev, "%s: %02X %02X %02X %02X %02X %02X %02X %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);
		sec_delay(10);

		if (data[0] == STM_TS_EVENT_PASS_REPORT || data[0] == STM_TS_EVENT_ERROR_REPORT) {
			max = data[3] << 8 | data[2];
			min = data[5] << 8 | data[4];

			break;
		}
		if (retry == 0)
			goto error;
	}

	mutex_unlock(&ts->wait_for);

	if (data[0] == STM_TS_EVENT_PASS_REPORT)
		snprintf(buff, sizeof(buff), "OK,%d,%d", min, max);
	else
		snprintf(buff, sizeof(buff), "NG,%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	ts->plat_data->init(ts);
	sec_input_irq_enable(ts->plat_data);
	return;
error:
	mutex_unlock(&ts->wait_for);
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	ts->plat_data->init(ts);

	sec_input_irq_enable(ts->plat_data);
}

static void run_reference_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
	int ret;
	short *frame;

	frame = kzalloc(ts->plat_data->x_node_num  * ts->plat_data->y_node_num * 2 + 1, GFP_KERNEL);
	if (!frame) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = stm_ts_read_frame(ts, TYPE_BASELINE_DATA, frame, &min, &max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read frame failed, %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		kfree(frame);
		return;
	}

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(frame);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

void run_rawcap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
	short rawcap[2] = {SHRT_MAX, SHRT_MIN}; /* min, max */
	short rawcap_edge[2] = {SHRT_MAX, SHRT_MIN}; /* min, max */
	int ret, i, j;
	short *frame;

	frame = kzalloc(ts->plat_data->x_node_num  * ts->plat_data->y_node_num * 2 + 1, GFP_KERNEL);
	if (!frame) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = stm_ts_read_frame(ts, TYPE_RAW_DATA, frame, &min, &max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read frame failed, %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
			sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "RAW_DATA");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		kfree(frame);
		return;
	}

	for (i = 0; i < ts->plat_data->y_node_num; i++) {
		for (j = 0; j < ts->plat_data->x_node_num; j++) {
			short *rawcap_ptr;

			if (i == 0 || i == ts->plat_data->y_node_num - 1 || j == 0 || j == ts->plat_data->x_node_num - 1)
				rawcap_ptr = rawcap_edge;
			else
				rawcap_ptr = rawcap;

			if (frame[(i * ts->plat_data->x_node_num) + j] < rawcap_ptr[0])
				rawcap_ptr[0] = frame[(i * ts->plat_data->x_node_num) + j];
			if (frame[(i * ts->plat_data->x_node_num) + j] > rawcap_ptr[1])
				rawcap_ptr[1] = frame[(i * ts->plat_data->x_node_num) + j];
		}
	}

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "RAW_DATA");
		snprintf(buff, SEC_CMD_STR_LEN, "%d,%d", rawcap[0], rawcap[1]);
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "TSP_RAWCAP");
		snprintf(buff, SEC_CMD_STR_LEN, "%d,%d", rawcap_edge[0], rawcap_edge[1]);
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "TSP_RAWCAP_EDGE");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(frame);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_rawcap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
	char *all_strbuff;
	int i, j, ret;
	short *frame;

	frame = kzalloc(ts->plat_data->x_node_num  * ts->plat_data->y_node_num * 2 + 1, GFP_KERNEL);
	if (!frame) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	all_strbuff = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 7 + 1, GFP_KERNEL);
	if (!all_strbuff)
		goto NG;

	input_raw_info(true, &ts->client->dev, "%s\n", __func__);

	enter_factory_mode(ts, true);
	ret = stm_ts_read_frame(ts, TYPE_RAW_DATA, frame, &min, &max);
	enter_factory_mode(ts, false);
	ts->plat_data->init(ts);
	if (ret < 0)
		goto NG;

	for (j = 0; j < ts->plat_data->y_node_num; j++) {
		for (i = 0; i < ts->plat_data->x_node_num ; i++) {
			snprintf(buff, sizeof(buff), "%d,", frame[j * ts->plat_data->x_node_num  + i]);
			strlcat(all_strbuff, buff, ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 7);
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, all_strbuff, strlen(all_strbuff));
	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, strlen(all_strbuff));
	kfree(frame);
	kfree(all_strbuff);
	return;
NG:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	kfree(frame);
	kfree(all_strbuff);
}

void run_delta_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
	int ret;
	short *frame;

	frame = kzalloc(ts->plat_data->x_node_num  * ts->plat_data->y_node_num * 2 + 1, GFP_KERNEL);
	if (!frame) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = stm_ts_read_frame(ts, TYPE_STRENGTH_DATA, frame, &min, &max);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read frame failed, %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		kfree(frame);
		return;
	}

	snprintf(buff, sizeof(buff), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(frame);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

#ifdef TCLM_CONCEPT
static void get_pat_information(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[50] = { 0 };

#ifdef CONFIG_SEC_FACTORY
	if (ts->factory_position == OFFSET_FAC_SUB) {
		sec_tclm_initialize(ts->tdata);
		stm_tclm_data_read(ts->tdata->dev, SEC_TCLM_NVM_ALL_DATA);
	}
#endif
	/* fixed tune version will be saved at excute autotune */
	snprintf(buff, sizeof(buff), "C%02XT%04X.%4s%s%c%d%c%d%c%d",
		ts->tdata->nvdata.cal_count, ts->tdata->nvdata.tune_fix_ver,
		ts->tdata->tclm_string[ts->tdata->nvdata.cal_position].f_name,
		(ts->tdata->tclm_level == TCLM_LEVEL_LOCKDOWN) ? ".L " : " ",
		ts->tdata->cal_pos_hist_last3[0], ts->tdata->cal_pos_hist_last3[1],
		ts->tdata->cal_pos_hist_last3[2], ts->tdata->cal_pos_hist_last3[3],
		ts->tdata->cal_pos_hist_last3[4], ts->tdata->cal_pos_hist_last3[5]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_external_factory(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	ts->tdata->external_factory = true;
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

int stm_tclm_data_read(struct device *dev, int address)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int ret = 0;
	int i = 0;
	u8 nbuff[STM_TS_NVM_OFFSET_ALL];
	u16 ic_version;

	switch (address) {
	case SEC_TCLM_NVM_OFFSET_IC_FIRMWARE_VER:
		ret = ts->stm_ts_get_version_info(ts);
		if (ret < 0)
			return ret;
		ic_version = (ts->plat_data->img_version_of_ic[SEC_INPUT_FW_MODULE_VER] << 8) |
				(ts->plat_data->img_version_of_ic[SEC_INPUT_FW_VER] & 0xFF);
		return ic_version;
	case SEC_TCLM_NVM_ALL_DATA:
		ret = get_nvm_data_by_size(ts, nvm_data[STM_TS_NVM_OFFSET_FAC_RESULT].offset,
				STM_TS_NVM_OFFSET_ALL, nbuff);
		if (ret < 0)
			return ret;
		ts->tdata->nvdata.cal_count = nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_COUNT].offset];
		ts->tdata->nvdata.tune_fix_ver = (nbuff[nvm_data[STM_TS_NVM_OFFSET_TUNE_VERSION].offset] << 8) |
							nbuff[nvm_data[STM_TS_NVM_OFFSET_TUNE_VERSION].offset + 1];
		ts->tdata->nvdata.cal_position = nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_POSITION].offset];
		ts->tdata->nvdata.cal_pos_hist_cnt = nbuff[nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT].offset];
		ts->tdata->nvdata.cal_pos_hist_lastp = nbuff[nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP].offset];
		for (i = nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset;
				i < nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset +
				nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].length; i++)
			ts->tdata->nvdata.cal_pos_hist_queue[i - nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset] = nbuff[i];

		ts->tdata->nvdata.cal_fail_flag = nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_FAIL_FLAG].offset];
		ts->tdata->nvdata.cal_fail_cnt = nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_FAIL_COUNT].offset];
		ts->fac_nv = nbuff[nvm_data[STM_TS_NVM_OFFSET_FAC_RESULT].offset];
		ts->disassemble_count = nbuff[nvm_data[STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT].offset];
		return ret;
	case SEC_TCLM_NVM_TEST:
		input_info(true, &ts->client->dev, "%s: dt: tclm_level [%d] afe_base [%04X]\n",
			__func__, ts->tdata->tclm_level, ts->tdata->afe_base);
		ret = get_nvm_data_by_size(ts, STM_TS_NVM_OFFSET_ALL + SEC_TCLM_NVM_OFFSET,
			SEC_TCLM_NVM_OFFSET_LENGTH, ts->tdata->tclm);
		if (ts->tdata->tclm[0] != 0xFF) {
			ts->tdata->tclm_level = ts->tdata->tclm[0];
			ts->tdata->afe_base = (ts->tdata->tclm[1] << 8) | ts->tdata->tclm[2];
			input_info(true, &ts->client->dev, "%s: nv: tclm_level [%d] afe_base [%04X]\n",
				__func__, ts->tdata->tclm_level, ts->tdata->afe_base);
		}
		return ret;
	default:
		return ret;
	}
}

int stm_tclm_data_write(struct device *dev, int address)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	int ret = 1;
	int i = 0;
	u8 nbuff[STM_TS_NVM_OFFSET_ALL];

	switch (address) {
	case SEC_TCLM_NVM_ALL_DATA:
		memset(nbuff, 0x00, STM_TS_NVM_OFFSET_ALL);
		nbuff[nvm_data[STM_TS_NVM_OFFSET_FAC_RESULT].offset] = ts->fac_nv;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT].offset] = ts->disassemble_count;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_COUNT].offset] = ts->tdata->nvdata.cal_count;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_TUNE_VERSION].offset] = (u8)(ts->tdata->nvdata.tune_fix_ver >> 8);
		nbuff[nvm_data[STM_TS_NVM_OFFSET_TUNE_VERSION].offset + 1] = (u8)(0xff & ts->tdata->nvdata.tune_fix_ver);
		nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_POSITION].offset] = ts->tdata->nvdata.cal_position;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_COUNT].offset] = ts->tdata->nvdata.cal_pos_hist_cnt;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_LASTP].offset] = ts->tdata->nvdata.cal_pos_hist_lastp;
		for (i = nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset;
				i < nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset +
				nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].length; i++)
			nbuff[i] = ts->tdata->nvdata.cal_pos_hist_queue[i - nvm_data[STM_TS_NVM_OFFSET_HISTORY_QUEUE_ZERO].offset];
		nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_FAIL_FLAG].offset] = ts->tdata->nvdata.cal_fail_flag;
		nbuff[nvm_data[STM_TS_NVM_OFFSET_CAL_FAIL_COUNT].offset] = ts->tdata->nvdata.cal_fail_cnt;
		ret = set_nvm_data_by_size(ts, nvm_data[STM_TS_NVM_OFFSET_FAC_RESULT].offset, STM_TS_NVM_OFFSET_ALL, nbuff);
		return ret;
	case SEC_TCLM_NVM_TEST:
		ret = set_nvm_data_by_size(ts, STM_TS_NVM_OFFSET_ALL + SEC_TCLM_NVM_OFFSET,
			SEC_TCLM_NVM_OFFSET_LENGTH, ts->tdata->tclm);
		return ret;
	default:
		return ret;
	}
}

static void tclm_test_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	struct sec_tclm_data *data = ts->tdata;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	if (!ts->tdata->support_tclm_test)
		goto not_support;

	ret = tclm_test_command(data, sec->cmd_param[0], sec->cmd_param[1], sec->cmd_param[2], buff);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;

not_support:
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	if (!ts->tdata->support_tclm_test)
		goto not_support;

	snprintf(buff, sizeof(buff), "%d", ts->is_cal_done);

	ts->is_cal_done = false;
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	return;

not_support:
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}
#endif

static void stm_ts_read_ix_data(struct stm_ts_data *ts, bool allnode)
{
	struct sec_cmd_data *sec = &ts->sec;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	int rc;

	u16 max_tx_ix_sum = 0;
	u16 min_tx_ix_sum = 0xFFFF;

	u16 max_rx_ix_sum = 0;
	u16 min_rx_ix_sum = 0xFFFF;

	u8 *data;

	u8 regAdd[STM_TS_EVENT_SIZE];

	u8 dataID;

	u16 *force_ix_data;
	u16 *sense_ix_data;

	int buff_size, j;
	char *mbuff = NULL;
	int num, n, a, fzero;
	char cnum;
	int i = 0;
	u16 comp_start_tx_addr, comp_start_rx_addr;
	unsigned int rx_num, tx_num;

	sec_cmd_set_default_result(sec);

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out_alloc_failed;
	}

	data = kzalloc((ts->plat_data->y_node_num + ts->plat_data->x_node_num) * 2 + 1, GFP_KERNEL);
	if (!data) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out_alloc_failed;
	}

	force_ix_data = kzalloc(ts->plat_data->y_node_num * 2, GFP_KERNEL);
	if (!force_ix_data) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		kfree(data);
		goto out_alloc_failed;
	}

	sense_ix_data = kzalloc(ts->plat_data->x_node_num  * 2, GFP_KERNEL);
	if (!sense_ix_data) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		kfree(force_ix_data);
		kfree(data);
		goto out_alloc_failed;
	}

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true); // Clear FIFO

	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);

	// Request compensation data type
	dataID = 0x52;
	regAdd[0] = 0xA4;
	regAdd[1] = 0x06;
	regAdd[2] = dataID; // SS - CX total
	rc = stm_ts_wait_for_echo_event(ts, &regAdd[0], 3, 0);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: failed to request data\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_input_irq_enable(ts->plat_data);
		goto out;
	}
	sec_input_irq_enable(ts->plat_data);

	// Read Header
	regAdd[0] = 0xA6;
	regAdd[1] = 0x00;
	regAdd[2] = 0x00;
	rc = ts->stm_ts_read(ts, &regAdd[0], 3, &data[0], STM_TS_COMP_DATA_HEADER_SIZE);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read header\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if ((data[0] != 0xA5) && (data[1] != dataID)) {
		input_err(true, &ts->client->dev, "%s: header mismatch\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	tx_num = data[4];
	rx_num = data[5];

	/* Read TX IX data */
	comp_start_tx_addr = (u16)STM_TS_COMP_DATA_HEADER_SIZE;
	regAdd[0] = 0xA6;
	regAdd[1] = (u8)(comp_start_tx_addr >> 8);
	regAdd[2] = (u8)(comp_start_tx_addr & 0xFF);
	rc = ts->stm_ts_read(ts, &regAdd[0], 3, &data[0], tx_num * 2);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read TX IX data\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	for (i = 0; i < tx_num; i++) {
		force_ix_data[i] = data[2 * i] | data[2 * i + 1] << 8;

		if (max_tx_ix_sum < force_ix_data[i])
			max_tx_ix_sum = force_ix_data[i];
		if (min_tx_ix_sum > force_ix_data[i])
			min_tx_ix_sum = force_ix_data[i];

	}

	/* Read RX IX data */
	comp_start_rx_addr = (u16)(STM_TS_COMP_DATA_HEADER_SIZE + (tx_num * 2));
	regAdd[0] = 0xA6;
	regAdd[1] = (u8)(comp_start_rx_addr >> 8);
	regAdd[2] = (u8)(comp_start_rx_addr & 0xFF);
	ts->stm_ts_read(ts, &regAdd[0], 3, &data[0], rx_num * 2);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read RX IX\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	for (i = 0; i < rx_num; i++) {
		sense_ix_data[i] = data[2 * i] | data[2 * i + 1] << 8;

		if (max_rx_ix_sum < sense_ix_data[i])
			max_rx_ix_sum = sense_ix_data[i];
		if (min_rx_ix_sum > sense_ix_data[i])
			min_rx_ix_sum = sense_ix_data[i];
	}

	stm_ts_print_channel(ts, force_ix_data, sense_ix_data);

	input_raw_info(true, &ts->client->dev, "%s: MIN_TX_IX_SUM : %d MAX_TX_IX_SUM : %d\n",
			__func__, min_tx_ix_sum, max_tx_ix_sum);
	input_raw_info(true, &ts->client->dev, "%s: MIN_RX_IX_SUM : %d MAX_RX_IX_SUM : %d\n",
			__func__, min_rx_ix_sum, max_rx_ix_sum);

	if (allnode == true) {
		buff_size = (ts->plat_data->y_node_num + ts->plat_data->x_node_num  + 2) * 5;
		mbuff = kzalloc(buff_size, GFP_KERNEL);
	}
	if (mbuff != NULL) {
		char *pBuf = mbuff;

		for (i = 0; i < ts->plat_data->y_node_num; i++) {
			num =  force_ix_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--) {
				n = n / 10;
				a = num / n;
				if (a)
					fzero = 1;
				cnum = a + '0';
				num  = num - a*n;
				if (fzero)
					*pBuf++ = cnum;
			}
			if (!fzero)
				*pBuf++ = '0';
			*pBuf++ = ',';
		}
		for (i = 0; i < ts->plat_data->x_node_num ; i++) {
			num =  sense_ix_data[i];
			n = 100000;
			fzero = 0;
			for (j = 5; j > 0; j--) {
				n = n / 10;
				a = num / n;
				if (a)
					fzero = 1;
				cnum = a + '0';
				num  = num - a * n;
				if (fzero)
					*pBuf++ = cnum;
			}
			if (!fzero)
				*pBuf++ = '0';
			if (i < (ts->plat_data->x_node_num  - 1))
				*pBuf++ = ',';
		}

		sec_cmd_set_cmd_result(sec, mbuff, buff_size);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		kfree(mbuff);
		kfree(sense_ix_data);
		kfree(force_ix_data);
		kfree(data);
		return;
	}

	if (allnode == true) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%d,%d,%d,%d",
				min_tx_ix_sum, max_tx_ix_sum, min_rx_ix_sum, max_rx_ix_sum);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

out:
	kfree(sense_ix_data);
	kfree(force_ix_data);
	kfree(data);

out_alloc_failed:
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		char ret_buff[SEC_CMD_STR_LEN] = { 0 };

		snprintf(ret_buff, sizeof(ret_buff), "%d,%d", min_rx_ix_sum, max_rx_ix_sum);
		sec_cmd_set_cmd_result_all(sec, ret_buff, strnlen(ret_buff, sizeof(ret_buff)), "IX_DATA_X");
		snprintf(ret_buff, sizeof(ret_buff), "%d,%d", min_tx_ix_sum, max_tx_ix_sum);
		sec_cmd_set_cmd_result_all(sec, ret_buff, strnlen(ret_buff, sizeof(ret_buff)), "IX_DATA_Y");
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_ix_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	input_raw_info(true, &ts->client->dev, "%s\n", __func__);
	stm_ts_read_ix_data(ts, false);
}

static void run_ix_data_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	enter_factory_mode(ts, true);
	stm_ts_read_ix_data(ts, true);
	enter_factory_mode(ts, false);
}

static void stm_ts_read_self_raw_frame(struct stm_ts_data *ts, bool allnode)
{
	struct sec_cmd_data *sec = &ts->sec;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct stm_ts_syncframeheader *pSyncFrameHeader;

	u8 regAdd[STM_TS_EVENT_SIZE] = {0};

	u8 *data;

	s16 *self_force_raw_data;
	s16 *self_sense_raw_data;

	int Offset = 0;
	u8 count = 0;
	int i;
	int ret;
	int totalbytes;
	int retry = 10;

	s16 min_tx_self_raw_data = S16_MAX;
	s16 max_tx_self_raw_data = S16_MIN;
	s16 min_rx_self_raw_data = S16_MAX;
	s16 max_rx_self_raw_data = S16_MIN;

	data = kzalloc((ts->plat_data->y_node_num + ts->plat_data->x_node_num) * 2 + 1, GFP_KERNEL);
	if (!data) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out_alloc_failed;
	}

	self_force_raw_data = kzalloc(ts->plat_data->y_node_num * 2, GFP_KERNEL);
	if (!self_force_raw_data) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		kfree(data);
		goto out_alloc_failed;
	}

	self_sense_raw_data = kzalloc(ts->plat_data->x_node_num  * 2, GFP_KERNEL);
	if (!self_sense_raw_data) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		kfree(data);
		kfree(self_force_raw_data);
		goto out_alloc_failed;
	}

	// Request Data Type
	regAdd[0] = 0xA4;
	regAdd[1] = 0x06;
	regAdd[2] = TYPE_RAW_DATA;
	ret = ts->stm_ts_write(ts, &regAdd[0], 3);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to request data\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	do {
		regAdd[0] = 0xA6;
		regAdd[1] = 0x00;
		regAdd[2] = 0x00;
		ret = ts->stm_ts_read(ts, &regAdd[0], 3, &data[0], STM_TS_COMP_DATA_HEADER_SIZE);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: read failed rc = %d\n", __func__, ret);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}

		pSyncFrameHeader = (struct stm_ts_syncframeheader *) &data[0];

		if ((pSyncFrameHeader->header == 0xA5) && (pSyncFrameHeader->host_data_mem_id == TYPE_RAW_DATA))
			break;

		sec_delay(100);
	} while (retry--);

	if (retry == 0) {
		input_err(true, &ts->client->dev, "%s: didn't match header or id. header = %02X, id = %02X\n",
				__func__, pSyncFrameHeader->header, pSyncFrameHeader->host_data_mem_id);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	Offset = STM_TS_COMP_DATA_HEADER_SIZE + pSyncFrameHeader->dbg_frm_len +
			(pSyncFrameHeader->ms_force_len * pSyncFrameHeader->ms_sense_len * 2);

	totalbytes = (pSyncFrameHeader->ss_force_len + pSyncFrameHeader->ss_sense_len) * 2;

	if (totalbytes > (ts->plat_data->y_node_num + ts->plat_data->x_node_num) * 2 + 1) {
		input_err(true, &ts->client->dev, "%s: totalbytes %d is over than allocated size\n", __func__, totalbytes);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	regAdd[0] = 0xA6;
	regAdd[1] = (u8)(Offset >> 8);
	regAdd[2] = (u8)(Offset & 0xFF);
	ret = ts->stm_ts_read(ts, &regAdd[0], 3, &data[0], totalbytes);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: read failed rc = %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	Offset = 0;
	for (count = 0; count < (ts->plat_data->y_node_num); count++) {
		self_force_raw_data[count] = (s16)(data[count * 2 + Offset] + (data[count * 2 + 1 + Offset] << 8));

		if (count == 0) /* only for TABS7+ : TX0 self raw value is too big */
			continue;

		if (max_tx_self_raw_data < self_force_raw_data[count])
			max_tx_self_raw_data = self_force_raw_data[count];
		if (min_tx_self_raw_data > self_force_raw_data[count])
			min_tx_self_raw_data = self_force_raw_data[count];
	}

	Offset = (ts->plat_data->y_node_num * 2);
	for (count = 0; count < ts->plat_data->x_node_num ; count++) {
		self_sense_raw_data[count] = (s16)(data[count * 2 + Offset] + (data[count * 2 + 1 + Offset] << 8));

		if (max_rx_self_raw_data < self_sense_raw_data[count])
			max_rx_self_raw_data = self_sense_raw_data[count];
		if (min_rx_self_raw_data > self_sense_raw_data[count])
			min_rx_self_raw_data = self_sense_raw_data[count];
	}

	stm_ts_print_channel(ts, self_force_raw_data, self_sense_raw_data);

	input_raw_info(true, &ts->client->dev, "%s: MIN_TX_SELF_RAW: %d MAX_TX_SELF_RAW : %d\n",
			__func__, (s16)min_tx_self_raw_data, (s16)max_tx_self_raw_data);
	input_raw_info(true, &ts->client->dev, "%s: MIN_RX_SELF_RAW : %d MIN_RX_SELF_RAW : %d\n",
			__func__, (s16)min_rx_self_raw_data, (s16)max_rx_self_raw_data);

	if (allnode == true) {
		char *mbuff;
		int buffsize = (ts->plat_data->y_node_num + ts->plat_data->x_node_num  + 2) * 10;
		char temp[10] = { 0 };

		mbuff = kzalloc(buffsize, GFP_KERNEL);
		if (!mbuff) {
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}

		for (i = 0; i < (ts->plat_data->y_node_num); i++) {
			snprintf(temp, sizeof(temp), "%d,", (s16)self_force_raw_data[i]);
			strlcat(mbuff, temp, buffsize);
		}
		for (i = 0; i < (ts->plat_data->x_node_num); i++) {
			snprintf(temp, sizeof(temp), "%d,", (s16)self_sense_raw_data[i]);
			strlcat(mbuff, temp, buffsize);
		}

		sec_cmd_set_cmd_result(sec, mbuff, buffsize);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		kfree(mbuff);
		kfree(data);
		kfree(self_force_raw_data);
		kfree(self_sense_raw_data);
		return;
	}

	snprintf(buff, sizeof(buff), "%d,%d,%d,%d",
			(s16)min_tx_self_raw_data, (s16)max_tx_self_raw_data,
			(s16)min_rx_self_raw_data, (s16)max_rx_self_raw_data);
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	kfree(data);
	kfree(self_force_raw_data);
	kfree(self_sense_raw_data);

out_alloc_failed:
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		char ret_buff[SEC_CMD_STR_LEN] = { 0 };

		snprintf(ret_buff, sizeof(ret_buff), "%d,%d", (s16)min_rx_self_raw_data, (s16)max_rx_self_raw_data);
		sec_cmd_set_cmd_result_all(sec, ret_buff, strnlen(ret_buff, sizeof(ret_buff)), "SELF_RAW_DATA_X");
		snprintf(ret_buff, sizeof(ret_buff), "%d,%d", (s16)min_tx_self_raw_data, (s16)max_tx_self_raw_data);
		sec_cmd_set_cmd_result_all(sec, ret_buff, strnlen(ret_buff, sizeof(ret_buff)), "SELF_RAW_DATA_Y");
	}
	sec_cmd_set_cmd_result(sec, &buff[0], strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_self_raw_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	input_raw_info(true, &ts->client->dev, "%s\n", __func__);
	stm_ts_read_self_raw_frame(ts, false);
}

static void run_self_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	enter_factory_mode(ts, true);
	stm_ts_read_self_raw_frame(ts, true);
	enter_factory_mode(ts, false);
}

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char test[32];
	int ret = 0;

	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	ret = stm_ts_panel_ito_test(ts, OPEN_SHORT_CRACK_TEST);
	if (ret == 0) {
		snprintf(buff, sizeof(buff), "OK");
		sec_cmd_send_event_to_user(sec, test, "RESULT=PASS");
	} else {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_send_event_to_user(sec, test, "RESULT=FAIL");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret = 0;

	ret = stm_ts_panel_ito_test(ts, OPEN_TEST);
	if (ret == 0)
		sec->cmd_state = SEC_CMD_STATUS_OK;
	else
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

static int read_ms_cx_data(struct stm_ts_data *ts, u8 *cx_min, u8 *cx_max)
{
	u8 *rdata;
	u8 regAdd[STM_TS_EVENT_SIZE] = { 0 };
	u8 dataID;
	u16 comp_start_addr;
	int txnum, rxnum, i, j, ret = 0;
	u8 *pStr = NULL;
	u8 pTmp[16] = { 0 };

	pStr = kzalloc(7 * (ts->plat_data->x_node_num  + 1), GFP_KERNEL);
	if (pStr == NULL)
		return -ENOMEM;

	rdata = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num, GFP_KERNEL);
	if (!rdata) {
		kfree(pStr);
		return -ENOMEM;
	}

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true); // Clear FIFO
	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);

	sec_delay(20);

	// Request compensation data type
	dataID = 0x11;  // MS - LP
	regAdd[0] = 0xA4;
	regAdd[1] = 0x06;
	regAdd[2] = dataID;
	ret = stm_ts_wait_for_echo_event(ts, &regAdd[0], 3, 0);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to request data\n", __func__);
		sec_input_irq_enable(ts->plat_data);
		goto out;
	}

	// Read Header
	regAdd[0] = 0xA6;
	regAdd[1] = 0x00;
	regAdd[2] = 0x00;
	ret = ts->stm_ts_read(ts, &regAdd[0], 3, &rdata[0], STM_TS_COMP_DATA_HEADER_SIZE);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to read header\n", __func__);
		sec_input_irq_enable(ts->plat_data);
		goto out;
	}

	sec_input_irq_enable(ts->plat_data);

	if ((rdata[0] != 0xA5) && (rdata[1] != dataID)) {
		input_info(true, &ts->client->dev, "%s: failed to read signature data of header.\n", __func__);
		ret = -EIO;
		goto out;
	}

	txnum = rdata[4];
	rxnum = rdata[5];

	comp_start_addr = (u16)STM_TS_COMP_DATA_HEADER_SIZE;
	regAdd[0] = 0xA6;
	regAdd[1] = (u8)(comp_start_addr >> 8);
	regAdd[2] = (u8)(comp_start_addr & 0xFF);
	ret = ts->stm_ts_read(ts, &regAdd[0], 3, &rdata[0], txnum * rxnum);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to read data\n", __func__);
		goto out;
	}

	*cx_min = *cx_max = rdata[0];
	for (j = 0; j < ts->plat_data->y_node_num; j++) {
		memset(pStr, 0x0, 7 * (ts->plat_data->x_node_num  + 1));
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", j);
		strlcat(pStr, pTmp, 7 * (ts->plat_data->x_node_num  + 1));

		for (i = 0; i < ts->plat_data->x_node_num ; i++) {
			snprintf(pTmp, sizeof(pTmp), "%3d", rdata[j * ts->plat_data->x_node_num  + i]);
			strlcat(pStr, pTmp, 7 * (ts->plat_data->x_node_num  + 1));
			*cx_min = min(*cx_min, rdata[j * ts->plat_data->x_node_num  + i]);
			*cx_max = max(*cx_max, rdata[j * ts->plat_data->x_node_num  + i]);
		}
		input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	}
	input_raw_info(true, &ts->client->dev, "cx min:%d, cx max:%d\n", *cx_min, *cx_max);

	if (ts->cx_data)
		memcpy(&ts->cx_data[0], &rdata[0], ts->plat_data->y_node_num * ts->plat_data->x_node_num);

out:
	kfree(rdata);
	kfree(pStr);
	return ret;
}

static void get_cx_gap_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int rx_max = 0, tx_max = 0, ii;

	for (ii = 0; ii < (ts->plat_data->x_node_num  * ts->plat_data->y_node_num); ii++) {
		/* rx(x) gap max */
		if ((ii + 1) % (ts->plat_data->x_node_num) != 0)
			rx_max = max(rx_max, (int)abs(ts->cx_data[ii + 1] - ts->cx_data[ii]));

		/* tx(y) gap max */
		if (ii < (ts->plat_data->y_node_num - 1) * ts->plat_data->x_node_num)
			tx_max = max(tx_max, (int)abs(ts->cx_data[ii + ts->plat_data->x_node_num] - ts->cx_data[ii]));
	}

	input_raw_info(true, &ts->client->dev, "%s: rx max:%d, tx max:%d\n", __func__, rx_max, tx_max);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", 0, rx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "CX_DATA_GAP_X");
		snprintf(buff, sizeof(buff), "%d,%d", 0, tx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "CX_DATA_GAP_Y");
	}
}

static int read_vpump_cap_data(struct stm_ts_data *ts, s8 *cap_min, s8 *cap_max)
{
	u8 *rdata;
	u8 regAdd[STM_TS_EVENT_SIZE] = { 0 };
	u8 dataID;
	u16 comp_start_addr;
	int txnum, rxnum, i, j, ret = 0;
	u8 *pStr = NULL;
	u8 pTmp[16] = { 0 };

	pStr = kzalloc(7 * (ts->plat_data->x_node_num  + 1), GFP_KERNEL);
	if (pStr == NULL)
		return -ENOMEM;

	rdata = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 2, GFP_KERNEL);
	if (!rdata) {
		kfree(pStr);
		return -ENOMEM;
	}

	ts->stm_ts_command(ts, STM_TS_CMD_CLEAR_ALL_EVENT, true); // Clear FIFO
	stm_ts_release_all_finger(ts);

	sec_input_irq_disable(ts->plat_data);

	// Request compensation data type
	dataID = 0x10;  // MS - Active
	regAdd[0] = 0xA4;
	regAdd[1] = 0x06;
	regAdd[2] = dataID;
	ret = stm_ts_wait_for_echo_event(ts, &regAdd[0], 3, 0);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to request data\n", __func__);
		sec_input_irq_enable(ts->plat_data);
		goto out;
	}

	sec_delay(50);

	// Read Header
	regAdd[0] = 0xA6;
	regAdd[1] = 0x00;
	regAdd[2] = 0x00;
	ret = ts->stm_ts_read(ts, &regAdd[0], 3, &rdata[0], STM_TS_COMP_DATA_HEADER_SIZE);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to read header\n", __func__);
		sec_input_irq_enable(ts->plat_data);
		goto out;
	}

	sec_input_irq_enable(ts->plat_data);

	if ((rdata[0] != 0xA5) && (rdata[1] != dataID)) {
		input_info(true, &ts->client->dev, "%s: failed to read signature data of header.\n", __func__);
		ret = -EIO;
		goto out;
	}

	txnum = rdata[4];
	rxnum = rdata[5];

	comp_start_addr = (u16)STM_TS_COMP_DATA_HEADER_SIZE;
	regAdd[0] = 0xA6;
	regAdd[1] = (u8)(comp_start_addr >> 8);
	regAdd[2] = (u8)(comp_start_addr & 0xFF);
	ret = ts->stm_ts_read(ts, &regAdd[0], 3, &rdata[0], txnum * rxnum);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: failed to read data\n", __func__);
		goto out;
	}

	*cap_min = *cap_max = rdata[0];
	for (j = 0; j < ts->plat_data->y_node_num; j++) {
		memset(pStr, 0x0, 7 * (ts->plat_data->x_node_num  + 1));
		snprintf(pTmp, sizeof(pTmp), "Tx%02d | ", j);
		strlcat(pStr, pTmp, 7 * (ts->plat_data->x_node_num  + 1));

		for (i = 0; i < ts->plat_data->x_node_num ; i++) {
			snprintf(pTmp, sizeof(pTmp), "%3d", (s8)rdata[j * ts->plat_data->x_node_num  + i]);
			strlcat(pStr, pTmp, 7 * (ts->plat_data->x_node_num  + 1));
			*cap_min = min(*cap_min, (s8)rdata[j * ts->plat_data->x_node_num  + i]);
			*cap_max = max(*cap_max, (s8)rdata[j * ts->plat_data->x_node_num  + i]);
		}
		input_raw_info(true, &ts->client->dev, "%s\n", pStr);
	}
	input_raw_info(true, &ts->client->dev, "vp cap min:%d, vp cap min max:%d\n", *cap_min, *cap_max);

	if (ts->vp_cap_data)
		memcpy(&ts->vp_cap_data[0], &rdata[0], ts->plat_data->y_node_num * ts->plat_data->x_node_num);

out:
	kfree(rdata);
	kfree(pStr);
	return ret;
}

static void run_vpump_cap_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_minmax[SEC_CMD_STR_LEN] = { 0 };
	int rc;
	s8 cap_min, cap_max;

	input_raw_info(true, &ts->client->dev, "%s: start\n", __func__);

	rc = read_vpump_cap_data(ts, &cap_min, &cap_max);
	if (rc < 0) {
		snprintf(buff, sizeof(buff), "NG");
		snprintf(buff_minmax, sizeof(buff_minmax), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff_minmax, sizeof(buff_minmax), "%d,%d", cap_min, cap_max);
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_vpump_cap_tx_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int tx_min, tx_max, i, j;

	tx_min = tx_max = (int)ts->vp_cap_data[0];
	for (i = 0; i < ts->plat_data->y_node_num - 1; i++) {
		for (j = 0; j < ts->plat_data->x_node_num ; j++) {
			tx_min = min(tx_min, (int)ts->vp_cap_data[i * ts->plat_data->x_node_num  + j]);
			tx_max = max(tx_max, (int)ts->vp_cap_data[i * ts->plat_data->x_node_num  + j]);
		}
	}

	input_raw_info(true, &ts->client->dev, "%s: tx min:%d tx max:%d\n", __func__, tx_min, tx_max);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", tx_min, tx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "VP_CAP_X");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void get_vpump_cap_sum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int tx_sum = 0, tx_max = 0, i, j;

	for (j = 1; j < ts->plat_data->x_node_num ; j += 2) {
		tx_sum = 0;
		for (i = 0; i < 6; i++)
			tx_sum += ts->vp_cap_data[j * ts->plat_data->x_node_num  + i];

		tx_max = max(tx_max, tx_sum);
	}

	input_raw_info(true, &ts->client->dev, "%s: tx sum max:%d\n", __func__, tx_max);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", 0, tx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "VP_CAP_SUM_X");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

void run_cx_data_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char buff_minmax[SEC_CMD_STR_LEN] = { 0 };
	int rc;
	u8 cx_min, cx_max;

	input_raw_info(true, &ts->client->dev, "%s: start\n", __func__);

	rc = read_ms_cx_data(ts, &cx_min, &cx_max);
	if (rc < 0) {
		snprintf(buff, sizeof(buff), "NG");
		snprintf(buff_minmax, sizeof(buff_minmax), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff_minmax, sizeof(buff_minmax), "%d,%d", cx_min, cx_max);
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff_minmax, strnlen(buff_minmax, sizeof(buff_minmax)), "CX_DATA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_cx_all_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int rc, i, j;
	u8 cx_min, cx_max;
	char *all_strbuff;

	input_raw_info(true, &ts->client->dev, "%s\n", __func__);

	input_info(true, &ts->client->dev, "%s: start\n", __func__);

	enter_factory_mode(ts, true);
	rc = read_ms_cx_data(ts, &cx_min, &cx_max);

	/* do not systemreset in COB type */
	if (ts->plat_data->chip_on_board)
		stm_ts_set_scanmode(ts, ts->scan_mode);
	else
		enter_factory_mode(ts, false);

	if (rc < 0)
		goto NG;

	all_strbuff = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 4 + 1, GFP_KERNEL);
	if (!all_strbuff) {
		input_err(true, &ts->client->dev, "%s: alloc failed\n", __func__);
		goto NG;
	}

	/* Read compensation data */
	if (ts->cx_data) {
		for (j = 0; j < ts->plat_data->y_node_num; j++) {
			for (i = 0; i < ts->plat_data->x_node_num ; i++) {
				snprintf(buff, sizeof(buff), "%d,", ts->cx_data[j * ts->plat_data->x_node_num  + i]);
				strlcat(all_strbuff, buff, ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 4 + 1);
			}
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, all_strbuff, strlen(all_strbuff));
	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, strlen(all_strbuff));
	kfree(all_strbuff);
	return;
NG:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

static void run_cx_gap_data_x_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char *buff = NULL;
	int ii;
	char temp[5] = { 0 };

	buff = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 5, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, sizeof(temp), "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (ii = 0; ii < (ts->plat_data->x_node_num  * ts->plat_data->y_node_num); ii++) {
		if ((ii + 1) % (ts->plat_data->x_node_num) != 0) {
			snprintf(temp, sizeof(temp), "%d,", (int)abs(ts->cx_data[ii + 1] - ts->cx_data[ii]));
			strlcat(buff, temp, ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 5);
			memset(temp, 0x00, 5);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 5));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_cx_gap_data_y_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char *buff = NULL;
	int ii;
	char temp[5] = { 0 };

	buff = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 5, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, sizeof(temp), "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (ii = 0; ii < (ts->plat_data->x_node_num  * ts->plat_data->y_node_num); ii++) {
		if (ii < (ts->plat_data->y_node_num - 1) * ts->plat_data->x_node_num) {
			snprintf(temp, sizeof(temp), "%d,",
					(int)abs(ts->cx_data[ii + ts->plat_data->x_node_num] - ts->cx_data[ii]));
			strlcat(buff, temp, ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 5);
			memset(temp, 0x00, 5);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 5));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void get_strength_all_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	short min = 0x7FFF;
	short max = 0x8000;
	char *all_strbuff;
	int i, j, ret;
	short *frame;

	frame = kzalloc(ts->plat_data->x_node_num  * ts->plat_data->y_node_num * 2 + 1, GFP_KERNEL);
	if (!frame) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	all_strbuff = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 7 + 1, GFP_KERNEL);
	if (!all_strbuff)
		goto NG;

	ret = stm_ts_read_frame(ts, TYPE_STRENGTH_DATA, frame, &min, &max);
	if (ret < 0)
		goto NG;

	for (i = 0; i < ts->plat_data->y_node_num; i++) {
		for (j = 0; j < ts->plat_data->x_node_num ; j++) {
			snprintf(buff, sizeof(buff), "%d,", frame[(i * ts->plat_data->x_node_num) + j]);
			strlcat(all_strbuff, buff, ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 7);
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, all_strbuff, strlen(all_strbuff));
	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, strlen(all_strbuff));
	kfree(frame);
	kfree(all_strbuff);
	return;
NG:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	kfree(frame);
	kfree(all_strbuff);
}

static void run_high_frequency_rawcap_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char *all_strbuff;
	int i, j, ret;
	short *frame;

	frame = kzalloc(ts->plat_data->x_node_num  * ts->plat_data->y_node_num * 2 + 1, GFP_KERNEL);
	if (!frame) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	all_strbuff = kzalloc(ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 7 + 1, GFP_KERNEL);
	if (!all_strbuff)
		goto NG;

	input_raw_info(true, &ts->client->dev, "%s\n", __func__);

	ret = stm_ts_get_hf_data(ts, frame);
	if (ret < 0)
		goto NG;

	for (j = 0; j < ts->plat_data->y_node_num; j++) {
		for (i = 0; i < ts->plat_data->x_node_num ; i++) {
			snprintf(buff, sizeof(buff), "%d,", frame[j * ts->plat_data->x_node_num  + i]);
			strlcat(all_strbuff, buff, ts->plat_data->y_node_num * ts->plat_data->x_node_num  * 7);
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, all_strbuff, strlen(all_strbuff));
	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, strlen(all_strbuff));
	kfree(frame);
	kfree(all_strbuff);
	return;
NG:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	kfree(frame);
	kfree(all_strbuff);
}

static void run_snr_non_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	u8 address[5] = { 0 };
	u16 status;
	int ret = 0;
	int wait_time = 0;
	int retry_cnt = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		input_err(true, &ts->client->dev, "%s: strange value frame:%d\n",
				__func__, sec->cmd_param[0]);
		goto out_init;
	}

	stm_ts_fix_active_mode(ts, true);

	/* enter SNR mode */
	address[0] = 0x70;
	address[1] = 0x25;
	ret = ts->stm_ts_write(ts, &address[0], 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c_write failed\n", __func__);
		goto out;
	}

	/* start Non-touched Peak Noise */
	address[0] = 0xC7;
	address[1] = 0x0B;
	address[2] = 0x01;
	address[3] = (u8)(sec->cmd_param[0] & 0xff);
	address[4] = (u8)(sec->cmd_param[0] >> 8);
	ret = ts->stm_ts_write(ts, &address[0], 5);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c_write failed\n", __func__);
		goto out;
	}


	wait_time = (sec->cmd_param[0] * 1000) / 120 + (sec->cmd_param[0] * 1000) % 120;

	sec_delay(wait_time);

	retry_cnt = 50;
	address[0] = 0x72;
	while ((ts->stm_ts_read(ts, &address[0], 1, (u8 *)&status, 2) > 0) && (retry_cnt-- > 0)) {
		if (status == 1)
			break;
		sec_delay(20);
	}

	if (status == 0) {
		input_err(true, &ts->client->dev, "%s: failed non-touched status:%d\n",
				__func__, status);
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

	/* EXIT SNR mode */
	address[0] = 0x70;
	address[1] = 0x00;
	ts->stm_ts_write(ts, &address[0], 2);
	stm_ts_fix_active_mode(ts, false);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, "OK");

	return;

out:
	address[0] = 0x70;
	address[1] = 0x00;
	ts->stm_ts_write(ts, &address[0], 2);
	stm_ts_fix_active_mode(ts, false);

out_init:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, "NG");
}

static void run_snr_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	struct stm_ts_snr_result_cmd snr_cmd_result;
	struct stm_ts_snr_result snr_result;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tbuff[SEC_CMD_STR_LEN] = { 0 };
	u8 address[5] = { 0 };
	int ret = 0;
	int wait_time = 0;
	int retry_cnt = 0;
	int i = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	memset(&snr_result, 0, sizeof(struct stm_ts_snr_result));
	memset(&snr_cmd_result, 0, sizeof(struct stm_ts_snr_result_cmd));

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		input_err(true, &ts->client->dev, "%s: strange value frame:%d\n",
				__func__, sec->cmd_param[0]);
		goto out_init;
	}

	stm_ts_fix_active_mode(ts, true);

	/* enter SNR mode */
	address[0] = 0x70;
	address[1] = 0x25;
	ret = ts->stm_ts_write(ts, &address[0], 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c_write failed\n", __func__);
		goto out;
	}

	/* start touched Peak Noise */
	address[0] = 0xC7;
	address[1] = 0x0B;
	address[2] = 0x02;
	address[3] = (u8)(sec->cmd_param[0] & 0xff);
	address[4] = (u8)(sec->cmd_param[0] >> 8);
	ret = ts->stm_ts_write(ts, &address[0], 5);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c_write failed\n", __func__);
		goto out;
	}

	wait_time = (sec->cmd_param[0] * 1000) / 120 + (sec->cmd_param[0] * 1000) % 120;

	sec_delay(wait_time);

	retry_cnt = 50;
	address[0] = 0x72;
	while ((ts->stm_ts_read(ts, &address[0], 1, (u8 *)&snr_cmd_result, 6) > 0) && (retry_cnt-- > 0)) {
		if (snr_cmd_result.status == 1)
			break;
		sec_delay(20);
	}

	if (snr_cmd_result.status == 0) {
		input_err(true, &ts->client->dev, "%s: failed non-touched status:%d\n",
				__func__, snr_cmd_result.status);
		goto out;
	} else {
		input_info(true, &ts->client->dev, "%s: status:%d, point:%d, average:%d\n", __func__,
			snr_cmd_result.status, snr_cmd_result.point, snr_cmd_result.average);
	}

	address[0] = 0x72;
	ret = ts->stm_ts_read(ts, &address[0], 1, (u8 *)&snr_result, sizeof(struct stm_ts_snr_result));
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c_write failed size:%ld\n",
				__func__, sizeof(struct stm_ts_snr_result));
		goto out;
	}

	for (i = 0; i < 9; i++) {
		input_info(true, &ts->client->dev, "%s: average:%d, snr1:%d, snr2:%d\n", __func__,
			snr_result.result[i].average, snr_result.result[i].snr1, snr_result.result[i].snr2);
		snprintf(tbuff, sizeof(tbuff), "%d,%d,%d,",
			snr_result.result[i].average,
			snr_result.result[i].snr1,
			snr_result.result[i].snr2);
		strlcat(buff, tbuff, sizeof(buff));
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	/* EXIT SNR mode */
	address[0] = 0x70;
	address[1] = 0x00;
	ts->stm_ts_write(ts, &address[0], 2);
	stm_ts_fix_active_mode(ts, false);
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return;
out:
	address[0] = 0x70;
	address[1] = 0x00;
	ts->stm_ts_write(ts, &address[0], 2);
	stm_ts_fix_active_mode(ts, false);
out_init:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_interrupt_gpio_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN];
	u8 drv_data[3] = { 0xA4, 0x01, 0x01 };
	u8 irq_data[3] = { 0xA4, 0x01, 0x00 };
	int drv_value = -1;
	int irq_value = -1;

	sec_input_irq_disable(ts->plat_data);

	ts->stm_ts_write(ts, drv_data, 3);
	sec_delay(50);
	drv_value = gpio_get_value(ts->plat_data->irq_gpio);
	input_info(true, &ts->client->dev, "%s: drv_value:%d (should be 0)\n", __func__, drv_value);

	ts->stm_ts_write(ts, irq_data, 3);
	sec_delay(50);
	irq_value = gpio_get_value(ts->plat_data->irq_gpio);
	input_info(true, &ts->client->dev, "%s: irq_value:%d (should be 1)\n", __func__, irq_value);

	if (drv_value == 0 && irq_value == 1) {
		snprintf(buff, sizeof(buff), "0");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		if (drv_value != 0)
			snprintf(buff, sizeof(buff), "1:HIGH");
		else if (irq_value != 1)
			snprintf(buff, sizeof(buff), "1:LOW");
		else
			snprintf(buff, sizeof(buff), "1:FAIL");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "INT_GPIO");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	ts->plat_data->init(ts);
	sec_input_irq_enable(ts->plat_data);
}

int stm_ts_get_tsp_test_result(struct stm_ts_data *ts)
{
	u8 data;
	int ret;

	ret = get_nvm_data(ts, STM_TS_NVM_OFFSET_FAC_RESULT, &data);
	if (ret < 0)
		goto err_read;

	if (data == 0xFF)
		data = 0;

	ts->test_result.data[0] = data;

err_read:
	return ret;
}

static void get_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	ret = stm_ts_get_tsp_test_result(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: get failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "M:%s, M:%d, A:%s, A:%d",
				ts->test_result.module_result == 0 ? "NONE" :
				ts->test_result.module_result == 1 ? "FAIL" :
				ts->test_result.module_result == 2 ? "PASS" : "A",
				ts->test_result.module_count,
				ts->test_result.assy_result == 0 ? "NONE" :
				ts->test_result.assy_result == 1 ? "FAIL" :
				ts->test_result.assy_result == 2 ? "PASS" : "A",
				ts->test_result.assy_count);

		sec_cmd_set_cmd_result(sec, buff, strlen(buff));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
}

/* FACTORY TEST RESULT SAVING FUNCTION
 * bit 3 ~ 0 : OCTA Assy
 * bit 7 ~ 4 : OCTA module
 * param[0] : OCTA module(1) / OCTA Assy(2)
 * param[1] : TEST NONE(0) / TEST FAIL(1) / TEST PASS(2) : 2 bit
 */
static void set_tsp_test_result(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = stm_ts_get_tsp_test_result(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: get failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}
	sec->cmd_state = SEC_CMD_STATUS_RUNNING;

	if (sec->cmd_param[0] == TEST_OCTA_ASSAY) {
		ts->test_result.assy_result = sec->cmd_param[1];
		if (ts->test_result.assy_count < 3)
			ts->test_result.assy_count++;

	} else if (sec->cmd_param[0] == TEST_OCTA_MODULE) {
		ts->test_result.module_result = sec->cmd_param[1];
		if (ts->test_result.module_count < 3)
			ts->test_result.module_count++;
	}

	input_info(true, &ts->client->dev, "%s: [0x%X] M:%s, M:%d, A:%s, A:%d\n",
			__func__, ts->test_result.data[0],
			ts->test_result.module_result == TEST_OCTA_NONE ? "NONE" :
			ts->test_result.module_result == TEST_OCTA_FAIL ? "FAIL" :
			ts->test_result.module_result == TEST_OCTA_ASSAY ? "PASS" : "A",
			ts->test_result.module_count,
			ts->test_result.assy_result == TEST_OCTA_NONE ? "NONE" :
			ts->test_result.assy_result == TEST_OCTA_FAIL ? "FAIL" :
			ts->test_result.assy_result == TEST_OCTA_ASSAY ? "PASS" : "A",
			ts->test_result.assy_count);

	ret = set_nvm_data(ts, STM_TS_NVM_OFFSET_FAC_RESULT, ts->test_result.data);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: set failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

int stm_ts_get_disassemble_count(struct stm_ts_data *ts)
{
	u8 data;
	int ret;

	ret = get_nvm_data(ts, STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT, &data);
	if (ret < 0)
		goto err_read;

	if (data == 0xFF)
		data = 0;

	ts->disassemble_count = data;

err_read:
	return ret;
}

static void increase_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = stm_ts_get_disassemble_count(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: get failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}
	sec->cmd_state = SEC_CMD_STATUS_RUNNING;

	if (ts->disassemble_count < 0xFE)
		ts->disassemble_count++;

	ret = set_nvm_data(ts, STM_TS_NVM_OFFSET_DISASSEMBLE_COUNT, &ts->disassemble_count);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: set failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

static void get_disassemble_count(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	ret = stm_ts_get_disassemble_count(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: get failed. ret: %d\n", __func__, ret);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%d", ts->disassemble_count);

		sec_cmd_set_cmd_result(sec, buff, strlen(buff));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
}

static int glove_mode_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}

	ts->glove_enabled = sec->cmd_param[0];
	if (ts->glove_enabled)
		ts->plat_data->touch_functions |= STM_TS_TOUCHTYPE_BIT_GLOVE;
	else
		ts->plat_data->touch_functions &= ~STM_TS_TOUCHTYPE_BIT_GLOVE;

	sec->cmd_state = SEC_CMD_STATUS_OK;
	return SEC_SUCCESS;
}


static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = glove_mode_save(device_data);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: %s\n", __func__, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (ts->probe_done)
		stm_ts_set_touch_function(ts);
	else
		input_info(true, &ts->client->dev, "%s: probe is not done\n", __func__);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, "OK");
}

static int clear_cover_mode_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

#ifdef CONFIG_SEC_FACTORY
	input_info(true, &ts->client->dev, "%s: skip for factory binary\n", __func__);
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	return SEC_SUCCESS;
#endif

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}

	if (sec->cmd_param[0] > 1) {
		ts->flip_enable = true;
		ts->plat_data->cover_type = sec->cmd_param[1];
	} else {
		ts->flip_enable = false;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	return SEC_SUCCESS;
};

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = clear_cover_mode_save(device_data);
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, &ts->client->dev, "%s: %s\n", __func__, "NG");
		return;
	}

	if (ts->reinit_done && ts->probe_done) {
		mutex_lock(&ts->plat_data->enable_mutex);
		stm_ts_set_cover_type(ts, ts->flip_enable);
		mutex_unlock(&ts->plat_data->enable_mutex);
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, "OK");
};

/*
 * set_wirelesscharger_mode
 *
 *	cmd_param is refered to batteryservice.java
 *	0 : none charge
 *	1 : wireless charger
 *	3 : wireless charge battery pack
 */

static int set_wirelesscharger_mode_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret = 0;

	ret = sec_input_check_wirelesscharger_mode(&ts->client->dev, sec->cmd_param[0], sec->cmd_param[1]);
	if (ret == SEC_ERROR)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;

	return ret;
};


static void set_wirelesscharger_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = set_wirelesscharger_mode_save(device_data);
	if (ret != SEC_SUCCESS)
		return;

	if (ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_CHARGER)
		ts->charger_mode = STM_TS_CHARGER_MODE_WIRELESS_CHARGER;
	else if (ts->plat_data->wirelesscharger_mode == TYPE_WIRELESS_BATTERY_PACK)
		ts->charger_mode = STM_TS_CHARGER_MODE_WIRELESS_BATTERY_PACK;
	else
		ts->charger_mode = STM_TS_CHARGER_MODE_NORMAL;

	ret = stm_ts_charger_mode(ts);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

}

/*********************************************************************
 * flag	1  :  set edge handler
 *	2  :  set (portrait, normal) edge zone data
 *	4  :  set (portrait, normal) dead zone data
 *	8  :  set landscape mode data
 *	16 :  mode clear
 * data
 *	0x00, FFF (y start), FFF (y end),  FF(direction)
 *	0x01, FFFF (edge zone)
 *	0x02, FF (up x), FF (down x), FFFF (y)
 *	0x03, FF (mode), FFF (edge), FFF (dead zone)
 * case
 *	edge handler set :  0x00....
 *	booting time :  0x00...  + 0x01...
 *	normal mode : 0x02...  (+0x01...)
 *	landscape mode : 0x03...
 *	landscape -> normal (if same with old data) : 0x03, 0
 *	landscape -> normal (etc) : 0x02....  + 0x03, 0
 *********************************************************************/

void stm_ts_set_grip_data_to_ic(struct device *dev, u8 flag)
{
	struct stm_ts_data *ts = dev_get_drvdata(dev);
	u8 data[4] = { 0 };
	u8 regAdd[11] = {STM_TS_CMD_SET_FUNCTION_ONOFF, };

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: ic power is off\n", __func__);
		return;
	}

	input_info(true, &ts->client->dev, "%s: flag: %02X (clr,lan,nor,edg,han)\n", __func__, flag);

	memset(&regAdd[1], 0x00, 10);

	if (flag & G_SET_EDGE_HANDLER) {
		if (ts->plat_data->grip_data.edgehandler_direction == 0) {
			data[0] = 0x0;
			data[1] = 0x0;
			data[2] = 0x0;
			data[3] = 0x0;
		} else {
			data[0] = (ts->plat_data->grip_data.edgehandler_start_y >> 4) & 0xFF;
			data[1] = (ts->plat_data->grip_data.edgehandler_start_y << 4 & 0xF0) |
					((ts->plat_data->grip_data.edgehandler_end_y >> 8) & 0xF);
			data[2] = ts->plat_data->grip_data.edgehandler_end_y & 0xFF;
			data[3] = ts->plat_data->grip_data.edgehandler_direction & 0xF;
		}

		regAdd[1] = STM_TS_FUNCTION_EDGE_HANDLER;
		regAdd[2] = data[0];
		regAdd[3] = data[1];
		regAdd[4] = data[2];
		regAdd[5] = data[3];

		ts->stm_ts_write(ts, regAdd, 6);
	}

	if (flag & G_SET_EDGE_ZONE) {
		/*
		 * ex) C1 07 00 3E 00 3E
		 *	- 0x003E(60) px : Grip Right zone
		 *	- 0x003E(60) px : Grip Left zone
		 */
		regAdd[1] = STM_TS_FUNCTION_EDGE_AREA;
		regAdd[2] = (ts->plat_data->grip_data.edge_range >> 8) & 0xFF;
		regAdd[3] = ts->plat_data->grip_data.edge_range & 0xFF;
		regAdd[4] = (ts->plat_data->grip_data.edge_range >> 8) & 0xFF;
		regAdd[5] = ts->plat_data->grip_data.edge_range & 0xFF;

		ts->stm_ts_write(ts, regAdd, 6);
	}

	if (flag & G_SET_NORMAL_MODE) {
		/*
		 * ex) C1 08 1E 1E 00 00
		 *	- 0x1E (30) px : upper X range
		 *	- 0x1E (30) px : lower X range
		 *	- 0x0000 (0) px : division Y
		 */
		regAdd[1] = STM_TS_FUNCTION_DEAD_ZONE;
		regAdd[2] = ts->plat_data->grip_data.deadzone_up_x & 0xFF;
		regAdd[3] = ts->plat_data->grip_data.deadzone_dn_x & 0xFF;
		regAdd[4] = (ts->plat_data->grip_data.deadzone_y >> 8) & 0xFF;
		regAdd[5] = ts->plat_data->grip_data.deadzone_y & 0xFF;

		ts->stm_ts_write(ts, regAdd, 6);
	}

	if (flag & G_SET_LANDSCAPE_MODE) {
		/*
		 * ex) C1 09 01 00 3C 00 3C 00 1E
		 *	- 0x01 : horizontal mode
		 *	- 0x03C (60) px : Grip zone range (Right)
		 *	- 0x03C (60) px : Grip zone range (Left)
		 *	- 0x01E (30) px : Reject zone range (Left/Right)
		 */
		regAdd[1] = STM_TS_FUNCTION_LANDSCAPE_MODE;
		regAdd[2] = ts->plat_data->grip_data.landscape_mode;
		regAdd[3] = (ts->plat_data->grip_data.landscape_edge >> 8) & 0xFF;
		regAdd[4] = ts->plat_data->grip_data.landscape_edge & 0xFF;
		regAdd[5] = (ts->plat_data->grip_data.landscape_edge >> 8) & 0xFF;
		regAdd[6] = ts->plat_data->grip_data.landscape_edge & 0xFF;
		regAdd[7] = (ts->plat_data->grip_data.landscape_deadzone >> 8) & 0xFF;
		regAdd[8] = ts->plat_data->grip_data.landscape_deadzone & 0xFF;

		ts->stm_ts_write(ts, regAdd, 9);

		/*
		 * ex) C1 0A 01 00 3C 00 3C 00 1E 00 1E
		 *	- 0x01(1) : Enable function
		 *	- 0x003C (60) px : Grip Top zone range
		 *	- 0x001E (60) px : Grip Bottom zone range
		 *	- 0x001E (30) px : Reject Top zone range
		 *	- 0x001E (30) px : Reject Bottom zone range
		 */
		regAdd[1] = STM_TS_FUNCTION_LANDSCAPE_TOP_BOTTOM;
		regAdd[2] = ts->plat_data->grip_data.landscape_mode;
		regAdd[3] = (ts->plat_data->grip_data.landscape_top_gripzone >> 8) & 0xFF;
		regAdd[4] = ts->plat_data->grip_data.landscape_top_gripzone & 0xFF;
		regAdd[5] = (ts->plat_data->grip_data.landscape_bottom_gripzone >> 8) & 0xFF;
		regAdd[6] = ts->plat_data->grip_data.landscape_bottom_gripzone & 0xFF;
		regAdd[7] = (ts->plat_data->grip_data.landscape_top_deadzone >> 8) & 0xFF;
		regAdd[8] = ts->plat_data->grip_data.landscape_top_deadzone & 0xFF;
		regAdd[9] = (ts->plat_data->grip_data.landscape_bottom_deadzone >> 8) & 0xFF;
		regAdd[10] = ts->plat_data->grip_data.landscape_bottom_deadzone & 0xFF;

		ts->stm_ts_write(ts, regAdd, 11);
	}

	if (flag & G_CLR_LANDSCAPE_MODE) {
		memset(&regAdd[1], 0x00, 10);

		/*
		 * ex) C1 09  00 00 00 00 00 00 00
		 *	- 0x00 : Apply previous vertical mode value for grip zone and reject zone range
		 */
		regAdd[1] = STM_TS_FUNCTION_LANDSCAPE_MODE;
		regAdd[2] = ts->plat_data->grip_data.landscape_mode;

		ts->stm_ts_write(ts, regAdd, 9);

		/*
		 *ex) C1 0A 00 00 00 00 00 00 00 00 00
		 *	- Disable function
		 */
		regAdd[1] = STM_TS_FUNCTION_LANDSCAPE_TOP_BOTTOM;

		ts->stm_ts_write(ts, regAdd, 11);
	}
}

/*********************************************************************
 * index
 *	0 :  set edge handler
 *	1 :  portrait (normal) mode
 *	2 :  landscape mode
 * data
 *	0, X (direction), X (y start), X (y end)
 *	direction : 0 (off), 1 (left), 2 (right)
 *	ex) echo set_grip_data,0,2,600,900 > cmd
 *
 *
 *	1, X (edge zone), X (dead zone up x), X (dead zone down x), X (dead zone y)
 *	ex) echo set_grip_data,1,200,10,50,1500 > cmd
 *
 *	2, 1 (landscape mode), X (edge zone), X (dead zone), X (dead zone top y), X (dead zone bottom y)
 *	ex) echo set_grip_data,2,1,200,100,120,0 > cmd
 *
 *	2, 0 (portrait mode)
 *	ex) echo set_grip_data,2,0  > cmd
 *********************************************************************/

static int set_grip_data_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	int mode = G_NONE;

	mode = sec_input_store_grip_data(sec->dev, sec->cmd_param);
	if (mode < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return mode;
}


static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	u8 mode = G_NONE;

	mode = set_grip_data_save(device_data);
	if (mode < 0)
		return;

	mutex_lock(&ts->plat_data->enable_mutex);
	stm_ts_set_grip_data_to_ic(&ts->client->dev, mode);
	mutex_unlock(&ts->plat_data->enable_mutex);
}

static int dead_zone_enable_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}

	ts->dead_zone = sec->cmd_param[0];
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = dead_zone_enable_save(device_data);
	if (ret < 0)
		return;

	ret = stm_ts_dead_zone_enable(ts);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

static int spay_enable_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_SWIPE;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SWIPE;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}


static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret = 0;

	if (spay_enable_save(device_data) < 0)
		return;

	ret = stm_ts_set_custom_library(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

static int aot_enable_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret = 0;

	if (aot_enable_save(device_data) < 0)
		return;

	ret = stm_ts_set_custom_library(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);
}

static int aod_enable_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_AOD;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_AOD;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret = 0;

	if (aod_enable_save(device_data) < 0)
		return;

	ret = stm_ts_set_custom_library(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);
}

static int set_aod_rect_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int i;

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &ts->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);
#endif

	for (i = 0; i < 4; i++)
		ts->plat_data->aod_data.rect_data[i] = sec->cmd_param[i];

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

int stm_ts_set_aod_rect(struct stm_ts_data *ts)
{
	int i, ret;
	u8 data[8];

	for (i = 0; i < 4; i++) {
		data[i * 2] = ts->plat_data->aod_data.rect_data[i] & 0xFF;
		data[i * 2 + 1] = (ts->plat_data->aod_data.rect_data[i] >> 8) & 0xFF;
	}

	ret = ts->stm_ts_write_to_sponge(ts, STM_TS_CMD_SPONGE_OFFSET_AOD_RECT, data, sizeof(data));
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);

	return ret;
}

static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	set_aod_rect_save(device_data);

	ret = stm_ts_set_aod_rect(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
}

static void fod_lp_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (!ts->plat_data->support_fod) {
		input_err(true, &ts->client->dev, "%s: fod is not supported\n", __func__);
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	}

	ts->plat_data->fod_lp_mode = sec->cmd_param[0] & 0xFF;
	input_info(true, &ts->client->dev, "%s: %d, lp:0x%02X\n",
			__func__, ts->plat_data->fod_lp_mode, ts->plat_data->lowpower_mode);

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void fod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret = 0;

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	} else if (!ts->plat_data->support_fod) {
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_PRESS;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_PRESS;

	ts->plat_data->fod_data.press_prop = !!sec->cmd_param[1];

	input_info(true, &ts->client->dev, "%s: %s, fast:%d, 0x%02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off",
			ts->plat_data->fod_data.press_prop, ts->plat_data->lowpower_mode);

	mutex_lock(&ts->plat_data->enable_mutex);
	if (!atomic_read(&ts->plat_data->enabled) && sec_input_need_ic_off(ts->plat_data)) {
		ts->plat_data->stop_device(ts);
	} else {
		ret = stm_ts_set_custom_library(ts);
		if (ret < 0) {
			mutex_unlock(&ts->plat_data->enable_mutex);
			input_err(true, &ts->client->dev,
					"%s: failed. ret: %d\n", __func__, ret);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			return;
		}

		stm_ts_set_press_property(ts);
		stm_ts_set_fod_finger_merge(ts);
	}
	mutex_unlock(&ts->plat_data->enable_mutex);

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

int stm_ts_set_fod_rect(struct stm_ts_data *ts)
{
	int i, ret;
	u8 data[8];

	if (ts->plat_data->fod_data.set_val == 0)
		return 0;

	for (i = 0; i < 4; i++) {
		data[i * 2] = ts->plat_data->fod_data.rect_data[i] & 0xFF;
		data[i * 2 + 1] = (ts->plat_data->fod_data.rect_data[i] >> 8) & 0xFF;
	}

	input_info(true, &ts->client->dev, "%s: l:%d, t:%d, r:%d, b:%d\n",
			__func__, ts->plat_data->fod_data.rect_data[0],
			ts->plat_data->fod_data.rect_data[1],
			ts->plat_data->fod_data.rect_data[2],
			ts->plat_data->fod_data.rect_data[3]);

	ret = ts->stm_ts_write_to_sponge(ts, STM_TS_CMD_SPONGE_FOD_RECT, data, sizeof(data));
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);

	return ret;
}

static int set_fod_rect_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: l:%d, t:%d, r:%d, b:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	if (!ts->plat_data->support_fod) {
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		return SEC_ERROR;
	}

	if (!sec_input_set_fod_rect(&ts->client->dev, sec->cmd_param)) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

static void set_fod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = set_fod_rect_save(device_data);
	if (ret < 0)
		return;

	ret = stm_ts_set_fod_rect(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}
}

static int singletap_enable_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_SINGLE_TAP;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SINGLE_TAP;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}


static void singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret = 0;

	singletap_enable_save(device_data);

	input_info(true, &ts->client->dev, "%s: %s, 0x%02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	ret = stm_ts_set_custom_library(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: failed. ret: %d\n", __func__, ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}
}

/*
 * Enable or disable external_noise_mode
 *
 * If mode has EXT_NOISE_MODE_MAX,
 * then write enable cmd for all enabled mode. (set as ts->plat_data->external_noise_mode bit value)
 * This routine need after IC power reset. TSP IC need to be re-wrote all enabled modes.
 *
 * Else if mode has specific value like EXT_NOISE_MODE_MONITOR,
 * then write enable/disable cmd about for that mode's latest setting value.
 *
 * If you want to add new mode,
 * please define new enum value like EXT_NOISE_MODE_MONITOR,
 * then set cmd for that mode like below. (it is in this function)
 * noise_mode_cmd[EXT_NOISE_MODE_MONITOR] = SEC_TS_CMD_SET_MONITOR_NOISE_MODE;
 */

int stm_ts_set_external_noise_mode(struct stm_ts_data *ts, u8 mode)
{
	int i, ret, fail_count = 0;
	u8 mode_bit_to_set, check_bit, mode_enable;
	u8 noise_mode_cmd[EXT_NOISE_MODE_MAX] = { 0 };
	u8 regAdd[3] = {STM_TS_CMD_SET_FUNCTION_ONOFF, 0x00, 0x00};

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return -ENODEV;
	}

	if (mode == EXT_NOISE_MODE_MAX) {
		/* write all enabled mode */
		mode_bit_to_set = ts->plat_data->external_noise_mode;
	} else {
		/* make enable or disable the specific mode */
		mode_bit_to_set = 1 << mode;
	}

	input_info(true, &ts->client->dev, "%s: %sable %d\n", __func__,
			ts->plat_data->external_noise_mode & mode_bit_to_set ? "en" : "dis", mode_bit_to_set);

	/* set cmd for each mode */
	noise_mode_cmd[EXT_NOISE_MODE_MONITOR] = STM_TS_FUNCTION_SET_MONITOR_NOISE_MODE;

	/* write mode */
	for (i = EXT_NOISE_MODE_NONE + 1; i < EXT_NOISE_MODE_MAX; i++) {
		check_bit = 1 << i;
		if (mode_bit_to_set & check_bit) {
			mode_enable = !!(ts->plat_data->external_noise_mode & check_bit);
			regAdd[1] = noise_mode_cmd[i];
			regAdd[2] = mode_enable;

			ret = ts->stm_ts_write(ts, regAdd, 3);
			if (ret < 0) {
				input_err(true, &ts->client->dev, "%s: failed to set %02X %02X %02X\n",
						__func__, regAdd[0], regAdd[1], regAdd[2]);
				fail_count++;
			}
		}
	}

	if (fail_count != 0)
		return -EIO;
	else
		return 0;
}

/*
 * Enable or disable specific external_noise_mode (sec_cmd)
 *
 * This cmd has 2 params.
 * param 0 : the mode that you want to change.
 * param 1 : enable or disable the mode.
 *
 * For example,
 * enable EXT_NOISE_MODE_MONITOR mode,
 * write external_noise_mode,1,1
 * disable EXT_NOISE_MODE_MONITOR mode,
 * write external_noise_mode,1,0
 */
static int external_noise_mode_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] <= EXT_NOISE_MODE_NONE || sec->cmd_param[0] >= EXT_NOISE_MODE_MAX ||
			sec->cmd_param[1] < 0 || sec->cmd_param[1] > 1) {
		input_err(true, &ts->client->dev, "%s: not support param\n", __func__);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}

	if (sec->cmd_param[1] == 1)
		ts->plat_data->external_noise_mode |= 1 << sec->cmd_param[0];
	else
		ts->plat_data->external_noise_mode &= ~(1 << sec->cmd_param[0]);

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

static void external_noise_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = external_noise_mode_save(device_data);
	if (ret < 0)
		return;

	ret = stm_ts_set_external_noise_mode(ts, sec->cmd_param[0]);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
}

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	ts->debug_string = sec->cmd_param[0];

	input_info(true, &ts->client->dev, "%s: command is %d\n", __func__, ts->debug_string);

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
}


static void run_sram_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char buff[SEC_CMD_STR_LEN];
	u8 regAdd;
	u8 data[STM_TS_EVENT_SIZE];
	int rc, retry = 0;

	ts->stm_ts_systemreset(ts, 0);

	sec_input_irq_disable(ts->plat_data);
	mutex_lock(&ts->wait_for);

	regAdd = STM_TS_CMD_RUN_SRAM_TEST;
	rc = ts->stm_ts_write(ts, &regAdd, 1);
	if (rc < 0) {
		input_err(true, &ts->client->dev, "%s: failed to write cmd\n", __func__);
		goto error;
	}

	sec_delay(300);

	memset(data, 0x0, STM_TS_EVENT_SIZE);
	rc = -EIO;
	regAdd = STM_TS_READ_ONE_EVENT;
	while (ts->stm_ts_read(ts, &regAdd, 1, data, STM_TS_EVENT_SIZE) > 0) {
		if (data[0] != 0x00)
			input_info(true, &ts->client->dev,
					"%s: event %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);

		if (data[0] == STM_TS_EVENT_PASS_REPORT && data[1] == STM_TS_EVENT_SRAM_TEST_RESULT) {
			rc = 0; /* PASS */
			break;
		} else if (data[0] == STM_TS_EVENT_ERROR_REPORT && data[1] == STM_TS_EVENT_SRAM_TEST_RESULT) {
			rc = 1; /* FAIL */
			break;
		}

		if (retry++ > STM_TS_RETRY_COUNT * 25) {
			input_err(true, &ts->client->dev,
					"%s: Time Over (%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X)\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7]);
			break;
		}
		sec_delay(20);
	}

error:
	mutex_unlock(&ts->wait_for);

	if (rc < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%d", rc);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	ts->plat_data->init(ts);
	sec_input_irq_enable(ts->plat_data);
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (sec_input_cmp_ic_status(&ts->client->dev, CHECK_POWEROFF)) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	mutex_lock(&ts->plat_data->enable_mutex);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	enter_factory_mode(ts, true);

	run_rawcap_read(sec);
	run_self_raw_read(sec);

	stm_ts_set_scanmode(ts, STM_TS_SCAN_MODE_SCAN_OFF);
	stm_ts_release_all_finger(ts);

	run_cx_data_read(sec);
	get_cx_gap_data(sec);
	run_ix_data_read(sec);

	/* do not systemreset in COB type */
	if (ts->plat_data->chip_on_board)
		stm_ts_set_scanmode(ts, ts->scan_mode);
	else
		enter_factory_mode(ts, false);

	run_factory_miscalibration(sec);
	run_sram_test(sec);
	run_interrupt_gpio_test(sec);

	run_high_frequency_rawcap_read_all(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	mutex_unlock(&ts->plat_data->enable_mutex);

out:
	input_info(true, &ts->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void run_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	input_info(true, &ts->client->dev, "%s\n",	__func__);

	stm_ts_panel_ito_test(ts, OPEN_SHORT_CRACK_TEST);
	run_reference_read(sec);
	run_factory_miscalibration(sec);
	run_high_frequency_rawcap_read_all(sec);

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

void stm_ts_run_rawdata_all(struct stm_ts_data *ts)
{
	struct sec_cmd_data *sec = &ts->sec;

	run_rawdata_read_all(sec);
}

static int run_force_calibration_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
#ifdef TCLM_CONCEPT
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	ts->tdata->external_factory = false;
#endif
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return SEC_SUCCESS;
}


static void run_force_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	bool touch_on = false;

	if (ts->rawdata_read_lock == 1) {
		input_err(true, &ts->client->dev, "%s: ramdump mode is running, %d\n",
				__func__, ts->rawdata_read_lock);
		goto autotune_fail;
	}

	if (ts->plat_data->touch_count > 0) {
		touch_on = true;
		input_err(true, &ts->client->dev, "%s: finger on touch(%d)\n", __func__, ts->plat_data->touch_count);
	}

	ts->stm_ts_systemreset(ts, 0);

	stm_ts_release_all_finger(ts);

	if (touch_on) {
		input_err(true, &ts->client->dev, "%s: finger! do not run autotune\n", __func__);
	} else {
		input_info(true, &ts->client->dev, "%s: run autotune\n", __func__);

		input_err(true, &ts->client->dev, "%s: RUN OFFSET CALIBRATION\n", __func__);
		if (stm_ts_execute_autotune(ts, true) < 0) {
			stm_ts_set_scanmode(ts, ts->scan_mode);
			goto autotune_fail;
		}
#ifdef TCLM_CONCEPT
		/* devide tclm case */
		sec_tclm_case(ts->tdata, sec->cmd_param[0]);

		input_info(true, &ts->client->dev, "%s: param, %d, %c, %d\n", __func__,
			sec->cmd_param[0], sec->cmd_param[0], ts->tdata->root_of_calibration);

		if (sec_execute_tclm_package(ts->tdata, 1) < 0)
			input_err(true, &ts->client->dev,
						"%s: sec_execute_tclm_package\n", __func__);

		sec_tclm_root_of_cal(ts->tdata, CALPOSITION_NONE);
#endif
	}

	stm_ts_set_scanmode(ts, ts->scan_mode);

	if (touch_on)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	else
		sec->cmd_state = SEC_CMD_STATUS_OK;

#ifdef TCLM_CONCEPT
	ts->tdata->external_factory = false;
#endif
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
	return;

autotune_fail:
#ifdef TCLM_CONCEPT
	ts->tdata->external_factory = false;
#endif
	sec_input_irq_enable(ts->plat_data);
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

static void set_factory_level(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < OFFSET_FAC_SUB || sec->cmd_param[0] > OFFSET_FAC_MAIN) {
		input_err(true, &ts->client->dev,
				"%s: cmd data is abnormal, %d\n", __func__, sec->cmd_param[0]);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ts->factory_position = sec->cmd_param[0];

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->factory_position);
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static int fix_active_mode_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}
	ts->fix_active_mode = !!sec->cmd_param[0];
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

static void fix_active_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = fix_active_mode_save(device_data);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: %s\n", __func__, "NG");
		return;
	}

	stm_ts_fix_active_mode(ts, ts->fix_active_mode);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, "OK");
}

static int touch_aging_mode_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}

	ts->touch_aging_mode = sec->cmd_param[0];
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

static void touch_aging_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	char regAdd[3];
	int ret;

	ret = touch_aging_mode_save(device_data);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: %s\n", __func__, "NG");
		return;
	}

	if (ts->touch_aging_mode) {
		regAdd[0] = 0xA0;
		regAdd[1] = 0x03;
		regAdd[2] = 0x20;

		ts->stm_ts_write(ts, &regAdd[0], 3);
	} else {
		ts->plat_data->init(ts);
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, "OK");
}

static int set_sip_mode_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}
	ts->sip_mode = sec->cmd_param[0];
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, ts->sip_mode ? "enable" : "disable");

	sec->cmd_state = SEC_CMD_STATUS_OK;
	return SEC_SUCCESS;
}

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = set_sip_mode_save(device_data);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: %s\n", __func__, "NG");
		return;
	}

	ret = stm_ts_sip_mode_enable(ts);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

static int set_note_mode_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}
	ts->note_mode = sec->cmd_param[0];
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, ts->note_mode ? "enable" : "disable");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

static void set_note_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = set_note_mode_save(device_data);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: %s\n", __func__, "NG");
		return;
	}

	ret = stm_ts_note_mode_enable(ts);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

/*
 *	for game mode
 *	byte[0]: Setting for the Game Mode
 *		- 0: Disable
 *		- 1: Enable
 */
static int set_game_mode_save(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return SEC_ERROR;
	}

	ts->game_mode = sec->cmd_param[0];
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, ts->game_mode ? "enable" : "disable");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return SEC_SUCCESS;
}

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);
	int ret;

	ret = set_game_mode_save(device_data);
	if (ret < 0) {
		input_info(true, &ts->client->dev, "%s: %s\n", __func__, "NG");
		return;
	}

	ret = stm_ts_game_mode_enable(ts);
	if (ret < 0)
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, sec->cmd_state == SEC_CMD_STATUS_OK ? "OK" : "NG");
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct stm_ts_data *ts = container_of(sec, struct stm_ts_data, sec);

	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, "NA");
}

struct sec_cmd sec_cmds[] = {
	{SEC_CMD_V2("fw_update", fw_update, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_fw_ver_bin", get_fw_ver_bin, NULL, CHECK_ALL, WAIT_RESULT),},
	{SEC_CMD_V2("get_fw_ver_ic", get_fw_ver_ic, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("module_off_master", module_off_master, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("module_on_master", module_on_master, NULL, CHECK_POWEROFF, WAIT_RESULT),},
	{SEC_CMD_V2("get_chip_vendor", get_chip_vendor, NULL, CHECK_ALL, WAIT_RESULT),},
	{SEC_CMD_V2("get_chip_name", get_chip_name, NULL, CHECK_ALL, WAIT_RESULT),},
	{SEC_CMD_V2("get_wet_mode", get_wet_mode, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_x_num", get_x_num, NULL, CHECK_ALL, WAIT_RESULT),},
	{SEC_CMD_V2("get_y_num", get_y_num, NULL, CHECK_ALL, WAIT_RESULT),},
	{SEC_CMD_V2("get_checksum_data", get_checksum_data, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_crc_check", check_fw_corruption, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_jitter_test", run_jitter_test, NULL, CHECK_POWERON, WAIT_RESULT),},
	{SEC_CMD_V2("run_factory_miscalibration", run_factory_miscalibration, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_factory_miscalibration_read_all", run_factory_miscalibration_read_all, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_miscalibration", run_miscalibration, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_reference_read", run_reference_read, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_rawcap_read", run_rawcap_read, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_rawcap_read_all", run_rawcap_read_all, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_delta_read", run_delta_read, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_cs_raw_read_all", run_rawcap_read_all, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_cs_delta_read_all", get_strength_all_data, NULL, CHECK_ON_LP, WAIT_RESULT),},
#ifdef TCLM_CONCEPT
	{SEC_CMD_V2("get_pat_information", get_pat_information, NULL, CHECK_ALL, WAIT_RESULT),},
	{SEC_CMD_V2("set_external_factory", set_external_factory, NULL, CHECK_ALL, WAIT_RESULT),},
	{SEC_CMD_V2("tclm_test_cmd", tclm_test_cmd, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_calibration", get_calibration, NULL, CHECK_ALL, WAIT_RESULT),},
#endif
	{SEC_CMD_V2("run_ix_data_read", run_ix_data_read, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_ix_data_read_all", run_ix_data_read_all, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_self_raw_read", run_self_raw_read, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_self_raw_read_all", run_self_raw_read_all, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_trx_short_test", run_trx_short_test, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("check_connection", check_connection, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_vpump_cap_data_read", run_vpump_cap_data_read, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_vpump_cap_tx_data", get_vpump_cap_tx_data, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_vpump_cap_sum_data", get_vpump_cap_sum_data, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_cx_data_read", run_cx_data_read, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_cx_data_read_all", get_cx_all_data, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_cx_all_data", get_cx_all_data, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_cx_gap_data_x_all", run_cx_gap_data_x_all, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_cx_gap_data_y_all", run_cx_gap_data_y_all, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_strength_all_data", get_strength_all_data, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_high_frequency_rawcap_read_all", run_high_frequency_rawcap_read_all, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_snr_non_touched", run_snr_non_touched, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_snr_touched", run_snr_touched, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_interrupt_gpio_test", run_interrupt_gpio_test, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_tsp_test_result", get_tsp_test_result, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("set_tsp_test_result", set_tsp_test_result, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("increase_disassemble_count", increase_disassemble_count, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("get_disassemble_count", get_disassemble_count, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("run_rawdata_read_all", run_rawdata_read_all, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2_H("glove_mode", glove_mode, glove_mode_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2_H("clear_cover_mode", clear_cover_mode, clear_cover_mode_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2("set_wirelesscharger_mode", set_wirelesscharger_mode, set_wirelesscharger_mode_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2("set_grip_data", set_grip_data, set_grip_data_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2("dead_zone_enable", dead_zone_enable, dead_zone_enable_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2_H("spay_enable", spay_enable, spay_enable_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2_H("aot_enable", aot_enable, aot_enable_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2_H("aod_enable", aod_enable, aod_enable_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2("set_aod_rect", set_aod_rect, set_aod_rect_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2_H("fod_lp_mode", fod_lp_mode, NULL, CHECK_ALL, EXIT_RESULT),},
	{SEC_CMD_V2_H("fod_enable", fod_enable, NULL, CHECK_ALL, EXIT_RESULT),},
	{SEC_CMD_V2("set_fod_rect", set_fod_rect, set_fod_rect_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2_H("singletap_enable", singletap_enable, singletap_enable_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2_H("external_noise_mode", external_noise_mode, external_noise_mode_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2("debug", debug, NULL, CHECK_ALL, EXIT_RESULT),},
	{SEC_CMD_V2("run_sram_test", run_sram_test, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("factory_cmd_result_all", factory_cmd_result_all, NULL, CHECK_ALL,  WAIT_RESULT),},
	{SEC_CMD_V2("run_force_calibration", run_force_calibration, run_force_calibration_save, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2("set_factory_level", set_factory_level, NULL, CHECK_ON_LP, WAIT_RESULT),},
	{SEC_CMD_V2_H("fix_active_mode", fix_active_mode, fix_active_mode_save, CHECK_POWERON, EXIT_RESULT),},
	{SEC_CMD_V2_H("touch_aging_mode", touch_aging_mode, touch_aging_mode_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2("set_sip_mode", set_sip_mode, set_sip_mode_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2_H("set_note_mode", set_note_mode, set_note_mode_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2_H("set_game_mode", set_game_mode, set_game_mode_save, CHECK_ON_LP, EXIT_RESULT),},
	{SEC_CMD_V2("not_support_cmd", not_support_cmd, NULL, CHECK_ALL, EXIT_RESULT),},
};

int stm_ts_fn_init(struct stm_ts_data *ts)
{
	int retval = 0;

	retval = sec_cmd_init(&ts->sec, &ts->client->dev, sec_cmds, ARRAY_SIZE(sec_cmds),
				SEC_CLASS_DEVT_TSP, &cmd_attr_group);
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
	}
	return retval;
}

void stm_ts_fn_remove(struct stm_ts_data *ts)
{
	input_err(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
}
