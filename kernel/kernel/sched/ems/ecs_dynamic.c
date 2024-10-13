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
	struct list_head		domain_list;
	struct cpumask			governor_cpus;
	struct cpumask			min_cpus;
	int				governor_disabled;
	unsigned int			default_gov;
	unsigned long			last_update_jiffies;

	int cpu_ar[VENDOR_NR_CPUS];

	struct kobject *kobj;

} dynamic;

static DEFINE_RAW_SPINLOCK(dynamic_lock);

/* Governor Disabled Type */
#define GOV_CTRL_BY_USER	(1 << 0)
#define GOV_CTRL_BY_SYSBUSY	(1 << 1)
#define GOV_CTRL_BY_RATIO	(1 << 2)
#define GOV_CTRL_BY_INIT	(1 << 3)

/* Domain status */
#define ECS_OPTIMIZED		(0)
#define ECS_UNDER_NRRUN		(1 << 0)
#define ECS_HAS_IDLECPU		(1 << 1)	/* idle cpu with avg_nr_run less than 0.5 */
#define	ECS_OVER_NRRUN		(1 << 2)	/* avg nr_running overs 1 */
#define ECS_UNDER_AR		(1 << 3)	/* avg active ratio unders 35% */
#define ECS_OVER_AR		(1 << 4)	/* avg active ratio overs 70% */
#define ECS_OVER_UTIL		(1 << 5)	/* util overs 70% of capacity */
#define ECS_OVERFULL		(1 << 6)	/* task starvated, this domain need a help */
#define	ECS_HELP_PREV		(1 << 7)	/* need to help prev domain */

#define DEFAULT_BUSY_RATIO	(70)

#define IS_OVERLOAD(x)		(((x) & ECS_OVER_AR) || ((x) & ECS_OVER_UTIL))

static void update_governor_cpus(const struct cpumask *new_cpus, char *event)
{
	struct dynamic_dom *domain;
	struct cpumask spared_cpus;
	struct cpumask target_cpus;

	cpumask_clear(&target_cpus);
	cpumask_or(&target_cpus, new_cpus, &dynamic.min_cpus);

	trace_ecs_update_governor_cpus(event,
			*(unsigned int *)cpumask_bits(&dynamic.governor_cpus),
			*(unsigned int *)cpumask_bits(&target_cpus));

	/* update governor cpus */
	cpumask_copy(&dynamic.governor_cpus, &target_cpus);

	/* update wether domain has spared_cpus or not and domain->governor_cpus */
	cpumask_complement(&spared_cpus, &dynamic.governor_cpus);
	list_for_each_entry(domain, &dynamic.domain_list, node) {
		cpumask_and(&domain->governor_cpus, &domain->cpus, &target_cpus);
		domain->has_spared_cpu = cpumask_intersects(&domain->cpus, &spared_cpus);
	}

	update_ecs_cpus();
}

static struct dynamic_dom *dynamic_get_cpu_domain(int cpu)
{
	struct dynamic_dom *domain;

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		if (cpumask_test_cpu(cpu, &domain->cpus))
			return domain;
	}
	return NULL;
}

static struct dynamic_dom *dynamic_get_domain(int id)
{
	struct dynamic_dom *domain;

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		if (domain->id == id)
			return domain;
	}
	return NULL;
}

/* Should be called with ECS LOCK */
static void dynamic_control_governor(int user, int enable)
{
	unsigned long flags;
	bool prev_mode, new_mode;

	raw_spin_lock_irqsave(&dynamic_lock, flags);

	prev_mode = dynamic.governor_disabled ? false : true;

	if (enable)
		dynamic.governor_disabled = dynamic.governor_disabled & (~user);
	else
		dynamic.governor_disabled = dynamic.governor_disabled | user;

	new_mode = dynamic.governor_disabled ? false : true;

	if (prev_mode == new_mode) {
		raw_spin_unlock_irqrestore(&dynamic_lock, flags);
		return;
	}

	update_governor_cpus(cpu_online_mask, "governor-ctrl");

	raw_spin_unlock_irqrestore(&dynamic_lock, flags);

}

