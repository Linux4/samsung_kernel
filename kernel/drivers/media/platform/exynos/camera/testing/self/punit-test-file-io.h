// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_SELF_TEST_FILE_IO_H
#define PABLO_SELF_TEST_FILE_IO_H

#include <linux/debugfs.h>

#include "pablo-mem.h"

enum pst_blob_node {
	PST_BLOB_BYRP_DMA_INPUT,
	PST_BLOB_RGBP_DMA_INPUT,
	PST_BLOB_MCFP_DMA_INPUT,
	PST_BLOB_YUVP_DMA_INPUT,
	PST_BLOB_CSTAT_DMA_INPUT,
	PST_BLOB_DMA_INPUT_MAX,

	PST_BLOB_BYRP_BYR = PST_BLOB_DMA_INPUT_MAX,
	PST_BLOB_BYRP_BYR_PROCESSED,
	PST_BLOB_RGBP_YUV,
	PST_BLOB_RGBP_RGB,
	PST_BLOB_MCFP_W,
	PST_BLOB_MCFP_YUV,
	PST_BLOB_YUVP_YUV,
	PST_BLOB_MCS_OUTPUT0,
	PST_BLOB_MCS_OUTPUT1,
	PST_BLOB_MCS_OUTPUT2,
	PST_BLOB_MCS_OUTPUT3,
	PST_BLOB_MCS_OUTPUT4,
	PST_BLOB_MCS_OUTPUT5,
	PST_BLOB_CSTAT_FDPIG,
	PST_BLOB_CSTAT_LMEDS0,
	PST_BLOB_CSTAT_DRC,
	PST_BLOB_CSTAT_SAT,
	PST_BLOB_CSTAT_CAV,
	PST_BLOB_NODE_MAX,
};

static const char *const blob_pst_name[PST_BLOB_NODE_MAX] = {
	[PST_BLOB_BYRP_DMA_INPUT] = "byrp_in",	    [PST_BLOB_RGBP_DMA_INPUT] = "rgbp_in",
	[PST_BLOB_MCFP_DMA_INPUT] = "mcfp_in",	    [PST_BLOB_YUVP_DMA_INPUT] = "yuvp_in",
	[PST_BLOB_CSTAT_DMA_INPUT] = "cstat_in",    [PST_BLOB_BYRP_BYR] = "byrp_byr",
	[PST_BLOB_BYRP_BYR_PROCESSED] = "byrp_zsl", [PST_BLOB_RGBP_YUV] = "rgbp_yuv",
	[PST_BLOB_RGBP_RGB] = "rgbp_rgb",	    [PST_BLOB_MCFP_W] = "mcfp_w",
	[PST_BLOB_MCFP_YUV] = "mcfp_yuv",	    [PST_BLOB_YUVP_YUV] = "yuvp_yuv",
	[PST_BLOB_MCS_OUTPUT0] = "mcs_ouput0",	    [PST_BLOB_MCS_OUTPUT1] = "mcs_ouput1",
	[PST_BLOB_MCS_OUTPUT2] = "mcs_ouput2",	    [PST_BLOB_MCS_OUTPUT3] = "mcs_ouput3",
	[PST_BLOB_MCS_OUTPUT4] = "mcs_ouput4",	    [PST_BLOB_MCS_OUTPUT5] = "mcs_ouput5",
	[PST_BLOB_CSTAT_FDPIG] = "cstat_fdpig",	    [PST_BLOB_CSTAT_LMEDS0] = "cstat_lme_ds0",
	[PST_BLOB_CSTAT_DRC] = "cstat_drc",	    [PST_BLOB_CSTAT_SAT] = "cstat_sat",
	[PST_BLOB_CSTAT_CAV] = "cstat_cav"
};

enum pst_blob_buf_act {
	PST_BLOB_BUF_FREE,
	PST_BLOB_BUF_ALLOC,
	PST_BLOB_BUF_ACT_MAX,
};

struct pablo_self_test_file_io {
	struct dentry *root;

	struct debugfs_blob_wrapper blob[PST_BLOB_NODE_MAX];
};

struct pablo_self_test_file_io *get_pst_fio(void);
int pablo_self_test_file_io_probe(void);

void pst_blob_inject(enum pst_blob_node node, struct is_priv_buf *pb);
void pst_blob_dump(enum pst_blob_node node, struct is_priv_buf *pb);

#endif
