/*
 * sound/soc/sprd/codec/sprd/v3/sprd-codec.h
 *
 * SPRD-CODEC -- SpreadTrum Tiger intergrated codec.
 *
 * Copyright (C) 2013 SpreadTrum Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY ork FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SPRD_CODEC_H
#define __SPRD_CODEC_H

#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/sprd-audio.h>
#include <mach/adi.h>
#include <asm/io.h>


/* unit: ms */
#define SPRD_CODEC_LDO_WAIT_TIME	(5)
#define SPRD_CODEC_LDO_VCM_TIME		(2)
#define SPRD_CODEC_DAC_MUTE_TIMEOUT	(600)

#define SPRD_CODEC_HP_POP_TIMEOUT	(1000)

#define SPRD_CODEC_RATE_8000   (10)
#define SPRD_CODEC_RATE_9600   ( 9)
#define SPRD_CODEC_RATE_11025  ( 8)
#define SPRD_CODEC_RATE_12000  ( 7)
#define SPRD_CODEC_RATE_16000  ( 6)
#define SPRD_CODEC_RATE_22050  ( 5)
#define SPRD_CODEC_RATE_24000  ( 4)
#define SPRD_CODEC_RATE_32000  ( 3)
#define SPRD_CODEC_RATE_44100  ( 2)
#define SPRD_CODEC_RATE_48000  ( 1)
#define SPRD_CODEC_RATE_96000  ( 0)

/* AUD_TOP_CTL */
#define DAC_EN_L		(0)
#define ADC_EN_L		(1)
#define DAC_EN_R		(2)
#define ADC_EN_R		(3)
#define ADC_SINC_SEL	(8)
#define ADC_SINC_SEL_MASK  (0x3)
#define ADC_DMIC_SEL	(9)

#define ADC1_EN_L		(10)
#define ADC1_EN_R		(11)
#define ADC1_SINC_SEL	(14)
#define ADC1_SINC_SEL_MASK  (0x3)
#define ADC1_DMIC1_SEL	(15)

/* AUD_DAC_CTL */
#define DAC_MUTE_START		(14)
#define DAC_MUTE_EN		(15)

/* AUD_ADC_CTL */
#define ADC_SRC_N 			(0)
#define ADC_SRC_N_MASK      (0x0f)
#define ADC1_SRC_N 			(4)
#define ADC1_SRC_N_MASK      (0xf0)

/* AUD_AUD_STS0 */
#define DAC_MUTE_U_MASK		(5)
#define DAC_MUTE_D_MASK		(4)
#define DAC_MUTE_U_RAW		(3)
#define DAC_MUTE_D_RAW		(2)
#define DAC_MUTE_ST		(0)
#define DAC_MUTE_ST_MASK	(0x3)

/* AUD_INT_CLR */
/* AUD_INT_EN */
#define DAC_MUTE_U		(1)
#define DAC_MUTE_D		(0)

/*AUD_DMIC_CTL*/
#define ADC_DMIC_CLK_MODE		(0)
#define ADC_DMIC_CLK_MODE_MASK	(0x3)
#define ADC_DMIC_LR_SEL		(2)
#define ADC1_DMIC_CLK_MODE	(3)
#define ADC1_DMIC_CLK_MODE_MASK	(0x3)
#define ADC1_DMIC_LR_SEL		(5)
#define ADC_DMIC_EN				(6)
#define ADC1_DMIC1_EN			(7)

/* AUDIF_ENB */
#define AUDIFA_DACL_EN		(0)
#define AUDIFA_ADCL_EN		(1)
#define AUDIFA_DACR_EN		(2)
#define AUDIFA_ADCR_EN		(3)

/* AUDIF_INT_EN */
/* AUDIF_INT_CLR */
/* AUDIF_INT_RAW */
/* AUDIF_INT_MASK */
#define AUDIO_POP_IRQ		(7)
#define OVP_IRQ			(6)
#define OTP_IRQ			(5)
#define PA_OCP_IRQ		(4)
#define LOR_OCP_IRQ		(3)
#define LOL_OCP_IRQ		(2)
#define EAR_OCP_IRQ		(1)
#define HP_OCP_IRQ		(0)

#define LDO_V_29		(0)
#define LDO_V_31		(1)
#define LDO_V_32		(2)
#define LDO_V_33		(3)
#define LDO_V_34		(4)
#define LDO_V_35		(5)
#define LDO_V_36		(6)
#define LDO_V_38		(7)

