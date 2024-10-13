/*
 * include/linux/gspn_sync.h
 *
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _GSPN_SYNC_H
#define _GSPN_SYNC_H

#include <linux/types.h>
#include <linux/kconfig.h>
#include <linux/sync.h>
#include "gspn_drv.h"

struct gspn_sync_timeline {
	struct	sync_timeline	obj;
	/*value of kcfg at the timline*/
	unsigned int		record;
	/*value of the timline*/
	unsigned int		value;
	struct mutex lock;
};

struct gspn_sync_pt {
	struct sync_pt		pt;

	unsigned int		value;
};


struct gspn_sync_create_fence_data {
	unsigned int	value;
	char	name[32];
	int32_t	__user *fd_addr; /* fd addr at user space */
};

struct gspn_layer_list_fence_data {
	uint32_t *rls_cnt;
	uint32_t *acq_cnt;
	struct sync_fence **rls_fen_arr;
	struct sync_fence **acq_fen_arr;
};


void gspn_layer_list_fence_free(struct gspn_layer_list_fence_data* list_data);
int gspn_sync_timeline_destroy(struct gspn_sync_timeline* obj);
void gspn_sync_fence_signal(struct gspn_sync_timeline *obj);
struct gspn_sync_timeline *gspn_sync_timeline_create(const char *name);
void gspn_sync_timeline_inc(struct gspn_sync_timeline *obj, u32 inc);
void gspn_layer_list_release_fence_free(struct sync_fence **fen_arr,
 										uint32_t *count);
void gspn_layer_list_acquire_fence_free(struct sync_fence **fen_arr,
 										uint32_t *count);
int gspn_layer_list_fence_process(GSPN_KCMD_INFO_T *kcmd,
		struct gspn_sync_timeline *timeline);

#endif /* _LINUX_GSPN_SYNC_H */
