/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth HCI Uart Transport Layer & H4                                    *
 *                                                                            *
 ******************************************************************************/

#include <linux/skbuff.h>
#include <linux/seq_file.h>

#include "hci_trans.h"
#include "hci_pkt.h"
#include "hci_h4.h"

#define SLSI_BT_WR_SKB_MAX_REUSE_CNT    0xFF

struct hci_h4 {
	struct sk_buff	        *rskb;
	struct sk_buff          *wskb;
	int                     use_rskb;
	int                     use_wskb;

	bool                    reverse;

	unsigned int            send_skb_count;
	unsigned int            recv_skb_count;
};

static inline struct sk_buff *alloc_hci_h4_pkt_skb(int size)
{
	return __alloc_hci_pkt_skb(size, HCI_H4_PKT_TYPE_SIZE);
}

static inline struct hci_h4 *get_hci_h4(struct hci_trans *htr)
{
	return htr ? (struct hci_h4 *)htr->tdata : NULL;
}

static int hci_h4_skb_pack(struct hci_trans *htr, struct sk_buff *skb)
{
	struct hci_h4 *h4 = get_hci_h4(htr);

	if (htr == NULL || skb == NULL || h4 == NULL)
		return 0;

	/* Add type bytes */
	skb_push(skb, 1);
	skb->data[0] = GET_HCI_PKT_TYPE(skb);

	/* Now, skb contains HCI packet with HCI type bytes */
	SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_H4);
	TR_DBG("H4 packet type=%d, len=%d\n", skb->data[0], skb->len);
	BTTR_TRACE_HEX(h4->reverse ? BTTR_TAG_H4_RX : BTTR_TAG_H4_TX, skb);

	h4->send_skb_count++;

	return h4->reverse ? hci_trans_default_recv_skb(htr, skb) :
		hci_trans_default_send_skb(htr, skb);
}

static bool hci_h4_pkt_check(char *data, size_t len)
{
	if (data && len >= HCI_H4_PKT_TYPE_SIZE)
		return hci_pkt_check_complete(data[0], &data[1], len - 1);
	return false;
}

static int hci_h4_skb_unpack(struct hci_trans *htr, struct sk_buff *skb)
{
	struct hci_h4 *h4 = get_hci_h4(htr);
	char type;

	if (htr == NULL || skb == NULL || h4 == NULL)
		return -EINVAL;

	if (GET_HCI_PKT_TR_TYPE(skb) == HCI_TRANS_H4) {
		BTTR_TRACE_HEX(
			h4->reverse ? BTTR_TAG_H4_TX : BTTR_TAG_H4_RX, skb);

		/* Remove type bytes */
		type = skb->data[0];
		skb_pull(skb, 1);

		/* Now, skb contains HCI packet only */
		SET_HCI_PKT_TYPE(skb, type);
		SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_HCI);
		h4->recv_skb_count++;
	} else if (GET_HCI_PKT_TR_TYPE(skb) != HCI_TRANS_HCI) {
		BT_WARNING("Invalid tr type: %u\n", GET_HCI_PKT_TR_TYPE(skb));
		return - EINVAL;
	}

	type = GET_HCI_PKT_TYPE(skb);

	TR_DBG("HCI packet type=%d, len=%d\n", type, skb->len);

	return h4->reverse ? hci_trans_default_send_skb(htr, skb) :
			hci_trans_default_recv_skb(htr, skb);
}

static struct sk_buff *get_collector_skb(struct sk_buff **ref, int *ref_cnt,
		size_t size, int is_h4)
{
	struct sk_buff *skb;
	if (ref == NULL)
		return NULL;

	skb = *ref;
	if (skb == NULL) {
		if (is_h4)
			skb = alloc_hci_h4_pkt_skb(size);
		else
			skb = alloc_hci_pkt_skb(size);

		if (skb == NULL)
			BT_ERR("failed to memory allocate\n");
		*ref_cnt = 0;

	} else if (skb_tailroom(skb) < size) {
		struct sk_buff *nskb = skb;

		BT_DBG("expand skb to %zu\n", size);
		nskb = skb_copy_expand(skb, 0, size, GFP_ATOMIC);
		if (nskb == NULL)
			BT_WARNING("failed to skb expand\n");

		kfree_skb(skb);
		skb = nskb;
	}

	(*ref_cnt)++;
	if (*ref_cnt > SLSI_BT_WR_SKB_MAX_REUSE_CNT) {
		BT_ERR("Reusing ref has reached its limit (%d)\n",
			SLSI_BT_WR_SKB_MAX_REUSE_CNT);
		kfree_skb(skb);
		skb = NULL;
	}

	// save collector skb
	*ref = skb;

	BT_DBG("ret: %p, ref_cnt: %d\n", skb, *ref_cnt);
	return skb;
}

static int collector_h4_pkt(struct sk_buff *skb, const char *data, size_t count,
		unsigned int flags)
{
	if (flags & HCITRFLAG_UBUF) {
		if (copy_from_user(skb_put(skb, count), data, count) != 0) {
			BT_WARNING("copy_from_user failed\n");
			return -EACCES;
		}
	} else {
		skb_put_data(skb, data, count);
	}

	return 0;
}

