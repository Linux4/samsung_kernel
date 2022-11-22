/*****************************************************************************
 *
 * Copyright (c) 2012 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include "debug.h"
#include "mlme.h"

#define SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, val) \
	{ \
		u16 attribute_len = 1; \
		struct sk_buff *req_p = req; \
		fapi_append_data((req_p), (u8 *)&(attribute), 2); \
		fapi_append_data((req_p), (u8 *)&attribute_len, 2); \
		fapi_append_data((req_p), (u8 *)&(val), 1); \
	}

#define SLSI_FAPI_NAN_ATTRIBUTE_PUT_U16(req, attribute, val) \
	{ \
		u16 attribute_len = 2; \
		__le16 le16val = cpu_to_le16(val); \
		struct sk_buff *req_p = req; \
		fapi_append_data((req_p), (u8 *)&(attribute), 2); \
		fapi_append_data((req_p), (u8 *)&attribute_len, 2); \
		fapi_append_data((req_p), (u8 *)&le16val, 2); \
	}

#define SLSI_FAPI_NAN_ATTRIBUTE_PUT_U32(req, attribute, val) \
	{ \
		u16 attribute_len = 4; \
		__le32 le32val = cpu_to_le32(val);\
		struct sk_buff *req_p = req; \
		fapi_append_data((req_p), (u8 *)&(attribute), 2); \
		fapi_append_data((req_p), (u8 *)&attribute_len, 2); \
		fapi_append_data((req_p), (u8 *)&le32val, 4); \
	}

#define SLSI_FAPI_NAN_ATTRIBUTE_PUT_DATA(req, attribute, val, val_len) \
	{ \
		u16 attribute_len = (val_len); \
		struct sk_buff *req_p = req; \
		fapi_append_data((req_p), (u8 *)&(attribute), 2); \
		fapi_append_data((req_p), (u8 *)&attribute_len, 2); \
		fapi_append_data((req_p), (val), (attribute_len)); \
	}

static void slsi_mlme_nan_enable_fapi_data(struct sk_buff *req, struct slsi_hal_nan_enable_req *hal_req)
{
	u8  nan_config_fields_header[] = {0xdd, 0x00, 0x00, 0x16, 0x32, 0x0b, 0x01};
	u8 *header_ptr;
	u16 attribute;
	u8  band_usage = BIT(0) | BIT(1);
	u8  scan_param[] = {0, 0, 0};
	int len = 0;

	header_ptr = fapi_append_data(req, nan_config_fields_header, sizeof(nan_config_fields_header));
	len += sizeof(nan_config_fields_header);

	if (hal_req->config_2dot4g_beacons && !hal_req->beacon_2dot4g_val)
		band_usage &= ~BIT(0);
	if (hal_req->config_2dot4g_sdf && !hal_req->sdf_2dot4g_val)
		band_usage &= ~BIT(1);
	if (hal_req->config_5g_beacons && hal_req->beacon_5g_val)
		band_usage |= BIT(2);
	if (hal_req->config_5g_sdf && hal_req->sdf_5g_val)
		band_usage |= BIT(3);

	attribute = SLSI_FAPI_NAN_CONFIG_PARAM_BAND_USAGE;
	SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, band_usage);
	attribute = SLSI_FAPI_NAN_CONFIG_PARAM_MASTER_PREFERENCE;
	SLSI_FAPI_NAN_ATTRIBUTE_PUT_U16(req, attribute, hal_req->master_pref);
	len += 11; /* 5 for band_usage, 6 for master preference */

	if (hal_req->config_sid_beacon) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_SID_BEACON;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->sid_beacon_val);
		len += 5;
	}

	if (hal_req->config_2dot4g_rssi_close) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_2_4_RSSI_CLOSE;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_close_2dot4g_val);
		len += 5;
	}

	if (hal_req->config_2dot4g_rssi_middle) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_2_4_RSSI_MIDDLE;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_middle_2dot4g_val);
		len += 5;
	}

	if (hal_req->config_2dot4g_rssi_proximity) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_2_4_RSSI_PROXIMITY;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_proximity_2dot4g_val);
		len += 5;
	}

	if (hal_req->config_5g_rssi_close) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_5_RSSI_CLOSE;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_close_5g_val);
		len += 5;
	}

	if (hal_req->config_5g_rssi_middle) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_5_RSSI_MIDDLE;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_middle_5g_val);
		len += 5;
	}

	if (hal_req->config_5g_rssi_close_proximity) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_5_RSSI_PROXIMITY;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_close_proximity_5g_val);
		len += 5;
	}

	if (hal_req->config_hop_count_limit) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_HOP_COUNT_LIMIT;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->hop_count_limit_val);
		len += 5;
	}

	if (hal_req->config_rssi_window_size) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_RSSI_WINDOW_SIZE;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_window_size_val);
		len += 5;
	}

	if (hal_req->config_scan_params) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_SCAN_PARAMETER_2_4;
		scan_param[0] = hal_req->scan_params_val.dwell_time[0];
		scan_param[1] = hal_req->scan_params_val.scan_period[0] & 0x00FF;
		scan_param[2] = (hal_req->scan_params_val.scan_period[0] & 0xFF00) >> 8;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_DATA(req, attribute, scan_param, 3);
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_SCAN_PARAMETER_5;
		scan_param[0] = hal_req->scan_params_val.dwell_time[1];
		scan_param[1] = hal_req->scan_params_val.scan_period[1] & 0x00FF;
		scan_param[2] = (hal_req->scan_params_val.scan_period[1] & 0xFF00) >> 8;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_DATA(req, attribute, scan_param, 3);
		len += 7 * 2;
	}

	/* update len */
	header_ptr[1] = len - 2;
}

