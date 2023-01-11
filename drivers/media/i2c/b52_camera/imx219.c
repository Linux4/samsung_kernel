/* Marvell ISP IMX219 Driver
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

#include "imx219.h"

static int IMX219_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	int Pll1_predivp = 1;
	int Pll1_mult = 0;
	int temp1 = 0, temp2 = 0;

	struct b52_sensor *sensor = to_b52_sensor(sd);
	b52_sensor_call(sensor, i2c_read, 0x0304, &Pll1_predivp, 1);
	b52_sensor_call(sensor, i2c_read, 0x0306, &temp1, 1);
	b52_sensor_call(sensor, i2c_read, 0x0307, &temp2, 1);
	Pll1_mult = (temp1<<8) + temp2;
	*rate = mclk * Pll1_mult / Pll1_predivp;
	return 0;
}

static int IMX219_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	IMX219_get_mipiclock(sd, &dphy_desc->clk_freq, mclk);
	dphy_desc->hs_prepare = 71;
	dphy_desc->hs_zero  = 100;

	return 0;
}

static int IMX219_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	int temp1 = 0, temp2;
	u32 mipi_clk;
	int lans[] = {0, 2, 0, 4};
	struct b52_sensor *sensor = to_b52_sensor(sd);
	IMX219_get_mipiclock(sd, &mipi_clk, mclk);
	b52_sensor_call(sensor, i2c_read, 0x0114, &temp1, 1);
	temp2 = lans[temp1];
	*rate = mipi_clk * temp2 / 10;
	return 0;
}
static void IMX219_write_i2c(struct b52_sensor *sensor, u16 reg, u8 val)
{
	b52_sensor_call(sensor, i2c_write, reg, val, 1);
}
static u8 IMX219_read_i2c(struct b52_sensor *sensor, u16 reg)
{
	int temp1 = 0;
	b52_sensor_call(sensor, i2c_read, reg, &temp1, 1);
	return temp1;
}
#if 0
static int check_otp_af(struct b52_sensor *sensor)
{
	int flag;
	int index;
	int i, group_id;
	int temp = 0, temp1 = 0, temp2 = 0;
	int startaddress;
	int indexaddress = OTP_DRV_INDEX_ADDR;
	IMX219_write_i2c(sensor, indexaddress, 0x0);
	index = IMX219_read_i2c(sensor, indexaddress);
	if (index != 0)
		return -1;

	flag = IMX219_read_i2c(sensor, 0x321D);
	if ((flag & OTP_DRV_GROUP2_FLAG) == OTP_DRV_GROUP_FLAG2_VAL) {
		startaddress = 0x3225;
		group_id = 2;
	} else if ((flag & OTP_DRV_GROUP1_FLAG)
			== OTP_DRV_GROUP_FLAG1_VAL) {
		startaddress = 0x321E;
		group_id = 1;
	} else
		return -1;

	for (i = 0; i < 6; i++) {
		temp = IMX219_read_i2c(sensor, startaddress + i);
		temp1 += temp;
	}
	temp2 = IMX219_read_i2c(sensor, startaddress + 6);
	temp1 = temp1 % 255 + 1;
	if (temp1 == temp2)
		return group_id;
	else
		return -1;
}
#endif
static int check_otp_info(struct b52_sensor *sensor)
{
	int flag;
	int index;
	int i, group_id;
	int temp = 0, temp1 = 0, temp2 = 0;
	int startaddress;
	int indexaddress = OTP_DRV_INDEX_ADDR;
	IMX219_write_i2c(sensor, indexaddress, 0x0);
	index = IMX219_read_i2c(sensor, indexaddress);
	if (index != 0x0)
		return -1;

	flag = IMX219_read_i2c(sensor, 0x3204);
	if ((flag & OTP_DRV_GROUP2_FLAG) == OTP_DRV_GROUP_FLAG2_VAL) {
		startaddress = 0x3211;
		group_id = 2;
	} else if ((flag & OTP_DRV_GROUP1_FLAG)
			== OTP_DRV_GROUP_FLAG1_VAL) {
		startaddress = 0x3205;
		group_id = 1;
	} else
		return -1;

	for (i = 0; i < 11; i++) {
		temp = IMX219_read_i2c(sensor, startaddress + i);
		temp1 += temp;
	}
	temp2 = IMX219_read_i2c(sensor, startaddress + 11);
	temp1 = temp1 % 255 + 1;
	if (temp1 == temp2)
		return group_id;
	else
		return -1;
}
static int check_otp_wb(struct b52_sensor *sensor, int *awb)
{
	int flag;
	int index;
	int i, group_id;
	int temp1 = 0, temp2 = 0;
	int startaddress;
	int indexaddress = OTP_DRV_INDEX_ADDR;
	IMX219_write_i2c(sensor, indexaddress, 0x1);
	index = IMX219_read_i2c(sensor, indexaddress);
	if (index != 0x1)
		return -1;

	flag = IMX219_read_i2c(sensor, 0x3204);
	if ((flag & OTP_DRV_GROUP2_FLAG) == OTP_DRV_GROUP_FLAG2_VAL) {
		startaddress = 0x321A;
		group_id = 2;
	} else if ((flag & OTP_DRV_GROUP1_FLAG)
			== OTP_DRV_GROUP_FLAG1_VAL) {
		startaddress = 0x3205;
		group_id = 1;
	} else
		return -1;

	for (i = 0; i < 20; i++) {
		awb[i] = IMX219_read_i2c(sensor, startaddress + i);
		temp1 += awb[i];
	}
	temp2 = IMX219_read_i2c(sensor, startaddress + 20);
	temp1 = temp1 % 255 + 1;
	if (temp1 == temp2)
		return group_id;
	else
		return -1;
}
static int check_otp_lenc(struct b52_sensor *sensor, int *lenc)
{
	int flag;
	int index;
	int i, group_id;
	int temp1 = 0, temp2 = 0;
	int startaddress;
	int indexaddress = OTP_DRV_INDEX_ADDR;
	IMX219_write_i2c(sensor, indexaddress, 0x0);
	index = IMX219_read_i2c(sensor, indexaddress);
	if (index != 0x0)
		return -1;

	flag = IMX219_read_i2c(sensor, 0x3243);
	if ((flag & OTP_DRV_GROUP2_FLAG) == OTP_DRV_GROUP_FLAG2_VAL) {
		group_id = 2;
		temp2 = IMX219_read_i2c(sensor, 0x3242);
	} else if ((flag & OTP_DRV_GROUP1_FLAG)
			== OTP_DRV_GROUP_FLAG1_VAL) {
		group_id = 1;
		temp2 = IMX219_read_i2c(sensor, 0x3241);
	} else
		return -1;

	startaddress = 0x3204;
	if (group_id == 1) {
		IMX219_write_i2c(sensor, indexaddress, 2);
		index = IMX219_read_i2c(sensor, indexaddress);
		if (index != 2)
			return -1;
		for (i = 0; i < 64; i++) {
			lenc[i] = IMX219_read_i2c(sensor, startaddress + i);
			temp1 += lenc[i];
		}
		IMX219_write_i2c(sensor, indexaddress, 3);
		index = IMX219_read_i2c(sensor, indexaddress);
		if (index != 3)
			return -1;
		for (i = 0; i < 64; i++) {
			lenc[i + 64] =
				IMX219_read_i2c(sensor, startaddress + i);
			temp1 += lenc[i + 64];
		}
		IMX219_write_i2c(sensor, indexaddress, 4);
		index = IMX219_read_i2c(sensor, indexaddress);
		if (index != 4)
			return -1;
		for (i = 0; i < 47; i++) {
			lenc[i + 128] =
				IMX219_read_i2c(sensor, startaddress + i);
			temp1 += lenc[i + 128];
		}

	} else if (group_id ==  2) {
		IMX219_write_i2c(sensor, indexaddress, 7);
		index = IMX219_read_i2c(sensor, indexaddress);
		if (index != 7)
			return -1;
		for (i = 30; i < 64; i++) {
			lenc[i - 30] =
				IMX219_read_i2c(sensor, startaddress + i);
			temp1 += lenc[i - 30];
		}
		IMX219_write_i2c(sensor, indexaddress, 8);
		index = IMX219_read_i2c(sensor, indexaddress);
		if (index != 8)
			return -1;
		for (i = 0; i < 64; i++) {
			lenc[i + 34] =
				IMX219_read_i2c(sensor, startaddress + i);
			temp1 += lenc[i + 64];
		}
		IMX219_write_i2c(sensor, indexaddress, 9);
		index = IMX219_read_i2c(sensor, indexaddress);
		if (index != 9)
			return -1;
		for (i = 0; i < 64; i++) {
			lenc[i + 98] =
				IMX219_read_i2c(sensor, startaddress + i);
			temp1 += lenc[i + 128];
		}
		IMX219_write_i2c(sensor, indexaddress, 10);
		index = IMX219_read_i2c(sensor, indexaddress);
		if (index != 10)
			return -1;
		for (i = 0; i < 13; i++) {
			lenc[i + 162] =
				IMX219_read_i2c(sensor, startaddress + i);
			temp1 += lenc[i + 162];
		}
	}

	temp1 = temp1 % 255 + 1;
	if (temp1 == temp2)
		return group_id;
	else
		return -1;
	return 0;
}
#if 0
static int read_otp_af(struct b52_sensor *sensor, int index,
				struct b52_sensor_otp *otp)
{
	int start_addr;
	int temp1, temp2;

	if (index == 2)
		start_addr = 0x3225;
	else if (index == 1)
		start_addr = 0x321E;
	else
		return -1;

	otp->af_cal_dir = IMX219_read_i2c(sensor, start_addr);
	temp1 = IMX219_read_i2c(sensor, start_addr + 1);
	temp2 = IMX219_read_i2c(sensor, start_addr + 2);
	otp->af_inf_cur = ((temp1 & 0xFF) << 2) +
				((temp2 & 0xC0) >> 6);
	temp1 = IMX219_read_i2c(sensor, start_addr + 3);
	temp2 = IMX219_read_i2c(sensor, start_addr + 4);
	otp->af_mac_cur = ((temp1 & 0xFF) << 2) +
				((temp2 & 0xC0) >> 6);
	return 0;
}
#endif
static int read_otp_info(struct b52_sensor *sensor, int index,
				struct b52_sensor_otp *otp)
{
	int start_addr;

	if (index == 2)
		start_addr = 0x3211;
	else if (index == 1)
		start_addr = 0x3205;
	else
		return -1;

	otp->module_id = IMX219_read_i2c(sensor, start_addr);
	otp->lens_id = IMX219_read_i2c(sensor, start_addr + 5);
	return 0;
}
static int read_otp_wb(struct b52_sensor *sensor, int index,
				struct b52_sensor_otp *otp, u32 *awb)
{
	int start_addr;

	/* FIXME just for dubug check */
	if (index == 2)
		start_addr = 0x321A;
	else if (index == 1)
		start_addr = 0x3205;
	else
		return -1;

	otp->rg_ratio =
		((awb[0] & 0xFF) << 2) + ((awb[1] & 0xC0) >> 6);
	otp->bg_ratio =
		((awb[2] & 0xFF) << 2) + ((awb[3] & 0xC0) >> 6);
	otp->gg_ratio =
		((awb[4] & 0xFF) << 2) + ((awb[5] & 0xC0) >> 6);
	otp->golden_rg_ratio =
		((awb[6] & 0xFF) << 2) + ((awb[7] & 0xC0) >> 6);
	otp->golden_bg_ratio =
		((awb[8] & 0xFF) << 2) + ((awb[9] & 0xC0) >> 6);
	otp->golden_gg_ratio =
		((awb[10] & 0xFF) << 2) + ((awb[11] & 0xC0) >> 6);
	return 0;
}
static int read_otp_lenc(struct b52_sensor *sensor, int index, int *lenc)
{
#if 0
	int i;
	int start_addr;

	if (index == 1)
		start_addr = 0xd200;
	else if (index == 2)
		start_addr = 0xd440;

	for (i = 0; i < OTP_DRV_LSC_SIZE; i++)
		IMX219_write_i2c(sensor, start_addr + i, lenc[i]);
#endif
	return 0;
}
#if 0
static int update_awb_gain(struct b52_sensor *sensor, int r_gain,
				int g_gain, int b_gain)
{
	return 0;
}
#endif
static int update_lenc(struct b52_sensor *sensor, int index)
{
	IMX219_write_i2c(sensor, 0x0190, 0x01);
	/* this module is auto load shading mode*/
	if (index == 1)
		IMX219_write_i2c(sensor, 0x0192, 0x0);
	else if (index == 2)
		IMX219_write_i2c(sensor, 0x0192, 0x2);
	IMX219_write_i2c(sensor, 0x0191, 0x00);
	IMX219_write_i2c(sensor, 0x0193, 0x00);
	IMX219_write_i2c(sensor, 0x01A4, 0x03);

	return 0;
}
#if 0
static int update_otp_af(struct b52_sensor *sensor,
					struct b52_sensor_otp *otp)
{
	int ret;
	int otp_index;

