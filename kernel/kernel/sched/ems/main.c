/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd
 */

#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/panic_notifier.h>

#include "ems.h"

#define CREATE_TRACE_POINTS
#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include <dt-bindings/soc/samsung/ems.h>

/******************************************************************************
 * panic handler for scheduler debugging                                      *
 ******************************************************************************/
static inline void print_task_info(struct task_struct *p)
{
	pr_info("\t\tcomm=%s pid=%d prio=%d isa=%s cpus_ptr=%*pbl\n",
		p->comm, p->pid, p->prio,
		test_ti_thread_flag(&p->thread_info, TIF_32BIT)
		? "32bit" : "64bit",
		cpumask_pr_args(p->cpus_ptr));
}

static int ems_panic_notifier_call(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	int cpu;
	struct rq *rq;

	pr_info("[SCHED-SNAPSHOT]\n");
	for_each_possible_cpu(cpu) {
		struct task_struct *curr, *p, *temp;
		struct plist_head *head;
		struct rb_node *next_node;

		if (!cpu_active(cpu)) {
			pr_info("cpu%d: offline\n", cpu);
			continue;
		}

		pr_info("<cpu%d>\n", cpu);

		rq = cpu_rq(cpu);

		pr_info("\tcurr:\n");
		curr = rq->curr;
		print_task_info(curr);

		if (list_empty(&rq->cfs_tasks))
			pr_info("\tcfs: no task\n");
		else {
			pr_info("\tcfs:\n");
			list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
				if (curr == p)
					continue;
				print_task_info(p);
			}
		}

		head = &rq->rt.pushable_tasks;
		if (plist_head_empty(head))
			pr_info("\trt: no task\n");
		else {
			pr_info("\t rt:\n");
			plist_for_each_entry_safe(p, temp, head, pushable_tasks)
				print_task_info(p);
		}

		if (RB_EMPTY_ROOT(&rq->dl.pushable_dl_tasks_root.rb_root))
			pr_info("\tdl: no task\n");
		else {
			pr_info("\t dl:\n");
			next_node = rq->dl.pushable_dl_tasks_root.rb_leftmost;
			while (next_node) {
				p = rb_entry(next_node, struct task_struct,
							pushable_dl_tasks);
				print_task_info(p);
				next_node = rq->dl.pushable_dl_tasks_root.rb_leftmost;
			}
		}

		pr_info("\n");
	}

	return 0;
}

static struct notifier_block ems_panic_nb = {
	.notifier_call = ems_panic_notifier_call,
	.priority = INT_MIN,
};

struct pe_list *pe_list;
static int num_of_list;

struct pe_list *get_pe_list(int index)
{
	if (unlikely(!pe_list))
		return NULL;

	if (index >= num_of_list)
		return NULL;

	return &pe_list[index];
}

int get_pe_list_size(void)
{
	return num_of_list;
}

/******************************************************************************
 * IOCTL interface                                                            *
 ******************************************************************************/
struct ems_bridge {
	int					req_count;
	wait_queue_head_t			ioctl_wq;

	struct file_operations			fops;
	struct miscdevice			miscdev;
	struct mutex				lock;
};
static struct ems_bridge *bridge;
static int send_mode;

void send_mode_to_user(int mode)
{
	mutex_lock(&bridge->lock);
	send_mode = mode;
	bridge->req_count++;
	mutex_unlock(&bridge->lock);

	wake_up_interruptible(&bridge->ioctl_wq);
}

static ssize_t
ems_fops_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	if (!access_ok(buf, sizeof(int)))
		return -EFAULT;

	mutex_lock(&bridge->lock);
	if (copy_to_user(buf, &send_mode, sizeof(int)))
		pr_info("%s: failed to send mode", __func__);

	bridge->req_count--;
	mutex_unlock(&bridge->lock);

	return count;
}

static __poll_t
ems_fops_poll(struct file *filp, struct poll_table_struct *wait)
{
	__poll_t mask = 0;

	poll_wait(filp, &bridge->ioctl_wq, wait);

	/* To manage kernel side request */
	if (bridge->req_count > 0)
		mask = EPOLLIN | EPOLLRDNORM;

	return mask;
}

static long
ems_fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static int ems_fops_release(struct inode *node, struct file *filp)
{
	return 0;
}

