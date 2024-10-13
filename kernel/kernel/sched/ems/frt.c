#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

static DEFINE_PER_CPU(cpumask_var_t, frt_local_cpu_mask);
static int frt_boost_status[CGROUP_COUNT];

#define cpu_selected(cpu)	(cpu >= 0)

static bool frt_task_fits_capacity(struct task_struct *p, int cpu)
{
	unsigned long capacity_orig = capacity_cpu_orig(cpu);
	unsigned long util = ml_task_util(p);

	return capacity_orig >= util;
}

static inline bool frt_can_sync_to_cur_cpu(struct rq *cur_rq, struct task_struct *p,
					   int cur_cpu)
{
	if (!cpu_active(cur_cpu))
		return false;

	if (!cpumask_test_cpu(cur_cpu, p->cpus_ptr))
		return false;

	if (p->prio > cur_rq->rt.highest_prio.next)
		return false;

	if (cur_rq->rt.rt_nr_running > 2)
		return false;

	return true;
}

/*****************************************************************************/
/*				SELECT WAKEUP CPU			     */
/*****************************************************************************/
int frt_get_init_index(struct task_struct *p)
{
	int index = cpuctl_task_group_idx(p);
	int pe_list_size = get_pe_list_size();

	/* In case of single cluster, init_index should be zero. */
	if (pe_list_size == 1)
		return pe_list_size - 1;

	return frt_boost_status[index] ?  1 : 0;
}

int frt_find_lowest_rq(struct task_struct *p, struct cpumask *lowest_mask, int ret)
{
	int index = frt_get_init_index(p);
	struct pe_list *pe_list = get_pe_list(index);
	unsigned long task_util = ml_task_util(p);
	unsigned long best_cpu_util = ULONG_MAX;
	unsigned int best_exit_latency = UINT_MAX;
	unsigned int exit_latency = UINT_MAX;
	int cluster, cpu;
	int best_cpu = -1;
	bool best_tex_stat = true;

	if (!ret)
		return best_cpu;

	rcu_read_lock();
	for (cluster = 0; cluster < pe_list->num_of_cpus; cluster++) {
		for_each_cpu_and(cpu, lowest_mask, &pe_list->cpus[cluster]) {
			unsigned long cpu_util = 0;
			unsigned long irq_load = ml_cpu_irq_load(cpu);
			unsigned long irq_thr = (capacity_cpu_orig(cpu) * MLT_IRQ_LOAD_RATIO) / 100;
			bool is_tex;

			if (!cpu_active(cpu))
				continue;

			if (!cpumask_test_cpu(cpu, ecs_available_cpus()))
				continue;

			trace_ems_frt_lowest_rq_stat(p, cpu, irq_load,
					get_tex_level(cpu_rq(cpu)->curr),
					available_idle_cpu(cpu),
					get_idle_exit_latency(cpu_rq(cpu)));

			if (irq_load >= irq_thr)
				continue;

			if (!available_idle_cpu(cpu)) {
				if (cpu_overutilized_with_task(cpu, task_util))
					continue;

				cpu_util = ml_cpu_util(cpu);
			}

			is_tex = get_tex_level(cpu_rq(cpu)->curr) != NOT_TEX;
			if (is_tex && !best_tex_stat)
				continue;

			if ((best_tex_stat && is_tex) || (!best_tex_stat && !is_tex))
				if (cpu_util > best_cpu_util)
					continue;

			if (best_cpu_util == cpu_util && best_cpu == task_cpu(p))
				continue;

			exit_latency = get_idle_exit_latency(cpu_rq(cpu));

			if (cpu != task_cpu(p) && best_cpu_util == cpu_util) {
				if (best_exit_latency < exit_latency)
					continue;

				if (best_exit_latency == exit_latency)
					continue;
			}

			best_exit_latency = exit_latency;
			best_cpu_util = cpu_util;
			best_cpu = cpu;
			best_tex_stat = is_tex;
		}

		if (cpu_selected(best_cpu))
			break;
	}

	rcu_read_unlock();

	if (cpu_selected(best_cpu))
		trace_ems_frt_lowest_rq(p, index, best_cpu, best_exit_latency,
					best_cpu_util, best_tex_stat);

	return best_cpu;
}

int frt_select_task_rq_rt(struct task_struct *p, int prev_cpu,
			  int sd_flag, int wake_flags)
{
	struct task_struct *curr;
	struct cpumask *lowest_mask;
	struct rq *rq, *cur_rq;
	bool sync = !!(wake_flags & WF_SYNC);
	int target_cpu = -1, cur_cpu;
	bool wnp = false;
	int cpu, ret;

	/* For anything but wake ups, just return the task_cpu */
	if (sd_flag != SD_BALANCE_WAKE && sd_flag != SD_BALANCE_FORK)
		goto out;

	cur_cpu = raw_smp_processor_id();
	cur_rq = cpu_rq(cur_cpu);

	/* Handle sync flag */
	if (sync && frt_can_sync_to_cur_cpu(cur_rq, p, cur_cpu)) {
		target_cpu = cur_cpu;
		goto out;
	}

	rq = cpu_rq(prev_cpu);

	rcu_read_lock();
	curr = READ_ONCE(rq->curr); /* unlocked access */

	wnp = task_may_not_preempt(curr, prev_cpu);

	lowest_mask = this_cpu_cpumask_var_ptr(frt_local_cpu_mask);
	ret = cpupri_find_fitness(&task_rq(p)->rd->cpupri, p,
				lowest_mask, frt_task_fits_capacity);

	cpu = frt_find_lowest_rq(p, lowest_mask, ret);


	if (likely(cpu >= 0) && (wnp || p->prio < cpu_rq(cpu)->rt.highest_prio.curr))
		target_cpu = cpu;
	else
		target_cpu = prev_cpu;

	rcu_read_unlock();

out:
	update_rt_stat(target_cpu);
	trace_frt_select_task_rq(p, target_cpu, "rt-wakeup");

	return target_cpu;
}

static int frt_boost_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int group;

	for (group = 0; group < CGROUP_COUNT; group++)
		frt_boost_status[group] = cur_set->frt_boost.enabled[group];

	return NOTIFY_OK;
}

static struct notifier_block frt_boost_emstune_notifier = {
	.notifier_call = frt_boost_emstune_notifier_call,
};

void frt_init(void)
{
	int cpu, group;

	for_each_possible_cpu(cpu) {
		if (!zalloc_cpumask_var_node(&per_cpu(frt_local_cpu_mask, cpu),
					GFP_KERNEL, cpu_to_node(cpu))) {
			pr_err("failed to alloc frt_local_cpu_mask\n");
		}
	}

	for (group = 0; group < CGROUP_COUNT; group++)
		frt_boost_status[group] = 0;

	emstune_register_notifier(&frt_boost_emstune_notifier);

	pr_info("%s: frt initialized complete!\n", __func__);
}
