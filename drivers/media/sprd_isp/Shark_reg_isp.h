/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _TIGER_REG_ISP_H_
#define _TIGER_REG_ISP_H_
#include <mach/globalregs.h>

#ifdef   __cplusplus
extern   "C"
{
#endif

#define BIT_0                                          0x01
#define BIT_1                                          0x02
#define BIT_2                                          0x04
#define BIT_3                                          0x08
#define BIT_4                                          0x10
#define BIT_5                                          0x20
#define BIT_6                                          0x40
#define BIT_7                                          0x80
#define BIT_8                                          0x0100
#define BIT_9                                          0x0200
#define BIT_10                                         0x0400
#define BIT_11                                         0x0800
#define BIT_12                                         0x1000
#define BIT_13                                         0x2000
#define BIT_14                                         0x4000
#define BIT_15                                         0x8000
#define BIT_16                                         0x010000
#define BIT_17                                         0x020000
#define BIT_18                                         0x040000
#define BIT_19                                         0x080000
#define BIT_20                                         0x100000
#define BIT_21                                         0x200000
#define BIT_22                                         0x400000
#define BIT_23                                         0x800000
#define BIT_24                                         0x01000000
#define BIT_25                                         0x02000000
#define BIT_26                                         0x04000000
#define BIT_27                                         0x08000000
#define BIT_28                                         0x10000000
#define BIT_29                                         0x20000000
#define BIT_30                                         0x40000000
#define BIT_31                                         0x80000000


#define ISP_DCAM_BASE		SPRD_DCAM_BASE
#define ISP_DCAM_INT_STS		(ISP_DCAM_BASE + 0x003CUL)
#define ISP_DCAM_INT_CLR		(ISP_DCAM_BASE + 0x0044UL)

#define ISP_BASE_ADDR		SPRD_ISP_BASE
#define ISP_LNC_STATUS		(ISP_BASE_ADDR+0x0200UL)
#define ISP_LNC_LOAD		(ISP_BASE_ADDR+0x0220UL)
#define ISP_AXI_MASTER		(ISP_BASE_ADDR+0x2000UL)
#define ISP_INT_RAW		(ISP_BASE_ADDR+0x2080UL)
#define ISP_INT_STATUS		(ISP_BASE_ADDR+0x2084UL)
#define ISP_INT_EN		(ISP_BASE_ADDR+0x2078UL)
#define ISP_INT_CLEAR		(ISP_BASE_ADDR+0x207cUL)
#define ISP_REG_MAX_SIZE		SPRD_ISP_SIZE
#define ISP_AXI_MASTER_STOP	(ISP_BASE_ADDR + 0X2054UL)

#define ISP_AHB_BASE		SPRD_MMAHB_BASE
#define ISP_MODULE_EB		(ISP_AHB_BASE + 0x0000UL)
#define ISP_MODULE_RESET	(ISP_AHB_BASE + 0x0004UL)
#define ISP_CORE_CLK_EB		(ISP_AHB_BASE + 0x0008UL)

#define ISP_CLOCK_BASE		SPRD_MMCKG_BASE
#define ISP_CLOCK			(ISP_CLOCK_BASE + 0X34)


/*irq line number in system*/
#define ISP_IRQ				IRQ_ISP_INT

#define ISP_EB_BIT		BIT_2
#define ISP_RST_LOG_BIT	BIT_2
#define ISP_RST_CFG_BIT	BIT_3		
#define ISP_CORE_CLK_EB_BIT	BIT_4
#define ISP_AXI_MASTER_STOP_BIT BIT_0

#define ISP_IRQ_HW_MASK_V0001 (0x1fffffff)
#define ISP_IRQ_NUM_V0001 (29)
#define ISP_TMP_BUF_SIZE_MAX_V0001 (32 * 1024)

#ifdef __cplusplus
}
#endif

#endif //_TIGER_REG_ISP_H_
