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
#include "pablo-hw-chain-info.h"
#include "votf/pablo-votf.h"
#include "pablo-device-iommu-group.h"
#include "is-core.h"

static struct is_hardware *hw;
static struct is_frame *frame;
static void *addr_contents;

static struct pablo_kunit_test_ctx {
	struct votf_dev *votf_dev_mock[VOTF_DEV_NUM];
	struct votf_dev *votf_dev[VOTF_DEV_NUM];
} pkt_ctx;

static void pablo_hw_chain_info_probe_kunit_test(struct kunit *test)
{
	int ret;

	ret = pablo_hw_chain_info_probe(hw);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_chain_info_cr_set_kunit_test(struct kunit *test)
{
	struct size_cr_set *scs;

	frame->kva_byrp_rta_info[PLANE_INDEX_CR_SET] = (u64)addr_contents;
	scs = CALL_HW_CHAIN_INFO_OPS(hw, get_cr_set, DEV_HW_BYRP, frame);
	KUNIT_EXPECT_EQ(test, (u64)scs, (u64)addr_contents);

	frame->kva_rgbp_rta_info[PLANE_INDEX_CR_SET] = (u64)addr_contents;
	scs = CALL_HW_CHAIN_INFO_OPS(hw, get_cr_set, DEV_HW_RGBP, frame);
	KUNIT_EXPECT_EQ(test, (u64)scs, (u64)addr_contents);

	frame->kva_mcfp_rta_info[PLANE_INDEX_CR_SET] = (u64)addr_contents;
	scs = CALL_HW_CHAIN_INFO_OPS(hw, get_cr_set, DEV_HW_MCFP, frame);
	KUNIT_EXPECT_EQ(test, (u64)scs, (u64)addr_contents);

	frame->kva_yuvp_rta_info[PLANE_INDEX_CR_SET] = (u64)addr_contents;
	scs = CALL_HW_CHAIN_INFO_OPS(hw, get_cr_set, DEV_HW_YPP, frame);
	KUNIT_EXPECT_EQ(test, (u64)scs, (u64)addr_contents);

	frame->kva_lme_rta_info[PLANE_INDEX_CR_SET] = (u64)addr_contents;
	scs = CALL_HW_CHAIN_INFO_OPS(hw, get_cr_set, DEV_HW_LME0, frame);
	KUNIT_EXPECT_EQ(test, (u64)scs, (u64)addr_contents);

	scs = CALL_HW_CHAIN_INFO_OPS(hw, get_cr_set, DEV_HW_3AA0, frame);
	KUNIT_EXPECT_EQ(test, (u64)scs, (u64)0);
}

static void pablo_hw_chain_info_get_config_kunit_test(struct kunit *test)
{
	void *config;
	u32 *config_data = addr_contents;

	frame->kva_byrp_rta_info[PLANE_INDEX_CONFIG] = (u64)addr_contents;
	config_data[0] = sizeof(struct is_byrp_config);
	config = CALL_HW_CHAIN_INFO_OPS(hw, get_config, DEV_HW_BYRP, frame);
	KUNIT_EXPECT_TRUE(test, config == (addr_contents));

	frame->kva_rgbp_rta_info[PLANE_INDEX_CONFIG] = (u64)addr_contents;
	config_data[0] = sizeof(struct is_rgbp_config);
	config = CALL_HW_CHAIN_INFO_OPS(hw, get_config, DEV_HW_RGBP, frame);
	KUNIT_EXPECT_TRUE(test, config == (addr_contents));

	frame->kva_mcfp_rta_info[PLANE_INDEX_CONFIG] = (u64)addr_contents;
	config_data[0] = sizeof(struct is_mcfp_config);
	config = CALL_HW_CHAIN_INFO_OPS(hw, get_config, DEV_HW_MCFP, frame);
	KUNIT_EXPECT_TRUE(test, config == (addr_contents));

	frame->kva_yuvp_rta_info[PLANE_INDEX_CONFIG] = (u64)addr_contents;
	config_data[0] = sizeof(struct is_yuvp_config);
	config = CALL_HW_CHAIN_INFO_OPS(hw, get_config, DEV_HW_YPP, frame);
	KUNIT_EXPECT_TRUE(test, config == (addr_contents));

	frame->kva_lme_rta_info[PLANE_INDEX_CONFIG] = (u64)addr_contents;
	config_data[0] = sizeof(struct is_lme_config);
	config = CALL_HW_CHAIN_INFO_OPS(hw, get_config, DEV_HW_LME0, frame);
	KUNIT_EXPECT_TRUE(test, config == (addr_contents));

	config = CALL_HW_CHAIN_INFO_OPS(hw, get_config, DEV_HW_3AA0, frame);
	KUNIT_EXPECT_TRUE(test, config == NULL);
}

static void pablo_hw_chain_info_get_hw_slot_id_kunit_test(struct kunit *test)
{
	int slot_id;

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_PAF0);
	KUNIT_EXPECT_TRUE(test, slot_id == 0);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_PAF1);
	KUNIT_EXPECT_TRUE(test, slot_id == 1);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_PAF2);
	KUNIT_EXPECT_TRUE(test, slot_id == 2);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_PAF3);
	KUNIT_EXPECT_TRUE(test, slot_id == 3);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_3AA0);
	KUNIT_EXPECT_TRUE(test, slot_id == 4);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_3AA1);
	KUNIT_EXPECT_TRUE(test, slot_id == 5);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_3AA2);
	KUNIT_EXPECT_TRUE(test, slot_id == 6);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_3AA3);
	KUNIT_EXPECT_TRUE(test, slot_id == 7);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_LME0);
	KUNIT_EXPECT_TRUE(test, slot_id == 8);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_YPP);
	KUNIT_EXPECT_TRUE(test, slot_id == 9);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_MCSC0);
	KUNIT_EXPECT_TRUE(test, slot_id == 10);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_BYRP);
	KUNIT_EXPECT_TRUE(test, slot_id == 11);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_RGBP);
	KUNIT_EXPECT_TRUE(test, slot_id == 12);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_MCFP);
	KUNIT_EXPECT_TRUE(test, slot_id == 13);

	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_SHRP);
	KUNIT_EXPECT_TRUE(test, slot_id == 14);

	/* fail case */
	slot_id = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_ISP0);
	KUNIT_EXPECT_TRUE(test, slot_id == -1);
}

