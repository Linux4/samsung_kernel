// SPDX-License-Identifier: GPL-2.0
/*
 * cpufreq-dbg-lite.c - eem debug driver
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Tungchen Shih <tungchen.shih@mediatek.com>
 */

/* system includes */
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "../mcupm/include/mcupm_driver.h"
#include "../mcupm/include/mcupm_ipi_id.h"
#include "cpufreq-dbg-lite.h"
#include "sugov/cpufreq.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[cpudvfs]: " fmt

static void __iomem *csram_base;

u32 *g_cpufreq_debug;

unsigned int cpufreq_debug_cpu;
unsigned int user_ctrl_mode;
bool dsu_ctrl_deubg_enable;

static int cpufreq_debug_proc_show(struct seq_file *m, void *v)
{
	struct cpufreq_policy *policy;

	if (cpufreq_debug_cpu >= 8) {
		seq_printf(m, "cpu%u is invalid!\n", cpufreq_debug_cpu);
		return 0;
	}

	policy = cpufreq_cpu_get(cpufreq_debug_cpu);
	if (policy == NULL) {
		seq_printf(m, "policy of cpu%u is null!\n", cpufreq_debug_cpu);
		return 0;
	}

	seq_printf(m, "cpu%u freq[%u]: policy min[%u] max[%u], cpuinfo min[%u] max[%u]\n",
		cpufreq_debug_cpu,
		policy->cur,
		policy->min,
		policy->max,
		policy->cpuinfo.min_freq,
		policy->cpuinfo.max_freq);
	cpufreq_cpu_put(policy);

	return 0;
}

static ssize_t cpufreq_debug_proc_write(struct file *file,
	const char __user *buffer, size_t count, loff_t *pos)
{
	int cpu = 0, min = 0, max = 0;
	unsigned long MHz;
	unsigned long mHz;
	char *buf = (char *) __get_free_page(GFP_USER);
	struct device *cpu_dev = get_cpu_device(cpu);
	struct cpufreq_policy *policy;

	if (!buf)
		return -ENOMEM;

	if (count > 255) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (copy_from_user(buf, buffer, count)) {
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	if (sscanf(buf, "%d %d %d", &cpu, &min, &max) != 3) {
		cpufreq_debug_cpu = cpu;
		free_page((unsigned long)buf);
		return -EINVAL;
	}

	free_page((unsigned long)buf);
	if (min <= 0 || max <= 0)
		return -EINVAL;
	MHz = max;
	mHz = min;
	MHz = MHz * 1000;
	mHz = mHz * 1000;
	dev_pm_opp_find_freq_floor(cpu_dev, &MHz);
	dev_pm_opp_find_freq_floor(cpu_dev, &mHz);

	max = (unsigned int)(MHz / 1000);
	min = (unsigned int)(mHz / 1000);

	policy = cpufreq_cpu_get(cpu);
	if (policy == NULL)
		return count;
	down_write(&policy->rwsem);
	policy->cpuinfo.max_freq = max;
	policy->cpuinfo.min_freq = min;
	up_write(&policy->rwsem);
	cpufreq_cpu_put(policy);
	/* cpufreq_update_limits(cpu); */
	return count;
}

PROC_FOPS_RW(cpufreq_debug);

static int create_cpufreq_debug_fs(void)
{
	struct proc_dir_entry *dir = NULL;
	int i;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};

	const struct pentry entries[] = {
		PROC_ENTRY_DATA(cpufreq_debug),
	};

	/* create /proc/cpudvfs */
	dir = proc_mkdir("cpudvfs", NULL);
	if (!dir) {
		pr_info("fail to create /proc/cpudvfs @ %s()\n",
								__func__);
		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		pr_info("%s created!\n", entries[i].name);
		if (!proc_create_data
			(entries[i].name, 0664,
			dir, entries[i].fops, NULL))
			pr_info("%s(), create /proc/cpudvfs/%s failed\n",
						__func__, entries[i].name);
	}
	return 0;
}

void set_cci_mode(unsigned int mode)
{
	/* mode = 0(Normal as 50%) mode = 1(Perf as 70%) */
	csram_write(OFFS_CCI_TBL_MODE, mode);
}

unsigned int cpufreq_get_cci_mode(void)
{
	unsigned int mode;

	/* Normal mode: 0, Perf mode: 1*/
	mode = csram_read(OFFS_CCI_TBL_MODE);

	if (mode > 1)
		return -1;

	return mode;
}

int cpufreq_set_cci_mode(unsigned int mode)
{
	if (mode > 1) {
		pr_info("%s: invalid input value: %d.\n", __func__, mode);
		return -EINVAL;
	}

	user_ctrl_mode = mode;
	set_cci_mode(user_ctrl_mode);
	if (dsu_ctrl_deubg_enable) {
		pr_info("%s: debug mode.\n", __func__);
		return 0;
	}

	if (user_ctrl_mode)
		set_eas_dsu_ctrl(0);
	else
		set_eas_dsu_ctrl(1);

	return 0;
}
EXPORT_SYMBOL_GPL(cpufreq_set_cci_mode);

int set_dsu_ctrl_debug(unsigned int eas_ctrl_mode, bool debug_enable)
{
	dsu_ctrl_deubg_enable = debug_enable;

	if (dsu_ctrl_deubg_enable)
		set_eas_dsu_ctrl(eas_ctrl_mode);
	else {
		if (user_ctrl_mode)
			set_eas_dsu_ctrl(0);
		else
			set_eas_dsu_ctrl(1);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(set_dsu_ctrl_debug);

static int mtk_cpudvfs_init(void)
{
	struct device_node *dvfs_node;
	struct platform_device *pdev;
	struct resource *csram_res;

	dvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (dvfs_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev = of_find_device_by_node(dvfs_node);
	if (pdev == NULL) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	create_cpufreq_debug_fs();

	csram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	csram_base = ioremap(csram_res->start, resource_size(csram_res));
	if (csram_base == NULL) {
		pr_notice("failed to map csram_base @ %s\n", __func__);
		return -EINVAL;
	}
	dsu_ctrl_deubg_enable = false;
	user_ctrl_mode = 0;

	return 0;
}
module_init(mtk_cpudvfs_init);

static void mtk_cpudvfs_exit(void)
{
}
module_exit(mtk_cpudvfs_exit);

MODULE_DESCRIPTION("MTK CPU DVFS Platform Driver Helper v0.1.1");
MODULE_AUTHOR("Tungchen Shih <tungchen.shih@mediatek.com>");
MODULE_LICENSE("GPL v2");
