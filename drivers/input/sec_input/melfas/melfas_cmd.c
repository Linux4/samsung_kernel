/*
 * MELFAS MMS400 Touchscreen
 *
 * Copyright (C) 2014 MELFAS Inc.
 *
 *
 * Command Functions (Optional)
 *
 */

#include "melfas_dev.h"

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[256] = { 0 };

#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
	input_info(true, &ts->client->dev,
			"%s: id: %d\n", __func__, ts->plat_data->gesture_id);
#else
	input_info(true, &ts->client->dev,
			"%s: id: %d, X:%d, Y:%d\n", __func__,
			ts->plat_data->gesture_id, ts->plat_data->gesture_x, ts->plat_data->gesture_y);
#endif
	snprintf(buff, sizeof(buff), "%d %d %d", ts->plat_data->gesture_id,
			ts->plat_data->gesture_x, ts->plat_data->gesture_y);

	ts->plat_data->gesture_x = 0;
	ts->plat_data->gesture_y = 0;

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

/* for bigdata */
/* read param */
static ssize_t hardware_param_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_INPUT_HW_PARAM_SIZE];
	char tbuff[SEC_CMD_STR_LEN];
	char temp[SEC_CMD_STR_LEN];

	memset(buff, 0x00, sizeof(buff));

	sec_input_get_common_hw_param(ts->plat_data, buff);

	/* module_id */
	memset(tbuff, 0x00, sizeof(tbuff));
	snprintf(tbuff, sizeof(tbuff), ",\"TMOD\":\"SE%02X%02X%02X%c%01X\"",
			ts->plat_data->img_version_of_bin[1], ts->plat_data->img_version_of_bin[2],
			ts->plat_data->img_version_of_bin[3],
			'0', 0);

	strlcat(buff, tbuff, sizeof(buff));

	/* vendor_id */
	memset(tbuff, 0x00, sizeof(tbuff));
	if (ts->plat_data->firmware_name) {
		memset(temp, 0x00, sizeof(temp));
		snprintf(temp, 5, "%s", ts->plat_data->firmware_name);

		snprintf(tbuff, sizeof(tbuff), ",\"TVEN\":\"MELFAS_%s\"", temp);
	} else {
		snprintf(tbuff, sizeof(tbuff), ",\"TVEN\":\"MELFAS\"");
	}
	strlcat(buff, tbuff, sizeof(buff));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

/* clear param */
static ssize_t hardware_param_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	sec_input_clear_common_hw_param(ts->plat_data);

	return count;
}


