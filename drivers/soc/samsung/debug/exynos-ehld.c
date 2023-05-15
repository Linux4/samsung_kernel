/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Exynos - Early Hard Lockup Detector
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/smp.h>
#include <linux/errno.h>
#include <linux/suspend.h>
#include <linux/perf_event.h>
#include <linux/of.h>
#include <linux/cpu_pm.h>
#include <linux/sched/clock.h>
#include <linux/notifier.h>
#include <linux/kallsyms.h>
#include <linux/smpboot.h>
#include <linux/hrtimer.h>
#include <linux/debug-snapshot.h>
#include <asm/core_regs.h>
#include <asm/io.h>

#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-ehld.h>
#include <soc/samsung/exynos-adv-tracer-ehld.h>

//#define DEBUG
#define EHLD_TASK_SUPPORT

#ifdef DEBUG
#define ehld_info(f, str...) dev_info(ehld_main.dev, str)
#define ehld_err(f, str...) dev_info(ehld_main.dev, str)
#else
#define ehld_info(f, str...) if (f == 1) dev_info(ehld_main.dev, str)
#define ehld_err(f, str...) if (f == 1) dev_info(ehld_main.dev, str)
#endif

#define MSB_MASKING		(0x0000FF0000000000)
#define MSB_PADDING		(0xFFFFFF0000000000)
#define DBG_UNLOCK(base)	\
	do { isb(); __raw_writel(OSLOCK_MAGIC, base + DBGLAR); } while (0)
#define DBG_LOCK(base)		\
	do { __raw_writel(0x1, base + DBGLAR); isb(); } while (0)
#define DBG_OS_UNLOCK(base)	\
	do { isb(); __raw_writel(0, base + DBGOSLAR); } while (0)
#define DBG_OS_LOCK(base)	\
	do { __raw_writel(0x1, base + DBGOSLAR); isb(); } while (0)

#ifdef CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU
extern struct atomic_notifier_head hardlockup_notifier_list;
#endif
extern int register_reboot_notifier(struct notifier_block *nb);

struct exynos_ehld_dbgc {
	unsigned int			support;
	unsigned int			enabled;
	unsigned int			interval;
	unsigned int			warn_count;
	unsigned int			lockup_count;
	unsigned int			kill_count;
};

struct exynos_ehld_main {
	raw_spinlock_t			update_lock;
	raw_spinlock_t			policy_lock;
	unsigned int			cs_base;
	int				enabled;
	bool				suspending;
	bool				resuming;
	bool				sjtag;
	struct exynos_ehld_dbgc		dbgc;
	struct device			*dev;
};

struct exynos_ehld_main ehld_main = {
	.update_lock = __RAW_SPIN_LOCK_UNLOCKED(ehld_main.update_lock),
	.policy_lock = __RAW_SPIN_LOCK_UNLOCKED(ehld_main.policy_lock),
};

struct exynos_ehld_data {
	unsigned long long		time[NUM_TRACE];
	unsigned long long		event[NUM_TRACE];
	unsigned long long		pmpcsr[NUM_TRACE];
	unsigned long			data_ptr;
};

struct exynos_ehld_ctrl {
	struct task_struct		*task;
	struct hrtimer			hrtimer;
	struct perf_event		*event;
	struct exynos_ehld_data		data;
	void __iomem			*dbg_base;
	int				ehld_running;
	raw_spinlock_t			lock;
	bool				need_to_task;
	unsigned int			kill_count;
};

static DEFINE_PER_CPU(struct exynos_ehld_ctrl, ehld_ctrl) =
{
	.lock = __RAW_SPIN_LOCK_UNLOCKED(ehld_ctrl.lock),
};

static struct perf_event_attr exynos_ehld_attr = {
	.type           = PERF_TYPE_HARDWARE,
	.config         = PERF_COUNT_HW_INSTRUCTIONS,
	.size           = sizeof(struct perf_event_attr),
	.sample_period  = GENMASK_ULL(31, 0),
	.pinned         = 1,
	.disabled       = 1,
};

static void exynos_ehld_callback(struct perf_event *event,
			       struct perf_sample_data *data,
			       struct pt_regs *regs)
{
	event->hw.interrupts++;       /* throttle interrupts */
}

