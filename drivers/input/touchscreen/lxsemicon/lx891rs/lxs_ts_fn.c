/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * lxs_ts_fn.c
 *
 * LXS touch sub-functions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "lxs_ts_dev.h"

#if defined(USE_PROXIMITY)
static int lxs_ts_set_prox_lp_scan_mode(struct lxs_ts_data *ts);
static int lxs_ts_set_ear_detect_enable(struct lxs_ts_data *ts);
#endif

/* BUS test : write, read, and compare */
static int lxs_ts_ic_test_unit(struct lxs_ts_data *ts, u32 data)
{
	u32 data_rd;
	int ret;

	ret = lxs_ts_write_value(ts, SPR_CHIP_TEST, data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "ic test wr err, %08Xh, %d\n", data, ret);
		goto out;
	}

	ret = lxs_ts_read_value(ts, SPR_CHIP_TEST, &data_rd);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "ic test rd err: %08Xh, %d\n", data, ret);
		goto out;
	}

	if (data != data_rd) {
		input_err(true, &ts->client->dev, "ic test cmp err, %08Xh, %08Xh\n", data, data_rd);
		ret = -EFAULT;
		goto out;
	}

out:
	return ret;
}

int lxs_ts_ic_test(struct lxs_ts_data *ts)
{
	u32 data[] = {
		0x5A5A5A5A,
		0xA5A5A5A5,
		0xF0F0F0F0,
		0x0F0F0F0F,
		0xFF00FF00,
		0x00FF00FF,
		0xFFFF0000,
		0x0000FFFF,
		0xFFFFFFFF,
		0x00000000,
	};
	int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(data); i++) {
		ret = lxs_ts_ic_test_unit(ts, data[i]);
		if (ret < 0)
			break;
	}

	if (!ret)
		input_info(true, &ts->client->dev, "%s\n", "ic bus r/w test done");

	return ret;
}

static int lxs_ts_tc_boot_status(struct lxs_ts_data *ts)
{
	u32 boot_failed = 0;
	u32 tc_status = 0;
	u32 valid_code_crc_mask = BIT(STS_POS_VALID_CODE_CRC);
	int ret = 0;

	ret = lxs_ts_read_value(ts, TC_STATUS, &tc_status);
	if (ret < 0)
		return ret;

	/* maybe nReset is low state */
	if (!tc_status || (tc_status == ~0))
		return BOOT_CHK_SKIP;

	if (valid_code_crc_mask && !(tc_status & valid_code_crc_mask))
		boot_failed |= (1<<4);

	if (boot_failed)
		input_err(true, &ts->client->dev, "boot fail: tc_status = %08Xh(%02Xh)\n",
			tc_status, boot_failed);

	return boot_failed;
}

static int lxs_ts_chk_boot(struct lxs_ts_data *ts)
{
	u32 boot_failed = 0;
	int retry;
	int ret = 0;

	retry = BOOT_CHK_FW_RETRY;
	while (retry--) {
		ret = lxs_ts_boot_result(ts);
		if (ret < 0)
			return ret;

		if (!ret)
			break;

		lxs_ts_delay(BOOT_CHK_FW_DELAY);
	}
	boot_failed |= ret;

	retry = BOOT_CHK_STS_RETRY;
	while (retry--) {
		ret = lxs_ts_tc_boot_status(ts);
		if (ret < 0)
			return ret;

		if (ret == BOOT_CHK_SKIP)
			return boot_failed;

		if (!ret)
			break;

		lxs_ts_delay(BOOT_CHK_STS_DELAY);
	}
	boot_failed |= ret;

//	return boot_failed;
	return 0;
}

void lxs_ts_init_recon(struct lxs_ts_data *ts)
{
	u32 data[2] = {0, };

	input_info(true, &ts->client->dev, "%s\n", __func__);

	lxs_ts_write_value(ts, SERIAL_DATA_OFFSET, 0xD08);
	lxs_ts_reg_write(ts, DATA_BASE_ADDR, data, sizeof(data));
}

void lxs_ts_init_ctrl(struct lxs_ts_data *ts)
{
	/* TBD */
}

static int lxs_ts_init_reg_set(struct lxs_ts_data *ts)
{
	int ret = 0;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ret = lxs_ts_write_value(ts, TC_DEVICE_CTL, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "failed to start chip, %d\n", ret);
		goto out;
	}

out:
	return ret;
}

static void lxs_ts_clock_con(struct lxs_ts_data *ts, int on)
{
	if (on) {
		/* TBD */
	} else {
		atomic_set(&ts->init, TC_IC_INIT_NEED);
		atomic_set(&ts->fwsts, 0);

		lxs_ts_write_value(ts, 0x080, 0);	//IRQ LOW

		lxs_ts_write_value(ts, 0xA00, 0);	//AFE(L) power down

		lxs_ts_write_value(ts, 0xB00, 0);	//AFE(R) power down

		lxs_ts_write_value(ts, 0x088, 0);	//OSC off

		input_info(true, &ts->client->dev, "%s: clock off\n", __func__);

		lxs_ts_delay(5);
	}
}

static int lxs_ts_get_driving_opt_x(struct lxs_ts_data *ts,
			u32 addr, u32 *data, char *name)
{
	u32 val = 0;
	int ret = 0;

	ret = lxs_ts_read_value(ts, addr, &val);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to get %s(0x%X)",
			__func__, name, addr);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: get %s(0x%X), 0x%08X\n",
		__func__, name, addr, val);

	if (data)
		*data = val;

	return 0;
}

