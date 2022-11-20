/*
 * drivers/video/decon_7580/panels/ltm184hl01_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <video/mipi_display.h>
#include "../dsim.h"
#include "dsim_panel.h"

#include "ltm184hl01_param.h"

#define BoostUPbrightness 3

int	backlight_on;
int board_rev;
int lvds_disp_on_comp;

struct i2c_client *lvds_client;
struct i2c_client *backlight1_client;
struct i2c_client *backlight2_client;

#define V5D3BX_VEESTRENGHT		0x00001f07
#define V5D3BX_VEEDEFAULTVAL		0
#define V5D3BX_DEFAULT_STRENGHT		5
#define V5D3BX_DEFAULT_LOW_STRENGHT	8
#define V5D3BX_DEFAULT_HIGH_STRENGHT	10
#define V5D3BX_MAX_STRENGHT		15

#define V5D3BX_CABCBRIGHTNESSRATIO	815
#define V5D3BX_180HZ_DEFAULT_RATIO	180957 /*0x2C2DD:100% duty*/
#define AUTOBRIGHTNESS_LIMIT_VALUE	207

#define MAX_BRIGHTNESS_LEVEL		255
#define MID_BRIGHTNESS_LEVEL		112
#define DIM_BRIGHTNESS_LEVEL		12
#define LOW_BRIGHTNESS_LEVEL		3
#define MIN_BRIGHTNESS				3

#define V5D3BX_MAX_BRIGHTNESS_LEVEL		255
#define V5D3BX_MID_BRIGHTNESS_LEVEL		112
#define V5D3BX_DIM_BRIGHTNESS_LEVEL		12
#define V5D3BX_LOW_BRIGHTNESS_LEVEL		3
#define V5D3BX_MIN_BRIGHTNESS			3

#define BL_DEFAULT_BRIGHTNESS		MID_BRIGHTNESS_LEVEL

/*#define CONFIG_BLIC_TUNING*/
#if defined(CONFIG_BLIC_TUNING)
struct dsim_device *dsim_t;
#endif

static struct LP8558_rom_data LP8558_BOOSTUP[] = {
	{0xA7,	0xF3},
	{0x00,	0xE3},/*Apply BRT setting for outdoor 22.5mA*//*480cd*/
};

static struct LP8558_rom_data LP8558_NORMAL[] = {
	{0xA7,	0xF3},
	{0x00,	0xCB},/*Apply BRT setting for normal for 21.5mA*//*420cd*/
};

static int dsim_panel_set_brightness(struct dsim_device *dsim, int force)
{
	int ret = 0;

	struct panel_private *panel = &dsim->priv;

	panel->ops->lvds_pwm_set(dsim);

	return ret;
}

static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct panel_private *priv = bl_get_data(bd);
	struct dsim_device *dsim;

	dsim = container_of(priv, struct dsim_device, priv);

	if (brightness < UI_MIN_BRIGHTNESS || brightness > UI_MAX_BRIGHTNESS) {
		pr_alert("Brightness should be in the range of 0 ~ 255\n");
		ret = -EINVAL;
		goto exit_set;
	}

	ret = dsim_panel_set_brightness(dsim, 0);
	if (ret) {
		dsim_err("%s : fail to set brightness\n", __func__);
		goto exit_set;
	}
exit_set:
	return ret;

}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};

static int dsim_backlight_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	panel->bd = backlight_device_register("panel", dsim->dev, &dsim->priv,
					&panel_backlight_ops, NULL);
	if (IS_ERR(panel->bd)) {
		dsim_err("%s:failed register backlight\n", __func__);
		ret = PTR_ERR(panel->bd);
	}

	panel->bd->props.max_brightness = UI_MAX_BRIGHTNESS;
	panel->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	return ret;
}


static int ltm184hl01_exit(struct dsim_device *dsim)
{
	int ret = 0;

	ret = gpio_request_one(backlight_on, GPIOF_OUT_INIT_LOW, "BLIC_ON");
	gpio_free(backlight_on);
	usleep_range(10000, 11000);

	msleep(100);

	lvds_disp_on_comp = 0;
	dsim_info("[lcd] : %s was called\n", __func__);

	return ret;
}