static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__,
			ts->plat_data->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->plat_data->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	long data;
	int ret;

	ret = kstrtol(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, &ts->client->dev, "%s: %ld\n", __func__, data);

	ts->plat_data->prox_power_off = data;

	return count;
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	u32 feature = 0;

	if (ts->plat_data->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	if (ts->plat_data->sync_reportrate_120)
		feature |= INPUT_FEATURE_ENABLE_SYNC_RR120;

	if (ts->plat_data->support_vrr)
		feature |= INPUT_FEATURE_ENABLE_VRR;

	if (ts->plat_data->support_open_short_test)
		feature |= INPUT_FEATURE_SUPPORT_OPEN_SHORT_TEST;

	if (ts->plat_data->support_mis_calibration_test)
		feature |= INPUT_FEATURE_SUPPORT_MIS_CALIBRATION_TEST;

	if (ts->plat_data->support_wireless_tx)
		feature |= INPUT_FEATURE_SUPPORT_WIRELESS_TX;

	input_info(true, &ts->client->dev, "%s: %d%s%s%s%s%s%s%s\n",
			__func__, feature,
			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "",
			feature & INPUT_FEATURE_ENABLE_PRESSURE ? " pressure" : "",
			feature & INPUT_FEATURE_ENABLE_SYNC_RR120 ? " RR120hz" : "",
			feature & INPUT_FEATURE_ENABLE_VRR ? " vrr" : "",
			feature & INPUT_FEATURE_SUPPORT_OPEN_SHORT_TEST ? " openshort" : "",
			feature & INPUT_FEATURE_SUPPORT_MIS_CALIBRATION_TEST ? " miscal" : "",
			feature & INPUT_FEATURE_SUPPORT_WIRELESS_TX ? " wirelesstx" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t melfas_ts_fod_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	u8 data[255] = { SEC_TS_CMD_SPONGE_FOD_POSITION, };
	char buff[3] = { 0 };
	int i, ret;

	if (!ts->plat_data->support_fod) {
		input_err(true, &ts->client->dev, "%s: fod is not supported\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	if (!ts->plat_data->fod_data.vi_size) {
		input_err(true, &ts->client->dev, "%s: not read fod_info yet\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "NG");
	}

	ret = ts->melfas_ts_read_sponge(ts, data, ts->plat_data->fod_data.vi_size);
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

static ssize_t melfas_ts_fod_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	return sec_input_get_fod_info(&ts->client->dev, buf);
}

/** for protos **/
static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	input_info(true, &ts->client->dev, "%s: %d\n", __func__,
			ts->hover_event);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->hover_event != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	u8 data;
	int ret;
	u8 wbuf[3];

	ret = kstrtou8(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, data);

	if (data != 0 && data != 1) {
		input_err(true, &ts->client->dev, "%s: incorrect data\n", __func__);
		return -EINVAL;
	}

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_PROXIMITY;
	wbuf[2] = data;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed to set ed_enable\n", __func__);

	return count;
}

static ssize_t get_lp_dump_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	u8 string_data[10] = {0, };
	u16 current_index;
	u16 dump_start, dump_end, dump_cnt;
	int i, ret, dump_area, dump_gain;
	unsigned char *sec_spg_dat;
	u8 dump_clear_packet[3] = {0x01, 0x00, 0x01};

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "TSP turned off");
	}

	if (ts->reset_is_on_going) {
		input_err(true, &ts->client->dev, "%s: Reset is ongoing!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "Reset is ongoing");
	}

	/* preparing dump buffer */
	sec_spg_dat = vmalloc(SEC_TS_MAX_SPONGE_DUMP_BUFFER);
	if (!sec_spg_dat) {
		input_err(true, &ts->client->dev, "%s : Failed!!\n", __func__);
		return snprintf(buf, SEC_CMD_BUF_SIZE, "vmalloc failed");
	}
	memset(sec_spg_dat, 0, SEC_TS_MAX_SPONGE_DUMP_BUFFER);

	disable_irq(ts->client->irq);

	string_data[0] = SEC_TS_CMD_SPONGE_LP_DUMP_CUR_IDX;
	string_data[1] = 0;

	ret = ts->melfas_ts_read_sponge(ts, string_data, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to read rect\n", __func__);
		snprintf(buf, SEC_CMD_BUF_SIZE, "NG, Failed to read rect");
		goto out;
	}

	if (ts->sponge_inf_dump)
		dump_gain = 2;
	else
		dump_gain = 1;

	current_index = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
	dump_start = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT;
	dump_end = dump_start + (ts->sponge_dump_format * ((ts->sponge_dump_event * dump_gain) - 1));

	if (current_index > dump_end || current_index < dump_start) {
		input_err(true, &ts->client->dev,
				"Failed to Sponge LP log %d\n", current_index);
		snprintf(buf, SEC_CMD_BUF_SIZE,
				"NG, Failed to Sponge LP log, current_index=%d",
				current_index);
		goto out;
	}

	/* legacy get_lp_dump */
	input_info(true, &ts->client->dev, "%s: DEBUG format=%d, num=%d, start=%d, end=%d, current_index=%d\n",
			__func__, ts->sponge_dump_format, ts->sponge_dump_event, dump_start, dump_end, current_index);

	for (i = (ts->sponge_dump_event * dump_gain) - 1 ; i >= 0 ; i--) {
		u16 data0, data1, data2, data3, data4;
		char buff[30] = {0, };
		u16 string_addr;

		if (current_index < (ts->sponge_dump_format * i))
			string_addr = (ts->sponge_dump_format * ts->sponge_dump_event * dump_gain) + current_index - (ts->sponge_dump_format * i);
		else
			string_addr = current_index - (ts->sponge_dump_format * i);

		if (string_addr < dump_start)
			string_addr += (ts->sponge_dump_format * ts->sponge_dump_event * dump_gain);

		string_data[0] = string_addr & 0xFF;
		string_data[1] = (string_addr & 0xFF00) >> 8;

		ret = ts->melfas_ts_read_sponge(ts, string_data, ts->sponge_dump_format);
		if (ret < 0) {
			input_err(true, &ts->client->dev,
					"%s: Failed to read sponge\n", __func__);
			snprintf(buf, SEC_CMD_BUF_SIZE,
					"NG, Failed to read sponge, addr=%d",
					string_addr);
			goto out;
		}

		data0 = (string_data[1] & 0xFF) << 8 | (string_data[0] & 0xFF);
		data1 = (string_data[3] & 0xFF) << 8 | (string_data[2] & 0xFF);
		data2 = (string_data[5] & 0xFF) << 8 | (string_data[4] & 0xFF);
		data3 = (string_data[7] & 0xFF) << 8 | (string_data[6] & 0xFF);
		data4 = (string_data[9] & 0xFF) << 8 | (string_data[8] & 0xFF);

		if (data0 || data1 || data2 || data3 || data4) {
			if (ts->sponge_dump_format == 10) {
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

	if (ts->sponge_inf_dump) {
		if (current_index >= ts->sponge_dump_border) {
			dump_cnt = ((current_index - (ts->sponge_dump_border)) / ts->sponge_dump_format) + 1;
			dump_area = 1;
			sec_spg_dat[0] = ts->sponge_dump_border_lsb;
			sec_spg_dat[1] = ts->sponge_dump_border_msb;
		} else {
			dump_cnt = ((current_index - SEC_TS_CMD_SPONGE_LP_DUMP_EVENT) / ts->sponge_dump_format) + 1;
			dump_area = 0;
			sec_spg_dat[0] = SEC_TS_CMD_SPONGE_LP_DUMP_EVENT;
			sec_spg_dat[1] = 0;
		}

		ret = ts->melfas_ts_read_sponge(ts, sec_spg_dat, dump_cnt * ts->sponge_dump_format);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to read sponge\n", __func__);
			goto out;
		}

		for (i = 0 ; i <= dump_cnt ; i++) {
			int e_offset = i * ts->sponge_dump_format;
			char ibuff[30] = {0, };
			u16 edata[5];

			edata[0] = (sec_spg_dat[1 + e_offset] & 0xFF) << 8 | (sec_spg_dat[0 + e_offset] & 0xFF);
			edata[1] = (sec_spg_dat[3 + e_offset] & 0xFF) << 8 | (sec_spg_dat[2 + e_offset] & 0xFF);
			edata[2] = (sec_spg_dat[5 + e_offset] & 0xFF) << 8 | (sec_spg_dat[4 + e_offset] & 0xFF);
			edata[3] = (sec_spg_dat[7 + e_offset] & 0xFF) << 8 | (sec_spg_dat[6 + e_offset] & 0xFF);
			edata[4] = (sec_spg_dat[9 + e_offset] & 0xFF) << 8 | (sec_spg_dat[8 + e_offset] & 0xFF);

			if (edata[0] || edata[1] || edata[2] || edata[3] || edata[4]) {
				snprintf(ibuff, sizeof(ibuff), "%03d: %04x%04x%04x%04x%04x\n",
						i + (ts->sponge_dump_event * dump_area),
						edata[0], edata[1], edata[2], edata[3], edata[4]);
#if IS_ENABLED(CONFIG_SEC_DEBUG_TSP_LOG)
				sec_tsp_sponge_log(ibuff);
#endif
			}
		}

		ts->sponge_dump_delayed_flag = false;
		ret = ts->melfas_ts_write_sponge(ts, dump_clear_packet, 3);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: Failed to clear sponge dump\n", __func__);
		}
	}
out:
	vfree(sec_spg_dat);
	enable_irq(ts->client->irq);
	return strlen(buf);

}

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	if (melfas_ts_get_image(ts, MELFAS_TS_IMG_TYPE_5POINT_INTENSITY)) {
		input_err(true, &ts->client->dev, "%s: melfas_ts_get_image fail!\n", __func__);
		return -1;
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, ts->print_buf);

	return snprintf(buf, PAGE_SIZE, "%s\n", ts->print_buf);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{

	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	u8 wbuf[64];
	int ret;
	unsigned long value = 0;

	wbuf[0] = MELFAS_TS_R0_CTRL;

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	input_err(true, &ts->client->dev, "%s: enable:%lu\n", __func__, value);

	if (value == 1) {
		wbuf[1] = MELFAS_TS_R1_CTRL_NP_ACTIVE_MODE;
		wbuf[2] = 1;
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: send sensitivity mode on fail!\n", __func__);
			return ret;
		}

		wbuf[1] = MELFAS_TS_R1_CTRL_5POINT_TEST_MODE;
		wbuf[2] = 1;
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: send sensitivity mode on fail!\n", __func__);
			return ret;
		}
		input_info(true, &ts->client->dev, "%s: enable end\n", __func__);
	} else {
		wbuf[1] = MELFAS_TS_R1_CTRL_NP_ACTIVE_MODE;
		wbuf[2] = 0;
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: send sensitivity mode off fail!\n", __func__);
			return ret;
		}

		wbuf[1] = MELFAS_TS_R1_CTRL_5POINT_TEST_MODE;
		wbuf[2] = 0;
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: send sensitivity mode off fail!\n", __func__);
			return ret;
		}
		input_info(true, &ts->client->dev, "%s: disable end\n", __func__);
	}
	input_info(true, &ts->client->dev, "%s: done\n", __func__);

	return count;
}

