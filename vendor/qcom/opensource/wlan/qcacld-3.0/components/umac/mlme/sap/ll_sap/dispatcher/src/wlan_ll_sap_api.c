/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "wlan_ll_sap_api.h"
#include <../../core/src/wlan_ll_lt_sap_bearer_switch.h>
#include <../../core/src/wlan_ll_lt_sap_main.h>
#include "wlan_cm_api.h"
#include "wlan_policy_mgr_ll_sap.h"
#include "wlan_policy_mgr_api.h"

wlan_bs_req_id
wlan_ll_lt_sap_bearer_switch_get_id(struct wlan_objmgr_psoc *psoc)
{
	return ll_lt_sap_bearer_switch_get_id(psoc);
}

QDF_STATUS
wlan_ll_lt_sap_switch_bearer_to_ble(struct wlan_objmgr_psoc *psoc,
				struct wlan_bearer_switch_request *bs_request)
{
	return ll_lt_sap_switch_bearer_to_ble(psoc, bs_request);
}

static void
connect_start_bearer_switch_requester_cb(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id,
					 wlan_bs_req_id request_id,
					 QDF_STATUS status, uint32_t req_value,
					 void *request_params)
{
	wlan_cm_id cm_id = req_value;

	wlan_cm_bearer_switch_resp(psoc, vdev_id, cm_id, status);
}

QDF_STATUS
wlan_ll_sap_switch_bearer_on_sta_connect_start(struct wlan_objmgr_psoc *psoc,
					       qdf_list_t *scan_list,
					       uint8_t vdev_id,
					       wlan_cm_id cm_id)
{
	struct scan_cache_node *scan_node = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	uint32_t ch_freq = 0;
	struct scan_cache_entry *entry;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_bearer_switch_request bs_request = {0};
	qdf_freq_t ll_lt_sap_freq;
	bool is_bearer_switch_required = false;
	QDF_STATUS status = QDF_STATUS_E_ALREADY;
	uint8_t vdev_id;

	vdev_id = wlan_policy_mgr_get_ll_lt_sap_vdev(psoc);
	/* LL_LT SAP is not present, bearer switch is not required */
	if (vdev_id == WLAN_INVALID_VDEV_ID)
		return status;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LL_SAP_ID);
	if (!vdev)
		return status;

	if (!scan_list || !qdf_list_size(scan_list))
		goto rel_ref;

	ll_lt_sap_freq = policy_mgr_get_lt_ll_sap_freq(psoc);
	qdf_list_peek_front(scan_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(scan_list, cur_node, &next_node);

		scan_node = qdf_container_of(cur_node, struct scan_cache_node,
					     node);
		entry = scan_node->entry;
		ch_freq = entry->channel.chan_freq;

		/*
		 * Switch the bearer in case of SCC/MCC for LL_LT SAP
		 */
		if (policy_mgr_2_freq_always_on_same_mac(psoc, ch_freq,
							 ll_lt_sap_freq)) {
			ll_sap_debug("Scan list has BSS of freq %d on same mac with ll_lt sap %d",
				     ch_freq, ll_lt_sap_freq);
			is_bearer_switch_required = true;
			break;
		}

		ch_freq = 0;
		cur_node = next_node;
		next_node = NULL;
	}

	if (!is_bearer_switch_required)
		goto rel_ref;

	bs_request.vdev_id = vdev_id;
	bs_request.request_id = ll_lt_sap_bearer_switch_get_id(psoc);
	bs_request.req_type = WLAN_BS_REQ_TO_NON_WLAN;
	bs_request.source = BEARER_SWITCH_REQ_CONNECT;
	bs_request.requester_cb = connect_start_bearer_switch_requester_cb;
	bs_request.arg_value = cm_id;

	status = ll_lt_sap_switch_bearer_to_ble(psoc, &bs_request);

	if (QDF_IS_STATUS_ERROR(status))
		status = QDF_STATUS_E_ALREADY;
rel_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LL_SAP_ID);

	return status;
}

static void connect_complete_bearer_switch_requester_cb(
						struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id,
						wlan_bs_req_id request_id,
						QDF_STATUS status,
						uint32_t req_value,
						void *request_params)
{
	/* Drop this response as no action is required */
}

QDF_STATUS wlan_ll_sap_switch_bearer_on_sta_connect_complete(
						struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id)
{
	struct wlan_bearer_switch_request bs_request = {0};
	QDF_STATUS status;

	bs_request.vdev_id = vdev_id;
	bs_request.request_id = ll_lt_sap_bearer_switch_get_id(psoc);
	bs_request.req_type = WLAN_BS_REQ_TO_WLAN;
	bs_request.source = BEARER_SWITCH_REQ_CONNECT;
	bs_request.requester_cb = connect_complete_bearer_switch_requester_cb;

	status = ll_lt_sap_switch_bearer_to_wlan(psoc, &bs_request);

	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_ALREADY;

	return QDF_STATUS_SUCCESS;
}