#define LVDS_I2C_RETRY_COUNT (2)


#if 0
static int ql_lvds_ReadReg(u16 reg)
{
	u8 tx_data[3], rx_data[4];
	int ret, val;

	struct i2c_msg msg[2] = {
		/* first write slave position to i2c devices */
		{ lvds_client->addr, 0, ARRAY_SIZE(tx_data), tx_data },
		/* Second read data from position */
		{ lvds_client->addr, I2C_M_RD, ARRAY_SIZE(rx_data), rx_data}
	};

	tx_data[0] = 0xE;
	tx_data[1] = (reg >> 8) & 0xff;
	tx_data[2] = reg & 0xff;

	ret = i2c_transfer(lvds_client->adapter, msg, 2);
	if (unlikely(ret < 0)) {
		dsim_err(" [%s] i2c read failed  on reg 0x%04x error %d\n",
				__func__,  reg, ret);
		return ret;
	}
	if (unlikely(ret < ARRAY_SIZE(msg))) {
		dsim_err("%s: reg 0x%04x msgs %d\n" ,
				__func__, reg, ret);
		return -EAGAIN;
	}
	val = (int)rx_data[0] << 24 | ((int)(rx_data[1]) << 16) |
		((int)(rx_data[2]) << 8) | ((int)(rx_data[3]));
	return val;
}
#endif

static int ql_lvds_WriteReg(u16 addr, u32 w_data)
{
	int retries = LVDS_I2C_RETRY_COUNT;
	int ret = 0, err = 0;
	u8 tx_data[7];

	struct i2c_msg msg[] = {
		{lvds_client->addr, 0, 7, tx_data }
	};

	/* NOTE: Register address big-endian, data little-endian. */
	tx_data[0] = 0xA;

	tx_data[1] = (u8)(addr >> 8) & 0xff;
	tx_data[2] = (u8)addr & 0xff;
	tx_data[3] = w_data & 0xff;
	tx_data[4] = (w_data >> 8) & 0xff;
	tx_data[5] = (w_data >> 16) & 0xff;
	tx_data[6] = (w_data >> 24) & 0xff;

	do {

		ret = i2c_transfer(lvds_client->adapter, msg, ARRAY_SIZE(msg));
		if (likely(ret == 1))
			break;

		usleep_range(10000, 11000);
		err = ret;
	} while (--retries > 0);


	/* Retry occured */
	if (unlikely(retries < LVDS_I2C_RETRY_COUNT)) {
			dsim_err("[vx5b3d]i2c_write: error %d, write (%04X, %08X), retry %d",
					err, addr, w_data, LVDS_I2C_RETRY_COUNT - retries);
	}

	if (unlikely(ret != 1)) {
			dsim_err("[vx5b3d]I2C does not work\n");
			return -EIO;
	}

	return 0;
}

static int Is_lvds_sysclk_16M(void)
{
	/*
		system_rev 0 ,9 : 19.2Mhz
		system_rev 1~8 ,10~:16.9344Mhz
	*/
	if ( board_rev == 0 || board_rev == 9 )
		return 0;
	else
		return 1;
}

static int Is_lvds_RGB666_backlight_RT8561(void)
{
	if (board_rev == 0)
		return 1;
	else
		return 0;
}

static int lp8558_array_write(struct i2c_client *client, const struct LP8558_rom_data *eprom_ptr, int eprom_size)
{
	int i = 0;
	int ret = 0;

	if (Is_lvds_RGB666_backlight_RT8561())
		return 0;

	if (client == backlight1_client)
		dsim_info("[backlight] lp8558_1_i2c\n");
	else
		dsim_info("[backlight] lp8558_2_i2c\n");

	for (i = 0; i < eprom_size; i++) {
		ret = i2c_smbus_write_byte_data(client, eprom_ptr[i].addr, eprom_ptr[i].val);
		if (ret < 0)
			dsim_err("[lp8558]i2c_write: error %d reg 0x%x : 0x%x\n", ret, eprom_ptr[i].addr, eprom_ptr[i].val);
	}

	return 0;
}

