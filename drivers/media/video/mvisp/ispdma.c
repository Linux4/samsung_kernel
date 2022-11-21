/*
 * ispdma.c
 *
 * Marvell DxO ISP - DMA module
 *	Based on omap3isp
 *
 * Copyright:  (C) Copyright 2011 Marvell International Ltd.
 *              Henry Zhao <xzhao10@marvell.com>
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


#include <linux/device.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/clk.h>


#include "isp.h"
#include "ispreg.h"
#include "ispdma.h"
#include <mach/pxa168fb.h>

#define MS_PER_JIFFIES  10

#define ISPDMA_MAX_IN_WIDTH			0x3FFF
#define ISPDMA_MAX_IN_HEIGHT		0x1FFF
#define ISPDMA_MAX_DISP_PITCH		0x3FFFFFF
#define ISPDMA_MAX_DISP_WIDTH		0x3FFFFFF
#define ISPDMA_MAX_DISP_HEIGHT		0x1FFF
#define ISPDMA_MAX_CODEC_PITCH		0x3FFFFFF
#define ISPDMA_MAX_CODEC_WIDTH		0x3FFFFFF
#define ISPDMA_MAX_CODEC_HEIGHT		0x1FFF

#define FBTX0_DMAENA		0x2
#define FBTX1_DMAENA		0x3
#define FBTX2_DMAENA		0x4
#define FBTX3_DMAENA		0x5
#define FBRX0_DMAENA		0x6
#define FBRX1_DMAENA		0x7
#define FBRX2_DMAENA		0x8
#define FBRX3_DMAENA		0x9
#define FBDMAENA_MAX		0xA

#define IS_CH_ENABLED(regval, ch) (regval & (0x1 << ch))
#define IS_TX_CH(ch) (ch >= FBTX0_DMAENA && ch <= FBTX3_DMAENA)
#define IS_RX_CH(ch) (ch >= FBRX0_DMAENA && ch <= FBRX3_DMAENA)
#define CLEAR_DMA_EN(reg, ch) (reg &= ~(0x1 << ch))
#define CLEAR_CLK_EN(reg, ch) do {\
	reg &= ~(0x3 << (ch - FBTX0_DMAENA));\
	reg |= (0x2 << (ch - FBTX0_DMAENA));\
	} while (0)

#define CLEAR_TX_IRQ_MASK(reg, ch) (reg &= ~((0x1 << ch)|(0x1 << (ch + 11))))
#define CLEAR_RX_IRQ_MASK(reg, ch) (reg &= ~(0x1 << ch))

#define SET_DMA_EN(reg, ch) (reg |= (0x1 << ch))
#define SET_CLK_EN(reg, ch) (reg |= (0x3 << (ch - FBTX0_DMAENA)))
#define SET_TX_IRQ_MASK(reg, ch) (reg |= ((0x1 << ch)|(0x1 << (ch + 11))))
#define SET_RX_IRQ_MASK(reg, ch) (reg |= (0x1 << ch))
struct isp_ispdma_device *g_ispdma;
static atomic_t isp_reset_count;

u32 *ispdma_desc_chain[4];
int ispdma_desc_chain_size;
int desc_num;

struct isp_reg_context ispdma_reg_list[ISPDMA_INPSDMA_MAX_CTX] = {
	{ISPDMA_IRQMASK, 0},
	{ISPDMA_DMA_ENA, 0},
	{ISPDMA_INPSDMA_CTRL, 0},
	{ISPDMA_CLKENA, 0},
	{ISPDMA_MAINCTRL, 0},
	{ISPDMA_INSZ, 0},
	{ISPDMA_FBTX0_SDCA, 0},
	{ISPDMA_FBTX0_DCSZ, 0},
	{ISPDMA_FBTX0_CTRL, 0},
	{ISPDMA_FBTX0_DSTSZ, 0},
	{ISPDMA_FBTX0_DSTADDR, 0},
	{ISPDMA_FBTX0_TMR, 0},
	{ISPDMA_FBTX0_RAMCTRL, 0},
	{ISPDMA_FBRX0_SDCA, 0},
	{ISPDMA_FBRX0_DCSZ, 0},
	{ISPDMA_FBRX0_CTRL, 0},
	{ISPDMA_FBRX0_TMR, 0},
	{ISPDMA_FBRX0_RAMCTRL, 0},
	{ISPDMA_FBRX0_STAT, 0},
	{ISPDMA_FBTX1_SDCA, 0},
	{ISPDMA_FBTX1_DCSZ, 0},
	{ISPDMA_FBTX1_CTRL, 0},
	{ISPDMA_FBTX1_DSTSZ, 0},
	{ISPDMA_FBTX1_DSTADDR, 0},
	{ISPDMA_FBTX1_TMR, 0},
	{ISPDMA_FBTX1_RAMCTRL, 0},
	{ISPDMA_FBRX1_SDCA, 0},
	{ISPDMA_FBRX1_DCSZ, 0},
	{ISPDMA_FBRX1_CTRL, 0},
	{ISPDMA_FBRX1_TMR, 0},
	{ISPDMA_FBRX1_RAMCTRL, 0},
	{ISPDMA_FBRX1_STAT, 0},
	{ISPDMA_FBTX2_SDCA, 0},
	{ISPDMA_FBTX2_DCSZ, 0},
	{ISPDMA_FBTX2_CTRL, 0},
	{ISPDMA_FBTX2_DSTSZ, 0},
	{ISPDMA_FBTX2_DSTADDR, 0},
	{ISPDMA_FBTX2_TMR, 0},
	{ISPDMA_FBTX2_RAMCTRL, 0},
	{ISPDMA_FBRX2_SDCA, 0},
	{ISPDMA_FBRX2_DCSZ, 0},
	{ISPDMA_FBRX2_CTRL, 0},
	{ISPDMA_FBRX2_TMR, 0},
	{ISPDMA_FBRX2_RAMCTRL, 0},
	{ISPDMA_FBRX2_STAT, 0},
	{ISPDMA_FBTX3_SDCA, 0},
	{ISPDMA_FBTX3_DCSZ, 0},
	{ISPDMA_FBTX3_CTRL, 0},
	{ISPDMA_FBTX3_DSTSZ, 0},
	{ISPDMA_FBTX3_DSTADDR, 0},
	{ISPDMA_FBTX3_TMR, 0},
	{ISPDMA_FBTX3_RAMCTRL, 0},
	{ISPDMA_FBRX3_SDCA, 0},
	{ISPDMA_FBRX3_DCSZ, 0},
	{ISPDMA_FBRX3_CTRL, 0},
	{ISPDMA_FBRX3_TMR, 0},
	{ISPDMA_FBRX3_RAMCTRL, 0},
	{ISPDMA_FBRX3_STAT, 0},
	{ISPDMA_DISP_CTRL, 0},
	{ISPDMA_DISP_CTRL_1, 0},
	{ISPDMA_DISP_DSTSZ, 0},
	{ISPDMA_DISP_DSTADDR, 0},
	{ISPDMA_DISP_DSTSZ_U, 0},
	{ISPDMA_DISP_DSTADDR_U, 0},
	{ISPDMA_DISP_DSTSZ_V, 0},
	{ISPDMA_DISP_DSTADDR_V, 0},
	{ISPDMA_DISP_RAMCTRL, 0},
	{ISPDMA_DISP_PITCH, 0},
	{ISPDMA_CODEC_CTRL, 0},
	{ISPDMA_CODEC_CTRL_1, 0},
	{ISPDMA_CODEC_DSTSZ, 0},
	{ISPDMA_CODEC_DSTADDR, 0},
	{ISPDMA_CODEC_DSTSZ_U, 0},
	{ISPDMA_CODEC_DSTADDR_U, 0},
	{ISPDMA_CODEC_DSTSZ_V, 0},
	{ISPDMA_CODEC_DSTADDR_V, 0},
	{ISPDMA_CODEC_RAMCTRL, 0},
	{ISPDMA_CODEC_STAT, 0},
	{ISPDMA_CODEC_PITCH, 0},
	{ISPDMA_CODEC_VBSZ, 0},
	{ISPDMA_INPSDMA_SRCADDR, 0},
	{ISPDMA_INPSDMA_SRCSZ, 0},
	{ISPDMA_INPSDMA_PIXSZ, 0},
	{ISP_IRQMASK, 0},
};

inline unsigned long get_dma_working_flag(struct isp_ispdma_device *ispdma)
{
	unsigned long dma_flags;
	unsigned long dma_working_flag;

	spin_lock_irqsave(&ispdma->dmaflg_lock, dma_flags);
	dma_working_flag = ispdma->dma_working_flag;
	spin_unlock_irqrestore(&ispdma->dmaflg_lock, dma_flags);

	return dma_working_flag;
}

static void __maybe_unused ispdma_reg_dump(
		struct isp_ispdma_device *ispdma)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	int cnt;

	for (cnt = 0; cnt < ISPDMA_INPSDMA_MAX_CTX; cnt++) {
		ispdma_reg_list[cnt].val =
			mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA,
				ispdma_reg_list[cnt].reg);

		dev_warn(isp->dev, "REG[0x%08X]--->0x%08X\n",
			ispdma_reg_list[cnt].reg,
			ispdma_reg_list[cnt].val);
	}

	return;
}

static int ispdma_dump_regs(struct isp_ispdma_device *ispdma,
			struct v4l2_ispdma_dump_registers *regs)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);

	if (NULL == regs || NULL == isp)
		return -EINVAL;

	regs->ispdma_mainctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_MAINCTRL);
	regs->ispdma_dmaena = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
	regs->ispdma_clkena = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);
	regs->ispdma_irqraw = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQRAW);
	regs->ispdma_irqmask = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
	regs->ispdma_irqstat = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQSTAT);
	regs->ispdma_insz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_INSZ);
	regs->ispdma_inpsdma_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);
	regs->ispdma_fbtx0_sdca = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_SDCA);
	regs->ispdma_fbtx0_dcsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_DCSZ);
	regs->ispdma_fbtx0_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_CTRL);
	regs->ispdma_fbtx0_dstsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_DSTSZ);
	regs->ispdma_fbtx0_dstaddr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_DSTADDR);
	regs->ispdma_fbtx0_tmr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_TMR);
	regs->ispdma_fbtx0_ramctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_RAMCTRL);
	regs->ispdma_fbrx0_sdca = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_SDCA);
	regs->ispdma_fbrx0_dcsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_DCSZ);
	regs->ispdma_fbrx0_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_CTRL);
	regs->ispdma_fbrx0_tmr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_TMR);
	regs->ispdma_fbrx0_ramctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_RAMCTRL);
	regs->ispdma_fbrx0_stat = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_STAT);
	regs->ispdma_fbtx1_sdca = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX1_SDCA);
	regs->ispdma_fbtx1_dcsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX1_DCSZ);
	regs->ispdma_fbtx1_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX1_CTRL);
	regs->ispdma_fbtx1_dstsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX1_DSTSZ);
	regs->ispdma_fbtx1_dstaddr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX1_DSTADDR);
	regs->ispdma_fbtx1_tmr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX1_TMR);
	regs->ispdma_fbtx1_ramctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX1_RAMCTRL);
	regs->ispdma_fbrx1_sdca = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX1_SDCA);
	regs->ispdma_fbrx1_dcsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX1_DCSZ);
	regs->ispdma_fbrx1_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX1_CTRL);
	regs->ispdma_fbrx1_tmr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX1_TMR);
	regs->ispdma_fbrx1_ramctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX1_RAMCTRL);
	regs->ispdma_fbrx1_stat = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX1_STAT);
	regs->ispdma_fbtx2_sdca = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX2_SDCA);
	regs->ispdma_fbtx2_dcsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX2_DCSZ);
	regs->ispdma_fbtx2_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX2_CTRL);
	regs->ispdma_fbtx2_dstsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX2_DSTSZ);
	regs->ispdma_fbtx2_dstaddr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX2_DSTADDR);
	regs->ispdma_fbtx2_tmr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX2_TMR);
	regs->ispdma_fbtx2_ramctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX2_RAMCTRL);
	regs->ispdma_fbrx2_sdca = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX2_SDCA);
	regs->ispdma_fbrx2_dcsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX2_DCSZ);
	regs->ispdma_fbrx2_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX2_CTRL);
	regs->ispdma_fbrx2_tmr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX2_TMR);
	regs->ispdma_fbrx2_ramctrl	= mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX2_RAMCTRL);
	regs->ispdma_fbrx2_stat = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX2_STAT);
	regs->ispdma_fbrx2_sdca = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX3_SDCA);
	regs->ispdma_fbtx3_dcsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX3_DCSZ);
	regs->ispdma_fbtx3_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX3_CTRL);
	regs->ispdma_fbtx3_dstsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX3_DSTSZ);
	regs->ispdma_fbtx3_dstaddr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX3_DSTADDR);
	regs->ispdma_fbtx3_tmr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX3_TMR);
	regs->ispdma_fbtx3_ramctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX3_RAMCTRL);
	regs->ispdma_fbtx3_sdca = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX3_SDCA);
	regs->ispdma_fbrx3_dcsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX3_DCSZ);
	regs->ispdma_fbrx3_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX3_CTRL);
	regs->ispdma_fbrx3_tmr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX3_TMR);
	regs->ispdma_fbrx3_ramctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX3_RAMCTRL);
	regs->ispdma_fbrx3_stat = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX3_STAT);
	regs->ispdma_disp_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DISP_CTRL);
	regs->ispdma_disp_dstsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DISP_DSTSZ);
	regs->ispdma_disp_dstaddr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DISP_DSTADDR);
	regs->ispdma_disp_ramctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DISP_RAMCTRL);
	regs->ispdma_disp_pitch = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DISP_PITCH);
	regs->ispdma_codec_ctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CODEC_CTRL);
	regs->ispdma_codec_dstsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CODEC_DSTSZ);
	regs->ispdma_codec_dstaddr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CODEC_DSTADDR);
	regs->ispdma_codec_ramctrl = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CODEC_RAMCTRL);
	regs->ispdma_codec_stat = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CODEC_STAT);
	regs->ispdma_codec_pitch = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CODEC_PITCH);
	regs->ispdma_codec_vbsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CODEC_VBSZ);
	regs->ispdma_inpsdma_srcaddr = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_SRCADDR);
	regs->ispdma_inpsdma_srcsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_SRCSZ);
	regs->ispdma_inpsdma_pixsz = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_PIXSZ);
	regs->isp_irqraw = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISP_IRQRAW);
	regs->isp_irqmask = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISP_IRQMASK);
	regs->isp_irqstat = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISP_IRQSTAT);

	return 0;

}

static inline u32 cyc2us(u32 cycle)
{
	return (cycle >> 2) + (cycle >> 5) + (cycle >> 6)
		+ (cycle >> 7) + (cycle >> 9) + (cycle >> 10);
}

static inline u32 read_timestamp(void)
{
	unsigned long r1, r2;
	__asm__ __volatile__ ("mrrc p15, 0, %0, %1, c14"
		: "=r" (r1), "=r" (r2) : : "cc");
	return r1;
}

static int ispdma_getdelta(struct v4l2_ispdma_timeinfo *param, bool delta)
{
	unsigned long cur_us = 0;
	struct timeval tv;

	if (NULL == param)
		return -EINVAL;

	do_gettimeofday(&tv);
	cur_us = tv.tv_sec * 1000000 + tv.tv_usec;

	if (delta)
		param->delta = cur_us - param->timestamp;
	else
		param->delta = 0;

	param->timestamp = cur_us;

	return 0;
}

static int ispdma_wait_ipc(struct isp_ispdma_device *ispdma,
		struct v4l2_dxoipc_ipcwait *ipc_wait)
{
	int ret = 0;
	unsigned long flags;
	long rc = 0;
	struct mvisp_device *isp = to_mvisp_device(ispdma);

	rc = wait_for_completion_timeout(&ispdma->ipc_event,
		(ipc_wait->timeout + MS_PER_JIFFIES - 1) / MS_PER_JIFFIES);
	spin_lock_irqsave(&ispdma->ipc_irq_lock, flags);
	if (rc == 0) {
		dev_warn(isp->dev, "IPC timeout %d - %d, eof %d, ipc %d, dma %d\n",
			ispdma->disp_mipi_ovr_cnt,
			ispdma->codec_mipi_ovr_cnt,
			ispdma->disp_eof_cnt,
			ispdma->ipc_event_cnt,
			ispdma->dma_event_cnt);
		ret = -ETIMEDOUT;
	}

	INIT_COMPLETION(ispdma->ipc_event);
	ipc_wait->tickinfo.timestamp = ispdma->tickinfo.timestamp;
	spin_unlock_irqrestore(&ispdma->ipc_irq_lock, flags);

	return ret;
}

void mv_ispdma_ipc_isr_handler(struct isp_ispdma_device *ispdma)
{
	unsigned long flags;

	spin_lock_irqsave(&ispdma->ipc_irq_lock, flags);
	ispdma->ipc_event_cnt++;
	ispdma_getdelta(&ispdma->tickinfo, false);
	complete_all(&ispdma->ipc_event);
	spin_unlock_irqrestore(&ispdma->ipc_irq_lock, flags);
}

static int ispdma_stream_cfg(struct isp_ispdma_device *ispdma,
		struct v4l2_dxoipc_streaming_config *stream_cfg)
{
	u32 reg_clk_en, reg_dma_en, reg_dma_en_old, reg_irq_mask;
	int fb_ch;
	struct mvisp_device *isp = to_mvisp_device(ispdma);

	mutex_lock(&ispdma->ispdma_mutex);

	reg_dma_en = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
	reg_clk_en = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);
	reg_irq_mask = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);

	reg_dma_en_old = reg_dma_en;
	for (fb_ch = FBTX0_DMAENA; fb_ch < FBDMAENA_MAX; fb_ch++) {
		if (IS_CH_ENABLED(reg_dma_en, fb_ch)) {
			if (IS_TX_CH(fb_ch))
				if (stream_cfg->enable_fbtx == 0) {
					CLEAR_DMA_EN(reg_dma_en, fb_ch);
					CLEAR_CLK_EN(reg_clk_en, fb_ch);
					CLEAR_TX_IRQ_MASK(reg_irq_mask, fb_ch);
				}
			if (IS_RX_CH(fb_ch))
				if (stream_cfg->enable_fbrx == 0) {
					CLEAR_DMA_EN(reg_dma_en, fb_ch);
					CLEAR_CLK_EN(reg_clk_en, fb_ch);
					CLEAR_RX_IRQ_MASK(reg_irq_mask, fb_ch);
				}
		} else {
			if (IS_TX_CH(fb_ch))
				if ((stream_cfg->enable_fbtx != 0)
					&& (fb_ch - FBTX0_DMAENA <
						ispdma->framebuf_count)) {
					SET_DMA_EN(reg_dma_en, fb_ch);
					SET_CLK_EN(reg_clk_en, fb_ch);
					SET_TX_IRQ_MASK(reg_irq_mask, fb_ch);
				}
			if (IS_RX_CH(fb_ch))
				if ((stream_cfg->enable_fbrx != 0)
					&& (fb_ch - FBRX0_DMAENA <
						ispdma->framebuf_count)) {
					SET_DMA_EN(reg_dma_en, fb_ch);
					SET_CLK_EN(reg_clk_en, fb_ch);
					SET_RX_IRQ_MASK(reg_irq_mask, fb_ch);
				}
		}
	}

	if (reg_dma_en != reg_dma_en_old) {
		mvisp_reg_writel(isp, reg_clk_en,
				ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);
		mvisp_reg_writel(isp, reg_irq_mask,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
		mvisp_reg_writel(isp, reg_dma_en,
				ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
	}

	mutex_unlock(&ispdma->ispdma_mutex);

	if (!stream_cfg->enable_fbrx) {
		atomic_set(&isp_reset_count, 1);
		wait_for_completion_timeout(&ispdma->isp_reset_event,
				HZ >> 1);
	}

	return 0;
}

static int ispdma_set_fb_reg_dc(struct mvisp_device *isp,
		int index, int num, struct v4l2_dxoipc_set_fb *cfg_fb)
{
	u32 regoffset;
	u32 regval;

	regoffset = index * 0x100;

	/*  TX Settings */
	regval = __pa(ispdma_desc_chain[index]);
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_SDCA + regoffset);
	regval = sizeof(u32)*num;
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_DCSZ + regoffset);

	regval = FBTX_DMA_TIMER_VAL;
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_TMR + regoffset);

	regval = 0x400 | ((cfg_fb->burst_write >> 3) & FBTX_DMABRSTSZ);
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_CTRL + regoffset);

	/*  RX Settings */

	regval = __pa(ispdma_desc_chain[index]);
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_SDCA + regoffset);
	regval = sizeof(u32)*num;
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_DCSZ + regoffset);

	regval = FBRX_DMA_TIMER_VAL;
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_TMR + regoffset);

	regval = (cfg_fb->burst_read >> 3) & FBRX_DMABRSTSZ;
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_CTRL + regoffset);

	return 0;
}

