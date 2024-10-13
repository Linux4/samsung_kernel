/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * Authors	:
 * Baolei Yuan <baolei.yuan@spreadtrum.com>
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

#include "sprdwl.h"
#include "cmdevt.h"
#include "vendor.h"

static const u8 *wpa_scan_get_ie(u8 *res, u8 ie_len, u8 ie)
{
	const u8 *end, *pos;

	pos = res;
	end = pos + ie_len;
	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == ie)
			return pos;
		pos += 2 + pos[1];
	}
	return NULL;
}

static int sprdwl_vendor_set_config(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int len)
{
	const struct nlattr *attr_num_buckets;
	const struct nlattr *attr_base_period;
	struct nlattr *attr_buckets_spec;
	struct nlattr *mbuckets[MAX_BUCKETS + 1];
	struct nlattr *bucket[GSCAN_ATTRIBUTE_MAX + 1];
	struct sprdwl_cmd_gscan_set_config *scan_params;
	const struct nlattr *cur = data;
	void *tmp;
	int i, j, ret, result, attrlen, tlen;
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	struct nlattr *mchannels[MAX_CHANNELS + 1];
	struct sprdwl_cmd_gscan_rsp_header rsp;
	u16 rlen = sizeof(struct sprdwl_cmd_gscan_rsp_header);

	attr_base_period = cur;
	cur = nla_next(cur, &len);
	attr_num_buckets = cur;
	if (!attr_num_buckets || !attr_base_period)
		return -EINVAL;

	scan_params = kmalloc(sizeof(*scan_params), GFP_KERNEL);
	if (!scan_params)
		return -ENOMEM;
	scan_params->num_buckets = nla_get_u32(attr_num_buckets);
	scan_params->base_period = nla_get_u32(attr_base_period);
	netdev_info(vif->ndev, "%s :bucket num:%d period:%d\n",
		    __func__, scan_params->num_buckets,
		    scan_params->base_period);

	attr_buckets_spec = nla_next(cur, &len);
	result = nla_parse(mbuckets, MAX_BUCKETS, attr_buckets_spec, len, NULL);
	kfree(vif->priv->gscan_res);
	vif->priv->gscan_buckets_num = scan_params->num_buckets;
	tlen = sizeof(struct sprdwl_gscan_cached_results);
	vif->priv->gscan_res = kmalloc(vif->priv->gscan_buckets_num *
				       tlen, GFP_KERNEL);
	if (!vif->priv->gscan_res)
		return -ENOMEM;
	memset(vif->priv->gscan_res, 0x0,
	       vif->priv->gscan_buckets_num *
	       sizeof(struct sprdwl_gscan_cached_results));

	for (i = 1; i <= scan_params->num_buckets; i++) {
		if (!mbuckets[i]) {
			ret = -EINVAL;
			goto out;
		}
		result = nla_parse(bucket, GSCAN_ATTRIBUTE_MAX,
				   nla_data(mbuckets[i]),
				   nla_len(mbuckets[i]), NULL);
		scan_params->buckets[i - 1].bucket =
		    nla_get_u32(bucket[GSCAN_ATTRIBUTE_BUCKET_ID]);
		scan_params->buckets[i - 1].period =
		    nla_get_u32(bucket[GSCAN_ATTRIBUTE_BUCKET_PERIOD]);
		scan_params->buckets[i - 1].band =
		    nla_get_u32(bucket[GSCAN_ATTRIBUTE_BUCKETS_BAND]);
		scan_params->buckets[i - 1].step_count =
		    nla_get_u32(bucket[GSCAN_ATTRIBUTE_BUCKET_STEP_COUNT]);
		scan_params->buckets[i - 1].max_period =
		    nla_get_u32(bucket[GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD]);
		scan_params->buckets[i - 1].num_channels =
		    nla_get_u32(bucket[GSCAN_ATTRIBUTE_BUCKET_NUM_CHANNELS]);
		scan_params->buckets[i - 1].report_events =
		    nla_get_u32(bucket[GSCAN_ATTRIBUTE_REPORT_EVENTS]);
		tmp = nla_data(bucket[GSCAN_ATTRIBUTE_BUCKET_CHANNELS]);
		attrlen = nla_len(bucket[GSCAN_ATTRIBUTE_BUCKET_CHANNELS]);
		result = nla_parse(mchannels, MAX_CHANNELS, tmp, attrlen, NULL);
		for (j = 1; j <= scan_params->buckets[i-1].num_channels; j++) {
			if (!mchannels[j]) {
				ret = -EINVAL;
				goto out;
			}
			scan_params->buckets[i - 1].channels[j - 1].channel =
			    nla_get_u32(mchannels[j]);
		}
		(vif->priv->gscan_res + i - 1)->scan_id =
		    scan_params->buckets[i - 1].bucket;
	}
	tlen = sizeof(struct sprdwl_cmd_gscan_set_config);
	ret = sprdwl_set_gscan_config(vif->priv, vif->mode, (void *)scan_params,
				      tlen, (u8 *)(&rsp), &rlen);
out:
	if (ret < 0)
		kfree(vif->priv->gscan_res);
	kfree(scan_params);
	return ret;
}

