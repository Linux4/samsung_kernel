/* linux/drivers/video/mdnie_tuning.c
 *
 * Register interface file for Samsung mDNIe driver
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/fb.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/uaccess.h>
#include <linux/magic.h>

#include "mdnie.h"

int mdnie_open_file(const char *path, char **fp)
{
	struct file *filp;
	char	*dp;
	long	length;
	int     ret;
	struct super_block *sb;
	loff_t  pos = 0;

	if (!path) {
		pr_err("%s: path is invalid\n", __func__);
		return -EPERM;
	}

	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("%s: filp_open skip: %s\n", __func__, path);
		return -EPERM;
	}

	length = i_size_read(filp->f_path.dentry->d_inode);
	if (length <= 0) {
		pr_err("%s: file length is %ld\n", __func__, length);
		return -EPERM;
	}

	dp = kzalloc(length + 1, GFP_KERNEL);
	if (dp == NULL) {
		pr_err("%s: failed to alloc size %ld\n", __func__, length);
		filp_close(filp, current->files);
		return -EPERM;
	}

	ret = kernel_read(filp, pos, dp, length);
	if (ret != length) {
		/* check node is sysfs, bus this way is not safe */
		sb = filp->f_path.dentry->d_inode->i_sb;
		if ((sb->s_magic != SYSFS_MAGIC) && (length != sb->s_blocksize)) {
			pr_err("%s: read size= %d, length= %ld\n", __func__, ret, length);
			kfree(dp);
			filp_close(filp, current->files);
			return -EPERM;
		}
	}

	filp_close(filp, current->files);

	*fp = dp;

	return ret;
}