static int ems_register_misc_device(void)
{
	int ret;
	struct file_operations *fops = &bridge->fops;
	struct miscdevice *miscdev = &bridge->miscdev;

	fops->owner		= THIS_MODULE;
	fops->llseek		= no_llseek;
	fops->read		= ems_fops_read;
	fops->poll		= ems_fops_poll;
	fops->unlocked_ioctl	= ems_fops_ioctl;
	fops->compat_ioctl	= ems_fops_ioctl;
	fops->release		= ems_fops_release;

	miscdev->minor		= MISC_DYNAMIC_MINOR;
	miscdev->name		= "ems";
	miscdev->fops		= fops;

	ret = misc_register(miscdev);
	if (ret) {
		pr_err("ems couldn't register misc device %d!", ret);
		return ret;
	}

	return 0;
}

static int ems_init_ioctl(void)
{
	bridge = kzalloc(sizeof(struct ems_bridge), GFP_KERNEL);
	if (!bridge) {
		pr_err("Failed to allocate bridge\n");
		return -ENOMEM;
	}

	bridge->req_count = 0;
	mutex_init(&bridge->lock);
	init_waitqueue_head(&bridge->ioctl_wq);

	ems_register_misc_device();

	return 0;
}

/******************************************************************************
 * common function for ems                                                    *
 ******************************************************************************/
int get_sched_class(struct task_struct *p)
{
	if (is_idle_task(p))
		return EMS_SCHED_IDLE;
	else if (dl_prio(p->prio))
		return EMS_SCHED_DL;
	else if (rt_prio(p->prio))
		return EMS_SCHED_RT;
	else
		return EMS_SCHED_FAIR;
}

int cpuctl_task_group_idx(struct task_struct *p)
{
#ifndef CONFIG_SCHED_EMS_TASK_GROUP
	int idx;
	struct cgroup_subsys_state *css;

	rcu_read_lock();
	css = task_css(p, cpu_cgrp_id);
	idx = css->id - 1;
	rcu_read_unlock();

	/* if customer add new group, use the last group */
	if (idx >= CGROUP_COUNT)
		idx = CGROUP_COUNT - 1;

	return max(idx, 0);
#else
	int idx;
	struct ems_task_struct *ep = (struct ems_task_struct *)p->android_vendor_data1;

	idx = ep->cgroup;

	if (idx >= CGROUP_COUNT)
		idx = CGROUP_COUNT - 1;

	return max(idx, 0);
#endif
}
EXPORT_SYMBOL(cpuctl_task_group_idx);

const struct cpumask *cpu_clustergroup_mask(int cpu)
{
	return &cpu_topology[cpu].cluster_sibling;
}

struct cpumask slowest_mask;
struct cpumask fastest_mask;

const struct cpumask *cpu_slowest_mask(void)
{
	return &slowest_mask;
}

const struct cpumask *cpu_fastest_mask(void)
{
	return &fastest_mask;
}

static void cpumask_speed_init(void)
{
	int cpu;
	unsigned long min_cap = ULONG_MAX, max_cap = 0;

	cpumask_clear(&slowest_mask);
	cpumask_clear(&fastest_mask);

	for_each_cpu(cpu, cpu_possible_mask) {
		unsigned long cap;

		cap = capacity_cpu_orig(cpu);
		if (cap < min_cap)
			min_cap = cap;
		if (cap > max_cap)
			max_cap = cap;
	}

	for_each_cpu(cpu, cpu_possible_mask) {
		unsigned long cap;

		cap = capacity_cpu_orig(cpu);
		if (cap == min_cap)
			cpumask_set_cpu(cpu, &slowest_mask);
		if (cap == max_cap)
			cpumask_set_cpu(cpu, &fastest_mask);
	}
}
static void pe_list_init(void)
{
	struct device_node *dn, *child;
	int index = 0;

	dn = of_find_node_by_path("/ems/pe-list");
	if (!dn) {
		pr_err("%s: Fail to get pe-list node\n", __func__);
		return;
	}

	num_of_list = of_get_child_count(dn);
	if (num_of_list == 0)
		return;

	pe_list = kmalloc_array(num_of_list, sizeof(struct pe_list), GFP_KERNEL);

	for_each_child_of_node(dn, child) {
		struct pe_list *pl = &pe_list[index++];
		const char *buf[VENDOR_NR_CPUS];
		int i;

		pl->num_of_cpus = of_property_count_strings(child, "cpus");
		if (pl->num_of_cpus == 0)
			continue;

		pl->cpus = kmalloc_array(pl->num_of_cpus, sizeof(struct cpumask), GFP_KERNEL);

		of_property_read_string_array(child, "cpus", buf, pl->num_of_cpus);
		for (i = 0; i < pl->num_of_cpus; i++)
			cpulist_parse(buf[i], &pl->cpus[i]);
	}
}

