/*
 * sound/soc/sprd/dai/vbc/r1p0/vbc-comm.h
 *
 * SPRD SoC VBC -- SpreadTrum SOC DAI for VBC Common function.
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
#ifndef __VBC_COMM_H
#define __VBC_COMM_H

#include <mach/sprd-audio.h>

#define VBC_VERSION	"vbc.r1p0"

/* VBADBUFFDTA */
enum {
	VBISADCK_INV = 9,
	VBISDACK_INV,
	VBLSB_EB,
	VBIIS_DLOOP = 13,
	VBPCM_MODE,
	VBIIS_LRCK,
};
/* VBDABUFFDTA */
enum {
	RAMSW_NUMB = 9,
	RAMSW_EN,
	VBAD0DMA_EN,
	VBAD1DMA_EN,
	VBDA0DMA_EN,
	VBDA1DMA_EN,
	VBENABLE,
};

/* VBDAPATH FM MIXER*/
enum {
	DAPATH_NO_MIX = 0,
	DAPATH_ADD_FM,
	DAPATH_SUB_FM,
};

/* AD DGMUX NUM*/
enum {
	ADC0_DGMUX = 0,
	ADC1_DGMUX,
	ADC_DGMUX_MAX
};

/* -------------------------- */

#define PHYS_VBDA0		(VBC_PHY_BASE + 0x0000)	/* 0x0000  Voice band DAC0 data buffer */
#define PHYS_VBDA1		(VBC_PHY_BASE + 0x0004)	/* 0x0004  Voice band DAC1 data buffer */
#define PHYS_VBAD0		(VBC_PHY_BASE + 0x0008)	/* 0x0008  Voice band ADC0 data buffer */
#define PHYS_VBAD1		(VBC_PHY_BASE + 0x000C)	/* 0x000C  Voice band ADC1 data buffer */

#define ARM_VB_BASE		VBC_BASE
#define VBDA0			(ARM_VB_BASE + 0x0000)	/*  Voice band DAC0 data buffer */
#define VBDA1			(ARM_VB_BASE + 0x0004)	/*  Voice band DAC1 data buffer */
#define VBAD0			(ARM_VB_BASE + 0x0008)	/*  Voice band ADC0 data buffer */
#define VBAD1			(ARM_VB_BASE + 0x000C)	/*  Voice band ADC1 data buffer */
#define VBBUFFSIZE		(ARM_VB_BASE + 0x0010)	/*  Voice band buffer size */
#define VBADBUFFDTA		(ARM_VB_BASE + 0x0014)	/*  Voice band AD buffer control */
#define VBDABUFFDTA		(ARM_VB_BASE + 0x0018)	/*  Voice band DA buffer control */
#define VBADCNT			(ARM_VB_BASE + 0x001C)	/*  Voice band AD buffer counter */
#define VBDACNT			(ARM_VB_BASE + 0x0020)	/*  Voice band DA buffer counter */
#define VBINTTYPE		(ARM_VB_BASE + 0x0034)
#define VBDATASWT		(ARM_VB_BASE + 0x0038)
#define VBIISSEL		(ARM_VB_BASE + 0x003C)