static DEVICE_ATTR(get_lp_dump, S_IRUGO, get_lp_dump_show, NULL);
static DEVICE_ATTR(scrub_pos, S_IRUGO, scrub_position_show, NULL);
static DEVICE_ATTR(hw_param, 0664, hardware_param_show, hardware_param_store); /* for bigdata */
static DEVICE_ATTR(sensitivity_mode, S_IRUGO | S_IWUSR | S_IWGRP, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(prox_power_off, 0664, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
static DEVICE_ATTR(fod_pos, 0444, melfas_ts_fod_position_show, NULL);
static DEVICE_ATTR(fod_info, 0444, melfas_ts_fod_info_show, NULL);
static DEVICE_ATTR(virtual_prox, 0664, protos_event_show, protos_event_store);

static struct attribute *cmd_attributes[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_hw_param.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_get_lp_dump.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_fod_pos.attr,
	&dev_attr_fod_info.attr,
	&dev_attr_virtual_prox.attr,
	NULL,
};

static const struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[64] = { 0 };
	int retval = 0;

	sec_cmd_set_default_result(sec);
	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	mutex_lock(&ts->modechange);
	retval = melfas_ts_firmware_update_on_hidden_menu(ts, sec->cmd_param[0]);
	if (retval < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, &ts->client->dev, "%s: failed [%d]\n", __func__, retval);
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, &ts->client->dev, "%s: success [%d]\n", __func__, retval);
	}

	mutex_unlock(&ts->modechange);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff),"ME%02X%02X%02X%02X",
					ts->plat_data->img_version_of_bin[0], ts->plat_data->img_version_of_bin[1],
					ts->plat_data->img_version_of_bin[2], ts->plat_data->img_version_of_bin[3]);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return;
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[16] = { 0 };
	u8 rbuf[16];
	char model[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped!\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (melfas_ts_get_fw_version(ts, rbuf)) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buff, sizeof(buff),"ME%02X%02X%02X%02X",
					rbuf[4], rbuf[5], rbuf[6], rbuf[7]);

	snprintf(model, sizeof(model), "ME%02X%02X",rbuf[4], rbuf[5]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buff, sec->cmd_state);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "MELFAS");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "IC_VENDOR");
	
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);

	return;
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_PANEL_CONN))
		goto EXIT;

	input_info(true, &ts->client->dev, "%s: connection check(%d)\n", __func__, ts->image_buf[0]);

	if (!ts->image_buf[0])
		goto EXIT;
	
	sprintf(buf, "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);

	return;

EXIT:
	sprintf(buf, "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	strncpy(buf, "MSS100", sizeof(buf));
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "IC_NAME");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);

	return;
}

static void get_config_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buf, sizeof(buf), "ME_%02d%02d",
		ts->plat_data->config_version_of_ic[2],
		ts->plat_data->config_version_of_ic[3]);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void get_checksum_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_CHECKSUM_REALTIME;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_err(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	u8 wbuf[5];
	u8 precal[5];
	u8 realtime[5];
	u8 rbuf[8];
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (melfas_ts_get_fw_version(ts, rbuf)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	if (ts->plat_data->img_version_of_ic[2] == 0xFF && ts->plat_data->img_version_of_ic[3] == 0xFF) {
		input_info(true, &ts->client->dev, "%s: fw version fail\n", __func__);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_CHECKSUM_PRECALC;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, precal, 1);
	if (ret < 0) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_CHECKSUM_REALTIME;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, realtime, 1);
	if (ret < 0) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	input_info(true, &ts->client->dev, "%s: checksum1:%02X, checksum2:%02X\n",
		__func__, precal[0], realtime[0]);

	if (precal[0] == realtime[0]) {
		snprintf(buf, sizeof(buf), "%s", "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

EXIT:
	input_err(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_NODE_NUM_X;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_NODE_NUM_Y;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = rbuf[0];

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void get_max_x(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_RESOLUTION_X;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 2);
	if (ret < 0) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (rbuf[0]) | (rbuf[1] << 8);

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void get_max_y(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	u8 rbuf[64];
	u8 wbuf[64];
	int val;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_RESOLUTION_Y;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 2);
	if (ret < 0) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	val = (rbuf[0]) | (rbuf[1] << 8);

	snprintf(buf, sizeof(buf), "%d", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void module_off_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	ts->plat_data->stop_device(ts);

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void module_on_master(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	ts->plat_data->start_device(ts);

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_dbg(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void run_test_cm(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d,%d", ts->test_min, ts->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM");
}

static void run_test_cm_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM)) {
		input_err(true, &ts->client->dev, "%s: failed to cm read\n", __func__);
		sprintf(ts->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
}

static void run_test_cm_h_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_DIFF_HOR)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d,%d", ts->test_min, ts->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM_H_GAP");
}

static void run_test_cm_h_gap_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_DIFF_HOR);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(ts->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
}

static void run_test_cm_v_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_DIFF_VER)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d,%d", ts->test_min, ts->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM_V_GAP");
}
static void run_test_cm_v_gap_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_DIFF_VER);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(ts->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
}

static void run_test_cm_jitter(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_JITTER)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d,%d", ts->test_min, ts->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CM_JITTER");
}

static void run_test_cm_jitter_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CM_JITTER);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(ts->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
}

static void run_jitter_delta_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	u8 wbuf[4] = {0, };
	u8 rbuf[4] = {0, };
	char data[SEC_CMD_STR_LEN] = {0};
	int ret = 0;
	int retry = 0;
	const int retry_cnt = 500;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_DELTA_TEST;
	rbuf[0] = 1;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		sprintf(data, "%s", "NG");
		goto out;
	}

	sec_delay(10);

	while (retry < retry_cnt) {
		sec_delay(20);

		melfas_ts_get_image(ts, MELFAS_TS_IMG_TYPE_JITTER_DELTA);

		if (!(ts->image_buf[0] == 0x7fff && ts->image_buf[1] == 0x7fff
			&& ts->image_buf[2] == 0x7fff && ts->image_buf[3] == 0x7fff
			&& ts->image_buf[4] == 0x7fff && ts->image_buf[5] == 0x7fff))
			break;
	}

	input_err(true, &ts->client->dev, "%s: %02X,%02X,%02X,%02X,%02X,%02X)\n",
				__func__, ts->image_buf[0], ts->image_buf[1], ts->image_buf[2], ts->image_buf[3], ts->image_buf[4], ts->image_buf[5]);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_DELTA_TEST;
	rbuf[0] = 0;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		sprintf(data, "%s", "NG");
		goto out;
	}

