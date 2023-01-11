/* Marvell ISP HI551 Driver
 *
 * Copyright (C) 2009-2014 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image HI551
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

#include "hi551.h"

static int HI551_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{

	int temp1, temp2;
	int Pll_prediv, Pll_multiplier, Pll_mipi_clk_div;
	int Pll_mipi_clk_div_map[] = {1, 2, 4, 8};

	struct b52_sensor *sensor = to_b52_sensor(sd);
	b52_sensor_call(sensor, i2c_read, 0x0b14, &temp1, 1);
	Pll_prediv = (temp1>>2) & 0x0f;
	Pll_multiplier = (temp1>>8) & 0x07f;
	b52_sensor_call(sensor, i2c_read, 0x0b16, &temp1, 1);
	temp2 = (temp1>>7) & 0x03;
	Pll_mipi_clk_div = Pll_mipi_clk_div_map[temp2];

	*rate = mclk / (Pll_prediv + 1) * Pll_multiplier / Pll_mipi_clk_div;
	return 0;
}

static int HI551_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	HI551_get_mipiclock(sd, &dphy_desc->clk_freq, mclk);
	dphy_desc->hs_prepare = 70;
	dphy_desc->hs_zero  = 150;

	return 0;
}

static int HI551_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
{
	int temp1, temp2;
	int Pll_prediv, Pll_multiplier, Pll_vt_syst_clk_div;
	int Pll_vt_syst_clk_div_map[] = {1, 2, 3, 4, 5, 6, 8, 10};

	struct b52_sensor *sensor = to_b52_sensor(sd);

	b52_sensor_call(sensor, i2c_read, 0x0b14, &temp1, 1);

	Pll_prediv = (temp1>>2) & 0x0f;
	Pll_multiplier = (temp1>>8) & 0x07f;
	b52_sensor_call(sensor, i2c_read, 0x0b16, &temp1, 1);
	temp2 = (temp1>>12) & 0x07;

	Pll_vt_syst_clk_div = Pll_vt_syst_clk_div_map[temp2];

	*rate = mclk / (Pll_prediv + 1) *  Pll_multiplier / Pll_vt_syst_clk_div;
	*rate = *rate * 2;

	return 0;
}

static void HI551_I2C_single_byte_write(struct b52_sensor *sensor, u16 reg, u8 val)
{
	const struct b52_sensor_i2c_attr attr = {
		.reg_len = I2C_16BIT,
		.val_len = I2C_8BIT,
		.addr = 0x20,
	};
	struct b52_cmd_i2c_data data;
	struct regval_tab tab;

	data.attr = &attr;
	data.tab = &tab;
	data.num = 1;
	data.pos = sensor->pos;

	tab.reg = reg;
	tab.val = val;
	tab.mask = 0xff;

	b52_cmd_write_i2c(&data);
}

static char HI551_I2C_single_byte_read(struct b52_sensor *sensor, u16 reg)
{
	const struct b52_sensor_i2c_attr attr = {
		.reg_len = I2C_16BIT,
		.val_len = I2C_8BIT,
		.addr = 0x20,
	};
	struct b52_cmd_i2c_data data;
	struct regval_tab tab;

	data.attr = &attr;
	data.tab = &tab;
	data.num = 1;
	data.pos = sensor->pos;

	tab.reg = reg;
	tab.mask = 0xff;
	b52_cmd_read_i2c(&data);

	return tab.val;
}

static int HI551_read_otp_reg(struct b52_sensor *sensor, u16 address)
{
	int temp;
	u8 addr_h, addr_l;

	addr_h = (u8)((address >> 8) & 0xff);
	addr_l = (u8)(address & 0xff);

	HI551_I2C_single_byte_write(sensor, 0x010a , addr_h);
	HI551_I2C_single_byte_write(sensor, 0x010b , addr_l);
	HI551_I2C_single_byte_write(sensor, 0x0102 , 0x00);

	temp = HI551_I2C_single_byte_read(sensor, 0x0108);

	return temp;
}

/***********************************
*   index: index of otp group. ( 0, 1, 2 )
*   return: 0, group index is empty
*           1, group index has invalid data
*           2, group index has valid data
************************************/
static int check_otp_info(struct b52_sensor *sensor, int index)
{
	int flag;
	u16 flag_addr;

    /* read group flag*/
	flag_addr = OTP_DRV_START_ADDR + index * OTP_DRV_GROUP_SIZE;

	flag = HI551_read_otp_reg(sensor, flag_addr);

	if (flag == 0x00) {
		pr_info("%s: otp group %d is empty!\n", __func__, index);
		return 0;
	} else if (flag == 0x01) {
		pr_info("%s: otp group %d has valid data!\n", __func__, index);
		return 2;
	} else if (flag == 0xff) {
		pr_info("%s: otp group %d has invalid data!\n", __func__, index);
		return 1;
	} else {
		pr_err("%s: Invalid flag value:%d\n", __func__, flag);
		return 1;
	}
}

