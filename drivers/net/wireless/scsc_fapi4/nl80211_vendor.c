/*****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/
#include <linux/version.h>
#include <net/cfg80211.h>
#include <linux/etherdevice.h>
#include "dev.h"
#include "cfg80211_ops.h"
#include "debug.h"
#include "mgt.h"
#include "mlme.h"
#include "netif.h"
#include "unifiio.h"
#include "mib.h"
#include "nl80211_vendor.h"

#define SLSI_GSCAN_INVALID_RSSI        0x7FFF
#define SLSI_EPNO_AUTH_FIELD_WEP_OPEN  1
#define SLSI_EPNO_AUTH_FIELD_WPA_PSK   2
#define SLSI_EPNO_AUTH_FIELD_WPA_EAP   4

#ifdef CONFIG_SCSC_WLAN_DEBUG
char *slsi_print_event_name(int event_id)
{
	switch (event_id) {
	case SLSI_NL80211_SIGNIFICANT_CHANGE_EVENT:
		return "SIGNIFICANT_CHANGE_EVENT";
	case SLSI_NL80211_HOTLIST_AP_FOUND_EVENT:
		return "HOTLIST_AP_FOUND_EVENT";
	case SLSI_NL80211_SCAN_RESULTS_AVAILABLE_EVENT:
		return "SCAN_RESULTS_AVAILABLE_EVENT";
	case SLSI_NL80211_FULL_SCAN_RESULT_EVENT:
		return "FULL_SCAN_RESULT_EVENT";
	case SLSI_NL80211_SCAN_EVENT:
		return "BUCKET_SCAN_DONE_EVENT";
	case SLSI_NL80211_HOTLIST_AP_LOST_EVENT:
		return "HOTLIST_AP_LOST_EVENT";
#ifdef CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD
	case SLSI_NL80211_VENDOR_SUBCMD_KEY_MGMT_ROAM_AUTH:
		return "KEY_MGMT_ROAM_AUTH";
#endif
	case SLSI_NL80211_VENDOR_HANGED_EVENT:
		return "SLSI_NL80211_VENDOR_HANGED_EVENT";
	case SLSI_NL80211_EPNO_EVENT:
		return "SLSI_NL80211_EPNO_EVENT";
	case SLSI_NL80211_HOTSPOT_MATCH:
		return "SLSI_NL80211_HOTSPOT_MATCH";
	case SLSI_NL80211_RSSI_REPORT_EVENT:
		return "SLSI_NL80211_RSSI_REPORT_EVENT";

	default:
		return "UNKNOWN_EVENT";
	}
}
#endif

int slsi_vendor_event(struct slsi_dev *sdev, int event_id, const void *data, int len)
{
	struct sk_buff *skb;

#ifdef CONFIG_SCSC_WLAN_DEBUG
	SLSI_DBG1_NODEV(SLSI_MLME, "Event: %s(%d), data = %p, len = %d\n",
			slsi_print_event_name(event_id), event_id, data, len);
#endif

	skb = cfg80211_vendor_event_alloc(sdev->wiphy, len, event_id, GFP_KERNEL);
	if (skb == NULL) {
		SLSI_ERR_NODEV("Failed to allocate skb for vendor event: %d\n", event_id);
		return -ENOMEM;
	}

	nla_put_nohdr(skb, len, data);

	cfg80211_vendor_event(skb, GFP_KERNEL);

	return 0;
}

static int slsi_vendor_cmd_reply(struct wiphy *wiphy, const void *data, int len)
{
	struct sk_buff *skb;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, len);
	if (skb == NULL) {
		SLSI_ERR_NODEV("Failed to allocate skb\n");
		return -ENOMEM;
	}

	nla_put_nohdr(skb, len, data);

	return cfg80211_vendor_cmd_reply(skb);
}

static struct net_device *slsi_gscan_get_netdev(struct slsi_dev *sdev)
{
	return slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
}

static struct netdev_vif *slsi_gscan_get_vif(struct slsi_dev *sdev)
{
	struct net_device *dev;

	dev = slsi_gscan_get_netdev(sdev);
	if (WARN_ON(!dev))
		return NULL;

	return netdev_priv(dev);
}

#ifdef CONFIG_SCSC_WLAN_DEBUG
static void slsi_gscan_add_dump_params(struct slsi_nl_gscan_param *nl_gscan_param)
{
	int i;
	int j;

	SLSI_DBG2_NODEV(SLSI_GSCAN, "Parameters for SLSI_NL80211_VENDOR_SUBCMD_ADD_GSCAN sub-command:\n");
	SLSI_DBG2_NODEV(SLSI_GSCAN, "base_period: %d max_ap_per_scan: %d report_threshold_percent: %d report_threshold_num_scans = %d num_buckets: %d\n",
			nl_gscan_param->base_period, nl_gscan_param->max_ap_per_scan,
			nl_gscan_param->report_threshold_percent, nl_gscan_param->report_threshold_num_scans,
			nl_gscan_param->num_buckets);

	for (i = 0; i < nl_gscan_param->num_buckets; i++) {
		SLSI_DBG2_NODEV(SLSI_GSCAN, "Bucket: %d\n", i);
		SLSI_DBG2_NODEV(SLSI_GSCAN, "\tbucket_index: %d band: %d period: %d report_events: %d num_channels: %d\n",
				nl_gscan_param->nl_bucket[i].bucket_index, nl_gscan_param->nl_bucket[i].band,
				nl_gscan_param->nl_bucket[i].period, nl_gscan_param->nl_bucket[i].report_events,
				nl_gscan_param->nl_bucket[i].num_channels);

		for (j = 0; j < nl_gscan_param->nl_bucket[i].num_channels; j++)
			SLSI_DBG2_NODEV(SLSI_GSCAN, "\tchannel_list[%d]: %d\n",
					j, nl_gscan_param->nl_bucket[i].channels[j].channel);
	}
}

static void slsi_gscan_set_hotlist_dump_params(struct slsi_nl_hotlist_param *nl_hotlist_param)
{
	int i;

	SLSI_DBG2_NODEV(SLSI_GSCAN, "Parameters for SUBCMD_SET_BSSID_HOTLIST sub-command:\n");
	SLSI_DBG2_NODEV(SLSI_GSCAN, "lost_ap_sample_size: %d, num_bssid: %d\n",
			nl_hotlist_param->lost_ap_sample_size, nl_hotlist_param->num_bssid);

	for (i = 0; i < nl_hotlist_param->num_bssid; i++) {
		SLSI_DBG2_NODEV(SLSI_GSCAN, "AP[%d]\n", i);
		SLSI_DBG2_NODEV(SLSI_GSCAN, "\tBSSID:%pM rssi_low:%d rssi_high:%d\n",
				nl_hotlist_param->ap[i].bssid, nl_hotlist_param->ap[i].low, nl_hotlist_param->ap[i].high);
	}
}

void slsi_gscan_scan_res_dump(struct slsi_gscan_result *scan_data)
{
	struct slsi_nl_scan_result_param *nl_scan_res = &scan_data->nl_scan_res;

	SLSI_DBG3_NODEV(SLSI_GSCAN, "TS:%llu SSID:%s BSSID:%pM Chan:%d RSSI:%d Bcn_Int:%d Capab:%#x IE_Len:%d\n",
			nl_scan_res->ts, nl_scan_res->ssid, nl_scan_res->bssid, nl_scan_res->channel,
			nl_scan_res->rssi, nl_scan_res->beacon_period, nl_scan_res->capability, nl_scan_res->ie_length);

	SLSI_DBG_HEX_NODEV(SLSI_GSCAN, &nl_scan_res->ie_data[0], nl_scan_res->ie_length, "IE_Data:\n");
	if (scan_data->anqp_length) {
		SLSI_DBG3_NODEV(SLSI_GSCAN, "ANQP_LENGTH:%d\n", scan_data->anqp_length);
		SLSI_DBG_HEX_NODEV(SLSI_GSCAN, nl_scan_res->ie_data + nl_scan_res->ie_length, scan_data->anqp_length, "ANQP_info:\n");
	}
}
#endif

static int slsi_gscan_get_capabilities(struct wiphy *wiphy,
				       struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_nl_gscan_capabilities nl_cap;
	int                               ret = 0;
	struct slsi_dev                   *sdev = SDEV_FROM_WIPHY(wiphy);

	SLSI_DBG1_NODEV(SLSI_GSCAN, "SUBCMD_GET_CAPABILITIES\n");

	memset(&nl_cap, 0, sizeof(struct slsi_nl_gscan_capabilities));

	ret = slsi_mib_get_gscan_cap(sdev, &nl_cap);
	if (ret != 0) {
		SLSI_ERR(sdev, "Failed to read mib\n");
		return ret;
	}

	nl_cap.max_scan_cache_size = SLSI_GSCAN_MAX_SCAN_CACHE_SIZE;
	nl_cap.max_ap_cache_per_scan = SLSI_GSCAN_MAX_AP_CACHE_PER_SCAN;
	nl_cap.max_scan_reporting_threshold = SLSI_GSCAN_MAX_SCAN_REPORTING_THRESHOLD;

	ret = slsi_vendor_cmd_reply(wiphy, &nl_cap, sizeof(struct slsi_nl_gscan_capabilities));
	if (ret)
		SLSI_ERR_NODEV("gscan_get_capabilities vendor cmd reply failed (err = %d)\n", ret);

	return ret;
}

static u32 slsi_gscan_put_channels(struct ieee80211_supported_band *chan_data, bool no_dfs, bool only_dfs, u32 *buf)
{
	u32 chan_count = 0;
	u32 chan_flags;
	int i;

	if (chan_data == NULL) {
		SLSI_DBG3_NODEV(SLSI_GSCAN, "Band not supported\n");
		return 0;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 14, 0)
	chan_flags = (IEEE80211_CHAN_PASSIVE_SCAN | IEEE80211_CHAN_NO_OFDM | IEEE80211_CHAN_RADAR);
#else
	chan_flags = (IEEE80211_CHAN_NO_IR | IEEE80211_CHAN_NO_OFDM | IEEE80211_CHAN_RADAR);
#endif

	for (i = 0; i < chan_data->n_channels; i++) {
		if (chan_data->channels[i].flags & IEEE80211_CHAN_DISABLED)
			continue;
		if (only_dfs) {
			if (chan_data->channels[i].flags & chan_flags)
				buf[chan_count++] = chan_data->channels[i].center_freq;
			continue;
		}
		if (no_dfs && (chan_data->channels[i].flags & chan_flags))
			continue;
		buf[chan_count++] = chan_data->channels[i].center_freq;
	}
	return chan_count;
}

static int slsi_gscan_get_valid_channel(struct wiphy *wiphy,
					struct wireless_dev *wdev, const void *data, int len)
{
	int             ret = 0, type, band;
	struct slsi_dev *sdev = SDEV_FROM_WIPHY(wiphy);
	u32             *chan_list;
	u32             chan_count = 0, mem_len = 0;
	struct sk_buff  *reply;

	type = nla_type(data);

	if (type == GSCAN_ATTRIBUTE_BAND)
		band = nla_get_u32(data);
	else
		return -EINVAL;

	if (band == 0) {
		SLSI_WARN(sdev, "NO Bands. return 0 channel\n");
		return ret;
	}

	SLSI_MUTEX_LOCK(sdev->netdev_add_remove_mutex);
	SLSI_DBG3(sdev, SLSI_GSCAN, "band %d\n", band);
	if (wiphy->bands[IEEE80211_BAND_2GHZ]) {
		mem_len += wiphy->bands[IEEE80211_BAND_2GHZ]->n_channels * sizeof(u32);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d 2.4GHz chan\n", wiphy->bands[IEEE80211_BAND_2GHZ]->n_channels);
	}
	if (wiphy->bands[IEEE80211_BAND_5GHZ]) {
		mem_len += wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels * sizeof(u32);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d 5GHz chan\n", wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels);
	}
	if (mem_len == 0) {
		ret = -ENOTSUPP;
		goto exit;
	}

	chan_list = kmalloc(mem_len, GFP_KERNEL);
	if (chan_list == NULL) {
		ret = -ENOMEM;
		goto exit;
	}
	mem_len += SLSI_NL_VENDOR_REPLY_OVERHEAD + (SLSI_NL_ATTRIBUTE_U32_LEN * 2);
	reply = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_len);
	if (reply == NULL) {
		ret = -ENOMEM;
		goto exit_with_chan_list;
	}
	switch (band) {
	case WIFI_BAND_BG:
		chan_count = slsi_gscan_put_channels(wiphy->bands[IEEE80211_BAND_2GHZ], false, false, chan_list);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d total chan filled(2.4 All)\n", chan_count);
		break;
	case WIFI_BAND_A:
		chan_count = slsi_gscan_put_channels(wiphy->bands[IEEE80211_BAND_5GHZ], true, false, chan_list);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d total chan filled(5GHz no dfs)\n", chan_count);
		break;
	case WIFI_BAND_A_DFS:
		chan_count = slsi_gscan_put_channels(wiphy->bands[IEEE80211_BAND_5GHZ], false, true, chan_list);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d total chan filled(5GHz dfs only)\n", chan_count);
		break;
	case WIFI_BAND_A_WITH_DFS:
		chan_count = slsi_gscan_put_channels(wiphy->bands[IEEE80211_BAND_5GHZ], false, false, chan_list);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d total chan filled(5GHz All)\n", chan_count);
		break;
	case WIFI_BAND_ABG:
		chan_count = slsi_gscan_put_channels(wiphy->bands[IEEE80211_BAND_2GHZ], true, false, chan_list);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d total chan filled(2.4 no dfs)\n", chan_count);
		chan_count += slsi_gscan_put_channels(wiphy->bands[IEEE80211_BAND_5GHZ], true, false, chan_list + chan_count);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d total chan filled(5 + 2.4 no dfs)\n", chan_count);
		break;
	case WIFI_BAND_ABG_WITH_DFS:
		chan_count = slsi_gscan_put_channels(wiphy->bands[IEEE80211_BAND_2GHZ], false, false, chan_list);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d total chan filled(2.4 ALL)\n", chan_count);
		chan_count += slsi_gscan_put_channels(wiphy->bands[IEEE80211_BAND_5GHZ], false, false, chan_list + chan_count);
		SLSI_DBG3(sdev, SLSI_GSCAN, "%d total chan filled(5 + 2.4 ALL)\n", chan_count);
		break;
	default:
		chan_count = 0;
		SLSI_WARN(sdev, "Invalid Band %d\n", band);
	}
	nla_put_u32(reply, GSCAN_ATTRIBUTE_NUM_CHANNELS, chan_count);
	nla_put(reply, GSCAN_ATTRIBUTE_CHANNEL_LIST, chan_count * sizeof(u32), chan_list);

	ret =  cfg80211_vendor_cmd_reply(reply);

	if (ret)
		SLSI_ERR(sdev, "FAILED to reply GET_VALID_CHANNELS\n");

exit_with_chan_list:
	kfree(chan_list);
exit:
	SLSI_MUTEX_UNLOCK(sdev->netdev_add_remove_mutex);
	return ret;
}

struct slsi_gscan_result *slsi_prepare_scan_result(struct sk_buff *skb, u16 anqp_length, int hs2_id)
{
	struct ieee80211_mgmt    *mgmt = fapi_get_mgmt(skb);
	struct slsi_gscan_result *scan_res;
	struct timespec          ts;
	const u8                 *ssid_ie;
	int                      mem_reqd;
	int                      ie_len;
	u8                       *ie;

	ie = &mgmt->u.beacon.variable[0];
	ie_len = fapi_get_datalen(skb) - (ie - (u8 *)mgmt) - anqp_length;

	/* Exclude 1 byte for ie_data[1]. sizeof(u16) to include anqp_length, sizeof(int) for hs_id */
	mem_reqd = (sizeof(struct slsi_gscan_result) - 1) + ie_len + anqp_length + sizeof(int) + sizeof(u16);

	/* Allocate memory for scan result */
	scan_res = kmalloc(mem_reqd, GFP_KERNEL);
	if (scan_res == NULL) {
		SLSI_ERR_NODEV("Failed to allocate memory for scan result\n");
		return NULL;
	}

	/* Exclude 1 byte for ie_data[1] */
	scan_res->scan_res_len = (sizeof(struct slsi_nl_scan_result_param) - 1) + ie_len;
	scan_res->anqp_length = 0;

	get_monotonic_boottime(&ts);
	scan_res->nl_scan_res.ts = (u64)TIMESPEC_TO_US(ts);

	ssid_ie = cfg80211_find_ie(WLAN_EID_SSID, &mgmt->u.beacon.variable[0], ie_len);
	if ((ssid_ie != NULL) && (ssid_ie[1] > 0) && (ssid_ie[1] < IEEE80211_MAX_SSID_LEN)) {
		memcpy(scan_res->nl_scan_res.ssid, &ssid_ie[2], ssid_ie[1]);
		scan_res->nl_scan_res.ssid[ssid_ie[1]] = '\0';
	} else {
		scan_res->nl_scan_res.ssid[0] = '\0';
	}

	SLSI_ETHER_COPY(scan_res->nl_scan_res.bssid, mgmt->bssid);
	scan_res->nl_scan_res.channel = fapi_get_u16(skb, u.mlme_scan_ind.channel_frequency) / 2;
	scan_res->nl_scan_res.rssi = fapi_get_s16(skb, u.mlme_scan_ind.rssi);
	scan_res->nl_scan_res.rtt = SLSI_GSCAN_RTT_UNSPECIFIED;
	scan_res->nl_scan_res.rtt_sd = SLSI_GSCAN_RTT_UNSPECIFIED;
	scan_res->nl_scan_res.beacon_period = mgmt->u.beacon.beacon_int;
	scan_res->nl_scan_res.capability = mgmt->u.beacon.capab_info;
	scan_res->nl_scan_res.ie_length = ie_len;
	memcpy(scan_res->nl_scan_res.ie_data, ie, ie_len);
	memcpy(scan_res->nl_scan_res.ie_data + ie_len, &hs2_id, sizeof(int));
	memcpy(scan_res->nl_scan_res.ie_data + ie_len + sizeof(int), &anqp_length, sizeof(u16));
	if (anqp_length) {
		memcpy(scan_res->nl_scan_res.ie_data + ie_len + sizeof(u16) + sizeof(int), ie + ie_len, anqp_length);
		scan_res->anqp_length = anqp_length + sizeof(u16) + sizeof(int);
	}

