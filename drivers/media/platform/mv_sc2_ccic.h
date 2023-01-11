/*
 * Copyright (C) 2011 Marvell International Ltd.
 *				 Libin Yang <lbyang@marvell.com>
 *
 * This driver is based on soc_camera.
 * Ported from Jonathan's marvell-ccic driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MV_SC2_CCIC_H
#define __MV_SC2_CCIC_H

#include <media/mv_sc2.h>

#define REG_Y0BAR	0x00
#define REG_U0BAR	0x0c
#define REG_V0BAR	0x18

#define REG_IMGPITCH	0x24	/* Image pitch register */
#define	  IMGP_YP_SHFT	  2		/* Y pitch params */
#define	  IMGP_YP_MASK	  0x00003ffc	/* Y pitch field */
#define	  IMGP_UVP_SHFT	  18		/* UV pitch (planar) */
#define	  IMGP_UVP_MASK	  0x3ffc0000

#define REG_IRQSTATRAW	0x28	/* RAW IRQ Status */
#define REG_IRQMASK	0x2c	/* IRQ mask - same bits as IRQSTAT */
#define REG_IRQSTAT	0x30	/* IRQ status / clear */
#define	  IRQ_EOF0	  0x00000001	/* End of frame 0 */
#define	  IRQ_SOF0	  0x00000008	/* Start of frame 0 */
#define	  IRQ_DMA_NOT_DONE		0x00000010
#define	  IRQ_SHADOW_NOT_RDY	0x00000020
#define	  IRQ_OVERFLOW	  0x00000040	/* FIFO overflow */
#define	  IRQ_CSI2IDI_HBLK2HSYNC	0x00000400
#define	  FRAMEIRQS	  (IRQ_EOF0 | IRQ_SOF0)
#define	  ALLIRQS	  (FRAMEIRQS | IRQ_OVERFLOW | IRQ_DMA_NOT_DONE | \
			IRQ_SHADOW_NOT_RDY | IRQ_CSI2IDI_HBLK2HSYNC)

#define REG_IMGSIZE	0x34	/* Image size */
#define	 IMGSZ_V_MASK	  0x1fff0000
#define	 IMGSZ_V_SHIFT	  16
#define	 IMGSZ_H_MASK	  0x00003fff
#define	 IMGSZ_H_SHIFT	  0

#define REG_IMGOFFSET	0x38	/* IMage offset */

#define REG_CTRL0	0x3c	/* Control 0 */
#define	  C0_ENABLE	  0x00000001	/* Makes the whole thing go */
/* Mask for all the format bits */
#define	  C0_DF_MASK	  0x08fffffc	/* Bits 2-23 */
/* RGB ordering */
#define	  C0_RGB4_RGBX	  0x00000000
#define	  C0_RGB4_XRGB	  0x00000004
#define	  C0_RGB4_BGRX	  0x00000008
#define	  C0_RGB4_XBGR	  0x0000000c
#define	  C0_RGB5_RGGB	  0x00000000
#define	  C0_RGB5_GRBG	  0x00000004
#define	  C0_RGB5_GBRG	  0x00000008
#define	  C0_RGB5_BGGR	  0x0000000c
/* YUV4222 to 420 Semi-Planar Enable */
#define	  C0_YUV420SP	  0x00000010
/* Spec has two fields for DIN and DOUT, but they must match, so
   combine them here. */
