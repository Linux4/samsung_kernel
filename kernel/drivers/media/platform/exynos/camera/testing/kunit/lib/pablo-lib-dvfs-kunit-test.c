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
#include "pablo-dvfs.h"
#include "is-config.h"

enum IS_SCENARIO_ID {
	IS_DVFS_SN_DEFAULT,
	IS_DVFS_SN_REAR_SINGLE_PHOTO,
	IS_DVFS_SN_REAR_SINGLE_PHOTO_FULL,
	IS_DVFS_SN_REAR_SINGLE_CAPTURE,
	IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30,
	IS_DVFS_SN_REAR_SINGLE_SSM,
	IS_DVFS_SN_FRONT_SINGLE_PHOTO,
	IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL,
	IS_DVFS_SN_FRONT_SINGLE_CAPTURE,
	IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30,
	IS_DVFS_SN_THROTTLING,
	IS_DVFS_SN_MAX,
	IS_SN_END,
};

#define TEST_START_DVFS_SN IS_DVFS_SN_DEFAULT

static struct is_dvfs_dt_t dvfs_kunit_hw_dt_arr[IS_SN_END] = {
	{
		.parse_scenario_nm = "default_",
		.scenario_id = IS_DVFS_SN_DEFAULT,
		.keep_frame_tick = -1,
	},
	/* rear sensor scenarios */
	{
		.parse_scenario_nm = "rear_single_photo_",
		.scenario_id = IS_DVFS_SN_REAR_SINGLE_PHOTO,
		.keep_frame_tick = -1,
	},
	{
		.parse_scenario_nm = "rear_single_photo_full_",
		.scenario_id = IS_DVFS_SN_REAR_SINGLE_PHOTO_FULL,
		.keep_frame_tick = -1,
	},
	{
		.parse_scenario_nm = "rear_single_capture_",
		.scenario_id = IS_DVFS_SN_REAR_SINGLE_CAPTURE,
		.keep_frame_tick = 8,
	},
	{
		.parse_scenario_nm = "rear_single_video_fhd30_",
		.scenario_id = IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30,
		.keep_frame_tick = -1,
	},
	{
		.parse_scenario_nm = "rear_single_ssm_",
		.scenario_id = IS_DVFS_SN_REAR_SINGLE_SSM,
		.keep_frame_tick = -1,
	},
	/* front sensor scenarios */
	{
		.parse_scenario_nm = "front_single_photo_",
		.scenario_id = IS_DVFS_SN_FRONT_SINGLE_PHOTO,
		.keep_frame_tick = -1,
	},
	{
		.parse_scenario_nm = "front_single_photo_full_",
		.scenario_id = IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL,
		.keep_frame_tick = -1,
	},
	{
		.parse_scenario_nm = "front_single_capture_",
		.scenario_id = IS_DVFS_SN_FRONT_SINGLE_CAPTURE,
		.keep_frame_tick = 8,
	},
	{
		.parse_scenario_nm = "front_single_video_fhd30_",
		.scenario_id = IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30,
		.keep_frame_tick = -1,
	},
	/* special scenarios */
	{
		.parse_scenario_nm = "throttling_",
		.scenario_id = IS_DVFS_SN_THROTTLING,
		.keep_frame_tick = -1,
	},
	/* max scenario */
	{
		.parse_scenario_nm = "max_",
		.scenario_id = IS_DVFS_SN_MAX,
		.keep_frame_tick = -1,
	},
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

static struct is_dvfs_iteration_mode dvfs_iteration_ctrl = {
	.changed = 0,
	.iter_svhist = 0,
};

static u32 test_sn_dvfs_data[IS_SN_END][IS_DVFS_END] = {
	{ 1, 1, 1, 1, 1, 1, 1, 0, 1, 0 }, // IS_DVFS_SN_DEFAULT
	{ 0, 5, 7, 5, 0, 8, 1, 0, 1, 0 }, // IS_DVFS_SN_REAR_SINGLE_PHOTO
	{ 0, 5, 6, 5, 0, 8, 1, 0, 1, 0 }, // IS_DVFS_SN_REAR_SINGLE_PHOTO_FULL
	{ 0, 5, 6, 6, 0, 7, 0, 0, 1, 0 }, // IS_DVFS_SN_REAR_SINGLE_CAPTURE
	{ 0, 5, 7, 7, 0, 8, 2, 0, 1, 0 }, // IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30
	{ 0, 0, 6, 0, 0, 0, 0, 4, 6, 0 }, // IS_DVFS_SN_REAR_SINGLE_SSM
	{ 0, 5, 7, 7, 0, 8, 0, 0, 1, 0 }, // IS_DVFS_SN_FRONT_SINGLE_PHOTO
	{ 0, 5, 7, 7, 0, 8, 0, 0, 1, 0 }, // IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL
	{ 0, 5, 6, 6, 0, 8, 0, 0, 1, 0 }, // IS_DVFS_SN_FRONT_SINGLE_CAPTURE
	{ 0, 5, 7, 7, 0, 8, 3, 0, 1, 0 }, // IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30
	{ 0, 0, 0, 6, 0, 7, 0, 0, 1, 0 }, // IS_DVFS_SN_THROTTLING
	{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }, // IS_DVFS_SN_MAX
};

static const char *test_dvfs_cpu[IS_SN_END] = {
	"0-3", // IS_DVFS_SN_DEFAULT
	"0-3", // IS_DVFS_SN_REAR_SINGLE_PHOTO
	"0-3", // IS_DVFS_SN_REAR_SINGLE_PHOTO_FULL
	"0-3", // IS_DVFS_SN_REAR_SINGLE_CAPTURE
	"0-3", // IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30
	"0-3", // IS_DVFS_SN_REAR_SINGLE_SSM
	"0-3", // IS_DVFS_SN_FRONT_SINGLE_PHOTO
	"0-3", // IS_DVFS_SN_FRONT_SINGLE_PHOTO_FULL
	"0-3", // IS_DVFS_SN_FRONT_SINGLE_CAPTURE
	"4-7", // IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30
	"0-3", // IS_DVFS_SN_THROTTLING
	"0-3", // IS_DVFS_SN_MAX
};

static void dvfs_kunit_pm_qos_add_request(struct is_pm_qos_request *req, int exynos_pm_qos_class,
					  s32 value)
{
	/* Do nothing */
};

static void dvfs_kunit_pm_qos_update_request(struct is_pm_qos_request *req, s32 new_value)
{
	/* Do nothing */
};

static void dvfs_kunit_pm_qos_remove_request(struct is_pm_qos_request *req)
{
	/* Do nothing */
};

static int dvfs_kunit_pm_qos_request_active(struct is_pm_qos_request *req)
{
	if (req->exynos_pm_qos_class == 0)
		return 0;
	else
		return 1;
};

static struct is_pm_qos_ops dvfs_kunit_pm_qos_req_ops = {
	.add_request = dvfs_kunit_pm_qos_add_request,
	.update_request = dvfs_kunit_pm_qos_update_request,
	.remove_request = dvfs_kunit_pm_qos_remove_request,
	.request_active = dvfs_kunit_pm_qos_request_active,
};

static void dvfs_kunit_emstune_request(void)
{
	/* Do nothing */
}

static void dvfs_kunit_emstune_boost(int enable)
{
	/* Do nothing */
}

static struct is_emstune_ops dvfs_kunit_emstune_ops = {
	.add_request = dvfs_kunit_emstune_request,
	.remove_request = dvfs_kunit_emstune_request,
	.boost = dvfs_kunit_emstune_boost,
};

static struct is_dvfs_ext_func dvfs_kunit_ext_func = {
	.pm_qos_ops = &dvfs_kunit_pm_qos_req_ops,
	.emstune_ops = &dvfs_kunit_emstune_ops,
};

static bool cancel_dwork_test;
static u32 dvfs_kunit_hw_dvfs_get_lv(struct is_dvfs_ctrl *dvfs_ctrl, u32 type)
{
	if (type == IS_DVFS_CSIS) {
		if (cancel_dwork_test)
			return 3;
		else
			return 1;
	}

	return 0;
}

static struct is_dvfs_ctrl dvfs_ctrl = {
	.dvfs_info = {
		.dvfs_data = test_sn_dvfs_data,
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
		.dvfs_cpu = test_dvfs_cpu,
		.scenario_count = IS_SN_END,
	},
	.static_ctrl = &dvfs_static_ctrl,
	.dynamic_ctrl = &dvfs_dynamic_ctrl,
	.iter_mode = &dvfs_iteration_ctrl,
};

static void pablo_lib_dvfs_get_bit_count_kunit_test(struct kunit *test)
{
	unsigned long test_bits;
	u32 ret_count;

	enum exynos_sensor_position {
		/* for the position of real sensors */
		SENSOR_POSITION_REAR,
		SENSOR_POSITION_FRONT,
		SENSOR_POSITION_REAR2,
		SENSOR_POSITION_FRONT2,
		SENSOR_POSITION_REAR3,
		SENSOR_POSITION_FRONT3,
		SENSOR_POSITION_MAX,
	};

	test_bits = 0;
	ret_count = is_get_bit_count(test_bits);
	KUNIT_EXPECT_EQ(test, ret_count, (u32)0);

	/* single */
	test_bits = SENSOR_POSITION_REAR | SENSOR_POSITION_FRONT;
	ret_count = is_get_bit_count(test_bits);
	KUNIT_EXPECT_EQ(test, ret_count, (u32)1);

	/* dual */
	test_bits = SENSOR_POSITION_REAR | SENSOR_POSITION_REAR2 | SENSOR_POSITION_FRONT;
	ret_count = is_get_bit_count(test_bits);
	KUNIT_EXPECT_EQ(test, ret_count, (u32)2);

	/* triple */
	test_bits = SENSOR_POSITION_REAR | SENSOR_POSITION_REAR2 | SENSOR_POSITION_REAR3 |
		    SENSOR_POSITION_FRONT;
	ret_count = is_get_bit_count(test_bits);
	KUNIT_EXPECT_EQ(test, ret_count, (u32)3);
}

void pablo_lib_dvfs_sel_static_kunit_test(struct kunit *test)
{
	struct is_dvfs_ctrl *test_dvfs_ctrl;
	struct is_dvfs_scenario_ctrl *static_ctrl;
	int ret, scenario_id;

	test_dvfs_ctrl = &dvfs_ctrl;

	scenario_id = IS_SN_END;
	ret = is_dvfs_sel_static(test_dvfs_ctrl, scenario_id);
	KUNIT_EXPECT_EQ(test, ret, (int)IS_SCENARIO_DEFAULT);

	scenario_id = -1;
	ret = is_dvfs_sel_static(test_dvfs_ctrl, scenario_id);
	KUNIT_EXPECT_EQ(test, ret, (int)IS_SCENARIO_DEFAULT);

	scenario_id = IS_DVFS_SN_REAR_SINGLE_PHOTO;
	ret = is_dvfs_sel_static(test_dvfs_ctrl, scenario_id);
	KUNIT_EXPECT_EQ(test, ret, scenario_id);

	static_ctrl = test_dvfs_ctrl->static_ctrl;
	KUNIT_EXPECT_EQ(test, static_ctrl->cur_scenario_id, (u32)scenario_id);
	KUNIT_EXPECT_EQ(test, static_ctrl->cur_frame_tick, -1);

	KUNIT_EXPECT_PTR_EQ(test, (char *)static_ctrl->scenario_nm,
			    (char *)dvfs_kunit_hw_dt_arr[scenario_id].parse_scenario_nm);
}

void pablo_lib_dvfs_sel_dynamic_kunit_test(struct kunit *test)
{
	struct is_dvfs_ctrl *test_dvfs_ctrl;
	int ret;

	test_dvfs_ctrl = &dvfs_ctrl;

	/* check reprocessing mode */
	ret = is_dvfs_sel_dynamic(test_dvfs_ctrl, false);
	KUNIT_EXPECT_EQ(test, ret, -EAGAIN);

	/* check vendor ssm scenario */
	test_dvfs_ctrl->dvfs_scenario = IS_DVFS_SCENARIO_VENDOR_SSM
					<< IS_DVFS_SCENARIO_VENDOR_SHIFT;
	ret = is_dvfs_sel_dynamic(test_dvfs_ctrl, true);
	KUNIT_EXPECT_EQ(test, ret, -EAGAIN);

	test_dvfs_ctrl->dvfs_scenario = 0;
	/* check reprocessing mode */
	ret = is_dvfs_sel_dynamic(test_dvfs_ctrl, true);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check cur_frame_tick decreasing */
	test_dvfs_ctrl->dynamic_ctrl->cur_frame_tick = 0;
	ret = is_dvfs_sel_dynamic(test_dvfs_ctrl, true);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->dynamic_ctrl->cur_frame_tick, -1);
}

