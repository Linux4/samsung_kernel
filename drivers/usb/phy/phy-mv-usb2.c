/*
 * Copyright (C) 2013 Marvell Inc.
 *
 * Author:
 *	Chao Xie <xiechao.mail@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/resource.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/platform_data/mv_usb.h>
#include <linux/usb/phy.h>
#include <linux/usb/mv_usb2_phy.h>
#include <linux/cputype.h>

/* phy regs */
/* for pxa910 and mmp2, there is no revision register */
#define PHY_55NM_REVISION		0x0
#define PHY_55NM_CTRL		0x4
#define PHY_55NM_PLL		0x8
#define PHY_55NM_TX			0xc
#define PHY_55NM_RX			0x10
#define PHY_55NM_IVREF		0x14
#define PHY_55NM_T0			0x18
#define PHY_55NM_T1			0x1c
#define PHY_55NM_T2			0x20
#define PHY_55NM_T3			0x24
#define PHY_55NM_T4			0x28
#define PHY_55NM_T5			0x2c
#define PHY_55NM_RESERVE		0x30
#define PHY_55NM_USB_INT		0x34
#define PHY_55NM_DBG_CTL		0x38
#define PHY_55NM_OTG_ADDON		0x3c

/* For UTMICTRL Register */
#define PHY_55NM_CTRL_USB_CLK_EN			(1 << 31)
/* pxa168 */
#define PHY_55NM_CTRL_SUSPEND_SET1			(1 << 30)
#define PHY_55NM_CTRL_SUSPEND_SET2			(1 << 29)
#define PHY_55NM_CTRL_RXBUF_PDWN			(1 << 24)
#define PHY_55NM_CTRL_TXBUF_PDWN			(1 << 11)

#define PHY_55NM_CTRL_INPKT_DELAY_SHIFT		30
#define PHY_55NM_CTRL_INPKT_DELAY_SOF_SHIFT		28
#define PHY_55NM_CTRL_PU_REF_SHIFT			20
#define PHY_55NM_CTRL_ARC_PULLDN_SHIFT		12
#define PHY_55NM_CTRL_PLL_PWR_UP_SHIFT		1
#define PHY_55NM_CTRL_PWR_UP_SHIFT			0

/* For UTMI_PLL Register */
#define PHY_55NM_PLL_PLLCALI12_SHIFT		29
#define PHY_55NM_PLL_PLLCALI12_MASK			(0x3 << 29)

#define PHY_55NM_PLL_PLLVDD18_SHIFT			27
#define PHY_55NM_PLL_PLLVDD18_MASK			(0x3 << 27)

#define PHY_55NM_PLL_PLLVDD12_SHIFT			25
#define PHY_55NM_PLL_PLLVDD12_MASK			(0x3 << 25)

#define PHY_55NM_PLL_PLL_READY			(0x1 << 23)
#define PHY_55NM_PLL_KVCO_EXT			(0x1 << 22)
#define PHY_55NM_PLL_VCOCAL_START			(0x1 << 21)

#define PHY_55NM_PLL_KVCO_SHIFT			15
#define PHY_55NM_PLL_KVCO_MASK			(0x7 << 15)

#define PHY_55NM_PLL_ICP_SHIFT			12
#define PHY_55NM_PLL_ICP_MASK			(0x7 << 12)

#define PHY_55NM_PLL_FBDIV_SHIFT			4
#define PHY_55NM_PLL_FBDIV_MASK			(0xFF << 4)

#define PHY_55NM_PLL_REFDIV_SHIFT			0
#define PHY_55NM_PLL_REFDIV_MASK			(0xF << 0)

/* For UTMI_TX Register */
#define PHY_55NM_TX_REG_EXT_FS_RCAL_SHIFT		27
#define PHY_55NM_TX_REG_EXT_FS_RCAL_MASK		(0xf << 27)

#define PHY_55NM_TX_REG_EXT_FS_RCAL_EN_SHIFT	26
#define PHY_55NM_TX_REG_EXT_FS_RCAL_EN_MASK		(0x1 << 26)

#define PHY_55NM_TX_TXVDD12_SHIFT			22
#define PHY_55NM_TX_TXVDD12_MASK			(0x3 << 22)

#define PHY_55NM_TX_CK60_PHSEL_SHIFT		17
#define PHY_55NM_TX_CK60_PHSEL_MASK			(0xf << 17)

#define PHY_55NM_TX_IMPCAL_VTH_SHIFT		14
#define PHY_55NM_TX_IMPCAL_VTH_MASK			(0x7 << 14)

#define PHY_55NM_TX_REG_RCAL_START			(0x1 << 12)

#define PHY_55NM_TX_LOW_VDD_EN_SHIFT		11

#define PHY_55NM_TX_AMP_SHIFT			0
#define PHY_55NM_TX_AMP_MASK			(0x7 << 0)

/* For UTMI_RX Register */
#define PHY_55NM_RX_REG_SQ_LENGTH_SHIFT		15
#define PHY_55NM_RX_REG_SQ_LENGTH_MASK		(0x3 << 15)

#define PHY_55NM_RX_SQ_THRESH_SHIFT			4
#define PHY_55NM_RX_SQ_THRESH_MASK			(0xf << 4)

/* For UTMI_OTG_ADDON Register. Only for pxa168 */
#define PHY_55NM_OTG_ADDON_OTG_ON			(1 << 0)

/* For pxa988 the register mapping are changed*/
#define PHY_40NM_PLL0		0x4
#define PHY_40NM_PLL1		0x8
#define PHY_40NM_TX0			0x10
#define PHY_40NM_TX1			0x14
#define PHY_40NM_TX2			0x18
#define PHY_40NM_RX0			0x20
#define PHY_40NM_RX1			0x24
#define PHY_40NM_RX2			0x28
#define PHY_40NM_ANA0		0x30
#define PHY_40NM_ANA1		0x34
#define PHY_40NM_DIG0		0x3c
#define PHY_40NM_DIG1		0x40
#define PHY_40NM_DIG2		0x44
#define PHY_40NM_T0			0x4c
#define PHY_40NM_T1			0x50
#define PHY_40NM_CHARGE0		0x58
#define PHY_40NM_OTG			0x5C
#define PHY_40NM_PHY_MON		0x60
#define PHY_40NM_RESERVE0		0x64
#define PHY_40NM_CTRL		0x104
#define PHY_40NM_STATUS_A0C		0x108
#define PHY_40NM_STATUS		0x128

/* default values are got from spec */
#define PHY_40NM_PLL0_DEFAULT	0x5A78
#define PHY_40NM_PLL1_DEFAULT	0x0231
#define PHY_40NM_TX0_DEFAULT		0x0488
#define PHY_40NM_TX1_DEFAULT		0x05B0
#define PHY_40NM_TX2_DEFAULT		0x02FF
#define PHY_40NM_RX0_DEFAULT		0xAA71
#define PHY_40NM_RX1_DEFAULT		0x3892
#define PHY_40NM_RX2_DEFAULT		0x0125
#define PHY_40NM_ANA1_DEFAULT	0x1680
#define PHY_40NM_OTG_DEFAULT		0x0
#define PHY_40NM_CTRL_DEFAULT	0x00801000
#define PHY_40NM_CTRL_OTG_DEFAULT	0x0398F000

#define PHY_40NM_PLL0_PLLVDD18(x)			(((x) & 0x3) << 14)
#define PHY_40NM_PLL0_REFDIV(x)			(((x) & 0x1f) << 9)
#define PHY_40NM_PLL0_FBDIV(x)			(((x) & 0x1ff) << 0)

#define PHY_40NM_PLL1_PLL_READY			(0x1 << 15)
#define PHY_40NM_PLL1_PLL_CONTROL_BY_PIN		(0x1 << 14)
#define PHY_40NM_PLL1_PU_PLL				(0x1 << 13)
#define PHY_40NM_PLL1_PLL_LOCK_BYPASS		(0x1 << 12)
#define PHY_40NM_PLL1_DLL_RESET			(0x1 << 11)
#define PHY_40NM_PLL1_ICP(x)				(((x) & 0x7) << 8)
#define PHY_40NM_PLL1_KVCO_EXT			(0x1 << 7)
#define PHY_40NM_PLL1_KVCO(x)			(((x) & 0x7) << 4)
#define PHY_40NM_PLL1_CLK_BLK_EN			(0x1 << 3)
#define PHY_40NM_PLL1_VCOCAL_START			(0x1 << 2)
#define PHY_40NM_PLL1_PLLCAL12(x)			(((x) & 0x3) << 0)

#define PHY_40NM_TX0_TXDATA_BLK_EN			(0x1 << 14)
#define PHY_40NM_TX0_RCAL_START			(0x1 << 13)
#define PHY_40NM_TX0_EXT_HS_RCAL_EN			(0x1 << 12)
#define PHY_40NM_TX0_EXT_FS_RCAL_EN			(0x1 << 11)
#define PHY_40NM_TX0_IMPCAL_VTH(x)			(((x) & 0x7) << 8)
#define PHY_40NM_TX0_EXT_HS_RCAL(x)			(((x) & 0xf) << 4)
#define PHY_40NM_TX0_EXT_FS_RCAL(x)			(((x) & 0xf) << 0)

