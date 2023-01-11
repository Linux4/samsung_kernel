/* Marvell ISP OV5670 Driver
 *
 * Copyright (C) 2009-2014 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image OV5670
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

#include "ov5670.h"

static void OV5670_write_i2c(struct b52_sensor *sensor, u16 reg, u8 val)
{
	b52_sensor_call(sensor, i2c_write, reg, val, 1);
}

static u8 OV5670_read_i2c(struct b52_sensor *sensor, u16 reg)
{
	int temp1;
	b52_sensor_call(sensor, i2c_read, reg, &temp1, 1);
	return temp1;
}

static int OV5670_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	/*calculate sysclk*/
	int temp1, temp2;
	int pll2_prediv0, pll2_prediv2x, pll2_mult;
	int pll2_prediv2x_map[] = {2, 3, 4, 5, 6, 8, 12, 16};
	struct b52_sensor *sensor = to_b52_sensor(sd);

	temp1 = OV5670_read_i2c(sensor, 0x0312);
	temp2 = temp1 & 0x10;
	if (temp2 == 10)
		pll2_prediv0 = 2;
	else
		pll2_prediv0 = 1;

	temp1 = OV5670_read_i2c(sensor, 0x030b);
	temp2 = temp1 & 0x07;
	pll2_prediv2x = pll2_prediv2x_map[temp2];
	temp1 = OV5670_read_i2c(sensor, 0x030d);
	temp2 = OV5670_read_i2c(sensor, 0x030c);
	pll2_mult = ((temp2 & 0x03) << 8) + temp1;
	*rate = mclk * 2 / pll2_prediv0 / pll2_prediv2x * pll2_mult;

	return 0;
}

static int OV5670_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	OV5670_get_mipiclock(sd, &dphy_desc->clk_freq, mclk);
	dphy_desc->hs_prepare = 26;
	dphy_desc->hs_zero  = 125;
	return 0;
}
static int OV5670_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	/*calculate sysclk*/
	int temp1, temp2;
	int pll2_sys_prediv, pll2_sys_divider2x;
	int sys_prediv, sclk_pdiv;
	u32 mipi_clk;
	int pll2_sys_divider2x_map[] = {2, 3, 4, 5, 6, 7, 8, 10};
	int sys_prediv_map[] = {1, 2, 4, 1};
	struct b52_sensor *sensor = to_b52_sensor(sd);

	OV5670_get_mipiclock(sd, &mipi_clk, mclk);
	temp1 = OV5670_read_i2c(sensor, 0x030f);
	temp2 = temp1 & 0x0f;
	pll2_sys_prediv = temp2 + 1;

	temp1 = OV5670_read_i2c(sensor, 0x030e);
	temp2 = temp1 & 0x07;
	pll2_sys_divider2x = pll2_sys_divider2x_map[temp2];
	temp1 = OV5670_read_i2c(sensor, 0x3106);
	temp2 = (temp1 >> 2) & 0x03;
	sys_prediv = sys_prediv_map[temp2];
	temp2 = (temp1 >> 4) & 0x0f;
	if (temp2 > 0)
		sclk_pdiv = temp2;
	else
		sclk_pdiv = 1;

	*rate = mipi_clk * 2 / pll2_sys_prediv / pll2_sys_divider2x
			/ sys_prediv / sclk_pdiv;
	*rate = *rate * 2;
	return 0;
}

static int OV5670_read_OTP(struct b52_sensor *sensor,
			struct b52_sensor_otp *otp, u32 *flag)
{
	int otp_flag, otp_base, temp, i;
	/*read OTP into buffer*/
	OV5670_write_i2c(sensor, 0x3d84, 0xC0);
	OV5670_write_i2c(sensor, 0x3d88, 0x70); /*OTP start address*/
	OV5670_write_i2c(sensor, 0x3d89, 0x10);
	OV5670_write_i2c(sensor, 0x3d8A, 0x70); /*OTP end address*/
	OV5670_write_i2c(sensor, 0x3d8B, 0x29);
	OV5670_write_i2c(sensor, 0x3d81, 0x01); /*load otp into buffer*/
	usleep_range(5000, 5010);

	/*OTP into*/
	otp_flag = OV5670_read_i2c(sensor, 0x7010);
	otp_base = 0;
	if ((otp_flag & 0xc0) == 0x40)
		otp_base = 0x7011;/*base address of info group 1*/
	else if ((otp_flag & 0x30) == 0x10)
		otp_base = 0x7016;/*base address of info group 2*/
	else if ((otp_flag & 0x0c) == 0x04)
		otp_base = 0x701b;/*base address of info group 3*/

