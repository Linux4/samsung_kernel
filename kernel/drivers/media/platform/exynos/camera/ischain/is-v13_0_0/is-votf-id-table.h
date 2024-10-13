// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos pablo group manager configurations
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VOTF_ID_TABLE_H
#define IS_VOTF_ID_TABLE_H

#include "is_groupmgr_config.h"
#include "is-subdev-ctrl.h"
#include "is-groupmgr.h"
#include "is-device-sensor.h"
#include "votf/pablo-votf.h"

/* for AXI-APB interface */
#define HIST_WIDTH 2048
#define HIST_HEIGHT 144

enum IS_VOTF_IP {
	VOTF_CSIS_W0	= 0x1A28,
	VOTF_CSIS_W1	= 0x1A29,
	VOTF_PDP	= 0x1A2A,
	VOTF_BYRP_W	= 0x1B8A,
	VOTF_CSTAT_R	= 0x1A90,
	VOTF_CSTAT_W	= 0x1A91,
	VOTF_CSTAT_W1	= 0x1A92,
	VOTF_RGBP_R	= 0x1B0C,
	VOTF_RGBP_W	= 0x1B0D,
#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
	VOTF_SHRP_R	= 0x1C89,
#else
	VOTF_MCSC_R	= 0x1C85,
#endif
	VOTF_MCSC_W	= 0x1C86,
	VOTF_GDC_R	= 0x2892,
	VOTF_GDC_W	= 0x2891,
	VOTF_GDCO_R	= 0x2895,
	VOTF_GDCO_W	= 0x2894,
};

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
#define KUNIT_VOTF_SRC_IP VOTF_RGBP_W
#if IS_ENABLED(CONFIG_PABLO_V12_1_0)
#define KUNIT_VOTF_DST_IP VOTF_SHRP_R
#define KUNIT_VOTF_GROUP_ID GROUP_ID_SHRP
#else
#define KUNIT_VOTF_DST_IP VOTF_MCSC_R
#define KUNIT_VOTF_GROUP_ID GROUP_ID_MCS0
#endif
#endif

enum RGBP_VOTF_ID {
	RGBP_HF,
};

enum MCSC_VOTF_ID {
	MCSC_HF,
};

enum SHRP_VOTF_ID {
	SHRP_HF,
};

enum ISP_VOTF_ID {
	ISP_L0_UV,
	ISP_L2_Y,
	ISP_L2_UV,
	ISP_NOISE,
	ISP_HIST,
	ISP_DRC_GAIN
};

enum YUVPP_VOTF_ID {
	YUVPP_YUVNR_L0_UV = 1,
	YUVPP_YUVNR_L2_Y,
	YUVPP_YUVNR_L2_UV = 5,
	YUVPP_NOISE = 8,
	YUVPP_YUVNR_DRC_GAIN,
	YUVPP_CLAHE_HIST,
};

int is_get_votf_ip(int group_id);
int is_get_votf_id(int group_id, int entry);

u32 is_votf_get_token_size(struct votf_info *vinfo);

void is_votf_subdev_flush(struct is_group *group);
int is_votf_subdev_create_link(struct is_group *group);
#if defined(USE_VOTF_AXI_APB)
void is_votf_subdev_destroy_link(struct is_group *group);
#endif


void is_votf_get_master_vinfo(struct is_group *group, struct is_group **src_gr, int *src_id, int *src_entry);
void is_votf_get_slave_vinfo(struct is_group *group, struct is_group **dst_gr, int *dst_id, int *dst_entry);
struct is_subdev *is_votf_get_slave_leader_subdev(struct is_group *group, enum votf_service type);

int is_hw_pdp_set_votf_config(struct is_group *group, struct is_sensor_cfg *s_cfg);
int is_hw_set_hf_votf_size_config(u32 width, u32 height);
#endif
