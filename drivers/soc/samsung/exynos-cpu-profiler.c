#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/percpu.h>
#include <linux/cpufreq.h>
#include <linux/cpu_pm.h>
#include <linux/io.h>
#include <linux/cpuhotplug.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/ems.h>

#include <asm/perf_event.h>

#include <soc/samsung/exynos-profiler.h>
#include <soc/samsung/exynos-migov.h>
#include <soc/samsung/exynos-dm.h>

enum pmu_event_enum {
	PMU_CCNTR,
	NUM_OF_PMUEVENT,
};

struct pmu_event_info {
	u32	enable;
	u32	id;
	u32	pa;
	void __iomem *va;
	u32	uss;
};

struct pmu_event {
	struct pmu_event_info *info;	/* shared with cpus */

	u64	last_cnt;
	u64	*cnt;			/* accomulated cnt matched with freq */
};

struct pmu_event_result {
	u64	*cnt_snap;	/* previous cont */
	u64	*cnt_delta;	/* delta between evtcnt and evtcnt_snap */
};

struct pmu_profile {
	struct pmu_event	*event[NUM_OF_PMUEVENT];	/* shared with users */
	struct pmu_event_result	 *result[NUM_OF_PMUEVENT][NUM_OF_USER];
};

/* Result during profile time */
struct profile_result {
	struct freq_cstate_result	fc_result;

	s32				cur_temp;
	s32				avg_temp;
};

struct cpu_profiler {
	raw_spinlock_t		lock;
	struct domain_profiler	*dom;

	int			enabled;

	struct task_struct	*init_task;	/* init task */
	struct completion	init_done;	/* completion flag */

	s32				cur_cstate;		/* this cpu cstate */
	ktime_t				last_update_time;

	struct freq_cstate		fc;
	struct freq_cstate_snapshot	fc_snap[NUM_OF_USER];
	struct profile_result		result[NUM_OF_USER];

	struct pmu_profile		pmu;
};

struct domain_profiler {
	struct list_head	list;
	struct cpumask		cpus;		/* cpus in this domain */
	struct cpumask		online_cpus;	/* online cpus in this domain */

	int			enabled;

	s32			migov_id;
	u32			cal_id;

	/* DVFS Data */
	u32			table_cnt;
	struct freq_table	*table;

	u32			cur_freq_idx;	/* current freq_idx */
	u32			max_freq_idx;	/* current max_freq_idx */
	u32			min_freq_idx;	/* current min_freq_idx */

	/* Power Data */
	u32			dyn_pwr_coeff;
	u32			st_pwr_coeff;

	/* Thermal Data */
	const char			*tz_name;
	struct thermal_zone_device	*tz;

	/* PMU Data */
	struct pmu_profile	pmu;

	/* Profile Result */
	struct profile_result	result[NUM_OF_USER];
};

static struct profiler {
	struct device_node	*root;
	int			initialized;

	struct list_head		list;	/* list for domain */
	struct cpu_profiler __percpu	*cpus;	/* cpu data for all cpus */

	struct kobject		*kobj;
} profiler;

/************************************************************************
 *				HELPER					*
 ************************************************************************/
static struct domain_profiler *get_dom_by_migov_id(int id)
{
	struct domain_profiler *dompro;

	list_for_each_entry(dompro, &profiler.list, list)
		if (dompro->migov_id == id)
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

static inline bool enabled_pmu_event(struct pmu_event *event)
{
	return !!(event);
}
/************************************************************************
 *				SUPPORT-MIGOV				*
 ************************************************************************/
u32 cpupro_get_table_cnt(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->table_cnt;
}

u32 cpupro_get_freq_table(s32 id, u32 *table)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	int idx;
	for (idx = 0; idx < dompro->table_cnt; idx++)
		table[idx] = dompro->table[idx].freq;
	return idx;
}

u32 cpupro_get_max_freq(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->table[dompro->max_freq_idx].freq;
}

u32 cpupro_get_min_freq(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->table[dompro->min_freq_idx].freq;
}

