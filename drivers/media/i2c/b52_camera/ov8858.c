/* Marvell ISP OV8858R2A Driver
 *
 * Copyright (C) 2009-2014 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image OV8858R2A
 *
 * Copyright (c) 2009 Mauro Carvalho Chehab (mchehab@redhat.com)
 * This code is placed under the terms of the GNU General Public License v2
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>

#include <asm/div64.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ctrls.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <uapi/media/b52_api.h>

#include "ov8858.h"

static int OV8858R2A_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	int temp1, temp2;
	int Pll1_predivp, Pll1_prediv2x, Pll1_mult, Pll1_divm;
	int Pll1_predivp_map[] = {1, 2};
	int Pll1_prediv2x_map[] = {2, 3, 4, 5, 6, 8, 12, 16};

	struct b52_sensor *sensor = to_b52_sensor(sd);
	b52_sensor_call(sensor, i2c_read, 0x030a, &temp1, 1);
	temp2 = (temp1) & 0x01;
	Pll1_predivp = Pll1_predivp_map[temp2];
	b52_sensor_call(sensor, i2c_read, 0x0300, &temp1, 1);
	temp2 = temp1 & 0x07;
	Pll1_prediv2x = Pll1_prediv2x_map[temp2];
	b52_sensor_call(sensor, i2c_read, 0x0301, &temp1, 1);
	temp2 = temp1 & 0x03;
	b52_sensor_call(sensor, i2c_read, 0x0302, &temp1, 1);
	Pll1_mult = (temp2<<8) + temp1;
	b52_sensor_call(sensor, i2c_read, 0x0303, &temp1, 1);
	temp2 = temp1 & 0x0f;
	Pll1_divm = temp2 + 1;
	*rate = mclk / Pll1_predivp * 2 / Pll1_prediv2x * Pll1_mult / Pll1_divm;

	return 0;
}

static int OV8858R2A_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	OV8858R2A_get_mipiclock(sd, &dphy_desc->clk_freq, mclk);
	dphy_desc->hs_prepare = 71;
	dphy_desc->hs_zero = 100;

	return 0;
}

static int OV8858R2A_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	int temp1, temp2;
	int pll2_prediv0, pll2_prediv2x, pll2_mult, sys_pre_div, sys_div;
	int pll2_prediv2x_map[] = {2, 3, 4, 5, 6, 8, 12, 16};
	int sys_div_map[] = {2, 3, 4, 5, 6, 7, 8, 10};
	struct b52_sensor *sensor = to_b52_sensor(sd);

	b52_sensor_call(sensor, i2c_read, 0x0312, &temp1, 1);
	temp2 = temp1 & 0x10;
	if (temp2 == 0x10)
		pll2_prediv0 = 2;
	else
		pll2_prediv0 = 1;

	b52_sensor_call(sensor, i2c_read, 0x030b, &temp1, 1);
	temp2 = temp1 & 0x07;
	pll2_prediv2x = pll2_prediv2x_map[temp2];
	b52_sensor_call(sensor, i2c_read, 0x030d, &temp1, 1);
	b52_sensor_call(sensor, i2c_read, 0x030c, &temp2, 1);
	pll2_mult = ((temp2 & 0x03) << 8) + temp1;
	b52_sensor_call(sensor, i2c_read, 0x030f, &temp1, 1);
	sys_pre_div = 1 + (temp1 & 0xF);
	b52_sensor_call(sensor, i2c_read, 0x030e, &temp1, 1);
	sys_div = sys_div_map[temp1 & 0x7];
	*rate = mclk * 2 / pll2_prediv0 / pll2_prediv2x * pll2_mult / sys_pre_div * 2 / sys_div;
	*rate = *rate * 2;

	return 0;
}

static void OV8858R2A_write_i2c(struct b52_sensor *sensor, u16 reg, u8 val)
{
	b52_sensor_call(sensor, i2c_write, reg, val, 1);
}
static u8 OV8858R2A_read_i2c(struct b52_sensor *sensor, u16 reg)
{
	int temp1;
	b52_sensor_call(sensor, i2c_read, reg, &temp1, 1);
	return temp1;
}
static int OV8858R2A_read_OTP(struct b52_sensor *sensor,
			struct b52_sensor_otp *otp, u32 *flag, u8 *lenc)
{
	int otp_flag, otp_base, temp, i, checksum, checksum2;
	/*read OTP into buffer*/
	OV8858R2A_write_i2c(sensor, 0x3d84, 0xC0);
	OV8858R2A_write_i2c(sensor, 0x3d88, 0x70); /*OTP start address*/
	OV8858R2A_write_i2c(sensor, 0x3d89, 0x10);
	OV8858R2A_write_i2c(sensor, 0x3d8A, 0x72); /*OTP end address*/
	OV8858R2A_write_i2c(sensor, 0x3d8B, 0x0a);
	OV8858R2A_write_i2c(sensor, 0x3d81, 0x01); /*load otp into buffer*/
	usleep_range(5000, 5010);

	/*OTP into*/
	otp_flag = OV8858R2A_read_i2c(sensor, 0x7010);
	otp_base = 0;
	if ((otp_flag & 0xc0) == 0x40)
		otp_base = 0x7011;/*base address of info group 1*/
	else if ((otp_flag & 0x30) == 0x10)
		otp_base = 0x7019;/*base address of info group 2*/

	if (otp_base != 0) {
		*flag = 0xC0; /*valid info in OTP*/
		(*otp).module_id = OV8858R2A_read_i2c(sensor, otp_base);
		(*otp).lens_id = OV8858R2A_read_i2c(sensor, otp_base + 1);
		temp = OV8858R2A_read_i2c(sensor, otp_base + 7);
		(*otp).rg_ratio = (OV8858R2A_read_i2c(sensor, otp_base + 5) << 2) + ((temp >> 6) & 0x03);
		(*otp).bg_ratio = (OV8858R2A_read_i2c(sensor, otp_base + 6) << 2) + ((temp >> 4) & 0x03);
	} else {
		*flag = 0x00; /*not info in OTP*/
		(*otp).module_id = 0;
		(*otp).lens_id = 0;
		(*otp).rg_ratio = 0;
		(*otp).bg_ratio = 0;
	}
	pr_err("Marvell_Unifiled_OTP_ID_INF for OV8858R2A: Module=0x%x, Lens=0x%x, rg_ratio=0x%x, bg_ratio=0x%x\n",
			(*otp).module_id, (*otp).lens_id, (*otp).rg_ratio, (*otp).bg_ratio);

