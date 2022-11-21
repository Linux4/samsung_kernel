/*
 * Exynos CPU Performance Profiling
 * Author: Jungwook Kim, jwook1.kim@samsung.com
 * Created: 2014
 * Updated: 2016-2020
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include "exynos_perf_cpu.h"

#define DEF_REG_STRUCT(n) \
	struct register_type reg_##n = {	\
	.name = ""#n,	\
	.read_reg = mrs_##n##_read	\
};

/* armv8 */
DEF_REG_STRUCT(SCTLR)
DEF_REG_STRUCT(MAIR)
DEF_REG_STRUCT(CPUACTLR)
DEF_REG_STRUCT(CPUECTLR)
DEF_REG_STRUCT(L2CTLR)
DEF_REG_STRUCT(L2ACTLR)
DEF_REG_STRUCT(L2ECTLR)
DEF_REG_STRUCT(MPIDR)
DEF_REG_STRUCT(MIDR)
DEF_REG_STRUCT(REVIDR)

/* armv8.2 */
DEF_REG_STRUCT(CPUACTLR_v82)
DEF_REG_STRUCT(CPUACTLR2_v82)
DEF_REG_STRUCT(CPUACTLR3_v82)
DEF_REG_STRUCT(CPUECTLR_v82)

/* ananke */
DEF_REG_STRUCT(CPUPWRCTLR)
DEF_REG_STRUCT(CLUSTERPWRCTLR)
DEF_REG_STRUCT(CLUSTERECTLR)

#define SET_CORE_REG(r, v) { .reg = &reg_##r, .val = v }

static struct core_register arm_v8_regs[] = {
	SET_CORE_REG(SCTLR, 0),
	SET_CORE_REG(MAIR, 0),
	SET_CORE_REG(MPIDR, 0),
	SET_CORE_REG(MIDR, 0),
	SET_CORE_REG(REVIDR, 0),
	SET_CORE_REG(CPUACTLR, 1),
	SET_CORE_REG(CPUECTLR, 1),
	SET_CORE_REG(L2CTLR, 1),
	SET_CORE_REG(L2ACTLR, 1),
	SET_CORE_REG(L2ECTLR, 1),
};

static struct core_register arm_a73_regs[] = {
	SET_CORE_REG(SCTLR, 0),
	SET_CORE_REG(MAIR, 0),
	SET_CORE_REG(MPIDR, 0),
	SET_CORE_REG(MIDR, 0),
	SET_CORE_REG(REVIDR, 0),
	SET_CORE_REG(CPUECTLR, 1),
	SET_CORE_REG(L2CTLR, 1),
	SET_CORE_REG(L2ECTLR, 1),
};

static struct core_register arm_a55_regs[] = {
	SET_CORE_REG(SCTLR, 0),
	SET_CORE_REG(MAIR, 0),
	SET_CORE_REG(MPIDR, 0),
	SET_CORE_REG(MIDR, 0),
	SET_CORE_REG(REVIDR, 0),
	SET_CORE_REG(CPUACTLR_v82, 1),
	SET_CORE_REG(CPUECTLR_v82, 1),
	SET_CORE_REG(CPUPWRCTLR, 1),
	SET_CORE_REG(CLUSTERPWRCTLR, 1),
	SET_CORE_REG(CLUSTERECTLR, 1),
};

static struct core_register arm_a75_regs[] = {
	SET_CORE_REG(SCTLR, 0),
	SET_CORE_REG(MAIR, 0),
	SET_CORE_REG(MPIDR, 0),
	SET_CORE_REG(MIDR, 0),
	SET_CORE_REG(REVIDR, 0),
	SET_CORE_REG(CPUACTLR_v82, 1),
	SET_CORE_REG(CPUACTLR2_v82, 1),
	SET_CORE_REG(CPUECTLR_v82, 1),
};