#ifdef CONFIG_SCSC_WLAN_DEBUG
	slsi_gscan_scan_res_dump(scan_res);
#endif

	return scan_res;
}

void slsi_hotlist_ap_lost_indication(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct slsi_nl_scan_result_param *nl_scan_res;
	struct slsi_hotlist_result       *hotlist, *temp;
	struct netdev_vif                *ndev_vif = netdev_priv(dev);
	u8                               *mac_addr;
	u16                              num_entries;
	int                              mem_reqd;
	bool                             found = false;
	int                              offset = 0;
	int                              i;

	if (WARN_ON(!ndev_vif)) {
		slsi_kfree_skb(skb);
		return;
	}

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	num_entries = fapi_get_s16(skb, u.mlme_ap_loss_ind.entries);
	mac_addr = fapi_get_data(skb);

	SLSI_NET_DBG1(dev, SLSI_GSCAN, "Hotlist AP Lost Indication: num_entries %d\n", num_entries);

	mem_reqd = num_entries * sizeof(struct slsi_nl_scan_result_param);
	nl_scan_res = kmalloc(mem_reqd, GFP_KERNEL);
	if (nl_scan_res == NULL) {
		SLSI_NET_ERR(dev, "Failed to allocate memory for hotlist lost\n");
		goto out;
	}

	for (i = 0; i < num_entries; i++) {
		SLSI_NET_DBG3(dev, SLSI_GSCAN,
			      "Remove the GSCAN results for the lost AP: %pM\n", &mac_addr[ETH_ALEN * i]);
		slsi_gscan_hash_remove(sdev, &mac_addr[ETH_ALEN * i]);

		list_for_each_entry_safe(hotlist, temp, &sdev->hotlist_results, list) {
			if (memcmp(hotlist->nl_scan_res.bssid, &mac_addr[ETH_ALEN * i], ETH_ALEN) == 0) {
				SLSI_NET_DBG2(dev, SLSI_GSCAN, "Lost AP [%d]: %pM\n", i, &mac_addr[ETH_ALEN * i]);
				list_del(&hotlist->list);

				hotlist->nl_scan_res.ie_length = 0; /* Not sending the IE for hotlist lost event */
				memcpy(&nl_scan_res[offset], &hotlist->nl_scan_res, sizeof(struct slsi_nl_scan_result_param));
				offset++;

				kfree(hotlist);
				found = true;
				break;
			}
		}

		if (!found)
			SLSI_NET_ERR(dev, "Hostlist record is not available in scan result\n");

		found = false;
	}

	slsi_vendor_event(sdev, SLSI_NL80211_HOTLIST_AP_LOST_EVENT,
			  nl_scan_res, (offset * sizeof(struct slsi_nl_scan_result_param)));

	kfree(nl_scan_res);
out:
	slsi_kfree_skb(skb);
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
}

void slsi_hotlist_ap_found_indication(struct slsi_dev *sdev, struct net_device *dev, struct slsi_gscan_result *scan_res)
{
	struct slsi_hotlist_result *hotlist, *temp;
	int                        num_hotlist_results = 0;
	struct netdev_vif          *ndev_vif = netdev_priv(dev);
	int                        mem_reqd;
	u8                         *event_buffer;
	int                        offset;

	if (WARN_ON(!ndev_vif))
		return;

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->scan_mutex));

	SLSI_NET_DBG1(dev, SLSI_GSCAN, "Hotlist AP Found Indication: %pM\n", scan_res->nl_scan_res.bssid);

	/* Check if the hotlist result is already available */
	list_for_each_entry_safe(hotlist, temp, &sdev->hotlist_results, list) {
		if (memcmp(hotlist->nl_scan_res.bssid, scan_res->nl_scan_res.bssid, ETH_ALEN) == 0) {
			SLSI_DBG3(sdev, SLSI_GSCAN, "Hotlist result already available for: %pM\n", scan_res->nl_scan_res.bssid);
			/* Delete the old result - store the new result */
			list_del(&hotlist->list);
			kfree(hotlist);
			break;
		}
	}

	/* Allocating memory for storing the hostlist result */
	mem_reqd = scan_res->scan_res_len + (sizeof(struct slsi_hotlist_result) - sizeof(struct slsi_nl_scan_result_param));
	SLSI_DBG3(sdev, SLSI_GSCAN, "hotlist result alloc size: %d\n", mem_reqd);
	hotlist = kmalloc(mem_reqd, GFP_KERNEL);
	if (hotlist == NULL) {
		SLSI_ERR(sdev, "Failed to allocate memory for hotlist\n");
		return;
	}

	hotlist->scan_res_len = scan_res->scan_res_len;
	memcpy(&hotlist->nl_scan_res, &scan_res->nl_scan_res, scan_res->scan_res_len);

	INIT_LIST_HEAD(&hotlist->list);
	list_add(&hotlist->list, &sdev->hotlist_results);

	/* Calculate the number of hostlist results and mem required */
	mem_reqd = 0;
	offset = 0;
	list_for_each_entry_safe(hotlist, temp, &sdev->hotlist_results, list) {
		mem_reqd += sizeof(struct slsi_nl_scan_result_param); /* If IE required use: hotlist->scan_res_len */
		num_hotlist_results++;
	}
	SLSI_DBG3(sdev, SLSI_GSCAN, "num_hotlist_results = %d, mem_reqd = %d\n", num_hotlist_results, mem_reqd);

	/* Allocate event buffer */
	event_buffer = kmalloc(mem_reqd, GFP_KERNEL);
	if (event_buffer == NULL) {
		SLSI_ERR_NODEV("Failed to allocate memory for event_buffer\n");
		return;
	}

	/* Prepare the event buffer */
	list_for_each_entry_safe(hotlist, temp, &sdev->hotlist_results, list) {
		memcpy(&event_buffer[offset], &hotlist->nl_scan_res, sizeof(struct slsi_nl_scan_result_param));
		offset += sizeof(struct slsi_nl_scan_result_param); /* If IE required use: hotlist->scan_res_len */
	}

	slsi_vendor_event(sdev, SLSI_NL80211_HOTLIST_AP_FOUND_EVENT, event_buffer, offset);

	kfree(event_buffer);
}

static void slsi_gscan_hash_add(struct slsi_dev *sdev, struct slsi_gscan_result *scan_res)
{
	u8                key = SLSI_GSCAN_GET_HASH_KEY(scan_res->nl_scan_res.bssid[5]);
	struct netdev_vif *ndev_vif;

	ndev_vif = slsi_gscan_get_vif(sdev);
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->scan_mutex));

	scan_res->hnext = sdev->gscan_hash_table[key];
	sdev->gscan_hash_table[key] = scan_res;

	/* Update the total buffer consumed and number of scan results */
	sdev->buffer_consumed += scan_res->scan_res_len;
	sdev->num_gscan_results++;
}

static struct slsi_gscan_result *slsi_gscan_hash_get(struct slsi_dev *sdev, u8 *mac)
{
	struct slsi_gscan_result *temp;
	struct netdev_vif        *ndev_vif;
	u8                       key = SLSI_GSCAN_GET_HASH_KEY(mac[5]);

	ndev_vif = slsi_gscan_get_vif(sdev);

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->scan_mutex));

	temp = sdev->gscan_hash_table[key];
	while (temp != NULL) {
		if (memcmp(temp->nl_scan_res.bssid, mac, ETH_ALEN) == 0)
			return temp;
		temp = temp->hnext;
	}

	return NULL;
}

void slsi_gscan_hash_remove(struct slsi_dev *sdev, u8 *mac)
{
	u8                       key = SLSI_GSCAN_GET_HASH_KEY(mac[5]);
	struct slsi_gscan_result *curr;
	struct slsi_gscan_result *prev;
	struct netdev_vif        *ndev_vif;
	struct slsi_gscan_result *scan_res = NULL;

	ndev_vif = slsi_gscan_get_vif(sdev);
	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->scan_mutex));

	if (sdev->gscan_hash_table[key] == NULL)
		return;

	if (memcmp(sdev->gscan_hash_table[key]->nl_scan_res.bssid, mac, ETH_ALEN) == 0) {
		scan_res = sdev->gscan_hash_table[key];
		sdev->gscan_hash_table[key] = sdev->gscan_hash_table[key]->hnext;
	} else {
		prev = sdev->gscan_hash_table[key];
		curr = prev->hnext;

		while (curr != NULL) {
			if (memcmp(curr->nl_scan_res.bssid, mac, ETH_ALEN) == 0) {
				scan_res = curr;
				prev->hnext = curr->hnext;
				break;
			}
			prev = curr;
			curr = curr->hnext;
		}
	}

	if (scan_res) {
		/* Update the total buffer consumed and number of scan results */
		sdev->buffer_consumed -= scan_res->scan_res_len;
		sdev->num_gscan_results--;
		kfree(scan_res);
	}

	if (WARN_ON(sdev->num_gscan_results < 0))
		SLSI_ERR(sdev, "Wrong num_gscan_results: %d\n", sdev->num_gscan_results);
}

int slsi_check_scan_result(struct slsi_dev *sdev, struct slsi_bucket *bucket, struct slsi_gscan_result *new_scan_res)
{
	struct slsi_gscan_result *scan_res;

	/* Check if the scan result for the same BSS already exists in driver buffer */
	scan_res = slsi_gscan_hash_get(sdev, new_scan_res->nl_scan_res.bssid);
	if (scan_res == NULL) { /* New scan result */
		if ((sdev->buffer_consumed + new_scan_res->scan_res_len) >= SLSI_GSCAN_MAX_SCAN_CACHE_SIZE) {
			SLSI_DBG2(sdev, SLSI_GSCAN,
				  "Scan buffer full, discarding scan result, buffer_consumed = %d, buffer_threshold = %d\n",
				  sdev->buffer_consumed, sdev->buffer_threshold);

			/* Scan buffer is full can't store anymore new results */
			return SLSI_DISCARD_SCAN_RESULT;
		}

		return SLSI_KEEP_SCAN_RESULT;
	}

	/* Even if scan buffer is full existing results can be replaced with the latest one */
	if (scan_res->scan_cycle == bucket->scan_cycle)
		/* For the same scan cycle the result will be replaced only if the RSSI is better */
		if (new_scan_res->nl_scan_res.rssi < scan_res->nl_scan_res.rssi)
			return SLSI_DISCARD_SCAN_RESULT;

	/* Remove the existing scan result */
	slsi_gscan_hash_remove(sdev, scan_res->nl_scan_res.bssid);

	return SLSI_KEEP_SCAN_RESULT;
}

