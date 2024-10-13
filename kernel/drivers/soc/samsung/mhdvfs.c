/*
 * Memory Hierarchy DVFS
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define CREATE_TRACE_POINTS
#include <trace/events/mhdvfs.h>
#undef CREATE_TRACE_POINTS

#include <linux/sched.h>
#include <linux/of_device.h>
#include <linux/devfreq.h>
#include <soc/samsung/exynos-devfreq.h>
#include <linux/exynos-dsufreq.h>
#include <linux/ems.h>

#include <trace/hooks/sched.h>

#include "../../../kernel/sched/ems/ems.h"

enum {
	NOT_BUSY,
	NORMAL,
	BUSY,
	SUPER_BUSY,
};

struct tlb {
	atomic64_t	tg_last_update;
	int		tg_busy;
	int		tg_busy_count;
	bool		group_leader_freed;
};

#define task_tlb_avd(p)		(task_avd(p)->mhdvfs_task_tlb)
#define task_tlb(p)		((struct tlb *)task_tlb_avd(p))

static unsigned long tlb_task_active_util(struct task_struct *p, u64 now,
					int period_cut)
{
	struct mlt_part *mlt = &task_mlt(p)->part;
	int pos, value = 0, active_period_count;

	period_cut = min(period_cut, MLT_PERIOD_COUNT / 2);

	/*
	 * Due to the time difference between each CPU, now is earlier than
	 * last_updated rarely. In this case, the time difference is very
	 * small, so we determine that all periods are active.
	 */
	active_period_count = MLT_PERIOD_COUNT - period_cut;
	if (likely(now > mlt->last_updated))
		active_period_count -= div64_u64(now - mlt->last_updated, MLT_PERIOD_SIZE);

	if (active_period_count <= 0)
		return 0;

	BUG_ON(active_period_count > MLT_PERIOD_COUNT);

	value = mlt->recent;

	pos = mlt->cur_period;
	while (active_period_count--) {
		value += mlt->periods[pos];
		pos = (pos - 1 + MLT_PERIOD_COUNT) % MLT_PERIOD_COUNT;
	}

	return value / (MLT_PERIOD_COUNT + 1 - period_cut);
}

static unsigned long tlb_tg_active_util(struct task_struct *group_leader,
				unsigned long *util_avg, int *task_count)
{
	struct tlb *tlb = task_tlb(group_leader);
	struct task_struct *p;
	u64 now = sched_clock();
	unsigned long util = 0;
	int count = 0, period_cut;

	if (unlikely(!tlb))
		return 0;

	if (tlb->tg_busy > NORMAL)
		period_cut = 0;
	else
		period_cut = tlb->tg_busy_count >> 1;

	list_for_each_entry_rcu(p, &group_leader->thread_group, thread_group) {
		unsigned long tmp = tlb_task_active_util(p, now, period_cut);

		util += tmp;
		if (tmp)
			count++;
	}

	if (!count)
		return 0;

	if (util_avg)
		*util_avg = util / count;
	if (task_count)
		*task_count = count;

	return util;
}

static int tlb_check_tg_busy(struct task_struct *group_leader)
{
	unsigned long util_sum, util_avg;
	int task_count, cpu_count = cpumask_weight(group_leader->cpus_ptr);

	util_sum = tlb_tg_active_util(group_leader, &util_avg, &task_count);

	trace_tlb_check_tg_busy(group_leader, util_sum, util_avg, task_count);

	if (util_sum > (SCHED_CAPACITY_SCALE * cpu_count >> 1)) {
		if (util_avg > SCHED_CAPACITY_SCALE >> 1)
			return SUPER_BUSY;

		return BUSY;
	}

	if (util_sum < (SCHED_CAPACITY_SCALE * cpu_count >> 3))
		return NOT_BUSY;

	return NORMAL;
}

static inline bool tlb_check_tg(struct task_struct *p)
{
	int idx;
	struct tlb *tlb;

	if (is_idle_task(p))
		return false;

	if (p->exit_state == EXIT_DEAD || p->exit_code == SIGKILL)
		return false;

	tlb = task_tlb(p);
	if (unlikely(!tlb))
		return false;

	if (tlb->group_leader_freed)
		return false;

	idx = cpuctl_task_group_idx(p);

	return idx == 3; /* 3 : index of topapp in cpu cgroup subsystem */
}

