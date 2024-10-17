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

#include "punit-test-file-io.h"
#include "pablo-blob.h"

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

static ssize_t pst_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct dentry *dentry = file->f_path.dentry;
	struct debugfs_blob_wrapper *blob = dentry->d_inode->i_private;

	return simple_write_to_buffer(blob->data, blob->size, ppos, user_buf, count);
}

static const struct file_operations pst_dma_in_fops = {
	.open = pst_open,
	.read = seq_read,
	.write = pst_write,
	.llseek = seq_lseek,
	.release = single_release,
};

int pablo_self_test_file_io_probe(void)
{
	int i;

	pst_fio.root = debugfs_create_dir("pablo-self-test", NULL);
	if (IS_ERR(pst_fio.root))
		return -ENOMEM;

	/* RDMA */
	for (i = PST_BLOB_BYRP_DMA_INPUT; i < PST_BLOB_DMA_INPUT_MAX; i++)
		debugfs_create_file(blob_pst_name[i], 0440, pst_fio.root, &pst_fio.blob[i],
				    &pst_dma_in_fops);

	/* WDMA */
	for (i = PST_BLOB_DMA_INPUT_MAX; i < PST_BLOB_NODE_MAX; i++)
		debugfs_create_blob(blob_pst_name[i], 0400, pst_fio.root, &pst_fio.blob[i]);

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
	int ret;

	if (node >= PST_BLOB_NODE_MAX)
		return;

	/* cache inv */
	CALL_BUFOP(pb, sync_for_cpu, pb, 0, pb->size, DMA_FROM_DEVICE);

	ret = pablo_blob_dump(&pst_fio.blob[node], (ulong *)&pb->kva, &pb->size, 1);
	if (ret)
		pr_info("[BLOB] PUnit blob_id %d %s failed\n", node, __func__);
	else
		pr_info("[BLOB] PUnit blob_id %d %s done\n", node, __func__);
}
