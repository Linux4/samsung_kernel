/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
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
#include "is-cis-imx355.h"
#include "is-cis-imx355-setB.h"

#include "is-helper-ixc.h"
#include "is-vender-specific.h"

#define SENSOR_NAME "IMX355"

static const struct v4l2_subdev_ops subdev_ops;

int sensor_imx355_cis_check_rev(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct i2c_client *client;
	struct is_cis *cis = NULL;
	u8 data8;
	int read_cnt = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		return ret;
	}

	memset(cis->cis_data, 0, sizeof(cis_shared_data));

	ret = cis->ixc_ops->write8(client, 0x0A02, 0x0F);
	ret |= cis->ixc_ops->write8(client, 0x0A00, 0x01);
	if (ret < 0) {
		err("cis->ixc_ops->write8 fail (ret %d)", ret);
		ret = -EAGAIN;
		goto p_err;
	}

	do {
		ret = cis->ixc_ops->read8(client, 0x0A01, &data8);
		read_cnt++;
		mdelay(1);
	} while (!(data8 & 0x01) && (read_cnt < 100));

	if (ret < 0) {
		err("cis->ixc_ops->read8 fail (ret %d)", ret);
		ret = -EAGAIN;
		goto p_err;
	}
	if ((data8 & 0x1) == false)
		err("status fail, (%d)", data8);

	/* 0x10 : PDAF(0APH5), 0x30 : Non PDAF(0ATH5) */
	ret = sensor_cis_check_rev(cis);
p_err:
	return ret;
}

u32 sensor_imx355_cis_calc_again_code(u32 permile)
{
	if (permile > 0)
		return 1024 - (1024 * 1000 / permile);
	else
		return 0;
}

u32 sensor_imx355_cis_calc_again_permile(u32 code)
{
	if (code < 1024)
		return ((1024 * 1000) / (1024 - code));
	else
		return 1000;
}

u32 sensor_imx355_cis_calc_dgain_code(u32 permile)
{
	u8 buf[2] = {0, 0};
	buf[0] = (permile / 1000) & 0x0F;
	buf[1] = (((permile - (buf[0] * 1000)) * 256) / 1000);

	return (buf[0] << 8 | buf[1]);
}

u32 sensor_imx355_cis_calc_dgain_permile(u32 code)
{
	return (((code & 0x0F00) >> 8) * 1000) + ((code & 0x00FF) * 1000 / 256);
}

int sensor_imx355_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;

	ktime_t st = ktime_get();

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	WARN_ON(!cis->cis_data);

	cis->rev_flag = false;

	ret = sensor_imx355_cis_check_rev(subdev);
	if (ret < 0) {
		warn("sensor_imx355_cis_check_rev is fail when cis init");
		cis->rev_flag = true;
		ret = 0;
	}

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->sens_config_index_pre = SENSOR_IMX355_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;

	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_imx355_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	u8 data8 = 0;
	u16 data16 = 0;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -ENODEV;
		goto p_err;
	}

	pr_info("[%s] *******************************\n", __func__);
	ret = cis->ixc_ops->read16(client, 0x0000, &data16);
	if (unlikely(!ret)) pr_info("model_id(0x%x)\n", data16);
	ret = cis->ixc_ops->read8(client, 0x0002, &data8);
	if (unlikely(!ret)) pr_info("revision_number(0x%x)\n", data8);
	ret = cis->ixc_ops->read8(client, 0x0005, &data8);
	if (unlikely(!ret)) pr_info("frame_count(0x%x)\n", data8);
	ret = cis->ixc_ops->read8(client, 0x0100, &data8);
	if (unlikely(!ret)) pr_info("mode_select(0x%x)\n", data8);

	pr_info("[%s] *******************************\n", __func__);

p_err:
	return ret;
}

int sensor_imx355_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;

	struct is_cis *cis = NULL;
	struct sensor_imx355_private_data *priv = NULL;
	WARN_ON(!subdev);
	cis = sensor_cis_get_cis(subdev);
	WARN_ON(!cis);
	priv = (struct sensor_imx355_private_data *)cis->sensor_info->priv;

	info("[%s] global setting start\n", __func__);
	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0) {
		err("global setting fail!!");
		goto p_err;
	}

	info("[%s] global setting done\n", __func__);

