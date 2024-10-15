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
#include <videodev2_exynos_camera.h>
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
#include "is-cis-4ha.h"
#include "is-cis-4ha-setA.h"
#include "is-cis-4ha-setB.h"

#include "is-helper-ixc.h"

#define SENSOR_NAME "S5K4HA"

static u8 sensor_4ha_fcount;

static int sensor_4ha_wait_stream_off_status(cis_shared_data *cis_data)
{
	int ret = 0;
	u32 timeout = 0;

	BUG_ON(!cis_data);

#define STREAM_OFF_WAIT_TIME 250
	while (timeout < STREAM_OFF_WAIT_TIME) {
		if (cis_data->is_active_area == false &&
				cis_data->stream_on == false) {
			pr_debug("actual stream off\n");
			break;
		}
		timeout++;
	}

	if (timeout == STREAM_OFF_WAIT_TIME) {
		pr_err("actual stream off wait timeout\n");
		ret = -1;
	}

	return ret;
}

int sensor_4ha_cis_init(struct v4l2_subdev *subdev)
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
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	cis->cis_data->sens_config_index_pre = SENSOR_4HA_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));
	return ret;
}

static const struct is_cis_log log_4ha[] = {
	{I2C_READ, 16, 0x0000, 0, "model_id"},
	{I2C_READ, 8, 0x0002, 0, "rev_number"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0136, 0, "0x0136"},
	{I2C_READ, 16, 0x0202, 0, "0x0202"},
	{I2C_READ, 16, 0x3C02, 0, "0x3C02"},
	{I2C_READ, 16, 0x3500, 0, "0x3500"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
};

int sensor_4ha_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_4ha, ARRAY_SIZE(log_4ha), (char *)__func__);

	return ret;
}

int sensor_4ha_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_4ha_private_data *priv = (struct sensor_4ha_private_data *)cis->sensor_info->priv;

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	dbg_sensor(1, "[%s] global setting done\n", __func__);

	return ret;
}

int sensor_4ha_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	const struct sensor_cis_mode_info *mode_info;

	if (mode > cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	mode_info = cis->sensor_info->mode_infos[mode];
	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0)
		err("sensor_4ha_set_registers fail!!");

	cis->cis_data->sens_config_index_pre = mode;
	info("[%s] mode changed(%d)\n", __func__, mode);

p_err:
	return ret;
}

