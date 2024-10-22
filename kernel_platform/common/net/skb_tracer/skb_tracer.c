// SPDX-License-Identifier: GPL-2.0-or-later

#if !IS_ENABLED(CONFIG_KUNIT_TEST)
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/slab.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/netdevice.h>
#include <linux/string.h>
#include <linux/skbuff.h>
#include <linux/fdtable.h>

#include <net/protocol.h>
#include <net/dst.h>
#include <net/sock.h>
#include <net/tcp.h>
#include <net/skb_tracer.h>

#include <linux/uaccess.h>
#include <linux/highmem.h>
#include <linux/capability.h>
#include <linux/user_namespace.h>
#include <linux/netfilter/nf_conntrack_sip.h>

#include <trace/events/skb.h>
#include <trace/events/sock.h>

#include "sock_queue_printer.h"
#include "skb_queue_printer.h"
#include "rb_queue_printer.h"

#define __SKB_TRACER_C__
#include "test/skb_tracer-test.h"

STATIC struct skb_tracer *skb_tracer_alloc(void)
{
	struct skb_tracer *new = kzalloc(sizeof(struct skb_tracer), GFP_ATOMIC);

	if (new)
		refcount_set(&new->refcnt, 1);

	return new;
}

static bool is_propper_sk(const struct sock *sk)
{
	if (!sk || sk->sk_state == TCP_NEW_SYN_RECV ||
	    sk->sk_state == TCP_LISTEN) {
		return false;
	}

	return true;
}

void skb_tracer_func_trace(const struct sock *sk,
		struct sk_buff *skb, enum skb_tracer_location location)
{
	struct skb_tracer *tracer;

	if (!is_propper_sk(sk) || sk->sk_tracer_mask == 0)
		return;

	tracer = (struct skb_tracer *)skb->tracer_obj;
	if (tracer)
		tracer->skb_mask |= location;
}

/* mask a bit on skb when transferring */
void skb_tracer_mask_on_skb(struct sock *sk, struct sk_buff *skb)
{
	struct skb_tracer *tracer;
	if (!sk->sk_tracer_mask)
		return;

	tracer = (struct skb_tracer *)skb->tracer_obj;
	if (tracer)
		return;

	tracer = skb_tracer_alloc();
	if (!tracer)
		return;

	tracer->sock_mask = sk->sk_tracer_mask;
	tracer->queue_mask = 0;
	tracer->skb_mask = 0;
	tracer->sk = sk;
	skb->tracer_obj = tracer;

	spin_lock_init(&tracer->lock);
}

STATIC int find_sk(const void *v, struct file *file, unsigned int n)
{
	struct sock *sk = (void *)v;

	if (file == sk->sk_socket->file)
		return n;

	return 0;
}

STATIC int fd_from_sk(struct sock *sk, struct task_struct *task)
{
	unsigned int fd = 0;

	task_lock(task);
	fd = iterate_fd(task->files, fd, find_sk, sk);
	task_unlock(task);

	return fd;
}

STATIC void MOCKABLE(print_sock_info)(struct sock *sk, struct task_struct *task)
{
	struct socket *sock = sk->sk_socket;
	struct inet_sock *inet = inet_sk(sk);

	if (!sock || !sock->file || !sock->file->f_inode)
		return;

	pr_info("sk(fd:%d) on %s(pid=%d, uid=%d, gid=%d) port(s:%d, d:%d) "
		"by 0x%llx\n",
			fd_from_sk(sk, task),
			task->comm, task->pid, sock->file->f_inode->i_uid.val,
			sock->file->f_inode->i_gid.val,
			ntohs(inet->inet_sport), ntohs(inet->inet_dport),
			sk->sk_tracer_mask);
}

STATIC void MOCKABLE(skb_tracer_err_report)(struct sock *sk)
{
	struct skb_queue_printer skb_q_printer;
	struct rb_queue_printer rb_q_printer;

	skb_queue_printer_init(&skb_q_printer, sk);
	rb_queue_printer_init(&rb_q_printer, sk);

	sock_queue_printer_print((struct sock_queue_printer *)&skb_q_printer);
	sock_queue_printer_print((struct sock_queue_printer *)&rb_q_printer);
}

STATIC void sk_error_report_callback(void *ignore, const struct sock *err_sk)
{
	struct sock *sk = (struct sock *)err_sk;

	if (!sk->sk_tracer_mask)
		return;

	pr_info("sk: mask=0x%llx, sock error=%d\n",
			sk->sk_tracer_mask, sk->sk_err);

	skb_tracer_err_report(sk);
}

STATIC int skb_tracer_uid_filter;
module_param_named(uid_filter, skb_tracer_uid_filter, uint, 0660);
MODULE_PARM_DESC(uid_filter, "uid filter");

void skb_tracer_init(struct sock *sk)
{
	pid_t pid;
	struct task_struct *task;
	struct inet_sock *inet = inet_sk(sk);

	if (!sk->sk_socket)
		return;

	task = current;
	pid = current->pid;

	if (sk->sk_tracer_mask != 0) {
		pr_err("sk(fd:%d) already has mask, 0x%llx\n",
			fd_from_sk(sk, task), sk->sk_tracer_mask);
		return;
	}

	if (skb_tracer_uid_filter &&
		sock_net_uid(sock_net(sk), sk).val == skb_tracer_uid_filter) {
		pr_info("uid_filter(%d): task=(%s,%d)\n", skb_tracer_uid_filter,
				task->comm, task->pid);
		goto skb_tracer_filtered;
	}

	/* filter to SIP protocol */
	switch (ntohs(inet->inet_dport)) {
		case SIP_PORT:	// 5060
		case 6000:
		case 6100 ... 6999:
			goto skb_tracer_filtered;
	}

	switch (ntohs(inet->inet_sport)) {
		case SIP_PORT:	// 5060
		case 6000:
		case 6100 ... 6999:
			break;

		default:
			return;
	}

skb_tracer_filtered:
	sk->sk_tracer_mask = sock_i_ino(sk);
	print_sock_info(sk, task);
}

static int __init init_skb_tracer(void)
{
	int rc = register_trace_inet_sk_error_report(sk_error_report_callback,
							NULL);
	if (rc) {
		pr_err("Failed to register to trace_inet_sk_error_report()");
	}

	return 0;
}

static void exit_skb_tracer(void)
{
	unregister_trace_inet_sk_error_report(sk_error_report_callback, NULL);
}

module_init(init_skb_tracer);
module_exit(exit_skb_tracer);
MODULE_LICENSE("GPL v2");
