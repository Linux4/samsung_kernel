/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
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

#ifndef __ANA_REGS_GLB_H__
#define __ANA_REGS_GLB_H__


/* registers definitions for controller ANA_REGS_GLB */
#define ANA_REG_GLB_CHIP_ID_LOW         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0000)
#define ANA_REG_GLB_CHIP_ID_HIGH        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0004)
#define ANA_REG_GLB_MODULE_EN0          SCI_ADDR(ANA_REGS_GLB_BASE, 0x0008)
#define ANA_REG_GLB_MODULE_EN1          SCI_ADDR(ANA_REGS_GLB_BASE, 0x000C)
#define ANA_REG_GLB_ARM_CLK_EN0         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0010)
#define ANA_REG_GLB_ARM_CLK_EN1         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0014)
#define ANA_REG_GLB_RTC_CLK_EN0         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0018)
#define ANA_REG_GLB_RTC_CLK_EN1         SCI_ADDR(ANA_REGS_GLB_BASE, 0x001C)
#define ANA_REG_GLB_SOFT_RST0           SCI_ADDR(ANA_REGS_GLB_BASE, 0x0020)
#define ANA_REG_GLB_SOFT_RST1           SCI_ADDR(ANA_REGS_GLB_BASE, 0x0024)
#define ANA_REG_GLB_POWER_PD_SW         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0028)
#define ANA_REG_GLB_POWER_PD_HW         SCI_ADDR(ANA_REGS_GLB_BASE, 0x002C)
#define ANA_REG_GLB_RESERVED_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0030)
#define ANA_REG_GLB_RESERVED_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0034)
#define ANA_REG_GLB_RESERVED_REG2       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0038)
#define ANA_REG_GLB_RESERVED_REG3       SCI_ADDR(ANA_REGS_GLB_BASE, 0x003C)
#define ANA_REG_GLB_RESERVED_REG4       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0040)
#define ANA_REG_GLB_RESERVED_REG5       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0044)
#define ANA_REG_GLB_DCDC_ARM0_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0048)
#define ANA_REG_GLB_DCDC_ARM0_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x004C)
#define ANA_REG_GLB_DCDC_ARM0_REG2      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0050)
#define ANA_REG_GLB_DCDC_ARM0_VOL       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0054)
#define ANA_REG_GLB_DCDC_ARM1_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0058)
#define ANA_REG_GLB_DCDC_ARM1_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x005C)
#define ANA_REG_GLB_DCDC_ARM1_REG2      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0060)
#define ANA_REG_GLB_DCDC_ARM1_VOL       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0064)
#define ANA_REG_GLB_DCDC_CORE_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0068)
#define ANA_REG_GLB_DCDC_CORE_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x006C)
#define ANA_REG_GLB_DCDC_CORE_REG2      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0070)
#define ANA_REG_GLB_DCDC_CORE_VOL       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0074)
#define ANA_REG_GLB_DCDC_GPU_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0078)
#define ANA_REG_GLB_DCDC_GPU_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x007C)
#define ANA_REG_GLB_DCDC_GPU_REG2       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0080)
#define ANA_REG_GLB_DCDC_GPU_VOL        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0084)
#define ANA_REG_GLB_DCDC_GPU_PD_HW      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0088)
#define ANA_REG_GLB_DCDC_MEM_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x008C)
#define ANA_REG_GLB_DCDC_MEM_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0090)
#define ANA_REG_GLB_DCDC_MEM_REG2       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0094)
#define ANA_REG_GLB_DCDC_MEM_VOL        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0098)
#define ANA_REG_GLB_DCDC_GEN_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x009C)
#define ANA_REG_GLB_DCDC_GEN_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00A0)
#define ANA_REG_GLB_DCDC_GEN_REG2       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00A4)
#define ANA_REG_GLB_DCDC_GEN_VOL        SCI_ADDR(ANA_REGS_GLB_BASE, 0x00A8)
#define ANA_REG_GLB_DCDC_RF_REG0        SCI_ADDR(ANA_REGS_GLB_BASE, 0x00AC)
#define ANA_REG_GLB_DCDC_RF_REG1        SCI_ADDR(ANA_REGS_GLB_BASE, 0x00B0)
#define ANA_REG_GLB_DCDC_RF_REG2        SCI_ADDR(ANA_REGS_GLB_BASE, 0x00B4)
#define ANA_REG_GLB_DCDC_RF_VOL         SCI_ADDR(ANA_REGS_GLB_BASE, 0x00B8)
#define ANA_REG_GLB_RESEVED_DCDC_CON_REG0 SCI_ADDR(ANA_REGS_GLB_BASE, 0x00BC)
#define ANA_REG_GLB_RESEVED_DCDC_CON_REG1 SCI_ADDR(ANA_REGS_GLB_BASE, 0x00C0)
#define ANA_REG_GLB_RESEVED_DCDC_CON_REG2 SCI_ADDR(ANA_REGS_GLB_BASE, 0x00C4)
#define ANA_REG_GLB_RESEVED_DCDC_CON_VOL SCI_ADDR(ANA_REGS_GLB_BASE, 0x00C8)
#define ANA_REG_GLB_DCDC_WPA_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00CC)
#define ANA_REG_GLB_DCDC_WPA_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00D0)
#define ANA_REG_GLB_DCDC_WPA_REG2       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00D4)
#define ANA_REG_GLB_DCDC_WPA_VOL        SCI_ADDR(ANA_REGS_GLB_BASE, 0x00D8)
#define ANA_REG_GLB_DCDC_WPA_DCM_HW     SCI_ADDR(ANA_REGS_GLB_BASE, 0x00DC)
#define ANA_REG_GLB_DCDC_CH_CTRL        SCI_ADDR(ANA_REGS_GLB_BASE, 0x00E0)
#define ANA_REG_GLB_DCDC_REF_TRIM       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00E4)
#define ANA_REG_GLB_RESERVED_REG6       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00E8)
#define ANA_REG_GLB_RESERVED_REG7       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00EC)
#define ANA_REG_GLB_RESERVED_REG8       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00F0)
#define ANA_REG_GLB_RESERVED_REG9       SCI_ADDR(ANA_REGS_GLB_BASE, 0x00F4)
#define ANA_REG_GLB_RESERVED_REG10      SCI_ADDR(ANA_REGS_GLB_BASE, 0x00F8)
#define ANA_REG_GLB_LDO_CAMA0_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x00FC)
#define ANA_REG_GLB_LDO_CAMA0_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0100)
#define ANA_REG_GLB_LDO_CAMA1_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0104)
#define ANA_REG_GLB_LDO_CAMA1_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0108)
#define ANA_REG_GLB_LDO_CAMMOT_REG0     SCI_ADDR(ANA_REGS_GLB_BASE, 0x010C)
#define ANA_REG_GLB_LDO_CAMMOT_REG1     SCI_ADDR(ANA_REGS_GLB_BASE, 0x0110)
#define ANA_REG_GLB_LDO_SIM0_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0114)
#define ANA_REG_GLB_LDO_SIM0_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0118)
#define ANA_REG_GLB_LDO_SIM1_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x011C)
#define ANA_REG_GLB_LDO_SIM1_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0120)
#define ANA_REG_GLB_LDO_VLDO_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0124)
#define ANA_REG_GLB_LDO_VLDO_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0128)
#define ANA_REG_GLB_LDO_EMMCCORE_REG0   SCI_ADDR(ANA_REGS_GLB_BASE, 0x012C)
#define ANA_REG_GLB_LDO_EMMCCORE_REG1   SCI_ADDR(ANA_REGS_GLB_BASE, 0x0130)
#define ANA_REG_GLB_LDO_SD_REG0         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0134)
#define ANA_REG_GLB_LDO_SD_REG1         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0138)
#define ANA_REG_GLB_LDO_SDIO_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x013C)
#define ANA_REG_GLB_LDO_SDIO_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0140)
#define ANA_REG_GLB_LDO_VDD28_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0144)
#define ANA_REG_GLB_LDO_VDD28_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0148)
#define ANA_REG_GLB_LDO_WIFIPA_REG0     SCI_ADDR(ANA_REGS_GLB_BASE, 0x014C)
#define ANA_REG_GLB_LDO_WIFIPA_REG1     SCI_ADDR(ANA_REGS_GLB_BASE, 0x0150)
#define ANA_REG_GLB_LDO_DCXO_REG0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0154)
#define ANA_REG_GLB_LDO_DCXO_REG1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0158)
#define ANA_REG_GLB_LDO_USB33_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x015C)
#define ANA_REG_GLB_LDO_USB33_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0160)
#define ANA_REG_GLB_RESERVED_REG11      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0164)
#define ANA_REG_GLB_RESERVED_REG12      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0168)
#define ANA_REG_GLB_RESERVED_REG13      SCI_ADDR(ANA_REGS_GLB_BASE, 0x016C)
#define ANA_REG_GLB_RESERVED_REG14      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0170)
#define ANA_REG_GLB_RESERVED_REG15      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0174)
#define ANA_REG_GLB_RESERVED_REG16      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0178)
#define ANA_REG_GLB_LDO_CAMD0_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x017C)
#define ANA_REG_GLB_LDO_CAMD0_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0180)
#define ANA_REG_GLB_LDO_CAMD1_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0184)
#define ANA_REG_GLB_LDO_CAMD1_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0188)
#define ANA_REG_GLB_LDO_CON_REG0        SCI_ADDR(ANA_REGS_GLB_BASE, 0x018C)
#define ANA_REG_GLB_LDO_CON_REG1        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0190)
#define ANA_REG_GLB_LDO_CAMIO_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0194)
#define ANA_REG_GLB_LDO_CAMIO_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0198)
#define ANA_REG_GLB_LDO_AVDD18_REG0     SCI_ADDR(ANA_REGS_GLB_BASE, 0x019C)
#define ANA_REG_GLB_LDO_AVDD18_REG1     SCI_ADDR(ANA_REGS_GLB_BASE, 0x01A0)
#define ANA_REG_GLB_LDO_VDDRF_REG0      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01A4)
#define ANA_REG_GLB_LDO_VDDRF_REG1      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01A8)
#define ANA_REG_GLB_LDO_VDDSRAM_REG0    SCI_ADDR(ANA_REGS_GLB_BASE, 0x01AC)
#define ANA_REG_GLB_LDO_VDDSRAM_REG1    SCI_ADDR(ANA_REGS_GLB_BASE, 0x01B0)
#define ANA_REG_GLB_RESERVED_REG17      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01B4)
#define ANA_REG_GLB_RESERVED_REG18      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01B8)
#define ANA_REG_GLB_RESERVED_REG19      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01BC)
#define ANA_REG_GLB_RESERVED_REG20      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01C0)
#define ANA_REG_GLB_RESERVED_REG21      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01C4)
#define ANA_REG_GLB_RESERVED_REG22      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01C8)
#define ANA_REG_GLB_LDO_CH_CTRL         SCI_ADDR(ANA_REGS_GLB_BASE, 0x01CC)
#define ANA_REG_GLB_DCDC_CLK_CTRL       SCI_ADDR(ANA_REGS_GLB_BASE, 0x01D0)
#define ANA_REG_GLB_RESERVED_REG23      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01D4)
#define ANA_REG_GLB_RESERVED_REG24      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01D8)
#define ANA_REG_GLB_RESERVED_REG25      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01DC)
#define ANA_REG_GLB_RESERVED_REG26      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01E0)
#define ANA_REG_GLB_RESERVED_REG27      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01E4)
#define ANA_REG_GLB_RESERVED_REG28      SCI_ADDR(ANA_REGS_GLB_BASE, 0x01E8)
#define ANA_REG_GLB_SLP_WAIT_DCDCARM    SCI_ADDR(ANA_REGS_GLB_BASE, 0x01EC)
#define ANA_REG_GLB_SLP_CTRL            SCI_ADDR(ANA_REGS_GLB_BASE, 0x01F0)
#define ANA_REG_GLB_SLP_DCDC_PD_CTRL    SCI_ADDR(ANA_REGS_GLB_BASE, 0x01F4)
#define ANA_REG_GLB_SLP_LDO_PD_CTRL0    SCI_ADDR(ANA_REGS_GLB_BASE, 0x01F8)
#define ANA_REG_GLB_SLP_LDO_PD_CTRL1    SCI_ADDR(ANA_REGS_GLB_BASE, 0x01FC)
#define ANA_REG_GLB_SLP_DCDC_LP_CTRL    SCI_ADDR(ANA_REGS_GLB_BASE, 0x0200)
#define ANA_REG_GLB_SLP_LDO_LP_CTRL0    SCI_ADDR(ANA_REGS_GLB_BASE, 0x0204)
#define ANA_REG_GLB_SLP_LDO_LP_CTRL1    SCI_ADDR(ANA_REGS_GLB_BASE, 0x0208)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL0 SCI_ADDR(ANA_REGS_GLB_BASE, 0x020C)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL1 SCI_ADDR(ANA_REGS_GLB_BASE, 0x0210)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL2 SCI_ADDR(ANA_REGS_GLB_BASE, 0x0214)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL3 SCI_ADDR(ANA_REGS_GLB_BASE, 0x0218)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL4 SCI_ADDR(ANA_REGS_GLB_BASE, 0x021C)
#define ANA_REG_GLB_DCDC_CORE_SLP_CTRL5 SCI_ADDR(ANA_REGS_GLB_BASE, 0x0220)
#define ANA_REG_GLB_DCDC_GPU_SLP_CTRL0  SCI_ADDR(ANA_REGS_GLB_BASE, 0x0224)
#define ANA_REG_GLB_DCDC_GPU_SLP_CTRL1  SCI_ADDR(ANA_REGS_GLB_BASE, 0x0228)
#define ANA_REG_GLB_DCDC_GPU_SLP_CTRL2  SCI_ADDR(ANA_REGS_GLB_BASE, 0x022C)
#define ANA_REG_GLB_DCDC_GPU_SLP_CTRL3  SCI_ADDR(ANA_REGS_GLB_BASE, 0x0230)
#define ANA_REG_GLB_DCDC_GPU_SLP_CTRL4  SCI_ADDR(ANA_REGS_GLB_BASE, 0x0234)
#define ANA_REG_GLB_DCDC_GPU_SLP_CTRL5  SCI_ADDR(ANA_REGS_GLB_BASE, 0x0238)
#define ANA_REG_GLB_DCDC_XTL_EN0        SCI_ADDR(ANA_REGS_GLB_BASE, 0x023C)
#define ANA_REG_GLB_DCDC_XTL_EN1        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0240)
#define ANA_REG_GLB_DCDC_XTL_EN2        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0244)
#define ANA_REG_GLB_DCDC_XTL_EN3        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0248)
#define ANA_REG_GLB_RESEVED_DCDC_XTL_EN4 SCI_ADDR(ANA_REGS_GLB_BASE, 0x024C)
#define ANA_REG_GLB_LDO_XTL_EN0         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0250)
#define ANA_REG_GLB_LDO_XTL_EN1         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0254)
#define ANA_REG_GLB_LDO_XTL_EN2         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0258)
#define ANA_REG_GLB_LDO_XTL_EN3         SCI_ADDR(ANA_REGS_GLB_BASE, 0x025C)
#define ANA_REG_GLB_LDO_XTL_EN4         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0260)
#define ANA_REG_GLB_LDO_XTL_EN5         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0264)
#define ANA_REG_GLB_LDO_XTL_EN6         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0268)
#define ANA_REG_GLB_LDO_XTL_EN7         SCI_ADDR(ANA_REGS_GLB_BASE, 0x026C)
#define ANA_REG_GLB_LDO_XTL_EN8         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0270)
#define ANA_REG_GLB_LDO_XTL_EN9         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0274)
#define ANA_REG_GLB_LDO_XTL_EN10        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0278)
#define ANA_REG_GLB_XO_BG_XTL_EN        SCI_ADDR(ANA_REGS_GLB_BASE, 0x027C)
#define ANA_REG_GLB_RESERVED_REG29      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0280)
#define ANA_REG_GLB_RESERVED_REG30      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0284)
#define ANA_REG_GLB_RESERVED_REG31      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0288)
#define ANA_REG_GLB_RESERVED_REG32      SCI_ADDR(ANA_REGS_GLB_BASE, 0x028C)
#define ANA_REG_GLB_RESERVED_REG33      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0290)
#define ANA_REG_GLB_RESERVED_REG34      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0294)
#define ANA_REG_GLB_DCDC_VLG_SEL        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0298)
#define ANA_REG_GLB_LDO_VLG_SEL0        SCI_ADDR(ANA_REGS_GLB_BASE, 0x029C)
#define ANA_REG_GLB_LDO_VLG_SEL1        SCI_ADDR(ANA_REGS_GLB_BASE, 0x02A0)
#define ANA_REG_GLB_ANA_32KLESS_CTRL0   SCI_ADDR(ANA_REGS_GLB_BASE, 0x02A4)
#define ANA_REG_GLB_ANA_32KLESS_CTRL1   SCI_ADDR(ANA_REGS_GLB_BASE, 0x02A8)
#define ANA_REG_GLB_ANA_32KLESS_CTRL2   SCI_ADDR(ANA_REGS_GLB_BASE, 0x02AC)
#define ANA_REG_GLB_ANA_32KLESS_CTRL3   SCI_ADDR(ANA_REGS_GLB_BASE, 0x02B0)
#define ANA_REG_GLB_AUXAD_CTL           SCI_ADDR(ANA_REGS_GLB_BASE, 0x02B4)
#define ANA_REG_GLB_XTL_WAIT_CTRL       SCI_ADDR(ANA_REGS_GLB_BASE, 0x02B8)
#define ANA_REG_GLB_RGB_CTRL            SCI_ADDR(ANA_REGS_GLB_BASE, 0x02BC)
#define ANA_REG_GLB_WHTLED_CTRL         SCI_ADDR(ANA_REGS_GLB_BASE, 0x02C0)
#define ANA_REG_GLB_KPLED_CTRL          SCI_ADDR(ANA_REGS_GLB_BASE, 0x02C4)
#define ANA_REG_GLB_VIBR_CTRL0          SCI_ADDR(ANA_REGS_GLB_BASE, 0x02C8)
#define ANA_REG_GLB_VIBR_CTRL1          SCI_ADDR(ANA_REGS_GLB_BASE, 0x02CC)
#define ANA_REG_GLB_AUDIO_CTRL0         SCI_ADDR(ANA_REGS_GLB_BASE, 0x02D0)
#define ANA_REG_GLB_AUDIO_CTRL1         SCI_ADDR(ANA_REGS_GLB_BASE, 0x02D4)
#define ANA_REG_GLB_CHGR_DET_FGU_CTRL   SCI_ADDR(ANA_REGS_GLB_BASE, 0x02D8)
#define ANA_REG_GLB_CHGR_STATUS         SCI_ADDR(ANA_REGS_GLB_BASE, 0x02DC)
#define ANA_REG_GLB_MIXED_CTRL0         SCI_ADDR(ANA_REGS_GLB_BASE, 0x02E0)
#define ANA_REG_GLB_MIXED_CTRL1         SCI_ADDR(ANA_REGS_GLB_BASE, 0x02E4)
#define ANA_REG_GLB_POR_RST_MONITOR     SCI_ADDR(ANA_REGS_GLB_BASE, 0x02E8)
#define ANA_REG_GLB_WDG_RST_MONITOR     SCI_ADDR(ANA_REGS_GLB_BASE, 0x02EC)
#define ANA_REG_GLB_POR_PIN_RST_MONITOR SCI_ADDR(ANA_REGS_GLB_BASE, 0x02F0)
#define ANA_REG_GLB_POR_SRC_FLAG        SCI_ADDR(ANA_REGS_GLB_BASE, 0x02F4)
#define ANA_REG_GLB_POR_7S_CTRL         SCI_ADDR(ANA_REGS_GLB_BASE, 0x02F8)
#define ANA_REG_GLB_HWRST_RTC           SCI_ADDR(ANA_REGS_GLB_BASE, 0x02FC)
#define ANA_REG_GLB_ARM_MF_REG          SCI_ADDR(ANA_REGS_GLB_BASE, 0x0300)
#define ANA_REG_GLB_ARCH_EN             SCI_ADDR(ANA_REGS_GLB_BASE, 0x0304)
#define ANA_REG_GLB_MCU_WR_PROT_VALUE   SCI_ADDR(ANA_REGS_GLB_BASE, 0x0308)
#define ANA_REG_GLB_PWR_WR_PROT_VALUE   SCI_ADDR(ANA_REGS_GLB_BASE, 0x030C)
#define ANA_REG_GLB_SMPL_CTRL0          SCI_ADDR(ANA_REGS_GLB_BASE, 0x0310)
#define ANA_REG_GLB_SMPL_CTRL1          SCI_ADDR(ANA_REGS_GLB_BASE, 0x0314)
#define ANA_REG_GLB_RTC_RST0            SCI_ADDR(ANA_REGS_GLB_BASE, 0x0318)
#define ANA_REG_GLB_RTC_RST1            SCI_ADDR(ANA_REGS_GLB_BASE, 0x031C)
#define ANA_REG_GLB_RTC_RST2            SCI_ADDR(ANA_REGS_GLB_BASE, 0x0320)
#define ANA_REG_GLB_BATDET_CUR_CTRL     SCI_ADDR(ANA_REGS_GLB_BASE, 0x0324)
#define ANA_REG_GLB_RTC_CLK_STOP        SCI_ADDR(ANA_REGS_GLB_BASE, 0x0328)
#define ANA_REG_GLB_VBAT_DROP_CNT       SCI_ADDR(ANA_REGS_GLB_BASE, 0x032C)
#define ANA_REG_GLB_SWRST_CTRL0         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0330)
#define ANA_REG_GLB_SWRST_CTRL1         SCI_ADDR(ANA_REGS_GLB_BASE, 0x0334)
#define ANA_REG_GLB_OSC_CTRL            SCI_ADDR(ANA_REGS_GLB_BASE, 0x0338)
#define ANA_REG_GLB_OTP_CTRL            SCI_ADDR(ANA_REGS_GLB_BASE, 0x033C)
#define ANA_REG_GLB_SWLPRO_CTRL_0       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0340)
#define ANA_REG_GLB_SWLPRO_CTRL_1       SCI_ADDR(ANA_REGS_GLB_BASE, 0x0344)
#define ANA_REG_GLB_FREE_TIMER_LOW      SCI_ADDR(ANA_REGS_GLB_BASE, 0x0348)
#define ANA_REG_GLB_FREE_TIMER_HIGH     SCI_ADDR(ANA_REGS_GLB_BASE, 0x034C)
#define ANA_REG_GLB_RC_CTRL_0           SCI_ADDR(ANA_REGS_GLB_BASE, 0x0350)