void slsi_gscan_handle_scan_result(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb, u16 scan_id, bool scan_done)
{
	struct slsi_gscan_result *scan_res = NULL;
	struct netdev_vif        *ndev_vif = netdev_priv(dev);
	struct slsi_bucket       *bucket;
	u16                      bucket_index;
	int                      event_type = WIFI_SCAN_FAILED;
	u16                      anqp_length;
	int                      hs2_network_id;

	WARN_ON(!SLSI_MUTEX_IS_LOCKED(ndev_vif->scan_mutex));

	SLSI_NET_DBG_HEX(dev, SLSI_GSCAN, skb->data, skb->len, "mlme_scan_ind skb->len: %d\n", skb->len);

	bucket_index = scan_id - SLSI_GSCAN_SCAN_ID_START;
	if (bucket_index >= SLSI_GSCAN_MAX_BUCKETS) {
		SLSI_NET_ERR(dev, "Invalid bucket index: %d (scan_id = %#x)\n", bucket_index, scan_id);
		goto out;
	}

	bucket = &sdev->bucket[bucket_index];
	if (!bucket->used) {
		SLSI_NET_DBG1(dev, SLSI_GSCAN, "Bucket is not active, index: %d (scan_id = %#x)\n", bucket_index, scan_id);
		goto out;
	}

	/* For scan_done indication - no need to store the results */
	if (scan_done) {
		bucket->scan_cycle++;
		bucket->gscan->num_scans++;

		SLSI_NET_DBG3(dev, SLSI_GSCAN, "scan done, scan_cycle = %d, num_scans = %d\n",
			      bucket->scan_cycle, bucket->gscan->num_scans);

		if (bucket->report_events & SLSI_REPORT_EVENTS_EACH_SCAN)
			event_type = WIFI_SCAN_RESULTS_AVAILABLE;
		if (bucket->gscan->num_scans % bucket->gscan->report_threshold_num_scans == 0)
			event_type = WIFI_SCAN_THRESHOLD_NUM_SCANS;
		if (sdev->buffer_consumed >= sdev->buffer_threshold)
			event_type = WIFI_SCAN_THRESHOLD_PERCENT;

		if (event_type != WIFI_SCAN_FAILED)
			slsi_vendor_event(sdev, SLSI_NL80211_SCAN_EVENT, &event_type, sizeof(event_type));

		goto out;
	}

	anqp_length = fapi_get_u16(skb, u.mlme_scan_ind.anqp_elements_length);
	/* TODO new FAPI 3.c has mlme_scan_ind.network_block_id, use that when fapi is updated. */
	hs2_network_id = 1;

	scan_res = slsi_prepare_scan_result(skb, anqp_length, hs2_network_id);
	if (scan_res == NULL) {
		SLSI_NET_ERR(dev, "Failed to prepare scan result\n");
		goto out;
	}

	/* Check for hotlist AP or ePNO networks */
	if (fapi_get_u16(skb, u.mlme_scan_ind.hotlisted_ap)) {
		slsi_hotlist_ap_found_indication(sdev, dev, scan_res);
	} else if (fapi_get_u16(skb, u.mlme_scan_ind.preferrednetwork_ap)) {
		if (anqp_length == 0)
			slsi_vendor_event(sdev, SLSI_NL80211_EPNO_EVENT,
					  &scan_res->nl_scan_res, scan_res->scan_res_len);
		else
			slsi_vendor_event(sdev, SLSI_NL80211_HOTSPOT_MATCH,
					  &scan_res->nl_scan_res, scan_res->scan_res_len + scan_res->anqp_length);
	}

	if (bucket->report_events & SLSI_REPORT_EVENTS_FULL_RESULTS) {
		struct sk_buff *nlevent;

		SLSI_NET_DBG3(dev, SLSI_GSCAN, "report_events: SLSI_REPORT_EVENTS_FULL_RESULTS\n");
		nlevent = cfg80211_vendor_event_alloc(sdev->wiphy, scan_res->scan_res_len + 4, SLSI_NL80211_FULL_SCAN_RESULT_EVENT, GFP_KERNEL);
		if (!nlevent) {
			SLSI_ERR(sdev, "failed to allocate sbk of size:%d\n", scan_res->scan_res_len + 4);
			kfree(scan_res);
			goto out;
		}
		if (nla_put_u32(nlevent, GSCAN_ATTRIBUTE_SCAN_BUCKET_BIT, (1 << bucket_index)) ||
		    nla_put(nlevent, GSCAN_ATTRIBUTE_SCAN_RESULTS, scan_res->scan_res_len, &scan_res->nl_scan_res)) {
			SLSI_ERR(sdev, "failed to put data\n");
			kfree_skb(nlevent);
			kfree(scan_res);
			goto out;
		}
		cfg80211_vendor_event(nlevent, GFP_KERNEL);
	}

	if (slsi_check_scan_result(sdev, bucket, scan_res) == SLSI_DISCARD_SCAN_RESULT) {
		kfree(scan_res);
		goto out;
	 }
	slsi_gscan_hash_add(sdev, scan_res);

out:
	slsi_kfree_skb(skb);
}

u8 slsi_gscan_get_scan_policy(enum wifi_band band)
{
	u8 scan_policy;

	switch (band) {
	case WIFI_BAND_UNSPECIFIED:
		scan_policy = FAPI_SCANPOLICY_ANY_RA;
		break;
	case WIFI_BAND_BG:
		scan_policy = FAPI_SCANPOLICY_2_4GHZ;
		break;
	case WIFI_BAND_A:
		scan_policy = (FAPI_SCANPOLICY_5GHZ |
			       FAPI_SCANPOLICY_NON_DFS);
		break;
	case WIFI_BAND_A_DFS:
		scan_policy = (FAPI_SCANPOLICY_5GHZ |
			       FAPI_SCANPOLICY_DFS);
		break;
	case WIFI_BAND_A_WITH_DFS:
		scan_policy = (FAPI_SCANPOLICY_5GHZ |
			       FAPI_SCANPOLICY_NON_DFS |
			       FAPI_SCANPOLICY_DFS);
		break;
	case WIFI_BAND_ABG:
		scan_policy = (FAPI_SCANPOLICY_5GHZ |
			       FAPI_SCANPOLICY_NON_DFS |
			       FAPI_SCANPOLICY_2_4GHZ);
		break;
	case WIFI_BAND_ABG_WITH_DFS:
		scan_policy = (FAPI_SCANPOLICY_5GHZ |
			       FAPI_SCANPOLICY_NON_DFS |
			       FAPI_SCANPOLICY_DFS |
			       FAPI_SCANPOLICY_2_4GHZ);
		break;
	default:
		scan_policy = FAPI_SCANPOLICY_ANY_RA;
		break;
	}

	SLSI_DBG2_NODEV(SLSI_GSCAN, "Scan Policy: %#x\n", scan_policy);

	return scan_policy;
}

static int slsi_gscan_add_read_params(struct slsi_nl_gscan_param *nl_gscan_param, const void *data, int len)
{
	int                         j = 0;
	int                         type, tmp, tmp1, tmp2, k = 0;
	const struct nlattr         *iter, *iter1, *iter2;
	struct slsi_nl_bucket_param *nl_bucket;

	nla_for_each_attr(iter, data, len, tmp) {
		type = nla_type(iter);

		if (j >= SLSI_GSCAN_MAX_BUCKETS)
			break;

		switch (type) {
		case GSCAN_ATTRIBUTE_BASE_PERIOD:
			nl_gscan_param->base_period = nla_get_u32(iter);
			break;
		case GSCAN_ATTRIBUTE_NUM_AP_PER_SCAN:
			nl_gscan_param->max_ap_per_scan = nla_get_u32(iter);
			break;
		case GSCAN_ATTRIBUTE_REPORT_THRESHOLD:
			nl_gscan_param->report_threshold_percent = nla_get_u32(iter);
			break;
		case GSCAN_ATTRIBUTE_REPORT_THRESHOLD_NUM_SCANS:
			nl_gscan_param->report_threshold_num_scans = nla_get_u32(iter);
			break;
		case GSCAN_ATTRIBUTE_NUM_BUCKETS:
			nl_gscan_param->num_buckets = nla_get_u32(iter);
			break;
		case GSCAN_ATTRIBUTE_CH_BUCKET_1:
		case GSCAN_ATTRIBUTE_CH_BUCKET_2:
		case GSCAN_ATTRIBUTE_CH_BUCKET_3:
		case GSCAN_ATTRIBUTE_CH_BUCKET_4:
		case GSCAN_ATTRIBUTE_CH_BUCKET_5:
		case GSCAN_ATTRIBUTE_CH_BUCKET_6:
		case GSCAN_ATTRIBUTE_CH_BUCKET_7:
		case GSCAN_ATTRIBUTE_CH_BUCKET_8:
			nla_for_each_nested(iter1, iter, tmp1) {
				type = nla_type(iter1);
				nl_bucket = nl_gscan_param->nl_bucket;

				switch (type) {
				case GSCAN_ATTRIBUTE_BUCKET_ID:
					nl_bucket[j].bucket_index = nla_get_u32(iter1);
					break;
				case GSCAN_ATTRIBUTE_BUCKET_PERIOD:
					nl_bucket[j].period = nla_get_u32(iter1);
					break;
				case GSCAN_ATTRIBUTE_BUCKET_NUM_CHANNELS:
					nl_bucket[j].num_channels = nla_get_u32(iter1);
					break;
				case GSCAN_ATTRIBUTE_BUCKET_CHANNELS:
					nla_for_each_nested(iter2, iter1, tmp2) {
						nl_bucket[j].channels[k].channel = nla_get_u32(iter2);
						k++;
					}
					k = 0;
					break;
				case GSCAN_ATTRIBUTE_BUCKETS_BAND:
					nl_bucket[j].band = nla_get_u32(iter1);
					break;
				case GSCAN_ATTRIBUTE_REPORT_EVENTS:
					nl_bucket[j].report_events = nla_get_u32(iter1);
					break;
				case GSCAN_ATTRIBUTE_BUCKET_EXPONENT:
					nl_bucket[j].exponent = nla_get_u32(iter1);
					break;
				case GSCAN_ATTRIBUTE_BUCKET_STEP_COUNT:
					nl_bucket[j].step_count = nla_get_u32(iter1);
					break;
				case GSCAN_ATTRIBUTE_BUCKET_MAX_PERIOD:
					nl_bucket[j].max_period = nla_get_u32(iter1);
					break;
				default:
					SLSI_ERR_NODEV("No ATTR_BUKTS_type - %x\n", type);
					break;
				}
			}
			j++;
			break;
		default:
			SLSI_ERR_NODEV("No GSCAN_ATTR_CH_BUKT_type - %x\n", type);
			break;
		}
	}

	return 0;
}

int slsi_gscan_add_verify_params(struct slsi_nl_gscan_param *nl_gscan_param)
{
	int i;

	if ((nl_gscan_param->max_ap_per_scan < 0) || (nl_gscan_param->max_ap_per_scan > SLSI_GSCAN_MAX_AP_CACHE_PER_SCAN)) {
		SLSI_ERR_NODEV("Invalid max_ap_per_scan: %d\n", nl_gscan_param->max_ap_per_scan);
		return -EINVAL;
	}

	if ((nl_gscan_param->report_threshold_percent < 0) || (nl_gscan_param->report_threshold_percent > SLSI_GSCAN_MAX_SCAN_REPORTING_THRESHOLD)) {
		SLSI_ERR_NODEV("Invalid report_threshold_percent: %d\n", nl_gscan_param->report_threshold_percent);
		return -EINVAL;
	}

	if ((nl_gscan_param->num_buckets <= 0) || (nl_gscan_param->num_buckets > SLSI_GSCAN_MAX_BUCKETS)) {
		SLSI_ERR_NODEV("Invalid num_buckets: %d\n", nl_gscan_param->num_buckets);
		return -EINVAL;
	}

	for (i = 0; i < nl_gscan_param->num_buckets; i++) {
		if ((nl_gscan_param->nl_bucket[i].band == WIFI_BAND_UNSPECIFIED) && (nl_gscan_param->nl_bucket[i].num_channels == 0)) {
			SLSI_ERR_NODEV("No band/channels provided for gscan: band = %d, num_channel = %d\n",
				       nl_gscan_param->nl_bucket[i].band, nl_gscan_param->nl_bucket[i].num_channels);
			return -EINVAL;
		}

		if (nl_gscan_param->nl_bucket[i].report_events > 2) {
			SLSI_ERR_NODEV("Unsupported report event: report_event = %d\n", nl_gscan_param->nl_bucket[i].report_events);
			return -EINVAL;
		}
	}

	return 0;
}

void slsi_gscan_add_to_list(struct slsi_gscan **sdev_gscan, struct slsi_gscan *gscan)
{
	gscan->next = *sdev_gscan;
	*sdev_gscan = gscan;
}

int slsi_gscan_alloc_buckets(struct slsi_dev *sdev, struct slsi_gscan *gscan, int num_buckets)
{
	int i;
	int bucket_index = 0;
	int free_buckets = 0;

	for (i = 0; i < SLSI_GSCAN_MAX_BUCKETS; i++)
		if (!sdev->bucket[i].used)
			free_buckets++;

	if (num_buckets > free_buckets) {
		SLSI_ERR_NODEV("Not enough free buckets, num_buckets = %d, free_buckets = %d\n",
			       num_buckets, free_buckets);
		return -EINVAL;
	}

	/* Allocate free buckets for the current gscan */
	for (i = 0; i < SLSI_GSCAN_MAX_BUCKETS; i++)
		if (!sdev->bucket[i].used) {
			sdev->bucket[i].used = true;
			sdev->bucket[i].gscan = gscan;
			gscan->bucket[bucket_index] = &sdev->bucket[i];
			bucket_index++;
			if (bucket_index == num_buckets)
				break;
		}

	return 0;
}

static void slsi_gscan_free_buckets(struct slsi_gscan *gscan)
{
	struct slsi_bucket *bucket;
	int                i;

	SLSI_DBG1_NODEV(SLSI_GSCAN, "gscan = %p, num_buckets = %d\n", gscan, gscan->num_buckets);

	for (i = 0; i < gscan->num_buckets; i++) {
		bucket = gscan->bucket[i];

		SLSI_DBG2_NODEV(SLSI_GSCAN, "bucket = %p, used = %d, report_events = %d, scan_id = %#x, gscan = %p\n",
				bucket, bucket->used, bucket->report_events, bucket->scan_id, bucket->gscan);
		if (bucket->used) {
			bucket->used = false;
			bucket->report_events = 0;
			bucket->gscan = NULL;
		}
	}
}

void slsi_gscan_flush_scan_results(struct slsi_dev *sdev)
{
	struct netdev_vif        *ndev_vif;
	struct slsi_gscan_result *temp;
	int                      i;

	ndev_vif = slsi_gscan_get_vif(sdev);
	if (WARN_ON(!ndev_vif))
		return;

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	for (i = 0; i < SLSI_GSCAN_HASH_TABLE_SIZE; i++)
		while (sdev->gscan_hash_table[i]) {
			temp = sdev->gscan_hash_table[i];
			sdev->gscan_hash_table[i] = sdev->gscan_hash_table[i]->hnext;
			sdev->num_gscan_results--;
			sdev->buffer_consumed -= temp->scan_res_len;
			kfree(temp);
		}

	SLSI_DBG2(sdev, SLSI_GSCAN, "num_gscan_results: %d, buffer_consumed = %d\n",
		  sdev->num_gscan_results, sdev->buffer_consumed);

	WARN_ON(sdev->num_gscan_results != 0);
	WARN_ON(sdev->buffer_consumed != 0);

	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
}

void slsi_gscan_flush_hotlist_results(struct slsi_dev *sdev)
{
	struct slsi_hotlist_result *hotlist, *temp;
	struct netdev_vif          *ndev_vif;

	ndev_vif = slsi_gscan_get_vif(sdev);
	if (WARN_ON(!ndev_vif))
		return;

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	list_for_each_entry_safe(hotlist, temp, &sdev->hotlist_results, list) {
		list_del(&hotlist->list);
		kfree(hotlist);
	}

	INIT_LIST_HEAD(&sdev->hotlist_results);

	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
}