p_err:
	return ret;
}

int sensor_imx355_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	const struct sensor_cis_mode_info *mode_info;

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	info("[%s] sensor mode(%d)\n", __func__, mode);

	mode_info = cis->sensor_info->mode_infos[mode];

	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("sensor_imx355_set_registers fail!!");
		goto p_err;
	}

	/* Disable Embedded Data */
	ret = cis->ixc_ops->write8(client, 0x300C, 0x01);
	if (ret < 0) {
		err("disable emb data fail");
	}
p_err:
	return ret;
}

int sensor_imx355_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	struct is_device_sensor *device;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	device = (struct is_device_sensor *)v4l2_get_subdev_hostdata(subdev);
	WARN_ON(!device);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	is_vendor_set_mipi_clock(device);

#ifdef DEBUG_IMX355_PLL
	{
	u16 pll;
	ret = cis->ixc_ops->read16(client, 0x0301, &pll);
	dbg_sensor(1, "______ vt_pix_clk_div(%x)\n", pll);
	ret = cis->ixc_ops->read16(client, 0x0303, &pll);
	dbg_sensor(1, "______ vt_sys_clk_div(%x)\n", pll);
	ret = cis->ixc_ops->read16(client, 0x0305, &pll);
	dbg_sensor(1, "______ pre_pll_clk_div(%x)\n", pll);
	ret = cis->ixc_ops->read16(client, 0x0307, &pll);
	dbg_sensor(1, "______ pll_multiplier(%x)\n", pll);
	ret = cis->ixc_ops->read16(client, 0x0309, &pll);
	dbg_sensor(1, "______ op_pix_clk_div(%x)\n", pll);
	ret = cis->ixc_ops->read16(client, 0x030b, &pll);
	dbg_sensor(1, "______ op_sys_clk_div(%x)\n", pll);

	ret = cis->ixc_ops->read16(client, 0x030d, &pll);
	dbg_sensor(1, "______ secnd_pre_pll_clk_div(%x)\n", pll);
	ret = cis->ixc_ops->read16(client, 0x030f, &pll);
	dbg_sensor(1, "______ secnd_pll_multiplier(%x)\n", pll);
	ret = cis->ixc_ops->read16(client, 0x0340, &pll);
	dbg_sensor(1, "______ frame_length_lines(%x)\n", pll);
	ret = cis->ixc_ops->read16(client, 0x0342, &pll);
	dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}
#endif
	/* Sensor stream on */
	info("%s\n", __func__);
	cis->ixc_ops->write8(client, 0x0100, 0x01);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0100, 0x01, ret);

#if 0
	/* WDR */
	if (cis_data->is_data.wdr_enable == true)
		ret = cis->ixc_ops->write8(client, 0x0216, 0x01);
	else
		ret = cis->ixc_ops->write8(client, 0x0216, 0x00);

	if (ret < 0)
		err("i2c transfer fail addr(%x), ret = %d\n", 0x0216, ret);
