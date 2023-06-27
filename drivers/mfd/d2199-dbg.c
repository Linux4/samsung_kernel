/*
 * d2199-dbg.c - Debug module for D2199 registers reading/interpreting
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
#include <linux/d2199/core.h>
#include <linux/d2199/d2199_reg.h>
#include <linux/d2199/d2199_debug.h>
#include "d2199-dbg.h"

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#endif

#define IGNORE_MCTL_3_n_2

struct dbg_fs_data {
	char *file_name;
	unsigned int id;
	struct reg_blk_info *r_blk;
};

static struct dentry *dbg_dir;
static u8 reg_val[MAX_D2199_REGS];

static char *mctl[] = {
	"OFF",
	"ON",
	"SLEEP",
	"TURBO"
};

static struct dbg_fs_data d2199_dbgfs[] = {
	{ "status",	STATUS_REGS,	&status_blk },
	{ "gpio",	GPIO_REGS,	&gpio_blk },
	{ "sequencer",	SEQ_REGS,	&sequencer_blk },
	{ "supplies",	LDO_REGS,	&supplies_blk },
	{ "mode_control",	MCTL_REGS,	&mode_control_blk },
	{ "control",	CTL_REGS,	&control_blk },
	{ "adc",	ADC_REGS,	&adc_blk },
	{ "rtc",	RTC_REGS,	&rtc_blk },
	{ "otp_config",	OTP_REGS,	&otp_config_blk },
	{ "audio",	AUDIO_REGS,	&audio_blk },
	{ "others",	OTHER_REGS,	&others_blk },
	{ "all",	 ALL_REGS,	&all_blk },
	{ NULL, 0, NULL }
};

static int show_reg_val(struct seq_file *sf, struct reg_blk_info *r_blk)
{
	int i;
	int start = r_blk->start_addr;
	int ret = d2199_extern_block_read(r_blk->start_addr,
						&reg_val[0], r_blk->num_regs);
	if (ret != 0) {
		pr_err("Error! Reading regsiter block %s\n", r_blk->blk_name);
		return ret;
	}

	if (sf) {
		seq_printf(sf, "=== D2199 - %s Regs ===\n", r_blk->blk_name);
		for (i = 0; i < r_blk->num_regs; i++) {
			if (d2199_reg_map[start + i].valid) {
				seq_printf(sf, "%-20s\t0x%02X\n",
					d2199_reg_map[start + i].reg_name,
					reg_val[i]);
			}
		}
	} else {
		pr_info("=== D2199 - %s Regs ===\n", r_blk->blk_name);
		for (i = 0; i < r_blk->num_regs; i++) {
			if (d2199_reg_map[start + i].valid) {
				pr_info("%-20s\t0x%02X\n",
					d2199_reg_map[start + i].reg_name,
					reg_val[i]);
			}
		}
	}
	return 0;
}

static struct reg_blk_info *get_d2199_block(unsigned int block_id)
{
	int i = 0;
	while (d2199_dbgfs[i].file_name != NULL) {
		if (d2199_dbgfs[i].id == block_id)
			return d2199_dbgfs[i].r_blk;
		i++;
	};
	return NULL;
}

int show_d2199_block_regs(unsigned int block_id)
{
	struct reg_blk_info *r_blk = get_d2199_block(block_id);
	if (r_blk)
		return show_reg_val(NULL, r_blk);

	return -EINVAL;
}
EXPORT_SYMBOL(show_d2199_block_regs);

static int d2199_mctl_info(struct seq_file *sf, struct reg_blk_info *r_blk)
{
	int i;
	int start = r_blk->start_addr;
	int ret = d2199_extern_block_read(r_blk->start_addr,
						&reg_val[0], r_blk->num_regs);
	if (ret != 0) {
		pr_err("Error! Reading regsiter block %s\n", r_blk->blk_name);
		return ret;
	}

	if (sf) {
		seq_printf(sf,
			"-----------------------------------------------------------------\n");
		seq_printf(sf,
			"%-20s\t| Value\t| MCTL3\t| MCTL2\t| MCTL1\t| MCTL0\t|\n",
			"Register");
		seq_printf(sf,
			"------------------------+-------+-------+-------+-------+-------+\n");
		for (i = 0; i < r_blk->num_regs; i++) {
			if (d2199_reg_map[start + i].valid) {
				seq_printf(sf,
					"%-20s\t| 0x%02X\t| %s\t| %s\t| %s\t| %s\t|\n",
					d2199_reg_map[start + i].reg_name,
#ifdef IGNORE_MCTL_3_n_2
					(reg_val[i] & 0x0F),
					"N/A", "N/A",
#else
					reg_val[i],
					mctl[((reg_val[i] & 0xC0) >> 6)],
					mctl[((reg_val[i] & 0x30) >> 4)],
#endif
					mctl[((reg_val[i] & 0x0C) >> 2)],
					mctl[(reg_val[i] & 0x03)]);
			}
		}
		seq_printf(sf,
			"-----------------------------------------------------------------\n");

	} else {
		pr_info("-----------------------------------------------------------------\n");
		pr_info("%-20s\t| Value\t| MCTL3\t| MCTL2\t| MCTL1\t| MCTL0\t|\n",
				"Register");
		pr_info("------------------------+-------+-------+-------+-------+-------+\n");
		for (i = 0; i < r_blk->num_regs; i++) {
			if (d2199_reg_map[start + i].valid) {
				pr_info("%-20s\t| 0x%02X\t| %s\t| %s\t| %s\t| %s\t|\n",
					d2199_reg_map[start + i].reg_name,
#ifdef IGNORE_MCTL_3_n_2
					(reg_val[i] & 0x0F),
					"N/A", "N/A",
#else
					reg_val[i],
					mctl[((reg_val[i] & 0xC0) >> 6)],
					mctl[((reg_val[i] & 0x30) >> 4)],
#endif
					mctl[((reg_val[i] & 0x0C) >> 2)],
					mctl[(reg_val[i] & 0x03)]);
			}
		}
		pr_info("-----------------------------------------------------------------\n");
	}
	return 0;
}

int show_d2199_block_info(unsigned int block_id)
{
	int ret;

	switch (block_id) {
	case MCTL_REGS:
		ret = d2199_mctl_info(NULL, &mode_control_blk);
		break;

	default:
		ret = -EINVAL;
		break;
	};
	return ret;
}
EXPORT_SYMBOL(show_d2199_block_info);

static int show_d2199_reg_info(struct seq_file *sf, void *unused)
{
	int ret = -EINVAL;
	struct dbg_fs_data *data = (struct dbg_fs_data *)sf->private;

	if (!sf)
		return -EINVAL;

	switch (data->id) {
	case MCTL_REGS:
		ret = d2199_mctl_info(sf, &mode_control_blk);
		break;

	case STATUS_REGS:
	case GPIO_REGS:
	case SEQ_REGS:
	case LDO_REGS:
	case CTL_REGS:
	case ADC_REGS:
	case RTC_REGS:
	case OTP_REGS:
	case AUDIO_REGS:
	case OTHER_REGS:
	case ALL_REGS:
		return show_reg_val(sf, data->r_blk);

	default:
		seq_printf(sf, "=== Error Unknown Input ===\n");
		break;
	};

	return ret;
}

static int d2199_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_d2199_reg_info, inode->i_private);
}

static const struct file_operations d2199_dbg_fops = {
	.open =	d2199_dbg_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init d2199_dbg_init(void)
{
	int i = 0;

#ifdef CONFIG_DEBUG_FS
	/* Creating debugfs directory for D2199 register display */
	dbg_dir = debugfs_create_dir("d2199_dbg", NULL);
	if (IS_ERR(dbg_dir))
		return PTR_ERR(dbg_dir);

	while (d2199_dbgfs[i].file_name != NULL) {
		if (IS_ERR_OR_NULL(debugfs_create_file(d2199_dbgfs[i].file_name,
					S_IRUGO, dbg_dir,
					&d2199_dbgfs[i], &d2199_dbg_fops)))
			pr_err("Failed to create file d2199_debug/%s\n",
					d2199_dbgfs[i].file_name);
		i++;
	}
#else
	pr_info("\nCONFIG_DEBUG_FS is not defined\n");
#endif
	return 0;
}

static void __exit d2199_dbg_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(dbg_dir);
#endif
}

module_init(d2199_dbg_init);
module_exit(d2199_dbg_exit);

MODULE_AUTHOR("Saurabh/Praveen");
MODULE_DESCRIPTION("D2199 registers dump driver");
MODULE_LICENSE("GPL");
