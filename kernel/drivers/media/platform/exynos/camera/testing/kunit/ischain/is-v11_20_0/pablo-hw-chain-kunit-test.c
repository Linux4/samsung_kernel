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
#include "is-hw-chain.h"
#include "is-hw.h"
#include "is-interface-ischain.h"
#include "pablo-crta-interface.h"
#include "pablo-crta-bufmgr.h"
#include "is-hw-cstat.h"
#include "is-hw-pdp.h"
#include "is-dvfs-config.h"
#include "lib/votf/pablo-votf.h"

#define KUNIT_TEST_SENSOR 0
#define KUNIT_TEST_INSTANCE 0
#define KUNIT_TEST_ADDR_VALUE 0xff

static int _test_vector_group_id[] = {
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
	GROUP_ID_MAX,
};

static int _test_vector_video_id[] = {
	IS_LVN_CSTAT0_LME_DS0,
	IS_LVN_CSTAT0_LME_DS1,
	IS_LVN_CSTAT0_FDPIG,
	IS_LVN_CSTAT0_DRC,
	IS_LVN_CSTAT0_CDS,
	IS_LVN_LME_PREV,
	IS_LVN_LME_PURE,
	IS_LVN_LME_SAD,
	IS_LVN_BYRP_BYR,
	IS_LVN_BYRP_BYR_PROCESSED,
	IS_LVN_BYRP_HDR,
	IS_LVN_RGBP_HF,
	IS_LVN_RGBP_SF,
	IS_LVN_RGBP_YUV,
	IS_LVN_RGBP_RGB,
	IS_LVN_YUVP_YUV,
	IS_LVN_YUVP_DRC,
	IS_LVN_YUVP_SEG,
	IS_LVN_MCSC_P0,
	IS_LVN_MCSC_P1,
	IS_LVN_MCSC_P2,
	IS_LVN_MCSC_P3,
	IS_LVN_MCSC_P4,
	IS_LVN_MCSC_P5,
	IS_LVN_MCFP_PREV_YUV,
	IS_LVN_MCFP_PREV_W,
	IS_LVN_MCFP_DRC,
	IS_LVN_MCFP_DP,
	IS_LVN_MCFP_MV,
	IS_LVN_MCFP_MVMIXER,
	IS_LVN_MCFP_SF,
	IS_LVN_MCFP_W,
	IS_LVN_MCFP_YUV,
	IS_LVN_BYRP_RTA_INFO,
	IS_LVN_RGBP_RTA_INFO,
	IS_LVN_MCFP_RTA_INFO,
	IS_LVN_YUVP_RTA_INFO,
	IS_LVN_LME_RTA_INFO,
	IS_VIDEO_COMMON,
};

static struct test_ctx {
	struct is_group group;
	int hw_list[sizeof(_test_vector_group_id)/sizeof(int)][GROUP_HW_MAX];
	struct is_interface_hwip itf;
	struct is_core core;
	struct v4l2_subdev subdev_csi;
	struct is_frame frame[2];
	struct pablo_crta_buf_info pcfi_buf;
	struct pablo_crta_frame_info pcfi;
	struct is_pdp pdp;
	struct is_hw_cstat cstat;
} test_ctx;

static void pablo_hw_chain_is_isr_ip_kunit_test(struct kunit *test)
{
	int i;
	irqreturn_t ret;
	struct is_interface_hwip *itf = &test_ctx.itf;

	for (i = 0; i < INTR_HWIP_MAX; i++) {
		itf->handler[i].valid = 0;
	}

	for (i = 0; i < sizeof(_test_vector_group_id)/sizeof(int); i++) {
		ret = is_hw_isr_check(test_ctx.hw_list[i][0], 0, itf);
		if (test_ctx.hw_list[i][0] == DEV_HW_3AA0 ||
				test_ctx.hw_list[i][0] == DEV_HW_3AA1 ||
				test_ctx.hw_list[i][0] == DEV_HW_3AA2 ||
				test_ctx.hw_list[i][0] == DEV_HW_LME0 ||
				test_ctx.hw_list[i][0] == DEV_HW_BYRP ||
				test_ctx.hw_list[i][0] == DEV_HW_RGBP ||
				test_ctx.hw_list[i][0] == DEV_HW_YPP ||
				test_ctx.hw_list[i][0] == DEV_HW_MCFP ||
				test_ctx.hw_list[i][0] == DEV_HW_MCSC0) {
			KUNIT_EXPECT_EQ(test, ret, IRQ_HANDLED);
		} else {
			KUNIT_EXPECT_EQ(test, ret, 0);
		}
	}
}