int slsi_mlme_nan_enable(struct slsi_dev *sdev, struct net_device *dev, struct slsi_hal_nan_enable_req *hal_req)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_oper_ctrl = 0;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");

	/* max mbulk data for mlme-nan-start.req is about 87 bytes but
	 * allocate 100 bytes
	 */
	req = fapi_alloc(mlme_nan_start_req, MLME_NAN_START_REQ, ndev_vif->ifnum, 100);
	if (!req) {
		SLSI_NET_ERR(dev, "fapi alloc failure\n");
		return -ENOMEM;
	}

	if (hal_req->config_cluster_attribute_val)
		nan_oper_ctrl |= FAPI_NANOPERATIONCONTROL_CLUSTER_SDF;
	nan_oper_ctrl |= FAPI_NANOPERATIONCONTROL_MAC_ADDRESS_EVENT | FAPI_NANOPERATIONCONTROL_START_CLUSTER_EVENT |
			FAPI_NANOPERATIONCONTROL_JOINED_CLUSTER_EVENT;

	fapi_set_u16(req, u.mlme_nan_start_req.cluster_low, hal_req->cluster_low);
	fapi_set_u16(req, u.mlme_nan_start_req.cluster_high, hal_req->cluster_high);
	fapi_set_u16(req, u.mlme_nan_start_req.nan_operation_control_flags, nan_oper_ctrl);

	slsi_mlme_nan_enable_fapi_data(req, hal_req);

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_START_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_nan_start_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NAN_START_CFM(result:0x%04x) ERROR\n",
			     fapi_get_u16(cfm, u.mlme_host_state_cfm.result_code));
		r = -EINVAL;
	}

	slsi_kfree_skb(cfm);
	return r;
}

