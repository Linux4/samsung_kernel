#include <v2/exynos-cpu-profiler.h>

/************************************************************************
 *				HELPER					*
 ************************************************************************/
static struct domain_profiler *get_dom_by_profiler_id(int id)
{
	struct domain_profiler *dompro;

	list_for_each_entry(dompro, &profiler.list, list)
		if (dompro->profiler_id == id)
			return dompro;
	return NULL;
}

static struct domain_profiler *get_dom_by_cpu(int cpu)
{
	struct domain_profiler *dompro;

	list_for_each_entry(dompro, &profiler.list, list)
		if (cpumask_test_cpu(cpu, &dompro->cpus))
			return dompro;
	return NULL;
}

/************************************************************************
 *				SUPPORT-PROFILER				*
 ************************************************************************/
u32 cpupro_get_table_cnt(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);

	if (!dompro)
		return 0;

	return dompro->table_cnt;
}

u32 cpupro_get_freq_table(s32 id, u32 *table)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);
	int idx;

	if (!dompro)
		return 0;

	for (idx = 0; idx < dompro->table_cnt; idx++)
		table[idx] = dompro->table[idx].freq;

	return idx;
}

u32 cpupro_get_max_freq(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);

	if (!dompro || !dompro->enabled)
		return 0;

	return dompro->table[dompro->max_freq_idx].freq;
}

u32 cpupro_get_min_freq(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);

	if (!dompro || !dompro->enabled)
		return 0;

	return dompro->table[dompro->min_freq_idx].freq;
}

u32 cpupro_get_freq(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);

	if (!dompro || !dompro->enabled)
		return 0;

	return dompro->result[PROFILER].fc_result.freq[CS_ACTIVE];
}

void cpupro_get_power(s32 id, u64 *dyn_power, u64 *st_power)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);

	if (!dompro || !dompro->enabled) {
		*dyn_power = 0;
		*st_power = 0;
		return;
	}

	*dyn_power = dompro->result[PROFILER].fc_result.dyn_power;
	*st_power = dompro->result[PROFILER].fc_result.st_power;
}
void cpupro_get_power_change(s32 id, s32 freq_delta_ratio,
		u32 *freq, u64 *dyn_power, u64 *st_power)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);
	struct cpu_profile_result *result = &dompro->result[PROFILER];
	struct freq_cstate_result *fc_result = &result->fc_result;
	int cpus_cnt = cpumask_weight(&dompro->online_cpus);
	int flag = (STATE_SCALE_WO_SPARE | STATE_SCALE_TIME);

	/* this domain is disabled */
	if (unlikely(!cpus_cnt))
		return;

	get_power_change(dompro->table, dompro->table_cnt,
			dompro->cur_freq_idx, dompro->min_freq_idx, dompro->max_freq_idx,
			fc_result->time[CS_ACTIVE], fc_result->time[CLK_OFF], freq_delta_ratio,
			fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);

	*dyn_power = (*dyn_power) * cpus_cnt;
	*st_power = (*st_power) * cpus_cnt;
}

u32 cpupro_get_active_pct(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);

	if (!dompro || !dompro->enabled)
		return 0;

	return dompro->result[PROFILER].fc_result.ratio[CS_ACTIVE];
}
s32 cpupro_get_temp(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);

	if (!dompro || !dompro->enabled)
		return 0;

	return dompro->result[PROFILER].avg_temp;
}

void cpupro_set_margin(s32 id, s32 margin)
{
	return;
}

void cpupro_set_dom_profile(struct domain_profiler *dompro, int enabled);
void cpupro_update_profile(struct domain_profiler *dompro, int user);
u32 cpupro_update_mode(s32 id, int mode)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);

	if (!dompro)
		return 0;

	if (dompro->enabled != mode) {
		cpupro_set_dom_profile(dompro, mode);
		return 0;
	}

	cpupro_update_profile(dompro, PROFILER);

	return 0;
}

s32 cpupro_cpu_active_pct(s32 id, s32 *cpu_active_pct)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);
	int cpu, cnt = 0;

	if (!dompro || !dompro->enabled)
		return 0;

	for_each_cpu(cpu, &dompro->online_cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		struct freq_cstate_result *fc_result = &cpupro->result[PROFILER].fc_result;
		cpu_active_pct[cnt++] = fc_result->ratio[CS_ACTIVE];
	}

	return 0;
};

