/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_PROC_H__
#define __KUNIT_MOCK_PROC_H__

#include "../procfs.h"

#define slsi_procfs_inc_node(args...)		kunit_mock_slsi_procfs_inc_node(args)
#define slsi_procfs_open_file_generic(args...)	kunit_mock_slsi_procfs_open_file_generic(args)
#define slsi_create_proc_dir(args...)		kunit_mock_slsi_create_proc_dir(args)
#define slsi_remove_proc_dir(args...)		kunit_mock_slsi_remove_proc_dir(args)
#define slsi_procfs_dec_node(args...)		kunit_mock_slsi_procfs_dec_node(args)


static void kunit_mock_slsi_procfs_inc_node(void)
{
	return;
}

static int kunit_mock_slsi_procfs_open_file_generic(struct inode *inode, struct file *file)
{
	return 0;
}

static int kunit_mock_slsi_create_proc_dir(struct slsi_dev *sdev)
{
	return 0;
}

static void kunit_mock_slsi_remove_proc_dir(struct slsi_dev *sdev)
{
	return;
}

static void kunit_mock_slsi_procfs_dec_node(void)
{
	return;
}
#endif