#endif

	cis_data->stream_on = true;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx355_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	u8 cur_frame_count = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis->cis_data->stream_on = false; /* for not working group_param_hold after stream off */

	ret = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	cis->ixc_ops->read8(client, 0x0005, &cur_frame_count);
	info("%s: frame_count(0x%x)\n", __func__, cur_frame_count);

	cis->ixc_ops->write8(client, 0x0100, 0x00);
	if (ret < 0)
		err("i2c transfer fail addr(%x), val(%x), ret = %d\n", 0x0100, 0x00, ret);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx355_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u32 cit_lshift_val = 0;
	int cit_lshift_count = 0;
	struct is_long_term_expo_mode *lte_mode;
	u32 target_frame_duration = 0;
	u64 max_fll_frame_time;
	u32 target_exp = 0;
	u16 frame_length_lines = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!target_exposure);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("[%s] invalid target exposure(%d, %d)\n", __func__,
				target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	lte_mode = &cis->long_term_mode;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	target_exp = target_exposure->short_val;
	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	max_fll_frame_time = (65535 * ((u64)line_length_pck) * 1000) / (u64)vt_pic_clk_freq_khz;
	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night Hyper Laps: min exp. is 1.5sec)
	 * If frame duration over than 1sec, then sequence is same as below
	 * 1. set CIT_LSHFIT
	 * 2. set COARSE_INTEGRATION_TIME
	 * 3. set FRM_LENGTH_LINES
	 */
	if ((cis_data->is_data.scene_mode == AA_SCENE_MODE_SUPER_NIGHT || cis_data->is_data.scene_mode == AA_SCENE_MODE_HYPERLAPSE)
			&& lte_mode->sen_strm_off_on_enable == false && cis_data->cur_frame_us_time > max_fll_frame_time) {
		target_frame_duration = cis_data->cur_frame_us_time;
		dbg_sensor(1, "[MOD:D:%d] %s, input frame duration(%d) for CIT SHIFT\n",
			cis->id, __func__, target_frame_duration);

		if (target_frame_duration > 100000) {
			cit_lshift_val = (target_frame_duration / 100000);
			while (cit_lshift_val > 1) {
				cit_lshift_val = cit_lshift_val >> 1;
				target_frame_duration = target_frame_duration >> 1;
				target_exp = target_exp >> 1;
				cit_lshift_count++;
			}

			if (cit_lshift_count > SENSOR_IMX355_MAX_CIT_LSHIFT_VALUE)
				cit_lshift_count = SENSOR_IMX355_MAX_CIT_LSHIFT_VALUE;
		}

		frame_length_lines = (u16)((vt_pic_clk_freq_khz * target_frame_duration) / (line_length_pck * 1000));

		cis_data->frame_length_lines = frame_length_lines;
		cis_data->frame_length_lines_shifter = cit_lshift_count;
		cis_data->max_coarse_integration_time =
			frame_length_lines - cis_data->max_margin_coarse_integration_time;

		dbg_sensor(1, "[MOD:D:%d] %s, target_frame_duration(%d), frame_length_line(%d), cit_lshift_cnt(%d)\n",
			cis->id, __func__, target_frame_duration, frame_length_lines, cit_lshift_count);
		dbg_sensor(1, "[MOD:D:%d] %s, max_fll_frame_time=%d, cis_data->cur_frame_us_time=%d\n",
			cis->id, __func__, max_fll_frame_time, cis_data->cur_frame_us_time);

	}

	long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	short_coarse_int = ((target_exp * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;

	if (long_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->max_coarse_integration_time);
		long_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (short_coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) max(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->max_coarse_integration_time);
		short_coarse_int = cis_data->max_coarse_integration_time;
	}

	if (long_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, long_coarse_int, cis_data->min_coarse_integration_time);
		long_coarse_int = cis_data->min_coarse_integration_time;
	}

	if (short_coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), short coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, short_coarse_int, cis_data->min_coarse_integration_time);
		short_coarse_int = cis_data->min_coarse_integration_time;
	}

	if (lte_mode->sen_strm_off_on_enable == false && cis_data->min_frame_us_time > max_fll_frame_time) {
		if (cit_lshift_count > 0) {
			ret = cis->ixc_ops->write8(client, cis->reg_addr->cit_shifter, cit_lshift_count);
			if (ret < 0)
				goto p_err;
		}
	}

	/* Short exposure */
	ret = cis->ixc_ops->write16(client, cis->reg_addr->cit, short_coarse_int);
	if (ret < 0)
		goto p_err;

	if (lte_mode->sen_strm_off_on_enable == false && cis_data->min_frame_us_time > max_fll_frame_time) {
		ret = cis->ixc_ops->write16(client, cis->reg_addr->fll, frame_length_lines);
		if (ret < 0)
			goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%d),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n", cis->id, __func__,
		cis_data->sen_vsync_count, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
		KERN_CONT "long_coarse_int %#x, short_coarse_int %#x\n", cis->id, __func__,
		cis_data->sen_vsync_count, cis_data->frame_length_lines, long_coarse_int, short_coarse_int);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx355_cis_adjust_frame_duration(struct v4l2_subdev *subdev,
						u32 input_exposure_time,
						u32 *target_duration)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 frame_length_lines = 0;
	u32 frame_duration = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!target_duration);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	if (input_exposure_time == 0) {
		input_exposure_time  = cis_data->min_frame_us_time;
		info("[%s] Not proper exposure time(0), so apply min frame duration to exposure time forcely!!!(%d)\n",
			__func__, cis_data->min_frame_us_time);
	}

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	frame_length_lines = (u32)(((vt_pic_clk_freq_khz * input_exposure_time) / 1000) / line_length_pck);
	frame_length_lines += cis_data->max_margin_coarse_integration_time;

	frame_duration = (u32)(((u64)frame_length_lines * line_length_pck) * 1000 / vt_pic_clk_freq_khz);

	dbg_sensor(1, "[%s](vsync cnt = %d) input exp(%d), adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, input_exposure_time, frame_duration, cis_data->min_frame_us_time);
	dbg_sensor(1, "[%s](vsync cnt = %d) adj duration, frame duraion(%d), min_frame_us(%d)\n",
			__func__, cis_data->sen_vsync_count, frame_duration, cis_data->min_frame_us_time);

	*target_duration = MAX(frame_duration, cis_data->min_frame_us_time);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

	return ret;
}