#define ANA_REG_GLB_ARM_MODULE_EN	ANA_REG_GLB_MODULE_EN0
#define ANA_REG_GLB_RTC_CLK_EN		ANA_REG_GLB_RTC_CLK_EN0
#define ANA_REG_GLB_ARM_CLK_EN		ANA_REG_GLB_ARM_CLK_EN0


/* bits definitions for register ANA_REG_GLB_CHIP_ID_LOW */
#define BITS_CHIP_ID_LOW(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_CHIP_ID_HIGH */
#define BITS_CHIP_ID_HIGH(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_MODULE_EN0 */
#define BIT_ANA_THM_EN                      ( BIT(10) )
#define BIT_ANA_BLTC_EN                     ( BIT(9) )
#define BIT_ANA_PINREG_EN                   ( BIT(8) )
#define BIT_ANA_FGU_EN                      ( BIT(7) )
#define BIT_ANA_EFS_EN                      ( BIT(6) )
#define BIT_ANA_ADC_EN                      ( BIT(5) )
#define BIT_ANA_AUD_EN                      ( BIT(4) )
#define BIT_ANA_EIC_EN                      ( BIT(3) )
#define BIT_ANA_WDG_EN                      ( BIT(2) )
#define BIT_ANA_RTC_EN                      ( BIT(1) )
#define BIT_ANA_CAL_EN                      ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_MODULE_EN1 */
#define BIT_ANA_SWITCH_CHG_EN               ( BIT(5) )
#define BIT_ANA_BIF_EN                      ( BIT(4) )
#define BIT_ANA_TYPEC_EN                    ( BIT(3) )
#define BIT_ANA_CHG_WDG_EN                  ( BIT(2) )
#define BIT_ANA_TMR_EN                      ( BIT(1) )
#define BIT_ANA_FAST_CHG_EN                 ( BIT(0) )