#define PHY_40NM_TX1_TXVDD15(x)			(((x) & 0x3) << 10)
#define PHY_40NM_TX1_TXVDD12(x)			(((x) & 0x3) << 8)
#define PHY_40NM_TX1_LOWVDD_EN			(0x1 << 7)
#define PHY_40NM_TX1_AMP(x)				(((x) & 0x7) << 4)
#define PHY_40NM_TX1_CK60_PHSEL(x)			(((x) & 0xf) << 0)

#define PHY_40NM_TX2_DRV_SLEWRATE(x)			(((x) & 0x3) << 10)
#define PHY_40NM_TX2_IMP_CAL_DLY(x)			(((x) & 0x3) << 8)
#define PHY_40NM_TX2_FSDRV_EN(x)			(((x) & 0xf) << 4)
#define PHY_40NM_TX2_HSDEV_EN(x)			(((x) & 0xf) << 0)

#define PHY_40NM_RX0_PHASE_FREEZE_DLY		(0x1 << 15)
#define PHY_40NM_RX0_USQ_LENGTH			(0x1 << 14)
#define PHY_40NM_RX0_ACQ_LENGTH(x)			(((x) & 0x3) << 12)
#define PHY_40NM_RX0_SQ_LENGTH(x)			(((x) & 0x3) << 10)
#define PHY_40NM_RX0_DISCON_THRESH(x)		(((x) & 0x3) << 8)
#define PHY_40NM_RX0_SQ_THRESH(x)			(((x) & 0xf) << 4)
#define PHY_40NM_RX0_LPF_COEF(x)			(((x) & 0x3) << 2)
#define PHY_40NM_RX0_INTPI(x)			(((x) & 0x3) << 0)

#define PHY_40NM_RX1_EARLY_VOS_ON_EN			(0x1 << 13)
#define PHY_40NM_RX1_RXDATA_BLOCK_EN			(0x1 << 12)
#define PHY_40NM_RX1_EDGE_DET_EN			(0x1 << 11)
#define PHY_40NM_RX1_CAP_SEL(x)			(((x) & 0x7) << 8)
#define PHY_40NM_RX1_RXDATA_BLOCK_LENGTH(x)		(((x) & 0x3) << 6)
#define PHY_40NM_RX1_EDGE_DET_SEL(x)			(((x) & 0x3) << 4)
#define PHY_40NM_RX1_CDR_COEF_SEL			(0x1 << 3)
#define PHY_40NM_RX1_CDR_FASTLOCK_EN			(0x1 << 2)
#define PHY_40NM_RX1_S2TO3_DLY_SEL(x)		(((x) & 0x3) << 0)

#define PHY_40NM_RX2_USQ_FILTER			(0x1 << 8)
#define PHY_40NM_RX2_SQ_CM_SEL			(0x1 << 7)
#define PHY_40NM_RX2_SAMPLER_CTRL			(0x1 << 6)
#define PHY_40NM_RX2_SQ_BUFFER_EN			(0x1 << 5)
#define PHY_40NM_RX2_SQ_ALWAYS_ON			(0x1 << 4)
#define PHY_40NM_RX2_RXVDD18(x)			(((x) & 0x3) << 2)
#define PHY_40NM_RX2_RXVDD12(x)			(((x) & 0x3) << 0)

#define PHY_40NM_ANA0_BG_VSEL(x)			(((x) & 0x3) << 8)
#define PHY_40NM_ANA0_DIG_SEL(x)			(((x) & 0x3) << 6)
#define PHY_40NM_ANA0_TOPVDD18(x)			(((x) & 0x3) << 4)
#define PHY_40NM_ANA0_VDD_USB2_DIG_TOP_SEL		(0x1 << 3)
#define PHY_40NM_ANA0_IPTAT_SEL(x)			(((x) & 0x7) << 0)

#define PHY_40NM_ANA1_PU_ANA				(0x1 << 14)
#define PHY_40NM_ANA1_ANA_CONTROL_BY_PIN		(0x1 << 13)
#define PHY_40NM_ANA1_SEL_LPFR			(0x1 << 12)
#define PHY_40NM_ANA1_V2I_EXT			(0x1 << 11)
#define PHY_40NM_ANA1_V2I(x)				(((x) & 0x7) << 8)
#define PHY_40NM_ANA1_R_ROTATE_SEL			(0x1 << 7)
#define PHY_40NM_ANA1_STRESS_TEST_MODE		(0x1 << 6)
#define PHY_40NM_ANA1_TESTMON_ANA(x)			(((x) & 0x3f) << 0)

#define PHY_40NM_DIG0_FIFO_UF			(0x1 << 15)
#define PHY_40NM_DIG0_FIFO_OV			(0x1 << 14)
#define PHY_40NM_DIG0_FS_EOP_MODE			(0x1 << 13)
#define PHY_40NM_DIG0_HOST_DISCON_SEL1		(0x1 << 12)
#define PHY_40NM_DIG0_HOST_DISCON_SEL0		(0x1 << 11)
#define PHY_40NM_DIG0_FORCE_END_EN			(0x1 << 10)
#define PHY_40NM_DIG0_SYNCDET_WINDOW_EN		(0x1 << 8)
#define PHY_40NM_DIG0_CLK_SUSPEND_EN			(0x1 << 7)
#define PHY_40NM_DIG0_HS_DRIBBLE_EN			(0x1 << 6)
#define PHY_40NM_DIG0_SYNC_NUM(x)			(((x) & 0x3) << 4)
#define PHY_40NM_DIG0_FIFO_FILL_NUM(x)		(((x) & 0xf) << 0)

#define PHY_40NM_DIG1_FS_RX_ERROR_MODE2		(0x1 << 15)
#define PHY_40NM_DIG1_FS_RX_ERROR_MODE1		(0x1 << 14)
#define PHY_40NM_DIG1_FS_RX_ERROR_MODE		(0x1 << 13)
#define PHY_40NM_DIG1_CLK_OUT_SEL			(0x1 << 12)
#define PHY_40NM_DIG1_EXT_TX_CLK_SEL			(0x1 << 11)
#define PHY_40NM_DIG1_ARC_DPDM_MODE			(0x1 << 10)
#define PHY_40NM_DIG1_DP_PULLDOWN			(0x1 << 9)
#define PHY_40NM_DIG1_DM_PULLDOWN			(0x1 << 8)
#define PHY_40NM_DIG1_SYNC_IGNORE_SQ			(0x1 << 7)
#define PHY_40NM_DIG1_SQ_RST_RX			(0x1 << 6)
#define PHY_40NM_DIG1_MON_SEL(x)			(((x) & 0x3f) << 0)

#define PHY_40NM_DIG2_PAD_STRENGTH(x)		(((x) & 0x1f) << 8)
#define PHY_40NM_DIG2_LONG_EOP			(0x1 << 5)
#define PHY_40NM_DIG2_NOVBUS_DPDM00			(0x1 << 4)
#define PHY_40NM_DIG2_ALIGN_FS_OUTEN			(0x1 << 2)
#define PHY_40NM_DIG2_HS_HDL_SYNC			(0x1 << 1)
#define PHY_40NM_DIG2_FS_HDL_OPMD			(0x1 << 0)

#define PHY_40NM_CHARGE0_ENABLE_SWITCH		(0x1 << 3)
#define PHY_40NM_CHARGE0_PU_CHRG_DTC			(0x1 << 2)
#define PHY_40NM_CHARGE0_TESTMON_CHRGDTC(x)		(((x) & 0x3) << 0)

#define PHY_40NM_OTG_TESTMODE(x)			(((x) & 0x7) << 0)
#define PHY_40NM_OTG_CONTROL_BY_PIN			(0x1 << 4)
#define PHY_40NM_OTG_PU				(0x1 << 3)

#define PHY_40NM_CDP_EN				(0x1 << 2)
#define PHY_40NM_DCP_EN				(0x1 << 3)
#define PHY_40NM_PD_EN				(0x1 << 4)
#define PHY_40NM_CDP_DM_AUTO_SWITCH			(0x1 << 5)
#define PHY_40NM_ENABLE_SWITCH_DM			(0x1 << 6)
#define PHY_40NM_ENABLE_SWITCH_DP			(0x1 << 7)
#define PHY_40NM_VDAT_CHARGE				(0x3 << 8)
#define PHY_40NM_VSRC_CHARGE				(0x3 << 10)
#define PHY_40NM_DP_DM_SWAP_CTRL			(0x1 << 15)
#define PHY_40NM_CHARGER_STATUS_MASK			(0x1 << 4)

#define PHY_40NM_CTRL_AVALID_CONTROL			(0x3 << 24)
#define PHY_40NM_CTRL_PU_PLL				(0x1 << 1)
#define PHY_40NM_CTRL_PU				(0x1 << 0)

/* USB PXA1928 PHY mapping */
#define PHY_28NM_PLL_REG0		0x0
#define PHY_28NM_PLL_REG1		0x4
#define PHY_28NM_CAL_REG		0x8
#define PHY_28NM_TX_REG0		0x0C
#define PHY_28NM_TX_REG1		0x10
#define PHY_28NM_RX_REG0		0x14
#define PHY_28NM_RX_REG1		0x18
#define PHY_28NM_DIG_REG0		0x1C
#define PHY_28NM_DIG_REG1		0x20
#define PHY_28NM_TEST_REG0		0x24
#define PHY_28NM_TEST_REG1		0x28
#define PHY_28NM_MOC_REG		0x2C
#define PHY_28NM_PHY_RESERVE	0x30
#define PHY_28NM_OTG_REG		0x34
#define PHY_28NM_CHRG_DET		0x38
#define PHY_28NM_CTRL_REG0		0xC4
#define PHY_28NM_CTRL_REG1		0xC8
#define PHY_28NM_CTRL_REG2		0xD4
#define PHY_28NM_CTRL_REG3		0xDC

