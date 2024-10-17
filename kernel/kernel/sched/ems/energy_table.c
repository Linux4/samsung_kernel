/*
 * Energy efficient cpu selection
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/pm_opp.h>
#include <linux/thermal.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/cal-if.h>

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include "ems.h"

static int enable_ipc_selection;
static unsigned int et_default_index;

struct field {
	int size;
	unsigned int min;
	unsigned int *list;
};

struct constraint {
	unsigned int cpu_freq;
	unsigned int dsu_freq;
};

#define MAX_ET 10

/*
 * Each cpu can have its own mips_per_mhz, coefficient and energy table.
 * Generally, cpus in the same frequency domain have the same mips_per_mhz,
 * coefficient and energy table.
 */
static struct energy_table {
	struct cpumask cpus;

	struct energy_state **states;
	struct energy_state *default_state;
	unsigned int nr_table;
	unsigned int nr_states;

	unsigned int mips_mhz[MAX_ET];
	unsigned int dynamic_coeff[MAX_ET];
	int dynamic_intercept[MAX_ET];

	struct cpumask pd_cpus;

	unsigned long cur_freq;	/* requested by governor, NOT real frequency */
	unsigned long cur_volt;	/* voltage matched with cur_freq */
	unsigned int cur_index;	/* real current frequency index */

	struct constraint **constraints;
	int nr_constraint;
} __percpu **energy_table;

static struct dsu_energy_table {
	struct energy_state *states;
	unsigned int nr_states;

	unsigned int dynamic_coeff;

	struct cpumask pd_cpus;
} *dsu_table;

#define per_cpu_et(cpu)		(*per_cpu_ptr(energy_table, cpu))

static int get_cap_index(struct energy_state *states, int size, unsigned long cap)
{
	int i;

	if (size == 0)
		return -1;

	for (i = 0; i < size; i++)
		if (states[i].capacity >= cap)
			break;

	return min(i, size - 1);
}

static int get_freq_index(struct energy_state *states, int size, unsigned long freq)
{
	int i;

	if (size == 0)
		return -1;

	for (i = 0; i < size; i++)
		if (states[i].frequency >= freq)
			break;

	return min(i, size - 1);
}
/****************************************************************************************
 *					Helper functions				*
 ****************************************************************************************/
static int emstune_index = 0;
static unsigned long get_needed_dsu_freq(struct energy_table *table, unsigned long cpu_freq)
{
	struct constraint *constraint;
	unsigned long dsu_freq = 0;
	int i;

	if (unlikely(table->nr_constraint == 0)
			|| unlikely(table->nr_constraint <= emstune_index))
		return dsu_freq;

	constraint = table->constraints[emstune_index];
	if (unlikely(!constraint))
		return dsu_freq;

	for (i = 0; i < table->nr_states; i++) {
		if (constraint[i].cpu_freq >= cpu_freq)
			dsu_freq = constraint[i].dsu_freq;
	}

	return dsu_freq;
}

static unsigned long get_dynamic_power(unsigned long f, unsigned long v, unsigned long c, long i)
{
	/* dynamic power = coefficent * frequency * voltage^2 + intercept */
	unsigned long power;

	f /= 1000;
	power = c * f * v * v;

	/*
	 * f_mhz is more than 3 digits and volt is also more than 3 digits,
	 * so calculated power is more than 9 digits. For convenience of
	 * calculation, divide the value by 10^9.
	 */
	do_div(power, 1000000000);

	return ((long)power + i) > 0 ? power + i : 1;
}

static unsigned long compute_dsu_energy(struct energy_state *state)
{
	unsigned long dp, e;

	if (unlikely(!dsu_table))
		return 0;

	dp = get_dynamic_power(state->frequency, state->voltage, dsu_table->dynamic_coeff, 0);
	e = dp << SCHED_CAPACITY_SHIFT;

	trace_ems_compute_dsu_energy(state->frequency, state->voltage, dp, e);

	return e;
}

static unsigned long __compute_cpu_energy(struct energy_state *state, int cpu)
{
	unsigned long dp, e;

	dp = state->dynamic_power;
	e = (dp << SCHED_CAPACITY_SHIFT) * state->util / state->capacity;
	e = e * 100 / state->weight;

	trace_ems_compute_energy(cpu, state->util, state->capacity,
			state->frequency, state->voltage, dp, e);

	return e;
}

