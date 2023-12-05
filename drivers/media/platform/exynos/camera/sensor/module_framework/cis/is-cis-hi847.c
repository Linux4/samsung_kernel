/*
 * Samsung Exynos SoC series Sensor driver
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
#include "is-cis-hi847.h"
#include "is-cis-hi847-setA.h"
#include "is-helper-ixc.h"
#ifdef CONFIG_CAMERA_VENDER_MCD_V2
#include "is-sec-define.h"
#endif

#include "interface/is-interface-library.h"
#include "is-vender.h"
#include "is-vender-specific.h"

#define SENSOR_NAME "HI847"

#define POLL_TIME_MS 5
#define STREAM_MAX_POLL_CNT 60

static const u32 *sensor_hi847_otp_initial;
static u32 sensor_hi847_otp_initial_size;

/*************************************************
 *  [HI847 Analog gain formular]
 *
 *  Analog Gain = (Reg value)/16 + 1
 *
 *  Analog Gain Range = x1.0 to x16.0
 *
 *************************************************/

u32 sensor_hi847_cis_calc_again_code(u32 permile)
{
	return ((permile - 1000) * 16 / 1000);
}

u32 sensor_hi847_cis_calc_again_permile(u32 code)
{
	return ((code * 1000 / 16) + 1000);
}

/*************************************************
 *  [HI847 Digital gain formular]
 *
 *  Digital Gain = bit[12:9] + bit[8:0]/512 (Gr, Gb, R, B)
 *
 *  Digital Gain Range = x1.0 to x15.99
 *
 *************************************************/

u32 sensor_hi847_cis_calc_dgain_code(u32 permile)
{
	u32 buf[2] = {0, 0};
	if (permile > 15990)
		permile = 15990;
	buf[0] = ((permile / 1000) & 0x0F) << 9;
	buf[1] = ((((permile % 1000) * 512) / 1000) & 0x1FF);
	return (buf[0] | buf[1]);
}

u32 sensor_hi847_cis_calc_dgain_permile(u32 code)
{
	return (((code & 0x1E00) >> 9) * 1000) + ((code & 0x01FF) * 1000 / 512);
}

/* CIS OPS */
int sensor_hi847_cis_init(struct v4l2_subdev *subdev)
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

	cis->cis_data->sens_config_index_pre = SENSOR_HI847_MODE_MAX;
	cis->cis_data->sens_config_index_cur = 0;
	CALL_CISOPS(cis, cis_data_calculation, subdev, cis->cis_data->sens_config_index_cur);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));
	return ret;
}

