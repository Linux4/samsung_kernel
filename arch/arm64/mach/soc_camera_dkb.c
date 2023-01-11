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
#include <linux/clk.h>
#include "soc_camera_dkb.h"

/* this is just define for more soc cameras */
static struct regulator *avdd_2v8;
static struct regulator *dovdd_1v8;
static struct regulator *af_2v8;
static struct regulator *dvdd_1v2;
static struct clk *mclk;
#ifdef CONFIG_SOC_CAMERA_SR200
static int sr200_sensor_power(struct device *dev, int on)
{
	int cam_enable, cam_reset;
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

	if (!dovdd_1v8) {
		dovdd_1v8 = regulator_get(dev, "dovdd_1v8");
		if (IS_ERR(dovdd_1v8)) {
			dev_warn(dev, "Failed to get regulator dovdd_1v8\n");
			dovdd_1v8 = NULL;
		}
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
		if (avdd_2v8) {
			regulator_set_voltage(avdd_2v8, 2800000, 2800000);
			ret = regulator_enable(avdd_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (af_2v8) {
			regulator_set_voltage(af_2v8, 2800000, 2800000);
			ret = regulator_enable(af_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (dvdd_1v2) {
			regulator_set_voltage(dvdd_1v2, 1200000, 1200000);
			ret = regulator_enable(dvdd_1v2);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (dovdd_1v8) {
			regulator_set_voltage(dovdd_1v8, 1800000, 1800000);
			ret = regulator_enable(dovdd_1v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		gpio_direction_output(cam_enable, 1);
		usleep_range(1000, 5000);

		clk_set_rate(mclk, 26000000);
		clk_prepare_enable(mclk);

		gpio_direction_output(cam_reset, 1);
		usleep_range(1000, 5000);
	} else {
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

		if (dovdd_1v8)
			regulator_disable(dovdd_1v8);
		if (dvdd_1v2)
			regulator_disable(dvdd_1v2);
		if (avdd_2v8)
			regulator_disable(avdd_2v8);
		if (af_2v8)
			regulator_disable(af_2v8);
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
	if (dvdd_1v2)
		regulator_put(dvdd_1v2);
	dvdd_1v2 = NULL;
	if (af_2v8)
		regulator_put(af_2v8);
	af_2v8 = NULL;
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

struct soc_camera_desc soc_camera_desc_1 = {
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

#ifdef CONFIG_SOC_CAMERA_S5K8AA
static int s5k8aa_sensor_power(struct device *dev, int on)
{
	int cam_enable, cam_reset;
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

	if (!dovdd_1v8) {
		dovdd_1v8 = regulator_get(dev, "dovdd_1v8");
		if (IS_ERR(dovdd_1v8)) {
			dev_warn(dev, "Failed to get regulator dovdd_1v8\n");
			dovdd_1v8 = NULL;
		}
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
		if (avdd_2v8) {
			regulator_set_voltage(avdd_2v8, 2800000, 2800000);
			ret = regulator_enable(avdd_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (af_2v8) {
			regulator_set_voltage(af_2v8, 2800000, 2800000);
			ret = regulator_enable(af_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (dvdd_1v2) {
			regulator_set_voltage(dvdd_1v2, 1200000, 1200000);
			ret = regulator_enable(dvdd_1v2);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (dovdd_1v8) {
			regulator_set_voltage(dovdd_1v8, 1800000, 1800000);
			ret = regulator_enable(dovdd_1v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		clk_set_rate(mclk, 26000000);
		clk_prepare_enable(mclk);

		gpio_direction_output(cam_reset, 0);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_enable, 1);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_reset, 1);
		usleep_range(5000, 20000);
	} else {
		/*
		 * Keep PWDN always on as defined in spec
		 * gpio_direction_output(cam_enable, 0);
		 * usleep_range(5000, 20000);
		 */
		gpio_direction_output(cam_reset, 0);

		clk_disable_unprepare(mclk);
		devm_clk_put(dev, mclk);

		if (dovdd_1v8)
			regulator_disable(dovdd_1v8);
		if (dvdd_1v2)
			regulator_disable(dvdd_1v2);
		if (avdd_2v8)
			regulator_disable(avdd_2v8);
		if (af_2v8)
			regulator_disable(af_2v8);
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
	if (dvdd_1v2)
		regulator_put(dvdd_1v2);
	dvdd_1v2 = NULL;
	if (af_2v8)
		regulator_put(af_2v8);
	af_2v8 = NULL;
	if (dovdd_1v8)
		regulator_put(dovdd_1v8);
	dovdd_1v8 = NULL;
	if (avdd_2v8)
		regulator_put(avdd_2v8);
	avdd_2v8 = NULL;

	return ret;
}

static struct sensor_board_data s5k8aa_data = {
	.mount_pos	= SENSOR_USED | SENSOR_POS_FRONT | SENSOR_RES_LOW,
	.bus_type	= V4L2_MBUS_CSI2,
	.bus_flag	= V4L2_MBUS_CSI2_1_LANE,
	.flags  = V4L2_MBUS_CSI2_1_LANE,
	.dphy = {0x1806, 0x00011, 0xe00},
};

static struct i2c_board_info dkb_i2c_s5k8aay = {
		I2C_BOARD_INFO("s5k8aay", 0x3c),
};

struct soc_camera_desc soc_camera_desc_0 = {
	.subdev_desc = {
		.power          = s5k8aa_sensor_power,
		.drv_priv		= &s5k8aa_data,
		.flags		= 0,
	},
	.host_desc = {
		.bus_id = S5K8AA_CCIC_PORT,	/* Default as ccic2 */
		.i2c_adapter_id = 0,
		.board_info     = &dkb_i2c_s5k8aay,
		.module_name    = "s5k8aay",
	},
};
#endif

#ifdef CONFIG_SOC_CAMERA_OV5640_ECS
static int soc_sensor_flash_led_set(void *control, bool op)
{
	int flash_on = GPIO_FLASH_EN;
	int torch_on = GPIO_TORCH_EN;
	struct v4l2_ctrl *ctrl = (struct v4l2_ctrl *) control;

	if (gpio_request(flash_on, "flash_power_on")) {
		pr_err("Request GPIO failed, gpio:%d\n", flash_on);
		return -EIO;
	}
	if (gpio_request(torch_on, "torch_power_on")) {
		pr_err("Request GPIO failed, gpio:%d\n", torch_on);
		return -EIO;
	}

	switch (ctrl->id) {
	case V4L2_CID_FLASH_STROBE:
		gpio_direction_output(flash_on, 1);
		gpio_direction_output(torch_on, 1);
		break;
	case V4L2_CID_FLASH_TORCH_INTENSITY:
		gpio_direction_output(torch_on, 1);
		gpio_direction_output(flash_on, 0);
		break;
	case V4L2_CID_FLASH_STROBE_STOP:
		gpio_direction_output(flash_on, 0);
		gpio_direction_output(torch_on, 0);
		break;
	default:
		break;
	}

	gpio_free(flash_on);
	gpio_free(torch_on);

	return 0;
}

/* ov5640 as back sensor on 1L88, as front sensor on 1U88 */
static int ov5640_sensor_power(struct device *dev, int on)
{
	static struct regulator *c1_avdd_2v8;
	static struct regulator *c1_dovdd_1v8;
	static struct regulator *c1_af_2v8;
	static struct regulator *c1_dvdd_1v2;
	int cam_enable, cam_reset;
	int ret;

	if (!c1_avdd_2v8) {
		c1_avdd_2v8 = regulator_get(dev, "avdd_2v8");
		if (IS_ERR(c1_avdd_2v8)) {
			dev_warn(dev, "Failed to get regulator avdd_2v8\n");
			c1_avdd_2v8 = NULL;
		}
	}

	if (!c1_dovdd_1v8) {
		c1_dovdd_1v8 = regulator_get(dev, "dovdd_1v8");
		if (IS_ERR(c1_dovdd_1v8)) {
			dev_warn(dev, "Failed to get regulator dovdd_1v8\n");
			c1_dovdd_1v8 = NULL;
		}
	}

	if (!c1_af_2v8) {
		c1_af_2v8 = regulator_get(dev, "af_2v8");
		if (IS_ERR(c1_af_2v8)) {
			dev_warn(dev, "Failed to get regulator af_2v8\n");
			c1_af_2v8 = NULL;
		}
	}

	if (!c1_dvdd_1v2) {
		c1_dvdd_1v2 = regulator_get(dev, "dvdd_1v2");
		if (IS_ERR(c1_dvdd_1v2)) {
			dev_warn(dev, "Failed to get regulator dvdd_1v2\n");
			c1_dvdd_1v2 = NULL;
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
		if (c1_dvdd_1v2) {
			regulator_set_voltage(c1_dvdd_1v2,
					1200000, 1200000);
			ret = regulator_enable(c1_dvdd_1v2);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (c1_dovdd_1v8) {
			regulator_set_voltage(c1_dovdd_1v8,
					1800000, 1800000);
			ret = regulator_enable(c1_dovdd_1v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (c1_avdd_2v8) {
			regulator_set_voltage(c1_avdd_2v8, 2800000, 2800000);
			ret = regulator_enable(c1_avdd_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (c1_af_2v8) {
			regulator_set_voltage(c1_af_2v8, 2800000, 2800000);
			ret = regulator_enable(c1_af_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		usleep_range(5000, 20000);
		gpio_direction_output(cam_enable, 0);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_reset, 0);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_reset, 1);
		usleep_range(5000, 20000);
	} else {
		gpio_direction_output(cam_reset, 0);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_reset, 1);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_enable, 1);

		if (c1_avdd_2v8)
			regulator_disable(c1_avdd_2v8);
			if (c1_dovdd_1v8)
				regulator_disable(c1_dovdd_1v8);
		if (c1_af_2v8)
			regulator_disable(c1_af_2v8);
			if (c1_dvdd_1v2)
				regulator_disable(c1_dvdd_1v2);
	}

	gpio_free(cam_enable);
	gpio_free(cam_reset);

	return 0;

cam_reset_failed:
	gpio_free(cam_enable);
cam_enable_failed:
	ret = -EIO;
cam_regulator_enable_failed:
	if (c1_dvdd_1v2)
		regulator_put(c1_dvdd_1v2);
	c1_dvdd_1v2 = NULL;
	if (c1_af_2v8)
		regulator_put(c1_af_2v8);
	c1_af_2v8 = NULL;
	if (c1_dovdd_1v8)
		regulator_put(c1_dovdd_1v8);
	c1_dovdd_1v8 = NULL;
	if (c1_avdd_2v8)
		regulator_put(c1_avdd_2v8);
	c1_avdd_2v8 = NULL;

	return ret;
}

static struct sensor_board_data ov5640_data = {
	.mount_pos	= SENSOR_USED | SENSOR_POS_BACK | SENSOR_RES_HIGH,
	.bus_type	= V4L2_MBUS_CSI2,
	.bus_flag	= V4L2_MBUS_CSI2_2_LANE, /* ov5640 used 2 lanes */
	.dphy		= {0x0D06, 0x33, 0x0900},
	.v4l2_flash_if = soc_sensor_flash_led_set,
};

static struct i2c_board_info dkb_i2c_ov5640 = {
		I2C_BOARD_INFO("ov5640", 0x3c),
};

struct soc_camera_desc soc_camera_desc_3 = {
	.subdev_desc = {
		.power          = ov5640_sensor_power,
		.drv_priv		= &ov5640_data,
		.flags		= 0,
	},
	.host_desc = {
		.bus_id = OV5640_CCIC_PORT,	/* Must match with the camera ID */
		.i2c_adapter_id = 0,
		.board_info     = &dkb_i2c_ov5640,
		.module_name    = "ov5640",
	},
};
#endif

#ifdef CONFIG_SOC_CAMERA_SP0A20_ECS
static int sp0a20_sensor_power(struct device *dev, int on)
{
	int cam_enable, cam_reset;
	int ret;

	/* Get the regulators and never put it */
	/*
	 * The regulators is for sensor and should be in sensor driver
	 * As SoC camera does not support device tree,  leave it here
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
		gpio_direction_output(cam_enable, 0);
		usleep_range(500, 1000);

		if (avdd_2v8) {
			regulator_set_voltage(avdd_2v8, 2800000, 2800000);
			ret = regulator_enable(avdd_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (af_2v8) {
			regulator_set_voltage(af_2v8, 2800000, 2800000);
			ret = regulator_enable(af_2v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}
		usleep_range(500, 1000);

		if (dovdd_1v8) {
			regulator_set_voltage(dovdd_1v8,
					1800000, 1800000);
			ret = regulator_enable(dovdd_1v8);
			if (ret)
				goto cam_regulator_enable_failed;
		}

		if (dvdd_1v2) {
			regulator_set_voltage(dvdd_1v2,
					1200000, 1200000);
			ret = regulator_enable(dvdd_1v2);
			if (ret)
				goto cam_regulator_enable_failed;
		}
		clk_set_rate(mclk, 26000000);
		clk_prepare_enable(mclk);
		gpio_direction_output(cam_reset, 0);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_enable, 1);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_enable, 0);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_reset, 1);
		usleep_range(5000, 20000);
	} else {
		gpio_direction_output(cam_reset, 0);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_enable, 0);
		usleep_range(5000, 20000);
		gpio_direction_output(cam_enable, 1);
		clk_disable_unprepare(mclk);
		devm_clk_put(dev, mclk);

		if (dovdd_1v8)
			regulator_disable(dovdd_1v8);
		if (dvdd_1v2)
			regulator_disable(dvdd_1v2);
		if (avdd_2v8)
			regulator_disable(avdd_2v8);
		if (af_2v8)
			regulator_disable(af_2v8);

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
	if (dvdd_1v2)
		regulator_put(dvdd_1v2);
	dvdd_1v2 = NULL;
	if (af_2v8)
		regulator_put(af_2v8);
	af_2v8 = NULL;
	if (dovdd_1v8)
		regulator_put(dovdd_1v8);
	dovdd_1v8 = NULL;
	if (avdd_2v8)
		regulator_put(avdd_2v8);
	avdd_2v8 = NULL;

	return ret;
}

static struct sensor_board_data sp0a20_data = {
	.mount_pos	= SENSOR_USED | SENSOR_POS_FRONT | SENSOR_RES_LOW,
	.bus_type	= V4L2_MBUS_CSI2,
	.bus_flag	= V4L2_MBUS_CSI2_1_LANE,
	.flags      = V4L2_MBUS_CSI2_1_LANE,
	.dphy = {0x1806, 0x00011, 0xe00},
};

static struct i2c_board_info dkb_i2c_sp0a20 = {
		I2C_BOARD_INFO("sp0a20", 0x21),
};

struct soc_camera_desc soc_camera_desc_2 = {
	.subdev_desc = {
		.power          = sp0a20_sensor_power,
		.drv_priv		= &sp0a20_data,
		.flags		= 0,
	},
	.host_desc = {
		.bus_id = SP0A20_CCIC_PORT,	/* Default as ccic2 */
		.i2c_adapter_id = 0,
		.board_info     = &dkb_i2c_sp0a20,
		.module_name    = "sp0a20",
	},
};
#endif