/* PHY_28NM_PLL_REG0 */
#define PHY_28NM_PLL_READY_MASK		(0x1 << 31)

#define PHY_28NM_PLL_SELLPFR_SHIFT		28
#define PHY_28NM_PLL_SELLPFR_MASK		(0x3 << 28)

#define PHY_28NM_PLL_FBDIV_SHIFT		16
#define PHY_28NM_PLL_FBDIV_MASK		(0x1ff << 16)

#define PHY_28NM_PLL_ICP_SHIFT			8
#define PHY_28NM_PLL_ICP_MASK			(0x7 << 8)

#define PHY_28NM_PLL_REFDIV_SHIFT		0
#define PHY_28NM_PLL_REFDIV_MASK		0x7f

/* PHY_28NM_PLL_REG1 */
#define PHY_28NM_PLL_PU_BY_REG_SHIFT		1
#define PHY_28NM_PLL_PU_BY_REG_MASK		(0x1 << 1)

#define PHY_28NM_PLL_PU_PLL_SHIFT		0
#define PHY_28NM_PLL_PU_PLL_MASK		(0x1 << 0)

/* PHY_28NM_CAL_REG */
#define PHY_28NM_PLL_PLLCAL_DONE_SHIFT		31
#define PHY_28NM_PLL_PLLCAL_DONE_MASK		(0x1 << 31)

#define PHY_28NM_PLL_IMPCAL_DONE_SHIFT		23
#define PHY_28NM_PLL_IMPCAL_DONE_MASK		(0x1 << 23)

#define PHY_28NM_PLL_KVCO_SHIFT		16
#define PHY_28NM_PLL_KVCO_MASK			(0x7 << 16)

#define PHY_28NM_PLL_CAL12_SHIFT		20
#define PHY_28NM_PLL_CAL12_MASK		(0x3 << 20)

#define PHY_28NM_IMPCAL_VTH_SHIFT		8
#define PHY_28NM_IMPCAL_VTH_MASK		(0x7 << 8)

#define PHY_28NM_PLLCAL_START_SHIFT		22
#define PHY_28NM_IMPCAL_START_SHIFT		13

/* PHY_28NM_TX_REG0 */
#define PHY_28NM_TX_PU_BY_REG_SHIFT		25

#define PHY_28NM_TX_PU_ANA_SHIFT		24

#define PHY_28NM_TX_AMP_SHIFT			20
#define PHY_28NM_TX_AMP_MASK			(0x7 << 20)

/* PHY_28NM_RX_REG0 */
#define PHY_28NM_RX_SQ_ANA_DTC_SEL_SHIFT	28
#define PHY_28NM_RX_SQ_ANA_DTC_SEL_MASK		(0x1 << 28)

#define PHY_28NM_RX_SQ_THRESH_SHIFT		0
#define PHY_28NM_RX_SQ_THRESH_MASK		(0xf << 0)

/* PHY_28NM_RX_REG1 */
#define PHY_28NM_RX_SQCAL_DONE_SHIFT		31
#define PHY_28NM_RX_SQCAL_DONE_MASK		(0x1 << 31)

#define PHY_28NM_RX_EXT_SQ_AMP_CAL_EN_SHIFT	3
#define PHY_28NM_RX_EXT_SQ_AMP_CAL_EN_MASK	(0x1 << 3)
#define PHY_28NM_RX_EXT_SQ_AMP_CAL_SHIFT	0
#define PHY_28NM_RX_EXT_SQ_AMP_CAL_MASK		(0x7 << 0)

/* PHY_28NM_DIG_REG0 */
#define PHY_28NM_DIG_BITSTAFFING_ERR_MASK	(0x1 << 31)
#define PHY_28NM_DIG_SYNC_ERR_MASK		(0x1 << 30)

#define PHY_28NM_DIG_SQ_FILT_SHIFT		16
#define PHY_28NM_DIG_SQ_FILT_MASK		(0x7 << 16)

#define PHY_28NM_DIG_SQ_BLK_SHIFT		12
#define PHY_28NM_DIG_SQ_BLK_MASK		(0x7 << 12)

#define PHY_28NM_DIG_SYNC_NUM_SHIFT		0
#define PHY_28NM_DIG_SYNC_NUM_MASK		(0x3 << 0)

#define PHY_28NM_PLL_LOCK_BYPASS_SHIFT		7

/* PHY_28NM_OTG_REG */
#define PHY_28NM_OTG_CONTROL_BY_PIN_SHIFT	5
#define PHY_28NM_OTG_PU_OTG_SHIFT		4

#define PHY_28NM_CHGDTC_ENABLE_SWITCH_DM_SHIFT_28 13
#define PHY_28NM_CHGDTC_ENABLE_SWITCH_DP_SHIFT_28 12
#define PHY_28NM_CHGDTC_VSRC_CHARGE_SHIFT_28	10
#define PHY_28NM_CHGDTC_VDAT_CHARGE_SHIFT_28	8
#define PHY_28NM_CHGDTC_CDP_DM_AUTO_SWITCH_SHIFT_28 7
#define PHY_28NM_CHGDTC_DP_DM_SWAP_SHIFT_28	6
#define PHY_28NM_CHGDTC_PU_CHRG_DTC_SHIFT_28	5
#define PHY_28NM_CHGDTC_PD_EN_SHIFT_28			4
#define PHY_28NM_CHGDTC_DCP_EN_SHIFT_28		3
#define PHY_28NM_CHGDTC_CDP_EN_SHIFT_28		2
#define PHY_28NM_CHGDTC_TESTMON_CHRGDTC_SHIFT_28 0

#define PHY_28NM_CTRL0_VBUSVALID_CTL_SHIFT	12
#define PHY_28NM_CTRL0_VBUSVALID_CTL_MASK	(0x3 << 12)

#define PHY_28NM_CTRL1_CHRG_DTC_OUT_SHIFT_28	4
#define PHY_28NM_CTRL1_VBUSDTC_OUT_SHIFT_28		2
#define PHY_28NM_RX_SQCAL_START_SHIFT		4
#define PHY_28NM_RX_SQCAL_START_MASK		(0x1 << 4)

#define PHY_28NM_CTRL_REG0_SHIFT	12

static struct mv_usb2_phy *mv_phy_ptr;
static int _mv_usb2_phy_55nm_init(struct mv_usb2_phy *mv_phy)
{
	struct platform_device *pdev = mv_phy->pdev;
	unsigned int loops = 0;
	void __iomem *base = mv_phy->base;
	unsigned int val;

	val = readl(base + PHY_55NM_CTRL);
	/* Initialize the USB PHY power */
	if (mv_phy->drv_data.phy_rev == REV_PXA910) {
		val |= (1 << PHY_55NM_CTRL_INPKT_DELAY_SOF_SHIFT)
			| (1 << PHY_55NM_CTRL_PU_REF_SHIFT);
	}

	val |= (1 << PHY_55NM_CTRL_PLL_PWR_UP_SHIFT)
		| (1 << PHY_55NM_CTRL_PWR_UP_SHIFT);
	writel(val, base + PHY_55NM_CTRL);

	/* UTMI_PLL settings */
	val = readl(base + PHY_55NM_PLL);
	val &= ~(PHY_55NM_PLL_PLLVDD18_MASK
		| PHY_55NM_PLL_PLLVDD12_MASK
		| PHY_55NM_PLL_PLLCALI12_MASK
		| PHY_55NM_PLL_FBDIV_MASK
		| PHY_55NM_PLL_REFDIV_MASK
		| PHY_55NM_PLL_ICP_MASK
		| PHY_55NM_PLL_KVCO_MASK
		| PHY_55NM_PLL_VCOCAL_START);

	val |= (0xee << PHY_55NM_PLL_FBDIV_SHIFT)
		| (0xb << PHY_55NM_PLL_REFDIV_SHIFT)
		| (3 << PHY_55NM_PLL_PLLVDD18_SHIFT)
		| (3 << PHY_55NM_PLL_PLLVDD12_SHIFT)
		| (3 << PHY_55NM_PLL_PLLCALI12_SHIFT)
		| (1 << PHY_55NM_PLL_ICP_SHIFT)
		| (3 << PHY_55NM_PLL_KVCO_SHIFT);
	writel(val, base + PHY_55NM_PLL);

	/* UTMI_TX */
	val = readl(base + PHY_55NM_TX);
	val &= ~(PHY_55NM_TX_REG_EXT_FS_RCAL_EN_MASK
		| PHY_55NM_TX_TXVDD12_MASK
		| PHY_55NM_TX_CK60_PHSEL_MASK
		| PHY_55NM_TX_IMPCAL_VTH_MASK
		| PHY_55NM_TX_REG_EXT_FS_RCAL_MASK
		| PHY_55NM_TX_AMP_MASK
		| PHY_55NM_TX_REG_RCAL_START);
	val |= (3 << PHY_55NM_TX_TXVDD12_SHIFT)
		| (4 << PHY_55NM_TX_CK60_PHSEL_SHIFT)
		| (4 << PHY_55NM_TX_IMPCAL_VTH_SHIFT)
		| (8 << PHY_55NM_TX_REG_EXT_FS_RCAL_SHIFT)
		| (3 << PHY_55NM_TX_AMP_SHIFT);
	writel(val, base + PHY_55NM_TX);

	/* UTMI_RX */
	val = readl(base + PHY_55NM_RX);
	val &= ~(PHY_55NM_RX_SQ_THRESH_MASK
		| PHY_55NM_RX_REG_SQ_LENGTH_MASK);
	val |= (7 << PHY_55NM_RX_SQ_THRESH_SHIFT)
		| (2 << PHY_55NM_RX_REG_SQ_LENGTH_SHIFT);
	writel(val, base + PHY_55NM_RX);

	/* UTMI_IVREF */
	if (mv_phy->drv_data.phy_rev == REV_PXA168)
		/*
		 * Fixing Microsoft Altair board interface with NEC hub issue -
		 * Set UTMI_IVREF from 0x4a3 to 0x4bf.
		 */
		writel(0x4bf, base + PHY_55NM_IVREF);

	/*
	 * Toggle VCOCAL_START bit of UTMI_PLL. It is active at rising edge.
	 * It is low level by UTMI_PLL initialization above.
	 */
	val = readl(base + PHY_55NM_PLL);
	/*
	 * Delay 200us for low level, and 40us for high level.
	 * Make sure we can get a effective rising edege.
	 * It should be set to low after issue rising edge.
	 * The delay value is suggested by DE.
	 */
	udelay(300);
	val |= PHY_55NM_PLL_VCOCAL_START;
	writel(val, base + PHY_55NM_PLL);
	udelay(40);
	val &= ~PHY_55NM_PLL_VCOCAL_START;
	writel(val, base + PHY_55NM_PLL);

	/*
	 * Toggle REG_RCAL_START bit of UTMI_TX.
	 * it is low level by UTMI_TX initialization above.
	 */
	val = readl(base + PHY_55NM_TX);
	/* same as VCOCAL_START, except it is triggered by low->high->low */
	udelay(400);
	val |= PHY_55NM_TX_REG_RCAL_START;
	writel(val, base + PHY_55NM_TX);
	udelay(40);
	val &= ~PHY_55NM_TX_REG_RCAL_START;
	writel(val, base + PHY_55NM_TX);
	udelay(400);

	/* Make sure PHY PLL is ready */
	loops = 0;
	while (1) {
		val = readl(base + PHY_55NM_PLL);
		if (val & PHY_55NM_PLL_PLL_READY)
			break;
		udelay(1000);
		loops++;
		if (loops > 100) {
			dev_warn(&pdev->dev, "calibrate timeout, UTMI_PLL %x\n",
				 readl(base + PHY_55NM_PLL));
			return -ETIME;
		}
	}

	if (mv_phy->drv_data.phy_rev == REV_PXA168) {
		val = readl(base + PHY_55NM_RESERVE);
		val |= (1 << 5);
		writel(val, base + PHY_55NM_RESERVE);
		/* Turn on UTMI PHY OTG extension */
		writel(PHY_55NM_OTG_ADDON_OTG_ON,
		       base + PHY_55NM_OTG_ADDON);
	}

	return 0;
}

