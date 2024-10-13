/****************************************************************************
 *
 * Copyright (c) 2023 - 2023 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "dev.h"
#include "debug.h"
#include "mib.h"
#include "mlme.h"
#include "local_packet_capture.h"
#include <linux/ieee80211.h>
#include <scsc/scsc_mx.h>

#define LPC_HASH_BITS 8
#define LPC_TABLE_SIZE BIT(LPC_HASH_BITS)

static unsigned int lpc_process_max = 516;
/* Module parameter for test */
module_param(lpc_process_max, int, 0644);
MODULE_PARM_DESC(lpc_process_max, "lpc process max");

static u32 ampdu_tx_agg_idx = 0;

struct lpc_skb_data {
	u32 lpc_tag;
	struct sk_buff *lpc_skb;
	struct list_head list;
};

struct slsi_lpc_struct {
	int enable;
	bool is_mgmt_type;
	bool is_data_type;
	bool is_ctrl_type;
	struct work_struct lpc_work;
	struct sk_buff_head queue;
	/* protects queue - send packet queue */
	spinlock_t lpc_queue_lock;
	struct list_head lpc_rx_ampdu_info_table[LPC_TABLE_SIZE];
	/* protects lpc_rx_ampdu_info_table */
	spinlock_t lpc_ampdu_info_lock;
	struct list_head lpc_tx_buffer_table[LPC_TABLE_SIZE];
	/* protects lpc_tx_buffer_table*/
	spinlock_t lpc_tx_skb_buffer_lock;
	struct list_head lpc_rx_buffer_table[LPC_TABLE_SIZE];
	/* protects lpc_rx_buffer_table*/
	spinlock_t lpc_rx_skb_buffer_lock;
} slsi_lpc_manager;

static const struct genl_multicast_group slsi_lpc_mcgrp[] = {
	{ .name = "slsi_lpc_grp", },
};

#if (KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE)
 /* attribute policy */
static struct nla_policy slsi_lpc_policy[] = {
	[SLSI_LPC_ATTR_PKT_LOG_TYPE] = {.type = NLA_U8}, /* u8 */
	[SLSI_LPC_ATTR_PKT_PAYLOAD] = {.type = NLA_BINARY}, /* binary */
};
#endif

 /* handler */
static int slsi_lpc_test_cb(struct sk_buff *skb, struct genl_info *info)
{
	return 0;
}

/* operation definition */
static struct genl_ops slsi_lpc_ops[] = {
	{
		.cmd = SLSI_LPC_C_TEST,
		.flags = 0,
#if (KERNEL_VERSION(5, 4, 0) > LINUX_VERSION_CODE)
		.policy = slsi_lpc_policy,
#endif
		.doit = slsi_lpc_test_cb,
		.dumpit = NULL,
	},
};

 /* family definition */
static struct genl_family slsi_lpc_family = {
	.hdrsize    = 0,
	.name       = "slsi_lpc_family",
	.version    = 1,
	.maxattr    = SLSI_LPC_ATTR_MAX,
	.ops        = slsi_lpc_ops,
	.n_ops      = ARRAY_SIZE(slsi_lpc_ops),
	.mcgrps     = slsi_lpc_mcgrp,
	.n_mcgrps   = ARRAY_SIZE(slsi_lpc_mcgrp),
};

static size_t slsi_lpc_nlmsg_size(size_t payload_len)
{
	size_t size = nlmsg_msg_size(GENL_HDRLEN + slsi_lpc_family.hdrsize);

	return NLMSG_ALIGN(size) +
		// SLSI_LPC_ATTR_PKT_LOG_TYPE: u8
		nla_total_size(sizeof(u8)) +
		// SLSI_LPC_ATTR_PKT_PAYLOAD: binary
		nla_total_size(payload_len);
}

static void *slsi_lpc_skb_to_nlmsg(struct sk_buff *skb)
{
	unsigned int payload_len;
	struct slsi_skb_cb *log_cb;
	struct sk_buff *msg;
	struct nlattr *attr;
	void *msg_head;

	payload_len = skb->len;
	log_cb = (struct slsi_skb_cb *)skb->cb;

	msg = genlmsg_new(slsi_lpc_nlmsg_size(payload_len), GFP_ATOMIC);
	if (!msg)
		return NULL;

	msg_head = genlmsg_put(msg, 0, 0, &slsi_lpc_family, 0, 0);
	if (!msg_head) {
		nlmsg_free(msg);
		return NULL;
	}

	if (nla_put_u8(msg, SLSI_LPC_ATTR_PKT_LOG_TYPE, SLSI_LOCAL_PACKET_CAPTURE_DATA)) {
		genlmsg_cancel(msg, msg_head);
		nlmsg_free(msg);
		return NULL;
	}
	attr = skb_put(msg, nla_total_size(payload_len));
	if (attr) {
		attr->nla_type = SLSI_LPC_ATTR_PKT_PAYLOAD;
		attr->nla_len = nla_attr_size(payload_len);
		memcpy(nla_data(attr), skb->data, payload_len);
	} else {
		genlmsg_cancel(msg, msg_head);
		nlmsg_free(msg);
			return NULL;
	}
	genlmsg_end(msg, msg_head);

	return msg;
}

static void slsi_lpc_work_func(struct work_struct *work)
{
	struct slsi_lpc_struct *slsi_lpc_manager_w = container_of(work, struct slsi_lpc_struct, lpc_work);
	struct sk_buff *skb;
	void *nlmsg;
	int cnt = 0;

	spin_lock_bh(&slsi_lpc_manager_w->lpc_queue_lock);
	if (skb_queue_empty(&slsi_lpc_manager_w->queue)) {
		spin_unlock_bh(&slsi_lpc_manager_w->lpc_queue_lock);
		return;
	}
	while (!skb_queue_empty(&slsi_lpc_manager_w->queue)) {
		skb = __skb_dequeue(&slsi_lpc_manager_w->queue);
		nlmsg = slsi_lpc_skb_to_nlmsg(skb);
		if (!nlmsg) {
			skb_queue_head(&slsi_lpc_manager_w->queue, skb);
			SLSI_INFO_NODEV("nlmsg error\n");
			break;
		}
		genlmsg_multicast(&slsi_lpc_family, nlmsg, 0, 0, GFP_ATOMIC);
		cnt++;
		consume_skb(skb);
	}
	spin_unlock_bh(&slsi_lpc_manager_w->lpc_queue_lock);
}