#define	  C0_DF_YUV	  0x00000000	/* Data is YUV */
#define	  C0_DF_RGB	  0x000000a0	/* Data is RGB */
#define	  C0_DF_BAYER	  0x00000140	/* Data is Bayer */
/* 8-8-8 must be missing from the below - ask */
#define	  C0_RGBF_565	  0x00000000
#define	  C0_RGBF_444	  0x00000800
#define	  C0_RGB_BGR	  0x00001000	/* Blue comes first */
#define	  C0_YUV_PLANAR	  0x00000000	/* YUV 422 planar format */
#define	  C0_YUV_PACKED	  0x00008000	/* YUV 422 packed format */
#define	  C0_YUV_420PL	  0x0000a000	/* YUV 420 planar format */
/* Think that 420 packed must be 111 - ask */
#define	  C0_YUVE_YUYV	  0x00000000	/* Y1CbY0Cr */
#define	  C0_YUVE_YVYU	  0x00010000	/* Y1CrY0Cb */
#define	  C0_YUVE_VYUY	  0x00020000	/* CrY1CbY0 */
#define	  C0_YUVE_UYVY	  0x00030000	/* CbY1CrY0 */
#define	  C0_YUVE_XYUV	  0x00000000	/* 420: .YUV */
#define	  C0_YUVE_XYVU	  0x00010000	/* 420: .YVU */
#define	  C0_YUVE_XUVY	  0x00020000	/* 420: .UVY */
#define	  C0_YUVE_XVUY	  0x00030000	/* 420: .VUY */
/* Bayer bits 18,19 if needed */
#define	  C0_HPOL_LOW	  0x01000000	/* HSYNC polarity active low */
#define	  C0_VPOL_LOW	  0x02000000	/* VSYNC polarity active low */
#define	  C0_VCLK_LOW	  0x04000000	/* VCLK on falling edge */
#define	  C0_420SP_UVSWAP	0x08000000	/* YUV420SP U/V Swap */
#define	  C0_SIFM_MASK	  0xc0000000	/* SIF mode bits */
#define	  C0_SIF_HVSYNC	  0x00000000	/* Use H/VSYNC */
#define	  C0_SOF_NOSYNC	  0x40000000	/* Use inband active signaling */
#define	  C0_EOF_VSYNC	  0x00400000	/* Generate EOF by VSYNC */
#define	  C0_VEDGE_CTRL	  0x00800000	/* Detecting falling edge of VSYNC */
/* bit 21: fifo overrun auto-recovery */
#define	  C0_EOFFLUSH	  0x00200000
/* bit 27 YUV420SP_UV_SWAP */
#define	  C0_YUV420SP_UV_SWAP	  0x08000000

#define REG_CTRL1	0x40	/* Control 1 */
#define	  C1_SENCLKGATE		0x00000001	/* Sensor Clock Gate */
#define	  C1_UNUSED			0x0000003c
#define	  C1_RESVZ			0x000dffc0
#define	  C1_DMAB_LENSEL	0x00020000	/* set 1, coupled CCICx */
#define	  C1_444ALPHA		0x00f00000	/* Alpha field in RGB444 */
#define	  C1_ALPHA_SHFT	  20
#define	  C1_AWCACHE	  0x00100000 /* set 1. coupled CCICx */
#define	  C1_DMAB64	  0x00000000	/* 64-byte DMA burst */
#define	  C1_DMAB128	  0x02000000	/* 128-byte DMA burst */
#define	  C1_DMAB256	  0x04000000	/* 256-byte DMA burst */
#define	  C1_DMAB_MASK	  0x06000000
#define	  C1_SHADOW_RDY	  0x08000000 /* set it 1 when BAR is set*/
#define	  C1_PWRDWN	  0x10000000	/* Power down */
#define	  C1_DMAPOSTED	  0x40000000	/* DMA Posted Select */

#define REG_CTRL2	0x44	/* Control 2 */
/* recommend set 1 to disable legacy calc DMA line num */
#define	  C2_LGCY_LNNUM		0x80000000
/* recommend set 1 to disaable legacy CSI2 hblank */
#define	  C2_LGCY_HBLANK	0x40000000

#define REG_CTRL3	0x48	/* Control 2 */

#define REG_LNNUM	0x60	/* Lines num DMA filled */

#define	  CLK_DIV_MASK	  0x0000ffff	/* Upper bits RW "reserved" */
#define REG_FRAME_CNT 0x23C