/* TODO: Sensor set size sequence(sensor done, sensor stop, 3AA done in FW case */
int sensor_4ha_cis_set_size(struct v4l2_subdev *subdev, cis_shared_data *cis_data)
{
	int ret = 0;
	bool binning = false;
	u32 ratio_w = 0, ratio_h = 0, start_x = 0, start_y = 0, end_x = 0, end_y = 0;
	u32 even_x= 0, odd_x = 0, even_y = 0, odd_y = 0;
	struct i2c_client *client = NULL;
	struct is_cis *cis = NULL;
	ktime_t st = ktime_get();

	BUG_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	BUG_ON(!cis);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	if (unlikely(!cis_data)) {
		err("cis data is NULL");
		if (unlikely(!cis->cis_data)) {
			ret = -EINVAL;
			goto p_err;
		} else {
			cis_data = cis->cis_data;
		}
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	/* Wait actual stream off */
	ret = sensor_4ha_wait_stream_off_status(cis_data);
	if (ret) {
		err("Must stream off\n");
		ret = -EINVAL;
		goto p_err;
	}

	binning = cis_data->binning;
	if (binning) {
		ratio_w = (cis->sensor_info->max_width / cis_data->cur_width);
		ratio_h = (cis->sensor_info->max_height / cis_data->cur_height);
	} else {
		ratio_w = 1;
		ratio_h = 1;
	}

	if (((cis_data->cur_width * ratio_w) > cis->sensor_info->max_width) ||
		((cis_data->cur_height * ratio_h) > cis->sensor_info->max_height)) {
		err("Config max sensor size over~!!\n");
		ret = -EINVAL;
		goto p_err;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);
	/* 1. page_select */
	ret = cis->ixc_ops->write16(client, 0x6028, 0x2000);
	if (ret < 0)
		 goto p_err;

	/* 2. pixel address region setting */
	start_x = ((cis->sensor_info->max_width - cis_data->cur_width * ratio_w) / 2) & (~0x1);
	start_y = ((cis->sensor_info->max_height - cis_data->cur_height * ratio_h) / 2) & (~0x1);
	end_x = start_x + (cis_data->cur_width * ratio_w - 1);
	end_y = start_y + (cis_data->cur_height * ratio_h - 1);

	if (!(end_x & (0x1)) || !(end_y & (0x1))) {
		err("Sensor pixel end address must odd\n");
		ret = -EINVAL;
		goto p_err;
	}

	ret = cis->ixc_ops->write16(client, 0x0344, start_x);
	if (ret < 0)
		 goto p_err;
	ret = cis->ixc_ops->write16(client, 0x0346, start_y);
	if (ret < 0)
		 goto p_err;
	ret = cis->ixc_ops->write16(client, 0x0348, end_x);
	if (ret < 0)
		 goto p_err;
	ret = cis->ixc_ops->write16(client, 0x034A, end_y);
	if (ret < 0)
		 goto p_err;

	/* 3. output address setting */
	ret = cis->ixc_ops->write16(client, 0x034C, cis_data->cur_width);
	if (ret < 0)
		 goto p_err;
	ret = cis->ixc_ops->write16(client, 0x034E, cis_data->cur_height);
	if (ret < 0)
		 goto p_err;

	/* If not use to binning, sensor image should set only crop */
	if (!binning) {
		dbg_sensor(1, "Sensor size set is not binning\n");
		goto p_err;
	}

	/* 4. sub sampling setting */
	even_x = 1;	/* 1: not use to even sampling */
	even_y = 1;
	odd_x = (ratio_w * 2) - even_x;
	odd_y = (ratio_h * 2) - even_y;

	ret = cis->ixc_ops->write16(client, 0x0380, even_x);
	if (ret < 0)
		 goto p_err;
	ret = cis->ixc_ops->write16(client, 0x0382, odd_x);
	if (ret < 0)
		 goto p_err;
	ret = cis->ixc_ops->write16(client, 0x0384, even_y);
	if (ret < 0)
		 goto p_err;
	ret = cis->ixc_ops->write16(client, 0x0386, odd_y);
	if (ret < 0)
		 goto p_err;

	/* 5. binnig setting */
	ret = cis->ixc_ops->write8(client, 0x0900, binning);	/* 1:  binning enable, 0: disable */
	if (ret < 0)
		goto p_err;
	ret = cis->ixc_ops->write8(client, 0x0901, (ratio_w << 4) | ratio_h);
	if (ret < 0)
		goto p_err;

	/* 6. scaling setting: but not use */
	/* scaling_mode (0: No scaling, 1: Horizontal, 2: Full) */
	ret = cis->ixc_ops->write16(client, 0x0400, 0x0000);
	if (ret < 0)
		goto p_err;
	/* down_scale_m: 1 to 16 upwards (scale_n: 16(fixed)) */
	/* down scale factor = down_scale_m / down_scale_n */
	ret = cis->ixc_ops->write16(client, 0x0404, 0x0010);
	if (ret < 0)
		goto p_err;

	cis_data->frame_time = (cis_data->line_readOut_time * cis_data->cur_height / 1000);
	cis->cis_data->rolling_shutter_skew = (cis->cis_data->cur_height - 1) * cis->cis_data->line_readOut_time;
	dbg_sensor(1, "[%s] frame_time(%d), rolling_shutter_skew(%lld)\n",
		__func__, cis->cis_data->frame_time, cis->cis_data->rolling_shutter_skew);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_4ha_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct is_device_sensor *device;
	ktime_t st = ktime_get();

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

	/* Sensor stream on */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	cis->cis_data->stream_on = true;
	sensor_4ha_fcount = 0;
	info("%s done\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_4ha_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();
	u8 cur_frame_count = 0;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);
	sensor_4ha_fcount = cur_frame_count;
	cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	info("%s done, frame_count(%d)\n", __func__, cur_frame_count);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_4ha_cis_recover_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	info("%s start\n", __func__);

	ret = sensor_4ha_cis_set_global_setting(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_4ha_cis_mode_change(subdev, cis->cis_data->sens_config_index_cur);
	if (ret < 0) goto p_err;
	ret = sensor_4ha_cis_stream_on(subdev);
	if (ret < 0) goto p_err;
	ret = sensor_cis_wait_streamon(subdev);
	if (ret < 0) goto p_err;

	info("%s end\n", __func__);
p_err:
	return ret;
}

int sensor_4ha_cis_wait_streamoff(struct v4l2_subdev *subdev)
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
		ret = cis->ixc_ops->read8(cis->client, 0x0005, &sensor_fcount);
		IXC_MUTEX_UNLOCK(cis->ixc_lock);
		if (ret < 0) {
			i2c_fail_cnt++;
			err("i2c transfer fail addr(0x0005), val(%x), try(%d), ret = %d",
				sensor_fcount, i2c_fail_cnt, ret);

			if (i2c_fail_cnt >= i2c_fail_max_cnt) {
				err("[MOD:D:%d] %s, i2c fail, i2c_fail_cnt(%d) >= i2c_fail_max_cnt(%d), sensor_fcount(%d)",
						cis->id, __func__, i2c_fail_cnt, i2c_fail_max_cnt, sensor_fcount);
				ret = -EINVAL;
				goto p_err;
			}
		}

		/* If fcount is '0xfe' or '0xff' in streamoff, delay by 33 ms. */
		if (sensor_4ha_fcount >= 0xFE && sensor_fcount == 0xFF) {
			usleep_range(33000, 33001);
			info("[%s] 33ms delay(stream_off fcount : %d, wait_stream_off fcount : %d)",
				__func__, sensor_4ha_fcount, sensor_fcount);
		}

		usleep_range(CIS_STREAM_OFF_WAIT_TIME, CIS_STREAM_OFF_WAIT_TIME + 1);
		wait_cnt++;

		if (wait_cnt >= time_out_cnt) {
			err("[MOD:D:%d] %s, time out, wait_limit(%d) > time_out(%d), sensor_fcount(%d)",
					cis->id, __func__, wait_cnt, time_out_cnt, sensor_fcount);
			ret = -EINVAL;
			goto p_err;
		}

		dbg_sensor(1, "[MOD:D:%d] %s, sensor_fcount(%d), (wait_limit(%d) < time_out(%d))\n",
				cis->id, __func__, sensor_fcount, wait_cnt, time_out_cnt);
	} while (sensor_fcount != 0xFF);

p_err:
	return ret;
}