static ssize_t hci_h4_pack(struct hci_trans *htr, const char __user *data,
		size_t count, unsigned int arg)
{
	struct hci_h4 *h4 = get_hci_h4(htr);
	struct sk_buff *skb;
	unsigned int flags = arg & HCITRFLAG_MASK;
	char type = (char)HCITRARG_GET(arg);
	ssize_t ret;

	BT_INFO("count: %zu, flags: %u\n", count, flags);
	if (htr == NULL|| data == NULL || h4 == NULL || count < 0)
		return -EINVAL;

	skb = get_collector_skb(&h4->wskb, &h4->use_wskb, count, false);
	if (skb && h4->use_wskb == 1) {
		BT_DBG("type: %d\n", type);
		SET_HCI_PKT_TYPE(skb, type);
	} else if (skb == NULL) {
		BT_ERR("failed to allocate write skb\n");
		return -ENOMEM;
	}

	if (collector_h4_pkt(skb, data, count, flags) != 0) {
		kfree_skb(skb);
		h4->wskb = NULL;
		return -EACCES;
	}

	if (hci_pkt_check_complete(GET_HCI_PKT_TYPE(skb), skb->data, skb->len)
			== false) {
		BT_DBG("HCI hci packet is not completed. wait more data\n");
		return count;
	}

	// completed HCI packet
	SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_HCI);
	h4->wskb = NULL;
	BT_DBG("write %d bytes\n", skb->len);
	ret = hci_h4_skb_pack(htr, skb);
	return (ret == 0) ? count : ret;
}

static ssize_t hci_h4_unpack(struct hci_trans *htr, const char __user *data,
		size_t count, unsigned int flags)
{
	struct hci_h4 *h4 = get_hci_h4(htr);
	struct sk_buff *skb;
	ssize_t ret = 0;

	BT_INFO("count: %zu\n", count);
	if (htr == NULL|| data == NULL || h4 == NULL || count < 0)
		return -EINVAL;

	skb = get_collector_skb(&h4->rskb, &h4->use_rskb, count, true);
	if (skb == NULL) {
		BT_ERR("failed to allocate write skb\n");
		return -ENOMEM;
	}

	if (collector_h4_pkt(skb, data, count, flags) != 0) {
		kfree_skb(skb);
		h4->rskb = NULL;
		return -EACCES;
	}

	if (hci_h4_pkt_check(skb->data, skb->len) == false) {
		BT_DBG("HCI H4 packet is not completed. wait more data\n");
		return count;
	}

	// completed H4 packet
	SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_H4);
	h4->rskb = NULL;
	BT_DBG("received %d bytes\n", skb->len);
	ret = hci_h4_skb_unpack(htr, skb);
	return (ret == 0) ? count : ret;
}

static int hci_h4_proc_show(struct hci_trans *htr, struct seq_file *m)
{
	struct hci_h4 *h4 = get_hci_h4(htr);
	if (h4 == NULL)
		return 0;

	seq_printf(m, "\n  %s:\n", htr->name);
	seq_printf(m, "    rskb status            = %s\n",
		h4->rskb ? "Wait for more data" : "NULL");
	if (h4->rskb) {
		seq_printf(m, "    use_rskb               = %d\n",
			h4->use_rskb);
	}
	seq_printf(m, "    wskb status            = %s\n",
		h4->wskb ? "Wait for more data" : "NULL");
	if (h4->wskb) {
		seq_printf(m, "    use_wskb               = %d\n",
			h4->use_wskb);
	}

	seq_puts(m, "\n");
	seq_printf(m, "    Sent skb count         = %u\n", h4->send_skb_count);
	seq_printf(m, "    Received skb count     = %u\n", h4->send_skb_count);

	return 0;
}

int hci_h4_init(struct hci_trans *htr, bool reverse)
{
	struct hci_h4 *h4;

	TR_INFO("reverse=%d\n", reverse);
	if (reverse) {
		hci_trans_init(htr, "H4 Transport (revert)");
		/* H4 packet is input data of send(), and HCI packet will be
		 * sent to the next transport. */
		htr->send_skb = hci_h4_skb_unpack;
		htr->recv_skb = hci_h4_skb_pack;
		htr->send = hci_h4_unpack;
		htr->recv = hci_h4_pack;
	} else {
		hci_trans_init(htr, "H4 Transport");
		htr->send_skb = hci_h4_skb_pack;
		htr->recv_skb = hci_h4_skb_unpack;
		htr->send = hci_h4_pack;
		htr->recv = hci_h4_unpack;
	}
	htr->proc_show = hci_h4_proc_show;
	htr->deinit = hci_h4_deinit;

	h4 = kzalloc(sizeof(struct hci_h4), GFP_KERNEL);
	h4->reverse = reverse;
	htr->tdata = h4;

	return 0;
}

void hci_h4_deinit(struct hci_trans *htr)
{
	struct hci_h4 *h4 = get_hci_h4(htr);

	if (htr == NULL || h4 == NULL)
		return;

	if (h4->rskb)
		kfree_skb(h4->rskb);
	if (h4->wskb)
		kfree_skb(h4->rskb);
	kfree(h4);
	htr->tdata = NULL;
	hci_trans_deinit(htr);
}