static int ltm184hl01_lvds_init(struct dsim_device *dsim)
{
	int ret = 0;
	unsigned int dsi_hs_clk;

	dsi_hs_clk = dsim->lcd_info.hs_clk;
	/* dsi_hs_clk
	lte :	843M
	wifi : 822M
	*/

	dsim_info("[lcd] : %s hs_clk(%d) was called\n", __func__, dsi_hs_clk);

	ql_lvds_WriteReg(0x700, 0x18900040);
	if (Is_lvds_sysclk_16M()){
		if (dsi_hs_clk == 843)
			ql_lvds_WriteReg(0x704, 0x10238);
		else
			ql_lvds_WriteReg(0x704, 0x1022A);
	}
	else
		ql_lvds_WriteReg(0x704, 0x101DA);
	ql_lvds_WriteReg(0x70C, 0x00004604);
	ql_lvds_WriteReg(0x710, 0x545100B);
	ql_lvds_WriteReg(0x714, 0x20);
	ql_lvds_WriteReg(0x718, 0x00000102);
	ql_lvds_WriteReg(0x71C, 0xA8002F);
	ql_lvds_WriteReg(0x720, 0x1800);
	ql_lvds_WriteReg(0x154, 0x00000000);
	usleep_range(1000, 1100);
	ql_lvds_WriteReg(0x154, 0x80000000);
	usleep_range(1000, 1100);
	ql_lvds_WriteReg(0x700, 0x18900840);
	ql_lvds_WriteReg(0x70C, 0x5E46);
	ql_lvds_WriteReg(0x718, 0x00000202);

	ql_lvds_WriteReg(0x154, 0x00000000);
	usleep_range(1000, 1100);
	ql_lvds_WriteReg(0x154, 0x80000000);
	usleep_range(1000, 1100);

	ql_lvds_WriteReg(0x120, 0x5);
	if (dsi_hs_clk == 843)
		ql_lvds_WriteReg(0x124, 0xEE1C780);
	else
		ql_lvds_WriteReg(0x124, 0x9E1C780);
	ql_lvds_WriteReg(0x128, 0x102008);
	if (dsi_hs_clk == 843)
		ql_lvds_WriteReg(0x12C, 0x3B);
	else
		ql_lvds_WriteReg(0x12C, 0x27);
	if (Is_lvds_sysclk_16M())
		ql_lvds_WriteReg(0x130, 0x3C18);
	else
		ql_lvds_WriteReg(0x130, 0x3C10);
	ql_lvds_WriteReg(0x134, 0x15);
	ql_lvds_WriteReg(0x138, 0xFF0000);
	ql_lvds_WriteReg(0x13C, 0x0);

	ql_lvds_WriteReg(0x114, 0xc6302);/*added for bl pwm*/
	ql_lvds_WriteReg(0x140, 0x10000);

	ql_lvds_WriteReg(0x20C, 0x24);
	if (Is_lvds_sysclk_16M())
		ql_lvds_WriteReg(0x21C, 0x69E);
	else
		ql_lvds_WriteReg(0x21C, 0x780);
	ql_lvds_WriteReg(0x224, 0x7);
	ql_lvds_WriteReg(0x228, 0x50000);
	ql_lvds_WriteReg(0x22C, 0xFF08);
	ql_lvds_WriteReg(0x230, 0x1);
	ql_lvds_WriteReg(0x234, 0xCA033E10);
	ql_lvds_WriteReg(0x238, 0x00000060);
	ql_lvds_WriteReg(0x23C, 0x82E86030);
	ql_lvds_WriteReg(0x240, 0x28616088);
	ql_lvds_WriteReg(0x244, 0x00160285);
	ql_lvds_WriteReg(0x250, 0x600882A8);
	if (dsi_hs_clk == 843)
		ql_lvds_WriteReg(0x258, 0x8003B);
	else
		ql_lvds_WriteReg(0x258, 0x80027);
	ql_lvds_WriteReg(0x158, 0x0);
	usleep_range(1000, 1100);
	ql_lvds_WriteReg(0x158, 0x1);
	usleep_range(1000, 1100);
	ql_lvds_WriteReg(0x37C, 0x00001063);
	ql_lvds_WriteReg(0x380, 0x82A86030);
	ql_lvds_WriteReg(0x384, 0x2861408B);
	ql_lvds_WriteReg(0x388, 0x00130285);
	ql_lvds_WriteReg(0x38C, 0x10630009);
	ql_lvds_WriteReg(0x394, 0x400B82A8);
	ql_lvds_WriteReg(0x600, 0x16CC78D);

	if(Is_lvds_RGB666_backlight_RT8561())
		ql_lvds_WriteReg(0x608, 0x20F0A);/*20 : delay for skew*/
	else
		ql_lvds_WriteReg(0x608, 0x20F80);/*20 : delay for skew*/

	ql_lvds_WriteReg(0x154, 0x00000000);
	usleep_range(1000, 1100);
	ql_lvds_WriteReg(0x154, 0x80000000);
	usleep_range(1000, 1100);

	/*vee strenght initialization*/
	ql_lvds_WriteReg(0x400, 0x0);
	usleep_range(1000, 1100);

	if(Is_lvds_RGB666_backlight_RT8561())
		ql_lvds_WriteReg(0x604, 0x3FFFFD08);
	else
		ql_lvds_WriteReg(0x604, 0x3FFFFC00);
	msleep(200);/*after init -3*/

	/* --------- displayon set */
	/*pwm freq.=33.66M /(0x2DAA1+1)= 180hz;  */
	ql_lvds_WriteReg(0x160, 0x2C2DD);

	ql_lvds_WriteReg(0x138, 0x3fff0000);

	/*ql_lvds_WriteReg(0x164, 0x1616F);*//* for 50% duty????*/
	ql_lvds_WriteReg(0x15c, 0x5);/*selelct pwm*/

	msleep(10);/*after init -3*/

#ifdef __RGB_OUT__
	dsim_info("[lcd-test]<delay for RGB out> !!! ================================\n");
	msleep(500);
	msleep(500);
	pr_info("[lcd-test]<making RGB out> !!! ===================================\n");
	ql_lvds_WriteReg(0x70C, 0x5E76);
	ql_lvds_WriteReg(0x710, 0x54D004F);
	ql_lvds_WriteReg(0x134, 0x05);
	mdelay(1);
	ql_lvds_WriteReg(0x154, 0x00000000);
	mdelay(1);
	ql_lvds_WriteReg(0x154, 0x80000000);
	mdelay(1);
	dsim_info("[lcd-test]<ending RGB out> !!! ===================================\n");
#endif

	dsim_info("[lcd] : %s was called--\n", __func__);
	return ret;

}

