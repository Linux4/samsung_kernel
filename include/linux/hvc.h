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

/* HVC FID */
#define HVC_FID_BASE                           (0xC6000000)

#define HVC_FID_SET_EL2_CRASH_INFO_FP_BUF      (HVC_FID_BASE + 0x40)

#define HVC_CMD_S2MPUFD_BASE		(0x600)
#define HVC_CMD_INIT_S2MPUFD		(HVC_FID_BASE | HVC_CMD_S2MPUFD_BASE | 0x1)
#define HVC_CMD_GET_S2MPUFD_FAIL_INFO	(HVC_FID_BASE | HVC_CMD_S2MPUFD_BASE | 0x2)
#ifndef	__ASSEMBLY__
#include <linux/types.h>
extern uint64_t exynos_hvc(uint32_t cmd,
			   uint64_t arg1,
			   uint64_t arg2,
			   uint64_t arg3,
			   uint64_t arg4);
#endif	/* __ASSEMBLY__ */
#endif	/* __HVC_H__ */
