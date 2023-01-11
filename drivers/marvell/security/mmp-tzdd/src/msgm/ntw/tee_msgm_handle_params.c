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
#define IS_TYPE_UNMATCH_PARENT_FLAG(_m, _n) (((_n) ^ (_m)) & (_n) & 0x3)
const char cmd_magic[] = "CmDm";
#include "tee_msgm_op_internal.h"

tee_stat_t set_params(tee_msgm_t msgm, tee_msg_op_info_t *arg)
{
	params_struct *dst = NULL;
	uint32_t index = 0, value = 0;
	uint8_t *last = NULL;
	tee_msgm_ntw_struct *ntw_handle = NULL;
	_TEEC_MRVL_SharedMemory *_teec_mrvl_sharedMem;

	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	dst = ntw_handle->ntw_params;
	if (!dst)
		return TEEC_SUCCESS;
	last = (uint8_t *) dst + sizeof(params_struct);
	dst->paramTypes = arg->paramTypes;

	for (index = 0; index < PARAM_NUMBERS; index++) {
		value = TEEC_PARAM_TYPE_GET(arg->paramTypes, index);
		if (IS_TYPE_NONE(value)) {
			memset(&(dst->param[index].value), 0,
			       sizeof(Union_Parameter));
			continue;
		}

		if (IS_TYPE_TMPREF(value)) {
			if (IS_PARAM_TAGED_INPUT(value)) {
				/* type of input */
				if (arg->params[index].tmpref.buffer) {
					memcpy(last,
					       arg->params[index].tmpref.buffer,
					       arg->params[index].tmpref.size);
				}
			} else {
				/* type of output, so clear it */
				memset(last, 0, arg->params[index].tmpref.size);
			}
			dst->param[index].tmpref.buffer =
			    (void *)(last -
				     ((uint8_t *) dst +
				      OFFSETOF(params_struct, param[index])));
			dst->param[index].tmpref.size =
			    arg->params[index].tmpref.size;
			last += dst->param[index].tmpref.size;
		} else if (IS_TYPE_MEMREF(value)) {
			/* for memref */
			if (TEEC_MEMREF_WHOLE == value) {
				_teec_mrvl_sharedMem =
					(_TEEC_MRVL_SharedMemory *)(arg->params[index].memref.parent->imp);
				dst->param[index].memref.buffer =
				    _teec_mrvl_sharedMem->vaddr_tw;
				dst->param[index].memref.size =
				    arg->params[index].memref.parent->size;
			} else {
				if (IS_TYPE_UNMATCH_PARENT_FLAG
				    (arg->params[index].memref.parent->flags,
				     value)) {
					TZDD_DBG("set_params: in/out un-compatible\n");
					return TEEC_ERROR_BAD_PARAMETERS;
				}
				OSA_ASSERT((arg->params[index].memref.offset +
					    arg->params[index].memref.size <=
					    arg->params[index].memref.parent->
					    size));

				_teec_mrvl_sharedMem = (_TEEC_MRVL_SharedMemory *)(arg->
                                         params[index].memref.parent->imp);

				dst->param[index].memref.buffer =
				    _teec_mrvl_sharedMem->vaddr_tw +
				    arg->params[index].memref.offset;
				dst->param[index].memref.size =
				    arg->params[index].memref.size;
			}
		} else if (IS_TYPE_VALUE(value)
			&& IS_PARAM_TAGED_INPUT(value)) {
			dst->param[index].value.a = arg->params[index].value.a;
			dst->param[index].value.b = arg->params[index].value.b;
		} else {
			TZDD_DBG("set_params: no input\n");
		}
	}
	return TEEC_SUCCESS;
}

/*
 * get temp_memref to tw/ntw
 * in ntw: set-> set offset and convert to abstract-addr
 *         get-> get buffer and convert to relative-addr
 * */
tee_stat_t update_params(tee_msgm_t msgm, tee_msg_op_info_t *arg)
{
	params_struct *src = NULL;
	uint32_t index = 0, value = 0;
	uint8_t *last = NULL, *start = NULL;
	TEEC_Result stat = TEEC_SUCCESS;
	tee_msgm_ntw_struct *ntw_handle = NULL;

	ntw_handle = (tee_msgm_ntw_struct *) msgm;
	src = ntw_handle->ntw_params;
	if (!src)
		return stat;

	start = (uint8_t *) src;
	arg->paramTypes = src->paramTypes;

	for (index = 0; index < PARAM_NUMBERS; index++) {
		value = TEEC_PARAM_TYPE_GET(src->paramTypes, index);
		if (IS_TYPE_NONE(value)
		    || (!IS_PARAM_TAGED_OUTPUT(value) && (0xc != value))) {
			continue;
		}

		if (IS_TYPE_TMPREF(value) || IS_TYPE_MEMREF(value)) {
			/* update size, update content if tmpref */
			if (src->param[index].memref.size >
			    arg->params[index].memref.size) {
				TZDD_DBG("Warning: short buffer\n");
			} else if (IS_TYPE_TMPREF(value)) {
				last =
				    start + OFFSETOF(params_struct,
						     param[index]) +
				    (ulong_t) src->param[index].memref.buffer;
				if (arg->params[index].tmpref.buffer) {
					memcpy(arg->params[index].tmpref.buffer,
					       last,
					       src->param[index].memref.size);
				}
			}
			arg->params[index].memref.size =
			    src->param[index].memref.size;
		} else if (IS_TYPE_VALUE(value)) {
			arg->params[index].value.a = src->param[index].value.a;
			arg->params[index].value.b = src->param[index].value.b;
		} else {
			TZDD_DBG("update_params: index is TEEC_NONE\n");
		}
	}
	return stat;
}
