// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/sched/cputime.h>
#include <sched/sched.h>
#include <sugov/cpufreq.h>
#include "common.h"
#if IS_ENABLED(CONFIG_MTK_GEARLESS_SUPPORT)
#include "mtk_energy_model/v2/energy_model.h"
#else
#include "mtk_energy_model/v1/energy_model.h"
#endif
#include "eas_plus.h"
#include "eas_trace.h"
#include <linux/sort.h>
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
#include <thermal_interface.h>
#endif
#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
#include "vip.h"
#endif
#include <mt-plat/mtk_irq_mon.h>

MODULE_LICENSE("GPL");

#define CORE_PAUSE_OUT		0
#define IB_ASYM_MISFIT		(0x02)
#define IB_SAME_CLUSTER		(0x01)
#define IB_OVERUTILIZATION	(0x04)

DEFINE_PER_CPU(__u32, active_softirqs);

struct cpumask __cpu_pause_mask;
EXPORT_SYMBOL(__cpu_pause_mask);

struct perf_domain *find_pd(struct perf_domain *pd, int cpu)
{
	while (pd) {
		if (cpumask_test_cpu(cpu, perf_domain_span(pd)))
			return pd;

		pd = pd->next;
	}

	return NULL;
}

static inline bool check_faster_idle_balance(struct sched_group *busiest, struct rq *dst_rq)
{

	int src_cpu = cpumask_first(sched_group_span(busiest));
	int dst_cpu = cpu_of(dst_rq);
	int cpu;

	if (cpu_cap_ceiling(dst_cpu) <= cpu_cap_ceiling(src_cpu))
		return false;

	for_each_cpu(cpu, sched_group_span(busiest)) {
		if (cpu_rq(cpu)->misfit_task_load)
			return true;
	}

	return false;
}

static inline bool check_has_overutilize_cpu(struct cpumask *grp)
{

	int cpu;

	for_each_cpu(cpu, grp) {
		if (cpu_rq(cpu)->nr_running >= 2 &&
			!fits_capacity(cpu_util(cpu), capacity_of(cpu), get_adaptive_margin(cpu)))
			return true;
	}
	return false;
}

void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance)
{
	int src_cpu = -1;
	int dst_cpu = dst_rq->cpu;

	if (cpu_paused(dst_cpu)) {
		*out_balance = 1;
		trace_sched_find_busiest_group(src_cpu, dst_cpu, *out_balance, CORE_PAUSE_OUT);
		return;
	}

	if (busiest) {
		struct perf_domain *pd = NULL;
		int dst_cpu = dst_rq->cpu;
		int fbg_reason = 0;

		pd = rcu_dereference(dst_rq->rd->pd);
		pd = find_pd(pd, dst_cpu);
		if (!pd)
			return;

		src_cpu = cpumask_first(sched_group_span(busiest));

		/*
		 *  1.same cluster
		 *  2.not same cluster but dst_cpu has a higher capacity and
		 *    busiest group has misfit task. The purpose of this condition
		 *    is trying to let misfit task goto hiehger cpu.
		 */
		if (cpumask_test_cpu(src_cpu, perf_domain_span(pd))) {
			*out_balance = 0;
			fbg_reason |= IB_SAME_CLUSTER;
		} else if (check_faster_idle_balance(busiest, dst_rq)) {
			*out_balance = 0;
			fbg_reason |= IB_ASYM_MISFIT;
		} else if (check_has_overutilize_cpu(sched_group_span(busiest))) {
			*out_balance = 0;
			fbg_reason |= IB_OVERUTILIZATION;
		}

		trace_sched_find_busiest_group(src_cpu, dst_cpu, *out_balance, fbg_reason);
	}
}