static int lxs_ts_set_driving_opt_x(struct lxs_ts_data *ts,
			u32 addr, u32 data, char *name)
{
	int ret = 0;

	ret = lxs_ts_write_value(ts, addr, data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to set %s(0x%X)",
			__func__, name, addr);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: set %s(0x%X), 0x%08X\n",
		__func__, name, addr, data);

	return 0;
}

int lxs_ts_get_driving_opt1(struct lxs_ts_data *ts, u32 *data)
{
	return lxs_ts_get_driving_opt_x(ts, TC_DRIVE_OPT1, data, "TC_DRIVE_OPT1");
}

int lxs_ts_set_driving_opt1(struct lxs_ts_data *ts, u32 data)
{
	return lxs_ts_set_driving_opt_x(ts, TC_DRIVE_OPT1, data, "TC_DRIVE_OPT1");
}

int lxs_ts_get_driving_opt2(struct lxs_ts_data *ts, u32 *data)
{
	return lxs_ts_get_driving_opt_x(ts, TC_DRIVE_OPT2, data, "TC_DRIVE_OPT2");
}

int lxs_ts_set_driving_opt2(struct lxs_ts_data *ts, u32 data)
{
	return lxs_ts_set_driving_opt_x(ts, TC_DRIVE_OPT2, data, "TC_DRIVE_OPT2");
}

static int lxs_ts_get_mode_sig(struct lxs_ts_data *ts)
{
	u32 data = 0;
	int ret;

	if (is_ts_power_state_lpm(ts) && ts->reset_is_on_going)
		lxs_ts_delay(50);

	ret = lxs_ts_read_value(ts, TC_MODE_SIG, &data);
	if (ret < 0)
		return ret;

	if (is_ts_power_state_on(ts)) {
		if (data != MODE_SIG_NP) {
			input_err(true, &ts->client->dev,
				"%s: NP(0x%08X) invalid, 0x%08X\n",
				__func__, MODE_SIG_NP, data);
			return -EFAULT;
		}
		input_info(true, &ts->client->dev, "%s: NP(0x%08X)\n",
			__func__, MODE_SIG_NP);
		return 0;
	}

	if (is_ts_power_state_lpm(ts)) {
		switch (ts->ear_detect_mode) {
		case 3:
			if (data != MODE_SIG_LP_PROX3) {
				input_err(true, &ts->client->dev,
					"%s: PROX3(0x%08X) invalid, 0x%08X\n",
					__func__, MODE_SIG_LP_PROX3, data);
				return -EFAULT;
			}
			input_info(true, &ts->client->dev, "%s: PROX3(0x%08X)\n",
				__func__, MODE_SIG_LP_PROX3);
			break;
		case 1:
			if (data != MODE_SIG_LP_PROX1) {
				input_err(true, &ts->client->dev,
					"%s: PROX1(0x%08X) invalid, 0x%08X\n",
					__func__, MODE_SIG_LP_PROX1, data);
				return -EFAULT;
			}
			input_info(true, &ts->client->dev, "%s: PROX1(0x%08X)\n",
				__func__, MODE_SIG_LP_PROX1);
			break;
		default:
			if (data != MODE_SIG_LP) {
				input_err(true, &ts->client->dev,
					"%s: LP(0x%08X) invalid, 0x%08X\n",
					__func__, MODE_SIG_LP, data);
				return -EFAULT;
			}
			input_info(true, &ts->client->dev, "%s: LP(0x%08X)\n",
				__func__, MODE_SIG_LP);
			break;
		}

		return 0;
	}

	return 0;
}

int lxs_ts_ic_lpm(struct lxs_ts_data *ts)
{
	u32 data;
	int running_status;
	int ret;

	input_info(true, &ts->client->dev, "%s\n", __func__);

	ts->driving_mode = TS_MODE_U0;

	ret = lxs_ts_ic_test(ts);
	if (ret < 0)
		goto out;

	lxs_ts_get_mode_sig(ts);

	if (ts->dbg_tc_delay)
		lxs_ts_delay(ts->dbg_tc_delay);
	else
		lxs_ts_delay(10);

	ret = lxs_ts_read_value(ts, TC_STATUS, &data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "failed to get tc_status, %d\n", ret);
		goto out;
	}

	running_status = tc_sts_running_sts(data);
	input_info(true, &ts->client->dev, "command done: mode %d, running_sts %02Xh\n",
		ts->driving_mode, running_status);

out:
	if (ret < 0) {
		atomic_set(&ts->init, TC_IC_INIT_NEED);
		input_err(true, &ts->client->dev, "%s lpm failed, %d\n", CHIP_DEVICE_NAME, ret);
	} else {
		atomic_set(&ts->init, TC_IC_INIT_DONE);
		input_info(true, &ts->client->dev, "%s lpm done\n", CHIP_DEVICE_NAME);
	}

	return ret;
}