static bool slsi_lpc_filter_capture_packet_type(u16 fc)
{
	if ((slsi_lpc_manager.is_mgmt_type && ieee80211_is_mgmt(fc)) ||
	    (slsi_lpc_manager.is_data_type && ieee80211_is_data(fc)) ||
	    (slsi_lpc_manager.is_ctrl_type && ieee80211_is_ctl(fc)))
		return true;
	return false;
}

void slsi_lpc_clear_packet_data(struct list_head *packet_list)
{
	struct lpc_skb_data *lpc_data, *lpc_data_tmp;

	list_for_each_entry_safe(lpc_data, lpc_data_tmp, packet_list, list) {
		list_del(&lpc_data->list);
		kfree_skb(lpc_data->lpc_skb);
		kfree(lpc_data);
	}
}

static struct lpc_skb_data *slsi_lpc_find_packet_data(u32 lpc_tag, struct list_head *packet_list)
{
	struct lpc_skb_data *lpc_data, *lpc_data_tmp;

	list_for_each_entry_safe(lpc_data, lpc_data_tmp, packet_list, list) {
		if (lpc_data->lpc_tag == lpc_tag)
			return lpc_data;
	}
	return NULL;
}

static struct sk_buff *slsi_lpc_delete_packet_data(u32 lpc_tag, struct list_head *packet_list)
{
	struct lpc_skb_data *lpc_data, *lpc_data_tmp;
	struct sk_buff *ret_skb;

	list_for_each_entry_safe(lpc_data, lpc_data_tmp, packet_list, list) {
		if (lpc_data->lpc_tag == lpc_tag) {
			list_del(&lpc_data->list);
			ret_skb = lpc_data->lpc_skb;
			kfree(lpc_data);
			return ret_skb;
		}
	}
	return NULL;
}

static int __slsi_lpc_add_packet_data(u32 lpc_tag, struct sk_buff *skb, struct list_head *packet_list)
{
	struct sk_buff *c_skb;
	struct lpc_skb_data *new_lpc_data;

	new_lpc_data = slsi_lpc_find_packet_data(lpc_tag, packet_list);
	if (new_lpc_data) {
		c_skb = slsi_lpc_delete_packet_data(lpc_tag, packet_list);
		if (c_skb)
			consume_skb(c_skb);
	}
	new_lpc_data = kmalloc(sizeof(*new_lpc_data), GFP_ATOMIC);
	c_skb = skb_copy(skb, GFP_ATOMIC);
	new_lpc_data->lpc_tag = lpc_tag;
	new_lpc_data->lpc_skb = c_skb;
	INIT_LIST_HEAD(&new_lpc_data->list);
	list_add_tail(&new_lpc_data->list, packet_list);

	return 0;
}

int slsi_lpc_add_packet_data(u32 lpc_tag, struct sk_buff *skb, int data_type)
{
	int ret = -EINVAL;

	if (!slsi_lpc_manager.enable)
		return 0;

	if (data_type == SLSI_LPC_DATA_TYPE_TX) {
		spin_lock_bh(&slsi_lpc_manager.lpc_tx_skb_buffer_lock);
		ret = __slsi_lpc_add_packet_data(lpc_tag, skb, &slsi_lpc_manager.lpc_tx_buffer_table[lpc_tag & (LPC_TABLE_SIZE - 1)]);
		spin_unlock_bh(&slsi_lpc_manager.lpc_tx_skb_buffer_lock);
	} else if (data_type == SLSI_LPC_DATA_TYPE_RX) {
		spin_lock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
		ret = __slsi_lpc_add_packet_data(lpc_tag, skb, &slsi_lpc_manager.lpc_rx_buffer_table[lpc_tag & (LPC_TABLE_SIZE - 1)]);
		spin_unlock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
	} else if (data_type == SLSI_LPC_DATA_TYPE_AMPDU_RX_INFO) {
		spin_lock_bh(&slsi_lpc_manager.lpc_ampdu_info_lock);
		ret = __slsi_lpc_add_packet_data(lpc_tag, skb, &slsi_lpc_manager.lpc_rx_ampdu_info_table[lpc_tag & (LPC_TABLE_SIZE - 1)]);
		spin_unlock_bh(&slsi_lpc_manager.lpc_ampdu_info_lock);
	}

	return ret;
}

#define MAC_TIMESTAMP_MAX (BIT(32))
#define CASE1_CASE2_DISTANCE (MAC_TIMESTAMP_MAX * 9 / 10)

static u64 slsi_lpc_mac_timestamp_latest = MAC_TIMESTAMP_MAX;
static u64 slsi_lpc_mac_timestamp_base;

static u64 slsi_lpc_get_mac_timestamp(u32 mactime)
{
	bool is_case1_or_case2;

	if (slsi_lpc_mac_timestamp_latest == MAC_TIMESTAMP_MAX) {
		slsi_lpc_mac_timestamp_latest = mactime;
		return mactime;
	}
	/* Assuming that "ts" doesn't go back more than  MAC_TIMESTAMP_MAX*1/10
	 * with reference to self._tstamp_latest, there are two corner cases.
	 * +-------------- MAC_TIMESTAMP_MAX ---------------------+
	 * |---R1--|------MAC_TIMESTAMP_MAX*9/10-----------|--R3--|
	 *    |ts                                             |_tstamp_latest : Case 1 (ts is in the future)
	 *    |_tstamp_latest                                 |ts             : Case 2 (ts comes from the past)
	 */
	is_case1_or_case2 = abs(mactime - slsi_lpc_mac_timestamp_latest) > CASE1_CASE2_DISTANCE;
	if (is_case1_or_case2) {
		if ((u64)mactime < slsi_lpc_mac_timestamp_latest) {
			slsi_lpc_mac_timestamp_base += MAC_TIMESTAMP_MAX;
			slsi_lpc_mac_timestamp_latest = mactime;
			return slsi_lpc_mac_timestamp_base + mactime;
		} else {
			return slsi_lpc_mac_timestamp_base - MAC_TIMESTAMP_MAX + mactime;
		}
	} else {
		slsi_lpc_mac_timestamp_latest = max_t(u64, slsi_lpc_mac_timestamp_latest, mactime);
		return slsi_lpc_mac_timestamp_base + mactime;
	}

	return 0;
}

