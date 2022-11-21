/*
 * Copyright (C) 2011 Marvell International Ltd. All rights reserved.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __ASM_ARCH_REGS_USB_H
#define __ASM_ARCH_REGS_USB_H

#ifdef CONFIG_CPU_PXA988
/* USB DEVICE REGISTER BASE ADDRESS */
#define PXA988_UDC_REGBASE		(0xd4208000)
#define PXA988_UDC_CAPREGS_RANGE	(0x100)
#define PXA988_UDC_REG_RANGE		(0x1ff)

/* USB PHY REGISTER BASE ADDRESS */
#define PXA988_UDC_PHYBASE	(0xd4207000)
#define PXA988_UDC_PHY_RANGE	(0xff)

/*
 * PXA988 USB DEVICE PHY(UTMI) Registers
 */
struct pxa988_usb_phy {
	u16 utmi_id;		/* 0x00 */
	u16 pad0;
	u16 utmi_pll_reg0;	/* 0x04 */
#define PLLVDD18(x)		((x & 0x3) << 14)
#define REFDIV(x)		((x & 0x1f) << 9)
#define FBDIV(x)		(x & 0x1ff)
	u16 pad1;
	u16 utmi_pll_reg1;	/* 0x08 */
#define PLL_READY		(0x1 << 15)
#define PLL_CONTROL_BY_PIN	(0x1 << 14)
#define PU_PLL			(0x1 << 13)
#define PLL_LOCK_BYPASS		(0x1 << 12)
#define DLL_RESET_BLK		(0x1 << 11)
#define ICP(x)			((x & 0x7) << 8)
#define KVCO_EXT		(0x1 << 7)
#define KVCO(x)			((x & 0x7) << 4)
#define CLK_BLK_EN		(0x1 << 3)
#define VCOCAL_START		(0x1 << 2)
#define PLLCAL12(x)		(x & 0x3)
	u16 pad2;
	u32 rsvd0;		/* 0x0c */
	u16 utmi_tx_reg0;	/* 0x10 */
#define TXDATA_BLK_EN		(0x1 << 14)
#define RCAL_START		(0x1 << 13)
#define EXT_HS_RCAL_EN		(0x1 << 12)
#define EXT_FS_RCAL_EN		(0x1 << 11)
#define IMPCAL_VTH(x)		((x & 0x7) << 8)
#define EXT_HS_RCAL(x)		((x & 0xf) << 4)
#define EXT_FS_RCAL(x)		(x & 0xf)
	u16 pad3;
	u16 utmi_tx_reg1;	/* 0x14 */
#define TXVDD15(x)		((x & 0x3) << 10)
#define TXVDD12(x)		((x & 0x3) << 8)
#define LOWVDD_EN		(0x1 << 7)
#define AMP(x)			((x & 0x7) << 4)
#define CK60_PHSEL(x)		(x & 0xf)
	u16 pad4;
	u16 utmi_tx_reg2;	/* 0x18 */
#define DRV_SLEWRATE(x)		((x & 0x3) << 10)
#define IMP_CAL_DLY(x)		((x & 0x3) << 8)
#define FSDRV_EN(x)		((x & 0xf) << 4)
#define HSDEV_EN(x)		(x & 0xf)
	u16 pad5;
	u32 rsvd1;		/* 0x1c */
	u16 utmi_rx_reg0;	/* 0x20 */
#define PHASE_FREEZE_DLY	(0x1 << 15)
#define USQ_LENGTH		(0x1 << 14)
#define ACQ_LENGTH(x)		((x & 0x3) << 12)
#define SQ_LENGTH(x)		((x & 0x3) << 10)
#define DISCON_THRESH(x)	((x & 0x3) << 8)
#define SQ_THRESH(x)		((x & 0xf) << 4)
#define LPF_COEF(x)		((x & 0x3) << 2)
#define INTPI(x)		(x & 0x3)
	u16 pad6;
	u16 utmi_rx_reg1;	/* 0x24 */
#define EARLY_VOS_ON_EN		(0x1 << 13)
#define RXDATA_BLOCK_EN		(0x1 << 12)
#define EDGE_DET_EN		(0x1 << 11)
#define CAP_SEL(x)		((x & 0x7) << 8)
#define RXDATA_BLOCK_LENGTH(x)	((x & 0x3) << 6)
#define EDGE_DET_SEL(x)		((x & 0x3) << 4)
#define CDR_COEF_SEL		(0x1 << 3)
#define CDR_FASTLOCK_EN		(0x1 << 2)
#define S2TO3_DLY_SEL(x)	(x & 0x3)
	u16 pad7;
	u16 utmi_rx_reg2;	/* 0x28 */
#define USQ_FILTER		(0x1 << 8)
#define SQ_CM_SEL		(0x1 << 7)
#define SAMPLER_CTRL		(0x1 << 6)
#define SQ_BUFFER_EN		(0x1 << 5)
#define SQ_ALWAYS_ON		(0x1 << 4)
#define RXVDD18(x)		((x & 0x3) << 2)
#define RXVDD12(x)		(x & 0x3)
	u16 pad8;
	u32 rsvd2;		/* 0x2c */
	u16 utmi_ana_reg0;	/* 0x30 */
#define BG_VSEL(x)		((x & 0x3) << 8)
#define DIG_SEL(x)		((x & 0x3) << 6)
#define TOPVDD18(x)		((x & 0x3) << 4)
#define VDD_USB2_DIG_TOP_SEL	(0x1 << 3)
#define IPTAT_SEL(x)		(x & 0x7)
	u16 pad9;
	u16 utmi_ana_reg1;	/* 0x34 */
#define PU_ANA			(0x1 << 14)
#define ANA_CONTROL_BY_PIN	(0x1 << 13)
#define SEL_LPFR		(0x1 << 12)
#define V2I_EXT			(0x1 << 11)
#define V2I(x)			((x & 0x7) << 8)
#define R_ROTATE_SEL		(0x1 << 7)
#define STRESS_TEST_MODE	(0x1 << 6)
#define TESTMON_ANA(x)		(x & 0x3f)
	u16 pad10;
	u32 rsvd3;		/* 0x38 */
	u16 utmi_dig_reg0;	/* 0x3c */
#define FIFO_UF			(0x1 << 15)
#define FIFO_OV			(0x1 << 14)
#define FS_EOP_MODE		(0x1 << 13)
#define HOST_DISCON_SEL1	(0x1 << 12)
#define HOST_DISCON_SEL0	(0x1 << 11)
#define FORCE_END_EN		(0x1 << 10)
#define EARLY_TX_EN		(0x1 << 9)
#define SYNCDET_WINDOW_EN	(0x1 << 8)
#define CLK_SUSPEND_EN		(0x1 << 7)
#define HS_DRIBBLE_EN		(0x1 << 6)
#define SYNC_NUM(x)		((x & 0x3) << 4)
#define FIFO_FILL_NUM(x)	(x & 0xf)
	u16 pad11;
	u16 utmi_dig_reg1;	/* 0x40 */
#define FS_RX_ERROR_MODE2	(0x1 << 15)
#define FS_RX_ERROR_MODE1	(0x1 << 14)
#define FS_RX_ERROR_MODE	(0x1 << 13)
#define CLK_OUT_SEL		(0x1 << 12)
#define EXT_TX_CLK_SEL		(0x1 << 11)
#define ARC_DPDM_MODE		(0x1 << 10)
#define DP_PULLDOWN		(0x1 << 9)
#define DM_PULLDOWN		(0x1 << 8)
#define SYNC_IGNORE_SQ		(0x1 << 7)
#define SQ_RST_RX		(0x1 << 6)
#define MON_SEL(x)		(x & 0x3f)
	u16 pad12;
	u16 utmi_dig_reg2;	/* 0x44 */
#define PAD_STRENGTH(x)		((x & 0x1f) << 8)
#define LONG_EOP		(0x1 << 5)
#define NOVBUS_DPDM00		(0x1 << 4)
#define ALIGN_FS_OUTEN		(0x1 << 2)
#define HS_HDL_SYNC		(0x1 << 1)
#define FS_HDL_OPMD		(0x1 << 0)
	u16 pad13;
	u32 rsvd4;		/* 0x48 */
	u16 utmi_test_reg0;	/* 0x4c */
	u16 pad14;
	u16 utmi_test_reg1;	/* 0x50 */
	u16 pad15;
	u32 rsvd5;		/* 0x54 */
	u16 utmi_charger_reg0;	/* 0x58 */
#define ENABLE_SWITCH		(0x1 << 3)
#define PU_CHRG_DTC		(0x1 << 2)
#define TESTMON_CHRGDTC(x)	(x & 0x3)
	u16 pad16;
	u16 utmi_otg_reg;	/* 0x5c */
	u16 pad17;
	u16 utmi_phy_mon0;	/* 0x60 */
	u16 pad18;
	u16 utmi_reserve_reg0;	/* 0x64 */
	u16 pad19;
};
#else /* CONFIG_CPU_PXA988 */

