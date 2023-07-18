#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/ftrace.h>
#include <linux/kernel.h>
#include <linux/sched/clock.h>

#ifdef CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU
static int __read_mostly trace_ready;
static DEFINE_PER_CPU(unsigned int, irqsoff_panic_is_tracing);
static DEFINE_PER_CPU(unsigned long long, irqsoff_panic_timestamp);

static DEFINE_PER_CPU(unsigned long [6], irqsoff_panic_callip);
static DEFINE_PER_CPU(unsigned int, irqsoff_panic_calllvl);

extern int irqsoff_unwind_backtrace(unsigned long *ip);

void irqsoff_path_panic_msg(void)
{
	int cpu, i;
	unsigned long long interval, now;

	now = sched_clock();
	for_each_possible_cpu(cpu) {
		if (per_cpu(irqsoff_panic_is_tracing, cpu)) {
			interval = now - per_cpu(irqsoff_panic_timestamp, cpu);
			pr_emerg("cpu%d: %s interval = %lld\n", cpu, irqs_disabled() ? "irq off" : "irq on", interval);
			for (i = 0; i < per_cpu(irqsoff_panic_calllvl, cpu); i++)
				pr_emerg("irqsoff cpu%d: [<%08lx>] (%ps)\n", cpu,
					per_cpu(irqsoff_panic_callip, cpu)[i], (void *)per_cpu(irqsoff_panic_callip, cpu)[i]);
		}
	}
}

void notrace start_irqsoff_panic_timing(void)
{
	if (!irqs_disabled())
		return;

	if (current->pid == 0)
		return;

	if (unlikely(!trace_ready))
		return;

	if (__this_cpu_read(irqsoff_panic_is_tracing))
		return;

	if (__this_cpu_read(irqsoff_panic_timestamp))
		return;

	if (oops_in_progress)
		return;

	__this_cpu_write(irqsoff_panic_timestamp, sched_clock());
	__this_cpu_write(irqsoff_panic_calllvl,
		irqsoff_unwind_backtrace(this_cpu_ptr(irqsoff_panic_callip)));
	__this_cpu_write(irqsoff_panic_is_tracing, 1);
}

void notrace stop_irqsoff_panic_timing(void)
{
	if (!irqs_disabled())
		return;

	if (unlikely(!trace_ready))
		return;

	if (!__this_cpu_read(irqsoff_panic_is_tracing))
		return;

	__this_cpu_write(irqsoff_panic_is_tracing, 0);
	__this_cpu_write(irqsoff_panic_timestamp, 0);
	__this_cpu_write(irqsoff_panic_calllvl, 0);

}

static int __init trace_irqsoff_panic_init(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		per_cpu(irqsoff_panic_timestamp, cpu) = 0;
		per_cpu(irqsoff_panic_is_tracing, cpu) = 0;
		per_cpu(irqsoff_panic_calllvl, cpu) = 0;
	}

	trace_ready = 1;
	pr_alert("%s exit\n", __func__);
	return 0;
}

fs_initcall(trace_irqsoff_panic_init);

#else
void irqsoff_path_panic_msg(void) {}
#endif
EXPORT_SYMBOL(irqsoff_path_panic_msg);
