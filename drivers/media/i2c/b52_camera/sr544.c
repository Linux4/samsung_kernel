/* Marvell ISP SR544 Driver
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
#include <linux/crc32.h>
#include "sr544.h"
#include <media/mv_sc2_twsi_conf.h>
#include <linux/clk.h>
struct regval_tab sr544_otp[] = {
	{0x0F02, 0x00, 0xff},
	{0x011A, 0x01, 0xff},
	{0x011B, 0x09, 0xff},
	{0x0d04, 0x01, 0xff},
	{0x0d00, 0x07, 0xff},
	{0x004C, 0x01, 0xff},
	{0x003E, 0x01, 0xff},
	{0x0118, 0x01, 0xff},
	{0x010a, (0x0680>>8)&0xff, 0xff},
	{0x010b, (0x0680)&0xff, 0xff},
	{0x0102, 0x01, 0xff},
};

extern int check_load_firmware(void);

static int SR544_get_mipiclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
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

static int SR544_get_dphy_desc(struct v4l2_subdev *sd,
			struct csi_dphy_desc *dphy_desc, u32 mclk)
{
	SR544_get_mipiclock(sd, &dphy_desc->clk_freq, mclk);
	dphy_desc->hs_prepare = 70;
	dphy_desc->hs_zero  = 100;

	return 0;
}
static int SR544_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk)
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
static void SR544_write_reg(struct v4l2_subdev *sd, short reg, char val)
{
	const struct b52_sensor_i2c_attr attr = {
		.reg_len = I2C_16BIT,
		.val_len = I2C_8BIT,
		.addr = 0x28,
	};
	struct b52_cmd_i2c_data data;
	struct regval_tab tab;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	data.attr = &attr;
	data.tab = &tab;
	data.num = 1;
	data.pos = sensor->pos;

	tab.reg = reg;
	tab.val = val;
	tab.mask = 0xff;
	b52_cmd_write_i2c(&data);
}
static int SR544_cmd_write(struct v4l2_subdev *sd, struct regval_tab *tab)
{
	const struct b52_sensor_i2c_attr attr = {
		.reg_len = I2C_16BIT,
		.val_len = I2C_8BIT,
		.addr = 0x28,
	};
	struct b52_cmd_i2c_data data;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!tab) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	data.attr = &attr;
	data.tab = tab;
	data.num = ARRAY_SIZE(sr544_otp);
	data.pos  = sensor->pos;

	/*FIXME:how to get completion*/
	return b52_cmd_write_i2c(&data);
}
static char SR544_read_reg(struct v4l2_subdev *sd, short reg)
{
	const struct b52_sensor_i2c_attr attr = {
		.reg_len = I2C_16BIT,
		.val_len = I2C_8BIT,
		.addr = 0x28,
	};
	struct b52_cmd_i2c_data data;
	struct regval_tab tab;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	data.attr = &attr;
	data.tab = &tab;
	data.num = 1;
	data.pos = sensor->pos;

	tab.reg = reg;
	tab.mask = 0xff;
	b52_cmd_read_i2c(&data);
	return tab.val;
}
__maybe_unused int SR544_check_crc32(
	char *buffer, int len, unsigned int checksum)
{
	unsigned int sum = 0;
	sum = crc32(~0, buffer, len);
	if (sum == checksum)
		return 0;
	else
		return -1;
}
#define BANK1_BASE 0x0690
#define BANK2_BASE 0x08b0
#define BANK3_BASE 0x0ad0

#define GROUP1_OFFSET 0
#define GROUP2_OFFSET 0x60
#define GROUP3_OFFSET 0xe0
#define GROUP4_OFFSET 0x120
#define FULL_OTP_OFFSET 0

#define GROUP1_LEN 0x60
#define GROUP2_LEN 0x80
#define GROUP3_LEN 0x40
#define GROUP4_LEN 0xf0
#define FULL_OTP_LEN 0x670

#define GROUP1_SUM_OFFSET 0x5c
#define GROUP2_SUM_OFFSET 0x7c
#define GROUP3_SUM_OFFSET 0x3c
#define GROUP4_SUM_OFFSET 0x6c

#define GROUP1_ID_OFFSET 0x30
#define GROUP2_AF_OFFSET 0x00
#define GROUP2_GOLDEN_OFFSET 0x40
#define GROUP3_CURRENT_OFFSET 0
#define GROUP4_LSC_OFFSET 0x0c

extern char gVendor_ID[12];
extern char *cam_checkfw_factory;