static unsigned long compute_cpu_energy(const struct cpumask *cpus, struct energy_state *states,
		int target_cpu, struct energy_backup *backup)
{
	unsigned long energy = 0;
	int cpu;

	for_each_cpu(cpu, cpus) {
		struct energy_state *state = &states[cpu];

		/*
		 * If this cpu is target or backup is null, do not use backup energy.
		 */
		if (cpu == target_cpu || !backup) {
			energy += __compute_cpu_energy(state, cpu);
			continue;
		}

		if (backup[cpu].voltage != state->voltage) {
			backup[cpu].energy = __compute_cpu_energy(state, cpu);
			backup[cpu].voltage = state->voltage;
		}

		energy += backup[cpu].energy;
	}

	return energy;
}

static void update_energy_state(const struct cpumask *cpus,
		struct energy_state *states, struct energy_state *dsu_state)
{
	struct energy_table *table;
	unsigned long cpu_volt, cpu_freq;
	unsigned long dsu_volt, dsu_freq = 0;
	int cpu, rep_cpu, index;

	if (unlikely(!dsu_table))
		return;

	if (unlikely(!dsu_table->states))
		return;

	/* 1. Find DSU frequency */
	for_each_possible_cpu(cpu) {
		table = per_cpu_et(cpu);
		if (cpu != cpumask_first(&table->cpus))
			continue;

		cpu_freq = states[cpu].frequency ? states[cpu].frequency : table->cur_freq;
		dsu_freq = max(dsu_freq, get_needed_dsu_freq(table, cpu_freq));
	}

	/* 2. Find DSU voltage */
	index = get_freq_index(dsu_table->states, dsu_table->nr_states, dsu_freq);
	if (index < 0)
		return;
	dsu_volt = dsu_table->states[index].voltage;

	/* 3. Apply DSU voltage to CPU */
	for_each_cpu_and(cpu, cpus, &dsu_table->pd_cpus)
		states[cpu].voltage = max(states[cpu].voltage, dsu_volt);

	/* 4. Apply CPU voltage of same pd-cpus */
	for_each_cpu(cpu, cpus) {
		table = per_cpu_et(cpu);
		if (cpu != cpumask_first(&table->cpus))
			continue;

		if (!cpumask_weight(&table->pd_cpus))
			continue;

		for_each_cpu(rep_cpu, &table->pd_cpus) {
			cpu_volt = states[rep_cpu].voltage ?
				states[rep_cpu].voltage : per_cpu_et(rep_cpu)->cur_volt;
			cpu_volt = max(states[cpu].voltage, cpu_volt);
		}

		for_each_cpu(rep_cpu, &table->cpus)
			states[rep_cpu].voltage = cpu_volt;
	}

	/* If dsu_state is NULL, don't need to update dsu_state */
	if (!dsu_state)
		return;

	/* 6. Apply CPU voltage to DSU */
	for_each_cpu(cpu, &dsu_table->pd_cpus) {
		table = per_cpu_et(cpu);
		if (cpu != cpumask_first(&table->cpus))
			continue;

		cpu_volt = states[cpu].voltage ? states[cpu].voltage : table->cur_volt;
		dsu_volt = max(dsu_volt, cpu_volt);
	}

	dsu_state->frequency = dsu_freq;
	dsu_state->voltage = dsu_volt;
}

int et_get_table_index_by_ipc(struct tp_env *env)
{
	int prev_cpu = env->prev_cpu;
	int task_ipc = env->ipc;
	int idx;
	struct energy_table *table = per_cpu_et(prev_cpu);

	if (!enable_ipc_selection)
		return et_default_index;

	for (idx = 0; idx < table->nr_table; idx++) {
		int cur = idx;
		int next = idx + 1;
		unsigned int ipc_avg;

		if (next == table->nr_table)
			return cur;

		if (task_ipc > table->mips_mhz[next])
			continue;

		ipc_avg = (table->mips_mhz[cur] + table->mips_mhz[next]) / 2;
		if (task_ipc <= ipc_avg)
			return cur;
		else
			return next;
	}

	return et_default_index;
}

/****************************************************************************************
 *					Extern APIs					*
 ****************************************************************************************/
unsigned int et_cur_freq_idx(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (unlikely(!table))
		return 0;

	if (unlikely(table->cur_index < 0))
		return 0;

	return table->cur_index;
}
EXPORT_SYMBOL_GPL(et_cur_freq_idx);