void lxs_ts_knock(struct lxs_ts_data *ts)
{
	int knock_on = !!(ts->aot_enable);
	int swipe_on = !!(ts->spay_enable);
	int ret = 0;

	ret = lxs_ts_write_value(ts, KNOCKON_ENABLE, knock_on);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: KNOCK(%d) failed, %d\n",
			__func__, knock_on, ret);
	else
		input_info(true, &ts->client->dev, "%s: KNOCK %s\n",
			__func__, knock_on ? "on" : "off");

	ret = lxs_ts_write_value(ts, SWIPE_ENABLE, swipe_on);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: SWIPE(%d) failed, %d\n",
			__func__, swipe_on, ret);
	else
		input_info(true, &ts->client->dev, "%s: SWIPE %s\n",
			__func__, swipe_on ? "on" : "off");
}

int lxs_ts_driving(struct lxs_ts_data *ts, int mode)
{
	u32 ctrl = -1;
	u32 addr;
	u32 data;
	u32 running_status;
	int ret = 0;

	switch (mode) {
	case TS_MODE_U3:
		ctrl = 0x181;
		break;
	case TS_MODE_U0:
		ctrl = 0x001;
		break;
	case TS_MODE_STOP:
		ctrl = 0x002;
		break;
	}

	if (ctrl < 0) {
		input_err(true, &ts->client->dev, "%s: mode(%d) not granted\n", __func__, mode);
		return -ESRCH;
	}

	input_info(true, &ts->client->dev, "current driving mode is %s\n", ts_driving_mode_str(mode));

	ts->driving_mode = mode;

	addr = TC_DRIVE_CTL;
	ret = lxs_ts_write_value(ts, addr, ctrl);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "TC Driving[%04Xh](0x%X) failed, %d\n",
				addr, ctrl, ret);
		return ret;
	}
	input_info(true, &ts->client->dev, "TC Driving[%04Xh] wr 0x%X\n", addr, ctrl);

	addr = SPR_SUBDISP_ST;
	ret = lxs_ts_read_value(ts, addr, &data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "DDI Display Mode[%04Xh] failed, %d\n",
				addr, ret);
		return ret;
	}
	input_info(true, &ts->client->dev, "DDI Display Mode[%04Xh] = 0x%X\n", addr, data);

	/* debugging */
	if (ts->dbg_tc_delay)
		lxs_ts_delay(ts->dbg_tc_delay);
	else
		lxs_ts_delay(10);

	ret = lxs_ts_read_value(ts, TC_STATUS, &data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "failed to get tc_status, %d\n", ret);
		return ret;
	}

	running_status = tc_sts_running_sts(data);

	if (mode == TS_MODE_STOP) {
		if (running_status)
			ret = -EFAULT;
	} else {
		if (!running_status)
			ret = -EFAULT;
	}

	if (ret < 0)
		input_err(true, &ts->client->dev, "command failed: mode %d, tc_status %08Xh\n",
			mode, data);
	else
		input_info(true, &ts->client->dev, "command done: mode %d, running_sts %02Xh\n",
			mode, running_status);

	return ret;
}

int lxs_ts_ic_info(struct lxs_ts_data *ts)
{
	struct lxs_ts_fw_info *fw = &ts->fw;
	u32 product[2] = {0};
	u32 chip_id = 0;
	u32 version = 0;
	u32 dev_code = 0;
	u32 revision = 0;
	u32 vchip, vproto;
	int ret = 0;

	ret = lxs_ts_read_value(ts, SPR_CHIP_ID, &chip_id);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "failed to get chip id, %d\n", ret);
		return ret;
	}

	ret = lxs_ts_read_value(ts, TC_VERSION, &version);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "failed to get version, %d\n", ret);
		return ret;
	}

	ret = lxs_ts_read_value(ts, TC_DEV_CODE, &dev_code);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "failed to get dev_code, %d\n", ret);
		return ret;
	}

	ret = lxs_ts_read_value(ts, INFO_CHIP_VERSION, &revision);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "failed to get revision, %d\n", ret);
		return ret;
	}

	ret = lxs_ts_reg_read(ts, TC_PRODUCT_ID1, product, 8);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "failed to get product id, %d\n", ret);
		return ret;
	}

	fw->chip_id_raw = chip_id;
	memset(fw->chip_id, 0, sizeof(fw->chip_id));
	fw->chip_id[0] = (chip_id>>24) & 0xFF;
	fw->chip_id[1] = (chip_id>>16) & 0xFF;
	fw->chip_id[2] = (chip_id>>8) & 0xFF;
	fw->chip_id[3] = chip_id & 0xFF;

	fw->dev_code = dev_code;

	fw->v.version_raw = version;
	vchip = fw->v.version.chip;
	vproto = fw->v.version.protocol;

	fw->revision = revision & 0xFF;

	memset(fw->product_id, 0, sizeof(fw->product_id));
	memcpy(fw->product_id, product, 8);

	ret = lxs_ts_chk_boot(ts);

	fw->ver_code_dev = ((dev_code>>8) & 0xFF)<<24;
	fw->ver_code_dev |= (dev_code & 0xFF)<<16;
	fw->ver_code_dev |= fw->v.version.module<<8;
	fw->ver_code_dev |= fw->v.version.ver;

	input_info(true, &ts->client->dev,
		"[T] class %s, LX%08X (0x%08X, 0x%02X)\n",
		fw->chip_id, fw->ver_code_dev, version, fw->revision);

	input_info(true, &ts->client->dev,
		"[T] product id %s, flash boot %s\n",
		fw->product_id, (ret < 0) ? "unknown" : (ret) ? "ERROR" : "ok");

	if (ret > 0) {
		atomic_set(&ts->boot, TC_IC_BOOT_FAIL);
		input_err(true, &ts->client->dev, "Boot fail detected, 0x%X\n", ret);
		return -EFAULT;
	} else if (ret < 0) {
		return ret;
	}

