// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

/*
 * If a irq is frequently triggered, it could result in problems.
 * The purpose of this feature is to catch the condition. When the
 * average time interval of a irq is below the threshold, we judge
 * the irq is triggered abnormally and print a message for reference.
 *
 * average time interval =
 *     statistics time / irq count increase during the statistics time
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/kernel_stat.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/percpu-defs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <mt-plat/aee.h>

#include "internal.h"

static bool irq_count_tracer __read_mostly;
static unsigned int irq_period_th1_ns = 666666; /* log */
static unsigned int irq_period_th2_ns = 200000; /* aee */
static unsigned int irq_count_aee_limit = 1;
/* period setting for specific irqs */
struct irq_count_period_setting {
	const char *name;
	unsigned int period;
} irq_count_plist[] = {
	{"usb0", 16666}, /* 60000 irqs per sec*/
	{"ufshcd", 10000}, /* 100000 irqs per sec*/
	{"arch_timer", 50000}, /* 20000 irqs per sec*/
	{"musb-hdrc", 16666}, /* 60000 irqs per sec*/
	{"11201000.usb0", 16666}, /* 60000 irqs per sec*/
	{"wlan0", 12500}, /* 80000 irqs per sec*/
	{"DPMAIF_AP", 1837}, /* 544125 irqs per sec */ /* data tput */
	{"CCIF_AP_DATA0", 0}, /* No limit for MD EE to save debug logs. */
};

const char *irq_to_name(int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);

	if (desc && desc->action && desc->action->name)
		return desc->action->name;
	return NULL;
}

const void *irq_to_handler(int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);

	if (desc && desc->action && desc->action->handler)
		return (void *)desc->action->handler;
	return NULL;
}

const int irq_to_ipi_type(int irq)
{
	struct irq_desc **ipi_desc = ipi_desc_get();
	struct irq_desc *desc = irq_to_desc(irq);
	int nr_ipi = nr_ipi_get();
	int i = 0;

	for (i = 0; i < nr_ipi; i++)
		if (ipi_desc[i] == desc)
			return i;
	return -1;
}

/*
 * return true: not in debounce (will do aee) and update time if update is true
 * return false: in debounce period (not do aee) and do not update time.
 */
bool irq_mon_aee_debounce_check(bool update)
{
	static unsigned long long irq_mon_aee_debounce = 5000000000; /* 5s */
	static unsigned long long t_prev_aee;
	static int in_debounce_check;
	unsigned long long t_check = 0;
	bool ret = true;

	/*
	 * if in_debounce_check = 0, set to 1 and return 0 (continue checking)
	 * if in_debounce_check = 1, return 1 (return false to caller)
	 */
	if (cmpxchg(&in_debounce_check, 0, 1))
		return false;

	t_check = sched_clock();

	if (t_prev_aee && irq_mon_aee_debounce &&
	    (t_check - t_prev_aee) < irq_mon_aee_debounce)
		ret = false;
	else if (update)
		t_prev_aee = t_check;

	xchg(&in_debounce_check, 0);

	return ret;
}

#ifdef MODULE
// workaround for kstat_irqs_cpu & kstat_irqs
static unsigned int irq_mon_irqs(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned int sum = 0;
	int cpu;

	if (!desc || !desc->kstat_irqs)
		return 0;

	for_each_possible_cpu(cpu)
		sum += *per_cpu_ptr(desc->kstat_irqs, cpu);
	return sum;
}

static unsigned int irq_mon_irqs_cpu(unsigned int irq, int cpu)
{
	struct irq_desc *desc = irq_to_desc(irq);

	return desc && desc->kstat_irqs ?
			*per_cpu_ptr(desc->kstat_irqs, cpu) : 0;
}
#else
#define irq_mon_irqs(irq) kstat_irqs(irq)
#define irq_mon_irqs_cpu(irq, cpu) kstat_irqs_cpu(irq, cpu)
#endif

#define REC_NUM 4

/* per cpu */
struct irq_count_stat {
	int enabled;
	unsigned long long t_start;
	unsigned long long t_end;
};

static struct irq_count_stat __percpu *irq_count_data;
static struct hrtimer __percpu *irq_count_tracer_hrtimer;

/* per irq and per record */
struct irq_count_rec {
	unsigned int num;
	unsigned int diff;
	bool warn;
};