int sensor_hi847_cis_log_status(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client = NULL;
	u16 data16 = 0;
	u8 data8 = 0;

	FIMC_BUG(!subdev);

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

	IXC_MUTEX_LOCK(cis->ixc_lock);

	pr_info("[%s] *******************************\n", __func__);
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_MODEL_ID_ADDR, &data16);
	if (unlikely(!ret)) pr_info("model id(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_STREAM_MODE_ADDR, &data16);
	if (unlikely(!ret)) pr_info("streaming mode(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_ISP_EN_ADDR, &data16);
	if (unlikely(!ret)) pr_info("ISP EN(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_COARSE_INTEG_TIME_ADDR, &data16);
	if (unlikely(!ret)) pr_info("coarse_integration_time(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read8(client, SENSOR_HI847_ANALOG_GAIN_ADDR, &data8);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data8);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_DIGITAL_GAIN_GR_ADDR, &data16);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_DIGITAL_GAIN_GB_ADDR, &data16);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_DIGITAL_GAIN_R_ADDR, &data16);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_DIGITAL_GAIN_B_ADDR, &data16);
	if (unlikely(!ret)) pr_info("gain_code_global(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_FRAME_LENGTH_LINE_ADDR, &data16);
	if (unlikely(!ret)) pr_info("frame_length_line(0x%x)\n", data16);
	else goto p_i2c_err;
	ret = cis->ixc_ops->read16(client, SENSOR_HI847_LINE_LENGTH_PCK_ADDR, &data16);
	if (unlikely(!ret)) pr_info("line_length_pck(0x%x)\n", data16);
	else goto p_i2c_err;
	ret= cis->ixc_ops->read8(client, 0x1004, &data8);
	if (unlikely(!ret)) pr_info("mipi img data id ctrl (0x%x)\n", data8);
	else goto p_i2c_err;
	ret= cis->ixc_ops->read8(client, 0x1005, &data8);
	if (unlikely(!ret)) pr_info("mipi pd data id ctrl (0x%x)\n", data8);
	else goto p_i2c_err;
	ret= cis->ixc_ops->read8(client, 0x1038, &data8);
	if (unlikely(!ret)) pr_info("mipi virtual channel ctrl (0x%x)\n", data8);
	else goto p_i2c_err;
	ret= cis->ixc_ops->read8(client, 0x1042, &data8);
	if (unlikely(!ret)) pr_info("mipi pd seperation ctrl (0x%x)\n", data8);
	else goto p_i2c_err;

	pr_info("[%s] *******************************\n", __func__);

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

p_err:
	return ret;
}

int sensor_hi847_cis_set_global_setting(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	struct sensor_hi847_private_data*priv = (struct sensor_hi847_private_data*)cis->sensor_info->priv;

	ret = sensor_cis_write_registers_locked(subdev, priv->global);
	if (ret < 0)
		err("global setting fail!!");

	dbg_sensor(1, "[%s] global setting done\n", __func__);

	return ret;
}

int sensor_hi847_cis_mode_change(struct v4l2_subdev *subdev, u32 mode)
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

	mode_info = cis->sensor_info->mode_infos[mode];

	info("[%s] sensor mode(%d)\n", __func__, mode);
	ret = sensor_cis_write_registers_locked(subdev, mode_info->setfile);
	if (ret < 0) {
		err("sensor_setfiles fail!!");
		goto p_err;
	}

	cis->cis_data->sens_config_index_pre = mode;
	info("[%s] mode changed(%d)\n", __func__, mode);

p_err:
	return ret;
}

int sensor_hi847_cis_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	IXC_MUTEX_LOCK(cis->ixc_lock);
#ifdef SENSOR_HI847_DEBUG_INFO
	{
	u16 pll;
	cis->ixc_ops->read16(client, 0x0702, &pll);
	dbg_sensor(1, "______ pll_cfg(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x0732, &pll);
	dbg_sensor(1, "______ pll_clkgen_en_ramp(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x0736, &pll);
	dbg_sensor(1, "______ pll_mdiv_ramp(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x0738, &pll);
	dbg_sensor(1, "______ pll_prediv_ramp(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x073C, &pll);
	dbg_sensor(1, "______ pll_tg_vt_sys_div1(%x)\n", (pll & 0x0F00));
	dbg_sensor(1, "______ pll_tg_vt_sys_div2(%x)\n", (pll & 0x0001));
	cis->ixc_ops->read16(client, 0x0742, &pll);
	dbg_sensor(1, "______ pll_clkgen_en_mipi(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x0746, &pll);
	dbg_sensor(1, "______ pll_mdiv_mipi(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x0748, &pll);
	dbg_sensor(1, "______ pll_prediv_mipi(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x074A, &pll);
	dbg_sensor(1, "______ pll_mipi_vt_sys_div1(%x)\n", (pll & 0x0F00));
	dbg_sensor(1, "______ pll_mipi_vt_sys_div2(%x)\n", (pll & 0x0011));
	cis->ixc_ops->read16(client, 0x074C, &pll);
	dbg_sensor(1, "______ pll_mipi_div1(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x074E, &pll);
	dbg_sensor(1, "______ pll_mipi_byte_div(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x020E, &pll);
	dbg_sensor(1, "______ frame_length_line(%x)\n", pll);
	cis->ixc_ops->read16(client, 0x0206, &pll);
	dbg_sensor(1, "______ line_length_pck(%x)\n", pll);
	}
#endif

#if SENSOR_HI847_PDAF_DISABLE
	cis->ixc_ops->write16(client, 0x0B04, 0x00FC);
	cis->ixc_ops->write16(client, 0x1004, 0x2BB0);
	cis->ixc_ops->write16(client, 0x1038, 0x0000);
	cis->ixc_ops->write16(client, 0x1042, 0x0008);
#endif

	/* Sensor stream on */
	info("%s\n", __func__);
	ret = cis->ixc_ops->write16(client, SENSOR_HI847_STREAM_MODE_ADDR, 0x0100);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI847_STREAM_MODE_ADDR, ret);
		goto p_i2c_err;
	}

	cis_data->stream_on = true;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_hi847_cis_set_test_pattern(struct v4l2_subdev *subdev, camera2_sensor_ctl_t *sensor_ctl)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	client = cis->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	dbg_sensor(1, "[MOD:D:%d] %s, cur_pattern_mode(%d), testPatternMode(%d)\n", cis->id, __func__,
			cis->cis_data->cur_pattern_mode, sensor_ctl->testPatternMode);

	if (cis->cis_data->cur_pattern_mode != sensor_ctl->testPatternMode) {
		info("%s REG : 0x0B04 write to 0x00dd", __func__);
		cis->ixc_ops->write16(client, 0x0B04, 0x00dd);

		cis->cis_data->cur_pattern_mode = sensor_ctl->testPatternMode;
		if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_OFF) {
			info("%s: set DEFAULT pattern! (testpatternmode : %d)\n", __func__, sensor_ctl->testPatternMode);

			IXC_MUTEX_LOCK(cis->ixc_lock);
			cis->ixc_ops->write16(client, 0x0C0A, 0x0000);
			IXC_MUTEX_UNLOCK(cis->ixc_lock);
		} else if (sensor_ctl->testPatternMode == SENSOR_TEST_PATTERN_MODE_BLACK) {
			info("%s: set BLACK pattern! (testpatternmode :%d), Data : 0x(%x, %x, %x, %x)\n",
				__func__, sensor_ctl->testPatternMode,
				(unsigned short)sensor_ctl->testPatternData[0],
				(unsigned short)sensor_ctl->testPatternData[1],
				(unsigned short)sensor_ctl->testPatternData[2],
				(unsigned short)sensor_ctl->testPatternData[3]);

			IXC_MUTEX_LOCK(cis->ixc_lock);
			cis->ixc_ops->write16(client, 0x0C0A, 0x0100);
			IXC_MUTEX_UNLOCK(cis->ixc_lock);
		}
	}

p_err:
	return ret;
}

int sensor_hi847_cis_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis;
	struct i2c_client *client;
	cis_shared_data *cis_data;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	dbg_sensor(1, "[MOD:D:%d] %s\n", cis->id, __func__);

	ret = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (ret < 0)
		err("group_param_hold_func failed at stream off");

	IXC_MUTEX_LOCK(cis->ixc_lock);

	info("%s\n", __func__);
	ret = cis->ixc_ops->write16(client, SENSOR_HI847_STREAM_MODE_ADDR, 0x0000);
	if (ret < 0) {
		err("i2c transfer fail addr(%x) ret = %d\n",
				SENSOR_HI847_STREAM_MODE_ADDR, ret);
		goto p_i2c_err;
	}

	cis_data->stream_on = false;

	info("[%s] Done.\n", __func__);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

	return ret;
}

int sensor_hi847_cis_get_otprom_data(struct v4l2_subdev *subdev, char *buf, bool camera_running, int rom_id)
{
	int ret = 0;
	u8 bank;
	const u16 read_addr = HI847_OTP_BANK_DATA_ADDR;
	u16 start_addr = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);

	info("[%s] camera_running(%d)\n", __func__, camera_running);

	IXC_MUTEX_LOCK(cis->ixc_lock);

	/* 1. prepare to otp read : sensor initial settings */
	if (camera_running == false)
		ret = sensor_cis_set_registers_addr8(subdev, sensor_otp_hi847_global, ARRAY_SIZE(sensor_otp_hi847_global));

	if (ret == -EINVAL) {
		err("OTP global setting failed. ret(%d)", ret);
		goto p_err_unlock;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x0B00, 0x00); /* standby off */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0B00, 0x0000);
		goto p_err_unlock;
	}

	usleep_range(10000, 10000); /* sleep 10msec */

	ret = cis->ixc_ops->write8(cis->client, 0x0260, 0x10); /* OTP mode enable */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0260, 0x10);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x030F, 0x14); /* OTP status */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x030F, 0x14);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x0B00, 0x01); /* standby on */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0B00, 0x01);
		goto exit;
	}

	usleep_range(1000, 1000); /* sleep 1msec */

	/* read otp bank */
	ret = cis->ixc_ops->write8(cis->client, 0x030A, ((HI847_OTP_BANK_SEL_ADDR) >> 8) & 0xFF); /* upper 8bit */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x030A, ((HI847_OTP_BANK_SEL_ADDR) >> 8) & 0xFF);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x030B, HI847_OTP_BANK_SEL_ADDR & 0xFF); /* lower 8bit */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x030B, HI847_OTP_BANK_SEL_ADDR & 0xFF);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x031C, 0x00); /* OTP Signal */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x031C, 0x00);
		goto exit;
	}
	
	ret = cis->ixc_ops->write8(cis->client, 0x031D, 0x00); /* OTP Signal */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x031D, 0x00);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x0302, 0x01); /* read mode */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0302, 0x01);
		goto exit;
	}

	ret = cis->ixc_ops->read8(cis->client, read_addr, &bank);
	if (unlikely(ret)) {
		err("failed to read OTP bank address (%d)\n", ret);
		goto exit;
	}

	/* select start address */
	switch (bank) {
	case 0x01:
		start_addr = HI847_OTP_START_ADDR_BANK1;
		break;
	case 0x03:
		start_addr = HI847_OTP_START_ADDR_BANK2;
		break;
	case 0x07:
		start_addr = HI847_OTP_START_ADDR_BANK3;
		break;
	case 0x0F:
		start_addr = HI847_OTP_START_ADDR_BANK4;
		break;
	default:
		start_addr = HI847_OTP_START_ADDR_BANK1;
		break;
	}

	info("%s: otp_bank = 0x%x start_addr = 0x%x\n", __func__, bank, start_addr);

	/* OTP burst read */
	ret = cis->ixc_ops->write8(cis->client, 0x030A, ((start_addr) >> 8) & 0xFF); /* upper 8bit */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x030A, ((start_addr) >> 8) & 0xFF);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x030B, start_addr & 0xFF); /* lower 8bit */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x030B, start_addr & 0xFF);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x031C, 0x00); /* OTP Signal */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x031C, 0x00);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x031D, 0x00); /* OTP Signal */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x031D, 0x00);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x0302, 0x01); /* read mode */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0302, 0x01);
		goto exit;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x0712, 0x01); /* burst read register on */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0712, 0x01);
		goto exit;
	}

	info("Camera: I2C read cal data for rom_id:%d\n", rom_id);
	ret = cis->ixc_ops->read8_size(cis->client, &buf[0], read_addr, IS_READ_MAX_HI847_OTP_CAL_SIZE);
	if (ret != IS_READ_MAX_HI847_OTP_CAL_SIZE) {
		err("failed to is_i2c_read (%d)\n", ret);
		ret = -EINVAL;
		goto exit;
	}