s32 cpupro_cpu_asv_ids(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_profiler_id(id);

	if (!dompro || !dompro->enabled)
		return 0;

	return cal_asv_get_ids_info(dompro->cal_id);
}

struct private_fn_cpu cpu_pd_fn = {
	.cpu_active_pct		= &cpupro_cpu_active_pct,
	.cpu_asv_ids		= &cpupro_cpu_asv_ids,
};

struct domain_fn cpu_fn = {
	.get_table_cnt		= &cpupro_get_table_cnt,
	.get_freq_table		= &cpupro_get_freq_table,
	.get_max_freq		= &cpupro_get_max_freq,
	.get_min_freq		= &cpupro_get_min_freq,
	.get_freq		= &cpupro_get_freq,
	.get_power		= &cpupro_get_power,
	.get_power_change	= &cpupro_get_power_change,
	.get_active_pct		= &cpupro_get_active_pct,
	.get_temp		= &cpupro_get_temp,
	.set_margin		= &cpupro_set_margin,
	.update_mode		= &cpupro_update_mode,
};

/************************************************************************
 *			Gathering CPUFreq Information			*
 ************************************************************************/
static void cpupro_update_time_in_freq(int cpu, int new_cstate, int new_freq_idx)
{
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);
	ktime_t cur_time, diff;
	unsigned long flag;

	if (!dompro || !dompro->enabled)
		return;

	raw_spin_lock_irqsave(&cpupro->lock, flag);
	if (unlikely(!cpupro->enabled)) {
		raw_spin_unlock_irqrestore(&cpupro->lock, flag);
		return;
	}

	cur_time = ktime_get();

	diff = ktime_sub(cur_time, cpupro->last_update_time);

	if (cpupro->fc.time[cpupro->cur_cstate] && dompro->cur_freq_idx < dompro->table_cnt)
		cpupro->fc.time[cpupro->cur_cstate][dompro->cur_freq_idx] += diff;

	if (new_cstate > -1)
		cpupro->cur_cstate = new_cstate;

	if (new_freq_idx > -1)
		dompro->cur_freq_idx = new_freq_idx;

	cpupro->last_update_time = cur_time;

	raw_spin_unlock_irqrestore(&cpupro->lock, flag);
}

void cpupro_make_domain_freq_cstate_result(struct domain_profiler *dompro, int user)
{
	struct cpu_profile_result *result = &dompro->result[user];
	struct freq_cstate_result *fc_result = &dompro->result[user].fc_result;
	int cpu, state, cpus_cnt = cpumask_weight(&dompro->online_cpus);

	/* this domain is disabled */
	if (unlikely(!cpus_cnt))
		return;

	/* clear previous result */
	fc_result->profile_time = 0;
	for_each_cstate(state)
		if (fc_result->time[state])
			memset(fc_result->time[state], 0, sizeof(ktime_t) * dompro->table_cnt);

	/* gathering cpus profile */
	for_each_cpu(cpu, &dompro->online_cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		struct freq_cstate_result *cpu_fc_result = &cpupro->result[user].fc_result;
		int idx;

		for_each_cstate(state) {
			if (!fc_result->time[state] || !cpu_fc_result->time[state])
				continue;

			for (idx = 0; idx < dompro->table_cnt; idx++)
				fc_result->time[state][idx] +=
					(cpu_fc_result->time[state][idx] / cpus_cnt);
		}
		fc_result->profile_time += (cpu_fc_result->profile_time / cpus_cnt);
	}

	/* compute power/freq/active_ratio from time_in_freq */
	compute_freq_cstate_result(dompro->table, fc_result,
			dompro->table_cnt, dompro->cur_freq_idx, result->avg_temp);

	/* should multiply cpu_cnt to power when computing domain power */
	fc_result->dyn_power = fc_result->dyn_power * cpus_cnt;
	fc_result->st_power = fc_result->st_power * cpus_cnt;
}