static void ispdma_desc_enable(struct mvisp_device *isp,
		int index)
{
	u32 regoffset;
	u32 regval;

	regoffset = index * 0x100;

	regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_CTRL + regoffset);
	regval |= FBTX_DC_ENA;
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_CTRL + regoffset);

	regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_CTRL + regoffset);
	regval |= FBRX_DC_ENA;
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_CTRL + regoffset);

	return;
}

static int ispdma_descriptor_chain(struct mvisp_device *isp,
		int cnt, u32 dpaddr, u32 dsize)
{
	if (desc_num >= ispdma_desc_chain_size)
		return 0;

	ispdma_desc_chain[cnt][desc_num] = dpaddr;
	desc_num++;
	ispdma_desc_chain[cnt][desc_num] = dsize;
	desc_num++;

	return 0;
}

static unsigned long uva_to_pa(unsigned long addr, struct page **page)
{
	unsigned long ret = 0UL;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = pgd_offset(current->mm, addr);
	if (!pgd_none(*pgd)) {
		pud = pud_offset(pgd, addr);
		if (!pud_none(*pud)) {
			pmd = pmd_offset(pud, addr);
			if (!pmd_none(*pmd)) {
				pte = pte_offset_map(pmd, addr);
				if (!pte_none(*pte) && pte_present(*pte)) {
					(*page) = pte_page(*pte);
					ret = page_to_phys(*page);
					ret |= (addr & (PAGE_SIZE-1));
				}
			}
		}
	}
	return ret;
}

static void va_to_pa(struct mvisp_device *isp,
		int index, u32 user_addr, u32 size,
		struct v4l2_dxoipc_set_fb *cfg_fb)
{
	u32 paddr, paddr_next;
	u32 vaddr_start = user_addr;
	u32 vaddr_cur = PAGE_ALIGN(user_addr);
	u32 vaddr_end = user_addr + size;
	u32 cur_dcsize;
	struct page *page = NULL;
	int j;
	int ret;

	if (vaddr_cur == 0)
		return;

	cur_dcsize = vaddr_cur - vaddr_start;

	paddr = uva_to_pa(vaddr_start, &page);
	j = 0;

	while (vaddr_cur <= vaddr_end) {
		paddr_next = uva_to_pa(vaddr_cur, &page);

		if ((vaddr_end - vaddr_cur) >= PAGE_SIZE) {
			if ((paddr_next - paddr) != cur_dcsize) {
				ispdma_descriptor_chain(isp,
						index, paddr, cur_dcsize);
				j++;
				paddr = paddr_next;
				cur_dcsize = 0;
			}

			vaddr_cur += PAGE_SIZE;
			cur_dcsize += PAGE_SIZE;

		} else {
			if ((paddr_next - paddr) != cur_dcsize) {
				ispdma_descriptor_chain(isp,
						index, paddr, cur_dcsize);
				j++;
				paddr = paddr_next;
				cur_dcsize = 0;
			}
			if (((vaddr_end - vaddr_cur) == 0)
					&& (cur_dcsize == 0)) {
				ret = ispdma_set_fb_reg_dc(isp,
						index, 2 * j, cfg_fb);
				break;
			} else {
				cur_dcsize += vaddr_end - vaddr_cur;
				ispdma_descriptor_chain(isp,
						index, paddr, cur_dcsize);
				j++;
				ret = ispdma_set_fb_reg_dc(isp,
						index, 2 * j, cfg_fb);
				break;
			}
		}
	}
}

static int ispdma_config_fb_dc(struct isp_ispdma_device *ispdma,
		struct v4l2_dxoipc_set_fb *cfg_fb)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	int cnt;
	u32 regval;

	if (cfg_fb->burst_read != 64
			&& cfg_fb->burst_read != 128
			&& cfg_fb->burst_read != 256)
		return -EINVAL;

	if (cfg_fb->burst_write != 64
			&& cfg_fb->burst_write != 128
			&& cfg_fb->burst_write != 256)
		return -EINVAL;

	if (cfg_fb->fbcnt < 0 || cfg_fb->fbcnt > 4)
		return -EINVAL;

	for (cnt = 0; cnt < cfg_fb->fbcnt; cnt++)
		ispdma_desc_chain[cnt] = kmalloc(sizeof(u32)*4096, GFP_KERNEL);

	ispdma_desc_chain_size = 4096;

	mutex_lock(&ispdma->ispdma_mutex);

	/*  Disable all FB DMA transfer */
	regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
	mvisp_reg_writel(isp, (regval & ~0x3FC),
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

	desc_num = 0;

	for (cnt = 0; cnt < cfg_fb->fbcnt; cnt++) {
		va_to_pa(isp, cnt, (u32)(cfg_fb->virAddr[cnt]),
				cfg_fb->size[cnt], cfg_fb);
		desc_num = 0;
	}

	for (cnt = 0; cnt < cfg_fb->fbcnt; cnt++)
		ispdma_desc_enable(isp, cnt);

	ispdma->framebuf_count = cfg_fb->fbcnt;

	/*  Restart FB DMA transfer if any */
	mvisp_reg_writel(isp, regval ,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

	mutex_unlock(&ispdma->ispdma_mutex);
	return 0;
}

static int ispdma_set_fb_reg(struct mvisp_device *isp,
	int index, struct v4l2_dxoipc_set_fb *cfg_fb)
{
	u32 regoffset;
	u32 regval;

	regoffset = index * 0x100;

	/* Disable Descriptor */
	regval = mvisp_reg_readl(isp,
		ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_CTRL + regoffset);
	regval &= ~FBTX_DC_ENA;
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_CTRL + regoffset);
	regval = mvisp_reg_readl(isp,
		ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_CTRL + regoffset);
	regval &= ~FBRX_DC_ENA;
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_CTRL + regoffset);

	/* TX Settings */
	regval = 0;
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_SDCA + regoffset);
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_DCSZ + regoffset);
	regval = 0x1 << 9;
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_RAMCTRL + regoffset);

	regval = ISPDMA_FBTXN_DSTSZ_MASK & cfg_fb->size[index];
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_DSTSZ + regoffset);

	regval = cfg_fb->phyAddr[index];
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_DSTADDR + regoffset);

	regval = FBTX_DMA_TIMER_VAL;
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_TMR + regoffset);

	regval = 0x400 | ((cfg_fb->burst_write >> 3) & FBTX_DMABRSTSZ);
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBTX0_CTRL + regoffset);

	/* RX Settings */
	regval = 0;
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_SDCA + regoffset);
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_DCSZ + regoffset);
	regval = 0x1 << 9;
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_RAMCTRL + regoffset);

	regval = FBRX_DMA_TIMER_VAL;
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_TMR + regoffset);

	regval = (cfg_fb->burst_read >> 3) & FBRX_DMABRSTSZ;
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_FBRX0_CTRL + regoffset);

	return 0;
}


