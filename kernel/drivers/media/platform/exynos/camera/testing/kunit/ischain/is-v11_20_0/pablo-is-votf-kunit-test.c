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
#include "is-votf-id-table.h"
#include "is-device-ischain.h"
#include "is-config.h"

#define KUNIT_TEST_SIZE_WIDTH 1920
#define KUNIT_TEST_SIZE_HEIGHT 1080

static struct test_ctx {
	struct votf_dev *votfdev[VOTF_DEV_NUM];
	struct votf_dev votf;
	struct votf_info vinfo;
	struct is_group group_rgbp;
	struct is_group group;
	struct is_device_ischain idi;
} test_ctx;

static int _test_vector[] = {
	GROUP_ID_3AA0,
	GROUP_ID_3AA1,
	GROUP_ID_3AA2,
	GROUP_ID_BYRP,
	GROUP_ID_RGBP,
	GROUP_ID_MCS0,
	GROUP_ID_PAF0,
	GROUP_ID_PAF1,
	GROUP_ID_PAF2,
	GROUP_ID_MCFP,
	GROUP_ID_YUVP,
	GROUP_ID_LME0,
	GROUP_ID_SS0,
	GROUP_ID_SS1,
	GROUP_ID_SS2,
	GROUP_ID_SS3,
	GROUP_ID_SS4,
	GROUP_ID_SS5,
	GROUP_ID_SS6,
	GROUP_ID_SS7,
	GROUP_ID_SS8,
};

static void __clear_test_group_link(struct is_group *group)
{
	group->next = NULL;
	group->prev = NULL;
	group->gnext = NULL;
	group->gprev = NULL;
	group->parent = NULL;
	group->child = NULL;
	group->head = NULL;
	group->tail = NULL;
	group->pnext = NULL;
	group->ptail = NULL;

	group->junction = NULL;
}

static void pablo_is_votf_is_get_votf_ip_kunit_test(struct kunit *test)
{
	int ret, i;

	for (i = 0; i < sizeof(_test_vector)/sizeof(int); i++) {
		ret = is_get_votf_ip(_test_vector[i]);
		if (_test_vector[i] >= GROUP_ID_SS0 && _test_vector[i] <= GROUP_ID_SS3) {
			KUNIT_EXPECT_EQ(test, ret, VOTF_CSIS_W0);
		} else if (_test_vector[i] >= GROUP_ID_3AA0 && _test_vector[i] < GROUP_ID_3AA_MAX) {
			KUNIT_EXPECT_EQ(test, ret, VOTF_CSTAT_R);
		} else if (_test_vector[i] == GROUP_ID_BYRP) {
			KUNIT_EXPECT_EQ(test, ret, VOTF_BYRP_R);
		} else if (_test_vector[i] == GROUP_ID_RGBP) {
			KUNIT_EXPECT_EQ(test, ret, VOTF_RGBP_W);
		} else if (_test_vector[i] == GROUP_ID_MCS0) {
			KUNIT_EXPECT_EQ(test, ret, VOTF_MCSC_R);
		} else {
			KUNIT_EXPECT_EQ(test, ret, 0xFFFFFFFF);
		}
	}
}

static void pablo_is_votf_is_get_votf_id_kunit_test(struct kunit *test)
{
	int ret;

	/* Invalid id */
	ret = is_get_votf_id(GROUP_ID_SS0, ENTRY_3AA);
	KUNIT_EXPECT_EQ(test, ret, 0xFFFFFFFF);

	ret = is_get_votf_id(GROUP_ID_SS0, ENTRY_SSVC0);
	KUNIT_EXPECT_NE(test, ret, 0xFFFFFFFF);
}

