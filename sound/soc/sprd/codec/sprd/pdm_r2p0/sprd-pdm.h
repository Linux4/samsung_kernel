/*
 * Copyright (C) 202- Spreadtrum Communications Inc.
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 */

#ifndef _PDM_REG_H
#define _PDM_REG_H

/* AUDCP AHB REG */
#define	AHB_MODULE_EB0_STS			0x0
#define	PDM_EB						BIT(3)

#define	AHB_MODULE_RST0_STS			0x8
#define	PDM_SOFT_RST				BIT(6)

/* AUDCP AON APB REG */
#define	APB_MODULE_EB0_STS          0x0
#define	PDM_IIS_EB                  BIT(4)

#define	APB_MODULE_EB1_STS          0x4
#define	PDM_AP_EB                   BIT(1)

#define	APB_MODULE_RST0_STS         0x8
#define	APB_PDM_IIS_SOFT_RST        BIT(2)
#define	APB_PDM_SOFT_RST            BIT(1)

#define	CLK_CGM_PDM_SEL_CFG         0x28
#define	CLK_CGM_PDM_SEL             BIT(0)

/* PDM REG */
#define	ADC0_CTRL					0x0
#define	ADC2_DMIC_EN_R				BIT(16)
#define	ADC2_DMIC_EN_L				BIT(15)
#define	ADC2_DP_EN					BIT(14)

#define	PDM_CLR						0x1c
#define	BIT_PDM_CLR					BIT(0)

#define	ADC2_CTRL					0x4c
#define	ADC2_CTRL_EN_R				BIT(10)
#define	ADC2_CTRL_EN_L				BIT(9)
#define	ADC2_DECIM_FIRST_EN_R		BIT(6)
#define	ADC2_DECIM_FIRST_EN_L		BIT(5)
#define	ADC2_D5_TAPS25_EN_R		    BIT(4)
#define	ADC2_D5_TAPS25_EN_L		    BIT(3)
#define	ADC2_DECIM_3RD_EN_R		    BIT(2)
#define	ADC2_DECIM_3RD_EN_L		    BIT(1)

#define	PDM_DMIC2_DIVIDER			0x50
#define	BIT_PDM_DMIC2_DIV(x)		((x) & GENMASK(7, 0))

#define	ADC2_FIRST_DECIM_CFG		0x54
#define	ADC2_DECIM_1ST_UP_NUM(x)	(((x) & GENMASK(2, 0)) << 4)
#define	ADC2_DECIM_1ST_DOWN_NUM(x)	((x) & GENMASK(2, 0))

#define	ADC2_DMIC_CFG				0x58
#define	ADC2_DMIC_LR_SEL			BIT(0)

#define	DG_CFG1						0x64
#define	ADC2_DG_R(x)				(((x) & GENMASK(7, 0)) << 8)
#define	ADC2_DG_L(x)				((x) & GENMASK(7, 0))

#define	IIS2_CTRL					0x68
#define	PDM_TX2_LRCK_INVERT			BIT(9)
#define	IIS2_SLOT_WIDTH(x)			(((x) & GENMASK(1, 0)) << 7)
#define	IIS2_EN						BIT(6)
#define	IIS2_MSB					BIT(3)

#define	TX_CFG3						0x6c
#define	TX2_LRCK_DIV_NUM(x)			(((x) & GENMASK(15, 0)) << 16)
#define	TX2_BCK_DIV_NUM(x)			((x) & GENMASK(15, 0))

#define PDM_REG_BAK					0x70
#define PDM_REG_MAX					(PDM_REG_BAK + 1)

#endif /* _PDM_REG_H */