static int _mv_usb2_phy_55nm_shutdown(struct mv_usb2_phy *mv_phy)
{
	void __iomem *base = mv_phy->base;
	unsigned int val;

	if (mv_phy->drv_data.phy_rev == REV_PXA168)
		writel(0, base + PHY_55NM_OTG_ADDON);

	val = readl(base + PHY_55NM_CTRL);
	val &= ~(PHY_55NM_CTRL_RXBUF_PDWN
		| PHY_55NM_CTRL_TXBUF_PDWN
		| PHY_55NM_CTRL_USB_CLK_EN
		| (1 << PHY_55NM_CTRL_PWR_UP_SHIFT)
		| (1 << PHY_55NM_CTRL_PLL_PWR_UP_SHIFT));

	writel(val, base + PHY_55NM_CTRL);

	return 0;
}

static int _mv_usb2_phy_40nm_init(struct mv_usb2_phy *mv_phy)
{
	void __iomem *base = mv_phy->base;

	u16 tmp16;
	u32 tmp32;

	/* Program 0xd4207004[8:0]= 0xF0 */
	/* Program 0xd4207004[13:9]=0xD */
	writew((PHY_40NM_PLL0_DEFAULT
		& (~PHY_40NM_PLL0_FBDIV(~0))
		& (~PHY_40NM_PLL0_REFDIV(~0)))
		| PHY_40NM_PLL0_FBDIV(0xF0)
		| PHY_40NM_PLL0_REFDIV(0xD), base + PHY_40NM_PLL0);

	/* Program 0xd4207008[11:8]=0x1 */
	/* Program 0xd4207008[14]=0x0 */
	/* Program 0xd4207008[13]=0x1 */
	/* Program 0xd4207008[12]=0x1 */
	/* Program 0xd4207008[1:0]=0x3 */
	writew((PHY_40NM_PLL1_DEFAULT
		& (~PHY_40NM_PLL1_ICP(~0))
		& (~PHY_40NM_PLL1_PLL_CONTROL_BY_PIN)
		& (~PHY_40NM_PLL1_PLLCAL12(~0)))
		| PHY_40NM_PLL1_ICP(0x1)
		| PHY_40NM_PLL1_PU_PLL
		| PHY_40NM_PLL1_PLL_LOCK_BYPASS
		| PHY_40NM_PLL1_PLLCAL12(3), base + PHY_40NM_PLL1);

	/* Program 0xd4207014[6:4]=0x5 */
	/* Program 0xd4207014[9:8]=0x3 */
	/* Program 0xd4207014[3:0]=0x4 */
	writew((PHY_40NM_TX1_DEFAULT
		& (~PHY_40NM_TX1_AMP(~0))
		& (~PHY_40NM_TX1_TXVDD12(~0))
		& (~PHY_40NM_TX1_CK60_PHSEL(~0)))
		| PHY_40NM_TX1_AMP(5)
		| PHY_40NM_TX1_TXVDD12(3)
		| PHY_40NM_TX1_CK60_PHSEL(4), base + PHY_40NM_TX1);

	/* Program 0xd4207018[11:10]=0x2 */
	writew((PHY_40NM_TX2_DEFAULT & (~PHY_40NM_TX2_DRV_SLEWRATE(3)))
		| PHY_40NM_TX2_DRV_SLEWRATE(2), base + PHY_40NM_TX2);

	/* Program 0xd4207020[7:4]=0xa */
	writew((PHY_40NM_RX0_DEFAULT
		& (~PHY_40NM_RX0_SQ_THRESH(~0)))
		| PHY_40NM_RX0_SQ_THRESH(0xA), base + PHY_40NM_RX0);

	/* Program 0xd4207034[14]=0x1 */
	writew(PHY_40NM_ANA1_DEFAULT
		| PHY_40NM_ANA1_PU_ANA, base + PHY_40NM_ANA1);

	/* Program 0xd420705c[3]=0x1 */
	writew(PHY_40NM_OTG_DEFAULT
		| PHY_40NM_OTG_PU, base + PHY_40NM_OTG);

	/* Program 0xD4207104[1] = 0x1 */
	/* Program 0xD4207104[0] = 0x1 */
	tmp32 = readl(base + PHY_40NM_CTRL);
	writel(tmp32 | PHY_40NM_CTRL_PU_PLL
		| PHY_40NM_CTRL_PU, base + PHY_40NM_CTRL);

	/* Wait for 200us */
	udelay(200);

	/* Program 0xd4207008[2]=0x1 */
	tmp16 = readw(base + PHY_40NM_PLL1);
	writew(tmp16
		| PHY_40NM_PLL1_VCOCAL_START, base + PHY_40NM_PLL1);

	/* Wait for 400us */
	udelay(400);

	/* Polling 0xd4207008[15]=0x1 */
	while ((readw(base + PHY_40NM_PLL1)
		& PHY_40NM_PLL1_PLL_READY) == 0)
		pr_info("polling usb phy\n");

	/* Program 0xd4207010[13]=0x1 */
	tmp16 = readw(base + PHY_40NM_TX0);
	writew(tmp16 | PHY_40NM_TX0_RCAL_START, base + PHY_40NM_TX0);

	/* Wait for 40us */
	udelay(40);

	/* Program 0xd4207010[13]=0x0 */
	tmp16 = readw(base + PHY_40NM_TX0);
	writew(tmp16 & (~PHY_40NM_TX0_RCAL_START), base + PHY_40NM_TX0);

	/* Wait for 400us */
	udelay(400);

	pr_info("usb phy inited %x!\n", readl(base + PHY_40NM_CTRL));

	return 0;
}

static void _mv_usb2_phy_40nm_shutdown(struct mv_usb2_phy *mv_phy)
{
	void __iomem *base = mv_phy->base;
	u16 tmp16;
	u32 tmp32;

	pr_info("usb phy deinit!\n");

	/* Program 0xd4207008[14:13]=0x0 */
	tmp16 = readw(base + PHY_40NM_PLL1);
	writew(tmp16
		& (~PHY_40NM_PLL1_PLL_CONTROL_BY_PIN)
		& (~PHY_40NM_PLL1_PU_PLL), base + PHY_40NM_PLL1);

	/* Program 0xd4207034[14]=0x0 */
	tmp16 = readw(base + PHY_40NM_ANA1);
	writew(tmp16
		& (~PHY_40NM_ANA1_PU_ANA), base + PHY_40NM_ANA1);

	/* Program 0xD4207104[1] = 0x0 */
	/* Program 0xD4207104[0] = 0x0 */
	tmp32 = readl(base + PHY_40NM_CTRL);
	writel(tmp32
		& (~PHY_40NM_CTRL_PU_PLL)
		& (~PHY_40NM_CTRL_PU), base + PHY_40NM_CTRL);
}

