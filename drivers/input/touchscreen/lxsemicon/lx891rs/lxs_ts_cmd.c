/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * lxs_ts_cmd.c
 *
 * LXS touch SEC cmd group
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "lxs_ts_dev.h"
#include "lxs_ts_reg.h"

static ssize_t fwup_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	if (sscanf(buf, "%255s", ts->test_fwpath) <= 0) {
		input_err(true, &ts->client->dev, "%s\n", "Invalid param");
		return count;
	}

	input_info(true, &ts->client->dev, "Manual F/W upgrade with %s\n", ts->test_fwpath);

	lxs_ts_load_fw_from_external(ts, ts->test_fwpath);

	lxs_ts_ic_reset(ts);

	return count;
}

static ssize_t reset_ctrl_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int value = 0;

	if (sscanf(buf, "%d", &value) <= 0) {
		input_err(true, &ts->client->dev, "%s\n", "Invalid param");
		return count;
	}

	switch (value) {
	case 2:
		atomic_set(&ts->fwsts, 0);
		/* fall through */
	case 1:
		ts->reset_retry = 0;
		lxs_ts_ic_reset(ts);
		break;
	}

	return count;
}

#define REG_BURST_MAX			512
#define REG_BURST_COL_PWR		4

#define REG_LOG_MAX		8
#define REG_DIR_NONE		0
#define REG_DIR_RD		1
#define REG_DIR_WR		2
#define REG_DIR_ERR		(1<<8)
#define REG_DIR_MASK		(REG_DIR_ERR-1)

#define __snprintf(_buf, _size, _fmt, _args...)	\
		snprintf(_buf + _size, PAGE_SIZE - _size, (const char *)_fmt, ##_args)

static ssize_t reg_ctrl_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
//	struct sec_cmd_data *sec = dev_get_drvdata(dev);
//	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int size = 0;

	size += __snprintf(buf, size, "\n[Usage]\n");
	size += __snprintf(buf, size, " echo wr 0x1234 {value} > reg_ctrl\n");
	size += __snprintf(buf, size, " echo rd 0x1234 > reg_ctrl\n");
	size += __snprintf(buf, size, " echo rd 0x1234 0x111 > reg_ctrl, 0x111 is size(max 0x%X)\n",
		REG_BURST_MAX);

	return (ssize_t)size;
}

static int __store_reg_ctrl_wr(struct lxs_ts_data *ts, u32 addr, void *data, int size)
{
	return lxs_ts_reg_write(ts, addr, data, size);
}

static int __store_reg_ctrl_rd(struct lxs_ts_data *ts, u32 addr, void *data, int size)
{
	return lxs_ts_reg_read(ts, addr, data, size);
}

static int __store_reg_ctrl_rd_single(struct lxs_ts_data *ts, u32 addr, void *data, int size)
{
	u32 *__data = (u32 *)data;
	int __size;
	int ret = 0;

	while (size) {
		__size = min(4, size);
		ret = lxs_ts_reg_read(ts, addr, __data, __size);
		if (ret < 0) {
			break;
		}

		addr++;
		__data++;
		size -= __size;
	}

	return ret;
}

static int __store_reg_ctrl_rd_burst(struct lxs_ts_data *ts, u32 addr, int size, int burst)
{
	u8 *rd_buf, *row_buf;
	int col_power = REG_BURST_COL_PWR;
	int col_width = (1<<col_power);
	int row_curr, col_curr;
	int ret = 0;

	size = min(size, REG_BURST_MAX);

	rd_buf = (u8 *)kzalloc(size, GFP_KERNEL);
	if (rd_buf == NULL) {
		input_err(true, &ts->client->dev, "%s\n", "failed to allocate rd_buf");
		return -ENOMEM;
	}

	if (burst) {
		ret = __store_reg_ctrl_rd(ts, addr, rd_buf, size);
	} else {
		ret = __store_reg_ctrl_rd_single(ts, addr, rd_buf, size);
	}
	if (ret < 0) {
		goto out;
	}

	input_info(true, &ts->client->dev,
		"rd: addr %04Xh, size %Xh %s\n",
		addr, size, (burst) ? "(burst)" : "");

	row_buf = rd_buf;
	row_curr = 0;
	while (size) {
		col_curr = min(col_width, size);

		if (col_curr)
			input_info(true, &ts->client->dev, "rd: [%3Xh] %*ph\n", row_curr, col_curr, row_buf);

		row_buf += col_curr;
		row_curr += col_curr;
		size -= col_curr;
	}

out:
	kfree(rd_buf);

	return ret;
}

static ssize_t reg_ctrl_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char command[6] = {0};
	u32 reg = 0;
	u32 data = 1;
	u32 reg_addr;
	int wr = -1;
	int rd = -1;
	int value = 0;
	int ret = 0;

	if (sscanf(buf, "%5s %X %X", command, &reg, &value) <= 0) {
		input_err(true, &ts->client->dev, "%s\n", "Invalid param");
		return count;
	}

	if (!strcmp(command, "wr") || !strcmp(command, "write")) {
		wr = 1;
	} else if (!strcmp(command, "rd") || !strcmp(command, "read")) {
		rd = 1;		/* single */
	} else if (!strcmp(command, "rdb") || !strcmp(command, "readb")) {
		rd = 2;		/* burst */
	}

	reg_addr = reg;
	if (wr != -1) {
		data = value;
		ret = __store_reg_ctrl_wr(ts, reg_addr, &data, sizeof(data));
		if (ret >= 0) {
			input_info(true, &ts->client->dev,
				"wr: reg[0x%04X] = 0x%08X\n", reg_addr, data);
		}
		goto out;
	}

	if (rd != -1) {
		if (value <= 4) {
			ret = lxs_ts_read_value(ts, reg_addr, &data);
			if (ret >= 0) {
				input_info(true, &ts->client->dev,
					"rd: reg[0x%04X] = 0x%08X\n", reg_addr, data);
			}
		} else {
			ret = __store_reg_ctrl_rd_burst(ts, reg_addr, value, (rd == 2));
		}
		goto out;
	}

	input_info(true, &ts->client->dev, "%s\n", "[Usage]");
	input_info(true, &ts->client->dev, "%s\n", " echo wr 0x1234 {value} > reg_ctrl");
	input_info(true, &ts->client->dev, "%s\n", " echo rd 0x1234 > reg_ctrl");
	input_info(true, &ts->client->dev, "%s\n", " (burst access)");
	input_info(true, &ts->client->dev, " echo rd 0x1234 0x111 > reg_ctrl, 0x111 is size(max 0x%X)\n",
		REG_BURST_MAX);

