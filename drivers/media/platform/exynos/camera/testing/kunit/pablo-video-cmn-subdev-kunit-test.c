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

#include "is-video.h"
#include "is-core.h"

static void pablo_common_video_probe_kunit_test(struct kunit *test)
{
	int ret;
	struct is_core *core;
	struct is_video *video;

	/* 1. abnormal test: core is null */
	ret = is_common_video_probe(NULL);
	KUNIT_EXPECT_TRUE(test, ret == -EINVAL);

	/* 2. abnormal test: core->pdev is null */
	core = kunit_kzalloc(test, sizeof(core), GFP_KERNEL);
	KUNIT_ASSERT_TRUE(test, core);

	ret = is_common_video_probe(core);
	KUNIT_EXPECT_TRUE(test, ret == -EINVAL);

	kunit_kfree(test, core);

	/* 3. normal test */
	core = is_get_is_core();
	video = &core->video_cmn;

	KUNIT_EXPECT_TRUE(test, video->resourcemgr == &core->resourcemgr);
	KUNIT_EXPECT_TRUE(test, video->group_id == GROUP_ID_MAX);
	KUNIT_EXPECT_TRUE(test, video->group_ofs == 0);
	KUNIT_EXPECT_TRUE(test, video->subdev_id == ENTRY_INTERNAL);
	KUNIT_EXPECT_TRUE(test, video->subdev_ofs == 0);
	KUNIT_EXPECT_TRUE(test, video->buf_rdycount == 0);

	KUNIT_EXPECT_TRUE(test, !strcmp(video->vd.name, "common"));
	KUNIT_EXPECT_TRUE(test, video->id == IS_VIDEO_COMMON);
	KUNIT_EXPECT_TRUE(test, video->video_type == IS_VIDEO_TYPE_COMMON);
}

static int pablo_video_cmn_subdev_kunit_test_init(struct kunit *test)
{
	return 0;
}

static void pablo_video_cmn_subdev_kunit_test_exit(struct kunit *test)
{
}

static struct kunit_case pablo_video_cmn_subdev_kunit_test_cases[] = {
	KUNIT_CASE(pablo_common_video_probe_kunit_test),
	{},
};

struct kunit_suite pablo_video_cmn_subdev_kunit_test_suite = {
	.name = "pablo-video-cmn-subdev-kunit-test",
	.init = pablo_video_cmn_subdev_kunit_test_init,
	.exit = pablo_video_cmn_subdev_kunit_test_exit,
	.test_cases = pablo_video_cmn_subdev_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_video_cmn_subdev_kunit_test_suite);

MODULE_LICENSE("GPL");
