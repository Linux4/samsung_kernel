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

#ifndef __REGS_AHB_H__
#define __REGS_AHB_H__

#define REGS_AHB

/* registers definitions for controller REGS_AHB */
#define REG_AHB_AHB_CTL0                SCI_ADDR(REGS_AHB_BASE, 0x0000)
#define REG_AHB_AHB_CTL1                SCI_ADDR(REGS_AHB_BASE, 0x0004)
#define REG_AHB_AHB_CTL2                SCI_ADDR(REGS_AHB_BASE, 0x0008)
#define REG_AHB_AHB_CTL3                SCI_ADDR(REGS_AHB_BASE, 0x000c)
#define REG_AHB_SOFT_RST                SCI_ADDR(REGS_AHB_BASE, 0x0010)
#define REG_AHB_AHB_PAUSE               SCI_ADDR(REGS_AHB_BASE, 0x0014)
#define REG_AHB_REMAP                   SCI_ADDR(REGS_AHB_BASE, 0x0018)
#define REG_AHB_MIPI_PHY_CTRL           SCI_ADDR(REGS_AHB_BASE, 0x001c)
#define REG_AHB_DISPC_CTRL              SCI_ADDR(REGS_AHB_BASE, 0x0020)
#define REG_AHB_ARM_CLK                 SCI_ADDR(REGS_AHB_BASE, 0x0024)
#define REG_AHB_AHB_SDIO_CTRL           SCI_ADDR(REGS_AHB_BASE, 0x0028)
#define REG_AHB_AHB_CTL4                SCI_ADDR(REGS_AHB_BASE, 0x002c)
#define REG_AHB_AHB_CTL5                SCI_ADDR(REGS_AHB_BASE, 0x0030)
#define REG_AHB_AHB_STATUS              SCI_ADDR(REGS_AHB_BASE, 0x0034)
#define REG_AHB_CA5_CFG                 SCI_ADDR(REGS_AHB_BASE, 0x0038)
#define REG_AHB_ISP_CTRL                SCI_ADDR(REGS_AHB_BASE, 0x003c)
#define REG_AHB_HOLDING_PEN             SCI_ADDR(REGS_AHB_BASE, 0x0040)
#define REG_AHB_JMP_ADDR_CPU0           SCI_ADDR(REGS_AHB_BASE, 0x0044)
#define REG_AHB_JMP_ADDR_CPU1           SCI_ADDR(REGS_AHB_BASE, 0x0048)
#define REG_AHB_CP_AHB_ARM_CLK          SCI_ADDR(REGS_AHB_BASE, 0x004c)
#define REG_AHB_CP_AHB_CTL              SCI_ADDR(REGS_AHB_BASE, 0x0050)
#define REG_AHB_CP_RST                  SCI_ADDR(REGS_AHB_BASE, 0x0054)
#define REG_AHB_CP_SLEEP_CTRL           SCI_ADDR(REGS_AHB_BASE, 0x0058)
#define REG_AHB_DEEPSLEEP_STATUS        SCI_ADDR(REGS_AHB_BASE, 0x005c)
#define REG_AHB_DDR_PHY_Z_VALUE         SCI_ADDR(REGS_AHB_BASE, 0x0060)
#define REG_AHB_DSP_JTAG_CTRL           SCI_ADDR(REGS_AHB_BASE, 0x0080)
#define REG_AHB_DSP_BOOT_EN             SCI_ADDR(REGS_AHB_BASE, 0x0084)
#define REG_AHB_DSP_BOOT_VEC            SCI_ADDR(REGS_AHB_BASE, 0x0088)
#define REG_AHB_DSP_RST                 SCI_ADDR(REGS_AHB_BASE, 0x008c)
#define REG_AHB_BIGEND_PORT             SCI_ADDR(REGS_AHB_BASE, 0x0090)
#define REG_AHB_USB_PHY_TUNE            SCI_ADDR(REGS_AHB_BASE, 0x00a0)
#define REG_AHB_USB_PHY_TEST            SCI_ADDR(REGS_AHB_BASE, 0x00a4)
#define REG_AHB_USB_PHY_CTRL            SCI_ADDR(REGS_AHB_BASE, 0x00a8)
#define REG_AHB_AHB_SPR_REG             SCI_ADDR(REGS_AHB_BASE, 0x00c0)
#define REG_AHB_MTX_CTRL                SCI_ADDR(REGS_AHB_BASE, 0x0100)
#define REG_AHB_EMC_CTRL_CFG0           SCI_ADDR(REGS_AHB_BASE, 0x0104)
#define REG_AHB_EMC_CTRL_CFG1           SCI_ADDR(REGS_AHB_BASE, 0x0108)
#define REG_AHB_EMC_CLIENT_CTRL         SCI_ADDR(REGS_AHB_BASE, 0x010c)
#define REG_AHB_EMC_PORT0_REMAP         SCI_ADDR(REGS_AHB_BASE, 0x0110)
#define REG_AHB_EMC_PORT1_REMAP         SCI_ADDR(REGS_AHB_BASE, 0x0114)
#define REG_AHB_EMC_PORT2_REMAP         SCI_ADDR(REGS_AHB_BASE, 0x0118)
#define REG_AHB_EMC_PORT3_REMAP         SCI_ADDR(REGS_AHB_BASE, 0x011c)
#define REG_AHB_EMC_PORT4_REMAP         SCI_ADDR(REGS_AHB_BASE, 0x0120)
#define REG_AHB_EMC_PORT5_REMAP         SCI_ADDR(REGS_AHB_BASE, 0x0124)
#define REG_AHB_EMC_PORT6_REMAP         SCI_ADDR(REGS_AHB_BASE, 0x0128)
#define REG_AHB_EMC_PORT7_REMAP         SCI_ADDR(REGS_AHB_BASE, 0x012c)
#define REG_AHB_MSTX_SIM0_CTL00         SCI_ADDR(REGS_AHB_BASE, 0x0130)
#define REG_AHB_MSTX_SIM0_CTL01         SCI_ADDR(REGS_AHB_BASE, 0x0134)
#define REG_AHB_MSTX_SIM1_CTL00         SCI_ADDR(REGS_AHB_BASE, 0x0140)
#define REG_AHB_MSTX_SIM1_CTL01         SCI_ADDR(REGS_AHB_BASE, 0x0144)
#define REG_AHB_MSTX_SIM2_CTL00         SCI_ADDR(REGS_AHB_BASE, 0x0150)
#define REG_AHB_MSTX_SIM2_CTL01         SCI_ADDR(REGS_AHB_BASE, 0x0154)
#define REG_AHB_MSTX_SIM3_CTL00         SCI_ADDR(REGS_AHB_BASE, 0x0160)
#define REG_AHB_MSTX_SIM3_CTL01         SCI_ADDR(REGS_AHB_BASE, 0x0164)
#define REG_AHB_MSTX_SIM4_CTL00         SCI_ADDR(REGS_AHB_BASE, 0x0170)
#define REG_AHB_MSTX_SIM4_CTL01         SCI_ADDR(REGS_AHB_BASE, 0x0174)
#define REG_AHB_MSTX_SIM4_CTL10         SCI_ADDR(REGS_AHB_BASE, 0x0178)
#define REG_AHB_MSTX_SIM4_CTL11         SCI_ADDR(REGS_AHB_BASE, 0x017c)
#define REG_AHB_DSPX_SIM0_CTL00         SCI_ADDR(REGS_AHB_BASE, 0x0180)
#define REG_AHB_DSPX_SIM0_CTL01         SCI_ADDR(REGS_AHB_BASE, 0x0184)
#define REG_AHB_CHIP_ID                 SCI_ADDR(REGS_AHB_BASE, 0x01fc)