/* per irq */
struct irq_count_desc {
	unsigned int __percpu *count;
	struct irq_count_rec rec[REC_NUM];
	u64 __percpu *time;
};

/* per record */
struct irq_count_all {
	spinlock_t lock; /* protect this struct and desc->rec[REC_NUM] */
	unsigned long long ts;
	unsigned long long te;
};

static struct irq_count_all irq_cpus[REC_NUM];
static unsigned int rec_indx;
static DEFINE_XARRAY(irqs_desc_xa);

static struct irq_count_desc *irq_count_desc_lookup(int irq)
{
	return xa_load(&irqs_desc_xa, irq);
}

static struct irq_count_desc *irq_count_desc_alloc(int irq)
{
	struct irq_count_desc *desc;
	int err = 0;

	desc = kzalloc(sizeof(*desc), GFP_ATOMIC);
	if (!desc)
		goto out;
	desc->count = alloc_percpu_gfp(unsigned int, GFP_ATOMIC);
	if (!desc->count)
		goto out_free_desc;
	desc->time = alloc_percpu_gfp((*desc->time), GFP_ATOMIC);
	if (!desc->time)
		goto out_free_count;
	/*
	 * This entry might be stored by concurrent irq_count_desc_alloc()
	 * Use xa_insert() to prevent override the entry.
	 */
	err = xa_insert(&irqs_desc_xa, irq, desc, GFP_ATOMIC);
	if (!err)
		goto out;

	free_percpu(desc->time);
out_free_count:
	free_percpu(desc->count);
out_free_desc:
	kfree(desc);

	/* Try to return the entry if it is present. */
	desc =  (err == -EBUSY) ? xa_load(&irqs_desc_xa, irq) : NULL;
out:
	return desc;
}

/* account the irq time for current cpu */
void irq_mon_account_irq_time(u64 time, int irq)
{
	struct irq_count_desc *desc = irq_count_desc_lookup(irq);

	desc = (desc) ? : irq_count_desc_alloc(irq);
	if (desc)
		__this_cpu_add(*desc->time, time);
}

static int irq_time_proc_show(struct seq_file *m, void *v)
{
	unsigned long index;
	struct irq_count_desc *desc;
	int cpu;

	xa_for_each(&irqs_desc_xa, index, desc) {
		seq_printf(m, "%u", (unsigned int)index);
		for_each_possible_cpu(cpu)
			seq_printf(m, " %llu", *per_cpu_ptr(desc->time, cpu));
		seq_putc(m, '\n');
	}
	return 0;
}

static unsigned int irq_count_irqs_cpu(int irq, int cpu)
{
	struct irq_count_desc *desc;

	desc = irq_count_desc_lookup(irq);
	return (desc && desc->count) ? *per_cpu_ptr(desc->count, cpu) : 0;
}

static void irq_count_save_irqs_cpu(int irq, unsigned int irqs, int cpu)
{
	struct irq_count_desc *desc;

	desc = irq_count_desc_lookup(irq);
	if (!desc)
		desc = irq_count_desc_alloc(irq);

	if (desc)
		*per_cpu_ptr(desc->count, cpu) = irqs;
}

static void __show_irq_count_info(unsigned int output)
{
	int cpu;

	irq_mon_msg(output, "===== IRQ Status =====");

	for_each_possible_cpu(cpu) {
		struct irq_count_stat *irq_cnt;
		int irq;
		unsigned long long now, prev;

		now = sched_clock();

		irq_cnt = per_cpu_ptr(irq_count_data, cpu);
		prev = irq_cnt->t_end;

		irq_mon_msg(output, "CPU: %d", cpu);
		irq_mon_msg(output, "from %lld.%06lu to %lld.%06lu, %lld ms",
			    sec_high(prev), sec_low(prev),
			    sec_high(now), sec_low(now),
			    msec_high(now - prev));

		for_each_irq_nr(irq) {
			unsigned int count, prev_count;
			const char *irq_name;

			count = irq_mon_irqs_cpu(irq, cpu);
			if (!count)
				continue;
			prev_count = irq_count_irqs_cpu(irq, cpu);

			irq_name = irq_to_name(irq);
			if (irq_name && !strcmp(irq_name, "IPI"))
				irq_mon_msg(output, "    %d:%s%d +%d(%d)",
					    irq, irq_name, irq_to_ipi_type(irq),
					    count - prev_count, count);
			else
				irq_mon_msg(output, "    %d:%s +%d(%d)",
					    irq, irq_name ? irq_name : "NULL",
					    count - prev_count, count);
		}
		irq_mon_msg(output, "");
	}
}