out:
	if (ret < 0)
		snprintf(data, sizeof(data), "NG");
	else
		snprintf(data, sizeof(data), "%d,%d,%d,%d,%d,%d", ts->image_buf[0], ts->image_buf[1], ts->image_buf[2], ts->image_buf[3], ts->image_buf[4], ts->image_buf[5]);

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		char buffer[SEC_CMD_STR_LEN] = { 0 };

		snprintf(buffer, sizeof(buffer), "%d,%d", ts->image_buf[0], ts->image_buf[1]);
		sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_MIN");

		memset(buffer, 0x00, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%d,%d", ts->image_buf[2], ts->image_buf[3]);
		sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_MAX");

		memset(buffer, 0x00, sizeof(buffer));
		snprintf(buffer, sizeof(buffer), "%d,%d", ts->image_buf[4], ts->image_buf[5]);
		sec_cmd_set_cmd_result_all(sec, buffer, strnlen(buffer, sizeof(buffer)), "JITTER_DELTA_AVG");
	}
	sec_cmd_set_cmd_result(sec, data, strnlen(data, sizeof(data)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, data);
}

static void run_test_cp(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	int i;
	int tx_min, rx_min;
	int tx_max, rx_max;

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CP)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	for (i = 0; i < ts->plat_data->y_node_num; i++) {
		if (i == 0)
			rx_min = rx_max = ts->image_buf[i];

		rx_min = min(rx_min, ts->image_buf[i]);
		rx_max = max(rx_max, ts->image_buf[i]);
	}

	for (i = ts->plat_data->y_node_num; i < (ts->plat_data->x_node_num + ts->plat_data->y_node_num); i++) {
		if (i == ts->plat_data->y_node_num)
			tx_min = tx_max = ts->image_buf[i];

		tx_min = min(tx_min, ts->image_buf[i]);
		tx_max = max(tx_max, ts->image_buf[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", ts->test_min, ts->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buf, sizeof(buf), "%d,%d", tx_min, tx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_TX");
		snprintf(buf, sizeof(buf), "%d,%d", rx_min, rx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_RX");
	}

	return;

ERROR:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_TX");
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_RX");
	}
}
static void run_test_cp_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CP);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(ts->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
}

static void run_test_cp_short(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	int i;
	int tx_min, rx_min;
	int tx_max, rx_max;

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CP_SHORT)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	for (i = 0; i < ts->plat_data->y_node_num; i++) {
		if (i == 0)
			rx_min = rx_max = ts->image_buf[i];

		rx_min = min(rx_min, ts->image_buf[i]);
		rx_max = max(rx_max, ts->image_buf[i]);
	}

	for (i = ts->plat_data->y_node_num; i < (ts->plat_data->x_node_num + ts->plat_data->y_node_num); i++) {
		if (i == ts->plat_data->y_node_num)
			tx_min = tx_max = ts->image_buf[i];

		tx_min = min(tx_min, ts->image_buf[i]);
		tx_max = max(tx_max, ts->image_buf[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", ts->test_min, ts->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buf, sizeof(buf), "%d,%d", tx_min, tx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_SHORT_TX");
		snprintf(buf, sizeof(buf), "%d,%d", rx_min, rx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_SHORT_RX");
	}

	return;

ERROR:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_SHORT_TX");
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_SHORT_RX");
	}
}
static void run_test_cp_short_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CP_SHORT);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(ts->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
}

static void run_test_cp_lpm(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	int i;
	int tx_min, rx_min;
	int tx_max, rx_max;

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CP_LPM)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto ERROR;
	}

	for (i = 0; i < ts->plat_data->y_node_num; i++) {
		if (i == 0)
			rx_min = rx_max = ts->image_buf[i];

		rx_min = min(rx_min, ts->image_buf[i]);
		rx_max = max(rx_max, ts->image_buf[i]);
	}

	for (i = ts->plat_data->y_node_num; i < (ts->plat_data->x_node_num + ts->plat_data->y_node_num); i++) {
		if (i == ts->plat_data->y_node_num)
			tx_min = tx_max = ts->image_buf[i];

		tx_min = min(tx_min, ts->image_buf[i]);
		tx_max = max(tx_max, ts->image_buf[i]);
	}

	snprintf(buf, sizeof(buf), "%d,%d", ts->test_min, ts->test_max);
	sec->cmd_state = SEC_CMD_STATUS_OK;

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buf, sizeof(buf), "%d,%d", tx_min, tx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_LPM_TX");
		snprintf(buf, sizeof(buf), "%d,%d", rx_min, rx_max);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_LPM_RX");
	}

	return;

ERROR:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_LPM_TX");
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "CP_LPM_RX");
	}
}

static void run_test_cp_lpm_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	int ret;

	sec_cmd_set_default_result(sec);

	ret = melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_CP_LPM);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		sprintf(ts->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
}

static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	u8 wbuf[4] = {0, };
	u8 rbuf[4] = {0, };
	char data[SEC_CMD_STR_LEN] = {0};
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (melfas_ts_get_image(ts, MELFAS_TS_IMG_TYPE_PROX_INTENSITY)) {
		input_err(true, &ts->client->dev, "%s: failed to read\n", __func__);
		sprintf(data, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_PROXIMITY_THD;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		sprintf(data, "%s", "NG");
		goto out;
	}

	snprintf(data, sizeof(data), "SUM_X:%d THD_X:%d", ts->image_buf[3], rbuf[0]);
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, data, strlen(data));
}

static void run_cs_delta_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	sec_cmd_set_default_result(sec);

	if (melfas_ts_get_image(ts, MELFAS_TS_IMG_TYPE_INTENSITY)) {
		input_err(true, &ts->client->dev, "%s: failed to read\n", __func__);
		sprintf(ts->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
}

static void run_snr_non_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 wbuf[2] = {0, };
	u8 rbuf[2] = {0, };
	u8 status;
	int ret = 0;
	int wait_time = 0;
	int retry_cnt = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		ret = -1;
		goto outinit;
	}

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		input_err(true, &ts->client->dev, "%s: strange value frame:%d\n",
				__func__, sec->cmd_param[0]);
		ret = -1;
		goto outinit;
	}

	/* input snr frame */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_FRAME;
	rbuf[0] = (u8)(sec->cmd_param[0] & 0xFF);
	rbuf[1] = (u8)((sec->cmd_param[0] >> 8) & 0xFF);
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c_write failed\n", __func__);
		goto out;
	}

	/* start Non-touched test */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_NONE_TOUCH_EN;
	rbuf[0] = 1;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c_write failed\n", __func__);
		goto out;
	}

	wait_time = (sec->cmd_param[0] * 1000) / 120 + (sec->cmd_param[0] * 1000) % 120;

	sec_delay(wait_time + 2000);

	retry_cnt = 50;
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_STATUS;
	while ((ts->melfas_ts_i2c_read(ts, wbuf, 2, &status, 1) > 0) && (retry_cnt-- > 0)) {
		if (status == 1)
			break;
		sec_delay(20);
	}

	if (status == 0) {
		input_err(true, &ts->client->dev, "%s: failed non-touched status:%d\n", __func__, status);
		ret = -1;
	}