static void reset_cpu_data(struct domain_profiler *dompro, struct cpu_profiler *cpupro, int user)
{
	struct freq_cstate *fc = &cpupro->fc;
	struct freq_cstate_snapshot *fc_snap = &cpupro->fc_snap[user];
	int table_cnt = dompro->table_cnt;

	cpupro->cur_cstate = CS_ACTIVE;
	cpupro->last_update_time = ktime_get();

	sync_fcsnap_with_cur(fc, fc_snap, table_cnt);
}

static void cpupro_init(int cpu)
{
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);
	unsigned long flag;

	/* Enable profiler */
	if (!dompro->enabled) {
		/* update cstate for running cpus */
		raw_spin_lock_irqsave(&cpupro->lock, flag);
		reset_cpu_data(dompro, cpupro, PROFILER);
		cpupro->enabled = true;
		raw_spin_unlock_irqrestore(&cpupro->lock, flag);

		cpupro_update_time_in_freq(cpu, -1, -1);
	} else {
		/* Disable profiler */
		raw_spin_lock_irqsave(&cpupro->lock, flag);
		cpupro->enabled = false;
		cpupro->cur_cstate = PWR_OFF;
		raw_spin_unlock_irqrestore(&cpupro->lock, flag);
	}
}

void cpupro_set_dom_profile(struct domain_profiler *dompro, int enabled)
{
	struct cpufreq_policy *policy;
	int cpu;

	mutex_lock(&profiler.mode_change_lock);
	/* this domain is disabled */
	if (!cpumask_weight(&dompro->online_cpus))
		goto unlock;

	cpu = cpumask_first(&dompro->online_cpus);
	if (cpu >= nr_cpu_ids)
		goto unlock;

	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		goto unlock;

	/* update current freq idx */
	dompro->cur_freq_idx = get_idx_from_freq(dompro->table, dompro->table_cnt,
			policy->cur, RELATION_LOW);
	cpufreq_cpu_put(policy);

	/* reset cpus of this domain */
	for_each_cpu(cpu, &dompro->online_cpus) {
		cpupro_init(cpu);
	}
	dompro->enabled = enabled;
unlock:
	mutex_unlock(&profiler.mode_change_lock);
}

void cpupro_update_profile(struct domain_profiler *dompro, int user)
{
	struct cpu_profile_result *result = &dompro->result[user];
	struct cpufreq_policy *policy;
	int cpu;

	/* this domain profile is disabled */
	if (unlikely(!dompro) || unlikely(!dompro->enabled))
		return;

	/* there is no online cores in this domain  */
	if (!cpumask_weight(&dompro->online_cpus))
		return;

	/* update this domain temperature */
	if (dompro->tz) {
		int temp = exynos_profiler_get_temp(dompro->tz);
		result->avg_temp = (temp + result->cur_temp) >> 1;
		result->cur_temp = temp;
	}

	/* update cpus profile */
	for_each_cpu(cpu, &dompro->online_cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		struct freq_cstate *fc = &cpupro->fc;
		struct freq_cstate_snapshot *fc_snap = &cpupro->fc_snap[user];
		struct freq_cstate_result *fc_result = &cpupro->result[user].fc_result;

		cpupro_update_time_in_freq(cpu, -1, -1);

		make_snapshot_and_time_delta(fc, fc_snap, fc_result, dompro->table_cnt);

		if (!fc_result->profile_time)
			continue;

		compute_freq_cstate_result(dompro->table, fc_result,
				dompro->table_cnt, dompro->cur_freq_idx, result->avg_temp);
	}

	/* update domain */
	cpupro_make_domain_freq_cstate_result(dompro, user);

	/* check cpu offline */
	cpu = cpumask_first(&dompro->online_cpus);
	if (cpu >= nr_cpu_ids)
		return;

	/* update max freq */
	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return;

	dompro->max_freq_idx = get_idx_from_freq(dompro->table, dompro->table_cnt,
			policy->max, RELATION_LOW);
	dompro->min_freq_idx = get_idx_from_freq(dompro->table, dompro->table_cnt,
			policy->min, RELATION_HIGH);
	cpufreq_cpu_put(policy);
}