static struct sk_buff *slsi_lpc_make_rx_radiotap_hdr(struct lpc_type_phy_dollop *pd, struct sk_buff *skb)
{
	struct rx_radiotap_hdr rx_radiotap;
	struct rx_radiotap_hdr *pradiotap_hdr;
	u32 is_present = 0;

	memset(&rx_radiotap, 0, sizeof(struct rx_radiotap_hdr));
	rx_radiotap.hdr.it_len = cpu_to_le16 (sizeof(struct rx_radiotap_hdr));

	rx_radiotap.rt_tsft = cpu_to_le64(slsi_lpc_get_mac_timestamp(pd->rx_start_mactime));
	is_present |= 1 << IEEE80211_RADIOTAP_TSFT;
	//TODO: make rate
	is_present |= 1 << IEEE80211_RADIOTAP_RATE;
	rx_radiotap.rssi = cpu_to_le16(pd->rssi);
	is_present |= 1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL;
	rx_radiotap.snr = cpu_to_le16(pd->snr);
	is_present |= 1 << IEEE80211_RADIOTAP_DBM_ANTNOISE;
	rx_radiotap.hdr.it_present = cpu_to_le32 (is_present);

	pradiotap_hdr = skb_push(skb, sizeof(struct rx_radiotap_hdr));
	memcpy(pradiotap_hdr, &rx_radiotap, sizeof(struct rx_radiotap_hdr));

	return skb;
}

static int slsi_lpc_send_mpdu_tx(struct lpc_type_mpdu_tx *mpdu_tx)
{
	struct sk_buff *c_skb = NULL;
	struct tx_radiotap_hdr_mpdu tx_radiotap;
	u32 is_present = 0;

	c_skb = alloc_skb(sizeof(struct tx_radiotap_hdr_mpdu) + mpdu_tx->content_len, GFP_ATOMIC);
	if (!c_skb) {
		SLSI_ERR_NODEV("Alloc skb Failed\n");
		return -1;
	}
	skb_reserve(c_skb, sizeof(struct tx_radiotap_hdr_mpdu));
	memcpy(skb_put(c_skb, mpdu_tx->content_len), mpdu_tx->content, mpdu_tx->content_len);
	if (!slsi_lpc_filter_capture_packet_type(((struct ieee80211_hdr *)c_skb->data)->frame_control)) {
		kfree_skb(c_skb);
		c_skb = NULL;
		return -1;
	}

	memset(&tx_radiotap, 0, sizeof(struct tx_radiotap_hdr_mpdu));
	tx_radiotap.hdr.it_len = cpu_to_le16 (sizeof(struct tx_radiotap_hdr_mpdu));
	tx_radiotap.rt_tsft = cpu_to_le64(slsi_lpc_get_mac_timestamp(mpdu_tx->tx_mactime));
	is_present |= 1 << IEEE80211_RADIOTAP_TSFT;
	tx_radiotap.hdr.it_present = cpu_to_le32(is_present);
	memcpy(skb_push(c_skb, sizeof(struct tx_radiotap_hdr_mpdu)), &tx_radiotap, sizeof(struct tx_radiotap_hdr_mpdu));

	spin_lock_bh(&slsi_lpc_manager.lpc_queue_lock);
	__skb_queue_tail(&slsi_lpc_manager.queue, c_skb);
	spin_unlock_bh(&slsi_lpc_manager.lpc_queue_lock);
	schedule_work(&slsi_lpc_manager.lpc_work);

	return 0;
}

static int slsi_lpc_make_tx_msdu_list(u32 start_tag, u8 aggs, struct sk_buff_head *msdu_list)
{
	struct lpc_skb_data *lpc_data = NULL;
	struct sk_buff *c_skb = NULL;
	int i, cnt = 0;
	u32 lpc_tag;

	if (start_tag < 1 || start_tag > 0xfffe)
		return 0;

	for (i = 0; i < aggs; i++) {
		lpc_tag = start_tag + i;
		spin_lock_bh(&slsi_lpc_manager.lpc_tx_skb_buffer_lock);
		lpc_data = slsi_lpc_find_packet_data(lpc_tag, &slsi_lpc_manager.lpc_tx_buffer_table[lpc_tag & (LPC_TABLE_SIZE - 1)]);
		if (lpc_data) {
			c_skb = slsi_lpc_delete_packet_data(lpc_tag, &slsi_lpc_manager.lpc_tx_buffer_table[lpc_tag & (LPC_TABLE_SIZE - 1)]);
			if (c_skb) {
				__skb_queue_tail(msdu_list, c_skb);
				cnt++;
			}
		}
		spin_unlock_bh(&slsi_lpc_manager.lpc_tx_skb_buffer_lock);
	}
	return cnt;
}

static struct sk_buff *slsi_lpc_data_header_amsdu_tx(struct lpc_type_ampdu_tx *ampdu_tx, int idx, struct sk_buff *skb)
{
	struct ieee80211_hdr_3addr tx_header;

