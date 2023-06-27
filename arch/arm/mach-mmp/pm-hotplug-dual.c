/*
 * linux/arch/arm/mach-mmp/pm-hotplug-dual.c
 *
 * Based on S5PV310 - Dual core dynamic CPU hotpluging
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/ktime.h>
#include <linux/tick.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/suspend.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/cpufreq.h>

#define CHECK_DELAY	(.5*HZ)
#define TRANS_LOAD_L	20
#define TRANS_LOAD_H	(TRANS_LOAD_L*3)

static struct workqueue_struct *hotplug_wq;
static struct delayed_work hotplug_work;

static unsigned int hotpluging_rate = CHECK_DELAY;
static unsigned int hp_lock;
static unsigned int trans_load_l = TRANS_LOAD_L;
static unsigned int trans_load_h = TRANS_LOAD_H;
static unsigned int bound_freq = 400 * 1000;

struct cpu_time_info {
	u64 prev_cpu_idle;
	u64 prev_cpu_wall;
	unsigned int load;
};

static DEFINE_PER_CPU(struct cpu_time_info, hotplug_cpu_time);

/* mutex can be used since hotplug_timer does not run in
   timer(softirq) context but in process context */
static DEFINE_MUTEX(hotplug_lock);

static void __ref hotplug_timer(struct work_struct *work)
{
	unsigned int i, avg_load = 0, load = 0;
	unsigned int cur_freq;

	mutex_lock(&hotplug_lock);

	if (hp_lock == 1)
		goto unlock;

	for_each_online_cpu(i) {
		struct cpu_time_info *tmp_info;
		cputime64_t cur_wall_time, cur_idle_time;
		unsigned int idle_time, wall_time;

		tmp_info = &per_cpu(hotplug_cpu_time, i);

		cur_idle_time = get_cpu_idle_time_us(i, &cur_wall_time);

		idle_time = (unsigned int)(cur_idle_time -
				tmp_info->prev_cpu_idle);
		tmp_info->prev_cpu_idle = cur_idle_time;

		wall_time = (unsigned int)(cur_wall_time -
				tmp_info->prev_cpu_wall);
		tmp_info->prev_cpu_wall = cur_wall_time;

		if (wall_time < idle_time)
			goto no_hotplug;

		tmp_info->load = 100 * (wall_time - idle_time) / wall_time;

		load += tmp_info->load;
	}

	avg_load = load / num_online_cpus();

	cur_freq = cpufreq_get(0);
	if (((avg_load < trans_load_l) && (cur_freq <= bound_freq)) &&
	    (cpu_online(1) == 1)) {
		cpu_down(1);
		hotpluging_rate = CHECK_DELAY;
	} else if (((avg_load > trans_load_h) && (cur_freq >= bound_freq)) &&
		   (cpu_online(1) == 0)) {
		cpu_up(1);
		hotpluging_rate = CHECK_DELAY * 4;
	}

no_hotplug:
	queue_delayed_work_on(0, hotplug_wq, &hotplug_work, hotpluging_rate);

unlock:
	mutex_unlock(&hotplug_lock);
}

static int mmp_pm_hotplug_notifier_event(struct notifier_block *this,
					     unsigned long event, void *ptr)
{
	static unsigned hp_lock_saved;

	switch (event) {
	case PM_SUSPEND_PREPARE:
		mutex_lock(&hotplug_lock);
		hp_lock_saved = hp_lock;
		hp_lock = 1;
		pr_info("%s: pm suspend saving pm_hotplug lock %x\n",
			__func__, hp_lock_saved);
		mutex_unlock(&hotplug_lock);
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&hotplug_lock);
		pr_info("%s: pm resume restoring pm_hotplug lock %x\n",
			__func__, hp_lock_saved);
		hp_lock = hp_lock_saved;
		mutex_unlock(&hotplug_lock);
		/*
		 * corner case:
		 * hp_lock set to 1 during suspend, and work_queue may goto
		 * "unlock", then work_queue never have chance to run again
		 */
		if (0 == hp_lock) {
			flush_delayed_work(&hotplug_work);
			queue_delayed_work_on(0, hotplug_wq, &hotplug_work,
				CHECK_DELAY);
		}
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mmp_pm_hotplug_notifier = {
	.notifier_call = mmp_pm_hotplug_notifier_event,
};

static int hotplug_reboot_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	mutex_lock(&hotplug_lock);
	pr_err("%s: disabling pm hotplug\n", __func__);
	hp_lock = 1;
	mutex_unlock(&hotplug_lock);

	return NOTIFY_DONE;
}