#ifdef CONFIG_USB_GADGET_CHARGE_ONLY
void usb_phy_force_dp_dm(struct usb_phy *phy, bool is_force)
{
#if 0
	struct mv_usb2_phy *mv_phy = container_of(phy, struct mv_usb2_phy, phy);
	void __iomem *base = mv_phy->base;
	u16 reg16;

	/* chips that has otg do not need to force dp dm down */
	if (has_feat_force_dpdm()) {
		reg16 = readw(base + PHY_40NM_DIG1);
		if (is_force) {
			reg16 = (reg16 & (~PHY_40NM_DIG1_ARC_DPDM_MODE))
				| PHY_40NM_DIG1_DP_PULLDOWN
				| PHY_40NM_DIG1_DM_PULLDOWN;
		} else {
			reg16 = (reg16 | PHY_40NM_DIG1_ARC_DPDM_MODE)
				& (~PHY_40NM_DIG1_DP_PULLDOWN)
				& (~PHY_40NM_DIG1_DM_PULLDOWN);
		}

		writew(reg16, base + PHY_40NM_DIG1);
		pr_info("dp_dm force? %s, reg=%x\n",
				is_force ? "yes" : "no", reg16);
	}
#endif
}
#endif /* CONFIG_USB_GADGET_CHARGE_ONLY */

static void wait_for_usb_phy_ready(struct mv_usb2_phy *mv_phy)
{
	struct platform_device *pdev = mv_phy->pdev;
	unsigned int loops = 0;
	void __iomem *base = mv_phy->base;
	unsigned int tmp, val;

	/*  Calibration Timing
	*		   ____________________________
	*  CAL START   ___|
	*			   ____________________
	*  CAL_DONE    ___________|
	*		  | 400us |
	*/
	/* IMP Calibrate */
	writel(readl(base + PHY_28NM_CAL_REG) |
		1 << PHY_28NM_IMPCAL_START_SHIFT,
		base + PHY_28NM_CAL_REG);

	/* Wait For IMP Calibrate PHY */
	udelay(400);
	/* Make sure PHY Calibration is ready */
	loops = 0;
	do {
		tmp = readl(base + PHY_28NM_CAL_REG);
		val = PHY_28NM_PLL_IMPCAL_DONE_MASK;
		tmp &= val;
		if (tmp == val)
			break;
		udelay(1000);
	} while ((++loops) && (loops < 100));
	if (loops >= 100)
		dev_warn(&pdev->dev, "USB PHY IMP Calibrate not done after 100mS.");
	writel(readl(base + PHY_28NM_CAL_REG) &
		~(1 << PHY_28NM_IMPCAL_START_SHIFT),
		base + PHY_28NM_CAL_REG);

	/* PLL Calibrate */
	writel(readl(base + PHY_28NM_CAL_REG) |
		1 << PHY_28NM_PLLCAL_START_SHIFT,
		base + PHY_28NM_CAL_REG);

	/* Wait For PLL Calibrate PHY */
	udelay(400);
	loops = 0;
	do {
		tmp = readl(base + PHY_28NM_CAL_REG);
		val = PHY_28NM_PLL_PLLCAL_DONE_MASK;
		tmp &= val;
		if (tmp == val)
			break;
		udelay(1000);
	} while ((++loops) && (loops < 100));
	if (loops >= 100)
		dev_warn(&pdev->dev, "USB PHY PLL Calibrate not done after 100mS.");
	writel(readl(base + PHY_28NM_CAL_REG) &
		~(1 << PHY_28NM_PLLCAL_START_SHIFT),
		base + PHY_28NM_CAL_REG);

	/* SQ Calibrate */
	writel(readl(base + PHY_28NM_RX_REG1) |
		1 << PHY_28NM_RX_SQCAL_START_SHIFT,
		base + PHY_28NM_RX_REG1);
	/* Wait For Calibrate PHY */
	udelay(400);
	/* Make sure PHY RX SQ Calibration is ready */
	loops = 0;
	do {
		tmp = readl(base + PHY_28NM_RX_REG1);
		val = PHY_28NM_RX_SQCAL_DONE_MASK;
		tmp &= val;
		if (tmp == val)
			break;
		udelay(1000);
	} while ((++loops) && (loops < 100));
	if (loops >= 100)
		dev_warn(&pdev->dev, "USB PHY Calibrate not done after 100mS.");
	writel(readl(base + PHY_28NM_RX_REG1) &
		~(1 << PHY_28NM_RX_SQCAL_START_SHIFT),
		base + PHY_28NM_RX_REG1);

	/* Make sure PHY PLL is ready */
	loops = 0;
	tmp = readl(base + PHY_28NM_PLL_REG0);
	while (!(tmp & PHY_28NM_PLL_READY_MASK)) {
		udelay(1000);
		loops++;
		if (loops > 100) {
			dev_warn(&pdev->dev, "PLL_READY not set after 100mS.");
			break;
		}
	}
}
#ifdef CONFIG_USB_PHY_TUNE
static int _mv_usb2_phy_28nm_tune(struct mv_usb2_phy *mv_phy, bool state)
{
	void __iomem *base = mv_phy->base;
	unsigned int tmp, val;

	pr_info("usb:  %s state %s \n",
		__func__,(state) ? "Peripheral":"Host");
	pr_info("usb: %s original phy tx_reg0=0x%08x\n",
		__func__, readl(base + PHY_28NM_TX_REG0));
	pr_info("usb: %s original phy rx_reg0=0x%08x\n",
		__func__, readl(base + PHY_28NM_RX_REG0));
	pr_info("usb: %s original phy rx_reg1=0x%08x\n",
		__func__, readl(base + PHY_28NM_RX_REG1));

	if(state) {	/* for Peripheral */
		/* PHY_28NM_TX_REG0 */
		writel(readl(base + PHY_28NM_TX_REG0) &
			~(PHY_28NM_TX_AMP_MASK),
			base + PHY_28NM_TX_REG0);
		/*
		* 20150323 :
		* TX_AMP : pxa1936=0x6 / others=0x4
		*/
		if (cpu_is_pxa1936()) {
			writel(readl(base + PHY_28NM_TX_REG0) |
				(0x1 << PHY_28NM_TX_PU_BY_REG_SHIFT
				| 0x6 << PHY_28NM_TX_AMP_SHIFT
				| 0x1 << PHY_28NM_TX_PU_ANA_SHIFT),
				base + PHY_28NM_TX_REG0);
		} else {
			writel(readl(base + PHY_28NM_TX_REG0) |
				(0x1 << PHY_28NM_TX_PU_BY_REG_SHIFT
				| 0x4 << PHY_28NM_TX_AMP_SHIFT
				| 0x1 << PHY_28NM_TX_PU_ANA_SHIFT),
				base + PHY_28NM_TX_REG0);
		}

		/* PHY_28NM_RX_REG0 */
		/* 20150205:
		SQ_THRESH = 0x7
		SQ_ANA_DTC_SEL = 0x1
		*/
		writel(readl(base + PHY_28NM_RX_REG0) &
			~(PHY_28NM_RX_SQ_THRESH_MASK | PHY_28NM_RX_SQ_ANA_DTC_SEL_MASK),
			base + PHY_28NM_RX_REG0);
#if defined(CONFIG_MACH_DEGASVELTE) || defined(CONFIG_MACH_J1ACELTE) || defined(CONFIG_MACH_J1ACELTE_LTN)
		writel(readl(base + PHY_28NM_RX_REG0) |
			((0x9 << PHY_28NM_RX_SQ_THRESH_SHIFT) | (0x1 << PHY_28NM_RX_SQ_ANA_DTC_SEL_SHIFT)),
			base + PHY_28NM_RX_REG0);
#else
		writel(readl(base + PHY_28NM_RX_REG0) |
			((0x7 << PHY_28NM_RX_SQ_THRESH_SHIFT) | (0x1 << PHY_28NM_RX_SQ_ANA_DTC_SEL_SHIFT)),
			base + PHY_28NM_RX_REG0);
#endif
		/* PHY_28NM_RX_REG1 */
		/* 20150205 :
		EXT_SQ_AMP_CAL_EN = 0x1
		EXT_SQ_AMP_CAL = 0x1
		*/
		writel(readl(base + PHY_28NM_RX_REG1) &
			~(PHY_28NM_RX_EXT_SQ_AMP_CAL_EN_MASK | PHY_28NM_RX_EXT_SQ_AMP_CAL_MASK),
			base + PHY_28NM_RX_REG1);
		writel(readl(base + PHY_28NM_RX_REG1) |
			((0x1 << PHY_28NM_RX_EXT_SQ_AMP_CAL_EN_SHIFT) | (0x1 << PHY_28NM_RX_EXT_SQ_AMP_CAL_SHIFT)),
			base + PHY_28NM_RX_REG1);
	} else {	/*for Host*/
		/* PHY_28NM_TX_REG0 */
		writel(readl(base + PHY_28NM_TX_REG0) &
			~PHY_28NM_TX_AMP_MASK,
			base + PHY_28NM_TX_REG0);
		/*
		* 20150128 :
		* TX_AMP : 0x2
		* If TX_AMP value is 0, DegasVELTE can't connect to SD Cardy
		*/
		writel(readl(base + PHY_28NM_TX_REG0) |
			(0x1 << PHY_28NM_TX_PU_BY_REG_SHIFT
			| 0x2 << PHY_28NM_TX_AMP_SHIFT
			| 0x1 << PHY_28NM_TX_PU_ANA_SHIFT),
			base + PHY_28NM_TX_REG0);
		/* PHY_28NM_RX_REG0 */
		/* 20141210 :
		* SQ_THRESH = 0x4
		* SQ_ANA_DTC_SEL = 0x1
		* If RX SQ_Thresh value is 7, DegasVELTE can't connect to SD Cardy
		*/
		writel(readl(base + PHY_28NM_RX_REG0) &
			~(PHY_28NM_RX_SQ_THRESH_MASK | PHY_28NM_RX_SQ_ANA_DTC_SEL_MASK),
			base + PHY_28NM_RX_REG0);
		writel(readl(base + PHY_28NM_RX_REG0) |
			((0x4 << PHY_28NM_RX_SQ_THRESH_SHIFT) | (0x1 << PHY_28NM_RX_SQ_ANA_DTC_SEL_SHIFT)),
			base + PHY_28NM_RX_REG0);
		/* PHY_28NM_RX_REG1 */
		/* 20141210 :
		EXT_SQ_AMP_CAL_EN = 0x1
		EXT_SQ_AMP_CAL = 0x1
		*/
		writel(readl(base + PHY_28NM_RX_REG1) &
			~(PHY_28NM_RX_EXT_SQ_AMP_CAL_EN_MASK | PHY_28NM_RX_EXT_SQ_AMP_CAL_MASK),
			base + PHY_28NM_RX_REG1);
		writel(readl(base + PHY_28NM_RX_REG1) |
			((0x1 << PHY_28NM_RX_EXT_SQ_AMP_CAL_EN_SHIFT) | (0x1 << PHY_28NM_RX_EXT_SQ_AMP_CAL_SHIFT)),
			base + PHY_28NM_RX_REG1);
	}

	wait_for_usb_phy_ready(mv_phy);

	pr_info("usb: %s modified phy tx_reg0=0x%08x\n",
		__func__, readl(base + PHY_28NM_TX_REG0));
	pr_info("usb: %s modified phy rx_reg0=0x%08x\n",
		__func__, readl(base + PHY_28NM_RX_REG0));
	pr_info("usb: %s modified phy rx_reg1=0x%08x\n",
		__func__, readl(base + PHY_28NM_RX_REG1));

	return 0;
}
#endif

