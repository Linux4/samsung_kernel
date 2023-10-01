/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * CPU Part
 *
 * CPU Hotplug driver for Exynos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/fb.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/debug-snapshot.h>

#include <soc/samsung/exynos-cpuhp.h>

#define CPUHP_USER_NAME_LEN	16

struct cpuhp_user {
	struct list_head	list;
	char name[CPUHP_USER_NAME_LEN];
	struct cpumask		online_cpus;
	int			type;
};

static struct {
	/* Control cpu hotplug operation */
	bool			enabled;

	/* flag for suspend */
	bool			suspended;

	/* list head for requester */
	struct list_head	users;

	/* user for system */
	struct cpuhp_user	system_user;
	/* user for sysfs */
	struct cpuhp_user	sysfs_user;

	/* Synchronizes accesses to refcount and cpumask */
	struct mutex		lock;

	/* user request mask */
	struct cpumask		online_cpus;

	/* cpuhp kobject */
	struct kobject		*kobj;
} cpuhp = {
	.lock = __MUTEX_INITIALIZER(cpuhp.lock),
};

/**********************************************************************************/
/*				   Helper					  */
/**********************************************************************************/
static int cpuhp_do(void);

/*
 * Update pm_suspend status.
 * During suspend-resume, cpuhp driver is stop
 */
static inline void cpuhp_suspend(bool enable)
{
	/* This lock guarantees completion of cpuhp_do() */
	cpuhp.suspended = enable;
}

/*
 * Update cpuhp enablestatus.
 * cpuhp driver is working when enabled big is TRUE
 */
static inline void cpuhp_enable(bool enable)
{
	cpuhp.enabled = enable;
}

/* find user matched name. if return NULL, there is no user matched name */
static struct cpuhp_user *cpuhp_find_user(char *name)
{
	struct cpuhp_user *user;

	list_for_each_entry(user, &cpuhp.users, list)
		if (!strcmp(user->name, name))
			return user;
	return NULL;
}

/* update user's requesting  cpu mask */
static int cpuhp_update_user(char *name, struct cpumask mask, int type)
{
	struct cpuhp_user *user = cpuhp_find_user(name);

	if (!user)
		return -EINVAL;

	cpumask_copy(&user->online_cpus, &mask);
	user->type = type;

	return 0;
}

/* remove user from hotplug requesting user list */
int exynos_cpuhp_unregister(char *name, struct cpumask mask, int type)
{
	return 0;
}

/*
 * Register cpu-hp user
 * Users and IPs that want to use cpu-hp should register through this function.
 *	name: Must have a unique value, and panic will occur if you use an already
 *	      registered name.
 *	mask: The cpu mask that user wants to ONLINE and cpu OFF bits has more HIGH
 *	      priority than ONLINE bit. This mask is default cpu mask at registration
 *	      and it is reflected immediately after registration.
 *	type: cpu-hp type (0-> legacy cpu hp, 0xFA57-> fast cpu hp)
 */
int exynos_cpuhp_register(char *name, struct cpumask mask, int type)
{
	int ret;
	struct cpuhp_user *user;
	char buf[10];

	mutex_lock(&cpuhp.lock);

	/* check wether name is already register or not */
	if (cpuhp_find_user(name))
		panic("CPUHP: Failed to register cpuhp! this name already existed\n");

	/* allocate memory for new user */
	user = kzalloc(sizeof(struct cpuhp_user), GFP_KERNEL);
	if (!user) {
		mutex_unlock(&cpuhp.lock);
		return -ENOMEM;
	}

	/* init new user's information */
	cpumask_copy(&user->online_cpus, &mask);
	strcpy(user->name, name);
	user->type = type;
	/* register user list */
	list_add(&user->list, &cpuhp.users);

	scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&user->online_cpus));
	pr_info("CPUHP: reigstered new user(name:%s, mask:%s)\n", user->name, buf);

	/* applying new user's request */
	ret = cpuhp_do();

	mutex_unlock(&cpuhp.lock);

	return ret;
}

/*
 * User requests cpu-hp.
 * The mask contains the requested cpu mask, and the type is hp operaton type.
 * The INTERSECTIONS of other user's request masks is determined by the final cpu-mask.
 */
int exynos_cpuhp_request(char *name, struct cpumask mask, int type)
{
	int ret;

	mutex_lock(&cpuhp.lock);

	if (cpuhp_update_user(name, mask, type)) {
		mutex_unlock(&cpuhp.lock);
		return 0;
	}

	ret = cpuhp_do();

	mutex_unlock(&cpuhp.lock);

	return ret;
}

