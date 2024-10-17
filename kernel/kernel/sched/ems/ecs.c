/*
 * Exynos Core Sparing Governor - Exynos Mobile Scheduler
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd
 * Youngtae Lee <yt0729.lee@samsung.com>
 */

#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

static struct {
	struct cpumask			cpus;

	struct ecs_governor		*gov;
	struct list_head		gov_list;

	/* ecs MAX request */
	struct list_head		requests_max;
	struct cpumask			requested_max_cpus;

	/* ecs MIN request */
	struct list_head		requests_min;
	struct cpumask			requested_min_cpus;

	struct kobject *ecs_kobj;
	struct kobject *governor_kobj;
} ecs;

static DEFINE_RAW_SPINLOCK(ecs_lock);
static DEFINE_RAW_SPINLOCK(ecs_req_lock);

/******************************************************************************
 *                                HELP Function				      *
 ******************************************************************************/
static inline const struct cpumask *get_governor_cpus(void)
{
	if (likely(ecs.gov))
		return ecs.gov->get_target_cpus();

	return cpu_possible_mask;
}

/******************************************************************************
 *                                core sparing	                              *
 ******************************************************************************/
static struct cpu_stop_work __percpu *ecs_migration_work;

static void detach_any_task(struct rq *src_rq, struct rq *dst_rq,
				struct task_struct *p, struct list_head *tasks)
{
	if (!can_migrate(p, dst_rq->cpu))
		return;

	detach_task(src_rq, dst_rq, p);
	list_add(&p->se.group_node, tasks);
}

static void detach_all_tasks(struct rq *src_rq,
		struct rq *dst_rq, struct list_head *tasks)
{
	struct task_struct *p, *n;

	update_rq_clock(src_rq);

	/* detach cfs tasks */
	list_for_each_entry_safe(p, n,
			&src_rq->cfs_tasks, se.group_node)
		detach_any_task(src_rq, dst_rq, p, tasks);

	/* detach rt tasks */
	plist_for_each_entry_safe(p, n,
			&src_rq->rt.pushable_tasks, pushable_tasks)
		detach_any_task(src_rq, dst_rq, p, tasks);
}

static void attach_all_tasks(struct rq *dst_rq, struct list_head *tasks)
{
	struct task_struct *p;
	struct rq_flags rf;

	rq_lock(dst_rq, &rf);
	update_rq_clock(dst_rq);

	while (!list_empty(tasks)) {
		p = list_first_entry(tasks, struct task_struct, se.group_node);
		list_del_init(&p->se.group_node);

		attach_task(dst_rq, p);
	}

	rq_unlock(dst_rq, &rf);
}

static int ecs_migration_cpu_stop(void *data)
{
	struct rq *src_rq = this_rq(), *dst_rq = data;
	struct list_head tasks = LIST_HEAD_INIT(tasks);
	struct rq_flags rf;

	rq_lock_irq(src_rq, &rf);
	detach_all_tasks(src_rq, dst_rq, &tasks);
	src_rq->active_balance = 0;
	rq_unlock(src_rq, &rf);

	if (!list_empty(&tasks))
		attach_all_tasks(dst_rq, &tasks);

	local_irq_enable();

	return 0;
}

static void __move_from_spared_cpus(int src_cpu, int dst_cpu)
{
	struct rq *rq = cpu_rq(src_cpu);
	unsigned long flags;

	if (unlikely(src_cpu == dst_cpu))
		return;

	raw_spin_rq_lock_irqsave(rq, flags);

	if (rq->active_balance) {
		raw_spin_rq_unlock_irqrestore(rq, flags);
		return;
	}

	rq->active_balance = 1;

	raw_spin_rq_unlock_irqrestore(rq, flags);

	/* Migrate all tasks from src to dst through stopper */
	stop_one_cpu_nowait(src_cpu, ecs_migration_cpu_stop, cpu_rq(dst_cpu),
			per_cpu_ptr(ecs_migration_work, src_cpu));
}

