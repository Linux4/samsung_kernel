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
	htr->send_skb = hci_trans_default_send_skb;
	htr->recv_skb = hci_trans_default_recv_skb;
	htr->send = hci_trans_default_send;;
	htr->recv = hci_trans_default_recv;;
	htr->deinit = NULL;
}

void hci_trans_deinit(struct hci_trans *htr)
{
	if (htr == NULL)
		return;

	if (htr->host)
		htr->host->target = htr->target;
	if (htr->target)
		htr->target->host = htr->host;
	htr->host = NULL;
	htr->target = NULL;
}

void hci_trans_setup_host_target(struct hci_trans *host,
		struct hci_trans *target)
{
	TR_INFO("host: %s -> target: %s\n", (host) ? host->name : "NULL",
					(target) ? target->name : "NULL");
	if (host)
		host->target = target;
	if (target)
		target->host = host;
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
			BT_WARNING("copy_from_user failed\n");
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
	BT_INFO("count: %zu\n", count);
	skb = hci_trans_default_make_skb(data, count, flags);
	return htr->send_skb(htr, skb);
}

ssize_t hci_trans_default_recv(struct hci_trans *htr, const char *data,
		size_t count, unsigned int flags)
{
	struct sk_buff *skb;
	BT_INFO("count: %zu\n", count);
	skb = hci_trans_default_make_skb(data, count, flags);
	return htr->recv_skb(htr, skb);
}
