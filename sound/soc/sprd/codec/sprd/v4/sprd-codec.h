/*
 * sound/soc/sprd/codec/sprd/v4/sprd-codec.h
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

#define SPRD_CODEC_INFO	(AUDIO_CODEC_2723)

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

/*AUD_SDM_CTL0*/
#define DAC_SDM_DODVL_START  (8)
#define DAC_SDM_DODVL_MASK  (0x0f)

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

/*DAC_SDM_DC_L*/
#define DAC_SDM_DC_L          (0)
#define DAC_SDM_DC_L_MASK     (0xffff)
/*DAC_SDM_DC_H*/
#define DAC_SDM_DC_R          (0)
#define DAC_SDM_DC_R_MASK     (0xff)


#define LDO_V_28		(0)
#define LDO_V_29		(1)
#define LDO_V_30		(2)
#define LDO_V_31		(3)
#define LDO_V_32		(4)
#define LDO_V_33		(5)
#define LDO_V_34		(6)
#define LDO_V_36		(7)


#define MIC_LDO_V_21		(0)
#define MIC_LDO_V_19		(1)
#define MIC_LDO_V_23		(2)
#define MIC_LDO_V_25		(3)

/* ANA_PMU0 */
#define PA_SW_EN		    (15)
#define PA_LDO_EN		    (14)
#define PA_EN			    (13)
#define OVP_LDO             (11)
#define LDOCG_EN            (10)
#define VB_EN			    (9)
#define VBO_EN			    (8)
#define HEADMICBIAS_EN		(7)
#define HEADMIC_SLEEP_EN	(6)

/* ANA_PMU1 */
#define BG_EN			(15)
#define BG_IBIAS_EN		(14)
#define VCM_EN			(13)
#define VCM_BUF_EN		(12)
#define SEL_VCMI		(11)
#define VCMI_FAST_EN	(10)
#define MICBIAS_EN		(9)
#define AUXMICBIAS_EN	(8)
#define LDOCG_EN_FAL_ILD	(3)
#define LDOCG_ISET_FAL	(2)
#define LDOCG_RAMP_EN	(1)

/* ANA_PMU2 */
#define VCM_V			    (13)
#define VCM_V_MASK		    (0x7)
#define MICBIAS_V		    (11)
#define MICBIAS_V_MASK	    (0x3)
#define HEADMICBIAS_V		(9)
#define HEADMICBIAS_V_MASK	(0x3)

/* ANA_PMU3 */
#define BG_I			(14)
#define BG_I_MASK		(0x3)
#define PA_AB_I         (11)
#define PA_AB_I_MASK    (0x7)
#define PA_LDO_V		(8)
#define PA_LDO_V_MASK	(0x7)
#define CG_LDO_V		(0)
#define CG_LDO_V_MASK	(0xff)

/* ANA_PMU4 */
#define VB_CAL			(8)
#define VB_CAL_MASK	(0x1F)
#define VCM_CAL			(5)
#define VCM_CAL_MASK	(0x7)
#define LDO_PA_CAL		(0)
#define LDO_PA_CAL_MASK (0x1F)

/* ANA_PMU5 */
#define PA_OVP_V		(13)
#define PA_OVP_V_MASK	(0x7)
#define PA_OVP_542		(0)
#define PA_OVP_527		(1)
#define PA_OVP_512		(2)
#define PA_OVP_496		(3)
#define PA_OVP_481		(4)
#define PA_OVP_465		(5)
#define PA_OVP_450		(6)
#define PA_OVP_435		(7)

/* ANA_PMU6 */
#define DRV_OCP_AOR_PD		(15)
#define DRV_OCP_AOL_PD		(14)
#define DRV_OCP_EAR_PD		(13)
#define DRV_OCP_HP_PD		(12)

/* ANA_CDC0 */
#define ADC_CLK_EN		(15)
#define ADC_CLK_RST		(14)
#define ADC_CLK_F		(12)
#define ADC_CLK_F_MASK	(0x3)
#define DAC_CLK_EN		(11)
#define DAC_CLK_F		(9)
#define DAC_CLK_F_MASK	(0x3)
#define DRV_CLK_EN		(8)

/* ANA_CDC1 */
#define ADCPGAL_BYP		    (14)
#define ADCPGAL_BYP_MASK	(0x3)
#define ADCPGAL_EN		    (12)
#define ADCPGAL_EN_MASK		(0x3)
#define ADCPGAR_BYP		    (10)
#define ADCPGAR_BYP_MASK	(0x3)
#define ADCPGAR_EN		    (8)
#define ADCPGAR_EN_MASK		(0x3)
#define ADC_IBUF_PD		    (7)
#define ADC_VREF1P5		    (6)
#define ADCL_PD			    (5)
#define ADCL_RST		    (4)
#define ADCR_PD			    (3)
#define ADCR_RST		    (2)
#define FM_REC_EN           (1)

