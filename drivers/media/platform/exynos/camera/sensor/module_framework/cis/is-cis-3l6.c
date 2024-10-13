/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#include <exynos-is-sensor.h>
#include "is-hw.h"
#include "is-core.h"
#include "is-param.h"
#include "is-device-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-resourcemgr.h"
#include "is-dt.h"
#include "is-cis-3l6.h"
#ifdef USE_S5K3L6_13MP_FULL_SIZE
#include "is-cis-3l6-setA-13mp.h"
#else
#include "is-cis-3l6-setA.h"
#endif

#include "is-helper-ixc.h"

#define SENSOR_NAME "S5K3L6"
/* #define DEBUG_3L6_PLL */

/* Value at address 0x344 in set file*/
#define CURR_X_INDEX_3L6 ((12 * 3) + 1)
/* Value at address 0x346 in set file*/
#define CURR_Y_INDEX_3L6 ((14 * 3) + 1)

#define CALIBRATE_CUR_X_3L6 48
#define CALIBRATE_CUR_Y_3L6 20

static const struct v4l2_subdev_ops subdev_ops;

/* For checking frame count */
static u32 sensor_3l6_fcount;

int sensor_3l6_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	ktime_t st = ktime_get();
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->stream_on = false;
	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->sens_config_index_pre = SENSOR_3L6_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;

	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	is_vendor_set_mipi_mode(cis);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

static const struct is_cis_log log_3l6[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0002, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 8, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0202, 0, "cit"},
};

int sensor_3l6_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_3l6, ARRAY_SIZE(log_3l6), (char *)__func__);

	return ret;
}

int sensor_3l6_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_3l6_private_data *priv = (struct sensor_3l6_private_data *)cis->sensor_info->priv;

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	dbg_sensor(1, "[%s] global setting done\n", __func__);

	return ret;
}

#if defined (USE_MS_PDAF)
static bool sensor_3l6_is_padf_enable(u32 mode)
{
	switch(mode)
	{
		case SENSOR_3L6_MODE_992x744_120FPS:
			return false;
		default:
			return true;
	}
}
#endif

int sensor_3l6_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	const struct sensor_cis_mode_info *mode_info;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

#if defined(USE_MS_PDAF)
	/*PDAF and binning are connected.
	if there is binning then padf is not applied.
	check the setfile xls for binning information.
	 */
	dbg_sensor(1, "USE_MS_PDAF: [%s] mode [%d]\n", __func__, mode);
	cis->use_pdaf = sensor_3l6_is_padf_enable(mode);
	if (cis->use_pdaf) {
		dbg_sensor(1, "USE_MS_PDAF: [%s] using pdaf\n", __func__);
		cis->cis_data->cur_pos_x = sensor_3l6_setfiles[mode][CURR_X_INDEX_3L6] - CALIBRATE_CUR_X_3L6;
		cis->cis_data->cur_pos_y = sensor_3l6_setfiles[mode][CURR_Y_INDEX_3L6] - CALIBRATE_CUR_Y_3L6;
	}
	else {
		dbg_sensor(1, "USE_MS_PDAF: [%s] NOT using pdaf\n", __func__);
		cis->cis_data->cur_pos_x = 0;
		cis->cis_data->cur_pos_y = 0;
	}
	dbg_sensor(1, "USE_MS_PDAF: [%s] cur_pos_x [%d] cur_pos_y [%d] \n", __func__, cis->cis_data->cur_pos_x, cis->cis_data->cur_pos_y);
#endif /* defined (USE_MS_PDAF) */

	info("[%s] sensor mode(%d)\n", __func__, mode);

	mode_info = cis->sensor_info->mode_infos[mode];

	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0)
		err("sensor_setfiles fail!!");

	cis->cis_data->sens_config_index_pre = mode;

	dbg_sensor(1, "[%s] mode changed(%d)\n", __func__, mode);