static void pablo_hw_chain_is_hw_group_open_kunit_test(struct kunit *test)
{
	int ret, i;
	struct is_group *group = &test_ctx.group;

	for (i = 0; i < sizeof(_test_vector_group_id)/sizeof(int); i++) {
		group->id = _test_vector_group_id[i];
		ret = is_hw_group_open(group);
		KUNIT_EXPECT_EQ(test, ret, 0);
	}
}

static void pablo_hw_chain_is_hw_get_irq_kunit_test(struct kunit *test)
{
	int ret, i;
	struct is_interface_hwip *itf = &test_ctx.itf;
	struct platform_device pdev;

	/* Failed to get irq */
	for (i = 0; i < sizeof(_test_vector_group_id)/sizeof(int); i++) {
		ret = is_hw_get_irq(itf, &pdev, test_ctx.hw_list[i][0]);
		KUNIT_EXPECT_EQ(test, ret, -EINVAL);
	}
}

static void pablo_hw_chain_is_hw_camif_cfg_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_sensor *ids = &test_ctx.core.sensor[KUNIT_TEST_SENSOR];

	/* sensor_data is null */
	ids = NULL;
	ret = is_hw_camif_cfg(ids);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	ids = &test_ctx.core.sensor[KUNIT_TEST_SENSOR];

	/* core is null */
	ids->private_data = NULL;
	ret = is_hw_camif_cfg(ids);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	ids->private_data = &test_ctx.core;

	/* csi is null */
	ret = is_hw_camif_cfg(ids);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);
}

static void pablo_hw_chain_is_hw_ischain_cfg_kunit_test(struct kunit *test)
{
	int ret;
	struct is_device_ischain *idi = &test_ctx.core.ischain[KUNIT_TEST_INSTANCE];

	/* reprocessing */
	set_bit(IS_ISCHAIN_REPROCESSING, &idi->state);
	ret = is_hw_ischain_cfg(idi);
	KUNIT_EXPECT_EQ(test, ret, 0);

	clear_bit(IS_ISCHAIN_REPROCESSING, &idi->state);
	ret = is_hw_ischain_cfg(idi);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_chain_is_hw_ischain_enable_kunit_test(struct kunit *test)
{
	int ret, i;
	struct is_core *core = &test_ctx.core;
	struct votf_dev *votfdev[VOTF_DEV_NUM];

	/* core is null */
	core = NULL;
	ret = is_hw_ischain_enable(core);
	KUNIT_EXPECT_EQ(test, ret, -ENODEV);

	core = &test_ctx.core;

	for (i = 0; i < VOTF_DEV_NUM; i++) {
		votfdev[i] = get_votf_dev(i);
		set_votf_dev(i, NULL);
	}
	ret = is_hw_ischain_enable(core);
	KUNIT_EXPECT_EQ(test, ret, 0);

	for (i = 0; i < VOTF_DEV_NUM; i++) {
		set_votf_dev(i, votfdev[i]);
	}
}

static void pablo_hw_chain_is_hw_ischain_disable_kunit_test(struct kunit *test)
{
	int ret;

	ret = is_hw_ischain_disable(NULL);
	KUNIT_EXPECT_EQ(test, ret, 0);
}

static void pablo_hw_chain_is_hw_configure_bts_scen_kunit_test(struct kunit *test)
{
	struct is_resourcemgr *resourcemgr = &test_ctx.core.resourcemgr;
	int scenario_id;

	for (scenario_id = IS_DVFS_SN_DEFAULT; scenario_id < IS_DVFS_SN_MAX; scenario_id++) {
		is_hw_configure_bts_scen(resourcemgr, scenario_id);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, resourcemgr);
	}
}

static void pablo_hw_chain_is_hw_get_capture_slot_kunit_test(struct kunit *test)
{
	int ret, i;
	struct is_frame *frame = &test_ctx.frame[0];
	dma_addr_t *taddr;
	dma_addr_t *taddr_k;

	for (i = 0; i < sizeof(_test_vector_video_id) / sizeof(int); i++) {
		taddr = NULL;
		taddr_k = NULL;
		ret = is_hw_get_capture_slot(frame, &taddr, &taddr_k, _test_vector_video_id[i]);
		if (_test_vector_video_id[i] == IS_VIDEO_COMMON) {
			/* Unsupported vid */
			KUNIT_EXPECT_EQ(test, ret, -EINVAL);
			KUNIT_EXPECT_PTR_EQ(test, taddr, NULL);
			KUNIT_EXPECT_PTR_EQ(test, taddr_k, NULL);
		} else {
			if (_test_vector_video_id[i] == IS_LVN_BYRP_RTA_INFO ||
					_test_vector_video_id[i] == IS_LVN_RGBP_RTA_INFO ||
					_test_vector_video_id[i] == IS_LVN_MCFP_RTA_INFO ||
					_test_vector_video_id[i] == IS_LVN_YUVP_RTA_INFO ||
					_test_vector_video_id[i] == IS_LVN_LME_RTA_INFO)
				KUNIT_EXPECT_PTR_NE(test, taddr_k, NULL);
			else
				KUNIT_EXPECT_PTR_NE(test, taddr, NULL);
			KUNIT_EXPECT_EQ(test, ret, 0);
		}
	}
}

