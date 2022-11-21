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

tee_stat_t set_cmd_unmap_shm(tee_msgm_t msgm, void *arg)
{
	unmap_shm_body *dst = NULL;
	tee_set_cmd_unmap_shm_arg_t *src = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;
	_TEEC_MRVL_SharedMemory *_teec_mrvl_sharedMem;

	src = (tee_set_cmd_unmap_shm_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	dst = (unmap_shm_body *) ntw_handle->body;

	_teec_mrvl_sharedMem = (_TEEC_MRVL_SharedMemory *) (src->imp);

	dst->vaddr_tw = _teec_mrvl_sharedMem->vaddr_tw;
	dst->sz = _teec_mrvl_sharedMem->size;
	return TEEC_SUCCESS;
}

tee_stat_t get_cmd_unmap_shm(tee_msgm_t msgm, void *arg)
{
	unmap_shm_body *src = NULL;
	tee_get_cmd_unmap_shm_arg_t *dst = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	dst = (tee_get_cmd_unmap_shm_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	src = (unmap_shm_body *) ntw_handle->body;
	dst->vaddr_tw = src->vaddr_tw;
	return TEEC_SUCCESS;
}

bool cmd_class_unmap_shm(tee_cmd_t cmd)
{
	return (TEE_CMD_UNMAP_SHM == cmd);
}

const tee_msgm_op_class unmap_shm_class = {
	.set_cmd = set_cmd_unmap_shm,
	.get_cmd = get_cmd_unmap_shm,
	.set_ret = NULL,
	.get_ret = NULL,
	.cmd_class = cmd_class_unmap_shm,
};