/* bits definitions for register REG_AHB_AHB_CTL0 */
#define BIT_AXIBUSMON2_EB               ( BIT(31) )
#define BIT_AXIBUSMON1_EB               ( BIT(30) )
#define BIT_AXIBUSMON0_EB               ( BIT(29) )
#define BIT_EMC_EB                      ( BIT(28) )
#define BIT_AHB_ARCH_EB                 ( BIT(27) )
#define BIT_SPINLOCK_EB                 ( BIT(25) )
#define BIT_SDIO2_EB                    ( BIT(24) )
#define BIT_EMMC_EB                     ( BIT(23) )
#define BIT_DISPC_EB                    ( BIT(22) )
#define BIT_G3D_EB                      ( BIT(21) )
#define BIT_SDIO1_EB                    ( BIT(19) )
#define BIT_DRM_EB                      ( BIT(18) )
#define BIT_BUSMON4_EB                  ( BIT(17) )
#define BIT_BUSMON3_EB                  ( BIT(16) )
#define BIT_BUSMON2_EB                  ( BIT(15) )
#define BIT_ROT_EB                      ( BIT(14) )
#define BIT_VSP_EB                      ( BIT(13) )
#define BIT_ISP_EB                      ( BIT(12) )
#define BIT_BUSMON1_EB                  ( BIT(11) )
#define BIT_DCAM_MIPI_EB                ( BIT(10) )
#define BIT_CCIR_EB                     ( BIT(9) )
#define BIT_NFC_EB                      ( BIT(8) )
#define BIT_BUSMON0_EB                  ( BIT(7) )
#define BIT_DMA_EB                      ( BIT(6) )
#define BIT_USBD_EB                     ( BIT(5) )
#define BIT_SDIO0_EB                    ( BIT(4) )
#define BIT_LCDC_EB                     ( BIT(3) )
#define BIT_CCIR_IN_EB                  ( BIT(2) )
#define BIT_DCAM_EB                     ( BIT(1) )