static int tlb_system_busy;
static void tlb_update_load(struct task_struct *p)
{
	struct tlb *tlb;
	u64 now = jiffies, last_update;
	int busy;

	if (!tlb_check_tg(p))
		return;

	tlb = task_tlb(p->group_leader);
	if (unlikely(!tlb))
		return;

	last_update = atomic64_read(&tlb->tg_last_update);
	if (now < last_update + 1)
		return;

	if (!atomic64_try_cmpxchg(&tlb->tg_last_update, &last_update, now))
		return;

	busy = tlb_check_tg_busy(p->group_leader);

	switch (busy) {
	case NOT_BUSY:
		if (tlb->tg_busy > NORMAL) {
			tlb->tg_busy = busy;
			tlb_system_busy = 0;
		}
		break;
	case NORMAL:
		break;
	case BUSY:
	case SUPER_BUSY:
		tlb->tg_busy_count++;
		if (tlb->tg_busy != busy) {
			tlb->tg_busy = busy;
			tlb_system_busy = busy;
		}
		break;
	default:
		break;
	}
}

void tlb_task_alloc(struct task_struct *p)
{
	struct tlb *tlb;

	tlb = kzalloc(sizeof(struct tlb), GFP_KERNEL);
	if (!tlb) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return;
	}

	task_tlb_avd(p) = (u64)tlb;
}
EXPORT_SYMBOL_GPL(tlb_task_alloc);

void tlb_task_free(struct task_struct *p)
{
	struct tlb *tlb = task_tlb(p);

	if (unlikely(!tlb))
		return;

	if (p->group_leader == p) {
		struct task_struct *group_leader = p;

		list_for_each_entry_rcu(p,
				&group_leader->thread_group, thread_group) {
			tlb = task_tlb(p);
			if (unlikely(!tlb))
				continue;

			tlb->group_leader_freed = true;
		}
	}

	kfree(tlb);
	task_tlb_avd(p) = 0;
}
EXPORT_SYMBOL_GPL(tlb_task_free);

static void tlb_init(void)
{
	struct task_struct *p;

	list_for_each_entry_rcu(p, &init_task.tasks, tasks) {
		get_task_struct(p);
		tlb_task_alloc(p);
		put_task_struct(p);
	}
}

#define VENDOR_NR_CPUS	CONFIG_VENDOR_NR_CPUS

/**************************************************************
 * uarch information management and AMU access functions:
 */

struct uarch_info {
	u64 core_cycles;
	u64 const_cycles;
	u64 inst_ret;
	u64 mem_stall;

	u64 last_core_cycles;
	u64 last_const_cycles;
	u64 last_inst_ret;
	u64 last_mem_stall;

	u64 last_profile_jiffy;
};

static struct uarch_info __percpu *pcpu_uinfo;

#define read_corecnt()	read_sysreg_s(SYS_AMEVCNTR0_CORE_EL0)
#define read_constcnt()	read_sysreg_s(SYS_AMEVCNTR0_CONST_EL0)
#define read_instret()	read_sysreg_s(SYS_AMEVCNTR0_INST_RET_EL0)
#define read_memstall()	read_sysreg_s(SYS_AMEVCNTR0_MEM_STALL)

static void this_cpu_uarch_info_update(void)
{
	/* Uarch information can be accessed by only this cpu */
	struct uarch_info *uinfo = per_cpu_ptr(pcpu_uinfo, smp_processor_id());

	uinfo->core_cycles = read_corecnt();
	uinfo->const_cycles = read_constcnt();
	uinfo->inst_ret = read_instret();
	uinfo->mem_stall = read_memstall();
}

struct mhdvfs_freq_avg {
	unsigned int	freq;
	u64		time;	/* time to start operating with frequency */
};

enum {
	DOMAIN_DSU,
	DOMAIN_MIF,
};

#define MAX_LEVEL		5
#define FREQ_AVG_BUF_SIZE	100

#define POLICY_PRESS		(1 << 0)	/* frequency control method, press or raise? */
#define POLICY_MULTI_LEVEL	(1 << 1)	/* frequency control level, multi or single level? */
#define POLICY_SOFT_LANDING	(1 << 2)	/* frequency control release method,
						   soft landing or at once? */
#define POLICY_FREQ_MAX		(1 << 3)	/* do frequency control based on max or avg freq */

struct mhdvfs_domain {
	int			id;
	bool			enabled;
	int			policy;
	int			req_level;
	bool			need_change;

	void			*pm_qos_req;
	unsigned int		default_qos;
	unsigned int		base_freq;

	unsigned int		*freq_table;
	int			freq_table_size;

	spinlock_t		freq_avg_lock;
	struct mhdvfs_freq_avg	freq_avg[FREQ_AVG_BUF_SIZE];
	int			freq_avg_pos;
	u64			last_updated;

	unsigned int		last_avg_freq;
	unsigned int		last_max_freq;

	u32			ipc_crit;
	u32			sw_ipc_crit;

	struct {
		u64		last_updated;
		u64		time[MAX_LEVEL];
		u64		usage[MAX_LEVEL];
	} stat;
};

struct {
	struct work_struct	work;

	struct mhdvfs_domain	mif_domain;
	struct mhdvfs_domain	dsu_domain;
	struct mhdvfs_domain	cpu_domain;