static int slsi_gscan_add_mlme(struct slsi_dev *sdev, struct slsi_nl_gscan_param *nl_gscan_param, struct slsi_gscan *gscan)
{
	struct slsi_gscan_param      gscan_param;
	struct net_device            *dev;
	int                          ret = 0;
	int                          i;

	dev = slsi_gscan_get_netdev(sdev);
	if (WARN_ON(!dev))
		return -EINVAL;

	for (i = 0; i < nl_gscan_param->num_buckets; i++) {
		u16 report_mode;

		gscan_param.nl_bucket = &nl_gscan_param->nl_bucket[i]; /* current bucket */
		gscan_param.bucket = gscan->bucket[i];

		switch (gscan_param.bucket->report_events) {
		case SLSI_REPORT_EVENTS_NONE:
		case SLSI_REPORT_EVENTS_EACH_SCAN:
		case SLSI_REPORT_EVENTS_FULL_RESULTS:
			report_mode = 1 << gscan_param.bucket->report_events;
			/* In case of epno no_batch mode should be set. */
			if (sdev->epno_active)
				report_mode |= FAPI_REPORTMODE_NO_BATCH;
			break;
		case SLSI_REPORT_EVENTS_NO_BATCH:
			report_mode = FAPI_REPORTMODE_NO_BATCH;
			break;
		default:
			SLSI_NET_ERR(dev, "Invalid report event value: %d\n", gscan_param.bucket->report_events);
			return -EINVAL;
		}

		ret = slsi_mlme_add_scan(sdev,
					 dev,
					 FAPI_SCANTYPE_GSCAN,
					 report_mode,
					 0,     /* n_ssids */
					 NULL,  /* ssids */
					 nl_gscan_param->nl_bucket[i].num_channels,
					 NULL,  /* ieee80211_channel */
					 &gscan_param,
					 NULL,  /* ies */
					 0,     /* ies_len */
					 false /* wait_for_ind */);

		if (ret != 0) {
			SLSI_NET_ERR(dev, "Failed to add bucket: %d\n", i);

			/* Delete the scan those are already added */
			for (i = (i - 1); i >= 0; i--)
				slsi_mlme_del_scan(sdev, dev, gscan->bucket[i]->scan_id, false);
			break;
		}
	}

	return ret;
}

static int slsi_gscan_add(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len)
{
	int                        ret = 0;
	struct slsi_dev            *sdev = SDEV_FROM_WIPHY(wiphy);
	struct slsi_nl_gscan_param *nl_gscan_param = NULL;
	struct slsi_gscan          *gscan;
	struct netdev_vif          *ndev_vif;
	int                        buffer_threshold;
	int                        i;

	SLSI_DBG1_NODEV(SLSI_GSCAN, "SUBCMD_ADD_GSCAN\n");

	if (WARN_ON(!sdev))
		return -EINVAL;

	if (!slsi_dev_gscan_supported())
		return -ENOTSUPP;

	ndev_vif = slsi_gscan_get_vif(sdev);

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	/* Allocate memory for the received scan params */
	nl_gscan_param = kzalloc(sizeof(*nl_gscan_param), GFP_KERNEL);
	if (nl_gscan_param == NULL) {
		SLSI_ERR_NODEV("Failed for allocate memory for gscan params\n");
		ret = -ENOMEM;
		goto exit;
	}

	slsi_gscan_add_read_params(nl_gscan_param, data, len);

#ifdef CONFIG_SCSC_WLAN_DEBUG
	slsi_gscan_add_dump_params(nl_gscan_param);
#endif

	ret = slsi_gscan_add_verify_params(nl_gscan_param);
	if (ret) {
		/* After adding a hotlist a new gscan is added with 0 buckets - return success */
		if (nl_gscan_param->num_buckets == 0) {
			kfree(nl_gscan_param);
			SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
			return 0;
		}

		goto exit;
	}

	/* Allocate Memory for the new gscan */
	gscan = kzalloc(sizeof(*gscan), GFP_KERNEL);
	if (gscan == NULL) {
		SLSI_ERR_NODEV("Failed to allocate memory for gscan\n");
		ret = -ENOMEM;
		goto exit;
	}

	gscan->num_buckets = nl_gscan_param->num_buckets;
	gscan->report_threshold_percent = nl_gscan_param->report_threshold_percent;
	gscan->report_threshold_num_scans = nl_gscan_param->report_threshold_num_scans;
	gscan->nl_bucket = nl_gscan_param->nl_bucket[0];

	/* If multiple gscan is added; consider the lowest report_threshold_percent */
	buffer_threshold = (SLSI_GSCAN_MAX_SCAN_CACHE_SIZE * nl_gscan_param->report_threshold_percent) / 100;
	if ((sdev->buffer_threshold == 0) || (buffer_threshold < sdev->buffer_threshold))
		sdev->buffer_threshold = buffer_threshold;

	ret = slsi_gscan_alloc_buckets(sdev, gscan, nl_gscan_param->num_buckets);
	if (ret)
		goto exit_with_gscan_free;

	for (i = 0; i < nl_gscan_param->num_buckets; i++)
		gscan->bucket[i]->report_events = nl_gscan_param->nl_bucket[i].report_events;

	ret = slsi_gscan_add_mlme(sdev, nl_gscan_param, gscan);
	if (ret) {
		/* Free the buckets */
		slsi_gscan_free_buckets(gscan);

		goto exit_with_gscan_free;
	}

	slsi_gscan_add_to_list(&sdev->gscan, gscan);

	goto exit;

exit_with_gscan_free:
	kfree(gscan);
exit:
	kfree(nl_gscan_param);

	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	return ret;
}

static int slsi_gscan_del(struct wiphy *wiphy,
			  struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device *dev;
	struct netdev_vif *ndev_vif;
	struct slsi_gscan *gscan;
	int               ret = 0;
	int               i;

	SLSI_DBG1_NODEV(SLSI_GSCAN, "SUBCMD_DEL_GSCAN\n");

	dev = slsi_gscan_get_netdev(sdev);
	if (WARN_ON(!dev))
		return -EINVAL;

	ndev_vif = netdev_priv(dev);

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	while (sdev->gscan != NULL) {
		gscan = sdev->gscan;

		SLSI_DBG3(sdev, SLSI_GSCAN, "gscan = %p, num_buckets = %d\n", gscan, gscan->num_buckets);

		for (i = 0; i < gscan->num_buckets; i++)
			if (gscan->bucket[i]->used)
				slsi_mlme_del_scan(sdev, dev, gscan->bucket[i]->scan_id, false);
		slsi_gscan_free_buckets(gscan);
		sdev->gscan = gscan->next;
		kfree(gscan);
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);

	slsi_gscan_flush_scan_results(sdev);

	sdev->buffer_threshold = 0;

	return ret;
}

static int slsi_gscan_get_scan_results(struct wiphy *wiphy,
				       struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev          *sdev = SDEV_FROM_WIPHY(wiphy);
	struct sk_buff           *skb;
	struct slsi_gscan_result *scan_res;
	struct nlattr            *scan_hdr;
	struct netdev_vif        *ndev_vif;
	int                      num_results = 0;
	int                      mem_needed;
	const struct nlattr      *attr;
	int                      nl_num_results = 0;
	int                      ret = 0;
	int                      temp;
	int                      type;
	int                      i;

	SLSI_DBG1_NODEV(SLSI_GSCAN, "SUBCMD_GET_SCAN_RESULTS\n");

	/* Read the number of scan results need to be given */
	nla_for_each_attr(attr, data, len, temp) {
		type = nla_type(attr);

		switch (type) {
		case GSCAN_ATTRIBUTE_NUM_OF_RESULTS:
			nl_num_results = nla_get_u32(attr);
			break;
		default:
			SLSI_ERR_NODEV("Unknown attribute: %d\n", type);
			break;
		}
	}

	ndev_vif = slsi_gscan_get_vif(sdev);
	if (WARN_ON(!ndev_vif))
		return -EINVAL;

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	num_results = sdev->num_gscan_results;

	SLSI_DBG3(sdev, SLSI_GSCAN, "nl_num_results: %d, num_results = %d\n", nl_num_results, sdev->num_gscan_results);

	if (num_results == 0) {
		SLSI_DBG1(sdev, SLSI_GSCAN, "No scan results available\n");
		/* Return value should be 0 for this scenario */
		goto exit;
	}

	/* Find the number of results to return */
	if (num_results > nl_num_results)
		num_results = nl_num_results;

	/* 12 bytes additional for scan_id, flags and num_resuls */
	mem_needed = num_results * sizeof(struct slsi_nl_scan_result_param) + 12;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, mem_needed);
	if (skb == NULL) {
		SLSI_ERR_NODEV("skb alloc failed");
		ret = -ENOMEM;
		goto exit;
	}

	scan_hdr = nla_nest_start(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS);
	if (scan_hdr == NULL) {
		kfree_skb(skb);
		SLSI_ERR_NODEV("scan_hdr is NULL.\n");
		ret = -ENOMEM;
		goto exit;
	}

	nla_put_u32(skb, GSCAN_ATTRIBUTE_SCAN_ID, 0);
	nla_put_u8(skb, GSCAN_ATTRIBUTE_SCAN_FLAGS, 0);
	nla_put_u32(skb, GSCAN_ATTRIBUTE_NUM_OF_RESULTS, num_results);

	for (i = 0; i < SLSI_GSCAN_HASH_TABLE_SIZE; i++)
		while (sdev->gscan_hash_table[i]) {
			scan_res = sdev->gscan_hash_table[i];
			sdev->gscan_hash_table[i] = sdev->gscan_hash_table[i]->hnext;
			sdev->num_gscan_results--;
			sdev->buffer_consumed -= scan_res->scan_res_len;
			/* TODO: If IE is included then HAL is not able to parse the results */
			nla_put(skb, GSCAN_ATTRIBUTE_SCAN_RESULTS, sizeof(struct slsi_nl_scan_result_param), &scan_res->nl_scan_res);
			kfree(scan_res);
			num_results--;
			if (num_results == 0)
				goto out;
		}
out:
	nla_nest_end(skb, scan_hdr);

	ret = cfg80211_vendor_cmd_reply(skb);
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	return ret;
}

static int slsi_gscan_set_hotlist_read_params(struct slsi_nl_hotlist_param *nl_hotlist_param, const void *data, int len)
{
	int                               tmp, tmp1, tmp2, type, j = 0;
	const struct nlattr               *outer, *inner, *iter;
	struct slsi_nl_ap_threshold_param *nl_ap;
	int                               flush;

	nla_for_each_attr(iter, data, len, tmp2) {
		type = nla_type(iter);
		switch (type) {
		case GSCAN_ATTRIBUTE_HOTLIST_BSSIDS:
			nla_for_each_nested(outer, iter, tmp) {
				nl_ap = &nl_hotlist_param->ap[j];
				nla_for_each_nested(inner, outer, tmp1) {
					type = nla_type(inner);
					switch (type) {
					case GSCAN_ATTRIBUTE_BSSID:
						SLSI_ETHER_COPY(&nl_ap->bssid[0], nla_data(inner));
						break;
					case GSCAN_ATTRIBUTE_RSSI_LOW:
						nl_ap->low = nla_get_s8(inner);
						break;
					case GSCAN_ATTRIBUTE_RSSI_HIGH:
						nl_ap->high = nla_get_s8(inner);
						break;
					default:
						SLSI_ERR_NODEV("Unknown type %d\n", type);
						break;
					}
				}
				j++;
			}
			nl_hotlist_param->num_bssid = j;
			break;
		case GSCAN_ATTRIBUTE_HOTLIST_FLUSH:
			flush = nla_get_u8(iter);
			break;
		case GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE:
			nl_hotlist_param->lost_ap_sample_size = nla_get_u32(iter);
			break;
		default:
			SLSI_ERR_NODEV("No ATTRIBUTE_HOTLIST - %d\n", type);
			break;
		}
	}

	return 0;
}

static int slsi_gscan_set_hotlist(struct wiphy *wiphy,
				  struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev              *sdev = SDEV_FROM_WIPHY(wiphy);
	struct slsi_nl_hotlist_param *nl_hotlist_param;
	struct net_device            *dev;
	struct netdev_vif            *ndev_vif;
	int                          ret = 0;

	SLSI_DBG1_NODEV(SLSI_GSCAN, "SUBCMD_SET_BSSID_HOTLIST\n");

	dev = slsi_gscan_get_netdev(sdev);
	if (WARN_ON(!dev))
		return -EINVAL;

	ndev_vif = netdev_priv(dev);

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	/* Allocate memory for the received scan params */
	nl_hotlist_param = kzalloc(sizeof(*nl_hotlist_param), GFP_KERNEL);
	if (nl_hotlist_param == NULL) {
		SLSI_ERR_NODEV("Failed for allocate memory for gscan hotlist_param\n");
		ret = -ENOMEM;
		goto exit;
	}

	slsi_gscan_set_hotlist_read_params(nl_hotlist_param, data, len);

#ifdef CONFIG_SCSC_WLAN_DEBUG
	slsi_gscan_set_hotlist_dump_params(nl_hotlist_param);
#endif
	ret = slsi_mlme_set_bssid_hotlist_req(sdev, dev, nl_hotlist_param);
	if (ret)
		SLSI_ERR_NODEV("Failed to set hostlist\n");

	kfree(nl_hotlist_param);
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
exit:
	return ret;
}

static int slsi_gscan_reset_hotlist(struct wiphy *wiphy,
				    struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev   *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device *dev;
	int               ret = 0;
	struct netdev_vif *ndev_vif;

	SLSI_DBG1_NODEV(SLSI_GSCAN, "SUBCMD_RESET_BSSID_HOTLIST\n");

	dev = slsi_gscan_get_netdev(sdev);
	if (WARN_ON(!dev))
		return -EINVAL;
	ndev_vif = netdev_priv(dev);

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	ret = slsi_mlme_set_bssid_hotlist_req(sdev, dev, NULL);
	if (ret)
		SLSI_ERR_NODEV("Failed to reset hostlist\n");

	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);

	slsi_gscan_flush_hotlist_results(sdev);

	return ret;
}

static struct slsi_gscan *slsi_mlme_get_tracking_scan_id(struct slsi_dev                          *sdev,
							 struct slsi_nl_significant_change_params *significant_param_ptr)
{
	/* If channel hint is not in significant change req, link to previous gscan, else new gscan */
	struct slsi_gscan *ret_gscan;

	if (sdev->gscan != NULL) {
		ret_gscan = sdev->gscan;
		SLSI_DBG3(sdev, SLSI_GSCAN, "Existing Scan for tracking\n");
	} else {
		struct slsi_gscan *gscan;
		/* Allocate Memory for the new gscan */
		gscan = kzalloc(sizeof(*gscan), GFP_KERNEL);
		if (gscan == NULL) {
			SLSI_ERR(sdev, "Failed to allocate memory for gscan\n");
			return NULL;
		}
		gscan->num_buckets = 1;
		if (slsi_gscan_alloc_buckets(sdev, gscan, gscan->num_buckets) != 0) {
			SLSI_ERR(sdev, "NO free buckets. Abort tracking\n");
			kfree(gscan);
			return NULL;
		}
		/*Build nl_bucket based on channels in significant_param_ptr->ap array*/
		gscan->nl_bucket.band = WIFI_BAND_UNSPECIFIED;
		gscan->nl_bucket.num_channels = 0;
		gscan->nl_bucket.period = 5 * 1000; /* Default */
		slsi_gscan_add_to_list(&sdev->gscan, gscan);
		ret_gscan = gscan;
		SLSI_DBG3(sdev, SLSI_GSCAN, "New Scan for tracking\n");
	}
	SLSI_DBG3(sdev, SLSI_GSCAN, "tracking channel num:%d\n", ret_gscan->nl_bucket.num_channels);
	return ret_gscan;
}

static int slsi_gscan_set_significant_change(struct wiphy *wiphy,
					     struct wireless_dev *wdev, const void *data, int len)
{
	int                                      ret = 0;
	struct slsi_dev                          *sdev = SDEV_FROM_WIPHY(wiphy);
	struct slsi_nl_significant_change_params *significant_change_param;
	u8                                       bss_count = 0;
	struct slsi_nl_ap_threshold_param        *bss_param_ptr;
	int                                      tmp, tmp1, tmp2, type;
	const struct nlattr                      *outer, *inner, *iter;
	struct net_device                        *net_dev;
	struct slsi_gscan                        *gscan;
	struct netdev_vif                        *ndev_vif;

