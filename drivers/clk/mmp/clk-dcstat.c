/*
 *  linux/arch/arm/mach-mmp/dc_profiling.c
 *
 *  Author:	Liang Chen <chl@marvell.com>
 *		Xiangzhan Meng <mengxzh@marvell.com>
 *  Copyright:	(C) 2013 Marvell International Ltd.
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 *
 * (C) Copyright 2012 Marvell International Ltd.
 * All Rights Reserved
 */
#include <linux/clk-private.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/pm_domain.h>
#include <linux/cputype.h>
#include <linux/pm_domain.h>

#include <linux/clk/mmpdcstat.h>
#include "clk.h"
#include <linux/pm_qos.h>

struct clk *cpu_dcstat_clk;
static DEFINE_PER_CPU(struct clk_dc_stat_info, cpu_dc_stat);
static DEFINE_SPINLOCK(clk_dc_lock);
static DEFINE_SPINLOCK(clk_dc_stats_lock);

static struct idle_dcstat_info idle_dcstat_info;
static u32 multi_cluster;
static spinlock_t allidle_lock;
static spinlock_t c1c2_enter_lock;
static spinlock_t c1c2_exit_lock;

static LIST_HEAD(core_dcstat_list);
static LIST_HEAD(clk_dcstat_list);

static powermode pxa_powermode;
static struct clk *gcsh_clk;

/*10us,50us,100us,250us,500us,750us,1ms,5ms,25ms,100ms */
#define MAX_TIME 1000000000000000000
static u64 cpuidle_breakdown[MAX_BREAKDOWN_TIME+1] = {10000, 50000, 100000,
250000, 500000, 750000, 1000000, 5000000, 25000000, 100000000, MAX_TIME};

static void clk_dutycycle_stats(struct clk *clk,
				enum clk_stat_msg msg,
				struct clk_dc_stat_info *dc_stat_info,
				unsigned int tgtstate)
{
	struct timespec cur_ts, prev_ts;
	u64 time_ns;
	struct op_dcstat_info *cur, *tgt;
	bool pwr_is_on = true;
	struct generic_pm_domain *genpd = NULL;

	/* do nothing if no stat operation is issued */
	if (!dc_stat_info->stat_start)
		return;

	spin_lock(&clk_dc_stats_lock);

	cur = &dc_stat_info->ops_dcstat[dc_stat_info->curopindex];
	ktime_get_ts(&cur_ts);

	prev_ts = cur->prev_ts;
	time_ns = ts2ns(cur_ts, prev_ts);

	/* Get power domain info */
	if (!strcmp(clk->name, "gcsh_clk"))
		genpd = clk_to_genpd("gc3d_clk");
	else if (!strcmp(clk->name, "gc3dsh_clk"))
		genpd = clk_to_genpd("gc3d1x_clk");
	else
		genpd = clk_to_genpd(clk->name);

	if (genpd) {
		if (genpd->status == GPD_STATE_POWER_OFF)
			pwr_is_on = false;
	}

	switch (msg) {
	case CLK_STAT_START:
		/* duty cycle stat start */
		cur->prev_ts = cur_ts;
		break;
	case CLK_STAT_STOP:
		/* duty cycle stat stop */
		if (pwr_is_on) {
			if (clk->enable_count)
				cur->clk_busy_time += time_ns;
			else
				cur->clk_idle_time += time_ns;
		} else {
			cur->pwr_off_time += time_ns;
		}
		break;
	case CLK_STATE_ON:
		/* clk switch from off->on */
		cur->prev_ts = cur_ts;
		if (pwr_is_on)
			cur->clk_idle_time += time_ns;
		else
			cur->pwr_off_time += time_ns;
		break;
	case CLK_STATE_OFF:
		/* clk switch from off->on */
		cur->prev_ts = cur_ts;
		if (pwr_is_on)
			cur->clk_busy_time += time_ns;
		else
			cur->pwr_off_time += time_ns;
		break;
	case CLK_RATE_CHANGE:
		/* rate change from old->new */
		cur->prev_ts = cur_ts;
		if (pwr_is_on) {
			if (clk->enable_count)
				cur->clk_busy_time += time_ns;
			else
				cur->clk_idle_time += time_ns;
		} else {
			cur->pwr_off_time += time_ns;
		}
		BUG_ON(tgtstate >= dc_stat_info->ops_stat_size);
		tgt = &dc_stat_info->ops_dcstat[tgtstate];
		tgt->prev_ts = cur_ts;
		break;
	case PWR_ON:
		/* power on */
		cur->prev_ts = cur_ts;
		cur->pwr_off_time += time_ns;
		break;
	case PWR_OFF:
		/* power off */
		cur->prev_ts = cur_ts;
		if (clk->enable_count)
			cur->clk_busy_time += time_ns;
		else
			cur->clk_idle_time += time_ns;
		break;
	default:
		break;
	}

	spin_unlock(&clk_dc_stats_lock);
}

static u32 clk_dcstat_rate2ppindex(struct clk *clk, unsigned long rate)
{
	struct clk_dcstat *cdcs;
	struct clk_dc_stat_info *dcstat;
	u32 opsize, idx;

	/* search the list of the registation for this clk */
	list_for_each_entry(cdcs, &clk_dcstat_list, node)
	    if (cdcs->clk == clk) {
			dcstat = &cdcs->clk_dcstat;
			opsize = dcstat->ops_stat_size;
			for (idx = 0; idx < opsize; idx++)
				if (dcstat->ops_dcstat[idx].pprate == rate)
					return dcstat->ops_dcstat[idx].ppindex;
		}
	pr_warn("Failed to find rate %lu for %s\n", rate, clk->name);
	return 0;
}

static int clk_dcstat_notifier_handler(struct notifier_block *nb,
		unsigned long msg, void *data)
{
	struct clk_notifier_data *cnd = data;
	struct clk *clk = cnd->clk;
	unsigned int idx;

	/* enable */
	if ((msg & PRE_RATE_CHANGE) && !cnd->old_rate && cnd->new_rate)
		clk_dcstat_event(clk, CLK_STATE_ON, 0);
	/* disable */
	else if ((msg & POST_RATE_CHANGE) && cnd->old_rate && !cnd->new_rate)
		clk_dcstat_event(clk, CLK_STATE_OFF, 0);
	/* rate change */
	else if ((msg & POST_RATE_CHANGE) && cnd->old_rate && cnd->new_rate) {
		idx = clk_dcstat_rate2ppindex(clk, cnd->new_rate);
		clk_dcstat_event(clk, CLK_RATE_CHANGE, idx);
	}

	pr_debug("clk %s, %lu->%lu\n", clk->name, cnd->old_rate, cnd->new_rate);
	return NOTIFY_OK;
}

