/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/reset/exynos/exynos-reset.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <soc/samsung/exynos/exynos-soc.h>
#include <linux/sched/clock.h>
#include <linux/module.h>
#include <linux/math.h>

#include "acpm.h"
#include "acpm_ipc.h"
#include "fw_header/framework.h"

static int ipc_done;
static unsigned long long ipc_time_start;
static unsigned long long ipc_time_end;
static void __iomem *fvmap_base_address;
static void __iomem *acpm_sram_base;

static struct acpm_info *exynos_acpm;

struct task_info *task_info;
bool *task_profile_running;

static int acpm_send_data(struct device_node *node, unsigned int check_id,
		struct ipc_config *config);

u32 acpm_num_eint_clk_users;

void *get_fvmap_base(void)
{
	return fvmap_base_address;
}
EXPORT_SYMBOL_GPL(get_fvmap_base);

void acpm_set_sram_addr(void __iomem *base)
{
	acpm_sram_base = base;
}

static void fvmap_base_init(void)
{
	const __be32 *prop;
	unsigned int offset;
	int len;

	prop = of_get_property(exynos_acpm->dev->of_node, "fvmap_offset", &len);
	if (prop) {
		offset = be32_to_cpup(prop);

		fvmap_base_address = acpm_sram_base + offset;

		pr_err("acpm_sram_base: 0x%x\n", acpm_sram_base);
		pr_err("fvmap_base_address: 0x%x\n", fvmap_base_address);
	}
}

static int debug_log_level_get(void *data, unsigned long long *val)
{

	return 0;
}

static int debug_log_level_set(void *data, unsigned long long val)
{
	acpm_fw_log_level((unsigned int)val);

	return 0;
}

static int debug_ipc_loopback_test_get(void *data, unsigned long long *val)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	config.cmd = cmd;
	config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_TEST);
	config.cmd[0] |= 0x3 << ACPM_IPC_PROTOCOL_ID;

	config.response = true;
	config.indirection = false;

	ipc_time_start = sched_clock();
	ret = acpm_send_data(exynos_acpm->dev->of_node, 3, &config);
	ipc_time_end = sched_clock();

	if (!ret)
		*val = ipc_time_end - ipc_time_start;
	else
		*val = 0;

	config.cmd = NULL;

	return 0;
}

static int debug_ipc_loopback_test_set(void *data, unsigned long long val)
{

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_log_level_fops,
		debug_log_level_get, debug_log_level_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(debug_ipc_loopback_test_fops,
		debug_ipc_loopback_test_get, debug_ipc_loopback_test_set, "%llu\n");

static bool is_running_task_profiler(void)
{
	return __raw_readl(task_profile_running);
}

static int get_task_num(void)
{
	return task_info->num_tasks;
}

static int get_task_profile_data(int task_id)
{
	struct acpm_task_struct *tasks;

	if (task_id >= get_task_num())
		return -EINVAL;

	tasks = (struct acpm_task_struct *)(get_acpm_sram_base() + task_info->tasks);

	return tasks[task_id].profile.latency;
}

static int acpm_enable_task_profiler(int start)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;

	if (start) {
		config.cmd[0] = (1 << ACPM_IPC_TASK_PROF_START);
		ret = acpm_send_data(exynos_acpm->dev->of_node, ACPM_IPC_TASK_PROF_START, &config);
	} else {
		config.cmd[0] = (1 << ACPM_IPC_TASK_PROF_END);
		ret = acpm_send_data(exynos_acpm->dev->of_node, ACPM_IPC_TASK_PROF_END, &config);
	}

	config.cmd = NULL;

	return 0;
}

