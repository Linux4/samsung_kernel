/*
 * sec_pm_qos_monitor.c
 *
 *  Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *  Minsung Kim <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/ktime.h>
#include <linux/suspend.h>
#include <linux/spinlock.h>
#include <linux/sec_pm_qos_monitor.h>

#define DEFAULT_SAMPLING_RATE_MS		(60 * 1000)
#define DEFAULT_DETECTION_THRESHOLD_MS		(10 * 1000)

struct pm_qos_monitor_data {
	char		enable_monitor;
	unsigned int	min_freq;
};

struct sec_pm_qos_monitor_info {
	struct device			*dev;
	struct pm_qos_monitor_data	md[EXYNOS_PM_QOS_NUM_CLASSES];
	struct delayed_work		monitor_work;
	struct notifier_block		pm_nb;
	unsigned int			sampling_rate_ms;
	unsigned int			detection_threshold_ms;
};

static struct sec_pm_qos_object *pm_qos_array;

static inline void show_pm_qos_request_detail(struct sec_pm_qos_object *obj,
			struct exynos_pm_qos_request *req, s64 active_time_ms)
{
	pr_info("sec_pm_qos: %s: %d: (%s:%d) (%lld ms)\n", obj->name,
			(req->node).prio, req->func, req->line, active_time_ms);
}

static void check_each_pm_qos_class(struct sec_pm_qos_monitor_info *info,
				int pm_qos_class)
{
	struct sec_pm_qos_object *obj;
	struct exynos_pm_qos_constraints *c;
	struct exynos_pm_qos_request *req;
	unsigned long flags;
	ktime_t time;
	s64 active_time_ms;

	if (pm_qos_class >= EXYNOS_PM_QOS_NUM_CLASSES) {
		dev_err(info->dev, "%s: invalid pm_qos_class(%d)\n", __func__,
				pm_qos_class);
		return;
	}

	obj = &pm_qos_array[pm_qos_class];
	c = obj->constraints;
	if (IS_ERR_OR_NULL(c)) {
		dev_err(info->dev, "%s: Bad constraints on qos?\n", __func__);
		return;
	}

	spin_lock_irqsave(&c->lock, flags);
	if (plist_head_empty(&c->list))
		goto out;

	time = ktime_get();

	plist_for_each_entry(req, &c->list, node) {
		if ((req->node).prio > info->md[pm_qos_class].min_freq) {
			active_time_ms = ktime_ms_delta(time, req->time);
			if (active_time_ms > info->detection_threshold_ms)
				show_pm_qos_request_detail(obj, req, active_time_ms);
		}
	}
out:
	spin_unlock_irqrestore(&c->lock, flags);
}

static void check_pm_qos_requests(struct sec_pm_qos_monitor_info *info)
{
	int i;

	for (i = 0; i < EXYNOS_PM_QOS_NUM_CLASSES; i++) {
		if (!info->md[i].enable_monitor)
			continue;

		check_each_pm_qos_class(info, i);
	}
}

static void sec_pm_qos_monitor_work(struct work_struct *work)
{
	struct sec_pm_qos_monitor_info *info =
				container_of(to_delayed_work(work),
				struct sec_pm_qos_monitor_info, monitor_work);

	if (!pm_qos_array) {
		dev_warn(info->dev, "%s: Stop monitor work!\n", __func__);
		return;
	}

	check_pm_qos_requests(info);

	schedule_delayed_work(&info->monitor_work,
			msecs_to_jiffies(info->sampling_rate_ms));
}

static int sec_pm_qos_pm_event(struct notifier_block *nb,
				unsigned long pm_event, void *unused)
{
	struct sec_pm_qos_monitor_info *info = container_of(nb,
					struct sec_pm_qos_monitor_info, pm_nb);

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		cancel_delayed_work_sync(&info->monitor_work);
		break;
	case PM_POST_SUSPEND:
		schedule_delayed_work(&info->monitor_work,
				msecs_to_jiffies(info->sampling_rate_ms));
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

#define show_one(file_name, object)					\
static ssize_t file_name##_show(struct device *dev,			\
		struct device_attribute *attr, char *buf)		\
{									\
	struct sec_pm_qos_monitor_info *info = dev_get_drvdata(dev);	\
	return sprintf(buf, "%u\n", info->object);			\
}

show_one(sampling_rate_ms, sampling_rate_ms);
show_one(detection_threshold_ms, detection_threshold_ms);

#define store_one(file_name, object)					\
static ssize_t file_name##_store(struct device *dev,			\
	struct device_attribute *attr, const char *buf,	size_t count)	\
{									\
	struct sec_pm_qos_monitor_info *info = dev_get_drvdata(dev);	\
	unsigned int val;						\
	int ret;							\
									\
	ret = sscanf(buf, "%u", &val);					\
	if (ret != 1)							\
		return -EINVAL;						\
									\
	info->object = val;						\
	return count;							\
}

store_one(sampling_rate_ms, sampling_rate_ms);
store_one(detection_threshold_ms, detection_threshold_ms);

static ssize_t monitoring_pm_qos_class_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sec_pm_qos_monitor_info *info = dev_get_drvdata(dev);
	unsigned int i, size = 0;

	if (!pm_qos_array)
		return 0;

	for (i = 0; i < EXYNOS_PM_QOS_NUM_CLASSES; i++) {
		if (info->md[i].enable_monitor)
			size += snprintf(buf + size, PAGE_SIZE, "%s(%u): %d\n",
					pm_qos_array[i].name, i,
					info->md[i].min_freq);
	}

	return size;
}

static ssize_t monitoring_pm_qos_class_store(struct device *dev,
		struct device_attribute *attr, const char *buf,	size_t count)
{
	struct sec_pm_qos_monitor_info *info = dev_get_drvdata(dev);
	unsigned int class, freq;
	int ret;

	ret = sscanf(buf, "%u %u", &class, &freq);
	if (ret != 2 || class >= EXYNOS_PM_QOS_NUM_CLASSES)
		return -EINVAL;

	info->md[class].enable_monitor = 1;
	info->md[class].min_freq = freq;

	return count;
}

static DEVICE_ATTR_RW(sampling_rate_ms);
static DEVICE_ATTR_RW(detection_threshold_ms);
static DEVICE_ATTR_RW(monitoring_pm_qos_class);

static struct attribute *sec_pm_qos_attrs[] = {
	&dev_attr_sampling_rate_ms.attr,
	&dev_attr_detection_threshold_ms.attr,
	&dev_attr_monitoring_pm_qos_class.attr,
	NULL
};
ATTRIBUTE_GROUPS(sec_pm_qos);

static void set_pm_qos_monitor_data(struct sec_pm_qos_monitor_info *info,
				unsigned int *raw_table, int size)
{
	unsigned int class, min_freq;
	int i;

	for (i = 0; i < size; i += 2) {
		class = raw_table[i];
		min_freq = raw_table[i+1];

		if (class >= EXYNOS_PM_QOS_NUM_CLASSES)
			continue;

		info->md[class].enable_monitor = 1;
		info->md[class].min_freq = min_freq;
	}
}

static int sec_pm_qos_parse_dt(struct platform_device *pdev)
{
	struct sec_pm_qos_monitor_info *info = platform_get_drvdata(pdev);
	struct device_node *dn;
	unsigned int *raw_table;
	u32 val;
	int size;

	if (!info || !pdev->dev.of_node)
		return -ENODEV;

	dn = pdev->dev.of_node;

	if (!of_property_read_u32(dn, "sampling_rate_ms", &val))
		info->sampling_rate_ms = val;
	else
		info->sampling_rate_ms = DEFAULT_SAMPLING_RATE_MS;

	if (!of_property_read_u32(dn, "detection_threshold_ms", &val))
		info->detection_threshold_ms = val;
	else
		info->detection_threshold_ms = DEFAULT_DETECTION_THRESHOLD_MS;

	size = of_property_count_u32_elems(dn, "pm_qos_classes");
	if (size <= 0)
		return -EINVAL;

	raw_table = kcalloc(size, sizeof(u32), GFP_KERNEL);
	if (!raw_table)
		return -ENOMEM;

	if (of_property_read_u32_array(dn, "pm_qos_classes", raw_table, size)) {
		kfree(raw_table);
		return -EINVAL;
	}

	set_pm_qos_monitor_data(info, raw_table, size);

	kfree(raw_table);

	return 0;
}

void sec_pm_qos_monitor_init_data(struct sec_pm_qos_object *objs)
{
	pr_info("%s\n", __func__);
	pm_qos_array = objs;
}
EXPORT_SYMBOL_GPL(sec_pm_qos_monitor_init_data);

static int sec_pm_qos_monitor_probe(struct platform_device *pdev)
{
	struct sec_pm_qos_monitor_info *info;
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);
	info->dev = &pdev->dev;

	ret = sec_pm_qos_parse_dt(pdev);
	if (ret) {
		dev_err(info->dev, "%s: fail to parse dt(%d)\n", __func__, ret);
		return ret;
	}

	info->pm_nb.notifier_call = sec_pm_qos_pm_event;
	ret = register_pm_notifier(&info->pm_nb);
	if (ret) {
		dev_err(info->dev, "%s: failed to register PM notifier(%d)\n",
				__func__, ret);
		return ret;
	}

	ret = sysfs_create_groups(&pdev->dev.kobj, sec_pm_qos_groups);
	if (ret) {
		dev_err(info->dev, "%s: failed to create sysfs groups(%d)\n",
				__func__, ret);
		goto error;
	}

	INIT_DELAYED_WORK(&info->monitor_work, sec_pm_qos_monitor_work);
	schedule_delayed_work(&info->monitor_work,
			msecs_to_jiffies(info->sampling_rate_ms * 2));

	return 0;

error:
	unregister_pm_notifier(&info->pm_nb);
	return ret;
}

static int sec_pm_qos_monitor_remove(struct platform_device *pdev)
{
	struct sec_pm_qos_monitor_info *info = platform_get_drvdata(pdev);

	dev_info(info->dev, "%s\n", __func__);

	return 0;
}

static const struct of_device_id sec_pm_qos_monitor_match[] = {
	{ .compatible = "samsung,sec-pm-qos-monitor", },
	{ },
};
MODULE_DEVICE_TABLE(of, sec_pm_qos_monitor_match);

static struct platform_driver sec_pm_qos_monitor_driver = {
	.driver = {
		.name = "sec-pm-qos-monitor",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sec_pm_qos_monitor_match),
	},
	.probe = sec_pm_qos_monitor_probe,
	.remove = sec_pm_qos_monitor_remove,
};

module_platform_driver(sec_pm_qos_monitor_driver);

MODULE_AUTHOR("Minsung Kim <ms925.kim@samsung.com>");
MODULE_DESCRIPTION("SEC PM QoS Monitor Driver");
MODULE_LICENSE("GPL");
