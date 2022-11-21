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

tee_stat_t set_cmd_close_ss(tee_msgm_t msgm, void *arg)
{
	close_ss_body *dst = NULL;
	tee_set_cmd_close_ss_arg_t src = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	src = (tee_set_cmd_close_ss_arg_t) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	dst = (close_ss_body *) ntw_handle->body;
	dst->ss_tw = src;
	return TEEC_SUCCESS;
}

tee_stat_t get_cmd_close_ss(tee_msgm_t msgm, void *arg)
{
	close_ss_body *src = NULL;
	tee_get_cmd_close_ss_arg_t *dst = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	dst = (tee_get_cmd_close_ss_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	src = (close_ss_body *) ntw_handle->body;
	dst->ss = src->ss_tw;
	return TEEC_SUCCESS;
}

bool cmd_class_close_ss(tee_cmd_t cmd)
{
	return (TEE_CMD_CLOSE_SS == cmd);
}

const tee_msgm_op_class close_ss_class = {
	.set_cmd = set_cmd_close_ss,
	.get_cmd = get_cmd_close_ss,
	.set_ret = NULL,
	.get_ret = NULL,
	.cmd_class = cmd_class_close_ss,
};
