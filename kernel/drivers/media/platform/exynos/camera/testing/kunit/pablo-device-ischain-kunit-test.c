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
#include "is-core.h"
#include "is-device-ischain.h"
#include "is-device-sensor.h"
#include "exynos-is-module.h"
#include "is-device-sensor-peri.h"

static struct pablo_kunit_groupmgr_func *fn_grp;
static struct pablo_kunit_device_ischain_func *func;

static struct test_ctx {
	struct is_device_ischain ischain;
	struct is_device_sensor sensor;
	struct is_module_enum module;
	struct is_video_ctx video_ctx;
	struct is_queue queue;
	struct vb2_queue vbq;
} test_ctx;

static int kunit_mock_group_start(struct is_group *group)
{
	if (test_bit(IS_GROUP_START, &group->state))
		return -EINVAL;

	set_bit(IS_GROUP_START, &group->state);
	return 0;
}

static int kunit_mock_group_stop(struct is_group *group)
{
	if (!group->head)
		return -EPERM;

	if (!test_bit(IS_GROUP_START, &group->state))
		return -EINVAL;

	clear_bit(IS_GROUP_START, &group->state);
	return 0;
}

static const struct is_group_ops kunit_mock_group_ops = {
	.start = kunit_mock_group_start,
	.stop = kunit_mock_group_stop,
};

static void pablo_ischain_itf_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_device_sensor *ids = &test_ctx.sensor;

	/* Sub TC: !device */
	ret = func->itf_open(NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->is_region */
	ret = func->itf_open(idi, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !device->sensor */
	idi->is_region = kunit_kzalloc(test, sizeof(struct is_region), 0);

	ret = func->itf_open(idi, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: set IS_ISCHAIN_OPEN_STREAM */
	idi->sensor = ids;
	set_bit(IS_ISCHAIN_OPEN_STREAM, &idi->state);

	ret = func->itf_open(idi, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: clear IS_ISCHAIN_OPEN_STREAM */
	clear_bit(IS_ISCHAIN_OPEN_STREAM, &idi->state);

	ret = func->itf_open(idi, 0);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	/* Restore */
	kunit_kfree(test, idi->is_region);
}

static void pablo_ischain_itf_close_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_device_sensor *ids = &test_ctx.sensor;
	struct is_minfo *im;

	idi->sensor = ids;

	/* Sub TC: clear IS_ISCHAIN_OPEN_STREAM */
	ret = func->itf_close(idi);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC: set IS_ISCHAIN_OPEN_STREAM/IS_ISCHAIN_INIT */
	set_bit(IS_ISCHAIN_OPEN_STREAM, &idi->state);
	set_bit(IS_ISCHAIN_INIT, &idi->state);
	im = is_get_is_minfo();
	refcount_set(&im->refcount_setfile[ids->position], 1);
	im->loaded_setfile[ids->position] = true;

	ret = func->itf_close(idi);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(IS_ISCHAIN_OPEN_STREAM, &idi->state), 0);
}

static void pablo_ischain_itf_setfile_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_device_sensor *ids = &test_ctx.sensor;
	struct is_module_enum *module = &test_ctx.module;
	struct is_minfo *im = is_get_is_minfo();

	idi->sensor = ids;

	/* Sub TC: fail is_sensor_g_module */
	refcount_set(&im->refcount_setfile[ids->position], 0);
	im->loaded_setfile[ids->position] = false;

	ret = func->itf_setfile(idi);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: success is_sensor_g_module */
	module->subdev = kunit_kzalloc(test, sizeof(struct v4l2_subdev), 0);
	v4l2_set_subdevdata(module->subdev, module);
	module->pdata = kunit_kzalloc(test, sizeof(struct exynos_platform_is_module), 0);
	module->setfile_pinned = false;
	module->setfile_name = "KUNIT";
	module->setfile_size = 100;

	ids->subdev_module = module->subdev;

	ret = func->itf_setfile(idi);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, im->pinned_setfile[ids->position], module->setfile_pinned);
	KUNIT_EXPECT_STREQ(test, im->name_setfile[ids->position], module->setfile_name);
	KUNIT_EXPECT_EQ(test, refcount_read(&im->refcount_setfile[ids->position]), 1U);

	/* Sub TC: im->loaded_setfile */
	ret = func->itf_setfile(idi);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, refcount_read(&im->refcount_setfile[ids->position]), 2U);

	/* Sub TC: success is_sensor_g_module */
	/* TODO: need mock of request_firmware */

	/* Restore */
	kunit_kfree(test, module->pdata);
	kunit_kfree(test, module->subdev);
}

