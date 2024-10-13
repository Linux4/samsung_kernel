/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
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
#include <linux/syscalls.h>
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
#include "is-cis-imx258.h"
#include "is-cis-imx258-setA.h"

#include "is-helper-ixc.h"
#include "interface/is-interface-library.h"

#define SENSOR_NAME "IMX258"

static const struct v4l2_subdev_ops subdev_ops;

int sensor_imx258_cis_check_rev_on_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct i2c_client *client;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	u8 data8;
	int read_cnt = 0;

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

u32 sensor_imx258_cis_calc_again_code(u32 permile)
{
	if (permile > 0)
		return 512 - (512 * 1000 / permile);
	else
		return 0;
}

u32 sensor_imx258_cis_calc_again_permile(u32 code)
{
	if (code < 512)
		return ((512 * 1000) / (512 - code));
	else
		return 1000;
}

int sensor_imx258_cis_init(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->cur_width = cis->sensor_info->max_width;
	cis->cis_data->cur_height = cis->sensor_info->max_height;
	cis->cis_data->low_expo_start = 33000;
	cis->need_mode_change = false;
	cis->long_term_mode.sen_strm_off_on_step = 0;
	cis->long_term_mode.sen_strm_off_on_enable = false;

	cis->cis_data->sens_config_index_pre = SENSOR_IMX258_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	return ret;
}

int sensor_imx258_cis_init_state(struct v4l2_subdev *subdev, int mode)
{
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	cis->cis_data->stream_on = false;
	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;
	cis->mipi_clock_index_new = CAM_MIPI_NOT_INITIALIZED;
	cis->cis_data->cur_pattern_mode = SENSOR_TEST_PATTERN_MODE_OFF;

	return 0;
}

static const struct is_cis_log log_imx258[] = {
	{I2C_READ, 16, 0x0016, 0, "model_id"},
	{I2C_READ, 8, 0x0005, 0, "frame_count"},
	{I2C_READ, 16, 0x0100, 0, "mode_select"},
	{I2C_READ, 16, 0x0340, 0, "fll"},
	{I2C_READ, 16, 0x0342, 0, "llp"},
};

int sensor_imx258_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	sensor_cis_log_status(cis, log_imx258, ARRAY_SIZE(log_imx258), (char *)__func__);

	return ret;
}

int sensor_imx258_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_imx258_private_data *priv = NULL;

	priv = (struct sensor_imx258_private_data *)cis->sensor_info->priv;

	info("[%s] start\n", __func__);
	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0) {
		err("global setting fail!!");
		goto p_err;
	}

	info("[%s] done\n", __func__);

p_err:
	return ret;
}

int sensor_imx258_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	const struct sensor_cis_mode_info *mode_info;

	if (mode >= cis->sensor_info->mode_count) {
		err("invalid mode(%d)!!", mode);
		ret = -EINVAL;
		goto p_err;
	}

	cis->mipi_clock_index_cur = CAM_MIPI_NOT_INITIALIZED;

	info("[%s] sensor mode(%d)\n", __func__, mode);

	mode_info = cis->sensor_info->mode_infos[mode];

	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("sensor_imx258_set_registers fail!!");
		goto p_err;
	}

	/* Disable Embedded Data */
	ret = cis->ixc_ops->write8(cis->client, 0x4041, 0x00);
	if (ret < 0)
		err("disable emb data fail");

p_err:
	info("[%s] X\n", __func__);
	return ret;
}

int sensor_imx258_cis_stream_on(struct v4l2_subdev *subdev)
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
	cis->ixc_ops->write8(cis->client, 0x0100, 0x01);
	cis->cis_data->stream_on = true;

	info("%s\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx258_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	u8 cur_frame_count = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	cis->ixc_ops->read8(cis->client, 0x0005, &cur_frame_count);
	cis->ixc_ops->write8(cis->client, 0x0100, 0x00);
	info("%s: frame_count(0x%x)\n", __func__, cur_frame_count);

	cis->cis_data->stream_on = false;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

	return ret;
}