void mtk_cpu_overutilized(void *data, int cpu, int *overutilized)
{
	struct perf_domain *pd = NULL;
	struct rq *rq = cpu_rq(cpu);
	unsigned long sum_util = 0, sum_cap = 0;
	int i = 0;

	rcu_read_lock();
	pd = rcu_dereference(rq->rd->pd);
	pd = find_pd(pd, cpu);
	if (!pd) {
		rcu_read_unlock();
		return;
	}

	if (cpumask_weight(perf_domain_span(pd)) == 1 &&
		capacity_orig_of(cpu) == SCHED_CAPACITY_SCALE) {
		*overutilized = 0;
		rcu_read_unlock();
		return;
	}

	for_each_cpu(i, perf_domain_span(pd)) {
		sum_util += cpu_util(i);
		sum_cap += capacity_of(i);
	}


	*overutilized = !fits_capacity(sum_util, sum_cap, get_adaptive_margin(cpu));
	trace_sched_cpu_overutilized(cpu, perf_domain_span(pd), sum_util, sum_cap, *overutilized);

	rcu_read_unlock();
}

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
int __read_mostly thermal_headroom[MAX_NR_CPUS]  ____cacheline_aligned;
unsigned long next_update_thermal;
static DEFINE_SPINLOCK(thermal_headroom_lock);

static void update_thermal_headroom(int this_cpu)
{
	int cpu;

	if (spin_trylock(&thermal_headroom_lock)) {
		if (time_before(jiffies, next_update_thermal)) {
			spin_unlock(&thermal_headroom_lock);
			return;
		}

		next_update_thermal = jiffies + thermal_headroom_interval_tick;
		for_each_cpu(cpu, cpu_possible_mask) {
			thermal_headroom[cpu] = get_thermal_headroom(cpu);
		}

		if (trace_sched_next_update_thermal_headroom_enabled())
			trace_sched_next_update_thermal_headroom(jiffies, next_update_thermal);

		spin_unlock(&thermal_headroom_lock);
	}

}

int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order, bool in_irq)
{
	int i, j, cpu, cnt = 0;
	int headroom_order[MAX_NR_CPUS] ____cacheline_aligned;

	if (cpumask_weight(cpus) == 1) {
		cpu = cpumask_first(cpus);
		*cpu_order = cpu;

		return 1;
	}

	if (in_irq) {
		cpu_order[0] = cpumask_first(cpus);

		return cpumask_weight(cpus);
	}

	spin_lock(&thermal_headroom_lock);
	for_each_cpu_and(cpu, cpus, cpu_active_mask) {
		int headroom;

		headroom = thermal_headroom[cpu];

		for (i = 0; i < cnt; i++) {
			if (headroom > headroom_order[i])
				break;
		}

		for (j = cnt; j >= i; j--) {
			headroom_order[j+1] = headroom_order[j];
			cpu_order[j+1] = cpu_order[j];
		}

		headroom_order[i] = headroom;
		cpu_order[i] = cpu;
		cnt++;
	}
	spin_unlock(&thermal_headroom_lock);

	return cnt;
}

#endif

/**
 * em_cpu_energy() - Estimates the energy consumed by the CPUs of a
		performance domain
 * @pd		: performance domain for which energy has to be estimated
 * @max_util	: highest utilization among CPUs of the domain
 * @sum_util	: sum of the utilization of all CPUs in the domain
 *
 * This function must be used only for CPU devices. There is no validation,
 * i.e. if the EM is a CPU type and has cpumask allocated. It is called from
 * the scheduler code quite frequently and that is why there is not checks.
 *
 * Return: the sum of the energy consumed by the CPUs of the domain assuming
 * a capacity state satisfying the max utilization of the domain.
 */
