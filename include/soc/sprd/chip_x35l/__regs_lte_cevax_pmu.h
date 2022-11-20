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


#ifndef __H_REGS_LTE_CEVAX_PMU_HEADFILE_H__
#define __H_REGS_LTE_CEVAX_PMU_HEADFILE_H__ __FILE__

#define	REGS_LTE_CEVAX_PMU

/* registers definitions for LTE_CEVAX_PMU */
#define REG_LTE_CEVAX_PMU_PLL_CFG0                        SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0000)
#define REG_LTE_CEVAX_PMU_PLL_CFG1                        SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0004)
#define REG_LTE_CEVAX_PMU_CXCLK_DIV                       SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0008)
#define REG_LTE_CEVAX_PMU_XHCLK_DIV                       SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x000C)
#define REG_LTE_CEVAX_PMU_XPCLK_DIV                       SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0010)
#define REG_LTE_CEVAX_PMU_CXPMOD                          SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0014)
#define REG_LTE_CEVAX_PMU_XHPMOD                          SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0018)
#define REG_LTE_CEVAX_PMU_XAPBMOD                         SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x001C)
#define REG_LTE_CEVAX_PMU_VERSION_CR                      SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0020)
#define REG_LTE_CEVAX_PMU_SLEEP_STATUS                    SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0024)
#define REG_LTE_CEVAX_PMU_POW_CTL0                        SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0028)
#define REG_LTE_CEVAX_PMU_POW_CTL1                        SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x002C)
#define REG_LTE_CEVAX_PMU_POW_CTL2                        SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0030)
#define REG_LTE_CEVAX_PMU_ACCCLK_EN0                      SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0034)
#define REG_LTE_CEVAX_PMU_ACCCLK_EN1                      SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0038)
#define REG_LTE_CEVAX_PMU_ACCCLK_EN2                      SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x003C)
#define REG_LTE_CEVAX_PMU_SOFT_RST0                       SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0040)
#define REG_LTE_CEVAX_PMU_SOFT_RST1                       SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0044)
#define REG_LTE_CEVAX_PMU_DSP_CKG_SEL                     SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0048)
#define REG_LTE_CEVAX_PMU_CLK_DLCH_SEL                    SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0050)
#define REG_LTE_CEVAX_PMU_CLK_ULCH_SEL                    SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0054)
#define REG_LTE_CEVAX_PMU_CLK_RATEM_SEL                   SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0058)
#define REG_LTE_CEVAX_PMU_CLK_LTE_SPI_DIV_SEL             SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x005C)
#define REG_LTE_CEVAX_PMU_CX_CKG_DIV                      SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0078)
#define REG_LTE_CEVAX_PMU_XH_CKG_DIV                      SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x007C)
#define REG_LTE_CEVAX_PMU_XP_CKG_DIV                      SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0080)
#define REG_LTE_CEVAX_PMU_LTE_PMU_CKG_EN                  SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0084)
#define REG_LTE_CEVAX_PMU_LTE_PMU_CLK_EN                  SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0088)
#define REG_LTE_CEVAX_PMU_APB_CLK_SEL0                    SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x008C)
#define REG_LTE_CEVAX_PMU_APB_CLK_SEL1                    SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0090)
#define REG_LTE_CEVAX_PMU_CLK_CXTMR0_SEL                  SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0094)
#define REG_LTE_CEVAX_PMU_CLK_CXTMR1_SEL                  SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0098)
#define REG_LTE_CEVAX_PMU_CLK_CXTMR2_SEL                  SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x009C)
#define REG_LTE_CEVAX_PMU_APB_CLK_SEL2                    SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x00A0)
#define REG_LTE_CEVAX_PMU_MEM_LP_CTRL                     SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0xA4)
#define REG_LTE_CEVAX_PMU_XC321_BOOT                      SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0120)
#define REG_LTE_CEVAX_PMU_XC321_CORE_RST                  SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0124)
#define REG_LTE_CEVAX_PMU_XC321_BVECTOR                   SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0128)
#define REG_LTE_CEVAX_PMU_RES_REG0                        SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0140)
#define REG_LTE_CEVAX_PMU_RES_REG1                        SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0144)
#define REG_LTE_CEVAX_PMU_RES_REG2                        SCI_ADDR(REGS_LTE_CEVAX_PMU_BASE, 0x0148)