#define MIC_LDO_V_21		(0)
#define MIC_LDO_V_19		(1)
#define MIC_LDO_V_23		(2)
#define MIC_LDO_V_25		(3)

/* PMUR2_PMUR1 */
#define PA_SW_EN		(15)
#define PA_LDO_EN		(14)
#define PA_EN			(13)
#define PAR_SW_EN		(12)
#define PAR_LDO_EN		(11)
#define PAR_EN			(10)
#define VB_EN			(7)
#define VBO_EN			(6)
#define HEADMICBIAS_EN		(5)
#define MICBIAS_V		(2)
#define MICBIAS_V_MASK		(0x7)
/* #define MICBIAS_HV_EN		(2) */
#define HEADMIC_SLEEP_EN	(1)

/* PMUR4_PMUR3 */
#define MICBIAS_EN		(15)
#define AUXMICBIAS_EN		(14)
#define VCM_V			(11)
#define VCM_V_MASK		(0x7)
#define BG_I			(9)
#define BG_I_MASK		(0x3)
#define AUXADC_EN		(8)
#define BG_EN			(7)
#define BG_RST			(6)
#define BG_IBIAS_EN		(5)
#define VCM_EN			(4)
#define VCM_BUF_EN		(3)
#define ICM_PLUS_EN		(2)
#define SEL_VCMI		(1)
#define VCMI_FAST_EN		(0)

/* PMUR6_PMUR5 */
#define PA_SWOCP_PD		(15)
#define PA_LDOOCP_PD		(14)
#define PA_LDO_V		(11)
#define PA_LDO_V_MASK		(0x7)
#define VCM_CAL			(0)
#define VCM_CAL_MASK		(0x1F)

/* PMUR8_PMUR7 */
#define PA_OVP_V		(13)
#define PA_OVP_V_MASK		(0x7)
#define PA_OVP_542		(0)        //5.48V for 2711
#define PA_OVP_527		(1)        //5.33V for 2711
#define PA_OVP_512		(2)        //5.19V for 2711
#define PA_OVP_496		(3)        //5.05V for 2711
#define PA_OVP_481		(4)        //4.91V for 2711
#define PA_OVP_465		(5)        //4.76V for 2711
#define PA_OVP_450		(6)        //4.62V for 2711
#define PA_OVP_435		(7)        //4.48V for 2711

/* CCR */
#define ADC_CLK_EN		(7)
#define ADC_CLK_RST		(6)
#define ADC_CLK_F		(4)
#define ADC_CLK_F_MASK		(0x3)
#define DAC_CLK_EN		(3)
#define DAC_CLK_F		(1)
#define DAC_CLK_F_MASK		(0x3)
#define DRV_CLK_EN		(0)

/* AACR2_AACR1 */
#define ADCPGAL_BYP		(14)
#define ADCPGAL_BYP_MASK	(0x3)
#define ADCPGAL_EN		(12)
#define ADCPGAL_EN_MASK		(0x3)
#define ADCPGAR_BYP		(10)
#define ADCPGAR_BYP_MASK	(0x3)
#define ADCPGAR_EN		(8)
#define ADCPGAR_EN_MASK		(0x3)
#define ADC_IBUF_PD		(7)
#define ADC_VREF1P5		(6)
#define ADCL_PD			(5)
#define ADCL_RST		(4)
#define ADCR_PD			(3)
#define ADCR_RST		(2)

/* AAICR2_AAICR1 */
#define MIC_ADCR		(15)
#define AUXMIC_ADCR		(14)
#define HEADMIC_ADCR		(13)
#define AIL_ADCR		(12)
#define AIR_ADCR		(11)
#define MIC_ADCL		(7)
#define AUXMIC_ADCL		(6)
#define HEADMIC_ADCL		(5)
#define AIL_ADCL		(4)
#define AIR_ADCL		(3)

/* DACGR_DACR */
#define DACL_EN			(7)
#define DACR_EN			(6)
#define DACBUF_I_S		(5)

/* DAOCR2 */
#define ADCL_AOL		(7)
#define ADCR_AOL		(6)
#define DACL_AOL		(5)
#define DACR_AOL		(4)
#define ADCL_AOR		(3)
#define ADCR_AOR		(2)
#define DACL_AOR		(1)
#define DACR_AOR		(0)

/* DAOCR3_DAOCR1 */
#define DACL_EAR		(15)
#define ADCL_P_HPL		(7)
#define ADCR_P_HPL		(6)
#define DACL_P_HPL		(5)
#define DACR_P_HPL		(4)
#define ADCL_N_HPR		(3)
#define ADCR_P_HPR		(2)
#define DACL_N_HPR		(1)
#define DACR_P_HPR		(0)

