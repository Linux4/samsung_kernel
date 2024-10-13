#define pr_fmt(fmt) "sprd_dbg:irqoff: " fmt
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/ftrace.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>

#define DEFAULT_WARNING_INTERVAL (30*NSEC_PER_MSEC)

static DEFINE_PER_CPU(unsigned long long, eirqsoff_start_timestamp);
static DEFINE_PER_CPU(unsigned int, cpu_is_tracing);
static unsigned long long __read_mostly warning_interval;
static int __read_mostly trace_ready;

void notrace start_eirqsoff_timing(void)
{
	if (!irqs_disabled())
		return;

	if (current->pid == 0)
		return;

	if (__this_cpu_read(cpu_is_tracing))
		return;

	if (__this_cpu_read(eirqsoff_start_timestamp))
		return;

	__this_cpu_write(eirqsoff_start_timestamp, sched_clock());

	__this_cpu_write(cpu_is_tracing, 1);
}
void notrace stop_eirqsoff_timing(void)
{
	unsigned long long stop_timestamp;
	long long interval;

	if (!irqs_disabled())
		return;

	if (unlikely(!trace_ready))
		return;

	if (!__this_cpu_read(cpu_is_tracing))
		return;

	__this_cpu_write(cpu_is_tracing, 0);

	stop_timestamp = sched_clock();

	interval = stop_timestamp - __this_cpu_read(eirqsoff_start_timestamp);

	if (interval > warning_interval) {
		pr_crit("stop=%lld start=%lld pid=%d\n",
			  stop_timestamp,
			  __this_cpu_read(eirqsoff_start_timestamp),
			  current->pid);
		dump_stack();
	}

	__this_cpu_write(eirqsoff_start_timestamp, 0);
}


static int notrace eirqsoff_interval_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%lld\n", warning_interval);
	return 0;
}

static int notrace eirqsoff_interval_open(struct inode *inodep,
					  struct file *filep)
{
	single_open(filep, eirqsoff_interval_show, NULL);
	return 0;
}

static ssize_t notrace eirqsoff_interval_write(struct file *filep,
					       const char __user *buf,
					       size_t len,
					       loff_t *ppos)
{
	unsigned long long interval;
	int err;

	if (len <= 5 || len >= 11)
		return -EINVAL;

	err = kstrtoull_from_user(buf, len, 0, &interval);

	if (err)
		return -EINVAL;

	warning_interval = interval;

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
	int cpu;

	for_each_possible_cpu(cpu) {
		per_cpu(eirqsoff_start_timestamp, cpu) = 0;
		per_cpu(cpu_is_tracing, cpu) = 0;
	}
	warning_interval = DEFAULT_WARNING_INTERVAL;
	trace_ready = 1;
	return 0;
}

early_initcall(trace_eirqsoff_init);
