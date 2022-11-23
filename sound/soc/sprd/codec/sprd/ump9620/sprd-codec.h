/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This file is dual-licensed: you can use it either under the terms
 * of the GPL or the X11 license, at your option. Note that this dual
 * licensing only applies to this file, and not this project as a
 * whole.
 *
 */

#ifndef __SPRD_CODEC_UMP9620_H
#define __SPRD_CODEC_UMP9620_H

#include "sprd-audio-ump9620.h"

#define SPRD_CODEC_RATE_8000   10
#define SPRD_CODEC_RATE_9600   9
#define SPRD_CODEC_RATE_11025  8
#define SPRD_CODEC_RATE_12000  7
#define SPRD_CODEC_RATE_16000  6
#define SPRD_CODEC_RATE_22050  5
#define SPRD_CODEC_RATE_24000  4
#define SPRD_CODEC_RATE_32000  3
#define SPRD_CODEC_RATE_44100  2
#define SPRD_CODEC_RATE_48000  1
#define SPRD_CODEC_RATE_96000  0

#define SPRD_CODEC_IIS0_ID  111


/* AUD_TOP_CTL */
#define DAC_EN_L		0
#define ADC_EN_L		1
#define DAC_EN_R		2
#define ADC_EN_R		3
#define ADC_SINC_SEL		8
#define ADC_SINC_SEL_ADC	0
#define ADC_SINC_SEL_DAC	1
#define ADC_SINC_SEL_D		2
#define ADC_SINC_SEL_MASK	0x3
#define ADC1_EN_L		10
#define ADC1_EN_R		11
#define ADC1_SINC_SEL		14
#define ADC1_SINC_SEL_ADC	0
#define ADC1_SINC_SEL_DAC	1
#define ADC1_SINC_SEL_D		2
#define ADC1_SINC_SEL_MASK	0x3

/* AUD_I2S_CTL */
#define ADC_LR_SEL		2
#define DAC_LR_SEL		1

/* AUD_DAC_CTL */
#define DAC_MUTE_EN		15
#define DAC_MUTE_CTL		14
#define DAC_FS_MODE		0
#define DAC_FS_MODE_96k		0
#define DAC_FS_MODE_48k		1
#define DAC_FS_MODE_MASK	0xf

/* AUD_ADC_CTL */
#define ADC_SRC_N		0
#define ADC_SRC_N_MASK		0xf
#define ADC1_SRC_N		4
#define ADC1_SRC_N_MASK		0xf

#define ADC_FS_MODE_48k		0xc
#define ADC_FS_MODE		0


/* AUD_LOOP_CTL */
#define AUD_LOOP_TEST		0
#define LOOP_ADC_PATH_SEL	9
#define LOOP_PATH_SEL		1
#define LOOP_PATH_SEL_MASK	0x3
/* AUD_AUD_STS0 */
#define DAC_MUTE_U_MASK		5
#define DAC_MUTE_D_MASK		4
#define DAC_MUTE_U_RAW		3
#define DAC_MUTE_D_RAW		2
#define DAC_MUTE_ST		0
#define DAC_MUTE_ST_MASK	0x3

/* AUD_INT_CLR */
/* AUD_INT_EN */
#define DAC_MUTE_U		1
#define DAC_MUTE_D		0

/*AUD_DMIC_CTL*/
#define ADC_DMIC_CLK_MODE	0
#define ADC_DMIC_CLK_MODE_MASK	0x3
#define ADC_DMIC_LR_SEL		2
#define ADC1_DMIC_CLK_MODE	3
#define ADC1_DMIC_CLK_MODE_MASK	0x3
#define ADC1_DMIC_LR_SEL	5
#define ADC_DMIC_EN		6
#define ADC1_DMIC1_EN		7

/* AUD_ADC1_I2S_CTL */
#define ADC1_LR_SEL		0

/*DAC_SDM_DC_L*/
#define DAC_SDM_DC_L		0
#define DAC_SDM_DC_L_MASK	0xffff
/*DAC_SDM_DC_H*/
#define DAC_SDM_DC_R		0
#define DAC_SDM_DC_R_MASK	0xff

/* AUD_DNS_SW */
#define RG_DNS_SW		0

#define RG_AUD_HPL_G_BP_EN	BIT(4)
#define RG_AUD_HPR_G_BP_EN	BIT(5)

/* DNS_AUTOGATE_EN */
#define RG_DNS_BYPASS				BIT(1)
#define RG_DNS_HPR_G(x)				(((x) & GENMASK(3, 0)) << 8)
#define RG_DNS_HPL_G(x)				(((x) & GENMASK(3, 0)) << 12)
#define RG_DNS_DG_SW				BIT(0)
#define RG_DNS_ZC_SW				BIT(0)
#define RG_DNS_PGA_TH_L(x)			(((x) & GENMASK(3, 0)) << 8)
#define RG_DNS_PGA_TH_R(x)			(((x) & GENMASK(3, 0)) << 4)
#define RG_DNS_MODE(x)				(((x) & GENMASK(1, 0)))
#define RG_DNS_DG_GAIN_WT_SW			BIT(0)
#define RG_DNS_BLOCK_MS(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_MS_SMPL(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_M_HOLD_SET_F(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_M_HOLD_SET_N(x)			(((x) & GENMASK(15, 0)))

#define RG_DNS_LEVEL_SET_0(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_1(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_2(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_3(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_4(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_5(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_6(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_7(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_8(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_9(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_10(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_11(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_LEVEL_SET_12(x)			(((x) & GENMASK(15, 0)))

#define RG_DNS_DG_GAIN_SET_0(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_1(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_2(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_3(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_4(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_5(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_6(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_7(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_8(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_9(x)			(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_10(x)		(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_11(x)		(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_GAIN_SET_12(x)		(((x) & GENMASK(15, 0)))

#define RG_DNS_DG_GAIN_VT_MAX(x)		(((x) & GENMASK(3, 0)))
#define RG_DNS_ZC_DG_UP_LAST_SET(x)		(((x) & GENMASK(15, 0)))
#define RG_DNS_DG_USTEP(x)			(((x) & GENMASK(3, 0)))
#define RG_DNS_ZC_DG_DN_C			BIT(0)
#define RG_DNS_ZC_PGA_C				BIT(0)
#define RG_DNS_QUICKEXTI_N(x)			(((x) & GENMASK(3, 0)))
#define RG_DNS_DG_GAIN_DEFAULT(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_MAX(x)			(((x) & GENMASK(5, 0)))

#define RG_DNS_PGA_GAIN_CAL_SET_0_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_1_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_2_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_3_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_4_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_5_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_6_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_7_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_8_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_9_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_10_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_11_L(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_12_L(x)		(((x) & GENMASK(13, 0)))

#define RG_DNS_PGA_DELAY_CAL_SET_0_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_1_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_2_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_3_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_4_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_5_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_6_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_7_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_8_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_9_L(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_10_L(x)	(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_11_L(x)	(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_12_L(x)	(((x) & GENMASK(4, 0)))

#define RG_DNS_PGA_GAIN_CAL_SET_0_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_1_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_2_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_3_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_4_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_5_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_6_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_7_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_8_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_9_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_10_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_11_R(x)		(((x) & GENMASK(13, 0)))
#define RG_DNS_PGA_GAIN_CAL_SET_12_R(x)		(((x) & GENMASK(13, 0)))

#define RG_DNS_PGA_DELAY_CAL_SET_0_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_1_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_2_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_3_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_4_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_5_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_6_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_7_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_8_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_9_R(x)		(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_10_R(x)	(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_11_R(x)	(((x) & GENMASK(4, 0)))
#define RG_DNS_PGA_DELAY_CAL_SET_12_R(x)	(((x) & GENMASK(4, 0)))

#define RG_DNS_CFG_LOAD_FLAG			BIT(0)

/*VB_V*/
#define LDO_V_3000		4
#define LDO_V_3025		5
#define LDO_V_3050		6
#define LDO_V_3075		7
#define LDO_V_3100		8
#define LDO_V_3125		9
#define LDO_V_3150		10
#define LDO_V_3175		11
#define LDO_V_3200		12
#define LDO_V_3225		13
#define LDO_V_3250		14
#define LDO_V_3275		15
#define LDO_V_3300		16
#define LDO_V_3325		17
#define LDO_V_3350		18
#define LDO_V_3375		19
#define LDO_V_3400		20
#define LDO_V_3425		21
#define LDO_V_3450		22
#define LDO_V_3475		23
#define LDO_V_3500		24
#define LDO_V_3525		25
#define LDO_V_3550		26
#define LDO_V_3575		27
#define LDO_V_3600		28


#define MIC_LDO_V_21		0
#define MIC_LDO_V_19		1
#define MIC_LDO_V_23		2
#define MIC_LDO_V_25		3

#define SPRD_CODEC_DP_BASE (CODEC_DP_BASE)
#define AUD_TOP_CTL		(SPRD_CODEC_DP_BASE + 0x0000)
#define AUD_AUD_CTR		(SPRD_CODEC_DP_BASE + 0x0004)
#define AUD_I2S_CTL		(SPRD_CODEC_DP_BASE + 0x0008)
#define AUD_DAC_CTL		(SPRD_CODEC_DP_BASE + 0x000C)
#define AUD_SDM_CTL0		(SPRD_CODEC_DP_BASE + 0x0010)
#define AUD_SDM_CTL1		(SPRD_CODEC_DP_BASE + 0x0014)
#define AUD_ADC_CTL		(SPRD_CODEC_DP_BASE + 0x0018)
#define AUD_LOOP_CTL		(SPRD_CODEC_DP_BASE + 0x001C)
#define AUD_AUD_STS0		(SPRD_CODEC_DP_BASE + 0x0020)
#define AUD_INT_CLR		(SPRD_CODEC_DP_BASE + 0x0024)
#define AUD_INT_EN		(SPRD_CODEC_DP_BASE + 0x0028)
#define AUDIF_FIFO_CTL		(SPRD_CODEC_DP_BASE + 0x002C)
#define AUD_DMIC_CTL		(SPRD_CODEC_DP_BASE + 0x0030)
#define AUD_ADC1_I2S_CTL		(SPRD_CODEC_DP_BASE + 0x0034)
#define AUD_DAC_SDM_L		(SPRD_CODEC_DP_BASE + 0x0038)
#define AUD_DAC_SDM_H		(SPRD_CODEC_DP_BASE + 0x003C)
#define AUD_DNS_AUTOGATE_EN	(SPRD_CODEC_DP_BASE + 0x0084)
#define AUD_DNS_SW		(SPRD_CODEC_DP_BASE + 0x0088)
#define AUD_DNS_DG_SW		(SPRD_CODEC_DP_BASE + 0x008C)
#define AUD_DNS_ZC_SW		(SPRD_CODEC_DP_BASE + 0x0090)
#define AUD_DNS_MODE		(SPRD_CODEC_DP_BASE + 0x0094)
#define AUD_DNS_GAIN_WT_SW	(SPRD_CODEC_DP_BASE + 0x00A4)
#define AUD_DNS_BLOCK_MS	(SPRD_CODEC_DP_BASE + 0x00A8)
#define AUD_DNS_BLOCK_SMPL	(SPRD_CODEC_DP_BASE + 0x00AC)
#define AUD_DNS_M_HOLD_SET_F	(SPRD_CODEC_DP_BASE + 0x00B0)
#define AUD_DNS_M_HOLD_SET_N	(SPRD_CODEC_DP_BASE + 0x00B4)

#define AUD_DNS_LEVEL_SET_0	(SPRD_CODEC_DP_BASE + 0x00B8)
#define AUD_DNS_LEVEL_SET_1	(SPRD_CODEC_DP_BASE + 0x00BC)
#define AUD_DNS_LEVEL_SET_2	(SPRD_CODEC_DP_BASE + 0x00C0)
#define AUD_DNS_LEVEL_SET_3	(SPRD_CODEC_DP_BASE + 0x00C4)
#define AUD_DNS_LEVEL_SET_4	(SPRD_CODEC_DP_BASE + 0x00C8)
#define AUD_DNS_LEVEL_SET_5	(SPRD_CODEC_DP_BASE + 0x00CC)
#define AUD_DNS_LEVEL_SET_6	(SPRD_CODEC_DP_BASE + 0x00D0)
#define AUD_DNS_LEVEL_SET_7	(SPRD_CODEC_DP_BASE + 0x00D4)
#define AUD_DNS_LEVEL_SET_8	(SPRD_CODEC_DP_BASE + 0x00D8)
#define AUD_DNS_LEVEL_SET_9	(SPRD_CODEC_DP_BASE + 0x00DC)
#define AUD_DNS_LEVEL_SET_10	(SPRD_CODEC_DP_BASE + 0x00E0)
#define AUD_DNS_LEVEL_SET_11	(SPRD_CODEC_DP_BASE + 0x00E4)
#define AUD_DNS_LEVEL_SET_12	(SPRD_CODEC_DP_BASE + 0x00E8)

#define AUD_DNS_DG_GAIN_SET_0	(SPRD_CODEC_DP_BASE + 0x00EC)
#define AUD_DNS_DG_GAIN_SET_1	(SPRD_CODEC_DP_BASE + 0x00F0)
#define AUD_DNS_DG_GAIN_SET_2	(SPRD_CODEC_DP_BASE + 0x00F4)
#define AUD_DNS_DG_GAIN_SET_3	(SPRD_CODEC_DP_BASE + 0x00F8)
#define AUD_DNS_DG_GAIN_SET_4	(SPRD_CODEC_DP_BASE + 0x00FC)
#define AUD_DNS_DG_GAIN_SET_5	(SPRD_CODEC_DP_BASE + 0x0100)
#define AUD_DNS_DG_GAIN_SET_6	(SPRD_CODEC_DP_BASE + 0x0104)
#define AUD_DNS_DG_GAIN_SET_7	(SPRD_CODEC_DP_BASE + 0x0108)
#define AUD_DNS_DG_GAIN_SET_8	(SPRD_CODEC_DP_BASE + 0x010C)
#define AUD_DNS_DG_GAIN_SET_9	(SPRD_CODEC_DP_BASE + 0x0110)
#define AUD_DNS_DG_GAIN_SET_10	(SPRD_CODEC_DP_BASE + 0x0114)
#define AUD_DNS_DG_GAIN_SET_11	(SPRD_CODEC_DP_BASE + 0x0118)
#define AUD_DNS_DG_GAIN_SET_12	(SPRD_CODEC_DP_BASE + 0x011C)

