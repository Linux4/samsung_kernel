/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _SKB_QUEUE_PRINTER_H
#define _SKB_QUEUE_PRINTER_H

struct sock_queue_printer;

struct skb_queue_printer {
	struct sock_queue_printer base;
};

void skb_queue_printer_init(struct skb_queue_printer *this, struct sock *sk);

#endif	/* _SKB_QUEUE_PRINTER_H */
