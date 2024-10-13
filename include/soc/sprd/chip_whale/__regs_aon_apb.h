/* the head file modifier:     g   2015-03-26 15:03:24*/

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


#ifndef __H_REGS_AON_APB_HEADFILE_H__
#define __H_REGS_AON_APB_HEADFILE_H__ __FILE__

#define  REGS_AON_APB_BASE  SPRD_AONAPB_BASE

#define REG_AON_APB_APB_EB0                      SCI_ADDR(REGS_AON_APB_BASE, 0x0000 )
#define REG_AON_APB_APB_EB1                      SCI_ADDR(REGS_AON_APB_BASE, 0x0004 )
#define REG_AON_APB_APB_RST0                     SCI_ADDR(REGS_AON_APB_BASE, 0x0008 )
#define REG_AON_APB_APB_RST1                     SCI_ADDR(REGS_AON_APB_BASE, 0x000C )
#define REG_AON_APB_APB_RTC_EB                   SCI_ADDR(REGS_AON_APB_BASE, 0x0010 )
#define REG_AON_APB_PWR_CTRL                     SCI_ADDR(REGS_AON_APB_BASE, 0x0024 )
#define REG_AON_APB_TS_CFG                       SCI_ADDR(REGS_AON_APB_BASE, 0x0028 )
#define REG_AON_APB_BOOT_MODE                    SCI_ADDR(REGS_AON_APB_BASE, 0x002C )
#define REG_AON_APB_BB_BG_CTRL                   SCI_ADDR(REGS_AON_APB_BASE, 0x0030 )
#define REG_AON_APB_CP_ARM_JTAG_CTRL             SCI_ADDR(REGS_AON_APB_BASE, 0x0034 )
#define REG_AON_APB_DCXO_LC_REG0                 SCI_ADDR(REGS_AON_APB_BASE, 0x003C )
#define REG_AON_APB_DCXO_LC_REG1                 SCI_ADDR(REGS_AON_APB_BASE, 0x0040 )
#define REG_AON_APB_CA53_AUTO_CLKCTRL_DIS        SCI_ADDR(REGS_AON_APB_BASE, 0x0048 )
#define REG_AON_APB_AGCP_BOOT_PROT               SCI_ADDR(REGS_AON_APB_BASE, 0x0078 )
#define REG_AON_APB_AON_REG_PROT                 SCI_ADDR(REGS_AON_APB_BASE, 0x007C )
#define REG_AON_APB_DAP_DJTAG_SEL                SCI_ADDR(REGS_AON_APB_BASE, 0x0084 )
#define REG_AON_APB_USER_RSV_FLAG1               SCI_ADDR(REGS_AON_APB_BASE, 0x0088 )
#define REG_AON_APB_AON_CGM_CFG                  SCI_ADDR(REGS_AON_APB_BASE, 0x0098 )
#define REG_AON_APB_AON_CHIP_ID0                 SCI_ADDR(REGS_AON_APB_BASE, 0x00F8 )
#define REG_AON_APB_AON_CHIP_ID1                 SCI_ADDR(REGS_AON_APB_BASE, 0x00FC )
#define REG_AON_APB_CCIR_RCVR_CFG                SCI_ADDR(REGS_AON_APB_BASE, 0x0100 )
#define REG_AON_APB_PLL_BG_CFG                   SCI_ADDR(REGS_AON_APB_BASE, 0x0108 )
#define REG_AON_APB_LVDSDIS_SEL                  SCI_ADDR(REGS_AON_APB_BASE, 0x010C )
#define REG_AON_APB_EMC_AUTO_GATE_EN             SCI_ADDR(REGS_AON_APB_BASE, 0x0120 )
#define REG_AON_APB_SP_CFG_BUS                   SCI_ADDR(REGS_AON_APB_BASE, 0x0124 )
#define REG_AON_APB_RTC4M_0_CFG                  SCI_ADDR(REGS_AON_APB_BASE, 0x0128 )
#define REG_AON_APB_APB_RST2                     SCI_ADDR(REGS_AON_APB_BASE, 0x0130 )
#define REG_AON_APB_RC100M_CFG                   SCI_ADDR(REGS_AON_APB_BASE, 0x0134 )
#define REG_AON_APB_CGM_REG1                     SCI_ADDR(REGS_AON_APB_BASE, 0x0138 )
#define REG_AON_APB_CGM_CLK_TOP_REG1             SCI_ADDR(REGS_AON_APB_BASE, 0x013C )
#define REG_AON_APB_AGCP_DSP_CTRL0               SCI_ADDR(REGS_AON_APB_BASE, 0x0140 )
#define REG_AON_APB_AGCP_DSP_CTRL1               SCI_ADDR(REGS_AON_APB_BASE, 0x0144 )
#define REG_AON_APB_AGCP_SOFT_RESET              SCI_ADDR(REGS_AON_APB_BASE, 0x0148 )
#define REG_AON_APB_AGCP_CTRL                    SCI_ADDR(REGS_AON_APB_BASE, 0x014C )
#define REG_AON_APB_WTLCP_LDSP_CTRL0             SCI_ADDR(REGS_AON_APB_BASE, 0x0150 )
#define REG_AON_APB_WTLCP_LDSP_CTRL1             SCI_ADDR(REGS_AON_APB_BASE, 0x0154 )
#define REG_AON_APB_WTLCP_TDSP_CTRL0             SCI_ADDR(REGS_AON_APB_BASE, 0x0158 )
#define REG_AON_APB_WTLCP_TDSP_CTRL1             SCI_ADDR(REGS_AON_APB_BASE, 0x015C )
#define REG_AON_APB_WTLCP_SOFT_RESET             SCI_ADDR(REGS_AON_APB_BASE, 0x0160 )
#define REG_AON_APB_WTLCP_CTRL                   SCI_ADDR(REGS_AON_APB_BASE, 0x0164 )
#define REG_AON_APB_WTL_WCDMA_EB                 SCI_ADDR(REGS_AON_APB_BASE, 0x0168 )
#define REG_AON_APB_PCP_AON_EB                   SCI_ADDR(REGS_AON_APB_BASE, 0x0170 )
#define REG_AON_APB_PCP_SOFT_RST                 SCI_ADDR(REGS_AON_APB_BASE, 0x0174 )
#define REG_AON_APB_PUBCP_CTRL                   SCI_ADDR(REGS_AON_APB_BASE, 0x0178 )
#define REG_AON_APB_USB3_CTRL                    SCI_ADDR(REGS_AON_APB_BASE, 0x0180 )
#define REG_AON_APB_DEBUG_U2PMU                  SCI_ADDR(REGS_AON_APB_BASE, 0x0184 )
#define REG_AON_APB_DEBUG_U3PMU                  SCI_ADDR(REGS_AON_APB_BASE, 0x0188 )
#define REG_AON_APB_FIREWALL_CLK_EN              SCI_ADDR(REGS_AON_APB_BASE, 0x0190 )
#define REG_AON_APB_GPU_CTRL                     SCI_ADDR(REGS_AON_APB_BASE, 0x0194 )
#define REG_AON_APB_A53_CTRL_0                   SCI_ADDR(REGS_AON_APB_BASE, 0x01A0 )
#define REG_AON_APB_A53_CTRL_1                   SCI_ADDR(REGS_AON_APB_BASE, 0x01A4 )
#define REG_AON_APB_A53_BIGCORE_VDROP_RSV        SCI_ADDR(REGS_AON_APB_BASE, 0x01A8 )
#define REG_AON_APB_A53_LITCORE_VDROP_RSV        SCI_ADDR(REGS_AON_APB_BASE, 0x01AC )
#define REG_AON_APB_A53_CTRL_2                   SCI_ADDR(REGS_AON_APB_BASE, 0x01B0 )
#define REG_AON_APB_A53_CTRL_3                   SCI_ADDR(REGS_AON_APB_BASE, 0x01B4 )
#define REG_AON_APB_A53_CTRL_4                   SCI_ADDR(REGS_AON_APB_BASE, 0x01B8 )
#define REG_AON_APB_A53_CTRL_5                   SCI_ADDR(REGS_AON_APB_BASE, 0x01BC )
#define REG_AON_APB_SUB_SYS_DEBUG_BUS_SEL_0      SCI_ADDR(REGS_AON_APB_BASE, 0x01C0 )
#define REG_AON_APB_SUB_SYS_DEBUG_BUS_SEL_1      SCI_ADDR(REGS_AON_APB_BASE, 0x01C4 )
#define REG_AON_APB_GLB_DEBUG_BUS_SEL            SCI_ADDR(REGS_AON_APB_BASE, 0x01C8 )
#define REG_AON_APB_SOFT_RST_AON_ADD1            SCI_ADDR(REGS_AON_APB_BASE, 0x01CC )
#define REG_AON_APB_EB_AON_ADD1                  SCI_ADDR(REGS_AON_APB_BASE, 0x01D0 )
#define REG_AON_APB_DBG_DJTAG_CTRL               SCI_ADDR(REGS_AON_APB_BASE, 0x01D4 )
#define REG_AON_APB_MPLL0_CTRL                   SCI_ADDR(REGS_AON_APB_BASE, 0x01D8 )
#define REG_AON_APB_MPLL1_CTRL                   SCI_ADDR(REGS_AON_APB_BASE, 0x01DC )
#define REG_AON_APB_PUB_FC_CTRL                  SCI_ADDR(REGS_AON_APB_BASE, 0x01E0 )
#define REG_AON_APB_IO_DLY_CTRL                  SCI_ADDR(REGS_AON_APB_BASE, 0x3014 )
#define REG_AON_APB_WDG_RST_FLAG                 SCI_ADDR(REGS_AON_APB_BASE, 0x3024 )
#define REG_AON_APB_PMU_RST_MONITOR              SCI_ADDR(REGS_AON_APB_BASE, 0x302C )
#define REG_AON_APB_THM_RST_MONITOR              SCI_ADDR(REGS_AON_APB_BASE, 0x3030 )
#define REG_AON_APB_AP_RST_MONITOR               SCI_ADDR(REGS_AON_APB_BASE, 0x3034 )
#define REG_AON_APB_CA53_RST_MONITOR             SCI_ADDR(REGS_AON_APB_BASE, 0x3038 )
#define REG_AON_APB_BOND_OPT0                    SCI_ADDR(REGS_AON_APB_BASE, 0x303C )
#define REG_AON_APB_BOND_OPT1                    SCI_ADDR(REGS_AON_APB_BASE, 0x3040 )
#define REG_AON_APB_RES_REG0                     SCI_ADDR(REGS_AON_APB_BASE, 0x3044 )
#define REG_AON_APB_RES_REG1                     SCI_ADDR(REGS_AON_APB_BASE, 0x3048 )
#define REG_AON_APB_AON_QOS_CFG                  SCI_ADDR(REGS_AON_APB_BASE, 0x304C )
#define REG_AON_APB_AON_MTX_PROT_CFG             SCI_ADDR(REGS_AON_APB_BASE, 0x3058 )
#define REG_AON_APB_PLL_LOCK_OUT_SEL             SCI_ADDR(REGS_AON_APB_BASE, 0x3064 )
#define REG_AON_APB_RTC4M_RC_VAL                 SCI_ADDR(REGS_AON_APB_BASE, 0x3068 )
#define REG_AON_APB_DDR_STAT_FLAG                SCI_ADDR(REGS_AON_APB_BASE, 0x306C )
#define REG_AON_APB_AON_MODEL_STAT               SCI_ADDR(REGS_AON_APB_BASE, 0x3070 )
#define REG_AON_APB_USER_COMM_FLAG               SCI_ADDR(REGS_AON_APB_BASE, 0x3074 )
#define REG_AON_APB_AON_MAIN_MTX_QOS1            SCI_ADDR(REGS_AON_APB_BASE, 0x3078 )
#define REG_AON_APB_AON_MAIN_MTX_QOS2            SCI_ADDR(REGS_AON_APB_BASE, 0x307C )
#define REG_AON_APB_RC100M_RC_VAL                SCI_ADDR(REGS_AON_APB_BASE, 0x3080 )
#define REG_AON_APB_AON_APB_RSV                  SCI_ADDR(REGS_AON_APB_BASE, 0x30F0 )
#define REG_AON_APB_GLB_CLK_AUTO_GATE_EN         SCI_ADDR(REGS_AON_APB_BASE, 0x30F4 )
#define REG_AON_APB_AON_OSC_FUNC_CNT             SCI_ADDR(REGS_AON_APB_BASE, 0x3100 )
#define REG_AON_APB_AON_OSC_FUNC_CTRL            SCI_ADDR(REGS_AON_APB_BASE, 0x3104 )
#define REG_AON_APB_DEEP_SLEEP_MUX_SEL           SCI_ADDR(REGS_AON_APB_BASE, 0x3108 )
#define REG_AON_APB_NIU_IDLE_DET_DISABLE         SCI_ADDR(REGS_AON_APB_BASE, 0x310C )
#define REG_AON_APB_FUNC_TEST_BOOT_ADDR          SCI_ADDR(REGS_AON_APB_BASE, 0x3110 )
#define REG_AON_APB_CGM_RESCUE                   SCI_ADDR(REGS_AON_APB_BASE, 0x3114 )
#define REG_AON_APB_AON_PLAT_ID0                 SCI_ADDR(REGS_AON_APB_BASE, 0x3118 )
#define REG_AON_APB_AON_PLAT_ID1                 SCI_ADDR(REGS_AON_APB_BASE, 0x311C )
#define REG_AON_APB_AON_VER_ID                   SCI_ADDR(REGS_AON_APB_BASE, 0x3120 )
#define REG_AON_APB_AON_MFT_ID                   SCI_ADDR(REGS_AON_APB_BASE, 0x3124 )
#define REG_AON_APB_AON_MPL_ID                   SCI_ADDR(REGS_AON_APB_BASE, 0x3128 )
#define REG_AON_APB_VSP_NOC_CTRL                 SCI_ADDR(REGS_AON_APB_BASE, 0x315C )
#define REG_AON_APB_CAM_NOC_CTRL                 SCI_ADDR(REGS_AON_APB_BASE, 0x3160 )
#define REG_AON_APB_DISP_NOC_CTRL                SCI_ADDR(REGS_AON_APB_BASE, 0x3164 )
#define REG_AON_APB_GSP_NOC_CTRL                 SCI_ADDR(REGS_AON_APB_BASE, 0x3168 )
#define REG_AON_APB_WTL_CP_NOC_CTRL              SCI_ADDR(REGS_AON_APB_BASE, 0x316C )
#define REG_AON_APB_LACC_NOC_CTRL                SCI_ADDR(REGS_AON_APB_BASE, 0x3170 )
#define REG_AON_APB_AGCP_NOC_CTRL                SCI_ADDR(REGS_AON_APB_BASE, 0x3174 )
#define REG_AON_APB_PCP_NOC_CTRL                 SCI_ADDR(REGS_AON_APB_BASE, 0x3178 )
#define REG_AON_APB_WTL_CP_RFFE_CTRL_SEL         SCI_ADDR(REGS_AON_APB_BASE, 0x3180 )
#define REG_AON_APB_SP_WAKEUP_MASK_EN1           SCI_ADDR(REGS_AON_APB_BASE, 0x3184 )
#define REG_AON_APB_SP_WAKEUP_MASK_EN2           SCI_ADDR(REGS_AON_APB_BASE, 0x3188 )

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_APB_EB0
// Register Offset : 0x0000
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_I2C_EB                                        BIT(31)
#define BIT_AON_APB_CA53_DAP_EB                                   BIT(30)
#define BIT_AON_APB_CA53_TS1_EB                                   BIT(29)
#define BIT_AON_APB_CA53_TS0_EB                                   BIT(28)
#define BIT_AON_APB_GPU_EB                                        BIT(27)
#define BIT_AON_APB_CKG_EB                                        BIT(26)
#define BIT_AON_APB_PIN_EB                                        BIT(25)
#define BIT_AON_APB_MSPI_EB                                       BIT(23)
#define BIT_AON_APB_SPLK_EB                                       BIT(22)
#define BIT_SPLK_EB                                   			  BIT_AON_APB_SPLK_EB
#define BIT_AON_APB_AP_INTC4_EB                                   BIT(21)
#define BIT_AON_APB_AP_INTC3_EB                                   BIT(20)
#define BIT_AON_APB_AP_INTC2_EB                                   BIT(19)
#define BIT_AON_APB_AP_INTC1_EB                                   BIT(18)
#define BIT_AON_APB_AP_INTC0_EB                                   BIT(17)
#define BIT_ADI_EB                                                BIT_AON_APB_ADI_EB
#define BIT_AON_APB_ADI_EB                                        BIT(16)
#define BIT_AON_APB_EIC_EB                                        BIT(14)
#define BIT_EFUSE_EB                                              BIT_AON_APB_EFUSE_EB
#define BIT_AON_APB_EFUSE_EB                                      BIT(13)
#define BIT_AON_APB_AP_TMR0_EB                                    BIT(12)
#define BIT_AON_APB_AON_TMR_EB                                    BIT(11)
#define BIT_AON_APB_AP_SYST_EB                                    BIT(10)
#define BIT_AON_APB_AON_SYST_EB                                   BIT(9)
#define BIT_AON_APB_KPD_EB                                        BIT(8)
#define BIT_AON_APB_PWM3_EB                                       BIT(7)
#define BIT_AON_APB_PWM2_EB                                       BIT(6)
#define BIT_AON_APB_PWM1_EB                                       BIT(5)
#define BIT_AON_APB_PWM0_EB                                       BIT(4)
#define BIT_AON_APB_GPIO_EB                                       BIT(3)
#define BIT_AON_APB_AP_INTC5_EB                                   BIT(2)
#define BIT_AON_APB_AVS_CA53_BIG_EB                               BIT(1)
#define BIT_AON_APB_AVS_CA53_LIT_EB                               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_APB_EB1
// Register Offset : 0x0004
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DBG_AXI_IF_EB                                 BIT(31)
#define BIT_AON_APB_DISP_EB                                       BIT(30)
#define BIT_AON_APB_CAM_EB                                        BIT(29)
#define BIT_AON_APB_VSP_EB                                        BIT(28)
#define BIT_AON_APB_ORP_JTAG_EB                                   BIT(27)
#define BIT_AON_APB_DEF_EB                                        BIT(25)
#define BIT_AON_APB_LVDS_PLL_DIV_EN                               BIT(24)
#define BIT_AON_APB_AON_DMA_EB                                    BIT(22)
#define BIT_AON_DMA_EB                                            BIT_AON_APB_AON_DMA_EB
#define BIT_AON_APB_MBOX_EB                                       BIT(21)
#define BIT_AON_APB_DJTAG_EB                                      BIT(20)
#define BIT_AON_APB_RC100M_CAL_EB                                 BIT(19)
#define BIT_AON_APB_RTC4M0_CAL_EB                                 BIT(18)
#define BIT_AON_APB_MDAR_EB                                       BIT(17)
#define BIT_AON_APB_LVDS_TCXO_EB                                  BIT(16)
#define BIT_AON_APB_LVDS_TRX_EB                                   BIT(15)
#define BIT_AON_APB_OSC_AON_TOP_EB                                BIT(14)
#define BIT_AON_APB_GSP_EMC_EB                                    BIT(13)
#define BIT_AON_APB_ZIP_EMC_EB                                    BIT(12)
#define BIT_AON_APB_DISP_EMC_EB                                   BIT(11)
#define BIT_AON_APB_AP_TMR2_EB                                    BIT(10)
#define BIT_AON_APB_AP_TMR1_EB                                    BIT(9)
#define BIT_AP_TMR1_EB                                            BIT_AON_APB_AP_TMR1_EB
#define BIT_AP_TMR2_EB                                            BIT_AON_APB_AP_TMR2_EB
#define BIT_AON_APB_CA53_WDG_EB                                   BIT(8)
#define BIT_CA7_WDG_EB                                            BIT_AON_APB_CA53_WDG_EB
#define BIT_AON_APB_AVS_GPU1_EB                                   BIT(7)
#define BIT_AON_APB_AVS_GPU0_EB                                   BIT(6)
#define BIT_AON_APB_PROBE_EB                                      BIT(5)
#define BIT_AON_APB_AUX2_EB                                       BIT(4)
#define BIT_AON_APB_AUX1_EB                                       BIT(3)
#define BIT_AON_APB_AUX0_EB                                       BIT(2)
#define BIT_AON_APB_THM_EB                                        BIT(1)
#define BIT_THM_EB                                                BIT_AON_APB_THM_EB
#define BIT_AON_APB_PMU_EB                                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_APB_RST0
// Register Offset : 0x0008
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_I2C_SOFT_RST                                  BIT(30)
#define BIT_AON_APB_CA53_TS1_SOFT_RST                             BIT(29)
#define BIT_AON_APB_CA53_TS0_SOFT_RST                             BIT(28)
#define BIT_AON_APB_DAP_MTX_SOFT_RST                              BIT(27)
#define BIT_AON_APB_MSPI1_SOFT_RST                                BIT(26)
#define BIT_AON_APB_MSPI0_SOFT_RST                                BIT(25)
#define BIT_AON_APB_SPLK_SOFT_RST                                 BIT(24)
#define BIT_AON_APB_CKG_SOFT_RST                                  BIT(23)
#define BIT_AON_APB_PIN_SOFT_RST                                  BIT(22)
#define BIT_AON_APB_AP_INTC3_SOFT_RST                             BIT(21)
#define BIT_AON_APB_AP_INTC2_SOFT_RST                             BIT(20)
#define BIT_AON_APB_AP_INTC1_SOFT_RST                             BIT(19)
#define BIT_AON_APB_AP_INTC0_SOFT_RST                             BIT(18)
#define BIT_AON_APB_ADI_SOFT_RST                                  BIT(17)
#define BIT_AON_APB_EIC_SOFT_RST                                  BIT(15)
#define BIT_EFUSE_SOFT_RST                                        BIT_AON_APB_EFUSE_SOFT_RST
#define BIT_AON_APB_EFUSE_SOFT_RST                                BIT(14)
#define BIT_AON_APB_AP_TMR0_SOFT_RST                              BIT(12)
#define BIT_AON_APB_AON_TMR_SOFT_RST                              BIT(11)
#define BIT_AON_APB_AP_SYST_SOFT_RST                              BIT(10)
#define BIT_AON_APB_AON_SYST_SOFT_RST                             BIT(9)
#define BIT_AON_APB_KPD_SOFT_RST                                  BIT(8)
#define BIT_AON_APB_PWM3_SOFT_RST                                 BIT(7)
#define BIT_AON_APB_PWM2_SOFT_RST                                 BIT(6)
#define BIT_AON_APB_PWM1_SOFT_RST                                 BIT(5)
#define BIT_AON_APB_PWM0_SOFT_RST                                 BIT(4)
#define BIT_AON_APB_GPIO_SOFT_RST                                 BIT(3)
#define BIT_AON_APB_AP_INTC4_SOFT_RST                             BIT(2)
#define BIT_AON_APB_AP_INTC5_SOFT_RST                             BIT(1)
#define BIT_AON_APB_RTC4M_ANA_SOFT_RST                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_APB_RST1
// Register Offset : 0x000C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AVS_CA53_BIG_SOFT_RST                         BIT(31)
#define BIT_AON_APB_AVS_CA53_LIT_SOFT_RST                         BIT(30)
#define BIT_AON_APB_DEF_SLV_INT_SOFT_CLR                          BIT(29)
#define BIT_AON_APB_DEF_SOFT_RST                                  BIT(28)
#define BIT_AON_APB_MBOX_SOFT_RST                                 BIT(24)
#define BIT_AON_APB_RC100M_CAL_SOFT_RST                           BIT(23)
#define BIT_AON_APB_RTC4M0_CAL_SOFT_RST                           BIT(22)
#define BIT_AON_APB_ADC3_CAL_SOFT_RST                             BIT(16)
#define BIT_AON_APB_ADC2_CAL_SOFT_RST                             BIT(15)
#define BIT_AON_APB_ADC1_CAL_SOFT_RST                             BIT(14)
#define BIT_AON_APB_MDAR_SOFT_RST                                 BIT(13)
#define BIT_AON_APB_LVDSDIS_SOFT_RST                              BIT(12)
#define BIT_AON_APB_BB_CAL_SOFT_RST                               BIT(11)
#define BIT_AON_APB_DCXO_LC_SOFT_RST                              BIT(10)
#define BIT_AON_APB_AP_TMR2_SOFT_RST                              BIT(9)
#define BIT_AON_APB_AP_TMR1_SOFT_RST                              BIT(8)
#define BIT_AON_APB_CA53_WDG_SOFT_RST                             BIT(7)
#define BIT_AON_APB_AON_DMA_SOFT_RST                              BIT(6)
#define BIT_AON_APB_GPU_THMA_SOFT_RST                             BIT(3)
#define BIT_AON_APB_ARM_THMA_SOFT_RST                             BIT(2)
#define BIT_AON_APB_THM_SOFT_RST                                  BIT(1)
#define BIT_AON_APB_PMU_SOFT_RST                                  BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_APB_RTC_EB
// Register Offset : 0x0010
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_RTCCNT_RTCDV10_EB                             BIT(27)
#define BIT_AON_APB_GPU_TS_EB                                     BIT(24)
#define BIT_AON_APB_AVS_GPU1_RTC_EB                               BIT(23)
#define BIT_AON_APB_AVS_GPU0_RTC_EB                               BIT(22)
#define BIT_AON_APB_AVS_CA53_LIT_RTC_EB                           BIT(21)
#define BIT_AON_APB_AVS_CA53_BIG_RTC_EB                           BIT(20)
#define BIT_AON_APB_BB_CAL_RTC_EB                                 BIT(18)
#define BIT_AON_APB_DCXO_LC_RTC_EB                                BIT(17)
#define BIT_AON_APB_AP_TMR2_RTC_EB                                BIT(16)
#define BIT_AON_APB_AP_TMR1_RTC_EB                                BIT(15)
#define BIT_AON_APB_GPU_THMA_RTC_AUTO_EN                          BIT(14)
#define BIT_AON_APB_ARM_THMA_RTC_AUTO_EN                          BIT(13)
#define BIT_AON_APB_GPU_THMA_RTC_EB                               BIT(12)
#define BIT_AON_APB_ARM_THMA_RTC_EB                               BIT(11)
#define BIT_AON_APB_THM_RTC_EB                                    BIT(10)
#define BIT_AON_APB_CA53_WDG_RTC_EB                               BIT(9)
#define BIT_CA7_WDG_RTC_EB                                        BIT_AON_APB_CA53_WDG_RTC_EB
#define BIT_AON_APB_EIC_RTCDV5_EB                                 BIT(7)
#define BIT_AON_APB_EIC_RTC_EB                                    BIT(6)
#define BIT_AON_APB_AP_TMR0_RTC_EB                                BIT(5)
#define BIT_AON_APB_AON_TMR_RTC_EB                                BIT(4)
#define BIT_AON_APB_AP_SYST_RTC_EB                                BIT(3)
#define BIT_AON_APB_AON_SYST_RTC_EB                               BIT(2)
#define BIT_AON_APB_KPD_RTC_EB                                    BIT(1)
#define BIT_AON_APB_ARCH_RTC_EB                                   BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_PWR_CTRL
// Register Offset : 0x0024
// Description     : 
---------------------------------------------------------------------------*/
#define BIT_CSI1_PHY_PD                                   ( BIT(11) )
#define BIT_CSI0_PHY_PD                                   ( BIT(10) )