#define IPI_EHLD_KICK		(0xE)
void _exynos_ehld_do_policy(int cpu, unsigned int lockup_level)
{
	unsigned int val, i;
	struct exynos_ehld_ctrl *ctrl = per_cpu_ptr(&ehld_ctrl, cpu);

	switch (lockup_level) {
	case EHLD_STAT_LOCKUP_WARN:
		exynos_ehld_event_raw_dump(cpu);
		break;
	case EHLD_STAT_LOCKUP_SW:
	case EHLD_STAT_LOCKUP_HW:
		exynos_ehld_event_raw_dump(cpu);
		if (ctrl->kill_count > ehld_main.dbgc.kill_count) {
#ifdef CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU
			dbg_snapshot_set_hardlockup(true);
			atomic_notifier_call_chain(&hardlockup_notifier_list, 0, (void *)&cpu);
#endif
			for_each_possible_cpu(i) {
				val = dbg_snapshot_get_core_pmu_val(i);
				ehld_info(1, "%s: cpu%d: pmu_val:0x%x\n", __func__, i, val);
			}
			panic("Watchdog detected hard LOCKUP on cpu %u / kill_count - %u", cpu, ctrl->kill_count);
		}
		break;
	}
}

void exynos_ehld_do_policy(void)
{
	unsigned long flags;
	unsigned int cpu, val;
	unsigned int warn = 0,lockup_hw = 0,lockup_sw = 0;
	struct exynos_ehld_ctrl *ctrl;

	raw_spin_lock_irqsave(&ehld_main.policy_lock, flags);
	for_each_possible_cpu(cpu) {
		val = dbg_snapshot_get_core_ehld_stat(cpu);
		dbg_snapshot_set_core_ehld_stat(EHLD_STAT_NORMAL, cpu);
		if (val) {
			ehld_info(0, "%s: cpu%u: val:%x timer:%llx", __func__, cpu, val, arch_counter_get_cntvct());
			ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
			switch (val) {
			case EHLD_STAT_LOCKUP_WARN:
				warn |= (1 << cpu);
				break;
			case EHLD_STAT_LOCKUP_SW:
				lockup_sw |= (1 << cpu);
				ctrl->kill_count++;
				break;
			case EHLD_STAT_LOCKUP_HW:
				lockup_hw |= (1 << cpu);
				ctrl->kill_count++;
				break;
			default:
				break;
			}
		}
	}
	raw_spin_unlock_irqrestore(&ehld_main.policy_lock, flags);

	for_each_possible_cpu(cpu) {
		if (warn & (1 << cpu)) {
			ehld_info(1, "%s: cpu%u is hardlockup warnning", __func__, cpu);
			exynos_ehld_event_raw_update(cpu);
			_exynos_ehld_do_policy(cpu, EHLD_STAT_LOCKUP_WARN);
		}
		if (lockup_sw & (1 << cpu)) {
			exynos_ehld_event_raw_update(cpu);
			ehld_info(1, "%s: cpu%u is hardlockup by software", __func__, cpu);
			_exynos_ehld_do_policy(cpu, EHLD_STAT_LOCKUP_SW);
		}
		if (lockup_hw & (1 << cpu)) {
			exynos_ehld_event_raw_update(cpu);
			ehld_info(1, "%s: cpu%u is hardlockup by hardware", __func__, cpu);
			_exynos_ehld_do_policy(cpu, EHLD_STAT_LOCKUP_HW);
		}
	}
}

static void exynos_ehld_value_raw_update_pwroff(int cpu)
{
	dbg_snapshot_set_core_pmu_val(0xc2, cpu);
	dbg_snapshot_set_core_ehld_stat(EHLD_STAT_NORMAL, cpu);
}

void exynos_ehld_value_raw_update(int cpu)
{
	u32 val, val_en;

	if (ehld_main.dbgc.support && ehld_main.dbgc.enabled) {
		write_sysreg(0, pmselr_el0);
		isb();
		val = read_sysreg(pmxevcntr_el0);
		val_en = read_sysreg(pmcntenset_el0);
		dbg_snapshot_set_core_pmu_val(val, cpu);

		ehld_info(0, "%s: cpu%u: val:%x timer:%llx", __func__, cpu, val, arch_counter_get_cntvct());

		if (val_en == 0) {
			write_sysreg(1, pmcntenset_el0);
			isb();
		}
	}
}

void exynos_ehld_do_ipi(int cpu, unsigned int ipinr)
{
	struct exynos_ehld_ctrl *ctrl = per_cpu_ptr(&ehld_ctrl, cpu);

	switch (ipinr) {
	case IPI_EHLD_KICK:
		ehld_info(1, "%s: cpu%u: timer:%llx", __func__, cpu, arch_counter_get_cntvct());
		exynos_ehld_value_raw_update(cpu);
		exynos_ehld_event_raw_update(cpu);

		/* It's not Hardlockup what Available to get IPI */
		ctrl->kill_count = 0;
		break;
	}
}