static ssize_t task_profile_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;
	char buf[32];

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);
	if (ret < 0)
		return ret;

	if (buf[0] == '0') acpm_enable_task_profiler(0);
	if (buf[0] == '1') acpm_enable_task_profiler(1);

	return ret;
}
static ssize_t task_profile_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *buf;
	int r, task_num, i;
	unsigned int *util, sum = 0;
	ssize_t size = 0;

	buf = kzalloc(sizeof(char) * 2048, GFP_KERNEL);

	if (is_running_task_profiler()){
		size += sprintf(buf + size, "!!task profiler running!!\n");
		size += sprintf(buf + size, "[stop task profiler]\n");
		size += sprintf(buf + size, "echo 0 > /d/acpm_framework/task_profile\n");

		r = simple_read_from_buffer(ubuf, cnt, ppos, buf, (int)size);
		kfree(buf);
		return r;
	}

	task_num = get_task_num();

	util = kzalloc(sizeof(unsigned int) * task_num, GFP_KERNEL);

	for (i = 0; i < task_num; i++) {
		util[i] = get_task_profile_data(i);
		sum += util[i];
	}

	for (i = 0; i < task_num; i++) {
		if (util[i] == 0)
			size += sprintf(buf + size, "TASK[%d] util : 0 %%\n", i);
		else
			size += sprintf(buf + size, "TASK[%d] util : %d.%2d %%\n", i,
					(util[i] * 100) / sum,
					DIV_ROUND_CLOSEST(((util[i] * 100) % sum) * 100, sum));
	}

	r = simple_read_from_buffer(ubuf, cnt, ppos, buf, (int)size);
	kfree(buf);
	kfree(util);

	return r;
}

static const struct file_operations debug_task_profile_fops = {
	.open		= simple_open,
	.read		= task_profile_read,
	.write		= task_profile_write,
	.llseek		= default_llseek,
};

void acpm_init_eint_clk_req(u32 eint_num)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	if (++acpm_num_eint_clk_users > 1) {
		WARN(1, "There is already 1 eint clk users\n");
		return;
	}

	config.cmd = cmd;
	/* Plugin ID = FLEXPMU */
	config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_TEST)
#if defined(CONFIG_SOC_S5E9925) || defined(CONFIG_SOC_S5E8535) || defined(CONFIG_SOC_S5E8835)
				| (0x4 << ACPM_IPC_PROTOCOL_ID) | (0x6 << IPC_PLUGIN_ID);
#elif defined(CONFIG_SOC_S5E9935)
				| (0x4 << ACPM_IPC_PROTOCOL_ID) | (0x5 << IPC_PLUGIN_ID);
#endif

	config.cmd[0] |= (4 << 0);
	config.cmd[1] = eint_num;

	config.response = true;
	config.indirection = false;

	ret = acpm_send_data(exynos_acpm->dev->of_node, 4, &config);

	config.cmd = NULL;
}
EXPORT_SYMBOL_GPL(acpm_init_eint_clk_req);

void acpm_init_eint_nfc_clk_req(u32 eint_num)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	config.cmd = cmd;
	/* Plugin ID = FLEXPMU */
	config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_TEST)
#if defined(CONFIG_SOC_S5E9925) || defined(CONFIG_SOC_S5E8535) || defined(CONFIG_SOC_S5E8835)
				| (0x4 << ACPM_IPC_PROTOCOL_ID) | (0x6 << IPC_PLUGIN_ID);
#elif defined(CONFIG_SOC_S5E9935)
				| (0x4 << ACPM_IPC_PROTOCOL_ID) | (0x5 << IPC_PLUGIN_ID);
#endif

	config.cmd[0] |= (5 << 0);
	config.cmd[1] = eint_num;

	config.response = true;
	config.indirection = false;

	ret = acpm_send_data(exynos_acpm->dev->of_node, 4, &config);

	config.cmd = NULL;
}
EXPORT_SYMBOL_GPL(acpm_init_eint_nfc_clk_req);

u32 acpm_enable_flexpmu_profiler(u32 start)
{
	unsigned int command[4];
	struct ipc_config config;
	int ret = 0;

	config.cmd = command;
	config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_TEST);
	config.cmd[0] |= 0x5 << ACPM_IPC_PROTOCOL_ID;
#if defined(CONFIG_SOC_S5E9935)
	config.cmd[0] |= 0x5 << ACPM_IPC_PLUGIN_ID;
#elif defined(CONFIG_SOC_S5E8535) || defined(CONFIG_SOC_S5E8835)
	config.cmd[0] |= 0x6 << ACPM_IPC_PLUGIN_ID;