u32 cpupro_get_freq(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->result[MIGOV].fc_result.freq[ACTIVE];
}

void cpupro_get_power(s32 id, u64 *dyn_power, u64 *st_power)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	*dyn_power = dompro->result[MIGOV].fc_result.dyn_power;
	*st_power = dompro->result[MIGOV].fc_result.st_power;
}
void cpupro_get_power_change(s32 id, s32 freq_delta_ratio,
			u32 *freq, u64 *dyn_power, u64 *st_power)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	struct profile_result *result = &dompro->result[MIGOV];
	struct freq_cstate_result *fc_result = &result->fc_result;
	int cpus_cnt = cpumask_weight(&dompro->online_cpus);
	int flag = (STATE_SCALE_WITH_SPARE | STATE_SCALE_TIME);

	/* this domain is disabled */
	if (unlikely(!cpus_cnt))
		return;

	get_power_change(dompro->table, dompro->table_cnt,
		dompro->cur_freq_idx, dompro->min_freq_idx, dompro->max_freq_idx,
		fc_result->time[ACTIVE], fc_result->time[CLK_OFF], freq_delta_ratio,
		fc_result->profile_time, result->avg_temp, flag, dyn_power, st_power, freq);

	*dyn_power = (*dyn_power) * cpus_cnt;
	*st_power = (*st_power) * cpus_cnt;
}

u32 cpupro_get_active_pct(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	return dompro->result[MIGOV].fc_result.ratio[ACTIVE];
}
s32 cpupro_get_temp(s32 id)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);
	if (unlikely(!dompro))
		return 0;
	return dompro->result[MIGOV].avg_temp;
}

void cpupro_set_margin(s32 id, s32 margin)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);

	esg_set_migov_boost(cpumask_first(&dompro->cpus), margin / 10);
}

void cpupro_set_dom_profile(struct domain_profiler *dompro, int enabled);
void cpupro_update_profile(struct domain_profiler *dompro, int user);
u32 cpupro_update_mode(s32 id, int mode)
{
	struct domain_profiler *dompro = get_dom_by_migov_id(id);

	if (dompro->enabled != mode) {
		/* sync up pmu cnt and kernei time with current state */
		cpupro_set_dom_profile(dompro, mode);

		return 0;
	}

	cpupro_update_profile(dompro, MIGOV);
	return 0;
}

u64 cpupro_get_stall_pct(s32 id) { return 0; };

struct private_fn_cpu cpu_pd_fn = {
	.get_stall_pct		= &cpupro_get_stall_pct,
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
#define ARMV8_PMU_CCNT_IDX 31
static void cpupro_update_pmuevent(int cpu,
	struct cpu_profiler *cpupro, int new_cstate, int freq_idx)
{
	struct pmu_profile *pmu = &cpupro->pmu;
	int idx;

	for (idx = 0; idx < NUM_OF_PMUEVENT; idx++) {
		struct pmu_event *event = pmu->event[idx];
		u64 delta = 0, cur_cnt0, cur_cnt1, cur_cnt;

		if (!enabled_pmu_event(event))
			continue;

		if (cpupro->cur_cstate == PWR_OFF && new_cstate == -1)
			continue;

		if (new_cstate == ACTIVE)
			event->last_cnt = 0;

		cur_cnt0 = __raw_readl(event->info->va + event->info->id);
		cur_cnt1 = __raw_readl((event->info->va + event->info->id + 0x8));
		cur_cnt = cur_cnt0 + (cur_cnt1 << 32);
		if (!event->last_cnt) {
			event->last_cnt = cur_cnt;
			continue;
		}

		if (cur_cnt < event->last_cnt) {
			pr_info("cpu%d cur_cnt=%llu last_cnt=%llu cur_cstate=%d new_cstate=%d\n",
				cpu, cur_cnt, event->last_cnt, cpupro->cur_cstate, new_cstate);
		}

		delta = cur_cnt - event->last_cnt;
		event->last_cnt = cur_cnt;
		event->cnt[freq_idx] += delta;
	}
}

static void cpupro_update_time_in_freq(int cpu, int new_cstate, int new_freq_idx)
{
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	struct freq_cstate *fc = &cpupro->fc;
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);
	ktime_t cur_time, diff;

