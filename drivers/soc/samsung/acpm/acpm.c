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
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/exynos-ss.h>
#include <linux/reboot.h>

#include "acpm.h"
#include "acpm_ipc.h"

static struct workqueue_struct *debug_logging_wq;

static int ipc_done;
static unsigned long long ipc_time_start;
static unsigned long long ipc_time_end;

static struct acpm_debug_info *debug;
static struct acpm_info *exynos_acpm;

static int acpm_send_data(struct device_node *node, unsigned int check_id,
		struct ipc_config *config);

static void acpm_framework_handler(unsigned int *cmd, unsigned int size)
{
	unsigned int id = 0;

	ipc_time_end = sched_clock();

	if (cmd[0] & ACPM_IPC_PROTOCOL_DP_CMD) {
		id = cmd[0] & ACPM_IPC_PROTOCOL_IDX;
		id = id	>> ACPM_IPC_PROTOCOL_ID;
		ipc_done = id;
	} else if (cmd[0] & (1 << ACPM_IPC_PROTOCOL_STOP)) {

		ipc_done = ACPM_IPC_PROTOCOL_STOP;
	} else if (cmd[0] & (1 << ACPM_IPC_PROTOCOL_TEST)) {
		id = cmd[0] & ACPM_IPC_PROTOCOL_IDX;
		id = id	>> ACPM_IPC_PROTOCOL_ID;
		ipc_done = id;
	}
}

static int plugin_fw_connet(struct device_node *node, unsigned int id,
		const char *p_name, unsigned int attach)
{
	struct ipc_config config;
	int ret = 0;

	config.cmd = kzalloc(SZ_16, GFP_KERNEL);
	if (attach)
		config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_DP_A);
	else
		config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_DP_D);

	config.cmd[0] |= id << ACPM_IPC_PROTOCOL_ID;

	config.responce = true;

	ret = acpm_send_data(node, id, &config);

	kfree(config.cmd);

	if (!ret) {
		if (attach)
			pr_info("[ACPM] %s plugin attach done!\n", p_name);
		else
			pr_info("[ACPM] %s plugin detach done!\n", p_name);
	}

	return ret;
}

static void firmware_load(void *base, const char *firmware, int size)
{
	memcpy(base, firmware, size);
}

static int firmware_update(struct device *dev, struct acpm_plugins *plugin)
{
	const struct firmware *fw_entry = NULL;
	int err;

	dev_info(dev, "Loading %s firmware ... ", plugin->fw_name);
	err = request_firmware(&fw_entry, plugin->fw_name, dev);
	if (err) {
		dev_err(dev, "firmware request FAIL \n");
		return err;
	}

	if (fw_entry) {
		firmware_load(plugin->fw_base, fw_entry->data, fw_entry->size);
	} else {
		dev_err(dev, "firmware loading FAIL \n");
		return -ENODEV;
	}

	dev_info(dev, "OK \n");

	release_firmware(fw_entry);

	return 0;
}

static int plugins_init(struct device_node *node, struct acpm_info *acpm)
{
	struct device_node *child;
	const __be32 *prop_0, *prop_1;
	int ret = 0;
	unsigned int len;
	int i = 0;
	unsigned int attach;

	acpm->plugin_num = of_get_child_count(node);

	acpm->plugin = devm_kzalloc(acpm->dev,
			sizeof(struct acpm_plugins) * acpm->plugin_num, GFP_KERNEL);

	for_each_child_of_node(node, child) {
		acpm->plugin[i].np = child;

		prop_0 = of_get_property(child, "attach", &len);
		if (prop_0)
			attach = be32_to_cpup(prop_0);
		else
			attach = 1;

		prop_0 = of_get_property(child, "id", &len);
		if (prop_0)
			acpm->plugin[i].id = be32_to_cpup(prop_0);
		else
			acpm->plugin[i].id = i;

		if (attach == 1) {
			if (of_property_read_string(child, "fw_name", &acpm->plugin[i].fw_name))
				return -ENODEV;

			prop_0 = of_get_property(child, "fw_base", &len);
			prop_1 = of_get_property(child, "fw_size", &len);
			if (prop_0 && prop_1) {
				acpm->plugin[i].fw_base = ioremap(be32_to_cpup(prop_0),
						be32_to_cpup(prop_1));
			} else {
				dev_info(acpm->dev, "fw base ioremap fail!!\n");
			}

			if (acpm->plugin[i].fw_base) {
				firmware_update(acpm->dev, &acpm->plugin[i]);
				iounmap(acpm->plugin[i].fw_base);
			}
		}

		if (attach != 2) {
			ret = plugin_fw_connet(child, acpm->plugin[i].id, child->name, attach);
			if (ret < 0)
				dev_err(acpm->dev, "plugin attach/detach:%u fail! plugin_name:%s, ret:%d",
						attach, child->name, ret);
		}

		i++;
	}

	return ret;
}

