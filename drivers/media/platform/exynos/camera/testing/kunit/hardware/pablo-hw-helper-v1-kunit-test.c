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
#include "pablo-hw-helper.h"

static struct is_hw_ip *hw_ip;
static struct is_frame *frame;
static struct camera2_shot *shot;
static struct is_lib_isp *lib;
static void *addr_base;
static void *addr_contents;
static void *addr_cur_contents;
#define MAX_NUM_CONTENTS 10

static void pablo_hw_helper_dummy_kunit_test(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, 0);
}

static void pablo_hw_helper_probe_kunit_test(struct kunit *test)
{
	int ret;

	ret = pablo_hw_helper_probe(hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_set_iq_set_kunit_test(struct kunit *test)
{
	int ret;
	u32 val_ret;
	u32 val_0 = 0x12345678;
	u32 val_1 = 0x87654321;
	struct is_hw_iq *iq_set = &hw_ip->iq_set[0];
	struct is_hw_iq *cur_iq_set = &hw_ip->cur_hw_iq_set[COREX_DIRECT];

	iq_set->regs = (struct cr_set *)addr_contents;
	cur_iq_set->regs = (struct cr_set *)addr_cur_contents;

	/* LME offset test */
	iq_set->size = 2;
	iq_set->regs[0].reg_addr = sizeof(u32) * 0;
	iq_set->regs[0].reg_data = val_0;
	iq_set->regs[1].reg_addr = sizeof(u32) * 1;
	iq_set->regs[1].reg_data = val_1;

	cur_iq_set->size = 0;
	cur_iq_set->regs[0].reg_addr = sizeof(u32) * 0;
	cur_iq_set->regs[0].reg_data = 0;
	cur_iq_set->regs[1].reg_addr = sizeof(u32) * 1;
	cur_iq_set->regs[1].reg_data = 0;

	ret = pablo_hw_helper_set_iq_set(addr_base, COREX_DIRECT, DEV_HW_LME0,
					iq_set, cur_iq_set);
	KUNIT_EXPECT_EQ(test, ret, 0);

	val_ret = *(u32 *)(addr_base + 0x8000 + sizeof(u32) * 0);
	KUNIT_EXPECT_EQ(test, val_ret, val_0);

	val_ret = *(u32 *)(addr_base + 0x8000 + sizeof(u32) * 1);
	KUNIT_EXPECT_EQ(test, val_ret, val_1);

	/* ISP/BYRP offset test */
	iq_set->size = 2;
	iq_set->regs[0].reg_addr = sizeof(u32) * 0;
	iq_set->regs[0].reg_data = val_0;
	iq_set->regs[1].reg_addr = sizeof(u32) * 1;
	iq_set->regs[1].reg_data = val_1;

	cur_iq_set->size = 0;
	cur_iq_set->regs[0].reg_addr = sizeof(u32) * 0;
	cur_iq_set->regs[0].reg_data = 0;
	cur_iq_set->regs[1].reg_addr = sizeof(u32) * 1;
	cur_iq_set->regs[1].reg_data = 0;

	ret = pablo_hw_helper_set_iq_set(addr_base, COREX_DIRECT, DEV_HW_ISP_BYRP,
					iq_set, cur_iq_set);
	KUNIT_EXPECT_EQ(test, ret, 0);

	val_ret = *(u32 *)(addr_base + sizeof(u32) * 0);
	KUNIT_EXPECT_EQ(test, val_ret, val_0);

	val_ret = *(u32 *)(addr_base + sizeof(u32) * 1);
	KUNIT_EXPECT_EQ(test, val_ret, val_1);

	/* cur_iq_set == iq_set */
	iq_set->size = 2;
	iq_set->regs[0].reg_addr = sizeof(u32) * 0;
	iq_set->regs[0].reg_data = val_0;
	iq_set->regs[1].reg_addr = sizeof(u32) * 1;
	iq_set->regs[1].reg_data = val_1;

	cur_iq_set->size = 2;
	cur_iq_set->regs[0].reg_addr = sizeof(u32) * 0;
	cur_iq_set->regs[0].reg_data = val_0;
	cur_iq_set->regs[1].reg_addr = sizeof(u32) * 1;
	cur_iq_set->regs[1].reg_data = val_1;

	ret = pablo_hw_helper_set_iq_set(addr_base, COREX_DIRECT, DEV_HW_ISP_BYRP,
					iq_set, cur_iq_set);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_set_rta_regs_kunit_test(struct kunit *test)
{
	bool ret;
	struct is_hw_iq *iq_set = &hw_ip->iq_set[0];
	bool skip;

	skip = true;
	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_TRUE(test, ret);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_FALSE(test, ret);

	skip = false;
	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_TRUE(test, ret);

	set_bit(CR_SET_CONFIG, &iq_set->state);
	ret = CALL_HW_HELPER_OPS(hw_ip, set_rta_regs, 0, COREX_DIRECT, skip, frame, NULL);
	KUNIT_EXPECT_EQ(test, ret, (bool)false);
	KUNIT_EXPECT_EQ(test, test_bit(CR_SET_EMPTY, &iq_set->state), 1);
}

static void pablo_hw_helper_lib_shot_kunit_test(struct kunit *test)
{
	int ret;
	u32 skip;
	u32 param_set[1];

	hw_ip->id = DEV_HW_MCSC0;
	skip = 0;
	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, 0, skip, frame, NULL, NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	skip = 1;
	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, 0, skip, frame, NULL, NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	skip = 0;
	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, 0, skip, frame, NULL, NULL);
	KUNIT_EXPECT_NE(test, ret, 0);

	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, 0, skip, frame, lib, NULL);
	KUNIT_EXPECT_NE(test, ret, 0);

	ret = CALL_HW_HELPER_OPS(hw_ip, lib_shot, 0, skip, frame, lib, param_set);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_alloc_free_iqset_kunit_test(struct kunit *test)
{
	int ret, i;
	struct is_hw_iq *iq_set, *cur_iq_set;

	ret = CALL_HW_HELPER_OPS(hw_ip, alloc_iqset, MAX_NUM_CONTENTS);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		iq_set = &hw_ip->iq_set[i];
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, iq_set->regs);
		KUNIT_EXPECT_EQ(test, test_bit(CR_SET_CONFIG, &iq_set->state), 0);
		KUNIT_EXPECT_EQ(test, test_bit(CR_SET_EMPTY, &iq_set->state), 1);
	}

	for (i = 0; i < COREX_MAX; i++) {
		cur_iq_set = &hw_ip->cur_hw_iq_set[i];
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, iq_set->regs);
	}

	CALL_HW_HELPER_OPS(hw_ip, free_iqset);

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		iq_set = &hw_ip->iq_set[i];
		KUNIT_EXPECT_EQ(test, (u64)iq_set->regs, (u64)0);
	}

	for (i = 0; i < COREX_MAX; i++) {
		cur_iq_set = &hw_ip->cur_hw_iq_set[i];
		KUNIT_EXPECT_EQ(test, (u64)cur_iq_set->regs, (u64)0);
	}
}

