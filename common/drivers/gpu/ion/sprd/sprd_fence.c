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

/*
 *  user interface
 * */

/*
 *  Now build two timeline,
 *  one for PrimaryDisplayDevice,
 *  the other for VirtualDisplayDevice.
 *  If we only use single timeline, the later Device will
 *  block the former Device.
 * */
static struct sync_timeline_data sprd_timeline;
static struct sync_timeline_data sprd_timeline_virtual;
int sprd_create_timeline(enum SPRD_DEVICE_SYNC_TYPE type)
{
	if (type == SPRD_DEVICE_PRIMARY_SYNC)
	{
		mutex_lock(&(sprd_timeline.sync_mutex));
		if (sprd_timeline.timeline == NULL)
		{
			sprd_timeline.timeline = sprd_sync_timeline_create("sprd-timeline");
			if (sprd_timeline.timeline == NULL)
			{
				printk(KERN_ERR "create_timeline, cannot create time line\n");
				mutex_unlock(&(sprd_timeline.sync_mutex));
				return -ENOMEM;
			}
			else
			{
				sprd_timeline.timeline_value = 0;
			}
		}
		mutex_unlock(&(sprd_timeline.sync_mutex));
	}
	else if (type == SPRD_DEVICE_VIRTUAL_SYNC)
	{
		mutex_lock(&(sprd_timeline_virtual.sync_mutex));
		if (sprd_timeline_virtual.timeline == NULL)
		{
			sprd_timeline_virtual.timeline = sprd_sync_timeline_create("sprd-timeline-virtual");
			if (sprd_timeline_virtual.timeline == NULL)
			{
				printk(KERN_ERR "create_timeline, cannot create virtual time line\n");
				mutex_unlock(&(sprd_timeline_virtual.sync_mutex));
				return -ENOMEM;
			}
			else
			{
				sprd_timeline_virtual.timeline_value = 0;
			}
		}
		mutex_unlock(&(sprd_timeline_virtual.sync_mutex));
	}

    return 0;
}

int sprd_destroy_timeline(struct sync_timeline_data *parent)
{
	struct sprd_sync_timeline *obj = NULL;

	if (parent->timeline == NULL)
	{
		printk(KERN_ERR "sprd_fence timeline has been released\n");
		return 0;
	}

	mutex_lock(&(parent->sync_mutex));
	obj = parent->timeline;
	sync_timeline_destroy(&obj->obj);
	mutex_unlock(&(parent->sync_mutex));

	return 0;
}

int open_sprd_sync_timeline(void)
{
	int ret = -1;
	static int init = 0;

	if (init == 0)
	{
		sprd_timeline.timeline_value = -1;
		sprd_timeline.timeline = NULL;
		mutex_init(&(sprd_timeline.sync_mutex));

		sprd_timeline_virtual.timeline_value = -1;
		sprd_timeline_virtual.timeline = NULL;
		mutex_init(&(sprd_timeline_virtual.sync_mutex));

		init = 1;
	}

	ret = sprd_create_timeline(SPRD_DEVICE_PRIMARY_SYNC);
	if (ret < 0)
	{
		return ret;
	}
	ret = sprd_create_timeline(SPRD_DEVICE_VIRTUAL_SYNC);

	return ret;
}

int close_sprd_sync_timeline(void)
{
	int ret = -1;

	ret = sprd_destroy_timeline(&sprd_timeline);

	ret = sprd_destroy_timeline(&sprd_timeline_virtual);

	return ret;
}

int sprd_fence_create(struct ion_fence_data *data, char *name)
{
	int fd = get_unused_fd();
	struct sync_pt *pt;
	struct sync_fence *fence;
	struct sync_timeline_data *parent = NULL;

	if (data == NULL || name == NULL)
	{
		printk(KERN_ERR "sprd_fence_create input para is NULL\n");
		return -EFAULT;
	}


	if (fd < 0)
	{
		fd = get_unused_fd();
		if (fd < 0)
		{
			printk(KERN_ERR "sprd_sync_pt_create failed to get fd\n");
		}
		return -EFAULT;
	}

	if (data->device_type == SPRD_DEVICE_PRIMARY_SYNC)
	{
		parent = &sprd_timeline;
	}
	else if (data->device_type == SPRD_DEVICE_VIRTUAL_SYNC)
	{
		parent = &sprd_timeline_virtual;
	}

	if (parent == NULL)
	{
		printk(KERN_ERR "sprd_fence_create failed to get sync timeline\n");
		return -EFAULT;
	}

	mutex_lock(&(parent->sync_mutex));

	pt = sprd_sync_pt_create(parent->timeline, parent->timeline_value + data->life_value);
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

	mutex_unlock(&(parent->sync_mutex));

	pr_debug("create a fence: %p, fd: %d, life_value: %d, name: %s\n",
			(void *)fence, fd, data->life_value, name);

    return fd;

err:
   put_unused_fd(fd);
   mutex_unlock(&(parent->sync_mutex));
   return -ENOMEM;
}

