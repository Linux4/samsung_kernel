/*
 * Energy efficient cpu selection
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include "../sched.h"
#include "ems.h"

/*
 * The compute capacity, power consumption at this compute capacity and
 * frequency of state. The cap and power are used to find the energy
 * efficiency cpu, and the frequency is used to create the capacity table.
 */
struct energy_state {
	unsigned long frequency;
	unsigned long cap;
	unsigned long power;

	/* for sse */
	unsigned long cap_s;
	unsigned long power_s;

	unsigned long static_power;
};

/*
 * Each cpu can have its own mips_per_mhz, coefficient and energy table.
 * Generally, cpus in the same frequency domain have the same mips_per_mhz,
 * coefficient and energy table.
 */
struct energy_table {
	unsigned int mips_per_mhz;
	unsigned int coefficient;
	unsigned int mips_per_mhz_s;
	unsigned int coefficient_s;
	unsigned int static_coefficient;

	struct energy_state *states;

	unsigned int nr_states;
	unsigned int nr_states_orig;
	unsigned int nr_states_requests[NUM_OF_REQUESTS];
};
static DEFINE_PER_CPU(struct energy_table, energy_table);
#define get_energy_table(cpu)	&per_cpu(energy_table, cpu)

__weak int get_gov_next_cap(int dst_cpu, struct task_struct *p)
{
	return -1;
}

static int
default_get_next_cap(int dst_cpu, struct task_struct *p)
{
	struct energy_table *table = get_energy_table(dst_cpu);
	unsigned long next_cap = 0;
	int cpu;

	for_each_cpu(cpu, cpu_coregroup_mask(dst_cpu)) {
		unsigned long util;

		if (cpu == dst_cpu) { /* util with task */
			util = ml_cpu_util_with(cpu, p);

			/* if it is over-capacity, give up eifficiency calculatation */
			if (util > table->states[table->nr_states - 1].cap)
				return 0;
		}
		else /* util without task */
			util = ml_cpu_util_without(cpu, p);

		/*
		 * The cpu in the coregroup has same capacity and the
		 * capacity depends on the cpu with biggest utilization.
		 * Find biggest utilization in the coregroup and use it
		 * as max floor to know what capacity the cpu will have.
		 */
		if (util > next_cap)
			next_cap = util;
	}

	return next_cap;
}

static inline unsigned int
__compute_energy(unsigned long util, unsigned long cap,
				unsigned long dp, unsigned long sp)
{
	if (!util)
		util = 1;

	return 1 + (dp * ((util << SCHED_CAPACITY_SHIFT) / cap))
				+ (sp << SCHED_CAPACITY_SHIFT);
}

static unsigned int
compute_energy(struct energy_table *table, struct task_struct *p,
			int target_cpu, int cap_idx)
{
	unsigned int energy;

	energy = __compute_energy(__ml_cpu_util_with(target_cpu, p, SSE),
				table->states[cap_idx].cap_s,
				table->states[cap_idx].power_s,
				table->states[cap_idx].static_power);
	energy += __compute_energy(__ml_cpu_util_with(target_cpu, p, USS),
				table->states[cap_idx].cap,
				table->states[cap_idx].power,
				table->states[cap_idx].static_power);

	return energy;
}

static unsigned int
compute_efficiency(struct task_struct *p, int target_cpu, unsigned int eff_weight)
{
	struct energy_table *table = get_energy_table(target_cpu);
	unsigned long next_cap = 0;
	unsigned long capacity, util, energy;
	unsigned long long eff;
	unsigned int cap_idx;
	int i;

	/* energy table does not exist */
	if (!table->nr_states)
		return 0;

	/* Get next capacity of cpu in coregroup with task */
	next_cap = get_gov_next_cap(target_cpu, p);
	if ((int)next_cap < 0)
		next_cap = default_get_next_cap(target_cpu, p);

	/* Find the capacity index according to next capacity */
	cap_idx = table->nr_states - 1;
	for (i = 0; i < table->nr_states; i++) {
		if (table->states[i].cap >= next_cap) {
			cap_idx = i;
			break;
		}
	}

	capacity = table->states[cap_idx].cap;
	util = ml_cpu_util_with(target_cpu, p);
	energy = compute_energy(table, p, target_cpu, cap_idx);

	/*
	 * Compute performance efficiency
	 *  efficiency = (capacity / util) / energy
	 */
	eff = (capacity << SCHED_CAPACITY_SHIFT * 2) / energy;
	eff = (eff * eff_weight) / 100;

	trace_ems_compute_eff(p, target_cpu, util, eff_weight,
						capacity, energy, (unsigned int)eff);

	return (unsigned int)eff;
}

