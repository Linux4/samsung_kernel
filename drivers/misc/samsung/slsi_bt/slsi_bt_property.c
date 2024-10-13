#include <linux/skbuff.h>
#include "hci_trans.h"
#include "hci_pkt.h"
#include "slsi_bt_property.h"
#include "slsi_bt_log.h"

enum prop_tags {
	MXLOG_LOG_EVENT_IND   = 0x01,
	MXLOG_LOGMASK_SET_REQ = 0x02,
};

int slsi_bt_property_recv(struct hci_trans *htr, struct sk_buff *skb)
{
	struct sk_buff *resp_skb = NULL;
	unsigned char tag, len, *value;
	int ret = 0;

	if (GET_HCI_PKT_TR_TYPE(skb) == HCI_TRANS_H4) {
		skb_pull(skb, 1);
		SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_HCI);
	}

	while (skb->len > 1) {
		tag  =  skb->data[0];
		len   =  skb->data[1];
		if (len > skb->len - 2) {
			ret = -EINVAL;
			break;
		}
		value = &skb->data[2];

		switch (tag) {
		case MXLOG_LOG_EVENT_IND:
			BT_DBG("MXLOG_LOG_EVENT_IND\n");
			slsi_bt_mxlog_log_event(len, value);
			resp_skb = NULL;
			break;

		default:
			BT_DBG("Unknown tag property tag: %u\n", tag);
			resp_skb = alloc_hci_pkt_skb(2);
			skb_put_data(resp_skb, skb->data, 2);
			SET_HCI_PKT_TYPE(resp_skb, HCI_PROPERTY_PKT);
		}

		if (resp_skb) {
			ret = htr->target->send_skb(htr->target, resp_skb);
			if (ret)
				break;
		}

		skb_pull(skb, len + 2);
	}
	kfree_skb(skb);

	return ret;
}

int slsi_bt_property_set_logmask(struct hci_trans *htr,
		const unsigned int *val, const unsigned char len)
{
	struct sk_buff *skb = alloc_hci_pkt_skb(sizeof(unsigned int)*len);

	if (htr == NULL || val == NULL || htr->target == NULL)
		return -EINVAL;

	if (skb == NULL)
		return -ENOMEM;

	if (len >= 2)
		BT_DBG("set logmask: 0x%016x%016x\n", val[0], val[1]);
	else
		BT_DBG("set logmask: 0x%016x\n", val[0]);

	SET_HCI_PKT_TYPE(skb, HCI_PROPERTY_PKT);
	SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_HCI);

	skb_put_u8(skb, MXLOG_LOGMASK_SET_REQ);
	skb_put_u8(skb, sizeof(unsigned int)*len);
	skb_put_data(skb, val, sizeof(unsigned int)*len);

	BT_DBG("MXLOG_LOGMASK_SET_REQ\n");
	return htr->target->send_skb(htr->target, skb);
}