out:
		/* EXIT SNR mode */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_NONE_TOUCH_EN;
	rbuf[0] = 0;
	ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 1);

	/* input snr frame */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_FRAME;
	rbuf[0] = 0;
	rbuf[1] = 0;
	ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 2);

outinit:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void run_snr_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	struct melfas_tsp_snr_result_of_point result[9];
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char tbuff[SEC_CMD_STR_LEN] = { 0 };
	u8 wbuf[2] = {0, };
	u8 rbuf[2] = {0, };
	u8 status;
	int ret = 0;
	int wait_time = 0;
	int retry_cnt = 0;
	int i = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		ret = -1;
		goto outinit;
	}

	if (sec->cmd_param[0] < 1 || sec->cmd_param[0] > 1000) {
		input_err(true, &ts->client->dev, "%s: strange value frame:%d\n",
				__func__, sec->cmd_param[0]);
		ret = -1;
		goto outinit;
	}

	/* input snr frame */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_FRAME;
	rbuf[0] = (u8)(sec->cmd_param[0] & 0xFF);
	rbuf[1] = (u8)((sec->cmd_param[0] >> 8) & 0xFF);
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 2);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c_write failed\n", __func__);
		goto out;
	}

	/* start touched test */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_TOUCH_EN;
	rbuf[0] = 1;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: i2c_write failed\n", __func__);
		goto out;
	}

	wait_time = (sec->cmd_param[0] * 1000) / 120 + (sec->cmd_param[0] * 1000) % 120;

	sec_delay(wait_time + 2000);

	retry_cnt = 50;
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_STATUS;
	while ((ts->melfas_ts_i2c_read(ts, wbuf, 2, &status, 1) > 0) && (retry_cnt-- > 0)) {
		if (status == 1)
			break;
		sec_delay(20);
	}

	if (status == 0) {
		input_err(true, &ts->client->dev, "%s: snr_touched status:%d\n", __func__, status);
		ret = -1;
		goto out;
	}

	melfas_ts_get_image(ts, MELFAS_TS_IMG_TYPE_SNR_DATA);

	for (i = 0; i < 9; i++) {
		result[i].max = ts->image_buf[i * 7];
		result[i].min = ts->image_buf[i * 7 + 1];
		result[i].average = ts->image_buf[i * 7 + 2];
		result[i].nontouch_peak_noise = ts->image_buf[i * 7 + 3];
		result[i].touch_peak_noise = ts->image_buf[i * 7 + 4];
		result[i].snr1 = ts->image_buf[i * 7 + 5];
		result[i].snr2 = ts->image_buf[i * 7 + 6];
		input_info(true, &ts->client->dev, "%s: average:%d, snr1:%d, snr2:%d\n", __func__,
			result[i].average, result[i].snr1, result[i].snr2);
		snprintf(tbuff, sizeof(tbuff), "%d,%d,%d,", result[i].average, result[i].snr1, result[i].snr2);
		strlcat(buff, tbuff, sizeof(buff));
	}
out:
		/* EXIT SNR mode */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_TOUCH_EN;
	rbuf[0] = 0;
	ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 1);

	/* EXIT snr frame */
	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_SNR_TEST_FRAME;
	rbuf[0] = 0;
	rbuf[1] = 0;
	ts->melfas_ts_i2c_write(ts, wbuf, 2, rbuf, 2);

outinit:

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void check_gpio(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	int value_low;
	int value_high;

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_GPIO_LOW)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto error;
	}
	value_low = ts->image_buf[0];

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_GPIO_HIGH)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto error;
	}

	value_high = ts->image_buf[0];

	if ((value_high == 1) && (value_low == 0)) {
		snprintf(buf, sizeof(buf), "%d", value_low);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

error:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "GPIO_CHECK");
}

static void check_wet_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	u8 wbuf[4] = {0, };
	u8 rbuf[4] = {0, };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_WET_MODE;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d", rbuf[0]);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:	
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "WET_MODE");
}

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };
	u8 rbuf[4];
	u8 wbuf[4];
	int ret = 0;

	sec_cmd_set_default_result(sec);

	wbuf[0] = MELFAS_TS_R0_INFO;
	wbuf[1] = MELFAS_TS_R1_INFO_CONTACT_THD_SCR;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 1);
	if (ret < 0) {
		sprintf(ts->print_buf, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	sprintf(buf, "%d", rbuf[0]);
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_err(true, &ts->client->dev, "%s - cmd[%s] state[%d]\n",
		__func__, buf, sec->cmd_state);
}

static void run_test_vsync(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buf[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_VSYNC)) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto EXIT;
	}

	snprintf(buf, sizeof(buf), "%d", ts->image_buf[0]);
	sec->cmd_state = SEC_CMD_STATUS_OK;

EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "VSYNC");
}

static void gap_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	char buff[16] = { 0 };
	int ii;
	int16_t node_gap_h = 0;
	int16_t node_gap_v = 0;
	int16_t h_max = 0;
	int16_t v_max = 0;
	int16_t h_min = 999;
	int16_t v_min = 999;
	
	int numerator_v;
	int denominator_v;

	int numerator_h;
	int denominator_h;

	for (ii = 0; ii < (ts->plat_data->x_node_num * ts->plat_data->y_node_num); ii++) {
		if ((ii + 1) % (ts->plat_data->y_node_num) != 0) {
			numerator_h = (int)(ts->image_buf[ii + 1] - ts->image_buf[ii]);
			denominator_h = (int)((ts->image_buf[ii + 1] + ts->image_buf[ii]) >> 1);

			if (denominator_h > 1)
				node_gap_h = (numerator_h * DIFF_SCALER) / denominator_h;
			else
				node_gap_h = (numerator_h * DIFF_SCALER) / 1;

			h_max = max(h_max, node_gap_h);
			h_min = min(h_min, node_gap_h);
		}

		if (ii < (ts->plat_data->x_node_num - 1) * ts->plat_data->y_node_num) {
			numerator_v = (int)(ts->image_buf[ii + ts->plat_data->y_node_num] - ts->image_buf[ii]) ;
			denominator_v = (int)((ts->image_buf[ii + ts->plat_data->y_node_num] + ts->image_buf[ii]) >> 1);

			if (denominator_v > 1)
				node_gap_v = (numerator_v * DIFF_SCALER) / denominator_v;
			else
				node_gap_v = (numerator_v * DIFF_SCALER) / 1;

			v_max = max(v_max, node_gap_v);
			v_min = min(v_min, node_gap_v);
		}
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", h_min, h_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "CM_H_GAP");
		snprintf(buff, sizeof(buff), "%d,%d", v_min, v_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "CM_V_GAP");
	}
}