static void pablo_hw_helper_set_regs_kunit_test(struct kunit *test)
{
	int ret;
	struct cr_set regs[MAX_NUM_CONTENTS];
	u32 regs_size = 0;
	u32 instance = 0;
	struct is_hw_iq *iq_set = &hw_ip->iq_set[instance];

	ret = CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, NULL, regs_size);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, regs, regs_size);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	ret = CALL_HW_HELPER_OPS(hw_ip, alloc_iqset, MAX_NUM_CONTENTS);
	KUNIT_EXPECT_EQ(test, ret, 0);

	clear_bit(CR_SET_EMPTY, &iq_set->state);
	ret = CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, regs, regs_size);
	KUNIT_EXPECT_EQ(test, ret, -EBUSY);

	regs_size = MAX_NUM_CONTENTS;
	regs[0].reg_addr = 0x12345678;
	regs[0].reg_data = 0x98765432;
	set_bit(CR_SET_EMPTY, &iq_set->state);
	ret = CALL_HW_HELPER_OPS(hw_ip, set_regs, instance, regs, regs_size);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(CR_SET_CONFIG, &iq_set->state), 1);
	KUNIT_EXPECT_EQ(test, iq_set->regs[0].reg_addr, regs[0].reg_addr);
	KUNIT_EXPECT_EQ(test, iq_set->regs[0].reg_data, regs[0].reg_data);

	CALL_HW_HELPER_OPS(hw_ip, free_iqset);
}

