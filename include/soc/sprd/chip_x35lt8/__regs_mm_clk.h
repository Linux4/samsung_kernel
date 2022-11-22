/* the head file modifier:     g   2015-03-19 15:48:44*/

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
#error  "Don't include this file directly, Pls include sci_glb_regs.h"
#endif 


#ifndef __H_REGS_MM_CLK_RF_HEADFILE_H__
#define __H_REGS_MM_CLK_RF_HEADFILE_H__

#define REGS_MM_CLK

/* registers definitions for controller REGS_MM_CLK */
#define REG_MM_CLK_MM_AHB_CFG           SCI_ADDR(REGS_MM_CLK_BASE, 0x0020)
#define REG_MM_CLK_SENSOR_CFG           SCI_ADDR(REGS_MM_CLK_BASE, 0x0024)
#define REG_MM_CLK_CCIR_CFG             SCI_ADDR(REGS_MM_CLK_BASE, 0x0028)
#define REG_MM_CLK_DCAM_CFG             SCI_ADDR(REGS_MM_CLK_BASE, 0x002c)
#define REG_MM_CLK_VSP_CFG              SCI_ADDR(REGS_MM_CLK_BASE, 0x0030)
#define REG_MM_CLK_ISP_CFG              SCI_ADDR(REGS_MM_CLK_BASE, 0x0034)
#define REG_MM_CLK_JPG_CFG              SCI_ADDR(REGS_MM_CLK_BASE, 0x0038)
#define REG_MM_CLK_MIPI_CSI_CFG         SCI_ADDR(REGS_MM_CLK_BASE, 0x003c)



/* bits definitions for register REG_MM_AHB_RF_clk_mm_ahb_cfg */
#define BITS_CLK_MM_AHB_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_MM_AHB_RF_clk_sensor_cfg */
#define BITS_CLK_SENSOR_DIV(_X_)				( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_CLK_SENSOR_SEL(_X_)				( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_MM_AHB_RF_clk_ccir_cfg */
#define BIT_CLK_CCIR_PAD_SEL					( BIT(16) )
#define BIT_CLK_CCIR_SEL					( BIT(0) )

/* bits definitions for register REG_MM_AHB_RF_clk_dcam_cfg */
#define BITS_CLK_DCAM_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_MM_AHB_RF_clk_vsp_cfg */
#define BITS_CLK_VSP_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_MM_AHB_RF_clk_isp_cfg */
#define BITS_CLK_ISP_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_MM_AHB_RF_clk_jpg_cfg */
#define BITS_CLK_JPG_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_MM_AHB_RF_clk_vpp_cfg */
#define BITS_CLK_VPP_SEL(_X_)					( (_X_) & (BIT(0)|BIT(1)) )

#endif
