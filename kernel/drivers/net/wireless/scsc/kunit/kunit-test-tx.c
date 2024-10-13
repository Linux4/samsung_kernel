// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-log2us.h"
#include "kunit-mock-cm_if.h"
#include "kunit-mock-txbp.h"
#include "kunit-mock-netif.h"
#ifdef CONFIG_SCSC_WLAN_HIP5
#include "kunit-mock-hip5.h"
#else
#include "kunit-mock-hip4.h"
#endif
#include "kunit-mock-scsc_wifi_fcq.h"
#include "kunit-mock-tdls_manager.h"
#include "kunit-mock-traffic_monitor.h"
#include "../tx.c"

/* Test function */
#ifdef CONFIG_SCSC_WLAN_TX_API
static void test_is_msdu_enable(struct kunit *test)
{
	is_msdu_enable();
}
#endif

static void test_slsi_get_dwell_time_for_wps(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *vif = netdev_priv(dev);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	u8 eapol[35];
	u16 eap_length = 100;

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;

	eapol[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_REQUEST;
	eapol[SLSI_EAP_TYPE_POS] = SLSI_EAP_TYPE_EXPANDED;
	eapol[SLSI_EAP_OPCODE_POS] = SLSI_EAP_OPCODE_WSC_MSG;
	eapol[SLSI_EAP_MSGTYPE_POS] = SLSI_EAP_MSGTYPE_POS;
	KUNIT_EXPECT_EQ(test, SLSI_EAP_WPS_DWELL_TIME, slsi_get_dwell_time_for_wps(sdev, vif, eapol, eap_length));

	vif->ifnum = SLSI_NET_INDEX_P2PX_SWLAN;
	eapol[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_REQUEST;
	eapol[SLSI_EAP_TYPE_POS] = SLSI_EAP_TYPE_IDENTITY;
	KUNIT_EXPECT_EQ(test, SLSI_EAP_WPS_DWELL_TIME, slsi_get_dwell_time_for_wps(sdev, vif, eapol, eap_length));
}

static void test_slsi_tx_eapol(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *vif = netdev_priv(dev);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(*peer), GFP_KERNEL);
	struct sk_buff *skb;
	u8 *ptr;
	u8 *eapol;
	u8 *data;
	int i;

	ptr = kunit_kzalloc(test, sizeof(char) * 2000, GFP_KERNEL);
	skb = (struct sk_buff *)ptr;
	data = kunit_kzalloc(test, sizeof(u8) * 1000, GFP_KERNEL);

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	vif->vif_type = FAPI_VIFTYPE_STATION;
	vif->sta.tdls_enabled = true;
	vif->peer_sta_record[0] = peer;
	vif->peer_sta_record[0]->valid = true;
	skb->head= (struct ethhdr *)data;
	skb->mac_header = 0;
	skb->protocol = htons(0x888e);
	skb->len = 114;
	skb->data = data + 10;

	for (i = 0; i < ETH_ALEN; i++) {
		eth_hdr(skb)->h_dest[i] = 0;
		vif->peer_sta_record[0]->address[i] = 0;
	}

	eapol = skb->data + sizeof(struct ethhdr);

	eapol[SLSI_EAPOL_IEEE8021X_TYPE_POS] = SLSI_IEEE8021X_TYPE_EAPOL_KEY;
	eapol[SLSI_EAPOL_TYPE_POS] = 254;
	eapol[SLSI_EAPOL_KEY_INFO_LOWER_BYTE_POS] = BIT(3);
	eapol[SLSI_EAPOL_KEY_INFO_HIGHER_BYTE_POS] = 255;
	eapol[SLSI_EAPOL_KEY_DATA_LENGTH_LOWER_BYTE_POS] = 0;
	eapol[SLSI_EAPOL_KEY_DATA_LENGTH_HIGHER_BYTE_POS] = 0;

	KUNIT_EXPECT_EQ(test, 0, slsi_tx_eapol(sdev, dev, skb));

	eapol[SLSI_EAPOL_IEEE8021X_TYPE_POS] = 4;
	eapol[SLSI_EAPOL_IEEE8021X_TYPE_POS] = SLSI_IEEE8021X_TYPE_EAP_PACKET;

	eapol[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_REQUEST;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_eapol(sdev, dev, skb));

	eapol[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_RESPONSE;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_eapol(sdev, dev, skb));

	eapol[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_SUCCESS;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_eapol(sdev, dev, skb));

	eapol[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_FAILURE;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_eapol(sdev, dev, skb));

	skb->protocol = htons(0x88b4);
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_eapol(sdev, dev, skb));

	skb->protocol = htons(0x8888);
	KUNIT_EXPECT_NE(test, 0, slsi_tx_eapol(sdev, dev, skb));
}