	otp_index = check_otp_af(sensor);
	if (otp_index < 0)
		return -1;
	ret = read_otp_af(sensor, otp_index, otp);
	if (ret < 0)
		return -1;
	return 0;
}
#endif
static int update_otp_info(struct b52_sensor *sensor,
					struct b52_sensor_otp *otp)
{
	int ret;
	int otp_index;

	otp_index = check_otp_info(sensor);
	if (otp_index < 0)
		return -1;
	ret = read_otp_info(sensor, otp_index, otp);
	if (ret < 0)
		return -1;
	return 0;
}

static int update_otp_wb(struct b52_sensor *sensor, struct b52_sensor_otp *otp)
{
	int ret;
	int otp_index;
	int awb[20];

	otp_index = check_otp_wb(sensor, awb);
	if (otp_index < 0)
		return -1;
	ret = read_otp_wb(sensor, otp_index, otp, awb);
	if (ret < 0)
		return -1;
#if 0
	update_awb_gain(sensor, r_gain, g_gain, b_gain);
#endif
	return 0;
}
static int update_otp_lenc(struct b52_sensor *sensor)
{
	int ret;
	int otp_index;
	int lenc[OTP_DRV_LSC_SIZE];

	otp_index = check_otp_lenc(sensor, lenc);
	if (otp_index < 0)
		return -1;
	ret = read_otp_lenc(sensor, otp_index, lenc);
	if (ret < 0)
		return -1;
	update_lenc(sensor, otp_index);
	return 0;
}
static int update_sensor_states(struct b52_sensor *sensor)
{
	int ret = 0;
	IMX219_write_i2c(sensor, 0x0100, 0x00);
	IMX219_write_i2c(sensor, 0x3302, 0x02);
	IMX219_write_i2c(sensor, 0x3303, 0x8A);
	IMX219_write_i2c(sensor, 0x012A, 0x1A);
	IMX219_write_i2c(sensor, 0x012b, 0x00);

	IMX219_write_i2c(sensor, 0x3300, 0x08);

	IMX219_write_i2c(sensor, 0x3200, 0x01);
	while (1) {
		ret = IMX219_read_i2c(sensor, 0x3201);
		if ((ret & 0x01) == 1)
			break;
	}
	return 0;
}
static int IMX219_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *otp)
{
	int ret = 0;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (otp->otp_type ==  SENSOR_TO_SENSOR) {
		ret = update_sensor_states(sensor);
		ret = update_otp_info(sensor, otp);
		if (ret < 0)
			return ret;

		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_WB) {
			ret = update_otp_wb(sensor, otp);
			if (ret < 0)
				return ret;
		}
#if 0
		ret = update_otp_af(sensor, otp);
		if (ret < 0)
			return ret;
#endif
		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_LENC) {
			ret = update_otp_lenc(sensor);
			if (ret < 0)
				return ret;
		}
		while (1) {
			ret = IMX219_read_i2c(sensor, 0x3201);
			if ((ret & 0x01) == 1)
				break;
		}
		IMX219_write_i2c(sensor, 0x0100, 0x01);
	}
	return 0;
}
