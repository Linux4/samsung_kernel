// SPDX-License-Identifier: GPL-2.0+
#include <kunit/test.h>

#include "kunit-common.h"
#include "kunit-mock-kernel.h"
#include "kunit-mock-mgt.h"
#include "kunit-mock-sap_mlme.h"
#include "../fw_test.c"

/* unit test function definition*/
static void test_slsi_fw_test_save_frame(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 vif = 0;

	fwtest->mlme_connect_req[0] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_connect_req), GFP_KERNEL);
	fapi_set_u16(skb, id, MLME_CONNECT_REQ);
	fapi_set_u16(skb, u.mlme_connect_req.vif, vif);

	slsi_fw_test_save_frame(sdev, fwtest, fwtest->mlme_connect_req, skb, true);

	kunit_kfree(test, skb->data);
	fwtest->mlme_connect_cfm[0] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_connect_cfm), GFP_KERNEL);
	fapi_set_u16(skb, id, MLME_CONNECT_CFM);
	fapi_set_u16(skb, u.mlme_connect_cfm.vif, vif);

	slsi_fw_test_save_frame(sdev, fwtest, fwtest->mlme_connect_cfm, skb, false);
}

static void test_slsi_fw_test_process_frame(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 vif = 0;

	skb->data = kunit_kzalloc(test, fapi_sig_size(ma_unitdata_ind), GFP_KERNEL);
	fapi_set_u16(skb, id, MA_UNITDATA_IND);
	fapi_set_u16(skb, u.ma_unitdata_ind.vif, vif);

	slsi_fw_test_process_frame(sdev, fwtest, skb, true);
	slsi_fw_test_process_frame(sdev, fwtest, skb, false);
}

static void test_slsi_fw_test_signal(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 vif = 0;

	skb->data = kunit_kzalloc(test, fapi_sig_size(ma_unitdata_ind), GFP_KERNEL);
	fapi_set_u16(skb, id, MA_UNITDATA_IND);
	fapi_set_u16(skb, u.ma_unitdata_ind.vif, vif);
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal(sdev, fwtest, skb));

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(skb, id, MLME_ADD_VIF_REQ);
	fapi_set_u16(skb, u.mlme_add_vif_req.vif, vif);
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal(sdev, fwtest, skb));

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_connect_req), GFP_KERNEL);
	fapi_set_u16(skb, id, MLME_CONNECT_REQ);
	fapi_set_u16(skb, u.mlme_connect_req.vif, vif);
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal(sdev, fwtest, skb));

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_del_vif_req), GFP_KERNEL);
	fapi_set_u16(skb, id, MLME_DEL_VIF_REQ);
	fapi_set_u16(skb, u.mlme_del_vif_req.vif, vif);
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal(sdev, fwtest, skb));
}

static void test_slsi_fw_test_signal_with_udi_header(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	struct udi_msg_t *msg = kunit_kzalloc(test, sizeof(struct udi_msg_t), GFP_KERNEL);
	struct fapi_vif_signal_header *fapi_header = kunit_kzalloc(test, sizeof(struct fapi_vif_signal_header), GFP_KERNEL);
	u16 vif = 0;

	skb->data = kunit_kzalloc(test, sizeof(struct udi_msg_t) + fapi_sig_size(mlme_del_vif_req), GFP_KERNEL);

	msg->direction = SLSI_LOG_DIRECTION_TO_HOST;
	memcpy(skb->data, msg, sizeof(struct udi_msg_t));

	fwtest->fw_test_enabled = true;

	fapi_header->id = cpu_to_le16(MLME_DISCONNECTED_IND);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	fapi_header->id = cpu_to_le16(MLME_CONNECT_IND);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	fapi_header->id = cpu_to_le16(MLME_CONNECTED_IND);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	fapi_header->id = cpu_to_le16(MLME_ROAMED_IND);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	fapi_header->id = cpu_to_le16(MLME_TDLS_PEER_IND);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	fapi_header->id = cpu_to_le16(MLME_CONNECT_CFM);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	fapi_header->id = cpu_to_le16(MLME_PROCEDURE_STARTED_IND);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	fapi_header->id = cpu_to_le16(MLME_START_CFM);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	fapi_header->id = cpu_to_le16(MA_BLOCKACKREQ_IND);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));

	fapi_header->id = cpu_to_le16(MA_SPARE_1_IND);
	memcpy(skb->data + sizeof(struct udi_msg_t), fapi_header, sizeof(struct fapi_vif_signal_header));
	KUNIT_EXPECT_EQ(test, 0, slsi_fw_test_signal_with_udi_header(sdev, fwtest, skb));
}