unsigned long et_cur_cap(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (unlikely(!table))
		return 0;

	if (unlikely(table->cur_index < 0))
		return 0;

	if (unlikely(!table->default_state))
		return 0;

	return table->default_state[table->cur_index].capacity;
}

unsigned long et_max_cap(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->default_state)
		return 0;

	return table->default_state[table->nr_states - 1].capacity;
}

unsigned long et_freq_to_cap(int cpu, unsigned long freq)
{
	struct energy_table *table = per_cpu_et(cpu);
	int index;

	if (unlikely(!table->default_state))
		return 0;

	index = get_freq_index(table->default_state, table->nr_states, freq);
	if (index < 0)
		return 0;

	return table->default_state[index].capacity;
}

void et_update_freq(int cpu, unsigned long cpu_freq)
{
	struct energy_table *table = per_cpu_et(cpu);
	int index;

	if (unlikely(!table->default_state))
		return;

	index = get_freq_index(table->default_state, table->nr_states, cpu_freq);
	if (index < 0)
		return;

	table->cur_freq = cpu_freq;
	table->cur_volt = table->default_state[index].voltage;
}

void et_fill_energy_state(struct tp_env *env, struct cpumask *cpus,
		struct energy_state *states, unsigned long capacity, int dst_cpu)
{
	int index, cpu = cpumask_any(cpus);
	struct energy_table *table = per_cpu_et(cpu);
	int table_index = et_default_index;

	if (unlikely(!table->default_state))
		return;

	index = get_cap_index(table->states[table_index], table->nr_states, capacity);
	if (index < 0)
		return;

	table_index = env->table_index;
	for_each_cpu(cpu, cpus) {
		states[cpu].capacity = table->states[table_index][index].capacity;
		states[cpu].frequency = table->states[table_index][index].frequency;
		states[cpu].voltage = table->states[table_index][index].voltage;
		states[cpu].dynamic_power = table->states[table_index][index].dynamic_power;

		if (cpu == dst_cpu)
			states[cpu].util = env->cpu_stat[cpu].util_with;
		else
			states[cpu].util = env->cpu_stat[cpu].util_wo;
		states[cpu].weight = env->weight[cpu];
	}
}

unsigned long et_compute_system_energy(const struct list_head *csd_head,
		struct energy_state *states, int target_cpu, struct energy_backup *backup)
{
	struct energy_state dsu_state;
	struct cs_domain *csd;
	unsigned long energy = 0;

	update_energy_state(cpu_possible_mask, states, &dsu_state);

	energy += compute_dsu_energy(&dsu_state);

	list_for_each_entry(csd, csd_head, list)
		energy += compute_cpu_energy(&csd->cpus, states, target_cpu, backup);

	return energy;
}

/****************************************************************************************
 *					CPUFREQ Change VH				*
 ****************************************************************************************/
void et_arch_set_freq_scale(const struct cpumask *cpus,
		unsigned long freq,  unsigned long max, unsigned long *scale)
{
	struct energy_table *table = per_cpu_et(cpumask_first(cpus));
	int index = get_freq_index(table->default_state, table->nr_states, freq);

	if (index > -1)
		table->cur_index = index;
}
EXPORT_SYMBOL_GPL(et_arch_set_freq_scale);

/****************************************************************************************
 *					Notifier Call					*
 ****************************************************************************************/
static void update_capacity(int table_index)
{
	struct energy_table *table;
	unsigned long max_mips = 0;
	int cpu, i;

	/*
	 * Since capacity of energy table is relative value, previously
	 * configured capacity can be reconfigurated according to maximum mips
	 * whenever new energy table is built.
	 *
	 * Find max mips among built energy table first, and calculate or
	 * recalculate capacity.
	 */
	for_each_possible_cpu(cpu) {
		unsigned long max_f, mips;

		table = per_cpu_et(cpu);

		if (cpu != cpumask_first(&table->cpus))
			continue;

		if (unlikely(!table->nr_states))
			continue;
		/* max mips = max_f * mips/mhz */
		max_f = table->states[table_index][table->nr_states - 1].frequency;
		mips = max_f * table->mips_mhz[table_index];
		if (mips > max_mips)
			max_mips = mips;
	}

	if (!max_mips)
		return;

	/* Fill energy table with capacity */
	for_each_possible_cpu(cpu) {
		table = per_cpu_et(cpu);

		if (cpu != cpumask_first(&table->cpus))
			continue;

		if (unlikely(!table->nr_states))
			continue;

		for (i = 0; i < table->nr_states; i++) {
			unsigned long capacity;

			/*
			 *     mips(f) = f * mips/mhz
			 * capacity(f) = mips(f) / max_mips * 1024
			 */
			capacity = table->states[table_index][i].frequency * table->mips_mhz[table_index];
			capacity = (capacity << 10) / max_mips;
			table->states[table_index][i].capacity = capacity;
		}
	}

	for_each_possible_cpu(cpu) {
		table = per_cpu_et(cpu);

		if (unlikely(!table->nr_states))
			continue;

		/* Set CPU scale with max capacity of the CPU */
		if (table_index == et_default_index)
			per_cpu(cpu_scale, cpu) = table->default_state[table->nr_states - 1].capacity;
	}
}