/* DCR2_DCR1 */
#define PA_D_EN			(15)
#define PA_DTRI_F		(13)
#define PA_DTRI_F_MASK		(0x03)
#define PA_DEMI_EN		(12)
#define PA_SS_EN		(11)
#define PA_SS_RST		(10)
#define DRV_STOP_EN		(9)

#define HPL_EN			(7)
#define HPR_EN			(6)
#define EAR_EN			(5)
#define AOL_EN			(4)
#define AOR_EN			(3)
#define DIFF_EN			(2)
#define HP_VCMI_EN		(1)

/* DCR4_DCR3 */
#define DRV_OCP_AOL_PD		(7)
#define DRV_OCP_AOR_PD		(6)
#define DRV_OCP_EAR_PD		(5)
#define DRV_OCP_HP_PD		(4)

/* DCR6_DCR5 */
#define AUD_NG_EN		(7)
#define AUD_NG_DA_EN		(6)
#define AUD_NG_PA_EN		(5)
/* DCR8_DCR7 */
/* HP PA V2 */
#define V2_AUDIO_CLASSG_EN	(15)
#define V2_AUDIO_CHP_EN		(14)
#define V2_AUDIO_CHP_HPL_EN	(13)
#define V2_AUDIO_CHP_HPR_EN	(12)
#define V2_AUDIO_CHP_LMUTE	(11)
#define V2_AUDIO_CHP_RMUTE	(10)
#define V2_AUDIO_CHP_CLK_SEL	(9)
#define V2_AUDIO_CHP_OSC	(6)
#define V2_AUDIO_CHP_OSC_MASK	(0x07)
//befor BA version
#define V2_AUDIO_CHP_LPW_MASK	(0x1)
#define V2_AUDIO_CHP_LPW	(5)
#define V2_AUDIO_CHP_MODE	(4)
//after BA version
#define V2_AUDIO_CHP_LPW_MASK_BA	(0x3)
#define V2_AUDIO_CHP_LPW_BA	(4)
#define V2_AUDIO_CHP_MODE_BA	(3)

#define V2_AUDIO_CGCAL_EN	(2)
#define V2_AUDIO_CGCAL_STOP_EN	(1)
#define V2_AUDIO_CHP_OCP_PD	(0)


/* HP PA V0 */
//after CA version
#define AUDIO_CHP_LPW_MASK_CA		(0x3)
#define AUDIO_CHP_LPW_CA		(14)
#define AUDIO_CHP_MODE_CA		(13)
//befor CA version
#define AUDIO_CHP_LPW_MASK		(0x1)
#define AUDIO_CHP_LPW		(15)
#define AUDIO_CHP_MODE		(14)

#define AUDIO_CLASSG_EN	(7)
#define AUDIO_CHP_EN		(6)
#define AUDIO_CHP_HPL_EN	(5)
#define AUDIO_CHP_HPR_EN	(4)
#define AUDIO_CHP_LMUTE		(3)
#define AUDIO_CHP_RMUTE		(2)
#define AUDIO_CHP_OSC		(0)
#define AUDIO_CHP_OSC_MASK	(0x03)
#define AUDIO_CGCAL_EN	(12)
#define AUDIO_CGCAL_STOP_EN	(11)
#define AUDIO_CHP_OCP_PD	(10)

/* PNRCR2_PNRCR1 */
#define HP_POP_CTL		(6)
#define HP_POP_CTL_MASK		(0x03)
#define HP_POP_CTL_DIS		(0)
#define HP_POP_CTL_UP		(1)
#define HP_POP_CTL_DOWN		(2)
#define HP_POP_CTL_HOLD		(3)

#define HP_POP_STEP		(3)
#define HP_POP_STEP_MASK	(0x07)
#define HP_POP_STEP_012		(0)
#define HP_POP_STEP_025		(1)
#define HP_POP_STEP_05		(2)
#define HP_POP_STEP_1		(3)
#define HP_POP_STEP_2		(4)
#define HP_POP_STEP_4		(5)
#define HP_POP_STEP_8		(6)
#define HP_POP_STEP_16		(7)

/* IFR2_IFR1 */
#define HP_POP_FLG		(4)
#define HP_POP_FLG_MASK		(0x03)
#define HP_POP_FLG_NEAR_CMP	(3)
#define OVP_FLAG		(2)

 /*HIB*/