static void pablo_hw_chain_is_hw_fill_target_address_kunit_test(struct kunit *test)
{
	int i;
	struct is_frame *dst_frame, *src_frame;
	dst_frame = &test_ctx.frame[0];
	src_frame = &test_ctx.frame[1];

	for (i = 0; i < sizeof(_test_vector_group_id)/sizeof(int); i++) {
		switch (test_ctx.hw_list[i][0]) {
		case DEV_HW_3AA0:
			src_frame->txpTargetAddress[0][0] = KUNIT_TEST_ADDR_VALUE;
			dst_frame->txpTargetAddress[0][0] = 0x0;
			break;
		case DEV_HW_LME0:
			src_frame->lmesTargetAddress[0] = KUNIT_TEST_ADDR_VALUE;
			dst_frame->lmesTargetAddress[0] = 0x0;
			break;
		case DEV_HW_BYRP:
			src_frame->dva_byrp_hdr[0] = KUNIT_TEST_ADDR_VALUE;
			dst_frame->dva_byrp_hdr[0] = 0x0;
			break;
		case DEV_HW_RGBP:
			src_frame->dva_rgbp_hf[0] = KUNIT_TEST_ADDR_VALUE;
			dst_frame->dva_rgbp_hf[0] = 0x0;
			break;
		case DEV_HW_YPP:
			src_frame->ixscmapTargetAddress[0] = KUNIT_TEST_ADDR_VALUE;
			dst_frame->ixscmapTargetAddress[0] = 0x0;
			break;
		case DEV_HW_MCFP:
			src_frame->dva_mcfp_prev_yuv[0] = KUNIT_TEST_ADDR_VALUE;
			dst_frame->dva_mcfp_prev_yuv[0] = 0x0;
			break;
		case DEV_HW_MCSC0:
			src_frame->sc0TargetAddress[0] = KUNIT_TEST_ADDR_VALUE;
			dst_frame->sc0TargetAddress[0] = 0x0;
			break;
		default:
			break;
		}

		is_hw_fill_target_address(test_ctx.hw_list[i][0], dst_frame, src_frame, false);

		switch (test_ctx.hw_list[i][0]) {
		case DEV_HW_3AA0:
			KUNIT_EXPECT_EQ(test, dst_frame->txpTargetAddress[0][0], KUNIT_TEST_ADDR_VALUE);
			break;
		case DEV_HW_LME0:
			KUNIT_EXPECT_EQ(test, dst_frame->lmesTargetAddress[0], KUNIT_TEST_ADDR_VALUE);
			break;
		case DEV_HW_BYRP:
			KUNIT_EXPECT_EQ(test, dst_frame->dva_byrp_hdr[0], KUNIT_TEST_ADDR_VALUE);
			break;
		case DEV_HW_RGBP:
			KUNIT_EXPECT_EQ(test, dst_frame->dva_rgbp_hf[0], KUNIT_TEST_ADDR_VALUE);
			break;
		case DEV_HW_YPP:
			KUNIT_EXPECT_EQ(test, dst_frame->ixscmapTargetAddress[0], KUNIT_TEST_ADDR_VALUE);
			break;
		case DEV_HW_MCFP:
			KUNIT_EXPECT_EQ(test, dst_frame->dva_mcfp_prev_yuv[0], KUNIT_TEST_ADDR_VALUE);
			break;
		case DEV_HW_MCSC0:
			KUNIT_EXPECT_EQ(test, dst_frame->sc0TargetAddress[0], KUNIT_TEST_ADDR_VALUE);
			break;
		default:
			break;
		}
	}
}

static void pablo_hw_chain_is_hw_get_iommu_mem_kunit_test(struct kunit *test)
{
	struct is_mem *mem = NULL;
	int i;

	for (i = 0; i < sizeof(_test_vector_video_id) / sizeof(int); i++) {
		mem = is_hw_get_iommu_mem(_test_vector_video_id[i]);
		if (_test_vector_video_id[i] == IS_VIDEO_COMMON) {
			KUNIT_EXPECT_PTR_EQ(test, mem, NULL);
		} else {
			KUNIT_EXPECT_PTR_NE(test, mem, NULL);
		}

		mem = NULL;
	}
}