int sensor_imx355_cis_set_frame_duration(struct v4l2_subdev *subdev, u32 frame_duration)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u16 frame_length_lines = 0;
	u64 max_fll_frame_time;
	struct is_long_term_expo_mode *lte_mode;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	lte_mode = &cis->long_term_mode;

	if (frame_duration < cis_data->min_frame_us_time) {
		dbg_sensor(1, "frame duration is less than min(%d)\n", frame_duration);
		frame_duration = cis_data->min_frame_us_time;
	}

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;

	max_fll_frame_time = (65535 * ((u64)line_length_pck) * 1000) / (u64)vt_pic_clk_freq_khz;
	/*
	 * For Long Exposure Mode without stream on_off. (ex. Night HyperLaps)
	 * If frame duration over than 1sec, then it has to be applied CIT shift.
	 * In this case, frame_duration is setted in set_exposure_time with CIT shift.
	 */
	if (lte_mode->sen_strm_off_on_enable == false && cis_data->min_frame_us_time > max_fll_frame_time) {
		cis_data->cur_frame_us_time = frame_duration;
		dbg_sensor(1, "[MOD:D:%d][%s] Skip set frame duration(%d) for CIT SHIFT.\n",
			cis->id, __func__, frame_duration);
		return ret;
	}

	frame_length_lines = (u16)((vt_pic_clk_freq_khz * frame_duration) / (line_length_pck * 1000));

	dbg_sensor(1, "[MOD:D:%d] %s, vt_pic_clk_freq_khz(%#x) frame_duration = %d us,"
		KERN_CONT "(line_length_pck%#x), frame_length_lines(%#x)\n",
		cis->id, __func__, vt_pic_clk_freq_khz, frame_duration, line_length_pck, frame_length_lines);

	if (lte_mode->sen_strm_off_on_enable == false && cis_data->frame_length_lines_shifter > 0) {
		cis_data->frame_length_lines_shifter = 0;
		ret = cis->ixc_ops->write8(client, cis->reg_addr->cit_shifter, 0);
		if (ret < 0) {
			err("cis->ixc_ops->write8 for cit shifter fail (ret %d)", ret);
			goto p_err;
		}
	}

	ret = cis->ixc_ops->write16(client, cis->reg_addr->fll, frame_length_lines);
	if (ret < 0)
		goto p_err;

	cis_data->cur_frame_us_time = frame_duration;
	cis_data->frame_length_lines = frame_length_lines;

	if (lte_mode->sen_strm_off_on_enable) {
		cis_data->max_coarse_integration_time =
			SENSOR_IMX355_MAX_COARSE_INTEG_WITH_FRM_LENGTH_CTRL - cis_data->max_margin_coarse_integration_time;
	} else {
		cis_data->max_coarse_integration_time = cis_data->frame_length_lines - cis_data->max_margin_coarse_integration_time;
	}


#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx355_cis_adjust_analog_gain(struct v4l2_subdev *subdev, u32 input_again, u32 *target_permile)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u32 again_code = 0;
	u32 again_permile = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!target_permile);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis_data = cis->cis_data;

	again_code = sensor_imx355_cis_calc_again_code(input_again);

	if (again_code > cis_data->max_analog_gain[0]) {
		again_code = cis_data->max_analog_gain[0];
	} else if (again_code < cis_data->min_analog_gain[0]) {
		again_code = cis_data->min_analog_gain[0];
	}

	again_permile = sensor_imx355_cis_calc_again_permile(again_code);

	dbg_sensor(1, "[%s] min again(%d), max(%d), input_again(%d), code(%d), permile(%d)\n", __func__,
			cis_data->max_analog_gain[0],
			cis_data->min_analog_gain[0],
			input_again,
			again_code,
			again_permile);

	*target_permile = again_permile;

	return ret;
}