static int find_biggest_spare_cpu(int sse,
			struct cpumask *candidates, int *cpu_util_wo)
{
	int cpu, max_cpu = -1, max_cap = 0;

	for_each_cpu(cpu, candidates) {
		int curr_cap;
		int spare_cap;

		/* get current cpu capacity */
		curr_cap = (capacity_cpu(cpu, sse)
				* arch_scale_freq_capacity(cpu)) >> SCHED_CAPACITY_SHIFT;
		spare_cap = curr_cap - cpu_util_wo[cpu];

		if (max_cap < spare_cap) {
			max_cap = spare_cap;
			max_cpu = cpu;
		}
	}
	return max_cpu;
}

static int set_candidate_cpus(struct tp_env *env, int task_util, const struct cpumask *mask,
		struct cpumask *idle_candidates, struct cpumask *active_candidates, int *cpu_util_wo)
{
	int cpu, bind = false;

	if (unlikely(!cpumask_equal(mask, cpu_possible_mask)))
		bind = true;

	for_each_cpu_and(cpu, mask, cpu_active_mask) {
		unsigned long capacity = capacity_cpu(cpu, env->p->sse);

		if (!cpumask_test_cpu(cpu, &env->p->cpus_allowed))
			continue;

		/* remove overfit cpus from candidates */
		if (likely(!bind) && (capacity < (cpu_util_wo[cpu] + task_util)))
			continue;

		if (!cpu_rq(cpu)->nr_running)
			cpumask_set_cpu(cpu, idle_candidates);
		else
			cpumask_set_cpu(cpu, active_candidates);
	}

	return bind;
}

static int find_best_idle(struct tp_env *env)
{
	struct cpumask idle_candidates, active_candidates;
	int cpu_util_wo[NR_CPUS];
	int cpu, best_cpu = -1;
	int task_util, sse = env->p->sse;
	int bind;

	cpumask_clear(&idle_candidates);
	cpumask_clear(&active_candidates);

	task_util = ml_task_util_est(env->p);
	for_each_cpu(cpu, cpu_active_mask) {
		/* get the ml cpu util wo */
		cpu_util_wo[cpu] = _ml_cpu_util(cpu, env->p->sse);
		if (cpu == task_cpu(env->p))
			cpu_util_wo[cpu] = max(cpu_util_wo[cpu] - task_util, 0);
	}

	bind = set_candidate_cpus(env, task_util, emstune_cpus_allowed(env->p),
			&idle_candidates, &active_candidates, cpu_util_wo);

	/* find biggest spare cpu among the idle_candidates */
	best_cpu = find_biggest_spare_cpu(sse, &idle_candidates, cpu_util_wo);
	if (best_cpu >= 0) {
		trace_ems_find_best_idle(env->p, task_util,
			*(unsigned int *)cpumask_bits(&idle_candidates),
			*(unsigned int *)cpumask_bits(&active_candidates), bind, best_cpu);
		return best_cpu;
	}

	/* find biggest spare cpu among the active_candidates */
	best_cpu = find_biggest_spare_cpu(sse, &active_candidates, cpu_util_wo);
	if (best_cpu >= 0) {
		trace_ems_find_best_idle(env->p, task_util,
			*(unsigned int *)cpumask_bits(&idle_candidates),
			*(unsigned int *)cpumask_bits(&active_candidates), bind, best_cpu);
		return best_cpu;
	}

	/* if there is no best_cpu, return previous cpu */
	best_cpu = task_cpu(env->p);
	trace_ems_find_best_idle(env->p, task_util,
		*(unsigned int *)cpumask_bits(&idle_candidates),
		*(unsigned int *)cpumask_bits(&active_candidates), bind, best_cpu);

	return best_cpu;
}

