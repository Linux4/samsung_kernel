// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 *
 * Samsung debugging code
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <soc/samsung/exynos/exynos-ssld.h>
#include <linux/workqueue.h>
#include <linux/sec_debug.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>
#include <linux/hashtable.h>
#include <linux/sec_debug.h>
#include "sec_debug_internal.h"
#include "../../../kernel/workqueue_internal.h"

#define MAX_ARRAY 20
#define MAX_CANDIDATE 2

#ifdef MODULE
static void __show_callstack(struct task_struct *task)
{
	secdbg_stra_show_callstack_auto(task);
}
#else /* MODULE */
static void __show_callstack(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_SEC_DEBUG_AUTO_COMMENT)
	show_stack_auto_comment(task, NULL);
#else
	show_stack(task, NULL);
#endif
}
#endif /* MODULE */


static bool secdbg_ssld_is_related_worker(struct task_struct *tsk, work_func_t curr_func)
{
	char symname[KSYM_NAME_LEN];
	unsigned long wchan;

	if (!tsk)
		return false;

	snprintf(symname, KSYM_NAME_LEN, "%ps", curr_func);

	if (!strstr(symname, "async_run_entry_fn"))
		return false;

	wchan = get_wchan(tsk);

	snprintf(symname, KSYM_NAME_LEN, "%ps", (void *)wchan);

	if (strstr(symname, "dpm_wait_for_superior"))
		return false;

	return true;
}

static void secdbg_ssld_find_candidate(struct task_struct *candidate[], int *candidate_count)
{
	int i, count = 0;
	struct task_struct *tsk_array[MAX_ARRAY];
	work_func_t curr_func[MAX_ARRAY];

	count = find_in_flight_task_in_one_workqueue(system_unbound_wq, tsk_array, curr_func, MAX_ARRAY);

	for (i = 0; i < count; i++) {
		if (!secdbg_ssld_is_related_worker(tsk_array[i], curr_func[i]))
			continue;

		candidate[*candidate_count] = tsk_array[i];
		if (++(*candidate_count) == MAX_CANDIDATE)
			return;
	}
}

static int secdbg_ssld_info_handler(struct notifier_block *nb,
				   unsigned long l, void *arg)
{
	struct ssld_notifier_data *nb_data = (struct ssld_notifier_data *)arg;
	struct s2r_trace_info *info = nb_data->last_info;
	struct task_struct *candidate[MAX_CANDIDATE];
	int candidate_count = 0;
	int i;

	if (!info->start) {
		secdbg_ssld_find_candidate(candidate, &candidate_count);
		for (i = 0; i < candidate_count; i++) {
			pr_auto(ASL9, "suspected task : %s\n", candidate[i]->comm);
			__show_callstack(candidate[i]);
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block nb_ssld_block = {
	.notifier_call = secdbg_ssld_info_handler,
};

static int __init secdbg_ssld_info_init(void)
{
	pr_info("%s: init\n", __func__);
	ssld_notifier_chain_register(&nb_ssld_block);

	return 0;
}
module_init(secdbg_ssld_info_init);

static void __exit secdbg_ssld_info_exit(void)
{
}
module_exit(secdbg_ssld_info_exit);

MODULE_DESCRIPTION("Samsung Debug Sched report driver");
MODULE_LICENSE("GPL v2");