/* bits definitions for register ANA_REG_GLB_ARM_CLK_EN0 */
#define BIT_CLK_AUD_SCLK_EN             ( BIT(7) )
#define BIT_CLK_AUXAD_EN                ( BIT(6) )
#define BIT_CLK_AUXADC_EN               ( BIT(5) )
#define BITS_CLK_CAL_SRC_SEL(_X_)       ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BIT_CLK_CAL_EN                  ( BIT(2) )
#define BIT_CLK_AUD_IF_6P5M_EN          ( BIT(1) )
#define BIT_CLK_AUD_IF_EN               ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_ARM_CLK_EN1 */
#define BIT_CLK_BIF_EN                  ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_RTC_CLK_EN0 */
#define BIT_RTC_FLASH_EN                ( BIT(12) )
#define BIT_RTC_EFS_EN                  ( BIT(11) )
#define BIT_RTC_THMA_AUTO_EN            ( BIT(10) )
#define BIT_RTC_THMA_EN                 ( BIT(9) )
#define BIT_RTC_THM_EN                  ( BIT(8) )
#define BIT_RTC_BLTC_EN                 ( BIT(7) )
#define BIT_RTC_FGU_EN                  ( BIT(6) )
#define BIT_RTC_FGUA_EN                 ( BIT(5) )
#define BIT_RTC_VIBR_EN                 ( BIT(4) )
#define BIT_RTC_EIC_EN                  ( BIT(3) )
#define BIT_RTC_WDG_EN                  ( BIT(2) )
#define BIT_RTC_RTC_EN                  ( BIT(1) )
#define BIT_RTC_ARCH_EN                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_RTC_CLK_EN1 */
#define BIT_RTC_SWITCH_CHG_EN           ( BIT(4) )
#define BIT_RTC_TYPEC_EN                ( BIT(3) )
#define BIT_RTC_CHG_WDG_EN              ( BIT(2) )
#define BIT_RTC_TMR_EN                  ( BIT(1) )
#define BIT_RTC_FAST_CHG_EN             ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SOFT_RST0 */
#define BIT_AUDRX_SOFT_RST              ( BIT(13) )
#define BIT_AUDTX_SOFT_RST              ( BIT(12) )
#define BIT_THMA_SOFT_RST               ( BIT(11) )
#define BIT_THM_SOFT_RST                ( BIT(10) )
#define BIT_BLTC_SOFT_RST               ( BIT(9) )
#define BIT_AUD_IF_SOFT_RST             ( BIT(8) )
#define BIT_EFS_SOFT_RST                ( BIT(7) )
#define BIT_ADC_SOFT_RST                ( BIT(6) )
#define BIT_PWM0_SOFT_RST               ( BIT(5) )
#define BIT_FGU_SOFT_RST                ( BIT(4) )
#define BIT_EIC_SOFT_RST                ( BIT(3) )
#define BIT_WDG_SOFT_RST                ( BIT(2) )
#define BIT_RTC_SOFT_RST                ( BIT(1) )
#define BIT_CAL_SOFT_RST                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SOFT_RST1 */
#define BIT_SWITCH_CHG_SOFT_RST         ( BIT(5) )
#define BIT_BIF_SOFT_RST                ( BIT(4) )
#define BIT_TYPEC_SOFT_RST              ( BIT(3) )
#define BIT_CHG_WDG_SOFT_RST            ( BIT(2) )
#define BIT_TMR_SOFT_RST                ( BIT(1) )
#define BIT_FAST_CHG_SOFT_RST           ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_POWER_PD_SW */
#define BIT_DCDC_RF_PD                  ( BIT(11) )
#define BIT_LDO_EMM_PD                  ( BIT(10) )
#define BIT_DCDC_TOPCLK6M_PD            ( BIT(9) )
#define BIT_DCDC_GEN_PD                 ( BIT(8) )
#define BIT_DCDC_MEM_PD                 ( BIT(7) )
#define BIT_DCDC_CORE_PD                ( BIT(6) )
#define BIT_DCDC_GPU_PD_SW              ( BIT(5) )
#define BIT_DCDC_ARM0_PD                ( BIT(4) )
#define BIT_DCDC_ARM1_PD                ( BIT(3) )
#define BIT_LDO_AVDD18_PD               ( BIT(2) )
#define BIT_LDO_VDD28_PD                ( BIT(1) )
#define BIT_BG_PD                       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_POWER_PD_HW */
#define BIT_PWR_OFF_SEQ_EN              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_RESERVED_REG0 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG1 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG2 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG3 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG4 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG5 */