static int ispdma_config_fb(struct isp_ispdma_device *ispdma,
			struct v4l2_dxoipc_set_fb *cfg_fb)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	int cnt;
	u32 regval;

	if (cfg_fb->burst_read != 64
		&& cfg_fb->burst_read != 128
		&& cfg_fb->burst_read != 256)
		return -EINVAL;

	if (cfg_fb->burst_write != 64
		&& cfg_fb->burst_write != 128
		&& cfg_fb->burst_write != 256)
		return -EINVAL;

	if (cfg_fb->fbcnt < 0 || cfg_fb->fbcnt > 4)
		return -EINVAL;

	for (cnt = 0; cnt < cfg_fb->fbcnt; cnt++) {
		if (cfg_fb->phyAddr[cnt] == 0 || cfg_fb->size[cnt] <= 0)
			return -EINVAL;
	}

	mutex_lock(&ispdma->ispdma_mutex);

	/* Disable all FB DMA transfer */
	regval = mvisp_reg_readl(isp,
		ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
	mvisp_reg_writel(isp, (regval & ~0x3FC),
		ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

	for (cnt = 0; cnt < cfg_fb->fbcnt; cnt++)
		ispdma_set_fb_reg(isp, cnt, cfg_fb);

	ispdma->framebuf_count = cfg_fb->fbcnt;

	/* Restart FB DMA transfer if any */
	mvisp_reg_writel(isp, regval,
		ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

	mutex_unlock(&ispdma->ispdma_mutex);
	return 0;
}

static void ispdma_set_inaddr(struct isp_ispdma_device *ispdma,
		struct isp_video_buffer *buffer)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	unsigned int bitsperpixel, width, height, size;

	if ((buffer == NULL) || (buffer->paddr[ISP_BUF_PADDR] == 0))
		return;

	width = ispdma->formats[ISPDMA_PAD_SINK].width;
	height = ispdma->formats[ISPDMA_PAD_SINK].height;

	switch (ispdma->formats[ISPDMA_PAD_SINK].code) {
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		bitsperpixel = 8;
		break;
	case V4L2_MBUS_FMT_UYVY8_1X16:
		bitsperpixel = 16;
		break;
	case V4L2_MBUS_FMT_Y12_1X12:
		bitsperpixel = 12;
		break;
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		bitsperpixel = 10;
		break;
	default:
		bitsperpixel = 0;
		break;
	}

	size = (bitsperpixel * width * height) >> 3;
	mvisp_reg_writel(isp, buffer->paddr[ISP_BUF_PADDR],
		ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_SRCADDR);
	mvisp_reg_writel(isp, size,
		ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_SRCSZ);

	ispdma->regcache[ISPDMA_PAD_SINK][ISPDMA_DST_REG] = buffer;
	if (ispdma->regcache[ISPDMA_PAD_SINK][ISPDMA_SHADOW_REG]
		!= buffer)
		buffer->state = ISP_BUF_STATE_QUEUED;
	else
		buffer->state = ISP_BUF_STATE_ACTIVE;

	return;
}

static void ispdma_set_disp_outaddr(struct isp_ispdma_device *ispdma,
		struct isp_video_buffer *buffer, dma_addr_t	paddr)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	unsigned int bitsperpixel, width, height, size;
	dma_addr_t	internal_paddr;

	if (buffer != NULL)
		internal_paddr = buffer->paddr[ISP_BUF_PADDR];
	else if (paddr != 0)
		internal_paddr = paddr;
	else
		return;

	width = ispdma->formats[ISPDMA_PAD_DISP_SRC].width;
	height = ispdma->formats[ISPDMA_PAD_DISP_SRC].height;

	switch (ispdma->formats[ISPDMA_PAD_DISP_SRC].code) {
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		bitsperpixel = 8;
		break;
	case V4L2_MBUS_FMT_UYVY8_1X16:
		bitsperpixel = 16;
		break;
	case V4L2_MBUS_FMT_Y12_1X12:
		/*Y component bitsperpixel*/
		bitsperpixel = 8;
		break;
	default:
		bitsperpixel = 0;
		break;
	}

	size = (bitsperpixel * width * height) >> 3;
	mvisp_reg_writel(isp, size,
		ISP_IOMEM_ISPDMA, ISPDMA_DISP_DSTSZ);
	mvisp_reg_writel(isp, internal_paddr,
		ISP_IOMEM_ISPDMA, ISPDMA_DISP_DSTADDR);

	if (isp->cpu_type == MV_PXA988) {
		switch (ispdma->formats[ISPDMA_PAD_DISP_SRC].code) {
		case V4L2_MBUS_FMT_Y12_1X12:
			if (!buffer)
				return;

			internal_paddr = buffer->paddr[ISP_BUF_PADDR_U];
			mvisp_reg_writel(isp, size/4,
				ISP_IOMEM_ISPDMA, ISPDMA_DISP_DSTSZ_U);
			mvisp_reg_writel(isp, internal_paddr,
				ISP_IOMEM_ISPDMA, ISPDMA_DISP_DSTADDR_U);

			internal_paddr = buffer->paddr[ISP_BUF_PADDR_V];
			mvisp_reg_writel(isp, size/4,
				ISP_IOMEM_ISPDMA, ISPDMA_DISP_DSTSZ_V);
			mvisp_reg_writel(isp, internal_paddr,
				ISP_IOMEM_ISPDMA, ISPDMA_DISP_DSTADDR_V);
			break;
		default:
			break;
		}
	}
	if (buffer != NULL) {
		ispdma->regcache[ISPDMA_PAD_DISP_SRC][ISPDMA_DST_REG]
			= buffer;
		if (ispdma->regcache[ISPDMA_PAD_DISP_SRC][ISPDMA_SHADOW_REG]
			!= buffer)
			buffer->state = ISP_BUF_STATE_QUEUED;
		else
			buffer->state = ISP_BUF_STATE_ACTIVE;
	} else
		ispdma->regcache[ISPDMA_PAD_DISP_SRC][ISPDMA_DST_REG]
			= NULL;


	return;
}

static void ispdma_set_codec_outaddr(struct isp_ispdma_device *ispdma,
		struct isp_video_buffer *buffer, dma_addr_t	paddr)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	unsigned int bitsperpixel, width, height, size;
	dma_addr_t	internal_paddr;

	if (buffer != NULL)
		internal_paddr = buffer->paddr[ISP_BUF_PADDR];
	else if (paddr != 0)
		internal_paddr = paddr;
	else
		return;

	width = ispdma->formats[ISPDMA_PAD_CODE_SRC].width;
	height = ispdma->formats[ISPDMA_PAD_CODE_SRC].height;

	switch (ispdma->formats[ISPDMA_PAD_CODE_SRC].code) {
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		bitsperpixel = 8;
		break;
	case V4L2_MBUS_FMT_UYVY8_1X16:
		bitsperpixel = 16;
		break;
	case V4L2_MBUS_FMT_Y12_1X12:
		/*Y component bitsperpixel*/
		bitsperpixel = 8;
		break;
	default:
		bitsperpixel = 0;
		break;
	}

	size = (bitsperpixel * width * height) >> 3;
	mvisp_reg_writel(isp, size,
		ISP_IOMEM_ISPDMA, ISPDMA_CODEC_DSTSZ);
	mvisp_reg_writel(isp, internal_paddr,
		ISP_IOMEM_ISPDMA, ISPDMA_CODEC_DSTADDR);

	if (isp->cpu_type == MV_PXA988) {
		switch (ispdma->formats[ISPDMA_PAD_CODE_SRC].code) {
		case V4L2_MBUS_FMT_Y12_1X12:
			if (!buffer)
				return;

			internal_paddr = buffer->paddr[ISP_BUF_PADDR_U];
			mvisp_reg_writel(isp, size/4,
				ISP_IOMEM_ISPDMA, ISPDMA_CODEC_DSTSZ_U);
			mvisp_reg_writel(isp, internal_paddr,
				ISP_IOMEM_ISPDMA, ISPDMA_CODEC_DSTADDR_U);

			internal_paddr = buffer->paddr[ISP_BUF_PADDR_V];
			mvisp_reg_writel(isp, size/4,
				ISP_IOMEM_ISPDMA, ISPDMA_CODEC_DSTSZ_V);
			mvisp_reg_writel(isp, internal_paddr,
				ISP_IOMEM_ISPDMA, ISPDMA_CODEC_DSTADDR_V);
			break;
		default:
				break;
		}
	}
	if (buffer != NULL) {
		ispdma->regcache[ISPDMA_PAD_CODE_SRC][ISPDMA_DST_REG]
			= buffer;
		if (ispdma->regcache[ISPDMA_PAD_CODE_SRC][ISPDMA_SHADOW_REG]
			!= buffer)
			buffer->state = ISP_BUF_STATE_QUEUED;
		else
			buffer->state = ISP_BUF_STATE_ACTIVE;
	} else
		ispdma->regcache[ISPDMA_PAD_CODE_SRC][ISPDMA_DST_REG]
			= NULL;

	return;
}

static void ispdma_reset_counter(struct isp_ispdma_device *ispdma)
{
	ispdma->ipc_event_cnt = 0;
	ispdma->dma_event_cnt = 0;
	ispdma->disp_mipi_ovr_cnt = 0;
	ispdma->codec_mipi_ovr_cnt = 0;
	ispdma->disp_eof_cnt = 0;
	ispdma->input_event_cnt = 0;
	return;
}

static void ispdma_init_params(struct isp_ispdma_device *ispdma)
{
	int i, j;

	ispdma_reset_counter(ispdma);
	ispdma->dma_working_flag = DMA_NOT_WORKING;
	ispdma->framebuf_count = 0;
	ispdma->sched_stop_disp = false;
	ispdma->sched_stop_codec = false;
	ispdma->sched_stop_input = false;
	for (i = 0; i < ISPDMA_PADS_NUM; i++)
		for (j = 0; j < ISPDMA_CACHE_MAX; j++)
			ispdma->regcache[i][j] = NULL;
	return;
}


static void ispdma_start_dma(struct isp_ispdma_device *ispdma
	, enum ispdma_port port)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	u32 regval;
	unsigned long dma_flags, video_flags;
	struct isp_video_buffer *buf, **shw;

	spin_lock_irqsave(&ispdma->dmaflg_lock, dma_flags);

	switch (port) {
	case ISPDMA_PORT_DISPLAY:
	{
		if (ispdma->dma_working_flag & DMA_DISP_WORKING)
			break;

		spin_lock_irqsave(&ispdma->vd_disp_out.irq_lock, video_flags);

		/* Enable Output DMA here. */
		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_DISP_CTRL);
		regval &= ~0x2;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_DISP_CTRL);

		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);
		regval |= 0x300;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);

		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
		regval |= 0x20801;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);

		regval = (0x1 << 9);
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_DISP_RAMCTRL);

		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
		regval |= 0x1;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

		ispdma->dma_working_flag |= DMA_DISP_WORKING;
		set_vd_dmaqueue_flg(
			&ispdma->vd_disp_out, ISP_VIDEO_DMAQUEUE_BUSY);

		buf = ispdma->regcache[ISPDMA_PAD_DISP_SRC][ISPDMA_DST_REG];
		shw = &ispdma->regcache[ISPDMA_PAD_DISP_SRC][ISPDMA_SHADOW_REG];
		if (buf != NULL) {
			*shw = buf;
			buf->state = ISP_BUF_STATE_ACTIVE;
		}

		spin_unlock_irqrestore(
			&ispdma->vd_disp_out.irq_lock, video_flags);
		break;
	}
	case ISPDMA_PORT_CODEC:
	{
		if (ispdma->dma_working_flag & DMA_CODEC_WORKING)
			break;

		spin_lock_irqsave(&ispdma->vd_codec_out.irq_lock, video_flags);
		/* Enable Output DMA here. */
		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_CODEC_CTRL);
		regval &= ~0x2;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_CODEC_CTRL);

		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);
		regval |= 0xC00;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);

		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
		regval |= 0x21002;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);

		regval = (0x1 << 9);
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_CODEC_RAMCTRL);

		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
		regval |= 0x2;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

		ispdma->dma_working_flag |= DMA_CODEC_WORKING;
		set_vd_dmaqueue_flg(
			&ispdma->vd_codec_out, ISP_VIDEO_DMAQUEUE_BUSY);

		buf = ispdma->regcache[ISPDMA_PAD_CODE_SRC][ISPDMA_DST_REG];
		shw = &ispdma->regcache[ISPDMA_PAD_CODE_SRC][ISPDMA_SHADOW_REG];
		if (buf != NULL) {
			*shw = buf;
			buf->state = ISP_BUF_STATE_ACTIVE;
		}

		spin_unlock_irqrestore(
			&ispdma->vd_codec_out.irq_lock, video_flags);
		break;
	}
	case ISPDMA_PORT_INPUT:
	{
		if (ispdma->dma_working_flag & DMA_INPUT_WORKING)
			break;

		spin_lock_irqsave(&ispdma->vd_in.irq_lock, video_flags);
		if (ispdma->input == ISPDMA_INPUT_MEMORY) {
			regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);
			regval |= 0x10;
			mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);

			/* Enable Input DMA here. */
			regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);
			regval |= 0x3000;
			mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);

			regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
			regval |= 0x20400;
			mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);

			/* Select input as input DMA*/
			regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_MAINCTRL);
			regval |= 0x4;
			mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_MAINCTRL);

			regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);
			regval |= 0x1;
			mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);

			ispdma->dma_working_flag |= DMA_INPUT_WORKING;
			set_vd_dmaqueue_flg(
				&ispdma->vd_in, ISP_VIDEO_DMAQUEUE_BUSY);
		}

		buf = ispdma->regcache[ISPDMA_PAD_SINK][ISPDMA_DST_REG];
		shw = &ispdma->regcache[ISPDMA_PAD_SINK][ISPDMA_SHADOW_REG];
		if (buf != NULL) {
			*shw = buf;
			buf->state = ISP_BUF_STATE_ACTIVE;
		}

		spin_unlock_irqrestore(&ispdma->vd_in.irq_lock, video_flags);
		break;
	}
	default:
		break;
	}

	spin_unlock_irqrestore(&ispdma->dmaflg_lock, dma_flags);

	return;
}