static void _dynamic_enable_cpus(struct dynamic_dom *domain,
				struct cpumask *new_cpus, int cnt)
{
	struct cpumask spare_cpus, target_cpus;
	int cpu;

	cpumask_copy(&target_cpus, &domain->governor_cpus);
	cpumask_andnot(&spare_cpus, &domain->cpus, &domain->governor_cpus);

	for_each_cpu(cpu, &spare_cpus) {
		 if (--cnt < 0)
			break;

		cpumask_set_cpu(cpu, &target_cpus);
	}

	cpumask_or(new_cpus, new_cpus, &target_cpus);
}

/*
 * It is fast track to enable sapred cpus
 * when enqueue task and should increase frequency because can't use spared cpus,
 * enable spared cpus imediately to avoid increase frequency
 */
static void dynamic_enqueue_enable_cpus(int prev_cpu, struct task_struct *p)
{
	int cpu, util_with, min_util = INT_MAX;
	struct dynamic_dom *domain;
	struct cpumask new_cpus;
	unsigned long flags;

	if (dynamic.governor_disabled)
		return;

	/* find min util cpus from available cpus */
	domain = dynamic_get_cpu_domain(prev_cpu);
	if (unlikely(!domain))
		return;

	if (!domain->has_spared_cpu)
		return;

	if (cpumask_empty(&domain->governor_cpus))
		return;

	for_each_cpu(cpu, &domain->governor_cpus) {
		struct rq *rq = cpu_rq(cpu);
		int cpu_util = ml_cpu_util(cpu) + cpu_util_rt(rq) + cpu_util_dl(rq);

		if (cpu_util > min_util)
			continue;

		min_util = cpu_util;
	}

	util_with = _ml_task_util_est(p) + min_util;
	util_with = util_with + (util_with >> 2);
	if (et_cur_cap(prev_cpu) >= util_with)
		return;


	raw_spin_lock_irqsave(&dynamic_lock, flags);

	cpumask_copy(&new_cpus, &dynamic.governor_cpus);

	/* enable cpus */
	domain->throttle_cnt = 0;
	_dynamic_enable_cpus(domain, &new_cpus, 1);

	update_governor_cpus(&new_cpus, "governor-enqueue");

	raw_spin_unlock_irqrestore(&dynamic_lock, flags);

}

static void dynamic_enable_cpus(struct dynamic_dom *domain, struct cpumask *new_cpus)
{
	int min_cnt, cnt = 0;

	/* Reset the throttle count */
	domain->throttle_cnt = 0;

	if (!domain->has_spared_cpu)
		return;

	/* Open cpu when cpu is overloaded */
	if (IS_OVERLOAD(domain->flag))
		cnt++;

	/* Check to should help prev domain */
	if (domain->flag & ECS_HELP_PREV) {
		/* if domain is overloaded or no nr_cpus, should open more cpu */
		if (IS_OVERLOAD(domain->flag) || !domain->nr_cpu) {
			if (domain->slower_misfit)
				cnt += domain->slower_misfit;
			else
				cnt++;
		}
	}

	min_cnt = max((domain->avg_nr_run - domain->nr_cpu), 0);
	cnt = max(min_cnt, cnt);

	_dynamic_enable_cpus(domain, new_cpus, cnt);
}

static void dynamic_sustain_cpus(struct dynamic_dom *domain)
{
	if (IS_OVERLOAD(domain->flag))
		domain->throttle_cnt = 0;
}

static void dynamic_disable_cpus(struct dynamic_dom *domain,
				struct cpumask *new_cpus)
{
	/* if domain is under load, throttle cpu more fast */
	int delta_cnt = domain->flag & ECS_UNDER_AR ? 2 : 1;

	if (!cpumask_weight(&domain->governor_cpus))
		return;

	domain->throttle_cnt += delta_cnt;
	if (domain->throttle_cnt < 6)
		return;