static int dip_status; // Current USB DIP status
static int dip_usb_status; // Current USB phy conf status
struct mutex dip_mutex;

void usb2_phy_dip_set(int on_off)
{
	void __iomem *base = mv_phy_ptr->base;
	int reg1, reg2;
	int tmp;

	mutex_lock(&dip_mutex);

	dip_status = on_off;

	/* if usb does not work, directly return */
	if ((mv_phy_ptr->phy).refcount == 0)
	{
		dip_usb_status = 0;
		mutex_unlock(&dip_mutex);
		return;
	}

	if(on_off == dip_usb_status)
	{
		mutex_unlock(&dip_mutex);
		return;
	}

	tmp = readl(base + PHY_28NM_PLL_REG0) &
			~(PHY_28NM_PLL_SELLPFR_MASK | PHY_28NM_PLL_ICP_MASK);

	reg1 = tmp | (0x3 << PHY_28NM_PLL_SELLPFR_SHIFT | 0x7 << PHY_28NM_PLL_ICP_SHIFT);
	reg2 = tmp | (0x1 << PHY_28NM_PLL_SELLPFR_SHIFT | 0x3 << PHY_28NM_PLL_ICP_SHIFT);

	if(on_off && !dip_usb_status)
		/* PHY_28NM_PLL_REG0 */
	{
		printk("usb: %s: PLL custom on\n", __func__);
		writel(reg1, base + PHY_28NM_PLL_REG0);
	}
	else if (!on_off && dip_usb_status)
	{
		printk("usb: %s: PLL custom off\n", __func__);
		writel(reg2, base + PHY_28NM_PLL_REG0);
	}
	else
		printk("usb: %s: Already DIP set off, do nothing\n", __func__);

	dip_usb_status = on_off;
	mutex_unlock(&dip_mutex);

	return;
}
EXPORT_SYMBOL(usb2_phy_dip_set);

static int _mv_usb2_phy_28nm_init(struct mv_usb2_phy *mv_phy)
{
	void __iomem *base = mv_phy->base;

	/* PHY_28NM_CTRL_REG0 */
	if (cpu_is_pxa1908() || cpu_is_pxa1936()) {
		writel(readl(base + PHY_28NM_CTRL_REG0) |
			((0x3) << PHY_28NM_CTRL_REG0_SHIFT),
			base + PHY_28NM_CTRL_REG0);
	}

	/* PHY_28NM_PLL_REG0 */
	writel(readl(base + PHY_28NM_PLL_REG0) &
		~(PHY_28NM_PLL_SELLPFR_MASK
		| PHY_28NM_PLL_FBDIV_MASK
		| PHY_28NM_PLL_ICP_MASK
		| PHY_28NM_PLL_REFDIV_MASK),
		base + PHY_28NM_PLL_REG0);

	writel(readl(base + PHY_28NM_PLL_REG0) |
		(0x1 << PHY_28NM_PLL_SELLPFR_SHIFT
		| 0xf0 << PHY_28NM_PLL_FBDIV_SHIFT
		| 0x3 << PHY_28NM_PLL_ICP_SHIFT
		| 0xd << PHY_28NM_PLL_REFDIV_SHIFT),
		base + PHY_28NM_PLL_REG0);


	/* PHY_28NM_PLL_REG1 */
	writel(readl(base + PHY_28NM_PLL_REG1) &
		~(PHY_28NM_PLL_PU_PLL_MASK
		| PHY_28NM_PLL_PU_BY_REG_MASK),
		base + PHY_28NM_PLL_REG1);

	writel(readl(base + PHY_28NM_PLL_REG1) |
		(0x1 << PHY_28NM_PLL_PU_PLL_SHIFT
		| 0x1 << PHY_28NM_PLL_PU_BY_REG_SHIFT),
		base + PHY_28NM_PLL_REG1);


	/* PHY_28NM_TX_REG0 */
	writel(readl(base + PHY_28NM_TX_REG0) &
		~PHY_28NM_TX_AMP_MASK,
		base + PHY_28NM_TX_REG0);
#ifndef CONFIG_USB_PHY_TUNE
	/*
	 * 20150323 :
	 * TX_AMP : pxa1936=0x6 / others=0x4
	 */
	if (cpu_is_pxa1936()) {
		writel(readl(base + PHY_28NM_TX_REG0) |
			(0x1 << PHY_28NM_TX_PU_BY_REG_SHIFT
			| 0x6 << PHY_28NM_TX_AMP_SHIFT
			| 0x1 << PHY_28NM_TX_PU_ANA_SHIFT),
			base + PHY_28NM_TX_REG0);
	} else {
		writel(readl(base + PHY_28NM_TX_REG0) |
			(0x1 << PHY_28NM_TX_PU_BY_REG_SHIFT
			| 0x4 << PHY_28NM_TX_AMP_SHIFT
			| 0x1 << PHY_28NM_TX_PU_ANA_SHIFT),
			base + PHY_28NM_TX_REG0);
	}

	/* PHY_28NM_RX_REG0 */
	writel(readl(base + PHY_28NM_RX_REG0) &
		~(PHY_28NM_RX_SQ_THRESH_MASK | PHY_28NM_RX_SQ_ANA_DTC_SEL_MASK),
		base + PHY_28NM_RX_REG0);

	writel(readl(base + PHY_28NM_RX_REG0) |
		((0x7 << PHY_28NM_RX_SQ_THRESH_SHIFT) | (0x1 << PHY_28NM_RX_SQ_ANA_DTC_SEL_SHIFT)),
		base + PHY_28NM_RX_REG0);

	/* PHY_28NM_RX_REG1 */
	writel(readl(base + PHY_28NM_RX_REG1) &
		~(PHY_28NM_RX_EXT_SQ_AMP_CAL_EN_MASK | PHY_28NM_RX_EXT_SQ_AMP_CAL_MASK),
		base + PHY_28NM_RX_REG1);

	writel(readl(base + PHY_28NM_RX_REG1) |
		((0x1 << PHY_28NM_RX_EXT_SQ_AMP_CAL_EN_SHIFT) | (0x1 << PHY_28NM_RX_EXT_SQ_AMP_CAL_SHIFT)),
		base + PHY_28NM_RX_REG1);
#endif
	/* PHY_28NM_DIG_REG0 */
	writel(readl(base + PHY_28NM_DIG_REG0) &
		~(PHY_28NM_DIG_BITSTAFFING_ERR_MASK
		| PHY_28NM_DIG_SYNC_ERR_MASK
		| PHY_28NM_DIG_SQ_FILT_MASK
		| PHY_28NM_DIG_SQ_BLK_MASK
		| PHY_28NM_DIG_SYNC_NUM_MASK),
		base + PHY_28NM_DIG_REG0);

	/* PHY_28NM_CTRL_REG0 */
	/* 20141118 :
	VBUSVALID_CTL_ = 0x3
	*/
	writel(readl(base + PHY_28NM_CTRL_REG0) &
		~PHY_28NM_CTRL0_VBUSVALID_CTL_MASK,
		base + PHY_28NM_CTRL_REG0);

	writel(readl(base + PHY_28NM_CTRL_REG0) |
		0x3 << PHY_28NM_CTRL0_VBUSVALID_CTL_SHIFT,
		base + PHY_28NM_CTRL_REG0);

	if (cpu_is_pxa1928_b0() || cpu_is_pxa1908() || cpu_is_pxa1936()) {
		writel(readl(base + PHY_28NM_DIG_REG0) |
			(0x0 << PHY_28NM_DIG_SQ_FILT_SHIFT
			| 0x0 << PHY_28NM_DIG_SQ_BLK_SHIFT
			| 0x1 << PHY_28NM_DIG_SYNC_NUM_SHIFT),
			base + PHY_28NM_DIG_REG0);
	} else {
		writel(readl(base + PHY_28NM_DIG_REG0) |
			(0x7 << PHY_28NM_DIG_SQ_FILT_SHIFT
			| 0x4 << PHY_28NM_DIG_SQ_BLK_SHIFT
			| 0x2 << PHY_28NM_DIG_SYNC_NUM_SHIFT),
			base + PHY_28NM_DIG_REG0);
	}

	if (mv_phy->drv_data.phy_flag & MV_PHY_FLAG_PLL_LOCK_BYPASS)
		writel(readl(base + PHY_28NM_DIG_REG0)
			| (0x1 << PHY_28NM_PLL_LOCK_BYPASS_SHIFT),
			base + PHY_28NM_DIG_REG0);

	/* PHY_28NM_OTG_REG */
	writel(readl(base + PHY_28NM_OTG_REG) |
		0x1 << PHY_28NM_OTG_PU_OTG_SHIFT,
		base + PHY_28NM_OTG_REG);


	writel(readl(base + PHY_28NM_OTG_REG) &
		~(0x1 << PHY_28NM_OTG_CONTROL_BY_PIN_SHIFT),
		base + PHY_28NM_OTG_REG);

	wait_for_usb_phy_ready(mv_phy);

	mv_phy_ptr = mv_phy;

	/* Config dip channel related by previous dip request */
	if (dip_status)
	{
		usb2_phy_dip_set(0);
		usb2_phy_dip_set(1);
	}
	return 0;
}