//	ts->opt.t_i2c_opt = (fw->v.version.major > 10);

	if (strcmp(fw->chip_id, CHIP_ID)) {
		input_err(true, &ts->client->dev, "Invalid chip id(%s), shall be %s\n",
			fw->chip_id, CHIP_ID);
		return -EINVAL;
	}

	if ((vchip != CHIP_VCHIP) || (vproto != CHIP_VPROTO)) {
		input_err(true, &ts->client->dev, "[T] IC info NG: %d, %d\n",
			vchip, vproto);
		return -EINVAL;
	}

	input_info(true, &ts->client->dev, "[T] IC info OK: %d, %d\n",
		vchip, vproto);

	return 0;
}

int lxs_ts_ic_init(struct lxs_ts_data *ts)
{
	int mode = TS_MODE_U3;
	int ret = 0;

	ret = lxs_ts_ic_test(ts);
	if (ret < 0)
		goto out;

	ret = lxs_ts_reload_fw(ts);
	if (ret < 0)
		goto out;

	input_info(true, &ts->client->dev, "%s init start\n", CHIP_DEVICE_NAME);

	atomic_set(&ts->boot, TC_IC_BOOT_DONE);

	if (is_ts_power_state_lpm(ts))
		mode = TS_MODE_U0;

	lxs_ts_init_ctrl(ts);

	ret = lxs_ts_ic_info(ts);
	if (ret < 0)
		goto out;

	ret = lxs_ts_init_reg_set(ts);
	if (ret < 0)
		goto out;

	switch (ts->op_state) {
	case OP_STATE_PROX:
		input_info(true, &ts->client->dev, "%s prox. state\n", CHIP_DEVICE_NAME);
		/* TBD */
		break;
	default:
		if (is_ts_power_state_lpm(ts)) {
			lxs_ts_knock(ts);
#if defined(USE_PROXIMITY)
			lxs_ts_set_ear_detect_enable(ts);
#endif
		}
		ret = lxs_ts_driving(ts, mode);
		if (ret < 0)
			goto out;
		break;
	}

	lxs_ts_get_mode_sig(ts);

	lxs_ts_mode_restore(ts);

#if defined(USE_INIT_STS_CHECK)
	lxs_ts_read_value(ts, IC_STATUS, &mode);
	input_info(true, &ts->client->dev, "%s: IC_STATUS 0x%08X\n", __func__, mode);
	lxs_ts_read_value(ts, TC_STATUS, &mode);
	input_info(true, &ts->client->dev, "%s: TC_STATUS 0x%08X\n", __func__, mode);
#endif

out:
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s init failed, %d\n", CHIP_DEVICE_NAME, ret);
	} else {
		atomic_set(&ts->init, TC_IC_INIT_DONE);
		input_info(true, &ts->client->dev, "%s init done\n", CHIP_DEVICE_NAME);
	}
	return ret;
}

static void lxs_ts_rinfo_channel(struct lxs_ts_data *ts)
{
	u32 addr = CHANNEL_INFO;
	u32 channel;
	u32 tx, rx;
	int ret = 0;

	if (lxs_addr_is_invalid(addr))
		return;

	ret = lxs_ts_read_value(ts, addr, &channel);
	if (ret < 0)
		return;

	tx = channel & 0xFFFF;
	rx = channel >> 16;

	ts->tx_count = tx;
	ts->rx_count = rx;

	if (!tx || !rx) {
		input_err(true, &ts->client->dev,
			"%s: info: invalud tx/rx: TX %d, RX %d\n",
			__func__, tx, rx);
		return;
	}

	input_info(true, &ts->client->dev,
		"%s: info: TX %d, RX %d\n",
		__func__, tx, rx);
}

static void lxs_ts_rinfo_resolution(struct lxs_ts_data *ts)
{
	u32 addr = RES_INFO;
	u32 resolution;
	u32 max_x, max_y;
	int ret = 0;

	if (lxs_addr_is_invalid(addr))
		return;

	ret = lxs_ts_read_value(ts, addr, &resolution);
	if (ret < 0)
		return;

	max_x = resolution & 0xFFFF;
	max_y = resolution >> 16;

	ts->res_x = max_x;
	ts->res_y = max_y;

	if (!max_x || !max_y) {
		input_err(true, &ts->client->dev,
			"%s: info: invalid resolution: RES_X %d, RES_Y %d\n",
			__func__, max_x, max_y);
		return;
	}

	ts->max_x = max_x;
	ts->max_y = max_y;

	input_info(true, &ts->client->dev,
		"%s: info: RES_X %d, RES_Y %d\n",
		__func__, max_x, max_y);
}

