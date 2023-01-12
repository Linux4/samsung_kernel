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

#include "../sched.h"
#include "ems.h"

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

struct static_power_table {
	unsigned long *power;
	unsigned int row_size;
	unsigned int col_size;

	unsigned int *volt_field;
	unsigned int *temp_field;

	struct thermal_zone_device *tz_dev;
};

/* complex power table */
struct complex_power_table {
	struct static_power_table *sl_spt;	/* shared logic static power table */

	int cr;		/* complex ratio to total static power */
	int ccr;	/* complex core ratio to complex power */
	int csr;	/* complex shared logic ratio to complex power */
	struct cpumask cpus[VENDOR_NR_CPUS];
};

/*
 * Each cpu can have its own mips_per_mhz, coefficient and energy table.
 * Generally, cpus in the same frequency domain have the same mips_per_mhz,
 * coefficient and energy table.
 */
struct energy_table {
	struct cpumask cpus;

	unsigned int mips_mhz;
	unsigned int mips_mhz_orig;
	unsigned int dynamic_coeff;
	unsigned int dynamic_coeff_orig;
	unsigned int static_coeff;

	struct energy_state *states;
	unsigned int nr_states;

	struct static_power_table *spt;
	struct complex_power_table *cpt;

	unsigned long cur_freq;
	unsigned long cur_volt;
	struct cpumask power_sharing_cpus;	/* power sharing cpus */
	int power_sharing_dsu;			/* power sharing with DSU */
} __percpu **energy_table;

#define per_cpu_et(cpu)		(*per_cpu_ptr(energy_table, cpu))

static struct energy_table *dsu_table;

static unsigned long et_dynamic_power(struct energy_table *table,
				unsigned long freq, unsigned long volt)
{
	unsigned long p, c = table->dynamic_coeff;

	/* power = coefficent * frequency * voltage^2 */
	p = c * freq * volt * volt;

	/*
	 * f_mhz is more than 3 digits and volt is also more than 3 digits,
	 * so calculated power is more than 9 digits. For convenience of
	 * calculation, divide the value by 10^9.
	 */
	do_div(p, 1000000000);

	return p;
}

/* list must be ascending order */
static int et_find_nearest_index(int value, int *list, int length)
{
	int i;

	/*
	 * length is less than 0 or given value is smaller than first
	 * element of list, pick index 0.
	 */
	if (length <= 0 || value < list[0])
		return 0;

	for (i = 0; i < length; i++) {
		if (value == list[i])
			return i;

		if (i == length - 1)
			return i;

		if (list[i] < value && value < list[i + 1]) {
			if (value - list[i] < list[i + 1] - value)
				return i;
			else
				return i + 1;
		}
	}

	return 0;
}

static unsigned long
__et_static_power(struct static_power_table *spt,
			unsigned long volt, int temp)
{
	int row, col;

	row = et_find_nearest_index(volt, spt->volt_field, spt->row_size);
	col = et_find_nearest_index(temp, spt->temp_field, spt->col_size);

	/* get static power from pre-calculated table */
	return spt->power[row * spt->col_size + col];
}

static unsigned long et_static_power(struct energy_table *table,
					unsigned long volt, int temp)
{
	/*
	 * Pre-calculated static power table does not exist,
	 * calculate static power with coefficient.
	 */
	if (!table->spt) {
		unsigned long p;

		/* power = coefficent * voltage^2 / 10^6 */
		p = table->static_coeff * volt * volt;
		do_div(p, 1000000);
		return p;
	}

	if (!temp && likely(table->spt->tz_dev))
		temp = table->spt->tz_dev->temperature / 1000;

	return __et_static_power(table->spt, volt, temp);
}

static int et_freq_index(struct energy_table *table, unsigned long freq)
{
	int i;

	for (i = 0; i < table->nr_states; i++)
		if (table->states[i].frequency >= freq)
			break;

	if (i == table->nr_states)
		i--;

	return i;
}

static int et_cap_index(struct energy_table *table, unsigned long cap)
{
	int i;

	for (i = 0; i < table->nr_states; i++)
		if (table->states[i].capacity >= cap)
			break;

	if (i == table->nr_states)
		i--;

	return i;
}

static int et_dpower_index(struct energy_table *table, unsigned long dpower)
{
	int i;

	for (i = 0; i < table->nr_states; i++)
		if (table->states[i].dynamic_power >= dpower)
			break;

	if (i == table->nr_states)
		i--;

	return i;
}

unsigned long et_max_cap(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[table->nr_states - 1].capacity;
}

unsigned long et_max_dpower(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[table->nr_states - 1].dynamic_power;
}

unsigned long et_min_dpower(int cpu)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[0].dynamic_power;
}

unsigned long et_cap_to_freq(int cpu, unsigned long cap)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[et_cap_index(table, cap)].frequency;
}

