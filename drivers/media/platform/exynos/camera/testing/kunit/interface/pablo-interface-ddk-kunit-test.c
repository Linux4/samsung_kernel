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
#include "is-interface-ddk.h"

static struct test_ctx {
	struct is_hw_ip hw_ip;
	struct is_hardware hardware;
	struct is_framemgr framemgr;
} _test_ctx;

static int frame_ndone(struct is_hw_ip *ldr_hw_ip,
		struct is_frame *frame, enum ShotErrorType done_type)
{
	ulong flags = 0;

	framemgr_e_barrier_common((&_test_ctx.framemgr), 0, flags);
	frame = peek_frame(&_test_ctx.framemgr, FS_HW_WAIT_DONE);
	trans_frame(&_test_ctx.framemgr, frame, FS_FREE);
	framemgr_x_barrier_common((&_test_ctx.framemgr), 0, flags);

	return 0;
}

static void pablo_interface_ddk_camera_callback_config_lock_kunit_test(struct kunit *test)
{
	enum lib_cb_event_type event_id;
	u32 instance_id = 0;
	ulong fcount = 123;

	_test_ctx.hw_ip.hardware = &_test_ctx.hardware;
	_test_ctx.hw_ip.framemgr = &_test_ctx.framemgr;
	init_waitqueue_head(&_test_ctx.hw_ip.status.wait_queue);

	event_id = LIB_EVENT_CONFIG_LOCK;
	is_lib_camera_callback(&_test_ctx.hw_ip, event_id, instance_id, &fcount);
}

static void pablo_interface_ddk_camera_callback_frame_start_isr_kunit_test(struct kunit *test)
{
	enum lib_cb_event_type event_id;
	u32 instance_id = 0;
	ulong fcount = 123;

	_test_ctx.hw_ip.hardware = &_test_ctx.hardware;
	_test_ctx.hw_ip.framemgr = &_test_ctx.framemgr;
	init_waitqueue_head(&_test_ctx.hw_ip.status.wait_queue);

	event_id = LIB_EVENT_FRAME_START_ISR;
	is_lib_camera_callback(&_test_ctx.hw_ip, event_id, instance_id, &fcount);
}