static void pablo_ischain_g_ddk_capture_meta_update_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_group *group;
	u32 slot = GROUP_SLOT_3AA;

	fn_grp->group_probe(NULL, idi, NULL, slot);
	group = idi->group[slot];

	ret = func->g_ddk_capture_meta_update(group, idi, 0, 0, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Restore */
	fn_grp->group_release(idi, slot);
}

static void pablo_ischain_g_ddk_setfile_version_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	size_t size = sizeof(struct ddk_setfile_ver);
	struct ddk_setfile_ver *user_ptr = kunit_kzalloc(test, size, 0);

	idi->sensor = &test_ctx.sensor;

	ret = func->g_ddk_setfile_version(idi, user_ptr);
	KUNIT_EXPECT_EQ(test, ret, (int)size);

	/* Restore */
	kunit_kfree(test, user_ptr);
}

static void pablo_ischain_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;

	idi->is_region = kunit_kzalloc(test, sizeof(struct is_region), 0);

	/* Sub TC: fail is_resource_get */
	ret = func->open(idi);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: success is_resource_get */
	/* TODO: It needs a resourcemgr mock. */

	/* Restore */
	kunit_kfree(test, idi->is_region);
}

static void pablo_ischain_close_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;

	idi->is_region = kunit_kzalloc(test, sizeof(struct is_region), 0);

	/* Sub TC: Not IS_ISCHAIN_OPEN state */
	ret = func->close(idi);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC: IS_ISCHAIN_OPEN state */
	func->open(idi);
	ret = func->close(idi);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Restore */
	kunit_kfree(test, idi->is_region);
}

static void pablo_ischain_change_vctx_group_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_device_ischain *idi_mch = is_get_ischain_device(1);
	struct is_video_ctx *ivc = &test_ctx.video_ctx;
	u32 slot = GROUP_SLOT_3AA;

	fn_grp->group_probe(NULL, idi, NULL, slot);
	ivc->group = idi->group[slot];
	ivc->video = kunit_kzalloc(test, sizeof(struct is_video), 0);

	/* Sub TC:  */
	ret = func->change_vctx_group(idi, ivc);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_USE_MULTI_CH, &ivc->group->state), 1);
	KUNIT_EXPECT_EQ(test, test_bit(IS_ISCHAIN_MULTI_CH, &idi_mch->state), 1);

	/* Restore */
	clear_bit(IS_ISCHAIN_MULTI_CH, &idi_mch->state);
	clear_bit(IS_GROUP_USE_MULTI_CH, &ivc->group->state);
	kunit_kfree(test, ivc->video);
	fn_grp->group_release(idi, slot);
}

static void pablo_ischain_group_open_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_device_ischain *idi_mch = is_get_ischain_device(1);
	struct is_video_ctx *ivc = &test_ctx.video_ctx;
	u32 slot = GROUP_SLOT_3AA;

	fn_grp->group_probe(NULL, idi, NULL, slot);
	ivc->group = idi->group[slot];
	ivc->video = kunit_kzalloc(test, sizeof(struct is_video), 0);

	/* Sub TC: !device */
	ret = func->group_open(NULL, NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: !vctx */
	ret = func->group_open(idi, NULL, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: fail CALL_GROUP_OPS(open) */
	ret = func->group_open(idi, ivc, idi->group[slot]->id);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: fail is_ischain_open_wrap() */
	idi->group[slot]->id = GROUP_ID_3AA0;
	set_bit(IS_ISCHAIN_CLOSING, &idi->state);
	ret = func->group_open(idi, ivc, idi->group[slot]->id);
	KUNIT_EXPECT_EQ(test, ret, -EPERM);

	/* Sub TC: success CALL_GROUP_OPS(open) */
	idi->group[slot]->id = GROUP_ID_3AA0;
	clear_bit(IS_ISCHAIN_CLOSING, &idi->state);
	ret = func->group_open(idi, ivc, idi->group[slot]->id);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, atomic_read(&idi->group_open_cnt), 1);

	/* Sub TC: -EAGAIN CALL_GROUP_OPS(open) */
	ret = func->group_open(idi, ivc, idi->group[slot]->id + 1);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_USE_MULTI_CH, &ivc->group->state), 1);
	KUNIT_EXPECT_EQ(test, test_bit(IS_ISCHAIN_MULTI_CH, &idi_mch->state), 1);

	/* Restore */
	clear_bit(IS_ISCHAIN_MULTI_CH, &idi_mch->state);
	clear_bit(IS_GROUP_USE_MULTI_CH, &ivc->group->state);
	kunit_kfree(test, ivc->video);
	fn_grp->group_release(idi, slot);
}