static int read_otp_info(struct b52_sensor *sensor, int index,
				struct b52_sensor_otp *otp)
{
	u16 start_addr;
	start_addr = OTP_DRV_START_ADDR + index * OTP_DRV_GROUP_SIZE;

	otp->module_id = HI551_read_otp_reg(sensor, start_addr + 1);
	otp->lens_id = HI551_read_otp_reg(sensor, start_addr + 2);

	return 0;
}

static int update_otp_info(struct b52_sensor *sensor,
					struct b52_sensor_otp *otp)
{
	int i;
	int otp_index;
	int temp;

	for (i = 0; i <= OTP_DRV_GROUP_COUNT; i++) {
		/*   return: 0, group index is empty *
		*   1, group index has invalid data *
		*   2, group index has valid data */
		temp = check_otp_info(sensor, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}
	if (i > OTP_DRV_GROUP_COUNT) {
		pr_err("%s: no valid otp group!\n", __func__);
		return -1;
	}

	read_otp_info(sensor, otp_index, otp);
	return 0;
}

/********************************
*   index: index of otp group. ( 0, 1, 2 )
*   return: 0, group index is empty
*           1, group index has invalid data
*           2, group index has valid data
********************************/
static int check_otp_wb(struct b52_sensor *sensor, int index)
{
	int ret;

	ret = check_otp_info(sensor, index);

	return ret;
}

/*check and read otp value*/
static int check_read_otp_wb(struct b52_sensor *sensor, int index,
				struct b52_sensor_otp *otp)
{
	u16 start_addr, end_addr, check_value_addr, addr;
	u8 i, otp_value[0x1f] = {0};
	u16 temp, check_value = 0, sum = 0;

	start_addr = OTP_DRV_START_ADDR + index * OTP_DRV_GROUP_SIZE;
	end_addr = start_addr + 0x1E;
	check_value_addr = start_addr + 0x1F;

	for (i = 1; i <= 0x1E; i++) {
		addr = start_addr + i;
		otp_value[i] = HI551_read_otp_reg(sensor, addr);
		sum += otp_value[i];
	}

	check_value = HI551_read_otp_reg(sensor, check_value_addr);
	temp = sum % 0xff;

	if (temp == check_value) {
		pr_info("%s: group %d data is correct!\n", __func__, index);

		otp->rg_ratio = ((u32)otp_value[0x08] & 0x03) << 8;
		otp->rg_ratio += otp_value[0x09];

		otp->bg_ratio = ((u32)otp_value[0x0a] & 0x03) << 8;
		otp->bg_ratio += otp_value[0x0b];

		otp->gg_ratio = ((u32)otp_value[0x0c] & 0x03) << 8;
		otp->gg_ratio += otp_value[0x0d];

		otp->golden_rg_ratio = ((u32)otp_value[0x0e] & 0x03) << 8;
		otp->golden_rg_ratio += otp_value[0x0f];

		otp->golden_bg_ratio = ((u32)otp_value[0x10] & 0x03) << 8;
		otp->golden_bg_ratio += otp_value[0x11];

		otp->golden_gg_ratio = ((u32)otp_value[0x12] & 0x03) << 8;
		otp->golden_gg_ratio += otp_value[0x13];
		return 0;
	} else {
		pr_err("%s: group %d data is error!\n", __func__, index);
		return -1;
	}
	return 0;
}


static int update_awb_gain(struct b52_sensor *sensor,
						struct b52_sensor_otp *otp)
{
	int rg_ratio , bg_ratio, gg_ratio, golden_rg_ratio;
	int golden_bg_ratio, golden_gg_ratio;
	int R_gain, B_gain, Gr_gain, Gb_gain, Min_gain;
	int temp;

	rg_ratio = otp->rg_ratio;
	bg_ratio = otp->bg_ratio;
	gg_ratio = otp->gg_ratio;

	golden_rg_ratio = otp->golden_rg_ratio;
	golden_bg_ratio = otp->golden_bg_ratio;
	golden_gg_ratio = otp->golden_gg_ratio;

	Gr_gain = 0x100;
	R_gain  = 0x100 * golden_rg_ratio / rg_ratio;
	B_gain  = 0x100 * golden_bg_ratio / bg_ratio;
	Gb_gain = 0x100 * golden_gg_ratio / gg_ratio;

	Min_gain = MIN(MIN(Gr_gain, R_gain), MIN(B_gain, Gb_gain));

	R_gain  = R_gain  * 0x100 / Min_gain;
	Gr_gain = Gr_gain * 0x100 / Min_gain;
	Gb_gain = Gb_gain * 0x100 / Min_gain;
	B_gain  = B_gain  * 0x100 / Min_gain;

	b52_sensor_call(sensor, i2c_read, D_GAIN_CTRL_ADDR, &temp, 1);

	if (!(temp & 0x400))
		pr_err("%s: hi551 digital gain is disable, please check\n", __func__);

