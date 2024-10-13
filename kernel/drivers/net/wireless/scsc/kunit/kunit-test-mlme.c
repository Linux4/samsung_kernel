#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-cm_if.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-misc.h"
#include "kunit-mock-tx.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-hip.h"
#include "kunit-mock-dev.h"
#include "kunit-mock-mib.h"
#include "kunit-mock-rx.h"
#ifdef CONFIG_SCSC_WLAN_HIP5
#include "kunit-mock-hip5.h"
#else
#include "kunit-mock-hip4.h"
#endif
#include "../mlme.c"

bool kn_validate_cfm_wait_ind(struct slsi_dev *sdev, struct net_device *dev, struct sk_buff *cfm)
{
	return true;
}

static void test_slsi_mlme_wait_for_cfm(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_sig_send *sig_wait = kunit_kzalloc(test, sizeof(struct slsi_sig_send), GFP_KERNEL);
	int timeout = 50;

	sdev->sig_wait_cfm_timeout = &timeout;
	sig_wait->cfm_id = 0x2105;
	sig_wait->req_id = 0x8765;
	sig_wait->process_id = 0xa8bf;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_wait_for_cfm(sdev, sig_wait));

	*sdev->sig_wait_cfm_timeout = 173;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_wait_for_cfm(sdev, sig_wait));

	sig_wait->cfm = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	sig_wait->cfm->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);

	(((struct fapi_signal *)(sig_wait->cfm)->data)->receiver_pid) = 0xCF01;
	(((struct fapi_signal *)(sig_wait->cfm)->data)->id) = FAPI_SAP_TYPE_MA;

	KUNIT_EXPECT_TRUE(test, slsi_mlme_wait_for_cfm(sdev, sig_wait));
}

static void test_panic_on_lost_ind(struct kunit *test)
{
	u16 ind_id = MLME_SCAN_DONE_IND;

	KUNIT_EXPECT_FALSE(test, panic_on_lost_ind(ind_id));

	ind_id = MLME_CONNECTED_IND;

	KUNIT_EXPECT_TRUE(test, panic_on_lost_ind(ind_id));
}

static void test_slsi_mlme_wait_for_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_sig_send *sig_wait = kunit_kzalloc(test, sizeof(struct slsi_sig_send), GFP_KERNEL);
	u16 ind_id = MLME_SCAN_DONE_IND;
	int timeout = 0;

	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	sdev->sig_wait_cfm_timeout = &timeout;
	sdev->device_config.ap_disconnect_ind_timeout = 123;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_wait_for_ind(sdev, dev, sig_wait, ind_id));

	ind_id = MLME_DISCONNECTED_IND;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_wait_for_ind(sdev, dev, sig_wait, ind_id));

	ind_id = MLME_NAN_EVENT_IND;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_wait_for_ind(sdev, dev, sig_wait, ind_id));

	ind_id = MLME_CONNECTED_IND;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_wait_for_ind(sdev, dev, sig_wait, ind_id));

	sig_wait->ind = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	sig_wait->ind->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);

	(((struct fapi_signal *)(sig_wait->ind)->data)->receiver_pid) = 0xCF01;

	KUNIT_EXPECT_TRUE(test, slsi_mlme_wait_for_ind(sdev, dev, sig_wait, ind_id));
}

static void test_slsi_mlme_tx_rx(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	struct sk_buff *mib_error = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 cfm_id = 0xa7c9;
	u16 ind_id = 0xa9b8;

	sdev->sig_wait_cfm_timeout = kunit_kzalloc(test, sizeof(int), GFP_KERNEL);
	ndev_vif->sig_wait.cfm = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->sig_wait.cfm->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(ndev_vif->sig_wait.cfm)->data)->receiver_pid) = 0xCF01;
	(((struct fapi_signal *)(ndev_vif->sig_wait.cfm)->data)->id) = 0xCF01;

	ndev_vif->sig_wait.mib_error = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->sig_wait.ind = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->sig_wait.ind->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(ndev_vif->sig_wait.ind)->data)->receiver_pid) = 0xCF01;
	(((struct fapi_signal *)(ndev_vif->sig_wait.ind)->data)->id) = 0xCF01;

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(skb)->data)->id) = 0xCF01;

	sdev->mlme_blocked = true;
	sdev->wlan_service_on = false;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_tx_rx(sdev, dev, skb, cfm_id, &mib_error, ind_id, kn_validate_cfm_wait_ind));

	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(skb)->data)->id) = 0xCF01;
	(((struct fapi_signal *)(skb)->data)->sender_pid) = 0xCF01;

	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	ndev_vif->sig_wait.process_id = 0xFFCC;
	ndev_vif->mgmt_tx_cookie = 0x9876;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_tx_rx(sdev, dev, skb, cfm_id, &mib_error, ind_id, kn_validate_cfm_wait_ind));

	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	ndev_vif->mgmt_tx_cookie = 0x1234;
	ndev_vif->sig_wait.process_id = 0xFFCC;
	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(skb)->data)->id) = 0xCF01;
	(((struct fapi_signal *)(skb)->data)->sender_pid) = 0xCF01;

	ndev_vif->sig_wait.cfm = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->sig_wait.cfm->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(ndev_vif->sig_wait.cfm)->data)->receiver_pid) = 0xCF01;
	(((struct fapi_signal *)(ndev_vif->sig_wait.cfm)->data)->id) = 0xCF01;

	ndev_vif->sig_wait.mib_error = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->sig_wait.ind = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->sig_wait.ind->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(ndev_vif->sig_wait.ind)->data)->receiver_pid) = 0xCF01;
	(((struct fapi_signal *)(ndev_vif->sig_wait.ind)->data)->id) = 0xCF01;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_tx_rx(sdev, dev, skb, cfm_id, &mib_error, ind_id, kn_validate_cfm_wait_ind));

	cfm_id = 0;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	ndev_vif->mgmt_tx_cookie = 0x1234;
	ndev_vif->sig_wait.process_id = 0xFF00;
	skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(skb)->data)->id) = 0xCF01;
	(((struct fapi_signal *)(skb)->data)->sender_pid) = 0xCF01;

	ndev_vif->sig_wait.cfm = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->sig_wait.cfm->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(ndev_vif->sig_wait.cfm)->data)->receiver_pid) = 0xCF01;
	(((struct fapi_signal *)(ndev_vif->sig_wait.cfm)->data)->id) = 0xCF01;

	ndev_vif->sig_wait.mib_error = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->sig_wait.ind = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->sig_wait.ind->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(ndev_vif->sig_wait.ind)->data)->receiver_pid) = 0xCF01;
	(((struct fapi_signal *)(ndev_vif->sig_wait.ind)->data)->id) = 0xCF01;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_tx_rx(sdev, dev, skb, cfm_id, NULL, ind_id, kn_validate_cfm_wait_ind));
}

static void test_slsi_mlme_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->sig_wait.process_id = 0xFFCC;
	ndev_vif->mgmt_tx_cookie = 0x9876;

	skb->data = (char*)kunit_kzalloc(test, sizeof(struct fapi_signal) + 100, GFP_KERNEL);
	(((struct fapi_signal *)(skb)->data)->id) = 0xCF01;
	(((struct fapi_signal *)(skb)->data)->sender_pid) = 0xCF01;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_req(sdev, dev, skb));
}

static void test_slsi_mlme_req_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);
	u16 ind_id = 0x9ac8;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_ind(sdev, dev, skb, 0));

	sdev->mlme_blocked = true;
	sdev->wlan_service_on = false;
	skb = alloc_skb(100, GFP_KERNEL);

	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_ind(sdev, dev, skb, ind_id));
	kfree_skb(skb);
}

static void test_slsi_mlme_req_no_cfm(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);

	sdev->mlme_blocked = true;
	sdev->wlan_service_on = false;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_no_cfm(sdev, dev, skb));
}

static void test_slsi_mlme_req_cfm(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);
	u16 cfm_id = 0x9ac8;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_cfm(sdev, dev, skb, 0));

	sdev->mlme_blocked = true;
	sdev->wlan_service_on = false;
	skb = alloc_skb(100, GFP_KERNEL);

	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_cfm(sdev, dev, skb, cfm_id));
	kfree_skb(skb);
}

static void test_slsi_mlme_req_cfm_mib(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);
	struct sk_buff *mib_error = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 cfm_id = 0;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_cfm_mib(sdev, dev, skb, cfm_id, NULL));

	cfm_id = 0x9ac8;
	skb = alloc_skb(100, GFP_KERNEL);

	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_cfm_mib(sdev, dev, skb, cfm_id, NULL));

	cfm_id = 0x9ac8;
	skb = alloc_skb(100, GFP_KERNEL);
	sdev->mlme_blocked = true;
	sdev->wlan_service_on = false;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_cfm_mib(sdev, dev, skb, cfm_id, &mib_error));
	kfree_skb(skb);
}

static void test_slsi_mlme_req_cfm_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = alloc_skb(100, GFP_KERNEL);
	u16 cfm_id = 0;
	u16 ind_id = 0;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_cfm_ind(sdev, dev, skb, cfm_id, ind_id, kn_validate_cfm_wait_ind));

	cfm_id = 0x1234;
	skb = alloc_skb(100, GFP_KERNEL);
	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_cfm_ind(sdev, dev, skb, cfm_id, ind_id, kn_validate_cfm_wait_ind));

	ind_id = 0xA24B;
	skb = alloc_skb(100, GFP_KERNEL);
	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_cfm_ind(sdev, dev, skb, cfm_id, ind_id, NULL));

	skb = alloc_skb(100, GFP_KERNEL);
	sdev->mlme_blocked = true;
	sdev->wlan_service_on = false;
	KUNIT_EXPECT_FALSE(test, slsi_mlme_req_cfm_ind(sdev, dev, skb, cfm_id, ind_id, kn_validate_cfm_wait_ind));
}

static void test_slsi_get_reg_rule(struct kunit *test)
{
	u32 center_freq = 5180000;
	struct slsi_802_11d_reg_domain *domain_info = kunit_kzalloc(test, sizeof(struct slsi_802_11d_reg_domain),
								    GFP_KERNEL);

	domain_info->regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
					       sizeof(struct ieee80211_reg_rule) * 2, GFP_KERNEL);
	domain_info->regdomain->n_reg_rules = 1;
	domain_info->regdomain->reg_rules[0].freq_range.start_freq_khz = 5170000;
	domain_info->regdomain->reg_rules[0].freq_range.end_freq_khz = 5190000;

	KUNIT_EXPECT_TRUE(test, slsi_get_reg_rule(center_freq, domain_info));
}

static void test_slsi_compute_chann_info(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u16 width = NL80211_CHAN_WIDTH_20;
	u16 center_freq0 = 5200;
	u16 channel_freq = 5210;

	KUNIT_EXPECT_EQ(test, 20, slsi_compute_chann_info(sdev, width, center_freq0, channel_freq));

	width = NL80211_CHAN_WIDTH_40;

	KUNIT_EXPECT_EQ(test, 296, slsi_compute_chann_info(sdev, width, center_freq0, channel_freq));

	width = NL80211_CHAN_WIDTH_80;
	channel_freq = 5270;

	KUNIT_EXPECT_EQ(test, 80, slsi_compute_chann_info(sdev, width, center_freq0, channel_freq));

	width = NL80211_CHAN_WIDTH_160;
	channel_freq = 5320;

	KUNIT_EXPECT_EQ(test, 160, slsi_compute_chann_info(sdev, width, center_freq0, channel_freq));

	width = NL80211_CHAN_WIDTH_160 + NL80211_CHAN_WIDTH_80;

	KUNIT_EXPECT_EQ(test, 0, slsi_compute_chann_info(sdev, width, center_freq0, channel_freq));
}

static void test_slsi_get_chann_info(struct kunit *test)
{
	struct cfg80211_chan_def *chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);

	chandef->width = NL80211_CHAN_WIDTH_20;

	KUNIT_EXPECT_EQ(test, 20, slsi_get_chann_info(NULL, chandef));

	chandef->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);

	chandef->width = NL80211_CHAN_WIDTH_80;
	chandef->chan->center_freq = 5270;
	chandef->center_freq1 = 5200;

	KUNIT_EXPECT_EQ(test, 80, slsi_get_chann_info(NULL, chandef));
}

static void test_slsi_check_channelization(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct cfg80211_chan_def *chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);
	int wscs = 1;

	chandef->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	chandef->width = NL80211_CHAN_WIDTH_20;
	chandef->chan->center_freq = 5745;
	sdev->wiphy = NULL;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_check_channelization(sdev, chandef, wscs));

	sdev->wiphy = ndev_vif->wdev.wiphy;
	chandef->width = NL80211_CHAN_WIDTH_40;
	sdev->wiphy->bands[NL80211_BAND_5GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->n_channels = 1;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].center_freq = 5745;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].freq_offset = 0;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].flags = IEEE80211_CHAN_DISABLED;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_check_channelization(sdev, chandef, wscs));

	chandef->width = NL80211_CHAN_WIDTH_80;
	sdev->wiphy->bands[NL80211_BAND_5GHZ]->channels[0].flags = IEEE80211_CHAN_16MHZ;
	wscs = 0;
	sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
								  sizeof(struct ieee80211_reg_rule) * 2, GFP_KERNEL);
	sdev->device_config.domain_info.regdomain->n_reg_rules = 1;
	sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.start_freq_khz = 5170000;
	sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.end_freq_khz = 5190000;
	sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.max_bandwidth_khz = 160000;
	chandef->center_freq1 = 5160;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_check_channelization(sdev, chandef, wscs));

	chandef->center_freq1 = 5180;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_check_channelization(sdev, chandef, wscs));

	chandef->width = NL80211_CHAN_WIDTH_160;

	KUNIT_EXPECT_EQ(test, 0, slsi_check_channelization(sdev, chandef, wscs));

	chandef->width = NL80211_CHAN_WIDTH_160 + NL80211_CHAN_WIDTH_80;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_check_channelization(sdev, chandef, wscs));
}