unsigned long et_freq_to_cap(int cpu, unsigned long freq)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[et_freq_index(table, freq)].capacity;
}

unsigned long et_freq_to_dpower(int cpu, unsigned long freq)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[et_freq_index(table, freq)].dynamic_power;
}

unsigned long et_dpower_to_cap(int cpu, unsigned long dpower)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[et_dpower_index(table, dpower)].capacity;
}

unsigned int et_freq_to_volt(int cpu, unsigned int freq)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return 0;

	return table->states[et_freq_index(table, freq)].voltage;
}

static void et_fill_orig_state(int cpu, struct energy_table *table,
				int index, struct energy_state *state)
{
	state->frequency = table->states[index].frequency;
	state->capacity = table->states[index].capacity;
	state->dynamic_power = table->states[index].dynamic_power;
	state->static_power = table->states[index].static_power;
	state->voltage = table->states[index].voltage;
}

int et_get_orig_state_with_freq(int cpu, unsigned long freq, struct energy_state *state)
{
	struct energy_table *table = per_cpu_et(cpu);

	if (!table->states)
		return -ENODATA;

	et_fill_orig_state(cpu, table, et_freq_index(table, freq), state);

	return 0;
}

static unsigned long et_get_shared_volt(struct energy_table *table)
{
	unsigned long v, max_v = 0;
	int cpu;

	if (table->power_sharing_dsu) {
		if (dsu_table)
			return dsu_table->cur_volt;
		else
			return 0;
	}

	for_each_cpu(cpu, &table->power_sharing_cpus) {
		v = per_cpu_et(cpu)->cur_volt;
		if (max_v < v)
			max_v = v;
	}

	return max_v;
}

static void et_fill_state(struct energy_table *table, unsigned long next_freq,
			unsigned long next_volt, struct energy_state *state)
{
	unsigned long f_mhz, v;
	int index = et_freq_index(table, next_freq);

	f_mhz = table->states[index].frequency / 1000;
	if (next_volt)
		v = next_volt;
	else {
		/* If next_volt is not given, get current voltage */
		v = max(et_get_shared_volt(table),
			table->states[index].voltage);
	}

	state->frequency = table->states[index].frequency;
	state->capacity = table->states[index].capacity;
	state->dynamic_power = et_dynamic_power(table, f_mhz, v);
	state->static_power = et_static_power(table, v, 0);
	state->voltage = v;
}

int et_get_state_with_freq(int cpu, unsigned long freq,
			struct energy_state *state, int dis_buck_share)
{
	struct energy_table *table = per_cpu_et(cpu);
	unsigned long f_mhz, v;
	int index;

	if (!table->states)
		return -ENODATA;

	index = et_freq_index(table, freq);
	f_mhz = table->states[index].frequency / 1000;
	if (dis_buck_share)
		v = table->states[index].voltage;
	else
		v = max(et_get_shared_volt(table), table->states[index].voltage);

	state->frequency = table->states[index].frequency;
	state->capacity = table->states[index].capacity;
	state->dynamic_power = et_dynamic_power(table, f_mhz, v);
	state->static_power = et_static_power(table, v, 0);
	state->voltage = v;

	return 0;
}

static inline unsigned long et_freq_cpu_to_dsu(unsigned long cpu_freq)
{
	/* CPU-DSU freq ratio = 50% */
	unsigned long dsu_freq = cpu_freq * 50 / 100;
	int index = et_freq_index(dsu_table, dsu_freq);

	return dsu_table->states[index].frequency;
}

static void et_update_dsu_freq(void)
{
	int cpu;
	unsigned long freq, max_freq = 0, dsu_freq;

	if (!dsu_table)
		return;

	for_each_cpu(cpu, cpu_active_mask) {
		freq = per_cpu_et(cpu)->cur_freq;
		if (max_freq < freq)
			max_freq = freq;
	}

	dsu_freq = et_freq_cpu_to_dsu(max_freq);

	dsu_table->cur_freq = dsu_freq;
	dsu_table->cur_volt =
		dsu_table->states[et_freq_index(dsu_table, dsu_freq)].voltage;
}

void et_update_freq(int cpu, unsigned long freq)
{
	per_cpu_et(cpu)->cur_freq = freq;
	per_cpu_et(cpu)->cur_volt = et_freq_to_volt(cpu, freq);

	et_update_dsu_freq();
}

unsigned long et_compute_cpu_energy(int cpu, unsigned long freq,
				unsigned long util, unsigned long weight)
{
	struct cpumask mask;
	unsigned long utils[VENDOR_NR_CPUS], weights[VENDOR_NR_CPUS];

	cpumask_clear(&mask);
	cpumask_set_cpu(cpu, &mask);

	utils[cpu] = util;
	weights[cpu] = weight;

	return et_compute_energy(&mask, cpu, freq, utils, weights, 1);
}