	memset(&tx_header, 0, sizeof(struct ieee80211_hdr_3addr));
	tx_header.frame_control = cpu_to_le16(ampdu_tx->fc);
	SLSI_ETHER_COPY(tx_header.addr1, (u8 *)ampdu_tx->ra);
	SLSI_ETHER_COPY(tx_header.addr2, (u8 *)ampdu_tx->ta);
	SLSI_ETHER_COPY(tx_header.addr3, (u8 *)ampdu_tx->ra);
	tx_header.seq_ctrl = cpu_to_le16(ampdu_tx->start_seq + (idx << 4));
	if (ieee80211_has_order(ampdu_tx->fc))
		memcpy(skb_push(skb, IEEE80211_HT_CTL_LEN), &ampdu_tx->rts_rate, IEEE80211_HT_CTL_LEN);
	if (ieee80211_is_data_qos(ampdu_tx->fc)) {
		u16 qos_ctl = 0;
		qos_ctl |= cpu_to_le16(ampdu_tx->tid) | cpu_to_le16(IEEE80211_QOS_CTL_A_MSDU_PRESENT);
		memcpy(skb_push(skb, IEEE80211_QOS_CTL_LEN), &qos_ctl, IEEE80211_QOS_CTL_LEN);
	}

	memcpy(skb_push(skb, sizeof(struct ieee80211_hdr_3addr)), &tx_header, sizeof(struct ieee80211_hdr_3addr));

	return skb;
}

static struct sk_buff *slsi_lpc_mgmt_header_amsdu_tx(struct lpc_type_ampdu_tx *ampdu_tx, int idx, struct sk_buff *skb)
{
	struct ieee80211_hdr_3addr tx_header;

	memset(&tx_header, 0, sizeof(struct ieee80211_hdr_3addr));
	tx_header.frame_control = cpu_to_le16(ampdu_tx->fc);
	SLSI_ETHER_COPY(tx_header.addr1, (u8 *)ampdu_tx->ra);
	SLSI_ETHER_COPY(tx_header.addr2, (u8 *)ampdu_tx->ta);
	SLSI_ETHER_COPY(tx_header.addr3, (u8 *)ampdu_tx->ra);
	tx_header.seq_ctrl = cpu_to_le16(ampdu_tx->start_seq + (idx << 4));
	if (ieee80211_has_order(ampdu_tx->fc))
		memcpy(skb_push(skb, IEEE80211_HT_CTL_LEN), &ampdu_tx->rts_rate, IEEE80211_HT_CTL_LEN);
	memcpy(skb_push(skb, sizeof(struct ieee80211_hdr_3addr)), &tx_header, sizeof(struct ieee80211_hdr_3addr));

	return skb;
}

static int slsi_lpc_send_ampdu_tx(struct lpc_type_ampdu_tx *ampdu_tx)
{
	struct sk_buff *c_skb = NULL;
	struct sk_buff *skb = NULL;
	struct tx_radiotap_hdr_ampdu tx_radiotap;
	int i, has_rts;
	struct sk_buff_head msdu_list, mpdu_list;
	int copy_skb_hdr_size = sizeof(struct tx_radiotap_hdr_ampdu) + sizeof(struct ieee80211_qos_hdr) + 50;
	u32 is_present = 0;
	u32 payload_len = 0;

	has_rts = (ampdu_tx->flag16 & LPC_TX_FLAG_RTS_REQUESTED);
	if (has_rts)
		SLSI_INFO_NODEV("has_rts\n");

	if (!slsi_lpc_filter_capture_packet_type(ampdu_tx->fc)) {
		SLSI_INFO_NODEV("filtering\n");
		return -1;
	}

	__skb_queue_head_init(&mpdu_list);
	ampdu_tx_agg_idx = (ampdu_tx_agg_idx + 1) % (BIT(32) - 1);

	for (i = 0; i < LPC_NUM_MPDU_MAX_IN_AMPDU_TX; i++) {
		if (!(ampdu_tx->req_bitmap & BIT(i)))
			continue;

		ampdu_tx->fc &= ~IEEE80211_FCTL_RETRY;
		if (ampdu_tx->retry_bitmap & BIT(i)) {
			ampdu_tx->fc |= IEEE80211_FCTL_RETRY;
		}
		__skb_queue_head_init(&msdu_list);

		if (!slsi_lpc_make_tx_msdu_list(ampdu_tx->tx_tag_list[i], ampdu_tx->fw_agg_tags[i], &msdu_list))
			continue;

		payload_len = 0;
		c_skb = alloc_skb(copy_skb_hdr_size + ampdu_tx->mpdu_len[i], GFP_ATOMIC);
		skb_reserve(c_skb, copy_skb_hdr_size);
		while (!skb_queue_empty(&msdu_list)) {
			skb = __skb_dequeue(&msdu_list);
			if (ampdu_tx->mpdu_len[i] > payload_len + skb->len) {
				skb_put_data(c_skb, skb->data, skb->len);
				if (skb->len % 4 != 0) {
					int pad = 4 - (skb->len % 4);
					memset(skb_put(c_skb, pad), 0, pad);
				}
				payload_len += skb->len;
			} else {
				SLSI_WARN_NODEV("mpdu_len:%d payload_len:%d skb->len:%d\n", ampdu_tx->mpdu_len[i], payload_len, skb->len);
			}
			consume_skb(skb);
		}

		if (ieee80211_is_data(ampdu_tx->fc))
			c_skb = slsi_lpc_data_header_amsdu_tx(ampdu_tx, i, c_skb);
		else if (ieee80211_is_mgmt(ampdu_tx->fc))
			c_skb = slsi_lpc_mgmt_header_amsdu_tx(ampdu_tx, i, c_skb);
		else {
			kfree_skb(c_skb);
			c_skb = NULL;
		}

		if (!c_skb)
			continue;

		memset(&tx_radiotap, 0, sizeof(struct tx_radiotap_hdr_ampdu));
		tx_radiotap.hdr.it_len = cpu_to_le16 (sizeof(struct tx_radiotap_hdr_ampdu));
		tx_radiotap.rt_tsft = cpu_to_le64(slsi_lpc_get_mac_timestamp(ampdu_tx->tx_mactime));
		is_present |= 1 << IEEE80211_RADIOTAP_TSFT;
		tx_radiotap.ampdu_flags |= cpu_to_le16(IEEE80211_RADIOTAP_AMPDU_LAST_KNOWN);
		tx_radiotap.ampdu_reference = cpu_to_le32(ampdu_tx_agg_idx);
		if ((i == LPC_NUM_MPDU_MAX_IN_AMPDU_TX - 1) || BIT(i + 1) > ampdu_tx->req_bitmap) {
			/* LAST MPDU */
			tx_radiotap.ampdu_flags |= cpu_to_le16(IEEE80211_RADIOTAP_AMPDU_IS_LAST);
		}
		is_present |= 1 << IEEE80211_RADIOTAP_AMPDU_STATUS;
		tx_radiotap.hdr.it_present = cpu_to_le32(is_present);
		memcpy(skb_push(c_skb, sizeof(struct tx_radiotap_hdr_ampdu)), &tx_radiotap, sizeof(struct tx_radiotap_hdr_ampdu));

		spin_lock_bh(&slsi_lpc_manager.lpc_queue_lock);
		__skb_queue_tail(&slsi_lpc_manager.queue, c_skb);
		spin_unlock_bh(&slsi_lpc_manager.lpc_queue_lock);
		schedule_work(&slsi_lpc_manager.lpc_work);
	}

	return 0;
}

