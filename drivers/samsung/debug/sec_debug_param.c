// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 */

#include <linux/of.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/blkdev.h>
#include <linux/uio.h>
#include <linux/vmalloc.h>
#include <linux/mount.h>
#include <linux/sec_debug.h>
#include "sec_debug_internal.h"

/* common header structure for DEBUG partition */
enum dprm {
	DPRM_HSDP = 1,
	DPRM_BRDP = 4,
	DPRM_SUMM = 6,
	MAX_DPRM
};

#define SDGD_EACH_DUMP_SIZE				(SZ_1M)
#define SDGD_BORE_DUMP_SIZE				(SDGD_EACH_DUMP_SIZE / 2)

static struct block_device *sdg_bdev;
static const char *bdev_path = "/dev/block/by-name/param";

static ssize_t sdg_part_read(void *buf, size_t bytes, loff_t pos)
{
	struct block_device *bdev;
	struct file file;
	struct kiocb kiocb;
	struct iov_iter iter;
	struct kvec iov = {.iov_base = buf, .iov_len = bytes};

	bdev = sdg_bdev;

	pr_debug("%s: start\n", __func__);

	memset(&file, 0, sizeof(struct file));
	file.f_mapping = bdev->bd_inode->i_mapping;
	file.f_flags = O_DSYNC | __O_SYNC | O_NOATIME;
	file.f_inode = bdev->bd_inode;
	file_ra_state_init(&file.f_ra, file.f_mapping);

	init_sync_kiocb(&kiocb, &file);
	kiocb.ki_pos = pos;
	iov_iter_kvec(&iter, READ | ITER_KVEC, &iov, 1, bytes);

	pr_debug("%s: end\n", __func__);

	return generic_file_read_iter(&kiocb, &iter);
}

static int init_sdg_bdev(void)
{
	struct block_device *bdev;
	fmode_t mode = FMODE_READ | FMODE_WRITE;
	int err = 0;

	pr_debug("%s: start\n", __func__);

	bdev = blkdev_get_by_path(bdev_path, mode, NULL);
	if (IS_ERR(bdev)) {
		dev_t devt;

		pr_err("%s: plan b\n", __func__);
		devt = name_to_dev_t(bdev_path);
		if (devt == 0) {
			pr_err("'name_to_dev_t' failed!\n");
			err = -EPROBE_DEFER;
			goto err_blkdev;
		}

		bdev = blkdev_get_by_dev(devt, mode, NULL);
		if (IS_ERR(bdev)) {
			pr_err("'blkdev_get_by_dev' failed! (%ld)\n",
					PTR_ERR(bdev));
			err = -EPROBE_DEFER;
			goto err_blkdev;
		}
	}

	sdg_bdev = bdev;

	return 0;

err_blkdev:
	pr_err("can't find a block device - %s\n", bdev_path);
	return err;
}

static ssize_t secdbg_param_read_member(char __user *buf, int size, int target_offset, size_t len, loff_t *offset, loff_t dm_offset)
{
	loff_t pos = dm_offset;
	ssize_t count, ret = 0;
	char *base = NULL;

	count = min(len, (size_t)(size - pos));
	pr_debug("%s: count: %ld\n", __func__, count);

	base = vmalloc(size);
	pr_debug("%s: base: %p\n", __func__, base);

	if (!sdg_part_read(base, size, target_offset)) {
		pr_err("%s: fail to read\n", __func__);
		goto fail;
	}

	if (copy_to_user(buf, base + pos, count)) {
		pr_err("%s: fail to copy to use\n", __func__);
		ret = -EFAULT;
	} else {
		*offset += count;
		ret = count;
	}

fail:
	vfree(base);
	return ret;
}

static int get_sdg_param_member_offset_by_type(int type)
{
	int ret = -1;

	if (type > MAX_DPRM || type < DPRM_HSDP)
		return ret;

	ret = type * SDGD_EACH_DUMP_SIZE;

	if (type == DPRM_BRDP) {
		pr_debug("%s: sdg_param_member_offset: 0x%x\n", __func__, ret);
		return ret + SDGD_BORE_DUMP_SIZE;
	}

	pr_debug("%s: sdg_param_member_offset: 0x%x\n", __func__, ret);
	return ret;
}