static void pablo_hw_helper_restore_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, restore, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, restore, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	set_bit(HW_OPEN, &hw_ip->state);
	ret = CALL_HW_HELPER_OPS(hw_ip, restore, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_get_meta_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, get_meta, instance, frame, NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, get_meta, instance, frame, NULL);
	KUNIT_EXPECT_NE(test, ret, 0);

	frame->shot = shot;
	ret = CALL_HW_HELPER_OPS(hw_ip, get_meta, instance, frame, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_get_cap_meta_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	u32 fcount = 0;
	u32 size = 0;
	u64 addr = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, get_cap_meta, instance, NULL, fcount, size, addr);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, get_cap_meta, instance, NULL, fcount, size, addr);
	KUNIT_EXPECT_NE(test, ret, 0);

	ret = CALL_HW_HELPER_OPS(hw_ip, get_cap_meta, instance, lib, fcount, size, addr);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_load_cal_data_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, load_cal_data, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, load_cal_data, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_load_setfile_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, -ESRCH);

	set_bit(HW_INIT, &hw_ip->state);
	ret = CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->setfile[0].using_count = 1;
	ret = CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(HW_TUNESET, &hw_ip->state), 1);
}

static void pablo_hw_helper_apply_setfile_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	u32 scenario = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, lib, scenario);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, lib, scenario);
	KUNIT_EXPECT_EQ(test, ret, -ESRCH);

	set_bit(HW_INIT, &hw_ip->state);
	hw_ip->setfile[0].using_count = 0;
	ret = CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, lib, scenario);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->setfile[0].using_count = 1;
	hw_ip->setfile[0].index[scenario] = 2;
	ret = CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, lib, scenario);
	KUNIT_EXPECT_EQ(test, ret, -EINVAL);

	hw_ip->setfile[0].using_count = 1;
	hw_ip->setfile[0].index[scenario] = 0;
	ret = CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, NULL, scenario);
	KUNIT_EXPECT_NE(test, ret, 0);

	ret = CALL_HW_HELPER_OPS(hw_ip, apply_setfile, instance, lib, scenario);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_delete_setfile_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	clear_bit(HW_INIT, &hw_ip->state);
	ret = CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(HW_INIT, &hw_ip->state);
	hw_ip->setfile[0].using_count = 0;
	ret = CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->setfile[0].using_count = 1;
	ret = CALL_HW_HELPER_OPS(hw_ip, load_setfile, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_HW_HELPER_OPS(hw_ip, delete_setfile, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, test_bit(HW_TUNESET, &hw_ip->state), 0);
}

