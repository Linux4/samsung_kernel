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

#include "slsi_bt_log.h"

enum {
	HCI_TRANS_HCI,
	HCI_TRANS_H4,	// Uart trasnport layer
	HCI_TRANS_BCSP,	// 3-Wired uart transport layer
};

#define HCITRFLAG_MASK          ((unsigned int)aligned_byte_mask(1))
#define HCITRFLAG_UBUF          BIT(1)

#define HCITRARG_MASK           (aligned_byte_mask(1) << 8)
#define HCITRARG_SET(arg)       ((arg << 8) & HCITRARG_MASK)
#define HCITRARG_GET(arg)       ((arg & HCITRARG_MASK) >> 8)

struct hci_trans {
	void                            *tdata;          /* transporter data */
	const char                      *name;

	struct hci_trans	        *host;
	struct hci_trans                *target;

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

#ifdef SLSI_BT_LOG_TRACE_TRANSPORT
#define BTTR_TRACE_TARGET(htr)                       \
do { \
	if (htr == NULL) TR_DBG("No hci_trans"); \
	if (!htr->target) TR_DBG("tr: %s, no target\n", htr->name); \
	else TR_DBG("tr: %s, target: %s\n", htr->name, htr->target->name); \
} while(0)

#define BTTR_TRACE_HOST(htr)                         \
do { \
	if (htr == NULL) TR_DBG("No hci_trans"); \
	if (!htr->host) TR_DBG("tr: %s, no host\n", htr->name); \
	else TR_DBG("tr: %s, host: %s\n", htr->name, htr->host->name); \
} while(0)

#define BTTR_TRACE_SEND(htr)                         \
do { \
	if (htr == NULL) TR_DBG("No hci_trans"); \
	if (!htr) TR_DBG("tr: NULL\n"); \
	else BTTR_TRACE_TARGET(htr); \
} while(0)

#define BTTR_TRACE_RECV(htr)                         \
do { \
	if (htr == NULL) TR_DBG("No hci_trans"); \
	if (!htr) TR_DBG("tr: NULL\n"); \
	else BTTR_TRACE_HOST(htr); \
} while(0)

#define BTTR_TRACE_HEX(tag, skb)                     \
	if (skb) slsi_bt_log_data_hex(__func__, (tag), (skb)->data, (skb)->len)
#else
#define BTTR_TRACE_TARGET(x)
#define BTTR_TRACE_HOST(x)
#define BTTR_TRACE_SEND(x)
#define BTTR_TRACE_RECV(x)

#define BTTR_TRACE_HEX(tag, skb)
#endif

static inline int hci_trans_default_send_skb(struct hci_trans *htr,
							struct sk_buff *skb)
{
	BTTR_TRACE_SEND(htr);

	if (!htr)
		return -EINVAL;
	return htr->target ? htr->target->send_skb(htr->target, skb) : -EINVAL;
}

static inline int hci_trans_default_recv_skb(struct hci_trans *htr,
							struct sk_buff *skb)
{
	BTTR_TRACE_RECV(htr);

	if (!htr)
		return -EINVAL;
	return htr->host ? htr->host->recv_skb(htr->host, skb) : -EINVAL;
}

void hci_trans_init(struct hci_trans *htr, const char *name);
void hci_trans_deinit(struct hci_trans *htr);
void hci_trans_setup_host_target(struct hci_trans *host,
		struct hci_trans *target);
struct sk_buff *hci_trans_default_make_skb(const char *data, size_t count,
		unsigned int flags);
ssize_t hci_trans_default_send(struct hci_trans *htr, const char *data,
		size_t count, unsigned int flags);
ssize_t hci_trans_default_recv(struct hci_trans *htr, const char *data,
		size_t count, unsigned int flags);
#endif /* __HCI_TRANSPORT_H__ */
