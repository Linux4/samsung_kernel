// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-kunit-test.h"
#include "pablo-kunit-test-ixc.h"

#include "pablo-sensor-adapter.h"
#include "is-device-sensor-peri.h"

#define CALLBACK_TIMEOUT			(HZ / 4)

static struct is_ixc_ops pkt_ixc_ops = {
	.addr8_read8 = pkt_ixc_addr8_read8,
	.read8 = pkt_ixc_read8,
	.read16 = pkt_ixc_read16,
	.read8_size = pkt_ixc_read8_size,
	.addr8_write8 = pkt_ixc_addr8_write8,
	.write8 = pkt_ixc_write8,
	.write8_array = pkt_ixc_write8_array,
	.write16 = pkt_ixc_write16,
	.write16_array = pkt_ixc_write16_array,
	.write16_burst = pkt_ixc_write16_burst,
	.write8_sequential = pkt_ixc_write8_sequential,
	.data_read16 = pkt_ixc_data_read16,
	.data_write16 = pkt_ixc_data_write16,
	.addr_data_write16 = pkt_ixc_addr_data_write16,
};

/* Define the test cases. */
static struct test_ctx {
	struct is_device_sensor_peri	*s_peri;
	struct pablo_sensor_adt		sensor_adt;
	struct is_sensor_interface	*sensor_interface;
	struct is_sensor_cfg		sensor_cfg;
	struct is_sensor_cfg		*backup_sensor_cfg;
	atomic_t			callback;
	wait_queue_head_t		callback_queue;
	u32				callback_ret;
	struct is_ixc_ops		*cis_ixc_ops;
	struct is_ixc_ops		*act_ixc_ops;
	struct is_ixc_ops		*fl_ixc_ops;
	struct is_ixc_ops		*apt_ixc_ops;
	struct is_ixc_ops		*ep_ixc_ops;
	u32				scenario;
} pablo_sensor_adt_test_ctx;

static void pablo_sensor_adt_v1_callback(void *caller, u32 instance, u32 ret, dma_addr_t dva, u32 idx)
{
	pablo_sensor_adt_test_ctx.callback_ret = ret;
	atomic_set(&pablo_sensor_adt_test_ctx.callback, 1);
	wake_up(&pablo_sensor_adt_test_ctx.callback_queue);
}

static void pablo_sensor_adt_v1_open_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	/* error case */
	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, -ENODEV, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, NULL);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	/* 1st open */
	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* 2nd open */
	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_update_actuator_info_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, update_actuator_info, true);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, update_actuator_info, false);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_update_actuator_info_negative_kunit_test(struct kunit *test)
{
	int ret = 0;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;

	ret = CALL_SS_ADT_OPS(sensor_adt, update_actuator_info, true);
	KUNIT_EXPECT_EQ(test, -ENODEV, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, update_actuator_info, false);
	KUNIT_EXPECT_EQ(test, -ENODEV, ret);
}