#define DAPATCHCTL		(ARM_VB_BASE + 0x0040)
#define DADGCTL			(ARM_VB_BASE + 0x0044)
#define DAHPCTL			(ARM_VB_BASE + 0x0048)
#define DAALCCTL0		(ARM_VB_BASE + 0x004C)
#define DAALCCTL1		(ARM_VB_BASE + 0x0050)
#define DAALCCTL2		(ARM_VB_BASE + 0x0054)
#define DAALCCTL3		(ARM_VB_BASE + 0x0058)
#define DAALCCTL4		(ARM_VB_BASE + 0x005C)
#define DAALCCTL5		(ARM_VB_BASE + 0x0060)
#define DAALCCTL6		(ARM_VB_BASE + 0x0064)
#define DAALCCTL7		(ARM_VB_BASE + 0x0068)
#define DAALCCTL8		(ARM_VB_BASE + 0x006C)
#define DAALCCTL9		(ARM_VB_BASE + 0x0070)
#define DAALCCTL10		(ARM_VB_BASE + 0x0074)
#define STCTL0			(ARM_VB_BASE + 0x0078)
#define STCTL1			(ARM_VB_BASE + 0x007C)
#define ADPATCHCTL		(ARM_VB_BASE + 0x0080)
#define ADDGCTL			(ARM_VB_BASE + 0x0084)
#define HPCOEF0			(ARM_VB_BASE + 0x0100)
#define HPCOEF1			(ARM_VB_BASE + 0x0104)
#define HPCOEF2			(ARM_VB_BASE + 0x0108)
#define HPCOEF3			(ARM_VB_BASE + 0x010C)
#define HPCOEF4			(ARM_VB_BASE + 0x0110)
#define HPCOEF5			(ARM_VB_BASE + 0x0114)
#define HPCOEF6			(ARM_VB_BASE + 0x0118)
#define HPCOEF7			(ARM_VB_BASE + 0x011C)
#define HPCOEF8			(ARM_VB_BASE + 0x0120)
#define HPCOEF9			(ARM_VB_BASE + 0x0124)
#define HPCOEF10		(ARM_VB_BASE + 0x0128)
#define HPCOEF11		(ARM_VB_BASE + 0x012C)
#define HPCOEF12		(ARM_VB_BASE + 0x0130)
#define HPCOEF13		(ARM_VB_BASE + 0x0134)
#define HPCOEF14		(ARM_VB_BASE + 0x0138)
#define HPCOEF15		(ARM_VB_BASE + 0x013C)
#define HPCOEF16		(ARM_VB_BASE + 0x0140)
#define HPCOEF17		(ARM_VB_BASE + 0x0144)
#define HPCOEF18		(ARM_VB_BASE + 0x0148)
#define HPCOEF19		(ARM_VB_BASE + 0x014C)
#define HPCOEF20		(ARM_VB_BASE + 0x0150)
#define HPCOEF21		(ARM_VB_BASE + 0x0154)
#define HPCOEF22		(ARM_VB_BASE + 0x0158)
#define HPCOEF23		(ARM_VB_BASE + 0x015C)
#define HPCOEF24		(ARM_VB_BASE + 0x0160)
#define HPCOEF25		(ARM_VB_BASE + 0x0164)
#define HPCOEF26		(ARM_VB_BASE + 0x0168)
#define HPCOEF27		(ARM_VB_BASE + 0x016C)
#define HPCOEF28		(ARM_VB_BASE + 0x0170)
#define HPCOEF29		(ARM_VB_BASE + 0x0174)
#define HPCOEF30		(ARM_VB_BASE + 0x0178)
#define HPCOEF31		(ARM_VB_BASE + 0x017C)
#define HPCOEF32		(ARM_VB_BASE + 0x0180)
#define HPCOEF33		(ARM_VB_BASE + 0x0184)
#define HPCOEF34		(ARM_VB_BASE + 0x0188)
#define HPCOEF35		(ARM_VB_BASE + 0x018C)
#define HPCOEF36		(ARM_VB_BASE + 0x0190)
#define HPCOEF37		(ARM_VB_BASE + 0x0194)
#define HPCOEF38		(ARM_VB_BASE + 0x0198)
#define HPCOEF39		(ARM_VB_BASE + 0x019C)
#define HPCOEF40		(ARM_VB_BASE + 0x01A0)
#define HPCOEF41		(ARM_VB_BASE + 0x01A4)
#define HPCOEF42		(ARM_VB_BASE + 0x01A8)

#define ARM_VB_END		(ARM_VB_BASE + 0x01AC)
#define IS_SPRD_VBC_RANG(reg)	(((reg) >= ARM_VB_BASE) && ((reg) < ARM_VB_END))
#define SPRD_VBC_BASE_HI	(ARM_VB_BASE & 0xFFFF0000)