void pablo_lib_dvfs_update_dynamic_kunit_test(struct kunit *test)
{
	struct is_dvfs_ctrl *test_dvfs_ctrl;
	struct is_dvfs_scenario_ctrl *dynamic_ctrl;
	int ret, scenario_id;

	test_dvfs_ctrl = &dvfs_ctrl;

	/* check dynamic setting */
	scenario_id = IS_DVFS_SN_REAR_SINGLE_PHOTO;
	ret = is_dvfs_update_dynamic(test_dvfs_ctrl, scenario_id);
	KUNIT_EXPECT_EQ(test, ret, true);

	dynamic_ctrl = test_dvfs_ctrl->dynamic_ctrl;
	KUNIT_EXPECT_EQ(test, dynamic_ctrl->cur_scenario_id, (u32)scenario_id);
	KUNIT_EXPECT_EQ(test, dynamic_ctrl->cur_frame_tick,
			dvfs_kunit_hw_dt_arr[scenario_id].keep_frame_tick);
	KUNIT_EXPECT_PTR_EQ(test, (char *)dynamic_ctrl->scenario_nm,
			    (char *)dvfs_kunit_hw_dt_arr[scenario_id].parse_scenario_nm);
}

void pablo_lib_dvfs_get_freq_kunit_test(struct kunit *test)
{
	int ret;
	struct is_dvfs_ctrl *test_dvfs_ctrl;
	u32 dvfs_freq[IS_DVFS_END];

	test_dvfs_ctrl = &dvfs_ctrl;

	test_dvfs_ctrl->static_ctrl = &dvfs_static_ctrl;
	ret = is_dvfs_get_freq(test_dvfs_ctrl, dvfs_freq);
	KUNIT_EXPECT_EQ(test, ret, 0);

	test_dvfs_ctrl->cur_lv[IS_DVFS_CSIS] = 0;
	test_dvfs_ctrl->cur_lv[IS_DVFS_CAM] = 1;
	test_dvfs_ctrl->cur_lv[IS_DVFS_ISP] = 2;
	test_dvfs_ctrl->cur_lv[IS_DVFS_INT_CAM] = 3;
	test_dvfs_ctrl->cur_lv[IS_DVFS_MIF] = 4;
	test_dvfs_ctrl->cur_lv[IS_DVFS_INT] = 5;

	ret = is_dvfs_get_freq(test_dvfs_ctrl, dvfs_freq);
	KUNIT_EXPECT_EQ(test, dvfs_freq[IS_DVFS_CSIS], (u32)700);
	KUNIT_EXPECT_EQ(test, dvfs_freq[IS_DVFS_CAM], (u32)533);
	KUNIT_EXPECT_EQ(test, dvfs_freq[IS_DVFS_ISP], (u32)468);
	KUNIT_EXPECT_EQ(test, dvfs_freq[IS_DVFS_INT_CAM], (u32)468);
	KUNIT_EXPECT_EQ(test, dvfs_freq[IS_DVFS_MIF], (u32)2288);
	KUNIT_EXPECT_EQ(test, dvfs_freq[IS_DVFS_INT], (u32)332);
}