static void pablo_ischain_group_close_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_video_ctx *ivc = &test_ctx.video_ctx;
	struct is_group *group;
	u32 slot = GROUP_SLOT_3AA;

	fn_grp->group_probe(NULL, idi, NULL, slot);
	group = ivc->group = idi->group[slot];

	/* Sub TC: fail CALL_GROUP_OPS(close) */
	atomic_set(&idi->group_open_cnt, 1);

	ret = func->group_close(idi, group);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, atomic_read(&idi->group_open_cnt), 0);

	/* Sub TC: success CALL_GROUP_OPS(close) */
	group->id = GROUP_ID_3AA0;
	set_bit(IS_GROUP_OPEN, &group->state);

	ret = func->group_close(idi, group);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC: sudden group close */
	group->head = group;
	set_bit(IS_GROUP_START, &group->state);
	group->id = GROUP_ID_3AA0;
	set_bit(IS_GROUP_OPEN, &group->state);

	ret = func->group_close(idi, group);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Restore */
	fn_grp->group_release(idi, slot);
}

static void pablo_ischain_group_s_input_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_video_ctx *ivc = &test_ctx.video_ctx;
	u32 slot = GROUP_SLOT_3AA;
	struct is_group *group;

	fn_grp->group_probe(NULL, idi, NULL, slot);
	group = ivc->group = idi->group[slot];

	/* Sub TC: fail CALL_GROUP_OPS(init) */
	ret = func->group_s_input(idi, group, IS_PREVIEW_STREAM, SP_REAR, GROUP_INPUT_MEMORY, 1);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: success CALL_GROUP_OPS(init) && fail is_ischain_init_wrap() */
	group->id = GROUP_ID_3AA0;
	ret = func->group_open(idi, ivc, idi->group[slot]->id);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = func->group_s_input(idi, group, IS_PREVIEW_STREAM, SP_REAR, GROUP_INPUT_MEMORY, 1);
	KUNIT_EXPECT_EQ(test, ret, -EMFILE);

	/* Sub TC: success is_ischain_init_wrap() */
	/* TODO: It needs "is_device_sensor" and "is_module_enum" classes. */
	set_bit(IS_ISCHAIN_OPEN, &idi->state);

	ret = func->group_s_input(idi, group, IS_PREVIEW_STREAM, SP_REAR, GROUP_INPUT_MEMORY, 1);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Restore */
	fn_grp->group_release(idi, slot);
}

static void pablo_ischain_device_start_kunit_test(struct kunit *test)
{
	int ret;
	struct is_queue_ops *qops;
	struct is_group *group;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_video_ctx *ivc = &test_ctx.video_ctx;
	struct is_queue *iq = &test_ctx.queue;
	u32 slot = GROUP_SLOT_3AA;
	struct is_groupmgr *grpmgr = is_get_groupmgr();

	fn_grp->group_probe(NULL, idi, NULL, slot);
	group = idi->group[slot];
	group->ops = &kunit_mock_group_ops;
	ivc->group = group;
	iq->vbq = &test_ctx.vbq;
	iq->vbq->drv_priv = ivc;

	qops = func->get_device_qops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, qops);

	/* Sub TC: fail CALL_GROUP_OPS(start) */
	ret = qops->start_streaming(idi, iq);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: fail is_ischain_start_wrap */
	group->id = GROUP_ID_3AA0;
	clear_bit(IS_GROUP_START, &group->state);
	ret = qops->start_streaming(idi, iq);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_START, &group->state), 1);

	/* Sub TC: success is_ischain_start_wrap */
	clear_bit(IS_GROUP_START, &group->state);
	set_bit(IS_ISCHAIN_INIT, &idi->state);
	grpmgr->leader[idi->instance] = NULL;
	ret = qops->start_streaming(idi, iq);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_START, &group->state), 1);

	/* Restore */
	fn_grp->group_release(idi, slot);
}

static void pablo_ischain_device_stop_kunit_test(struct kunit *test)
{
	int ret;
	struct is_queue_ops *qops;
	struct is_group *group;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_video_ctx *ivc = &test_ctx.video_ctx;
	struct is_queue *iq = &test_ctx.queue;
	u32 slot = GROUP_SLOT_3AA;
	struct is_groupmgr *grpmgr = is_get_groupmgr();

	fn_grp->group_probe(NULL, idi, NULL, slot);
	group = idi->group[slot];
	group->ops = &kunit_mock_group_ops;
	ivc->group = group;
	iq->vbq = &test_ctx.vbq;
	iq->vbq->drv_priv = ivc;

	qops = func->get_device_qops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, qops);

	idi->is_region = kunit_kzalloc(test, sizeof(struct is_region), 0);

	/* Sub TC: clear IS_GROUP_INIT */
	ret = qops->stop_streaming(idi, iq);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC: ret == -EPERM */
	set_bit(IS_GROUP_INIT, &group->state);
	group->head = NULL;
	ret = qops->stop_streaming(idi, iq);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* Sub TC: fail CALL_GROUP_OPS(stop) */
	group->head = group;
	clear_bit(IS_GROUP_START, &group->state);
	ret = qops->stop_streaming(idi, iq);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	/* Sub TC: fail is_ischain_stop_wrap */
	set_bit(IS_GROUP_START, &group->state);
	grpmgr->leader[idi->instance] = group;
	clear_bit(IS_ISCHAIN_START, &idi->state);
	ret = qops->stop_streaming(idi, iq);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_START, &group->state), 0);

	/* Sub TC: success is_ischain_stop_wrap */
	set_bit(IS_GROUP_START, &group->state);
	grpmgr->leader[idi->instance] = group;
	set_bit(IS_ISCHAIN_START, &idi->state);
	ret = qops->stop_streaming(idi, iq);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(IS_GROUP_START, &group->state), 0);
	KUNIT_EXPECT_EQ(test, test_bit(IS_ISCHAIN_START, &idi->state), 0);

	/* Restore */
	fn_grp->group_release(idi, slot);
	kunit_kfree(test, idi->is_region);
}

