/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth HCI Packet                                                       *
 *                                                                            *
 ******************************************************************************/
#ifndef __HCI_PKT_H__
#define __HCI_PKT_H__
#include <linux/skbuff.h>

/* HCI Packet type indicator */
enum {
	HCI_UNKNOWN_PKT	= 0,
	HCI_COMMAND_PKT,
	HCI_ACL_DATA_PKT,
	HCI_SCO_DATA_PKT,
	HCI_EVENT_PKT,
	HCI_ISO_DATA_PKT,
	HCI_PROPERTY_PKT = 14,
};

enum {
	HCI_PROPERTY_HW_ERROR = 0xF,
};

/* HCI Context Block for socket buffer */
struct hci_cb {
	char type;
	char trans_type;
	char property;
};

#define GET_HCI_CB(skb)		        ((struct hci_cb *)(skb->cb))

#define GET_HCI_PKT_TYPE(skb)           (GET_HCI_CB(skb)->type)
#define SET_HCI_PKT_TYPE(skb, _TYPE)    (GET_HCI_CB(skb)->type = _TYPE)

#define GET_HCI_PKT_TR_TYPE(skb)        (GET_HCI_CB(skb)->trans_type)
#define SET_HCI_PKT_TR_TYPE(skb, _TYPE) (GET_HCI_CB(skb)->trans_type = _TYPE)

#define GET_HCI_PKT_PROPERTY(skb)       (GET_HCI_CB(skb)->property)
#define SET_HCI_PKT_PROPERTY(skb, PROP) (GET_HCI_CB(skb)->property = PROP)

#define SET_HCI_PKT_HW_ERROR(skb)        do {\
	SET_HCI_PKT_TYPE(skb, HCI_PROPERTY_PKT);\
	SET_HCI_PKT_PROPERTY(skb, HCI_PROPERTY_HW_ERROR); } while(0)

#define TEST_HCI_PKT_HW_ERROR(skb)      \
	(GET_HCI_PKT_TYPE(skb) == HCI_PROPERTY_PKT && \
	 GET_HCI_PKT_PROPERTY(skb) == HCI_PROPERTY_HW_ERROR)


#define HCI_PKT_HEAD_ROOM_SIZE          (1+4)	/* slip start + bcsp header */
#define HCI_PKT_TAIL_ROOM_SIZE          (1+2)	/* slip end + bcsp tail */

#define HCI_PKT_MIN_BUF_SIZE		(2048)

static inline struct sk_buff *__alloc_hci_pkt_skb(unsigned int size,
						  unsigned int hdr_size)
{
	struct sk_buff *skb;

	size += HCI_PKT_HEAD_ROOM_SIZE + HCI_PKT_TAIL_ROOM_SIZE;

	/* It prevents frequent expansion */
	size = size > HCI_PKT_MIN_BUF_SIZE ? size : HCI_PKT_MIN_BUF_SIZE;

	skb = alloc_skb(size, GFP_ATOMIC);
	if (skb && hdr_size < HCI_PKT_HEAD_ROOM_SIZE) {
		skb->data += HCI_PKT_HEAD_ROOM_SIZE - hdr_size;
		skb->tail += HCI_PKT_HEAD_ROOM_SIZE - hdr_size;
	}
	return skb;
}

static inline struct sk_buff *alloc_hci_pkt_skb(unsigned int size)
{
	return __alloc_hci_pkt_skb(size, 0);
}

bool hci_pkt_check_complete(char type, char *data, size_t len);
#endif /* __HCI_PKT_H__ */
