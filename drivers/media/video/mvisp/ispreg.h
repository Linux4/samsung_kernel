/*
 * ispreg.h
 *
 * Marvell DxO ISP - DMA module
 *
 * Copyright:  (C) Copyright 2011 Marvell International Ltd.
 *              Henry Zhao <xzhao10@marvell.com>
 *
 * Contacts: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *	     Sakari Ailus <sakari.ailus@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef ISP_REG_H
#define ISP_REG_H

#include <mach/addr-map.h>

struct isp_reg_context {
	u32 reg;
	u32 val;
};

#define ISPDMA_MAINCTRL				0x0000
#define ISPDMA_DMA_ENA				0x0004
#define ISPDMA_CLKENA				0x0008
#define ISPDMA_IRQRAW				0x0010
#define ISPDMA_IRQMASK				0x0014
#define ISPDMA_IRQSTAT				0x0018
#define ISP_IRQRAW					0x0D00
#define ISP_IRQMASK					0x0D04
#define ISP_IRQSTAT					0x0D08
#define ISPDMA_INSZ					0x0020

#define DXO_MEM_CTRL				0x0050

#define ISPDMA_FBTX0_SDCA			0x0300
#define ISPDMA_FBTX0_DCSZ			0x0304
#define ISPDMA_FBTX0_CTRL			0x0308
#define ISPDMA_FBTX0_DSTSZ			0x030C
#define ISPDMA_FBTX0_DSTADDR		0x0310
#define ISPDMA_FBTX0_TMR			0x0324
#define ISPDMA_FBTX0_RAMCTRL		0x0328
#define ISPDMA_FBRX0_SDCA			0x0700
#define ISPDMA_FBRX0_DCSZ			0x0704
#define ISPDMA_FBRX0_CTRL			0x0708
#define ISPDMA_FBRX0_TMR			0x0724
#define ISPDMA_FBRX0_RAMCTRL		0x0728
#define ISPDMA_FBRX0_STAT			0x0730


#define ISPDMA_FBTX1_SDCA			0x0400
#define ISPDMA_FBTX1_DCSZ			0x0404
#define ISPDMA_FBTX1_CTRL			0x0408
#define ISPDMA_FBTX1_DSTSZ			0x040C
#define ISPDMA_FBTX1_DSTADDR		0x0410
#define ISPDMA_FBTX1_TMR			0x0424
#define ISPDMA_FBTX1_RAMCTRL		0x0428
#define ISPDMA_FBRX1_SDCA			0x0800
#define ISPDMA_FBRX1_DCSZ			0x0804
#define ISPDMA_FBRX1_CTRL			0x0808
#define ISPDMA_FBRX1_TMR			0x0824
#define ISPDMA_FBRX1_RAMCTRL		0x0828
#define ISPDMA_FBRX1_STAT			0x0830

#define ISPDMA_FBTX2_SDCA			0x0500
#define ISPDMA_FBTX2_DCSZ			0x0504
#define ISPDMA_FBTX2_CTRL			0x0508
#define ISPDMA_FBTX2_DSTSZ			0x050C
#define ISPDMA_FBTX2_DSTADDR		0x0510
#define ISPDMA_FBTX2_TMR			0x0524
#define ISPDMA_FBTX2_RAMCTRL		0x0528
#define ISPDMA_FBRX2_SDCA			0x0900
#define ISPDMA_FBRX2_DCSZ			0x0904
#define ISPDMA_FBRX2_CTRL			0x0908
#define ISPDMA_FBRX2_TMR			0x0924
#define ISPDMA_FBRX2_RAMCTRL		0x0928
#define ISPDMA_FBRX2_STAT			0x0930

#define ISPDMA_FBTX3_SDCA			0x0600
#define ISPDMA_FBTX3_DCSZ			0x0604
#define ISPDMA_FBTX3_CTRL			0x0608
#define ISPDMA_FBTX3_DSTSZ			0x060C
#define ISPDMA_FBTX3_DSTADDR		0x0610
#define ISPDMA_FBTX3_TMR			0x0624
#define ISPDMA_FBTX3_RAMCTRL		0x0628
#define ISPDMA_FBRX3_SDCA			0x0A00
#define ISPDMA_FBRX3_DCSZ			0x0A04
#define ISPDMA_FBRX3_CTRL			0x0A08
#define ISPDMA_FBRX3_TMR			0x0A24
#define ISPDMA_FBRX3_RAMCTRL		0x0A28
#define ISPDMA_FBRX3_STAT			0x0A30

#define ISPDMA_DISP_HSIZE			0x0114
#define ISPDMA_DISP_VSIZE			0x0118
#define ISPDMA_DISP_PITCH			0x011C
#define ISPDMA_CODEC_HSIZE			0x0214
#define ISPDMA_CODEC_VSIZE			0x0218
#define ISPDMA_CODEC_PITCH			0x021C
#define ISPDMA_CODEC_VBSZ			0x0220
#define ISPDMA_INPSDMA_CTRL			0x0C00
#define ISPDMA_INPSDMA_SRCADDR		0x0C04
#define ISPDMA_INPSDMA_SRCSZ		0x0C08
#define ISPDMA_INPSDMA_PIXSZ		0x0C0C


#define ISPDMA_DISP_CTRL			0x0108
#define ISPDMA_DISP_CTRL_1			0x013C
#define ISPDMA_DISP_DSTSZ			0x010C
#define ISPDMA_DISP_DSTADDR			0x0110
#define ISPDMA_DISP_DSTSZ_U			0x0154
#define ISPDMA_DISP_DSTADDR_U			0x0150
#define ISPDMA_DISP_DSTSZ_V			0x015C
#define ISPDMA_DISP_DSTADDR_V			0x0158
#define ISPDMA_DISP_RAMCTRL			0x0128

#define ISPDMA_CODEC_CTRL			0x0208
#define ISPDMA_CODEC_CTRL_1			0x023C
#define ISPDMA_CODEC_DSTSZ			0x020C
#define ISPDMA_CODEC_DSTADDR		0x0210
#define ISPDMA_CODEC_DSTSZ_U		0x0254
#define ISPDMA_CODEC_DSTADDR_U		0x0250
#define ISPDMA_CODEC_DSTSZ_V		0x025C
#define ISPDMA_CODEC_DSTADDR_V		0x0258
#define ISPDMA_CODEC_RAMCTRL		0x0228
#define ISPDMA_CODEC_STAT			0x0230



#define FBTX_DMA_TIMER_VAL				0x80
#define FBRX_DMA_TIMER_VAL				0x80

/*ISPDMA_DMA_ENA Settings */
#define FBRX3DMAENA						(0x1 << 9)
#define FBRX2DMAENA						(0x1 << 8)
#define FBRX1DMAENA						(0x1 << 7)
#define FBRX0DMAENA						(0x1 << 6)
#define FBTX3DMAENA						(0x1 << 5)
#define FBTX2DMAENA						(0x1 << 4)
#define FBTX1DMAENA						(0x1 << 3)
#define FBTX0DMAENA						(0x1 << 2)
#define CODEC0DMAENA					(0x1 << 1)
#define DISP0DMAENA						(0x1 << 0)


