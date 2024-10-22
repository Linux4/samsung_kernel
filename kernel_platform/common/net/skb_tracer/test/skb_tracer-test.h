/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef _SKB_TRACER_TEST_H
#define _SKB_TRACER_TEST_H

/* for KUNIT test */
#if IS_ENABLED(CONFIG_KUNIT_TEST)
#include <kunit/test.h>
#include <kunit/mock.h>

extern struct kunit *g_test;

/* kunit define wrapper */
#define STATIC
#define MOCKABLE(func)        	__weak __real__##func

/* define mockable functions */
#ifdef __SKB_TRACER_C__
/* skb_tracer.c */
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(
	print_sock_info, PARAMS(struct sock *, struct task_struct *));
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(
	skb_tracer_err_report, PARAMS(struct sock *));
#endif

#ifdef __SOCK_QUEUE_PRINTER_C__
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(
	sock_queue_printer_init, PARAMS(struct sock_queue_printer *, void *));
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(
	sock_queue_printer_print, PARAMS(struct sock_queue_printer *));
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(
	sock_queue_printer_print_skb, PARAMS(struct sk_buff *));
#endif

#ifdef __SKB_QUEUE_PRINTER_C__
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(
	skb_queue_traverse, PARAMS(struct sock_queue_printer *));
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(
	skb_queue_printer_init, PARAMS(struct skb_queue_printer *, struct sock *));
#endif

#ifdef __RB_QUEUE_PRINTER_C__
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(
	rb_queue_traverse, PARAMS(struct sock_queue_printer *));
DEFINE_REDIRECT_MOCKABLE_VOID_RETURN(
	rb_queue_printer_init, PARAMS(struct rb_queue_printer *, struct sock *));
#endif

#else
#define STATIC			static
#define MOCKABLE(func)		func

#endif

#endif /* _SKB_TRACER_TEST_H */