/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is-sensor.h>

#include "fimc-is-hw.h"
#include "fimc-is-core.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-resourcemgr.h"
#include "fimc-is-dt.h"
#include "fimc-is-device-sr261.h"

#define SENSOR_NAME "SR261"

static struct fimc_is_sensor_cfg config_sr261[] = {
	/* 1936x1090@30fps */
	FIMC_IS_SENSOR_CFG(1936, 1090, 30, 8, 0),
};

static struct fimc_is_vci vci_sr261[] = {
	{
		.pixelformat = V4L2_PIX_FMT_SBGGR10,
		.config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_UNKNOWN}, {3, 0}}
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR12,
		.config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_UNKNOWN}, {3, 0}}
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR16,
		.config = {{0, HW_FORMAT_RAW10}, {1, HW_FORMAT_UNKNOWN}, {2, HW_FORMAT_UNKNOWN}, {3, 0}}
	}
};

static int sensor_sr261_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	pr_info("%s\n", __func__);
	return 0;
}
static int sensor_sr261_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	pr_info("%s\n", __func__);
	return 0;
}
static int sensor_sr261_registered(struct v4l2_subdev *sd)
{
	pr_info("%s\n", __func__);
	return 0;
}

static void sensor_sr261_unregistered(struct v4l2_subdev *sd)
{
	pr_info("%s\n", __func__);
}

static const struct v4l2_subdev_internal_ops internal_ops = {
	.open = sensor_sr261_open,
	.close = sensor_sr261_close,
	.registered = sensor_sr261_registered,
	.unregistered = sensor_sr261_unregistered,
};

static int sensor_sr261_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_sr261 *module_sr261;
	struct i2c_client *client;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_sr261 = module->private_data;
	if (unlikely(!module_sr261)) {
		err("module_sr261 is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = module->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_sr261->system_clock = 146 * 1000 * 1000;
	module_sr261->line_length_pck = 146 * 1000 * 1000;

	pr_info("%s\n", __func__);
	/* sensor init */



	pr_info("[MOD:D:%d] %s(%d)\n", module->sensor_id, __func__, val);
p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = sensor_sr261_init
};

static int sensor_sr261_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct fimc_is_module_enum *sensor;

	pr_info("%s\n", __func__);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!sensor) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (enable) {
		ret = CALL_MOPS(sensor, stream_on, subdev);
		if (ret) {
			err("stream_on is fail(%d)", ret);
			goto p_err;
		}
	} else {
		ret = CALL_MOPS(sensor, stream_off, subdev);
		if (ret) {
			err("stream_off is fail(%d)", ret);
			goto p_err;
		}
	}

p_err:
	return 0;
}

