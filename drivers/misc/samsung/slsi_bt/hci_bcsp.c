/******************************************************************************
 *                                                                            *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved       *
 *                                                                            *
 * Bluetooth HCI Three-Wire Uart Transport Layer & BCSP                       *
 * (SIG Core v5.3 - Vol4. PartD and BlueCore Serial Protocol 2004 by CSR)     *
 *                                                                            *
 ******************************************************************************/
#include <linux/skbuff.h>
#include <linux/crc-ccitt.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/bitrev.h>
#include <linux/seq_file.h>

#include "slsi_bt_err.h"
#include "hci_trans.h"
#include "hci_pkt.h"
#include "hci_bcsp.h"

struct hci_bcsp {
	struct hci_trans                *htr;

	struct work_struct              work_tx;
	struct work_struct              work_timeout;
	struct workqueue_struct	        *wq;
	atomic_t                        tx_running;

	struct mutex                    lock;

	struct sk_buff_head             rel_q;
	struct sk_buff_head             sent_q;	        /* waiting ack */
	struct sk_buff_head             unrel_q;

	struct sk_buff                  *collect_skb;      /* collector */

	struct timer_list               resend_timer;
	int                             resent_cnt;
	int                             ack_mismatch;

	enum bcsp_link_state {
		HCI_BCSP_LINK_UNINITITALIZED = 0,
		HCI_BCSP_LINK_INITIALIZED,
		HCI_BCSP_LINK_ACTIVE,
	} state;

	unsigned char                   seq;       /* Sequence number */
	unsigned char                   ack;       /* Acknowledgement number */
	unsigned char                   rack;      /* Received ack number */
	bool                            use_crc;
	unsigned char                   credit;    /* Windows size */
	bool                            sw_fctrl;  /* OOF Flow Control */
	bool                            ack_req;
};

static inline struct sk_buff *alloc_hci_bcsp_pkt_skb(const int size)
{
	/* Size includes 2 bytes of crc and 2 bytes of delimiters for slip.
	 * The packet uses 4 bytes of BCSP header
	 */
	return __alloc_hci_pkt_skb(size + 2 + 2, 4); /* HDR = 4 bytes */
}

static inline struct sk_buff *alloc_hci_slip_pkt_skb(const int size)
{
	/* Size includes 2 bytes of crc and 2 bytes of delimiters for slip.
	 * The packet uses 4 bytes of BCSP header and 1 bytes of Start slip
	 * oct.
	 */
	return __alloc_hci_pkt_skb(size + 2 + 2, 5);
}

static inline struct hci_bcsp *get_hci_bcsp(const struct hci_trans *htr)
{
	return htr ? (struct hci_bcsp *)htr->tdata : NULL;
}

/********************************** SLIP **************************************/

static inline void slip_put_c0(struct sk_buff *skb)
{
	const unsigned char oct_c0 = 0xc0;
	skb_put_data(skb, &oct_c0, 1);
}

static inline int slip_special_oct_search(const unsigned char *data, int len,
		bool sw_fctrl)
{
	int offset = 0;
	while (offset < len) {
		switch (data[offset]) {
		case 0x11:
		case 0x13:
			if (!sw_fctrl)
				break;
		case 0xc0:
		case 0xdb:
			return offset;
		}
		offset++;
	}
	return len;
}

static inline int slip_esc_oct_search(const unsigned char *data, int len)
{
	int offset = 0;
	while (offset < len - 1) {
		if (data[offset] == 0xdb)
			return offset;
		offset++;
	}
	return len;
}

static struct sk_buff *slip_encode(const unsigned char *data, int len,
		bool sw_fctrl)
{
	struct sk_buff *skb;
	const char esc_c0[2] = { 0xdb, 0xdc }, esc_db[2] = { 0xdb, 0xdd },
		esc_11[2] = { 0xdb, 0xde }, esc_13[2] = { 0xdb, 0xdf };
	int offset = 0;

	/* slip packet needs to double size of buffer for the worst case.
	 * example) all packets are c0
	 */
	skb = alloc_hci_bcsp_pkt_skb(len * 2);
	if (skb == NULL)
		return NULL;

	slip_put_c0(skb);

	while (len > 0) {
		offset = slip_special_oct_search(data, len, sw_fctrl);
		skb_put_data(skb, data, offset);
		if (len == offset)
			break;          /* Complete. No more special oct. */

		data += offset;

		switch ((unsigned char)*data) {
		case 0x11: /* 0x11 and 0x13 are using when sw_fctrl == true */
			skb_put_data(skb, esc_11, 2);
			break;
		case 0x13:
			skb_put_data(skb, esc_13, 2);
			break;
		case 0xc0:
			skb_put_data(skb, esc_c0, 2);
			break;
		case 0xdb:
			skb_put_data(skb, esc_db, 2);
			break;
		default:
			skb_put_data(skb, data, 1);
		}
		data++;
		len -= (offset + 1);
	}

	slip_put_c0(skb);
	return skb;
}

