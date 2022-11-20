/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SPRD_FENCE_H_
#define _SPRD_FENCE_H_

#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/sync.h>
#include <video/ion_sprd.h>

struct sprd_sync_timeline {
    struct sync_timeline obj;
    u32 value;
};

struct sprd_sync_pt {
    struct sync_pt pt;
    u32 value;
};

struct sprd_sync_create_fence_data {
    __u32 value;
    char name[32];
    __s32 fence; /* fd of new fence */
};

struct sync_timeline_data {
    int timeline_value;
    struct mutex sync_mutex;
    struct sprd_sync_timeline *timeline;
};

extern int open_sprd_sync_timeline(void);
extern int close_sprd_sync_timeline(void);
extern int sprd_fence_build(struct ion_fence_data *data);
extern int sprd_fence_destroy(struct ion_fence_data *data);
extern int sprd_fence_signal(struct ion_fence_data *data);
extern int sprd_fence_wait(int fence_fd);
extern struct sync_pt *sprd_fence_dup(struct sync_pt *sync_pt);

#endif
