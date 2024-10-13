/* the head file modifier:     g   2015-03-26 16:05:35*/

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


#ifndef __H_REGS_AON_PREDIV_HEADFILE_H__
#define __H_REGS_AON_PREDIV_HEADFILE_H__ __FILE__



#define REG_AON_PREDIV_SOFT_CNT_DONE0_CFG         SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0020 )
#define REG_AON_PREDIV_PLL_WAIT_SEL0_CFG          SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0024 )
#define REG_AON_PREDIV_PLL_WAIT_SW_CTL0_CFG       SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0028 )
#define REG_AON_PREDIV_DIV_EN_SEL0_CFG            SCI_ADDR(REGS_AON_PREDIV_BASE, 0x002C )
#define REG_AON_PREDIV_DIV_EN_SEL1_CFG            SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0030 )
#define REG_AON_PREDIV_DIV_EN_SW_CTL0_CFG         SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0034 )
#define REG_AON_PREDIV_DIV_EN_SW_CTL1_CFG         SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0038 )
#define REG_AON_PREDIV_GATE_EN_SEL0_CFG           SCI_ADDR(REGS_AON_PREDIV_BASE, 0x003C )
#define REG_AON_PREDIV_GATE_EN_SEL1_CFG           SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0040 )
#define REG_AON_PREDIV_GATE_EN_SEL2_CFG           SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0044 )
#define REG_AON_PREDIV_GATE_EN_SEL3_CFG           SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0048 )
#define REG_AON_PREDIV_GATE_EN_SEL4_CFG           SCI_ADDR(REGS_AON_PREDIV_BASE, 0x004C )
#define REG_AON_PREDIV_GATE_EN_SW_CTL0_CFG        SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0050 )
#define REG_AON_PREDIV_GATE_EN_SW_CTL1_CFG        SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0054 )
#define REG_AON_PREDIV_GATE_EN_SW_CTL2_CFG        SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0058 )
#define REG_AON_PREDIV_GATE_EN_SW_CTL3_CFG        SCI_ADDR(REGS_AON_PREDIV_BASE, 0x005C )
#define REG_AON_PREDIV_GATE_EN_SW_CTL4_CFG        SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0060 )
#define REG_AON_PREDIV_OUTPUT_CLOCK_MUX_SEL0_CFG  SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0064 )
#define REG_AON_PREDIV_OUTPUT_CLOCK_MUX_SEL1_CFG  SCI_ADDR(REGS_AON_PREDIV_BASE, 0x0068 )

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_SOFT_CNT_DONE0_CFG
// Register Offset : 0x0020
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_GPLL_SOFT_CNT_DONE                                    BIT(18)
#define BIT_AON_PREDIV_TWPLL_1536M_SOFT_CNT_DONE                             BIT(17)
#define BIT_AON_PREDIV_TWPLL_768M_SOFT_CNT_DONE                              BIT(16)
#define BIT_AON_PREDIV_TWPLL_512M_SOFT_CNT_DONE                              BIT(15)
#define BIT_AON_PREDIV_TWPLL_307M_SOFT_CNT_DONE                              BIT(14)
#define BIT_AON_PREDIV_LTEPLL0_1228M_SOFT_CNT_DONE                           BIT(13)
#define BIT_AON_PREDIV_LTEPLL0_614M_SOFT_CNT_DONE                            BIT(12)
#define BIT_AON_PREDIV_LTEPLL0_409M_SOFT_CNT_DONE                            BIT(11)
#define BIT_AON_PREDIV_LTEPLL0_245M_SOFT_CNT_DONE                            BIT(10)
#define BIT_AON_PREDIV_LTEPLL1_614M_SOFT_CNT_DONE                            BIT(9)
#define BIT_AON_PREDIV_LTEPLL1_409M_SOFT_CNT_DONE                            BIT(8)
#define BIT_AON_PREDIV_LTEPLL1_245M_SOFT_CNT_DONE                            BIT(7)
#define BIT_AON_PREDIV_XBUF_SOFT_CNT_DONE                                    BIT(6)
#define BIT_AON_PREDIV_RPLL0_1040M_SOFT_CNT_DONE                             BIT(5)
#define BIT_AON_PREDIV_RPLL0_26M_SOFT_CNT_DONE                               BIT(4)
#define BIT_AON_PREDIV_RPLL1_936M_SOFT_CNT_DONE                              BIT(3)
#define BIT_AON_PREDIV_RPLL1_26M_SOFT_CNT_DONE                               BIT(2)
#define BIT_AON_PREDIV_RC_SOFT_CNT_DONE                                      BIT(1)
#define BIT_AON_PREDIV_RCO_SOFT_CNT_DONE                                     BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_PLL_WAIT_SEL0_CFG
// Register Offset : 0x0024
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_GPLL_WAIT_AUTO_GATE_SEL                               BIT(18)
#define BIT_AON_PREDIV_TWPLL_1536M_WAIT_AUTO_GATE_SEL                        BIT(17)
#define BIT_AON_PREDIV_TWPLL_768M_WAIT_AUTO_GATE_SEL                         BIT(16)
#define BIT_AON_PREDIV_TWPLL_512M_WAIT_AUTO_GATE_SEL                         BIT(15)
#define BIT_AON_PREDIV_TWPLL_307M_WAIT_AUTO_GATE_SEL                         BIT(14)
#define BIT_AON_PREDIV_LTEPLL0_1228M_WAIT_AUTO_GATE_SEL                      BIT(13)
#define BIT_AON_PREDIV_LTEPLL0_614M_WAIT_AUTO_GATE_SEL                       BIT(12)
#define BIT_AON_PREDIV_LTEPLL0_409M_WAIT_AUTO_GATE_SEL                       BIT(11)
#define BIT_AON_PREDIV_LTEPLL0_245M_WAIT_AUTO_GATE_SEL                       BIT(10)
#define BIT_AON_PREDIV_LTEPLL1_614M_WAIT_AUTO_GATE_SEL                       BIT(9)
#define BIT_AON_PREDIV_LTEPLL1_409M_WAIT_AUTO_GATE_SEL                       BIT(8)
#define BIT_AON_PREDIV_LTEPLL1_245M_WAIT_AUTO_GATE_SEL                       BIT(7)
#define BIT_AON_PREDIV_XBUF_WAIT_AUTO_GATE_SEL                               BIT(6)
#define BIT_AON_PREDIV_RPLL0_1040M_WAIT_AUTO_GATE_SEL                        BIT(5)
#define BIT_AON_PREDIV_RPLL0_26M_WAIT_AUTO_GATE_SEL                          BIT(4)
#define BIT_AON_PREDIV_RPLL1_936M_WAIT_AUTO_GATE_SEL                         BIT(3)
#define BIT_AON_PREDIV_RPLL1_26M_WAIT_AUTO_GATE_SEL                          BIT(2)
#define BIT_AON_PREDIV_RC_WAIT_AUTO_GATE_SEL                                 BIT(1)
#define BIT_AON_PREDIV_RCO_WAIT_AUTO_GATE_SEL                                BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_PLL_WAIT_SW_CTL0_CFG
// Register Offset : 0x0028
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_GPLL_WAIT_FORCE_EN                                    BIT(18)
#define BIT_AON_PREDIV_TWPLL_1536M_WAIT_FORCE_EN                             BIT(17)
#define BIT_AON_PREDIV_TWPLL_768M_WAIT_FORCE_EN                              BIT(16)
#define BIT_AON_PREDIV_TWPLL_512M_WAIT_FORCE_EN                              BIT(15)
#define BIT_AON_PREDIV_TWPLL_307M_WAIT_FORCE_EN                              BIT(14)
#define BIT_AON_PREDIV_LTEPLL0_1228M_WAIT_FORCE_EN                           BIT(13)
#define BIT_AON_PREDIV_LTEPLL0_614M_WAIT_FORCE_EN                            BIT(12)
#define BIT_AON_PREDIV_LTEPLL0_409M_WAIT_FORCE_EN                            BIT(11)
#define BIT_AON_PREDIV_LTEPLL0_245M_WAIT_FORCE_EN                            BIT(10)
#define BIT_AON_PREDIV_LTEPLL1_614M_WAIT_FORCE_EN                            BIT(9)
#define BIT_AON_PREDIV_LTEPLL1_409M_WAIT_FORCE_EN                            BIT(8)
#define BIT_AON_PREDIV_LTEPLL1_245M_WAIT_FORCE_EN                            BIT(7)
#define BIT_AON_PREDIV_XBUF_WAIT_FORCE_EN                                    BIT(6)
#define BIT_AON_PREDIV_RPLL0_1040M_WAIT_FORCE_EN                             BIT(5)
#define BIT_AON_PREDIV_RPLL0_26M_WAIT_FORCE_EN                               BIT(4)
#define BIT_AON_PREDIV_RPLL1_936M_WAIT_FORCE_EN                              BIT(3)
#define BIT_AON_PREDIV_RPLL1_26M_WAIT_FORCE_EN                               BIT(2)
#define BIT_AON_PREDIV_RC_WAIT_FORCE_EN                                      BIT(1)
#define BIT_AON_PREDIV_RCO_WAIT_FORCE_EN                                     BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_DIV_EN_SEL0_CFG
// Register Offset : 0x002C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_GPLL_DIV_37M5_AUTO_GATE_SEL                           BIT(31)
#define BIT_AON_PREDIV_LTEPLL0_DIV_245M_122M9_AUTO_GATE_SEL                  BIT(30)
#define BIT_AON_PREDIV_LTEPLL0_DIV_245M_61M4_AUTO_GATE_SEL                   BIT(29)
#define BIT_AON_PREDIV_LTEPLL0_DIV_409M_204M8_AUTO_GATE_SEL                  BIT(28)
#define BIT_AON_PREDIV_LTEPLL0_DIV_409M_102M4_AUTO_GATE_SEL                  BIT(27)
#define BIT_AON_PREDIV_LTEPLL0_DIV_614M_307M2_AUTO_GATE_SEL                  BIT(26)
#define BIT_AON_PREDIV_LTEPLL0_DIV_614M_153M6_AUTO_GATE_SEL                  BIT(25)
#define BIT_AON_PREDIV_LTEPLL0_DIV_614M_38M4_AUTO_GATE_SEL                   BIT(24)
#define BIT_AON_PREDIV_LTEPLL1_DIV_245M_122M9_AUTO_GATE_SEL                  BIT(23)
#define BIT_AON_PREDIV_LTEPLL1_DIV_245M_61M4_AUTO_GATE_SEL                   BIT(22)
#define BIT_AON_PREDIV_LTEPLL1_DIV_409M_204M8_AUTO_GATE_SEL                  BIT(21)
#define BIT_AON_PREDIV_LTEPLL1_DIV_409M_102M4_AUTO_GATE_SEL                  BIT(20)
#define BIT_AON_PREDIV_LTEPLL1_DIV_614M_307M2_AUTO_GATE_SEL                  BIT(19)
#define BIT_AON_PREDIV_LTEPLL1_DIV_614M_153M6_AUTO_GATE_SEL                  BIT(18)
#define BIT_AON_PREDIV_LTEPLL1_DIV_614M_38M4_AUTO_GATE_SEL                   BIT(17)
#define BIT_AON_PREDIV_RC_DIV_2M_AUTO_GATE_SEL                               BIT(16)
#define BIT_AON_PREDIV_RCO_DIV_4M_AUTO_GATE_SEL                              BIT(15)
#define BIT_AON_PREDIV_RCO_DIV_25M_AUTO_GATE_SEL                             BIT(14)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_173M3_AUTO_GATE_SEL                   BIT(13)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_86M7_AUTO_GATE_SEL                    BIT(12)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_43M3_AUTO_GATE_SEL                    BIT(11)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_115M6_AUTO_GATE_SEL                   BIT(10)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_57M8_AUTO_GATE_SEL                    BIT(9)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_468M_AUTO_GATE_SEL                     BIT(8)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_312M_AUTO_GATE_SEL                     BIT(7)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_156M_AUTO_GATE_SEL                     BIT(6)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_78M_AUTO_GATE_SEL                      BIT(5)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_39M_AUTO_GATE_SEL                      BIT(4)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_104M_AUTO_GATE_SEL                     BIT(3)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_52M_AUTO_GATE_SEL                      BIT(2)
#define BIT_AON_PREDIV_RTC_DIV_3K_AUTO_GATE_SEL                              BIT(1)
#define BIT_AON_PREDIV_RTC_DIV_1K_AUTO_GATE_SEL                              BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_DIV_EN_SEL1_CFG
// Register Offset : 0x0030
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_TWPLL_DIV_307M_153M6_AUTO_GATE_SEL                    BIT(17)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_76M8_AUTO_GATE_SEL                     BIT(16)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_38M4_AUTO_GATE_SEL                     BIT(15)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_19M2_AUTO_GATE_SEL                     BIT(14)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_102M4_AUTO_GATE_SEL                    BIT(13)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_51M2_AUTO_GATE_SEL                     BIT(12)
#define BIT_AON_PREDIV_TWPLL_DIV_512M_256M_AUTO_GATE_SEL                     BIT(11)
#define BIT_AON_PREDIV_TWPLL_DIV_512M_128M_AUTO_GATE_SEL                     BIT(10)
#define BIT_AON_PREDIV_TWPLL_DIV_512M_64M_AUTO_GATE_SEL                      BIT(9)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_384M_AUTO_GATE_SEL                     BIT(8)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_192M_AUTO_GATE_SEL                     BIT(7)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_96M_AUTO_GATE_SEL                      BIT(6)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_48M_AUTO_GATE_SEL                      BIT(5)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_24M_AUTO_GATE_SEL                      BIT(4)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_12M_AUTO_GATE_SEL                      BIT(3)
#define BIT_AON_PREDIV_XBUF_DIV_2M_AUTO_GATE_SEL                             BIT(2)
#define BIT_AON_PREDIV_XBUF_DIV_1M_AUTO_GATE_SEL                             BIT(1)
#define BIT_AON_PREDIV_XBUF_DIV_4M3_AUTO_GATE_SEL                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_DIV_EN_SW_CTL0_CFG
// Register Offset : 0x0034
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_GPLL_DIV_37M5_FORCE_EN                                BIT(31)
#define BIT_AON_PREDIV_LTEPLL0_DIV_245M_122M9_FORCE_EN                       BIT(30)
#define BIT_AON_PREDIV_LTEPLL0_DIV_245M_61M4_FORCE_EN                        BIT(29)
#define BIT_AON_PREDIV_LTEPLL0_DIV_409M_204M8_FORCE_EN                       BIT(28)
#define BIT_AON_PREDIV_LTEPLL0_DIV_409M_102M4_FORCE_EN                       BIT(27)
#define BIT_AON_PREDIV_LTEPLL0_DIV_614M_307M2_FORCE_EN                       BIT(26)
#define BIT_AON_PREDIV_LTEPLL0_DIV_614M_153M6_FORCE_EN                       BIT(25)
#define BIT_AON_PREDIV_LTEPLL0_DIV_614M_38M4_FORCE_EN                        BIT(24)
#define BIT_AON_PREDIV_LTEPLL1_DIV_245M_122M9_FORCE_EN                       BIT(23)
#define BIT_AON_PREDIV_LTEPLL1_DIV_245M_61M4_FORCE_EN                        BIT(22)
#define BIT_AON_PREDIV_LTEPLL1_DIV_409M_204M8_FORCE_EN                       BIT(21)
#define BIT_AON_PREDIV_LTEPLL1_DIV_409M_102M4_FORCE_EN                       BIT(20)
#define BIT_AON_PREDIV_LTEPLL1_DIV_614M_307M2_FORCE_EN                       BIT(19)
#define BIT_AON_PREDIV_LTEPLL1_DIV_614M_153M6_FORCE_EN                       BIT(18)
#define BIT_AON_PREDIV_LTEPLL1_DIV_614M_38M4_FORCE_EN                        BIT(17)
#define BIT_AON_PREDIV_RC_DIV_2M_FORCE_EN                                    BIT(16)
#define BIT_AON_PREDIV_RCO_DIV_4M_FORCE_EN                                   BIT(15)
#define BIT_AON_PREDIV_RCO_DIV_25M_FORCE_EN                                  BIT(14)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_173M3_FORCE_EN                        BIT(13)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_86M7_FORCE_EN                         BIT(12)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_43M3_FORCE_EN                         BIT(11)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_115M6_FORCE_EN                        BIT(10)
#define BIT_AON_PREDIV_RPLL0_DIV_1040M_57M8_FORCE_EN                         BIT(9)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_468M_FORCE_EN                          BIT(8)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_312M_FORCE_EN                          BIT(7)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_156M_FORCE_EN                          BIT(6)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_78M_FORCE_EN                           BIT(5)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_39M_FORCE_EN                           BIT(4)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_104M_FORCE_EN                          BIT(3)
#define BIT_AON_PREDIV_RPLL1_DIV_936M_52M_FORCE_EN                           BIT(2)
#define BIT_AON_PREDIV_RTC_DIV_3K_FORCE_EN                                   BIT(1)
#define BIT_AON_PREDIV_RTC_DIV_1K_FORCE_EN                                   BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_DIV_EN_SW_CTL1_CFG
// Register Offset : 0x0038
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_TWPLL_DIV_307M_153M6_FORCE_EN                         BIT(17)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_76M8_FORCE_EN                          BIT(16)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_38M4_FORCE_EN                          BIT(15)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_19M2_FORCE_EN                          BIT(14)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_102M4_FORCE_EN                         BIT(13)
#define BIT_AON_PREDIV_TWPLL_DIV_307M_51M2_FORCE_EN                          BIT(12)
#define BIT_AON_PREDIV_TWPLL_DIV_512M_256M_FORCE_EN                          BIT(11)
#define BIT_AON_PREDIV_TWPLL_DIV_512M_128M_FORCE_EN                          BIT(10)
#define BIT_AON_PREDIV_TWPLL_DIV_512M_64M_FORCE_EN                           BIT(9)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_384M_FORCE_EN                          BIT(8)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_192M_FORCE_EN                          BIT(7)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_96M_FORCE_EN                           BIT(6)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_48M_FORCE_EN                           BIT(5)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_24M_FORCE_EN                           BIT(4)
#define BIT_AON_PREDIV_TWPLL_DIV_768M_12M_FORCE_EN                           BIT(3)
#define BIT_AON_PREDIV_XBUF_DIV_2M_FORCE_EN                                  BIT(2)
#define BIT_AON_PREDIV_XBUF_DIV_1M_FORCE_EN                                  BIT(1)
#define BIT_AON_PREDIV_XBUF_DIV_4M3_FORCE_EN                                 BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SEL0_CFG
// Register Offset : 0x003C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_LPLL_614M4_CA53_AUTO_GATE_SEL                     BIT(31)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_CA53_AUTO_GATE_SEL                     BIT(30)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_CA53_AUTO_GATE_SEL                     BIT(29)
#define BIT_AON_PREDIV_CGM_XTL_26M_CA53_AUTO_GATE_SEL                        BIT(28)
#define BIT_AON_PREDIV_CGM_XTL_26M_GPU_AUTO_GATE_SEL                         BIT(27)
#define BIT_AON_PREDIV_CGM_GPLL_600M_GPU_AUTO_GATE_SEL                       BIT(26)
#define BIT_AON_PREDIV_CGM_LPLL_614M4_GPU_AUTO_GATE_SEL                      BIT(25)
#define BIT_AON_PREDIV_CGM_TWPLL_768M_GPU_AUTO_GATE_SEL                      BIT(24)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_GPU_AUTO_GATE_SEL                      BIT(23)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_GPU_AUTO_GATE_SEL                      BIT(22)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_GPU_AUTO_GATE_SEL                      BIT(21)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_GPU_AUTO_GATE_SEL                     BIT(20)
#define BIT_AON_PREDIV_CGM_XTL_26M_PUB0_AUTO_GATE_SEL                        BIT(19)
#define BIT_AON_PREDIV_CGM_XTL_26M_PUB1_AUTO_GATE_SEL                        BIT(18)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_AP_AUTO_GATE_SEL                       BIT(17)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_AP_AUTO_GATE_SEL                       BIT(16)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_AP_AUTO_GATE_SEL                      BIT(15)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_AP_AUTO_GATE_SEL                       BIT(14)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_AP_AUTO_GATE_SEL                        BIT(13)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_AP_AUTO_GATE_SEL                        BIT(12)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_AP_AUTO_GATE_SEL                       BIT(11)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_AP_AUTO_GATE_SEL                        BIT(10)
#define BIT_AON_PREDIV_CGM_TWPLL_24M_AP_AUTO_GATE_SEL                        BIT(9)
#define BIT_AON_PREDIV_CGM_XTL_26M_AP_AUTO_GATE_SEL                          BIT(8)
#define BIT_AON_PREDIV_CGM_TWPLL_307M2_VSP_AUTO_GATE_SEL                     BIT(7)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_VSP_AUTO_GATE_SEL                      BIT(6)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_VSP_AUTO_GATE_SEL                     BIT(5)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_VSP_AUTO_GATE_SEL                      BIT(4)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_VSP_AUTO_GATE_SEL                       BIT(3)
#define BIT_AON_PREDIV_CGM_TWPLL_76M8_VSP_AUTO_GATE_SEL                      BIT(2)
#define BIT_AON_PREDIV_CGM_XTL_26M_VSP_AUTO_GATE_SEL                         BIT(1)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_CAM_AUTO_GATE_SEL                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SEL1_CFG
// Register Offset : 0x0040
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_TWPLL_307M2_CAM_AUTO_GATE_SEL                     BIT(31)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_CAM_AUTO_GATE_SEL                      BIT(30)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_CAM_AUTO_GATE_SEL                      BIT(29)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_CAM_AUTO_GATE_SEL                     BIT(28)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_CAM_AUTO_GATE_SEL                      BIT(27)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_CAM_AUTO_GATE_SEL                       BIT(26)
#define BIT_AON_PREDIV_CGM_TWPLL_76M8_CAM_AUTO_GATE_SEL                      BIT(25)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_CAM_AUTO_GATE_SEL                       BIT(24)
#define BIT_AON_PREDIV_CGM_RPLL1_468M_CAM_AUTO_GATE_SEL                      BIT(23)
#define BIT_AON_PREDIV_CGM_XTL_26M_CAM_AUTO_GATE_SEL                         BIT(22)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_WTLCP_AUTO_GATE_SEL                    BIT(21)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_WTLCP_AUTO_GATE_SEL                    BIT(20)
#define BIT_AON_PREDIV_CGM_TWPLL_307M2_WTLCP_AUTO_GATE_SEL                   BIT(19)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_WTLCP_AUTO_GATE_SEL                    BIT(18)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_WTLCP_AUTO_GATE_SEL                    BIT(17)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_WTLCP_AUTO_GATE_SEL                   BIT(16)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_WTLCP_AUTO_GATE_SEL                    BIT(15)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_WTLCP_AUTO_GATE_SEL                     BIT(14)
#define BIT_AON_PREDIV_CGM_TWPLL_76M8_WTLCP_AUTO_GATE_SEL                    BIT(13)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_WTLCP_AUTO_GATE_SEL                     BIT(12)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_WTLCP_AUTO_GATE_SEL                    BIT(11)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_WTLCP_AUTO_GATE_SEL                     BIT(10)
#define BIT_AON_PREDIV_CGM_LPLL0_614M4_WTLCP_AUTO_GATE_SEL                   BIT(9)
#define BIT_AON_PREDIV_CGM_LPLL0_307M2_WTLCP_AUTO_GATE_SEL                   BIT(8)
#define BIT_AON_PREDIV_CGM_LPLL0_245M76_WTLCP_AUTO_GATE_SEL                  BIT(7)
#define BIT_AON_PREDIV_CGM_LPLL0_204M8_WTLCP_AUTO_GATE_SEL                   BIT(6)
#define BIT_AON_PREDIV_CGM_LPLL0_153M6_WTLCP_AUTO_GATE_SEL                   BIT(5)
#define BIT_AON_PREDIV_CGM_LPLL0_122M88_WTLCP_AUTO_GATE_SEL                  BIT(4)
#define BIT_AON_PREDIV_CGM_LPLL0_102M4_WTLCP_AUTO_GATE_SEL                   BIT(3)
#define BIT_AON_PREDIV_CGM_LPLL0_61M44_WTLCP_AUTO_GATE_SEL                   BIT(2)
#define BIT_AON_PREDIV_CGM_LPLL1_307M2_WTLCP_AUTO_GATE_SEL                   BIT(1)
#define BIT_AON_PREDIV_CGM_LPLL1_245M76_WTLCP_AUTO_GATE_SEL                  BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SEL2_CFG
// Register Offset : 0x0044
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_LPLL1_204M8_WTLCP_AUTO_GATE_SEL                   BIT(31)
#define BIT_AON_PREDIV_CGM_LPLL1_153M6_WTLCP_AUTO_GATE_SEL                   BIT(30)
#define BIT_AON_PREDIV_CGM_LPLL1_122M88_WTLCP_AUTO_GATE_SEL                  BIT(29)
#define BIT_AON_PREDIV_CGM_LPLL1_102M4_WTLCP_AUTO_GATE_SEL                   BIT(28)
#define BIT_AON_PREDIV_CGM_LPLL1_61M44_WTLCP_AUTO_GATE_SEL                   BIT(27)
#define BIT_AON_PREDIV_CGM_XTL_26M_RF0_WTLCP_AUTO_GATE_SEL                   BIT(26)
#define BIT_AON_PREDIV_CGM_XTL_26M_RF1_WTLCP_AUTO_GATE_SEL                   BIT(25)
#define BIT_AON_PREDIV_CGM_LPLL_614M4_PUBCP_AUTO_GATE_SEL                    BIT(24)
#define BIT_AON_PREDIV_CGM_TWPLL_768M_PUBCP_AUTO_GATE_SEL                    BIT(23)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_PUBCP_AUTO_GATE_SEL                    BIT(22)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_PUBCP_AUTO_GATE_SEL                    BIT(21)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_PUBCP_AUTO_GATE_SEL                   BIT(20)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_PUBCP_AUTO_GATE_SEL                    BIT(19)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_PUBCP_AUTO_GATE_SEL                     BIT(18)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_PUBCP_AUTO_GATE_SEL                     BIT(17)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_PUBCP_AUTO_GATE_SEL                    BIT(16)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_PUBCP_AUTO_GATE_SEL                     BIT(15)
#define BIT_AON_PREDIV_CGM_XTL_26M_PUBCP_AUTO_GATE_SEL                       BIT(14)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_DISP_AUTO_GATE_SEL                     BIT(13)
#define BIT_AON_PREDIV_CGM_TWPLL_307M2_DISP_AUTO_GATE_SEL                    BIT(12)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_DISP_AUTO_GATE_SEL                     BIT(11)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_DISP_AUTO_GATE_SEL                     BIT(10)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_DISP_AUTO_GATE_SEL                    BIT(9)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_DISP_AUTO_GATE_SEL                     BIT(8)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_DISP_AUTO_GATE_SEL                      BIT(7)
#define BIT_AON_PREDIV_CGM_XTL_26M_DISP_AUTO_GATE_SEL                        BIT(6)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_AGCP_AUTO_GATE_SEL                     BIT(5)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_AGCP_AUTO_GATE_SEL                     BIT(4)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_AGCP_AUTO_GATE_SEL                     BIT(3)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_AGCP_AUTO_GATE_SEL                    BIT(2)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_AGCP_AUTO_GATE_SEL                     BIT(1)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_AGCP_AUTO_GATE_SEL                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SEL3_CFG
// Register Offset : 0x0048
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_TWPLL_76M8_AGCP_AUTO_GATE_SEL                     BIT(31)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_AGCP_AUTO_GATE_SEL                      BIT(30)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_AGCP_AUTO_GATE_SEL                     BIT(29)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_AGCP_AUTO_GATE_SEL                      BIT(28)
#define BIT_AON_PREDIV_CGM_XTL_26M_AGCP_AUTO_GATE_SEL                        BIT(27)
#define BIT_AON_PREDIV_CGM_XTL_26M_GGE_AGCP_AUTO_GATE_SEL                    BIT(26)
#define BIT_AON_PREDIV_CGM_GPLL_37M_AON_AUTO_GATE_SEL                        BIT(25)
#define BIT_AON_PREDIV_CGM_LPLL_1228M8_AON_AUTO_GATE_SEL                     BIT(24)
#define BIT_AON_PREDIV_CGM_LPLL_614M4_AON_AUTO_GATE_SEL                      BIT(23)
#define BIT_AON_PREDIV_CGM_LPLL_38M_AON_AUTO_GATE_SEL                        BIT(22)
#define BIT_AON_PREDIV_CGM_TWPLL_1536M_AON_AUTO_GATE_SEL                     BIT(21)
#define BIT_AON_PREDIV_CGM_TWPLL_768M_AON_AUTO_GATE_SEL                      BIT(20)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_AON_AUTO_GATE_SEL                      BIT(19)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_AON_AUTO_GATE_SEL                      BIT(18)
#define BIT_AON_PREDIV_CGM_TWPLL_307M2_AON_AUTO_GATE_SEL                     BIT(17)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_AON_AUTO_GATE_SEL                      BIT(16)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_AON_AUTO_GATE_SEL                      BIT(15)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_AON_AUTO_GATE_SEL                     BIT(14)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_AON_AUTO_GATE_SEL                      BIT(13)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_AON_AUTO_GATE_SEL                       BIT(12)
#define BIT_AON_PREDIV_CGM_TWPLL_76M8_AON_AUTO_GATE_SEL                      BIT(11)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_AON_AUTO_GATE_SEL                      BIT(10)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_AON_AUTO_GATE_SEL                       BIT(9)
#define BIT_AON_PREDIV_CGM_TWPLL_38M4_AON_AUTO_GATE_SEL                      BIT(8)
#define BIT_AON_PREDIV_CGM_TWPLL_24M_AON_AUTO_GATE_SEL                       BIT(7)
#define BIT_AON_PREDIV_CGM_TWPLL_19M2_AON_AUTO_GATE_SEL                      BIT(6)
#define BIT_AON_PREDIV_CGM_TWPLL_12M_AON_AUTO_GATE_SEL                       BIT(5)
#define BIT_AON_PREDIV_CGM_26M_RF0_AON_AUTO_GATE_SEL                         BIT(4)
#define BIT_AON_PREDIV_CGM_26M_RF1_AON_AUTO_GATE_SEL                         BIT(3)
#define BIT_AON_PREDIV_CGM_26M_AON_AUTO_GATE_SEL                             BIT(2)
#define BIT_AON_PREDIV_CGM_4M_AON_AUTO_GATE_SEL                              BIT(1)
#define BIT_AON_PREDIV_CGM_2M_AON_AUTO_GATE_SEL                              BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SEL4_CFG
// Register Offset : 0x004C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_1M_AON_AUTO_GATE_SEL                              BIT(5)
#define BIT_AON_PREDIV_CGM_4M_RC_AON_AUTO_GATE_SEL                           BIT(4)
#define BIT_AON_PREDIV_CGM_2M_RC_AON_AUTO_GATE_SEL                           BIT(3)
#define BIT_AON_PREDIV_CGM_RCO_100M_AON_AUTO_GATE_SEL                        BIT(2)
#define BIT_AON_PREDIV_CGM_RCO_25M_AON_AUTO_GATE_SEL                         BIT(1)
#define BIT_AON_PREDIV_CGM_RCO_4M_AON_AUTO_GATE_SEL                          BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SW_CTL0_CFG
// Register Offset : 0x0050
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_LPLL_614M4_CA53_FORCE_EN                          BIT(31)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_CA53_FORCE_EN                          BIT(30)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_CA53_FORCE_EN                          BIT(29)
#define BIT_AON_PREDIV_CGM_XTL_26M_CA53_FORCE_EN                             BIT(28)
#define BIT_AON_PREDIV_CGM_XTL_26M_GPU_FORCE_EN                              BIT(27)
#define BIT_AON_PREDIV_CGM_GPLL_600M_GPU_FORCE_EN                            BIT(26)
#define BIT_AON_PREDIV_CGM_LPLL_614M4_GPU_FORCE_EN                           BIT(25)
#define BIT_AON_PREDIV_CGM_TWPLL_768M_GPU_FORCE_EN                           BIT(24)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_GPU_FORCE_EN                           BIT(23)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_GPU_FORCE_EN                           BIT(22)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_GPU_FORCE_EN                           BIT(21)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_GPU_FORCE_EN                          BIT(20)
#define BIT_AON_PREDIV_CGM_XTL_26M_PUB0_FORCE_EN                             BIT(19)
#define BIT_AON_PREDIV_CGM_XTL_26M_PUB1_FORCE_EN                             BIT(18)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_AP_FORCE_EN                            BIT(17)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_AP_FORCE_EN                            BIT(16)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_AP_FORCE_EN                           BIT(15)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_AP_FORCE_EN                            BIT(14)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_AP_FORCE_EN                             BIT(13)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_AP_FORCE_EN                             BIT(12)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_AP_FORCE_EN                            BIT(11)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_AP_FORCE_EN                             BIT(10)
#define BIT_AON_PREDIV_CGM_TWPLL_24M_AP_FORCE_EN                             BIT(9)
#define BIT_AON_PREDIV_CGM_XTL_26M_AP_FORCE_EN                               BIT(8)
#define BIT_AON_PREDIV_CGM_TWPLL_307M2_VSP_FORCE_EN                          BIT(7)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_VSP_FORCE_EN                           BIT(6)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_VSP_FORCE_EN                          BIT(5)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_VSP_FORCE_EN                           BIT(4)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_VSP_FORCE_EN                            BIT(3)
#define BIT_AON_PREDIV_CGM_TWPLL_76M8_VSP_FORCE_EN                           BIT(2)
#define BIT_AON_PREDIV_CGM_XTL_26M_VSP_FORCE_EN                              BIT(1)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_CAM_FORCE_EN                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SW_CTL1_CFG
// Register Offset : 0x0054
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_TWPLL_307M2_CAM_FORCE_EN                          BIT(31)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_CAM_FORCE_EN                           BIT(30)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_CAM_FORCE_EN                           BIT(29)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_CAM_FORCE_EN                          BIT(28)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_CAM_FORCE_EN                           BIT(27)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_CAM_FORCE_EN                            BIT(26)
#define BIT_AON_PREDIV_CGM_TWPLL_76M8_CAM_FORCE_EN                           BIT(25)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_CAM_FORCE_EN                            BIT(24)
#define BIT_AON_PREDIV_CGM_RPLL1_468M_CAM_FORCE_EN                           BIT(23)
#define BIT_AON_PREDIV_CGM_XTL_26M_CAM_FORCE_EN                              BIT(22)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_WTLCP_FORCE_EN                         BIT(21)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_WTLCP_FORCE_EN                         BIT(20)
#define BIT_AON_PREDIV_CGM_TWPLL_307M2_WTLCP_FORCE_EN                        BIT(19)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_WTLCP_FORCE_EN                         BIT(18)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_WTLCP_FORCE_EN                         BIT(17)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_WTLCP_FORCE_EN                        BIT(16)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_WTLCP_FORCE_EN                         BIT(15)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_WTLCP_FORCE_EN                          BIT(14)
#define BIT_AON_PREDIV_CGM_TWPLL_76M8_WTLCP_FORCE_EN                         BIT(13)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_WTLCP_FORCE_EN                          BIT(12)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_WTLCP_FORCE_EN                         BIT(11)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_WTLCP_FORCE_EN                          BIT(10)
#define BIT_AON_PREDIV_CGM_LPLL0_614M4_WTLCP_FORCE_EN                        BIT(9)
#define BIT_AON_PREDIV_CGM_LPLL0_307M2_WTLCP_FORCE_EN                        BIT(8)
#define BIT_AON_PREDIV_CGM_LPLL0_245M76_WTLCP_FORCE_EN                       BIT(7)
#define BIT_AON_PREDIV_CGM_LPLL0_204M8_WTLCP_FORCE_EN                        BIT(6)
#define BIT_AON_PREDIV_CGM_LPLL0_153M6_WTLCP_FORCE_EN                        BIT(5)
#define BIT_AON_PREDIV_CGM_LPLL0_122M88_WTLCP_FORCE_EN                       BIT(4)
#define BIT_AON_PREDIV_CGM_LPLL0_102M4_WTLCP_FORCE_EN                        BIT(3)
#define BIT_AON_PREDIV_CGM_LPLL0_61M44_WTLCP_FORCE_EN                        BIT(2)
#define BIT_AON_PREDIV_CGM_LPLL1_307M2_WTLCP_FORCE_EN                        BIT(1)
#define BIT_AON_PREDIV_CGM_LPLL1_245M76_WTLCP_FORCE_EN                       BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SW_CTL2_CFG
// Register Offset : 0x0058
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_LPLL1_204M8_WTLCP_FORCE_EN                        BIT(31)
#define BIT_AON_PREDIV_CGM_LPLL1_153M6_WTLCP_FORCE_EN                        BIT(30)
#define BIT_AON_PREDIV_CGM_LPLL1_122M88_WTLCP_FORCE_EN                       BIT(29)
#define BIT_AON_PREDIV_CGM_LPLL1_102M4_WTLCP_FORCE_EN                        BIT(28)
#define BIT_AON_PREDIV_CGM_LPLL1_61M44_WTLCP_FORCE_EN                        BIT(27)
#define BIT_AON_PREDIV_CGM_XTL_26M_RF0_WTLCP_FORCE_EN                        BIT(26)
#define BIT_AON_PREDIV_CGM_XTL_26M_RF1_WTLCP_FORCE_EN                        BIT(25)
#define BIT_AON_PREDIV_CGM_LPLL_614M4_PUBCP_FORCE_EN                         BIT(24)
#define BIT_AON_PREDIV_CGM_TWPLL_768M_PUBCP_FORCE_EN                         BIT(23)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_PUBCP_FORCE_EN                         BIT(22)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_PUBCP_FORCE_EN                         BIT(21)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_PUBCP_FORCE_EN                        BIT(20)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_PUBCP_FORCE_EN                         BIT(19)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_PUBCP_FORCE_EN                          BIT(18)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_PUBCP_FORCE_EN                          BIT(17)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_PUBCP_FORCE_EN                         BIT(16)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_PUBCP_FORCE_EN                          BIT(15)
#define BIT_AON_PREDIV_CGM_XTL_26M_PUBCP_FORCE_EN                            BIT(14)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_DISP_FORCE_EN                          BIT(13)
#define BIT_AON_PREDIV_CGM_TWPLL_307M2_DISP_FORCE_EN                         BIT(12)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_DISP_FORCE_EN                          BIT(11)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_DISP_FORCE_EN                          BIT(10)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_DISP_FORCE_EN                         BIT(9)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_DISP_FORCE_EN                          BIT(8)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_DISP_FORCE_EN                           BIT(7)
#define BIT_AON_PREDIV_CGM_XTL_26M_DISP_FORCE_EN                             BIT(6)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_AGCP_FORCE_EN                          BIT(5)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_AGCP_FORCE_EN                          BIT(4)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_AGCP_FORCE_EN                          BIT(3)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_AGCP_FORCE_EN                         BIT(2)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_AGCP_FORCE_EN                          BIT(1)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_AGCP_FORCE_EN                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SW_CTL3_CFG
// Register Offset : 0x005C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_TWPLL_76M8_AGCP_FORCE_EN                          BIT(31)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_AGCP_FORCE_EN                           BIT(30)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_AGCP_FORCE_EN                          BIT(29)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_AGCP_FORCE_EN                           BIT(28)
#define BIT_AON_PREDIV_CGM_XTL_26M_AGCP_FORCE_EN                             BIT(27)
#define BIT_AON_PREDIV_CGM_XTL_26M_GGE_AGCP_FORCE_EN                         BIT(26)
#define BIT_AON_PREDIV_CGM_GPLL_37M_AON_FORCE_EN                             BIT(25)
#define BIT_AON_PREDIV_CGM_LPLL_1228M8_AON_FORCE_EN                          BIT(24)
#define BIT_AON_PREDIV_CGM_LPLL_614M4_AON_FORCE_EN                           BIT(23)
#define BIT_AON_PREDIV_CGM_LPLL_38M_AON_FORCE_EN                             BIT(22)
#define BIT_AON_PREDIV_CGM_TWPLL_1536M_AON_FORCE_EN                          BIT(21)
#define BIT_AON_PREDIV_CGM_TWPLL_768M_AON_FORCE_EN                           BIT(20)
#define BIT_AON_PREDIV_CGM_TWPLL_512M_AON_FORCE_EN                           BIT(19)
#define BIT_AON_PREDIV_CGM_TWPLL_384M_AON_FORCE_EN                           BIT(18)
#define BIT_AON_PREDIV_CGM_TWPLL_307M2_AON_FORCE_EN                          BIT(17)
#define BIT_AON_PREDIV_CGM_TWPLL_256M_AON_FORCE_EN                           BIT(16)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_AON_FORCE_EN                           BIT(15)
#define BIT_AON_PREDIV_CGM_TWPLL_153M6_AON_FORCE_EN                          BIT(14)
#define BIT_AON_PREDIV_CGM_TWPLL_128M_AON_FORCE_EN                           BIT(13)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_AON_FORCE_EN                            BIT(12)
#define BIT_AON_PREDIV_CGM_TWPLL_76M8_AON_FORCE_EN                           BIT(11)
#define BIT_AON_PREDIV_CGM_TWPLL_51M2_AON_FORCE_EN                           BIT(10)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_AON_FORCE_EN                            BIT(9)
#define BIT_AON_PREDIV_CGM_TWPLL_38M4_AON_FORCE_EN                           BIT(8)
#define BIT_AON_PREDIV_CGM_TWPLL_24M_AON_FORCE_EN                            BIT(7)
#define BIT_AON_PREDIV_CGM_TWPLL_19M2_AON_FORCE_EN                           BIT(6)
#define BIT_AON_PREDIV_CGM_TWPLL_12M_AON_FORCE_EN                            BIT(5)
#define BIT_AON_PREDIV_CGM_26M_RF0_AON_FORCE_EN                              BIT(4)
#define BIT_AON_PREDIV_CGM_26M_RF1_AON_FORCE_EN                              BIT(3)
#define BIT_AON_PREDIV_CGM_26M_AON_FORCE_EN                                  BIT(2)
#define BIT_AON_PREDIV_CGM_4M_AON_FORCE_EN                                   BIT(1)
#define BIT_AON_PREDIV_CGM_2M_AON_FORCE_EN                                   BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_GATE_EN_SW_CTL4_CFG
// Register Offset : 0x0060
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_1M_AON_FORCE_EN                                   BIT(5)
#define BIT_AON_PREDIV_CGM_4M_RC_AON_FORCE_EN                                BIT(4)
#define BIT_AON_PREDIV_CGM_2M_RC_AON_FORCE_EN                                BIT(3)
#define BIT_AON_PREDIV_CGM_RCO_100M_AON_FORCE_EN                             BIT(2)
#define BIT_AON_PREDIV_CGM_RCO_25M_AON_FORCE_EN                              BIT(1)
#define BIT_AON_PREDIV_CGM_RCO_4M_AON_FORCE_EN                               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_OUTPUT_CLOCK_MUX_SEL0_CFG
// Register Offset : 0x0064
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_TWPLL_192M_AP_SEL(x)                              (((x) & 0x3) << 30)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_AP_SEL(x)                               (((x) & 0x3) << 28)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_AP_SEL(x)                               (((x) & 0x3) << 26)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_AP_SEL(x)                               (((x) & 0x3) << 24)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_VSP_SEL(x)                              (((x) & 0x3) << 22)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_CAM_SEL(x)                              (((x) & 0x3) << 20)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_CAM_SEL(x)                              (((x) & 0x3) << 18)
#define BIT_AON_PREDIV_CGM_RPLL1_468M_CAM_SEL                                BIT(17)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_WTLCP_SEL(x)                           (((x) & 0x3) << 15)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_WTLCP_SEL(x)                            (((x) & 0x3) << 13)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_WTLCP_SEL(x)                            (((x) & 0x3) << 11)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_WTLCP_SEL(x)                            (((x) & 0x3) << 9)
#define BIT_AON_PREDIV_CGM_XTL_26M_RF0_WTLCP_SEL(x)                          (((x) & 0x3) << 7)
#define BIT_AON_PREDIV_CGM_XTL_26M_RF1_WTLCP_SEL(x)                          (((x) & 0x3) << 5)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_PUBCP_SEL(x)                           (((x) & 0x3) << 3)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_PUBCP_SEL(x)                            (((x) & 0x3) << 1)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_PREDIV_OUTPUT_CLOCK_MUX_SEL1_CFG
// Register Offset : 0x0068
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_PREDIV_CGM_TWPLL_64M_PUBCP_SEL(x)                            (((x) & 0x3) << 23)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_PUBCP_SEL(x)                            (((x) & 0x3) << 21)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_DISP_SEL(x)                            (((x) & 0x3) << 19)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_DISP_SEL(x)                             (((x) & 0x3) << 17)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_AGCP_SEL(x)                            (((x) & 0x3) << 15)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_AGCP_SEL(x)                             (((x) & 0x3) << 13)
#define BIT_AON_PREDIV_CGM_TWPLL_64M_AGCP_SEL(x)                             (((x) & 0x3) << 11)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_AGCP_SEL(x)                             (((x) & 0x3) << 9)
#define BIT_AON_PREDIV_CGM_XTL_26M_GGE_AGCP_SEL(x)                           (((x) & 0x3) << 7)
#define BIT_AON_PREDIV_CGM_LPLL_38M_AON_SEL                                  BIT(6)
#define BIT_AON_PREDIV_CGM_TWPLL_192M_AON_SEL(x)                             (((x) & 0x3) << 4)
#define BIT_AON_PREDIV_CGM_TWPLL_96M_AON_SEL(x)                              (((x) & 0x3) << 2)
#define BIT_AON_PREDIV_CGM_TWPLL_48M_AON_SEL(x)                              (((x) & 0x3))

#endif