static void ispdma_stop_dma(struct isp_ispdma_device *ispdma
		, enum ispdma_port port)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	u32 regval;
	unsigned long dma_flags;

	spin_lock_irqsave(&ispdma->dmaflg_lock, dma_flags);

	switch (port) {
	case ISPDMA_PORT_DISPLAY:
	{
		if ((ispdma->dma_working_flag & DMA_DISP_WORKING) == 0)
			break;

		/* Disable Output DMA here. */
		regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
		regval &= ~0x1;
		mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

		regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);
		regval &= ~0x300;
		regval |= 0x200;
		mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);

		regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
		regval &= ~0x801;
		mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);

		ispdma->dma_working_flag &= ~DMA_DISP_WORKING;
		ispdma->sched_stop_disp = false;

		dev_warn(isp->dev, "ispdma stop disp\n");
		break;
	}
	case ISPDMA_PORT_CODEC:
	{
		if ((ispdma->dma_working_flag & DMA_CODEC_WORKING) == 0)
			break;

		/* Disable Output DMA here. */
		regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
		regval &= ~0x2;
		mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

		regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);
		regval &= ~0xC00;
		regval |= 0x800;
		mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);

		regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
		regval &= ~0x1002;
		mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);

		ispdma->dma_working_flag &= ~DMA_CODEC_WORKING;
		ispdma->sched_stop_codec = false;

		dev_warn(isp->dev, "ispdma stop codec\n");
		break;
	}
	case ISPDMA_PORT_INPUT:
	{
		if ((ispdma->dma_working_flag & DMA_INPUT_WORKING) == 0)
			break;

		if (ispdma->input == ISPDMA_INPUT_MEMORY) {
			/* Disable Input DMA here. */
			regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);
			regval &= ~0x1;
			mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);

			regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);
			regval &= ~0x3000;
			regval |= 0x2000;
			mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_CLKENA);

			regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
			regval &= ~0x400;
			mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
		}

		ispdma->dma_working_flag &= ~DMA_INPUT_WORKING;
		ispdma->sched_stop_input = false;

		dev_warn(isp->dev, "ispdma stop input\n");
		break;
	}
	default:
		break;
	}

	if (ispdma->dma_working_flag == DMA_NOT_WORKING) {
		regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
		regval &= ~0x20000;
		mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
	}

	spin_unlock_irqrestore(&ispdma->dmaflg_lock, dma_flags);

	return;
}

static void ispdma_config_csi_input(struct isp_ispdma_device *ispdma)
{
	u32 regval;
	struct mvisp_device *isp = to_mvisp_device(ispdma);

	regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_MAINCTRL);
	regval |= 1 << 3;  /* Set input as CSI2 */
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_MAINCTRL);
	return;
}

static int load_dummy_buffer(struct isp_ispdma_device *ispdma,
			enum ispdma_port port)
{
	enum isp_pipeline_start_condition
			start_dma = ISP_CAN_NOT_START;
	struct mvisp_device *isp = to_mvisp_device(ispdma);

	switch (port) {
	case ISPDMA_PORT_DISPLAY:
		if (isp->dummy_paddr == 0) {
			ispdma->sched_stop_disp = true;
			dev_warn(isp->dev,
				"isp display dma schduled stop [no dummy buffer]\n");
		} else {
			ispdma_set_disp_outaddr(ispdma, NULL, isp->dummy_paddr);
			start_dma |= ISP_DISPLAY_CAN_START;
		}
		break;
	case ISPDMA_PORT_CODEC:
		if (isp->dummy_paddr == 0) {
			ispdma->sched_stop_codec = true;
			dev_warn(isp->dev,
				"isp codec dma schduled stop [no dummy buffer]\n");
		} else {
			ispdma_set_codec_outaddr(ispdma,
				NULL, isp->dummy_paddr);
			start_dma |= ISP_CODEC_CAN_START;
		}
		break;
	case ISPDMA_PORT_INPUT:
		ispdma->sched_stop_input = true;
		start_dma |= ISP_INPUT_CAN_START;
		dev_warn(isp->dev,
			"isp input dma schduled stop [no dummy buffer]\n");
		break;
	default:
		break;
	}

	return start_dma;
}

static int ispdma_isr_load_buffer(struct isp_ispdma_device *ispdma
			, enum ispdma_port port)
{
	enum isp_pipeline_start_condition
			start_dma = ISP_CAN_NOT_START;
	struct isp_video_buffer *buffer;
	bool needdummy = false;

	switch (port) {
	case ISPDMA_PORT_DISPLAY:
		buffer = mvisp_video_buffer_next(
			&ispdma->vd_disp_out, ispdma->disp_mipi_ovr_cnt);
		if (buffer != NULL) {
			ispdma_set_disp_outaddr(ispdma, buffer, 0);
			start_dma |= ISP_DISPLAY_CAN_START;
		} else {
			needdummy = true;
		}
		break;
	case ISPDMA_PORT_CODEC:
		buffer = mvisp_video_buffer_next(
			&ispdma->vd_codec_out, ispdma->codec_mipi_ovr_cnt);
		if (buffer != NULL) {
			ispdma_set_codec_outaddr(ispdma, buffer, 0);
			start_dma |= ISP_CODEC_CAN_START;
		} else {
			needdummy = true;
		}
		break;
	case ISPDMA_PORT_INPUT:
		buffer = mvisp_video_buffer_next(&ispdma->vd_in, 0);
		if (buffer != NULL) {
			ispdma_set_inaddr(ispdma, buffer);
			start_dma |= ISP_INPUT_CAN_START;
		} else {
			needdummy = true;
		}
		break;
	default:
		break;
	}

	if (needdummy == true)
		start_dma |= load_dummy_buffer(ispdma, port);

	return start_dma;
}

static void ispdma_isr_buffer(struct isp_ispdma_device *ispdma
			, enum ispdma_port port)
{
	enum isp_pipeline_start_condition condition;
	unsigned long dma_working_flag;

	condition = ispdma_isr_load_buffer(ispdma, port);
	switch (ispdma->state) {
	case ISP_PIPELINE_STREAM_CONTINUOUS:
		dma_working_flag = get_dma_working_flag(ispdma);

		if ((dma_working_flag & DMA_INPUT_WORKING) &&
			(condition & ISP_INPUT_CAN_START) == 0 &&
			port == ISPDMA_PORT_INPUT) {
			ispdma_stop_dma(ispdma, ISPDMA_PORT_INPUT);
		}
		if ((dma_working_flag & DMA_DISP_WORKING) &&
			(condition & ISP_DISPLAY_CAN_START) == 0 &&
			port == ISPDMA_PORT_DISPLAY)
			ispdma_stop_dma(ispdma, ISPDMA_PORT_DISPLAY);

		if ((dma_working_flag & DMA_CODEC_WORKING) &&
			(condition & ISP_CODEC_CAN_START) == 0 &&
			port == ISPDMA_PORT_CODEC)
			ispdma_stop_dma(ispdma, ISPDMA_PORT_CODEC);

		break;
	case ISP_PIPELINE_STREAM_STOPPED:
		dma_working_flag = get_dma_working_flag(ispdma);

		if (dma_working_flag & DMA_INPUT_WORKING)
			ispdma_stop_dma(ispdma, ISPDMA_PORT_INPUT);
		if (dma_working_flag & DMA_DISP_WORKING)
			ispdma_stop_dma(ispdma, ISPDMA_PORT_DISPLAY);
		if (dma_working_flag & DMA_CODEC_WORKING)
			ispdma_stop_dma(ispdma, ISPDMA_PORT_CODEC);
		break;
	default:
		break;
	}

	return;
}

static void ispdma_disp_handler(struct isp_ispdma_device *ispdma,
	struct isp_video *video)
{
	unsigned long video_flags;
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	unsigned long dma_working_flag;
	struct isp_video_buffer *buf = NULL;
	u32 regval;
	bool dummy_in_transfer;

	dma_working_flag = get_dma_working_flag(ispdma);
	if ((dma_working_flag & DMA_DISP_WORKING) == 0)
		return;

	buf = ispdma->regcache[ISPDMA_PAD_DISP_SRC][ISPDMA_SHADOW_REG];
	if (buf != NULL)
		buf->state = ISP_BUF_STATE_ACTIVE;
	ispdma->regcache[ISPDMA_PAD_DISP_SRC][ISPDMA_SHADOW_REG] =
		ispdma->regcache[ISPDMA_PAD_DISP_SRC][ISPDMA_DST_REG];

	ispdma->disp_eof_cnt++;

	if (ispdma->sched_stop_disp == true) {
		dev_warn(isp->dev, "display dma schedule stops\n");
		ispdma_stop_dma(ispdma, ISPDMA_PORT_DISPLAY);
	}

	if ((isp->ispdma_dummy_ena == true) && (isp->dummy_paddr != 0)) {
		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_DISP_DSTADDR);
		if (regval != isp->dummy_paddr)
			dummy_in_transfer = false;
		else
			dummy_in_transfer = true;
	} else
		dummy_in_transfer = false;

	if (dummy_in_transfer == false) {
		ispdma_isr_buffer(ispdma, ISPDMA_PORT_DISPLAY);
		ispdma->disp_mipi_ovr_cnt = 0;
	} else {
		ispdma->disp_mipi_ovr_cnt = 0;
		spin_lock_irqsave(&video->irq_lock, video_flags);
		if (!list_empty(&video->dmaidlequeue)) {
			buf = list_first_entry(&video->dmaidlequeue,
				struct isp_video_buffer, dmalist);
			list_del(&buf->dmalist);
			video->dmaidlecnt--;
			list_add_tail(&buf->dmalist, &video->dmabusyqueue);
			video->dmabusycnt++;
			ispdma_set_disp_outaddr(ispdma, buf, 0);
		}
		spin_unlock_irqrestore(&video->irq_lock, video_flags);
	}

	/* Enable MIPI overrun IRQ for error handling */
	regval = mvisp_reg_readl(isp, ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
	regval |= 0x20000;
	mvisp_reg_writel(isp, regval, ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);

	return;
}

static void ispdma_codec_handler(struct isp_ispdma_device *ispdma,
	struct isp_video *video)
{
	unsigned long video_flags;
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	unsigned long dma_working_flag;
	struct isp_video_buffer *buf = NULL;
	u32 regval;
	bool dummy_in_transfer;

	dma_working_flag = get_dma_working_flag(ispdma);
	if ((dma_working_flag & DMA_CODEC_WORKING) == 0)
		return;

	buf = ispdma->regcache[ISPDMA_PAD_CODE_SRC][ISPDMA_SHADOW_REG];
	if (buf != NULL)
		buf->state = ISP_BUF_STATE_ACTIVE;
	ispdma->regcache[ISPDMA_PAD_CODE_SRC][ISPDMA_SHADOW_REG] =
		ispdma->regcache[ISPDMA_PAD_CODE_SRC][ISPDMA_DST_REG];

	if (ispdma->codec_band_max != 0) {
		ispdma->codec_band_cnt++;
		if (ispdma->codec_band_cnt < ispdma->codec_band_max)
			return;
		else
			ispdma->codec_band_cnt = 0;
	}

	if (ispdma->sched_stop_codec == true) {
		dev_warn(isp->dev, "codec dma schedule stops\n");
		ispdma_stop_dma(ispdma, ISPDMA_PORT_CODEC);
	}

	if ((isp->ispdma_dummy_ena == true) && (isp->dummy_paddr != 0)) {
		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_CODEC_DSTADDR);
		if (regval != isp->dummy_paddr)
			dummy_in_transfer = false;
		else
			dummy_in_transfer = true;
	} else
		dummy_in_transfer = false;

	if (dummy_in_transfer == false) {
		ispdma_isr_buffer(ispdma, ISPDMA_PORT_CODEC);
		ispdma->codec_mipi_ovr_cnt = 0;
	} else {
		ispdma->codec_mipi_ovr_cnt = 0;
		spin_lock_irqsave(&video->irq_lock, video_flags);
		if (!list_empty(&video->dmaidlequeue)) {
			buf = list_first_entry(&video->dmaidlequeue
				, struct isp_video_buffer, dmalist);
			list_del(&buf->dmalist);
			video->dmaidlecnt--;
			list_add_tail(&buf->dmalist, &video->dmabusyqueue);
			video->dmabusycnt++;
			ispdma_set_codec_outaddr(ispdma, buf, 0);
		}
		spin_unlock_irqrestore(&video->irq_lock, video_flags);
	}

	/* Enable MIPI overrun IRQ for error handling */
	regval = mvisp_reg_readl(isp, ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
	regval |= 0x20000;
	mvisp_reg_writel(isp, regval, ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);

	return;
}

static void ispdma_input_handler(struct isp_ispdma_device *ispdma,
	struct isp_video *video)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	unsigned long dma_working_flag;
	struct isp_video_buffer *buf = NULL;

	dma_working_flag = get_dma_working_flag(ispdma);
	if ((dma_working_flag & DMA_INPUT_WORKING) == 0)
		return;

	buf = ispdma->regcache[ISPDMA_PAD_SINK][ISPDMA_SHADOW_REG];
	if (buf != NULL)
		buf->state = ISP_BUF_STATE_ACTIVE;
	ispdma->regcache[ISPDMA_PAD_SINK][ISPDMA_SHADOW_REG] =
		ispdma->regcache[ISPDMA_PAD_SINK][ISPDMA_DST_REG];

	if (ispdma->sched_stop_input == true) {
		dev_warn(isp->dev, "input dma schedule stops\n");
		ispdma_stop_dma(ispdma, ISPDMA_PORT_INPUT);
	}

	ispdma_isr_buffer(ispdma, ISPDMA_PORT_INPUT);

	return;
}


