// SPDX-License-Identifier: GPL-2.0
#include <kunit/test.h>
#include <kunit/mock.h>

#include <linux/netdevice.h>
#include <net/protocol.h>
#include <linux/skbuff.h>
#include <linux/rbtree.h>
#include <net/net_namespace.h>
#include <net/request_sock.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <net/skb_tracer.h>

#include "../sock_queue_printer.h"
#include "../skb_queue_printer.h"
#include "../rb_queue_printer.h"

struct kunit *g_test;

/* declare static mockable functions here
 * because they don't have header files.
 */
DECLARE_REDIRECT_MOCKABLE(print_sock_info, RETURNS(void),
			PARAMS(struct sock *, struct task_struct *));
DECLARE_REDIRECT_MOCKABLE(skb_tracer_err_report, RETURNS(void),
			PARAMS(struct sock *));

DECLARE_REDIRECT_MOCKABLE(sock_queue_printer_init, RETURNS(void),
			PARAMS(struct sock_queue_printer *, void *));
DECLARE_REDIRECT_MOCKABLE(sock_queue_printer_print, RETURNS(void),
			PARAMS(struct sock_queue_printer *));
DECLARE_REDIRECT_MOCKABLE(sock_queue_printer_print_skb, RETURNS(void),
			PARAMS(struct sk_buff *));

DECLARE_REDIRECT_MOCKABLE(skb_queue_traverse, RETURNS(void),
			PARAMS(struct sock_queue_printer *));
DECLARE_REDIRECT_MOCKABLE(skb_queue_printer_init, RETURNS(void),
			PARAMS(struct skb_queue_printer *, struct sock *));

DECLARE_REDIRECT_MOCKABLE(rb_queue_traverse, RETURNS(void),
			PARAMS(struct sock_queue_printer *));
DECLARE_REDIRECT_MOCKABLE(rb_queue_printer_init, RETURNS(void),
			PARAMS(struct rb_queue_printer *, struct sock *));

/* define mockable functions  */
DEFINE_FUNCTION_MOCK_VOID_RETURN(print_sock_info,
			PARAMS(struct sock *, struct task_struct *));
DEFINE_FUNCTION_MOCK_VOID_RETURN(skb_tracer_err_report,
			PARAMS(struct sock *));

DEFINE_FUNCTION_MOCK_VOID_RETURN(sock_queue_printer_init,
			PARAMS(struct sock_queue_printer *, void *));
DEFINE_FUNCTION_MOCK_VOID_RETURN(sock_queue_printer_print,
			PARAMS(struct sock_queue_printer *));
DEFINE_FUNCTION_MOCK_VOID_RETURN(sock_queue_printer_print_skb,
			PARAMS(struct sk_buff *));

DEFINE_FUNCTION_MOCK_VOID_RETURN(skb_queue_traverse,
			PARAMS(struct sock_queue_printer *));
DEFINE_FUNCTION_MOCK_VOID_RETURN(skb_queue_printer_init,
			PARAMS(struct skb_queue_printer *, struct sock *));

DEFINE_FUNCTION_MOCK_VOID_RETURN(rb_queue_traverse,
			PARAMS(struct sock_queue_printer *));
DEFINE_FUNCTION_MOCK_VOID_RETURN(rb_queue_printer_init,
			PARAMS(struct rb_queue_printer *, struct sock *));

/* other externs */
extern void sk_error_report_callback(void *ignore, const struct sock *err_sk);
extern int fd_from_sk(struct sock *sk, struct task_struct *task);
extern struct skb_tracer *skb_tracer_alloc(void);
extern int skb_tracer_uid_filter;

/* mocking 'print_sock_info' */
void REAL_ID(mock_print_sock_info)(struct sock *sk, struct task_struct *task)
{
	/* do nothing */
	kunit_info(g_test, "mocked\n");
}
DEFINE_INVOKABLE(mock_print_sock_info,
		void, NO_ASSIGN, PARAMS(struct sock *, struct task_struct *));