out:
	return count;
}

static ssize_t dbg_mask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int size = 0;

	size += __snprintf(buf, size, "dbg_mask %08Xh\n", ts->dbg_mask);
	size += __snprintf(buf, size, "dbg_tc_delay %d\n", ts->dbg_tc_delay);
	size += __snprintf(buf, size, "ic_status_mask_error %08Xh\n", ts->ic_status_mask_error);
	size += __snprintf(buf, size, "ic_status_mask_abnormal %08Xh\n", ts->ic_status_mask_abnormal);

	size += __snprintf(buf, size, "\n[Usage]\n");
	size += __snprintf(buf, size, " echo 0 {mask value} > dbg_mask\n");
	size += __snprintf(buf, size, " echo 8 {delay value} > dbg_mask\n");

	return (ssize_t)size;
}

static void dbg_mask_store_help(struct device *dev)
{
	input_info(true, dev, "%s\n", "Usage:");
	input_info(true, dev, "%s\n", " echo 0 {mask value} > dbg_mask");
	input_info(true, dev, "%s\n", " echo 8 {delay value} > dbg_mask");
}

static ssize_t dbg_mask_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int type = 0;
	u32 old_value = 0;
	u32 new_value = 0;
	u32 sig = 0;

	if (sscanf(buf, "%d %X %X", &type, &new_value, &sig) <= 0) {
		input_err(true, dev, "%s\n", "Invalid param");
		return count;
	}

	switch (type) {
	case 0:
		old_value = ts->dbg_mask;
		ts->dbg_mask = new_value;
		input_info(true, dev, "dbg_mask changed : %08Xh -> %08Xh\n",
			old_value, new_value);
		break;
	case 8:
		old_value = ts->dbg_tc_delay;
		ts->dbg_tc_delay = new_value;
		input_info(true, dev, "dbg_tc_delay changed : %d -> %d\n",
			old_value, new_value);
		break;
	case 101:
		if (sig != 0x5A5A)
			break;

		old_value = ts->ic_status_mask_error;
		ts->ic_status_mask_error = new_value;
		input_info(true, dev, "ic_status_mask_error changed : %08Xh -> %08Xh\n",
			old_value, new_value);
		break;
	case 102:
		if (sig != 0x5A5A)
			break;

		old_value = ts->ic_status_mask_abnormal;
		ts->ic_status_mask_abnormal = new_value;
		input_info(true, dev, "ic_status_mask_abnormal changed : %08Xh -> %08Xh\n",
			old_value, new_value);
		break;
	default:
		dbg_mask_store_help(dev);
		break;
	}

	return count;
}

static ssize_t tc_driving_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int mode = ts->driving_mode;
	int size = 0;

	if (mode != TS_MODE_STOP)
		mode = is_ts_power_state_lpm(ts) ? TS_MODE_U0 : TS_MODE_U3;

	size += __snprintf(buf, size, "current driving mode is %s(%d)\n",
				ts_driving_mode_str(mode), mode);

	return (ssize_t)size;
}

static ssize_t tc_driving_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int mode = 0;

	if (sscanf(buf, "%d", &mode) <= 0) {
		input_err(true, dev, "%s\n", "Invalid param");
		return count;
	}

	switch (mode) {
	case TS_MODE_U0:
	case TS_MODE_U3:
	case TS_MODE_STOP:
		lxs_ts_driving(ts, mode);
		break;
	default:
		break;
	}

	return count;
}

static ssize_t tc_driving_opt1_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	u32 data = 0;
	int size = 0;
	int ret = 0;

	ret = lxs_ts_get_driving_opt1(ts, &data);
	if (ret < 0)
		size = __snprintf(buf, size, "failed to get TC_DRIVE_OPT1, %d\n", ret);
	else
		size = __snprintf(buf, size, "current TC_DRIVE_OPT1 is 0x%08X\n", data);

	return (ssize_t)size;
}

static ssize_t tc_driving_opt1_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	u32 data = 0;

	if (sscanf(buf, "%X", &data) <= 0) {
		input_err(true, dev, "%s\n", "Invalid param");
		return count;
	}

	lxs_ts_set_driving_opt1(ts, data);

	return count;
}

static ssize_t tc_driving_opt2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	u32 data = 0;
	int size = 0;
	int ret = 0;

	ret = lxs_ts_get_driving_opt2(ts, &data);
	if (ret < 0)
		size = __snprintf(buf, size, "failed to get TC_DRIVE_OPT2, %d\n", ret);
	else
		size = __snprintf(buf, size, "current TC_DRIVE_OPT2 is 0x%08X\n", data);

	return (ssize_t)size;
}

static ssize_t tc_driving_opt2_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	u32 data = 0;

	if (sscanf(buf, "%X", &data) <= 0) {
		input_err(true, dev, "%s\n", "Invalid param");
		return count;
	}

	lxs_ts_set_driving_opt2(ts, data);

	return count;
}

static ssize_t channel_info_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int size = 0;

	input_info(true, &ts->client->dev, "%s: tx %d, rx %d, res_x %d res_y %d, max_x %d, max_y %d\n",
		__func__, ts->tx_count, ts->rx_count, ts->res_x, ts->res_y, ts->max_x, ts->max_y);

	size += snprintf(buf + size, SEC_CMD_BUF_SIZE - size,
			"tx %d, rx %d, res_x %d res_y %d, max_x %d, max_y %d\n",
			ts->tx_count, ts->rx_count, ts->res_x, ts->res_y, ts->max_x, ts->max_y);

	return (ssize_t)size;
}