static void test_slsi_tx_arp(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *vif = netdev_priv(dev);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(*peer), GFP_KERNEL);
	struct sk_buff *skb;
	u8 *ptr;
	u8 *frame;
	u8 *data;

	ptr = kunit_kzalloc(test, sizeof(char) * 2000, GFP_KERNEL);
	skb = (struct sk_buff *)ptr;
	data = kunit_kzalloc(test, sizeof(u8) * 1000, GFP_KERNEL);
	skb->data = data + 10;

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	vif->vif_type = FAPI_VIFTYPE_STATION;
	vif->sta.tdls_enabled = true;
	vif->peer_sta_record[0] = peer;
	vif->peer_sta_record[0]->authorized = true;

	skb->head= (struct ethhdr *)data;

	frame = skb->data + sizeof(struct ethhdr);
	frame[SLSI_ARP_OPCODE_OFFSET] = 1;
	frame[SLSI_ARP_OPCODE_OFFSET + 1] = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_tx_arp(sdev, dev, skb));

	vif->peer_sta_record[0]->authorized = false;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_arp(sdev, dev, skb));
}

static void test_slsi_dumg_msgtype(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	u32 dhcp_msg_type;

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;

	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_DISCOVER;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_OFFER;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_REQUEST;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_DECLINE;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_ACK;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_NAK;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_RELEASE;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_INFORM;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_FORCERENEW;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
	dhcp_msg_type = SLSI_DHCP_MESSAGE_TYPE_INVALID;
	slsi_dump_msgtype(sdev, dhcp_msg_type);
}

static void test_slsi_tx_dhcp(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *vif = netdev_priv(dev);
	struct hip_priv *priv = kunit_kzalloc(test, sizeof(struct hip_priv), GFP_KERNEL);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(*peer), GFP_KERNEL);
	struct sk_buff *skb;
	u8 *ptr;
	u8 *data;

	ptr = kunit_kzalloc(test, sizeof(char) * 2000, GFP_KERNEL);
	skb = (struct sk_buff *)ptr;
	data = kunit_kzalloc(test, sizeof(u8) * 1000, GFP_KERNEL);
	skb->data = data + 10;
	skb->len = 300;

	priv->hip = &(sdev->hip);
	sdev->hip.hip_priv = priv;
	vif->vif_type = FAPI_VIFTYPE_STATION;
	vif->sta.tdls_enabled = true;
	vif->peer_sta_record[0] = peer;
	vif->peer_sta_record[0]->authorized = true;
	skb->head = (struct ethhdr *)data;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_tx_dhcp(sdev, dev, skb));

	vif->peer_sta_record[0]->authorized = false;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_dhcp(sdev, dev, skb));
}