void show_irq_count_info(unsigned int output)
{
	if (irq_count_tracer)
		__show_irq_count_info(output);
}

enum hrtimer_restart irq_count_tracer_hrtimer_fn(struct hrtimer *hrtimer)
{
	struct irq_count_stat *irq_cnt = this_cpu_ptr(irq_count_data);
	int cpu = smp_processor_id();
	int irq, irq_num, i, skip;
	unsigned int count;
	unsigned long long t_avg, t_diff, t_diff_ms;
	char aee_msg[MAX_MSG_LEN];
	int list_num = ARRAY_SIZE(irq_count_plist);

	if (!irq_count_tracer)
		return HRTIMER_NORESTART;

	hrtimer_forward_now(hrtimer, ms_to_ktime(1000));
	/* skip checking if more than one work is blocked in queue */
	if (sched_clock() - irq_cnt->t_end < 500000000ULL)
		return HRTIMER_RESTART;

	/* check irq count on all cpu */
	if (cpu == 0) {
		unsigned int pre_idx;
		unsigned int pre_num;
		struct irq_count_desc *desc;

		spin_lock(&irq_cpus[rec_indx].lock);

		pre_idx = rec_indx ? rec_indx - 1 : REC_NUM - 1;
		irq_cpus[rec_indx].ts = irq_cpus[pre_idx].te;
		irq_cpus[rec_indx].te = sched_clock();

		for_each_irq_nr(irq) {
			irq_num = irq_mon_irqs(irq);
			desc = irq_count_desc_lookup(irq);
			if (!desc) {
				/* Don't alloc memory if no irqs ever */
				if (!irq_num)
					continue;

				desc = irq_count_desc_alloc(irq);
				if (!desc)
					continue;
			}
			pre_num = desc->rec[pre_idx].num;
			desc->rec[rec_indx].num = irq_num;
			desc->rec[rec_indx].diff = irq_num - pre_num;
			desc->rec[rec_indx].warn = 0;
		}

		spin_unlock(&irq_cpus[rec_indx].lock);

		rec_indx = (rec_indx == REC_NUM - 1) ? 0 : rec_indx + 1;

		if (0)
			show_irq_count_info(TO_BOTH);
	}

	irq_cnt->t_start = irq_cnt->t_end;
	irq_cnt->t_end = sched_clock();
	t_diff = irq_cnt->t_end - irq_cnt->t_start;

