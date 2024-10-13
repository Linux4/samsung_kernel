/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "focaltech_core.h"

enum ALL_NODE_TEST_TYPE {
	ALL_NODE_TYPE_SHORT,
	ALL_NODE_TYPE_OPEN,
	ALL_NODE_TYPE_CB,
	ALL_NODE_TYPE_RAWDATA,
	ALL_NODE_TYPE_NOISE,
	ALL_NODE_TYPE_NONFALSH_RAM,
};

static int fts_wait_test_done(u8 cmd, u8 mode, int delayms)
{
	int i, ret;
	u8 rbuff;
	u8 read_addr = 0;
	u8 status_done = 0;
	ktime_t start_time = ktime_get();

	switch (cmd) {
	case DEVICE_MODE_ADDR:
		read_addr = cmd;
		if (mode == FTS_REG_WORKMODE)
			status_done = FTS_REG_WORKMODE;
		else
			status_done = FTS_REG_WORKMODE_FACTORY_VALUE;
		break;
	case FACTORY_REG_SHORT_TEST_EN:
		read_addr = FACTORY_REG_SHORT_TEST_STATE;
		status_done = TEST_RETVAL_AA;
		break;
	case FACTORY_REG_OPEN_START:
		read_addr = FACTORY_REG_OPEN_STATE;
		status_done = TEST_RETVAL_AA;
		break;
	case FACTORY_REG_OPEN_START2:
		read_addr = FACTORY_REG_OPEN_STATE2;
		status_done = TEST_RETVAL_AA;
		break;
	case FACTORY_REG_CLB:
		read_addr = FACTORY_REG_CLB;
		status_done = FACTORY_REG_CLB_DONE;
		break;
	case FACTORY_REG_LCD_NOISE_START:
		read_addr = FACTORY_REG_LCD_NOISE_TEST_STATE;
		status_done = TEST_RETVAL_AA;
		break;
	}

	FTS_INFO("write: 0x%02X 0x%02X, need reply: 0x%02X 0x%02X", cmd, mode, read_addr, status_done);

	ret = fts_write_reg(cmd, mode);
	if (ret < 0) {
		FTS_ERROR("failed to write 0x%02X to cmd 0x%02X", mode, cmd);
		return ret;
	}

	sec_delay(delayms);

	for (i = 0; i < FACTORY_TEST_RETRY; i++) {
		ret = fts_read_reg(read_addr, &rbuff);
		if (ret < 0) {
			FTS_ERROR("failed to read 0x%02X", read_addr);
			return ret;
		}

		if (rbuff == status_done) {
			FTS_INFO("read 0x%02X and get 0x%02X success, %lldms taken",
					read_addr, status_done, ktime_ms_delta(ktime_get(), start_time));
			return 0;
		}

		sec_delay(FACTORY_TEST_DELAY);
	}

	FTS_ERROR("[TIMEOUT %lldms] read 0x%02X but failed to get 0x%02X, current:0x%02X",
			ktime_ms_delta(ktime_get(), start_time), read_addr, status_done, rbuff);
	return -EIO;
}

static int enter_work_mode(void)
{
	int ret = 0;
	u8 mode = 0;

	FTS_FUNC_ENTER();

	ret = fts_read_reg(DEVICE_MODE_ADDR, &mode);
	if ((ret >= 0) && (mode == FTS_REG_WORKMODE))
		return 0;

	return fts_wait_test_done(DEVICE_MODE_ADDR, FTS_REG_WORKMODE, 200);
}

static int enter_factory_mode(void)
{
	int ret = 0;
	u8 mode = 0;

	FTS_FUNC_ENTER();

	ret = fts_read_reg(DEVICE_MODE_ADDR, &mode);
	if ((ret >= 0) && (mode == FTS_REG_WORKMODE_FACTORY_VALUE))
		return 0;

	return fts_wait_test_done(DEVICE_MODE_ADDR, FTS_REG_WORKMODE_FACTORY_VALUE, 200);
}

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, i;
	u8 cmd = FTS_REG_SENSITIVITY_VALUE;
	u8 data[18] = { 0 };
	short diff[9] = { 0 };
	char buff[10 * 9] = { 0 };

	ret = fts_write_reg(FTS_REG_SNR_BUFFER_COUNTER, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write FTS_REG_SNR_BUFFER_COUNTER, ret=%d", ret);
	}

	ret = fts_read(&cmd, 1, data, 19);
	if (ret < 0)
		FTS_ERROR("read sensitivity data failed");

	for (i = 0; i < 9; i++)
		diff[i] = (data[2 * i] << 8) + data[2 * i + 1];

	snprintf(buff, sizeof(buff), "%d,%d,%d,%d,%d,%d,%d,%d,%d",
			diff[0], diff[1], diff[2],
			diff[3], diff[4], diff[5],
			diff[6], diff[7], diff[8]);
	FTS_INFO("%s", buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int data;
	int ret;
	u8 enable;

	ret = kstrtoint(buf, 10, &data);
	if (ret < 0)
		return count;

	if (data != 0 && data != 1) {
		FTS_ERROR("invalid param %d", data);
		return count;
	}

	enable = data & 0xFF;

	if (enable) {
		ret = fts_write_reg(FTS_REG_BANK_MODE, 0x00);
		if (ret < 0) {
			FTS_ERROR("failed to write bank mode %d, ret=%d", data, ret);
			goto out;
		}

		ret = fts_write_reg(FTS_REG_POWER_MODE, 0x00);
		if (ret < 0) {
			FTS_ERROR("failed to write power mode 0x00, ret=%d", ret);
			goto out;
		}

		ret = fts_write_reg(FTS_REG_SENSITIVITY_MODE, 0x01);
		if (ret < 0) {
			FTS_ERROR("failed to write sensitivity mode %d, ret=%d", data, ret);
			goto out;
		}
	} else {
		ret = fts_write_reg(FTS_REG_SENSITIVITY_MODE, 0x00);
		if (ret < 0) {
			FTS_ERROR("failed to write sensitivity mode %d, ret=%d", data, ret);
			goto out;
		}

		ret = fts_write_reg(FTS_REG_BANK_MODE, 0x00);
		if (ret < 0) {
			FTS_ERROR("failed to write bank mode %d, ret=%d", data, ret);
			goto out;
		}
	}

out:
	FTS_INFO("%s %s", data ? "on" : "off", (ret < 0) ? "fail" : "success");

	if (ret < 0) {
		ret = fts_write_reg(FTS_REG_BANK_MODE, 0x00);
		if (ret < 0)
			FTS_ERROR("failed to write bank mode %d, ret=%d", data, ret);
	}

	return count;
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);

	FTS_INFO("%d", ts->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	int data;
	int ret;

	ret = kstrtoint(buf, 10, &data);
	if (ret < 0)
		return ret;

	ts->prox_power_off = data;

	FTS_INFO("%d", ts->prox_power_off);

	return count;
}