/* bits definitions for register REG_AHB_AHB_CTL1 */
#define BIT_ARM_DAHB_SLP_EN             ( BIT(16) )
#define BIT_MSTMTX_AUTO_GATE_EN         ( BIT(14) )
#define BIT_MCU_AUTO_GATE_EN            ( BIT(13) )
#define BIT_AHB_AUTO_GATE_EN            ( BIT(12) )
#define BIT_ARM_AUTO_GATE_EN            ( BIT(11) )
#define BIT_APB_FRC_SLEEP               ( BIT(10) )
#define BIT_EMC_CH_AUTO_GATE_EN         ( BIT(9) )
#define BIT_EMC_AUTO_GATE_EN            ( BIT(8) )

/* bits definitions for register REG_AHB_AHB_CTL2 */
#define BIT_DISPMTX_CLK_EN              ( BIT(11) )
#define BIT_MMMTX_CLK_EN                ( BIT(10) )
#define BIT_DISPC_CORE_CLK_EN           ( BIT(9) )
#define BIT_LCDC_CORE_CLK_EN            ( BIT(8) )
#define BIT_ISP_CORE_CLK_EN             ( BIT(7) )
#define BIT_VSP_CORE_CLK_EN             ( BIT(6) )
#define BIT_DCAM_CORE_CLK_EN            ( BIT(5) )
#define BITS_MCU_SHM0_CTRL(_x_)         ( (_x_) << 3 & (BIT(3)|BIT(4)) )

/* bits definitions for register REG_AHB_AHB_CTL3 */
#define BIT_CLK_ULPI_EN                 ( BIT(10) )
#define BIT_UTMI_SUSPEND_INV            ( BIT(9) )
#define BIT_UTMIFS_TX_EN_INV            ( BIT(8) )
#define BIT_CLK_UTMIFS_EN               ( BIT(7) )
#define BIT_CLK_USB_REF_EN              ( BIT(6) )
#define BIT_BUSMON_SEL1                 ( BIT(5) )
#define BIT_BUSMON_SEL0                 ( BIT(4) )
#define BIT_USB_M_HBIGENDIAN            ( BIT(2) )
#define BIT_USB_S_HBIGEIDIAN            ( BIT(1) )
#define BIT_CLK_USB_REF_SEL             ( BIT(0) )