/*
 * Returns
 *  - active or unknown idle state : -1
 *  - C1 : 0
 *  - C2 : 1
 */
static int et_get_idle_state(int cpu)
{
	struct cpuidle_state *state = idle_get_state(cpu_rq(cpu));

	if (!state)
		return -1;

	if (strcmp(state->name, "WFI"))
		return 0;
	else
		return 1;

	return -1;
}

static unsigned long et_sl_static_power(struct static_power_table *sl_spt,
			struct cpumask *complex_cpus, unsigned long volt,
			int target_cpu)
{
	int cpu;
	int sl_active = 0;

	rcu_read_lock();

	for_each_cpu(cpu, complex_cpus) {
		/*
		 * All cpus in complex muxt be in C2 state to turn off
		 * shared logic. If even one CPU is running, the shared
		 * logic becomes active state and comsumes power. Even
		 * if the CPU is in C2 state, CPU is target, CPU will
		 * become active state, so shared logic also become
		 * active state.
		 */
		if ((et_get_idle_state(cpu) != 1) || cpu == target_cpu) {
			sl_active = 1;
			break;
		}
	}

	rcu_read_unlock();

	return sl_active ? __et_static_power(sl_spt, volt, 0) : 0;
 }

static unsigned long
__et_compute_energy(const struct cpumask *cpus, struct energy_state *state,
				int target_cpu, unsigned long *utils,
				unsigned long *weights, int apply_sp)
{
	unsigned long cap, v, dp, sp, energy = 0, weight;
	int cpu;

	cap = state->capacity;
	v = state->voltage;
	dp = state->dynamic_power;
	sp = apply_sp ? state->static_power : 0;

	for_each_cpu(cpu, cpus) {
		struct energy_table *table;
		unsigned long e, sl_sp = 0;

		weight = weights ? weights[cpu] : 100;

		/*
		 * Compute energy
		 *  energy = (util / capacity) * (d.p + s.p) * weight(%)
		 */
		e = ((utils[cpu] << SCHED_CAPACITY_SHIFT) *
				(dp + sp) / cap) * 100 / weight;

		/* support complex? */
		table = per_cpu_et(cpu);
		if (table->cpt) {
			if (!apply_sp)
				goto skip;

			/* only last cpu in complex applies energy */
			if (cpu != cpumask_last(&table->cpt->cpus[cpu]))
				goto skip;

			/*
			 * MUST be modified:
			 * Track complex shared utilization and calculate energy.
			 */
			sl_sp = et_sl_static_power(table->cpt->sl_spt,
					&table->cpt->cpus[cpu], v, target_cpu);
		}

skip:
		e += (sl_sp << SCHED_CAPACITY_SHIFT);
		trace_ems_compute_energy(cpu, utils[cpu], cap, v,
				dp, sp, sl_sp, apply_sp, weight, e);

		energy += e;
	}

	return energy;
}

/* All cpus must be in same frequency domain */
unsigned long et_compute_energy(const struct cpumask *cpus, int target_cpu,
			unsigned long freq, unsigned long *utils,
			unsigned long *weights, int apply_sp)
{
	struct energy_table *table;
	struct energy_state state;

	table = per_cpu_et(cpumask_any(cpus));
	if (!table->states)
		return 0;

	et_fill_state(table, freq, 0, &state);

	return __et_compute_energy(cpus, &state, target_cpu,
				utils, weights, apply_sp);
}

static unsigned long
et_compute_dsu_energy(struct list_head *csd_head, unsigned long *utils,
					unsigned long *next_dsu_volt)
{
	unsigned long max_cpu_freq = 0;
	unsigned long dsu_freq, v, dp, sp, e;
	struct cs_domain *csd;
	struct energy_state state;

	if (!dsu_table || !dsu_table->nr_states)
		return 0;

	list_for_each_entry(csd, csd_head, list) {
		/* find max cpu freq to determine next dsu freq */
		if (max_cpu_freq < csd->next_freq)
			max_cpu_freq = csd->next_freq;
	}

	/* dsu freq is determined by the CPU that requested the highest freq */
	dsu_freq = et_freq_cpu_to_dsu(max_cpu_freq);

	et_fill_state(dsu_table, dsu_freq, 0, &state);
	v = state.voltage;
	dp = state.dynamic_power;
	sp = state.static_power;

	/*
	 * Since DSU is not an actively operating H/W, monitor unit for DSU is
	 * necessary to get DSU utilization. Since SoC is upport monitor unit
	 * currently, it assumes that DSU is running at 100%.
	 *
	 * DSU energy = (dp + sp) * 100%
	 */
	e = (dp + sp) << SCHED_CAPACITY_SHIFT;

	trace_ems_compute_dsu_energy(max_cpu_freq, dsu_freq, v, dp, sp, e);

	*next_dsu_volt = v;

	return e;
}