static int sensor_sr261_s_param(struct v4l2_subdev *subdev, struct v4l2_streamparm *param)
{
	int ret = 0;
	struct fimc_is_module_enum *sensor;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;
	u64 duration;

	BUG_ON(!subdev);
	BUG_ON(!param);

	pr_info("%s\n", __func__);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	if (!tpf->denominator) {
		err("denominator is 0");
		ret = -EINVAL;
		goto p_err;
	}

	if (!tpf->numerator) {
		err("numerator is 0");
		ret = -EINVAL;
		goto p_err;
	}

	duration = (u64)(tpf->numerator * 1000 * 1000 * 1000) /
					(u64)(tpf->denominator);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (!sensor) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_MOPS(sensor, s_duration, subdev, duration);
	if (ret) {
		err("s_duration is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int sensor_sr261_s_format(struct v4l2_subdev *subdev, struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct fimc_is_module_enum *module;
	struct fimc_is_module_sr261 *module_sr261;
	struct i2c_client *client;

	BUG_ON(!subdev);
	BUG_ON(!fmt);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!module)) {
		err("module is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	module_sr261 = module->private_data;
	if (unlikely(!module_sr261)) {
		err("module_sr261 is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = module->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = sensor_sr261_s_stream,
	.s_parm = sensor_sr261_s_param,
	.s_mbus_fmt = sensor_sr261_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops
};

int sensor_sr261_stream_on(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int sensor_sr261_stream_off(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

/*
 * @ brief
 * frame duration time
 * @ unit
 * nano second
 * @ remarks
 */
int sensor_sr261_s_duration(struct v4l2_subdev *subdev, u64 duration)
{
	int ret = 0;
	u8 value[2];
	u32 framerate, result;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	pr_info("%s\n", __func__);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	framerate = 1000 * 1000 * 1000 / (u32)duration;
	result = 1060 / framerate;
	value[0] = result & 0xFF;
	value[1] = (result >> 8) & 0xFF;

p_err:
	return ret;
}

int sensor_sr261_g_min_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_sr261_g_max_duration(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_sr261_s_exposure(struct v4l2_subdev *subdev, u64 exposure)
{
	int ret = 0;
	u8 value;
	struct fimc_is_module_enum *sensor;
	struct i2c_client *client;

	BUG_ON(!subdev);

	pr_info("%s(%d)\n", __func__, (u32)exposure);

	sensor = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	if (unlikely(!sensor)) {
		err("sensor is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	client = sensor->client;
	if (unlikely(!client)) {
		err("client is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	value = exposure & 0xFF;

p_err:
	return ret;
}

int sensor_sr261_g_min_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_sr261_g_max_exposure(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_sr261_s_again(struct v4l2_subdev *subdev, u64 sensitivity)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	return ret;
}

int sensor_sr261_g_min_again(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_sr261_g_max_again(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_sr261_s_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_sr261_g_min_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

int sensor_sr261_g_max_dgain(struct v4l2_subdev *subdev)
{
	int ret = 0;
	return ret;
}

struct fimc_is_sensor_ops module_sr261_ops = {
	.stream_on	= sensor_sr261_stream_on,
	.stream_off	= sensor_sr261_stream_off,
	.s_duration	= sensor_sr261_s_duration,
	.g_min_duration	= sensor_sr261_g_min_duration,
	.g_max_duration	= sensor_sr261_g_max_duration,
	.s_exposure	= sensor_sr261_s_exposure,
	.g_min_exposure	= sensor_sr261_g_min_exposure,
	.g_max_exposure	= sensor_sr261_g_max_exposure,
	.s_again	= sensor_sr261_s_again,
	.g_min_again	= sensor_sr261_g_min_again,
	.g_max_again	= sensor_sr261_g_max_again,
	.s_dgain	= sensor_sr261_s_dgain,
	.g_min_dgain	= sensor_sr261_g_min_dgain,
	.g_max_dgain	= sensor_sr261_g_max_dgain
};

#ifdef CONFIG_OF
static int sensor_sr261_power_setpin(struct platform_device *pdev,
		struct exynos_platform_fimc_is_module *pdata)
{
	struct device *dev;
	struct device_node *dnode;
	int gpio_reset = 0;
	int gpio_none = 0;
	int gpio_standby = 0;

	BUG_ON(!pdev);

	dev = &pdev->dev;
	dnode = dev->of_node;

	dev_info(dev, "%s E v4\n", __func__);

	gpio_reset = of_get_named_gpio(dnode, "gpio_reset", 0);
	if (!gpio_is_valid(gpio_reset)) {
		dev_err(dev, "failed to get gpio_reset\n");
		return -EINVAL;
	} else {
		gpio_request_one(gpio_reset, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_reset);
	}

	gpio_standby = of_get_named_gpio(dnode, "gpio_standby", 0);
	if (!gpio_is_valid(gpio_standby)) {
		err("failed to get gpio_standby");
	} else {
		gpio_request_one(gpio_standby, GPIOF_OUT_INIT_LOW, "CAM_GPIO_OUTPUT_LOW");
		gpio_free(gpio_standby);
	}

	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_ON);
	SET_PIN_INIT(pdata, SENSOR_SCENARIO_VISION, GPIO_SCENARIO_OFF);
#if defined(CONFIG_MACH_SMDK7580)
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "sen_rst low", PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, "sen_rst high", PIN_OUTPUT, 1, 0);
#elif defined(CONFIG_MACH_UNIVERSAL7580)
	/* Front Camera ON */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDDIO_1.8V_VT", PIN_REGULATOR, 1, 100);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "VDD_VTCAM_SENSOR_A2P8", PIN_REGULATOR, 1, 100);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_standby, NULL, PIN_OUTPUT, 1, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_none, "pin", PIN_FUNCTION, 0, 100);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_ON, gpio_reset, NULL, PIN_OUTPUT, 1, 0);

	/* Front Camera OFF */
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "pin", PIN_FUNCTION, 1, 100);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_reset, NULL, PIN_OUTPUT, 0, 0);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_standby, NULL, PIN_OUTPUT, 0, 100);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDD_VTCAM_SENSOR_A2P8", PIN_REGULATOR, 0, 100);
	SET_PIN(pdata, SENSOR_SCENARIO_NORMAL, GPIO_SCENARIO_OFF, gpio_none, "VDDIO_1.8V_VT", PIN_REGULATOR, 0, 10);
#endif

	dev_info(dev, "%s X v4\n", __func__);

	return 0;
}
#endif /* CONFIG_OF */

int sensor_sr261_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;
	static bool probe_retried = false;
	struct exynos_platform_fimc_is_module *pdata;
	struct device *dev;

	if (!fimc_is_dev)
		goto probe_defer;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	dev = &pdev->dev;

#ifdef CONFIG_OF
	fimc_is_sensor_module_parse_dt(pdev, sensor_sr261_power_setpin);
#endif

	pdata = dev_get_platdata(dev);
	device = &core->sensor[pdata->id];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		probe_err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	/* SR261 */
	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	clear_bit(FIMC_IS_MODULE_GPIO_ON, &module->state);
	module->pdata = pdata;
	module->pdev = pdev;
	module->sensor_id = SENSOR_NAME_SR261;
	module->subdev = subdev_module;
	module->device = pdata->id;
	module->client = NULL;
	module->active_width = 1920;
	module->active_height = 1080;
	module->margin_left = 8;
	module->margin_right = 8;
	module->margin_top = 6;
	module->margin_bottom = 4;
	module->pixel_width = module->active_width + 16;
	module->pixel_height = module->active_height + 10;
	module->max_framerate = 30;
	module->position = pdata->position;
	module->mode = CSI_MODE_CH0_ONLY;
	module->lanes = CSI_DATA_LANES_2;
	module->vcis = ARRAY_SIZE(vci_sr261);
	module->vci = vci_sr261;
	module->sensor_maker = "SF";
	module->sensor_name = "SR261";
	module->setfile_name = "setfile_sr261.bin";
	module->cfgs = ARRAY_SIZE(config_sr261);
	module->cfg = config_sr261;
	module->private_data = kzalloc(sizeof(struct fimc_is_module_sr261), GFP_KERNEL);
	if (!module->private_data) {
		probe_err("private_data is NULL");
		ret = -ENOMEM;
		kfree(subdev_module);
		goto p_err;
	}

	ext = &module->ext;
	ext->mipi_lane_num = module->lanes;
	ext->I2CSclk = I2C_L0;
	ext->sensor_con.product_name = SENSOR_NAME_SR261;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = pdata->sensor_i2c_ch;
	ext->sensor_con.peri_setting.i2c.slave_address = pdata->sensor_i2c_addr;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;

	ext->companion_con.product_name = COMPANION_NAME_NOTHING;

	ext->actuator_con.product_name = ACTUATOR_NAME_NOTHING;

	ext->flash_con.product_name = FLADRV_NAME_NOTHING;

	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->sensor_id);

p_err:
	probe_info("%s(%d)\n", __func__, ret);
	return ret;

probe_defer:
	if (probe_retried) {
		err("probe has already been retried!!");
		BUG();
	}

	probe_retried = true;
	err("core device is not yet probed");
	return -EPROBE_DEFER;
}

static int sensor_sr261_remove(struct platform_device *pdev)
{
	int ret = 0;
	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id exynos_fimc_is_sensor_sr261_match[] = {
	{
		.compatible = "samsung,exynos5-fimc-is-sensor-sr261",
	},
	{},
};
#endif

static struct platform_driver sensor_sr261_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_fimc_is_sensor_sr261_match
#endif
	},
	.probe	= sensor_sr261_probe,
	.remove	= sensor_sr261_remove,
};

module_platform_driver(sensor_sr261_driver);

static int __init fimc_is_sensor_module_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sensor_sr261_driver);
	if (ret)
		err("platform_driver_register failed: %d\n", ret);

	return ret;
}

static void __exit fimc_is_sensor_module_exit(void)
{
	platform_driver_unregister(&sensor_sr261_driver);
}
module_init(fimc_is_sensor_module_init);
module_exit(fimc_is_sensor_module_exit);

MODULE_AUTHOR("Gilyeon lim");
MODULE_DESCRIPTION("Sensor SR261 driver");
MODULE_LICENSE("GPL v2");