/* bits definitions for register REG_AHB_SOFT_RST */
#define BIT_DISPMTX_SOFT_RST            ( BIT(31) )
#define BIT_MMMTX_SOFT_RST              ( BIT(30) )
#define BIT_CA5_CORE1_SOFT_RST          ( BIT(29) )
#define BIT_CA5_CORE0_SOFT_RST          ( BIT(28) )
#define BIT_MIPI_CSIHOST_SOFT_RST       ( BIT(27) )
#define BIT_MIPI_DSIHOST_SOFT_RST       ( BIT(26) )
#define BIT_SPINLOCK_SOFT_RST           ( BIT(25) )
#define BIT_CAM1_SOFT_RST               ( BIT(24) )
#define BIT_CAM0_SOFT_RST               ( BIT(23) )
#define BIT_SD2_SOFT_RST                ( BIT(22) )
#define BIT_EMMC_SOFT_RST               ( BIT(21) )
#define BIT_DISPC_SOFT_RST              ( BIT(20) )
#define BIT_G3D_SOFT_RST                ( BIT(19) )
#define BIT_DBG_SOFT_RST                ( BIT(18) )
#define BIT_CA2AP_AB_SOFT_RST           ( BIT(17) )
#define BIT_SD1_SOFT_RST                ( BIT(16) )
#define BIT_VSP_SOFT_RST                ( BIT(15) )
#define BIT_ADC_SOFT_RST                ( BIT(14) )
#define BIT_DRM_SOFT_RST                ( BIT(13) )
#define BIT_SD0_SOFT_RST                ( BIT(12) )
#define BIT_EMC_SOFT_RST                ( BIT(11) )
#define BIT_ROT_SOFT_RST                ( BIT(10) )
#define BIT_ISP_SOFT_RST                ( BIT(8) )
#define BIT_USBPHY_SOFT_RST             ( BIT(7) )
#define BIT_USBD_UTMI_SOFT_RST          ( BIT(6) )
#define BIT_NFC_SOFT_RST                ( BIT(5) )
#define BIT_LCDC_SOFT_RST               ( BIT(3) )
#define BIT_CCIR_SOFT_RST               ( BIT(2) )
#define BIT_DCAM_SOFT_RST               ( BIT(1) )
#define BIT_DMA_SOFT_RST                ( BIT(0) )

/* bits definitions for register REG_AHB_AHB_PAUSE */
#define BIT_MCU_DEEP_SLP_EN             ( BIT(2) )
#define BIT_MCU_SYS_SLP_EN              ( BIT(1) )
#define BIT_MCU_CORE_FRC_SLP            ( BIT(0) )

/* bits definitions for register REG_AHB_REMAP */
#define BITS_ARM_RES_STRAPPIN(_x_)      ( (_x_) << 30 & (BIT(30)|BIT(31)) )
#define BIT_FUNC_TEST_MODE_AS_SEL       ( BIT(8) )
#define BIT_FUNC_TEST_MODE              ( BIT(7) )
#define BIT_ARM_BOOT_MD3                ( BIT(6) )
#define BIT_ARM_BOOT_MD2                ( BIT(5) )
#define BIT_ARM_BOOT_MD1                ( BIT(4) )
#define BIT_ARM_BOOT_MD0                ( BIT(3) )
#define BIT_USB_DLOAD_EN                ( BIT(2) )
#define BIT_REMAP                       ( BIT(0) )

/* bits definitions for register REG_AHB_MIPI_PHY_CTRL */
#define BIT_MIPI_CPHY_EN                ( BIT(1) )
#define BIT_MIPI_DPHY_EN                ( BIT(0) )

