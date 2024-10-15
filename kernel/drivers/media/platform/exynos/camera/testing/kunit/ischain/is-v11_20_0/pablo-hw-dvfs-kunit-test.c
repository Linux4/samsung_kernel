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

#define KUNIT_TEST_INSTANCE  0
#define KUNIT_TEST_SENSOR    0

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

static void pablo_hw_dvfs_get_face_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_dvfs_scenario_param *param = &test_ctx.param;

	param->rear_face = 1;
	param->front_face = 1;
	ret = is_hw_dvfs_get_face(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_FACE_PIP);

	param->rear_face = 1;
	param->front_face = 0;
	ret = is_hw_dvfs_get_face(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_FACE_REAR);

	param->rear_face = 0;
	param->front_face = 1;
	ret = is_hw_dvfs_get_face(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_FACE_FRONT);

	param->rear_face = 0;
	param->front_face = 0;
	ret = is_hw_dvfs_get_face(idi, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_hw_dvfs_get_num_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_dvfs_scenario_param *param = &test_ctx.param;

	param->sensor_active_map = 1;
	ret = is_hw_dvfs_get_num(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_NUM_SINGLE);

	param->sensor_active_map = 3;
	ret = is_hw_dvfs_get_num(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_NUM_DUAL);

	param->sensor_active_map = 7;
	ret = is_hw_dvfs_get_num(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_NUM_TRIPLE);

	param->sensor_active_map = 15;
	ret = is_hw_dvfs_get_num(idi, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_hw_dvfs_get_sensor_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_dvfs_scenario_param *param = &test_ctx.param;

	param->sensor_mode = IS_SPECIAL_MODE_FASTAE;
	ret = is_hw_dvfs_get_sensor(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SENSOR_FASTAE);

	param->sensor_mode = IS_SPECIAL_MODE_REMOSAIC;
	ret = is_hw_dvfs_get_sensor(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SENSOR_REMOSAIC);

	param->sensor_mode = 3;
	param->secure = IS_SCENARIO_SECURE;
	ret = is_hw_dvfs_get_sensor(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SENSOR_SECURE);

	param->dvfs_scenario = 0x20000;
	ret = is_hw_dvfs_get_sensor(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SENSOR_SSM);

	param->dvfs_scenario = 0x10000;
	ret = is_hw_dvfs_get_sensor(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SENSOR_VT);

	param->secure = IS_SCENAIRO_FACTORY_RAW;
	param->dvfs_scenario = 0;
	param->dvfs_scenario = 0;
	ret = is_hw_dvfs_get_sensor(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SENSOR_NORMAL);
}

static void pablo_hw_dvfs_get_mode_kunit_test(struct kunit *test)
{
	int ret;
	int flag_capture = 1;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_dvfs_scenario_param *param = &test_ctx.param;

	param->sensor_only = 1;
	ret = is_hw_dvfs_get_mode(idi, flag_capture, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_MODE_SENSOR_ONLY);

	param->sensor_only = 0;
	ret = is_hw_dvfs_get_mode(idi, flag_capture, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_MODE_CAPTURE);

	param->sensor_only = 0;
	flag_capture = 0;
	param->dvfs_scenario = 1;
	ret = is_hw_dvfs_get_mode(idi, flag_capture, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_MODE_PHOTO);

	param->dvfs_scenario = 2;
	ret = is_hw_dvfs_get_mode(idi, flag_capture, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_MODE_VIDEO);

	param->dvfs_scenario = 0;
	ret = is_hw_dvfs_get_mode(idi, flag_capture, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_hw_dvfs_get_resol_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_dvfs_scenario_param *param = &test_ctx.param;

	param->mode = IS_DVFS_MODE_PHOTO;
	param->scen = IS_SCENARIO_FULL_SIZE;
	ret = is_hw_dvfs_get_resol(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_RESOL_FULL);

	param->mode = IS_DVFS_MODE_CAPTURE;
	idi->resourcemgr->dvfs_ctrl.dvfs_rec_size = 0x1;
	ret = is_hw_dvfs_get_resol(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_RESOL_HD);

	idi->resourcemgr->dvfs_ctrl.dvfs_rec_size = 0xbb80780;
	ret = is_hw_dvfs_get_resol(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_RESOL_FHD);

	idi->resourcemgr->dvfs_ctrl.dvfs_rec_size = 0x17700f00;
	ret = is_hw_dvfs_get_resol(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_RESOL_UHD);

	idi->resourcemgr->dvfs_ctrl.dvfs_rec_size = 0x2ee01e00;
	ret = is_hw_dvfs_get_resol(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_RESOL_8K);

	idi->resourcemgr->dvfs_ctrl.dvfs_rec_size = 0xffffffff;
	ret = is_hw_dvfs_get_resol(idi, param);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_dvfs_get_fps_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_dvfs_scenario_param *param = &test_ctx.param;

	param->sensor_fps = 24;
	ret = is_hw_dvfs_get_fps(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_FPS_24);

	param->sensor_fps = 30;
	ret = is_hw_dvfs_get_fps(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_FPS_30);

	param->sensor_fps = 60;
	ret = is_hw_dvfs_get_fps(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_FPS_60);

	param->sensor_fps = 120;
	ret = is_hw_dvfs_get_fps(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_FPS_120);

	param->sensor_fps = 240;
	ret = is_hw_dvfs_get_fps(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_FPS_240);

	param->sensor_fps = 480;
	ret = is_hw_dvfs_get_fps(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_FPS_480);

	param->sensor_fps = 960;
	ret = is_hw_dvfs_get_fps(idi, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_hw_dvfs_get_lv_kunit_test(struct kunit *test)
{
	int ret;
	u32 type;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_device_sensor *ids = test_ctx.ids1;
	struct is_device_csi *csi = NULL;
	struct is_dvfs_ctrl *dvfs_ctrl = &ids->resourcemgr->dvfs_ctrl;

	csi = (struct is_device_csi *)v4l2_get_subdevdata(ids->subdev_csi);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, csi);

	ids->cfg->lanes = 1;
	ids->cfg->mipi_speed = 1000;
	ids->cfg->max_fps = 120;
	ids->cfg->height = 1920;
	ids->cfg->width = 1080;
	ids->cfg->votf = 1;
	ids->cfg->dvfs_lv_csis = IS_DVFS_LV_END;
	ids->cfg->output[0][0].hwformat = 0x2B;
	ids->cfg->output[0][0].extformat = 0x300;
	ids->cfg->dma_num = 1;
	ids->cfg->link_vc[0][0] = 0;
	ids->state = 0xfffffff;

	for (type = 0; type < IS_DVFS_END; type++) {
		ids->dma_output[0].fmt.hw_format = 5;
		ret = is_hw_dvfs_get_lv(dvfs_ctrl, type);

		ids->dma_output[0].fmt.hw_format = 0;
		ids->dma_output[0].fmt.bitsperpixel[0] = 10;
		set_bit(IS_SUBDEV_EXTERNAL_USE, &csi->dma_subdev[0][0]->state);
		ret = is_hw_dvfs_get_lv(dvfs_ctrl, type);

		set_bit(IS_SUBDEV_INTERNAL_USE, &csi->dma_subdev[0][0]->state);
		ret = is_hw_dvfs_get_lv(dvfs_ctrl, type);

		ids->pdata->use_cphy = 0;
		ret = is_hw_dvfs_get_lv(dvfs_ctrl, type);

		ids->cfg->votf = 0;
		ret = is_hw_dvfs_get_lv(dvfs_ctrl, type);

		ids->pdata->use_cphy = 1;
		ret = is_hw_dvfs_get_lv(dvfs_ctrl, type);

		ids->cfg->dvfs_lv_csis = 1;
		ret = is_hw_dvfs_get_lv(dvfs_ctrl, type);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, idi);
	}

	ids->state = 0x0;
}

static void pablo_hw_dvfs_get_scenario_function_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = test_ctx.idi;
	struct is_dvfs_scenario_param *param = &test_ctx.param;

	for (param->mode = IS_DVFS_MODE_PHOTO; param->mode < IS_DVFS_MODE_END; param->mode++) {
		for (param->resol = IS_DVFS_RESOL_HD; param->resol < IS_DVFS_RESOL_END; param->resol++) {
			for (param->fps = IS_DVFS_FPS_24; param->fps < IS_DVFS_FPS_END; param->fps++) {
				is_hw_dvfs_get_rear_single_scenario(idi, param);
				is_hw_dvfs_get_rear_single_remosaic_scenario(idi, param);
				is_hw_dvfs_get_rear_dual_wide_tele_scenario(idi, param);
				is_hw_dvfs_get_rear_dual_wide_ultrawide_scenario(idi, param);
				is_hw_dvfs_get_rear_dual_wide_macro_scenario(idi, param);
				is_hw_dvfs_get_front_single_front_scenario(idi, param);
				is_hw_dvfs_get_pip_dual_scenario(idi, param);
				is_hw_dvfs_get_triple_scenario(idi, param);
				is_hw_dvfs_get_front_single_remosaic_scenario(idi, param);
				KUNIT_ASSERT_NOT_ERR_OR_NULL(test, idi);
			}
		}
	}

	param->mode = IS_DVFS_MODE_VIDEO;
	param->resol = IS_DVFS_RESOL_FHD;
	param->fps = IS_DVFS_FPS_30;
	param->dvfs_scenario = 0x30000;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEOHDR);

	param->fps = IS_DVFS_FPS_30;
	param->hf = 0;
	param->dvfs_scenario = 0x40000;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);

	param->hf = 1;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30_HF_SUPERSTEADY);

	param->hf = 2;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	param->fps = IS_DVFS_FPS_60;
	param->hf = 0;
	param->dvfs_scenario = 0x40000;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60_SUPERSTEADY);

	param->hf = 1;
	param->dvfs_scenario = 0x40000;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60_HF_SUPERSTEADY);

	param->hf = 1;
	param->dvfs_scenario = 0x1;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60_HF);

	param->hf = 2;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	param->fps = IS_DVFS_FPS_END;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	param->resol = IS_DVFS_RESOL_UHD;
	param->fps = IS_DVFS_FPS_60;
	param->hf = 1;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD60_HF);

	param->hf = 2;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);

	param->resol = IS_DVFS_RESOL_8K;
	param->fps = IS_DVFS_FPS_24;
	param->hf = 1;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);

	param->hf = 2;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	param->fps = IS_DVFS_FPS_30;
	param->hf = 1;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, IS_DVFS_SN_REAR_SINGLE_VIDEO_8K30_HF);

	param->hf = 2;
	ret = is_hw_dvfs_get_rear_single_scenario(idi, param);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_hw_dvfs_get_qos_throughput_kunit_test(struct kunit *test)
{
	u32 qos_thput[IS_DVFS_END];

	is_hw_dvfs_get_qos_throughput(qos_thput);
	KUNIT_EXPECT_EQ(test, qos_thput[IS_DVFS_M2M], PM_QOS_M2M_THROUGHPUT);
}