void test_skb_tracer_init(struct kunit *test)
{
	struct mock_expectation *exp_print_info;
	struct inet_sock *test_inet_sock;
	struct sock *test_sk;
	struct socket *test_socket = NULL;
	int err;
	int test_uid = 10245;

	/* Pre-requisites */
	/* create a inet sock and reserve dest port to use 5060.
	 * this 'inet sock' is compatible with 'sock'
	 */
	test_inet_sock = kunit_kzalloc(test, sizeof(struct inet_sock),
					GFP_KERNEL);
	if (!test_inet_sock)
		return;
	test_inet_sock->inet_dport = ntohs(5060);
	test_sk = (struct sock *)test_inet_sock;
	test_sk->sk_uid.val = test_uid;

	err = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
				&test_socket);
	if (err < 0)
		goto test_skb_tracer_init_sock_err;

	exp_print_info = Times(3, EXPECT_CALL(print_sock_info(
					ptr_eq(test, test_sk),
					ptr_eq(test, current))));
	ActionOnMatch(exp_print_info, INVOKE_REAL(test, mock_print_sock_info));

	/* 1. even though sk_socket is null,
	 *    expect not being crashed here. */
	test_sk->sk_socket = NULL;
	skb_tracer_init(test_sk);
	KUNIT_EXPECT_EQ(test, (u64)test_sk->sk_socket, 0);

	/* 2. expect not assigned new mask. */
	test_sk->sk_socket = test_socket;
	test_sk->sk_tracer_mask = 0x10;
	skb_tracer_init(test_sk);
	KUNIT_EXPECT_EQ(test, test_sk->sk_tracer_mask, 0x10);

	/* 3. expect normally assigned new mask. */
	test_sk->sk_tracer_mask = 0;
	skb_tracer_init(test_sk);
	KUNIT_EXPECT_EQ(test, test_sk->sk_tracer_mask, sock_i_ino(test_sk));

	/* 4. expect normally assigned new mask even in sport = 5060 */
	test_sk->sk_tracer_mask = 0;
	test_inet_sock->inet_dport = 0;
	test_inet_sock->inet_sport = ntohs(5060);
	skb_tracer_init(test_sk);
	KUNIT_EXPECT_EQ(test, test_sk->sk_tracer_mask, sock_i_ino(test_sk));

	/* 5. check if uid_filter works with test uid = 10245 */
	test_sk->sk_tracer_mask = 0;
	skb_tracer_uid_filter = test_uid;
	skb_tracer_init(test_sk);
	KUNIT_EXPECT_EQ(test, test_sk->sk_tracer_mask, sock_i_ino(test_sk));

	/* Post-requisites */
	kernel_sock_shutdown(test_socket, SHUT_RDWR);
	sock_release(test_socket);

test_skb_tracer_init_sock_err:
	kunit_kfree(test, test_inet_sock);
}

/* mocking 'skb_tracer_print_skb_queue' */
void REAL_ID(mock_skb_tracer_err_report)(struct sock *sk)
{
	/* do nothing */
	kunit_info(g_test, "%s: mock invoked", __func__);
}
DEFINE_INVOKABLE(mock_skb_tracer_err_report,
	void, NO_ASSIGN, PARAMS(struct sock *));

void test_sk_error_report_callback(struct kunit *test)
{
	struct sock *test_sk;
	struct mock_expectation *exp1, *exp2;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		goto test_sk_failed;

	/* 1. In this call, skb_tracer_err_report MUST not be called */
	exp1 = RetireOnSaturation(
		Never(EXPECT_CALL(skb_tracer_err_report(any(test))))
	);
	ActionOnMatch(exp1, INVOKE_REAL(test, mock_skb_tracer_err_report));
	sk_error_report_callback(NULL, (const struct sock *)test_sk);

	/* 2. In this time, skb_tracer_err_report MUST be called */
	exp2 = RetireOnSaturation(
		Times(1, EXPECT_CALL(skb_tracer_err_report(
					ptr_eq(test, test_sk))))
	);
	ActionOnMatch(exp2, INVOKE_REAL(test, mock_skb_tracer_err_report));
	test_sk->sk_tracer_mask = 0x1234;
	sk_error_report_callback(NULL, (const struct sock *)test_sk);

	KUNIT_EXPECT_TRUE(test, 1);

test_sk_failed:
	kunit_kfree(test, test_sk);
}