/* bits definitions for register REG_AHB_DISPC_CTRL */
#define BITS_CLK_DISPC_DPI_DIV(_x_)     ( (_x_) << 19 & (BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)) )
#define BITS_CLK_DISPC_DPIPLL_SEL(_x_)  ( (_x_) << 17 & (BIT(17)|BIT(18)) )
#define BITS_CLK_DISPC_DBI_DIV(_x_)     ( (_x_) << 11 & (BIT(11)|BIT(12)|BIT(13)) )
#define BITS_CLK_DISPC_DBIPLL_SEL(_x_)  ( (_x_) << 9 & (BIT(9)|BIT(10)) )
#define BITS_CLK_DISPC_DIV(_x_)         ( (_x_) << 3 & (BIT(3)|BIT(4)|BIT(5)) )
#define BITS_CLK_DISPC_PLL_SEL(_x_)     ( (_x_) << 1 & (BIT(1)|BIT(2)) )

/* bits definitions for register REG_AHB_ARM_CLK */
#define BITS_AHB_DIV_INUSE(_x_)         ( (_x_) << 27 & (BIT(27)|BIT(28)|BIT(29)) )
#define BIT_AHB_ERR_YET                 ( BIT(26) )
#define BIT_AHB_ERR_CLR                 ( BIT(25) )
#define BITS_CLK_MCU_SEL(_x_)           ( (_x_) << 23 & (BIT(23)|BIT(24)) )
#define BITS_CLK_ARM_PERI_DIV(_x_)      ( (_x_) << 20 & (BIT(20)|BIT(21)|BIT(22)) )
#define BITS_CLK_DBG_DIV(_x_)           ( (_x_) << 14 & (BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )
#define BITS_CLK_EMC_SEL(_x_)           ( (_x_) << 12 & (BIT(12)|BIT(13)) )
#define BITS_CLK_EMC_DIV(_x_)           ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)|BIT(11)) )
#define BITS_CLK_AHB_DIV(_x_)           ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BIT_CLK_EMC_SYNC_SEL            ( BIT(3) )
#define BITS_CLK_ARM_DIV(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AHB_AHB_SDIO_CTRL */
#define BIT_EMMC_SLOT_SEL               ( BIT(5) )
#define BIT_SDIO2_SLOT_SEL              ( BIT(4) )
#define BITS_SDIO1_SLOT_SEL(_x_)        ( (_x_) << 2 & (BIT(2)|BIT(3)) )
#define BITS_SDIO0_SLOT_SEL(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AHB_AHB_CTL4 */
#define BIT_RX_CLK_SEL_ARM              ( BIT(31) )
#define BIT_RX_CLK_INV_ARM              ( BIT(30) )
#define BIT_RX_INV                      ( BIT(29) )

/* bits definitions for register REG_AHB_AHB_CTL5 */
#define BIT_BUSMON4_BIGEND_EN           ( BIT(17) )
#define BIT_BUSMON3_BIGEND_EN           ( BIT(16) )
#define BIT_BUSMON2_BIGEND_EN           ( BIT(15) )
#define BIT_EMMC_BIGEND_EN              ( BIT(14) )
#define BIT_SDIO2_BIGEND_EN             ( BIT(13) )
#define BIT_DISPC_BIGEND_EN             ( BIT(12) )
#define BIT_SDIO1_BIGEND_EN             ( BIT(11) )
#define BIT_SHRAM0_BIGEND_EN            ( BIT(9) )
#define BIT_BUSMON1_BIGEND_EN           ( BIT(8) )
#define BIT_BUSMON0_BIGEND_EN           ( BIT(7) )
#define BIT_ROT_BIGEND_EN               ( BIT(6) )
#define BIT_SDIO0_BIGEND_EN             ( BIT(3) )
#define BIT_LCDC_BIGEND_EN              ( BIT(2) )
#define BIT_DMA_BIGEND_EN               ( BIT(0) )

/* bits definitions for register REG_AHB_AHB_STATUS */
#define BIT_APB_PERI_EN                 ( BIT(20) )
#define BIT_DSP_MAHB_SLP_EN             ( BIT(19) )
#define BIT_DMA_BUSY                    ( BIT(18) )
#define BIT_EMC_SLEEP                   ( BIT(17) )
#define BIT_EMC_STOP                    ( BIT(16) )
#define BITS_EMC_CTL_STA(_x_)           ( (_x_) << 8 & (BIT(8)|BIT(9)|BIT(10)) )
#define BIT_EMC_STOP_CH7                ( BIT(7) )
#define BIT_EMC_STOP_CH6                ( BIT(6) )
#define BIT_EMC_STOP_CH5                ( BIT(5) )
#define BIT_EMC_STOP_CH4                ( BIT(4) )
#define BIT_EMC_STOP_CH3                ( BIT(3) )
#define BIT_EMC_STOP_CH2                ( BIT(2) )
#define BIT_EMC_STOP_CH1                ( BIT(1) )
#define BIT_EMC_STOP_CH0                ( BIT(0) )