static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);

	FTS_INFO("%d", ts->hover_event);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->hover_event != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	u8 data;
	int ret;

	ret = kstrtou8(buf, 10, &data);
	if (ret < 0)
		return ret;

	FTS_INFO("%d\n", data);

	if (data != 0 && data != 1) {
		FTS_ERROR("incorrect data");
		return -EINVAL;
	}

	if (ts_data->power_status == POWER_OFF_STATUS) {
		FTS_ERROR("POWER_STATUS : OFF!");
		return count;
	}

	ts_data->proximity_mode = data;

	ret = fts_write_reg(FTS_REG_PROXIMITY_MODE, ts_data->proximity_mode);
	if (ret < 0) {
		FTS_ERROR("send ear detect cmd failed");
	}
	ts_data->hover_event = 0xFF;

	return count;
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	u32 feature = 0;

	if (ts->pdata->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	if (ts->pdata->enable_sysinput_enabled)
		feature |= INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED;

	if (ts->pdata->prox_lp_scan_enabled)
		feature |= INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED;

	FTS_INFO("%d%s%s%s", feature,
			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "",
			feature & INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED ? " SE" : "",
			feature & INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED ? " LPSCAN" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);

	FTS_INFO("power_status : %d", ts_data->power_status);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts_data->power_status);
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	int buff[2];
	int ret;

	if (!ts_data->pdata->enable_sysinput_enabled)
		return count;

	ret = sscanf(buf, "%d,%d", &buff[0], &buff[1]);
	if (ret != 2) {
		FTS_ERROR("failed read params, %d", ret);
		return -EINVAL;
	}

	FTS_INFO("%d %d", buff[0], buff[1]);

	if (buff[0] == DISPLAY_STATE_ON || buff[0] == DISPLAY_STATE_DOZE || buff[0] == DISPLAY_STATE_DOZE_SUSPEND) {
		if (buff[1] == DISPLAY_EVENT_EARLY) {
			if (ts_data->gesture_mode) {
				if (ts_data->lp_dump_enabled) {
					fts_get_lp_dump_data(ts_data);
				}
				fts_ctrl_lcd_reset_regulator(ts_data, false);
			}
		} else if (buff[1] == DISPLAY_EVENT_LATE) {
			queue_work(ts_data->ts_workqueue, &ts_data->resume_work);
		}
	} else if (buff[0] == DISPLAY_STATE_OFF) {
		if (buff[1] == DISPLAY_EVENT_EARLY) {
			cancel_work_sync(&ts_data->resume_work);
			fts_ts_suspend(ts_data->dev);
		} else if (buff[1] == DISPLAY_EVENT_LATE) {
			/* do nothing */
		}
	} else if (buff[0] == DISPLAY_STATE_SERVICE_SHUTDOWN || buff[0] == DISPLAY_STATE_LPM_OFF) {
		cancel_work_sync(&ts_data->resume_work);
		ts_data->power_status = POWER_OFF_STATUS;
		ts_data->power_disabled = true;
		mutex_lock(&ts_data->device_lock);
		fts_irq_disable();
#if FTS_PINCTRL_EN
		fts_pinctrl_select_release(ts_data);
#endif
		mutex_unlock(&ts_data->device_lock);
	}

	return count;
}

static ssize_t get_lp_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);

	if (ts_data->lp_dump_enabled) {
		fts_lpwg_dump_buf_read(ts_data, buf);
		return strlen(buf);
	} else {
		return snprintf(buf, SEC_CMD_BUF_SIZE, "Not support lp dump!\n");
	}
}

static DEVICE_ATTR(sensitivity_mode, 0644, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(prox_power_off, 0644, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(virtual_prox, 0664, protos_event_show, protos_event_store);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
static DEVICE_ATTR(enabled, 0664, enabled_show, enabled_store);
static DEVICE_ATTR(get_lp_dump, 0444, get_lp_dump, NULL);


static struct attribute *cmd_attributes[] = {
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_virtual_prox.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_enabled.attr,
	&dev_attr_get_lp_dump.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};


static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (ts_data->power_status == POWER_OFF_STATUS) {
		FTS_ERROR("IC is suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	fts_release_all_finger();

	switch (sec->cmd_param[0]) {
	case TSP_BUILT_IN:
	case TSP_SDCARD:
		ret = fts_fwupg_func(sec->cmd_param[0], true);
		break;
	default:
		ret = -EINVAL;
		FTS_ERROR("not supported %d", sec->cmd_param[0]);
		break;
	}

	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		fts_wait_tp_to_valid();
		fts_ex_mode_recovery(ts_data);

		snprintf(buff, sizeof(buff), "OK");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	FTS_INFO("%s", buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	char model[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (ts_data->power_status == POWER_OFF_STATUS) {
		FTS_ERROR("IC is suspend state");
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	ret = fts_get_fw_ver(ts_data);
	if (ret < 0) {
		FTS_ERROR("failed to read version, %d", ret);
		snprintf(buff, sizeof(buff), "NG");
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	/* IC version, Project version, module version, fw version */
	snprintf(buff, sizeof(buff), "FT%02X%02X%02X%02X",
			ts_data->ic_fw_ver.ic_name,
			ts_data->ic_fw_ver.project_name,
			ts_data->ic_fw_ver.module_id,
			ts_data->ic_fw_ver.fw_ver);
	snprintf(model, sizeof(model), "FT%02X%02X",
			ts_data->ic_fw_ver.ic_name,
			ts_data->ic_fw_ver.project_name);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	FTS_INFO("%s", buff);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	/* IC version, Project version, module version, fw version */
	snprintf(buff, sizeof(buff), "FT%02X%02X%02X%02X",
			ts_data->bin_fw_ver.ic_name,
			ts_data->bin_fw_ver.project_name,
			ts_data->bin_fw_ver.module_id,
			ts_data->bin_fw_ver.fw_ver);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	FTS_INFO("%s", buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "FOCALTECH");
	FTS_INFO("%s", buff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "FT%02X%02X",
			ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);

	FTS_INFO("%s", buff);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%d", ts_data->tx_num);
	FTS_INFO("%s", buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}
static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%d", ts_data->rx_num);
	FTS_INFO("%s", buff);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void fts_print_frame(struct fts_ts_data *ts, short *min, short *max)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int lsize = 10 * (ts->tx_num + 1);

	FTS_FUNC_ENTER();

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), "      TX");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->tx_num; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d ", i);
		strlcat(pStr, pTmp, lsize);
	}
	FTS_RAW_INFO("%s", pStr);

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ts->tx_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "----");
		strlcat(pStr, pTmp, lsize);
	}

	FTS_RAW_INFO("%s", pStr);

	*min = *max = ts->pFrame[0];

	for (i = 0; i < ts->rx_num; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "Rx%02d | ", i);
		strlcat(pStr, pTmp, lsize);

		for (j = 0; j < ts->tx_num; j++) {
			snprintf(pTmp, sizeof(pTmp), " %3d", ts->pFrame[(i * ts->tx_num) + j]);

			if (ts->pFrame[(i * ts->tx_num) + j] < *min)
				*min = ts->pFrame[(i * ts->tx_num) + j];

			if (ts->pFrame[(i * ts->tx_num) + j] > *max)
				*max = ts->pFrame[(i * ts->tx_num) + j];

			strlcat(pStr, pTmp, lsize);
		}
		FTS_RAW_INFO("%s", pStr);
	}
	kfree(pStr);

	FTS_RAW_INFO("min:%d, max:%d", *min, *max);
}

int fts_test_read(u8 addr, u8 *readbuf, int readlen)
{
	int ret = 0;
	int i = 0;
	int packet_length = 0;
	int packet_num = 0;
	int packet_remainder = 0;
	int offset = 0;
	int byte_num = readlen;

	packet_num = byte_num / BYTES_PER_TIME;
	packet_remainder = byte_num % BYTES_PER_TIME;
	if (packet_remainder)
		packet_num++;

	if (byte_num < BYTES_PER_TIME) {
		packet_length = byte_num;
	} else {
		packet_length = BYTES_PER_TIME;
	}
	/* FTS_TEST_DBG("packet num:%d, remainder:%d", packet_num, packet_remainder); */

	ret = fts_read(&addr, 1, &readbuf[offset], packet_length);
	if (ret < 0) {
		FTS_ERROR("read buffer fail");
		return ret;
	}
	for (i = 1; i < packet_num; i++) {
		offset += packet_length;
		if ((i == (packet_num - 1)) && packet_remainder) {
			packet_length = packet_remainder;
		}

		ret = fts_read(&addr, 1, &readbuf[offset],
						packet_length);
		if (ret < 0) {
			FTS_ERROR("read buffer fail");
			return ret;
		}
	}

	return 0;
}

static int fts_read_frame(struct fts_ts_data *ts_data, u8 cmd, short *min, short *max, bool print_log)
{
	u8 *rawbuff;
	short *temp = NULL;
	int ret, i, j;
	int read_size;
	bool read_one_byte = false;

	if (cmd == FACTORY_REG_CB_ADDR)
		read_one_byte = true;

	memset(ts_data->pFrame, 0x00, ts_data->pFrame_size);

	rawbuff = kzalloc(ts_data->pFrame_size, GFP_KERNEL);
	if (!rawbuff) {
		FTS_ERROR("failed to alloc rawbuff");
		ret = -ENOMEM;
		goto out;
	}

	temp = kzalloc(ts_data->pFrame_size, GFP_KERNEL);
	if (!temp) {
		FTS_ERROR("failed to alloc temp buff");
		ret = -ENOMEM;
		goto out;
	}

	if (read_one_byte)
		read_size = ts_data->pFrame_size / 2;
	else
		read_size = ts_data->pFrame_size;

	ret = fts_test_read(cmd, rawbuff, read_size);
	if (ret < 0) {
		FTS_ERROR("failed to read rawdata");
		goto out;
	}

	if (read_one_byte) {
		for (i = 0; i <= read_size; i++)
			temp[i] = rawbuff[i];
	} else {
		for (i = 0; i <= ts_data->pFrame_size; i += 2)
			temp[i / 2] = (rawbuff[i] << 8) + rawbuff[i + 1];
	}

	for (i = 0; i < ts_data->tx_num; i++)
		for (j = 0; j < ts_data->rx_num; j++)
			ts_data->pFrame[(j * ts_data->tx_num) + i] = temp[(i * ts_data->rx_num) + j];

	if (print_log)
		fts_print_frame(ts_data, min, max);

out:
	if (rawbuff)
		kfree(rawbuff);
	if (temp)
		kfree(temp);

	return ret;
}

static void get_short(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0, i;
	short min_value, max_value;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		fts_enter_test_environment(1);
		enter_factory_mode();
	}

	ret = fts_wait_test_done(FACTORY_REG_SHORT_TEST_EN, 1, 500);
	if (ret < 0) {
		FTS_ERROR("failed to do short test");
		goto out;
	}

	ret = fts_read_frame(ts_data, FACTORY_REG_SHORT_ADDR, &min_value, &max_value, false);
	if (ret < 0) {
		FTS_ERROR("failed to read adc");
		goto out;
	}

	/* calculate resistor */
	for (i = 0; i < ts_data->tx_num * ts_data->rx_num; i++) {
		unsigned short tmp_adc = ts_data->pFrame[i];

		if ((tmp_adc >= 4003) && (tmp_adc < 4096))
			tmp_adc = 4003;

		ts_data->pFrame[i] = (67 * tmp_adc + 7735) / (4095 - tmp_adc);
	}

	FTS_RAW_INFO("%s", __func__);
	fts_print_frame(ts_data, &min_value, &max_value);

out:
	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		enter_work_mode();
		fts_enter_test_environment(0);

	}

	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SHORT");
	FTS_INFO("%s", buff);
}