	u64			last_jiffies;
	int			interval_jiffies;
	struct cpumask		monitor_cpus;
	struct cpumask		super_wide_cpus;

	spinlock_t		lock;

	u32			*mpi_crit_table;
	u32			mpi_crit_table_size;

	bool			active;
} mhdvfs_data;

#define dsu_domain	(&mhdvfs_data.dsu_domain)
#define mif_domain	(&mhdvfs_data.mif_domain)
#define cpu_domain	(&mhdvfs_data.cpu_domain)

static inline int mhdvfs_enabled(void)
{
	return dsu_domain->enabled || mif_domain->enabled || cpu_domain->enabled;
}

static inline unsigned int mhdvfs_get_cur_freq(struct mhdvfs_domain *d)
{
	if (unlikely(!d->freq_avg_pos))
		return 0;

	return d->freq_avg[d->freq_avg_pos - 1].freq;
}

static void mhdvfs_reset_freq_info(struct mhdvfs_domain *d)
{
	u64 now = sched_clock();

	d->freq_avg[0].freq = d->freq_avg[d->freq_avg_pos].freq;
	d->freq_avg[0].time = now;
	d->freq_avg_pos = 1;
	d->last_updated = now;
}

static void mhdvfs_update_freq_info(struct mhdvfs_domain *d)
{
	u64 now = sched_clock(), avg_freq = 0;
	unsigned int freq, max_freq = 0;
	int i = 0;

	d->last_avg_freq = 0;
	d->last_max_freq = 0;

	spin_lock(&d->freq_avg_lock);

	if (unlikely(!d->freq_avg_pos)) {
		spin_unlock(&d->freq_avg_lock);
		return;
	}

	while (i < d->freq_avg_pos) {
		u64 start, end;

		freq = d->freq_avg[i].freq;

		/* find max frequency within last period */
		if (max_freq < freq)
			max_freq = freq;

		/* calculate average frequency of last period */
		start = d->freq_avg[i].time;
		if (i + 1 == d->freq_avg_pos)
			end = now;
		else
			end = d->freq_avg[i + 1].time;

		avg_freq += freq * (end - start);

		if (unlikely(++i >= FREQ_AVG_BUF_SIZE - 1))
			break;
	}

	d->last_avg_freq = (unsigned int)div64_u64(avg_freq, now - d->last_updated);
	d->last_max_freq = max_freq;

	trace_mhdvfs_update_freq_info(d->id, d->last_avg_freq, d->last_max_freq);

	d->freq_avg[0].freq = freq;
	d->freq_avg[0].time = now;
	d->freq_avg_pos = 1;
	d->last_updated = now;

	spin_unlock(&d->freq_avg_lock);
}

static void mhdvfs_accumulate_freq(struct mhdvfs_domain *d, int freq)
{
	unsigned long flags;

	if (!mhdvfs_enabled())
		return;

	spin_lock_irqsave(&d->freq_avg_lock, flags);

	if (d->freq_avg_pos >= FREQ_AVG_BUF_SIZE) {
		WARN_ON(1);
		spin_unlock_irqrestore(&d->freq_avg_lock, flags);
		return;
	}

	d->freq_avg[d->freq_avg_pos].freq = freq;
	d->freq_avg[d->freq_avg_pos].time = sched_clock();
	d->freq_avg_pos++;

	spin_unlock_irqrestore(&d->freq_avg_lock, flags);
}

static void mhdvfs_update_stat(struct mhdvfs_domain *d)
{
	u64 now = sched_clock();
	int lv = d->req_level;

	d->stat.usage[lv]++;
	d->stat.time[lv] += now - d->stat.last_updated;

	d->stat.last_updated = now;
}

static void
mhdvfs_pm_qos_update_request(struct mhdvfs_domain *d, int target_freq)
{
	if (d->id == DOMAIN_MIF)
		exynos_pm_qos_update_request(d->pm_qos_req, target_freq);
	else if (d->id == DOMAIN_DSU)
		dev_pm_qos_update_request(d->pm_qos_req, target_freq);
	else
		BUG_ON(1);
}

static void
mhdvfs_change_freq(struct mhdvfs_domain *d)
{
	unsigned int base_freq = 0;
	int adjust_level;
	int lv, req_freq = d->default_qos;

	if (!d->need_change)
		return;

	if (!d->req_level) {
		adjust_level = 0;
		d->base_freq = 0;
		goto out;
	}

	if (d->policy & POLICY_PRESS)
		adjust_level = -d->req_level;
	else
		adjust_level = d->req_level;

	/*
	 * We choice the current frequency as base frequency for control
	 * when the state is changed from non-control to control.
	 */
	if (!d->base_freq) {
		if (d->policy & POLICY_FREQ_MAX)
			d->base_freq = d->last_max_freq;
		else
			d->base_freq = d->last_avg_freq;
	}

	base_freq = d->base_freq;

	for (lv = 0; lv < d->freq_table_size; lv++)
		if (base_freq <= d->freq_table[lv])
			break;

	lv = clamp_val(lv + adjust_level, 0, d->freq_table_size - 1);
	req_freq = d->freq_table[lv];

out:
	trace_mhdvfs_change_freq(d->id, base_freq, req_freq, adjust_level);
	mhdvfs_pm_qos_update_request(d, req_freq);
}

