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

#ifndef _TZDD_IOCTL_H_
#define _TZDD_IOCTL_H_

#include "tee_client_api.h"

#define OPERATION_EXIST        (1)
#define OPERATION_NON_EXIST    (0)

typedef struct {
	const char *name;
	tee_impl tee_ctx_ntw;
	uint32_t ret;
} teec_init_context_args;

typedef struct {
	tee_impl tee_ctx_ntw;
	uint32_t ret;
} teec_final_context_args;

/* allocate and register use the same structure */
typedef struct {
	tee_impl tee_ctx_ntw;
	void *buffer;
	size_t size;
	uint32_t flags;
	tee_impl tee_shm_ntw;
	uint32_t ret;
} teec_map_shared_mem_args;

typedef struct {
	void *buffer;
	size_t size;
	uint32_t flags;
	tee_impl tee_shm_ntw;
	uint32_t ret;
} teec_unmap_shared_mem_args;

typedef struct {
	tee_impl tee_ctx_ntw;
	tee_impl tee_ss_ntw;
	const TEEC_UUID *destination;
	uint32_t connectionMethod;
	const void *connectionData;
	/* if the openSS includes the TEE_Operation,
	 * the flag should be set to 1.
	*  if flag=1, that means driver need to allomate tee_op_ntw
	*/
	uint32_t flags;
	uint32_t paramTypes;
	TEEC_Parameter params[4];
	tee_impl tee_op_ntw;
	tee_impl *tee_op_ntw_for_cancel;
	uint32_t returnOrigin;
	uint32_t ret;
} teec_open_ss_args;

typedef struct {
	tee_impl tee_ss_ntw;
	uint32_t ret;
} teec_close_ss_args;

typedef struct {
	tee_impl tee_ss_ntw;
	uint32_t cmd_ID;
	uint32_t started;
	/* if flag=1, that means driver need to allomate tee_op_ntw */
	uint32_t flags;
	uint32_t paramTypes;
	TEEC_Parameter params[4];
	tee_impl tee_op_ntw;
	tee_impl *tee_op_ntw_for_cancel;
	uint32_t returnOrigin;
	uint32_t ret;
} teec_invoke_cmd_args;

typedef struct {
	tee_impl tee_op_ntw;
	uint32_t ret;
} teec_request_cancel_args;

#define TEEC_IOCTL_MAGIC        't'

#define TEEC_INIT_CONTEXT       _IOW(TEEC_IOCTL_MAGIC, 0, unsigned int)

#define TEEC_FINAL_CONTEXT      _IOW(TEEC_IOCTL_MAGIC, 1, unsigned int)

#define TEEC_REGIST_SHARED_MEM  _IOW(TEEC_IOCTL_MAGIC, 2, unsigned int)

#define TEEC_ALLOC_SHARED_MEM   _IOW(TEEC_IOCTL_MAGIC, 3, unsigned int)

#define TEEC_RELEASE_SHARED_MEM _IOW(TEEC_IOCTL_MAGIC, 4, unsigned int)

#define TEEC_OPEN_SS            _IOW(TEEC_IOCTL_MAGIC, 5, unsigned int)

#define TEEC_CLOSE_SS           _IOW(TEEC_IOCTL_MAGIC, 6, unsigned int)

#define TEEC_INVOKE_CMD         _IOW(TEEC_IOCTL_MAGIC, 7, unsigned int)

#define TEEC_REQUEST_CANCEL     _IOW(TEEC_IOCTL_MAGIC, 8, unsigned int)

#endif /* _TZDD_IOCTL_H_ */
