// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 */

#define pr_fmt(fmt) "[BSP] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/list_sort.h>
#include <linux/sec_debug.h>
#include <linux/hashtable.h>
#include <linux/ftrace.h>
#include "sec_debug_internal.h"
#include <soc/samsung/exynos/debug-snapshot.h>
#include <soc/samsung/exynos/debug-snapshot-log.h>
#include <linux/sched/prio.h>
#include <linux/panic_notifier.h>
#include "../../../kernel/sched/sched.h"
#include <trace/events/irq.h>
#include <linux/string.h>
#include <asm-generic/hardirq.h>
#include <linux/notifier.h>
#include "sec_debug_bspinfo.h"

#define TASKLET_DEBUG		0

#define LOG_MAX_NUM			100
#define NUM_OF_PRINT_ITEMS	10
#define BUCKET_SIZE			6

#define decrement_by_n(a, b)	\
	(((a)-(b) < 0)?LOG_MAX_NUM - ((b)-(a)):((a)-(b)))
#define increment_by_n(a, b)	\
	(((a)+(b) >= LOG_MAX_NUM)?((a)+(b) - LOG_MAX_NUM):((a)+(b)))


#define secdbg_pr(dst, f, ...) \
	(dst != NULL ? seq_printf(dst, f, ##__VA_ARGS__) : pr_info(f, ##__VA_ARGS__))

#if TASKLET_DEBUG
enum log_flag {
	FLAG_IN			= 1,
	FLAG_ON			= 2,
	FLAG_OUT		= 3,
};

struct tasklet_log_item {
	unsigned long long time;
	void *callback;
	int en;
};

struct tasklet_log {
	struct hlist_node hlist;
	void *callback;
	unsigned long count;
};

static DECLARE_HASHTABLE(tasklet_log_hash[CONFIG_VENDOR_NR_CPUS], BUCKET_SIZE);

static unsigned long total_tasklet_log_count[CONFIG_VENDOR_NR_CPUS];
static struct tasklet_log_item logs[CONFIG_VENDOR_NR_CPUS][LOG_MAX_NUM];
static int log_item_idx[CONFIG_VENDOR_NR_CPUS];

static void secdbg_softirq_pr_tasklet_stats(struct seq_file *dst);
static inline void secdbg_softirq_log_tasklet(void *callback, int flag);
static void secdbg_softirq_pr_tasklet_stats_on_cpu(struct seq_file *dst, int cpu);
static void secdbg_softirq_pr_tasklet_log_on_cpu(struct seq_file *dst, int cpu);


static inline struct tasklet_log *secdbg_softirq_search_cb_on_cpu(void *callback, int cpu)
{
	struct tasklet_log *el;

	hash_for_each_possible(tasklet_log_hash[cpu], el, hlist, (unsigned long)callback) {
		if (el->callback == callback)
			return el;
	}

	return NULL;
}

static inline void secdbg_softirq_count_tasklet(void *callback)
{
	struct tasklet_log *log_item, *new_item;
	int cpu = raw_smp_processor_id();

	new_item = NULL;
	log_item = secdbg_softirq_search_cb_on_cpu(callback, cpu);

	if (log_item != NULL) {
		log_item->count++;
	} else {
		new_item = kmalloc(
			sizeof(struct tasklet_log),
			GFP_NOWAIT | __GFP_NOWARN | __GFP_NORETRY);

		if (!new_item)
			return;
		new_item->count = 1;
		new_item->callback = callback;
		hash_add(tasklet_log_hash[cpu], &new_item->hlist, (unsigned long)callback);
	}
	total_tasklet_log_count[cpu]++;
}

static void secdbg_softirq_pr_tasklet_stats(struct seq_file *dst)
{
	unsigned long flags;
	int i;

	secdbg_pr(dst, "all tasklet stats ==================\n");
	local_irq_save(flags);

	for (i = 0; i < CONFIG_VENDOR_NR_CPUS; i++)
		secdbg_softirq_pr_tasklet_stats_on_cpu(dst, i);

	local_irq_restore(flags);
}

static inline char *secdbg_softirq_get_func_name(char *buf, int buf_size, void *funcp)
{
	char *idx = buf;
	char *tok;

	snprintf(buf, buf_size, "%ps", funcp);
	tok = strsep(&idx, ".");

	return tok;
}

static void secdbg_softirq_pr_tasklet_stats_on_cpu(struct seq_file *dst, int cpu)
{
	struct tasklet_log *pos;
	char buf[200];
	int j;

	secdbg_pr(dst, "core %d stats\n", cpu);
	secdbg_pr(dst, "%30s\t %5s\n", "func", "count");
	hash_for_each(tasklet_log_hash[cpu], j, pos, hlist) {
		secdbg_pr(dst, "%30s\t %5lu\n",
			secdbg_softirq_get_func_name(buf, sizeof(buf), pos->callback),
			pos->count);
	}
}

static void secdbg_softirq_trace_tasklet_entry(void *unused, void *callback)
{
	unsigned long flags;

	local_irq_save(flags);
	secdbg_softirq_log_tasklet(callback, FLAG_IN);
	local_irq_restore(flags);
}

static void secdbg_softirq_trace_tasklet_exit(void *unused, void *callback)
{
	unsigned long flags;

	local_irq_save(flags);
	secdbg_softirq_log_tasklet(callback, FLAG_OUT);
	secdbg_softirq_count_tasklet(callback);
	local_irq_restore(flags);
}

static inline void secdbg_softirq_log_tasklet(void *callback, int flag)
{
	int idx;
	struct tasklet_log_item *item;
	int cpu = raw_smp_processor_id();

	idx = log_item_idx[cpu];
	log_item_idx[cpu] = increment_by_n(log_item_idx[cpu], 1);
	item = &logs[cpu][idx];

	item->time = local_clock();
	item->callback = callback;
	item->en = flag;
}

static void secdbg_softirq_pr_tasklet_log(struct seq_file *dst)
{
	int cpu;

	secdbg_pr(dst, "all tasklet log latest %d tasklets\n", NUM_OF_PRINT_ITEMS);
	for (cpu = 0; cpu < CONFIG_VENDOR_NR_CPUS; cpu++)
		secdbg_softirq_pr_tasklet_log_on_cpu(dst, cpu);

}

static void secdbg_softirq_pr_tasklet_log_on_cpu(struct seq_file *dst, int cpu)
{
	char buf[200];
	int idx, i;
	struct tasklet_log_item *item;

	idx = decrement_by_n(log_item_idx[cpu], NUM_OF_PRINT_ITEMS);

	secdbg_pr(dst, "core %d\n", cpu);
	secdbg_pr(dst, "%15s\t%30s\t%5s\n", "time", "func", "state");

	for (i = 0; i < NUM_OF_PRINT_ITEMS; i++) {
		item = &logs[cpu][idx];
		secdbg_pr(dst, "%15llu\t%30s\t%5d\n",
			item->time,
			secdbg_softirq_get_func_name(buf, sizeof(buf), item->callback),
			item->en);
		idx = increment_by_n(idx, 1);
	}

}

static void secdbg_softirq_init_hash(void)
{
	int i;

	for (i = 0; i < CONFIG_VENDOR_NR_CPUS; i++)
		hash_init(tasklet_log_hash[i]);

}
#endif

static void secdbg_softirq_pr_softirq_status(struct seq_file *dst)
{
	int cpu;

	secdbg_pr(dst, "softirq pending status ==================\n");
	for (cpu = 0; cpu < CONFIG_VENDOR_NR_CPUS; cpu++) {
		unsigned int pending;

		pending = *per_cpu_ptr(&irq_stat.__softirq_pending, cpu);
		secdbg_pr(dst, "core:%d 0x%08x\n", cpu, pending);
	}
}

static int secdbg_softirq_softirq_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	secdbg_softirq_pr_softirq_status(NULL);
#if TASKLET_DEBUG
	secdbg_softirq_pr_tasklet_stats(NULL);
	secdbg_softirq_pr_tasklet_log(NULL);
#endif
	return NOTIFY_DONE;
}

static struct notifier_block nb_panic_block = {
	.notifier_call = secdbg_softirq_softirq_panic_handler,
};

#if IS_ENABLED(CONFIG_SEC_DEBUG_BSPINFO)
static int callback_softirq(struct seq_file *dst)
{
	secdbg_softirq_pr_softirq_status(dst);
#if TASKLET_DEBUG
	secdbg_softirq_pr_tasklet_stats(dst);
	secdbg_softirq_pr_tasklet_log(dst);
#endif

	return NOTIFY_DONE;
}

BSP_INFO_DISPLAY_FN(softirq, "Softirq information", callback_softirq);
#endif

static int __init secdbg_softirq_init(void)
{
	int ret;

	pr_info("%s: init\n", __func__);

	ret = atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);
	if (ret) {
		pr_info("%s: failed to register panic notifier\n", __func__);
		return 0;
	}

#if TASKLET_DEBUG
	secdbg_softirq_init_hash();


	ret = register_trace_tasklet_entry(secdbg_softirq_trace_tasklet_entry, NULL);
	if (ret) {
		pr_info("%s: failed to register_trace_tasklet_entry\n", __func__);
		return 0;
	}

	ret = register_trace_tasklet_exit(secdbg_softirq_trace_tasklet_exit, NULL);
	if (ret) {
		pr_info("%s: failed to register_trace_tasklet_exit\n", __func__);
		return 0;
	}
#endif

#if IS_ENABLED(CONFIG_SEC_DEBUG_BSPINFO)
	ret = secdbg_bspinfo_register_info_displayer(&softirq_display_notifier);
	if (ret) {
		pr_info("%s: failed to register softirq_display_notifier\n", __func__);
		return 0;
	}
#endif

	return 0;
}
late_initcall_sync(secdbg_softirq_init);

static void __exit secdbg_softirq_exit(void)
{
	atomic_notifier_chain_unregister(&panic_notifier_list, &nb_panic_block);
}
module_exit(secdbg_softirq_exit);

MODULE_DESCRIPTION("Samsung Debug BSP Info driver");
MODULE_LICENSE("GPL v2");
#if IS_ENABLED(CONFIG_SEC_DEBUG_BSPINFO)
MODULE_SOFTDEP("pre: secdbg-bspinfo");
#endif