	for_each_irq_nr(irq) {
		const char *tmp_irq_name = NULL;
		char irq_name[64];
		char irq_handler_addr[20];
		char irq_handler_name[64];
		const void *irq_handler = NULL;

		irq_num = irq_mon_irqs_cpu(irq, cpu);
		count = irq_num - irq_count_irqs_cpu(irq, cpu);

		/* The irq is not triggered in this period */
		if (count == 0)
			continue;

		irq_count_save_irqs_cpu(irq, irq_num, cpu);
		/* The irq count is decreased */
		if (unlikely(count > UINT_MAX / 2))
			continue;

		t_avg = t_diff;
		t_diff_ms = t_diff;
		do_div(t_avg, count);
		do_div(t_diff_ms, 1000000);

		if (t_avg > irq_period_th1_ns)
			continue;

		tmp_irq_name = irq_to_name(irq);
		if (!tmp_irq_name)
			continue;

		for (i = 0, skip = 0; i < list_num && !skip; i++) {
			if (!strcmp(tmp_irq_name, irq_count_plist[i].name))
				if (t_avg > irq_count_plist[i].period)
					skip = 1;
		}

		if (skip)
			continue;

		if (!strcmp(tmp_irq_name, "IPI")) {
			scnprintf(irq_name, sizeof(irq_name), "%s%d",
				  tmp_irq_name, irq_to_ipi_type(irq));
			skip = 1;
		} else {
			scnprintf(irq_name, sizeof(irq_name), "%s", tmp_irq_name);
		}

		irq_handler = irq_to_handler(irq);
		scnprintf(irq_handler_addr, sizeof(irq_handler_addr), "%px", irq_handler);
		scnprintf(irq_handler_name, sizeof(irq_handler_name), "%pS", irq_handler);

		for (i = 0; i < REC_NUM; i++) {
			char msg[MAX_MSG_LEN];
			struct irq_count_desc *desc;

			spin_lock(&irq_cpus[i].lock);

			desc = irq_count_desc_lookup(irq);
			if (!desc || desc->rec[i].warn || !desc->rec[i].diff) {
				spin_unlock(&irq_cpus[i].lock);
				continue;
			}
			desc->rec[i].warn = 1;

			t_diff_ms = irq_cpus[i].te - irq_cpus[i].ts;
			do_div(t_diff_ms, 1000000);

			scnprintf(msg, sizeof(msg),
				  "irq: %d [<%s>]%s, %s count +%d in %lld ms, from %lld.%06lu to %lld.%06lu on all CPU",
				  irq, irq_handler_addr, irq_handler_name, irq_name,
				  desc->rec[i].diff, t_diff_ms,
				  sec_high(irq_cpus[i].ts),
				  sec_low(irq_cpus[i].ts),
				  sec_high(irq_cpus[i].te),
				  sec_low(irq_cpus[i].te));

			if (irq_period_th2_ns && t_avg < irq_period_th2_ns &&
			    irq_count_aee_limit && !skip)
				/* aee threshold and aee limit meet */
				if (!irq_mon_aee_debounce_check(false))
					/* in debounce period, to FTRACE only */
					irq_mon_msg(TO_FTRACE, msg);
				else
					/* will do aee and kernel log */
					irq_mon_msg(TO_BOTH, msg);
			else
				/* no aee, just logging (to FTRACE) */
				irq_mon_msg(TO_FTRACE, msg);

			spin_unlock(&irq_cpus[i].lock);
		}

		scnprintf(aee_msg, sizeof(aee_msg),
			  "irq: %d [<%s>]%s, %s count +%d in %lld ms, from %lld.%06lu to %lld.%06lu on CPU:%d",
			  irq, irq_handler_addr, irq_handler_name, irq_name,
			  count, t_diff_ms,
			  sec_high(irq_cnt->t_start), sec_low(irq_cnt->t_start),
			  sec_high(irq_cnt->t_end), sec_low(irq_cnt->t_end),
			  raw_smp_processor_id());

		if (irq_period_th2_ns && t_avg < irq_period_th2_ns &&
		    irq_count_aee_limit && !skip)
			/* aee threshold and aee limit meet */
			if (!irq_mon_aee_debounce_check(true)) {
				/* in debounce period, to FTRACE only */
				irq_mon_msg(TO_FTRACE, aee_msg);
			} else {
				char module[100];
				/* do aee and kernel log */
				irq_mon_msg(TO_BOTH, aee_msg);
				scnprintf(module, sizeof(module),
					  "BURST IRQ:%d, %s %s +%d in %lldms",
					  irq, irq_handler_name, irq_name,
					  count, t_diff_ms);
				aee_kernel_warning_api(__FILE__, __LINE__,
						       DB_OPT_DUMMY_DUMP | DB_OPT_FTRACE,
						       module, aee_msg);
			}
		else
			/* no aee, just logging (to FTRACE) */
			irq_mon_msg(TO_FTRACE, aee_msg);
	}

	return HRTIMER_RESTART;
}

static void irq_count_tracer_start(int cpu)
{
	struct hrtimer *hrtimer = this_cpu_ptr(irq_count_tracer_hrtimer);

	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_HARD);
	hrtimer->function = irq_count_tracer_hrtimer_fn;
	hrtimer_start(hrtimer, ms_to_ktime(1000), HRTIMER_MODE_REL_PINNED_HARD);
}

static int irq_count_tracer_start_fn(void *ignore)
{
	int cpu = smp_processor_id();

	per_cpu_ptr(irq_count_data, cpu)->enabled = 1;
	barrier();
	irq_count_tracer_start(cpu);
	return 0;
}

