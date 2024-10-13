/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "is-device-sensor-peri.h"
#include "is-vender-test-sensor.h"

#ifdef USE_SENSOR_DEBUG

/* Sensor Test with ADB Commend */
static int test_set_adaptive_mipi_mode(const char *val, const struct kernel_param *kp)
{
	int ret, position, argc, adaptive_mipi_mode;
	char **argv;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module;
	struct is_cis *cis;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &position);
	ret = kstrtouint(argv[1], 0, &adaptive_mipi_mode);

	is_vendor_get_module_from_position(position, &module);
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	cis = &sensor_peri->cis;
	WARN_ON(!cis);

	if (cis->mipi_sensor_mode_size == 0) {
		info("[%s][%d] adaptive mipi is not supported", __func__, position);
	} else {
		info("[%s][%d] adaptive_mipi is %s", __func__,
			position, (adaptive_mipi_mode == 1) ? "disabled" : "enabled");
		if (adaptive_mipi_mode == 1)
			cis->vendor_use_adaptive_mipi = false;
		else
			cis->vendor_use_adaptive_mipi = true;
	}

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_mipi_mode = {
	.set = test_set_adaptive_mipi_mode,
	.get = NULL,
};

/**
 * Command : adb shell "echo 0 1 > /sys/module/fimc_is/parameters/test_mipi_mode"
 * @param 0 Select sensor position
 * @param 1 Select adaptive mipi mode : Disable[1]
 */
module_param_cb(test_mipi_mode, &param_ops_test_mipi_mode, NULL, 0644);

static int test_set_seamless_mode(const char *val, const struct kernel_param *kp)
{
	int ret, position, argc, zoom_ratio;
	char **argv;
	struct is_device_sensor_peri *sensor_peri = NULL;
	struct is_module_enum *module;
	struct is_cis *cis;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		err("No argument!");
		return -EINVAL;
	}

	ret = kstrtouint(argv[0], 0, &position);
	info("[%s] sensor position %d", __func__, position);

	ret = kstrtouint(argv[1], 0, &zoom_ratio);
	info("[%s] zoom_ratio %d", __func__, zoom_ratio);

	is_vendor_get_module_from_position(position, &module);
	WARN_ON(!module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	WARN_ON(!sensor_peri);

	cis = &sensor_peri->cis;
	WARN_ON(!cis);
	WARN_ON(!cis->cis_data);

	cis->cis_data->cur_remosaic_zoom_ratio = zoom_ratio;
	//cis->cis_data->cur_lownoise_mode = ln;

	ret = CALL_CISOPS(cis, cis_update_seamless_mode, cis->subdev);

	argv_free(argv);
	return ret;
}

static const struct kernel_param_ops param_ops_test_seamless_mode = {
	.set = test_set_seamless_mode,
	.get = NULL,
};

/**
 * Command : adb shell "echo 0 1 > /sys/module/fimc_is/parameters/test_seamless_mode"
 * @param 0 Select sensor position
 * @param 1 Select cropped remosaic mode(zoom ratio) : Croped_zoom[20]
 */
module_param_cb(test_seamless_mode, &param_ops_test_seamless_mode, NULL, 0644);
#endif
