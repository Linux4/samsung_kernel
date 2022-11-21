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

#ifndef _OSA_DBG_H_
#define _OSA_DBG_H_

#include "osa.h"

#define OSA_RM_WARNING(x)   ((void)(x))

/* no implemented in print proc, for osa_dbg_level only */
#define DBG_ALL         (0)

#define DBG_INFO        (1)
#define DBG_WARN        (2)
#define DBG_ERR         (3)

/* DBG LOG will be printed even in release version */
#define DBG_LOG         (0xFFFFFFFF)

#ifdef OSA_DEBUG

#define OSA_ASSERT(x) \
	{ \
		if (!(x)) { \
			osa_dbg_print(DBG_ERR, \
				"ASSERT FAILURE:\r\n");\
			osa_dbg_print(DBG_ERR, \
				"%s (%d): %s\r\n", \
				__FILE__, __LINE__, __func__); \
			osa_dbg_bt(); \
			while (1) \
				; \
		} \
	}
#else

#define OSA_ASSERT(x)

#endif

extern uint32_t osa_dbg_level;

extern uint32_t osa_dbg_print(uint32_t level, const void *format, ...);
extern void osa_dbg_bt(void);

#endif /* _OSA_DBG_H_ */
