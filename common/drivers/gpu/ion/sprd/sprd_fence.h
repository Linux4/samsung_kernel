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

struct fence_sync {
    int timeline_value;
    struct sprd_sync_timeline *timeline;
};

extern int sprd_create_timeline(struct fence_sync *sprd_fence);
extern int sprd_destroy_timeline(struct fence_sync *sprd_fence);
extern int sprd_fence_create(char *name, struct fence_sync *sprd_fence, u32 value, struct sync_fence **fence_obj);
extern int sprd_fence_signal(struct fence_sync *sprd_fence);
extern int sprd_fence_wait(struct fence_sync *sprd_fence, struct sync_fence *fence_obj);
extern struct sync_pt *sprd_fence_dup(struct sync_pt *sync_pt);

#endif
