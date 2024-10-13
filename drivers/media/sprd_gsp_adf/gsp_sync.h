/*
 * include/linux/gsp_sync.h
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

#ifndef __GSP_SYNC_H__
#define __GSP_SYNC_H__

#include <linux/types.h>
#include <linux/kconfig.h>
#include <linux/sync.h>
#include <video/gsp_types_shark_adf.h>
#include <video/ion_sprd.h>

#define GSP_WAIT_FENCE_TIMEOUT 3000/*3000ms*/

struct gsp_sync_timeline {
	struct	sync_timeline	obj;
	/*value of kcfg at the timline*/
	unsigned int		record;
	/*value of the timline*/
	unsigned int		value;
	struct mutex lock;
};

struct gsp_sync_pt {
	struct sync_pt		pt;

	unsigned int		value;
};

struct layer_en {
	int img_en;
	int osd_en;
};

struct gsp_sync_create_fence_info {
	u32		value;
	char	name[32];
	int32_t	__user *fd_addr; /* fd addr at user space */
};


#define GSP_SIGNAL_FENCE_MAX 3 
#define GSP_WAIT_FENCE_MAX 3
struct gsp_fence_data {
	/*manage wait&sig sync fence*/
	struct sync_fence *sig_fen;
	struct sync_fence *wait_fen_arr[GSP_WAIT_FENCE_MAX];
	int wait_cnt;
	/*get from hwc_layer_1_t*/
	int img_wait_fd;
	int osd_wait_fd;
	int des_wait_fd;
	/*judge handling fence or not*/
	struct layer_en enable;
	int __user * des_sig_ufd;
};

void gsp_layer_list_fence_free(struct gsp_fence_data* data);
int gsp_sync_timeline_destroy(struct gsp_sync_timeline* obj);
void gsp_sync_fence_signal(struct gsp_sync_timeline *obj);
struct gsp_sync_timeline *gsp_sync_timeline_create(const char *name);
void gsp_sync_timeline_inc(struct gsp_sync_timeline *obj, u32 inc);
void gsp_layer_list_signal_fence_free(struct sync_fence *fen);
void gsp_layer_list_wait_fence_free(struct sync_fence **fen_arr,
 										uint32_t *count);
int gsp_layer_list_fence_process(struct gsp_fence_data *data,
		struct gsp_sync_timeline *timeline);

#endif
