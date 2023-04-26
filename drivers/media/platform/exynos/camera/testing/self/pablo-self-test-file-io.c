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

#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/debugfs.h>

#include "pablo-self-test-file-io.h"

static int pst_set_blob(const char *val, const struct kernel_param *kp);
static int pst_get_blob(char *buffer, const struct kernel_param *kp);
static const struct kernel_param_ops pablo_blob_param_ops = {
	.set = pst_set_blob,
	.get = pst_get_blob,
};
module_param_cb(blob_buf, &pablo_blob_param_ops, NULL, 0644);

static struct pablo_self_test_file_io pst_fio;

struct pablo_self_test_file_io *get_pst_fio(void)
{
	return &pst_fio;
}

static int pst_open_show(struct seq_file *buf, void *d)
{
	return 0;
}

static int pst_open(struct inode *inode, struct file *file)
{
	return single_open(file, pst_open_show, inode->i_private);
}

static ssize_t pst_byrp_in_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	void *to = pst_fio.blob[PST_BLOB_BYRP_DMA_INPUT].data;
	size_t size = pst_fio.blob[PST_BLOB_BYRP_DMA_INPUT].size;

	return simple_write_to_buffer(to, size, ppos, user_buf, count);
}

static const struct file_operations pst_byrp_in_fops = {
	.open		= pst_open,
	.read		= seq_read,
	.write		= pst_byrp_in_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t pst_rgbp_in_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	void *to = pst_fio.blob[PST_BLOB_RGBP_DMA_INPUT].data;
	size_t size = pst_fio.blob[PST_BLOB_RGBP_DMA_INPUT].size;

	return simple_write_to_buffer(to, size, ppos, user_buf, count);
}

static const struct file_operations pst_rgbp_in_fops = {
	.open		= pst_open,
	.read		= seq_read,
	.write		= pst_rgbp_in_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t pst_mcfp_in_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	void *to = pst_fio.blob[PST_BLOB_MCFP_DMA_INPUT].data;
	size_t size = pst_fio.blob[PST_BLOB_MCFP_DMA_INPUT].size;

	return simple_write_to_buffer(to, size, ppos, user_buf, count);
}

static const struct file_operations pst_mcfp_in_fops = {
	.open		= pst_open,
	.read		= seq_read,
	.write		= pst_mcfp_in_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t pst_yuvp_in_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	void *to = pst_fio.blob[PST_BLOB_YUVP_DMA_INPUT].data;
	size_t size = pst_fio.blob[PST_BLOB_YUVP_DMA_INPUT].size;

	return simple_write_to_buffer(to, size, ppos, user_buf, count);
}

static const struct file_operations pst_yuvp_in_fops = {
	.open		= pst_open,
	.read		= seq_read,
	.write		= pst_yuvp_in_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t pst_cstat_in_write(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	void *to = pst_fio.blob[PST_BLOB_CSTAT_DMA_INPUT].data;
	size_t size = pst_fio.blob[PST_BLOB_CSTAT_DMA_INPUT].size;

	return simple_write_to_buffer(to, size, ppos, user_buf, count);
}

static const struct file_operations pst_cstat_in_fops = {
	.open		= pst_open,
	.read		= seq_read,
	.write		= pst_cstat_in_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

int pablo_self_test_file_io_probe(void)
{
	pst_fio.root = debugfs_create_dir("pablo-self-test", NULL);
	if (IS_ERR(pst_fio.root))
		return -ENOMEM;

	/* RDMA */
	debugfs_create_file("byrp_in", 0440, pst_fio.root, NULL, &pst_byrp_in_fops);
	debugfs_create_file("rgbp_in", 0440, pst_fio.root, NULL, &pst_rgbp_in_fops);
	debugfs_create_file("mcfp_in", 0440, pst_fio.root, NULL, &pst_mcfp_in_fops);
	debugfs_create_file("yuvp_in", 0440, pst_fio.root, NULL, &pst_yuvp_in_fops);
	debugfs_create_file("cstat_in", 0440, pst_fio.root, NULL, &pst_cstat_in_fops);

	/* WDMA */
	debugfs_create_blob("byrp_byr", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_BYRP_BYR]);
	debugfs_create_blob("byrp_zsl", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_BYRP_BYR_PROCESSED]);

	debugfs_create_blob("rgbp_yuv", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_RGBP_YUV]);
	debugfs_create_blob("rgbp_rgb", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_RGBP_RGB]);

	debugfs_create_blob("mcfp_w", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_MCFP_W]);
	debugfs_create_blob("mcfp_yuv", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_MCFP_YUV]);

	debugfs_create_blob("yuvp_yuv", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_YUVP_YUV]);

	debugfs_create_blob("mcs_ouput0", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_MCS_OUTPUT0]);
	debugfs_create_blob("mcs_ouput1", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_MCS_OUTPUT1]);
	debugfs_create_blob("mcs_ouput2", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_MCS_OUTPUT2]);
	debugfs_create_blob("mcs_ouput3", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_MCS_OUTPUT3]);
	debugfs_create_blob("mcs_ouput4", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_MCS_OUTPUT4]);
	debugfs_create_blob("mcs_ouput5", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_MCS_OUTPUT5]);

	debugfs_create_blob("cstat_fdpig", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_CSTAT_FDPIG]);
	debugfs_create_blob("cstat_lme_ds0", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_CSTAT_LMEDS0]);
	debugfs_create_blob("cstat_drc", 0400, pst_fio.root, &pst_fio.blob[PST_BLOB_CSTAT_DRC]);

	return 0;
}

