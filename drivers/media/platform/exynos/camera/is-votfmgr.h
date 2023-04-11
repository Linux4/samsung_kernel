/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IS_VOTF_MGR_H
#define IS_VOTF_MGR_H

#include "votf/camerapp-votf-common-enum.h"
#include "is-groupmgr.h"

#define NUM_OF_VOTF_BUF		(1)

int is_votf_flush(struct is_group *group);
int is_votf_create_link(struct is_group *group, u32 width, u32 height);
int is_votf_destroy_link(struct is_group *group);
int is_votf_change_link(struct is_group *group);
struct is_framemgr *is_votf_get_framemgr(struct is_group *group, enum votf_service type,
	unsigned long id);
struct is_frame *is_votf_get_frame(struct is_group *group, enum votf_service type,
	unsigned long id, u32 fcount);
int is_votf_register_framemgr(struct is_group *group, enum votf_service type,
	void *data, votf_s_addr fn, unsigned long id);
int is_votf_register_oneshot(struct is_group *group, enum votf_service type,
	void *data, votf_s_oneshot fn, unsigned long id);

#define votf_fmgr_call(mgr, o, f, args...)				\
	({								\
		int __result = 0;					\
		if (!(mgr))						\
			__result = -ENODEV;				\
		else if (!((mgr)->o.f))					\
			__result = -ENOIOCTLCMD;			\
		else							\
			(mgr)->o.f((mgr)->o.data, mgr->o.id, ##args);	\
		__result;						\
	})

#endif