#if defined(USE_RINFO)
static void lxs_ts_rinfo_base(struct lxs_ts_data *ts)
{
	u32 addr = INFO_PTR;
	u32 info_data;
	u32 temp, tmsb, tlsb;

	if (lxs_addr_is_invalid(addr))
		return;

	lxs_ts_read_value(ts, addr, &info_data);
	ts->iinfo_addr = info_data & 0xFFF;	//0x0642
	ts->rinfo_addr = (info_data >> 12) & 0xFFF; //0x0642

	input_info(true, &ts->client->dev, "reg_info_addr: 0x%X\n", ts->rinfo_addr);

	lxs_ts_reg_read(ts, ts->rinfo_addr, ts->rinfo_data, sizeof(ts->rinfo_data));

	temp = ts->rinfo_data[7];
	tlsb = temp & 0xFFFF;
	tmsb = temp >> 16;

#if 0
	for (temp = 0 ; temp < ARRAY_SIZE(ts->rinf_data); temp++) {
		input_info(true, &ts->client->dev, "rinf_data[%d] = 0x%08X\n", temp, ts->rinfo_data[temp]);
	}
#endif

	if ((tlsb != ts->iinfo_addr) || (tmsb != ts->rinfo_addr)) {
		input_err(true, &ts->client->dev, "info invalid: 0x%08X vs. 0x%08X\n", info_data, temp);
		return;
	}

	input_info(true, &ts->client->dev, "info valid: 0x%08X\n", info_data);

	ts->rinfo_ok = 1;
}
#endif

int lxs_ts_hw_init(struct lxs_ts_data *ts)
{
	int ret = 0;

	ts_set_power_state_on(ts);
	lxs_ts_gpio_reset(ts, true);

	lxs_ts_delay(TOUCH_POWER_ON_DWORK_TIME);

#if 0
	input_info(true, &ts->client->dev, "%s: power_state %d, irq_enabled %d\n",
		__func__, ts_get_power_state(ts), ts->irq_enabled);
#endif

	ret = lxs_ts_ic_test(ts);
	if (ret < 0)
		return ret;

	ts->fw_data = devm_kzalloc(&ts->client->dev, SIZEOF_FW, GFP_KERNEL);
	if (!ts->fw_data) {
		ret = -ENOMEM;
		return ret;
	}

	ret = lxs_ts_request_fw(ts, ts->plat_data->firmware_name_ts);
	if (ret < 0)
		return ret;

	ret = lxs_ts_reload_fw(ts);
	if (ret < 0)
		return ret;

	ret = lxs_ts_chk_boot(ts);
	if (ret < 0)
		return ret;

#if defined(USE_RINFO)
	lxs_ts_rinfo_base(ts);
#endif

	lxs_ts_rinfo_resolution(ts);

	lxs_ts_rinfo_channel(ts);

	ts->touch_count = 0;

	ts->op_state = OP_STATE_STOP;
	ts->driving_mode = TS_MODE_STOP;

	input_info(true, &ts->client->dev, "%s: [status mask]\n", __func__);
	input_info(true, &ts->client->dev, "%s: normal  : 0x%08X\n", __func__, STATUS_MASK_NORMAL);
	input_info(true, &ts->client->dev, "%s: logging : 0x%08X\n", __func__, STATUS_MASK_LOGGING);
	input_info(true, &ts->client->dev, "%s: reset   : 0x%08X\n", __func__, STATUS_MASK_RESET);

	return 0;
}

void lxs_ts_touch_off(struct lxs_ts_data *ts)
{
	if (!atomic_read(&ts->fwsts))
		return;

	lxs_ts_driving(ts, TS_MODE_STOP);
	lxs_ts_clock_con(ts, 0);
}

int lxs_ts_set_charger(struct lxs_ts_data *ts)
{
	u32 charger_mode = ts->charger_mode;
	int ret;

	ret = lxs_ts_write_value(ts, CHARGER_INFO, charger_mode);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
			__func__, CHARGER_INFO);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: 0x%X\n", __func__, charger_mode);
	return 0;
}

static int lxs_ts_set_sip_mode(struct lxs_ts_data *ts)
{
	u32 sip_mode = ts->sip_mode;
	int ret;

	ret = lxs_ts_write_value(ts, SIP_MODE, sip_mode);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
			__func__, SIP_MODE);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, sip_mode);
	return 0;
}

static int lxs_ts_set_game_mode(struct lxs_ts_data *ts)
{
	u32 game_mode = ts->game_mode;
	int ret;

	ret = lxs_ts_write_value(ts, GAME_MODE, game_mode);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
			__func__, GAME_MODE);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, game_mode);
	return 0;
}

static int lxs_ts_set_glove_mode(struct lxs_ts_data *ts)
{
	u32 glove_mode = ts->glove_mode;
	int ret;

	ret = lxs_ts_write_value(ts, GLOVE_MODE, glove_mode);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
			__func__, GLOVE_MODE);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, glove_mode);
	return 0;
}

static int lxs_ts_set_cover_mode(struct lxs_ts_data *ts)
{
	u32 cover_mode = ts->cover_mode;
	int ret;

	ret = lxs_ts_write_value(ts, COVER_MODE, cover_mode);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
			__func__, COVER_MODE);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, cover_mode);
	return 0;
}

static int lxs_ts_set_dead_zone_enable(struct lxs_ts_data *ts)
{
	u32 dead_zone_enable = ts->dead_zone_enable;
	int ret;

	ret = lxs_ts_write_value(ts, DEAD_ZONE_MODE, dead_zone_enable);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
			__func__, DEAD_ZONE_MODE);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, dead_zone_enable);
	return 0;
}

#if defined(USE_GRIP_DATA)
static int lxs_ts_set_touchable_area(struct lxs_ts_data *ts)
{
	u32 touchable_area = ts->touchable_area;

	/* TBD */

	input_info(true, &ts->client->dev, "%s: set 16:9 mode %s\n", __func__,
		touchable_area ? "enable" : "disabled");

	return 0;
}

