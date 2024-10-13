/*
 * sound/soc/sprd/dai/vbc/r3p0/vbc-phy-r3p0.h
 *
 * SPRD SoC VBC -- SpreadTrum SOC R3P0 VBC driver function.
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

#ifndef __VBC_PHY_DRV_H
#define __VBC_PHY_DRV_H
#include <sound/vbc-utils.h>

/* ap vbc register start */
/* agdsp ofld reg */
//#define     VBC_OFLD_ADDR      (0x41480000)
#define     VBC_OFLD_ADDR      (0x00001000)

#define    VBC_AUDPLY_FIFO_WR_0        (VBC_OFLD_ADDR + 0x0000)
#define    VBC_AUDPLY_FIFO_WR_1        (VBC_OFLD_ADDR + 0x0004)
#define    VBC_AUDRCD_FIFO_RD_0        (VBC_OFLD_ADDR + 0x0008)
#define    VBC_AUDRCD_FIFO_RD_1        (VBC_OFLD_ADDR + 0x000c)

#define    VBC_AUDPLY_FIFO_CTRL        (VBC_OFLD_ADDR + 0x0020)
#define    VBC_AUDRCD_FIFO_CTRL        (VBC_OFLD_ADDR + 0x0024)
#define    VBC_AUD_SRC_CTRL            (VBC_OFLD_ADDR + 0x0028)
#define    VBC_AUD_EN                  (VBC_OFLD_ADDR + 0x002C)
#define    VBC_AUD_CLR                 (VBC_OFLD_ADDR + 0x0030)
#define    VBC_AUDPLY_FIFO0_STS        (VBC_OFLD_ADDR + 0x0034)
#define    VBC_AUDPLY_FIFO1_STS        (VBC_OFLD_ADDR + 0x0038)
#define    VBC_AUDRCD_FIFO0_STS        (VBC_OFLD_ADDR + 0x003C)
#define    VBC_AUDRCD_FIFO1_STS        (VBC_OFLD_ADDR + 0x0040)
#define    VBC_AUD_INT_EN              (VBC_OFLD_ADDR + 0x0044)
#define    VBC_AUD_INT_STS             (VBC_OFLD_ADDR + 0x0048)
/* vbc mem reg */
#define	 CTL_BASE_VBC  (0x01700000)
#define  VBC_MEM_ADDR  (CTL_BASE_VBC)
#define    VBC_DAC0_FIFO_WR_0             (VBC_MEM_ADDR + 0x0000)
#define    VBC_DAC0_FIFO_WR_1             (VBC_MEM_ADDR + 0x0004)
#define    VBC_DAC1_FIFO_WR_0             (VBC_MEM_ADDR + 0x0008)
#define    VBC_DAC1_FIFO_WR_1             (VBC_MEM_ADDR + 0x000c)

#define    VBC_ADC0_FIFO_RD_0             (VBC_MEM_ADDR + 0x0010)
#define    VBC_ADC0_FIFO_RD_1             (VBC_MEM_ADDR + 0x0014)
#define    VBC_ADC1_FIFO_RD_0             (VBC_MEM_ADDR + 0x0018)
#define    VBC_ADC1_FIFO_RD_1             (VBC_MEM_ADDR + 0x001c)
#define    VBC_ADC2_FIFO_RD_0             (VBC_MEM_ADDR + 0x0020)
#define    VBC_ADC2_FIFO_RD_1             (VBC_MEM_ADDR + 0x0024)
#define    VBC_ADC3_FIFO_RD_0             (VBC_MEM_ADDR + 0x0028)
#define    VBC_ADC3_FIFO_RD_1             (VBC_MEM_ADDR + 0x002c)

#define    VBC_AUD_PLY_FIFO_RD_0             (VBC_MEM_ADDR + 0x0030)
#define    VBC_AUD_PLY_FIFO_RD_1             (VBC_MEM_ADDR + 0x0034)
#define    VBC_AUD_RCD_FIFO_WR_0           (VBC_MEM_ADDR + 0x0038)
#define    VBC_AUD_RCD_FIFO_WR_1           (VBC_MEM_ADDR + 0x003c)

