/*

 *(C) Copyright 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * All Rights Reserved
 */

#include <linux/module.h>
#include <linux/netdevice.h>	/* dev_kfree_skb_any */
#include <linux/ip.h>
#include <linux/debugfs.h>
#include "common_datastub.h"
#include "data_channel_kernel.h"
#include "data_path.h"
#include "psd_data_channel.h"
#include "tel_trace.h"
#include "debugfs.h"

#define NETWORK_EMBMS_CID    0xFF

#define IPV4_ACK_LENGTH_LIMIT 96
#define IPV6_ACK_LENGTH_LIMIT 128

struct pduhdr {
	__be16 length;
	__u8 offset;
	__u8 reserved;
	int cid;
} __packed;

struct data_path *psd_dp;

static int embms_cid = 7;

static struct psd_user __rcu *psd_users[MAX_CID_NUM];

static u32 ack_opt = true;
static u32 data_drop = true;
static u32 ndev_fc;

#define MMS_CID    1

static inline enum data_path_priority packet_priority(int cid,
	struct sk_buff *skb)
{
	const struct iphdr *iph;
	enum data_path_priority prio = dp_priority_default;

	if (!ack_opt)
		return dp_priority_high;

	if (cid == MMS_CID)
		prio = dp_priority_high;
	else {
		bool is_ack = false;

		iph = ip_hdr(skb);
		if (iph) {
			if (iph->version == 4) {
				if (skb->len <= IPV4_ACK_LENGTH_LIMIT)
					is_ack = true;
			} else if (iph->version == 6) {
				if (skb->len <= IPV6_ACK_LENGTH_LIMIT)
					is_ack = true;
			}

			if (is_ack)
					prio = dp_priority_high;
		}
	}

	return prio;
}

int psd_register(const struct psd_user *user, int cid)
{
	DP_PRINT("%s: cid:%d,\n", __func__, cid);
	return cmpxchg(&psd_users[cid], NULL, user) == NULL ? 0 : -EEXIST;
}
EXPORT_SYMBOL(psd_register);


int psd_unregister(const struct psd_user *user, int cid)
{
	int ret;
	DP_PRINT("%s: cid:%d,\n", __func__, cid);
	ret =  cmpxchg(&psd_users[cid], user, NULL) == user ? 0 : -ENOENT;
	if (ret == 0)
		synchronize_net();
	return ret;
}
EXPORT_SYMBOL(psd_unregister);

void set_embms_cid(int cid)
{
	embms_cid = cid;
}
EXPORT_SYMBOL(set_embms_cid);


int sendPSDData(int cid, struct sk_buff *skb)
{
	struct pduhdr *hdr;
	struct sk_buff *skb2;
	unsigned len;
	unsigned tailpad;
	enum data_path_priority prio;

	DP_ENTER();

	if (!gCcinetDataEnabled) {
		dev_kfree_skb_any(skb);
		return PSD_DATA_SEND_DROP;
	}

	/* data path is not open, drop the packet and return */
	if (!psd_dp) {
		DP_PRINT("%s: data path is not open!\n", __func__);
		dev_kfree_skb_any(skb);
		return PSD_DATA_SEND_DROP;
	}
	prio = packet_priority(cid & (~(1 << 31)), skb);

	/*
	 * tx_q is full or link is down
	 * allow ack when queue is full
	 */
	if (unlikely((prio != dp_priority_high &&
		data_path_is_tx_q_full(psd_dp)) ||
		 !data_path_is_link_up())) {
		DP_PRINT("%s: tx_q is full or link is down\n", __func__);
		/* tx q is full, should schedule tx immediately */
		if (data_path_is_link_up())
			data_path_schedule_tx(psd_dp);

		if (data_drop) {
			DP_PRINT("%s: drop the packet\n", __func__);
			dev_kfree_skb_any(skb);
			return PSD_DATA_SEND_DROP;
		} else {
			DP_PRINT("%s: return net busy to upper layer\n",
				__func__);
			return PSD_DATA_SEND_BUSY;
		}
	}

	len = skb->len;
	tailpad = padding_size(sizeof(*hdr) + len);

	if (likely(!skb_cloned(skb))) {
		int headroom = skb_headroom(skb);
		int tailroom = skb_tailroom(skb);

		/* enough room as-is? */
		if (likely(sizeof(*hdr) + tailpad <= headroom + tailroom)) {
			/* do not need to be readjusted */
			if (sizeof(*hdr) <= headroom && tailpad <= tailroom)
				goto fill;

			skb->data = memmove(skb->head + sizeof(*hdr),
					    skb->data, len);
			skb_set_tail_pointer(skb, len);
			goto fill;
		}
	}

	/* create a new skb, with the correct size (and tailpad) */
	skb2 = skb_copy_expand(skb, sizeof(*hdr), tailpad + 1, GFP_ATOMIC);
	if (skb2)
		trace_psd_xmit_skb_realloc(skb, skb2);
	if (unlikely(!skb2))
		return PSD_DATA_SEND_BUSY;
	dev_kfree_skb_any(skb);
	skb = skb2;

	/* fill out the pdu header */
fill:
	hdr = (void *)__skb_push(skb, sizeof(*hdr));
	memset(hdr, 0, sizeof(*hdr));
	hdr->length = cpu_to_be16(len);
	hdr->cid = cid;
	memset(skb_put(skb, tailpad), 0, tailpad);

	data_path_xmit(psd_dp, skb, prio);
	return PSD_DATA_SEND_OK;
}
EXPORT_SYMBOL(sendPSDData);

enum data_path_result psd_data_rx(unsigned char *data, unsigned int length)
{
	unsigned char *p = data;
	unsigned int remains = length;
	DP_ENTER();

