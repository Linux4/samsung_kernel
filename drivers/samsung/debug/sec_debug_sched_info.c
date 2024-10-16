// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list_sort.h>
#include <linux/sec_debug.h>
#include <soc/samsung/exynos/debug-snapshot.h>
#include <soc/samsung/exynos/debug-snapshot-log.h>

#include "../../../kernel/sched/sched.h"
#include "../../soc/samsung/exynos/debug/debug-snapshot-local.h"
#include "sec_debug_internal.h"

#include <trace/hooks/wqlockup.h>

#define PRINT_LINE_MAX	512
#define INFLIGHTWORKER 0x0200
#define CPU_MASK 0xff
#define MAX_TRACE_LOG 5

struct busy_info {
	struct list_head node;
	unsigned long long residency;
	struct task_struct *tsk;
	char task_comm[TASK_COMM_LEN];
	pid_t pid;
};

static DEFINE_PER_CPU(struct list_head, busy_info_list);
static DEFINE_PER_CPU(unsigned long long, real_duration);

#define is_valid_task(data) (data->pid == data->tsk->pid \
		&& !strcmp(data->task_comm, data->tsk->comm) \
		&& data->tsk->stack != 0)

static void secdbg_scin_show_sched_info(unsigned int cpu, int count)
{
	unsigned long long task_idx;
	ssize_t offset = 0;
	int max_count = dss_get_len_task_log_by_cpu(0);
	char buf[PRINT_LINE_MAX];
	struct dbg_snapshot_log_item *log_item = dbg_snapshot_log_get_item_by_index(DSS_LOG_TASK_ID);
	struct task_log *task;

	if (!log_item->entry.enabled)
		return;

	if (cpu >= NR_CPUS) {
		pr_warn("%s: invalid cpu %d\n", __func__, cpu);
		return;
	}

	if (count > max_count)
		count = max_count;

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "Sched info ");
	task_idx = dss_get_last_task_log_idx(cpu);

	for_each_item_in_dss_by_cpu(task, cpu, task_idx, count, false) {
		if (task->time == 0)
			break;
		offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "[%d]<", task->pid);
	}

	pr_auto(ASL5, "%s\n", buf);
}

static struct list_head *__create_busy_info(struct task_log *task, unsigned long long residency)
{
	struct busy_info *info;

	info = kzalloc(sizeof(struct busy_info), GFP_ATOMIC);
	if (!info)
		return NULL;

	info->pid = task->pid;
	info->tsk = task->task;
	strncpy(info->task_comm, task->task->comm, TASK_COMM_LEN - 1);
	info->residency = residency;

	return &info->node;
}

static int __residency_cmp(void *priv, const struct list_head *a, const struct list_head *b)
{
	struct busy_info *busy_info_a;
	struct busy_info *busy_info_b;

	busy_info_a = container_of(a, struct busy_info, node);
	busy_info_b = container_of(b, struct busy_info, node);

	if (busy_info_a->residency < busy_info_b->residency)
		return 1;
	else if (busy_info_a->residency > busy_info_b->residency)
		return -1;
	else
		return 0;
}

static void show_callstack(void *info)
{
	secdbg_stra_show_callstack_auto(NULL);
}

static bool __add_task_to_busy_info(unsigned int cpu, struct task_log *task, unsigned long long residency, struct list_head *busy_list)
{
	struct list_head *entry;
	struct busy_info *data;

	per_cpu(real_duration, cpu) += residency;

	list_for_each_entry(data, busy_list, node) {
		if (data->pid == task->pid && !strcmp(data->task_comm, task->task_comm)) {
			data->residency += residency;
			return true;
		}
	}

	entry = __create_busy_info(task, residency);

	if (!entry)
		return false;

	list_add(entry, busy_list);
	return true;
}

static bool secdbg_scin_make_busy_task_list(unsigned int cpu, unsigned long long duration, struct list_head *busy_list)
{
	long len = dss_get_len_task_log_by_cpu(0) - 1;
	long start = dss_get_last_task_log_idx(cpu);
	unsigned long long limit_time = local_clock() - duration * NSEC_PER_SEC;
	struct task_log *task, *next_task;
	bool is_busy = false;


	dbg_snapshot_set_item_enable("log_kevents", false);

	task = dss_get_task_log_by_idx(cpu, start);

	if (task && task->time != 0 && task->pid != 0 &&
		__add_task_to_busy_info(cpu, task, local_clock() - task->time, busy_list)) {

		is_busy = true;
		next_task = task;
		start = start > 0 ? (start - 1) : len;

		for_each_item_in_dss_by_cpu(task, cpu, start, len, false) {
			if (task->time != 0 && task->time <= next_task->time &&
				task->pid != 0 && __add_task_to_busy_info(cpu, task, next_task->time - task->time, busy_list)) {
				next_task = task;
			} else {
				is_busy = next_task->time < limit_time ? true : false;
				break;
			}
		}
	}

	dbg_snapshot_set_item_enable("log_kevents", true);

	return is_busy;
}

