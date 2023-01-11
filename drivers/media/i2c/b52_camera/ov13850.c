/* Marvell ISP OV13850 Driver
 *
 * Copyright (C) 2009-2010 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image OV5642
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

#include "ov13850.h"
static int OV13850_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
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

static int OV13850_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	OV13850_get_mipiclock(sd, &dphy_desc->clk_freq, mclk);
	dphy_desc->hs_prepare = 50;
	dphy_desc->hs_zero  = 150;

	return 0;
}

static int OV13850_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	int temp1, temp2;
	int PLL1_divmipi;
	u32 mipi_clk;
	int Pll1_divmipi_map[] = {4, 5, 6, 8};
	struct b52_sensor *sensor = to_b52_sensor(sd);
	OV13850_get_mipiclock(sd, &mipi_clk, mclk);
	b52_sensor_call(sensor, i2c_read, 0x0304, &temp1, 1);
	temp2 = temp1 & 0x03;
	PLL1_divmipi = Pll1_divmipi_map[temp2];
	*rate = mipi_clk / PLL1_divmipi * sensor->drvdata->nr_lane;
	return 0;
}
static void OV13850_write_i2c(struct b52_sensor *sensor, u16 reg, u8 val)
{
	b52_sensor_call(sensor, i2c_write, reg, val, 1);
}
static u8 OV13850_read_i2c(struct b52_sensor *sensor, u16 reg)
{
	int temp1;
	b52_sensor_call(sensor, i2c_read, reg, &temp1, 1);
	return temp1;
}
static int OV13850_OTP_access_start(struct b52_sensor *sensor)
{
	int temp;
	temp = OV13850_read_i2c(sensor, 0x5002);
	temp = temp & 0xfd;
	OV13850_write_i2c(sensor, 0x5002, temp);
	return 0;
}
static int OV13850_OTP_access_end(struct b52_sensor *sensor)
{
	int temp;
	temp = OV13850_read_i2c(sensor, 0x5002);
	temp = temp | 0x02;
	OV13850_write_i2c(sensor, 0x5002, temp);
	return 0;
}
static int check_otp_info(struct b52_sensor *sensor, int index)
{
	int flag;
	int flagaddress = OTP_DRV_START_ADDR;
	OV13850_OTP_access_start(sensor);
	OV13850_write_i2c(sensor, 0x3d84, 0xC0);
	OV13850_write_i2c(sensor, 0x3d88, (flagaddress >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d89, flagaddress & 0xff);
	OV13850_write_i2c(sensor, 0x3d8A, (flagaddress >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d8B, flagaddress & 0xff);
	OV13850_write_i2c(sensor, 0x3d81, 0x01);
	usleep_range(5000, 5010);
	flag = OV13850_read_i2c(sensor, flagaddress);
	if (index == 1)
		flag = (flag >> 6) & 0x03;
	else if (index == 2)
		flag = (flag >> 4) & 0x03;
	else
		flag = (flag >> 2) & 0x03;
	OV13850_write_i2c(sensor, flagaddress, 0x00);
	OV13850_OTP_access_end(sensor);
	if (flag == 0x00)
		return 0;
	else if (flag & 0x02)
		return 1;
	else
		return 2;
}
static int check_otp_wb(struct b52_sensor *sensor, int index)
{
	int flag;
	int flagaddress = OTP_DRV_START_ADDR + 1 + OTP_DRV_INFO_GROUP_COUNT
			* OTP_DRV_INFO_SIZE;
	OV13850_OTP_access_start(sensor);
	OV13850_write_i2c(sensor, 0x3d84, 0xC0);
	OV13850_write_i2c(sensor, 0x3d88, (flagaddress >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d89, flagaddress & 0xff);
	OV13850_write_i2c(sensor, 0x3d8A, (flagaddress >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d8B, flagaddress & 0xff);
	OV13850_write_i2c(sensor, 0x3d81, 0x01);
	usleep_range(5000, 5010);
	flag = OV13850_read_i2c(sensor, flagaddress);
	if (index == 1)
		flag = (flag>>6) & 0x03;
	else if (index == 2)
		flag = (flag>>4) & 0x03;
	else
		flag = (flag>>2) & 0x03;
	OV13850_write_i2c(sensor, flagaddress, 0x00);
	OV13850_OTP_access_end(sensor);
	if (flag == 0x00)
		return 0;
	else if (flag & 0x02)
		return 1;
	else
		return 2;
}
static int check_otp_lenc(struct b52_sensor *sensor, int index)
{
	int flag;
	int nflagaddress = OTP_DRV_START_ADDR + 1 + OTP_DRV_INFO_GROUP_COUNT
				* OTP_DRV_INFO_SIZE
				+ 1 + OTP_DRV_AWB_GROUP_COUNT * OTP_DRV_AWB_SIZE
				+ 1 + OTP_DRV_VCM_GROUP_COUNT
				* OTP_DRV_VCM_SIZE;
	OV13850_OTP_access_start(sensor);
	OV13850_write_i2c(sensor, 0x3d84, 0xc0);
	OV13850_write_i2c(sensor, 0x3d88, (nflagaddress >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d89, nflagaddress & 0xff);
	OV13850_write_i2c(sensor, 0x3d8A, (nflagaddress >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d8B, nflagaddress & 0xff);
	OV13850_write_i2c(sensor, 0x3d81, 0x01);
	usleep_range(5000, 5010);
	flag = OV13850_read_i2c(sensor, nflagaddress);
	if (index == 1)
		flag = (flag >> 6) & 0x03;
	else if (index == 2)
		flag = (flag >> 4) & 0x03;
	else
		flag = (flag >> 2) & 0x03;
	OV13850_write_i2c(sensor, nflagaddress, 0x00);
	OV13850_OTP_access_end(sensor);
	if (flag == 0x00)
		return 0;
	else if (flag & 0x02)
		return 1;
	else
		return 2;
}
static int read_otp_info(struct b52_sensor *sensor, int index,
				struct b52_sensor_otp *otp)
{
	int i;
	int nflagaddress = OTP_DRV_START_ADDR;
	int start_addr, end_addr;
	start_addr = nflagaddress + 1 + (index - 1) * OTP_DRV_INFO_SIZE;
	end_addr = start_addr + OTP_DRV_INFO_SIZE - 1;
	OV13850_OTP_access_start(sensor);
	OV13850_write_i2c(sensor, 0x3d84, 0xC0);
	OV13850_write_i2c(sensor, 0x3d88, (start_addr >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d89, start_addr & 0xff);
	OV13850_write_i2c(sensor, 0x3d8A, (end_addr >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d8B, end_addr & 0xff);
	OV13850_write_i2c(sensor, 0x3d81, 0x01);
	usleep_range(5000, 5010);
	otp->module_id = OV13850_read_i2c(sensor, start_addr);
	otp->lens_id = OV13850_read_i2c(sensor, start_addr + 1);
	for (i = start_addr; i <= end_addr; i++)
		OV13850_write_i2c(sensor, i, 0x00);
	OV13850_OTP_access_end(sensor);
	return 0;
}
static int read_otp_wb(struct b52_sensor *sensor, int index,
				struct b52_sensor_otp *otp)
{
	int i;
	int temp;
	int start_addr, end_addr;
	int nflagaddress = OTP_DRV_START_ADDR + 1 +
				OTP_DRV_INFO_GROUP_COUNT
				* OTP_DRV_INFO_SIZE;
	start_addr = nflagaddress + 1 + (index - 1)
				* OTP_DRV_AWB_SIZE;
	end_addr = start_addr + OTP_DRV_AWB_SIZE - 1;
	OV13850_OTP_access_start(sensor);
	OV13850_write_i2c(sensor, 0x3d84, 0xC0);
	OV13850_write_i2c(sensor, 0x3d88, (start_addr >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d89, start_addr & 0xff);
	OV13850_write_i2c(sensor, 0x3d8A, (end_addr >> 8) & 0xff);
	OV13850_write_i2c(sensor, 0x3d8B, end_addr & 0xff);
	OV13850_write_i2c(sensor, 0x3d81, 0x01);
	usleep_range(5000, 5010);
	temp = OV13850_read_i2c(sensor, start_addr + 4);
	otp->rg_ratio = (OV13850_read_i2c(sensor, start_addr) << 2)
					+ ((temp >> 6) & 0x03);
	otp->bg_ratio = (OV13850_read_i2c(sensor, start_addr + 1) << 2)
					+ ((temp >> 4) & 0x03);
	otp->user_data[0] = (OV13850_read_i2c(sensor, start_addr + 2) << 2)
					+ ((temp >> 2) & 0x03);
	otp->user_data[1] = (OV13850_read_i2c(sensor, start_addr + 3) << 2)
					+ (temp & 0x03);
	for (i = start_addr; i <= end_addr; i++)
		OV13850_write_i2c(sensor, i, 0x00);
	OV13850_OTP_access_end(sensor);
	return 0;
}
static int read_otp_lenc(struct b52_sensor *sensor, int index, u8 *lenc)
{
	int i;
	int start_addr, end_addr;
	int nflagaddress = OTP_DRV_START_ADDR + 1 + OTP_DRV_INFO_GROUP_COUNT
				* OTP_DRV_INFO_SIZE
				+ 1 + OTP_DRV_AWB_GROUP_COUNT
				* OTP_DRV_AWB_SIZE
				+ 1 + OTP_DRV_VCM_GROUP_COUNT
				* OTP_DRV_VCM_SIZE;
	start_addr = nflagaddress + 1 + (index - 1) * OTP_DRV_LSC_SIZE;
	end_addr = start_addr + OTP_DRV_LSC_SIZE - 1;
	OV13850_OTP_access_start(sensor);
	OV13850_write_i2c(sensor, 0x3d84, 0xC0);
	OV13850_write_i2c(sensor, 0x3d88, (start_addr >> 8));
	OV13850_write_i2c(sensor, 0x3d89, (start_addr & 0xff));
	OV13850_write_i2c(sensor, 0x3d8A, (end_addr >> 8));
	OV13850_write_i2c(sensor, 0x3d8B, (end_addr & 0xff));
	OV13850_write_i2c(sensor, 0x3d81, 0x01);
	usleep_range(10000, 10010);
	for (i = 0; i < OTP_DRV_LSC_SIZE; i++)
		lenc[i] = OV13850_read_i2c(sensor, start_addr + i);
	for (i = start_addr; i <= end_addr; i++)
		OV13850_write_i2c(sensor, i, 0x00);
	OV13850_OTP_access_end(sensor);
	return 0;
}
static int update_awb_gain(struct b52_sensor *sensor, int r_gain,
				int g_gain, int b_gain)
{
	if (r_gain >= 0x400) {
		OV13850_write_i2c(sensor, 0x5056, r_gain >> 8);
		OV13850_write_i2c(sensor, 0x5057, r_gain & 0x00ff);
	}
	if (g_gain >= 0x400) {
		OV13850_write_i2c(sensor, 0x5058, g_gain >> 8);
		OV13850_write_i2c(sensor, 0x5059, g_gain & 0x00ff);
	}
	if (b_gain >= 0x400) {
		OV13850_write_i2c(sensor, 0x505a, b_gain >> 8);
		OV13850_write_i2c(sensor, 0x505b, b_gain & 0x00ff);
	}
	return 0;
}
static int update_lenc(struct b52_sensor *sensor, u8 *lenc)
{
	int i, temp;
	temp = OV13850_read_i2c(sensor, 0x5000);
	temp = 0x01 | temp;
	OV13850_write_i2c(sensor, 0x5000, temp);
	for (i = 0; i < OTP_DRV_LSC_SIZE; i++)
		OV13850_write_i2c(sensor, OTP_DRV_LSC_REG_ADDR + i,
					lenc[i]);
	return 0;
}
static int update_otp_info(struct b52_sensor *sensor,
					struct b52_sensor_otp *otp)
{
	int i;
	int otp_index;
	int temp;
	for (i = 1; i <= OTP_DRV_INFO_GROUP_COUNT; i++) {
		temp = check_otp_info(sensor, i);
		if (temp == 2) {
			otp_index = i << 2;
			break;
		}
	}
	if (i > OTP_DRV_INFO_GROUP_COUNT)
		return -1;
	read_otp_info(sensor, otp_index, otp);
	return 0;
}

static int update_otp_wb(struct b52_sensor *sensor, struct b52_sensor_otp *otp)
{
	int i;
	int otp_index;
	int temp;
	int r_gain, g_gain, b_gain, g_gain_r, g_gain_b;
	int rg, bg;
	for (i = 1; i <= OTP_DRV_AWB_GROUP_COUNT; i++) {
		temp = check_otp_wb(sensor, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}
	if (i > OTP_DRV_AWB_GROUP_COUNT)
		return -1;
	read_otp_wb(sensor, otp_index, otp);
	if (otp->user_data[0] == 0)
		rg = otp->rg_ratio;
	else
		rg = otp->rg_ratio * (otp->user_data[0] + 512) / 1024;
	if (otp->user_data[1] == 0)
		bg = otp->bg_ratio;
	else
		bg = otp->bg_ratio * (otp->user_data[1] + 512) / 1024;
	if (bg < bg_ratio_typical) {
		if (rg < rg_ratio_typical) {
			g_gain = 0x400;
			b_gain = 0x400 * bg_ratio_typical / bg;
			r_gain = 0x400 * rg_ratio_typical / rg;
		} else {
			r_gain = 0x400;
			g_gain = 0x400 * rg / rg_ratio_typical;
			b_gain = g_gain * bg_ratio_typical / bg;
		}
	} else {
		if (rg < rg_ratio_typical) {
			b_gain = 0x400;
			g_gain = 0x400 * bg / bg_ratio_typical;
			r_gain = g_gain * rg_ratio_typical / rg;
		} else {
			g_gain_b = 0x400 * bg / bg_ratio_typical;
			g_gain_r = 0x400 * rg / rg_ratio_typical;
			if (g_gain_b > g_gain_r) {
				b_gain = 0x400;
				g_gain = g_gain_b;
				r_gain = g_gain * rg_ratio_typical / rg;
			} else {
				r_gain = 0x400;
				g_gain = g_gain_r;
				b_gain = g_gain * bg_ratio_typical / bg;
			}
		}
	}
	update_awb_gain(sensor, r_gain, g_gain, b_gain);
	return 0;
}
static int update_otp_lenc(struct b52_sensor *sensor)
{
	int i;
	int otp_index;
	int temp;
	u8 lenc[OTP_DRV_LSC_SIZE];
	for (i = 1; i <= OTP_DRV_LSC_GROUP_COUNT; i++) {
		temp = check_otp_lenc(sensor, i);
		if (temp == 2) {
			otp_index = i<<2;
			break;
		}
	}
	if (i > OTP_DRV_LSC_GROUP_COUNT)
		return -1;
	read_otp_lenc(sensor, otp_index, lenc);
	update_lenc(sensor, lenc);
	return 0;
}
static int OV13850_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *otp)
{
	int ret = 0;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (otp->otp_type ==  SENSOR_TO_SENSOR) {
		ret = update_otp_info(sensor, otp);
		if (ret < 0)
			return ret;
		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_WB) {
			ret = update_otp_wb(sensor, otp);
			if (ret < 0)
				return ret;
		}
		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_LENC) {
			ret = update_otp_lenc(sensor);
			if (ret < 0)
				return ret;
		}
	}
	return 0;
}