/* MIPI */
#define REG_CSI2_CTRL0	0x100
#define		CSI2_C0_ENABLE	0x01
#define		CSI2_C0_LANE_NUM(n)	(((n)-1) << 1)
#define		CSI2_C0_LANE_NUM_MASK	0x06
#define		CSI2_C0_VLEN	(0x4 << 4)
#define		CSI2_C0_VLEN_MASK	(0xf << 4)
#define REG_CSI2_CTRL2	0x140
#define		CSI2_C2_DPCM_REPACK_MUX_SEL_VCDT	0x02000000
#define		CSI2_C2_DPCM_REPACK_MUX_SEL_OTHER	0x04000000
#define		CSI2_C2_IDI_MUX_SEL_DPCM	0x01000000
#define		CSI2_C2_REPACK_RST	0x00200000
#define		CSI2_C2_REPACK_ENA	0x00010000
#define		CSI2_C2_DPCM_ENA	0x00000001
#define REG_CSI2_DPHY3	0x12c
#define		CSI2_DPHY3_HS_SETTLE_SHIFT	8
#define REG_CSI2_DPHY5	0x134
#define		CSI2_DPHY5_LANE_RESC_ENA_SHIFT	4
#define		CSI2_DPHY5_LANE_ENA(n)	((1 << (n)) - 1)
#define REG_CSI2_DPHY6	0x138
#define		CSI2_DPHY6_CK_SETTLE_SHIFT	8
#define REG_CSI2_CTRL3	0x144

/* IDI */
#define REG_IDI_CTRL	0x310
#define		IDI_SEL_MASK	0x06
#define		IDI_SEL_DPCM_REPACK	0x00
#define		IDI_SEL_PARALLEL	0x02
#define		IDI_SEL_AHB		0x04
#define		IDI_RELEASE_RESET	0x01

/* APMU */
#define REG_CLK_CCIC_RES 0x50
#define REG_CLK_CCIC2_RES 0x24

#define CF_SINGLE_BUF	0
#define CF_FRAME_SOF0	1
#define CF_FRAME_OVERFLOW	2


struct msc2_frame_stat {
	int frames;
	int singles;
	int delivered;
};

/*
 * Device register I/O
 */
static inline u32 ccic_reg_read(struct msc2_ccic_dev *ccic_dev,
			unsigned int reg)
{
	return ioread32(ccic_dev->base + reg);
}

static inline void ccic_reg_write(struct msc2_ccic_dev *ccic_dev,
			unsigned int reg, u32 val)
{
	iowrite32(val, ccic_dev->base + reg);
}

static inline void ccic_reg_write_mask(struct msc2_ccic_dev *ccic_dev,
			unsigned int reg, u32 val, u32 mask)
{
	u32 v = ccic_reg_read(ccic_dev, reg);

	v = (v & ~mask) | (val & mask);
	ccic_reg_write(ccic_dev, reg, v);
}

static inline void ccic_reg_set_bit(struct msc2_ccic_dev *ccic_dev,
			unsigned int reg, u32 val)
{
	ccic_reg_write_mask(ccic_dev, reg, val, val);
}

static inline void ccic_reg_clear_bit(struct msc2_ccic_dev *ccic_dev,
			unsigned int reg, u32 val)
{
	ccic_reg_write_mask(ccic_dev, reg, 0, val);
}

static void ccic_frameirq_enable(struct msc2_ccic_dev *ccic_dev)
{
	ccic_reg_write(ccic_dev, REG_IRQSTAT, ALLIRQS);
	ccic_reg_set_bit(ccic_dev, REG_IRQMASK, ALLIRQS);
}

static void ccic_frameirq_disable(struct msc2_ccic_dev *ccic_dev)
{
	ccic_reg_clear_bit(ccic_dev, REG_IRQMASK, ALLIRQS);
}

#define d_inf(level, fmt, arg...)					\
	do {								\
		if (trace >= level)					\
			pr_debug("%s: " fmt "\n", KBUILD_BASENAME,	\
							## arg);	\
	} while (0)
#endif