static void gap_data_h_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char *buff = NULL;
	int ii;
	int16_t node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	int numerator;
	int denominator;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->plat_data->x_node_num * ts->plat_data->y_node_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, sizeof(temp), "%s", "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (ii = 0; ii < (ts->plat_data->x_node_num * ts->plat_data->y_node_num); ii++) {
		if ((ii + 1) % (ts->plat_data->y_node_num) != 0) {
			numerator = (int)(ts->image_buf[ii + 1] - ts->image_buf[ii]);
			denominator = (int)((ts->image_buf[ii + 1] + ts->image_buf[ii]) >> 1);

			if (denominator > 1)
				node_gap = (numerator * DIFF_SCALER) / denominator;
			else
				node_gap = (numerator * DIFF_SCALER) / 1;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->plat_data->x_node_num * ts->plat_data->y_node_num * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->plat_data->x_node_num * ts->plat_data->y_node_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void gap_data_v_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char *buff = NULL;
	int ii;
	int16_t node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	int numerator;
	int denominator;

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->plat_data->x_node_num * ts->plat_data->y_node_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		snprintf(temp, sizeof(temp), "%s", "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	for (ii = 0; ii < (ts->plat_data->x_node_num * ts->plat_data->y_node_num); ii++) {
		if (ii < (ts->plat_data->x_node_num - 1) * ts->plat_data->y_node_num) {
			numerator = (int)(ts->image_buf[ii + ts->plat_data->y_node_num] - ts->image_buf[ii]) ;
			denominator = (int)((ts->image_buf[ii + ts->plat_data->y_node_num] + ts->image_buf[ii]) >> 1);
			
			if (denominator > 1)
				node_gap = (numerator * DIFF_SCALER) / denominator;
			else
				node_gap = (numerator * DIFF_SCALER) / 1;

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->plat_data->x_node_num * ts->plat_data->y_node_num * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->plat_data->x_node_num * ts->plat_data->y_node_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_trx_short_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char test[32];
	char result[32];
	u8 wbuf[4] = {0, };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	memset(test, 0x00, sizeof(test));
	memset(result, 0x00, sizeof(result));
	if (sec->cmd_param[1])
		snprintf(test, sizeof(test), "TEST=%d,%d", sec->cmd_param[0], sec->cmd_param[1]);
	else
		snprintf(test, sizeof(test), "TEST=%d", sec->cmd_param[0]);

	if (sec->cmd_param[0] == OPEN_SHORT_TEST && sec->cmd_param[1] == 0) {
		input_err(true, &ts->client->dev,
				"%s: seperate cm1 test open / short test result\n", __func__);

		snprintf(ts->print_buf, sizeof(ts->print_buf), "%s", "CONT");
		sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(sec, test, result);
		return;
	}

	if (sec->cmd_param[0] == OPEN_SHORT_TEST &&
		sec->cmd_param[1] == CHECK_ONLY_OPEN_TEST) {
		ts->open_short_type = CHECK_ONLY_OPEN_TEST;

	} else if (sec->cmd_param[0] == OPEN_SHORT_TEST &&
		sec->cmd_param[1] == CHECK_ONLY_SHORT_TEST) {
		ts->open_short_type = CHECK_ONLY_SHORT_TEST;
	} else {
		input_err(true, &ts->client->dev, "%s: not support\n", __func__);
		snprintf(ts->print_buf, PAGE_SIZE, "%s", "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(sec, test, result);
		sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
		return;
	}

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_ASYNC;
	wbuf[2] = 0;
	ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s failed to write async cmd\n", __func__);
		snprintf(ts->print_buf, PAGE_SIZE, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(sec, test, result);
		sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
		return;
	} else {
		input_info(true, &ts->client->dev, "%s success to write async cmd\n", __func__);
	}

	if (melfas_ts_run_test(ts, MELFAS_TS_TEST_TYPE_OPEN_SHORT)) {
		input_err(true, &ts->client->dev, "%s: failed to read open short\n", __func__);
		snprintf(ts->print_buf, PAGE_SIZE, "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(sec, test, result);
		sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
		return;
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

	if (ts->open_short_result == 1) {
		snprintf(result, sizeof(result), "RESULT=PASS");
		sec_cmd_send_event_to_user(sec, test, result);
	} else {
		snprintf(result, sizeof(result), "RESULT=FAIL");
		sec_cmd_send_event_to_user(sec, test, result);
	}

	sec_cmd_set_cmd_result(sec, ts->print_buf, strlen(ts->print_buf));
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[64] = { 0 };
	int enable = sec->cmd_param[0];
	u8 wbuf[4];
	int status;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev, "%s %d\n", __func__, enable);

	if (enable)
		status = 0;
	else
		status = 2;

	wbuf[0] = MELFAS_TS_R0_CTRL;
	wbuf[1] = MELFAS_TS_R1_CTRL_DISABLE_EDGE_EXPAND;
	wbuf[2] = status;

	if ((enable == 0) || (enable == 1)) {
		ret = ts->melfas_ts_i2c_write(ts, wbuf, 2, &wbuf[2], 1);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s melfas_ts_i2c_write\n", __func__);
			goto out;
		} else
			input_info(true, &ts->client->dev, "%s - value[%d]\n", __func__, wbuf[2]);
	} else {
		input_err(true, &ts->client->dev, "%s [ERROR] Unknown value[%d]\n", __func__, status);
		goto out;
	}
	input_dbg(true, &ts->client->dev, "%s [DONE]\n", __func__);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		ts->glove_enable = sec->cmd_param[0];

		if (ts->plat_data->power_state != SEC_INPUT_STATE_POWER_OFF) {
			if (ts->glove_enable)
				ts->plat_data->touch_functions = ts->plat_data->touch_functions | MELFAS_TS_BIT_SETFUNC_GLOVE;
			else
				ts->plat_data->touch_functions = (ts->plat_data->touch_functions & (~MELFAS_TS_BIT_SETFUNC_GLOVE));
		}

		ret = melfas_ts_set_glove_mode(ts, ts->glove_enable);
		if (ret < 0)
			goto out;
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);
}

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, &ts->client->dev, "%s: start clear_cover_mode %s\n", __func__, buff);
	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (sec->cmd_param[0] > 1) {
			ts->plat_data->cover_type = sec->cmd_param[1];
			melfas_ts_set_cover_type(ts, true);
		} else {
			melfas_ts_set_cover_type(ts, false);
		}

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};

