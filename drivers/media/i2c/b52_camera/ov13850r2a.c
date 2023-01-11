/* Marvell ISP OV13850R2A Driver
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

#include "ov13850r2a.h"
static int OV13850R2A_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	int temp1 = 0, temp2 = 0;
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

static int OV13850R2A_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	OV13850R2A_get_mipiclock(sd, &dphy_desc->clk_freq, mclk);
	dphy_desc->hs_prepare = 50;
	dphy_desc->hs_zero  = 150;

	return 0;
}

static int OV13850R2A_get_pixelclock(struct v4l2_subdev *sd,
						u32 *rate, u32 mclk)
{
	int temp1 = 0, temp2 = 0;
	int PLL1_divmipi;
	u32 mipi_clk;
	int Pll1_divmipi_map[] = {4, 5, 6, 8};
	struct b52_sensor *sensor = to_b52_sensor(sd);
	OV13850R2A_get_mipiclock(sd, &mipi_clk, mclk);
	b52_sensor_call(sensor, i2c_read, 0x0304, &temp1, 1);
	temp2 = temp1 & 0x03;
	PLL1_divmipi = Pll1_divmipi_map[temp2];
	*rate = mipi_clk / PLL1_divmipi * sensor->drvdata->nr_lane;
	return 0;
}
static void OV13850r2a_write_i2c(struct b52_sensor *sensor, u16 reg, u8 val)
{
	b52_sensor_call(sensor, i2c_write, reg, val, 1);
}

static u8 OV13850r2a_read_i2c(struct b52_sensor *sensor, u16 reg)
{
	int temp1 = 0;
	b52_sensor_call(sensor, i2c_read, reg, &temp1, 1);
	return temp1;
}

static void OV13850r2a_LumaDecoder(uint8_t *pdata, uint8_t *ppara)
{
	uint32_t offset, bit, option;
	uint32_t i, k;
	uint8_t pcenter[16], pmiddle[32], pcorner[72];

	offset = pdata[0];
	bit = pdata[1] >> 4;
	option = pdata[1] & 0xf;

	if (bit <= 5) {
		for (i = 0, k = 2; i < 120; i += 8, k += 5) {
			ppara[i] = pdata[k] >> 3;
			ppara[i+1] = ((pdata[k]&0x7) << 2) | (pdata[k+1] >> 6);
			ppara[i+2] = (pdata[k+1]&0x3e) >> 1;
			ppara[i+3] = ((pdata[k+1]&0x1) << 4) | (pdata[k+2] >> 4);
			ppara[i+4] = ((pdata[k+2]&0xf) << 1) | (pdata[k+3] >> 7);
			ppara[i+5] = (pdata[k+3]&0x7c) >> 2;
			ppara[i+6] = ((pdata[k+3]&0x3) << 3) | (pdata[k+4] >> 5);
			ppara[i+7] = pdata[k+4]&0x1f;
		}
	} else {
		for (i = 0, k = 2; i < 48; i += 8, k += 5) {
			ppara[i] = pdata[k] >> 3;
			ppara[i+1] = ((pdata[k] & 0x7) << 2) | (pdata[k+1] >> 6);
			ppara[i+2] = (pdata[k+1] & 0x3e) >> 1;
			ppara[i+3] = ((pdata[k+1] & 0x1) << 4)|(pdata[k+2] >> 4);
			ppara[i+4] = ((pdata[k+2] & 0xf) << 1)|(pdata[k+3] >> 7);
			ppara[i+5] = (pdata[k+3] & 0x7c) >> 2;
			ppara[i+6] = ((pdata[k+3] & 0x3) << 3) | (pdata[k+4] >> 5);
			ppara[i+7] = pdata[k+4] & 0x1f;
		}
		for (i = 48, k = 32; i < 120; i += 4, k += 3) {
			ppara[i] = pdata[k] >> 2;
			ppara[i+1] = ((pdata[k] & 0x3) << 4) | (pdata[k+1] >> 4);
			ppara[i+2] = ((pdata[k+1] & 0xf) << 2) | (pdata[k+2] >> 6);
			ppara[i+3] = pdata[k+2] & 0x3f;
		}
		memcpy(pcenter, ppara, 16);
		memcpy(pmiddle, ppara + 16, 32);
		memcpy(pcorner, ppara + 48, 72);
		for (i = 0; i < 32; i++)
			pmiddle[i] <<= (bit-6);
		for (i = 0; i < 72; i++)
			pcorner[i] <<= (bit-6);

		if (option == 0) {
			memcpy(ppara, pcorner, 26);
			memcpy(ppara + 26, pmiddle, 8);
			memcpy(ppara + 34, pcorner + 26, 4);
			memcpy(ppara + 38, pmiddle + 8, 2);
			memcpy(ppara + 40, pcenter, 4);
			memcpy(ppara + 44, pmiddle + 10, 2);
			memcpy(ppara + 46, pcorner + 30, 4);
			memcpy(ppara + 50, pmiddle + 12, 2);
			memcpy(ppara + 52, pcenter + 4, 4);
			memcpy(ppara + 56, pmiddle + 14, 2);
			memcpy(ppara + 58, pcorner + 34, 4);
			memcpy(ppara + 62, pmiddle + 16, 2);
			memcpy(ppara + 64, pcenter + 8, 4);
			memcpy(ppara + 68, pmiddle + 18, 2);
			memcpy(ppara + 70, pcorner + 38, 4);
			memcpy(ppara + 74, pmiddle + 20, 2);
			memcpy(ppara + 76, pcenter + 12, 4);
			memcpy(ppara + 80, pmiddle + 22, 2);
			memcpy(ppara + 82, pcorner + 42, 4);
			memcpy(ppara + 86, pmiddle + 24, 8);
			memcpy(ppara + 94, pcorner + 46, 26);
		} else {
			memcpy(ppara, pcorner, 22);
			memcpy(ppara + 22, pmiddle, 6);
			memcpy(ppara + 28, pcorner + 22, 4);
			memcpy(ppara + 32, pmiddle + 6, 6);
			memcpy(ppara + 38, pcorner + 26, 4);
			memcpy(ppara + 42, pmiddle + 12, 1);
			memcpy(ppara + 43, pcenter, 4);
			memcpy(ppara + 47, pmiddle + 13, 1);
			memcpy(ppara + 48, pcorner + 30, 4);
			memcpy(ppara + 52, pmiddle + 14, 1);
			memcpy(ppara + 53, pcenter + 4, 4);
			memcpy(ppara + 57, pmiddle + 15, 1);
			memcpy(ppara + 58, pcorner + 34, 4);
			memcpy(ppara + 62, pmiddle + 16, 1);
			memcpy(ppara + 63, pcenter + 8, 4);
			memcpy(ppara + 67, pmiddle + 17, 1);
			memcpy(ppara + 68, pcorner + 38, 4);
			memcpy(ppara + 72, pmiddle + 18, 1);
			memcpy(ppara + 73, pcenter + 12, 4);
			memcpy(ppara + 77, pmiddle + 19, 1);
			memcpy(ppara + 78, pcorner + 42, 4);
			memcpy(ppara + 82, pmiddle + 20, 6);
			memcpy(ppara + 88, pcorner + 46, 4);
			memcpy(ppara + 92, pmiddle + 26, 6);
			memcpy(ppara + 98, pcorner + 50, 22);
		}
	}

	for (i = 0; i < 120; i++)
		ppara[i] += offset;

	return;
}

static void OV13850r2a_ColorDecoder(uint8_t *pdata, uint8_t *ppara)
{
	uint32_t offset, bit, option;
	uint32_t i, k;
	uint8_t pbase[30];

	offset = pdata[0];
	bit = pdata[1] >> 7;
	option = (pdata[1]&0x40) >> 6;
	ppara[0] = (pdata[1]&0x3e) >> 1;
	ppara[1] = ((pdata[1]&0x1) << 4) | (pdata[2] >> 4);
	ppara[2] = ((pdata[2]&0xf) << 1) | (pdata[3] >> 7);
	ppara[3] = (pdata[3]&0x7c) >> 2;
	ppara[4] = ((pdata[3]&0x3) << 3) | (pdata[4] >> 5);
	ppara[5] = pdata[4]&0x1f;

	for (i = 6, k = 5; i < 30; i += 8, k += 5) {
		ppara[i] = pdata[k]>>3;
		ppara[i+1] = ((pdata[k]&0x07) << 2) | (pdata[k+1] >> 6);
		ppara[i+2] = (pdata[k+1]&0x3e) >> 1;
		ppara[i+3] = ((pdata[k+1]&0x1) << 4) | (pdata[k+2] >> 4);
		ppara[i+4] = ((pdata[k+2]&0xf) << 1) | (pdata[k+3] >> 7);
		ppara[i+5] = (pdata[k+3]&0x7c) >> 2;
		ppara[i+6] = ((pdata[k+3]&0x3) << 3) | (pdata[k+4] >> 5);
		ppara[i+7] = pdata[k+4]&0x1f;
	}
	memcpy(pbase, ppara, 30);
	for (i = 0, k = 20; i < 120; i += 4, k++) {
		ppara[i] = pdata[k]>>6;
		ppara[i+1] = (pdata[k]&0x30) >> 4;
		ppara[i+2] = (pdata[k]&0xc) >> 2;
		ppara[i+3] = pdata[k]&0x3;
	}

	if (option == 0) {
		for (i = 0; i < 5; i++) {
			for (k = 0; k < 6; k++) {
				ppara[i*24+k*2] += pbase[i*6+k];
				ppara[i*24+k*2+1] += pbase[i*6+k];
				ppara[i*24+k*2+12] += pbase[i*6+k];
				ppara[i*24+k*2+13] += pbase[i*6+k];
			}
		}
	} else {
		for (i = 0; i < 6; i++) {
			for (k = 0; k < 5; k++) {
				ppara[i*20+k*2] += pbase[i*5+k];
				ppara[i*20+k*2+1] += pbase[i*5+k];
				ppara[i*20+k*2+10] += pbase[i*5+k];
				ppara[i*20+k*2+11] += pbase[i*5+k];
			}
		}
	}

	for (i = 0; i < 120; i++)
		ppara[i] = (ppara[i] << bit) + offset;

	return;
}

static void OV13850r2a_Lenc_Decoder(uint8_t *pdata, uint8_t *ppara)
{
	OV13850r2a_LumaDecoder(pdata, ppara);
	OV13850r2a_ColorDecoder(pdata + 86, ppara + 120);
	OV13850r2a_ColorDecoder(pdata + 136, ppara + 240);
}

static int OV13850r2a_OTP_access_start(struct b52_sensor *sensor)
{
	int temp;
	temp = OV13850r2a_read_i2c(sensor, 0x5002);
	OV13850r2a_write_i2c(sensor, 0x5002, (0x00 & 0x02) | (temp & (~0x02)));

	OV13850r2a_write_i2c(sensor, 0x3d84, 0xC0);
	OV13850r2a_write_i2c(sensor, 0x3d88, 0x72);
	OV13850r2a_write_i2c(sensor, 0x3d89, 0x20);
	OV13850r2a_write_i2c(sensor, 0x3d8A, 0x73);
	OV13850r2a_write_i2c(sensor, 0x3d8B, 0xBE);
	OV13850r2a_write_i2c(sensor, 0x3d81, 0x01);
	usleep_range(5000, 5010);

	return 0;
}

static int OV13850r2a_OTP_access_end(struct b52_sensor *sensor)
{
	int temp, i;
	for (i = 0x7220; i <= 0x73be; i++)
		OV13850r2a_write_i2c(sensor, i, 0);

	temp = OV13850r2a_read_i2c(sensor, 0x5002);
	OV13850r2a_write_i2c(sensor, 0x5002, (0x02 & 0x02) | (temp & (~0x02)));
	return 0;
}

static int check_otp_info(struct b52_sensor *sensor)
{
	int flag, addr = 0x0;

	flag = OV13850r2a_read_i2c(sensor, 0x7220);
	if ((flag & 0xc0) == 0x40)
		addr = 0x7221;
	else if ((flag & 0x30) == 0x10)
		addr = 0x7229;

	return addr;
}

static int check_otp_lenc(struct b52_sensor *sensor)
{
	int flag, addr = 0x0;

	flag = OV13850r2a_read_i2c(sensor, 0x7231);
	if ((flag & 0xc0) == 0x40)
		addr = 0x7232;
	else if ((flag & 0x30) == 0x10)
		addr = 0x72ef;

	return addr;
}

static int read_otp_info(struct b52_sensor *sensor, int addr,
				struct b52_sensor_otp *otp)
{
	otp->module_id = OV13850r2a_read_i2c(sensor, addr);
	otp->lens_id = OV13850r2a_read_i2c(sensor, addr + 1);
	return 0;
}

static int read_otp_wb(struct b52_sensor *sensor, int addr,
				struct b52_sensor_otp *otp)
{
	int temp;

	temp = OV13850r2a_read_i2c(sensor, addr + 7);
	otp->rg_ratio = (OV13850r2a_read_i2c(sensor, addr + 5) << 2)
				+ ((temp >> 6) & 0x03);
	otp->bg_ratio = (OV13850r2a_read_i2c(sensor, addr + 6) << 2)
			+ ((temp >> 4) & 0x03);
	otp->user_data[0] = 0;
	otp->user_data[1] = 0;

	return 0;
}

static int read_otp_lenc(struct b52_sensor *sensor, int addr, u8 *lenc)
{
	int i;

	for (i = 0; i < 186; i++)
		lenc[i] = OV13850r2a_read_i2c(sensor, addr + i);

	return 0;
}

static int update_lenc(struct b52_sensor *sensor, int addr, u8 *lenc)
{
	int i, temp, flag = 0;
	uint8_t lenc_out[360];
	uint32_t cs_lsc = 0, cs_otp = 0, cs_total = 0;

	temp = OV13850r2a_read_i2c(sensor, 0x5000);
	temp = 0x01 | temp;
	OV13850r2a_write_i2c(sensor, 0x5000, temp);

	for (i = 0; i < 186; i++)
		cs_lsc += lenc[i];

	for (i = 0; i < 360; i++)
		lenc_out[i] = 0;

	OV13850r2a_Lenc_Decoder(lenc, lenc_out);
	for (i = 0; i < 360; i++)
		cs_otp += lenc_out[i];

	cs_lsc = (cs_lsc) % 255 + 1;
	cs_otp = cs_otp % 255 + 1;
	cs_total = (cs_lsc) ^ (cs_otp);

	if ((cs_lsc == OV13850r2a_read_i2c(sensor, addr + 186)) &&
		(cs_otp == OV13850r2a_read_i2c(sensor, addr + 187)))
		flag = 1;
	else if (cs_total == OV13850r2a_read_i2c(sensor, addr + 188))
		flag = 1;

	if (flag == 1) {
		for (i = 0; i < 360; i++)
			OV13850r2a_write_i2c(sensor, 0x5200+i, lenc_out[i]);
	}

	return 0;
}


static int update_awb_gain(struct b52_sensor *sensor, int r_gain,
				int g_gain, int b_gain)
{
	if (r_gain >= 0x400) {
		OV13850r2a_write_i2c(sensor, 0x5056, r_gain >> 8);
		OV13850r2a_write_i2c(sensor, 0x5057, r_gain & 0x00ff);
	}
	if (g_gain >= 0x400) {
		OV13850r2a_write_i2c(sensor, 0x5058, g_gain >> 8);
		OV13850r2a_write_i2c(sensor, 0x5059, g_gain & 0x00ff);
	}
	if (b_gain >= 0x400) {
		OV13850r2a_write_i2c(sensor, 0x505a, b_gain >> 8);
		OV13850r2a_write_i2c(sensor, 0x505b, b_gain & 0x00ff);
	}
	return 0;
}

static int update_otp_info(struct b52_sensor *sensor,
					struct b52_sensor_otp *otp)
{
	int opt_addr;

	opt_addr = check_otp_info(sensor);
	read_otp_info(sensor, opt_addr, otp);

	return 0;
}

static int update_otp_wb(struct b52_sensor *sensor, struct b52_sensor_otp *otp)
{
	int opt_addr;
	int r_gain, g_gain, b_gain, base_gain;

	opt_addr = check_otp_info(sensor);
	read_otp_wb(sensor, opt_addr, otp);

	r_gain = (rg_ratio_typical * 1000) / otp->rg_ratio;
	b_gain = (bg_ratio_typical * 1000) / otp->bg_ratio;
	g_gain = 1000;

	if (r_gain < 1000 || b_gain < 1000) {
		if (r_gain < b_gain)
			base_gain = r_gain;
		else
			base_gain = b_gain;
	} else {
		base_gain = g_gain;
	}

	r_gain = 0x400 * r_gain / base_gain;
	b_gain = 0x400 * b_gain / base_gain;
	g_gain = 0x400 * g_gain / base_gain;

	update_awb_gain(sensor, r_gain, g_gain, b_gain);
	return 0;
}

static int update_otp_lenc(struct b52_sensor *sensor)
{
	int opt_addr;
	u8 lenc[186];

	opt_addr = check_otp_lenc(sensor);
	read_otp_lenc(sensor, opt_addr, lenc);
	update_lenc(sensor, opt_addr, lenc);

	return 0;
}

int OV13850R2A_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *otp)
{
	int ret = 0;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (otp->otp_type ==  SENSOR_TO_SENSOR) {
		OV13850r2a_OTP_access_start(sensor);

		ret = update_otp_info(sensor, otp);
		if (ret < 0)
			goto fail;

		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_WB) {
			ret = update_otp_wb(sensor, otp);
			if (ret < 0)
				goto fail;
		}

		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_LENC) {
			ret = update_otp_lenc(sensor);
			if (ret < 0)
				goto fail;
		}

		OV13850r2a_OTP_access_end(sensor);
	} else {
		return -1;
	}

	return 0;
fail:
	pr_err("otp update fail\n");
	return ret;
}