static void test_slsi_fw_test_connect_station_roam(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 flow_id = 0;

	skb->data = kunit_kzalloc(test, fapi_sig_size(ma_unitdata_ind), GFP_KERNEL);
	ndev_vif->is_fw_test = true;
	ndev_vif->activated = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	fwtest->mlme_procedure_started_ind[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_roamed_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_roamed_ind.flow_id, flow_id);

	slsi_fw_test_connect_station_roam(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_connect_start_station(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->activated = false;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;

	fwtest->mlme_connect_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_connect_req[ndev_vif->ifnum]->data = kunit_kzalloc(test, fapi_sig_size(mlme_connect_req), GFP_KERNEL);
	fwtest->mlme_connect_cfm[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	dev->ieee80211_ptr = kunit_kzalloc(test, sizeof(struct wireless_dev), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_connect_req), GFP_KERNEL);
	fapi_set_memcpy(skb, u.mlme_connect_req.bssid, "00:11:22:33:44:55");

	slsi_fw_test_connect_start_station(sdev, dev, fwtest, skb);

	kunit_kfree(test, ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = NULL;
	ndev_vif->activated = false;

	slsi_fw_test_connect_start_station(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_connect_station(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 flow_id = 0;

	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->activated = true;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_connect_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_connect_ind.result_code, FAPI_RESULTCODE_SUCCESS);
	fapi_set_u16(skb, u.mlme_connect_ind.flow_id, flow_id);

	fwtest->mlme_connect_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_connect_cfm[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	fwtest->mlme_procedure_started_ind[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	slsi_fw_test_connect_station(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_started_network(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->is_fw_test = true;
	ndev_vif->activated = false;
	dev->ieee80211_ptr = &ndev_vif->wdev;

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_start_cfm), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_start_cfm.result_code, FAPI_RESULTCODE_SUCCESS);

	slsi_fw_test_started_network(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_stop_network(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->is_fw_test = true;
	ndev_vif->activated = true;

	slsi_fw_test_stop_network(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_connect_start_ap(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 flow_id = 256;

	ndev_vif->peer_sta_record[0] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_procedure_started_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_procedure_started_ind.flow_id, flow_id);

	slsi_fw_test_connect_start_ap(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_connected_network(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 flow_id = 256;

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_connected_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_connected_ind.flow_id, flow_id);

	slsi_fw_test_connected_network(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_procedure_started_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 procedure_type = FAPI_PROCEDURETYPE_CONNECTION_STARTED;
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;
	u16 flow_id = 256;
	ndev_vif->is_fw_test = true;

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_procedure_started_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_procedure_started_ind.procedure_type, procedure_type);

	ndev_vif->ifnum = 0;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_add_vif_req[ndev_vif->ifnum]->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	
	slsi_fw_test_procedure_started_ind(sdev, dev, fwtest, skb);

	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(skb, u.mlme_procedure_started_ind.flow_id, flow_id);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_procedure_started_ind(sdev, dev, fwtest, skb);

	virtual_interface_type = FAPI_VIFTYPE_NAN;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_procedure_started_ind(sdev, dev, fwtest, skb);

	procedure_type = FAPI_PROCEDURETYPE_UNKNOWN;
	fapi_set_u16(skb, u.mlme_procedure_started_ind.procedure_type, procedure_type);
	slsi_fw_test_procedure_started_ind(sdev, dev, fwtest, skb);

	ndev_vif->is_fw_test = false;
	slsi_fw_test_procedure_started_ind(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_connect_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;

	ndev_vif->ifnum = 0;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_add_vif_req[ndev_vif->ifnum]->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	ndev_vif->is_fw_test = true;
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	slsi_fw_test_connect_ind(sdev, dev, fwtest, skb);

	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_connect_ind(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_connected_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;

	ndev_vif->ifnum = 0;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_add_vif_req[ndev_vif->ifnum]->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	slsi_fw_test_connected_ind(sdev, dev, fwtest, skb);

	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_connected_ind(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_roamed_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;

	ndev_vif->ifnum = 0;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_add_vif_req[ndev_vif->ifnum]->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	slsi_fw_test_roamed_ind(sdev, dev, fwtest, skb);

	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_roamed_ind(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_disconnect_station(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->is_fw_test = true;
	ndev_vif->ifnum = 0;
	ndev_vif->activated = true;

	slsi_fw_test_disconnect_station(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_disconnect_network(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_disconnected_ind), GFP_KERNEL);
	fapi_set_memcpy(skb, u.mlme_disconnected_ind.peer_sta_address, "00:11:22:33:44:55");
	ndev_vif->is_fw_test = true;
	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = false;
	ndev_vif->peer_sta_record[0] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[0]->valid = true;
	memcpy(ndev_vif->peer_sta_record[0]->address,
	       ((struct fapi_signal *)(skb)->data)->u.mlme_disconnected_ind.peer_sta_address, ETH_ALEN);

	slsi_fw_test_disconnect_network(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_disconnected_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;

	ndev_vif->ifnum = 0;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_add_vif_req[ndev_vif->ifnum]->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	ndev_vif->is_fw_test = true;
	ndev_vif->ifnum = 0;
	slsi_fw_test_disconnected_ind(sdev, dev, fwtest, skb);

	virtual_interface_type = FAPI_VIFTYPE_AP;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_disconnected_ind(sdev, dev, fwtest, skb);

	virtual_interface_type = FAPI_VIFTYPE_NAN;
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_disconnected_ind(sdev, dev, fwtest, skb);

}

static void test_slsi_fw_test_tdls_event_connected(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 flow_id = 512;

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_disconnected_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.flow_id, flow_id);
	fapi_set_memcpy(skb, u.mlme_tdls_peer_ind.peer_sta_address, "00:11:22:33:44:55");

	slsi_fw_test_tdls_event_connected(sdev, dev, skb);
}

static void test_slsi_fw_test_tdls_event_disconnected(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_disconnected_ind), GFP_KERNEL);
	fapi_set_memcpy(skb, u.mlme_tdls_peer_ind.peer_sta_address, "00:11:22:33:44:55");

	ndev_vif->vif_type = FAPI_VIFTYPE_STATION;
	ndev_vif->sta.tdls_enabled = false;
	ndev_vif->peer_sta_record[0] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[0]->valid = true;
	ndev_vif->peer_sta_record[0]->aid = 1;
	memcpy(ndev_vif->peer_sta_record[0]->address,
	       ((struct fapi_signal *)(skb)->data)->u.mlme_tdls_peer_ind.peer_sta_address, ETH_ALEN);

	slsi_fw_test_tdls_event_disconnected(sdev, dev, skb);
}

static void test_slsi_fw_test_tdls_peer_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 virtual_interface_type = FAPI_VIFTYPE_STATION;
	u16 tdls_event = FAPI_TDLSEVENT_DISCONNECTED;

	ndev_vif->is_fw_test = true;
	ndev_vif->activated = true;
	ndev_vif->ifnum = 0;
	ndev_vif->ifnum = SLSI_NET_INDEX_WLAN;
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET] = kunit_kzalloc(test, sizeof(struct slsi_peer), GFP_KERNEL);
	ndev_vif->peer_sta_record[SLSI_STA_PEER_QUEUESET]->valid = true;
	ndev_vif->sta.tdls_enabled = false;

	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_add_vif_req[ndev_vif->ifnum]->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_tdls_peer_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, tdls_event);
	slsi_fw_test_tdls_peer_ind(sdev, dev, fwtest, skb);

	tdls_event = FAPI_TDLSEVENT_CONNECTED;
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_tdls_peer_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, tdls_event);
	slsi_fw_test_tdls_peer_ind(sdev, dev, fwtest, skb);

	tdls_event = FAPI_TDLSEVENT_DISCOVERED;
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_tdls_peer_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, tdls_event);
	slsi_fw_test_tdls_peer_ind(sdev, dev, fwtest, skb);

	tdls_event += 1;
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_tdls_peer_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.tdls_event, tdls_event);
	slsi_fw_test_tdls_peer_ind(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_start_cfm(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 virtual_interface_type = FAPI_VIFTYPE_AP;
	u16 tdls_event = FAPI_TDLSEVENT_CONNECTED;

	ndev_vif->is_fw_test = true;
	ndev_vif->ifnum = 0;
	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_add_vif_req[ndev_vif->ifnum]->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);
	slsi_fw_test_start_cfm(sdev, dev, fwtest, skb);

	virtual_interface_type = FAPI_VIFTYPE_STATION;
	slsi_fw_test_start_cfm(sdev, dev, fwtest, skb);

	ndev_vif->is_fw_test = false;
	slsi_fw_test_start_cfm(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_add_vif_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 vif = 0;

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_add_vif_req.vif, vif);

	slsi_fw_test_add_vif_req(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_del_vif_req(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 virtual_interface_type = FAPI_VIFTYPE_AP;
	u16 vif = 0;

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_del_vif_req), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_del_vif_req.vif, vif);
	ndev_vif->ifnum = 0;

	fwtest->mlme_add_vif_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	fwtest->mlme_add_vif_req[ndev_vif->ifnum]->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(fwtest->mlme_add_vif_req[ndev_vif->ifnum], u.mlme_add_vif_req.virtual_interface_type,
		     virtual_interface_type);

	fwtest->mlme_connect_req[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	fwtest->mlme_connect_cfm[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	fwtest->mlme_procedure_started_ind[ndev_vif->ifnum] = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	slsi_fw_test_del_vif_req(sdev, dev, fwtest, skb);

	virtual_interface_type = FAPI_VIFTYPE_STATION;
	slsi_fw_test_del_vif_req(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_ma_blockackreq_ind(struct kunit *test)
{
	struct net_device *dev = TEST_TO_DEV(test);
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct slsi_dev *sdev = ndev_vif->sdev;
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);

	ndev_vif->is_fw_test = true;
	slsi_fw_test_ma_blockackreq_ind(sdev, dev, fwtest, skb);

	ndev_vif->is_fw_test = false;
	slsi_fw_test_ma_blockackreq_ind(sdev, dev, fwtest, skb);
}

static void test_slsi_fw_test_work(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);
	struct sk_buff *skb = kunit_kzalloc(test, sizeof(struct sk_buff), GFP_KERNEL);
	u16 vif = 1;

	fwtest->sdev = sdev;
	slsi_fw_test_init(fwtest->sdev, fwtest);

	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_procedure_started_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_procedure_started_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_PROCEDURE_STARTED_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_connect_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_connect_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_CONNECT_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_roamed_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_roamed_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_ROAMED_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_connected_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_connected_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_CONNECTED_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_disconnected_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_disconnected_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_DISCONNECTED_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_tdls_peer_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_tdls_peer_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_TDLS_PEER_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_start_cfm), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_start_cfm.vif, vif);
	fapi_set_u16(skb, id, MLME_START_CFM);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_add_vif_req), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_add_vif_req.vif, vif);
	fapi_set_u16(skb, id, MLME_ADD_VIF_REQ);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_del_vif_req), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_del_vif_req.vif, vif);
	fapi_set_u16(skb, id, MLME_DEL_VIF_REQ);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(ma_blockackreq_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.ma_blockackreq_ind.vif, vif);
	fapi_set_u16(skb, id, MA_BLOCKACKREQ_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

#ifdef CONFIG_SCSC_WLAN_TX_API
	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_frame_transmission_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_frame_transmission_ind.vif, vif);
	fapi_set_u16(skb, id, MLME_FRAME_TRANSMISSION_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(mlme_send_frame_cfm), GFP_KERNEL);
	fapi_set_u16(skb, u.mlme_send_frame_cfm.vif, vif);
	fapi_set_u16(skb, id, MLME_SEND_FRAME_CFM);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);
#endif

	//Unhanlded signal case
	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(ma_spare_1_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.ma_spare_1_ind.vif, vif);
	fapi_set_u16(skb, id, MA_SPARE_1_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	vif = CONFIG_SCSC_WLAN_MAX_INTERFACES + 1;
	kunit_kfree(test, skb->data);
	skb->data = kunit_kzalloc(test, fapi_sig_size(ma_spare_1_ind), GFP_KERNEL);
	fapi_set_u16(skb, u.ma_spare_1_ind.vif, vif);
	fapi_set_u16(skb, id, MA_SPARE_1_IND);
	slsi_skb_work_enqueue(&fwtest->fw_test_work, skb);
	slsi_fw_test_work(&fwtest->fw_test_work.work);

	slsi_fw_test_deinit(fwtest->sdev, fwtest);
}

static void test_slsi_fw_test_init(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);

	slsi_fw_test_init(sdev, fwtest);
}

static void test_slsi_fw_test_deinit(struct kunit *test)
{
	struct slsi_dev *sdev = TEST_TO_SDEV(test);
	struct slsi_fw_test *fwtest = kunit_kzalloc(test, sizeof(struct slsi_fw_test), GFP_KERNEL);

	slsi_fw_test_deinit(sdev, fwtest);
}


/* Test fictures */
static int fw_test_test_init(struct kunit *test)
{
	test_dev_init(test);

	kunit_log(KERN_INFO, test, "%s completed.", __func__);
	return 0;
}

static void fw_test_test_exit(struct kunit *test)
{
	kunit_log(KERN_INFO, test, "%s: completed.", __func__);
}

/* KUnit testcase definitions */
static struct kunit_case fw_test_test_cases[] = {
	KUNIT_CASE(test_slsi_fw_test_init),
	KUNIT_CASE(test_slsi_fw_test_save_frame),
	KUNIT_CASE(test_slsi_fw_test_process_frame),
	KUNIT_CASE(test_slsi_fw_test_signal),
	KUNIT_CASE(test_slsi_fw_test_signal_with_udi_header),
	KUNIT_CASE(test_slsi_fw_test_connect_station_roam),
	KUNIT_CASE(test_slsi_fw_test_connect_start_station),
	KUNIT_CASE(test_slsi_fw_test_connect_station),
	KUNIT_CASE(test_slsi_fw_test_started_network),
	KUNIT_CASE(test_slsi_fw_test_stop_network),
	KUNIT_CASE(test_slsi_fw_test_connect_start_ap),
	KUNIT_CASE(test_slsi_fw_test_connected_network),
	KUNIT_CASE(test_slsi_fw_test_procedure_started_ind),
	KUNIT_CASE(test_slsi_fw_test_connect_ind),
	KUNIT_CASE(test_slsi_fw_test_connected_ind),
	KUNIT_CASE(test_slsi_fw_test_roamed_ind),
	KUNIT_CASE(test_slsi_fw_test_disconnect_station),
	KUNIT_CASE(test_slsi_fw_test_disconnect_network),
	KUNIT_CASE(test_slsi_fw_test_disconnected_ind),
	KUNIT_CASE(test_slsi_fw_test_tdls_event_connected),
	KUNIT_CASE(test_slsi_fw_test_tdls_event_disconnected),
	KUNIT_CASE(test_slsi_fw_test_tdls_peer_ind),
	KUNIT_CASE(test_slsi_fw_test_start_cfm),
	KUNIT_CASE(test_slsi_fw_test_add_vif_req),
	KUNIT_CASE(test_slsi_fw_test_del_vif_req),
	KUNIT_CASE(test_slsi_fw_test_ma_blockackreq_ind),
	KUNIT_CASE(test_slsi_fw_test_work),
	KUNIT_CASE(test_slsi_fw_test_deinit),
	{}
};

static struct kunit_suite fw_test_test_suite[] = {
	{
		.name = "fw_test-test",
		.test_cases = fw_test_test_cases,
		.init = fw_test_test_init,
		.exit = fw_test_test_exit,
	}
};

kunit_test_suites(fw_test_test_suite);