#if 0
	/*OTP VCM Calibration*/
	otp_flag = OV8858R2A_read_i2c(sensor, 0x7021);
	otp_base = 0;
	if ((otp_flag & 0xc0) == 0x40)
		otp_base = 0x7022; /*base address of VCM Calibration group 1*/
	else if ((otp_flag & 0x30) == 0x10)
		otp_base = 0x7025; /*base address of VCM Calibration group 2*/

	if (otp_base != 0) {
		*flag |= 0x20;
		temp = OV8858R2A_read_i2c(sensor, otp_base + 2);
		(*otp).vcm_start = (OV8858R2A_read_i2c(sensor, otp_base) << 2)
					 | ((temp>>6) & 0x03);
		(*otp).vcm_end = (OV8858R2A_read_i2c(sensor, otp_base + 1) << 2)
					 | ((temp>>4) & 0x03);
		(*otp).vcm_dir = (temp >> 2) & 0x03;
	} else {
		(*otp).vcm_start = 0;
		(*otp).vcm_end = 0;
		(*otp).vcm_dir = 0;
	}
#endif
	/*OTP Lenc Calibration*/
	otp_flag = OV8858R2A_read_i2c(sensor, 0x7028);
	otp_base = 0;
	checksum = 0;
	checksum2 = 0;
	if ((otp_flag & 0xc0) == 0x40)
		otp_base = 0x7029; /*base address of Lenc Calibration group 1*/
	else if ((otp_flag & 0x30) == 0x10)
		otp_base = 0x711a; /*base address of Lenc Calibration group 2*/

	if (otp_base != 0) {
		for (i = 0; i < 240; i++) {
			lenc[i] = OV8858R2A_read_i2c(sensor, otp_base + i);
			checksum2 += lenc[i];
		}
		checksum2 = (checksum2)%255 + 1;
		checksum = OV8858R2A_read_i2c(sensor, otp_base + 240);
		if (checksum == checksum2)
			*flag |= 0x10;
	} else
		for (i = 0; i < 240; i++)
			lenc[i] = 0;

	/* Clear OTP data */
	for (i = 0x7010; i <= 0x720a; i++)
		OV8858R2A_write_i2c(sensor, i, 0x0);

	return *flag;
}

