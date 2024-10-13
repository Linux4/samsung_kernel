#include <linux/skbuff.h>
#include "hci_trans.h"
#include "hci_pkt.h"
#include "slsi_bt_property.h"
#include "slsi_bt_log.h"

typedef int (*vs_channel_handler)(struct hci_trans *htr, struct sk_buff *skb);
#define UNKNOWN_TAG (0)

enum mxlog_tags {
	MXLOG_LOG_EVENT_IND   = 0x01,
	MXLOG_LOGMASK_SET_REQ = 0x02,
};

static inline unsigned char get_tag(struct sk_buff *skb)
{
	const int idx = 0;

	if (skb && skb->len > idx)
		return skb->data[idx];
	return UNKNOWN_TAG;
}

static inline unsigned char get_len(struct sk_buff *skb)
{
	const int idx = 1;

	if (skb && skb->len > idx)
		return skb->data[idx];
	return 0;
}

static inline unsigned char *get_value(struct sk_buff *skb)
{
	const int idx = 2;

	if (skb && skb->len > idx)
		return &skb->data[2];
	return NULL;
}

static int vsc_mxlog_handler(struct hci_trans *htr, struct sk_buff *skb)
{
	struct sk_buff *resp_skb = NULL;
	unsigned char tag, len, *value;
	int ret = 0;

	while ((len = get_len(skb)) > 0) {
		tag = get_tag(skb);
		value = get_value(skb);
		if (value == NULL || len > skb->len - 2) {
			ret = -EINVAL;
			break;
		}

		switch (tag) {
		case MXLOG_LOG_EVENT_IND:
			slsi_bt_mxlog_log_event(len, value);
			resp_skb = NULL;
			break;

		default:
			BT_WARNING("Unknown tag property tag: %u\n", tag);
			resp_skb = alloc_hci_pkt_skb(2);
			if (resp_skb) {
				skb_put_data(resp_skb, skb->data, 2);
				SET_HCI_PKT_TYPE(resp_skb, HCI_PROPERTY_PKT);
			}
		}

		if (resp_skb) {
			ret = hci_trans_send_skb(htr, resp_skb);
			if (ret)
				break;
		}

		skb_pull(skb, len + 2);
	}
	kfree_skb(skb);
	return ret;
}

static vs_channel_handler handlers[] = {
	NULL,
	vsc_mxlog_handler,
};

static inline unsigned char get_channel(struct sk_buff *skb)
{
	if (skb && skb->len > 0 && skb->data[0] < ARRAY_SIZE(handlers))
		return skb->data[0];
	return SLSI_BTP_VS_CHANNEL_UNKNOWN;
}

int slsi_bt_property_recv(struct hci_trans *htr, struct sk_buff *skb)
{
	unsigned char channel;

	if (GET_HCI_PKT_TR_TYPE(skb) == HCI_TRANS_H4) {
		skb_pull(skb, 1);
		SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_HCI);
	}
	channel = get_channel(skb);
	if (channel != SLSI_BTP_VS_CHANNEL_UNKNOWN && handlers[channel]) {
		skb_pull(skb, 1); // remove channel
		return handlers[channel](htr, skb);
	}
	kfree_skb(skb);
	BT_WARNING("Unsupported channel: %u\n", channel);
	return -EINVAL;
}

int slsi_bt_property_set_logmask(struct hci_trans *htr,
		const unsigned int *val, const unsigned char len)
{
	const int size = (sizeof(unsigned int)*len)+1+2; /* channel + TL */
	struct sk_buff *skb = NULL;

	if (htr == NULL || val == NULL)
		return -EINVAL;

	skb = alloc_hci_pkt_skb(size);
	if (skb == NULL)
		return -ENOMEM;

	if (len >= 2)
		BT_DBG("set logmask: 0x%016x%016x\n", val[0], val[1]);
	else
		BT_DBG("set logmask: 0x%016x\n", val[0]);

	SET_HCI_PKT_TYPE(skb, HCI_PROPERTY_PKT);
	SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_HCI);

	skb_put_u8(skb, SLSI_BTP_VS_CHANNEL_MXLOG);
	skb_put_u8(skb, MXLOG_LOGMASK_SET_REQ);
	skb_put_u8(skb, sizeof(unsigned int)*len);
	skb_put_data(skb, val, sizeof(unsigned int)*len);

	BT_DBG("MXLOG_LOGMASK_SET_REQ\n");
	return hci_trans_send_skb(htr, skb);
}