static void move_from_spared_cpus(struct cpumask *spared_cpus)
{
	int fcpu, idlest_cpu = 0;

	for_each_cpu(fcpu, cpu_active_mask) {
		int cpu, min_util = INT_MAX;

		/* To loop each domain */
		if (fcpu != cpumask_first(cpu_clustergroup_mask(fcpu)))
			continue;

		/* To find idlest cpu in this domain */
		for_each_cpu_and(cpu, get_governor_cpus(), cpu_clustergroup_mask(fcpu)) {
			struct rq *rq = cpu_rq(cpu);
			int cpu_util;

			cpu_util = ml_cpu_util(cpu) + cpu_util_rt(rq);
			if (min_util < cpu_util)
				continue;

			min_util = cpu_util;
			idlest_cpu = cpu;
		}

		/* Move task to idlest cpu */
		for_each_cpu_and(cpu, spared_cpus, cpu_clustergroup_mask(fcpu)) {
			if (available_idle_cpu(cpu))
				continue;
			__move_from_spared_cpus(cpu, idlest_cpu);
		}
	}
}

void update_ecs_cpus(void)
{
	struct cpumask spared_cpus, prev_cpus;
	unsigned long flags;

	raw_spin_lock_irqsave(&ecs_lock, flags);

	cpumask_copy(&prev_cpus, &ecs.cpus);

	cpumask_and(&ecs.cpus, get_governor_cpus(), cpu_possible_mask);
	cpumask_or(&ecs.cpus, &ecs.cpus, &ecs.requested_min_cpus);
	cpumask_and(&ecs.cpus, &ecs.cpus, &ecs.requested_max_cpus);

	if (cpumask_subset(&prev_cpus, &ecs.cpus)) {
		raw_spin_unlock_irqrestore(&ecs_lock, flags);
		return;
	}

	cpumask_andnot(&spared_cpus, cpu_active_mask, &ecs.cpus);
	cpumask_and(&spared_cpus, &spared_cpus, &prev_cpus);
	if (!cpumask_empty(&spared_cpus))
		move_from_spared_cpus(&spared_cpus);

	raw_spin_unlock_irqrestore(&ecs_lock, flags);
}

int ecs_cpu_available(int cpu, struct task_struct *p)
{
	if (p && is_per_cpu_kthread(p))
		return true;

	return cpumask_test_cpu(cpu, &ecs.cpus);
}

const struct cpumask *ecs_available_cpus(void)
{
	return &ecs.cpus;
}

const struct cpumask *ecs_cpus_allowed(struct task_struct *p)
{
	if (p && is_per_cpu_kthread(p))
		return cpu_active_mask;

	return &ecs.cpus;
}

void ecs_enqueue_update(int prev_cpu, struct task_struct *p)
{
	if (ecs.gov && ecs.gov->enqueue_update)
		ecs.gov->enqueue_update(prev_cpu, p);
}

void ecs_update(void)
{
	if (likely(ecs.gov))
		ecs.gov->update();
}

static void ecs_governor_set(struct ecs_governor *gov)
{
	if (ecs.gov == gov)
		return;

	/* stop current working governor */
	if (ecs.gov)
		ecs.gov->stop();

	/* change governor pointer and start new governor */
	gov->start();
	ecs.gov = gov;
}

void ecs_governor_register(struct ecs_governor *gov, bool default_gov)
{
	list_add(&gov->list, &ecs.gov_list);

	if (default_gov)
		ecs_governor_set(gov);
}

/******************************************************************************
 *                               ECS requestes                                *
 ******************************************************************************/
struct ecs_request {
	char			name[ECS_USER_NAME_LEN];
	struct cpumask		cpus;
	struct list_head	list;
};

static struct ecs_request *ecs_find_request(char *name, enum ecs_type type)
{
	struct ecs_request *req;
	struct list_head *head = type == ECS_MAX ? &ecs.requests_max : &ecs.requests_min;

	list_for_each_entry(req, head, list)
		if (!strcmp(req->name, name))
			return req;

	return NULL;
}

static void ecs_request_combine_and_apply(void)
{
	struct cpumask max_mask;
	struct cpumask min_mask;
	struct ecs_request *req;
	char buf[10];

	cpumask_setall(&max_mask);
	cpumask_clear(&min_mask);

	list_for_each_entry(req, &ecs.requests_max, list)
		cpumask_and(&max_mask, &max_mask, &req->cpus);

	if (cpumask_empty(&max_mask) || !cpumask_test_cpu(0, &max_mask)) {
		scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&max_mask));
		panic("ECS cpumask(%s) is wrong\n", buf);
	}
	cpumask_copy(&ecs.requested_max_cpus, &max_mask);

	list_for_each_entry(req, &ecs.requests_min, list)
		cpumask_or(&min_mask, &min_mask, &req->cpus);

	cpumask_and(&min_mask, &min_mask, cpu_possible_mask);
	cpumask_copy(&ecs.requested_min_cpus, &min_mask);

	update_ecs_cpus();
}