/* bits definitions for register ANA_REG_GLB_DCDC_ARM0_REG0 */
#define BITS_DCDC_ARM0_DEADTIME(_X_)    ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DCDC_ARM0_STBOP(_X_)       ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_DCDC_ARM0_PDRSLOW(_X_)     ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_DCDC_ARM0_QKRSPS            ( BIT(3) )
#define BIT_DCDC_ARM0_PFM               ( BIT(2) )
#define BIT_DCDC_ARM0_DCM               ( BIT(1) )
#define BIT_DCDC_ARM0_LP_EN             ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_ARM0_REG1 */
#define BIT_DCDC_ARM0_OSCSYCEN_SW       ( BIT(15) )
#define BIT_DCDC_ARM0_OSCSYCEN_HW_EN    ( BIT(14) )
#define BIT_DCDC_ARM0_OSCSYC_DIV_EN     ( BIT(13) )
#define BITS_DCDC_ARM0_OSCSYC_DIV(_X_)  ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BIT_DCDC_ARM0_DUALSEL           ( BIT(7) )
#define BIT_DCDC_ARM0_MERGEEN           ( BIT(6) )
#define BITS_DCDC_ARM0_CF(_X_)          ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DCDC_ARM0_PFMAD(_X_)       ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_DCDC_ARM0_CL_CTRL(_X_)     ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DCDC_ARM0_REG2 */
#define BITS_DCDC_ARM0_DISCHRG(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BITS_DCDC_ARM0_EFFIMPRV(_X_)    ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_DCDC_ARM0_EFFIMPRV_EN       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_ARM0_VOL */
#define BITS_DCDC_ARM0_CTRL(_X_)        ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_ARM0_CAL(_X_)         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_ARM1_REG0 */
#define BITS_DCDC_ARM1_DEADTIME(_X_)    ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DCDC_ARM1_STBOP(_X_)       ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_DCDC_ARM1_PDRSLOW(_X_)     ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_DCDC_ARM1_QKRSPS            ( BIT(3) )
#define BIT_DCDC_ARM1_PFM               ( BIT(2) )
#define BIT_DCDC_ARM1_DCM               ( BIT(1) )
#define BIT_DCDC_ARM1_LP_EN             ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_ARM1_REG1 */
#define BIT_DCDC_ARM1_OSCSYCEN_SW       ( BIT(15) )
#define BIT_DCDC_ARM1_OSCSYCEN_HW_EN    ( BIT(14) )
#define BIT_DCDC_ARM1_OSCSYC_DIV_EN     ( BIT(13) )
#define BITS_DCDC_ARM1_OSCSYC_DIV(_X_)  ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BIT_DCDC_ARM1_DUALSEL           ( BIT(7) )
#define BIT_DCDC_ARM1_MERGEEN           ( BIT(6) )
#define BITS_DCDC_ARM1_CF(_X_)          ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DCDC_ARM1_PFMAD(_X_)       ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_DCDC_ARM1_CL_CTRL(_X_)     ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DCDC_ARM1_REG2 */
#define BIT_DCDC_ARM1_APSEN             ( BIT(8) )
#define BIT_DCDC_ARM1_DPSC              ( BIT(7) )
#define BITS_DCDC_ARM1_APSDELAY(_X_)    ( (_X_) << 5 & (BIT(5)|BIT(6)) )
#define BITS_DCDC_ARM1_DISCHRG(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BITS_DCDC_ARM1_EFFIMPRV(_X_)    ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_DCDC_ARM1_EFFIMPRV_EN       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_ARM1_VOL */
#define BITS_DCDC_ARM1_CTRL(_X_)        ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_ARM1_CAL(_X_)         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_REG0 */
#define BITS_DCDC_CORE_DEADTIME(_X_)    ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DCDC_CORE_STBOP(_X_)       ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_DCDC_CORE_PDRSLOW(_X_)     ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_DCDC_CORE_QKRSPS            ( BIT(3) )
#define BIT_DCDC_CORE_PFM               ( BIT(2) )
#define BIT_DCDC_CORE_DCM               ( BIT(1) )
#define BIT_DCDC_CORE_LP_EN             ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_REG1 */
#define BIT_DCDC_CORE_OSCSYCEN_SW       ( BIT(15) )
#define BIT_DCDC_CORE_OSCSYCEN_HW_EN    ( BIT(14) )
#define BIT_DCDC_CORE_OSCSYC_DIV_EN     ( BIT(13) )
#define BITS_DCDC_CORE_OSCSYC_DIV(_X_)  ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BIT_DCDC_CORE_DUALSEL           ( BIT(7) )
#define BIT_DCDC_CORE_MERGEEN           ( BIT(6) )
#define BITS_DCDC_CORE_CF(_X_)          ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DCDC_CORE_PFMAD(_X_)       ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_DCDC_CORE_CL_CTRL(_X_)     ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_REG2 */
#define BIT_DCDC_CORE_APSEN             ( BIT(8) )
#define BIT_DCDC_CORE_DPSC              ( BIT(7) )
#define BITS_DCDC_CORE_APSDELAY(_X_)    ( (_X_) << 5 & (BIT(5)|BIT(6)) )
#define BITS_DCDC_CORE_DISCHRG(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BITS_DCDC_CORE_EFFIMPRV(_X_)    ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_DCDC_CORE_EFFIMPRV_EN       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_VOL */
#define BITS_DCDC_CORE_CTRL(_X_)        ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_CORE_CAL(_X_)         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_REG0 */
#define BITS_DCDC_GPU_DEADTIME(_X_)     ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DCDC_GPU_STBOP(_X_)        ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_DCDC_GPU_PDRSLOW(_X_)      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_DCDC_GPU_QKRSPS             ( BIT(3) )
#define BIT_DCDC_GPU_PFM                ( BIT(2) )
#define BIT_DCDC_GPU_DCM                ( BIT(1) )
#define BIT_DCDC_GPU_LP_EN              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_REG1 */
#define BIT_DCDC_GPU_OSCSYCEN_SW        ( BIT(15) )
#define BIT_DCDC_GPU_OSCSYCEN_HW_EN     ( BIT(14) )
#define BIT_DCDC_GPU_OSCSYC_DIV_EN      ( BIT(13) )
#define BITS_DCDC_GPU_OSCSYC_DIV(_X_)   ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BIT_DCDC_GPU_DUALSEL            ( BIT(7) )
#define BIT_DCDC_GPU_MERGEEN            ( BIT(6) )
#define BITS_DCDC_GPU_CF(_X_)           ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DCDC_GPU_PFMAD(_X_)        ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_DCDC_GPU_CL_CTRL(_X_)      ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_REG2 */
#define BIT_DCDC_GPU_PD_SEL             ( BIT(5) )
#define BITS_DCDC_GPU_DISCHRG(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BITS_DCDC_GPU_EFFIMPRV(_X_)     ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_DCDC_GPU_EFFIMPRV_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_VOL */
#define BITS_DCDC_GPU_CTRL(_X_)         ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_GPU_CAL(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_PD_HW */
#define BIT_DCDC_GPU_PD_HW              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_MEM_REG0 */
#define BITS_DCDC_MEM_DEADTIME(_X_)     ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DCDC_MEM_STBOP(_X_)        ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_DCDC_MEM_PDRSLOW(_X_)      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_DCDC_MEM_QKRSPS             ( BIT(3) )
#define BIT_DCDC_MEM_PFM                ( BIT(2) )
#define BIT_DCDC_MEM_DCM                ( BIT(1) )
#define BIT_DCDC_MEM_LP_EN              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_MEM_REG1 */
#define BIT_DCDC_MEM_OSCSYCEN_SW        ( BIT(13) )
#define BIT_DCDC_MEM_OSCSYCEN_HW_EN     ( BIT(12) )
#define BIT_DCDC_MEM_OSCSYC_DIV_EN      ( BIT(11) )
#define BITS_DCDC_MEM_OSCSYC_DIV(_X_)   ( (_X_) << 6 & (BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )
#define BITS_DCDC_MEM_CF(_X_)           ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DCDC_MEM_PFMAD(_X_)        ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_DCDC_MEM_CL_CTRL(_X_)      ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DCDC_MEM_REG2 */
#define BITS_DCDC_MEM_DISCHRG(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BITS_DCDC_MEM_EFFIMPRV(_X_)     ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_DCDC_MEM_EFFIMPRV_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_MEM_VOL */
#define BITS_DCDC_MEM_CTRL(_X_)         ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_MEM_CAL(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GEN_REG0 */
#define BITS_DCDC_GEN_DEADTIME(_X_)     ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DCDC_GEN_STBOP(_X_)        ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_DCDC_GEN_PDRSLOW(_X_)      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_DCDC_GEN_QKRSPS             ( BIT(3) )
#define BIT_DCDC_GEN_PFM                ( BIT(2) )
#define BIT_DCDC_GEN_DCM                ( BIT(1) )
#define BIT_DCDC_GEN_LP_EN              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_GEN_REG1 */
#define BIT_DCDC_GEN_OSCSYCEN_SW        ( BIT(13) )
#define BIT_DCDC_GEN_OSCSYCEN_HW_EN     ( BIT(12) )
#define BIT_DCDC_GEN_OSCSYC_DIV_EN      ( BIT(11) )
#define BITS_DCDC_GEN_OSCSYC_DIV(_X_)   ( (_X_) << 6 & (BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)) )
#define BITS_DCDC_GEN_CF(_X_)           ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DCDC_GEN_PFMAD(_X_)        ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_DCDC_GEN_CL_CTRL(_X_)      ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GEN_REG2 */
#define BITS_DCDC_GEN_DISCHRG(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BITS_DCDC_GEN_EFFIMPRV(_X_)     ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_DCDC_GEN_EFFIMPRV_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_GEN_VOL */
#define BITS_DCDC_GEN_CTRL(_X_)         ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_GEN_CAL(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_RF_REG0 */
#define BITS_DCDC_RF_DEADTIME(_X_)      ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DCDC_RF_STBOP(_X_)         ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_DCDC_RF_PDRSLOW(_X_)       ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_DCDC_RF_QKRSPS              ( BIT(3) )
#define BIT_DCDC_RF_PFM                 ( BIT(2) )
#define BIT_DCDC_RF_DCM                 ( BIT(1) )
#define BIT_DCDC_RF_LP_EN               ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_RF_REG1 */
#define BIT_DCDC_RF_OSCSYCEN_SW         ( BIT(14) )
#define BIT_DCDC_RF_OSCSYCEN_HW_EN      ( BIT(13) )
#define BIT_DCDC_RF_OSCSYC_DIV_EN       ( BIT(12) )
#define BITS_DCDC_RF_OSCSYC_DIV(_X_)    ( (_X_) << 7 & (BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_DCDC_RF_MERGEEN             ( BIT(6) )
#define BITS_DCDC_RF_CF(_X_)            ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DCDC_RF_PFMAD(_X_)         ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_DCDC_RF_CL_CTRL(_X_)       ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DCDC_RF_REG2 */
#define BITS_DCDC_RF_DISCHRG(_X_)       ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BITS_DCDC_RF_EFFIMPRV(_X_)      ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_DCDC_RF_EFFIMPRV_EN         ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_RF_VOL */
#define BITS_DCDC_RF_CTRL(_X_)          ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_RF_CAL(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_RESEVED_DCDC_CON_REG0 */

/* bits definitions for register ANA_REG_GLB_RESEVED_DCDC_CON_REG1 */

/* bits definitions for register ANA_REG_GLB_RESEVED_DCDC_CON_REG2 */

/* bits definitions for register ANA_REG_GLB_RESEVED_DCDC_CON_VOL */

/* bits definitions for register ANA_REG_GLB_DCDC_WPA_REG0 */
#define BITS_DCDC_WPA_DEADTIME(_X_)     ( (_X_) << 14 & (BIT(14)|BIT(15)) )
#define BITS_DCDC_WPA_STBOP(_X_)        ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_DCDC_WPA_PDRSLOW(_X_)      ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_DCDC_WPA_QKRSPS             ( BIT(3) )
#define BIT_DCDC_WPA_PFM                ( BIT(2) )
#define BIT_DCDC_WPA_LP_EN              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_WPA_REG1 */
#define BIT_DCDC_WPA_BPOUT_SOFTW        ( BIT(15) )
#define BITS_DCDC_WPA_VBAT_DIV(_X_)     ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)) )
#define BIT_DCDC_WPA_BPEN               ( BIT(11) )
#define BIT_DCDC_WPA_BPMODE             ( BIT(10) )
#define BIT_DCDC_WPA_DEGEN              ( BIT(9) )
#define BIT_DCDC_WPA_APTEN              ( BIT(8) )
#define BITS_DCDC_WPA_DEBC_SEL(_X_)     ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_DCDC_WPA_CF(_X_)           ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_DCDC_WPA_PFMAD(_X_)        ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_DCDC_WPA_CL_CTRL(_X_)      ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_DCDC_WPA_REG2 */
#define BIT_DCDC_WPA_PD                 ( BIT(13) )
#define BIT_DCDC_WPA_OSCSYCEN_SW        ( BIT(12) )
#define BIT_DCDC_WPA_OSCSYCEN_HW_EN     ( BIT(11) )
#define BIT_DCDC_WPA_OSCSYC_DIV_EN      ( BIT(10) )
#define BITS_DCDC_WPA_OSCSYC_DIV(_X_)   ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_WPA_DISCHRG(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BITS_DCDC_WPA_EFFIMPRV(_X_)     ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_DCDC_WPA_EFFIMPRV_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_WPA_VOL */
#define BITS_DCDC_WPA_CTRL(_X_)         ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_WPA_CAL(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_WPA_DCM_HW */
#define BIT_DCDC_WPA_DCM_HW             ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_CH_CTRL */
#define BITS_DCDC_CAL_SEL(_X_)          ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_DCDC_REF_TRIM */
#define BITS_DCDC_VREF_CAL(_X_)         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_RESERVED_REG6 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG7 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG8 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG9 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG10 */

/* bits definitions for register ANA_REG_GLB_LDO_CAMA0_REG0 */
#define BIT_LDOCAMA0_LP_EN_SW           ( BIT(8) )
#define BITS_LDO_CAMA0_REFTRIM(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_CAMA0_EADBIAS_EN        ( BIT(2) )
#define BIT_LDO_CAMA0_SHPT_PD           ( BIT(1) )
#define BIT_LDO_CAMA0_PD                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMA0_REG1 */
#define BITS_LDO_CAMA0_V(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMA1_REG0 */
#define BIT_LDOCAMA1_LP_EN_SW           ( BIT(8) )
#define BITS_LDO_CAMA1_REFTRIM(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_CAMA1_EADBIAS_EN        ( BIT(2) )
#define BIT_LDO_CAMA1_SHPT_PD           ( BIT(1) )
#define BIT_LDO_CAMA1_PD                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMA1_REG1 */
#define BITS_LDO_CAMA1_V(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMMOT_REG0 */
#define BIT_LDOCAMMOT_LP_EN_SW          ( BIT(8) )
#define BITS_LDO_CAMMOT_REFTRIM(_X_)    ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_CAMMOT_EADBIAS_EN       ( BIT(2) )
#define BIT_LDO_CAMMOT_SHPT_PD          ( BIT(1) )
#define BIT_LDO_CAMMOT_PD               ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMMOT_REG1 */
#define BITS_LDO_CAMMOT_V(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_SIM0_REG0 */
#define BIT_LDOSIM0_LP_EN_SW            ( BIT(8) )
#define BITS_LDO_SIM0_REFTRIM(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_SIM0_EADBIAS_EN         ( BIT(2) )
#define BIT_LDO_SIM0_SHPT_PD            ( BIT(1) )
#define BIT_LDO_SIM0_PD                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_SIM0_REG1 */
#define BITS_LDO_SIM0_V(_X_)            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_SIM1_REG0 */
#define BIT_LDOSIM1_LP_EN_SW            ( BIT(8) )
#define BITS_LDO_SIM1_REFTRIM(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_SIM1_EADBIAS_EN         ( BIT(2) )
#define BIT_LDO_SIM1_SHPT_PD            ( BIT(1) )
#define BIT_LDO_SIM1_PD                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_SIM1_REG1 */
#define BITS_LDO_SIM1_V(_X_)            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_VLDO_REG0 */
#define BIT_LDOVLDO_LP_EN_SW            ( BIT(8) )
#define BITS_LDO_VLDO_REFTRIM(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_VLDO_EADBIAS_EN         ( BIT(2) )
#define BIT_LDO_VLDO_SHPT_PD            ( BIT(1) )
#define BIT_LDO_VLDO_PD                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_VLDO_REG1 */
#define BITS_LDO_VLDO_V(_X_)            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_EMMCCORE_REG0 */
#define BIT_LDOEMMCCORE_LP_EN_SW        ( BIT(8) )
#define BITS_LDO_EMMCCORE_REFTRIM(_X_)  ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_EMMCCORE_EADBIAS_EN     ( BIT(2) )
#define BIT_LDO_EMMCCORE_SHPT_PD        ( BIT(1) )
#define BIT_LDO_EMMCCORE_PD             ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_EMMCCORE_REG1 */
#define BITS_LDO_EMMCCORE_V(_X_)        ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_SD_REG0 */
#define BIT_LDOSDCORE_LP_EN_SW          ( BIT(8) )
#define BITS_LDO_SDCORE_REFTRIM(_X_)    ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_SDCORE_EADBIAS_EN       ( BIT(2) )
#define BIT_LDO_SDCORE_SHPT_PD          ( BIT(1) )
#define BIT_LDO_SDCORE_PD               ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_SD_REG1 */
#define BITS_LDO_SDCORE_V(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_SDIO_REG0 */
#define BIT_LDOSDIO_LP_EN_SW            ( BIT(8) )
#define BITS_LDO_SDIO_REFTRIM(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_SDIO_EADBIAS_EN         ( BIT(2) )
#define BIT_LDO_SDIO_SHPT_PD            ( BIT(1) )
#define BIT_LDO_SDIO_PD                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_SDIO_REG1 */
#define BITS_LDO_SDIO_V(_X_)            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_VDD28_REG0 */
#define BIT_LDOVDD28_LP_EN_SW           ( BIT(8) )
#define BITS_LDO_VDD28_REFTRIM(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_VDD28_EADBIAS_EN        ( BIT(2) )
#define BIT_LDO_VDD28_SHPT_PD           ( BIT(1) )

