/* the head file modifier:     g   2015-03-26 15:29:45*/

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


#ifndef __H_REGS_AVS_APB_HEADFILE_H__
#define __H_REGS_AVS_APB_HEADFILE_H__ __FILE__



#define REG_AVS_APB_AVS_TUNE_LMT_CFG          SCI_ADDR(REGS_AVS_APB_BASE, 0x0000 )
#define REG_AVS_APB_AVS_TUNE_STATUS           SCI_ADDR(REGS_AVS_APB_BASE, 0x0004 )
#define REG_AVS_APB_AVS_CFG                   SCI_ADDR(REGS_AVS_APB_BASE, 0x0008 )
#define REG_AVS_APB_AVS_TUNE_STEP_CFG         SCI_ADDR(REGS_AVS_APB_BASE, 0x000C )
#define REG_AVS_APB_AVS_WAIT_CFG              SCI_ADDR(REGS_AVS_APB_BASE, 0x0010 )
#define REG_AVS_APB_AVS_INT_CFG               SCI_ADDR(REGS_AVS_APB_BASE, 0x0014 )
#define REG_AVS_APB_AVS_START_SINGLE_ACT      SCI_ADDR(REGS_AVS_APB_BASE, 0x0018 )
#define REG_AVS_APB_AVS_START_REPEAT_ACT      SCI_ADDR(REGS_AVS_APB_BASE, 0x001C )
#define REG_AVS_APB_AVS_HPM_EN                SCI_ADDR(REGS_AVS_APB_BASE, 0x0020 )
#define REG_AVS_APB_AVS_HPM_RPT_ANLS          SCI_ADDR(REGS_AVS_APB_BASE, 0x0024 )
#define REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_0  SCI_ADDR(REGS_AVS_APB_BASE, 0x0028 )
#define REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_1  SCI_ADDR(REGS_AVS_APB_BASE, 0x002C )
#define REG_AVS_APB_AVS_FSM_STS               SCI_ADDR(REGS_AVS_APB_BASE, 0x0030 )

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_TUNE_LMT_CFG
// Register Offset : 0x0000
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_VOLT_TUNE_VAL_MAX(x)                        (((x) & 0xFF) << 8)
#define BIT_AVS_APB_VOLT_TUNE_VAL_MIN(x)                        (((x) & 0xFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_TUNE_STATUS
// Register Offset : 0x0004
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_VOLT_TUNE_VAL(x)                            (((x) & 0xFF) << 8)
#define BIT_AVS_APB_VOLT_OBS_VAL(x)                             (((x) & 0xFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_CFG
// Register Offset : 0x0008
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_HPM1_RPT_CHKSUM_ERR_EN                      BIT(17)
#define BIT_AVS_APB_HPM0_RPT_CHKSUM_ERR_EN                      BIT(16)
#define BIT_AVS_APB_PAUSE_OCCUR_ERR_EN                          BIT(8)
#define BIT_AVS_APB_VOLT_TUNE_FORBID_EN                         BIT(4)
#define BIT_AVS_APB_VOLT_OBS_FORBID_EN                          BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_TUNE_STEP_CFG
// Register Offset : 0x000C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_VOLT_TUNE_DOWN_STEP(x)                      (((x) & 0x1FF) << 16)
#define BIT_AVS_APB_VOLT_TUNE_UP_STEP(x)                        (((x) & 0x1FF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_WAIT_CFG
// Register Offset : 0x0010
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_RND_INTVAL_WAIT_NUM(x)                      (((x) & 0xFFFF) << 16)
#define BIT_AVS_APB_VOLT_STB_WAIT_NUM(x)                        (((x) & 0xFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_INT_CFG
// Register Offset : 0x0014
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_AVS_TUNE_DONE_INT_MASK_STATUS               BIT(13)
#define BIT_AVS_APB_AVS_TUNE_ERR_INT_MASK_STATUS                BIT(12)
#define BIT_AVS_APB_AVS_TUNE_DONE_INT_RAW_STATUS                BIT(9)
#define BIT_AVS_APB_AVS_TUNE_ERR_INT_RAW_STATUS                 BIT(8)
#define BIT_AVS_APB_AVS_TUNE_DONE_INT_CLR                       BIT(5)
#define BIT_AVS_APB_AVS_TUNE_ERR_INT_CLR                        BIT(4)
#define BIT_AVS_APB_AVS_TUNE_DONE_INT_EN                        BIT(1)
#define BIT_AVS_APB_AVS_TUNE_ERR_INT_EN                         BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_START_SINGLE_ACT
// Register Offset : 0x0018
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_AVS_START_SINGLE_ACT                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_START_REPEAT_ACT
// Register Offset : 0x001C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_AVS_START_REPEAT_ACT                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_HPM_EN
// Register Offset : 0x0020
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_HPM1_EN                                     BIT(1)
#define BIT_AVS_APB_HPM0_EN                                     BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_HPM_RPT_ANLS
// Register Offset : 0x0024
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_HPM_RPT_TUNE_DWN_MARK(x)                    (((x) & 0xFFFF) << 16)
#define BIT_AVS_APB_HPM_RPT_TUNE_UP_MARK(x)                     (((x) & 0xFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_0
// Register Offset : 0x0028
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_HPM1_OSC_RPT_VLD(x)                         (((x) & 0xFFFF) << 16)
#define BIT_AVS_APB_HPM0_OSC_RPT_VLD(x)                         (((x) & 0xFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_1
// Register Offset : 0x002C
// Description     : 
---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------
// Register Name   : REG_AVS_APB_AVS_FSM_STS
// Register Offset : 0x0030
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AVS_APB_AVS_FSM_STS(x)                              (((x) & 0xF))

#endif
