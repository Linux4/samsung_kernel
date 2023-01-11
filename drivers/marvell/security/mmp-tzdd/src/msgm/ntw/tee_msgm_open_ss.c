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
#include "tee_client_api.h"
#define LOGIN_METHOD_NULL  (0)
#define LOGIN_METHOD_VALID (sizeof(TEEC_UUID))
#include "tee_msgm_op_internal.h"

/*
 * set cmd
 * */
tee_stat_t set_cmd_open_ss(tee_msgm_t msgm, void *arg)
{
	open_ss_body *dst = NULL;
	tee_set_cmd_open_ss_arg_t *src = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	src = (tee_set_cmd_open_ss_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	dst = (open_ss_body *) ntw_handle->body;
	dst->cntx = src->cntx;
	memcpy(&(dst->uuid), (uint8_t *) src->uuid, sizeof(TEEC_UUID));
	dst->meth = src->meth;
	dst->data_sz = LOGIN_METHOD_VALID;
	dst->data = src->data;
	return TEEC_SUCCESS;
}

/*
 * set cmd
 * */
tee_stat_t get_cmd_open_ss(tee_msgm_t msgm, void *arg)
{
	open_ss_body *src = NULL;
	tee_get_cmd_open_ss_arg_t *dst = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	dst = (tee_get_cmd_open_ss_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	src = (open_ss_body *) ntw_handle->body;
	dst->uuid = &(src->uuid);
	dst->meth = src->meth;
	dst->data_sz = src->data_sz;
	dst->data = src->data;
	return TEEC_SUCCESS;
}

/*
 * set ret
 * */
tee_stat_t set_ret_open_ss(tee_msgm_t msgm, void *arg)
{
	open_ss_body *dst = NULL;
	tee_set_ret_open_ss_arg_t *src = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	src = (tee_set_ret_open_ss_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	dst = (open_ss_body *) ntw_handle->body;
	dst->ss_tw = src->ss_tw;
	dst->ret_orig = src->ret_orig;
	dst->ret = src->ret;
	return TEEC_SUCCESS;
}

/*
 * get ret
 * */
tee_stat_t get_ret_open_ss(tee_msgm_t msgm, void *arg)
{
	open_ss_body *src = NULL;
	tee_get_ret_open_ss_arg_t *dst = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	dst = (tee_get_ret_open_ss_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	src = (open_ss_body *) ntw_handle->body;
	dst->ss_tw = src->ss_tw;
	dst->ret_orig = src->ret_orig;
	dst->ret = src->ret;
	return TEEC_SUCCESS;
}

bool cmd_class_open_ss(tee_cmd_t cmd)
{
	return (TEE_CMD_OPEN_SS == cmd);
}

const tee_msgm_op_class open_ss_class = {
	.set_cmd = set_cmd_open_ss,
	.get_cmd = get_cmd_open_ss,
	.set_ret = set_ret_open_ss,
	.get_ret = get_ret_open_ss,
	.cmd_class = cmd_class_open_ss,
};