static int sprdwl_vendor_set_scan_config(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int len)
{
	struct nlattr *mattributes[GSCAN_ATTRIBUTE_MAX + 1];
	struct sprdwl_cmd_gscan_set_scan_config scan_params;
	int result;
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	struct sprdwl_cmd_gscan_rsp_header rsp;
	int slen = sizeof(struct sprdwl_cmd_gscan_set_scan_config);
	u16 rlen = sizeof(struct sprdwl_cmd_gscan_rsp_header);

	result = nla_parse(mattributes, GSCAN_ATTRIBUTE_MAX, data, len, NULL);
	if (result < 0 ||
	    !mattributes[GSCAN_ATTRIBUTE_REPORT_THRESHOLD] ||
	    !mattributes[GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN] ||
	    !mattributes[GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE]) {
		netdev_err(vif->ndev, "%s gscan null attributes\n", __func__);
		return -EINVAL;
	}
	scan_params.max_ap_per_scan =
	    nla_get_u32(mattributes[GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN]);
	scan_params.report_threshold_percent =
	    nla_get_u32(mattributes[GSCAN_ATTRIBUTE_REPORT_THRESHOLD]);
	scan_params.report_threshold_num_scans =
	    nla_get_u32(mattributes[GSCAN_ATTRIBUTE_NUM_SCANS_TO_CACHE]);
	netdev_info(vif->ndev, "%s gscan attributes :%d %d %d\n", __func__,
		    scan_params.max_ap_per_scan,
		    scan_params.report_threshold_percent,
		    scan_params.report_threshold_num_scans);
	return sprdwl_set_gscan_scan_config(vif->priv, vif->mode,
					    (void *)(&scan_params),
					    slen, (u8 *)(&rsp), &rlen);
}

static int sprdwl_vendor_enable_gscan(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int len)
{
	struct nlattr *mattributes[GSCAN_ATTRIBUTE_MAX + 1];
	int enable;
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	int result = nla_parse(mattributes, GSCAN_ATTRIBUTE_MAX, data,
			       len, NULL);
	struct sprdwl_cmd_gscan_rsp_header rsp;
	u16 rlen = sizeof(struct sprdwl_cmd_gscan_rsp_header);

	if (result < 0 || !mattributes[GSCAN_ATTRIBUTE_ENABLE_FEATURE])
		return -EINVAL;
	enable = nla_get_u32(mattributes[GSCAN_ATTRIBUTE_ENABLE_FEATURE]);
	netdev_info(vif->ndev, "%s %s\n", __func__,
		    (enable ? "start" : "stop"));
	return sprdwl_enable_gscan(vif->priv, vif->mode,
				   (void *)(&enable), (u8 *)(&rsp), &rlen);
}

static int sprdwl_vendor_enable_full_gscan(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data, int len)
{
	struct nlattr *mattributes[GSCAN_ATTRIBUTE_MAX + 1];

	return nla_parse(mattributes, GSCAN_ATTRIBUTE_MAX, data, len, NULL);
}

