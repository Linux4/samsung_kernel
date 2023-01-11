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
#include "soc_camera_j1ace.h"
#include <linux/clk.h>

/* this is just define for more soc cameras */
static struct regulator *avdd_2v8;
static struct regulator *af_2v8;
static struct regulator *dvdd_1v2;
static struct clk *mclk;
#ifdef CONFIG_SOC_CAMERA_DB8221A
static int db8221a_sensor_power(struct device *dev, int on)
{
	int cam_io, cam_enable, cam_reset;
	int ret;

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

	if (!dvdd_1v2) {
		dvdd_1v2 = regulator_get(dev, "dvdd_1v2");
		if (IS_ERR(dvdd_1v2)) {
			dev_warn(dev, "Failed to get regulator dvdd_1v2\n");
			dvdd_1v2 = NULL;
		}
	}

	cam_io = of_get_named_gpio(dev->of_node, "dovdd_1v8-gpios", 0);
	if (gpio_is_valid(cam_io)) {
		if (gpio_request(cam_io, "CAM2_IO")) {
			dev_err(dev, "Request GPIO %d failed\n", cam_io);
			goto cam_io_failed;
		}
	} else {
		dev_err(dev, "invalid dovdd_1v8 gpio %d\n", cam_io);
		goto cam_io_failed;
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
		printk("[DHL] DB8221A Power On..!!\n");
		gpio_direction_output(cam_enable, 0);
		gpio_direction_output(cam_reset, 0);
		msleep(1);

		/** AVDD Voltage : 2.8V On **/
		if (avdd_2v8) {
			regulator_set_voltage(avdd_2v8, 2800000, 2800000);
			ret = regulator_enable(avdd_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}
		msleep(10);

		/** IO Voltage 1.8V On **/
		gpio_direction_output(cam_io, 1);
		msleep(1);

		/** 5M Core Voltage 1.2V On **/
		if (dvdd_1v2) {
			regulator_set_voltage(dvdd_1v2, 1250000, 1250000);
			ret = regulator_enable(dvdd_1v2);
			if (ret){
				printk("Debug, dvdd is error and return cause no mclk\n");
				goto cam_regulator_enable_failed;
			}
		}
		msleep(2);

		/** 5M Core Voltage 1.2V Off **/		
		if (dvdd_1v2)
			regulator_disable(dvdd_1v2);	
		msleep(1);

		/** MCLK 26MHz On **/
		clk_set_rate(mclk, 26000000); 
		clk_prepare_enable(mclk);
		msleep(1);

		/** 2M Enable Pin On**/
		gpio_direction_output(cam_enable, 1);
		msleep(1);

		/** 2M Reset Pin On**/
		gpio_direction_output(cam_reset, 1);
		msleep(5);
	} else {
		/*
		 * Keep PWDN always on as defined in spec
		 * gpio_direction_output(cam_enable, 0);
		 * usleep_range(5000, 20000);
		 */
		msleep(5);
		
		/** 2M Reset Pin Off**/
		gpio_direction_output(cam_reset, 0);
		msleep(1);

		/** 2M Enable Pin Off**/
		gpio_direction_output(cam_enable, 0);
		msleep(1);
	
		/** MCLK 26MHz Off **/	
		clk_disable_unprepare(mclk);
		devm_clk_put(dev, mclk);

		/** AVDD Voltage : 2.8V Off **/		
		if (avdd_2v8)
			regulator_disable(avdd_2v8);
		msleep(1);

		/** IO Voltage 1.8V Off **/		
		gpio_direction_output(cam_io, 0);
	}

	gpio_free(cam_io);
	gpio_free(cam_enable);
	gpio_free(cam_reset);

	return 0;

cam_io_failed:
	gpio_free(cam_io);
cam_reset_failed:
	devm_clk_put(dev, mclk);
	gpio_free(cam_reset);
cam_enable_failed:
	gpio_free(cam_enable);
	ret = -EIO;
cam_regulator_enable_failed:
	if (avdd_2v8)
		regulator_put(avdd_2v8);
	avdd_2v8 = NULL;
	
	if (dvdd_1v2)
		regulator_put(dvdd_1v2);
	dvdd_1v2 = NULL;

	return ret;
}

static struct sensor_board_data db8221a_data = {
	.mount_pos	= SENSOR_USED | SENSOR_POS_FRONT | SENSOR_RES_LOW,
	.bus_type	= V4L2_MBUS_CSI2,
	.bus_flag	= V4L2_MBUS_CSI2_1_LANE,
	.flags  = V4L2_MBUS_CSI2_1_LANE,
	.dphy = {0x200a, 0x33, 0x1000},
};

static struct i2c_board_info dkb_i2c_db8221a = {
		I2C_BOARD_INFO("db8221a", 0x8A >> 1),
};

struct soc_camera_desc soc_camera_desc_1 = {
	.subdev_desc = {
		.power          = db8221a_sensor_power,
		.drv_priv		= &db8221a_data,
		.flags		= 0,
	},
	.host_desc = {
		.bus_id = DB8221A_CCIC_PORT,	/* Default as ccic2 */
		.i2c_adapter_id = 0,
		.board_info     = &dkb_i2c_db8221a,
		.module_name    = "db8221a",
	},
};
#endif

#ifdef CONFIG_SOC_CAMERA_S5K4ECGX
static int s5k4ecgx_sensor_power(struct device *dev, int on)
{
	int cam_io, cam_enable, cam_reset;
	int ret;

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

	cam_io = of_get_named_gpio(dev->of_node, "dovdd_1v8-gpios", 0);
	if (gpio_is_valid(cam_io)) {
		if (gpio_request(cam_io, "CAM2_IO")) {
			dev_err(dev, "Request GPIO %d failed\n", cam_io);
			goto cam_io_failed;
		}
	} else {
		dev_err(dev, "invalid dovdd_1v8 gpio %d\n", cam_io);
		goto cam_io_failed;
	}

	if (!af_2v8) {
		af_2v8 = regulator_get(dev, "af_2v8");
		if (IS_ERR(af_2v8)) {
			dev_warn(dev, "Failed to get regulator af_2v8\n");
			af_2v8 = NULL;
		}
	}

	if (!dvdd_1v2) {
		dvdd_1v2 = regulator_get(dev, "dvdd_1v2");
		if (IS_ERR(dvdd_1v2)) {
			dev_warn(dev, "Failed to get regulator dvdd_1v2\n");
			dvdd_1v2 = NULL;
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
		printk("[DHL] S5K4ECGX Power On..!!\n");
		gpio_direction_output(cam_enable, 0);
		gpio_direction_output(cam_reset, 0);
		msleep(1);

		/** AVDD Voltage : 2.8V On **/
		if (avdd_2v8) {
			regulator_set_voltage(avdd_2v8, 2800000, 2800000);
			ret = regulator_enable(avdd_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		/** IO Voltage 1.8V On **/
		gpio_direction_output(cam_io, 1);

		msleep(1);

		/** Auto Focus Voltage : 2.8V On **/
		if (af_2v8) {
			regulator_set_voltage(af_2v8, 2800000, 2800000);
			ret = regulator_enable(af_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}
		msleep(5);
		
		/** MCLK 26MHz On **/
		clk_set_rate(mclk, 26000000); 
		clk_prepare_enable(mclk);
		msleep(1);
		
		/** 5M Core Voltage 1.2V On **/
		if (dvdd_1v2) {
			regulator_set_voltage(dvdd_1v2, 1250000, 1250000);
			ret = regulator_enable(dvdd_1v2);
			if (ret){
				printk("Debug, dvdd is error and return cause no mclk\n");
				goto cam_regulator_enable_failed;
			}
		}
		msleep(1);
		
		/** 5M STBY Pin On**/
		gpio_direction_output(cam_enable, 1);
		msleep(1);

		/** 5M RESET Pin On**/
		gpio_direction_output(cam_reset, 1);
		msleep(5);		
	} else {
		/*
		 * Keep PWDN always on as defined in spec
		 * gpio_direction_output(cam_enable, 0);
		 * usleep_range(5000, 20000);
		 */
		 
		/** 5M RESET Pin Off**/
		gpio_direction_output(cam_reset, 0);
		msleep(1);
		
		/** MCLK 26MHz Off **/
		clk_disable_unprepare(mclk);
		devm_clk_put(dev, mclk);
		msleep(1);
		
		/** 5M STBY Pin Off**/
		gpio_direction_output(cam_enable, 0);
		msleep(1);

		/** IO Voltage 1.8V Off **/
		gpio_direction_output(cam_io, 0);

		/** AVDD Voltage : 2.8V Off **/
		if (avdd_2v8)
			regulator_disable(avdd_2v8);
		msleep(1);

		/** 5M Core Voltage 1.2V Off **/
		if (dvdd_1v2)
			regulator_disable(dvdd_1v2);
		msleep(1);

		/** Auto Focus Voltage : 2.8V Off **/
		if (af_2v8)
			regulator_disable(af_2v8);	

	}

	gpio_free(cam_io);
	gpio_free(cam_enable);
	gpio_free(cam_reset);

	return 0;

cam_io_failed:
	gpio_free(cam_io);
cam_reset_failed:
	devm_clk_put(dev, mclk);
	gpio_free(cam_reset);
cam_enable_failed:
	gpio_free(cam_enable);
	ret = -EIO;
cam_regulator_enable_failed:
	if (avdd_2v8)
		regulator_put(avdd_2v8);
	avdd_2v8 = NULL;
	
	if (dvdd_1v2)
		regulator_put(dvdd_1v2);
	dvdd_1v2 = NULL;

	if (af_2v8)
		regulator_put(af_2v8);
	af_2v8 = NULL;
	printk("debug: power error\n");

	return ret;
}

static struct sensor_board_data s5k4ecgx_data = {
	.mount_pos	= SENSOR_USED | SENSOR_POS_BACK | SENSOR_RES_HIGH,
	.bus_type	= V4L2_MBUS_CSI2,
	.bus_flag	= V4L2_MBUS_CSI2_2_LANE,
	.flags  = V4L2_MBUS_CSI2_2_LANE,
	.dphy = {0x0a06, 0x33, 0x0900},
};

static struct i2c_board_info dkb_i2c_s5k4ecgx = {
		I2C_BOARD_INFO("s5k4ecgx", 0xAC >> 1),
};

struct soc_camera_desc soc_camera_desc_0 = {
	.subdev_desc = {
		.power          = s5k4ecgx_sensor_power,
		.drv_priv		= &s5k4ecgx_data,
		.flags		= 0,
	},
	.host_desc = {
		.bus_id = S5K4ECGX_CCIC_PORT,	/* Default as ccic2 */
		.i2c_adapter_id = 0,
		.board_info     = &dkb_i2c_s5k4ecgx,
		.module_name    = "s5k4ecgx",
	},
};
#endif