void REAL_ID(mock_skb_queue_printer_init)(
		struct skb_queue_printer *p, struct sock *sk)
{
	/* do nothing */
}
DEFINE_INVOKABLE(mock_skb_queue_printer_init, void, NO_ASSIGN,
		PARAMS(struct skb_queue_printer *, struct sock *));

void REAL_ID(mock_rb_queue_printer_init)(
		struct rb_queue_printer *p, struct sock *sk)
{
	/* do nothing */
}
DEFINE_INVOKABLE(mock_rb_queue_printer_init, void, NO_ASSIGN,
		PARAMS(struct rb_queue_printer *, struct sock *));

void REAL_ID(mock_sock_queue_printer_print)(struct sock_queue_printer *p)
{
	/* do nothing */
}
DEFINE_INVOKABLE(mock_sock_queue_printer_print, void, NO_ASSIGN,
		PARAMS(struct sock_queue_printer *));

void test_skb_tracer_err_report(struct kunit *test)
{
	struct mock_expectation *exp_queue, *exp_rbtree, *exp;
	struct sock *test_sk;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;

	exp_queue = Times(1, EXPECT_CALL(skb_queue_printer_init(
					any(test),
					ptr_eq(test, test_sk))));
	ActionOnMatch(exp_queue,
			INVOKE_REAL(test, mock_skb_queue_printer_init));

	exp_rbtree = Times(1, EXPECT_CALL(rb_queue_printer_init(
					any(test),
					ptr_eq(test, test_sk))));
	ActionOnMatch(exp_rbtree,
			INVOKE_REAL(test, mock_rb_queue_printer_init));

	exp = Times(2, EXPECT_CALL(sock_queue_printer_print(any(test))));
	ActionOnMatch(exp, INVOKE_REAL(test, mock_sock_queue_printer_print));

	/* test */
	skb_tracer_err_report(test_sk);
	KUNIT_EXPECT_TRUE(test, 1);

	/* Post-requisites */
	kunit_kfree(test, test_sk);
}

void test_print_sock_info(struct kunit *test)
{
	struct sock *test_sk;
	struct inet_sock *test_inet;
	struct task_struct *test_task = current;
	struct socket *test_socket = NULL;
	int err;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct inet_sock), GFP_KERNEL);
	if (!test_sk) {
		kunit_err(g_test, "%s: failed to alloc inet_sock", __func__);
		return;
	}

	err = sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
				&test_socket);
	if (err < 0) {
		kunit_err(g_test, "%s: failed to alloc socket", __func__);
		goto test_print_sock_info_free_sk;
	}

	test_sk->sk_socket = test_socket;
	test_sk->sk_tracer_mask = 0x1234;

	test_inet = inet_sk(test_sk);
	test_inet->inet_sport = 0x1010;
	test_inet->inet_dport = 0x2020;

	/* 1. */
	print_sock_info(test_sk, test_task);
	KUNIT_EXPECT_TRUE(test, 1);

	/* Post-requisites */
	kernel_sock_shutdown(test_socket, SHUT_RDWR);
	sock_release(test_socket);

test_print_sock_info_free_sk:
	kunit_kfree(test, test_sk);
}

int sock_map_fd(struct socket *sock, int flags)
{
	struct file *newfile;
	int fd = get_unused_fd_flags(flags);
	if (unlikely(fd < 0)) {
		sock_release(sock);
		return fd;
	}

	newfile = sock_alloc_file(sock, flags, NULL);
	if (!IS_ERR(newfile)) {
		fd_install(fd, newfile);
		return fd;
	}

	put_unused_fd(fd);
	return PTR_ERR(newfile);
}

