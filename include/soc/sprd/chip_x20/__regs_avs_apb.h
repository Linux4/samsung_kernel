/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
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


#ifndef __H_REGS_AVS_APB_HEADFILE_H__
#define __H_REGS_AVS_APB_HEADFILE_H__ __FILE__

#define	REGS_AVS_APB

/* registers definitions for AVS_APB */
#define REG_AVS_APB_AVS_TUNE_LMT_CFG                      SCI_ADDR(REGS_AVS_APB_BASE, 0x0000)
#define REG_AVS_APB_AVS_TUNE_STATUS                       SCI_ADDR(REGS_AVS_APB_BASE, 0x0004)
#define REG_AVS_APB_AVS_CFG                               SCI_ADDR(REGS_AVS_APB_BASE, 0x0008)
#define REG_AVS_APB_AVS_TUNE_STEP_CFG                     SCI_ADDR(REGS_AVS_APB_BASE, 0x000C)
#define REG_AVS_APB_AVS_WAIT_CFG                          SCI_ADDR(REGS_AVS_APB_BASE, 0x0010)
#define REG_AVS_APB_AVS_INT_CFG                           SCI_ADDR(REGS_AVS_APB_BASE, 0x0014)
#define REG_AVS_APB_AVS_START_SINGLE_ACT                  SCI_ADDR(REGS_AVS_APB_BASE, 0x0018)
#define REG_AVS_APB_AVS_START_REPEAT_ACT                  SCI_ADDR(REGS_AVS_APB_BASE, 0x001C)
#define REG_AVS_APB_AVS_HPM_EN                            SCI_ADDR(REGS_AVS_APB_BASE, 0x0020)
#define REG_AVS_APB_AVS_HPM_RPT_ANLS                      SCI_ADDR(REGS_AVS_APB_BASE, 0x0024)
#define REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_0              SCI_ADDR(REGS_AVS_APB_BASE, 0x0028)
#define REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_1              SCI_ADDR(REGS_AVS_APB_BASE, 0x002C)
#define REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_2              SCI_ADDR(REGS_AVS_APB_BASE, 0x0030)
#define REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_3              SCI_ADDR(REGS_AVS_APB_BASE, 0x0034)
#define REG_AVS_APB_AVS_HPM_DLY_WIND_SEL                  SCI_ADDR(REGS_AVS_APB_BASE, 0x0038)
#define REG_AVS_APB_FSM_STS                               SCI_ADDR(REGS_AVS_APB_BASE, 0x0040)



/* bits definitions for register REG_AVS_APB_AVS_TUNE_LMT_CFG */
#define BITS_VOLT_TUNE_VAL_MAX(_X_)                       ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_VOLT_TUNE_VAL_MIN(_X_)                       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AVS_APB_AVS_TUNE_STATUS */
#define BITS_VOLT_TUNE_VAL(_X_)                           ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_VOLT_OBS_VAL(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AVS_APB_AVS_CFG */
#define BIT_HPM15_RPT_CHKSUM_ERR_EN                       ( BIT(31) )
#define BIT_HPM14_RPT_CHKSUM_ERR_EN                       ( BIT(30) )
#define BIT_HPM13_RPT_CHKSUM_ERR_EN                       ( BIT(29) )
#define BIT_HPM12_RPT_CHKSUM_ERR_EN                       ( BIT(28) )
#define BIT_HPM11_RPT_CHKSUM_ERR_EN                       ( BIT(27) )
#define BIT_HPM10_RPT_CHKSUM_ERR_EN                       ( BIT(26) )
#define BIT_HPM9_RPT_CHKSUM_ERR_EN                        ( BIT(25) )
#define BIT_HPM8_RPT_CHKSUM_ERR_EN                        ( BIT(24) )
#define BIT_HPM7_RPT_CHKSUM_ERR_EN                        ( BIT(23) )
#define BIT_HPM6_RPT_CHKSUM_ERR_EN                        ( BIT(22) )
#define BIT_HPM5_RPT_CHKSUM_ERR_EN                        ( BIT(21) )
#define BIT_HPM4_RPT_CHKSUM_ERR_EN                        ( BIT(20) )
#define BIT_HPM3_RPT_CHKSUM_ERR_EN                        ( BIT(19) )
#define BIT_HPM2_RPT_CHKSUM_ERR_EN                        ( BIT(18) )
#define BIT_HPM1_RPT_CHKSUM_ERR_EN                        ( BIT(17) )
#define BIT_HPM0_RPT_CHKSUM_ERR_EN                        ( BIT(16) )
#define BIT_PAUSE_OCCUR_ERR_EN                            ( BIT(8) )
#define BIT_VOLT_TUNE_FORBIDEN                            ( BIT(4) )
#define BIT_VOLT_OBS_FORBIDEN                             ( BIT(0) )