static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = G_NONE;

	sec_cmd_set_default_result(sec);

	memset(buff, 0, sizeof(buff));

	mutex_lock(&ts->device_mutex);

	if (sec->cmd_param[0] == 0) {	// edge handler
		if (sec->cmd_param[1] == 0) {	// clear
			ts->plat_data->grip_data.edgehandler_direction = 0;
		} else if (sec->cmd_param[1] < 3) {
			ts->plat_data->grip_data.edgehandler_direction = sec->cmd_param[1];
			ts->plat_data->grip_data.edgehandler_start_y = sec->cmd_param[2];
			ts->plat_data->grip_data.edgehandler_end_y = sec->cmd_param[3];
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}

		mode = mode | G_SET_EDGE_HANDLER;
		melfas_set_grip_data_to_ic(&ts->client->dev, mode);

	} else if (sec->cmd_param[0] == 1) {	// normal mode
		if (ts->plat_data->grip_data.edge_range != sec->cmd_param[1])
			mode = mode | G_SET_EDGE_ZONE;

		ts->plat_data->grip_data.edge_range = sec->cmd_param[1];
		ts->plat_data->grip_data.deadzone_up_x = sec->cmd_param[2];
		ts->plat_data->grip_data.deadzone_dn_x = sec->cmd_param[3];
		ts->plat_data->grip_data.deadzone_y = sec->cmd_param[4];
		mode = mode | G_SET_NORMAL_MODE;

		if (ts->plat_data->grip_data.landscape_mode == 1) {
			ts->plat_data->grip_data.landscape_mode = 0;
			mode = mode | G_CLR_LANDSCAPE_MODE;
		}
		melfas_set_grip_data_to_ic(&ts->client->dev, mode);
	} else if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// normal mode
			ts->plat_data->grip_data.landscape_mode = 0; // need to add melfas
			mode = mode | G_CLR_LANDSCAPE_MODE;
		} else if (sec->cmd_param[1] == 1) {
			ts->plat_data->grip_data.landscape_mode = 1;
			ts->plat_data->grip_data.landscape_edge = sec->cmd_param[2];
			ts->plat_data->grip_data.landscape_deadzone	= sec->cmd_param[3];
			ts->plat_data->grip_data.landscape_top_deadzone = sec->cmd_param[4];
			ts->plat_data->grip_data.landscape_bottom_deadzone = sec->cmd_param[5];
			ts->plat_data->grip_data.landscape_top_gripzone = sec->cmd_param[6];
			ts->plat_data->grip_data.landscape_bottom_gripzone = sec->cmd_param[7];
			mode = mode | G_SET_LANDSCAPE_MODE;
		} else {
			input_err(true, &ts->client->dev, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
		melfas_set_grip_data_to_ic(&ts->client->dev, mode);
	} else {
		input_err(true, &ts->client->dev, "%s: cmd0 is abnormal, %d", __func__, sec->cmd_param[0]);
		goto err_grip_data;
	}

	mutex_unlock(&ts->device_mutex);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

err_grip_data:
	mutex_unlock(&ts->device_mutex);

	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void pocket_mode_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!ts->plat_data->support_ear_detect || sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		ts->plat_data->pocket_mode = sec->cmd_param[0];

		ret = melfas_ts_pocket_mode_enable(ts, ts->plat_data->pocket_mode);
		if (ret < 0) {
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		}
		snprintf(buff, sizeof(buff), "OK");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!ts->plat_data->support_ear_detect || sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		ts->plat_data->ed_enable = sec->cmd_param[0];
		ret = melfas_ts_ear_detect_enable(ts, ts->plat_data->ed_enable);
		if (ret < 0) {
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		}
		snprintf(buff, sizeof(buff), "OK");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void fod_lp_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);

	ts->fod_lp_mode = sec->cmd_param[0];

	input_info(true, &ts->client->dev, "%s: fod_lp_mode %d\n", __func__, ts->fod_lp_mode);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
}

static void fod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	} else if (!ts->plat_data->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_PRESS;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_PRESS;

	ts->plat_data->fod_data.press_prop = (sec->cmd_param[1] & 0x01) | ((sec->cmd_param[2] & 0x01) << 1);

	input_info(true, &ts->client->dev, "%s: %s, fast:%s, strict:%s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off",
			ts->plat_data->fod_data.press_prop & 1 ? "on" : "off",
			ts->plat_data->fod_data.press_prop & 2 ? "on" : "off",
			ts->plat_data->lowpower_mode);

	mutex_lock(&ts->modechange);
	if (!ts->plat_data->enabled && !ts->plat_data->lowpower_mode && !ts->plat_data->ed_enable && !ts->plat_data->fod_lp_mode) {
		if (device_may_wakeup(&ts->client->dev) && ts->plat_data->power_state == SEC_INPUT_STATE_LPM)
			disable_irq_wake(ts->client->irq);
		ts->plat_data->stop_device(ts);
	} else {
		melfas_ts_set_custom_library(ts);
		melfas_ts_set_press_property(ts);
	}
	mutex_unlock(&ts->modechange);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

}

static void set_fod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	input_info(true, &ts->client->dev, "%s: l:%d, t:%d, r:%d, b:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	if (!ts->plat_data->support_fod) {
		snprintf(buff, sizeof(buff), "NA");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (!sec_input_set_fod_rect(&ts->client->dev, sec->cmd_param))
		goto NG;

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: Touch is stopped! Set data at reinit()\n", __func__);
		goto OK;
	}

	ret = melfas_ts_set_fod_rect(ts);
	if (ret < 0)
		goto NG;

OK:
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

NG:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	melfas_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_SWIPE;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SWIPE;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	melfas_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_SINGLE_TAP;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SINGLE_TAP;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	melfas_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec_cmd_set_cmd_exit(sec);
		return;
	}

	if (sec->cmd_param[0])
		ts->plat_data->lowpower_mode |= SEC_TS_MODE_SPONGE_AOD;
	else
		ts->plat_data->lowpower_mode &= ~SEC_TS_MODE_SPONGE_AOD;

	input_info(true, &ts->client->dev, "%s: %s, %02X\n",
			__func__, sec->cmd_param[0] ? "on" : "off", ts->plat_data->lowpower_mode);

	melfas_ts_set_custom_library(ts);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[64] = { 0 };
	int i;
	int ret;

	sec_cmd_set_default_result(sec);

	disable_irq(ts->client->irq);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev,
			  "%s: [ERROR] Touch is stopped\n", __func__);
		snprintf(buff, sizeof(buff), "%s", "TSP turned off");
		goto out;
	}

	input_info(true, &ts->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, sec->cmd_param[0], sec->cmd_param[1],
			sec->cmd_param[2], sec->cmd_param[3]);

	for (i = 0; i < 4; i++)
		ts->plat_data->aod_data.rect_data[i] = sec->cmd_param[i];

	ret = melfas_ts_set_aod_rect(ts);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail set aod rect \n", __func__);
		goto out;
	}

	enable_irq(ts->client->irq);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;

