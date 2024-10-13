/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _RB_QUEUE_PRINTER_H
#define _RB_QUEUE_PRINTER_H

//#include "sock_queue_printer.h"
struct sock_queue_printer;

struct rb_queue_printer {
	struct sock_queue_printer base;
};

void rb_queue_printer_init(struct rb_queue_printer *this, struct sock *sk);

#endif	/* _RB_QUEUE_PRINTER_H */