void test_fd_from_sk(struct kunit *test)
{
	struct sock *test_sk;
	struct socket *test_socket = NULL;
	struct socket *dummy_sockets[2] = {0, };
	int fd, test_fd;
	int i;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);

	/* create a 'socket' at current task
	 * and also, Two dummy 'socket's.
	 */
	for (i = 0; i < 2; i++) {
		sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
					&dummy_sockets[i]);
	}
	sock_create_kern(&init_net, AF_INET, SOCK_STREAM, IPPROTO_TCP,
				&test_socket);

	for (i = 0; i < 2; i++)
		sock_map_fd(dummy_sockets[i], (O_CLOEXEC | O_NONBLOCK));
	fd = sock_map_fd(test_socket, (O_CLOEXEC | O_NONBLOCK));

	test_sk->sk_socket = test_socket;

	/* call testee */
	test_fd = fd_from_sk(test_sk, current);

	/* check if retrieved 'fd' has a valid index */
	KUNIT_EXPECT_EQ(test, fd, test_fd);

	/* check if retrieved 'fd' has gone after closing */
	if (test_socket)
		sock_release(test_socket);

	fd = fd_from_sk(test_sk, current);

	KUNIT_EXPECT_TRUE(test, fd == 0);

	/* Post-requisites */
	for (i = 0; i < 2; i++)
		sock_release(dummy_sockets[i]);

	kunit_kfree(test, test_sk);
}

void test_skb_tracer_mask_on_skb(struct kunit *test)
{
	struct sock *test_sk;
	struct sk_buff *test_skb;
	struct skb_tracer *tracer;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;
	test_skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	if (!test_skb)
		goto test_sk_failed;

	/* 1. since sk is filled with 0, it MUST do nothing. */
	test_sk->sk_tracer_mask = 0;
	skb_tracer_mask_on_skb(test_sk, test_skb);
	KUNIT_EXPECT_PTR_EQ(test, test_skb->tracer_obj, NULL);

	/* 2. after adding some test mask value on a sk and invoke the func,
	 *    skb MUST have same mask value as sk's.
	 */
	test_sk->sk_tracer_mask = 0x4214;
	skb_tracer_mask_on_skb(test_sk, test_skb);
	tracer = test_skb->tracer_obj;

	KUNIT_EXPECT_NOT_ERR_OR_NULL(test, tracer);
	KUNIT_EXPECT_EQ(test, tracer->sock_mask, 0x4214);
	KUNIT_EXPECT_EQ(test, tracer->skb_mask, 0);

	/* Post-requisites */
	skb_tracer_put(test_skb);

	kunit_kfree(test, test_sk);
test_sk_failed:
	kunit_kfree(test, test_skb);
}

void test_sock_queue_printer_print_skb(struct kunit *test)
{
	struct sk_buff *test_skb;
	struct skb_tracer *tracer;

	/* Pre-requisites */
	test_skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	if (!test_skb)
		return;

	test_skb->len = 10;
	test_skb->data_len = 10;

	TCP_SKB_CB(test_skb)->seq = 0x12345600;
	TCP_SKB_CB(test_skb)->end_seq = 0x12345678;
	TCP_SKB_CB(test_skb)->ack_seq = 0x21436587;
	TCP_SKB_CB(test_skb)->tcp_flags = 0x10;
	TCP_SKB_CB(test_skb)->sacked = TCPCB_SACKED_ACKED
					| TCPCB_SACKED_RETRANS;

	/* 1. check in case of null-tracer.
	      It's just fine if it does not cause any panic!
	 */
	sock_queue_printer_print_skb(test_skb);
	KUNIT_EXPECT_TRUE(test, 1);

	/* 2. It's just fine if it does not cause any panic! */
	tracer = skb_tracer_alloc();
	tracer->skb_mask = 0x1234;
	tracer->sock_mask = 0x5678;

	sock_queue_printer_print_skb(test_skb);
	KUNIT_EXPECT_TRUE(test, 1);

	/* Post-requisites */
	skb_tracer_put(test_skb);
	kunit_kfree(test, test_skb);
}