static void test_mib_buffer_dump_to_log(struct kunit *test)
{
	u8 *mib_buffer = kunit_kzalloc(test, sizeof(u8) * 130, GFP_KERNEL);
	unsigned int mib_buffer_len = 100;

	mib_buffer_dump_to_log(NULL, NULL, 10);

	mib_buffer[0] = 0;

	mib_buffer_dump_to_log(NULL, mib_buffer, mib_buffer_len);

	mib_buffer[0] = 1;
	mib_buffer[20] = 155;
	mib_buffer[40] = 101;
	mib_buffer[60] = 159;
	mib_buffer[80] = 150;
	mib_buffer_dump_to_log(NULL, mib_buffer, mib_buffer_len);
}

static void test_slsi_mlme_set_ip_address(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 321;

	sdev->sig_wait_cfm_timeout = &timeout;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->ipaddress = (192 << 24U);
	ndev_vif->ipaddress |= (168 << 16U);
	ndev_vif->ipaddress |= (63 << 8U);
	ndev_vif->ipaddress |= (1);

	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_set_ip_address_req, MLME_SET_IP_ADDRESS_REQ, ndev_vif->ifnum,
					    sizeof(ndev_vif->ipaddress));
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_set_ip_address_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_ip_address(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_ip_address_cfm, MLME_SET_IP_ADDRESS_CFM, 0, 10);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	*sdev->sig_wait_cfm_timeout = 200;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_ip_address(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_ip_address_cfm, MLME_SET_IP_ADDRESS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_ip_address_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_ip_address(sdev, dev));
}

#if IS_ENABLED(CONFIG_IPV6)
static void test_slsi_mlme_set_ipv6_address(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait_cfm_timeout = &timeout;

	ndev_vif->sta.nd_offload_enabled = 1;
	memcpy(&ndev_vif->ipv6address.s6_addr[13], "192", 3);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_ipv6_address(sdev, dev));

	ndev_vif->sta.nd_offload_enabled = 0;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_ipv6_address(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_ip_address_cfm, MLME_SET_IP_ADDRESS_CFM, 0, 10);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	*sdev->sig_wait_cfm_timeout = 200;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_ipv6_address(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_ip_address_cfm, MLME_SET_IP_ADDRESS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_ip_address_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_ipv6_address(sdev, dev));
}
#endif

static void test_slsi_mlme_set_with_cfm(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *mib = "\x10\x45\xAB\xB5\x6C\x12\xD5\xFF\xFC";
	int mib_len = 10;
	int timeout = 321;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_set_with_cfm(sdev, dev, mib, mib_len));
}

static void test_slsi_mlme_set(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *mib = "\x10\x45\xAB\xB5\x6C\x12\xD5\xFF\xFC";
	int mib_len = 10;
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set(sdev, dev, mib, mib_len));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_ip_address_cfm, MLME_SET_IP_ADDRESS_CFM, 0, 10);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	*sdev->sig_wait_cfm_timeout = 200;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_set(sdev, dev, mib, mib_len));
}

static void test_slsi_mlme_get(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *mib = "\x10\x45\xAB\xB5\x6C\x12\xD5\xFF\xFC";
	int mib_len = 10;
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;

	sdev->sig_wait_cfm_timeout = &timeout;
	sdev->mlme_blocked = true;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_get(sdev, dev, mib, mib_len, NULL, 0, &mib_len));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_ip_address_cfm, MLME_SET_IP_ADDRESS_CFM, 0, 10);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	*sdev->sig_wait_cfm_timeout = 200;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_get(sdev, dev, mib, mib_len, NULL, 0, &mib_len));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_ip_address_cfm, MLME_SET_IP_ADDRESS_CFM, 0, 10);
	sdev->sig_wait.mib_error = fapi_alloc(mlme_set_ip_address_cfm, MLME_SET_IP_ADDRESS_CFM, 0, 10);
	ndev_vif->mgmt_tx_cookie = 0x193A;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_get(sdev, dev, mib, mib_len, NULL, 0, &mib_len));
}

static void test_slsi_mlme_add_vif(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *if_addr = "\x12\x34\x56\x78\x90\xAB";
	u8 *dev_addr = "\xAB\x13\x24\x35\x46\x57";
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;
	sdev->require_vif_delete[SLSI_NET_INDEX_WLAN] = true;
	sdev->device_config.user_suspend_mode = EINVAL;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_vif(sdev, dev, if_addr, dev_addr));

	sdev->mlme_blocked = true;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_vif(sdev, dev, if_addr, dev_addr));

	sdev->device_config.user_suspend_mode = 0;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_vif(sdev, dev, if_addr, dev_addr));

	sdev->sig_wait.cfm = fapi_alloc(mlme_add_vif_cfm, MLME_ADD_VIF_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_add_vif_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	*sdev->sig_wait_cfm_timeout = 200;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_add_vif(sdev, dev, if_addr, dev_addr));

	sdev->sig_wait.cfm = fapi_alloc(mlme_add_vif_cfm, MLME_ADD_VIF_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_add_vif_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_add_vif(sdev, dev, if_addr, dev_addr));
}

static void test_slsi_mlme_add_detect_vif(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *if_addr = "\x12\x34\x56\x78\x90\xAB";
	u8 *dev_addr = "\xAB\x13\x24\x35\x46\x57";
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;
	sdev->require_vif_delete[SLSI_NET_INDEX_WLAN] = true;
	sdev->device_config.user_suspend_mode = EINVAL;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_detect_vif(sdev, dev, if_addr, dev_addr));

	sdev->device_config.user_suspend_mode = 0;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_detect_vif(sdev, dev, if_addr, dev_addr));

	sdev->sig_wait.cfm = fapi_alloc(mlme_add_vif_cfm, MLME_ADD_VIF_CFM, 0, 10);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	*sdev->sig_wait_cfm_timeout = 200;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_add_detect_vif(sdev, dev, if_addr, dev_addr));

	sdev->sig_wait.cfm = fapi_alloc(mlme_add_vif_cfm, MLME_ADD_VIF_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_add_vif_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_add_detect_vif(sdev, dev, if_addr, dev_addr));
}

static void test_slsi_mlme_del_detect_vif(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;
	sdev->detect_vif_active = true;
	sdev->mlme_blocked = false;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_del_detect_vif(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_del_vif_cfm, MLME_DEL_VIF_CFM, 0, 10);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	*sdev->sig_wait_cfm_timeout = 200;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_del_detect_vif(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_del_vif_cfm, MLME_DEL_VIF_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_del_vif_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_del_detect_vif(sdev, dev));
}

static void test_slsi_mlme_set_band_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	uint band = 1;
	u16 avoid_disconnection = 12;
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;

	sdev->detect_vif_active = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_set_band_cfm, MLME_SET_BAND_CFM, ndev_vif->ifnum, 100);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_set_band_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_band_req(sdev, dev, band, avoid_disconnection));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_band_cfm, MLME_SET_BAND_CFM, ndev_vif->ifnum, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_band_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	*sdev->sig_wait_cfm_timeout = 200;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_band_req(sdev, dev, band, avoid_disconnection));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_band_cfm, MLME_SET_BAND_CFM, ndev_vif->ifnum, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_band_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_band_req(sdev, dev, band, avoid_disconnection));
}

static void test_slsi_mlme_set_scan_mode_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 321;

	sdev->detect_vif_active = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_scan_mode_req(sdev, dev, 0, 0, 0, 0, 0));

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_scan_mode_req(sdev, dev, 0, 0, 0, 0, 0));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.process_id = SLSI_TX_PROCESS_ID_MAX;

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_set_scan_mode_cfm, MLME_SET_SCAN_MODE_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_set_scan_mode_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	timeout = 200;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_set_scan_mode_req(sdev, dev, 0, 0, 0, 0, 0));

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_set_scan_mode_cfm, MLME_SET_SCAN_MODE_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_set_scan_mode_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_set_scan_mode_req(sdev, dev, 0, 0, 0, 0, 0));
	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
}

static void test_slsi_mlme_set_roaming_parameters(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 psid = 0;
	int mib_value = 0x12;
	int mib_length = 1;
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	sdev->detect_vif_active = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_roaming_parameters(sdev, dev, psid, mib_value, mib_length));

	mib_length = 2;
	mib_value = 0x1234;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_roaming_parameters(sdev, dev, psid, mib_value, mib_length));

	mib_length = 4;
	mib_value = 0x12345676;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_roaming_parameters(sdev, dev, psid, mib_value, mib_length));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_roaming_parameters_cfm, MLME_SET_ROAMING_PARAMETERS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_roaming_parameters_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	timeout = 200;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_set_roaming_parameters(sdev, dev, psid, mib_value, mib_length));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_roaming_parameters_cfm, MLME_SET_ROAMING_PARAMETERS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_roaming_parameters_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_set_roaming_parameters(sdev, dev, psid, mib_value, mib_length));

	mib_length = 89;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_roaming_parameters(sdev, dev, psid, mib_value, mib_length));
}

static void test_slsi_mlme_set_channel(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct ieee80211_channel *chan;
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	chan->center_freq = 4824;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	sdev->detect_vif_active = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_channel(sdev, dev, chan, 200, 70, 32));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_channel_cfm, MLME_SET_CHANNEL_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_channel_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	timeout = 200;
	*sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_channel(sdev, dev, chan, 200, 70, 32));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_channel_cfm, MLME_SET_CHANNEL_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_channel_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_channel(sdev, dev, chan, 200, 70, 32));
}

static void test_slsi_mlme_unset_channel_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 321;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	sdev->detect_vif_active = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_unset_channel_req(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_unset_channel_cfm, MLME_UNSET_CHANNEL_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_unset_channel_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	timeout = 200;
	*sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_unset_channel_req(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_unset_channel_cfm, MLME_UNSET_CHANNEL_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_unset_channel_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_unset_channel_req(sdev, dev));
}

static void test_slsi_ap_obss_scan_done_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct ieee80211_mgmt *buf = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + sizeof(u8) * 25, GFP_KERNEL);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results = kunit_kzalloc(test,sizeof(struct slsi_scan_result), GFP_KERNEL);
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 100);

	memcpy(&buf->u.beacon.variable[0], "\x2d\x1a\xef\x09\x03\xff\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00", 22);
	buf->u.beacon.capab_info = 0x12;
	buf->u.beacon.beacon_int = -32;

	fapi_set_u16(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon, u.mlme_scan_ind.channel_frequency, 4824);
	fapi_set_s16(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon, u.mlme_scan_ind.rssi, -35);
	fapi_set_s16(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon, u.mlme_scan_ind.scan_id, SLSI_SCAN_HW_ID);

	slsi_skb_cb_get(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon)->data_length = 100;
	memcpy(((struct fapi_signal *)(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon)->data) +
	       fapi_get_siglen(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon),
	       buf, sizeof(struct ieee80211_mgmt));

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->hidden = 76;

	slsi_ap_obss_scan_done_ind(dev, ndev_vif);

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 100);
	memcpy(&buf->u.beacon.variable[0], "\x2d\x1a\xef", 3);
	buf->u.beacon.capab_info = 0x12;
	buf->u.beacon.beacon_int = -32;

	fapi_set_u16(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon, u.mlme_scan_ind.channel_frequency, 4824);
	fapi_set_s16(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon, u.mlme_scan_ind.rssi, -35);
	fapi_set_s16(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon, u.mlme_scan_ind.scan_id, SLSI_SCAN_HW_ID);

	slsi_skb_cb_get(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon)->data_length = 10;
	memcpy(((struct fapi_signal *)(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon)->data) +
	       fapi_get_siglen(ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon),
	       buf, sizeof(struct ieee80211_mgmt));

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->hidden = 76;

	slsi_ap_obss_scan_done_ind(dev, ndev_vif);

	SLSI_MUTEX_UNLOCK(ndev_vif->vif_mutex);
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
}

static void test_slsi_scan_cfm_validate(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *cfm = fapi_alloc(mlme_add_scan_cfm, MLME_ADD_SCAN_CFM, 0, 100);

	fapi_set_u16(cfm, u.mlme_add_scan_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_FALSE(test, slsi_scan_cfm_validate(sdev, dev, cfm));
}

static void test_slsi_mlme_set_channels_ie(struct kunit *test)
{
	struct slsi_mlme_parameters *channels_list_ie = kunit_kzalloc(test, sizeof(struct slsi_mlme_parameters), GFP_KERNEL);

	slsi_mlme_set_channels_ie(channels_list_ie);
}

static void test_slsi_mlme_append_gscan_channel_list(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *req = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 7);
	struct slsi_nl_bucket_param *nl_bucket = kunit_kzalloc(test, sizeof(struct slsi_nl_bucket_param), GFP_KERNEL);

	fapi_append_data(req, NULL, 157);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_gscan_channel_list(sdev, dev, req, nl_bucket));

	req = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 17);
	nl_bucket->band = WIFI_BAND_UNSPECIFIED;
	nl_bucket->num_channels = 1;
	nl_bucket->channels[0].channel = 4824;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_gscan_channel_list(sdev, dev, req, nl_bucket));

	fapi_append_data(req, NULL, 139);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_gscan_channel_list(sdev, dev, req, nl_bucket));

	nl_bucket->band = WIFI_BAND_ABG;

	req = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 17);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_gscan_channel_list(sdev, dev, req, nl_bucket));

	fapi_append_data(req, NULL, 139);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_gscan_channel_list(sdev, dev, req, nl_bucket));
}