p_err:
	return ret;
}

int sensor_3l6_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	/* Sensor stream on */
	ret = cis->ixc_ops->write16(cis->client, 0x3892, 0x3600);
	ret |= cis->ixc_ops->write16(cis->client, SENSOR_3L6_PLL_POWER_CONTROL_ADDR, 0x0100);
	usleep_range(3000, 3000);

	ret |= cis->ixc_ops->write16(cis->client, SENSOR_3L6_MODE_SELECT_ADDR, 0x0100);
	usleep_range(3000, 3000);

	ret |= cis->ixc_ops->write16(cis->client, SENSOR_3L6_PLL_POWER_CONTROL_ADDR, 0x0000);
	usleep_range(3000, 3000);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	cis_data->stream_on = true;
	info("%s done\n", __func__);
	sensor_3l6_fcount = 0;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 cur_frame_count = 0;
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	ret = cis->ixc_ops->read8(cis->client, SENSOR_3L6_FRAME_COUNT_ADDR, &cur_frame_count);
	sensor_3l6_fcount = cur_frame_count;

	/* Sensor stream off */
	ret = cis->ixc_ops->write16(cis->client, SENSOR_3L6_MODE_SELECT_ADDR, 0x0000);
	info("%s done, frame_count(%d)\n", __func__, cur_frame_count);

	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_3l6_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	u16 long_gain = 0;
	u16 short_gain = 0;
	u16 dgains[4] = {0};

	WARN_ON(!dgain);

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (long_gain < cis->cis_data->min_digital_gain[0])
		long_gain = cis->cis_data->min_digital_gain[0];

	if (long_gain > cis->cis_data->max_digital_gain[0])
		long_gain = cis->cis_data->max_digital_gain[0];

	if (short_gain < cis->cis_data->min_digital_gain[0])
		short_gain = cis->cis_data->min_digital_gain[0];

	if (short_gain > cis->cis_data->max_digital_gain[0])
		short_gain = cis->cis_data->max_digital_gain[0];

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d, long_gain(%#x), short_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->long_val, dgain->short_val,
			long_gain, short_gain);

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = short_gain;
	/* Short digital gain */
	ret = cis->ixc_ops->write16_array(cis->client, cis->reg_addr->dgain, dgains, 4);
	if (ret < 0)
		goto p_err;

	/* Long digital gain */
	if (is_vender_wdr_mode_on(cis_data)) {
		dgains[0] = dgains[1] = dgains[2] = dgains[3] = long_gain;
		ret = cis->ixc_ops->write16_array(cis->client, 0x3062, dgains, 4);
		if (ret < 0)
			goto p_err;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_3l6_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	u32 wait_cnt = 0, time_out_cnt = 250;
	u8 sensor_fcount = 0;
	u32 i2c_fail_cnt = 0, i2c_fail_max_cnt = 5;

	/*
	 * Read sensor frame counter (sensor_fcount address = 0x0005)
	 * stream on (0x00 ~ 0xFF), stream off (0xFF)
	 */
	do {
		IXC_MUTEX_LOCK(cis->ixc_lock);
		ret = cis->ixc_ops->read8(cis->client, SENSOR_3L6_FRAME_COUNT_ADDR, &sensor_fcount);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0) {
			i2c_fail_cnt++;
			err("i2c transfer fail addr(%x), val(%x), try(%d), ret = %d\n",
				SENSOR_3L6_FRAME_COUNT_ADDR, sensor_fcount, i2c_fail_cnt, ret);

			if (i2c_fail_cnt >= i2c_fail_max_cnt) {
				err("[MOD:D:%d] %s, i2c fail, i2c_fail_cnt(%d) >= i2c_fail_max_cnt(%d), sensor_fcount(%d)",
						cis->id, __func__, i2c_fail_cnt, i2c_fail_max_cnt, sensor_fcount);
				ret = -EINVAL;
				goto p_err;
			}
		}
		
		/* If fcount is '0xfe' or '0xff' in streamoff, delay by 33 ms. */
		if (sensor_3l6_fcount >= 0xFE && sensor_fcount == 0xFF) {
			usleep_range(33000, 33000);
			info("[%s] delay by 33 ms (stream_off fcount : %d, wait_stream_off fcount : %d",
				__func__, sensor_3l6_fcount, sensor_fcount);
		}

		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME);
		wait_cnt++;

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, time out, wait_limit(%d) > time_out(%d), sensor_fcount(%d)",
					cis->id, __func__, wait_cnt, time_out_cnt, sensor_fcount);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	}while (sensor_fcount != 0xFF);

	info("[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
			cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
p_err:
	return ret;
}