static int lxs_ts_set_grip_edge_handler(struct lxs_ts_data *ts)
{
	struct lxs_ts_grip_data *grip_data = &ts->grip_data;
	int direction = grip_data->exception_direction;

	switch (direction) {
	case 0:	/* disable */
		input_info(true, &ts->client->dev, "%s: disable\n", __func__);
		break;
	case 1:	/* enable left */
		input_info(true, &ts->client->dev, "%s: enable left side\n", __func__);
		break;
	case 2: /* enable right */
		input_info(true, &ts->client->dev, "%s: enable right side\n", __func__);
		break;
	default:
		input_err(true, &ts->client->dev, "%s: invalid direction, 0x%02X\n", __func__, direction);
		return -EINVAL;
	}

	if (direction) {
		/*
		grip_data->exception_upper_y;
		grip_data->exception_lower_y;
		*/
	} else {

	}

	lxs_ts_delay(10);

	input_info(true, &ts->client->dev, "%s (noop)\n", __func__);

	return 0;
}

static int lxs_ts_set_grip_portrait_mode(struct lxs_ts_data *ts)
{
	/*
	struct lxs_ts_grip_data *grip_data = &ts->plat_data->grip_data;

	grip_data->portrait_zone;
	grip_data->portrait_upper;
	grip_data->portrait_lower;
	grip_data->portrait_boundary_y;
	*/

	input_info(true, &ts->client->dev, "%s (noop)\n", __func__);

	return 0;
}

static int lxs_ts_set_grip_landscape_mode(struct lxs_ts_data *ts)
{
	struct lxs_ts_grip_data *grip_data = &ts->grip_data;
	int mode = grip_data->landscape_mode;

	switch (mode) {
	case 0:	/* use previous portrait setting */
		input_info(true, &ts->client->dev, "%s: use previous portrait settings\n", __func__);
		break;
	case 1:	/* enable landscape mode */
		input_info(true, &ts->client->dev, "%s: set landscape mode parameters\n", __func__);
		/*
		grip_data->landscape_reject_bl;
		grip_data->landscape_grip_bl;
		grip_data->landscape_reject_ts;
		grip_data->landscape_reject_bs;
		grip_data->landscape_grip_ts;
		grip_data->landscape_grip_bs;
		*/
		break;
	default:
		input_err(true, &ts->client->dev, "%s: invalid mode, 0x%02X\n", __func__, mode);
		return -EINVAL;
	}

	return 0;
}
#endif	/* USE_GRIP_DATA */

#if defined(USE_PROXIMITY)
static int lxs_ts_set_prox_lp_scan_mode(struct lxs_ts_data *ts)
{
	u32 prox_lp_scan_mode = ts->prox_lp_scan_mode;
	int ret;

	if (!ts->plat_data->prox_lp_scan_enabled)
		return 0;

	if (!is_ts_power_state_lpm(ts))
		return 0;

	if (ts->op_state != OP_STATE_PROX)
		return 0;

	ret = lxs_ts_write_value(ts, PROX_LP_SCAN, prox_lp_scan_mode);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
			__func__, PROX_LP_SCAN);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, prox_lp_scan_mode);

	return 0;
}

static int __lxs_ts_set_ed_mode(struct lxs_ts_data *ts, u32 mode)
{
	int ret;

	ret = lxs_ts_write_value(ts, PROX_MODE, mode);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
			__func__, PROX_MODE);
		return ret;
	}

	return 0;
}

static int lxs_ts_set_ear_detect_enable(struct lxs_ts_data *ts)
{
	u32 ear_detect_mode = ts->ear_detect_mode;
	int ret;

	if (!ts->plat_data->support_ear_detect) {
		ts->ed_reset_flag = false;
		return 0;
	}

	if (!ear_detect_mode && ts->ed_reset_flag) {
		input_info(true, &ts->client->dev, "%s : set ed on & off\n", __func__);
		__lxs_ts_set_ed_mode(ts, 1);
		__lxs_ts_set_ed_mode(ts, 0);
	}
	ts->ed_reset_flag = false;

	ret = __lxs_ts_set_ed_mode(ts, ear_detect_mode);
	if (ret < 0)
		return ret;

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, ear_detect_mode);

	return 0;
}
#endif

static int lxs_ts_set_note_mode(struct lxs_ts_data *ts)
{
	u32 note_mode = ts->note_mode;
	int ret;

	ret = lxs_ts_write_value(ts, NOTE_MODE, note_mode);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command(0x%x)",
			__func__, NOTE_MODE);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: %d\n", __func__, note_mode);
	return 0;
}