static ssize_t scrub_position_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[256] = { 0, };

	input_info(true, &ts->client->dev,
			"%s: id: %d, X:%d, Y:%d\n", __func__,
			ts->gesture_id, ts->gesture_x, ts->gesture_y);

	snprintf(buff, sizeof(buff), "%d %d %d",
			ts->gesture_id, ts->gesture_x, ts->gesture_y);

	ts->gesture_x = 0;
	ts->gesture_y = 0;

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

/*
 * read_support_feature function
 * returns the bit combination of specific feature that is supported.
 */
static ssize_t read_support_feature(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[64] = { 0, };
	int size = 0;
	u32 feature = 0;

	if (ts->plat_data->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;
	size += snprintf(buff + size, sizeof(buff) - size, "%s",
			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "");

	if (ts->plat_data->enable_sysinput_enabled)
		feature |= INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED;
	size += snprintf(buff + size, sizeof(buff) - size, "%s",
			feature & INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED ? " SE" : "");

#if defined(INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED)
	if (ts->plat_data->prox_lp_scan_enabled)
		feature |= INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED;
	size += snprintf(buff + size, sizeof(buff) - size, "%s",
			feature & INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED ? " LPSCAN" : "");
#endif

	input_info(true, &ts->client->dev, "%s: %d%s\n",
		__func__, feature, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int prox_power_off = ts->prox_power_off;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
#if defined(USE_PROXIMITY)
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int value;
	int ret;

	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, value);

	mutex_lock(&ts->device_mutex);
	ts->prox_power_off = value;
	mutex_unlock(&ts->device_mutex);
#endif

	return count;
}

static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int prox_state = ts->prox_state;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, prox_state);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", prox_state != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int value;
	int ret;

	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	input_info(true, &ts->client->dev, "%s: set ear detect %d\n", __func__, value);

	if (!ts->plat_data->support_ear_detect) {
		input_err(true, &ts->client->dev, "%s: not support ear detection\n", __func__);
		return count;
	}

	switch (value) {
	case 0:
	case 1:
	case 3:
		ts->ear_detect_mode = value;
		break;
	default:
		input_err(true, &ts->client->dev,
			"%s: invalid parameter %d\n", __func__, value);
		return count;
	}

	if (!is_ts_power_state_on(ts)) {
		input_err(true, &ts->client->dev, "%s: power is not on\n", __func__);
		return count;
	}

	mutex_lock(&ts->device_mutex);
	lxs_ts_mode_switch(ts, MODE_SWITCH_EAR_DETECT_ENABLE, value);
	mutex_unlock(&ts->device_mutex);

	return count;
}

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	u32 data[5] = {0, };
	s16 diff[9] = {0, };

	mutex_lock(&ts->device_mutex);
	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ts->sensitivity_mode);
	if (ts->sensitivity_mode) {
		lxs_ts_write_value(ts, SENSITIVITY_GET, 1);
		lxs_ts_delay(100);
		lxs_ts_reg_read(ts, SENSITIVITY_DATA, (void *)data, sizeof(data));
		memcpy(diff, data, sizeof(diff));
	}
	mutex_unlock(&ts->device_mutex);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d,%d,%d,%d,%d,%d,%d,%d,%d",
		diff[0], diff[1], diff[2], diff[3], diff[4], diff[5], diff[6], diff[7], diff[8]);
}

static ssize_t sensitivity_mode_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int value;
	int ret;

	ret = kstrtoint(buf, 10, &value);
	if (ret < 0)
		return ret;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, value);

	mutex_lock(&ts->device_mutex);
	ts->sensitivity_mode = value;
	mutex_unlock(&ts->device_mutex);

	return count;
}

static ssize_t enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	return scnprintf(buf, SEC_CMD_BUF_SIZE, "%d", ts->power_state);
}

/*
 * Screen Off:
 * 'echo 1,0 > enabled' -> MIPI for U0 -> 'echo 1,1 > enabled'
 *
 * Screen On:
 * 'echo 2,0 > enabled' -> MIPI for U3 -> 'echo 2,1 > enabled'
 */
static ssize_t enabled_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int buff[2] = { 0, };
	int ret = 0;

	ret = sscanf(buf, "%d,%d", &buff[0], &buff[1]);
	if (ret != 2) {
		input_err(true, &ts->client->dev,
			"%s: failed read params [%d]", __func__, ret);
		return size;
	}

	input_info(true, &ts->client->dev, "%s: %d %d\n", __func__, buff[0], buff[1]);

	/* handle same sequence : buff[0] = DISPLAY_STATE_ON, DISPLAY_STATE_DOZE, DISPLAY_STATE_DOZE_SUSPEND */
	if (buff[0] == DISPLAY_STATE_DOZE || buff[0] == DISPLAY_STATE_DOZE_SUSPEND)
		buff[0] = DISPLAY_STATE_ON;

	switch (buff[0]) {
	case DISPLAY_STATE_ON:
		switch (buff[1]) {
		case DISPLAY_EVENT_EARLY:
			input_info(true, &ts->client->dev, "%s: DISPLAY_STATE_ON(early)\n", __func__);
			lxs_ts_resume_early(ts);
			break;
		case DISPLAY_EVENT_LATE:
			input_info(true, &ts->client->dev, "%s: DISPLAY_STATE_ON\n", __func__);
			lxs_ts_resume(ts);
			break;
		}
		break;
	case DISPLAY_STATE_OFF:
		switch (buff[1]) {
		case DISPLAY_EVENT_EARLY:
			input_info(true, &ts->client->dev, "%s: DISPLAY_STATE_OFF(early)\n", __func__);
			lxs_ts_suspend_early(ts);
			break;
		case DISPLAY_EVENT_LATE:
			input_info(true, &ts->client->dev, "%s: DISPLAY_STATE_OFF\n", __func__);
			lxs_ts_suspend(ts);
			break;
		}
		break;
	case DISPLAY_STATE_LPM_OFF:
		input_info(true, &ts->client->dev, "%s: enter sleep mode in lpcharge\n", __func__);
		lxs_ts_suspend_early(ts);	/* TBD */
		lxs_ts_suspend(ts);
		break;
	}

	return size;
}

static ssize_t lp_dump_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	lxs_ts_lpwg_dump_buf_read(ts, buf);

	return strlen(buf);
}