static int sprdwl_vendor_set_country(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int len)
{
	struct nlattr *country_attr = (struct nlattr *)data;
	char *country = (char *)nla_data(country_attr);

	if (!country || strlen(country) != SPRDWL_COUNTRY_CODE_LEN) {
		wiphy_err(wiphy, "%s invalid country code!\n", __func__);
		return -EINVAL;
	}
	wiphy_info(wiphy, "%s %c%c\n", __func__,
		   toupper(country[0]), toupper(country[1]));
	return regulatory_hint(wiphy, country);
}

/*link layer stats*/
static int sprdwl_llstat(struct sprdwl_priv *priv, u8 vif_mode, u8 subtype,
			 const void *buf, u8 len, u8 *r_buf, u16 *r_len)
{
	u8 *sub_cmd, *buf_pos;
	u8 msglen = sizeof(*buf) + 1;
	struct sprdwl_msg_buf *msg;

	msg = sprdwl_cmd_getbuf(priv, msglen, vif_mode,
				SPRDWL_HEAD_RSP, WIFI_CMD_LLSTAT);
	if (!msg)
		return -ENOMEM;
	sub_cmd = (u8 *)msg->data;
	*sub_cmd = subtype;
	buf_pos = sub_cmd + 1;
	memcpy(buf_pos, buf, len);

	if (subtype == SPRDWL_SUBCMD_SET)
		return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, 0, 0);
	else
		return sprdwl_cmd_send_recv(priv, msg, CMD_WAIT_TIMEOUT, r_buf,
					    r_len);
}

static int sprdwl_vendor_set_llstat_handler(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int len)
{
	int ret = 0;
	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);
	struct wifi_link_layer_params *ll_params;

	ll_params = (struct wifi_link_layer_params *)data;
	if (ll_params->aggressive_statistics_gathering)
		ret = sprdwl_llstat(priv, vif->mode, SPRDWL_SUBCMD_SET, data,
				    len, 0, 0);
	return ret;
}

static int sprdwl_vendor_get_llstat_handler(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int len)
{
	struct sk_buff *reply;
	struct sprdwl_llstat_data *llst;
	struct sprdwl_vendor_data *vendor_data;
	u16 r_len = sizeof(*llst);
	u8 r_buf[r_len], ret, i;
	u32 reply_length;

	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);

	memset(r_buf, 0, r_len);
	vendor_data = kzalloc(sizeof(*vendor_data), GFP_KERNEL);
	if (!vendor_data)
		return -ENOMEM;
	ret = sprdwl_llstat(priv, vif->mode, SPRDWL_SUBCMD_GET, NULL, 0, r_buf,
			    &r_len);
	llst = (struct sprdwl_llstat_data *)r_buf;
	vendor_data->iface_st.info.mode = vif->mode;
	memcpy(vendor_data->iface_st.info.mac_addr, vif->ndev->dev_addr,
	       ETH_ALEN);
	vendor_data->iface_st.info.state = vif->sm_state;
	memcpy(vendor_data->iface_st.info.ssid, vif->ssid,
	       IEEE80211_MAX_SSID_LEN);
	memcpy(vendor_data->iface_st.info.bssid, vif->bssid, ETH_ALEN);
	vendor_data->iface_st.beacon_rx = llst->beacon_rx;
	vendor_data->iface_st.rssi_mgmt = llst->rssi_mgmt;
	for (i = 0; i < WIFI_AC_MAX; i++) {
		vendor_data->iface_st.ac[i].tx_mpdu = llst->ac[i].tx_mpdu;
		vendor_data->iface_st.ac[i].rx_mpdu = llst->ac[i].rx_mpdu;
		vendor_data->iface_st.ac[i].mpdu_lost = llst->ac[i].mpdu_lost;
		vendor_data->iface_st.ac[i].retries = llst->ac[i].retries;
	}
	vendor_data->radio_st.on_time = llst->on_time;
	vendor_data->radio_st.tx_time = llst->tx_time;
	vendor_data->radio_st.rx_time = llst->rx_time;
	vendor_data->radio_st.on_time_scan = llst->on_time_scan;

	reply_length = sizeof(struct sprdwl_vendor_data) +
	    sizeof(struct nl80211_vendor_cmd_info);
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, reply_length);
	if (!reply)
		return -ENOMEM;
	nla_put_u32(reply, NL80211_ATTR_VENDOR_ID, OUI_SPREAD);
	nla_put_u32(reply, NL80211_ATTR_VENDOR_SUBCMD,
		    SPRDWL_VENDOR_GET_LLSTAT);
	nla_put(reply, SPRDWL_VENDOR_ATTR_GET_LLSTAT, sizeof(*vendor_data),
		vendor_data);
	ret = cfg80211_vendor_cmd_reply(reply);

	kfree(vendor_data);
	return ret;
}