static struct sk_buff *slip_decode(const unsigned char *data, int len)
{
	const char c0 = 0xc0, db = 0xdb, s11 = 0x11, s13 = 0x13;
	struct sk_buff *skb;
	int esc_offset = 0;

	skb = alloc_hci_bcsp_pkt_skb(len);
	if (skb == NULL)
		return NULL;

	/* Remove start c0 and end c0. 2bytes */
	data++;
	len -= 2;

	while (len > 0) {
		esc_offset = slip_esc_oct_search(data, len);
		skb_put_data(skb, data, esc_offset);
		if (len == esc_offset)
			break;          /* Complete. No more esc oct */

		data += esc_offset + 1;
		switch((unsigned char)*data) {
		case 0xdc:
			skb_put_data(skb, &c0, 1);
			break;
		case 0xdd:
			skb_put_data(skb, &db, 1);
			break;
		case 0xde:
			skb_put_data(skb, &s11, 1);
			break;
		case 0xdf:
			skb_put_data(skb, &s13, 1);
			break;
		default:
			// It's not slip packet
			esc_offset -= 1;
			data--;
			skb_put_data(skb, data, 1);
		}

		data++;
		len -= (esc_offset + 2);
	}

	return skb;
}

static int slip_collect_pkt(struct sk_buff **ref_skb, const unsigned char *data,
		int count)
{
	struct sk_buff *skb = *ref_skb;
	int s = 0, e = 0;

	if (ref_skb && data)
		skb = *ref_skb;
	else
		return -EINVAL;

	if (skb == NULL) {
		/* find slip starter */
		while (s < count && data[s] != 0xc0) s++;
		if (s > 0)
			BT_WARNING("skip invalid bytes: %d\n", s);
		if (s < count) {
			skb = alloc_hci_slip_pkt_skb(count - s);
			if (skb == NULL) {
				BT_ERR("alloc_hci_slip_pkt_skb is failed.\n");
				return -ENOMEM;
			}
			e = s+1;
		} else
			return -EINVAL;
	}

	/* find slip end. All slip packets start and end with 0xc0 */
	while (e < count) {
		if (data[e++] == 0xc0) {
			SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_BCSP);
			BT_DBG("complete a slip packet\n");
			break;
		}
	}

	skb_put_data(skb, data + s, e - s);
	BT_DBG("collected len: %d\n", skb->len);

	if (GET_HCI_PKT_TR_TYPE(skb) == HCI_TRANS_BCSP && skb->len == 2) {
		BT_WARNING("Continous c0: reuse the second c0 for next start");
		kfree_skb(skb);
		*ref_skb = NULL;
		return e - 1;
	}

	*ref_skb = skb;
	return e;
}

static inline bool slip_check_complete(struct sk_buff *skb)
{
	if (GET_HCI_PKT_TR_TYPE(skb) != HCI_TRANS_BCSP)
		return false;
	if (skb->data[0] != 0xc0 || skb->data[skb->len-1] != 0xc0)
		return false;
	return true;
}

/********************************** BCSP Packet *******************************/

static void bcsp_skb_put_crc(struct sk_buff *skb)
{
	unsigned int crc = 0;

	/* Remove first, if skb has crc bytes */
	if (GET_HCI_PKT_TR_TYPE(skb) == HCI_TRANS_BCSP)
		skb_trim(skb, skb->len - 2);

	crc = crc_ccitt(0xffff, skb->data, skb->len);

	/* The MSB should be transmitted first, but UART transmits LSB first.
	 * reverse it.
	 */
	crc = bitrev16(crc);

	skb_put_u8(skb, (crc >> 8) & 0x00ff);
	skb_put_u8(skb, crc & 0x00ff);
}

static bool bcsp_skb_check_crc(struct sk_buff *skb)
{
	unsigned int crc;
	crc = (skb->data[skb->len - 2] << 8) | (skb->data[skb->len - 1]);
	BT_DBG("crc bytes : %04x\n", crc);
	BT_DBG("bit revert: %04x\n", bitrev16(crc));
	BT_DBG("expect crc: %04x\n", crc_ccitt(0xffff, skb->data, skb->len - 2));
	return bitrev16(crc) == crc_ccitt(0xffff, skb->data, skb->len - 2);
}

static inline bool bcsp_check_reliable(const unsigned char type)
{
	/* BCSP specification uses different packet type for Link Establish */
	switch (type) {
	case HCI_BCSP_TYPE_ACL:
	case HCI_BCSP_TYPE_CMD:
		return true;
	case HCI_BCSP_TYPE_ACK:
	case HCI_BCSP_TYPE_SCO:
	case HCI_BCSP_TYPE_ISO:
	case HCI_BCSP_TYPE_VENDOR:
	case HCI_BCSP_TYPE_LINK_CONTROL:
	default:
		return false;
	}
	return false;
}

