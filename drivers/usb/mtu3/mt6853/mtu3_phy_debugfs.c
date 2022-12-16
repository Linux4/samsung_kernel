/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include <linux/phy/phy.h>
#include <linux/phy/mediatek/mtk_usb_phy.h>

#include "../../../phy/mediatek/phy-mtk.h"

static struct dentry *usb_debugfs;

void u3phywrite32(struct phy *phy, int offset, int mask, int value)
{
	u32 cur_value;
	u32 new_value;

	cur_value = usb_mtkphy_io_read(phy, offset);
	new_value = (cur_value & (~mask)) | value;
	usb_mtkphy_io_write(phy, new_value, offset);
}

static int usb_phy_show(struct seq_file *s, void *unused)
{
	struct phy *phy = s->private;
	struct mtk_phy_instance *instance;
	struct mtk_phy_tuning *pdata;
	int i = 0;
	u32 val = 0;

	instance = phy_get_drvdata(phy);
	pdata = instance->phy_tuning;

	for (i = 0; i < instance->phy_data_cnt; i++) {
		struct mtk_phy_tuning *data = &pdata[i];

		val = usb_mtkphy_io_read(phy, data->offset);
		val = val >> data->shift;
		val = val & data->mask;

		seq_printf(s, "%s : device 0x%x host 0x%x\n",
				data->name, val, data->host);
		pr_info("%s, %s device 0x%x host 0x%x\n", __func__,
				data->name, val, data->host);
	}

	return 0;
}

u32 usb_phy_get_data(struct phy *phy, char *name)
{
	struct mtk_phy_instance *instance;
	struct mtk_phy_tuning *pdata;
	struct mtk_phy_tuning *data;
	bool found = 0;
	int i = 0;
	u32 val = 0;

	instance = phy_get_drvdata(phy);
	pdata = instance->phy_tuning;

	for (i = 0; i < instance->phy_data_cnt; i++) {
		data = &pdata[i];

		if (!strncmp(name, data->name, strlen(data->name))) {
			found = true;
			val = data->value;
			break;
		}
	}

	if (!found)
		pr_err("%s failed to find %s\n", __func__, name);

	return val;
}

static ssize_t usb_phy_write(struct file *file,
	const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct phy *phy = s->private;
	struct mtk_phy_instance *instance;
	struct mtk_phy_tuning *pdata, *data;
	char buf[32];
	char name[32];
	int i = 0, found = 0, ret = 0, buf_size = 0;
	u32 val = 0, val2 = 0, cur = 0;

	memset(buf, 0x00, sizeof(buf));
	/* Get userspace string and assure termination */
	buf_size = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, ubuf, buf_size))
		return -EFAULT;

	ret = sscanf(buf, "%s %x %x", name, &val, &val2);
	pr_info("%s %s : read %d\n", __func__, buf, ret);

	instance = phy_get_drvdata(phy);
	pdata = instance->phy_tuning;

	for (i = 0; i < instance->phy_data_cnt; i++) {
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

		cur = usb_mtkphy_io_read(phy, data->offset);
		cur &= (~data->mask << data->shift);
		cur |= (val << data->shift);
		usb_mtkphy_io_write(phy, cur, data->offset);
	}

	return count;
}

#define GEN_DFS_FOPS(name) \
static int usb_##name##_open(struct inode *inode, struct file *file) \
{ \
	return single_open(file, usb_##name##_show, inode->i_private); \
} \
static const struct file_operations usb_##name##_fops = { \
	.open = usb_##name##_open, \
	.write = usb_##name##_write,	\
	.read = seq_read, \
	.llseek = seq_lseek, \
	.release = single_release, \
	.owner = THIS_MODULE, \
}

GEN_DFS_FOPS(phy);

int mtu3_phy_init_debugfs(struct phy *phy)
{
	struct device_node *node =
		of_find_compatible_node(NULL,
			NULL, "mediatek,phy_tuning");
	struct device_node *nn;
	struct dentry *file;
	struct mtk_phy_instance *instance;
	struct mtk_phy_tuning *phy_data;
	int i = 0;

	if (!phy)
		return -EINVAL;

	instance = phy_get_drvdata(phy);
	file = debugfs_create_file("usb_phy", S_IRUGO | S_IWUSR,
			usb_debugfs, phy, &usb_phy_fops);
	if (!file)
		return -ENOMEM;

	instance->phy_data_cnt = of_get_child_count(node);
	if (instance->phy_data_cnt) {
		phy_data = devm_kzalloc(&phy->dev,
		     sizeof(*phy_data) + instance->phy_data_cnt * sizeof(*phy_data),
		     GFP_KERNEL);
		if (!phy_data) {
			pr_err("%s out of memory\n", __func__);
			return -ENOMEM;
		}
		instance->phy_tuning = phy_data;
		for_each_child_of_node(node, nn) {
			struct mtk_phy_tuning *data = &phy_data[i++];

			data->name = of_get_property(nn, "label", NULL);
			of_property_read_u32(nn, "offset", &data->offset);
			of_property_read_u32(nn, "shift", &data->shift);
			of_property_read_u32(nn, "mask", &data->mask);
			of_property_read_u32(nn, "value", &data->value);
			of_property_read_u32(nn, "host", &data->host);

			pr_info("%s %s : 0x%x 0x%x 0x%x 0x%x 0x%x\n",
				__func__, data->name,
				data->offset, data->shift,
				data->mask, data->value,
				data->host);
		}
	}

	return 0;
}

int mtu3_phy_exit_debugfs(void)
{
	debugfs_remove_recursive(usb_debugfs);

	return 0;
}
