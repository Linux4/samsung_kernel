/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#pragma once

#include <linux/miscdevice.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>

#define DEFAULT_EVAL_WORKQUEUE_TICK_MS		(1000)

enum hts_req_list {
	REQ_NONE = -1,
	REQ_SYSBUSY,
	REQ_LAUNCH
};

struct hts_app_event_data {
	struct hts_data		*data;
	struct plist_node	req;
};

struct hts_devfs {
	struct file_operations		fops;
	struct miscdevice		miscdev;
};

struct hts_reg_data {
	cpumask_t			support_cpu;
};

#if IS_ENABLED(CONFIG_HTS_DEBUG)
struct hts_debug_data {
	int				pid;
	int				idx;
};
#endif

struct hts_data {
	struct platform_device		*pdev;
	struct hts_devfs		devfs;

	int				probed;
	int				enabled_count;

	struct workqueue_struct		*hts_wq;
	struct delayed_work		work;
	unsigned long			tick_ms;

	int				enable_wait;
	wait_queue_head_t		wait_queue;

	int				enable_tick;
	int				enable_emsmode;
	spinlock_t			lock;

	cpumask_t			log_mask;
	cpumask_t			ref_mask;

	struct hts_reg_data		*hts_reg;
	int				nr_reg;

	spinlock_t			req_lock;
	atomic_t			req_value;
	struct plist_head		req_list;
	struct plist_node		req_def_node;

	int				core_threshold;
	int				total_threshold;

	spinlock_t			app_event_lock;

#if IS_ENABLED(CONFIG_HTS_DEBUG)
	struct hts_debug_data		debug_data;
#endif
};
