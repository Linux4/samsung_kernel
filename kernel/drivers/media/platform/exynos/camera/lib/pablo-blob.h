// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_BLOB_H
#define PABLO_BLOB_H

#include <linux/debugfs.h>

#include "pablo-mem.h"
#include "pablo-framemgr.h"

enum pablo_blob_act {
	PABLO_BLOB_SET_RESERVED = 0,
	PABLO_BLOB_SET_DUMP_LVN_NAME = 1,
	PABLO_BLOB_SET_CLEAR_PD = 2,
	PABLO_BLOB_SET_DUMP_PD = 3,
	PABLO_BLOB_BUF_ACT_MAX,
};

struct pablo_blob_lvn {
	struct dentry *dentry;
	const char *lvn_name;
	struct debugfs_blob_wrapper blob;
};

#define PD_BLOB_MAX 30
#define PD_BLOB_STAT_INFO_MAX 50
struct pablo_blob_pd {
	struct dentry *dentry;
	struct dentry *dentry_stat_info;
	struct debugfs_blob_wrapper blob;
	struct debugfs_blob_wrapper blob_stat_info;
	struct work_struct work;
	struct is_frame *frame;
	char stat_info[PD_BLOB_STAT_INFO_MAX];
};

int pablo_blob_dump(struct debugfs_blob_wrapper *blob, ulong *kva, size_t *size, u32 num_plane);
int pablo_blob_lvn_dump(struct pablo_blob_lvn *blob_lvn, struct is_sub_dma_buf *buf);
int pablo_blob_lvn_probe(struct dentry *root, struct pablo_blob_lvn **blob_lvn,
			 const char **lvn_name, size_t array_size);
void pablo_blob_lvn_remove(struct pablo_blob_lvn **blob_lvn, size_t array_size);
int pablo_blob_pd_dump(struct pablo_blob_pd *blob_pd, struct is_frame *frame, const char *src_name,
		       ...);
int pablo_blob_pd_probe(struct dentry *root, struct pablo_blob_pd **blob_pd);
void pablo_blob_pd_remove(struct pablo_blob_pd **blob_pd);
int pablo_blob_debugfs_create_dir(const char *root_name, struct dentry **root);
void pablo_blob_debugfs_remove(struct dentry *root);

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
struct pablo_kunit_blob_func {
	int (*set_dump_lvn_name)(int, char **);
	void (*set_clear_pd)(struct pablo_blob_pd *blob_pd);
	int (*set_dump_pd)(int argc, char **argv);
	int (*set)(const char *, const struct kernel_param *);
	int (*get)(char *, const struct kernel_param *);
	int (*dump)(struct debugfs_blob_wrapper *blob, ulong *kva, size_t *size, u32 num_plane);
	int (*lvn_name_to_blob_id)(struct pablo_blob_lvn *, const char *);
	int (*lvn_dump)(struct pablo_blob_lvn *, struct is_sub_dma_buf *);
	int (*lvn_probe)(struct dentry *root, struct pablo_blob_lvn **blob_lvn,
			 const char **lvn_name, size_t array_size);
	void (*lvn_remove)(struct pablo_blob_lvn **blob_lvn, size_t array_size);
	void (*wq_func_pd_dump)(struct work_struct *data);
	int (*pd_dump)(struct pablo_blob_pd *blob_pd, struct is_frame *frame, const char *src_name,
		       ...);
	int (*pd_probe)(struct dentry *root, struct pablo_blob_pd **blob_pd);
	void (*pd_remove)(struct pablo_blob_pd **blob_pd);
	int (*debugfs_create_dir)(const char *root_name, struct dentry **root);
	void (*debugfs_remove)(struct dentry *root);
};

struct pablo_kunit_blob_func *pablo_kunit_get_blob_func(void);
#endif
#endif
