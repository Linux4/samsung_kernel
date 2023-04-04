/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/idr.h>
#include <linux/kthread.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/wait.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)
#include <linux/timekeeping.h>
#else
#include <linux/hrtimer.h>
#endif

#include "tzdev_internal.h"
#include "core/event.h"
#include "core/iw_events.h"
#include "core/iw_mem.h"
#include "core/log.h"
#include "core/notifier.h"
#include "core/smc_channel.h"
#include "core/sysdep.h"
#include "core/wait.h"

#define TZDEV_IW_EVENTS_BITMAP_SIZE \
		(sizeof(struct iwd_events_buf) + round_up(CONFIG_TZDEV_IW_EVENTS_MAX_EVENTS, 32) / 8)

struct tzdev_iw_events_handler {
	iw_event_callback_t callback;
	void *arg;
};

static struct {
	struct tzdev_iw_mem *mem;
	struct iwd_events_buf *in_bitmap;
	struct iwd_events_buf *out_bitmap;
	struct idr handlers;
	struct mutex handlers_lock;
	struct task_struct *event_thread;
	wait_queue_head_t wq;
} events;

static enum {
	UNINITIALIZED,
	INITIALIZED,
} status = UNINITIALIZED;

/**
 * tzdev_iw_events_register() - Register nwd iw event.
 * @callback: event handler
 * @arg: event handler arg
 *
 * It shouldn't be called from callback.
 *
 * Return: nwd event id(>=0) on success and error on failure.
 */
int tzdev_iw_events_register(iw_event_callback_t callback, void *arg)
{
	struct tzdev_iw_events_handler *handler;
	int event_id;

	if (unlikely(status == UNINITIALIZED)) {
		log_error(tzdev_iw_events, "Subsystem isn't initialized\n");
		return -ENODEV;
	}

	/* Prevents speculative reading of events values before checking status */
	smp_rmb();

	handler = kzalloc(sizeof(struct tzdev_iw_events_handler), GFP_KERNEL);
	if (!handler) {
		log_error(tzdev_iw_events, "Failed to allocate event handler\n");
		return -ENOMEM;
	}
	handler->callback = callback;
	handler->arg = arg;

	mutex_lock(&events.handlers_lock);
	event_id = idr_alloc(&events.handlers, handler, 0, CONFIG_TZDEV_IW_EVENTS_MAX_EVENTS, GFP_KERNEL);
	mutex_unlock(&events.handlers_lock);

	if (event_id < 0) {
		log_error(tzdev_iw_events, "Failed to allocate event id\n");
		kfree(handler);
		return -ENOSPC;
	}

	log_debug(tzdev_iw_events, "nwd event { %d id, %p callback, %p arg } was registered\n",
			event_id, callback, arg);
	return event_id;
}

/**
 * tzdev_iw_events_unregister() - Unregister nwd iw event.
 * @nwd_event_id: nwd event id.
 */
void tzdev_iw_events_unregister(unsigned int nwd_event_id)
{
	struct tzdev_iw_events_handler *handler;

	if (unlikely(status == UNINITIALIZED)) {
		log_error(tzdev_iw_events, "Subsystem isn't initialized\n");
		return;
	}

	/* Prevents speculative reading of events values before checking status */
	smp_rmb();

	if (current != events.event_thread)
		mutex_lock(&events.handlers_lock);

	handler = idr_find(&events.handlers, nwd_event_id);
	BUG_ON(!handler);

	idr_remove(&events.handlers, nwd_event_id);
	tz_event_del(events.in_bitmap, nwd_event_id);

	if (current != events.event_thread)
		mutex_unlock(&events.handlers_lock);

	log_debug(tzdev_iw_events, "nwd event { %u id, %p callback, %p arg } was unregistered\n",
			nwd_event_id, handler->callback, handler->arg);

	kfree(handler);
}

/**
 * tzdev_iw_events_raise() - Raise swd iw event.
 * @swd_event_id: swd event id.
 *
 * If swd event is raised, it does nothing.
 */
void tzdev_iw_events_raise(unsigned int swd_event_id)
{
	if (unlikely(status == UNINITIALIZED)) {
		log_error(tzdev_iw_events, "Subsystem isn't initialized\n");
		return;
	}

	/* Prevents speculative reading of events values before checking status */
	smp_rmb();

	tz_event_add(events.out_bitmap, swd_event_id);

	log_debug(tzdev_iw_events, "swd event { %u id } was raised\n", swd_event_id);
}