#ifndef CONFIG_SLSI_BT_3WUTL
static inline unsigned char bcsp_type_to_channel(const unsigned char type)
{
	switch (type) {
	case HCI_BCSP_TYPE_ACK:
		return HCI_BCSP_CHANNEL_ACK;
	case HCI_BCSP_TYPE_LINK_CONTROL:
		return HCI_BCSP_CHANNEL_LINK_CONTROL;
	case HCI_BCSP_TYPE_CMD:
	case HCI_BCSP_TYPE_EVT:
		return HCI_BCSP_CHANNEL_CMD_EVT;
	case HCI_BCSP_TYPE_ACL:
		return HCI_BCSP_CHANNEL_ACL;
	case HCI_BCSP_TYPE_SCO:
		return HCI_BCSP_CHANNEL_SCO;
	case HCI_BCSP_TYPE_ISO:
		return HCI_BCSP_CHANNEL_ISO;
	case HCI_BCSP_TYPE_VENDOR:
		return HCI_BCSP_CHANNEL_MXLOG;
	default:
		return HCI_BCSP_CHANNEL_UNKNOWN;
	}
	return HCI_BCSP_CHANNEL_UNKNOWN;
}

static inline unsigned char bcsp_channel_to_type(unsigned char ch)
{
	switch (ch) {
	case HCI_BCSP_CHANNEL_ACK:
		return HCI_BCSP_TYPE_ACK;
	case HCI_BCSP_CHANNEL_LINK_CONTROL:
		return HCI_BCSP_TYPE_LINK_CONTROL;
	case HCI_BCSP_CHANNEL_CMD_EVT:
		return HCI_BCSP_TYPE_EVT;
	case HCI_BCSP_CHANNEL_ACL:
		return HCI_BCSP_TYPE_ACL;
	case HCI_BCSP_CHANNEL_SCO:
		return HCI_BCSP_TYPE_SCO;
	case HCI_BCSP_CHANNEL_ISO:
		return HCI_BCSP_TYPE_ISO;
	case HCI_BCSP_CHANNEL_MXLOG:
		return HCI_BCSP_TYPE_VENDOR;
	default:
		return HCI_BCSP_TYPE_UNKNOWN;
	}
	return HCI_BCSP_TYPE_UNKNOWN;
}
#endif

static void bcsp_skb_update_header(struct hci_bcsp *bcsp, struct sk_buff *skb)
{
	char type = GET_HCI_PKT_TYPE(skb);
	unsigned char hdr[4] = { 0, };
	size_t plen = 0;

	if (GET_HCI_PKT_TR_TYPE(skb) == HCI_TRANS_HCI) {
		plen = skb->len;
		skb_push(skb, sizeof(hdr));
	} else if (GET_HCI_PKT_TR_TYPE(skb) == HCI_TRANS_BCSP) {
		/* Do not add CRC bytes to payload length */
		plen = skb->len - sizeof(hdr) - 2;
	}

	if (bcsp_check_reliable(type)) {
		BT_DBG("reliable packet. seq=%d\n", bcsp->seq);
		hdr[0] = (bcsp->seq & 0x7) | (0x1 << 7);
		bcsp->seq = (bcsp->seq + 1) & 0x7;
	}
#ifndef CONFIG_SLSI_BT_3WUTL
	/* BCSP uses a different type for Link establish but it is unreliable
	 * packet. */
	type = bcsp_type_to_channel(type);
#endif

	hdr[0] |= ((bcsp->ack & 0x7) << 3) | ((bcsp->use_crc & 0x1) << 6);
	hdr[1] = ((plen & 0xF) << 4) | (type & 0xF);
	hdr[2] = plen >> 4;
	hdr[3] = ~(hdr[0] + hdr[1] + hdr[2]) & 0xff;

	memcpy(skb->data, hdr, sizeof(hdr));
}

enum {
	BCSP_LINK_MSG_UNKNOWN,
	BCSP_LINK_MSG_SYNC,
	BCSP_LINK_MSG_SYNC_RSP,
	BCSP_LINK_MSG_CONF,
	BCSP_LINK_MSG_CONF_RSP,
};

static struct sk_buff *bcsp_link_msg_build(int message)
{
#ifdef CONFIG_SLSI_BT_3WUTL
	char const sync_msg[]  = { 0x01, 0x7e },
		sync_rsp_msg[] = { 0x02, 0x7d };
	char    conf_msg[]     = { 0x03, 0xfc, 0x00 },
		conf_rsp_msg[] = { 0x04, 0x7b, 0x00 };
	char *msg;
#else
	char const sync_msg[]  = { 0xda, 0xdc, 0xed, 0xed },
		sync_rsp_msg[] = { 0xac, 0xaf, 0xef, 0xee },
		conf_msg[]     = { 0xad, 0xef, 0xac, 0xed },
		conf_rsp_msg[] = { 0xde, 0xad, 0xd0, 0xd0 };
	char const *msg;
#endif
	struct sk_buff *skb;
	unsigned char len = 0;