static void pablo_sensor_adt_v1_get_sensor_info_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_crta_sensor_info pcsi;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, &pcsi);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_get_sensor_info_negative_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, NULL);
	KUNIT_EXPECT_EQ(test, -ENODEV, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, NULL);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_control_sensor_negative_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	/* control sensor before opening sensor_adt */
	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, -ENODEV, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* control_sensor without start case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_REQUEST_EXPOSURE;
	psci.sensor_control[2] = 3;
	psci.sensor_control[3] = 10000;
	psci.sensor_control[4] = 20000;
	psci.sensor_control[5] = 30000;
	psci.sensor_control[6] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* magic number error */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	/* control id error */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	/* invalid argument error */
	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, NULL, 0, 0);
	KUNIT_EXPECT_EQ(test, -EINVAL, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_request_exp_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_REQUEST_EXPOSURE;
	psci.sensor_control[2] = 3;
	psci.sensor_control[3] = 10000;
	psci.sensor_control[4] = 20000;
	psci.sensor_control[5] = 30000;
	psci.sensor_control[6] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[6] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_request_gain_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_REQUEST_GAIN;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = 1000;
	psci.sensor_control[4] = 1000;
	psci.sensor_control[5] = 1000;
	psci.sensor_control[6] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[6] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_alg_reset_flag_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_SET_ALG_RESET_FLAG;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_num_of_frame_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_SET_NUM_OF_FRAME_PER_ONE_RTA;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_apply_sensor_setting_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_APPLY_SENSOR_SETTING;
	psci.sensor_control[2] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[2] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_reqeust_reset_expo_gain_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_REQUEST_RESET_EXPO_GAIN;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = 30000;
	psci.sensor_control[4] = 1000;
	psci.sensor_control[5] = 1000;
	psci.sensor_control[6] = 1000;
	psci.sensor_control[7] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[7] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_stream_off_on_mode_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_SET_STREAM_OFF_ON_MODE;
	psci.sensor_control[2] = 30000;
	psci.sensor_control[3] = 30000;
	psci.sensor_control[4] = 1000;
	psci.sensor_control[5] = 1000;
	psci.sensor_control[6] = 1000;
	psci.sensor_control[7] = 1000;
	psci.sensor_control[8] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[8] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_sensor_info_mode_change_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_SET_SENSOR_INFO_MODE_CHANGE;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = 30000;
	psci.sensor_control[4] = 1000;
	psci.sensor_control[5] = 1000;
	psci.sensor_control[6] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[6] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_sensor_info_mfhdr_mode_change_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_REQUEST_SENSOR_INFO_MFHDR_MODE_CHANGE;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = 30000;
	psci.sensor_control[4] = 1000;
	psci.sensor_control[5] = 1000;
	psci.sensor_control[6] = 0;
	psci.sensor_control[7] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[7] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_adjust_sync_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_ADJUST_SYNC;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_request_frame_length_line_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_REQUEST_FRAME_LENGTH_LINE;
	psci.sensor_control[2] = 10000;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_request_sensitivity_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_REQUEST_SENSITIVITY;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_sensor_12bit_state_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_SET_12BIT_STATE;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_low_noise_mode_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_SET_NOISE_MODE;
	psci.sensor_control[2] = 0;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_request_wb_gain_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* noraml case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_REQUEST_WB_GAIN;
	psci.sensor_control[2] = 1000;
	psci.sensor_control[3] = 1000;
	psci.sensor_control[4] = 1000;
	psci.sensor_control[5] = 1000;
	psci.sensor_control[6] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[6] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback),
				CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_mainflash_duration_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_crta_sensor_info pcsi;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, &pcsi);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
		test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_SET_MAINFLASH_DURATION;
	psci.sensor_control[2] = 1;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_request_direct_flash_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_crta_sensor_info pcsi;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, &pcsi);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_REQUEST_DIRECT_FLASH;
	psci.sensor_control[2] = 0;
	psci.sensor_control[3] = 0;
	psci.sensor_control[4] = 0;
	psci.sensor_control[5] = 0;
	psci.sensor_control[6] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);

	/* error case */
	psci.sensor_control[6] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS( sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_hdr_mode_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_SET_HDR_MODE;
	psci.sensor_control[2] = 0;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_position_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_crta_sensor_info pcsi;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, &pcsi);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_ACTUATOR_SET_POSITION;
	psci.sensor_control[2] = 0;
	psci.sensor_control[3] = 0;
	psci.sensor_control[4] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);

	/* error case */
	psci.sensor_control[4] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_soft_landing_config_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_crta_sensor_info pcsi;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, &pcsi);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_CIS_SET_SOFT_LANDING_CONFIG;
	psci.sensor_control[2] = 0;
	psci.sensor_control[3] = 1;
	psci.sensor_control[4] = 0;
	psci.sensor_control[5] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);

	/* error case */
	psci.sensor_control[5] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_request_flash_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_crta_sensor_info pcsi;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, &pcsi);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_FLASH_REQUEST_FLASH;
	psci.sensor_control[2] = 0;
	psci.sensor_control[3] = 0;
	psci.sensor_control[4] = 0;
	psci.sensor_control[5] = 0;
	psci.sensor_control[6] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);

	/* error case */
	psci.sensor_control[6] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_request_flash_expo_gain_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_crta_sensor_info pcsi;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, &pcsi);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_FLASH_REQUEST_EXPO_GAIN;
	psci.sensor_control[2] = 30000;
	psci.sensor_control[3] = 30000;
	psci.sensor_control[4] = 1000;
	psci.sensor_control[5] = 1000;
	psci.sensor_control[6] = 1000;
	psci.sensor_control[7] = 1000;
	psci.sensor_control[8] = 1000;
	psci.sensor_control[9] = 1000;
	psci.sensor_control[10] = 30000;
	psci.sensor_control[11] = 30000;
	psci.sensor_control[12] = 1000;
	psci.sensor_control[13] = 1000;
	psci.sensor_control[14] = 1000;
	psci.sensor_control[15] = 1000;
	psci.sensor_control[16] = 1000;
	psci.sensor_control[17] = 1000;
	psci.sensor_control[18] = 30000;
	psci.sensor_control[19] = 30000;
	psci.sensor_control[20] = 1000;
	psci.sensor_control[21] = 1000;
	psci.sensor_control[22] = 1000;
	psci.sensor_control[23] = 1000;
	psci.sensor_control[24] = 1000;
	psci.sensor_control[25] = 1000;
	psci.sensor_control[26] = 0;
	psci.sensor_control[27] = 0;
	psci.sensor_control[28] = 0;
	psci.sensor_control[29] = 0;
	psci.sensor_control[30] = 0;
	psci.sensor_control[31] = 0;
	psci.sensor_control[32] = 0;
	psci.sensor_control[34] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);

	/* error case */
	psci.sensor_control[34] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static void pablo_sensor_adt_v1_set_aperture_value_kunit_test(struct kunit *test)
{
	int ret = 0;
	u32 instance;
	struct test_ctx *test_ctx;
	struct pablo_sensor_adt *sensor_adt;
	struct pablo_crta_sensor_info pcsi;
	struct pablo_sensor_control_info psci;

	test_ctx = &pablo_sensor_adt_test_ctx;
	sensor_adt = &test_ctx->sensor_adt;
	instance = 0;

	ret = CALL_SS_ADT_OPS(sensor_adt, open, instance, test_ctx->sensor_interface);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, register_control_sensor_callback,
			      test, pablo_sensor_adt_v1_callback);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, get_sensor_info, &pcsi);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, start);
	KUNIT_EXPECT_EQ(test, 0, ret);

	/* normal case */
	psci.sensor_control_size = 1;
	psci.sensor_control[0] = PABLO_CRTA_MAGIC_NUMBER;
	psci.sensor_control[1] = SS_CTRL_APERTURE_SET_APERTURE_VALUE;
	psci.sensor_control[2] = 0;
	psci.sensor_control[3] = SS_CTRL_END;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, 0);

	/* error case */
	psci.sensor_control[3] = 0;

	atomic_set(&test_ctx->callback, 0);

	ret = CALL_SS_ADT_OPS(sensor_adt, control_sensor, &psci, 0, 0);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = wait_event_timeout(test_ctx->callback_queue,
				atomic_read(&test_ctx->callback), CALLBACK_TIMEOUT);
	KUNIT_EXPECT_NE(test, 0, ret);
	KUNIT_EXPECT_EQ(test, test_ctx->callback_ret, -EINVAL);

	ret = CALL_SS_ADT_OPS(sensor_adt, stop);
	KUNIT_EXPECT_EQ(test, 0, ret);

	ret = CALL_SS_ADT_OPS(sensor_adt, close);
	KUNIT_EXPECT_EQ(test, 0, ret);
}

