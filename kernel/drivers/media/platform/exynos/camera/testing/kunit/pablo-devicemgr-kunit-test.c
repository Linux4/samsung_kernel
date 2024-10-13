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
#include "is-devicemgr.h"
#include "is-subdev-ctrl.h"
#include "is-err.h"

static struct pablo_kunit_devicemgr_ops *devmgr_ops;
static struct pablo_devicemgr_test_ctx {
	struct is_devicemgr devicemgr;
	struct is_group group;
	struct is_video_ctx vctx;
	struct is_queue queue;
	struct is_frame frame;
	struct is_device_ischain ischain;
	struct is_device_sensor sensor;
	char stm[10];
	struct camera2_shot_ext shot_ext;
	struct is_subdev subdev;
} pkt_ctx;

static int pablo_mock_sensor_group_tag(struct is_device_sensor *device, struct is_frame *frame,
				       struct camera2_node *ldr_node)
{
	set_bit(IS_SENSOR_BACK_START, &device->state);
	return -ENODEV;
}

static struct pablo_device_sensor_ops pkt_dev_sensor_ops = {
	.group_tag = pablo_mock_sensor_group_tag,
};

static void pablo_devicemgr_do_sensor_tag_negative_kunit_test(struct kunit *test)
{
	struct is_devicemgr *devicemgr = &pkt_ctx.devicemgr;
	struct devicemgr_sensor_tag_data *tag_data;
	struct is_group_task *gtask;
	unsigned long state;
	struct is_framemgr *framemgr = &pkt_ctx.queue.framemgr;
	struct is_frame *frame;

	tag_data = &devicemgr->tag_data[0];
	tag_data->fcount = 0;
	tag_data->devicemgr = devicemgr;
	tag_data->stream = 0;
	tag_data->devicemgr->sensor[0] = &pkt_ctx.sensor;
	tag_data->group = &pkt_ctx.group;
	tag_data->group->stm = pkt_ctx.stm;

	set_bit(IS_GROUP_FORCE_STOP, &tag_data->group->state);
	devmgr_ops->sensor_tag((unsigned long)tag_data);

	gtask = get_group_task(tag_data->group->id);
	state = gtask->state;
	clear_bit(IS_GROUP_FORCE_STOP, &tag_data->group->state);
	set_bit(IS_GTASK_REQUEST_STOP, &gtask->state);
	devmgr_ops->sensor_tag((unsigned long)tag_data);

	/* check when framemgr is NULL. */
	clear_bit(IS_GTASK_REQUEST_STOP, &gtask->state);
	devmgr_ops->sensor_tag((unsigned long)tag_data);

	/* check when frame is null. */
	tag_data->group->head = &pkt_ctx.group;
	(tag_data->group->head->leader).vctx = &pkt_ctx.vctx;
	devmgr_ops->sensor_tag((unsigned long)tag_data);

	/* check when return value of group_tag is not zero. */
	frame_manager_probe(framemgr, "KUNIT");
	frame_manager_open(framemgr, 8, false);
	frame = peek_frame(framemgr, FS_FREE);
	trans_frame(framemgr, frame, FS_PROCESS);
	frame->shot_ext = &pkt_ctx.shot_ext;
	clear_bit(IS_SENSOR_BACK_START, &pkt_ctx.sensor.state);
	devmgr_ops->sensor_tag((unsigned long)tag_data);
	KUNIT_EXPECT_EQ(test, test_bit(IS_SENSOR_BACK_START, &pkt_ctx.sensor.state), 1);

	/* restore */
	frame_manager_close(framemgr);

	gtask->state = state;
}

static void pablo_devicemgr_shot_callback_device_sensor_negative_kunit_test(struct kunit *test)
{
	struct is_group *group = &pkt_ctx.group;
	struct is_frame *frame = &pkt_ctx.frame;
	int test_result;

	group->device->sensor = &pkt_ctx.sensor;

	/* M2M state */
	test_result = devmgr_ops->shot_callback(group, frame, 0, IS_DEVICE_SENSOR);
	KUNIT_EXPECT_EQ(test, test_result, -EINVAL);
}

