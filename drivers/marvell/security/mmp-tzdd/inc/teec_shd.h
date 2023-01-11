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

#ifndef __TEEC_SHD_H__
#define __TEEC_SHD_H__

#define TEEC_SHD_MAGIC                (0x4E734864) /* NsHd */

#define TEEC_SHD_TYPE_INV             (0x00000000)
#define TEEC_SHD_TYPE_INPUT           (0x00000001)
#define TEEC_SHD_TYPE_DISPLAY         (0x00000002)
#define TEEC_SHD_TYPE_GPIO            (0x00000004)
#define TEEC_SHD_TYPE_MISC            (0x00000008)

#define TEEC_SHD_MAX_NAME_LENGTH      (10)

typedef struct _teec_shd_t teec_shd_t;
typedef struct _teec_shd_t
{
	uint32_t            magic;
	uint32_t            ver;
	int8_t              *name;    /* should same as driver in TW and less than TEEC_SHD_MAX_NAME_LENGTH*/
	uint32_t            type;
	uint32_t            id;

	int32_t             (*init)(teec_shd_t *, void **, uint32_t *); /* called when register */
	void                (*cleanup)(teec_shd_t *); /* called when unregister */

	int32_t             (*sharing)(teec_shd_t *, void *); /* called when TW attach */
	int32_t             (*revert)(teec_shd_t *, void *);  /* called when TW detach */

	void                *priv;
} teec_shd_t;

int32_t teec_register_shd(teec_shd_t *shd);
int32_t teec_unregister_shd(teec_shd_t *shd);

#endif /* __TEEC_SHD_H__ */