int clk_register_dcstat(struct clk *clk,
			    unsigned long *opt, unsigned int opt_size)
{
	struct clk_dcstat *cdcs;
	struct clk_dc_stat_info *clk_dcstat;
	unsigned int i;
	unsigned long rate;

	/* search the list of the registation for this clk */
	list_for_each_entry(cdcs, &clk_dcstat_list, node)
	    if (cdcs->clk == clk)
			break;

	/* if clk wasn't in the list, allocate new dcstat info */
	if (cdcs->clk != clk) {
		cdcs = kzalloc(sizeof(struct clk_dcstat), GFP_KERNEL);
		if (!cdcs)
			goto out;

		rate = clk_get_rate(clk);
		cdcs->clk = clk;
		/* allocate and fill dc stat information */
		clk_dcstat = &cdcs->clk_dcstat;
		clk_dcstat->ops_dcstat = kzalloc(opt_size *
						     sizeof(struct
							    op_dcstat_info),
						     GFP_KERNEL);
		if (!clk_dcstat->ops_dcstat) {
			pr_err("%s clk %s memory allocate failed!\n",
			       __func__, clk->name);
			goto out1;
		}
		for (i = 0; i < opt_size; i++) {
			clk_dcstat->ops_dcstat[i].ppindex = i;
			clk_dcstat->ops_dcstat[i].pprate = opt[i];
			if (rate == opt[i])
				clk_dcstat->curopindex = i;
		}
		clk_dcstat->ops_stat_size = opt_size;
		clk_dcstat->stat_start = false;

		list_add(&cdcs->node, &clk_dcstat_list);

		/* for dc stat to get clk_enable/disable/freq-chg event */
		cdcs->nb.notifier_call = clk_dcstat_notifier_handler;
		clk_notifier_register(clk, &cdcs->nb);
	}

	if (!gcsh_clk)
		gcsh_clk = __clk_lookup("gcsh_clk");

	return 0;
out1:
	kfree(cdcs);
out:
	return -ENOMEM;
}
EXPORT_SYMBOL(clk_register_dcstat);

void clk_dcstat_event(struct clk *clk,
			  enum clk_stat_msg msg, unsigned int tgtstate)
{
	struct clk_dcstat *cdcs;
	struct clk_dc_stat_info *dcstat_info;

	list_for_each_entry(cdcs, &clk_dcstat_list, node)
	    if (cdcs->clk == clk) {
		dcstat_info = &cdcs->clk_dcstat;
		clk_dutycycle_stats(clk, msg, dcstat_info, tgtstate);
		/*
		 * always update curopindex, no matter stat
		 * is started or not
		 */
		if (msg == CLK_RATE_CHANGE)
			dcstat_info->curopindex = tgtstate;
		break;
	}
}
EXPORT_SYMBOL(clk_dcstat_event);

void clk_dcstat_event_check(struct clk *clk,
			  enum clk_stat_msg msg, unsigned int tgtstate)
{
	if (!clk)
		return;

	if (!strcmp(clk->name, "gc3d_clk")) {
		clk_dcstat_event(clk, msg, tgtstate);
		if (gcsh_clk)
			clk_dcstat_event(gcsh_clk, msg, tgtstate);
	} else
		clk_dcstat_event(clk, msg, tgtstate);
}
EXPORT_SYMBOL(clk_dcstat_event_check);

int show_dc_stat_info(struct clk *clk, struct seq_file *seq, void *data)
{
	unsigned int i, dc_int, dc_frac, gr_int, gr_frac;
	unsigned int clk_idle_int, clk_idle_frac, pwr_off_int, pwr_off_frac;
	u64 total_time = 0, run_total = 0, idle_total = 0, pwr_off_total = 0;
	struct clk_dcstat *cdcs;
	struct clk_dc_stat_info *dc_stat_info = NULL;
#ifdef CONFIG_DDR_DEVFREQ
	unsigned int ddr_glob_ratio = 0, ddr_idle_ratio = 0, ddr_busy_ratio = 0,
	ddr_data_ratio = 0, ddr_util_ratio = 0, ddr_pprate = 0;
	u64 ddr_data = 0, ddr_data_total = 0;
#endif

	list_for_each_entry(cdcs, &clk_dcstat_list, node)
	    if (cdcs->clk == clk) {
		dc_stat_info = &cdcs->clk_dcstat;
		break;
	}

	if (!dc_stat_info) {
		pr_err("clk %s NULL dc stat info\n", clk->name);
		return -EINVAL;
	}

	if (dc_stat_info->stat_start) {
		seq_printf(seq, "Please stop the %s duty cycle stat first\n",
				clk->name);
		return 0;
	}

	/* Also change time unit from ns to ms here */
	for (i = 0; i < dc_stat_info->ops_stat_size; i++) {
		run_total += dc_stat_info->ops_dcstat[i].clk_busy_time;
		do_div(dc_stat_info->ops_dcstat[i].clk_busy_time,
			NSEC_PER_MSEC);
		idle_total += dc_stat_info->ops_dcstat[i].clk_idle_time;
		do_div(dc_stat_info->ops_dcstat[i].clk_idle_time,
			NSEC_PER_MSEC);
		pwr_off_total += dc_stat_info->ops_dcstat[i].pwr_off_time;
		do_div(dc_stat_info->ops_dcstat[i].pwr_off_time,
			NSEC_PER_MSEC);
	}

	do_div(run_total, NSEC_PER_MSEC);
	do_div(idle_total, NSEC_PER_MSEC);
	do_div(pwr_off_total, NSEC_PER_MSEC);
	total_time = run_total + idle_total + pwr_off_total;
	if (!total_time) {
		seq_puts(seq, "No stat information! ");
		seq_puts(seq, "Help information :\n");
		seq_puts(seq, "1. echo 1 to start duty cycle stat.\n");
		seq_puts(seq, "2. echo 0 to stop duty cycle stat.\n");
		seq_puts(seq, "3. cat to check duty cycle info during stat.\n\n");
		return 0;
	}

	dc_int = calculate_dc((u32)run_total, (u32)total_time, &dc_frac);
	seq_printf(seq, "\n%s clk: Totally %u.%2u%% busy in %lums.\n",
			clk->name, dc_int, dc_frac, (long)total_time);

	if (!strcmp(clk->name, "ddr")) {
#ifdef CONFIG_DDR_DEVFREQ
		ddr_profiling_show(dc_stat_info);

		seq_printf(seq, "|%3s|%10s|%10s|%7s|%7s|%7s|%7s|%7s|%7s|%7s|\n",
		"OP#", "rate(HZ)", "run (ms)", "rt", "gl",
		"glob(tick)", "idle(tick)", "busy(tick)",
		"data(tick)", "util(tick)");
#endif
	} else
		seq_printf(seq, "|%3s|%10s|%10s|%10s|%10s|%7s|%7s|%7s|%7s|\n",
		"OP#", "rate(HZ)", "run (ms)", "idle (ms)", "pwroff(ms)",
			"rt", "idle", "pwr off", "gl");

	for (i = 0; i < dc_stat_info->ops_stat_size; i++) {
		dc_int =
		calculate_dc((u32)dc_stat_info->ops_dcstat[i].clk_busy_time,
				     (u32)total_time, &dc_frac);
		clk_idle_int = calculate_dc((u32)dc_stat_info->ops_dcstat[i].clk_idle_time,
					    (u32)total_time, &clk_idle_frac);
		pwr_off_int = calculate_dc((u32)dc_stat_info->ops_dcstat[i].pwr_off_time,
					   (u32)total_time, &pwr_off_frac);
		gr_int =
		calculate_dc((u32)dc_stat_info->ops_dcstat[i].clk_busy_time +
				(u32)dc_stat_info->ops_dcstat[i].clk_idle_time +
				(u32)dc_stat_info->ops_dcstat[i].pwr_off_time,
				(u32)total_time, &gr_frac);

#ifdef CONFIG_DDR_DEVFREQ
		if (!strcmp(clk->name, "ddr")) {
			ddr_glob_ratio =
				dc_stat_info->ops_dcstat[i].ddr_glob_ratio;
			ddr_idle_ratio =
				dc_stat_info->ops_dcstat[i].ddr_idle_ratio;
			ddr_busy_ratio =
				dc_stat_info->ops_dcstat[i].ddr_busy_ratio;
			ddr_data_ratio =
				dc_stat_info->ops_dcstat[i].ddr_data_ratio;
			ddr_util_ratio =
				dc_stat_info->ops_dcstat[i].ddr_util_ratio;
		}
#endif

		if (!strcmp(clk->name, "ddr")) {
#ifdef CONFIG_DDR_DEVFREQ
			/* eden ddr pprate is 312000, helanlte ddr pprate is 312000000,
			 * the ddr clock format is different.
			 */
			if (dc_stat_info->ops_dcstat[i].pprate >= 1000000)
				ddr_pprate = dc_stat_info->ops_dcstat[i].pprate/1000000;
			else if (dc_stat_info->ops_dcstat[i].pprate >= 1000)
				ddr_pprate = dc_stat_info->ops_dcstat[i].pprate/1000;

			ddr_data = (ddr_pprate * 8 * dc_int)/100;
			ddr_data *= ddr_data_ratio;
			ddr_data_total += ddr_data;

			ddr_data = (ddr_pprate * 8 * dc_frac)/10000;
			ddr_data *= ddr_data_ratio;
			ddr_data_total += ddr_data;

			seq_printf(seq, "|%3u|%10lu|%10ld|",
			dc_stat_info->ops_dcstat[i].ppindex,
			dc_stat_info->ops_dcstat[i].pprate,
			(long)dc_stat_info->ops_dcstat[i].clk_busy_time);
			seq_printf(seq, "%3u.%2u%%|%3u.%2u%%|",
				dc_int, dc_frac, gr_int, gr_frac);
			seq_printf(seq, "%6u.%2u%%|%6u.%2u%%|%6u.%2u%%|%6u.%2u%%|%6u.%2u%%|\n",
				ddr_glob_ratio/1000, (ddr_glob_ratio%1000)/10,
				ddr_idle_ratio/1000, (ddr_idle_ratio%1000)/10,
				ddr_busy_ratio/1000, (ddr_busy_ratio%1000)/10,
				ddr_data_ratio/1000, (ddr_data_ratio%1000)/10,
				ddr_util_ratio/1000, (ddr_util_ratio%1000)/10);
			if (i == dc_stat_info->ops_stat_size - 1) {
				seq_printf(seq, "Totally ddr data access bandwidth is %lld MB/s\n",
				div64_u64(ddr_data_total, (u64)100000));
			}
#endif
		} else {
			seq_printf(seq, "|%3u|%10lu|%10ld|%10ld|%10ld",
				dc_stat_info->ops_dcstat[i].ppindex,
				dc_stat_info->ops_dcstat[i].pprate,
				(long)dc_stat_info->ops_dcstat[i].clk_busy_time,
				(long)dc_stat_info->ops_dcstat[i].clk_idle_time,
				(long)dc_stat_info->ops_dcstat[i].pwr_off_time);

			seq_printf(seq, "|%3u.%2u%%|%3u.%2u%%|%3u.%2u%%|%3u.%2u%%|\n",
				dc_int, dc_frac, clk_idle_int, clk_idle_frac,
				pwr_off_int, pwr_off_frac, gr_int, gr_frac);
		}
	}
	seq_puts(seq, "\n");
	return 0;
}
EXPORT_SYMBOL(show_dc_stat_info);

