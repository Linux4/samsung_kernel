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

#include "sprd_fence.h"
#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/fs.h>

#define WAIT_FENCE_TIMEOUT 200

static struct mutex sync_mutex;

/*
 *  Porting from software sync
 *  (sw_sync.h sw_sync.c)
 * */
static int sprd_sync_cmp(u32 a, u32 b)
{
    if (a == b)
        return 0;

    return ((s32)a - (s32)b) < 0 ? -1 : 1;
}

struct sync_pt *sprd_sync_pt_create(struct sprd_sync_timeline *obj, u32 value)
{
    struct sprd_sync_pt *pt;

    pt = (struct sprd_sync_pt *)
        sync_pt_create(&obj->obj, sizeof(struct sprd_sync_pt));

    pt->value = value;

    return (struct sync_pt *)pt;
}

static struct sync_pt *sprd_sync_pt_dup(struct sync_pt *sync_pt)
{
    struct sprd_sync_pt *pt = (struct sprd_sync_pt *) sync_pt;
    struct sprd_sync_timeline *obj =
        (struct sprd_sync_timeline *)sync_pt->parent;

    return (struct sync_pt *) sprd_sync_pt_create(obj, pt->value);
}

static int sprd_sync_pt_has_signaled(struct sync_pt *sync_pt)
{
    struct sprd_sync_pt *pt = (struct sprd_sync_pt *)sync_pt;
    struct sprd_sync_timeline *obj =
        (struct sprd_sync_timeline *)sync_pt->parent;

    return sprd_sync_cmp(obj->value, pt->value) >= 0;
}

static int sprd_sync_pt_compare(struct sync_pt *a, struct sync_pt *b)
{
    struct sprd_sync_pt *pt_a = (struct sprd_sync_pt *)a;
    struct sprd_sync_pt *pt_b = (struct sprd_sync_pt *)b;

    return sprd_sync_cmp(pt_a->value, pt_b->value);
}


static int sprd_sync_fill_driver_data(struct sync_pt *sync_pt,
                                    void *data, int size)
{
    struct sprd_sync_pt *pt = (struct sprd_sync_pt *)sync_pt;

    if (size < sizeof(pt->value))
        return -ENOMEM;

    memcpy(data, &pt->value, sizeof(pt->value));

    return sizeof(pt->value);
}

static void sprd_sync_timeline_value_str(struct sync_timeline *sync_timeline,
                                       char *str, int size)
{
    struct sprd_sync_timeline *timeline =
        (struct sprd_sync_timeline *)sync_timeline;
    snprintf(str, size, "%d", timeline->value);
}

static void sprd_sync_pt_value_str(struct sync_pt *sync_pt,
                                 char *str, int size)
{
    struct sprd_sync_pt *pt = (struct sprd_sync_pt *)sync_pt;
    snprintf(str, size, "%d", pt->value);
}

static struct sync_timeline_ops sprd_sync_timeline_ops = {
    .driver_name = "sprd_sync",
    .dup = sprd_sync_pt_dup,
    .has_signaled = sprd_sync_pt_has_signaled,
    .compare = sprd_sync_pt_compare,
    .fill_driver_data = sprd_sync_fill_driver_data,
    .timeline_value_str = sprd_sync_timeline_value_str,
    .pt_value_str = sprd_sync_pt_value_str,
};

struct sprd_sync_timeline *sprd_sync_timeline_create(const char *name)
{
    struct sprd_sync_timeline *obj = (struct sprd_sync_timeline *)
        sync_timeline_create(&sprd_sync_timeline_ops,
                             sizeof(struct sprd_sync_timeline),
                             name);

    return obj;
}

void sprd_sync_timeline_inc(struct sprd_sync_timeline *obj, u32 inc)
{
    obj->value += inc;

    sync_timeline_signal(&obj->obj);
}

/*
 *  Porting software sync done
 * */