exit:
	ret = cis->ixc_ops->write8(cis->client, 0x0712, 0x00); /* burst read register off */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0712, 0x00);
		goto p_err_unlock;
	}

	/* streaming mode change */
	ret = cis->ixc_ops->write8(cis->client, 0x0809, 0x00); /* stream off */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0809, 0x00);
		goto p_err_unlock;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x0b00, 0x00); /* stream off */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0b00, 0x00);
		goto p_err_unlock;
	}

	usleep_range(10000, 10000); /* sleep 10msec */
	ret = cis->ixc_ops->write8(cis->client, 0x0260, 0x00); /* OTP mode disable */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0260, 0x00);
		goto p_err_unlock;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x0809, 0x01); /* stream on */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0809, 0x01);
		goto p_err_unlock;
	}

	ret = cis->ixc_ops->write8(cis->client, 0x0b00, 0x01); /* stream on */
	if (ret < 0) {
		err("is->ixc_ops->write8 fail, ret(%d), addr(%#x), data(%#x)\n",
			ret, 0x0b00, 0x01);
		goto p_err_unlock;
	}

p_err_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	usleep_range(1000, 1000); /* sleep 1msec */
	return ret;
}

int sensor_hi847_cis_set_digital_gain(struct v4l2_subdev *subdev, struct ae_param *dgain)
{
	int ret = 0;
	int hold = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	cis_shared_data *cis_data = NULL;

	u16 dgain_code = 0;
	u16 dgains[4] = {0};
	u16 read_val = 0;
	u16 enable_dgain = 0;
	ktime_t st = ktime_get();

	FIMC_BUG(!subdev);
	FIMC_BUG(!dgain);

	cis = (struct is_cis *)v4l2_get_subdevdata(subdev);

	FIMC_BUG(!cis);
	FIMC_BUG(!cis->cis_data);

	client = cis->client;
	FIMC_BUG(!client);

	cis_data = cis->cis_data;

	dgain_code = (u16)sensor_hi847_cis_calc_dgain_code(dgain->val);

	if (dgain_code < cis->cis_data->min_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to min_digital_gain\n", __func__);
		dgain_code = cis->cis_data->min_digital_gain[0];
	}
	if (dgain_code > cis->cis_data->max_digital_gain[0]) {
		info("[%s] not proper dgain_code value, reset to max_digital_gain\n", __func__);
		dgain_code = cis->cis_data->max_digital_gain[0];
	}

	dbg_sensor(1, "[MOD:D:%d] %s(vsync cnt = %d), input_dgain permile(%d), dgain_code(0x%X)\n",
			cis->id, __func__, cis->cis_data->sen_vsync_count, dgain->val, dgain_code);

	hold = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
	if (hold < 0) {
		ret = hold;
		goto p_i2c_err;
	}

	IXC_MUTEX_LOCK(cis->ixc_lock);

	dgains[0] = dgains[1] = dgains[2] = dgains[3] = dgain_code;
	ret = cis->ixc_ops->write16_array(client, SENSOR_HI847_DIGITAL_GAIN_ADDR, dgains, 4);
	if (ret < 0) {
		goto p_i2c_err;
	}

	ret = cis->ixc_ops->read16(client, SENSOR_HI847_ISP_EN_ADDR, &read_val);
	if (ret < 0) {
		goto p_i2c_err;
	}

	enable_dgain = read_val | (0x1 << 4); // B[4]: D gain enable
	ret = cis->ixc_ops->write16(client, SENSOR_HI847_ISP_EN_ADDR, enable_dgain);
	if (ret < 0) {
		goto p_i2c_err;
	}

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_i2c_err:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	if (hold > 0) {
		hold = CALL_CISOPS(cis, cis_group_param_hold, subdev, false);
		if (hold < 0)
			ret = hold;
	}

	return ret;
}