static int sprdwl_vendor_clr_llstat_handler(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data, int len)
{
	struct sk_buff *reply;
	struct wifi_clr_llstat_rsp clr_rsp;
	u32 *stats_clear_rsp_mask, *stats_clear_req_mask;
	u16 r_len = sizeof(*stats_clear_rsp_mask);
	u8 r_buf[r_len];
	u32 reply_length, ret;

	struct sprdwl_priv *priv = wiphy_priv(wiphy);
	struct sprdwl_vif *vif = container_of(wdev, struct sprdwl_vif, wdev);

	memset(r_buf, 0, r_len);
	if (!data) {
		wiphy_info(wiphy, "no clear data get from HAL");
		return -EINVAL;
	}
	stats_clear_req_mask = (u32 *)data;
	wiphy_info(wiphy, "stats_clear_req_mask = %u\n", *stats_clear_req_mask);
	ret = sprdwl_llstat(priv, vif->mode, SPRDWL_SUBCMD_DEL,
			    stats_clear_req_mask, r_len, r_buf, &r_len);
	stats_clear_rsp_mask = (u32 *)r_buf;
	clr_rsp.stats_clear_rsp_mask = *stats_clear_req_mask;
	clr_rsp.stop_rsp = 1;

	reply_length = sizeof(clr_rsp) + sizeof(struct nl80211_vendor_cmd_info);
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, reply_length);
	if (!reply)
		return -ENOMEM;
	nla_put_u32(reply, NL80211_ATTR_VENDOR_ID, OUI_SPREAD);
	nla_put_u32(reply, NL80211_ATTR_VENDOR_SUBCMD,
		    SPRDWL_VENDOR_CLR_LLSTAT);
	nla_put(reply, SPRDWL_VENDOR_ATTR_CLR_LLSTAT, sizeof(clr_rsp),
		&clr_rsp);
	ret = cfg80211_vendor_cmd_reply(reply);

	return ret;
}

static int sprdwl_vendor_get_gscan_capabilities(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int len)
{
	u16 rlen;
	struct sk_buff *reply;
	int ret = 0, payload;
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	struct sprdwl_cmd_gscan_rsp_header *hdr;
	struct sprdwl_gscan_capabilities *p = NULL;
	void *rbuf;
	unsigned char *tmp;

	rlen = sizeof(struct sprdwl_gscan_capabilities) +
	    sizeof(struct sprdwl_cmd_gscan_rsp_header);
	rbuf = kmalloc(rlen, GFP_KERNEL);
	if (!rbuf)
		return -ENOMEM;

	ret = sprdwl_get_gscan_capabilities(vif->priv,
					    vif->mode, (u8 *)rbuf, &rlen);
	if (ret < 0) {
		netdev_err(vif->ndev, "%s failed to get capabilities!\n",
			   __func__);
		goto out;
	}
	hdr = (struct sprdwl_cmd_gscan_rsp_header *)rbuf;
	p = (struct sprdwl_gscan_capabilities *)
	    (rbuf + sizeof(struct sprdwl_cmd_gscan_rsp_header));
	netdev_info(vif->ndev, "%s capbility buffer! %d %d %d\n",
		    __func__, p->max_scan_cache_size,
		    p->max_scan_buckets, p->max_number_of_white_listed_ssid);
	payload = rlen + 0x100;
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}
	tmp = skb_tail_pointer(reply);
	memcpy(tmp, (void *)(rbuf + sizeof(struct sprdwl_cmd_gscan_rsp_header)),
	       sizeof(struct sprdwl_gscan_capabilities));
	skb_put(reply, sizeof(struct sprdwl_gscan_capabilities));
	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret)
		netdev_err(vif->ndev, "%s failed to reply skb!\n", __func__);