static int et_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	int cpu;

	emstune_index = emstune_cpu_dsu_table_index(v);
	emstune_index = max(emstune_index, 0);

	enable_ipc_selection = cur_set->et.uarch_selection;

	if (et_default_index == cur_set->et.default_idx)
		return NOTIFY_OK;

	et_default_index = cur_set->et.default_idx;
	for_each_possible_cpu(cpu) {
		struct energy_table *table = per_cpu_et(cpu);

		if (unlikely(!table))
			continue;

		if (unlikely(!table->default_state))
			continue;

		table->default_state = table->states[et_default_index];
		per_cpu(cpu_scale, cpu) = table->default_state[table->nr_states - 1].capacity;
	}

	return NOTIFY_OK;
}

static struct notifier_block et_emstune_notifier = {
	.notifier_call = et_emstune_notifier_call,
};

/****************************************************************************************
 *					SYSFS						*
 ****************************************************************************************/
#define MSG_SIZE (2048 * VENDOR_NR_CPUS)
static char *sysfs_msg;
static ssize_t energy_table_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t size)
{
	int cpu, i, table_index;
	ssize_t count = 0, msg_size;

	for_each_possible_cpu(cpu) {
		struct energy_table *table = per_cpu_et(cpu);

		if (unlikely(!table) || cpumask_first(&table->cpus) != cpu)
			continue;


		for (table_index = 0; table_index < table->nr_table; table_index++) {
			count += sprintf(sysfs_msg + count, "[Energy Table: cpu%d-%d]\n", cpu, table_index);
			count += sprintf(sysfs_msg + count,
					"+------------+------------+---------------+\n"
					"|  frequency |  capacity  | dynamic power |\n"
					"+------------+------------+---------------+\n");
			for (i = 0; i < table->nr_states; i++) {
				count += sprintf(sysfs_msg + count,
						"| %10lu | %10lu | %13lu |\n",
						table->states[table_index][i].frequency,
						table->states[table_index][i].capacity,
						table->states[table_index][i].dynamic_power);
			}
			count += sprintf(sysfs_msg + count,
					"+------------+------------+---------------+\n");
			count += sprintf(sysfs_msg + count, "[dcoeff:%d | mips:%d | intercept:%d]\n\n",
					table->dynamic_coeff[table_index],
					table->mips_mhz[table_index],
					table->dynamic_intercept[table_index]);
		}
	}
	msg_size = min_t(ssize_t, count, MSG_SIZE);
	msg_size = memory_read_from_buffer(buf, size, &offset, sysfs_msg, msg_size);

	return msg_size;
}
static BIN_ATTR(energy_table, 0440, energy_table_read, NULL, 0);

static ssize_t default_table_idx_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t size)
{
	int cpu;
	ssize_t count = 0, msg_size;

	for_each_possible_cpu(cpu) {
		count += sprintf(sysfs_msg + count, "cpu%d: et_default_index:%u cpu_scale=%lu\n",
			cpu, et_default_index, per_cpu(cpu_scale, cpu));
	}

	msg_size = min_t(ssize_t, count, MSG_SIZE);
	msg_size = memory_read_from_buffer(buf, size, &offset, sysfs_msg, msg_size);

	return msg_size;
}