/* bits definitions for register ANA_REG_GLB_LDO_VDD28_REG1 */
#define BITS_LDO_VDD28_V(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_WIFIPA_REG0 */
#define BIT_LDOWIFIPA_LP_EN_SW          ( BIT(8) )
#define BITS_LDO_WIFIPA_REFTRIM(_X_)    ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_WIFIPA_EADBIAS_EN       ( BIT(2) )
#define BIT_LDO_WIFIPA_SHPT_PD          ( BIT(1) )
#define BIT_LDO_WIFIPA_PD               ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_WIFIPA_REG1 */
#define BITS_LDO_WIFIPA_V(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_DCXO_REG0 */
#define BIT_LDODCXO_LP_EN_SW            ( BIT(8) )
#define BITS_LDO_DCXO_REFTRIM(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_DCXO_EADBIAS_EN         ( BIT(2) )
#define BIT_LDO_DCXO_SHPT_PD            ( BIT(1) )
#define BIT_LDO_DCXO_PD                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_DCXO_REG1 */
#define BITS_LDO_DCXO_V(_X_)            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_LDO_USB33_REG0 */
#define BIT_LDOUSB33_LP_EN_SW           ( BIT(8) )
#define BITS_LDO_USB33_REFTRIM(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_USB33_EADBIAS_EN        ( BIT(2) )
#define BIT_LDO_USB33_SHPT_PD           ( BIT(1) )
#define BIT_LDO_USB33_PD                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_USB33_REG1 */
#define BITS_LDO_USB33_V(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_RESERVED_REG11 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG12 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG13 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG14 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG15 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG16 */

/* bits definitions for register ANA_REG_GLB_LDO_CAMD0_REG0 */
#define BIT_LDOCAMD0_LP_EN_SW           ( BIT(8) )
#define BITS_LDO_CAMD0_REFTRIM(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_CAMD0_EADBIAS_EN        ( BIT(2) )
#define BIT_LDO_CAMD0_SHPT_PD           ( BIT(1) )
#define BIT_LDO_CAMD0_PD                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMD0_REG1 */
#define BITS_LDO_CAMD0_V(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMD1_REG0 */
#define BIT_LDOCAMD1_LP_EN_SW           ( BIT(8) )
#define BITS_LDO_CAMD1_REFTRIM(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_CAMD1_EADBIAS_EN        ( BIT(2) )
#define BIT_LDO_CAMD1_SHPT_PD           ( BIT(1) )
#define BIT_LDO_CAMD1_PD                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMD1_REG1 */
#define BITS_LDO_CAMD1_V(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register ANA_REG_GLB_LDO_CON_REG0 */
#define BIT_LDOCON_LP_EN_SW             ( BIT(8) )
#define BITS_LDO_CON_REFTRIM(_X_)       ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_CON_EADBIAS_EN          ( BIT(2) )
#define BIT_LDO_CON_SHPT_PD             ( BIT(1) )
#define BIT_LDO_CON_PD                  ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_CON_REG1 */
#define BITS_LDO_CON_V(_X_)             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMIO_REG0 */
#define BIT_LDOCAMIO_LP_EN_SW           ( BIT(8) )
#define BITS_LDO_CAMIO_REFTRIM(_X_)     ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_CAMIO_EADBIAS_EN        ( BIT(2) )
#define BIT_LDO_CAMIO_SHPT_PD           ( BIT(1) )
#define BIT_LDO_CAMIO_PD                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_CAMIO_REG1 */
#define BITS_LDO_CAMIO_V(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register ANA_REG_GLB_LDO_AVDD18_REG0 */
#define BIT_LDOAVDD18_LP_EN_SW          ( BIT(8) )
#define BITS_LDO_AVDD18_REFTRIM(_X_)    ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_AVDD18_EADBIAS_EN       ( BIT(2) )
#define BIT_LDO_AVDD18_SHPT_PD          ( BIT(1) )

/* bits definitions for register ANA_REG_GLB_LDO_AVDD18_REG1 */
#define BITS_LDO_AVDD18_V(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register ANA_REG_GLB_LDO_VDDRF_REG0 */
#define BIT_LDORF_LP_EN_SW              ( BIT(8) )
#define BITS_LDO_RF_REFTRIM(_X_)        ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_RF_EADBIAS_EN           ( BIT(2) )
#define BIT_LDO_RF_SHPT_PD              ( BIT(1) )
#define BIT_LDO_RF_PD                   ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_VDDRF_REG1 */
#define BITS_LDO_RF_V(_X_)              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register ANA_REG_GLB_LDO_VDDSRAM_REG0 */
#define BIT_LDOSRAM_LP_EN_SW            ( BIT(8) )
#define BITS_LDO_SRAM_REFTRIM(_X_)      ( (_X_) << 3 & (BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_LDO_SRAM_EADBIAS_EN         ( BIT(2) )
#define BIT_LDO_SRAM_SHPT_PD            ( BIT(1) )
#define BIT_LDO_SRAM_PD                 ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_VDDSRAM_REG1 */
#define BITS_LDO_VDDSRAM_V(_X_)         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register ANA_REG_GLB_RESERVED_REG17 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG18 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG19 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG20 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG21 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG22 */

/* bits definitions for register ANA_REG_GLB_LDO_CH_CTRL */
#define BITS_VBATBK_V(_X_)              ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_LDOB_CAL_SEL(_X_)          ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BITS_LDOA_CAL_SEL                   (_X_)( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)) )
#define BITS_LDOD_CAL_SEL(_X_)          ( (_X_) << 3 & (BIT(3)|BIT(4)) )
#define BITS_LDODCDC_CAL_SEL(_X_)       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CLK_CTRL */
#define BIT_DCDC_3MCLKCAL_EN            ( BIT(13) )
#define BIT_DCDC_2MCLKCAL_EN            ( BIT(12) )
#define BITS_DCDC_6MFRECAL_SW(_X_)      ( (_X_) << 7 & (BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_DCDC_4MFRECAL_SW(_X_)      ( (_X_) << 2 & (BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )
#define BIT_DCDC_CLK_SP_SEL             ( BIT(1) )
#define BIT_DCDC_CLK_SP_EN              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_RESERVED_REG23 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG24 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG25 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG26 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG27 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG28 */