#define VBADBUFFERSIZE_SHIFT	(0)
#define VBADBUFFERSIZE_MASK	(0xFF<<VBADBUFFERSIZE_SHIFT)
#define VBDABUFFERSIZE_SHIFT	(8)
#define VBDABUFFERSIZE_MASK	(0xFF<<VBDABUFFERSIZE_SHIFT)

#define VBDACHEN_SHIFT  	(0)
#define VBADCHEN_SHIFT  	(2)

 /*VBIISSEL*/
#define VBIISSEL_AD_PORT_SHIFT	(0)
#define VBIISSEL_AD_PORT_MASK		(0x7)
#define VBIISSEL_DA_PORT_SHIFT		(3)
#define VBIISSEL_DA_PORT_MASK		(0x7<<VBIISSEL_DA_PORT_SHIFT)
#define VBIISSEL_DA_LRCK		(9)
#define VBIISSEL_AD_LRCK		(8)
     /*DAPATHCTL*/
#define VBDAPATH_DA0_ADDFM_SHIFT	(0)
#define VBDAPATH_DA0_ADDFM_MASK		(0x3<<VBDAPATH_DA0_ADDFM_SHIFT)
#define VBDAPATH_DA1_ADDFM_SHIFT	(2)
#define VBDAPATH_DA1_ADDFM_MASK		(0x3<<VBDAPATH_DA1_ADDFM_SHIFT)
     /*ADPATHCTL*/
#define VBADPATH_AD0_INMUX_SHIFT    	(0)
#define VBADPATH_AD0_INMUX_MASK    	(0x3<<VBADPATH_AD0_INMUX_SHIFT)
#define VBADPATH_AD1_INMUX_SHIFT    	(2)
#define VBADPATH_AD1_INMUX_MASK    	(0x3<<VBADPATH_AD1_INMUX_SHIFT)
#define VBADPATH_AD0_DGMUX_SHIFT    	(4)
#define VBADPATH_AD0_DGMUX_MASK    	(0x1<<VBADPATH_AD0_DGMUX_SHIFT)
#define VBADPATH_AD1_DGMUX_SHIFT    	(5)
#define VBADPATH_AD1_DGMUX_MASK    	(0x1<<VBADPATH_AD1_DGMUX_SHIFT)
/*STCTL0 */
#define VBST_EN_0			(12)
#define VBST_HPF_0			(11)
/*STCTL1 */
#define VBST_HPF_1			(11)
#define VBST_EN_1	               	(12)
#define VBST0_SEL_CHN			(13)
#define VBST1_SEL_CHN			(14)
    enum {
	VBC_LEFT = 0,
	VBC_RIGHT = 1,
};

enum {
	VBC_PLAYBACK = 0,
	SPRD_VBC_PLAYBACK_COUNT = 1,
	VBC_CAPTRUE = 1,
	VBC_IDX_MAX
};

static const char *vbc_get_name(int vbc_idx);

static int vbc_reg_read(unsigned int reg);
static void vbc_reg_raw_write(unsigned int reg, int val);
static int vbc_reg_write(unsigned int reg, int val);
static int vbc_reg_update(unsigned int reg, int val, int mask);
static void vbc_da_buffer_clear_all(int vbc_idx);
static int vbc_enable(int enable);
static void vbc_clk_set(struct clk *clk);
static struct clk *vbc_clk_get(void);
static int vbc_power(int enable);
static int vbc_chan_enable(int enable, int vbc_idx, int chan);
static int vbc_codec_startup(int vbc_idx, struct snd_soc_dai *dai);
static int fm_set_vbc_buffer_size(void);

static inline void vbc_safe_mem_release(void **free)
{
	if (*free) {
		kfree(*free);
		*free = NULL;
	}
}

#define vbc_safe_kfree(p) vbc_safe_mem_release((void**)p)

#endif /* __VBC_COMM_H */