int start_stop_dc_stat(struct clk *clk, unsigned int start)
{
	unsigned int i;
	struct clk_dcstat *cdcs;
	struct clk_dc_stat_info *dc_stat_info = NULL;

	list_for_each_entry(cdcs, &clk_dcstat_list, node)
	    if (cdcs->clk == clk) {
		dc_stat_info = &cdcs->clk_dcstat;
		break;
	}

	if (!dc_stat_info) {
		pr_err("clk %s NULL dc stat info\n", clk->name);
		return -EINVAL;
	}

	start = !!start;
	if (start == dc_stat_info->stat_start) {
		pr_err("[WARNING]%s stat is already %s\n",
		       clk->name,
		       dc_stat_info->stat_start ? "started" : "stopped");
		return -EINVAL;
	}

	if (start) {
		/* clear old stat information */
		for (i = 0; i < dc_stat_info->ops_stat_size; i++) {
			dc_stat_info->ops_dcstat[i].clk_idle_time = 0;
			dc_stat_info->ops_dcstat[i].clk_busy_time = 0;
			dc_stat_info->ops_dcstat[i].pwr_off_time = 0;
		}
		dc_stat_info->stat_start = true;
		clk_dutycycle_stats(clk, CLK_STAT_START, dc_stat_info, 0);

#ifdef CONFIG_DDR_DEVFREQ
		if (!strcmp(clk->name, "ddr"))
			ddr_profiling_store(1);
#endif

	} else {
		clk_dutycycle_stats(clk, CLK_STAT_STOP, dc_stat_info, 0);
		dc_stat_info->stat_start = false;
#ifdef CONFIG_DDR_DEVFREQ
		if (!strcmp(clk->name, "ddr"))
			ddr_profiling_store(0);
#endif
	}
	return 0;
}
EXPORT_SYMBOL(start_stop_dc_stat);

static int cpu_id;
int register_cpu_dcstat(struct clk *clk, unsigned int cpunum,
	unsigned int *op_table, unsigned int opt_size, powermode func, unsigned int cluster)
{
	struct clk_dc_stat_info *cpu_dcstat;
	struct core_dcstat *op;
	unsigned int i, j, cpu, rate;
	static int cpu_dc_init;
	BUG_ON(!func);
	pxa_powermode = func;

	if (!cpu_dc_init) {
		spin_lock_init(&allidle_lock);
		spin_lock_init(&c1c2_enter_lock);
		spin_lock_init(&c1c2_exit_lock);
		cpu_dc_init++;
	}

	list_for_each_entry(op, &core_dcstat_list, node) {
		if (op->clk == clk)
			return 0;
	}

	op = kzalloc(sizeof(struct core_dcstat), GFP_KERNEL);
	if (!op) {
		pr_err("[WARNING]CPU stat info malloc failed\n");
		return -ENOMEM;
	}
	op->clk = clk;

	list_add(&op->node, &core_dcstat_list);
	op->cpu_id = kzalloc(sizeof(int) * cpunum, GFP_KERNEL);
	if (!op->cpu_id) {
		pr_err("[WARNING]CPU stat cpuid info malloc failed\n");
		goto err1;
	}

	for (i = 0; i < cpunum; i++)
		op->cpu_id[i] = cpu_id++;

	op->cpu_num = i;
	rate = clk_get_rate(clk);
	rate /= MHZ;
	/* get cur core rate */
	for (j = 0; j < op->cpu_num; j++) {
		cpu = op->cpu_id[j];
		cpu_dcstat = &per_cpu(cpu_dc_stat, cpu);
		cpu_dcstat->ops_dcstat = kzalloc(opt_size *
						 sizeof(struct
							op_dcstat_info),
						 GFP_KERNEL);
		if (!cpu_dcstat->ops_dcstat) {
			pr_err("%s: memory allocate failed!\n", __func__);
			goto err2;
		}
		for (i = 0; i < opt_size; i++) {
			cpu_dcstat->ops_dcstat[i].ppindex = i;
			/*pprate should be MHZ*/
			cpu_dcstat->ops_dcstat[i].pprate = op_table[i];
			if (cpu_dcstat->ops_dcstat[i].pprate == rate)
				cpu_dcstat->curopindex = i;
		}
		cpu_dcstat->ops_stat_size = i;
		cpu_dcstat->stat_start = false;
	}

	multi_cluster = cluster;

	return 0;
err2:
	kfree(op->cpu_id);
err1:
	kfree(op);
	return -ENOMEM;
}
EXPORT_SYMBOL(register_cpu_dcstat);