static struct sk_buff *slsi_lpc_send_mpdu_rx(struct lpc_type_mpdu_rx *mpdu_rx)
{
	struct sk_buff *c_skb = NULL;

	if (mpdu_rx->lpc_tag > 0 && mpdu_rx->lpc_tag < 0xffff) {
		struct lpc_skb_data *lpc_data = NULL;

		spin_lock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
		lpc_data = slsi_lpc_find_packet_data(mpdu_rx->lpc_tag, &slsi_lpc_manager.lpc_rx_buffer_table[mpdu_rx->lpc_tag & (LPC_TABLE_SIZE - 1)]);
		spin_unlock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
		if (lpc_data) {
			spin_lock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
			c_skb = slsi_lpc_delete_packet_data(mpdu_rx->lpc_tag, &slsi_lpc_manager.lpc_rx_buffer_table[mpdu_rx->lpc_tag & (LPC_TABLE_SIZE - 1)]);
			spin_unlock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
			if (!c_skb) {
				goto exit;
			}
			if (!slsi_lpc_filter_capture_packet_type(((struct ieee80211_hdr *)c_skb->data)->frame_control)) {
				kfree_skb(c_skb);
				c_skb = NULL;
			}
		}
	} else {
		c_skb = alloc_skb(sizeof(struct rx_radiotap_hdr) + mpdu_rx->content_len, GFP_ATOMIC);
		if (!c_skb) {
			SLSI_ERR_NODEV("Alloc skb Failed\n");
			goto exit;
		}
		skb_reserve(c_skb, sizeof(struct rx_radiotap_hdr));
		memcpy(skb_put(c_skb, mpdu_rx->content_len), mpdu_rx->content, mpdu_rx->content_len);
		if (!slsi_lpc_filter_capture_packet_type(((struct ieee80211_hdr *)c_skb->data)->frame_control)) {
			kfree_skb(c_skb);
			c_skb = NULL;
		}
	}
exit:
	return c_skb;
}

static int slsi_lpc_make_rx_amsdu_list(struct lpc_type_ampdu_rx *ampdu_rx, struct sk_buff_head *amsdu_list, struct sk_buff_head *info_list)
{
	struct sk_buff *c_skb = NULL;
	int i, cnt = 0;
	int num_mpdu = ampdu_rx->num_mpdu;

	if (ampdu_rx->lpc_tag > 0 && ampdu_rx->lpc_tag < 0xffff) {
		struct lpc_skb_data *lpc_data = NULL;
		u32 lpc_tag;

		for (i = 0; i < num_mpdu; i++) {
			lpc_tag = ampdu_rx->lpc_tag + i;
			if (lpc_tag == LPC_TAG_NUM_MAX)
				lpc_tag = 1;
			c_skb = NULL;
			spin_lock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
			lpc_data = slsi_lpc_find_packet_data(lpc_tag, &slsi_lpc_manager.lpc_rx_buffer_table[lpc_tag & (LPC_TABLE_SIZE - 1)]);
			spin_unlock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
			if (lpc_data) {
				spin_lock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
				c_skb = slsi_lpc_delete_packet_data(lpc_tag, &slsi_lpc_manager.lpc_rx_buffer_table[lpc_tag & (LPC_TABLE_SIZE - 1)]);
				spin_unlock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
				if (!c_skb)
					continue;

				if (!slsi_lpc_filter_capture_packet_type(((struct ieee80211_hdr *)c_skb->data)->frame_control)) {
					kfree_skb(c_skb);
					c_skb = NULL;
				}
				if (c_skb) {
					cnt++;
					__skb_queue_tail(amsdu_list, c_skb);
				}
			} else {
				int size = sizeof(struct lpc_type_ampdu_rx);

				c_skb = alloc_skb(sizeof(struct rx_radiotap_hdr) + size, GFP_ATOMIC);
				if (!c_skb) {
					SLSI_ERR_NODEV("Alloc skb Failed\n");
					continue;
				}
				ampdu_rx->lpc_tag = lpc_tag;
				skb_reserve(c_skb, sizeof(struct rx_radiotap_hdr));
				memcpy(skb_put(c_skb, size), ampdu_rx, size);
				cnt++;
				__skb_queue_tail(info_list, c_skb);
			}
		}
	}
	return cnt;
}

static void slsi_lpc_send_ampdu_rx(struct lpc_type_phy_dollop *phy_dollop, struct sk_buff_head *amsdu_list)
{
	struct sk_buff *skb = NULL;
	int cnt = 0;

	while (!skb_queue_empty(amsdu_list)) {
		skb = __skb_dequeue(amsdu_list);
		skb = slsi_lpc_make_rx_radiotap_hdr(phy_dollop, skb);

		spin_lock_bh(&slsi_lpc_manager.lpc_queue_lock);
		__skb_queue_tail(&slsi_lpc_manager.queue, skb);
		spin_unlock_bh(&slsi_lpc_manager.lpc_queue_lock);
		cnt++;
	}
	if (cnt)
		schedule_work(&slsi_lpc_manager.lpc_work);
}