static enum hrtimer_restart ehld_value_raw_hrtimer_fn(struct hrtimer *hrtimer)
{
	int cpu = raw_smp_processor_id();
	struct exynos_ehld_ctrl *ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
	struct perf_event *event = ctrl->event;

	if (!event || !ehld_main.dbgc.support) {
		ehld_err(1, "@%s: cpu%d, HRTIMER is cancel\n", __func__, cpu);
		return HRTIMER_NORESTART;
	}

	if (!ehld_main.dbgc.enabled) {
		ehld_err(1, "@%s: cpu%d, dbgc is not enabled, re-start\n", __func__, cpu);
		hrtimer_forward_now(hrtimer, ns_to_ktime(NSEC_PER_SEC * 10));
		return HRTIMER_RESTART;
	}

	if (event->state != PERF_EVENT_STATE_ACTIVE) {
		exynos_ehld_value_raw_update_pwroff(cpu);
		ehld_err(1, "@%s: cpu%d, event state is not active: %d\n", __func__, cpu, event->state);
		return HRTIMER_NORESTART;
	} else {
		exynos_ehld_value_raw_update(cpu);
		exynos_ehld_do_policy();
	}

	ehld_info(0, "@%s: cpu%d hrtimer is running\n", __func__, cpu);

	if (ehld_main.dbgc.interval > 0) {
		hrtimer_forward_now(hrtimer,
			ns_to_ktime(ehld_main.dbgc.interval * 1000 * 1000));
	} else {
		ehld_info(1, "@%s: cpu%d hrtimer interval is abnormal: %u\n",
				__func__, cpu, ehld_main.dbgc.interval);
		return HRTIMER_NORESTART;
	}

	return HRTIMER_RESTART;
}

static int exynos_ehld_start_cpu(unsigned int cpu)
{
	struct exynos_ehld_ctrl *ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
	struct perf_event *event = ctrl->event;
	struct hrtimer *hrtimer = &ctrl->hrtimer;

	if (!event) {
		event = perf_event_create_kernel_counter(&exynos_ehld_attr, cpu, NULL,
							 exynos_ehld_callback, NULL);
		if (IS_ERR(event)) {
			ehld_err(1, "@%s: cpu%d event make failed err: %ld\n",
							__func__, cpu, PTR_ERR(event));
			return PTR_ERR(event);
		} else {
			ehld_info(1, "@%s: cpu%d event make success\n", __func__, cpu);
		}
		ctrl->event = event;

		if (ehld_main.dbgc.support) {
			ehld_info(1, "@%s: cpu%d hrtimer start\n", __func__, cpu);
			hrtimer = &ctrl->hrtimer;
			hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			hrtimer->function = ehld_value_raw_hrtimer_fn;
			hrtimer_start(hrtimer,
					ns_to_ktime(ehld_main.dbgc.interval * 1000 * 1000),
					HRTIMER_MODE_REL_PINNED);
		}
	}

	if (event) {
		ehld_info(1, "@%s: cpu%d event enabled\n", __func__, cpu);
		perf_event_enable(event);
		ctrl->ehld_running = 1;
	}

	return 0;
}

static int exynos_ehld_stop_cpu(unsigned int cpu)
{
	struct exynos_ehld_ctrl *ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
	struct perf_event *event = ctrl->event;
	struct hrtimer *hrtimer = &ctrl->hrtimer;

	exynos_ehld_value_raw_update_pwroff(cpu);

	if (event) {
		ctrl->ehld_running = 0;
		perf_event_disable(event);
		perf_event_release_kernel(event);
		ctrl->event = NULL;

		ehld_info(1, "@%s: cpu%d event disabled\n", __func__, cpu);

		if (ehld_main.dbgc.support) {
			ehld_info(1, "@%s: cpu%d hrtimer cancel\n", __func__, cpu);
			hrtimer_cancel(hrtimer);
		}
	}

	return 0;
}

unsigned long long exynos_ehld_event_read_cpu(int cpu)
{
	struct exynos_ehld_ctrl *ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
	struct perf_event *event = ctrl->event;
	unsigned long long total = 0;
	unsigned long long enabled, running;

	if (!in_irq() && event) {
		total = perf_event_read_value(event, &enabled, &running);
		ehld_info(0, "%s: cpu%d - enabled: %llu, running: %llu, total: %llu\n",
				__func__, cpu, enabled, running, total);
	}
	return total;
}