static int OV8858R2A_update_wb(struct b52_sensor *sensor,
			struct b52_sensor_otp *otp, u32 *flag)
{
	int rg, bg, r_gain, g_gain, b_gain, base_gain;
	/*apply OTP WB Calibration*/
	if ((*flag) & 0x40) {
		rg = (*otp).rg_ratio;
		bg = (*otp).bg_ratio;
		/*calculate sensor WB gain, 0x400 = 1x gain*/
		r_gain = 1000 * rg_ratio_typical / rg;
		g_gain = 1000;
		b_gain = 1000 * bg_ratio_typical / bg;
		/*find gain<1000*/
		if (r_gain < 1000 || b_gain < 1000) {
			if (r_gain < b_gain)
				base_gain = r_gain;
			else
				base_gain = b_gain;
		} else
			base_gain = g_gain;
		/*set min gain to 0x400*/
		r_gain = 0x400 * r_gain / base_gain;
		g_gain = 0x400 * g_gain / base_gain;
		b_gain = 0x400 * b_gain / base_gain;
		/*update sensor WB gain*/
		if (r_gain > 0x400) {
			OV8858R2A_write_i2c(sensor, 0x5032, r_gain >> 8);
			OV8858R2A_write_i2c(sensor, 0x5033, r_gain & 0x00ff);
		}
		if (g_gain > 0x400) {
			OV8858R2A_write_i2c(sensor, 0x5034, g_gain >> 8);
			OV8858R2A_write_i2c(sensor, 0x5035, g_gain & 0x00ff);
		}
		if (b_gain > 0x400) {
			OV8858R2A_write_i2c(sensor, 0x5036, b_gain >> 8);
			OV8858R2A_write_i2c(sensor, 0x5037, b_gain & 0x00ff);
		}
	}
	return *flag;
}
static int OV8858R2A_update_lenc(struct b52_sensor *sensor,
			struct b52_sensor_otp *otp, u32 *flag, u8 *lenc)
{
	int temp, i;
	if (*flag & 0x10) {
		temp = OV8858R2A_read_i2c(sensor, 0x5000);
		temp = 0x80 | temp;
		OV8858R2A_write_i2c(sensor, 0x5000, temp);
		for (i = 0; i < 240; i++)
			OV8858R2A_write_i2c(sensor, 0x5800 + i, lenc[i]);
	}
	return *flag;
}

static void OV8858R2A_otp_access_start(struct b52_sensor *sensor)
{
	int temp;
	temp = OV8858R2A_read_i2c(sensor, 0x5002);
	OV8858R2A_write_i2c(sensor, 0x5002, temp&(~0x08));
	usleep_range(10000, 10010);
	OV8858R2A_write_i2c(sensor, 0x0100, 0x01);
}

static void OV8858R2A_otp_access_end(struct b52_sensor *sensor)
{
	int temp;
	temp = OV8858R2A_read_i2c(sensor, 0x5002);
	OV8858R2A_write_i2c(sensor, 0x5002, temp|(0x08));
	usleep_range(10000, 10010);
	OV8858R2A_write_i2c(sensor, 0x0100, 0x00);
}

static int OV8858R2A_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *otp)
{
	int ret = 0;
	int flag = 0;
	u8 lenc[240];
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (otp->otp_type ==  SENSOR_TO_SENSOR) {
		/*access otp data start*/
		OV8858R2A_otp_access_start(sensor);

		/*read otp data firstly*/
		ret = OV8858R2A_read_OTP(sensor, otp, &flag, lenc);

		/*access otp data end*/
		OV8858R2A_otp_access_end(sensor);

		if (ret < 0)
			goto fail;
		/*apply some otp data, include awb and lenc*/
		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_WB) {
			ret = OV8858R2A_update_wb(sensor, otp, &flag);
			if (ret < 0)
				goto fail;
		}
		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_LENC) {
			ret = OV8858R2A_update_lenc(sensor, otp, &flag, lenc);
			if (ret < 0)
				goto fail;
		}
		return 0;
	} else
		return -1;
fail:
	pr_err("otp update fail\n");
	return ret;
}
