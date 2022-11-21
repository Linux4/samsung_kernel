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

#include <osa.h>

#ifdef OSA_DEBUG

uint32_t osa_dbg_level = DBG_ALL;

#else

uint32_t osa_dbg_level = DBG_LOG;

#endif

uint32_t osa_dbg_print(uint32_t level, const void *format, ...)
{
	va_list args;
	uint32_t ret = 0;

	if (level >= osa_dbg_level) {
		va_start(args, format);
		ret = vprintk(format, args);
		va_end(args);
	}

	return ret;
}
OSA_EXPORT_SYMBOL(osa_dbg_print);

void osa_dbg_bt(void)
{
#ifdef OSA_DEBUG

	dump_stack();

#endif /* OSA_DEBUG */
}
OSA_EXPORT_SYMBOL(osa_dbg_bt);
