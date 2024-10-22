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
#include <net/tcp.h>
#include <net/skb_tracer.h>
#include "sock_queue_printer.h"

#define __SOCK_QUEUE_PRINTER_C__
#include "test/skb_tracer-test.h"

STATIC void MOCKABLE(sock_queue_printer_print_skb)(struct sk_buff *skb)
{
	struct skb_tracer *tracer = skb->tracer_obj;

	if (!tracer)
		return;

	pr_info("mask=(0x%llx, 0x%llx), len=%d, data_len=%d "
		"seq=(%u, %u), ack=%u, flags=0x%x, sacked=0x%x\n",
			tracer->skb_mask, tracer->sock_mask,
			skb->len, skb->data_len,
			TCP_SKB_CB(skb)->seq, TCP_SKB_CB(skb)->end_seq,
			TCP_SKB_CB(skb)->ack_seq,
			TCP_SKB_CB(skb)->tcp_flags, TCP_SKB_CB(skb)->sacked);
}

void MOCKABLE(sock_queue_printer_print)(struct sock_queue_printer *this)
{
	this->traverse_queue(this);
}

void MOCKABLE(sock_queue_printer_init)(struct sock_queue_printer *this, void *queue)
{
	this->queue = queue;
	this->print_skb = sock_queue_printer_print_skb;
}
