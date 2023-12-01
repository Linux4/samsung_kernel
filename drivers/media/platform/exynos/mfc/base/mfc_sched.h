/*
 * drivers/media/platform/exynos/mfc/base/mfc_sched.h
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __MFC_SCHED_H
#define __MFC_SCHED_H __FILE__

#include "mfc_common.h"

extern struct mfc_sched_class mfc_sched_rr;
extern struct mfc_sched_class mfc_sched_prio;

static inline int mfc_get_prio(struct mfc_core *core, int rt, int prio)
{
	return (rt == MFC_NON_RT) ? (core->num_prio + prio) : prio;
}

static inline void mfc_sched_init_core(struct mfc_core *core)
{
	core->sched = &mfc_sched_rr;
	core->sched->create_work(core);

	core->sched = &mfc_sched_prio;
	core->sched->create_work(core);

	if (core->dev->pdata->scheduler)
		core->sched = &mfc_sched_prio;
	else
		core->sched = &mfc_sched_rr;

	core->sched->init_work(core);
}

static inline void mfc_sched_init_plugin(struct mfc_core *core)
{
	/* Plugin driver supports only round-robin scheduler */
	core->sched = &mfc_sched_rr;
	core->sched->create_work(core);
	core->sched->init_work(core);
}

/* This is used just for debugging */
static inline void mfc_sched_change_type(struct mfc_core *core)
{
	if (core->dev->debugfs.sched_type == 1)
		core->sched = &mfc_sched_rr;
	else if (core->dev->debugfs.sched_type == 2)
		core->sched = &mfc_sched_prio;
	else
		core->sched = &mfc_sched_rr;

	core->sched->init_work(core);
}
#endif /* __MFC_SCHED_H */