static int debug_ipc_loopback_test_get(void *data, unsigned long long *val)
{
	struct acpm_info *acpm = data;
	struct ipc_config config;
	int ret = 0;

	config.cmd = kzalloc(SZ_16, GFP_KERNEL);
	config.cmd[0] = (1 << ACPM_IPC_PROTOCOL_TEST);
	config.cmd[0] |= 0x3 << ACPM_IPC_PROTOCOL_ID;

	config.responce = true;

	ret = acpm_send_data(acpm->plugin[0].np, 3, &config);

	if (!ret)
		*val = ipc_time_end - ipc_time_start;
	else
		*val = 0;

	kfree(config.cmd);

	return 0;
}

static int debug_ipc_loopback_test_set(void *data, unsigned long long val)
{

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_ipc_loopback_test_fops,
		debug_ipc_loopback_test_get, debug_ipc_loopback_test_set, "%llu\n");

static void acpm_debugfs_init(struct acpm_info *acpm)
{
	struct dentry *den;

	den = debugfs_create_dir("acpm_framework", NULL);
	debugfs_create_file("ipc_loopback_test", 0644, den, acpm, &debug_ipc_loopback_test_fops);
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

void timestamp_write(void)
{
	unsigned int tmp_index;

	if (spin_trylock(&debug->lock)) {
		tmp_index = __raw_readl(debug->time_index);

		tmp_index++;

		if (tmp_index == debug->time_len)
			tmp_index = 0;

		debug->timestamps[tmp_index] = sched_clock();

		__raw_writel(tmp_index, debug->time_index);
		spin_unlock(&debug->lock);
	}
}

void acpm_log_print(void)
{
	unsigned int front;
	unsigned int rear;
	unsigned int id;
	unsigned int index;
	unsigned int count;
	unsigned char str[9] = {0,};
	unsigned int val;
	unsigned int log_header;
	unsigned long long time;

	/* ACPM Log data dequeue & print */
	front = __raw_readl(debug->log_buff_front);
	rear = __raw_readl(debug->log_buff_rear);

	while (rear != front) {
		log_header = __raw_readl(debug->log_buff_base + debug->log_buff_size * rear);

		/* log header information
		 * id: [31:29]
		 * index: [28:23]
		 * apm systick count: [22:0]
		 */
		id = (log_header & (0x7 << LOG_ID_SHIFT)) >> LOG_ID_SHIFT;
		index = (log_header & (0x3f << LOG_TIME_INDEX)) >> LOG_TIME_INDEX;
		count = log_header & 0x7fffff;

		/* string length: log_buff_size - header(4) - integer_data(4) */
		memcpy_align_4(str, debug->log_buff_base + (debug->log_buff_size * rear) + 4,
				debug->log_buff_size - 8);

		val = __raw_readl(debug->log_buff_base + debug->log_buff_size * rear +
				debug->log_buff_size - 4);

		time = debug->timestamps[index];

		/* systick period: 1 / 26MHz */
		time += count * APM_SYSTICK_NS_PERIOD;

		exynos_ss_acpm(time, str, val);
		if (debug->debug_logging_level == 1)
			pr_info("[ACPM_FW][%llu] id:%u, %s, %u\n", time, id, str, val);
		else {
			exynos_ss_printk("[ACPM_FW][%llu] id:%u, %s, %u\n", time, id, str, val);
			trace_printk("[ACPM_FW][%llu] id:%u, %s, %u\n", time, id, str, val);
		}

		if (debug->log_buff_len == (rear + 1))
			rear = 0;
		else
			rear++;

		__raw_writel(rear, debug->log_buff_rear);
		front = __raw_readl(debug->log_buff_front);
	}

	timestamp_write();
}

static void acpm_debug_logging(struct work_struct *work)
{
	acpm_log_print();

	queue_delayed_work(debug_logging_wq, &debug->work,
			msecs_to_jiffies(debug->period));
}

void exynos_apm_power_down(void)
{
	unsigned int reg;

	reg = __raw_readl(exynos_acpm->mcore_base + EXYNOS_PMU_CORTEX_APM_CONFIGURATION);
	reg &= APM_LOCAL_PWR_CFG_RESET;
	__raw_writel(reg, exynos_acpm->mcore_base + EXYNOS_PMU_CORTEX_APM_CONFIGURATION);
}

void acpm_enter_wfi(void)
{
	struct ipc_config config;
	int ret = 0;

	config.cmd = kzalloc(SZ_16, GFP_KERNEL);
	config.responce = true;
	config.cmd[0] = 1 << ACPM_IPC_PROTOCOL_STOP;

	ret = acpm_send_data(exynos_acpm->plugin[0].np, ACPM_IPC_PROTOCOL_STOP, &config);

	kfree(config.cmd);

	if (ret)
		pr_err("[ACPM] acpm enter wfi fail!!\n");
}

void acpm_ramdump(void)
{
#ifdef CONFIG_EXYNOS_SNAPSHOT_ACPM
	if (debug->dump_size)
		memcpy(debug->dump_dram_base, debug->dump_base, debug->dump_size);
#endif
}

static int exynos_acpm_reboot_notifier_call(struct notifier_block *this,
				   unsigned long code, void *_cmd)
{
	acpm_enter_wfi();
	exynos_apm_power_down();
	acpm_log_print();
	acpm_ramdump();
	return NOTIFY_DONE;
}

static struct notifier_block exynos_acpm_reboot_notifier = {
	.notifier_call = exynos_acpm_reboot_notifier_call,
	.priority = (INT_MIN + 1),
};

static void log_buffer_init(struct device *dev, struct device_node *node)
{
	const __be32 *prop;
	unsigned int base = 0;
	unsigned int time_len = 0;
	unsigned int buff_size = 0;
	unsigned int buff_len = 0;
	unsigned int len = 0;
	unsigned int dump_base = 0;
	unsigned int dump_size = 0;

	prop = of_get_property(node, "log_base", &len);
	if (prop)
		base = be32_to_cpup(prop);

	prop = of_get_property(node, "time_len", &len);
	if (prop)
		time_len = be32_to_cpup(prop);

	prop = of_get_property(node, "log_buff_size", &len);
	if (prop)
		buff_size = be32_to_cpup(prop);

	prop = of_get_property(node, "log_buff_len", &len);
	if (prop)
		buff_len = be32_to_cpup(prop);

	if (base && time_len && buff_size && buff_len) {
		debug = devm_kzalloc(dev, sizeof(struct acpm_debug_info), GFP_KERNEL);

		debug->time_index = ioremap(base,
				(buff_len * buff_size) + SZ_16);
		debug->time_len = time_len;
		debug->timestamps = devm_kzalloc(dev,
				sizeof(unsigned long long) * time_len, GFP_KERNEL);

		debug->log_buff_rear = debug->time_index + SZ_8;
		debug->log_buff_front = debug->log_buff_rear + SZ_4;
		debug->log_buff_base = debug->log_buff_front + SZ_4;
		debug->log_buff_len = buff_len;
		debug->log_buff_size = buff_size;

		prop = of_get_property(node, "debug_logging_level", &len);
		if (prop)
			debug->debug_logging_level = be32_to_cpup(prop);
	}

	prop = of_get_property(node, "dump_base", &len);
	if (prop)
		dump_base = be32_to_cpup(prop);

	prop = of_get_property(node, "dump_size", &len);
	if (prop)
		dump_size = be32_to_cpup(prop);

	if (debug && base && buff_size) {
		debug->dump_base = ioremap(dump_base, dump_size);
		debug->dump_size = dump_size;
	}

	prop = of_get_property(node, "logging_period", &len);
	if (prop)
		debug->period = be32_to_cpup(prop);

#ifdef CONFIG_EXYNOS_SNAPSHOT_ACPM
	debug->dump_dram_base = kzalloc(debug->dump_size, GFP_KERNEL);
	exynos_ss_printk("[ACPM] acpm framework SRAM dump to dram base: 0x%x\n",
			virt_to_phys(debug->dump_dram_base));
#endif

	spin_lock_init(&debug->lock);
}

static int acpm_send_data(struct device_node *node, unsigned int check_id,
		struct ipc_config *config)
{
	unsigned int channel_num, size;
	int ret = 0;
	int timeout_flag;

	if (!acpm_ipc_request_channel(node, acpm_framework_handler,
				&channel_num, &size)) {
		ipc_done = -1;

		ipc_time_start = sched_clock();
		ret = acpm_ipc_send_data(channel_num, config);

		/* Responce interrupt waitting */
		UNTIL_EQUAL(ipc_done, check_id, timeout_flag);

		if (timeout_flag)
			ret = -ETIMEDOUT;

		acpm_ipc_release_channel(channel_num);
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

	acpm->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	acpm->mcore_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(acpm->mcore_base))
		return PTR_ERR(acpm->mcore_base);

	log_buffer_init(&pdev->dev, node);

	node = of_get_child_by_name(node, "plugins");

	if (node) {
		ret = plugins_init(node, acpm);
		acpm_debugfs_init(acpm);
	};

	if (debug->period) {
		debug_logging_wq = create_freezable_workqueue("acpm_debug_logging");
		INIT_DELAYED_WORK(&debug->work, acpm_debug_logging);

		queue_delayed_work(debug_logging_wq, &debug->work,
				msecs_to_jiffies(10000));
	}

	exynos_acpm = acpm;

	register_reboot_notifier(&exynos_acpm_reboot_notifier);

	return ret;
}

static int acpm_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id acpm_match[] = {
	{ .compatible = "samsung,exynos-acpm" },
	{},
};

static struct platform_driver samsung_acpm_driver = {
	.probe	= acpm_probe,
	.remove	= acpm_remove,
	.driver	= {
		.name = "exynos-acpm",
		.owner	= THIS_MODULE,
		.of_match_table	= acpm_match,
	},
};

static int __init exynos_acpm_init(void)
{
	return platform_driver_register(&samsung_acpm_driver);
}
fs_initcall_sync(exynos_acpm_init);
