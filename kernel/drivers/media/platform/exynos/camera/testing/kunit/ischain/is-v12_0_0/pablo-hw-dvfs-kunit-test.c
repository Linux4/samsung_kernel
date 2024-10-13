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
#include "is-hw-dvfs.h"
#include "is-core.h"
#include "is-device-sensor.h"
#include "exynos-is.h"

#define KUNIT_TEST_INSTANCE 0
#define KUNIT_TEST_SENSOR 0

static struct test_ctx {
	struct is_resourcemgr resourcemgr;
	struct is_sensor_cfg cfg;
	struct is_dvfs_scenario_param param;
	struct is_core *core;
	struct is_device_ischain *idi;
	struct is_device_ischain idi_ori;
	struct is_device_sensor *ids1;
	struct is_device_sensor *ids2;
	struct is_device_sensor *ids3;
	struct is_device_sensor ids_ori1;
	struct is_device_sensor ids_ori2;
	struct is_device_sensor ids_ori3;
} test_ctx;

static struct pablo_kunit_hw_dvfs_function *fp;

static struct is_dvfs_iteration_mode dvfs_iteration_ctrl = {
	.changed = 0,
	.iter_svhist = 0,
};

static struct is_dvfs_scenario_ctrl dvfs_static_ctrl = {
	.cur_scenario_id = 0,
	.cur_frame_tick = 0,
	.scenario_nm = NULL,
};

static struct is_dvfs_scenario_ctrl dvfs_dynamic_ctrl = {
	.cur_scenario_id = 0,
	.cur_frame_tick = 0,
	.scenario_nm = NULL,
};

static struct is_dvfs_ctrl dvfs_ctrl = {
	.dvfs_info = {
		.qos_tb = {
			{ //IS_DVFS_CSIS
				{0, 700, 700000}, // level 0
				{1, 664, 664000}, // level 1
				{2, 533, 533000}, // level 2
				{3, 468, 468000}, // level 3
				{4, 400, 400000}, // level 4
				{5, 332, 332000}, // level 5
				{6, 267, 267000}, // level 6
				{7, 234, 234000}, // level 7
				{8, 166, 166000}, // level 8
			},
			{ //IS_DVFS_CAM
				{0, 664, 664000}, // level 0
				{1, 533, 533000}, // level 1
				{2, 468, 468000}, // level 2
				{3, 400, 400000}, // level 3
				{4, 332, 332000}, // level 4
				{5, 234, 234000}, // level 5
				{6, 166, 166000}, // level 6
				{7, 100, 100000}, // level 7
			},
			{ //IS_DVFS_ISP
				{0, 664, 664000}, // level 0
				{1, 533, 533000}, // level 1
				{2, 468, 468000}, // level 2
				{3, 400, 400000}, // level 3
				{4, 332, 332000}, // level 4
				{5, 234, 234000}, // level 5
				{6, 166, 166000}, // level 6
				{7, 100, 100000}, // level 7
			},
			{ //IS_DVFS_INT_CAM
				{0, 700, 700000}, // level 0
				{1, 664, 664000}, // level 1
				{2, 533, 533000}, // level 2
				{3, 468, 468000}, // level 3
				{4, 400, 400000}, // level 4
				{5, 332, 332000}, // level 5
				{6, 267, 267000}, // level 6
				{7, 234, 234000}, // level 7
				{8, 166, 166000}, // level 8
			},
			{ //IS_DVFS_TNR
				{0, 700, 700000}, // level 0
				{1, 664, 664000}, // level 1
				{2, 533, 533000}, // level 2
				{3, 468, 468000}, // level 3
				{4, 400, 400000}, // level 4
				{5, 332, 332000}, // level 5
				{6, 267, 267000}, // level 6
				{7, 234, 234000}, // level 7
				{8, 166, 166000}, // level 8
			},
			{ //IS_DVFS_MIF
				{0, 4206, 4206000}, // level 0
				{1, 3738, 3738000}, // level 1
				{2, 3172, 3172000}, // level 2
				{3, 2730, 2730000}, // level 3
				{4, 2288, 2288000}, // level 4
				{5, 2028, 2028000}, // level 5
				{6, 1716, 1716000}, // level 6
				{7, 1539, 1539000}, // level 7
				{8, 1352, 1352000}, // level 8
				{9, 1014, 1014000}, // level 9
				{10, 845, 845000}, // level 10
				{11, 676, 676000}, // level 11
				{12, 421, 421000}, // level 12

			},
			{ //IS_DVFS_INT
				{0, 700, 700000}, // level 0
				{1, 664, 664000}, // level 1
				{2, 533, 533000}, // level 2
				{3, 468, 468000}, // level 3
				{4, 400, 400000}, // level 4
				{5, 332, 332000}, // level 5
				{6, 267, 267000}, // level 6
				{7, 234, 234000}, // level 7
				{8, 166, 166000}, // level 8
			},
		},
		.qos_otf = {true, false, },
		.qos_name = {
			"IS_DVFS_CSIS",
			"IS_DVFS_CAM",
			"IS_DVFS_ISP",
			"IS_DVFS_INT_CAM",
			"IS_DVFS_TNR",
			"IS_DVFS_MIF",
			"IS_DVFS_INT",
			"IS_DVFS_M2M",
			"IS_DVFS_HPG",
			"IS_DVFS_ICPU",
		},
		.scenario_count = IS_SN_END,
	},
	.static_ctrl = &dvfs_static_ctrl,
	.dynamic_ctrl = &dvfs_dynamic_ctrl,
	.iter_mode = &dvfs_iteration_ctrl,
};