void REAL_ID(mock_skb_queue_traverse)(struct sock_queue_printer *p)
{
	/* do nothing */
}
DEFINE_INVOKABLE(mock_skb_queue_traverse, void, NO_ASSIGN,
		PARAMS(struct sock_queue_printer *));

void test_sock_queue_printer_print(struct kunit *test)
{
	struct mock_expectation *exp;
	struct sock_queue_printer test_printer;

	test_printer.traverse_queue = skb_queue_traverse;

	exp = Times(1, EXPECT_CALL(skb_queue_traverse(
					ptr_eq(test, &test_printer))));
	ActionOnMatch(exp,
			INVOKE_REAL(test, mock_skb_queue_traverse));

	/* check if 'skb_queue_traverse' is really called
	 * with 'test_printer'
	 */
	sock_queue_printer_print(&test_printer);
	KUNIT_EXPECT_TRUE(test, true);
}

void test_sock_queue_printer_init(struct kunit *test)
{
	struct sock *test_sk;
	struct skb_queue_printer skb_q_printer;
	struct sock_queue_printer *test_printer =
			(struct sock_queue_printer *)&skb_q_printer;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;

	sock_queue_printer_init(test_printer, &test_sk->sk_write_queue);

	KUNIT_EXPECT_PTR_EQ(test, test_printer->queue, &test_sk->sk_write_queue);
	KUNIT_EXPECT_PTR_EQ(test, test_printer->print_skb,
				(void *)sock_queue_printer_print_skb);

	/* Post-requisites */
	kunit_kfree(test, test_sk);
}

void test_skb_tracer_func_trace(struct kunit *test)
{
	struct sock *test_sock;
	struct sk_buff *test_skb;
	struct skb_tracer *tracer;
	u64 mask = 0x00040;

	/* Pre-requisites */
	test_sock = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	test_skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	/* 1. For case, skb doesn't have 'tracer' object. */
	skb_tracer_func_trace(test_sock, test_skb, STL_IP6_OUTPUT);
	tracer = test_skb->tracer_obj;
	KUNIT_EXPECT_TRUE(test, !tracer);

	/* 2. For case, skb has 'tracer' object. */
	test_sock->sk_tracer_mask = mask;
	skb_tracer_mask_on_skb(test_sock, test_skb);
	skb_tracer_mask_with(test_skb, &test_sock->sk_write_queue);
	skb_tracer_func_trace(test_sock, test_skb, STL_IP6_OUTPUT);
	tracer = test_skb->tracer_obj;
	KUNIT_ASSERT_TRUE(test, !!tracer);
	KUNIT_EXPECT_EQ(test, STL_IP6_OUTPUT, tracer->skb_mask);

	/* Post-requisites */
	skb_tracer_put(test_skb);
	kunit_kfree(test, test_sock);
	kunit_kfree(test, test_skb);
}

void test_skb_tracer_mask_unmask_with(struct kunit *test)
{
	struct sock *test_sock;
	struct sk_buff *test_skb;
	struct skb_tracer *tracer;

	/* Pre-requisites */
	test_sock = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	test_skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	test_skb->sk = test_sock;
	test_sock->sk_tracer_mask = 0x10;
	skb_tracer_mask_on_skb(test_sock, test_skb);

	/* 1. call "skb_tracer_mask",
	 *    test if it has a propper mask value.
	 *    unmask and then test if it's value is back to 0. */
	skb_tracer_mask_with(test_skb, &test_sock->sk_write_queue);

	tracer = test_skb->tracer_obj;
	KUNIT_EXPECT_TRUE(test, !!tracer);

	KUNIT_EXPECT_EQ(test, tracer->queue_mask, (1 << STQ_WRITE_QUEUE));
	skb_tracer_unmask_with(test_skb, &test_sock->sk_write_queue);
	KUNIT_EXPECT_EQ(test, tracer->queue_mask, 0);

	/* 2. check if it works as same when the queue is rb_queue */
	skb_tracer_mask_with(test_skb, &test_sock->tcp_rtx_queue);

	tracer = test_skb->tracer_obj;
	KUNIT_EXPECT_TRUE(test, !!tracer);

	KUNIT_EXPECT_EQ(test, tracer->queue_mask, (1 << STQ_RTX_QUEUE));
	skb_tracer_unmask_with(test_skb, &test_sock->tcp_rtx_queue);
	KUNIT_EXPECT_EQ(test, tracer->queue_mask, 0);

	/* Post-requisites */
	kunit_kfree(test, test_sock);
	kunit_kfree(test, test_skb);
}