#define AUD_DNS_DG_GAIN_VT_MAX		(SPRD_CODEC_DP_BASE + 0x0120)
#define AUD_DNS_ZC_DG_UP_LAST_SET	(SPRD_CODEC_DP_BASE + 0x0124)
#define AUD_DNS_DG_USTEP		(SPRD_CODEC_DP_BASE + 0x0128)
#define AUD_DNS_ZC_DG_DN_C		(SPRD_CODEC_DP_BASE + 0x012C)
#define AUD_DNS_ZC_PGA_C		(SPRD_CODEC_DP_BASE + 0x0130)
#define AUD_DNS_QUICKEXTI_N		(SPRD_CODEC_DP_BASE + 0x0134)
#define AUD_DNS_DG_GAIN_DEFAULT		(SPRD_CODEC_DP_BASE + 0x0138)
#define AUD_DNS_PGA_MAX			(SPRD_CODEC_DP_BASE + 0x013C)

#define AUD_DNS_PGA_GAIN_CAL_SET_0_L	(SPRD_CODEC_DP_BASE + 0x0140)
#define AUD_DNS_PGA_GAIN_CAL_SET_1_L	(SPRD_CODEC_DP_BASE + 0x0144)
#define AUD_DNS_PGA_GAIN_CAL_SET_2_L	(SPRD_CODEC_DP_BASE + 0x0148)
#define AUD_DNS_PGA_GAIN_CAL_SET_3_L	(SPRD_CODEC_DP_BASE + 0x014C)
#define AUD_DNS_PGA_GAIN_CAL_SET_4_L	(SPRD_CODEC_DP_BASE + 0x0150)
#define AUD_DNS_PGA_GAIN_CAL_SET_5_L	(SPRD_CODEC_DP_BASE + 0x0154)
#define AUD_DNS_PGA_GAIN_CAL_SET_6_L	(SPRD_CODEC_DP_BASE + 0x0158)
#define AUD_DNS_PGA_GAIN_CAL_SET_7_L	(SPRD_CODEC_DP_BASE + 0x015C)
#define AUD_DNS_PGA_GAIN_CAL_SET_8_L	(SPRD_CODEC_DP_BASE + 0x0160)
#define AUD_DNS_PGA_GAIN_CAL_SET_9_L	(SPRD_CODEC_DP_BASE + 0x0164)
#define AUD_DNS_PGA_GAIN_CAL_SET_10_L	(SPRD_CODEC_DP_BASE + 0x0168)
#define AUD_DNS_PGA_GAIN_CAL_SET_11_L	(SPRD_CODEC_DP_BASE + 0x016C)
#define AUD_DNS_PGA_GAIN_CAL_SET_12_L	(SPRD_CODEC_DP_BASE + 0x0170)

#define AUD_DNS_PGA_DELAY_CAL_SET_0_L	(SPRD_CODEC_DP_BASE + 0x0174)
#define AUD_DNS_PGA_DELAY_CAL_SET_1_L	(SPRD_CODEC_DP_BASE + 0x0178)
#define AUD_DNS_PGA_DELAY_CAL_SET_2_L	(SPRD_CODEC_DP_BASE + 0x017C)
#define AUD_DNS_PGA_DELAY_CAL_SET_3_L	(SPRD_CODEC_DP_BASE + 0x0180)
#define AUD_DNS_PGA_DELAY_CAL_SET_4_L	(SPRD_CODEC_DP_BASE + 0x0184)
#define AUD_DNS_PGA_DELAY_CAL_SET_5_L	(SPRD_CODEC_DP_BASE + 0x0188)
#define AUD_DNS_PGA_DELAY_CAL_SET_6_L	(SPRD_CODEC_DP_BASE + 0x018C)
#define AUD_DNS_PGA_DELAY_CAL_SET_7_L	(SPRD_CODEC_DP_BASE + 0x0190)
#define AUD_DNS_PGA_DELAY_CAL_SET_8_L	(SPRD_CODEC_DP_BASE + 0x0194)
#define AUD_DNS_PGA_DELAY_CAL_SET_9_L	(SPRD_CODEC_DP_BASE + 0x0198)
#define AUD_DNS_PGA_DELAY_CAL_SET_10_L	(SPRD_CODEC_DP_BASE + 0x019C)
#define AUD_DNS_PGA_DELAY_CAL_SET_11_L	(SPRD_CODEC_DP_BASE + 0x01A0)
#define AUD_DNS_PGA_DELAY_CAL_SET_12_L	(SPRD_CODEC_DP_BASE + 0x01A4)

#define AUD_DNS_PGA_GAIN_CAL_SET_0_R	(SPRD_CODEC_DP_BASE + 0x01AC)
#define AUD_DNS_PGA_GAIN_CAL_SET_1_R	(SPRD_CODEC_DP_BASE + 0x01B0)
#define AUD_DNS_PGA_GAIN_CAL_SET_2_R	(SPRD_CODEC_DP_BASE + 0x01B4)
#define AUD_DNS_PGA_GAIN_CAL_SET_3_R	(SPRD_CODEC_DP_BASE + 0x01B8)
#define AUD_DNS_PGA_GAIN_CAL_SET_4_R	(SPRD_CODEC_DP_BASE + 0x01BC)
#define AUD_DNS_PGA_GAIN_CAL_SET_5_R	(SPRD_CODEC_DP_BASE + 0x01C0)
#define AUD_DNS_PGA_GAIN_CAL_SET_6_R	(SPRD_CODEC_DP_BASE + 0x01C4)
#define AUD_DNS_PGA_GAIN_CAL_SET_7_R	(SPRD_CODEC_DP_BASE + 0x01C8)
#define AUD_DNS_PGA_GAIN_CAL_SET_8_R	(SPRD_CODEC_DP_BASE + 0x01CC)
#define AUD_DNS_PGA_GAIN_CAL_SET_9_R	(SPRD_CODEC_DP_BASE + 0x01D0)
#define AUD_DNS_PGA_GAIN_CAL_SET_10_R	(SPRD_CODEC_DP_BASE + 0x01D4)
#define AUD_DNS_PGA_GAIN_CAL_SET_11_R	(SPRD_CODEC_DP_BASE + 0x01D8)
#define AUD_DNS_PGA_GAIN_CAL_SET_12_R	(SPRD_CODEC_DP_BASE + 0x01DC)

#define AUD_DNS_PGA_DELAY_CAL_SET_0_R	(SPRD_CODEC_DP_BASE + 0x01E0)
#define AUD_DNS_PGA_DELAY_CAL_SET_1_R	(SPRD_CODEC_DP_BASE + 0x01E4)
#define AUD_DNS_PGA_DELAY_CAL_SET_2_R	(SPRD_CODEC_DP_BASE + 0x01E8)
#define AUD_DNS_PGA_DELAY_CAL_SET_3_R	(SPRD_CODEC_DP_BASE + 0x01EC)
#define AUD_DNS_PGA_DELAY_CAL_SET_4_R	(SPRD_CODEC_DP_BASE + 0x01F0)
#define AUD_DNS_PGA_DELAY_CAL_SET_5_R	(SPRD_CODEC_DP_BASE + 0x01F4)
#define AUD_DNS_PGA_DELAY_CAL_SET_6_R	(SPRD_CODEC_DP_BASE + 0x01F8)
#define AUD_DNS_PGA_DELAY_CAL_SET_7_R	(SPRD_CODEC_DP_BASE + 0x01FC)
#define AUD_DNS_PGA_DELAY_CAL_SET_8_R	(SPRD_CODEC_DP_BASE + 0x0200)
#define AUD_DNS_PGA_DELAY_CAL_SET_9_R	(SPRD_CODEC_DP_BASE + 0x0204)
#define AUD_DNS_PGA_DELAY_CAL_SET_10_R	(SPRD_CODEC_DP_BASE + 0x0208)
#define AUD_DNS_PGA_DELAY_CAL_SET_11_R	(SPRD_CODEC_DP_BASE + 0x020C)
#define AUD_DNS_PGA_DELAY_CAL_SET_12_R	(SPRD_CODEC_DP_BASE + 0x0210)

#define AUD_DNS_CFG_LOAD_FLAG		(SPRD_CODEC_DP_BASE + 0x0218)

#define SPRD_CODEC_DP_END		(SPRD_CODEC_DP_BASE + 0x02f5)
#define IS_SPRD_CODEC_DP_RANG(reg) (((reg) >= SPRD_CODEC_DP_BASE) \
	&& ((reg) < SPRD_CODEC_DP_END))


#define SPRD_CODEC_AP_BASE (CODEC_AP_BASE)
#define CTL_BASE_AUD_CFGA_RF SPRD_CODEC_AP_BASE