static struct core_register arm_a76_regs[] = {
	SET_CORE_REG(SCTLR, 0),
	SET_CORE_REG(MAIR, 0),
	SET_CORE_REG(MPIDR, 0),
	SET_CORE_REG(MIDR, 0),
	SET_CORE_REG(REVIDR, 0),
	SET_CORE_REG(CPUACTLR_v82, 1),
	SET_CORE_REG(CPUACTLR2_v82, 1),
	SET_CORE_REG(CPUACTLR3_v82, 1),
	SET_CORE_REG(CPUECTLR_v82, 1),
};

static struct core_register arm_a77_regs[] = {
	SET_CORE_REG(SCTLR, 0),
	SET_CORE_REG(MAIR, 0),
	SET_CORE_REG(MPIDR, 0),
	SET_CORE_REG(MIDR, 0),
	SET_CORE_REG(REVIDR, 0),
	SET_CORE_REG(CPUACTLR_v82, 1),
	SET_CORE_REG(CPUACTLR2_v82, 1),
	SET_CORE_REG(CPUACTLR3_v82, 1),
	SET_CORE_REG(CPUECTLR_v82, 1),
};

static struct core_register arm_a78_regs[] = {
	SET_CORE_REG(SCTLR, 0),
	SET_CORE_REG(MAIR, 0),
	SET_CORE_REG(MPIDR, 0),
	SET_CORE_REG(MIDR, 0),
	SET_CORE_REG(REVIDR, 0),
	SET_CORE_REG(CPUACTLR_v82, 1),
	SET_CORE_REG(CPUACTLR2_v82, 1),
	SET_CORE_REG(CPUACTLR3_v82, 1),
	SET_CORE_REG(CPUECTLR_v82, 1),
};

static struct core_register arm_a79_regs[] = {
	SET_CORE_REG(SCTLR, 0),
	SET_CORE_REG(MAIR, 0),
	SET_CORE_REG(MPIDR, 0),
	SET_CORE_REG(MIDR, 0),
	SET_CORE_REG(REVIDR, 0),
	SET_CORE_REG(CPUACTLR_v82, 1),
	SET_CORE_REG(CPUACTLR2_v82, 1),
	SET_CORE_REG(CPUACTLR3_v82, 1),
	SET_CORE_REG(CPUECTLR_v82, 1),
};

enum company_type {
	ARM = 0x41,
	SAMSUNG = 0x53,
};
enum arm_core {
	CORTEX_A53 = 0xD03,
	CORTEX_A57 = 0XD07,
	CORTEX_A73 = 0xD09,
	CORTEX_A75 = 0xD0A,
	CORTEX_A76 = 0xD0B,
	CORTEX_A77 = 0xD0C,
	CORTEX_A78 = 0xD41,
	CORTEX_A79 = 0xD44,
	ANANKE = 0xD05,
};
enum samsung_core {
	MONGOOSE = 0x001,
	MEERKAT = 0x002,
	CHEETAH = 0x003,
	LION = 0x004,
};
static char *arm_core_names[] = {"", "", "", "A53", "", "ANANKE", "", "A57", "", "A73", "A75", "A76", "A77"};
static char *arm_core_names2[] = {"", "HERC", "", "", "HERA"};

static int core_num = 0;

static enum company_type get_core_company(void)
{
	u32 midr = (u32)mrs_MIDR_read();
	/*
	 *	31:24	- implementer
	 *	23:20	- variant
	 *	19:16	- architecture
	 *	11:4	- part number
	 *	3:0	- revision
	 */
	return (midr >> 24);
}

static int get_core_type(void)
{
	u32 midr = (u32)mrs_MIDR_read();
	/*
	 *	31:24	- implementer
	 *	23:20	- variant
	 *	19:16	- architecture
	 *	11:4	- part number
	 *	3:0	- revision
	 */
	return ((midr & 0xfff0) >> 4);
}

static int count = 1;