static void slsi_mlme_nan_append_tlv(struct sk_buff *req, __le16 tlv_t, __le16 tlv_l, u8 *tlv_v, u8 **header_ptr,
				     u8 header_ie_generic_len, u8 **end_ptr)
{
	u8 tmp_buf[255 + 4];
	u8 *tmp_buf_pos;
	int tmp_buf_len, len1, ip_ie_len;

	memcpy(tmp_buf, &tlv_t, 2);
	memcpy(tmp_buf + 2, &tlv_l, 2);
	memcpy(tmp_buf + 4, tlv_v, tlv_l);
	tmp_buf_len = 4 + tlv_l;
	ip_ie_len = *end_ptr - *header_ptr - 2;
	tmp_buf_pos = tmp_buf;

	while (tmp_buf_len + ip_ie_len > 255) {
		len1 = 255 - ip_ie_len;
		fapi_append_data(req, tmp_buf_pos, len1);
		(*header_ptr)[1] = 255;
		tmp_buf_len -= len1;
		tmp_buf_pos += len1;
		ip_ie_len = 0;
		if (tmp_buf_len) {
			*header_ptr = fapi_append_data(req, *header_ptr, header_ie_generic_len);
			*end_ptr = *header_ptr + header_ie_generic_len;
		} else {
			*end_ptr = *header_ptr + header_ie_generic_len + 255;
		}
	}
	if (tmp_buf_len) {
		fapi_append_data(req, tmp_buf, tmp_buf_len);
		*end_ptr += tmp_buf_len;
	}
}

static void slsi_mlme_nan_publish_fapi_data(struct sk_buff *req, struct slsi_hal_nan_publish_req *hal_req)
{
	u8  nan_publish_fields_header[] = {0xdd, 0x00, 0x00, 0x16, 0x32, 0x0b, 0x02};
	u8 *header_ptr, *end_ptr;
	__le16 le16val;
	u32 binding_mask = 0;

	header_ptr = fapi_append_data(req, nan_publish_fields_header, sizeof(nan_publish_fields_header));
	le16val = cpu_to_le16(hal_req->ttl);
	fapi_append_data(req, (u8 *)&le16val, 2);
	le16val = cpu_to_le16(hal_req->period);
	fapi_append_data(req, (u8 *)&le16val, 2);
	fapi_append_data(req, &hal_req->publish_type, 1);
	fapi_append_data(req, &hal_req->tx_type, 1);
	fapi_append_data(req, &hal_req->publish_count, 1);
	fapi_append_data(req, &hal_req->publish_match_indicator, 1);
	fapi_append_data(req, &hal_req->rssi_threshold_flag, 1);
	end_ptr = fapi_append_data(req, (u8 *)&binding_mask, 4);
	end_ptr += 4;

	if (hal_req->service_name_len)
		slsi_mlme_nan_append_tlv(req, cpu_to_le16 (SLSI_FAPI_NAN_SERVICE_NAME),
					 cpu_to_le16 (hal_req->service_name_len), hal_req->service_name, &header_ptr,
					 sizeof(nan_publish_fields_header), &end_ptr);

	if (hal_req->service_specific_info_len)
		slsi_mlme_nan_append_tlv(req, cpu_to_le16 (SLSI_FAPI_NAN_SERVICE_SPECIFIC_INFO),
					 cpu_to_le16 (hal_req->service_specific_info_len),
					 hal_req->service_specific_info, &header_ptr,
					 sizeof(nan_publish_fields_header), &end_ptr);

	if (hal_req->rx_match_filter_len)
		slsi_mlme_nan_append_tlv(req, cpu_to_le16 (SLSI_FAPI_NAN_RX_MATCH_FILTER),
					 cpu_to_le16 (hal_req->rx_match_filter_len), hal_req->rx_match_filter,
					 &header_ptr, sizeof(nan_publish_fields_header), &end_ptr);

	if (hal_req->tx_match_filter_len)
		slsi_mlme_nan_append_tlv(req, cpu_to_le16 (SLSI_FAPI_NAN_TX_MATCH_FILTER),
					 cpu_to_le16 (hal_req->tx_match_filter_len), hal_req->tx_match_filter,
					 &header_ptr, sizeof(nan_publish_fields_header), &end_ptr);

	/* update len */
	header_ptr[1] = end_ptr - header_ptr - 2;
}