void pablo_lib_dvfs_set_dvfs_otf_kunit_test(struct kunit *test)
{
	struct is_dvfs_ctrl *test_dvfs_ctrl;
	int ret, scenario_id;
	int cur_lv = 0;

	test_dvfs_ctrl = &dvfs_ctrl;

	/* check the dvfs level when scenario is IS_DVFS_SN_DEFAULT */
	scenario_id = IS_DVFS_SN_DEFAULT;
	ret = is_set_dvfs_otf(test_dvfs_ctrl, scenario_id);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CSIS], (u32)1);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check the dvfs level when scenario is IS_DVFS_SN_REAR_SINGLE_PHOTO */
	scenario_id = IS_DVFS_SN_REAR_SINGLE_PHOTO;
	ret = is_set_dvfs_otf(test_dvfs_ctrl, scenario_id);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CSIS], (u32)0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* is_get_qos_table() exception case test */
	scenario_id = IS_DVFS_SN_REAR_SINGLE_CAPTURE;
	test_dvfs_ctrl->dvfs_info.dvfs_data[scenario_id][IS_DVFS_CSIS] = -1;
	ret = is_set_dvfs_otf(test_dvfs_ctrl, scenario_id);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CSIS], (u32)1);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* cancel_delayed_work_sync case test */
	cancel_dwork_test = true;
	scenario_id = IS_DVFS_SN_REAR_SINGLE_PHOTO;
	test_dvfs_ctrl->dvfs_info.dvfs_data[scenario_id][IS_DVFS_CSIS] = IS_DVFS_LV_END;
	test_dvfs_ctrl->cur_lv[IS_DVFS_CSIS] = cur_lv;
	test_dvfs_ctrl->static_ctrl->cur_scenario_id = IS_DVFS_SN_REAR_SINGLE_PHOTO;
	ret = is_set_dvfs_otf(test_dvfs_ctrl, scenario_id);
	is_remove_dvfs(test_dvfs_ctrl, TEST_START_DVFS_SN); /* calling cancel_delayed_work_sync */
	msleep(IS_DVFS_DEC_DTIME * 2);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CSIS], (u32)cur_lv + 1);
	cancel_dwork_test = false;
}