#define PXA168_U2O_REGBASE	(0xd4208000)
#define PXA168_U2O_PHYBASE	(0xd4207000)

#define PXA168_U2H_REGBASE      (0xd4209000)
#define PXA168_U2H_PHYBASE      (0xd4206000)

#define MMP3_HSIC1_REGBASE	(0xf0001000)
#define MMP3_HSIC1_PHYBASE	(0xf0001800)

#define MMP3_HSIC2_REGBASE	(0xf0002000)
#define MMP3_HSIC2_PHYBASE	(0xf0002800)

#define MMP3_FSIC_REGBASE	(0xf0003000)
#define MMP3_FSIC_PHYBASE	(0xf0003800)


#define USB_REG_RANGE		(0x1ff)
#define USB_PHY_RANGE		(0xff)

/* registers */
#define U2x_CAPREGS_OFFSET       0x100

/* phy regs */
#define UTMI_REVISION		0x0
#define UTMI_CTRL		0x4
#define UTMI_PLL		0x8
#define UTMI_TX			0xc
#define UTMI_RX			0x10
#define UTMI_IVREF		0x14
#define UTMI_T0			0x18
#define UTMI_T1			0x1c
#define UTMI_T2			0x20
#define UTMI_T3			0x24
#define UTMI_T4			0x28
#define UTMI_T5			0x2c
#define UTMI_RESERVE		0x30
#define UTMI_USB_INT		0x34
#define UTMI_DBG_CTL		0x38
#define UTMI_OTG_ADDON		0x3c