void mv_ispdma_dma_isr_handler(struct isp_ispdma_device *ispdma
		, unsigned long irq_status)
{
	unsigned long dma_irq_flags;
	u32 regval;
	struct mvisp_device *isp = to_mvisp_device(ispdma);

	spin_lock_irqsave(&ispdma->dma_irq_lock, dma_irq_flags);

	ispdma->dma_event_cnt++;
	if (irq_status & CSI2_BRIDEG) {
		if (ispdma->disp_mipi_ovr_cnt < 0xFFFF)
			ispdma->disp_mipi_ovr_cnt++;
		if (ispdma->codec_mipi_ovr_cnt < 0xFFFF)
			ispdma->codec_mipi_ovr_cnt++;

		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
		regval &= ~0x20000;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
	}

	if (irq_status & DISP_PS_EOF)
		ispdma_getdelta(&ispdma->dma_timeinfo.disp_ps_timeinfo, false);

	if (irq_status & DISP_DMA_EOF) {
		ispdma_getdelta(&ispdma->dma_timeinfo.disp_dma_timeinfo, false);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(isp->mcd_root_display.mcd, MCD_DMA, MCD_DMA_EOF, 1);
#endif
		ispdma_disp_handler(ispdma, &ispdma->vd_disp_out);
	}

	if (irq_status & CODEC_PS_EOF)
		ispdma_getdelta(&ispdma->dma_timeinfo.codec_ps_timeinfo, false);

	if (irq_status & CODEC_DMA_EOF) {
		ispdma_getdelta(&ispdma->dma_timeinfo.codec_dma_timeinfo,
				false);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(isp->mcd_root_codec.mcd, MCD_DMA, MCD_DMA_EOF, 1);
#endif
		ispdma_codec_handler(ispdma, &ispdma->vd_codec_out);
	}

	if (irq_status & INPUT_DMA_EOF)
		ispdma_input_handler(ispdma, &ispdma->vd_in);

	spin_unlock_irqrestore(&ispdma->dma_irq_lock, dma_irq_flags);
	return;
}

static int ispdma_reload_disp_buffer(struct isp_ispdma_device *ispdma)
{
	struct isp_video_buffer *buffer = NULL;
	unsigned long video_flags;

	buffer = mvisp_video_get_next_work_buf(&ispdma->vd_disp_out);
	if (buffer == NULL)
		return -EINVAL;

	if (isp_buf_type_is_capture_mp(buffer->vb2_buf.v4l2_buf.type)) {
		spin_lock_irqsave(&ispdma->vd_disp_out.irq_lock, video_flags);
		ispdma_set_disp_outaddr(ispdma, buffer, 0);
		set_vd_dmaqueue_flg(&ispdma->vd_disp_out,
			ISP_VIDEO_DMAQUEUE_QUEUED);
		spin_unlock_irqrestore(
			&ispdma->vd_disp_out.irq_lock, video_flags);
	}

	return 0;
}

static int ispdma_reload_codec_buffer(struct isp_ispdma_device *ispdma)
{
	struct isp_video_buffer *buffer = NULL;
	unsigned long video_flags;

	buffer = mvisp_video_get_next_work_buf(&ispdma->vd_codec_out);
	if (buffer == NULL)
		return -EINVAL;

	if (isp_buf_type_is_capture_mp(buffer->vb2_buf.v4l2_buf.type)) {
		spin_lock_irqsave(&ispdma->vd_codec_out.irq_lock, video_flags);
		ispdma_set_codec_outaddr(ispdma, buffer, 0);
		set_vd_dmaqueue_flg(&ispdma->vd_codec_out,
			ISP_VIDEO_DMAQUEUE_QUEUED);
		spin_unlock_irqrestore(
			&ispdma->vd_codec_out.irq_lock, video_flags);
	}

	return 0;
}

static int ispdma_reload_input_buffer(struct isp_ispdma_device *ispdma)
{
	struct isp_video_buffer *buffer = NULL;
	unsigned long video_flags;

	buffer = mvisp_video_get_next_work_buf(&ispdma->vd_in);
	if (buffer == NULL)
		return -EINVAL;

	if (isp_buf_type_is_output_mp(buffer->vb2_buf.v4l2_buf.type)) {
		spin_lock_irqsave(&ispdma->vd_in.irq_lock, video_flags);
		ispdma_set_inaddr(ispdma, buffer);
		set_vd_dmaqueue_flg(&ispdma->vd_in,
			ISP_VIDEO_DMAQUEUE_QUEUED);
		spin_unlock_irqrestore(&ispdma->vd_in.irq_lock, video_flags);
	}

	return 0;
}

static int ispdma_try_restart_disp_dma(struct isp_ispdma_device *ispdma)
{
	enum isp_video_dmaqueue_flags dmaqueue_flags;
	unsigned long dma_flag;
	int ret;

	dmaqueue_flags = get_vd_dmaqueue_flg(&ispdma->vd_disp_out);
	switch (dmaqueue_flags) {
	case ISP_VIDEO_DMAQUEUE_UNDERRUN:
		ret = ispdma_reload_disp_buffer(ispdma);
		if (ret != 0)
			return ret;
	case ISP_VIDEO_DMAQUEUE_QUEUED:
		dma_flag = get_dma_working_flag(ispdma);
		if ((dma_flag & DMA_DISP_WORKING) == 0) {
			ispdma_start_dma(ispdma, ISPDMA_PORT_DISPLAY);
			ispdma_reload_disp_buffer(ispdma);
		}
		break;
	case ISP_VIDEO_DMAQUEUE_BUSY:
	default:
		break;
	}

	return 0;
}

static int ispdma_try_restart_codec_dma(struct isp_ispdma_device *ispdma)
{
	enum isp_video_dmaqueue_flags dmaqueue_flags;
	unsigned long dma_flag;
	int ret;

	dmaqueue_flags =
		get_vd_dmaqueue_flg(&ispdma->vd_codec_out);
	switch (dmaqueue_flags) {
	case ISP_VIDEO_DMAQUEUE_UNDERRUN:
		ret = ispdma_reload_codec_buffer(ispdma);
		if (ret != 0)
			return ret;
	case ISP_VIDEO_DMAQUEUE_QUEUED:
		dma_flag = get_dma_working_flag(ispdma);
		if ((dma_flag & DMA_CODEC_WORKING) == 0) {
			ispdma_start_dma(ispdma, ISPDMA_PORT_CODEC);
			ispdma_reload_codec_buffer(ispdma);
		}
		break;
	case ISP_VIDEO_DMAQUEUE_BUSY:
	default:
		break;
	}

	return 0;
}

static int ispdma_try_restart_input_dma(struct isp_ispdma_device *ispdma)
{
	enum isp_video_dmaqueue_flags dmaqueue_flags;
	unsigned long dma_flag;
	int ret;

	dmaqueue_flags =
		get_vd_dmaqueue_flg(&ispdma->vd_in);
	switch (dmaqueue_flags) {
	case ISP_VIDEO_DMAQUEUE_UNDERRUN:
		ret = ispdma_reload_input_buffer(ispdma);
		if (ret != 0)
			return ret;
	case ISP_VIDEO_DMAQUEUE_QUEUED:
		dma_flag = get_dma_working_flag(ispdma);
		if ((dma_flag & DMA_INPUT_WORKING) == 0) {
			ispdma_start_dma(ispdma, ISPDMA_PORT_INPUT);
			ispdma_reload_input_buffer(ispdma);
		}
		break;
	case ISP_VIDEO_DMAQUEUE_BUSY:
	default:
		break;
	}

	return 0;
}

static int ispdma_try_restart_dma(struct isp_ispdma_device *ispdma)
{
	struct isp_pipeline *pipe = to_isp_pipeline(&ispdma->subdev.entity);
	enum isp_pipeline_start_condition condition;
	int ret = 0;

	if (!pipe) {
		printk(KERN_ERR "ispdma pipeline is NULL\n");
		return -EINVAL;
	}

	condition = isp_pipeline_ready(pipe);
	if (ispdma->state == ISP_PIPELINE_STREAM_CONTINUOUS) {
		if (condition & ISP_DISPLAY_CAN_START)
			ret = ispdma_try_restart_disp_dma(ispdma);
		if (condition & ISP_CODEC_CAN_START)
			ret = ispdma_try_restart_codec_dma(ispdma);
		if (condition & ISP_INPUT_CAN_START)
			ret = ispdma_try_restart_input_dma(ispdma);
	}

	return ret;
}

static void ctx_adjust_buffers(struct isp_video *video)
{
	struct isp_video_buffer *buf;
	struct isp_pipeline *pipe = to_isp_pipeline(&video->video.entity);
	enum isp_pipeline_state state;
	struct list_head temp_list;
	unsigned long video_flags;

	INIT_LIST_HEAD(&temp_list);

	spin_lock_irqsave(&video->irq_lock, video_flags);

	while (!list_empty(&video->dmaidlequeue)) {
		buf = list_first_entry(&video->dmaidlequeue,
			struct isp_video_buffer, dmalist);
		list_del(&buf->dmalist);
		video->dmaidlecnt--;
		list_add_tail(&buf->dmalist, &temp_list);
	}

	while (!list_empty(&video->dmabusyqueue)) {
		buf = list_first_entry(&video->dmabusyqueue,
			struct isp_video_buffer, dmalist);
		list_del(&buf->dmalist);
		video->dmabusycnt--;
		list_add_tail(&buf->dmalist, &video->dmaidlequeue);
		video->dmaidlecnt++;
	}

	while (!list_empty(&temp_list)) {
		buf = list_first_entry(&temp_list,
			struct isp_video_buffer, dmalist);
		list_del(&buf->dmalist);
		list_add_tail(&buf->dmalist, &video->dmaidlequeue);
		video->dmaidlecnt++;
	}

	if (list_empty(&video->dmaidlequeue) == 0) {
		switch (video->video_type) {
		case ISP_VIDEO_DISPLAY:
			state = ISP_PIPELINE_DISPLAY_QUEUED;
			break;
		case ISP_VIDEO_CODEC:
			state = ISP_PIPELINE_CODEC_QUEUED;
			break;
		case ISP_VIDEO_INPUT:
			state = ISP_PIPELINE_INPUT_QUEUED;
			break;
		default:
			state = 0;
			break;
		}

		pipe->state |= state;
	}
	set_vd_dmaqueue_flg(video, ISP_VIDEO_DMAQUEUE_UNDERRUN);

	spin_unlock_irqrestore(&video->irq_lock, video_flags);

	return;
}

static void ispdma_context_save(struct isp_ispdma_device *ispdma)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	int cnt;

	for (cnt = 0; cnt < ISPDMA_INPSDMA_MAX_CTX; cnt++) {
		ispdma_reg_list[cnt].val =
			mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA,
				ispdma_reg_list[cnt].reg);

		if (cnt == ISPDMA_DMA_ENA_CTX) {
			mvisp_reg_writel(isp, 0,
				ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

			ctx_adjust_buffers(&ispdma->vd_disp_out);
			ctx_adjust_buffers(&ispdma->vd_codec_out);
		} else if (cnt == ISPDMA_INPSDMA_CTRL_CTX) {
			mvisp_reg_writel(isp, 0,
				ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);

			ctx_adjust_buffers(&ispdma->vd_in);
		} else if (cnt == ISPDMA_IRQMASK_CTX) {
			mvisp_reg_writel(isp, 0,
				ISP_IOMEM_ISPDMA, ISPDMA_IRQMASK);
		}
	}

	return;
}

static void ispdma_context_restore(struct isp_ispdma_device *ispdma)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	int cnt;
	unsigned long dma_flags;

	spin_lock_irqsave(&ispdma->dmaflg_lock, dma_flags);
	ispdma->dma_working_flag = 0;
	spin_unlock_irqrestore(&ispdma->dmaflg_lock, dma_flags);

	for (cnt = 0; cnt < ISPDMA_INPSDMA_MAX_CTX; cnt++) {
		if (cnt == ISPDMA_INPSDMA_CTRL_CTX) {
			/*Restore input settings*/
			mvisp_reg_writel(isp,
				(ispdma_reg_list[cnt].val & ~0x1),
				ISP_IOMEM_ISPDMA,
				ispdma_reg_list[cnt].reg);

			/*Restart input DMA, IRQ mask*/
			ispdma_try_restart_dma(ispdma);
		} else if (cnt == ISPDMA_DMA_ENA_CTX) {
			/*Restart all the DMAs except disp/codec*/
			mvisp_reg_writel(isp,
				(ispdma_reg_list[cnt].val & ~0x3),
				ISP_IOMEM_ISPDMA,
				ispdma_reg_list[cnt].reg);

			/*Restart all disp/codec DMAs, IRQ masks*/
			ispdma_try_restart_dma(ispdma);
		} else if (cnt == ISPDMA_IRQMASK_CTX) {
			/*Only restore FB IRQ masks here*/
			mvisp_reg_writel(isp,
				(ispdma_reg_list[cnt].val & ~0x21C03),
				ISP_IOMEM_ISPDMA,
				ispdma_reg_list[cnt].reg);
		} else {
			mvisp_reg_writel(isp,
				ispdma_reg_list[cnt].val,
				ISP_IOMEM_ISPDMA,
				ispdma_reg_list[cnt].reg);
		}
	}

	return;
}