static void pablo_ischain_device_s_fmt_kunit_test(struct kunit *test)
{
	int ret;
	struct is_queue_ops *qops;
	struct is_subdev *leader;
	struct is_device_ischain *idi = &test_ctx.ischain;
	struct is_video_ctx *ivc = &test_ctx.video_ctx;
	struct is_queue *iq = &test_ctx.queue;
	u32 slot = GROUP_SLOT_3AA;

	fn_grp->group_probe(NULL, idi, NULL, slot);
	ivc->group = idi->group[slot];
	iq->vbq = &test_ctx.vbq;
	iq->vbq->drv_priv = ivc;
	iq->framecfg.width = 1234;
	iq->framecfg.height = 5678;

	qops = func->get_device_qops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, qops);

	/* Sub TC: success is_ischain_group_s_fmt */
	ret = qops->s_fmt(idi, iq);
	KUNIT_EXPECT_EQ(test, ret, 0);

	leader = &ivc->group->leader;
	KUNIT_EXPECT_EQ(test, leader->input.width, iq->framecfg.width);
	KUNIT_EXPECT_EQ(test, leader->input.height, iq->framecfg.height);
	KUNIT_EXPECT_EQ(test, leader->input.crop.w, iq->framecfg.width);
	KUNIT_EXPECT_EQ(test, leader->input.crop.h, iq->framecfg.height);

	/* Restore */
	fn_grp->group_release(idi, slot);
}

static void pablo_ischain_get_device_qops_kunit_test(struct kunit *test)
{
	struct is_queue_ops *qops;

	qops = func->get_device_qops();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, qops);
}

static int pablo_ischain_kunit_test_init(struct kunit *test)
{
	fn_grp = pablo_kunit_get_groupmgr_func();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, fn_grp);

	func = pablo_kunit_get_device_ischain_func();
	memset(&test_ctx, 0x0, sizeof(struct test_ctx));

	return 0;
}

static void pablo_ischain_kunit_test_exit(struct kunit *test)
{
	func = NULL;
	fn_grp = NULL;
}

static struct kunit_case pablo_ischain_kunit_test_cases[] = {
	KUNIT_CASE(pablo_ischain_itf_open_kunit_test),
	KUNIT_CASE(pablo_ischain_itf_close_kunit_test),
	KUNIT_CASE(pablo_ischain_itf_setfile_kunit_test),
	KUNIT_CASE(pablo_ischain_g_ddk_capture_meta_update_kunit_test),
	KUNIT_CASE(pablo_ischain_g_ddk_setfile_version_kunit_test),
	KUNIT_CASE(pablo_ischain_open_kunit_test),
	KUNIT_CASE(pablo_ischain_close_kunit_test),
	KUNIT_CASE(pablo_ischain_change_vctx_group_kunit_test),
	KUNIT_CASE(pablo_ischain_group_open_kunit_test),
	KUNIT_CASE(pablo_ischain_group_close_kunit_test),
	KUNIT_CASE(pablo_ischain_group_s_input_kunit_test),
	KUNIT_CASE(pablo_ischain_device_start_kunit_test),
	KUNIT_CASE(pablo_ischain_device_stop_kunit_test),
	KUNIT_CASE(pablo_ischain_device_s_fmt_kunit_test),
	KUNIT_CASE(pablo_ischain_get_device_qops_kunit_test),
	{},
};

struct kunit_suite pablo_ischain_kunit_test_suite = {
	.name = "pablo-device-ischain-kunit-test",
	.init = pablo_ischain_kunit_test_init,
	.exit = pablo_ischain_kunit_test_exit,
	.test_cases = pablo_ischain_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_ischain_kunit_test_suite);

MODULE_LICENSE("GPL");