static unsigned int
compute_performance(struct task_struct *p, int target_cpu)
{
	struct energy_table *table = get_energy_table(target_cpu);
	unsigned long next_cap = 0;
	unsigned int cap_idx;
	int i, idle_state_idx;

	/* energy table does not exist */
	if (!table->nr_states)
		return 0;

	/* Get next capacity of cpu in coregroup with task */
	next_cap = get_gov_next_cap(target_cpu, p);
	if ((int)next_cap < 0)
		next_cap = default_get_next_cap(target_cpu, p);

	/* Find the capacity index according to next capacity */
	cap_idx = table->nr_states - 1;
	for (i = 0; i < table->nr_states; i++) {
		if (table->states[i].cap >= next_cap) {
			cap_idx = i;
			break;
		}
	}

	rcu_read_lock();
	idle_state_idx = idle_get_state_idx(cpu_rq(target_cpu));
	rcu_read_unlock();

	trace_ems_compute_perf(p, target_cpu, table->states[cap_idx].cap, idle_state_idx);

	return table->states[cap_idx].cap - idle_state_idx;
}

static int
find_best_eff(struct tp_env *env, int *eff, int idle)
{
	struct cpumask candidates;
	unsigned int best_eff = 0;
	int best_cpu = -1;
	int cpu;
	int prefer_idle = 0;

	if (idle) {
		cpumask_copy(&candidates, &env->idle_candidates);
		prefer_idle = env->prefer_idle;
	} else
		cpumask_copy(&candidates, &env->candidates);

	if (cpumask_empty(&candidates))
		return -1;

	/* find best efficiency cpu */
	for_each_cpu(cpu, &candidates) {
		unsigned int eff;

		if (prefer_idle)
			eff = compute_performance(env->p, cpu);
		else
			eff = compute_efficiency(env->p, cpu, env->eff_weight[cpu]);

		if (eff > best_eff) {
			best_eff = eff;
			best_cpu = cpu;
		}
	}

	*eff = best_eff;

	return best_cpu;
}

int find_best_cpu(struct tp_env *env)
{
	unsigned int best_eff = 0, best_idle_eff = 0;
	int best_cpu, best_idle_cpu;

	if (env->prefer_idle) {
		best_idle_cpu = find_best_idle(env);
		if (best_idle_cpu >= 0)
			return best_idle_cpu;
	}

	/*
	 * Find best cpu among idle cpus.
	 * If prefer-idle is enabled in the current EMS mode, the idle cpu is
	 * selected first.
	 */
	best_idle_cpu = find_best_eff(env, &best_idle_eff, 1);
	if (env->prefer_idle && best_idle_cpu >= 0)
		return best_idle_cpu;

	/*
	 * Find best cpu among running cpus.
	  */
	best_cpu = find_best_eff(env, &best_eff, 0);

	/*
	 * Pick more efficient cpu between best_cpu and best_idle_cpu
	 */
	if (best_idle_eff >= best_eff)
		best_cpu = best_idle_cpu;

	return best_cpu;
}

/*
 * returns allowed capacity base on the allowed power
 * freq: base frequency to find base_power
 * power: allowed_power = base_power + power
 */
int find_allowed_capacity(int cpu, unsigned int freq, int power)
{
	struct energy_table *table = get_energy_table(cpu);
	unsigned long new_power = 0;
	int i, max_idx = table->nr_states - 1;

	if (max_idx < 0)
		return 0;

	/* find power budget for new frequency */
	for (i = 0; i < max_idx; i++)
		if (table->states[i].frequency >= freq)
			break;

	/* calaculate new power budget */
	new_power = table->states[i].power + power;

	/* find minimum freq over the new power budget */
	for (i = 0; i < table->nr_states; i++)
		if (table->states[i].power >= new_power)
			return table->states[i].cap;

	/* return max capacity */
	return table->states[max_idx].cap;
}

