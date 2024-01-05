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
#include <mach/regs-gpio.h>
#include <mach/regs-clock.h>
#include <plat/clock.h>
#include <plat/gpio-cfg.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <mach/exynos-fimc-is-sensor.h>

#include "../fimc-is-core.h"
#include "../fimc-is-device-sensor.h"
#include "../fimc-is-resourcemgr.h"
#include "../fimc-is-hw.h"
#include "fimc-is-device-sr261.h"
#include "sr261_vision_regs.h"

#define SENSOR_NAME "SR261"

static struct fimc_is_sensor_cfg config_sr261[] = {
	/* 1936x1090@30fps */
	FIMC_IS_SENSOR_CFG(1936, 1090, 30, 18, 0),
	/* 1936x1090@24fps */
	FIMC_IS_SENSOR_CFG(1936, 1090, 24, 14, 1),
	/* temporary code for vision : 184x104@30fps */
	FIMC_IS_SENSOR_CFG(184, 104, 30, 18, 6),
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
	int i = 0;
	int init_table_size = ARRAY_SIZE(sr261_init_regs);
	struct fimc_is_module_enum *module;
	struct fimc_is_module_sr261 *module_sr261;
	struct i2c_client *client;

	BUG_ON(!subdev);

	module = (struct fimc_is_module_enum *)v4l2_get_subdevdata(subdev);
	module_sr261 = module->private_data;
	client = module->client;

	module_sr261->system_clock = 146 * 1000 * 1000;
	module_sr261->line_length_pck = 146 * 1000 * 1000;

	pr_info("%s\n", __func__);
	/* sensor init */

	pr_info("%s :sr261_init_table(%d)\n",__func__, init_table_size);
	for(i = 0; i < init_table_size; i++) {
		fimc_is_sensor_write8(client, sr261_init_regs[i].addr, sr261_init_regs[i].data);
	}

	pr_info("[MOD:D:%d] %s(%d)\n", module->id, __func__, val);

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
			err("s_duration is fail(%d)", ret);
			goto p_err;
		}
	} else {
		ret = CALL_MOPS(sensor, stream_off, subdev);
		if (ret) {
			err("s_duration is fail(%d)", ret);
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
	/* TODO */
	return 0;
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
	int i = 0;
	int vision_table_size = ARRAY_SIZE(sr261_vision_mode_regs);
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

	pr_info("%s:sr261_vision_mode_table(%d)\n",__func__, vision_table_size);
	for(i = 0; i < vision_table_size; i++) {
		fimc_is_sensor_write8(client, sr261_vision_mode_regs[i].addr, sr261_vision_mode_regs[i].data);
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
#if 0
	ret = fimc_is_sensor_write8(client, 0x4100, 0);
	if (ret < 0) {
		err("fimc_is_sensor_write8 is fail(%d)", ret);
		goto p_err;
	}
#endif
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
#if 0
	fimc_is_sensor_write8(client, SENSOR_REG_VIS_DURATION_MSB, value[1]);
	fimc_is_sensor_write8(client, SENSOR_REG_VIS_DURATION_LSB, value[0]);
#endif
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
#if 0
	fimc_is_sensor_write8(client, SENSOR_REG_VIS_AE_TARGET, value);
#endif
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

int sensor_sr261_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;
	struct sensor_open_extended *ext;
	static bool probe_retried = false;

	if (!fimc_is_dev)
		goto probe_defer;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		err("core device is not yet probed");
		return -EPROBE_DEFER;
	}

	device = &core->sensor[SENSOR_SR261_INSTANCE];

	subdev_module = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_module) {
		err("subdev_module is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	/* SR261 */
	module = &device->module_enum[atomic_read(&core->resourcemgr.rsccount_module)];
	atomic_inc(&core->resourcemgr.rsccount_module);
	module->id = SENSOR_SR261_NAME;
	module->subdev = subdev_module;
	module->device = SENSOR_SR261_INSTANCE;
	module->ops = &module_sr261_ops;
	module->client = client;
	module->active_width = 1920;
	module->active_height = 1080;
	module->pixel_width = module->active_width + 16;
	module->pixel_height = module->active_height + 10;
	module->max_framerate = 30;
	module->position = SENSOR_POSITION_FRONT;
	module->mode = CSI_MODE_CH0_ONLY;
	module->lanes = CSI_DATA_LANES_1;
	module->setfile_name = "setfile_sr261.bin";
	module->cfgs = ARRAY_SIZE(config_sr261);
	module->cfg = config_sr261;
	module->private_data = kzalloc(sizeof(struct fimc_is_module_sr261), GFP_KERNEL);
	if (!module->private_data) {
		err("private_data is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	ext = &module->ext;
	ext->mipi_lane_num = module->lanes;
	ext->I2CSclk = I2C_L0;
	ext->sensor_con.product_name = SENSOR_NAME_SR261;
	ext->sensor_con.peri_type = SE_I2C;
	ext->sensor_con.peri_setting.i2c.channel = SENSOR_CONTROL_I2C1;
	ext->sensor_con.peri_setting.i2c.slave_address = 0x40;
	ext->sensor_con.peri_setting.i2c.speed = 400000;

	ext->from_con.product_name = FROMDRV_NAME_NOTHING;

	ext->companion_con.product_name = COMPANION_NAME_NOTHING;

	if (client) {
		v4l2_i2c_subdev_init(subdev_module, client, &subdev_ops);
		subdev_module->internal_ops = &internal_ops;
	} else {
		v4l2_subdev_init(subdev_module, &subdev_ops);
	}

	v4l2_set_subdevdata(subdev_module, module);
	v4l2_set_subdev_hostdata(subdev_module, device);
	snprintf(subdev_module->name, V4L2_SUBDEV_NAME_SIZE, "sensor-subdev.%d", module->id);

p_err:
	info("%s(%d)\n", __func__, ret);
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

static int sensor_sr261_remove(struct i2c_client *client)
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

static const struct i2c_device_id sensor_sr261_idt[] = {
	{ SENSOR_NAME, 0 },
};

static struct i2c_driver sensor_sr261_driver = {
	.driver = {
		.name	= SENSOR_NAME,
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = exynos_fimc_is_sensor_sr261_match
#endif
	},
	.probe	= sensor_sr261_probe,
	.remove	= sensor_sr261_remove,
	.id_table = sensor_sr261_idt
};

static int __init sensor_sr261_load(void)
{
	return i2c_add_driver(&sensor_sr261_driver);
}

static void __exit sensor_sr261_unload(void)
{
	i2c_del_driver(&sensor_sr261_driver);
}

module_init(sensor_sr261_load);
module_exit(sensor_sr261_unload);

MODULE_AUTHOR("Gilyeon lim");
MODULE_DESCRIPTION("Sensor SR261 driver");
MODULE_LICENSE("GPL v2");