static void pablo_hw_dvfs_get_lv_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids = test_ctx.ids1;
	struct is_device_csi *csi = NULL;
	struct is_dvfs_ctrl *dvfs_ctrl = &ids->resourcemgr->dvfs_ctrl;

	csi = (struct is_device_csi *)v4l2_get_subdevdata(ids->subdev_csi);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, csi);

	ids->cfg->lanes = 1;
	ids->cfg->mipi_speed = 3712;
	ids->cfg->max_fps = 60;
	ids->cfg->height = 4080;
	ids->cfg->width = 3060;
	ids->cfg->dvfs_lv_csis = IS_DVFS_LV_END;
	ids->cfg->output[0][0].hwformat = HW_FORMAT_RAW10;
	ids->cfg->dma_num = 1;
	ids->cfg->link_vc[0][0] = 0;
	ids->dma_output[0].fmt.hw_format = ids->cfg->output[0][0].hwformat;
	ids->dma_output[0].fmt.bitsperpixel[0] = 10;
	ids->dma_output[0].fmt.hw_bitwidth = 10;
	ids->state = 0xfffffff;

	ret = is_hw_dvfs_get_lv(dvfs_ctrl, IS_DVFS_CSIS);
	KUNIT_EXPECT_EQ(test, ret, 7);

	ids->dma_output[0].fmt.hw_bitwidth = 16;
	ret = is_hw_dvfs_get_lv(dvfs_ctrl, IS_DVFS_CSIS);
	KUNIT_EXPECT_EQ(test, ret, 7);

	ids->cfg->dvfs_lv_csis = 1;
	ret = is_hw_dvfs_get_lv(dvfs_ctrl, IS_DVFS_CSIS);
	KUNIT_EXPECT_EQ(test, ret, 1);

	ids->state = 0x0;
}

static void pablo_hw_dvfs_get_scenario_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_device_sensor *ids1 = test_ctx.ids1;
	struct is_device_sensor *ids2 = test_ctx.ids2;
	struct is_device_sensor *ids3 = test_ctx.ids3;
	struct is_dvfs_scenario_param *param = &test_ctx.param;
	struct is_resourcemgr *resourcemgr = ids1->resourcemgr;
	struct is_dvfs_ctrl *dvfs_ctrl = &resourcemgr->dvfs_ctrl;
	struct is_sensor_cfg *cfg = ids1->cfg;

	param->sensor_active_map = 0x2;
	ids1->position = 1;
	ids2->position = 0;
	ids3->position = 2;
	/* param.num = 0 */
	set_bit(IS_SENSOR_START, &ids1->state);
	ret = is_hw_dvfs_get_scenario(idi, 1);

	/* param->sensor_mode cases */
	cfg->special_mode = IS_SPECIAL_MODE_FASTAE;
	ret = is_hw_dvfs_get_scenario(idi, 1);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_FRONT_SINGLE_FASTAE);

	/* special_mode remosaic is not support */
	cfg->special_mode = IS_SPECIAL_MODE_REMOSAIC;
	ret = is_hw_dvfs_get_scenario(idi, 0);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	cfg->special_mode = 0;
	ids1->ex_scenario = IS_SCENARIO_SECURE;
	ret = is_hw_dvfs_get_scenario(idi, 1);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_FRONT_SINGLE_PHOTO);

	dvfs_ctrl->dvfs_scenario = 0x10000;
	ret = is_hw_dvfs_get_scenario(idi, 1);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_FRONT_SINGLE_VT);

	/* param.num = 1 */
	set_bit(IS_SENSOR_START, &ids2->state);
	ret = is_hw_dvfs_get_scenario(idi, 1);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_PIP_DUAL_CAPTURE);

	/* param.num = 2 */
	set_bit(IS_SENSOR_START, &ids3->state);
	ret = is_hw_dvfs_get_scenario(idi, 1);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_TRIPLE_CAPTURE);

	clear_bit(IS_SENSOR_START, &ids1->state);
	clear_bit(IS_SENSOR_START, &ids2->state);
	clear_bit(IS_SENSOR_START, &ids3->state);
}