static void mhdvfs_update_work(struct work_struct *work)
{
	mhdvfs_change_freq(dsu_domain);
	mhdvfs_change_freq(mif_domain);
	mhdvfs_change_freq(cpu_domain);
}

static int  __mhdvfs_kick(struct mhdvfs_domain *d, int next_level)
{
	d->need_change = false;

	if (next_level != d->req_level) {
		mhdvfs_update_stat(d);
		d->req_level = next_level;
		d->need_change = true;
		return 1;
	}

	return 0;
}

static void mhdvfs_kick(int dsu_level, int mif_level, int cpu_level)
{
	int ret;

	if (work_busy(&mhdvfs_data.work))
		return;

	ret = __mhdvfs_kick(dsu_domain, dsu_level);
	ret += __mhdvfs_kick(mif_domain, mif_level);
	ret += __mhdvfs_kick(cpu_domain, cpu_level);

	if (!ret)
		return;

	trace_mhdvfs_kick(dsu_level, mif_level, cpu_level);

	schedule_work_on(smp_processor_id(), &mhdvfs_data.work);
}

static int mhdvfs_get_mpi_criterion(void)
{
	int mif_freq = mif_domain->last_avg_freq;
	int i;

	for (i = 0; i < mhdvfs_data.mpi_crit_table_size; i += 2)
		if (mhdvfs_data.mpi_crit_table[i] >= mif_freq)
			return mhdvfs_data.mpi_crit_table[i + 1];

	return INT_MAX;
}

static int mhdvfs_get_next_level(struct mhdvfs_domain *d, int mpi,
				int mpi_crit, int ipc, int sw_ipc)
{
	int level = 0, next_level = 0;

	if (!d->enabled)
		return 0;

	/*
	 * If super wide cpu processes a lot of instruction, we do not control
	 * DVFS to avoid interfering with the processing of the super wide cpu.
	 */
	if (sw_ipc > d->sw_ipc_crit)
		goto skip;

	if (d->policy & POLICY_MULTI_LEVEL) {
		/*
		 * Multi level DVFS control. If MPI is
		 *  - smaller than 100% of MPI_crit	--> 0 level
		 *  - between 100%~125% of MPI_crit	--> 1 level
		 *  - between 125%~150% of MPI_crit	--> 2 level
		 *  - between 150%~175% of MPI_crit	--> 3 level
		 *  - greater than 175% of MIP_crit	--> 4 level
		 */
		if (d->ipc_crit < ipc || mpi_crit > mpi)
			level = 0;
		else if (mpi_crit <= mpi && mpi < mpi_crit * 5 / 4)
			level = 1;
		else if (mpi_crit * 5 / 4 <= mpi && mpi < mpi_crit * 6 / 4)
			level = 2;
		else if (mpi_crit * 6 / 4 <= mpi && mpi < mpi_crit * 7 / 4)
			level = 3;
		else if (mpi_crit * 7 / 4 <= mpi)
			level = 4;
	} else {
		/*
		 * Support single level DVFS control. Kick DVFS if
		 *  - MPI > 125% of MPI_crit and
		 *  - IPC < IPC_crit.
		 */
		level = !!(mpi >= mpi_crit * 5 / 4 && ipc < d->ipc_crit);
	}

	if (d->policy & POLICY_SOFT_LANDING)
		next_level = level ? level : max(d->req_level - 1, 0);
	else
		next_level = level ? level : 0;

	next_level = clamp_val(next_level, 0, MAX_LEVEL - 1);

skip:
	trace_mhdvfs_next_level(d->id, d->req_level, next_level,
			mpi, mpi_crit, ipc, d->ipc_crit, sw_ipc, d->sw_ipc_crit);

	return next_level;
}

static void mhdvfs_release(void)
{
	mhdvfs_kick(0, 0, 0);
}