static struct kunit_case pablo_sensor_adt_v1_kunit_test_cases[] = {
	KUNIT_CASE(pablo_sensor_adt_v1_open_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_update_actuator_info_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_update_actuator_info_negative_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_get_sensor_info_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_get_sensor_info_negative_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_control_sensor_negative_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_request_exp_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_request_gain_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_alg_reset_flag_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_num_of_frame_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_apply_sensor_setting_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_reqeust_reset_expo_gain_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_stream_off_on_mode_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_sensor_info_mode_change_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_sensor_info_mfhdr_mode_change_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_adjust_sync_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_request_frame_length_line_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_request_sensitivity_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_sensor_12bit_state_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_low_noise_mode_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_request_wb_gain_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_mainflash_duration_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_request_direct_flash_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_hdr_mode_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_position_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_soft_landing_config_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_request_flash_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_request_flash_expo_gain_kunit_test),
	KUNIT_CASE(pablo_sensor_adt_v1_set_aperture_value_kunit_test),
	{},
};

static int pablo_sensor_adt_v1_kunit_test_init(struct kunit *test)
{
	int ret;
	u32 i2c_ch;
	struct is_core *core;
	struct is_device_sensor *sensor;
	struct is_module_enum *module;
	struct v4l2_subdev *subdev_module;
	struct is_device_sensor_peri *sensor_peri;

	/* probe sensor_adt */
	pablo_sensor_adt_probe(&pablo_sensor_adt_test_ctx.sensor_adt);

	/* get sensor_interface */
	core = is_get_is_core();

	sensor = &core->sensor[RESOURCE_TYPE_SENSOR0];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor);

	module = &sensor->module_enum[0];
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, module);

	subdev_module = module->subdev;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, subdev_module);

	sensor_peri = (struct is_device_sensor_peri *)module->private_data;
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sensor_peri);
	pablo_sensor_adt_test_ctx.s_peri = sensor_peri;

	/* This is originaly in the set_input */
	pablo_sensor_adt_test_ctx.backup_sensor_cfg = sensor->cfg;
	pablo_sensor_adt_test_ctx.sensor_cfg.input[0].width = 4032;
	pablo_sensor_adt_test_ctx.sensor_cfg.input[0].height = 3024;
	sensor->cfg = &pablo_sensor_adt_test_ctx.sensor_cfg;
	i2c_ch = module->pdata->sensor_i2c_ch;
	KUNIT_ASSERT_LE(test, i2c_ch, (u32)SENSOR_CONTROL_I2C_MAX);
	sensor_peri->cis.i2c_lock = &core->i2c_lock[i2c_ch];

	/* Hook IXC ops for Kunit test */
	pablo_sensor_adt_test_ctx.cis_ixc_ops = sensor_peri->cis.ixc_ops;
	sensor_peri->cis.ixc_ops = &pkt_ixc_ops;

	if (sensor_peri->actuator)  {
		pablo_sensor_adt_test_ctx.act_ixc_ops = sensor_peri->actuator->ixc_ops;
		sensor_peri->actuator->ixc_ops = &pkt_ixc_ops;
	}

	if (sensor_peri->flash)  {
		pablo_sensor_adt_test_ctx.fl_ixc_ops = sensor_peri->flash->ixc_ops;
		sensor_peri->flash->ixc_ops = &pkt_ixc_ops;
	}

	if (sensor_peri->aperture)  {
		pablo_sensor_adt_test_ctx.apt_ixc_ops = sensor_peri->aperture->ixc_ops;
		sensor_peri->aperture->ixc_ops = &pkt_ixc_ops;
	}

	if (sensor_peri->eeprom)  {
		pablo_sensor_adt_test_ctx.ep_ixc_ops = sensor_peri->eeprom->ixc_ops;
		sensor_peri->eeprom->ixc_ops = &pkt_ixc_ops;
	}

	/* For unit test, sensor vision scenario is enough to minimize the control propagation. */
	pablo_sensor_adt_test_ctx.scenario = sensor->pdata->scenario;
	sensor->pdata->scenario = SENSOR_SCENARIO_VISION;

	sensor->subdev_module = subdev_module;
	ret = v4l2_subdev_call(subdev_module, core, init, 0);
	KUNIT_ASSERT_EQ(test, ret, 0);

	pablo_sensor_adt_test_ctx.sensor_interface = &sensor_peri->sensor_interface;

	/* init waitQ */
	atomic_set(&pablo_sensor_adt_test_ctx.callback, 0);
	init_waitqueue_head(&pablo_sensor_adt_test_ctx.callback_queue);

	return 0;
}