	switch (message) {
	case BCSP_LINK_MSG_SYNC:
		msg = sync_msg;
		len = sizeof(sync_msg);
		break;
	case BCSP_LINK_MSG_SYNC_RSP:
		msg = sync_rsp_msg;
		len = sizeof(sync_rsp_msg);
		break;
	case BCSP_LINK_MSG_CONF:
	case BCSP_LINK_MSG_CONF_RSP:
		if (message == BCSP_LINK_MSG_CONF) {
			msg = conf_msg;
			len = sizeof(conf_msg);
		} else {
			msg = conf_rsp_msg;
			len = sizeof(conf_rsp_msg);
		}
#ifdef CONFIG_SLSI_BT_3WUTL
		if (bcsp->credit == 0) {
			/* The first build of configuration field */
			msg[2] = HCI_BCSP_WINDOWSIZE & 0x7;
			msg[2] |= (HCI_BCSP_SW_FLOW_CTR & 0x1) << 3;
			msg[2] |= (HCI_BCSP_DATA_INT_TYPE & 0x1) << 4;
			msg[2] |= (HCI_BCSP_VERSION & 0x3) << 5;
		} else {
			/* Peer sent configuration field first. It needs to
			 * decide. It confirm all of peer's configurations */
			msg[2] = bcsp->credit & 0x7;
			msg[2] |= (bcsp_sw_fctrl & 0x1) << 3;
			msg[2] |= (bcsp->use_crc & 0x1) << 4;
		}
		msg[2] |= (HCI_BCSP_VERSION & 0x3) << 5;
#endif
		break;
	default:
		return NULL;
	}

	skb = alloc_hci_pkt_skb(len);
	skb_put_data(skb, msg, len);
	SET_HCI_PKT_TYPE(skb, HCI_BCSP_TYPE_LINK_CONTROL);

	return skb;
}

static unsigned int bcsp_link_msg_recv(struct sk_buff *skb)
{
	unsigned int msg;
#ifdef CONFIG_SLSI_BT_3WUTL
	const unsigned int sync_msg = 0x017e, sync_rsp_msg = 0x027d;
	const unsigned int conf_msg = 0x03fc, conf_rsp_msg = 0x047b;

	if (skb == NULL)
		return BCSP_LINK_MSG_UNKNOWN;

	msg = (skb->data[0] << 8) | skb->data[1];
#else
	const unsigned int sync_msg = 0xdadceded, sync_rsp_msg = 0xacafefee;
	const unsigned int conf_msg = 0xadefaced, conf_rsp_msg = 0xdeadd0d0;

	if (skb == NULL)
		return BCSP_LINK_MSG_UNKNOWN;

	msg = (skb->data[0] << 24) | (skb->data[1] << 16) | (skb->data[2] << 8)
		|  skb->data[3];
#endif

	if (msg == sync_msg) {
		return BCSP_LINK_MSG_SYNC;
	} else if (msg == sync_rsp_msg) {
		return BCSP_LINK_MSG_SYNC_RSP;
	} else if (msg == conf_msg) {
		return BCSP_LINK_MSG_CONF;
	} else if (msg == conf_rsp_msg) {
		return BCSP_LINK_MSG_CONF_RSP;
	}
	return BCSP_LINK_MSG_UNKNOWN;
}

static int bcsp_link_establishment(struct hci_bcsp *bcsp, struct sk_buff *rsp)
{
	struct sk_buff *skb = NULL;
	int msg = bcsp_link_msg_recv(rsp);

	if (bcsp == NULL)
		return -EINVAL;

	BT_DBG("link state=%d, msg=%d\n", bcsp->state, msg);
	if (rsp) {
		BT_DBG("stop resend timer\n");
		del_timer(&bcsp->resend_timer);
	}

	switch (bcsp->state) {
	case HCI_BCSP_LINK_UNINITITALIZED:
		switch (msg) {
		case BCSP_LINK_MSG_UNKNOWN:
			skb = bcsp_link_msg_build(BCSP_LINK_MSG_SYNC);
			break;
		case BCSP_LINK_MSG_SYNC:
			skb = bcsp_link_msg_build(BCSP_LINK_MSG_SYNC_RSP);
			break;
		case BCSP_LINK_MSG_SYNC_RSP:
			bcsp->state = HCI_BCSP_LINK_INITIALIZED;
			skb = bcsp_link_msg_build(BCSP_LINK_MSG_CONF);
			break;
		default:
			BT_ERR("Invalid message\n");
			return -EINVAL;
		}
		break;

	case HCI_BCSP_LINK_INITIALIZED:
		switch (msg) {
		case BCSP_LINK_MSG_UNKNOWN:    /* Time out */
			skb = bcsp_link_msg_build(BCSP_LINK_MSG_CONF);
			break;
		case BCSP_LINK_MSG_SYNC:
			skb = bcsp_link_msg_build(BCSP_LINK_MSG_SYNC_RSP);
			break;
		case BCSP_LINK_MSG_CONF:
			skb = bcsp_link_msg_build(BCSP_LINK_MSG_CONF_RSP);
			break;
		case BCSP_LINK_MSG_CONF_RSP:
			bcsp->seq      = 0;
			bcsp->ack      = 0;
#ifdef CONFIG_SLSI_BT_3WUTL
			bcsp->credit   =  skb->data[2] & 0x7;
			bcsp->sw_fctrl = (skb->data[2] >> 3) & 0x01;
			bcsp->use_crc  = (skb->data[2] >> 4) & 0x1;
#else
			bcsp->credit   =  4;
			bcsp->sw_fctrl =  0;
			bcsp->use_crc  =  1;
#endif
			BT_DBG("HCI bcsp link is actived. "
				"window=%u, use_crc=%d, sw_fctrl=%d\n",
				bcsp->credit, bcsp->use_crc, bcsp->sw_fctrl);

			bcsp->state = HCI_BCSP_LINK_ACTIVE;
			break;
		default:
			BT_ERR("Invalid message\n");
			return -EINVAL;
		}

		break;

	case HCI_BCSP_LINK_ACTIVE:
		switch (msg) {
		case BCSP_LINK_MSG_CONF:
			skb = bcsp_link_msg_build(BCSP_LINK_MSG_CONF_RSP);
			break;
		case BCSP_LINK_MSG_SYNC:
			BT_WARNING("BCSP_LINK_MSG_SYNC is received."
				" BT should be re-initialized.");
			slsi_bt_err(SLSI_BT_ERR_BCSP_FAIL);
			return -EINVAL;
		default:
			BT_ERR("Invalid message\n");
			return -EINVAL;
		}
		break;
	}

	if (skb) {
#ifdef CONFIG_SLSI_BT_3WUTL
		/* If the controller sends CONFIG message without a
		 * Configuration field, the host sends CONFIG RESPONSE message
		 * without a Configuration Field. */
		if (rsp && skb && msg == BCSP_LINK_MSG_CONF && rsp->len == 2)
			skb_trim(skb, 2);
#endif
		skb_queue_head(&bcsp->unrel_q, skb);
		//bcsp_txwork_continue(bcsp);
		if (bcsp->state != HCI_BCSP_LINK_ACTIVE) {
			BT_DBG("update resend timer\n");
			mod_timer(&bcsp->resend_timer, HCI_BCSP_RESEND_TIMEOUT);
		}
	}

	return 0;
}