/* For UTMICTRL Register */
#define UTMI_CTRL_USB_CLK_EN                    (1 << 31)
/* pxa168 */
#define UTMI_CTRL_SUSPEND_SET1                  (1 << 30)
#define UTMI_CTRL_SUSPEND_SET2                  (1 << 29)
#define UTMI_CTRL_RXBUF_PDWN                    (1 << 24)
#define UTMI_CTRL_TXBUF_PDWN                    (1 << 11)

#define UTMI_CTRL_INPKT_DELAY_SHIFT             30
#define UTMI_CTRL_INPKT_DELAY_SOF_SHIFT		28
#define UTMI_CTRL_PU_REF_SHIFT			20
#define UTMI_CTRL_ARC_PULLDN_SHIFT              12
#define UTMI_CTRL_PLL_PWR_UP_SHIFT              1
#define UTMI_CTRL_PWR_UP_SHIFT                  0

/* For UTMI_PLL Register */
#define UTMI_PLL_PLLCALI12_SHIFT		29
#define UTMI_PLL_PLLCALI12_MASK			(0x3 << 29)

#define UTMI_PLL_PLLVDD18_SHIFT			27
#define UTMI_PLL_PLLVDD18_MASK			(0x3 << 27)

#define UTMI_PLL_PLLVDD12_SHIFT			25
#define UTMI_PLL_PLLVDD12_MASK			(0x3 << 25)