static int ltm184hl01_displayon(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("[lcd] : %s was called\n", __func__);

	if(panel->lcdConnected == PANEL_DISCONNEDTED)
		return ret;

	msleep(3);

	ret = gpio_request_one(backlight_on, GPIOF_OUT_INIT_HIGH, "BLIC_ON");
	gpio_free(backlight_on);
	usleep_range(10000, 11000);

	lp8558_array_write(backlight1_client, LP8558_eprom_drv_arr_init, ARRAY_SIZE(LP8558_eprom_drv_arr_init));
	lp8558_array_write(backlight2_client, LP8558_eprom_drv_arr_init, ARRAY_SIZE(LP8558_eprom_drv_arr_init));

	lvds_disp_on_comp = 1;

	dsim_info("[lcd] : %s was called--\n", __func__);
	return ret;
}

static int vx5b3d_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;

	dsim_info("[lcd] : %s was called\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[lcd]lvds : need I2C_FUNC_I2C.\n");
		ret = -ENODEV;
		return ret;
	}

	lvds_client = client;

	return ret;
}

static struct i2c_device_id vx5b3d_id[] = {
	{"lvds", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, vx5b3d_id);

static struct of_device_id vx5b3d_i2c_dt_ids[] = {
	{ .compatible = "lvds" },
	{ }
};

MODULE_DEVICE_TABLE(of, vx5b3d_i2c_dt_ids);

static struct i2c_driver vx5b3d_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "lvds_vx5b3d",
		.of_match_table	= of_match_ptr(vx5b3d_i2c_dt_ids),
	},
	.id_table = vx5b3d_id,
	.probe = vx5b3d_probe,
};