static bool ispdma_can_fast_qbuf(struct isp_video *video, int pad)
{
	struct isp_video_buffer	*regdst, *regshd;
	struct isp_ispdma_device *ispdma = &video->isp->mvisp_ispdma;

	if (pad > ISPDMA_PADS_NUM - 1)
		return false;

	regdst = ispdma->regcache
		[pad][ISPDMA_DST_REG];
	regshd = ispdma->regcache
		[pad][ISPDMA_SHADOW_REG];

	if ((regdst == regshd) && (regdst != NULL))
		return true;

	return false;
}

static int ispdma_video_qbuf_notify(struct isp_video *video)
{
	struct isp_ispdma_device *ispdma = &video->isp->mvisp_ispdma;
	bool need_try_restart;
	unsigned long dma_flags;

	spin_lock_irqsave(&ispdma->dmaflg_lock, dma_flags);
	switch (video->video_type) {
	case ISP_VIDEO_DISPLAY:
		if ((ispdma->dma_working_flag & DMA_DISP_WORKING) == 0)
			need_try_restart = true;
		else {
			need_try_restart = false;
			if (ispdma_can_fast_qbuf(video, ISPDMA_PAD_DISP_SRC))
				ispdma_reload_disp_buffer(ispdma);
		}
		break;
	case ISP_VIDEO_CODEC:
		if ((ispdma->dma_working_flag & DMA_CODEC_WORKING) == 0)
			need_try_restart = true;
		else {
			need_try_restart = false;
			if (ispdma_can_fast_qbuf(video, ISPDMA_PAD_CODE_SRC))
				ispdma_reload_codec_buffer(ispdma);
		}
		break;
	case ISP_VIDEO_INPUT:
		if ((ispdma->dma_working_flag & DMA_INPUT_WORKING) == 0)
			need_try_restart = true;
		else {
			need_try_restart = false;
			if (ispdma_can_fast_qbuf(video, ISPDMA_PAD_SINK))
				ispdma_reload_input_buffer(ispdma);
		}
		break;
	default:
		need_try_restart = false;
		break;
	}
	spin_unlock_irqrestore(&ispdma->dmaflg_lock, dma_flags);

	if (need_try_restart)
		ispdma_try_restart_dma(ispdma);

	return 0;
}

static int ispdma_video_stream_on_notify(struct isp_video *video)
{
	struct isp_ispdma_device *ispdma = &video->isp->mvisp_ispdma;
	int need_try_restart = 0;
	unsigned long dma_flags;

	spin_lock_irqsave(&ispdma->dmaflg_lock, dma_flags);
	switch (video->video_type) {
	case ISP_VIDEO_DISPLAY:
		if ((ispdma->dma_working_flag & DMA_DISP_WORKING) == 0)
			need_try_restart = 1;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		CLEAR(video->isp->mcd_root_display.mcd, MCD_DMA, MCD_DMA_SOF);
		CLEAR(video->isp->mcd_root_display.mcd, MCD_DMA, MCD_DMA_EOF);
		CLEAR(video->isp->mcd_root_display.mcd,
			MCD_DMA, MCD_DMA_OVERFLOW);
		CLEAR(video->isp->mcd_root_display.mcd,
			MCD_DMA, MCD_DMA_DROP_FRAME);
#endif
		break;
	case ISP_VIDEO_CODEC:
		if ((ispdma->dma_working_flag & DMA_CODEC_WORKING) == 0)
			need_try_restart = 1;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		CLEAR(video->isp->mcd_root_codec.mcd, MCD_DMA, MCD_DMA_SOF);
		CLEAR(video->isp->mcd_root_codec.mcd, MCD_DMA, MCD_DMA_EOF);
		CLEAR(video->isp->mcd_root_codec.mcd,
			MCD_DMA, MCD_DMA_OVERFLOW);
		CLEAR(video->isp->mcd_root_codec.mcd,
			MCD_DMA, MCD_DMA_DROP_FRAME);
#endif
		break;
	case ISP_VIDEO_INPUT:
		if ((ispdma->dma_working_flag & DMA_INPUT_WORKING) == 0)
			need_try_restart = 1;
		break;
	default:
		need_try_restart = 0;
		break;
	}
	spin_unlock_irqrestore(&ispdma->dmaflg_lock, dma_flags);

	if (need_try_restart)
		ispdma_try_restart_dma(ispdma);

	return 0;
}

static int ispdma_video_stream_off_notify(struct isp_video *video)
{
	struct isp_ispdma_device *ispdma = &video->isp->mvisp_ispdma;
	unsigned long dma_working_flag;

	dma_working_flag = get_dma_working_flag(ispdma);
	switch (video->video_type) {
	case ISP_VIDEO_DISPLAY:
		if (dma_working_flag & DMA_DISP_WORKING)
			ispdma_stop_dma(ispdma, ISPDMA_PORT_DISPLAY);
		break;
	case ISP_VIDEO_CODEC:
		if (dma_working_flag & DMA_CODEC_WORKING)
			ispdma_stop_dma(ispdma, ISPDMA_PORT_CODEC);
		break;
	case ISP_VIDEO_INPUT:
		if (dma_working_flag & ISP_VIDEO_INPUT)
			ispdma_stop_dma(ispdma, ISPDMA_PORT_INPUT);
		break;
	default:
		break;
	}

	return 0;
}

static const struct isp_video_operations ispdma_video_ops = {
	.qbuf_notify = ispdma_video_qbuf_notify,
	.stream_on_notify = ispdma_video_stream_on_notify,
	.stream_off_notify = ispdma_video_stream_off_notify,
};

static int ispdma_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct isp_ispdma_device *ispdma =
		container_of(ctrl->handler
			, struct isp_ispdma_device, ctrls);

	mutex_lock(&ispdma->ispdma_mutex);

	ispdma = ispdma;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	}

	mutex_unlock(&ispdma->ispdma_mutex);
	return 0;
}

static const struct v4l2_ctrl_ops ispdma_ctrl_ops = {
	.s_ctrl = ispdma_s_ctrl,
};

static int ispdma_config_capture_mode(
	struct isp_ispdma_device *ispdma,
	struct v4l2_ispdma_capture_mode *mode_cfg)
{
	if (mode_cfg->mode > ISPVIDEO_STILL_CAPTURE)
		return -EINVAL;

	switch (mode_cfg->port) {
	case ISPDMA_PORT_CODEC:
		ispdma->vd_codec_out.capture_mode = mode_cfg->mode;
		break;
	case ISPDMA_PORT_DISPLAY:
		ispdma->vd_disp_out.capture_mode = mode_cfg->mode;
		break;
	default:
		break;
	}

	return 0;
}

static int ispdma_config_codec(struct isp_ispdma_device *ispdma,
		struct v4l2_dxoipc_config_codec *cfg_codec)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	u32 regval;
	unsigned long dma_irq_flags;

	if (cfg_codec == NULL)
		return -EINVAL;

	if (cfg_codec->vbsize < 0)
		return -EINVAL;

	mutex_lock(&ispdma->ispdma_mutex);

	regval = mvisp_reg_readl(isp, ISP_IOMEM_ISPDMA, ISPDMA_CODEC_CTRL);
	if ((cfg_codec->vbnum > 1) && (cfg_codec->vbnum < 5)) {
		regval &= ~(0x3 << 6);
		regval |=  (cfg_codec->vbnum - 1) << 0x6;
		regval |= 0x8;
		spin_lock_irqsave(&ispdma->dma_irq_lock, dma_irq_flags);
		ispdma->codec_band_max = cfg_codec->vbnum;
		ispdma->codec_band_cnt = 0;
		spin_unlock_irqrestore(&ispdma->dma_irq_lock, dma_irq_flags);
		mvisp_reg_writel(isp, cfg_codec->vbsize,
			ISP_IOMEM_ISPDMA, ISPDMA_CODEC_VBSZ);
	} else {
		regval &= ~(0xC8);
		spin_lock_irqsave(&ispdma->dma_irq_lock, dma_irq_flags);
		ispdma->codec_band_max = 0;
		ispdma->codec_band_cnt = 0;
		spin_unlock_irqrestore(&ispdma->dma_irq_lock, dma_irq_flags);
	}

	if (cfg_codec->dma_burst_size == 64
		|| cfg_codec->dma_burst_size == 128
		|| cfg_codec->dma_burst_size == 256) {
		regval |= ((cfg_codec->dma_burst_size >> 3) & 0x30);
	} else
		regval &= ~(0x30);
	mvisp_reg_writel(isp, regval, ISP_IOMEM_ISPDMA, ISPDMA_CODEC_CTRL);

	mutex_unlock(&ispdma->ispdma_mutex);
	return 0;
}

static int ispdma_copy_timeinfo(
		struct v4l2_ispdma_timeinfo *dest,
		const struct v4l2_ispdma_timeinfo *src)
{
	if (NULL == dest || NULL == src)
		return -EINVAL;

	dest->timestamp = src->timestamp;
	dest->delta = src->delta;

	return 0;
}

static int ispdma_get_dma_timeinfo(struct isp_ispdma_device *ispdma,
		struct v4l2_ispdma_dma_timeinfo *dma_timeinfo)
{
	unsigned long dma_irq_flags;

	if (NULL == dma_timeinfo || NULL == ispdma)
		return -EINVAL;

	spin_lock_irqsave(&ispdma->dma_irq_lock, dma_irq_flags);

	ispdma_copy_timeinfo(&dma_timeinfo->disp_dma_timeinfo,
			&ispdma->dma_timeinfo.disp_dma_timeinfo);
	ispdma_copy_timeinfo(&dma_timeinfo->disp_ps_timeinfo,
			&ispdma->dma_timeinfo.disp_ps_timeinfo);
	ispdma_copy_timeinfo(&dma_timeinfo->codec_dma_timeinfo,
			&ispdma->dma_timeinfo.codec_dma_timeinfo);
	ispdma_copy_timeinfo(&dma_timeinfo->codec_ps_timeinfo,
			&ispdma->dma_timeinfo.codec_ps_timeinfo);

	spin_unlock_irqrestore(&ispdma->dma_irq_lock, dma_irq_flags);

	return 0;
}

static int ispdma_set_stream(struct v4l2_subdev *sd
			, int enable)
{
	struct isp_ispdma_device *ispdma
				= v4l2_get_subdevdata(sd);
	struct mvisp_device *isp
				= to_mvisp_device(ispdma);
	u32 regval;
	unsigned long dma_working_flag;

	mutex_lock(&ispdma->ispdma_mutex);

	switch (enable) {
	case ISP_PIPELINE_STREAM_CONTINUOUS:
		if (ispdma->stream_refcnt++ == 0) {
			if ((ispdma->input == ISPDMA_INPUT_CCIC_1)
				&& (isp->sensor_connected == true)) {
				regval = mvisp_reg_readl(isp, ISP_IOMEM_ISPDMA,
					ISPDMA_MAINCTRL);
				regval |= 1 << 24;
				mvisp_reg_writel(isp, regval, ISP_IOMEM_ISPDMA,
					ISPDMA_MAINCTRL);
			}
				ispdma->state = enable;
		}
		break;
	case ISP_PIPELINE_STREAM_STOPPED:
		if (--ispdma->stream_refcnt == 0) {
			dma_working_flag = get_dma_working_flag(ispdma);

			if (ispdma->disp_out == ISPDMA_OUTPUT_MEMORY) {
				if (dma_working_flag & DMA_DISP_WORKING)
					ispdma_stop_dma(ispdma,
							ISPDMA_PORT_DISPLAY);
			}

			if (ispdma->codec_out == ISPDMA_OUTPUT_MEMORY) {
				if (dma_working_flag & DMA_CODEC_WORKING)
					ispdma_stop_dma(ispdma,
							ISPDMA_PORT_CODEC);
			}

			if (ispdma->input == ISPDMA_INPUT_MEMORY) {
				if (dma_working_flag & DMA_INPUT_WORKING)
					ispdma_stop_dma(ispdma,
							ISPDMA_PORT_INPUT);
			} else if ((ispdma->input == ISPDMA_INPUT_CCIC_1)
				&& (isp->sensor_connected == true)) {
				regval = mvisp_reg_readl(isp, ISP_IOMEM_ISPDMA,
					ISPDMA_MAINCTRL);
				regval &= ~(1 << 24);
				mvisp_reg_writel(isp, regval, ISP_IOMEM_ISPDMA,
					ISPDMA_MAINCTRL);
			}

			ispdma->state = enable;
			ispdma_reset_counter(ispdma);
		} else if (ispdma->stream_refcnt < 0)
			ispdma->stream_refcnt = 0;
		break;
	default:
		break;
	}

	mutex_unlock(&ispdma->ispdma_mutex);
	return 0;
}

static int ispdma_io_set_stream(struct v4l2_subdev *sd
			, int *enable)
{
	enum isp_pipeline_stream_state state;

	if ((NULL == sd) || (NULL == enable) || (*enable < 0))
		return -EINVAL;

	state = *enable ? ISP_PIPELINE_STREAM_CONTINUOUS :\
			ISP_PIPELINE_STREAM_STOPPED;

	return ispdma_set_stream(sd, state);
}

static int ispdma_get_isp_func_clk(struct isp_ispdma_device *ispdma,
		int *clk_rate)
{
	struct mvisp_device *isp = to_mvisp_device(ispdma);

	if (!clk_rate || !isp || !isp->clock[0])
		return -EINVAL;

	*clk_rate = clk_get_rate(isp->clock[0]) / 1000000;

	return 0;
}

void isp_reset_clock(void)
{
	struct isp_ispdma_device *ispdma = g_ispdma;

	if (!ispdma)
		goto out;

	if (atomic_read(&isp_reset_count)) {
		if (ispdma->mvisp_reset) {
			ispdma_context_save(ispdma);
			ispdma->mvisp_reset(
				(struct v4l2_ispdma_reset *) ispdma);
			ispdma_context_restore(ispdma);
		}
		complete_all(&ispdma->isp_reset_event);
	}
out:
	atomic_set(&isp_reset_count, 0);
}
EXPORT_SYMBOL(isp_reset_clock);

