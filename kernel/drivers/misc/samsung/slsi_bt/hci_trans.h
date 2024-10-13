/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth HCI Transport Layer                                              *
 *                                                                            *
 ******************************************************************************/
#ifndef __HCI_TRANSPORT_H__
#define __HCI_TRANSPORT_H__
#include <linux/skbuff.h>
#include <linux/bitops.h>
#include <linux/seq_file.h>
#include <linux/list.h>

#include "slsi_bt_log.h"

enum {
	HCI_TRANS_HCI,
	HCI_TRANS_H4,	// Uart trasnport layer
	HCI_TRANS_BCSP,	// 3-Wired uart transport layer
	HCI_TRANS_SLIP,	// 3-Wired uart transport layer

	HCI_TRANS_ON_READING = 0xF0, // Now, it is not completed packet
};

#define HCITRFLAG_MASK          ((unsigned int)aligned_byte_mask(1))
#define HCITRFLAG_UBUF          BIT(1)

#define HCITRARG_MASK           (aligned_byte_mask(1) << 8)
#define HCITRARG_SET(arg)       ((arg << 8) & HCITRARG_MASK)
#define HCITRARG_GET(arg)       ((arg & HCITRARG_MASK) >> 8)

struct hci_trans {
	void                            *tdata;          /* transporter data */
	const char                      *name;

	struct list_head                list;
	struct hci_trans                *head;

	/* Sending HCI Transport packet to low transport layer.
	 * send_skb() may APPEND a pakcet header of own transport layer. */
	int (*send_skb)(struct hci_trans *htr, struct sk_buff *skb);

	/* Receving HCI Transport packet to upper transport layer.
	 * recv_skb() may REMOVE a packet header of own transport layer. */
	int (*recv_skb)(struct hci_trans *htr, struct sk_buff *skb);

	ssize_t (*send)(struct hci_trans *htr, const char __user *data,
			size_t count, unsigned int arg);
	ssize_t (*recv)(struct hci_trans *htr, const char __user *data,
			size_t count, unsigned int flags);

	int (*proc_show)(struct hci_trans *htr, struct seq_file *m);
	void (*deinit)(struct hci_trans *htr);
};

#if defined(SLSI_BT_LOG_TRACE_DATA_HEX) || defined(SLSI_BT_LOG_TRACE_TRANSPORT)
#define BTTR_TRACE_HEX(tag, skb)                     \
	if (skb) slsi_bt_log_data_hex(__func__, (tag), (skb)->data, (skb)->len)
#else
#define BTTR_TRACE_HEX(tag, skb)
#endif

struct hci_trans *hci_trans_new(const char *name);
void hci_trans_free(struct hci_trans *htr);
void hci_trans_add_tail(struct hci_trans *htr, struct hci_trans *head);
void hci_trans_del(struct hci_trans *htr);
struct hci_trans *hci_trans_get_next(struct hci_trans *htr);
struct hci_trans *hci_trans_get_prev(struct hci_trans *htr);

int hci_trans_send_skb(struct hci_trans *htr, struct sk_buff *skb);
int hci_trans_recv_skb(struct hci_trans *htr, struct sk_buff *skb);
const char *hci_trans_get_name(struct hci_trans *htr);

void hci_trans_init(struct hci_trans *htr, const char *name);
void hci_trans_deinit(struct hci_trans *htr);
struct sk_buff *hci_trans_default_make_skb(const char *data, size_t count,
		unsigned int flags);
ssize_t hci_trans_default_send(struct hci_trans *htr, const char *data,
		size_t count, unsigned int flags);
ssize_t hci_trans_default_recv(struct hci_trans *htr, const char *data,
		size_t count, unsigned int flags);
#endif /* __HCI_TRANSPORT_H__ */