/* bits definitions for register REG_AHB_CA5_CFG */
#define BIT_CA5_WDRESET_EN              ( BIT(18) )
#define BIT_CA5_TS_EN                   ( BIT(17) )
#define BIT_CA5_CORE1_GATE_EN           ( BIT(16) )
#define BIT_CA5_CFGSDISABLE             ( BIT(13) )
#define BITS_CA5_CLK_AXI_DIV(_x_)       ( (_x_) << 11 & (BIT(11)|BIT(12)) )
#define BIT_CA5_CLK_DBG_EN_SEL          ( BIT(10) )
#define BIT_CA5_CLK_DBG_EN              ( BIT(9) )
#define BIT_CA5_DBGEN                   ( BIT(8) )
#define BIT_CA5_NIDEN                   ( BIT(7) )
#define BIT_CA5_SPIDEN                  ( BIT(6) )
#define BIT_CA5_SPNIDEN                 ( BIT(5) )
#define BIT_CA5_CPI15DISABLE            ( BIT(4) )
#define BIT_CA5_TEINIT                  ( BIT(3) )
#define BIT_CA5_L1RSTDISABLE            ( BIT(2) )
#define BIT_CA5_L2CFGEND                ( BIT(1) )
#define BIT_CA5_L2SPNIDEN               ( BIT(0) )

/* bits definitions for register REG_AHB_ISP_CTRL */
#define BITS_CLK_ISP_DIV(_x_)           ( (_x_) << 2 & (BIT(2)|BIT(3)|BIT(4)) )
#define BITS_CLK_ISPPLL_SEL(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)) )

/* bits definitions for register REG_AHB_CP_AHB_ARM_CLK */
#define BITS_CP_AHB_DIV_INUSE(_x_)      ( (_x_) << 27 & (BIT(27)|BIT(28)|BIT(29)) )
#define BIT_CP_AHB_ERR_YET              ( BIT(26) )
#define BIT_CP_AHB_ERR_CLR              ( BIT(25) )
#define BITS_CLK_CP_MCU_SEL(_x_)        ( (_x_) << 23 & (BIT(23)|BIT(24)) )
#define BITS_CLK_CP_AHB_DIV(_x_)        ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)) )
#define BITS_CLK_CP_ARM_DIV(_x_)        ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AHB_CP_AHB_CTL */
#define BIT_CP_AP_JTAG_CHAIN_EN         ( BIT(3) )
#define BIT_ASHB_CPTOAP_EN_I            ( BIT(2) )
#define BIT_CP_RAM_SEL                  ( BIT(1) )
#define BIT_CLK_CP_EN                   ( BIT(0) )

/* bits definitions for register REG_AHB_CP_RST */
#define BIT_CP_CORE_SRST_N              ( BIT(0) )

/* bits definitions for register REG_AHB_CP_SLEEP_CTRL */
#define BIT_FORCE_CP_DEEP_SLEEP_EN      ( BIT(0) )

/* bits definitions for register REG_AHB_DEEPSLEEP_STATUS */
#define BITS_CP_SLEEP_FLAG(_x_)         ( (_x_) << 16 & (BIT(16)|BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27)|BIT(28)|BIT(29)|BIT(30)|BIT(31)) )
#define BITS_AP_SLEEP_FLAG(_x_)         ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AHB_DDR_PHY_Z_VALUE */
#define BITS_DDR_PHY_Z_VALUE(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)|BIT(16)|BIT(17)|BIT(18)|BIT(19)) )

