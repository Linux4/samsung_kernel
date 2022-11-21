/*
 * Copyright (c) [2009-2013] Marvell International Ltd. and its affiliates.
 * All rights reserved.
 * This software file (the "File") is owned and distributed by Marvell
 * International Ltd. and/or its affiliates ("Marvell") under the following
 * licensing terms.
 * If you received this File from Marvell, you may opt to use, redistribute
 * and/or modify this File in accordance with the terms and conditions of
 * the General Public License Version 2, June 1991 (the "GPL License"), a
 * copy of which is available along with the File in the license.txt file
 * or by writing to the Free Software Foundation, Inc., 59 Temple Place,
 * Suite 330, Boston, MA 02111-1307 or on the worldwide web at
 * http://www.gnu.org/licenses/gpl.txt. THE FILE IS DISTRIBUTED AS-IS,
 * WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this
 * warranty disclaimer.
 */

#ifndef __TEEC_CB_H__
#define __TEEC_CB_H__

#include "teec_types.h"
#include "tee_client_api.h"
#include "tee_cb_common.h"

/* alias */
typedef TEEC_UUID   teec_cb_uuid_t;
typedef TEEC_Result teec_cb_stat_t;

/* context in non-trust world */
typedef void        *teec_cb_cntx_t;
/* callback handle */
typedef void        *teec_cb_handle_t;

/* callback function type */
typedef teec_cb_stat_t (*teec_cb_fn_t)(tee_cb_arg_t *, teec_cb_cntx_t);

/* the function should be called before tee_cb_call_ntw is invoked. */
teec_cb_handle_t teec_reg_cb(const teec_cb_uuid_t *uuid,
                    teec_cb_fn_t func, teec_cb_cntx_t cntx);
/* once unregistering, the uuid call will be invalid. */
void teec_unreg_cb(teec_cb_handle_t handle);

#endif /* __TEEC_CB_H__ */
