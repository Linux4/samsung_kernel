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

#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include "pablo-blob.h"
#include "pablo-debug.h"
#include "is-video-config.h"

static int pablo_blob_set(const char *val, const struct kernel_param *kp);
static int pablo_blob_get(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_blob_param_ops = {
	.set = pablo_blob_set,
	.get = pablo_blob_get,
};
module_param_cb(blob, &pablo_blob_param_ops, NULL, 0644);

#define NAME_LEN 10
static char dump_lvn_name[NAME_LEN];
static char dump_pd_src_name[NAME_LEN];
static u32 dump_pd_count;
static u32 done_pd_count;

static int pablo_blob_set_dump_lvn_name(int argc, char **argv)
{
	if (argc < 2) {
		pr_err("Not enough parameters. %d < 2\n", argc);
		pr_err("ex) echo <act> <lvn_name>\n");
		return -EINVAL;
	}

	strncpy(dump_lvn_name, argv[1], NAME_LEN);

	return 0;
}

static void pablo_blob_set_clear_pd(struct pablo_blob_pd *blob_pd)
{
	int i;

	for (i = 0; i < PD_BLOB_MAX; i++) {
		vfree(blob_pd[i].blob.data);
		vfree(blob_pd[i].blob_stat_info.data);

		blob_pd[i].blob.data = NULL;
		blob_pd[i].blob.size = 0;

		blob_pd[i].blob_stat_info.data = NULL;
		blob_pd[i].blob_stat_info.size = 0;
	}

	dump_pd_src_name[0] = '\0';
	dump_pd_count = 0;
	done_pd_count = 0;
}

static int pablo_blob_set_dump_pd(int argc, char **argv)
{
	int ret;

	if (argc < 3) {
		pr_err("[@]Not enough parameters. %d < 3\n", argc);
		pr_err("[@]ex) echo <act> <src_name> <count>\n");
		return -EINVAL;
	}

	strncpy(dump_pd_src_name, argv[1], NAME_LEN);

	ret = kstrtouint(argv[2], 0, &dump_pd_count);
	if (ret) {
		pr_err("[@]Invalid dump_pd_count %s ret %d\n", argv[2], ret);
		return ret;
	}

	return 0;
}

static int pablo_blob_set(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	u32 act;

	argv = argv_split(GFP_KERNEL, val, &argc);
	if (!argv) {
		pr_err("No argument!\n");
		return -EINVAL;
	}

	if (argc < 1) {
		pr_err("Not enough parameters. %d < 1\n", argc);
		ret = -EINVAL;
		goto func_exit;
	}

	ret = kstrtouint(argv[0], 0, &act);
	if (ret) {
		pr_err("Invalid act %d ret %d\n", act, ret);
		goto func_exit;
	}

	switch (act) {
	case PABLO_BLOB_SET_DUMP_LVN_NAME:
		pablo_blob_set_dump_lvn_name(argc, argv);
		break;
	case PABLO_BLOB_SET_CLEAR_PD:
		pablo_blob_set_clear_pd(is_debug_get()->blob_pd);
		break;
	case PABLO_BLOB_SET_DUMP_PD:
		pablo_blob_set_dump_pd(argc, argv);
		break;
	default:
		pr_err("%s: NOT supported act %u\n", __func__, act);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int pablo_blob_get(char *buffer, const struct kernel_param *kp)
{
	return 0;
}

int pablo_blob_dump(struct debugfs_blob_wrapper *blob, ulong *kva, size_t *size, u32 num_plane)
{
	size_t size_sum = 0;
	int i;
	void *dst;

	for (i = 0; i < num_plane; i++) {
		size_sum += size[i];
		pr_info("%s: plane_size[%d]:%lu\n", __func__, i, size[i]);
	}

	if (!size_sum)
		return -EINVAL;

	vfree(blob->data);
	blob->size = 0;

	blob->data = vmalloc(size_sum);
	if (!blob->data)
		return -ENOMEM;

	blob->size = size_sum;
	dst = blob->data;

	for (i = 0; i < num_plane; i++) {
		memcpy(dst, (void *)kva[i], size[i]);
		dst += size[i];
	}

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_blob_dump);

static int lvn_name_to_blob_id(struct pablo_blob_lvn *blob, const char *lvn_name)
{
	int blob_id;

	for (blob_id = 0; blob[blob_id].lvn_name; blob_id++) {
		if (!strncmp(lvn_name, blob[blob_id].lvn_name, NAME_LEN))
			return blob_id;
	}

	return -EINVAL;
}

int pablo_blob_lvn_dump(struct pablo_blob_lvn *blob_lvn, struct is_sub_dma_buf *buf)
{
	const char *lvn_name;
	int ret, blob_id;

	FIMC_BUG(!blob_lvn);
	FIMC_BUG(!buf);

	lvn_name = vn_name[buf->vid];
	if (strncmp(lvn_name, dump_lvn_name, NAME_LEN))
		return -ENOENT;

	dump_lvn_name[0] = '\0';

	blob_id = lvn_name_to_blob_id(blob_lvn, lvn_name);
	if (blob_id < 0) {
		pr_err("[BLOB] invalid lvn_name(%s)\n", lvn_name);
		return -EINVAL;
	}

	/* TODO: It needs to move to proper location. */
	if (!buf->kva[0])
		is_subbuf_kvmap(buf);

	ret = pablo_blob_dump(&blob_lvn[blob_id].blob, buf->kva, buf->size, buf->num_plane);
	if (ret)
		pr_err("[BLOB] %s %s failed\n", lvn_name, __func__);
	else
		pr_info("[BLOB] %s %s done\n", lvn_name, __func__);

	return ret;
}
EXPORT_SYMBOL_GPL(pablo_blob_lvn_dump);

int pablo_blob_lvn_probe(struct dentry *root, struct pablo_blob_lvn **blob_lvn,
			 const char **lvn_name, size_t array_size)
{
	int i;
	struct pablo_blob_lvn *blob_tmp;

	FIMC_BUG(!root);

	blob_tmp = kvzalloc(sizeof(struct pablo_blob_lvn) * array_size, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(blob_tmp)) {
		pr_err("[@] out of memory for blob_tmp\n");
		return -ENOMEM;
	}

	for (i = 0; i < array_size; i++) {
		blob_tmp[i].lvn_name = lvn_name[i];
		blob_tmp[i].dentry =
			debugfs_create_blob(lvn_name[i], 0400, root, &blob_tmp[i].blob);
	}

	*blob_lvn = blob_tmp;

	return 0;
}

void pablo_blob_lvn_remove(struct pablo_blob_lvn **blob_lvn, size_t array_size)
{
	int i;

	for (i = 0; i < array_size; i++)
		debugfs_remove((*blob_lvn)[i].dentry);

	kvfree(*blob_lvn);
	*blob_lvn = NULL;
}

static void wq_func_pd_dump(struct work_struct *data)
{
	int ret;
	void *dst;
	struct pablo_blob_pd *blob_pd;
	struct is_frame *frame;

	blob_pd = container_of(data, struct pablo_blob_pd, work);
	frame = blob_pd->frame;

	vfree(blob_pd->blob_stat_info.data);
	blob_pd->blob_stat_info.size = 0;

	blob_pd->blob_stat_info.data = vmalloc(PD_BLOB_STAT_INFO_MAX);
	dst = blob_pd->blob_stat_info.data;

	memcpy(dst, (void *)blob_pd->stat_info, PD_BLOB_STAT_INFO_MAX);
	blob_pd->blob_stat_info.size = PD_BLOB_STAT_INFO_MAX;

	ret = pablo_blob_dump(&blob_pd->blob, frame->kvaddr_buffer, frame->size, frame->planes);
	if (ret)
		pr_err("[@][BLOB][F%d] %s failed\n", frame->fcount, __func__);
	else
		pr_info("[@][BLOB][F%d] %s done\n", frame->fcount, __func__);
}

int pablo_blob_pd_dump(struct pablo_blob_pd *blob_pd, struct is_frame *frame, const char *src_name,
		       ...)
{
	int blob_id = done_pd_count;
	size_t len = strnlen(dump_pd_src_name, NAME_LEN);
	va_list args;

	if (!len || strncmp(src_name, dump_pd_src_name, len))
		return -ENOENT;

	if (blob_id >= dump_pd_count || blob_id >= PD_BLOB_MAX)
		return -ENFILE;

	va_start(args, src_name);
	vsnprintf(blob_pd[blob_id].stat_info, sizeof(blob_pd[blob_id].stat_info), src_name, args);
	va_end(args);

	blob_pd[blob_id].frame = frame;
	schedule_work(&blob_pd[blob_id].work);

	done_pd_count++;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_blob_pd_dump);

int pablo_blob_pd_probe(struct dentry *root, struct pablo_blob_pd **blob_pd)
{
	int i;
	struct pablo_blob_pd *blob_tmp;
	char name[TASK_COMM_LEN];
	char stat_info_name[TASK_COMM_LEN];

	FIMC_BUG(!root);

	blob_tmp = kvzalloc(sizeof(struct pablo_blob_pd) * PD_BLOB_MAX, GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(blob_tmp)) {
		pr_err("[@] out of memory for blob_tmp\n");
		return -ENOMEM;
	}

	for (i = 0; i < PD_BLOB_MAX; i++) {
		snprintf(name, sizeof(name), "PD_%d", i);
		snprintf(stat_info_name, sizeof(stat_info_name), "PD_INFO_%d", i);
		blob_tmp[i].dentry = debugfs_create_blob(name, 0400, root, &blob_tmp[i].blob);
		blob_tmp[i].dentry_stat_info = debugfs_create_blob(stat_info_name, 0400, root,
								   &blob_tmp[i].blob_stat_info);
		INIT_WORK(&blob_tmp[i].work, wq_func_pd_dump);
	}

	*blob_pd = blob_tmp;

	return 0;
}

void pablo_blob_pd_remove(struct pablo_blob_pd **blob_pd)
{
	int i;

	for (i = 0; i < PD_BLOB_MAX; i++) {
		debugfs_remove((*blob_pd)[i].dentry);
		debugfs_remove((*blob_pd)[i].dentry_stat_info);
	}

	kvfree(*blob_pd);
	*blob_pd = NULL;
}

int pablo_blob_debugfs_create_dir(const char *root_name, struct dentry **root)
{
	struct dentry *root_tmp;

	root_tmp = debugfs_create_dir(root_name, NULL);
	if (IS_ERR(root_tmp)) {
		pr_err("[@] failed to debugfs_create_dir\n");
		return -ENOMEM;
	}

	*root = root_tmp;

	return 0;
}

void pablo_blob_debugfs_remove(struct dentry *root)
{
	debugfs_remove(root);
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static struct pablo_kunit_blob_func blob_func = {
	.set_dump_lvn_name = pablo_blob_set_dump_lvn_name,
	.set_clear_pd = pablo_blob_set_clear_pd,
	.set_dump_pd = pablo_blob_set_dump_pd,
	.set = pablo_blob_set,
	.get = pablo_blob_get,
	.dump = pablo_blob_dump,
	.lvn_name_to_blob_id = lvn_name_to_blob_id,
	.lvn_dump = pablo_blob_lvn_dump,
	.lvn_probe = pablo_blob_lvn_probe,
	.lvn_remove = pablo_blob_lvn_remove,
	.wq_func_pd_dump = wq_func_pd_dump,
	.pd_dump = pablo_blob_pd_dump,
	.pd_probe = pablo_blob_pd_probe,
	.pd_remove = pablo_blob_pd_remove,
	.debugfs_create_dir = pablo_blob_debugfs_create_dir,
	.debugfs_remove = pablo_blob_debugfs_remove,
};

struct pablo_kunit_blob_func *pablo_kunit_get_blob_func(void)
{
	return &blob_func;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_blob_func);
#endif
