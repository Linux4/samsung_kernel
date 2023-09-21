/* SPDX-License-Identifier: GPL-2.0+ */

#ifndef __KUNIT_MOCK_MLME_NAN_H__
#define __KUNIT_MOCK_MLME_NAN_H__

#include "../mlme.h"

#define slsi_mlme_nan_publish(args...)		kunit_mock_slsi_mlme_nan_publish(args)
#define slsi_mlme_ndp_terminate(args...)	kunit_mock_slsi_mlme_ndp_terminate(args)
#define slsi_mlme_ndp_response(args...)		kunit_mock_slsi_mlme_ndp_response(args)
#define slsi_mlme_nan_subscribe(args...)	kunit_mock_slsi_mlme_nan_subscribe(args)
#define slsi_mlme_nan_set_config(args...)	kunit_mock_slsi_mlme_nan_set_config(args)
#define slsi_mlme_nan_tx_followup(args...)	kunit_mock_slsi_mlme_nan_tx_followup(args)
#define slsi_mlme_nan_range_cancel_req(args...)	kunit_mock_slsi_mlme_nan_range_cancel_req(args)
#define slsi_mlme_nan_range_req(args...)	kunit_mock_slsi_mlme_nan_range_req(args)
#define slsi_mlme_nan_enable(args...)		kunit_mock_slsi_mlme_nan_enable(args)
#define slsi_mlme_ndp_request(args...)		kunit_mock_slsi_mlme_ndp_request(args)


static int kunit_mock_slsi_mlme_nan_publish(struct slsi_dev *sdev, struct net_device *dev,
					    struct slsi_hal_nan_publish_req *hal_req,
					    u16 publish_id)
{
	return 0;
}

static int kunit_mock_slsi_mlme_ndp_terminate(struct slsi_dev *sdev, struct net_device *dev,
					      u16 ndp_instance_id, u16 transaction_id)
{
	return 0;
}

static int kunit_mock_slsi_mlme_ndp_response(struct slsi_dev *sdev, struct net_device *dev,
					     struct slsi_hal_nan_data_path_indication_response *hal_req,
					     u16 local_ndp_instance_id)
{
	return 0;
}

static int kunit_mock_slsi_mlme_nan_subscribe(struct slsi_dev *sdev, struct net_device *dev,
					      struct slsi_hal_nan_subscribe_req *hal_req,
					      u16 subscribe_id)
{
	return 0;
}

static int kunit_mock_slsi_mlme_nan_set_config(struct slsi_dev *sdev, struct net_device *dev,
					       struct slsi_hal_nan_config_req *hal_req)
{
	return 0;
}

static int kunit_mock_slsi_mlme_nan_tx_followup(struct slsi_dev *sdev, struct net_device *dev,
						struct slsi_hal_nan_transmit_followup_req *hal_req)
{
	return 0;
}

static int kunit_mock_slsi_mlme_nan_range_cancel_req(struct slsi_dev *sdev, struct net_device *dev)
{
	return 0;
}

static int kunit_mock_slsi_mlme_nan_range_req(struct slsi_dev *sdev, struct net_device *dev,
					      u8 count, struct slsi_rtt_config *nl_rtt_params)
{
	return 0;
}

static int kunit_mock_slsi_mlme_nan_enable(struct slsi_dev *sdev, struct net_device *dev,
					   struct slsi_hal_nan_enable_req *hal_req)
{
	return 0;
}

static int kunit_mock_slsi_mlme_ndp_request(struct slsi_dev *sdev, struct net_device *dev,
					    struct slsi_hal_nan_data_path_initiator_req *hal_req,
					    u32 ndp_instance_id, u16 ndl_vif_id)
{
	return 0;
}
#endif
