// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-mock-kernel.h"
#include "kunit-mock-mlme.h"
#include "../log2us.c"

/* For memory free */
static void free_log2us_ctx_list(struct slsi_dev *sdev)
{
	struct buff_list *tmp_node = NULL;

	if (!sdev)
		return;

	while (sdev->conn_log2us_ctx.list) {
		tmp_node = sdev->conn_log2us_ctx.list;
		sdev->conn_log2us_ctx.list = sdev->conn_log2us_ctx.list->next;
		kfree(tmp_node);
	}

	while (sdev->conn_log2us_ctx.list_roam_cand) {
		tmp_node = sdev->conn_log2us_ctx.list_roam_cand;
		sdev->conn_log2us_ctx.list_roam_cand = sdev->conn_log2us_ctx.list_roam_cand->next;
		kfree(tmp_node);
	}
}

/* unit test function definition */
static void test_delete_node(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	sdev->conn_log2us_ctx.list = kzalloc(sizeof(struct buff_list), GFP_KERNEL);

	delete_node(sdev);
}

static void test_slsi_send_conn_log_event(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	sdev->wiphy = kunit_kzalloc(test, sizeof(struct wiphy), GFP_KERNEL);
	sdev->wiphy->n_vendor_events = SLSI_NL80211_VENDOR_TWT_NOTIFICATION_EVENT +1;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_send_conn_log_event(sdev));

	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	KUNIT_EXPECT_EQ(test, -1, slsi_send_conn_log_event(sdev));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, -1, slsi_send_conn_log_event(sdev));

	sdev->conn_log2us_ctx.list = kzalloc(sizeof(struct buff_list), GFP_KERNEL);
	sdev->conn_log2us_ctx.list->next = NULL;
	KUNIT_EXPECT_EQ(test, 0, slsi_send_conn_log_event(sdev));
}

static void test_enqueue_log_buffer_for_roam_cand(struct kunit *test)
{
	struct buff_list *new_node = kunit_kzalloc(test, sizeof(struct buff_list), GFP_KERNEL);
	struct conn_log2us *ctx = kunit_kzalloc(test, sizeof(struct conn_log2us), GFP_KERNEL);

	enqueue_log_buffer_for_roam_cand(new_node, ctx);

	ctx->list_roam_cand = kunit_kzalloc(test, sizeof(struct buff_list), GFP_KERNEL);
	ctx->list_roam_cand->next = kunit_kzalloc(test, sizeof(struct buff_list), GFP_KERNEL);
	enqueue_log_buffer_for_roam_cand(new_node, ctx);
}

static void test_enqueue_log_buffer(struct kunit *test)
{
	struct buff_list *new_node = kunit_kzalloc(test, sizeof(struct buff_list), GFP_KERNEL);
	struct conn_log2us *ctx = kunit_kzalloc(test, sizeof(struct conn_log2us), GFP_KERNEL);

	enqueue_log_buffer(new_node, ctx);
}

static void test_conn_log_worker(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	sdev->wiphy = kunit_kzalloc(test, sizeof(struct wiphy), GFP_KERNEL);
	sdev->wiphy->n_vendor_events = SLSI_NL80211_VENDOR_TWT_NOTIFICATION_EVENT +1;
	sdev->netdev[SLSI_NET_INDEX_WLAN] = dev;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;

	sdev->conn_log2us_ctx.list = kzalloc(sizeof(struct buff_list), GFP_KERNEL);
	sdev->conn_log2us_ctx.list->next = NULL;
	sdev->conn_log2us_ctx.stop = false;

	conn_log_worker(&sdev->conn_log2us_ctx.log2us_work);
}

static void test_slsi_conn_log2us_init(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	slsi_conn_log2us_init(sdev);
}

static void test_slsi_conn_log2us_deinit(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	sdev->conn_log2us_ctx.list = kzalloc(sizeof(struct buff_list), GFP_KERNEL);
	sdev->conn_log2us_ctx.list->next = NULL;
	sdev->conn_log2us_ctx.list_roam_cand = kzalloc(sizeof(struct buff_list), GFP_KERNEL);
	sdev->conn_log2us_ctx.list_roam_cand->next = NULL;

	slsi_conn_log2us_deinit(sdev);
}