static ssize_t default_table_idx_write(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t count)
{
	int cpu;
	unsigned int index;

	sscanf(buf, "%u", &index);

	if (index < 0 || index >= MAX_ET)
		return count;	

	et_default_index = index;
	for_each_possible_cpu(cpu) {
		struct energy_table *table = per_cpu_et(cpu);
		table->default_state = table->states[et_default_index];
		per_cpu(cpu_scale, cpu) = table->default_state[table->nr_states - 1].capacity;
	}

	return count;
}
static BIN_ATTR(default_table_idx, 0640, default_table_idx_read, default_table_idx_write, 0);

static ssize_t enable_ipc_selection_read(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t size)
{
	ssize_t count = 0, msg_size;

	count += sprintf(sysfs_msg + count, "%d\n", enable_ipc_selection);

	msg_size = min_t(ssize_t, count, MSG_SIZE);
	msg_size = memory_read_from_buffer(buf, size, &offset, sysfs_msg, msg_size);

	return msg_size;
}

static ssize_t enable_ipc_selection_write(struct file *file, struct kobject *kobj,
		struct bin_attribute *attr, char *buf,
		loff_t offset, size_t count)
{
	int enable;

	sscanf(buf, "%u", &enable);
	enable_ipc_selection = enable;

	return count;
}
static BIN_ATTR(enable_ipc_selection, 0640, enable_ipc_selection_read, enable_ipc_selection_write, 0);

static struct bin_attribute *et_bin_attrs[] = {
	&bin_attr_energy_table,
	&bin_attr_default_table_idx,
	&bin_attr_enable_ipc_selection,
	NULL,
};

static struct attribute_group et_attr_group = {
	.name		= "energy_table",
	.bin_attrs	= et_bin_attrs,
};

/****************************************************************************************
 *					Initialize					*
 ****************************************************************************************/
static struct ect_gen_param_table *get_ect_gen_param_table(char *name)
{
	void *gen_block = ect_get_block("GEN");

	if (!gen_block) {
		pr_err("Failed to get general parameter block from ECT\n");
		return NULL;
	}

	return ect_gen_param_get_table(gen_block, name);
}

void et_register_dsu_constraint(int cpu, void *p, int size)
{
	struct energy_table *table = per_cpu_et(cpu);
	struct constraint *constraint = (struct constraint *)p;
	struct constraint *new_constraint;
	struct constraint **temp;
	int i;

	temp = kcalloc(table->nr_constraint + 1, sizeof(struct constraint *), GFP_KERNEL);
	if (!temp)
		return;

	for (i = 0; i < table->nr_constraint; i++)
		temp[i] = table->constraints[i];

	new_constraint = kcalloc(size, sizeof(struct constraint), GFP_KERNEL);
	if (!new_constraint) {
		kfree(temp);
		return;
	}

	memcpy(new_constraint, constraint, sizeof(struct constraint) * size);
	temp[table->nr_constraint] = new_constraint;

	kfree(table->constraints);
	table->constraints = temp;
	table->nr_constraint++;
}
EXPORT_SYMBOL_GPL(et_register_dsu_constraint);

void et_init_dsu_table(unsigned long *freq_table, unsigned int *volt_table, int size)
{
	int i;

	if (unlikely(!dsu_table))
		pr_err("Failed to init dsu_table\n");

	dsu_table->states = kcalloc(size, sizeof(struct energy_state), GFP_KERNEL);
	if (!dsu_table->states)
		return;

	dsu_table->nr_states = size;

	for (i = 0; i < size; i++) {
		dsu_table->states[i].frequency = freq_table[i];
		dsu_table->states[i].voltage = volt_table[i] / 1000;
	}
}
EXPORT_SYMBOL_GPL(et_init_dsu_table);

/*
 * Whenever frequency domain is registered, and energy table corresponding to
 * the domain is created. Because cpu in the same frequency domain has the same
 * energy table. Capacity is calculated based on the max frequency of the fastest
 * cpu, so once the frequency domain of the faster cpu is regsitered, capacity
 * is recomputed.
 */