static struct kobject *ems_kobj;
int send_uevent(char *msg)
{
	char *envp[] = { msg, NULL };
	int ret;

	ret = kobject_uevent_env(ems_kobj, KOBJ_CHANGE, envp);
	if (ret)
		pr_warn("%s: Failed to send uevent\n", __func__);

	return ret;
}

void detach_task(struct rq *src_rq, struct rq *dst_rq, struct task_struct *p)
{
	deactivate_task(src_rq, p, DEQUEUE_NOCLOCK);
	set_task_cpu(p, dst_rq->cpu);
}

int detach_one_task(struct rq *src_rq, struct rq *dst_rq,
					struct task_struct *target)
{
	struct task_struct *p, *n;

	list_for_each_entry_safe(p, n, &src_rq->cfs_tasks, se.group_node) {
		if (p != target)
			continue;

		if (!can_migrate(p, dst_rq->cpu))
			break;

		update_rq_clock(src_rq);
		detach_task(src_rq, dst_rq, p);
		return 1;
	}

	return 0;
}

void attach_task(struct rq *dst_rq, struct task_struct *p)
{
	activate_task(dst_rq, p, ENQUEUE_NOCLOCK);
	check_preempt_curr(dst_rq, p, 0);
}

void attach_one_task(struct rq *dst_rq, struct task_struct *p)
{
	struct rq_flags rf;

	rq_lock(dst_rq, &rf);
	update_rq_clock(dst_rq);
	attach_task(dst_rq, p);
	rq_unlock(dst_rq, &rf);
}

void ems_update_cpu_capacity(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long arch_capacity = arch_scale_cpu_capacity(cpu);
	unsigned long freq_capacity = arch_capacity, prev;
	unsigned int max_freq = 0, max_freq_orig = 0;

	ego_get_max_freq(cpu, &max_freq, &max_freq_orig);

	if (max_freq_orig && max_freq != max_freq_orig)
		freq_capacity = mult_frac(freq_capacity, max_freq, max_freq_orig);

	prev = rq->cpu_capacity_orig;
	rq->cpu_capacity_orig = freq_capacity;

	if (prev != rq->cpu_capacity_orig)
		trace_ems_update_cpu_capacity(cpu, prev, rq->cpu_capacity_orig,
				freq_capacity, arch_capacity);
}

/******************************************************************************
 * cpufreq notifier                                                           *
 ******************************************************************************/
static cpumask_var_t cpus_to_visit;
static void parsing_done_workfn(struct work_struct *work);
static DECLARE_WORK(parsing_done_work, parsing_done_workfn);

static int ems_cpufreq_policy_notify_callback(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct cpufreq_policy *policy = data;

	if (val != CPUFREQ_CREATE_POLICY)
		return 0;

	cpumask_andnot(cpus_to_visit, cpus_to_visit, policy->related_cpus);

	et_init_table(policy);
	fv_init_table(policy);

	if (cpumask_empty(cpus_to_visit)) {
		emstune_ontime_init();

		pr_debug("ems: energy: init energy table done\n");
		schedule_work(&parsing_done_work);
	}
	return 0;
}

static struct notifier_block ems_cpufreq_policy_notifier = {
	.notifier_call = ems_cpufreq_policy_notify_callback,
	.priority = INT_MIN,
};

static void parsing_done_workfn(struct work_struct *work)
{
	cpufreq_unregister_notifier(&ems_cpufreq_policy_notifier,
					 CPUFREQ_POLICY_NOTIFIER);
	free_cpumask_var(cpus_to_visit);
}

