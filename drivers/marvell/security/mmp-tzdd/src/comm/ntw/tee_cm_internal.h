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

#ifndef _TEE_CM_INTERNAL_H_
#define _TEE_CM_INTERNAL_H_

#include "tee_client_api.h"
#include "osa.h"
#include "tee_msgm_ntw.h"

#define CM_BLK_MAGIC    (0xC1C2C3C4)
#define CM_PAD_CHAR     (0)

/* read/write index header */
typedef struct {
	int32_t idx_read;	/* res pool read index */
	int32_t idx_write;	/* req pool write index */
} cm_index_header_t;

/* smi trap arguments */
typedef struct {
	uint32_t arg0;
	uint32_t arg1;
	uint32_t arg2;
} tee_cm_smi_input_t;

#endif /* _TEE_CM_INTERNAL_H_ */