unsigned long mtk_em_cpu_energy(int gear_idx, struct em_perf_domain *pd,
		unsigned long max_util, unsigned long sum_util,
		unsigned long allowed_cpu_cap, struct energy_env *eenv)
{
	unsigned long freq, scale_cpu;
	struct em_perf_state *ps;
	int cpu, this_cpu, opp = -1, wl_type = 0;
#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	unsigned long pwr_eff, cap, freq_legacy, sum_cap = 0;
	struct mtk_em_perf_state *mtk_ps;
#else
	int i;
#endif
	unsigned long dyn_pwr = 0, static_pwr = 0;
	unsigned long energy;
	int *cpu_temp = eenv->cpu_temp;
	unsigned int share_volt = 0, cpu_volt = 0;
	struct dsu_info *dsu = &eenv->dsu;
	unsigned int dsu_opp;
	struct dsu_state *dsu_ps;

	if (!sum_util)
		return 0;

	/*
	 * In order to predict the performance state, map the utilization of
	 * the most utilized CPU of the performance domain to a requested
	 * frequency, like schedutil.
	 */
	this_cpu = cpu = cpumask_first(to_cpumask(pd->cpus));
	scale_cpu = arch_scale_cpu_capacity(cpu);
	ps = &pd->table[pd->nr_perf_states - 1];
#if IS_ENABLED(CONFIG_NONLINEAR_FREQ_CTL)
	mtk_map_util_freq(NULL, max_util, ps->frequency, to_cpumask(pd->cpus), &freq,
			eenv->wl_type);
#else
	max_util = map_util_perf(max_util);
	max_util = min(max_util, allowed_cpu_cap);
	freq = map_util_freq(max_util, ps->frequency, scale_cpu);
#endif
	freq = max(freq, per_cpu(min_freq, cpu));

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	mtk_ps = pd_get_freq_ps(eenv->wl_type, cpu, freq, &opp);
	pwr_eff = mtk_ps->pwr_eff;
	cap = mtk_ps->capacity;
#else
	/*
	 * Find the lowest performance state of the Energy Model above the
	 * requested frequency.
	 */
	for (i = 0; i < pd->nr_perf_states; i++) {
		ps = &pd->table[i];
		if (ps->frequency >= freq)
			break;
	}

	i = min(i, pd->nr_perf_states - 1);
	opp = pd->nr_perf_states - i - 1;
#endif

#if IS_ENABLED(CONFIG_MTK_LEAKAGE_AWARE_TEMP)
	for_each_cpu_and(cpu, to_cpumask(pd->cpus), cpu_online_mask) {
		unsigned int cpu_static_pwr;

		cpu_static_pwr = pd_get_opp_leakage(cpu, opp, cpu_temp[cpu]);
		static_pwr += cpu_static_pwr;
		sum_cap += cap;

		if (trace_sched_leakage_enabled())
			trace_sched_leakage(cpu, opp, cpu_temp[cpu], cpu_static_pwr,
					static_pwr, sum_cap);
	}
	static_pwr = (likely(sum_cap) ? (static_pwr * sum_util) / sum_cap : 0);
#endif

	/*
	 * The capacity of a CPU in the domain at the performance state (ps)
	 * can be computed as:
	 *
	 *             ps->freq * scale_cpu
	 *   ps->cap = --------------------                          (1)
	 *                 cpu_max_freq
	 *
	 * So, ignoring the costs of idle states (which are not available in
	 * the EM), the energy consumed by this CPU at that performance state
	 * is estimated as:
	 *
	 *             ps->power * cpu_util
	 *   cpu_nrg = --------------------                          (2)
	 *                   ps->cap
	 *
	 * since 'cpu_util / ps->cap' represents its percentage of busy time.
	 *
	 *   NOTE: Although the result of this computation actually is in
	 *         units of power, it can be manipulated as an energy value
	 *         over a scheduling period, since it is assumed to be
	 *         constant during that interval.
	 *
	 * By injecting (1) in (2), 'cpu_nrg' can be re-expressed as a product
	 * of two terms:
	 *
	 *             ps->power * cpu_max_freq   cpu_util
	 *   cpu_nrg = ------------------------ * ---------          (3)
	 *                    ps->freq            scale_cpu
	 *
	 * The first term is static, and is stored in the em_perf_state struct
	 * as 'ps->cost'.
	 *
	 * Since all CPUs of the domain have the same micro-architecture, they
	 * share the same 'ps->cost', and the same CPU capacity. Hence, the
	 * total energy of the domain (which is the simple sum of the energy of
	 * all of its CPUs) can be factorized as:
	 *
	 *            ps->cost * \Sum cpu_util
	 *   pd_nrg = ------------------------                       (4)
	 *                  scale_cpu
	 */

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	dyn_pwr = pwr_eff * sum_util;

	if (eenv->wl_support) {
		if (share_buck.gear_idx == gear_idx) {
			cpu_volt = mtk_ps->volt;
			share_volt = (dsu->dsu_volt > cpu_volt) ? dsu->dsu_volt : cpu_volt;
		}

		if (eenv->dsu_freq_thermal != -1)
			eenv->dsu_freq_new = min(mtk_ps->dsu_freq, eenv->dsu_freq_thermal);
		dsu_opp = dsu_get_freq_opp(eenv->dsu_freq_new);
		dsu_ps = dsu_get_opp_ps(eenv->wl_type, dsu_opp);
		eenv->dsu_volt_new = dsu_ps->volt;
		wl_type = eenv->wl_type;

		if (trace_sched_dsu_freq_enabled())
			trace_sched_dsu_freq(gear_idx, eenv->dsu_freq_new, eenv->dsu_volt_new, freq,
					mtk_ps->freq, dyn_pwr, share_volt, cpu_volt);

		if (share_volt > cpu_volt)
			dyn_pwr = (unsigned long long)dyn_pwr * (unsigned long long)share_volt *
				(unsigned long long)share_volt / cpu_volt / cpu_volt;
	}

	/* for pd_opp_capacity is scaled based on maximum scale 1024, so cost = pwr_eff * 1024 */
	if (trace_sched_em_cpu_energy_enabled()) {
		freq_legacy = pd_get_opp_freq_legacy(this_cpu, pd_get_freq_opp_legacy(this_cpu,
											freq));
		trace_sched_em_cpu_energy(wl_type, opp, freq_legacy, "pwr_eff", pwr_eff,
			scale_cpu, dyn_pwr, static_pwr);
	}
#else
	dyn_pwr = (ps->cost * sum_util / scale_cpu);
	if (trace_sched_em_cpu_energy_enabled())
		trace_sched_em_cpu_energy(wl_type, opp, freq, "ps->cost", ps->cost,
			scale_cpu, dyn_pwr, static_pwr);
#endif

	energy = dyn_pwr + static_pwr;

	return energy;
}