static void ltm184hl01_lvds_pwm_set(struct dsim_device *dsim)
{
	struct panel_private *panel = &dsim->priv;

	int vx5b3d_level = 0;
	int level;
	u32 bl_reg;

	if (!lvds_disp_on_comp) {
		dsim_info("[lcd] %s lvds_disp_on_comp needed\n", __func__);
		return;
	}

	dsim_info("[lcd] : %s was called- bl level(%d) autobl(%d) weakness_hbm(%d)\n", __func__
		, panel->bd->props.brightness, panel->auto_brightness, panel->weakness_hbm_comp);

	level = panel->bd->props.brightness;

	if (panel->auto_brightness >= 6 || panel->weakness_hbm_comp == BoostUPbrightness) {
		lp8558_array_write(backlight1_client, LP8558_BOOSTUP, ARRAY_SIZE(LP8558_BOOSTUP));
		lp8558_array_write(backlight2_client, LP8558_BOOSTUP, ARRAY_SIZE(LP8558_BOOSTUP));
	} else {
		lp8558_array_write(backlight1_client, LP8558_NORMAL, ARRAY_SIZE(LP8558_NORMAL));
		lp8558_array_write(backlight2_client, LP8558_NORMAL, ARRAY_SIZE(LP8558_NORMAL));
	}

	/* brightness tuning*/
	if (level > MAX_BRIGHTNESS_LEVEL)
		level = MAX_BRIGHTNESS_LEVEL;

	if (level >= MID_BRIGHTNESS_LEVEL) {
		vx5b3d_level  = (level - MID_BRIGHTNESS_LEVEL) *
		(V5D3BX_MAX_BRIGHTNESS_LEVEL - V5D3BX_MID_BRIGHTNESS_LEVEL) / (MAX_BRIGHTNESS_LEVEL-MID_BRIGHTNESS_LEVEL) + V5D3BX_MID_BRIGHTNESS_LEVEL;
	} else if (level >= DIM_BRIGHTNESS_LEVEL) {
		vx5b3d_level  = (level - DIM_BRIGHTNESS_LEVEL) *
		(V5D3BX_MID_BRIGHTNESS_LEVEL - V5D3BX_DIM_BRIGHTNESS_LEVEL) / (MID_BRIGHTNESS_LEVEL-DIM_BRIGHTNESS_LEVEL) + V5D3BX_DIM_BRIGHTNESS_LEVEL;
	} else if (level >= LOW_BRIGHTNESS_LEVEL) {
		vx5b3d_level  = (level - LOW_BRIGHTNESS_LEVEL) *
		(V5D3BX_DIM_BRIGHTNESS_LEVEL - V5D3BX_LOW_BRIGHTNESS_LEVEL) / (DIM_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) + V5D3BX_LOW_BRIGHTNESS_LEVEL;
	} else if (level > 0)
		vx5b3d_level  = V5D3BX_MIN_BRIGHTNESS;
	else
		vx5b3d_level = 0;

        bl_reg = (vx5b3d_level*V5D3BX_180HZ_DEFAULT_RATIO)/MAX_BRIGHTNESS_LEVEL;
	ql_lvds_WriteReg(0x164, bl_reg);

	dsim_info("[lcd]: brightness level(%d) vx5b3d_level(%d) bl_reg(%d) \n", level, vx5b3d_level, bl_reg);
	return;
}