static void  pablo_hw_dvfs_get_scenario_kunit_test(struct kunit *test)
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

static int pablo_hw_dvfs_kunit_test_init(struct kunit *test)
{
	struct is_core **core = &test_ctx.core;
	struct is_device_ischain **idi = &test_ctx.idi;
	struct is_device_sensor **ids1 = &test_ctx.ids1;
	struct is_device_sensor **ids2 = &test_ctx.ids2;
	struct is_device_sensor **ids3 = &test_ctx.ids3;
	struct is_dvfs_scenario_param *param = &test_ctx.param;
	struct is_device_csi *csi = NULL;

	*core = NULL;
	*core = is_get_is_core();
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, *core);

	*idi = is_get_ischain_device(KUNIT_TEST_INSTANCE);
	*ids1 = is_get_sensor_device(KUNIT_TEST_SENSOR);
	*ids2 = is_get_sensor_device(KUNIT_TEST_SENSOR + 1);
	*ids3 = is_get_sensor_device(KUNIT_TEST_SENSOR + 2);

	csi = (struct is_device_csi *)v4l2_get_subdevdata((*ids1)->subdev_csi);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, csi);

	memcpy(&test_ctx.idi_ori, *idi, sizeof(struct is_device_ischain));
	memcpy(&test_ctx.ids_ori1, *ids1, sizeof(struct is_device_sensor));
	memcpy(&test_ctx.ids_ori2, *ids2, sizeof(struct is_device_sensor));
	memcpy(&test_ctx.ids_ori3, *ids3, sizeof(struct is_device_sensor));

	(*ids1)->ischain = *idi;
	(*ids1)->resourcemgr = &test_ctx.resourcemgr;
	(*ids1)->cfg = &test_ctx.cfg;
	(*idi)->sensor = *ids1;
	memset(param, 0, sizeof(struct is_dvfs_scenario_param));

	return 0;
}

