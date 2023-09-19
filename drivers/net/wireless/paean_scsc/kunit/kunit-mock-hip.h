/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_HIP_H__
#define __KUNIT_MOCK_HIP_H__

#include "../hip.h"

#define slsi_hip_get_skb_from_smapper(args...)		kunit_mock_slsi_hip_get_skb_from_smapper(args)
#define slsi_hip_sap_setup(args...)			kunit_mock_slsi_hip_sap_setup(args)
#define slsi_hip_sap_register(args...)			kunit_mock_slsi_hip_sap_register(args)
#define slsi_hip_consume_smapper_entry(args...)		kunit_mock_slsi_hip_consume_smapper_entry(args)
#define slsi_hip_rx(args...)				kunit_mock_slsi_hip_rx(args)
#define slsi_hip_tx_done(args...)			kunit_mock_slsi_hip_tx_done(args)
#define slsi_hip_cm_unregister(args...)			kunit_mock_slsi_hip_cm_unregister(args)
#define slsi_hip_get_skb_data_from_smapper(args...)	kunit_mock_slsi_hip_get_skb_data_from_smapper(args)
#define slsi_hip_reprocess_skipped_data_bh(args...)	kunit_mock_slsi_hip_reprocess_skipped_data_bh(args)
#define slsi_hip_sap_unregister(args...)		kunit_mock_slsi_hip_sap_unregister(args)
#define slsi_hip_reprocess_skipped_ctrl_bh(args...)	kunit_mock_slsi_hip_reprocess_skipped_ctrl_bh(args)
#define slsi_hip_start(args...)				kunit_mock_slsi_hip_start(args)
#define slsi_hip_stop(args...)				kunit_mock_slsi_hip_stop(args)
#define slsi_hip_cm_register(args...)			kunit_mock_slsi_hip_cm_register(args)
#define slsi_hip_setup_ext(args...)			kunit_mock_slsi_hip_setup_ext(args)


static struct sk_buff *kunit_mock_slsi_hip_get_skb_from_smapper(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return NULL;
}

static int kunit_mock_slsi_hip_sap_setup(struct slsi_dev *sdev)
{
	if (sdev->device_config.host_state == 100)
		return -EILSEQ;

	return 0;
}

static int kunit_mock_slsi_hip_sap_register(struct sap_api *sap_api)
{
	return 0;
}

static int kunit_mock_slsi_hip_consume_smapper_entry(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return 0;
}

static int kunit_mock_slsi_hip_rx(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return 0;
}

#ifdef CONFIG_SCSC_WLAN_TX_API
static int kunit_mock_slsi_hip_tx_done(struct slsi_dev *sdev, u32 colour, bool more)
{
	return 0;
}
#else
static int kunit_mock_slsi_hip_tx_done(struct slsi_dev *sdev, u8 vif, u8 peer_index, u8 ac)
{
	return 0;
}
#endif

static void kunit_mock_slsi_hip_cm_unregister(struct slsi_dev *sdev)
{
	return;
}

static void *kunit_mock_slsi_hip_get_skb_data_from_smapper(struct slsi_dev *sdev, struct sk_buff *skb)
{
	return NULL;
}

static void kunit_mock_slsi_hip_reprocess_skipped_data_bh(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_hip_sap_unregister(struct sap_api *sap_api)
{
	return 0;
}

static void kunit_mock_slsi_hip_reprocess_skipped_ctrl_bh(struct slsi_dev *sdev)
{
	return;
}

static int kunit_mock_slsi_hip_start(struct slsi_dev *sdev)
{
	if (sdev->service) {
		if (sdev->device_config.host_state == 12)
			return -EIO;
		else
			return 0;
	}
	return 0;
}

static int kunit_mock_slsi_hip_stop(struct slsi_dev *sdev)
{
	return 0;
}

static int kunit_mock_slsi_hip_cm_register(struct slsi_dev *sdev, struct device *dev)
{
	return 0;
}

static int kunit_mock_slsi_hip_setup_ext(struct slsi_dev *sdev)
{
	if (sdev->hip.hip_priv) {
		if (sdev->hip.hip_priv->host_pool_id_dat == 11)
			return -EIO;
	}
	return 0;
}
#endif
