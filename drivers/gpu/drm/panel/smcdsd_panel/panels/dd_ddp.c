// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* temporary solution: Do not use these sysfs as official purpose */
/* these function are not official one. only purpose is for temporary test */

#if defined(CONFIG_DEBUG_FS) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP) && defined(CONFIG_SMCDSD_LCD_DEBUG)
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/lcd.h>
#include <linux/platform_device.h>
#include <linux/module.h>

#include <video/mipi_display.h>

#include "../smcdsd_board.h"
#include "../smcdsd_panel.h"
#if defined(CONFIG_MTK_FB)
#include "smcdsd_notify.h"
#else
#include "../smcdsd_notify.h"
#endif

#include "../../../mediatek/mtk_debug.h"

#include "dd.h"

static bool log_boot;
#define dbg_info(fmt, ...)	pr_info(pr_fmt("%s: %3d: %s: " fmt), "smcdsd", __LINE__, __func__, ##__VA_ARGS__)
#define dbg_warn(fmt, ...)	pr_warn(pr_fmt("%s: %3d: %s: " fmt), "smcdsd", __LINE__, __func__, ##__VA_ARGS__)
#define dbg_boot(fmt, ...)	do { if (unlikely(log_boot)) dbg_info(fmt, ##__VA_ARGS__); } while (0)

#define PANEL_DTS_NAME	"lcd_info"

#define DD_DDP_LIST	\
__XX(DATA_RATE,		"lcm_params-dsi-data_rate",	"data_rate",		0600)	\
__XX(LCD_INFO_END,	"",			"",			0)	\

#define __XX(id, dts, sysfs, mode) D_##id,
enum { DD_DDP_LIST DD_DDP_LIST_MAX };
#undef __XX

#define __XX(id, dts, sysfs, mode) (#id),
static char *DD_DDP_LIST_NAME[] = { DD_DDP_LIST };
#undef __XX

static struct {
	char *dts_name;
	char *sysfs_name;
	umode_t mode;
	u32	length;
} debugfs_list[] = {
#define __XX(id, dts, sysfs, mode) {dts, sysfs, mode, 0},
	DD_DDP_LIST
#undef __XX
};

struct d_info {
	struct dentry	*debugfs_root;

	struct notifier_block	fb_notifier;
	unsigned int enable;

	u32 regdump;

	u32 default_param[DD_DDP_LIST_MAX];	/* get from dts */
	u32 request_param[DD_DDP_LIST_MAX];	/* get from sysfs input */
	u32 pending_param[DD_DDP_LIST_MAX];	/* get from sysfs input flag */
	u32 current_param[DD_DDP_LIST_MAX];	/* get from real data */

	u32 *point[DD_DDP_LIST_MAX];
	u32 *sub_point[DD_DDP_LIST_MAX];
};

static void configure_lcd_info(u32 **point, struct mipi_dsi_lcd_config *config)
{
	point[D_DATA_RATE]		=	&config->ext.data_rate;
}

static void configure_param(struct d_info *d)
{
	struct mipi_dsi_lcd_common *plcd = get_lcd_common(0);

	configure_lcd_info(d->point, &plcd->config[LCD_CONFIG_DFT]);

	if (plcd->config[LCD_CONFIG_DFT].vrr_count > 0)
		configure_lcd_info(d->sub_point, &plcd->config[LCD_CONFIG_1]);
}

static inline void update_value(u32 *dest, u32 *src, u32 update)
{
	if (IS_ERR_OR_NULL(dest) || IS_ERR_OR_NULL(src) || !update)
		return;

	if (*dest == *src || !update)
		return;

	*dest = *src;

	dbg_boot("dest: %4u, src: %4u, update: %u\n", *dest, *src, update);
}

static void update_point(u32 **dest, u32 *src, u32 *update)
{
	unsigned int i, j;
	return ;
	for (i = 0; i < DD_DDP_LIST_MAX; i++) {
		for (j = 0; j < debugfs_list[i].length; j++)
			update_value(dest[i + j], &src[i + j], update ? update[i + j] : 1);
	}
}

static void update_clear(u32 *update)
{
	unsigned int i, j;
	return ;
	for (i = 0; i < DD_DDP_LIST_MAX; i++) {
		for (j = 0; j < debugfs_list[i].length; j++)
			if (update)
				update[i + j] = 0;
	}
}