int find_step_power(int cpu, int step)
{
	struct energy_table *table = get_energy_table(cpu);
	int max_idx = table->nr_states - 1;

	if (!step || max_idx < 0)
		return 0;

	return (table->states[max_idx].power - table->states[0].power) / step;
}

/*
 * Information of per_cpu cpu capacity variable
 *
 * cpu_capacity_orig{_s}
 * : Original capacity of cpu. It never be changed since initialization.
 *
 * cpu_capacity{_s}
 * : Capacity of cpu. It is same as cpu_capacity_orig normally but it can be
 *   changed by CPU frequency restriction.
 *
 * cpu_capacity_ratio{_s}
 * : Ratio between capacity of sse and uss. It is used for calculating
 *   cpu utilization in Multi Load for optimization.
 */
static DEFINE_PER_CPU(unsigned long, cpu_capacity_orig) = SCHED_CAPACITY_SCALE;
static DEFINE_PER_CPU(unsigned long, cpu_capacity_orig_s) = SCHED_CAPACITY_SCALE;

static DEFINE_PER_CPU(unsigned long, cpu_capacity) = SCHED_CAPACITY_SCALE;
static DEFINE_PER_CPU(unsigned long, cpu_capacity_s) = SCHED_CAPACITY_SCALE;

static DEFINE_PER_CPU(unsigned long, cpu_capacity_ratio) = SCHED_CAPACITY_SCALE;
static DEFINE_PER_CPU(unsigned long, cpu_capacity_ratio_s) = SCHED_CAPACITY_SCALE;

unsigned long capacity_cpu_orig(int cpu, int sse)
{
	return sse ? per_cpu(cpu_capacity_orig_s, cpu) :
			per_cpu(cpu_capacity_orig, cpu);
}

unsigned long capacity_cpu(int cpu, int sse)
{
	return sse ? per_cpu(cpu_capacity_s, cpu) : per_cpu(cpu_capacity, cpu);
}

unsigned long capacity_ratio(int cpu, int sse)
{
	return sse ? per_cpu(cpu_capacity_ratio_s, cpu) : per_cpu(cpu_capacity_ratio, cpu);
}

static int sched_cpufreq_policy_callback(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;

	if (event != CPUFREQ_NOTIFY)
		return NOTIFY_DONE;

	/*
	 * When policy->max is pressed, the performance of the cpu is restricted.
	 * In the restricted state, the cpu capacity also changes, and the
	 * overutil condition changes accordingly, so the cpu capcacity is updated
	 * whenever policy is changed.
	 */
	rebuild_sched_energy_table(policy->related_cpus, policy->max,
					policy->cpuinfo.max_freq, STATES_FREQ);

	return NOTIFY_OK;
}

static struct notifier_block sched_cpufreq_policy_notifier = {
	.notifier_call = sched_cpufreq_policy_callback,
};

static void
fill_frequency_table(struct energy_table *table, int table_size,
			unsigned long *f_table, int max_f, int min_f)
{
	int i, index = 0;

	for (i = table_size - 1; i >=0; i--) {
		if (f_table[i] > max_f || f_table[i] < min_f)
			continue;

		table->states[index].frequency = f_table[i];
		index++;
	}
}

static void
fill_power_table(struct energy_table *table, int table_size,
			unsigned long *f_table, unsigned int *v_table,
			int max_f, int min_f)
{
	int i, index = 0;
	int c = table->coefficient, c_s = table->coefficient_s, v;
	int static_c = table->static_coefficient;
	unsigned long f, power, power_s, static_power;

	/* energy table and frequency table are inverted */
	for (i = table_size - 1; i >= 0; i--) {
		if (f_table[i] > max_f || f_table[i] < min_f)
			continue;

		f = f_table[i] / 1000;	/* KHz -> MHz */
		v = v_table[i] / 1000;	/* uV -> mV */

		/*
		 * power = coefficent * frequency * voltage^2
		 */
		power = c * f * v * v;
		power_s = c_s * f * v * v;
		static_power = static_c * v * v;

		/*
		 * Generally, frequency is more than treble figures in MHz and
		 * voltage is also more then treble figures in mV, so the
		 * calculated power is larger than 10^9. For convenience of
		 * calculation, divide the value by 10^9.
		 */
		do_div(power, 1000000000);
		do_div(power_s, 1000000000);
		do_div(static_power, 1000000);
		table->states[index].power = power;
		table->states[index].power_s = power_s;
		table->states[index].static_power = static_power;

		index++;
	}
}