static void get_open(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	short min_value, max_value;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		fts_enter_test_environment(1);
		enter_factory_mode();
	}

	ret = fts_write_reg(FACTORY_REG_OPEN_DV, 0x04);
	if (ret < 0) {
		FTS_ERROR("failed to write FACTORY_REG_OPEN_DV");
		goto out;
	}

	ret = fts_wait_test_done(FACTORY_REG_OPEN_START2, 0x01, 500);
	if (ret < 0) {
		FTS_ERROR("failed to do open test");
		goto out;
	}

	FTS_RAW_INFO("%s", __func__);
	ret = fts_read_frame(ts_data, FACTORY_REG_OPEN_ADDR2, &min_value, &max_value, true);
	if (ret < 0) {
		FTS_ERROR("failed to read open data");
		goto out;
	}

out:
	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		enter_work_mode();
		fts_enter_test_environment(0);
	}

	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "OPEN");
	FTS_INFO("%s", buff);
}

static void get_cb(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	short min_value, max_value;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		fts_enter_test_environment(1);
		enter_factory_mode();
	}

	ret = fts_write_reg(FACTORY_REG_CB_TEST_EN, 1);
	if (ret < 0) {
		FTS_ERROR("failed to enable cb test");
		goto out;
	}

	ret = fts_wait_test_done(FACTORY_REG_CLB, FACTORY_REG_CLB_START, 100);
	if (ret < 0) {
		FTS_ERROR("failed to do calibration");
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_CB_ADDR_H, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write cb addr H");
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_CB_ADDR_L, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write cb addr L");
		goto out_testmode;
	}

	FTS_RAW_INFO("%s", __func__);
	ret = fts_read_frame(ts_data, FACTORY_REG_CB_ADDR, &min_value, &max_value, true);
	if (ret < 0) {
		FTS_ERROR("failed to read cb data");
		goto out_testmode;
	}

out_testmode:
	if (fts_write_reg(FACTORY_REG_CB_TEST_EN, 0) < 0) {
		FTS_ERROR("failed to disable cb test");
		ret = -EIO;
	}

out:
	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		enter_work_mode();
		fts_enter_test_environment(0);
	}

	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "CB");
	FTS_INFO("%s", buff);
}

static void get_rawdata(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	short min_value, max_value;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		fts_enter_test_environment(1);
		enter_factory_mode();
	}

	ret = fts_write_reg(FACTORY_REG_RAWDATA_TEST_EN, 1);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata test enable");
		goto out;
	}

	ret = fts_wait_test_done(DEVICE_MODE_ADDR, FACTORY_REG_START_SCAN, 20);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata scan");
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_LINE_ADDR, FACTORY_REG_RAWADDR_SET);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata addr setting");
		goto out_testmode;
	}

	FTS_RAW_INFO("%s", __func__);
	ret = fts_read_frame(ts_data, FACTORY_REG_RAWDATA_ADDR, &min_value, &max_value, true);
	if (ret < 0) {
		FTS_ERROR("failed to read rawdata");
		goto out_testmode;
	}

out_testmode:
	if (fts_write_reg(FACTORY_REG_RAWDATA_TEST_EN, 0) < 0) {
		FTS_ERROR("failed to write rawdata test disable");
		ret = -EIO;
	}

out:
	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		enter_work_mode();
		fts_enter_test_environment(0);
	}

	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "RAWDATA");
	FTS_INFO("%s", buff);
}

