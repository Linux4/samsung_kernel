/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_sensor.h"
#include "msm.h"
#include "msm_cci.h"

#define S5K4H5YB_SENSOR_NAME "s5k4h5yb"
#define s5k4h5yb_obj s5k4h5yb_##obj
#include <linux/gpio.h>
#include <mach/gpiomux.h>

#define S5K4H5YB_DEBUG
#undef CDBG
#ifdef S5K4H5YB_DEBUG
#define CDBG(fmt, args...) printk("s5k4h5yb: %s:%d : "fmt,   __FUNCTION__, __LINE__, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

DEFINE_MUTEX(s5k4h5yb_mut);
static struct msm_sensor_ctrl_t s5k4h5yb_s_ctrl;

static struct msm_sensor_power_setting s5k4h5yb_power_setting[] = {
	{	//	CAM1_RST_N
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{	//	CAM_ANALOG_EN
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_VANA_POWER,
		.config_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
	{	//	CAM_ISP_SENSOR_IO_1.8V - VREG_L27_2P05
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 1,
		.delay = 5,
	},
	{	//	CAM_AF_2.8V - VREG_L28_DUAL
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 1,
		.delay = 5,
	},
	{	//	VREG_L26_1P225
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 1,
		.delay = 5,
	},
	{	//	CAM_MCLK0
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 2,
	},
	{	//	CAM1_RST_N
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 5,
	},
};

static struct msm_sensor_power_setting s5k4h5yb_power_off_setting[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 2,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_VANA_POWER,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 5,
	},
};

static struct v4l2_subdev_info s5k4h5yb_subdev_info[] = {
	{
	.code = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt = 1,
	.order = 0,
	},
	/* more can be supported, to be added later */
};

static int32_t msm_s5k4h5yb_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &s5k4h5yb_s_ctrl);
}
static const struct i2c_device_id s5k4h5yb_i2c_id[] = {
	{S5K4H5YB_SENSOR_NAME, (kernel_ulong_t)&s5k4h5yb_s_ctrl},
	{ }
};

static struct i2c_driver s5k4h5yb_i2c_driver = {
	.id_table = s5k4h5yb_i2c_id,
	.probe  = msm_s5k4h5yb_i2c_probe,
	.driver = {
		.name = S5K4H5YB_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k4h5yb_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id s5k4h5yb_dt_match[] = {
	{.compatible = "qcom,s5k4h5yb", .data = &s5k4h5yb_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, s5k4h5yb_dt_match);

static struct platform_driver s5k4h5yb_platform_driver = {
	.driver = {
		.name = "qcom,s5k4h5yb",
		.owner = THIS_MODULE,
		.of_match_table = s5k4h5yb_dt_match,
	},
};


int32_t s5k4h5yb_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
	
	if (rc < 0) {
		CDBG("%s: %s: read id failed  id = %x \n", __func__,
			s_ctrl->sensordata->sensor_name , s_ctrl->sensordata->slave_info->sensor_slave_addr);
		// rev 0.2 sensor has slave address of 0x6e. Please try with 0x6e slave address.
		s_ctrl->sensordata->slave_info->sensor_slave_addr = 0x6e;
		if (s_ctrl->sensor_i2c_client->cci_client != NULL)
			s_ctrl->sensor_i2c_client->cci_client->sid = s_ctrl->sensordata->slave_info->sensor_slave_addr >> 1;
		if (s_ctrl->sensor_i2c_client->client != NULL)
			s_ctrl->sensor_i2c_client->client->addr = s_ctrl->sensordata->slave_info->sensor_slave_addr;
			
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);		

		if(chipid != s_ctrl->sensordata->slave_info->sensor_id)
			s_ctrl->sensordata->slave_info->sensor_id = 0x5B03;
			
		if (rc < 0) 
		CDBG("%s: %s: read id failed  id = %x \n", __func__,
			s_ctrl->sensordata->sensor_name , s_ctrl->sensordata->slave_info->sensor_slave_addr);
	}
	CDBG("%s: read id: %x expected id %x:\n", __func__, chipid,
		s_ctrl->sensordata->slave_info->sensor_id);
	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		CDBG("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}
	return rc;
}

static int32_t s5k4h5yb_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	CDBG("E\n");
	match = of_match_device(s5k4h5yb_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	CDBG("X  = %d \n", rc);
	return rc;
}

static int __init s5k4h5yb_init_module(void)
{
	int32_t rc = 0;
	CDBG("E\n");
	rc = platform_driver_probe(&s5k4h5yb_platform_driver,
		s5k4h5yb_platform_probe);
	if (!rc)
		return rc;
	CDBG("X: rc=%d\n", rc);
	return i2c_add_driver(&s5k4h5yb_i2c_driver);
}

static void __exit s5k4h5yb_exit_module(void)
{
	CDBG("E\n");
	if (s5k4h5yb_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&s5k4h5yb_s_ctrl);
		platform_driver_unregister(&s5k4h5yb_platform_driver);
	} else
		i2c_del_driver(&s5k4h5yb_i2c_driver);
	CDBG("X\n");
	return;
}

static struct msm_sensor_fn_t s5k4h5yb_sensor_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = s5k4h5yb_sensor_match_id,
};

static struct msm_sensor_ctrl_t s5k4h5yb_s_ctrl = {
	.sensor_i2c_client = &s5k4h5yb_sensor_i2c_client,
	.power_setting_array.power_setting = s5k4h5yb_power_setting,
	.power_setting_array.size = ARRAY_SIZE(s5k4h5yb_power_setting),
	.power_setting_array.power_off_setting = s5k4h5yb_power_off_setting,
	.power_setting_array.off_size = ARRAY_SIZE(s5k4h5yb_power_off_setting),
	.msm_sensor_mutex = &s5k4h5yb_mut,
	.sensor_v4l2_subdev_info = s5k4h5yb_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k4h5yb_subdev_info),
	.func_tbl = &s5k4h5yb_sensor_func_tbl,
};

module_init(s5k4h5yb_init_module);
module_exit(s5k4h5yb_exit_module);
MODULE_DESCRIPTION("s5k4h5yb");
MODULE_LICENSE("GPL v2");
