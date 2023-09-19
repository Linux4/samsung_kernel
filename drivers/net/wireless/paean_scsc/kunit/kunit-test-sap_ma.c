// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-netif.h"
#include "kunit-mock-traffic_monitor.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-ba.h"
#include "kunit-mock-ba_replay.h"
#include "kunit-mock-txbp.h"
#include "../sap_ma.c"

static void test_sap_ma_notifier(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	sdev->netdev[SLSI_NET_INDEX_WLAN] = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, sap_ma_notifier(sdev, SCSC_WIFI_STOP));
	KUNIT_EXPECT_EQ(test, 0, sap_ma_notifier(sdev, SCSC_WIFI_FAILURE_RESET));
	KUNIT_EXPECT_EQ(test, -EIO, sap_ma_notifier(sdev, SCSC_MAX_NOTIFIER));
}

static void test_sap_ma_version_supported(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, 0, sap_ma_version_supported(0x0000));
	KUNIT_EXPECT_EQ(test, -EINVAL, sap_ma_version_supported(0x1234));
}

static void test_slsi_rx_check_mc_addr_regd(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct ethhdr *ehdr = kunit_kzalloc(test, sizeof(struct ethhdr), GFP_KERNEL);

	ndev_vif->sta.regd_mc_addr_count = 1;
	KUNIT_EXPECT_EQ(test, true, slsi_rx_check_mc_addr_regd(sdev, dev, ehdr));

	ndev_vif->sta.regd_mc_addr_count = 0;
	KUNIT_EXPECT_EQ(test, false, slsi_rx_check_mc_addr_regd(sdev, dev, ehdr));
}

static void test_slsi_rx_amsdu_deaggregate(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);
	struct sk_buff_head *msdu_list = kunit_kzalloc(test, sizeof(struct sk_buff_head), GFP_KERNEL);
	struct slsi_skb_cb *skb_cb;
	struct ethhdr *mh = NULL;
	unsigned char mac_0[ETH_ALEN] = { 0 };
	unsigned int data_len = 0;
	unsigned int subframe_len = 0;

	__skb_queue_head_init(&msdu_list);

	skb->len = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_rx_amsdu_deaggregate(dev, skb, msdu_list));

	skb = alloc_skb(100, GFP_KERNEL);
	skb->len = ETH_ALEN * 2 + 2;
	skb->data[(ETH_ALEN * 2) + 1] = LLC_SNAP_HDR_LEN - 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_rx_amsdu_deaggregate(dev, skb, msdu_list));

	skb = alloc_skb(100, GFP_KERNEL);
	skb->len = ETH_ALEN * 2 + 2;
	skb->data[(ETH_ALEN * 2) + 1] = LLC_SNAP_HDR_LEN + 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_rx_amsdu_deaggregate(dev, skb, msdu_list));

	skb = alloc_skb(100, GFP_KERNEL);
	skb_cb = slsi_skb_cb_get(skb);
	skb_cb->discard = 1;
	skb->data[(ETH_ALEN * 2) + 1] = LLC_SNAP_HDR_LEN + 1;
	data_len = (skb->data[ETH_ALEN * 2] << 8) | skb->data[(ETH_ALEN * 2) + 1];
	data_len -= LLC_SNAP_HDR_LEN;
	subframe_len = sizeof(struct ethhdr) + data_len;
	skb->len = ETH_ALEN * 2 + 2 + LLC_SNAP_HDR_LEN + data_len;
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_amsdu_deaggregate(dev, skb, msdu_list));

	skb = alloc_skb(100, GFP_KERNEL);
	skb->data[(ETH_ALEN * 2) + 1] = LLC_SNAP_HDR_LEN + 1;
	mh = (struct ethhdr *)skb->head;
	memset(mh->h_dest, 0, 6);
	data_len = (skb->data[ETH_ALEN * 2] << 8) | skb->data[(ETH_ALEN * 2) + 1];
	data_len -= LLC_SNAP_HDR_LEN;
	subframe_len = sizeof(struct ethhdr) + data_len;
	skb->len = subframe_len + LLC_SNAP_HDR_LEN;
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_amsdu_deaggregate(dev, skb, msdu_list));

	skb = alloc_skb(100, GFP_KERNEL);
	skb->len = subframe_len + LLC_SNAP_HDR_LEN + 1;
	skb->data[(ETH_ALEN * 2) + 1] = LLC_SNAP_HDR_LEN + 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_amsdu_deaggregate(dev, skb, msdu_list));
	kfree_skb(skb);

	skb = alloc_skb(100, GFP_KERNEL);
	skb_cb = slsi_skb_cb_get(skb);
	skb_cb->discard = 1;
	skb->data[(ETH_ALEN * 2) + 1] = LLC_SNAP_HDR_LEN + 1;
	data_len = (skb->data[ETH_ALEN * 2] << 8) | skb->data[(ETH_ALEN * 2) + 1];
	data_len -= LLC_SNAP_HDR_LEN;
	subframe_len = sizeof(struct ethhdr) + data_len;
	skb->len = ETH_ALEN * 2 + 2 + LLC_SNAP_HDR_LEN + data_len;
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_amsdu_deaggregate(dev, skb, msdu_list));
}