int sensor_hi847_cis_wait_streamoff(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *) v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	client = cis->client;
	FIMC_BUG(!client);

	do {
		ret = cis->ixc_ops->read16(client, SENSOR_HI847_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n",
					SENSOR_HI847_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x \n", __func__, PLL_en);
		/* pll_bypass */
		if (!(PLL_en & 0x0100))
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);

	if (wait_cnt < STREAM_MAX_POLL_CNT) {
		info("%s: finished after %d ms\n", __func__,
				(wait_cnt + 1) * POLL_TIME_MS);
	} else {
		warn("%s: finished : polling timeout occurred after %d ms\n", __func__,
				(wait_cnt + 1) * POLL_TIME_MS);
	}

p_err:
	return ret;
}

int sensor_hi847_cis_wait_streamon(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct i2c_client *client = NULL;
	u32 wait_cnt = 0;
	u16 PLL_en = 0;

	FIMC_BUG(!subdev);

	cis = (struct is_cis *) v4l2_get_subdevdata(subdev);
	FIMC_BUG(!cis);

	client = cis->client;
	FIMC_BUG(!client);

	probe_info("[%s] start\n", __func__);

	do {
		ret = cis->ixc_ops->read16(client, SENSOR_HI847_ISP_PLL_ENABLE_ADDR, &PLL_en);
		if (ret < 0) {
			err("i2c transfer fail addr(%x) ret = %d\n", SENSOR_HI847_ISP_PLL_ENABLE_ADDR, ret);
			goto p_err;
		}

		dbg_sensor(1, "%s: PLL_en 0x%x\n", __func__, PLL_en);
		/* pll_enable */
		if (PLL_en & 0x0100)
			break;

		wait_cnt++;
		msleep(POLL_TIME_MS);
	} while (wait_cnt < STREAM_MAX_POLL_CNT);

	if (wait_cnt < STREAM_MAX_POLL_CNT) {
		info("%s: finished after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	} else {
		warn("%s: finished : polling timeout occurred after %d ms\n", __func__, (wait_cnt + 1) * POLL_TIME_MS);
	}

p_err:
	return ret;
}

int sensor_hi847_cis_set_analog_digital_gain(struct v4l2_subdev *subdev, struct ae_param *again, struct ae_param *dgain)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;

	u32 cal_analog_val1 = 0;
	u32 cal_analog_val2 = 0;
	u32 cal_digital = 0;
	u8 analog_gain = 0;

	u16 dgain_code = 0;
	u16 dgains[4] = {0};
	u16 read_val = 0;
	u16 enable_dgain = 0;
	ktime_t st = ktime_get();

	cis_data = cis->cis_data;

	analog_gain = (u8)sensor_hi847_cis_calc_again_code(again->val);

	if (analog_gain < cis_data->min_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to min_analog_gain\n", __func__);
		analog_gain = cis_data->min_analog_gain[0];
	}

	if (analog_gain > cis_data->max_analog_gain[0]) {
		info("[%s] not proper analog_gain value, reset to max_analog_gain\n", __func__);
		analog_gain = cis_data->max_analog_gain[0];
	}

	dbg_sensor(1, "%s [%d] VCNT[%d] input_again[%d] analog_gain[%d]\n",
		__func__, cis->id, cis_data->sen_vsync_count, again->val, analog_gain);

	analog_gain &= 0xFF;

	/* Set Analog gains */
	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write8(cis->client, cis->reg_addr->again, analog_gain);
	if (ret < 0)
		goto p_err_unlock;

	/* Set Digital gains */
	if (analog_gain == cis_data->max_analog_gain[0]) {
		dgain_code = (u16)sensor_hi847_cis_calc_dgain_code(dgain->val);
		if (dgain_code < cis_data->min_digital_gain[0]) {
			info("[%s] not proper dgain_code value, reset to min_digital_gain\n", __func__);
			dgain_code = cis_data->min_digital_gain[0];
		}
		if (dgain_code > cis_data->max_digital_gain[0]) {
			info("[%s] not proper dgain_code value, reset to max_digital_gain\n", __func__);
			dgain_code = cis_data->max_digital_gain[0];
		}
	} else {
		cal_analog_val1 = sensor_hi847_cis_calc_again_code(again->val);
		cal_analog_val2 = sensor_hi847_cis_calc_again_permile(cal_analog_val1);
		cal_digital = (again->val * 1000) / cal_analog_val2;
		dgain_code = (u16)sensor_hi847_cis_calc_dgain_code(cal_digital);
		if (cal_digital < 0) {
			err("calculate dgain is fail");
			goto p_err_unlock;
		}
		dbg_sensor(1, "[%s] dgain compensation : input_again = %d, calculated_analog_gain = %d\n",
			__func__, again->val, cal_analog_val2);
	}

	dbg_sensor(1, "[%s] cal_digital = %d, dgain_code = %d(%#x)\n", __func__, cal_digital, dgain_code, dgain_code);
	dgains[0] = dgains[1] = dgains[2] = dgains[3] = dgain_code;
	ret = cis->ixc_ops->write16_array(cis->client, cis->reg_addr->dgain, dgains, 4);
	if (ret < 0)
		goto p_err_unlock;

	ret = cis->ixc_ops->read16(cis->client, SENSOR_HI847_ISP_EN_ADDR, &read_val);
	if (ret < 0)
		goto p_err_unlock;

	enable_dgain = read_val | (0x1 << 4); /* [4]: D gain enable */
	ret = cis->ixc_ops->write16(cis->client, SENSOR_HI847_ISP_EN_ADDR, enable_dgain);
	if (ret < 0)
		goto p_err_unlock;

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);
	return ret;
}