static void et_find_next_volt(struct list_head *csd_head,
				unsigned long next_dsu_volt)
{
	struct cs_domain *csd;
	unsigned long next_volt[VENDOR_NR_CPUS];
	int cpu;

	list_for_each_entry(csd, csd_head, list) {
		cpu = cpumask_any(&csd->cpus);
		next_volt[cpu] = et_freq_to_volt(cpu, csd->next_freq);
	}

	list_for_each_entry(csd, csd_head, list) {
		struct energy_table *table;

		cpu = cpumask_any(&csd->cpus);
		table = per_cpu_et(cpu);

#if 0
		if (table->power_sharing_dsu) {
			csd->next_volt = max(next_volt[cpu], next_dsu_volt);
			continue;
		}
#endif

		if (!cpumask_empty(&table->power_sharing_cpus)) {
			int scpu = cpumask_any(&table->power_sharing_cpus);

			csd->next_volt = max(next_volt[cpu], next_volt[scpu]);
			continue;
		}

		csd->next_volt = next_volt[cpu];
	}
}

unsigned long et_compute_system_energy(struct list_head *csd_head,
				unsigned long *utils, unsigned long *weights,
				int target_cpu)
{
	struct cs_domain *csd;
	unsigned long energy, next_dsu_volt = 0;

	energy = et_compute_dsu_energy(csd_head, utils, &next_dsu_volt);

	et_find_next_volt(csd_head, next_dsu_volt);

	list_for_each_entry(csd, csd_head, list) {
		struct energy_table *table;
		struct energy_state state;

		table = per_cpu_et(cpumask_any(&csd->cpus));
		if (!table->states)
			continue;

		et_fill_state(table, csd->next_freq, csd->next_volt, &state);

		energy += __et_compute_energy(&csd->cpus, &state, target_cpu,
					utils, weights, csd->apply_sp);
	}

	return energy;
}

/****************************************************************
 *		   emstune mode update notifier			*
 ****************************************************************/
static void et_update_dynamic_power(void)
{
	struct energy_table *table;
	int cpu, i;

	/* Fill energy table with dynamic power */
	for_each_possible_cpu(cpu) {
		table = per_cpu_et(cpu);
		if (!table->nr_states)
			continue;

		for (i = 0; i < table->nr_states; i++) {
			unsigned long f_mhz = table->states[i].frequency / 1000;
			unsigned long v = table->states[i].voltage;

			table->states[i].dynamic_power =
						et_dynamic_power(table, f_mhz, v);
		}
	}
}

static void et_set_cpu_scale(unsigned int cpu, unsigned long capacity)
{
	per_cpu(cpu_scale, cpu) = capacity;
}

static void et_update_capacity(void)
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
		if (!table->nr_states)
			continue;

		/* max mips = max_f * mips/mhz */
		max_f = table->states[table->nr_states - 1].frequency;
		mips = max_f * table->mips_mhz;
		if (mips > max_mips)
			max_mips = mips;
	}

	if (!max_mips)
		return;

	/* Fill energy table with capacity */
	for_each_possible_cpu(cpu) {
		table = per_cpu_et(cpu);
		if (!table->nr_states)
			continue;

		for (i = 0; i < table->nr_states; i++) {
			unsigned long f = table->states[i].frequency;

			/*
			 *     mips(f) = f * mips/mhz
			 * capacity(f) = mips(f) / max_mips * 1024
			 */
			table->states[i].capacity =
				f * table->mips_mhz * 1024 / max_mips;
		}

		/* Set CPU scale with max capacity of the CPU */
		et_set_cpu_scale(cpu, table->states[table->nr_states - 1].capacity);
	}
}

static int et_emstune_notifier_call(struct notifier_block *nb,
				unsigned long val, void *v)
{
	struct emstune_set *cur_set = (struct emstune_set *)v;
	struct emstune_specific_energy_table *emstune_specific_et;
	int cpu;
	bool need_to_update_dynamic_power = false, need_to_update_capacity = false;

	emstune_specific_et = &cur_set->specific_energy_table;

	for_each_possible_cpu(cpu) {
		struct energy_table *table;
		unsigned int next_mips_mhz;
		unsigned int next_dynamic_coeff;

		table = per_cpu_et(cpu);

		/* Check whether dynamic power coefficient is changed */
		next_dynamic_coeff = emstune_specific_et->dynamic_coeff[cpu] ?
				     emstune_specific_et->dynamic_coeff[cpu] :
				     table->dynamic_coeff_orig;
		if (table->dynamic_coeff != next_dynamic_coeff) {
			table->dynamic_coeff = next_dynamic_coeff;
			need_to_update_dynamic_power = true;
		}

		/* Check whether mips per mhz is changed */
		next_mips_mhz = emstune_specific_et->mips_mhz[cpu] ?
				emstune_specific_et->mips_mhz[cpu] :
				table->mips_mhz_orig;
		if (table->mips_mhz != next_mips_mhz) {
			table->mips_mhz = next_mips_mhz;
			need_to_update_capacity = true;
		}
	}

	if (need_to_update_dynamic_power)
		et_update_dynamic_power();

	if (need_to_update_capacity)
		et_update_capacity();

	return NOTIFY_OK;
}