#define CONST_CYCLES_PER_JIFFY	104000
static void do_mhdvfs(void *data, struct rq *rq)
{
	u64 now = jiffies;
	u64 ipc[VENDOR_NR_CPUS], mspc[VENDOR_NR_CPUS], ar[VENDOR_NR_CPUS];
	int ipc_sum = 0, mspc_sum = 0, mpi, mpi_crit;
	int max_ipc, max_ipc_ar, max_actual_ipc = 0, sw_ipc = 0;
	int dsu_level = 0, mif_level = 0, cpu_level = 0;
	int cpu;
	struct cpumask active_sw_cpus;

	tlb_update_load(rq->curr);

	this_cpu_uarch_info_update();

	if (!mhdvfs_enabled())
		return;

	if (!spin_trylock(&mhdvfs_data.lock))
		return;

	if (now < mhdvfs_data.last_jiffies + mhdvfs_data.interval_jiffies) {
		spin_unlock(&mhdvfs_data.lock);
		return;
	}

	mhdvfs_data.last_jiffies = now;

	if (!mhdvfs_data.active || tlb_system_busy) {
		mhdvfs_reset_freq_info(mif_domain);
		mhdvfs_reset_freq_info(dsu_domain);
		goto skip;
	}

	/* gather uarch infomration */
	for_each_cpu(cpu, cpu_active_mask) {
		struct uarch_info *uinfo = per_cpu_ptr(pcpu_uinfo, cpu);

		ipc[cpu] = div64_u64((uinfo->inst_ret - uinfo->last_inst_ret) << 10,
				uinfo->core_cycles - uinfo->last_core_cycles);
		mspc[cpu] = div64_u64((uinfo->mem_stall - uinfo->last_mem_stall) << 10,
				uinfo->core_cycles - uinfo->last_core_cycles);
		ar[cpu] = div64_u64((uinfo->const_cycles - uinfo->last_const_cycles) << 10,
				(now - uinfo->last_profile_jiffy) * CONST_CYCLES_PER_JIFFY);

		ar[cpu] = min_t(u64, ar[cpu], 1024);

		if (uinfo->last_const_cycles != uinfo->const_cycles)
			uinfo->last_profile_jiffy = now;

		uinfo->last_core_cycles = uinfo->core_cycles;
		uinfo->last_const_cycles = uinfo->const_cycles;
		uinfo->last_inst_ret = uinfo->inst_ret;
		uinfo->last_mem_stall = uinfo->mem_stall;
	}

	/* get IPC, MSPC and MPI of monitor CPUs */
	for_each_cpu_and(cpu, cpu_active_mask, &mhdvfs_data.monitor_cpus) {
		int actual_ipc = (ipc[cpu] * ar[cpu]) >> 10;

		if (max_actual_ipc < actual_ipc) {
			max_actual_ipc = actual_ipc;
			max_ipc = (int)ipc[cpu];	/* for debugging */
			max_ipc_ar = (int)ar[cpu];	/* for debugging */
		}

		ipc_sum += ipc[cpu];
		mspc_sum += mspc[cpu];
	}

	/* get average IPC of super wide cpus */
	cpumask_and(&active_sw_cpus, cpu_active_mask,
				&mhdvfs_data.super_wide_cpus);
	if (!cpumask_empty(&active_sw_cpus)) {
		for_each_cpu(cpu, &active_sw_cpus)
			sw_ipc += (ipc[cpu] * ar[cpu]) >> 10;

		sw_ipc /= cpumask_weight(&active_sw_cpus);
	}

	mhdvfs_update_freq_info(mif_domain);
	mhdvfs_update_freq_info(dsu_domain);

	if (ipc_sum)
		mpi = (mspc_sum << 10) / ipc_sum;
	else
		mpi = 0;
	mpi_crit = mhdvfs_get_mpi_criterion();

	trace_mhdvfs_profile(ipc_sum, mspc_sum, mpi, mpi_crit,
			max_actual_ipc, max_ipc, max_ipc_ar);

	dsu_level = mhdvfs_get_next_level(dsu_domain, mpi, mpi_crit,
					max_actual_ipc, sw_ipc);
	mif_level = mhdvfs_get_next_level(mif_domain, mpi, mpi_crit,
					max_actual_ipc, sw_ipc);
	cpu_level = 0;

skip:
	mhdvfs_kick(dsu_level, mif_level, cpu_level);
	spin_unlock(&mhdvfs_data.lock);
}

/**************************************************************
 * DSU frequency control functions:
 */

static int mhdvfs_dsufreq_notifier(struct notifier_block *nb,
					unsigned long event, void *data)
{
	unsigned int freq = *(unsigned int *)data;

	if (!dsu_domain->enabled)
		return NOTIFY_OK;

	if (mhdvfs_get_cur_freq(dsu_domain) == freq)
		return NOTIFY_OK;

	mhdvfs_accumulate_freq(dsu_domain, freq);

	trace_mhdvfs_transition_notifier(dsu_domain->id, freq);

	return NOTIFY_OK;
}

static struct notifier_block mhdvfs_dsufreq_nb = {
	.notifier_call = mhdvfs_dsufreq_notifier,
};

