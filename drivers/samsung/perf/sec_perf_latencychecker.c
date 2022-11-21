/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung TN debugging code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sec_debug.h>
#include <linux/debugfs.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/stacktrace.h>
#include <linux/sec_perf.h>
#include <soc/samsung/exynos-coresight.h>
#include <linux/debug-snapshot.h>
#include <asm/stack_pointer.h>
#include <asm/stacktrace.h>
#include <linux/delay.h>
#include <asm/irq_regs.h>
#include <linux/cpu.h>
#include <linux/cpu_pm.h>
#include "../../../kernel/sched/sched.h"

#define MAX_LATENCYCHECKER_LOG 128
#define MAX_LATENCYCHECKER_CALLER 32
#define CALLSTACK_MAX_NUM 5
#define LOG_LINE_MAX		512
#define MIN_THRESHOLD_MS 10

enum latency_type {
	LATENCY_NONE,
	LATENCY_TASK,
	LATENCY_IRQ,
};

struct latencychecker_info {
	unsigned long long time;
	char task_comm[TASK_COMM_LEN];
	void *pc;
	void *fn;
	int cpu;
	int detective;
	enum latency_type type;
};

struct latencychecker_caller {
	unsigned long long time;
	char task_comm[TASK_COMM_LEN];
	void *addrs[CALLSTACK_MAX_NUM];
	int cpu;
};

static cpumask_t __read_mostly latencychecker_cpus;
static cpumask_t __read_mostly online_cpus;
static unsigned long __read_mostly hardlatency_thresh = 20;

static DEFINE_PER_CPU(unsigned long, latencychecker_interrupts);
static DEFINE_PER_CPU(unsigned long, latencychecker_interrupts_saved);
static DEFINE_PER_CPU(unsigned long, hardlatency_touch_ts);
static DEFINE_PER_CPU(atomic_t, latencychecker_ipi_pending);

static const char * const latency_type_name[] = {
	"NONE", "TASK", "IRQ"
};

static atomic_t latencychecker_info_idx = ATOMIC_INIT(-1);
static atomic_t latencychecker_caller_idx = ATOMIC_INIT(-1);
static atomic_t latencychecker_active = ATOMIC_INIT(0);
static atomic_t latencychecker_dumping = ATOMIC_INIT(0);
static bool init_complete;
static bool latencychecker_enable = true;
static DEFINE_PER_CPU(call_single_data_t, latencychecker_csd);

static struct latencychecker_info lc_info[MAX_LATENCYCHECKER_LOG];
static struct latencychecker_caller lc_caller[MAX_LATENCYCHECKER_CALLER];


static unsigned int latencychecker_next_cpu(unsigned int cpu)
{
	cpumask_t cpus = latencychecker_cpus;
	unsigned int next_cpu;

	next_cpu = cpumask_next(cpu, &cpus);
	if (next_cpu >= nr_cpu_ids)
		next_cpu = cpumask_first(&cpus);

	if (next_cpu == cpu)
		return nr_cpu_ids;

	return next_cpu;
}

static unsigned long get_timestamp(void)
{
	return running_clock() >> 20LL;  /* 2^20 ~= 10^6 */
}

static void __touch_latencychecker(void)
{
	__this_cpu_write(hardlatency_touch_ts, get_timestamp());
}

static void latencychcker_interrupt_count(void)
{
	__this_cpu_inc(latencychecker_interrupts);
}

static void latencychecker_save_stack_handler(void *data)
{
	struct pt_regs *regs;
	unsigned long index;

	if (atomic_read(&latencychecker_dumping) == 1 || !latencychecker_enable)
		return;

	atomic_inc(&latencychecker_active);
	index = atomic_inc_return(&latencychecker_caller_idx) & (MAX_LATENCYCHECKER_CALLER - 1);
	regs = get_irq_regs();

	lc_caller[index].time = local_clock();
	lc_caller[index].cpu = smp_processor_id();
	strncpy(lc_caller[index].task_comm, current->comm, TASK_COMM_LEN - 1);

	if (regs) {
#ifdef CONFIG_STACKTRACE
		struct stack_trace trace;

		trace.nr_entries = 0;
		trace.max_entries = CALLSTACK_MAX_NUM;
		trace.entries = (unsigned long *)lc_caller[index].addrs;
		trace.skip = 0;
		save_stack_trace_regs(regs, &trace);
#endif
	}
	atomic_set(this_cpu_ptr(&latencychecker_ipi_pending), 0);
	atomic_dec(&latencychecker_active);
}