void cpu_dcstat_event(struct clk *clk, unsigned int cpuid,
			  enum clk_stat_msg msg, unsigned int tgtop)
{
	struct clk_dc_stat_info *dc_stat_info = NULL;
	struct op_dcstat_info *cur;
	unsigned int cpu_i;
	bool mark_keytime;
	u64 ktime_temp, temp_time = 0;
	u32 i, j, lpm_min, cpuidle_qos, cluster_flag = 0, cluster_flag1 = 0,
	mp2_flag = 0, mp2_flag1 = 0, active_flag = 0;
	u32 cluster0_online = 0, cluster1_online = 0;
	u32 cpuidle_qos_min = PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE;
	struct core_dcstat *op;

	dc_stat_info = &per_cpu(cpu_dc_stat, cpuid);
	if (!dc_stat_info->stat_start)
		return;

	list_for_each_entry(op, &core_dcstat_list, node) {
		if (op->clk == clk)
			break;
	}

	cur = &dc_stat_info->ops_dcstat[dc_stat_info->curopindex];
	if (msg == CLK_RATE_CHANGE) {
		/* BUG_ON(tgtop >= dc_stat_info->ops_stat_size); */
		dc_stat_info->curopindex = tgtop;
	}

	if (multi_cluster) {
		struct pm_qos_object *pm_qos_cpuidle;
		pm_qos_cpuidle = pm_qos_array[PM_QOS_CPUIDLE_BLOCK];
		cpuidle_qos_min = pm_qos_read_value(pm_qos_cpuidle->constraints);
		/* cluster_flag1 is used for other cluster */
		if (cpuid < 4) {
			cluster_flag = 1;
			cluster_flag1 = 0;
		} else if (cpuid >= 4 && cpuid < 8) {
			cluster_flag = 0;
			cluster_flag1 = 1;
		}
	}

	switch (msg) {
	case CLK_STAT_START:
		if ((0 == cpuid || 4 == cpuid) && (idle_dcstat_info.init_flag)) {
			memset(&idle_dcstat_info, 0,
			       sizeof(idle_dcstat_info));
			idle_dcstat_info.init_flag = 0;
			ktime_temp = ktime_to_ns(ktime_get());
			idle_dcstat_info.all_idle_start = ktime_temp;
			idle_dcstat_info.all_idle_end = ktime_temp;
			idle_dcstat_info.all_active_start = ktime_temp;
			idle_dcstat_info.all_active_end = ktime_temp;
			idle_dcstat_info.cal_duration = ktime_temp;
			idle_dcstat_info.M2_cluster0_idle_start = 0;
			idle_dcstat_info.M2_cluster1_idle_start = 0;
			idle_dcstat_info.D1P_idle_start = 0;
			idle_dcstat_info.D1_idle_start = 0;
			idle_dcstat_info.D2_idle_start = 0;
			for_each_possible_cpu(cpu_i) {
				dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
				dc_stat_info->power_mode = pxa_powermode(cpu_i);
				dc_stat_info->breakdown_start = 0;

				dc_stat_info->runtime_start = 0;
				for (i = 0; i < MAX_LPM_INDEX_DC; i++)
					dc_stat_info->runtime_op_total[i] = 0;

				if (dc_stat_info->power_mode == MAX_LPM_INDEX
					&& cpu_online(cpu_i)) {
					dc_stat_info->runtime_start = ktime_temp;
					dc_stat_info->runtime_op_index = dc_stat_info->curopindex;
				}

				for (i = 0; i <= MAX_BREAKDOWN_TIME; i++) {
					dc_stat_info->breakdown_time_total[i] = 0;
					dc_stat_info->breakdown_time_count[i] = 0;
				}
				dc_stat_info->C1_idle_start = 0;
				dc_stat_info->C2_idle_start = 0;
				for (i = 0; i < MAX_LPM_INDEX_DC; i++) {
					dc_stat_info->C1_op_total[i] = 0;
					dc_stat_info->C1_count[i] = 0;
					dc_stat_info->C2_op_total[i] = 0;
					dc_stat_info->C2_count[i] = 0;
				}
			}
			for (i = 0; i < MAX_LPM_INDEX_DC; i++) {
				idle_dcstat_info.all_idle_op_total[i] = 0;
				idle_dcstat_info.all_idle_count[i] = 0;
			}
		}
		break;
	case CLK_STAT_STOP:
		if (0 == cpuid) {
			ktime_temp = ktime_to_ns(ktime_get());
			idle_dcstat_info.cal_duration =
			    ktime_temp
			    - idle_dcstat_info.cal_duration;
		}
		break;
	case CLK_RATE_CHANGE:
		ktime_temp = ktime_to_ns(ktime_get());

		for (j = 0; j < op->cpu_num; j++) {
			cpu_i = op->cpu_id[j];
			if (cpuid == cpu_i)
				continue;
			dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);

			spin_lock(&c1c2_exit_lock);
			if ((u64) 0 != dc_stat_info->runtime_start) {
				dc_stat_info->runtime_end = ktime_temp;
				dc_stat_info->runtime_op_total
				    [dc_stat_info->runtime_op_index] +=
				    dc_stat_info->runtime_end -
					dc_stat_info->runtime_start;
				dc_stat_info->runtime_start = ktime_temp;
				dc_stat_info->runtime_op_index = tgtop;
			}
			if ((dc_stat_info->idle_flag == LPM_C1) &&
			    ((u64) 0 != dc_stat_info->C1_idle_start)) {
				dc_stat_info->C1_idle_end = ktime_temp;
				dc_stat_info->C1_op_total
				    [dc_stat_info->C1_op_index] +=
				    dc_stat_info->C1_idle_end -
					dc_stat_info->C1_idle_start;
				dc_stat_info->C1_count[dc_stat_info->C1_op_index]++;
				dc_stat_info->C1_idle_start = ktime_temp;
				dc_stat_info->C1_op_index = tgtop;
			} else if ((dc_stat_info->idle_flag == LPM_C2) &&
				   ((u64) 0 != dc_stat_info->C2_idle_start)) {
				dc_stat_info->C2_idle_end = ktime_temp;
				dc_stat_info->C2_op_total
				    [dc_stat_info->C2_op_index] +=
						dc_stat_info->C2_idle_end -
						 dc_stat_info->C2_idle_start;
				dc_stat_info->C2_count[dc_stat_info->C2_op_index]++;
				dc_stat_info->C2_idle_start = ktime_temp;
				dc_stat_info->C2_op_index = tgtop;
			}
			if ((u64) 0 != dc_stat_info->breakdown_start) {
				dc_stat_info->breakdown_end = ktime_temp;
				temp_time = dc_stat_info->breakdown_end -
							 dc_stat_info->breakdown_start;
				for (i = 0; i <= MAX_BREAKDOWN_TIME; i++)
					if (temp_time && temp_time < cpuidle_breakdown[i]) {
						dc_stat_info->breakdown_time_total[i]
							+= temp_time;
						dc_stat_info->breakdown_time_count[i]++;
						break;
					}
				dc_stat_info->breakdown_start = ktime_temp;
			}
			spin_unlock(&c1c2_exit_lock);
		}
		break;
	case CPU_IDLE_ENTER:
		ktime_temp = ktime_to_ns(ktime_get());
		spin_lock(&c1c2_enter_lock);
		if ((u64) 0 != dc_stat_info->runtime_start) {
			dc_stat_info->runtime_end = ktime_temp;
			dc_stat_info->runtime_op_index = dc_stat_info->curopindex;
			dc_stat_info->runtime_op_total[dc_stat_info->runtime_op_index] +=
				dc_stat_info->runtime_end - dc_stat_info->runtime_start;
			dc_stat_info->runtime_start = 0;
		}
		if (LPM_C1 == tgtop) {
			dc_stat_info->C1_op_index = dc_stat_info->curopindex;
			dc_stat_info->C1_idle_start = ktime_temp;
			dc_stat_info->idle_flag = LPM_C1;
		} else if (tgtop >= LPM_C2 && tgtop <= LPM_D2_UDR) {
			dc_stat_info->C2_op_index = dc_stat_info->curopindex;
			dc_stat_info->C2_idle_start = ktime_temp;
			dc_stat_info->idle_flag = LPM_C2;
		}
		if ((tgtop >= LPM_C1) && (tgtop <= LPM_D2_UDR))
			dc_stat_info->breakdown_start = ktime_temp;
		spin_unlock(&c1c2_enter_lock);
		dc_stat_info->power_mode = tgtop;