static int mhdvfs_init_dsu(struct device *dev)
{
	int i, table_size;
	unsigned long *table;

	dsu_domain->id = DOMAIN_DSU;
	dsu_domain->freq_avg_pos = 0;
	dsu_domain->policy = POLICY_PRESS |
			     POLICY_MULTI_LEVEL |
			     POLICY_SOFT_LANDING;

	spin_lock_init(&dsu_domain->freq_avg_lock);

	/* initialize frequency table information */
	table = dsufreq_get_freq_table(&table_size);
	dsu_domain->freq_table = kcalloc(table_size,
					sizeof(unsigned int), GFP_KERNEL);
	if (unlikely(!dsu_domain->freq_table))
		return -ENOMEM;

	i = 0;
	do {
		dsu_domain->freq_table[i] = table[i];
	} while (++i < table_size);

	dsu_domain->freq_table_size = table_size;

	/* initialize pm qos request */
	dsu_domain->pm_qos_req = kzalloc(sizeof(struct dev_pm_qos_request),
								GFP_KERNEL);
	if (unlikely(!dsu_domain->pm_qos_req))
		return -ENOMEM;

	dsu_domain->default_qos = table[table_size - 1];
	dsufreq_qos_add_request("mhdvfs", dsu_domain->pm_qos_req,
			DEV_PM_QOS_MAX_FREQUENCY, dsu_domain->default_qos);

	/* register mif notifier */
	dsufreq_register_notifier(&mhdvfs_dsufreq_nb);

	return 0;
}

/**************************************************************
 * MIF frequency control functions:
 */

static int mhdvfs_devfreq_notifier(struct notifier_block *nb,
					unsigned long event, void *data)
{
	struct devfreq_freqs *freqs = (struct devfreq_freqs *)data;

	if (event != DEVFREQ_POSTCHANGE)
		return NOTIFY_OK;

	if (!mhdvfs_enabled())
		return NOTIFY_OK;

	if (mhdvfs_get_cur_freq(mif_domain) == freqs->new)
		return NOTIFY_OK;

	mhdvfs_accumulate_freq(mif_domain, freqs->new);

	trace_mhdvfs_transition_notifier(mif_domain->id, freqs->new);

	return NOTIFY_OK;
}

static struct notifier_block mhdvfs_devfreq_nb = {
	.notifier_call = mhdvfs_devfreq_notifier,
};

static int mhdvfs_init_mif(struct device *dev)
{
	struct devfreq *devfreq;
	struct exynos_devfreq_data *data;
	int i;

	devfreq = devfreq_get_devfreq_by_phandle(dev, "devfreq-mif", 0);
	if (IS_ERR(devfreq)) {
		pr_warn("Failed to get devfreq data with err=%ld, deferred!\n",
				PTR_ERR(devfreq));
		return PTR_ERR(devfreq);
	}

	mif_domain->id = DOMAIN_MIF;
	mif_domain->freq_avg_pos = 0;
	mif_domain->policy = 0;

	spin_lock_init(&mif_domain->freq_avg_lock);

	/* initialize frequency table information */
	mif_domain->freq_table = kcalloc(devfreq->max_state,
					sizeof(unsigned int), GFP_KERNEL);
	if (unlikely(!mif_domain->freq_table))
		return -ENOMEM;

	i = 0;
	do {
		mif_domain->freq_table[i] =
			devfreq->freq_table[devfreq->max_state - (i + 1)];
	} while (++i < devfreq->max_state);

	mif_domain->freq_table_size = devfreq->max_state;

	/* initialize pm qos request */
	mif_domain->pm_qos_req = kzalloc(sizeof(struct exynos_pm_qos_request),
								GFP_KERNEL);
	if (unlikely(!mif_domain->pm_qos_req))
		return -ENOMEM;

	mif_domain->default_qos = mif_domain->freq_table[0];
	data = dev_get_drvdata(devfreq->dev.parent);
	exynos_pm_qos_add_request(mif_domain->pm_qos_req, data->pm_qos_class,
				mif_domain->default_qos);

	/* register mif notifier */
	devm_devfreq_register_notifier(devfreq->dev.parent, devfreq,
				&mhdvfs_devfreq_nb, DEVFREQ_TRANSITION_NOTIFIER);

	return 0;
}

/**************************************************************
 * Exception scenario handling:
 */

#if IS_ENABLED(CONFIG_SCHED_EMS)
static int mhdvfs_emstune_notifier(struct notifier_block *nb,
				unsigned long val, void *v)
{
	int mode, level;

	/* FIXME */
	mode = *(int *)v;
	level = *((int *)(v) + 1);

	spin_lock(&mhdvfs_data.lock);
	if (mode == EMSTUNE_GAME_MODE && level != EMSTUNE_BOOST_LEVEL)
		mhdvfs_data.active = true;
	else {
		if (mhdvfs_data.active) {
			mhdvfs_data.active = false;
			mhdvfs_release();
		}
	}
	spin_unlock(&mhdvfs_data.lock);

	return NOTIFY_OK;
}