#if defined(CONFIG_BLIC_TUNING)

static ssize_t backlight_i2c_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	sprintf(buf, "BL addr 0x%x : val 0x%x\n",
		LP8558_eprom_drv_arr_normal[0].addr,
		LP8558_eprom_drv_arr_normal[0].val);

	return strlen(buf);
}

static ssize_t backlight_i2c_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int reg, value;
	int ret;
	struct panel_private *panel = &dsim_t->priv;

	ret = sscanf(buf, "%x %x", &reg, &value);

	LP8558_eprom_drv_arr_normal[0].addr = reg;
	LP8558_eprom_drv_arr_normal[0].val = value;

	ltm184hl01_lvds_pwm_set(dsim_t);

	return size;
}

static DEVICE_ATTR(bl_ic_tune, 0664, backlight_i2c_show , backlight_i2c_store);

#endif

static int lp8558_1_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
#if defined(CONFIG_BLIC_TUNING)
	struct class *backlight_class;
	struct device *backlight_dev;
#endif

	dsim_info("[backlight] : %s was called\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[lp8558_1] : need I2C_FUNC_I2C.\n");
		ret = -ENODEV;
		return ret;
	}

	backlight1_client = client;
	backlight_on = of_get_gpio(client->dev.of_node, 0);

	of_property_read_u32(client->dev.of_node, "rev", &board_rev);
	dsim_info("[backlight] : board rev (%d)\n", board_rev);

#if defined(CONFIG_BLIC_TUNING)
	backlight_class = class_create(THIS_MODULE, "bl-dbg");
	backlight_dev = device_create(backlight_class, NULL, 0, NULL,  "ic-tuning");
	if (IS_ERR(backlight_dev))
		dsim_info("[backlight] :Failed to create device(backlight_dev)!\n");
	else {
		if (device_create_file(backlight_dev, &dev_attr_bl_ic_tune) < 0)
			dsim_info("[backlight] :Failed to create device file(%s)!\n", dev_attr_bl_ic_tune.attr.name);
	}
#endif

	return ret;
}

static struct i2c_device_id lp8558_1_id[] = {
	{"lp8558_1", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lp8558_1_id);

static struct of_device_id lp8558_1_i2c_dt_ids[] = {
	{ .compatible = "lp8558_1" },
	{ }
};

MODULE_DEVICE_TABLE(of, lp8558_1_i2c_dt_ids);

static struct i2c_driver lp8558_1_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "bl_lp8558_1",
		.of_match_table	= of_match_ptr(lp8558_1_i2c_dt_ids),
	},
	.id_table = lp8558_1_id,
	.probe = lp8558_1_probe,
};

static int lp8558_2_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;

	dsim_info("[backlight] : %s was called\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[lp8558_2] : need I2C_FUNC_I2C.\n");
		ret = -ENODEV;
		return ret;
	}

	backlight2_client = client;

	return ret;
}

static struct i2c_device_id lp8558_2_id[] = {
	{"lp8558_2", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, lp8558_2_id);

static struct of_device_id lp8558_2_i2c_dt_ids[] = {
	{ .compatible = "lp8558_2" },
	{ }
};

MODULE_DEVICE_TABLE(of, lp8558_2_i2c_dt_ids);

static struct i2c_driver lp8558_2_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "bl_lp8558_2",
		.of_match_table	= of_match_ptr(lp8558_2_i2c_dt_ids),
	},
	.id_table = lp8558_2_id,
	.probe = lp8558_2_probe,
};

static int ltm184hl01_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("[lcd] : %s was called new 0x%x %d\n", __func__ ,  lcdtype, board_rev);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;

	if (lcdtype == 0x0)
		panel->lcdConnected = PANEL_DISCONNEDTED;

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("[lcd] : %s lcd was not connected\n", __func__);

		gpio_request_one(backlight_on, GPIOF_OUT_INIT_LOW, "BLIC_ON");
		gpio_free(backlight_on);
		usleep_range(10000, 11000);

		goto probe_exit;
	}

	dsim->priv.id[0] = lcdtype;
	lvds_disp_on_comp = 1; /* in the first boot, lvds was initialized on bootloader already.*/