static int SR544_read_data(struct v4l2_subdev *sd,
				struct b52_sensor_otp *otp)
{
	int i, len, tmp_len = 0;
	int flag, ret = 0;
	char *paddr = NULL;
	ushort bank_addr;
	ushort bank_map[3] = {BANK1_BASE, BANK2_BASE, BANK3_BASE};
	ushort bank_base =  bank_map[0];
	char *bank_grp1, *bank_grp2, *bank_grp3, *bank_grp4, *full_otp;

	if (!otp->user_otp) {
		pr_err("user otp haven't init\n");
		return 0;
	}

	bank_grp1 = devm_kzalloc(sd->dev, GROUP1_LEN, GFP_KERNEL);
	bank_grp2 = devm_kzalloc(sd->dev, GROUP2_LEN, GFP_KERNEL);
	bank_grp3 = devm_kzalloc(sd->dev, GROUP3_LEN, GFP_KERNEL);
	bank_grp4 = devm_kzalloc(sd->dev, GROUP4_LEN, GFP_KERNEL);
	full_otp = devm_kzalloc(sd->dev, FULL_OTP_LEN, GFP_KERNEL);

	SR544_write_reg(sd, 0x0118,  0x00);
	msleep(100);
	SR544_cmd_write(sd, sr544_otp);
#if 0
	SR544_write_reg(sd, 0x0F02,  0x00);
	SR544_write_reg(sd, 0x011A,  0x01);
	SR544_write_reg(sd, 0x011B,  0x09);
	SR544_write_reg(sd, 0x0d04,  0x01);
	SR544_write_reg(sd, 0x0d00,  0x07);
	SR544_write_reg(sd, 0x004C,  0x01);
	SR544_write_reg(sd, 0x003E,  0x01);
	SR544_write_reg(sd, 0x0118,  0x01);

	SR544_write_reg(sd, 0x010a, (0x0680>>8)&0xff);
	SR544_write_reg(sd, 0x010b, (0x0680)&0xff);
	SR544_write_reg(sd, 0x0102, 0x01);
#endif
	if (otp->user_otp->read_otp_len != NULL)
		*(otp->user_otp->read_otp_len) = 0;
	if (otp->user_otp->full_otp_len > 0) {
		len = FULL_OTP_LEN;
		if (otp->user_otp->read_otp_len != NULL)
			*(otp->user_otp->read_otp_len) = FULL_OTP_LEN;
		for (i = 0; i < len; i++)
			full_otp[i] = SR544_read_reg(sd, 0x0108);
		paddr = otp->user_otp->full_otp;
		if (copy_to_user(paddr, &full_otp[FULL_OTP_OFFSET],
							len)) {
			ret = -EIO;
			goto err;
		}
		SR544_write_reg(sd, 0x010a, (0x0680>>8)&0xff);
		SR544_write_reg(sd, 0x010b, (0x0680)&0xff);
		SR544_write_reg(sd, 0x0102, 0x01);
	}