int sensor_4ha_cis_clear_and_initialize(struct is_cis *cis)
{
	int ret = 0;
	ret = cis->ixc_ops->write8(cis->client, S5K4HA_OTP_R_W_MODE_ADDR, 0x04); /* clear error bit */
	usleep_range(1000, 1001); /* sleep 1msec */
	ret |= cis->ixc_ops->write8(cis->client, S5K4HA_OTP_R_W_MODE_ADDR, 0x00); /* initial command */
	if (ret < 0)
		err("failed to clear and init");

	return ret;
}

int sensor_4ha_cis_is_check_otp_status(struct is_cis *cis)
{
	int ret = 0;
	int retry = 2; /* IS_CAL_RETRY_CNT */
	u8 read_command_complete_check = 0;

	ret = cis->ixc_ops->write8(cis->client, S5K4HA_OTP_R_W_MODE_ADDR, 0x01); /* read mode */

	/* confirm write complete */
write_complete_retry:
	usleep_range(1000, 1001); /* sleep 1msec */

	ret = cis->ixc_ops->read8(cis->client, S5K4HA_OTP_CHECK_ADDR, &read_command_complete_check);
	if (ret < 0) {
		err("failed to check_opt_status");
		goto exit;
	}
	if (read_command_complete_check != 1) {
		if (retry >= 0) {
			retry--;
			goto write_complete_retry;
		}
		ret = -EINVAL;
		goto exit;
	}

exit:
	return ret;
}