static void pablo_hw_dvfs_kunit_test_exit(struct kunit *test)
{
	struct is_device_ischain **idi = &test_ctx.idi;
	struct is_device_sensor **ids1 = &test_ctx.ids1;
	struct is_device_sensor **ids2 = &test_ctx.ids2;
	struct is_device_sensor **ids3 = &test_ctx.ids3;

	memcpy(*idi, &test_ctx.idi_ori, sizeof(struct is_device_ischain));
	memcpy(*ids1, &test_ctx.ids_ori1, sizeof(struct is_device_sensor));
	memcpy(*ids2, &test_ctx.ids_ori2, sizeof(struct is_device_sensor));
	memcpy(*ids3, &test_ctx.ids_ori3, sizeof(struct is_device_sensor));

	memset(&test_ctx, 0, sizeof(struct test_ctx));
}

static struct kunit_case pablo_hw_dvfs_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_dvfs_get_face_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_num_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_sensor_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_mode_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_resol_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_fps_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_lv_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_scenario_function_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_qos_throughput_kunit_test),
	KUNIT_CASE(pablo_hw_dvfs_get_scenario_kunit_test),
	{},
};

struct kunit_suite pablo_hw_dvfs_kunit_test_suite = {
	.name = "pablo-hw-dvfs-kunit-test",
	.init = pablo_hw_dvfs_kunit_test_init,
	.exit = pablo_hw_dvfs_kunit_test_exit,
	.test_cases = pablo_hw_dvfs_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_dvfs_kunit_test_suite);

MODULE_LICENSE("GPL v2");