void pablo_lib_dvfs_set_dvfs_m2m_kunit_test(struct kunit *test)
{
	struct is_dvfs_ctrl *test_dvfs_ctrl;
	int ret, scenario_id;

	test_dvfs_ctrl = &dvfs_ctrl;

	scenario_id = IS_DVFS_SN_DEFAULT;
	ret = is_set_dvfs_m2m(test_dvfs_ctrl, scenario_id, false);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check the dvfs level when scenario is IS_DVFS_SN_DEFAULT */
	scenario_id = IS_DVFS_SN_DEFAULT;
	ret = is_set_dvfs_m2m(test_dvfs_ctrl, scenario_id, false);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CAM], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_ISP], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT_CAM], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_MIF], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT], (u32)1);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check the dvfs level when scenario is IS_DVFS_SN_REAR_SINGLE_PHOTO */
	scenario_id = IS_DVFS_SN_REAR_SINGLE_PHOTO;
	ret = is_set_dvfs_m2m(test_dvfs_ctrl, scenario_id, false);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CAM], (u32)5);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_ISP], (u32)7);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT_CAM], (u32)5);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_MIF], (u32)8);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT], (u32)1);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check dvfs thrott control */
	test_dvfs_ctrl->thrott_ctrl = IS_DVFS_SN_THROTTLING;
	set_bit(IS_DVFS_TICK_THROTT, &test_dvfs_ctrl->state);
	ret = is_set_dvfs_m2m(test_dvfs_ctrl, scenario_id, false);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CAM], (u32)5);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_ISP], (u32)7);

	if (IS_ENABLED(CONFIG_THROTTLING_INTCAM_ENABLE))
		KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT_CAM], (u32)6);
	else
		KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT_CAM], (u32)5);

	if (IS_ENABLED(CONFIG_THROTTLING_MIF_ENABLE))
		KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_MIF], (u32)7);
	else
		KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_MIF], (u32)8);

	if (IS_ENABLED(CONFIG_THROTTLING_INT_ENABLE))
		KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT], (u32)0);
	else
		KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT], (u32)1);

	KUNIT_EXPECT_EQ(test, ret, 0);

	test_dvfs_ctrl->thrott_ctrl = IS_DVFS_THROTT_CTRL_OFF;
	clear_bit(IS_DVFS_TICK_THROTT, &test_dvfs_ctrl->state);

	/* check boost up when cur_iter > 0 */
	test_dvfs_ctrl->iter_mode->iter_svhist = 1;
	ret = is_set_dvfs_m2m(test_dvfs_ctrl, scenario_id, false);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CAM], (u32)2);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_ISP], (u32)5);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT_CAM], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_MIF], (u32)3);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT], (u32)1);
	KUNIT_EXPECT_EQ(test, ret, 0);

	test_dvfs_ctrl->iter_mode->iter_svhist = 0;
	/* check emstune on - boost up */
	test_dvfs_ctrl->cur_hmp_bst = 0;
	scenario_id = IS_DVFS_SN_REAR_SINGLE_SSM;
	ret = is_set_dvfs_m2m(test_dvfs_ctrl, scenario_id, false);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_hmp_bst, 1);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check emstune off */
	scenario_id = IS_DVFS_SN_REAR_SINGLE_PHOTO;
	ret = is_set_dvfs_m2m(test_dvfs_ctrl, scenario_id, false);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_hmp_bst, 0);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check cpu */
	test_dvfs_ctrl->static_ctrl->cur_scenario_id = IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30;
	ret = is_set_dvfs_m2m(test_dvfs_ctrl, scenario_id, false);
	KUNIT_EXPECT_PTR_EQ(
		test, (char *)test_dvfs_ctrl->cur_cpus,
		(char *)test_dvfs_ctrl->dvfs_info.dvfs_cpu[IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30]);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