probe_exit:
	return ret;
}

static int ltm184hl01_early_probe(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("[lcd] : %s was called\n", __func__);

#if defined(CONFIG_BLIC_TUNING)
	dsim_t = dsim;
#endif
	if (!lvds_client) {
		ret = i2c_add_driver(&vx5b3d_i2c_driver);
		if (ret)
			pr_err("[lcd] vx5b3dx_i2c_init registration failed. ret= %d\n", ret);
	}

	if (!backlight1_client) {
		ret = i2c_add_driver(&lp8558_1_i2c_driver);
		if (ret)
			pr_err("[backlight] lp8558_1_i2c_init registration failed. ret= %d\n", ret);
	}

	if (!backlight2_client) {
		ret = i2c_add_driver(&lp8558_2_i2c_driver);
		if (ret)
			pr_err("[backlight ]lp8558_2_i2c_init registration failed. ret= %d\n", ret);
	}

	return ret;
}


struct dsim_panel_ops ltm184hl01_panel_ops = {
	.early_probe = ltm184hl01_early_probe,
	.probe		= ltm184hl01_probe,
	.displayon	= ltm184hl01_displayon,
	.exit		= ltm184hl01_exit,
	.init		= NULL,
	.lvds_init	= ltm184hl01_lvds_init,
	.lvds_pwm_set	= ltm184hl01_lvds_pwm_set,
};


static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "SDC_%02X%02X%02X\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%x %x %x\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *panel = dev_get_drvdata(dev);

	char *pos = buf;
	int nit, i;

	if (panel->br_tbl == NULL)
		return strlen(buf);

	for (i = 0; i <= UI_MAX_BRIGHTNESS; i++) {
		nit = panel->br_tbl[i];
		pos += sprintf(pos, "%3d %3d\n", i, nit);
	}
	return pos - buf;
}

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->auto_brightness);

	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (priv->auto_brightness != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->auto_brightness, value);
			mutex_lock(&priv->lock);
			priv->auto_brightness = value;
			mutex_unlock(&priv->lock);
			dsim_panel_set_brightness(dsim, 0);
		}
	}
	return size;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->siop_enable);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (priv->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->siop_enable, value);
			mutex_lock(&priv->lock);
			priv->siop_enable = value;
			mutex_unlock(&priv->lock);

			dsim_panel_set_brightness(dsim, 1);
		}
	}
	return size;
}

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->acl_enable);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);
	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);

	if (rc < 0)
		return rc;
	else {
		if (priv->acl_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->acl_enable, value);
			mutex_lock(&priv->lock);
			priv->acl_enable = value;
			mutex_unlock(&priv->lock);
			dsim_panel_set_brightness(dsim, 1);
		}
	}
	return size;
}


static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-20, -19, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		mutex_lock(&priv->lock);
		priv->temperature = value;
		mutex_unlock(&priv->lock);

		dsim_panel_set_brightness(dsim, 1);
		dev_info(dev, "%s: %d, %d\n", __func__, value, priv->temperature);
	}

	return size;
}
#ifdef _BRIGHTNESS_BOOSTUP_
static ssize_t weakness_hbm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", priv->weakness_hbm_comp);

	return strlen(buf);
}

static ssize_t weakness_hbm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;
	else {
		if (priv->weakness_hbm_comp != value){
			dev_info(dev, "%s: %d, %d\n", __func__, priv->weakness_hbm_comp, value);
			if((value == 1) || (value == 2)) {
				pr_info("%s don't support 1,2\n", __func__);
				return size;
			}
			priv->weakness_hbm_comp = value;
			dsim_panel_set_brightness(dsim, 0);
		}
	}
	return size;
}
#endif
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(auto_brightness, 0644, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
#ifdef _BRIGHTNESS_BOOSTUP_
static DEVICE_ATTR(weakness_hbm_comp, 0664, weakness_hbm_show, weakness_hbm_store);
#endif

static void lcd_init_sysfs(struct dsim_device *dsim)
{
	int ret = -1;

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_window_type);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_brightness_table);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->priv.bd->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		dev_err(&dsim->priv.bd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_siop_enable);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_temperature);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

