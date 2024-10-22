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
 * DOC: contains ll_lt_sap structure definitions specific to the bearer
 * switch functionalities
 */

#ifndef _WLAN_LL_LT_SAP_BEARER_SWITCH_PUBLIC_STRUCTS_H_
#define _WLAN_LL_LT_SAP_BEARER_SWITCH_PUBLIC_STRUCTS_H_

#include "wlan_objmgr_psoc_obj.h"
#include <qdf_status.h>

/* Indicates MAX bearer switch requesters at a time */
#define MAX_BEARER_SWITCH_REQUESTERS 5
#define BS_REQ_ID_INVALID 0xFFFFFFFF
typedef uint32_t wlan_bs_req_id;

/**
 * enum bearer_switch_req_type: Bearer switch request type
 * @WLAN_BS_REQ_TO_NON_WLAN: Bearer switch request to non-wlan
 * @WLAN_BS_REQ_TO_WLAN: Bearer switch request to wlan
 * @WLAN_BS_REQ_INVALID: Invalid bearer switch request
 */
enum bearer_switch_req_type {
	WLAN_BS_REQ_TO_NON_WLAN = 0,
	WLAN_BS_REQ_TO_WLAN = 1,
	WLAN_BS_REQ_INVALID,
};

/**
 * enum bearer_switch_status: Bearer switch status
 * @WLAN_BS_STATUS_REJECTED: Bearer switch request rejected
 * @WLAN_BS_STATUS_COMPLETED: Bearer switch request completed
 * @WLAN_BS_STATUS_INVALID: Invalid bearer switch request
 */
enum bearer_switch_status {
	WLAN_BS_STATUS_REJECTED = 0,
	WLAN_BS_STATUS_COMPLETED = 1,
	WLAN_BS_STATUS_INVALID,
};

/**
 * enum bearer_switch_req_source: Bearer switch requester source
 * @BEARER_SWITCH_REQ_CONNECT: Bearer switch requester is connect
 * @BEARER_SWITCH_REQ_CSA: Bearer switch requester is CSA
 * @BEARER_SWITCH_REQ_FW: Bearer switch requester is FW
 * @BEARER_SWITCH_REQ_MAX: Indicates MAX bearer switch requester
 */
enum bearer_switch_req_source {
	BEARER_SWITCH_REQ_CONNECT,
	BEARER_SWITCH_REQ_CSA,
	BEARER_SWITCH_REQ_FW,
	BEARER_SWITCH_REQ_MAX,
};

 /**
 * typedef bearer_switch_requester_cb() - Callback function, which will
 * be invoked with the bearer switch request status.
 * @psoc: Psoc pointer
 * @vdev_id: vdev id of the requester
 * @request_id: Request ID
 * @status: Status of the bearer switch request
 * @req_value: Request value for the bearer switch request
 * @request_params: Request params for the bearer switch request
 *
 * Return: None
 */
typedef void (*bearer_switch_requester_cb)(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id,
					   wlan_bs_req_id request_id,
					   QDF_STATUS status,
					   uint32_t req_value,
					   void *request_params);

/**
 * struct wlan_bearer_switch_request - Data structure to store the bearer switch
 * request
 * @vdev_id: Vdev id of the requester
 * @request_id: Unique value to identify the request
 * @req_type: Type of the request,  bearer switch to wlan or bearer switch
 * to non-wlan
 * @source: Bearer switch request source
 * @requester_cb: Callback which needs to be invoked to indicate the status of
 * the request to the requester, this callback will be passed by the requester
 * when requester will send the request.
 * @arg_value: argument value passed by requester, this will be returned back
 * in the callback
 * @arg: Argument passed by requester, this will be returned back in the
 * callback
 */
struct wlan_bearer_switch_request {
	uint8_t vdev_id;
	wlan_bs_req_id request_id;
	enum bearer_switch_req_type req_type;
	enum bearer_switch_req_source source;
	bearer_switch_requester_cb requester_cb;
	uint32_t arg_value;
	void *arg;
};

/**
 * struct ll_sap_ops - ll_sap osif callbacks
 * @ll_sap_send_audio_transport_switch_req_cb: Send audio transport request to
 * userspace
 */
struct ll_sap_ops {
		void (*ll_sap_send_audio_transport_switch_req_cb)(
					struct wlan_objmgr_vdev *vdev,
					enum bearer_switch_req_type req_type);
};

#endif /* _WLAN_LL_LT_SAP_BEARER_SWITCH_PUBLIC_STRUCTS_H_ */