	flag = SR544_read_reg(sd, 0x0108);
	if (flag == 0x01)
		bank_base =  bank_map[0];
	else if (flag == 0x03)
		bank_base =  bank_map[1];
	else if (flag == 0x07)
		bank_base =  bank_map[2];
	else {
		pr_err("the module has no OTP data\n");
		ret = -EPERM;
		goto err;
	}
	len = otp->user_otp->module_data_len;
	if (len > 0) {
		SR544_write_reg(sd, 0x010a, ((bank_base
					+ GROUP1_ID_OFFSET)>>8)&0xff);
		SR544_write_reg(sd, 0x010b, (bank_base
					+ GROUP1_ID_OFFSET)&0xff);
		SR544_write_reg(sd, 0x0102, 0x01);
		for (i = 0; i < len; i++)
			bank_grp1[i] = SR544_read_reg(sd, 0x0108);
		paddr = otp->user_otp->module_data;

		for(i=0;i<11;i++){
			gVendor_ID[i]=bank_grp1[i];

		}	
		
		if (gVendor_ID[10] == 'M') {
			cam_checkfw_factory = "OK";
		} else {
			cam_checkfw_factory = "NG";
		}
		if (copy_to_user(paddr, &bank_grp1[0],
							len)) {
			ret = -EIO;
			goto err;
		}
	}
	len = otp->user_otp->vcm_otp_len;
	if (len != 0 || otp->user_otp->wb_otp_len != 0) {
		bank_addr = bank_base + GROUP2_OFFSET;
		SR544_write_reg(sd, 0x010a, ((bank_addr
					+ GROUP2_AF_OFFSET)>>8)&0xff);
		SR544_write_reg(sd, 0x010b, (bank_addr
					+ GROUP2_AF_OFFSET)&0xff);
		SR544_write_reg(sd, 0x0102, 0x01);
		for (i = 0; i < len; i++)
			bank_grp2[i] = SR544_read_reg(sd, 0x0108);
		if (len > 0) {
			paddr = (char *)otp->user_otp->otp_data + tmp_len;
			if (copy_to_user(paddr,
					&bank_grp2[0],
					len)) {
				ret = -EIO;
				goto err;
			}
			tmp_len += len;
		}
		len = otp->user_otp->wb_otp_len/2;
		SR544_write_reg(sd, 0x010a, ((bank_addr
					+ GROUP2_GOLDEN_OFFSET)>>8)&0xff);
		SR544_write_reg(sd, 0x010b, (bank_addr
					+ GROUP2_GOLDEN_OFFSET)&0xff);
		SR544_write_reg(sd, 0x0102, 0x01);
		for (i = 0; i < len; i++)
			bank_grp2[i] = SR544_read_reg(sd, 0x0108);
		if (len > 0) {
			paddr = (char *) otp->user_otp->otp_data + tmp_len;
			if (copy_to_user(paddr,
				&bank_grp2[0], len)) {
				ret = -EIO;
				goto err;
			}
			tmp_len += len;
		}
	}
	len = otp->user_otp->wb_otp_len/2;
	if (len > 0) {
		bank_addr = bank_base + GROUP3_OFFSET;
		SR544_write_reg(sd, 0x010a, ((bank_addr
					+ GROUP3_CURRENT_OFFSET)>>8)&0xff);
		SR544_write_reg(sd, 0x010b, (bank_addr
					+ GROUP3_CURRENT_OFFSET)&0xff);
		SR544_write_reg(sd, 0x0102, 0x01);
		for (i = 0; i < len; i++)
			bank_grp3[i] = SR544_read_reg(sd, 0x0108);
		paddr = (char *)otp->user_otp->otp_data + tmp_len;
		if (copy_to_user(paddr,
				&bank_grp3[0], len)) {
			ret = -EIO;
			goto err;
		}
		tmp_len += len;
	}
	len = otp->user_otp->lsc_otp_len;
	if (len > 0) {
		bank_addr = bank_base + GROUP4_OFFSET;
		SR544_write_reg(sd, 0x010a, ((bank_addr
					+ GROUP4_LSC_OFFSET)>>8)&0xff);
		SR544_write_reg(sd, 0x010b, (bank_addr
					+ GROUP4_LSC_OFFSET)&0xff);
		SR544_write_reg(sd, 0x0102, 0x01);
		for (i = 0; i < len; i++)
			bank_grp4[i] = SR544_read_reg(sd, 0x0108);
		paddr = (char *)(otp->user_otp->otp_data + tmp_len);
		if (copy_to_user(paddr, &bank_grp4[0], len)) {
			ret = -EIO;
			goto err;
		}
	}
err:
	SR544_write_reg(sd, 0x0118,  0x00);
	SR544_write_reg(sd, 0x003e,  0x00);
	SR544_write_reg(sd, 0x0118,  0x01);
	devm_kfree(sd->dev, bank_grp1);
	devm_kfree(sd->dev, bank_grp2);
	devm_kfree(sd->dev, bank_grp3);
	devm_kfree(sd->dev, bank_grp4);
	devm_kfree(sd->dev, full_otp);
	return ret;
}
static int SR544_update_otp(struct v4l2_subdev *sd,
				struct b52_sensor_otp *otp)
{
	if (otp->otp_type ==  SENSOR_TO_ISP)
		return SR544_read_data(sd, otp);
	return -1;
}