/* ANA_CDC2 */
#define DACL_EN			    (15)
#define DACR_EN			    (14)
#define DACBUF_I_S		    (13)
#define DACDC_RMV_EN        (12)
#define DACDC_RMV_S         (10)
#define DACDC_RMV_S_MASK    (0x3)
#define DRV_SOFT_EN         (9)
#define HPL_EN              (8)
#define HPR_EN			    (7)
#define DIFF_EN			    (6)
#define EAR_EN			    (5)
#define AOL_EN			    (4)
#define AOR_EN			    (3)

/* ANA_CDC3 */
#define CG_REF_EN       (15)
#define CG_CHP_EN       (14)
#define CG_HPL_EN       (13)
#define CG_HPR_EN       (12)
#define CG_HP_DIFF_EN   (11)
#define CG_CHP_CLK_S    (10)
#define CG_CHP_OSC      (7)
#define CG_CHP_OSC_MASK (0x7)
#define CG_LPW          (5)
#define CG_LPW_MASK     (0x3)
#define CG_CHP_MODE     (4)
#define CG_HP_MODE      (3)
#define CG_HPCAL_EN     (2)
#define CG_HPCALSTP_EN  (1)

/* ANA_CDC4 */
#define PA_D_EN			(15)
#define PA_DTRI_F		(13)
#define PA_DTRI_F_MASK	(0x03)
#define PA_DEMI_EN		(12)
#define PA_SWOCP_PD		(11)
#define PA_LDOOCP_PD	(10)
#define PA_SS_EN		(9)
#define PA_SS_RST		(8)
#define PA_SS_F		    (6)
#define PA_SS_F_MASK    (0x3)
#define PA_SS_32K_EN    (5)
#define PA_SS_T         (2)
#define PA_SS_T_MASK    (0x7)
#define DRV_STOP_EN		(1)

/* ANA_CDC5 */
#define MIC_ADCR		(15)
#define AUXMIC_ADCR		(14)
#define HEADMIC_ADCR	(13)
#define AIL_ADCR		(12)
#define AIR_ADCR		(11)
#define MIC_ADCL		(10)
#define AUXMIC_ADCL		(9)
#define HEADMIC_ADCL	(8)
#define AIL_ADCL		(7)
#define AIR_ADCL		(6)

/* ANA_CDC6 */
#define ADCL_AOL		(15)
#define ADCR_AOL		(14)
#define DACL_AOL		(13)
#define DACR_AOL		(12)
#define ADCL_AOR		(11)
#define ADCR_AOR		(10)
#define DACL_AOR		(9)
#define DACR_AOR		(8)

/* ANA_CDC7 */
#define ADCL_EAR		(15)
#define ADCR_EAR		(14)
#define DACL_EAR		(13)
#define DACR_EAR		(12)
#define HPL_CGL         (11)
#define HPR_CGR         (10)
#define ADCL_P_HPL		(9)
#define ADCR_P_HPL		(8)
#define DACL_P_HPL		(7)
#define DACR_P_HPL		(6)
#define ADCL_N_HPR		(5)
#define ADCR_P_HPR		(4)
#define DACL_N_HPR		(3)
#define DACR_P_HPR		(2)

/* ANA_CDC8 */
#define ADC_MIC_PGA             (14)
#define ADC_MIC_PGA_MASK        (0x3)
#define ADC_AUXMIC_PGA          (12)
#define ADC_AUXMIC_PGA_MASK     (0x3)
#define ADC_HEADMIC_PGA         (10)
#define ADC_HEADMIC_PGA_MASK    (0x3)
#define ADC_AILR_PGA              (8)
#define ADC_AILR_PGA_MASK         (0x3)

/* ANA_CDC9 */
#define ADC_PGA_L       (4)
#define ADC_PGA_L_MASK  (0x3F)
#define ADC_PGA_R       (10)
#define ADC_PGA_R_MASK  (0x3F)


/* ANA_CDC10 */
#define DACL_G			(13)
#define DACL_G_MASK		(0x7)
#define DACR_G			(10)
#define DACR_G_MASK		(0x7)

/* ANA_CDC11 */
#define AOL_G			(12)
#define AOL_G_MASK		(0xF)
#define AOR_G			(8)
#define AOR_G_MASK		(0xF)

/* ANA_CDC12 */
#define EAR_G			(12)
#define EAR_G_MASK		(0xF)
#define HPL_G			(8)
#define HPL_G_MASK		(0xF)
#define HPR_G			(4)
#define HPR_G_MASK		(0xF)