static void test_slsi_mlme_is_support_band(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	int band = NL80211_BAND_2GHZ;

	sdev->device_config.supported_band = SLSI_FREQ_BAND_5GHZ;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_is_support_band(sdev, band));

	band = NL80211_BAND_5GHZ;
	sdev->device_config.supported_band = SLSI_FREQ_BAND_2GHZ;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_is_support_band(sdev, band));

	band = NL80211_BAND_5GHZ;
	sdev->device_config.supported_band = SLSI_FREQ_BAND_5GHZ;

	KUNIT_EXPECT_TRUE(test, slsi_mlme_is_support_band(sdev, band));
}

static void test_slsi_mlme_get_channel_list_ie_size(struct kunit *test)
{
	u32 n_channels = 10;

	KUNIT_EXPECT_EQ(test, 47, slsi_mlme_get_channel_list_ie_size(n_channels));
}

static void test_slsi_mlme_append_channel_ie(struct kunit *test)
{
	struct sk_buff *req = fapi_alloc(mlme_scan_ind, MLME_SCAN_IND, 0, 7);
	struct slsi_mlme_parameters *res;

	res = slsi_mlme_append_channel_ie(req);

	KUNIT_EXPECT_EQ(test, SLSI_WLAN_EID_VENDOR_SPECIFIC, res->element_id);
	KUNIT_EXPECT_EQ(test, SLSI_MLME_PARAM_HEADER_SIZE, res->length);
	KUNIT_EXPECT_EQ(test, SLSI_MLME_TYPE_SCAN_PARAM, res->oui_type);
	KUNIT_EXPECT_EQ(test, SLSI_MLME_SUBTYPE_CHANNEL_LIST, res->oui_subtype);
}

static void test_slsi_mlme_append_channel_list_data(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *req;
	struct ieee80211_channel *channel;
	u16 scan_type = FAPI_SCANTYPE_FULL_SCAN;
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	u32 scan_flags = NL80211_SCAN_FLAG_COLOCATED_6GHZ;
#endif
	bool passive_scan = true;
	size_t alloc_data_size = 0;

	channel = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);

	alloc_data_size += sizeof(struct slsi_mlme_scan_timing_element);
	alloc_data_size += 97;
	req = fapi_alloc(mlme_add_scan_req, MLME_ADD_SCAN_REQ, 0, alloc_data_size);

	channel->hw_value = 11;
	channel->band = NL80211_BAND_2GHZ;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_channel_list_data(sdev, req, channel, scan_type,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
								    scan_flags,
#endif
								    passive_scan));

	passive_scan = false;
	scan_type = FAPI_SCANTYPE_AP_AUTO_CHANNEL_SELECTION;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_channel_list_data(sdev, req, channel, scan_type,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
								    scan_flags,
#endif
								    passive_scan));

	kfree_skb(req);
}

static void test_slsi_mlme_append_channel_list(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *req;
	u32 num_channels = 63;
	struct ieee80211_channel *channel[63];
	u16 scan_type = FAPI_SCANTYPE_FULL_SCAN;
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	u32 scan_flags = NL80211_SCAN_FLAG_COLOCATED_6GHZ;
#endif
	bool passive_scan = true;
	int i = 0;

	channel[0] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	channel[0]->band = NL80211_BAND_2GHZ;
	channel[0]->hw_value = 1;
	channel[1] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	channel[1]->band = NL80211_BAND_5GHZ;
	channel[1]->hw_value = 44;
	channel[2] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	channel[2]->band = NL80211_BAND_2GHZ;
	channel[2]->hw_value = 6;
	channel[3] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	channel[3]->band = NL80211_BAND_2GHZ;
	channel[3]->hw_value = 11;
	channel[4] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	channel[4]->band = NL80211_BAND_5GHZ;
	channel[4]->hw_value = 106;
	for (i = 5; i < 63; i++) {
		channel[i] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
		channel[i]->band = NL80211_BAND_2GHZ;
		channel[i]->hw_value = 3;
	}

	req = alloc_skb(1, GFP_KERNEL);
	fapi_append_data(req, NULL, 190);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_append_channel_list(sdev, dev, req, num_channels, channel, scan_type,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
								     scan_flags,
#endif
								     passive_scan));

	scan_type = FAPI_SCANTYPE_P2P_SCAN_FULL;
	req = alloc_skb(1, GFP_KERNEL);
	fapi_append_data(req, NULL, 185);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_channel_list(sdev, dev, req, num_channels, channel, scan_type,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
							       scan_flags,
#endif
							       passive_scan));

	req = alloc_skb(1, GFP_KERNEL);
	sdev->band_6g_supported = true;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_channel_list(sdev, dev, req, num_channels, channel, scan_type,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
							       scan_flags,
#endif
							       passive_scan));

	req = alloc_skb(1, GFP_KERNEL);
	sdev->band_6g_supported = false;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_channel_list(sdev, dev, req, num_channels, channel, scan_type,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
							       scan_flags,
#endif
							       passive_scan));

	scan_type = FAPI_SCANTYPE_FULL_SCAN;
	req = alloc_skb(1, GFP_KERNEL);
	sdev->device_config.supported_band = SLSI_FREQ_BAND_5GHZ;
	fapi_append_data(req, NULL, 184);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_append_channel_list(sdev, dev, req, num_channels, channel, scan_type,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
							       scan_flags,
#endif
							       passive_scan));

	req = alloc_skb(195, GFP_KERNEL);
	fapi_append_data(req, "0x122345", 445);
	sdev->device_config.supported_band = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_append_channel_list(sdev, dev, req, num_channels, channel, scan_type,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
								     scan_flags,
#endif
								     passive_scan));

	req = alloc_skb(1, GFP_KERNEL);
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_append_channel_list(sdev, dev, req, 0, channel, scan_type,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
								     scan_flags,
#endif
								     passive_scan));

}

static void test_slsi_set_scan_params(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u16 scan_id = 0x1234;
	u16 scan_type = FAPI_SCANTYPE_FULL_SCAN;
	u16 report_mode = FAPI_REPORTMODE_END_OF_SCAN_CYCLE | FAPI_REPORTMODE_NO_BATCH;
	int num_ssids = 1;
	struct cfg80211_ssid *ssids = kunit_kzalloc(test, sizeof(struct cfg80211_ssid), GFP_KERNEL);
	struct sk_buff *req;
	size_t alloc_data_size = 0;

	alloc_data_size += sizeof(struct slsi_mlme_scan_timing_element) + 10;

	req = alloc_skb(1, GFP_KERNEL);
	fapi_append_data(req, NULL, 191);
	dev->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;

	KUNIT_EXPECT_EQ(test, 0, slsi_set_scan_params(dev, scan_id, scan_type, report_mode,
						      num_ssids, ssids, req));

	req = alloc_skb(1, GFP_KERNEL);
	memcpy(ssids->ssid, "KUNIT_SSID", 11);
	ssids->ssid_len = 11;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_scan_params(dev, scan_id, scan_type, report_mode,
						      num_ssids, ssids, req));
}

static void test_slsi_mlme_add_sched_scan(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *ies = "\x12\x34\x45\x56\x67\x78\x89\x90\xA1";
	u16 ies_len = 10;
	struct cfg80211_sched_scan_request *request;
	struct netdev_vif *sndev_vif;
	int timeout = 200;

	request = kunit_kzalloc(test, sizeof(struct cfg80211_sched_scan_request) + sizeof(struct ieee80211_channel) * 2,
				GFP_KERNEL);
	request->match_sets = kunit_kzalloc(test, sizeof(struct cfg80211_match_set), GFP_KERNEL);
	request->channels[0] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	request->ssids = kunit_kzalloc(test, sizeof(struct cfg80211_ssid), GFP_KERNEL);
	request->scan_plans = kunit_kzalloc(test, sizeof(struct cfg80211_sched_scan_plan), GFP_KERNEL);
	request->n_channels = 1;
	request->channels[0]->band = NL80211_BAND_5GHZ;
	request->n_ssids = 1;
	memcpy(request->ssids->ssid, "KUNIT_SSID", 11);
	request->ssids->ssid_len = 11;
	request->n_match_sets = 1;

	dev->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	request->scan_plans->iterations = 0xF111;

	sdev->band_6g_supported = false;
	sdev->device_config.supported_band = SLSI_FREQ_BAND_5GHZ;

	sdev->default_scan_ies_len = 3;
	sdev->default_scan_ies = "\x12\x34\x45";

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_sched_scan(sdev, dev, request, ies, ies_len));

	request->scan_plans->iterations = 0xFFFF;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_sched_scan(sdev, dev, request, ies, ies_len));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_add_scan_cfm, MLME_ADD_SCAN_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_add_scan_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_add_sched_scan(sdev, dev, request, ies, ies_len));

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_add_scan_cfm, MLME_ADD_SCAN_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_add_scan_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_add_sched_scan(sdev, dev, request, ies, ies_len));
}

static void test_slsi_mlme_add_scan(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 scan_type = FAPI_SCANTYPE_P2P_SCAN_SOCIAL;
	u16 report_mode = 0x12;
	u32 n_ssids = 1;
	struct cfg80211_ssid *ssids;
	u32 n_channels = 3;
	struct ieee80211_channel *channels[3];
	struct slsi_gscan_param *gscan = NULL;
	const u8 *ies = "\x12\x34\x45\x56\x67\x78\x89\x90\xA1";
	u16 ies_len = 10;
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	u32 n_6ghz_params;
	struct cfg80211_scan_6ghz_params *scan_6ghz_params;
	u32 scan_flags;
#endif
	bool wait_for_ind;
	int timeout = 200;

#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
	n_6ghz_params = 1;
	scan_6ghz_params = kunit_kzalloc(test, sizeof(struct cfg80211_scan_6ghz_params), GFP_KERNEL);
	scan_flags = 1;
#endif

	dev->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	sdev->device_config.supported_band = true;
	sdev->device_config.supported_band = SLSI_FREQ_BAND_2GHZ;
	sdev->default_scan_ies_len = 10;

	ssids = kunit_kzalloc(test, sizeof(struct cfg80211_ssid), GFP_KERNEL);
	memcpy(ssids->ssid, "HELLO_KUNIT", 12);
	ssids->ssid_len = 12;

	channels[0] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	channels[0]->band = NL80211_BAND_2GHZ;
	channels[1] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	channels[1]->band = NL80211_BAND_2GHZ;
	channels[2] = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	channels[2]->band = NL80211_BAND_2GHZ;

	wait_for_ind = true;
	SLSI_MUTEX_LOCK(ndev_vif->scan_mutex);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_scan(sdev, dev, scan_type, report_mode, n_ssids, ssids,
						       n_channels, channels, gscan, ies, ies_len,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
						       n_6ghz_params, scan_6ghz_params, scan_flags,
#endif
						       wait_for_ind));



	wait_for_ind = false;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_scan(sdev, dev, scan_type, report_mode, n_ssids, ssids,
						       n_channels, channels, gscan, ies, ies_len,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
						       n_6ghz_params, scan_6ghz_params, scan_flags,
#endif
						       wait_for_ind));

	gscan = kunit_kzalloc(test, sizeof(struct slsi_gscan_param), GFP_KERNEL);
	gscan->nl_bucket = kunit_kzalloc(test, sizeof(struct slsi_nl_bucket_param), GFP_KERNEL);
	gscan->nl_bucket->exponent = 1;
	gscan->bucket = kunit_kzalloc(test, sizeof(struct slsi_bucket), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_scan(sdev, dev, scan_type, report_mode, n_ssids, ssids,
						       n_channels, channels, gscan, ies, ies_len,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
						       n_6ghz_params, scan_6ghz_params, scan_flags,
#endif
						       wait_for_ind));


	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_add_scan_cfm, MLME_ADD_SCAN_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_add_scan_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_add_scan(sdev, dev, scan_type, report_mode, n_ssids, ssids,
						    n_channels, channels, gscan, ies, ies_len,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
						    n_6ghz_params, scan_6ghz_params, scan_flags,
#endif
						    wait_for_ind));

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_add_scan_cfm, MLME_ADD_SCAN_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_add_scan_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_add_scan(sdev, dev, scan_type, report_mode, n_ssids, ssids,
						    n_channels, channels, gscan, ies, ies_len,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
						    n_6ghz_params, scan_6ghz_params, scan_flags,
#endif
						    wait_for_ind));

	ndev_vif->mgmt_tx_cookie = 0x865A;
	ndev_vif->sig_wait.ind = fapi_alloc(mlme_scan_done_ind, MLME_SCAN_DONE_IND, 0, 10);
	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_add_scan_cfm, MLME_ADD_SCAN_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_add_scan_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_add_scan(sdev, dev, scan_type, report_mode, n_ssids, ssids,
						    n_channels, channels, gscan, ies, ies_len,
#ifdef CONFIG_SCSC_WLAN_SUPPORT_6G
						    n_6ghz_params, scan_6ghz_params, scan_flags,
#endif
						    true));
	SLSI_MUTEX_UNLOCK(ndev_vif->scan_mutex);
}

static void test_slsi_mlme_del_scan(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 scan_id = SLSI_SCAN_HW_ID;
	int timeout = 200;

	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = kunit_kzalloc(test, sizeof(struct cfg80211_scan_request), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_del_scan(sdev, dev, scan_id, false));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_del_scan_cfm, MLME_DEL_SCAN_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_del_scan_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_del_scan(sdev, dev, scan_id, false));

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_del_scan_cfm, MLME_DEL_SCAN_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_del_scan_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_del_scan(sdev, dev, scan_id, false));
}

static void test_slsi_ap_add_ext_capab_ie(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *req;
	u8 *prev_ext = "\x12\x02\x34\xAB\x98";

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	req = fapi_alloc(mlme_start_req, MLME_START_REQ, ndev_vif->ifnum, 20);

	slsi_ap_add_ext_capab_ie(req, ndev_vif, prev_ext);

	kfree_skb(req);
}