static DEVICE_ATTR(fwup, 0664, NULL, fwup_store);
static DEVICE_ATTR(reset_ctrl, 0664, NULL, reset_ctrl_store);
static DEVICE_ATTR(reg_ctrl, 0664, reg_ctrl_show, reg_ctrl_store);
static DEVICE_ATTR(dbg_mask, 0664, dbg_mask_show, dbg_mask_store);

static DEVICE_ATTR(tc_driving, 0664, tc_driving_show, tc_driving_store);
static DEVICE_ATTR(tc_driving_opt1, 0664, tc_driving_opt1_show, tc_driving_opt1_store);
static DEVICE_ATTR(tc_driving_opt2, 0664, tc_driving_opt2_show, tc_driving_opt2_store);
static DEVICE_ATTR(channel_info, 0444, channel_info_show, NULL);

static DEVICE_ATTR(scrub_pos, 0444, scrub_position_show, NULL);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
static DEVICE_ATTR(prox_power_off, 0664, prox_power_off_show, prox_power_off_store);

static DEVICE_ATTR(virtual_prox, 0664, protos_event_show, protos_event_store);
static DEVICE_ATTR(sensitivity_mode, 0664, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(enabled, 0664, enabled_show, enabled_store);

static DEVICE_ATTR(get_lp_dump, 0444, lp_dump_show, NULL);

static struct attribute *cmd_attributes[] = {
	&dev_attr_fwup.attr,
	&dev_attr_reset_ctrl.attr,
	&dev_attr_reg_ctrl.attr,
	&dev_attr_dbg_mask.attr,
	/* */
	&dev_attr_tc_driving.attr,
	&dev_attr_tc_driving_opt1.attr,
	&dev_attr_tc_driving_opt2.attr,
	&dev_attr_channel_info.attr,
	/* */
	&dev_attr_scrub_pos.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_virtual_prox.attr,
	&dev_attr_sensitivity_mode.attr,
	/* */
	&dev_attr_enabled.attr,
	/* */
	&dev_attr_get_lp_dump.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[64] = { 0, };
	int do_reset = 0;
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		ret = -EIO;
		goto out;
	}

	mutex_lock(&ts->modechange);

	switch (sec->cmd_param[0]) {
	case TSP_BUILT_IN:
		mutex_lock(&ts->device_mutex);
		ret = lxs_ts_load_fw_from_ts_bin(ts);
		mutex_unlock(&ts->device_mutex);
		do_reset = 1;
		break;
	case TSP_UMS:
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		ret = lxs_ts_load_fw_from_external(ts, TSP_EXTERNAL_FW_SIGNED);
#else
		ret = lxs_ts_load_fw_from_external(ts, TSP_EXTERNAL_FW);
#endif
		do_reset = 1;
		break;
	default:
		input_err(true, &ts->client->dev, "%s: not supported command(%d)\n",
			__func__, sec->cmd_param[0]);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&ts->modechange);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	if (do_reset)
		lxs_ts_ic_reset(ts);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[16] = { 0, };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "LX%08X", ts->fw.ver_code_bin);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[16] = { 0, };
	char model[16] = { 0, };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
		return;
	}

	snprintf(buff, sizeof(buff), "LX%08X", ts->fw.ver_code_dev);
	snprintf(model, sizeof(model), "LX%04X", ts->fw.ver_code_dev>>16);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[16] = { 0, };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%d", ts->rx_count);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[16] = { 0, };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%d", ts->tx_count);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[16] = { 0, };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "LXS");

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[16] = { 0, };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), CHIP_DEVICE_NAME);

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[16] = { 0, };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_wirelesscharger_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is of\n", __func__);
		ret = -EIO;
		goto out;
	}

	mutex_lock(&ts->device_mutex);
	ret = lxs_ts_mode_switch(ts, MODE_SWITCH_WIRELESS_CHARGER, sec->cmd_param[0]);
	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};

#if defined(USE_LOWPOWER_MODE)
static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (!ts->plat_data->enable_settings_aot) {
		input_err(true, &ts->client->dev, "%s: not support aot\n", __func__);
		ret = -EPERM;
		goto out;
	}

	ts->aot_enable = sec->cmd_param[0];

	if (sec->cmd_param[0])
		ts->lowpower_mode |= SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;
	else
		ts->lowpower_mode &= ~SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP;

	input_info(true, &ts->client->dev, "%s: AOT %s, LPM 0x%02X\n",
		__func__, sec->cmd_param[0] ? "on" : "off", ts->lowpower_mode);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (!ts->plat_data->support_spay) {
		input_err(true, &ts->client->dev, "%s: not support spay\n", __func__);
		ret = -EPERM;
		goto out;
	}

	ts->spay_enable = sec->cmd_param[0];

	if (sec->cmd_param[0])
		ts->lowpower_mode |= SEC_TS_MODE_SPONGE_SWIPE;
	else
		ts->lowpower_mode &= ~SEC_TS_MODE_SPONGE_SWIPE;

	input_info(true, &ts->client->dev, "%s: SPAY %s, LPM 0x%02X\n",
		__func__, sec->cmd_param[0] ? "on" : "off", ts->lowpower_mode);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}