static void irq_count_tracer_work(struct work_struct *work)
{
	int cpu, done;

	do {
		done = 1;
		for_each_possible_cpu(cpu) {
			if (per_cpu_ptr(irq_count_data, cpu)->enabled)
				continue;

			if (cpu_online(cpu))
				smp_call_on_cpu(cpu, irq_count_tracer_start_fn, NULL, false);
			else
				done = 0;
		}
		if (!done)
			msleep(500);
	} while (!done);
}

static void irq_count_rec_init(void)
{
	int i, irq;
	unsigned int irq_num;
	struct irq_count_desc *desc;

	for (i = 0; i < REC_NUM; i++)
		spin_lock_init(&irq_cpus[i].lock);

	WARN_ON(rec_indx);

	/* Initialize first record. no race before the tracer start */
	/* Disable preemption to get more precise values */
	preempt_disable();
	irq_cpus[rec_indx].ts = 0;
	irq_cpus[rec_indx].te = sched_clock();

	for_each_irq_nr(irq) {
		irq_num = irq_mon_irqs(irq);
		if (!irq_num)
			continue;
		/*
		 * The irq_count_desc_alloc() might be called in
		 * interrupt context. Let's disable irq here to
		 * prevent inconsistent lock state.
		 */
		local_irq_disable();
		desc = irq_count_desc_alloc(irq);
		local_irq_enable();
		if (!desc)
			continue;
		desc->rec[rec_indx].num = irq_num;
		desc->rec[rec_indx].diff = irq_num;
		desc->rec[rec_indx].warn = 0;
	}
	preempt_enable();

	rec_indx = (rec_indx == REC_NUM - 1) ? 0 : rec_indx + 1;
}

extern bool b_count_tracer_default_enabled;
static DECLARE_WORK(tracer_work, irq_count_tracer_work);
int irq_count_tracer_init(void)
{
	irq_count_data = alloc_percpu(struct irq_count_stat);
	if (!irq_count_data) {
		pr_info("Failed to alloc irq_count_data\n");
		return -ENOMEM;
	}

	irq_count_tracer_hrtimer = alloc_percpu(struct hrtimer);
	if (!irq_count_tracer_hrtimer) {
		free_percpu(irq_count_data);
		pr_info("Failed to alloc irq_count_tracer_hrtimer\n");
		return -ENOMEM;
	}

	irq_count_rec_init();
	if (b_count_tracer_default_enabled) {
		irq_count_tracer = 1;
		schedule_work(&tracer_work);
	}
	return 0;
}

void irq_count_tracer_exit(void)
{
	struct irq_count_desc *desc;
	unsigned long index;

	free_percpu(irq_count_data);
	free_percpu(irq_count_tracer_hrtimer);
	xa_for_each(&irqs_desc_xa, index, desc) {
		free_percpu(desc->count);
		kfree(desc);
	}
	xa_destroy(&irqs_desc_xa);
}

/* Must holding lock*/
void irq_count_tracer_set(bool val)
{
	int cpu;

	if (irq_count_tracer == val)
		return;

	if (val) {
		/* restart irq_count_tracer */
		irq_count_tracer = 1;
		schedule_work(&tracer_work);
	} else {
		flush_work(&tracer_work);
		irq_count_tracer = 0;
		for_each_possible_cpu(cpu) {
			struct hrtimer *hrtimer =
				per_cpu_ptr(irq_count_tracer_hrtimer, cpu);

			per_cpu_ptr(irq_count_data, cpu)->enabled = 0;
			hrtimer_cancel(hrtimer);
		}
	}
}

const struct proc_ops irq_mon_count_pops = {
	.proc_open = irq_mon_bool_open,
	.proc_write = irq_mon_count_set,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

void irq_count_tracer_proc_init(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *dir;

	proc_create_single("irq_time", 0444, parent, irq_time_proc_show);
	dir = proc_mkdir("irq_count_tracer", parent);
	if (!dir)
		return;

	proc_create_data("irq_count_tracer", 0644,
			 dir, &irq_mon_count_pops, (void *)&irq_count_tracer);
	IRQ_MON_TRACER_PROC_ENTRY(irq_period_th1_ns, 0644, uint, dir, &irq_period_th1_ns);
	IRQ_MON_TRACER_PROC_ENTRY(irq_period_th2_ns, 0644, uint, dir, &irq_period_th2_ns);
	IRQ_MON_TRACER_PROC_ENTRY(irq_count_aee_limit, 0644, uint, dir, &irq_count_aee_limit);
	return;
}
