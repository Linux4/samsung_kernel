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

#ifndef CONFIG_MINI_TZDD
#include "tzdd_internal.h"
#else
#include "mini_tzdd_internal.h"
#endif

#include "tee_memm.h"
#include "tee_msgm_op_internal.h"

tee_stat_t set_cmd_map_shm(tee_msgm_t msgm, void *arg)
{
	uint32_t pages_num = 0;
	tee_memm_ss_t memm_handle = NULL;
	map_shm_body *ptr = NULL;
	tee_set_cmd_map_shm_arg_t *src = NULL;
	tee_stat_t ntw_stat = TEEC_SUCCESS;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	memm_handle = tee_memm_create_ss();
	src = (tee_set_cmd_map_shm_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	ptr = (map_shm_body *) ntw_handle->body;

	ntw_stat = tee_memm_set_phys_pages(
			   memm_handle,
			   src->shm->buffer, src->shm->size,
			   (tee_msgm_phys_memblock_t *)((uint8_t *)ptr +
			   sizeof(map_shm_body)), &pages_num);
	if (TEEC_SUCCESS != ntw_stat)
		return ntw_stat;

	tee_memm_destroy_ss(memm_handle);
	ptr->arr_sz = pages_num;
	ptr->flags = src->shm->flags;
	ptr->cntx = src->cntx;
	return TEEC_SUCCESS;
}

tee_stat_t get_cmd_map_shm(tee_msgm_t msgm, void *arg)
{
	map_shm_body *src = NULL;
	tee_get_cmd_map_shm_arg_t *dst = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	dst = (tee_get_cmd_map_shm_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	src = (map_shm_body *) ntw_handle->body;
	dst->arr_sz = src->arr_sz;
	dst->arr = (tee_msgm_phys_memblock_t(*)[])src->arr;
	return TEEC_SUCCESS;
}

tee_stat_t set_ret_map_shm(tee_msgm_t msgm, void *arg)
{
	map_shm_body *dst = NULL;
	tee_set_ret_map_shm_arg_t *src = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	src = (tee_set_ret_map_shm_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	dst = (map_shm_body *) ntw_handle->body;
	dst->vaddr_tw = src->vaddr_tw;
	dst->ret = src->ret;
	return TEEC_SUCCESS;
}

tee_stat_t get_ret_map_shm(tee_msgm_t msgm, void *arg)
{
	map_shm_body *src = NULL;
	tee_get_ret_map_shm_arg_t *dst = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	dst = (tee_get_ret_map_shm_arg_t *) arg;
	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	src = (map_shm_body *) ntw_handle->body;
	dst->vaddr_tw = src->vaddr_tw;
	dst->ret = src->ret;
	return TEEC_SUCCESS;
}

bool cmd_class_map_shm(tee_cmd_t cmd)
{
	return (TEE_CMD_MAP_SHM == cmd);
}

const tee_msgm_op_class map_shm_class = {
	.set_cmd = set_cmd_map_shm,
	.get_cmd = get_cmd_map_shm,
	.set_ret = set_ret_map_shm,
	.get_ret = get_ret_map_shm,
	.cmd_class = cmd_class_map_shm,
};