static void update_param(u32 *dest, u32 **src, u32 *update)
{
	unsigned int i, j;
	return ;
	for (i = 0; i < DD_DDP_LIST_MAX; i++) {
		for (j = 0; j < debugfs_list[i].length; j++)
			update_value(&dest[i + j], src[i + j], update ? update[i + j] : 1);
	}
}

static int fb_notifier_callback(struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	struct d_info *d = NULL;
	int fb_blank;

	switch (event) {
	case FB_EVENT_BLANK:
	case SMCDSD_EARLY_EVENT_BLANK:
		break;
	default:
		return NOTIFY_DONE;
	}

	d = container_of(self, struct d_info, fb_notifier);

	fb_blank = *(int *)evdata->data;

	if (evdata->info->node)
		return NOTIFY_DONE;

	log_boot = true;

	if (event == SMCDSD_EARLY_EVENT_BLANK)
		d->enable = 0;
	else if (event == FB_EVENT_BLANK && fb_blank == FB_BLANK_UNBLANK)
		d->enable = 1;

	if (fb_blank == FB_BLANK_UNBLANK && event == FB_EARLY_EVENT_BLANK) {
		update_point(d->point, d->request_param, d->pending_param);
		update_point(d->sub_point, d->request_param, d->pending_param);
		update_clear(d->pending_param);
	}

	if (fb_blank == FB_BLANK_UNBLANK && event == FB_EVENT_BLANK)
		update_param(d->current_param, d->point, NULL);

	return NOTIFY_DONE;
}

/* copy from debugfs/file.c */
struct array_data {
	void *array;
	void *pending;
	u32 elements;
};

static ssize_t u32_array_write(struct file *f, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct array_data *data = ((struct seq_file *)f->private_data)->private;
	u32 *array = data->array;
	u32 *pending = data->pending;
	int array_size = data->elements;

	unsigned char ibuf[MAX_INPUT] = {0, };
	unsigned int tbuf[MAX_INPUT] = {0, };
	unsigned int value, i;
	char *pbuf, *token = NULL;
	int ret = 0, end = 0;

	ret = dd_simple_write_to_buffer(ibuf, sizeof(ibuf), ppos, user_buf, count);
	if (ret < 0) {
		dbg_info("dd_simple_write_to_buffer fail: %d\n", ret);
		goto exit;
	}

	pbuf = ibuf;
	while (--array_size >= 0 && (token = strsep(&pbuf, " "))) {
		dbg_info("%d, %s\n", array_size, token);
		if (*token == '\0')
			continue;
		ret = kstrtou32(token, 0, &value);
		if (ret < 0) {
			dbg_info("kstrtou32 fail: ret: %d\n", ret);
			break;
		}

		if (end == ARRAY_SIZE(tbuf)) {
			dbg_info("invalid input: end: %d, tbuf: %zu\n", end, ARRAY_SIZE(tbuf));
			break;
		}

		tbuf[end] = value;
		end++;
	}

	if (ret < 0 || !end || data->elements < end || end == ARRAY_SIZE(tbuf)) {
		dbg_info("invalid input: end: %d, %s\n", end, user_buf);
		goto exit;
	}

	for (i = 0; i < end; i++) {
		array[i] = tbuf[i];
		pending[i] = 1;
	}

exit:
	return count;
}

static size_t u32_format_array(char *buf, size_t bufsize,
			       u32 *array, int array_size)
{
	size_t ret = 0;

	while (--array_size >= 0) {
		size_t len;
		char term = array_size ? ' ' : '\n';

		len = snprintf(buf, bufsize, "%u%c", *array++, term);
		ret += len;

		buf += len;
		bufsize -= len;
	}
	return ret;
}