int ecs_request_register(char *name, const struct cpumask *mask, enum ecs_type type)
{
	struct ecs_request *req;
	char buf[ECS_USER_NAME_LEN];
	unsigned long flags;

	/* allocate memory for new request */
	req = kzalloc(sizeof(struct ecs_request), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	raw_spin_lock_irqsave(&ecs_req_lock, flags);

	/* check whether name is already register or not */
	if (ecs_find_request(name, type))
		panic("Failed to register ecs request! already existed\n");

	/* init new request information */
	if (mask)
		cpumask_copy(&req->cpus, mask);
	else
		cpumask_clear(&req->cpus);
	strcpy(req->name, name);

	/* register request list */
	if (type == ECS_MAX)
		list_add(&req->list, &ecs.requests_max);
	else
		list_add(&req->list, &ecs.requests_min);

	scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&req->cpus));
	pr_info("Register new ECS request [name:%s, mask:%s, type=%s]\n",
				req->name, buf, type ? "MAX" : "MIN");

	/* applying new request */
	ecs_request_combine_and_apply();

	raw_spin_unlock_irqrestore(&ecs_req_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ecs_request_register);

/* remove request on the request list of exynos_core_sparing request */
int ecs_request_unregister(char *name, enum ecs_type type)
{
	struct ecs_request *req;
	unsigned long flags;

	raw_spin_lock_irqsave(&ecs_req_lock, flags);

	req = ecs_find_request(name, type);
	if (!req) {
		raw_spin_unlock_irqrestore(&ecs_req_lock, flags);
		return -ENODEV;
	}

	/* remove request from list and free */
	list_del(&req->list);
	kfree(req);

	ecs_request_combine_and_apply();

	raw_spin_unlock_irqrestore(&ecs_req_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ecs_request_unregister);

int ecs_request(char *name, const struct cpumask *mask, enum ecs_type type)
{
	struct ecs_request *req;
	unsigned long flags;

	raw_spin_lock_irqsave(&ecs_req_lock, flags);

	req = ecs_find_request(name, type);
	if (!req) {
		raw_spin_unlock_irqrestore(&ecs_req_lock, flags);
		return -EINVAL;
	}

	cpumask_copy(&req->cpus, mask);
	ecs_request_combine_and_apply();

	raw_spin_unlock_irqrestore(&ecs_req_lock, flags);

	return 0;
}
EXPORT_SYMBOL_GPL(ecs_request);


struct kobject *ecs_get_governor_object(void)
{
	return ecs.governor_kobj;
}

/******************************************************************************
 *                                    sysfs                                   *
 ******************************************************************************/

#define STR_LEN (6)
static int __store_ecs_cpus(const char *buf, struct cpumask *mask)
{
	int ret;
	char str[STR_LEN];

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%5s", str))
		return -EINVAL;

	if (str[0] == '0' && str[1] =='x')
		ret = cpumask_parse(str + 2, mask);
	else
		ret = cpumask_parse(str, mask);

	if (ret){
		pr_err("input of req_cpus(%s) is not correct\n", buf);
		return -EINVAL;
	}

	return ret;
}

static ssize_t store_ecs_min_cpus(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct cpumask mask;

	if (__store_ecs_cpus(buf, &mask))
		return -EINVAL;

	ecs_request("SYSFS", &mask, ECS_MIN);

	return count;
}
static struct kobj_attribute ecs_min_cpus_attr =
__ATTR(cpus, 0644, NULL, store_ecs_min_cpus);

static ssize_t store_ecs_max_cpus(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct cpumask mask;

	if (__store_ecs_cpus(buf, &mask))
		return -EINVAL;

	ecs_request("SYSFS", &mask, ECS_MAX);

	return count;
}
static struct kobj_attribute ecs_max_cpus_attr =
__ATTR(cpus, 0644, NULL, store_ecs_max_cpus);

static ssize_t show_ecs_cpus(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ecs_request *req;
	int ret = 0;

	/* All combined CPUs */
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cur_cpus : %#x\n",
				*(unsigned int *)cpumask_bits(&ecs.cpus));

	/* Governor CPUs */
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"* governor target cpus : %#x\n",
				*(unsigned int *)cpumask_bits(get_governor_cpus()));

	/* Requested from kernel CPUs */
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"* requsted max cpus : %#x\n",
				*(unsigned int *)cpumask_bits(&ecs.requested_max_cpus));
	list_for_each_entry(req, &ecs.requests_max, list)
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     %s: %#x\n", req->name, *(unsigned int *)cpumask_bits(&req->cpus));

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"* requsted min cpus : %#x\n",
				*(unsigned int *)cpumask_bits(&ecs.requested_min_cpus));
	list_for_each_entry(req, &ecs.requests_min, list)
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"     %s: %#x\n", req->name, *(unsigned int *)cpumask_bits(&req->cpus));

	return ret;
}
static struct kobj_attribute ecs_cpus_attr =
__ATTR(cpus, 0444, show_ecs_cpus, NULL);