#define UTMI_PLL_CLK_BLK_EN_SHIFT               24
#define CLK_BLK_EN                              (0x1 << 24)
#define PLL_READY                               (0x1 << 23)
#define KVCO_EXT                                (0x1 << 22)
#define VCOCAL_START                            (0x1 << 21)

#define UTMI_PLL_KVCO_SHIFT			15
#define UTMI_PLL_KVCO_MASK                      (0x7 << 15)

#define UTMI_PLL_ICP_SHIFT			12
#define UTMI_PLL_ICP_MASK                       (0x7 << 12)

#define UTMI_PLL_FBDIV_SHIFT                    4
#define UTMI_PLL_FBDIV_MASK                     (0xFF << 4)

#define UTMI_PLL_REFDIV_SHIFT                   0
#define UTMI_PLL_REFDIV_MASK                    (0xF << 0)

/* For UTMI_TX Register */
#define UTMI_TX_REG_EXT_FS_RCAL_SHIFT		27
#define UTMI_TX_REG_EXT_FS_RCAL_MASK		(0xf << 27)

#define UTMI_TX_REG_EXT_FS_RCAL_EN_SHIFT	26
#define UTMI_TX_REG_EXT_FS_RCAL_EN_MASK		(0x1 << 26)

#define UTMI_TX_TXVDD12_SHIFT                   22
#define UTMI_TX_TXVDD12_MASK                    (0x3 << 22)

#define UTMI_TX_CK60_PHSEL_SHIFT                17
#define UTMI_TX_CK60_PHSEL_MASK                 (0xf << 17)

#define UTMI_TX_IMPCAL_VTH_SHIFT                14
#define UTMI_TX_IMPCAL_VTH_MASK                 (0x7 << 14)

#define REG_RCAL_START                          (0x1 << 12)

#define UTMI_TX_LOW_VDD_EN_SHIFT                11

#define UTMI_TX_AMP_SHIFT			0
#define UTMI_TX_AMP_MASK			(0x7 << 0)

/* For UTMI_RX Register */
#define UTMI_REG_SQ_LENGTH_SHIFT                15
#define UTMI_REG_SQ_LENGTH_MASK                 (0x3 << 15)

#define UTMI_RX_SQ_THRESH_SHIFT                 4
#define UTMI_RX_SQ_THRESH_MASK                  (0xf << 4)

#define UTMI_OTG_ADDON_OTG_ON			(1 << 0)

/* For MMP3 USB Phy */
#define USB2_PLL_REG0		0x4
#define USB2_PLL_REG1		0x8
#define USB2_TX_REG0		0x10
#define USB2_TX_REG1		0x14
#define USB2_TX_REG2		0x18
#define USB2_RX_REG0		0x20
#define USB2_RX_REG1		0x24
#define USB2_RX_REG2		0x28
#define USB2_ANA_REG0		0x30
#define USB2_ANA_REG1		0x34
#define USB2_ANA_REG2		0x38
#define USB2_DIG_REG0		0x3C
#define USB2_DIG_REG1		0x40
#define USB2_DIG_REG2		0x44
#define USB2_DIG_REG3		0x48
#define USB2_TEST_REG0		0x4C
#define USB2_TEST_REG1		0x50
#define USB2_TEST_REG2		0x54
#define USB2_CHARGER_REG0	0x58
#define USB2_OTG_REG0		0x5C
#define USB2_PHY_MON0		0x60
#define USB2_RESETVE_REG0	0x64
#define USB2_ICID_REG0		0x78
#define USB2_ICID_REG1		0x7C

/* USB2_PLL_REG0 */
/* This is for Ax stepping */
#define USB2_PLL_FBDIV_SHIFT_MMP3		0
#define USB2_PLL_FBDIV_MASK_MMP3		(0xFF << 0)

#define USB2_PLL_REFDIV_SHIFT_MMP3		8
#define USB2_PLL_REFDIV_MASK_MMP3		(0xF << 8)

#define USB2_PLL_VDD12_SHIFT_MMP3		12
#define USB2_PLL_VDD18_SHIFT_MMP3		14