static void pablo_is_votf_is_votf_get_token_size_kunit_test(struct kunit *test)
{
	u32 ret;
	struct votf_info *vinfo = &test_ctx.vinfo;

	vinfo->ip = VOTF_CSIS_W0;
	vinfo->mode = VOTF_FRS;
	ret = is_votf_get_token_size(vinfo);
	KUNIT_EXPECT_EQ(test, ret, 40);

	vinfo->mode = VOTF_TRS_HEIGHT_X2;
	vinfo->service = TRS;
	ret = is_votf_get_token_size(vinfo);
	KUNIT_EXPECT_EQ(test, ret, 2);

	vinfo->ip = VOTF_RGBP_W;
	ret = is_votf_get_token_size(vinfo);
	KUNIT_EXPECT_EQ(test, ret, 1);
}

static void pablo_is_votf_is_votf_get_master_vinfo_kunit_test(struct kunit *test)
{
	struct is_group *group = &test_ctx.group;
	struct is_group *src_gr = NULL;
	int src_id = 0, src_entry = 0;
	int i;

	for (i = 0; i < sizeof(_test_vector)/sizeof(int); i++) {
		group->id = _test_vector[i];
		group->prev = group;
		group->junction = &group->leader;
		group->leader.id = ENTRY_SSVC0;
		src_gr = NULL;
		src_id = 0;
		src_entry = 0;
		is_votf_get_master_vinfo(group, &src_gr, &src_id, &src_entry);
		if (_test_vector[i] == GROUP_ID_3AA0 || _test_vector[i] == GROUP_ID_MCS0) {
			KUNIT_EXPECT_PTR_NE(test, src_gr, NULL);
		} else {
			KUNIT_EXPECT_PTR_EQ(test, src_gr, NULL);
			KUNIT_EXPECT_EQ(test, src_id, 0);
		}
		__clear_test_group_link(group);
	}
}

static void pablo_is_votf_is_votf_get_slave_vinfo_kunit_test(struct kunit *test)
{
	struct is_group *group = &test_ctx.group;
	struct is_group *dst_gr = NULL;
	int dst_id = 0, dst_entry = 0;
	int i;

	for (i = 0; i < sizeof(_test_vector)/sizeof(int); i++) {
		group->id = _test_vector[i];
		group->leader.id = ENTRY_SSVC0;
		dst_gr = NULL;
		dst_id = 0;
		dst_entry = 0;
		is_votf_get_slave_vinfo(group, &dst_gr, &dst_id, &dst_entry);
		if (_test_vector[i] == GROUP_ID_3AA0 || _test_vector[i] == GROUP_ID_MCS0) {
			KUNIT_EXPECT_PTR_NE(test, dst_gr, NULL);
		} else {
			KUNIT_EXPECT_PTR_EQ(test, dst_gr, NULL);
			KUNIT_EXPECT_EQ(test, dst_id, 0);
		}
		__clear_test_group_link(group);
	}
}

static void pablo_is_votf_is_votf_get_slave_leader_subdev_kunit_test(struct kunit *test)
{
	struct is_group *group = &test_ctx.group;
	struct is_subdev *leader = NULL;
	int i;

	for (i = 0; i < sizeof(_test_vector)/sizeof(int); i++) {
		group->prev = group;
		group->tail = group;
		group->next = group;
		group->id = _test_vector[i];

		leader = is_votf_get_slave_leader_subdev(group, TWS);
		KUNIT_EXPECT_PTR_NE(test, leader, NULL);

		leader = NULL;

		leader = is_votf_get_slave_leader_subdev(group, TRS);
		KUNIT_EXPECT_PTR_NE(test, leader, NULL);

		__clear_test_group_link(group);
	}
}