#define HIB_SBUT_MASK (0x0f)
#define HIB_SBUT              (0)
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
#define AUD_DMIC_CTL	(SPRD_CODEC_DP_BASE + 0x0030)
#define AUD_ADC1_I2S_CTL	(SPRD_CODEC_DP_BASE + 0x0034)
#define SPRD_CODEC_DP_END	(SPRD_CODEC_DP_BASE + 0x0034)
#define IS_SPRD_CODEC_DP_RANG(reg) (((reg) >= SPRD_CODEC_DP_BASE) && ((reg) < SPRD_CODEC_DP_END))
#define SPRD_CODEC_AP_BASE (CODEC_AP_BASE)
#define AUDIF_ENB			(SPRD_CODEC_AP_BASE + 0x0000)
#define AUDIF_OCP_OTP_TMR_CTL		(SPRD_CODEC_AP_BASE + 0x0004)
#define AUDIF_OVPTMR_SHUTDOWN_CTL	(SPRD_CODEC_AP_BASE + 0x0008)
#define AUDIF_INT_CLR			(SPRD_CODEC_AP_BASE + 0x000C)
#define AUDIF_INT_EN			(SPRD_CODEC_AP_BASE + 0x0010)
#define AUDIF_INT_RAW			(SPRD_CODEC_AP_BASE + 0x0014)
#define AUDIF_INT_MASK			(SPRD_CODEC_AP_BASE + 0x0018)
/* 0x001C ~ 0x003C is reserved for ADIE digital part */
#define PMUR2_PMUR1			(SPRD_CODEC_AP_BASE + 0x0040)
#define PMUR4_PMUR3			(SPRD_CODEC_AP_BASE + 0x0044)
#define PMUR6_PMUR5			(SPRD_CODEC_AP_BASE + 0x0048)
#define PMUR8_PMUR7			(SPRD_CODEC_AP_BASE + 0x004C)
/* 0x0050 ~ 0x005C is reserved for analog power part */
#define CCR				(SPRD_CODEC_AP_BASE + 0x0060)
#define AACR2_AACR1			(SPRD_CODEC_AP_BASE + 0x0064)
#define AAICR2_AAICR1			(SPRD_CODEC_AP_BASE + 0x0068)
#define ACGR1				(SPRD_CODEC_AP_BASE + 0x006C)
#define ACGR3_ACGR2			(SPRD_CODEC_AP_BASE + 0x0070)
#define DACGR_DACR			(SPRD_CODEC_AP_BASE + 0x0074)
#define DAOCR2				(SPRD_CODEC_AP_BASE + 0x0078)
#define DAOCR3_DAOCR1			(SPRD_CODEC_AP_BASE + 0x007C)
#define DCR2_DCR1			(SPRD_CODEC_AP_BASE + 0x0080)
#define DCR4_DCR3			(SPRD_CODEC_AP_BASE + 0x0084)
#define DCR6_DCR5			(SPRD_CODEC_AP_BASE + 0x0088)
#define DCR8_DCR7			(SPRD_CODEC_AP_BASE + 0x008C)
#define DCGR2_DCGR1			(SPRD_CODEC_AP_BASE + 0x0090)
#define DCGR3				(SPRD_CODEC_AP_BASE + 0x0094)
#define PNRCR2_PNRCR1			(SPRD_CODEC_AP_BASE + 0x0098)
#define PNRCR3				(SPRD_CODEC_AP_BASE + 0x009C)
#define HIBDR2_HIBDR1			(SPRD_CODEC_AP_BASE + 0x00A0)
#define HIBDR3				(SPRD_CODEC_AP_BASE + 0x00A4)
/* 0x00A8 ~ 0x00BC is reserved */
#define IFR2_IFR1			(SPRD_CODEC_AP_BASE + 0x00C0)
#define AUD_DANGL		(SPRD_CODEC_AP_BASE + 0x00C4)
#define AUD_DANGR		(SPRD_CODEC_AP_BASE + 0x00C8)
/* 0x00C4 ~ 0x00C8 is reserved */
#define IFR4_IFR3			(SPRD_CODEC_AP_BASE + 0x00CC)
/* 0x00D0 ~ 0x00D8 is reserved */
#define SPRD_CODEC_AP_END		(SPRD_CODEC_AP_BASE + 0x00D0)
#define IS_SPRD_CODEC_AP_RANG(reg) (((reg) >= SPRD_CODEC_AP_BASE) && ((reg) < SPRD_CODEC_AP_END))
#define SPRD_CODEC_IIS1_ID  111
#endif /* __SPRD_CODEC_H */
