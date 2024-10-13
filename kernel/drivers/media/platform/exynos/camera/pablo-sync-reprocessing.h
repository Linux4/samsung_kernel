// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef PABLO_SYNC_REPRO_H
#define PABLO_SYNC_REPRO_H

#include "is-config.h"

#define CALL_SYNC_REPRO_OPS(sync_repro, op, args...)	\
	(((sync_repro) && (sync_repro)->ops && (sync_repro)->ops->op) ? ((sync_repro)->ops->op(args)) : 0)

struct pablo_sync_repro_ops {
	bool (*start_trigger)(struct is_group_task *gtask, struct is_group *group, struct is_frame *frame);
	int (*init)(struct is_group_task *gtask);
	int (*flush_task)(struct is_group_task *gtask, struct is_group *head);
};

struct pablo_sync_repro {
	struct list_head		sync_list;
	atomic_t			preview_cnt[IS_STREAM_COUNT];
	struct list_head		preview_list[IS_STREAM_COUNT];

	struct timer_list		trigger_timer;
	struct is_group_task		*trigger_gtask;
	spinlock_t			trigger_slock;

	const struct pablo_sync_repro_ops *ops;
};

#if IS_ENABLED(ENABLE_SYNC_REPROCESSING)
int pablo_sync_repro_set_main(u32 instance);
int pablo_sync_repro_reset_main(u32 instance);
#else
#define pablo_sync_repro_set_main(instance) { 0; }
#define pablo_sync_repro_reset_main(instance) { 0; }
#endif
struct pablo_sync_repro *pablo_sync_repro_probe(void);

#endif