#define    VBC_DAC0_EQ6_ALC_COEF_START      (VBC_MEM_ADDR + 0x0050)
#define    VBC_DAC0_EQ6_ALC_COEF_END           (VBC_MEM_ADDR + 0x013c)

#define    VBC_DAC0_EQ4_COEF_START      (VBC_MEM_ADDR + 0x0140)
#define    VBC_DAC0_EQ4_COEF_END           (VBC_MEM_ADDR + 0x019c)

#define    VBC_DAC0_MBDRC_BLEQ_DRC_COEF_START      (VBC_MEM_ADDR + 0x01A0)
#define    VBC_DAC0_MBDRC_BLEQ_DRC_COEF_END      (VBC_MEM_ADDR + 0x0210)

#define    VBC_DAC0_MBDRC_CROSS_EQ_COEF_START      (VBC_MEM_ADDR + 0x0214)
#define    VBC_DAC0_MBDRC_CROSS_EQ_COEF_END      (VBC_MEM_ADDR + 0x0390)

#define    VBC_DAC0_MBDRC_CROS_DRC_COEF_START      (VBC_MEM_ADDR + 0x0394)
#define    VBC_DAC0_MBDRC_CROS_DRC_COEF_END      (VBC_MEM_ADDR + 0x04a0)

#define    VBC_DAC0_NCH_COEF_START      (VBC_MEM_ADDR + 0x04a4)
#define    VBC_DAC0_NCH_COEF_END      (VBC_MEM_ADDR + 0x0590)

#define    VBC_DAC1_EQ6_ALC_COEF_START      (VBC_MEM_ADDR + 0x0594)
#define    VBC_DAC1_EQ6_ALC_COEF_END           (VBC_MEM_ADDR + 0x0680)

#define    VBC_DAC1_EQ4_COEF_START      (VBC_MEM_ADDR + 0x0684)
#define    VBC_DAC1_EQ4_COEF_END           (VBC_MEM_ADDR + 0x06e0)

#define    VBC_DAC1_MBDRC_COEF2_START      (VBC_MEM_ADDR + 0x06e4)
#define    VBC_DAC1_MBDRC_COEF2_END      (VBC_MEM_ADDR + 0x0754)

#define    VBC_DAC1_MBDRC_COEF0_START      (VBC_MEM_ADDR + 0x0758)
#define    VBC_DAC1_MBDRC_COEF0_END      (VBC_MEM_ADDR + 0x08d4)

#define    VBC_DAC1_MBDRC_CROS_DRC_COEF_START      (VBC_MEM_ADDR + 0x08d8)
#define    VBC_DAC1_MBDRC_CROS_DRC_COEF_END      (VBC_MEM_ADDR + 0x09e4)

#define    VBC_DAC1_NCH_COEF_START      (VBC_MEM_ADDR + 0x09e8)
#define    VBC_DAC1_NCH_COEF_END      (VBC_MEM_ADDR + 0x0ad4)

#define    VBC_ADC0_EQ6_COEF_START      (VBC_MEM_ADDR + 0x0ad8)
#define    VBC_ADC0_EQ6_COEF_END           (VBC_MEM_ADDR + 0x0b80)

#define    VBC_ADC1_EQ6_COEF_START      (VBC_MEM_ADDR + 0x0b84)
#define    VBC_ADC1_EQ6_COEF_END           (VBC_MEM_ADDR + 0x0c2c)

#define    VBC_ADC2_EQ6_COEF_START      (VBC_MEM_ADDR + 0x0c30)
#define    VBC_ADC2_EQ6_COEF_END           (VBC_MEM_ADDR + 0x0cd8)

#define    VBC_ADC3_EQ6_COEF_START      (VBC_MEM_ADDR + 0x0cdc)
#define    VBC_ADC3_EQ6_COEF_END           (VBC_MEM_ADDR + 0x0d84)