enum sched_class_type {
	SECDBG_SCHED_NONE,
	SECDBG_SCHED_IDLE,
	SECDBG_SCHED_FAIR,
	SECDBG_SCHED_RT,
	SECDBG_SCHED_DL,
	SECDBG_SCHED_STOP,
	SECDBG_SCHED_MAX
};

struct sched_class_info {
	int type;
	const struct sched_class *sclass;
	const char *symname;
};

struct sched_class_info sched_class_array[SECDBG_SCHED_MAX] = {
	[SECDBG_SCHED_IDLE] = {SECDBG_SCHED_IDLE, NULL, "idle_sched_class"},
	[SECDBG_SCHED_FAIR] = {SECDBG_SCHED_FAIR, NULL, "fair_sched_class"},
	[SECDBG_SCHED_RT] = {SECDBG_SCHED_RT, NULL, "rt_sched_class"},
	[SECDBG_SCHED_DL] = {SECDBG_SCHED_DL, NULL, "dl_sched_class"},
	[SECDBG_SCHED_STOP] = {SECDBG_SCHED_STOP, NULL, "stop_sched_class"},
};

static int get_current_sclass(void)
{
	int i;
	char sym[KSYM_SYMBOL_LEN] = {0,};

	snprintf(sym, KSYM_SYMBOL_LEN, "%ps", current->sched_class);

	for (i = SECDBG_SCHED_IDLE; i < SECDBG_SCHED_MAX; i++) {
		if (!strncmp(sched_class_array[i].symname, sym, KSYM_SYMBOL_LEN))
			return i;
	}

	return SECDBG_SCHED_NONE;
}

static const struct sched_class *__get_sclass_address(int type, int base_type,
					const struct sched_class *base_address)
{
	return base_address + (type - base_type);
}

static void calculate_address_sclass(int base_type,
					const struct sched_class *base_address)
{
	int i;

	for (i = SECDBG_SCHED_IDLE; i < SECDBG_SCHED_MAX; i++)
		sched_class_array[i].sclass =
			__get_sclass_address(i, base_type, base_address);
}

static void initialize_sched_class_array(void)
{
	int type = get_current_sclass();

	pr_info("%s: current type:%d\n", __func__, type);

	if (type != SECDBG_SCHED_NONE)
		calculate_address_sclass(type, current->sched_class);
}

static enum sched_class_type get_sched_class(struct task_struct *tsk)
{
	const struct sched_class *class = tsk->sched_class;
	int type = SECDBG_SCHED_NONE;

	for (type = SECDBG_SCHED_IDLE; type < SECDBG_SCHED_MAX; type++) {
		if (class == sched_class_array[type].sclass)
			return type;
	}

	return SECDBG_SCHED_NONE;
}