static void slsi_lpc_save_ampdu_rx_info(struct lpc_type_phy_dollop *phy_dollop, struct sk_buff_head *info_list)
{
	struct sk_buff *skb = NULL;

	while (!skb_queue_empty(info_list)) {
		struct lpc_type_ampdu_rx * ampdu_rx;
		u32 lpc_tag;

		skb = __skb_dequeue(info_list);
		ampdu_rx = (struct lpc_type_ampdu_rx *)skb->data;
		lpc_tag = ampdu_rx->lpc_tag;
		skb = slsi_lpc_make_rx_radiotap_hdr(phy_dollop, skb);
		slsi_lpc_add_packet_data(lpc_tag, skb, SLSI_LPC_DATA_TYPE_AMPDU_RX_INFO);
	}
}

int slsi_lpc_send_ampdu_rx_later(u32 lpc_tag, struct sk_buff *skb)
{
	int ret = 0;
	struct sk_buff *f_skb = NULL;
	struct sk_buff *c_skb = NULL;
	struct lpc_skb_data *lpc_data = NULL;
	struct rx_radiotap_hdr *pradiotap_hdr;

	spin_lock_bh(&slsi_lpc_manager.lpc_ampdu_info_lock);
	lpc_data = slsi_lpc_find_packet_data(lpc_tag, &slsi_lpc_manager.lpc_rx_ampdu_info_table[lpc_tag & (LPC_TABLE_SIZE - 1)]);
	spin_unlock_bh(&slsi_lpc_manager.lpc_ampdu_info_lock);
	if (lpc_data) {
		spin_lock_bh(&slsi_lpc_manager.lpc_ampdu_info_lock);
		f_skb = slsi_lpc_delete_packet_data(lpc_tag, &slsi_lpc_manager.lpc_rx_ampdu_info_table[lpc_tag & (LPC_TABLE_SIZE - 1)]);
		spin_unlock_bh(&slsi_lpc_manager.lpc_ampdu_info_lock);
		if (!f_skb) {
			ret = 0;
			goto exit;
		}
		if (!slsi_lpc_filter_capture_packet_type(((struct ieee80211_hdr *)skb->data)->frame_control)) {
			kfree_skb(f_skb);
			f_skb = NULL;
			ret = 0;
			goto exit;
		}

		c_skb = skb_copy(skb, GFP_ATOMIC);
		pradiotap_hdr = skb_push(c_skb, sizeof(struct rx_radiotap_hdr));
		memcpy(pradiotap_hdr, f_skb->data, sizeof(struct rx_radiotap_hdr));

		spin_lock_bh(&slsi_lpc_manager.lpc_queue_lock);
		__skb_queue_tail(&slsi_lpc_manager.queue, c_skb);
		spin_unlock_bh(&slsi_lpc_manager.lpc_queue_lock);
		schedule_work(&slsi_lpc_manager.lpc_work);
		ret = 1;
	}
exit:
	return ret;
}
static bool slsi_lpc_is_required_read_index_change(u32 r, u32 buffer_size, u16 next_type)
{
	if (r + sizeof(struct tlv_hdr) > buffer_size)
		return true;
	if (next_type == LPC_TYPE_PADDING)
		return true;
	return false;
}

static bool slsi_lpc_check_last_packet(void *log_buffer, u32 r, u32 w, u32 buffer_size)
{
	struct tlv_hdr *tlv_data;
	int i;

	for (i = 0; i < 2; i++) {
		tlv_data = (struct tlv_hdr *)(log_buffer + r);
		r += sizeof(struct tlv_hdr) + tlv_data->len;
		if (slsi_lpc_is_required_read_index_change(r, buffer_size, tlv_data->type))
			r = 0;
		if (r == w)
			return true;
	}
	return false;
}

