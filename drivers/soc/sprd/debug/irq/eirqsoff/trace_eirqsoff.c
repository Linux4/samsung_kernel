#define pr_fmt(fmt) "sprd_eirqsoff warning: " fmt

#include <linux/debugfs.h>
#include <linux/ftrace.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/uaccess.h>
#include "../../sprd_debugfs.h"

#define DEFAULT_WARNING_INTERVAL (30*NSEC_PER_MSEC)

extern void show_stack(struct task_struct *task, unsigned long *sp);

static u64 __read_mostly warning_interval = DEFAULT_WARNING_INTERVAL;

static DEFINE_PER_CPU(u64, eirqsoff_start_timestamp);
static DEFINE_PER_CPU(u64, eirqsoff_pause_timestamp);
static DEFINE_PER_CPU(u64, eirqsoff_paused_interval);

static DEFINE_PER_CPU(pid_t, eirqsoff_pid);
static DEFINE_PER_CPU(unsigned long, eirqsoff_ip);
static DEFINE_PER_CPU(unsigned long, eirqsoff_parent_ip);

#ifdef CONFIG_PREEMPT_TRACER
static u64 __read_mostly epreempt_interval = DEFAULT_WARNING_INTERVAL<<2;

static DEFINE_PER_CPU(unsigned int, epreempt_is_tracing);
static DEFINE_PER_CPU(u64, epreempt_start_timestamp);
static DEFINE_PER_CPU(u64, epreempt_paused_interval);
static DEFINE_PER_CPU(u64, epreempt_pause_timestamp);

static DEFINE_PER_CPU(pid_t, epreempt_pid);
static DEFINE_PER_CPU(unsigned long, epreempt_ip);
static DEFINE_PER_CPU(unsigned long, epreempt_parent_ip);
#endif

static void notrace clear_eirqsoff_data(void)
{
	__this_cpu_write(eirqsoff_pid, 0);
	__this_cpu_write(eirqsoff_ip, 0);
	__this_cpu_write(eirqsoff_parent_ip, 0);
	__this_cpu_write(eirqsoff_start_timestamp, 0);
	__this_cpu_write(eirqsoff_pause_timestamp, 0);
	__this_cpu_write(eirqsoff_paused_interval, 0);
}

static void notrace show_eirqsoff_info(u64 start_time, u64 interval)
{
	u64 start_timestamp_ms = do_div(start_time, NSEC_PER_SEC);
	u64 interval_us = do_div(interval, NSEC_PER_MSEC);

	pr_warn("Detected Process %d disable interrupt %lld.%06lldms "
		"from %lld.%09llds\n",
		__this_cpu_read(eirqsoff_pid),
		interval, interval_us,
		start_time, start_timestamp_ms);

	pr_warn("disable at:\n");
	print_ip_sym(__this_cpu_read(eirqsoff_parent_ip));
	print_ip_sym(__this_cpu_read(eirqsoff_ip));
	show_stack(NULL, NULL);
}

void notrace start_eirqsoff_timing(unsigned long ip, unsigned long parent_ip)
{
	if (!irqs_disabled())
		/* Should never into here! */
		return;

	/* Don't trace idle thread */
	if (current->pid == 0)
		return;

	if (__this_cpu_read(eirqsoff_start_timestamp))
		return;

	if (oops_in_progress)
		return;

	__this_cpu_write(eirqsoff_pid, current->pid);
	__this_cpu_write(eirqsoff_ip, ip);
	__this_cpu_write(eirqsoff_parent_ip, parent_ip);
	__this_cpu_write(eirqsoff_start_timestamp, sched_clock());
}

void notrace pause_eirqsoff_timing(void)
{
	if (!__this_cpu_read(eirqsoff_start_timestamp))
		return;

	__this_cpu_write(eirqsoff_pause_timestamp, sched_clock());
}

void notrace continue_eirqsoff_timing(void)
{
	u64 pause_ts = __this_cpu_read(eirqsoff_pause_timestamp);

	if (!__this_cpu_read(eirqsoff_start_timestamp) || !pause_ts)
		return;

	__this_cpu_add(eirqsoff_paused_interval, sched_clock() - pause_ts);
}

void notrace stop_eirqsoff_timing(unsigned long ip, unsigned long parent_ip)
{
	u64 start_timestamp, stop_timestamp;
	u64 interval;

	if (!irqs_disabled())
		return;

	if (!__this_cpu_read(eirqsoff_start_timestamp)) {
		clear_eirqsoff_data();
		return;
	}

	if (!oops_in_progress) {

		stop_timestamp = sched_clock();
		start_timestamp = __this_cpu_read(eirqsoff_start_timestamp);

		interval = stop_timestamp - start_timestamp;
		interval -= __this_cpu_read(eirqsoff_paused_interval);

		if (interval > warning_interval) {
			show_eirqsoff_info(start_timestamp, interval);
		}
	}

	clear_eirqsoff_data();
}