static void pablo_devicemgr_shot_callback_device_ischain_kunit_test(struct kunit *test)
{
	struct is_group *group = &pkt_ctx.group;
	struct is_frame *frame = &pkt_ctx.frame;
	int test_result;

	group->head = &pkt_ctx.group;
	group->head->sensor = &pkt_ctx.sensor;

	test_result = devmgr_ops->shot_callback(group, frame, 0, IS_DEVICE_ISCHAIN);
	KUNIT_EXPECT_EQ(test, test_result, 0);
}

static void pablo_devicemgr_shot_done_kunit_test(struct kunit *test)
{
	struct is_group *group = &pkt_ctx.group;
	struct is_frame *frame = &pkt_ctx.frame;
	int test_result;

	test_result = devmgr_ops->shot_done(group, frame, IS_SHOT_SUCCESS);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);

	group->device_type = IS_DEVICE_SENSOR;
	test_result = devmgr_ops->shot_done(group, frame, IS_SHOT_LATE_FRAME);
	KUNIT_EXPECT_EQ(test, test_result, (int)0);
}

static void pablo_devicemgr_late_shot_handle_kunit_test(struct kunit *test)
{
	struct is_group *group = &pkt_ctx.group;
	struct is_frame *frame = &pkt_ctx.frame;
	struct is_framemgr *framemgr = &pkt_ctx.queue.framemgr;

	/* check when device_type is IS_DEVICE_ISCHAIN */
	group->device_type = IS_DEVICE_ISCHAIN;
	devmgr_ops->late_shot_handle(group, frame, FS_HW_WAIT_DONE);

	/* check when framemgr is NULL */
	group->device_type = IS_DEVICE_SENSOR;
	devmgr_ops->late_shot_handle(group, frame, FS_HW_WAIT_DONE);

	/* check when ldr_frame is NULL */
	group->head = &pkt_ctx.group;
	group->head->leader.vctx = &pkt_ctx.vctx;
	devmgr_ops->late_shot_handle(group, frame, FS_HW_WAIT_DONE);

	/* check when ldr_frame is found */
	frame_manager_probe(framemgr, "KUNIT");
	frame_manager_open(framemgr, 8, false);
	frame = peek_frame(framemgr, FS_FREE);
	trans_frame(framemgr, frame, FS_PROCESS);
	devmgr_ops->late_shot_handle(group, frame, FS_HW_WAIT_DONE);

	/* restore */
	frame_manager_close(framemgr);
}

static int pablo_devicemgr_kunit_test_init(struct kunit *test)
{
	memset(&pkt_ctx, 0, sizeof(pkt_ctx));

	devmgr_ops = pablo_kunit_get_devicemgr();

	pkt_ctx.sensor.ops = &pkt_dev_sensor_ops;
	pkt_ctx.vctx.queue = &pkt_ctx.queue;
	pkt_ctx.group.device = &pkt_ctx.ischain;

	return 0;
}

static void pablo_devicemgr_kunit_test_exit(struct kunit *test)
{
	devmgr_ops = NULL;
}

static struct kunit_case pablo_devicemgr_kunit_test_cases[] = {
	KUNIT_CASE(pablo_devicemgr_do_sensor_tag_negative_kunit_test),
	KUNIT_CASE(pablo_devicemgr_shot_callback_device_sensor_negative_kunit_test),
	KUNIT_CASE(pablo_devicemgr_shot_callback_device_ischain_kunit_test),
	KUNIT_CASE(pablo_devicemgr_shot_done_kunit_test),
	KUNIT_CASE(pablo_devicemgr_late_shot_handle_kunit_test),
	{},
};

struct kunit_suite pablo_devicemgr_kunit_test_suite = {
	.name = "pablo-devicemgr-kunit-test",
	.init = pablo_devicemgr_kunit_test_init,
	.exit = pablo_devicemgr_kunit_test_exit,
	.test_cases = pablo_devicemgr_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_devicemgr_kunit_test_suite);

struct kunit_suite pablo_devicemgr_kunit_test_suite_end = {
	.name = "pablo-devicemgr-kunit-test-end",
	.test_cases = NULL,
};
define_pablo_kunit_test_suites_end(&pablo_devicemgr_kunit_test_suite_end);
MODULE_LICENSE("GPL");
