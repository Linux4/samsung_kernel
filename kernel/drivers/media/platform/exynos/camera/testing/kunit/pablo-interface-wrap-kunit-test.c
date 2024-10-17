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

#include "pablo-kunit-test.h"
#include "is-interface-wrap.h"

static struct pablo_itf_wrap_test_ctx {
	struct is_group group;
	struct is_device_ischain idi;
	struct is_device_sensor sensor;
	struct is_sensor_cfg cfg;
	struct is_frame frame;
} pkt_ctx;

static struct pablo_kunit_itf_wrap_ops *pkt_ops;

static void pablo_itf_wrap_sudden_stop_kunit_test(struct kunit *test)
{
	struct is_device_ischain *device;
	struct is_group *group = &pkt_ctx.group;
	struct is_core *core = is_get_is_core();
	struct is_device_sensor *org_sensor, *tmp_sensor;
	u32 instance = 0;

	org_sensor = &core->sensor[0];
	tmp_sensor = kunit_kzalloc(test, sizeof(struct is_device_sensor), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, tmp_sensor);
	memcpy(tmp_sensor, org_sensor, sizeof(struct is_device_sensor));

	device = NULL;
	pkt_ops->sudden_stop(device, instance, group);

	device = &pkt_ctx.idi;
	device->sensor = NULL;
	pkt_ops->sudden_stop(device, instance, group);

	device->sensor = &pkt_ctx.sensor;
	device->sensor->private_data = NULL;
	pkt_ops->sudden_stop(device, instance, group);

	device->sensor->private_data = core;
	set_bit(IS_SENSOR_FRONT_START, &device->sensor->state);
	set_bit(IS_SENSOR_FRONT_START, &org_sensor->state);
	org_sensor->ischain = NULL;
	group->head = &pkt_ctx.group;
	pkt_ops->sudden_stop(device, instance, group);

	/* Restore */
	memcpy(org_sensor, tmp_sensor, sizeof(struct is_device_sensor));
}

static void pablo_itf_wrap_change_chain_kunit_test(struct kunit *test)
{
	struct is_group *group = &pkt_ctx.group;
	u32 next_id = 1;
	int test_result;

	test_result = pkt_ops->change_chain(group, next_id);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_itf_wrap_sensor_mode_kunit_test(struct kunit *test)
{
	struct is_device_ischain *device = &pkt_ctx.idi;
	struct is_sensor_cfg *cfg = &pkt_ctx.cfg;
	int test_result;

	cfg->mode = SENSOR_MODE_DEINIT;
	test_result = pkt_ops->sensor_mode(device, cfg);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_itf_wrap_shot_kunit_test(struct kunit *test)
{
	struct is_group *group = &pkt_ctx.group;
	struct is_frame *frame = &pkt_ctx.frame;
	int test_result;

	group->head = &pkt_ctx.group;
	group->hw_grp_ops = is_hw_get_m2m_group_ops();
	frame->hw_slot_id[0] = HW_SLOT_MAX;

	test_result = pkt_ops->shot(group, frame);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static int pablo_itf_wrap_kunit_test_init(struct kunit *test)
{
	memset(&pkt_ctx, 0, sizeof(pkt_ctx));
	pkt_ops = pablo_kunit_get_itf_wrap();

	return 0;
}

static void pablo_itf_wrap_kunit_test_exit(struct kunit *test)
{
	pkt_ops = NULL;
}

static struct kunit_case pablo_itf_wrap_kunit_test_cases[] = {
	KUNIT_CASE(pablo_itf_wrap_sudden_stop_kunit_test),
	KUNIT_CASE(pablo_itf_wrap_change_chain_kunit_test),
	KUNIT_CASE(pablo_itf_wrap_sensor_mode_kunit_test),
	KUNIT_CASE(pablo_itf_wrap_shot_kunit_test),
	{},
};

struct kunit_suite pablo_itf_wrap_kunit_test_suite = {
	.name = "pablo-itf-wrap-kunit-test",
	.init = pablo_itf_wrap_kunit_test_init,
	.exit = pablo_itf_wrap_kunit_test_exit,
	.test_cases = pablo_itf_wrap_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_itf_wrap_kunit_test_suite);

struct kunit_suite pablo_itf_wrap_kunit_test_suite_end = {
	.name = "pablo-itf-wrap-kunit-test-end",
	.test_cases = NULL,
};
define_pablo_kunit_test_suites_end(&pablo_itf_wrap_kunit_test_suite_end);
MODULE_LICENSE("GPL");