static void pablo_hw_dvfs_restore_static_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_resourcemgr *resourcemgr = idi->resourcemgr;
	struct is_dvfs_ctrl *dvfs_ctrl = &resourcemgr->dvfs_ctrl;

	dvfs_ctrl->dynamic_ctrl->cur_frame_tick = 0;
	ret = is_hw_dvfs_restore_static(idi);
	KUNIT_EXPECT_TRUE(test, ret);

	dvfs_ctrl->dynamic_ctrl->cur_frame_tick = 1;
	ret = is_hw_dvfs_restore_static(idi);
	KUNIT_EXPECT_FALSE(test, ret);
}

static void pablo_hw_dvfs_get_qos_throughput_kunit_test(struct kunit *test)
{
	u32 qos_thput[IS_DVFS_END];

	is_hw_dvfs_get_qos_throughput(qos_thput);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_INT_CAM], PM_QOS_INTCAM_THROUGHPUT);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_TNR], 0);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_CSIS], PM_QOS_CSIS_THROUGHPUT);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_ISP], PM_QOS_ISP_THROUGHPUT);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_INT], PM_QOS_DEVICE_THROUGHPUT);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_MIF], PM_QOS_BUS_THROUGHPUT);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_CAM], PM_QOS_CAM_THROUGHPUT);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_M2M], 0);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_ICPU], PM_QOS_ICPU_THROUGHPUT);
}

static void pablo_hw_dvfs_get_real_scenario_kunit_test(struct kunit *test)
{
	struct is_device_ischain *idi;
	struct is_dvfs_scenario_param *param;
	int ret;

	idi = test_ctx.idi;
	param = &test_ctx.param;

	/* Sub TC : Photo table test */
	param->mode = IS_DVFS_MODE_PHOTO;
	param->resol = IS_DVFS_RESOL_FULL;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_PHOTO_FULL);
	param->resol = IS_DVFS_RESOL_END;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_PHOTO);

	/* Sub TC : Capture table test */
	param->mode = IS_DVFS_MODE_CAPTURE;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_CAPTURE);

	/* Sub TC : video table test */
	param->mode = IS_DVFS_MODE_VIDEO;
	param->resol = IS_DVFS_RESOL_FHD;
	param->fps = IS_DVFS_FPS_30;
	param->recursive = 1;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30_RECURSIVE);

	param->recursive = 0;
	param->dvfs_scenario = IS_DVFS_SCENARIO_VENDOR_VIDEO_HDR << IS_DVFS_SCENARIO_VENDOR_SHIFT;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30);

	param->recursive = 0;
	param->dvfs_scenario = IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY
			       << IS_DVFS_SCENARIO_VENDOR_SHIFT;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30_SUPERSTEADY);

	param->fps = IS_DVFS_FPS_60;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60_SUPERSTEADY);

	param->dvfs_scenario = IS_DVFS_SCENARIO_VENDOR_VIDEO_HDR << IS_DVFS_SCENARIO_VENDOR_SHIFT;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60);

	param->fps = IS_DVFS_FPS_120;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD120);

	param->fps = IS_DVFS_FPS_240;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD240);

	param->fps = IS_DVFS_FPS_480;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD480);

	/* Sub TC : video UHD */
	param->resol = IS_DVFS_RESOL_UHD;
	param->fps = IS_DVFS_FPS_24;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD30);

	param->fps = IS_DVFS_FPS_30;
	param->recursive = 1;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD30_RECURSIVE);

	param->recursive = 0;
	param->dvfs_scenario = IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY
			       << IS_DVFS_SCENARIO_VENDOR_SHIFT;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD30_SUPERSTEADY);

	param->fps = IS_DVFS_FPS_60;
	param->dvfs_scenario = IS_DVFS_SCENARIO_VENDOR_VIDEO_HDR << IS_DVFS_SCENARIO_VENDOR_SHIFT;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD60);

	param->dvfs_scenario = IS_DVFS_SCENARIO_VENDOR_SUPER_STEADY
			       << IS_DVFS_SCENARIO_VENDOR_SHIFT;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD60_SUPERSTEADY);

	param->fps = IS_DVFS_FPS_120;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD120);

	/* Sub TC : video 8k */
	param->resol = IS_DVFS_RESOL_8K;
	param->fps = IS_DVFS_FPS_24;
	param->hf = 0;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_8K24);

	param->hf = 1;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_8K24_HF);

	param->fps = IS_DVFS_FPS_30;
	param->hf = 0;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_8K30);

	param->hf = 1;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_8K30_HF);

	/* Sub TC : sensor only */
	param->mode = IS_DVFS_MODE_SENSOR_ONLY;
	ret = fp->get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_SENSOR_ONLY_REAR_SINGLE);
}