#endif

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ret = lxs_ts_mode_switch(ts, MODE_SWITCH_SIP_MODE, sec->cmd_param[0]);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ret = lxs_ts_mode_switch(ts, MODE_SWITCH_GAME_MODE, sec->cmd_param[0]);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void glove_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ret = lxs_ts_mode_switch(ts, MODE_SWITCH_GLOVE_MODE, sec->cmd_param[0]);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void clear_cover_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	switch (sec->cmd_param[0]) {
	case 0:
		input_info(true, &ts->client->dev,
			"%s: cover off.\n", __func__);
		break;
	case 3:
		input_info(true, &ts->client->dev,
			"%s: cover on. (cover type %d)\n", __func__, sec->cmd_param[1]);
		break;
	default:
		input_err(true, &ts->client->dev,
			"%s: invalid parameter %d\n", __func__, sec->cmd_param[0]);
		ret = -EINVAL;
		goto out;
	}

	sec_cmd_set_default_result(sec);

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ts->cover_mode = (sec->cmd_param[0]) ? sec->cmd_param[1] : 0;

	if (!is_ts_power_state_off(ts))
		ret = lxs_ts_mode_switch(ts, MODE_SWITCH_COVER_MODE, 0);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
};

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ret = lxs_ts_mode_switch(ts, MODE_SWITCH_DEAD_ZONE_ENABLE, sec->cmd_param[0]);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (!ts->plat_data->support_ear_detect) {
		input_err(true, &ts->client->dev, "%s: not support ear detection\n", __func__);
		goto out;
	}

	switch (sec->cmd_param[0]) {
	case 0:
	case 1:
	case 3:
		ts->ear_detect_mode = sec->cmd_param[0];
		break;
	default:
		input_err(true, &ts->client->dev,
			"%s: invalid parameter %d\n", __func__, sec->cmd_param[0]);
		goto out;
	}

	if (!is_ts_power_state_on(ts)) {
		ts->ed_reset_flag = true;
		input_err(true, &ts->client->dev, "%s: power is not on\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ret = lxs_ts_mode_switch(ts, MODE_SWITCH_EAR_DETECT_ENABLE, sec->cmd_param[0]);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void prox_lp_scan_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (!is_ts_power_state_lpm(ts)) {
		input_err(true, &ts->client->dev, "%s: power is is not lpm\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (ts->op_state != OP_STATE_PROX) {
		input_err(true, &ts->client->dev, "%s: not OP_STATE_PROX\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ret = lxs_ts_mode_switch(ts, MODE_SWITCH_PROX_LP_SCAN_MODE, sec->cmd_param[0]);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

#if defined(USE_GRIP_DATA)
static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	struct lxs_ts_grip_data *grip_data = &ts->plat_data->grip_data;
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X, 0x%X\n",
		__func__, sec->cmd_param[0], sec->cmd_param[1], sec->cmd_param[2], sec->cmd_param[3],
		sec->cmd_param[4], sec->cmd_param[5], sec->cmd_param[6], sec->cmd_param[7]);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	switch (sec->cmd_param[0] == 0) {
	case 0:
		input_info(true, &ts->client->dev, "%s: GRPI_EDGE_HANDLER\n", __func__);

		grip_data->exception_direction = sec->cmd_param[1];
		grip_data->exception_upper_y = sec->cmd_param[2];
		grip_data->exception_lower_y = sec->cmd_param[3];

		ret = lxs_ts_mode_switch(ts, MODE_SWITCH_GRIP_EDGE_HANDLER, 0);
		break;
	case 1:
		input_info(true, &ts->client->dev, "%s: GRIP_PORTRAIT_MODE\n", __func__);

		grip_data->portrait_mode = sec->cmd_param[1];
		grip_data->portrait_upper = sec->cmd_param[2];
		grip_data->portrait_lower = sec->cmd_param[3];
		grip_data->portrait_boundary_y = sec->cmd_param[4];

		ret = lxs_ts_mode_switch(ts, MODE_SWITCH_GRIP_PORTRAIT_MODE, 0);
		break;
	case 2:
		input_info(true, &ts->client->dev, "%s: GRIP_LANDSCAPE_MODE\n", __func__);

		grip_data->landscape_mode = sec->cmd_param[1];
		grip_data->landscape_reject_bl = sec->cmd_param[2];
		grip_data->landscape_grip_bl = sec->cmd_param[3];
		grip_data->landscape_reject_ts = sec->cmd_param[4];
		grip_data->landscape_reject_bs = sec->cmd_param[5];
		grip_data->landscape_grip_ts = sec->cmd_param[6];
		grip_data->landscape_grip_bs = sec->cmd_param[7];

		ret = lxs_ts_mode_switch(ts, MODE_SWITCH_GRIP_LANDSCAPE_MODE, 0);
		break;
	default:
		input_err(true, &ts->client->dev,
			"%s: not support mode, cmd_param[0]=0x02X\n", __func__, sec->cmd_param[0]);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void set_touchable_area(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ret = lxs_ts_mode_switch(ts, MODE_SWITCH_SET_TOUCHABLE_AREA, sec->cmd_param[0]);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

}
#endif	/* USE_GRIP_DATA */

static void set_note_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ret = lxs_ts_mode_switch(ts, MODE_SWITCH_SET_NOTE_MODE, sec->cmd_param[0]);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void __run_test_post(struct lxs_ts_data *ts)
{
#if defined(USE_FW_MP)
	atomic_set(&ts->fwsts, 0);
	lxs_ts_load_fw_from_ts_bin(ts);
#endif
	lxs_ts_ic_reset_sync(ts);
}

static void __run_read(struct lxs_ts_data *ts, struct sec_cmd_data *sec,
	const char *fname, char *title, int is_lpm, int load_fw, int opt, void *__run_op)
{
	int (*run_op)(void *ts_data, int *min, int *max, char *prt_buf, int opt) = __run_op;
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int min = 0;
	int max = 0;
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", fname);

	sec_cmd_set_default_result(sec);

	if (is_lpm) {
		if (!is_ts_power_state_lpm(ts)) {
			input_err(true, &ts->client->dev, "%s: power is not lpm\n", fname);
			ret = -EIO;
			goto out;
		}
	} else {
		if (!is_ts_power_state_on(ts)) {
			input_err(true, &ts->client->dev, "%s: power is not on\n", fname);
			ret = -EIO;
			goto out;
		}
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", fname);
		ret = -EBUSY;
		goto out;
	}

	if (load_fw) {
		lxs_ts_irq_enable(ts, false);
		lxs_ts_driving(ts, TS_MODE_STOP);
		lxs_ts_load_fw_from_mp_bin(ts);
	}

	if (run_op)
		ret = run_op(ts, &min, &max, NULL, opt);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "%d,%d", min, max);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), title);

	input_info(true, &ts->client->dev, "%s: %s\n", fname, buff);

	if (load_fw && (ret >= 0)) {
		__run_test_post(ts);
	}
}

static void __run_read_all(struct lxs_ts_data *ts, struct sec_cmd_data *sec,
	const char *fname, char *title, int is_lpm, void *__run_op)
{
	int (*run_op)(void *ts_data, int *min, int *max, char *prt_buf, int opt) = __run_op;
	char buff[SEC_CMD_STR_LEN] = { 0, };
	char *prt_buf = NULL;
	int min = 0;
	int max = 0;
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", fname);

	sec_cmd_set_default_result(sec);

	if (is_lpm) {
		if (!is_ts_power_state_lpm(ts)) {
			input_err(true, &ts->client->dev, "%s: power is not lpm\n", fname);
			ret = -EIO;
			goto out;
		}
	} else {
		if (!is_ts_power_state_on(ts)) {
			input_err(true, &ts->client->dev, "%s: power is not on\n", fname);
			ret = -EIO;
			goto out;
		}
	}

	if (!ts->run_prt_size) {
		input_err(true, &ts->client->dev,
			"%s: run_prt_size is zero\n", fname);
		ret = -EINVAL;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", fname);
		ret = -EBUSY;
		goto out;
	}

	prt_buf = kzalloc(ts->run_prt_size, GFP_KERNEL);
	if (prt_buf == NULL) {
		input_err(true, &ts->client->dev,
			"%s: kzalloc failed\n", fname);
		mutex_unlock(&ts->device_mutex);
		ret = -ENOMEM;
		goto out;
	}

	lxs_ts_irq_enable(ts, false);
	lxs_ts_driving(ts, TS_MODE_STOP);
	lxs_ts_load_fw_from_mp_bin(ts);

	if (run_op)
		ret = run_op(ts, &min, &max, prt_buf, 0);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	} else {
		sec_cmd_set_cmd_result(sec, prt_buf, strlen(prt_buf));
		sec->cmd_state = SEC_CMD_STATUS_OK;

		snprintf(buff, sizeof(buff), "OK");
	}

	if (prt_buf)
		kfree(prt_buf);

	input_info(true, &ts->client->dev, "%s: %s\n", fname, buff);

	if (ret >= 0) {
		__run_test_post(ts);
	}
}