static struct notifier_block mhdvfs_emstune_nb = {
	.notifier_call = mhdvfs_emstune_notifier,
};
#endif

/**************************************************************
 * sysfs handler and creation:
 */

static ssize_t amu_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct uarch_info *uinfo;
	int cpu;
	int ret = 0;

	ret += snprintf(buf + ret,
			PAGE_SIZE - ret, "cpu       core_cycle       const_cycle"
					"     inst_ret_cnt    mem_stall_cnt\n");

	for_each_possible_cpu(cpu) {
		uinfo = per_cpu_ptr(pcpu_uinfo, cpu);
		ret += snprintf(buf + ret,
				PAGE_SIZE - ret, "%2d  %16llu %16llu %16llu %16llu\n",
				cpu, uinfo->core_cycles, uinfo->const_cycles,
				uinfo->inst_ret, uinfo->mem_stall);
	}

	return ret;
}

static ssize_t enabled_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int dsu, mif, cpu;

	if (sscanf(buf, "%d %d %d", &dsu, &mif, &cpu) != 3)
		return -EINVAL;

	mhdvfs_release();

	dsu_domain->enabled = dsu;
	mif_domain->enabled = mif;
	cpu_domain->enabled = cpu;

	return count;
}

static ssize_t enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"%d %d %d (DSU MIF CPU)\n",
				dsu_domain->enabled,
				mif_domain->enabled,
				cpu_domain->enabled);
}

static ssize_t policy_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int dsu, mif, cpu;

	if (sscanf(buf, "%d %d %d", &dsu, &mif, &cpu) != 3)
		return -EINVAL;

	dsu_domain->policy = dsu;
	mif_domain->policy = mif;
	cpu_domain->policy = cpu;

	return count;
}

static ssize_t policy_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;

	ret += snprintf(buf, PAGE_SIZE,
			"%#x %#x %#x (DSU MIF CPU)\n",
				dsu_domain->policy,
				mif_domain->policy,
				cpu_domain->policy);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"\n[Description]\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"[0] POLICY_PRESS\t(=0:raise,\t=1:press)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"[1] POLICY_MULTI_LEVEL\t(=0:single,\t=1:multi)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"[2] POLICY_SOFT_LANDING\t(=0:at once,\t=1:soft landing)\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"[3] POLICY_FREQ_MAX\t(=0:avg,\t=1:max)\n");

	return ret;
}

static ssize_t ipc_crit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE,
			"%d %d (DSU MIF)\n", dsu_domain->ipc_crit,
					     mif_domain->ipc_crit);
}

static ssize_t mpi_crit_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, i;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "  MIF    MPI\n");
	for (i = 0; i < mhdvfs_data.mpi_crit_table_size; i += 2)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"%7d %4d\n", mhdvfs_data.mpi_crit_table[i],
				   mhdvfs_data.mpi_crit_table[i + 1]);

	return ret;
}

static ssize_t stat_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0, i;

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "[MIF]\n");
	for (i = 0; i < MAX_LEVEL; i++) {
		if (i == 0)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
							"normal ");
		else
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
							"level%d ", i);

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%llu\t%llu\n",
						mif_domain->stat.usage[i],
						mif_domain->stat.time[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "[DSU]\n");
	for (i = 0; i < MAX_LEVEL; i++) {
		if (i == 0)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
							"normal ");
		else
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
							"level%d ", i);

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%llu\t%llu\n",
						dsu_domain->stat.usage[i],
						dsu_domain->stat.time[i]);
	}
	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "[CPU]\n");
	for (i = 0; i < MAX_LEVEL; i++) {
		if (i == 0)
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
							"normal ");
		else
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
							"level%d ", i);

		ret += snprintf(buf + ret, PAGE_SIZE - ret, "%llu\t%llu\n",
						cpu_domain->stat.usage[i],
						cpu_domain->stat.time[i]);
	}

	return ret;
}

DEVICE_ATTR_RO(amu_count);
DEVICE_ATTR_RW(enabled);
DEVICE_ATTR_RW(policy);
DEVICE_ATTR_RO(ipc_crit);
DEVICE_ATTR_RO(mpi_crit);
DEVICE_ATTR_RO(stat);