#ifdef _BRIGHTNESS_BOOSTUP_
	ret = device_create_file(&dsim->priv.bd->dev, &dev_attr_weakness_hbm_comp);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif
}



static int dsim_panel_early_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	panel->ops = dsim_panel_get_priv_ops(dsim);

	if (panel->ops->early_probe)
		ret = panel->ops->early_probe(dsim);
	return ret;
}

static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim->lcd = lcd_device_register("panel", dsim->dev, &dsim->priv, NULL);
	if (IS_ERR(dsim->lcd)) {
		dsim_err("%s : faield to register lcd device\n", __func__);
		ret = PTR_ERR(dsim->lcd);
		goto probe_err;
	}
	ret = dsim_backlight_probe(dsim);
	if (ret) {
		dsim_err("%s : failed to prbe backlight driver\n", __func__);
		goto probe_err;
	}

	panel->lcdConnected = PANEL_CONNECTED;
	panel->state = PANEL_STATE_RESUMED;
	panel->temperature = NORMAL_TEMPERATURE;
	panel->acl_enable = 0;
	panel->current_acl = 0;
	panel->auto_brightness = 0;
	panel->siop_enable = 0;
	panel->current_hbm = 0;
	panel->current_vint = 0;
	mutex_init(&panel->lock);

	if (panel->ops->probe) {
		ret = panel->ops->probe(dsim);
		if (ret) {
			dsim_err("%s : failed to probe panel\n", __func__);
			goto probe_err;
		}
	}

#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
	lcd_init_sysfs(dsim);
#endif
	dsim_info("%s probe done\n", __FILE__);
probe_err:
	return ret;
}

static int dsim_panel_lvds_init(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("%s : lvds init, panel state (%d)\n", __func__, panel->state);

	if (panel->state == PANEL_STATE_SUSPENED) {
		panel->state = PANEL_STATE_RESUMED;
		if (panel->ops->lvds_init) {
			ret = panel->ops->lvds_init(dsim);
			if (ret) {
				dsim_err("%s : failed to lvds init\n", __func__);
				panel->state = PANEL_STATE_SUSPENED;
				goto lvds_init_err;
			}
		}
	}

lvds_init_err:

	return ret;
}

static int dsim_panel_displayon(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->state == PANEL_STATE_SUSPENED) {
		panel->state = PANEL_STATE_RESUMED;
		if (panel->ops->init) {
			ret = panel->ops->init(dsim);
			if (ret) {
				dsim_err("%s : failed to panel init\n", __func__);
				panel->state = PANEL_STATE_SUSPENED;
				goto displayon_err;
			}
		}
	}

	dsim_panel_set_brightness(dsim, 1);

	if (panel->ops->displayon) {
		ret = panel->ops->displayon(dsim);
		if (ret) {
			dsim_err("%s : failed to panel display on\n", __func__);
			goto displayon_err;
		}
	}

displayon_err:
	return ret;
}

static int dsim_panel_suspend(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->state == PANEL_STATE_SUSPENED)
		goto suspend_err;

	panel->state = PANEL_STATE_SUSPENDING;

	if (panel->ops->exit) {
		ret = panel->ops->exit(dsim);
		if (ret) {
			dsim_err("%s : failed to panel exit\n", __func__);
			goto suspend_err;
		}
	}
	panel->state = PANEL_STATE_SUSPENED;

suspend_err:
	return ret;
}

static int dsim_panel_resume(struct dsim_device *dsim)
{
	int ret = 0;
	return ret;
}


struct mipi_dsim_lcd_driver ltm184hl01_mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
	.display_lvds_init = dsim_panel_lvds_init,
};

