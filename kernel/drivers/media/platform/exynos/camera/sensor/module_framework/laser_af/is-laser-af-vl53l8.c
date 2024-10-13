/*
 * Samsung Exynos5 SoC series Ranging sensor driver
 *
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>

#include <linux/videodev2.h>
#include <videodev2_exynos_camera.h>

#include "is-device-sensor.h"
#include "is-interface-sensor.h"
#include "is-device-sensor-peri.h"
#include "is-core.h"

enum vl53l8_external_control {
    VL53L8_EXT_START,
    VL53L8_EXT_STOP,
    VL53L8_EXT_GET_RANGE,
    VL53L8_EXT_SET_SAMPLING_RATE,
    VL53L8_EXT_SET_INTEG_TIME,
};

extern int vl53l8_ext_control(enum vl53l8_external_control input, void *data, u32 *size);

static u64 laser_af_start_time;
static struct itf_laser_af_data_VL53L8 laser_af_data;
static u32 laser_af_data_size;
static u64 previous_time;

#define VL53L8_FIRST_DELAY	168000000L
#define VL53L8_GET_RANGE_HZ	15

static void laser_af_vl53l8_start_work(struct work_struct *work)
{
	struct is_laser_af *laser_af;

	laser_af = container_of(work, struct is_laser_af, start_work);

	WARN_ON(!laser_af);

	mutex_lock(laser_af->laser_lock);

	info("[%s] VL53L8_EXT_START E\n", __func__);
	vl53l8_ext_control(VL53L8_EXT_START, NULL, NULL);
	info("[%s] VL53L8_EXT_START X\n", __func__);

	laser_af_start_time = ktime_get_ns();

	laser_af->active = true;

	mutex_unlock(laser_af->laser_lock);
}

static void laser_af_vl53l8_stop_work(struct work_struct *work)
{
	struct is_laser_af *laser_af;

	laser_af = container_of(work, struct is_laser_af, stop_work);

	WARN_ON(!laser_af);

	mutex_lock(laser_af->laser_lock);

	info("[%s] VL53L8_EXT_STOP E\n", __func__);
	vl53l8_ext_control(VL53L8_EXT_STOP, NULL, NULL);
	info("[%s] VL53L8_EXT_STOP X\n", __func__);

	laser_af->active = false;

	mutex_unlock(laser_af->laser_lock);
}

static void laser_af_vl53l8_get_distance_work(struct work_struct *work)
{
	struct is_laser_af *laser_af;
	u64 current_time;
	unsigned long flags;

	laser_af = container_of(work, struct is_laser_af, get_distance_work);

	WARN_ON(!laser_af);

	if (!laser_af->active) {
		dbg_actuator("[%s] LAF is not activated\n", __func__);
		return;
	}

	spin_lock_irqsave(&laser_af->laf_work_lock, flags);

	current_time = ktime_get_ns();

	if ((current_time > laser_af_start_time + VL53L8_FIRST_DELAY)
		&& ((current_time - previous_time) / 1000 >= (1000000 / VL53L8_GET_RANGE_HZ))) {
		previous_time = current_time;
		dbg_actuator("[%s] VL53L8_EXT_GET_RANGE start\n", __func__);
		vl53l8_ext_control(VL53L8_EXT_GET_RANGE, (void *)&laser_af_data, &laser_af_data_size);
		dbg_actuator("[%s] VL53L8_EXT_GET_RANGE end, data_size(%d)\n", __func__, laser_af_data_size);
	} else {
		dbg_actuator("[%s] VL53L8_EXT_GET_RANGE skip\n", __func__);
	}

	spin_unlock_irqrestore(&laser_af->laf_work_lock, flags);
}

static int laser_af_vl53l8_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct is_laser_af *laser_af;

	WARN_ON(!subdev);

	laser_af = (struct is_laser_af *)v4l2_get_subdevdata(subdev);

	WARN_ON(!laser_af);

	dbg_actuator("[%s] E\n", __func__);

	laser_af_data_size = 0;
	previous_time = 0;
	laser_af_start_time = 0;

	spin_lock_init(&laser_af->laf_work_lock);

	laser_af->workqueue = alloc_workqueue("vl53l8_workqueue", WQ_HIGHPRI | WQ_UNBOUND, 0);

	INIT_WORK(&laser_af->start_work, laser_af_vl53l8_start_work);
	INIT_WORK(&laser_af->stop_work, laser_af_vl53l8_stop_work);
	INIT_WORK(&laser_af->get_distance_work, laser_af_vl53l8_get_distance_work);

	dbg_actuator("[%s] X\n", __func__);

	return ret;
}

static void laser_af_vl53l8_work_deinit(struct is_laser_af *laser_af)
{
	WARN_ON(!laser_af);

	dbg_actuator("[%s] E\n", __func__);

	laser_af_data_size = 0;
	previous_time = 0;
	laser_af_start_time = 0;

	flush_work(&laser_af->start_work);
	cancel_work_sync(&laser_af->start_work);
	flush_work(&laser_af->get_distance_work);
	cancel_work_sync(&laser_af->get_distance_work);
	flush_work(&laser_af->stop_work);
	cancel_work_sync(&laser_af->stop_work);

	destroy_workqueue(laser_af->workqueue);

	dbg_actuator("[%s] X\n", __func__);
}

static int laser_af_vl53l8_set_active(struct v4l2_subdev *subdev, bool is_active)
{
	int ret = 0;
	struct is_laser_af *laser_af;
	struct is_core *core = pablo_get_core_async();

	WARN_ON(!subdev);

	laser_af = (struct is_laser_af *)v4l2_get_subdevdata(subdev);

	if (is_active) {
		if (!laser_af->active) {
			atomic_inc(&core->laser_refcount);

			if (atomic_read(&core->laser_refcount) == 1) {
				laser_af_vl53l8_init(subdev, 0);
				queue_work(laser_af->workqueue, &laser_af->start_work);
			}
		}
	} else {
		if (laser_af->active) {
			atomic_dec(&core->laser_refcount);

			if (atomic_read(&core->laser_refcount) == 0) {
				laser_af_start_time = 0;
				flush_work(&laser_af->get_distance_work);
				queue_work(laser_af->workqueue, &laser_af->stop_work);
				laser_af_vl53l8_work_deinit(laser_af);
			}
		}
	}

	return ret;
}

static int laser_af_vl53l8_get_distance(struct v4l2_subdev *subdev, void *data, u32 *size)
{
	int ret = 0;
	struct is_laser_af *laser_af;
	unsigned long flags;

	WARN_ON(!subdev);

	laser_af = (struct is_laser_af *)v4l2_get_subdevdata(subdev);

	WARN_ON(!laser_af);

	spin_lock_irqsave(&laser_af->laf_work_lock, flags);
	memcpy(data, &laser_af_data, laser_af_data_size);
	*size = laser_af_data_size;
	dbg_actuator("[%s] depth16[0](%d), data_size(%d)\n", __func__, laser_af_data.depth16[0], laser_af_data_size);
	spin_unlock_irqrestore(&laser_af->laf_work_lock, flags);

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = laser_af_vl53l8_init,
};

static struct is_laser_af_ops laser_af_ops = {
	.set_active = laser_af_vl53l8_set_active,
	.get_distance = laser_af_vl53l8_get_distance,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

static int laser_af_vl53l8_probe(struct device *dev)
{
	int ret = 0;
	struct is_core *core;
	struct v4l2_subdev *subdev_laser_af = NULL;
	struct is_device_sensor *device;
	struct is_laser_af *laser_af = NULL;
	struct device_node *dnode;
	const u32 *sensor_id_spec;
	u32 sensor_id_len;
	u32 sensor_id[IS_SENSOR_COUNT];
	int i;

	WARN_ON(!dev);

	dnode = dev->of_node;

	core = pablo_get_core_async();
	if (!core) {
		probe_info("core device is not yet probed\n");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec || sensor_id_len == 0) {
		err("sensor_id num read is fail(%d), sensor_id_len(%d)", ret, sensor_id_len);
		goto p_err;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		if (!device) {
			err("sensor device is NULL");
			ret = -EPROBE_DEFER;
			goto p_err;
		}
	}

	laser_af = kzalloc(sizeof(struct is_laser_af), GFP_KERNEL);
	if (!laser_af) {
		err("laser_af is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_laser_af = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_laser_af) {
		err("subdev_laser_af is NULL");
		ret = -ENOMEM;
		kfree(laser_af);
		goto p_err;
	}

	laser_af->id = LASER_AF_NAME_VL53L8;
	laser_af->subdev = subdev_laser_af;
	laser_af->laser_af_ops = &laser_af_ops;
	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		device->subdev_laser_af = subdev_laser_af;
		device->laser_af = laser_af;
		v4l2_subdev_init(subdev_laser_af, &subdev_ops);
		v4l2_set_subdev_hostdata(subdev_laser_af, device);
	}

	v4l2_set_subdevdata(subdev_laser_af, laser_af);
	snprintf(subdev_laser_af->name, V4L2_SUBDEV_NAME_SIZE,
				"laser_af-subdev.%d", laser_af->id);

	probe_info("%s done\n", __func__);

	return ret;

p_err:
	if (laser_af)
		kfree(laser_af);

	if (subdev_laser_af)
		kfree(subdev_laser_af);

	return ret;
}

static int laser_af_vl53l8_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	WARN_ON(!pdev);

	dev = &pdev->dev;

	ret = laser_af_vl53l8_probe(dev);
	if (ret < 0) {
		probe_err("laser_af vl53l8 probe fail(%d)", ret);
		goto p_err;
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static const struct of_device_id exynos_is_laser_af_vl53l8_match[] = {
	{
		.compatible = "samsung,laser-af-vl53l8",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_is_laser_af_vl53l8_match);

/* register platform driver */
static struct platform_driver sensor_laser_af_vl53l8_platform_driver = {
	.probe = laser_af_vl53l8_platform_probe,
	.driver = {
		.name   = "IS-LASER-AF-VL53L8-PLATFORM",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_is_laser_af_vl53l8_match,
	}
};

#ifdef MODULE
static int laser_af_vl53l8_module_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&sensor_laser_af_vl53l8_platform_driver);
	if (ret) {
		err("sensor_laser_af_vl53l8_platform_driver register failed(%d)", ret);
		return ret;
	}

	return ret;
}

static void laser_af_vl53l8_module_exit(void)
{
	platform_driver_unregister(&sensor_laser_af_vl53l8_platform_driver);
}
module_init(laser_af_vl53l8_module_init);
module_exit(laser_af_vl53l8_module_exit);
#else
static int __init _laser_af_vl53l8_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_laser_af_vl53l8_platform_driver,
				laser_af_vl53l8_platform_probe);
	if (ret)
		err("failed to probe %s driver: %d",
			sensor_laser_af_vl53l8_platform_driver.driver.name, ret);

	return ret;
}

late_initcall_sync(_laser_af_vl53l8_init);
#endif

MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: fimc-is");