void pablo_lib_dvfs_set_static_dvfs_kunit_test(struct kunit *test)
{
	int ret;
	struct is_dvfs_ctrl *test_dvfs_ctrl;
	int scenario_id = IS_DVFS_SN_DEFAULT;

	test_dvfs_ctrl = &dvfs_ctrl;

	scenario_id = IS_SN_END;
	ret = pablo_set_static_dvfs(test_dvfs_ctrl, "DVFS-OTF-KUNIT-TEST", scenario_id,
				    IS_DVFS_PATH_OTF, false);
	KUNIT_EXPECT_EQ(test, ret, (int)IS_SCENARIO_DEFAULT);

	scenario_id = IS_DVFS_SN_DEFAULT;
	/* check otf path */
	ret = pablo_set_static_dvfs(test_dvfs_ctrl, "DVFS-OTF-KUNIT-TEST", scenario_id,
				    IS_DVFS_PATH_OTF, false);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CSIS], (u32)1);
	KUNIT_EXPECT_EQ(test, ret, 0);

	/* check m2m path */
	ret = pablo_set_static_dvfs(test_dvfs_ctrl, "DVFS-M2M-KUNIT-TEST", scenario_id,
				    IS_DVFS_PATH_M2M, false);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CAM], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_ISP], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT_CAM], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_MIF], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT], (u32)1);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