	raw_spin_lock(&cpupro->lock);
	if (unlikely(!cpupro->enabled)) {
		raw_spin_unlock(&cpupro->lock);
		return;
	}

	cur_time = ktime_get();

	diff = ktime_sub(cur_time, cpupro->last_update_time);
	fc->time[cpupro->cur_cstate][dompro->cur_freq_idx] += diff;

	cpupro_update_pmuevent(cpu, cpupro, new_cstate, dompro->cur_freq_idx);

	if (new_cstate > -1)
		cpupro->cur_cstate = new_cstate;

	if (new_freq_idx > -1)
		dompro->cur_freq_idx = new_freq_idx;

	cpupro->last_update_time = cur_time;

	raw_spin_unlock(&cpupro->lock);
}

void cpupro_make_domain_freq_cstate_result(struct domain_profiler *dompro, int user)
{
	struct profile_result *result = &dompro->result[user];
	struct freq_cstate_result *fc_result = &dompro->result[user].fc_result;
	int cpu, state, cpus_cnt = cpumask_weight(&dompro->online_cpus);

	/* this domain is disabled */
	if (unlikely(!cpus_cnt))
		return;

	/* clear previous result */
	fc_result->profile_time = 0;
	for (state = 0; state < NUM_OF_CSTATE; state++)
		memset(fc_result->time[state], 0, sizeof(ktime_t) * dompro->table_cnt);

	/* gathering cpus profile */
	for_each_cpu(cpu, &dompro->online_cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		struct freq_cstate_result *cpu_fc_result = &cpupro->result[user].fc_result;
		int idx;

		for (state = 0; state < NUM_OF_CSTATE; state++) {
			if (!cpu_fc_result->time[state])
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

void cpupro_make_clkoff_active_table(struct freq_table *table, int size,
					struct freq_cstate_result *fc_result,
					struct pmu_event_result *pmu_result)
{
	int idx;
#ifdef CONFIG_SOC_EXYNOS9830
	/*
	 * 9830 doesn't need to call this function,
	 * because it can profile c0/c1/c2 from idle driver
	 */
	return;
#endif

	for (idx = 0; idx < size; idx++) {
		ktime_t active_time, clkoff_time;
		/* usec = cycle * 1000000 / freq (KHz) */
		active_time = pmu_result->cnt_delta[idx] * 1000000 / table[idx].freq;
		clkoff_time = fc_result->time[ACTIVE][idx] - active_time;
		fc_result->time[CLK_OFF][idx] = max(clkoff_time, (ktime_t) 0);
		fc_result->time[ACTIVE][idx] = (ktime_t) active_time;
	}
}

void cpupro_set_cpu_profile(int cpu)
{
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);

	/* Enable profiler */
	if (!dompro->enabled) {
		/* update cstate for running cpus */
		raw_spin_lock(&cpupro->lock);
		cpupro->enabled = true;
		cpupro->cur_cstate = ACTIVE;
		cpupro->last_update_time = ktime_get();
		raw_spin_unlock(&cpupro->lock);
		cpupro_update_time_in_freq(cpu, -1, -1);
	} else {
	/* Disable profiler */
		raw_spin_lock(&cpupro->lock);
		cpupro->enabled = false;
		cpupro->cur_cstate = PWR_OFF;
		raw_spin_unlock(&cpupro->lock);
	}
}

void cpupro_set_dom_profile(struct domain_profiler *dompro, int enabled)
{
	struct cpufreq_policy *policy;
	int cpu;

	/* this domain is disabled */
	if (!cpumask_weight(&dompro->cpus))
		return;

	policy = cpufreq_cpu_get(cpumask_first(&dompro->online_cpus));
	if (policy) {
		/* update current freq idx */
		dompro->cur_freq_idx = get_idx_from_freq(dompro->table, dompro->table_cnt,
				policy->cur, RELATION_LOW);
		cpufreq_cpu_put(policy);
	}

	/* reset cpus of this domain */
	for_each_cpu(cpu, &dompro->cpus)
		cpupro_set_cpu_profile(cpu);

	dompro->enabled = enabled;
}

void cpupro_update_profile(struct domain_profiler *dompro, int user)
{
	struct profile_result *result = &dompro->result[user];
	struct cpufreq_policy *policy;
	int cpu, idx;

	/* this domain profile is disabled */
	if (unlikely(!dompro->enabled))
		return;

	/* there is no online cores in this domain  */
	if (!cpumask_weight(&dompro->online_cpus))
		return;

	/* update this domain temperature */
	if (dompro->tz) {
		int temp = get_temp(dompro->tz);
		result->avg_temp = (temp + result->cur_temp) >> 1;
		result->cur_temp = temp;
	}

	/* update cpus profile */
	for_each_cpu(cpu, &dompro->online_cpus) {
		struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
		struct freq_cstate *fc = &cpupro->fc;
		struct freq_cstate_snapshot *fc_snap = &cpupro->fc_snap[user];
		struct freq_cstate_result *fc_result = &cpupro->result[user].fc_result;
		struct pmu_profile *pmu = &cpupro->pmu;

		cpupro_update_time_in_freq(cpu, -1, -1);

		make_snapshot_and_time_delta(fc, fc_snap, fc_result, dompro->table_cnt);

		for (idx = 0; idx < NUM_OF_PMUEVENT; idx++) {
			struct pmu_event *event = pmu->event[idx];
			struct pmu_event_result *pmu_result = pmu->result[idx][user];

			if (!event)
				continue;

			make_snap_and_delta(event->cnt, pmu_result->cnt_snap,
					pmu_result->cnt_delta, dompro->table_cnt);

			if (idx == PMU_CCNTR)
				cpupro_make_clkoff_active_table(dompro->table,
					dompro->table_cnt, fc_result, pmu_result);
		}

		if (!fc_result->profile_time)
			continue;

		compute_freq_cstate_result(dompro->table, fc_result,
				dompro->table_cnt, dompro->cur_freq_idx, result->avg_temp);
	}

	/* update domain */
	cpupro_make_domain_freq_cstate_result(dompro, user);

	/* update max freq */
	policy = cpufreq_cpu_get(cpumask_first(&dompro->online_cpus));
	if (!policy)
		return;

	dompro->max_freq_idx = get_idx_from_freq(dompro->table, dompro->table_cnt,
					policy->max, RELATION_LOW);
	dompro->min_freq_idx = get_idx_from_freq(dompro->table, dompro->table_cnt,
					policy->min, RELATION_HIGH);
	cpufreq_cpu_put(policy);
}

static int cpupro_cpufreq_callback(struct notifier_block *nb,
					unsigned long flag, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct domain_profiler *dompro;
	int cpu, new_freq_idx;

#ifdef CONFIG_SOC_EXYNOS9830
	dompro = get_dom_by_cpu(freq->cpu);
#else
	dompro = get_dom_by_cpu(freq->policy->cpu);
#endif

	if (!dompro->enabled)
		return NOTIFY_OK;

	if (flag != CPUFREQ_POSTCHANGE)
		return NOTIFY_OK;

#ifdef CONFIG_SOC_EXYNOS9830
	if (freq->cpu != cpumask_first(&dompro->online_cpus))
		return NOTIFY_OK;
#else
	if (freq->policy->cpu != cpumask_first(&dompro->online_cpus))
		return NOTIFY_OK;
#endif

	/* update current freq */
	new_freq_idx = get_idx_from_freq(dompro->table,
			dompro->table_cnt, freq->new, RELATION_HIGH);

	for_each_cpu(cpu, &dompro->online_cpus)
		cpupro_update_time_in_freq(cpu, -1, new_freq_idx);

	return NOTIFY_OK;
}

static struct notifier_block cpupro_cpufreq_notifier = {
	.notifier_call  = cpupro_cpufreq_callback,
};

#ifdef CONFIG_SOC_EXYNOS9830
void cpupro_pm_update(int cpu, int idx, int entry)
{
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	int new_cstate;

	if (unlikely(!profiler.initialized))
		return;

	if (!cpupro->enabled)
		return;

	if (entry)      /* enter power mode */
		new_cstate = idx ? PWR_OFF : CLK_OFF;
	else            /* wake-up */
		new_cstate = ACTIVE;

	cpupro_update_time_in_freq(cpu, new_cstate, -1);
} EXPORT_SYMBOL(cpupro_pm_update);
#else
static void set_ccntr(void)
{
	u32 val;

	/* Enable All Counters */
	val = read_sysreg(pmcr_el0) | ARMV8_PMU_PMCR_E;
	write_sysreg(val, pmcr_el0);

	val = read_sysreg(pmcntenset_el0) | BIT(ARMV8_PMU_CCNT_IDX);
	write_sysreg(val, pmcntenset_el0);
}

static void set_pmu_event(void)
{
	set_ccntr();
}
static int cpupro_pm_notifier(struct notifier_block *self,
						unsigned long f, void *v)
{
	int cpu = raw_smp_processor_id();
	struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
	int new_cstate;

	if (f != CPU_PM_EXIT && f != CPU_PM_ENTER)
		return NOTIFY_OK;

	if (!cpupro->enabled)
		return NOTIFY_OK;

	new_cstate = (f == CPU_PM_EXIT) ? ACTIVE : PWR_OFF;

	/* when wake-up from C2, should set pmu */
	if (f == CPU_PM_EXIT)
		set_pmu_event();

	cpupro_update_time_in_freq(cpu, new_cstate, -1);

	return NOTIFY_OK;
}

static struct notifier_block cpupro_pm_nb = {
	.notifier_call = cpupro_pm_notifier,
};
#endif

static int cpupro_cpupm_online(unsigned int cpu)
{
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);
	cpumask_set_cpu(cpu, &dompro->online_cpus);
	return 0;
}

static int cpupro_cpupm_offline(unsigned int cpu)
{
	struct domain_profiler *dompro = get_dom_by_cpu(cpu);
	cpumask_clear_cpu(cpu, &dompro->online_cpus);
	return 0;
}
/************************************************************************
 *				INITIALIZATON				*
 ************************************************************************/
static int parse_pmuevent_dt(struct domain_profiler *dompro, struct device_node *dn)
{
	struct pmu_profile *pmu = &dompro->pmu;
	int idx, cpu;

	for (idx = 0; idx < NUM_OF_PMUEVENT; idx++) {
		char name[10];
		u32 buf[4];

		snprintf(name, sizeof(name), "event%d", idx);
		if (of_property_read_u32_array(dn, (const char *)name, buf, 4))
			continue;

		pmu->event[idx] = kzalloc(sizeof(struct pmu_event), GFP_KERNEL);
		if (!pmu->event[idx])
			return -ENOMEM;

		pmu->event[idx]->info = kzalloc(sizeof(struct pmu_event_info), GFP_KERNEL);
		if (!pmu->event[idx]->info)
			return -ENOMEM;

		pmu->event[idx]->info->enable = buf[0];

		/* alloc VA memory for cpu */
		for_each_cpu(cpu, &dompro->cpus) {
			struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
			struct pmu_profile *cpu_pmu = &cpupro->pmu;
			struct pmu_event_info *cpu_info;

			cpu_pmu->event[idx] = kzalloc(sizeof(struct pmu_event), GFP_KERNEL);
			if (!cpu_pmu->event[idx])
				return -ENOMEM;

			cpu_info = kzalloc(sizeof(struct pmu_event_info), GFP_KERNEL);
			if (!cpu_info)
				return -ENOMEM;

			cpu_info->enable = buf[0];
			cpu_info->id = buf[1];
			cpu_info->pa = buf[2] + buf[3] * cpu;
			cpu_info->va = ioremap(cpu_info->pa, SZ_4K);
			cpu_pmu->event[idx]->info = cpu_info;
		}
	}

	return 0;
}

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
	ret = of_property_read_s32(dn, "migov-id", &dompro->migov_id);
	if (ret)
		dompro->migov_id = -1;	/* Don't support migov */

	of_property_read_u32(dn, "power-coefficient", &dompro->dyn_pwr_coeff);
	of_property_read_u32(dn, "static-power-coefficient", &dompro->st_pwr_coeff);
	of_property_read_string(dn, "tz-name", &dompro->tz_name);

	/* PMU event */
	if (parse_pmuevent_dt(dompro, dn))
		return -3;

	return 0;
}

