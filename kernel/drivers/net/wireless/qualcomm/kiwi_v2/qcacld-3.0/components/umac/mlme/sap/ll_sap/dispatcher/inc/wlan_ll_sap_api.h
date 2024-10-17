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

/**
 * DOC: contains ll_lt_sap API definitions specific to the bearer
 * switch functionalities
 */

#ifndef _WLAN_LL_LT_SAP_API_H_
#define _WLAN_LL_LT_SAP_API_H_

#include <wlan_cmn.h>
#include <wlan_objmgr_vdev_obj.h>
#include "wlan_ll_sap_public_structs.h"
#include "wlan_cm_public_struct.h"

#ifdef WLAN_FEATURE_LL_LT_SAP
/**
 * wlan_ll_lt_sap_bearer_switch_get_id() - Get the request id for bearer switch
 * request
 * @psoc: Pointer to psoc
 * Return: Bearer switch request id
 */
wlan_bs_req_id
wlan_ll_lt_sap_bearer_switch_get_id(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_ll_lt_sap_switch_bearer_to_ble() - Switch audio transport to BLE
 * @psoc: Pointer to psoc
 * @bs_request: Pointer to bearer switch request
 * Return: QDF_STATUS_SUCCESS on successful bearer switch else failure
 */
QDF_STATUS wlan_ll_lt_sap_switch_bearer_to_ble(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_bearer_switch_request *bs_request);

/**
 * wlan_ll_sap_switch_bearer_on_sta_connect_start() - Switch bearer during
 * station connection start
 * @psoc: Pointer to psoc
 * @scan_list: Pointer to the candidate list
 * @vdev_id: Vdev id of the requesting vdev
 * @cm_id: connection manager id of the current connect request
 * Return: QDF_STATUS_SUCCESS on successful bearer switch
 *         QDF_STATUS_E_ALREADY, if bearer switch is not required
 *         else failure
 */
QDF_STATUS wlan_ll_sap_switch_bearer_on_sta_connect_start(
						struct wlan_objmgr_psoc *psoc,
						qdf_list_t *scan_list,
						uint8_t vdev_id,
						wlan_cm_id cm_id);

/**
 * wlan_ll_sap_switch_bearer_on_sta_connect_complete() - Switch bearer during
 * station connection complete
 * @psoc: Pointer to psoc
 * @vdev_id: Vdev id of the requesting vdev
 * Return: QDF_STATUS_SUCCESS on successful bearer switch
 *         QDF_STATUS_E_ALREADY, if bearer switch is not required
 *         else failure
 */
QDF_STATUS wlan_ll_sap_switch_bearer_on_sta_connect_complete(
						struct wlan_objmgr_psoc *psoc,
						uint8_t vdev_id);

#else

static inline wlan_bs_req_id
wlan_ll_lt_sap_bearer_switch_get_id(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline QDF_STATUS
wlan_ll_lt_sap_switch_bearer_to_ble(
				struct wlan_objmgr_psoc *psoc,
				struct wlan_bearer_switch_request *bs_request)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
wlan_ll_sap_switch_bearer_on_sta_connect_start(struct wlan_objmgr_psoc *psoc,
					       qdf_list_t *scan_list,
					       uint8_t vdev_id,
					       wlan_cm_id cm_id)

{
	return QDF_STATUS_E_ALREADY;
}

static inline QDF_STATUS
wlan_ll_sap_switch_bearer_on_sta_connect_complete(struct wlan_objmgr_psoc *psoc,
						  uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_LL_LT_SAP */
#endif /* _WLAN_LL_LT_SAP_API_H_ */
