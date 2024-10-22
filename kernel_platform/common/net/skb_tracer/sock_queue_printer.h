/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _SOCK_QUEUE_PRINTER_H
#define _SOCK_QUEUE_PRINTER_H

struct sk_buff;
struct sock_queue_printer;

typedef void (*print_skb_t)(struct sk_buff *);
typedef void (*traverse_t)(struct sock_queue_printer *this);

struct sock_queue_printer {
	void *queue;

	print_skb_t print_skb;
	traverse_t traverse_queue;
};

void sock_queue_printer_init(struct sock_queue_printer *this, void *queue);
void sock_queue_printer_print(struct sock_queue_printer *this);

#endif	/* _SOCK_QUEUE_PRINTER_H */