	while (remains > 0) {
		struct psd_user *user;
		struct pduhdr	*hdr = (void *)p;
		u32				iplen, offset_len;
		u32				tailpad;
		u32				total_len;
		int				sim_id;

		total_len = be16_to_cpu(hdr->length);
		offset_len = hdr->offset;

		if (unlikely(total_len < offset_len)) {
			DP_ERROR("%s: packet error\n", __func__);
			return dp_rx_packet_error;
		}

		iplen = total_len - offset_len;
		tailpad = padding_size(sizeof(*hdr) + iplen + offset_len);
		sim_id = (hdr->cid >> 31) & 1;
		hdr->cid &= ~(1 << 31);

		DP_PRINT("%s: SIM%d remains %u, iplen %u\n" __func__,
			sim_id+1, remains, iplen);
		DP_PRINT("%s: offset %u, cid %d, tailpad %u\n", __func__,
			offset_len, hdr->cid, tailpad);

		if (unlikely(remains < (iplen + offset_len
					+ sizeof(*hdr) + tailpad))) {
			DP_ERROR("%s: packet length error\n", __func__);
			return dp_rx_packet_error;
		}

		/* offset domain data */
		p += sizeof(*hdr);
		remains -= sizeof(*hdr);

		/* ip payload */
		p += offset_len;
		remains -= offset_len;

		if (hdr->cid == NETWORK_EMBMS_CID)
			hdr->cid = embms_cid;
		if (likely(hdr->cid >= 0 && hdr->cid < MAX_CID_NUM)) {
			rcu_read_lock();
			user = rcu_dereference(psd_users[hdr->cid]);
			if (user && user->on_receive)
				user->on_receive(user->priv, p, iplen);
			rcu_read_unlock();
			if (!user)
				pr_err_ratelimited(
					"%s: no psd user for cid:%d\n",
					__func__, hdr->cid);
		} else
			pr_err_ratelimited(
				"%s: invalid cid:%d, simid:%d\n",
				__func__, hdr->cid, sim_id);

		p += iplen + tailpad;
		remains -= iplen + tailpad;
	}
	return dp_success;
}

static void psd_tx_traffic_control(bool is_throttle)
{
	int i;

	if (!ndev_fc)
		return;

	for (i = 0; i < MAX_CID_NUM; ++i) {
		struct psd_user *user;
		rcu_read_lock();
		user = rcu_dereference(psd_users[i]);
		if (user && user->on_throttle)
			user->on_throttle(user->priv, is_throttle);
		rcu_read_unlock();
	}
}


void psd_tx_stop(void)
{
	DP_ENTER();
	psd_tx_traffic_control(true);
	DP_LEAVE();
}

void psd_tx_resume(void)
{
	DP_ENTER();
	psd_tx_traffic_control(false);
	DP_LEAVE();
}

void psd_rx_stop(void)
{
	DP_ENTER();

	DP_LEAVE();
	return;
}

void psd_link_down(void)
{
	DP_ENTER();

	DP_LEAVE();
	return;
}

void psd_link_up(void)
{
	DP_ENTER();
	DP_LEAVE();
	return;
}

struct data_path_callback psd_cbs = {
	.data_rx = psd_data_rx,
	.tx_stop = psd_tx_stop,
	.tx_resume = psd_tx_resume,
	.rx_stop = psd_rx_stop,
	.link_down = psd_link_down,
	.link_up = psd_link_up,
};

static struct dentry *tel_debugfs_root_dir;
static struct dentry *psd_debugfs_root_dir;

static int psd_debugfs_init(void)
{
	char buf[256];
	char path[256];

	tel_debugfs_root_dir = tel_debugfs_get();
	if (!tel_debugfs_root_dir)
		return -ENOMEM;

	psd_debugfs_root_dir = debugfs_create_dir("psd", tel_debugfs_root_dir);
	if (IS_ERR_OR_NULL(psd_debugfs_root_dir))
		goto putrootfs;

	snprintf(path, sizeof(path), "../../%s",
		dentry_path_raw(psd_dp->dentry, buf, sizeof(buf)));

	if (IS_ERR_OR_NULL(debugfs_create_symlink("dp", psd_debugfs_root_dir,
			path)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_bool("ndev_fc", S_IRUGO | S_IWUSR,
			psd_debugfs_root_dir, &ndev_fc)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_bool("data_drop", S_IRUGO | S_IWUSR,
			psd_debugfs_root_dir, &data_drop)))
		goto error;

	if (IS_ERR_OR_NULL(debugfs_create_bool("ack_opt", S_IRUGO | S_IWUSR,
			psd_debugfs_root_dir, &ack_opt)))
		goto error;

	return 0;

error:
	debugfs_remove_recursive(psd_debugfs_root_dir);
	psd_debugfs_root_dir = NULL;
putrootfs:
	tel_debugfs_put(tel_debugfs_root_dir);
	tel_debugfs_root_dir = NULL;
	return -1;
}

static int psd_debugfs_exit(void)
{
	debugfs_remove_recursive(psd_debugfs_root_dir);
	psd_debugfs_root_dir = NULL;
	tel_debugfs_put(tel_debugfs_root_dir);
	tel_debugfs_root_dir = NULL;
	return 0;
}

int InitPsdChannel(void)
{
	psd_dp = data_path_open(dp_type_psd, &psd_cbs);
	if (!psd_dp)
		return -1;

	if (psd_debugfs_init() < 0)
		data_path_close(psd_dp);

	return 0;
}

void DeInitPsdChannel(void)
{
	psd_debugfs_exit();
	data_path_close(psd_dp);
}

