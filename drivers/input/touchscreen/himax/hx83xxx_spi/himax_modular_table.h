/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for modularize functions
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef __HIMAX_MODULAR_TABLE_H__
#define __HIMAX_MODULAR_TABLE_H__

#include "himax_ic_core.h"

#define TO_STR(VAR)	#VAR

enum modular_table {
	MODULE_NOT_FOUND = -1,
	MODULE_FOUND,
	MODULE_EMPTY,
};

#ifdef HX_USE_KSYM
#define DECLARE(sym)	struct himax_chip_entry sym; \
			EXPORT_SYMBOL(sym)
static const char * const himax_ksym_lookup[] = {
	#if defined(HX_MOD_KSYM_HX852xG)
	TO_STR(HX_MOD_KSYM_HX852xG),
	#endif
	#if defined(HX_MOD_KSYM_HX852xH)
	TO_STR(HX_MOD_KSYM_HX852xH),
	#endif
	#if defined(HX_MOD_KSYM_HX83102)
	TO_STR(HX_MOD_KSYM_HX83102),
	#endif
	#if defined(HX_MOD_KSYM_HX83103)
	TO_STR(HX_MOD_KSYM_HX83103),
	#endif
	#if defined(HX_MOD_KSYM_HX83106)
	TO_STR(HX_MOD_KSYM_HX83106),
	#endif
	#if defined(HX_MOD_KSYM_HX83108)
	TO_STR(HX_MOD_KSYM_HX83108),
	#endif
	#if defined(HX_MOD_KSYM_HX83111)
	TO_STR(HX_MOD_KSYM_HX83111),
	#endif
	#if defined(HX_MOD_KSYM_HX83112)
	TO_STR(HX_MOD_KSYM_HX83112),
	#endif
	#if defined(HX_MOD_KSYM_HX83113)
	TO_STR(HX_MOD_KSYM_HX83113),
	#endif
	#if defined(HX_MOD_KSYM_HX83191)
	TO_STR(HX_MOD_KSYM_HX83191),
	#endif
	#if defined(HX_MOD_KSYM_HX83121)
	TO_STR(HX_MOD_KSYM_HX83121),
	#endif
	#if defined(HX_MOD_KSYM_HX83122)
	TO_STR(HX_MOD_KSYM_HX83122),
	#endif
	NULL
};
#else
#define DECLARE(sym)
extern struct himax_chip_entry himax_ksym_lookup;
#endif

#endif