int sprd_fence_destroy(struct ion_fence_data *data)
{
	if (data == NULL)
	{
		printk(KERN_ERR "sprd_fence_destroy parameters NULL\n");
		return -EFAULT;
	}

	if (data->release_fence_fd >= 0)
	{
		struct sync_fence *fence = sync_fence_fdget(data->release_fence_fd);
		if (fence == NULL)
		{
			printk(KERN_ERR "sprd_fence_destroy failed fence == NULL\n");
			return -EFAULT;
		}
	}
	else if (data->retired_fence_fd >= 0)
	{
		struct sync_fence *fence = sync_fence_fdget(data->retired_fence_fd);
		if (fence == NULL)
		{
			printk(KERN_ERR "sprd_fence_destroy failed fence == NULL\n");
		}	return -EFAULT;
	}

	return 0;
}
int sprd_fence_build(struct ion_fence_data *data)
{
	if (data == NULL)
	{
		printk(KERN_ERR "sprd_fence_build input para is NULL\n");
		return -EFAULT;
	}

	data->release_fence_fd = sprd_fence_create(data, "HWCRelease");
	if (data->release_fence_fd < 0)
	{
		printk(KERN_ERR "sprd_fence_build failed to create release fence\n");
		return -ENOMEM;
	}

	data->retired_fence_fd = sprd_fence_create(data, "HWCRetire");
	if (data->retired_fence_fd < 0)
	{
		printk(KERN_ERR "sprd_fence_build failed to create release fence\n");
		return -ENOMEM;
	}

	return 0;
}

int sprd_fence_signal_timeline(struct sync_timeline_data *parent)
{
	if (parent == NULL)
	{
		printk(KERN_ERR "sprd_fence_signal_timeline timeline is NULL\n");
		return -EFAULT;
	}

	mutex_lock(&(parent->sync_mutex));

	if (parent->timeline)
	{
		sprd_sync_timeline_inc(parent->timeline, 1);
		parent->timeline_value++;

		/*
		 *  For avoiding overflow
		 * */
		if (parent->timeline_value < 0)
		{
			parent->timeline_value = 0;
		}
	}

	mutex_unlock(&(parent->sync_mutex));

	pr_debug("sprd_signal_fence value: %d\n", parent->timeline_value);

	return 0;
}
int sprd_fence_signal(struct ion_fence_data *data)
{
	struct sync_timeline_data *parent = NULL;
	enum SPRD_DEVICE_SYNC_TYPE device_type= SPRD_DEVICE_PRIMARY_SYNC;

	if (data == NULL)
	{
		printk(KERN_ERR "sprd_fence_signal parameter is NULL\n");
		return -EFAULT;
	}

	if (data->device_type == SPRD_DEVICE_PRIMARY_SYNC)
	{
		parent = &sprd_timeline;
	}
	else if (data->device_type == SPRD_DEVICE_VIRTUAL_SYNC)
	{
		parent = &sprd_timeline_virtual;
	}

	if (parent == NULL)
	{
		printk(KERN_ERR "sprd_fence_signal failed to get sync timeline\n");
		return -EFAULT;
	}

	sprd_fence_signal_timeline(parent);
	pr_debug("sprd_signal_fence device_type: %d\n", data->device_type);

	return 0;
}

int sprd_fence_wait(int fence_fd)
{
	int ret = 0;

	struct sync_fence *fence = NULL;

	if (fence_fd < 0)
	{
		printk(KERN_ERR "sprd_wait_fence input parameters is NULL\n");
		return -EFAULT;
	}

	fence = sync_fence_fdget(fence_fd);
	if (fence == NULL)
	{
		printk(KERN_ERR "sprd_fence_wait failed fence == NULL\n");
		return -EFAULT;
	}

	ret = sync_fence_wait(fence, WAIT_FENCE_TIMEOUT);
	sync_fence_put(fence);
	if (ret < 0)
	{
		printk(KERN_ERR "sync_fence_wait failed, ret = %x\n", ret);
	}

	pr_debug("sprd_wait_fence wait fence: %p done\n", (void *)fence);

	return ret;
}

struct sync_pt *sprd_fence_dup(struct sync_pt *sync_pt)
{
	struct sync_pt *pt = NULL;

	pt = sprd_sync_pt_dup(sync_pt);

	return pt;
}
