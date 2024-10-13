// SPDX-License-Identifier: GPL-2.0
/*
 * linux/mm/mm_debug.c
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/ratelimit.h>
#include <trace/systrace_mark.h>

#define MB_TO_PAGES(x) ((x) << (20 - PAGE_SHIFT))
#define MIN_FILE_SIZE	300
#define K(x) ((x) << (PAGE_SHIFT-10))

noinline void tracing_mark_write(int type, const char *str)
{
	if (!tracing_is_on())
		return;

	switch (type) {
	case SYSTRACE_MARK_TYPE_BEGIN:
		trace_printk("B|%d|%s\n", current->tgid, str);
		break;
	case SYSTRACE_MARK_TYPE_END:
		trace_printk("E|%d|%s\n", current->tgid, str);
		break;
	default:
		break;
	}
}

static DEFINE_RATELIMIT_STATE(mm_debug_rs, 30 * HZ, 1);

static unsigned long mm_debug_count(struct shrinker *s,
				  struct shrink_control *sc)
{
	unsigned long inactive_file, active_file, file;

	if (!__ratelimit(&mm_debug_rs))
		goto out;

	inactive_file = global_node_page_state(NR_INACTIVE_FILE);
	active_file = global_node_page_state(NR_ACTIVE_FILE);
	file = inactive_file + active_file;
	if (file < MB_TO_PAGES(MIN_FILE_SIZE)) {
		pr_info("mm_debug for low file size %lukB\n", K(file));
		show_mem(0, NULL);
		dump_tasks(NULL, NULL);
	}

out:
	return 0; /* return 0 not to call to scan_objects */
}

static struct shrinker mm_debug_shrinker = {
	.count_objects = mm_debug_count,
};

static int __init mm_debug_init(void)
{
	register_shrinker(&mm_debug_shrinker);
	return 0;
}

static void __exit mm_debug_exit(void)
{
	unregister_shrinker(&mm_debug_shrinker);
}

module_init(mm_debug_init);
module_exit(mm_debug_exit);