static int creg_show(void *data)
{
	struct seq_file *file = (struct seq_file *)data;
	u64 val;
	enum company_type company = get_core_company();
	int core_type = get_core_type();
	struct core_register *regs = NULL;
	u32 midr = (u32)mrs_MIDR_read();
	u32 revidr = (u32)mrs_REVIDR_read();
	char *core_name;
	int core_reg_count = 0, i;
	int idx = 0;

	switch (company) {
	case ARM:
		idx = core_type - 0xD00;
		if (idx > 0x40) {
			idx -= 0x40;
			core_name = arm_core_names2[idx];
		} else {
			if (idx < (sizeof(arm_core_names)/sizeof(char *)))
				core_name = arm_core_names[idx];
			else
				return -EINVAL;
		}
		switch (core_type) {
		case CORTEX_A57:
			regs = arm_v8_regs;
			core_reg_count = ARRAY_SIZE(arm_v8_regs);
			break;
		case CORTEX_A73:
			regs = arm_a73_regs;
			core_reg_count = ARRAY_SIZE(arm_a73_regs);
			break;
		case CORTEX_A75:
			regs = arm_a75_regs;
			core_reg_count = ARRAY_SIZE(arm_a75_regs);
			break;
		case CORTEX_A76:
			regs = arm_a76_regs;
			core_reg_count = ARRAY_SIZE(arm_a76_regs);
			break;
		case CORTEX_A77:
			regs = arm_a77_regs;
			core_reg_count = ARRAY_SIZE(arm_a77_regs);
			break;
		case CORTEX_A78:
			regs = arm_a78_regs;
			core_reg_count = ARRAY_SIZE(arm_a78_regs);
			break;
		case CORTEX_A79:
			regs = arm_a79_regs;
			core_reg_count = ARRAY_SIZE(arm_a79_regs);
			break;
		case CORTEX_A53:
			regs = arm_v8_regs;
			core_reg_count = ARRAY_SIZE(arm_v8_regs);
			break;
		case ANANKE:
			regs = arm_a55_regs;
			core_reg_count = ARRAY_SIZE(arm_a55_regs);
			break;
		}
		break;
	default:
		core_name = "unknown";
		regs = arm_v8_regs;
		core_reg_count = ARRAY_SIZE(arm_v8_regs);
		break;
	}

	seq_printf(file, "%s r%xp%x revidr=0x%08x\n",
			core_name,
			(midr & 0xf00000) >> 20,
			midr & 0xf,
			revidr);

	if (regs == NULL || !core_reg_count)
		return -EINVAL;

	for (i = 0; i < core_reg_count; ++i) {
		val = regs[i].reg->read_reg();
		seq_printf(file, "%10s: 0x%016llX\n", regs[i].reg->name, val);
	}

	return 0;
}

static struct task_struct *task;
static int creg_seq_show(struct seq_file *file, void *iter)
{
	count = 1;
	task = kthread_create_on_node(creg_show, file, NUMA_NO_NODE, "thread%u", 0);
	kthread_bind(task, core_num);
	wake_up_process(task);
	msleep(1000);

	return 0;
}

/*-----------*/
static int core_seq_show(struct seq_file *file, void *iter)
{
	seq_printf(file, "%d", core_num);
	return 0;
}
static ssize_t core_seq_write(struct file *file, const char __user *buffer, size_t count, loff_t *off)
{
	char buf[10];
	count = (count > 10)? 10 : count;
	if (copy_from_user(buf, buffer, count) != 0)
		return -EFAULT;
	sscanf(buf, "%d", &core_num);
	return count;
}

/*-----------*/
static int creg_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, creg_seq_show, inode->i_private);
}
static int core_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, core_seq_show, inode->i_private);
}

static struct file_operations creg_debugfs_fops = {
	.owner          = THIS_MODULE,
	.open           = creg_debugfs_open,
	.write          = NULL,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};
static struct file_operations core_debugfs_fops = {
	.owner          = THIS_MODULE,
	.open           = core_debugfs_open,
	.write          = core_seq_write,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};


// main
int exynos_perf_cpu_init(struct platform_device *pdev)
{
	struct dentry *root, *d;

	root = debugfs_create_dir("exynos_perf_cpu", NULL);
	if (!root) {
		printk("%s: create debugfs\n", __FILE__);
		return -ENOMEM;
	}

	d = debugfs_create_file("creg", S_IRUSR, root,
					(unsigned int *)0,
					&creg_debugfs_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("core", S_IRUSR, root,
					(unsigned int *)0,
					&core_debugfs_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}
