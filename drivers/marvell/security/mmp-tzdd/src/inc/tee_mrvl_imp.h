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
#ifndef _TEE_MRVL_IMP_H_
#define _TEE_MRVL_IMP_H_

#include "tzdd_types.h"
#include "osa.h"
#include "tzdd_list.h"

typedef void *tee_ss_ntw;
typedef void *tee_msg_handle_t;

#define   MGC_NUM   (4)

typedef struct __tee_ctx_ntw {
	uint8_t magic[MGC_NUM];
	/* add the count if there is a session or sharedmemory */
	osa_atomic_t count;
	tee_impl tee_ctx_ntw;
	tee_impl private_data;
} _TEEC_MRVL_Context;

typedef struct __tee_ss_ntw {
	uint8_t magic[MGC_NUM];
	/* add the count if there is an operation */
	osa_atomic_t count;
	tee_impl tee_ctx_ntw;
	tee_impl tee_ss_tw;
	tee_impl tee_ss_ntw;
	tee_impl private_data;
} _TEEC_MRVL_Session;

typedef struct __tee_op_ntw {
	uint8_t magic[MGC_NUM];
	tee_ss_ntw tee_ss_ntw;
	tee_msg_handle_t tee_msg_ntw;
	tee_impl tee_op_ntw;
} _TEEC_MRVL_Operation;

typedef struct __tee_shm_ntw {
	uint8_t magic[MGC_NUM];
	/* add the count if there is a memref in the operation */
	osa_atomic_t count;
	tee_impl tee_ctx_ntw;
	void *vaddr_tw;
	uint32_t size;
	/* who allocates the mem, if flag=1,
	* it means driver allocate the buffer
	*/
	uint32_t flag;
	tee_impl tee_shm_ntw;
	tee_impl private_data;
} _TEEC_MRVL_SharedMemory;

#endif /* _TEE_MRVL_IMP_H_ */