static struct notifier_block et_emstune_notifier = {
	.notifier_call = et_emstune_notifier_call,
};

static ssize_t et_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct energy_table *table = NULL;
	struct static_power_table *spt;
	int cpu, i, ret = 0;

	for_each_possible_cpu(cpu) {
		if (table && cpumask_test_cpu(cpu, &table->cpus))
			continue;

		table = per_cpu_et(cpu);
		if (unlikely(!table))
			continue;

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"[Energy Table: cpu%d]\n", cpu);

		spt = table->spt;
		if (!spt) {
			for (i = 0; i < table->nr_states; i++) {
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"cap=%4lu dyn-power=%4lu sta-power=%lu\n",
					table->states[i].capacity,
					table->states[i].dynamic_power,
					table->states[i].static_power);
			}
		} else {

			for (i = 0; i < table->nr_states; i++) {
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"cap=%4lu dyn-power=%lu\n",
					table->states[i].capacity,
					table->states[i].dynamic_power);
			}
		}

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	return ret;
}

static DEVICE_ATTR(energy_table, S_IRUGO, et_show, NULL);

static ssize_t et_sp_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct energy_table *table = NULL;
	struct static_power_table *spt;
	int cpu, i, row, ret = 0;

	for_each_possible_cpu(cpu) {
		if (table && cpumask_test_cpu(cpu, &table->cpus))
			continue;

		table = per_cpu_et(cpu);
		if (unlikely(!table))
			continue;

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"[cpu%d]\n", cpu);

		spt = table->spt;
		if (!spt) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"Does not support precise static power table\n\n");
			continue;
		}

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "       ");
		for (i = 0; i < spt->col_size; i++)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" %2lu℃", spt->temp_field[i]);
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		for (row = 0; row < spt->row_size; row++) {
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"%4lumV ", spt->volt_field[row]);
			for (i = row * spt->col_size; i < (row + 1) * spt->col_size; i++)
				ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"%4lu ", spt->power[i]);
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
		}

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	return ret;
}

static DEVICE_ATTR(static_power_table, S_IRUGO, et_sp_show, NULL);

static ssize_t et_dsu_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct energy_table *table;
	struct static_power_table *spt;
	int i, row, ret = 0;

	if (!dsu_table)
		return 0;

	table = dsu_table;
	spt = dsu_table->spt;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "[Energy Table: DSU]\n");

	for (i = 0; i < table->nr_states; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"freq=%4lu dyn-power=%lu\n",
				table->states[i].frequency,
				table->states[i].dynamic_power);
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\nsta-power table:\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "       ");
	for (i = 0; i < spt->col_size; i++)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				" %2lu℃", spt->temp_field[i]);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	for (row = 0; row < spt->row_size; row++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"%4lumV ", spt->volt_field[row]);
		for (i = row * spt->col_size; i < (row + 1) * spt->col_size; i++)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
					"%4lu ", spt->power[i]);
		ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	return ret;
}

static DEVICE_ATTR(dsu_energy_table, S_IRUGO, et_dsu_show, NULL);

static unsigned long
et_calulate_static_power(int ag, int ts, int mV, int vt_c,
				int ac_a, int ac_b, int ac_c)
{
	unsigned long long sp;
	long long ratio;

	ratio = (vt_c * ((ac_a * ag * ag) +
			 (ac_b * ag) + ac_c));

	sp = div64_u64(mV * ts * ratio, 10000000000);

	return (unsigned long)sp;
}

static struct ect_gen_param_table *et_ect_gen_param_table(char *name)
{
	void *gen_block = ect_get_block("GEN");

	if (!gen_block) {
		pr_err("Failed to get general parameter block from ECT\n");
		return NULL;
	}

	return ect_gen_param_get_table(gen_block, name);
}

static void et_init_complex_power_table(struct device_node *np,
					struct energy_table *table)
{
	struct device_node *complex_np;
	struct complex_power_table *cpt;
	const char *buf[VENDOR_NR_CPUS];
	struct ect_gen_param_table *ect_cr, *ect_ccr, *ect_csr;
	int i, count;