#define BIT_AON_APB_CA53_TS1_STOP                                 BIT(9)
#define BIT_AON_APB_CA53_TS0_STOP                                 BIT(8)
#define BIT_AON_APB_EFUSE_BIST_PWR_ON                             BIT(3)
#define BIT_AON_APB_EFUSE_PWON_RD_END_FLAG                        BIT(2)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_TS_CFG
// Register Offset : 0x0028
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DBG_TRACE_CTRL_EN                             BIT(16)
#define BIT_AON_APB_EVENTACK_RESTARTREQ_TS01                      BIT(4)
#define BIT_AON_APB_EVENT_RESTARTREQ_TS01                         BIT(1)
#define BIT_AON_APB_EVENT_HALTREQ_TS01                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_BOOT_MODE
// Register Offset : 0x002C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_PTEST_FUNC_ATSPEED_SEL                        BIT(8)
#define BIT_AON_APB_PTEST_FUNC_MODE                               BIT(7)
#define BIT_AON_APB_FUNCTST_DMA_EB                                BIT(6)
#define BIT_AON_APB_PTEST_BIST_MODE                               BIT(5)
#define BIT_AON_APB_USB_DLOAD_EN                                  BIT(4)
#define BIT_AON_APB_ARM_BOOT_MD3                                  BIT(3)
#define BIT_AON_APB_ARM_BOOT_MD2                                  BIT(2)
#define BIT_AON_APB_ARM_BOOT_MD1                                  BIT(1)
#define BIT_AON_APB_ARM_BOOT_MD0                                  BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_BB_BG_CTRL
// Register Offset : 0x0030
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_BB_BG_AUTO_PD_EN                              BIT(3)
#define BIT_AON_APB_BB_BG_SLP_PD_EN                               BIT(2)
#define BIT_AON_APB_BB_BG_FORCE_ON                                BIT(1)
#define BIT_AON_APB_BB_BG_FORCE_PD                                BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_CP_ARM_JTAG_CTRL
// Register Offset : 0x0034
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CP_ARM_JTAG_PIN_SEL(x)                        (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_DCXO_LC_REG0
// Register Offset : 0x003C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DCXO_LC_FLAG                                  BIT(8)
#define BIT_AON_APB_DCXO_LC_FLAG_CLR                              BIT(1)
#define BIT_AON_APB_DCXO_LC_CNT_CLR                               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_DCXO_LC_REG1
// Register Offset : 0x0040
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DCXO_LC_CNT(x)                                (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_CA53_AUTO_CLKCTRL_DIS
// Register Offset : 0x0048
// Description     : for CA53 lit,auto clk ctrl disable
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CCI_AUTO_CLK_CTRL_DISABLE                     BIT(3)
#define BIT_AON_APB_NIC_AUTO_CLK_CTRL_DISABLE                     BIT(2)
#define BIT_AON_APB_DAP2CCI_AUTO_CLK_CTRL_DISABLE                 BIT(1)
#define BIT_AON_APB_NIU_AUTO_CLK_CTRL_DISABLE                     BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AGCP_BOOT_PROT
// Register Offset : 0x0078
// Description     : protect register for AGCP
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AGCP_BOOTCTRL_PROT                            BIT(31)
#define BIT_AON_APB_AGCP_REG_PROT_VAL(x)                          (((x) & 0xFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_REG_PROT
// Register Offset : 0x007C
// Description     : protect register for LDSP
---------------------------------------------------------------------------*/

#define BIT_AON_APB_LDSP_CTRL_PROT                                BIT(31)
#define BIT_AON_APB_REG_PROT_VAL(x)                               (((x) & 0xFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_DAP_DJTAG_SEL
// Register Offset : 0x0084
// Description     : DAP_DJTAG_SEL
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DAP_DBGPWRUP_SOFT_EN                          BIT(2)
#define BIT_AON_APB_DAP_SYSPWRUP_SOFT_EN                          BIT(1)
#define BIT_AON_APB_DAP_DJTAG_EN                                  BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_USER_RSV_FLAG1
// Register Offset : 0x0088
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_USER_RSV_FLAG1_B1                             BIT(1)
#define BIT_AON_APB_USER_RSV_FLAG1_B0                             BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_CGM_CFG
// Register Offset : 0x0098
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_EMC_DDR1_CKG_SEL_CURRENT(x)                   (((x) & 0x7F) << 24)
#define BIT_AON_APB_CLK_DMC1_SEL_HW_EN                            BIT(23)
#define BIT_AON_APB_CLK_DMC1_2X_DIV(x)                            (((x) & 0x7) << 20)
#define BIT_AON_APB_CLK_DMC1_1X_DIV                               BIT(19)
#define BIT_AON_APB_CLK_DMC1_SEL(x)                               (((x) & 0x7) << 16)
#define BIT_AON_APB_EMC_DDR0_CKG_SEL_CURRENT(x)                   (((x) & 0x7F) << 8)
#define BIT_AON_APB_CLK_DMC0_SEL_HW_EN                            BIT(7)
#define BIT_AON_APB_CLK_DMC0_2X_DIV(x)                            (((x) & 0x7) << 4)
#define BIT_AON_APB_CLK_DMC0_1X_DIV                               BIT(3)
#define BIT_AON_APB_CLK_DMC0_SEL(x)                               (((x) & 0x7))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_CHIP_ID0
// Register Offset : 0x00F8
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AON_CHIP_ID0(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_CHIP_ID1
// Register Offset : 0x00FC
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AON_CHIP_ID1(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_CCIR_RCVR_CFG
// Register Offset : 0x0100
// Description     : APB clock control
---------------------------------------------------------------------------*/

#define BIT_AON_APB_ANALOG_PLL_RSV(x)                             (((x) & 0xFF) << 16)
#define BIT_AON_APB_ANALOG_TESTMUX(x)                             (((x) & 0xFF) << 8)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_PLL_BG_CFG
// Register Offset : 0x0108
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_PLL_BG_RSV(x)                                 (((x) & 0x3F))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_LVDSDIS_SEL
// Register Offset : 0x010C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_LVDSDIS_LOG_SEL(x)                            (((x) & 0x3) << 1)
#define BIT_AON_APB_LVDSDIS_DBG_SEL                               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_EMC_AUTO_GATE_EN
// Register Offset : 0x0120
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_PUBCP_AON_AUTO_GATE_EN                        BIT(20)
#define BIT_AON_APB_AGCP_AON_AUTO_GATE_EN                         BIT(19)
#define BIT_AON_APB_WTLCP_AON_AUTO_GATE_EN                        BIT(18)
#define BIT_AON_APB_AP_AON_AUTO_GATE_EN                           BIT(17)
#define BIT_AON_APB_PUB_AON_AUTO_GATE_EN                          BIT(16)
#define BIT_AON_APB_CAM_DMC_AUTO_GATE_EN                          BIT(7)
#define BIT_AON_APB_DISP_DMC_AUTO_GATE_EN                         BIT(6)
#define BIT_AON_APB_VSP_DMC_AUTO_GATE_EN                          BIT(5)
#define BIT_AON_APB_PUBCP_DMC_AUTO_GATE_EN                        BIT(4)
#define BIT_AON_APB_AGCP_DMC_AUTO_GATE_EN                         BIT(3)
#define BIT_AON_APB_WTLCP_DMC_AUTO_GATE_EN                        BIT(2)
#define BIT_AON_APB_AP_DMC_AUTO_GATE_EN                           BIT(1)
#define BIT_AON_APB_CA53_DMC_AUTO_GATE_EN                         BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_SP_CFG_BUS
// Register Offset : 0x0124
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_ARM7_AHB_CLK_SOFT_EN                          BIT(7)
#define BIT_AON_APB_CM3_SLEEPING_STAT                             BIT(6)
#define BIT_AON_APB_CM3_LOCKUP_STAT                               BIT(5)
#define BIT_AON_APB_CM3_SOFT_MPUDIS                               BIT(4)
#define BIT_AON_APB_MMTX_SLEEP_CM3_PUB_WR                         BIT(3)
#define BIT_AON_APB_MMTX_SLEEP_CM3_PUB_RD                         BIT(2)
#define BIT_AON_APB_INT_REQ_CM3_SOFT                              BIT(1)
#define BIT_AON_APB_SP_CFG_BUS_SLEEP                              BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_RTC4M_0_CFG
// Register Offset : 0x0128
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_RTC4M0_CAL_DONE                               BIT(6)
#define BIT_AON_APB_RTC4M0_CAL_START                              BIT(5)
#define BIT_AON_APB_RTC4M0_FORCE_EN                               BIT(1)
#define BIT_AON_APB_RTC4M0_AUTO_GATE_EN                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_APB_RST2
// Register Offset : 0x0130
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DJTAG_PUB1_SOFT_RST                           BIT(18)
#define BIT_AON_APB_DJTAG_PUB0_SOFT_RST                           BIT(17)
#define BIT_AON_APB_DJTAG_AON_SOFT_RST                            BIT(16)
#define BIT_AON_APB_DJTAG_AGCP_SOFT_RST                           BIT(10)
#define BIT_AON_APB_DJTAG_WTLCP_SOFT_RST                          BIT(9)
#define BIT_AON_APB_DJTAG_PUBCP_SOFT_RST                          BIT(8)
#define BIT_AON_APB_DJTAG_DISP_SOFT_RST                           BIT(5)
#define BIT_AON_APB_DJTAG_CAM_SOFT_RST                            BIT(4)
#define BIT_AON_APB_DJTAG_VSP_SOFT_RST                            BIT(3)
#define BIT_AON_APB_DJTAG_GPU_SOFT_RST                            BIT(2)
#define BIT_AON_APB_DJTAG_CA53_SOFT_RST                           BIT(1)
#define BIT_AON_APB_DJTAG_AP_SOFT_RST                             BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_RC100M_CFG
// Register Offset : 0x0134
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_RC100M_DIV(x)                                 (((x) & 0x3F) << 7)
#define BIT_AON_APB_RC100M_CAL_DONE                               BIT(6)
#define BIT_AON_APB_RC100M_CAL_START                              BIT(5)
#define BIT_AON_APB_RC100M_ANA_SOFT_RST                           BIT(4)
#define BIT_AON_APB_RC100M_FORCE_EN                               BIT(1)
#define BIT_AON_APB_RC100M_AUTO_GATE_EN                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_CGM_REG1
// Register Offset : 0x0138
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CGM_CM3_TMR2_EN                               BIT(19)
#define BIT_AON_APB_CGM_DET_32K_EN                                BIT(18)
#define BIT_AON_APB_CGM_DEBOUNCE_EN                               BIT(17)
#define BIT_AON_APB_CGM_RC100M_FDK_EN                             BIT(16)
#define BIT_AON_APB_CGM_RC100M_REF_EN                             BIT(15)
#define BIT_AON_APB_CGM_HSIC_REF_EN                               BIT(14)
#define BIT_AON_APB_CGM_USB3_SUSPEND_EN                           BIT(13)
#define BIT_AON_APB_CGM_USB3_REF_EN                               BIT(12)
#define BIT_AON_APB_CGM_OTG2_REF_EN                               BIT(11)
#define BIT_AON_APB_CGM_DPHY_REF_EN                               BIT(10)
#define BIT_AON_APB_CGM_DJTAG_TCK_EN                              BIT(9)
#define BIT_AON_APB_CGM_RTC4M0_FDK_EN                             BIT(8)
#define BIT_AON_APB_CGM_RTC4M0_REF_EN                             BIT(7)
#define BIT_AON_APB_CGM_MDAR_CHK_EN                               BIT(6)
#define BIT_AON_APB_CGM_LVDSRF_CALI_EN                            BIT(5)
#define BIT_AON_APB_CGM_RFTI2_LTH_EN                              BIT(4)
#define BIT_AON_APB_CGM_RFTI2_XO_EN                               BIT(3)
#define BIT_AON_APB_CGM_RFTI1_LTH_EN                              BIT(2)
#define BIT_AON_APB_CGM_RFTI1_XO_EN                               BIT(1)
#define BIT_AON_APB_CGM_RFTI_SBI_EN                               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_CGM_CLK_TOP_REG1
// Register Offset : 0x013C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_SOFT_DDR0_CKG_1X_EN                           BIT(26)
#define BIT_AON_APB_SOFT_DDR1_CKG_1X_EN                           BIT(25)
#define BIT_AON_APB_SOFT_DDR1_DATA_RET                            BIT(24)
#define BIT_AON_APB_SOFT_DDR0_DATA_RET                            BIT(23)
#define BIT_AON_APB_LIGHT_SLEEP_DDR1_DATA_RET_EN                  BIT(22)
#define BIT_AON_APB_LIGHT_SLEEP_DDR0_DATA_RET_EN                  BIT(21)
#define BIT_AON_APB_EMC1_CKG_SEL_LOAD                             BIT(20)
#define BIT_AON_APB_EMC0_CKG_SEL_LOAD                             BIT(19)
#define BIT_AON_APB_DMC_DDR1_1X_EN                                BIT(18)
#define BIT_AON_APB_DMC_DDR1_2X_EN                                BIT(17)
#define BIT_AON_APB_DMC_DDR0_1X_EN                                BIT(16)
#define BIT_AON_APB_DMC_DDR0_2X_EN                                BIT(15)
#define BIT_AON_APB_CGM_W_SYS_EN                                  BIT(14)
#define BIT_AON_APB_CGM_W_ZIF_EN                                  BIT(13)
#define BIT_AON_APB_CGM_RFTI_RX_WD_EN                             BIT(12)
#define BIT_AON_APB_CGM_RFTI_TX_WD_EN                             BIT(11)
#define BIT_AON_APB_CGM_WCDMA_EN                                  BIT(10)
#define BIT_AON_APB_CGM_EMMC_2X_EN                                BIT(9)
#define BIT_AON_APB_CGM_EMMC_1X_EN                                BIT(8)
#define BIT_AON_APB_CGM_SDIO2_1X_EN                               BIT(7)
#define BIT_AON_APB_CGM_SDIO2_2X_EN                               BIT(6)
#define BIT_AON_APB_CGM_SDIO1_1X_EN                               BIT(5)
#define BIT_AON_APB_CGM_SDIO1_2X_EN                               BIT(4)
#define BIT_AON_APB_CGM_SDIO0_1X_EN                               BIT(3)
#define BIT_AON_APB_CGM_SDIO0_2X_EN                               BIT(2)
#define BIT_AON_APB_CGM_AP_AXI_EN                                 BIT(1)
#define BIT_AON_APB_CGM_CSSYS_EN                                  BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AGCP_DSP_CTRL0
// Register Offset : 0x0140
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AGCP_DSP_BOOT_VECTOR(x)                       (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AGCP_DSP_CTRL1
// Register Offset : 0x0144
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AGCP_STCK_DSP                                 BIT(13)
#define BIT_AON_APB_AGCP_STMS_DSP                                 BIT(12)
#define BIT_AON_APB_AGCP_STDO_DSP                                 BIT(11)
#define BIT_AON_APB_AGCP_STDI_DSP                                 BIT(10)
#define BIT_AON_APB_AGCP_STRTCK_DSP                               BIT(9)
#define BIT_AON_APB_AGCP_SW_JTAG_ENA_DSP                          BIT(8)
#define BIT_AON_APB_AGCP_DSP_EXTERNAL_WAIT                        BIT(1)
#define BIT_AON_APB_AGCP_DSP_BOOT                                 BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AGCP_SOFT_RESET
// Register Offset : 0x0148
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AGCP_DSP_CORE_RST                             BIT(1)
#define BIT_AON_APB_AGCP_DSP_SYS_RST                              BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AGCP_CTRL
// Register Offset : 0x014C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AGCP_FRC_CLK_DSP_EN                           BIT(1)
#define BIT_AON_APB_AGCP_TOP_ACCESS_EN                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WTLCP_LDSP_CTRL0
// Register Offset : 0x0150
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_WTLCP_LDSP_BOOT_VECTOR(x)                     (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WTLCP_LDSP_CTRL1
// Register Offset : 0x0154
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_WTLCP_STCK_LDSP                               BIT(13)
#define BIT_AON_APB_WTLCP_STMS_LDSP                               BIT(12)
#define BIT_AON_APB_WTLCP_STDO_LDSP                               BIT(11)
#define BIT_AON_APB_WTLCP_STDI_LDSP                               BIT(10)
#define BIT_AON_APB_WTLCP_STRTCK_LDSP                             BIT(9)
#define BIT_AON_APB_WTLCP_SW_JTAG_ENA_LDSP                        BIT(8)
#define BIT_AON_APB_WTLCP_LDSP_EXTERNAL_WAIT                      BIT(1)
#define BIT_AON_APB_WTLCP_LDSP_BOOT                               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WTLCP_TDSP_CTRL0
// Register Offset : 0x0158
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_WTLCP_TDSP_BOOT_VECTOR(x)                     (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WTLCP_TDSP_CTRL1
// Register Offset : 0x015C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_WTLCP_STCK_TDSP                               BIT(13)
#define BIT_AON_APB_WTLCP_STMS_TDSP                               BIT(12)
#define BIT_AON_APB_WTLCP_STDO_TDSP                               BIT(11)
#define BIT_AON_APB_WTLCP_STDI_TDSP                               BIT(10)
#define BIT_AON_APB_WTLCP_STRTCK_TDSP                             BIT(9)
#define BIT_AON_APB_WTLCP_SW_JTAG_ENA_TDSP                        BIT(8)
#define BIT_AON_APB_WTLCP_TDSP_EXTERNAL_WAIT                      BIT(1)
#define BIT_AON_APB_WTLCP_TDSP_BOOT                               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WTLCP_SOFT_RESET
// Register Offset : 0x0160
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_WTLCP_TDSP_CORE_SRST                          BIT(1)
#define BIT_AON_APB_WTLCP_LDSP_CORE_SRST                          BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WTLCP_CTRL
// Register Offset : 0x0164
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_WTLCP_AON_FRC_WSYS_LT_STOP                    BIT(4)
#define BIT_AON_APB_WTLCP_AON_FRC_WSYS_STOP                       BIT(3)
#define BIT_AON_APB_WTLCP_DSP_DEEP_SLEEP_EN                       BIT(2)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WTL_WCDMA_EB
// Register Offset : 0x0168
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_WTLCP_WCMDA_EB                                BIT(16)
#define BIT_AON_APB_WCDMA_AUTO_GATE_EN                            BIT(8)
#define BIT_AON_APB_WTLCP_WTLSYS_RFTI_TX_EB                       BIT(1)
#define BIT_AON_APB_WTLCP_WTLSYS_RFTI_RX_EB                       BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_PCP_AON_EB
// Register Offset : 0x0170
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_PUBCP_SYST_RTC_EB                             BIT(11)
#define BIT_AON_APB_PUBCP_TMR_EB                                  BIT(10)
#define BIT_AON_APB_PUBCP_TMR_RTC_EB                              BIT(9)
#define BIT_AON_APB_PUBCP_SYST_EB                                 BIT(8)
#define BIT_AON_APB_PUBCP_WDG_EB                                  BIT(7)
#define BIT_AON_APB_PUBCP_WDG_RTC_EB                              BIT(6)
#define BIT_AON_APB_PUBCP_ARCH_RTC_EB                             BIT(5)
#define BIT_AON_APB_PUBCP_EIC_EB                                  BIT(4)
#define BIT_AON_APB_PUBCP_EIC_RTCDV5_EB                           BIT(3)
#define BIT_AON_APB_PUBCP_EIC_RTC_EB                              BIT(2)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_PCP_SOFT_RST
// Register Offset : 0x0174
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_PUBCP_CR5_CORE_SOFT_RST                       BIT(10)
#define BIT_AON_APB_PUBCP_CR5_DBG_SOFT_RST                        BIT(9)
#define BIT_AON_APB_PUBCP_CR5_ETM_SOFT_RST                        BIT(8)
#define BIT_AON_APB_PUBCP_CR5_MP_SOFT_RST                         BIT(7)
#define BIT_AON_APB_PUBCP_CR5_CS_DBG_SOFT_RST                     BIT(6)
#define BIT_AON_APB_PUBCP_TMR_SOFT_RST                            BIT(5)
#define BIT_AON_APB_PUBCP_SYST_SOFT_RST                           BIT(4)
#define BIT_AON_APB_PUBCP_WDG_SOFT_RST                            BIT(3)
#define BIT_AON_APB_PUBCP_EIC_SOFT_RST                            BIT(2)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_PUBCP_CTRL
// Register Offset : 0x0178
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_PUBCP_CR5_STANDBYWFI_N                        BIT(12)
#define BIT_AON_APB_PUBCP_CR5_STANDBYWFE_N                        BIT(11)
#define BIT_AON_APB_PUBCP_CR5_CLKSTOPPED0_N                       BIT(10)
#define BIT_AON_APB_PUBCP_CR5_L2IDLE                              BIT(9)
#define BIT_AON_APB_PUBCP_CR5_VALIRQ0_N                           BIT(8)
#define BIT_AON_APB_PUBCP_CR5_VALFIQ0_N                           BIT(7)
#define BIT_AON_APB_PUBCP_CR5_STOP                                BIT(6)
#define BIT_AON_APB_PUBCP_CR5_CSYSACK_ATB                         BIT(5)
#define BIT_AON_APB_PUBCP_CR5_CACTIVE_ATB                         BIT(4)
#define BIT_AON_APB_PUBCP_CR5_CSSYNC_REQ                          BIT(3)
#define BIT_AON_APB_PUBCP_CR5_CSYSREQ_ATB                         BIT(2)
#define BIT_AON_APB_PUBCP_CR5_NODBGCLK                            BIT(1)
#define BIT_AON_APB_PUBCP_CR5_CFGEE                               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_USB3_CTRL
// Register Offset : 0x0180
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_USB3_SUSPEND_EB                               BIT(31)
#define BIT_AON_APB_USB3_SOFT_RST                                 BIT(30)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_DEBUG_U2PMU
// Register Offset : 0x0184
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DEBUG_U2PMU(x)                                (((x) & 0x7FFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_DEBUG_U3PMU
// Register Offset : 0x0188
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DEBUG_U3PMU(x)                                (((x) & 0x7FFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_FIREWALL_CLK_EN
// Register Offset : 0x0190
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AGCP_FIREWALL_CLK_EN                          BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_GPU_CTRL
// Register Offset : 0x0194
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_GPU_TS0_SOFT_RST                              BIT(1)
#define BIT_AON_APB_GPU_AVS_SOFT_RST                              BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_A53_CTRL_0
// Register Offset : 0x01A0
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CA53_ARQOSARB_GPU_CCI(x)                      (((x) & 0xF) << 24)
#define BIT_AON_APB_CA53_AWREGION_GPU_CCI(x)                      (((x) & 0xF) << 20)
#define BIT_AON_APB_CA53_ARREGION_GPU_CCI(x)                      (((x) & 0xF) << 16)
#define BIT_AON_APB_CA53_CSYSREQ_ATB7                             BIT(15)
#define BIT_AON_APB_CA53_CSYSREQ_ATB6                             BIT(14)
#define BIT_AON_APB_CA53_CSYSREQ_ATB5                             BIT(13)
#define BIT_AON_APB_CA53_CSYSREQ_ATB4                             BIT(12)
#define BIT_AON_APB_CA53_CSYSREQ_ATB3                             BIT(11)
#define BIT_AON_APB_CA53_CSYSREQ_ATB2                             BIT(10)
#define BIT_AON_APB_CA53_CSYSREQ_ATB1                             BIT(9)
#define BIT_AON_APB_CA53_CSYSREQ_ATB0                             BIT(8)
#define BIT_AON_APB_CA53_CCI_CSYSREQ                              BIT(7)
#define BIT_AON_APB_CA53_GIC_SOFT_RST                             BIT(3)
#define BIT_AON_APB_CA53_TRACE2LVDS_SOFT_RST                      BIT(2)
#define BIT_AON_APB_CA53_CSSYS_SOFT_RST                           BIT(1)
#define BIT_AON_APB_CA53_CCI_SOFT_RST                             BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_A53_CTRL_1
// Register Offset : 0x01A4
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CA53_AW_QOS_RSV(x)                            (((x) & 0xF) << 24)
#define BIT_AON_APB_CA53_AR_QOS_RSV(x)                            (((x) & 0xF) << 8)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_A53_BIGCORE_VDROP_RSV
// Register Offset : 0x01A8
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CA53_BIG_VDROP0_RSV                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_A53_LITCORE_VDROP_RSV
// Register Offset : 0x01AC
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CA53_LIT_VDROP0_EN                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_A53_CTRL_2
// Register Offset : 0x01B0
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CA53_BIG_AUTO_REG_SOFT_TRIG                   BIT(29)
#define BIT_AON_APB_CA53_BIG_SMPEN(x)                             (((x) & 0xF) << 25)
#define BIT_AON_APB_CA53_BIG_AUTO_REG_TRIG_SEL                    BIT(24)
#define BIT_AON_APB_CA53_BIG_AUTO_REG_SAVE_EN                     BIT(23)
#define BIT_AON_APB_CA53_BIG_EDBGRQ(x)                            (((x) & 0xF) << 19)
#define BIT_AON_APB_CA53_BIG_DBG_EN                               BIT(18)
#define BIT_AON_APB_CA53_BIG_ATB_EN                               BIT(17)
#define BIT_AON_APB_CA53_BIG_CORE_EN                              BIT(16)
#define BIT_AON_APB_CA53_LIT_AUTO_REG_SOFT_TRIG                   BIT(13)
#define BIT_AON_APB_CA53_LIT_SMPEN(x)                             (((x) & 0xF) << 9)
#define BIT_AON_APB_CA53_LIT_AUTO_REG_TRIG_SEL                    BIT(8)
#define BIT_AON_APB_CA53_LIT_AUTO_REG_SAVE_EN                     BIT(7)
#define BIT_AON_APB_CA53_LIT_EDBGRQ(x)                            (((x) & 0xF) << 3)
#define BIT_AON_APB_CA53_LIT_DBG_EN                               BIT(2)
#define BIT_AON_APB_CA53_LIT_ATB_EN                               BIT(1)
#define BIT_AON_APB_CA53_LIT_CORE_EN                              BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_A53_CTRL_3
// Register Offset : 0x01B4
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CA53_GIC_IRQOUT(x)                            (((x) & 0xFF) << 15)
#define BIT_AON_APB_CA53_GIC_FIQOUT(x)                            (((x) & 0xFF) << 7)
#define BIT_AON_APB_CA53_CLK_CCI_EN                               BIT(6)
#define BIT_AON_APB_CA53_CLK_CSSYS_EN                             BIT(1)
#define BIT_AON_APB_CA53_CLK_GIC_EN                               BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_A53_CTRL_4
// Register Offset : 0x01B8
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CA53_BIG_CTIIRQACK(x)                         (((x) & 0xF) << 24)
#define BIT_AON_APB_CA53_BIG_STANDBYWFE(x)                        (((x) & 0xF) << 20)
#define BIT_AON_APB_CA53_BIG_STANDBYWFI(x)                        (((x) & 0xF) << 16)
#define BIT_AON_APB_CA53_LIT_CTIIRQACK(x)                         (((x) & 0xF) << 8)
#define BIT_AON_APB_CA53_LIT_STANDBYWFE(x)                        (((x) & 0xF) << 4)
#define BIT_AON_APB_CA53_LIT_STANDBYWFI(x)                        (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_A53_CTRL_5
// Register Offset : 0x01BC
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CA53_BIG_L2VICTIRAM_LIGHT_SLEEP               BIT(24)
#define BIT_AON_APB_CA53_BIG_L2DATARAM_LIGHT_SLEEP                BIT(23)
#define BIT_AON_APB_CA53_BIG_L2FLUSHDONE                          BIT(22)
#define BIT_AON_APB_CA53_BIG_L2FLUSHREQ                           BIT(21)
#define BIT_AON_APB_CA53_BIG_L2QDENY                              BIT(20)
#define BIT_AON_APB_CA53_BIG_L2QACCEPT_N                          BIT(19)
#define BIT_AON_APB_CA53_BIG_L2QREQ_N                             BIT(18)
#define BIT_AON_APB_CA53_BIG_L2QACTIVE                            BIT(17)
#define BIT_AON_APB_CA53_BIG_L2_STANDBYWFI                        BIT(16)
#define BIT_AON_APB_CA53_LIT_L2VICTIRAM_LIGHT_SLEEP               BIT(8)
#define BIT_AON_APB_CA53_LIT_L2DATARAM_LIGHT_SLEEP                BIT(7)
#define BIT_AON_APB_CA53_LIT_L2FLUSHDONE                          BIT(6)
#define BIT_AON_APB_CA53_LIT_L2FLUSHREQ                           BIT(5)
#define BIT_AON_APB_CA53_LIT_L2QDENY                              BIT(4)
#define BIT_AON_APB_CA53_LIT_L2QACCEPT_N                          BIT(3)
#define BIT_AON_APB_CA53_LIT_L2QREQ_N                             BIT(2)
#define BIT_AON_APB_CA53_LIT_L2QACTIVE                            BIT(1)
#define BIT_AON_APB_CA53_LIT_L2_STANDBYWFI                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_SUB_SYS_DEBUG_BUS_SEL_0
// Register Offset : 0x01C0
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_WTLCP_DEBUG_BUS_SELECT(x)                     (((x) & 0xF) << 28)
#define BIT_AON_APB_PUBCP_DEBUG_BUS_SELECT(x)                     (((x) & 0xF) << 24)
#define BIT_AON_APB_DISP_DEBUG_BUS_SELECT(x)                      (((x) & 0xF) << 20)
#define BIT_AON_APB_CAM_DEBUG_BUS_SELECT(x)                       (((x) & 0xF) << 16)
#define BIT_AON_APB_VSP_DEBUG_BUS_SELECT(x)                       (((x) & 0xF) << 12)
#define BIT_AON_APB_GPU_DEBUG_BUS_SELECT(x)                       (((x) & 0xF) << 8)
#define BIT_AON_APB_CA53_DEBUG_BUS_SELECT(x)                      (((x) & 0xF) << 4)
#define BIT_AON_APB_AP_DEBUG_BUS_SELECT(x)                        (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_SUB_SYS_DEBUG_BUS_SEL_1
// Register Offset : 0x01C4
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_MDAR_LVDSRF_DBG_SEL(x)                        (((x) & 0xF) << 20)
#define BIT_AON_APB_MDAR_DEBUG_BUS_SELECT(x)                      (((x) & 0xF) << 16)
#define BIT_AON_APB_PUB1_DEBUG_BUS_SELECT(x)                      (((x) & 0xF) << 12)
#define BIT_AON_APB_PUB0_DEBUG_BUS_SELECT(x)                      (((x) & 0xF) << 8)
#define BIT_AON_APB_AON_DEBUG_BUS_SELECT(x)                       (((x) & 0xF) << 4)
#define BIT_AON_APB_AGCP_DEBUG_BUS_SELECT(x)                      (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_GLB_DEBUG_BUS_SEL
// Register Offset : 0x01C8
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DEBUG_SUBSYS_SELECT(x)                        (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_SOFT_RST_AON_ADD1
// Register Offset : 0x01CC
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_BSMTMR_SOFT_RST                               BIT(7)
#define BIT_AON_APB_RFTI2_LTH_SOFT_RST                            BIT(6)
#define BIT_AON_APB_RFTI1_LTH_SOFT_RST                            BIT(5)
#define BIT_AON_APB_CSSYS_SOFT_RST                                BIT(4)
#define BIT_AON_APB_ANLG_SOFT_RST                                 BIT(3)
#define BIT_AON_APB_AO_MODEL_SOFT_RST                             BIT(2)
#define BIT_AON_APB_RFTI_SBI_SOFT_RST                             BIT(1)
#define BIT_AON_APB_LVDSRF_CALI_SOFT_RST                          BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_EB_AON_ADD1
// Register Offset : 0x01D0
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AON_APB_IDLE_EN                               BIT(2)
#define BIT_AON_APB_BSM_TMR_EB                                    BIT(1)
#define BIT_AON_APB_ANLG_EB                                       BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_DBG_DJTAG_CTRL
// Register Offset : 0x01D4
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DBGSYS_CSSYS_STM_NSGUAREN                     BIT(16)
#define BIT_AON_APB_DJTAG_SRC_MUX_SEL                             BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_MPLL0_CTRL
// Register Offset : 0x01D8
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CGM_MPLL0_CA53_FORCE_EN                       BIT(9)
#define BIT_AON_APB_CGM_MPLL0_CA53_AUTO_GATE_SEL                  BIT(8)
#define BIT_AON_APB_MPLL0_WAIT_FORCE_EN                           BIT(2)
#define BIT_AON_APB_MPLL0_WAIT_AUTO_GATE_SEL                      BIT(1)
#define BIT_AON_APB_MPLL0_SOFT_CNT_DONE                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_MPLL1_CTRL
// Register Offset : 0x01DC
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CGM_MPLL1_CA53_FORCE_EN                       BIT(9)
#define BIT_AON_APB_CGM_MPLL1_CA53_AUTO_GATE_SEL                  BIT(8)
#define BIT_AON_APB_MPLL1_WAIT_FORCE_EN                           BIT(2)
#define BIT_AON_APB_MPLL1_WAIT_AUTO_GATE_SEL                      BIT(1)
#define BIT_AON_APB_MPLL1_SOFT_CNT_DONE                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_PUB_FC_CTRL
// Register Offset : 0x01E0
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_EMC_CKG_SEL_DEFAULT_PUB1(x)                   (((x) & 0x3F) << 26)
#define BIT_AON_APB_PUB1_DMC_2X_CGM_SEL                           BIT(25)
#define BIT_AON_APB_PUB1_DMC_1X_CGM_SEL                           BIT(24)
#define BIT_AON_APB_PUB1_DFS_SWITCH_WAIT_TIME(x)                  (((x) & 0xFF) << 16)
#define BIT_AON_APB_EMC_CKG_SEL_DEFAULT_PUB0(x)                   (((x) & 0x3F) << 10)
#define BIT_AON_APB_PUB0_DMC_2X_CGM_SEL                           BIT(9)
#define BIT_AON_APB_PUB0_DMC_1X_CGM_SEL                           BIT(8)
#define BIT_AON_APB_PUB0_DFS_SWITCH_WAIT_TIME(x)                  (((x) & 0xFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_IO_DLY_CTRL
// Register Offset : 0x3014
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CLK_CP1DSP_DLY_SEL(x)                         (((x) & 0xF) << 4)
#define BIT_AON_APB_CLK_CP0DSP_DLY_SEL(x)                         (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WDG_RST_FLAG
// Register Offset : 0x3024
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_PCP_WDG_RST_FLAG                              BIT(5)
#define BIT_AON_APB_WTLCP_LTE_WDG_RST_FLAG                        BIT(4)
#define BIT_AON_APB_WTLCP_TG_WDG_RST_FLAG                         BIT(3)
#define BIT_AON_APB_AGCP_WDG_RST_FLAG                             BIT(2)
#define BIT_AON_APB_CA53_WDG_RST_FLAG                             BIT(1)
#define BIT_AON_APB_SEC_WDG_RST_FLAG                              BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_PMU_RST_MONITOR
// Register Offset : 0x302C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_PMU_RST_MONITOR(x)                            (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_THM_RST_MONITOR
// Register Offset : 0x3030
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_THM_RST_MONITOR(x)                            (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AP_RST_MONITOR
// Register Offset : 0x3034
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AP_RST_MONITOR(x)                             (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_CA53_RST_MONITOR
// Register Offset : 0x3038
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CA53_RST_MONITOR(x)                           (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_BOND_OPT0
// Register Offset : 0x303C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_BOND_OPTION0(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_BOND_OPT1
// Register Offset : 0x3040
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_BOND_OPTION1(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_RES_REG0
// Register Offset : 0x3044
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_RES_REG0(x)                                   (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_RES_REG1
// Register Offset : 0x3048
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_RES_REG1(x)                                   (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_QOS_CFG
// Register Offset : 0x304C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_QOS_R_GPU(x)                                  (((x) & 0xF) << 12)
#define BIT_AON_APB_QOS_W_GPU(x)                                  (((x) & 0xF) << 8)
#define BIT_AON_APB_QOS_R_GSP(x)                                  (((x) & 0xF) << 4)
#define BIT_AON_APB_QOS_W_GSP(x)                                  (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_MTX_PROT_CFG
// Register Offset : 0x3058
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_HPROT_DMAW(x)                                 (((x) & 0xF) << 4)
#define BIT_AON_APB_HPROT_DMAR(x)                                 (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_PLL_LOCK_OUT_SEL
// Register Offset : 0x3064
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_REC_26MHZ_0_BUF_PD                            BIT(8)
#define BIT_AON_APB_SLEEP_PLLLOCK_SEL                             BIT(7)
#define BIT_AON_APB_PLL_LOCK_SEL(x)                               (((x) & 0x7) << 4)
#define BIT_AON_APB_SLEEP_DBG_SEL(x)                              (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_RTC4M_RC_VAL
// Register Offset : 0x3068
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_RTC4M0_RC_SEL                                 BIT(15)
#define BIT_AON_APB_RTC4M0_RC_VAL(x)                              (((x) & 0x1FF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_DDR_STAT_FLAG
// Register Offset : 0x306C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_INT_DFS_PUB0_COMPLETE                         BIT(3)
#define BIT_AON_APB_INT_DFS_PUB1_COMPLETE                         BIT(2)
#define BIT_AON_APB_INT_REQ_PUB1_CTRL_FLAG                        BIT(1)
#define BIT_AON_APB_INT_REQ_PUB0_CTRL_FLAG                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_MODEL_STAT
// Register Offset : 0x3070
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AO_LCS_IS_CM                                  BIT(4)
#define BIT_AON_APB_AO_LCS_IS_DM                                  BIT(3)
#define BIT_AON_APB_AO_LCS_IS_SECURE                              BIT(2)
#define BIT_AON_APB_AO_LCS_IS_SD                                  BIT(1)
#define BIT_AON_APB_AO_LCS_IS_RMA                                 BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_USER_COMM_FLAG
// Register Offset : 0x3074
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_USER_COMM_FLAG(x)                             (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_MAIN_MTX_QOS1
// Register Offset : 0x3078
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_ARQOS_M4_SP_AON(x)                            (((x) & 0xF) << 4)
#define BIT_AON_APB_AWQOS_M4_SP_AON(x)                            (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_MAIN_MTX_QOS2
// Register Offset : 0x307C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_ARQOS_M3_AGCP_AON(x)                          (((x) & 0xF) << 28)
#define BIT_AON_APB_AWQOS_M3_AGCP_AON(x)                          (((x) & 0xF) << 24)
#define BIT_AON_APB_ARQOS_M2_PUBCP_AON(x)                         (((x) & 0xF) << 20)
#define BIT_AON_APB_AWQOS_M2_PUBCP_AON(x)                         (((x) & 0xF) << 16)
#define BIT_AON_APB_ARQOS_M1_WTLCP_AON(x)                         (((x) & 0xF) << 12)
#define BIT_AON_APB_AWQOS_M1_WTLCP_AON(x)                         (((x) & 0xF) << 8)
#define BIT_AON_APB_ARQOS_M0_AP_AON(x)                            (((x) & 0xF) << 4)
#define BIT_AON_APB_AWQOS_M0_AP_AON(x)                            (((x) & 0xF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_RC100M_RC_VAL
// Register Offset : 0x3080
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_RC100M_RC_SEL                                 BIT(15)
#define BIT_AON_APB_RC100M_RC_VAL(x)                              (((x) & 0x1FF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_APB_RSV
// Register Offset : 0x30F0
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AON_APB_RSV(x)                                (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_GLB_CLK_AUTO_GATE_EN
// Register Offset : 0x30F4
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_GLB_CLK_AUTO_GATE_EN(x)                       (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_OSC_FUNC_CNT
// Register Offset : 0x3100
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_OSC_CHNL1_FUNC_CNT(x)                         (((x) & 0xFFFF) << 16)
#define BIT_AON_APB_OSC_CHNL2_FUNC_CNT(x)                         (((x) & 0xFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_OSC_FUNC_CTRL
// Register Offset : 0x3104
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_RST_BINING_FUNC_N                             BIT(25)
#define BIT_AON_APB_OSC_BINING_SEL_FUNC(x)                        (((x) & 0x7F) << 18)
#define BIT_AON_APB_OSC_BINING_CTRL_FUNC_EN                       BIT(17)
#define BIT_AON_APB_OSC_BINING_CLK_FUNC_NUM(x)                    (((x) & 0x1FFF) << 4)
#define BIT_AON_APB_OSC_CHNL2_FUNC_OVERFLOW                       BIT(3)
#define BIT_AON_APB_OSC_CHNL1_FUNC_OVERFLOW                       BIT(2)
#define BIT_AON_APB_OSC_CHNL2_FUNC_CNT_VALID                      BIT(1)
#define BIT_AON_APB_OSC_CHNL1_FUNC_CNT_VALID                      BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_DEEP_SLEEP_MUX_SEL
// Register Offset : 0x3108
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AP_DEEP_SLEEP_SEL                             BIT(3)
#define BIT_AON_APB_WTLCP_DEEP_SLEEP_SEL                          BIT(2)
#define BIT_AON_APB_PUBCP_DEEP_SLEEP_SEL                          BIT(1)
#define BIT_AON_APB_AGCP_DEEP_SLEEP_SEL                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_NIU_IDLE_DET_DISABLE
// Register Offset : 0x310C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_VSP_NIU_IDLE_DET_DISABLE                      BIT(4)
#define BIT_AON_APB_DISP_NIU_IDLE_DET_DISABLE                     BIT(3)
#define BIT_AON_APB_GSP_NIU_IDLE_DET_DISABLE                      BIT(2)
#define BIT_AON_APB_CAM_NIU_IDLE_DET_DISABLE                      BIT(1)
#define BIT_AON_APB_CA53_NIU_IDLE_DET_DISABLE                     BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_FUNC_TEST_BOOT_ADDR
// Register Offset : 0x3110
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_FUNC_TEST_BOOT_ADDR(x)                        (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_CGM_RESCUE
// Register Offset : 0x3114
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CGM_RESCUE(x)                                 (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_PLAT_ID0
// Register Offset : 0x3118
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AON_PLAT_ID0(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_PLAT_ID1
// Register Offset : 0x311C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AON_PLAT_ID1(x)                               (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_VER_ID
// Register Offset : 0x3120
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AON_VER_ID(x)                                 (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_MFT_ID
// Register Offset : 0x3124
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AON_MFT_ID(x)                                 (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AON_MPL_ID
// Register Offset : 0x3128
// Description     : IMPL_ID
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AON_IMPL_ID(x)                                (((x) & 0xFFFFFFFF))

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_VSP_NOC_CTRL
// Register Offset : 0x315C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_VSP_M0_IDLE                                   BIT(8)
#define BIT_AON_APB_VSP_SERVICE_ACCESS_EN                         BIT(3)
#define BIT_AON_APB_VSP_INTERLEAVE_MODE(x)                        (((x) & 0x3) << 1)
#define BIT_AON_APB_VSP_INTERLEAVE_SEL                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_CAM_NOC_CTRL
// Register Offset : 0x3160
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_CAM_M0_IDLE                                   BIT(8)
#define BIT_AON_APB_CAM_SERVICE_ACCESS_EN                         BIT(3)
#define BIT_AON_APB_CAM_INTERLEAVE_MODE(x)                        (((x) & 0x3) << 1)
#define BIT_AON_APB_CAM_INTERLEAVE_SEL                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_DISP_NOC_CTRL
// Register Offset : 0x3164
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_DISP_M0_IDLE                                  BIT(8)
#define BIT_AON_APB_DISP_SERVICE_ACCESS_EN                        BIT(3)
#define BIT_AON_APB_DISP_INTERLEAVE_MODE(x)                       (((x) & 0x3) << 1)
#define BIT_AON_APB_DISP_INTERLEAVE_SEL                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_GSP_NOC_CTRL
// Register Offset : 0x3168
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_GSP_M0_IDLE                                   BIT(8)
#define BIT_AON_APB_GSP_SERVICE_ACCESS_EN                         BIT(3)
#define BIT_AON_APB_GSP_INTERLEAVE_MODE(x)                        (((x) & 0x3) << 1)
#define BIT_AON_APB_GSP_INTERLEAVE_SEL                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WTL_CP_NOC_CTRL
// Register Offset : 0x316C
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_WTL_CP_M0_IDLE                                BIT(8)
#define BIT_AON_APB_WTL_CP_ADDR_REMAP_BIT(x)                      (((x) & 0x7) << 4)
#define BIT_AON_APB_WTL_CP_SERVICE_ACCESS_EN                      BIT(3)
#define BIT_AON_APB_WTL_CP_INTERLEAVE_MODE(x)                     (((x) & 0x3) << 1)
#define BIT_AON_APB_WTL_CP_INTERLEAVE_SEL                         BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_LACC_NOC_CTRL
// Register Offset : 0x3170
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_LACC_M0_IDLE                                  BIT(8)
#define BIT_AON_APB_LACC_ADDR_REMAP_BIT(x)                        (((x) & 0x7) << 4)
#define BIT_AON_APB_LACC_SERVICE_ACCESS_EN                        BIT(3)
#define BIT_AON_APB_LACC_INTERLEAVE_MODE(x)                       (((x) & 0x3) << 1)
#define BIT_AON_APB_LACC_INTERLEAVE_SEL                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_AGCP_NOC_CTRL
// Register Offset : 0x3174
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_AGCP_M0_IDLE                                  BIT(8)
#define BIT_AON_APB_AGCP_ADDR_REMAP_BIT(x)                        (((x) & 0x7) << 4)
#define BIT_AON_APB_AGCP_SERVICE_ACCESS_EN                        BIT(3)
#define BIT_AON_APB_AGCP_INTERLEAVE_MODE(x)                       (((x) & 0x3) << 1)
#define BIT_AON_APB_AGCP_INTERLEAVE_SEL                           BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_PCP_NOC_CTRL
// Register Offset : 0x3178
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_PCP_M0_IDLE                                   BIT(8)
#define BIT_AON_APB_PCP_ADDR_REMAP_BIT(x)                         (((x) & 0x7) << 4)
#define BIT_AON_APB_PCP_SERVICE_ACCESS_EN                         BIT(3)
#define BIT_AON_APB_PCP_INTERLEAVE_MODE(x)                        (((x) & 0x3) << 1)
#define BIT_AON_APB_PCP_INTERLEAVE_SEL                            BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_WTL_CP_RFFE_CTRL_SEL
// Register Offset : 0x3180
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_RFFE3_CTRL_SEL                                BIT(3)
#define BIT_AON_APB_RFFE2_CTRL_SEL                                BIT(2)
#define BIT_AON_APB_RFFE1_CTRL_SEL                                BIT(1)
#define BIT_AON_APB_RFFE0_CTRL_SEL                                BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_SP_WAKEUP_MASK_EN1
// Register Offset : 0x3184
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_SP_PUB1_DFI_BUSMON_INT_WAKE_EN                BIT(31)
#define BIT_AON_APB_SP_PUB1_AXI_BUSMON_INT_WAKE_EN                BIT(30)
#define BIT_AON_APB_SP_THM_INT_WAKE_EN                            BIT(29)
#define BIT_AON_APB_SP_PUB0_CTRL_INT_WAKE_EN                      BIT(28)
#define BIT_AON_APB_SP_PUB1_CTRL_INT_WAKE_EN                      BIT(27)
#define BIT_AON_APB_SP_DFS_PUB0_INT_WAKE_EN                       BIT(26)
#define BIT_AON_APB_SP_DFS_PUB1_INT_WAKE_EN                       BIT(25)
#define BIT_AON_APB_SP_BUSMON_WTLCP_INT_WAKE_EN                   BIT(24)
#define BIT_AON_APB_SP_BUSMON_PUBCP_INT_WAKE_EN                   BIT(23)
#define BIT_AON_APB_SP_BUSMON_AGCP_INT_WAKE_EN                    BIT(22)
#define BIT_AON_APB_SP_BUSMON_AP_INT_WAKE_EN                      BIT(21)
#define BIT_AON_APB_SP_AVS_CA53_BIG_INT_WAKE_EN                   BIT(20)
#define BIT_AON_APB_SP_AVS_CA53_LIT_INT_WAKE_EN                   BIT(19)
#define BIT_AON_APB_SP_AVS_GPU0_INT_WAKE_EN                       BIT(18)
#define BIT_AON_APB_SP_EIC_NOTLAT_INT_WAKE_EN                     BIT(17)
#define BIT_AON_APB_SP_CA53_BUSMON_INT_WAKE_EN                    BIT(16)
#define BIT_AON_APB_SP_ADI_INT_WAKE_EN                            BIT(15)
#define BIT_AON_APB_SP_ANA_INT_WAKE_EN                            BIT(14)
#define BIT_AON_APB_SP_AON_I2C_INT_WAKE_EN                        BIT(13)
#define BIT_AON_APB_SP_GPIO_INT_WAKE_EN                           BIT(12)
#define BIT_AON_APB_SP_DMA_INT_WAKE_EN                            BIT(11)
#define BIT_AON_APB_SP_MBOX_SRC_INT_WAKE_EN                       BIT(10)
#define BIT_AON_APB_SP_MBOX_TAR_INT_WAKE_EN                       BIT(9)
#define BIT_AON_APB_SP_WDG_INT_WAKE_EN                            BIT(8)
#define BIT_AON_APB_SP_I2C1_INT_WAKE_EN                           BIT(7)
#define BIT_AON_APB_SP_I2C0_INT_WAKE_EN                           BIT(6)
#define BIT_AON_APB_SP_UART1_INT_WAKE_EN                          BIT(5)
#define BIT_AON_APB_SP_UART0_INT_WAKE_EN                          BIT(4)
#define BIT_AON_APB_SP_SYST_INT_WAKE_EN                           BIT(3)
#define BIT_AON_APB_SP_TMR_INT_WAKE_EN                            BIT(2)
#define BIT_AON_APB_SP_EIC_LAT_INT_WAKE_EN                        BIT(0)

/*---------------------------------------------------------------------------
// Register Name   : REG_AON_APB_SP_WAKEUP_MASK_EN2
// Register Offset : 0x3188
// Description     : 
---------------------------------------------------------------------------*/

#define BIT_AON_APB_SP_GPU_VDROP_OUT_1_INT_WAKE_EN                BIT(19)
#define BIT_AON_APB_SP_GPU_VDROP_OUT_0_INT_WAKE_EN                BIT(18)
#define BIT_AON_APB_SP_CA53_LIT_VDROP0_INT_WAKE_EN                BIT(16)
#define BIT_AON_APB_SP_CA53_BIG_VDROP0_INT_WAKE_EN                BIT(14)
#define BIT_AON_APB_SP_PUB1_DFS_ERROR_INT_WAKE_EN                 BIT(13)
#define BIT_AON_APB_SP_PUB0_DFS_ERROR_INT_WAKE_EN                 BIT(12)
#define BIT_AON_APB_SP_PUB1_DJTAG_INT_WAKE_EN                     BIT(11)
#define BIT_AON_APB_SP_PUB0_DJTAG_INT_WAKE_EN                     BIT(10)
#define BIT_AON_APB_SP_AON_DEF_SLV_INT_WAKE_EN                    BIT(9)
#define BIT_AON_APB_SP_BUSMON_M0_INT_WAKE_EN                      BIT(8)
#define BIT_AON_APB_SP_BUSMON_M1_INT_WAKE_EN                      BIT(7)
#define BIT_AON_APB_SP_BUSMON_M2_INT_WAKE_EN                      BIT(6)
#define BIT_AON_APB_SP_BUSMON_M3_INT_WAKE_EN                      BIT(5)
#define BIT_AON_APB_SP_BUSMON_M4_INT_WAKE_EN                      BIT(4)
#define BIT_AON_APB_SP_PUB0_DFS_EXIT_INT_WAKE_EN                  BIT(3)
#define BIT_AON_APB_SP_PUB1_DFS_EXIT_INT_WAKE_EN                  BIT(2)
#define BIT_AON_APB_SP_PUB0_DFI_BUSMON_INT_WAKE_EN                BIT(1)
#define BIT_AON_APB_SP_PUB0_AXI_BUSMON_INT_WAKE_EN                BIT(0)

#endif