int sensor_hi847_cis_set_exposure_time(struct v4l2_subdev *subdev, struct ae_param *target_exposure)
{
	int ret = 0;
	struct is_cis *cis = sensor_cis_get_cis(subdev);
	cis_shared_data *cis_data = NULL;

	u64 pix_rate_freq_khz = 0;
	u16 coarse_int = 0;
	u32 line_length_pck = 0;
	u32 min_fine_int = 0;
	ktime_t st = ktime_get();

	if ((target_exposure->long_val <= 0) || (target_exposure->short_val <= 0)) {
		err("invalid target exposure(%d, %d)", target_exposure->long_val, target_exposure->short_val);
		ret = -EINVAL;
		goto p_err;
	}

	cis_data = cis->cis_data;
	cis_data->last_exp = *target_exposure;

	pix_rate_freq_khz = cis_data->pclk / 1000;
	line_length_pck = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	coarse_int = (u16)(((target_exposure->val * pix_rate_freq_khz) - min_fine_int) / (line_length_pck * 1000));

	if (coarse_int > cis_data->max_coarse_integration_time) {
		dbg_sensor(1, "%s [%d] VCNT[%d] coarse(%d) max(%d)\n",
			__func__, cis->id, cis_data->sen_vsync_count, coarse_int, cis_data->max_coarse_integration_time);
		coarse_int = cis_data->max_coarse_integration_time;
	}

	if (coarse_int < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "%s [%d] VCNT[%d] coarse(%d) min(%d)\n",
			__func__, cis->id, cis_data->sen_vsync_count, coarse_int, cis_data->min_coarse_integration_time);
		coarse_int = cis_data->min_coarse_integration_time;
	}

	cis_data->cur_exposure_coarse = coarse_int;
	cis_data->cur_long_exposure_coarse = coarse_int;
	cis_data->cur_short_exposure_coarse = coarse_int;

	IXC_MUTEX_LOCK(cis->ixc_lock);
	ret = cis->ixc_ops->write16(cis->client, cis->reg_addr->cit, coarse_int);
		if (ret < 0)
			goto p_err_unlock;

	dbg_sensor(1, "%s [%d] VCNT[%d] target[%d] => FLL[%#x] CIT[%#x]\n",
		__func__, cis->id, cis_data->sen_vsync_count, target_exposure->val, cis_data->frame_length_lines, coarse_int);

	if (IS_ENABLED(DEBUG_SENSOR_TIME))
		dbg_sensor(1, "[%s] time %ldus\n", __func__, PABLO_KTIME_US_DELTA_NOW(st));

