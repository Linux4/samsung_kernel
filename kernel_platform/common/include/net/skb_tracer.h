/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _SKB_TRACER_H
#define _SKB_TRACER_H

#include <linux/kernel.h>

struct sk_buff;
struct sock;
struct spinlock_t;

struct skb_tracer {
	u64 queue_mask;
	u64 sock_mask;
	u64 skb_mask;
	struct sock *sk;
	spinlock_t lock; /* TODO: really need this? */
	refcount_t refcnt;
};

#define ID_TO_MASK(id)		(1 << id)

enum skb_tracer_location {
	STL_TCP_TRANSMIT_SKB 	= ID_TO_MASK(0),
	STL_TCP_WRITE_XMIT	= ID_TO_MASK(1),
	STL___IP_QUEUE_XMIT	= ID_TO_MASK(2),
	STL___IP_LOCAL_OUT	= ID_TO_MASK(3),
	STL_IP6_OUTPUT		= ID_TO_MASK(4),
	STL_IP_OUTPUT		= ID_TO_MASK(5),
	STL_IP6_FINISH_OUTPUT2	= ID_TO_MASK(6),
	STL_IP6_XMIT_01		= ID_TO_MASK(7),
	STL_IP6_XMIT_02		= ID_TO_MASK(8),
};

enum skb_tracer_queue_type {
	STQ_WRITE_QUEUE		= 0,
	STQ_RTX_QUEUE		= 1,
	STQ_OoO_QUEUE		= 2,
};

void skb_tracer_func_trace(const struct sock *sk, struct sk_buff *skb,
				enum skb_tracer_location location);
void skb_tracer_mask_on_skb(struct sock *sk, struct sk_buff *skb);

#if 0
void skb_tracer_mask(struct sk_buff *skb, u64 mask);
void skb_tracer_unmask(struct sk_buff *skb, u64 mask);
#endif

static inline void skb_tracer_get(struct sk_buff *old)
{
	struct skb_tracer *tracer = (struct skb_tracer *)old->tracer_obj;
	
	if (tracer)
		refcount_inc(&tracer->refcnt);
}

static inline void skb_tracer_put(struct sk_buff *skb)
{
	struct skb_tracer *tracer = (struct skb_tracer *)skb->tracer_obj;
	
	if (!tracer)
		return;
	
	if (refcount_read(&tracer->refcnt) == 1)
		goto free_now;

	if (!refcount_dec_and_test(&tracer->refcnt))
		return;

free_now:
	kfree(tracer);
}

static inline void skb_tracer_mask_with(struct sk_buff *skb, void *queue)
{
	struct skb_tracer *tracer = (struct skb_tracer *)skb->tracer_obj;
	struct sock *sk = skb->sk;

	if (!tracer || !tracer->sk || !tracer->sk->sk_tracer_mask)
		return;

	if (skb->sk != tracer->sk) {
		/* what do I do ?*/
	}

	if (queue == (void *)&sk->sk_write_queue)
		tracer->queue_mask = 1 << STQ_WRITE_QUEUE;
	else if (queue == (void *)&sk->tcp_rtx_queue)
		tracer->queue_mask = 1 << STQ_RTX_QUEUE;
}

static inline void skb_tracer_unmask_with(struct sk_buff *skb, void *queue)
{
	struct skb_tracer *tracer = (struct skb_tracer *)skb->tracer_obj;
	struct sock *sk = skb->sk;

	if (!tracer || !tracer->sk || !tracer->sk->sk_tracer_mask)
		return;

	if (skb->sk != tracer->sk) {
		/* what do I do ?*/
	}

	if (queue == (void *)&sk->sk_write_queue)
		tracer->queue_mask &= ~(1 << STQ_WRITE_QUEUE);
	else if (queue == (void *)&sk->tcp_rtx_queue)
		tracer->queue_mask &= ~(1 << STQ_RTX_QUEUE);
}

void skb_tracer_init(struct sock *sk);

#endif	/* _SKB_TRACER_H */