#endif
	config.cmd[0] |= (2 << 0);
	config.cmd[1] = start;

	config.response = true;
	ret = acpm_send_data(exynos_acpm->dev->of_node, 5, &config);
	if (ret) {
		pr_err("%s - acpm_ipc_send_data fail.\n", __func__);
		return ret;
	}

	config.cmd = NULL;
	return ret;
}
EXPORT_SYMBOL_GPL(acpm_enable_flexpmu_profiler);

static void acpm_debugfs_init(struct acpm_info *acpm)
{
	struct dentry *den;

	den = debugfs_create_dir("acpm_framework", NULL);
	debugfs_create_file("ipc_loopback_test", 0644, den, acpm, &debug_ipc_loopback_test_fops);
	debugfs_create_file("log_level", 0644, den, NULL, &debug_log_level_fops);
	debugfs_create_file("task_profile", 0644, den, NULL, &debug_task_profile_fops);
}

void *memcpy_align_4(void *dest, const void *src, unsigned int n)
{
	unsigned int *dp = dest;
	const unsigned int *sp = src;
	int i;

	if ((n % 4))
		BUG();

	n = n >> 2;

	for (i = 0; i < n; i++)
		*dp++ = *sp++;

	return dest;
}

void acpm_enter_wfi(void)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	if (exynos_acpm->enter_wfi)
		return;

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = 1 << ACPM_IPC_PROTOCOL_STOP;

	ret = acpm_send_data(exynos_acpm->dev->of_node, ACPM_IPC_PROTOCOL_STOP, &config);

	config.cmd = NULL;

	if (ret) {
		pr_err("[ACPM] acpm enter wfi fail!!\n");
	} else {
		pr_err("[ACPM] wfi done\n");
		exynos_acpm->enter_wfi++;
	}
}

u32 exynos_get_peri_timer_icvra(void)
{
       return (EXYNOS_PERI_TIMER_MAX - __raw_readl(exynos_acpm->timer_base + EXYNOS_TIMER_APM_TCVR)) & EXYNOS_PERI_TIMER_MAX;
}

void exynos_acpm_timer_clear(void)
{
	writel(exynos_acpm->timer_cnt, exynos_acpm->timer_base + EXYNOS_TIMER_APM_TCVR);
}

void exynos_acpm_reboot(void)
{
	u32 soc_id, revision;

	soc_id = exynos_soc_info.product_id;
	revision = exynos_soc_info.revision;

	acpm_enter_wfi();
}
EXPORT_SYMBOL_GPL(exynos_acpm_reboot);

void exynos_acpm_force_apm_wdt_reset(void)
{
	struct ipc_config config;
	int ret = 0;
	unsigned int cmd[4] = {0, };

	config.cmd = cmd;
	config.response = true;
	config.indirection = false;
	config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_STOP_WDT);

	ret = acpm_send_data(exynos_acpm->dev->of_node, ACPM_IPC_PROTOCOL_STOP_WDT, &config);

	pr_err("ACPM force WDT reset. (ret: %d)\n", ret);
}
EXPORT_SYMBOL_GPL(exynos_acpm_force_apm_wdt_reset);

static int acpm_send_data(struct device_node *node, unsigned int check_id,
		struct ipc_config *config)
{
	unsigned int channel_num, size;
	int ret = 0;
	int timeout_flag;
	unsigned int id = 0;

