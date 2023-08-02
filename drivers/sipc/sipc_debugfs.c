/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>

#include <linux/sipc.h>

#if defined(CONFIG_DEBUG_FS)

extern int sbuf_init_debugfs( void *root );
extern int smsg_init_debugfs( void *root );
extern int sblock_init_debugfs( void *root );
extern int smem_init_debugfs( void *root );

static int __init sipc_init_debugfs( void)
{
	struct dentry *root = debugfs_create_dir("sipc", NULL);
	if (!root)
		return -ENXIO;

	smsg_init_debugfs(root);
	sbuf_init_debugfs(root);
	sblock_init_debugfs(root);
	smem_init_debugfs(root);
	return 0;
}

__initcall(sipc_init_debugfs);

#endif /* CONFIG_DEBUG_FS */
