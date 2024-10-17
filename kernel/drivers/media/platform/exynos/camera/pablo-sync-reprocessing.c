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

#include "pablo-debug.h"
#include "is-groupmgr.h"
#include "is-device-ischain.h"
#include "is-core.h"
#include "pablo-sync-reprocessing.h"

static struct is_device_ischain *main_dev;

IS_TIMER_FUNC(is_group_trigger_timer)
{
	struct pablo_sync_repro *sync_repro = from_timer(sync_repro, (struct timer_list *)data, trigger_timer);
	struct is_frame *rframe;
	struct is_group_task *gtask;
	unsigned long flag;

	gtask = sync_repro->trigger_gtask;

	spin_lock_irqsave(&sync_repro->trigger_slock, flag);
	if (!list_empty(&sync_repro->sync_list)) {
		rframe = list_first_entry(&sync_repro->sync_list, struct is_frame, sync_list);
		list_del(&rframe->sync_list);
		kthread_queue_work(&gtask->worker, &rframe->work);
	}
	spin_unlock_irqrestore(&sync_repro->trigger_slock, flag);
}

static bool pablo_sync_repro_start_trigger(struct is_group_task *gtask,
				struct is_group *group,
				struct is_frame *frame)
{
	struct is_frame *rframe = NULL;
	struct is_device_ischain *device;
	struct is_core *core;
	int i;
	int loop_cnt;
	unsigned long flag;
	struct pablo_sync_repro *sync_repro = gtask->sync_repro;

	device = group->device;
	core = is_get_is_core();

	if (atomic_read(&gtask->refcount) <= 1)
		return false;

	if (test_bit(IS_ISCHAIN_REPROCESSING, &group->device->state)) {
		list_add_tail(&frame->sync_list, &sync_repro->sync_list);
		sync_repro->trigger_gtask = gtask;
		mod_timer(&sync_repro->trigger_timer, jiffies + msecs_to_jiffies(300));
		mgrdbgs(1, "[SYNC_REPRO] set sync list\n", group, group, frame);
	} else {
		/* trigger_timer reset in preview path */
		if (timer_pending(&sync_repro->trigger_timer))
			del_timer(&sync_repro->trigger_timer);

		/* main */
		if (device == main_dev) {
			kthread_queue_work(&gtask->worker, &frame->work);
			for (i = 0; i < IS_STREAM_COUNT; i++) {
				if (i == group->instance)
					continue;
				loop_cnt = 0;
				/* process Preview queue */
				if (!list_empty(&sync_repro->preview_list[i])) {
					loop_cnt = (int)core->ischain[i].sensor->cfg->framerate / (int)main_dev->sensor->cfg->framerate;
					/* main fps <= sub fps */
					if (loop_cnt) {
						while (loop_cnt-- && !list_empty(&sync_repro->preview_list[i])) {
							atomic_dec(&sync_repro->preview_cnt[i]);
							rframe = list_first_entry(&sync_repro->preview_list[i], struct is_frame, preview_list);
							list_del(&rframe->preview_list);
							kthread_queue_work(&gtask->worker, &rframe->work);
							mgrdbgs(1, "[SYNC_REPRO] get sync list 1\n", group, group, rframe);
						}
					} else {
						if (list_empty(&sync_repro->preview_list[i]))
							break;
						atomic_dec(&sync_repro->preview_cnt[i]);
						rframe = list_first_entry(&sync_repro->preview_list[i], struct is_frame, preview_list);
						list_del(&rframe->preview_list);
						kthread_queue_work(&gtask->worker, &rframe->work);
						mgrdbgs(1, "[SYNC_REPRO] get sync list 2\n", group, group, rframe);
					}
				}
			}
			/* process Capture queue */
			spin_lock_irqsave(&sync_repro->trigger_slock, flag);
			if (!list_empty(&sync_repro->sync_list)) {
				rframe = list_first_entry(&sync_repro->sync_list, struct is_frame, sync_list);
				list_del(&rframe->sync_list);
				kthread_queue_work(&gtask->worker, &rframe->work);
				mgrdbgs(1, "[SYNC_REPRO] get sync list 3\n", group, group, rframe);
			}
			spin_unlock_irqrestore(&sync_repro->trigger_slock, flag);
		} else {
			loop_cnt = (int)core->ischain[group->instance].sensor->cfg->framerate / (int)main_dev->sensor->cfg->framerate;
			if ((!list_empty(&sync_repro->preview_list[group->instance]))
				&& atomic_read(&sync_repro->preview_cnt[group->instance]) >= loop_cnt) {
					atomic_dec(&sync_repro->preview_cnt[group->instance]);
					rframe = list_first_entry(&sync_repro->preview_list[group->instance], struct is_frame, preview_list);
					list_del(&rframe->preview_list);
					kthread_queue_work(&gtask->worker, &rframe->work);
					mgrdbgs(1, "[SYNC_REPRO] get sync list 4\n", group, group, rframe);
			}

			atomic_inc(&sync_repro->preview_cnt[group->instance]);
			list_add_tail(&frame->preview_list, &sync_repro->preview_list[group->instance]);
		}
	}