void et_init_table(struct cpufreq_policy *policy)
{
	struct energy_table *table;
	struct device *dev;
	struct cpufreq_frequency_table *cursor;
	int table_size = 0, table_index, i = 0;

	table = per_cpu_et(policy->cpu);
	if (unlikely(!table))
		return;

	dev = get_cpu_device(policy->cpu);
	if (unlikely(!dev))
		return;

	/* Count valid frequency */
	cpufreq_for_each_entry(cursor, policy->freq_table) {
		if ((cursor->frequency > policy->cpuinfo.max_freq) ||
				(cursor->frequency < policy->cpuinfo.min_freq))
			continue;

		table_size++;
	}

	/* There is no valid frequency in the table, cancels building energy table */
	if (!table_size)
		return;

	table->states = kcalloc(table->nr_table, sizeof(struct energy_state *), GFP_KERNEL);
	if (!table->states)
		return;

	table->nr_states = table_size;

	for (table_index = 0; table_index < table->nr_table; table_index++) {
		i = 0;
		table->states[table_index] = kcalloc(table_size, sizeof(struct energy_state), GFP_KERNEL);
		/* Fill the energy table with frequency, dynamic power and voltage */
		cpufreq_for_each_entry(cursor, policy->freq_table) {
			struct dev_pm_opp *opp;
			struct energy_state *state;
			unsigned long f_hz;

			if ((cursor->frequency > policy->cpuinfo.max_freq) ||
					(cursor->frequency < policy->cpuinfo.min_freq))
				continue;

			f_hz = cursor->frequency * 1000;
			opp = dev_pm_opp_find_freq_ceil(dev, &f_hz);

			state = &table->states[table_index][i++];
			state->frequency = cursor->frequency;
			state->voltage = dev_pm_opp_get_voltage(opp) / 1000;

			state->dynamic_power = get_dynamic_power(state->frequency, state->voltage,
					table->dynamic_coeff[table_index],
					table->dynamic_intercept[table_index]);
		}
	}

	table->default_state = table->states[et_default_index];

	for (table_index = 0; table_index < table->nr_table; table_index++)
		update_capacity(table_index);
}

static int parse_multi_table(struct energy_table *table,
			struct device_node *et_parent, int *nr_cluster)
{
	struct device_node *et_child;
	int nr_table = 0;

	for_each_child_of_node(et_parent, et_child) {
		int *tmp_mips;
		int *tmp_dynamic_coeff;
		int *tmp_dynamic_intercept;

		tmp_mips = kcalloc(*nr_cluster + 1, sizeof(int), GFP_KERNEL);
		if (unlikely(!tmp_mips)) {
			return -ENOMEM;
		}

		tmp_dynamic_coeff = kcalloc(*nr_cluster + 1, sizeof(int), GFP_KERNEL);
		if (unlikely(!tmp_dynamic_coeff)) {
			kfree(tmp_mips);
			return -ENOMEM;
		}

		tmp_dynamic_intercept = kcalloc(*nr_cluster + 1, sizeof(int), GFP_KERNEL);
		if (unlikely(!tmp_dynamic_intercept)) {
			kfree(tmp_mips);
			kfree(tmp_dynamic_coeff);
			return -EINVAL;
		}

		if (of_property_read_u32_array(et_child, "mips", tmp_mips, *nr_cluster + 1)) {
			kfree(tmp_mips);
			kfree(tmp_dynamic_coeff);
			kfree(tmp_dynamic_intercept);
			return -EINVAL;
		}

		if (of_property_read_u32_array(et_child, "dynamic-coeff", tmp_dynamic_coeff, *nr_cluster + 1)) {
			kfree(tmp_mips);
			kfree(tmp_dynamic_coeff);
			kfree(tmp_dynamic_intercept);
			return -EINVAL;
		}

		if (of_property_read_u32_array(et_child, "dynamic-intercept", tmp_dynamic_intercept, *nr_cluster + 1)) {
			/* optional property: set default to 0 */
			tmp_dynamic_intercept[*nr_cluster] = 0;
		}

		if (of_property_read_bool(et_child, "default_table"))
			et_default_index = nr_table;

		table->mips_mhz[nr_table] = tmp_mips[*nr_cluster];
		table->dynamic_coeff[nr_table] = tmp_dynamic_coeff[*nr_cluster];
		table->dynamic_intercept[nr_table] = tmp_dynamic_intercept[*nr_cluster];
		nr_table++;

		kfree(tmp_mips);
		kfree(tmp_dynamic_coeff);
		kfree(tmp_dynamic_intercept);
	}

	table->nr_table = nr_table;

	return 0;
}

static int parse_default_table(struct device_node *np,
			struct energy_table *table, struct ect_gen_param_table *dp_coeff)
{
	int tmp_mips, tmp_dynamic_coeff = 0, tmp_dynamic_intercept = 0, index;
	int ret = 0;

	et_default_index = 0;

	ret = of_property_read_u32(np, "capacity-dmips-mhz", &tmp_mips);