	SLSI_DBG3(sdev, SLSI_GSCAN, "SUBCMD_SET_SIGNIFICANT_CHANGE Received\n");

	significant_change_param = kmalloc(sizeof(*significant_change_param), GFP_KERNEL);
	if (!significant_change_param) {
		SLSI_ERR(sdev, "NO mem for significant_change_param\n");
		return -ENOMEM;
	}
	memset(significant_change_param, 0, sizeof(struct slsi_nl_significant_change_params));
	nla_for_each_attr(iter, data, len, tmp2) {
		type = nla_type(iter);
		switch (type) {
		case GSCAN_ATTRIBUTE_RSSI_SAMPLE_SIZE:
			significant_change_param->rssi_sample_size = nla_get_u16(iter);
			SLSI_DBG3(sdev, SLSI_GSCAN, "rssi_sample_size %d\n", significant_change_param->rssi_sample_size);
			break;
		case GSCAN_ATTRIBUTE_LOST_AP_SAMPLE_SIZE:
			significant_change_param->lost_ap_sample_size = nla_get_u16(iter);
			SLSI_DBG3(sdev, SLSI_GSCAN, "lost_ap_sample_size %d\n", significant_change_param->lost_ap_sample_size);
			break;
		case GSCAN_ATTRIBUTE_MIN_BREACHING:
			significant_change_param->min_breaching = nla_get_u16(iter);
			SLSI_DBG3(sdev, SLSI_GSCAN, "min_breaching %d\n", significant_change_param->min_breaching);
			break;
		case GSCAN_ATTRIBUTE_SIGNIFICANT_CHANGE_BSSIDS:
			nla_for_each_nested(outer, iter, tmp) {
				bss_param_ptr = &significant_change_param->ap[bss_count];
				bss_count++;
				SLSI_DBG3(sdev, SLSI_GSCAN, "bssids[%d]:\n", bss_count);
				if (bss_count == SLSI_GSCAN_MAX_SIGNIFICANT_CHANGE_APS) {
					SLSI_ERR(sdev, "Can support max:%d aps. Skipping excess\n", SLSI_GSCAN_MAX_SIGNIFICANT_CHANGE_APS);
					break;
				}
				nla_for_each_nested(inner, outer, tmp1) {
					switch (nla_type(inner)) {
					case GSCAN_ATTRIBUTE_BSSID:
						SLSI_ETHER_COPY(&bss_param_ptr->bssid[0], nla_data(inner));
						SLSI_DBG3(sdev, SLSI_GSCAN, "\tbssid %pM\n", bss_param_ptr->bssid);
						break;
					case GSCAN_ATTRIBUTE_RSSI_HIGH:
						bss_param_ptr->high = nla_get_s8(inner);
						SLSI_DBG3(sdev, SLSI_GSCAN, "\thigh %d\n", bss_param_ptr->high);
						break;
					case GSCAN_ATTRIBUTE_RSSI_LOW:
						bss_param_ptr->low = nla_get_s8(inner);
						SLSI_DBG3(sdev, SLSI_GSCAN, "\tlow %d\n", bss_param_ptr->low);
						break;
					default:
						SLSI_ERR(sdev, "unknown attribute:%d\n", type);
						break;
					}
				}
			}
			break;
		default:
			SLSI_ERR(sdev, "Unknown type:%d\n", type);
			break;
		}
	}
	significant_change_param->num_bssid = bss_count;
	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(net_dev);

	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	gscan = slsi_mlme_get_tracking_scan_id(sdev, significant_change_param);
	if (gscan) {
		if (slsi_mlme_significant_change_set(sdev, net_dev, significant_change_param)) {
			SLSI_ERR(sdev, "Could not set GSCAN significant cfg\n");
			ret = -EINVAL;
		}
	} else {
		ret = -ENOMEM;
	}
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	kfree(significant_change_param);
	return ret;
}

void slsi_rx_significant_change_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u32               eventdata_len;
	u16               bssid_count = fapi_get_buff(skb, u.mlme_significant_change_ind.number_of_results);
	u16               rssi_entry_count = fapi_get_buff(skb, u.mlme_significant_change_ind.number_of_rssi_entries);
	u32               i, j;
	u8                *eventdata  = NULL;
	u8                *op_ptr, *ip_ptr;
	u16               *le16_ptr;

	/* convert fapi buffer to wifi-hal structure
	 *   fapi buffer: [mac address 6 bytes] [chan_freq 2 bytes]
	 *                [riis history N*2 bytes]
	 *   wifi-hal structure
	 *      typedef struct {
	 *              uint16_t channel;
	 *              mac_addr bssid;
	 *              short rssi_history[8];
	 *      } ChangeInfo;
	 */

	SLSI_DBG3(sdev, SLSI_GSCAN, "No BSSIDs:%d\n", bssid_count);

	eventdata_len = (8 + (8 * 2)) * bssid_count; /* see wifi-hal structure in above comments */
	eventdata = kmalloc(eventdata_len, GFP_KERNEL);
	if (!eventdata) {
		SLSI_ERR(sdev, "no mem for event data\n");
		slsi_kfree_skb(skb);
		return;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	op_ptr = eventdata;
	ip_ptr = fapi_get_data(skb);
	for (i = 0; i < bssid_count; i++) {
		le16_ptr = (u16 *)&ip_ptr[6]; /* channel: required unit MHz. received unit 512KHz */
		*(u16 *)op_ptr = le16_to_cpu(*le16_ptr) / 2;
		SLSI_DBG3(sdev, SLSI_GSCAN, "[%d] channel:%d\n", i, *(u16 *)op_ptr);
		op_ptr += 2;

		SLSI_ETHER_COPY(op_ptr, ip_ptr); /* mac_addr */
		SLSI_DBG3(sdev, SLSI_GSCAN, "[%d] mac:%pM\n", i, op_ptr);
		op_ptr += ETH_ALEN;

		for (j = 0; j < 8; j++) {
			if (j < rssi_entry_count) {
				*op_ptr = ip_ptr[8 + j * 2];
				*(op_ptr + 1) = ip_ptr[9 + j * 2];
			} else {
				s16 invalid_rssi = SLSI_GSCAN_INVALID_RSSI;
				*(u16 *)op_ptr = invalid_rssi;
			}
			op_ptr += 2;
		}
		ip_ptr += 8 + (rssi_entry_count * 2);
	}
	SLSI_DBG_HEX(sdev, SLSI_GSCAN, eventdata, eventdata_len, "significant change event buffer:\n");
	SLSI_DBG_HEX(sdev, SLSI_GSCAN, fapi_get_data(skb), fapi_get_datalen(skb), "significant change skb buffer:\n");
	slsi_vendor_event(sdev, SLSI_NL80211_SIGNIFICANT_CHANGE_EVENT, eventdata, eventdata_len);
	kfree(eventdata);
	slsi_kfree_skb(skb);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

void slsi_rx_rssi_report_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *skb)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_rssi_monitor_evt event_data;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	SLSI_ETHER_COPY(event_data.bssid , fapi_get_buff(skb, u.mlme_rssi_report_ind.bssid));
	event_data.rssi = fapi_get_s16(skb, u.mlme_rssi_report_ind.rssi);
	SLSI_DBG3(sdev, SLSI_GSCAN, "RSSI threshold breached, Current RSSI for %pM= %d\n", event_data.bssid, event_data.rssi);
	slsi_vendor_event(sdev, SLSI_NL80211_RSSI_REPORT_EVENT, &event_data, sizeof(event_data));
	slsi_kfree_skb(skb);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

static int slsi_gscan_reset_significant_change(struct wiphy *wiphy,
					       struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev    *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device  *net_dev;
	struct netdev_vif  *ndev_vif;
	struct slsi_bucket *bucket = NULL;
	u32                i;

	SLSI_DBG3(sdev, SLSI_GSCAN, "SUBCMD_RESET_SIGNIFICANT_CHANGE Received\n");

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(net_dev);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	for (i = 0; i < SLSI_GSCAN_MAX_BUCKETS; i++) {
		bucket = &sdev->bucket[i];
		if (!bucket->used || !bucket->for_change_tracking)
			continue;

		(void)slsi_mlme_del_scan(sdev, net_dev, bucket->scan_id, false);
		bucket->for_change_tracking = false;
		bucket->used = false;
		SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
		return 0;
	}
	SLSI_DBG3(sdev, SLSI_GSCAN, "Significant change scan not found\n");
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);

	return 0;
}

#ifdef CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD
static int slsi_key_mgmt_set_pmk(struct wiphy *wiphy,
				 struct wireless_dev *wdev, const void *pmk, int pmklen)
{
	struct slsi_dev    *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device  *net_dev;
	struct netdev_vif  *ndev_vif;
	int r = 0;

	if (wdev->iftype == NL80211_IFTYPE_P2P_CLIENT) {
		SLSI_DBG3(sdev, SLSI_GSCAN, "Not required to set PMK for P2P client\n");
		return r;
	}
	SLSI_DBG3(sdev, SLSI_GSCAN, "SUBCMD_SET_PMK Received\n");

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(net_dev);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	r = slsi_mlme_set_pmk(sdev, net_dev, pmk, (u16)pmklen);

	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}
#endif

static int slsi_set_bssid_blacklist(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev    *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device  *net_dev;
	struct netdev_vif *ndev_vif;
	int                      temp1;
	int                      type;
	const struct nlattr      *attr;
	u32 num_bssids = 0;
	u8 i = 0;
	int ret;
	u8 *bssid = NULL;
	struct cfg80211_acl_data *acl_data = NULL;

	SLSI_DBG1_NODEV(SLSI_GSCAN, "SUBCMD_SET_BSSID_BLACK_LIST\n");

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	if (WARN_ON(!net_dev))
		return -EINVAL;

	ndev_vif = netdev_priv(net_dev);
	/*This subcmd can be issued in either connected or disconnected state.
	  * Hence using scan_mutex and not vif_mutex
	  */
	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);
	nla_for_each_attr(attr, data, len, temp1) {
		type = nla_type(attr);

		switch (type) {
		case GSCAN_ATTRIBUTE_NUM_BSSID:
			num_bssids = nla_get_u32(attr);
			acl_data = kmalloc(sizeof(*acl_data) + (sizeof(struct mac_address) * num_bssids) , GFP_KERNEL);
			if (acl_data == NULL) {
				ret = -ENOMEM;
				goto exit;
			}
			acl_data->n_acl_entries = num_bssids;
			break;

		case GSCAN_ATTRIBUTE_BLACKLIST_BSSID:
			if (!acl_data) {
				ret = -EINVAL;
				goto exit;
			}
			bssid = (u8 *)nla_data(attr);
			SLSI_ETHER_COPY(acl_data->mac_addrs[i].addr, bssid);
			SLSI_DBG3_NODEV(SLSI_GSCAN, "mac_addrs[%d]:%pM)\n", i, acl_data->mac_addrs[i].addr);
			i++;
			break;
		default:
			SLSI_ERR_NODEV("Unknown attribute: %d\n", type);
			ret = -EINVAL;
			goto exit;
		}
	}

	if (acl_data != NULL) {
		acl_data->acl_policy = FAPI_ACLPOLICY_BLACKLIST;
		ret = slsi_mlme_set_acl(sdev, net_dev, 0, acl_data);
		if (ret)
			SLSI_ERR_NODEV("Failed to set bssid blacklist\n");
	} else {
		ret =  -EINVAL;
	}
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
	kfree(acl_data);
	return ret;
}