void REAL_ID(mock_sock_queue_printer_init)(struct sock_queue_printer *p,
		void *q)
{
	/* do nothing */
}
DEFINE_INVOKABLE(mock_sock_queue_printer_init, void, NO_ASSIGN,
		PARAMS(struct sock_queue_printer *, void *));

void test_skb_queue_printer_init(struct kunit *test)
{
	struct skb_queue_printer test_skb_queue_printer;
	struct sock_queue_printer *test_printer =
			(struct sock_queue_printer *)&test_skb_queue_printer;
	struct sock *test_sk;
	struct mock_expectation *exp;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;

	exp = Times(1, EXPECT_CALL(sock_queue_printer_init(
				ptr_eq(test, test_printer),
				ptr_eq(test, &test_sk->sk_write_queue))));
	ActionOnMatch(exp, INVOKE_REAL(test, mock_sock_queue_printer_init));

	/* check if 'sock_queue_printer_init' is called once and,
	 * check if traverse_queue is skb_queue_traverse
	 */
	skb_queue_printer_init(&test_skb_queue_printer, test_sk);

	KUNIT_EXPECT_PTR_EQ(test, test_printer->traverse_queue,
				(void *)skb_queue_traverse);

	/* Post-requisites */
	kunit_kfree(test, test_sk);
}

/* mocking 'skb_tracer_print_skb_queue' */
void REAL_ID(mock_sock_queue_printer_print_skb)(struct sk_buff *skb)
{
	/* do nothing */
}
DEFINE_INVOKABLE(mock_sock_queue_printer_print_skb, void, NO_ASSIGN,
			PARAMS(struct sk_buff *));

void test_skb_queue_traverse(struct kunit *test)
{
	int i;
	struct sk_buff *test_skb[4];
	struct skb_queue_printer test_skb_printer;
	struct sock_queue_printer *test_printer = &test_skb_printer.base;
	struct mock_expectation *exp, *exps[4];
	struct sock *test_sk;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;

	test_sk->sk_tracer_mask = 0x10;

	/* 1. test for empty queue. */
	exp = RetireOnSaturation(
		Never(EXPECT_CALL(sock_queue_printer_print_skb(any(test))))
	);
	ActionOnMatch(exp, INVOKE_REAL(test,
				mock_sock_queue_printer_print_skb));

	skb_queue_head_init(&test_sk->sk_write_queue);

	skb_queue_printer_init(&test_skb_printer, test_sk);
	skb_queue_traverse(test_printer);
	KUNIT_EXPECT_TRUE(test, 1);

	/* 2. enqueue 4 skbs, and make each expectation. */
	for (i = 0; i < ARRAY_SIZE(test_skb); i++) {
		test_skb[i] = kunit_kzalloc(test, sizeof(struct sk_buff),
						GFP_KERNEL);
		test_skb[i]->tracer_obj = skb_tracer_alloc();

		/* append each skb on test_queue */
		skb_queue_tail(&test_sk->sk_write_queue, test_skb[i]);

		/* each skb MUST be invoked in sequence */
		exps[i] = Times(1,
				EXPECT_CALL(sock_queue_printer_print_skb(
						ptr_eq(test, test_skb[i])))
		);
	}

	InSequence(test, exps[0], exps[1], exps[2], exps[3]);
	ActionOnMatch(exps[0], INVOKE_REAL(test,
					mock_sock_queue_printer_print_skb));
	ActionOnMatch(exps[1], INVOKE_REAL(test,
					mock_sock_queue_printer_print_skb));
	ActionOnMatch(exps[2], INVOKE_REAL(test,
					mock_sock_queue_printer_print_skb));
	ActionOnMatch(exps[3], INVOKE_REAL(test,
					mock_sock_queue_printer_print_skb));

	skb_queue_traverse(test_printer);
	KUNIT_EXPECT_TRUE(test, 1);

	/* Post-requisites */
	for (i = 0; i < ARRAY_SIZE(test_skb); i++) {
		skb_tracer_put(test_skb[i]);
		kunit_kfree(test, test_skb[i]);
	}

	kunit_kfree(test, test_sk);
}