#define ANA_PMU0      (CTL_BASE_AUD_CFGA_RF + 0x0000)
#define ANA_PMU1      (CTL_BASE_AUD_CFGA_RF + 0x0004)
#define ANA_PMU2      (CTL_BASE_AUD_CFGA_RF + 0x0008)
#define ANA_PMU3      (CTL_BASE_AUD_CFGA_RF + 0x000C)
#define ANA_PMU4      (CTL_BASE_AUD_CFGA_RF + 0x0010)
#define ANA_PMU5      (CTL_BASE_AUD_CFGA_RF + 0x0014)
#define ANA_PMU6      (CTL_BASE_AUD_CFGA_RF + 0x0018)
#define ANA_PMU7      (CTL_BASE_AUD_CFGA_RF + 0x001C)
#define ANA_PMU8      (CTL_BASE_AUD_CFGA_RF + 0x0020)
#define ANA_PMU9      (CTL_BASE_AUD_CFGA_RF + 0x0024)
#define ANA_PMU10     (CTL_BASE_AUD_CFGA_RF + 0x0028)
#define ANA_PMU11     (CTL_BASE_AUD_CFGA_RF + 0x002C)
#define ANA_PMU12     (CTL_BASE_AUD_CFGA_RF + 0x0030)
#define ANA_PMU13     (CTL_BASE_AUD_CFGA_RF + 0x0034)
#define ANA_PMU14     (CTL_BASE_AUD_CFGA_RF + 0x0038)
#define ANA_PMU15     (CTL_BASE_AUD_CFGA_RF + 0x003C)
#define ANA_PMU16     (CTL_BASE_AUD_CFGA_RF + 0x0040)
#define ANA_PMU17     (CTL_BASE_AUD_CFGA_RF + 0x0044)
#define ANA_PMU18     (CTL_BASE_AUD_CFGA_RF + 0x0048)
#define ANA_PMU19     (CTL_BASE_AUD_CFGA_RF + 0x004C)
#define ANA_DAC0      (CTL_BASE_AUD_CFGA_RF + 0x0050)
#define ANA_DAC1      (CTL_BASE_AUD_CFGA_RF + 0x0054)
#define ANA_DAC2      (CTL_BASE_AUD_CFGA_RF + 0x0058)
#define ANA_DAC3      (CTL_BASE_AUD_CFGA_RF + 0x005C)
#define ANA_DAC4      (CTL_BASE_AUD_CFGA_RF + 0x0060)
#define ANA_RSV1      (CTL_BASE_AUD_CFGA_RF + 0x0064)
#define ANA_CLK0      (CTL_BASE_AUD_CFGA_RF + 0x0068)
#define ANA_CLK1      (CTL_BASE_AUD_CFGA_RF + 0x006C)
#define ANA_CLK2      (CTL_BASE_AUD_CFGA_RF + 0x0070)
#define ANA_CDC0      (CTL_BASE_AUD_CFGA_RF + 0x0074)
#define ANA_CDC1      (CTL_BASE_AUD_CFGA_RF + 0x0078)
#define ANA_CDC2      (CTL_BASE_AUD_CFGA_RF + 0x007C)
#define ANA_CDC3      (CTL_BASE_AUD_CFGA_RF + 0x0080)
#define ANA_CDC4      (CTL_BASE_AUD_CFGA_RF + 0x0084)
#define ANA_CDC5      (CTL_BASE_AUD_CFGA_RF + 0x0088)
#define ANA_CDC6      (CTL_BASE_AUD_CFGA_RF + 0x008C)
#define ANA_CDC7      (CTL_BASE_AUD_CFGA_RF + 0x0090)
#define ANA_CDC8      (CTL_BASE_AUD_CFGA_RF + 0x0094)
#define ANA_CDC9      (CTL_BASE_AUD_CFGA_RF + 0x0098)
#define ANA_CDC10     (CTL_BASE_AUD_CFGA_RF + 0x009C)
#define ANA_CDC11     (CTL_BASE_AUD_CFGA_RF + 0x00A0)
#define ANA_CDC12     (CTL_BASE_AUD_CFGA_RF + 0x00A4)
#define ANA_CDC13     (CTL_BASE_AUD_CFGA_RF + 0x00A8)
#define ANA_CDC14     (CTL_BASE_AUD_CFGA_RF + 0x00AC)
#define ANA_CDC15     (CTL_BASE_AUD_CFGA_RF + 0x00B0)
#define ANA_CDC16     (CTL_BASE_AUD_CFGA_RF + 0x00B4)
#define ANA_CDC17     (CTL_BASE_AUD_CFGA_RF + 0x00B8)
#define ANA_CDC18     (CTL_BASE_AUD_CFGA_RF + 0x00BC)
#define ANA_CDC19     (CTL_BASE_AUD_CFGA_RF + 0x00C0)
#define ANA_CDC20     (CTL_BASE_AUD_CFGA_RF + 0x00C4)
#define ANA_CDC21     (CTL_BASE_AUD_CFGA_RF + 0x00C8)
#define ANA_RSV3      (CTL_BASE_AUD_CFGA_RF + 0x00CC)
#define ANA_HDT0      (CTL_BASE_AUD_CFGA_RF + 0x00D0)
#define ANA_HDT1      (CTL_BASE_AUD_CFGA_RF + 0x00D4)
#define ANA_HDT2      (CTL_BASE_AUD_CFGA_RF + 0x00D8)
#define ANA_HDT3      (CTL_BASE_AUD_CFGA_RF + 0x00DC)
#define ANA_HDT4      (CTL_BASE_AUD_CFGA_RF + 0x00E0)
#define ANA_IMPD0     (CTL_BASE_AUD_CFGA_RF + 0x00E4)
#define ANA_IMPD1     (CTL_BASE_AUD_CFGA_RF + 0x00E8)
#define ANA_IMPD2     (CTL_BASE_AUD_CFGA_RF + 0x00EC)
#define ANA_IMPD3     (CTL_BASE_AUD_CFGA_RF + 0x00F0)
#define ANA_IMPD4     (CTL_BASE_AUD_CFGA_RF + 0x00F4)
#define ANA_RSV5      (CTL_BASE_AUD_CFGA_RF + 0x00F8)
#define ANA_DCL0      (CTL_BASE_AUD_CFGA_RF + 0x00FC)
#define ANA_DCL1      (CTL_BASE_AUD_CFGA_RF + 0x0100)
#define ANA_DCL2      (CTL_BASE_AUD_CFGA_RF + 0x0104)
#define ANA_DCL3      (CTL_BASE_AUD_CFGA_RF + 0x0108)
#define ANA_DCL4      (CTL_BASE_AUD_CFGA_RF + 0x010C)
#define ANA_DCL5      (CTL_BASE_AUD_CFGA_RF + 0x0110)
#define ANA_DCL6      (CTL_BASE_AUD_CFGA_RF + 0x0114)
#define ANA_DCL7      (CTL_BASE_AUD_CFGA_RF + 0x0118)
#define ANA_DCL8      (CTL_BASE_AUD_CFGA_RF + 0x011C)
#define ANA_DCL9      (CTL_BASE_AUD_CFGA_RF + 0x0120)
#define ANA_DCL10     (CTL_BASE_AUD_CFGA_RF + 0x0124)
#define ANA_DCL11     (CTL_BASE_AUD_CFGA_RF + 0x0128)
#define ANA_DCL12     (CTL_BASE_AUD_CFGA_RF + 0x012C)
#define ANA_DCL13     (CTL_BASE_AUD_CFGA_RF + 0x0130)
#define ANA_DCL14     (CTL_BASE_AUD_CFGA_RF + 0x0134)
#define ANA_DCL15     (CTL_BASE_AUD_CFGA_RF + 0x0138)
#define ANA_RSV7      (CTL_BASE_AUD_CFGA_RF + 0x013C)
#define ANA_RSV8      (CTL_BASE_AUD_CFGA_RF + 0x0140)
#define ANA_HID0      (CTL_BASE_AUD_CFGA_RF + 0x0144)
#define ANA_HID1      (CTL_BASE_AUD_CFGA_RF + 0x0148)
#define ANA_HID2      (CTL_BASE_AUD_CFGA_RF + 0x014C)
#define ANA_HID3      (CTL_BASE_AUD_CFGA_RF + 0x0150)
#define ANA_HID4      (CTL_BASE_AUD_CFGA_RF + 0x0154)
#define ANA_CFGA0     (CTL_BASE_AUD_CFGA_RF + 0x0158)
#define ANA_CFGA1     (CTL_BASE_AUD_CFGA_RF + 0x015C)
#define ANA_CFGA2     (CTL_BASE_AUD_CFGA_RF + 0x0160)
#define ANA_WR_PROT0  (CTL_BASE_AUD_CFGA_RF + 0x0164)
#define ANA_RSV9      (CTL_BASE_AUD_CFGA_RF + 0x0168)
#define ANA_DBG0      (CTL_BASE_AUD_CFGA_RF + 0x016C)
#define ANA_DBG1      (CTL_BASE_AUD_CFGA_RF + 0x0170)
#define ANA_DBG2      (CTL_BASE_AUD_CFGA_RF + 0x0174)
#define ANA_DBG3      (CTL_BASE_AUD_CFGA_RF + 0x0178)
#define ANA_DBG4      (CTL_BASE_AUD_CFGA_RF + 0x017C)
#define ANA_DBG5      (CTL_BASE_AUD_CFGA_RF + 0x0180)
#define ANA_STS0      (CTL_BASE_AUD_CFGA_RF + 0x0184)
#define ANA_STS1      (CTL_BASE_AUD_CFGA_RF + 0x0188)
#define ANA_STS2      (CTL_BASE_AUD_CFGA_RF + 0x018C)
#define ANA_STS3      (CTL_BASE_AUD_CFGA_RF + 0x0190)
#define ANA_STS4      (CTL_BASE_AUD_CFGA_RF + 0x0194)
#define ANA_STS5      (CTL_BASE_AUD_CFGA_RF + 0x0198)
#define ANA_STS6      (CTL_BASE_AUD_CFGA_RF + 0x019C)
#define ANA_STS7      (CTL_BASE_AUD_CFGA_RF + 0x01A0)
#define ANA_STS8      (CTL_BASE_AUD_CFGA_RF + 0x01A4)
#define ANA_STS9      (CTL_BASE_AUD_CFGA_RF + 0x01A8)
#define ANA_STS10     (CTL_BASE_AUD_CFGA_RF + 0x01AC)
#define ANA_STS11     (CTL_BASE_AUD_CFGA_RF + 0x01B0)
#define ANA_STS12     (CTL_BASE_AUD_CFGA_RF + 0x01B4)
#define ANA_STS13     (CTL_BASE_AUD_CFGA_RF + 0x01B8)
#define ANA_STS14     (CTL_BASE_AUD_CFGA_RF + 0x01BC)
#define ANA_RSV12     (CTL_BASE_AUD_CFGA_RF + 0x01C0)
#define ANA_DATO0     (CTL_BASE_AUD_CFGA_RF + 0x01C4)
#define ANA_DATO1     (CTL_BASE_AUD_CFGA_RF + 0x01C8)
#define ANA_DATO2     (CTL_BASE_AUD_CFGA_RF + 0x01CC)
#define ANA_DATO3     (CTL_BASE_AUD_CFGA_RF + 0x01D0)
#define ANA_DATO4     (CTL_BASE_AUD_CFGA_RF + 0x01D4)
#define ANA_DATO5     (CTL_BASE_AUD_CFGA_RF + 0x01D8)
#define ANA_DATO6     (CTL_BASE_AUD_CFGA_RF + 0x01DC)
#define ANA_DATO7     (CTL_BASE_AUD_CFGA_RF + 0x01E0)
#define ANA_DATO8     (CTL_BASE_AUD_CFGA_RF + 0x01E4)
#define ANA_DATO9     (CTL_BASE_AUD_CFGA_RF + 0x01E8)
#define ANA_DATO10    (CTL_BASE_AUD_CFGA_RF + 0x01EC)
#define ANA_DATO11    (CTL_BASE_AUD_CFGA_RF + 0x01F0)
#define ANA_INT0      (CTL_BASE_AUD_CFGA_RF + 0x0200)
#define ANA_INT1      (CTL_BASE_AUD_CFGA_RF + 0x0204)
#define ANA_INT2      (CTL_BASE_AUD_CFGA_RF + 0x0208)
#define ANA_INT3      (CTL_BASE_AUD_CFGA_RF + 0x020C)
#define ANA_INT4      (CTL_BASE_AUD_CFGA_RF + 0x0210)
#define ANA_INT5      (CTL_BASE_AUD_CFGA_RF + 0x0214)
#define ANA_INT6      (CTL_BASE_AUD_CFGA_RF + 0x0218)
#define ANA_INT7      (CTL_BASE_AUD_CFGA_RF + 0x021C)
#define ANA_INT8      (CTL_BASE_AUD_CFGA_RF + 0x0220)
#define ANA_INT9      (CTL_BASE_AUD_CFGA_RF + 0x0224)
#define ANA_INT10     (CTL_BASE_AUD_CFGA_RF + 0x0228)
#define ANA_INT11     (CTL_BASE_AUD_CFGA_RF + 0x022C)
#define ANA_INT12     (CTL_BASE_AUD_CFGA_RF + 0x0230)
#define ANA_INT13     (CTL_BASE_AUD_CFGA_RF + 0x0234)
#define ANA_INT14     (CTL_BASE_AUD_CFGA_RF + 0x0238)
#define ANA_INT15     (CTL_BASE_AUD_CFGA_RF + 0x023C)
#define ANA_INT16     (CTL_BASE_AUD_CFGA_RF + 0x0240)
#define ANA_INT17     (CTL_BASE_AUD_CFGA_RF + 0x0244)
#define ANA_INT18     (CTL_BASE_AUD_CFGA_RF + 0x0248)
#define ANA_INT19     (CTL_BASE_AUD_CFGA_RF + 0x024C)
#define ANA_INT20     (CTL_BASE_AUD_CFGA_RF + 0x0250)
#define ANA_INT21     (CTL_BASE_AUD_CFGA_RF + 0x0254)
#define ANA_INT22     (CTL_BASE_AUD_CFGA_RF + 0x0258)
#define ANA_INT23     (CTL_BASE_AUD_CFGA_RF + 0x025C)
#define ANA_INT24     (CTL_BASE_AUD_CFGA_RF + 0x0260)
#define ANA_INT25     (CTL_BASE_AUD_CFGA_RF + 0x0264)
#define ANA_INT26     (CTL_BASE_AUD_CFGA_RF + 0x0268)
#define ANA_INT27     (CTL_BASE_AUD_CFGA_RF + 0x026C)
#define ANA_INT28     (CTL_BASE_AUD_CFGA_RF + 0x0270)
#define ANA_INT29     (CTL_BASE_AUD_CFGA_RF + 0x0274)
#define ANA_INT30     (CTL_BASE_AUD_CFGA_RF + 0x0278)
#define ANA_INT31     (CTL_BASE_AUD_CFGA_RF + 0x027C)
#define ANA_INT32     (CTL_BASE_AUD_CFGA_RF + 0x0280)
#define ANA_INT33     (CTL_BASE_AUD_CFGA_RF + 0x0284)
#define ANA_INT34     (CTL_BASE_AUD_CFGA_RF + 0x0288)
#define ANA_INT35     (CTL_BASE_AUD_CFGA_RF + 0x028C)
#define DIG_INT4      (CTL_BASE_AUD_CFGA_RF + 0x0290)
#define SPRD_CODEC_AP_END DIG_INT4

#define IS_SPRD_CODEC_AP_RANG(reg) (((reg) >= SPRD_CODEC_AP_BASE - 0x1000 + 0x100) \
	&& ((reg) <= SPRD_CODEC_AP_END))

/*
 * Register Name   : ANA_PMU0
 * Register Offset : 0x0000
 * Description     : analog module
 *                   AUD_BIAS_TOP
 *                   & AUD_CLASSG_TOP_7520
 */

#define VB_EN                   BIT(13)
#define VB_EADBIAS_CTRL         BIT(12)
#define VB_SHPT_PD              BIT(11)
#define VB_SLEEP_EN             BIT(10)
#define BG_EN                   BIT(9)
#define VCMI_SEL                BIT(8)
#define VBG_TEMP_BIASTUNE       BIT(7)
#define VBG_TEMP_TUNE(x)        (((x) & GENMASK(1, 0)) << 5)
#define BIAS_EN                 BIT(4)
#define CP_EN                   BIT(3)
#define CP_AD_EN                BIT(2)
#define CP_LDO_EN               BIT(1)

#define CP_LDO_EN_S             1

/*
 * Register Name   : ANA_PMU1
 * Register Offset : 0x0004
 * Description     : analog module
 *                   AUD_BIAS_TOP
 */

#define HMIC_BIAS_SOFT_T_SEL            BIT(14)
#define HMIC_BIAS_SOFT_EN               BIT(13)
#define HMIC_BIAS_VREF_SEL              BIT(12)
#define HMIC_BIAS_CAPMODE_SEL           BIT(11)
#define MIC3_BIAS_CAPMODE_SEL           BIT(10)
#define MIC2_BIAS_CAPMODE_SEL           BIT(9)
#define MIC1_BIAS_CAPMODE_SEL           BIT(8)
#define HMIC_BIAS_SLEEP_EN              BIT(7)
#define MIC3_BIAS_SLEEP_EN              BIT(6)
#define MIC2_BIAS_SLEEP_EN              BIT(5)
#define MIC1_BIAS_SLEEP_EN              BIT(4)
#define HMIC_BIAS_EN                    BIT(3)
#define MIC3_BIAS_EN                    BIT(2)
#define MIC2_BIAS_EN                    BIT(1)
#define MIC1_BIAS_EN                    BIT(0)

/*
 * Register Name   : ANA_PMU2
 * Register Offset : 0x0008
 * Description     : analog module
 *                   AUD_BIAS_TOP
 */

#define BIAS_RSV1(x)              (((x) & GENMASK(3, 0)) << 12)
#define VB_CAL(x)                 (((x) & GENMASK(5, 0)) << 6)
#define BIAS_CAL_EN               BIT(5)
#define BIAS_CAL(x)               (((x) & GENMASK(4, 0)))