static int bcsp_skb_send(struct hci_bcsp *bcsp, struct sk_buff *skb)
{
	struct sk_buff *slip_skb = NULL;

	if (bcsp == NULL || skb == NULL)
		return -EINVAL;

	/* BCSP packing */
	bcsp_skb_update_header(bcsp, skb);
	if (bcsp->use_crc)
		bcsp_skb_put_crc(skb);
	SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_BCSP);
	bcsp->ack_req = false;
	BT_DBG("BCSP Packet length=%d\n", skb->len);
	BTTR_TRACE_HEX(BTTR_TAG_BCSP_TX, skb);

	/* Slip Packing */
	slip_skb = slip_encode(skb->data, skb->len, bcsp->sw_fctrl);
	if (!slip_skb) {
		BT_ERR("error to allocate skb\n");
		slsi_bt_err(SLSI_BT_ERR_NOMEM);
		return -ENOMEM;
	}

	/*************************
	 * Send to next transport
	 *************************/
	BT_DBG("send slip packet len: %d\n", slip_skb->len);
	BTTR_TRACE_HEX(BTTR_TAG_SLIP_TX, slip_skb);
	return hci_trans_default_send_skb(bcsp->htr, slip_skb);
}

static inline bool rel_q_transmitable(struct hci_bcsp *bcsp)
{
	return !skb_queue_empty(&bcsp->rel_q) && bcsp->credit > 0;
}

static void bcsp_txwork_func(struct work_struct *work)
{
	struct hci_bcsp *bcsp = container_of(work, struct hci_bcsp, work_tx);
	struct sk_buff *skb;

	BT_DBG("enter: credits=%u, unrel_q#=%d, rel_q#=%d\n", bcsp->credit,
		skb_queue_len(&bcsp->unrel_q), skb_queue_len(&bcsp->rel_q));

	mutex_lock(&bcsp->lock);
	atomic_set(&bcsp->tx_running, true);

	while (!skb_queue_empty(&bcsp->unrel_q) || rel_q_transmitable(bcsp)) {
		/* Unreliable channel */
		skb = skb_dequeue(&bcsp->unrel_q);
		if (skb) {
			BT_DBG("Unreliable packet. len=%d\n", skb->len);
			mutex_unlock(&bcsp->lock);
			bcsp_skb_send(bcsp, skb);
			mutex_lock(&bcsp->lock);

			kfree_skb(skb);
		}

		/* Reliable channel */
		skb = bcsp->credit ? skb_dequeue(&bcsp->rel_q) : NULL;
		if (skb) {
			BT_DBG("Reliable packet. len=%d\n", skb->len);
			//mutex_unlock(&bcsp->lock);
			bcsp_skb_send(bcsp, skb);
			//mutex_lock(&bcsp->lock);

			bcsp->credit--;
			skb_queue_tail(&bcsp->sent_q, skb);
			BT_DBG("mod_timer\n");
			mod_timer(&bcsp->resend_timer, HCI_BCSP_RESEND_TIMEOUT);
		}
	}

	if (bcsp->ack_req) {
		BT_DBG("send ack: %u\n", bcsp->ack);
		/* The default value is same as following setting.
		 * SET_HCI_PKT_TYPE(skb, HCI_BCSP_TYPE_ACK);
		 * SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_HCI);
		 */
		skb = alloc_hci_pkt_skb(0);
		if (skb)
			bcsp_skb_send(bcsp, skb);
		kfree_skb(skb);
	}

	atomic_set(&bcsp->tx_running, false);
	mutex_unlock(&bcsp->lock);

	BT_DBG("exit\n");
}

