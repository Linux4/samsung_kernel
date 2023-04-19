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
	PST_BLOB_BYRP_BYR,
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
	PST_BLOB_NODE_MAX,
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
