/*****************************************************************************
 *
 * Copyright (c) 2021 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include "dev.h"
#include "debug.h"
#include "slsi_tracepoint_debug.h"

static DEFINE_MUTEX(slsi_tracepoint_cb_mutex);
static bool slsi_trace_log_enabled;

#define TRACEPOINT_HASH_BITS 5
#define TRACEPOINT_TABLE_SIZE BIT(TRACEPOINT_HASH_BITS)
static struct hlist_head tracepoint_table[TRACEPOINT_TABLE_SIZE];

struct tracepoint_entry {
	struct tracepoint *tp;
	struct list_head probes;
	struct hlist_node hlist;
	char name[0];
};

struct tracepoint_cb {
	struct tracepoint_func tp_func;
	struct list_head list;
};

struct tcp_sk_memory_info {
	bool enabled;
	ktime_t sk_stream_wait_memory_start_time;
	bool is_sock_info;
	int sock_sndbuf;
	int sock_wmem_queued;
};

static struct tracepoint_entry *slsi_get_tracepoint(const char *tracepoint_name)
{
	struct tracepoint_entry *e;
	u32 hash = jhash(tracepoint_name, strlen(tracepoint_name), 0);
	struct hlist_head *head = &tracepoint_table[hash & (TRACEPOINT_TABLE_SIZE - 1)];

	hlist_for_each_entry(e, head, hlist) {
		if (!strcmp(tracepoint_name, e->name))
			return e;
	}
	return NULL;
}

static void slsi_add_tracepoint(struct tracepoint *tp, void *data)
{
	struct hlist_head *head;
	struct tracepoint_entry *e;
	char *tp_name;
	u32 hash;

	if (!data)
		return;

	tp_name = (char *)data;
	if (strcmp(tp->name, tp_name))
		return;

	e = kmalloc(sizeof(struct tracepoint_entry) + (strlen(tp_name) + 1), GFP_KERNEL);
	if (!e)
		return;

	hash = jhash(tp->name, strlen(tp->name), 0);
	head = &tracepoint_table[hash & (TRACEPOINT_TABLE_SIZE - 1)];
	memcpy(e->name, tp_name, strlen(tp_name) + 1);
	e->tp = tp;
	INIT_LIST_HEAD(&e->probes);
	hlist_add_head(&e->hlist, head);
}

static struct tracepoint_entry *slsi_add_new_tracepoint(const char *tp_name)
{
	for_each_kernel_tracepoint(slsi_add_tracepoint, (void *)tp_name);
	return slsi_get_tracepoint(tp_name);
}

int slsi_register_tracepoint_callback(const char *tp_name, void *func, void *data)
{
	struct tracepoint_entry *e;
	struct tracepoint_cb *tp_cb;
	int ret = 0;

	mutex_lock(&slsi_tracepoint_cb_mutex);
	e = slsi_get_tracepoint(tp_name);
	if (!e) {
		e = slsi_add_new_tracepoint(tp_name);
		if (!e) {
			SLSI_ERR_NODEV(" Tracepoint[%s] : Name is not correct.\n", tp_name);
			ret = -EINVAL;
			goto end;
		}
	}
	if (!list_empty(&e->probes)) {
		list_for_each_entry(tp_cb, &e->probes, list) {
			if (tp_cb->tp_func.func == func) {
				SLSI_INFO_NODEV(" Tracepoint[%s] : Function already exists.\n", tp_name);
				goto end;
			}
		}
	}
	tp_cb = kmalloc(sizeof(struct tracepoint_cb), GFP_KERNEL);
	if (!tp_cb) {
		if (list_empty(&e->probes)) {
			hlist_del(&e->hlist);
			kfree(e);
		}
		ret = -ENOMEM;
		goto end;
	}
	tp_cb->tp_func.func = func;
	tp_cb->tp_func.data = data;
	list_add(&tp_cb->list, &e->probes);

	ret = tracepoint_probe_register(e->tp, tp_cb->tp_func.func, tp_cb->tp_func.data);
	SLSI_INFO_NODEV(" Tracepoint[%s] : Registration callback function is success.\n", tp_name);
end:
	mutex_unlock(&slsi_tracepoint_cb_mutex);
	return ret;
}

int slsi_unregister_tracepoint_callback(const char *tp_name, void *func)
{
	struct tracepoint_entry *e;
	struct tracepoint_cb *tp_cb;
	int ret = -EINVAL;

	mutex_lock(&slsi_tracepoint_cb_mutex);
	e = slsi_get_tracepoint(tp_name);
	if (!e)
		goto err;

	if (!list_empty(&e->probes)) {
		list_for_each_entry(tp_cb, &e->probes, list) {
			if (tp_cb->tp_func.func == func) {
				ret = tracepoint_probe_unregister(e->tp, tp_cb->tp_func.func, NULL);
				list_del(&tp_cb->list);
				kfree(tp_cb);
				goto end;
			}
		}
	}
end:
	if (list_empty(&e->probes)) {
		hlist_del(&e->hlist);
		kfree(e);
	}
	SLSI_INFO_NODEV(" Tracepoint[%s] : Callback function is unregistered\n", tp_name);
err:
	mutex_unlock(&slsi_tracepoint_cb_mutex);
	return ret;
}

int slsi_unregister_tracepoint_callback_all(const char *tp_name)
{
	struct tracepoint_entry *e;
	struct tracepoint_cb *tp_cb;
	struct tracepoint_cb *backup_tp_cb;
	int ret = -EINVAL;

	mutex_lock(&slsi_tracepoint_cb_mutex);
	e = slsi_get_tracepoint(tp_name);
	if (!e)
		goto end;

	if (!list_empty(&e->probes)) {
		list_for_each_entry_safe(tp_cb, backup_tp_cb, &e->probes, list) {
			ret = tracepoint_probe_unregister(e->tp, tp_cb->tp_func.func, NULL);
			if (ret < 0)
				goto end;
			list_del(&tp_cb->list);
			kfree(tp_cb);
		}
	}
	hlist_del(&e->hlist);
	kfree(e);
	SLSI_INFO_NODEV(" Tracepoint[%s] : All callback functions are unregisters.\n", tp_name);
end:
	mutex_unlock(&slsi_tracepoint_cb_mutex);
	return ret;
}

static void remove_tracepoint(struct tracepoint *tp, void *ignore)
{
	slsi_unregister_tracepoint_callback_all(tp->name);
}

void slsi_unregister_all_tracepoints(void)
{
	for_each_kernel_tracepoint(remove_tracepoint, NULL);
}

void tcp_retransmit_skb_callback(void *ignore, const struct sock *sk, const struct sk_buff *skb)
{
	struct tcp_sock *tp = NULL;
	struct inet_sock *inet = NULL;
	__be32 daddr = 0;
	__be32 saddr = 0;

	if (!slsi_trace_log_enabled)
		return;

	if (!sk || !skb) {
		SLSI_ERR_NODEV("sk is NULL %d\n");
		return;
	}

	tp = tcp_sk(sk);
	inet = inet_sk(sk);

	daddr = inet->inet_daddr;
	saddr = inet->inet_saddr;

	if (sk->sk_state == TCP_ESTABLISHED) {
		SLSI_INFO_NODEV("%pI4:%hu->%pI4:%hu len:%d snd_una:%u snd_cwnd:%u prior_cwnd:%u\n",
				&saddr, ntohs(inet->inet_sport), &daddr, ntohs(inet->inet_dport), skb->len,
				tp->snd_una, tp->snd_cwnd, tp->prior_cwnd);
	}
}

static int sk_stream_wait_memory_entry_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	struct sock *sk = NULL;
	struct tcp_sk_memory_info *data;

	if (!current->mm)
		return 1;

	data = (struct tcp_sk_memory_info *)ri->data;

	if (!slsi_trace_log_enabled) {
		data->enabled = false;
		return 0;
	}

	data->enabled = true;
	data->sk_stream_wait_memory_start_time = ktime_get();

#if defined(__aarch64__) || defined(__x86_64__)
	sk =  (struct sock *)regs->regs[0];
#else /* __arm__ */
	sk = (struct sock *)regs->di;