/* ANA_CDC13 */
#define CG_HPL_G_2		(7)
#define CG_HPL_G_2_MASK	(0x7)
#define CG_HPL_G_1		(10)
#define CG_HPL_G_1_MASK	(0x3F)

/* ANA_CDC14 */
#define CG_HPR_G_2		(7)
#define CG_HPR_G_2_MASK	(0x7)
#define CG_HPR_G_1		(10)
#define CG_HPR_G_1_MASK	(0x3F)


/* ANA_CDC15 */
#define AUD_NG_EN		    (15)
#define AUD_NG_DA_EN	    (14)
#define AUD_NG_PA_EN	    (13)
#define AUD_NG_PA_EN	    (13)
#define AUD_NG_CG_LPW_EN    (12)
#define AUD_CG_DRE_EN       (11)
#define AUD_CG_DRE_HYS_EN   (10)
#define AUD_CG_POP_EN       (9)
#define AUD_CG_POP_HYS_EN   (8)

/* ANA_CDC16 */
#define HP_POP_CTL		    (6)
#define HP_POP_CTL_MASK		(0x3)
#define HP_POP_CTL_DIS		(0)
#define HP_POP_CTL_UP		(1)
#define HP_POP_CTL_DOWN		(2)
#define HP_POP_CTL_HOLD		(3)

#define HP_POP_STEP		    (3)
#define HP_POP_STEP_MASK	(0x7)
#define HP_POP_STEP_012		(0)
#define HP_POP_STEP_025		(1)
#define HP_POP_STEP_05		(2)
#define HP_POP_STEP_1		(3)
#define HP_POP_STEP_2		(4)
#define HP_POP_STEP_4		(5)
#define HP_POP_STEP_8		(6)
#define HP_POP_STEP_16		(7)

/* ANA_CDC17 */

/* ANA_CDC18 */
#define CG_HPL_DCVI		    (8)
#define CG_HPL_DCVI_MASK    (0xFF)
#define CG_HPR_DCVI		    (0)
#define CG_HPR_DCVI_MASK    (0xFF)

/* ANA_HDT0 */
#define AUD_V2ADC_EN        (15)
#define AUD_V2ADC_SEL       (11)
#define AUD_V2ADC_SEL_MASK  (0xF)
#define HIB_SBUT            (1)
#define HIB_SBUT_MASK       (0xF)

/* ANA_HDT1 */
#define AUD_GND_DET_PU_PD	(15)
#define AUD_HEAD_L_INT_PU_PD	(14)
#define AUD_HEAD_APP_G		(9)
#define AUD_HEAD_APP_G_MASK	(3)
#define AUD_HEAD_APP_P		(7)
#define AUD_HEAD_APP_P_MASK	(3)

/* ANA_HDT2 */

/* ANA_STS0 */
#define HP_POP_FLG		    (7)////////////CHECK
#define HP_POP_FLG_MASK		(0x3)
#define HP_POP_FLG_NEAR_CMP	(3)
#define OVP_FLAG		    (6)

/* ANA_STS1 */

/* ANA_STS2 */

/* DANGL */
#define	AUDIO_DANGL			(15)
/* DANGR */
#define	AUDIO_DANGR			(15)
/* DRE */

/* DIG_CFG0 */
#define AUDIFA_DACL_EN		(9)
#define AUDIFA_ADCL_EN		(10)
#define AUDIFA_DACR_EN		(11)
#define AUDIFA_ADCR_EN		(12)

/* DIG_CFG1 */

/* DIG_CFG2 */

/* DIG_CFG3 */
#define AUD_A_INT_CLR         (3)
#define AUD_A_INT_CLR_MASK    (0xFF)
/* DIG_CFG4 */
#define AUD_A_INT_EN         (8)
#define AUD_A_INT_EN_MASK    (0xFF)
/* DIG_STS0 */
#define AUDIO_POP_IRQ		(7)
#define OVP_IRQ			    (6)
#define OTP_IRQ			    (5)
#define PA_OCP_IRQ		    (4)
#define LOR_OCP_IRQ		    (3)
#define LOL_OCP_IRQ		    (2)
#define EAR_OCP_IRQ		    (1)
#define HP_OCP_IRQ		    (0)