#define    VBC_ST_DRC_COEF_START      (VBC_MEM_ADDR + 0x0d88)
#define    VBC_ST_DRC_COEF_END           (VBC_MEM_ADDR + 0x0dc8)

/* vbc ctl reg */
#define    VBC_CTL_ADDR         (CTL_BASE_VBC)

#define    VBC_MODULE_CLR0               (VBC_CTL_ADDR + 0x0DCC)
#define    VBC_MODULE_CLR1               (VBC_CTL_ADDR + 0x0DD0)
#define    VBC_MODULE_CLR2               (VBC_CTL_ADDR + 0x0DD4)
#define    VBC_DAC_DG_CFG                (VBC_CTL_ADDR + 0x0DD8)
#define    VBC_DAC0_SMTH_DG_CFG          (VBC_CTL_ADDR + 0x0DDC)
#define    VBC_DAC1_SMTH_DG_CFG          (VBC_CTL_ADDR + 0x0DE0)
#define    VBC_DAC0_DGMIXER_DG_CFG       (VBC_CTL_ADDR + 0x0DE4)
#define    VBC_DAC1_DGMIXER_DG_CFG       (VBC_CTL_ADDR + 0x0DE8)
#define    VBC_DAC_DGMIXER_DG_STEP       (VBC_CTL_ADDR + 0x0DF0)
#define    VBC_DRC_MODE                  (VBC_CTL_ADDR + 0x0DF4)
#define    VBC_DAC_EQ4_COEF_UPDT         (VBC_CTL_ADDR + 0x0DF8)
#define    VBC_DAC_TONE_GEN_CTRL         (VBC_CTL_ADDR + 0xDFC)
#define    VBC_DAC0_TONE_GEN_PARAM0      (VBC_CTL_ADDR + 0x0E00)
#define    VBC_DAC0_TONE_GEN_PARAM1      (VBC_CTL_ADDR + 0x0E04)
#define    VBC_DAC0_TONE_GEN_PARAM2      (VBC_CTL_ADDR + 0x0E08)
#define    VBC_DAC0_TONE_GEN_PARAM3      (VBC_CTL_ADDR + 0x0E0C)
#define    VBC_DAC0_TONE_GEN_PARAM4      (VBC_CTL_ADDR + 0x0E10)
#define    VBC_DAC0_TONE_GEN_PARAM5      (VBC_CTL_ADDR + 0x0E14)
#define    VBC_DAC0_TONE_GEN_PARAM6      (VBC_CTL_ADDR + 0x0E18)
#define    VBC_DAC0_TONE_GEN_PARAM7      (VBC_CTL_ADDR + 0x0E1C)
#define    VBC_DAC0_TONE_GEN_PARAM8      (VBC_CTL_ADDR + 0x0E20)
#define    VBC_DAC0_TONE_GEN_PARAM9      (VBC_CTL_ADDR + 0x0E24)
#define    VBC_DAC1_TONE_GEN_PARAM0      (VBC_CTL_ADDR + 0x0E28)
#define    VBC_DAC1_TONE_GEN_PARAM1      (VBC_CTL_ADDR + 0x0E2C)
#define    VBC_DAC1_TONE_GEN_PARAM2      (VBC_CTL_ADDR + 0x0E30)
#define    VBC_DAC1_TONE_GEN_PARAM3      (VBC_CTL_ADDR + 0x0E34)
#define    VBC_DAC1_TONE_GEN_PARAM4      (VBC_CTL_ADDR + 0x0E38)
#define    VBC_DAC1_TONE_GEN_PARAM5      (VBC_CTL_ADDR + 0x0E3C)
#define    VBC_DAC1_TONE_GEN_PARAM6      (VBC_CTL_ADDR + 0x0E40)
#define    VBC_DAC1_TONE_GEN_PARAM7      (VBC_CTL_ADDR + 0x0E44)
#define    VBC_DAC1_TONE_GEN_PARAM8      (VBC_CTL_ADDR + 0x0E48)
#define    VBC_DAC1_TONE_GEN_PARAM9      (VBC_CTL_ADDR + 0x0E4C)
#define    VBC_DAC0_MIXER_CFG            (VBC_CTL_ADDR + 0x0E50)
#define    VBC_DAC1_MIXER_CFG            (VBC_CTL_ADDR + 0x0E54)
#define    VBC_ADC_DG_CFG0               (VBC_CTL_ADDR + 0x0E58)
#define    VBC_ADC_DG_CFG1               (VBC_CTL_ADDR + 0x0E5C)
#define    VBC_FM_FIFO_CTRL              (VBC_CTL_ADDR + 0x0E60)
#define    VBC_FM_MIXER_CTRL             (VBC_CTL_ADDR + 0x0E64)
#define    VBC_FM_HPF_CTRL               (VBC_CTL_ADDR + 0x0E68)
#define    VBC_ST_FIFO_CTRL              (VBC_CTL_ADDR + 0x0E6C)
#define    VBC_ST_MIXER_CTRL             (VBC_CTL_ADDR + 0x0E70)
#define    VBC_ST_HPF_CTRL               (VBC_CTL_ADDR + 0x0E74)
#define    VBC_ST_DRC_MODE               (VBC_CTL_ADDR + 0x0E78)
#define    VBC_DAC0_FIFO0_STS            (VBC_CTL_ADDR + 0x0E7C)
#define    VBC_DAC0_FIFO1_STS            (VBC_CTL_ADDR + 0x0E80)
#define    VBC_ADC2_FIFO0_STS            (VBC_CTL_ADDR + 0x0E84)
#define    VBC_ADC2_FIFO1_STS            (VBC_CTL_ADDR + 0x0E88)
#define    VBC_ADC3_FIFO0_STS            (VBC_CTL_ADDR + 0x0E8C)
#define    VBC_ADC3_FIFO1_STS            (VBC_CTL_ADDR + 0x0E90)
#define    VBC_ST_FIFO_STS               (VBC_CTL_ADDR + 0x0E94)
#define    VBC_FM_FIFO_STS               (VBC_CTL_ADDR + 0x0E98)
#define    VBC_AUDPLY_OFLD_FIFO0_STS     (VBC_CTL_ADDR + 0x0E9C)
#define    VBC_AUDPLY_OFLD_FIFO1_STS     (VBC_CTL_ADDR + 0x0EA0)
#define    VBC_AUDRCD_OFLD_FIFO1_STS     (VBC_CTL_ADDR + 0x0EA4)
#define    VBC_INT_EN                    (VBC_CTL_ADDR + 0x0EA8)
#define    VBC_INT_CLR                   (VBC_CTL_ADDR + 0x0EAC)
#define    VBC_DMA_EN                    (VBC_CTL_ADDR + 0x0EB0)
#define    VBC_INT_STS_RAW               (VBC_CTL_ADDR + 0x0EB4)
#define    VBC_INT_STS_MSK               (VBC_CTL_ADDR + 0x0EB8)
#define    VBC_IIS_CTRL                  (VBC_CTL_ADDR + 0x0EBC)
#define    VBC_MODULE_CLR3               (VBC_CTL_ADDR + 0x0EC0)
#define    VBC_SRC_MODE                  (VBC_CTL_ADDR + 0x0EC4)
#define    VBC_SRC_PAUSE                 (VBC_CTL_ADDR + 0x0EC8)
#define    VBC_PATH_CTRL0                (VBC_CTL_ADDR + 0x0ECC)
#define    VBC_DAC0_MTDG_CTRL            (VBC_CTL_ADDR + 0x0ED0)
#define    VBC_DAC1_MTDG_CTRL            (VBC_CTL_ADDR + 0x0ED4)
#define    VBC_DATAPATH_EN               (VBC_CTL_ADDR + 0x0ED8)
#define    VBC_MODULE_EN0                (VBC_CTL_ADDR + 0x0EDC)
#define    VBC_MODULE_EN1                (VBC_CTL_ADDR + 0x0EE0)
#define    VBC_MODULE_EN2                (VBC_CTL_ADDR + 0x0EE4)
#define    VBC_MODULE_EN3                (VBC_CTL_ADDR + 0x0EE8)
#define    VBC_DAC0_FIFO_CFG             (VBC_CTL_ADDR + 0x0EEC)
#define    VBC_DAC1_FIFO_CFG             (VBC_CTL_ADDR + 0x0EF0)
#define    VBC_ADC0_FIFO_CFG0            (VBC_CTL_ADDR + 0x0EF4)
#define    VBC_ADC1_FIFO_CFG0            (VBC_CTL_ADDR + 0x0EF8)
#define    VBC_ADC2_FIFO_CFG0            (VBC_CTL_ADDR + 0x0EFC)
#define    VBC_ADC3_FIFO_CFG0            (VBC_CTL_ADDR + 0x0F00)
#define    VBC_AUDPLY_DSP_FIFO_CTRL      (VBC_CTL_ADDR + 0x0F04)
#define    VBC_AUDRCD_DSP_FIFO_CTRL      (VBC_CTL_ADDR + 0x0F08)
#define    VBC_PATH_CTRL1                (VBC_CTL_ADDR + 0x0F0C)
#define    VBC_DAC_NOISE_GEN_GAIN        (VBC_CTL_ADDR + 0x0F10)
#define    VBC_DAC1_NOISE_GEN_SEED       (VBC_CTL_ADDR + 0x0F1C)
#define    VBC_DAC0_NOISE_GEN_SEED       (VBC_CTL_ADDR + 0x0F20)
#define    VBC_MODULE_EN4                (VBC_CTL_ADDR + 0x0F24)
#define    VBC_PATH_EX_FIFO_STATUS       (VBC_CTL_ADDR + 0x0F28)
#define    VBC_DAC2ADC_LOOP_CTL          (VBC_CTL_ADDR + 0x0F2C)
#define    VBC_DAT_FORMAT_CTL            (VBC_CTL_ADDR + 0x0F30)
#define    VBC_PATH_EX_FIFO_CLR          (VBC_CTL_ADDR + 0x0F34)
#define    VBC_AUDPLY_DAC_FIFO_CTL       (VBC_CTL_ADDR + 0x0F38)
#define    VBC_ADC_AUDRCD_FIFO_STATUS    (VBC_CTL_ADDR + 0x0F3C)
#define    VBC_VERSION                   (VBC_CTL_ADDR + 0x0F40)
#define    VBC_DAC1_FIFO0_STS            (VBC_CTL_ADDR + 0x0F44)
#define    VBC_DAC1_FIFO1_STS            (VBC_CTL_ADDR + 0x0F48)
#define    VBC_ADC0_FIFO0_STS            (VBC_CTL_ADDR + 0x0F4C)
#define    VBC_ADC0_FIFO1_STS            (VBC_CTL_ADDR + 0x0F50)
#define    VBC_ADC1_FIFO0_STS            (VBC_CTL_ADDR + 0x0F54)
#define    VBC_ADC1_FIFO1_STS            (VBC_CTL_ADDR + 0x0F58)
#define    VBC_DAC_SMTH_DG_STP           (VBC_CTL_ADDR + 0x0F5C)
#define    VBC_AUDRCD_OFLD_FIFO0_STS     (VBC_CTL_ADDR + 0x0F60)
#define    VBC_AUDPLY_DSP_FIFO0_STS      (VBC_CTL_ADDR + 0x0F64)
#define    VBC_AUDPLY_DSP_FIFO1_STS      (VBC_CTL_ADDR + 0x0F68)
#define    VBC_AUDRCD_DSP_FIFO1_STS      (VBC_CTL_ADDR + 0x0F6C)
#define    VBC_AUDRCD_DSP_FIFO0_STS      (VBC_CTL_ADDR + 0x0F70)
#define	   VBC_BAK_REG					 (VBC_CTL_ADDR + 0x0F80)
#define	   VBC_IIS_IN_STS				 (VBC_CTL_ADDR + 0x0F8C)