void exynos_ehld_event_raw_update(int cpu)
{
	struct exynos_ehld_ctrl *ctrl = NULL;
	struct exynos_ehld_data *data = NULL;
	unsigned long long val = 0;
	unsigned long flags, count;

	raw_spin_lock_irqsave(&ehld_main.update_lock, flags);
	ctrl = per_cpu_ptr(&ehld_ctrl, cpu);

	if (ctrl && ctrl->event) {
		raw_spin_lock_irqsave(&ctrl->lock, flags);
		data = &ctrl->data;
		count = ++data->data_ptr & (NUM_TRACE - 1);
		data->time[count] = cpu_clock(cpu);
		if (cpu_is_offline(cpu) || !exynos_cpu.power_state(cpu) ||
			!ctrl->ehld_running) {
			ehld_info(0, "%s: cpu%d is turned off : running:%x, power:%x, offline:%ld\n",
				__func__, cpu, ctrl->ehld_running, exynos_cpu.power_state(cpu), cpu_is_offline(cpu));
			val = 0xC2;
			data->event[count] = val;
			data->pmpcsr[count] = 0;
		} else {
			ehld_info(0, "%s: cpu%d is turned on : running:%x, power:%x, offline:%ld\n",
				__func__, cpu, ctrl->ehld_running, exynos_cpu.power_state(cpu), cpu_is_offline(cpu));

			if (!ehld_main.sjtag) {
				DBG_UNLOCK(ctrl->dbg_base + PMU_OFFSET);
				val = __raw_readq(ctrl->dbg_base + PMU_OFFSET + PMUPCSR);
				if (MSB_MASKING == (MSB_MASKING & val))
					val |= MSB_PADDING;
				data->pmpcsr[count] = val;
				val = __raw_readl(ctrl->dbg_base + PMU_OFFSET);
				data->event[count] = val;
				DBG_LOCK(ctrl->dbg_base + PMU_OFFSET);
			} else {
				val = dbg_snapshot_get_core_pmu_val(cpu);
				data->event[count] = val;
				data->pmpcsr[count] = 0;
			}
		}
		raw_spin_unlock_irqrestore(&ctrl->lock, flags);
		ehld_info(0, "%s: cpu%x - time:%llu, event:0x%llx\n",
			__func__, cpu, data->time[count], data->event[count]);
	}
	raw_spin_unlock_irqrestore(&ehld_main.update_lock, flags);
}

void exynos_ehld_event_raw_update_allcpu(void)
{
	struct exynos_ehld_ctrl *ctrl = NULL;
	struct exynos_ehld_data *data = NULL;
	unsigned long long val = 0;
	unsigned long flags, count;
	unsigned int cpu;

	raw_spin_lock_irqsave(&ehld_main.update_lock, flags);
	for_each_possible_cpu(cpu) {
		ctrl = per_cpu_ptr(&ehld_ctrl, cpu);

		if (ctrl && ctrl->event) {
			raw_spin_lock_irqsave(&ctrl->lock, flags);
			data = &ctrl->data;
			count = ++data->data_ptr & (NUM_TRACE - 1);
			data->time[count] = cpu_clock(cpu);
			if (cpu_is_offline(cpu) || !exynos_cpu.power_state(cpu) ||
				!ctrl->ehld_running) {
				ehld_info(0, "%s: cpu%d is turned off : running:%x, power:%x, offline:%ld\n",
					__func__, cpu, ctrl->ehld_running, exynos_cpu.power_state(cpu), cpu_is_offline(cpu));
				val = 0xC2;
				data->event[count] = val;
				data->pmpcsr[count] = 0;
			} else {
				ehld_info(0, "%s: cpu%d is turned on : running:%x, power:%x, offline:%ld\n",
					__func__, cpu, ctrl->ehld_running, exynos_cpu.power_state(cpu), cpu_is_offline(cpu));

				if (!ehld_main.sjtag) {
					DBG_UNLOCK(ctrl->dbg_base + PMU_OFFSET);
					val = __raw_readq(ctrl->dbg_base + PMU_OFFSET + PMUPCSR);
					if (MSB_MASKING == (MSB_MASKING & val))
						val |= MSB_PADDING;
					data->pmpcsr[count] = val;
					val = __raw_readl(ctrl->dbg_base + PMU_OFFSET);
					data->event[count] = val;
					DBG_LOCK(ctrl->dbg_base + PMU_OFFSET);
				} else {
					val = dbg_snapshot_get_core_pmu_val(cpu);
					data->event[count] = val;
					data->pmpcsr[count] = 0;
				}
			}

			raw_spin_unlock_irqrestore(&ctrl->lock, flags);
			ehld_info(0, "%s: cpu%x - time:%llu, event:0x%llx\n",
				__func__, cpu, data->time[count], data->event[count]);
		}
	}
	raw_spin_unlock_irqrestore(&ehld_main.update_lock, flags);
}