static void test_slsi_rx_check_opt_out_packet(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct ethhdr *ehdr;
	u8 mac[ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 *frame;

	skb->data = kunit_kzalloc(test, 30, GFP_KERNEL);
	skb->data[sizeof(struct ethhdr)] = SLSI_ARP_REQUEST_OPCODE;
	skb->data[sizeof(struct ethhdr) + 1] = 0;
	ehdr = (struct ethhdr *)skb->data;
	memcpy(ehdr->h_dest, mac, ETH_ALEN);
	slsi_rx_check_opt_out_packet(sdev, dev, skb);

#ifdef __BIG_ENDIAN
	//not broad but multi
	ehdr->h_dest[1] = 0x01;
	slsi_rx_check_opt_out_packet(sdev, dev, skb);

	//neither broad nor multi
	ehdr->h_test[1] = 0x00;
	slsi_rx_check_opt_out_packet(sdev, dev, skb);
#else
	//not broad but multi
	ehdr->h_dest[0] = 0x01;
	slsi_rx_check_opt_out_packet(sdev, dev, skb);

	//neither broad nor multi
	ehdr->h_dest[0] = 0x00;
	slsi_rx_check_opt_out_packet(sdev, dev, skb);
#endif
	frame = skb->data + sizeof(struct ethhdr);
	frame[SLSI_ARP_OPCODE_OFFSET + 1] = 1;
	ehdr->h_proto = cpu_to_be16(ETH_P_ARP);
	slsi_rx_check_opt_out_packet(sdev, dev, skb);
}

static void test_slsi_parse_and_pull_ma_unitdata_ind_signal(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = fapi_alloc(ma_unitdata_ind, MA_UNITDATA_IND, 0, 10);
	struct slsi_skb_cb *skb_cb = slsi_skb_cb_get(skb);

	KUNIT_EXPECT_PTR_EQ(test, skb, (struct sk_buff *)slsi_parse_and_pull_ma_unitdata_ind_signal(sdev, skb));
	kfree_skb(skb);

	skb = fapi_alloc(ma_unitdata_ind, MA_UNITDATA_IND, 0, 10);
	skb_cb = slsi_skb_cb_get(skb);
	skb_cb->smapper_linked = true;
	KUNIT_EXPECT_PTR_EQ(test, NULL, (struct sk_buff *)slsi_parse_and_pull_ma_unitdata_ind_signal(sdev, skb));

	kfree_skb(skb);
}

static void test_slsi_get_msdu_frame(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct slsi_skb_cb *skb_cb = slsi_skb_cb_get(skb);
	struct ieee80211_hdr *hdr;

	skb->data = kunit_kzalloc(test, sizeof(struct ieee80211_hdr), GFP_KERNEL);
	hdr = (struct ieee80211_hdr *)skb->data;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_msdu_frame(dev, skb));

	hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA);
	KUNIT_EXPECT_EQ(test, 0, slsi_get_msdu_frame(dev, skb));

	hdr->frame_control = 0x88;
	KUNIT_EXPECT_EQ(test, 0, slsi_get_msdu_frame(dev, skb));

	hdr->frame_control = 0x8888;
	skb_cb->is_ciphered = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_get_msdu_frame(dev, skb));
}