	return true;
}

static int pablo_sync_repro_init(struct is_group_task *gtask)
{
	int i;
	struct pablo_sync_repro *sync_repro = gtask->sync_repro;

	INIT_LIST_HEAD(&sync_repro->sync_list);
	for (i = 0; i < IS_STREAM_COUNT; i++) {
		atomic_set(&sync_repro->preview_cnt[i], 0);
		INIT_LIST_HEAD(&sync_repro->preview_list[i]);
	}

	timer_setup(&sync_repro->trigger_timer, (void (*)(struct timer_list *))is_group_trigger_timer, 0);
	spin_lock_init(&sync_repro->trigger_slock);

	return 0;
}

/* Do triggering remainng capture task at stream off of preview instance. */
static int pablo_sync_repro_flush_task(struct is_group_task *gtask, struct is_group *head)
{
	struct pablo_sync_repro *sync_repro = gtask->sync_repro;

	if (!list_empty(&sync_repro->sync_list)) {
		struct is_frame *rframe;
		rframe = list_first_entry(&sync_repro->sync_list, struct is_frame, sync_list);
		list_del(&rframe->sync_list);
		mgrinfo("flush SYNC capture(%d)\n", head, head, rframe, rframe->index);
		kthread_queue_work(&gtask->worker, &rframe->work);
	}

	if (!list_empty(&sync_repro->preview_list[head->instance])) {
		struct is_frame *rframe;
		atomic_dec(&sync_repro->preview_cnt[head->instance]);
		rframe = list_first_entry(&sync_repro->preview_list[head->instance], struct is_frame, preview_list);
		list_del(&rframe->preview_list);
		mgrinfo("flush SYNC preview(%d)\n", head, head, rframe, rframe->index);
		kthread_queue_work(&gtask->worker, &rframe->work);
	}

	del_timer(&sync_repro->trigger_timer);

	return 0;
}

int pablo_sync_repro_set_main(u32 instance)
{
	struct is_device_ischain *device = is_get_ischain_device(instance);

	if (!device) {
		mierr("device is NULL", instance);
		return -EINVAL;
	}

	/* find main_dev for sync processing */
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state) && !main_dev) {
		main_dev = device;
		minfo("SYNC : set main device\n", device);
	}

	return 0;
}

/* In case of dual camera, set another preivew instance to main stream */
int pablo_sync_repro_reset_main(u32 instance)
{
	int i;
	struct is_core *core;
	u32 main_instance;
	struct is_device_ischain *device = is_get_ischain_device(instance);

	if (!device) {
		mierr("device is NULL", instance);
		return -EINVAL;
	}

	core = is_get_is_core();
	/* reset main device for sync processing */
	if (!test_bit(IS_ISCHAIN_REPROCESSING, &device->state) && main_dev == device) {
		main_instance = main_dev->instance;
		main_dev = NULL;
		for (i = 0; i < IS_STREAM_COUNT; i++) {
			if (!test_bit(IS_ISCHAIN_REPROCESSING, &core->ischain[i].state)
			    && test_bit(IS_ISCHAIN_START, &core->ischain[i].state)
			    && (i != main_instance)) {
				main_dev = &core->ischain[i];
				minfo("SYNC : reset main device(%d)\n", device, main_dev->instance);
				break;
			}
		}
	}

	return 0;
}

const struct pablo_sync_repro_ops psr_ops = {
	.start_trigger = pablo_sync_repro_start_trigger,
	.init = pablo_sync_repro_init,
	.flush_task = pablo_sync_repro_flush_task,
};

struct pablo_sync_repro *pablo_sync_repro_probe(void)
{
	struct pablo_sync_repro *sync_repro;

	sync_repro = vzalloc(sizeof(struct pablo_sync_repro));
	if (!sync_repro) {
		err("failed to allocate sync_repro");
		return NULL;
	}

	sync_repro->ops = &psr_ops;

	return sync_repro;
}