	domain->throttle_cnt = 0;

	cpumask_clear_cpu(cpumask_last(&domain->governor_cpus), new_cpus);
}

static void update_domain_cpus(struct dynamic_dom *domain, struct cpumask *new_cpus)
{
	if (domain->flag & ECS_OVER_NRRUN || domain->flag & ECS_HELP_PREV)
		dynamic_enable_cpus(domain, new_cpus);
	else if (domain->flag & ECS_UNDER_NRRUN && domain->flag & ECS_HAS_IDLECPU)
		dynamic_disable_cpus(domain, new_cpus);
	else
		dynamic_sustain_cpus(domain);
}

static int update_domain_info(struct dynamic_dom *domain,
			int prev_misfit, int prev_flag)
{
	int nr_cpu, nr_busy_cpu = 0, avg_nr_run = 0, misfit = 0, active_ratio= 0;
	unsigned long util = 0;
	int cpu, flag = ECS_OPTIMIZED;
	int threshold, cap = 0;

	for_each_cpu(cpu, &domain->cpus) {
		struct rq *rq = cpu_rq(cpu);
		int avg_nr;

		/* 1. compute average avg_nr_run */
		avg_nr = mlt_runnable(rq);
		avg_nr_run += avg_nr;

		/*
		 * 2. compute busy cpu
		 * If cpu average nr running overs 0.5, this cpu is busy
		 */
		nr_busy_cpu += min(((avg_nr + MLT_RUNNABLE_ROUNDS_UNIT)
						/ MLT_RUNNABLE_UNIT), 1);

		/* 3. compute misfit */
		misfit += ems_rq_nr_misfited(rq);

		/* 4. compute util */
		util += (ml_cpu_util(cpu) + cpu_util_rt(rq));

		active_ratio += dynamic.cpu_ar[cpu] = mlt_cpu_value(cpu, 2, AVERAGE, ACTIVE);

		/* 5. compute capacity of available cpu */
		if (!cpumask_test_cpu(cpu, &domain->governor_cpus))
			continue;
		cap += capacity_orig_of(cpu);
	}

	domain->nr_cpu = nr_cpu = cpumask_weight(&domain->governor_cpus);
	avg_nr_run = (avg_nr_run + domain->nr_run_up_ratio) / MLT_RUNNABLE_UNIT;
	domain->avg_nr_run = avg_nr_run;
	domain->nr_busy_cpu = nr_busy_cpu;
	domain->slower_misfit = prev_misfit;
	domain->util = util;
	domain->cap = cap;

	/* nr_running is basic information */
	if (avg_nr_run > nr_cpu)
		flag = ECS_OVER_NRRUN;
	else if (avg_nr_run < nr_cpu)
		flag = ECS_UNDER_NRRUN;

	if (nr_busy_cpu < nr_cpu)
		flag |= ECS_HAS_IDLECPU;

	threshold = domain->busy_ratio * 10 * nr_cpu;
	if (active_ratio > threshold)
		flag |= ECS_OVER_AR;
	else if (active_ratio < (threshold >> 1))
		flag |= ECS_UNDER_AR;

	threshold = domain->cap * domain->busy_ratio / 100;
	if (util > threshold)
		flag |= ECS_OVER_UTIL;

	if (!domain->has_spared_cpu && (flag & ECS_OVER_NRRUN) && IS_OVERLOAD(flag)) {
		flag |= ECS_OVERFULL;
		misfit += prev_misfit;
	}

	if (prev_misfit || prev_flag & ECS_OVERFULL)
		flag |= ECS_HELP_PREV;

	domain->flag = flag;

	trace_dynamic_domain_info(cpumask_first(&domain->cpus), domain);

	return misfit;
}