static void pablo_is_votf_is_hw_pdp_set_votf_config_kunit_test(struct kunit *test)
{
	int ret;
	struct is_group *group = &test_ctx.group;

	ret = is_hw_pdp_set_votf_config(group, NULL);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_is_votf_is_hw_mcsc_set_votf_size_config_kunit_test(struct kunit *test)
{
	int ret;

	ret = is_hw_mcsc_set_votf_size_config(KUNIT_TEST_SIZE_WIDTH, KUNIT_TEST_SIZE_HEIGHT);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_is_votf_is_votf_subdev_flush_kunit_test(struct kunit *test)
{
	struct is_group *group = &test_ctx.group;
	struct votf_dev *votf = &test_ctx.votf;
	struct is_device_ischain *idi = &test_ctx.idi;

	votf->votf_table[TWS][0][RGBP_HF].ip = VOTF_RGBP_W;
	votf->votf_table[TWS][0][RGBP_HF].use = 0;
	votf->votf_table[TRS][0][MCSC_HF].ip = VOTF_MCSC_R;
	votf->votf_table[TRS][0][MCSC_HF].use = 0;
	set_votf_dev(0, votf);

	group->id = GROUP_ID_MCS0;
	idi->group[GROUP_SLOT_RGBP]->id = GROUP_ID_RGBP;
	is_votf_subdev_flush(group);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, group);

	set_votf_dev(0, NULL);
	__clear_test_group_link(group);
}

static void pablo_is_votf_is_votf_subdev_create_link_kunit_test(struct kunit *test)
{
	int ret;
	struct is_group *group = &test_ctx.group;
	struct is_device_ischain *idi = &test_ctx.idi;

	/* cannot get master vinfo */
	group->id = GROUP_ID_MAX;
	ret = is_votf_subdev_create_link(group);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	group->id = GROUP_ID_MCS0;
	idi->group[GROUP_SLOT_RGBP]->id = GROUP_ID_RGBP;
	ret = is_votf_subdev_create_link(group);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);
}

static void pablo_is_votf_is_votf_subdev_destroy_link_kunit_test(struct kunit *test)
{
#if defined(USE_VOTF_AXI_APB)
	struct is_group *group = &test_ctx.group;
	struct is_device_ischain *idi = &test_ctx.idi;

	group->id = GROUP_ID_MCS0;
	idi->group[GROUP_SLOT_RGBP]->id = GROUP_ID_RGBP;
	is_votf_subdev_destroy_link(group);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, group);
#endif
}

static int pablo_is_votf_kunit_test_init(struct kunit *test)
{
	int i;
	struct is_group *group = &test_ctx.group;

	for (i = 0; i < VOTF_DEV_NUM; i++) {
		test_ctx.votfdev[i] = get_votf_dev(i);
		set_votf_dev(i, NULL);
	}

	group->device = &test_ctx.idi;
	test_ctx.idi.group[GROUP_SLOT_RGBP] = &test_ctx.group_rgbp;

	return 0;
}

static void pablo_is_votf_kunit_test_exit(struct kunit *test)
{
	int i;
	struct is_group *group = &test_ctx.group;

	for (i = 0; i < VOTF_DEV_NUM; i++) {
		set_votf_dev(i, test_ctx.votfdev[i]);
	}

	__clear_test_group_link(group);
}

static struct kunit_case pablo_is_votf_kunit_test_cases[] = {
	KUNIT_CASE(pablo_is_votf_is_get_votf_ip_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_get_votf_id_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_votf_get_token_size_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_votf_get_master_vinfo_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_votf_get_slave_vinfo_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_votf_get_slave_leader_subdev_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_hw_mcsc_set_votf_size_config_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_hw_pdp_set_votf_config_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_votf_subdev_flush_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_votf_subdev_create_link_kunit_test),
	KUNIT_CASE(pablo_is_votf_is_votf_subdev_destroy_link_kunit_test),
	{},
};

struct kunit_suite pablo_is_votf_kunit_test_suite = {
	.name = "pablo-is-votf-kunit-test",
	.init = pablo_is_votf_kunit_test_init,
	.exit = pablo_is_votf_kunit_test_exit,
	.test_cases = pablo_is_votf_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_is_votf_kunit_test_suite);

MODULE_LICENSE("GPL v2");
