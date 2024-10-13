// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <exynos-is-sensor.h>

#include "pablo-kunit-test.h"

#include "is-device-sensor-peri.h"
#include "exynos-is-module.h"
#include "is-ixc-config.h"

static struct pablo_kunit_ixc_ops *ixc_func;
static struct pablo_ixc_test_ctx {
	struct is_module_enum module;
	struct is_device_sensor_peri sensor_peri;
	struct v4l2_subdev subdev;
	struct is_device_sensor device;
	struct is_ois ois;
	struct is_aperture aperture;
	struct is_mcu mcu;
	struct exynos_platform_is_module pdata;
	struct is_actuator actuator;
} pkt_ctx;

/* Define the test cases. */
static void pablo_ixc_pin_control_kunit_test(struct kunit *test)
{
	struct is_module_enum *module = &pkt_ctx.module;
	u32 scenario;
	int test_result;
	struct is_core *core = is_get_is_core();
	u32 pre_rsccount, rsccount;
	int i;

	module->private_data = (void *)&pkt_ctx.sensor_peri;
	module->subdev = &pkt_ctx.subdev;
	module->subdev->host_priv = (void *)&pkt_ctx.device;

	pkt_ctx.device.ois = &pkt_ctx.ois;
	pkt_ctx.device.aperture = &pkt_ctx.aperture;
	pkt_ctx.device.mcu = &pkt_ctx.mcu;

	scenario = SENSOR_SCENARIO_NORMAL;
	module->pdata = &pkt_ctx.pdata;
	module->pdata->ois_i2c_ch = 0;

	pre_rsccount = atomic_read(&core->ixc_rsccount[0]);
	test_result = ixc_func->pin_control(module, scenario, 1);
	rsccount = atomic_read(&core->ixc_rsccount[0]);
	KUNIT_EXPECT_LT(test, pre_rsccount, rsccount);
	if (rsccount > pre_rsccount)
		atomic_dec(&core->ixc_rsccount[0]);

	scenario = SENSOR_SCENARIO_OIS_FACTORY;
	for (i = 0; i < ACTUATOR_MAX_ENUM; i++) {
		pkt_ctx.device.actuator[i] = &pkt_ctx.actuator;
	}
	test_result = ixc_func->pin_control(module, scenario, 1);
	KUNIT_EXPECT_EQ(test, test_result, 0);

	scenario = SENSOR_SCENARIO_READ_ROM;
	test_result = ixc_func->pin_control(module, scenario, 1);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static int pablo_ixc_kunit_test_init(struct kunit *test)
{
	ixc_func = pablo_kunit_get_ixc();

	memset(&pkt_ctx, 0, sizeof(pkt_ctx));

	return 0;
}

static void pablo_ixc_kunit_test_exit(struct kunit *test)
{
	ixc_func = NULL;
}

static struct kunit_case pablo_ixc_config_kunit_test_cases[] = {
	KUNIT_CASE(pablo_ixc_pin_control_kunit_test),
	{},
};

struct kunit_suite pablo_ixc_config_kunit_test_suite = {
	.name = "pablo-ixc-config-kunit-test",
	.init = pablo_ixc_kunit_test_init,
	.exit = pablo_ixc_kunit_test_exit,
	.test_cases = pablo_ixc_config_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_ixc_config_kunit_test_suite);

MODULE_LICENSE("GPL");