		/*  this mark_keytime is flag indicate enter all idle mode,
		 *  if two core both enter the idle,and power mode isn't
		 *  eaqual to MAX_LPM_INDEX, mean the other core don't
		 *  exit idle.
		 */
		mark_keytime = true;
		for_each_possible_cpu(cpu_i) {
			if (cpuid == cpu_i)
				continue;
			dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
			if (MAX_LPM_INDEX == dc_stat_info->power_mode) {
				mark_keytime = false;
				break;
			}
		}

		if (mark_keytime) {
			idle_dcstat_info.all_idle_start = ktime_temp;
			idle_dcstat_info.all_idle_op_index =
			    dc_stat_info->curopindex;
		}

		/*  this mark_keytime is flag indicate enter all active
		 *  mode,if two core both exit the idle,and power mode
		 *  is both eaqual to MAX_LPM_INDEX, mean the other
		 *  core both exit idle.
		 */

		mark_keytime = true;
		for_each_possible_cpu(cpu_i) {
			if (cpuid == cpu_i)
				continue;
			dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
			if (MAX_LPM_INDEX != dc_stat_info->power_mode) {
				mark_keytime = false;
				break;
			}
		}
		if (mark_keytime) {
			idle_dcstat_info.all_active_end = ktime_temp;
			idle_dcstat_info.total_all_active +=
			    idle_dcstat_info.all_active_end -
				       idle_dcstat_info.all_active_start;
			idle_dcstat_info.total_all_active_count++;
		}
		break;
	case CPU_IDLE_EXIT:
		ktime_temp = ktime_to_ns(ktime_get());
		spin_lock(&c1c2_exit_lock);

		dc_stat_info->runtime_op_index = dc_stat_info->curopindex;
		dc_stat_info->runtime_start = ktime_temp;

		if ((dc_stat_info->idle_flag == LPM_C1) &&
		    ((u64) 0 != dc_stat_info->C1_idle_start)) {
			dc_stat_info->C1_idle_end = ktime_temp;
			dc_stat_info->C1_op_total[dc_stat_info->C1_op_index] +=
			dc_stat_info->C1_idle_end - dc_stat_info->C1_idle_start;
			dc_stat_info->C1_count[dc_stat_info->C1_op_index]++;
			dc_stat_info->C1_idle_start = 0;
		} else if ((dc_stat_info->idle_flag == LPM_C2) &&
			   ((u64) 0 !=
			    dc_stat_info->C2_idle_start)) {
			dc_stat_info->C2_idle_end = ktime_temp;
			dc_stat_info->C2_op_total[dc_stat_info->C2_op_index] +=
			    dc_stat_info->C2_idle_end -
						  dc_stat_info->C2_idle_start;
			dc_stat_info->C2_count[dc_stat_info->C2_op_index]++;
			dc_stat_info->C2_idle_start = 0;
		}
		spin_unlock(&c1c2_exit_lock);
		dc_stat_info->idle_flag = MAX_LPM_INDEX;
		if ((u64) 0 != dc_stat_info->breakdown_start) {
			dc_stat_info->breakdown_end = ktime_temp;
			temp_time = dc_stat_info->breakdown_end -
						 dc_stat_info->breakdown_start;
			dc_stat_info->breakdown_start = 0;
			if (temp_time) {
				for (i = 0; i <= MAX_BREAKDOWN_TIME; i++)
					if (temp_time < cpuidle_breakdown[i]) {
						dc_stat_info->breakdown_time_total[i] += temp_time;
						dc_stat_info->breakdown_time_count[i]++;
						break;
					}
			}
		}
		dc_stat_info->power_mode = tgtop;

		if (multi_cluster) {
			mark_keytime = true;
			for (cpu_i = 0 + 4 * cluster_flag1; cpu_i < 4 + 4 * cluster_flag1; cpu_i++) {
				if (cpuid == cpu_i)
					continue;
				dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
				if (MAX_LPM_INDEX == dc_stat_info->power_mode) {
					mark_keytime = false;
					break;
				}
			}
			if (mark_keytime) {
				if ((cpuid < 4) && ((u64) 0 != idle_dcstat_info.M2_cluster0_idle_start)) {
					idle_dcstat_info.M2_cluster0_idle_total +=
					    ktime_temp - idle_dcstat_info.M2_cluster0_idle_start;
					idle_dcstat_info.M2_cluster0_count++;
					idle_dcstat_info.M2_cluster0_idle_start = 0;
				} else if ((cpuid >= 4 && cpuid < 8) &&
				((u64) 0 != idle_dcstat_info.M2_cluster1_idle_start)) {
					idle_dcstat_info.M2_cluster1_idle_total +=
					    ktime_temp - idle_dcstat_info.M2_cluster1_idle_start;
					idle_dcstat_info.M2_cluster1_count++;
					idle_dcstat_info.M2_cluster1_idle_start = 0;
				}
			}
		}