static ssize_t secdbg_param_dhst_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	ssize_t ret = 0;
	ssize_t in_dhst = 0;
	int sdg_data_dhst_offset, sdg_data_bore_offset;
	loff_t bore_offset = *offset;

	if (!sdg_bdev) {
		pr_err("%s: re-init sdg bdev\n", __func__);
		ret = (ssize_t)init_sdg_bdev();
		if (ret)
			return 0;
	}

	sdg_data_dhst_offset = get_sdg_param_member_offset_by_type(DPRM_HSDP);
	if (sdg_data_dhst_offset < 0) {
		pr_err("%s: fail to get dhst_dump\n", __func__);
		return 0;
	}
	pr_debug("%s: sdg_data_dhst_offset: 0x%x\n", __func__, sdg_data_dhst_offset);

	in_dhst = (size_t)(SDGD_EACH_DUMP_SIZE - *offset);
	if (in_dhst > 0) {
		ret = secdbg_param_read_member(buf, SDGD_EACH_DUMP_SIZE, sdg_data_dhst_offset, len, offset, *offset);
	} else {
		sdg_data_bore_offset = get_sdg_param_member_offset_by_type(DPRM_BRDP);
		if (sdg_data_bore_offset < 0) {
			pr_err("%s: fail to get sdg_data_bore_offset\n", __func__);
			return 0;
		}
		pr_debug("%s: sdg_data_bore_offset: 0x%x\n", __func__, sdg_data_bore_offset);

		bore_offset -= SDGD_EACH_DUMP_SIZE;
		ret = secdbg_param_read_member(buf, SDGD_BORE_DUMP_SIZE, sdg_data_bore_offset, len, offset, bore_offset);
	}

	return ret;
}

static ssize_t secdbg_param_bore_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	ssize_t ret = 0;
	int sdg_data_bore_offset;

	if (!sdg_bdev) {
		pr_err("%s: re-init sdg bdev\n", __func__);
		ret = (ssize_t)init_sdg_bdev();
		if (ret)
			return ret;
	}

	sdg_data_bore_offset = get_sdg_param_member_offset_by_type(DPRM_BRDP);
	if (sdg_data_bore_offset < 0) {
		pr_err("%s: fail to get sdg_data_bore_offset\n", __func__);
		return 0;
	}
	pr_debug("%s: sdg_data_bore_offset: 0x%x\n", __func__, sdg_data_bore_offset);

	ret = secdbg_param_read_member(buf, SDGD_BORE_DUMP_SIZE, sdg_data_bore_offset, len, offset, *offset);

	return ret;
}

static ssize_t secdbg_param_summ_read(struct file *file, char __user *buf, size_t len, loff_t *offset)
{
	ssize_t ret = 0;
	int sdg_data_summ_offset;

	if (!sdg_bdev) {
		pr_notice("%s: re-init sdg bdev\n", __func__);
		ret = (ssize_t)init_sdg_bdev();
		if (ret)
			return ret;
	}

	sdg_data_summ_offset = get_sdg_param_member_offset_by_type(DPRM_SUMM);
	if (sdg_data_summ_offset < 0) {
		pr_err("%s: fail to get sdg_data_summ_offset\n", __func__);
		return 0;
	}
	pr_debug("%s: sdg_data_summ_offset: 0x%x\n", __func__, sdg_data_summ_offset);

	ret = secdbg_param_read_member(buf, SDGD_EACH_DUMP_SIZE, sdg_data_summ_offset, len, offset, *offset);

	return ret;
}

static const struct file_operations dparam_dhst_file_ops = {
	.owner = THIS_MODULE,
	.read = secdbg_param_dhst_read,
};

static const struct file_operations dparam_bore_file_ops = {
	.owner = THIS_MODULE,
	.read = secdbg_param_bore_read,
};

static const struct file_operations dparam_summ_file_ops = {
	.owner = THIS_MODULE,
	.read = secdbg_param_summ_read,
};

static int __init secdbg_param_init(void)
{
	struct proc_dir_entry *dhst_entry;
	struct proc_dir_entry *bore_entry;
	struct proc_dir_entry *summ_entry;

	dhst_entry = proc_create("debug_history", S_IFREG | 0444, NULL, &dparam_dhst_file_ops);
	if (!dhst_entry) {
		pr_err("%s: failed to create proc entry param part debug history log\n", __func__);
		return 0;
	}

	bore_entry = proc_create("boot_reset", S_IFREG | 0444, NULL, &dparam_bore_file_ops);
	if (!bore_entry) {
		pr_err("%s: failed to create proc entry param part boot reset log\n", __func__);
		return 0;
	}

	summ_entry = proc_create("reset_summary", S_IFREG | 0444, NULL, &dparam_summ_file_ops);
	if (!summ_entry) {
		pr_err("%s: failed to create proc entry param part dump summary\n", __func__);
		return 0;
	}

	pr_info("%s: success to create proc entry\n", __func__);

	proc_set_size(dhst_entry, (size_t)(SDGD_EACH_DUMP_SIZE));
	proc_set_size(bore_entry, (size_t)(SDGD_BORE_DUMP_SIZE));
	proc_set_size(summ_entry, (size_t)(SDGD_EACH_DUMP_SIZE));

	return 0;
}
module_init(secdbg_param_init);

static void __exit secdbg_param_exit(void)
{
}
module_exit(secdbg_param_exit);

MODULE_DESCRIPTION("Samsung Debug Param Driver");
MODULE_LICENSE("GPL v2");
