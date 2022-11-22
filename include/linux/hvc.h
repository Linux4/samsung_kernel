/*
 *  Copyright (c) 2019 Samsung Electronics.
 *
 * A header for Hypervisor Call(HVC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __HVC_H__
#define __HVC_H__

/* For Power Management */
#define HVC_CMD_BASE			(0xC6000000)

#ifndef	__ASSEMBLY__
extern unsigned long exynos_hvc(unsigned long cmd,
				unsigned long arg1,
				unsigned long arg2,
				unsigned long arg3,
				unsigned long arg4);
#endif	/* __ASSEMBLY__ */
#endif	/* __HVC_H__ */