		mark_keytime = true;
		for_each_possible_cpu(cpu_i) {
			if (cpuid == cpu_i)
				continue;
			dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
			if (MAX_LPM_INDEX == dc_stat_info->power_mode) {
				mark_keytime = false;
				break;
			}
		}
		spin_lock(&allidle_lock);
		if (mark_keytime) {
			idle_dcstat_info.all_idle_end = ktime_temp;
			idle_dcstat_info.total_all_idle +=
			idle_dcstat_info.all_idle_end -
				       idle_dcstat_info.all_idle_start;
			idle_dcstat_info.total_all_idle_count++;

			if ((u64) 0 != idle_dcstat_info.all_idle_start) {
				idle_dcstat_info.all_idle_op_total
				    [idle_dcstat_info.all_idle_op_index] +=
						idle_dcstat_info.
						 all_idle_end -
						 idle_dcstat_info.
						 all_idle_start;
				idle_dcstat_info.
				    all_idle_count[idle_dcstat_info.
						   all_idle_op_index]++;
			}

			if (!multi_cluster) {
				if ((u64) 0 != idle_dcstat_info.M2_idle_start) {
					idle_dcstat_info.M2_idle_total +=
					    idle_dcstat_info.
							 all_idle_end -
							 idle_dcstat_info.
							 M2_idle_start;
					idle_dcstat_info.M2_count++;
				}
				idle_dcstat_info.M2_idle_start = 0;
			}
			if ((u64) 0 != idle_dcstat_info.D1P_idle_start) {
				idle_dcstat_info.D1P_idle_total +=
						idle_dcstat_info.
						 all_idle_end -
						 idle_dcstat_info.
						 D1P_idle_start;
				idle_dcstat_info.D1p_count++;
			} else if ((u64) 0 != idle_dcstat_info.D1_idle_start) {
				idle_dcstat_info.D1_idle_total +=
						idle_dcstat_info.
						 all_idle_end -
						 idle_dcstat_info.
						 D1_idle_start;
				idle_dcstat_info.D1_count++;
			} else if ((u64) 0 != idle_dcstat_info.D2_idle_start) {
				idle_dcstat_info.D2_idle_total +=
						idle_dcstat_info.
						 all_idle_end -
						 idle_dcstat_info.
						 D2_idle_start;
				idle_dcstat_info.D2_count++;
			}

			idle_dcstat_info.D1P_idle_start = 0;
			idle_dcstat_info.D1_idle_start = 0;
			idle_dcstat_info.D2_idle_start = 0;

			if (multi_cluster) {
				active_flag = 0;
				lpm_min = MAX_LPM_INDEX;
				for (cpu_i = 0 + 4 * cluster_flag; cpu_i < 4 + 4 * cluster_flag;
					cpu_i++) {
					dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
					if (MAX_LPM_INDEX != dc_stat_info->power_mode) {
						if (lpm_min > dc_stat_info->power_mode)
							lpm_min = dc_stat_info->power_mode;
						} else {
							active_flag = 1;
							break;
						}
				}
				if (active_flag == 0 && lpm_min >= LPM_MP2) {
					if ((cluster_flag1 == 1) &&
					((u64) 0 == idle_dcstat_info.M2_cluster0_idle_start))
						idle_dcstat_info.M2_cluster0_idle_start
							= ktime_temp;
					if ((cluster_flag1 == 0) &&
					((u64) 0 == idle_dcstat_info.M2_cluster1_idle_start))
						idle_dcstat_info.M2_cluster1_idle_start
							= ktime_temp;
				}
			}
		}
		spin_unlock(&allidle_lock);
		mark_keytime = true;
		for_each_possible_cpu(cpu_i) {
			if (cpuid == cpu_i)
				continue;
			dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
			if (MAX_LPM_INDEX != dc_stat_info->power_mode) {
				mark_keytime = false;
				break;
			}
		}
		if (mark_keytime)
			idle_dcstat_info.all_active_start = ktime_temp;
		break;
	case CPU_M2_OR_DEEPER_ENTER:
		ktime_temp = ktime_to_ns(ktime_get());

		if (multi_cluster) {
			for_each_possible_cpu(cpu_i)
				if (!cpu_online(cpu_i)) {
					dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
					dc_stat_info->power_mode = LPM_D2;
				}
			for (cpu_i = 0; cpu_i < 4; cpu_i++)
				if (cpu_online(cpu_i)) {
					cluster0_online = 1;
					break;
				}
			for (cpu_i = 4; cpu_i < 8; cpu_i++)
				if (cpu_online(cpu_i)) {
					cluster1_online = 1;
					break;
				}

			dc_stat_info = &per_cpu(cpu_dc_stat, cpuid);
			dc_stat_info->power_mode = tgtop;
			lpm_min = MAX_LPM_INDEX;
			active_flag = 0;
			for_each_online_cpu(cpu_i) {
				dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
				if (MAX_LPM_INDEX != dc_stat_info->power_mode) {
					if (lpm_min > dc_stat_info->power_mode)
						lpm_min = dc_stat_info->power_mode;
				} else {
					active_flag = 1;
					break;
				}
			}

			cpuidle_qos = MAX_LPM_INDEX;
			if (cpuidle_qos_min != PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE) {
				if (cpuidle_qos_min == LPM_D2_UDR)
					cpuidle_qos = LPM_D1;
				else if (cpuidle_qos_min == LPM_D1)
					cpuidle_qos = LPM_D1P;
				else if (cpuidle_qos_min == LPM_D1P)
					cpuidle_qos = LPM_MP2;
			}

			if (cpuidle_qos < lpm_min)
				lpm_min = cpuidle_qos;

			if (active_flag == 0) {
				dc_stat_info = &per_cpu(cpu_dc_stat, cpuid);
				if (LPM_D1P == lpm_min)
					idle_dcstat_info.D1P_idle_start = ktime_temp;
				else if (LPM_D1 == lpm_min)
					idle_dcstat_info.D1_idle_start = ktime_temp;
				else if (LPM_D2 == lpm_min)
					idle_dcstat_info.D2_idle_start = ktime_temp;

#ifdef CONFIG_VOLDC_STAT
				if (LPM_D1P == lpm_min)
					vol_dcstat_event(VLSTAT_LPM_ENTRY, 3, 0);
				else if (LPM_D1 == lpm_min)
					vol_dcstat_event(VLSTAT_LPM_ENTRY, 4, 0);
				else if (LPM_D2 == lpm_min)
					vol_dcstat_event(VLSTAT_LPM_ENTRY, 5, 0);
#endif
			}
			if (cpuidle_qos >= LPM_MP2) {
				for (cpu_i = 4 * cluster_flag; cpu_i < 4 + 4 * (cluster_flag); cpu_i++) {
					dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
					if (cpu_online(cpu_i)) {
						if (MAX_LPM_INDEX == dc_stat_info->power_mode ||
							(LPM_C1 <= dc_stat_info->power_mode &&
							LPM_C2 >= dc_stat_info->power_mode))
							mp2_flag = 1;
							break;
					}
				}

				if ((cluster_flag1 == 1) && (cluster1_online == 0))
					mp2_flag = 1;
				else if ((cluster_flag == 1) && (cluster0_online == 0))
					mp2_flag = 1;

				if (mp2_flag == 1 && lpm_min >= LPM_MP2)
					mp2_flag1 = 1;

				lpm_min = MAX_LPM_INDEX;
				for (cpu_i = 4 * cluster_flag1; cpu_i < 4 + 4 * cluster_flag1; cpu_i++) {
					if (cpu_online(cpu_i)) {
						dc_stat_info = &per_cpu(cpu_dc_stat, cpu_i);
						if (MAX_LPM_INDEX != dc_stat_info->power_mode) {
							if (lpm_min > dc_stat_info->power_mode)
								lpm_min = dc_stat_info->power_mode;
						} else
							return;
					}
				}
				if (cpuidle_qos  < lpm_min)
					lpm_min = cpuidle_qos;

				if (mp2_flag1 == 1 || lpm_min >= LPM_MP2) {
					if (((cluster_flag == 0) || (!cluster1_online)) &&
					((u64) 0 == idle_dcstat_info.M2_cluster1_idle_start))
						idle_dcstat_info.M2_cluster1_idle_start
							= ktime_temp;

					if (((cluster_flag == 1) || (!cluster0_online)) &&
					((u64) 0 == idle_dcstat_info.M2_cluster0_idle_start))
						idle_dcstat_info.M2_cluster0_idle_start
							= ktime_temp;

#ifdef CONFIG_VOLDC_STAT
					/* When helan3 2 cluster enter M2 mode, we will start
						vlstat M2 mode */
					if (((u64) 0 != idle_dcstat_info.M2_cluster0_idle_start
					|| !cluster0_online) &&
					((u64) 0 != idle_dcstat_info.M2_cluster1_idle_start
					|| !cluster1_online))
						vol_dcstat_event(VLSTAT_LPM_ENTRY, 2, 0);
#endif
				}
			}
		} else {
			if (LPM_MP2 == tgtop)
				idle_dcstat_info.M2_idle_start = ktime_temp;
			else if (LPM_D1P == tgtop)
				idle_dcstat_info.D1P_idle_start = ktime_temp;
			else if (LPM_D1 == tgtop)
				idle_dcstat_info.D1_idle_start = ktime_temp;
			else if (LPM_D2 == tgtop)
				idle_dcstat_info.D2_idle_start = ktime_temp;
		}
		break;
	default:
		break;
	}
}