/**********************************************************************************/
/*				 cpu hp operater				  */
/**********************************************************************************/
/*
 * Return last target online cpu mask
 * Returns the cpu_mask INTERSECTIONS of all users in the user list.
 */
static struct cpumask cpuhp_get_online_cpus(void)
{
	struct cpumask mask;
	struct cpuhp_user *user;
	char buf[10];

	cpumask_setall(&mask);

	list_for_each_entry(user, &cpuhp.users, list)
		cpumask_and(&mask, &mask, &user->online_cpus);

	if (cpumask_empty(&mask) || !cpumask_test_cpu(0, &mask)) {
		scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&mask));
		panic("CPUHP: Online mask(%s) is wrong\n", buf);
	}

	return mask;
}

/*
 * Executes cpu_up
 * Run cpu_up according to the cpu control operation type.
 */
static int cpuhp_cpu_up(struct cpumask enable_cpus)
{
	int cpu, ret = 0;

	for_each_cpu(cpu, &enable_cpus) {
		if (cpumask_test_cpu(cpu, cpu_online_mask))
			continue;

		ret = cpu_up(cpu);
		if (ret) {
			/*
			 * If it fails to enable cpu,
			 * it cancels cpu hotplug request and retries later.
			 */
			pr_err("%s: Failed to hotplug in CPU%d with error %d\n",
								__func__, cpu, ret);
			break;
		}
	}

	return ret;
}

/*
 * Executes cpu_down
 * Run cpu_up according to the cpu control operation type.
 */
static int cpuhp_cpu_down(struct cpumask disable_cpus)
{
	int cpu, ret = 0;

	/*
	 * Reverse order of cpu,
	 * explore cpu7, cpu6, cpu5, ... cpu1
	 */
	for (cpu = nr_cpu_ids - 1; cpu > 0; cpu--) {
		if (!cpumask_test_cpu(cpu, &disable_cpus))
			continue;

		if (!cpumask_test_cpu(cpu, cpu_online_mask))
			continue;

		ret = cpu_down(cpu);
		if (ret)
			pr_err("%s: Failed to hotplug out CPU%d with error %d\n",
								__func__, cpu, ret);
	}

	return ret;
}

/* print cpu control informatoin for deubgging */
static void cpuhp_print_debug_info(struct cpumask online_cpus)
{
	char new_buf[10], pre_buf[10];

	scnprintf(pre_buf, sizeof(pre_buf), "%*pbl", cpumask_pr_args(cpu_online_mask));
	scnprintf(new_buf, sizeof(new_buf), "%*pbl", cpumask_pr_args(&online_cpus));

	dbg_snapshot_printk("CPUHP: %s -> %s\n", pre_buf, new_buf);
	pr_info("CPUHP: %s -> %s\n", pre_buf, new_buf);
}

/*
 * cpuhp_do() is the main function for cpu hotplug. Only this function
 * enables or disables cpus, so all APIs in this driver call cpuhp_do()
 * eventually.
 */
static int cpuhp_do(void)
{
	int ret = 0;
	struct cpumask online_cpus, enable_cpus, disable_cpus;

	/*
	 * If cpu hotplug is disabled or suspended,
	 * cpuhp_do() do nothing.
	 */
	if (!cpuhp.enabled || cpuhp.suspended)
		return 0;

	if (!cpumask_equal(&cpuhp.online_cpus, cpu_online_mask)) {
		char new_buf[10], pre_buf[10];
		scnprintf(pre_buf, sizeof(pre_buf), "%*pbl", cpumask_pr_args(&cpuhp.online_cpus));
		scnprintf(new_buf, sizeof(new_buf), "%*pbl", cpumask_pr_args(cpu_online_mask));
		pr_warn("CPUHP: Somebody did cpu-hotplug(target:%s, cur:%s\n", pre_buf, new_buf);
	}

	online_cpus = cpuhp_get_online_cpus();

	if (cpumask_equal(&online_cpus, cpu_online_mask))
		return 0;

	cpuhp_print_debug_info(online_cpus);

	/* get the enable cpus mask for new online cpu */
	cpumask_andnot(&enable_cpus, &online_cpus, cpu_online_mask);
	/* get the disable cpus mask for new offline cpu */
	cpumask_andnot(&disable_cpus, cpu_online_mask, &online_cpus);

	if (!cpumask_empty(&enable_cpus))
		ret = cpuhp_cpu_up(enable_cpus);

	if (!cpumask_empty(&disable_cpus))
		ret = cpuhp_cpu_down(disable_cpus);

	cpumask_copy(&cpuhp.online_cpus, &online_cpus);

	return ret;
}