static void test_get_eap_type_from_val(struct kunit *test)
{
	int val = SLSI_EAP_TYPE_IDENTITY;
	u8 *str = kunit_kzalloc(test, sizeof(u8) * 20, GFP_KERNEL);

	KUNIT_EXPECT_STREQ(test, "IDENTITY", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_NOTIFICATION;
	KUNIT_EXPECT_STREQ(test, "NOTIFICATION", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_NAK;
	KUNIT_EXPECT_STREQ(test, "NAK", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_MD5;
	KUNIT_EXPECT_STREQ(test, "MD5", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_OTP;
	KUNIT_EXPECT_STREQ(test, "OTP", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_GTC;
	KUNIT_EXPECT_STREQ(test, "GTC", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_TLS;
	KUNIT_EXPECT_STREQ(test, "TLS", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_LEAP;
	KUNIT_EXPECT_STREQ(test, "LEAP", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_SIM;
	KUNIT_EXPECT_STREQ(test, "SIM", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_TTLS;
	KUNIT_EXPECT_STREQ(test, "TTLS", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_AKA;
	KUNIT_EXPECT_STREQ(test, "AKA", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_PEAP;
	KUNIT_EXPECT_STREQ(test, "PEAP", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_MSCHAPV2;
	KUNIT_EXPECT_STREQ(test, "MSCHAPV2", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_TLV;
	KUNIT_EXPECT_STREQ(test, "TLV", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_TNC;
	KUNIT_EXPECT_STREQ(test, "TNC", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_FAST;
	KUNIT_EXPECT_STREQ(test, "FAST", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_PAX;
	KUNIT_EXPECT_STREQ(test, "PAX", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_PSK;
	KUNIT_EXPECT_STREQ(test, "PSK", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_SAKE;
	KUNIT_EXPECT_STREQ(test, "SAKE", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_IKEV2;
	KUNIT_EXPECT_STREQ(test, "IKEV2", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_AKA_PRIME;
	KUNIT_EXPECT_STREQ(test, "AKA PRIME", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_GPSK;
	KUNIT_EXPECT_STREQ(test, "GPSK", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_PWD;
	KUNIT_EXPECT_STREQ(test, "PWD", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_EKE;
	KUNIT_EXPECT_STREQ(test, "EKE", get_eap_type_from_val(val, str));

	val = SLSI_EAP_TYPE_EXPANDED;
	KUNIT_EXPECT_STREQ(test, "EXPANDED", get_eap_type_from_val(val, str));

	val = 255;
	KUNIT_EXPECT_STREQ(test, "UNKNOWN", get_eap_type_from_val(val, str));
}

static void test_slsi_eapol_eap_handle_tx_status(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u16 host_tag = 0x1234;
	sdev->conn_log2us_ctx.host_tag_eap = host_tag;
	sdev->conn_log2us_ctx.host_tag_eap_type = SLSI_IEEE8021X_TYPE_EAP_PACKET;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;

	slsi_eapol_eap_handle_tx_status(sdev, ndev_vif, host_tag, FAPI_TRANSMISSIONSTATUS_SUCCESSFUL);

	free_log2us_ctx_list(sdev);

	sdev->conn_log2us_ctx.host_tag_eap = host_tag;
	sdev->conn_log2us_ctx.host_tag_eap_type = SLSI_IEEE8021X_TYPE_EAP_START;
	slsi_eapol_eap_handle_tx_status(sdev, ndev_vif, host_tag, FAPI_TRANSMISSIONSTATUS_RETRY_LIMIT);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_log2us_get_rssi(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int mib_value;
	u16 psid = 0;

	sdev->recovery_timeout = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_log2us_get_rssi(sdev, dev, &mib_value, psid));

	sdev->recovery_timeout = 2;
	KUNIT_EXPECT_EQ(test, 0, slsi_log2us_get_rssi(sdev, dev, &mib_value, psid));

	sdev->recovery_timeout = 3;
	KUNIT_EXPECT_EQ(test, 0, slsi_log2us_get_rssi(sdev, dev, &mib_value, psid));
}

static void test_slsi_conn_log2us_alloc_new_node(struct kunit *test)
{
	struct buff_list *new_node = (struct buff_list *)slsi_conn_log2us_alloc_new_node();

	KUNIT_EXPECT_PTR_NE(test, NULL, new_node);

	if (new_node)
		kfree(new_node);
}

static void test_slsi_conn_log2us_alloc_atomic_new_node(struct kunit *test)
{
	struct buff_list *new_node = (struct buff_list *)slsi_conn_log2us_alloc_atomic_new_node();

	KUNIT_EXPECT_PTR_NE(test, NULL, new_node);

	if (new_node)
		kfree(new_node);
}

static void test_slsi_conn_log2us_connecting(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params), GFP_KERNEL);
	const u8 ie[12] = {0, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	sme->ie = ie;
	sme->ie_len = 15;
	sme->ssid = "log2us-ap";
	sme->ssid_len = 11;
	sme->bssid = "00:11:22:33:44:55";
	sme->bssid_hint = "log2us";
	sme->channel = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	sme->channel->center_freq = 2412;
	sme->channel_hint = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	sme->channel_hint->center_freq = 2412;
	sme->auth_type = NL80211_AUTHTYPE_SAE;
	sme->crypto.n_ciphers_pairwise = 1;
	sme->crypto.cipher_group = 1;
	sme->crypto.n_akm_suites = 1;

	sdev->conn_log2us_ctx.btcoex_hid = 1;
	slsi_conn_log2us_connecting(sdev, dev, sme);

	free_log2us_ctx_list(sdev);

	ndev_vif->iftype = NL80211_IFTYPE_UNSPECIFIED;
	slsi_conn_log2us_connecting(sdev, dev, sme);
}

static void test_slsi_conn_log2us_connecting_fail(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	const unsigned char *bssid = "00:11:22:33:44:55";
	int freq = 0;
	int reason = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_connecting_fail(sdev, dev, bssid, freq, reason);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_disconnect(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	const unsigned char *bssid = "00:11:22:33:44:55";
	int reason = 100;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	slsi_conn_log2us_disconnect(sdev, dev, bssid, reason);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_disconnect(sdev, dev, bssid, reason);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_eapol_gtk(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int eapol_msg_type = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_eapol_gtk(sdev, dev, eapol_msg_type);
	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_eapol_gtk_tx(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u32 status_code = 0;

	slsi_conn_log2us_eapol_gtk_tx(sdev, status_code);
	free_log2us_ctx_list(sdev);

	status_code = FAPI_TRANSMISSIONSTATUS_RETRY_LIMIT;
	slsi_conn_log2us_eapol_gtk_tx(sdev, status_code);
	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_eapol_ptk(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int eapol_msg_type = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_eapol_ptk(sdev, dev, eapol_msg_type);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_eapol_ptk_tx(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	u32 status_code = 0;

	slsi_conn_log2us_eapol_ptk_tx(sdev, status_code);
	free_log2us_ctx_list(sdev);

	status_code = FAPI_TRANSMISSIONSTATUS_RETRY_LIMIT;
	slsi_conn_log2us_eapol_ptk_tx(sdev, status_code);
	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_eapol_tx(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	u32 status_code = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	sdev->conn_log2us_ctx.is_eapol_ptk = true;
	slsi_conn_log2us_eapol_tx(sdev, dev, status_code);

	free_log2us_ctx_list(sdev);

	sdev->conn_log2us_ctx.is_eapol_gtk = true;
	slsi_conn_log2us_eapol_tx(sdev, dev, status_code);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_roam_scan_start(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int reason = 0;
	int roam_rssi_val = 0;
	int chan_utilisation = 0;
	int rssi_thresh = 0;
	u64 timestamp = ktime_get_real_ns();
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_roam_scan_start(sdev, dev, reason, roam_rssi_val,
					 chan_utilisation, rssi_thresh, timestamp);
	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_roam_result(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	char *bssid = "00:11:22:33:44:55";
	u64 timestamp = ktime_get_real_ns();
	bool roam_candidate = true;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_roam_result(sdev, dev, bssid, timestamp, roam_candidate);
	free_log2us_ctx_list(sdev);

	roam_candidate = false;
	slsi_conn_log2us_roam_result(sdev, dev, bssid, timestamp, roam_candidate);
	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_eap_with_len(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	u8 eap[SLSI_EAP_CODE_POS + 1] = {0, 0, 0, 0, SLSI_EAP_PACKET_REQUEST};
	int eap_length = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_eap_with_len(sdev, dev, eap, eap_length);
	free_log2us_ctx_list(sdev);

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	slsi_conn_log2us_eap_with_len(sdev, dev, eap, eap_length);
}

static void test_slsi_conn_log2us_eap_tx(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int eap_length = 0;
	int eap_type = 0;
	char *tx_status_str = "";

	slsi_conn_log2us_eap_tx(sdev, ndev_vif, eap_length, eap_type, tx_status_str);
	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_eap(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	u8 eap[SLSI_EAP_CODE_POS +1];
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	eap[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_SUCCESS;
	slsi_conn_log2us_eap(sdev, dev, eap);
	free_log2us_ctx_list(sdev);

	eap[SLSI_EAP_CODE_POS] = SLSI_EAP_PACKET_FAILURE;
	slsi_conn_log2us_eap(sdev, dev, eap);
	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_dhcp(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	char *str = "";
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_dhcp(sdev, dev, str);
	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_dhcp_tx(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	char *str = "";
	char *tx_status = "";
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_dhcp_tx(sdev, dev, str, tx_status);
	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_auth_req(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	const unsigned char *bssid = "00:11:22:33:44:55";
	int auth_algo = 0;
	int sae_type = 0;
	int sn = 0;
	int status = 0;
	u32 tx_status = FAPI_RESULTCODE_AUTH_TX_FAIL;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	slsi_conn_log2us_auth_req(sdev, dev, bssid, auth_algo, sae_type, sn, status, tx_status, true);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_auth_req(sdev, dev, bssid, auth_algo, sae_type, sn, status, tx_status, true);

	free_log2us_ctx_list(sdev);

	tx_status = FAPI_RESULTCODE_AUTH_NO_ACK;
	slsi_conn_log2us_auth_req(sdev, dev, bssid, auth_algo, sae_type, sn, status, tx_status, false);

	free_log2us_ctx_list(sdev);

	tx_status = FAPI_RESULTCODE_AUTH_TIMEOUT;
	slsi_conn_log2us_auth_req(sdev, dev, bssid, auth_algo, sae_type, sn, status, tx_status, false);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_auth_resp(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	const unsigned char *bssid = "00:11:22:33:44:55";
	int auth_algo = 0;
	int sae_type = 0;
	int sn = 0;
	int status = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_auth_resp(sdev, dev, bssid, auth_algo, sae_type, sn, status, true);

	free_log2us_ctx_list(sdev);

	slsi_conn_log2us_auth_resp(sdev, dev, bssid, auth_algo, sae_type, sn, status, false);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_assoc_req(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	const unsigned char *bssid = "00:11:22:33:44:55";
	int sn = 0;
	int tx_status = FAPI_RESULTCODE_ASSOC_TX_FAIL;
	int mgmt_frame_subtype = SLSI_MGMT_FRAME_SUBTYPE_ASSOC_REQ;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	slsi_conn_log2us_assoc_req(sdev, dev, bssid, sn, tx_status, mgmt_frame_subtype);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_assoc_req(sdev, dev, bssid, sn, tx_status, mgmt_frame_subtype);

	free_log2us_ctx_list(sdev);

	mgmt_frame_subtype = SLSI_MGMT_FRAME_SUBTYPE_REASSOC_REQ;
	slsi_conn_log2us_assoc_req(sdev, dev, bssid, sn, tx_status, mgmt_frame_subtype);

	free_log2us_ctx_list(sdev);

	tx_status = FAPI_RESULTCODE_ASSOC_NO_ACK;
	slsi_conn_log2us_assoc_req(sdev, dev, bssid, sn, tx_status, mgmt_frame_subtype);

	free_log2us_ctx_list(sdev);

	tx_status = FAPI_RESULTCODE_ASSOC_TIMEOUT;
	slsi_conn_log2us_assoc_req(sdev, dev, bssid, sn, tx_status, mgmt_frame_subtype);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_assoc_resp(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	const unsigned char *bssid = "00:11:22:33:44:55";
	int sn = 0;
	int status = 0;
	int mgmt_frame_subtype = SLSI_MGMT_FRAME_SUBTYPE_ASSOC_RESP;
	int aid = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_assoc_resp(sdev, dev, bssid, sn, status, mgmt_frame_subtype, aid);

	free_log2us_ctx_list(sdev);

	mgmt_frame_subtype = SLSI_MGMT_FRAME_SUBTYPE_REASSOC_RESP;
	slsi_conn_log2us_assoc_resp(sdev, dev, bssid, sn, status, mgmt_frame_subtype, aid);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_deauth(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	char *str_type = "deauth";
	const unsigned char *bssid = "00:11:22:33:44:55";
	int sn = 0;
	int status = 0;
	char *vs_ie = NULL;

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	slsi_conn_log2us_deauth(sdev, dev, str_type, bssid, sn, status, vs_ie);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_deauth(sdev, dev, str_type, bssid, sn, status, vs_ie);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_disassoc(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	char *str_type = "deauth";
	const unsigned char *bssid = "00:11:22:33:44:55";
	int sn = 0;
	int status = 0;
	char *vs_ie = NULL;

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	slsi_conn_log2us_disassoc(sdev, dev, str_type, bssid, sn, status, vs_ie);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_disassoc(sdev, dev, str_type, bssid, sn, status, vs_ie);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_roam_scan_save(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int scan_type = FAPI_SCANTYPE_SOFT_CACHED_ROAMING_SCAN;
	int freq_count = 1;
	int freq_list[1] = {0,};
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_roam_scan_save(sdev, dev, scan_type, freq_count, freq_list);

	scan_type = FAPI_SCANTYPE_SOFT_FULL_ROAMING_SCAN;
	slsi_conn_log2us_roam_scan_save(sdev, dev, scan_type, freq_count, freq_list);
}

static void test_slsi_conn_log2us_roam_scan_done(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	u64 timestamp = ktime_get_real_ns();
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	sdev->conn_log2us_ctx.btcoex_hid = 1;
	sdev->conn_log2us_ctx.roam_freq_count = 1;
	sdev->conn_log2us_ctx.list_roam_cand = slsi_conn_log2us_alloc_new_node();
	slsi_conn_log2us_roam_scan_done(sdev, dev, timestamp);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_roam_scan_result(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	char *bssid = "00:11:22:33:44:55";
	int freq = 2412;
	int rssi = -70;
	int cu = 1;
	int score = 5;
	int tp_score = 3;
	bool eligible = true;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_roam_scan_result(sdev, dev, true, bssid, freq, rssi, cu, score,
						tp_score, eligible);

	free_log2us_ctx_list(sdev);

	slsi_conn_log2us_roam_scan_result(sdev, dev, false, bssid, freq, rssi, cu, score,
						tp_score, eligible);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_btm_query(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int dialog = 0;
	int reason = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_btm_query(sdev, dev, dialog, reason);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_btm_req(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int dialog = 0;
	int btm_mode = 0;
	int disassoc_timer = 0;
	int validity_time = 0;
	int candidate_count = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_btm_req(sdev, dev, dialog, btm_mode, disassoc_timer,
				 validity_time, candidate_count);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_btm_resp(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int dialog = 0;
	int btm_mode = 0;
	int delay = 0;
	char *bssid = "00:11:22:33:44:55";
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_btm_resp(sdev, dev, dialog, btm_mode, delay, bssid);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_btm_cand(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	char *bssid = "00:11:22:33:44:55";
	int prefer = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_btm_cand(sdev, dev, bssid, prefer);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_nr_frame_req(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	int dialog_token = 0;
	char *ssid = "";

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_nr_frame_req(sdev, dev, dialog_token, ssid);

	slsi_conn_log2us_nr_frame_req(sdev, dev, dialog_token, NULL);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_nr_frame_resp(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int dialog_token = 0;
	int freq_count = 2;
	int freq_list[2] = {10, 100};
	int report_number = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_nr_frame_resp(sdev, dev, dialog_token, freq_count, freq_list, report_number);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_beacon_report_request(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int dialog_token = 0;
	int operating_class = 0;
	char *string = "log2us";
	int measure_duration = 0;
	char *measure_mode = "log2us";
	u8 request_mode = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_beacon_report_request(sdev, dev, dialog_token, operating_class, string,
					       measure_duration, measure_mode, request_mode);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_beacon_report_response(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	int dialog_token = 0;
	int reason_code = 0;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_conn_log2us_beacon_report_response(sdev, dev, dialog_token, reason_code);

	free_log2us_ctx_list(sdev);
}

static void test_slsi_conn_log2us_ncho_mode(struct kunit *test)
{
	struct slsi_dev *sdev = kunit_kzalloc(test, sizeof(struct slsi_dev), GFP_KERNEL);
	struct net_device *dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->iftype = NL80211_IFTYPE_STATION;

	slsi_conn_log2us_ncho_mode(sdev, dev, true);

	free_log2us_ctx_list(sdev);
}


/* Test fictures */
static int log2us_test_init(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void log2us_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case log2us_test_cases[] = {
	KUNIT_CASE(test_delete_node),
	KUNIT_CASE(test_slsi_send_conn_log_event),
	KUNIT_CASE(test_enqueue_log_buffer_for_roam_cand),
	KUNIT_CASE(test_enqueue_log_buffer),
	KUNIT_CASE(test_conn_log_worker),
	KUNIT_CASE(test_slsi_conn_log2us_init),
	KUNIT_CASE(test_slsi_conn_log2us_deinit),
	KUNIT_CASE(test_get_eap_type_from_val),
	KUNIT_CASE(test_slsi_eapol_eap_handle_tx_status),
	KUNIT_CASE(test_slsi_log2us_get_rssi),
	KUNIT_CASE(test_slsi_conn_log2us_alloc_new_node),
	KUNIT_CASE(test_slsi_conn_log2us_alloc_atomic_new_node),
	KUNIT_CASE(test_slsi_conn_log2us_connecting),
	KUNIT_CASE(test_slsi_conn_log2us_connecting_fail),
	KUNIT_CASE(test_slsi_conn_log2us_disconnect),
	KUNIT_CASE(test_slsi_conn_log2us_eapol_gtk),
	KUNIT_CASE(test_slsi_conn_log2us_eapol_gtk_tx),
	KUNIT_CASE(test_slsi_conn_log2us_eapol_ptk),
	KUNIT_CASE(test_slsi_conn_log2us_eapol_ptk_tx),
	KUNIT_CASE(test_slsi_conn_log2us_eapol_tx),
	KUNIT_CASE(test_slsi_conn_log2us_roam_scan_start),
	KUNIT_CASE(test_slsi_conn_log2us_roam_result),
	KUNIT_CASE(test_slsi_conn_log2us_eap_with_len),
	KUNIT_CASE(test_slsi_conn_log2us_eap_tx),
	KUNIT_CASE(test_slsi_conn_log2us_eap),
	KUNIT_CASE(test_slsi_conn_log2us_dhcp),
	KUNIT_CASE(test_slsi_conn_log2us_dhcp_tx),
	KUNIT_CASE(test_slsi_conn_log2us_auth_req),
	KUNIT_CASE(test_slsi_conn_log2us_auth_resp),
	KUNIT_CASE(test_slsi_conn_log2us_assoc_req),
	KUNIT_CASE(test_slsi_conn_log2us_assoc_resp),
	KUNIT_CASE(test_slsi_conn_log2us_deauth),
	KUNIT_CASE(test_slsi_conn_log2us_disassoc),
	KUNIT_CASE(test_slsi_conn_log2us_roam_scan_save),
	KUNIT_CASE(test_slsi_conn_log2us_roam_scan_done),
	KUNIT_CASE(test_slsi_conn_log2us_roam_scan_result),
	KUNIT_CASE(test_slsi_conn_log2us_btm_query),
	KUNIT_CASE(test_slsi_conn_log2us_btm_req),
	KUNIT_CASE(test_slsi_conn_log2us_btm_resp),
	KUNIT_CASE(test_slsi_conn_log2us_btm_cand),
	KUNIT_CASE(test_slsi_conn_log2us_nr_frame_req),
	KUNIT_CASE(test_slsi_conn_log2us_nr_frame_resp),
	KUNIT_CASE(test_slsi_conn_log2us_beacon_report_request),
	KUNIT_CASE(test_slsi_conn_log2us_beacon_report_response),
	KUNIT_CASE(test_slsi_conn_log2us_ncho_mode),
	{}
};

static struct kunit_suite log2us_test_suite[] = {
	{
		.name = "log2us-test",
		.test_cases = log2us_test_cases,
		.init = log2us_test_init,
		.exit = log2us_test_exit,
	}
};

kunit_test_suites(log2us_test_suite);