static int slsi_start_keepalive_offload(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev    *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device  *net_dev;
	struct netdev_vif *ndev_vif;

	int                      temp;
	int                      type;
	const struct nlattr      *attr;
	u16 ip_pkt_len = 0;
	u8 *ip_pkt = NULL, *src_mac_addr = NULL, *dst_mac_addr = NULL;
	u32 period = 0;
	struct slsi_peer *peer;
	struct sk_buff *skb;
	struct ethhdr *ehdr;
	int r = 0;
	u16 host_tag = 0;
	u8 index = 0;

	SLSI_DBG3(sdev, SLSI_MLME, "SUBCMD_START_KEEPALIVE_OFFLOAD received\n");

#ifndef CONFIG_SCSC_NAT_KEEPALIVE
	SLSI_DBG3(sdev, SLSI_MLME, "Returning Not Supported, as the NAT Keep Alive Feature is disabled as of now.\n");
	return -EOPNOTSUPP;
#endif

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(net_dev);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (WARN_ON(!ndev_vif->activated)) {
		r = -EINVAL;
		goto exit;
	}
	if (WARN_ON(ndev_vif->vif_type != FAPI_VIFTYPE_STATION)) {
		r = -EINVAL;
		goto exit;
	}
	if (WARN_ON(ndev_vif->sta.vif_status != SLSI_VIF_STATUS_CONNECTED)) {
		r = -EINVAL;
		goto exit;
	}

	peer = slsi_get_peer_from_qs(sdev, net_dev, SLSI_STA_PEER_QUEUESET);
	if (WARN_ON(!peer)) {
		r = -EINVAL;
		goto exit;
	}

	nla_for_each_attr(attr, data, len, temp) {
		type = nla_type(attr);

		switch (type) {
		case MKEEP_ALIVE_ATTRIBUTE_IP_PKT_LEN:
			ip_pkt_len = nla_get_u16(attr);
			break;

		case MKEEP_ALIVE_ATTRIBUTE_IP_PKT:
			ip_pkt = (u8 *)nla_data(attr);
			break;

		case MKEEP_ALIVE_ATTRIBUTE_PERIOD_MSEC:
			period = nla_get_u32(attr);
			break;

		case MKEEP_ALIVE_ATTRIBUTE_DST_MAC_ADDR:
			dst_mac_addr = (u8 *)nla_data(attr);
			break;

		case MKEEP_ALIVE_ATTRIBUTE_SRC_MAC_ADDR:
			src_mac_addr = (u8 *)nla_data(attr);
			break;

		case MKEEP_ALIVE_ATTRIBUTE_ID:
			index = nla_get_u8(attr);
			break;

		default:
			SLSI_ERR_NODEV("Unknown attribute: %d\n", type);
			r = -EINVAL;
			goto exit;
		}
	}

	/*stop any existing request. This may fail if no request exists so ignore the return value*/
	slsi_mlme_send_frame_mgmt(sdev, net_dev, NULL, 0, FAPI_DATAUNITDESCRIPTOR_IEEE802_3_FRAME,
				  FAPI_MESSAGETYPE_PERIODIC_OFFLOAD,
				  ndev_vif->sta.keepalive_host_tag[index - 1], 0, 0, 0);

	skb = slsi_alloc_skb(sizeof(struct ethhdr) + ip_pkt_len, GFP_KERNEL);
	if (WARN_ON(!skb)) {
		r = -ENOMEM;
		goto exit;
	}

	skb_reset_mac_header(skb);

	/* Ethernet Header */
	ehdr = (struct ethhdr *)skb_put(skb, sizeof(struct ethhdr));

	if (dst_mac_addr)
		SLSI_ETHER_COPY(ehdr->h_dest, dst_mac_addr);
	if (src_mac_addr)
		SLSI_ETHER_COPY(ehdr->h_source, src_mac_addr);
	ehdr->h_proto = cpu_to_be16(ETH_P_IP);
	memcpy(skb_put(skb, ip_pkt_len), ip_pkt, ip_pkt_len);

	skb->dev = net_dev;
	skb->protocol = ETH_P_IP;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb->queue_mapping = slsi_netif_get_peer_queue(0, 0); /* Queueset 0 AC 0 */
	host_tag = slsi_tx_mgmt_host_tag(sdev);
	r = slsi_mlme_send_frame_data(sdev, net_dev, skb, host_tag, FAPI_MESSAGETYPE_PERIODIC_OFFLOAD, (period * 1000));
	if (r == 0)
		ndev_vif->sta.keepalive_host_tag[index - 1] = host_tag;
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

static int slsi_stop_keepalive_offload(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev    *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device  *net_dev;
	struct netdev_vif *ndev_vif;
	int r = 0;
	int                      temp;
	int                      type;
	const struct nlattr      *attr;
	u8 index = 0;

	SLSI_DBG3(sdev, SLSI_MLME, "SUBCMD_STOP_KEEPALIVE_OFFLOAD received\n");

#ifndef CONFIG_SCSC_NAT_KEEPALIVE
	SLSI_DBG3(sdev, SLSI_MLME, "Returning Not Supported, as the NAT Keep Alive Feature is disabled as of now.\n");
	return -EOPNOTSUPP;
#endif

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(net_dev);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	if (WARN_ON(!ndev_vif->activated)) {
		r = -EINVAL;
		goto exit;
	}
	if (WARN_ON(ndev_vif->vif_type != FAPI_VIFTYPE_STATION)) {
		r = -EINVAL;
		goto exit;
	}
	if (WARN_ON(ndev_vif->sta.vif_status != SLSI_VIF_STATUS_CONNECTED)) {
		r = -EINVAL;
		goto exit;
	}

	nla_for_each_attr(attr, data, len, temp) {
		type = nla_type(attr);

		switch (type) {
		case MKEEP_ALIVE_ATTRIBUTE_ID:
			index = nla_get_u8(attr);
			break;

		default:
			SLSI_ERR_NODEV("Unknown attribute: %d\n", type);
			r = -EINVAL;
			goto exit;
		}
	}

	r = slsi_mlme_send_frame_mgmt(sdev, net_dev, NULL, 0, FAPI_DATAUNITDESCRIPTOR_IEEE802_3_FRAME,
				      FAPI_MESSAGETYPE_PERIODIC_OFFLOAD, ndev_vif->sta.keepalive_host_tag[index-1], 0, 0, 0);
	ndev_vif->sta.keepalive_host_tag[index - 1] = 0;

exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

static inline int slsi_epno_ssid_list_get(struct slsi_dev *sdev,
					  struct slsi_epno_ssid_param *epno_params, const struct nlattr *outer)
{
	int type, tmp;
	u8  epno_auth, len;
	const struct nlattr *inner;

	nla_for_each_nested(inner, outer, tmp) {
		type = nla_type(inner);
		switch (type) {
		case SLSI_ATTRIBUTE_EPNO_SSID:
			memcpy(epno_params->ssid, nla_data(inner), 32);
			break;
		case SLSI_ATTRIBUTE_EPNO_SSID_LEN:
			len = nla_get_u8(inner);
			if (len <= 32) {
				epno_params->ssid_len = len;
			} else {
				SLSI_ERR(sdev, "SSID too long %d\n", len);
				return -EINVAL;
			}
			break;
		case SLSI_ATTRIBUTE_EPNO_RSSI:
			epno_params->rssi_thresh = nla_get_s8(inner);
			break;
		case SLSI_ATTRIBUTE_EPNO_FLAGS:
			epno_params->flags |= nla_get_u8(inner);
			break;
		case SLSI_ATTRIBUTE_EPNO_AUTH:
			epno_auth = nla_get_u8(inner);
			if (epno_auth & SLSI_EPNO_AUTH_FIELD_WEP_OPEN)
				epno_params->flags |= FAPI_EPNOPOLICY_AUTH_OPEN;
			else if (epno_auth & SLSI_EPNO_AUTH_FIELD_WPA_PSK)
				epno_params->flags |= FAPI_EPNOPOLICY_AUTH_PSK;
			else if (epno_auth & SLSI_EPNO_AUTH_FIELD_WPA_EAP)
				epno_params->flags |= FAPI_EPNOPOLICY_AUTH_EAPOL;
			break;
		default:
			SLSI_WARN(sdev, "Ignoring unknown type:%d\n", type);
		}
	}
	return 0;
}

static int slsi_set_epno_ssid(struct wiphy *wiphy,
			      struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev             *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device           *net_dev;
	struct netdev_vif           *ndev_vif;
	int                         r = 0;
	int                         tmp, tmp1, type, num = 0;
	const struct nlattr         *outer, *iter;
	u8                          i = 0;
	struct slsi_epno_ssid_param *epno_param_array;
	struct slsi_epno_ssid_param *epno_params;

	SLSI_DBG3(sdev, SLSI_GSCAN, "SUBCMD_SET_EPNO_LIST Received\n");

	if (!slsi_dev_epno_supported())
		return -EOPNOTSUPP;

	epno_param_array = kmalloc(sizeof(*epno_param_array) * SLSI_GSCAN_MAX_EPNO_SSIDS, GFP_KERNEL);
	if (!epno_param_array) {
		SLSI_ERR(sdev, "Mem alloc fail\n");
		return -ENOMEM;
	}

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(net_dev);

	nla_for_each_attr(iter, data, len, tmp1) {
		type = nla_type(iter);
		switch (type) {
		case SLSI_ATTRIBUTE_EPNO_SSID_LIST:
			nla_for_each_nested(outer, iter, tmp) {
				epno_params = &epno_param_array[i];
				epno_params->flags = 0;
				i++;
				r = slsi_epno_ssid_list_get(sdev, epno_params, outer);
				if (r)
					goto exit;
			}
			break;
		case SLSI_ATTRIBUTE_EPNO_SSID_NUM:
			num = nla_get_u8(iter);
			if (num > SLSI_GSCAN_MAX_EPNO_SSIDS) {
				SLSI_ERR(sdev, "Cannot support %d SSIDs. max %d\n", num, SLSI_GSCAN_MAX_EPNO_SSIDS);
				r = -EINVAL;
				goto exit;
			}
			break;
		default:
			SLSI_ERR(sdev, "Invalid attribute %d\n", type);
			r = -EINVAL;
			goto exit;
		}
	}
	if (i != num) {
		SLSI_ERR(sdev, "num_ssid %d does not match ssids sent %d\n", num, i);
		r = -EINVAL;
		goto exit;
	}
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	r = slsi_mlme_set_pno_list(sdev, num, epno_param_array, NULL);
	if (r == 0)
		sdev->epno_active = (num != 0);
	else
		sdev->epno_active = false;
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
exit:
	kfree(epno_param_array);
	return r;
}

static int slsi_set_hs_params(struct wiphy *wiphy,
			      struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev            *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device          *net_dev;
	struct netdev_vif          *ndev_vif;
	int                        r = 0;
	int                        tmp, tmp1, tmp2, type, num = 0;
	const struct nlattr        *outer, *inner, *iter;
	u8                         i = 0;
	struct slsi_epno_hs2_param *epno_hs2_params_array;
	struct slsi_epno_hs2_param *epno_hs2_params;

	SLSI_DBG3(sdev, SLSI_GSCAN, "SUBCMD_SET_HS_LIST Received\n");

	if (!slsi_dev_epno_supported())
		return -EOPNOTSUPP;

	epno_hs2_params_array = kmalloc(sizeof(*epno_hs2_params_array) * SLSI_GSCAN_MAX_EPNO_HS2_PARAM, GFP_KERNEL);
	if (!epno_hs2_params_array) {
		SLSI_ERR(sdev, "Mem alloc fail\n");
		return -ENOMEM;
	}

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(net_dev);

	nla_for_each_attr(iter, data, len, tmp2) {
		type = nla_type(iter);
		switch (type) {
		case SLSI_ATTRIBUTE_EPNO_HS_PARAM_LIST:
			nla_for_each_nested(outer, iter, tmp) {
				epno_hs2_params = &epno_hs2_params_array[i];
				i++;
				nla_for_each_nested(inner, outer, tmp1) {
					type = nla_type(inner);

					switch (type) {
					case SLSI_ATTRIBUTE_EPNO_HS_ID:
						epno_hs2_params->id = (u32)nla_get_u32(inner);
						break;
					case SLSI_ATTRIBUTE_EPNO_HS_REALM:
						memcpy(epno_hs2_params->realm, nla_data(inner), 256);
						break;
					case SLSI_ATTRIBUTE_EPNO_HS_CONSORTIUM_IDS:
						memcpy(epno_hs2_params->roaming_consortium_ids, nla_data(inner), 16 * 8);
						break;
					case SLSI_ATTRIBUTE_EPNO_HS_PLMN:
						memcpy(epno_hs2_params->plmn, nla_data(inner), 3);
						break;
					default:
						SLSI_WARN(sdev, "Ignoring unknown type:%d\n", type);
					}
				}
			}
			break;
		case SLSI_ATTRIBUTE_EPNO_HS_NUM:
			num = nla_get_u8(iter);
			if (num > SLSI_GSCAN_MAX_EPNO_HS2_PARAM) {
				SLSI_ERR(sdev, "Cannot support %d SSIDs. max %d\n", num, SLSI_GSCAN_MAX_EPNO_SSIDS);
				r = -EINVAL;
				goto exit;
			}
			break;
		default:
			SLSI_ERR(sdev, "Invalid attribute %d\n", type);
			r = -EINVAL;
			goto exit;
		}
	}
	if (i != num) {
		SLSI_ERR(sdev, "num_ssid %d does not match ssids sent %d\n", num, i);
		r = -EINVAL;
		goto exit;
	}

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	r = slsi_mlme_set_pno_list(sdev, num, NULL, epno_hs2_params_array);
	if (r == 0)
		sdev->epno_active = true;
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
exit:
	kfree(epno_hs2_params_array);
	return r;
}

static int slsi_reset_hs_params(struct wiphy *wiphy,
				struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev    *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device  *net_dev;
	struct netdev_vif  *ndev_vif;
	int                r;

	SLSI_DBG3(sdev, SLSI_GSCAN, "SUBCMD_RESET_HS_LIST Received\n");

	if (!slsi_dev_epno_supported())
		return -EOPNOTSUPP;

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	ndev_vif = netdev_priv(net_dev);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	r = slsi_mlme_set_pno_list(sdev, 0, NULL, NULL);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	sdev->epno_active = false;
	return r;
}

static int slsi_set_rssi_monitor(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev            *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device          *net_dev;
	struct netdev_vif          *ndev_vif;
	int                        r = 0;
	int                      temp;
	int                      type;
	const struct nlattr      *attr;
	s8 min_rssi = 0, max_rssi = 0;
	u16 enable = 0;

	SLSI_DBG3(sdev, SLSI_GSCAN, "Recd RSSI monitor command\n");

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	if (net_dev == NULL) {
		SLSI_ERR(sdev, "netdev is NULL!!\n");
		return -ENODEV;
	}

	ndev_vif = netdev_priv(net_dev);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (!ndev_vif->activated) {
		SLSI_ERR(sdev, "Vif not activated\n");
		r = -EINVAL;
		goto exit;
	}
	if (ndev_vif->vif_type != FAPI_VIFTYPE_STATION) {
		SLSI_ERR(sdev, "Not a STA vif\n");
		r = -EINVAL;
		goto exit;
	}
	if (ndev_vif->sta.vif_status != SLSI_VIF_STATUS_CONNECTED) {
		SLSI_ERR(sdev, "STA vif not connected\n");
		r = -EINVAL;
		goto exit;
	}

	nla_for_each_attr(attr, data, len, temp) {
		type = nla_type(attr);
		switch (type) {
		case SLSI_RSSI_MONITOR_ATTRIBUTE_START:
			enable = (u16)nla_get_u8(attr);
			break;
		case SLSI_RSSI_MONITOR_ATTRIBUTE_MIN_RSSI:
			min_rssi = nla_get_s8(attr);
			break;
		case SLSI_RSSI_MONITOR_ATTRIBUTE_MAX_RSSI:
			max_rssi = nla_get_s8(attr);
			break;
		default:
			r = -EINVAL;
			goto exit;
		}
	}
	if (min_rssi > max_rssi) {
		SLSI_ERR(sdev, "Invalid params, min_rssi= %d ,max_rssi = %d\n", min_rssi, max_rssi);
		r = -EINVAL;
		goto exit;
	}
	r = slsi_mlme_set_rssi_monitor(sdev, net_dev, enable, min_rssi, max_rssi);
exit:
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	return r;
}

#ifdef CONFIG_SCSC_WLAN_DEBUG
void slsi_lls_debug_dump_stats(struct slsi_dev *sdev, struct slsi_lls_radio_stat *radio_stat,
			       struct slsi_lls_iface_stat *iface_stat, u8 *buf, int buf_len)
{
	int i;
	struct slsi_lls_channel_info *channel;

	SLSI_DBG3(sdev, SLSI_GSCAN, "radio_stat====\n");
	SLSI_DBG3(sdev, SLSI_GSCAN, "\tradio_id %d\n", radio_stat->radio);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\ton_time %d\n", radio_stat->on_time);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\ttx_time %d\n", radio_stat->tx_time);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\trx_time %d\n", radio_stat->rx_time);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\ton_time_scan %d\n", radio_stat->on_time_scan);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\tnum_channels %d\n", radio_stat->num_channels);

	for (i = 0; i < radio_stat->num_channels; i++) {
		channel = &radio_stat->channels[i].channel;
		SLSI_DBG3(sdev, SLSI_GSCAN, "\tchan index %d\n", i);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\twidth %d\n", channel->width);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tcenter_freq %d\n", channel->center_freq);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tcenter_freq0 %d\n", channel->center_freq0);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tcenter_freq1 %d\n", channel->center_freq1);
	}
	SLSI_DBG3(sdev, SLSI_GSCAN, "iface_stat====\n");
	SLSI_DBG3(sdev, SLSI_GSCAN, "\tiface %p\n", iface_stat->iface);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\tinfo\n");
	SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tmode %d\n", iface_stat->info.mode);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tmac_addr %pM\n", iface_stat->info.mac_addr);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tstate %d\n", iface_stat->info.state);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\t\troaming %d\n", iface_stat->info.roaming);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tcapabilities %d\n", iface_stat->info.capabilities);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tssid %s\n", iface_stat->info.ssid);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tbssid %pM\n", iface_stat->info.bssid);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tap_country_str [%c%c%d]\n", iface_stat->info.ap_country_str[0],
		  iface_stat->info.ap_country_str[1], iface_stat->info.ap_country_str[2]);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tcountry_str [%c%c%d]\n", iface_stat->info.country_str[0],
		  iface_stat->info.country_str[1], iface_stat->info.country_str[2]);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\tbeacon_rx %d\n", iface_stat->beacon_rx);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\tleaky_ap_detected %d\n", iface_stat->leaky_ap_detected);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\tleaky_ap_guard_time %d\n", iface_stat->leaky_ap_guard_time);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\trssi_data %d\n", iface_stat->rssi_data);
	SLSI_DBG3(sdev, SLSI_GSCAN, "\twifi_wmm_ac_stat\n");

	for (i = 0; i < SLSI_LLS_AC_MAX; i++) {
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tac %d\n", iface_stat->ac[i].ac);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\ttx_mpdu %d\n", iface_stat->ac[i].tx_mpdu);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\trx_mpdu %d\n", iface_stat->ac[i].rx_mpdu);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tmpdu_lost %d\n", iface_stat->ac[i].mpdu_lost);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tretries %d\n", iface_stat->ac[i].retries);
	}
	SLSI_DBG3(sdev, SLSI_GSCAN, "\tnum_peers %d\n", iface_stat->num_peers);
	for (i = 0; i < iface_stat->num_peers; i++) {
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\ttype %d\n", iface_stat->peer_info[i].type);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tpeer_mac_address %pM\n", iface_stat->peer_info[i].peer_mac_address);
		SLSI_DBG3(sdev, SLSI_GSCAN, "\t\tcapabilities %d\n", iface_stat->peer_info[i].capabilities);
	}

	SLSI_DBG_HEX(sdev, SLSI_GSCAN, buf, buf_len, "return buffer\n");
}
#endif

