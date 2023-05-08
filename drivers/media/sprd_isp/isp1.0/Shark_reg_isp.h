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
#ifndef CONFIG_64BIT
#include <soc/sprd/globalregs.h>
#else
#include <soc/sprd/irqs.h>
#endif

#ifdef   __cplusplus
extern   "C"
{
#endif




#define ISP_DCAM_BASE		DCAM_BASE
#define ISP_DCAM_INT_STS		(ISP_DCAM_BASE + 0x003CUL)
#define ISP_DCAM_INT_CLR		(ISP_DCAM_BASE + 0x0044UL)

#define ISP_BASE_ADDR		ISP_BASE
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