static ssize_t store_ecs_scaling_governor(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	char str[ECS_USER_NAME_LEN];
	struct ecs_governor *gov = NULL;

	if (strlen(buf) >= ECS_USER_NAME_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%s", str))
		return -EINVAL;

	list_for_each_entry(gov, &ecs.gov_list, list) {
		if (!strncasecmp(str, gov->name, ECS_USER_NAME_LEN)) {
			ecs_governor_set(gov);
			return count;
		}
	}

	pr_err("input of govenor name(%s) is not correct\n", buf);
	return -EINVAL;
}

static ssize_t show_ecs_scaling_governor(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	if (!ecs.gov)
		return snprintf(buf, PAGE_SIZE, "there is no working governor\n");

	return scnprintf(buf, ECS_USER_NAME_LEN, "%s\n", ecs.gov->name);
}

static struct kobj_attribute ecs_scaling_governor_attr =
__ATTR(scaling_governor, 0644, show_ecs_scaling_governor, store_ecs_scaling_governor);

static int ecs_sysfs_init(struct kobject *ems_kobj)
{
	int ret;

	ecs.ecs_kobj = kobject_create_and_add("ecs", ems_kobj);
	if (!ecs.ecs_kobj) {
		pr_info("%s: fail to create node\n", __func__);
		return -EINVAL;
	}

	ecs.governor_kobj = kobject_create_and_add("governos", ecs.ecs_kobj);
	if (!ecs.governor_kobj) {
		pr_info("%s: fail to create node for governos\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(ecs.ecs_kobj, &ecs_cpus_attr.attr);
	if (ret)
		pr_warn("%s: failed to create ecs_cpus sysfs\n", __func__);

	ret = sysfs_create_file(ecs.ecs_kobj, &ecs_min_cpus_attr.attr);
	if (ret)
		pr_warn("%s: failed to create ecs_min_cpus sysfs\n", __func__);

	ret = sysfs_create_file(ecs.ecs_kobj, &ecs_max_cpus_attr.attr);
	if (ret)
		pr_warn("%s: failed to create ecs_max_cpus sysfs\n", __func__);

	ret = sysfs_create_file(ecs.governor_kobj, &ecs_scaling_governor_attr.attr);
	if (ret)
		pr_warn("%s: failed to create ecs_scaling_governor sysfs\n", __func__);

	return 0;
}

int ecs_init(struct kobject *ems_kobj)
{
	ecs_migration_work = alloc_percpu(struct cpu_stop_work);
	if (!ecs_migration_work) {
		pr_err("falied to allocate ecs_migration_work\n");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&ecs.requests_max);
	INIT_LIST_HEAD(&ecs.requests_min);
	INIT_LIST_HEAD(&ecs.gov_list);

	ecs.gov = NULL;
	cpumask_copy(&ecs.cpus, cpu_possible_mask);
	cpumask_copy(&ecs.requested_max_cpus, cpu_possible_mask);
	cpumask_clear(&ecs.requested_min_cpus);
	ecs_request_register("SYSFS", cpu_possible_mask, ECS_MAX);
	ecs_request_register("SYSFS", NULL, ECS_MIN);

	ecs_sysfs_init(ems_kobj);

	return 0;
}