int slsi_mlme_nan_publish(struct slsi_dev *sdev, struct net_device *dev, struct slsi_hal_nan_publish_req *hal_req,
			  u16 publish_id)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_sdf_flags = 0;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");
	if (hal_req) {
		/* max possible length for publish attributes : 8*255 */
		req = fapi_alloc(mlme_nan_publish_req, MLME_NAN_PUBLISH_REQ, ndev_vif->ifnum, 8 * 255);
		if (!req) {
			SLSI_NET_ERR(dev, "fapi alloc failure\n");
			return -ENOMEM;
		}

		/* Set/Enable corresponding bits to disable any indications
		 * that follow a publish.
		 * BIT0 - Disable publish termination indication.
		 * BIT1 - Disable match expired indication.
		 * BIT2 - Disable followUp indication received (OTA).
		 */
		if (hal_req->recv_indication_cfg & BIT(0))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_PUBLISH_END_EVENT;
		if (hal_req->recv_indication_cfg & BIT(1))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_MATCH_EXPIRED_EVENT;
		if (hal_req->recv_indication_cfg & BIT(2))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_RECEIVED_FOLLOWUP_EVENT;
	} else {
		req = fapi_alloc(mlme_nan_publish_req, MLME_NAN_PUBLISH_REQ, ndev_vif->ifnum, 0);
		if (!req) {
			SLSI_NET_ERR(dev, "fapi alloc failure\n");
			return -ENOMEM;
		}
	}

	fapi_set_u16(req, u.mlme_nan_publish_req.publish_id, publish_id);
	fapi_set_u16(req, u.mlme_nan_publish_req.nan_sdf_flags, nan_sdf_flags);

	if (hal_req)
		slsi_mlme_nan_publish_fapi_data(req, hal_req);

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_PUBLISH_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_nan_publish_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NAN_PUBLISH_CFM(result:0x%04x) ERROR\n",
			     fapi_get_u16(cfm, u.mlme_host_state_cfm.result_code));
		r = -EINVAL;
	}

	if (hal_req && !r)
		ndev_vif->nan.publish_id_map |= BIT(publish_id);
	else
		ndev_vif->nan.publish_id_map &= ~BIT(publish_id);
	slsi_kfree_skb(cfm);
	return r;
}

static void slsi_mlme_nan_subscribe_fapi_data(struct sk_buff *req, struct slsi_hal_nan_subscribe_req *hal_req)
{
	u8  nan_subscribe_fields_header[] = {0xdd, 0x00, 0x00, 0x16, 0x32, 0x0b, 0x03};
	u8 *header_ptr, *end_ptr;
	__le16 le16val;
	u32 binding_mask = 0;

	header_ptr = fapi_append_data(req, nan_subscribe_fields_header, sizeof(nan_subscribe_fields_header));
	le16val = cpu_to_le16(hal_req->ttl);
	fapi_append_data(req, (u8 *)&le16val, 2);
	le16val = cpu_to_le16(hal_req->period);
	fapi_append_data(req, (u8 *)&le16val, 2);
	fapi_append_data(req, &hal_req->subscribe_type, 1);
	fapi_append_data(req, &hal_req->service_response_filter, 1);
	fapi_append_data(req, &hal_req->service_response_include, 1);
	fapi_append_data(req, &hal_req->use_service_response_filter, 1);
	fapi_append_data(req, &hal_req->ssi_required_for_match_indication, 1);
	fapi_append_data(req, &hal_req->subscribe_match_indicator, 1);
	fapi_append_data(req, &hal_req->subscribe_count, 1);
	fapi_append_data(req, &hal_req->rssi_threshold_flag, 1);
	end_ptr = fapi_append_data(req, (u8 *)&binding_mask, 4);
	end_ptr += 4;

	if (hal_req->service_name_len)
		slsi_mlme_nan_append_tlv(req, cpu_to_le16 (SLSI_FAPI_NAN_SERVICE_NAME),
					 cpu_to_le16 (hal_req->service_name_len), hal_req->service_name, &header_ptr,
					 sizeof(nan_subscribe_fields_header), &end_ptr);

	if (hal_req->service_specific_info_len)
		slsi_mlme_nan_append_tlv(req, cpu_to_le16 (SLSI_FAPI_NAN_SERVICE_SPECIFIC_INFO),
					 cpu_to_le16 (hal_req->service_specific_info_len),
					 hal_req->service_specific_info, &header_ptr,
					 sizeof(nan_subscribe_fields_header), &end_ptr);

	if (hal_req->rx_match_filter_len)
		slsi_mlme_nan_append_tlv(req, cpu_to_le16 (SLSI_FAPI_NAN_RX_MATCH_FILTER),
					 cpu_to_le16 (hal_req->rx_match_filter_len), hal_req->rx_match_filter,
					 &header_ptr, sizeof(nan_subscribe_fields_header), &end_ptr);

	if (hal_req->tx_match_filter_len)
		slsi_mlme_nan_append_tlv(req, cpu_to_le16 (SLSI_FAPI_NAN_TX_MATCH_FILTER),
					 cpu_to_le16 (hal_req->tx_match_filter_len), hal_req->tx_match_filter,
					 &header_ptr, sizeof(nan_subscribe_fields_header), &end_ptr);

	/* update len */
	header_ptr[1] = end_ptr - header_ptr - 2;
}

