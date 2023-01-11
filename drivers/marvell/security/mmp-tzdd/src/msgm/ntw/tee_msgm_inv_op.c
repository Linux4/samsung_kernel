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
#include "tee_msgm_op_internal.h"

tee_stat_t set_cmd_inv_op(tee_msgm_t msgm, void *arg)
{
	inv_op_body *dst = NULL;
	tee_set_cmd_inv_op_arg_t *src = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	src = (tee_set_cmd_inv_op_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	dst = (inv_op_body *) ntw_handle->body;
	dst->srv_cmd = src->srv_cmd;
	dst->ss_tw = src->ss;
	return TEEC_SUCCESS;
}

tee_stat_t get_cmd_inv_op(tee_msgm_t msgm, void *arg)
{
	inv_op_body *src = NULL;
	tee_get_cmd_inv_op_arg_t *dst = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	dst = (tee_get_cmd_inv_op_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	src = (inv_op_body *) ntw_handle->body;
	dst->ss = src->ss_tw;
	dst->srv_cmd = src->srv_cmd;
	return TEEC_SUCCESS;
}

tee_stat_t set_ret_inv_op(tee_msgm_t msgm, void *arg)
{
	inv_op_body *dst = NULL;
	tee_set_ret_inv_op_arg_t *src = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	src = (tee_set_ret_inv_op_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	dst = (inv_op_body *) ntw_handle->body;
	dst->ret_orig = src->ret_orig;
	dst->ret = src->ret;
	return TEEC_SUCCESS;
}

tee_stat_t get_ret_inv_op(tee_msgm_t msgm, void *arg)
{
	inv_op_body *src = NULL;
	tee_get_ret_inv_op_arg_t *dst = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	dst = (tee_get_ret_inv_op_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	src = (inv_op_body *) ntw_handle->body;
	dst->ret_orig = src->ret_orig;
	dst->ret = src->ret;
	return TEEC_SUCCESS;
}

bool cmd_class_inv_op(tee_cmd_t cmd)
{
	return (TEE_CMD_INV_OP == cmd);
}

const tee_msgm_op_class inv_op_class = {
	.set_cmd = set_cmd_inv_op,
	.get_cmd = get_cmd_inv_op,
	.set_ret = set_ret_inv_op,
	.get_ret = get_ret_inv_op,
	.cmd_class = cmd_class_inv_op,
};
