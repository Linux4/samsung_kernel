/* SPDX-License-Identifier: GPL */
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

#ifndef PABLO_ICPU_MSG_QUEUE_H
#define PABLO_ICPU_MSG_QUEUE_H

struct icpu_itf_msg;
struct icpu_msg_queue {
	u32 num_priority;
	struct list_head *msg_list;
	u32 msg_cnt;
	spinlock_t lock;

	struct icpu_itf_msg *(*get_msg)(struct icpu_msg_queue *queue);
	struct icpu_itf_msg *(*get_msg_key)(struct icpu_msg_queue *queue, u64 key);
	struct icpu_itf_msg *(*get_msg_prio)(struct icpu_msg_queue *queue, u32 prio);
	void (*set_msg)(struct icpu_msg_queue *queue, struct icpu_itf_msg *msg);
	u32 (*len)(struct icpu_msg_queue *queue);
	void (*dump)(struct icpu_msg_queue *queue);
};

int icpu_msg_queue_init(struct icpu_msg_queue *queue, u32 num_prio);
void icpu_msg_queue_deinit(struct icpu_msg_queue *queue);

#define QUEUE_INIT(q) icpu_msg_queue_init(q, 1)
#define PRIORITY_QUEUE_INIT(q, n) icpu_msg_queue_init(q, n)
#define QUEUE_DEINIT(q) icpu_msg_queue_deinit(q)

#define QUEUE_LEN(q) (q && (q)->len ? (q)->len(q) : 0)

#define QUEUE_GET_MSG(q)									\
	({ struct icpu_itf_msg *_msg = NULL;							\
	do {											\
		unsigned long _lock_flags;							\
		if (!q || !(q)->get_msg) break;							\
		spin_lock_irqsave(&(q)->lock, _lock_flags);					\
		_msg = (q)->get_msg(q);								\
		spin_unlock_irqrestore(&(q)->lock, _lock_flags);				\
	} while(0);										\
	_msg; })

#define QUEUE_GET_MSG_KEY(q, k) \
	({ struct icpu_itf_msg *_msg = NULL;							\
	do {											\
		unsigned long _lock_flags;							\
		if (!q || !(q)->get_msg_key) break;						\
		spin_lock_irqsave(&(q)->lock, _lock_flags);					\
		_msg = (q)->get_msg_key(q, k);							\
		spin_unlock_irqrestore(&(q)->lock, _lock_flags);				\
	} while(0);										\
	_msg;})

#define QUEUE_SET_MSG(q, m)									\
	do {											\
		unsigned long _lock_flags;							\
		if (!q || !m || !(q)->set_msg) break;						\
		spin_lock_irqsave(&(q)->lock, _lock_flags);					\
		(q)->set_msg(q, m);								\
		spin_unlock_irqrestore(&(q)->lock, _lock_flags);				\
	} while(0)

#define QUEUE_SET_MSG_IF_NOT_EMPTY(q, m)							\
	({ int _ret = 0;									\
	do {											\
		unsigned long _lock_flags;							\
		if (!q || !m) break;								\
		spin_lock_irqsave(&(q)->lock, _lock_flags);					\
		if ((q)->msg_cnt > 0 && (q)->set_msg) {						\
			(q)->set_msg(q, m);							\
			_ret = 1;								\
		}										\
		spin_unlock_irqrestore(&(q)->lock, _lock_flags);				\
	} while(0);										\
	_ret; })

#define MSG_DUMP(m)										\
	do {											\
		int i;										\
		for(i = 0; i < (m)->len; i++)							\
			ICPU_INFO("\tmsg->data[%d]=0x%x(dec.%d)", i, (m)->data[i], (m)->data[i]);	\
	} while(0)

#define QUEUE_DUMP(q) { if (q && (q)->dump) (q)->dump(q); }

#endif
