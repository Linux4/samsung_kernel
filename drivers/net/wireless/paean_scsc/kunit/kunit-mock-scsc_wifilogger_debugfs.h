/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_SCSC_WIFILOGGER_DEBUGFS_H__
#define __KUNIT_MOCK_SCSC_WIFILOGGER_DEBUGFS_H__

#include "../scsc_wifilogger_debugfs.h"

#define scsc_wlog_register_debugfs_entry(args...)			kunit_mock_scsc_wlog_register_debugfs_entry(args)
#define scsc_register_common_debugfs_entries(args...)			kunit_mock_scsc_register_common_debugfs_entries(args)
#define dfs_open(args...)						kunit_mock_dfs_open(args)
#define init_ring_test_object(args...)					kunit_mock_init_ring_test_object(args)
#define dfs_release(args...)						kunit_mock_dfs_release(args)
#define scsc_wifilogger_debugfs_remove_top_dir_recursive(args...)	kunit_mock_scsc_wifilogger_debugfs_remove_top_dir_recursive(args)


static void *kunit_mock_scsc_wlog_register_debugfs_entry(const char *ring_name,
							 const char *fname,
							 const struct file_operations *fops,
							 void *rto,
							 struct scsc_wlog_debugfs_info *di)
{
	return NULL;
}

static void kunit_mock_scsc_register_common_debugfs_entries(char *ring_name, void *rto,
					   struct scsc_wlog_debugfs_info *di)
{
	return;
}

static int kunit_mock_dfs_open(struct inode *ino, struct file *filp)
{
	return 0;
}

static struct scsc_ring_test_object *kunit_mock_init_ring_test_object(struct scsc_wlog_ring *r)
{
	return NULL;
}

static int kunit_mock_dfs_release(struct inode *ino, struct file *filp)
{
	return 0;
}

static void kunit_mock_scsc_wifilogger_debugfs_remove_top_dir_recursive(void)
{
	return;
}
#endif