/* bits definitions for register REG_AVS_APB_AVS_TUNE_STEP_CFG */
#define BITS_VOLT_TUNE_DOWN_STEP(_X_)                     ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)) )
#define BITS_VOLT_TUNE_UP_STEP(_X_)                       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)) )

/* bits definitions for register REG_AVS_APB_AVS_WAIT_CFG */
#define BITS_RND_INTVAL_WAIT_NUM(_X_)                     ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_VOLT_STB_WAIT_NUM(_X_)                       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AVS_APB_AVS_INT_CFG */
#define BIT_INT_RPT_FORMAT_ERR_CLR                        ( BIT(14) )
#define BIT_INT_RPT_ANLS_ERR_CLR                          ( BIT(13) )
#define BIT_INT_VOLT_TUNE_ERR_CLR                         ( BIT(12) )
#define BIT_INT_RPT_FORMAT_ERR_MASK_STATUS                ( BIT(10) )
#define BIT_INT_RPT_ANLS_ERR_MASK_STATUS                  ( BIT(9) )
#define BIT_INT_VOLT_TUNE_ERR_MASK_STATUS                 ( BIT(8) )
#define BIT_INT_RPT_FORMAT_ERR_RAW_STATUS                 ( BIT(6) )
#define BIT_INT_RPT_ANLS_ERR_RAW_STATUS                   ( BIT(5) )
#define BIT_INT_VOLT_TUNE_ERR_RAW_STATUS                  ( BIT(4) )
#define BIT_INT_RPT_FORMAT_ERR_EN                         ( BIT(2) )
#define BIT_INT_RPT_ANLS_ERR_EN                           ( BIT(1) )
#define BIT_INT_VOLT_TUNE_ERR_EN                          ( BIT(0) )

/* bits definitions for register REG_AVS_APB_AVS_START_SINGLE_ACT */
#define BIT_AVS_START_SINGLE_ACT                          ( BIT(0) )

/* bits definitions for register REG_AVS_APB_AVS_START_REPEAT_ACT */
#define BIT_AVS_START_REPEAT_ACT                          ( BIT(0) )

/* bits definitions for register REG_AVS_APB_AVS_HPM_EN */
#define BIT_HPM15_EN                                      ( BIT(15) )
#define BIT_HPM14_EN                                      ( BIT(14) )
#define BIT_HPM13_EN                                      ( BIT(13) )
#define BIT_HPM12_EN                                      ( BIT(12) )
#define BIT_HPM11_EN                                      ( BIT(11) )
#define BIT_HPM10_EN                                      ( BIT(10) )
#define BIT_HPM9_EN                                       ( BIT(9) )
#define BIT_HPM8_EN                                       ( BIT(8) )
#define BIT_HPM7_EN                                       ( BIT(7) )
#define BIT_HPM6_EN                                       ( BIT(6) )
#define BIT_HPM5_EN                                       ( BIT(5) )
#define BIT_HPM4_EN                                       ( BIT(4) )
#define BIT_HPM3_EN                                       ( BIT(3) )
#define BIT_HPM2_EN                                       ( BIT(2) )
#define BIT_HPM1_EN                                       ( BIT(1) )
#define BIT_HPM0_EN                                       ( BIT(0) )

/* bits definitions for register REG_AVS_APB_AVS_HPM_RPT_ANLS */
#define BITS_HPM_RPT_FF_ANLS(_X_)                         ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_HPM_RPT_7F_ANLS(_X_)                         ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_HPM_RPT_3F_ANLS(_X_)                         ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_HPM_RPT_1F_ANLS(_X_)                         ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_HPM_RPT_0F_ANLS(_X_)                         ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_HPM_RPT_07_ANLS(_X_)                         ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_HPM_RPT_03_ANLS(_X_)                         ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_HPM_RPT_01_ANLS(_X_)                         ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_HPM_RPT_00_ANLS(_X_)                         ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_0 */
#define BITS_HPM3_RPT_VLD(_X_)                            ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_HPM2_RPT_VLD(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_HPM1_RPT_VLD(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HPM0_RPT_VLD(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_1 */
#define BITS_HPM7_RPT_VLD(_X_)                            ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_HPM6_RPT_VLD(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_HPM5_RPT_VLD(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HPM4_RPT_VLD(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_2 */
#define BITS_HPM11_RPT_VLD(_X_)                           ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_HPM10_RPT_VLD(_X_)                           ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_HPM9_RPT_VLD(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HPM8_RPT_VLD(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AVS_APB_AVS_HPM_RPT_VLD_STATUS_3 */
#define BITS_HPM15_RPT_VLD(_X_)                           ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_HPM14_RPT_VLD(_X_)                           ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_HPM13_RPT_VLD(_X_)                           ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HPM12_RPT_VLD(_X_)                           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AVS_APB_AVS_HPM_DLY_WIND_SEL */
#define BITS_DLY_WIND_SEL(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_AVS_APB_FSM_STS */
#define BITS_AVS_FSM_STS(_X_)                             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

#endif