int sensor_imx258_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	u64 vt_pic_clk_freq_khz = 0;
	u16 long_coarse_int = 0;
	u16 short_coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	WARN_ON(!target_exposure);

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("invalid target exposure(%d, %d)",
			target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), target long(%d), short(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, target_exposure->long_val, target_exposure->short_val);

	vt_pic_clk_freq_khz = cis_data->pclk / (1000);
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	long_coarse_int = ((target_exposure->long_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;
	short_coarse_int = ((target_exposure->short_val * vt_pic_clk_freq_khz) / 1000 - min_fine_int) / line_length_pck;

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

	/* Short exposure */
	ret = cis->ixc_ops->write16(cis->client, cis->reg_addr->cit, short_coarse_int);
	if (ret < 0)
		goto p_err;

	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), vt_pic_clk_freq_khz (%llu),"
		KERN_CONT "line_length_pck(%d), min_fine_int (%d)\n", cis->id, __func__,
		cis_data->sen_vsync_count, vt_pic_clk_freq_khz, line_length_pck, min_fine_int);
	dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), frame_length_lines(%#x),"
		KERN_CONT "long_coarse_int %#x, short_coarse_int %#x\n", cis->id, __func__,
		cis_data->sen_vsync_count, cis_data->frame_length_lines, long_coarse_int, short_coarse_int);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_imx258_cis_set_analog_gain(struct v4l2_subdev *subdev, struct ae_param *again)
{
	int ret = 0;
	u16 analog_again = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	WARN_ON(!again);

	analog_again = (u16)CALL_CISOPS(cis, cis_calc_again_code, again->val);

	if (analog_again < cis->cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_again = cis->cis_data->min_analog_gain[0];
	}

	if (analog_again > cis->cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_again = cis->cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "%s [%d][%d] SS_CTL L[%d] => A_GAIN_L[%#x]\n",
		__func__, cis->id, cis->cis_data->sen_vsync_count, again->val, analog_again);

	ret = cis->ixc_ops->write16(cis->client, cis->reg_addr->again, analog_again);
	if (ret < 0)
		goto p_err;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_imx258_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	u16 long_dgain = 0;
	u16 short_dgain = 0;
	u16 dgains[4] = {0};
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	ktime_t st = ktime_get();

	WARN_ON(!dgain);

	long_dgain = (u16)CALL_CISOPS(cis, cis_calc_dgain_code, dgain->long_val);
	short_dgain = (u16)CALL_CISOPS(cis, cis_calc_dgain_code, dgain->short_val);

	if (long_dgain < cis->cis_data->min_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to min_digital_gain\n", __func__);
		long_dgain = cis->cis_data->min_digital_gain[0];
	}
	if (long_dgain > cis->cis_data->max_digital_gain[0]) {
		info("[%s] not proper long_gain value, reset to max_digital_gain\n", __func__);
		long_dgain = cis->cis_data->max_digital_gain[0];
	}

	if (short_dgain < cis->cis_data->min_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to min_digital_gain\n", __func__);
		short_dgain = cis->cis_data->min_digital_gain[0];
	}
	if (short_dgain > cis->cis_data->max_digital_gain[0]) {
		info("[%s] not proper short_gain value, reset to max_digital_gain\n", __func__);
		short_dgain = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "%s [%d][%d] SS_CTL L[%d] S[%d] => D_GAIN_L[%#x] D_GAIN_S[%#x]\n",
		__func__, cis->id, cis->cis_data->sen_vsync_count,
		dgain->long_val, dgain->short_val, long_dgain, short_dgain);

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = long_dgain;
	/* Long digital gain */
	ret = cis->ixc_ops->write16_array(cis->client, cis->reg_addr->dgain, dgains, 4);
	if (ret < 0)
		goto p_err;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %lldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err:
	return ret;
}