static void pablo_hw_chain_info_get_hw_ip_kunit_test(struct kunit *test)
{
	struct is_hw_ip *hw_ip;

	hw_ip = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_ip, DEV_HW_PAF0);
	KUNIT_EXPECT_TRUE(test, hw_ip == &hw->hw_ip[0]);

	hw_ip = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_ip, DEV_HW_ISP0);
	KUNIT_EXPECT_TRUE(test, hw_ip == NULL);
}

static void pablo_hw_chain_info_query_cap_kunit_test(struct kunit *test)
{
	struct is_hw_mcsc_cap cap;

	cap.max_output = 0;
	CALL_HW_CHAIN_INFO_OPS(hw, query_cap, DEV_HW_MCSC0, (void *)&cap);
	KUNIT_EXPECT_TRUE(test, cap.max_output == 5);

	cap.max_output = 0;
	CALL_HW_CHAIN_INFO_OPS(hw, query_cap, DEV_HW_MCSC1, (void *)&cap);
	KUNIT_EXPECT_TRUE(test, cap.max_output == 0);
}

static void pablo_hw_chain_info_get_ctrl_kunit_test(struct kunit *test)
{
	bool value;

	value = false;
	CALL_HW_CHAIN_INFO_OPS(hw, get_ctrl, 0, HW_G_CTRL_HAS_VRA_CH1_ONLY, (void *)&value);
	KUNIT_EXPECT_TRUE(test, value == true);
}

static void pablo_hw_chain_info_set_votf_hwfc_kunit_test(struct kunit *test)
{
	int ret;

	ret = CALL_HW_CHAIN_INFO_OPS(hw, set_votf_hwfc, 0);
	KUNIT_EXPECT_TRUE(test, ret == 0);

	ret = CALL_HW_CHAIN_INFO_OPS(hw, set_votf_hwfc, 1);
	KUNIT_EXPECT_TRUE(test, ret == 0);

	ret = CALL_HW_CHAIN_INFO_OPS(hw, set_votf_hwfc, 2);
	KUNIT_EXPECT_TRUE(test, ret == 0);

	ret = CALL_HW_CHAIN_INFO_OPS(hw, set_votf_hwfc, 3);
	KUNIT_EXPECT_TRUE(test, ret == 0);

	ret = CALL_HW_CHAIN_INFO_OPS(hw, set_votf_hwfc, 4);
	KUNIT_EXPECT_TRUE(test, ret == 0);
}

static void pablo_hw_chain_info_set_votf_reset_kunit_test(struct kunit *test)
{
	int ret;

	ret = CALL_HW_CHAIN_INFO_OPS(hw, set_votf_reset);
	KUNIT_EXPECT_TRUE(test, ret == 0);
}

static void pablo_hw_chain_info_get_iommu_mem_kunit_test(struct kunit *test)
{
	struct is_mem *mem, *mem_tmp;
	struct is_core *core = is_get_is_core();

	mem = CALL_HW_CHAIN_INFO_OPS(hw, get_iommu_mem, GROUP_ID_BYRP);
	mem_tmp = &core->resourcemgr.mem;
	KUNIT_EXPECT_TRUE(test, mem == mem_tmp);
}