static void ems_init_cpufreq_notifier(void)
{
	if (!alloc_cpumask_var(&cpus_to_visit, GFP_KERNEL))
		return;

	cpumask_copy(cpus_to_visit, cpu_possible_mask);

	if (cpufreq_register_notifier(&ems_cpufreq_policy_notifier,
					CPUFREQ_POLICY_NOTIFIER))
		free_cpumask_var(cpus_to_visit);
}


/******************************************************************************
*				SYSFS						*
 ******************************************************************************/
static ssize_t show_sched_topology(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cpu;
	struct sched_domain *sd;
	int ret = 0;

	rcu_read_lock();
	for_each_possible_cpu(cpu) {
		int sched_domain_level = 0;

		sd = rcu_dereference_check_sched_domain(cpu_rq(cpu)->sd);
		if (!sd)
			continue;
		while (sd->parent) {
			sched_domain_level++;
			sd = sd->parent;
		}

		for (; sd; sd = sd->child) {
			ret += snprintf(buf + ret, 70,
					"[lv%d] cpu%d: flags=%#x sd->span=%#x sg->span=%#x\n",
					sched_domain_level, cpu, sd->flags,
					*(unsigned int *)cpumask_bits(sched_domain_span(sd)),
					*(unsigned int *)cpumask_bits(sched_group_span(sd->groups)));
			sched_domain_level--;
		}
		ret += snprintf(buf + ret,
				50, "----------------------------------------\n");
	}
	rcu_read_unlock();

	return ret;
}

static DEVICE_ATTR(sched_topology, S_IRUGO, show_sched_topology, NULL);

static ssize_t show_pe_list(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int index, i;
	int ret = 0;

	for (index = 0; index < num_of_list; index++) {
		struct pe_list *pl = &pe_list[index];

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "pe_list[%d]: ", index);
		for (i = 0; i < pl->num_of_cpus; i++)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "<%*pbl> ",
					cpumask_pr_args(&pl->cpus[i]));
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	return ret;
}

static DEVICE_ATTR(pe_list, S_IRUGO, show_pe_list, NULL);

struct _rq_avd1 rq_avd1[VENDOR_NR_CPUS];

static int ems_probe(struct platform_device *pdev)
{
	int cpu;
	int ret;

	ems_kobj = &pdev->dev.kobj;

	for_each_possible_cpu(cpu) {
		RQ_AVD1_0(cpu_rq(cpu)) = (u64)&rq_avd1[cpu];
		pr_info("%s:%d: RQ_AVD1_0[%d] = %p\n", __func__, __LINE__,
				cpu, (void *)RQ_AVD1_0(cpu_rq(cpu)));
	}

	/* compile time data size check */
	EMS_VENDOR_DATA_SIZE_CHECK(struct ems_task_struct, struct task_struct);

	exynos_perf_events_init(ems_kobj);

	cpumask_speed_init();
	pe_list_init();
	mlt_init(ems_kobj, pdev->dev.of_node);
	profile_sched_init(ems_kobj);

	ems_idle_select_init(pdev);
	ems_freq_select_init(pdev);
	ems_core_select_init(pdev);

	ret = hook_init();
	if (ret) {
		WARN_ON("EMS failed to register vendor hook\n");
		return ret;
	}

	ems_init_ioctl();

	if (sysfs_create_file(ems_kobj, &dev_attr_sched_topology.attr))
		pr_warn("failed to create sched_topology\n");

	if (sysfs_create_file(ems_kobj, &dev_attr_pe_list.attr))
		pr_warn("failed to create pe_list\n");

	if (sysfs_create_link(kernel_kobj, ems_kobj, "ems"))
		pr_warn("failed to link ems\n");

#ifdef CONFIG_SCHED_EMS_TASK_GROUP
	if (task_group_init(ems_kobj))
		pr_warn("failed to taskgroup_init\n");
#endif

	pr_info("EMS initialized\n");

	atomic_notifier_chain_register(&panic_notifier_list, &ems_panic_nb);

	ems_init_cpufreq_notifier();

	return 0;
}

static const struct of_device_id of_ems_match[] = {
	{ .compatible = "samsung,ems", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_ems_match);

static struct platform_driver ems_driver = {
	.driver = {
		.name = "ems",
		.owner = THIS_MODULE,
		.of_match_table = of_ems_match,
	},
	.probe          = ems_probe,
};

module_platform_driver(ems_driver);

MODULE_DESCRIPTION("EMS");
MODULE_LICENSE("GPL");