static void test_slsi_tx_data(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *vif = netdev_priv(dev);
	struct slsi_peer *peer = kunit_kzalloc(test, sizeof(*peer), GFP_KERNEL);
	struct sk_buff *skb;
	int i;
	u8 *ptr;
	u8 *data;

	vif->peer_sta_records = 1;
	vif->peer_sta_record[0] = peer;
	vif->activated = true;

	ptr = kunit_kzalloc(test, sizeof(char) * 1000, GFP_KERNEL);
	data = kunit_kzalloc(test, sizeof(u8) * 1000, GFP_KERNEL);
	skb = (struct sk_buff *)ptr;

	skb->mac_header -= (sizeof(struct msduhdr) - sizeof(struct ethhdr));
	skb->head= (struct ethhdr *)data;
	skb->len = 10;

	vif->vif_type = FAPI_VIFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_data(sdev, dev, skb));

	vif->peer_sta_records = 0;
	vif->vif_type = FAPI_VIFTYPE_AP;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_data(sdev, dev, skb));

	vif->activated = false;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_data(sdev, dev, skb));
}

static void test_slsi_tx_data_lower(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *skb;
	u8 *ptr;
	u8 *data;

	ptr = kunit_kzalloc(test, sizeof(char) * 1000, GFP_KERNEL);
	data = kunit_kzalloc(test, sizeof(u8) * 1000, GFP_KERNEL);
	skb = (struct sk_buff *)ptr;

	skb->mac_header -= (sizeof(struct msduhdr) - sizeof(struct ethhdr));
	skb->head = (struct ethhdr *)data;
	skb->len = 10;

	KUNIT_EXPECT_EQ(test, 0, slsi_tx_data_lower(sdev, skb));
}

static void test_slsi_tx_control(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *vif = netdev_priv(dev);
	struct sk_buff *skb;
	u8 *ptr;
	u8 *data;

	ptr = kunit_kzalloc(test, sizeof(char) * 1000, GFP_KERNEL);
	skb = (struct sk_buff *)ptr;
	data = kunit_kzalloc(test, sizeof(u8) * 10, GFP_KERNEL);
	skb->head= (struct ethhdr *)data;
	skb->data = data;
	sdev->netdev[0] = dev;

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MASK;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MA;
	KUNIT_EXPECT_EQ(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MA | FAPI_SIG_TYPE_CFM;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MA | FAPI_SIG_TYPE_RES;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MA | FAPI_SIG_TYPE_IND;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MLME;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MLME | FAPI_SIG_TYPE_CFM;;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MLME | FAPI_SIG_TYPE_RES;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_MLME | FAPI_SIG_TYPE_IND;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_DEBUG;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_DEBUG | FAPI_SIG_TYPE_CFM;;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_DEBUG | FAPI_SIG_TYPE_RES;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_DEBUG | FAPI_SIG_TYPE_IND;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_TEST;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_TEST | FAPI_SIG_TYPE_CFM;;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_TEST | FAPI_SIG_TYPE_RES;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));

	(((struct fapi_signal *)(skb)->data)->id) = FAPI_SAP_TYPE_TEST | FAPI_SIG_TYPE_IND;
	KUNIT_EXPECT_NE(test, 0, slsi_tx_control(sdev, dev, skb));
}

/* Test fictures*/
static int tx_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void tx_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case tx_test_cases[] = {
	/* tx.c */
#ifdef CONFIG_SCSC_WLAN_TX_API
	KUNIT_CASE(test_is_msdu_enable),
#endif
	KUNIT_CASE(test_slsi_get_dwell_time_for_wps),
	KUNIT_CASE(test_slsi_tx_eapol),
	KUNIT_CASE(test_slsi_tx_arp),
	KUNIT_CASE(test_slsi_dumg_msgtype),
	KUNIT_CASE(test_slsi_tx_dhcp),
	KUNIT_CASE(test_slsi_tx_data),
	KUNIT_CASE(test_slsi_tx_data_lower),
	KUNIT_CASE(test_slsi_tx_control),
	{}
};

static struct kunit_suite tx_test_suite[] = {
	{
		.name = "kunit-tx-test",
		.test_cases = tx_test_cases,
		.init = tx_test_init,
		.exit = tx_test_exit,
	}
};

kunit_test_suites(tx_test_suite);