static int slsi_lls_set_stats(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev     *sdev = SDEV_FROM_WIPHY(wiphy);
	struct net_device   *net_dev = NULL;
	struct netdev_vif   *ndev_vif = NULL;
	int                 temp;
	int                 type;
	const struct nlattr *attr;
	u32                 mpdu_size_threshold = 0;
	u32                 aggr_stat_gathering = 0;
	int                 r = 0, i;

	SLSI_DBG3_NODEV(SLSI_GSCAN, "\n");

	if (!slsi_dev_lls_supported())
		return -EOPNOTSUPP;

	nla_for_each_attr(attr, data, len, temp) {
		type = nla_type(attr);

		switch (type) {
		case LLS_ATTRIBUTE_SET_MPDU_SIZE_THRESHOLD:
			mpdu_size_threshold = nla_get_u32(attr);
			SLSI_WARN(sdev, "mpdu_size_threshold:%u\n", mpdu_size_threshold);
			break;

		case LLS_ATTRIBUTE_SET_AGGR_STATISTICS_GATHERING:
			aggr_stat_gathering = nla_get_u32(attr);
			SLSI_WARN(sdev, "aggr_stat_gathering:%u\n", aggr_stat_gathering);
			break;

		default:
			SLSI_ERR_NODEV("Unknown attribute: %d\n", type);
			r = -EINVAL;
		}
	}

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	/* start Statistics measurements in Firmware */
	(void)slsi_mlme_start_link_stats_req(sdev, mpdu_size_threshold, aggr_stat_gathering);

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	if (net_dev) {
		ndev_vif = netdev_priv(net_dev);
		for (i = 0; i < SLSI_LLS_AC_MAX; i++) {
			ndev_vif->rx_packets[i] = 0;
			ndev_vif->tx_packets[i] = 0;
			ndev_vif->tx_no_ack[i] = 0;
		}
	}
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
	return 0;
}

static int slsi_lls_clear_stats(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev    *sdev = SDEV_FROM_WIPHY(wiphy);
	int                      temp;
	int                      type;
	const struct nlattr      *attr;
	u32 stats_clear_req_mask = 0;
	u32 stop_req             = 0;
	int r = 0, i;
	struct net_device *net_dev = NULL;
	struct netdev_vif *ndev_vif = NULL;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	nla_for_each_attr(attr, data, len, temp) {
		type = nla_type(attr);

		switch (type) {
		case LLS_ATTRIBUTE_CLEAR_STOP_REQUEST_MASK:
			stats_clear_req_mask = nla_get_u32(attr);
			SLSI_DBG3(sdev, SLSI_GSCAN, "stats_clear_req_mask:%u\n", stats_clear_req_mask);
			break;

		case LLS_ATTRIBUTE_CLEAR_STOP_REQUEST:
			stop_req = nla_get_u32(attr);
			SLSI_DBG3(sdev, SLSI_GSCAN, "stop_req:%u\n", stop_req);
			break;

		default:
			SLSI_ERR(sdev, "Unknown attribute:%d\n", type);
			r = -EINVAL;
		}
	}

	/* stop_req = 0 : clear the stats which are flaged 0
	 * stop_req = 1 : clear the stats which are flaged 1
	 */
	if (!stop_req)
		stats_clear_req_mask = ~stats_clear_req_mask;

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	(void)slsi_mlme_stop_link_stats_req(sdev, stats_clear_req_mask);
	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);
	if (net_dev) {
		ndev_vif = netdev_priv(net_dev);
		for (i = 0; i < SLSI_LLS_AC_MAX; i++) {
			ndev_vif->rx_packets[i] = 0;
			ndev_vif->tx_packets[i] = 0;
			ndev_vif->tx_no_ack[i] = 0;
		}
	}
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);
	return 0;
}

static u32 slsi_lls_ie_to_cap(const u8 *ies, int ies_len)
{
	u32 capabilities = 0;
	const u8 *ie_data;
	const u8 *ie;
	int ie_len;

	if (!ies || ies_len == 0) {
		SLSI_ERR_NODEV("no ie[&%p %d]\n", ies, ies_len);
		return 0;
	}
	ie = cfg80211_find_ie(WLAN_EID_EXT_CAPABILITY, ies, ies_len);
	if (ie) {
		ie_len = ie[1];
		ie_data = &ie[2];
		if ((ie_len >= 4) && (ie_data[3] & SLSI_WLAN_EXT_CAPA3_INTERWORKING_ENABLED))
			capabilities |= SLSI_LLS_CAPABILITY_INTERWORKING;
		if ((ie_len >= 7) && (ie_data[6] & 0x01)) /* Bit48: UTF-8 ssid */
			capabilities |= SLSI_LLS_CAPABILITY_SSID_UTF8;
	}

	ie = cfg80211_find_vendor_ie(WLAN_OUI_WFA, SLSI_WLAN_OUI_TYPE_WFA_HS20_IND, ies, ies_len);
	if (ie)
		capabilities |= SLSI_LLS_CAPABILITY_HS20;
	return capabilities;
}

static void slsi_lls_iface_sta_stats(struct slsi_dev *sdev, struct netdev_vif *ndev_vif,
				     struct slsi_lls_iface_stat *iface_stat)
{
	int                       i;
	struct slsi_lls_interface_link_layer_info *lls_info = &iface_stat->info;
	enum slsi_lls_peer_type   peer_type;
	struct slsi_peer          *peer;
	const u8                  *ie_data, *ie;
	u8                        ie_len;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	if (ndev_vif->ifnum == SLSI_NET_INDEX_WLAN) {
		lls_info->mode = SLSI_LLS_INTERFACE_STA;
		peer_type = SLSI_LLS_PEER_AP;
	} else {
		lls_info->mode = SLSI_LLS_INTERFACE_P2P_CLIENT;
		peer_type = SLSI_LLS_PEER_P2P_GO;
	}

	switch (ndev_vif->sta.vif_status) {
	case SLSI_VIF_STATUS_CONNECTING:
		lls_info->state = SLSI_LLS_AUTHENTICATING;
		break;
	case SLSI_VIF_STATUS_CONNECTED:
		lls_info->state = SLSI_LLS_ASSOCIATED;
		break;
	default:
		lls_info->state = SLSI_LLS_DISCONNECTED;
	}
	lls_info->roaming = ndev_vif->sta.roam_in_progress ?
				SLSI_LLS_ROAMING_ACTIVE : SLSI_LLS_ROAMING_IDLE;

	iface_stat->info.capabilities = 0;
	lls_info->ssid[0] = 0;
	if (ndev_vif->sta.sta_bss) {
		ie = cfg80211_find_ie(WLAN_EID_SSID, ndev_vif->sta.sta_bss->ies->data,
				      ndev_vif->sta.sta_bss->ies->len);
		if (ie) {
			ie_len = ie[1];
			ie_data = &ie[2];
			memcpy(lls_info->ssid, ie_data, ie_len);
			lls_info->ssid[ie_len] = 0;
		}
		SLSI_ETHER_COPY(lls_info->bssid, ndev_vif->sta.sta_bss->bssid);
		ie = cfg80211_find_ie(WLAN_EID_COUNTRY, ndev_vif->sta.sta_bss->ies->data,
				      ndev_vif->sta.sta_bss->ies->len);
		if (ie) {
			ie_data = &ie[2];
			memcpy(lls_info->ap_country_str, ie_data, 3);
			iface_stat->peer_info[0].capabilities |= SLSI_LLS_CAPABILITY_COUNTRY;
		}
	}

	peer = ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]; /* connected AP */
	if (peer && peer->valid && peer->assoc_ie && peer->assoc_resp_ie) {
		iface_stat->info.capabilities |= slsi_lls_ie_to_cap(peer->assoc_ie->data, peer->assoc_ie->len);
		if (peer->capabilities & WLAN_CAPABILITY_PRIVACY) {
			iface_stat->peer_info[0].capabilities |= SLSI_LLS_CAPABILITY_PROTECTED;
			iface_stat->info.capabilities |= SLSI_LLS_CAPABILITY_PROTECTED;
		}
		if (peer->qos_enabled) {
			iface_stat->peer_info[0].capabilities |= SLSI_LLS_CAPABILITY_QOS;
			iface_stat->info.capabilities |= SLSI_LLS_CAPABILITY_QOS;
		}
		iface_stat->peer_info[0].capabilities |= slsi_lls_ie_to_cap(peer->assoc_resp_ie->data, peer->assoc_resp_ie->len);

		SLSI_ETHER_COPY(iface_stat->peer_info[0].peer_mac_address, peer->address);
		iface_stat->peer_info[0].type = peer_type;
		iface_stat->num_peers = 1;
	}

	for (i = MAP_AID_TO_QS(SLSI_TDLS_PEER_INDEX_MIN); i <= MAP_AID_TO_QS(SLSI_TDLS_PEER_INDEX_MAX); i++) {
		peer = ndev_vif->peer_sta_record[i];
		if (peer && peer->valid) {
			SLSI_ETHER_COPY(iface_stat->peer_info[iface_stat->num_peers].peer_mac_address, peer->address);
			iface_stat->peer_info[iface_stat->num_peers].type = SLSI_LLS_PEER_TDLS;
			if (peer->qos_enabled)
				iface_stat->peer_info[iface_stat->num_peers].capabilities |= SLSI_LLS_CAPABILITY_QOS;
			iface_stat->peer_info[iface_stat->num_peers].num_rate = 0;
			iface_stat->num_peers++;
		}
	}
}

static void slsi_lls_iface_ap_stats(struct slsi_dev *sdev, struct netdev_vif *ndev_vif, struct slsi_lls_iface_stat *iface_stat)
{
	enum slsi_lls_peer_type peer_type = SLSI_LLS_PEER_INVALID;
	struct slsi_peer        *peer;
	int                     i;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	/* We are AP/GO, so we advertize our own country. */
	memcpy(iface_stat->info.ap_country_str, iface_stat->info.country_str, 3);

	if (ndev_vif->ifnum == SLSI_NET_INDEX_WLAN) {
		iface_stat->info.mode = SLSI_LLS_INTERFACE_SOFTAP;
		peer_type = SLSI_LLS_PEER_STA;
	} else if (ndev_vif->ifnum == SLSI_NET_INDEX_P2PX) {
		iface_stat->info.mode = SLSI_LLS_INTERFACE_P2P_GO;
		peer_type = SLSI_LLS_PEER_P2P_CLIENT;
	}

	for (i = MAP_AID_TO_QS(SLSI_PEER_INDEX_MIN); i <= MAP_AID_TO_QS(SLSI_PEER_INDEX_MAX); i++) {
		peer = ndev_vif->peer_sta_record[i];
		if (peer && peer->valid) {
			SLSI_ETHER_COPY(iface_stat->peer_info[iface_stat->num_peers].peer_mac_address, peer->address);
			iface_stat->peer_info[iface_stat->num_peers].type = peer_type;
			iface_stat->peer_info[iface_stat->num_peers].num_rate = 0;
			if (peer->qos_enabled)
				iface_stat->peer_info[iface_stat->num_peers].capabilities = SLSI_LLS_CAPABILITY_QOS;
			iface_stat->num_peers++;
		}
	}

	memcpy(iface_stat->info.ssid, ndev_vif->ap.ssid, ndev_vif->ap.ssid_len);
	iface_stat->info.ssid[ndev_vif->ap.ssid_len] = 0;
	if (ndev_vif->ap.privacy)
		iface_stat->info.capabilities |= SLSI_LLS_CAPABILITY_PROTECTED;
	if (ndev_vif->ap.qos_enabled)
		iface_stat->info.capabilities |= SLSI_LLS_CAPABILITY_QOS;
}

static void slsi_lls_iface_stat_fill(struct slsi_dev *sdev,
				     struct net_device *net_dev,
				     struct slsi_lls_iface_stat *iface_stat)
{
	int                       i;
	struct netdev_vif         *ndev_vif;
	struct slsi_mib_data      mibrsp = { 0, NULL };
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = {{ SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_BE + 1, 0 } },
						 { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_BK + 1, 0 } },
						 { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_VI + 1, 0 } },
						 { SLSI_PSID_UNIFI_AC_RETRIES, { SLSI_TRAFFIC_Q_VO + 1, 0 } },
						 { SLSI_PSID_UNIFI_BEACON_RECEIVED, {0, 0} },
						 { SLSI_PSID_UNIFI_PS_LEAKY_AP, {0, 0} },
						 { SLSI_PSID_UNIFI_RSSI, {0, 0} } };

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	iface_stat->iface = NULL;
	iface_stat->info.mode = SLSI_LLS_INTERFACE_UNKNOWN;
	iface_stat->info.country_str[0] = sdev->device_config.domain_info.alpha2[0];
	iface_stat->info.country_str[1] = sdev->device_config.domain_info.alpha2[1];
	iface_stat->info.country_str[2] = ' '; /* 3rd char of our country code is ASCII<space> */

	for (i = 0; i < SLSI_LLS_AC_MAX; i++)
		iface_stat->ac[i].ac = SLSI_LLS_AC_MAX;

	if (!net_dev)
		return;

	ndev_vif = netdev_priv(net_dev);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	if (!ndev_vif->activated)
		goto exit;

	if (ndev_vif->vif_type == FAPI_VIFTYPE_STATION) {
		slsi_lls_iface_sta_stats(sdev, ndev_vif, iface_stat);
	} else if (ndev_vif->vif_type == FAPI_VIFTYPE_AP) {
		slsi_lls_iface_ap_stats(sdev, ndev_vif, iface_stat);
		SLSI_ETHER_COPY(iface_stat->info.bssid, net_dev->dev_addr);
	}
	SLSI_ETHER_COPY(iface_stat->info.mac_addr, net_dev->dev_addr);

	mibrsp.dataLength = 10 * sizeof(get_values) / sizeof(get_values[0]);
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (!mibrsp.data) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes for interface MIBs\n", mibrsp.dataLength);
		goto exit;
	}

	values = slsi_read_mibs(sdev, net_dev, get_values, sizeof(get_values) / sizeof(get_values[0]), &mibrsp);
	if (!values)
		goto exit;

	for (i = 0; i < SLSI_LLS_AC_MAX; i++) {
		if (values[i].type == SLSI_MIB_TYPE_UINT) {
			iface_stat->ac[i].ac = slsi_fapi_to_android_traffic_q(i);
			iface_stat->ac[i].retries = values[i].u.uintValue;
			iface_stat->ac[i].rx_mpdu = ndev_vif->rx_packets[i];
			iface_stat->ac[i].tx_mpdu = ndev_vif->tx_packets[i];
			iface_stat->ac[i].mpdu_lost = ndev_vif->tx_no_ack[i];
		} else {
			SLSI_WARN(sdev, "LLS: expected datatype 1 but received %d\n", values[i].type);
		}
	}

	if (values[4].type == SLSI_MIB_TYPE_UINT)
		iface_stat->beacon_rx = values[4].u.uintValue;

	if (values[5].type == SLSI_MIB_TYPE_UINT) {
		iface_stat->leaky_ap_detected = values[5].u.uintValue;
		iface_stat->leaky_ap_guard_time = 5; /* 5 milli sec. As mentioned in lls document */
	}

	if (values[6].type == SLSI_MIB_TYPE_INT)
		iface_stat->rssi_data = values[6].u.intValue;