p_err_unlock:
	IXC_MUTEX_UNLOCK(cis->ixc_lock);

p_err:
	return ret;
}

int sensor_hi847_cis_compensate_gain_for_extremely_br(struct v4l2_subdev *subdev, u32 expo, u32 *again, u32 *dgain)
{
	int ret = 0;
	struct is_cis *cis;
	cis_shared_data *cis_data;
	const struct sensor_cis_mode_info *mode_info;
	u32 mode;
	u64 pclk_khz = 0;
	u32 llp = 0;
	u32 min_fine_int = 0;
	u16 cit = 0;
	u32 again_input, again_comp = 0;
	u32 dgain_input, dgain_comp = 0;
	u64 expected_exp, real_cit_exp;

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

	pclk_khz = cis_data->pclk / 1000;
	llp = cis_data->line_length_pck;
	min_fine_int = cis_data->min_fine_integration_time;

	if (llp <= 0) {
		err("[%s] invalid line_length_pck(%d)\n", __func__, llp);
		goto p_err;
	}

	cit = ((expo * pclk_khz) / 1000 - min_fine_int) / llp;

	/* keep backward compatibility for legacy driver */
	if (cis->sensor_info) {
		mode = cis_data->sens_config_index_cur;
		mode_info = cis->sensor_info->mode_infos[mode];
		cit = ALIGN_DOWN(cit, mode_info->align_cit);
	}

	if (cit < cis_data->min_coarse_integration_time) {
		dbg_sensor(1, "[MOD:D:%d] %s, vsync_cnt(%d), long coarse(%d) min(%d)\n", cis->id, __func__,
			cis_data->sen_vsync_count, cit, cis_data->min_coarse_integration_time);
		cit = cis_data->min_coarse_integration_time;
	}

	if (cit <= 100) {
		again_input = *again;
		dgain_input = *dgain;

		expected_exp = (expo * pclk_khz) / 1000 - min_fine_int;
		real_cit_exp = llp * cit;

		if (again_input >= cis_data->max_analog_gain[1]) {
			again_comp = cis_data->max_analog_gain[1];
			dgain_comp = dgain_input * expected_exp / real_cit_exp;
		} else {
			again_comp = again_input * expected_exp / real_cit_exp;
			dgain_comp = dgain_input;

			if (again_comp < cis_data->min_analog_gain[1]) {
				again_comp = cis_data->min_analog_gain[1];
			} else if (again_comp >= cis_data->max_analog_gain[1]) {
				dgain_comp = dgain_input * again_comp / cis_data->max_analog_gain[1];
				again_comp = cis_data->max_analog_gain[1];
			}
		}

		*again = again_comp;
		*dgain = dgain_comp;

		dbg_sensor(1, "[%s] exp[%d] again[%d] dgain[%d] cit[%d] => compensated again[%d] dgain[%d]\n",
			__func__, expo, again_input, dgain_input, cit, again_comp, dgain_comp);
	}

p_err:
	return ret;
}