static int is_hardlatency_other_cpu(unsigned int cpu)
{
	unsigned long hrint = per_cpu(latencychecker_interrupts, cpu);

	if (per_cpu(latencychecker_interrupts_saved, cpu) == hrint) {
		unsigned long now = get_timestamp();
		unsigned long touch_ts = per_cpu(hardlatency_touch_ts, cpu);

		if (time_after(now, touch_ts) &&
				(now - touch_ts >= hardlatency_thresh)) {
			per_cpu(hardlatency_touch_ts, cpu) = now;
			return 1;
		}
	}

	per_cpu(latencychecker_interrupts_saved, cpu) = hrint;
	return 0;
}

static void sec_perf_latencychecker_save_latency_info(unsigned int next_cpu)
{
	unsigned long index;

	index = atomic_inc_return(&latencychecker_info_idx) & (MAX_LATENCYCHECKER_LOG - 1);

	lc_info[index].time = local_clock();
	lc_info[index].cpu = next_cpu;
	lc_info[index].detective = smp_processor_id();
	strncpy(lc_info[index].task_comm, cpu_curr(next_cpu)->comm, TASK_COMM_LEN - 1);

	if (dbg_snapshot_get_sjtag_status() == true)
		lc_info[index].pc = 0;
	else
		lc_info[index].pc = (void *)exynos_cs_read_pc(next_cpu);
	lc_info[index].fn = (void *)secdbg_snapshot_get_hardlatency_info(next_cpu);

	if (lc_info[index].fn) {
		lc_info[index].type = LATENCY_IRQ;
	} else {
		lc_info[index].type = LATENCY_TASK;
		if (atomic_cmpxchg_acquire(per_cpu_ptr(&latencychecker_ipi_pending, next_cpu), 0, 1) == 0) {
			smp_call_function_single_async(next_cpu, per_cpu_ptr(&latencychecker_csd, next_cpu));
		}
	}

	pr_debug("LatencyChecker detected hard latency over %lu ms on cpu %u. Where : %s Detective : %u PC : %pS FN : %pS",
		hardlatency_thresh, next_cpu, latency_type_name[lc_info[index].type], lc_info[index].detective, lc_info[index].pc, lc_info[index].fn);
}

void sec_perf_latencychecker_check_latency_other_cpu(void)
{
	unsigned int next_cpu;

	if (!init_complete || !latencychecker_enable)
		return;

	__touch_latencychecker();
	latencychcker_interrupt_count();

	if (atomic_read(&latencychecker_dumping) == 1)
		return;

	 atomic_inc(&latencychecker_active);

	/* check for hard latency on the next cpu */
	next_cpu = latencychecker_next_cpu(smp_processor_id());

	if (next_cpu >= nr_cpu_ids)
		goto out;

	if (is_hardlatency_other_cpu(next_cpu))
		sec_perf_latencychecker_save_latency_info(next_cpu);

out:

	atomic_dec(&latencychecker_active);
}

void sec_perf_latencychecker_stop(void)
{
	latencychecker_enable = false;
}

static int sec_perf_latencychecker_start_cpu(unsigned int cpu)
{
	pr_info("cpu%u %s\n", cpu, __func__);
	__touch_latencychecker();
	cpumask_set_cpu(cpu, &online_cpus);
	cpumask_set_cpu(cpu, &latencychecker_cpus);

	return 0;
}

static int sec_perf_latencychecker_stop_cpu(unsigned int cpu)
{
	pr_info("cpu%u %s\n", cpu, __func__);
	cpumask_clear_cpu(cpu, &online_cpus);
	cpumask_clear_cpu(cpu, &latencychecker_cpus);

	return 0;
}

int sec_perf_latencychecker_enable(unsigned int cpu)
{
	__touch_latencychecker();

	if(cpumask_test_cpu((cpu), &online_cpus))
		cpumask_set_cpu(cpu, &latencychecker_cpus);

	return 0;
}

int sec_perf_latencychecker_disable(unsigned int cpu)
{
	if(cpumask_test_cpu((cpu), &online_cpus))
		cpumask_clear_cpu(cpu, &latencychecker_cpus);

	return 0;
}