out:
	kfree(rbuf);
	return ret;
}

static int sprdwl_vendor_get_channel_list(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int len)
{
	struct nlattr *mattributes[GSCAN_ATTRIBUTE_MAX + 1];
	int ret = 0, band, payload;
	u16 rlen;
	int result = nla_parse(mattributes, GSCAN_ATTRIBUTE_MAX, data,
			       len, NULL);
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	struct sprdwl_cmd_gscan_channel_list channel_list;
	struct sk_buff *reply;
	struct sprdwl_cmd_gscan_rsp_header *hdr;
	struct sprdwl_cmd_gscan_channel_list *p = NULL;
	void *rbuf;

	if (result < 0 || !mattributes[GSCAN_ATTRIBUTE_BAND])
		return -EINVAL;
	band = nla_get_u32(mattributes[GSCAN_ATTRIBUTE_BAND]);
	rlen = sizeof(struct sprdwl_cmd_gscan_channel_list) +
	    sizeof(struct sprdwl_cmd_gscan_rsp_header);
	rbuf = kmalloc(rlen, GFP_KERNEL);
	if (!rbuf)
		return -ENOMEM;

	memset(rbuf, 0x0, rlen);
	ret = sprdwl_get_gscan_channel_list(vif->priv, vif->mode,
					    (void *)&band, (u8 *)rbuf, &rlen);
	if (ret < 0) {
		netdev_err(vif->ndev, "%s failed to get channel!\n", __func__);
		goto out;
	}
	hdr = (struct sprdwl_cmd_gscan_rsp_header *)rbuf;
	p = (struct sprdwl_cmd_gscan_channel_list *)
	    (rbuf + sizeof(struct sprdwl_cmd_gscan_rsp_header));

	payload = rlen + 0x100;
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}

	nla_put_u32(reply, GSCAN_ATTRIBUTE_NUM_CHANNELS, p->num_channels);
	nla_put(reply, GSCAN_ATTRIBUTE_CHANNEL_LIST,
		sizeof(channel_list.channels), (void *)(p->channels));
	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret)
		netdev_err(vif->ndev, "%s failed to reply skb!\n", __func__);
out:
	kfree(rbuf);
	return ret;
}

static int sprdwl_vendor_get_gscan_results(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data, int len)
{
	struct nlattr *mattributes[GSCAN_ATTRIBUTE_MAX + 1];
	int ret = 0, i, rlen, flush, payload, num;
	int result = nla_parse(mattributes, GSCAN_ATTRIBUTE_MAX, data,
			       len, NULL);
	struct sprdwl_vif *vif = netdev_priv(wdev->netdev);
	struct sk_buff *reply;
	struct nlattr *scan_res;

	if (result < 0 ||
	    !mattributes[GSCAN_ATTRIBUTE_NUM_OF_RESULTS] ||
	    !mattributes[GSCAN_ATTRIBUTE_FLUSH_RESULTS])
		return -EINVAL;
	num = nla_get_u32(mattributes[GSCAN_ATTRIBUTE_NUM_OF_RESULTS]);
	flush = nla_get_u32(mattributes[GSCAN_ATTRIBUTE_FLUSH_RESULTS]);
	rlen = vif->priv->gscan_buckets_num *
	    sizeof(struct sprdwl_gscan_cached_results);
	payload = rlen + 0x100;
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, payload);
	if (!reply)
		return -ENOMEM;
	for (i = 0; i < vif->priv->gscan_buckets_num; i++) {
		if (!(vif->priv->gscan_res + i)->num_results)
			continue;
		scan_res = nla_nest_start(reply, GSCAN_ATTRIBUTE_SCAN_RESULTS);
		nla_put_u32(reply, GSCAN_ATTRIBUTE_SCAN_ID,
			    (vif->priv->gscan_res + i)->scan_id);
		nla_put_u8(reply, GSCAN_ATTRIBUTE_SCAN_FLAGS,
			   (vif->priv->gscan_res + i)->flags);
		nla_put_u32(reply, GSCAN_ATTRIBUTE_NUM_OF_RESULTS,
			    (vif->priv->gscan_res + i)->num_results);
		nla_put(reply, GSCAN_ATTRIBUTE_SCAN_RESULTS,
			sizeof(struct sprdwl_gscan_result) *
			((vif->priv->gscan_res + i)->num_results),
			(void *)((vif->priv->gscan_res + i)->results));
		nla_nest_end(reply, scan_res);
	}
	nla_put_u8(reply, GSCAN_ATTRIBUTE_SCAN_RESULTS_COMPLETE, 1);
	ret = cfg80211_vendor_cmd_reply(reply);
	if (ret < 0)
		netdev_err(vif->ndev, "%s failed to reply skb!\n", __func__);
	return ret;
}