static void test_slsi_prepare_country_ie(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);

	char country_ie[] = {
		0x07, 0x10, 0x4b, 0x52, 0x20, 0x24, 0x04, 0x11,
		0x34, 0x04, 0x17, 0x64, 0x07, 0x17, 0x95, 0x04,
		0x18, 0x00
	};
	u8 *new_country_ie;

	sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
								  sizeof(struct ieee80211_reg_rule) * 2, GFP_KERNEL);
	sdev->device_config.domain_info.regdomain->alpha2[0] = 'K';
	sdev->device_config.domain_info.regdomain->alpha2[1] = 'R';
	sdev->device_config.domain_info.regdomain->n_reg_rules = 2;
	sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.start_freq_khz = 2401000;
	sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.end_freq_khz = 2423000;

	sdev->device_config.domain_info.regdomain->reg_rules[1].freq_range.start_freq_khz = 5210000;
	sdev->device_config.domain_info.regdomain->reg_rules[1].freq_range.end_freq_khz = 5250000;

	KUNIT_EXPECT_EQ(test, 0, slsi_prepare_country_ie(sdev, NL80211_BAND_2GHZ, country_ie, &new_country_ie));

	kfree(new_country_ie);
	new_country_ie = NULL;

	KUNIT_EXPECT_EQ(test, 0, slsi_prepare_country_ie(sdev, NL80211_BAND_5GHZ, country_ie, &new_country_ie));

	kfree(new_country_ie);
}

static void test_slsi_modify_ies(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	u8 eid = WLAN_EID_VHT_CAPABILITY;
	u8 *ies = "\x12\x23\x34\x45\x56\x67\x78\x89\x90";
	int ies_len = 10;
	u8 ie_index = 2;
	u8 ie_value = "\xAB";

	KUNIT_EXPECT_TRUE(test, slsi_modify_ies(dev, eid, ies, ies_len, ie_index, ie_value));

	eid = WLAN_EID_HT_OPERATION;

	KUNIT_EXPECT_TRUE(test, slsi_modify_ies(dev, eid, ies, ies_len, ie_index, ie_value));

	ie_index = 1;

	KUNIT_EXPECT_TRUE(test, slsi_modify_ies(dev, eid, ies, ies_len, ie_index, ie_value));

	eid = WLAN_EID_COUNTRY;

	KUNIT_EXPECT_FALSE(test, slsi_modify_ies(dev, eid, ies, ies_len, ie_index, ie_value));

	KUNIT_EXPECT_FALSE(test, slsi_modify_ies(dev, eid, NULL, 0, ie_index, ie_value));
}

static void test_slsi_mlme_start_prepare_ies(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff *req;
	struct cfg80211_ap_settings *settings;
	u8 *wpa_ie_pos;
	u8 *wmm_ie_pos;
	u8 *tmpdata = "\x12\x23\x34\x45\x56\x67\x78\x90\xA3\x12\x23\x34\x45\x56\x67\x78\x90\xA3\x12\x23\x34\x45\x56\x67\x78\x90\xA3";

	req = alloc_skb(1, GFP_KERNEL);

	ndev_vif->sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
									    sizeof(struct ieee80211_reg_rule) * 2,
									    GFP_KERNEL);
	ndev_vif->sdev->device_config.domain_info.regdomain->alpha2[0] = 'K';
	ndev_vif->sdev->device_config.domain_info.regdomain->alpha2[1] = 'R';
	ndev_vif->sdev->device_config.domain_info.regdomain->n_reg_rules = 1;
	ndev_vif->sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.start_freq_khz = 2401000;
	ndev_vif->sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.end_freq_khz = 2423000;

	ndev_vif->chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);
	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_80;

	ndev_vif->chandef->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	ndev_vif->chandef->chan->center_freq = 2412;
	ndev_vif->chandef->chan->band = NL80211_BAND_2GHZ;

	settings = kunit_kzalloc(test, sizeof(struct cfg80211_ap_settings), GFP_KERNEL);
	settings->beacon.tail = kunit_kzalloc(test, sizeof(u8) * 14, GFP_KERNEL);
	settings->beacon.tail = "\xBF\x04\x4B\x52\x12\x43";
	settings->beacon.tail_len = 7;
	settings->beacon.tail += 7;
	settings->beacon.tail = "\xBF\x04\x4B\x52\x12\x43";
	settings->beacon.tail_len += 7;
	ndev_vif->ap.non_ht_bss_present = true;

	wmm_ie_pos = NULL;
	wpa_ie_pos = NULL;

	slsi_mlme_start_prepare_ies(req, ndev_vif, settings, wpa_ie_pos, wmm_ie_pos);

	kfree_skb(req);

	req = alloc_skb(1, GFP_KERNEL);
	wmm_ie_pos = "\x12\x23\x34\x45\x56\x67\x78\x89\x90\xAB\xA3";
	wpa_ie_pos = "\x2d\x1a\xef\x09\x03\xff\xff\xFF\x12\x12\x01\x12";
	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_80;
	settings->beacon.tail = kunit_kzalloc(test, sizeof(u8) * 14, GFP_KERNEL);
	settings->beacon.tail = "\xBF\x04\x4B\x52\x12\x43";
	settings->beacon.tail_len = 7;
	settings->beacon.tail += 7;
	settings->beacon.tail = "\xBF\x04\x4B\x52\x12\x43";
	settings->beacon.tail_len += 7;

	slsi_mlme_start_prepare_ies(req, ndev_vif, settings, wpa_ie_pos, wmm_ie_pos);

	kfree_skb(req);

	req = alloc_skb(1, GFP_KERNEL);
	settings->beacon.tail = kunit_kzalloc(test, sizeof(u8) * 14, GFP_KERNEL);
	settings->beacon.tail = "\xBF\x04\x4B\x52\x12\x43\xBF\x04\x4B\x52\x12\x43";
	settings->beacon.tail_len = 5;
	wmm_ie_pos = tmpdata + 21;
	wpa_ie_pos = NULL;
	ndev_vif->ap.wpa_ie_len = 7;
	ndev_vif->sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.start_freq_khz = 2401;
	ndev_vif->sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.end_freq_khz = 2423;
	ndev_vif->ap.non_ht_bss_present = false;

	slsi_mlme_start_prepare_ies(req, ndev_vif, settings, wpa_ie_pos, wmm_ie_pos);

	kfree_skb(req);

	req = alloc_skb(1, GFP_KERNEL);
	settings->beacon.tail = kunit_kzalloc(test, sizeof(u8) * 14, GFP_KERNEL);
	settings->beacon.tail = "\xBF\x0D\x4B\x52\x12\x43\x32\x12\x12\x12\x00\xFF\xFF";
	settings->beacon.tail_len = 13;
	wmm_ie_pos = "\xBF\x04\x4B\x52\x02\xFF";
	wpa_ie_pos = "\x2d\x1a\xef\x09\x03\x00";
	ndev_vif->ap.wpa_ie_len = 10;
	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_20;

	slsi_mlme_start_prepare_ies(req, ndev_vif, settings, wpa_ie_pos, wmm_ie_pos);

	kfree_skb(req);
}

static void test_slsi_prepare_vht_ies(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	u8 *vht_ie_capab;
	u8 *vht_ie_operation;
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);
	ndev_vif->chandef->center_freq1 = 2412;

	KUNIT_EXPECT_EQ(test, 0, slsi_prepare_vht_ies(dev, &vht_ie_capab, &vht_ie_operation));

	kfree(vht_ie_capab);
	kfree(vht_ie_operation);
}

static void test_slsi_mlme_start(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *bssid;
	struct cfg80211_ap_settings *settings;
	u8 *wpa_ie_pos;
	u8 *wmm_ie_pos;
	bool append_vht_ies;
	struct ieee80211_mgmt *buf;
	int timeout = 200;

	dev->dev_addr = SLSI_DEFAULT_HW_MAC_ADDR;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	ndev_vif->sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain) +
									    sizeof(struct ieee80211_reg_rule) * 2,
									    GFP_KERNEL);
	ndev_vif->sdev->device_config.domain_info.regdomain->alpha2[0] = 'K';
	ndev_vif->sdev->device_config.domain_info.regdomain->alpha2[1] = 'R';
	ndev_vif->sdev->device_config.domain_info.regdomain->n_reg_rules = 1;
	ndev_vif->sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.start_freq_khz = 2401000;
	ndev_vif->sdev->device_config.domain_info.regdomain->reg_rules[0].freq_range.end_freq_khz = 2423000;

	ndev_vif->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	ndev_vif->chan->center_freq = 2412;

	ndev_vif->chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);
	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_80;
	ndev_vif->chandef->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	ndev_vif->chandef->center_freq1 = 5270;
	ndev_vif->chandef->chan->center_freq = 5200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	buf = kunit_kzalloc(test, sizeof(struct ieee80211_mgmt) + sizeof(u8) * 12, GFP_KERNEL);
	memcpy(buf->u.beacon.variable, "\x00\x06x75\x72\x65\x61\x64\x79", 11);
	buf->u.beacon.capab_info = 0x12;
	buf->u.beacon.beacon_int = -32;
	settings = kunit_kzalloc(test, sizeof(struct cfg80211_ap_settings), GFP_KERNEL);

	settings->beacon.head = buf;
	settings->beacon.head_len = 10;

	settings->beacon.tail = "\x23\x04\x4B\x52\x12\x43";
	settings->beacon.tail_len = 7;

	bssid = SLSI_DEFAULT_HW_MAC_ADDR;
	settings->ssid_len = 12;
	settings->ssid = "HELLO_kUNIT";
	settings->hidden_ssid = 1;

	settings->beacon_interval = 100;
	settings->dtim_period = 20;

	settings->auth_type = NL80211_AUTHTYPE_SHARED_KEY;
	settings->crypto.sae_pwe = NL80211_SAE_PWE_HASH_TO_ELEMENT;
	append_vht_ies = true;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_start(sdev, dev, bssid, settings, NULL, NULL, append_vht_ies));

	settings->beacon.tail = "\x26\x04\x4B\x52\x12\x43";
	settings->beacon.tail_len = 7;

	settings->auth_type = NL80211_AUTHTYPE_AUTOMATIC;
	settings->privacy = true;
	settings->crypto.cipher_group = 0;

	settings->hidden_ssid = 0;

	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_40;
	ndev_vif->chandef->center_freq1 = 5250;
	ndev_vif->chandef->chan->center_freq = 5270;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_start(sdev, dev, bssid, settings, NULL, NULL, append_vht_ies));

	settings->beacon.tail = "\x26\x04\x4B\x52\x12\x43";
	settings->beacon.tail_len = 7;

	settings->auth_type = NL80211_AUTHTYPE_AUTOMATIC;
	settings->privacy = true;
	settings->crypto.cipher_group = 0;
	settings->hidden_ssid = 2;
	sdev->sig_wait.cfm = fapi_alloc(mlme_start_cfm, MLME_START_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_start_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	ndev_vif->chandef->width = NL80211_CHAN_WIDTH_160;
	ndev_vif->chandef->center_freq1 = 5220;
	ndev_vif->chandef->chan->center_freq = 5250;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_start(sdev, dev, bssid, settings, NULL, NULL, append_vht_ies));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 41))
	settings->beacon.he_bss_color.enabled = true;
#else
	settings->he_bss_color.enabled = true;
#endif
	sdev->sig_wait.cfm = fapi_alloc(mlme_start_cfm, MLME_START_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_start_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_start(sdev, dev, bssid, settings, NULL, NULL, append_vht_ies));

	settings->auth_type = NL80211_AUTHTYPE_AUTOMATIC + 0xFF;

	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_mlme_start(sdev, dev, bssid, settings, NULL, NULL, append_vht_ies));

	settings->auth_type = NL80211_AUTHTYPE_SHARED_KEY;
	settings->ssid_len = 500;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_start(sdev, dev, bssid, settings, NULL, NULL, append_vht_ies));
}

static void test_slsi_mlme_connect_get_sec_ie(struct kunit *test)
{
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params), GFP_KERNEL);
	int sec_ie_len;
	bool rsnx_flag;

	sme->crypto.wpa_versions = 0;
	sme->ie = "\x12\x05\x10\x00\x05";
	sme->ie_len = 5;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_connect_get_sec_ie(sme, &sec_ie_len, &rsnx_flag));

	sme->ie = "\x12\x05\x01\x00\x05";
	sme->ie_len = 5;

	KUNIT_EXPECT_TRUE(test, slsi_mlme_connect_get_sec_ie(sme, &sec_ie_len, &rsnx_flag));

	sme->crypto.wpa_versions = 2;
	sme->ie = "\x12\x05\x10\x00\x05";
	sme->ie_len = 5;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_connect_get_sec_ie(sme, &sec_ie_len, &rsnx_flag));

	sme->ie = "\x12\x05\x01\x00\x05";
	sme->ie_len = 5;

	KUNIT_EXPECT_TRUE(test, slsi_mlme_connect_get_sec_ie(sme, &sec_ie_len, &rsnx_flag));
}

static void test_slsi_mlme_connect_info_elems_ie_prep(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 *connect_ie = NULL;
	size_t connect_ie_len = 0;
	bool is_copy = false;
	u8 *ie_dest = kunit_kzalloc(test, sizeof(u8)*30, GFP_KERNEL);
	int ie_dest_len = 10;

	sdev->device_config.qos_info = 0;

	KUNIT_EXPECT_EQ(test, 9,
			slsi_mlme_connect_info_elems_ie_prep(sdev, connect_ie, connect_ie_len,
							     is_copy, ie_dest, ie_dest_len));

	is_copy = true;
	sdev->device_config.qos_info = 0;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_mlme_connect_info_elems_ie_prep(sdev, connect_ie, connect_ie_len,
							     is_copy, ie_dest, ie_dest_len));

	sdev->device_config.qos_info = 0;
	ie_dest_len = 5;

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_mlme_connect_info_elems_ie_prep(sdev, connect_ie, connect_ie_len,
							     is_copy, ie_dest, ie_dest_len));

	ie_dest_len = 1;
	connect_ie = "\x0A\x07\x03\x01\x01\x02\x01\x00\x00";
	connect_ie_len = 5;

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_mlme_connect_info_elems_ie_prep(sdev, connect_ie, connect_ie_len,
							     is_copy, ie_dest, ie_dest_len));
}