/* bits definitions for register ANA_REG_GLB_SLP_WAIT_DCDCARM */
#define BITS_SLP_IN_WAIT_DCDCARM(_X_)   ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_SLP_OUT_WAIT_DCDCARM(_X_)  ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_SLP_CTRL */
#define BIT_LDO_XTL_EN                  ( BIT(3) )
#define BIT_SLP_BG_LP_EN                ( BIT(2) )
#define BIT_SLP_IO_EN                   ( BIT(1) )
#define BIT_SLP_LDO_PD_EN               ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SLP_DCDC_PD_CTRL */
#define BIT_SLP_DCDCGPU_PD_EN           ( BIT(9) )
#define BIT_SLP_DCDCGPU_DROP_EN         ( BIT(8) )
#define BIT_SLP_DCDCCORE_DROP_EN        ( BIT(7) )
#define BIT_SLP_DCDCRF_PD_EN            ( BIT(6) )
#define BIT_SLP_DCDCWPA_PD_EN           ( BIT(3) )
#define BIT_SLP_DCDCARM0_PD_EN          ( BIT(2) )
#define BIT_SLP_DCDCARM1_PD_EN          ( BIT(1) )
#define BIT_SLP_DCDCMEM_PD_EN           ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SLP_LDO_PD_CTRL0 */
#define BIT_SLP_LDORF0_PD_EN            ( BIT(15) )
#define BIT_SLP_LDOEMMCCORE_PD_EN       ( BIT(14) )
#define BIT_SLP_LDODCXO_PD_EN           ( BIT(13) )
#define BIT_SLP_LDOWIFIPA_PD_EN         ( BIT(12) )
#define BIT_SLP_LDOVDD28_PD_EN          ( BIT(11) )
#define BIT_SLP_LDOSDCORE_PD_EN         ( BIT(10) )
#define BIT_SLP_LDOSDIO_PD_EN           ( BIT(9) )
#define BIT_SLP_LDOUSB33_PD_EN          ( BIT(8) )
#define BIT_SLP_LDOCAMMOT_PD_EN         ( BIT(7) )
#define BIT_SLP_LDOCAMIO_PD_EN          ( BIT(6) )
#define BIT_SLP_LDOCAMD0_PD_EN          ( BIT(5) )
#define BIT_SLP_LDOCAMD1_PD_EN          ( BIT(4) )
#define BIT_SLP_LDOCAMA0_PD_EN          ( BIT(3) )
#define BIT_SLP_LDOCAMA1_PD_EN          ( BIT(2) )
#define BIT_SLP_VLDO_PD_EN              ( BIT(1) )
#define BIT_SLP_LDOSIM1_PD_EN           ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SLP_LDO_PD_CTRL1 */
#define BIT_SLP_LDOCON_PD_EN            ( BIT(3) )
#define BIT_SLP_LDOSIM0_PD_EN           ( BIT(2) )
#define BIT_SLP_LDOAVDD18_PD_EN         ( BIT(1) )
#define BIT_SLP_LDOSRAM_PD_EN           ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SLP_DCDC_LP_CTRL */
#define BIT_SLP_DCDCRF_LP_EN            ( BIT(8) )
#define BIT_SLP_DCDCCORE_LP_EN          ( BIT(6) )
#define BIT_SLP_DCDCGPU_LP_EN           ( BIT(5) )
#define BIT_SLP_DCDCMEM_LP_EN           ( BIT(4) )
#define BIT_SLP_DCDCARM1_LP_EN          ( BIT(3) )
#define BIT_SLP_DCDCARM0_LP_EN          ( BIT(2) )
#define BIT_SLP_DCDCGEN_LP_EN           ( BIT(1) )
#define BIT_SLP_DCDCWPA_LP_EN           ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SLP_LDO_LP_CTRL0 */
#define BIT_SLP_LDORF0_LP_EN            ( BIT(15) )
#define BIT_SLP_LDOEMMCCORE_LP_EN       ( BIT(14) )
#define BIT_SLP_LDODCXO_LP_EN           ( BIT(13) )
#define BIT_SLP_LDOWIFIPA_LP_EN         ( BIT(12) )
#define BIT_SLP_LDOVDD28_LP_EN          ( BIT(11) )
#define BIT_SLP_LDOSDCORE_LP_EN         ( BIT(10) )
#define BIT_SLP_LDOSDIO_LP_EN           ( BIT(9) )
#define BIT_SLP_LDOUSB33_LP_EN          ( BIT(8) )
#define BIT_SLP_LDOCAMMOT_LP_EN         ( BIT(7) )
#define BIT_SLP_LDOCAMIO_LP_EN          ( BIT(6) )
#define BIT_SLP_LDOCAMD0_LP_EN          ( BIT(5) )
#define BIT_SLP_LDOCAMD1_LP_EN          ( BIT(4) )
#define BIT_SLP_LDOCAMA0_LP_EN          ( BIT(3) )
#define BIT_SLP_LDOCAMA1_LP_EN          ( BIT(2) )
#define BIT_SLP_VLDO_LP_EN              ( BIT(1) )
#define BIT_SLP_LDOSIM1_LP_EN           ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SLP_LDO_LP_CTRL1 */
#define BIT_SLP_LDOCON_LP_EN            ( BIT(3) )
#define BIT_SLP_LDOSIM0_LP_EN           ( BIT(2) )
#define BIT_SLP_LDOAVDD18_LP_EN         ( BIT(1) )
#define BIT_SLP_LDOSRAM_LP_EN           ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_SLP_CTRL0 */
#define BITS_SLP_DCDCCORE_VOL_DROP_CNT(_X_)( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_DCDC_CORE_SLP_OUT_STEP_EN   ( BIT(1) )
#define BIT_DCDC_CORE_SLP_IN_STEP_EN    ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_SLP_CTRL1 */
#define BITS_DCDC_CORE_CAL_DS_SW(_X_)   ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_CORE_CTRL_DS_SW(_X_)  ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_SLP_CTRL2 */
#define BITS_DCDC_CORE_CTRL_SLP_STEP3(_X_)( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_DCDC_CORE_CTRL_SLP_STEP2(_X_)( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_CORE_CTRL_SLP_STEP1(_X_)( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_SLP_CTRL3 */
#define BITS_DCDC_CORE_CTRL_SLP_STEP5(_X_)( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_CORE_CTRL_SLP_STEP4(_X_)( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_SLP_CTRL4 */
#define BITS_DCDC_CORE_CAL_SLP_STEP3(_X_)( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_DCDC_CORE_CAL_SLP_STEP2(_X_)( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_CORE_CAL_SLP_STEP1(_X_)( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_CORE_SLP_CTRL5 */
#define BITS_DCDC_CORE_CAL_SLP_STEP5(_X_)( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_CORE_CAL_SLP_STEP4(_X_)( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_SLP_CTRL0 */
#define BIT_DCDC_GPU_SLP_OUT_STEP_EN    ( BIT(1) )
#define BIT_DCDC_GPU_SLP_IN_STEP_EN     ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_SLP_CTRL1 */
#define BITS_DCDC_GPU_CAL_DS_SW(_X_)    ( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_GPU_CTRL_DS_SW(_X_)   ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_SLP_CTRL2 */
#define BITS_DCDC_GPU_CTRL_SLP_STEP3(_X_)( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_DCDC_GPU_CTRL_SLP_STEP2(_X_)( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_GPU_CTRL_SLP_STEP1(_X_)( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_SLP_CTRL3 */
#define BITS_DCDC_GPU_CTRL_SLP_STEP5(_X_)( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_GPU_CTRL_SLP_STEP4(_X_)( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_SLP_CTRL4 */
#define BITS_DCDC_GPU_CAL_SLP_STEP3(_X_)( (_X_) << 10 & (BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_DCDC_GPU_CAL_SLP_STEP2(_X_)( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_GPU_CAL_SLP_STEP1(_X_)( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_GPU_SLP_CTRL5 */
#define BITS_DCDC_GPU_CAL_SLP_STEP5(_X_)( (_X_) << 5 & (BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)) )
#define BITS_DCDC_GPU_CAL_SLP_STEP4(_X_)( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_DCDC_XTL_EN0 */
#define BIT_DCDC_CORE_EXT_XTL0_EN       ( BIT(15) )
#define BIT_DCDC_CORE_EXT_XTL1_EN       ( BIT(14) )
#define BIT_DCDC_CORE_EXT_XTL2_EN       ( BIT(13) )
#define BIT_DCDC_CORE_EXT_XTL3_EN       ( BIT(12) )
#define BIT_DCDC_GPU_EXT_XTL0_EN        ( BIT(3) )
#define BIT_DCDC_GPU_EXT_XTL1_EN        ( BIT(2) )
#define BIT_DCDC_GPU_EXT_XTL2_EN        ( BIT(1) )
#define BIT_DCDC_GPU_EXT_XTL3_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_XTL_EN1 */
#define BIT_DCDC_ARM0_EXT_XTL0_EN       ( BIT(15) )
#define BIT_DCDC_ARM0_EXT_XTL1_EN       ( BIT(14) )
#define BIT_DCDC_ARM0_EXT_XTL2_EN       ( BIT(13) )
#define BIT_DCDC_ARM0_EXT_XTL3_EN       ( BIT(12) )
#define BIT_DCDC_ARM1_EXT_XTL0_EN       ( BIT(3) )
#define BIT_DCDC_ARM1_EXT_XTL1_EN       ( BIT(2) )
#define BIT_DCDC_ARM1_EXT_XTL2_EN       ( BIT(1) )
#define BIT_DCDC_ARM1_EXT_XTL3_EN       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_XTL_EN2 */
#define BIT_DCDC_MEM_EXT_XTL0_EN        ( BIT(15) )
#define BIT_DCDC_MEM_EXT_XTL1_EN        ( BIT(14) )
#define BIT_DCDC_MEM_EXT_XTL2_EN        ( BIT(13) )
#define BIT_DCDC_MEM_EXT_XTL3_EN        ( BIT(12) )
#define BIT_DCDC_GEN_EXT_XTL0_EN        ( BIT(3) )
#define BIT_DCDC_GEN_EXT_XTL1_EN        ( BIT(2) )
#define BIT_DCDC_GEN_EXT_XTL2_EN        ( BIT(1) )
#define BIT_DCDC_GEN_EXT_XTL3_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_DCDC_XTL_EN3 */
#define BIT_DCDC_RF_EXT_XTL0_EN         ( BIT(15) )
#define BIT_DCDC_RF_EXT_XTL1_EN         ( BIT(14) )
#define BIT_DCDC_RF_EXT_XTL2_EN         ( BIT(13) )
#define BIT_DCDC_RF_EXT_XTL3_EN         ( BIT(12) )
#define BIT_DCDC_WPA_EXT_XTL0_EN        ( BIT(3) )
#define BIT_DCDC_WPA_EXT_XTL1_EN        ( BIT(2) )
#define BIT_DCDC_WPA_EXT_XTL2_EN        ( BIT(1) )
#define BIT_DCDC_WPA_EXT_XTL3_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_RESEVED_DCDC_XTL_EN4 */

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN0 */
#define BIT_LDO_DCXO_EXT_XTL0_EN        ( BIT(15) )
#define BIT_LDO_DCXO_EXT_XTL1_EN        ( BIT(14) )
#define BIT_LDO_DCXO_EXT_XTL2_EN        ( BIT(13) )
#define BIT_LDO_DCXO_EXT_XTL3_EN        ( BIT(12) )
#define BIT_LDO_VDD28_EXT_XTL0_EN       ( BIT(3) )
#define BIT_LDO_VDD28_EXT_XTL1_EN       ( BIT(2) )
#define BIT_LDO_VDD28_EXT_XTL2_EN       ( BIT(1) )
#define BIT_LDO_VDD28_EXT_XTL3_EN       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN1 */
#define BIT_LDO_RF0_EXT_XTL0_EN         ( BIT(15) )
#define BIT_LDO_RF0_EXT_XTL1_EN         ( BIT(14) )
#define BIT_LDO_RF0_EXT_XTL2_EN         ( BIT(13) )
#define BIT_LDO_RF0_EXT_XTL3_EN         ( BIT(12) )
#define BIT_LDO_WIFIPA_EXT_XTL0_EN      ( BIT(3) )
#define BIT_LDO_WIFIPA_EXT_XTL1_EN      ( BIT(2) )
#define BIT_LDO_WIFIPA_EXT_XTL2_EN      ( BIT(1) )
#define BIT_LDO_WIFIPA_EXT_XTL3_EN      ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN2 */
#define BIT_LDO_SIM0_EXT_XTL0_EN        ( BIT(15) )
#define BIT_LDO_SIM0_EXT_XTL1_EN        ( BIT(14) )
#define BIT_LDO_SIM0_EXT_XTL2_EN        ( BIT(13) )
#define BIT_LDO_SIM0_EXT_XTL3_EN        ( BIT(12) )
#define BIT_LDO_SIM1_EXT_XTL0_EN        ( BIT(3) )
#define BIT_LDO_SIM1_EXT_XTL1_EN        ( BIT(2) )
#define BIT_LDO_SIM1_EXT_XTL2_EN        ( BIT(1) )
#define BIT_LDO_SIM1_EXT_XTL3_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN3 */
#define BIT_LDO_VLDO_EXT_XTL0_EN        ( BIT(15) )
#define BIT_LDO_VLDO_EXT_XTL1_EN        ( BIT(14) )
#define BIT_LDO_VLDO_EXT_XTL2_EN        ( BIT(13) )
#define BIT_LDO_VLDO_EXT_XTL3_EN        ( BIT(12) )
#define BIT_LDO_SRAM_EXT_XTL0_EN        ( BIT(3) )
#define BIT_LDO_SRAM_EXT_XTL1_EN        ( BIT(2) )
#define BIT_LDO_SRAM_EXT_XTL2_EN        ( BIT(1) )
#define BIT_LDO_SRAM_EXT_XTL3_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN4 */
#define BIT_LDO_CAMMOT_EXT_XTL0_EN      ( BIT(15) )
#define BIT_LDO_CAMMOT_EXT_XTL1_EN      ( BIT(14) )
#define BIT_LDO_CAMMOT_EXT_XTL2_EN      ( BIT(13) )
#define BIT_LDO_CAMMOT_EXT_XTL3_EN      ( BIT(12) )
#define BIT_LDO_CAMIO_EXT_XTL0_EN       ( BIT(3) )
#define BIT_LDO_CAMIO_EXT_XTL1_EN       ( BIT(2) )
#define BIT_LDO_CAMIO_EXT_XTL2_EN       ( BIT(1) )
#define BIT_LDO_CAMIO_EXT_XTL3_EN       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN5 */
#define BIT_LDO_CAMA0_EXT_XTL0_EN       ( BIT(15) )
#define BIT_LDO_CAMA0_EXT_XTL1_EN       ( BIT(14) )
#define BIT_LDO_CAMA0_EXT_XTL2_EN       ( BIT(13) )
#define BIT_LDO_CAMA0_EXT_XTL3_EN       ( BIT(12) )
#define BIT_LDO_CAMA1_EXT_XTL0_EN       ( BIT(3) )
#define BIT_LDO_CAMA1_EXT_XTL1_EN       ( BIT(2) )
#define BIT_LDO_CAMA1_EXT_XTL2_EN       ( BIT(1) )
#define BIT_LDO_CAMA1_EXT_XTL3_EN       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN6 */
#define BIT_LDO_CAMD0_EXT_XTL0_EN       ( BIT(15) )
#define BIT_LDO_CAMD0_EXT_XTL1_EN       ( BIT(14) )
#define BIT_LDO_CAMD0_EXT_XTL2_EN       ( BIT(13) )
#define BIT_LDO_CAMD0_EXT_XTL3_EN       ( BIT(12) )
#define BIT_LDO_CAMD1_EXT_XTL0_EN       ( BIT(3) )
#define BIT_LDO_CAMD1_EXT_XTL1_EN       ( BIT(2) )
#define BIT_LDO_CAMD1_EXT_XTL2_EN       ( BIT(1) )
#define BIT_LDO_CAMD1_EXT_XTL3_EN       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN7 */
#define BIT_LDO_SDIO_EXT_XTL0_EN        ( BIT(15) )
#define BIT_LDO_SDIO_EXT_XTL1_EN        ( BIT(14) )
#define BIT_LDO_SDIO_EXT_XTL2_EN        ( BIT(13) )
#define BIT_LDO_SDIO_EXT_XTL3_EN        ( BIT(12) )
#define BIT_LDO_SDCORE_EXT_XTL0_EN      ( BIT(3) )
#define BIT_LDO_SDCORE_EXT_XTL1_EN      ( BIT(2) )
#define BIT_LDO_SDCORE_EXT_XTL2_EN      ( BIT(1) )
#define BIT_LDO_SDCORE_EXT_XTL3_EN      ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN8 */
#define BIT_LDO_EMMCCORE_EXT_XTL0_EN    ( BIT(15) )
#define BIT_LDO_EMMCCORE_EXT_XTL1_EN    ( BIT(14) )
#define BIT_LDO_EMMCCORE_EXT_XTL2_EN    ( BIT(13) )
#define BIT_LDO_EMMCCORE_EXT_XTL3_EN    ( BIT(12) )
#define BIT_LDO_USB33_EXT_XTL0_EN       ( BIT(3) )
#define BIT_LDO_USB33_EXT_XTL1_EN       ( BIT(2) )
#define BIT_LDO_USB33_EXT_XTL2_EN       ( BIT(1) )
#define BIT_LDO_USB33_EXT_XTL3_EN       ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN9 */
#define BIT_LDO_KPLED_EXT_XTL0_EN       ( BIT(15) )
#define BIT_LDO_KPLED_EXT_XTL1_EN       ( BIT(14) )
#define BIT_LDO_KPLED_EXT_XTL2_EN       ( BIT(13) )
#define BIT_LDO_KPLED_EXT_XTL3_EN       ( BIT(12) )
#define BIT_LDO_VIBR_EXT_XTL0_EN        ( BIT(3) )
#define BIT_LDO_VIBR_EXT_XTL1_EN        ( BIT(2) )
#define BIT_LDO_VIBR_EXT_XTL2_EN        ( BIT(1) )
#define BIT_LDO_VIBR_EXT_XTL3_EN        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_XTL_EN10 */
#define BIT_LDO_CON_EXT_XTL0_EN         ( BIT(15) )
#define BIT_LDO_CON_EXT_XTL1_EN         ( BIT(14) )
#define BIT_LDO_CON_EXT_XTL2_EN         ( BIT(13) )
#define BIT_LDO_CON_EXT_XTL3_EN         ( BIT(12) )
#define BIT_LDO_AVDD18_EXT_XTL0_EN      ( BIT(3) )
#define BIT_LDO_AVDD18_EXT_XTL1_EN      ( BIT(2) )
#define BIT_LDO_AVDD18_EXT_XTL2_EN      ( BIT(1) )
#define BIT_LDO_AVDD18_EXT_XTL3_EN      ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_XO_BG_XTL_EN */
#define BIT_XO_EXT_XTL0_EN              ( BIT(15) )
#define BIT_XO_EXT_XTL1_EN              ( BIT(14) )
#define BIT_XO_EXT_XTL2_EN              ( BIT(13) )
#define BIT_XO_EXT_XTL3_EN              ( BIT(12) )
#define BIT_BG_EXT_XTL0_EN              ( BIT(3) )
#define BIT_BG_EXT_XTL1_EN              ( BIT(2) )
#define BIT_BG_EXT_XTL2_EN              ( BIT(1) )
#define BIT_BG_EXT_XTL3_EN              ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_RESERVED_REG29 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG30 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG31 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG32 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG33 */