/* ISPDMA_FBTXN_SDCA */
#define ISPDMA_FBTXN_SDCA_MASK		0xFFFFFFFC

/* ISPDMA_FBTXN_DCSZ */
#define ISPDMA_FBTXN_DCSZ_MASK		0x00FFFFF8

/* ISPDMA_FBTXN_DSTSZ */
#define ISPDMA_FBTXN_DSTSZ_MASK		0x03FFFFFF

/* ISPDMA_FBTXN_TMR */
#define ISPDMA_FBTXN_TMR_MASK		0x00000FFF

/* ISPDMA_FBTXN_RAMCTRL */
#define FBTX_DCFF_PDOWN					(0x1 << 9)
#define FBTX_DCFF_WTC					(0x3 << 7)
#define FBTX_DCFF_RTC					(0x3 << 5)
#define FBTX_FF_PDWN					(0x1 << 4)
#define FBTX_FF_WTC						(0x3 << 2)
#define FBTX_FF_RTC						(0x3 << 0)

/* ISPDMA_FBTXN_CTRL */
#define FBTX_EOFSEL						(0x3 << 12)
#define FBTX_FFMAXLVL					(0xF << 8)
#define FBTX_DMABRSTSZ					(0x3 << 4)
#define FBTX_DC_ENA						(0x1 << 1)

/* ISPDMA_FBRXN_SDCA */
#define ISPDMA_FBRXN_SDCA_MASK		0xFFFFFFFC

/* ISPDMA_FBRXN_DCSZ */
#define ISPDMA_FBRXN_DCSZ_MASK		0x00FFFFF8

/* ISPDMA_FBRXN_TMR */
#define ISPDMA_FBRXN_TMR_MASK		0x00000FFF