int sensor_3l6_cis_get_otprom_data(struct v4l2_subdev *subdev, char *buf, bool camera_running, int rom_id)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u16 read_addr;
	u8 otp_bank;
	u16 start_addr = 0;
	int page_byte_index = 0;
	int num_otp_byte_read = 0;
	int page_number = 0;
	struct is_device_sensor_peri *sensor_peri = NULL;
	int rom_type;

	sensor_peri = container_of(cis, struct is_device_sensor_peri, cis);
	rom_type = sensor_peri->module->pdata->rom_type;

	info("[%s] camera_running(%d) rom_type(%d)\n", __func__, camera_running, rom_type);

	/* 1. prepare to otp read : sensor initial settings */
	if (camera_running == false)
		ret = sensor_3l6_cis_set_global_setting(subdev);

	ret |= sensor_3l6_cis_mode_change(subdev, 1);
	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret |= cis->ixc_ops->write8(cis->client, 0x0100, 0x00); /* stream on */
	ret |= cis->ixc_ops->write8(cis->client, 0x0101, 0x01); /* stream on */
	if (ret < 0) {
		err("failed to init_setting");
		goto exit;
	}

	/* 2. select otp bank */
	ret = cis->ixc_ops->write8(cis->client, 0x0A00, 0x04); /* make initial state */
	ret |= cis->ixc_ops->write8(cis->client, 0x0A02, S5K3L6_OTP_BANK_READ_PAGE); /* select page */
	ret |= cis->ixc_ops->write8(cis->client, 0x0A00, 0x01); /* read mode */
	usleep_range(47, 48); /* sleep 47usec */
	ret |= cis->ixc_ops->read8(cis->client, S5K3L6_OTP_PAGE_START_ADDR, &otp_bank);
	if (ret < 0) {
		err("failed to select_opt_bank");
		goto exit;
	}

	start_addr = S5K3L6_OTP_READ_START_ADDR;
	/* select start address */
	switch (otp_bank) {
	case 0x01:
		page_number = S5K3L6_OTP_START_PAGE_BANK1;
		break;
	case 0x03:
		page_number = S5K3L6_OTP_START_PAGE_BANK2;
		break;
	default:
		page_number = S5K3L6_OTP_START_PAGE_BANK1;
		break;
	}
	info("%s: otp_bank = %d start_addr = %x page_number = %x\n", __func__, otp_bank, start_addr, page_number);

	/* Disable read mode */
	cis->ixc_ops->write8(cis->client, 0x0A00, 0x04); /* make initial state */
	cis->ixc_ops->write8(cis->client, 0x0A00, 0x00); /* disable NVM controller */

	/* 3. Read OTP Cal Data */
	num_otp_byte_read = 0;
	page_byte_index = 5;
	read_addr = start_addr; /* bank read address */

	while (num_otp_byte_read < IS_READ_MAX_S5K3L6_OTP_CAL_SIZE) {
		/* set page for cal read */
		ret = cis->ixc_ops->write8(cis->client, 0x0A00, 0x04); /* make initial state */
		ret |= cis->ixc_ops->write8(cis->client, 0x0A02, page_number); /* select page */
		ret |= cis->ixc_ops->write8(cis->client, 0x0A00, 0x01); /* read mode */
		if (ret < 0) {
			err("failed to read_mode, num_otp_byte_read(%d)", num_otp_byte_read);
			goto exit;
		}
		usleep_range(47, 48); /* sleep 47usec */

		while (page_byte_index <= S5K3L6_OTP_PAGE_SIZE) {
			ret = cis->ixc_ops->read8(cis->client, read_addr, &buf[num_otp_byte_read]);
			if (ret < 0) {
				err("failed to read OTP bank address(%d) ret(%d)", read_addr, ret);
				goto exit;
			}
			read_addr++;
			page_byte_index++;
			num_otp_byte_read++;
			if (num_otp_byte_read == IS_READ_MAX_S5K3L6_OTP_CAL_SIZE)
				break;
		}
		page_number++;
		read_addr = S5K3L6_OTP_PAGE_START_ADDR;
		page_byte_index = 1;

		/* Disable read mode */
		cis->ixc_ops->write8(cis->client, 0x0A00, 0x04); /* make initial state */
		cis->ixc_ops->write8(cis->client, 0x0A00, 0x00); /* disable NVM controller */
	}

	/* 4. Set to initial state and*/
