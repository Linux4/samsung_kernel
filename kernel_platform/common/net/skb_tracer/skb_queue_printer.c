// SPDX-License-Identifier: GPL-2.0-or-later

#if !IS_ENABLED(CONFIG_KUNIT_TEST)
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/skb_tracer.h>
#include "sock_queue_printer.h"
#include "skb_queue_printer.h"

#define __SKB_QUEUE_PRINTER_C__
#include "test/skb_tracer-test.h"

STATIC void MOCKABLE(skb_queue_traverse)(struct sock_queue_printer *this)
{
	unsigned long flags;
	struct sk_buff *skb, *tmp;
	struct sk_buff_head *queue = (struct sk_buff_head *)this->queue;

	if (skb_queue_empty(queue))
		return;

	spin_lock_irqsave(&queue->lock, flags);

	pr_info("write_queue len=%d\n", queue->qlen);

	skb = skb_peek(queue);
	skb_queue_walk_from_safe(queue, skb, tmp) {
		this->print_skb(skb);
	}

	spin_unlock_irqrestore(&queue->lock, flags);
}

void MOCKABLE(skb_queue_printer_init)(struct skb_queue_printer *this, struct sock *sk)
{
	sock_queue_printer_init(&(this->base), &sk->sk_write_queue);
	this->base.traverse_queue = skb_queue_traverse;
}