static void get_noise(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	short min_value, max_value;
	u8 prev_data;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		fts_enter_test_environment(1);
		enter_factory_mode();
	}

	ret = fts_read_reg(FACTORY_REG_DATA_SELECT, &prev_data);
	if (ret < 0) {
		FTS_ERROR("failed to read data select type");
		goto out;
	}

	ret = fts_write_reg(FACTORY_REG_DATA_SELECT, 0x01);
	if (ret < 0) {
		FTS_ERROR("failed to write data select type 1");
		goto out;
	}

	ret = fts_write_reg(FACTORY_REG_LINE_ADDR, FACTORY_REG_RAWADDR_SET);
	if (ret < 0) {
		FTS_ERROR("failed to write rawdata addr setting");
		goto out_testmode;
	}

	ret = fts_write_reg(FACTORY_REG_LCD_NOISE_FRAME, 0x32); /* 200 frame */
	if (ret < 0) {
		FTS_ERROR("failed to write noise frame");
		goto out_testmode;
	}

	ret = fts_wait_test_done(FACTORY_REG_LCD_NOISE_START, 0x01, 1500);
	if (ret < 0) {
		FTS_ERROR("failed to do noise test");
		goto out_testmode;
	}

	FTS_RAW_INFO("%s", __func__);
	ret = fts_read_frame(ts_data, FACTORY_REG_RAWDATA_ADDR, &min_value, &max_value, true);
	if (ret < 0) {
		FTS_ERROR("failed to read noise test rawdata");
		goto out_testmode;
	}

out_testmode:
	FTS_INFO("restore data select type 0x%02X", prev_data);
	if (fts_write_reg(FACTORY_REG_DATA_SELECT, prev_data) < 0) {
		FTS_ERROR("failed to restore data select type");
		ret = -EIO;
	}

	if (fts_write_reg(FACTORY_REG_LCD_NOISE_START, 0) < 0) {
		FTS_ERROR("failed to disable noise test");
		ret = -EIO;
	}

out:
	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING) {
		enter_work_mode();
		fts_enter_test_environment(0);
	}

	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d,%d", min_value, max_value);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "NOISE");
	FTS_INFO("%s", buff);
}

static void get_nonflash_ram_test(void *device_data)
{
	
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	int i = 0;
	bool tmp_result = false;
	uint8_t val[4] = { 0 };
	uint8_t cmd = 0x85;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		fts_enter_test_environment(1);

	ret = fts_download_ramtest_bin();
	if (ret < 0) {
		FTS_ERROR("download ramtest bin fails");
		goto err_reset;
	}
    
	for (i = 0; i < 25; i++) {
		sec_delay(50);
		ret = fts_read(&cmd, 1, val, 4);
		if ((val[0] == 0xA1) && (val[1] == 0xB0) && (val[2] == 0x00)) {
			FTS_INFO("ram test done,0x%x", val[3]);
			break;
		}
	}

	if (i >= 25) {
		FTS_ERROR("ram test timeout");
		goto err_reset;
	}

	FTS_INFO("ram test result:0x%x", val[3]);
	tmp_result = (val[3] == 0x80);
	ret = 0;

err_reset:
	if (tmp_result) {
		FTS_INFO("------ Ram Test PASS");
	} else {
		FTS_INFO("------ Ram Test NG");
	}

out:
	if (sec->cmd_all_factory_state != SEC_CMD_STATUS_RUNNING)
		fts_enter_test_environment(0);

	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "%d", TEST_RESULT_FAIL);
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "%d", TEST_RESULT_PASS);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");
	FTS_INFO("%s", buff);
}


static void run_allnode_data(void *device_data, int test_type)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buff;
	int i;

	sec_cmd_set_default_result(sec);

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(temp, sizeof(temp), "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		return;
	}

	buff = kzalloc(ts_data->tx_num * ts_data->rx_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff) {
		FTS_ERROR("failed to alloc allnode buff");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(temp, sizeof(temp), "NG");
		sec_cmd_set_cmd_result(sec, temp, strnlen(temp, sizeof(temp)));
		return;
	}

	fts_enter_test_environment(1);

	enter_factory_mode();

	FTS_INFO("test_type:%d", test_type);

	switch (test_type) {
	case ALL_NODE_TYPE_SHORT:
		get_short(sec);
		break;
	case ALL_NODE_TYPE_OPEN:
		get_open(sec);
		break;
	case ALL_NODE_TYPE_CB:
		get_cb(sec);
		break;
	case ALL_NODE_TYPE_RAWDATA:
		get_rawdata(sec);
		break;
	case ALL_NODE_TYPE_NOISE:
		get_noise(sec);
		break;
	case ALL_NODE_TYPE_NONFALSH_RAM:
		get_nonflash_ram_test(sec);
		break;
	default:
		FTS_ERROR("test type is not supported %d", test_type);
		break;
	}

	enter_work_mode();
	fts_enter_test_environment(0);

	sec_cmd_set_default_result(sec);

	for (i = 0; i < ts_data->tx_num * ts_data->rx_num; i++) {
		snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", ts_data->pFrame[i]);
		strlcat(buff, temp, ts_data->tx_num * ts_data->rx_num * CMD_RESULT_WORD_LEN);
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	FTS_INFO("%s", buff);

	kfree(buff);
}

static void run_short_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_SHORT);
}

static void run_open_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_OPEN);
}

static void run_cb_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_CB);
}

static void run_rawdata_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_RAWDATA);
}

static void run_noise_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_NOISE);
}

static void run_nonflash_ram_test_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	FTS_FUNC_ENTER();
	run_allnode_data(sec, ALL_NODE_TYPE_NONFALSH_RAM);
}


static void get_gap_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ii;
	int node_gap_tx = 0;
	int node_gap_rx = 0;
	int tx_max = 0;
	int rx_max = 0;

	for (ii = 0; ii < (ts->rx_num * ts->tx_num); ii++) {
		if ((ii + 1) % (ts->tx_num) != 0) {
			if (ts->pFrame[ii] > ts->pFrame[ii + 1])
				node_gap_tx = 100 - (ts->pFrame[ii + 1] * 100 / ts->pFrame[ii]);
			else
				node_gap_tx = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + 1]);
			tx_max = max(tx_max, node_gap_tx);
		}

		if (ii < (ts->rx_num - 1) * ts->tx_num) {
			if (ts->pFrame[ii] > ts->pFrame[ii + ts->tx_num])
				node_gap_rx = 100 - (ts->pFrame[ii + ts->tx_num] * 100 / ts->pFrame[ii]);
			else
				node_gap_rx = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + ts->tx_num]);
			rx_max = max(rx_max, node_gap_rx);
		}
	}

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		snprintf(buff, sizeof(buff), "%d,%d", 0, tx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "RAW_GAP_X");
		snprintf(buff, sizeof(buff), "%d,%d", 0, rx_max);
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "RAW_GAP_Y");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void run_gap_data_x_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	for (ii = 0; ii < (ts->rx_num * ts->tx_num); ii++) {
		if ((ii + 1) % (ts->tx_num) != 0) {
			if (ts->pFrame[ii] > ts->pFrame[ii + 1])
				node_gap = 100 - (ts->pFrame[ii + 1] * 100 / ts->pFrame[ii]);
			else
				node_gap = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + 1]);

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void run_gap_data_y_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	char *buff = NULL;
	int ii;
	int node_gap = 0;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	buff = kzalloc(ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		return;

	for (ii = 0; ii < (ts->rx_num * ts->tx_num); ii++) {
		if (ii < (ts->rx_num - 1) * ts->tx_num) {
			if (ts->pFrame[ii] > ts->pFrame[ii + ts->tx_num])
				node_gap = 100 - (ts->pFrame[ii + ts->tx_num] * 100 / ts->pFrame[ii]);
			else
				node_gap = 100 - (ts->pFrame[ii] * 100 / ts->pFrame[ii + ts->tx_num]);

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, ts->tx_num * ts->rx_num * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	kfree(buff);
}

static void factory_cmd_result_all(void *dev_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);

	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);
	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	mutex_lock(&ts_data->device_lock);

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	fts_enter_test_environment(1);
	
	enter_factory_mode();

	get_short(sec);
	get_open(sec);
	get_cb(sec);
	get_noise(sec);
	get_rawdata(sec);
	get_gap_data(sec);

	enter_work_mode();
	get_nonflash_ram_test(sec);
	fts_enter_test_environment(0);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;
	FTS_RAW_INFO("%s: %s", __func__, sec->cmd_result_all);

	mutex_unlock(&ts_data->device_lock);
}