static void tzdev_iw_events_process(unsigned int event_id)
{
	struct tzdev_iw_events_handler *handler;
	s64 process_start;

	mutex_lock(&events.handlers_lock);

	handler = idr_find(&events.handlers, event_id);
	BUG_ON(!handler);

	process_start = ktime_to_ms(ktime_get());
	handler->callback(handler->arg);
	log_debug(tzdev_iw_events, "nwd event { %u id } was processed in %lld ms\n",
			event_id, ktime_to_ms(ktime_get()) - process_start);

	mutex_unlock(&events.handlers_lock);
}

static int tzdev_iw_events_notifier(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	if (tz_event_get_num_pending(events.in_bitmap) > 0)
		wake_up(&events.wq);

	return NOTIFY_DONE;
}

static struct notifier_block events_post_smc_notifier = {
	.notifier_call = tzdev_iw_events_notifier
};

static int tzdev_iw_events_kthreadfn(void *data)
{
	(void)data;

	while (1) {
		wait_event_uninterruptible_freezable_nested(events.wq,
				(status == INITIALIZED && tz_event_get_num_pending(events.in_bitmap) > 0)
				|| kthread_should_stop());

		if (kthread_should_stop())
			return 0;

		tz_event_process(events.in_bitmap, tzdev_iw_events_process);
	}
	return 0;
}

/**
 * tzdev_iw_events_init() - Initialize iw events subsystem.
 *
 * Return: 0 on success.
 */
int tzdev_iw_events_init(void)
{
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };
	struct tzdev_smc_channel *ch;
	struct tzdev_iw_mem *mem;
	char *buffer;
	int ret;

	mem = tzdev_iw_mem_create(TZDEV_IW_EVENTS_BITMAP_SIZE * 2);
	if (IS_ERR(mem)) {
		log_error(tzdev_iw_events, "Failed to create iw memory (error %ld)\n", PTR_ERR(mem));
		return 1;
	}

	buffer = tzdev_iw_mem_map(mem);
	if (buffer == NULL) {
		log_error(tzdev_iw_events, "Failed to map iw memory\n");
		goto mem_destroy;
	}

	events.mem = mem;
	events.in_bitmap = (struct iwd_events_buf *)buffer;
	events.out_bitmap = (struct iwd_events_buf *)(buffer + TZDEV_IW_EVENTS_BITMAP_SIZE);

	tz_event_init(events.in_bitmap, CONFIG_TZDEV_IW_EVENTS_MAX_EVENTS);
	tz_event_init(events.out_bitmap, CONFIG_TZDEV_IW_EVENTS_MAX_EVENTS);

	idr_init(&events.handlers);
	mutex_init(&events.handlers_lock);
	init_waitqueue_head(&events.wq);

	ch = tzdev_smc_channel_acquire();
	ret = tzdev_iw_mem_pack(mem, ch);
	if (ret) {
		tzdev_smc_channel_release(ch);
		log_error(tzdev_iw_events, "Failed to pack iw memory (error %d)\n", ret);
		goto mem_unmap;
	}
	ret = tzdev_smc_iw_events_init(CONFIG_TZDEV_IW_EVENTS_MAX_EVENTS);
	if (ret) {
		tzdev_smc_channel_release(ch);
		log_error(tzdev_iw_events, "Failed to init subsystem in SWD (error %d)\n", ret);
		goto mem_unmap;
	}
	tzdev_smc_channel_release(ch);

	events.event_thread = kthread_run(tzdev_iw_events_kthreadfn, NULL, "tzdev_iw_events_thread");
	if (IS_ERR(events.event_thread)) {
		log_error(tzdev_iw_events, "Failed to run event thread (error %ld)\n",
				PTR_ERR(events.event_thread));
		goto mem_unmap;
	}
	sched_setscheduler(events.event_thread, SCHED_FIFO, &param);

	tzdev_atomic_notifier_register(TZDEV_POST_SMC_NOTIFIER, &events_post_smc_notifier);

	/* Commits writes before setting status value to INITILIZED */
	smp_wmb();

	status = INITIALIZED;
	log_info(tzdev_iw_events, "IW Events initialization done.\n");
	return 0;

mem_unmap:
	tzdev_iw_mem_unmap(mem);
mem_destroy:
	tzdev_iw_mem_destroy(mem);
	return 1;
}

/**
 * tzdev_iw_events_fini() - Release recources allocated for iw events subsystem.
 */
void tzdev_iw_events_fini(void)
{
	status = UNINITIALIZED;

	/* Commits status value before events finalization */
	smp_wmb();

	tzdev_atomic_notifier_unregister(TZDEV_POST_SMC_NOTIFIER, &events_post_smc_notifier);
	kthread_stop(events.event_thread);

	log_info(tzdev_iw_events, "IW Events finalization done.\n");
}