int sensor_imx355_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	analog_gain = (u16)sensor_imx355_cis_calc_again_code(again->val);

	if (analog_gain < cis->cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis->cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis->cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_again = %d us, analog_gain(%#x)\n",
		cis->id, __func__, cis->cis_data->sen_vsync_count, again->val, analog_gain);

	ret = cis->ixc_ops->write16(client, cis->reg_addr->again, analog_gain);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx355_cis_get_analog_gain(struct v4l2_subdev *subdev, u32 *again)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	u16 analog_gain = 0;

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!again);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = cis->ixc_ops->read16(client, cis->reg_addr->again, &analog_gain);
	if (ret < 0)
		goto p_err;

	*again = sensor_imx355_cis_calc_again_permile(analog_gain);

	dbg_sensor(1, "[MOD:D:%d] %s, cur_again = %d us, analog_gain(%#x)\n",
			cis->id, __func__, *again, analog_gain);

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx355_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;

	u16 long_gain = 0;
	u16 short_gain = 0;
	u16 dgains[4] = {0};

#ifdef DEBUG_SENSOR_TIME
	struct timeval st, end;
	do_gettimeofday(&st);
#endif

	WARN_ON(!subdev);
	WARN_ON(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	long_gain = (u16)sensor_cis_calc_dgain_code(dgain->long_val);
	short_gain = (u16)sensor_cis_calc_dgain_code(dgain->short_val);

	if (long_gain < cis->cis_data->min_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to min_digital_gain\n", __func__);
		long_gain = cis->cis_data->min_digital_gain[0];
	}
	if (long_gain > cis->cis_data->max_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to max_digital_gain\n", __func__);
		long_gain = cis->cis_data->max_digital_gain[0];
	}

	if (short_gain < cis->cis_data->min_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to min_digital_gain\n", __func__);
		short_gain = cis->cis_data->min_digital_gain[0];
	}
	if (short_gain > cis->cis_data->max_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to max_digital_gain\n", __func__);
		short_gain = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain = %d/%d us, long_gain(%#x), short_gain(%#x)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->long_val, dgain->short_val, long_gain, short_gain);

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = long_gain;
	/* Long digital gain */
	ret = cis->ixc_ops->write16_array(client, cis->reg_addr->dgain, dgains, 4);
	if (ret < 0)
		goto p_err;

#ifdef DEBUG_SENSOR_TIME
	do_gettimeofday(&end);
	dbg_sensor(1, "[%s] time %lu us\n", __func__, (end.tv_sec - st.tv_sec)*1000000 + (end.tv_usec - st.tv_usec));
#endif

p_err:
	return ret;
}

int sensor_imx355_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;

	u64 vt_pic_clk_freq_khz = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	u32 coarse_int = 0;
	u32 compensated_dgain = 0;

	WARN_ON(!subdev);
	WARN_ON(!again);
	WARN_ON(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);
	if (!cis) {
		err("cis is NULL");
		ret = -EINVAL;
		goto p_err;
	}
	cis_data = cis->cis_data;

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	if (line_length_pck <= 0) {
		err("[%s] invalid line_length_pck(%d)\n", __func__, line_length_pck);
		goto p_err;
	}

	coarse_int = ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	if (coarse_int <= 100) {
		compensated_dgain = (*dgain * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);

		if (compensated_dgain < cis_data->min_digital_gain[0]) {
			compensated_dgain = cis_data->min_digital_gain[0];
		} else if (compensated_dgain >= cis_data->max_digital_gain[0]) {
			*again = (*again * ((expo * vt_pic_clk_freq_khz) / 1000 - min_fine_int)) / (line_length_pck * coarse_int);
			compensated_dgain = cis_data->max_digital_gain[0];
		}
		*dgain = compensated_dgain;

		dbg_sensor(1, "[%s] exp(%d), again(%d), dgain(%d), coarse_int(%d), compensated_dgain(%d)\n",
			__func__, expo, *again, *dgain, coarse_int, compensated_dgain);
	}

p_err:
	return ret;
}

int sensor_imx355_cis_set_totalgain(struct v4l2_subdev *subdev, struct ae_param *target_exposure,
	struct ae_param *again, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;
	struct ae_param total_again;
	struct ae_param total_dgain;
	u32 cal_analog_val1 = 0;
	u32 cal_analog_val2 = 0;
	u32 cal_digital = 0;
	u32 analog_gain = 0;

	FIMC_BUG(!subdev);
	FIMC_BUG(!target_exposure);
	FIMC_BUG(!again);
	FIMC_BUG(!dgain);

	cis_data = cis->cis_data;

	total_again.val = again->val;
	total_again.short_val = again->short_val;
	total_dgain.long_val = dgain->val;
	total_dgain.short_val = dgain->short_val;
	ret = sensor_imx355_cis_set_exposure_time(subdev, target_exposure);
	if (ret < 0) {
		err("[%s] sensor_imx355_cis_set_exposure_time fail\n", __func__);
		goto p_err;
	}

	analog_gain = sensor_imx355_cis_calc_again_code(again->val);
	/* Set Digital gains */
	if (analog_gain == cis_data->max_analog_gain[0]) {
		total_dgain.long_val = dgain->val;
	} else {
		cal_analog_val1 = sensor_imx355_cis_calc_again_code(again->val);
		cal_analog_val2 = sensor_imx355_cis_calc_again_permile(cal_analog_val1);
		cal_digital = (again->val * 1000) / cal_analog_val2;
		if (cal_digital < 1000)
			cal_digital = 1000;
		total_dgain.long_val = total_dgain.long_val * cal_digital / 1000;
		total_again.val = cal_analog_val2;
		dbg_sensor(1, "[%s] cal_analog_val1 = %d, cal_analog_val2 = %d cal_digital = %d\n",
			__func__, again->val, cal_analog_val2, cal_digital);
		if (cal_digital < 0) {
			err("calculate dgain is fail");
			goto p_err;
		}
		dbg_sensor(1, "[%s] dgain compensation : input_again = %d, calculated_analog_gain = %d\n",
			__func__, again->val, cal_analog_val2);
	}

	ret = sensor_imx355_cis_set_analog_gain(subdev, &total_again);
	if (ret < 0) {
		err("[%s] sensor_cis_set_analog_gain fail\n", __func__);
		goto p_err;
	}

	ret = sensor_imx355_cis_set_digital_gain(subdev, &total_dgain);
	if (ret < 0) {
		err("[%s] sensor_cis_set_digital_gain fail\n", __func__);
		goto p_err;
	}

p_err:
	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_imx355_cis_init,
	.cis_log_status = sensor_imx355_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_imx355_cis_set_global_setting,
	.cis_mode_change = sensor_imx355_cis_mode_change,
	.cis_stream_on = sensor_imx355_cis_stream_on,
	.cis_stream_off = sensor_imx355_cis_stream_off,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = NULL,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_adjust_frame_duration = sensor_imx355_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_imx355_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = NULL,
	.cis_get_analog_gain = sensor_imx355_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_calc_again_code = sensor_imx355_cis_calc_again_code,
	.cis_calc_again_permile = sensor_imx355_cis_calc_again_permile,
	.cis_set_digital_gain = NULL,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_imx355_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_imx355_cis_calc_dgain_permile,
	.cis_set_totalgain = sensor_imx355_cis_set_totalgain,
	.cis_compensate_gain_for_extremely_br = sensor_imx355_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
};

static int cis_imx355_probe_i2c(struct i2c_client *client,
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

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_RG_GB;
	cis->use_wb_gain = true;
	cis->use_total_gain = true;
	cis->reg_addr = &sensor_imx355_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setB") == 0)
		probe_info("%s setfile_B\n", __func__);
	else
		err("%s setfile index out of bound, take default (setfile_B)", __func__);

	cis->sensor_info = &sensor_imx355_info_B;

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_imx355_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx355",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx355_match);

static const struct i2c_device_id sensor_cis_imx355_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx355_driver = {
	.probe	= cis_imx355_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx355_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx355_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_imx355_driver);
#else
static int __init sensor_cis_imx355_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_imx355_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx355_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx355_init);
#endif
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