int slsi_lpc_get_packets_info(struct slsi_dev *sdev)
{
	void *log_buffer = NULL;
	struct log_descriptor *lpc_ld;
	struct tlv_container *tlv_con;
	struct tlv_hdr *tlv_data;
	void *log_start, *log_end;
	int todo_cnt = lpc_process_max;
	u32 cnt = 0;
	u32 r, w, bufsize;

	if (!slsi_lpc_manager.enable)
		return todo_cnt;

	log_buffer = scsc_service_mxlogger_buff(sdev->service);

	if (!log_buffer) {
		SLSI_ERR_NODEV("Can't get TLV Container\n");
		return todo_cnt;
	}

	lpc_ld = (struct log_descriptor *)log_buffer;
	if (lpc_ld->magic_number != LPC_STORAGE_DESCRIPTOR_MAGIC_NUMBER ||
	    lpc_ld->version_major != LPC_STORAGE_DESCRIPTOR_VERSION_MAJOR ||
	    lpc_ld->version_minor != LPC_STORAGE_DESCRIPTOR_VERSION_MINOR) {
		SLSI_ERR_NODEV("Wrong log_descriptor\n");
		return todo_cnt;
	}

	log_buffer += sizeof(struct log_descriptor);

	tlv_con = (struct tlv_container *)log_buffer;
	if (tlv_con->magic_number != LPC_TLV_CONTAINER_MAGIC_NUMBER) {
		SLSI_ERR_NODEV("Wrong tlv container\n");
		return todo_cnt;
	}
	r = tlv_con->r_index;
	w = tlv_con->w_index;
	log_buffer += sizeof(struct tlv_container);

	log_start = log_buffer;
	log_end = log_start + tlv_con->buffer_size;
	bufsize = tlv_con->buffer_size;

	while (r != w && todo_cnt > 0) {
		struct sk_buff_head amsdu_list, info_list;
		struct sk_buff *c_skb = NULL;
		int amsdu_list_cnt;

		tlv_data = (struct tlv_hdr *)(log_buffer + r);

		if (slsi_lpc_is_required_read_index_change(r, bufsize, tlv_data->type)) {
			r = 0;
			tlv_data = (struct tlv_hdr *)(log_buffer + r);
		}

		if (slsi_lpc_check_last_packet(log_buffer, r, w, bufsize))
			break;

		if (!tlv_data->type) {
			SLSI_INFO_NODEV("TLV Type is zero len:%x\n", tlv_data->len);
			break;
		}

		switch (tlv_data->type) {
		case LPC_TYPE_MPDU_TX:
			if (tlv_data->len != sizeof(struct lpc_type_mpdu_tx)) {
				SLSI_WARN_NODEV("MPDU_TX TLV length is mismatch:%x %x\n", tlv_data->len, sizeof(struct lpc_type_mpdu_tx));
				r += sizeof(struct tlv_hdr) + tlv_data->len;
				break;
			}
			r += sizeof(struct tlv_hdr);
			slsi_lpc_send_mpdu_tx((struct lpc_type_mpdu_tx *)(log_buffer + r));
			r += tlv_data->len;
			break;
		case LPC_TYPE_AMPDU_TX:
			if (tlv_data->len != sizeof(struct lpc_type_ampdu_tx)) {
				SLSI_WARN_NODEV("AMPDU_TX TLV length is mismatch:%x %x\n", tlv_data->len, sizeof(struct lpc_type_ampdu_tx));
				r += sizeof(struct tlv_hdr) + tlv_data->len;
				break;
			}
			r += sizeof(struct tlv_hdr);
			slsi_lpc_send_ampdu_tx((struct lpc_type_ampdu_tx *)(log_buffer + r));
			r += tlv_data->len;
			break;
		case LPC_TYPE_MPDU_RX:
			if (tlv_data->len != sizeof(struct lpc_type_mpdu_rx)) {
				SLSI_WARN_NODEV("MPDU_RX TLV length is mismatch:%x %x\n", tlv_data->len, sizeof(struct lpc_type_mpdu_rx));
				r += sizeof(struct tlv_hdr) + tlv_data->len;
				break;
			}
			r += sizeof(struct tlv_hdr);
			c_skb = slsi_lpc_send_mpdu_rx((struct lpc_type_mpdu_rx *)(log_buffer + r));
			r += tlv_data->len;
			if (c_skb) {
				tlv_data = (struct tlv_hdr *)(log_buffer + r);
				if (tlv_data->type != LPC_TYPE_PHY_DOLLOP) {
					if (c_skb)
						kfree_skb(c_skb);
					break;
				}
				r += sizeof(struct tlv_hdr);
				c_skb = slsi_lpc_make_rx_radiotap_hdr((struct lpc_type_phy_dollop *)(log_buffer + r), c_skb);
				r += tlv_data->len;
				cnt++;
				todo_cnt--;

				spin_lock_bh(&slsi_lpc_manager.lpc_queue_lock);
				__skb_queue_tail(&slsi_lpc_manager.queue, c_skb);
				spin_unlock_bh(&slsi_lpc_manager.lpc_queue_lock);
				schedule_work(&slsi_lpc_manager.lpc_work);
			}
			break;
		case LPC_TYPE_AMPDU_RX:
			if (tlv_data->len != sizeof(struct lpc_type_ampdu_rx)) {
				SLSI_WARN_NODEV("AMPDU_RX TLV length is mismatch:%x %x\n", tlv_data->len, sizeof(struct lpc_type_ampdu_rx));
				r += sizeof(struct tlv_hdr) + tlv_data->len;
				break;
			}
			__skb_queue_head_init(&amsdu_list);
			__skb_queue_head_init(&info_list);
			r += sizeof(struct tlv_hdr);
			amsdu_list_cnt = slsi_lpc_make_rx_amsdu_list((struct lpc_type_ampdu_rx *)(log_buffer + r), &amsdu_list, &info_list);
			r += tlv_data->len;
			if (amsdu_list_cnt) {
				tlv_data = (struct tlv_hdr *)(log_buffer + r);
				if (tlv_data->type != LPC_TYPE_PHY_DOLLOP) {
					__skb_queue_purge(&amsdu_list);
					__skb_queue_purge(&info_list);
					break;
				}
				r += sizeof(struct tlv_hdr);
				slsi_lpc_send_ampdu_rx((struct lpc_type_phy_dollop *)(log_buffer + r), &amsdu_list);
				slsi_lpc_save_ampdu_rx_info((struct lpc_type_phy_dollop *)(log_buffer + r), &info_list);
				r += tlv_data->len;
				cnt++;
				todo_cnt--;
			}
			break;
		case LPC_TYPE_PHY_DOLLOP:
			r += sizeof(struct tlv_hdr) + tlv_data->len;
			break;
		default:
			SLSI_INFO_NODEV("type:%04x len:%x\n", tlv_data->type, tlv_data->len);
			r += sizeof(struct tlv_hdr) + tlv_data->len;
			break;
		}

		cnt++;
		todo_cnt--;
	}

	tlv_con->r_index = r;
	/* CPU memory barrier */
	smp_wmb();
	return todo_cnt;
}

static int slsi_lpc_set_mib_local_packet_capture_mode(struct slsi_dev *sdev, bool value)
{
	struct slsi_mib_data mib_data = {0, NULL};
	int ret;

	ret = slsi_mib_encode_bool(&mib_data, SLSI_PSID_UNIFI_LOCAL_PACKET_CAPTURE_MODE, value, 0);
	if (ret != SLSI_MIB_STATUS_SUCCESS) {
		SLSI_ERR(sdev, "LPC Failed: no mem for MIB\n");
		return -ENOMEM;
	}
	ret = slsi_mlme_set(sdev, NULL, mib_data.data, mib_data.dataLength);
	if (ret)
		SLSI_ERR(sdev, "Enable/Disable LPC mode failed. %d\n", ret);

	kfree(mib_data.data);
	return ret;
}