static int sprdwl_vendor_cached_scan_result(struct sprdwl_vif *vif,
					    u8 bucket_id,
					    struct sprdwl_gscan_result *item)
{
	struct sprdwl_priv *priv = vif->priv;
	u32 i;
	struct sprdwl_gscan_cached_results *p = NULL;

	if (bucket_id >= priv->gscan_buckets_num || !priv->gscan_res) {
		netdev_err(vif->ndev, "%s the gscan buffer invalid!\n",
			   __func__);
		return -EINVAL;
	}
	for (i = 0; i < priv->gscan_buckets_num; i++) {
		p = priv->gscan_res + i;
		if (p->scan_id == bucket_id)
			break;
	}
	if (!p) {
		netdev_err(vif->ndev, "%s the bucket isnot exsit.\n", __func__);
		return -EINVAL;
	}
	if (MAX_AP_CACHE_PER_SCAN <= p->num_results) {
		netdev_err(vif->ndev, "%s the scan result reach the MAX num.\n",
			   __func__);
		return -EINVAL;
	}
	netdev_info(vif->ndev, "%s buketid: %d ,num_results:%d !\n",
		    __func__, bucket_id, p->num_results);
	for (i = 0; i < p->num_results; i++) {
		if (!memcmp(p->results[i].bssid, item->bssid, ETH_ALEN) &&
		    strlen(p->results[i].ssid) == strlen(item->ssid) &&
		    !memcmp(p->results[i].ssid, item->ssid,
			    strlen(item->ssid))) {
			netdev_err(vif->ndev, "%s BSS : %s  %pM exist.\n",
				   __func__, item->ssid, item->bssid);
			return -EINVAL;
		}
	}
	memcpy((void *)(&p->results[p->num_results]),
	       (void *)item, sizeof(struct sprdwl_gscan_result));
	p->results[p->num_results].ie_length = 0;
	p->results[p->num_results].ie_data[0] = 0;
	p->num_results++;
	return 0;
}

static int sprdwl_vendor_report_full_scan(struct sprdwl_vif *vif,
					  struct sprdwl_gscan_result *item)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct sk_buff *reply;
	int payload, rlen, ret;
	unsigned char *tmp;

	rlen = sizeof(struct sprdwl_gscan_result) + item->ie_length;
	payload = rlen + 0x100;
	reply = cfg80211_vendor_event_alloc(wiphy, payload, 2, GFP_KERNEL);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}
	tmp = skb_tail_pointer(reply);
	memcpy(tmp, (void *)item,
	       sizeof(struct sprdwl_gscan_result) + item->ie_length);
	skb_put(reply, sizeof(struct sprdwl_gscan_result) + item->ie_length);
	cfg80211_vendor_event(reply, GFP_KERNEL);
out:
	return ret;
}