/* bits definitions for register ANA_REG_GLB_RESERVED_REG34 */

/* bits definitions for register ANA_REG_GLB_DCDC_VLG_SEL */
#define BIT_DCDC_VREF_SW_SEL            ( BIT(10) )
#define BIT_DCDC_WPA_SW_SEL             ( BIT(9) )
#define BIT_DCDC_RF_SW_SEL              ( BIT(8) )
#define BIT_DCDC_GEN_SW_SEL             ( BIT(7) )
#define BIT_DCDC_MEM_SW_SEL             ( BIT(6) )
#define BIT_DCDC_ARM1_SW_SEL            ( BIT(5) )
#define BIT_DCDC_ARM0_SW_SEL            ( BIT(4) )
#define BIT_DCDC_GPU_SLP_SW_SEL         ( BIT(3) )
#define BIT_DCDC_GPU_NOR_SW_SEL         ( BIT(2) )
#define BIT_DCDC_CORE_SLP_SW_SEL        ( BIT(1) )
#define BIT_DCDC_CORE_NOR_SW_SEL        ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_VLG_SEL0 */
#define BIT_LDO_SIM0_SW_SEL             ( BIT(15) )
#define BIT_LDO_SIM1_SW_SEL             ( BIT(14) )
#define BIT_LDO_RF0_SW_SEL              ( BIT(13) )
#define BIT_LDO_USB33_SW_SEL            ( BIT(12) )
#define BIT_LDO_EMMCCORE_SW_SEL         ( BIT(11) )
#define BIT_LDO_AVDD18_SW_SEL           ( BIT(10) )
#define BIT_LDO_VDD28_SW_SEL            ( BIT(9) )
#define BIT_LDO_DCXO_SW_SEL             ( BIT(8) )
#define BIT_LDO_SDCORE_SW_SEL           ( BIT(7) )
#define BIT_LDO_SDIO_SW_SEL             ( BIT(6) )
#define BIT_LDO_CAMMOT_SW_SEL           ( BIT(5) )
#define BIT_LDO_CAMIO_SW_SEL            ( BIT(4) )
#define BIT_LDO_CAMA0_SW_SEL            ( BIT(3) )
#define BIT_LDO_CAMA1_SW_SEL            ( BIT(2) )
#define BIT_LDO_CAMD0_SW_SEL            ( BIT(1) )
#define BIT_LDO_CAMD1_SW_SEL            ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_LDO_VLG_SEL1 */
#define BIT_LDO_KPLED_SW_SEL            ( BIT(5) )
#define BIT_LDO_VIBR_SW_SEL             ( BIT(4) )
#define BIT_LDO_CON_SW_SEL              ( BIT(3) )
#define BIT_LDO_WIFIPA_SW_SEL           ( BIT(2) )
#define BIT_LDO_SRAM_SW_SEL             ( BIT(1) )
#define BIT_LDO_VLDO_SW_SEL             ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_ANA_32KLESS_CTRL0 */
#define BIT_RC_MODE_WR_ACK_FLAG         ( BIT(14) )
#define BIT_XO_LOW_CUR_FLAG             ( BIT(13) )
#define BIT_XO_LOW_CUR_FRC_RTCSET       ( BIT(12) )
#define BIT_XO_LOW_CUR_FRC_RTCCLR       ( BIT(11) )
#define BIT_RC_MODE_WR_ACK_FLAG_CLR     ( BIT(10) )
#define BIT_XO_LOW_CUR_FLAG_CLR         ( BIT(9) )
#define BIT_XO_LOW_CUR_CNT_CLR          ( BIT(8) )
#define BIT_LDO_DCXO_LP_EN_RTCSET       ( BIT(7) )
#define BIT_LDO_DCXO_LP_EN_RTCCLR       ( BIT(6) )
#define BIT_SLP_XO_LOW_CUR_EN           ( BIT(5) )
#define BIT_XO_LOW_CUR_EN               ( BIT(3) )
#define BIT_EXT_32K_PD                  ( BIT(2) )
#define BIT_RC_32K_SEL                  ( BIT(1) )
#define BIT_RC_32K_EN                   ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_ANA_32KLESS_CTRL1 */
#define BITS_RC_MODE(_X_)               ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_ANA_32KLESS_CTRL2 */
#define BITS_XO_LOW_CUR_CNT_LOW(_X_)    ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_ANA_32KLESS_CTRL3 */
#define BITS_XO_LOW_CUR_CNT_HIGH(_X_)   ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_AUXAD_CTL */
#define BITS_AUXAD_RESERVED(_X_)        ( (_X_) << 11 & (BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BIT_AUXAD_NTCREF_SEL            ( BIT(10) )
#define BIT_AUXAD_NTCAMP_SWP            ( BIT(9) )
#define BIT_AUXAD_NTCAMP_EN             ( BIT(8) )
#define BIT_AUXAD_TEST_EN               ( BIT(7) )
#define BIT_AUXAD_CURRENTSEN_EN         ( BIT(6) )
#define BIT_AUXAD_CURRENTSEL            ( BIT(5) )
#define BITS_AUXAD_CURRENT_IBS(_X_)     ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_XTL_WAIT_CTRL */
#define BIT_RSV                         ( BIT(15) )
#define BIT_CUR_SEL                     ( BIT(14) )
#define BIT_EXT_XTL3_FOR_26M_EN         ( BIT(13) )
#define BIT_EXT_XTL2_FOR_26M_EN         ( BIT(12) )
#define BIT_EXT_XTL1_FOR_26M_EN         ( BIT(11) )
#define BIT_EXT_XTL0_FOR_26M_EN         ( BIT(10) )
#define BIT_SLP_XTLBUF_PD_EN            ( BIT(9) )
#define BIT_XTL_EN                      ( BIT(8) )
#define BITS_XTL_WAIT(_X_)              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_RGB_CTRL */
#define BIT_RTC_PWM0_EN                 ( BIT(15) )
#define BIT_PWM0_EN                     ( BIT(14) )
#define BIT_IB_REX_EN                   ( BIT(12) )
#define BIT_IB_TRIM_EM_SEL              ( BIT(11) )
#define BITS_RGB_V(_X_)                 ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)) )
#define BIT_SLP_RGB_PD_EN               ( BIT(2) )
#define BIT_RGB_PD_HW_EN                ( BIT(1) )
#define BIT_RGB_PD_SW                   ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_WHTLED_CTRL */
#define BITS_IB_TRIM(_X_)               ( (_X_) << 9 & (BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_WHTLED_SERIES_EN            ( BIT(8) )
#define BIT_WHTLED_PD_SEL               ( BIT(7) )
#define BITS_WHTLED_V(_X_)              ( (_X_) << 1 & (BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )
#define BIT_WHTLED_PD                   ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_KPLED_CTRL */
#define BITS_KPLED_V(_X_)               ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BIT_KPLED_PD                    ( BIT(11) )
#define BIT_KPLED_PULLDOWN_EN           ( BIT(10) )
#define BIT_SLP_LDOKPLED_PD_EN          ( BIT(9) )
#define BIT_LDO_KPLED_PD                ( BIT(8) )
#define BITS_LDO_KPLED_V(_X_)           ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_VIBR_CTRL0 */
#define BIT_LDO_KPLED_SHPT_PD           ( BIT(15) )
#define BITS_CUR_DRV_CAL_SEL(_X_)       ( (_X_) << 12 & (BIT(12)|BIT(13)) )
#define BIT_VIBR_PULLDOWN_EN            ( BIT(11) )
#define BIT_VIBR_PULLUP_EN              ( BIT(10) )
#define BIT_SLP_LDOVIBR_PD_EN           ( BIT(9) )
#define BIT_LDO_VIBR_PD                 ( BIT(8) )
#define BITS_LDO_VIBR_V(_X_)            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_VIBR_CTRL1 */
#define BITS_LDO_KPLED_REFTRIM(_X_)     ( (_X_) << 7 & (BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BIT_LDO_KPLED_EADBIAS_EN        ( BIT(6) )
#define BITS_LDO_VIBR_REFTRIM(_X_)      ( (_X_) << 1 & (BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)) )
#define BIT_LDO_VIBR_EADBIAS_EN         ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_AUDIO_CTRL0 */
#define BIT_AUD_SLP_APP_RST_EN          ( BIT(15) )
#define BITS_CLK_AUD_HBD_DIV(_X_)       ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)) )
#define BIT_CLK_AUD_LOOP_INV_EN         ( BIT(4) )
#define BIT_CLK_AUD_IF_TX_INV_EN        ( BIT(3) )
#define BIT_CLK_AUD_IF_RX_INV_EN        ( BIT(2) )
#define BIT_CLK_AUD_IF_6P5M_TX_INV_EN   ( BIT(1) )
#define BIT_CLK_AUD_IF_6P5M_RX_INV_EN   ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_AUDIO_CTRL1 */
#define BIT_HEAD_INSERT_EIC_EN          ( BIT(6) )
#define BIT_AUDIO_CHP_CLK_DIV_EN        ( BIT(5) )
#define BITS_AUDIO_CHP_CLK_DIV(_X_)     ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)) )

