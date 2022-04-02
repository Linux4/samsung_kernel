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
 * uint64_t exynos_hvc(uint32_t cmd,
 *			uint64_t arg1,
 *			uint64_t arg2,
 *			uint64_t arg3,
 *			uint64_t arg4)
 */

ENTRY(exynos_hvc)
	dsb	sy
	hvc	#0
	ret
ENDPROC(exynos_hvc)
