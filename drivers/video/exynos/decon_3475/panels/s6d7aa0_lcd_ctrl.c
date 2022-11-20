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

	if ( dsim->rev >= 0x1 )
		sky82896_array_write(LP8557_eprom_drv_arr_off, ARRAY_SIZE(LP8557_eprom_drv_arr_off));

exit_err:
	return ret;
}

static int s6d7aa0_init(struct dsim_device *dsim)
{
	int i, ret;

	dsim_info("MDD : %s was called\n", __func__);

	msleep(100);

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

	ret = dsim_read_hl_data(dsim, S6D7AA0_ID_REG, S6D7AA0_ID_LEN, dsim->priv.id);
	if (ret != S6D7AA0_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
	}

	dsim_info("READ ID : ");
	for(i = 0; i < S6D7AA0_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	if (dsim->priv.id[0] == 0x5E) { /* bring up panel */

		dsim_info("bring up panel...\n");

		ret = dsim_write_hl_data(dsim, SEQ_INIT_1_OLD, ARRAY_SIZE(SEQ_INIT_1_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_1\n", __func__);
			goto init_exit;
		}

		msleep(1);

		ret = dsim_write_hl_data(dsim, SEQ_INIT_2_OLD, ARRAY_SIZE(SEQ_INIT_2_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_2\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_3_OLD, ARRAY_SIZE(SEQ_INIT_3_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_3\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_4_OLD, ARRAY_SIZE(SEQ_INIT_4_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_4\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_5_OLD, ARRAY_SIZE(SEQ_INIT_5_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_5\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_6_OLD, ARRAY_SIZE(SEQ_INIT_6_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_6\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_7_OLD, ARRAY_SIZE(SEQ_INIT_7_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_7\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_8_OLD, ARRAY_SIZE(SEQ_INIT_8_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_8\n", __func__);
			goto init_exit;
		}

		msleep(5);

		ret = dsim_write_hl_data(dsim, SEQ_INIT_9_OLD, ARRAY_SIZE(SEQ_INIT_9_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_9\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}

		msleep(120);

		ret = dsim_write_hl_data(dsim, SEQ_INIT_10_OLD, ARRAY_SIZE(SEQ_INIT_10_OLD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_9\n", __func__);
			goto init_exit;
		}

	} else { /* real_panel */

		dsim_info("real panel...\n");

		ret = dsim_write_hl_data(dsim, SEQ_INIT_1, ARRAY_SIZE(SEQ_INIT_1));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_1\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_2, ARRAY_SIZE(SEQ_INIT_2));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_2\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_3, ARRAY_SIZE(SEQ_INIT_3));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_3\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_4, ARRAY_SIZE(SEQ_INIT_4));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_4\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_5, ARRAY_SIZE(SEQ_INIT_5));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_5\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_6, ARRAY_SIZE(SEQ_INIT_6));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_6\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_7, ARRAY_SIZE(SEQ_INIT_7));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_7\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_8, ARRAY_SIZE(SEQ_INIT_8));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_8\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_9, ARRAY_SIZE(SEQ_INIT_9));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_9\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_10, ARRAY_SIZE(SEQ_INIT_10));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_9\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_11, ARRAY_SIZE(SEQ_INIT_11));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_11\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_12, ARRAY_SIZE(SEQ_INIT_12));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_12\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_13, ARRAY_SIZE(SEQ_INIT_13));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_13\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_14, ARRAY_SIZE(SEQ_INIT_14));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_14\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_15, ARRAY_SIZE(SEQ_INIT_15));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_15\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_16, ARRAY_SIZE(SEQ_INIT_16));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_16\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_17, ARRAY_SIZE(SEQ_INIT_17));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_17\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_18, ARRAY_SIZE(SEQ_INIT_18));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_18\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_19, ARRAY_SIZE(SEQ_INIT_19));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_19\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_20, ARRAY_SIZE(SEQ_INIT_20));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_20\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_21, ARRAY_SIZE(SEQ_INIT_21));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_21\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_22, ARRAY_SIZE(SEQ_INIT_22));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_22\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_23, ARRAY_SIZE(SEQ_INIT_23));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_23\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_24, ARRAY_SIZE(SEQ_INIT_24));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_24\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_25, ARRAY_SIZE(SEQ_INIT_25));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_25\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_26, ARRAY_SIZE(SEQ_INIT_26));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_26\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_27, ARRAY_SIZE(SEQ_INIT_27));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_27\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_28, ARRAY_SIZE(SEQ_INIT_28));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_28\n", __func__);
			goto init_exit;
		}

		ret = dsim_write_hl_data(dsim, SEQ_INIT_29, ARRAY_SIZE(SEQ_INIT_29));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_INIT_29\n", __func__);
			goto init_exit;
		}

		msleep(10);

		ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}

		msleep(120);
	}
 	/* Init backlight i2c */

	ret = gpio_request_one(backlight_reset, GPIOF_OUT_INIT_HIGH, "BL_reset");
	gpio_free(backlight_reset);
	usleep_range(10000, 11000);

	dsim_info(" %s : %d \n ", __func__ , dsim->rev );

	if ( dsim->rev >= 0x1 ) {
		sky82896_array_write(LP8557_eprom_drv_arr, ARRAY_SIZE(LP8557_eprom_drv_arr));
	} else {
		sky82896_array_write(SKY82896_eprom_drv_arr, ARRAY_SIZE(SKY82896_eprom_drv_arr));
	}
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
