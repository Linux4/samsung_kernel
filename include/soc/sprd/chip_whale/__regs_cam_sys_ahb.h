/* the head file modifier:     g   2015-03-26 15:20:42*/

/*
* Copyright (C) 2013 Spreadtrum Communications Inc.
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

#ifndef __SCI_GLB_REGS_H__
#error "Don't include this file directly, Pls include sci_glb_regs.h"
#endif


#ifndef __H_REGS_CAM_AHB_HEADFILE_H__
#define __H_REGS_CAM_AHB_HEADFILE_H__ __FILE__
#define  REGS_CAM_AHB_BASE SPRD_MMAHB_BASE

/* registers definitions for CAM_AHB */
#define REG_CAM_AHB_AHB_EB         SCI_ADDR(REGS_CAM_AHB_BASE, 0x0000 )
#define REG_CAM_AHB_AHB_RST        SCI_ADDR(REGS_CAM_AHB_BASE, 0x0004 )
#define REG_CAM_AHB_GEN_CKG_CFG    SCI_ADDR(REGS_CAM_AHB_BASE, 0x0008 )
#define REG_CAM_AHB_MIPI_CSI_CTRL  SCI_ADDR(REGS_CAM_AHB_BASE, 0x000C )
#define REG_CAM_AHB_MM_QOS_CFG0    SCI_ADDR(REGS_CAM_AHB_BASE, 0x0010 )
#define REG_CAM_AHB_MM_QOS_CFG1    SCI_ADDR(REGS_CAM_AHB_BASE, 0x0014 )

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_AHB_AHB_EB
// Register Offset : 0x0000
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_AHB_MMU_EB                              BIT(8)
#define BIT_CAM_AHB_CKG_EB                              BIT(7)
#define BIT_CAM_AHB_JPG1_EB                             BIT(6)
#define BIT_CAM_AHB_JPG0_EB                             BIT(5)
#define BIT_CAM_AHB_CSI1_EB                             BIT(4)
#define BIT_CAM_AHB_CSI0_EB                             BIT(3)
#define BIT_CAM_AHB_ISP_EB                              BIT(2)
#define BIT_CAM_AHB_DCAM1_EB                            BIT(1)
#define BIT_CAM_AHB_DCAM0_EB                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_AHB_AHB_RST
// Register Offset : 0x0004
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_AHB_CCIR_SOFT_RST                       BIT(18)
#define BIT_CAM_AHB_MMU_SOFT_RST                        BIT(17)
#define BIT_CAM_AHB_CKG_SOFT_RST                        BIT(16)
#define BIT_CAM_AHB_DCAM1_ROT_SOFT_RST                  BIT(15)
#define BIT_CAM_AHB_DCAM1_CAM2_SOFT_RST                 BIT(14)
#define BIT_CAM_AHB_DCAM1_CAM1_SOFT_RST                 BIT(13)
#define BIT_CAM_AHB_DCAM1_CAM0_SOFT_RST                 BIT(12)
#define BIT_CAM_AHB_DCAM0_ROT_SOFT_RST                  BIT(11)
#define BIT_CAM_AHB_DCAM0_CAM2_SOFT_RST                 BIT(10)
#define BIT_CAM_AHB_DCAM0_CAM1_SOFT_RST                 BIT(9)
#define BIT_CAM_AHB_DCAM0_CAM0_SOFT_RST                 BIT(8)
#define BIT_CAM_AHB_JPG1_SOFT_RST                       BIT(7)
#define BIT_CAM_AHB_JPG0_SOFT_RST                       BIT(6)
#define BIT_CAM_AHB_CSI1_SOFT_RST                       BIT(5)
#define BIT_CAM_AHB_CSI0_SOFT_RST                       BIT(4)
#define BIT_CAM_AHB_ISP_CFG_SOFT_RST                    BIT(3)
#define BIT_CAM_AHB_ISP_LOG_SOFT_RST                    BIT(2)
#define BIT_CAM_AHB_DCAM1_SOFT_RST                      BIT(1)
#define BIT_CAM_AHB_DCAM0_SOFT_RST                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_AHB_GEN_CKG_CFG
// Register Offset : 0x0008
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_AHB_ISP_AXI_CKG_EN                      BIT(10)
#define BIT_CAM_AHB_JPG1_AXI_CKG_EN                     BIT(9)
#define BIT_CAM_AHB_JPG0_AXI_CKG_EN                     BIT(8)
#define BIT_CAM_AHB_SENSOR1_CKG_EN                      BIT(7)
#define BIT_CAM_AHB_SENSOR0_CKG_EN                      BIT(6)
#define BIT_CAM_AHB_DCAM1_AXI_CKG_EN                    BIT(5)
#define BIT_CAM_AHB_DCAM0_AXI_CKG_EN                    BIT(4)
#define BIT_CAM_AHB_MIPI_CSI1_CKG_EN                    BIT(3)
#define BIT_CAM_AHB_CPHY1_CFG_CKG_EN                    BIT(2)
#define BIT_CAM_AHB_MIPI_CSI0_CKG_EN                    BIT(1)
#define BIT_CAM_AHB_CPHY0_CFG_CKG_EN                    BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_AHB_MIPI_CSI_CTRL
// Register Offset : 0x000C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_AHB_MIPI_CPHY1_SAMPLE_SEL(x)            (((x) & 0x3) << 11)
#define BIT_CAM_AHB_MIPI_CPHY1_SYNC_MODE                BIT(10)
#define BIT_CAM_AHB_MIPI_CPHY1_TEST_CTL                 BIT(9)
#define BIT_CAM_AHB_MIPI_CPHY1_SEL                      BIT(8)
#define BIT_CAM_AHB_MIPI_CPHY0_SAMPLE_SEL(x)            (((x) & 0x3) << 3)
#define BIT_CAM_AHB_MIPI_CPHY0_SYNC_MODE                BIT(2)
#define BIT_CAM_AHB_MIPI_CPHY0_TEST_CTL                 BIT(1)
#define BIT_CAM_AHB_MIPI_CPHY0_SEL                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_AHB_MM_QOS_CFG0
// Register Offset : 0x0010
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_AHB_QOS_R_DCAM1(x)                      (((x) & 0xF) << 28)
#define BIT_CAM_AHB_QOS_W_DCAM1(x)                      (((x) & 0xF) << 24)
#define BIT_CAM_AHB_QOS_R_DCAM0(x)                      (((x) & 0xF) << 20)
#define BIT_CAM_AHB_QOS_W_DCAM0(x)                      (((x) & 0xF) << 16)
#define BIT_CAM_AHB_QOS_R_JPG1(x)                       (((x) & 0xF) << 12)
#define BIT_CAM_AHB_QOS_W_JPG1(x)                       (((x) & 0xF) << 8)
#define BIT_CAM_AHB_QOS_R_JPG0(x)                       (((x) & 0xF) << 4)
#define BIT_CAM_AHB_QOS_W_JPG0(x)                       (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_CAM_AHB_MM_QOS_CFG1
// Register Offset : 0x0014
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_CAM_AHB_CAM_DDR_ADDR_BIT32                  BIT(16)
#define BIT_CAM_AHB_CAM_NIU_AR_QOS(x)                   (((x) & 0xF) << 12)
#define BIT_CAM_AHB_CAM_NIU_AW_QOS(x)                   (((x) & 0xF) << 8)
#define BIT_CAM_AHB_QOS_R_ISP(x)                        (((x) & 0xF) << 4)
#define BIT_CAM_AHB_QOS_W_ISP(x)                        (((x) & 0xF))

#endif