static void pablo_hw_chain_info_check_crta_group_kunit_test(struct kunit *test)
{
	bool ret;

	ret = CALL_HW_CHAIN_INFO_OPS(hw, check_crta_group, GROUP_ID_PAF0);
	KUNIT_EXPECT_TRUE(test, ret == true);
	ret = CALL_HW_CHAIN_INFO_OPS(hw, check_crta_group, GROUP_ID_PAF1);
	KUNIT_EXPECT_TRUE(test, ret == true);
	ret = CALL_HW_CHAIN_INFO_OPS(hw, check_crta_group, GROUP_ID_PAF2);
	KUNIT_EXPECT_TRUE(test, ret == true);
	ret = CALL_HW_CHAIN_INFO_OPS(hw, check_crta_group, GROUP_ID_PAF3);
	KUNIT_EXPECT_TRUE(test, ret == true);
	ret = CALL_HW_CHAIN_INFO_OPS(hw, check_crta_group, GROUP_ID_3AA0);
	KUNIT_EXPECT_TRUE(test, ret == true);
	ret = CALL_HW_CHAIN_INFO_OPS(hw, check_crta_group, GROUP_ID_3AA1);
	KUNIT_EXPECT_TRUE(test, ret == true);
	ret = CALL_HW_CHAIN_INFO_OPS(hw, check_crta_group, GROUP_ID_3AA2);
	KUNIT_EXPECT_TRUE(test, ret == true);
	ret = CALL_HW_CHAIN_INFO_OPS(hw, check_crta_group, GROUP_ID_3AA3);
	KUNIT_EXPECT_TRUE(test, ret == true);
	ret = CALL_HW_CHAIN_INFO_OPS(hw, check_crta_group, GROUP_ID_BYRP);
	KUNIT_EXPECT_TRUE(test, ret == false);
}

static void pablo_hw_chain_info_kunit_test_init_votfdev(struct kunit *test)
{
	struct votf_dev *dev, *org_dev;
	u32 dev_id, ip;
	void *votf_addr;

	for (dev_id = 0; dev_id < VOTF_DEV_NUM; dev_id++) {
		org_dev = get_votf_dev(dev_id);
		if (!org_dev)
			continue;

		/* Copy original dev infos */
		dev = kunit_kzalloc(test, sizeof(struct votf_dev), 0);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);
		memcpy(dev, org_dev, sizeof(struct votf_dev));

		/* Set dummy VOTF addr region */
		for (ip = 0; ip < IP_MAX; ip++) {
			votf_addr = kunit_kzalloc(test, 0x10000, 0);
			KUNIT_ASSERT_NOT_ERR_OR_NULL(test, votf_addr);
			dev->votf_addr[ip] = votf_addr;
		}


		pkt_ctx.votf_dev[dev_id] = org_dev;
		pkt_ctx.votf_dev_mock[dev_id] = dev;

		set_votf_dev(dev_id, dev);
	}
}

static void pablo_hw_chain_info_kunit_test_deinit_votfdev(struct kunit *test)
{
	struct votf_dev *dev;
	u32 dev_id, ip;
	void *votf_addr;

	for (dev_id = 0; dev_id < VOTF_DEV_NUM; dev_id++) {
		dev = pkt_ctx.votf_dev_mock[dev_id];
		if (!dev)
			continue;

		for (ip = 0; ip < IP_MAX; ip++) {
			votf_addr = dev->votf_addr[ip];
			if (!votf_addr)
				continue;

			kunit_kfree(test, votf_addr);
			dev->votf_addr[ip] = NULL;
		}

		set_votf_dev(dev_id, pkt_ctx.votf_dev[dev_id]);
		kunit_kfree(test, pkt_ctx.votf_dev_mock[dev_id]);

		pkt_ctx.votf_dev[dev_id] = NULL;
		pkt_ctx.votf_dev_mock[dev_id] = NULL;
	}
}

static int pablo_hw_chain_info_kunit_test_init(struct kunit *test)
{
	int ret;

	hw = kunit_kzalloc(test, sizeof(struct is_hardware), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hw);

	frame = kunit_kzalloc(test, sizeof(struct is_frame), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);

	addr_contents = kunit_kzalloc(test, sizeof(struct size_cr_set), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr_contents);

	ret = pablo_hw_chain_info_probe(hw);
	KUNIT_EXPECT_EQ(test, ret, 0);

	pablo_hw_chain_info_kunit_test_init_votfdev(test);

	return 0;
}

static void pablo_hw_chain_info_kunit_test_exit(struct kunit *test)
{
	pablo_hw_chain_info_kunit_test_deinit_votfdev(test);
	kunit_kfree(test, addr_contents);
	kunit_kfree(test, frame);
	kunit_kfree(test, hw);
}

static struct kunit_case pablo_hw_chain_info_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_chain_info_probe_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_cr_set_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_get_config_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_get_hw_slot_id_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_get_hw_ip_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_query_cap_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_get_ctrl_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_set_votf_hwfc_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_set_votf_reset_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_get_iommu_mem_kunit_test),
	KUNIT_CASE(pablo_hw_chain_info_check_crta_group_kunit_test),
	{},
};

struct kunit_suite pablo_hw_chain_info_kunit_test_suite = {
	.name = "pablo-hw-chain-info-kunit-test",
	.init = pablo_hw_chain_info_kunit_test_init,
	.exit = pablo_hw_chain_info_kunit_test_exit,
	.test_cases = pablo_hw_chain_info_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_chain_info_kunit_test_suite);

MODULE_LICENSE("GPL v2");