static void test_slsi_mlme_connect_info_elements(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params), GFP_KERNEL);
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	sdev->device_config.qos_info = 0;
	sme->ie = "\x02\x0A\x05\x01\x02";
	sme->ie_len = 5;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->sta.assoc_req_add_info_elem_len = 1;

	sdev->sig_wait.cfm = fapi_alloc(mlme_add_info_elements_cfm, MLME_ADD_INFO_ELEMENTS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_add_info_elements_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_connect_info_elements(sdev, dev, sme));

	kfree(ndev_vif->sta.assoc_req_add_info_elem);

	sme->ie = "\x02\x0A\xDC";
	sme->ie_len = 3;
	ndev_vif->ifnum = SLSI_NET_INDEX_AP2;
	ndev_vif->sta.assoc_req_add_info_elem_len = 1;

	sdev->sig_wait.cfm = fapi_alloc(mlme_add_info_elements_cfm, MLME_ADD_INFO_ELEMENTS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_add_info_elements_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_connect_info_elements(sdev, dev, sme));

	sme->ie = NULL;
	sme->ie_len = 0;
	ndev_vif->sta.assoc_req_add_info_elem_len = 1;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_connect_info_elements(sdev, dev, sme));
}

static void test_slsi_mlme_connect(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_connect_params *sme = kunit_kzalloc(test, sizeof(struct cfg80211_connect_params), GFP_KERNEL);
	struct ieee80211_channel *channel = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	u8 *bssid = "KUNIT_HELLO";
	u8 *ssid = "KUIIT_HELLO";
	u8 *tmp_ie = "\x0A\x02\x02\x01\x00\x00\x00\x00\x00";
	u8 *tmp_key = "\x12\x34\x45\x65\xA7\xBA\xCA\x0A\x08\x10";
	int timeout = 500;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	sdev->wlan_service_on = false;
	sdev->mlme_blocked = true;
	sme->ssid_len = 12;
	sme->ssid = ssid;
	sme->auth_type = NL80211_AUTHTYPE_SHARED_KEY;
	sme->crypto.cipher_group = WLAN_CIPHER_SUITE_WEP40;
	sme->key = tmp_key;
	sme->key_len = 10;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_connect(sdev, dev, sme, channel, bssid));

	sme->key_len = 5;
	sme->crypto.n_ciphers_pairwise = 1;
	sme->crypto.ciphers_pairwise[0] = WLAN_CIPHER_SUITE_WEP40;
	sme->key_idx = 7;
	sdev->sig_wait.cfm = fapi_alloc(mlme_connect_cfm, MLME_CONNECT_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_connect_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_connect(sdev, dev, sme, channel, bssid));

	ndev_vif->mgmt_tx_cookie = 0x0;
	sme->auth_type = NL80211_AUTHTYPE_SAE;
	sme->ie = tmp_ie;
	sme->ie_len = 5;
	sme->ssid_len = 5;
	sdev->device_config.qos_info = 0;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_connect(sdev, dev, sme, channel, bssid));

	sme->auth_type = NL80211_AUTHTYPE_AUTOMATIC;
	sme->privacy = true;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_connect(sdev, dev, sme, channel, bssid));

	ndev_vif->mgmt_tx_cookie = 0x0;
	sme->auth_type = NL80211_AUTHTYPE_AUTOMATIC;
	sme->privacy = false;
	tmp_ie = "\x12\x02\x45\x00\x00\x00\x00\x00\x00\x00";
	sme->ie_len = 4;
	sdev->device_config.qos_info = 0;
	sme->key = NULL;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_connect(sdev, dev, sme, channel, bssid));

	sme->auth_type = NL80211_AUTHTYPE_AUTOMATIC;
	sme->privacy = false;
	sme->ie_len = 0;
	sdev->device_config.qos_info = 0;
	channel->center_freq = 2412;
	sme->ssid = "KUNIT";
	sme->ssid_len = 5;
	sdev->sig_wait.cfm = fapi_alloc(mlme_connect_cfm, MLME_CONNECT_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_connect_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_connect(sdev, dev, sme, channel, bssid));

	sme->auth_type = NL80211_AUTHTYPE_AUTOMATIC + 10;

	KUNIT_EXPECT_EQ(test, -EOPNOTSUPP, slsi_mlme_connect(sdev, dev, sme, channel, bssid));
}

static void test_slsi_mlme_connect_resp(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	slsi_mlme_connect_resp(sdev, dev);
}

static void test_slsi_mlme_connected_resp(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	slsi_mlme_connected_resp(sdev, dev, 2);
}

static void test_slsi_mlme_roamed_resp(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	slsi_mlme_roamed_resp(sdev, dev);
}

static void test_slsi_disconnect_cfm_validate(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *cfm = fapi_alloc(mlme_disconnect_cfm, MLME_DISCONNECT_CFM, 0, 10);

	fapi_set_u16(cfm, u.mlme_disconnect_cfm.result_code, FAPI_RESULTCODE_SUCCESS);

	KUNIT_EXPECT_TRUE(test, slsi_disconnect_cfm_validate(sdev, dev, cfm));

	cfm = fapi_alloc(mlme_disconnect_cfm, MLME_DISCONNECT_CFM, 0, 10);

	fapi_set_u16(cfm, u.mlme_disconnect_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_FALSE(test, slsi_disconnect_cfm_validate(sdev, dev, cfm));
}

static void test_slsi_roaming_channel_list_cfm_validate(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *cfm = fapi_alloc(mlme_roaming_channel_list_cfm, MLME_ROAMING_CHANNEL_LIST_CFM, 0, 10);

	fapi_set_u16(cfm, u.mlme_roaming_channel_list_cfm.result_code, FAPI_RESULTCODE_SUCCESS);

	KUNIT_EXPECT_TRUE(test, slsi_roaming_channel_list_cfm_validate(sdev, dev, cfm));

	cfm = fapi_alloc(mlme_roaming_channel_list_cfm, MLME_ROAMING_CHANNEL_LIST_CFM, 0, 10);

	fapi_set_u16(cfm, u.mlme_roaming_channel_list_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_FALSE(test, slsi_roaming_channel_list_cfm_validate(sdev, dev, cfm));
}

static void test_slsi_mlme_roaming_channel_list_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_FALSE(test, slsi_mlme_roaming_channel_list_req(sdev, dev));
}

static void test_slsi_mlme_disconnect(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *mac = SLSI_DEFAULT_HW_MAC_ADDR;
	u16 reason_code = FAPI_RESULTCODE_REJECTED_INVALID_IE;
	bool wait_ind = true;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	ndev_vif->sta.vendor_disconnect_ies_len = 11;
	ndev_vif->sta.vendor_disconnect_ies = "\x12\x0A\x45\x34\x56\x67\x78\x89\x0A\xBC";

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_disconnect(sdev, dev, mac, reason_code, wait_ind));

	mac = NULL;
	wait_ind = false;
	ndev_vif->activated = true;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_disconnect(sdev, dev, mac, reason_code, wait_ind));

	sdev->sig_wait.cfm = fapi_alloc(mlme_disconnect_cfm, MLME_DISCONNECT_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_disconnect_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_disconnect(sdev, dev, mac, reason_code, wait_ind));

	sdev->sig_wait.cfm = fapi_alloc(mlme_disconnect_cfm, MLME_DISCONNECT_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_disconnect_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_disconnect(sdev, dev, mac, reason_code, wait_ind));
}

static void test_slsi_mlme_get_key(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 key_id = 0xFA12;
	u16 key_type = FAPI_KEYTYPE_GROUP;
	u8 seq[10];
	int seq_len;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_get_key(sdev, dev, key_id, key_type, seq, &seq_len));

	sdev->sig_wait.cfm = fapi_alloc(mlme_get_key_sequence_cfm, MLME_GET_KEY_SEQUENCE_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_get_key_sequence_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_get_key_sequence_cfm.sequence_number[0], 1);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_get_key_sequence_cfm.sequence_number[1], 1);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_get_key_sequence_cfm.sequence_number[2], 1);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_get_key_sequence_cfm.sequence_number[3], 1);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_get_key(sdev, dev, key_id, key_type, seq, &seq_len));

	sdev->sig_wait.cfm = fapi_alloc(mlme_get_key_sequence_cfm, MLME_GET_KEY_SEQUENCE_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_get_key_sequence_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -ENOENT, slsi_mlme_get_key(sdev, dev, key_id, key_type, seq, &seq_len));
}

static void test_slsi_decode_fw_rate(struct kunit *test)
{
	u32 fw_rate = 0;
	struct rate_info *rate = kunit_kzalloc(test, sizeof(struct rate_info), GFP_KERNEL);
	unsigned long data_rate_mbps = 1234;

	fw_rate = 0x000A;
	fw_rate |= SLSI_FW_API_RATE_NON_HT_SELECTED;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x001A;
	fw_rate |= SLSI_FW_API_RATE_HT_SELECTED;
	fw_rate |= SLSI_FW_API_RATE_BW_40MHZ;
	fw_rate |= SLSI_FW_API_RATE_SGI;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x0001;
	fw_rate |= SLSI_FW_API_RATE_HT_SELECTED;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x0008;
	fw_rate |= SLSI_FW_API_RATE_HT_SELECTED;
	fw_rate |= SLSI_FW_API_RATE_SGI;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x0220;
	fw_rate |= SLSI_FW_API_RATE_HT_SELECTED;
	fw_rate |= SLSI_FW_API_RATE_SGI;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x0220;
	fw_rate |= SLSI_FW_API_RATE_HT_SELECTED;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x0F20;
	fw_rate |= SLSI_FW_API_RATE_HT_SELECTED;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x0220;
	fw_rate |= SLSI_FW_API_RATE_HE_SELECTED;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x022F;
	fw_rate |= SLSI_FW_API_RATE_HE_SELECTED;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x0220;
	fw_rate |= SLSI_FW_API_RATE_VHT_SELECTED;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);

	fw_rate = 0x022F;
	fw_rate |= SLSI_FW_API_RATE_VHT_SELECTED;

	slsi_decode_fw_rate(fw_rate, rate, &data_rate_mbps, NULL, NULL, NULL);
}

static void test_slsi_mlme_get_sinfo_mib(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_peer *peer = NULL;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_get_sinfo_mib(sdev, dev, peer));

	peer = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	peer->sinfo_mib_get_rs.begin = 123;
	peer->sinfo_mib_get_rs.interval = 10;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_get_sinfo_mib(sdev, dev, peer));
}

static void test_slsi_mlme_connect_scan(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u32 n_ssids = 1;
	struct cfg80211_ssid *ssids = kunit_kzalloc(test, sizeof(struct cfg80211_ssid), GFP_KERNEL);
	struct ieee80211_channel *channel = NULL;

	sdev->wiphy->bands[NL80211_BAND_2GHZ] = kunit_kzalloc(test, sizeof(struct ieee80211_supported_band), GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_2GHZ]->n_channels = 1;
	sdev->wiphy->bands[NL80211_BAND_2GHZ]->channels = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	sdev->wiphy->bands[NL80211_BAND_2GHZ]->channels[0].hw_value = 1;
	sdev->wiphy->bands[NL80211_BAND_2GHZ]->channels[0].flags = 0;
	sdev->wiphy->bands[NL80211_BAND_2GHZ]->channels[0].band = NL80211_BAND_2GHZ;
	sdev->wiphy->bands[NL80211_BAND_2GHZ]->channels[0].center_freq = 2412;
	sdev->wiphy->bands[NL80211_BAND_2GHZ]->channels[0].freq_offset = 0;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->is_available = true;
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_req = kunit_kzalloc(test, sizeof(struct cfg80211_scan_request), GFP_KERNEL);
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results = kunit_kzalloc(test, sizeof(struct slsi_scan_result), GFP_KERNEL);
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->hidden = 76;
	ndev_vif->scan[SLSI_SCAN_HW_ID].scan_results->beacon = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_connect_scan(sdev, dev, n_ssids, ssids, channel));
}

static void test_slsi_mlme_powermgt_unlocked(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 power_mode = 0;
	int timeout = 200;

	ndev_vif->activated = false;
	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_powermgt_unlocked(sdev, dev, power_mode));

	ndev_vif->activated = true;
	ndev_vif->power_mode = power_mode;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_powermgt_unlocked(sdev, dev, power_mode));

	power_mode = 12;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_powermgt_unlocked(sdev, dev, power_mode));

	sdev->sig_wait.cfm = fapi_alloc(mlme_powermgt_cfm, MLME_POWERMGT_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_powermgt_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_powermgt_unlocked(sdev, dev, power_mode));

	power_mode = 2;
	sdev->sig_wait.cfm = fapi_alloc(mlme_powermgt_cfm, MLME_POWERMGT_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_powermgt_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_powermgt_unlocked(sdev, dev, power_mode));
}

static void test_slsi_mlme_powermgt(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 power_mode = 12;

	ndev_vif->activated = true;
	ndev_vif->power_mode = 0;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_powermgt(sdev, dev, power_mode));
}

static void test_slsi_mlme_synchronised_response(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_external_auth_params *params;

	params = kunit_kzalloc(test, sizeof(struct cfg80211_external_auth_params), GFP_KERNEL);
	params->status = FAPI_RESULTCODE_SUCCESS;
	memcpy(params->bssid, "HELLO_KUNIT", 12);

	ndev_vif->activated = false;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_synchronised_response(sdev, dev, params));

	ndev_vif->activated = true;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_synchronised_response(sdev, dev, params));
}