/* ISPDMA_FBRXN_RAMCTRL */
#define FBRX_DCFF_PDOWN					(0x1 << 9)
#define FBRX_DCFF_WTC					(0x3 << 7)
#define FBRX_DCFF_RTC					(0x3 << 5)
#define FBRX_FF_PDWN					(0x1 << 4)
#define FBRX_FF_WTC						(0x3 << 2)
#define FBRX_FF_RTC						(0x3 << 0)

/* ISPDMA_FBRXN_CTRL */
#define FBRX_DMABRSTSZ					(0x3 << 4)
#define FBRX_DC_ENA						(0x1 << 1)

/* ISPDMA_CLKENA */
#define FB0CLKENA						(0x3 << 0)
#define FB0CLK_VAL_HW0					(0x0 << 0)
#define FB0CLK_VAL_HW1					(0x1 << 0)
#define FB0CLK_VAL_SW_OFF				(0x2 << 0)
#define FB0CLK_VAL_SW_ON				(0x3 << 0)

/* ISPDMA_IRQSTAT */
#define	DISP_DMA_EOF					(0X1 << 0)
#define	CODEC_DMA_EOF					(0X1 << 1)
#define	FBTX0_DMA_EOF					(0X1 << 2)
#define	FBTX1_DMA_EOF					(0X1 << 3)
#define	FBTX2_DMA_EOF					(0X1 << 3)
#define	FBTX3_DMA_EOF					(0X1 << 5)
#define	FBRX0_DMA_EOF					(0X1 << 6)
#define	FBRX1_DMA_EOF					(0X1 << 7)
#define	FBRX2_DMA_EOF					(0X1 << 8)
#define	FBRX3_DMA_EOF					(0X1 << 9)
#define INPUT_DMA_EOF					(0x1 << 10)
#define DISP_PS_EOF						(0x1 << 11)
#define CODEC_PS_EOF					(0x1 << 12)
#define FBTX0_PS_EOF					(0x1 << 13)
#define FBTX1_PS_EOF					(0x1 << 14)
#define FBTX2_PS_EOF					(0x1 << 15)
#define FBTX3_PS_EOF					(0x1 << 16)
#define CSI2_BRIDEG						(0x1 << 17)

#define CCIC1_VIRT_BASE					(AXI_VIRT_BASE + 0xA000)
#define CCIC1_REG(x)					(CCIC1_VIRT_BASE + (x))

#define CCIC2_VIRT_BASE					(AXI_VIRT_BASE + 0xA800)
#define CCIC2_REG(x)					(CCIC2_VIRT_BASE + (x))

#define CCIC_CLKDIV					0x0000FFFF

#define CCIC_Y0_BASE_ADDR				0x0000
#define CCIC_Y1_BASE_ADDR				0x0004
#define CCIC_Y2_BASE_ADDR				0x0008
#define CCIC_U0_BASE_ADDR				0x000C
#define CCIC_U1_BASE_ADDR				0x0010
#define CCIC_U2_BASE_ADDR				0x0014
#define CCIC_V0_BASE_ADDR				0x0018
#define CCIC_V1_BASE_ADDR				0x001C
#define CCIC_V2_BASE_ADDR				0x0020
#define CCIC_IMG_PITCH					0x0024
#define CCIC_IRQ_RAW_STATUS				0x0028
#define CCIC_IRQ_MASK					0x002C
#define CCIC_IRQ_STATUS					0x0030
#define CCIC_IMG_SIZE					0x0034
#define CCIC_IMG_OFFSET					0x0038
#define CCIC_CTRL_0						0x003C
#define CCIC_CTRL_1						0x0040
#define CCIC_CTRL_2						0x0044
#define CCIC_LNNUM						0x0060
#define CCIC_CLOCK_CTRL					0x0088
#define CCIC_SRAM_TC0_TEST_ONLY			0x008C
#define CCIC_SRAM_TC1_TEST_ONLY			0x0090
#define CCIC_CSI2_CTRL0					0x0100
#define CCIC_CSI2_IRQ_RAW_STATUS		0x0108
#define CCIC_CSI2_GSPFF					0x0110
#define CCIC_CSI2_VCCTRL				0x0114
#define CCIC_CSI2_DPHY3					0x012C
#define CCIC_CSI2_DPHY5					0x0134
#define CCIC_CSI2_DPHY6					0x0138

#define CCIC_IRQ_STATUS_EOF0			0x0001
#define CCIC_IRQ_STATUS_EOF1			0x0002
#define CCIC_IRQ_STATUS_EOF2			0x0004

#endif	/* ISP_REG_H */