static void update_dynamic_governor_cpus(void)
{
	struct dynamic_dom *domain;
	struct cpumask new_cpus;
	int prev_misfit = 0, prev_flag = 0;

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		prev_misfit = update_domain_info(domain, prev_misfit, prev_flag);
		prev_flag = domain->flag;
	}

	cpumask_copy(&new_cpus, &dynamic.governor_cpus);
	list_for_each_entry_reverse(domain, &dynamic.domain_list, node)
		update_domain_cpus(domain, &new_cpus);

	if (cpumask_equal(&dynamic.governor_cpus, &new_cpus))
		return;

	update_governor_cpus(&new_cpus, "governor");
}

/******************************************************************************
 *                       emstune mode update notifier                         *
 ******************************************************************************/
static int dynamic_mode_update_callback(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct dynamic_dom *dom;
	int prev_en, new_en = false;

	/* update min_cpus */
	cpumask_copy(&dynamic.min_cpus, &cur_set->ecs_dynamic.min_cpus);

	/* update busy ratio */
	list_for_each_entry(dom, &dynamic.domain_list, node) {
		int cpu = cpumask_first(&dom->cpus);
		dom->busy_ratio = cur_set->ecs_dynamic.dynamic_busy_ratio[cpu];
		if (dom->busy_ratio)
			new_en = true;
	}

	prev_en = !(dynamic.governor_disabled & GOV_CTRL_BY_RATIO);
	if (new_en != prev_en)
		dynamic_control_governor(GOV_CTRL_BY_RATIO, new_en);

	return NOTIFY_OK;
}

static struct notifier_block dynamic_mode_update_notifier = {
	.notifier_call = dynamic_mode_update_callback,
};

/******************************************************************************
 *                      sysbusy state change notifier                         *
 ******************************************************************************/
static int dynamic_sysbusy_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v)
{
	enum sysbusy_state state = *(enum sysbusy_state *)v;
	int enable;

	if (val != SYSBUSY_STATE_CHANGE)
		return NOTIFY_OK;

	enable = (state == SYSBUSY_STATE0);
	dynamic_control_governor(GOV_CTRL_BY_SYSBUSY, enable);

	return NOTIFY_OK;
}

static struct notifier_block dynamic_sysbusy_notifier = {
	.notifier_call = dynamic_sysbusy_notifier_call,
};

/******************************************************************************
 *				SYSFS		                              *
 ******************************************************************************/
static ssize_t show_dynamic_dom(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct dynamic_dom *domain;
	struct cpumask min_cpus;

	if (list_empty(&dynamic.domain_list)) {
		ret += sprintf(buf + ret, "domain list empty\n");
		return ret;
	}

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		cpumask_and(&min_cpus, &domain->cpus, &dynamic.min_cpus);
		ret += sprintf(buf + ret, "[domain%d]\n", domain->id);
		ret += sprintf(buf + ret, "cpus : %*pbl\n",
					cpumask_pr_args(&domain->cpus));
		ret += sprintf(buf + ret, "min-cpus : %*pbl\n",
					cpumask_pr_args(&dynamic.min_cpus));
		ret += sprintf(buf + ret, "governor_cpus : %*pbl\n",
					cpumask_pr_args(&domain->governor_cpus));
		ret += sprintf(buf + ret, "emstune busy ratio : %u\n",
					domain->busy_ratio);
		ret += sprintf(buf + ret, "emstune nr_run_up_ratio : %d\n",
					domain->nr_run_up_ratio);
		ret += sprintf(buf + ret, " --------------------------------\n");
	}

	return ret;
}

static struct kobj_attribute dynamic_dom_attr =
__ATTR(domains, 0444, show_dynamic_dom, NULL);

static ssize_t show_dynamic_governor_disabled(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "disabled=%d\n", dynamic.governor_disabled);
}

static ssize_t store_dynamic_governor_disabled(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int disable;

	if (sscanf(buf, "%d", &disable) != 1)
		return -EINVAL;

	dynamic_control_governor(GOV_CTRL_BY_USER, !disable);

	return count;
}

static struct kobj_attribute dynamic_governor_enable_attr =
__ATTR(governor_disabled, 0644, show_dynamic_governor_disabled,
				store_dynamic_governor_disabled);