static void pablo_hw_chain_is_hw_print_target_dva_kunit_test(struct kunit *test)
{
	struct is_frame *frame;
	frame = &test_ctx.frame[0];

	is_hw_print_target_dva(frame, KUNIT_TEST_INSTANCE);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, frame);
}

static void pablo_hw_chain_is_hw_config_kunit_test(struct kunit *test)
{
	int ret;
	struct is_hw_ip *hw_ip = &test_ctx.core.hardware.hw_ip[0];
	struct pablo_crta_buf_info *pcfi_buf = &test_ctx.pcfi_buf;

	hw_ip->id = DEV_HW_3AA0;
	hw_ip->priv_info = &test_ctx.cstat;
	ret = is_hw_config(hw_ip, pcfi_buf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = DEV_HW_PAF0;
	hw_ip->priv_info = &test_ctx.pdp;
	ret = is_hw_config(hw_ip, pcfi_buf);
	KUNIT_EXPECT_EQ(test, ret, 0);

	hw_ip->id = 0;
	hw_ip->priv_info = NULL;
}

static void pablo_hw_chain_is_hw_update_pcfi_kunit_test(struct kunit *test)
{
	struct is_hardware *hardware = &test_ctx.core.hardware;
	struct pablo_crta_buf_info *pcfi_buf = &test_ctx.pcfi_buf;
	struct is_frame *frame = &test_ctx.frame[0];
	struct is_group *group = &test_ctx.group;

	group->id = GROUP_ID_BYRP;
	group->device_type = IS_DEVICE_ISCHAIN;
	group->instance = KUNIT_TEST_INSTANCE;
	group->tail = group;
	group->parent = NULL;

	/* pcfi kva is null */
	pcfi_buf->kva = NULL;
	is_hw_update_pcfi(hardware, group, frame, pcfi_buf);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pcfi_buf);

	pcfi_buf->kva = &test_ctx.pcfi;

	/* cannot get hw_ip */
	hardware->chain_info_ops = NULL;
	is_hw_update_pcfi(hardware, group, frame, pcfi_buf);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pcfi_buf);

	pablo_hw_chain_info_probe(hardware);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, hardware);
	is_hw_update_pcfi(hardware, group, frame, pcfi_buf);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, pcfi_buf);
}

static int pablo_hw_chain_kunit_test_init(struct kunit *test)
{
	int ret, i;
	struct is_device_sensor *ids = &test_ctx.core.sensor[KUNIT_TEST_SENSOR];
	struct pablo_crta_buf_info *pcfi_buf = &test_ctx.pcfi_buf;

	pablo_hw_chain_info_probe(&test_ctx.core.hardware);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, test_ctx.core.hardware.chain_info_ops);

	for (i = 0; i < sizeof(_test_vector_group_id)/sizeof(int); i++) {
		ret = is_get_hw_list(_test_vector_group_id[i], test_ctx.hw_list[i]);
	}

	ids->private_data = &test_ctx.core;
	ids->subdev_csi = &test_ctx.subdev_csi;
	pcfi_buf->kva = &test_ctx.pcfi;

	return 0;
}

static void pablo_hw_chain_kunit_test_exit(struct kunit *test)
{
	int i;

	for (i = 0; i < sizeof(_test_vector_group_id)/sizeof(int); i++) {
		memset(test_ctx.hw_list[i], 0, GROUP_HW_MAX);
	}
}

static struct kunit_case pablo_hw_chain_kunit_test_cases[] = {
	KUNIT_CASE(pablo_hw_chain_is_isr_ip_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_group_open_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_get_irq_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_camif_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_ischain_cfg_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_ischain_enable_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_ischain_disable_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_configure_bts_scen_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_get_capture_slot_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_fill_target_address_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_get_iommu_mem_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_print_target_dva_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_config_kunit_test),
	KUNIT_CASE(pablo_hw_chain_is_hw_update_pcfi_kunit_test),
	{},
};

struct kunit_suite pablo_hw_chain_kunit_test_suite = {
	.name = "pablo-hw-chain-kunit-test",
	.init = pablo_hw_chain_kunit_test_init,
	.exit = pablo_hw_chain_kunit_test_exit,
	.test_cases = pablo_hw_chain_kunit_test_cases,
};
define_pablo_kunit_test_suites(&pablo_hw_chain_kunit_test_suite);

MODULE_LICENSE("GPL v2");