static void test_slsi_rx_data_deliver_skb(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);
	struct slsi_skb_cb *skb_cb = slsi_skb_cb_get(skb);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct ethhdr *ehdr;
	u8 mac[ETH_ALEN] = { 0x0FF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 dev_addr[ETH_ALEN] = { 0x01, 0x01, 0x01, 0x01, 0x01, 0x00 };

	ehdr = (struct ethhdr *)skb->data;
	slsi_rx_data_deliver_skb(sdev, dev, skb, 0);

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;

	skb = alloc_skb(100, GFP_KERNEL);
	ehdr = (struct ethhdr *)skb->data;
	skb_cb = slsi_skb_cb_get(skb);
	skb_cb->is_amsdu = true;
	skb->len = 1;
	slsi_rx_data_deliver_skb(sdev, dev, skb, 0);

	skb = alloc_skb(100, GFP_KERNEL);
	ehdr = (struct ethhdr *)skb->data;
	skb_cb = slsi_skb_cb_get(skb);
	skb_cb->discard = true;
	slsi_rx_data_deliver_skb(sdev, dev, skb, 0);

	skb = alloc_skb(100, GFP_KERNEL);
	ehdr = (struct ethhdr *)skb->data;
	skb_cb = slsi_skb_cb_get(skb);
	skb_cb->wakeup = true;
	memcpy(ehdr->h_dest, mac, ETH_ALEN);
	memset(ehdr->h_source, 0x01, ETH_ALEN);
	dev->dev_addr = ehdr->h_source;
	slsi_rx_data_deliver_skb(sdev, dev, skb, 0);

	skb = alloc_skb(100, GFP_KERNEL);
	ehdr = (struct ethhdr *)skb->data;
	dev->dev_addr = dev_addr;
	memcpy(ehdr->h_dest, mac, ETH_ALEN);
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->peer_sta_records = 1;
	memcpy(ndev_vif->peer_sta_record[0]->address, ehdr->h_source, ETH_ALEN);
	slsi_rx_data_deliver_skb(sdev, dev, skb, 0);
	kfree_skb(skb);

	skb = alloc_skb(100, GFP_KERNEL);
	ehdr = (struct ethhdr *)skb->data;
	ehdr->h_dest[0] = 0x00;		//not multi
	dev->dev_addr = dev_addr;
	memcpy(ehdr->h_dest, mac, ETH_ALEN);
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->peer_sta_records = 1;
	memcpy(ndev_vif->peer_sta_record[0]->address, ehdr->h_source, ETH_ALEN);
	sdev->hip.hip_priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	sdev->hip.hip_priv->bh_dat = kunit_kzalloc(test, sizeof(struct bh_struct), GFP_KERNEL);
	slsi_rx_data_deliver_skb(sdev, dev, skb, 1);
	kfree_skb(skb);
}