#ifdef CONFIG_PREEMPT_TRACER
static void notrace clear_epreempt_data(void)
{
	__this_cpu_write(epreempt_pid, 0);
	__this_cpu_write(epreempt_ip, 0);
	__this_cpu_write(epreempt_parent_ip, 0);
	__this_cpu_write(epreempt_start_timestamp, 0);
	__this_cpu_write(epreempt_pause_timestamp, 0);
	__this_cpu_write(epreempt_paused_interval, 0);
}

static void notrace show_epreempt_info(u64 start_time, u64 interval)
{
	u64 start_timestamp_ms = do_div(start_time, NSEC_PER_SEC);
	u64 interval_us = do_div(interval, NSEC_PER_MSEC);

	pr_warn("Detected Process %d disable preempt %lld.%06lldms "
		"from %lld.%09llds\n",
		__this_cpu_read(epreempt_pid),
		interval, interval_us,
		start_time, start_timestamp_ms);

	pr_warn("disable at:\n");
	print_ip_sym(__this_cpu_read(epreempt_parent_ip));
	print_ip_sym(__this_cpu_read(epreempt_ip));
	show_stack(NULL, NULL);
}

void notrace start_epreempt_timing(unsigned long ip, unsigned long parent_ip)
{
	if (current->pid == 0)
		return;

	if (__this_cpu_read(epreempt_start_timestamp))
		return;

	if (oops_in_progress)
		return;

	__this_cpu_write(epreempt_pid, current->pid);
	__this_cpu_write(epreempt_ip, ip);
	__this_cpu_write(epreempt_parent_ip, parent_ip);
	__this_cpu_write(epreempt_start_timestamp, sched_clock());
}

void notrace pause_epreempt_timing(void)
{
	if (!__this_cpu_read(epreempt_start_timestamp))
		return;

	__this_cpu_write(epreempt_pause_timestamp, sched_clock());
}

void notrace continue_epreempt_timing(void)
{
	u64 pause_ts = __this_cpu_read(epreempt_pause_timestamp);

	if (!__this_cpu_read(epreempt_start_timestamp) || !pause_ts)
		return;

	__this_cpu_add(epreempt_paused_interval, sched_clock() - pause_ts);
}

void notrace stop_epreempt_timing(unsigned long ip, unsigned long parent_ip)
{
	u64 start_timestamp, stop_timestamp;
	u64 interval;

	if (!__this_cpu_read(epreempt_start_timestamp)) {
		clear_epreempt_data();
		return;
	}

	__this_cpu_write(epreempt_is_tracing, 0);

	if (!oops_in_progress) {
		stop_timestamp = sched_clock();
		start_timestamp = __this_cpu_read(epreempt_start_timestamp);

		interval = stop_timestamp - start_timestamp;
		interval -= __this_cpu_read(epreempt_paused_interval);

		if (interval > epreempt_interval) {
			show_epreempt_info(start_timestamp, interval);
		}
	}

	clear_epreempt_data();
}

#endif

static int notrace eirqsoff_interval_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%lld\n", warning_interval);
	return 0;
}

static int
notrace eirqsoff_interval_open(struct inode *inodep, struct file *filep)
{
	single_open(filep, eirqsoff_interval_show, NULL);
	return 0;
}

static ssize_t
notrace eirqsoff_interval_write(struct file *filep, const char __user *buf,
				size_t len, loff_t *ppos)
{
	u64 interval;
	int err;

	if (len <= 5 || len >= 11)
		return -EINVAL;

	err = kstrtoull_from_user(buf, len, 0, &interval);

	if (err)
		return -EINVAL;

	warning_interval = interval;
#ifdef CONFIG_PREEMPT_TRACER
	epreempt_interval = interval<<2;
#endif

	return len;
}

const struct file_operations eirqsoff_interval_fops = {
	.open    = eirqsoff_interval_open,
	.read    = seq_read,
	.write   = eirqsoff_interval_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int __init trace_eirqsoff_init(void)
{
	debugfs_create_file("warning_interval", 0644, sprd_debugfs_entry(IRQ),
			    NULL, &eirqsoff_interval_fops);
	return 0;
}
fs_initcall(trace_eirqsoff_init);