static void _mv_usb2_phy_28nm_shutdown(struct mv_usb2_phy *mv_phy)
{
	void __iomem *base = mv_phy->base;
	unsigned int val;

	val = readw(base + PHY_28NM_PLL_REG1);
	val &= ~PHY_28NM_PLL_PU_PLL_MASK;
	writew(val, base + PHY_28NM_PLL_REG1);

	/* power down PHY Analog part */
	val = readw(base + PHY_28NM_TX_REG0);
	val |= (0x1 << PHY_28NM_TX_PU_ANA_SHIFT) | (0x1 << PHY_28NM_TX_PU_BY_REG_SHIFT);
	writew(val, base + PHY_28NM_TX_REG0);

	/* power down PHY OTG part */
	val = readw(base + PHY_28NM_OTG_REG);
	val &= ~(0x1 << PHY_28NM_OTG_PU_OTG_SHIFT);
	writew(val, base + PHY_28NM_OTG_REG);

	/* PHY_28NM_CTRL_REG0 */
	if (cpu_is_pxa1908() || cpu_is_pxa1936()) {
		val = readl(base + PHY_28NM_CTRL_REG0);
		val &= ~((0x3) << PHY_28NM_CTRL_REG0_SHIFT);
		writew(val, base + PHY_28NM_CTRL_REG0);
	}
}

#ifdef CONFIG_USB_PHY_TUNE
static int mv_usb2_phy_tune(struct usb_phy *phy, bool state)
{
	struct mv_usb2_phy *mv_phy = container_of(phy, struct mv_usb2_phy, phy);

	switch (mv_phy->drv_data.phy_type) {
	case PHY_28LP:
		_mv_usb2_phy_28nm_tune(mv_phy, state);
		break;
	default:
		pr_err("No phy type tuning supported!\n");
		break;
	}

	return 0;
}
#endif

static int mv_usb2_phy_init(struct usb_phy *phy)
{
	struct mv_usb2_phy *mv_phy = container_of(phy, struct mv_usb2_phy, phy);

	clk_enable(mv_phy->clk);
	switch (mv_phy->drv_data.phy_type) {
	case PHY_40LP:
		_mv_usb2_phy_40nm_init(mv_phy);
		break;
	case PHY_28LP:
		_mv_usb2_phy_28nm_init(mv_phy);
		break;
	case PHY_55LP:
		_mv_usb2_phy_55nm_init(mv_phy);
		break;
	default:
		pr_err("No such phy type supported!\n");
		break;
	}

	return 0;
}

static void mv_usb2_phy_shutdown(struct usb_phy *phy)
{
	struct mv_usb2_phy *mv_phy = container_of(phy, struct mv_usb2_phy, phy);

	switch (mv_phy->drv_data.phy_type) {
	case PHY_40LP:
		_mv_usb2_phy_40nm_shutdown(mv_phy);
		break;
	case PHY_28LP:
		_mv_usb2_phy_28nm_shutdown(mv_phy);
		break;
	case PHY_55LP:
		_mv_usb2_phy_55nm_shutdown(mv_phy);
		break;
	default:
		pr_err("No such phy type supported!\n");
		break;
	}

	clk_disable(mv_phy->clk);
}

/*
 * can only detect DEFAULT_CHARGER, DCP_CHARGER, SDP_CHARGER
 * for other charger types, need to be confirmed by USB enumuration
 */
static int _mv_usb2_phy_40nm_charger_detect(struct mv_usb2_phy *mv_phy)
{
	void __iomem *base = mv_phy->base;
	int charger_type_bc12 = NULL_CHARGER;
	u16 reg16;

	/* debounce time for slowly insering USB cable */
	msleep(80);

	/*set PU_ANA*/
	reg16 = readw(base + PHY_40NM_ANA1);
	reg16 |= PHY_40NM_ANA1_PU_ANA;
	writew(reg16, base + PHY_40NM_ANA1);

	/* power up the charger detector */
	reg16 = readw(base + PHY_40NM_CHARGE0);
	reg16 |= PHY_40NM_CHARGE0_ENABLE_SWITCH |
		PHY_40NM_CHARGE0_PU_CHRG_DTC;
	writew(reg16, base + PHY_40NM_CHARGE0);
	usleep_range(2, 5);

	/*
	 * primary detect:
	 * to detect if there is a charger at upstream port
	 */
	reg16 = readw(base + PHY_40NM_RESERVE0);
	reg16 &= ~(PHY_40NM_CDP_EN | PHY_40NM_DCP_EN |
			PHY_40NM_CDP_DM_AUTO_SWITCH |
			PHY_40NM_DP_DM_SWAP_CTRL);
	writew(reg16, base + PHY_40NM_RESERVE0);

	reg16 = readw(base + PHY_40NM_RESERVE0);
	reg16 |= (PHY_40NM_PD_EN | PHY_40NM_ENABLE_SWITCH_DM |
			PHY_40NM_ENABLE_SWITCH_DP |
			PHY_40NM_VDAT_CHARGE |
			PHY_40NM_VSRC_CHARGE);
	writew(reg16, base + PHY_40NM_RESERVE0);
	/* wait 20 us */
	usleep_range(20, 30);

	/* read charger detector output register */
	if (readl(base + PHY_40NM_CTRL) & PHY_40NM_CTRL_AVALID_CONTROL)
		reg16 = readw(base + PHY_40NM_STATUS_A0C);
	else
		reg16 = readw(base + PHY_40NM_STATUS);
	if (reg16 & PHY_40NM_CHARGER_STATUS_MASK) {
		/*
		 * secondary detect:
		 * detect if the upstream port is a dedicated
		 * charger or a USB2 host port
		 */
		reg16 = readw(base + PHY_40NM_RESERVE0);
		reg16 |= PHY_40NM_DP_DM_SWAP_CTRL;
		writew(reg16, base + PHY_40NM_RESERVE0);
		/* wait 20us */
		usleep_range(20, 30);

		/* read charger detector output register */
		if (readl(base + PHY_40NM_CTRL)
		   & PHY_40NM_CTRL_AVALID_CONTROL)
			reg16 = readw(base +
					PHY_40NM_STATUS_A0C);
		else
			reg16 = readw(base +
					PHY_40NM_STATUS);
		if (reg16 & PHY_40NM_CHARGER_STATUS_MASK)
			charger_type_bc12 = DCP_CHARGER;
		else
			charger_type_bc12 = CDP_CHARGER;
	} else
		charger_type_bc12 = DEFAULT_CHARGER;

	/*disable switch, shutdown the charger detector*/
	reg16 = readw(base + PHY_40NM_CHARGE0);
	reg16 &= ~(PHY_40NM_CHARGE0_ENABLE_SWITCH |
			PHY_40NM_CHARGE0_PU_CHRG_DTC);
	writew(reg16, base + PHY_40NM_CHARGE0);
	return charger_type_bc12;
}

