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

DEFINE_PER_CPU(struct energy_table, energy_table);

__weak int get_gov_next_cap(int grp_cpu, int dst_cpu, struct tp_env *env)
{
	return -1;
}

int
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

struct wake_wide {
	struct cpumask	mask;
	unsigned long	last_wake_wide;
	int		sch;	/* sibling_count_hint */
};
struct wake_wide ems_ww;

static void ww_init(int sch)
{
	int cpu;

	ems_ww.sch = sch;
	ems_ww.last_wake_wide = jiffies;

	cpumask_clear(&ems_ww.mask);

	while(sch--) {
		int min_util = INT_MAX, min_cpu = 0;

		for_each_cpu(cpu, cpu_active_mask) {
			int cpu_util;
			if(cpumask_test_cpu(cpu, &ems_ww.mask))
				continue;

			cpu_util = ml_cpu_util(cpu);
			if (cpu_util < min_util) {
				min_util = cpu_util;
				min_cpu = cpu;
			}
		}
		cpumask_set_cpu(min_cpu, &ems_ww.mask);
	}

	trace_ww_init(ems_ww.sch, *(unsigned int *)cpumask_bits(&ems_ww.mask));
}

int find_wide_cpu(struct tp_env *env, int sch)
{
	int cpu, best_cpu = -1, min_spare = INT_MAX;
	int max_cpu = 0, max_spare = 0;
	int task_util = ml_task_util_est(env->p);

	/* init wake_wide */
	if (!ems_ww.sch || (jiffies - ems_ww.last_wake_wide) > HZ * 2)
		ww_init(sch);

	/* find minimum spare fit cpu(min spare cpu) */
	for_each_cpu(cpu, &ems_ww.mask) {
		int cpu_cap = capacity_cpu(cpu, env->p->sse);
		int cpu_util = ml_cpu_util_without(cpu, env->p);
		int cpu_spare = cpu_cap - cpu_util;

		/* pre-find max spare cpu */
		if (cpu_spare > max_spare) {
			max_spare = cpu_spare;
			max_cpu = cpu;
		}

		if (cpu_spare + task_util > ((cpu_cap * 5) / 4))
			continue;

		if (task_cpu(env->p) == cpu) {
			best_cpu = cpu;
			continue;
		}

		if (cpu_spare < min_spare) {
			min_spare = cpu_spare;
			best_cpu = cpu;
		}
	}

	/* select maximum spare cpu */
	if (best_cpu == -1)
		best_cpu = max_cpu;

	cpumask_clear_cpu(best_cpu, &ems_ww.mask);
	ems_ww.sch--;

	if (!cpumask_test_cpu(best_cpu, cpu_active_mask))
		best_cpu = -1;

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
	int max_idx = table->nr_states_orig - 1;

	if (!step || max_idx < 0)
		return 0;

	return (table->states[max_idx].power - table->states[0].power) / step;
}

static unsigned int get_table_index(int cpu, unsigned int freq)
{
	struct energy_table *table = get_energy_table(cpu);
	unsigned int i;
	for (i = 0; i < table->nr_states; i++)
		if (table->states[i].frequency >= freq)
			return i;
	return 0;
}

unsigned int get_diff_num_levels(int cpu, unsigned int freq1, unsigned int freq2)
{
	unsigned int index1, index2;
	index1 = get_table_index(cpu, freq1);
	index2 = get_table_index(cpu, freq2);
	return abs(index1 - index2);
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

static unsigned long scale_rt_capacity(int cpu, unsigned long max, int sse)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long used, free;
	unsigned long irq;

	irq = cpu_util_irq(rq);

	if (unlikely(irq >= max))
		return 1;

	used = READ_ONCE(rq->avg_rt.util_avg);
	used += READ_ONCE(rq->avg_dl.util_avg);

	if (sse)
		used = (used * capacity_ratio(cpu, SSE)) >> SCHED_CAPACITY_SHIFT;

	if (unlikely(used >= max))
		return 1;

	free = max - used;

	return scale_irq_capacity(free, irq, max);
}

static void
__update_capacity(struct energy_table *table, int cpu, bool init)
{
	int last_state = table->nr_states - 1;
	unsigned long cpu_scale, ratio;
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
	cpu_scale = scale_rt_capacity(cpu, table->states[last_state].cap_s, SSE);
	per_cpu(cpu_capacity_s, cpu) = cpu_scale;
	cpu_scale = scale_rt_capacity(cpu, table->states[last_state].cap, USS);
	per_cpu(cpu_capacity, cpu) = cpu_scale;

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

void update_capacity(int cpu, bool init)
{
	struct energy_table *table = get_energy_table(cpu);

	if (!table->states)
		return;

	__update_capacity(table, cpu, init);
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
	unsigned long max_mips = 0;
	int cpu;

	max_mips = find_max_mips();
	for_each_possible_cpu(cpu) {
		table = get_energy_table(cpu);
		if (!table->states)
			continue;

		fill_cap_table(table, max_mips);
		__update_capacity(table, cpu, false);
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

		table->nr_states = table->nr_states_orig = valid_table_size;

		for (i = 0; i < NUM_OF_REQUESTS; i++)
			table->nr_states_requests[i] = valid_table_size;

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
		__update_capacity(table, cpu, true);

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