void sprdwl_report_gscan_result(struct sprdwl_vif *vif,
				u32 report_event, u8 bucket_id,
				u16 chan, s16 rssi, const u8 *frame, u16 len)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *)frame;
	struct ieee80211_supported_band *band;
	struct ieee80211_channel *channel;
	struct sprdwl_gscan_result *gscan_res = NULL;
	u16 capability, beacon_interval;
	u32 freq;
	s32 signal;
	u64 tsf;
	u8 *ie;
	size_t ielen;
	const u8 *ssid;

	band = wiphy->bands[IEEE80211_BAND_2GHZ];
	freq = ieee80211_channel_to_frequency(chan, band->band);
	channel = ieee80211_get_channel(wiphy, freq);
	if (!channel) {
		netdev_err(vif->ndev, "%s invalid freq!\n", __func__);
		return;
	}
	signal = rssi * 100;
	if (!mgmt) {
		netdev_err(vif->ndev, "%s NULL frame!\n", __func__);
		return;
	}
	ie = mgmt->u.probe_resp.variable;
	ielen = len - offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
	tsf = le64_to_cpu(mgmt->u.probe_resp.timestamp);
	beacon_interval = le16_to_cpu(mgmt->u.probe_resp.beacon_int);
	capability = le16_to_cpu(mgmt->u.probe_resp.capab_info);
	netdev_dbg(vif->ndev, "   %s, %pM, channel %2u, signal %d\n",
		   ieee80211_is_probe_resp(mgmt->frame_control)
		   ? "proberesp" : "beacon   ", mgmt->bssid, chan, rssi);

	gscan_res = kmalloc(sizeof(*gscan_res) + ielen, GFP_KERNEL);
	if (!gscan_res)
		return;
	memset(gscan_res, 0x0, sizeof(struct sprdwl_gscan_result) + ielen);
	gscan_res->channel = freq;
	gscan_res->beacon_period = beacon_interval;
	gscan_res->ts = tsf;
	gscan_res->rssi = signal;
	gscan_res->ie_length = ielen;
	memcpy(gscan_res->bssid, mgmt->bssid, 6);
	memcpy(gscan_res->ie_data, ie, ielen);

	ssid = wpa_scan_get_ie(ie, ielen, WLAN_EID_SSID);
	if (!ssid) {
		netdev_err(vif->ndev, "%s BSS: No SSID IE included for %pM!\n",
			   __func__, mgmt->bssid);
		goto out;
	}
	if (ssid[1] > 32) {
		netdev_err(vif->ndev, "%s BSS: Too long SSID IE for %pM!\n",
			   __func__, mgmt->bssid);
		goto out;
	}
	memcpy(gscan_res->ssid, ssid + 2, ssid[1]);
	netdev_err(vif->ndev, "%s %pM : %s !\n", __func__,
		   mgmt->bssid, gscan_res->ssid);
	sprdwl_vendor_cached_scan_result(vif, bucket_id, gscan_res);
	if (report_event & REPORT_EVENTS_FULL_RESULTS)
		sprdwl_vendor_report_full_scan(vif, gscan_res);
out:
	kfree(gscan_res);
}

int sprdwl_buffer_full_event(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct sk_buff *reply;
	int payload, rlen, ret;

	rlen = sizeof(enum sprdwl_gscan_event) + sizeof(u32);
	payload = rlen + 0x100;
	reply = cfg80211_vendor_event_alloc(wiphy, payload, 0, GFP_KERNEL);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}
	nla_put_u32(reply, NL80211_ATTR_VENDOR_DATA, WIFI_SCAN_BUFFER_FULL);
	cfg80211_vendor_event(reply, GFP_KERNEL);
out:
	return ret;
}

int sprdwl_available_event(struct sprdwl_vif *vif)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct sk_buff *reply;
	struct sprdwl_gscan_cached_results *p = NULL;
	int ret = 0, payload, rlen, i, num = 0;
	unsigned char *tmp;

	rlen = sizeof(enum sprdwl_gscan_event) + sizeof(u32);
	payload = rlen + 0x100;
	reply = cfg80211_vendor_event_alloc(wiphy, payload, 1, GFP_KERNEL);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < priv->gscan_buckets_num; i++) {
		p = priv->gscan_res + i;
		num += p->num_results;
	}
	netdev_info(vif->ndev, "%s num:%d!\n", __func__, num);
	tmp = skb_tail_pointer(reply);
	memcpy(tmp, &num, sizeof(num));
	skb_put(reply, sizeof(num));
	cfg80211_vendor_event(reply, GFP_KERNEL);