/*
 * Register Name   : ANA_PMU3
 * Register Offset : 0x000C
 * Description     : analog module
 *                   AUD_BIAS_TOP
 *                   & AUD_CLASSG_TOP_7520
 */

#define HMIC_BIAS_MBGREF_S(x)  (((x) & GENMASK(2, 0)) << 12)
#define HMIC_BIAS_SW_EN        BIT(11)
#define BIAS_RSV0(x)          (((x) & GENMASK(5, 0)) << 5)
#define CP_LDO_CAL(x)         (((x) & GENMASK(4, 0)))

/*
 * Register Name   : ANA_PMU4
 * Register Offset : 0x0010
 * Description     : 2730 BST_PA removed
 *                   AUD_VAD_TOP_7520 (OSC)
 */

#define OSC_RESERVED(x)          (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_PMU5
 * Register Offset : 0x0014
 * Description     : analog module
 *                   AUD_BIAS_TOP
 */

#define HMIC_BIAS_V(x)           (((x) & GENMASK(3, 0)) << 12)
#define MIC3_BIAS_V(x)           (((x) & GENMASK(3, 0)) << 8)
#define MIC2_BIAS_V(x)           (((x) & GENMASK(3, 0)) << 4)
#define MIC1_BIAS_V(x)           (((x) & GENMASK(3, 0)))

/*
 * Register Name   : ANA_PMU6
 * Register Offset : 0x0018
 * Description     : 2730 BST_PA removed
 *                   AUD_BIAS_TOP
 */

#define VAD_MIC_VREF_FRC         BIT(13)
#define VAD_MIC_VREF_SEL         BIT(12)
#define MIC_REF_TRIM(x)          (((x) & GENMASK(2, 0)) << 8)
#define VAD_MIC_LDO_SEL(x)       (((x) & GENMASK(2, 0)) << 5)
#define VB_V(x)                  (((x) & GENMASK(4, 0)))

/*
 * Register Name   : ANA_PMU7
 * Register Offset : 0x001C
 */

#define CP_NEG_PD_VNEG         BIT(10)
#define CP_NEG_PD_FLYP         BIT(9)
#define CP_NEG_PD_FLYN         BIT(8)
#define CP_NEG_ET_CMP_BYP_EN   BIT(7)
#define CP_NEG_CLIMIT_EN       BIT(6)
#define CP_NEG_CLIMIT_S(x)     (((x) & GENMASK(1, 0)) << 4)
#define CP_HLIMIT_MOD_SEL      BIT(3)
#define CP_NEG_OCP_DBNC_EN     BIT(2)
#define CP_DISCHARGE_PD        BIT(1)
#define CP_DVAL_EN             BIT(0)

/*
 * Register Name   : ANA_PMU8
 * Register Offset : 0x0020
 * Description     : AUD_CLASSG_TOP_7520 & IMPD
 */

#define CP_IMPD_DETP2_EN        BIT(7)
#define CP_IMPD_DETP1_EN        BIT(6)
#define CP_IMPD_DETN_EN         BIT(5)
#define CP_NEG_DETCMP_HYS_EN    BIT(4)
#define CP_NEG_SHDT_FLYP_EN     BIT(3)
#define CP_NEG_SHDT_FLYN_EN     BIT(2)
#define CP_NEG_SHDT_VCPN_EN     BIT(1)
#define CP_NEG_SHDT_PGSEL       BIT(0)

/*
 * Register Name   : ANA_PMU9
 * Register Offset : 0x0024
 */

#define CP_RSV(x)                  (((x) & GENMASK(7, 0)) << 6)
#define CP_DATO_DUMP_CLK_PN_SEL    BIT(5)
#define CP_DATO_DUMP_TRIG          BIT(4)
#define CP_TEST_OUT_EN             BIT(3)
#define CP_AD_DELAY(x)             (((x) & GENMASK(1, 0)) << 1)
#define CP_AD_VREFP_SEL            BIT(0)

/*
 * Register Name   : ANA_PMU10
 * Register Offset : 0x0028
 */

#define CP_KD_D4(x)        (((x) & GENMASK(3, 0)) << 12)
#define CP_KD_D3(x)        (((x) & GENMASK(3, 0)) << 8)
#define CP_KD_D2(x)        (((x) & GENMASK(3, 0)) << 4)
#define CP_KD_D1(x)        (((x) & GENMASK(3, 0)))

/*
 * Register Name   : ANA_PMU11
 * Register Offset : 0x002C
 */

#define CP_KD_D5(x)         (((x) & GENMASK(3, 0)) << 12)
#define CP_KP_D4(x)         (((x) & GENMASK(2, 0)) << 9)
#define CP_KP_D3(x)         (((x) & GENMASK(2, 0)) << 6)
#define CP_KP_D2(x)         (((x) & GENMASK(2, 0)) << 3)
#define CP_KP_D1(x)         (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_PMU12
 * Register Offset : 0x0030
 */

#define CP_PD_C29(x)        (((x) & GENMASK(1, 0)) << 11)
#define CP_PD_C19(x)        (((x) & GENMASK(1, 0)) << 9)
#define CP_KP_D7(x)         (((x) & GENMASK(2, 0)) << 6)
#define CP_KP_D6(x)         (((x) & GENMASK(2, 0)) << 3)
#define CP_KP_D5(x)         (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_PMU13
 * Register Offset : 0x0034
 */

#define CP_PD_C18(x)       (((x) & GENMASK(1, 0)) << 14)
#define CP_PD_C17(x)       (((x) & GENMASK(1, 0)) << 12)
#define CP_PD_C16(x)       (((x) & GENMASK(1, 0)) << 10)
#define CP_PD_C15(x)       (((x) & GENMASK(1, 0)) << 8)
#define CP_PD_C14(x)       (((x) & GENMASK(1, 0)) << 6)
#define CP_PD_C13(x)       (((x) & GENMASK(1, 0)) << 4)
#define CP_PD_C12(x)       (((x) & GENMASK(1, 0)) << 2)
#define CP_PD_C11(x)       (((x) & GENMASK(1, 0)))

/*
 * Register Name   : ANA_PMU14
 * Register Offset : 0x0038
 */

#define CP_PD_C28(x)       (((x) & GENMASK(1, 0)) << 14)
#define CP_PD_C27(x)       (((x) & GENMASK(1, 0)) << 12)
#define CP_PD_C26(x)       (((x) & GENMASK(1, 0)) << 10)
#define CP_PD_C25(x)       (((x) & GENMASK(1, 0)) << 8)
#define CP_PD_C24(x)       (((x) & GENMASK(1, 0)) << 6)
#define CP_PD_C23(x)       (((x) & GENMASK(1, 0)) << 4)
#define CP_PD_C22(x)       (((x) & GENMASK(1, 0)) << 2)
#define CP_PD_C21(x)       (((x) & GENMASK(1, 0)))

/*
 * Register Name   : ANA_PMU15
 * Register Offset : 0x003C
 */

#define CP_CFG_DQ_EN        BIT(15)
#define CP_CFG_DQ(x)        (((x) & GENMASK(5, 0)) << 9)
#define CP_GT_D3(x)         (((x) & GENMASK(2, 0)) << 6)
#define CP_GT_D2(x)         (((x) & GENMASK(2, 0)) << 3)
#define CP_GT_D1(x)         (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_PMU16
 * Register Offset : 0x0040
 */

#define CP_DREF(x)              (((x) & GENMASK(3, 0)) << 12)
#define CP_HLIMIT_SSTART(x)     (((x) & GENMASK(5, 0)) << 6)
#define CP_HLIMIT_VAL(x)        (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_PMU17
 * Register Offset : 0x0044
 */

#define CP_NEG_OCP_DBNC_NUM(x)     (((x) & GENMASK(10, 0)))

/*
 * Register Name   : ANA_PMU18
 * Register Offset : 0x0048
 */

#define CP_HLIMIT_OCP(x)       (((x) & GENMASK(5, 0)) << 5)
#define CP_POS_V_LRANGE_EN     BIT(4)
#define CP_LDO_CLIMIT_S        BIT(3)
#define CP_LDO_SHPT_PD         BIT(2)
#define CP_LDO_LP_EN           BIT(1)
#define CP_LDO_BYP_EN          BIT(0)

/*
 * Register Name   : ANA_PMU19
 * Register Offset : 0x004C
 */

#define CP_POS_V(x)             (((x) & GENMASK(6, 0)) << 8)
#define CP_NEG_V(x)             (((x) & GENMASK(7, 0)))

/*
 * Register Name   : ANA_DAC0
 * Register Offset : 0x0050
 * Description     : 2730 BST_PA removed
 *                   change to DAC
 *                   AUDIO_DAC_TOP_7520
 *                   analog reg
 */

#define DALDO_EN                   BIT(9)
#define DALDO_BYPASS               BIT(8)
#define DALDO_TRIM(x)              (((x) & GENMASK(3, 0)) << 4)
#define DALDO_V(x)                 (((x) & GENMASK(1, 0)) << 2)
#define DA_IG(x)                   (((x) & GENMASK(1, 0)))

#define DALDO_BYPASS_S              8
#define DA_IG_S                     0

/*
 * Register Name   : ANA_DAC1
 * Register Offset : 0x0054
 * Description     : 2730 BST_PA removed
 *                   change to DAC
 *                   AUDIO_DAC_TOP_7520
 *                   analog reg
 */

#define DAHP_RSV(x)                (((x) & GENMASK(3, 0)) << 9)
#define DAHP_BUF_ITRIM             BIT(8)
#define DAHP_PD_SEL                BIT(7)
#define DAHPL_EN                   BIT(6)
#define DAHPR_EN                   BIT(5)
#define DAHP_OS_EN                 BIT(4)
#define DAHP_OS_INV                BIT(3)
#define DAHP_OS_D(x)               (((x) & GENMASK(2, 0)))

#define DAHPL_EN_S                  6
#define DAHPR_EN_S                  5
#define DAHP_OS_EN_S				4
#define DAHP_OS_INV_S               3
#define DAHP_OS_D_S					0

/*
 * Register Name   : ANA_DAC2
 * Register Offset : 0x0058
 * Description     : 2730 BST_PA removed
 *                   change to DAC
 *                   AUDIO_DAC_TOP_7520
 *                   analog reg
 */

#define DAAO_RSV(x)                (((x) & GENMASK(3, 0)) << 9)
#define DAAO_BUF_ITRIM             BIT(8)
#define DAAO_PD_SEL                BIT(7)
#define DAAOL_EN                   BIT(6)
#define DAAOR_EN                   BIT(5)
#define DAAO_OS_EN                 BIT(4)
#define DAAO_OS_INV                BIT(3)
#define DAAO_OS_D(x)               (((x) & GENMASK(2, 0)))

#define DAAOL_EN_S                 6
#define DAAOR_EN_S                 5


/*
 * Register Name   : ANA_DAC3
 * Register Offset : 0x005C
 * Description     : 2730 BST_PA removed
 *                   change to DAC
 *                   AUDIO_DAC_TOP_7520
 *                   analog reg
 */

#define DAHP_DSEL_L2R              BIT(9)
#define DAHP_DSEL_R2L              BIT(8)
#define DAHPL_VSET_EN              BIT(7)
#define DAHPR_VSET_EN              BIT(6)
#define DAHP_VSET_PN_SEL           BIT(5)
#define DAAO_DSEL_L2R              BIT(4)
#define DAAO_DSEL_R2L              BIT(3)
#define DAAOL_VSET_EN              BIT(2)
#define DAAOR_VSET_EN              BIT(1)
#define DAAO_VSET_PN_SEL           BIT(0)

/*
 * Register Name   : ANA_DAC4
 * Register Offset : 0x0060
 * Description     : 2730 BST_PA removed
 *                   change to DAC
 *                   AUDIO_DAC_TOP_7520 drv_softstart reg
 */

#define HPR_DM_DCVLI_CS(x)         (((x) & GENMASK(2, 0)) << 9)
#define HPR_CM_DCVLI_CS(x)         (((x) & GENMASK(2, 0)) << 6)
#define HPL_DM_DCVLI_CS(x)         (((x) & GENMASK(2, 0)) << 3)
#define HPL_CM_DCVLI_CS(x)         (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_RSV1
 * Register Offset : 0x0064
 */


/*
 * Register Name   : ANA_CLK0
 * Register Offset : 0x0068
 */

#define ANA_CLK_EN        BIT(15)
#define CLK_26M_IN_SEL    BIT(14)
#define CLK_DCL_32K_EN    BIT(13)
#define CLK_DCL_6M5_EN    BIT(12)
#define CLK_DIG_6M5_EN    BIT(11)
#define CLK_IMPD_EN       BIT(10)
#define CLK_ADC_EN        BIT(9)
#define CLK_DAC_EN        BIT(8)
#define CLK_CP_EN         BIT(7)
#define CLK_CP_32K_EN     BIT(6)
/* useless in ump9620
#define CLK_PABST_EN      BIT(5)
#define CLK_PABST_32K_EN  BIT(4)
#define CLK_PACAL_EN      BIT(3)
#define CLK_PA_32K_EN     BIT(2)
#define CLK_PA_DFLCK_EN   BIT(1)
#define CLK_PA_IVSNS_EN   BIT(0)
*/

#define ANA_CLK_EN_S        15
#define CLK_26M_IN_SEL_S    14
#define CLK_DCL_32K_EN_S    13
#define CLK_DCL_6M5_EN_S    12
#define CLK_DIG_6M5_EN_S    11
#define CLK_IMPD_EN_S       10
#define CLK_ADC_EN_S        9
#define CLK_DAC_EN_S        8
#define CLK_CP_EN_S         7
#define CLK_CP_32K_EN_S     6
/* useless in ump9620
#define CLK_PABST_EN_S      5
#define CLK_PABST_32K_EN_S  4
#define CLK_PACAL_EN_S      3
#define CLK_PA_32K_EN_S     2
#define CLK_PA_DFLCK_EN_S   1
#define CLK_PA_IVSNS_EN_S   0
*/
/*
 * Register Name   : ANA_CLK1
 * Register Offset : 0x006C
 */

