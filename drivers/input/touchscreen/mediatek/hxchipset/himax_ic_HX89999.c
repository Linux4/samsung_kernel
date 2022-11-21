// SPDX-License-Identifier: GPL-2.0
/*
 *  Himax Android Driver Sample Code for HX89999 chipset
 *
 *  Copyright (C) 2018 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2, as published by the Free Software Foundation, and
 *  may be copied, distributed, and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_common.h"
extern void himax_trigger_platform_probe(void);

int g_hx89999_refcnt;
EXPORT_SYMBOL(g_hx89999_refcnt);

static int __init himax_hx89999_init(void)
{
	I("%s\n", __func__);
	if (g_hx89999_refcnt++ > 0) {
		I("hx89999 has already been loaded, ignoring....\n");
		return 0;
	}
#ifdef __HIMAX_MODULIZE__
	himax_trigger_platform_probe();
#endif
	return 0;
}

static void __exit himax_hx89999_exit(void)
{
	I("%s\n", __func__);
}

module_init(himax_hx89999_init);
module_exit(himax_hx89999_exit);

MODULE_DESCRIPTION("HIMAX HX89999 touch driver");
MODULE_LICENSE("GPL");