	b52_sensor_call(sensor, i2c_write, D_GAIN_Gr_ADDR, Gr_gain, 1);

	b52_sensor_call(sensor, i2c_write, D_GAIN_Gb_ADDR, Gb_gain, 1);
	b52_sensor_call(sensor, i2c_write, D_GAIN_R_ADDR, R_gain, 1);
	b52_sensor_call(sensor, i2c_write, D_GAIN_B_ADDR, B_gain, 1);

	return 0;
}

static int update_otp_wb(struct b52_sensor *sensor,
						struct b52_sensor_otp *otp)
{
	int ret;
	int otp_index;
	int i, temp;

	for (i = 0; i <= OTP_DRV_GROUP_COUNT; i++) {
		/*   return: 0, group index is empty *
		*   1, group index has invalid data *
		*   2, group index has valid data */
		temp = check_otp_wb(sensor, i);
		if (temp == 2) {
			otp_index = i;
			break;
		}
	}

	if (i > OTP_DRV_GROUP_COUNT) {
		pr_err("%s: no valid wb otp group!\n", __func__);
		return -1;
	}

	ret = check_read_otp_wb(sensor, otp_index, otp);

	return ret;
}

static void HI551_switch_to_otp_mode(struct b52_sensor *sensor)
{
    /*switch to otp mode*/
	HI551_I2C_single_byte_write(sensor, 0x0118, 0x00);
	/*sleep on*/
	msleep(20);
	/*pll disable*/
	HI551_I2C_single_byte_write(sensor, 0x0F02, 0x00);
	/*CP TRI_H*/
	HI551_I2C_single_byte_write(sensor, 0x011A, 0x01);
	/*IPGM TRIM_H*/
	HI551_I2C_single_byte_write(sensor, 0x011B, 0x09);
	/*Fsync Output enable*/
	HI551_I2C_single_byte_write(sensor, 0x0d04, 0x01);
	/*Fsync Output Drivability*/
	HI551_I2C_single_byte_write(sensor, 0x0d00, 0x07);
	/*TG MCU enable*/
	HI551_I2C_single_byte_write(sensor, 0x004C, 0x01);
	/*OTP R/W*/
	HI551_I2C_single_byte_write(sensor, 0x003E, 0x01);
	/*sleep off */
	HI551_I2C_single_byte_write(sensor, 0x0118, 0x01);
	msleep(20);
	/*TG MCU disable*/
	HI551_I2C_single_byte_write(sensor, 0x004C, 0x00);
	/*TG MCU enable*/
	HI551_I2C_single_byte_write(sensor, 0x004C, 0x01);
	msleep(20);

	return;
}

static void HI551_switch_to_normal_mode(struct b52_sensor *sensor)
{
	/*sleep on*/
	HI551_I2C_single_byte_write(sensor, 0x0118, 0x00);
	/*OTP Mode off*/
	HI551_I2C_single_byte_write(sensor, 0x003E, 0x00);
	/*sleep off */
	HI551_I2C_single_byte_write(sensor, 0x0118, 0x01);
}

static void HI551_print_otp_info(struct b52_sensor_otp *otp)
{
	pr_info("Hi551 OTP INOF :\n");
	pr_info("module_id          = 0x%x\n", otp->module_id);
	pr_info("lens_id            = 0x%x\n", otp->lens_id);

	pr_info("rg_ratio           = 0x%x\n", otp->rg_ratio);
	pr_info("bg_ratio           = 0x%x\n", otp->bg_ratio);
	pr_info("gg_ratio           = 0x%x\n", otp->gg_ratio);

	pr_info("golden_rg_ratio    = 0x%x\n", otp->golden_rg_ratio);
	pr_info("golden_bg_ratio    = 0x%x\n", otp->golden_bg_ratio);
	pr_info("golden_gg_ratio    = 0x%x\n", otp->golden_gg_ratio);
}

static int HI551_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *otp)
{
	int ret;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (otp->otp_type ==  SENSOR_TO_SENSOR) {
		HI551_switch_to_otp_mode(sensor);

		ret = update_otp_info(sensor, otp);
		if (ret < 0) {
			pr_err("%s: update otp info failed!\n", __func__);
			HI551_switch_to_normal_mode(sensor);
			return ret;
		}

		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_WB) {
			ret = update_otp_wb(sensor, otp);
			if (ret < 0) {
				pr_err("%s: update otp wb failed!\n", __func__);
				HI551_switch_to_normal_mode(sensor);
				return ret;
			}
		}

		if (otp->otp_ctrl & V4L2_CID_SENSOR_OTP_CONTROL_LENC)
			pr_err("%s: HI551 no lenc otp at present!\n", __func__);

		HI551_print_otp_info(otp);

		HI551_switch_to_normal_mode(sensor);

		ret = update_awb_gain(sensor, otp);
		if (ret < 0)
			pr_err("%s: update awb gain failed!\n", __func__);

		return ret;
	}

	return 0;
}
