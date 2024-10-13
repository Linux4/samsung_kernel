/*
 * Core Exynos Mobile Scheduler
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/pm_qos.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>

#include "../sched.h"
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

/******************************************************************************
 * IOCTL interface                                                            *
 ******************************************************************************/
struct ems_bridge {
	int					req_count;
	wait_queue_head_t			ioctl_wq;

	struct file_operations			fops;
	struct miscdevice			miscdev;
};
static struct ems_bridge *bridge;

static ssize_t
ems_fops_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
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
	int ret = 0;
	unsigned long size;

	switch (cmd) {
	case EMS_IOCTL_R_ECS_DOMAIN_COUNT:
	{
		int __user *user_count;
		int ecs_domain_count;

		size = sizeof(int);

		user_count = (int __user *)arg;
		if (!access_ok(user_count, size)) {
			ret = -EFAULT;
			break;
		}

		/* Get the number of domains in ECS. */
		ecs_domain_count = ecs_ioctl_get_domain_count();

		if (copy_to_user(user_count, &ecs_domain_count, size)) {
			pr_warn("EMS: EMS_IOCTL_R_ECS_DOMAIN_COUNT doesn't work\n");
			ret = -EFAULT;
		}

		break;
	}
	case EMS_IOCTL_R_ECS_DOMAIN_INFO:
	{
		struct ems_ioctl_ecs_domain_info __user *user_info;
		struct ems_ioctl_ecs_domain_info ecs_domain_info;

		size = sizeof(struct ems_ioctl_ecs_domain_info);

		user_info = (struct ems_ioctl_ecs_domain_info __user *)arg;
		if (!access_ok(user_info, size)) {
			ret = -EFAULT;
			break;
		}

		/*
		 * Get domain info from ECS.
		 *
		 * NOTE: target domain should be delieved before this cmd.
		 */
		ecs_ioctl_get_ecs_domain_info(&ecs_domain_info);

		if (copy_to_user(user_info, &ecs_domain_info, size)) {
			pr_warn("EMS: EMS_IOCTL_R_ECS_DOMAIN_INFO doesn't work\n");
			ret = -EFAULT;
		}

		break;
	}
	case EMS_IOCTL_W_ECS_TARGET_DOMAIN_ID:
	{
		unsigned int __user *user_target_domain_id;
		unsigned int target_domain_id;

		size = sizeof(unsigned int);

		user_target_domain_id = (unsigned int __user *)arg;
		if (!access_ok(user_target_domain_id, size)) {
			ret = -EFAULT;
			break;
		}

		if (copy_from_user(&target_domain_id, user_target_domain_id, size)) {
			pr_warn("EMS: EMS_IOCTL_W_ECS_TARGET_DOMAIN_ID doesn't work\n");
			ret = -EFAULT;
			break;
		}

		/* Notify traget domain from user to ECS */
		ecs_ioctl_set_target_domain(target_domain_id);

		break;
	}
	case EMS_IOCTL_W_ECS_TARGET_STAGE_ID:
	{
		unsigned int __user *user_target_stage_id;
		unsigned int target_stage_id;

		size = sizeof(unsigned int);

		user_target_stage_id = (unsigned int __user *)arg;
		if (!access_ok(user_target_stage_id, size)) {
			ret = -EFAULT;
			break;
		}

		if (copy_from_user(&target_stage_id, user_target_stage_id, size)) {
			pr_warn("EMS: EMS_IOCTL_W_ECS_TARGET_STAGE_ID doesn't work\n");
			ret = -EFAULT;
			break;
		}

		/* Notify traget stage from user to ECS */
		ecs_ioctl_set_target_stage(target_stage_id);

		break;
	}
	case EMS_IOCTL_W_ECS_STAGE_THRESHOLD:
	{
		unsigned int __user *user_threshold;
		unsigned int threshold;

		size = sizeof(unsigned int);

		user_threshold = (unsigned int __user *)arg;
		if (!access_ok(user_threshold, size)) {
			ret = -EFAULT;
			break;
		}

		if (copy_from_user(&threshold, user_threshold, size)) {
			pr_warn("EMS: EMS_IOCTL_W_ECS_STAGE_THRESHOLD doesn't work\n");
			ret = -EFAULT;
			break;
		}

		/*
		 * Notify threshold for a stage in a domain to ECS
		 *
		 * NOTE: target domain and stage should be delieved before this cmd.
		 */
		ecs_ioctl_set_stage_threshold(threshold);

		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
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
	init_waitqueue_head(&bridge->ioctl_wq);

	ems_register_misc_device();

	return 0;
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
 * common function for ems                                                    *
 ******************************************************************************/
static const struct sched_class *sched_class_begin;
int get_sched_class_idx(const struct sched_class *class)
{
	const struct sched_class *c;
	int class_idx;

	for (c = sched_class_begin, class_idx = 0;
	     class_idx < NUM_OF_SCHED_CLASS;
	     c--, class_idx++)
		if (c == class)
			break;

	return 1 << class_idx;
}

int cpuctl_task_group_idx(struct task_struct *p)
{
	int idx;
	struct cgroup_subsys_state *css;

	rcu_read_lock();
	css = task_css(p, cpu_cgrp_id);
	idx = css->id - 1;
	rcu_read_unlock();

	return idx;
}

const struct cpumask *cpu_coregroup_mask(int cpu)
{
	return &cpu_topology[cpu].core_sibling;
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

static void qjump_rq_list_init(void)
{
	int cpu;

	for_each_cpu(cpu, cpu_possible_mask)
		INIT_LIST_HEAD(ems_qjump_list(cpu_rq(cpu)));
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

/******************************************************************************
 * main function for ems                                                      *
 ******************************************************************************/
int ems_select_task_rq_rt(struct task_struct *p, int prev_cpu,
			 int sd_flag, int wake_flag)
{
	return frt_select_task_rq_rt(p, prev_cpu, sd_flag);
}

int ems_select_task_rq_fair(struct task_struct *p, int prev_cpu,
			   int sd_flag, int wake_flag)
{
	return __ems_select_task_rq_fair(p, prev_cpu, sd_flag, wake_flag);
}

int ems_select_fallback_rq(struct task_struct *p, int target_cpu)
{
	if (target_cpu >= 0 && cpu_active(target_cpu))
		return target_cpu;

	return -1;
}

int ems_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	if (!cpu_overutilized(task_cpu(p))) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "underutilized");
		return 0;
	}

	if (!ontime_can_migrate_task(p, dst_cpu)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "ontime");
		return 0;
	}

	if (!cpumask_test_cpu(dst_cpu, cpus_binding_mask(p))) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "emstune");
		return 0;
	}

	if (tex_task(p)) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "prio pinning");
		return 0;
	}

	if (tex_suppress_task(p) &&
		cpumask_test_cpu(dst_cpu, cpu_fastest_mask())) {
		trace_ems_can_migrate_task(p, dst_cpu, false, "suppress task");
		return 0;
	}

	trace_ems_can_migrate_task(p, dst_cpu, true, "n/a");

	return 1;
}