#define OFFS_THERMAL_LIMIT_S 0x1208
#define THERMAL_INFO_SIZE 200

static void __iomem *sram_base_addr;
int init_sram_info(void)
{
	struct device_node *dvfs_node;
	struct platform_device *pdev_temp;
	struct resource *csram_res;

	dvfs_node = of_find_node_by_name(NULL, "cpuhvfs");
	if (dvfs_node == NULL) {
		pr_info("failed to find node @ %s\n", __func__);
		return -ENODEV;
	}

	pdev_temp = of_find_device_by_node(dvfs_node);
	if (pdev_temp == NULL) {
		pr_info("failed to find pdev @ %s\n", __func__);
		return -EINVAL;
	}

	csram_res = platform_get_resource(pdev_temp, IORESOURCE_MEM, 1);

	if (csram_res)
		sram_base_addr =
			ioremap(csram_res->start + OFFS_THERMAL_LIMIT_S, THERMAL_INFO_SIZE);
	else {
		pr_info("%s can't get resource\n", __func__);
		return -ENODEV;
	}

	if (!sram_base_addr) {
		pr_info("Remap thermal info failed\n");

		return -EIO;
	}

	return 0;
}

inline void update_thermal_pressure_capacity(unsigned int current_thermal_freq, unsigned int this_cpu)
{
	unsigned int gear_id, cpu;
	unsigned long max_capacity, capacity, th_pressure;
	unsigned long last_th_pressure = READ_ONCE(per_cpu(thermal_pressure, this_cpu));
	struct cpumask *cpus;

	capacity = pd_get_freq_util(this_cpu, current_thermal_freq);
	max_capacity = arch_scale_cpu_capacity(this_cpu);
	th_pressure = max_capacity - capacity;
	if (th_pressure == last_th_pressure)
		return;

	gear_id = topology_cluster_id(this_cpu);
	cpus = get_gear_cpumask(gear_id);
	for_each_cpu(cpu, cpus)
		WRITE_ONCE(per_cpu(thermal_pressure, cpu), th_pressure);
}