static ssize_t show_dynamic_ratio(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct dynamic_dom *domain;

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"domain=%d busy-ratio=%d\n",
				domain->id, domain->busy_ratio);
	}

	return ret;
}

static ssize_t store_dynamic_ratio(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int domain_id, ratio;
	struct dynamic_dom *domain;

	if (sscanf(buf, "%d %d", &domain_id, &ratio) != 2)
		return -EINVAL;

	if (domain_id < 0 || domain_id >= VENDOR_NR_CPUS)
		return -EINVAL;

	domain = dynamic_get_domain(domain_id);
	if (!domain)
		return -EINVAL;

	domain->busy_ratio = ratio;

	return count;
}
static struct kobj_attribute dynamic_ratio =
__ATTR(ratio, 0644, show_dynamic_ratio, store_dynamic_ratio);

static ssize_t show_dynamic_nr_run_up_unit(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	struct dynamic_dom *domain;

	list_for_each_entry(domain, &dynamic.domain_list, node) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"domain=%d nr_run_up_ratio=%d\n",
				domain->id, domain->nr_run_up_ratio);
	}

	return ret;
}

static ssize_t store_dynamic_nr_run_up_unit(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int domain_id, ratio;
	struct dynamic_dom *domain;

	if (sscanf(buf, "%d %d", &domain_id, &ratio) != 2)
		return -EINVAL;

	if (domain_id < 0 || domain_id >= VENDOR_NR_CPUS)
		return -EINVAL;

	domain = dynamic_get_domain(domain_id);
	if (!domain)
		return -EINVAL;

	domain->nr_run_up_ratio = ratio;

	return count;
}
static struct kobj_attribute dynamic_nr_run_up_unit =
__ATTR(avg_nr_run_unit, 0644, show_dynamic_nr_run_up_unit, store_dynamic_nr_run_up_unit);

static ssize_t show_dynamic_min_cpus(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "min-cpus: %*pbl\n",cpumask_pr_args(&dynamic.min_cpus));
}

#define STR_LEN 7
static ssize_t store_dynamic_min_cpus(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct cpumask new_cpus;
	char str[STR_LEN], *str_ptr;

	if (sscanf(buf, "%6s", str) != 2)
		return -EINVAL;

	str_ptr = &str[0];
	if (str[0] == '0' && toupper(str[1]) == 'X')
		str_ptr = str_ptr + 2;
	if (cpumask_parse(str_ptr, &new_cpus) || cpumask_empty(&new_cpus))
		return -EINVAL;

	cpumask_copy(&dynamic.min_cpus, &new_cpus);

	return count;
}
static struct kobj_attribute dynamic_min_cpus =
__ATTR(min_cpus, 0644, show_dynamic_min_cpus, store_dynamic_min_cpus);

static int dynamic_sysfs_init(struct kobject *ecs_gov_kobj)
{
	int ret;

	dynamic.kobj = kobject_create_and_add("dynamic", ecs_gov_kobj);
	if (!dynamic.kobj) {
		pr_info("%s: fail to create node\n", __func__);
		return -EINVAL;
	}

	ret = sysfs_create_file(dynamic.kobj, &dynamic_dom_attr.attr);
	if (ret)
		pr_warn("%s: failed to create dynamic sysfs\n", __func__);
	ret = sysfs_create_file(dynamic.kobj, &dynamic_governor_enable_attr.attr);
	if (ret)
		pr_warn("%s: failed to create dynamic sysfs\n", __func__);

	ret = sysfs_create_file(dynamic.kobj, &dynamic_ratio.attr);
	if (ret)
		pr_warn("%s: failed to create dynamic sysfs\n", __func__);

	ret = sysfs_create_file(dynamic.kobj, &dynamic_nr_run_up_unit.attr);
	if (ret)
		pr_warn("%s: failed to create dynamic sysfs\n", __func__);

	ret = sysfs_create_file(dynamic.kobj, &dynamic_min_cpus.attr);
	if (ret)
		pr_warn("%s: failed to create dynamic sysfs\n", __func__);

	return ret;
}