void exynos_ehld_event_raw_dump(int cpu)
{
	struct exynos_ehld_ctrl *ctrl;
	struct exynos_ehld_data *data;
	unsigned long flags, count;
	int i;
	char symname[KSYM_NAME_LEN];

	symname[KSYM_NAME_LEN - 1] = '\0';
	raw_spin_lock_irqsave(&ehld_main.update_lock, flags);
	ehld_info(1, "--------------------------------------------------------------------------\n");
	ehld_info(1, "      Exynos Early Lockup Detector Information\n\n");
	ehld_info(1, "      CPU    NUM     TIME                 Value                PC\n\n");
	ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
	data = &ctrl->data;
	i = 0;
	do {
		count = ++data->data_ptr & (NUM_TRACE - 1);
		symname[KSYM_NAME_LEN - 1] = '\0';
		i++;

		if (data->pmpcsr[count] == 0 ||
			lookup_symbol_name(data->pmpcsr[count], symname) < 0)
			symname[0] = '\0';

		ehld_info(1, "      %03d    %03d     %015llu      0x%015llx      0x%016llx(%s)\n",
				cpu, i,
				(unsigned long long)data->time[count],
				(unsigned long long)data->event[count],
				(unsigned long long)data->pmpcsr[count],
				symname);

		if (i >= NUM_TRACE)
			break;
	} while (1);
	ehld_info(1, "--------------------------------------------------------------------------\n");
	raw_spin_unlock_irqrestore(&ehld_main.update_lock, flags);
}

void exynos_ehld_event_raw_dump_allcpu(void)
{
	struct exynos_ehld_ctrl *ctrl;
	struct exynos_ehld_data *data;
	unsigned long flags, count;
	int cpu, i;
	char symname[KSYM_NAME_LEN];

	symname[KSYM_NAME_LEN - 1] = '\0';
	raw_spin_lock_irqsave(&ehld_main.update_lock, flags);
	ehld_info(1, "--------------------------------------------------------------------------\n");
	ehld_info(1, "      Exynos Early Lockup Detector Information\n\n");
	ehld_info(1, "      CPU    NUM     TIME                 Value                PC\n\n");
	for_each_possible_cpu(cpu) {
		ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
		data = &ctrl->data;
		i = 0;
		do {
			count = ++data->data_ptr & (NUM_TRACE - 1);
			symname[KSYM_NAME_LEN - 1] = '\0';
			i++;

			if (lookup_symbol_name(data->pmpcsr[count], symname) < 0)
				symname[0] = '\0';

			ehld_info(1, "      %03d    %03d     %015llu      0x%015llx      0x%016llx(%s)\n",
					cpu, i,
					(unsigned long long)data->time[count],
					(unsigned long long)data->event[count],
					(unsigned long long)data->pmpcsr[count],
					symname);

			if (i >= NUM_TRACE)
				break;
		} while (1);
		ehld_info(1, "--------------------------------------------------------------------------\n");
	}
	raw_spin_unlock_irqrestore(&ehld_main.update_lock, flags);
}

static int exynos_ehld_cpu_online(unsigned int cpu)
{
	struct exynos_ehld_ctrl *ctrl;
	unsigned long flags;

	ctrl = per_cpu_ptr(&ehld_ctrl, cpu);

	raw_spin_lock_irqsave(&ctrl->lock, flags);
	ctrl->ehld_running = 1;
	ctrl->kill_count = 0;
	raw_spin_unlock_irqrestore(&ctrl->lock, flags);

	return 0;
}

static int exynos_ehld_cpu_predown(unsigned int cpu)
{
	struct exynos_ehld_ctrl *ctrl;
	unsigned long flags;

	ctrl = per_cpu_ptr(&ehld_ctrl, cpu);

	raw_spin_lock_irqsave(&ctrl->lock, flags);
	ctrl->ehld_running = 0;
	ctrl->kill_count = 0;
	raw_spin_unlock_irqrestore(&ctrl->lock, flags);

	return 0;
}

int exynos_ehld_start(void)
{
	int cpu;

	get_online_cpus();
	for_each_online_cpu(cpu)
		exynos_ehld_start_cpu(cpu);
	put_online_cpus();

	return 0;
}