void test_rb_queue_printer_init(struct kunit *test)
{
	struct rb_queue_printer test_rb_queue_printer;
	struct sock_queue_printer *test_printer =
			(struct sock_queue_printer *)&test_rb_queue_printer;
	struct sock *test_sk;
	struct mock_expectation *exp;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;

	exp = Times(1, EXPECT_CALL(sock_queue_printer_init(
				ptr_eq(test, test_printer),
				ptr_eq(test, &test_sk->tcp_rtx_queue))));
	ActionOnMatch(exp, INVOKE_REAL(test, mock_sock_queue_printer_init));

	/* check if 'sock_queue_printer_init' is called once and,
	 * check if traverse_queue is skb_queue_traverse
	 */
	rb_queue_printer_init(&test_rb_queue_printer, test_sk);

	KUNIT_EXPECT_PTR_EQ(test, test_printer->traverse_queue,
				(void *)rb_queue_traverse);

	/* Post-requisites */
	kunit_kfree(test, test_sk);
}

void test_rb_queue_traverse(struct kunit *test)
{
	int i;
	struct sk_buff *test_skb[4];
	struct rb_queue_printer test_rb_printer;
	struct sock_queue_printer *test_printer = &test_rb_printer.base;
	struct mock_expectation *exp, *exps[4];
	struct sock *test_sk;

	/* Pre-requisites */
	test_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	if (!test_sk)
		return;

	test_sk->tcp_rtx_queue = RB_ROOT;
	test_sk->sk_tracer_mask = 0x10;

	/* 1. test for empty queue. */
	exp = RetireOnSaturation(
		Never(EXPECT_CALL(sock_queue_printer_print_skb(any(test))))
	);
	ActionOnMatch(exp, INVOKE_REAL(test,
				mock_sock_queue_printer_print_skb));

	rb_queue_printer_init(&test_rb_printer, test_sk);
	rb_queue_traverse(test_printer);
	KUNIT_EXPECT_TRUE(test, 1);

	/* 2. enqueue 4 skbs, and make each expectation. */
	for (i = 0; i < 4; i++) {
		test_skb[i] = kunit_kzalloc(test, sizeof(struct sk_buff),
						GFP_KERNEL);
		test_skb[i]->tracer_obj = skb_tracer_alloc();
		TCP_SKB_CB(test_skb[i])->seq = i;

		/* append each skb on test_rb */
		tcp_rbtree_insert(&test_sk->tcp_rtx_queue, test_skb[i]);

		/* each skb MUST be invoked in sequence */
		exps[i] = Times(1,
				EXPECT_CALL(sock_queue_printer_print_skb(
						ptr_eq(test, test_skb[i])))
		);
	}

	InSequence(test, exps[0], exps[1], exps[2], exps[3]);
	ActionOnMatch(exps[0], INVOKE_REAL(test,
					mock_sock_queue_printer_print_skb));
	ActionOnMatch(exps[1], INVOKE_REAL(test,
					mock_sock_queue_printer_print_skb));
	ActionOnMatch(exps[2], INVOKE_REAL(test,
					mock_sock_queue_printer_print_skb));
	ActionOnMatch(exps[3], INVOKE_REAL(test,
					mock_sock_queue_printer_print_skb));

	rb_queue_traverse(test_printer);
	KUNIT_EXPECT_TRUE(test, 1);

	/* Post-requisites */
	for (i = 0; i < 4; i++) {
		skb_tracer_put(test_skb[i]);
		kunit_kfree(test, test_skb[i]);
	}

	kunit_kfree(test, test_sk);
}