static int ispdma_lcd_dma_ops(int *val)
{
	if (!val)
		return -EINVAL;

	if (*val)
		panel_dma_ctrl(1);
	else
		panel_dma_ctrl(0);

	return 0;
}

static long ispdma_ioctl(struct v4l2_subdev *sd
			, unsigned int cmd, void *arg)
{
	struct isp_ispdma_device *ispdma;
	int ret;

	ispdma = v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_PRIVATE_DXOIPC_SET_FB:
		ret = ispdma_config_fb
			(ispdma, (struct v4l2_dxoipc_set_fb *)arg);
		break;
	case VIDIOC_PRIVATE_DXOIPC_SET_FB_DC:
		ret = ispdma_config_fb_dc
			(ispdma, (struct v4l2_dxoipc_set_fb *)arg);
		break;
	case VIDIOC_PRIVATE_DXOIPC_WAIT_IPC:
		ret = ispdma_wait_ipc
			(ispdma,  (struct v4l2_dxoipc_ipcwait *)arg);
		break;
	case VIDIOC_PRIVATE_DXOIPC_SET_STREAM:
		ret = ispdma_stream_cfg
			(ispdma,
			(struct v4l2_dxoipc_streaming_config *)arg);
		break;
	case VIDIOC_PRIVATE_DXOIPC_CONFIG_CODEC:
		ret = ispdma_config_codec(ispdma,
			(struct v4l2_dxoipc_config_codec *) arg);
		break;
	case VIDIOC_PRIVATE_ISPDMA_CAPTURE_MODE:
		ret = ispdma_config_capture_mode(ispdma,
			(struct v4l2_ispdma_capture_mode *) arg);
		break;

	case VIDIOC_PRIVATE_ISPDMA_RESET:
		atomic_set(&isp_reset_count, 1);
		wait_for_completion_timeout(&ispdma->isp_reset_event,
				HZ >> 1);

		ret = 0;
		break;
	case VIDIOC_PRIVATE_ISPDMA_GETDELTA:
		ret = ispdma_getdelta(
				(struct v4l2_ispdma_timeinfo *) arg, false);
		break;
	case VIDIOC_PRIVATE_ISPDMA_DUMP_REGISTERS:
		ret = ispdma_dump_regs(ispdma,
				(struct v4l2_ispdma_dump_registers *) arg);
		break;
	case VIDIOC_PRIVATE_ISPDMA_GET_DMA_TIMEINFO:
		ret = ispdma_get_dma_timeinfo(ispdma,
				(struct v4l2_ispdma_dma_timeinfo *) arg);
		break;
	case VIDIOC_PRIVATE_ISPDMA_SET_STREAM:
		ret = ispdma_io_set_stream(sd, (int *) arg);
		break;
	case VIDIOC_PRIVATE_ISPDMA_GET_ISP_FUNC_CLK:
		ret = ispdma_get_isp_func_clk(ispdma, (int *) arg);
		break;
	case VIDIOC_PRIVATE_ISPDMA_LCD_DMA_OPS:
		ret = ispdma_lcd_dma_ops((int *) arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static struct v4l2_mbus_framefmt *
__ispdma_get_format(struct isp_ispdma_device *ispdma,
			struct v4l2_subdev_fh *fh,
			unsigned int pad,
			enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &ispdma->formats[pad];
}

/* ispdma format descriptions */
static const struct pad_formats ispdma_input_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_Y12_1X12, V4L2_COLORSPACE_JPEG},
};

static const struct pad_formats ispdma_disp_out_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_Y12_1X12, V4L2_COLORSPACE_JPEG},
};

static const struct pad_formats ispdma_codec_out_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_Y12_1X12, V4L2_COLORSPACE_JPEG},
};

static int ispdma_try_format(
				struct isp_ispdma_device *ispdma,
				struct v4l2_subdev_fh *fh, unsigned int pad,
				struct v4l2_mbus_framefmt *fmt,
				enum v4l2_subdev_format_whence which)
{
	int ret = 0;
	int i;

	switch (pad) {
	case ISPDMA_PAD_SINK:
		if (ispdma->input
				== ISPDMA_INPUT_MEMORY) {
			fmt->width =
				min_t(u32, fmt->width, ISPDMA_MAX_IN_WIDTH);
			fmt->height =
				min_t(u32, fmt->height, ISPDMA_MAX_IN_HEIGHT);
		}

		for (i = 0; i < ARRAY_SIZE(ispdma_input_fmts); i++) {
			if (fmt->code == ispdma_input_fmts[i].mbusfmt) {
				fmt->colorspace =
					ispdma_input_fmts[i].colorspace;
				break;
			}
		}

		if (i >= ARRAY_SIZE(ispdma_input_fmts))
			ret = -EINVAL;

		break;
	case ISPDMA_PAD_DISP_SRC:
		fmt->width =
				min_t(u32, fmt->width,
					ISPDMA_MAX_DISP_WIDTH);
		fmt->height =
				min_t(u32, fmt->height,
					ISPDMA_MAX_DISP_HEIGHT);

		for (i = 0; i < ARRAY_SIZE(ispdma_disp_out_fmts); i++) {
			if (fmt->code == ispdma_disp_out_fmts[i].mbusfmt) {
				fmt->colorspace =
					ispdma_disp_out_fmts[i].colorspace;
				break;
			}
		}

		if (i >= ARRAY_SIZE(ispdma_disp_out_fmts))
			ret = -EINVAL;

		break;
	case ISPDMA_PAD_CODE_SRC:
		fmt->width =
				min_t(u32, fmt->width,
					ISPDMA_MAX_CODEC_WIDTH);
		fmt->height =
				min_t(u32, fmt->height,
					ISPDMA_MAX_CODEC_HEIGHT);

		for (i = 0; i < ARRAY_SIZE(ispdma_codec_out_fmts); i++) {
			if (fmt->code == ispdma_codec_out_fmts[i].mbusfmt) {
				fmt->colorspace =
					ispdma_codec_out_fmts[i].colorspace;
				break;
			}
		}

		if (i >= ARRAY_SIZE(ispdma_codec_out_fmts))
			ret = -EINVAL;

		break;
	default:
		ret = -EINVAL;
		break;
	}

	fmt->field = V4L2_FIELD_NONE;

	return ret;
}

static int ispdma_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	int ret = 0;
	struct isp_ispdma_device *ispdma
				= v4l2_get_subdevdata(sd);

	mutex_lock(&ispdma->ispdma_mutex);

	switch (code->pad) {
	case ISPDMA_PAD_SINK:
		if (code->index >=
				ARRAY_SIZE(ispdma_input_fmts))
			ret = -EINVAL;
		else
			code->code =
				ispdma_input_fmts[code->index].mbusfmt;
		break;
	case ISPDMA_PAD_DISP_SRC:
		if (code->index >=
				ARRAY_SIZE(ispdma_disp_out_fmts))
			ret = -EINVAL;
		else
			code->code =
				ispdma_disp_out_fmts[code->index].mbusfmt;
		break;
	case ISPDMA_PAD_CODE_SRC:
		if (code->index >=
				ARRAY_SIZE(ispdma_codec_out_fmts))
			ret = -EINVAL;
		else
			code->code =
				ispdma_codec_out_fmts[code->index].mbusfmt;
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&ispdma->ispdma_mutex);
	return ret;
}

static int ispdma_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct isp_ispdma_device *ispdma
			= v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt format;
	int ret = 0;

	if (fse->index != 0)
		return -EINVAL;

	format.code = fse->code;
	format.width = 1;
	format.height = 1;
	ret = ispdma_try_format(ispdma, fh,
		fse->pad, &format, V4L2_SUBDEV_FORMAT_TRY);
	if (ret)
		return ret;
	fse->min_width = format.width;
	fse->min_height = format.height;
	if (format.code != fse->code)
		return -EINVAL;

	format.code = fse->code;
	format.width = -1;
	format.height = -1;
	ret = ispdma_try_format(ispdma, fh,
		fse->pad, &format, V4L2_SUBDEV_FORMAT_TRY);
	if (ret)
		return ret;
	fse->max_width = format.width;
	fse->max_height = format.height;
	if (format.code != fse->code)
		return -EINVAL;

	return 0;
}

static int ispdma_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct isp_ispdma_device *ispdma = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;
	int ret = 0;

	mutex_lock(&ispdma->ispdma_mutex);

	format = __ispdma_get_format(ispdma, fh, fmt->pad, fmt->which);
	if (format == NULL)
		ret = -EINVAL;
	else
		fmt->format = *format;

	mutex_unlock(&ispdma->ispdma_mutex);

	return 0;
}