void exynos_ehld_stop(void)
{
	int cpu;

	get_online_cpus();
	for_each_online_cpu(cpu)
		exynos_ehld_stop_cpu(cpu);
	put_online_cpus();
}

#ifdef EHLD_TASK_SUPPORT
static int ehld_task_should_run(unsigned int cpu)
{
	struct exynos_ehld_ctrl *ctrl;
	ctrl = per_cpu_ptr(&ehld_ctrl, cpu);

	ehld_info(0, "%s: CPU:%u, ehld_main.resuming: %x, ehld_main.suspending: %x\n",
			__func__, cpu, ehld_main.resuming, ehld_main.suspending);

	if (ehld_main.suspending && ctrl->need_to_task)
		return 1;
	else
		return 0;
}

static void ehld_task_run(unsigned int cpu)
{
	struct exynos_ehld_ctrl *ctrl;
	ctrl = per_cpu_ptr(&ehld_ctrl, cpu);

	ehld_info(0, "%s: CPU:%u, ehld_main.resuming: %x, ehld_main.suspending: %x\n",
			__func__, cpu, ehld_main.resuming, ehld_main.suspending);

	if (ehld_main.resuming)
		exynos_ehld_start_cpu(cpu);
	else
		exynos_ehld_stop_cpu(cpu);

	/* Done to progress of calling task */
	ctrl->need_to_task = false;
}

static void ehld_disable(unsigned int cpu)
{
	struct exynos_ehld_ctrl *ctrl;

	/*
	 * It tries to shutdown CPU0 of perf events
	 * smp_hotplug_thread don't support on CPU0
	 */
	if (cpu == 7) {
		/* This task shouldn't be processed in resuming */
		ctrl = per_cpu_ptr(&ehld_ctrl, 0);
		ehld_main.resuming = false;
		ctrl->need_to_task = true;
		wake_up_process(ctrl->task);
	}

	exynos_ehld_stop_cpu(cpu);
}

static void ehld_enable(unsigned int cpu)
{
	struct exynos_ehld_ctrl *ctrl;

	exynos_ehld_start_cpu(cpu);

	/*
	 * It tries to shutdown CPU0 of perf events
	 * smp_hotplug_thread don't support on CPU0
	 */
	if (cpu == 1) {
		/* This task should be processed in resuming */
		ctrl = per_cpu_ptr(&ehld_ctrl, 0);
		ehld_main.resuming = true;
		ctrl->need_to_task = true;
		wake_up_process(ctrl->task);
	}
}

static void ehld_cleanup(unsigned int cpu, bool online)
{
	ehld_disable(cpu);
}

static struct smp_hotplug_thread ehld_threads = {
	.store			= &ehld_ctrl.task,
	.thread_should_run	= ehld_task_should_run,
	.thread_fn		= ehld_task_run,
	.thread_comm		= "ehld_task/%u",
	.setup			= ehld_enable,
	.cleanup		= ehld_cleanup,
	.park			= ehld_disable,
	.unpark			= ehld_enable,
};
#else
static enum cpuhp_state hp_online;
void exynos_ehld_shutdown(void)
{
	struct perf_event *event;
	struct exynos_ehld_ctrl *ctrl;
	int cpu;

	cpuhp_remove_state(hp_online);

	for_each_possible_cpu(cpu) {
		ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
		event = ctrl->event;
		if (!event)
			continue;
		perf_event_disable(event);
		ctrl->event = NULL;
		perf_event_release_kernel(event);
	}
}
#endif

void exynos_ehld_prepare_panic(void)
{
	if (ehld_main.dbgc.support)
		ehld_main.dbgc.support = false;
}

#ifdef CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU
#define NUM_TRACE_HARDLOCKUP	(NUM_TRACE / 3)
extern struct atomic_notifier_head hardlockup_notifier_list;

static int exynos_ehld_hardlockup_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	int i;

	for (i = 0; i < NUM_TRACE_HARDLOCKUP; i++)
		exynos_ehld_event_raw_update_allcpu();

	exynos_ehld_event_raw_dump_allcpu();
	return 0;
}

static struct notifier_block exynos_ehld_hardlockup_block = {
	.notifier_call = exynos_ehld_hardlockup_handler,
};

static void register_hardlockup_notifier_list(void)
{
	atomic_notifier_chain_register(&hardlockup_notifier_list,
					&exynos_ehld_hardlockup_block);
}
#else
static void register_hardlockup_notifier_list(void) {}
#endif

