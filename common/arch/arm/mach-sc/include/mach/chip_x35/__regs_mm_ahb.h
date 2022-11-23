/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *************************************************
 * Automatically generated C header: do not edit *
 *************************************************
 */

#ifndef __REGS_MM_AHB_H__
#define __REGS_MM_AHB_H__

#ifndef __SCI_GLB_REGS_H__
#error  "Don't include this file directly, include <mach/sci_glb_regs.h>"
#endif

#define REGS_MM_AHB

/* registers definitions for controller REGS_MM_AHB */
#define REG_MM_AHB_AHB_EB               SCI_ADDR(REGS_MM_AHB_BASE, 0x0000)
#define REG_MM_AHB_AHB_RST              SCI_ADDR(REGS_MM_AHB_BASE, 0x0004)
#define REG_MM_AHB_GEN_CKG_CFG          SCI_ADDR(REGS_MM_AHB_BASE, 0x0008)

/* bits definitions for register REG_MM_AHB_AHB_EB */
#define BIT_MM_CKG_EB                   ( BIT(6) )
#define BIT_JPG_EB                      ( BIT(5) )
#define BIT_CSI_EB                      ( BIT(4) )
#define BIT_VSP_EB                      ( BIT(3) )
#define BIT_ISP_EB                      ( BIT(2) )
#define BIT_CCIR_EB                     ( BIT(1) )
#define BIT_DCAM_EB                     ( BIT(0) )

/* bits definitions for register REG_MM_AHB_AHB_RST */
#define BIT_MM_CKG_SOFT_RST             ( BIT(13) )
#define BIT_MM_MTX_SOFT_RST             ( BIT(12) )
#define BIT_OR1200_SOFT_RST             ( BIT(11) )
#define BIT_ROT_SOFT_RST                ( BIT(10) )
#define BIT_CAM2_SOFT_RST               ( BIT(9) )
#define BIT_CAM1_SOFT_RST               ( BIT(8) )
#define BIT_CAM0_SOFT_RST               ( BIT(7) )
#define BIT_JPG_SOFT_RST                ( BIT(6) )
#define BIT_CSI_SOFT_RST                ( BIT(5) )
#define BIT_VSP_SOFT_RST                ( BIT(4) )
#define BIT_ISP_CFG_SOFT_RST            ( BIT(3) )
#define BIT_ISP_LOG_SOFT_RST            ( BIT(2) )
#define BIT_CCIR_SOFT_RST               ( BIT(1) )
#define BIT_DCAM_SOFT_RST               ( BIT(0) )

/* bits definitions for register REG_MM_AHB_GEN_CKG_CFG */
#define BIT_MM_MTX_AXI_CKG_EN           ( BIT(8) )
#define BIT_MM_AXI_CKG_EN               ( BIT(7) )
#define BIT_JPG_AXI_CKG_EN              ( BIT(6) )
#define BIT_VSP_AXI_CKG_EN              ( BIT(5) )
#define BIT_ISP_AXI_CKG_EN              ( BIT(4) )
#define BIT_DCAM_AXI_CKG_EN             ( BIT(3) )
#define BIT_SENSOR_CKG_EN               ( BIT(2) )
#define BIT_MIPI_CSI_CKG_EN             ( BIT(1) )
#define BIT_CPHY_CFG_CKG_EN             ( BIT(0) )

/* vars definitions for controller REGS_MM_AHB */

#endif //__REGS_MM_AHB_H__