out:
	return ret;
}

int sprdwl_gscan_done(struct sprdwl_vif *vif, u8 bucketid)
{
	struct sprdwl_priv *priv = vif->priv;
	struct wiphy *wiphy = priv->wiphy;
	struct sk_buff *reply;
	int payload, rlen, ret;
	int value;
	unsigned char *tmp;

	rlen = sizeof(enum sprdwl_gscan_event) +
	    sizeof(enum sprdwl_gscan_wifi_event);
	payload = rlen + 0x100;
	reply = cfg80211_vendor_event_alloc(wiphy, payload, 0, GFP_KERNEL);
	if (!reply) {
		ret = -ENOMEM;
		goto out;
	}
	value = WIFI_SCAN_COMPLETE;
	tmp = skb_tail_pointer(reply);
	memcpy(tmp, &value, sizeof(value));
	skb_put(reply, sizeof(value));
	cfg80211_vendor_event(reply, GFP_KERNEL);
out:
	return ret;
}

const struct wiphy_vendor_command sprdwl_vendor_cmd[] = {
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = SPRDWL_VENDOR_SET_COUNTRY_CODE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_set_country
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = SPRDWL_VENDOR_SET_LLSTAT
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_set_llstat_handler
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = SPRDWL_VENDOR_GET_LLSTAT
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_get_llstat_handler
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = SPRDWL_VENDOR_CLR_LLSTAT
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_clr_llstat_handler
	},
	{
	    {
		.vendor_id = OUI_SPREAD,
		.subcmd = GSCAN_GET_CAPABILITIES,
	    },
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_get_gscan_capabilities,
	},
	{
	    {
		.vendor_id = OUI_SPREAD,
		.subcmd = GSCAN_GET_CHANNEL_LIST,
	    },
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_get_channel_list,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = GSCAN_SET_CONFIG,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_set_config,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = GSCAN_SET_SCAN_CONFIG,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_set_scan_config,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = GSCAN_ENABLE_GSCAN,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_enable_gscan,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = GSCAN_ENABLE_FULL_SCAN_RESULTS,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_enable_full_gscan,
	},
		{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = GSCAN_ENABLE_GSCAN,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_enable_gscan,
	},
	{
		{
			.vendor_id = OUI_SPREAD,
			.subcmd = GSCAN_GET_SCAN_RESULTS,
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = sprdwl_vendor_get_gscan_results,
	}
};

static const struct nl80211_vendor_cmd_info sprdwl_vendor_events[] = {
	{
		.vendor_id = OUI_SPREAD,
		.subcmd = GSCAN_EVENT_COMPLETE_SCAN,
	},
	{
		.vendor_id = OUI_SPREAD,
		.subcmd = GSCAN_EVENT_SCAN_RESULTS_AVAILABLE,
	},
	{
		.vendor_id = OUI_SPREAD,
		.subcmd = GSCAN_EVENT_FULL_SCAN_RESULTS,
	},
};

int sprdwl_vendor_init(struct wiphy *wiphy)
{
	wiphy->vendor_commands = sprdwl_vendor_cmd;
	wiphy->n_vendor_commands = ARRAY_SIZE(sprdwl_vendor_cmd);
	wiphy->vendor_events = sprdwl_vendor_events;
	wiphy->n_vendor_events = ARRAY_SIZE(sprdwl_vendor_events);
	return 0;
}

int sprdwl_vendor_deinit(struct wiphy *wiphy)
{
	struct sprdwl_priv *priv = wiphy_priv(wiphy);

	wiphy->vendor_commands = NULL;
	wiphy->n_vendor_commands = 0;
	kfree(priv->gscan_res);
	return 0;
}