void exynos_profiler_pb_set_cur_freqlv(int id, int level);
static void cpupro_hook_freq_change(void *data, struct cpufreq_policy *policy)
{
	struct domain_profiler *dompro = get_dom_by_cpu(policy->cpu);
	int cpu, new_freq_idx;

	if (!dompro || !dompro->enabled)
		return;

	if (policy->cpu != cpumask_first(&dompro->online_cpus))
		return;

	/* update current freq */
	new_freq_idx = get_idx_from_freq(dompro->table,
			dompro->table_cnt, policy->cur, RELATION_HIGH);

	if (PROFILER_CL0 < dompro->profiler_id && dompro->profiler_id < NUM_OF_CPU_DOMAIN)
		exynos_profiler_pb_set_cur_freqlv(dompro->profiler_id, new_freq_idx);

	for_each_cpu(cpu, &dompro->online_cpus)
		cpupro_update_time_in_freq(cpu, -1, new_freq_idx);
}

static int cpupro_pm_update(int flag)
{
	int cpu = raw_smp_processor_id();
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);

	if (!cpupro || !cpupro->enabled)
		return NOTIFY_OK;

	cpupro_update_time_in_freq(cpu, flag, -1);

	return NOTIFY_OK;
}

static void android_vh_cpupro_idle_enter(void *data, int *state,
		struct cpuidle_device *dev)
{
	int flag = (*state) ? PWR_OFF : CLK_OFF;
	cpupro_pm_update(flag);
}
static void android_vh_cpupro_idle_exit(void *data, int state,
		struct cpuidle_device *dev)
{
	cpupro_pm_update(CS_ACTIVE);
}

static int cpupro_cpupm_online(unsigned int cpu)
{
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);

	if (dompro)
		cpumask_set_cpu(cpu, &dompro->online_cpus);

	return 0;
}

static int cpupro_cpupm_offline(unsigned int cpu)
{
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	unsigned long flag;

	if (!dompro)
		return 0;

	raw_spin_lock_irqsave(&cpupro->lock, flag);

	cpumask_clear_cpu(cpu, &dompro->online_cpus);
	cpupro->enabled = false;

	raw_spin_unlock_irqrestore(&cpupro->lock, flag);

	if (!cpumask_weight(&dompro->online_cpus))
		dompro->enabled = false;

	return 0;
}
/************************************************************************
 *				INITIALIZATON				*
 ************************************************************************/
static int parse_domain_dt(struct domain_profiler *dompro, struct device_node *dn)
{
	const char *buf;
	int ret;

	/* necessary data */
	ret = of_property_read_string(dn, "sibling-cpus", &buf);
	if (ret)
		return -1;

	cpulist_parse(buf, &dompro->cpus);
	cpumask_copy(&dompro->online_cpus, &dompro->cpus);

	ret = of_property_read_u32(dn, "cal-id", &dompro->cal_id);
	if (ret)
		return -2;

	/* un-necessary data */
	ret = of_property_read_s32(dn, "profiler-id", &dompro->profiler_id);
	if (ret)
		dompro->profiler_id = -1;	/* Don't support profiler */

	of_property_read_u32(dn, "power-coefficient", &dompro->dyn_pwr_coeff);
	of_property_read_u32(dn, "static-power-coefficient", &dompro->st_pwr_coeff);
	of_property_read_string(dn, "tz-name", &dompro->tz_name);

	return 0;
}

static int init_profile_result(struct domain_profiler *dompro,
		struct cpu_profile_result *result, int dom_init)
{
	int size = dompro->table_cnt;

	if (init_freq_cstate_result(&result->fc_result, size))
		return -ENOMEM;

	return 0;
}

static struct freq_table *cpupro_init_freq_table(struct domain_profiler *dompro,
		struct cpufreq_policy *policy)
{
	struct cpufreq_frequency_table *pos;
	struct freq_table *table;
	struct device *dev;
	int idx;

	dev = get_cpu_device(policy->cpu);
	if (unlikely(!dev)) {
		pr_err("cpupro: failed to get cpu device\n");
		return NULL;
	}

	dompro->table_cnt = cpufreq_table_count_valid_entries(policy);

	table = kcalloc(dompro->table_cnt, sizeof(struct freq_table), GFP_KERNEL);
	if (!table)
		return NULL;