int sensor_imx258_cis_set_totalgain(struct v4l2_subdev *subdev, struct ae_param *target_exposure,
	struct ae_param *again, struct ae_param *dgain)
{
#define GAIN_X1 (1000)
#define GAIN_X1P125 (1125)
#define GAIN_X3 (3000)
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;
	struct ae_param total_again;
	struct ae_param total_dgain;

	WARN_ON(!target_exposure);
	WARN_ON(!again);
	WARN_ON(!dgain);

	cis_data = cis->cis_data;

	total_again.val = again->val;
	total_again.short_val = again->short_val;
	total_dgain.long_val = dgain->val;
	total_dgain.short_val = dgain->short_val;

	ret = sensor_imx258_cis_set_exposure_time(subdev, target_exposure);
	if (ret < 0) {
		err("[%s] sensor_imx258_cis_set_exposure_time fail\n", __func__);
		goto p_err;
	}

	if (total_again.val >= GAIN_X3) {
		total_again.val = (u32) ((total_again.val * GAIN_X1) / GAIN_X1P125);
		total_dgain.long_val = (u32) ((dgain->val * GAIN_X1P125) / 1000);
		dbg_sensor(2, "[%s] again[%d] dgain[%d] => total_again[%d] total_dgain[%d]",
			__func__, again->val, dgain->val, total_again.val, total_dgain.long_val);
	}

	ret = sensor_imx258_cis_set_analog_gain(subdev, &total_again);
	if (ret < 0) {
		err("[%s] sensor_cis_set_analog_gain fail\n", __func__);
		goto p_err;
	}

	ret = sensor_imx258_cis_set_digital_gain(subdev, &total_dgain);
	if (ret < 0) {
		err("[%s] sensor_cis_set_digital_gain fail\n", __func__);
		goto p_err;
	}

p_err:
	return ret;
}

static struct is_cis_ops cis_ops = {
	.cis_init = sensor_imx258_cis_init,
	.cis_init_state = sensor_imx258_cis_init_state,
	.cis_log_status = sensor_imx258_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_imx258_cis_set_global_setting,
	.cis_mode_change = sensor_imx258_cis_mode_change,
	.cis_stream_on = sensor_imx258_cis_stream_on,
	.cis_stream_off = sensor_imx258_cis_stream_off,
	.cis_wait_streamon = sensor_cis_wait_streamon,
	.cis_wait_streamoff = sensor_cis_wait_streamoff,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_exposure_time = NULL,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_set_long_term_exposure = sensor_cis_long_term_exposure,
	.cis_adjust_frame_duration = sensor_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = NULL,
	.cis_get_analog_gain = sensor_cis_get_analog_gain,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_calc_again_code = sensor_imx258_cis_calc_again_code,
	.cis_calc_again_permile = sensor_imx258_cis_calc_again_permile,
	.cis_set_digital_gain = NULL,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_calc_dgain_code = sensor_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_cis_calc_dgain_permile,
	.cis_set_totalgain = sensor_imx258_cis_set_totalgain,
	.cis_compensate_gain_for_extremely_br = sensor_cis_compensate_gain_for_extremely_br,
	.cis_check_rev_on_init = sensor_imx258_cis_check_rev_on_init,
};

static int cis_imx258_probe_i2c(struct i2c_client *client,
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
	cis->use_total_gain = true;
	cis->reg_addr = &sensor_imx258_reg_addr;

	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0)
		probe_info("%s setfile_A\n", __func__);
	else
		err("%s setfile index out of bound, take default (setfile_A)", __func__);

	cis->sensor_info = &sensor_imx258_info_A;
	is_vendor_set_mipi_mode(cis);

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_imx258_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-imx258",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_imx258_match);

static const struct i2c_device_id sensor_cis_imx258_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_imx258_driver = {
	.probe	= cis_imx258_probe_i2c,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_imx258_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_imx258_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_imx258_driver);
#else
static int __init sensor_cis_imx258_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_imx258_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_imx258_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_imx258_init);
#endif
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