static int _mv_usb2_phy_28nm_charger_detect(struct mv_usb2_phy *mv_phy)
{
	void __iomem *base = mv_phy->base;
	int charger_type_bc12 = NULL_CHARGER;
	u32 reg32;

	/* Power up Analog */
	reg32 = readl(base + PHY_28NM_TX_REG0);
	reg32 |= (1 << PHY_28NM_TX_PU_BY_REG_SHIFT |
			1 << PHY_28NM_TX_PU_ANA_SHIFT);
	writel(reg32, base + PHY_28NM_TX_REG0);

	/* Power up Charger Detector */
	reg32 = readl(base + PHY_28NM_CHRG_DET);
	reg32 |= (1 << PHY_28NM_CHGDTC_PU_CHRG_DTC_SHIFT_28);
	writel(reg32, base + PHY_28NM_CHRG_DET);

	usleep_range(2, 5);

	/* Primary detection */
	reg32 = readl(base + PHY_28NM_CHRG_DET);
	reg32 &= ~(1  <<  PHY_28NM_CHGDTC_DP_DM_SWAP_SHIFT_28 |
			3 << PHY_28NM_CHGDTC_VSRC_CHARGE_SHIFT_28 |
			3 << PHY_28NM_CHGDTC_VDAT_CHARGE_SHIFT_28 |
			1 << PHY_28NM_CHGDTC_CDP_DM_AUTO_SWITCH_SHIFT_28 |
			1 << PHY_28NM_CHGDTC_DCP_EN_SHIFT_28 |
			1 << PHY_28NM_CHGDTC_CDP_EN_SHIFT_28);
	writel(reg32, base + PHY_28NM_CHRG_DET);

	reg32 = readl(base + PHY_28NM_CHRG_DET);
	reg32 |= (1 << PHY_28NM_CHGDTC_VSRC_CHARGE_SHIFT_28 |
			1 << PHY_28NM_CHGDTC_VDAT_CHARGE_SHIFT_28 |
			1 << PHY_28NM_CHGDTC_PD_EN_SHIFT_28);
	writel(reg32, base + PHY_28NM_CHRG_DET);

	/* Enable swtich DM/DP */
	reg32 = readl(base + PHY_28NM_CHRG_DET);
	reg32 |= (1 << PHY_28NM_CHGDTC_ENABLE_SWITCH_DM_SHIFT_28 |
			1 << PHY_28NM_CHGDTC_ENABLE_SWITCH_DP_SHIFT_28);
	writel(reg32, base + PHY_28NM_CHRG_DET);

	if ((readl(base + PHY_28NM_CTRL_REG1)) &
		(1 << PHY_28NM_CTRL1_CHRG_DTC_OUT_SHIFT_28)) {
		/* We have CHRG_DTC_OUT set.
		 * Now proceed with Secondary Detection
		 */

		msleep(60);

		reg32 = readl(base + PHY_28NM_CHRG_DET);
		reg32 |= (1 << PHY_28NM_CHGDTC_DP_DM_SWAP_SHIFT_28);
		writel(reg32, base + PHY_28NM_CHRG_DET);

		msleep(80);

		if ((readl(base + PHY_28NM_CTRL_REG1)) &
			(1 << PHY_28NM_CTRL1_CHRG_DTC_OUT_SHIFT_28))
			charger_type_bc12 = DCP_CHARGER;
		else
			charger_type_bc12 = CDP_CHARGER;
	} else
		charger_type_bc12 = DEFAULT_CHARGER;

	/* Disable swtich DM/DP */
	reg32 = readl(base + PHY_28NM_CHRG_DET);
	reg32 &= ~(1<<PHY_28NM_CHGDTC_ENABLE_SWITCH_DM_SHIFT_28 |
			1 << PHY_28NM_CHGDTC_ENABLE_SWITCH_DP_SHIFT_28);
	writel(reg32, base + PHY_28NM_CHRG_DET);

	/* Power down Charger Detector */
	reg32 = readl(base + PHY_28NM_CHRG_DET);
	reg32 &= ~(1 << PHY_28NM_CHGDTC_PU_CHRG_DTC_SHIFT_28);
	writel(reg32, base + PHY_28NM_CHRG_DET);

	return charger_type_bc12;
}

static int mv_usb2_phy_charger_detect(struct usb_phy *phy)
{
	struct mv_usb2_phy *mv_phy = container_of(phy, struct mv_usb2_phy, phy);
	int ret;

	switch (mv_phy->drv_data.phy_type) {
	case PHY_40LP:
		ret = _mv_usb2_phy_40nm_charger_detect(mv_phy);
		break;
	case PHY_28LP:
		ret = _mv_usb2_phy_28nm_charger_detect(mv_phy);
		break;
	default:
		pr_err("Such phy does not support to detect charger type!\n");
		ret = DEFAULT_CHARGER;
		break;
	}
#ifdef CONFIG_USB_NOTIFY_LAYER
	ret = SDP_CHARGER;
#endif
	return ret;
}

static const struct of_device_id mv_usbphy_dt_match[];

static int mv_usb2_get_phydata(struct platform_device *pdev,
				struct mv_usb2_phy *mv_phy)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	u32 phy_rev;

	match = of_match_device(mv_usbphy_dt_match, &pdev->dev);
	if (!match)
		return -ENODEV;

	mv_phy->drv_data.phy_type = (unsigned long)match->data;

	if (!of_property_read_u32(np, "marvell,usb2-phy-rev", &phy_rev))
		mv_phy->drv_data.phy_rev = phy_rev;
	else
		pr_info("No PHY revision found, use the default setting!");

	if (of_property_read_bool(np, "marvell,pll-lock-bypass"))
		mv_phy->drv_data.phy_flag |= MV_PHY_FLAG_PLL_LOCK_BYPASS;

	return 0;
}

static void mv_usb2_phy_bind_device(struct mv_usb2_phy *mv_phy)
{
	const char *device_name;

	struct device_node *np = (mv_phy->phy.dev)->of_node;

	if (!of_property_read_string(np, "marvell,udc-name", &device_name))
		usb_bind_phy(device_name, MV_USB2_PHY_INDEX,
						dev_name(mv_phy->phy.dev));

	if (!of_property_read_string(np, "marvell,ehci-name", &device_name))
		usb_bind_phy(device_name, MV_USB2_PHY_INDEX,
						dev_name(mv_phy->phy.dev));

	if (!of_property_read_string(np, "marvell,otg-name", &device_name))
		usb_bind_phy(device_name, MV_USB2_PHY_INDEX,
						dev_name(mv_phy->phy.dev));
}

static int mv_usb2_phy_probe(struct platform_device *pdev)
{
	struct mv_usb2_phy *mv_phy;
	struct resource *r;
	int ret = 0;
	struct device_node *np = pdev->dev.of_node;

	mutex_init(&dip_mutex);
	mv_phy = devm_kzalloc(&pdev->dev, sizeof(*mv_phy), GFP_KERNEL);
	if (mv_phy == NULL) {
		dev_err(&pdev->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	of_property_read_string(np, "marvell,udc-name",
			&((pdev->dev).init_name));
	mv_phy->pdev = pdev;

	ret = mv_usb2_get_phydata(pdev, mv_phy);
	if (ret) {
		dev_err(&pdev->dev, "No matching phy founded\n");
		return ret;
	}

	mv_phy->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(mv_phy->clk)) {
		dev_err(&pdev->dev, "failed to get clock.\n");
		return PTR_ERR(mv_phy->clk);
	}
	clk_prepare(mv_phy->clk);

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (r == NULL) {
		dev_err(&pdev->dev, "no phy I/O memory resource defined\n");
		return -ENODEV;
	}
	mv_phy->base = devm_ioremap_resource(&pdev->dev, r);
	if (mv_phy->base == NULL) {
		dev_err(&pdev->dev, "error map register base\n");
		return -EBUSY;
	}

	mv_phy->phy.dev = &pdev->dev;
	mv_phy->phy.label = "mv-usb2";
	mv_phy->phy.type = USB_PHY_TYPE_USB2;
	mv_phy->phy.init = mv_usb2_phy_init;
	mv_phy->phy.shutdown = mv_usb2_phy_shutdown;
	mv_phy->phy.charger_detect = mv_usb2_phy_charger_detect;
#ifdef CONFIG_USB_PHY_TUNE
	mv_phy->phy.tune = mv_usb2_phy_tune;
#endif

	mv_usb2_phy_bind_device(mv_phy);

	usb_add_phy_dev(&mv_phy->phy);

	platform_set_drvdata(pdev, mv_phy);

	return 0;
}

static int mv_usb2_phy_remove(struct platform_device *pdev)
{
	struct mv_usb2_phy *mv_phy = platform_get_drvdata(pdev);

	usb_remove_phy(&mv_phy->phy);

	clk_unprepare(mv_phy->clk);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id mv_usbphy_dt_match[] = {
	{ .compatible = "marvell,usb2-phy-55lp", .data = (void *)PHY_55LP },
	{ .compatible = "marvell,usb2-phy-40lp", .data = (void *)PHY_40LP },
	{ .compatible = "marvell,usb2-phy-28lp", .data = (void *)PHY_28LP },
	{},
};
MODULE_DEVICE_TABLE(of, mv_usbphy_dt_match);

static struct platform_driver mv_usb2_phy_driver = {
	.probe	= mv_usb2_phy_probe,
	.remove = mv_usb2_phy_remove,
	.driver = {
		.name   = "mv-usb2-phy",
		.owner  = THIS_MODULE,
		.of_match_table = of_match_ptr(mv_usbphy_dt_match),
	},
};

module_platform_driver(mv_usb2_phy_driver);
MODULE_ALIAS("platform: mv_usb2");
MODULE_AUTHOR("Marvell Inc.");
MODULE_DESCRIPTION("Marvell USB2 phy driver");
MODULE_LICENSE("GPL v2");