/******************************************************************************
 *                               initialization                               *
 ******************************************************************************/
static int
init_dynamic_dom(struct device_node *dn, struct list_head *domain_list, int domain_id)
{
	int ret = 0;
	const char *buf;
	struct dynamic_dom *domain;

	domain = kzalloc(sizeof(struct dynamic_dom), GFP_KERNEL);
	if (!domain) {
		pr_err("%s: fail to alloc dynamic domain\n", __func__);
		return -ENOMEM;
	}

	if (!of_property_read_string(dn, "cpus", &buf))
		cpulist_parse(buf, &domain->cpus);

	if (of_property_read_s32(dn, "nr-run-up-ratio", &domain->nr_run_up_ratio))
		domain->nr_run_up_ratio = MLT_RUNNABLE_UNIT;

	domain->id = domain_id;
	domain->busy_ratio = DEFAULT_BUSY_RATIO;
	cpumask_copy(&domain->governor_cpus, &domain->cpus);
	list_add_tail(&domain->node, domain_list);

	return ret;
}

static int init_dynamic_dom_list(void)
{
	int ret = 0, domain_id = 0;
	struct device_node *dn, *domain_dn;

	dn = of_find_node_by_path("/ems/ecs_dynamic");
	if (!dn) {
		pr_err("%s: fail to get dynamic.device node\n", __func__);
		return -EINVAL;
	}

	INIT_LIST_HEAD(&dynamic.domain_list);

	for_each_child_of_node(dn, domain_dn) {
		if (init_dynamic_dom(domain_dn, &dynamic.domain_list, domain_id)) {
			ret = -EINVAL;
			goto finish;
		}
		domain_id++;
	}

	cpumask_copy(&dynamic.min_cpus, cpu_possible_mask);

	of_property_read_u32(dn, "default-gov", &dynamic.default_gov);

finish:
	return ret;
}

static void dynamic_start(void)
{
	dynamic_control_governor(GOV_CTRL_BY_USER, true);
}

#define UPDATE_PERIOD	(1)		/* 1 jiffies */
static void dynamic_update(void)
{
	unsigned long now = jiffies;

	if (!raw_spin_trylock(&dynamic_lock))
		return;

	if (dynamic.governor_disabled)
		goto out;

	if (now < (dynamic.last_update_jiffies + UPDATE_PERIOD))
		goto out;

	update_dynamic_governor_cpus();

	dynamic.last_update_jiffies = now;

out:
	raw_spin_unlock(&dynamic_lock);
}

static void dynamic_stop(void)
{
	dynamic_control_governor(GOV_CTRL_BY_USER, false);
}

static const struct cpumask *dynamic_get_target_cpus(void)
{
	return &dynamic.governor_cpus;
}

static struct ecs_governor dynamic_gov = {
	.name = "dynamic",
	.update = dynamic_update,
	.enqueue_update = dynamic_enqueue_enable_cpus,
	.start = dynamic_start,
	.stop = dynamic_stop,
	.get_target_cpus = dynamic_get_target_cpus,
};

int ecs_gov_dynamic_init(struct kobject *ems_kobj)
{
	if (init_dynamic_dom_list()) {
		pr_info("ECS governor will not affect scheduler\n");

		/* Reset domain list and dynamic.out_of_governing_cpus */
		INIT_LIST_HEAD(&dynamic.domain_list);
		/* disable governor */
		dynamic_control_governor(GOV_CTRL_BY_INIT, false);
	}

	dynamic_control_governor(GOV_CTRL_BY_USER, false);

	dynamic_sysfs_init(ecs_get_governor_object());
	emstune_register_notifier(&dynamic_mode_update_notifier);
	sysbusy_register_notifier(&dynamic_sysbusy_notifier);

	ecs_governor_register(&dynamic_gov, dynamic.default_gov);

	return 0;
}