/* bits definitions for register REG_LTE_CEVAX_PMU_PLL_CFG0 */
#define BITS_PLL_CFG0_R(_X_)                              ( (_X_) << 11 & (BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BIT_IIS3_EB                                       ( BIT(10) )
#define BIT_IIS2_EB                                       ( BIT(9) )
#define BIT_DSP_DLCH_EN                                   ( BIT(8) )
#define BIT_DSP_ULCH_EN                                   ( BIT(7) )
#define BIT_DLCH_AUTO_EN                                  ( BIT(6) )
#define BIT_ULCH_AUTO_EN                                  ( BIT(5) )
#define BIT_IIS1_EB                                       ( BIT(4) )
#define BIT_IIS0_EB                                       ( BIT(3) )
#define BIT_UART1_EB                                      ( BIT(2) )
#define BIT_UART0_EB                                      ( BIT(1) )
#define BIT_RFFE_EB                                       ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_PLL_CFG1 */
#define BIT_CHIP_SLP_REC_DSP                              ( BIT(16) )
#define BIT_LTE_POW_OFF                                   ( BIT(10) )
#define BIT_LTE_POW_READY                                 ( BIT(9) )
#define BITS_LTE_POW_WAIT(_X_)                            ( (_X_) << 1 & (BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)) )
#define BIT_CHIP_SLP_DSP_CLR                              ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CXCLK_DIV */
#define BITS_CXREG_AUTO_OFF(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_XHCLK_DIV */
#define BIT_BUSMON2_CHN_SEL                               ( BIT(25) )
#define BIT_BUSMON1_CHN_SEL                               ( BIT(24) )
#define BIT_BUSMON0_CHN_SEL                               ( BIT(23) )
#define BITS_DSP_SHM_CTRL(_X_)                            ( (_X_) << 21 & (BIT(21)|BIT(22)) )
#define BIT_DSPAPB_DES_AUTO_EN                            ( BIT(20) )
#define BIT_DSPAHB_DES_AUTO_EN                            ( BIT(19) )
#define BITS_LBUSMON2_CHN_SEL(_X_)                        ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_LBUSMON1_CHN_SEL(_X_)                        ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_LBUSMON0_CHN_SEL(_X_)                        ( (_X_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_DSPAPB_DIV_DES(_X_)                          ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)) )
#define BITS_DSPAHB_DIV_DES(_X_)                          ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)) )
#define BITS_CX_DSPPLL_SEL(_X_)                           ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_XPCLK_DIV */
#define BIT_PMU_DMA_FRC_EB                                ( BIT(24) )
#define BIT_ZBUS_32ACCESS                                 ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CXPMOD */
#define BIT_MCU_FORCE_DEEP_SLEEP                          ( BIT(31) )
#define BIT_LDSP_DEEP_SLP_CTRL                            ( BIT(30) )
#define BIT_EMC_LIGHT_SLEEP_EN_M6                         ( BIT(29) )
#define BIT_EMC_DEEP_SLEEP_EN_M6                          ( BIT(28) )
#define BIT_EMC_LIGHT_SLEEP_EN_M7                         ( BIT(27) )
#define BIT_EMC_DEEP_SLEEP_EN_M7                          ( BIT(26) )
#define BIT_EMC_LIGHT_SLEEP_EN_M8                         ( BIT(25) )
#define BIT_EMC_DEEP_SLEEP_EN_M8                          ( BIT(24) )
#define BIT_LDSP2PUB_ACCESS_EN                            ( BIT(8) )
#define BIT_LDSP_WAKEUP_XTL_EN                            ( BIT(7) )
#define BIT_DMA_LSLP_EN                                   ( BIT(4) )
#define BIT_DSP_LIGHT_SLEEP_EN                            ( BIT(3) )
#define BIT_DSP_DEEP_SLEEP_EN                             ( BIT(2) )
#define BIT_DSP_SYS_SLEEP_EN                              ( BIT(1) )
#define BIT_DSP_CORE_SLEEP                                ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_XHPMOD */
#define BIT_BUSMON2_EB                                    ( BIT(31) )
#define BIT_BUSMON1_EB                                    ( BIT(30) )
#define BIT_BUSMON0_EB                                    ( BIT(29) )
#define BIT_LACCX_AUTO_GATE_EN                            ( BIT(14) )
#define BIT_LDSPX_AUTO_GATE_EN                            ( BIT(13) )
#define BIT_CXDMA_CLK_AUTO_EN                             ( BIT(12) )
#define BIT_CXAPB_AUTO_GATE_EN                            ( BIT(11) )
#define BIT_CXAHB_AUTO_GATE_EN                            ( BIT(10) )
#define BIT_CXDSP_AUTO_GATE_EN                            ( BIT(9) )
#define BIT_CXCORE_AUTO_GATE_EN                           ( BIT(8) )
#define BIT_CXMTX_AUTO_GATE_EN                            ( BIT(7) )
#define BIT_DSP_MAHB_SLEEP_EN                             ( BIT(6) )
#define BIT_ACCZ_ARCH_EB                                  ( BIT(5) )
#define BIT_CXBUS_ARCH_EB                                 ( BIT(4) )
#define BIT_LTEPROC_ARCH_EB                               ( BIT(3) )
#define BIT_PMU_DLCH_SLEEP_R                              ( BIT(1) )
#define BIT_PMU_ULCH_SLEEP_R                              ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_XAPBMOD */
#define BITS_HPROTDMAW(_X_)                               ( (_X_) << 20 & (BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_HPROTDMAR(_X_)                               ( (_X_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_PMU_XPSLEEP_P_R(_X_)                         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_VERSION_CR */
#define BITS_VERSION_CR(_X_)                              (_X_)