int sensor_hi847_cis_set_totalgain(struct v4l2_subdev *subdev, struct ae_param *target_exposure,
	struct ae_param *again, struct ae_param *dgain)
{
	int ret = 0;

	/* Set Exposure Time */
	ret = sensor_hi847_cis_set_exposure_time(subdev, target_exposure);

	/* Set Analog & Digital gains */
	ret |= sensor_hi847_cis_set_analog_digital_gain(subdev, again, dgain);
	return ret;
}

static struct is_cis_ops cis_ops_hi847 = {
	.cis_init = sensor_hi847_cis_init,
	.cis_log_status = sensor_hi847_cis_log_status,
	.cis_group_param_hold = sensor_cis_set_group_param_hold,
	.cis_set_global_setting = sensor_hi847_cis_set_global_setting,
	.cis_mode_change = sensor_hi847_cis_mode_change,
	.cis_stream_on = sensor_hi847_cis_stream_on,
	.cis_stream_off = sensor_hi847_cis_stream_off,
	.cis_adjust_frame_duration = sensor_cis_adjust_frame_duration,
	.cis_set_frame_duration = sensor_cis_set_frame_duration,
	.cis_set_frame_rate = sensor_cis_set_frame_rate,
	.cis_get_min_exposure_time = sensor_cis_get_min_exposure_time,
	.cis_get_max_exposure_time = sensor_cis_get_max_exposure_time,
	.cis_set_exposure_time = NULL,
	.cis_get_min_analog_gain = sensor_cis_get_min_analog_gain,
	.cis_get_max_analog_gain = sensor_cis_get_max_analog_gain,
	.cis_adjust_analog_gain = sensor_cis_adjust_analog_gain,
	.cis_set_analog_gain = NULL,
	.cis_get_analog_gain = sensor_cis_get_analog_gain,
	.cis_calc_again_code = sensor_hi847_cis_calc_again_code,
	.cis_calc_again_permile = sensor_hi847_cis_calc_again_permile,
	.cis_get_min_digital_gain = sensor_cis_get_min_digital_gain,
	.cis_get_max_digital_gain = sensor_cis_get_max_digital_gain,
	.cis_set_digital_gain = NULL,
	.cis_get_digital_gain = sensor_cis_get_digital_gain,
	.cis_calc_dgain_code = sensor_hi847_cis_calc_dgain_code,
	.cis_calc_dgain_permile = sensor_hi847_cis_calc_dgain_permile,
	.cis_set_totalgain = sensor_hi847_cis_set_totalgain,
	.cis_compensate_gain_for_extremely_br = sensor_hi847_cis_compensate_gain_for_extremely_br,
	.cis_wait_streamoff = sensor_hi847_cis_wait_streamoff,
	.cis_wait_streamon = sensor_hi847_cis_wait_streamon,
	.cis_data_calculation = sensor_cis_data_calculation,
	.cis_set_test_pattern = sensor_hi847_cis_set_test_pattern,
	.cis_get_otprom_data = sensor_hi847_cis_get_otprom_data,
	.cis_set_adjust_sync = NULL,
#ifdef USE_CAMERA_MIPI_CLOCK_VARIATION
	.cis_update_mipi_info = NULL,
#endif
	.cis_check_rev_on_init = sensor_cis_check_rev_on_init,
	.cis_set_initial_exposure = sensor_cis_set_initial_exposure,
	.cis_recover_stream_on = NULL,
};