static void mhdvfs_sysfs_init(struct kobject *kobj)
{
	if (sysfs_create_file(kobj, &dev_attr_amu_count.attr))
		pr_warn("Failed to create amu_count node\n");
	if (sysfs_create_file(kobj, &dev_attr_enabled.attr))
		pr_warn("Failed to create enabled node\n");
	if (sysfs_create_file(kobj, &dev_attr_policy.attr))
		pr_warn("Failed to create policy node\n");
	if (sysfs_create_file(kobj, &dev_attr_ipc_crit.attr))
		pr_warn("Failed to create ipc_crit node\n");
	if (sysfs_create_file(kobj, &dev_attr_mpi_crit.attr))
		pr_warn("Failed to create mpi_crit node\n");
	if (sysfs_create_file(kobj, &dev_attr_stat.attr))
		pr_warn("Failed to create mpi_crit node\n");

	/* Link mhdvfs node to /sys/kernel/mhdvfs */
	if (sysfs_create_link(kernel_kobj, kobj, "mhdvfs"))
		pr_warn("Failed to symbolic link for mhdvfs node\n");
}

/**************************************************************
 * initialization of mhdvfs core:
 */

static void mhdvfs_stat_init(void)
{
	u64 now = sched_clock();

	mif_domain->stat.last_updated = now;
	dsu_domain->stat.last_updated = now;
	cpu_domain->stat.last_updated = now;
}

static void mhdvfs_param_init(struct device_node *dn)
{
	const char *buf;
	int size;
	u32 *table;

	mhdvfs_data.interval_jiffies = 5;	/* 5 jiffies = 20ms */

	if (!of_property_read_string(dn, "monitor-cpus", &buf))
		cpulist_parse(buf, &mhdvfs_data.monitor_cpus);
	else
		cpumask_copy(&mhdvfs_data.monitor_cpus, cpu_possible_mask);

	if (!of_property_read_string(dn, "super-wide-cpus", &buf))
		cpulist_parse(buf, &mhdvfs_data.super_wide_cpus);
	else
		cpumask_clear(&mhdvfs_data.super_wide_cpus);

	if (of_property_read_u32(dn, "ipc-crit-dsu", &dsu_domain->ipc_crit))
		dsu_domain->ipc_crit = 0;

	if (of_property_read_u32(dn, "ipc-crit-mif", &mif_domain->ipc_crit))
		mif_domain->ipc_crit = 0;

	if (of_property_read_u32(dn, "sw-ipc-crit-dsu", &dsu_domain->sw_ipc_crit))
		dsu_domain->sw_ipc_crit = UINT_MAX;

	if (of_property_read_u32(dn, "sw-ipc-crit-mif", &mif_domain->sw_ipc_crit))
		mif_domain->sw_ipc_crit = UINT_MAX;

	if (of_property_read_bool(dn, "dsu-control"))
		dsu_domain->enabled = true;

	if (of_property_read_bool(dn, "mif-control"))
		mif_domain->enabled = true;

	size = of_property_count_u32_elems(dn, "mpi-criterion-table");
	if (size < 0)
		return;

	table = kcalloc(size, sizeof(u32), GFP_KERNEL);
	if (!table)
		return;

	of_property_read_u32_array(dn, "mpi-criterion-table", table, size);

	mhdvfs_data.mpi_crit_table_size = size;
	mhdvfs_data.mpi_crit_table = table;
}

static int mhdvfs_probe(struct platform_device *pdev)
{
	if (mhdvfs_init_mif(&pdev->dev))
		return -EPROBE_DEFER;

	if (mhdvfs_init_dsu(&pdev->dev))
		return -EPROBE_DEFER;

	pcpu_uinfo = alloc_percpu(struct uarch_info);

	spin_lock_init(&mhdvfs_data.lock);
	INIT_WORK(&mhdvfs_data.work, mhdvfs_update_work);

	mhdvfs_param_init(pdev->dev.of_node);
	mhdvfs_stat_init();
	mhdvfs_sysfs_init(&pdev->dev.kobj);

	tlb_init();

#if IS_ENABLED(CONFIG_SCHED_EMS)
	emstune_register_notifier(&mhdvfs_emstune_nb);
#endif

	if (register_trace_android_vh_scheduler_tick(do_mhdvfs, NULL))
		pr_err("Failed to register vendor hook for scheduler tick\n");

	return 0;
}

static const struct of_device_id of_mhdvfs_match[] = {
	{ .compatible = "samsung,mhdvfs", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_mhdvfs_match);

static struct platform_driver mhdvfs_driver = {
	.driver = {
		.name = "mhdvfs",
		.owner = THIS_MODULE,
		.of_match_table = of_mhdvfs_match,
	},
	.probe		= mhdvfs_probe,
};

static int __init mhdvfs_driver_init(void)
{
	return platform_driver_register(&mhdvfs_driver);
}
arch_initcall(mhdvfs_driver_init);

static void __exit mhdvfs_driver_exit(void)
{
	platform_driver_unregister(&mhdvfs_driver);
}
module_exit(mhdvfs_driver_exit);

MODULE_DESCRIPTION("MHDVFS driver");
MODULE_LICENSE("GPL");