#define CLK_DAC_F(x)        (((x) & GENMASK(1, 0)) << 5)
#define CLK_PA_DFLCK_F(x)   (((x) & GENMASK(1, 0)) << 3)
#define CLK_ADC_F(x)        (((x) & GENMASK(1, 0)) << 1)
#define CLK_PA_IVSNS_F      BIT(0)

/*
 * Register Name   : ANA_CLK2
 * Register Offset : 0x0070
 */

#define CLK3_RSV(x)            (((x) & GENMASK(1, 0)) << 6)
#define CLK_RST                BIT(5)
#define CLK_ADC_RST            BIT(4)
#define CLK_DCL_32K_PN_SEL     BIT(3)
#define CLK_DIG_6M5_PN_SEL     BIT(2)
#define CLK_DAC_PN_SEL         BIT(1)
#define CLK_ADC_PN_SEL         BIT(0)

/*
 * Register Name   : ANA_CDC0
 * Register Offset : 0x0074
 */

#define ADC_3_RST                     BIT(15)
#define ADC_3_CLK_EN                  BIT(14)
#define ADC_3_EN                      BIT(13)
#define ADPGA_3_EN                    BIT(12)
#define PGA_ADC_IBIAS_EN              BIT(11)
#define PGA_ADC_VCM_VREF_BUF_EN       BIT(10)
#define ADPGA_1_EN                    BIT(9)
#define ADPGA_2_EN                    BIT(8)
#define ADC_1_EN                      BIT(7)
#define ADC_2_EN                      BIT(6)
#define ADC_1_CLK_EN                  BIT(5)
#define ADC_2_CLK_EN                  BIT(4)
#define ADC_1_RST                     BIT(3)
#define ADC_2_RST                     BIT(2)
#define ADC_1_CLK_RST                 BIT(1)
#define ADC_2_CLK_RST                 BIT(0)

#define ADC_3_RST_S                     15
#define ADC_3_CLK_EN_S                  14
#define ADC_3_EN_S                      13
#define ADPGA_3_EN_S                    12
#define PGA_ADC_IBIAS_EN_S              11
#define PGA_ADC_VCM_VREF_BUF_EN_S       10
#define ADPGA_1_EN_S                    9
#define ADPGA_2_EN_S                    8
#define ADC_1_EN_S                      7
#define ADC_2_EN_S                      6
#define ADC_1_CLK_EN_S                  5
#define ADC_2_CLK_EN_S                  4
#define ADC_1_RST_S                     3
#define ADC_2_RST_S                     2
#define ADC_1_CLK_RST_S                 1
#define ADC_2_CLK_RST_S                 0

/*
 * Register Name   : ANA_CDC1
 * Register Offset : 0x0078
 */

#define ADC_3_CLK_RST              BIT(15)
#define SHMICPGA_3                 BIT(5)
#define SMIC3PGA_3                 BIT(4)
#define SHMICPGA_1                 BIT(3)
#define SHMICPGA_2                 BIT(2)
#define SMIC1PGA_1                 BIT(1)
#define SMIC2PGA_2                 BIT(0)

#define ADC_3_CLK_RST_S              15
#define SHMICPGA_3_S                 5
#define SMIC3PGA_3_S                 4
#define SHMICPGA_1_S                 3
#define SHMICPGA_2_S                 2
#define SMIC1PGA_1_S                 1
#define SMIC2PGA_2_S                 0
/*
 * Register Name   : ANA_CDC2
 * Register Offset : 0x007C
 * Description     : AUD_ADPGA_TOP_7520
 *                   & AUD_VAD_TOP_7520
 */

#define LATCH_BIAS_SEL_3V3         BIT(13)
#define VAD_VCM_VEF_BUF_EN         BIT(12)
#define ADPGA_3_G(x)               (((x) & GENMASK(2, 0)) << 6)
#define ADPGA_1_G(x)               (((x) & GENMASK(2, 0)) << 3)
#define ADPGA_2_G(x)               (((x) & GENMASK(2, 0)))

#define LATCH_BIAS_SEL_3V3_S         13
#define VAD_VCM_VEF_BUF_EN_S         12
#define ADPGA_3_G_S               6
#define ADPGA_1_G_S               3
#define ADPGA_2_G_S               0
/*
 * Register Name   : ANA_CDC3
 * Register Offset : 0x0080
 */

#define PGA_ADC_IBIAS_SEL(x)       (((x) & GENMASK(3, 0)) << 12)
#define PGA_ADC_VCMI_V(x)          (((x) & GENMASK(1, 0)) << 10)
#define ADC_VREFP_V(x)             (((x) & GENMASK(1, 0)) << 8)
#define ADPGA_1_BYP(x)             (((x) & GENMASK(1, 0)) << 6)
#define ADPGA_2_BYP(x)             (((x) & GENMASK(1, 0)) << 4)
#define ADPGA_1_CAP(x)             (((x) & GENMASK(1, 0)) << 2)
#define ADPGA_2_CAP(x)             (((x) & GENMASK(1, 0)))

/*
 * Register Name   : ANA_CDC4
 * Register Offset : 0x0084
 */

#define ADPGA_3_BYP(x)             (((x) & GENMASK(1, 0)) << 14)
#define ADPGA_3_CAP(x)             (((x) & GENMASK(1, 0)) << 12)
#define ADC_3_DAT_GATE_EN          BIT(9)
#define ADC_3_DAT_INV              BIT(8)
#define ADC_1_DAT_GATE_EN          BIT(7)
#define ADC_2_DAT_GATE_EN          BIT(6)
#define ADC_1_DAT_INV              BIT(5)
#define ADC_2_DAT_INV              BIT(4)
#define HMIC_DPOP_EN               BIT(3)
#define HMIC_DPOP_VCM_EN           BIT(2)
#define ADC_VREF_SFCUR             BIT(1)
#define ADC_BULKSW                 BIT(0)

/*
 * Register Name   : ANA_CDC5
 * Register Offset : 0x0088
 * Description     : AUDIO_DAC_TOP_7520
 *                   dac_buf_cal
 */

#define DABUF_DCCAL_T(x)           (((x) & GENMASK(1, 0)) << 14)
#define DABUF_DCCAL_CNT(x)         (((x) & GENMASK(1, 0)) << 12)
#define DAHPL_BUF_DCCAL_EN         BIT(11)
#define DAHPL_BUF_CAL_MODE_SEL     BIT(10)
#define DAHPL_BUF_CAL_CLR          BIT(9)
#define DAHPR_BUF_DCCAL_EN         BIT(8)
#define DAHPR_BUF_CAL_MODE_SEL     BIT(7)
#define DAHPR_BUF_CAL_CLR          BIT(6)
#define DAAOL_BUF_DCCAL_EN         BIT(5)
#define DAAOL_BUF_CAL_MODE_SEL     BIT(4)
#define DAAOL_BUF_CAL_CLR          BIT(3)
#define DAAOR_BUF_DCCAL_EN         BIT(2)
#define DAAOR_BUF_CAL_MODE_SEL     BIT(1)
#define DAAOR_BUF_CAL_CLR          BIT(0)

#define DABUF_DCCAL_T_S           14
#define DABUF_DCCAL_CNT_S         12
#define DAHPL_BUF_DCCAL_EN_S         11
#define DAHPL_BUF_CAL_MODE_SEL_S     10
#define DAHPL_BUF_CAL_CLR_S          9
#define DAHPR_BUF_DCCAL_EN_S         8
#define DAHPR_BUF_CAL_MODE_SEL_S     7
#define DAHPR_BUF_CAL_CLR_S          6
#define DAAOL_BUF_DCCAL_EN_S         5
#define DAAOL_BUF_CAL_MODE_SEL_S     4
#define DAAOL_BUF_CAL_CLR_S          3
#define DAAOR_BUF_DCCAL_EN_S         2
#define DAAOR_BUF_CAL_MODE_SEL_S     1
#define DAAOR_BUF_CAL_CLR_S          0

/*
 * Register Name   : ANA_CDC6
 * Register Offset : 0x008C
 * Description     : AUDIO_DAC_TOP_7520
 *                   dac_buf_cal
 */

#define DA_BUF_CALO_SEL(x)         (((x) & GENMASK(1, 0)))

/*
 * Register Name   : ANA_CDC7
 * Register Offset : 0x0090
 * Description     : AUDIO_DAC_TOP_7520
 *                   dac_buf_cal
 */

#define DAHPL_VRPBUF_CAL_DONE      BIT(7)
#define DAHPL_VRNBUF_CAL_DONE      BIT(6)
#define DAHPR_VRPBUF_CAL_DONE      BIT(5)
#define DAHPR_VRNBUF_CAL_DONE      BIT(4)
#define DAAOL_VRPBUF_CAL_DONE      BIT(3)
#define DAAOL_VRNBUF_CAL_DONE      BIT(2)
#define DAAOR_VRPBUF_CAL_DONE      BIT(1)
#define DAAOR_VRNBUF_CAL_DONE      BIT(0)

#define DAHPL_VRPBUF_CAL_DONE_S    7
#define DAHPL_VRNBUF_CAL_DONE_S    6
#define DAHPR_VRPBUF_CAL_DONE_S    5
#define DAHPR_VRNBUF_CAL_DONE_S    4
#define DAAOL_VRPBUF_CAL_DONE_S    3
#define DAAOL_VRNBUF_CAL_DONE_S    2
#define DAAOR_VRPBUF_CAL_DONE_S    1
#define DAAOR_VRNBUF_CAL_DONE_S    0

/*
 * Register Name   : ANA_CDC8
 * Register Offset : 0x0094
 */

#define AO_G(x)          (((x) & GENMASK(3, 0)) << 12)
#define RCV_G(x)         (((x) & GENMASK(3, 0)) << 8)
#define HPL_G(x)         (((x) & GENMASK(3, 0)) << 4)
#define HPR_G(x)         (((x) & GENMASK(3, 0)))
#define AO_G_S           12
#define RCV_G_S          8
#define HPL_G_S          4
#define HPR_G_S          0

/*
 * Register Name   : ANA_CDC9
 * Register Offset : 0x0098
 */

#define SDAAOL_AO                  BIT(5)
#define SDAAOR_AO                  BIT(4)
#define SDALHPL_HPL                BIT(3)
#define SDARHPR_HPR                BIT(2)
#define SDAAOL_RCV                 BIT(1)
#define SDAHPL_RCV                 BIT(0)

#define SDAAOL_AO_S                  5
#define SDAAOR_AO_S                  4
#define SDALHPL_HPL_S                3
#define SDARHPR_HPR_S                2
#define SDAAOL_RCV_S                 1
#define SDAHPL_RCV_S                 0

/*
 * Register Name   : ANA_CDC10
 * Register Offset : 0x009C
 */

#define HPL_EN                     BIT(3)
#define HPR_EN                     BIT(2)
#define RCV_EN                     BIT(1)
#define AO_EN                      BIT(0)

#define HPL_EN_S    3
#define HPR_EN_S    2
#define RCV_EN_S    1
#define AO_EN_S     0
/*
 * Register Name   : ANA_CDC11
 * Register Offset : 0x00A0
 */

#define HP_IB_SEL(x)               (((x) & GENMASK(1, 0)) << 13)
#define HP_OPA_IF0                 BIT(12)
#define HP_OPA_RF1                 BIT(11)
#define HP_OPA_IF1                 BIT(10)
#define HP_OPA_IF3(x)              (((x) & GENMASK(3, 0)) << 6)
#define HPL_G_BP_EN                BIT(5)
#define HPR_G_BP_EN                BIT(4)
#define HP_BIAS_CUR_SEL            BIT(3)
#define HP_CAL_CUR_SEL             BIT(2)
#define HP_BIAS_CUR_TUNE(x)        (((x) & GENMASK(1, 0)))

/*
 * Register Name   : ANA_CDC12
 * Register Offset : 0x00A4
 */

#define HP_RSV(x)                  (((x) & GENMASK(4, 0)) << 11)
#define HPSW_NEGV_SEL              BIT(10)
#define HP_OCP_PD                  BIT(9)
#define HP_OCP_MODE(x)             (((x) & GENMASK(1, 0)) << 7)
#define CP_HPEN_DLY_T(x)           (((x) & GENMASK(1, 0)) << 5)
#define DRV_CPDLY_PD               BIT(4)
#define DRV_SOFT_EN                BIT(3)
#define DRV_SOFT_T(x)              (((x) & GENMASK(2, 0)))

#define DRV_SOFT_EN_S               3
#define DRV_SOFT_T_S                0

/*
 * Register Name   : ANA_CDC13
 * Register Offset : 0x00A8
 */

#define RCV_RSV(x)                 (((x) & GENMASK(5, 0)) << 5)
#define RCV_OCP_PD                 BIT(4)
#define RCV_OCP_ITH                BIT(3)
#define RCV_IB_SEL(x)              (((x) & GENMASK(1, 0)) << 1)
#define RCV_CUR                    BIT(0)

/*
 * Register Name   : ANA_CDC14
 * Register Offset : 0x00AC
 */

#define AO_RSV(x)                  (((x) & GENMASK(5, 0)) << 5)
#define AO_OCP_PD                  BIT(4)
#define AO_OCP_ITH                 BIT(3)
#define AO_IB_SEL(x)               (((x) & GENMASK(1, 0)) << 1)
#define AO_CUR                     BIT(0)

#define AO_CUR_S                   0

/*
 * Register Name   : ANA_CDC15
 * Register Offset : 0x00B0
 * Description     : AUDIO_DAC_TOP_7520
 *                   dac_buf_cal
 */