void ems_idle_exit(int cpu, int state)
{
	mlt_idle_exit(cpu);
}

void ems_idle_enter(int cpu, int *state)
{
	mlt_idle_enter(cpu, *state);
}

void ems_tick(struct rq *rq)
{
	profile_sched_data();

	mlt_update(cpu_of(rq));

	stt_update(rq, NULL);

	frt_update_available_cpus(rq);

	monitor_sysbusy();
	somac_tasks();

	ontime_migration();
	ecs_update();

	gsc_update_tick(rq);
}

void ems_enqueue_task(struct rq *rq, struct task_struct *p, int flags)
{
	stt_enqueue_task(rq, p);

	tex_enqueue_task(p, cpu_of(rq));

	freqboost_enqueue_task(p, cpu_of(rq), flags);
}

void ems_dequeue_task(struct rq *rq, struct task_struct *p, int flags)
{
	stt_dequeue_task(rq, p);

	tex_dequeue_task(p, cpu_of(rq));

	freqboost_dequeue_task(p, cpu_of(rq), flags);
}

void ems_replace_next_task_fair(struct rq *rq, struct task_struct **p_ptr,
				struct sched_entity **se_ptr, bool *repick,
				bool simple, struct task_struct *prev)
{
	tex_replace_next_task_fair(rq, p_ptr, se_ptr, repick, simple, prev);
}