void pablo_lib_dvfs_set_static_dvfs_stream_kunit_test(struct kunit *test)
{
	int ret;
	struct is_dvfs_ctrl *test_dvfs_ctrl;
	u32 scenario, dvfs;

	test_dvfs_ctrl = &dvfs_ctrl;

	ret = is_set_static_dvfs(test_dvfs_ctrl, IS_SN_END, true, true);
	KUNIT_EXPECT_EQ(test, ret, (int)IS_SCENARIO_DEFAULT);

	/* check stream on */
	ret = is_set_static_dvfs(test_dvfs_ctrl, IS_DVFS_SN_DEFAULT, true, true);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CAM], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_ISP], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT_CAM], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_MIF], (u32)1);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_INT], (u32)1);
	KUNIT_EXPECT_EQ(test, ret, 0);

	scenario = IS_DVFS_SN_REAR_SINGLE_PHOTO;
	ret = is_set_static_dvfs(test_dvfs_ctrl, scenario, true, false);
	KUNIT_EXPECT_EQ(test, ret, scenario);
	for (dvfs = 0; dvfs < IS_DVFS_END; dvfs++) {
		if (!test_dvfs_ctrl->dvfs_info.qos_name[dvfs])
			KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[dvfs],
				test_sn_dvfs_data[scenario][dvfs]);
	}

	/* check stream off */
	ret = is_set_static_dvfs(test_dvfs_ctrl, IS_DVFS_SN_DEFAULT, false, false);
	KUNIT_EXPECT_EQ(test, test_dvfs_ctrl->cur_lv[IS_DVFS_CSIS], (u32)1);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static int pablo_lib_dvfs_kunit_test_init(struct kunit *test)
{
	int ret;
	u32 qos_thput[IS_DVFS_END];
	struct is_dvfs_ctrl *test_dvfs_ctrl;

	test_dvfs_ctrl = &dvfs_ctrl;
	ret = is_dvfs_init(test_dvfs_ctrl, dvfs_kunit_hw_dt_arr, dvfs_kunit_hw_dvfs_get_lv);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_dvfs_set_ext_func(&dvfs_kunit_ext_func);
	KUNIT_EXPECT_EQ(test, ret, 0);

	qos_thput[IS_DVFS_INT_CAM] = PM_QOS_INTCAM_THROUGHPUT;
	qos_thput[IS_DVFS_TNR] = 0;
	qos_thput[IS_DVFS_CSIS] = PM_QOS_CSIS_THROUGHPUT;
	qos_thput[IS_DVFS_ISP] = PM_QOS_ISP_THROUGHPUT;
	qos_thput[IS_DVFS_INT] = PM_QOS_DEVICE_THROUGHPUT;
	qos_thput[IS_DVFS_MIF] = PM_QOS_BUS_THROUGHPUT;
	qos_thput[IS_DVFS_CAM] = PM_QOS_CAM_THROUGHPUT;
	qos_thput[IS_DVFS_M2M] = 0;

	ret = is_add_dvfs(test_dvfs_ctrl, TEST_START_DVFS_SN, qos_thput);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = is_dvfs_ems_init();
	KUNIT_ASSERT_EQ(test, ret, 0);

	return 0;
}