#define    VBC_MEM_ADDR_START       (VBC_MEM_ADDR + 0x0000)
#define    VBC_MEM_ADDR_END         (VBC_MEM_ADDR + 0x0dc8)
#define    VBC_CTL_ADDR_START       (VBC_CTL_ADDR + 0x0DCC)
#define    VBC_CTL_ADDR_END         (VBC_CTL_ADDR + 0x0F8C)
#define    VBC_OFLD_ADDR_START		(VBC_OFLD_ADDR + 0x0000)
#define    VBC_OFLD_ADDR_END		(VBC_OFLD_ADDR + 0x0048)
#define	   IS_AP_VBC_RANG(reg)	(((reg) >= VBC_OFLD_ADDR_START) && ((reg) <= VBC_OFLD_ADDR_END))
#define    AP_VBC_BASE_HI	(VBC_OFLD_ADDR & 0xFFFF0000)
#define	   IS_DSP_VBC_RANG(reg)	(((reg) >= VBC_MEM_ADDR_START) && ((reg) <= VBC_CTL_ADDR_END))
#define    DSP_VBC_BASE_HI	(VBC_CTL_ADDR & 0xFFFF0000)

/* shift */
/* VBC_AUDPLY_FIFO_CTRL */
#define	   AUDPLY_AP_FIFO_EMPTY_LVL			(0)
#define	   AUDPLY_AP_FIFO_EMPTY_LVL_MASK	(0x1ff)
#define	   AUDPLY_AP_DAT_FMT_CTL			(9)
#define	   AUDPLY_AP_DAT_FMT_CTL_MASK		(0x3)
#define	   AUDPLY_AP_FIFO_FULL_LVL			(16)
#define	   AUDPLY_AP_FIFO_FULL_LVL_MASK		(0x1ff)
/* VBC_AUDRCD_FIFO_CTRL */
#define	   AUDRCD_AP_FIFO_EMPTY_LVL			(0)
#define	   AUDRCD_AP_FIFO_EMPTY_LVL_MASK	(0x1ff)
#define	   AUDRCD_AP_DAT_FMT_CTL			(9)
#define	   AUDRCD_AP_DAT_FMT_CTL_MASK		(0x3)
#define	   AUDRCD_AP_FIFO_FULL_LVL			(16)
#define	   AUDRCD_AP_FIFO_FULL_LVL_MASK		(0x1ff)
/* VBC_AUD_SRC_CTRL */
#define	   AUDPLY_AP_SRC_MODE			(0)
#define	   AUDPLY_AP_SRC_MODE_MASK		(0xf)
#define	   AUDRCD_AP_SRC_MODE			(4)
#define	   AUDRCD_AP_SRC_MODE_MASK		(0xf)
#define    AUDPLY_AP_SRC_PAUSE			(8)
#define    AUDRCD_AP_SRC_PAUSE			(9)
/* VBC_AUD_EN */
#define	   AUDPLY_AP_FIFO1_EN			(9)
#define	   AUDPLY_AP_FIFO0_EN			(8)
#define	   AUDRCD_DMA_AD1_EN			(7)
#define	   AUDRCD_DMA_AD0_EN			(6)
#define	   AUDPLY_DMA_DA1_EN			(5)
#define	   AUDPLY_DMA_DA0_EN			(4)
#define	   AUDRCD_SRC_EN_1				(3)
#define	   AUDRCD_SRC_EN_0				(2)
#define	   AUDPLY_SRC_EN_1				(1)
#define	   AUDPLY_SRC_EN_0				(0)
/* VBC_AUD_CLR */
#define	   AUDRCD_AP_INT_CLR			(5)
#define	   AUDPLY_AP_INT_CLR			(4)
#define	   AUDRCD_AP_FIFO_CLR			(3)
#define	   AUDPLY_AP_FIFO_CLR			(2)
#define	   AUDRCD_AP_SRC_CLR			(1)
#define	   AUDPLY_AP_SRC_CLR			(0)
/* VBC_AUD_INT_EN */
#define	   AUDPLY_AP_INT_EN				(0)
#define	   AUDRCD_AP_INT_EN				(1)
/* ap vbc register end */