static void
fill_cap_table(struct energy_table *table, unsigned long max_mips)
{
	int i;
	int mpm = table->mips_per_mhz;
	int mpm_s = table->mips_per_mhz_s;
	unsigned long f;

	for (i = 0; i < table->nr_states; i++) {
		f = table->states[i].frequency;

		/*
		 *     mips(f) = f * mips_per_mhz
		 * capacity(f) = mips(f) / max_mips * 1024
		 */
		table->states[i].cap = f * mpm * 1024 / max_mips;
		table->states[i].cap_s = f * mpm_s * 1024 / max_mips;
	}
}

static void print_energy_table(struct energy_table *table, int cpu)
{
	int i;

	pr_info("[Energy Table: cpu%d]\n", cpu);
	for (i = 0; i < table->nr_states; i++) {
		pr_info("[%2d] cap=%4lu power=%4lu | cap(S)=%4lu power(S)=%4lu | static-power=%4lu\n",
			i, table->states[i].cap, table->states[i].power,
			table->states[i].cap_s, table->states[i].power_s,
			table->states[i].static_power);
	}
}

static DEFINE_SPINLOCK(rebuilding_lock);

static inline int
find_nr_states(struct energy_table *table, int clipped_freq)
{
	int i;

	for (i = table->nr_states_orig - 1; i >= 0; i--) {
		if (table->states[i].frequency <= clipped_freq)
			break;
	}

	return i + 1;
}

static inline int
find_min_nr_states(struct energy_table *table)
{
	int i, min = table->nr_states_orig;

	for (i = 0; i < NUM_OF_REQUESTS; i++) {
		if (table->nr_states_requests[i] < min)
			min = table->nr_states_requests[i];
	}

	return min;
}

static bool
update_nr_states(struct cpumask *cpus, int clipped_freq, int max_freq, int type)
{
	struct energy_table *table, *cursor;
	int cpu, nr_states, nr_states_request;

	if (type >= NUM_OF_REQUESTS)
		return false;

	table = get_energy_table(cpumask_any(cpus));
	if (!table || !table->states)
		return false;

	/* find new nr_states for requester */
	nr_states_request = find_nr_states(table, clipped_freq);
	if (!nr_states_request)
		return false;

	for_each_cpu(cpu, cpus) {
		cursor = get_energy_table(cpu);
		cursor->nr_states_requests[type] = nr_states_request;
	}

	/*
	 * find min nr_states among nr_states of requesters
	 * If nr_states in energy table is not changed, skip rebuilding.
	 */
	nr_states = find_min_nr_states(table);
	if (nr_states == table->nr_states)
		return false;

	/* Update clipped state of all cpus which are clipped */
	for_each_cpu(cpu, cpus) {
		cursor = get_energy_table(cpu);
		cursor->nr_states = nr_states;
	}

	return true;
}

static unsigned long find_max_mips(void)
{
	struct energy_table *table;
	unsigned long max_mips = 0;
	int cpu;

	/*
	 * Find fastest cpu among the cpu to which the energy table is allocated.
	 * The mips and max frequency of fastest cpu are needed to calculate
	 * capacity.
	 */
	for_each_possible_cpu(cpu) {
		unsigned long max_f, mpm;
		unsigned long mips;

		table = get_energy_table(cpu);
		if (!table->states)
			continue;

		/* max mips = max_f * mips_per_mhz */
		max_f = table->states[table->nr_states - 1].frequency;
		mpm = max(table->mips_per_mhz, table->mips_per_mhz_s);
		mips = max_f * mpm;
		if (mips > max_mips)
			max_mips = mips;
	}

	return max_mips;
}