static int init_profile_result(struct domain_profiler *dompro,
			struct profile_result *result, int dom_init)
{
	int size = dompro->table_cnt;

	if (init_freq_cstate_result(&result->fc_result, NUM_OF_CSTATE, size))
		return -ENOMEM;
	return 0;
}

static int init_pmu_profile(struct domain_profiler *dompro, struct pmu_profile *pmu, int cpu_init)
{
	int idx, user;
	u64 *cnt_in_freq;

	/* init pmu_event */
	for (idx = 0; idx < NUM_OF_PMUEVENT; idx++) {
		struct pmu_event *event = pmu->event[idx];

		if (!enabled_pmu_event(event))
			continue;

		cnt_in_freq = alloc_state_in_freq(dompro->table_cnt);
		if (!cnt_in_freq)
			return -ENOMEM;
		event->cnt = cnt_in_freq;

		/* init pmu_event_result */
		for (user = 0; user < NUM_OF_USER; user++) {
			struct pmu_event_result *result;
			result = kzalloc(sizeof(struct pmu_event_result), GFP_KERNEL);
			if (!result)
				return -ENOMEM;

			/* cpu profile needs snapshot to make result */
			if (cpu_init) {
				cnt_in_freq = alloc_state_in_freq(dompro->table_cnt);
				if (!cnt_in_freq)
					return -ENOMEM;
				result->cnt_snap = cnt_in_freq;
			}

			cnt_in_freq = alloc_state_in_freq(dompro->table_cnt);
			if (!cnt_in_freq)
				return -ENOMEM;
			result->cnt_delta = cnt_in_freq;
			pmu->result[idx][user] = result;
		}
	}
	return 0;
}