static int secdbg_scin_show_busy_task(unsigned int cpu, unsigned long long duration, int count)
{
	struct busy_info *info;
	struct task_struct *main_busy_tsk;
	ssize_t offset = 0;
	char buf[PRINT_LINE_MAX];
	struct dbg_snapshot_log_item *log_item = dbg_snapshot_log_get_item_by_index(DSS_LOG_TASK_ID);
	char sched_class_array[] = {'0', 'I', 'F', 'R', 'D', 'S'};
	int curr_cpu = smp_processor_id();
	struct list_head *list = per_cpu_ptr(&busy_info_list, cpu);
	bool is_busy;

	if (cpu >= NR_CPUS) {
		pr_warn("%s: invalid cpu %d\n", __func__, cpu);
		return -EINVAL;
	}

	/* This needs runqueues with EXPORT_SYMBOL */
	pr_auto(ASL5, "CPU%u [CFS util_avg:%lu load_avg:%lu nr_running:%u][RT rt_nr_running:%u][avg_rt util_avg:%lu]\n",
		cpu, cpu_rq(cpu)->cfs.avg.util_avg, cpu_rq(cpu)->cfs.avg.load_avg, cpu_rq(cpu)->cfs.nr_running,
		cpu_rq(cpu)->rt.rt_nr_running, cpu_rq(cpu)->avg_rt.util_avg);

	if (!log_item->entry.enabled)
		return -EINVAL;

	is_busy = secdbg_scin_make_busy_task_list(cpu, duration, list);

	if (!is_busy)
		return 0;

	if (list_empty(list))
		return -EINVAL;

	list_sort(NULL, list, __residency_cmp);

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "CPU Usage: PERIOD(%us)", (unsigned int)(per_cpu(real_duration, cpu) / NSEC_PER_SEC));

	list_for_each_entry(info, list, node) {
		unsigned int cpu_share = per_cpu(real_duration, cpu) > 0 ? (unsigned int)((info->residency * 100) / per_cpu(real_duration, cpu)) : 0;

		if (is_valid_task(info)) {
			offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset,
				" %s:%d[%c,%d](%u%%)", info->task_comm, info->pid,
				sched_class_array[get_sched_class(info->tsk)],
				info->tsk->prio, cpu_share);
		} else {
			offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset,
				" %s:%d[%c,%d](%u%%)", info->task_comm, info->pid,
				sched_class_array[0],
				-1, cpu_share);
		}
		if (--count == 0)
			break;
	}

	pr_auto(ASL5, "%s\n", buf);

	info = list_first_entry(list, struct busy_info, node);
	main_busy_tsk = info->tsk;

	if (main_busy_tsk->on_cpu && (main_busy_tsk->cpu == cpu)) {
		if (curr_cpu == cpu)
			dump_stack();
		else
			smp_call_function_single(cpu, show_callstack, NULL, 1);
	} else {
		secdbg_stra_show_callstack_auto(main_busy_tsk);
#if IS_ENABLED(CONFIG_SEC_DEBUG_SHOW_USER_STACK)
		secdbg_send_sig_debuggerd(main_busy_tsk, 2);
#endif
	}

	return 0;
}

static void secdbg_scin_show_inflight_worker(unsigned int cpu, struct task_struct *inflight_worker)
{
	int count;
	unsigned long task_idx;
	struct task_log *task;
	int log_count = 0;
	ssize_t offset = 0;
	char buf[PRINT_LINE_MAX];


	if (cpu < 0 || cpu >= NR_CPUS) {
		pr_warn("%s: invalid cpu %d\n", __func__, cpu);
		return;
	}

	dbg_snapshot_set_item_enable("log_kevents", false);

	offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, "Inflight Worker(%s:%d) Trace :", inflight_worker->comm, inflight_worker->pid);

	count = dss_get_len_task_log_by_cpu(0);
	task_idx = dss_get_last_task_log_idx(cpu);

	for_each_item_in_dss_by_cpu(task, cpu, task_idx, count, false) {
		if (log_count >= MAX_TRACE_LOG)
			break;
		if (task->task == inflight_worker) {
			log_count++;
			offset += scnprintf(buf + offset, PRINT_LINE_MAX - offset, " %llu", task->time);
		}
	}

	dbg_snapshot_set_item_enable("log_kevents", true);

	if (log_count)
		pr_auto(ASL5, "%s\n", buf);

}

static void secdbg_wqlockup_info_handler(void *ignore, int cpu, unsigned long pool_ts)
{
	if (cpu < 0)
		return;

	if (cpu & INFLIGHTWORKER) {
		secdbg_scin_show_inflight_worker(cpu & CPU_MASK, (struct task_struct *)pool_ts);
	} else {
		secdbg_scin_show_sched_info(cpu & CPU_MASK, 10);
		secdbg_scin_show_busy_task(cpu & CPU_MASK, jiffies_to_msecs(jiffies - pool_ts) / 1000, 5);
	}
}

static int __init secdbg_scin_init(void)
{
	int cpu;

	pr_info("%s: init\n", __func__);

	initialize_sched_class_array();

	for_each_possible_cpu(cpu) {
		INIT_LIST_HEAD(&per_cpu(busy_info_list, cpu));
	}

	if (IS_ENABLED(CONFIG_SEC_DEBUG_WQ_LOCKUP_INFO))
		register_trace_android_vh_wq_lockup_pool(secdbg_wqlockup_info_handler, NULL);

	return 0;
}
module_init(secdbg_scin_init);

static void __exit secdbg_scin_exit(void)
{
	if (IS_ENABLED(CONFIG_SEC_DEBUG_WQ_LOCKUP_INFO))
		unregister_trace_android_vh_wq_lockup_pool(secdbg_wqlockup_info_handler, NULL);
}
module_exit(secdbg_scin_exit);

MODULE_DESCRIPTION("Samsung Debug Sched info driver");
MODULE_LICENSE("GPL v2");
