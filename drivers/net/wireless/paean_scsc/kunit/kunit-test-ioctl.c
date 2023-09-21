/* kunit-test-ioctl.c */
#include <kunit/test.h>

#include "kunit-mock-mgt.h"
#include "kunit-mock-mlme.h"
#include "kunit-mock-netif.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mib.h"
#include "../ioctl.c"

#define TEST_TO_DEV(test) ((struct net_device *)(test)->priv)

static u8 test_ioctl_cmd_buf[MAX_LEN_PRIV_COMMAND]; /* max length from ioctl.c */

static u8 *cbuf(u8 *cmd)
{
	snprintf(test_ioctl_cmd_buf, sizeof(test_ioctl_cmd_buf), "%s", cmd);
	return test_ioctl_cmd_buf;
}

static int cbuf_size(void)
{
	return (int)sizeof(test_ioctl_cmd_buf);
}

static void test_dev_init(struct kunit *test)
{
	struct wiphy *wiphy = NULL;
	struct slsi_dev *sdev = NULL;
	struct net_device *dev = NULL;
	struct netdev_vif *ndev_vif = NULL;

	/* wiphy_new */
	wiphy = kunit_kzalloc(test, sizeof(struct wiphy) + sizeof(struct slsi_dev), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, wiphy);

	sdev = SDEV_FROM_WIPHY(wiphy);
	sdev->wiphy = wiphy;

	/* alloc_netdev_mqs */
	dev = kunit_kzalloc(test, sizeof(struct net_device) + sizeof(struct netdev_vif), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, dev);

	ndev_vif = netdev_priv(dev);
	ndev_vif->sdev = sdev;

	test->priv = dev;
}

static struct ieee80211_rate dummy_rates[] = {
	{.bitrate = 10, .hw_value = 2, },
};

static struct ieee80211_channel dummy_ch_2ghz[] = {
	{.center_freq = 2412, .hw_value = 1, },
	{.center_freq = 2462, .hw_value = 11, },
};

static struct ieee80211_supported_band dummy_band_2ghz = {
	.channels   = dummy_ch_2ghz,
	.band       = NL80211_BAND_2GHZ,
	.n_channels = ARRAY_SIZE(dummy_ch_2ghz),
	.bitrates   = dummy_rates,
	.n_bitrates = ARRAY_SIZE(dummy_rates),
};

static struct ieee80211_channel dummy_ch_5ghz[] = {
	{.center_freq = 5200, .hw_value = 40, },
	{.center_freq = 5825, .hw_value = 165, },
};

static struct ieee80211_supported_band dummy_band_5ghz = {
	.channels   = dummy_ch_5ghz,
	.band       = NL80211_BAND_5GHZ,
	.n_channels = ARRAY_SIZE(dummy_ch_5ghz),
	.bitrates   = dummy_rates,
	.n_bitrates = ARRAY_SIZE(dummy_rates),
};

static struct ieee80211_channel dummy_ch_6ghz[] = {
	{.center_freq = 6135, .hw_value = 37, },
	{.center_freq = 7115, .hw_value = 233, },
};

static struct ieee80211_supported_band dummy_band_6ghz = {
	.channels   = dummy_ch_6ghz,
	.band       = NL80211_BAND_6GHZ,
	.n_channels = ARRAY_SIZE(dummy_ch_6ghz),
	.bitrates   = dummy_rates,
	.n_bitrates = ARRAY_SIZE(dummy_rates),
};

static void test_init_sta_bss(struct kunit *test, struct ieee80211_channel *chan)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ndev_vif->sta.sta_bss);

	ndev_vif->sta.sta_bss->channel = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ndev_vif->sta.sta_bss->channel);

	ndev_vif->sta.sta_bss->channel->band = chan->band;
	ndev_vif->sta.sta_bss->channel->center_freq = chan->center_freq;
	ndev_vif->sta.sta_bss->channel->hw_value = chan->hw_value;

	ndev_vif->sta.sta_bss->ies = kunit_kzalloc(test, sizeof(struct cfg80211_bss_ies __rcu), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ndev_vif->sta.sta_bss->ies);
}

static void test_ioctl_slsi_set_suspend_mode(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_suspend_mode(dev, cbuf("SETSUSPENDMODE 3"), cbuf_size()));

	sdev->device_config.user_suspend_mode = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_suspend_mode(dev, cbuf("SETSUSPENDMODE 0"), cbuf_size()));

	sdev->device_config.user_suspend_mode = 0;
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->igmp_offload_activated = 1;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_suspend_mode(dev, cbuf("SETSUSPENDMODE 1"), cbuf_size()));

	sdev->device_config.user_suspend_mode = 1;
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_suspend_mode(dev, cbuf("SETSUSPENDMODE 0"), cbuf_size()));
}

