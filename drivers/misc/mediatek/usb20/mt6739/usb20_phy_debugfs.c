/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <musb_core.h>
#include <musb_debug.h>
#include <mtk_musb.h>
#include "usb20.h"

static struct dentry *usb20_debugfs;

static int usb20_phy_show(struct seq_file *s, void *unused)
{
	struct mt_usb_phy_data *pdata = s->private;
	int i = 0;
	u32 val = 0;

	for (i = 0; i < phy_data_cnt; i++) {
		struct mt_usb_phy_data *data = &pdata[i];

		val = USBPHY_READ32(data->offset);
		val = val >> data->shift;
		val = val & data->mask;

		seq_printf(s, "%s : device 0x%x host 0x%x\n",
				data->name, val, data->host);
		pr_info("%s, %s device 0x%x host 0x%x\n", __func__,
				data->name, val, data->host);
	}

	return 0;
}

static ssize_t usb20_phy_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct mt_usb_phy_data *pdata = s->private;
	struct mt_usb_phy_data *data;
	char buf[32];
	char name[32];
	int i = 0;
	int buf_size;
	u32 val = 0;
	u32 val2 = 0;
	int found = 0;
	int ret = 0;

	memset(buf, 0x00, sizeof(buf));
	/* Get userspace string and assure termination */
	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, ubuf, buf_size))
		return -EFAULT;

	ret = sscanf(buf, "%s %x %x", name, &val, &val2);
	pr_info("%s %s : read %d\n", __func__, buf, ret);

	for (i = 0; i < phy_data_cnt; i++) {
		data = &pdata[i];

		if (!strncmp(name, data->name, strlen(data->name))) {
			found = true;
			break;
		}
	}

	if (!found)
		pr_info("%s : %s was not found!\n", __func__, name);
	else if (val > data->mask)
		pr_info("wrong value 0x%x\n", val);
	else {
		pr_info("%s %s device 0x%x, host 0x%x\n", __func__,
				name, val, val2);
		data->value = val;
		data->host = val2;
		USBPHY_CLR32(data->offset, data->mask << data->shift);
		USBPHY_SET32(data->offset, val << data->shift);
	}

	return count;
}

#define GEN_DFS_FOPS(name) \
static int usb20_##name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, usb20_##name##_show, inode->i_private); \
} \
static const struct file_operations usb20_##name##_fops = { \
	.open = usb20_##name##_open, \
	.write = usb20_##name##_write,	\
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
	.owner = THIS_MODULE, \
}

GEN_DFS_FOPS(phy);

int usb20_phy_init_debugfs(void)
{
	struct dentry *file;
	int ret = 0;

	file = debugfs_create_file("usb_phy", S_IRUGO|S_IWUSR,
			usb20_debugfs, phy_data,
			&usb20_phy_fops);
	if (!file)
		ret = -ENOMEM;

	return ret;
}

void /* __init_or_exit */ usb20_phy_exit_debugfs(struct musb *musb)
{
	debugfs_remove_recursive(usb20_debugfs);
}