static void bcsp_txwork_continue(struct hci_bcsp *bcsp)
{
	/* This function must be called in ciritical section that protected
	 * area by bcsp->lock. Make sure get bcsp->lock before it call.
	 */
	if (!mutex_is_locked(&bcsp->lock))
		BT_WARNING("bcsp is not locked. packets can be pending.\n");

	if (atomic_read(&bcsp->tx_running) == false)
		queue_work(bcsp->wq, &bcsp->work_tx);
}

static void bcsp_timeoutwork_func(struct work_struct *work)
{
	struct hci_bcsp *bcsp = container_of(work, struct hci_bcsp,
		work_timeout);
	struct sk_buff *skb;

	BT_DBG("timeout status=%d, resent count=%d, retry_limit=%d\n",
		bcsp->state, bcsp->resent_cnt, HCI_BCSP_RESEND_LIMIT);

	mutex_lock(&bcsp->lock);
	if (bcsp->state != HCI_BCSP_LINK_ACTIVE) {
		BT_DBG("link establishment timeout.\n");
		bcsp_link_establishment(bcsp, NULL);

	} else if (++bcsp->resent_cnt > HCI_BCSP_RESEND_LIMIT) {
		BT_DBG("The retry limit has been reached\n");
		slsi_bt_err(SLSI_BT_ERR_BCSP_RESEND_LIMIT);

	} else {
		BT_DBG("retry %d sent packet\n", skb_queue_len(&bcsp->sent_q));

		/* Move noack packets to sending queue */
		while ((skb = skb_dequeue_tail(&bcsp->sent_q)) != NULL) {
			skb_queue_head(&bcsp->rel_q, skb);
			bcsp->seq = (bcsp->seq - 1) & 0x7;
			bcsp->credit++;
		}
	}

	bcsp_txwork_continue(bcsp);
	mutex_unlock(&bcsp->lock);
}

static void bcsp_send_timeout(struct timer_list *t)
{
	struct hci_bcsp *bcsp = from_timer(bcsp, t, resend_timer);
	BT_DBG("ack timeout: %lu\n", HCI_BCSP_RESEND_TIMEOUT);
	queue_work(bcsp->wq, &bcsp->work_timeout);
}

static void bcsp_ack_handle(struct hci_bcsp *bcsp, unsigned char rack)
{
	unsigned char ack_cnt= ((rack + 8) - bcsp->rack) & 0x7;
	unsigned char unack_cnt = ((bcsp->seq + 8) - rack) & 0x7;

	BT_DBG("seq=%u, old_rack=%u\n", bcsp->seq, bcsp->rack);
	BT_DBG("received ack=%u, unack_cnt=%u\n", rack, unack_cnt);

	bcsp->credit += ack_cnt;
	if (bcsp->credit == 0) {
		BT_DBG("No more credit. wait for next ack\n");
		return;
	}

	/* Remove got ack packets */
	if (bcsp->seq == rack) {
		skb_queue_purge(&bcsp->sent_q);

		BT_DBG("del_timer\n");
		del_timer(&bcsp->resend_timer);
	} else {
		while (ack_cnt-- && !skb_queue_empty(&bcsp->sent_q))
			skb_dequeue(&bcsp->sent_q);
	}

	/* Restore seq no */
	bcsp->rack = rack;
	bcsp->resent_cnt = 0;
}

static int bcsp_skb_recv(struct hci_bcsp *bcsp, struct sk_buff *skb)
{
	unsigned char rseq, rack, use_crc, reliable, type;
	unsigned char check_sum;
	size_t len;

	BT_DBG("link state: %d, rx data len: %u\n", bcsp->state, skb->len);

	/* BCSP header */
	if (skb->len < 4) {
		BT_ERR("Small bcsp packet(%u)\n", skb->len);
		return -EINVAL;
	}

	check_sum = ~(skb->data[0] + skb->data[1] + skb->data[2]) & 0xff;
	if (check_sum != skb->data[3]) {
		BT_ERR("header checksum error: expected %d, but has %d\n",
			check_sum, skb->data[3]);
		return -EINVAL;
	}
	rseq      = skb->data[0] & 0x7;
	rack      = (skb->data[0] >> 3) & 0x7;
	use_crc   = (skb->data[0] >> 6) & 0x1;
	reliable  = (skb->data[0] >> 7) & 0x1;
	type      = skb->data[1] & 0xF;
	len       = ((skb->data[1] >> 4) & 0xF) | (skb->data[2] << 4);

#ifndef CONFIG_SLSI_BT_3WUTL
	type = bcsp_channel_to_type(type);
#endif

	BT_DBG("rx seq:%u, ack:%u, crc:%u, rel:%u, type:%u, payload len:%zu\n",
		rseq, rack, use_crc, reliable, type, len);

	if (skb->len != len + (use_crc ? 6 : 4)) {
		BT_ERR("slip payload length error. it has %u\n", skb->len);
		return -EINVAL;
	}

	if (use_crc) {
		if (bcsp_skb_check_crc(skb) == false) {
			BT_ERR("data integrity check error\n");
			return -EINVAL;
		}
		skb_trim(skb, skb->len - 2);
	}

	/* Handle reliable packet */
	if (reliable) {
		BT_DBG("Sent ack: %u, Received seq no: %u\n", bcsp->ack, rseq);
		if (bcsp->ack == rseq) {
			bcsp->ack = (bcsp->ack + 1) & 0x7;
		} else {
			bcsp->ack_mismatch++;
			BT_ERR("out of sequence packet error\n");
			return -EINVAL;
		}
		bcsp->ack_req = true;
	}
	bcsp_ack_handle(bcsp, rack);
	if (rel_q_transmitable(bcsp) || bcsp->ack_req)
		bcsp_txwork_continue(bcsp);

	/* Remove header */
	skb_pull(skb, 4);

	SET_HCI_PKT_TYPE(skb, type);
	SET_HCI_PKT_TR_TYPE(skb, HCI_TRANS_HCI);
	return 0;
}