int sprd_create_timeline(struct fence_sync *sprd_fence)
{
    static int init = 0;

    if (sprd_fence == NULL)
    {
        printk(KERN_ERR "create_timeline, input parameter is NULL\n");
        return -EINVAL;
    }

    if (init == 0)
    {
        sprd_fence->timeline_value = -1;
        sprd_fence->timeline = NULL;
        mutex_init(&sync_mutex);

        init = 1;
    }

    mutex_lock(&sync_mutex);
    if (sprd_fence->timeline == NULL)
    {
        sprd_fence->timeline = sprd_sync_timeline_create("sprd-timeline");
        if (sprd_fence->timeline == NULL)
        {
            printk(KERN_ERR "create_timeline, cannot create time line\n");
            mutex_unlock(&sync_mutex);
            return -ENOMEM;
        }
        else
        {
            sprd_fence->timeline_value = 0;
        }
    }
    mutex_unlock(&sync_mutex);

    return 0;
}

int sprd_destroy_timeline(struct fence_sync *sprd_fence)
{
    struct sprd_sync_timeline *obj = NULL;

    if (sprd_fence == NULL)
    {
        printk(KERN_ERR "sprd_fence is NULL\n");
        return -EINVAL;
    }

    if (sprd_fence->timeline == NULL)
    {
        printk(KERN_ERR "sprd_fence timeline has been released\n");
        return 0;
    }

    obj = sprd_fence->timeline;
    mutex_lock(&sync_mutex);
    sync_timeline_destroy(&obj->obj);
    mutex_unlock(&sync_mutex);

    return 0;
}

int sprd_fence_create(char *name, struct fence_sync *sprd_fence, u32 value, struct sync_fence **fence_obj)
{
    int fd = get_unused_fd();
    struct sync_pt *pt;
    struct sync_fence *fence;

    mutex_lock(&sync_mutex);

    if (fd < 0)
    {
        fd = get_unused_fd();
        if (fd < 0)
        {
            printk(KERN_ERR "sprd_sync_pt_create failed to get fd\n");
            goto err2;
        }
    }

    pt = sprd_sync_pt_create(sprd_fence->timeline, sprd_fence->timeline_value + value);
    if (pt == NULL)
    {
        printk(KERN_ERR "sprd_sync_pt_create failed\n");
        goto err;
    }

    name[sizeof(name) - 1] = '\0';
    fence = sync_fence_create(name, pt);
    if (fence == NULL)
    {
        sync_pt_free(pt);
        printk(KERN_ERR "sprd_create_fence failed\n");
        goto err;
    }

    sync_fence_install(fence, fd);

    *fence_obj = fence;

    mutex_unlock(&sync_mutex);

    pr_debug("create a fence: %p, fd: %d, value: %d\n",
             (void *)fence, fd, value);

    return fd;

err:
   put_unused_fd(fd);
err2:
   mutex_unlock(&sync_mutex);
   return -ENOMEM;
}

int sprd_fence_signal(struct fence_sync *sprd_fence)
{
    mutex_lock(&sync_mutex);

    if (sprd_fence->timeline)
    {
        sprd_sync_timeline_inc(sprd_fence->timeline, 1);
        sprd_fence->timeline_value++;

        /*
         *  For avoiding overflow
         * */
        if (sprd_fence->timeline_value < 0)
        {
            sprd_fence->timeline_value = 0;
        }
    }

    mutex_unlock(&sync_mutex);

    pr_debug("sprd_signal_fence value: %d\n", sprd_fence->timeline_value);

    return 0;
}

int sprd_fence_wait(struct fence_sync *sprd_fence, struct sync_fence *fence_obj)
{
    int ret = 0;

    if (sprd_fence == NULL || fence_obj == NULL)
    {
        printk(KERN_ERR "sprd_wait_fence input parameters is NULL\n");
        return -EFAULT;
    }

    ret = sync_fence_wait(fence_obj, WAIT_FENCE_TIMEOUT);
    sync_fence_put(fence_obj);
    if (ret < 0)
    {
        printk(KERN_ERR "sync_fence_wait failed, ret = %x\n", ret);
    }

    pr_debug("sprd_wait_fence wait fence: %p done\n", (void *)sprd_fence);

    return ret;
}

struct sync_pt *sprd_fence_dup(struct sync_pt *sync_pt)
{
    struct sync_pt *pt = NULL;

    mutex_lock(&sync_mutex);
    pt = sprd_sync_pt_dup(sync_pt);
    mutex_unlock(&sync_mutex);

    return pt;
}