static void run_snr_non_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0, i;
	u8 status = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_param[0] <= 0 || sec->cmd_param[0] > 1000) {
		FTS_ERROR("invalid param %d", sec->cmd_param[0]);
		ret = -EINVAL;
		goto out;
	}

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
		goto out;
	}

	ret = fts_write_reg(FTS_REG_SNR_FRAME_COUNT_H, (sec->cmd_param[0] >> 8) & 0xFF);
	if (ret < 0) {
		FTS_ERROR("failed to write snr frame count High, ret=%d", ret);
		goto out_mode;
	}

	ret = fts_write_reg(FTS_REG_SNR_FRAME_COUNT_L, sec->cmd_param[0] & 0xFF);
	if (ret < 0) {
		FTS_ERROR("failed to write snr frame count Low, ret=%d", ret);
		goto out_mode;
	}

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 0, ret=%d", ret);
		goto out;
	}

	ret = fts_write_reg(FTS_REG_POWER_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write power mode 0, ret=%d", ret);
		goto out;
	}

	FTS_INFO("test start");
	ret = fts_write_reg(FTS_REG_SNR_NON_TOUCHED_TEST, 1);
	if (ret < 0) {
		FTS_ERROR("failed to start snr non touched test, ret=%d", ret);
		goto out;
	}

	sec_delay(8 * sec->cmd_param[0]);

	for (i = 0; i < 50; i++) {
		ret = fts_read_reg(FTS_REG_SNR_NON_TOUCHED_TEST, &status);
		if (ret < 0) {
			FTS_ERROR("failed to read snr test status, ret=%d", ret);
			goto out;
		}

		if (status == TEST_RETVAL_00)
			break;

		sec_delay(100);
	}

	if (i == 50) {
		FTS_ERROR("test timeout, status=0x%02X", status);
		ret = -EIO;
		goto out;
	}

out_mode:
	if (fts_write_reg(FTS_REG_BANK_MODE, 0) < 0)
		FTS_ERROR("failed to write bank mode 0");

out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "OK");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	FTS_INFO("%s", buff);
}

static void run_snr_touched(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[27 * 10] = { 0 };
	int ret = 0, i;
	u8 status = 0, cmd;
	u8 status2 = 0;
	u8 rbuff[27 * 2] = { 0 };
	short average[9] = { 0 };
	short snr1[9] = { 0 };
	short snr2[9] = { 0 };

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	if (sec->cmd_param[0] <= 0 || sec->cmd_param[0] > 1000) {
		FTS_ERROR("invalid param %d", sec->cmd_param[0]);
		ret = -EINVAL;
		goto out;
	}

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode 1, ret=%d", ret);
		goto out;
	}

	FTS_INFO("test start");
	ret = fts_write_reg(FTS_REG_SNR_TOUCHED_TEST, 1);
	if (ret < 0) {
		FTS_ERROR("failed to start snr non touched test, ret=%d", ret);
		goto out_mode;
	}

	sec_delay(8 * sec->cmd_param[0]);

	for (i = 0; i < 50; i++) {
		ret = fts_read_reg(FTS_REG_SNR_TEST_STATUS, &status);
		if (ret < 0) {
			FTS_ERROR("failed to read snr test status, ret=%d", ret);
			goto out_mode;
		}

		ret = fts_read_reg(FTS_REG_SNR_TOUCHED_TEST, &status2);
		if (ret < 0) {
			FTS_ERROR("failed to read snr test status, ret=%d", ret);
			goto out_mode;
		}

		if (status == TEST_RETVAL_AA && status2 == TEST_RETVAL_00)
			break;

		sec_delay(100);
	}

	if (i == 50) {
		FTS_ERROR("test timeout, status=0x%02X", status);
		ret = -EIO;
		goto out_mode;
	}

	ret = fts_write_reg(FTS_REG_SNR_BUFFER_COUNTER, 0);
	if (ret < 0) {
		FTS_ERROR("failed to write FTS_REG_SNR_BUFFER_COUNTER, ret=%d", ret);
		goto out_mode;
	}

	cmd = FTS_REG_SNR_TEST_RESULT1;
	ret = fts_read(&cmd, 1, rbuff, 24);
	if (ret < 0) {
		FTS_ERROR("failed to read snr test result1, ret=%d", ret);
		goto out_mode;
	}

	cmd = FTS_REG_SNR_TEST_RESULT1;
	ret = fts_read(&cmd, 1, &rbuff[24], 30);
	if (ret < 0) {
		FTS_ERROR("failed to read snr test result2, ret=%d", ret);
		goto out_mode;
	}

	for (i = 0; i < 9; i++) {
		char temp[3 * 10] = { 0 };

		average[i] = rbuff[i * 6] << 8 | rbuff[(i * 6) + 1];
		snr1[i] = rbuff[(i * 6) + 2] << 8 | rbuff[(i * 6) + 3]; 
		snr2[i] = rbuff[(i * 6) + 4] << 8 | rbuff[(i * 6) + 5];
		snprintf(temp, sizeof(temp), "%d,%d,%d%s",
				average[i], snr1[i], snr2[i], (i < 8) ? "," : "");
		strlcat(buff, temp, sizeof(buff));
	}

out_mode:
	if (fts_write_reg(FTS_REG_BANK_MODE, 0) < 0)
		FTS_ERROR("failed to write bank mode 0");

out:
	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	FTS_INFO("%s", buff);
}