void mtk_tick_entry(void *data, struct rq *rq)
{

	struct em_perf_domain *pd;
	unsigned int this_cpu = cpu_of(rq), gear_id;
	unsigned int freq_thermal;
	bool sbb_trigger;
	u64 idle_time, wall_time, cpu_utilize;
	struct sbb_cpu_data *sbb_data = per_cpu(sbb, rq->cpu);

	irq_log_store();

	if (is_wl_support())
		update_wl_tbl(this_cpu);

	sbb_trigger = is_sbb_trigger(rq);

	if (sbb_trigger) {
		if (sbb_data->tick_start) {
			idle_time = get_cpu_idle_time(rq->cpu, &wall_time, 1);

			cpu_utilize = 100 - (100 * (idle_time -
				sbb_data->idle_time)) /
				(wall_time - sbb_data->wall_time);

			sbb_data->idle_time = idle_time;
			sbb_data->wall_time = wall_time;

			if (cpu_utilize >=
				get_sbb_active_ratio_gear(topology_cluster_id(this_cpu))) {
				sbb_data->active = 1;

				sbb_data->boost_factor =
				min_t(u32, sbb_data->boost_factor * 2, 4);

				sbb_data->cpu_utilize = cpu_utilize;
			} else {
				sbb_data->active = 0;
				sbb_data->boost_factor = 1;
			}
		} else {
			sbb_data->active = 0;
			sbb_data->tick_start = 1;
			sbb_data->boost_factor = 1;
		}
	} else {
		sbb_data->active = 0;
		sbb_data->tick_start = 0;
		sbb_data->boost_factor = 1;
	}

#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
	update_thermal_headroom(this_cpu);
#endif

	irq_log_store();
	pd = em_cpu_get(this_cpu);
	if (!pd)
		return;

	if (this_cpu != cpumask_first(to_cpumask(pd->cpus)))
		return;

	gear_id = topology_cluster_id(this_cpu);
	irq_log_store();
	freq_thermal = get_cpu_ceiling_freq (gear_id);
	update_thermal_pressure_capacity(freq_thermal, this_cpu);

	if (trace_sched_frequency_limits_enabled())
		trace_sched_frequency_limits(this_cpu, freq_thermal);
	irq_log_store();
}

/*
 * Enable/Disable honoring sync flag in energy-aware wakeups
 */
unsigned int sched_sync_hint_enable = 1;
void set_wake_sync(unsigned int sync)
{
	sched_sync_hint_enable = sync;
}
EXPORT_SYMBOL_GPL(set_wake_sync);

unsigned int get_wake_sync(void)
{
	return sched_sync_hint_enable;
}
EXPORT_SYMBOL_GPL(get_wake_sync);

void mtk_set_wake_flags(void *data, int *wake_flags, unsigned int *mode)
{
	if (!sched_sync_hint_enable)
		*wake_flags &= ~WF_SYNC;
}

unsigned int new_idle_balance_interval_ns  =  1000000;
unsigned int thermal_headroom_interval_tick =  1;

void set_newly_idle_balance_interval_us(unsigned int interval_us)
{
	new_idle_balance_interval_ns = interval_us * 1000;

	trace_sched_newly_idle_balance_interval(interval_us);
}
EXPORT_SYMBOL_GPL(set_newly_idle_balance_interval_us);

unsigned int get_newly_idle_balance_interval_us(void)
{
	return new_idle_balance_interval_ns / 1000;
}
EXPORT_SYMBOL_GPL(get_newly_idle_balance_interval_us);

void set_get_thermal_headroom_interval_tick(unsigned int tick)
{
	thermal_headroom_interval_tick = tick;

	trace_sched_headroom_interval_tick(tick);
}
EXPORT_SYMBOL_GPL(set_get_thermal_headroom_interval_tick);

