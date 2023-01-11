/*
 *  this based on linux/arch/arm/mach-mmp/mmpx-dt.c
 *
 *  Copyright (C) 2014 Marvell Technology Group Ltd.
 *  Author: Owen Zhang <xinzha@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  publishhed by the Free Software Foundation.
 */

#include <linux/of_gpio.h>
#include "soc_camera_xcover3lte.h"
#include <linux/clk.h>

/* this is just define for more soc cameras */
static struct regulator *avdd_2v8;
static struct regulator *dovdd_1v8;
static struct clk *mclk;
#ifdef CONFIG_SOC_CAMERA_SR200
static int sr200_sensor_power(struct device *dev, int on)
{
	int cam_enable, cam_reset;
	int ret;
	printk("sr200 power on enter\n");
	/* Get the regulators and never put it */
	/*
	 * The regulators is for sensor and should be in sensor driver
	 * As SoC camera does not support device tree, adding code here
	 */

	mclk = devm_clk_get(dev, "SC2MCLK");
	if (IS_ERR(mclk))
		return PTR_ERR(mclk);

	if (!avdd_2v8) {
		avdd_2v8 = regulator_get(dev, "avdd_2v8");
		if (IS_ERR(avdd_2v8)) {
			dev_warn(dev, "Failed to get regulator avdd_2v8\n");
			avdd_2v8 = NULL;
		}
	}

	if (!dovdd_1v8) {
		dovdd_1v8 = regulator_get(dev, "dovdd_1v8");
		if (IS_ERR(dovdd_1v8)) {
			dev_warn(dev, "Failed to get regulator dovdd_1v8\n");
			dovdd_1v8 = NULL;
		}
	}

	cam_enable = of_get_named_gpio(dev->of_node, "pwdn-gpios", 0);
	if (gpio_is_valid(cam_enable)) {
		if (gpio_request(cam_enable, "CAM2_POWER")) {
			dev_err(dev, "Request GPIO %d failed\n", cam_enable);
			goto cam_enable_failed;
		}
	} else {
		dev_err(dev, "invalid pwdn gpio %d\n", cam_enable);
		goto cam_enable_failed;
	}

	cam_reset = of_get_named_gpio(dev->of_node, "reset-gpios", 0);
	if (gpio_is_valid(cam_reset)) {
		if (gpio_request(cam_reset, "CAM2_RESET")) {
			dev_err(dev, "Request GPIO %d failed\n", cam_reset);
			goto cam_reset_failed;
		}
	} else {
		dev_err(dev, "invalid pwdn gpio %d\n", cam_reset);
		goto cam_reset_failed;
	}

	if (on) {

		gpio_direction_output(cam_enable, 0);
		usleep_range(2000, 5000);
		gpio_direction_output(cam_reset, 0);

		if (dovdd_1v8) {
			regulator_set_voltage(dovdd_1v8, 1800000, 1800000);
			ret = regulator_enable(dovdd_1v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}
		usleep_range(2000, 5000);
		if (avdd_2v8) {
			regulator_set_voltage(avdd_2v8, 2800000, 2800000);
			ret = regulator_enable(avdd_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		usleep_range(3000, 5000);
		gpio_direction_output(cam_enable, 1);
		usleep_range(3000, 5000);
		clk_set_rate(mclk, 26000000); 
		clk_prepare_enable(mclk);
		msleep(30);
		gpio_direction_output(cam_reset, 1);
	} 

	else {
		/*
		 * Keep PWDN always on as defined in spec
		 * gpio_direction_output(cam_enable, 0);
		 * usleep_range(5000, 20000);
		 */

		gpio_direction_output(cam_reset, 0);
		usleep_range(5000, 20000);
		
		clk_disable_unprepare(mclk);
		devm_clk_put(dev, mclk);
		
		gpio_direction_output(cam_enable, 0);
		usleep_range(5000, 20000);
		if (avdd_2v8)
			regulator_disable(avdd_2v8);
		if (dovdd_1v8)
			regulator_disable(dovdd_1v8);
	}

	gpio_free(cam_enable);
	gpio_free(cam_reset);
	return 0;

cam_reset_failed:
	devm_clk_put(dev, mclk);
	gpio_free(cam_enable);

cam_enable_failed:
	ret = -EIO;

cam_regulator_enable_failed:
	if (dovdd_1v8)
		regulator_put(dovdd_1v8);
	dovdd_1v8 = NULL;

	if (avdd_2v8)
		regulator_put(avdd_2v8);
	avdd_2v8 = NULL;

	return ret;
}

static struct sensor_board_data sr200_data = {
	.mount_pos	= SENSOR_USED | SENSOR_POS_FRONT | SENSOR_RES_LOW,
	.bus_type	= V4L2_MBUS_CSI2,
	.bus_flag	= V4L2_MBUS_CSI2_1_LANE,
	.flags  = V4L2_MBUS_CSI2_1_LANE,
	.dphy = {0x1806, 0x00011, 0xe00},
};

static struct i2c_board_info dkb_i2c_sr200 = {
		I2C_BOARD_INFO("sr200", 0x20),
};

struct soc_camera_desc soc_camera_desc_0 = {
	.subdev_desc = {
		.power          = sr200_sensor_power,
		.drv_priv		= &sr200_data,
		.flags		= 0,
	},
	.host_desc = {
		.bus_id = SR200_CCIC_PORT,	/* Default as ccic2 */
		.i2c_adapter_id = 0,
		.board_info     = &dkb_i2c_sr200,
		.module_name    = "sr200",
	},
};
#endif