static int SR544_s_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;
	int reset_delay = 100;
	struct sensor_power *power;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct b52_sensor *sensor = to_b52_sensor(sd);

	power = (struct sensor_power *) &(sensor->power);
	if (on) {
		if (power->ref_cnt++ > 0)
			return 0;
		power->pwdn = devm_gpiod_get(&client->dev, "pwdn");
		if (IS_ERR(power->pwdn)) {
			dev_warn(&client->dev, "Failed to get gpio pwdn\n");
			power->pwdn = NULL;
		} else {
			ret = gpiod_direction_output(power->pwdn, 0);
			if (ret < 0) {
				dev_err(&client->dev, "Failed to set gpio pwdn\n");
				goto st_err;
			}
		}

		power->rst = devm_gpiod_get(&client->dev, "reset");
		if (IS_ERR(power->rst)) {
			dev_warn(&client->dev, "Failed to get gpio reset\n");
			power->rst = NULL;
		} else {
			ret = gpiod_direction_output(power->rst, 0);
			if (ret < 0) {
				dev_err(&client->dev, "Failed to set gpio rst\n");
				goto rst_err;
			}
		}
		if (power->af_2v8) {
			regulator_set_voltage(power->af_2v8,
						2800000, 2800000);
			ret = regulator_enable(power->af_2v8);
			if (ret < 0)
				goto af_err;
		}
		usleep_range(5000, 8000);
		if (power->dovdd_1v8) {
			regulator_set_voltage(power->dovdd_1v8,
						1800000, 1800000);
			ret = regulator_enable(power->dovdd_1v8);
			if (ret < 0)
				goto dovdd_err;
		}
		usleep_range(5000, 8000);
		if (power->avdd_2v8) {
			regulator_set_voltage(power->avdd_2v8,
						2800000, 2800000);
			ret = regulator_enable(power->avdd_2v8);
			if (ret < 0)
				goto avdd_err;
		}
		usleep_range(5000, 8000);
		if (power->dvdd_1v2) {
			regulator_set_voltage(power->dvdd_1v2,
						1200000, 1200000);
			ret = regulator_enable(power->dvdd_1v2);
			if (ret < 0)
				goto dvdd_err;
		}
		usleep_range(5000, 8000);
		if (power->pwdn)
			gpiod_set_value_cansleep(power->pwdn, 0);
	usleep_range(5000, 8000);
		clk_set_rate(sensor->clk, sensor->mclk);
		clk_prepare_enable(sensor->clk);
		msleep(15);
		if (power->rst) {
			if (sensor->drvdata->reset_delay)
				reset_delay = sensor->drvdata->reset_delay;
			usleep_range(reset_delay, reset_delay + 10);
			gpiod_set_value_cansleep(power->rst, 0);
		}
		if (sensor->i2c_dyn_ctrl) {
			ret = sc2_select_pins_state(sensor->pos - 1,
					SC2_PIN_ST_SCCB, SC2_MOD_B52ISP);
			if (ret < 0) {
				pr_err("b52 sensor i2c pin is not configured\n");
				goto i2c_err;
			}
		}
	} else {
		if (WARN_ON(power->ref_cnt == 0))
			return -EINVAL;

		if (--power->ref_cnt > 0)
			return 0;

		if (check_load_firmware()) {
			int distance =  0x0;
			b52isp_set_focus_distance(distance, 0);
		}

		if (sensor->i2c_dyn_ctrl) {
			ret = sc2_select_pins_state(sensor->pos - 1,
					SC2_PIN_ST_GPIO, SC2_MOD_B52ISP);
			if (ret < 0)
				pr_err("b52 sensor gpio pin is not configured\n");
		}
		if (power->rst)
			gpiod_set_value_cansleep(power->rst, 1);

		msleep(10);
		
		clk_disable_unprepare(sensor->clk);
		
		if (power->pwdn)
			gpiod_set_value_cansleep(power->pwdn, 1);
		
		if (power->dvdd_1v2)
			regulator_disable(power->dvdd_1v2);

		msleep(1);

		if (power->avdd_2v8)
			regulator_disable(power->avdd_2v8);
		
		msleep(1);

		if (power->dovdd_1v8)
			regulator_disable(power->dovdd_1v8);

		
		msleep(1);
		
		if (power->af_2v8)
			regulator_disable(power->af_2v8);
		
		if (sensor->power.rst)
			devm_gpiod_put(&client->dev, sensor->power.rst);
		if (sensor->power.pwdn)
			devm_gpiod_put(&client->dev, sensor->power.pwdn);
		sensor->sensor_init = 0;
	}

	return ret;
i2c_err:
	clk_disable_unprepare(sensor->clk);
	if (power->af_2v8)
		regulator_disable(power->af_2v8);
af_err:
	if (power->dvdd_1v2)
		regulator_disable(power->dvdd_1v2);
dvdd_err:
	if (power->dovdd_1v8)
		regulator_disable(power->dovdd_1v8);
avdd_err:
	if (sensor->power.rst)
		devm_gpiod_put(&client->dev, sensor->power.rst);
dovdd_err:
	if (power->avdd_2v8)
		regulator_disable(power->af_2v8);
rst_err:
	if (sensor->power.pwdn)
		devm_gpiod_put(&client->dev, sensor->power.pwdn);

st_err:
	power->ref_cnt--;
	return ret;
}