static void get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[27 * 10] = { 0 };
	int ret = 0;
	u8 rbuff = 0;
	u16 crc = 0, crc_r = 0;
	u32 crc_result = 0;

	sec_cmd_set_default_result(sec);

	mutex_lock(&ts_data->device_lock);

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0x00);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode, ret=%d", ret);
		goto out;
	}

	sec_delay(200);

	ret = fts_read_reg(FTS_REG_CRC_H, &rbuff);
	if (ret < 0) {
		FTS_ERROR("failed to read crc H, ret=%d", ret);
		goto out_mode;
	}
	crc |= rbuff << 8;

	ret = fts_read_reg(FTS_REG_CRC_L, &rbuff);
	if (ret < 0) {
		FTS_ERROR("failed to read crc L, ret=%d", ret);
		goto out_mode;
	}
	crc |= rbuff;

	ret = fts_read_reg(FTS_REG_CRC_REVERT_H, &rbuff);
	if (ret < 0) {
		FTS_ERROR("failed to read crc_r H, ret=%d", ret);
		goto out_mode;
	}
	crc_r |= rbuff << 8;

	ret = fts_read_reg(FTS_REG_CRC_REVERT_L, &rbuff);
	if (ret < 0) {
		FTS_ERROR("failed to read crc_r L, ret=%d", ret);
		goto out_mode;
	}
	crc_r |= rbuff;

	crc_result = crc | crc_r;
	FTS_INFO("crc:0x%04X, crc_r:0x%04X, result:0x%04X", crc, crc_r, crc_result);

	if (crc_result == 0xFFFF)
		ret = 0;
	else
		ret = -EIO;

out_mode:
	if (fts_write_reg(FTS_REG_BANK_MODE, 0) < 0)
		FTS_ERROR("failed to write bank mode 0");
