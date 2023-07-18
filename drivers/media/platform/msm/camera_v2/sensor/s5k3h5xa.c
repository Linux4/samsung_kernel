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

#define S5K3H5XA_SENSOR_NAME "s5k3h5xa"
//#define PLATFORM_DRIVER_NAME "msm_camera_s5k3h5xa"
#define s5k3h5xa_obj s5k3h5xa_##obj
#define cam_err(fmt, arg...)			\
	do {					\
		printk(KERN_ERR "[CAMERA]][%s:%d] " fmt,		\
			__func__, __LINE__, ##arg);	\
	}						\
	while (0)

#include <linux/gpio.h>
#include <mach/gpiomux.h>
//#include <mach/socinfo.h>

DEFINE_MUTEX(s5k3h5xa_mut);
static struct msm_sensor_ctrl_t s5k3h5xa_s_ctrl;

static struct msm_sensor_power_setting s5k3h5xa_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{ //CAM Analog EN
		.seq_type = SENSOR_GPIO,
		.seq_val = CAM_VANA,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{ // CAM IO EN
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_CAMIO_EN,
		.config_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
	{ // HOST 1.8 V
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 1,
		.delay = 5,
	},
	{ // HOST 2.8 V - VAF
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 1,
		.delay = 5,
	},
	{ // CAM CORE EN
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_CAM_CORE_EN,
		.config_val = GPIO_OUT_HIGH,
		.delay = 5, //iTo 
	},
	{ // HOST 1.8 V
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 1,
		.delay = 5,
	},
	{ // MCLK 
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 2,
	},
	{ // Reset
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

static struct msm_sensor_power_setting s5k3h5xa_power_off_setting[] = {
#if 0
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = CAM_VANA,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},	
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 0,
	},
    {
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
    },
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 1,
	},
	{ // CAM IO EN
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_CAMIO_EN,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{ // CAM CORE EN
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_CAM_CORE_EN,
		.config_val = GPIO_OUT_LOW,
		.delay = 1, //iTo 
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 1,
	},
#else
{ //CAM Analog DIS
		.seq_type = SENSOR_GPIO,
		.seq_val = CAM_VANA,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{ // CAM IO DIS
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_CAMIO_EN,
		.config_val = GPIO_OUT_LOW,
		.delay = 5,
	},
	{ // CAM CORE DIS
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_EXT_CAM_CORE_EN,
		.config_val = GPIO_OUT_LOW,
		.delay = 5, //iTo 
	},
	{ // HOST 1.8 V
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 5,
	},
	{ // MCLK  DIS
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 2,
	},
	{ // Reset DIS
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
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
	{ // HOST 1.8 V
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 5,
	},
#endif
};



static struct v4l2_subdev_info s5k3h5xa_subdev_info[] = {
	{
	.code = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt = 1,
	.order = 0,
	},
	/* more can be supported, to be added later */
};

static int32_t msm_s5k3h5xa_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &s5k3h5xa_s_ctrl);
}
static const struct i2c_device_id s5k3h5xa_i2c_id[] = {
	{S5K3H5XA_SENSOR_NAME, (kernel_ulong_t)&s5k3h5xa_s_ctrl},
	{ }
};

static struct i2c_driver s5k3h5xa_i2c_driver = {
	.id_table = s5k3h5xa_i2c_id,
	.probe  = msm_s5k3h5xa_i2c_probe,
	.driver = {
		.name = S5K3H5XA_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k3h5xa_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id s5k3h5xa_dt_match[] = {
	{.compatible = "qcom,s5k3h5xa", .data = &s5k3h5xa_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, s5k3h5xa_dt_match);

static struct platform_driver s5k3h5xa_platform_driver = {
	.driver = {
		.name = "qcom,s5k3h5xa",
		.owner = THIS_MODULE,
		.of_match_table = s5k3h5xa_dt_match,
	},
};

#define REG_DIGITAL_GAIN_GREEN_R  0x020E
#define REG_DIGITAL_GAIN_RED  0x0210
#define REG_DIGITAL_GAIN_BLUE  0x0212
#define REG_DIGITAL_GAIN_GREEN_B  0x0214
#define QC_TEST	0


static int32_t s5k3h5xa_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	printk("Madhu: s5k3h5xa_platform_probe");
	match = of_match_device(s5k3h5xa_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init s5k3h5xa_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&s5k3h5xa_platform_driver,
		s5k3h5xa_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&s5k3h5xa_i2c_driver);
}

static void __exit s5k3h5xa_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (s5k3h5xa_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&s5k3h5xa_s_ctrl);
		platform_driver_unregister(&s5k3h5xa_platform_driver);
	} else
		i2c_del_driver(&s5k3h5xa_i2c_driver);
	return;
}

static struct msm_sensor_ctrl_t s5k3h5xa_s_ctrl = {
	.sensor_i2c_client = &s5k3h5xa_sensor_i2c_client,
	.power_setting_array.power_setting = s5k3h5xa_power_setting,
	.power_setting_array.size = ARRAY_SIZE(s5k3h5xa_power_setting),
	.power_setting_array.power_off_setting = s5k3h5xa_power_off_setting,
	.power_setting_array.off_size = ARRAY_SIZE(s5k3h5xa_power_off_setting),
	.msm_sensor_mutex = &s5k3h5xa_mut,
	.sensor_v4l2_subdev_info = s5k3h5xa_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k3h5xa_subdev_info),
};

module_init(s5k3h5xa_init_module);
module_exit(s5k3h5xa_exit_module);
MODULE_DESCRIPTION("s5k3h5xa");
MODULE_LICENSE("GPL v2");
