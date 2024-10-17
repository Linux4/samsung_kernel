/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth HCI Transport Layer                                              *
 *                                                                            *
 ******************************************************************************/
#include <linux/skbuff.h>
#include "hci_trans.h"
#include "hci_pkt.h"

#ifdef SLSI_BT_LOG_TRACE_TRANSPORT
#define BTTR_CALL_TRACE(a, b)\
	TR_DBG("tr: %s, %s: %s\n", a, #b, b ? b : "NULL")
#else
#define BTTR_CALL_TRACE(a, b)
#endif

const char *hci_trans_get_name(struct hci_trans *htr)
{
	return htr ? (htr->name ? htr->name : "No name") : "NULL";
}

struct hci_trans *hci_trans_get_next(struct hci_trans *htr)
{
	if (htr && htr->head) {
		struct hci_trans *next = list_next_entry(htr, list);

		if (next != htr && next != htr->head)
			return next;
	}
	return NULL;
}

struct hci_trans *hci_trans_get_prev(struct hci_trans *htr)
{
	if (htr && htr->head) {
		struct hci_trans *prev = list_prev_entry(htr, list);
		struct hci_trans *last = list_prev_entry(htr->head, list);

		if (prev != htr && prev != last)
			return prev;
	}
	return NULL;
}

int hci_trans_send_skb(struct hci_trans *htr, struct sk_buff *skb)
{
	struct hci_trans *next = hci_trans_get_next(htr);

	BTTR_CALL_TRACE(htr, next);
	if (next)
		return next->send_skb(next, skb);

	if (skb)
		kfree_skb(skb);
	return -EINVAL;
}

int hci_trans_recv_skb(struct hci_trans *htr, struct sk_buff *skb)
{
	struct hci_trans *prev = hci_trans_get_prev(htr);

	BTTR_CALL_TRACE(htr, prev);
	TR_DBG("%s\n", hci_trans_get_name(prev));
	if (prev)
		return prev->recv_skb(prev, skb);

	if (skb)
		kfree_skb(skb);
	return -EINVAL;
}

void hci_trans_init(struct hci_trans *htr, const char *name)
{
	if (htr == NULL)
		return;

	TR_INFO("%s\n", name ? name : "Unknown");

	memset(htr, 0, sizeof(struct hci_trans));
	if (name)
		htr->name = name;
	else
		htr->name = "Unknown";

	INIT_LIST_HEAD(&htr->list);
	htr->head = htr;

	htr->send_skb = hci_trans_send_skb;
	htr->recv_skb = hci_trans_recv_skb;
	htr->send = hci_trans_default_send;;
	htr->recv = hci_trans_default_recv;;
	htr->deinit = NULL;
}

void hci_trans_deinit(struct hci_trans *htr)
{
	if (htr == NULL)
		return;

	if (htr->deinit && htr->deinit != hci_trans_deinit)
		htr->deinit(htr);
}

struct hci_trans *hci_trans_new(const char *name)
{
	struct hci_trans *htr =  kmalloc(sizeof(struct hci_trans), GFP_KERNEL);

	if (htr) {
		hci_trans_init(htr, name);
		return htr;
	}
	return NULL;
}

void hci_trans_free(struct hci_trans *htr)
{
	if (htr) {
		hci_trans_deinit(htr);
		kfree(htr);
	}
}

void hci_trans_add_tail(struct hci_trans *htr, struct hci_trans *head)
{
	struct hci_trans *prev = NULL;

	if (htr && head) {
		list_add_tail(&htr->list, &head->list);
		htr->head = head;

		prev = hci_trans_get_prev(htr);
		TR_INFO("prev: %s -> htr: %s -> next: %s\n",
			hci_trans_get_name(hci_trans_get_prev(htr)),
			hci_trans_get_name(htr),
			hci_trans_get_name(hci_trans_get_next(htr)));
	}
}

void hci_trans_del(struct hci_trans *htr)
{
	if (htr) {
		htr->head = htr;
		list_del(&htr->list);
	}
}

struct sk_buff *hci_trans_default_make_skb(const char *data, size_t count,
		unsigned int flags)
{
	struct sk_buff *skb;

	if (data == NULL)
		return NULL;

	skb = alloc_hci_pkt_skb(count);
	if (!skb)
		return NULL;

	if (flags & HCITRFLAG_UBUF) {
		if (copy_from_user(skb_put(skb, count), data, count) != 0) {
			TR_WARNING("copy_from_user failed\n");
			kfree_skb(skb);
			return NULL;
		}
	} else {
		skb_put_data(skb, data, count);
	}
	return skb;
}

ssize_t hci_trans_default_send(struct hci_trans *htr, const char *data,
		size_t count, unsigned int flags)
{
	struct sk_buff *skb;
	TR_DBG("count: %zu\n", count);
	skb = hci_trans_default_make_skb(data, count, flags);
	return htr->send_skb(htr, skb);
}

ssize_t hci_trans_default_recv(struct hci_trans *htr, const char *data,
		size_t count, unsigned int flags)
{
	struct sk_buff *skb;
	TR_DBG("count: %zu\n", count);
	skb = hci_trans_default_make_skb(data, count, flags);
	return htr->recv_skb(htr, skb);
}