/* DIG_STS1 */
#define AUD_IRQ_MSK		    (8)
#define AUD_IRQ_MSK_MASK	(0xFF)


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
#define AUD_DMIC_CTL	(SPRD_CODEC_DP_BASE + 0x0030)
#define AUD_ADC1_I2S_CTL	(SPRD_CODEC_DP_BASE + 0x0034)
#define AUD_DAC_SDM_L   (SPRD_CODEC_DP_BASE + 0x0038)
#define AUD_DAC_SDM_H   (SPRD_CODEC_DP_BASE + 0x003C)
#define SPRD_CODEC_DP_END	(SPRD_CODEC_DP_BASE + 0x003C)
#define IS_SPRD_CODEC_DP_RANG(reg) (((reg) >= SPRD_CODEC_DP_BASE) && ((reg) < SPRD_CODEC_DP_END))
#define SPRD_CODEC_AP_BASE (CODEC_AP_BASE)
#define ANA_PMU0			(SPRD_CODEC_AP_BASE + 0x0000)
#define ANA_PMU1		(SPRD_CODEC_AP_BASE + 0x0004)
#define ANA_PMU2	(SPRD_CODEC_AP_BASE + 0x0008)
#define ANA_PMU3			(SPRD_CODEC_AP_BASE + 0x000C)
#define ANA_PMU4			(SPRD_CODEC_AP_BASE + 0x0010)
#define ANA_PMU5			(SPRD_CODEC_AP_BASE + 0x0014)
#define ANA_PMU6			(SPRD_CODEC_AP_BASE + 0x0018)
#define ANA_CDC0			(SPRD_CODEC_AP_BASE + 0x001C)
#define ANA_CDC1			(SPRD_CODEC_AP_BASE + 0x0020)
#define ANA_CDC2			(SPRD_CODEC_AP_BASE + 0x0024)
#define ANA_CDC3			(SPRD_CODEC_AP_BASE + 0x0028)
#define ANA_CDC4			(SPRD_CODEC_AP_BASE + 0x002C)
#define ANA_CDC5			(SPRD_CODEC_AP_BASE + 0x0030)
#define ANA_CDC6			(SPRD_CODEC_AP_BASE + 0x0034)
#define ANA_CDC7			(SPRD_CODEC_AP_BASE + 0x0038)
#define ANA_CDC8			(SPRD_CODEC_AP_BASE + 0x003C)
#define ANA_CDC9			(SPRD_CODEC_AP_BASE + 0x0040)
#define ANA_CDC10			(SPRD_CODEC_AP_BASE + 0x0044)
#define ANA_CDC11			(SPRD_CODEC_AP_BASE + 0x0048)
#define ANA_CDC12			(SPRD_CODEC_AP_BASE + 0x004C)
#define ANA_CDC13			(SPRD_CODEC_AP_BASE + 0x0050)
#define ANA_CDC14			(SPRD_CODEC_AP_BASE + 0x0054)
#define ANA_CDC15			(SPRD_CODEC_AP_BASE + 0x0058)
#define ANA_CDC16			(SPRD_CODEC_AP_BASE + 0x005C)
#define ANA_CDC17			(SPRD_CODEC_AP_BASE + 0x0060)
#define ANA_CDC18			(SPRD_CODEC_AP_BASE + 0x0064)
#define ANA_HDT0			(SPRD_CODEC_AP_BASE + 0x0068)
#define ANA_HDT1			(SPRD_CODEC_AP_BASE + 0x006C)
#define ANA_HDT2			(SPRD_CODEC_AP_BASE + 0x0070)
#define ANA_STS0			(SPRD_CODEC_AP_BASE + 0x0074)
#define ANA_STS1			(SPRD_CODEC_AP_BASE + 0x0078)
#define ANA_STS2			(SPRD_CODEC_AP_BASE + 0x007C)
#define DANGL			    (SPRD_CODEC_AP_BASE + 0x0080)
#define DANGR			    (SPRD_CODEC_AP_BASE + 0x0084)
#define DRE			        (SPRD_CODEC_AP_BASE + 0x0088)
#define DIG_CFG0			(SPRD_CODEC_AP_BASE + 0x008C)
#define DIG_CFG1			(SPRD_CODEC_AP_BASE + 0x0090)
#define DIG_CFG2			(SPRD_CODEC_AP_BASE + 0x0094)
#define DIG_CFG3			(SPRD_CODEC_AP_BASE + 0x0098)
#define DIG_CFG4			(SPRD_CODEC_AP_BASE + 0x009C)
#define DIG_STS0			(SPRD_CODEC_AP_BASE + 0x00A0)
#define DIG_STS1			(SPRD_CODEC_AP_BASE + 0x00A4)
#define SPRD_CODEC_AP_END	(SPRD_CODEC_AP_BASE + 0x00A8)
#define IS_SPRD_CODEC_AP_RANG(reg) (((reg) >= SPRD_CODEC_AP_BASE) && ((reg) < SPRD_CODEC_AP_END))

#define SPRD_CODEC_IIS1_ID  111
#endif /* __SPRD_CODEC_H */