static void test_slsi_rx_data_ind(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(*sdev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);
	struct slsi_skb_cb *skb_cb;
	struct ethhdr *eth_hdr;
	struct ieee80211_hdr *hdr;
	u8 mac[ETH_ALEN] = { 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	skb_cb = slsi_skb_cb_get(skb);
	slsi_rx_data_ind(sdev, dev, skb);

	skb = alloc_skb(100, GFP_KERNEL);
	skb_cb = slsi_skb_cb_get(skb);
	skb_cb->wakeup = true;
	eth_hdr = (struct ethhdr *)skb->data;
	hdr = (struct ieee80211_hdr *)skb->data;
	hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA);
	slsi_rx_data_ind(sdev, dev, skb);

	skb = alloc_skb(100, GFP_KERNEL);
	skb_cb = slsi_skb_cb_get(skb);
	eth_hdr = (struct ethhdr *)skb->data;
	hdr = (struct ieee80211_hdr *)skb->data;
	hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA);

	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;
	memcpy(ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->address, eth_hdr->h_source, ETH_ALEN);
	slsi_rx_data_ind(sdev, dev, skb);

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	memcpy(eth_hdr->h_dest, mac, ETH_ALEN);
	skb = alloc_skb(100, GFP_KERNEL);
	skb_cb = slsi_skb_cb_get(skb);
	eth_hdr = (struct ethhdr *)skb->data;
	hdr = (struct ieee80211_hdr *)skb->data;
	//the address of hdr->frame_control is the same with the one of eth_hdr->h_dest.
	hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA + 0x0001);

	dev->dev_addr = mac;
	slsi_rx_data_ind(sdev, dev, skb);
	kfree_skb(skb);

	skb = alloc_skb(100, GFP_KERNEL);
	skb_cb = slsi_skb_cb_get(skb);
	eth_hdr = (struct ethhdr *)skb->data;
	hdr = (struct ieee80211_hdr *)skb->data;

	hdr->frame_control = cpu_to_le16(IEEE80211_FTYPE_DATA);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->aid = SLSI_TDLS_PEER_INDEX_MIN - 1;
	slsi_rx_data_ind(sdev, dev, skb);
	kfree_skb(skb);

	skb = alloc_skb(100, GFP_KERNEL);
	skb_cb = slsi_skb_cb_get(skb);
	eth_hdr = (struct ethhdr *)skb->data;
	hdr = (struct ieee80211_hdr *)skb->data;

	hdr->frame_control = cpu_to_le16(IEEE80211_FCTL_TODS) + cpu_to_le16(IEEE80211_FTYPE_DATA);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->aid = SLSI_TDLS_PEER_INDEX_MIN;
	slsi_rx_data_ind(sdev, dev, skb);

	ndev_vif->vif_type = FAPI_VIFTYPE_MONITOR;
	slsi_rx_data_ind(sdev, dev, skb);

	sdev->device_state = -1;
	slsi_rx_data_ind(sdev, dev, skb);
	kfree_skb(skb);
}

#ifdef CONFIG_SCSC_WLAN_RX_NAPI
static void test_slsi_rx_napi_process(struct kunit *test)
{
	struct netdev_vif *ndev_vif;
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(*sdev), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, sizeof(struct fapi_signal), GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_rx_napi_process(sdev, skb));

	((struct fapi_vif_signal_header *)(skb)->data)->vif = SLSI_NET_INDEX_WLAN;
	sdev->netdev[SLSI_NET_INDEX_WLAN] = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	ndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);
	ndev_vif->activated = true;
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_napi_process(sdev, skb));

	fapi_set_u16(skb, id, MA_UNITDATA_IND);
	slsi_spinlock_lock(&ndev_vif->ba_lock);
	atomic_set(&ndev_vif->ba_flush, 1);
	slsi_spinlock_unlock(&ndev_vif->ba_lock);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_napi_process(sdev, skb));
}