static int cpuhp_control(bool enable)
{
	struct cpumask mask;
	int ret = 0;

	mutex_lock(&cpuhp.lock);
	if (enable) {
		cpuhp_enable(true);
		cpuhp_do();
	} else {
		cpumask_setall(&mask);
		cpumask_andnot(&mask, &mask, cpu_online_mask);

		/*
		 * If it success to enable all CPUs, clear cpuhp.enabled flag.
		 * Since then all hotplug requests are ignored.
		 */
		ret = cpuhp_cpu_up(mask);
		if (!ret) {
			/*
			 * In this position, can't use cpuhp_enable()
			 * because already taken cpuhp.lock
			 */
			cpuhp.enabled = false;
		} else {
			pr_err("Fail to disable cpu hotplug, please try again\n");
		}
	}
	mutex_unlock(&cpuhp.lock);

	return ret;
}

/**********************************************************************************/
/*				        SYSFS					  */
/**********************************************************************************/

/*
 * User can change the number of online cpu by using min_online_cpu and
 * max_online_cpu sysfs node. User input minimum and maxinum online cpu
 * to this node as below:
 *
 * #echo mask > /sys/power/cpuhp/set_online_cpu
 */
#define STR_LEN 6
static inline toupper(char ch)
{
	if ('a' <= ch && ch <= 'z')
		ch += 'A' - 'a';

	return ch;
}

static ssize_t show_set_online_cpu(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int online_cpus;

	online_cpus = *(unsigned int *)cpumask_bits(&cpuhp.sysfs_user.online_cpus);
	return snprintf(buf, 30, "set online cpu : 0x%x\n", online_cpus);
}

#define BIT_BOOT_CPU	(1 << 0)
#define BIT_ALL_CPUS	((1 << NR_CPUS) - 1)
static ssize_t store_set_online_cpu(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	struct cpumask online_cpus;
	char str[STR_LEN], re_str[STR_LEN];
	unsigned int cpumask_value;
	int input;

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%5s", str))
		return -EINVAL;

	/* CPU0 must be on and input should be in range of nr_cpus */
	if (input <= 0 || input > BIT_ALL_CPUS)
		return -EINVAL;

	if (str[0] == '0' && toupper(str[1]) == 'X')
		/* Move str pointer to remove "0x" */
		cpumask_parse(str + 2, &online_cpus);
	else {
		if (kstrtoint(str, 0, &input))
			return -EINVAL;

		if (!(input & BIT_BOOT_CPU))
			return -EINVAL;

		if (!sscanf(str, "%d", &cpumask_value))
			return -EINVAL;

		snprintf(re_str, STR_LEN - 1, "%x", cpumask_value);
		cpumask_parse(re_str, &online_cpus);
	}

	cpumask_copy(&cpuhp.sysfs_user.online_cpus, &online_cpus);
	cpuhp_do();

	return count;
}

static struct kobj_attribute cpuhp_set_online_cpu =
__ATTR(set_online_cpu, 0644, show_set_online_cpu, store_set_online_cpu);

/*
 * It shows users information(name, requesting cpu_mask, type)
 * registered in cpu-hp user_list
 *
 * #cat /sys/power/cpuhp/users
 */
static ssize_t show_users(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int online_cpus;
	struct cpuhp_user *user;
	ssize_t ret = 0;

	list_for_each_entry(user, &cpuhp.users, list) {
		online_cpus = *(unsigned int *)cpumask_bits(&user->online_cpus);
		ret += scnprintf(&buf[ret], 30, "%s: (0x%x)\n", user->name, online_cpus);
	}

	return ret;
}

/*
 * User can control the cpu hotplug operation as below:
 *
 * #echo 1 > /sys/power/cpuhp/enabled => enable
 * #echo 0 > /sys/power/cpuhp/enabled => disable
 *
 * If enabled become 0, hotplug driver enable the all cpus and no hotplug
 * operation happen from hotplug driver.
 */

static ssize_t show_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", cpuhp.enabled);
}

static ssize_t store_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	cpuhp_control(!!input);

	return count;
}

static struct kobj_attribute cpuhp_enabled =
__ATTR(enabled, 0644, show_enable, store_enable);
static struct kobj_attribute cpuhp_users =
__ATTR(users, 0444, show_users, NULL);

static struct attribute *cpuhp_attrs[] = {
	&cpuhp_set_online_cpu.attr,
	&cpuhp_enabled.attr,
	&cpuhp_users.attr,
	NULL,
};