#define DAL_VRPBUF_CALI(x)         (((x) & GENMASK(8, 0)))

/*
 * Register Name   : ANA_CDC16
 * Register Offset : 0x00B4
 * Description     : AUDIO_DAC_TOP_7520
 *                   dac_buf_cal
 */

#define DAL_VRNBUF_CALI(x)         (((x) & GENMASK(8, 0)))

/*
 * Register Name   : ANA_CDC17
 * Register Offset : 0x00B8
 * Description     : AUDIO_DAC_TOP_7520
 *                   dac_buf_cal
 */

#define DAR_VRPBUF_CALI(x)         (((x) & GENMASK(8, 0)))

/*
 * Register Name   : ANA_CDC18
 * Register Offset : 0x00BC
 * Description     : AUDIO_DAC_TOP_7520
 *                   dac_buf_cal
 */

#define DAR_VRNBUF_CALI(x)         (((x) & GENMASK(8, 0)))

/*
 * Register Name   : ANA_CDC19
 * Register Offset : 0x00C0
 * Description     : AUDIO_DAC_TOP_7520
 *                   dac_buf_cal
 */

#define DA_VRPBUF_CALO(x)          (((x) & GENMASK(8, 0)))

/*
 * Register Name   : ANA_CDC20
 * Register Offset : 0x00C4
 * Description     : AUDIO_DAC_TOP_7520
 *                   dac_buf_cal
 */

#define DA_VRNBUF_CALO(x)          (((x) & GENMASK(8, 0)))

/*
 * Register Name   : ANA_CDC21
 * Register Offset : 0x00C8
 * Description     : 2730 BST_PA removed
 *                   change to
 *                   AUD_VAD_TOP_7520
 */

#define VAD_AD_BULKSW              BIT(15)
#define VAD_ADPGA_VCMI_V(x)        (((x) & GENMASK(1, 0)) << 13)
#define VAD_ADPGA_IBIAS_SEL(x)     (((x) & GENMASK(3, 0)) << 9)
#define VAD_ADPGA_IBIAS_EN         BIT(8)
#define VAD_ADC_VREF_SFCUR         BIT(7)
#define VAD_ADC_VREFP_V(x)         (((x) & GENMASK(1, 0)) << 5)
#define VAD_ADC_RST                BIT(4)
#define VAD_ADC_EN                 BIT(3)
#define VAD_ADC_DAT_INV            BIT(2)
#define VAD_LOWPOWER(x)            (((x) & GENMASK(1, 0)))

/*
 * Register Name   : ANA_RSV3
 * Register Offset : 0x00CC
 * Description     : RSV3
 *                   change to
 *                   AUD_VAD_TOP_7520
 */

#define VAD_SMIC3                  BIT(15)
#define VAD_SMIC2                  BIT(14)
#define VAD_SMIC1                  BIT(13)
#define VAD_PGA_G(x)               (((x) & GENMASK(2, 0)) << 10)
#define VAD_PGA_EN                 BIT(9)
#define VAD_PGA_CAP(x)             (((x) & GENMASK(1, 0)) << 7)
#define VAD_PGA_BYP(x)             (((x) & GENMASK(1, 0)) << 5)
#define VAD_HMIC_DPOP_VCM_EN       BIT(4)
#define VAD_HMIC_DPOP              BIT(3)
#define VAD_AD_D_GATE              BIT(2)
#define VAD_AD_CLK_RST             BIT(1)
#define VAD_AD_CLK_EN              BIT(0)

/*
 * Register Name   : ANA_HDT0
 * Register Offset : 0x00D0
 */

#define HEDET_VREF_EN          BIT(15)
#define HEDET_JACK_TYPE        BIT(14)
#define HEDET_BDET_EN          BIT(13)
#define HEDET_BDET_HYS_SEL(x)  (((x) & GENMASK(1, 0)) << 11)
#define HEDET_BDET_REF_SEL(x)  (((x) & GENMASK(2, 0)) << 8)
#define HEDET_GDET_EN          BIT(7)
#define HEDET_GDET_REF_SEL(x)  (((x) & GENMASK(2, 0)) << 4)
#define HEDET_GDET_I_SEL(x)    (((x) & GENMASK(3, 0)))

/*
 * Register Name   : ANA_HDT1
 * Register Offset : 0x00D4
 */

#define HEDET_PLGPD_EN        BIT(13)
#define HEDET_LDET_CMP_SEL    BIT(12)
#define HEDET_LDET_I_SEL(x)   (((x) & GENMASK(3, 0)) << 8)
#define HEDET_LDRVEN_SEL      BIT(7)
#define HEDET_LDRV_EN         BIT(6)
#define HEDET_HPL_PULLD_EN    BIT(5)
#define HEDET_HPL_RSEL(x)     (((x) & GENMASK(1, 0)) << 3)
#define HEDET_HPR_PULLD_EN    BIT(2)
#define HEDET_HPR_RSEL(x)     (((x) & GENMASK(1, 0)))

/*
 * Register Name   : ANA_HDT2
 * Register Offset : 0x00D8
 */

#define HEDET_MDET_EN          BIT(15)
#define HEDET_MDET_HYS_SEL(x)  (((x) & GENMASK(1, 0)) << 13)
#define HEDET_MDET_REF_SEL(x)  (((x) & GENMASK(2, 0)) << 10)
#define HEDET_LDETH_EN         BIT(9)
#define HEDET_LDETH_HYS_SEL(x) (((x) & GENMASK(1, 0)) << 7)
#define HEDET_LDETH_REF_SEL(x) (((x) & GENMASK(1, 0)) << 5)
#define HEDET_LDETL_EN         BIT(4)
#define HEDET_LDETL_FLT_EN     BIT(3)
#define HEDET_LDETL_REF_SEL(x) (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_HDT3
 * Register Offset : 0x00DC
 */

#define HPL_EN_D2HDT_EN         BIT(9)
#define HPL_EN_D2HDT_T(x)       (((x) & GENMASK(1, 0)) << 7)
#define HEDET_V2AD_EN           BIT(6)
#define HEDET_V2AD_SCALE_SEL    BIT(5)
#define HEDET_V2AD_SWAP         BIT(4)
#define HEDET_V2AD_CH_SEL(x)    (((x) & GENMASK(3, 0)))

/*
 * Register Name   : ANA_HDT4
 * Register Offset : 0x00E0
 */

#define HEDET_MDET_VREF_SEL          BIT(9)
#define HEDET_GDET_BIST_EN           BIT(8)
#define HEDET_LDET_BIST_EN           BIT(7)
#define HEDET_MDET_BIST_EN           BIT(6)
#define HEDET_GDET_BIST_REF_SEL(x)   (((x) & GENMASK(2, 0)) << 3)
#define HEDET_LDET_BIST_REF_SEL(x)   (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_IMPD0
 * Register Offset : 0x00E4
 */

#define IMPD_CH_SEL(x)        (((x) & GENMASK(3, 0)) << 8)
#define IMPD_ADC_EN           BIT(7)
#define IMPD_ADC_CHOP_EN      BIT(6)
#define IMPD_ADC_RST          BIT(5)
#define IMPD_ADC_SHORT_EN     BIT(4)
#define IMPD_ADC_VCMI_SEL     BIT(3)
#define IMPD_BUF_EN           BIT(2)
#define IMPD_BUF_CHOP_EN      BIT(1)
#define IMPD_BUF_SWAP         BIT(0)

/*
 * Register Name   : ANA_IMPD1
 * Register Offset : 0x00E8
 */

#define IMPD_ADC_CLK_SEL(x)   (((x) & GENMASK(1, 0)) << 9)
#define IMPD_CHOP_CLK_F(x)    (((x) & GENMASK(1, 0)) << 7)
#define IMPD_R1(x)            (((x) & GENMASK(2, 0)) << 4)
#define IMPD_RREF(x)          (((x) & GENMASK(1, 0)) << 2)
#define IMPD_STEP_I(x)        (((x) & GENMASK(1, 0)))

/*
 * Register Name   : ANA_IMPD2
 * Register Offset : 0x00EC
 */

#define IMPD_BIST_LIMIT_VAL(x)   (((x) & GENMASK(8, 0)) << 7)
#define IMPD_COUNT_AGAIN         BIT(6)
#define IMPD_CUR_EN              BIT(5)
#define IMPD_STEP_T(x)           (((x) & GENMASK(1, 0)) << 3)
#define IMPD_TIME_CNT_SEL(x)     (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_IMPD3
 * Register Offset : 0x00F0
 */

#define IMPD_CLK_EDGE_SEL      BIT(14)
#define IMPD_ATE_CUR_EN        BIT(13)
#define IMPD_BIST_EN           BIT(12)
#define IMPD_BIST_BYP_EN       BIT(11)
#define IMPD_DIG_BIST_EN       BIT(10)
#define IMPD_BIST_TIME_SEL(x)  (((x) & GENMASK(1, 0)) << 8)
#define IMPD_BIST_LIMIT(x)     (((x) & GENMASK(7, 0)))

/*
 * Register Name   : ANA_IMPD4
 * Register Offset : 0x00F4
 */

#define IMPD_BIST_ADC_DATO(x)     (((x) & GENMASK(9, 0)))

/*
 * Register Name   : ANA_RSV5
 * Register Offset : 0x00F8
 */


/*
 * Register Name   : ANA_DCL0
 * Register Offset : 0x00FC
 */

#define RSTN_AUD_DCL_32K           BIT(15)
#define RSTN_AUD_HID               BIT(14)
#define RSTN_AUD_HID_DBNC          BIT(12)
#define RSTN_AUD_DIG_DRV_SOFT      BIT(10)
#define RSTN_AUD_HPDPOP            BIT(9)
#define RSTN_AUD_CFGA_PRT          BIT(8)
#define RSTN_AUD_DIG_IMPD_ADC      BIT(7)
#define RSTN_AUD_CP_DCL            BIT(6)
#define RSTN_AUD_CP_ADCAL          BIT(5)
#define RSTN_AUD_CP_DAT_DUMP       BIT(4)
#define RG_RSTN_AUD_OSC            BIT(3)
#define RG_RSTN_DAC_BUF            BIT(2)
#define RG_RSTN_AUD_RCV            BIT(1)
#define RSTN_AUD_DIG_DCL           BIT(0)

/*
 * Register Name   : ANA_DCL1
 * Register Offset : 0x0100
 */

#define DIG_OSC_4M_EN              BIT(15)
#define DIG_OSC_LDO_EN             BIT(14)
#define DIG_CLK_OSC_EN             BIT(13)
#define DIG_CLK_DAC_BUF_EN         BIT(12)
#define DIG_CLK_RCV_EN             BIT(11)
#define RSTN_AUD_DIG_INTC          BIT(10)
#define DCL_EN                     BIT(9)
#define DIG_CLK_INTC_EN            BIT(8)
#define DIG_CLK_HID_EN             BIT(7)
#define DIG_CLK_CFGA_PRT_EN        BIT(6)
#define DIG_CLK_HPDPOP_EN          BIT(4)
#define DIG_CLK_IMPD_EN            BIT(3)
#define DIG_CLK_DRV_SOFT_EN        BIT(1)

#define DIG_OSC_4M_EN_S              15
#define DIG_OSC_LDO_EN_S             14
#define DIG_CLK_OSC_EN_S             13
#define DIG_CLK_DAC_BUF_EN_S         12
#define DIG_CLK_RCV_EN_S             11
#define RSTN_AUD_DIG_INTC_S          10
#define DCL_EN_S                     9
#define DIG_CLK_INTC_EN_S            8
#define DIG_CLK_HID_EN_S             7
#define DIG_CLK_CFGA_PRT_EN_S        6
#define DIG_CLK_HPDPOP_EN_S          4
#define DIG_CLK_IMPD_EN_S            3
#define DIG_CLK_DRV_SOFT_EN_S        1

/*
 * Register Name   : ANA_DCL2
 * Register Offset : 0x0104
 * Description     : AUD_DCL_FGU remove
 *                   AUD_VAD_TOP_7520
 *                   OSC_2M
 */

#define CLK_2M_OUT_SEL(x)          (((x) & GENMASK(1, 0)) << 14)
#define OSC_4M_CAL_EN(x)           (((x) & GENMASK(1, 0)) << 12)
#define OSC_4M_CAL(x)              (((x) & GENMASK(11, 0)))

/*
 * Register Name   : ANA_DCL3
 * Register Offset : 0x0108
 * Description     : AUD_DCL_FGU remove
 *                   AUD_VAD_TOP_7520
 *                   OSC_2M
 */

#define OSC_4M_CY_N3(x)            (((x) & GENMASK(7, 0)) << 8)
#define OSC_4M_CY_N2(x)            (((x) & GENMASK(7, 0)))

/*
 * Register Name   : ANA_DCL4
 * Register Offset : 0x010C
 * Description     : AUD_DCL_FGU remove
 *                   AUD_VAD_TOP_7520
 *                   OSC_2M
 */

#define OSC_LDO_4M_BIAS_SEL(x)        (((x) & GENMASK(1, 0)) << 14)
#define OSC_4M_N1(x)                  (((x) & GENMASK(8, 0)) << 5)
#define OSC_4M_CY_N1(x)               (((x) & GENMASK(4, 0)))

/*
 * Register Name   : ANA_DCL5
 * Register Offset : 0x0110
 * Description     : AUD_CLASSG_TOP_7520
 *                   depop
 */

#define HPL_CAL_RSV                   BIT(10)
#define HPR_CAL_RSV                   BIT(9)
#define RCV_DPOP_BPS              BIT(8)
#define RCV_DAC_FADE_BPS          BIT(7)
#define HP_DPOP_EN                BIT(6)
#define HP_DPOP_AUTO_EN           BIT(5)
#define HP_DPOP_FDIN_EN           BIT(4)
#define HP_DPOP_FDOUT_EN          BIT(3)
#define HP_DPOP_CHG_OFF_PD        BIT(2)
#define HPL_DPOP_CHG              BIT(1)
#define HPR_DPOP_CHG              BIT(0)

