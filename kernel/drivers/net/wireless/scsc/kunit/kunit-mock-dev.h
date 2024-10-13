/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_DEV_H__
#define __KUNIT_MOCK_DEV_H__

#include "../dev.h"

#define slsi_dev_6ghz_skip_acs(args...)			kunit_mock_slsi_dev_6ghz_skip_acs(args)
#define slsi_dev_gscan_supported(args...)		kunit_mock_slsi_dev_gscan_supported(args)
#define slsi_dev_llslogs_supported(args...)		kunit_mock_slsi_dev_llslogs_supported(args)
#define slsi_dev_6ghz_split_scan_enabled(args...)	kunit_mock_slsi_dev_6ghz_split_scan_enabled(args)
#define slsi_get_nan_max_ndp_instances(args...)		kunit_mock_slsi_get_nan_max_ndp_instances(args)
#define slsi_dump_system_error_buffer(args...)		kunit_mock_slsi_dump_system_error_buffer(args)
#define slsi_dev_vo_vi_block_ack(args...)		kunit_mock_slsi_dev_vo_vi_block_ack(args)
#define slsi_get_nan_max_ndi_ifaces(args...)		kunit_mock_slsi_get_nan_max_ndi_ifaces(args)
#define slsi_get_nan_ndp_delay(args...)			kunit_mock_slsi_get_nan_ndp_delay(args)
#define slsi_dev_rtt_supported(args...)			kunit_mock_slsi_dev_rtt_supported(args)
#define slsi_dev_epno_supported(args...)		kunit_mock_slsi_dev_epno_supported(args)
#define slsi_add_log_to_system_error_buffer(args...)	kunit_mock_slsi_add_log_to_system_error_buffer(args)
#define slsi_dev_nan_supported(args...)			kunit_mock_slsi_dev_nan_supported(args)
#define slsi_dev_attach(args...)			kunit_mock_slsi_dev_attach(args)
#define slsi_dev_get_scan_result_count(args...)		kunit_mock_slsi_dev_get_scan_result_count(args)
#define slsi_dev_lls_supported(args...)			kunit_mock_slsi_dev_lls_supported(args)
#define slsi_dev_nan_is_ipv6_link_tlv_include(args...)	kunit_mock_slsi_dev_nan_is_ipv6_link_tlv_include(args)
#define slsi_dev_detach(args...)			kunit_mock_slsi_dev_detach(args)
#define slsi_get_nan_mac_random(args...)		kunit_mock_slsi_get_nan_mac_random(args)
#define slsi_get_nan_ndp_max_time(args...)		kunit_mock_slsi_get_nan_ndp_max_time(args)
#define slsi_rx_data_deliver_skb(args...)		kunit_mock_slsi_rx_data_deliver_skb(args)
#define slsi_rx_netdev_data_work(args...)		kunit_mock_slsi_rx_netdev_data_work(args)


static struct slsi_dev sdev;
static bool kunit_gscan_disabled;
static bool kunit_lls_disabled;
static bool kunit_rtt_disabled;

static void kunit_set_gscan_disabled(bool disable)
{
	kunit_gscan_disabled = disable;
}

static void kunit_set_lls_disabled(bool disable)
{
	kunit_lls_disabled = disable;
}

static void kunit_set_rtt_disabled(bool disable)
{
	kunit_rtt_disabled = disable;
}

static bool kunit_mock_slsi_dev_6ghz_skip_acs(void)
{
	return false;
}

static bool kunit_mock_slsi_dev_gscan_supported(void)
{
	return !kunit_gscan_disabled;
}

static bool kunit_mock_slsi_dev_rtt_supported(void)
{
	return !kunit_rtt_disabled;
}

static bool kunit_mock_slsi_dev_llslogs_supported(void)
{
	return false;
}

static bool kunit_mock_slsi_dev_6ghz_split_scan_enabled(void)
{
	return false;
}

static int kunit_mock_slsi_get_nan_max_ndp_instances(void)
{
	return 0;
}

static void kunit_mock_slsi_dump_system_error_buffer(struct slsi_dev *sdev)
{
	return;
}

static bool kunit_mock_slsi_dev_vo_vi_block_ack(void)
{
	return false;
}

static int kunit_mock_slsi_get_nan_max_ndi_ifaces(void)
{
	return 0;
}

static int kunit_mock_slsi_get_nan_ndp_delay(void)
{
	return 0;
}

static bool kunit_mock_slsi_dev_epno_supported(void)
{
	return false;
}

static void kunit_mock_slsi_add_log_to_system_error_buffer(struct slsi_dev *sdev, char *input_buffer)
{
	return;
}

static int kunit_mock_slsi_dev_nan_supported(struct slsi_dev *sdev)
{
	return 0;
}

static struct slsi_dev *kunit_mock_slsi_dev_attach(struct device *dev, struct scsc_mx *core,
						   struct scsc_service_client *mx_wlan_client)
{
	return &sdev;
}

static int kunit_mock_slsi_dev_get_scan_result_count(void)
{
	return 200;
}

static bool kunit_mock_slsi_dev_lls_supported(void)
{
	return !kunit_lls_disabled;
}

static bool kunit_mock_slsi_dev_nan_is_ipv6_link_tlv_include(void)
{
	return false;
}

static void kunit_mock_slsi_dev_detach(struct slsi_dev *sdev)
{
	return;
}

static bool kunit_mock_slsi_get_nan_mac_random(void)
{
	return false;
}

static int kunit_mock_slsi_get_nan_ndp_max_time(void)
{
	return 0;
}

static void kunit_mock_slsi_rx_data_deliver_skb(struct slsi_dev *sdev, struct net_device *dev,
						struct sk_buff *skb, bool ctx_napi)
{
}

static void kunit_mock_slsi_rx_netdev_data_work(struct work_struct *work)
{
}
#endif