	if (dp_coeff && !of_property_read_u32(np, "ect-coeff-idx", &index))
		tmp_dynamic_coeff = dp_coeff->parameter[index];
	else
		ret = of_property_read_u32(np, "dynamic-power-coefficient", &tmp_dynamic_coeff);

	ret = of_property_read_s32(np, "dynamic-power-intercept", &tmp_dynamic_intercept);
	if (ret) {
		/* intercept is not default property */
		tmp_dynamic_intercept = 0;
		ret = 0;
	}

	table->dynamic_coeff[et_default_index] = tmp_dynamic_coeff;
	table->mips_mhz[et_default_index] = tmp_mips;
	table->dynamic_intercept[et_default_index] = tmp_dynamic_intercept;
	table->nr_table = 1;

	return ret;
}

static int init_et_data(struct device_node *np,
			struct ect_gen_param_table *dp_coeff, int *nr_cluster)
{
	struct device_node *et_parent;
	struct energy_table *table;
	const char *buf;
	int cpu, ret = 0;

	table = kzalloc(sizeof(struct energy_table), GFP_KERNEL);
	if (unlikely(!table))
		return -ENOMEM;

	ret = of_property_read_string(np, "cpus", &buf);
	if (ret)
		goto fail;
	cpulist_parse(buf, &table->cpus);

	cpumask_clear(&table->pd_cpus);
	if (!of_property_read_string(np, "pd-cpus", &buf))
		cpulist_parse(buf, &table->pd_cpus);

	et_parent = of_find_node_by_path("/power-data/energy-table");
	if (!et_parent)
		ret = parse_default_table(np, table, dp_coeff);
	else
		ret = parse_multi_table(table, et_parent, nr_cluster);

	of_node_put(et_parent);

	if (ret)
		goto fail;

	for_each_cpu(cpu, &table->cpus) {
		per_cpu_et(cpu) = table;
		ems_rq_cluster_idx(cpu_rq(cpu)) = *nr_cluster;
	}
	*nr_cluster = *nr_cluster + 1;

	return 0;

fail:
	kfree(table);
	return -EINVAL;
}

static void init_dsu_data(void)
{
	struct device_node *np;
	struct ect_gen_param_table *dp_coeff;
	const char *buf;
	int index;

	np = of_find_node_by_path("/power-data/dsu");
	if (!np)
		return;

	if (of_property_read_u32(np, "ect-coeff-idx", &index))
		return;

	dp_coeff = get_ect_gen_param_table("DTM_PWR_Coeff");
	if (!dp_coeff || !dp_coeff->parameter)
		return;

	dsu_table = kzalloc(sizeof(struct dsu_energy_table), GFP_KERNEL);
	if (unlikely(!dsu_table))
		return;

	dsu_table->dynamic_coeff = dp_coeff->parameter[index];

	cpumask_clear(&dsu_table->pd_cpus);
	if (!of_property_read_string(np, "pd-cpus", &buf))
		cpulist_parse(buf, &dsu_table->pd_cpus);
}

static int init_data(void)
{
	struct device_node *np, *child;
	struct ect_gen_param_table *dp_coeff;
	int nr_cluster = 0;

	/* DSU power data should be initialized before CPU */
	init_dsu_data();

	/* Initialize CPU power data */
	np = of_find_node_by_path("/power-data/cpu");
	if (!np)
		return -ENODATA;

	dp_coeff = get_ect_gen_param_table("DTM_PWR_Coeff");

	for_each_child_of_node(np, child)
		if (init_et_data(child, dp_coeff, &nr_cluster))
			return -EINVAL;

	of_node_put(np);

	return 0;
}

int et_init(struct kobject *ems_kobj)
{
	int ret;

	energy_table = alloc_percpu(struct energy_table *);
	if (!energy_table) {
		pr_err("Failed to allocate energy table\n");
		return -ENOMEM;
	}

	ret = init_data();
	if (ret) {
		kfree(energy_table);
		pr_err("Failed to initialize energy table\n");
		return ret;
	}

	if (sysfs_create_group(ems_kobj, &et_attr_group))
		pr_err("failed to initialize energy_table sysfs\n");

	emstune_register_notifier(&et_emstune_notifier);

	sysfs_msg = kcalloc(MSG_SIZE, sizeof(char), GFP_KERNEL);
	enable_ipc_selection = true;

	return ret;
}