/*
 * Register Name   : ANA_DCL6
 * Register Offset : 0x0114
 * Description     : AUD_CLASSG_TOP_7520
 *                   depop
 */

#define RCV_ICAL_IBSEL               BIT(15)
#define RCV_ICAL_RSL_INI(x)          (((x) & GENMASK(1, 0)) << 13)
#define RCV_DUM_BPS                  BIT(12)
#define RCV_DUM_T(x)                 (((x) & GENMASK(2, 0)) << 9)
#define RCV_DAC_FADE_STEP_N1(x)      (((x) & GENMASK(2, 0)) << 6)
#define RCV_DAC_FADE_STEP_N2(x)      (((x) & GENMASK(2, 0)) << 3)
#define RCV_DAC_FADE_STEP_T(x)       (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_DCL7
 * Register Offset : 0x0118
 * Description     : AUD_CLASSG_TOP_7520
 *                   depop
 */

#define HPL_CM_DCCAL_SEL(x)           (((x) & GENMASK(2, 0)) << 13)
#define HPR_CM_DCCAL_SEL(x)           (((x) & GENMASK(2, 0)) << 10)
#define HPL_DM_DCCAL_SEL(x)           (((x) & GENMASK(2, 0)) << 7)
#define HPR_DM_DCCAL_SEL(x)           (((x) & GENMASK(2, 0)) << 4)
#define HP_DCV_VAL1(x)                (((x) & GENMASK(3, 0)))

/*
 * Register Name   : ANA_DCL8
 * Register Offset : 0x011C
 * Description     : AUD_CLASSG_TOP_7520
 *                   depop
 */

#define HP_DCV_VAL2(x)   (((x) & GENMASK(3, 0)) << 10)
#define HP_DPOP_T1(x)    (((x) & GENMASK(1, 0)) << 8)
#define HP_DPOP_T2(x)    (((x) & GENMASK(1, 0)) << 6)
#define HP_DPOP_TH(x)    (((x) & GENMASK(1, 0)) << 4)
#define HP_DPOP_TO(x)    (((x) & GENMASK(1, 0)) << 2)
#define HP_DPOP_AVG(x)   (((x) & GENMASK(1, 0)))

/*
 * Register Name   : ANA_DCL9
 * Register Offset : 0x0120
 * Description     : AUD_CLASSG_TOP_7520
 *                   depop
 */

#define HP_DPOP_GAIN_N1(x)     (((x) & GENMASK(2, 0)) << 6)
#define HP_DPOP_GAIN_N2(x)     (((x) & GENMASK(2, 0)) << 3)
#define HP_DPOP_GAIN_T(x)      (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_DCL10
 * Register Offset : 0x0124
 */

#define CP_AD_CMP_CAL_AVG(x)         (((x) & GENMASK(1, 0)) << 14)
#define CP_AD_CMP_CAL_EN             BIT(13)
#define CP_AD_CMP_AUTO_CAL_EN        BIT(12)
#define CP_POS_SOFT_EN               BIT(11)
#define CP_POS_SOFT_T(x)             (((x) & GENMASK(1, 0)) << 9)
#define CP_POS_SOFT_VSTEP(x)         (((x) & GENMASK(2, 0)) << 6)
#define CP_NEG_SOFT_EN               BIT(5)
#define CP_NEG_SOFT_T(x)             (((x) & GENMASK(1, 0)) << 3)
#define CP_NEG_SOFT_VSTEP(x)         (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_DCL11
 * Register Offset : 0x0128
 */

#define CP_AD_CMP_AUTO_CAL_RANGE  BIT(15)
#define CP_VREF_AUTO_PD(x)        (((x) & GENMASK(1, 0)) << 13)
#define CP_POS_ET_FORCE_EN        BIT(12)
#define CP_POS_ET_T(x)            (((x) & GENMASK(1, 0)) << 10)
#define CP_POS_ET_USTEP(x)        (((x) & GENMASK(1, 0)) << 8)
#define CP_POS_ET_DSTEP(x)        (((x) & GENMASK(6, 0)))

/*
 * Register Name   : ANA_DCL12
 * Register Offset : 0x012C
 */

#define CP_NEG_ET_FORCE_EN   BIT(13)
#define CP_NEG_ET_T(x)       (((x) & GENMASK(1, 0)) << 11)
#define CP_NEG_ET_USTEP(x)   (((x) & GENMASK(2, 0)) << 8)
#define CP_NEG_ET_DSTEP(x)   (((x) & GENMASK(7, 0)))

/*
 * Register Name   : ANA_DCL13
 * Register Offset : 0x0130
 */

#define CP_POS_HV(x)               (((x) & GENMASK(6, 0)) << 7)
#define CP_POS_LV(x)               (((x) & GENMASK(6, 0)))

/*
 * Register Name   : ANA_DCL14
 * Register Offset : 0x0134
 */

#define CP_NEG_HV(x)          (((x) & GENMASK(7, 0)) << 8)
#define CP_NEG_LV(x)          (((x) & GENMASK(7, 0)))

/*
 * Register Name   : ANA_DCL15
 * Register Offset : 0x0138
 * Description     : AUD_CLASSG_TOP_7520
 *                   depop
 */

#define RCV_DPOP_WAIT_T(x)           (((x) & GENMASK(2, 0)) << 12)
#define RCV_ICAL_T(x)                (((x) & GENMASK(1, 0)) << 10)
#define RCV_ICAL_LIMIT_L(x)          (((x) & GENMASK(4, 0)) << 5)
#define RCV_ICAL_LIMIT_H(x)          (((x) & GENMASK(4, 0)))

/*
 * Register Name   : ANA_RSV7
 * Register Offset : 0x013C
 * Description     : RSV7 change to
 *                   AUD_VAD_TOP_7520
 *                   OSC_2M
 */

#define OSC_4M_FSM_CALVAL_SEL        BIT(15)
#define OSC_4M_TMP_CAL_Z(x)          (((x) & GENMASK(2, 0)) << 12)
#define OSC_4M_N2(x)                 (((x) & GENMASK(11, 0)))

/*
 * Register Name   : ANA_RSV8
 * Register Offset : 0x0140
 * Description     : RSV8 change to
 *                   AUD_VAD_TOP_7520
 *                   OSC_2M
 */

#define OSC_4M_N3(x)              (((x) & GENMASK(11, 0)))

/*
 * Register Name   : ANA_HID0
 * Register Offset : 0x0144
 */

#define HID_EN                   BIT(14)
#define HID_DBNC_EN(x)           (((x) & GENMASK(1, 0)) << 12)
#define HID_TMR_CLK_SEL(x)       (((x) & GENMASK(1, 0)) << 10)
#define HID_TMR_T0(x)            (((x) & GENMASK(4, 0)) << 5)
#define HID_TMR_T1T2_STEP(x)     (((x) & GENMASK(4, 0)))

/*
 * Register Name   : ANA_HID1
 * Register Offset : 0x0148
 */

#define HID_HIGH_DBNC_THD0(x)    (((x) & GENMASK(7, 0)) << 8)
#define HID_LOW_DBNC_THD0(x)     (((x) & GENMASK(7, 0)))

/*
 * Register Name   : ANA_HID2
 * Register Offset : 0x014C
 * Description     : ANA_HID2
 */

#define HID_HIGH_DBNC_THD1(x)     (((x) & GENMASK(7, 0)) << 8)
#define HID_LOW_DBNC_THD1(x)      (((x) & GENMASK(7, 0)))

/*
 * Register Name   : ANA_HID3
 * Register Offset : 0x0150
 */

#define HID_TMR_T1(x)    (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_HID4
 * Register Offset : 0x0154
 */

#define HID_TMR_T2(x)      (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_CFGA0
 * Register Offset : 0x0158
 */

#define PRT_EN                BIT(5)
#define CFGA_PRT_CLK_SEL      BIT(4)
#define CFGA_HPL_SHUTDOWN_EN  BIT(3)
#define CFGA_HPR_SHUTDOWN_EN  BIT(2)
#define CFGA_EAR_SHUTDOWN_EN  BIT(1)
#define CFGA_AO_SHUTDOWN_EN   BIT(0)

/*
 * Register Name   : ANA_CFGA1
 * Register Offset : 0x015C
 */

#define CFGA_HPL_SHUTDOWN_CLR     BIT(3)
#define CFGA_HPR_SHUTDOWN_CLR     BIT(2)
#define CFGA_EAR_SHUTDOWN_CLR     BIT(1)
#define CFGA_AO_SHUTDOWN_CLR      BIT(0)


/*
 * Register Name   : ANA_CFGA2
 * Register Offset : 0x0160
 */

#define CFGA_OCP_PD_THD(x)     (((x) & GENMASK(2, 0)) << 3)
#define CFGA_OCP_PRECIS(x)     (((x) & GENMASK(2, 0)))

/*
 * Register Name   : ANA_WR_PROT0
 * Register Offset : 0x0164
 * Description     : ANA_WR_PROT0
 *                   PABST remove
 */


/*
 * Register Name   : ANA_RSV9
 * Register Offset : 0x0168
 */


/*
 * Register Name   : ANA_DBG0
 * Register Offset : 0x016C
 * Description     : ANA_DBG0
 *                   PABST remove
 */


/*
 * Register Name   : ANA_DBG1
 * Register Offset : 0x0170
 */

#define HP_DPOP_S(x)             (((x) & 0x7) << 13)
#define CP_AD_COMP_CAL_IC(x)     (((x) & GENMASK(12, 0)))

/*
 * Register Name   : ANA_DBG2
 * Register Offset : 0x0174
 */

#define HP_DCV_IH(x)         (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_DBG3
 * Register Offset : 0x0178
 * Description     : ANA_DBG3
 */

#define HP_DCV_IL(x)        (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_DBG4
 * Register Offset : 0x017C
 * Description     : ANA_DBG4
 *
 *                   BST PA removed
 *                   change to RCVDPOP
 */

#define RCV_ICAL_EN                BIT(9)
#define RCV_DPOP_EN                BIT(8)
#define RCV_AB_ICAL_PNSL           BIT(7)
#define RCV_ICAL_RSL(x)            (((x) & GENMASK(1, 0)) << 5)
#define RCV_ICAL(x)                (((x) & GENMASK(4, 0)))


#define RCV_DPOP_EN_S               8


/*
 * Register Name   : ANA_DBG5
 * Register Offset : 0x0180
 * Description     : ANA_DBG5
 *                   BST PA removed
 */


/*
 * Register Name   : ANA_STS0
 * Register Offset : 0x0184
 */

#define HEDET_INSERT_ALL          BIT(15)
#define HEDET_BDET_INSERT         BIT(14)
#define HEDET_GDET_INSERT         BIT(13)
#define HEDET_LDETH_INSERT        BIT(12)
#define HEDET_LDETL_INSERT        BIT(11)
#define HEDET_MDET_INSERT         BIT(10)
#define PA_SHT_FLAG(x)            (((x) & GENMASK(1, 0)) << 8)
#define DRV_OCP_FLAG(x)           (((x) & GENMASK(7, 0)))

/*
 * Register Name   : ANA_STS1
 * Register Offset : 0x0188
 */

#define HP_DPOP_DCCAL_STS(x)       (((x) & GENMASK(3, 0)) << 11)
#define RCV_DCCAL_DVLD              BIT(10)
#define HPL_DPOP_STS(x)             (((x) & GENMASK(1, 0)) << 8)
#define HPR_DPOP_STS(x)             (((x) & GENMASK(1, 0)) << 6)
#define HEAD_BUTTON_DVLD            BIT(5)
#define HPL_DPOP_DVLD               BIT(4)
#define HPR_DPOP_DVLD               BIT(3)
#define HP_DPOP_DVLD                BIT(2)
#define RCV_LOOP_DVLD               BIT(1)
#define RCV_DAC_FDIN_DVLD           BIT(0)

/*
 * Register Name   : ANA_STS2
 * Register Offset : 0x018C
 */

#define HP_DCV_OH(x)          (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_STS3
 * Register Offset : 0x0190
 */

#define HP_DCV_OL(x)      (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_STS4
 * Register Offset : 0x0194
 */

#define DAHPL_SOFT_DVLD       BIT(9)
#define DAHPR_SOFT_DVLD       BIT(8)
#define DAAOL_SOFT_DVLD       BIT(7)
#define DAAOR_SOFT_DVLD       BIT(6)
#define CP_HPEN_DLY           BIT(5)
#define CP_POS_SSTART_DONE    BIT(4)
#define CP_NEG_SSTART_DONE    BIT(3)
#define CP_POS_VIN_SO         BIT(2)
#define CP_NEG_VIN_SO         BIT(1)
#define CP_VIN_STS            BIT(0)

/*
 * Register Name   : ANA_STS5
 * Register Offset : 0x0198
 */

#define CP_CLIMIT_FLAG          BIT(3)
#define CP_NEG_SH_FLAG          BIT(2)
#define CP_AD_DATO_DVLD         BIT(1)
#define CP_DQ_DATO_DVLD         BIT(0)

/*
 * Register Name   : ANA_STS6
 * Register Offset : 0x019C
 */

#define CMP_CAL_CTRL       BIT(12)
#define CMP_CAL_P(x)       (((x) & GENMASK(5, 0)) << 6)
#define CMP_CAL_N(x)       (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_STS7
 * Register Offset : 0x01A0
 * Description     : AUD_DCL_FGU remove
 *                   change to OSC
 */

#define OSC_4M_CAL_DONE                  BIT(13)
#define OSC_4M_ERROR                     BIT(12)
#define OSC_4M_FSM(x)                    (((x) & GENMASK(11, 0)))