out:
	mutex_unlock(&ts_data->device_lock);

	if (ret < 0) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buff, sizeof(buff), "OK");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	FTS_INFO("%s", buff);
}

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 enable;
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;

	ret = fts_write_reg(FTS_REG_SIP_MODE, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write sip mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", enable ? "on" : "off");
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 enable;
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;

	ret = fts_write_reg(FTS_REG_GAME_MODE, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write game mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", enable ? "on" : "off");
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_note_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 enable;
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;

	ret = fts_write_reg(FTS_REG_NOTE_MODE, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write note mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", enable ? "on" : "off");
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
#if 1
	u8 enable;
	int ret = 0;
#endif

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ts_data->spay_enable = sec->cmd_param[0];
	if (ts_data->spay_enable)
		ts_data->gesture_mode |= GESTURE_SPAY_EN;
	else
		ts_data->gesture_mode &= ~GESTURE_SPAY_EN;

#if 1	/* vendor request 0(need to check) */
	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;
	ret = fts_write_reg(FTS_REG_SPAY_EN, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write spay mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}
#endif

	FTS_INFO("%s, gesture_mode:0x%02X", sec->cmd_param[0] ? "on" : "off",
			ts_data->gesture_mode);
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 enable;
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ts_data->aot_enable = sec->cmd_param[0];
	if (ts_data->aot_enable)
		ts_data->gesture_mode |= GESTURE_DOUBLECLICK_EN;
	else
		ts_data->gesture_mode &= ~GESTURE_DOUBLECLICK_EN;

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		ret = -ENODEV;
		goto out;
	}

	enable = sec->cmd_param[0] & 0xFF;
	ret = fts_write_reg(FTS_REG_DOUBLETAP_TO_WAKEUP_EN, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write aot mode %d", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s, gesture_mode:0x%02X", sec->cmd_param[0] ? "on" : "off",
			ts_data->gesture_mode);
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	ts_data->glove_mode = sec->cmd_param[0];

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", ts_data->glove_mode ? "on" : "off");

	ret = fts_write_reg(FTS_REG_GLOVE_MODE_EN, ts_data->glove_mode);
	if (ret < 0) {
		FTS_ERROR("failed to write glove mode");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 enable;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	enable = !(sec->cmd_param[0] & 0xFF);

	FTS_INFO("mode %s", enable ? "on" : "off");

	ret = fts_write_reg(FTS_REG_DEAD_ZONE_ENABLE, enable);
	if (ret < 0) {
		FTS_ERROR("failed to write 0x9E 0x%02X", enable);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void fts_set_grip_data_to_ic(struct fts_ts_data *ts, u8 flag)
{
	int ret, i, data_size = 0;
	u8 regAddr[9] = { 0 };
	u8 data[9] = { 0 };

	if (ts->power_disabled) {
		FTS_INFO("ic is off now, skip 0x%02X", flag);
		return;
	}

	if (flag == G_SET_EDGE_HANDLER) {
		FTS_INFO("set edge handler");

		regAddr[0] = FTS_REG_EDGE_HANDLER;
		regAddr[1] = FTS_REG_EDGE_HANDLER_DIRECTION;
		regAddr[2] = FTS_REG_EDGE_HANDLER_UPPER_H;
		regAddr[3] = FTS_REG_EDGE_HANDLER_UPPER_L;
		regAddr[4] = FTS_REG_EDGE_HANDLER_LOWER_H;
		regAddr[5] = FTS_REG_EDGE_HANDLER_LOWER_L;

		data[0] = 0x00;
		data[1] = ts->grip_data.edgehandler_direction & 0xFF;
		data[2] = (ts->grip_data.edgehandler_start_y >> 8) & 0xFF;
		data[3] = ts->grip_data.edgehandler_start_y & 0xFF;
		data[4] = (ts->grip_data.edgehandler_end_y >> 8) & 0xFF;
		data[5] = ts->grip_data.edgehandler_end_y & 0xFF;
		data_size = 6;
	} else if (flag == G_SET_NORMAL_MODE) {
		FTS_INFO("set portrait mode");

		regAddr[0] = FTS_REG_GRIP_MODE;
		regAddr[1] = FTS_REG_GRIP_PORTRAIT_MODE;
		regAddr[2] = FTS_REG_GRIP_PORTRAIT_GRIP_ZONE;
		regAddr[3] = FTS_REG_GRIP_PORTRAIT_REJECT_UPPER;
		regAddr[4] = FTS_REG_GRIP_PORTRAIT_REJECT_LOWER;
		regAddr[5] = FTS_REG_GRIP_PORTRAIT_REJECT_Y_H;
		regAddr[6] = FTS_REG_GRIP_PORTRAIT_REJECT_Y_L;

		data[0] = 0x01;
		data[1] = 0x01;
		data[2] = ts->grip_data.edge_range & 0xFF;
		data[3] = ts->grip_data.deadzone_up_x & 0xFF;
		data[4] = ts->grip_data.deadzone_dn_x & 0xFF;
		data[5] = (ts->grip_data.deadzone_y >> 8) & 0xFF;
		data[6] = ts->grip_data.deadzone_y & 0xFF;
		data_size = 7;
 	} else if (flag == G_SET_LANDSCAPE_MODE) {
		FTS_INFO("set landscape mode");

		regAddr[0] = FTS_REG_GRIP_MODE;
		regAddr[1] = FTS_REG_GRIP_LANDSCAPE_MODE;
		regAddr[2] = FTS_REG_GRIP_LANDSCAPE_ENABLE;
		regAddr[3] = FTS_REG_GRIP_LANDSCAPE_GRIP_ZONE;
		regAddr[4] = FTS_REG_GRIP_LANDSCAPE_REJECT_ZONE;
		regAddr[5] = FTS_REG_GRIP_LANDSCAPE_REJECT_TOP;
		regAddr[6] = FTS_REG_GRIP_LANDSCAPE_REJECT_BOTTOM;
		regAddr[7] = FTS_REG_GRIP_LANDSCAPE_GRIP_TOP;
		regAddr[8] = FTS_REG_GRIP_LANDSCAPE_GRIP_BOTTOM;

		data[0] = 0x02;
		data[1] = 0x02;
		data[2] = ts->grip_data.landscape_mode & 0xFF;
		data[3] = ts->grip_data.landscape_edge & 0xFF;
		data[4] = ts->grip_data.landscape_deadzone & 0xFF;
		data[5] = ts->grip_data.landscape_top_deadzone & 0xFF;
		data[6] = ts->grip_data.landscape_bottom_deadzone & 0xFF;
		data[7] = ts->grip_data.landscape_top_gripzone & 0xFF;
		data[8] = ts->grip_data.landscape_bottom_gripzone & 0xFF;
		data_size = 9;
 	} else if (flag == G_CLR_LANDSCAPE_MODE) {
		FTS_INFO("clear landscape mode");

		regAddr[0] = FTS_REG_GRIP_MODE;
		regAddr[1] = FTS_REG_GRIP_LANDSCAPE_MODE;
		regAddr[2] = FTS_REG_GRIP_LANDSCAPE_ENABLE;

		data[0] = 0x01;
		data[1] = 0x02;
		data[2] = ts->grip_data.landscape_mode & 0xFF;
		data_size = 3;
	} else {
		FTS_ERROR("flag 0x%02X is invalid", flag);
		return;
	}

	for (i = 0; i < data_size; i++) {
		ret = fts_write_reg(regAddr[i], data[i]);
		if (ret < 0) {
			FTS_ERROR("failed to write 0x%02X 0x%02X", regAddr[i], data[i]);
			return;
		}
	}
}

void fts_set_edge_handler(struct fts_ts_data *ts)
{
	u8 mode = G_NONE;

	FTS_INFO("re-init direction:%d", ts->grip_data.edgehandler_direction);

	if (ts->grip_data.edgehandler_direction != 0)
		mode = G_SET_EDGE_HANDLER;

	if (mode)
		fts_set_grip_data_to_ic(ts, mode);
}

/*
 *	index  0 :  set edge handler
 *		1 :  portrait (normal) mode
 *		2 :  landscape mode
 *
 *	data
 *		0, X (direction), X (y start), X (y end)
 *		direction : 0 (off), 1 (left), 2 (right)
 *			ex) echo set_grip_data,0,2,600,900 > cmd
 *
 *		1, X (edge zone), X (dead zone up x), X (dead zone down x), X (dead zone up y), X (dead zone bottom x), X (dead zone down y)
 *			ex) echo set_grip_data,1,60,10,32,926,32,3088 > cmd
 *
 *		2, 1 (landscape mode), X (edge zone), X (dead zone x), X (dead zone top y), X (dead zone bottom y), X (edge zone top y), X (edge zone bottom y)
 *			ex) echo set_grip_data,2,1,200,100,120,0 > cmd
 *
 *		2, 0 (portrait mode)
 *			ex) echo set_grip_data,2,0 > cmd
 */
static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u8 mode = G_NONE;

	sec_cmd_set_default_result(sec);

	mutex_lock(&ts->device_lock);

	if (sec->cmd_param[0] == 0) {	// edge handler
		if (sec->cmd_param[1] >= 0 && sec->cmd_param[1] < 3) {
			ts->grip_data.edgehandler_direction = sec->cmd_param[1];
			ts->grip_data.edgehandler_start_y = sec->cmd_param[2];
			ts->grip_data.edgehandler_end_y = sec->cmd_param[3];
		} else {
			FTS_ERROR("cmd1 is abnormal, %d (%d)", sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
		mode = G_SET_EDGE_HANDLER;
	} else if (sec->cmd_param[0] == 1) {	// normal mode
		ts->grip_data.edge_range = sec->cmd_param[1];
		ts->grip_data.deadzone_up_x = sec->cmd_param[2];
		ts->grip_data.deadzone_dn_x = sec->cmd_param[3];
		ts->grip_data.deadzone_y = sec->cmd_param[4];
		mode = G_SET_NORMAL_MODE;
	} else if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// normal mode
			ts->grip_data.landscape_mode = 0;
			mode = G_CLR_LANDSCAPE_MODE;
		} else if (sec->cmd_param[1] == 1) {
			ts->grip_data.landscape_mode = 1;
			ts->grip_data.landscape_edge = sec->cmd_param[2];
			ts->grip_data.landscape_deadzone = sec->cmd_param[3];
			ts->grip_data.landscape_top_deadzone = sec->cmd_param[4];
			ts->grip_data.landscape_bottom_deadzone = sec->cmd_param[5];
			ts->grip_data.landscape_top_gripzone = sec->cmd_param[6];
			ts->grip_data.landscape_bottom_gripzone = sec->cmd_param[7];
			mode = G_SET_LANDSCAPE_MODE;
		} else {
			FTS_ERROR("cmd1 is abnormal, %d (%d)", sec->cmd_param[1], __LINE__);
			goto err_grip_data;
		}
	} else {
		FTS_ERROR("cmd0 is abnormal, %d", sec->cmd_param[0]);
		goto err_grip_data;
	}

	fts_set_grip_data_to_ic(ts, mode);

	mutex_unlock(&ts->device_lock);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

err_grip_data:
	mutex_unlock(&ts->device_lock);

	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

void fts_set_scan_off(bool scan_off)
{
	int ret;
	u8 cmd;

	if (scan_off)
		cmd = FTS_REG_POWER_MODE_SCAN_OFF;
	else
		cmd = 0x00;

	FTS_INFO("scan %s", scan_off ? "off" : "on");

	ret = fts_write_reg(FTS_REG_POWER_MODE, cmd);
	if (ret < 0)
		FTS_ERROR("failed to write scan off");
}

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 3) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (sec->cmd_param[0] > 1)
		ts_data->cover_mode = true;
	else
		ts_data->cover_mode = false;

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", ts_data->cover_mode ? "closed" : "opened");

	if (ts_data->pdata->scan_off_when_cover_closed) {
		mutex_lock(&ts_data->device_lock);
		fts_set_scan_off(ts_data->cover_mode);
		mutex_unlock(&ts_data->device_lock);
	} else {
		ret = fts_write_reg(FTS_REG_COVER_MODE_EN, ts_data->cover_mode);
		if (ret < 0) {
			FTS_ERROR("failed to write cover mode");
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto out;
		}
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void scan_block(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] != 0 && sec->cmd_param[0] != 1) {
		FTS_ERROR("cmd_param is wrong %d", sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	FTS_INFO("%s", sec->cmd_param[0] ? "block" : "unblock");

	fts_set_scan_off(sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!(sec->cmd_param[0] == 0 || sec->cmd_param[0] == 1 || sec->cmd_param[0] == 3)) {
		FTS_ERROR("abnormal parm (%d)", sec->cmd_param[0]);
		goto fail;
	}

	ts_data->proximity_mode = sec->cmd_param[0];

	if (ts_data->power_disabled) {
		FTS_ERROR("ic power is disabled");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto fail;
	}

	ret = fts_write_reg(FTS_REG_PROXIMITY_MODE, ts_data->proximity_mode);
	if (ret < 0) {
		FTS_ERROR("send ear detect cmd failed");
		goto fail;
	}
	ts_data->hover_event = 0xFF;

	FTS_INFO("%d", sec->cmd_param[0]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

fail:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;
}

static void prox_lp_scan_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;
	u8 mode = FTS_REG_POWER_MODE_SCAN_OFF;
	int retry = 10;

	sec_cmd_set_default_result(sec);

	if (!ts_data->pdata->prox_lp_scan_enabled) {
		FTS_ERROR("Not support LPSCAN!");
		goto out;
	}

	if (ts_data->power_status != LP_MODE_STATUS) {
		FTS_ERROR("Not LP_MODE_STATUS!");
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		FTS_ERROR("invalid parameter %d", sec->cmd_param[0]);
		goto out;
	} else {
		mode = sec->cmd_param[0] ? FTS_REG_POWER_MODE_SCAN_ON : FTS_REG_POWER_MODE_SCAN_OFF;
	}

	if (mutex_lock_interruptible(&ts_data->device_lock)) {
		FTS_ERROR("another task is running");
		goto out;
	}

	if (mode == FTS_REG_POWER_MODE_SCAN_ON) {
		ret = fts_write_reg(FTS_PROX_AOT_IDLE_SCAN, 1);
		if (ret < 0) {
			FTS_ERROR("write FTS_PROX_AOT_IDLE_SCAN fail\n");
			mutex_unlock(&ts_data->device_lock);
			goto out;
		}
	}

	while (retry) {
		ret = fts_write_reg(FTS_REG_POWER_MODE, mode);
		if (ret < 0) {
			FTS_ERROR("retry:%d", retry);
			retry--;
		} else {
			break;
		}
	}
	if (ret < 0) {
		FTS_ERROR("failed to switch %s mode", (mode == FTS_REG_POWER_MODE_SCAN_OFF) ? "SCAN OFF" : "SCAN");
		mutex_unlock(&ts_data->device_lock);
		goto out;
	}

	mutex_unlock(&ts_data->device_lock);

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	FTS_INFO("switch to %s mode OK",
					(mode == FTS_REG_POWER_MODE_SCAN_OFF) ? "SCAN OFF" : "SCAN");

	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	FTS_ERROR("failed to switch %d mode", mode);
}

static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
//	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u16 thd = 0;
	short sum = 0;
	u8 cmd = 0;
	u8 val[4] = {0};
	int ret = 0;

	sec_cmd_set_default_result(sec);

	FTS_FUNC_ENTER();

	cmd = FTS_PROX_TOUCH_SENS;
	ret = fts_read(&cmd, 1, val, 4);
	if (ret < 0) {
		FTS_ERROR("failed to read snr test result1, ret=%d", ret);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
		goto out;
	}

	thd = (val[0] << 8) + val[1];
	sum = (val[2] << 8) + val[3];
	snprintf(buff, sizeof(buff), "SUM_X:%d THD_X:%d", sum, thd);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	FTS_INFO("%s", buff);
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("get_short", get_short),},
	{SEC_CMD("run_short_all", run_short_all),},
	{SEC_CMD("get_open", get_open),},
	{SEC_CMD("run_open_all", run_open_all),},
	{SEC_CMD("get_cb", get_cb),},
	{SEC_CMD("run_cb_all", run_cb_all),},
	{SEC_CMD("get_rawdata", get_rawdata),},
	{SEC_CMD("run_rawdata_all", run_rawdata_all),},
	{SEC_CMD("run_gap_data_x_all", run_gap_data_x_all),},
	{SEC_CMD("run_gap_data_y_all", run_gap_data_y_all),},
	{SEC_CMD("get_noise", get_noise),},
	{SEC_CMD("run_noise_all", run_noise_all),},
	{SEC_CMD("get_nonflash_ram_test", get_nonflash_ram_test),},
	{SEC_CMD("run_nonflash_ram_test_all", run_nonflash_ram_test_all),},
	{SEC_CMD("run_snr_non_touched", run_snr_non_touched),},
	{SEC_CMD("run_snr_touched", run_snr_touched),},
	{SEC_CMD("run_cs_raw_read_all", run_rawdata_all),},
	{SEC_CMD("get_crc_check", get_crc_check),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("set_sip_mode", set_sip_mode),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
	{SEC_CMD_H("set_note_mode", set_note_mode),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD_H("scan_block", scan_block),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("prox_lp_scan_mode", prox_lp_scan_mode),},
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

void fts_run_rawdata_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct fts_ts_data *ts_data = container_of(sec, struct fts_ts_data, sec);

	mutex_lock(&ts_data->device_lock);
	FTS_RAW_INFO("%s\n", __func__);

	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);
	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	fts_enter_test_environment(1);
	enter_factory_mode();

	get_short(sec);
	get_open(sec);
	get_cb(sec);
	get_noise(sec);
	get_rawdata(sec);
	get_gap_data(sec);

	enter_work_mode();
	fts_enter_test_environment(0);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;
	FTS_RAW_INFO("%s: %s", __func__, sec->cmd_result_all);

	mutex_unlock(&ts_data->device_lock);
}

static int fts_sec_facory_test_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	u8 rbuf;

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0x00);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode, ret=%d", ret);
	}

	ret = fts_read_reg(FTS_REG_TX_NUM, &rbuf);
	if (ret < 0) {
		FTS_ERROR("get tx_num fail");
		ts_data->tx_num = TX_NUM_MAX;
	} else {
		ts_data->tx_num = rbuf;
	}

	ret = fts_read_reg(FTS_REG_RX_NUM, &rbuf);
	if (ret < 0) {
		FTS_ERROR("get rx_num fail");
		ts_data->rx_num = RX_NUM_MAX;
	} else {
		ts_data->rx_num = rbuf;
	}

	ret = fts_write_reg(FTS_REG_BANK_MODE, 0x00);
	if (ret < 0) {
		FTS_ERROR("failed to write bank mode, ret=%d", ret);
	}

	ts_data->pFrame_size = ts_data->tx_num * ts_data->rx_num * sizeof(short);
	ts_data->pFrame = devm_kzalloc(ts_data->dev, ts_data->pFrame_size, GFP_KERNEL);
	if (!ts_data->pFrame) {
		FTS_ERROR("failed to alloc pFrame buff");
		return -ENOMEM;
	}

	FTS_INFO("tx_num:%d, rx_num:%d, pFrame_size:%d", ts_data->tx_num, ts_data->rx_num, ts_data->pFrame_size);
	return 0;
}