/* This is for B0 stepping */
#define USB2_PLL_FBDIV_SHIFT_MMP3_B0		0
#define USB2_PLL_REFDIV_SHIFT_MMP3_B0		9
#define USB2_PLL_VDD18_SHIFT_MMP3_B0		14
#define USB2_PLL_FBDIV_MASK_MMP3_B0		0x01FF
#define USB2_PLL_REFDIV_MASK_MMP3_B0		0x3E00

#define USB2_PLL_CAL12_SHIFT_MMP3		0
#define USB2_PLL_CALI12_MASK_MMP3		(0x3 << 0)

#define USB2_PLL_VCOCAL_START_SHIFT_MMP3	2

#define USB2_PLL_KVCO_SHIFT_MMP3		4
#define USB2_PLL_KVCO_MASK_MMP3			(0x7<<4)

#define USB2_PLL_ICP_SHIFT_MMP3			8
#define USB2_PLL_ICP_MASK_MMP3			(0x7<<8)

#define USB2_PLL_LOCK_BYPASS_SHIFT_MMP3		12

#define USB2_PLL_PU_PLL_SHIFT_MMP3		13
#define USB2_PLL_PU_PLL_MASK			(0x1 << 13)

#define USB2_PLL_READY_MASK_MMP3		(0x1 << 15)

/* USB2_TX_REG0 */
#define USB2_TX_IMPCAL_VTH_SHIFT_MMP3		8
#define USB2_TX_IMPCAL_VTH_MASK_MMP3		(0x7 << 8)

#define USB2_TX_RCAL_START_SHIFT_MMP3		13

/* USB2_TX_REG1 */
#define USB2_TX_CK60_PHSEL_SHIFT_MMP3		0
#define USB2_TX_CK60_PHSEL_MASK_MMP3		(0xf << 0)

#define USB2_TX_AMP_SHIFT_MMP3			4
#define USB2_TX_AMP_MASK_MMP3			(0x7 << 4)

#define USB2_TX_VDD12_SHIFT_MMP3		8
#define USB2_TX_VDD12_MASK_MMP3			(0x3 << 8)

/* USB2_TX_REG2 */
#define USB2_TX_DRV_SLEWRATE_SHIFT		10

/* USB2_RX_REG0 */
#define USB2_RX_SQ_THRESH_SHIFT_MMP3		4
#define USB2_RX_SQ_THRESH_MASK_MMP3		(0xf << 4)

#define USB2_RX_SQ_LENGTH_SHIFT_MMP3		10
#define USB2_RX_SQ_LENGTH_MASK_MMP3		(0x3 << 10)

/* USB2_ANA_REG1*/
#define USB2_ANA_PU_ANA_SHIFT_MMP3		14

/* USB2_OTG_REG0 */
#define USB2_OTG_PU_OTG_SHIFT_MMP3		3

/* fsic registers */
#define FSIC_MISC			0x4
#define FSIC_INT			0x28
#define FSIC_CTRL			0x30

/* HSIC registers */
#define HSIC_PAD_CTRL			0x4

#define HSIC_CTRL			0x8
#define HSIC_CTRL_HSIC_ENABLE		(1<<7)
#define HSIC_CTRL_PLL_BYPASS		(1<<4)

#define TEST_GRP_0			0xc
#define TEST_GRP_1			0x10

#define HSIC_INT			0x14
#define HSIC_INT_READY_INT_EN		(1<<10)
#define HSIC_INT_CONNECT_INT_EN		(1<<9)
#define HSIC_INT_CORE_INT_EN		(1<<8)
#define HSIC_INT_HS_READY		(1<<2)
#define HSIC_INT_CONNECT		(1<<1)
#define HSIC_INT_CORE			(1<<0)

#define HSIC_CONFIG			0x18
#define USBHSIC_CTRL			0x20

#define HSIC_USB_CTRL			0x28
#define HSIC_USB_CTRL_CLKEN		1
#define	HSIC_USB_CLK_PHY		0x0
#define HSIC_USB_CLK_PMU		0x1

#endif /* CONFIG_CPU_PXA988 */
#endif /* __ASM_ARCH_REGS_USB_H */