static int cpu_dc_show(struct seq_file *seq, void *data)
{
	unsigned int cpu, i, dc_int = 0, dc_fra = 0;
	struct clk_dc_stat_info *percpu_stat = NULL;
	u32 total_time = 0, run_total, idle_total, busy_time, rt_total = 0;
	u32 av_mips, av_mips_total = 0;
	u32 av_mips_l, av_mips_h, rt_h, rt_l, idle_h, idle_l, total_all_idle,
	run_time, idle_time;
	u32 temp_total_time = 0, temp_total_count = 0;
	char *lpm_time_string[12] = { "<10 us", "<50 us", "<100 us",
		"<250 us", "<500 us", "<750 us", "<1 ms", "<5 ms",
		"<25 ms", "<100 ms", ">100 ms"
	};

	percpu_stat = &per_cpu(cpu_dc_stat, 0);
	if (percpu_stat->stat_start) {
		seq_puts(seq, "Please stop the cpu duty cycle stats at first\n");
		goto out;
	}

	total_time = (u32) div64_u64(idle_dcstat_info.cal_duration,
			(u64) NSEC_PER_MSEC);

	seq_printf(seq, "\n| CPU# | %10s | rt ratio | MIPS (MHz) |\n",
			"run (ms)");
	for_each_possible_cpu(cpu) {
		percpu_stat = &per_cpu(cpu_dc_stat, cpu);
		av_mips = run_total = idle_total = 0;
		for (i = 0; i < percpu_stat->ops_stat_size; i++) {
			percpu_stat->idle_op_total[i] =
			percpu_stat->C1_op_total[i] + percpu_stat->C2_op_total[i];
			idle_time = (u32) div64_u64(percpu_stat->idle_op_total[i],
			(u64) NSEC_PER_MSEC);
			run_time = (u32) div64_u64(percpu_stat->runtime_op_total[i],
			(u64) NSEC_PER_MSEC);
			idle_total += idle_time;
			run_total += run_time;
			av_mips += (percpu_stat->ops_dcstat[i].pprate *
				 run_time);
		}
		av_mips_total += av_mips;
		if (!total_time) {
			seq_puts(seq, "No stat information! ");
			seq_puts(seq, "Help information :\n");
			seq_puts(seq, "1. echo 1 to start duty cycle stat.\n");
			seq_puts(seq, "2. echo 0 to stop duty cycle stat.\n");
			seq_puts(seq, "3. cat to check duty cycle info during stat.\n\n");
			goto out;
		}
		av_mips_l = 0;
		av_mips_h = div_u64_rem(av_mips, total_time, &av_mips_l);
		av_mips_l = div_u64(av_mips_l * 100, total_time);
		dc_int = calculate_dc(run_total, total_time, &dc_fra);
		seq_printf(seq, "| %-4u | %10u | %4u.%02u%% | %7u.%02u |\n", cpu,
				run_total, dc_int, dc_fra, av_mips_h, av_mips_l);
		rt_total += run_total;
	}
	seq_puts(seq, "\n");
	rt_l = 0;
	rt_h = calculate_dc(rt_total, total_time, &rt_l);
	av_mips_l = 0;
	av_mips_h = div_u64_rem(av_mips_total, total_time, &av_mips_l);
	av_mips_l = div_u64(av_mips_l * 100, total_time);
	seq_printf(seq, "Total stat for %ums, total MIPS: %u.%02uMHz. rt_total %u.%02u%%\n\n",
			total_time, av_mips_h, av_mips_l, rt_h, rt_l);

	seq_printf(seq, "| %-10s | %5s | %10s | %10s |\n",
		     "state", "ratio", "time(ms)", "count");
	seq_printf(seq, "| %-10s | %4lld%% | %10lld | %10lld |\n", "All active",
		     div64_u64(idle_dcstat_info.total_all_active *
			       (u64) (100), idle_dcstat_info.cal_duration),
		     div64_u64(idle_dcstat_info.total_all_active,
			       (u64) NSEC_PER_MSEC),
		     idle_dcstat_info.total_all_active_count);

	idle_l = 0;
	total_all_idle = (u32) div64_u64(idle_dcstat_info.total_all_idle,
			(u64) NSEC_PER_MSEC);
	idle_h = calculate_dc(total_all_idle, total_time, &idle_l);
	seq_printf(seq, "| %-10s | %2u.%2u%%| %10d | %10lld |\n", "All idle",
		     idle_h, idle_l, total_all_idle,
		     idle_dcstat_info.total_all_idle_count);

	if (multi_cluster) {
		seq_printf(seq, "| %-10s | %4lld%% | %10lld | %10lld |\n", "M2_clst0",
			     div64_u64(idle_dcstat_info.M2_cluster0_idle_total * (u64) (100),
				       idle_dcstat_info.cal_duration),
			     div64_u64(idle_dcstat_info.M2_cluster0_idle_total, (u64) NSEC_PER_MSEC),
			     idle_dcstat_info.M2_cluster0_count);
		seq_printf(seq, "| %-10s | %4lld%% | %10lld | %10lld |\n", "M2_clst1",
			     div64_u64(idle_dcstat_info.M2_cluster1_idle_total * (u64) (100),
				       idle_dcstat_info.cal_duration),
			     div64_u64(idle_dcstat_info.M2_cluster1_idle_total, (u64) NSEC_PER_MSEC),
			     idle_dcstat_info.M2_cluster1_count);
	} else {
		seq_printf(seq, "| %-10s | %4lld%% | %10lld | %10lld |\n", "M2",
			     div64_u64(idle_dcstat_info.M2_idle_total * (u64) (100),
				       idle_dcstat_info.cal_duration),
			     div64_u64(idle_dcstat_info.M2_idle_total, (u64) NSEC_PER_MSEC),
			     idle_dcstat_info.M2_count);
	}
	seq_printf(seq, "| %-10s | %4lld%% | %10lld | %10lld |\n", "D1P",
		     div64_u64(idle_dcstat_info.D1P_idle_total *
			       (u64) (100), idle_dcstat_info.cal_duration),
		     div64_u64(idle_dcstat_info.D1P_idle_total, (u64) NSEC_PER_MSEC),
		     idle_dcstat_info.D1p_count);
	seq_printf(seq, "| %-10s | %4lld%% | %10lld | %10lld |\n", "D1",
		     div64_u64(idle_dcstat_info.D1_idle_total * (u64) (100),
			       idle_dcstat_info.cal_duration),
		     div64_u64(idle_dcstat_info.D1_idle_total, (u64) NSEC_PER_MSEC),
		     idle_dcstat_info.D1_count);
	seq_printf(seq, "| %-10s | %4lld%% | %10lld | %10lld |\n\n", "D2",
		     div64_u64(idle_dcstat_info.D2_idle_total * (u64) (100),
			       idle_dcstat_info.cal_duration),
		     div64_u64(idle_dcstat_info.D2_idle_total, (u64) NSEC_PER_MSEC),
		     idle_dcstat_info.D2_count);

	seq_printf(seq, "| %4s | %3s | %10s | %8s | %8s | %8s | %13s | %15s | %15s |\n",
			"CPU#", "OP#", "rate (MHz)",
			"run (ms)", "idle (ms)", "rt ratio", "All idle r, c",
			"C1 ratio, count", "C2 ratio, count");
	for_each_possible_cpu(cpu) {
		percpu_stat = &per_cpu(cpu_dc_stat, cpu);
		for (i = 0; i < percpu_stat->ops_stat_size; i++) {
			if (total_time) {
				busy_time = (u32) div64_u64(percpu_stat->runtime_op_total[i],
			(u64) NSEC_PER_MSEC);
				dc_int = calculate_dc(busy_time, total_time,
							  &dc_fra);
			}
			if (!i)
				seq_printf(seq, "| %-4d ", cpu);
			else
				seq_puts(seq, "|      ");
			seq_printf(seq, "| %-3u | %10lu | %8llu | %9llu | %4u.%2u%% |%3lld%%, %-7llu | %3lld%%, %-9lld | %3lld%%, %-9llu |\n",
				percpu_stat->ops_dcstat[i].ppindex,
				percpu_stat->ops_dcstat[i].pprate,
				div64_u64(percpu_stat->runtime_op_total[i],
				(u64) NSEC_PER_MSEC),
				div64_u64(percpu_stat->idle_op_total[i],
				(u64) NSEC_PER_MSEC),
				dc_int, dc_fra,
				div64_u64
				(idle_dcstat_info.
				 all_idle_op_total[i] * (u64) (100),
				 idle_dcstat_info.cal_duration),
				idle_dcstat_info.all_idle_count[i],
				div64_u64(percpu_stat->C1_op_total[i] *
					  (u64) (100),
					  idle_dcstat_info.
					  cal_duration),
				percpu_stat->C1_count[i],
				div64_u64(percpu_stat->C2_op_total[i] *
					  (u64) (100),
					  idle_dcstat_info.
					  cal_duration),
				percpu_stat->C2_count[i]);
		}
	}
	seq_puts(seq, "\n");

	for_each_possible_cpu(cpu) {
		percpu_stat = &per_cpu(cpu_dc_stat, cpu);
		seq_printf(seq, "| CPU%d idle | %15s | %15s |\n",
					cpu, "time (us)", "count");
		temp_total_time = temp_total_count = 0;
		for (i = 0; i <= MAX_BREAKDOWN_TIME; i++) {
			if (0 != percpu_stat->breakdown_time_total[i] ||
			    0 != percpu_stat->breakdown_time_count[i]) {
				seq_printf(seq, "| %9s | %15lld | %15lld |\n",
						lpm_time_string[i],
						div64_u64(percpu_stat->
							  breakdown_time_total
							  [i], (u64) NSEC_PER_USEC),
						percpu_stat->
						breakdown_time_count[i]);
				temp_total_time +=
				    div64_u64(percpu_stat->
					      breakdown_time_total[i],
					      (u64) NSEC_PER_USEC);
				temp_total_count +=
				    percpu_stat->breakdown_time_count[i];
			}
		}
		seq_printf(seq, "| %9s | %15u | %15u |\n", "SUM",
				temp_total_time, temp_total_count);
	}
	seq_puts(seq, "\n");

out:
	return 0;
}