static void
update_capacity(struct energy_table *table, int cpu, bool init)
{
	int last_state = table->nr_states - 1;
	unsigned long ratio;
	struct sched_domain *sd;

	if (last_state < 0)
		return;

	/*
	 * update cpu_capacity_orig{_s},
	 * this value never changes after initialization.
	 */
	if (init) {
		per_cpu(cpu_capacity_orig_s, cpu) = table->states[last_state].cap_s;
		per_cpu(cpu_capacity_orig, cpu) = table->states[last_state].cap;
	}

	/* update cpu_capacity{_s} */
	per_cpu(cpu_capacity_s, cpu) = table->states[last_state].cap_s;
	per_cpu(cpu_capacity, cpu) = table->states[last_state].cap;

	/* update cpu_capacity_ratio{_s} */
	ratio = (per_cpu(cpu_capacity, cpu) << SCHED_CAPACITY_SHIFT);
	ratio /= per_cpu(cpu_capacity_s, cpu);
	per_cpu(cpu_capacity_ratio, cpu) = ratio;
	ratio = (per_cpu(cpu_capacity_s, cpu) << SCHED_CAPACITY_SHIFT);
	ratio /= per_cpu(cpu_capacity, cpu);
	per_cpu(cpu_capacity_ratio_s, cpu) = ratio;

	/* announce capacity update to cfs */
	topology_set_cpu_scale(cpu, table->states[last_state].cap);
	rcu_read_lock();
	for_each_domain(cpu, sd)
		update_group_capacity(sd, cpu);
	rcu_read_unlock();
}

static ssize_t show_energy_table(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int cpu, i;
	int ret = 0;

	for_each_possible_cpu(cpu) {
		struct energy_table *table;

		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "[Energy Table: cpu%d]\n", cpu);

		table = get_energy_table(cpu);
		for (i = 0; i < table->nr_states; i++) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cap=%4lu power=%4lu | cap(S)=%4lu power(S)=%4lu | static-power=%4lu\n",
				table->states[i].cap, table->states[i].power,
				table->states[i].cap_s, table->states[i].power_s,
				table->states[i].static_power);
		}

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	return ret;
}

static struct kobj_attribute energy_table_attr =
__ATTR(energy_table, 0444, show_energy_table, NULL);

static void
__rebuild_sched_energy_table(void)
{
	struct energy_table *table;
	int cpu, max_mips = 0;

	max_mips = find_max_mips();
	for_each_possible_cpu(cpu) {
		table = get_energy_table(cpu);
		if (!table->states)
			continue;

		fill_cap_table(table, max_mips);
		update_capacity(table, cpu, false);
	}
}

/*
 * Because capacity is a relative value computed based on max_mips, it must be
 * recalculated if maximum mips is changed. If maximum mips changes when max
 * frequency is changed by policy->max or pm_qos, recalculate capacity of
 * energy table.
 */
void rebuild_sched_energy_table(struct cpumask *cpus,
				int clipped_freq,
				int max_freq, int type)
{
	spin_lock(&rebuilding_lock);

	/*
	 * Update nr_states in energy table depending on clipped_freq.
	 * If there's no update, skip rebuilding energy table.
	 */
	if (!update_nr_states(cpus, clipped_freq, max_freq, type))
		goto unlock;

	__rebuild_sched_energy_table();
unlock:
	spin_unlock(&rebuilding_lock);
}

/*
 * Whenever frequency domain is registered, and energy table corresponding to
 * the domain is created. Because cpu in the same frequency domain has the same
 * energy table. Capacity is calculated based on the max frequency of the fastest
 * cpu, so once the frequency domain of the faster cpu is regsitered, capacity
 * is recomputed.
 */