static int __init sec_perf_latencychecker_set_thresh(char *str)
{
	unsigned long threshold;
	int ret;

	if (!str)
		return 0;
	ret = kstrtoul(str, 0, &threshold);
	if (ret < 0)
		return 0;

	if (threshold < MIN_THRESHOLD_MS)
		hardlatency_thresh = MIN_THRESHOLD_MS;
	else
		hardlatency_thresh = threshold;

	return 1;
}
__setup("latencychecker_thresh=", sec_perf_latencychecker_set_thresh);

static ssize_t sec_perf_latencychecker_threshold_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	char buf[80];
	ssize_t ret;

	ret = snprintf(buf, sizeof(buf), "%lu\n", hardlatency_thresh);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t sec_perf_latencychecker_threshold_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	int threshold;
	ssize_t retval;

	retval = kstrtoint_from_user(user_buf, count, 10, &threshold);
	if (retval)
		return retval;

	if (threshold < MIN_THRESHOLD_MS)
		hardlatency_thresh = MIN_THRESHOLD_MS;
	else
		hardlatency_thresh = threshold;

	return count;
}

static const struct file_operations sec_perf_latencychecker_threshold_fops = {
	.open    = simple_open,
	.read    = sec_perf_latencychecker_threshold_read,
	.write = sec_perf_latencychecker_threshold_write,
	.llseek  = default_llseek,
};

static int __init sec_perf_latencychecker_activate(char *str)
{
	unsigned long enable;
	int ret;

	if (!str)
		return 0;

	ret = kstrtoul(str, 0, &enable);
	if (ret < 0)
		return 0;

	if (enable)
		latencychecker_enable = true;
	else
		latencychecker_enable = false;

	return 1;
}

__setup("latencychecker_enable=", sec_perf_latencychecker_activate);

static ssize_t sec_perf_latencychecker_enable_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	char buf[80];
	ssize_t ret;

	ret = snprintf(buf, sizeof(buf), "%u\n", (unsigned int)latencychecker_enable);
	if (ret < 0)
		return ret;

	return simple_read_from_buffer(user_buf, count, ppos, buf, ret);
}

static ssize_t sec_perf_latencychecker_enable_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	int enable;
	ssize_t retval;

	retval = kstrtoint_from_user(user_buf, count, 10, &enable);
	if (retval)
		return retval;

	if (enable)
		latencychecker_enable = true;
	else
		latencychecker_enable = false;

	return count;
}

static const struct file_operations sec_perf_latencychecker_enable_fops = {
	.open    = simple_open,
	.read    = sec_perf_latencychecker_enable_read,
	.write = sec_perf_latencychecker_enable_write,
	.llseek  = default_llseek,
};

static void sec_perf_latencychecker_dump(struct seq_file *m, int index)
{
	char buf[LOG_LINE_MAX];
	ssize_t offset = 0;

	offset = snprintf(buf, LOG_LINE_MAX, "%16llu  %d   %16s   %4s    %d       %pS",
		lc_info[index].time, lc_info[index].cpu, lc_info[index].task_comm, latency_type_name[lc_info[index].type], lc_info[index].detective, lc_info[index].pc);
	if (lc_info[index].type == LATENCY_IRQ)
		offset += snprintf(buf + offset, LOG_LINE_MAX - offset, " %pS", lc_info[index].fn);
	seq_printf(m, "%s\n", buf);
}

static void sec_perf_latencychecker_dump_caller(struct seq_file *m, int index)
{
	int i;
	char buf[LOG_LINE_MAX];
	ssize_t offset = 0;

	offset = snprintf(buf, LOG_LINE_MAX, "%16llu  %d   %16s ", lc_caller[index].time, lc_caller[index].cpu, lc_caller[index].task_comm);
	for (i = 0; i < CALLSTACK_MAX_NUM; i++)
		offset += snprintf(buf + offset, LOG_LINE_MAX - offset, "%c %pS ", (i == 0) ? ' ' : '<', lc_caller[index].addrs[i]);
	seq_printf(m, "%s\n", buf);
}