static int u32_array_show(struct seq_file *m, void *unused)
{
	struct array_data *data = m->private;
	int size, elements = data->elements;
	char *buf;
	int ret;

	/*
	 * Max size:
	 *  - 10 digits + ' '/'\n' = 11 bytes per number
	 *  - terminating NUL character
	 */
	size = elements*11;
	buf = kzalloc(size+1, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;
	buf[size] = 0;

	ret = u32_format_array(buf, size, data->array, data->elements);

	seq_printf(m, "%s", buf);

	kfree(buf);

	return 0;
}

static int u32_array_open(struct inode *inode, struct file *f)
{
	return single_open(f, u32_array_show, inode->i_private);
}

static const struct file_operations u32_array_fops = {
	.open		= u32_array_open,
	.write		= u32_array_write,
	.read		= seq_read,
	.llseek		= no_llseek,
	.release	= single_release,
};

static struct dentry *debugfs_create_array(const char *name, umode_t mode,
					struct dentry *parent,
					u32 *request, u32 *pending, u32 elements)
{
	struct array_data *data = kzalloc(sizeof(*data), GFP_KERNEL);

	if (data == NULL)
		return NULL;

	data->array = request;
	data->pending = pending;

	data->elements = elements;

	return debugfs_create_file(name, mode, parent, data, &u32_array_fops);
}

static int init_debugfs_lcd_info(struct d_info *d)
{
	struct device_node *np = NULL;
	struct device_node *nplcd = NULL;
	struct device_node *npetc = NULL;
	int ret = 0, count;
	unsigned int i = 0;
	struct dentry *debugfs = NULL;

	nplcd = of_find_recommend_lcd_info(NULL);
	if (!nplcd) {
		dbg_warn("of_find_recommend_lcd_info fail\n");
		return -EINVAL;
	}

	configure_param(d);

	for (i = 0; i < DD_DDP_LIST_MAX; i++) {
		if (!strlen(debugfs_list[i].dts_name))
			continue;

		if (i < D_LCD_INFO_END)
			count = of_property_count_u32_elems(nplcd, debugfs_list[i].dts_name);
		else {
			npetc = of_find_node_with_property(NULL, debugfs_list[i].dts_name);
			count = npetc ? of_property_count_u32_elems(npetc, debugfs_list[i].dts_name) : 0;
		}

		if (count <= 0)
			continue;

		np = (i < D_LCD_INFO_END) ? nplcd : npetc;

		debugfs_list[i].length = count;

		debugfs = debugfs_create_array(debugfs_list[i].sysfs_name, debugfs_list[i].mode, d->debugfs_root,
			&d->request_param[i], &d->pending_param[i], count);

		if (debugfs)
			dbg_info("%s is created and length is %d\n", debugfs_list[i].sysfs_name, debugfs_list[i].length);

	}

	update_param(d->default_param, d->point, NULL);
	update_param(d->request_param, d->point, NULL);
	update_param(d->current_param, d->point, NULL);

	return ret;
}

static int status_show(struct seq_file *m, void *unused)
{
	struct d_info *d = m->private;
	unsigned int i, j;

	seq_puts(m, "---------------------------------------------------------------------------------\n");
	seq_puts(m, "          |   DEFAULT|   REQUEST|   CURRENT|   DEFAULT|   REQUEST|   CURRENT|  RW\n");
	seq_puts(m, "---------------------------------------------------------------------------------\n");
	for (i = 0; i < DD_DDP_LIST_MAX; i++) {
		for (j = 0; j < debugfs_list[i].length; j++) {
			if (!DD_DDP_LIST_NAME[i + j])
				continue;

			if (!strncmp(DD_DDP_LIST_NAME[i + j], "HIDDEN", strlen("HIDDEN")))
				continue;

			seq_printf(m, "%10s|", DD_DDP_LIST_NAME[i + j]);

			seq_printf(m, "%10u|", d->default_param[i + j]);
			(d->pending_param[i + j]) ? seq_printf(m, "%10u|", d->request_param[i + j]) : seq_printf(m, "%10s|", "");
			seq_printf(m, "%10u|", d->current_param[i + j]);

			seq_printf(m, "%#10x|", d->default_param[i + j]);
			(d->pending_param[i + j]) ? seq_printf(m, "%#10x|", d->request_param[i + j]) : seq_printf(m, "%10s|", "");
			seq_printf(m, "%#10x|", d->current_param[i + j]);

			seq_printf(m, "%4s\n", (debugfs_list[i].mode & 0222) ? "RW" : "R");
		}
	}
	seq_puts(m, "\n");

	return 0;
}

static int status_open(struct inode *inode, struct file *f)
{
	return single_open(f, status_show, inode->i_private);
}

static ssize_t status_write(struct file *f, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct d_info *d = ((struct seq_file *)f->private_data)->private;
	unsigned char ibuf[MAX_INPUT] = {0, };
	int ret = 0;

	ret = dd_simple_write_to_buffer(ibuf, sizeof(ibuf), ppos, user_buf, count);
	if (ret < 0) {
		dbg_info("dd_simple_write_to_buffer fail: %d\n", ret);
		goto exit;
	}

	if (!strncmp(ibuf, "0", count - 1)) {
		dbg_info("input is 0(zero). reset request parameter to default\n");

		memcpy(d->request_param, d->default_param, sizeof(d->request_param));
		memset(d->pending_param, 1, sizeof(d->pending_param));
	}

exit:
	return count;
}

static const struct file_operations status_fops = {
	.open		= status_open,
	.write		= status_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int regdump_show(struct seq_file *m, void *unused)
{
	struct d_info *d = m->private;
	u32 reg = 0, val = 0;
	void __iomem *ioregs = NULL;

	if (!d->enable) {
		dbg_info("enable is %s\n", d->enable ? "on" : "off");
		seq_printf(m, "enable is %s\n", d->enable ? "on" : "off");
		goto exit;
	}

	reg = d->regdump;
	if (!reg) {
		dbg_info("input reg is invalid, %8x\n", reg);
		goto exit;
	}

	ioregs = ioremap(d->regdump, SZ_4);
	if (IS_ERR_OR_NULL(ioregs)) {
		dbg_info("ioremap fail for %8x\n", reg);
		goto exit;
	}

	val = readl(ioregs);

	dbg_info("reg: %8x, val: %8x\n", reg, val);

	seq_printf(m, "reg: %8x, val: %8x\n", reg, val);

	iounmap(ioregs);

exit:
	return 0;
}

static int regdump_open(struct inode *inode, struct file *f)
{
	return single_open(f, regdump_show, inode->i_private);
}

static ssize_t regdump_write(struct file *f, const char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct d_info *d = ((struct seq_file *)f->private_data)->private;
	int ret = 0;
	u32 reg = 0, val = 0;
	void __iomem *ioregs = NULL;
	unsigned char ibuf[MAX_INPUT] = {0, };

	if (!d->enable) {
		dbg_info("enable is %s\n", d->enable ? "on" : "off");
		goto exit;
	}

	ret = dd_simple_write_to_buffer(ibuf, sizeof(ibuf), ppos, user_buf, count);
	if (ret < 0) {
		dbg_info("dd_simple_write_to_buffer fail: %d\n", ret);
		goto exit;
	}

	ret = sscanf(ibuf, "%x %x", &reg, &val);
	if (clamp(ret, 1, 2) != ret) {
		dbg_info("input is invalid, %d\n", ret);
		goto exit;
	}

	if (reg > UINT_MAX) {
		dbg_info("input is invalid, reg: %02x\n", reg);
		goto exit;
	}

	if (val > UINT_MAX) {
		dbg_info("input is invalid, val: %02x\n", val);
		goto exit;
	}

	if (!reg) {
		dbg_info("input reg is invalid, %8x\n", reg);
		goto exit;
	}

	d->regdump = reg;

	ioregs = ioremap(d->regdump, SZ_4);
	if (IS_ERR_OR_NULL(ioregs)) {
		dbg_info("ioremap fail for %8x\n", reg);
		goto exit;
	}

	if (ret == 2)
		writel(val, ioregs);
	else if (ret == 1)
		val = readl(ioregs);

	dbg_info("reg: %8x, val: %8x%s\n", reg, val, (ret == 2) ? ", write_mode" : "");

	iounmap(ioregs);
exit:
	return count;
}

static const struct file_operations regdump_fops = {
	.open		= regdump_open,
	.write		= regdump_write,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int help_show(struct seq_file *m, void *unused)
{
	struct d_info *d = m->private;
	unsigned int i, j;

	seq_puts(m, "\n");
	seq_puts(m, "------------------------------------------------------------\n");
	seq_puts(m, "* ATTENTION\n");
	seq_puts(m, "* These sysfs can NOT be mandatory official purpose\n");
	seq_puts(m, "* These sysfs has risky, harmful, unstable function\n");
	seq_puts(m, "* So we can not support these sysfs for official use\n");
	seq_puts(m, "* DO NOT request improvement related with these function\n");
	seq_puts(m, "* DO NOT request these function as the mandatory requirements\n");
	seq_puts(m, "* If you insist, we eliminate these function immediately\n");
	seq_puts(m, "------------------------------------------------------------\n");
	seq_puts(m, "\n");
	seq_puts(m, "----------\n");
	seq_puts(m, "# cd /d/dd_ddp\n");
	seq_puts(m, "\n");
	seq_puts(m, "---------- usage\n");
	seq_puts(m, "1. you can request to change paremter like below\n");
	for (i = 0; i < DD_DDP_LIST_MAX; i++) {
		if (!debugfs_list[i].length || !(debugfs_list[i].mode & 0222))
			continue;

		seq_puts(m, "# echo ");
		for (j = 0; j < debugfs_list[i].length; j++) {
			if (!DD_DDP_LIST_NAME[i + j])
				continue;

			if (!strncmp(DD_DDP_LIST_NAME[i + j], "HIDDEN", strlen("HIDDEN")))
				continue;

			seq_printf(m, "%s ", DD_DDP_LIST_NAME[i + j]);
		}
		seq_printf(m, "> %s\n", debugfs_list[i].sysfs_name);
	}

	for (i = 0; i < DD_DDP_LIST_MAX; i++) {
		if (!debugfs_list[i].length || !(debugfs_list[i].mode & 0222))
			continue;

		seq_puts(m, "ex) # echo ");
		for (j = 0; j < debugfs_list[i].length; j++) {
			if (!DD_DDP_LIST_NAME[i + j])
				continue;

			if (!strncmp(DD_DDP_LIST_NAME[i + j], "HIDDEN", strlen("HIDDEN")))
				continue;

			seq_printf(m, "%d ", d->default_param[i + j]);
		}
		seq_printf(m, "> %s\n", debugfs_list[i].sysfs_name);
	}

	seq_puts(m, "\n");
	seq_puts(m, "2. and you must do lcd off -> on to apply your request\n");
	seq_puts(m, "3. it is IMPOSSIBLE to apply parameter immediately while lcd on runtime\n");
	seq_puts(m, "\n");
	seq_puts(m, "---------- status usage\n");
	seq_puts(m, "1. you can check current configuration status like below\n");
	seq_puts(m, "# cat status\n");

	status_show(m, NULL);

	seq_puts(m, "= R: Read Only. you can not modify this value\n");
	seq_puts(m, "= DEFAULT: default booting parameter\n");
	seq_puts(m, "= REQUEST: request parameter (not applied yet)\n");
	seq_puts(m, "= CURRENT: current applied parameter\n");
	seq_puts(m, "------------------------------------------------------------\n");
	seq_puts(m, "\n");


	return 0;
}

static int help_open(struct inode *inode, struct file *f)
{
	return single_open(f, help_show, inode->i_private);
}

static const struct file_operations help_fops = {
	.open		= help_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int init_debugfs_ddp(void)
{
	int ret = 0;
	static struct dentry *debugfs_root;
	struct d_info *d = NULL;

	dbg_info("+\n");

	d = kzalloc(sizeof(struct d_info), GFP_KERNEL);

	if (!debugfs_root)
		debugfs_root = debugfs_create_dir("dd_ddp", NULL);

	d->debugfs_root = debugfs_root;

	debugfs_create_file("_help", 0400, debugfs_root, d, &help_fops);
	debugfs_create_file("status", 0400, debugfs_root, d, &status_fops);
	debugfs_create_file("regdump", 0600, debugfs_root, d, &regdump_fops);

	init_debugfs_lcd_info(d);

	d->fb_notifier.notifier_call = fb_notifier_callback;
	ret = smcdsd_register_notifier(&d->fb_notifier);
	d->enable = 1;

	dbg_info("-\n");

	return ret;
}

static int __init dd_ddp_init(void)
{
	init_debugfs_ddp();

	return 0;
}
late_initcall_sync(dd_ddp_init);
#endif

