/* drivers/video/backlight/gen_panel/gen-panel-tuning.c
 *
 * Copyright (C) 2014 Samsung Electronics Co, Ltd.
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
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/platform_data/gen-panel.h>

#define MAX_TFILE_NAME_LEN (100)
#define MAX_BUF_SIZE (1024)
static struct gen_cmds_info *tcmds;
static char tfile_name[MAX_TFILE_NAME_LEN];
static char *tfile_paths[] = {"/sdcard/", "/data/"};

#define isxdigit(c)	(('0' <= (c) && (c) <= '9') \
			 || ('a' <= (c) && (c) <= 'f') \
			 || ('A' <= (c) && (c) <= 'F'))
#define isspace(c)	((c == ' ') || (c == '\t') || (c == '\n') \
			|| (c == '\v') || (c == '\f') || (c == '\r'))
#define isnewline(c)	((c == '\n') || (c == '\r'))
#define iscomment(c)	(c == '/')
/* start of property */
#define issop(c)	(c == '[')
/* end of property*/
#define iseop(c)	((c == ']') || (c == ';') || (c == '\0'))
#define for_each_names(idx, names)	\
	for (idx = 0; idx < ARRAY_SIZE(names); idx++)

static int parse_u8_array_string(unsigned char *d, const char *s)
{
	int i = 0;
	bool sop = 0;

	while (!iseop(*s)) {
		if (iscomment(*s)) {
			if (s[1] == '*') {
				s += 2;
				while (!(s[0] == '*' && s[1] == '/'))
					s++;
				s += 2;
				pr_info("%s: find end of comment\n", __func__);
				continue;
			} else if (s[1] == '/') {
				s += 2;
				while (!isnewline(*s++));
				pr_info("%s: find new line\n", __func__);
				continue;
			} else {
				/* syntax error */
				pr_info("%s: syntax error\n", __func__);
				return -EINVAL;
			}
		}

		if (sop) {
			if (isspace(*s)) {
				s++;
				continue;
			}

			if (!isxdigit(s[0]) || !isxdigit(s[1])) {
				/* syntax error */
				pr_err("%s: syntax error\n", __func__);
				return -EINVAL;
			}

			sscanf(s, "%2hhx", &d[i]);
			s += 2; i++;
			if (i >= MAX_BUF_SIZE) {
				pr_err("%s: exceeded dst buf size(%d)!!\n",
						__func__, MAX_BUF_SIZE);
				return -EINVAL;
			}
			continue;
		}

		if (issop(*s))
			sop = true;
		s++;
	}

	return i;
}

static int parse_property(unsigned char *d,
		const char *s, const char *name)
{
	int nr_data = 0;

	s = strstr(s, name);
	if (s)
		nr_data = parse_u8_array_string(d, s);

	return nr_data;
}

static int parse_dcs_cmds(const char *src,
		const char *propname, struct gen_cmds_info *cmd)
{
	unsigned char *data;
	struct gen_cmd_hdr *dchdr;
	struct gen_cmd_desc *desc;
	int sz = 0, i = 0, nr_desc = 0, nr;

	data = kmalloc(MAX_BUF_SIZE * sizeof(unsigned char), GFP_KERNEL);
	if (unlikely(!data)) {
		pr_err("%s: failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	sz = parse_property(data, src, propname);
	if (!sz || sz < 0) {
		pr_err("%s: failed to parse (%s)\n",
				__func__, propname);
		kfree(data);
		return 0;
	}

	/* scan dcs commands */
	while (sz > i + sizeof(*dchdr)) {
		dchdr = (struct gen_cmd_hdr *)(data + i);
		dchdr->dlen = ntohs(dchdr->dlen);
		dchdr->wait = ntohs(dchdr->wait);
		if (i + dchdr->dlen > sz) {
			pr_err("%s: parse error dtsi cmd=0x%02x, len=0x%02x, nr_desc=%d\n",
				__func__, dchdr->dtype, dchdr->dlen, nr_desc);
			dchdr = NULL;
			kfree(data);
			return -ENOMEM;
		}
		i += sizeof(*dchdr) + dchdr->dlen;
		nr_desc++;
	}

	if (unlikely(sz != i)) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, data[0], sz);
		kfree(data);
		return -ENOMEM;
	}

	cmd->desc = kzalloc(nr_desc * sizeof(struct gen_cmd_desc),
						GFP_KERNEL);
	cmd->name = kzalloc(sizeof(char) * (strlen(propname) + 1),
			GFP_KERNEL);
	strncpy(cmd->name, propname, strlen(propname) + 1);
	cmd->nr_desc = nr_desc;
	if (unlikely(!cmd->desc)) {
		pr_err("%s: fail to allocate cmd\n", __func__);
		goto err_alloc_cmds;
	}

	desc = cmd->desc;
	for (i = 0, nr = 0; nr < nr_desc; nr++) {
		dchdr = (struct gen_cmd_hdr *)(data + i);
		desc[nr].data_type = dchdr->dtype;
		desc[nr].lp = dchdr->txmode;
		desc[nr].delay = dchdr->wait;
		desc[nr].length = dchdr->dlen;
		desc[nr].data = kzalloc(dchdr->dlen * sizeof(unsigned char),
				GFP_KERNEL);
		if (!desc[nr].data) {
			pr_err("%s: fail to allocate data\n", __func__);
			goto err_alloc_data;
		}
		memcpy(desc[nr].data, &data[i + sizeof(*dchdr)],
				dchdr->dlen * sizeof(unsigned char));
		i += sizeof(*dchdr) + dchdr->dlen;
		pr_info("type:%x, %s, %d ms, %d bytes, %02Xh\n",
				desc[nr].data_type, "hs-mode",
				desc[nr].delay, desc[nr].length,
				(desc[nr].data)[0]);
	}
	kfree(data);
	pr_info("parse %s done!\n", propname);
	return 0;
err_alloc_data:
	for (nr = 0; nr < nr_desc; nr++) {
		kfree(desc[nr].data);
		desc[nr].data = NULL;
	}
err_alloc_cmds:
	kfree(cmd->name);
	cmd->name = NULL;
	kfree(cmd->desc);
	cmd->desc = NULL;
	kfree(data);

	return -EINVAL;
}