	complex_np = of_find_node_by_name(np, "complex");
	if (!complex_np)
		return;

	cpt = kzalloc(sizeof(struct complex_power_table), GFP_KERNEL);
	if (unlikely(!cpt))
		return;

	/* initialize complex cpu configuration */
	count = of_property_count_strings(complex_np, "cpus");
	if (!count) {
		kfree(cpt);
		return;
	}

	/* Read complex ratio information  */
	ect_cr = et_ect_gen_param_table("COMPLEX_RATIO");
	ect_ccr = et_ect_gen_param_table("COMPLEX_CORE_RATIO");
	ect_csr = et_ect_gen_param_table("COMPLEX_SHARED_RATIO");
	if (!ect_cr || !ect_ccr || !ect_csr) {
		kfree(cpt);
		return;
	}

	of_property_read_string_array(complex_np, "cpus", buf, count);
	for (i = 0; i < count; i++) {
		struct cpumask mask;
		int cpu;

		cpulist_parse(buf[i], &mask);
		for_each_cpu(cpu, &table->cpus)
			if (cpumask_test_cpu(cpu, &mask))
				cpumask_copy(&cpt->cpus[cpu], &mask);
	}

	cpt->cr = ect_cr->parameter[0];
	cpt->ccr = ect_ccr->parameter[0];
	cpt->csr = ect_csr->parameter[0];

	table->cpt = cpt;
}

static struct static_power_table *
et_alloc_static_power_table(int row_size, int col_size)
{
	struct static_power_table *spt;

	spt = kzalloc(sizeof(struct static_power_table), GFP_KERNEL);
	if (unlikely(!spt))
		return NULL;

	spt->row_size = row_size;
	spt->col_size = col_size;

	spt->power = kcalloc(spt->row_size * spt->col_size,
			sizeof(unsigned long), GFP_KERNEL);
	spt->volt_field = kcalloc(spt->row_size, sizeof(int), GFP_KERNEL);
	spt->temp_field = kcalloc(spt->col_size, sizeof(int), GFP_KERNEL);
	if (!spt->power || !spt->volt_field || !spt->temp_field) {
		kfree(spt->power);
		kfree(spt->volt_field);
		kfree(spt->temp_field);
		kfree(spt);
		return NULL;
	}

	return spt;
}

static void et_copy_static_power_table(struct static_power_table *dst,
					struct static_power_table *src)
{
	memcpy(dst->volt_field, src->volt_field,
				sizeof(unsigned int) * src->row_size);
	memcpy(dst->temp_field, src->temp_field,
				sizeof(unsigned int) * src->col_size);
}

static void et_init_dsu_static_power_table(struct static_power_table *spt)
{
	struct ect_gen_param_table *ect_dsr;
	struct static_power_table *dsu_spt;
	int row, col, dsr;

	if (!dsu_table)
		return;

	ect_dsr = et_ect_gen_param_table("DSU_STATIC_RATIO");
	if (!ect_dsr)
		return;

	dsr = ect_dsr->parameter[0];

	/* size of dsu static power table is same as this domain */
	dsu_spt = et_alloc_static_power_table(spt->row_size, spt->col_size);
	if (!dsu_spt)
		return;

	et_copy_static_power_table(dsu_spt, spt);

	for (row = 0; row < spt->row_size; row++) {
		for (col = 0; col < spt->col_size; col++) {
			int i = row * spt->col_size + col;

			dsu_spt->power[i] = spt->power[i] * dsr / 100;
			spt->power[i] = spt->power[i] * (100 - dsr) / 100;
		}
	}

	dsu_table->spt = dsu_spt;
}

static struct static_power_table *
__et_init_static_power_table(struct device_node *np)
{
	struct static_power_table *spt;
	const char *ect_vt_name, *ect_a_name;
	struct ect_gen_param_table *vt, *a;
	int cal_id, ts, ag;
	int row, i, cnt = 0;

	if (of_property_read_u32(np, "cal-id", &cal_id) ||
	    of_property_read_string(np, "ect-param-vt", &ect_vt_name) ||
	    of_property_read_string(np, "ect-param-a", &ect_a_name))
		return NULL;

	vt = et_ect_gen_param_table((char *)ect_vt_name);
	a = et_ect_gen_param_table((char *)ect_a_name);
	if (!vt || !a) {
		pr_err("Failed to get coefficient table from ECT\n");
		return NULL;
	}

	spt = et_alloc_static_power_table(vt->num_of_row - 1,
					  vt->num_of_col - 1);
	if (!spt)
		return NULL;

	ag = max(cal_asv_get_grp(cal_id), 0);
	if (!ag)
		ag = 8;		/* 8 = last group */

	ts = cal_asv_get_ids_info(cal_id);
	if (!ts)
		ts = 73;	/* 73 = temporarily value */

	/* fill temperature field (row) */
	for (row = 1; row < vt->num_of_row; row++)
		spt->volt_field[row - 1] = vt->parameter[row * vt->num_of_col];

	/* fill temperature field (column) */
	for (i = 1; i < vt->num_of_col; i++)
		spt->temp_field[i - 1] = vt->parameter[i];

	for (row = 1; row < vt->num_of_row; row++) {
		for (i = row * vt->num_of_col + 1; i < (row + 1) * vt->num_of_col; i++) {
			unsigned int volt = vt->parameter[row * vt->num_of_col];

			spt->power[cnt++] = et_calulate_static_power(ag,
					ts, volt, vt->parameter[i],
					a->parameter[row * a->num_of_col + 0],
					a->parameter[row * a->num_of_col + 1],
					a->parameter[row * a->num_of_col + 2]);
		}
	}

	return spt;
}