static void __run_read_np(struct lxs_ts_data *ts, struct sec_cmd_data *sec,
	const char *fname, char *title, int load_fw, int opt, void *__run_op)
{
	__run_read(ts, sec, fname, title, 0, load_fw, opt, __run_op);
}

static void __run_read_np_all(struct lxs_ts_data *ts, struct sec_cmd_data *sec,
	const char *fname, char *title, void *__run_op)
{
	__run_read_all(ts, sec, fname, title, 0, __run_op);
}

static void __run_read_lp(struct lxs_ts_data *ts, struct sec_cmd_data *sec,
	const char *fname, char *title, int load_fw, int opt, void *__run_op)
{
	__run_read(ts, sec, fname, title, 1, load_fw, opt, __run_op);
}

static void __run_read_lp_all(struct lxs_ts_data *ts, struct sec_cmd_data *sec,
	const char *fname, char *title, void *__run_op)
{
	__run_read_all(ts, sec, fname, title, 1, __run_op);
}

static void __run_open_read(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_np(ts, sec, __func__, "OPEN_RAW", load_fw, opt, ts->run_open);
}

static void run_open_read(void *device_data)
{
	__run_open_read(device_data, 1);
}

static void run_open_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_np_all(ts, sec, __func__, "OPEN_RAW", ts->run_open);
}

static void __run_short_mux_read(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_np(ts, sec, __func__, "SHORT_MUX", load_fw, opt, ts->run_short_mux);
}

static void run_short_mux_read(void *device_data)
{
	__run_short_mux_read(device_data, 1);
}

static void run_short_mux_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_np_all(ts, sec, __func__, "SHORT_MUX", ts->run_short_mux);
}

static void __run_short_ch_read(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = (load_fw) ? 0 : BIT(0);

	__run_read_np(ts, sec, __func__, "SHORT_CH", load_fw, opt, ts->run_short_ch);
}

static void run_short_ch_read(void *device_data)
{
	__run_short_ch_read(device_data, 1);
}

static void run_short_ch_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_np_all(ts, sec, __func__, "SHORT_CH", ts->run_short_ch);
}

static void __run_np_raw_m1_read(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_np(ts, sec, __func__, "FW_RAW_M1_DATA", load_fw, opt, ts->run_np_m1_raw);
}

static void run_np_raw_m1_read(void *device_data)
{
	__run_np_raw_m1_read(device_data, 1);
}

static void run_np_raw_m1_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_np_all(ts, sec, __func__, "FW_RAW_M1_DATA", ts->run_np_m1_raw);
}

static void __run_np_raw_m1_jitter(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_np(ts, sec, __func__, "FW_RAW_M1_JITTER", load_fw, opt, ts->run_np_m1_jitter);
}

static void run_np_raw_m1_jitter(void *device_data)
{
	__run_np_raw_m1_jitter(device_data, 1);
}

static void run_np_raw_m1_jitter_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_np_all(ts, sec, __func__, "FW_RAW_M1_JITTER", ts->run_np_m1_jitter);
}

static void __run_np_raw_read(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_np(ts, sec, __func__, "FW_RAW_DATA", load_fw, opt, ts->run_np_m2_raw);
}

static void run_np_raw_read(void *device_data)
{
	__run_np_raw_read(device_data, 1);
}

static void run_np_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_np_all(ts, sec, __func__, "FW_RAW_DATA", ts->run_np_m2_raw);
}

static void __run_np_raw_gap_x(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = (load_fw) ? 0 : BIT(0);

	__run_read_np(ts, sec, __func__, "FW_RAW_GAP_X", load_fw, opt, ts->run_np_m2_gap_x);
}

static void run_np_raw_gap_x(void *device_data)
{
	__run_np_raw_gap_x(device_data, 1);
}

static void run_np_raw_gap_x_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_np_all(ts, sec, __func__, "FW_RAW_GAP_X", ts->run_np_m2_gap_x);
}

static void __run_np_raw_gap_y(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = (load_fw) ? 0 : BIT(0);

	__run_read_np(ts, sec, __func__, "FW_RAW_GAP_Y", load_fw, opt, ts->run_np_m2_gap_y);
}

static void run_np_raw_gap_y(void *device_data)
{
	__run_np_raw_gap_y(device_data, 1);
}