extern struct request_sock_ops tcp_request_sock_ops;

void test_small_sk_check(struct kunit *test)
{
	struct request_sock *test_request_sk;
	struct sock *test_listen_sk;
	struct sock *test_sk;
	struct sk_buff *test_skb;
	struct skb_tracer *tracer;

	/* Pre-requisites */
	test_skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	KUNIT_ASSERT_TRUE(test, !!test_skb);

	test_listen_sk = kunit_kzalloc(test, sizeof(struct sock), GFP_KERNEL);
	KUNIT_ASSERT_TRUE(test, !!test_listen_sk);

	test_request_sk = inet_reqsk_alloc(&tcp_request_sock_ops,
						test_listen_sk, false);
	KUNIT_ASSERT_TRUE(test, !!test_request_sk);

	test_sk = (struct sock *)test_request_sk;

	test_skb->tracer_obj = skb_tracer_alloc();
	tracer = test_skb->tracer_obj;
	KUNIT_ASSERT_TRUE(test,!!tracer);
	KUNIT_ASSERT_EQ(test, 1, refcount_read(&tracer->refcnt));

	/* test with small size of sock(like request_sock)
	 * skb_tracer_func_trace MUST do nothing.
	 */
	KUNIT_ASSERT_GT(test, offsetof(struct sock, sk_tracer_mask),
						sizeof(struct request_sock));
	skb_tracer_func_trace(test_sk, test_skb, STL_IP6_OUTPUT);
	KUNIT_EXPECT_EQ(test, 0, tracer->skb_mask);

	/* Post-requisites */
	skb_tracer_put(test_skb);
	reqsk_put(test_request_sk);
	kunit_kfree(test, test_listen_sk);
	kunit_kfree(test, test_skb);
}

#define MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(func_name) \
		mock_set_default_action(mock_get_global_mock(), \
			(#func_name), func_name, INVOKE_REAL(test, func_name))

static int skb_tracer_test_init(struct kunit *test)
{
	g_test = test;

	/* set default action as INVOKE_REAL() */
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(print_sock_info);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(skb_tracer_err_report);

	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(sock_queue_printer_init);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(sock_queue_printer_print);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(sock_queue_printer_print_skb);

	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(skb_queue_traverse);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(skb_queue_printer_init);

	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(rb_queue_traverse);
	MOCK_SET_DEFAULT_ACTION_INVOKE_REAL(rb_queue_printer_init);

	return 0;
}

static void skb_tracer_test_exit(struct kunit *test)
{
	g_test = NULL;
}

static struct kunit_case skb_tracer_test_cases[] = {
	KUNIT_CASE(test_skb_tracer_init),
	KUNIT_CASE(test_sk_error_report_callback),
	KUNIT_CASE(test_skb_tracer_err_report),
	KUNIT_CASE(test_print_sock_info),
	KUNIT_CASE(test_fd_from_sk),
	KUNIT_CASE(test_skb_tracer_func_trace),
	KUNIT_CASE(test_skb_tracer_mask_unmask_with),
	KUNIT_CASE(test_skb_tracer_mask_on_skb),

	KUNIT_CASE(test_sock_queue_printer_print_skb),
	KUNIT_CASE(test_sock_queue_printer_print),
	KUNIT_CASE(test_sock_queue_printer_init),

	KUNIT_CASE(test_skb_queue_printer_init),
	KUNIT_CASE(test_skb_queue_traverse),

	KUNIT_CASE(test_rb_queue_printer_init),
	KUNIT_CASE(test_rb_queue_traverse),

	KUNIT_CASE(test_small_sk_check),
	{},
};

static struct kunit_suite skb_tracer_test_module = {
	.name = "skb_tracer_test",
	.init = skb_tracer_test_init,
	.exit = skb_tracer_test_exit,
	.test_cases = skb_tracer_test_cases,
};

kunit_test_suites(&skb_tracer_test_module);