static int parse_text(struct lcd *lcd, const char *src, int len)
{
	int i;

	tcmds = kzalloc(PANEL_OP_CMD_MAX *
			sizeof(struct gen_cmds_info), GFP_KERNEL);

	for (i = 0; i < ARRAY_SIZE(op_cmd_names); i++)
		parse_dcs_cmds(src, op_cmd_names[i], &tcmds[i]);

	return 1;
}

static int unload_tuning_data(struct lcd *lcd)
{
	int index, i;
	unsigned int nr_desc;
	struct gen_cmd_desc *desc;

	if (!tcmds)
		return 0;

	for_each_names(index, op_cmd_names) {
		pr_info("%s: unload %s, %p, %d ea\n", __func__,
				op_cmd_names[index],
				tcmds[index].desc,
				tcmds[index].nr_desc);
		desc = tcmds[index].desc;
		nr_desc = tcmds[index].nr_desc;
		for (i = 0; desc && i < nr_desc; i++) {
			kfree(desc[i].data);
			desc[i].data = NULL;
		}
		kfree(tcmds[index].desc);
		tcmds[index].desc = NULL;
	}
	kfree(tcmds);
	tcmds = NULL;

	return 0;
}

static int load_tuning_data(struct lcd *lcd, char *filename)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret, num;
	mm_segment_t fs;

	pr_info("%s: loading (%s)\n", __func__, filename);
	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		pr_err("%s: failed to open (%ld)\n", __func__, IS_ERR(filp));
		return -1;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	pr_info("%s: loading (%ld) bytes\n", __func__, l);

	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		pr_err("%s: not enough memory\n", __func__);
		filp_close(filp, current->files);
		return -1;
	}
	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	if (ret != l) {
		pr_err("%s: fail to read (%d)\n", __func__, ret);
		kfree(dp);
		filp_close(filp, current->files);
		return -1;
	}
	filp_close(filp, current->files);

	set_fs(fs);
	unload_tuning_data(lcd);
	num = parse_text(lcd, dp, l);
	if (!num) {
		pr_err("%s: parse error\n", __func__);
		kfree(dp);
		return -1;
	}
	pr_info("%s: loading count (%d)\n", __func__, num);
	kfree(dp);
	return num;
}

static ssize_t tuning_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "filename : %s\n", tfile_name);
}

static ssize_t tuning_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd *lcd = dev_get_drvdata(dev);
	char *fname, *name;
	unsigned int len, i;
	int num = 0, tuning;
	static struct gen_cmds_info *op_cmds;

	if (buf[0] == '0' || buf[0] == '1') {
		tuning = buf[0] - '0';
		pr_info("%s: tuning %s\n", __func__,
				tuning ? "enable" : "disable");
		if (tuning) {
			if (tcmds) {
				pr_info("%s: use tuning desc\n", __func__);
				if (!op_cmds)
					op_cmds = lcd->op_cmds;
				lcd->op_cmds = tcmds;
			}
		} else {
			if (op_cmds) {
				pr_info("%s: restore\n", __func__);
				lcd->op_cmds = op_cmds;
			}
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(tfile_paths); i++) {
			len = strlen(tfile_paths[i]) + strlen(buf) + 1;
			fname = kzalloc(sizeof(char) * len, GFP_KERNEL);
			sprintf(fname, "%s%s", tfile_paths[i], buf);
			name = fname;
			while (*name) {
				if (isspace(*name)) {
					*name = '\0';
					break;
				}
				name++;
			}
			pr_info("%s: Loading %s\n", __func__, fname);
			num = load_tuning_data(lcd, fname);
			if (num > 0) {
				strncpy(tfile_name, fname, len);
				kfree(fname);
				break;
			}
			kfree(fname);
		}

		if (num <= 0) {
			pr_err("%s: failed to load (%s)\n", __func__, buf);
			return size;
		}
	}

	return size;
}

static DEVICE_ATTR(tuning, 0664, tuning_show, tuning_store);

int gen_panel_attach_tuning(struct lcd *lcd)
{
	int ret;

	if (IS_ERR(lcd->class)) {
		ret = PTR_ERR(lcd->class);
		pr_err("no class(lcd)");
		return ret;
	}

	if (IS_ERR(lcd->dev)) {
		ret = PTR_ERR(lcd->dev);
		pr_err("no device(panel)!\n");
		return ret;
	}

	ret = device_create_file(lcd->dev, &dev_attr_tuning);
	if (unlikely(ret < 0)) {
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_tuning.attr.name);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(gen_panel_attach_tuning);

void gen_panel_detach_tuning(struct lcd *lcd)
{
	unload_tuning_data(lcd);
	device_remove_file(lcd->dev, &dev_attr_tuning);
	return;
}
EXPORT_SYMBOL(gen_panel_detach_tuning);

MODULE_DESCRIPTION("GENERIC PANEL TUNING Driver");
MODULE_LICENSE("GPL");