out:
	enable_irq(ts->client->irq);

	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);	
}


static void get_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[64] = { 0 };
	u8 wbuf[16];
	u8 rbuf[16];
	u16 rect_data[4] = {0, };
	int i;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	disable_irq(ts->client->irq);

	wbuf[0] = MELFAS_TS_R0_AOT;
	wbuf[1] = MELFAS_TS_R0_AOT_BOX_W;
	ret = ts->melfas_ts_i2c_read(ts, wbuf, 2, rbuf, 8);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s melfas_ts_i2c_write\n", __func__);
		goto out;
	}

	enable_irq(ts->client->irq);

	for (i = 0; i < 4; i++)
		rect_data[i] = (rbuf[i * 2 + 1] & 0xFF) << 8 | (rbuf[i * 2] & 0xFF);

	input_info(true, &ts->client->dev, "%s: w:%d, h:%d, x:%d, y:%d\n",
			__func__, rect_data[0], rect_data[1], rect_data[2], rect_data[3]);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);
	return;
out:
	enable_irq(ts->client->irq);
	
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);	
}

/**
 * Command : Unknown cmd
 */
static void unknown_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: \"%s\"\n", __func__, buff);
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: IC is power off\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	run_test_cm(sec);

	gap_data(sec);

	run_test_cm_jitter(sec);
	run_test_cp(sec);
	run_test_cp_short(sec);
	run_test_cp_lpm(sec);
	check_gpio(sec);
	check_wet_mode(sec);

	run_test_vsync(sec);

	disable_irq(ts->client->irq);
	melfas_ts_reset(ts, 10);
	enable_irq(ts->client->irq);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &ts->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void factory_cmd_result_all_imagetest(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	if (ts->plat_data->power_state == SEC_INPUT_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: [ERROR] Touch is stopped\n",
				__func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}


	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	run_jitter_delta_test(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

out:
	input_info(true, &ts->client->dev, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct melfas_ts_data *ts = container_of(sec, struct melfas_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	ts->debug_flag = sec->cmd_param[0];

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

/**
 * List of command functions
 */
static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("get_crc_check", get_crc_check),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("get_max_x", get_max_x),},
	{SEC_CMD("get_max_y", get_max_y),},
	{SEC_CMD("module_off_master", module_off_master),},
	{SEC_CMD("module_on_master", module_on_master),},
	{SEC_CMD("run_cm_read", run_test_cm),},
	{SEC_CMD("run_cm_read_all", run_test_cm_all),},
	{SEC_CMD("run_cm_h_gap_read", run_test_cm_h_gap),},
	{SEC_CMD("run_cm_h_gap_read_all", run_test_cm_h_gap_all),},
	{SEC_CMD("run_cm_v_gap_read", run_test_cm_v_gap),},
	{SEC_CMD("run_cm_v_gap_read_all", run_test_cm_v_gap_all),},
	{SEC_CMD("run_cm_jitter_read", run_test_cm_jitter),},
	{SEC_CMD("run_cm_jitter_read_all", run_test_cm_jitter_all),},
	{SEC_CMD("run_cp_read", run_test_cp),},
	{SEC_CMD("run_cp_read_all", run_test_cp_all),},
	{SEC_CMD("run_cp_short_read", run_test_cp_short),},
	{SEC_CMD("run_cp_short_read_all", run_test_cp_short_all),},
	{SEC_CMD("run_cp_lpm_read", run_test_cp_lpm),},
	{SEC_CMD("run_cp_lpm_read_all", run_test_cp_lpm_all),},
	{SEC_CMD("gap_data_h_all", gap_data_h_all),},
	{SEC_CMD("gap_data_v_all", gap_data_v_all),},
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
	{SEC_CMD("run_vsync_read", run_test_vsync),},
	{SEC_CMD("run_cs_delta_read_all", run_cs_delta_read_all),},
	{SEC_CMD("run_snr_non_touched", run_snr_non_touched),},
	{SEC_CMD("run_snr_touched", run_snr_touched),},
	{SEC_CMD("run_jitter_delta_test", run_jitter_delta_test),},
	{SEC_CMD("get_config_ver", get_config_ver),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("check_gpio", check_gpio),},
	{SEC_CMD("wet_mode", check_wet_mode),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD_H("aod_enable", aod_enable),},
	{SEC_CMD_H("singletap_enable", singletap_enable),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("fod_enable", fod_enable),},
	{SEC_CMD_H("fod_lp_mode", fod_lp_mode),},
	{SEC_CMD_H("set_fod_rect", set_fod_rect),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("pocket_mode_enable", pocket_mode_enable),},
	{SEC_CMD("set_aod_rect", set_aod_rect),},
	{SEC_CMD("get_aod_rect", get_aod_rect),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("run_trx_short_test", run_trx_short_test),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("factory_cmd_result_all_imagetest", factory_cmd_result_all_imagetest),},
	{SEC_CMD("debug", debug),},
	{SEC_CMD("not_support_cmd", unknown_cmd),},
};

/**
 * Create sysfs command functions
 */
int melfas_ts_fn_init(struct melfas_ts_data *ts)
{
	int retval = 0;

	retval = sec_cmd_init(&ts->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to sec_cmd_init\n", __func__);
		goto exit;
	}

	retval = sysfs_create_group(&ts->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to create sysfs attributes\n", __func__);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sysfs_create_link(&ts->sec.fac_dev->kobj,
			&ts->plat_data->input_dev->dev.kobj, "input");
	if (retval < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to create input symbolic link\n",
				__func__);
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sec_input_sysfs_create(&ts->plat_data->input_dev->dev.kobj);
	if (retval < 0) {
		sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		input_err(true, &ts->client->dev,
				"%s: Failed to create sec_input_sysfs attributes\n", __func__);
		goto exit;
	}

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	retval = melfas_ts_dev_create(ts);
	if (retval < 0) {
		sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");
		sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
		sec_input_sysfs_remove(&ts->plat_data->input_dev->dev.kobj);
		input_err(true, &ts->client->dev,
				"%s: Failed to create sec_input_sysfs attributes\n", __func__);
		goto exit;
	}
	ts->class = class_create(THIS_MODULE, MELFAS_TS_DEVICE_NAME);
	device_create(ts->class, NULL, ts->melfas_dev, NULL, MELFAS_TS_DEVICE_NAME);
	melfas_ts_sysfs_create(ts);
#endif
	return 0;

exit:
	return retval;	
}

/**
 * Remove sysfs command functions
 */
void melfas_ts_fn_remove(struct melfas_ts_data *ts)
{
	input_err(true, &ts->client->dev, "%s\n", __func__);

	sec_input_sysfs_remove(&ts->plat_data->input_dev->dev.kobj);

	sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");

	sysfs_remove_group(&ts->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&ts->sec, SEC_CLASS_DEVT_TSP);
}