static void run_np_raw_gap_y_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_np_all(ts, sec, __func__, "FW_RAW_GAP_Y", ts->run_np_m2_gap_y);
}

static void __run_np_raw_jitter(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_np(ts, sec, __func__, "FW_RAW_JITTER", load_fw, opt, ts->run_np_m2_jitter);
}

static void run_np_raw_jitter(void *device_data)
{
	__run_np_raw_jitter(device_data, 1);
}

static void run_np_raw_jitter_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_np_all(ts, sec, __func__, "FW_RAW_JITTER", ts->run_np_m2_jitter);
}

static void __run_sram_test(void *device_data, int do_post)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	char test[32] = { 0, };
	char result[32] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (is_ts_power_state_off(ts)) {
		input_err(true, &ts->client->dev, "%s: power is off\n", __func__);
		ret = -EIO;
		goto out;
	}

	if (mutex_lock_interruptible(&ts->device_mutex)) {
		input_err(true, &ts->client->dev,
			"%s: another task is running\n", __func__);
		ret = -EBUSY;
		goto out;
	}

	ret = lxs_ts_sram_test(ts, 1);

	mutex_unlock(&ts->device_mutex);

out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(result, sizeof(result), "RESULT=FAIL");
	} else {
		snprintf(buff, sizeof(buff), "%d", TEST_RESULT_PASS);
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(result, sizeof(result), "RESULT=PASS");
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");

	sec_cmd_send_event_to_user(&ts->sec, test, result);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);

	if (do_post)
		__run_test_post(ts);
}

static void run_sram_test(void *device_data)
{
	__run_sram_test(device_data, 1);
}

static void __run_lp_raw_m1_read(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_lp(ts, sec, __func__, "LP_RAW_M1_DATA", load_fw, opt, ts->run_lp_m1_raw);
}

static void run_lp_raw_m1_read(void *device_data)
{
	__run_lp_raw_m1_read(device_data, 1);
}

static void run_lp_raw_m1_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_lp_all(ts, sec, __func__, "LP_RAW_M1_DATA", ts->run_lp_m1_raw);
}

static void __run_lp_raw_m1_jitter(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_lp(ts, sec, __func__, "LP_RAW_M1_JITTER", load_fw, opt, ts->run_lp_m1_jitter);
}

static void run_lp_raw_m1_jitter(void *device_data)
{
	__run_lp_raw_m1_jitter(device_data, 1);
}

static void run_lp_raw_m1_jitter_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_lp_all(ts, sec, __func__, "LP_RAW_M1_JITTER", ts->run_lp_m1_jitter);
}

static void __run_lp_raw_read(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_lp(ts, sec, __func__, "LP_RAW_DATA", load_fw, opt, ts->run_lp_m2_raw);
}

static void run_lp_raw_read(void *device_data)
{
	__run_lp_raw_read(device_data, 1);
}

static void run_lp_raw_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_lp_all(ts, sec, __func__, "LP_RAW_DATA", ts->run_lp_m2_raw);
}

static void __run_lp_raw_jitter(void *device_data, int load_fw)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	int opt = 0;

	__run_read_lp(ts, sec, __func__, "LP_RAW_JITTER", load_fw, opt, ts->run_lp_m2_jitter);
}

static void run_lp_raw_jitter(void *device_data)
{
	__run_lp_raw_jitter(device_data, 1);
}

static void run_lp_raw_jitter_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	__run_read_lp_all(ts, sec, __func__, "LP_RAW_JITTER", ts->run_lp_m2_jitter);
}

static void run_rawdata_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_raw_data_clear();

	lxs_ts_irq_enable(ts, false);
	lxs_ts_driving(ts, TS_MODE_STOP);

	input_raw_info_d(true, &ts->client->dev,
		"%s: start (noise:%d, wet:%d, tc:%d)##\n",
		__func__, ts->touch_noise_status, ts->wet_mode, ts->touch_count);

	__run_open_read(sec, 0);
	__run_short_mux_read(sec, 0);
	__run_short_ch_read(sec, 0);
	__run_np_raw_m1_read(sec, 0);
	__run_np_raw_m1_jitter(sec, 0);
	__run_np_raw_read(sec, 0);
	__run_np_raw_jitter(sec, 0);

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	__run_test_post(ts);

	input_raw_info_d(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

void lxs_ts_run_rawdata_all(struct lxs_ts_data *ts)
{
	struct sec_cmd_data *sec = &ts->sec;

	run_rawdata_read_all(sec);
}


static int factory_check_exception(struct lxs_ts_data *ts, const char *fname)
{
	if (atomic_read(&ts->boot) == TC_IC_BOOT_FAIL) {
		input_err(true, &ts->client->dev, "%s: boot failed\n", fname);
		return 1;
	}

	if (atomic_read(&ts->init) != TC_IC_INIT_DONE) {
		input_err(true, &ts->client->dev, "%s: not ready, need IC init\n", fname);
		return 2;
	}

	return 0;
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	sec->item_count = 0;

	memset(sec->cmd_result_all, 0, SEC_CMD_RESULT_STR_LEN);

	if (factory_check_exception(ts, __func__)) {
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (!is_ts_power_state_on(ts)) {
		input_err(true, &ts->client->dev, "%s: power is not on\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	lxs_ts_irq_enable(ts, false);
	lxs_ts_driving(ts, TS_MODE_STOP);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);

	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	lxs_ts_load_fw_from_mp_bin(ts);

	__run_open_read(sec, 0);
	__run_short_mux_read(sec, 0);
	__run_short_ch_read(sec, 0);
	__run_np_raw_m1_read(sec, 0);
	__run_np_raw_m1_jitter(sec, 0);
	__run_np_raw_read(sec, 0);
	__run_np_raw_jitter(sec, 0);

	__run_sram_test(sec, 0);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %d%s\n",
		__func__, sec->item_count, sec->cmd_result_all);

	__run_test_post(ts);

	lxs_ts_delay(100);
}

static void factory_lcdoff_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);

	sec->item_count = 0;

	memset(sec->cmd_result_all, 0, SEC_CMD_RESULT_STR_LEN);

	if (factory_check_exception(ts, __func__)) {
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	if (!is_ts_power_state_lpm(ts)) {
		input_err(true, &ts->client->dev, "%s: power is not lpm\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	lxs_ts_irq_enable(ts, false);
	lxs_ts_driving(ts, TS_MODE_STOP);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	lxs_ts_load_fw_from_mp_bin(ts);

	__run_lp_raw_m1_read(sec, 0);
	__run_lp_raw_m1_jitter(sec, 0);
	__run_lp_raw_read(sec, 0);
	__run_lp_raw_jitter(sec, 0);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, &ts->client->dev, "%s: %d%s\n",
		__func__, sec->item_count, sec->cmd_result_all);

	__run_test_post(ts);

	lxs_ts_delay(100);
}

static void incell_power_control(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, &ts->client->dev, "%s: invalid parameter %d\n",
			__func__, sec->cmd_param[0]);
		ret = -EINVAL;
		goto out;
	}

	ts->lcdoff_test = !!sec->cmd_param[0];

out:
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

static void debug(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0, };

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	ts->debug_flag = sec->cmd_param[0];
	input_info(true, &ts->client->dev, "%s: command is %d\n", __func__, ts->debug_flag);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

	sec->cmd_state = SEC_CMD_STATUS_WAITING;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);
	char buff[16] = { 0, };

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%s", "NA");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

	sec_cmd_set_cmd_exit(sec);

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, buff);
}

struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_module_vendor", not_support_cmd),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("set_wirelesscharger_mode", set_wirelesscharger_mode),},
#if defined(USE_LOWPOWER_MODE)
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("spay_enable", spay_enable),},
#endif
	{SEC_CMD_H("set_sip_mode", set_sip_mode),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
	{SEC_CMD_H("glove_mode", glove_mode),},
	{SEC_CMD_H("clear_cover_mode", clear_cover_mode),},
	{SEC_CMD_H("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("prox_lp_scan_mode", prox_lp_scan_mode),},
#if defined(USE_GRIP_DATA)
	{SEC_CMD_H("set_grip_data", set_grip_data),},
	{SEC_CMD_H("set_touchable_area", set_touchable_area),},
#endif
	{SEC_CMD_H("set_note_mode", set_note_mode),},
	/* */
	{SEC_CMD("run_open_read", run_open_read),},
	{SEC_CMD("run_open_read_all", run_open_read_all),},
	{SEC_CMD("run_short_mux_read", run_short_mux_read),},
	{SEC_CMD("run_short_mux_read_all", run_short_mux_read_all),},
	{SEC_CMD("run_short_ch_read", run_short_ch_read),},
	{SEC_CMD("run_short_ch_read_all", run_short_ch_read_all),},
	{SEC_CMD("run_np_raw_m1_read", run_np_raw_m1_read),},
	{SEC_CMD("run_np_raw_m1_read_all", run_np_raw_m1_read_all),},
	{SEC_CMD("run_np_raw_m1_jitter", run_np_raw_m1_jitter),},
	{SEC_CMD("run_np_raw_m1_jitter_all", run_np_raw_m1_jitter_all),},
	{SEC_CMD("run_np_raw_read", run_np_raw_read),},
	{SEC_CMD("run_np_raw_read_all", run_np_raw_read_all),},
	{SEC_CMD("run_np_raw_gap_x", run_np_raw_gap_x),},
	{SEC_CMD("run_np_raw_gap_x_all", run_np_raw_gap_x_all),},
	{SEC_CMD("run_np_raw_gap_y", run_np_raw_gap_y),},
	{SEC_CMD("run_np_raw_gap_y_all", run_np_raw_gap_y_all),},
	{SEC_CMD("run_np_raw_jitter", run_np_raw_jitter),},
	{SEC_CMD("run_np_raw_jitter_all", run_np_raw_jitter_all),},
	{SEC_CMD("run_sram_test", run_sram_test),},
	{SEC_CMD("run_lp_raw_m1_read", run_lp_raw_m1_read),},
	{SEC_CMD("run_lp_raw_m1_read_all", run_lp_raw_m1_read_all),},
	{SEC_CMD("run_lp_raw_m1_jitter", run_lp_raw_m1_jitter),},
	{SEC_CMD("run_lp_raw_m1_jitter_all", run_lp_raw_m1_jitter_all),},
	{SEC_CMD("run_lp_raw_read", run_lp_raw_read),},
	{SEC_CMD("run_lp_raw_read_all", run_lp_raw_read_all),},
	{SEC_CMD("run_lp_raw_jitter", run_lp_raw_jitter),},
	{SEC_CMD("run_lp_raw_jitter_all", run_lp_raw_jitter_all),},
	{SEC_CMD("run_rawdata_read_all_for_ghost", run_rawdata_read_all),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("factory_lcdoff_cmd_result_all", factory_lcdoff_cmd_result_all),},
	{SEC_CMD("incell_power_control", incell_power_control),},
	{SEC_CMD("debug", debug),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

#define __LXS_TS_DEVT_TSP	SEC_CLASS_DEVT_TSP

int lxs_ts_cmd_init(struct lxs_ts_data *ts)
{
	int ret = 0;

	ret = sec_cmd_init(&ts->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), __LXS_TS_DEVT_TSP);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to sec_cmd_init, %d\n", __func__, ret);
		goto out;
	}

	ret = sysfs_create_group(&ts->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to create sysfs attributes, %d\n", __func__, ret);
		goto out_group;
	}

	ret = sysfs_create_link(&ts->sec.fac_dev->kobj,
			&ts->input_dev->dev.kobj, "input");
	if (ret < 0) {
		input_err(true, &ts->client->dev,
				"%s: Failed to create input symbolic link, %d\n",
				__func__, ret);
		goto out_link;
	}

	return 0;

out_link:
	sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);

out_group:
	sec_cmd_exit(&ts->sec, __LXS_TS_DEVT_TSP);

out:
	return ret;
}

void lxs_ts_cmd_free(struct lxs_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	sysfs_remove_link(&ts->sec.fac_dev->kobj, "input");

	sysfs_remove_group(&ts->sec.fac_dev->kobj, &cmd_attr_group);

	sec_cmd_exit(&ts->sec, __LXS_TS_DEVT_TSP);
}

MODULE_LICENSE("GPL");

