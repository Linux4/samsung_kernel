/*
 * sound/soc/sprd/dai/vbc/r2p0/vbc-comm.h
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

#define VBC_VERSION	"vbc.r2p0"

/* VBADBUFFDTA */
enum {
	VBISADCK_INV = 9,
	VBISDACK_INV,
	VBISAD23CK_INV,
	VBIIS_DLOOP = 13,
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

/* VBADPATH ST INMUX*/
enum {
	ST_INPUT_NORMAL = 0,
	ST_INPUT_INVERSE,
	ST_INPUT_ZERO,
};

/* AD DGMUX NUM*/
enum {
	ADC0_DGMUX = 0,
	ADC1_DGMUX,
	ADC2_DGMUX,
	ADC3_DGMUX,
	ADC_DGMUX_MAX
};

/* -------------------------- */

#define PHYS_VBDA0		(VBC_PHY_BASE + 0x0000)	/* 0x0000  Voice band DAC0 data buffer */
#define PHYS_VBDA1		(VBC_PHY_BASE + 0x0004)	/* 0x0004  Voice band DAC1 data buffer */
#define PHYS_VBAD0		(VBC_PHY_BASE + 0x0008)	/* 0x0008  Voice band ADC0 data buffer */
#define PHYS_VBAD1		(VBC_PHY_BASE + 0x000C)	/* 0x000C  Voice band ADC1 data buffer */
#define PHYS_VBAD2		(VBC_PHY_BASE + 0x00C0)	/* 0x00C0  Voice band ADC2 data buffer */
#define PHYS_VBAD3		(VBC_PHY_BASE + 0x00C4)	/* 0x00C4  Voice band ADC3 data buffer */

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
#define VBAD23CNT		(ARM_VB_BASE + 0x0024)	/*  Voice band AD23 buffer counter */
#define VBADDMA			(ARM_VB_BASE + 0x0028)	/*  Voice band AD23 DMA control */
#define VBBUFFAD23		(ARM_VB_BASE + 0x002C)	/*  Voice band AD23 buffer size */
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
#define ADDG01CTL		(ARM_VB_BASE + 0x0084)
#define ADDG23CTL		(ARM_VB_BASE + 0x0088)
#define ADHPCTL			(ARM_VB_BASE + 0x008C)
#define ADCSRCCTL		(ARM_VB_BASE + 0x0090)
#define DACSRCCTL		(ARM_VB_BASE + 0x0094)
#define MIXERCTL		(ARM_VB_BASE + 0x0098)
#define STFIFOLVL		(ARM_VB_BASE + 0x009C)
#define VBIRQEN			(ARM_VB_BASE + 0x00A0)
#define VBIRQCLR		(ARM_VB_BASE + 0x00A4)
#define VBIRQRAW		(ARM_VB_BASE + 0x00A8)
#define VBIRQSTS		(ARM_VB_BASE + 0x00AC)
#define VBNGCVTHD		(ARM_VB_BASE + 0x00B0)
#define VBNGCTTHD		(ARM_VB_BASE + 0x00B4)
#define VBNGCTL			(ARM_VB_BASE + 0x00B8)

/* DA HPF EQ6 start,  end */
#define HPCOEF0_H		(ARM_VB_BASE + 0x0100)
#define HPCOEF0_L		(ARM_VB_BASE + 0x0104)
#define HPCOEF35_H		(ARM_VB_BASE + 0x0218)
#define HPCOEF35_L		(ARM_VB_BASE + 0x021c)
#define HPCOEF42_H		(ARM_VB_BASE + 0x0250)
#define HPCOEF42_L		(ARM_VB_BASE + 0x0254)
/* DA HPF EQ4  start,  end*/
#define HPCOEF43_H		(ARM_VB_BASE + 0x0258)
#define HPCOEF43_L		(ARM_VB_BASE + 0x025C)
#define HPCOEF64_H		(ARM_VB_BASE + 0x0300)
#define HPCOEF64_L		(ARM_VB_BASE + 0x0304)
#define HPCOEF71_H		(ARM_VB_BASE + 0x0338)
#define HPCOEF71_L		(ARM_VB_BASE + 0x033C)

/* AD01 HPF EQ6 start,  end */
#define AD01_HPCOEF0_H		(ARM_VB_BASE + 0x0400)
#define AD01_HPCOEF0_L		(ARM_VB_BASE + 0x0404)
#define AD01_HPCOEF35_H	(ARM_VB_BASE + 0x0418)
#define AD01_HPCOEF35_L		(ARM_VB_BASE + 0x041c)
#define AD01_HPCOEF42_H		(ARM_VB_BASE + 0x0550)
#define AD01_HPCOEF42_L		(ARM_VB_BASE + 0x0554)
/* AD23 HPF EQ6 start,  end */
#define AD23_HPCOEF0_H		(ARM_VB_BASE + 0x0600)
#define AD23_HPCOEF0_L		(ARM_VB_BASE + 0x0604)
#define AD23_HPCOEF35_H	(ARM_VB_BASE + 0x0618)
#define AD23_HPCOEF35_L		(ARM_VB_BASE + 0x061c)
#define AD23_HPCOEF42_H		(ARM_VB_BASE + 0x0750)
#define AD23_HPCOEF42_L		(ARM_VB_BASE + 0x0754)

#define ARM_VB_END		(ARM_VB_BASE + 0x0758)
#define IS_SPRD_VBC_RANG(reg)	(((reg) >= ARM_VB_BASE) && ((reg) < ARM_VB_END))
#define SPRD_VBC_BASE_HI	(ARM_VB_BASE & 0xFFFF0000)

#define VBADBUFFERSIZE_SHIFT	(0)
#define VBADBUFFERSIZE_MASK	(0xFF<<VBADBUFFERSIZE_SHIFT)
#define VBDABUFFERSIZE_SHIFT	(8)
#define VBDABUFFERSIZE_MASK	(0xFF<<VBDABUFFERSIZE_SHIFT)
#define VBAD23BUFFERSIZE_SHIFT	(0)
#define VBAD23BUFFERSIZE_MASK	(0xFF<<VBAD23BUFFERSIZE_SHIFT)

#define VBCHNEN			(ARM_VB_BASE + 0x00C8)
#define VBDA0_EN		(0)
#define VBDA1_EN		(1)
#define VBAD0_EN		(2)
#define VBAD1_EN		(3)
#define VBAD2_EN		(4)
#define VBAD3_EN		(5)

#define VBDACHEN_SHIFT  	(0)
#define VBADCHEN_SHIFT  	(2)
#define VBAD23CHEN_SHIFT  	(4)

#define VBAD2DMA_EN	 	(0)
#define VBAD3DMA_EN		(1)