/* bits definitions for register ANA_REG_GLB_CHGR_DET_FGU_CTRL */
#define BIT_FGUA_SOFT_RST               ( BIT(13) )
#define BIT_LDO_FGU_PD                  ( BIT(12) )
#define BITS_CHG_INT_DELAY(_X_)         ( (_X_) << 9 & (BIT(9)|BIT(10)|BIT(11)) )
#define BIT_SD_CHOP_CAP_EN              ( BIT(8) )
#define BITS_SD_CLK_P(_X_)              ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BIT_SD_DCOFFSET_EN              ( BIT(5) )
#define BIT_SD_CHOP_EN                  ( BIT(4) )
#define BIT_DP_DM_FC_ENB                ( BIT(2) )
#define BIT_DP_DM_AUX_EN                ( BIT(1) )
#define BIT_DP_DM_BC_ENB                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_CHGR_STATUS */
#define BIT_CHGR_INT_EN                 ( BIT(13) )
#define BIT_NON_DCP_INT                 ( BIT(12) )
#define BIT_CHG_DET_DONE                ( BIT(11) )
#define BIT_DP_LOW                      ( BIT(10) )
#define BIT_DCP_DET                     ( BIT(9) )
#define BIT_CHG_DET                     ( BIT(8) )
#define BIT_SDP_INT                     ( BIT(7) )
#define BIT_DCP_INT                     ( BIT(6) )
#define BIT_CDP_INT                     ( BIT(5) )
#define BIT_CHGR_CV_STATUS              ( BIT(4) )
#define BIT_CHGR_ON                     ( BIT(3) )
#define BIT_CHGR_INT                    ( BIT(2) )
#define BIT_VBAT_OVI                    ( BIT(1) )
#define BIT_VCHG_OVI                    ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_MIXED_CTRL0 */
#define BIT_PTEST_PD_RTCSET             ( BIT(15) )
#define BIT_DCDC_V_CTRL_MODE            ( BIT(14) )
#define BIT_LDO_RAMP_EN                 ( BIT(13) )
#define BIT_BG_LP_EN                    ( BIT(12) )
#define BITS_VBAT_CRASH_V(_X_)          ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_OVLO_EN                     ( BIT(9) )
#define BITS_OVLO_CAL(_X_)              ( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)) )
#define BITS_OVLO_V(_X_)                ( (_X_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_OVLO_T(_X_)                ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_MIXED_CTRL1 */
#define BITS_XOSC32K_CTL(_X_)           ( (_X_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_BATON_T(_X_)               ( (_X_) << 10 & (BIT(10)|BIT(11)) )
#define BIT_BATDET_LDO_SEL              ( BIT(9) )
#define BIT_THM_CHIP_PD_FLAG            ( BIT(8) )
#define BIT_THM_CHIP_PD_FLAG_CLR        ( BIT(7) )
#define BITS_THM_CAL_SEL(_X_)           ( (_X_) << 5 & (BIT(5)|BIT(6)) )
#define BIT_THM_AUTO_PD_EN              ( BIT(4) )
#define BIT_ALL_GPI_DEB                 ( BIT(3) )
#define BIT_GPI_DEBUG_EN                ( BIT(2) )
#define BIT_ALL_INT_DEB                 ( BIT(1) )
#define BIT_INT_DEBUG_EN                ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_POR_RST_MONITOR */
#define BITS_POR_RST_MONITOR(_X_)       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_WDG_RST_MONITOR */
#define BITS_WDG_RST_MONITOR(_X_)       ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_POR_PIN_RST_MONITOR */
#define BITS_POR_PIN_RST_MONITOR(_X_)   ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_POR_SRC_FLAG */
#define BIT_POR_SW_FORCE_ON             ( BIT(15) )
#define BITS_POR_SRC_FLAG(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )

/* bits definitions for register ANA_REG_GLB_POR_7S_CTRL */
#define BIT_PBINT_7S_FLAG_CLR           ( BIT(15) )
#define BIT_EXT_RSTN_FLAG_CLR           ( BIT(14) )
#define BIT_CHGR_INT_FLAG_CLR           ( BIT(13) )
#define BIT_PBINT2_FLAG_CLR             ( BIT(12) )
#define BIT_PBINT_FLAG_CLR              ( BIT(11) )
#define BIT_PBINT_7S_RST_SWMODE         ( BIT(8) )
#define BITS_PBINT_7S_RST_THRESHOLD(_X_)( (_X_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BIT_EXT_RSTN_MODE               ( BIT(3) )
#define BIT_PBINT_7S_AUTO_ON_EN         ( BIT(2) )
#define BIT_PBINT_7S_RST_DISABLE        ( BIT(1) )
#define BIT_PBINT_7S_RST_MODE           ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_HWRST_RTC */
#define BITS_HWRST_RTC_REG_STS(_X_)     ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_HWRST_RTC_REG_SET(_X_)     ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)) )

/* bits definitions for register ANA_REG_GLB_ARM_MF_REG */
#define BITS_ARM_MF_REG(_X_)            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_ARCH_EN */
#define BIT_ARCH_EN                     ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_MCU_WR_PROT_VALUE */
#define BIT_MCU_WR_PROT                 ( BIT(15) )
#define BITS_MCU_WR_PROT_VALUE(_X_)     ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )

/* bits definitions for register ANA_REG_GLB_PWR_WR_PROT_VALUE */
#define BIT_PWR_WR_PROT                 ( BIT(15) )
#define BITS_PWR_WR_PROT_VALUE(_X_)     ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )

/* bits definitions for register ANA_REG_GLB_SMPL_CTRL0 */
#define BITS_SMPL_MODE(_X_)             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_SMPL_CTRL1 */
#define BIT_SMPL_PWR_ON_FLAG            ( BIT(15) )
#define BIT_SMPL_MODE_WR_ACK_FLAG       ( BIT(14) )
#define BIT_SMPL_PWR_ON_FLAG_CLR        ( BIT(13) )
#define BIT_SMPL_MODE_WR_ACK_FLAG_CLR   ( BIT(12) )
#define BIT_SMPL_PWR_ON_SET             ( BIT(11) )
#define BIT_SMPL_EN                     ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_RTC_RST0 */
#define BITS_RTC_CLK_FLAG_SET(_X_)      ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_RTC_RST1 */
#define BITS_RTC_CLK_FLAG_CLR(_X_)      ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_RTC_RST2 */
#define BITS_RTC_CLK_FLAG_RTC(_X_)      ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_BATDET_CUR_CTRL */
#define BIT_BAT_TYPE                    ( BIT(5) )
#define BIT_BATDET_CUR_EN               ( BIT(4) )
#define BITS_BATDET_CUR_I(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register ANA_REG_GLB_RTC_CLK_STOP */
#define BIT_RTC_CLK_STOP_FLAG           ( BIT(7) )
#define BITS_RTC_CLK_STOP_THRESHOLD(_X_)( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register ANA_REG_GLB_VBAT_DROP_CNT */
#define BITS_VBAT_DROP_CNT(_X_)         ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)) )

/* bits definitions for register ANA_REG_GLB_SWRST_CTRL0 */
#define BIT_POR_RTC_PD                  ( BIT(15) )
#define BITS_POR_RTC_I(_X_)             ( (_X_) << 13 & (BIT(13)|BIT(14)) )
#define BIT_EXT_RSTN_PD_EN              ( BIT(10) )
#define BIT_PB_7S_RST_PD_EN             ( BIT(9) )
#define BIT_KEY2_7S_RST_EN              ( BIT(7) )
#define BIT_WDG_RST_PD_EN               ( BIT(6) )
#define BITS_SW_RST_PD_THRESHOLD(_X_)   ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)) )

/* bits definitions for register ANA_REG_GLB_SWRST_CTRL1 */
#define BIT_SW_RST_USB33_PD_EN          ( BIT(3) )
#define BIT_SW_RST_EMMCCORE_PD_EN       ( BIT(2) )
#define BIT_SW_RST_SDIO_PD_EN           ( BIT(1) )
#define BIT_SW_RST_SDCORE_PD_EN         ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_OSC_CTRL */
#define BITS_OSC_32K_CMP_ICTRL(_X_)     ( (_X_) & (BIT(0)|BIT(1)) )

/* bits definitions for register ANA_REG_GLB_OTP_CTRL */
#define BITS_OTP_AUD_OP(_X_)            ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_OTP_AUD_EN                  ( BIT(3) )
#define BITS_OTP_OP(_X_)                ( (_X_) << 1 & (BIT(1)|BIT(2)) )
#define BIT_OTP_EN                      ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SWLPRO_CTRL_0 */
#define BIT_SWLPRO_OTP_EN               ( BIT(15) )
#define BIT_LDO_SUP_PD                  ( BIT(14) )
#define BITS_SWLPRO_VTRIM(_X_)          ( (_X_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)) )
#define BITS_SWLPRO_V(_X_)              ( (_X_) << 6 & (BIT(6)|BIT(7)) )
#define BITS_SWLPRO_STOP_T(_X_)         ( (_X_) << 4 & (BIT(4)|BIT(5)) )
#define BIT_SWLPRO_FLG                  ( BIT(3) )
#define BIT_SWLPRO_AUTODIS              ( BIT(2) )
#define BIT_SWLPRO_DIS                  ( BIT(1) )
#define BIT_SWLPRO_EN                   ( BIT(0) )

/* bits definitions for register ANA_REG_GLB_SWLPRO_CTRL_1 */
#define BIT_SWLPRO_VTRIM_SW_SEL         ( BIT(15) )
#define BITS_SWLPRO_NTCTRIM(_X_)        ( (_X_) << 9 & (BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)) )
#define BITS_SWLPRO_START_T(_X_)        ( (_X_) << 7 & (BIT(7)|BIT(8)) )
#define BITS_SWLPRO_ITRIM(_X_)          ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* bits definitions for register ANA_REG_GLB_FREE_TIMER_LOW */
#define BITS_TIMER_LOW(_X_)             ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_FREE_TIMER_HIGH */
#define BITS_TIMER_HIGH(_X_)            ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register ANA_REG_GLB_RC_CTRL_0 */
#define BIT_RTC_RSV                     ( BIT(7) )
#define BITS_RC1M_CAL(_X_)              ( (_X_) & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)) )

/* vars definitions for controller ANA_REGS_GLB */

#endif /* __ANA_REGS_GLB_H__ */
