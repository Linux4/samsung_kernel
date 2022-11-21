// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 * Gwanghui Lee <gwanghui.lee@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include "panel_drv.h"
#include "panel_debug.h"

#define MAX_NAME_SIZE       32

const char *panel_debugfs_name[] = {
	[PANEL_DEBUGFS_LOG] = "log",
	[PANEL_DEBUGFS_CMD_LOG] = "cmd_log",
};

static int panel_debug_log_show(struct seq_file *s)
{
	seq_printf(s, "%d\n", panel_log_level);

	return 0;
}

static int panel_debug_cmd_log_show(struct seq_file *s)
{
	seq_printf(s, "%d\n", panel_cmd_log);

	return 0;
}

static int panel_debug_simple_show(struct seq_file *s, void *unused)
{
	struct panel_debugfs *debugfs = s->private;

	switch (debugfs->id) {
	case PANEL_DEBUGFS_LOG:
		panel_debug_log_show(s);
		break;
	case PANEL_DEBUGFS_CMD_LOG:
		panel_debug_cmd_log_show(s);
		break;
	default:
		break;
	}

	return 0;
}

static ssize_t panel_debug_simple_write(struct file *file,
		const char __user *buf, size_t count, loff_t *f_ops)
{
	struct seq_file *s;
	struct panel_debugfs *debugfs;
	int rc = 0;
	int res = 0;

	s = file->private_data;
	debugfs = s->private;

	rc = kstrtoint_from_user(buf, count, 10, &res);
	if (rc)
		return rc;

	switch (debugfs->id) {
	case PANEL_DEBUGFS_LOG:
		panel_log_level = res;
		panel_info("panel_log_level: %d\n", panel_log_level);
		break;
	case PANEL_DEBUGFS_CMD_LOG:
		panel_cmd_log = res;
		panel_info("panel_cmd_log: %d\n", panel_cmd_log);
		break;
	default:
		break;
	}

	return count;
}

static int panel_debug_simple_open(struct inode *inode, struct file *file)
{
	return single_open(file, panel_debug_simple_show, inode->i_private);
}

static const struct file_operations panel_debugfs_simple_fops = {
	.open = panel_debug_simple_open,
	.write = panel_debug_simple_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int panel_create_debugfs(struct panel_device *panel)
{
	struct panel_debugfs *debugfs;
	char name[MAX_NAME_SIZE];
	int i;

	if (!panel)
		return -EINVAL;

	if (panel->id == 0)
		snprintf(name, MAX_NAME_SIZE, "panel");
	else
		snprintf(name, MAX_NAME_SIZE, "panel%d", panel->id);

	panel->d.dir = debugfs_create_dir(name, NULL);
	if (!panel->d.dir) {
		panel_err("failed to create debugfs directory(%s).\n", name);
		return -ENOENT;
	}

	for (i = 0; i < MAX_PANEL_DEBUGFS; i++) {
		debugfs = kmalloc(sizeof(struct panel_debugfs), GFP_KERNEL);
		if (!debugfs)
			return -ENOMEM;

		debugfs->private = panel;
		debugfs->id = i;
		debugfs->file = debugfs_create_file(panel_debugfs_name[i], 0660,
				panel->d.dir, debugfs, &panel_debugfs_simple_fops);
		if (!debugfs->file) {
			panel_err("failed to create debugfs file(%s)\n",
					panel_debugfs_name[i]);
			kfree(debugfs);
			return -ENOMEM;
		}
		panel->d.debugfs[i] = debugfs;
	}

	return 0;
}

void panel_destroy_debugfs(struct panel_device *panel)
{
	int i;

	debugfs_remove_recursive(panel->d.dir);
	for (i = 0; i < MAX_PANEL_DEBUGFS; i++) {
		kfree(panel->d.debugfs[i]);
		panel->d.debugfs[i] = NULL;
	}
}