exit:
	/* Disable read mode */
	cis->ixc_ops->write8(cis->client, 0x0A00, 0x04); /* make initial state */
	cis->ixc_ops->write8(cis->client, 0x0A00, 0x00); /* disable NVM controller */
	/* stream off */
	cis->ixc_ops->write16(cis->client, 0x0100, 0x0000); /* stream off */
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	usleep_range(1000, 1001); /* sleep 1msec */

	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_3l6_cis_init,
	.cis_log_status = sensor_3l6_cis_log_status,
	.cis_group_param_hold = NULL,
	.cis_set_global_setting = sensor_3l6_cis_set_global_setting,
	.cis_mode_change = sensor_3l6_cis_mode_change,
//	.cis_set_size = sensor_3l6_cis_set_size,
	.cis_stream_on = sensor_3l6_cis_stream_on,
	.cis_stream_off = sensor_3l6_cis_stream_off,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_wait_streamoff = sensor_3l6_cis_wait_streamoff,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = sensor_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_set_long_term_exposure = NULL,
	.cis_adjust_frame_duration = sensor_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = sensor_cis_set_analog_gain,
	.cis_get_analog_gain = sensor_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_calc_again_code = sensor_cis_calc_again_code,
	.cis_calc_again_permile = sensor_cis_calc_again_permile,
	.cis_set_digital_gain = sensor_3l6_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
#ifdef USE_CAMERA_RECOVER_3L6
	.cis_recover_stream_on = sensor_cis_recover_stream_on,
	.cis_recover_stream_off = sensor_cis_recover_stream_off,
#endif
	.cis_get_otprom_data = sensor_3l6_cis_get_otprom_data,
};

int cis_3l6_probe_i2c(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;

	probe_info("%s: sensor_cis_probe started\n", __func__);
	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;

	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->reg_addr = &sensor_3l6_reg_addr;
	cis->use_dgain = false;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("[%s] setfile_A mclk: 26Mhz \n", __func__);
	else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	cis->sensor_info = &sensor_3l6_info_A;

	probe_info("%s done\n", __func__);

	return ret;
}

static int cis_3l6_remove_i2c(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

static const struct of_device_id sensor_is_cis_3l6_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-3l6",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_is_cis_3l6_match);

static const struct i2c_device_id sensor_cis_3l6_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_3l6_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_is_cis_3l6_match,
	},
	.probe	= cis_3l6_probe_i2c,
	.remove	= cis_3l6_remove_i2c,
	.id_table = sensor_cis_3l6_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_3l6_driver);
#else
static int __init sensor_cis_3l6_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_3l6_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_3l6_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_3l6_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