int slsi_mlme_nan_subscribe(struct slsi_dev *sdev, struct net_device *dev, struct slsi_hal_nan_subscribe_req *hal_req,
			    u16 subscribe_id)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_sdf_flags = 0;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");
	if (hal_req) {
		/*max possible length for publish attributes: 8*255 */
		req = fapi_alloc(mlme_nan_subscribe_req, MLME_NAN_SUBSCRIBE_REQ, ndev_vif->ifnum, 8 * 255);
		if (!req) {
			SLSI_NET_ERR(dev, "fapi alloc failure\n");
			return -ENOMEM;
		}
		/* Set/Enable corresponding bits to disable
		 * indications that follow a subscribe.
		 * BIT0 - Disable subscribe termination indication.
		 * BIT1 - Disable match expired indication.
		 * BIT2 - Disable followUp indication received (OTA).
		 */
		if (hal_req->recv_indication_cfg & BIT(0))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_SUBSCRIBE_END_EVENT;
		if (hal_req->recv_indication_cfg & BIT(1))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_MATCH_EXPIRED_EVENT;
		if (hal_req->recv_indication_cfg & BIT(2))
			nan_sdf_flags |= FAPI_NANSDFCONTROL_RECEIVED_FOLLOWUP_EVENT;
	} else {
		req = fapi_alloc(mlme_nan_subscribe_req, MLME_NAN_SUBSCRIBE_REQ, ndev_vif->ifnum, 0);
		if (!req) {
			SLSI_NET_ERR(dev, "fapi alloc failure\n");
			return -ENOMEM;
		}
	}

	fapi_set_u16(req, u.mlme_nan_subscribe_req.subscribe_id, subscribe_id);
	fapi_set_u16(req, u.mlme_nan_subscribe_req.nan_sdf_flags, nan_sdf_flags);

	if (hal_req)
		slsi_mlme_nan_subscribe_fapi_data(req, hal_req);

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_SUBSCRIBE_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_nan_subscribe_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NAN_SUBSCRIBE_CFM(res:0x%04x)\n",
			     fapi_get_u16(cfm, u.mlme_host_state_cfm.result_code));
		r = -EINVAL;
	}

	if (hal_req && !r)
		ndev_vif->nan.subscribe_id_map |= BIT(subscribe_id);
	else
		ndev_vif->nan.subscribe_id_map &= ~BIT(subscribe_id);
	slsi_kfree_skb(cfm);
	return r;
}