static int hci_bcsp_skb_send(struct hci_trans *htr, struct sk_buff *skb)
{
	struct hci_bcsp *bcsp = get_hci_bcsp(htr);

	if (htr == NULL || skb == NULL || bcsp == NULL)
		return -EINVAL;

	if (htr->target == NULL)
		return -EINVAL;

	BT_DBG("hci package type:%d, len:%u\n",
		GET_HCI_PKT_TYPE(skb), skb->len);

	mutex_lock(&bcsp->lock);

	if (bcsp_check_reliable(GET_HCI_PKT_TYPE(skb))) {
		BT_DBG("queue to rel_q\n");
		skb_queue_tail(&bcsp->rel_q, skb);
	} else {
		BT_DBG("queue to unrel_q\n");
		skb_queue_tail(&bcsp->unrel_q, skb);
	}

	bcsp_txwork_continue(bcsp);

	mutex_unlock(&bcsp->lock);

	BT_DBG("done.\n");
	return 0;
}

static int hci_bcsp_skb_recv(struct hci_trans *htr, struct sk_buff *slip_skb)
{
	struct hci_bcsp *bcsp = get_hci_bcsp(htr);
	struct sk_buff *skb;
	int ret;

	if (htr == NULL || slip_skb == NULL || bcsp == NULL)
		return -EINVAL;

	if (htr->host == NULL)
		return -EINVAL;

	if (!slip_check_complete(slip_skb)) {
		BT_ERR("Not SLIP packet\n");
		return -EINVAL;
	}

	BTTR_TRACE_HEX(BTTR_TAG_SLIP_RX, slip_skb);
	skb = slip_decode(slip_skb->data, slip_skb->len);
	if (skb == NULL) {
		BT_ERR("Failed to slip decoding.\n");
		kfree_skb(slip_skb);
		return -ENOMEM;;
	}

	// rx bcsp
	BTTR_TRACE_HEX(BTTR_TAG_BCSP_RX, skb);
	mutex_lock(&bcsp->lock);
	ret = bcsp_skb_recv(bcsp, skb);
	mutex_unlock(&bcsp->lock);
	if (ret) {
		BT_WARNING("failed to bcsp. The packet is discarded. %d\n", ret);
		kfree_skb(slip_skb);
		return ret;
	}
	kfree_skb(slip_skb);


	switch(GET_HCI_PKT_TYPE(skb)) {
	case HCI_BCSP_TYPE_LINK_CONTROL:
		//bcsp_link_msg_handle(bcsp, skb);
		bcsp_link_establishment(bcsp, skb);
		bcsp_txwork_continue(bcsp);
		/* fall through */
	case HCI_BCSP_TYPE_ACK:
		kfree_skb(skb);
		return 0;
	default:
		break;
	}

	BT_DBG("Received len: %u\n", skb->len);
	return hci_trans_default_recv_skb(htr, skb);
}

static ssize_t hci_bcsp_recv(struct hci_trans *htr, const char *data,
		size_t count, unsigned int flags)
{
	struct hci_bcsp *bcsp = get_hci_bcsp(htr);
	ssize_t ret = 0;

	if (htr == NULL || bcsp == NULL || data == NULL)
		return 0;

	BT_DBG("count: %zu\n", count);
	ret = slip_collect_pkt(&bcsp->collect_skb, data, count);
	if (ret < 0) {
		BT_WARNING("cannot collect SLIP packet: drop all bytes\n");
		return count;
	} else if (bcsp->collect_skb == NULL) {
		BT_WARNING("cannot collect SLIP packet: drop %zd bytes\n", ret);
		return ret;
	}

	if (GET_HCI_PKT_TR_TYPE(bcsp->collect_skb) == HCI_TRANS_BCSP) {
		struct sk_buff *skb = bcsp->collect_skb;
		bcsp->collect_skb = NULL;

		BT_DBG("Receive completed slip packet: %u\n", skb->len);
		hci_bcsp_skb_recv(htr, skb);
	}

	/* return handled data length */
	return ret;
}