static int sec_perf_latencychecker_dump_show(struct seq_file *m, void *v)
{
	int i;
	int start_idx;
	int total;

	atomic_set(&latencychecker_dumping, 1);

	/* we cannot wait IPI that dump call stack. Although no information for call stack, start dump */
	while (atomic_read(&latencychecker_active)) {
		msleep(1);
	};

	start_idx = (atomic_read(&latencychecker_info_idx) + 1) & (MAX_LATENCYCHECKER_LOG - 1);
	total = (lc_info[start_idx].type == LATENCY_NONE) ? start_idx : MAX_LATENCYCHECKER_LOG;
	start_idx = (lc_info[start_idx].type == LATENCY_NONE) ? 0 : start_idx;

	if (total == 0)
		goto out;

	seq_puts(m, "\n");
	seq_printf(m, "Latency Checker Info (threshold %lums)\n", hardlatency_thresh);
	seq_puts(m, "\n");
	seq_puts(m, "--- Latency Info -------------------------------------------------------------------------\n");
	seq_puts(m, "      time       cpu        task_comm    where  detective            pc               fn\n");
	seq_puts(m, "------------------------------------------------------------------------------------------\n");

	for (i = start_idx; total > 0; total--, i = (i + 1) & (MAX_LATENCYCHECKER_LOG - 1))
		sec_perf_latencychecker_dump(m, i);

	seq_puts(m, "\n");

	start_idx = (atomic_read(&latencychecker_caller_idx) + 1) & (MAX_LATENCYCHECKER_CALLER - 1);

	if (lc_caller[start_idx].time == 0) {
		total = start_idx;
		start_idx = 0;
	} else {
		total = MAX_LATENCYCHECKER_CALLER;
	}

	if (total == 0)
		goto out;

	seq_puts(m, "--- Dump Stack ---------------------------------------------------------------------------\n");
	seq_puts(m, "      time       cpu        task_comm    caller\n");
	seq_puts(m, "------------------------------------------------------------------------------------------\n");

	for (i = start_idx; total > 0; total--, i = (i + 1) & (MAX_LATENCYCHECKER_CALLER - 1))
		sec_perf_latencychecker_dump_caller(m, i);

out:
	atomic_set(&latencychecker_dumping, 0);
	return 0;
}

static int sec_perf_latencychecker_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_perf_latencychecker_dump_show, NULL);
}

static const struct file_operations sec_perf_latencychecker_dump_fops = {
	.open    = sec_perf_latencychecker_dump_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int sec_perf_latencychecker_count_show(struct seq_file *m, void *v)
{
	unsigned long count = atomic_read(&latencychecker_info_idx) + 1;

	seq_printf(m, "%lu\n", count);
	return 0;
}

static int sec_perf_latencychecker_count_open(struct inode *inode, struct file *file)
{
	return single_open(file, sec_perf_latencychecker_count_show, NULL);
}

static const struct file_operations sec_perf_latencychecker_count_fops = {
	.open    = sec_perf_latencychecker_count_open,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init sec_perf_latencychecker_init(void)
{
	struct proc_dir_entry *proc_lc;
	int i;
	int ret;

	// proc
	proc_lc = proc_mkdir("latency_checker", NULL);
	if (!proc_lc)
		return -1;
	if (!proc_create("dump", 0644, proc_lc, &sec_perf_latencychecker_dump_fops))
		goto err;
	if (!proc_create("threshold", 0664, proc_lc, &sec_perf_latencychecker_threshold_fops))
		goto err;
	if (!proc_create("count", 0644, proc_lc, &sec_perf_latencychecker_count_fops))
		goto err;
	if (!proc_create("enable", 0664, proc_lc, &sec_perf_latencychecker_enable_fops))
		goto err;

	for_each_present_cpu(i) {
		if (i < nr_cpu_ids) {
			call_single_data_t *csd = per_cpu_ptr(&latencychecker_csd, i);

			csd->flags = 0;
			csd->func = latencychecker_save_stack_handler;
			csd->info = 0;
		}
	}

	/* register cpuhp state for hot plug(non-boot cores) */
	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "latencychecker:online",
				sec_perf_latencychecker_start_cpu, sec_perf_latencychecker_stop_cpu);

	if (ret < 0)
		goto err;

	init_complete = true;
	return 0;
err:
	remove_proc_subtree("latency_checker", NULL);
	return -1;
}

late_initcall(sec_perf_latencychecker_init);