static void et_init_static_power_table(struct device_node *np,
					struct energy_table *table)
{
	struct static_power_table *spt;
	const char *tz_name;
	int  row, col, i;

	/*
	 * __et_init_static_power_table() fills static power with whole static
	 * power for cpu domain. It must be divided by the value corresponding
	 * to each CPU.
	 */
	spt = __et_init_static_power_table(np);
	if (!spt)
		return;

	table->spt = spt;

	/* get thermal zone dev to get temperature */
	if (!of_property_read_string(np, "tz-name", &tz_name))
		spt->tz_dev = thermal_zone_get_zone_by_name(tz_name);

	/* If this domain contains dsu, separate dsu from entire power */
	if (table->power_sharing_dsu)
		et_init_dsu_static_power_table(spt);

	/*
	 * If complex is supported, allocate shared logic static power table and
	 * divide static power with complex ratio.
	 */
	if (table->cpt) {
		struct static_power_table *sl_spt = NULL;

		sl_spt = et_alloc_static_power_table(spt->row_size, spt->col_size);
		if (!sl_spt)
			goto skip;

		et_copy_static_power_table(sl_spt, spt);

		for (row = 0; row < spt->row_size; row++) {
			for (col = 0; col < spt->col_size; col++) {
				unsigned long power;
				i = row * spt->col_size + col;

				power = spt->power[i];
				/*
				 * P(complex) = P(total) * complex_ratio / 100
				 * P(complex-core) = P(complex) * complex_core_ratio / 100
				 * P(complex-shared) = P(complex) * complex_shared_ratio / 100
				 */
				spt->power[i] = power * table->cpt->cr *
						table->cpt->ccr / 10000;
				sl_spt->power[i] = power * table->cpt->cr *
						table->cpt->csr / 10000;
			}
		}

		table->cpt->sl_spt = sl_spt;
		return;
	}

skip:
	/*
	 * et_init_static_power() fills static power with whole static
	 * power for cpu domain. It divided by num of cpus.
	 */
	for (row = 0; row < spt->row_size; row++) {
		for (col = 0; col < spt->col_size; col++) {
			spt->power[row * spt->col_size + col]
					/= cpumask_weight(&table->cpus);
		}
	}
}

static void et_init_dsu_table(struct energy_table *table, struct device *dev)
{
	int i;

	if (!dsu_table)
		return;

	dsu_table->states = kcalloc(table->nr_states,
				sizeof(struct energy_state), GFP_KERNEL);
	if (!dsu_table->states)
		return;

	dsu_table->nr_states = table->nr_states;
	for (i = 0; i < dsu_table->nr_states; i++) {
		unsigned long f_mhz, v;

		f_mhz = table->states[i].frequency / 1000;		/* KHz -> MHz */
		v = table->states[i].voltage;

		/*
		 * To get DSU frequency, it needs to register DSU freq table to
		 * opp. Build DSU table with CPU table temporarily.
		 */
		dsu_table->states[i].frequency = table->states[i].frequency;
		dsu_table->states[i].dynamic_power = et_dynamic_power(dsu_table, f_mhz, v);
		dsu_table->states[i].voltage = v;
	}
}