static void test_ioctl_slsi_set_p2p_oppps(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_p2p_oppps(dev, cbuf("P2P_SET_PS 20"), cbuf_size()));

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_p2p_oppps(dev, cbuf("P2P_SET_PS 2 0 20"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_p2p_oppps(dev, cbuf("P2P_SET_PS - 0 20"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_p2p_oppps(dev, cbuf("P2P_SET_PS 2 - 20"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_p2p_oppps(dev, cbuf("P2P_SET_PS 2 0 -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_p2p_oppps(dev, cbuf("P2P_SET_PS 2 0 21"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_p2p_oppps(dev, cbuf("P2P_SET_PS 2 1 21"), cbuf_size()));
}

static void test_ioctl_slsi_p2p_set_noa_params(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_set_noa_params(dev, cbuf("P2P_SET_NOA 30"), cbuf_size()));

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_set_noa_params(dev, cbuf("P2P_SET_NOA 255 100 30"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->iftype = NL80211_IFTYPE_P2P_GO;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_set_noa_params(dev, cbuf("P2P_SET_NOA - 100 30"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_set_noa_params(dev, cbuf("P2P_SET_NOA 255 - 30"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_set_noa_params(dev, cbuf("P2P_SET_NOA 255 100 -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_p2p_set_noa_params(dev, cbuf("P2P_SET_NOA 255 100 30"), cbuf_size()));
}

static void test_ioctl_slsi_p2p_ecsa(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct cfg80211_chan_def chandef;

	chandef.chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, chandef.chan);
	chandef.chan->center_freq = 5775;
	chandef.chan->band = NL80211_BAND_2GHZ;

	slsi_p2p_ecsa(dev, cbuf("P2P_ECSA 1"), cbuf_size());

	slsi_p2p_ecsa(dev, cbuf("P2P_ECSA 44 160"), cbuf_size());

	sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN] = kunit_kzalloc(test, sizeof(struct net_device), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sdev->netdev[SLSI_NET_INDEX_P2PX_SWLAN]);
	slsi_p2p_ecsa(dev, cbuf("P2P_ECSA - 20"), cbuf_size());

	slsi_p2p_ecsa(dev, cbuf("P2P_ECSA 166 20"), cbuf_size());

	slsi_p2p_ecsa(dev, cbuf("P2P_ECSA 1 -"), cbuf_size());

	sdev->band_5g_supported = true;
	slsi_p2p_ecsa(dev, cbuf("P2P_ECSA 36 80"), cbuf_size());
	slsi_p2p_ecsa(dev, cbuf("P2P_ECSA 165 80"), cbuf_size());
}

static void test_ioctl_slsi_set_ap_p2p_wps_ie(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->activated = true;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 1 2"), cbuf_size());

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 2 2"), cbuf_size());
	ndev_vif->activated = false;
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 3 2"), cbuf_size());

	ndev_vif->activated = false;
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 1 2"), cbuf_size());
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 1"), cbuf_size());
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE - 2"), cbuf_size());
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 1 -"), cbuf_size());

	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 1 1"), cbuf_size());
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 2 1"), cbuf_size());
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 1 2"), cbuf_size());

	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 0 2"), cbuf_size());
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 1 2"), cbuf_size());
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 2 2"), cbuf_size());
	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 3 2"), cbuf_size());

	slsi_set_ap_p2p_wps_ie(dev, cbuf("SET_AP_P2P_WPS_IE 0 2"), cbuf_size());
}

static void test_ioctl_slsi_p2p_lo_start(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_lo_start(dev, cbuf("P2P_LO_START 1500 5000 7"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_lo_start(dev, cbuf("P2P_LO_START 0 1500 5000 7"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_lo_start(dev, cbuf("P2P_LO_START - 1500 5000 7"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_lo_start(dev, cbuf("P2P_LO_START 44 - 5000 7"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_lo_start(dev, cbuf("P2P_LO_START 44 1500 - 7"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_lo_start(dev, cbuf("P2P_LO_START 44 1500 5000 -"), cbuf_size()));

	ndev_vif->activated = false;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_lo_start(dev, cbuf("P2P_LO_START 36 1500 5000 6"), cbuf_size()));

	sdev->wiphy->bands[NL80211_BAND_2GHZ] = &dummy_band_2ghz;

	ndev_vif->chan = kunit_kzalloc(test, sizeof(struct ieee80211_channel), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ndev_vif->chan);
	ndev_vif->chan->hw_value = 155;
	ndev_vif->chan->center_freq = 5775;

	ndev_vif->activated = true;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_p2p_lo_start(dev, cbuf("P2P_LO_START 1 1500 5000 7"), cbuf_size()));

	sdev->p2p_state = P2P_SCANNING;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_p2p_lo_start(dev, cbuf("P2P_LO_START 44 1500 5000 8"), cbuf_size()));
}

static void test_ioctl_slsi_p2p_lo_stop(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	KUNIT_EXPECT_EQ(test, 0, slsi_p2p_lo_stop(dev, cbuf("P2P_LO_STOP 44 1500 5000 7"), cbuf_size()));

	ndev_vif->sdev->p2p_state = P2P_LISTENING;
	KUNIT_EXPECT_EQ(test, 0, slsi_p2p_lo_stop(dev, cbuf("P2P_LO_STOP 44 1500 5000 8"), cbuf_size()));
}

static void test_ioctl_slsi_rx_filter_add(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_rx_filter_add(dev, cbuf("RXFILTER-ADD 4"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_rx_filter_add(dev, cbuf("RXFILTER-ADD 2"), cbuf_size()));
}

static void test_ioctl_slsi_rx_filter_remove(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, 0, slsi_rx_filter_remove(dev, cbuf("RXFILTER-REMOVE"), cbuf_size()));
}

static void test_ioctl_slsi_legacy_roam_trigger_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	slsi_legacy_roam_trigger_write(dev, cbuf("SETROAMTRIGGER_LEGACY -"), cbuf_size());

	slsi_legacy_roam_trigger_write(dev, cbuf("SETROAMTRIGGER_LEGACY -120"), cbuf_size());

	sdev->device_config.ncho_mode = 1;
	slsi_legacy_roam_trigger_write(dev, cbuf("SETROAMTRIGGER_LEGACY -60"), cbuf_size());

	sdev->device_config.ncho_mode = 0;
	ndev_vif->sta.vif_status = 1;
	slsi_legacy_roam_trigger_write(dev, cbuf("SETROAMTRIGGER_LEGACY -70"), cbuf_size());

	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	slsi_legacy_roam_trigger_write(dev, cbuf("SETROAMTRIGGER_LEGACY -80"), cbuf_size());
}

static void test_ioctl_slsi_legacy_roam_scan_trigger_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 1;
	slsi_legacy_roam_scan_trigger_read(dev, cbuf(""), cbuf_size());

	sdev->device_config.ncho_mode = 0;
	slsi_legacy_roam_scan_trigger_read(dev, cbuf(""), cbuf_size());
}

static void test_ioctl_slsi_roam_add_scan_frequencies_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct ieee80211_channel chan = {.band = NL80211_BAND_5GHZ, .center_freq = 5775, .hw_value = 155};

	sdev->wiphy->bands[NL80211_BAND_5GHZ] = &dummy_band_5ghz;
	sdev->wiphy->bands[NL80211_BAND_6GHZ] = &dummy_band_6ghz;

	test_init_sta_bss(test, &chan);
	ndev_vif->chan = &chan;

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test,
			-1,
			slsi_roam_add_scan_frequencies_legacy(dev,
							      cbuf("ADDROAMSCANFREQUENCIES_LEGACY 3 5745 5785 5825"),
							      cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies_legacy(dev,
							      cbuf("ADDROAMSCANFREQUENCIES_LEGACY 3 4444 5785 5825"),
							      cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = SLSI_MAX_CHANNEL_LIST;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies_legacy(dev,
							      cbuf("ADDROAMSCANFREQUENCIES_LEGACY 3 5632 5785 5825"),
							      cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies_legacy(dev,
							      cbuf("ADDROAMSCANFREQUENCIES_LEGACY 3 2548 5785 5825"),
							      cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = SLSI_MAX_CHANNEL_LIST - 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies_legacy(dev,
							      cbuf("ADDROAMSCANFREQUENCIES_LEGACY - 7895 5785 5825"),
							      cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies_legacy(dev,
							      cbuf("ADDROAMSCANFREQUENCIES_LEGACY 0 5454 5785 5825"),
							      cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	ndev_vif->sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = SLSI_MAX_CHANNEL_LIST - 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies_legacy(dev,
							      cbuf("ADDROAMSCANFREQUENCIES_LEGACY 1 -"),
							      cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_roam_add_scan_frequencies_legacy(dev,
							      cbuf("ADDROAMSCANFREQUENCIES_LEGACY 1 7115"),
							      cbuf_size()));
}

static void test_ioctl_slsi_roam_add_scan_channels_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct ieee80211_channel chan = {.band = NL80211_BAND_5GHZ, .center_freq = 5775, .hw_value = 155};

	test_init_sta_bss(test, &chan);
	ndev_vif->chan = &chan;

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test,
			-1,
			slsi_roam_add_scan_channels_legacy(dev,
							   cbuf("ADDROAMSCANCHANNELS_LEGACY 2 11 149"),
							   cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_channels_legacy(dev,
							   cbuf("ADDROAMSCANCHANNELS_LEGACY 2 11 143"),
							   cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = SLSI_MAX_CHANNEL_LIST;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_channels_legacy(dev,
							   cbuf("ADDROAMSCANCHANNELS_LEGACY 2 11 157"),
							   cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = 0;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_roam_add_scan_channels_legacy(dev,
							   cbuf("ADDROAMSCANCHANNELS_LEGACY 2 13 149"),
							   cbuf_size()));
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_channels_legacy(dev,
							   cbuf("ADDROAMSCANCHANNELS_LEGACY - 12 149"),
							   cbuf_size()));
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_channels_legacy(dev,
							   cbuf("ADDROAMSCANCHANNELS_LEGACY 0 1 149"),
							   cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_channels_legacy(dev,
							   cbuf("ADDROAMSCANCHANNELS_LEGACY 2 - 7"),
							   cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_channels_legacy(dev,
							   cbuf("ADDROAMSCANCHANNELS_LEGACY 1 0"),
							   cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.ncho_mode = 0;
	sdev->device_config.legacy_roam_scan_list.n = SLSI_MAX_CHANNEL_LIST - 1;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_roam_add_scan_channels_legacy(dev,
							   cbuf("ADDROAMSCANCHANNELS_LEGACY 2 6 149"),
							   cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_frequencies_read_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_frequencies_read_legacy(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 31, slsi_roam_scan_frequencies_read_legacy(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_frequencies_read_legacy(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_frequencies_read_legacy(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_channels_read_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_channels_read_legacy(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_UNSPECIFIED;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 21, slsi_roam_scan_channels_read_legacy(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_channels_read_legacy(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_channels_read_legacy(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_set_wtc_mode(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 1 -80 -70 -70"), cbuf_size()));

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -1, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 1 -80 -70 -70 -70"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE - 1 -80 -70 -70 -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 - -80 -70 -70 -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 1 - -70 -70 -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 1 -80 - -70 -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 1 -80 -70 - -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 1 -80 -70 -70 -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 3 -80 -70 -70 -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 3 80 -70 -70 -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 3 -80 -70 -70 -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 3 -80 -70 -70 -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 256 3 -80 -70 -70 -70"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 1 80 -70 -70 -70"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_wtc_mode(dev, cbuf("SETWTCMODE 0 1 -80 -70 -70 -70"), cbuf_size()));
}

static void test_ioctl_slsi_reassoc_frequency_write_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->activated = false;

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_reassoc_frequency_write_legacy(dev,
							    cbuf("REASSOC_FREQUENCY_LEGACY f2:08:04:0e:0e:17 5220"),
							    cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_reassoc_frequency_write_legacy(dev,
							    cbuf("REASSOC_FREQUENCY_LEGACY f2:08:04:0e:0e: 5220"),
							    cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_reassoc_frequency_write_legacy(dev,
							    cbuf("REASSOC_FREQUENCY_LEGACY f2:08:04:0e:0e:17 -"),
							    cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_reassoc_frequency_write_legacy(dev,
							    cbuf("REASSOC_FREQUENCY_LEGACY f2:08:04:0e:0e:17 5230"),
							    cbuf_size()));
}

static void test_ioctl_slsi_reassoc_write_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	u8 bssid[6] = { 0 };

	ndev_vif->activated = false;

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_write_legacy(dev, cbuf("REASSOC_LEGACY 32:07:4d:69:0c:59 12"), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_write_legacy(dev, cbuf("REASSOC_LEGACY 32:07:4d:69:0c: 12"), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_write_legacy(dev, cbuf("REASSOC_LEGACY 32:07:4d:69:0c:59 -"), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_write_legacy(dev, cbuf("REASSOC_LEGACY 32:07:4d:69:0c:59 12"), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_write_legacy(dev, cbuf("REASSOC_LEGACY 32:07:4d:69:0c:59 0"), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_write_legacy(dev, cbuf("REASSOC_LEGACY 32:07:4d:69:0c:59 15"), cbuf_size()));

	memset(bssid, 0, 6);
}

static void test_ioctl_slsi_set_country_rev(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, 0, slsi_set_country_rev(dev, cbuf("SETCOUNTRYREV GB 6"), cbuf_size()));
}

static void test_ioctl_slsi_get_country_rev(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.domain_info.regdomain = kunit_kzalloc(test, sizeof(struct ieee80211_regdomain), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, sdev->device_config.domain_info.regdomain);

	KUNIT_EXPECT_EQ(test, 18, slsi_get_country_rev(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_ioctl_set_roam_band(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ioctl_set_roam_band(dev, cbuf("SETROAMBAND 5"), cbuf_size()));

	sdev->band_5g_supported = 1;
	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ioctl_set_roam_band(dev, cbuf("SETROAMBAND 0x0000"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.supported_roam_band = 0x0003;
	KUNIT_EXPECT_EQ(test, 0, slsi_ioctl_set_roam_band(dev, cbuf("SETROAMBAND 0x0003"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.supported_roam_band = 0x0001;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, -5, slsi_ioctl_set_roam_band(dev, cbuf("SETROAMBAND 0x0002"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.supported_roam_band = 0x0001;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_ioctl_set_roam_band(dev, cbuf("SETROAMBAND 0x0001"), cbuf_size()));
}

static void test_ioctl_slsi_ioctl_set_band(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ioctl_set_band(dev, cbuf("SETBAND 2"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ioctl_set_band(dev, cbuf("SETBAND 3"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.supported_band = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_ioctl_set_band(dev, cbuf("SETBAND 0"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.supported_band = 2;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_ioctl_set_band(dev, cbuf("SETBAND 1"), cbuf_size()));
}

static void test_ioctl_slsi_ioctl_get_roam_band(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.supported_roam_band = FAPI_BAND_AUTO;

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_NE(test, -EINVAL, slsi_ioctl_get_roam_band(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ioctl_get_roam_band(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_ioctl_get_band(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.supported_roam_band = FAPI_BAND_AUTO;
	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 9, slsi_ioctl_get_band(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ioctl_get_band(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_factory_freq_band_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_factory_freq_band_write(dev, cbuf("FACTORY_SETBAND 3"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_factory_freq_band_write(dev, cbuf("FACTORY_SETBAND 1"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_trigger_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_trigger_write(dev, cbuf("SETROAMTRIGGER -65"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_trigger_write(dev, cbuf("SETROAMTRIGGER -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_trigger_write(dev, cbuf("SETROAMTRIGGER -49"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_roam_scan_trigger_write(dev, cbuf("SETROAMTRIGGER -75"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_trigger_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_trigger_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 16, slsi_roam_scan_trigger_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_delta_trigger_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_delta_trigger_write(dev, cbuf("SETROAMDELTA 10"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_delta_trigger_write(dev, cbuf("SETROAMDELTA -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_delta_trigger_write(dev, cbuf("SETROAMDELTA 101"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_roam_delta_trigger_write(dev, cbuf("SETROAMDELTA 11"), cbuf_size()));
}

static void test_ioctl_slsi_roam_delta_trigger_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_delta_trigger_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 14, slsi_roam_delta_trigger_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_reassoc_frequency_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_frequency_write(dev, cbuf("REASSOC_LEGACY f2:08:04:0e:0e:17 5124"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_frequency_write(dev, cbuf("REASSOC_LEGACY f2:08:04:0e:0e:17"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_frequency_write(dev, cbuf("REASSOC_LEGACY f2:08:04:0e:0e: 5220"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_frequency_write(dev, cbuf("REASSOC_LEGACY f2:08:04:0e:0e:17 -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_reassoc_frequency_write(dev, cbuf("REASSOC_LEGACY f2:08:04:0e:0e:17 5112"), cbuf_size()));
}

static void test_ioctl_slsi_reassoc_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_reassoc_write(dev, cbuf("REASSOC 40:01:7a:d2:72:f5 11"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_reassoc_write(dev, cbuf("REASSOC 40:01:7a:d2:72:f5"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_reassoc_write(dev, cbuf("REASSOC 40:01:7a:d2:72: 11"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_reassoc_write(dev, cbuf("REASSOC 40:01:7a:d2:72:f5 -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_reassoc_write(dev, cbuf("REASSOC 40:01:7a:d2:72:f5 0"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_reassoc_write(dev, cbuf("REASSOC 40:01:7a:d2:72:f5 15"), cbuf_size()));
}

static void test_ioctl_slsi_cached_channel_scan_period_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_cached_channel_scan_period_write(dev, cbuf("SETROAMSCANPERIOD 20"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_cached_channel_scan_period_write(dev, cbuf("SETROAMSCANPERIOD -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_cached_channel_scan_period_write(dev, cbuf("SETROAMSCANPERIOD 70"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_cached_channel_scan_period_write(dev, cbuf("SETROAMSCANPERIOD 30"), cbuf_size()));
}

static void test_ioctl_slsi_cached_channel_scan_period_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_cached_channel_scan_period_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 19, slsi_cached_channel_scan_period_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_full_roam_scan_period_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_full_roam_scan_period_write(dev, cbuf("SETFULLROAMSCANPERIOD 60"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_full_roam_scan_period_write(dev, cbuf("SETFULLROAMSCANPERIOD -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_full_roam_scan_period_write(dev, cbuf("SETFULLROAMSCANPERIOD 601"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_full_roam_scan_period_write(dev, cbuf("SETFULLROAMSCANPERIOD 50"), cbuf_size()));
}

static void test_ioctl_slsi_full_roam_scan_period_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_full_roam_scan_period_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 23, slsi_full_roam_scan_period_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_max_active_channel_time_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_max_active_channel_time_write(dev, cbuf("SETSCANCHANNELTIME 60"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_max_active_channel_time_write(dev, cbuf("SETSCANCHANNELTIME -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_max_active_channel_time_write(dev, cbuf("SETSCANCHANNELTIME 301"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_roam_scan_max_active_channel_time_write(dev, cbuf("SETSCANCHANNELTIME 50"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_max_active_channel_time_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_max_active_channel_time_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 20, slsi_roam_scan_max_active_channel_time_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_probe_interval_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_probe_interval_write(dev, cbuf("SETSCANNPROBES 3"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_probe_interval_write(dev, cbuf("SETSCANNPROBES -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_probe_interval_write(dev, cbuf("SETSCANNPROBES 11"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_roam_scan_probe_interval_write(dev, cbuf("SETSCANNPROBES 5"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_probe_interval_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_probe_interval_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 16, slsi_roam_scan_probe_interval_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_mode_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_mode_write(dev, cbuf("SETROAMMODE 2"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_mode_write(dev, cbuf("SETROAMMODE -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_mode_write(dev, cbuf("SETROAMMODE 3"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_roam_mode_write(dev, cbuf("SETROAMMODE 1"), cbuf_size()));
}

static void test_ioctl_slsi_roam_mode_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_mode_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 13, slsi_roam_mode_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_offload_ap_list(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_offload_ap_list(dev, cbuf("SETROAMOFFLAPLIST -,44:ad:d9:e5:24:70"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_offload_ap_list(dev, cbuf("SETROAMOFFLAPLIST 0,44:ad:d9:e5:24:70"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_offload_ap_list(dev, cbuf("SETROAMOFFLAPLIST 2,44:ad:d9:e5:24:70"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_offload_ap_list(dev, cbuf("SETROAMOFFLAPLIST 1,44:ad:d9:e5:24:"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0,
			slsi_roam_offload_ap_list(dev, cbuf("SETROAMOFFLAPLIST 1,44:ad:d9:e5:24:70"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_band_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_band_write(dev, cbuf("SETROAMINTRABAND 2"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_band_write(dev, cbuf("SETROAMINTRABAND -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_band_write(dev, cbuf("SETROAMINTRABAND 3"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_roam_scan_band_write(dev, cbuf("SETROAMINTRABAND 1"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_band_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_band_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 18, slsi_roam_scan_band_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_control_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_control_write(dev, cbuf("SETROAMSCANCONTROL 2"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_control_write(dev, cbuf("SETROAMSCANCONTROL -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_roam_scan_control_write(dev, cbuf("SETROAMSCANCONTROL 0"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_control_write(dev, cbuf("SETROAMSCANCONTROL 3"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_control_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_control_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_roam_scan_control_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 20, slsi_roam_scan_control_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_home_time_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_home_time_write(dev, cbuf("SETSCANHOMETIME 100"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_home_time_write(dev, cbuf("SETSCANHOMETIME -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_home_time_write(dev, cbuf("SETSCANHOMETIME 400"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_roam_scan_home_time_write(dev, cbuf("SETSCANHOMETIME 101"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_home_time_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_home_time_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_roam_scan_home_time_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 17, slsi_roam_scan_home_time_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_home_away_time_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_home_away_time_write(dev, cbuf("SETSCANHOMEAWAYTIME 100"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_home_away_time_write(dev, cbuf("SETSCANHOMEAWAYTIME -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_home_away_time_write(dev, cbuf("SETSCANHOMEAWAYTIME 400"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_roam_scan_home_away_time_write(dev, cbuf("SETSCANHOMEAWAYTIME 101"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_home_away_time_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_home_away_time_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_roam_scan_home_away_time_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 21, slsi_roam_scan_home_away_time_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_set_home_away_time_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_home_away_time_legacy(dev, cbuf("SETSCANHOMEAWAYTIME_LEGACY -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_home_away_time_legacy(dev, cbuf("SETSCANHOMEAWAYTIME_LEGACY -1"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0,
			slsi_set_home_away_time_legacy(dev, cbuf("SETSCANHOMEAWAYTIME_LEGACY 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0,
			slsi_set_home_away_time_legacy(dev, cbuf("SETSCANHOMEAWAYTIME_LEGACY 30"), cbuf_size()));
}

static void test_ioctl_slsi_set_home_time_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_home_time_legacy(dev, cbuf("SETSCANHOMETIME_LEGACY -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_home_time_legacy(dev, cbuf("SETSCANHOMETIME_LEGACY -1"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0,
			slsi_set_home_time_legacy(dev, cbuf("SETSCANHOMETIME_LEGACY 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0,
			slsi_set_home_time_legacy(dev, cbuf("SETSCANHOMETIME_LEGACY 30"), cbuf_size()));
}

static void test_ioctl_slsi_set_channel_time_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_channel_time_legacy(dev, cbuf("SETSCANCHANNELTIME_LEGACY -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_channel_time_legacy(dev, cbuf("SETSCANCHANNELTIME_LEGACY -1"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_channel_time_legacy(dev, cbuf("SETSCANCHANNELTIME_LEGACY 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_channel_time_legacy(dev, cbuf("SETSCANCHANNELTIME_LEGACY 30"), cbuf_size()));
}

static void test_ioctl_slsi_set_passive_time_legacy(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_passive_time_legacy(dev, cbuf("SETSCANPASSIVETIME_LEGACY -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_passive_time_legacy(dev, cbuf("SETSCANPASSIVETIME_LEGACY -1"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_passive_time_legacy(dev, cbuf("SETSCANPASSIVETIME_LEGACY 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_passive_time_legacy(dev, cbuf("SETSCANPASSIVETIME_LEGACY 30"), cbuf_size()));
}

static void test_ioctl_slsi_set_tid(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_tid(dev, cbuf("SET_TID 4 0 5"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_tid(dev, cbuf("SET_TID - 0 5"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tid(dev, cbuf("SET_TID 0 0 5"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_tid(dev, cbuf("SET_TID 1 - 5"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_tid(dev, cbuf("SET_TID 1 0 -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tid(dev, cbuf("SET_TID 1 0 5"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_frequencies_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct ieee80211_channel chan = {.band = NL80211_BAND_5GHZ, .center_freq = 5775, .hw_value = 155};

	sdev->wiphy->bands[NL80211_BAND_5GHZ] = &dummy_band_5ghz;

	test_init_sta_bss(test, &chan);
	ndev_vif->chan = &chan;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_scan_frequencies_write(dev,
							 cbuf("SETROAMSCANFREQUENCIES 3 2412 2437 5180"),
							 cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_scan_frequencies_write(dev,
							 cbuf("SETROAMSCANFREQUENCIES - 2412 2437 5180"),
							 cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_scan_frequencies_write(dev,
							 cbuf("SETROAMSCANFREQUENCIES -1 2412 2437 5180"),
							 cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_scan_frequencies_write(dev,
							 cbuf("SETROAMSCANFREQUENCIES 30 2412 2437 5180"),
							 cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_scan_frequencies_write(dev,
							 cbuf("SETROAMSCANFREQUENCIES 3 - 2437 5180"),
							 cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_scan_frequencies_write(dev,
							 cbuf("SETROAMSCANFREQUENCIES 1 5180"),
							 cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 1;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_roam_scan_frequencies_write(dev,
							 cbuf("SETROAMSCANFREQUENCIES 1 5825"),
							 cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_channels_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct ieee80211_channel chan = {.band = NL80211_BAND_5GHZ, .center_freq = 5775, .hw_value = 155};

	test_init_sta_bss(test, &chan);
	ndev_vif->chan = &chan;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_channels_write(dev, cbuf("SETROAMSCANCHANNELS 3 1 6 36"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_channels_write(dev, cbuf("SETROAMSCANCHANNELS - 1 6 36"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_channels_write(dev, cbuf("SETROAMSCANCHANNELS -1 1 6 36"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_channels_write(dev, cbuf("SETROAMSCANCHANNELS 22 1 6 36"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_channels_write(dev, cbuf("SETROAMSCANCHANNELS 3 - 6 36"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_scan_channels_write(dev, cbuf("SETROAMSCANCHANNELS 3 0 7 36"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_roam_scan_channels_write(dev, cbuf("SETROAMSCANCHANNELS 3 2 6 36"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 1;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_roam_scan_channels_write(dev, cbuf("SETROAMSCANCHANNELS 3 3 6 36"), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_frequencies_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_frequencies_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, 24, slsi_roam_scan_frequencies_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_frequencies_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_frequencies_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_scan_channels_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_channels_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_channels_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_channels_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_scan_channels_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_roam_add_scan_frequencies(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct ieee80211_channel chan = {.band = NL80211_BAND_5GHZ, .center_freq = 5775, .hw_value = 155};

	sdev->wiphy->bands[NL80211_BAND_2GHZ] = &dummy_band_2ghz;
	sdev->wiphy->bands[NL80211_BAND_5GHZ] = &dummy_band_5ghz;

	test_init_sta_bss(test, &chan);
	ndev_vif->chan = &chan;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies(dev,
						       cbuf("ADDROAMSCANFREQUENCIES 3 2412 5180 5500"),
						       cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 1;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies(dev,
						       cbuf("ADDROAMSCANFREQUENCIES 3 2412 5200 5500"),
						       cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	sdev->device_config.wes_roam_scan_list.n = 20;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies(dev,
						       cbuf("ADDROAMSCANFREQUENCIES 3 2412 5300 5500"),
						       cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.wes_roam_scan_list.n = 0;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies(dev,
						       cbuf("ADDROAMSCANFREQUENCIES 3 2121 5180 5500"),
						       cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	sdev->device_config.wes_roam_scan_list.n = 2;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies(dev,
						       cbuf("ADDROAMSCANFREQUENCIES - 2412 5180 5500"),
						       cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	sdev->device_config.wes_roam_scan_list.n = 2;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies(dev,
						       cbuf("ADDROAMSCANFREQUENCIES -1 2412 5180 5500"),
						       cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	sdev->device_config.wes_roam_scan_list.n = 2;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roam_add_scan_frequencies(dev,
						       cbuf("ADDROAMSCANFREQUENCIES 3 - 5180 5500"),
						       cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	sdev->device_config.wes_roam_scan_list.n = 2;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_roam_add_scan_frequencies(dev,
						       cbuf("ADDROAMSCANFREQUENCIES 1 2412"),
						       cbuf_size()));
}

static void test_ioctl_slsi_roam_add_scan_channels(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct ieee80211_channel chan = {.band = NL80211_BAND_5GHZ, .center_freq = 5775, .hw_value = 155};

	test_init_sta_bss(test, &chan);
	ndev_vif->chan = &chan;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS 1 144"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS 1 157"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	sdev->device_config.wes_roam_scan_list.n = SLSI_NCHO_MAX_CHANNEL_LIST;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS 1 57"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.wes_roam_scan_list.n = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS 1 62"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	sdev->device_config.wes_roam_scan_list.n = SLSI_NCHO_MAX_CHANNEL_LIST - 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS - 144"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	sdev->device_config.wes_roam_scan_list.n = SLSI_NCHO_MAX_CHANNEL_LIST - 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS 22 144"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	sdev->device_config.wes_roam_scan_list.n = SLSI_NCHO_MAX_CHANNEL_LIST - 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS 2 144"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	sdev->device_config.wes_roam_scan_list.n = SLSI_NCHO_MAX_CHANNEL_LIST - 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS 1 -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	sdev->device_config.wes_roam_scan_list.n = SLSI_NCHO_MAX_CHANNEL_LIST - 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS 1 0"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->device_config.roam_scan_mode = 0;
	sdev->device_config.wes_roam_scan_list.n = SLSI_NCHO_MAX_CHANNEL_LIST - 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_roam_add_scan_channels(dev, cbuf("ADDROAMSCANCHANNELS 1 44"), cbuf_size()));
}

static void test_ioctl_slsi_okc_mode_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_okc_mode_write(dev, cbuf("SETOKCMODE 2"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_okc_mode_write(dev, cbuf("SETOKCMODE -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_okc_mode_write(dev, cbuf("SETOKCMODE 1"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_okc_mode_write(dev, cbuf("SETOKCMODE 4"), cbuf_size()));
}

static void test_ioctl_slsi_okc_mode_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_okc_mode_read(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 12, slsi_okc_mode_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_wes_mode_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_wes_mode_write(dev, cbuf("SETWESMODE 2"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_wes_mode_write(dev, cbuf("SETWESMODE -"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_wes_mode_write(dev, cbuf("SETWESMODE 1"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_wes_mode_write(dev, cbuf("SETWESMODE 4"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->device_config.wes_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_wes_mode_write(dev, cbuf("SETWESMODE 0"), cbuf_size()));
}

static void test_ioctl_slsi_wes_mode_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_wes_mode_read(dev, cbuf("GETWESMODE"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 12, slsi_wes_mode_read(dev, cbuf("GETWESMODE"), cbuf_size()));
}

static void test_ioctl_slsi_set_ncho_mode(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_ncho_mode(dev, cbuf("SETNCHOMODE -"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_ncho_mode(dev, cbuf("SETNCHOMODE 2"), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_ncho_mode(dev, cbuf("SETNCHOMODE 0"), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, -5, slsi_set_ncho_mode(dev, cbuf("SETNCHOMODE 1"), cbuf_size()));
}

static void test_ioctl_slsi_get_ncho_mode(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, 13, slsi_get_ncho_mode(dev, cbuf("GETNCHOMODE"), cbuf_size()));
}

static void test_ioctl_slsi_set_dfs_scan_mode(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dfs_scan_mode(dev, cbuf("SETDFSSCANMODE -"), cbuf_size()));

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dfs_scan_mode(dev, cbuf("SETDFSSCANMODE 5"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_dfs_scan_mode(dev, cbuf("SETDFSSCANMODE 2"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dfs_scan_mode(dev, cbuf("SETDFSSCANMODE 6"), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_dfs_scan_mode(dev, cbuf("SETDFSSCANMODE 1"), cbuf_size()));
}

static void test_ioctl_slsi_get_dfs_scan_mode(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.ncho_mode = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_dfs_scan_mode(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 1, slsi_get_dfs_scan_mode(dev, cbuf(""), cbuf_size()));

	sdev->device_config.ncho_mode = 1;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 16, slsi_get_dfs_scan_mode(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_set_pmk(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_pmk(dev, cbuf("SET_PMK a"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_pmk(dev, cbuf("SET_PMK aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), cbuf_size()));
}

static void test_ioctl_slsi_auto_chan_read(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, 2, slsi_auto_chan_read(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_send_action_frame(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	u8 *bssid = "1c:aa:07:7b:4e:10";

	dev->dev_addr = "1c:aa:07:7b:4e:10";
	SLSI_ETHER_COPY(ndev_vif->sta.bssid, bssid);

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 1 20 520 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 1 20 522 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 1 20 520"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61: 1 20 520 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3b 1 20 520 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a - 20 520 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 0 20 520 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 15 20 520 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 1 - 520 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 1 20 - 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 1 20 1025 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 1 20 500 7f0000f0"),
					       cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_send_action_frame(dev,
					       cbuf("SENDACTIONFRAME 31:63:3a:61:61:3a 1 20 521 7f0000f0"),
					       cbuf_size()));
}

static void test_ioctl_slsi_send_action_frame_cert(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_send_action_frame_cert(dev, cbuf("CERTSENDACTIONFRAME 0 1"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_send_action_frame_cert(dev, cbuf("CERTSENDACTIONFRAME 0"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_send_action_frame_cert(dev, cbuf("CERTSENDACTIONFRAME - 1"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 0, slsi_send_action_frame_cert(dev, cbuf("CERTSENDACTIONFRAME 1 1"), cbuf_size()));
}

static void test_ioctl_slsi_setting_max_sta_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_setting_max_sta_write(dev, cbuf("HAPD_MAX_NUM_STA -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_setting_max_sta_write(dev, cbuf("HAPD_MAX_NUM_STA 11"), cbuf_size()));

	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_setting_max_sta_write(dev, cbuf("HAPD_MAX_NUM_STA 2"), cbuf_size()));

	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_setting_max_sta_write(dev, cbuf("HAPD_MAX_NUM_STA 3"), cbuf_size()));
}

static void test_ioctl_slsi_country_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_country_write(dev, cbuf("COUNTRY P"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_country_write(dev, cbuf("COUNTRY KR"), cbuf_size()));
}

static void test_ioctl_slsi_update_rssi_boost(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_update_rssi_boost(dev, cbuf("SETJOINPREFER 0102000"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_update_rssi_boost(dev, cbuf("SETJOINPREFER 010200000302000004121401"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0,
			slsi_update_rssi_boost(dev, cbuf("SETJOINPREFER 010200000302000004021400"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0,
			slsi_update_rssi_boost(dev, cbuf("SETJOINPREFER 010200000302000004021401"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0,
			slsi_update_rssi_boost(dev, cbuf("SETJOINPREFER 010200000302000004021402"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_update_rssi_boost(dev, cbuf("SETJOINPREFER 010200000302000004021403"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_update_rssi_boost(dev, cbuf("SETJOINPREFER 110200000302000004021401"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_update_rssi_boost(dev, cbuf("SETJOINPREFER 1102000003"), cbuf_size()));
}

static void test_ioctl_slsi_set_tx_power_calling(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING -1"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 1"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 2"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 3"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 4"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 5"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 6"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 7"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 8"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 9"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_tx_power_calling(dev, cbuf("SET_TX_POWER_CALLING 10"), cbuf_size()));
}

static void test_ioctl_slsi_set_tx_power_sub6_band(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_config.host_state_sub6_band = 0;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_tx_power_sub6_band(dev, cbuf("SET_TX_POWER_SUB6_BAND 6"), cbuf_size()));

	sdev->device_config.host_state_sub6_band = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_tx_power_sub6_band(dev, cbuf("SET_TX_POWER_SUB6_BAND -"), cbuf_size()));

	sdev->device_config.host_state_sub6_band = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_sub6_band(dev, cbuf("SET_TX_POWER_SUB6_BAND 7"), cbuf_size()));

	sdev->device_config.host_state_sub6_band = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_sub6_band(dev, cbuf("SET_TX_POWER_SUB6_BAND 38"), cbuf_size()));

	sdev->device_config.host_state_sub6_band = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_sub6_band(dev, cbuf("SET_TX_POWER_SUB6_BAND 40"), cbuf_size()));

	sdev->device_config.host_state_sub6_band = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_sub6_band(dev, cbuf("SET_TX_POWER_SUB6_BAND 41"), cbuf_size()));

	sdev->device_config.host_state_sub6_band = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_sub6_band(dev, cbuf("SET_TX_POWER_SUB6_BAND 77"), cbuf_size()));

	sdev->device_config.host_state_sub6_band = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_tx_power_sub6_band(dev, cbuf("SET_TX_POWER_SUB6_BAND 78"), cbuf_size()));

	sdev->device_config.host_state_sub6_band = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_tx_power_sub6_band(dev, cbuf("SET_TX_POWER_SUB6_BAND 80"), cbuf_size()));
}

static struct ieee80211_regdomain dummy_regdomain = {
	.reg_rules = {
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
		REG_RULE(0, 0, 0, 0, 0, 0),
	}
};

static void test_ioctl_slsi_get_regulatory(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_regulatory(dev, cbuf("GETREGULATORY -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_regulatory(dev, cbuf("GETREGULATORY 3"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.sta_bss = kunit_kzalloc(test, sizeof(struct cfg80211_bss), GFP_KERNEL);
	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ndev_vif->sta.sta_bss);
	sdev->device_config.domain_info.regdomain = &dummy_regdomain;

	KUNIT_EXPECT_EQ(test, 33, slsi_get_regulatory(dev, cbuf("GETREGULATORY 1"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 33, slsi_get_regulatory(dev, cbuf("GETREGULATORY 0"), cbuf_size()));
}

static void test_ioctl_slsi_set_fcc_channel(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_fcc_channel(dev, cbuf("SET_FCC_CHANNEL -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_fcc_channel(dev, cbuf("SET_FCC_CHANNEL 2"), cbuf_size()));

	sdev->nan_enabled = 0;
	ndev_vif->sdev->device_config.disable_ch12_ch13 = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_fcc_channel(dev, cbuf("SET_FCC_CHANNEL 0"), cbuf_size()));

	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_fcc_channel(dev, cbuf("SET_FCC_CHANNEL -1"), cbuf_size()));
}

static void test_ioctl_slsi_fake_mac_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_fake_mac_write(dev, cbuf("FAKEMAC ON"), cbuf_size()));
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_fake_mac_write(dev, cbuf("FAKEMAC OFF"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_fake_mac_write(dev, cbuf("FAKEMAC OH"), cbuf_size()));
}

static void test_ioctl_slsi_get_sta_info(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	int i = 0;

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_sta_info(dev, cbuf("GETSTAINFO"), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_AP;
	for (; i < SLSI_PEER_INDEX_MAX; i++) {
		ndev_vif->peer_sta_record[i] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
		KUNIT_ASSERT_NOT_ERR_OR_NULL(test, ndev_vif->peer_sta_record[i]);
	}

	KUNIT_EXPECT_LE(test, 0, slsi_get_sta_info(dev, cbuf("GETSTAINFO"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_get_sta_info(dev, cbuf("GETSTAINFO ALL"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_get_sta_info(dev, cbuf("GETSTAINFO 12:34:56:78:90:ab"), cbuf_size()));
}

static void test_ioctl_slsi_get_bss_rssi(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, 1, slsi_get_bss_rssi(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_get_bss_info(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, 36, slsi_get_bss_info(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_get_cu(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, 3, slsi_get_cu(dev, cbuf(""), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 2, slsi_get_cu(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_get_assoc_reject_info(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->sdev->assoc_result_code = 0x0000;
	KUNIT_EXPECT_EQ(test, 31, slsi_get_assoc_reject_info(dev, cbuf(""), cbuf_size()));

	ndev_vif->sdev->assoc_result_code = 0x004f;
	KUNIT_EXPECT_EQ(test, 45, slsi_get_assoc_reject_info(dev, cbuf(""), cbuf_size()));

	ndev_vif->sdev->assoc_result_code = 0x800b;
	KUNIT_EXPECT_EQ(test, 48, slsi_get_assoc_reject_info(dev, cbuf(""), cbuf_size()));

	ndev_vif->sdev->assoc_result_code = 0x800c;
	KUNIT_EXPECT_EQ(test, 47, slsi_get_assoc_reject_info(dev, cbuf(""), cbuf_size()));

	ndev_vif->sdev->assoc_result_code = 0x800f;
	KUNIT_EXPECT_EQ(test, 41, slsi_get_assoc_reject_info(dev, cbuf(""), cbuf_size()));

	ndev_vif->sdev->assoc_result_code = 0x8010;
	KUNIT_EXPECT_EQ(test, 40, slsi_get_assoc_reject_info(dev, cbuf(""), cbuf_size()));

	ndev_vif->sdev->assoc_result_code = 0x8011;
	KUNIT_EXPECT_EQ(test, 41, slsi_get_assoc_reject_info(dev, cbuf(""), cbuf_size()));

	ndev_vif->sdev->assoc_result_code = 0x8012;
	KUNIT_EXPECT_EQ(test, 39, slsi_get_assoc_reject_info(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_set_bandwidth(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_bandwidth(dev, cbuf("SETBAND -"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_bandwidth(dev, cbuf("SETBAND 25"), cbuf_size()));

	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_bandwidth(dev, cbuf("SETBAND 20"), cbuf_size()));

	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_bandwidth(dev, cbuf("SETBAND 40"), cbuf_size()));
}

static void test_ioctl_slsi_set_bss_bandwidth(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_bss_bandwidth(dev, cbuf("SET_BSS_CHANNEL_WIDTH 11 1"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_bss_bandwidth(dev, cbuf("SET_BSS_CHANNEL_WIDTH - 2"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_bss_bandwidth(dev, cbuf("SET_BSS_CHANNEL_WIDTH 20 -"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_bss_bandwidth(dev, cbuf("SET_BSS_CHANNEL_WIDTH 20 2"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	KUNIT_EXPECT_NE(test, -EINVAL, slsi_set_bss_bandwidth(dev, cbuf("SET_BSS_CHANNEL_WIDTH 20 1"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_NE(test, -EINVAL, slsi_set_bss_bandwidth(dev, cbuf("SET_BSS_CHANNEL_WIDTH 40 1"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_NE(test, -EINVAL, slsi_set_bss_bandwidth(dev, cbuf("SET_BSS_CHANNEL_WIDTH 80 1"), cbuf_size()));
}

static void test_ioctl_slsi_set_disconnect_ies(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_disconnect_ies(dev, cbuf("SET_DISCONNECT_IES dd090000F0220301"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0,
			slsi_set_disconnect_ies(dev, cbuf("SET_DISCONNECT_IES dd090000F0220301020100"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_set_disconnect_ies(dev, cbuf("SET_DISCONNECT_IES d1230000F0120301021111"), cbuf_size()));
}

static void test_ioctl_slsi_start_power_measurement_detection(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->sdev->detect_vif_active = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL,
			slsi_start_power_measurement_detection(dev, cbuf("POWER_MEASUREMENT_START"), cbuf_size()));

	sdev->nan_enabled = 1;
	ndev_vif->sdev->detect_vif_active = 0;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_start_power_measurement_detection(dev, cbuf("POWER_MEASUREMENT_START"), cbuf_size()));

	sdev->nan_enabled = 0;
	ndev_vif->sdev->detect_vif_active = 0;
	KUNIT_EXPECT_EQ(test, 0,
			slsi_start_power_measurement_detection(dev, cbuf("POWER_MEASUREMENT_START"), cbuf_size()));
}

static void test_ioctl_slsi_ioctl_auto_chan_write(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_ioctl_auto_chan_write(dev,
						   cbuf("SET_SAP_CHANNEL_LIST - 1 6 11"),
						   cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_ioctl_auto_chan_write(dev,
						   cbuf("SET_SAP_CHANNEL_LIST 15 1 6 11"),
						   cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_ioctl_auto_chan_write(dev,
						   cbuf("SET_SAP_CHANNEL_LIST 3 1 6 11"),
						   cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_ioctl_auto_chan_write(dev,
						   cbuf("SET_SAP_CHANNEL_LIST 2 1 6 11"),
						   cbuf_size()));
}

static void test_ioctl_slsi_ioctl_test_force_hang(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, 0, slsi_ioctl_test_force_hang(dev, cbuf("SLSI_TEST_FORCE_HANG"), cbuf_size()));
}

static void test_ioctl_slsi_ioctl_set_latency_crt_data(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_ioctl_set_latency_crt_data(dev,
							cbuf("SET_LATENCY_CRT_DATA -"),
							cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_ioctl_set_latency_crt_data(dev,
							cbuf("SET_LATENCY_CRT_DATA 4"),
							cbuf_size()));
	KUNIT_EXPECT_NE(test,
			-EINVAL,
			slsi_ioctl_set_latency_crt_data(dev,
							cbuf("SET_LATENCY_CRT_DATA 1"),
							cbuf_size()));
}

static void test_ioctl_slsi_ioctl_driver_bug_dump(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, 0, slsi_ioctl_driver_bug_dump(dev, cbuf("CMD_DRIVERDEBUGDUMP"), cbuf_size()));
}

static void test_ioctl_slsi_elna_bypass(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_elna_bypass(dev, cbuf("ELNA_BYPASS -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_elna_bypass(dev, cbuf("ELNA_BYPASS 2"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	KUNIT_EXPECT_NE(test, -EINVAL, slsi_elna_bypass(dev, cbuf("ELNA_BYPASS 0"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_NE(test, -EINVAL, slsi_elna_bypass(dev, cbuf("ELNA_BYPASS 1"), cbuf_size()));
}

static void test_ioctl_slsi_elna_bypass_int(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_elna_bypass_int(dev, cbuf("ELNA_BYPASS_INT -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_elna_bypass_int(dev, cbuf("ELNA_BYPASS_INT 65536"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_AP;
	KUNIT_EXPECT_EQ(test, 0, slsi_elna_bypass_int(dev, cbuf("ELNA_BYPASS_INT 100"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, -1, slsi_elna_bypass_int(dev, cbuf("ELNA_BYPASS_INT 200"), cbuf_size()));
}

static void test_ioctl_slsi_set_dwell_time(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME - 100 40 45"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME -1 100 40 45"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME 60 - 40 45"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME 60 -1 40 45"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME 60 100 - 45"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME 60 100 -1 45"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME 60 100 40 -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME 60 100 40 -1"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME 60 100 - 45"), cbuf_size()));
	KUNIT_EXPECT_NE(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME 0 100 40 45"), cbuf_size()));
	KUNIT_EXPECT_NE(test, -EINVAL, slsi_set_dwell_time(dev, cbuf("SET_DWELL_TIME 60 100 40 45"), cbuf_size()));
}

static void test_ioctl_slsi_max_dtim_suspend(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_max_dtim_suspend(dev, cbuf("MAX_DTIM_IN_SUSPEND -"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, -1, slsi_max_dtim_suspend(dev, cbuf("MAX_DTIM_IN_SUSPEND 3"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 0, slsi_max_dtim_suspend(dev, cbuf("MAX_DTIM_IN_SUSPEND 1"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_max_dtim_suspend(dev, cbuf("MAX_DTIM_IN_SUSPEND 2"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 0, slsi_max_dtim_suspend(dev, cbuf("MAX_DTIM_IN_SUSPEND 0"), cbuf_size()));
}

static void test_ioctl_slsi_set_dtim_suspend(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dtim_suspend(dev, cbuf("SET_DTIM_IN_SUSPEND -"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, -1, slsi_set_dtim_suspend(dev, cbuf("SET_DTIM_IN_SUSPEND 3"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->max_dtim_recv = 0;
	KUNIT_EXPECT_EQ(test, -1, slsi_set_dtim_suspend(dev, cbuf("SET_DTIM_IN_SUSPEND 1"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->max_dtim_recv = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_set_dtim_suspend(dev, cbuf("SET_DTIM_IN_SUSPEND 2"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	sdev->max_dtim_recv = 1;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_set_dtim_suspend(dev, cbuf("SET_DTIM_IN_SUSPEND -1"), cbuf_size()));
}

static void test_ioctl_slsi_force_roaming_bssid(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	sdev->device_state = SLSI_DEVICE_STATE_STARTED;
	ndev_vif->acl_data_supplicant = false;
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_force_roaming_bssid(dev,
						 cbuf("FORCE_ROAMING_BSSID 48:0e:ec:f1:1b: 1"),
						 cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_force_roaming_bssid(dev,
						 cbuf("FORCE_ROAMING_BSSID 48:0e:ec:f1:1b:b9 -"),
						 cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_force_roaming_bssid(dev,
						 cbuf("FORCE_ROAMING_BSSID 48:0e:ec:f1:1b:b9 166"),
						 cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_force_roaming_bssid(dev,
						 cbuf("FORCE_ROAMING_BSSID 48:0e:ec:f1:1b:b9 7"),
						 cbuf_size()));
}

static void test_ioctl_slsi_roaming_blacklist_add(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roaming_blacklist_add(dev,
						   cbuf("ROAMING_BLACKLIST_ADD 00:12:fb:00:00:"),
						   cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_roaming_blacklist_add(dev,
						   cbuf("ROAMING_BLACKLIST_ADD 00:12:fb:00:00:0e"),
						   cbuf_size()));
}

static void test_ioctl_slsi_roaming_blacklist_remove(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test,
			-EINVAL,
			slsi_roaming_blacklist_remove(dev,
						      cbuf("ROAMING_BLACKLIST_REMOVE 00:12:fb:00:00:"),
						      cbuf_size()));
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_roaming_blacklist_remove(dev,
						      cbuf("ROAMING_BLACKLIST_REMOVE 00:12:fb:00:00:0d"),
						      cbuf_size()));
}

static void test_ioctl_slsi_ioctl_get_ps_disabled_duration(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ioctl_get_ps_disabled_duration(dev, cbuf(""), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 2, slsi_ioctl_get_ps_disabled_duration(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_ioctl_get_ps_entry_counter(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->activated = false;
	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ioctl_get_ps_entry_counter(dev, cbuf(""), cbuf_size()));

	ndev_vif->activated = true;
	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 2, slsi_ioctl_get_ps_entry_counter(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_ioctl_cmd_success(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);

	KUNIT_EXPECT_EQ(test, 0, slsi_ioctl_cmd_success(dev, cbuf("GET_PS_ENTRY_COUNTER"), cbuf_size()));
}

static void test_ioctl_slsi_twt_setup(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, 4, slsi_twt_setup(dev, cbuf("TWT_SETUP 0 0 0 0 0 0 0 0 0 0 0 0 0"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 0 0 0 0 0 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP -1 0 0 0 0 0 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP - 0 0 0 0 0 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 1 - 0 0 0 0 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 2 0 - 0 0 0 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 3 0 0 - 0 0 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 4 0 0 0 - 0 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 5 0 0 0 5000 - 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 6 0 0 0 5000 9000 - 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 7 0 0 0 5000 9000 0 - 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 8 0 0 0 5000 9000 0 0 - 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 9 0 0 0 5000 9000 0 0 0 - 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 10 0 0 0 5000 9000 0 0 0 0 - 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 20 0 0 0 5000 9000 0 0 0 0 0 - 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 30 0 0 0 5000 9000 0 0 0 0 0 0 -"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 30 2 0 0 5000 9000 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 30 0 0 0 3000 9000 0 0 0 0 0 0 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_setup(dev, cbuf("TWT_SETUP 30 0 0 0 5000 7000 0 0 0 0 0 0 0"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 0;
	KUNIT_EXPECT_EQ(test,
			3,
			slsi_twt_setup(dev,
				       cbuf("TWT_SETUP 3 1 1 1 5000 9000 1 1 9001 1 5001 1 1"),
				       cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	KUNIT_EXPECT_EQ(test,
			0,
			slsi_twt_setup(dev,
				       cbuf("TWT_SETUP 3 1 1 1 7000 10000 1 1 10000 1 7000 1 1"),
				       cbuf_size()));
}

static void test_ioctl_slsi_twt_teardown(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_teardown(dev, cbuf("TWT_TEARDOWN 0"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_teardown(dev, cbuf("TWT_TEARDOWN - 0"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_teardown(dev, cbuf("TWT_TEARDOWN -1 0"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_teardown(dev, cbuf("TWT_TEARDOWN 1 -"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_teardown(dev, cbuf("TWT_TEARDOWN 1 2"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 0;
	KUNIT_EXPECT_EQ(test, 3, slsi_twt_teardown(dev, cbuf("TWT_TEARDOWN 1 1"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_teardown(dev, cbuf("TWT_TEARDOWN 2 1"), cbuf_size()));
}

static void test_ioctl_slsi_twt_suspend(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTING;
	KUNIT_EXPECT_EQ(test, 3, slsi_twt_suspend(dev, cbuf("TWT_INFO_FRM 0 0"), cbuf_size()));

	ndev_vif->iftype = NL80211_IFTYPE_STATION;
	ndev_vif->sta.vif_status = SLSI_VIF_STATUS_CONNECTED;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_suspend(dev, cbuf("TWT_INFO_FRM 0"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_suspend(dev, cbuf("TWT_INFO_FRM - 0"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_suspend(dev, cbuf("TWT_INFO_FRM 0 -"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 0;
	KUNIT_EXPECT_EQ(test, 3, slsi_twt_suspend(dev, cbuf("TWT_INFO_FRM 0 1"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_suspend(dev, cbuf("TWT_INFO_FRM 0 2"), cbuf_size()));
}

static void test_ioctl_slsi_twt_get_status(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_get_status(dev, cbuf("GET_TWT_STATUS "), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_get_status(dev, cbuf("GET_TWT_STATUS -"), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 3, slsi_twt_get_status(dev, cbuf("GET_TWT_STATUS 1"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 0;
	KUNIT_EXPECT_EQ(test, 3, slsi_twt_get_status(dev, cbuf("GET_TWT_STATUS 2"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_get_status(dev, cbuf("GET_TWT_STATUS 3"), cbuf_size()));
}

static void test_ioctl_slsi_twt_get_capabilities(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_mib_value *values;
	struct slsi_vif_sta *sta = &ndev_vif->sta;

	ndev_vif->sta.twt_peer_cap = 0;
	KUNIT_EXPECT_EQ(test, 11, slsi_twt_get_capabilities(dev, cbuf(""), cbuf_size()));

	ndev_vif->sta.twt_peer_cap = 2;
	KUNIT_EXPECT_EQ(test, 5, slsi_twt_get_capabilities(dev, cbuf(""), cbuf_size()));
}

static void test_ioctl_slsi_twt_get_statistics_query(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_mib_value *values;
	struct slsi_vif_sta *sta = &ndev_vif->sta;

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_twt_get_statistics_query(dev, cbuf("GET_TWT_STATISTICS"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_get_statistics_query(dev, cbuf("GET_TWT_STATISTICS -"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 0;
	KUNIT_EXPECT_EQ(test, 3, slsi_twt_get_statistics_query(dev, cbuf("GET_TWT_STATISTICS 1"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_get_statistics_query(dev, cbuf("GET_TWT_STATISTICS 2"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	sdev->recovery_timeout = 2;
	KUNIT_EXPECT_EQ(test, 10, slsi_twt_get_statistics_query(dev, cbuf("GET_TWT_STATISTICS 3"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	sdev->recovery_timeout = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_get_statistics_query(dev, cbuf("GET_TWT_STATISTICS 4"), cbuf_size()));
}

static void test_ioctl_slsi_twt_clear_statistics(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_mib_value *values;
	struct slsi_vif_sta *sta = &ndev_vif->sta;

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_clear_statistics(dev, cbuf("CLEAR_TWT_STATISTICS "), cbuf_size()));
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_clear_statistics(dev, cbuf("CLEAR_TWT_STATISTICS -"), cbuf_size()));

	KUNIT_EXPECT_EQ(test, 0, slsi_twt_clear_statistics(dev, cbuf("CLEAR_TWT_STATISTICS 9"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 0;
	KUNIT_EXPECT_EQ(test, 3, slsi_twt_clear_statistics(dev, cbuf("CLEAR_TWT_STATISTICS 1"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_clear_statistics(dev, cbuf("CLEAR_TWT_STATISTICS 2"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	sdev->nan_enabled = 0;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_clear_statistics(dev, cbuf("CLEAR_TWT_STATISTICS 3"), cbuf_size()));

	ndev_vif->sta.twt_allowed = 1;
	sdev->nan_enabled = 1;
	KUNIT_EXPECT_EQ(test, 0, slsi_twt_clear_statistics(dev, cbuf("CLEAR_TWT_STATISTICS 4"), cbuf_size()));
}

static void test_ioctl_slsi_ioctl(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct ifreq *rq = kunit_kzalloc(test, sizeof(struct ifreq), GFP_KERNEL);

	KUNIT_ASSERT_NOT_ERR_OR_NULL(test, rq);

	rq->ifr_data = "GETBAND";

	KUNIT_EXPECT_EQ(test, -EINVAL, slsi_ioctl(dev, rq, 0));
}

static int ioctl_test_init(struct kunit *test)
{
	memset(test_ioctl_cmd_buf, 0x0, sizeof(test_ioctl_cmd_buf));

	test_dev_init(test);

	return 0;
}

static void ioctl_test_exit(struct kunit *test)
{
	/* nothing to do */
}

static struct kunit_case ioctl_example_test_cases[] = {
	KUNIT_CASE(test_ioctl_slsi_set_suspend_mode),
	KUNIT_CASE(test_ioctl_slsi_set_p2p_oppps),
	KUNIT_CASE(test_ioctl_slsi_p2p_set_noa_params),
	KUNIT_CASE(test_ioctl_slsi_p2p_ecsa),
	KUNIT_CASE(test_ioctl_slsi_set_ap_p2p_wps_ie),
	KUNIT_CASE(test_ioctl_slsi_p2p_lo_start),
	KUNIT_CASE(test_ioctl_slsi_p2p_lo_stop),
	KUNIT_CASE(test_ioctl_slsi_rx_filter_add),
	KUNIT_CASE(test_ioctl_slsi_rx_filter_remove),
	KUNIT_CASE(test_ioctl_slsi_legacy_roam_trigger_write),
	KUNIT_CASE(test_ioctl_slsi_legacy_roam_scan_trigger_read),
	KUNIT_CASE(test_ioctl_slsi_roam_add_scan_frequencies_legacy),
	KUNIT_CASE(test_ioctl_slsi_roam_add_scan_channels_legacy),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_frequencies_read_legacy),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_channels_read_legacy),
	KUNIT_CASE(test_ioctl_slsi_set_wtc_mode),
	KUNIT_CASE(test_ioctl_slsi_reassoc_frequency_write_legacy),
	KUNIT_CASE(test_ioctl_slsi_reassoc_write_legacy),
	KUNIT_CASE(test_ioctl_slsi_set_country_rev),
	KUNIT_CASE(test_ioctl_slsi_get_country_rev),
	KUNIT_CASE(test_ioctl_slsi_ioctl_set_roam_band),
	KUNIT_CASE(test_ioctl_slsi_ioctl_set_band),
	KUNIT_CASE(test_ioctl_slsi_ioctl_get_roam_band),
	KUNIT_CASE(test_ioctl_slsi_ioctl_get_band),
	KUNIT_CASE(test_ioctl_slsi_factory_freq_band_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_trigger_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_trigger_read),
	KUNIT_CASE(test_ioctl_slsi_roam_delta_trigger_write),
	KUNIT_CASE(test_ioctl_slsi_roam_delta_trigger_read),
	KUNIT_CASE(test_ioctl_slsi_reassoc_frequency_write),
	KUNIT_CASE(test_ioctl_slsi_reassoc_write),
	KUNIT_CASE(test_ioctl_slsi_cached_channel_scan_period_write),
	KUNIT_CASE(test_ioctl_slsi_cached_channel_scan_period_read),
	KUNIT_CASE(test_ioctl_slsi_full_roam_scan_period_write),
	KUNIT_CASE(test_ioctl_slsi_full_roam_scan_period_read),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_max_active_channel_time_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_max_active_channel_time_read),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_probe_interval_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_probe_interval_read),
	KUNIT_CASE(test_ioctl_slsi_roam_mode_write),
	KUNIT_CASE(test_ioctl_slsi_roam_mode_read),
	KUNIT_CASE(test_ioctl_slsi_roam_offload_ap_list),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_band_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_band_read),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_control_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_control_read),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_home_time_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_home_time_read),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_home_away_time_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_home_away_time_read),
	KUNIT_CASE(test_ioctl_slsi_set_home_away_time_legacy),
	KUNIT_CASE(test_ioctl_slsi_set_home_time_legacy),
	KUNIT_CASE(test_ioctl_slsi_set_channel_time_legacy),
	KUNIT_CASE(test_ioctl_slsi_set_passive_time_legacy),
	KUNIT_CASE(test_ioctl_slsi_set_tid),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_frequencies_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_channels_write),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_frequencies_read),
	KUNIT_CASE(test_ioctl_slsi_roam_scan_channels_read),
	KUNIT_CASE(test_ioctl_slsi_roam_add_scan_frequencies),
	KUNIT_CASE(test_ioctl_slsi_roam_add_scan_channels),
	KUNIT_CASE(test_ioctl_slsi_okc_mode_write),
	KUNIT_CASE(test_ioctl_slsi_okc_mode_read),
	KUNIT_CASE(test_ioctl_slsi_wes_mode_write),
	KUNIT_CASE(test_ioctl_slsi_wes_mode_read),
	KUNIT_CASE(test_ioctl_slsi_set_ncho_mode),
	KUNIT_CASE(test_ioctl_slsi_get_ncho_mode),
	KUNIT_CASE(test_ioctl_slsi_set_dfs_scan_mode),
	KUNIT_CASE(test_ioctl_slsi_get_dfs_scan_mode),
	KUNIT_CASE(test_ioctl_slsi_set_pmk),
	KUNIT_CASE(test_ioctl_slsi_auto_chan_read),
	KUNIT_CASE(test_ioctl_slsi_send_action_frame),
	KUNIT_CASE(test_ioctl_slsi_setting_max_sta_write),
	KUNIT_CASE(test_ioctl_slsi_country_write),
	KUNIT_CASE(test_ioctl_slsi_update_rssi_boost),
	KUNIT_CASE(test_ioctl_slsi_set_tx_power_calling),
	KUNIT_CASE(test_ioctl_slsi_set_tx_power_sub6_band),
	KUNIT_CASE(test_ioctl_slsi_get_regulatory),
	KUNIT_CASE(test_ioctl_slsi_set_fcc_channel),
	KUNIT_CASE(test_ioctl_slsi_fake_mac_write),
	KUNIT_CASE(test_ioctl_slsi_get_bss_rssi),
	KUNIT_CASE(test_ioctl_slsi_get_bss_info),
	KUNIT_CASE(test_ioctl_slsi_get_cu),
	KUNIT_CASE(test_ioctl_slsi_get_assoc_reject_info),
	KUNIT_CASE(test_ioctl_slsi_set_bandwidth),
	KUNIT_CASE(test_ioctl_slsi_set_bss_bandwidth),
	KUNIT_CASE(test_ioctl_slsi_set_disconnect_ies),
	KUNIT_CASE(test_ioctl_slsi_start_power_measurement_detection),
	KUNIT_CASE(test_ioctl_slsi_ioctl_auto_chan_write),
	KUNIT_CASE(test_ioctl_slsi_ioctl_test_force_hang),
	KUNIT_CASE(test_ioctl_slsi_ioctl_set_latency_crt_data),
	KUNIT_CASE(test_ioctl_slsi_elna_bypass),
	KUNIT_CASE(test_ioctl_slsi_elna_bypass_int),
	KUNIT_CASE(test_ioctl_slsi_set_dwell_time),
	KUNIT_CASE(test_ioctl_slsi_max_dtim_suspend),
	KUNIT_CASE(test_ioctl_slsi_set_dtim_suspend),
	KUNIT_CASE(test_ioctl_slsi_force_roaming_bssid),
	KUNIT_CASE(test_ioctl_slsi_roaming_blacklist_add),
	KUNIT_CASE(test_ioctl_slsi_roaming_blacklist_remove),
	KUNIT_CASE(test_ioctl_slsi_ioctl_get_ps_disabled_duration),
	KUNIT_CASE(test_ioctl_slsi_ioctl_get_ps_entry_counter),
	KUNIT_CASE(test_ioctl_slsi_ioctl_cmd_success),
	KUNIT_CASE(test_ioctl_slsi_twt_setup),
	KUNIT_CASE(test_ioctl_slsi_twt_teardown),
	KUNIT_CASE(test_ioctl_slsi_twt_suspend),
	KUNIT_CASE(test_ioctl_slsi_twt_get_status),
	KUNIT_CASE(test_ioctl_slsi_twt_get_capabilities),
	KUNIT_CASE(test_ioctl_slsi_twt_get_statistics_query),
	KUNIT_CASE(test_ioctl_slsi_twt_clear_statistics),
	KUNIT_CASE(test_ioctl_slsi_get_sta_info),
	{}
};

static struct kunit_suite ioctl_test_module = {
		.name = "ioctl.c",
		.init = ioctl_test_init,
		.exit = ioctl_test_exit,
		.test_cases = ioctl_example_test_cases,
};

kunit_test_suite(ioctl_test_module);