u16 sensor_4ha_cis_get_otp_bank_4ha(struct is_cis *cis)
{
	int ret = 0;
	u8 otp_bank = 0;
	u16 curr_page = 0;

	/* The Bank details itself is present in bank-1 page-0 */
	ret = cis->ixc_ops->write8(cis->client, S5K4HA_OTP_PAGE_SELECT_ADDR, S5K4HA_OTP_START_PAGE_BANK1);
	ret |= cis->ixc_ops->read8(cis->client, S5K4HA_OTP_BANK_SELECT, &otp_bank);
	if (ret < 0) {
		err("failed to select_opt_bank");
		goto exit;
	}

	switch (otp_bank) {
	case 0x01:
		curr_page = S5K4HA_OTP_START_PAGE_BANK1;
		break;
	case 0x03:
		curr_page = S5K4HA_OTP_START_PAGE_BANK2;
		break;
	case 0x07:
		curr_page = S5K4HA_OTP_START_PAGE_BANK3;
		break;
	case 0x0F:
		curr_page = S5K4HA_OTP_START_PAGE_BANK4;
		break;
	case 0x1F:
		curr_page = S5K4HA_OTP_START_PAGE_BANK5;
		break;
	default:
		curr_page = S5K4HA_OTP_START_PAGE_BANK1;
		break;
	}

exit:
	info("%s: otp_bank = %d page_number = %x\n", __func__, otp_bank, curr_page);
	return curr_page;
}