#define DEFAULT_TEMP	85

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
	int table_size = 0, i, cpu = policy->cpu;

	table = per_cpu_et(cpu);
	if (unlikely(!table))
		return;

	dev = get_cpu_device(cpu);
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

	table->nr_states = table_size;
	table->states = kcalloc(table_size,
			sizeof(struct energy_state), GFP_KERNEL);
	if (!table->states)
		return;

	/* Fill the energy table with frequency, dynamic/static power and voltage */
	i = 0;
	cpufreq_for_each_entry(cursor, policy->freq_table) {
		struct dev_pm_opp *opp;
		unsigned long f_hz, f_mhz, v;

		if ((cursor->frequency > policy->cpuinfo.max_freq) ||
		    (cursor->frequency < policy->cpuinfo.min_freq))
			continue;

		f_mhz = cursor->frequency / 1000;		/* KHz -> MHz */
		f_hz = cursor->frequency * 1000;		/* KHz -> Hz */

		/* Get voltage from opp */
		opp = dev_pm_opp_find_freq_ceil(dev, &f_hz);
		v = dev_pm_opp_get_voltage(opp) / 1000;		/* uV -> mV */

		table->states[i].frequency = cursor->frequency;
		table->states[i].dynamic_power = et_dynamic_power(table, f_mhz, v);
		table->states[i].static_power = et_static_power(table, v, DEFAULT_TEMP);
		table->states[i].voltage = v;
		i++;
	}

	if (table->power_sharing_dsu)
		et_init_dsu_table(table, dev);

	et_update_capacity();
}

static void et_init_dsu_data(void)
{
	struct device_node *np;
	struct ect_gen_param_table *dp_coeff;
	int index;
	const char *buf;

	np = of_find_node_by_path("/power-data/dsu");
	if (!np)
		return;

	dp_coeff = et_ect_gen_param_table("DTM_PWR_Coeff");
	if (!dp_coeff)
		return;

	if (of_property_read_u32(np, "ect-coeff-idx", &index) ||
	    of_property_read_string(np, "power-sharing-cpus", &buf))
		return;

	dsu_table = kzalloc(sizeof(struct energy_table), GFP_KERNEL);
	if (!dsu_table)
		return;

	dsu_table->dynamic_coeff = dp_coeff->parameter[index];
	cpulist_parse(buf, &dsu_table->power_sharing_cpus);
}

static int et_init_data(void)
{
	struct device_node *np, *child;
	struct ect_gen_param_table *dp_coeff;

	/* Initialize DSU power data, it is optional */
	et_init_dsu_data();

	/* Initialize CPU power data */
	np = of_find_node_by_path("/power-data/cpu");
	if (!np)
		return -ENODATA;

	dp_coeff = et_ect_gen_param_table("DTM_PWR_Coeff");

	for_each_child_of_node(np, child) {
		struct energy_table *table;
		const char *buf;
		int cpu, index;

		if (of_property_read_string(child, "cpus", &buf))
			continue;

		table = kzalloc(sizeof(struct energy_table), GFP_KERNEL);
		if (unlikely(!table))
			continue;

		cpulist_parse(buf, &table->cpus);


		/* Read dmps/mhz and power coefficient from device tree */
		of_property_read_u32(child, "capacity-dmips-mhz", &table->mips_mhz_orig);
		table->mips_mhz = table->mips_mhz_orig;

		of_property_read_u32(child, "static-power-coefficient", &table->static_coeff);

		/*
		 * Read dynamic power coefficient from ECT first, otherwise read
		 * it from device tree.
		 */
		if (dp_coeff && !of_property_read_u32(child, "ect-coeff-idx", &index))
			table->dynamic_coeff_orig = dp_coeff->parameter[index];
		else
			of_property_read_u32(child, "dynamic-power-coefficient",
							&table->dynamic_coeff_orig);
		table->dynamic_coeff = table->dynamic_coeff_orig;

		/*
		 * Read power sharing cpus(for single buck).
		 * If cpu does not share power domain with other cpu,
		 * table->power_sharing_cpus is empty.
		 */
		cpumask_clear(&table->power_sharing_cpus);
		if (!of_property_read_string(child, "power-sharing-cpus", &buf))
			cpulist_parse(buf, &table->power_sharing_cpus);
		if (of_property_read_bool(child, "power-sharing-dsu"))
			table->power_sharing_dsu = 1;

		for_each_cpu(cpu, &table->cpus)
			per_cpu_et(cpu) = table;

		et_init_complex_power_table(child, table);
		et_init_static_power_table(child, table);
	}

	of_node_put(np);

	return 0;
}

int et_init(struct kobject *ems_kobj)
{
	int ret;

	energy_table = alloc_percpu(struct energy_table *);
	if (!energy_table) {
		pr_err("failed to allocate energy table\n");
		return -ENOMEM;
	}

	ret = et_init_data();
	if (ret) {
		pr_err("failed to initialize energy table\n");
		return ret;
	}

	if (sysfs_create_file(ems_kobj, &dev_attr_energy_table.attr))
		pr_warn("%s: failed to create sysfs\n", __func__);
	if (sysfs_create_file(ems_kobj, &dev_attr_dsu_energy_table.attr))
		pr_warn("%s: failed to create sysfs\n", __func__);
	if (sysfs_create_file(ems_kobj, &dev_attr_static_power_table.attr))
		pr_warn("%s: failed to create sysfs\n", __func__);

	emstune_register_notifier(&et_emstune_notifier);

	return ret;
}