static ssize_t cpu_dc_write(struct file *filp,
				const char __user *buffer, size_t count,
				loff_t *ppos)
{
	unsigned int start, cpu, i, ret;
	char buf[10] = { 0 };
	struct clk_dc_stat_info *percpu_stat = NULL;
	struct clk *cpu_clk;
	unsigned int j, rate, cpu_ops_size;
	unsigned long pprate;
	struct core_dcstat *op;

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;
	ret = sscanf(buf, "%d", &start);

	start = !!start;
	percpu_stat = &per_cpu(cpu_dc_stat, 0);
	if (start == percpu_stat->stat_start) {
		pr_err("[WARNING]CPU stat is already %s\n",
		       percpu_stat->stat_start ? "started" : "stopped");
		return -EINVAL;
	}

	idle_dcstat_info.init_flag = 1;
	list_for_each_entry(op, &core_dcstat_list, node) {
		cpu_clk = op->clk;
		rate = clk_get_rate(op->clk);
		rate /= MHZ;
		/*
		 * hold the same lock of clk_enable, disable, set_rate ops
		 * here to avoid the status change when start/stop and lead
		 * to incorrect stat info
		 */
		spin_lock(&clk_dc_lock);
		if (start) {
			/* clear old stat information */
			for (j = 0; j < op->cpu_num; j++) {
				cpu = op->cpu_id[j];
				percpu_stat = &per_cpu(cpu_dc_stat, cpu);
				for (i = 0; i < percpu_stat->ops_stat_size; i++) {
					percpu_stat->runtime_op_total[i] = 0;
					percpu_stat->idle_op_total[i] = 0;
				}

				cpu_ops_size = percpu_stat->ops_stat_size;
				for (i = 0; i < cpu_ops_size; i++) {
					pprate =
					percpu_stat->ops_dcstat[i].pprate;
					if (rate == pprate) {
						percpu_stat->curopindex = i;
						break;
					}
				}
				if (i >= cpu_ops_size)
					pr_err("current cpu rate is not in cpu duty cycle PP table\n");

				percpu_stat->stat_start = true;
				cpu_dcstat_event(cpu_clk, cpu,
					CLK_STAT_START, 0);
			}
		} else {
			/*we stop runtime and idle time stat, before cpu_dc_stat stop */
			for (j = 0; j < op->cpu_num; j++) {
				cpu = op->cpu_id[j];
				percpu_stat = &per_cpu(cpu_dc_stat, cpu);
				if ((u64) 0 != percpu_stat->runtime_start)
					cpu_dcstat_event(cpu_clk, cpu, CPU_IDLE_ENTER, LPM_C2);
				else
					cpu_dcstat_event(cpu_clk, cpu, CPU_IDLE_EXIT,
						MAX_LPM_INDEX);
			}
			for (j = 0; j < op->cpu_num; j++) {
				cpu = op->cpu_id[j];
				percpu_stat = &per_cpu(cpu_dc_stat, cpu);
				cpu_dcstat_event(cpu_clk, cpu,
					CLK_STAT_STOP, 0);
				percpu_stat->stat_start = false;
			}
		}
		spin_unlock(&clk_dc_lock);
	}
	return count;
}

static int cpu_dc_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpu_dc_show, NULL);
}

static const struct file_operations cpu_dc_ops = {
	.open		= cpu_dc_open,
	.read		= seq_read,
	.write		= cpu_dc_write,
	.llseek		= seq_lseek,
	.release	= single_release,
	.owner		= THIS_MODULE,
};

struct dentry *cpu_dcstat_file_create(const char *file_name,
				struct dentry *parent)
{
	struct dentry *cpu_dc_stat;
	cpu_dc_stat = debugfs_create_file(file_name,
			S_IRUGO | S_IWUSR | S_IWGRP, parent, NULL, &cpu_dc_ops);

	if (!cpu_dc_stat) {
		pr_err("debugfs entry created failed in %s\n", __func__);
		return 0;
	}

	return cpu_dc_stat;
}
EXPORT_SYMBOL(cpu_dcstat_file_create);

struct dentry *clk_dcstat_file_create(const char *file_name,
	struct dentry *parent,
	const struct file_operations *dc_ops)
{
	struct dentry *dc_stat;
	dc_stat = debugfs_create_file(file_name, 0664, parent,
					NULL, dc_ops);
	if (!dc_stat) {
		pr_err("debugfs entry created failed in %s\n", __func__);
		return 0;
	}

	return dc_stat;
}
EXPORT_SYMBOL(clk_dcstat_file_create);
