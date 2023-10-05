// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/vmalloc.h>

#include "pablo-icpu.h"
#include "pablo-icpu-msg-queue.h"
#include "pablo-icpu-itf.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-MSGQUEUE]",
};

struct icpu_logger *get_icpu_msgqueue_log(void)
{
	return &_log;
}

static struct icpu_itf_msg *__get_msg(struct icpu_msg_queue *queue);
static struct icpu_itf_msg *__get_msg_key(struct icpu_msg_queue *queue, u64 key);
static struct icpu_itf_msg *__get_msg_prio(struct icpu_msg_queue *queue, u32 prio);
static void __set_msg(struct icpu_msg_queue *queue, struct icpu_itf_msg *msg);
static u32 __len(struct icpu_msg_queue *queue);
static void __dump(struct icpu_msg_queue *queue);

int icpu_msg_queue_init(struct icpu_msg_queue *queue, u32 num_prio)
{
	int i;

	if (!queue || !num_prio)
		return -EINVAL;

	queue->msg_list = vmalloc(sizeof(struct list_head) * num_prio);
	if (!queue->msg_list)
		return -ENOMEM;

	for (i = 0; i < num_prio; i++)
		INIT_LIST_HEAD(&queue->msg_list[i]);

	queue->num_priority = num_prio;
	queue->msg_cnt = 0;

	spin_lock_init(&queue->lock);

	queue->get_msg = __get_msg;
	queue->get_msg_key = __get_msg_key;
	queue->get_msg_prio = num_prio > 1 ? __get_msg_prio : NULL;
	queue->set_msg = __set_msg;
	queue->len = __len;
	queue->dump = __dump;

	return 0;
}
KUNIT_EXPORT_SYMBOL(icpu_msg_queue_init);

void icpu_msg_queue_deinit(struct icpu_msg_queue *queue)
{
	if (!queue)
		return;

	vfree(queue->msg_list);
	queue->msg_list = NULL;
	queue->num_priority = 0;
	queue->msg_cnt = 0;
	queue->get_msg = NULL;
	queue->get_msg_key = NULL;
	queue->get_msg_prio = NULL;
	queue->set_msg = NULL;
	queue->len = NULL;
	queue->dump = NULL;
}
KUNIT_EXPORT_SYMBOL(icpu_msg_queue_deinit);

static struct icpu_itf_msg *__get_msg(struct icpu_msg_queue *queue)
{
	int i = 0;
	struct icpu_itf_msg *msg = NULL;
	u32 list_cnt = queue->num_priority - 1;

	/* ICPU_MSG_PRIO_0 is top priority. Let start from top priority */
	for (i = list_cnt; i >= 0; --i) {
		msg = list_first_entry_or_null(&queue->msg_list[i], struct icpu_itf_msg, list);
		if (msg) {
			list_del_init(&msg->list);
			queue->msg_cnt--;
			break;
		}
	}

	return msg;
}

static struct icpu_itf_msg *__get_msg_key(struct icpu_msg_queue *queue, u64 key)
{
	int i = 0;
	struct icpu_itf_msg *msg = NULL;
	u32 list_cnt = queue->num_priority - 1;

	for (i = list_cnt; i >= 0; --i) {
		if (list_empty(&queue->msg_list[i]))
			continue;

		list_for_each_entry(msg, &queue->msg_list[i], list) {
			if (msg->cmd.key == key) {
				list_del_init(&msg->list);
				queue->msg_cnt--;
				return msg;
			}
		}
	}

	return NULL;
}

static struct icpu_itf_msg *__get_msg_prio(struct icpu_msg_queue *queue, u32 prio)
{
	struct icpu_itf_msg *msg = NULL;

	if (queue->num_priority <= prio)
		return NULL;

	msg = list_first_entry_or_null(&queue->msg_list[prio], struct icpu_itf_msg, list);
	if (msg) {
		list_del_init(&msg->list);
		queue->msg_cnt--;
	}

	return msg;
}

static void __set_msg(struct icpu_msg_queue *queue, struct icpu_itf_msg *msg)
{
	u32 prio = queue->num_priority > 1 ? msg->prio : 0;

	list_add_tail(&msg->list, &queue->msg_list[prio]);
	queue->msg_cnt++;
}

static u32 __len(struct icpu_msg_queue *queue)
{
	u32 len;
	unsigned long _lock_flags;

	spin_lock_irqsave(&queue->lock, _lock_flags);
	len = queue->msg_cnt;
	spin_unlock_irqrestore(&queue->lock, _lock_flags);

	return len;
}

static void __dump(struct icpu_msg_queue *queue)
{
	int i;
	struct icpu_itf_msg *msg;
	char *str_prio[ICPU_MSG_PRIO_MAX] = { "HIGH", "NORMAL", "LOW", };
	u32 list_cnt = queue->num_priority - 1;
	unsigned long lock_flags;
	bool empty;

	spin_lock_irqsave(&queue->lock, lock_flags);

	for (i = list_cnt; i >= 0; --i) {
		empty = list_empty(&queue->msg_list[i]);
		if (queue->num_priority > 1)
			ICPU_INFO("Priority_%s: %s", str_prio[i], empty ? "empty" : "");

		if (empty)
			continue;

		list_for_each_entry(msg, &queue->msg_list[i], list)
			MSG_DUMP(msg);
	}

	spin_unlock_irqrestore(&queue->lock, lock_flags);
}