/* bits definitions for register REG_LTE_CEVAX_PMU_SLEEP_STATUS */
#define BITS_PMU_SLEEP_STATUS(_X_)                        (_X_)

/* bits definitions for register REG_LTE_CEVAX_PMU_POW_CTL0 */
#define BITS_LTE_ISO_ON_NUM(_X_)                          ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_LTE_ISO_OFF_NUM(_X_)                         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_POW_CTL1 */
#define BIT_CLK_PWR_LTE_SEL                               ( BIT(10) )
#define BIT_LTE_SLP_POWOFF_AUTO_EN                        ( BIT(9) )
#define BIT_LTE_POW_FORCE_PD                              ( BIT(8) )
#define BITS_DSP_MEM_PD_REG(_X_)                          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_POW_CTL2 */
#define BITS_DSP_MEM_PD_EN(_X_)                           ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)) )
#define BITS_PD_LTE_STATUS(_X_)                           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_ACCCLK_EN0 */
#define BIT_FFT_OFFLN_HCLKEN                              ( BIT(30) )
#define BIT_FFT_OFFLN_CLKEN                               ( BIT(29) )
#define BIT_FFT_ONLN_HCLKEN                               ( BIT(28) )
#define BIT_FFT_ONLN_CLKEN                                ( BIT(27) )
#define BIT_LTE_DRM_CLKEN                                 ( BIT(24) )
#define BIT_MIMO_HCLKEN                                   ( BIT(23) )
#define BIT_MIMO_CLKEN                                    ( BIT(22) )
#define BIT_PBCH_ACLKEN                                   ( BIT(21) )
#define BIT_PBCH_HCLKEN                                   ( BIT(20) )
#define BIT_PBCH_CLKEN                                    ( BIT(19) )
#define BIT_PCFICH_PHICH_HCLKEN                           ( BIT(18) )
#define BIT_PCFICH_PHICH_CLKEN                            ( BIT(17) )
#define BIT_PDCCH_ACLKEN                                  ( BIT(16) )
#define BIT_PDCCH_HCLKEN                                  ( BIT(15) )
#define BIT_PDCCH_CLKEN                                   ( BIT(14) )
#define BIT_PDSCH_TB_CTL_HCLKEN                           ( BIT(13) )
#define BIT_PDSCH_TB_CTL_CLKEN                            ( BIT(12) )
#define BIT_RXDFE_HCLKEN                                  ( BIT(11) )
#define BIT_SYNC_ACLKEN                                   ( BIT(10) )
#define BIT_SYNC_HCLKEN                                   ( BIT(9) )
#define BIT_SYNC_CLKEN                                    ( BIT(8) )
#define BIT_TBUF_ACLKEN                                   ( BIT(7) )
#define BIT_TBUF_HCLKEN                                   ( BIT(6) )
#define BIT_TBUF_CLKEN                                    ( BIT(5) )
#define BIT_VTB_CLKEN                                     ( BIT(4) )
#define BIT_LTE_PROC_HCLKEN                               ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_ACCCLK_EN1 */
#define BIT_LBUSMON2_EB                                   ( BIT(28) )
#define BIT_LBUSMON1_EB                                   ( BIT(27) )
#define BIT_LBUSMON0_EB                                   ( BIT(26) )
#define BIT_HSDL_CLKEN                                    ( BIT(25) )
#define BIT_ULMAC_HCLKEN                                  ( BIT(24) )
#define BIT_TBE_HCLKEN                                    ( BIT(23) )
#define BIT_TBE_CLKEN                                     ( BIT(22) )
#define BIT_RM_HCLKEN                                     ( BIT(21) )
#define BIT_RM_CLKEN                                      ( BIT(20) )
#define BIT_PCM_HCLKEN                                    ( BIT(19) )
#define BIT_PCM_CLKEN                                     ( BIT(18) )
#define BIT_UCM_HCLKEN                                    ( BIT(17) )
#define BIT_UCM_CLKEN                                     ( BIT(16) )
#define BIT_CHE_ACLKEN                                    ( BIT(15) )
#define BIT_CHE_HCLKEN                                    ( BIT(14) )
#define BIT_CHE_SYS_CLKEN                                 ( BIT(13) )
#define BIT_CHEPP_ACLKEN                                  ( BIT(12) )
#define BIT_CHEPP_HCLKEN                                  ( BIT(11) )
#define BIT_CHEPP_SYS_CLKEN                               ( BIT(10) )
#define BIT_DBUF_ACLKEN                                   ( BIT(9) )
#define BIT_DBUF_HCLKEN                                   ( BIT(8) )
#define BIT_DBUF_CLKEN                                    ( BIT(7) )
#define BIT_DFE_CLKEN                                     ( BIT(6) )
#define BIT_FBUF_ACLKEN                                   ( BIT(5) )
#define BIT_FBUF_HCLKEN                                   ( BIT(4) )
#define BIT_FBUF_CLKEN                                    ( BIT(3) )
#define BIT_FEC_ACLKEN                                    ( BIT(2) )
#define BIT_FEC_HCLKEN                                    ( BIT(1) )
#define BIT_FEC_CLKEN                                     ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_ACCCLK_EN2 */

