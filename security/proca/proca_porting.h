/*
 * This is needed backporting of source code from Kernel version 4.x
 *
 * Copyright (C) 2018 Samsung Electronics, Inc.
 *
 * Hryhorii Tur, <hryhorii.tur@partner.samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LINUX_PROCA_PORTING_H
#define __LINUX_PROCA_PORTING_H

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)

static inline struct inode *locks_inode(const struct file *f)
{
	return f->f_path.dentry->d_inode;
}

static inline ssize_t
__vfs_getxattr(struct dentry *dentry, struct inode *inode, const char *name,
	       void *value, size_t size)
{
	if (inode->i_op->getxattr)
		return inode->i_op->getxattr(dentry, name, value, size);
	else
		return -EOPNOTSUPP;
}

#define security_add_hooks(hooks, count, name) security_add_hooks(hooks, count)

#endif

#endif /* __LINUX_PROCA_PORTING_H */