int lxs_ts_mode_switch(struct lxs_ts_data *ts, int mode, int value)
{
	int ret = 0;

	switch (mode) {
	case MODE_SWITCH_SIP_MODE:
		if (value < 0 || value > 1) {
			input_err(true, &ts->client->dev,
				"%s: [%d] invalid param %d\n", __func__, mode, value);
			return -EINVAL;
		}

		ts->sip_mode = value;
		ret = lxs_ts_set_sip_mode(ts);
		break;
	case MODE_SWITCH_GAME_MODE:
		if (value < 0 || value > 1) {
			input_err(true, &ts->client->dev,
				"%s: [%d] invalid param %d\n", __func__, mode, value);
			return -EINVAL;
		}

		ts->game_mode = value;
		ret = lxs_ts_set_game_mode(ts);
		break;
	case MODE_SWITCH_GLOVE_MODE:
		if (value < 0 || value > 1) {
			input_err(true, &ts->client->dev,
				"%s: [%d] invalid param %d\n", __func__, mode, value);
			return -EINVAL;
		}

		ts->glove_mode = value;
		ret = lxs_ts_set_glove_mode(ts);
		break;
	case MODE_SWITCH_COVER_MODE:
		ret = lxs_ts_set_cover_mode(ts);
		break;
	case MODE_SWITCH_DEAD_ZONE_ENABLE:
		if (value < 0 || value > 1) {
			input_err(true, &ts->client->dev,
				"%s: [%d] invalid param %d\n", __func__, mode, value);
			return -EINVAL;
		}

		ts->dead_zone_enable = value;
		ret = lxs_ts_set_dead_zone_enable(ts);
		break;

#if defined(USE_GRIP_DATA)
	case MODE_SWITCH_SET_TOUCHABLE_AREA:
		if (value < 0 || value > 1) {
			input_err(true, &ts->client->dev,
				"%s: [%d] invalid param %d\n", __func__, mode, value);
			return -EINVAL;
		}

		ts->touchable_area = value;
		ret = lxs_ts_set_touchable_area(ts);
		break;
	case MODE_SWITCH_GRIP_EDGE_HANDLER:
		ret = lxs_ts_set_grip_edge_handler(ts);
		break;
	case MODE_SWITCH_GRIP_PORTRAIT_MODE:
		ret = lxs_ts_set_grip_portrait_mode(ts);
		break;
	case MODE_SWITCH_GRIP_LANDSCAPE_MODE:
		ret = lxs_ts_set_grip_landscape_mode(ts);
		break;
#endif	/* USE_GRIP_DATA */

	case MODE_SWITCH_WIRELESS_CHARGER:
		if (value < 0 || value > 3) {
			input_err(true, &ts->client->dev,
				"%s: [%d] invalid param %d\n", __func__, mode, value);
			return -EINVAL;
		}

		ts->wirelesscharger_mode = value;
		if (value) {
			ts->charger_mode |= LXS_TS_BIT_CHARGER_MODE_WIRELESS_CHARGER;
		} else {
			ts->charger_mode &= ~LXS_TS_BIT_CHARGER_MODE_WIRELESS_CHARGER;
		}
		ret = lxs_ts_set_charger(ts);
		break;

#if defined(USE_PROXIMITY)
	case MODE_SWITCH_PROX_LP_SCAN_MODE:
		if (!ts->plat_data->prox_lp_scan_enabled) {
			input_err(true, &ts->client->dev, "%s: [%d] not support prox lp scan\n", __func__, mode);
			return -EPERM;
		}

		if (!is_ts_power_state_lpm(ts)) {
			input_err(true, &ts->client->dev, "%s: power is is not lpm\n", __func__);
			return -EINVAL;
		}

		if (ts->op_state != OP_STATE_PROX) {
			input_err(true, &ts->client->dev, "%s: not OP_STATE_PROX\n", __func__);
			return -EINVAL;
		}

		if (value < 0 || value > 1) {
			input_err(true, &ts->client->dev,
				"%s: [%d] invalid param %d\n", __func__, mode, value);
			return -EINVAL;
		}

		ts->prox_lp_scan_mode = value;
		ret = lxs_ts_set_prox_lp_scan_mode(ts);
		break;
	case MODE_SWITCH_EAR_DETECT_ENABLE:
		ret = lxs_ts_set_ear_detect_enable(ts);
		break;
#endif	/* USE_PROXIMITY */

	case MODE_SWITCH_SET_NOTE_MODE:
		if (value < 0 || value > 1) {
			input_err(true, &ts->client->dev,
				"%s: [%d] invalid param %d\n", __func__, mode, value);
			return -EINVAL;
		}

		ts->note_mode = value;
		ret = lxs_ts_set_note_mode(ts);
		break;
	default:
		input_err(true, &ts->client->dev, "%s: unknown mode %d\n", __func__, mode);
		return -EFAULT;
	}

	return ret;
}

void lxs_ts_mode_restore(struct lxs_ts_data *ts)
{
#if defined(USE_PROXIMITY)
	if (!is_ts_power_state_lpm(ts))
		lxs_ts_set_ear_detect_enable(ts);

	switch (ts->op_state) {
	case OP_STATE_PROX:
		lxs_ts_set_prox_lp_scan_mode(ts);
		break;
	default:
		ts->prox_lp_scan_mode = 0;
		break;
	}
#endif

	lxs_ts_set_charger(ts);
	lxs_ts_set_sip_mode(ts);
	lxs_ts_set_game_mode(ts);
	lxs_ts_set_glove_mode(ts);
	lxs_ts_set_cover_mode(ts);
	lxs_ts_set_dead_zone_enable(ts);

#if 0	//defined(USE_GPRIO_DATA)
	lxs_ts_set_touchable_area(ts);
	lxs_ts_set_grip_exception_zone(ts);
	lxs_ts_set_grip_portrait_mode(ts);
	lxs_ts_set_grip_landscape_mode(ts);
#endif

	lxs_ts_set_note_mode(ts);
}

void lxs_ts_change_scan_rate(struct lxs_ts_data *ts, u8 rate)
{
	/* TBD */

	input_info(true, &ts->client->dev, "%s: scan rate (%d Hz)\n", __func__, rate);
}