static void pablo_lib_dvfs_kunit_test_exit(struct kunit *test)
{
	int ret;
	struct is_dvfs_ctrl *test_dvfs_ctrl;

	test_dvfs_ctrl = &dvfs_ctrl;
	ret = is_remove_dvfs(test_dvfs_ctrl, TEST_START_DVFS_SN);
	KUNIT_EXPECT_EQ(test, ret, 0);

	test_dvfs_ctrl->cur_hmp_bst = 1;
	ret = is_dvfs_ems_reset(test_dvfs_ctrl);
	KUNIT_ASSERT_EQ(test, ret, 0);
}

static struct kunit_case pablo_lib_dvfs_kunit_test_cases[] = {
	KUNIT_CASE(pablo_lib_dvfs_get_bit_count_kunit_test),
	KUNIT_CASE(pablo_lib_dvfs_sel_static_kunit_test),
	KUNIT_CASE(pablo_lib_dvfs_sel_dynamic_kunit_test),
	KUNIT_CASE(pablo_lib_dvfs_update_dynamic_kunit_test),
	KUNIT_CASE(pablo_lib_dvfs_get_freq_kunit_test),
	KUNIT_CASE(pablo_lib_dvfs_set_dvfs_otf_kunit_test),
	KUNIT_CASE(pablo_lib_dvfs_set_dvfs_m2m_kunit_test),
	KUNIT_CASE(pablo_lib_dvfs_set_static_dvfs_kunit_test),
	KUNIT_CASE(pablo_lib_dvfs_set_static_dvfs_stream_kunit_test),
	{},
};

struct kunit_suite pablo_lib_dvfs_kunit_test_suite = {
	.name = "pablo-lib-dvfs-kunit-test",
	.test_cases = pablo_lib_dvfs_kunit_test_cases,
	.init = pablo_lib_dvfs_kunit_test_init,
	.exit = pablo_lib_dvfs_kunit_test_exit,
};
define_pablo_kunit_test_suites(&pablo_lib_dvfs_kunit_test_suite);

MODULE_LICENSE("GPL");
