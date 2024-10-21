/* SPDX-License-Identifier: GPL-2.0 */
/*
 * cpufreq-dbg-lite.c - eem debug driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Tungchen Shih <tungchen.shih@mediatek.com>
 */

#define csram_read(offs)	__raw_readl(csram_base + (offs))
#define csram_write(offs, val)	__raw_writel(val, csram_base + (offs))

#ifndef PROC_FOPS_RW
#define PROC_FOPS_RW(name)\
static int name ## _proc_open(struct inode *inode, struct file *file)\
{\
	return single_open(file, name ## _proc_show, pde_data(inode));\
} \
static const struct proc_ops name ## _proc_fops = {\
	.proc_open           = name ## _proc_open,\
	.proc_read           = seq_read,\
	.proc_lseek          = seq_lseek,\
	.proc_release        = single_release,\
	.proc_write          = name ## _proc_write,\
}
#endif

#ifndef PROC_FOPS_RO
#define PROC_FOPS_RO(name)\
static int name##_proc_open(struct inode *inode, struct file *file)\
{\
	return single_open(file, name##_proc_show, pde_data(inode));\
} \
static const struct proc_ops name##_proc_fops = {\
	.proc_open = name##_proc_open,\
	.proc_read = seq_read,\
	.proc_lseek = seq_lseek,\
	.proc_release = single_release,\
}
#endif

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}
#define PROC_ENTRY_DATA(name)	\
{__stringify(name), &name ## _proc_fops, g_ ## name}

#define OFFS_CCI_TBL_MODE 0x0F9C

extern int mtk_eem_init(struct platform_device *pdev);
extern int mtk_devinfo_init(struct platform_device *pdev);
extern int set_dsu_ctrl_debug(unsigned int eas_ctrl, bool debug_enable);
extern int cpufreq_set_cci_mode(unsigned int mode);
unsigned int cpufreq_get_cci_mode(void);
void set_cci_mode(unsigned int mode);