static struct notifier_block hotplug_reboot_notifier = {
	.notifier_call = hotplug_reboot_notifier_call,
};

static struct kobject hotplug_kobj;

static long parse_arg(const char *buf, size_t count)
{
	char *token, *s;
	char str[128];
	int err;
	long lock_tmp;

	memcpy(str, buf, count);
	s = str;
	token = strsep(&s, " \t\n");
	err = kstrtol(token, 10, &lock_tmp);
	if (err)
		return 0;
	return lock_tmp;
}

static int loadh_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", trans_load_h);
}
static int loadh_set(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int loadh_tmp;
	if (!sscanf(buf, "%d", &loadh_tmp))
		return -EINVAL;
	trans_load_h = loadh_tmp;
	return count;

}
static DEVICE_ATTR(loadh, S_IRUGO | S_IWUSR, loadh_get, loadh_set);

static int loadl_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", trans_load_l);
}
static int loadl_set(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int loadl_tmp;
	if (!sscanf(buf, "%d", &loadl_tmp))
		return -EINVAL;
	trans_load_l = loadl_tmp;
	return count;
}
static DEVICE_ATTR(loadl, S_IRUGO | S_IWUSR, loadl_get, loadl_set);

static int bound_freq_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", bound_freq);
}
static int bound_freq_set(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	int freq_tmp;
	if (!sscanf(buf, "%d", &freq_tmp))
		return -EINVAL;
	bound_freq = freq_tmp;
	return count;
}
static DEVICE_ATTR(bound_freq, S_IRUGO | S_IWUSR, bound_freq_get,
		bound_freq_set);


static int lock_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", hp_lock);
}

static int lock_set(struct device *dev, struct device_attribute *attr,
			const char *buf, size_t count)
{
	u32 val;
	int restart_hp = 0;

	val = parse_arg(buf, count);

	mutex_lock(&hotplug_lock);
	/* we want to re-enable governor */
	if ((1 == hp_lock) && (0 == val))
		restart_hp = 1;
	hp_lock = val ? 1 : 0;
	mutex_unlock(&hotplug_lock);

	if (restart_hp) {
		flush_delayed_work(&hotplug_work);
		queue_delayed_work_on(0, hotplug_wq, &hotplug_work,
				CHECK_DELAY);
	}
	return count;
}
static DEVICE_ATTR(lock, S_IRUGO | S_IWUSR, lock_get, lock_set);

static struct attribute *hotplug_attributes[] = {
	&dev_attr_lock.attr,
	&dev_attr_loadh.attr,
	&dev_attr_loadl.attr,
	&dev_attr_bound_freq.attr,
	NULL,
};

static struct kobj_type hotplug_dir_ktype = {
	.sysfs_ops	= &kobj_sysfs_ops,
	.default_attrs	= hotplug_attributes,
};

static int __init mmp_pm_hotplug_init(void)
{
	pr_info("mmp PM-hotplug init function\n");
	hotplug_wq = create_singlethread_workqueue("dynamic hotplug");
	if (!hotplug_wq) {
		pr_err("Creation of hotplug work failed\n");
		return -EFAULT;
	}

	INIT_DELAYED_WORK_DEFERRABLE(&hotplug_work, hotplug_timer);

	queue_delayed_work_on(0, hotplug_wq, &hotplug_work, 60 * HZ);

	register_pm_notifier(&mmp_pm_hotplug_notifier);
	register_reboot_notifier(&hotplug_reboot_notifier);

	if (kobject_init_and_add(&hotplug_kobj, &hotplug_dir_ktype,
				&(cpu_subsys.dev_root->kobj), "hotplug"))
		pr_err("%s: Failed to add kobject for hotplug\n", __func__);

	return 0;
}

late_initcall(mmp_pm_hotplug_init);