/*
 * Register Name   : ANA_STS8
 * Register Offset : 0x01A4
 * Description     : PABST remove
 *                   change to RCVDPOP
 */

#define RCV_ICAL_RSL_O(x)                (((x) & GENMASK(1, 0)) << 6)
#define RCV_AB_ICAL_PNSL_O                BIT(5)
#define RCV_ICAL_O(x)                     (((x) & GENMASK(4, 0)))

/*
 * Register Name   : ANA_STS9
 * Register Offset : 0x01A8
 * Description     : PABST remove
 */


/*
 * Register Name   : ANA_STS10
 * Register Offset : 0x01AC
 * Description     : PABST remove
 */


/*
 * Register Name   : ANA_STS11
 * Register Offset : 0x01B0
 * Description     : PABST remove
 */


/*
 * Register Name   : ANA_STS12
 * Register Offset : 0x01B4
 */

#define HMIC_BIAS_MBGREF_SEL(x)       (((x) & GENMASK(2, 0)) << 10)
#define HID_DBNC_STS0(x)              (((x) & GENMASK(2, 0)) << 7)
#define HID_DBNC_STS1(x)              (((x) & GENMASK(2, 0)) << 4)
#define HPL_SHUTDOWN_DVLD             BIT(3)
#define HPR_SHUTDOWN_DVLD             BIT(2)
#define EAR_SHUTDOWN_DVLD             BIT(1)
#define AO_SHUTDOWN_DVLD              BIT(0)

/*
 * Register Name   : ANA_STS13
 * Register Offset : 0x01B8
 */

#define IMPD_BIST_DVLD            BIT(8)
#define IMPD_BIST_FAIL_FLAG(x)    (((x) & GENMASK(7, 0)))

/*
 * Register Name   : ANA_STS14
 * Register Offset : 0x01BC
 */

#define IMPD_ADC_DATO(x)       (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_RSV12
 * Register Offset : 0x01C0
 */


/*
 * Register Name   : ANA_DATO0
 * Register Offset : 0x01C4
 */

#define CP_DQ_DAT1(x)        (((x) & GENMASK(5, 0)) << 8)
#define CP_DQ_DAT0(x)        (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_DATO1
 * Register Offset : 0x01C8
 */

#define CP_DQ_DAT3(x)      (((x) & GENMASK(5, 0)) << 8)
#define CP_DQ_DAT2(x)      (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_DATO2
 * Register Offset : 0x01CC
 */

#define CP_DQ_DAT5(x)       (((x) & GENMASK(5, 0)) << 8)
#define CP_DQ_DAT4(x)       (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_DATO3
 * Register Offset : 0x01D0
 */

#define CP_DQ_DAT7(x)        (((x) & GENMASK(5, 0)) << 8)
#define CP_DQ_DAT6(x)        (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_DATO4
 * Register Offset : 0x01D4
 */

#define CP_DQ_DAT9(x)        (((x) & GENMASK(5, 0)) << 8)
#define CP_DQ_DAT8(x)        (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_DATO5
 * Register Offset : 0x01D8
 */

#define CP_DQ_DAT11(x)        (((x) & GENMASK(5, 0)) << 8)
#define CP_DQ_DAT10(x)        (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_DATO6
 * Register Offset : 0x01DC
 */

#define CP_DQ_DAT13(x)          (((x) & GENMASK(5, 0)) << 8)
#define CP_DQ_DAT12(x)          (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_DATO7
 * Register Offset : 0x01E0
 */

#define CP_DQ_DAT15(x)          (((x) & GENMASK(5, 0)) << 8)
#define CP_DQ_DAT14(x)          (((x) & GENMASK(5, 0)))

/*
 * Register Name   : ANA_DATO8
 * Register Offset : 0x01E4
 */

#define CP_AD_DAT3(x)        (((x) & GENMASK(3, 0)) << 12)
#define CP_AD_DAT2(x)        (((x) & GENMASK(3, 0)) << 8)
#define CP_AD_DAT1(x)        (((x) & GENMASK(3, 0)) << 4)
#define CP_AD_DAT0(x)        (((x) & GENMASK(3, 0)))

/*
 * Register Name   : ANA_DATO9
 * Register Offset : 0x01E8
 */

#define CP_AD_DAT7(x)        (((x) & GENMASK(3, 0)) << 12)
#define CP_AD_DAT6(x)        (((x) & GENMASK(3, 0)) << 8)
#define CP_AD_DAT5(x)        (((x) & GENMASK(3, 0)) << 4)
#define CP_AD_DAT4(x)        (((x) & GENMASK(3, 0)))

/*
 * Register Name   : ANA_DATO10
 * Register Offset : 0x01EC
 */

#define CP_AD_DAT11(x)      (((x) & GENMASK(3, 0)) << 12)
#define CP_AD_DAT10(x)      (((x) & GENMASK(3, 0)) << 8)
#define CP_AD_DAT9(x)       (((x) & GENMASK(3, 0)) << 4)
#define CP_AD_DAT8(x)       (((x) & GENMASK(3, 0)))

/*
 * Register Name   : ANA_DATO11
 * Register Offset : 0x01F0
 */

#define CP_AD_DAT15(x)        (((x) & GENMASK(3, 0)) << 12)
#define CP_AD_DAT14(x)        (((x) & GENMASK(3, 0)) << 8)
#define CP_AD_DAT13(x)        (((x) & GENMASK(3, 0)) << 4)
#define CP_AD_DAT12(x)        (((x) & GENMASK(3, 0)))

/*
 * Register Name   : ANA_INT0
 * Register Offset : 0x0200
 */

#define EIC_DBNC_DATA(x)        (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT1
 * Register Offset : 0x0204
 */

#define EIC_DBNC_DMSK(x)       (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT2
 * Register Offset : 0x0208
 */


/*
 * Register Name   : ANA_INT3
 * Register Offset : 0x020C
 */

#define EIC_DBNC_IS(x)    (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT4
 * Register Offset : 0x0210
 */

#define EIC_DBNC_IBE(x)         (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT5
 * Register Offset : 0x0214
 */

#define EIC_DBNC_IEV(x)         (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT6
 * Register Offset : 0x0218
 */

#define EIC_DBNC_IE(x)        (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT7
 * Register Offset : 0x021C
 */

#define EIC_DBNC_RIS(x)     (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT8
 * Register Offset : 0x0220
 */

#define EIC_DBNC_MIS(x)         (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT9
 * Register Offset : 0x0224
 * Description     : ANA_INT9
 */

#define EIC_DBNC_IC(x)        (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT10
 * Register Offset : 0x0228
 */

#define EIC_DBNC_TRIG(x)        (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT11
 * Register Offset : 0x022C
 */

#define EIC_DBNC_TRIGSTS1(x)     (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT12
 * Register Offset : 0x0230
 */

#define EIC_DBNC_TRIGSTS2(x)    (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT13
 * Register Offset : 0x0234
 */

#define EIC_DBNC_FSM1(x)     (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT14
 * Register Offset : 0x0238
 */

#define EIC_DBNC_FSM2(x)        (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT15
 * Register Offset : 0x023C
 */

#define EIC_DBNC_FSM3(x)    (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT16
 * Register Offset : 0x0240
 */

#define EIC0_DBNC_CTRL(x)     (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT17
 * Register Offset : 0x0244
 */

#define EIC1_DBNC_CTRL(x)      (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT18
 * Register Offset : 0x0248
 */

#define EIC2_DBNC_CTRL(x)   (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT19
 * Register Offset : 0x024C
 */

#define EIC3_DBNC_CTRL(x)   (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT20
 * Register Offset : 0x0250
 */

#define EIC4_DBNC_CTRL(x)      (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT21
 * Register Offset : 0x0254
 */

#define EIC5_DBNC_CTRL(x)   (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT22
 * Register Offset : 0x0258
 */

#define EIC6_DBNC_CTRL(x)   (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT23
 * Register Offset : 0x025C
 */

#define EIC7_DBNC_CTRL(x)    (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT24
 * Register Offset : 0x0260
 */

#define EIC8_DBNC_CTRL(x)    (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT25
 * Register Offset : 0x0264
 */

#define EIC9_DBNC_CTRL(x)    (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT26
 * Register Offset : 0x0268
 */

#define EIC10_DBNC_CTRL(x)      (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT27
 * Register Offset : 0x026C
 */

#define EIC11_DBNC_CTRL(x)    (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT28
 * Register Offset : 0x0270
 */

#define EIC12_DBNC_CTRL(x)          (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT29
 * Register Offset : 0x0274
 */

#define EIC13_DBNC_CTRL(x)      (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT30
 * Register Offset : 0x0278
 */

#define EIC14_DBNC_CTRL(x)      (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT31
 * Register Offset : 0x027C
 */

#define EIC15_DBNC_CTRL(x)    (((x) & GENMASK(15, 0)))

/*
 * Register Name   : ANA_INT32
 * Register Offset : 0x0280
 */

#define ANA_INT_EN                 BIT(14)
#define PA_DCCAL_INT_EN            BIT(13)
#define PA_CLK_CAL_INT_EN          BIT(12)
#define HPR_SHUTDOWN_INT_EN        BIT(11)
#define HPL_SHUTDOWN_INT_EN        BIT(10)
#define AO_SHUTDOWN_INT_EN         BIT(9)
#define EAR_SHUTDOWN_INT_EN        BIT(8)
#define RCV_DPOP_INT_EN            BIT(7)
#define HPR_DPOP_INT_EN            BIT(6)
#define HPL_DPOP_INT_EN            BIT(5)
#define IMPD_DISCHARGE_INT_EN      BIT(4)
#define IMPD_CHARGE_INT_EN         BIT(3)
#define IMPD_BIST_DONE_INT_EN      BIT(2)
#define FGU_LOW_LIMIT_INT_EN       BIT(1)
#define HDT_BUTTON_INT_EN          BIT(0)

/*
 * Register Name   : ANA_INT33
 * Register Offset : 0x0284
 */

#define ANA_INT_CLR                 BIT(14)
#define PA_DCCAL_INT_CLR            BIT(13)
#define PA_CLK_CAL_INT_CLR          BIT(12)
#define HPR_SHUTDOWN_INT_CLR        BIT(11)
#define HPL_SHUTDOWN_INT_CLR        BIT(10)
#define AO_SHUTDOWN_INT_CLR         BIT(9)
#define EAR_SHUTDOWN_INT_CLR        BIT(8)
#define RCV_DPOP_INT_CLR            BIT(7)
#define HPR_DPOP_INT_CLR            BIT(6)
#define HPL_DPOP_DVLD_CLR           BIT(5)
#define IMPD_DISCHARGE_INT_CLR      BIT(4)
#define IMPD_CHARGE_INT_CLR         BIT(3)
#define IMPD_BIST_DONE_CLR          BIT(2)
#define FGU_LOW_LIMIT_INT_CLR       BIT(1)
#define HDT_BUTTON_INT_CLR          BIT(0)

#define ANA_INT33_CODEC_INTC_CLR(x)            (((x) & GENMASK(14, 0)))

/*
 * Register Name   : ANA_INT34
 * Register Offset : 0x0288
 */

#define ANA_INT_SHADOW_STATUS                BIT(14)
#define PA_DCCAL_INT_SHADOW_STATUS           BIT(13)
#define PA_CLK_CAL_INT_SHADOW_STATUS         BIT(12)
#define HPR_SHUTDOWN_INT_SHADOW_STATUS       BIT(11)
#define HPL_SHUTDOWN_INT_SHADOW_STATUS       BIT(10)
#define AO_SHUTDOWN_INT_SHADOW_STATUS        BIT(9)
#define EAR_SHUTDOWN_INT_SHADOW_STATUS       BIT(8)
#define RCV_DPOP_INT_SHADOW_STATUS           BIT(7)
#define HPR_DPOP_INT_SHADOW_STATUS          BIT(6)
#define HPL_DPOP_INT_SHADOW_STATUS           BIT(5)
#define IMPD_DISCHARGE_INT_SHADOW_STATUS     BIT(4)
#define IMPD_CHARGE_INT_SHADOW_STATUS        BIT(3)
#define IMPD_BIST_INT_SHADOW_STATUS          BIT(2)
#define FGU_LOW_LIMIT_INT_SHADOW_STATUS      BIT(1)
#define HDT_BUTTON_INT_SHADOW_STATUS         BIT(0)

#define ANA_INT34_BITS_SET(x)                (((x) & GENMASK(14, 0)))
/*
 * Register Name   : ANA_INT35
 * Register Offset : 0x028C
 */

#define ANA_INT_RAW_STATUS               BIT(14)
#define PA_DCCAL_INT_RAW_STATUS          BIT(13)
#define PA_CLK_CAL_INT_RAW_STATUS        BIT(12)
#define HPR_SHUTDOWN_INT_RAW_STATUS      BIT(11)
#define HPL_SHUTDOWN_INT_RAW_STATUS      BIT(10)
#define AO_SHUTDOWN_INT_RAW_STATUS       BIT(9)
#define EAR_SHUTDOWN_INT_RAW_STATUS      BIT(8)
#define RCV_DPOP_INT_RAW_STATUS          BIT(7)
#define HPR_DPOP_INT_RAW_STATUS          BIT(6)
#define HPL_DPOP_INT_RAW_STATUS          BIT(5)
#define IMPD_DISCHARGE_INT_RAW_STATUS    BIT(4)
#define IMPD_CHARGE_INT_RAW_STATUS       BIT(3)
#define IMPD_BIST_INT_RAW_STATUS         BIT(2)
#define FGU_LOW_LIMIT_INT_RAW_STATUS     BIT(1)
#define HDT_BUTTON_INT_RAW_STATUS        BIT(0)

/*
 * Register Name   : DIG_INT4
 * Register Offset : 0x0290
 */



#endif // __SPRD_CODEC_UMP9620_H
