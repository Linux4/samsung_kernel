/*
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - Stage 2 Protection Unit(S2MPU)
 * Author: Junho Choi <junhosj.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_S2MPU_H__
#define __EXYNOS_S2MPU_H__

#include <linux/hvc.h>

#define HVC_FID_SET_S2MPU			(HVC_FID_BASE + 0x100)
#define HVC_FID_SET_S2MPU_FOR_WIFI		(HVC_FID_BASE + 0x100 + 0x1)

#define EXYNOS_MAX_S2MPU_INSTANCE		(64)
#define EXYNOS_MAX_S2MPU_NAME_LEN		(10)

#define S2MPU_VID_SHIFT				(16)

/* Error */
#define ERROR_INVALID_S2MPU_NAME		(0x1)
#define ERROR_DO_NOT_SUPPORT_S2MPU		(0x2)
#define ERROR_S2MPU_NOT_INITIALIZED		(0xE12E0001)

#ifndef __ASSEMBLY__
/* S2MPU access permission */
enum stage2_ap {
	ATTR_NO_ACCESS = 0x0,
	ATTR_RO = 0x1,
	ATTR_WO = 0x2,
	ATTR_RW = 0x3
};

uint64_t exynos_set_dev_stage2_pcie_ap(const char *s2mpu_name,
					uint32_t vid,
					uint64_t base,
					uint64_t size,
					uint32_t ap);
uint64_t exynos_set_dev_stage2_ap(const char *s2mpu_name,
				  uint32_t vid,
				  uint64_t base,
				  uint64_t size,
				  uint32_t ap);
#endif	/* __ASSEMBLY__ */
#endif	/* __EXYNOS_S2MPU_H__ */