int sensor_4ha_cis_get_otprom_data(struct v4l2_subdev *subdev, char *buf, bool camera_running, int rom_id)
{
	int ret = 0;
	u16 read_addr;
	int index;
	u16 cal_size = 0;
	u16 page_number = 0;
	u16 start_addr = 0;

	struct is_cis *cis = sensor_cis_get_cis(subdev);

	info("[%s] camera_running(%d)\n", __func__, camera_running);

	/* 1. prepare to otp read : sensor initial settings */
	if (camera_running == false)
		ret = sensor_cis_set_registers(subdev, sensor_otp_4ha_global, ARRAY_SIZE(sensor_otp_4ha_global));

	/* streaming on */
	ret |= cis->ixc_ops->write8(cis->client, S5K4HA_STREAM_ON_ADDR, 0x01);
	if (ret < 0) {
		err("failed to init_setting");
		goto exit;
	}
	msleep(50); /* sleep 50msec */

	/* confirm write complete */
	ret = sensor_4ha_cis_is_check_otp_status(cis);
	if (ret < 0) {
		err("failed to write_complete(%d)", ret);
		goto exit;
	}

	/* 2. select otp bank */
	page_number = sensor_4ha_cis_get_otp_bank_4ha(cis);

	/* 3. Read OTP Cal Data */
	start_addr = S5K4HA_OTP_START_ADDR;
	cal_size = S5K4HA_OTP_USED_CAL_SIZE;
	read_addr = start_addr;

	sensor_4ha_cis_clear_and_initialize(cis);
	sensor_4ha_cis_is_check_otp_status(cis);
	ret = cis->ixc_ops->write8(cis->client, S5K4HA_OTP_PAGE_SELECT_ADDR, page_number);
	ret = cis->ixc_ops->write8(cis->client, S5K4HA_OTP_PAGE_SELECT_ADDR, page_number);
	if (ret < 0) {
		err("failed to page_select ret(%d), read_addr(%#x) page_number(%#x)",
			ret, read_addr, page_number);
		goto exit;
	}

	for (index = 0; index < cal_size ; index++) {
		ret = cis->ixc_ops->read8(cis->client, read_addr, &buf[index]); /* OTP read */
		if (ret < 0) {
			err("failed to otp_read(%d)", ret);
			goto exit;
		}
		read_addr++;
		if (read_addr > S5K4HA_OTP_PAGE_ADDR_H) {
			read_addr = S5K4HA_OTP_PAGE_ADDR_L;
			page_number++;

			sensor_4ha_cis_clear_and_initialize(cis);
			sensor_4ha_cis_is_check_otp_status(cis);
			ret = cis->ixc_ops->write8(cis->client, S5K4HA_OTP_PAGE_SELECT_ADDR, page_number);
			if (ret < 0) {
				err("failed to page_select ret(%d), read_addr(%#x) page_number(%#x)",
					ret, read_addr, page_number);
				goto exit;
			}
		}
	}

	/* 4. Set to initial state */
exit:
	sensor_4ha_cis_clear_and_initialize(cis);

	/* streaming off */
	ret = cis->ixc_ops->write8(cis->client, S5K4HA_STREAM_ON_ADDR, 0x00);
	if (ret < 0) {
		err("failed to turn off streaming");
		return ret;
	}

	sensor_4ha_cis_wait_streamoff(subdev);
	usleep_range(1000, 1100);

	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_4ha_cis_init,
	.cis_log_status = sensor_4ha_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_4ha_cis_set_global_setting,
	.cis_mode_change = sensor_4ha_cis_mode_change,
//	.cis_set_size = sensor_4ha_cis_set_size,
	.cis_stream_on = sensor_4ha_cis_stream_on,
	.cis_stream_off = sensor_4ha_cis_stream_off,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_wait_streamoff = sensor_4ha_cis_wait_streamoff,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = sensor_cis_set_exposure_time,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
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
	.cis_set_digital_gain = sensor_cis_set_digital_gain,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
//	.cis_recover_stream_on = sensor_4ha_cis_recover_stream_on,
	.cis_get_otprom_data = sensor_4ha_cis_get_otprom_data,
};

int cis_4ha_probe_i2c(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret;
	struct is_cis *cis;
	struct is_device_sensor_peri *sensor_peri;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	client->addr = 0x2D;//for M54 4HA sensor bringup on A54 board due to 3L6 sharing same I2C ID as 4HA
	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;
	cis->reg_addr = &sensor_4ha_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("[%s] setfile_A mclk: 26Mhz \n", __func__);
		cis->sensor_info = &sensor_4ha_info_A;
	} else if (strcmp(setfile, "setB") == 0) {
		probe_info("[%s] setfile_B mclk: 26Mhz \n", __func__);
		cis->sensor_info = &sensor_4ha_info_B;
	} else
		err("setfile index out of bound, take default (setfile_A mclk: 26Mhz)");

	probe_info("%s done\n", __func__);

	return ret;
}

static int cis_4ha_remove(struct i2c_client *client)
{
	int ret = 0;
	return ret;
}

static const struct of_device_id sensor_cis_4ha_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-4ha",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_4ha_match);

static const struct i2c_device_id sensor_cis_4ha_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_4ha_driver = {
	.probe	= cis_4ha_probe_i2c,
	.remove	= cis_4ha_remove,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_4ha_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_4ha_idt
};
#ifdef MODULE
builtin_i2c_driver(sensor_cis_4ha_driver);
#else
static int __init sensor_cis_4ha_init(void)
{
        int ret;

        ret = i2c_add_driver(&sensor_cis_4ha_driver);
        if (ret)
                err("failed to add %s driver: %d\n",
                        sensor_cis_4ha_driver.driver.name, ret);

        return ret;
}
late_initcall_sync(sensor_cis_4ha_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
