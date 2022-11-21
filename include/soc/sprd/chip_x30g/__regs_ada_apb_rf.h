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


#ifndef __H_REGS_ADA_APB_RF_HEADFILE_H__
#define __H_REGS_ADA_APB_RF_HEADFILE_H__ __FILE__

#define	REGS_ADA_APB_RF

/* registers definitions for ADA_APB_RF */
#define REG_ADA_APB_RF_APB_DAC_CTR0                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0000)
#define REG_ADA_APB_RF_APB_DAC_CTR1                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0004)
#define REG_ADA_APB_RF_APB_DAC_CTR2                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0008)
#define REG_ADA_APB_RF_APB_DAC_CTR3                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x000C)
#define REG_ADA_APB_RF_APB_DAC_CTR4                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0010)
#define REG_ADA_APB_RF_APB_DAC_STS0                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0014)
#define REG_ADA_APB_RF_APB_DAC_STS1                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0018)
#define REG_ADA_APB_RF_APB_ADC0_CTR0                      SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x001C)
#define REG_ADA_APB_RF_APB_ADC0_CTR1                      SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0020)
#define REG_ADA_APB_RF_APB_ADC0_CTR2                      SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0024)
#define REG_ADA_APB_RF_APB_ADC0_CTR3                      SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0028)
#define REG_ADA_APB_RF_APB_CAL_CTR0                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0030)
#define REG_ADA_APB_RF_APB_CAL_CTR1                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0034)
#define REG_ADA_APB_RF_APB_CAL_CTR3                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x003C)
#define REG_ADA_APB_RF_APB_CAL_STS0                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0044)
#define REG_ADA_APB_RF_APB_CAL_STS1                       SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0048)
#define REG_ADA_APB_RF_APB_ADA_CTRL0                      SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0054)
#define REG_ADA_APB_RF_APB_ADA_CTRL1                      SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0058)
#define REG_ADA_APB_RF_APB_RX_GSM_DFT                     SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x005C)
#define REG_ADA_APB_RF_APB_RX_3G_DFT                      SCI_ADDR(REGS_ADA_APB_RF_BASE, 0x0060)



/* bits definitions for register REG_ADA_APB_RF_APB_DAC_CTR0 */
#define BITS_DAC_PCTRL_1(_X_)                             ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BIT_DAC_OUT_SEL_1                                 ( BIT(17) )
#define BIT_DAC_CLK_SEL_1                                 ( BIT(16) )
#define BITS_DAC_PCTRL_0(_X_)                             ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BIT_DAC_OUT_SEL_0                                 ( BIT(1) )
#define BIT_DAC_CLK_SEL_0                                 ( BIT(0) )

/* bits definitions for register REG_ADA_APB_RF_APB_DAC_CTR1 */
#define BITS_DAC_WD_OVR(_X_)                              ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_DAC_WADDR_OVR(_X_)                           ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_DAC_WLE_OVR                                   ( BIT(2) )
#define BIT_DAC_W_OVR_I_QN                                ( BIT(1) )
#define BIT_DAC_W_OVR_EN                                  ( BIT(0) )

/* bits definitions for register REG_ADA_APB_RF_APB_DAC_CTR2 */
#define BITS_DAC_RSV(_X_)                                 ( (_X_) << 22 & (BIT(22)|BIT(23)|BIT(24)|BIT(25)) )
#define BIT_DAC_IQ_OUT_MSB_INV                            ( BIT(21) )
#define BIT_DAC_IQ_IN_MSB_INV                             ( BIT(20) )
#define BIT_DAC_OVR_EN_CAL                                ( BIT(19) )
#define BIT_DAC_EN_OOSCAL                                 ( BIT(18) )
#define BIT_DAC_IQ_SWAP                                   ( BIT(17) )
#define BIT_DAC_CAL_START                                 ( BIT(16) )
#define BITS_DAC_OFFSETI(_X_)                             ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_DAC_OFFSETQ(_X_)                             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_ADA_APB_RF_APB_DAC_CTR3 */
#define BITS_DAC_Q_DC(_X_)                                ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)) )
#define BITS_DAC_I_DC(_X_)                                ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )

/* bits definitions for register REG_ADA_APB_RF_APB_DAC_CTR4 */
#define BITS_DAC_I_CAL(_X_)                               ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)) )
#define BITS_DAC_Q_CAL(_X_)                               ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )

/* bits definitions for register REG_ADA_APB_RF_APB_DAC_STS0 */
#define BITS_DAC_WTI(_X_)                                 ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_DAC_WTQ(_X_)                                 ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ADA_APB_RF_APB_DAC_STS1 */
#define BITS_DAC_DA_STATE(_X_)                            ( (_X_) << 17 & (BIT(17)|BIT(18)) )
#define BIT_DAC_DONE                                      ( BIT(16) )
#define BITS_DAC_WQ_OVR(_X_)                              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_ADA_APB_RF_APB_ADC0_CTR0 */
#define BITS_ADC0_CALRDOUT(_X_)                           ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BIT_ADC0_CAL_SOFT_RST                             ( BIT(10) )
#define BIT_ADC0_CALRD                                    ( BIT(9) )
#define BIT_ADC0_CALEN                                    ( BIT(8) )
#define BITS_ADC0_CALADDR(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )

/* bits definitions for register REG_ADA_APB_RF_APB_ADC0_CTR1 */
#define BITS_ADC0_DLL_OUT(_X_)                            ( (_X_) << 24 & (BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)) )
#define BITS_ADC0_DLLOOF(_X_)                             ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)) )
#define BIT_ADC0_IQ_SWAP                                  ( BIT(14) )
#define BITS_ADC0_DLLIN(_X_)                              ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BIT_ADC0_IQ_OUT_MSB_INV                           ( BIT(7) )
#define BIT_ADC0_IQ_IN_MSB_INV                            ( BIT(6) )
#define BIT_ADC0_DLL_SOFT_RST                             ( BIT(4) )
#define BIT_ADC0_DLLWR                                    ( BIT(3) )
#define BIT_ADC0_DLLRD                                    ( BIT(2) )
#define BIT_ADC0_DLLEN                                    ( BIT(1) )
#define BIT_ADC0_DEBUG_EN                                 ( BIT(0) )

/* bits definitions for register REG_ADA_APB_RF_APB_ADC0_CTR2 */
#define BITS_ADC0_RSV_0(_X_)                              ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_ADC0_DIG_CLK_SEL_0                            ( BIT(11) )
#define BIT_ADC0_VCM_SEL_0                                ( BIT(10) )
#define BITS_ADC0_DELAY_CAP_0(_X_)                        ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ADC0_BIT_SEL_0(_X_)                          ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_ADC0_VREF_SEL_0(_X_)                         ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_ADC0_VREF_BOOST_0(_X_)                       ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_ADC0_QUANT_BOOST_0(_X_)                      ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ADA_APB_RF_APB_ADC0_CTR3 */
#define BITS_ADC0_RSV_1(_X_)                              ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_ADC0_DIG_CLK_SEL_1                            ( BIT(11) )
#define BIT_ADC0_VCM_SEL_1                                ( BIT(10) )
#define BITS_ADC0_DELAY_CAP_1(_X_)                        ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_ADC0_BIT_SEL_1(_X_)                          ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_ADC0_VREF_SEL_1(_X_)                         ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_ADC0_VREF_BOOST_1(_X_)                       ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_ADC0_QUANT_BOOST_1(_X_)                      ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_ADA_APB_RF_APB_CAL_CTR0 */
#define BIT_LDO_EN_AUTO_EN                                ( BIT(27) )
#define BIT_LDO_EN_FRC                                    ( BIT(26) )
#define BIT_BG_EN_AUTO_EN                                 ( BIT(25) )
#define BIT_BG_EN_FRC                                     ( BIT(24) )
#define BITS_ADC_CLK_CAL_DLY(_X_)                         ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_ADA_CAL_WAIT(_X_)                            ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_CLK_ADA_CAL_EN                                ( BIT(6) )
#define BIT_DAC_PD_CAL                                    ( BIT(5) )
#define BIT_ADC0_EN_CAL                                   ( BIT(3) )
#define BIT_ADC0_CAL                                      ( BIT(1) )
#define BIT_ADA_MODE_SEL                                  ( BIT(0) )

/* bits definitions for register REG_ADA_APB_RF_APB_CAL_CTR1 */
#define BITS_ADC0_Q_DC(_X_)                               ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)) )
#define BITS_ADC0_I_DC(_X_)                               ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )

/* bits definitions for register REG_ADA_APB_RF_APB_CAL_CTR3 */
#define BIT_ADA0_CAL_START                                ( BIT(0) )

/* bits definitions for register REG_ADA_APB_RF_APB_CAL_STS0 */
#define BIT_ADA0_CAL_DONE                                 ( BIT(31) )
#define BITS_ADC0_I_CAL_RESULT(_X_)                       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )

/* bits definitions for register REG_ADA_APB_RF_APB_CAL_STS1 */
#define BITS_ADC0_Q_CAL_RESULT(_X_)                       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )

/* bits definitions for register REG_ADA_APB_RF_APB_ADA_CTRL0 */
#define BITS_ADA_AUTO_CTRL(_X_)                           (_X_)

/* bits definitions for register REG_ADA_APB_RF_APB_ADA_CTRL1 */
#define BITS_ADA_FRC_CTRL(_X_)                            (_X_)

/* bits definitions for register REG_ADA_APB_RF_APB_RX_GSM_DFT */
#define BITS_RX_GSM_DFT_Q(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)) )
#define BITS_RX_GSM_DFT_I(_X_)                            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )

/* bits definitions for register REG_ADA_APB_RF_APB_RX_3G_DFT */
#define BITS_RX_3G_DFT_Q(_X_)                             ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)) )
#define BITS_RX_3G_DFT_I(_X_)                             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )

#endif