static void pablo_hw_helper_init_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	bool flag = 0;
	u32 f_type = 0;
	u32 lf_type = LIB_FUNC_BYRP;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, lib, lf_type);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_HW_HELPER_OPS(hw_ip, init, instance, lib, flag, f_type, lf_type);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, lib, lf_type);
	KUNIT_EXPECT_EQ(test, ret, 0);
	ret = CALL_HW_HELPER_OPS(hw_ip, init, instance, lib, flag, f_type, lf_type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_deinit_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, deinit, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, deinit, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_close_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, close, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, close, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_open_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;
	u32 lf_type = LIB_FUNC_BYRP;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, lib, lf_type);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, open, instance, lib, lf_type);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_disable_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, disable, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	clear_bit(HW_RUN, &hw_ip->state);
	ret = CALL_HW_HELPER_OPS(hw_ip, disable, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	set_bit(HW_RUN, &hw_ip->state);
	ret = CALL_HW_HELPER_OPS(hw_ip, disable, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_helper_notify_timeout_kunit_test(struct kunit *test)
{
	int ret;
	u32 instance = 0;

	hw_ip->id = DEV_HW_MCSC0;
	ret = CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_ISP_BYRP;
	ret = CALL_HW_HELPER_OPS(hw_ip, notify_timeout, instance, lib);
	KUNIT_EXPECT_EQ(test, ret, 0);
}


static int pablo_hw_helper_kunit_test_init(struct kunit *test)
{
	int ret;

	hw_ip = kunit_kzalloc(test, sizeof(struct is_hw_ip), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hw_ip);

	hw_ip->hardware = kunit_kzalloc(test, sizeof(struct is_hardware), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hw_ip->hardware);

	frame = kunit_kzalloc(test, sizeof(struct is_frame), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);

	shot = kunit_kzalloc(test, sizeof(struct camera2_shot), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, shot);

	addr_base = kunit_kzalloc(test, 0x10000, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr_base);

	addr_contents = kunit_kzalloc(test, sizeof(struct cr_set) * MAX_NUM_CONTENTS, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr_contents);
	addr_cur_contents = kunit_kzalloc(test, sizeof(struct cr_set) * MAX_NUM_CONTENTS, 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, addr_cur_contents);

	ret = pablo_hw_helper_probe(hw_ip);
	KUNIT_EXPECT_EQ(test, ret, 0);

	ret = pablo_hw_chain_info_probe(hw_ip->hardware);
	KUNIT_EXPECT_EQ(test, ret, 0);

	lib = kunit_kzalloc(test, sizeof(struct is_lib_isp), 0);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, lib);

	pablo_kunit_make_mock_interface_ddk(lib);

	return 0;
}

static void pablo_hw_helper_kunit_test_exit(struct kunit *test)
{
	kunit_kfree(test, lib);
	kunit_kfree(test, addr_cur_contents);
	kunit_kfree(test, addr_contents);
	kunit_kfree(test, addr_base);
	kunit_kfree(test, shot);
	kunit_kfree(test, frame);
	kunit_kfree(test, hw_ip->hardware);
	kunit_kfree(test, hw_ip);
}

static struct kunit_case pablo_hw_helper_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_helper_dummy_kunit_test),
	KUNIT_CASE(pablo_hw_helper_probe_kunit_test),
	KUNIT_CASE(pablo_hw_helper_set_iq_set_kunit_test),
	KUNIT_CASE(pablo_hw_helper_set_rta_regs_kunit_test),
	KUNIT_CASE(pablo_hw_helper_lib_shot_kunit_test),
	KUNIT_CASE(pablo_hw_helper_alloc_free_iqset_kunit_test),
	KUNIT_CASE(pablo_hw_helper_set_regs_kunit_test),
	KUNIT_CASE(pablo_hw_helper_restore_kunit_test),
	KUNIT_CASE(pablo_hw_helper_get_meta_kunit_test),
	KUNIT_CASE(pablo_hw_helper_get_cap_meta_kunit_test),
	KUNIT_CASE(pablo_hw_helper_load_cal_data_kunit_test),
	KUNIT_CASE(pablo_hw_helper_load_setfile_kunit_test),
	KUNIT_CASE(pablo_hw_helper_apply_setfile_kunit_test),
	KUNIT_CASE(pablo_hw_helper_delete_setfile_kunit_test),
	KUNIT_CASE(pablo_hw_helper_init_kunit_test),
	KUNIT_CASE(pablo_hw_helper_deinit_kunit_test),
	KUNIT_CASE(pablo_hw_helper_close_kunit_test),
	KUNIT_CASE(pablo_hw_helper_open_kunit_test),
	KUNIT_CASE(pablo_hw_helper_disable_kunit_test),
	KUNIT_CASE(pablo_hw_helper_notify_timeout_kunit_test),
	{},
};

struct kunit_suite pablo_hw_helper_kunit_test_suite = {
	.name = "pablo-hw-helper-v1-kunit-test",
	.init = pablo_hw_helper_kunit_test_init,
	.exit = pablo_hw_helper_kunit_test_exit,
	.test_cases = pablo_hw_helper_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_helper_kunit_test_suite);

MODULE_LICENSE("GPL v2");