	idx = dompro->table_cnt - 1;
	cpufreq_for_each_valid_entry(pos, policy->freq_table) {
		struct dev_pm_opp *opp;
		unsigned long f_hz, f_mhz, v;

		if ((pos->frequency > policy->cpuinfo.max_freq) ||
		    (pos->frequency < policy->cpuinfo.min_freq))
			continue;

		f_mhz = pos->frequency / 1000;		/* KHz -> MHz */
		f_hz = pos->frequency * 1000;		/* KHz -> Hz */

		/* Get voltage from opp */
		opp = dev_pm_opp_find_freq_ceil(dev, &f_hz);
		v = dev_pm_opp_get_voltage(opp);

		table[idx].freq = pos->frequency;
		table[idx].volt = v;
		table[idx].dyn_cost = pwr_cost(pos->frequency, v,
				dompro->dyn_pwr_coeff, PWR_COST_CFVV);
		table[idx].st_cost = pwr_cost(pos->frequency, v,
				dompro->st_pwr_coeff, PWR_COST_CFVV);
		idx--;
	}

	dompro->max_freq_idx = 0;
	dompro->min_freq_idx = dompro->table_cnt - 1;
	dompro->cur_freq_idx = get_idx_from_freq(table,
			dompro->table_cnt, policy->cur, RELATION_HIGH);

	return table;
}

static int init_domain(struct domain_profiler *dompro, struct device_node *dn)
{
	struct cpufreq_policy *policy;
	int ret, cpu, num_of_cpus;

	/* Parse data from Device Tree to init domain */
	ret = parse_domain_dt(dompro, dn);
	if (ret) {
		pr_err("cpupro: failed to parse dt(ret: %d)\n", ret);
		return -EINVAL;
	}

	cpu = cpumask_first(&dompro->online_cpus);
	if (cpu >= nr_cpu_ids) {
		pr_err("cpupro: not a online cpu (cpu%d)\n", cpu);
		return -EINVAL;
	}

	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		pr_err("cpupro: failed to get cpufreq_policy(cpu%d)\n", cpu);
		return -EINVAL;
	}

	/* init freq table */
	dompro->table = cpupro_init_freq_table(dompro, policy);
	cpufreq_cpu_put(policy);
	if (!dompro->table) {
		pr_err("cpupro: failed to init freq_table (cpu%d)\n", cpu);
		return -ENOMEM;
	}

	if (dompro->profiler_id < NUM_OF_CPU_DOMAIN) {
		ret = exynos_profiler_pb_set_powertable(dompro->profiler_id, dompro->table_cnt, dompro->table);
		if (ret) {
			pr_err("cpupro: failed to set power table(ret: %d)\n", ret);
			return -ENOMEM;
		}
	}

	for (int idx = 0; idx < NUM_OF_USER; idx++)
		if (init_profile_result(dompro, &dompro->result[idx], 1)) {
			kfree(dompro->table);
			return -EINVAL;
		}

	/* get thermal-zone to get temperature */
	if (dompro->tz_name)
		dompro->tz = exynos_profiler_init_temp(dompro->tz_name);

	/* when DTM support this IP and support static cost table, user DTM static table */
	num_of_cpus = cpumask_weight(&dompro->cpus);
	if (dompro->tz)
		init_static_cost(dompro->table, dompro->table_cnt, num_of_cpus, dn, dompro->tz);

	return 0;
}

static int init_cpus_of_domain(struct domain_profiler *dompro)
{
	int cpu, idx;

	for_each_cpu(cpu, &dompro->cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		if (!cpupro) {
			pr_err("cpupro: failed to alloc cpu_profiler(%d)\n", cpu);
			return -ENOMEM;
		}

		if (init_freq_cstate(&cpupro->fc, dompro->table_cnt))
			return -ENOMEM;

		for (idx = 0; idx < NUM_OF_USER; idx++) {
			if (init_freq_cstate_snapshot(&cpupro->fc_snap[idx], dompro->table_cnt))
				return -ENOMEM;

			if (init_profile_result(dompro, &cpupro->result[idx], 1))
				return -ENOMEM;
		}

		/* init spin lock */
		raw_spin_lock_init(&cpupro->lock);
	}

	return 0;
}