static const struct attribute_group cpuhp_group = {
	.attrs = cpuhp_attrs,
};

/**********************************************************************************/
/*				        PM_NOTI					  */
/**********************************************************************************/
static int exynos_cpuhp_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	mutex_lock(&cpuhp.lock);
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		cpuhp_suspend(true);
		break;
	case PM_POST_SUSPEND:
		cpuhp_suspend(false);
		cpuhp_do();
		break;
	}
	mutex_unlock(&cpuhp.lock);

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpuhp_nb = {
	.notifier_call = exynos_cpuhp_pm_notifier,
};

/**********************************************************************************/
/*                            CPUFREQ PM QOS HANDLER                              */
/**********************************************************************************/
static int exynos_cpuhp_pm_qos_callback(struct notifier_block *nb,
						unsigned long val, void *v)
{
	int pm_qos_class = *((int *)v);
	struct cpumask mask;
	int cpu = 0, max;

	if (val < 0)
		return NOTIFY_BAD;

	cpumask_clear(&mask);

	switch (pm_qos_class) {
	case PM_QOS_CPU_ONLINE_MIN:
		return NOTIFY_OK;

	case PM_QOS_CPU_ONLINE_MAX:
		max = val;
		break;
	default:
		return NOTIFY_BAD;
	}

	do {
		cpumask_set_cpu(cpu, &mask);
	} while(++cpu < max && cpu < nr_cpu_ids);

	exynos_cpuhp_request("HP_QOS", mask, 0);

	return NOTIFY_OK;
}

/**********************************************************************************/
/*				        INIT					  */
/**********************************************************************************/
struct notifier_block	hp_qos_min_notifier;
struct notifier_block	hp_qos_max_notifier;

static void __init cpuhp_pm_qos_init(void)
{
	exynos_cpuhp_register("HP_QOS", *cpu_online_mask, 0);

	hp_qos_min_notifier.notifier_call = exynos_cpuhp_pm_qos_callback;
	hp_qos_min_notifier.priority = INT_MAX;
	hp_qos_max_notifier.notifier_call = exynos_cpuhp_pm_qos_callback;
	hp_qos_max_notifier.priority = INT_MAX;

	pm_qos_add_notifier(PM_QOS_CPU_ONLINE_MIN, &hp_qos_min_notifier);
	pm_qos_add_notifier(PM_QOS_CPU_ONLINE_MAX, &hp_qos_max_notifier);
}

static void __init cpuhp_user_init(void)
{
	struct cpumask mask;

	/* init user list */
	INIT_LIST_HEAD(&cpuhp.users);

	cpumask_copy(&mask, cpu_possible_mask);

	/* register user for SYSFS */
	cpumask_copy(&cpuhp.system_user.online_cpus, &mask);
	strcpy(cpuhp.system_user.name, "SYSTEM");
	cpuhp.system_user.type = 0;
	list_add(&cpuhp.system_user.list, &cpuhp.users);

	/* register user for SYSTEM */
	cpumask_copy(&cpuhp.sysfs_user.online_cpus, &mask);
	strcpy(cpuhp.sysfs_user.name, "SYSFS");
	cpuhp.sysfs_user.type = 0;
	list_add(&cpuhp.sysfs_user.list, &cpuhp.users);

	cpumask_copy(&cpuhp.online_cpus, cpu_online_mask);

	/* init pm_qos request and handler */
	cpuhp_pm_qos_init();
}

static void __init cpuhp_sysfs_init(void)
{
	cpuhp.kobj = kobject_create_and_add("cpuhp", power_kobj);
	if (!cpuhp.kobj) {
		pr_err("Fail to create cpuhp kboject\n");
		return;
	}

	/* Create /sys/power/cpuhotplug */
	if (sysfs_create_group(cpuhp.kobj, &cpuhp_group)) {
		pr_err("Fail to create cpuhp group\n");
		return;
	}

	/* link cpuhotplug directory to /sys/devices/system/cpu/cpuhp */
	if (sysfs_create_link(&cpu_subsys.dev_root->kobj, cpuhp.kobj, "cpuhp"))
		pr_err("Fail to link cpuctrl directory");
}

static int __init cpuhp_init(void)
{
	/* Initialize pm_qos request and handler */
	cpuhp_user_init();

	/* Create sysfs */
	cpuhp_sysfs_init();

	/* register pm notifier */
	register_pm_notifier(&exynos_cpuhp_nb);

	/* Enable cpuhp */
	cpuhp_enable(true);

	return 0;
}
arch_initcall(cpuhp_init);