 /* VBIISSEL */
#define VBIISSEL_AD01_PORT_SHIFT	(0)
#define VBIISSEL_AD01_PORT_MASK		(0x7)
#define VBIISSEL_AD23_PORT_SHIFT	(3)
#define VBIISSEL_AD23_PORT_MASK		(0x7<<VBIISSEL_AD23_PORT_SHIFT)
#define VBIISSEL_DA_PORT_SHIFT		(6)
#define VBIISSEL_DA_PORT_MASK		(0x7<<VBIISSEL_DA_PORT_SHIFT)
#define VBIISSEL_DA_LRCK		(14)
#define VBIISSEL_AD01_LRCK		(13)
     /* VBDATASWT */
#define VBDATASWT_AD23_LRCK		(6)
     /* DAPATHCTL */
#define VBDAPATH_DA0_ADDFM_SHIFT	(0)
#define VBDAPATH_DA0_ADDFM_MASK		(0x3<<VBDAPATH_DA0_ADDFM_SHIFT)
#define VBDAPATH_DA1_ADDFM_SHIFT	(2)
#define VBDAPATH_DA1_ADDFM_MASK		(0x3<<VBDAPATH_DA1_ADDFM_SHIFT)
     /* ADPATHCTL */
#define VBADPATH_AD0_INMUX_SHIFT    	(0)
#define VBADPATH_AD0_INMUX_MASK    	(0x3<<VBADPATH_AD0_INMUX_SHIFT)
#define VBADPATH_AD1_INMUX_SHIFT    	(2)
#define VBADPATH_AD1_INMUX_MASK    	(0x3<<VBADPATH_AD1_INMUX_SHIFT)
#define VBADPATH_AD2_INMUX_SHIFT    	(4)
#define VBADPATH_AD2_INMUX_MASK    	(0x3<<VBADPATH_AD2_INMUX_SHIFT)
#define VBADPATH_AD3_INMUX_SHIFT    	(6)
#define VBADPATH_AD3_INMUX_MASK    	(0x3<<VBADPATH_AD3_INMUX_SHIFT)
#define VBADPATH_AD0_DGMUX_SHIFT    	(8)
#define VBADPATH_AD0_DGMUX_MASK    	(0x1<<VBADPATH_AD0_DGMUX_SHIFT)
#define VBADPATH_AD1_DGMUX_SHIFT    	(9)
#define VBADPATH_AD1_DGMUX_MASK    	(0x1<<VBADPATH_AD1_DGMUX_SHIFT)
#define VBADPATH_AD2_DGMUX_SHIFT    	(10)
#define VBADPATH_AD2_DGMUX_MASK    	(0x1<<VBADPATH_AD2_DGMUX_SHIFT)
#define VBADPATH_AD3_DGMUX_SHIFT    	(11)
#define VBADPATH_AD3_DGMUX_MASK    	(0x1<<VBADPATH_AD3_DGMUX_SHIFT)
#define VBADPATH_ST0_INMUX_SHIFT	(12)
#define VBADPATH_ST0_INMUX_MASK		(0x3<<VBADPATH_ST0_INMUX_SHIFT)
#define VBADPATH_ST1_INMUX_SHIFT	(14)
#define VBADPATH_ST1_INMUX_MASK		(0x3<<VBADPATH_ST1_INMUX_SHIFT)
     /* DACSRCCTL */
#define VBDACSRC_EN			(0)
#define VBDACSRC_CLR			(1)
#define VBDACSRC_F1F2F3_BP		(3)
#define VBDACSRC_F1_SEL			(4)
#define	VBDACSRC_F0_BP			(5)
#define VBDACSRC_F0_SEL			(6)
     /* ADCSRCCTL */
#define VBADCSRC_EN_01			(0)
#define VBADCSRC_CLR_01			(1)
#define VBADCSRC_PAUSE_01		(2)
#define VBADCSRC_F1F2F3_BP_01		(3)
#define VBADCSRC_F1_SEL_01		(4)
#define VBADCSRC_EN_23			(8)
#define VBADCSRC_CLR_23			(9)
#define VBADCSRC_PAUSE_23		(10)
#define VBADCSRC_F1F2F3_BP_23		(11)
#define VBADCSRC_F1_SEL_23		(12)
/* STCTL0 */
#define VBST_EN_0			(12)
#define VBST_HPF_0			(11)
/* STCTL1 */
#define VBST_HPF_1			(11)
#define VBST_EN_1	               	(12)
#define VBST0_SEL_CHN			(13)
#define VBST1_SEL_CHN			(14)
     /* DAHPCTL */
#define VBDAEQ_HP_REG_CLR (9)
#define  VBDAC_EQ6_EN		(10)
#define  VBDAC_ALC_EN		(11)
#define  VBDAC_EQ4_EN		(12)
#define  VBDAC_EQ4_POS_SEL (13)
#define VBDAC_ALC_DP_T_MODE (14)
     /* ADHPCTL */
#define  VBADC01_EQ6_EN		(0)
#define  VBADC23_EQ6_EN		(1)
#define  VBADC01EQ_HP_REG_CLR	(2)
#define  VBADC23EQ_HP_REG_CLR	(3)

enum {
	VBC_LEFT = 0,
	VBC_RIGHT = 1,
};

enum {
	VBC_PLAYBACK = 0,
	SPRD_VBC_PLAYBACK_COUNT = 1,
	VBC_CAPTRUE = 1,
	VBC_CAPTRUE1 = 2,
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
static int vbc_src_set(int rate, int vbc_idx);
static int vbc_codec_startup(int vbc_idx, struct snd_soc_dai *dai);
static int fm_set_vbc_buffer_size(void);
static int vbc_set_phys_addr(int vbc_switch);

static inline void vbc_safe_mem_release(void **free)
{
	if (*free) {
		kfree(*free);
		*free = NULL;
	}
}

#define vbc_safe_kfree(p) vbc_safe_mem_release((void**)p)

#endif /* __VBC_COMM_H */