static int hci_bcsp_proc_show(struct hci_trans *htr, struct seq_file *m)
{
	struct hci_bcsp *bcsp = get_hci_bcsp(htr);
	if (bcsp == NULL)
		return 0;

	mutex_lock(&bcsp->lock);
	seq_printf(m, "\n  %s:\n", htr->name);
	seq_printf(m, "    TX Work running state  = %u\n",
		atomic_read(&bcsp->tx_running));
	seq_puts(m, "\n");
	seq_printf(m, "    Reliable queue length  = %u\n",
		skb_queue_len(&bcsp->rel_q));
	seq_printf(m, "    Unreliable queue length= %u\n",
		skb_queue_len(&bcsp->unrel_q));
	seq_printf(m, "    Wait for ack packets   = %u\n",
		skb_queue_len(&bcsp->sent_q));
	seq_puts(m, "\n");
	seq_printf(m, "    Re-send timeout        = %d ms\n", 1000/4);
	seq_printf(m, "    Re-send count (timeout)= %d\n", bcsp->resent_cnt);
	seq_printf(m, "    Ack mismatch count     = %d\n", bcsp->ack_mismatch);
	seq_puts(m, "\n");
	seq_printf(m, "    BCSP Link state        = %d\n", bcsp->state);
	seq_printf(m, "    BCSP Sequence number   = %u\n", bcsp->seq);
	seq_printf(m, "    BCSP Acknowledgement   = %u\n", bcsp->ack);
	seq_printf(m, "    BCSP Use CRC           = %d\n", bcsp->use_crc);
	seq_printf(m, "    BCSP Current credits   = %u\n", bcsp->credit);
	seq_printf(m, "    BCSP Ack requests      = %d\n", bcsp->ack_req);

	mutex_unlock(&bcsp->lock);
	return 0;
}

int hci_bcsp_init(struct hci_trans *htr, struct hci_trans *next)
{
	struct hci_bcsp *bcsp;

	if (htr == NULL)
		return -EINVAL;

	bcsp = kzalloc(sizeof(struct hci_bcsp), GFP_KERNEL);
	if (bcsp == NULL)
		return -ENOMEM;

	bcsp->wq = create_singlethread_workqueue("bt_hci_bcsp_wq");
	if (bcsp->wq == NULL) {
		kfree(bcsp);
		BT_ERR("Fail to create workqueue\n");
		return -ENOMEM;
	}

	bcsp->htr = htr;
	bcsp->use_crc = false;

	INIT_WORK(&bcsp->work_tx, bcsp_txwork_func);
	INIT_WORK(&bcsp->work_timeout, bcsp_timeoutwork_func);

	mutex_init(&bcsp->lock);

	skb_queue_head_init(&bcsp->rel_q);
	skb_queue_head_init(&bcsp->sent_q);
	skb_queue_head_init(&bcsp->unrel_q);

	timer_setup(&bcsp->resend_timer, bcsp_send_timeout, 0);

	/* Setup HCI transport for bcsp */
	hci_trans_init(htr, "BCSP Transport");
	htr->send_skb = hci_bcsp_skb_send;
	htr->recv_skb = hci_bcsp_skb_recv;
	htr->recv = hci_bcsp_recv;
	htr->proc_show = hci_bcsp_proc_show;
	htr->deinit = hci_bcsp_deinit;
	htr->tdata = bcsp;

	/* Join BCSP stack to target stack */
	hci_trans_setup_host_target(htr, next);

	/* Link Establishment */
	bcsp_link_establishment(bcsp, NULL);
	bcsp_txwork_continue(bcsp);

	return 0;
}

void hci_bcsp_deinit(struct hci_trans *htr)
{
	struct hci_bcsp *bcsp = get_hci_bcsp(htr);

	BT_DBG("bcsp=%p\n", bcsp);
	if (bcsp) {
		flush_workqueue(bcsp->wq);
#ifndef KUNIT_SLSI_BT_BCSP_EXCLUDE_DESTROY_WORKQUEUE
		destroy_workqueue(bcsp->wq);
#endif /* Ref. hci_bcsp_unittest.c for more information*/
		cancel_work_sync(&bcsp->work_tx);
		cancel_work_sync(&bcsp->work_timeout);

		mutex_lock(&bcsp->lock);
		del_timer(&bcsp->resend_timer);

		if (bcsp->collect_skb)
			kfree_skb(bcsp->collect_skb);

		skb_queue_purge(&bcsp->rel_q);
		skb_queue_purge(&bcsp->sent_q);
		skb_queue_purge(&bcsp->unrel_q);

		mutex_unlock(&bcsp->lock);
		mutex_destroy(&bcsp->lock);
		kfree(bcsp);

	}
	htr->tdata = NULL;
	hci_trans_deinit(htr);
	BT_DBG("done.\n");
}