static void pablo_hw_dvfs_get_fast_launch_kunit_test(struct kunit *test)
{
	struct is_dvfs_ctrl *dvfs_ctrl = &test_ctx.ids1->resourcemgr->dvfs_ctrl;
	bool fast_launch;

	dvfs_ctrl->dvfs_scenario = 0;
	fast_launch = is_hw_dvfs_get_fast_launch(dvfs_ctrl);
	KUNIT_EXPECT_TRUE(test, fast_launch);

	dvfs_ctrl->dvfs_scenario = IS_DVFS_SCENARIO_VENDOR_SENSING_CAMERA
				   << IS_DVFS_SCENARIO_VENDOR_SHIFT;
	fast_launch = is_hw_dvfs_get_fast_launch(dvfs_ctrl);
	KUNIT_EXPECT_FALSE(test, fast_launch);
}

static int pablo_hw_dvfs_kunit_test_init(struct kunit *test)
{
	test_ctx.idi = is_get_ischain_device(KUNIT_TEST_INSTANCE);
	test_ctx.ids1 = is_get_sensor_device(KUNIT_TEST_SENSOR);
	test_ctx.ids2 = is_get_sensor_device(KUNIT_TEST_SENSOR + 1);
	test_ctx.ids3 = is_get_sensor_device(KUNIT_TEST_SENSOR + 2);

	memcpy(&test_ctx.idi_ori, test_ctx.idi, sizeof(struct is_device_ischain));
	memcpy(&test_ctx.ids_ori1, test_ctx.ids2, sizeof(struct is_device_sensor));
	memcpy(&test_ctx.ids_ori2, test_ctx.ids2, sizeof(struct is_device_sensor));
	memcpy(&test_ctx.ids_ori3, test_ctx.ids3, sizeof(struct is_device_sensor));

	test_ctx.ids1->ischain = test_ctx.idi;
	test_ctx.ids1->resourcemgr = &test_ctx.resourcemgr;
	test_ctx.ids1->cfg = &test_ctx.cfg;
	test_ctx.ids2->cfg = &test_ctx.cfg;
	test_ctx.ids3->cfg = &test_ctx.cfg;
	test_ctx.idi->sensor = test_ctx.ids1;
	fp = pablo_kunit_get_hw_dvfs_func();
	memset(&test_ctx.param, 0, sizeof(struct is_dvfs_scenario_param));
	memcpy(&test_ctx.resourcemgr.dvfs_ctrl, &dvfs_ctrl, sizeof(dvfs_ctrl));

	return 0;
}

static void pablo_hw_dvfs_kunit_test_exit(struct kunit *test)
{
	memcpy(test_ctx.idi, &test_ctx.idi_ori, sizeof(struct is_device_ischain));
	memcpy(test_ctx.ids1, &test_ctx.ids_ori1, sizeof(struct is_device_sensor));
	memcpy(test_ctx.ids2, &test_ctx.ids_ori2, sizeof(struct is_device_sensor));
	memcpy(test_ctx.ids3, &test_ctx.ids_ori3, sizeof(struct is_device_sensor));

	memset(&test_ctx, 0, sizeof(struct test_ctx));
}

static struct kunit_case pablo_hw_dvfs_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_dvfs_get_lv_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_scenario_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_restore_static_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_qos_throughput_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_real_scenario_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_fast_launch_kunit_test),
	{},
};

struct kunit_suite pablo_hw_dvfs_kunit_test_suite = {
	.name = "pablo-hw-dvfs-kunit-test",
	.init = pablo_hw_dvfs_kunit_test_init,
	.exit = pablo_hw_dvfs_kunit_test_exit,
	.test_cases = pablo_hw_dvfs_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_dvfs_kunit_test_suite);

MODULE_LICENSE("GPL");