static int exynos_ehld_reboot_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	if (ehld_main.dbgc.support) {
		adv_tracer_ehld_set_enable(false);
		ehld_main.dbgc.support = false;
	}
	return 0;
}

static struct notifier_block exynos_ehld_reboot_block = {
	.notifier_call = exynos_ehld_reboot_handler,
};

static int exynos_ehld_panic_handler(struct notifier_block *nb,
					   unsigned long l, void *p)
{
	int i;

	for (i = 0; i < NUM_TRACE_HARDLOCKUP; i++)
		exynos_ehld_event_raw_update_allcpu();

	exynos_ehld_event_raw_dump_allcpu();
	return 0;
}

static struct notifier_block exynos_ehld_panic_block = {
	.notifier_call = exynos_ehld_panic_handler,
};

static int exynos_ehld_c2_pm_notifier(struct notifier_block *self,
						unsigned long action, void *v)
{
	int cpu = raw_smp_processor_id();

	switch (action) {
	case CPU_PM_ENTER:
		exynos_ehld_value_raw_update_pwroff(cpu);
		exynos_ehld_cpu_predown(cpu);
		break;
        case CPU_PM_ENTER_FAILED:
        case CPU_PM_EXIT:
		exynos_ehld_cpu_online(cpu);
		exynos_ehld_value_raw_update(cpu);
		break;
	case CPU_CLUSTER_PM_ENTER:
		exynos_ehld_value_raw_update_pwroff(cpu);
		exynos_ehld_cpu_predown(cpu);
		break;
	case CPU_CLUSTER_PM_ENTER_FAILED:
	case CPU_CLUSTER_PM_EXIT:
		exynos_ehld_cpu_online(cpu);
		exynos_ehld_value_raw_update(cpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block exynos_ehld_c2_pm_nb = {
	.notifier_call = exynos_ehld_c2_pm_notifier,
};

static int exynos_ehld_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	int cpu;
	/*
	 * We should control re-init / exit for all CPUs
	 * Originally all CPUs are controlled by cpuhp framework.
	 * But CPU0 is not controlled by cpuhp framework in exynos BSP.
	 * So mainline code of perf(kernel/cpu.c) for CPU0 is not called by cpuhp framework.
	 * As a result, it's OK to not control CPU0.
	 * CPU0 will be controlled by CPU_PM notifier call.
	 */

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
		if (ehld_main.dbgc.support) {
			adv_tracer_ehld_set_enable(false);
			ehld_main.dbgc.enabled = false;
		}
		ehld_main.suspending = 1;
		break;

	case PM_POST_SUSPEND:
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
		ehld_main.suspending = 0;
		if (ehld_main.dbgc.support) {
			for_each_possible_cpu(cpu)
				exynos_ehld_value_raw_update_pwroff(cpu);
			adv_tracer_ehld_set_enable(true);
			ehld_main.dbgc.enabled = true;
		}
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_ehld_nb = {
	.notifier_call = exynos_ehld_pm_notifier,
};

static int exynos_ehld_init_dt_parse(struct device_node *np)
{
	struct device_node *child;
	int ret = 0, cpu = 0;
	unsigned int offset, base;
	char name[SZ_16];
	struct exynos_ehld_ctrl *ctrl;

	if (of_property_read_u32(np, "cs_base", &base)) {
		ehld_info(1, "exynos-ehld: no coresight base address in device tree\n");
		return -EINVAL;
	}
	ehld_main.cs_base = base;

#ifndef CONFIG_SOC_EXYNOS9830_EVT0
	child = of_get_child_by_name(np, "dbgc");
	if (child) {
		if (!of_property_read_u32(child, "support", &base)) {
			ehld_main.dbgc.support = base;
			ehld_info(1, "try to support %s dbgc\n", base ? "with" : "without");
			if (!of_property_read_u32(child, "interval", &base)) {
				ehld_main.dbgc.interval = base;
				ehld_info(1, "try to support dbgc interval:%u ms\n", base);
			}
			if (!of_property_read_u32(child, "warn-count", &base)) {
				ehld_main.dbgc.warn_count = base;
				ehld_info(1, "try to support dbgc warnning count: %u times\n", base);
			}
			if (!of_property_read_u32(child, "lockup-count", &base)) {
				ehld_main.dbgc.lockup_count = base;
				ehld_info(1, "try to support dbgc lockup count: %u times\n", base);
			}
			if (!of_property_read_u32(child, "kill-count", &base)) {
				ehld_main.dbgc.kill_count = base;
				ehld_info(1, "try to support dbgc kill count: %u times\n", base);
			}
		}
	}
#else
	ehld_main.dbgc.support = false;
#endif
	for_each_possible_cpu(cpu) {
		snprintf(name, sizeof(name), "cpu%d", cpu);
		child = of_get_child_by_name(np, (const char *)name);

		if (!child) {
			ehld_err(1, "exynos-ehld: device tree is not completed - cpu%d\n", cpu);
			return -EINVAL;
		}

		ret = of_property_read_u32(child, "dbg-offset", &offset);
		if (ret)
			return -EINVAL;

		ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
		ctrl->dbg_base = ioremap(ehld_main.cs_base + offset, SZ_256K);

		if (!ctrl->dbg_base) {
			ehld_err(1, "exynos-ehld: fail ioremap for dbg_base of cpu%d\n", cpu);
			return -ENOMEM;
		}
		ehld_info(1, "exynos-ehld: cpu#%d, cs_base:0x%x, dbg_base:0x%x, total:0x%x, ioremap:0x%lx\n",
				cpu, base, offset, ehld_main.cs_base + offset,
				(unsigned long)ctrl->dbg_base);
	}
	return ret;
}

static const struct of_device_id ehld_of_match[] __initconst = {
	{ .compatible	= "exynos-ehld",
	  .data		= exynos_ehld_init_dt_parse},
	{},
};

typedef int (*ehld_initcall_t)(const struct device_node *);
static int __init exynos_ehld_init_dt(void)
{
	struct device_node *np;
	const struct of_device_id *matched_np;
	ehld_initcall_t init_fn;

	np = of_find_matching_node_and_match(NULL, ehld_of_match, &matched_np);

	if (!np) {
		ehld_err(1, "exynos-ehld: couldn't find device tree file\n");
		return -ENODEV;
	}

	init_fn = (ehld_initcall_t)matched_np->data;
	return init_fn(np);
}

static int exynos_ehld_setup(void)
{
	struct exynos_ehld_ctrl *ctrl;
	int cpu;

	/* register pm notifier */
	register_pm_notifier(&exynos_ehld_nb);

	/* register cpu pm notifier for C2 */
	cpu_pm_register_notifier(&exynos_ehld_c2_pm_nb);

	register_hardlockup_notifier_list();

	register_reboot_notifier(&exynos_ehld_reboot_block);
	atomic_notifier_chain_register(&panic_notifier_list, &exynos_ehld_panic_block);

	ehld_main.sjtag = dbg_snapshot_get_sjtag_status();

	for_each_possible_cpu(cpu) {
		ctrl = per_cpu_ptr(&ehld_ctrl, cpu);
		memset((void *)&ctrl->data, 0, sizeof(struct exynos_ehld_data));
	}
#ifdef EHLD_TASK_SUPPORT
	return smpboot_register_percpu_thread(&ehld_threads);
#else
	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "exynos-ehld:online",
			exynos_ehld_start_cpu, exynos_ehld_stop_cpu);
	return 0;
#endif
}

int __init exynos_ehld_init(void)
{
	int err = 0;

	ehld_main.dev = create_empty_device();
	if (!ehld_main.dev) {
		panic("[EHLD]: fail to register empty device\n");
	} else {
		dev_set_socdata(ehld_main.dev, "Exynos", "EHLD");
	}

	err = exynos_ehld_init_dt();
	if (err) {
		ehld_err(1, "exynos-ehld: fail to process device tree for ehld:%d\n", err);
		return err;
	}

	err = exynos_ehld_setup();
	if (err) {
		ehld_err(1, "exynos-ehld: fail to process setup for ehld:%d\n", err);
		return err;
	}

	ehld_info(1, "exynos-ehld: success to initialize\n");

	return 0;
}
device_initcall_sync(exynos_ehld_init);

#ifndef CONFIG_SOC_EXYNOS9830_EVT0
int __init exynos_ehld_init_dbgc_enable(void)
{
	int val;

	if (ehld_main.dbgc.support) {
		adv_tracer_ehld_set_interval(ehld_main.dbgc.interval);
		adv_tracer_ehld_set_warn_count(ehld_main.dbgc.warn_count);
		adv_tracer_ehld_set_lockup_count(ehld_main.dbgc.lockup_count);

		adv_tracer_ehld_set_enable(true);
		val = adv_tracer_ehld_get_enable();
		if (val >= 0) {
			ehld_main.dbgc.enabled = val;
		} else {
			ehld_main.dbgc.enabled = false;
		}
	}
	return 0;
}
late_initcall(exynos_ehld_init_dbgc_enable)
#endif
