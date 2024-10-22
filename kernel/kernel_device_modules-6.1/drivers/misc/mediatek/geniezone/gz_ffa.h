/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2022 MediaTek Inc.
 */

struct ffa_driver *gz_get_ffa_dev(void);

struct ffa_mem_list_head {
	struct ffa_mem_list_head *prev;
	struct ffa_mem_list_head *next;
};

struct ffa_mem_record_table {
	unsigned long handle;
	/* this is original pointer got from kmalloc, can be used for free */
	void *origin_mem_region;
	/* this is page-aligned pointer, used for read/write and passed to ffa_mem */
	void *mem_region;
	uint32_t mem_size;
	void *meta_data;
	struct ffa_mem_list_head list;
};