int fts_sec_cmd_init(struct fts_ts_data *ts_data)
{
	int retval = 0;

	FTS_FUNC_ENTER();

	retval = fts_sec_facory_test_init(ts_data);
	if (retval < 0) {
		FTS_ERROR("Failed to init sec_factory_test");
		goto exit;
	}

	retval = sec_cmd_init(&ts_data->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		FTS_ERROR("Failed to sec_cmd_init");
		goto exit;
	}

	ts_data->sec.wait_cmd_result_done = true;

	retval = sysfs_create_group(&ts_data->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		FTS_ERROR("Failed to create sysfs attributes");
		sec_cmd_exit(&ts_data->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	retval = sysfs_create_link(&ts_data->sec.fac_dev->kobj,
			&ts_data->input_dev->dev.kobj, "input");
	if (retval < 0) {
		FTS_ERROR("Failed to create input symbolic link");
		sysfs_remove_group(&ts_data->sec.fac_dev->kobj, &cmd_attr_group);
		sec_cmd_exit(&ts_data->sec, SEC_CLASS_DEVT_TSP);
		goto exit;
	}

	FTS_FUNC_EXIT();
	return 0;

exit:
	return retval;

}

void fts_sec_cmd_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();

	sysfs_remove_link(&ts_data->sec.fac_dev->kobj, "input");

	sysfs_remove_group(&ts_data->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&ts_data->sec, SEC_CLASS_DEVT_TSP);
}

MODULE_LICENSE("GPL");