	if (otp_base != 0) {
		*flag = 0x80; /*valid info in OTP*/
		(*otp).module_id = OV5670_read_i2c(sensor, otp_base);
		(*otp).lens_id = OV5670_read_i2c(sensor, otp_base + 1);
	} else {
		*flag = 0x00; /*not info in OTP*/
		(*otp).module_id = 0;
		(*otp).lens_id = 0;
	}
	pr_err("Marvell_Unifiled_OTP_ID_INF for OV5670: Module=0x%x, Lens=0x%x\n",
		(*otp).module_id, (*otp).lens_id);
	/*OTP WB Calibration*/
	otp_flag = OV5670_read_i2c(sensor, 0x7020);
	otp_base = 0;
	if ((otp_flag & 0xc0) == 0x40)
		otp_base = 0x7021; /*base address of WB Calibration group 1*/
	else if ((otp_flag & 0x30) == 0x10)
		otp_base = 0x7024; /*base address of WB Calibration group 2*/
	else if ((otp_flag & 0x0c) == 0x04)
		otp_base = 0x7027; /*base address of WB Calibration group 3*/

	if (otp_base != 0) {
		*flag |= 0x40;
		temp = OV5670_read_i2c(sensor, otp_base + 2);
		(*otp).rg_ratio = (OV5670_read_i2c(sensor, otp_base)<<2)
					 + ((temp>>6) & 0x03);
		(*otp).bg_ratio = (OV5670_read_i2c(sensor, otp_base + 1)<<2)
					 + ((temp>>4) & 0x03);
	} else {
		(*otp).rg_ratio = 0;
		(*otp).bg_ratio = 0;
	}
	pr_err("Marvell_Unifiled_OTP_ID_INF for OV5670: rg_ratio=0x%x, bg_ratio=0x%x\n",
		(*otp).rg_ratio, (*otp).bg_ratio);
	/*Post-processing*/
	if ((*flag) != 0) {
		for (i = 0x7010; i <= 0x7029; i++) {
			OV5670_write_i2c(sensor, i, 0);
			/*clear OTP buffer,recommended use
				continuous write to accelarate*/
		}
	}
	return *flag;
}

static int OV5670_update_wb(struct b52_sensor *sensor,
			struct b52_sensor_otp *otp, u32 *flag)
{
	int rg, bg, r_gain, g_gain, b_gain, base_gain;
	/*apply OTP WB Calibration*/
	if ((*flag) & 0x40) {
		rg = (*otp).rg_ratio;
		bg = (*otp).bg_ratio;
		/*calculate sensor WB gain, 0x400 = 1x gain*/
		r_gain = 0x400 * rg_ratio_typical / rg;
		g_gain = 0x400;
		b_gain = 0x400 * bg_ratio_typical / bg;
		/*find gain<0x400*/
		if (r_gain < 0x400 || b_gain < 0x400) {
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
			OV5670_write_i2c(sensor, 0x5032, r_gain >> 8);
			OV5670_write_i2c(sensor, 0x5033, r_gain & 0x00ff);
		}
		if (g_gain > 0x400) {
			OV5670_write_i2c(sensor, 0x5034, g_gain >> 8);
			OV5670_write_i2c(sensor, 0x5035, g_gain & 0x00ff);
		}
		if (b_gain > 0x400) {
			OV5670_write_i2c(sensor, 0x5036, b_gain >> 8);
			OV5670_write_i2c(sensor, 0x5037, b_gain & 0x00ff);
		}
	}
	return *flag;
}

static void OV5670_otp_access_start(struct b52_sensor *sensor)
{
	int temp;
	temp = OV5670_read_i2c(sensor, 0x5002);
	OV5670_write_i2c(sensor, 0x5002, temp&(~0x08));
	usleep_range(10000, 10010);
	OV5670_write_i2c(sensor, 0x0100, 0x01);
}

static void OV5670_otp_access_end(struct b52_sensor *sensor)
{
	int temp;
	temp = OV5670_read_i2c(sensor, 0x5002);
	OV5670_write_i2c(sensor, 0x5002, temp|(0x08));
	usleep_range(10000, 10010);
	OV5670_write_i2c(sensor, 0x0100, 0x00);
}

static int OV5670_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *otp)
{
	int ret = 0;
	int flag = 0;

	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (otp->otp_type ==  SENSOR_TO_SENSOR) {
		/*access otp data start*/
		OV5670_otp_access_start(sensor);

		/*read otp data firstly*/
		ret = OV5670_read_OTP(sensor, otp, &flag);

		/*access otp data end*/
		OV5670_otp_access_end(sensor);

		if (ret < 0)
			goto fail;
		/*apply some otp data, include awb*/
		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_WB) {
			ret = OV5670_update_wb(sensor, otp, &flag);
			if (ret < 0)
				goto fail;
		}
		return 0;
	}
	return -1;
fail:
	pr_err("otp update fail\n");
	return ret;
}