static void show_domain_info(struct domain_profiler *dompro)
{
	int idx;
	char buf[10];

	scnprintf(buf, sizeof(buf), "%*pbl", cpumask_pr_args(&dompro->cpus));
	pr_info("================ domain(cpus : %s) ================\n", buf);
	pr_info("min= %dKHz, max= %dKHz\n",
			dompro->table[dompro->table_cnt - 1].freq, dompro->table[0].freq);
	for (idx = 0; idx < dompro->table_cnt; idx++)
		pr_info("lv=%3d freq=%8d volt=%8d dyn_cost=%lld st_cost=%lld\n",
				idx, dompro->table[idx].freq, dompro->table[idx].volt,
				dompro->table[idx].dyn_cost,
				dompro->table[idx].st_cost);
	if (dompro->tz_name)
		pr_info("support temperature (tz_name=%s)\n", dompro->tz_name);
	if (dompro->profiler_id != -1)
		pr_info("support profiler domain(id=%d)\n", dompro->profiler_id);
}

static int exynos_cpu_profiler_probe(struct platform_device *pdev)
{
	struct domain_profiler *dompro;
	struct device_node *dn;

	/* get node of device tree */
	if (!pdev->dev.of_node) {
		pr_err("cpupro: failed to get device treee\n");
		return -EINVAL;
	}
	profiler.root = pdev->dev.of_node;
	profiler.kobj = &pdev->dev.kobj;

	/* init per_cpu variable */
	profiler.cpus = alloc_percpu(struct cpu_profiler);

	/* init list for domain */
	INIT_LIST_HEAD(&profiler.list);

	/* init domains */
	for_each_child_of_node(profiler.root, dn) {
		dompro = kzalloc(sizeof(struct domain_profiler), GFP_KERNEL);
		if (!dompro) {
			pr_err("cpupro: failed to alloc domain\n");
			return -ENOMEM;
		}

		if (init_domain(dompro, dn)) {
			pr_err("cpupro: failed to alloc domain\n");
			return -ENOMEM;
		}

		init_cpus_of_domain(dompro);

		list_add_tail(&dompro->list, &profiler.list);

		/* register profiler to profiler */
		if (dompro->profiler_id != -1)
			exynos_profiler_register_domain(dompro->profiler_id, &cpu_fn, &cpu_pd_fn);
	}

	/* initialize work and completion for cpu initial setting */
	mutex_init(&profiler.mode_change_lock);

	/* register cpufreq vendor hook */
	register_trace_android_rvh_cpufreq_transition(cpupro_hook_freq_change, NULL);

	/* register cpu pm notifier */
	register_trace_android_vh_cpu_idle_enter(android_vh_cpupro_idle_enter, NULL);
	register_trace_android_vh_cpu_idle_exit(android_vh_cpupro_idle_exit, NULL);

	/* register cpu hotplug notifier */
	cpuhp_setup_state(CPUHP_BP_PREPARE_DYN,
			"EXYNOS_PROFILER_CPU_POWER_UP_CONTROL",
			cpupro_cpupm_online, NULL);

	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"EXYNOS_PROFILER_CPU_POWER_DOWN_CONTROL",
			NULL, cpupro_cpupm_offline);

	/* show domain information */
	list_for_each_entry(dompro, &profiler.list, list)
		show_domain_info(dompro);

	return 0;
}

static const struct of_device_id exynos_cpu_profiler_match[] = {
	{
		.compatible	= "samsung,exynos-cpu-profiler",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_cpu_profiler_match);

static struct platform_driver exynos_cpu_profiler_driver = {
	.probe		= exynos_cpu_profiler_probe,
	.driver	= {
		.name	= "exynos-cpu-profiler",
		.owner	= THIS_MODULE,
		.of_match_table = exynos_cpu_profiler_match,
	},
};

static int exynos_cpu_profiler_init(void)
{
	return platform_driver_register(&exynos_cpu_profiler_driver);
}
late_initcall(exynos_cpu_profiler_init);

MODULE_DESCRIPTION("Exynos CPU Profiler v2");
MODULE_SOFTDEP("pre: exynos_thermal_v2 exynos-cpufreq sgpu exynos_devfreq");
MODULE_LICENSE("GPL");