static void slsi_mlme_nan_followup_fapi_data(struct sk_buff *req, struct slsi_hal_nan_transmit_followup_req *hal_req)
{
	u8  nan_followup_fields_header[] = {0xdd, 0x00, 0x00, 0x16, 0x32, 0x0b, 0x05};
	u8 *header_ptr, *end_ptr;

	header_ptr = fapi_append_data(req, nan_followup_fields_header, sizeof(nan_followup_fields_header));
	fapi_append_data(req, hal_req->addr, ETH_ALEN);
	fapi_append_data(req, &hal_req->priority, 1);
	end_ptr = fapi_append_data(req, &hal_req->dw_or_faw, 1);
	end_ptr += 1;

	if (hal_req->service_specific_info_len)
		slsi_mlme_nan_append_tlv(req, cpu_to_le16 (SLSI_FAPI_NAN_SERVICE_SPECIFIC_INFO),
					 cpu_to_le16 (hal_req->service_specific_info_len),
					 hal_req->service_specific_info, &header_ptr,
					 sizeof(nan_followup_fields_header), &end_ptr);

	/* update len */
	header_ptr[1] = end_ptr - header_ptr - 2;
}

int slsi_mlme_nan_tx_followup(struct slsi_dev *sdev, struct net_device *dev,
			      struct slsi_hal_nan_transmit_followup_req *hal_req)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_sdf_flags = 0;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");

	/* max possible length for publish attributes: 5*255 */
	req = fapi_alloc(mlme_nan_followup_req, MLME_NAN_FOLLOWUP_REQ, ndev_vif->ifnum, 5 * 255);
	if (!req) {
		SLSI_NET_ERR(dev, "fapi alloc failure\n");
		return -ENOMEM;
	}

	/* Set/Enable corresponding bits to disable responses after followUp.
	 * BIT0 - Disable followUp response from FW.
	 */
	if (hal_req->recv_indication_cfg & BIT(0))
		nan_sdf_flags |= FAPI_NANSDFCONTROL_DISABLE_RESPONSES_AFTER_FOLLOWUP;

	fapi_set_u16(req, u.mlme_nan_followup_req.requestor_instance_id, hal_req->publish_subscribe_id);
	fapi_set_u16(req, u.mlme_nan_followup_req.requestor_instance_id, hal_req->requestor_instance_id);
	fapi_set_u16(req, u.mlme_nan_subscribe_req.nan_sdf_flags, nan_sdf_flags);

	slsi_mlme_nan_followup_fapi_data(req, hal_req);

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_FOLLOWUP_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_nan_followup_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NAN_FOLLOWUP_CFM(res:0x%04x)\n",
			     fapi_get_u16(cfm, u.mlme_host_state_cfm.result_code));
		r = -EINVAL;
	}

	slsi_kfree_skb(cfm);
	return r;
}