	if (!acpm_ipc_request_channel(node, NULL,
				&channel_num, &size)) {
		ipc_done = -1;

		ipc_time_start = sched_clock();
		ret = acpm_ipc_send_data(channel_num, config);

		if (config->cmd[0] & (1 << ACPM_IPC_TASK_PROF_START)) {
			ipc_done = ACPM_IPC_TASK_PROF_START;
		} else if (config->cmd[0] & (1 << ACPM_IPC_TASK_PROF_END)) {
			ipc_done = ACPM_IPC_TASK_PROF_END;
		} else if (config->cmd[0] & (1 << ACPM_IPC_PROTOCOL_STOP)) {
			ipc_done = ACPM_IPC_PROTOCOL_STOP;
		} else if (config->cmd[0] & (1 << ACPM_IPC_PROTOCOL_TEST)) {
			id = config->cmd[0] & ACPM_IPC_PROTOCOL_IDX;
			id = id	>> ACPM_IPC_PROTOCOL_ID;
			ipc_done = id;
		} else {
			id = config->cmd[0] & ACPM_IPC_PROTOCOL_IDX;
			id = id	>> ACPM_IPC_PROTOCOL_ID;
			ipc_done = id;
		}

		/* Response interrupt waiting */
		UNTIL_EQUAL(ipc_done, check_id, timeout_flag);

		if (timeout_flag)
			ret = -ETIMEDOUT;

		acpm_ipc_release_channel(node, channel_num);
	} else {
		pr_err("%s ipc request_channel fail, id:%u, size:%u\n",
				__func__, channel_num, size);
		ret = -EBUSY;
	}

	return ret;
}

static int acpm_probe(struct platform_device *pdev)
{
	struct acpm_info *acpm;
	struct device_node *node = pdev->dev.of_node;
	struct resource *res;
	int ret = 0;

	dev_info(&pdev->dev, "acpm probe\n");

	if (!node) {
		dev_err(&pdev->dev, "driver doesnt support"
				"non-dt devices\n");
		return -ENODEV;
	}

	acpm = devm_kzalloc(&pdev->dev,
			sizeof(struct acpm_info), GFP_KERNEL);
	if (IS_ERR(acpm))
		return PTR_ERR(acpm);
	if (!acpm)
		return -ENOMEM;

	acpm->dev = &pdev->dev;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "timer_apm");
	acpm->timer_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(acpm->timer_base))
		dev_info(&pdev->dev, "could not find timer base addr \n");

		if (of_property_read_u32(node, "peritimer-cnt", &acpm->timer_cnt))
		pr_warn("No matching property: peritiemr_cnt\n");

	exynos_reboot_register_acpm_ops(exynos_acpm_reboot);

	exynos_acpm = acpm;

	acpm_debugfs_init(acpm);

	exynos_acpm_timer_clear();

	fvmap_base_init();

	dev_info(&pdev->dev, "acpm probe done.\n");

	return ret;
}

static int acpm_remove(struct platform_device *pdev)
{
	return 0;
}

static struct acpm_ipc_drvdata acpm_drvdata = {
	.is_mailbox_master = true,
	.is_apm = true,
	.is_asm = false,
};

static const struct of_device_id acpm_ipc_match[] = {
	{
		.compatible = "samsung,exynos-acpm-ipc",
		.data = &acpm_drvdata,
	},
	{},
};
MODULE_DEVICE_TABLE(of, acpm_ipc_match);

static struct platform_driver samsung_acpm_ipc_driver = {
	.probe	= acpm_ipc_probe,
	.remove	= acpm_ipc_remove,
	.driver	= {
		.name = "exynos-acpm-ipc",
		.owner	= THIS_MODULE,
		.of_match_table	= acpm_ipc_match,
	},
};

static const struct of_device_id acpm_match[] = {
	{ .compatible = "samsung,exynos-acpm" },
	{},
};
MODULE_DEVICE_TABLE(of, acpm_match);

static struct platform_driver samsung_acpm_driver = {
	.probe	= acpm_probe,
	.remove	= acpm_remove,
	.driver	= {
		.name = "exynos-acpm",
		.owner	= THIS_MODULE,
		.of_match_table	= acpm_match,
	},
};

static int exynos_acpm_init(void)
{
	platform_driver_register(&samsung_acpm_ipc_driver);
	platform_driver_register(&samsung_acpm_driver);
	return 0;
}
postcore_initcall_sync(exynos_acpm_init);

static void exynos_acpm_exit(void)
{
	platform_driver_unregister(&samsung_acpm_ipc_driver);
	platform_driver_unregister(&samsung_acpm_driver);
}
module_exit(exynos_acpm_exit);

MODULE_LICENSE("GPL");