exit:
	kfree(values);
	kfree(mibrsp.data);
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

static void slsi_lls_radio_stat_fill(struct slsi_dev *sdev, struct net_device *dev,
				     struct slsi_lls_radio_stat *radio_stat,
				     int max_chan_count)
{
	struct slsi_mib_data      mibrsp = { 0, NULL };
	struct slsi_mib_data      supported_chan_mib = { 0, NULL };
	struct slsi_mib_value     *values = NULL;
	struct slsi_mib_get_entry get_values[] = {{ SLSI_PSID_UNIFI_RADIO_SCAN_TIME, { 0, 0 } },
						 { SLSI_PSID_UNIFI_RADIO_RX_TIME, { 0, 0 } },
						 { SLSI_PSID_UNIFI_RADIO_TX_TIME, { 0, 0 } },
						 { SLSI_PSID_UNIFI_RADIO_ON_TIME, { 0, 0 } },
						 { SLSI_PSID_UNIFI_SUPPORTED_CHANNELS, { 0, 0 } } };
	u32                       *radio_data[] = {&radio_stat->on_time_scan, &radio_stat->rx_time,
						   &radio_stat->tx_time, &radio_stat->on_time};
	int                       i, j, chan_count, chan_start, k;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	radio_stat->radio = 0;

	/* Expect each mib length in response is <= 15 So assume 15 bytes for each MIB */
	mibrsp.dataLength = 15 * sizeof(get_values) / sizeof(get_values[0]);
	mibrsp.data = kmalloc(mibrsp.dataLength, GFP_KERNEL);
	if (mibrsp.data == NULL) {
		SLSI_ERR(sdev, "Cannot kmalloc %d bytes\n", mibrsp.dataLength);
		return;
	}

	values = slsi_read_mibs(sdev, NULL, get_values, sizeof(get_values) / sizeof(get_values[0]), &mibrsp);
	if (!values)
		goto exit_with_mibrsp;

	for (i = 0; i < (sizeof(get_values) / sizeof(get_values[0])) - 1; i++) {
		if (values[i].type == SLSI_MIB_TYPE_UINT) {
			*radio_data[i] = values[i].u.uintValue;
			SLSI_DBG2(sdev, SLSI_GSCAN, "MIB value = %ud\n", *radio_data[i]);
		} else {
			SLSI_ERR(sdev, "invalid type. iter:%d", i);
		}
	}
	if (values[4].type != SLSI_MIB_TYPE_OCTET) {
		SLSI_ERR(sdev, "Supported_Chan invalid type.");
		goto exit_with_values;
	}

	supported_chan_mib = values[4].u.octetValue;
	for (j = 0; j < supported_chan_mib.dataLength / 2; j++) {
		struct slsi_lls_channel_info *radio_chan;

		chan_start = supported_chan_mib.data[j*2];
		chan_count = supported_chan_mib.data[j*2 + 1];
		if (radio_stat->num_channels + chan_count > max_chan_count)
			chan_count = max_chan_count - radio_stat->num_channels;
		if (chan_start == 1) { /* for 2.4GHz */
			for (k = 0; k < chan_count; k++) {
				radio_chan = &radio_stat->channels[radio_stat->num_channels + k].channel;
				if (k + chan_start == 14)
					radio_chan->center_freq = 2484;
				else
					radio_chan->center_freq = 2407 + (chan_start + k) * 5;
				radio_chan->width = SLSI_LLS_CHAN_WIDTH_20;
			}
		} else {
			/* for 5GHz */
			for (k = 0; k < chan_count; k++) {
				radio_chan = &radio_stat->channels[radio_stat->num_channels + k].channel;
				radio_chan->center_freq = 5000 + (chan_start + (k * 4)) * 5;
				radio_chan->width = SLSI_LLS_CHAN_WIDTH_20;
			}
		}
		radio_stat->num_channels += chan_count;
	}
exit_with_values:
	kfree(values);
exit_with_mibrsp:
	kfree(mibrsp.data);
}

static int slsi_lls_fill(struct slsi_dev *sdev, u8 **src_buf)
{
	struct net_device          *net_dev = NULL;
	struct slsi_lls_radio_stat *radio_stat;
	struct slsi_lls_iface_stat *iface_stat;
	int                        buf_len = 0;
	int                        max_chan_count = 0;
	u8                         *buf;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	net_dev = slsi_get_netdev(sdev, SLSI_NET_INDEX_WLAN);

	if (sdev->wiphy->bands[IEEE80211_BAND_2GHZ])
		max_chan_count = sdev->wiphy->bands[IEEE80211_BAND_2GHZ]->n_channels;
	if (sdev->wiphy->bands[IEEE80211_BAND_5GHZ])
		max_chan_count += sdev->wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels;
	buf_len = (int)(sizeof(struct slsi_lls_radio_stat) + sizeof(struct slsi_lls_iface_stat)
			+ (sizeof(struct slsi_lls_peer_info) * SLSI_ADHOC_PEER_CONNECTIONS_MAX)
			+ (sizeof(struct slsi_lls_channel_stat) * max_chan_count));
	buf = kzalloc(buf_len, GFP_KERNEL);
	if (!buf) {
		SLSI_ERR(sdev, "No mem. Size:%d\n", buf_len);
		return -ENOMEM;
	}
	*src_buf = buf;

	iface_stat = (struct slsi_lls_iface_stat *)buf;
	slsi_lls_iface_stat_fill(sdev, net_dev, iface_stat);

	radio_stat = (struct slsi_lls_radio_stat *)(buf + sizeof(struct slsi_lls_iface_stat) +
		     (sizeof(struct slsi_lls_peer_info) * iface_stat->num_peers));
	slsi_lls_radio_stat_fill(sdev, net_dev, radio_stat, max_chan_count);
#ifdef CONFIG_SCSC_WLAN_DEBUG
	slsi_lls_debug_dump_stats(sdev, radio_stat, iface_stat, buf, buf_len);
#endif
	return buf_len;
}

static int slsi_lls_get_stats(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev        *sdev = SDEV_FROM_WIPHY(wiphy);
	int                    ret;
	u8                     *buf = NULL;
	int                    buf_len;

	SLSI_DBG3(sdev, SLSI_GSCAN, "\n");

	if (!slsi_dev_lls_supported())
		return -EOPNOTSUPP;

	SLSI_MUTEX_LOCK(sdev->device_config_mutex);
	buf_len = slsi_lls_fill(sdev, &buf);
	SLSI_MUTEX_UNLOCK(sdev->device_config_mutex);

	if (buf_len > 0) {
		ret = slsi_vendor_cmd_reply(wiphy, buf, buf_len);
		if (ret)
			SLSI_ERR_NODEV("vendor cmd reply failed (err:%d)\n", ret);
	} else {
		ret = buf_len;
	}
	kfree(buf);
	return ret;
}

static int slsi_gscan_set_oui(struct wiphy *wiphy,
			      struct wireless_dev *wdev, const void *data, int len)
{
	SLSI_UNUSED_PARAMETER_NOT_DEBUG(wiphy);
	SLSI_UNUSED_PARAMETER_NOT_DEBUG(wdev);
	SLSI_UNUSED_PARAMETER_NOT_DEBUG(data);
	SLSI_UNUSED_PARAMETER_NOT_DEBUG(len);
	SLSI_DBG1_NODEV(SLSI_GSCAN, "SUBCMD_SET_GSCAN_OUI\n");

	return -ENOTSUPP;
}

static int slsi_get_feature_set(struct wiphy *wiphy,
				struct wireless_dev *wdev, const void *data, int len)
{
	u32 feature_set = 0;
	int ret = 0;

	SLSI_DBG3_NODEV(SLSI_GSCAN, "\n");

	feature_set |= SLSI_WIFI_HAL_FEATURE_RSSI_MONITOR;
#ifdef CONFIG_SCSC_NAT_KEEPALIVE
	feature_set |= SLSI_WIFI_HAL_FEATURE_MKEEP_ALIVE;
#endif
	if (slsi_dev_gscan_supported())
		feature_set |= SLSI_WIFI_HAL_FEATURE_GSCAN;
	if (slsi_dev_lls_supported())
		feature_set |= SLSI_WIFI_HAL_FEATURE_LINK_LAYER_STATS;
	if (slsi_dev_epno_supported())
		feature_set |= SLSI_WIFI_HAL_FEATURE_HAL_EPNO;

	ret = slsi_vendor_cmd_reply(wiphy, &feature_set, sizeof(feature_set));

	return ret;
}

static int slsi_set_country_code(struct wiphy *wiphy, struct wireless_dev *wdev, const void *data, int len)
{
	struct slsi_dev          *sdev = SDEV_FROM_WIPHY(wiphy);
	int                      ret = 0;
	int                      temp;
	int                      type;
	const struct nlattr      *attr;
	char country_code[SLSI_COUNTRY_CODE_LEN];

	SLSI_DBG3(sdev, SLSI_GSCAN, "Received country code command\n");

	nla_for_each_attr(attr, data, len, temp) {
		type = nla_type(attr);
		switch (type) {
		case SLSI_NL_ATTRIBUTE_COUNTRY_CODE:
		{
			if (nla_len(attr) < (SLSI_COUNTRY_CODE_LEN-1)) {
				ret = -EINVAL;
				SLSI_ERR(sdev, "Insufficient Country Code Length : %d\n", nla_len(attr));
				return ret;
			}
			memcpy(country_code, nla_data(attr), (SLSI_COUNTRY_CODE_LEN-1));
			break;
		}
		default:
			ret = -EINVAL;
			SLSI_ERR(sdev, "Invalid type : %d\n", type);
			return ret;
		}
	}
	ret = slsi_set_country_update_regd(sdev, country_code, SLSI_COUNTRY_CODE_LEN);
	if (ret < 0)
		SLSI_ERR(sdev, "Set country failed ret:%d\n", ret);
	return ret;
}

static const struct  nl80211_vendor_cmd_info slsi_vendor_events[] = {
	{ OUI_GOOGLE, SLSI_NL80211_SIGNIFICANT_CHANGE_EVENT },
	{ OUI_GOOGLE, SLSI_NL80211_HOTLIST_AP_FOUND_EVENT },
	{ OUI_GOOGLE, SLSI_NL80211_SCAN_RESULTS_AVAILABLE_EVENT },
	{ OUI_GOOGLE, SLSI_NL80211_FULL_SCAN_RESULT_EVENT },
	{ OUI_GOOGLE, SLSI_NL80211_SCAN_EVENT },
	{ OUI_GOOGLE, SLSI_NL80211_HOTLIST_AP_LOST_EVENT },
#ifdef CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD
	{ OUI_SAMSUNG, SLSI_NL80211_VENDOR_SUBCMD_KEY_MGMT_ROAM_AUTH },
#endif
	{ OUI_SAMSUNG, SLSI_NL80211_VENDOR_HANGED_EVENT },
	{ OUI_GOOGLE,  SLSI_NL80211_EPNO_EVENT },
	{ OUI_GOOGLE,  SLSI_NL80211_HOTSPOT_MATCH },
	{ OUI_GOOGLE,  SLSI_NL80211_RSSI_REPORT_EVENT},
};

static const struct wiphy_vendor_command     slsi_vendor_cmd[] = {
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_GET_CAPABILITIES
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_get_capabilities
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_GET_VALID_CHANNELS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_get_valid_channel
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_ADD_GSCAN
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_add
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_DEL_GSCAN
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_del
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_GET_SCAN_RESULTS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_get_scan_results
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_SET_BSSID_HOTLIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_set_hotlist
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_RESET_BSSID_HOTLIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_reset_hotlist
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_SET_SIGNIFICANT_CHANGE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_set_significant_change
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_RESET_SIGNIFICANT_CHANGE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_reset_significant_change
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_SET_GSCAN_OUI
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_gscan_set_oui
	},
#ifdef CONFIG_SCSC_WLAN_KEY_MGMT_OFFLOAD
	{
		{
			.vendor_id = OUI_SAMSUNG,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_KEY_MGMT_SET_KEY
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_key_mgmt_set_pmk
	},
#endif
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_SET_BSSID_BLACKLIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_set_bssid_blacklist
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_START_KEEP_ALIVE_OFFLOAD
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_start_keepalive_offload
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_STOP_KEEP_ALIVE_OFFLOAD
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_stop_keepalive_offload
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_SET_EPNO_LIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_set_epno_ssid
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_SET_HS_LIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_set_hs_params
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_RESET_HS_LIST
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_reset_hs_params
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_SET_RSSI_MONITOR
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_set_rssi_monitor
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_LSTATS_SUBCMD_SET_STATS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_lls_set_stats
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_LSTATS_SUBCMD_GET_STATS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_lls_get_stats
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_LSTATS_SUBCMD_CLEAR_STATS
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_lls_clear_stats
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_GET_FEATURE_SET
		},
		.flags = 0,
		.doit = slsi_get_feature_set
	},
	{
		{
			.vendor_id = OUI_GOOGLE,
			.subcmd = SLSI_NL80211_VENDOR_SUBCMD_SET_COUNTRY_CODE
		},
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = slsi_set_country_code
	}
};

void slsi_nl80211_vendor_deinit(struct slsi_dev *sdev)
{
	SLSI_DBG2(sdev, SLSI_GSCAN, "De-initialise vendor command and events\n");
	sdev->wiphy->vendor_commands = NULL;
	sdev->wiphy->n_vendor_commands = 0;
	sdev->wiphy->vendor_events = NULL;
	sdev->wiphy->n_vendor_events = 0;

	SLSI_DBG2(sdev, SLSI_GSCAN, "Gscan cleanup\n");
	slsi_gscan_flush_scan_results(sdev);

	SLSI_DBG2(sdev, SLSI_GSCAN, "Hotlist cleanup\n");
	slsi_gscan_flush_hotlist_results(sdev);
}

void slsi_nl80211_vendor_init(struct slsi_dev *sdev)
{
	int i;

	SLSI_DBG2(sdev, SLSI_GSCAN, "Init vendor command and events\n");

	sdev->wiphy->vendor_commands = slsi_vendor_cmd;
	sdev->wiphy->n_vendor_commands = ARRAY_SIZE(slsi_vendor_cmd);
	sdev->wiphy->vendor_events = slsi_vendor_events;
	sdev->wiphy->n_vendor_events = ARRAY_SIZE(slsi_vendor_events);

	for (i = 0; i < SLSI_GSCAN_MAX_BUCKETS; i++)
		sdev->bucket[i].scan_id = (SLSI_GSCAN_SCAN_ID_START + i);

	for (i = 0; i < SLSI_GSCAN_HASH_TABLE_SIZE; i++)
		sdev->gscan_hash_table[i] = NULL;

	INIT_LIST_HEAD(&sdev->hotlist_results);
}