static int cis_hi847_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct is_cis *cis = NULL;
	struct is_device_sensor_peri *sensor_peri = NULL;
	char const *setfile;
	struct device_node *dnode = client->dev.of_node;

	ret = sensor_cis_probe(client, &(client->dev), &sensor_peri, I2C_TYPE);
	if (ret) {
		probe_info("%s: sensor_cis_probe ret(%d)\n", __func__, ret);
		return ret;
	}

	cis = &sensor_peri->cis;
	cis->ctrl_delay = N_PLUS_TWO_FRAME;
	cis->cis_ops = &cis_ops_hi847;
	cis->rev_flag = false;
	/* belows are depend on sensor cis. MUST check sensor spec */
	cis->bayer_order = OTF_INPUT_ORDER_BAYER_GR_BG;

	cis->use_total_gain = true;
	cis->use_wb_gain = true;
	cis->reg_addr = &sensor_hi847_reg_addr;


	ret = of_property_read_string(dnode, "setfile", &setfile);
	if (ret) {
		err("setfile index read fail(%d), take default setfile!!", ret);
		setfile = "default";
	}

	if (strcmp(setfile, "default") == 0 || strcmp(setfile, "setA") == 0) {
		probe_info("%s setfile_A\n", __func__);
		sensor_hi847_otp_initial = sensor_hi847_otp_initial_A;
		sensor_hi847_otp_initial_size = ARRAY_SIZE(sensor_hi847_otp_initial_A);
		cis->sensor_info = &sensor_hi847_info_A;
	}
	else {
		err("%s setfile index out of bound, take default (setfile_A)", __func__);
		sensor_hi847_otp_initial = sensor_hi847_otp_initial_A;
		sensor_hi847_otp_initial_size = ARRAY_SIZE(sensor_hi847_otp_initial_A);
		cis->sensor_info = &sensor_hi847_info_A;
	}

	probe_info("%s done\n", __func__);
	return ret;
}

static const struct of_device_id sensor_cis_hi847_match[] = {
	{
		.compatible = "samsung,exynos-is-cis-hi847",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_cis_hi847_match);

static const struct i2c_device_id sensor_cis_hi847_idt[] = {
	{ SENSOR_NAME, 0 },
	{},
};

static struct i2c_driver sensor_cis_hi847_driver = {
	.probe	= cis_hi847_probe,
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sensor_cis_hi847_match,
		.suppress_bind_attrs = true,
	},
	.id_table = sensor_cis_hi847_idt
};

#ifdef MODULE
builtin_i2c_driver(sensor_cis_hi847_driver);
#else
static int __init sensor_cis_hi847_init(void)
{
	int ret;

	ret = i2c_add_driver(&sensor_cis_hi847_driver);
	if (ret)
		err("failed to add %s driver: %d\n",
			sensor_cis_hi847_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_cis_hi847_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
