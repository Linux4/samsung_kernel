/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_XEN_BE_H_
#define _MC_XEN_BE_H_

#include <linux/version.h>

#ifdef CONFIG_XEN
char *xen_be_vm_id(void);
int xen_be_init(void);
void xen_be_exit(void);
#else
static inline char *xen_be_vm_id(void)
{
	/* Can't be a Dom0 if Xen is not enabled, so this is safe */
	return ERR_PTR(-EINVAL);
}

static inline int xen_be_init(void)
{
	return 0;
}

static inline void xen_be_exit(void)
{
}
#endif

#endif /* _MC_XEN_BE_H_ */