static int ispdma_config_format(
			struct isp_ispdma_device *ispdma,
			unsigned int pad)
{
	struct v4l2_mbus_framefmt *format = &ispdma->formats[pad];
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	unsigned long width, height, pitch, in_bpp;
	int cfg_out_fmt;
	u32 regval;
	int ret = 0;

	width = format->width;
	height = format->height;

	switch (format->code) {
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		in_bpp = 0;
		pitch = width;
		cfg_out_fmt = 0;
		break;
	case V4L2_MBUS_FMT_UYVY8_1X16:
		in_bpp = 0;
		pitch = width * 2;
		cfg_out_fmt = 0;
		break;
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		in_bpp = 1;
		pitch = width * 10 / 8;
		cfg_out_fmt = 0;
		break;
	case V4L2_MBUS_FMT_Y12_1X12:
		in_bpp = 2;
		/*Y pitch for YUV420M*/
		pitch = width;
		cfg_out_fmt = 1;
		break;
	default:
		in_bpp = 0;
		pitch  = 0;
		cfg_out_fmt = 0;
		ret = -EINVAL;
		break;
	}

	if (ret != 0)
		return ret;

	switch (pad) {
	case ISPDMA_PAD_SINK:
		regval = ((height & ISPDMA_MAX_IN_HEIGHT) << 16)
				| (width & ISPDMA_MAX_IN_WIDTH);
		mvisp_reg_writel(isp, regval, ISP_IOMEM_ISPDMA,
				ISPDMA_INPSDMA_PIXSZ);
		/* Set input BPP */
		regval = mvisp_reg_readl(isp,
				ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);
		regval |= in_bpp << 1;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_INPSDMA_CTRL);

		break;
	case ISPDMA_PAD_CODE_SRC:
		regval = pitch & ISPDMA_MAX_CODEC_PITCH;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_CODEC_PITCH);

		if (isp->cpu_type == MV_PXA988) {
			/*set output format*/
			regval = mvisp_reg_readl(isp,
					ISP_IOMEM_ISPDMA, ISPDMA_CODEC_CTRL_1);
			regval |= cfg_out_fmt << 4;
			mvisp_reg_writel(isp, regval,
					ISP_IOMEM_ISPDMA, ISPDMA_CODEC_CTRL_1);
		}
		break;
	case ISPDMA_PAD_DISP_SRC:
		regval = pitch & ISPDMA_MAX_DISP_PITCH;
		mvisp_reg_writel(isp, regval,
				ISP_IOMEM_ISPDMA, ISPDMA_DISP_PITCH);

		if (isp->cpu_type == MV_PXA988) {
			/*set output format*/
			regval = mvisp_reg_readl(isp,
					ISP_IOMEM_ISPDMA, ISPDMA_DISP_CTRL_1);
			regval |= cfg_out_fmt << 4;
			mvisp_reg_writel(isp, regval,
					ISP_IOMEM_ISPDMA, ISPDMA_DISP_CTRL_1);
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int ispdma_set_format(
					struct v4l2_subdev *sd,
					struct v4l2_subdev_fh *fh,
					struct v4l2_subdev_format *fmt)
{
	struct isp_ispdma_device *ispdma = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;
	int ret;

	mutex_lock(&ispdma->ispdma_mutex);

	format = __ispdma_get_format(ispdma, fh,
				fmt->pad, fmt->which);
	if (format == NULL) {
		mutex_unlock(&ispdma->ispdma_mutex);
		return -EINVAL;
	}

	ret = ispdma_try_format(ispdma, fh, fmt->pad,
				&fmt->format, fmt->which);
	if (ret) {
		mutex_unlock(&ispdma->ispdma_mutex);
		return -EINVAL;
	}

	*format = fmt->format;

	if (fmt->which != V4L2_SUBDEV_FORMAT_TRY)
		ret = ispdma_config_format(ispdma, fmt->pad);
	else
		ret = 0;

	mutex_unlock(&ispdma->ispdma_mutex);

	return ret;
}

static int ispdma_init_formats(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;
	struct v4l2_mbus_framefmt *format_active, *format_try;
	struct isp_ispdma_device *ispdma = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (fh == NULL) {
		memset(&format, 0, sizeof(format));
		format.pad = ISPDMA_PAD_SINK;
		format.which =  V4L2_SUBDEV_FORMAT_ACTIVE;
		format.format.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		format.format.width = 640;
		format.format.height = 480;
		format.format.colorspace = V4L2_COLORSPACE_SRGB;
		format.format.field = V4L2_FIELD_NONE;
		ispdma_set_format(sd, fh, &format);

		format.format.code = V4L2_MBUS_FMT_UYVY8_1X16;
		format.pad = ISPDMA_PAD_CODE_SRC;
		format.format.width = 640;
		format.format.height = 480;
		format.format.colorspace = V4L2_COLORSPACE_JPEG;
		format.format.field = V4L2_FIELD_NONE;
		ispdma_set_format(sd, fh, &format);

		format.pad = ISPDMA_PAD_DISP_SRC;
		ret = ispdma_set_format(sd, fh, &format);
	} else {
		/* Copy the active format to a newly opened fh structure */
		mutex_lock(&ispdma->ispdma_mutex);
		format_active =
			__ispdma_get_format(ispdma, fh,
			ISPDMA_PAD_SINK, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (format_active == NULL) {
			ret = -EINVAL;
			goto error;
		}
		format_try =
			__ispdma_get_format(ispdma, fh,
			ISPDMA_PAD_SINK, V4L2_SUBDEV_FORMAT_TRY);
		if (format_try == NULL) {
			ret = -EINVAL;
			goto error;
		}
		memcpy(format_try, format_active,
			sizeof(struct v4l2_subdev_format));

		format_active = __ispdma_get_format(ispdma,
			fh, ISPDMA_PAD_DISP_SRC,
			V4L2_SUBDEV_FORMAT_ACTIVE);
		if (format_active == NULL) {
			ret = -EINVAL;
			goto error;
		}
		format_try = __ispdma_get_format(ispdma, fh,
			ISPDMA_PAD_DISP_SRC, V4L2_SUBDEV_FORMAT_TRY);
		if (format_try == NULL) {
			ret = -EINVAL;
			goto error;
		}
		memcpy(format_try, format_active,
			sizeof(struct v4l2_subdev_format));

		format_active = __ispdma_get_format(ispdma, fh,
			ISPDMA_PAD_CODE_SRC, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (format_active == NULL) {
			ret = -EINVAL;
			goto error;
		}
		format_try = __ispdma_get_format(ispdma, fh,
			ISPDMA_PAD_CODE_SRC, V4L2_SUBDEV_FORMAT_TRY);
		if (format_try == NULL) {
			ret = -EINVAL;
			goto error;
		}
		memcpy(format_try, format_active,
				sizeof(struct v4l2_subdev_format));

error:
		mutex_unlock(&ispdma->ispdma_mutex);
	}

	return ret;
}

static int ispdma_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct isp_ispdma_device *ispdma = v4l2_get_subdevdata(sd);
	struct mvisp_device *isp = to_mvisp_device(ispdma);

	mvisp_get(isp);

	if (ispdma->input == ISPDMA_INPUT_CCIC_1)
		ispdma_config_csi_input(ispdma);

	ispdma_init_params(ispdma);
	return ispdma_init_formats(sd, fh);
}

static int ispdma_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct isp_ispdma_device *ispdma = v4l2_get_subdevdata(sd);
	struct mvisp_device *isp = to_mvisp_device(ispdma);
	u32 regval;

	regval = mvisp_reg_readl(isp,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);
	regval &= ~(FBRX3DMAENA | FBRX2DMAENA
				| FBRX1DMAENA | FBRX0DMAENA
				| FBTX3DMAENA | FBTX2DMAENA
				| FBTX1DMAENA | FBTX0DMAENA);
	mvisp_reg_writel(isp, regval,
			ISP_IOMEM_ISPDMA, ISPDMA_DMA_ENA);

	mutex_lock(&ispdma->ispdma_mutex);
	if (ispdma->state != ISP_PIPELINE_STREAM_STOPPED) {
		ispdma->stream_refcnt = 1;
		mutex_unlock(&ispdma->ispdma_mutex);
		ispdma_set_stream(sd, 0);
	} else
		mutex_unlock(&ispdma->ispdma_mutex);

	mvisp_put(isp);

	return 0;
}

/* subdev core operations */
static const struct v4l2_subdev_core_ops ispdma_v4l2_core_ops = {
	.ioctl = ispdma_ioctl,
};

/* subdev video operations */
static const struct v4l2_subdev_video_ops ispdma_v4l2_video_ops = {
	.s_stream = ispdma_set_stream,
};

/* subdev pad operations */
static const struct v4l2_subdev_pad_ops ispdma_v4l2_pad_ops = {
	.enum_mbus_code = ispdma_enum_mbus_code,
	.enum_frame_size = ispdma_enum_frame_size,
	.get_fmt = ispdma_get_format,
	.set_fmt = ispdma_set_format,
};

/* subdev operations */
static const struct v4l2_subdev_ops ispdma_v4l2_ops = {
	.core = &ispdma_v4l2_core_ops,
	.video = &ispdma_v4l2_video_ops,
	.pad = &ispdma_v4l2_pad_ops,
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops
ispdma_v4l2_internal_ops = {
	.open = ispdma_open,
	.close = ispdma_close,
};

static int ispdma_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct isp_ispdma_device *ispdma = v4l2_get_subdevdata(sd);
	struct mvisp_device *isp = to_mvisp_device(ispdma);

	switch (local->index | media_entity_type(remote->entity)) {
	case ISPDMA_PAD_SINK | MEDIA_ENT_T_DEVNODE:
		/* read from memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (ispdma->input != ISPDMA_INPUT_NONE)
				return -EBUSY;
			ispdma->input = ISPDMA_INPUT_MEMORY;
		} else {
			if (ispdma->input == ISPDMA_INPUT_MEMORY)
				ispdma->input = ISPDMA_INPUT_NONE;
		}
		break;
	case ISPDMA_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		/* read from ccic */
		if (isp->sensor_connected == false)
			return -EINVAL;
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (ispdma->input != ISPDMA_INPUT_NONE)
				return -EBUSY;
			ispdma->input = ISPDMA_INPUT_CCIC_1;
			ispdma_config_csi_input(ispdma);
		} else {
			if (ispdma->input == ISPDMA_INPUT_CCIC_1)
				ispdma->input = ISPDMA_INPUT_NONE;
		}
		break;
	case ISPDMA_PAD_CODE_SRC | MEDIA_ENT_T_DEVNODE:
		/* write to memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (ispdma->codec_out == ISPDMA_OUTPUT_MEMORY)
				return -EBUSY;
			ispdma->codec_out = ISPDMA_OUTPUT_MEMORY;
		} else {
			ispdma->codec_out = ISPDMA_OUTPUT_NONE;
		}
		break;

	case ISPDMA_PAD_DISP_SRC | MEDIA_ENT_T_DEVNODE:
		/* write to memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (ispdma->disp_out == ISPDMA_OUTPUT_MEMORY)
				return -EBUSY;
			ispdma->disp_out = ISPDMA_OUTPUT_MEMORY;
		} else {
			ispdma->disp_out = ISPDMA_OUTPUT_NONE;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static const struct media_entity_operations ispdma_media_ops = {
	.link_setup = ispdma_link_setup,
};

static int ispdma_init_entities(struct isp_ispdma_device *ispdma)
{
	struct v4l2_subdev *sd = &ispdma->subdev;
	struct media_pad *pads = ispdma->pads;
	struct media_entity *me = &sd->entity;
	int ret;

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	{
		struct mvisp_device *isp = to_mvisp_device(ispdma);
		int ret;

		ispdma->mcd_dma_display = default_mcd_dma;
		strcpy(ispdma->mcd_dma_display.entity.name, "dma");
		ret = mcd_entity_init(&ispdma->mcd_dma_display.entity,
				&isp->mcd_root_display.mcd);
		if (ret < 0)
			return ret;
		else
			isp->mcd_root_display.pitem[MCD_DMA] =
				&ispdma->mcd_dma_display.entity;
		printk(KERN_INFO "cam: mount node debugfs/%s/%s\n",
				isp->mcd_root_display.mcd.name,
				ispdma->mcd_dma_display.entity.name);

		ispdma->mcd_dma_codec = default_mcd_dma;
		strcpy(ispdma->mcd_dma_codec.entity.name, "dma");
		ret = mcd_entity_init(&ispdma->mcd_dma_codec.entity,
					&isp->mcd_root_codec.mcd);
		if (ret < 0)
			return ret;
		else
			isp->mcd_root_codec.pitem[MCD_DMA] =
				&ispdma->mcd_dma_codec.entity;
		printk(KERN_INFO "cam: mount node debugfs/%s/%s\n",
				isp->mcd_root_codec.mcd.name,
				ispdma->mcd_dma_codec.entity.name);
	}
#endif

	g_ispdma = ispdma;
	spin_lock_init(&ispdma->ipc_irq_lock);
	spin_lock_init(&ispdma->dma_irq_lock);
	spin_lock_init(&ispdma->dmaflg_lock);
	init_completion(&ispdma->ipc_event);
	init_completion(&ispdma->isp_reset_event);
	mutex_init(&ispdma->ispdma_mutex);

	ispdma->input = ISPDMA_INPUT_NONE;
	ispdma->disp_out = ISPDMA_OUTPUT_NONE;
	ispdma->codec_out = ISPDMA_OUTPUT_NONE;
	ispdma->state = ISP_PIPELINE_STREAM_STOPPED;
	ispdma->stream_refcnt = 0;

	v4l2_subdev_init(sd, &ispdma_v4l2_ops);
	sd->internal_ops = &ispdma_v4l2_internal_ops;
	strlcpy(sd->name, "mvisp_ispdma", sizeof(sd->name));
	sd->grp_id = 1 << 16;	/* group ID for isp subdevs */
	v4l2_set_subdevdata(sd, ispdma);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

#if 0
	v4l2_ctrl_handler_init(&ispdma->ctrls, 2);
	v4l2_ctrl_new_std(&ispdma->ctrls, &ispdma_ctrl_ops,
				V4L2_CID_BRIGHTNESS,
				ISPPRV_BRIGHT_LOW, ISPPRV_BRIGHT_HIGH,
				ISPPRV_BRIGHT_STEP, ISPPRV_BRIGHT_DEF);
	v4l2_ctrl_new_std(&ispdma->ctrls, &ispdma_ctrl_ops,
				V4L2_CID_CONTRAST,
				ISPPRV_CONTRAST_LOW, ISPPRV_CONTRAST_HIGH,
				ISPPRV_CONTRAST_STEP, ISPPRV_CONTRAST_DEF);
	v4l2_ctrl_handler_setup(&ispdma->ctrls);
	sd->ctrl_handler = &ispdma->ctrls;
#endif

	pads[ISPDMA_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	pads[ISPDMA_PAD_CODE_SRC].flags = MEDIA_PAD_FL_SOURCE;
	pads[ISPDMA_PAD_DISP_SRC].flags = MEDIA_PAD_FL_SOURCE;

	me->ops = &ispdma_media_ops;
	ret = media_entity_init(me, ISPDMA_PADS_NUM, pads, 0);
	if (ret < 0)
		return ret;

	ispdma_init_formats(sd, NULL);

	ispdma->vd_in.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ispdma->vd_in.ops = &ispdma_video_ops;
	ispdma->vd_in.isp = to_mvisp_device(ispdma);
	ispdma->vd_disp_out.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ispdma->vd_disp_out.ops = &ispdma_video_ops;
	ispdma->vd_disp_out.isp = to_mvisp_device(ispdma);
	ispdma->vd_codec_out.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ispdma->vd_codec_out.ops = &ispdma_video_ops;
	ispdma->vd_codec_out.isp = to_mvisp_device(ispdma);

	ret = mvisp_video_init
		(&ispdma->vd_in, ISP_VIDEO_INPUT_NAME);
	if (ret < 0)
		return ret;

	ret = mvisp_video_init
		(&ispdma->vd_codec_out, ISP_VIDEO_CODEC_NAME);
	if (ret < 0)
		return ret;
	ret = mvisp_video_init
		(&ispdma->vd_disp_out, ISP_VIDEO_DISPLAY_NAME);
	if (ret < 0)
		return ret;

	/* Connect the video nodes to the ispdma subdev. */
	ret = media_entity_create_link
			(&ispdma->vd_in.video.entity, 0,
			&ispdma->subdev.entity, ISPDMA_PAD_SINK, 0);
	if (ret < 0)
		return ret;

	ret = media_entity_create_link
			(&ispdma->subdev.entity, ISPDMA_PAD_CODE_SRC,
			&ispdma->vd_codec_out.video.entity, 0, 0);
	if (ret < 0)
		return ret;

	ret = media_entity_create_link
			(&ispdma->subdev.entity, ISPDMA_PAD_DISP_SRC,
			&ispdma->vd_disp_out.video.entity, 0, 0);
	if (ret < 0)
		return ret;
	return 0;
}

void mv_ispdma_unregister_entities(struct isp_ispdma_device *ispdma)
{
	media_entity_cleanup(&ispdma->subdev.entity);

	v4l2_device_unregister_subdev(&ispdma->subdev);
/*	v4l2_ctrl_handler_free(&ispdma->ctrls); */
	mvisp_video_unregister(&ispdma->vd_in);
	mvisp_video_unregister(&ispdma->vd_disp_out);
	mvisp_video_unregister(&ispdma->vd_codec_out);
}

int mv_ispdma_register_entities(struct isp_ispdma_device *ispdma,
	struct v4l2_device *vdev)
{
	int ret;

	/* Register the subdev and video nodes. */
	ret = v4l2_device_register_subdev(vdev, &ispdma->subdev);
	if (ret < 0)
		goto error;

	ret = mvisp_video_register(&ispdma->vd_in, vdev);
	if (ret < 0)
		goto error;

	ret = mvisp_video_register(&ispdma->vd_codec_out, vdev);
	if (ret < 0)
		goto error;

	ret = mvisp_video_register(&ispdma->vd_disp_out, vdev);
	if (ret < 0)
		goto error;

	return 0;

error:
	mv_ispdma_unregister_entities(ispdma);
	return ret;
}

void mv_ispdma_cleanup(struct mvisp_device *isp)
{
}

int mv_ispdma_init(struct mvisp_device *isp)
{
	struct isp_ispdma_device *ispdma = &isp->mvisp_ispdma;
	struct mvisp_platform_data *pdata = isp->pdata;
	int ret;

	ispdma_init_params(ispdma);
	ispdma->mvisp_reset = pdata->mvisp_reset;

	ret = ispdma_init_entities(ispdma);
	if (ret < 0)
		goto out;

out:
	if (ret)
		mv_ispdma_cleanup(isp);

	return ret;
}