int slsi_lpc_start(struct slsi_dev *sdev, int type)
{
	struct sk_buff *msg;
	void *msg_head;
	int ret = 7; /* Failed due to unknown errors */
	size_t size = 0;

	ret = slsi_lpc_set_mib_local_packet_capture_mode(sdev, true);
	if (ret)
		return 7; /* or 1 ? failed as file system not available : not enough memory*/

	if (slsi_lpc_manager.enable) {
		SLSI_INFO_NODEV("Local Packet Capture is already Running.\n");
		return 3; /*capture is alread running*/
	}

	size = NLMSG_ALIGN(nlmsg_msg_size(GENL_HDRLEN + slsi_lpc_family.hdrsize)) + nla_total_size(sizeof(u8));
	msg = genlmsg_new(size, GFP_ATOMIC);
	if (!msg)
		return 7;

	msg_head = genlmsg_put(msg, 0, 0, &slsi_lpc_family, 0, 0);
	if (!msg_head) {
		nlmsg_free(msg);
		return 7;
	}

	if (nla_put_u8(msg, SLSI_LPC_ATTR_PKT_LOG_TYPE, SLSI_LOCAL_PACKET_CAPTURE_START)) {
		genlmsg_cancel(msg, msg_head);
		nlmsg_free(msg);
		return 7;
	}
	genlmsg_end(msg, msg_head);
	ret = genlmsg_multicast(&slsi_lpc_family, msg, 0, 0, GFP_ATOMIC);
	if (ret < 0)
		return 1; /* failed as file system not available : not enough memory */

	slsi_lpc_manager.is_mgmt_type = (type & BIT(0)) ? true : false;
	slsi_lpc_manager.is_data_type = (type & BIT(1)) ? true : false;
	slsi_lpc_manager.is_ctrl_type = (type & BIT(2)) ? true : false;

	SLSI_INFO_NODEV("Local packet Capture is started type:%d\n", type);
	slsi_lpc_manager.enable = 1;
	ampdu_tx_agg_idx = 0;

	return 0;
}

int slsi_lpc_stop(struct slsi_dev *sdev)
{
	struct sk_buff *msg;
	void *msg_head;
	int ret = 7; /* Failed due to unknown errors */
	size_t size = 0;
	int i;

	ret = slsi_lpc_set_mib_local_packet_capture_mode(sdev, false);
	if (ret)
		return 7; /* or 1 ? failed as file system not available : not enough memory*/

	if (!slsi_lpc_manager.enable)
		return 1; /*capture is not alread running*/

	slsi_lpc_manager.enable = 0;

	spin_lock_bh(&slsi_lpc_manager.lpc_queue_lock);
	__skb_queue_purge(&slsi_lpc_manager.queue);
	spin_unlock_bh(&slsi_lpc_manager.lpc_queue_lock);
	flush_work(&slsi_lpc_manager.lpc_work);

	size = NLMSG_ALIGN(nlmsg_msg_size(GENL_HDRLEN + slsi_lpc_family.hdrsize)) + nla_total_size(sizeof(u8));
	msg = genlmsg_new(size, GFP_ATOMIC);
	if (!msg)
		return 7;

	msg_head = genlmsg_put(msg, 0, 0, &slsi_lpc_family, 0, 0);
	if (!msg_head) {
		nlmsg_free(msg);
		return 7;
	}

	if (nla_put_u8(msg, SLSI_LPC_ATTR_PKT_LOG_TYPE, SLSI_LOCAL_PACKET_CAPTURE_STOP)) {
		genlmsg_cancel(msg, msg_head);
		nlmsg_free(msg);
		return 7;
	}
	genlmsg_end(msg, msg_head);
	ret = genlmsg_multicast(&slsi_lpc_family, msg, 0, 0, GFP_ATOMIC);

	if (ret < 0)
		SLSI_WARN_NODEV("genlmsg_multicast is failed ret:%d\n", ret);

	spin_lock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
	for (i = 0; i < LPC_TABLE_SIZE; i++)
		slsi_lpc_clear_packet_data(&slsi_lpc_manager.lpc_rx_buffer_table[i]);

	spin_unlock_bh(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);

	spin_lock_bh(&slsi_lpc_manager.lpc_tx_skb_buffer_lock);
	for (i = 0; i < LPC_TABLE_SIZE; i++)
		slsi_lpc_clear_packet_data(&slsi_lpc_manager.lpc_tx_buffer_table[i]);

	spin_unlock_bh(&slsi_lpc_manager.lpc_tx_skb_buffer_lock);

	spin_lock_bh(&slsi_lpc_manager.lpc_ampdu_info_lock);
	for (i = 0; i < LPC_TABLE_SIZE; i++)
		slsi_lpc_clear_packet_data(&slsi_lpc_manager.lpc_rx_ampdu_info_table[i]);

	spin_unlock_bh(&slsi_lpc_manager.lpc_ampdu_info_lock);

	SLSI_INFO_NODEV("Local packet Capture is Stopped.\n");

	return 0;
}

int slsi_lpc_init(void)
{
	int ret = 0;
	int i;

	ret = genl_register_family(&slsi_lpc_family);

	INIT_WORK(&slsi_lpc_manager.lpc_work, slsi_lpc_work_func);
	skb_queue_head_init(&slsi_lpc_manager.queue);
	spin_lock_init(&slsi_lpc_manager.lpc_queue_lock);
	spin_lock_init(&slsi_lpc_manager.lpc_ampdu_info_lock);
	spin_lock_init(&slsi_lpc_manager.lpc_tx_skb_buffer_lock);
	spin_lock_init(&slsi_lpc_manager.lpc_rx_skb_buffer_lock);
	for (i = 0; i < LPC_TABLE_SIZE; i++) {
		INIT_LIST_HEAD(&slsi_lpc_manager.lpc_tx_buffer_table[i]);
		INIT_LIST_HEAD(&slsi_lpc_manager.lpc_rx_buffer_table[i]);
		INIT_LIST_HEAD(&slsi_lpc_manager.lpc_rx_ampdu_info_table[i]);
	}

	return ret;
}

int slsi_lpc_deinit(struct slsi_dev *sdev)
{
	int ret = 0;

	slsi_lpc_stop(sdev);
	ret = genl_unregister_family(&slsi_lpc_family);

	return ret;
}

int slsi_lpc_is_lpc_enabled(void)
{
	return slsi_lpc_manager.enable;
}