static void pablo_sensor_adt_v1_kunit_test_exit(struct kunit *test)
{
	struct is_core *core;
	struct is_device_sensor *sensor;
	struct is_device_sensor_peri *sensor_peri;

	core = is_get_is_core();
	sensor = &core->sensor[RESOURCE_TYPE_SENSOR0];
	sensor_peri = pablo_sensor_adt_test_ctx.s_peri;

	sensor->cfg = pablo_sensor_adt_test_ctx.backup_sensor_cfg;

	/* Restore IXC ops */
	sensor_peri->cis.ixc_ops = pablo_sensor_adt_test_ctx.cis_ixc_ops;
	if (sensor_peri->actuator)
		sensor_peri->actuator->ixc_ops = pablo_sensor_adt_test_ctx.act_ixc_ops;
	if (sensor_peri->flash)
		sensor_peri->flash->ixc_ops = pablo_sensor_adt_test_ctx.fl_ixc_ops;
	if (sensor_peri->aperture)
		sensor_peri->aperture->ixc_ops = pablo_sensor_adt_test_ctx.apt_ixc_ops;
	if (sensor_peri->eeprom)
		sensor_peri->eeprom->ixc_ops = pablo_sensor_adt_test_ctx.ep_ixc_ops;

	/* Restore sensor scenario */
	sensor->pdata->scenario = pablo_sensor_adt_test_ctx.scenario;
}

struct kunit_suite pablo_sensor_adt_v1_kunit_test_suite = {
	.name = "pablo-sensor_adt_v1-kunit-test",
	.init = pablo_sensor_adt_v1_kunit_test_init,
	.exit = pablo_sensor_adt_v1_kunit_test_exit,
	.test_cases = pablo_sensor_adt_v1_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_sensor_adt_v1_kunit_test_suite);

MODULE_LICENSE("GPL");