static void pablo_interface_ddk_camera_callback_frame_end_kunit_test(struct kunit *test)
{
	int ret;
	enum lib_cb_event_type event_id;
	u32 instance_id = 0;
	ulong fcount = 123;
	struct is_hardware_ops hw_ops = { 0, };
	struct is_frame *frame;
	ulong flags = 0;

	_test_ctx.hw_ip.hardware = &_test_ctx.hardware;
	_test_ctx.hw_ip.framemgr = &_test_ctx.framemgr;
	init_waitqueue_head(&_test_ctx.hw_ip.status.wait_queue);

	event_id = LIB_EVENT_FRAME_END;
	hw_ops.frame_ndone = frame_ndone;
	_test_ctx.hw_ip.hw_ops = &hw_ops;

	ret = frame_manager_open(&_test_ctx.framemgr, 1);
	KUNIT_EXPECT_EQ(test, ret, 0);

	framemgr_e_barrier_common((&_test_ctx.framemgr), 0, flags);
	frame = peek_frame(&_test_ctx.framemgr, FS_FREE);
	frame->fcount = 122;
	trans_frame(&_test_ctx.framemgr, frame, FS_HW_WAIT_DONE);
	framemgr_x_barrier_common((&_test_ctx.framemgr), 0, flags);

	is_lib_camera_callback(&_test_ctx.hw_ip, event_id, instance_id, &fcount);

	framemgr_e_barrier_common((&_test_ctx.framemgr), 0, flags);
	frame = peek_frame(&_test_ctx.framemgr, FS_FREE);
	frame->fcount = 124;
	trans_frame(&_test_ctx.framemgr, frame, FS_HW_WAIT_DONE);
	framemgr_x_barrier_common((&_test_ctx.framemgr), 0, flags);

	is_lib_camera_callback(&_test_ctx.hw_ip, event_id, instance_id, &fcount);
	ret = CALL_HW_OPS(&_test_ctx.hw_ip, frame_ndone, &_test_ctx.hw_ip, frame,
			IS_SHOT_INVALID_FRAMENUMBER);
	KUNIT_EXPECT_EQ(test, ret, 0);

	framemgr_e_barrier_common((&_test_ctx.framemgr), 0, flags);
	frame = peek_frame(&_test_ctx.framemgr, FS_FREE);
	frame->type = SHOT_TYPE_LATE;
	trans_frame(&_test_ctx.framemgr, frame, FS_HW_WAIT_DONE);
	framemgr_x_barrier_common((&_test_ctx.framemgr), 0, flags);

	is_lib_camera_callback(&_test_ctx.hw_ip, event_id, instance_id, &fcount);

	framemgr_e_barrier_common((&_test_ctx.framemgr), 0, flags);
	frame = peek_frame(&_test_ctx.framemgr, FS_FREE);
	frame->fcount = 122;
	atomic_set(&_test_ctx.hw_ip.fcount, 123);
	trans_frame(&_test_ctx.framemgr, frame, FS_HW_CONFIGURE);
	framemgr_x_barrier_common((&_test_ctx.framemgr), 0, flags);

	is_lib_camera_callback(&_test_ctx.hw_ip, event_id, instance_id, &fcount);

	framemgr_e_barrier_common((&_test_ctx.framemgr), 0, flags);
	frame = peek_frame(&_test_ctx.framemgr, FS_FREE);
	frame->fcount = 123;
	atomic_set(&_test_ctx.hw_ip.fcount, 123);
	trans_frame(&_test_ctx.framemgr, frame, FS_HW_CONFIGURE);
	framemgr_x_barrier_common((&_test_ctx.framemgr), 0, flags);

	is_lib_camera_callback(&_test_ctx.hw_ip, event_id, instance_id, &fcount);

	ret = frame_manager_close(&_test_ctx.framemgr);
	KUNIT_EXPECT_EQ(test, ret, 0);

	is_lib_camera_callback(&_test_ctx.hw_ip, event_id, instance_id, &fcount);
}

static void pablo_interface_ddk_camera_callback_config_lock_delay_kunit_test(struct kunit *test)
{
	enum lib_cb_event_type event_id;
	u32 instance_id = 0;
	ulong fcount = 123;

	_test_ctx.hw_ip.hardware = &_test_ctx.hardware;
	_test_ctx.hw_ip.framemgr = &_test_ctx.framemgr;
	init_waitqueue_head(&_test_ctx.hw_ip.status.wait_queue);

	event_id = LIB_EVENT_ERROR_CONFIG_LOCK_DELAY;
	is_lib_camera_callback(&_test_ctx.hw_ip, event_id, instance_id, &fcount);
}

static int pablo_interface_ddk_kunit_test_init(struct kunit *test)
{
	memset(&_test_ctx, 0, sizeof(_test_ctx));

	return 0;
}

static void pablo_interface_ddk_kunit_test_exit(struct kunit *test)
{
}

static struct kunit_case pablo_interface_ddk_kunit_test_cases[] = {
	KUNIT_CASE(pablo_interface_ddk_camera_callback_config_lock_kunit_test),
	KUNIT_CASE(pablo_interface_ddk_camera_callback_frame_start_isr_kunit_test),
	KUNIT_CASE(pablo_interface_ddk_camera_callback_frame_end_kunit_test),
	KUNIT_CASE(pablo_interface_ddk_camera_callback_config_lock_delay_kunit_test),
	{},
};

struct kunit_suite pablo_interface_ddk_kunit_test_suite = {
	.name = "pablo-interface-ddk-kunit-test",
	.init = pablo_interface_ddk_kunit_test_init,
	.exit = pablo_interface_ddk_kunit_test_exit,
	.test_cases = pablo_interface_ddk_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_interface_ddk_kunit_test_suite);

MODULE_LICENSE("GPL");
