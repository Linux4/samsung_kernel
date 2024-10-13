/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Keguang Zhang <keguang.zhang@spreadtrum.com>
 * Jingxiang Li <Jingxiang.li@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/skbuff.h>

#include "sprdwl.h"
#include "wapi.h"
#include "txrx.h"
#include "cfg80211.h"
#include "cmdevt.h"
#include "intf_ops.h"

static int sprdwl_rebuild_wapi_skb(struct sprdwl_vif *vif, struct sk_buff **skb)
{
	int addr_len;
	struct sk_buff *wskb;
	struct sk_buff *sskb;

	sskb = *skb;
	wskb = dev_alloc_skb(sskb->len + vif->priv->skb_head_len +
			     SPRDWL_WAPI_ATTACH_LEN + ETHERNET_HDR_LEN);
	if (!wskb)
		return -ENOMEM;
	skb_reserve(wskb, sizeof(struct sprdwl_data_hdr) + 2);
	memcpy(wskb->data, sskb->data, ETHERNET_HDR_LEN);
	addr_len = sprdwl_wapi_enc(vif, sskb->data,
				   (sskb->len - ETHERNET_HDR_LEN),
				   ((unsigned char *)(wskb->data) +
				    ETHERNET_HDR_LEN));
	addr_len += ETHERNET_HDR_LEN;
	skb_put(wskb, addr_len);
	dev_kfree_skb(sskb);
	*skb = wskb;

	return 0;
}

/* if err, the caller judge the skb if need free,
 * here just free the msg buf to the freelist
 * if wapi free skb here
 */
int sprdwl_send_data(struct sprdwl_vif *vif,
		     struct sprdwl_msg_buf *msg,
		     struct sk_buff *skb, u8 type, u8 offset)
{
	int ret;
	struct sprdwl_data_hdr *hdr;

	if (type == SPRDWL_DATA_TYPE_WAPI)
		if (sprdwl_rebuild_wapi_skb(vif, &skb)) {
			sprdwl_intf_free_msg_buf(vif->priv, msg);
			return -ENOMEM;
		}

	skb_push(skb, sizeof(*hdr) + offset);
	hdr = (struct sprdwl_data_hdr *)skb->data;
	memset(hdr, 0, sizeof(*hdr));
	hdr->common.type = SPRDWL_TYPE_DATA;
	hdr->common.mode = vif->mode;
	hdr->info1 = type | (offset & SPRDWL_DATA_OFFSET_MASK);
	hdr->plen = cpu_to_le16(skb->len);

	sprdwl_fill_msg(msg, skb, skb->data, skb->len);

	ret = sprdwl_intf_tx(vif->priv, msg);
	if (ret) {
		if (type == SPRDWL_DATA_TYPE_WAPI)
			dev_kfree_skb(skb);
		pr_err("%s TX data Err: %d\n", __func__, ret);
	}

	return ret;
}

int sprdwl_send_cmd(struct sprdwl_priv *priv, struct sprdwl_msg_buf *msg)
{
	int ret;
	struct sk_buff *skb;

	skb = msg->skb;
	ret = sprdwl_intf_tx(priv, msg);
	if (ret) {
		pr_err("%s TX cmd Err: %d\n", __func__, ret);
		/* now cmd msg droped */
		dev_kfree_skb(skb);
	}

	return ret;
}

static int sprdwl_rx_normal_data_process(struct sprdwl_vif *vif,
					 unsigned char *pdata,
					 unsigned short len)
{
	struct sk_buff *skb;
	struct net_device *ndev;

	skb = dev_alloc_skb(len + NET_IP_ALIGN);
	if (!skb)
		return -ENOMEM;
	ndev = vif->ndev;
	skb_reserve(skb, NET_IP_ALIGN);
	memcpy(skb->data, pdata, len);
	skb_put(skb, len);
	sprdwl_netif_rx(skb, ndev);

	return 0;
}

static int sprdwl_rx_wapi_data_process(struct sprdwl_vif *vif,
				       unsigned char *pdata, unsigned short len)
{
	int decryp_data_len = 0;
	struct ieee80211_hdr_3addr *addr;
	struct sk_buff *skb;
	struct net_device *ndev;
	u8 snap_header[6] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

	ndev = vif->ndev;
	addr = (struct ieee80211_hdr_3addr *)pdata;
	skb = dev_alloc_skb(len + NET_IP_ALIGN);
	if (!skb)
		return -ENOMEM;
	skb_reserve(skb, NET_IP_ALIGN);

	decryp_data_len = sprdwl_wapi_dec(vif, (unsigned char *)addr,
					  24, (len - 24), (skb->data + 12));
	if (!decryp_data_len) {
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	if (!memcmp((skb->data + 12), snap_header, sizeof(snap_header))) {
		skb_reserve(skb, 6);
		memcpy(skb->data, addr->addr1, ETH_ALEN);
		memcpy(skb->data + ETH_ALEN, addr->addr2, ETH_ALEN);
		skb_put(skb, (decryp_data_len + 6));
	} else {
		/* copy eth header */
		memcpy(skb->data, addr->addr3, ETH_ALEN);
		memcpy(skb->data + ETH_ALEN, addr->addr2, ETH_ALEN);
		skb_put(skb, (decryp_data_len + 12));
	}

	sprdwl_netif_rx(skb, ndev);

	return 0;
}

unsigned short sprdwl_rx_data_process(struct sprdwl_priv *priv,
				      unsigned char *msg)
{
	unsigned char mode, data_type;
	unsigned short len, plen;
	unsigned char *data;
	struct sprdwl_data_hdr *hdr;
	struct sprdwl_vif *vif;

	hdr = (struct sprdwl_data_hdr *)msg;
	mode = hdr->common.mode;
	data_type = SPRDWL_GET_DATA_TYPE(hdr->info1);
	if (mode == SPRDWL_MODE_NONE || mode > SPRDWL_MODE_MAX ||
	    data_type > SPRDWL_DATA_TYPE_MAX) {
		pr_err("%s [mode %d]RX err[type %d]\n", __func__, mode,
		       data_type);
		return 0;
	}

	data = (unsigned char *)msg;
	data += sizeof(*hdr) + (hdr->info1 & SPRDWL_DATA_OFFSET_MASK);
	plen = SPRDWL_GET_LE16(hdr->plen);
	if (!priv) {
		pr_err("%s sdio->priv not init.\n", __func__);
		return plen;
	}

	vif = mode_to_vif(priv, mode);
	if (!vif) {
		pr_err("%s cant't get vif %d\n", __func__, mode);
		return plen;
	}

	len = plen - sizeof(*hdr) - (hdr->info1 & SPRDWL_DATA_OFFSET_MASK);
	switch (data_type) {
	case SPRDWL_DATA_TYPE_NORMAL:
		sprdwl_rx_normal_data_process(vif, data, len);
		break;
	case SPRDWL_DATA_TYPE_WAPI:
		pr_debug("SPRDWL_DATA_TYPE_WAPI\n");
		sprdwl_rx_wapi_data_process(vif, data, len);
		break;
	default:
		break;
	}
	sprdwl_put_vif(vif);

	return plen;
}
