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
#include "tee_cm_internal.h"
#include "tee_cm.h"

extern void *in_header;
extern void *out_header;

static int32_t _g_idx_out_write;

/* get in read idx's pointer */
inline int32_t *_cm_in_read_idx(void)
{
	static int32_t index_in_read;

	return &index_in_read;
}

/* get in write idx's pointer(the other end modify) */
inline int32_t *_cm_in_write_idx(void)
{
	cm_index_header_t *in_head = (cm_index_header_t *) in_header;

	return &(in_head->idx_write);
}

/* get out read idx's pointer(the other and modify */
inline int32_t *_cm_out_read_idx(void)
{
	cm_index_header_t *out_head = (cm_index_header_t *) out_header;

	return &(out_head->idx_read);
}

/* get out write idx's pointer */
inline int32_t *_cm_out_write_idx(void)
{
	static int32_t _g_idx_out_write;

	return &_g_idx_out_write;
}

inline int32_t _cm_get_out_write_idx(void)
{
	return _g_idx_out_write;
}

inline void _cm_update_out_write_idx(int32_t idx)
{
	cm_index_header_t *out_head = (cm_index_header_t *) out_header;

	_g_idx_out_write = idx;
	out_head->idx_write = idx;

	return;
}