static size_t pst_get_blob_size(enum pst_blob_node node)
{
	/* TODO: actual size calculation */
	return SZ_32M;
}

static int pst_set_blob(const char *val, const struct kernel_param *kp)
{
	int ret, argc;
	char **argv;
	u32 act, node, size, i;

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
	case PST_BLOB_BUF_FREE:
		for (i = 0; i < PST_BLOB_NODE_MAX; i++) {
			vfree(pst_fio.blob[i].data);
			pst_fio.blob[i].data = NULL;
			pst_fio.blob[i].size = 0;
		}
		break;
	case PST_BLOB_BUF_ALLOC:
		if (argc < 2) {
			pr_err("Not enough parameters. %d < 3\n", argc);
			pr_err("ex) echo <1> <node> <size>\n");
			ret = -EINVAL;
			goto func_exit;
		}

		ret = kstrtouint(argv[1], 0, &node);
		if (argc > 2)
			ret |= kstrtouint(argv[2], 0, &size);
		else
			size = pst_get_blob_size(node);

		if (ret) {
			pr_err("%s: Invalid parameters(ret %d)\n", __func__, ret);
			goto func_exit;
		}

		vfree(pst_fio.blob[node].data);
		pst_fio.blob[node].size = 0;

		if (size) {
			pst_fio.blob[node].data = vmalloc(size);
			if (!pst_fio.blob[node].data) {
				pr_err("[BLBO %d] failed to alloc size(%d)\n", node, size);
				ret = -ENOMEM;
				goto func_exit;
			}

			pst_fio.blob[node].size = size;
		}
		break;
	default:
		pr_err("%s: NOT supported act %u\n", __func__, act);
		goto func_exit;
	}

func_exit:
	argv_free(argv);
	return ret;
}

static int pst_get_blob(char *buffer, const struct kernel_param *kp)
{
	int ret = 0, i;

	for (i = 0; i < PST_BLOB_NODE_MAX; i++)
		ret += sprintf(buffer, "%s[%d: data(0x%p), size(%lu)]\n", buffer,
				i, pst_fio.blob[i].data, pst_fio.blob[i].size);

	return ret;
}

void pst_blob_inject(enum pst_blob_node node, struct is_priv_buf *pb)
{
	size_t min_size;

	if (node >= PST_BLOB_NODE_MAX)
		return;

	min_size = min(pb->size, pst_fio.blob[node].size);
	if (min_size && pst_fio.blob[node].data) {
		memcpy(pb->kva, pst_fio.blob[node].data, min_size);
		pr_info("[BLOB %d] %s done\n", node, __func__);
	}

	/* cache clean */
	CALL_BUFOP(pb, sync_for_device, pb, 0, pb->size, DMA_TO_DEVICE);
}

void pst_blob_dump(enum pst_blob_node node, struct is_priv_buf *pb)
{
	if (node >= PST_BLOB_NODE_MAX)
		return;

	/* cache inv */
	CALL_BUFOP(pb, sync_for_cpu, pb, 0, pb->size, DMA_FROM_DEVICE);

	vfree(pst_fio.blob[node].data);
	pst_fio.blob[node].size = 0;

	pst_fio.blob[node].data = vmalloc(pb->size);
	if (pst_fio.blob[node].data) {
		memcpy(pst_fio.blob[node].data, pb->kva, pb->size);
		pst_fio.blob[node].size = pb->size;
		pr_info("[BLOB %d] %s done\n", node, __func__);
	} else {
		pr_err("[BLOB %d] failed to alloc\n", node);
	}
}