#define VB_AUDRCD_FULL_WATERMARK	(392)
#define VB_AUDPLY_EMPTY_WATERMARK	(120)
#define VBC_PHY_AP_TO_AGCP(phy_addr)	((phy_addr) - 0x40000000)

/* dma hw request */
#define DMA_REQ_DA0_DEV_ID	(1)
#define DMA_REQ_DA1_DEV_ID	(2)
#define DMA_REQ_AD0_DEV_ID	(3)
#define DMA_REQ_AD1_DEV_ID	(4)

/* ap vbc phy driver start */
void ap_vbc_src_clear(int stream);
void ap_vbc_aud_int_en(int stream, int en);
int ap_vbc_fifo_enable(int enable, int stream, int chan);
void ap_vbc_fifo_clear(int stream);
void ap_vbc_aud_dat_format_set(int stream, VBC_DAT_FORMAT dat_fmt);
void ap_vbc_aud_dma_chn_en(int enable, int stream, int chan);
int ap_vbc_set_fifo_size(int stream, int full, int empty);
void ap_vbc_src_set(int en, int stream, int rate);
/* ap vbc phy driver end */

/* dsp vbc phy driver start */
int dsp_vbc_mdg_set(int id, int enable, int mdg_step);
int dsp_vbc_dg_set(int id, int dg_l, int dg_r);
int dsp_offload_dg_set(int id, int dg_l, int dg_r);
int dsp_vbc_smthdg_set(int id, int smthdg_l, int smthdg_r);
int dsp_vbc_smthdg_step_set(int id, int smthdg_step);
int dsp_vbc_mixerdg_step_set(int mixerdg_step);
int dsp_vbc_mixerdg_mainpath_set(int id, int mixerdg_main_l, int mixerdg_main_r);
int dsp_vbc_mixerdg_mixpath_set(int id, int mixerdg_mix_l, int mixerdg_mix_r);
int dsp_vbc_mux_set(int id, int mux_sel);
int dsp_vbc_adder_set(int id, int adder_mode_l, int adder_mode_r);
int dsp_vbc_src_set(int id, int enable, VBC_SRC_MODE_E src_mode);
int dsp_vbc_bt_call_set(int id, int enable, VBC_SRC_MODE_E src_mode);
int dsp_vbc_set_volume(struct snd_soc_codec *codec);
int dsp_vbc_loopback_set(struct snd_soc_codec *codec);
int dsp_vbc_dp_en_set(struct snd_soc_codec *codec, int id);

/* dsp vbc phy driver end */

int ap_vbc_reg_update(unsigned int reg, int val, int mask);
int ap_vbc_reg_write(unsigned int reg, int val);
unsigned int ap_vbc_reg_read(unsigned int reg);
int dsp_vbc_reg_write(unsigned int reg, int val);
unsigned int dsp_vbc_reg_read(unsigned int reg);

int vbc_of_setup(struct device_node *node, struct audio_sipc_of_data *info, struct device *dev);
int dsp_vbc_get_src_mode(int rate);

/* vbc driver function start */
int vbc_dsp_func_startup(struct device *dev, int id,
			int stream, const char *name);
int vbc_dsp_func_shutdown(struct device *dev, int id,
			int stream, const char *name);
int vbc_dsp_func_hwparam(struct device *dev, int id, int stream,
			const char *name, unsigned int channels, unsigned int format, unsigned int rate);
int vbc_dsp_func_hw_free(struct device *dev, int id,
			int stream, const char *name);
int vbc_dsp_func_trigger(struct device *dev, int id, int stream,
			const char *name, int cmd);
/* vbc driver function end */

#endif /* __VBC_PHY_DRV_H */