unsigned int get_thermal_headroom_interval_tick(void)
{
	return thermal_headroom_interval_tick;
}
EXPORT_SYMBOL_GPL(get_thermal_headroom_interval_tick);

static DEFINE_RAW_SPINLOCK(migration_lock);

int select_idle_cpu_from_domains(struct task_struct *p,
					struct perf_domain **prefer_pds, unsigned int len)
{
	unsigned int i = 0;
	struct perf_domain *pd;
	int cpu, best_cpu = -1;

	for (; i < len; i++) {
		pd = prefer_pds[i];
		for_each_cpu_and(cpu, perf_domain_span(pd),
						cpu_active_mask) {
			if (!cpumask_test_cpu(cpu, p->cpus_ptr))
				continue;
			if (available_idle_cpu(cpu)) {
				best_cpu = cpu;
				break;
			}
		}
		if (best_cpu != -1)
			break;
	}

	return best_cpu;
}

int select_bigger_idle_cpu(struct task_struct *p)
{
	struct root_domain *rd = cpu_rq(smp_processor_id())->rd;
	struct perf_domain *pd, *prefer_pds[MAX_NR_CPUS];
	int cpu = task_cpu(p), bigger_idle_cpu = -1;
	unsigned int i = 0;
	long max_capacity = cpu_cap_ceiling(cpu);
	long capacity;

	rcu_read_lock();
	pd = rcu_dereference(rd->pd);

	for (; pd; pd = pd->next) {
		capacity = cpu_cap_ceiling(cpumask_first(perf_domain_span(pd)));
		if (capacity > max_capacity &&
			cpumask_intersects(p->cpus_ptr, perf_domain_span(pd))) {
			prefer_pds[i++] = pd;
		}
	}

	if (i != 0)
		bigger_idle_cpu = select_idle_cpu_from_domains(p, prefer_pds, i);

	rcu_read_unlock();
	return bigger_idle_cpu;
}

void check_for_migration(struct task_struct *p)
{
	int new_cpu = -1, better_idle_cpu = -1;
	int cpu = task_cpu(p);
	struct rq *rq = cpu_rq(cpu);

	irq_log_store();

	if (rq->misfit_task_load) {
		struct em_perf_domain *pd;
		struct cpufreq_policy *policy;
		int opp_curr = 0, thre = 0, thre_idx = 0;

		if (rq->curr->__state != TASK_RUNNING ||
			rq->curr->nr_cpus_allowed == 1)
			return;

		pd = em_cpu_get(cpu);
		if (!pd)
			return;

		thre_idx = (pd->nr_perf_states >> 3) - 1;
		if (thre_idx >= 0)
			thre = pd->table[thre_idx].frequency;

		policy = cpufreq_cpu_get(cpu);
		irq_log_store();

		if (policy) {
			opp_curr = policy->cur;
			cpufreq_cpu_put(policy);
		}

		if (opp_curr <= thre) {
			irq_log_store();
			return;
		}

		raw_spin_lock(&migration_lock);
		irq_log_store();
		raw_spin_lock(&p->pi_lock);
		irq_log_store();

		new_cpu = p->sched_class->select_task_rq(p, cpu, WF_TTWU);
		irq_log_store();

		raw_spin_unlock(&p->pi_lock);

		if ((new_cpu < 0) || new_cpu >= MAX_NR_CPUS ||
			(cpu_cap_ceiling(new_cpu) <= cpu_cap_ceiling(cpu)))
			better_idle_cpu = select_bigger_idle_cpu(p);

		if (better_idle_cpu >= 0)
			new_cpu = better_idle_cpu;

		if (new_cpu < 0) {
			raw_spin_unlock(&migration_lock);
			irq_log_store();
			return;
		}

		irq_log_store();
		if ((better_idle_cpu >= 0) ||
			(new_cpu < MAX_NR_CPUS && new_cpu >= 0 &&
			(cpu_cap_ceiling(new_cpu) > cpu_cap_ceiling(cpu)))) {
			raw_spin_unlock(&migration_lock);

			migrate_running_task(new_cpu, p, rq, MIGR_TICK_PULL_MISFIT_RUNNING);
			irq_log_store();
		} else {
#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
			int thre_rot = 0, thre_rot_idx = 0;

			thre_rot_idx = (pd->nr_perf_states >> 1) - 1;
			if (thre_rot_idx >= 0)
				thre_rot = pd->table[thre_rot_idx].frequency;

			if (opp_curr > thre_rot) {
				task_check_for_rotation(rq);
				irq_log_store();
			}

#endif
			raw_spin_unlock(&migration_lock);
		}
	}
	irq_log_store();
}