static void test_slsi_mlme_register_action_frame(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u32 af_bitmap_active = 0x12345678;
	u32 af_bitmap_suspended = 0x987654;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	KUNIT_EXPECT_EQ(test,
			-EIO,
			slsi_mlme_register_action_frame(sdev, dev, af_bitmap_active, af_bitmap_suspended));

	sdev->sig_wait.cfm = fapi_alloc(mlme_register_action_frame_cfm, MLME_REGISTER_ACTION_FRAME_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_register_action_frame_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_mlme_register_action_frame(sdev, dev, af_bitmap_active, af_bitmap_suspended));

	sdev->sig_wait.cfm = fapi_alloc(mlme_register_action_frame_cfm, MLME_REGISTER_ACTION_FRAME_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_register_action_frame_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_mlme_register_action_frame(sdev, dev, af_bitmap_active, af_bitmap_suspended));
}

static void test_slsi_mlme_channel_switch(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 center_freq = 2412;
	u16 chan_info = 0x12;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_channel_switch(sdev, dev, center_freq, chan_info));

	sdev->sig_wait.cfm = fapi_alloc(mlme_channel_switch_cfm, MLME_CHANNEL_SWITCH_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_channel_switch_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_channel_switch(sdev, dev, center_freq, chan_info));

	sdev->sig_wait.cfm = fapi_alloc(mlme_channel_switch_cfm, MLME_CHANNEL_SWITCH_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_channel_switch_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_channel_switch(sdev, dev, center_freq, chan_info));
}

static void test_slsi_mlme_add_info_elements(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 purpose = 12;
	u8 *ies = "\x12\x03\x54\x65\x23";
	u16 ies_len = 5;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_info_elements(sdev, dev, purpose, ies, ies_len));

	sdev->sig_wait.cfm = fapi_alloc(mlme_add_info_elements_cfm, MLME_ADD_INFO_ELEMENTS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_add_info_elements_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_add_info_elements(sdev, dev, purpose, ies, ies_len));

	sdev->sig_wait.cfm = fapi_alloc(mlme_add_info_elements_cfm, MLME_ADD_INFO_ELEMENTS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_add_info_elements_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_add_info_elements(sdev, dev, purpose, ies, ies_len));
}

static void test_slsi_mlme_send_frame_data(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct sk_buff *skb = fapi_alloc(mlme_send_frame_req, MLME_SEND_FRAME_REQ, 0, 100);
	u16 msg_type = FAPI_MESSAGETYPE_ARP;
	u16 host_tag = 0x0;
	u32 dwell_time = 0;
	u32 period = 100;

	atomic_set(&sdev->tx_host_tag[0], SLSI_HOST_TAG_ARP_MASK);
	sdev->hip.card_info.sdio_block_size = 345;
	ndev_vif->sta.m4_host_tag = 0x18;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	KUNIT_EXPECT_GT(test,
			NETDEV_TX_OK,
			slsi_mlme_send_frame_data(sdev, dev, skb, msg_type, host_tag, dwell_time, period));
	kfree_skb(skb);

	sdev->hip.card_info.sdio_block_size = 45;
	skb = fapi_alloc(mlme_send_frame_req, MLME_SEND_FRAME_REQ, 0, 100);
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_mlme_send_frame_data(sdev, dev, skb, msg_type, host_tag, dwell_time, period));
	kfree_skb(skb);

	msg_type = FAPI_MESSAGETYPE_EAPOL_KEY_M4;
	sdev->hip.card_info.sdio_block_size = 5;
	ndev_vif->mgmt_tx_cookie = 0x9876;

	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_mlme_send_frame_data(sdev, dev, skb, msg_type, host_tag, dwell_time, period));
	skb = fapi_alloc(mlme_send_frame_req, MLME_SEND_FRAME_REQ, 0, 100);
	skb->next = alloc_skb(200, GFP_KERNEL);
	skb_reserve(skb->next, 100);
	ndev_vif->mgmt_tx_cookie = 0x01;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_mlme_send_frame_data(sdev, dev, skb, msg_type, host_tag, dwell_time, period));
	kfree_skb(skb->next);

	ndev_vif->sta.is_wps = false;
	ndev_vif->sta.eap_hosttag = 0x12;
	ndev_vif->iftype = NL80211_IFTYPE_AP_VLAN;
	ndev_vif->netdev_ap = dev;
	msg_type = FAPI_MESSAGETYPE_EAP_MESSAGE;
	skb = fapi_alloc(mlme_send_frame_req, MLME_SEND_FRAME_REQ, 0, 100);
	skb->next = alloc_skb(200, GFP_KERNEL);
	skb_reserve(skb->next, 100);
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_mlme_send_frame_data(sdev, dev, skb, msg_type, host_tag, dwell_time, period));
	kfree_skb(skb->next);
}

static void test_slsi_mlme_send_frame_mgmt(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *frame = "\x12\x05\x23\x03\xA5\xB3\x43";
	int frame_len = 7;
	u16 data_desc = 0;
	u16 msg_type = FAPI_MESSAGETYPE_EAP_MESSAGE;
	u16 host_tag = 0x1234;
	u16 freq = 5200;
	u32 dwell_time = 100;
	u32 period = 200;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO,
			slsi_mlme_send_frame_mgmt(sdev, dev, frame, frame_len, data_desc,
						  msg_type, host_tag, freq, dwell_time, period));

	sdev->sig_wait.cfm = fapi_alloc(mlme_send_frame_cfm, MLME_SEND_FRAME_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_send_frame_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_mlme_send_frame_mgmt(sdev, dev, frame, frame_len, data_desc,
						  msg_type, host_tag, freq, dwell_time, period));

	sdev->sig_wait.cfm = fapi_alloc(mlme_send_frame_cfm, MLME_SEND_FRAME_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_send_frame_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0,
			slsi_mlme_send_frame_mgmt(sdev, dev, frame, frame_len, data_desc,
						  msg_type, host_tag, freq, dwell_time, period));

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_mlme_send_frame_mgmt(sdev, dev, frame, -100, data_desc,
						  msg_type, host_tag, freq, dwell_time, period));

	KUNIT_EXPECT_EQ(test, -ENOMEM,
			slsi_mlme_send_frame_mgmt(sdev, dev, frame, 999999999, data_desc,
						  msg_type, host_tag, freq, dwell_time, period));
}

static void test_slsi_mlme_wifisharing_permitted_channels(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 permitted_channels[5] = {1, 6, 9, 11, 149};
	int timeout = 200;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_wifisharing_permitted_channels(sdev, dev, permitted_channels));

	sdev->sig_wait.cfm = fapi_alloc(mlme_wifisharing_permitted_channels_cfm,
					MLME_WIFISHARING_PERMITTED_CHANNELS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_wifisharing_permitted_channels_cfm.result_code,
		     FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_wifisharing_permitted_channels(sdev, dev, permitted_channels));
}

static void test_slsi_mlme_reset_dwell_time(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->activated = true;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_reset_dwell_time(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_reset_dwell_time_cfm, MLME_RESET_DWELL_TIME_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_reset_dwell_time_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_reset_dwell_time(sdev, dev));
}

static void test_slsi_mlme_set_packet_filter(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_mlme_pkt_filter_elem *pkt_filter_elems = kunit_kzalloc(test,
									   sizeof(struct slsi_mlme_pkt_filter_elem),
									   GFP_KERNEL);
	u8 num_filters = 1;
	int pkt_filter_len = 1;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	ndev_vif->activated = true;
	pkt_filter_elems->num_pattern_desc = 1;
	pkt_filter_elems->pattern_desc[0].offset = 0;
	pkt_filter_elems->pattern_desc[0].mask_length = 5;
	memcpy(pkt_filter_elems->pattern_desc[0].mask, "\x01\x43\xA6\x7B\xCD", 5);
	memcpy(pkt_filter_elems->pattern_desc[0].pattern, "\x12\x34\x45\x56", 4);
	sdev->sig_wait.cfm = fapi_alloc(mlme_set_packet_filter_cfm, MLME_SET_PACKET_FILTER_CFM, 0, 10);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_set_packet_filter(sdev, dev, pkt_filter_len, num_filters, pkt_filter_elems));
}

static void test_slsi_mlme_roam(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *bssid = "KUNIT_HELLO";
	u16 freq = 5200;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->activated = true;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_roam(sdev, dev, bssid, freq));

	sdev->sig_wait.cfm = fapi_alloc(mlme_roam_cfm, MLME_ROAM_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_roam_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_roam(sdev, dev, bssid, freq));
}

static void test_slsi_mlme_wtc_mode_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int wtc_mode = 2;
	int scan_mode = FAPI_LOWLATENCYMODE_LOW_LATENCY_2;
	int rssi = -57;
	int rssi_th_2g = -27;
	int rssi_th_5g = -57;
	int rssi_th_6g = -97;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO,
			slsi_mlme_wtc_mode_req(sdev, dev, wtc_mode, scan_mode,
					       rssi, rssi_th_2g, rssi_th_5g, rssi_th_6g));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_wtc_mode_cfm, MLME_SET_WTC_MODE_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_wtc_mode_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_mlme_wtc_mode_req(sdev, dev, wtc_mode, scan_mode,
					       rssi, rssi_th_2g, rssi_th_5g, rssi_th_6g));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_wtc_mode_cfm, MLME_SET_WTC_MODE_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_wtc_mode_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_mlme_wtc_mode_req(sdev, dev, wtc_mode, scan_mode,
					       rssi, rssi_th_2g, rssi_th_5g, rssi_th_6g));
}

static void test_slsi_convert_cached_channel(struct kunit *test)
{
	enum nl80211_band band;
	u8 chan;
	u16 cached_channel = 149;

	slsi_convert_cached_channel(&band, &chan, cached_channel);
	KUNIT_EXPECT_EQ(test, chan, cached_channel);
	KUNIT_EXPECT_EQ(test, band, NL80211_BAND_5GHZ);
}

static void test_slsi_mlme_set_cached_channels(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u32 channels_count = 2;
	u16 channels[2] = {1, 149};
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_cached_channels(sdev, dev, channels_count, channels));

	channels_count = 0;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_cached_channels(sdev, dev, channels_count, channels));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_cached_channels_cfm, MLME_SET_CACHED_CHANNELS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_cached_channels_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_set_cached_channels(sdev, dev, channels_count, channels));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_cached_channels_cfm, MLME_SET_CACHED_CHANNELS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_cached_channels_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_set_cached_channels(sdev, dev, channels_count, channels));
}

static void test_slsi_mlme_set_acl(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 ifnum = 1;
	enum nl80211_acl_policy acl_policy = NL80211_ACL_POLICY_DENY_UNLESS_LISTED;
	int max_acl_entries = 2;
	struct mac_address mac_addrs[2];
	int timeout = 200;

	SLSI_ETHER_COPY(mac_addrs[0].addr, "\xAB\xB3\xC4\x65\x90\xA1");
	SLSI_ETHER_COPY(mac_addrs[1].addr, "\x12\x23\x34\x45\x56\x67");

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_acl(sdev, dev, 0, acl_policy, max_acl_entries, mac_addrs));

	ndev_vif->activated = true;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_acl(sdev, dev, ifnum, acl_policy, max_acl_entries, mac_addrs));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_set_acl_cfm, MLME_SET_ACL_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_acl_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_acl(sdev, dev, ifnum, acl_policy, max_acl_entries, mac_addrs));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_acl_cfm, MLME_SET_ACL_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_acl_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_acl(sdev, dev, ifnum, acl_policy, max_acl_entries, mac_addrs));
}

static void test_slsi_mlme_set_traffic_parameters(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 user_priority = 0x1;
	u16 medium_time = 130;
	u16 minimum_data_rate = 120;
	u8 *mac = "\x12\x23\x34\x45\x56\x67";
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;

	KUNIT_EXPECT_EQ(test, -EIO,
			slsi_mlme_set_traffic_parameters(sdev, dev, user_priority,
							 medium_time, minimum_data_rate, mac));

	KUNIT_EXPECT_EQ(test, -EIO,
			slsi_mlme_set_traffic_parameters(sdev, dev, user_priority,
							 medium_time, minimum_data_rate, NULL));

	sdev->sig_wait.cfm = fapi_alloc(mlme_set_traffic_parameters_cfm, MLME_SET_TRAFFIC_PARAMETERS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_traffic_parameters_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_mlme_set_traffic_parameters(sdev, dev, user_priority,
							 medium_time, minimum_data_rate, NULL));
}

static void test_slsi_mlme_del_traffic_parameters(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 user_priority = 0x11;
	int timeout = 200;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_del_traffic_parameters(sdev, dev, user_priority));

	sdev->sig_wait.cfm = fapi_alloc(mlme_del_traffic_parameters_cfm, MLME_DEL_TRAFFIC_PARAMETERS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_del_traffic_parameters_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_del_traffic_parameters(sdev, dev, user_priority));
}

static void test_slsi_mlme_set_ext_capab(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u8 data[7] = {'\x41', '\x42', '\x43', '\x44', '\x45', '\x46', '\x47'};
	int datalength = 7;
	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_ext_capab(sdev, dev, data, datalength));

	data[0] = 'K';
	data[1] = 'R';
	datalength = 2;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_ext_capab(sdev, dev, data, datalength));
}

static void test_slsi_mlme_tdls_action(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *peer = "\x12\x23\x34\x45\x56\x67";
	int action = 0;
	int timeout = 200;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_tdls_action(sdev, dev, peer, action));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	sdev->sig_wait.cfm = fapi_alloc(mlme_tdls_action_cfm, MLME_TDLS_ACTION_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_tdls_action_cfm.result_code, FAPI_RESULTCODE_NOT_SUPPORTED);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_tdls_action(sdev, dev, peer, action));

	sdev->sig_wait.cfm = fapi_alloc(mlme_tdls_action_cfm, MLME_TDLS_ACTION_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_tdls_action_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_tdls_action(sdev, dev, peer, action));
}

