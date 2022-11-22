/*
 * Copyright (C) 2018 Samsung Electronics.
 *
 * For Hypervisor Call(HVC)
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>

/*
 * Function signature:
 * unsigned long exynos_hvc(unsigned long cmd,
 *				unsigned long arg1,
 *				unsigned long arg2,
 *				unsigned long arg3,
 *				unsigned long arg4)
 */

ENTRY(exynos_hvc)
	dsb	sy
	hvc	#0
	ret
ENDPROC(exynos_hvc)
