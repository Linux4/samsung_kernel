/*
 * drivers/video/decon_7580/panels/s6d7aa0_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <video/mipi_display.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "../dsim.h"

#include "panel_info.h"

struct i2c_client *backlight_client;
int	backlight_reset;

static int s6d7aa0_read_init_info(struct dsim_device *dsim)
{
	int i = 0;
	int ret;
	struct panel_private *panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
	}

	// id
	ret = dsim_read_hl_data(dsim, S6D7AA0_ID_REG, S6D7AA0_ID_LEN, dsim->priv.id);
	if (ret != S6D7AA0_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNEDTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for(i = 0; i < S6D7AA0_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto read_fail;
	}
read_exit:
	return 0;

read_fail:
	return -ENODEV;

}

static int s6d7aa0_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_ON\n", __func__);
 		goto displayon_err;
	}

	msleep(12);

displayon_err:
	return ret;

}

static int s6d7aa0_exit(struct dsim_device *dsim)
{
	int ret = 0;
	dsim_info("MDD : %s was called\n", __func__);
	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : DISPLAY_OFF\n", __func__);
		goto exit_err;
	}

	msleep(10);

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SLEEP_IN\n", __func__);
		goto exit_err;
	}

	msleep(150);

exit_err:
	return ret;
}

int sky82896_array_write(const struct SKY82896_rom_data * eprom_ptr, int eprom_size) {
	int i = 0;
	int ret = 0;
	for (i = 0; i < eprom_size; i++) {
		ret = i2c_smbus_write_byte_data(backlight_client, eprom_ptr[i].addr, eprom_ptr[i].val);
		if (ret < 0)
			dsim_err(":%s error : BL DEVICE_CTRL setting fail\n", __func__);
	}

	return 0;
}

static int s6d7aa0_init(struct dsim_device *dsim)
{
	int i, ret;

	dsim_info("MDD : %s was called\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_PASSWD1, ARRAY_SIZE(SEQ_PASSWD1));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PASSWD1\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PASSWD2, ARRAY_SIZE(SEQ_PASSWD2));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PASSWD2\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PASSWD3, ARRAY_SIZE(SEQ_PASSWD3));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PASSWD3\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_OTP_RELOAD, ARRAY_SIZE(SEQ_OTP_RELOAD));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_OTP_RELOAD\n", __func__);
		goto init_exit;
	}


	ret = dsim_read_hl_data(dsim, S6D7AA0_ID_REG, S6D7AA0_ID_LEN, dsim->priv.id);
	if (ret != S6D7AA0_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
	}


	dsim_info("READ ID : ");
	for(i = 0; i < S6D7AA0_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, SEQ_BL_ON_CTL, ARRAY_SIZE(SEQ_BL_ON_CTL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_BL_ON_CTL\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_BC_PARAM_MDNIE, ARRAY_SIZE(SEQ_BC_PARAM_MDNIE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_BC_PARAM_MDNIE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_FD_PARAM_MDNIE, ARRAY_SIZE(SEQ_FD_PARAM_MDNIE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_FD_PARAM_MDNIE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_FE_PARAM_MDNIE, ARRAY_SIZE(SEQ_FE_PARAM_MDNIE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_FE_PARAM_MDNIE\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, SEQ_B3_PARAM, ARRAY_SIZE(SEQ_B3_PARAM));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_B3_PARAM\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_BL_CTL, ARRAY_SIZE(SEQ_BL_CTL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_BACKLIGHT_CTL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PORCH_CTL, ARRAY_SIZE(SEQ_PORCH_CTL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_PORCH_CTL\n", __func__);
		goto init_exit;
	}

	msleep(12);

	ret = dsim_write_hl_data(dsim, SEQ_CABC_CTL, ARRAY_SIZE(SEQ_CABC_CTL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_CABC_CTL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_CD_ON, ARRAY_SIZE(SEQ_CD_ON));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_CD_CTL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_CE_CTL, ARRAY_SIZE(SEQ_CE_CTL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_CE_CTL\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_BC_MODE, ARRAY_SIZE(SEQ_BC_MODE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_BC_MODE\n", __func__);
		goto init_exit;
	}

	msleep(12);

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}

	msleep(120);

	ret = dsim_write_hl_data(dsim, SEQ_TEON_CTL, ARRAY_SIZE(SEQ_TEON_CTL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TE_OUT\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PASSWD3_LOCK, ARRAY_SIZE(SEQ_PASSWD3_LOCK));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PASSWD3_LOCK\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PASSWD2_LOCK, ARRAY_SIZE(SEQ_PASSWD2_LOCK));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PASSWD2_LOCK\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_PASSWD1_LOCK, ARRAY_SIZE(SEQ_PASSWD1_LOCK));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PASSWD1_LOCK\n", __func__);
		goto init_exit;
	}

 	/* Init backlight i2c */

	ret = gpio_request_one(backlight_reset, GPIOF_OUT_INIT_HIGH, "BL_reset");
	gpio_free(backlight_reset);
	usleep_range(10000, 11000);

	sky82896_array_write(SKY82896_eprom_drv_arr, ARRAY_SIZE(SKY82896_eprom_drv_arr));

init_exit:
	return ret;
}


static int sky82896_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "led : need I2C_FUNC_I2C.\n");
		ret = -ENODEV;
		goto err_i2c;
	}

	backlight_client = client;

	backlight_reset = of_get_gpio(client->dev.of_node, 0);

err_i2c:
	return ret;
}

static struct i2c_device_id sky82896_id[] = {
	{"sky82896", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, sky82896_id);

static struct of_device_id sky82896_i2c_dt_ids[] = {
	{ .compatible = "sky82896,i2c" },
	{ }
};

MODULE_DEVICE_TABLE(of, sky82896_i2c_dt_ids);

static struct i2c_driver sky82896_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sky82896",
		.of_match_table	= of_match_ptr(sky82896_i2c_dt_ids),
	},
	.id_table = sky82896_id,
	.probe = sky82896_probe,
};

static int s6d7aa0_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("MDD : %s was called\n", __func__);

	panel->dim_data = (void *)NULL;
	panel->lcdConnected = PANEL_CONNECTED;
	panel->panel_type = 0;

	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;

	}

	s6d7aa0_read_init_info(dsim);
	if (panel->lcdConnected == PANEL_DISCONNEDTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}

	i2c_add_driver(&sky82896_i2c_driver);

probe_exit:
	return ret;
}

struct dsim_panel_ops s6d7aa0_panel_ops = {
	.early_probe = NULL,
	.probe		= s6d7aa0_probe,
	.displayon	= s6d7aa0_displayon,
	.exit		= s6d7aa0_exit,
	.init		= s6d7aa0_init,
};

struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	return &s6d7aa0_panel_ops;
}