#endif

	if (sk) {
		data->is_sock_info = true;
		data->sock_wmem_queued = READ_ONCE(sk->sk_wmem_queued);
		data->sock_sndbuf = READ_ONCE(sk->sk_sndbuf);
	} else {
		data->is_sock_info = false;
	}

	return 0;
}

static int sk_stream_wait_memory_handler(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	ktime_t now;
	struct tcp_sk_memory_info *data;

	if (!current->mm)
		return 1;

	now = ktime_get();
	data = (struct tcp_sk_memory_info *)ri->data;

	if (!slsi_trace_log_enabled || !data->enabled) {
		data->enabled = false;
		return 0;
	}

	if (data->is_sock_info)
		SLSI_INFO_NODEV("For %lldns sndbuf:%d wmem_queued:%d\n",
				ktime_to_ns(ktime_sub(now, data->sk_stream_wait_memory_start_time)),
				data->sock_sndbuf, data->sock_wmem_queued);
	else
		SLSI_INFO_NODEV("For %lldns\n", ktime_to_ns(ktime_sub(now, data->sk_stream_wait_memory_start_time)));

	return 0;
}

struct kretprobe rp_sk_stream_wait_memory = {
	.kp.symbol_name = "sk_stream_wait_memory",
	.handler = sk_stream_wait_memory_handler,
	.entry_handler = sk_stream_wait_memory_entry_handler,
	.data_size = sizeof(struct tcp_sk_memory_info),
	.maxactive = 20
};

int register_tcp_debugpoints(void)
{
	int ret = 0;

	ret = slsi_register_tracepoint_callback("tcp_retransmit_skb", tcp_retransmit_skb_callback, NULL);
	if (ret < 0) {
		SLSI_ERR_NODEV("Register Failed:tcp_retransmit_skb %d\n", ret);
		return ret;
	}
	ret = register_kretprobe(&rp_sk_stream_wait_memory);
	if (ret < 0) {
		SLSI_ERR_NODEV("Register Failed:rp_sk_stream_wait_memory %d\n", ret);
		slsi_unregister_tracepoint_callback("tcp_retransmit_skb", tcp_retransmit_skb_callback);
	}

	return ret;
}

void unregister_tcp_debugpoints(void)
{
	slsi_unregister_tracepoint_callback("tcp_retransmit_skb", tcp_retransmit_skb_callback);
	unregister_kretprobe(&rp_sk_stream_wait_memory);
}

void slsi_tracepoint_log_enable(bool enable)
{
	slsi_trace_log_enabled = enable;
}
