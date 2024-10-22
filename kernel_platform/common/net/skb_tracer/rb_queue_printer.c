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
#include "rb_queue_printer.h"

#define __RB_QUEUE_PRINTER_C__
#include "test/skb_tracer-test.h"

STATIC void MOCKABLE(rb_queue_traverse)(struct sock_queue_printer *this)
{
	struct sk_buff *skb, *tmp;
	struct rb_root *queue = (struct rb_root *)this->queue;

	if (RB_EMPTY_ROOT(queue))
		return;

	pr_info("rtx_queue\n");

	skb = skb_rb_first(queue);
	skb_rbtree_walk_from_safe(skb, tmp) {
		this->print_skb(skb);
	}
}

void MOCKABLE(rb_queue_printer_init)(struct rb_queue_printer *this, struct sock *sk)
{
	sock_queue_printer_init(&(this->base), &sk->tcp_rtx_queue);
	this->base.traverse_queue = rb_queue_traverse;
}