static void slsi_mlme_nan_config_fapi_data(struct sk_buff *req, struct slsi_hal_nan_config_req *hal_req)
{
	u8  nan_config_fields_header[] = {0xdd, 0x00, 0x00, 0x16, 0x32, 0x0b, 0x01};
	u8 *header_ptr;
	u16 attribute;
	u8  scan_param[] = {0, 0, 0};
	int len = 0;

	header_ptr = fapi_append_data(req, nan_config_fields_header, sizeof(nan_config_fields_header));
	attribute = SLSI_FAPI_NAN_CONFIG_PARAM_MASTER_PREFERENCE;
	SLSI_FAPI_NAN_ATTRIBUTE_PUT_U16(req, attribute, hal_req->master_pref);
	len += sizeof(nan_config_fields_header) + 5;

	if (hal_req->config_sid_beacon) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_SID_BEACON;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->sid_beacon);
		len += 5;
	}

	if (hal_req->config_rssi_proximity) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_2_4_RSSI_PROXIMITY;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_proximity);
		len += 5;
	}

	if (hal_req->config_5g_rssi_close_proximity) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_5_RSSI_PROXIMITY;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_close_proximity_5g_val);
		len += 5;
	}

	if (hal_req->config_rssi_window_size) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_RSSI_WINDOW_SIZE;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_window_size_val);
		len += 5;
	}

	if (hal_req->config_scan_params) {
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_SCAN_PARAMETER_2_4;
		scan_param[0] = hal_req->scan_params_val.dwell_time[0];
		scan_param[1] = hal_req->scan_params_val.scan_period[0] & 0x00FF;
		scan_param[2] = (hal_req->scan_params_val.scan_period[0] & 0xFF00) >> 8;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_DATA(req, attribute, scan_param, 3);
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_SCAN_PARAMETER_5;
		scan_param[0] = hal_req->scan_params_val.dwell_time[1];
		scan_param[1] = hal_req->scan_params_val.scan_period[1] & 0x00FF;
		scan_param[2] = (hal_req->scan_params_val.scan_period[1] & 0xFF00) >> 8;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_DATA(req, attribute, scan_param, 3);
		len += 7 * 2;
	}

	if (hal_req->config_conn_capability) {
		u8 con_cap = 0;

		if (hal_req->conn_capability_val.is_wfd_supported)
			con_cap |= BIT(0);
		if (hal_req->conn_capability_val.is_wfds_supported)
			con_cap |= BIT(1);
		if (hal_req->conn_capability_val.is_tdls_supported)
			con_cap |= BIT(2);
		if (hal_req->conn_capability_val.wlan_infra_field)
			con_cap |= BIT(3);
		attribute = SLSI_FAPI_NAN_CONFIG_PARAM_CONNECTION_CAPAB;
		SLSI_FAPI_NAN_ATTRIBUTE_PUT_U8(req, attribute, hal_req->rssi_window_size_val);
		len += 5;
	}
	/* update len */
	header_ptr[1] = len - 2;
}

int slsi_mlme_nan_set_config(struct slsi_dev *sdev, struct net_device *dev, struct slsi_hal_nan_config_req *hal_req)
{
	struct netdev_vif *ndev_vif = netdev_priv(dev);
	struct sk_buff    *req;
	struct sk_buff    *cfm;
	int               r = 0;
	u16               nan_oper_ctrl = 0;

	SLSI_NET_DBG3(dev, SLSI_MLME, "\n");
	/* max possible length for publish attributes 5*255 */
	req = fapi_alloc(mlme_nan_config_req, MLME_NAN_CONFIG_REQ, ndev_vif->ifnum, 5 * 255);
	if (!req) {
		SLSI_NET_ERR(dev, "fapi alloc failure\n");
		return -ENOMEM;
	}

	if (hal_req->config_cluster_attribute_val)
		nan_oper_ctrl |= FAPI_NANOPERATIONCONTROL_CLUSTER_SDF;
	nan_oper_ctrl |= FAPI_NANOPERATIONCONTROL_MAC_ADDRESS_EVENT | FAPI_NANOPERATIONCONTROL_START_CLUSTER_EVENT |
			FAPI_NANOPERATIONCONTROL_JOINED_CLUSTER_EVENT;
	fapi_set_u16(req, u.mlme_nan_config_req.nan_operation_control_flags, nan_oper_ctrl);

	slsi_mlme_nan_config_fapi_data(req, hal_req);

	cfm = slsi_mlme_req_cfm(sdev, dev, req, MLME_NAN_FOLLOWUP_CFM);
	if (!cfm)
		return -EIO;

	if (fapi_get_u16(cfm, u.mlme_nan_followup_cfm.result_code) != FAPI_RESULTCODE_SUCCESS) {
		SLSI_NET_ERR(dev, "MLME_NAN_FOLLOWUP_CFM(res:0x%04x)\n",
			     fapi_get_u16(cfm, u.mlme_host_state_cfm.result_code));
		r = -EINVAL;
	}

	slsi_kfree_skb(cfm);
	return r;
}