static void test_slsi_mlme_reassociate(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_reassociate(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_reassociate_cfm, MLME_REASSOCIATE_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_reassociate_cfm.result_code, FAPI_RESULTCODE_HOST_REQUEST_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_reassociate(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_reassociate_cfm, MLME_REASSOCIATE_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_reassociate_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_reassociate(sdev, dev));
}

static void test_slsi_mlme_reassociate_resp(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	slsi_mlme_reassociate_resp(sdev, dev);
}

static void test_slsi_mlme_add_range_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_rtt_config *nl_rtt_params = kunit_kzalloc(test, sizeof(struct slsi_rtt_config), GFP_KERNEL);
	u16 rtt_id = 0x98;
	u8 count = 2;
	u8 *source_addr = "\x12\x23\x34\x45\x56\x67";
	int timeout = 200;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_add_range_req(sdev, dev, count, nl_rtt_params, rtt_id, source_addr));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_add_range_cfm, MLME_ADD_RANGE_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_add_range_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_add_range_req(sdev, dev, count, nl_rtt_params, rtt_id, source_addr));

	ndev_vif->sig_wait.cfm = fapi_alloc(mlme_add_range_cfm, MLME_ADD_RANGE_CFM, 0, 10);
	fapi_set_u16(ndev_vif->sig_wait.cfm, u.mlme_add_range_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_add_range_req(sdev, dev, count, nl_rtt_params, rtt_id, source_addr));
}

static void test_slsi_del_range_cfm_validate(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct sk_buff *cfm = fapi_alloc(mlme_del_range_cfm, MLME_DEL_RANGE_CFM, 0, 10);

	fapi_set_u16(cfm, u.mlme_del_range_cfm.result_code, FAPI_RESULTCODE_SUCCESS);

	KUNIT_EXPECT_TRUE(test, slsi_del_range_cfm_validate(sdev, dev, cfm));

	cfm = fapi_alloc(mlme_del_range_cfm, MLME_DEL_RANGE_CFM, 0, 10);
	fapi_set_u16(cfm, u.mlme_del_range_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_FALSE(test, slsi_del_range_cfm_validate(sdev, dev, cfm));
}

static void test_slsi_mlme_del_range_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 count = 2;
	u8 *addr = "\x12\x23\x34\x45\x56\x67";
	u16 rtt_id = 0x58;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_del_range_req(sdev, dev, count, addr, rtt_id));
}

static void test_slsi_mlme_start_link_stats_req(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u16 mpdu_size_threshold = 0x123;
	bool aggressive_stats_enabled = false;
	struct netdev_vif *sndev_vif;
	int timeout = 200;

	KUNIT_EXPECT_EQ(test,
			-EIO,
			slsi_mlme_start_link_stats_req(sdev, mpdu_size_threshold, aggressive_stats_enabled));

	sndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);

	slsi_sig_send_init(&sndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	sndev_vif->sig_wait.cfm = fapi_alloc(mlme_start_link_statistics_cfm, MLME_START_LINK_STATISTICS_CFM, 0, 10);
	fapi_set_u16(sndev_vif->sig_wait.cfm, u.mlme_start_link_statistics_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	sndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_mlme_start_link_stats_req(sdev, mpdu_size_threshold, aggressive_stats_enabled));

	sndev_vif->sig_wait.cfm = fapi_alloc(mlme_start_link_statistics_cfm, MLME_START_LINK_STATISTICS_CFM, 0, 10);
	fapi_set_u16(sndev_vif->sig_wait.cfm, u.mlme_start_link_statistics_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_mlme_start_link_stats_req(sdev, mpdu_size_threshold, aggressive_stats_enabled));
}

static void test_slsi_mlme_stop_link_stats_req(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u16 stats_stop_mask = 0xFAFA;
	struct netdev_vif *sndev_vif;
	int timeout = 200;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_stop_link_stats_req(sdev, stats_stop_mask));

	sndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);

	slsi_sig_send_init(&sndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	sndev_vif->sig_wait.cfm = fapi_alloc(mlme_stop_link_statistics_cfm, MLME_STOP_LINK_STATISTICS_CFM, 0, 10);
	fapi_set_u16(sndev_vif->sig_wait.cfm, u.mlme_stop_link_statistics_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	sndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_stop_link_stats_req(sdev, stats_stop_mask));

	sndev_vif->sig_wait.cfm = fapi_alloc(mlme_stop_link_statistics_cfm, MLME_STOP_LINK_STATISTICS_CFM, 0, 10);
	fapi_set_u16(sndev_vif->sig_wait.cfm, u.mlme_stop_link_statistics_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_stop_link_stats_req(sdev, stats_stop_mask));
}

static void test_slsi_mlme_set_rssi_monitor(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 enable = true;
	s8 low_rssi_threshold = -76;
	s8 high_rssi_threshold = -50;
	int timeout = 200;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_rssi_monitor(sdev, dev, enable, low_rssi_threshold, high_rssi_threshold));

	sdev->sig_wait.cfm = fapi_alloc(mlme_monitor_rssi_cfm, MLME_MONITOR_RSSI_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_monitor_rssi_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_rssi_monitor(sdev, dev, enable, low_rssi_threshold, high_rssi_threshold));
}

static void test_slsi_read_mibs(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int mib_count = 0;
	struct slsi_mib_data *mibrsp = kunit_kzalloc(test, sizeof(struct slsi_mib_data), GFP_KERNEL);
	struct slsi_mib_get_entry *mib_entries = kunit_kzalloc(test, sizeof(struct slsi_mib_get_entry), GFP_KERNEL);
	int timeout = 200;

	KUNIT_EXPECT_FALSE(test, slsi_read_mibs(sdev, dev, mib_entries, mib_count, mibrsp));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_get_cfm, MLME_GET_CFM, 0, 10);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_FALSE(test, slsi_read_mibs(sdev, dev, mib_entries, mib_count, mibrsp));
}

static void test_slsi_mlme_set_ctwindow(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 200;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_ctwindow(sdev, dev, 10));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_set_ctwindow_cfm, MLME_SET_CTWINDOW_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_ctwindow_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_ctwindow(sdev, dev, 10));
}

static void test_slsi_mlme_set_p2p_noa(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	unsigned int noa_count = 3;
	unsigned int interval = 100;
	unsigned int duration = 200;
	int timeout = 200;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->ap.beacon_interval = 59;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_p2p_noa(sdev, dev, noa_count, interval, duration));

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_p2p_noa(sdev, dev, noa_count, 0, duration));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_set_noa_cfm, MLME_SET_NOA_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_noa_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_p2p_noa(sdev, dev, noa_count, interval, duration));
}

static void test_slsi_mlme_set_host_state(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	u16 host_state = 0x0010;
	struct netdev_vif *sndev_vif;
	int timeout = 200;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_host_state(sdev, dev, host_state));

	sndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);

	slsi_sig_send_init(&sndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	sndev_vif->sig_wait.cfm = fapi_alloc(mlme_host_state_cfm, MLME_HOST_STATE_CFM, 0, 10);
	fapi_set_u16(sndev_vif->sig_wait.cfm, u.mlme_host_state_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	sndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_host_state(sdev, dev, host_state));

	sndev_vif->sig_wait.cfm = fapi_alloc(mlme_host_state_cfm, MLME_HOST_STATE_CFM, 0, 10);
	fapi_set_u16(sndev_vif->sig_wait.cfm, u.mlme_host_state_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_host_state(sdev, dev, host_state));
}

static void test_slsi_mlme_read_apf_request(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *host_state;
	int datalen;
	int timeout = 200;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->activated = false;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_read_apf_request(sdev, dev, &host_state, &datalen));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_read_apf_request(sdev, dev, &host_state, &datalen));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_read_apf_request(sdev, dev, &host_state, &datalen));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_read_apf_cfm, MLME_READ_APF_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_read_apf_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_read_apf_request(sdev, dev, &host_state, &datalen));
}

static void test_slsi_mlme_install_apf_request(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 program = NULL;
	u32 program_len = 99999946;
	int timeout;

	ndev_vif->activated = false;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_install_apf_request(sdev, dev, program, program_len));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_install_apf_request(sdev, dev, program, program_len));

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_install_apf_request(sdev, dev, program, program_len));

	program_len = 0;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_install_apf_request(sdev, dev, program, program_len));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_install_apf_cfm, MLME_INSTALL_APF_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_install_apf_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_install_apf_request(sdev, dev, program, program_len));
}

static void test_slsi_mlme_start_detect_request(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 200;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_start_detect_request(sdev, dev));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_start_detect_cfm, MLME_START_DETECT_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_start_detect_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_start_detect_request(sdev, dev));
}

static void test_slsi_test_sap_configure_monitor_mode(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_chan_def *chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);
	int timeout = 200;

	chandef->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	chandef->width = NL80211_CHAN_WIDTH_80;
	chandef->chan->center_freq = 5270;
	chandef->center_freq1 = 5200;
	chandef->center_freq2 = 5340;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_test_sap_configure_monitor_mode(sdev, dev, chandef));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_set_channel_cfm, TEST_CONFIGURE_MONITOR_MODE_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_channel_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_test_sap_configure_monitor_mode(sdev, dev, chandef));
}

static void test_slsi_mlme_delba_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 *peer_qsta_address = SLSI_DEFAULT_HW_MAC_ADDR;
	u16 priority = 0x12;
	u16 direction = 0x1;
	u16 sq_num = 0x3;
	u16 reason_code = FAPI_RESULTCODE_UNSPECIFIED_FAILURE;
	int timeout = 200;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);

	KUNIT_EXPECT_EQ(test, -EIO,
			slsi_mlme_delba_req(sdev, dev, peer_qsta_address, priority, direction, sq_num, reason_code));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_delba_cfm, MLME_DELBA_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_delba_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0,
			slsi_mlme_delba_req(sdev, dev, peer_qsta_address, priority, direction, sq_num, reason_code));
}

static void test_slsi_mlme_set_country(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	char alpha2[2] = {'K', 'R'};
	struct netdev_vif *sndev_vif;
	struct netdev_vif *p2pndev_vif;
	int timeout = 200;

	sdev->hip.hip_control = kunit_kzalloc(test, sizeof(*sdev->hip.hip_control), GFP_KERNEL);
	sdev->regdb.regdb_state = SLSI_REG_DB_SET;
	sdev->regdb.num_countries = 1;
	sdev->forced_bandwidth = 20;
	sdev->regdb.country = kunit_kzalloc(test, sizeof(struct regdb_file_reg_country), GFP_KERNEL);

	sdev->regdb.country[0].alpha2[0] = 'K';
	sdev->regdb.country[0].alpha2[1] = 'O';

	sdev->regdb.country[0].collection = kunit_kzalloc(test, sizeof(struct regdb_file_reg_rules_collection),
							  GFP_KERNEL);
	sdev->regdb.country[0].collection->reg_rule_num = 3;
	sdev->regdb.country[0].collection->reg_rule[0] = kunit_kzalloc(test, sizeof(struct regdb_file_reg_rule),
								       GFP_KERNEL);
	sdev->regdb.country[0].collection->reg_rule[0]->freq_range = kunit_kzalloc(test,
										   sizeof(struct regdb_file_freq_range),
										   GFP_KERNEL);
	sdev->regdb.country[0].collection->reg_rule[0]->freq_range->start_freq = 57000;
	sdev->regdb.country[0].collection->reg_rule[0]->freq_range->max_bandwidth = 160;

	sdev->regdb.country[0].collection->reg_rule[1] = kunit_kzalloc(test, sizeof(struct regdb_file_reg_rule),
								       GFP_KERNEL);
	sdev->regdb.country[0].collection->reg_rule[1]->freq_range = kunit_kzalloc(test,
										   sizeof(struct regdb_file_freq_range),
										   GFP_KERNEL);
	sdev->regdb.country[0].collection->reg_rule[1]->freq_range->start_freq = 5190;
	sdev->regdb.country[0].collection->reg_rule[1]->freq_range->end_freq = 5210;
	sdev->regdb.country[0].collection->reg_rule[1]->freq_range->max_bandwidth = 20;

	sdev->regdb.country[0].collection->reg_rule[2] = kunit_kzalloc(test, sizeof(struct regdb_file_reg_rule),
								       GFP_KERNEL);
	sdev->regdb.country[0].collection->reg_rule[2]->freq_range = kunit_kzalloc(test,
										   sizeof(struct regdb_file_freq_range),
										   GFP_KERNEL);
	sdev->regdb.country[0].collection->reg_rule[2]->freq_range->start_freq = 2426;
	sdev->regdb.country[0].collection->reg_rule[2]->freq_range->end_freq = 2448;
	sdev->regdb.country[0].collection->reg_rule[2]->freq_range->max_bandwidth = 20;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_country(sdev, alpha2));

	sdev->regdb.country[0].alpha2[1] = 'R';

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_country(sdev, alpha2));

	sndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_WLAN]);

	slsi_sig_send_init(&sndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	sndev_vif->sig_wait.cfm = fapi_alloc(mlme_set_country_cfm, MLME_SET_COUNTRY_CFM, 0, 10);
	fapi_set_u16(sndev_vif->sig_wait.cfm, u.mlme_set_country_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	sndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_set_country(sdev, alpha2));

	test_netdev_init(test, sdev, SLSI_NET_INDEX_P2P);
	p2pndev_vif = netdev_priv(sdev->netdev[SLSI_NET_INDEX_P2P]);
	sndev_vif->mgmt_tx_cookie = 0xABCD;
	sndev_vif->sig_wait.cfm = fapi_alloc(mlme_set_country_cfm, MLME_SET_COUNTRY_CFM, 0, 10);
	fapi_set_u16(sndev_vif->sig_wait.cfm, u.mlme_set_country_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	p2pndev_vif->is_available = true;
	p2pndev_vif->sig_wait.cfm = fapi_alloc(mlme_set_country_cfm, MLME_SET_COUNTRY_CFM, 0, 10);
	fapi_set_u16(p2pndev_vif->sig_wait.cfm, u.mlme_set_country_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_country(sdev, alpha2));
}

static void test_slsi_mlme_configure_monitor_mode(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_chan_def *chandef = kunit_kzalloc(test, sizeof(struct cfg80211_chan_def), GFP_KERNEL);
	int timeout = 200;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	chandef->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	chandef->width = NL80211_CHAN_WIDTH_80;
	chandef->chan->center_freq = 5270;
	chandef->center_freq1 = 5200;
	chandef->center_freq2 = 5340;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_configure_monitor_mode(sdev, dev, chandef));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_set_channel_cfm, MLME_CONFIGURE_MONITOR_MODE_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_channel_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_configure_monitor_mode(sdev, dev, chandef));
}