int lxs_ts_set_external_noise_mode(struct lxs_ts_data *ts, u8 mode)
{
	/* TBD */

	input_info(true, &ts->client->dev, "%s: mode %d\n", __func__, mode);

	return 0;
}

#define LPWG_DUMP_PACKET_SIZE	5		/* 5 byte */
#define LPWG_DUMP_TOTAL_SIZE	500		/* 5 byte * 100 */
#define LPWG_DUMP_LXS_SIZE	200
void lxs_ts_lpwg_dump_buf_init(struct lxs_ts_data *ts)
{
	ts->lpwg_dump_buf = devm_kzalloc(&ts->client->dev, LPWG_DUMP_TOTAL_SIZE, GFP_KERNEL);
	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "kzalloc for lpwg_dump_buf failed!\n");
		return;
	}
	ts->lpwg_dump_buf_idx = 0;
	input_info(true, &ts->client->dev, "%s : alloc done\n", __func__);
}

int lxs_ts_lpwg_dump_buf_write(struct lxs_ts_data *ts, u8 *buf)
{
	int i = 0;

	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "%s : kzalloc for lpwg_dump_buf failed!\n", __func__);
		return -1;
	}

	for (i = 0 ; i < LPWG_DUMP_PACKET_SIZE ; i++) {
		ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx++] = buf[i];
	}
	if (ts->lpwg_dump_buf_idx >= LPWG_DUMP_TOTAL_SIZE) {
		input_info(true, &ts->client->dev, "%s : write end of data buf(%d)!\n",
					__func__, ts->lpwg_dump_buf_idx);
		ts->lpwg_dump_buf_idx = 0;
	}
	return 0;
}

int lxs_ts_lpwg_dump_buf_read(struct lxs_ts_data *ts, u8 *buf)
{

	u8 read_buf[30] = { 0 };
	int read_packet_cnt;
	int start_idx;
	int i;

	if (ts->lpwg_dump_buf == NULL) {
		input_err(true, &ts->client->dev, "%s : kzalloc for lpwg_dump_buf failed!\n", __func__);
		return 0;
	}

	if (ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx] == 0
		&& ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx + 1] == 0
		&& ts->lpwg_dump_buf[ts->lpwg_dump_buf_idx + 2] == 0) {
		start_idx = 0;
		read_packet_cnt = ts->lpwg_dump_buf_idx / LPWG_DUMP_PACKET_SIZE;
	} else {
		start_idx = ts->lpwg_dump_buf_idx;
		read_packet_cnt = LPWG_DUMP_TOTAL_SIZE / LPWG_DUMP_PACKET_SIZE;
	}

	input_info(true, &ts->client->dev, "%s : lpwg_dump_buf_idx(%d), start_idx (%d), read_packet_cnt(%d)\n",
				__func__, ts->lpwg_dump_buf_idx, start_idx, read_packet_cnt);

	for (i = 0 ; i < read_packet_cnt ; i++) {
		memset(read_buf, 0x00, 30);
		snprintf(read_buf, 30, "%03d : %02X%02X%02X%02X%02X\n",
					i, ts->lpwg_dump_buf[start_idx + 0], ts->lpwg_dump_buf[start_idx + 1],
					ts->lpwg_dump_buf[start_idx + 2], ts->lpwg_dump_buf[start_idx + 3],
					ts->lpwg_dump_buf[start_idx + 4]);

		strlcat(buf, read_buf, PAGE_SIZE);

		if (start_idx + LPWG_DUMP_PACKET_SIZE >= LPWG_DUMP_TOTAL_SIZE) {
			start_idx = 0;
		} else {
			start_idx += 5;
		}
	}

	return 0;
}

int lxs_ts_get_lp_dump_data(struct lxs_ts_data *ts)
{
	int ret;
	u32 data = 0;
	u8 buf[LPWG_DUMP_LXS_SIZE + 1];
	u8 read_lpdump_size;
	unsigned int i;
	
	ret = lxs_ts_read_value(ts, LP_DUMP_CHK, &data);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to check lp_dump, %d\n", __func__, ret);
		return ret;
	}

	if (!data) {
		input_err(true, &ts->client->dev, "%s: lp_dump not support\n", __func__);
		return 0;
	}
	//offset write
	ret = lxs_ts_write_value(ts, SERIAL_DATA_OFFSET, LP_DUMP_BUF);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to get lp_dump data, %d\n", __func__, ret);
		return ret;
	}

	//read raw data
	ret = lxs_ts_reg_read(ts, DATA_BASE_ADDR, buf, LPWG_DUMP_LXS_SIZE + 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to get lp_dump data, %d\n", __func__, ret);
		return ret;
	}

	read_lpdump_size = buf[0];
	if (!read_lpdump_size || (read_lpdump_size * LPWG_DUMP_PACKET_SIZE > LPWG_DUMP_LXS_SIZE)) {
		input_err(true, &ts->client->dev, "%s : abnormal lp dump data size(%d)\n", __func__, read_lpdump_size);
		return -EFAULT;
	}
	
	for (i = 0; i < read_lpdump_size; i++) {

		lxs_ts_lpwg_dump_buf_write(ts, &buf[i * LPWG_DUMP_PACKET_SIZE + 1]);
	}

	input_info(true, &ts->client->dev, "%s: get lp_dump data\n", __func__);

	return 0;
}

MODULE_LICENSE("GPL");