/* bits definitions for register REG_AHB_DSP_JTAG_CTRL */
#define BIT_CEVA_SW_JTAG_ENA            ( BIT(8) )
#define BIT_STDO                        ( BIT(4) )
#define BIT_STCK                        ( BIT(3) )
#define BIT_STMS                        ( BIT(2) )
#define BIT_STDI                        ( BIT(1) )
#define BIT_STRTCK                      ( BIT(0) )

/* bits definitions for register REG_AHB_DSP_BOOT_EN */
#define BIT_ASHB_ARMTODSP_EN_I          ( BIT(2) )
#define BIT_FRC_CLK_DSP_EN              ( BIT(1) )
#define BIT_DSP_BOOT_EN                 ( BIT(0) )

/* bits definitions for register REG_AHB_DSP_RST */
#define BIT_DSP_SYS_SRST                ( BIT(1) )
#define BIT_DSP_CORE_SRST_N             ( BIT(0) )

/* bits definitions for register REG_AHB_BIGEND_PORT */
#define BIT_AHB_BIGEND_PROT             ( BIT(31) )
#define BITS_BIGEND_PROT_VAL(_x_)       ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)|BIT(3)|BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)|BIT(10)|BIT(11)|BIT(12)|BIT(13)|BIT(14)|BIT(15)) )

/* bits definitions for register REG_AHB_USB_PHY_TUNE */
#define BITS_OTGTUNE(_x_)               ( (_x_) << 28 & (BIT(28)|BIT(29)|BIT(30)) )
#define BITS_COMPDISTUNE(_x_)           ( (_x_) << 24 & (BIT(24)|BIT(25)|BIT(26)) )
#define BIT_TXPREEMPPULSETUNE           ( BIT(20) )
#define BITS_TXRESTUNE(_x_)             ( (_x_) << 18 & (BIT(18)|BIT(19)) )
#define BITS_TXHSXVTUNE(_x_)            ( (_x_) << 16 & (BIT(16)|BIT(17)) )
#define BITS_TXVREFTUNE(_x_)            ( (_x_) << 12 & (BIT(12)|BIT(13)|BIT(14)|BIT(15)) )
#define BITS_TXPREEMPAMP(_x_)           ( (_x_) << 10 & (BIT(10)|BIT(11)) )
#define BITS_TXRISETUNE(_x_)            ( (_x_) << 8 & (BIT(8)|BIT(9)) )
#define BITS_TXFSLSTUNE(_x_)            ( (_x_) << 4 & (BIT(4)|BIT(5)|BIT(6)|BIT(7)) )
#define BITS_SQRXTUNE(_x_)              ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AHB_USB_PHY_CTRL */
#define BIT_TXBITSTUFFENH               ( BIT(23) )
#define BIT_TXBITSTUFFEN                ( BIT(22) )
#define BIT_DMPULLDOWN                  ( BIT(21) )
#define BIT_DPPULLDOWN                  ( BIT(20) )
#define BIT_DMPULLUP                    ( BIT(9) )
#define BIT_COMMONONN                   ( BIT(8) )
#define BITS_REFCLKSEL(_x_)             ( (_x_) << 4 & (BIT(4)|BIT(5)) )
#define BITS_USBPHY_FSEL(_x_)           ( (_x_) << 0 & (BIT(0)|BIT(1)|BIT(2)) )

/* bits definitions for register REG_AHB_MTX_CTRL */
#define BIT_DSPP_BUF_EN                 ( BIT(10) )
#define BIT_DSPD_BUF_EN                 ( BIT(9) )
#define BIT_DSP_MTX_DMA_BUF_EN          ( BIT(8) )
#define BIT_MST_MTX_MST_BUF_EN          ( BIT(7) )
#define BIT_MST_MTX_MST_BUF_FRC_EN      ( BIT(6) )

/* vars definitions for controller REGS_AHB */
#define REG_AHB_SET(A)                  ( A + 0x1000 )
#define REG_AHB_CLR(A)                  ( A + 0x2000 )

#endif //__REGS_AHB_H__