/* can_attach has non-zero value, it is not allowed to attach */
void ems_cpu_cgroup_can_attach(struct cgroup_taskset *tset, int can_attach)
{
	if (!can_attach)
		freqboost_can_attach(tset);
}

/* If EMS allows load balancing, return 0 */
int ems_load_balance(struct rq *rq)
{
	if (sysbusy_on_somac())
		return -EBUSY;

	if (!ecs_cpu_available(rq->cpu, NULL))
		return -EBUSY;

	return 0;
}

void ems_post_init_entity_util_avg(struct sched_entity *se)
{
	ntu_apply(se);
}

void ems_schedule(struct task_struct *prev,
		struct task_struct *next, struct rq *rq)
{
	int state = RUNNING;

	if (prev == next)
		return;

	if (get_sched_class_idx(next->sched_class) == EMS_SCHED_IDLE)
		state = IDLE_START;
	else if (get_sched_class_idx(prev->sched_class) == EMS_SCHED_IDLE)
		state = IDLE_END;

	mlt_task_switch(rq->cpu, next, state);
}

/*
 * idle load balancing details
 * - When one of the busy CPUs notice that there may be an idle rebalancing
 *   needed, they will kick the idle load balancer, which then does idle
 *   load balancing for all the idle CPUs.
 * - HK_FLAG_MISC CPUs are used for this task, because HK_FLAG_SCHED not set
 *   anywhere yet.
 * - Consider ECS cpus. If ECS seperate a cpu from scheduling, skip it.
 */
int ems_find_new_ilb(struct cpumask *nohz_idle_cpus_mask)
{
	int cpu;

	for_each_cpu_and(cpu, nohz_idle_cpus_mask,
			      housekeeping_cpumask(HK_FLAG_MISC)) {
		/* ECS doesn't allow the cpu to do idle load balance */
		if (!ecs_cpu_available(cpu, NULL))
			continue;

		if (available_idle_cpu(cpu))
			return cpu;
	}

	return nr_cpu_ids;
}

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

int busy_cpu_ratio = 150;

static int ems_probe(struct platform_device *pdev)
{
	int ret;

	/* get first sched_class pointer */
	sched_class_begin = cpu_rq(0)->stop->sched_class;

	cpumask_speed_init();
	qjump_rq_list_init();

	ems_kobj = &pdev->dev.kobj;

	core_init(pdev->dev.of_node);
	et_init(ems_kobj);
	mlt_init();
	ntu_init(ems_kobj);
	profile_sched_init(ems_kobj);
	ontime_init(ems_kobj);
	fclamp_init();
	esgov_pre_init(ems_kobj);
	ego_pre_init(ems_kobj);
	freqboost_init();
	frt_init(ems_kobj);
	ecs_init(ems_kobj);
	sysbusy_init(ems_kobj);
	emstune_init(ems_kobj, pdev->dev.of_node);
	stt_init(ems_kobj);
	gsc_init(ems_kobj);

	ret = hook_init();
	if (ret) {
		WARN_ON("EMS failed to register vendor hook\n");
		return ret;
	}

	ems_init_ioctl();

	if (sysfs_create_file(ems_kobj, &dev_attr_sched_topology.attr))
		pr_warn("failed to create sched_topology\n");

	if (sysfs_create_link(kernel_kobj, ems_kobj, "ems"))
		pr_warn("failed to link ems\n");

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