/* bits definitions for register REG_LTE_CEVAX_PMU_SOFT_RST0 */
#define BITS_SOFT_RST_R(_X_)                              (_X_)

/* bits definitions for register REG_LTE_CEVAX_PMU_SOFT_RST1 */
#define BIT_LTE_PROC_SOFT_RST_RFT                         ( BIT(31) )
#define BIT_HSDL_SOFT_RST                                 ( BIT(30) )
#define BIT_RFSPI_SOFT_RST                                ( BIT(29) )
#define BIT_LTE_PROC_SOFT_RST                             ( BIT(28) )
#define BIT_ULMAC_SOFT_RST                                ( BIT(27) )
#define BIT_PCM_SOFT_RST                                  ( BIT(26) )
#define BIT_UCM_SOFT_RST                                  ( BIT(25) )
#define BIT_TBE_SOFT_RST                                  ( BIT(24) )
#define BIT_RM_SOFT_RST                                   ( BIT(23) )
#define BIT_RFT_SOFT_RST                                  ( BIT(22) )
#define BIT_DFE_SOFT_RST                                  ( BIT(21) )
#define BIT_MIMO_SOFT_RST                                 ( BIT(20) )
#define BIT_ON_LINE_FFT_SOFT_RST                          ( BIT(19) )
#define BIT_OFF_LINE_FFT_SOFT_RST                         ( BIT(18) )
#define BIT_PCFICH_SOFT_RST                               ( BIT(17) )
#define BIT_PDSCH_SOFT_RST                                ( BIT(16) )
#define BIT_DRM_SOFT_RST                                  ( BIT(15) )
#define BIT_VTB_SOFT_RST                                  ( BIT(14) )
#define BIT_FEC_SOFT_RST                                  ( BIT(13) )
#define BIT_CHE_SOFT_RST                                  ( BIT(12) )
#define BIT_CHEPP_SOFT_RST                                ( BIT(11) )
#define BIT_SYNC_SOFT_RST                                 ( BIT(10) )
#define BIT_TBUF_SOFT_RST                                 ( BIT(9) )
#define BIT_DBUF_SOFT_RST                                 ( BIT(8) )
#define BIT_FBUF_SOFT_RST                                 ( BIT(7) )
#define BIT_PDCCH_SOFT_RST                                ( BIT(6) )
#define BIT_PBCH_SOFT_RST                                 ( BIT(5) )
#define BIT_RXADC_SOFT_RST                                ( BIT(4) )
#define BIT_TXDAC_SOFT_RST                                ( BIT(3) )
#define BIT_CAL_SOFT_RST                                  ( BIT(2) )
#define BIT_RTC_SOFT_RST                                  ( BIT(1) )
#define BIT_LTE_SPI_SOFT_RST                              ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_DSP_CKG_SEL */
#define BITS_DSP_CKG_SEL(_X_)                             ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CLK_DLCH_SEL */
#define BITS_CLK_DLCH_SEL(_X_)                            ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CLK_ULCH_SEL */
#define BITS_CLK_ULCH_SEL(_X_)                            ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CLK_RATEM_SEL */
#define BITS_CLK_RATEM_SEL(_X_)                           ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CLK_LTE_SPI_DIV_SEL */
#define BITS_CLK_LTE_SPI_DIV(_X_)                         ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)) )
#define BITS_CLK_LTE_SPI_SEL(_X_)                         ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CX_CKG_DIV */
#define BITS_CX_CKG_DIV(_X_)                              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_XH_CKG_DIV */
#define BITS_XH_CKG_DIV(_X_)                              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_XP_CKG_DIV */
#define BITS_XP_CKG_DIV(_X_)                              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_LTE_PMU_CKG_EN */
#define BIT_DSP_CKG_EN                                    ( BIT(8) )
#define BIT_CX_CKG_EN                                     ( BIT(6) )
#define BIT_XH_CKG_EN                                     ( BIT(5) )
#define BIT_XP_CKG_EN                                     ( BIT(4) )
#define BIT_DMA_CKG_EN                                    ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_LTE_PMU_CLK_EN */
#define BIT_CLK_LTE_SA_EN                                 ( BIT(17) )
#define BIT_CLK_CXTMR2_EN                                 ( BIT(16) )
#define BIT_CLK_CXTMR1_EN                                 ( BIT(15) )
#define BIT_CLK_CXTMR0_EN                                 ( BIT(14) )
#define BIT_CLK_AXI_EN                                    ( BIT(13) )
#define BIT_CLK_AHB_EN                                    ( BIT(12) )
#define BIT_CLK_DFE_EN                                    ( BIT(11) )
#define BIT_CLK_RFT_EN                                    ( BIT(10) )
#define BIT_CLK_DLCH_EN                                   ( BIT(9) )
#define BIT_CLK_ULCH_EN                                   ( BIT(8) )
#define BIT_CLK_RATEM_EN                                  ( BIT(7) )
#define BIT_CLK_RXADC_EN                                  ( BIT(6) )
#define BIT_CLK_TXDAC_EN                                  ( BIT(5) )
#define BIT_CLK_LTE_ANA_2X_EN                             ( BIT(4) )
#define BIT_CLK_LTE_ANA_1X_EN                             ( BIT(3) )
#define BIT_CLK_CAL_EN                                    ( BIT(2) )
#define BIT_CLK_RTC_EN                                    ( BIT(1) )
#define BIT_CLK_LTE_SPI_EN                                ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_APB_CLK_SEL0 */
#define BITS_CLK_UART1_DIV(_X_)                           ( (_X_) << 7 & (BIT(7)|BIT(8)|BIT(9)) )
#define BITS_CLK_UART1_SEL(_X_)                           ( (_X_) << 5 & (BIT(5)|BIT(6)) )
#define BITS_CLK_UART0_DIV(_X_)                           ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)) )
#define BITS_CLK_UART0_SEL(_X_)                           ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_APB_CLK_SEL1 */
#define BITS_CLK_IIS1_DIV(_X_)                            ( (_X_) << 18 & (BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_CLK_IIS1_SEL(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_CLK_IIS0_DIV(_X_)                            ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CLK_IIS0_SEL(_X_)                            ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CLK_CXTMR0_SEL */
#define BITS_CLK_CXTMR0_SEL(_X_)                          ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CLK_CXTMR1_SEL */
#define BITS_CLK_CXTMR1_SEL(_X_)                          ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_CLK_CXTMR2_SEL */
#define BITS_CLK_CXTMR2_SEL(_X_)                          ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_APB_CLK_SEL2 */
#define BITS_CLK_IIS3_DIV(_X_)                            ( (_X_) << 18 & (BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)) )
#define BITS_CLK_IIS3_SEL(_X_)                            ( (_X_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_CLK_IIS2_DIV(_X_)                            ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_CLK_IIS2_SEL(_X_)                            ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_LTE_CEVAX_PMU_MEM_LP_CTRL */
#define BITS_MEM_LP_CTRL(_X_)                             (_X_)
/* bits definitions for register REG_LTE_CEVAX_PMU_XC321_BOOT */
#define BIT_FRC_CLK_XC321_EN                              ( BIT(1) )
#define BIT_XC321_BOOT                                    ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_XC321_CORE_RST */
#define BIT_XC321_SYS_SRST                                ( BIT(1) )
#define BIT_XC321_CORE_SRST_N                             ( BIT(0) )

/* bits definitions for register REG_LTE_CEVAX_PMU_XC321_BVECTOR */
#define BITS_XC321_BOOT_VECTOR(_X_)                       (_X_)

/* bits definitions for register REG_LTE_CEVAX_PMU_RES_REG0 */
#define BITS_RES_REG0(_X_)                                (_X_)

/* bits definitions for register REG_LTE_CEVAX_PMU_RES_REG1 */
#define BITS_RES_REG1(_X_)                                (_X_)

/* bits definitions for register REG_LTE_CEVAX_PMU_RES_REG2 */
#define BITS_RES_REG2(_X_)                                (_X_)

#endif