#else
static void test_slsi_rx_netdev_data_work(struct kunit *test)
{
	struct sk_buff *skb = fapi_alloc(ma_unitdata_ind, MA_UNITDATA_IND, SLSI_NET_INDEX_WLAN, 10);
	struct slsi_skb_work *w = kunit_kzalloc(test, sizeof(struct slsi_skb_work), GFP_KERNEL);
	struct netdev_vif *ndev_vif;

	w->sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	w->dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	ndev_vif = netdev_priv(w->dev);

	slsi_rx_netdev_data_work(&w->work);
	kfree_skb(skb);
}
static void test_slsi_rx_queue_data(struct kunit *test)
{
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	ndev_vif->rx_data = kunit_kzalloc(test, sizeof(struct slsi_skb_work), GFP_KERNEL);

	skb->data = kunit_kzalloc(test, sizeof(struct fapi_signal), GFP_KERNEL);
	fapi_set_u16(skb, id, MA_UNITDATA_IND);
	fapi_set_u16(skb, u.ma_unitdata_ind.vif, SLSI_NAN_DATA_IFINDEX_START);
	KUNIT_EXPECT_EQ(test, 0, slsi_rx_queue_data(sdev, skb));
}
#endif

static void test_sap_ma_rx_handler(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct sk_buff *skb = fapi_alloc(ma_unitdata_ind, MA_UNITDATA_IND, 0, 10);
	struct slsi_skb_cb *skb_cb = slsi_skb_cb_get(skb);

	skb_cb->smapper_linked = true;
	fapi_set_u16(skb, id, MA_UNITDATA_IND);
	KUNIT_EXPECT_EQ(test, -EINVAL, sap_ma_rx_handler(sdev, skb));
	kfree_skb(skb);

	skb = fapi_alloc(ma_blockackreq_ind, MA_BLOCKACKREQ_IND, 0, 10);
	KUNIT_EXPECT_EQ(test, -EINVAL, sap_ma_rx_handler(sdev, skb));
	kfree_skb(skb);
}

static void test_sap_ma_txdone(struct kunit *test)
{
#ifdef CONFIG_SCSC_WLAN_TX_API
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);

	sap_ma_txdone(sdev, 0, 0);
#else
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u16 vif = SLSI_NET_INDEX_WLAN;

	sap_ma_txdone(sdev, vif, 0, 0);
#endif
}

static void test_sap_ma_init(struct kunit *test)
{
	sap_ma_init();
}

static void test_sap_ma_deinit(struct kunit *test)
{
	sap_ma_deinit();
}

//Init and Exit
static int sap_ma_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void sap_ma_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

//Kunit test cases and suite
static struct kunit_case sap_ma_test_cases[] = {
	KUNIT_CASE(test_sap_ma_notifier),
	KUNIT_CASE(test_sap_ma_version_supported),
	KUNIT_CASE(test_slsi_rx_check_mc_addr_regd),
	KUNIT_CASE(test_slsi_rx_amsdu_deaggregate),
	KUNIT_CASE(test_slsi_rx_check_opt_out_packet),
	KUNIT_CASE(test_slsi_parse_and_pull_ma_unitdata_ind_signal),
	KUNIT_CASE(test_slsi_get_msdu_frame),
	KUNIT_CASE(test_slsi_rx_data_deliver_skb),
	KUNIT_CASE(test_slsi_rx_data_ind),
#ifdef CONFIG_SCSC_WLAN_RX_NAPI
	KUNIT_CASE(test_slsi_rx_napi_process),
#else
	KUNIT_CASE(test_slsi_rx_netdev_data_work),
	KUNIT_CASE(test_slsi_rx_queue_data),
#endif
	KUNIT_CASE(test_sap_ma_rx_handler),
	KUNIT_CASE(test_sap_ma_txdone),
	KUNIT_CASE(test_sap_ma_init),
	KUNIT_CASE(test_sap_ma_deinit),
	{}
};

static struct kunit_suite sap_ma_test_suite[] = {
	{
		.name = "kunit-sap_ma-test",
		.test_cases = sap_ma_test_cases,
		.init = sap_ma_test_init,
		.exit = sap_ma_test_exit,
	}
};

kunit_test_suites(sap_ma_test_suite);