void hook_scheduler_tick(void *data, struct rq *rq)
{

	struct root_domain *rd = rq->rd;

	rcu_read_lock();
	rd->android_vendor_data1[0] = system_has_many_heavy_task();
	rcu_read_unlock();

	if (rq->curr->policy == SCHED_NORMAL)
		check_for_migration(rq->curr);
}

void mtk_hook_after_enqueue_task(void *data, struct rq *rq,
				struct task_struct *p, int flags)
{
	int this_cpu = smp_processor_id();
	struct rq *this_rq = cpu_rq(this_cpu);
	struct sugov_rq_data *sugov_data_ptr;
	struct sugov_rq_data *sugov_data_ptr2;

	irq_log_store();

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
	rotat_after_enqueue_task(data, rq, p);
#endif
	irq_log_store();

#if IS_ENABLED(CONFIG_MTK_SCHED_VIP_TASK)
	if (vip_fair_task(p))
		vip_enqueue_task(rq, p);
#endif

#if IS_ENABLED(CONFIG_MTK_CPUFREQ_SUGOV_EXT)
	irq_log_store();
	sugov_data_ptr = &((struct mtk_rq *) rq->android_vendor_data1)->sugov_data;
	sugov_data_ptr2 = &((struct mtk_rq *) this_rq->android_vendor_data1)->sugov_data;
	if (READ_ONCE(sugov_data_ptr->enq_update_dsu_freq) == true
			|| READ_ONCE(sugov_data_ptr2->enq_dvfs) == true) {
		cpufreq_update_util(rq, 0);
		WRITE_ONCE(sugov_data_ptr2->enq_dvfs, false);
	}
	WRITE_ONCE(sugov_data_ptr->enq_ing, false);
#endif
	irq_log_store();
}

#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
static inline
unsigned long aligned_freq_to_legacy_freq(int cpu, unsigned long freq)
{
	return pd_get_opp_freq_legacy(cpu, pd_get_freq_opp_legacy(cpu, freq));
}

__always_inline
unsigned long calc_pwr_eff(int wl_type, int cpu, unsigned long cpu_util)
{
	int opp;
	struct mtk_em_perf_state *ps;
	unsigned long static_pwr_eff, pwr_eff;

	ps = pd_get_util_ps(wl_type, cpu, map_util_perf(cpu_util), &opp);

	static_pwr_eff = mtk_get_leakage(cpu, opp, get_cpu_temp(cpu)/1000) / ps->capacity;
	pwr_eff = ps->pwr_eff + static_pwr_eff;

	if (trace_sched_calc_pwr_eff_enabled())
		trace_sched_calc_pwr_eff(cpu, cpu_util, opp, ps->capacity,
				ps->pwr_eff, static_pwr_eff, pwr_eff);

	return pwr_eff;
}
#else
__always_inline
unsigned long calc_pwr_eff(int wl_type, int cpu, unsigned long task_util)
{
	return 0;
}
#endif

#if IS_ENABLED(CONFIG_MTK_EAS)
void mtk_pelt_rt_tp(void *data, struct rq *rq)
{
	cpufreq_update_util(rq, 0);
}

void mtk_sched_switch(void *data, unsigned int sched_mode, struct task_struct *prev,
		struct task_struct *next, struct rq *rq)
{
	if (next->pid == 0)
		per_cpu(sbb, rq->cpu)->active = 0;

}
#endif

int set_util_est_ctrl(bool enable)
{
	sysctl_util_est = enable;
	return 0;
}