void init_sched_energy_table(struct cpumask *cpus, int table_size,
				unsigned long *f_table, unsigned int *v_table,
				int max_f, int min_f)
{
	struct energy_table *table;
	int i, cpu, valid_table_size = 0;
	unsigned long max_mips;

	/* get size of valid frequency table to allocate energy table */
	for (i = 0; i < table_size; i++) {
		if (f_table[i] > max_f || f_table[i] < min_f)
			continue;

		valid_table_size++;
	}

	/* there is no valid row in the table, energy table is not created */
	if (!valid_table_size)
		return;

	/* allocate memory for energy table and fill frequency */
	for_each_cpu(cpu, cpus) {
		table = get_energy_table(cpu);
		table->states = kcalloc(valid_table_size,
				sizeof(struct energy_state), GFP_KERNEL);
		if (unlikely(!table->states))
			return;

		table->nr_states = valid_table_size;
		fill_frequency_table(table, table_size, f_table, max_f, min_f);
	}

	/*
	 * Because 'capacity' column of energy table is a relative value, the
	 * previously configured capacity of energy table can be reconfigurated
	 * based on the maximum mips whenever new cpu's energy table is
	 * initialized. On the other hand, since 'power' column of energy table
	 * is an obsolute value, it needs to be configured only once at the
	 * initialization of the energy table.
	 */
	max_mips = find_max_mips();
	for_each_possible_cpu(cpu) {
		table = get_energy_table(cpu);
		if (!table->states)
			continue;

		/*
		 * 1. fill power column of energy table only for the cpu that
		 *    initializes the energy table.
		 */
		if (cpumask_test_cpu(cpu, cpus))
			fill_power_table(table, table_size,
					f_table, v_table, max_f, min_f);

		/* 2. fill capacity column of energy table */
		fill_cap_table(table, max_mips);

		/* 3. update per-cpu capacity variable */
		update_capacity(table, cpu, true);

		print_energy_table(get_energy_table(cpu), cpu);
	}

	topology_update();
}

static int __init init_sched_energy_data(void)
{
	struct device_node *cpu_node, *cpu_phandle;
	int cpu, ret;

	for_each_possible_cpu(cpu) {
		struct energy_table *table;

		cpu_node = of_get_cpu_node(cpu, NULL);
		if (!cpu_node) {
			pr_warn("CPU device node missing for CPU %d\n", cpu);
			return -ENODATA;
		}

		cpu_phandle = of_parse_phandle(cpu_node, "sched-energy-data", 0);
		if (!cpu_phandle) {
			pr_warn("CPU device node has no sched-energy-data\n");
			return -ENODATA;
		}

		table = get_energy_table(cpu);
		if (of_property_read_u32(cpu_phandle, "mips-per-mhz", &table->mips_per_mhz)) {
			pr_warn("No mips-per-mhz data\n");
			return -ENODATA;
		}

		if (of_property_read_u32(cpu_phandle, "power-coefficient", &table->coefficient)) {
			pr_warn("No power-coefficient data\n");
			return -ENODATA;
		}

		if (of_property_read_u32(cpu_phandle, "static-power-coefficient",
								&table->static_coefficient)) {
			pr_warn("No static-power-coefficient data\n");
			return -ENODATA;
		}

		/*
		 * Data for sse is OPTIONAL.
		 * If it does not fill sse data, sse table and uss table are same.
		 */
		if (of_property_read_u32(cpu_phandle, "mips-per-mhz-s", &table->mips_per_mhz_s))
			table->mips_per_mhz_s = table->mips_per_mhz;
		if (of_property_read_u32(cpu_phandle, "power-coefficient-s", &table->coefficient_s))
			table->coefficient_s = table->coefficient;

		of_node_put(cpu_phandle);
		of_node_put(cpu_node);

		pr_info("cpu%d mips_per_mhz=%d coefficient=%d mips_per_mhz_s=%d coefficient_s=%d static_coefficient=%d\n",
			cpu, table->mips_per_mhz, table->coefficient,
			table->mips_per_mhz_s, table->coefficient_s, table->static_coefficient);
	}

	cpufreq_register_notifier(&sched_cpufreq_policy_notifier, CPUFREQ_POLICY_NOTIFIER);

	ret = sysfs_create_file(ems_kobj, &energy_table_attr.attr);
	if (ret)
		pr_warn("%s: failed to create sysfs\n", __func__);

	return 0;
}
core_initcall(init_sched_energy_data);