static void test_slsi_mlme_set_multicast_ip(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	__be32 multicast_ip_list[3] = {123456787, 98765432, 283746566};
	int count = 3;
	int timeout = 200;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_multicast_ip(sdev, dev, multicast_ip_list, count));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_set_ip_address_cfm, MLME_SET_IP_ADDRESS_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_set_ip_address_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_mlme_set_multicast_ip(sdev, dev, multicast_ip_list, count));
}

static void test_slsi_mlme_twt_setup(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct twt_setup *tsetup = kunit_kzalloc(test, sizeof(struct twt_setup), GFP_KERNEL);
	int timeout = 200;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	tsetup->setup_id = 0x1294;
	tsetup->negotiation_type = 0x1;
	tsetup->flow_type = 0x2;
	tsetup->trigger_type = 0x3;
	tsetup->d_wake_duration = 500;
	tsetup->d_wake_interval = 100;
	tsetup->d_wake_time = 200;
	tsetup->min_wake_interval = 100;
	tsetup->max_wake_interval = 500;
	tsetup->min_wake_duration = 500;
	tsetup->max_wake_duration = 1000;
	tsetup->avg_pkt_num = 2;
	tsetup->avg_pkt_size = 4;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_twt_setup(sdev, dev, tsetup));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_twt_setup_cfm, MLME_TWT_SETUP_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_twt_setup_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -99, slsi_mlme_twt_setup(sdev, dev, tsetup));
}

static void test_slsi_mlme_twt_teardown(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 setup_id = 0x12;
	u16 negotiation_type = 0x32;
	int timeout = 200;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_twt_teardown(sdev, dev, setup_id, negotiation_type));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_twt_teardown_cfm, MLME_TWT_TEARDOWN_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_twt_teardown_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_EQ(test, -99, slsi_mlme_twt_teardown(sdev, dev, setup_id, negotiation_type));
}

static void test_slsi_mlme_twt_status_query(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	char buf[256] = {0};
	u16 setup_id = 1;
	int command_pos = 0;
	int ret = 0;
	int timeout = 200;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_twt_status_query(sdev, dev, buf, sizeof(buf), &command_pos, setup_id));

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);
	sdev->sig_wait.cfm = fapi_alloc(mlme_twt_status_query_cfm, MLME_TWT_STATUS_QUERY_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_twt_status_query_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;

	KUNIT_EXPECT_NE(test, -EINVAL, slsi_mlme_twt_status_query(sdev, dev, buf, sizeof(buf), &command_pos, setup_id));
}

static void test_slsi_mlme_set_key(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u16 key_id = 0x12;
	u16 key_type = FAPI_KEYTYPE_GROUP;
	u8 *address = SLSI_DEFAULT_HW_MAC_ADDR;
	struct key_params *key = kunit_kzalloc(test, sizeof(struct key_params), GFP_KERNEL);

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	key->seq_len = 10;
	key->key_len = 100;
	key->seq = "\x12\x34\x45\x56\x77\x23\x12";
	key->cipher = WLAN_CIPHER_SUITE_WEP104;

	KUNIT_EXPECT_EQ(test, -EIO, slsi_mlme_set_key(sdev, dev, key_id, key_type, address, key));
}

static void test_slsi_mlme_del_vif(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int timeout = 200;

	SLSI_MUTEX_LOCK(ndev_vif->vif_mutex);
	ndev_vif->activated = true;
	ndev_vif->ifnum = SLSI_NET_INDEX_P2P;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->delete_probe_req_ies = true;

	slsi_sig_send_init(&ndev_vif->sig_wait);
	slsi_sig_send_init(&sdev->sig_wait);

	sdev->sig_wait.cfm = fapi_alloc(mlme_del_vif_cfm, MLME_DEL_VIF_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_del_vif_cfm.result_code, FAPI_RESULTCODE_SUCCESS);
	ndev_vif->mgmt_tx_cookie = 0x1234;
	sdev->mlme_blocked = false;
	sdev->wlan_service_on = true;
	sdev->sig_wait_cfm_timeout = &timeout;
	ndev_vif->probe_req_ies = kmalloc(sizeof(u8) * 10, GFP_KERNEL);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_del_vif(sdev, dev));

	sdev->sig_wait.cfm = fapi_alloc(mlme_del_vif_cfm, MLME_DEL_VIF_CFM, 0, 10);
	fapi_set_u16(sdev->sig_wait.cfm, u.mlme_del_vif_cfm.result_code, FAPI_RESULTCODE_UNSPECIFIED_FAILURE);

	KUNIT_EXPECT_EQ(test, 0, slsi_mlme_del_vif(sdev, dev));
}

/*
 * Test fictures
 */
static int mlme_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s: initialized.", __func__);
	return 0;
}

static void mlme_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
	return;
}

/*
 * KUnit testcase definitions
 */
static struct kunit_case mlme_test_cases[] = {
	KUNIT_CASE(test_slsi_mlme_set_country),
	KUNIT_CASE(test_slsi_mlme_wait_for_cfm),
	KUNIT_CASE(test_panic_on_lost_ind),
	KUNIT_CASE(test_slsi_mlme_wait_for_ind),
	KUNIT_CASE(test_slsi_mlme_tx_rx),
	KUNIT_CASE(test_slsi_mlme_req),
	KUNIT_CASE(test_slsi_mlme_req_ind),
	KUNIT_CASE(test_slsi_mlme_req_no_cfm),
	KUNIT_CASE(test_slsi_mlme_req_cfm),
	KUNIT_CASE(test_slsi_mlme_req_cfm_mib),
	KUNIT_CASE(test_slsi_mlme_req_cfm_ind),
	KUNIT_CASE(test_slsi_get_reg_rule),
	KUNIT_CASE(test_slsi_compute_chann_info),
	KUNIT_CASE(test_slsi_get_chann_info),
	KUNIT_CASE(test_slsi_check_channelization),
	KUNIT_CASE(test_mib_buffer_dump_to_log),
	KUNIT_CASE(test_slsi_mlme_set_ip_address),
#if IS_ENABLED(CONFIG_IPV6)
	KUNIT_CASE(test_slsi_mlme_set_ipv6_address),
#endif
	KUNIT_CASE(test_slsi_mlme_set_with_cfm),
	KUNIT_CASE(test_slsi_mlme_set),
	KUNIT_CASE(test_slsi_mlme_get),
	KUNIT_CASE(test_slsi_mlme_add_vif),
	KUNIT_CASE(test_slsi_mlme_add_detect_vif),
	KUNIT_CASE(test_slsi_mlme_del_detect_vif),
	KUNIT_CASE(test_slsi_mlme_set_band_req),
	KUNIT_CASE(test_slsi_mlme_set_scan_mode_req),
	KUNIT_CASE(test_slsi_mlme_set_roaming_parameters),
	KUNIT_CASE(test_slsi_mlme_set_channel),
	KUNIT_CASE(test_slsi_mlme_unset_channel_req),
	KUNIT_CASE(test_slsi_ap_obss_scan_done_ind),
	KUNIT_CASE(test_slsi_scan_cfm_validate),
	KUNIT_CASE(test_slsi_mlme_set_channels_ie),
	KUNIT_CASE(test_slsi_mlme_append_gscan_channel_list),
	KUNIT_CASE(test_slsi_mlme_is_support_band),
	KUNIT_CASE(test_slsi_mlme_get_channel_list_ie_size),
	KUNIT_CASE(test_slsi_mlme_append_channel_ie),
	KUNIT_CASE(test_slsi_mlme_append_channel_list_data),
	KUNIT_CASE(test_slsi_mlme_append_channel_list),
	KUNIT_CASE(test_slsi_set_scan_params),
	KUNIT_CASE(test_slsi_mlme_add_sched_scan),
	KUNIT_CASE(test_slsi_mlme_add_scan),
	KUNIT_CASE(test_slsi_mlme_del_scan),
	KUNIT_CASE(test_slsi_ap_add_ext_capab_ie),
	KUNIT_CASE(test_slsi_prepare_country_ie),
	KUNIT_CASE(test_slsi_modify_ies),
	KUNIT_CASE(test_slsi_mlme_start_prepare_ies),
	KUNIT_CASE(test_slsi_prepare_vht_ies),
	KUNIT_CASE(test_slsi_mlme_start),
	KUNIT_CASE(test_slsi_mlme_connect_get_sec_ie),
	KUNIT_CASE(test_slsi_mlme_connect_info_elems_ie_prep),
	KUNIT_CASE(test_slsi_mlme_connect_info_elements),
	KUNIT_CASE(test_slsi_mlme_connect),
	KUNIT_CASE(test_slsi_mlme_connect_resp),
	KUNIT_CASE(test_slsi_mlme_connected_resp),
	KUNIT_CASE(test_slsi_mlme_roamed_resp),
	KUNIT_CASE(test_slsi_disconnect_cfm_validate),
	KUNIT_CASE(test_slsi_roaming_channel_list_cfm_validate),
	KUNIT_CASE(test_slsi_mlme_roaming_channel_list_req),
	KUNIT_CASE(test_slsi_mlme_disconnect),
	KUNIT_CASE(test_slsi_mlme_get_key),
	KUNIT_CASE(test_slsi_decode_fw_rate),
	KUNIT_CASE(test_slsi_mlme_get_sinfo_mib),
	KUNIT_CASE(test_slsi_mlme_connect_scan),
	KUNIT_CASE(test_slsi_mlme_powermgt_unlocked),
	KUNIT_CASE(test_slsi_mlme_powermgt),
	KUNIT_CASE(test_slsi_mlme_synchronised_response),
	KUNIT_CASE(test_slsi_mlme_register_action_frame),
	KUNIT_CASE(test_slsi_mlme_channel_switch),
	KUNIT_CASE(test_slsi_mlme_add_info_elements),
	KUNIT_CASE(test_slsi_mlme_send_frame_data),
	KUNIT_CASE(test_slsi_mlme_send_frame_mgmt),
	KUNIT_CASE(test_slsi_mlme_wifisharing_permitted_channels),
	KUNIT_CASE(test_slsi_mlme_reset_dwell_time),
	KUNIT_CASE(test_slsi_mlme_set_packet_filter),
	KUNIT_CASE(test_slsi_mlme_roam),
	KUNIT_CASE(test_slsi_mlme_wtc_mode_req),
	KUNIT_CASE(test_slsi_convert_cached_channel),
	KUNIT_CASE(test_slsi_mlme_set_cached_channels),
	KUNIT_CASE(test_slsi_mlme_set_acl),
	KUNIT_CASE(test_slsi_mlme_set_traffic_parameters),
	KUNIT_CASE(test_slsi_mlme_del_traffic_parameters),
	KUNIT_CASE(test_slsi_mlme_set_ext_capab),
	KUNIT_CASE(test_slsi_mlme_tdls_action),
	KUNIT_CASE(test_slsi_mlme_reassociate),
	KUNIT_CASE(test_slsi_mlme_reassociate_resp),
	KUNIT_CASE(test_slsi_mlme_add_range_req),
	KUNIT_CASE(test_slsi_del_range_cfm_validate),
	KUNIT_CASE(test_slsi_mlme_del_range_req),
	KUNIT_CASE(test_slsi_mlme_start_link_stats_req),
	KUNIT_CASE(test_slsi_mlme_stop_link_stats_req),
	KUNIT_CASE(test_slsi_mlme_set_rssi_monitor),
	KUNIT_CASE(test_slsi_read_mibs),
	KUNIT_CASE(test_slsi_mlme_set_ctwindow),
	KUNIT_CASE(test_slsi_mlme_set_p2p_noa),
	KUNIT_CASE(test_slsi_mlme_set_host_state),
	KUNIT_CASE(test_slsi_mlme_read_apf_request),
	KUNIT_CASE(test_slsi_mlme_install_apf_request),
	KUNIT_CASE(test_slsi_mlme_start_detect_request),
	KUNIT_CASE(test_slsi_test_sap_configure_monitor_mode),
	KUNIT_CASE(test_slsi_mlme_delba_req),
	KUNIT_CASE(test_slsi_mlme_configure_monitor_mode),
	KUNIT_CASE(test_slsi_mlme_set_multicast_ip),
	KUNIT_CASE(test_slsi_mlme_twt_setup),
	KUNIT_CASE(test_slsi_mlme_twt_teardown),
	KUNIT_CASE(test_slsi_mlme_twt_status_query),
	KUNIT_CASE(test_slsi_mlme_set_key),
	KUNIT_CASE(test_slsi_mlme_del_vif),
	{}
};

static struct kunit_suite mlme_test_suite[] = {
	{
		.name = "kunit-mlme-test",
		.test_cases = mlme_test_cases,
		.init = mlme_test_init,
		.exit = mlme_test_exit,
	}};

kunit_test_suites(mlme_test_suite);
