/*
 * sec_pm_debug.c
 *
 *  Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/sec_class.h>
#include <linux/sec_pm_debug.h>
#include <linux/pm_qos.h>
#include <linux/sched/clock.h>

static DEFINE_SPINLOCK(pm_qos_lock);

struct sec_pm_debug_info {
	struct device		*dev;
	struct device		*sec_pm_dev;
	struct notifier_block	nb[PM_QOS_NUM_CLASSES];
};

u8 pmic_onsrc;
u8 pmic_offsrc;

unsigned long long sleep_time_sec;
unsigned int sleep_count;

static struct dentry *sec_pm_debugfs_root;

#define QOS_FUNC_NAME_LEN	32

struct __pm_qos_log {
	unsigned long long time;
	int pm_qos_class;
	int value;
	char func[QOS_FUNC_NAME_LEN];
	unsigned int line;
};

struct sec_pm_debug_log {
	struct __pm_qos_log pm_qos[SZ_4K];
};

struct sec_pm_debug_log_misc {
	atomic_t pm_qos_log_idx;
};

#define QOS_NUM_OF_EXCLUSIVE_CLASSES	2

static char pm_qos_exclusive_class[PM_QOS_NUM_CLASSES] = {
	[PM_QOS_DEVICE_THROUGHPUT] = 1,
	[PM_QOS_NETWORK_THROUGHPUT] = 1,
};

static struct sec_pm_debug_log *sec_pm_log;
static struct sec_pm_debug_log_misc sec_pm_log_misc;

static int sec_pm_debug_ocp_info_show(struct seq_file *m, void *v)
{
	seq_printf(m, "No OCP(ON:0x%02X OFF:0x%02X)\n", pmic_onsrc,
			pmic_offsrc);

	return 0;
}

static int ocp_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_pm_debug_ocp_info_show, inode->i_private);
}

const static struct file_operations ocp_info_fops = {
	.open		= ocp_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int sec_pm_debug_on_offsrc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "PWRONSRC:0x%02X OFFSRC:0x%02X\n", pmic_onsrc,
			pmic_offsrc);

	return 0;
}

static int on_offsrc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_pm_debug_on_offsrc_show, inode->i_private);
}

const static struct file_operations on_offsrc_fops = {
	.open		= on_offsrc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static ssize_t sleep_time_sec_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%llu\n", sleep_time_sec);
}

static ssize_t sleep_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", sleep_count);
}

static ssize_t pwr_on_off_src_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "ONSRC:0x%02X OFFSRC:0x%02X\n", pmic_onsrc,
			pmic_offsrc);
}

static DEVICE_ATTR_RO(sleep_time_sec);
static DEVICE_ATTR_RO(sleep_count);
static DEVICE_ATTR_RO(pwr_on_off_src);

static struct attribute *sec_pm_debug_attrs[] = {
	&dev_attr_sleep_time_sec.attr,
	&dev_attr_sleep_count.attr,
	&dev_attr_pwr_on_off_src.attr,
	NULL
};
ATTRIBUTE_GROUPS(sec_pm_debug);

static int sec_pm_debug_qos_notifier(struct notifier_block *nb,
					unsigned long val, void *v)
{
	struct pm_qos_request *req;
	unsigned long flags;
	unsigned long i;

	if (!v) {
		pr_err("%s: v is NULL!\n", __func__);
		return NOTIFY_OK;
	}

	req = container_of(v, struct pm_qos_request, pm_qos_class);

	i = atomic_inc_return(&sec_pm_log_misc.pm_qos_log_idx) &
		(ARRAY_SIZE(sec_pm_log->pm_qos) - 1);

	spin_lock_irqsave(&pm_qos_lock, flags);
	sec_pm_log->pm_qos[i].time = cpu_clock(raw_smp_processor_id());
	sec_pm_log->pm_qos[i].pm_qos_class = req->pm_qos_class;
	sec_pm_log->pm_qos[i].value = val;
	strncpy(sec_pm_log->pm_qos[i].func, req->func, QOS_FUNC_NAME_LEN - 1);
	sec_pm_log->pm_qos[i].line = req->line;
	spin_unlock_irqrestore(&pm_qos_lock, flags);

	return NOTIFY_OK;
}

static void sec_pm_debug_register_notifier(struct sec_pm_debug_info *info)
{
	int ret, i;

	dev_info(info->dev, "%s\n", __func__);

	for (i = PM_QOS_CPU_DMA_LATENCY; i < PM_QOS_NUM_CLASSES; i++) {
		if (pm_qos_exclusive_class[i])
			continue;

		info->nb[i].notifier_call = sec_pm_debug_qos_notifier;
		ret = pm_qos_add_notifier(i, &info->nb[i]);
		if (ret < 0)
			dev_err(info->dev, "%s: fail to add notifier(%d)\n",
					__func__, i);
	}
}

static void sec_pm_debug_unregister_notifier(struct sec_pm_debug_info *info)
{
	int ret, i;

	dev_info(info->dev, "%s\n", __func__);

	for (i = PM_QOS_CPU_DMA_LATENCY; i < PM_QOS_NUM_CLASSES; i++) {
		if (pm_qos_exclusive_class[i])
			continue;

		ret = pm_qos_remove_notifier(i, &info->nb[i]);
		if (ret < 0)
			dev_err(info->dev, "%s: fail to remove notifier(%d)\n",
					__func__, i);
	}
}

static int sec_pm_debug_qos_show(struct seq_file *m, void *unused)
{
	const char pm_qos_class_name[PM_QOS_NUM_CLASSES][24] = {
		"RESERVED", "CPU_DMA_LATENCY",
		"NETWORK_LATENCY", "CLUSTER0_FREQ_MIN",
		"CLUSTER0_FREQ_MAX", "CLUSTER1_FREQ_MIN",
		"CLUSTER1_FREQ_MAX", "CLUSTER2_FREQ_MIN",
		"CLUSTER2_FREQ_MAX", "CPU_ONLINE_MIN",
		"CPU_ONLINE_MAX", "DEVICE_THROUGHPUT",
		"INTCAM_THROUGHPUT", "DEVICE_THROUGHPUT_MAX",
		"INTCAM_THROUGHPUT_MAX", "BUS_THROUGHPUT",
		"BUS_THROUGHPUT_MAX", "NETWORK_THROUGHPUT",
		"MEMORY_BANDWIDTH", "DISPLAY_THROUGHPUT",
		"DISPLAY_THROUGHPUT_MAX", "CAM_THROUGHPUT",
		"AUD_THROUGHPUT", "DSP_THROUGHPUT",
		"DNC_THROUGHPUT", "FSYS0_THROUGHPUT",
		"CAM_THROUGHPUT_MAX", "AUD_THROUGHPUT_MAX",
		"DSP_THROUGHPUT_MAX", "DNC_THROUGHPUT_MAX",
		"FSYS0_THROUGHPUT_MAX", "MFC_THROUGHPUT",
		"NPU_THROUGHPUT", "MFC_THROUGHPUT_MAX",
		"NPU_THROUGHPUT_MAX", "TNR_THROUGHPUT",
		"TNR_THROUGHPUT_MAX", "GPU_THROUGHPUT_MIN",
		"GPU_THROUGHPUT_MAX",
	};
	struct sec_pm_debug_info *info = m->private;
	unsigned long idx, size;
	unsigned long flags;
	int i;

	dev_info(info->dev, "%s\n", __func__);

	seq_puts(m, "time\t\t\tclass\t\t\t\tfrequency\tfunction\n");

	size = ARRAY_SIZE(sec_pm_log->pm_qos);
	idx = atomic_read(&sec_pm_log_misc.pm_qos_log_idx);

	for (i = 0; i < size; i++, idx--) {
		struct __pm_qos_log pm_qos;
		const char *class;
		unsigned long rem_nsec;

		idx &= size - 1;

		spin_lock_irqsave(&pm_qos_lock, flags);

		pm_qos.time = sec_pm_log->pm_qos[idx].time;
		if (!pm_qos.time) {
			spin_unlock_irqrestore(&pm_qos_lock, flags);
			break;
		}

		pm_qos.pm_qos_class = sec_pm_log->pm_qos[idx].pm_qos_class;
		pm_qos.value = sec_pm_log->pm_qos[idx].value,

		strncpy(pm_qos.func, sec_pm_log->pm_qos[idx].func,
				QOS_FUNC_NAME_LEN);
		pm_qos.line = sec_pm_log->pm_qos[idx].line;

		spin_unlock_irqrestore(&pm_qos_lock, flags);

		rem_nsec = do_div(pm_qos.time, NSEC_PER_SEC);

		if (pm_qos.pm_qos_class < PM_QOS_NUM_CLASSES)
			class = pm_qos_class_name[pm_qos.pm_qos_class];
		else
			class = pm_qos_class_name[0];

		seq_printf(m, "[%8lu.%06lu]\t%-24s\t%-8d\t%s:%u\n",
				(unsigned long)pm_qos.time,
				rem_nsec / NSEC_PER_USEC, class,
				pm_qos.value, pm_qos.func, pm_qos.line);
	}

	return 0;
}

static int sec_pm_debug_qos_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_pm_debug_qos_show, inode->i_private);
}

static const struct file_operations pm_qos_fops = {
	.owner = THIS_MODULE,
	.open = sec_pm_debug_qos_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void sec_pm_debug_debugfs_init(struct sec_pm_debug_info *info)
{
	debugfs_create_file("ocp_info", 0444, NULL, NULL, &ocp_info_fops);
	debugfs_create_file("pwr_on_offsrc", 0444, NULL, NULL, &on_offsrc_fops);

	sec_pm_debugfs_root = debugfs_create_dir("sec_pm", NULL);
	if (!sec_pm_debugfs_root) {
		dev_err(info->dev, "%s: Failed to create debugfs root\n",
				__func__);
		return;
	}

	debugfs_create_file("pm_qos", 0444, sec_pm_debugfs_root, info,
			&pm_qos_fops);
}

void __init sec_pm_debug_init_log_idx(void)
{
	atomic_set(&(sec_pm_log_misc.pm_qos_log_idx), -1);
}

static int sec_pm_debug_probe(struct platform_device *pdev)
{
	struct sec_pm_debug_info *info;
	struct device *sec_pm_dev;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info) {
		dev_err(&pdev->dev, "%s: Fail to alloc info\n", __func__);
		return -ENOMEM;
	}

	sec_pm_log = devm_kzalloc(&pdev->dev, sizeof(*sec_pm_log), GFP_KERNEL);
	if (!sec_pm_log) {
		dev_err(&pdev->dev, "%s: Fail to alloc sec_pm_log\n", __func__);
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;

	sec_pm_dev = sec_device_create(info, "pm");

	if (IS_ERR(sec_pm_dev)) {
		dev_err(info->dev, "%s: fail to create sec_pm_dev\n", __func__);
		return PTR_ERR(sec_pm_dev);
	}

	info->sec_pm_dev = sec_pm_dev;

	ret = sysfs_create_groups(&sec_pm_dev->kobj, sec_pm_debug_groups);
	if (ret) {
		dev_err(info->dev, "%s: failed to create sysfs groups(%d)\n",
				__func__, ret);
		goto err_create_sysfs;
	}

	sec_pm_debug_debugfs_init(info);
	sec_pm_debug_init_log_idx();
	sec_pm_debug_register_notifier(info);

	return 0;

err_create_sysfs:
	sec_device_destroy(sec_pm_dev->devt);

	return ret;
}

static int sec_pm_debug_remove(struct platform_device *pdev)
{
	struct sec_pm_debug_info *info = platform_get_drvdata(pdev);

	sec_pm_debug_unregister_notifier(info);
	return 0;
}

static const struct of_device_id sec_pm_debug_match[] = {
	{ .compatible = "samsung,sec-pm-debug", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_pm_debug_match);

static struct platform_driver sec_pm_debug_driver = {
	.driver = {
		.name = "sec-pm-debug",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_debug_match),
	},
	.probe = sec_pm_debug_probe,
	.remove = sec_pm_debug_remove,
};

static int __init sec_pm_debug_init(void)
{
	return platform_driver_register(&sec_pm_debug_driver);
}
late_initcall(sec_pm_debug_init);

static void __exit sec_pm_debug_exit(void)
{
	platform_driver_unregister(&sec_pm_debug_driver);
}
module_exit(sec_pm_debug_exit);

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("SEC PM Debug Driver");
MODULE_LICENSE("GPL");
