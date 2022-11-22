/*
 * 88pm822-dbg.c - Debug module for 88PM822 registers reading/interpreting
 *
 * Copyright (C) 2013, Samsung Electronics, Co., Ltd.
 *
 * Authors:
 *	Saurabh Sengar <saurabh1.s@samsung.com>
 *	Praveen BP <bp.praveen@samsung.com>
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/88pm822.h>
#include "88pm822-dbg.h"

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#endif

struct dbg_fs_data {
	char *file_name;
	unsigned int id;
	struct reg_blk_info *r_blk;
};

enum pm822_reg_id {
	BASE_REGS = 0,
	POWER_REGS,
	GPADC_REGS,
};

static struct dentry *dbg_dir;
static u8 reg_val[MAX_PM822_REGS];

static struct dbg_fs_data pm822_dbgfs[] = {
	{ "base",	BASE_REGS,	&base_blk },
	{ "power",	POWER_REGS,	&power_blk },
	{ "gpadc",	GPADC_REGS,	&gpadc_blk },
	{ NULL, 0, NULL }
};

static int pm822_extern_block_read(int const start_reg,
		u8 *dest, int num_regs)
{
	int i, page;

	if ( start_reg < PM822_BASE_REG_NUM )
		page = PM822_BASE_PAGE;
	else if ( start_reg < PM822_BASE_REG_NUM+PM822_POWER_REG_NUM )
		page = PM822_POWER_PAGE;
	else
		page = PM822_GPADC_PAGE;

	for ( i = start_reg; i < start_reg+num_regs; i++) {
		if ( pm822_reg_map[i].valid ) {
			dest[i] = pm822_extern_read(page, i-start_reg);
			if (dest[i] < 0) {
				pr_err("%s:  page: %d, reg: 0x%x - fail to read\n",
					__func__, page, i-start_reg);
				return dest[i];
			}
		}
	}

	return 0;
}

static int show_reg_val(struct seq_file *sf, struct reg_blk_info *r_blk)
{
	int i;
	int start = r_blk->start_addr;
	int ret = pm822_extern_block_read(start,
							&reg_val[0], r_blk->num_regs);

	if (ret != 0) {
		pr_err("Error! Reading regsiter block %s\n", r_blk->blk_name);
		return ret;
	}

	seq_printf(sf, "=== PM822 - %s Regs ===\n", r_blk->blk_name);
	for (i = 0; i < r_blk->num_regs; i++) {
		if (pm822_reg_map[start + i].valid) {
			seq_printf(sf, "%-20s\t0x%02X\t0x%02X\n",
				pm822_reg_map[start + i].reg_name,
				i, reg_val[start + i]);
		}
	}
	return 0;
}

static struct reg_blk_info *get_pm822_block(unsigned int block_id)
{
	int i = 0;
	while (pm822_dbgfs[i].file_name != NULL) {
		if (pm822_dbgfs[i].id == block_id)
			return pm822_dbgfs[i].r_blk;
		i++;
	};
	return NULL;
}

static int show_pm822_reg_info(struct seq_file *sf, void *unused)
{
	int ret = -EINVAL;
	struct dbg_fs_data *data = (struct dbg_fs_data *)sf->private;

	if (!sf)
		return -EINVAL;

	switch (data->id) {
	case BASE_REGS:
	case POWER_REGS:
	case GPADC_REGS:
		return show_reg_val(sf, data->r_blk);

	default:
		seq_printf(sf, "=== Error Unknown Input ===\n");
		break;
	};

	return ret;
}

static int pm822_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_pm822_reg_info, inode->i_private);
}

static const struct file_operations pm822_dbg_fops = {
	.open =	pm822_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init pm822_dbg_init(void)
{
#ifdef CONFIG_DEBUG_FS
	int i = 0;

	/* Creating debugfs directory for pm822 register display */
	dbg_dir = debugfs_create_dir("pm822_dbg", NULL);
	if (IS_ERR(dbg_dir))
		return PTR_ERR(dbg_dir);

	while (pm822_dbgfs[i].file_name != NULL) {
		if (IS_ERR_OR_NULL(debugfs_create_file(pm822_dbgfs[i].file_name,
					S_IRUGO, dbg_dir,
					&pm822_dbgfs[i], &pm822_dbg_fops)))
			pr_err("Failed to create file pm822_debug/%s\n",
					pm822_dbgfs[i].file_name);
		i++;
	}
#else
	pr_info("\nCONFIG_DEBUG_FS is not defined\n");
#endif
	return 0;
}

static void __exit pm822_dbg_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(dbg_dir);
#endif
}

module_init(pm822_dbg_init);
module_exit(pm822_dbg_exit);

MODULE_AUTHOR("Saurabh/Praveen");
MODULE_DESCRIPTION("88pm822 registers dump driver");
MODULE_LICENSE("GPL");