static int init_domain(struct domain_profiler *dompro, struct device_node *dn)
{
	struct cpufreq_policy *policy;
	unsigned int org_max_freq, org_min_freq, cur_freq;
	int ret, idx, num_of_cpus;

	/* Parse data from Device Tree to init domain */
	ret = parse_domain_dt(dompro, dn);
	if (ret) {
		pr_err("cpupro: failed to parse dt(ret: %d)\n", ret);
		return -EINVAL;
	}

	/* get valid min/max freq and number of freq level from cpufreq */
	policy = cpufreq_cpu_get(cpumask_first(&dompro->cpus));
	if (!policy) {
		pr_err("cpupro: failed to get cpufreq_policy(cpu%d)\n",
					cpumask_first(&dompro->cpus));
		return -EINVAL;
	}
	org_max_freq = policy->cpuinfo.max_freq;
	org_min_freq = policy->cpuinfo.min_freq;
	cur_freq = policy->cur;
	dompro->table_cnt = cpufreq_table_count_valid_entries(policy);
	cpufreq_cpu_put(policy);

	/* init freq table */
	dompro->table = init_freq_table(NULL, dompro->table_cnt,
			dompro->cal_id, org_max_freq, org_min_freq,
			dompro->dyn_pwr_coeff, dompro->st_pwr_coeff,
			PWR_COST_CFVV, PWR_COST_CFVV);
	if (!dompro->table) {
		pr_err("cpupro: failed to init freq_table (cpu%d)\n",
					cpumask_first(&dompro->cpus));
		return -EINVAL;
	}
	dompro->max_freq_idx = 0;
	dompro->min_freq_idx = dompro->table_cnt - 1;
	dompro->cur_freq_idx = get_idx_from_freq(dompro->table,
				dompro->table_cnt, cur_freq, RELATION_HIGH);

	for (idx = 0; idx < NUM_OF_USER; idx++)
		if (init_profile_result(dompro, &dompro->result[idx], 1))
			return -EINVAL;

	if (init_pmu_profile(dompro, &dompro->pmu, 0))
		return -EINVAL;

	/* get thermal-zone to get temperature */
	if (dompro->tz_name)
		dompro->tz = init_temp(dompro->tz_name);

	/* when DTM support this IP and support static cost table, user DTM static table */
	num_of_cpus = cpumask_weight(&dompro->cpus);
	if (dompro->tz)
		init_static_cost(dompro->table, dompro->table_cnt,
					num_of_cpus, dn, dompro->tz);

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

		if (init_freq_cstate(&cpupro->fc, NUM_OF_CSTATE, dompro->table_cnt))
			return -ENOMEM;

		for (idx = 0; idx < NUM_OF_USER; idx++) {
			if (init_freq_cstate_snapshot(&cpupro->fc_snap[idx],
						NUM_OF_CSTATE, dompro->table_cnt))
				return -ENOMEM;

			if (init_profile_result(dompro, &cpupro->result[idx], 1))
				return -ENOMEM;
		}

		init_pmu_profile(dompro, &cpupro->pmu, 1);

		/* init spin lock */
		raw_spin_lock_init(&cpupro->lock);

		/* init completion */
		init_completion(&cpupro->init_done);
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
		pr_info("lv=%3d freq=%8d volt=%8d dyn_cost=%5d st_cost=%5d\n",
			idx, dompro->table[idx].freq, dompro->table[idx].volt,
			dompro->table[idx].dyn_cost,
			dompro->table[idx].st_cost);
	if (dompro->tz_name)
		pr_info("support temperature (tz_name=%s)\n", dompro->tz_name);
	if (dompro->migov_id != -1)
		pr_info("support migov domain(id=%d)\n", dompro->migov_id);

	for (idx = 0; idx < NUM_OF_PMUEVENT; idx++) {
		struct pmu_event *event = dompro->pmu.event[idx];
		int cpu;
		if (!enabled_pmu_event(event))
			continue;

		for_each_cpu(cpu, &dompro->cpus) {
			struct cpu_profiler *cpupro = per_cpu_ptr(profiler.cpus, cpu);
			struct pmu_event_info *cpu_info = cpupro->pmu.event[idx]->info;

			pr_info("cpu%d: event%d: enable= %d va= %xllu\n",
					cpu, idx, cpu_info->enable, cpu_info->va);
		}
	}
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

		/* register profiler to migov */
		if (dompro->migov_id != -1)
			exynos_migov_register_domain(dompro->migov_id,
						&cpu_fn, &cpu_pd_fn);
	}

	/* register cpufreq notifier */
	cpufreq_register_notifier(&cpupro_cpufreq_notifier,
				CPUFREQ_TRANSITION_NOTIFIER);
#ifndef CONFIG_SOC_EXYNOS9830
	/* register cpu pm notifier */
	cpu_pm_register_notifier(&cpupro_pm_nb);
#endif

	/* register cpu hotplug notifier */
	cpuhp_setup_state(CPUHP_BP_PREPARE_DYN,
			"AP_EXYNOS_CPU_POWER_UP_CONTROL",
			cpupro_cpupm_online, NULL);

	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"AP_EXYNOS_CPU_POWER_DOWN_CONTROL",
			NULL, cpupro_cpupm_offline);

	/* show domain information */
	list_for_each_entry(dompro, &profiler.list, list)
		show_domain_info(dompro);

	profiler.initialized = true;

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

MODULE_SOFTDEP("pre: exynos-migov");
MODULE_DESCRIPTION("Exynos CPU Profiler");
MODULE_LICENSE("GPL");
