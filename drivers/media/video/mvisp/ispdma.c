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
#include <linux/dma-mapping.h>

#include "isp.h"
#include "ispreg.h"
#include "ispdma.h"
#include <video/mmpdisp_export_funcs.h>
#include <linux/interrupt.h>
#include <mach/features.h>

#define MC_DXODMA_DRV_NAME	"dxo-dma"
#define ISPDMA_NAME		"mvisp_ispdma"
#define DXO_CORE_NAME		"dxo-core"
#define DXO_DMAD_NAME		"dxo-display"
#define DXO_DMAC_NAME		"dxo-codec"
#define DXO_DMAI_NAME		"dxo-input"
#define DXO_IRQ_NAME		"dxo-wrapper-irq"
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

#define FB_DESC_ARRAY_SIZE (1024*16)

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

static void __maybe_unused ispdma_reg_dump(
		struct isp_ispdma_device *ispdma)
{
	int cnt;

	for (cnt = 0; cnt < ISPDMA_INPSDMA_MAX_CTX; cnt++) {
		ispdma_reg_list[cnt].val =
			mod_read(ispdma, ispdma_reg_list[cnt].reg);

		dev_warn(ispdma->dev, "REG[0x%08X]--->0x%08X\n",
			ispdma_reg_list[cnt].reg,
			ispdma_reg_list[cnt].val);
	}

	return;
}

static int ispdma_dump_regs(struct isp_ispdma_device *ispdma,
			struct v4l2_ispdma_dump_registers *regs)
{
	if (NULL == regs)
		return -EINVAL;

	regs->ispdma_mainctrl = mod_read(ispdma, ISPDMA_MAINCTRL);
	regs->ispdma_dmaena = mod_read(ispdma, ISPDMA_DMA_ENA);
	regs->ispdma_clkena = mod_read(ispdma, ISPDMA_CLKENA);
	regs->ispdma_irqraw = mod_read(ispdma, ISPDMA_IRQRAW);
	regs->ispdma_irqmask = mod_read(ispdma, ISPDMA_IRQMASK);
	regs->ispdma_irqstat = mod_read(ispdma, ISPDMA_IRQSTAT);
	regs->ispdma_insz = mod_read(ispdma, ISPDMA_INSZ);
	regs->ispdma_inpsdma_ctrl = mod_read(ispdma, ISPDMA_INPSDMA_CTRL);
	regs->ispdma_fbtx0_sdca = mod_read(ispdma, ISPDMA_FBTX0_SDCA);
	regs->ispdma_fbtx0_dcsz = mod_read(ispdma, ISPDMA_FBTX0_DCSZ);
	regs->ispdma_fbtx0_ctrl = mod_read(ispdma, ISPDMA_FBTX0_CTRL);
	regs->ispdma_fbtx0_dstsz = mod_read(ispdma, ISPDMA_FBTX0_DSTSZ);
	regs->ispdma_fbtx0_dstaddr = mod_read(ispdma, ISPDMA_FBTX0_DSTADDR);
	regs->ispdma_fbtx0_tmr = mod_read(ispdma, ISPDMA_FBTX0_TMR);
	regs->ispdma_fbtx0_ramctrl = mod_read(ispdma, ISPDMA_FBTX0_RAMCTRL);
	regs->ispdma_fbrx0_sdca = mod_read(ispdma, ISPDMA_FBRX0_SDCA);
	regs->ispdma_fbrx0_dcsz = mod_read(ispdma, ISPDMA_FBRX0_DCSZ);
	regs->ispdma_fbrx0_ctrl = mod_read(ispdma, ISPDMA_FBRX0_CTRL);
	regs->ispdma_fbrx0_tmr = mod_read(ispdma, ISPDMA_FBRX0_TMR);
	regs->ispdma_fbrx0_ramctrl = mod_read(ispdma, ISPDMA_FBRX0_RAMCTRL);
	regs->ispdma_fbrx0_stat = mod_read(ispdma, ISPDMA_FBRX0_STAT);
	regs->ispdma_fbtx1_sdca = mod_read(ispdma, ISPDMA_FBTX1_SDCA);
	regs->ispdma_fbtx1_dcsz = mod_read(ispdma, ISPDMA_FBTX1_DCSZ);
	regs->ispdma_fbtx1_ctrl = mod_read(ispdma, ISPDMA_FBTX1_CTRL);
	regs->ispdma_fbtx1_dstsz = mod_read(ispdma, ISPDMA_FBTX1_DSTSZ);
	regs->ispdma_fbtx1_dstaddr = mod_read(ispdma, ISPDMA_FBTX1_DSTADDR);
	regs->ispdma_fbtx1_tmr = mod_read(ispdma, ISPDMA_FBTX1_TMR);
	regs->ispdma_fbtx1_ramctrl = mod_read(ispdma, ISPDMA_FBTX1_RAMCTRL);
	regs->ispdma_fbrx1_sdca = mod_read(ispdma, ISPDMA_FBRX1_SDCA);
	regs->ispdma_fbrx1_dcsz = mod_read(ispdma, ISPDMA_FBRX1_DCSZ);
	regs->ispdma_fbrx1_ctrl = mod_read(ispdma, ISPDMA_FBRX1_CTRL);
	regs->ispdma_fbrx1_tmr = mod_read(ispdma, ISPDMA_FBRX1_TMR);
	regs->ispdma_fbrx1_ramctrl = mod_read(ispdma, ISPDMA_FBRX1_RAMCTRL);
	regs->ispdma_fbrx1_stat = mod_read(ispdma, ISPDMA_FBRX1_STAT);
	regs->ispdma_fbtx2_sdca = mod_read(ispdma, ISPDMA_FBTX2_SDCA);
	regs->ispdma_fbtx2_dcsz = mod_read(ispdma, ISPDMA_FBTX2_DCSZ);
	regs->ispdma_fbtx2_ctrl = mod_read(ispdma, ISPDMA_FBTX2_CTRL);
	regs->ispdma_fbtx2_dstsz = mod_read(ispdma, ISPDMA_FBTX2_DSTSZ);
	regs->ispdma_fbtx2_dstaddr = mod_read(ispdma, ISPDMA_FBTX2_DSTADDR);
	regs->ispdma_fbtx2_tmr = mod_read(ispdma, ISPDMA_FBTX2_TMR);
	regs->ispdma_fbtx2_ramctrl = mod_read(ispdma, ISPDMA_FBTX2_RAMCTRL);
	regs->ispdma_fbrx2_sdca = mod_read(ispdma, ISPDMA_FBRX2_SDCA);
	regs->ispdma_fbrx2_dcsz = mod_read(ispdma, ISPDMA_FBRX2_DCSZ);
	regs->ispdma_fbrx2_ctrl = mod_read(ispdma, ISPDMA_FBRX2_CTRL);
	regs->ispdma_fbrx2_tmr = mod_read(ispdma, ISPDMA_FBRX2_TMR);
	regs->ispdma_fbrx2_ramctrl = mod_read(ispdma, ISPDMA_FBRX2_RAMCTRL);
	regs->ispdma_fbrx2_stat = mod_read(ispdma, ISPDMA_FBRX2_STAT);
	regs->ispdma_fbrx2_sdca = mod_read(ispdma, ISPDMA_FBTX3_SDCA);
	regs->ispdma_fbtx3_dcsz = mod_read(ispdma, ISPDMA_FBTX3_DCSZ);
	regs->ispdma_fbtx3_ctrl = mod_read(ispdma, ISPDMA_FBTX3_CTRL);
	regs->ispdma_fbtx3_dstsz = mod_read(ispdma, ISPDMA_FBTX3_DSTSZ);
	regs->ispdma_fbtx3_dstaddr = mod_read(ispdma, ISPDMA_FBTX3_DSTADDR);
	regs->ispdma_fbtx3_tmr = mod_read(ispdma, ISPDMA_FBTX3_TMR);
	regs->ispdma_fbtx3_ramctrl = mod_read(ispdma, ISPDMA_FBTX3_RAMCTRL);
	regs->ispdma_fbtx3_sdca = mod_read(ispdma, ISPDMA_FBRX3_SDCA);
	regs->ispdma_fbrx3_dcsz = mod_read(ispdma, ISPDMA_FBRX3_DCSZ);
	regs->ispdma_fbrx3_ctrl = mod_read(ispdma, ISPDMA_FBRX3_CTRL);
	regs->ispdma_fbrx3_tmr = mod_read(ispdma, ISPDMA_FBRX3_TMR);
	regs->ispdma_fbrx3_ramctrl = mod_read(ispdma, ISPDMA_FBRX3_RAMCTRL);
	regs->ispdma_fbrx3_stat = mod_read(ispdma, ISPDMA_FBRX3_STAT);
	regs->ispdma_disp_ctrl = mod_read(ispdma, ISPDMA_DISP_CTRL);
	regs->ispdma_disp_dstsz = mod_read(ispdma, ISPDMA_DISP_DSTSZ);
	regs->ispdma_disp_dstaddr = mod_read(ispdma, ISPDMA_DISP_DSTADDR);
	regs->ispdma_disp_ramctrl = mod_read(ispdma, ISPDMA_DISP_RAMCTRL);
	regs->ispdma_disp_pitch = mod_read(ispdma, ISPDMA_DISP_PITCH);
	regs->ispdma_codec_ctrl = mod_read(ispdma, ISPDMA_CODEC_CTRL);
	regs->ispdma_codec_dstsz = mod_read(ispdma, ISPDMA_CODEC_DSTSZ);
	regs->ispdma_codec_dstaddr = mod_read(ispdma, ISPDMA_CODEC_DSTADDR);
	regs->ispdma_codec_ramctrl = mod_read(ispdma, ISPDMA_CODEC_RAMCTRL);
	regs->ispdma_codec_stat = mod_read(ispdma, ISPDMA_CODEC_STAT);
	regs->ispdma_codec_pitch = mod_read(ispdma, ISPDMA_CODEC_PITCH);
	regs->ispdma_codec_vbsz = mod_read(ispdma, ISPDMA_CODEC_VBSZ);
	regs->ispdma_inpsdma_srcaddr = mod_read(ispdma, ISPDMA_INPSDMA_SRCADDR);
	regs->ispdma_inpsdma_srcsz = mod_read(ispdma, ISPDMA_INPSDMA_SRCSZ);
	regs->ispdma_inpsdma_pixsz = mod_read(ispdma, ISPDMA_INPSDMA_PIXSZ);
	regs->isp_irqraw = mod_read(ispdma, ISP_IRQRAW);
	regs->isp_irqmask = mod_read(ispdma, ISP_IRQMASK);
	regs->isp_irqstat = mod_read(ispdma, ISP_IRQSTAT);

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

static int ispdma_get_time(struct v4l2_ispdma_timeinfo *param)
{
	struct timespec ts;

	if (NULL == param)
		return -EINVAL;

	ktime_get_ts(&ts);

	param->sec = ts.tv_sec;
	param->usec = ts.tv_nsec / NSEC_PER_USEC;

	return 0;
}

static int ispdma_wait_ipc(struct isp_ispdma_device *ispdma,
		struct v4l2_dxoipc_ipcwait *ipc_wait)
{
	int ret = 0;
	unsigned long flags;
	long rc = 0;

	rc = wait_for_completion_timeout(&ispdma->ipc_event,
		(ipc_wait->timeout + MS_PER_JIFFIES - 1) / MS_PER_JIFFIES);

	spin_lock_irqsave(&ispdma->ipc_irq_lock, flags);
	if (rc == 0) {
		dev_warn(ispdma->dev, "IPC timeout %d, eof %d, ipc %d, dma %d\n",
			ispdma->mipi_ovr_cnt,
			ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].eof_cnt,
			ispdma->ipc_event_cnt,
			ispdma->dma_event_cnt);
		ret = -ETIMEDOUT;
	}
	INIT_COMPLETION(ispdma->ipc_event);
	ipc_wait->tickinfo.sec = ispdma->tickinfo.sec;
	ipc_wait->tickinfo.usec = ispdma->tickinfo.usec;
	spin_unlock_irqrestore(&ispdma->ipc_irq_lock, flags);

	return ret;
}

void mv_ispdma_ipc_isr_handler(struct isp_ispdma_device *ispdma)
{
	spin_lock(&ispdma->ipc_irq_lock);
	ispdma->ipc_event_cnt++;
	ispdma_get_time(&ispdma->tickinfo);
	spin_unlock(&ispdma->ipc_irq_lock);
	complete_all(&ispdma->ipc_event);
}

static int ispdma_set_fb_reg(struct isp_ispdma_device *ispdma,
	int index)
{
	u32 regoffset;
	u32 regval;

	regoffset = index * 0x100;

	/* Disable Descriptor */
	mod_clrbit(ispdma, ISPDMA_FBTX0_CTRL + regoffset, FBTX_DC_ENA);
	mod_clrbit(ispdma, ISPDMA_FBRX0_CTRL + regoffset, FBRX_DC_ENA);
	/* TX Settings */
	mod_write(ispdma, ISPDMA_FBTX0_SDCA + regoffset, 0);
	mod_write(ispdma, ISPDMA_FBTX0_DCSZ + regoffset, 0);
	mod_write(ispdma, ISPDMA_FBTX0_RAMCTRL + regoffset, 1 << 9);

	regval = ISPDMA_FBTXN_DSTSZ_MASK &
				ispdma->ispdma_cfg_fb.size[index];
	mod_write(ispdma, ISPDMA_FBTX0_DSTSZ + regoffset, regval);

	regval = ispdma->ispdma_cfg_fb.phyAddr[index];
	mod_write(ispdma, ISPDMA_FBTX0_DSTADDR + regoffset, regval);

	regval = FBTX_DMA_TIMER_VAL;
	mod_write(ispdma, ISPDMA_FBTX0_TMR + regoffset, regval);

	regval = 0x1400 |
		((ispdma->ispdma_cfg_fb.burst_wt[index] >> 3) &
		 FBTX_DMABRSTSZ);
	mod_write(ispdma, ISPDMA_FBTX0_CTRL + regoffset, regval);

	/* RX Settings */
	mod_write(ispdma, ISPDMA_FBRX0_SDCA + regoffset, 0);
	mod_write(ispdma, ISPDMA_FBRX0_DCSZ + regoffset, 0);
	mod_write(ispdma, ISPDMA_FBRX0_RAMCTRL + regoffset, 1 << 9);

	regval = FBRX_DMA_TIMER_VAL;
	mod_write(ispdma, ISPDMA_FBRX0_TMR + regoffset, regval);

	regval = (ispdma->ispdma_cfg_fb.burst_rd[index] >> 3) &
		FBRX_DMABRSTSZ;
	mod_write(ispdma, ISPDMA_FBRX0_CTRL + regoffset, regval);

	return 0;
}

static void ispdma_set_fb_reg_dc(struct isp_ispdma_device *ispdma,
		int index)
{
	u32 regoffset;
	u32 regval;

	regoffset = index * 0x100;

	/*  TX Settings */
	regval = ispdma->ispdma_fb_dc.dc_phy_handle[index];
	mod_write(ispdma, ISPDMA_FBTX0_SDCA + regoffset, regval);
	regval = sizeof(u32)*(ispdma->ispdma_fb_dc.dc_size[index]);
	mod_write(ispdma, ISPDMA_FBTX0_DCSZ + regoffset, regval);

	regval = FBTX_DMA_TIMER_VAL;
	mod_write(ispdma, ISPDMA_FBTX0_TMR + regoffset, regval);

	regval = 0x1400 | ((ispdma->ispdma_cfg_fb.burst_wt[index] >> 3)
			& FBTX_DMABRSTSZ);
	mod_write(ispdma, ISPDMA_FBTX0_CTRL + regoffset, regval);

	/*  RX Settings */

	regval = ispdma->ispdma_fb_dc.dc_phy_handle[index];
	mod_write(ispdma, ISPDMA_FBRX0_SDCA + regoffset, regval);
	regval = sizeof(u32)*(ispdma->ispdma_fb_dc.dc_size[index]);
	mod_write(ispdma, ISPDMA_FBRX0_DCSZ + regoffset, regval);

	regval = FBRX_DMA_TIMER_VAL;
	mod_write(ispdma, ISPDMA_FBRX0_TMR + regoffset, regval);

	regval = (ispdma->ispdma_cfg_fb.burst_rd[index] >> 3) & FBRX_DMABRSTSZ;
	mod_write(ispdma, ISPDMA_FBRX0_CTRL + regoffset, regval);

	return;
}

static void ispdma_desc_enable(struct isp_ispdma_device *ispdma,
		int index, int on)
{
	u32 regoffset;

	regoffset = index * 0x100;

	if (on) {
		mod_setbit(ispdma, ISPDMA_FBTX0_CTRL + regoffset, FBTX_DC_ENA);
		mod_setbit(ispdma, ISPDMA_FBRX0_CTRL + regoffset, FBRX_DC_ENA);
	} else {
		mod_clrbit(ispdma, ISPDMA_FBTX0_CTRL + regoffset, FBTX_DC_ENA);
		mod_clrbit(ispdma, ISPDMA_FBRX0_CTRL + regoffset, FBRX_DC_ENA);
	}

	return;
}

static int ispdma_descriptor_chain(struct isp_ispdma_device *ispdma,
		int cnt, u32 dpaddr, u32 dsize)
{
	if (ispdma->ispdma_fb_dc.dc_dw_cnt > ispdma->ispdma_fb_dc.dc_max_size) {
		printk(KERN_ERR "descriptor chain num is overfolow!\n");
		return -EINVAL;
	}

	ispdma->ispdma_fb_dc.dc_vir[cnt][ispdma->ispdma_fb_dc.dc_dw_cnt]
								= dpaddr;
	ispdma->ispdma_fb_dc.dc_dw_cnt++;
	ispdma->ispdma_fb_dc.dc_vir[cnt][ispdma->ispdma_fb_dc.dc_dw_cnt]
								= dsize;
	ispdma->ispdma_fb_dc.dc_dw_cnt++;

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
static int __maybe_unused va_to_pa(struct isp_ispdma_device *ispdma,
		int index, u32 user_addr, u32 size)
{
	u32 paddr, paddr_next;
	u32 vaddr_start = user_addr;
	u32 vaddr_cur = PAGE_ALIGN(user_addr);
	u32 vaddr_end = user_addr + size;
	u32 cur_dcsize;
	struct page *page = NULL;
	int dc_num;
	int ret = 0;

	if (vaddr_cur == 0) {
		printk(KERN_ERR "Descriptor vaddr_cur is NULL\n");
		return -EINVAL;
	}

	cur_dcsize = vaddr_cur - vaddr_start;

	paddr = uva_to_pa(vaddr_start, &page);
	if (!paddr) {
		printk(KERN_ERR "Descriptor vaddr_start->paddr is NULL\n");
		return -EINVAL;
	}

	dc_num = 0;

	while (vaddr_cur <= vaddr_end) {
		paddr_next = uva_to_pa(vaddr_cur, &page);
		if (!paddr_next) {
			printk(KERN_ERR "Descriptor vaddr_cur->paddr is NULL\n");
			return -EINVAL;
		}

		if ((vaddr_end - vaddr_cur) >= PAGE_SIZE) {
			if ((paddr_next - paddr) != cur_dcsize) {
				ispdma_descriptor_chain(ispdma,
						index, paddr, cur_dcsize);
				dc_num++;
				paddr = paddr_next;
				cur_dcsize = 0;
			}

			vaddr_cur += PAGE_SIZE;
			cur_dcsize += PAGE_SIZE;

		} else {
			if ((paddr_next - paddr) != cur_dcsize) {
				ispdma_descriptor_chain(ispdma,
						index, paddr, cur_dcsize);
				dc_num++;
				paddr = paddr_next;
				cur_dcsize = 0;
			}
			if (((vaddr_end - vaddr_cur) == 0)
					&& (cur_dcsize == 0)) {
				ret = 2 * dc_num;
				break;
			} else {
				cur_dcsize += vaddr_end - vaddr_cur;
				ispdma_descriptor_chain(ispdma,
						index, paddr, cur_dcsize);
				dc_num++;
				ret = 2 * dc_num;
				break;
			}
		}
	}
	return ret;
}

static void ispdma_fill_descriptor(struct isp_ispdma_device *ispdma, int index,
					u32 page_offset, u32 size,
					struct page **pages, u32 num_pages)
{
	int i;
	unsigned long prev_pa, cur_pa;
	unsigned long len;

	for (i = 0; i < num_pages; ++i) {
		if (unlikely(!i)) {
			prev_pa = dma_map_page(ispdma->dev, pages[i], 0,
						PAGE_SIZE, DMA_TO_DEVICE);
			prev_pa += page_offset;
			len = min_t(u32, (PAGE_SIZE-page_offset), size);
			size -= min_t(u32, (PAGE_SIZE-page_offset), size);
		} else {
			cur_pa = dma_map_page(ispdma->dev, pages[i], 0,
						PAGE_SIZE, DMA_TO_DEVICE);

			if (prev_pa + len == cur_pa) {
				len += min_t(u32, PAGE_SIZE, size);
				size -= min_t(u32, PAGE_SIZE, size);
			} else {
				ispdma_descriptor_chain(ispdma, index,
							prev_pa, len);
				ispdma->ispdma_fb_dc.dc_size[index] += 2;
				prev_pa = cur_pa;
				len = min_t(u32, PAGE_SIZE, size);
				size -= min_t(u32, PAGE_SIZE, size);
			}

			if (i == num_pages - 1) {
				ispdma_descriptor_chain(ispdma, index,
							prev_pa, len);
				ispdma->ispdma_fb_dc.dc_size[index] += 2;
			}
		}
	}

}
static int ispdma_map_single_fb_dc(struct isp_ispdma_device *ispdma, int index)
{
	u32 first, last;
	int num_pages_user, num_pages;
	u32 user_addr, size;
	struct ispdma_set_fb *cfg_fb = &ispdma->ispdma_cfg_fb;
	struct ispdma_dc_mod *fb_dc = &ispdma->ispdma_fb_dc;

	if (index > 3 || index < 0)
		return -EINVAL;
	user_addr = (u32)cfg_fb->virAddr[index];
	size = cfg_fb->size[index];

	first = (user_addr & PAGE_MASK) >> PAGE_SHIFT;
	last  = ((user_addr + size - 1) & PAGE_MASK) >> PAGE_SHIFT;
	num_pages_user = last - first + 1;
	fb_dc->dc_pages[index] = kzalloc(num_pages_user * sizeof(struct page *),
						GFP_KERNEL);
	if (fb_dc->dc_pages[index] == NULL)
		return -EINVAL;
	num_pages = get_user_pages(current, current->mm,
			user_addr & PAGE_MASK,
			num_pages_user,
			1,
			1, /* force */
			fb_dc->dc_pages[index],
			NULL);
	if (num_pages_user != num_pages)
		return -EINVAL;

	fb_dc->dc_dw_cnt = 0;
	ispdma_fill_descriptor(ispdma, index, (user_addr & (~PAGE_MASK)), size,
					fb_dc->dc_pages[index], num_pages_user);
	return 0;
}

static int ispdma_unmap_single_fb_dc(struct isp_ispdma_device *ispdma,
		int index)
{
	int i;
	struct ispdma_set_fb *cfg_fb = &ispdma->ispdma_cfg_fb;
	struct ispdma_dc_mod *fb_dc = &ispdma->ispdma_fb_dc;
	struct page **pages;
	u32 first, last;
	int num_pages_from_user;
	u32 user_addr;
	u32 size;

	if (index > 3 || index < 0)
		return -EINVAL;

	pages = fb_dc->dc_pages[index];
	if (!pages)
		return 0;

	user_addr = (u32)cfg_fb->virAddr[index];
	size = cfg_fb->size[index];
	first = (user_addr & PAGE_MASK) >> PAGE_SHIFT;
	last  = ((user_addr + size - 1) & PAGE_MASK) >> PAGE_SHIFT;
	num_pages_from_user = last - first + 1;

	for (i = 0; i < num_pages_from_user; ++i) {
		dma_unmap_page(ispdma->dev,
				pfn_to_dma(ispdma->dev, page_to_pfn(pages[i])),
				PAGE_SIZE, DMA_BIDIRECTIONAL);
		set_page_dirty_lock(fb_dc->dc_pages[index][i]);
		put_page(fb_dc->dc_pages[index][i]);
	}
	fb_dc->dc_size[index] = 0;
	kfree(fb_dc->dc_pages[index]);
	fb_dc->dc_pages[index] = NULL;
	return 0;
}

static int ispdma_map_fb_dc(struct isp_ispdma_device *ispdma,
	struct v4l2_dxoipc_streaming_config *stream_cfg)

{
	int cnt;
	int ret;

	for (cnt = 0; cnt < ispdma->ispdma_cfg_fb.fbcnt; cnt++) {
		ret = ispdma_map_single_fb_dc(ispdma, cnt);
		if (ret < 0) {
			dev_err(ispdma->dev, "dxo setup_fb_dc failed\n");
			goto out;
		}
		/* set descirptor chain address and size */
		ispdma_set_fb_reg_dc(ispdma, cnt);
	}
	ispdma->ispdma_fb_dc.page_mapped = 1;

	/* enable descriptor chain mode */
	for (cnt = 0; cnt < ispdma->ispdma_cfg_fb.fbcnt; cnt++)
			ispdma_desc_enable(ispdma, cnt, 1);

	return 0;

out:
	while (--cnt >= 0)
		ispdma_unmap_single_fb_dc(ispdma, cnt);

	return ret;
}
static int ispdma_unmap_fb_dc(struct isp_ispdma_device *ispdma)
{
	int cnt;

	for (cnt = 0; cnt < ispdma->ispdma_cfg_fb.fbcnt; cnt++)
		ispdma_unmap_single_fb_dc(ispdma, cnt);

	ispdma->ispdma_fb_dc.page_mapped = 0;
	return 0;
}

static int ispdma_fb_alloc_pages(struct isp_ispdma_device *ispdma,
		int index)
{
	int i;
	int num_pages;
	struct ispdma_set_fb *cfg_fb = &ispdma->ispdma_cfg_fb;
	struct ispdma_dc_mod *fb_dc = &ispdma->ispdma_fb_dc;

	if (index > 3 || index < 0)
		return -EINVAL;

	num_pages = (cfg_fb->size[index] + PAGE_SIZE - 1) >> PAGE_SHIFT;

	fb_dc->dc_pages[index] = kzalloc(num_pages * sizeof(struct page *),
			     GFP_KERNEL);
	if (!fb_dc->dc_pages[index]) {
		dev_err(ispdma->dev, "dxo alloc pages_array failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_pages; ++i) {
		fb_dc->dc_pages[index][i] = alloc_page(
				GFP_KERNEL | __GFP_NOWARN);
		if (!fb_dc->dc_pages[index][i])
			goto fail_pages_alloc;
	}

	return 0;

fail_pages_alloc:
	dev_err(ispdma->dev, "dxo alloc pages failed\n");
	while (--i >= 0)
		__free_page(fb_dc->dc_pages[index][i]);

	kfree(fb_dc->dc_pages[index]);
	return -ENOMEM;

}

static int ispdma_fb_free_pages(struct isp_ispdma_device *ispdma,
		int index)
{
	int i;
	int num_pages;
	struct ispdma_set_fb *cfg_fb = &ispdma->ispdma_cfg_fb;
	struct ispdma_dc_mod *fb_dc = &ispdma->ispdma_fb_dc;

	if (index > 3 || index < 0)
		return -EINVAL;

	num_pages = (cfg_fb->size[index] + PAGE_SIZE - 1) >> PAGE_SHIFT;

	for (i = 0; i < num_pages; ++i) {
		set_page_dirty_lock(fb_dc->dc_pages[index][i]);
		__free_page(fb_dc->dc_pages[index][i]);
	}
	kfree(fb_dc->dc_pages[index]);
	fb_dc->dc_pages[index] = NULL;

	return 0;
}

static int ispdma_setup_single_fb_dc(struct isp_ispdma_device *ispdma,
		int index)
{
	int num_pages;
	struct ispdma_set_fb *cfg_fb = &ispdma->ispdma_cfg_fb;
	struct ispdma_dc_mod *fb_dc = &ispdma->ispdma_fb_dc;
	struct page **pages;
	int ret;
	int size;

	if (index > 3 || index < 0)
		return -EINVAL;
	size = ispdma->ispdma_cfg_fb.size[index];
	ret = ispdma_fb_alloc_pages(ispdma, index);
	if (ret < 0)
		return ret;

	pages = fb_dc->dc_pages[index];
	num_pages = (cfg_fb->size[index] + PAGE_SIZE - 1) >> PAGE_SHIFT;
	ispdma->ispdma_fb_dc.dc_dw_cnt = 0;
	ispdma_fill_descriptor(ispdma, index, 0, size, pages, num_pages);
	return 0;
}


static int ispdma_release_single_fb_dc(struct isp_ispdma_device *ispdma,
		int index)
{
	int i;
	int num_pages;
	struct ispdma_set_fb *cfg_fb = &ispdma->ispdma_cfg_fb;
	struct ispdma_dc_mod *fb_dc = &ispdma->ispdma_fb_dc;
	struct page **pages;

	if (index > 3 || index < 0)
		return -EINVAL;

	pages = fb_dc->dc_pages[index];
	if (!pages)
		return 0;

	num_pages = (cfg_fb->size[index] + PAGE_SIZE - 1) >> PAGE_SHIFT;

	for (i = 0; i < num_pages; ++i)
		dma_unmap_page(ispdma->dev,
				pfn_to_dma(ispdma->dev, page_to_pfn(pages[i])),
				PAGE_SIZE, DMA_BIDIRECTIONAL);

	ispdma->ispdma_fb_dc.dc_size[index] = 0;

	ispdma_fb_free_pages(ispdma, index);

	return 0;
}

static int ispdma_setup_fb_dc(struct isp_ispdma_device *ispdma,
	struct v4l2_dxoipc_streaming_config *stream_cfg)

{
	int cnt;
	int ret;

	for (cnt = 0; cnt < ispdma->ispdma_cfg_fb.fbcnt; cnt++) {
		ret = ispdma_setup_single_fb_dc(ispdma, cnt);
		if (ret < 0) {
			dev_err(ispdma->dev, "dxo setup_fb_dc failed\n");
			goto out;
		}
		/* set descirptor chain address and size */
		ispdma_set_fb_reg_dc(ispdma, cnt);
	}
	ispdma->ispdma_fb_dc.page_mapped = 1;

	/* enable descriptor chain mode */
	for (cnt = 0; cnt < ispdma->ispdma_cfg_fb.fbcnt; cnt++)
			ispdma_desc_enable(ispdma, cnt, 1);

	return 0;

out:
	while (--cnt >= 0)
		ispdma_release_single_fb_dc(ispdma, cnt);

	return ret;
}

static int ispdma_release_fb_dc(struct isp_ispdma_device *ispdma)
{
	int cnt;

	for (cnt = 0; cnt < ispdma->ispdma_cfg_fb.fbcnt; cnt++)
		ispdma_release_single_fb_dc(ispdma, cnt);

	ispdma->ispdma_fb_dc.page_mapped = 0;
	return 0;
}

static int  ispdma_dump_single_fb(struct isp_ispdma_device *ispdma,
					const char *name, int index)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	unsigned size;
	int i;
	int num_pages;
	struct ispdma_set_fb *cfg_fb = &ispdma->ispdma_cfg_fb;
	struct ispdma_dc_mod *fb_dc = &ispdma->ispdma_fb_dc;

	if (index > 3 || index < 0)
		return -EINVAL;

	size = cfg_fb->size[index];
	num_pages = (size - 1) >> PAGE_SHIFT;

	fp = filp_open(name, O_RDWR | O_CREAT, 0644);
	if (IS_ERR(fp)) {
		dev_err(ispdma->dev, "create file error\n");
		return -1;
	}
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	for (i = 0; i < num_pages; ++i) {
		vfs_write(fp, (char *)page_address(fb_dc->dc_pages[index][i]),
				min_t(u32, PAGE_SIZE, size), &pos);

		pos += PAGE_SIZE;
		size -= PAGE_SIZE;
	}
	filp_close(fp, NULL);
	set_fs(fs);

	return 0;
}

static int __maybe_unused ispdma_dump_fb(struct isp_ispdma_device *ispdma)
{
	int cnt;
	char buffer[20];
	static int times;

	for (cnt = 0; cnt < ispdma->ispdma_cfg_fb.fbcnt; cnt++) {
		snprintf(buffer, sizeof(buffer), "/data/fb%d_%d.yuv",
			times, cnt);
		ispdma_dump_single_fb(ispdma, buffer, cnt);
	}

	times++;

	return 0;
}

static int ispdma_stream_cfg(struct isp_ispdma_device *ispdma,
		struct v4l2_dxoipc_streaming_config *stream_cfg)
{
	u32 reg_clk_en, reg_dma_en, reg_dma_en_old, reg_irq_mask;
	int fb_ch;
	int cnt;
	mutex_lock(&ispdma->ispdma_mutex);

	if (stream_cfg->enable_fbtx != 0) {
		ispdma->ispdma_fb_dc.dc_dw_cnt = 0;

		if (ispdma->ispdma_fb_dc.dc_mode) {
			if (ispdma->ispdma_cfg_fb.virAddr[0] == NULL) {
				ispdma_setup_fb_dc(ispdma, stream_cfg);
			} else {
				ispdma_map_fb_dc(ispdma, stream_cfg);
			}
		} else {
			/* no-descriptor mode*/
			for (cnt = 0; cnt < ispdma->ispdma_cfg_fb.fbcnt; cnt++)
				ispdma_set_fb_reg(ispdma, cnt);
		}
	}

	reg_dma_en = mod_read(ispdma, ISPDMA_DMA_ENA);
	reg_clk_en = mod_read(ispdma, ISPDMA_CLKENA);
	reg_irq_mask = mod_read(ispdma, ISPDMA_IRQMASK);

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
					SET_CLK_EN(reg_clk_en, fb_ch);
					SET_RX_IRQ_MASK(reg_irq_mask, fb_ch);
				}
		}
	}

	if (reg_dma_en != reg_dma_en_old) {
		mod_write(ispdma, ISPDMA_CLKENA, reg_clk_en);
		mod_write(ispdma, ISPDMA_IRQMASK, reg_irq_mask);
		mod_write(ispdma, ISPDMA_DMA_ENA, reg_dma_en);
	}

	if (stream_cfg->enable_fbtx == 0) {
		if (ispdma->ispdma_cfg_fb.virAddr[0] == NULL)
			ispdma_release_fb_dc(ispdma);
		else
			ispdma_unmap_fb_dc(ispdma);
	}

	mutex_unlock(&ispdma->ispdma_mutex);

	if (has_feat_isp_reset()) {
		if (!stream_cfg->enable_fbrx) {
			/* Enable panel path eof intr before do isp reset */
			disp_eofintr_onoff(1);
			atomic_set(&isp_reset_count, 1);
			wait_for_completion_timeout(
					&ispdma->isp_reset_event, HZ >> 1);
		}
	}

	return 0;
}

static int ispdma_config_fb_dc(struct isp_ispdma_device *ispdma,
		struct v4l2_dxoipc_set_fb *cfg_fb)
{
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

	mutex_lock(&ispdma->ispdma_mutex);

	regval = mod_read(ispdma, ISPDMA_DMA_ENA);
	if (regval & 0x3FC) {
		printk(KERN_ERR "ISPDMA is working on now!\n");
		return -EINVAL;
	}

	ispdma->ispdma_cfg_fb.fbcnt = cfg_fb->fbcnt;
	ispdma->ispdma_fb_dc.dc_mode = 1;
	/* Check the FB buffer address must be 16 byte aligned */
	for (cnt = 0; cnt < cfg_fb->fbcnt; cnt++) {
		if ((u32)cfg_fb->virAddr[cnt] & 0xf) {
			printk(KERN_ERR "address not aligned!\n");
			return -EINVAL;
		}
		ispdma->ispdma_cfg_fb.virAddr[cnt] = cfg_fb->virAddr[cnt];
		ispdma->ispdma_cfg_fb.size[cnt] = cfg_fb->size[cnt];
		ispdma->ispdma_cfg_fb.burst_rd[cnt] = cfg_fb->burst_read;
		ispdma->ispdma_cfg_fb.burst_wt[cnt] = cfg_fb->burst_write;
	}

	ispdma->framebuf_count = cfg_fb->fbcnt;

	/*  Restart FB DMA transfer if any */
	mod_write(ispdma, ISPDMA_DMA_ENA, regval);

	mutex_unlock(&ispdma->ispdma_mutex);
	return 0;
}

static int ispdma_config_fb(struct isp_ispdma_device *ispdma,
			struct v4l2_dxoipc_set_fb *cfg_fb)
{
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

	regval = mod_read(ispdma, ISPDMA_DMA_ENA);
	if (regval & 0x3FC) {
		printk(KERN_ERR "ISPDMA is working on now!\n");
		return -EINVAL;
	}

	ispdma->ispdma_fb_dc.dc_mode = 0;
	ispdma->ispdma_cfg_fb.fbcnt = cfg_fb->fbcnt;

	for (cnt = 0; cnt < cfg_fb->fbcnt; cnt++) {
		ispdma->ispdma_cfg_fb.phyAddr[cnt] = cfg_fb->phyAddr[cnt];
		ispdma->ispdma_cfg_fb.size[cnt] = cfg_fb->size[cnt];
		ispdma->ispdma_cfg_fb.burst_rd[cnt] = cfg_fb->burst_read;
		ispdma->ispdma_cfg_fb.burst_wt[cnt] = cfg_fb->burst_write;
	}

	ispdma->framebuf_count = cfg_fb->fbcnt;

	/* Restart FB DMA transfer if any */
	mod_write(ispdma, ISPDMA_DMA_ENA, regval);

	mutex_unlock(&ispdma->ispdma_mutex);
	return 0;
}

static void ispdma_set_inaddr(struct isp_ispdma_device *ispdma,
		struct map_videobuf *buffer)
{
	unsigned int bitsperpixel, width, height, size;

	if ((buffer == NULL) || (buffer->paddr[ISP_BUF_PADDR] == 0))
		return;

	width = ispdma->agent.fmt_hw[ISPDMA_PAD_SINK].width;
	height = ispdma->agent.fmt_hw[ISPDMA_PAD_SINK].height;

	switch (ispdma->agent.fmt_hw[ISPDMA_PAD_SINK].code) {
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		bitsperpixel = 8;
		break;
	case V4L2_MBUS_FMT_UYVY8_1X16:
		bitsperpixel = 16;
		break;
	case V4L2_MBUS_FMT_Y12_1X12:
	case V4L2_MBUS_FMT_YUYV8_1_5X8:
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
	mod_write(ispdma, ISPDMA_INPSDMA_SRCADDR, buffer->paddr[ISP_BUF_PADDR]);
	mod_write(ispdma, ISPDMA_INPSDMA_SRCSZ, size);

	ispdma->dma_mod[ISPDMA_PAD_SINK].regcache[ISPDMA_DST_REG] = buffer;
	return;
}

static void ispdma_set_outaddr(struct isp_ispdma_device *ispdma,
		struct map_videobuf *buffer, dma_addr_t paddr,
		enum ispdma_pad dma_pad)
{
	struct mvisp_device *isp = ispdma->isp;
	unsigned int bitsperpixel, width, height, size;
	dma_addr_t	internal_paddr;

	if (buffer != NULL)
		internal_paddr = buffer->paddr[ISP_BUF_PADDR];
	else if (paddr != 0)
		internal_paddr = paddr;
	else
		return;

	width = ispdma->agent.fmt_hw[dma_pad].width;
	height = ispdma->agent.fmt_hw[dma_pad].height;

	switch (ispdma->agent.fmt_hw[dma_pad].code) {
	case V4L2_MBUS_FMT_SBGGR8_1X8:
		bitsperpixel = 8;
		break;
	case V4L2_MBUS_FMT_UYVY8_1X16:
		bitsperpixel = 16;
		break;
	case V4L2_MBUS_FMT_Y12_1X12:
	case V4L2_MBUS_FMT_YUYV8_1_5X8:
		/*Y component bitsperpixel*/
		bitsperpixel = 8;
		break;
	default:
		bitsperpixel = 0;
		break;
	}

	size = (bitsperpixel * width * height) >> 3;
	dma_pad_write(ispdma, dma_pad, ISPDMA_DSTSZ, size);
	dma_pad_write(ispdma, dma_pad, ISPDMA_DSTADDR, internal_paddr);

	if (isp->cpu_type == MV_PXA988) {
		switch (ispdma->agent.fmt_hw[dma_pad].code) {
		case V4L2_MBUS_FMT_Y12_1X12:
			if (!buffer)
				return;

			internal_paddr = buffer->paddr[ISP_BUF_PADDR_U];
			dma_pad_write(ispdma, dma_pad, ISPDMA_DSTSZ_U, size/4);
			dma_pad_write(ispdma, dma_pad, ISPDMA_DSTADDR_U,
					internal_paddr);

			internal_paddr = buffer->paddr[ISP_BUF_PADDR_V];
			dma_pad_write(ispdma, dma_pad, ISPDMA_DSTSZ_V, size/4);
			dma_pad_write(ispdma, dma_pad, ISPDMA_DSTADDR_V,
					internal_paddr);
			break;
		default:
			break;
		}
	}

	if (buffer != NULL) {
		ispdma->dma_mod[dma_pad].regcache[ISPDMA_DST_REG] = buffer;
	} else
		ispdma->dma_mod[dma_pad].regcache[ISPDMA_DST_REG] = NULL;

	return;
}

static void ispdma_set_dma_addr(struct isp_ispdma_device *ispdma,
		struct map_videobuf *buffer, dma_addr_t paddr,
		enum ispdma_pad dma_pad)
{
	switch (dma_pad) {
	case ISPDMA_PAD_DISP_SRC:
	case ISPDMA_PAD_CODE_SRC:
		ispdma_set_outaddr(ispdma, buffer, paddr, dma_pad);
		break;
	case ISPDMA_PAD_SINK:
		ispdma_set_inaddr(ispdma, buffer);
		break;
	default:
		break;
	}

}
static void ispdma_reset_counter(struct isp_ispdma_device *ispdma)
{
	ispdma->ipc_event_cnt = 0;
	ispdma->dma_event_cnt = 0;
	ispdma->mipi_ovr_cnt = 0;
	ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].eof_cnt = 0;
	ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].eof_max = ~0;	/* max value */
	/* codec port EOF counters don't need to be reset */
	ispdma->input_event_cnt = 0;
	return;
}

static void ispdma_init_params(struct isp_ispdma_device *ispdma)
{
	int i, j;

	ispdma_reset_counter(ispdma);
	ispdma->framebuf_count = 0;
	ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].sched_stop = false;
	ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].sched_stop = false;
	ispdma->dma_mod[ISPDMA_PAD_SINK].sched_stop = false;
	for (i = 0; i < ISPDMA_PADS_NUM; i++) {
		for (j = 0; j < ISPDMA_CACHE_MAX; j++)
			ispdma->dma_mod[i].regcache[j] = NULL;
		ispdma->dma_mod[i].dma_on = DMA_NOT_WORKING;
	}
	return;
}

static int ispdma_isp_func_clk_ops(struct isp_ispdma_device *ispdma,
		struct v4l2_ispdma_isp_func_clk *func_clk);
static void ispdma_start_dma(struct isp_ispdma_device *ispdma,
		enum ispdma_pad dma_pad)
{
	u32 regval;
	unsigned long dma_flags, video_flags;
	struct map_videobuf *buf, **shw;
	struct isp_dma_mod *mod;

	if (dma_pad >= ISPDMA_PADS_NUM)
		return;

	spin_lock_irqsave(&ispdma->dmaflg_lock, dma_flags);

	mod = ispdma->dma_mod + dma_pad;
	switch (dma_pad) {
	case ISPDMA_PAD_DISP_SRC:
	{
		if (mod->dma_on)
			break;
		spin_lock_irqsave(&agent_get_video(&mod->agent)->vb_lock,
					video_flags);
		/* Enable Output DMA here. */
		regval = mod_read(ispdma, ISPDMA_DISP_CTRL);
		regval &= ~0x2;
		mod_write(ispdma, ISPDMA_DISP_CTRL, regval);

		regval = mod_read(ispdma, ISPDMA_CLKENA);
		regval |= 0x300;
		mod_write(ispdma, ISPDMA_CLKENA, regval);

		regval = mod_read(ispdma, ISPDMA_IRQMASK);
		regval |= 0x20801;
		mod_write(ispdma, ISPDMA_IRQMASK, regval);

		regval = (0x1 << 9);
		mod_write(ispdma, ISPDMA_DISP_RAMCTRL, regval);

		regval = mod_read(ispdma, ISPDMA_DMA_ENA);
		regval |= 0x1;
		mod_write(ispdma, ISPDMA_DMA_ENA, regval);

		mod->dma_on = 1;
		buf = mod->regcache[ISPDMA_DST_REG];
		shw = &mod->regcache[ISPDMA_SHADOW_REG];
		if (buf != NULL) {
			*shw = buf;
		}
		spin_unlock_irqrestore(&agent_get_video(&mod->agent)->vb_lock,
					video_flags);
		break;
	}
	case ISPDMA_PAD_CODE_SRC:
	{
		if (mod->dma_on)
			break;
		spin_lock_irqsave(&agent_get_video(&mod->agent)->vb_lock,
					video_flags);
		/* Enable Output DMA here. */
		regval = mod_read(ispdma, ISPDMA_CODEC_CTRL);
		regval &= ~0x2;
		mod_write(ispdma, ISPDMA_CODEC_CTRL, regval);

		regval = mod_read(ispdma, ISPDMA_CLKENA);
		regval |= 0xC00;
		mod_write(ispdma, ISPDMA_CLKENA, regval);

		regval = mod_read(ispdma, ISPDMA_IRQMASK);
		regval |= 0x21002;
		mod_write(ispdma, ISPDMA_IRQMASK, regval);

		regval = (0x1 << 9);
		mod_write(ispdma, ISPDMA_CODEC_RAMCTRL, regval);

		regval = mod_read(ispdma, ISPDMA_DMA_ENA);
		regval |= 0x2;
		mod_write(ispdma, ISPDMA_DMA_ENA, regval);

		mod->dma_on = 1;
		buf = mod->regcache[ISPDMA_DST_REG];
		shw = &mod->regcache[ISPDMA_SHADOW_REG];
		if (buf != NULL) {
			*shw = buf;
		}
		spin_unlock_irqrestore(&agent_get_video(&mod->agent)->vb_lock,
					video_flags);
		break;
	}
	case ISPDMA_PAD_SINK:
	{
		if (mod->dma_on)
			break;
		spin_lock_irqsave(&agent_get_video(&mod->agent)->vb_lock,
					video_flags);
		if (mod->dma_type == ISPDMA_INPUT_MEMORY) {
			regval = mod_read(ispdma, ISPDMA_INPSDMA_CTRL);
			regval |= 0x10;
			mod_write(ispdma, ISPDMA_INPSDMA_CTRL, regval);

			/* Enable Input DMA here. */
			regval = mod_read(ispdma, ISPDMA_CLKENA);
			regval |= 0x3000;
			mod_write(ispdma, ISPDMA_CLKENA, regval);

			regval = mod_read(ispdma, ISPDMA_IRQMASK);
			regval |= 0x20400;
			mod_write(ispdma, ISPDMA_IRQMASK, regval);

			/* Select input as input DMA*/
			regval = mod_read(ispdma, ISPDMA_MAINCTRL);
			regval |= 0x4;
			mod_write(ispdma, ISPDMA_MAINCTRL, regval);

			regval = mod_read(ispdma, ISPDMA_INPSDMA_CTRL);
			regval |= 0x1;
			mod_write(ispdma, ISPDMA_INPSDMA_CTRL, regval);

			mod->dma_on = 1;
		}

		buf = mod->regcache[ISPDMA_DST_REG];
		shw = &mod->regcache[ISPDMA_SHADOW_REG];
		if (buf != NULL) {
			*shw = buf;
		}
		spin_unlock_irqrestore(&agent_get_video(&mod->agent)->vb_lock,
					video_flags);
		break;
	}
	default:
		break;
	}

	spin_unlock_irqrestore(&ispdma->dmaflg_lock, dma_flags);

	return;
}


static void ispdma_stop_dma(struct isp_ispdma_device *ispdma,
		enum ispdma_pad dma_pad)
{
	u32 regval;
	unsigned long dma_flags;
	struct isp_dma_mod *mod;

	if (dma_pad >= ISPDMA_PADS_NUM)
		return;

	spin_lock_irqsave(&ispdma->dmaflg_lock, dma_flags);
	mod = ispdma->dma_mod + dma_pad;

	switch (dma_pad) {
	case ISPDMA_PAD_DISP_SRC:
	{
		if (!mod->dma_on)
			break;

		/* Disable Output DMA here. */
		regval = mod_read(ispdma, ISPDMA_DMA_ENA);
		regval &= ~0x1;
		mod_write(ispdma, ISPDMA_DMA_ENA, regval);

		regval = mod_read(ispdma, ISPDMA_CLKENA);
		regval &= ~0x300;
		regval |= 0x200;
		mod_write(ispdma, ISPDMA_CLKENA, regval);

		regval = mod_read(ispdma, ISPDMA_IRQMASK);
		regval &= ~0x801;
		mod_write(ispdma, ISPDMA_IRQMASK, regval);

		mod->dma_on = 0;
		ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].sched_stop = false;

		dev_warn(ispdma->dev, "ispdma stop disp\n");
		break;
	}
	case ISPDMA_PAD_CODE_SRC:
	{
		if (!mod->dma_on)
			break;

		/* Disable Output DMA here. */
		regval = mod_read(ispdma, ISPDMA_DMA_ENA);
		regval &= ~0x2;
		mod_write(ispdma, ISPDMA_DMA_ENA, regval);

		regval = mod_read(ispdma, ISPDMA_CLKENA);
		regval &= ~0xC00;
		regval |= 0x800;
		mod_write(ispdma, ISPDMA_CLKENA, regval);

		regval = mod_read(ispdma, ISPDMA_IRQMASK);
		regval &= ~0x1002;
		mod_write(ispdma, ISPDMA_IRQMASK, regval);

		mod->dma_on = 0;
		ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].sched_stop = false;

		dev_warn(ispdma->dev, "ispdma stop codec\n");
		break;
	}
	case ISPDMA_PAD_SINK:
	{
		if (!mod->dma_on)
			break;

		if (ispdma->dma_mod[ISPDMA_PAD_SINK].dma_type ==
			ISPDMA_INPUT_MEMORY) {
			/* Disable Input DMA here. */
			regval = mod_read(ispdma, ISPDMA_INPSDMA_CTRL);
			regval &= ~0x1;
			mod_write(ispdma, ISPDMA_INPSDMA_CTRL, regval);

			regval = mod_read(ispdma, ISPDMA_CLKENA);
			regval &= ~0x3000;
			regval |= 0x2000;
			mod_write(ispdma, ISPDMA_CLKENA, regval);

			regval = mod_read(ispdma, ISPDMA_IRQMASK);
			regval &= ~0x400;
			mod_write(ispdma, ISPDMA_IRQMASK, regval);
		}

		mod->dma_on = 0;
		ispdma->dma_mod[ISPDMA_PAD_SINK].sched_stop = false;

		dev_warn(ispdma->dev, "ispdma stop input\n");
		break;
	}
	default:
		break;
	}

	if ((!ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].dma_on)
		&& (!ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].dma_on)
		&& (!ispdma->dma_mod[ISPDMA_PAD_SINK].dma_on)) {
		regval = mod_read(ispdma, ISPDMA_IRQMASK);
		regval &= ~0x20000;
		mod_write(ispdma, ISPDMA_IRQMASK, regval);
	}

	spin_unlock_irqrestore(&ispdma->dmaflg_lock, dma_flags);

	return;
}

static void ispdma_config_csi_input(struct isp_ispdma_device *ispdma)
{
	u32 regval;

	regval = mod_read(ispdma, ISPDMA_MAINCTRL);
	regval |= 1 << 3;  /* Set input as CSI2 */
	mod_write(ispdma, ISPDMA_MAINCTRL, regval);
	return;
}

static int load_dummy_buffer(struct isp_ispdma_device *ispdma,
			enum ispdma_pad dma_pad, bool *stop_dma)
{
	struct mvisp_device *isp = ispdma->isp;

	switch (dma_pad) {
	case ISPDMA_PAD_DISP_SRC:
	case ISPDMA_PAD_CODE_SRC:
		if (isp->dummy_paddr == 0) {
			ispdma->dma_mod[dma_pad].sched_stop = true;
			*stop_dma = true;
			dev_warn(ispdma->dev,
				"isp dma pad %d stop [no buffer]\n", dma_pad);
		} else {
			ispdma_set_dma_addr(ispdma, NULL,
					isp->dummy_paddr, dma_pad);
			*stop_dma = false;
		}
		break;
	case ISPDMA_PAD_SINK:
		ispdma->dma_mod[dma_pad].sched_stop = true;
		*stop_dma = false;
		dev_warn(ispdma->dev,
			"isp input dma stop [no buffer]\n");
		break;
	default:
		break;
	}

	return 0;
}

static int ispdma_isr_load_buffer(struct isp_ispdma_device *ispdma,
			enum ispdma_pad dma_pad, bool *stop_dma)
{
	struct map_videobuf *buffer;
	bool needdummy = false;

	if (dma_pad >= ISPDMA_PADS_NUM)
		return -EINVAL;

	buffer = map_vnode_xchg_buffer(
			agent_get_video(&ispdma->dma_mod[dma_pad].agent),
			true, false);

	if (buffer) {
		ispdma_set_dma_addr(ispdma, buffer, 0, dma_pad);
		*stop_dma = false;
	} else {
		needdummy = true;
		*stop_dma = true;
	}

	if (needdummy == true)
		load_dummy_buffer(ispdma, dma_pad, stop_dma);

	return 0;
}

static void ispdma_isr_buffer(struct isp_ispdma_device *ispdma,
			enum ispdma_pad dma_pad)
{
	bool stop_dma = true;

	ispdma_isr_load_buffer(ispdma, dma_pad, &stop_dma);
	switch (ispdma->state) {
	case ISP_PIPELINE_STREAM_CONTINUOUS:
		if (stop_dma)
			ispdma_stop_dma(ispdma, dma_pad);
		break;
	case ISP_PIPELINE_STREAM_STOPPED:
		ispdma_stop_dma(ispdma, dma_pad);
		break;
	default:
		break;
	}

	return;
}

static void ispdma_out_handler(struct isp_ispdma_device *ispdma,
		enum ispdma_pad dma_pad)
{
	struct map_videobuf *buf = NULL;
	struct isp_dma_mod *mod = ispdma->dma_mod + dma_pad;

	buf = mod->regcache[ISPDMA_SHADOW_REG];
	mod->regcache[ISPDMA_SHADOW_REG] = mod->regcache[ISPDMA_DST_REG];

	mod->eof_cnt++;
	switch (dma_pad) {
	case ISPDMA_PAD_DISP_SRC:
	case ISPDMA_PAD_SINK:
		break;
	case ISPDMA_PAD_CODE_SRC:
		if (mod->eof_cnt < mod->eof_max)
			return;
		mod->eof_cnt = 0;
		break;
	default:
		BUG_ON(1);
	}

	if (mod->sched_stop == true) {
		dev_warn(ispdma->dev, "dma #%d schedule stops\n", dma_pad);
		ispdma_stop_dma(ispdma, dma_pad);
	}

	ispdma_isr_buffer(ispdma, dma_pad);
	ispdma->mipi_ovr_cnt = 0;

	/* Enable MIPI overrun IRQ for error handling */
	mod_setbit(ispdma, ISPDMA_IRQMASK, 0x20000);

	return;
}

static void ispdma_input_handler(struct isp_ispdma_device *ispdma,
	struct map_vnode *vnode)
{
	struct map_videobuf *buf = NULL;
	struct isp_dma_mod *mod = ispdma->dma_mod + ISPDMA_PAD_SINK;

	buf = mod->regcache[ISPDMA_SHADOW_REG];
	mod->regcache[ISPDMA_SHADOW_REG] =
		mod->regcache[ISPDMA_DST_REG];

	if (mod->sched_stop == true) {
		dev_warn(ispdma->dev, "input dma schedule stops\n");
		ispdma_stop_dma(ispdma, ISPDMA_PAD_SINK);
	}

	ispdma_isr_buffer(ispdma, ISPDMA_PAD_SINK);

	return;
}

static void ctx_adjust_buffers(struct map_vnode *vnode)
{
	struct map_videobuf *buf;
	struct list_head temp_list;
	unsigned long video_flags;

	INIT_LIST_HEAD(&temp_list);

	spin_lock_irqsave(&vnode->vb_lock, video_flags);

	while (!list_empty(&vnode->idle_buf)) {
		buf = list_first_entry(&vnode->idle_buf,
			struct map_videobuf, hook);
		list_del(&buf->hook);
		vnode->idle_buf_cnt--;
		list_add_tail(&buf->hook, &temp_list);
	}

	while (!list_empty(&vnode->busy_buf)) {
		buf = list_first_entry(&vnode->busy_buf,
			struct map_videobuf, hook);
		list_del(&buf->hook);
		vnode->busy_buf_cnt--;
		list_add_tail(&buf->hook, &vnode->idle_buf);
		vnode->idle_buf_cnt++;
	}

	while (!list_empty(&temp_list)) {
		buf = list_first_entry(&temp_list, struct map_videobuf, hook);
		list_del(&buf->hook);
		list_add_tail(&buf->hook, &vnode->idle_buf);
		vnode->idle_buf_cnt++;
	}

	spin_unlock_irqrestore(&vnode->vb_lock, video_flags);
	return;
}

static void ispdma_context_save(struct isp_ispdma_device *ispdma)
{
	int cnt;

	for (cnt = 0; cnt < ISPDMA_INPSDMA_MAX_CTX; cnt++) {
		ispdma_reg_list[cnt].val =
			mod_read(ispdma,
				ispdma_reg_list[cnt].reg);
		if (cnt == ISPDMA_DMA_ENA_CTX) {
			mod_write(ispdma, ISPDMA_DMA_ENA, 0);

			ctx_adjust_buffers(agent_get_video(&ispdma->\
					dma_mod[ISPDMA_PAD_DISP_SRC].agent));
			ctx_adjust_buffers(agent_get_video(&ispdma->\
					dma_mod[ISPDMA_PAD_CODE_SRC].agent));
		} else if (cnt == ISPDMA_INPSDMA_CTRL_CTX) {
			mod_write(ispdma, ISPDMA_INPSDMA_CTRL, 0);

			ctx_adjust_buffers(agent_get_video(&ispdma->\
					dma_mod[ISPDMA_PAD_SINK].agent));
		} else if (cnt == ISPDMA_IRQMASK_CTX) {
			mod_write(ispdma, ISPDMA_IRQMASK, 0);
		}
	}

	return;
}

static void ispdma_context_restore(struct isp_ispdma_device *ispdma)
{
	int cnt;
	unsigned long dma_flags;

	spin_lock_irqsave(&ispdma->dmaflg_lock, dma_flags);
	ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].dma_on = 0;
	ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].dma_on = 0;
	ispdma->dma_mod[ISPDMA_PAD_SINK].dma_on = 0;
	spin_unlock_irqrestore(&ispdma->dmaflg_lock, dma_flags);

	for (cnt = 0; cnt < ISPDMA_INPSDMA_MAX_CTX; cnt++) {
		if (cnt == ISPDMA_INPSDMA_CTRL_CTX) {
			/*Restore input settings*/
			mod_write(ispdma, ispdma_reg_list[cnt].reg,
				(ispdma_reg_list[cnt].val & ~0x1));

			/*Restart input DMA, IRQ mask*/
			/* ispdma_try_restart_dma(ispdma); */
		} else if (cnt == ISPDMA_DMA_ENA_CTX) {
			/*Restart all the DMAs except disp/codec*/
			mod_write(ispdma, ispdma_reg_list[cnt].reg,
				(ispdma_reg_list[cnt].val & ~0x3));

			/*Restart all disp/codec DMAs, IRQ masks*/
			/* ispdma_try_restart_dma(ispdma); */
		} else if (cnt == ISPDMA_IRQMASK_CTX) {
			/*Only restore FB IRQ masks here*/
			mod_write(ispdma, ispdma_reg_list[cnt].reg,
				(ispdma_reg_list[cnt].val & ~0x21C03));
		} else {
			mod_write(ispdma, ispdma_reg_list[cnt].reg,
				ispdma_reg_list[cnt].val);
		}
	}

	return;
}

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
	int bufcnt;

	switch (mode_cfg->mode) {
	case ISPVIDEO_NORMAL_CAPTURE:
		bufcnt = MIN_DRV_BUF;
		break;
	case ISPVIDEO_STILL_CAPTURE:
		bufcnt = 0;
		break;
	default:
		return -EINVAL;
	}

	switch (mode_cfg->port) {
	case ISPDMA_PORT_CODEC:
		agent_get_video(&ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].agent)->\
			min_buf_cnt = bufcnt;
		break;
	case ISPDMA_PORT_DISPLAY:
		agent_get_video(&ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].agent)->\
			min_buf_cnt = bufcnt;
		break;
	default:
		break;
	}

	return 0;
}

static int ispdma_config_codec(struct isp_ispdma_device *ispdma,
		struct v4l2_dxoipc_config_codec *cfg_codec)
{
	u32 regval;
	unsigned long dma_irq_flags;

	if (cfg_codec == NULL)
		return -EINVAL;

	if (cfg_codec->vbsize < 0)
		return -EINVAL;

	mutex_lock(&ispdma->ispdma_mutex);

	regval = mod_read(ispdma, ISPDMA_CODEC_CTRL);
	if ((cfg_codec->vbnum > 1) && (cfg_codec->vbnum < 5)) {
		regval &= ~(0x3 << 6);
		regval |=  (cfg_codec->vbnum - 1) << 0x6;
		regval |= 0x8;
		spin_lock_irqsave(&ispdma->dma_irq_lock, dma_irq_flags);
		ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].eof_max = cfg_codec->vbnum;
		ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].eof_cnt = 0;
		spin_unlock_irqrestore(&ispdma->dma_irq_lock, dma_irq_flags);
		mod_write(ispdma, ISPDMA_CODEC_VBSZ, cfg_codec->vbsize);
	} else {
		regval &= ~(0xC8);
		spin_lock_irqsave(&ispdma->dma_irq_lock, dma_irq_flags);
		ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].eof_max = 0;
		ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].eof_cnt = 0;
		spin_unlock_irqrestore(&ispdma->dma_irq_lock, dma_irq_flags);
	}

	if (cfg_codec->dma_burst_size == 64
		|| cfg_codec->dma_burst_size == 128
		|| cfg_codec->dma_burst_size == 256) {
		regval |= ((cfg_codec->dma_burst_size >> 3) & 0x30);
	} else
		regval &= ~(0x30);
	mod_write(ispdma, ISPDMA_CODEC_CTRL, regval);

	mutex_unlock(&ispdma->ispdma_mutex);
	return 0;
}

static int ispdma_copy_timeinfo(
		struct v4l2_ispdma_timeinfo *dest,
		const struct v4l2_ispdma_timeinfo *src)
{
	if (NULL == dest || NULL == src)
		return -EINVAL;

	dest->sec = src->sec;
	dest->usec = src->usec;

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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;
	struct mvisp_device *isp = ispdma->isp;
	u32 regval;

	mutex_lock(&ispdma->ispdma_mutex);

	switch (enable) {
	case ISP_PIPELINE_STREAM_CONTINUOUS:
		if (ispdma->stream_refcnt++ == 0) {
			if ((ispdma->dma_mod[ISPDMA_PAD_SINK].dma_type
				== ISPDMA_INPUT_CCIC_1)
				&& (isp->sensor_connected == true)) {
				regval = mod_read(ispdma, ISPDMA_MAINCTRL);
				regval |= 1 << 24;
				mod_write(ispdma, ISPDMA_MAINCTRL, regval);
			}
				ispdma->state = enable;
		}
		break;
	case ISP_PIPELINE_STREAM_STOPPED:
		if (--ispdma->stream_refcnt == 0) {
			if (ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].dma_type
				== ISPDMA_OUTPUT_MEMORY)
				ispdma_stop_dma(ispdma, ISPDMA_PAD_DISP_SRC);

			if (ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].dma_type
				== ISPDMA_OUTPUT_MEMORY)
				ispdma_stop_dma(ispdma, ISPDMA_PAD_CODE_SRC);

			if (ispdma->dma_mod[ISPDMA_PAD_SINK].dma_type
				== ISPDMA_INPUT_MEMORY)
				ispdma_stop_dma(ispdma, ISPDMA_PAD_SINK);
			else if ((ispdma->dma_mod[ISPDMA_PAD_SINK].dma_type
				== ISPDMA_INPUT_CCIC_1)
				&& (isp->sensor_connected == true)) {
				regval = mod_read(ispdma, ISPDMA_MAINCTRL);
				regval &= ~(1 << 24);
				mod_write(ispdma, ISPDMA_MAINCTRL, regval);
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

static int ispdma_isp_func_clk_ops(struct isp_ispdma_device *ispdma,
		struct v4l2_ispdma_isp_func_clk *fclk)
{
	int i = 0 , j = 0;
	struct mvisp_platform_data *pdata;
	unsigned int cur_clk_rate;

	if (!fclk || !ispdma || !ispdma->agent.hw.clock[0])
		return -EINVAL;

	pdata = ispdma->isp->pdata;

	switch (fclk->ops) {
	case GET_CURR_CLK:
		fclk->fclk_mhz =
			clk_get_rate(ispdma->agent.hw.clock[0]) / 1000000;
		break;
	case GET_AVAILABLE_CLK:
		while (i < pdata->isp_clk_rate_num) {
			if (pdata->isp_clk_rate[i] >= ispdma->isp->min_fclk_mhz)
				fclk->avail_clk_rate[j++] =
					pdata->isp_clk_rate[i];
			i++;
		}

		fclk->avail_clk_rate_num = j;
		break;
	case SET_CLK:
		cur_clk_rate = clk_get_rate(
				ispdma->agent.hw.clock[0]) / 1000000;
		if (cur_clk_rate == fclk->fclk_mhz)
			return 0;

		while (i < pdata->isp_clk_rate_num) {
			if (fclk->fclk_mhz == pdata->isp_clk_rate[i])
				break;
			i++;
		}
		if (i >= pdata->isp_clk_rate_num)
			return -EINVAL;

		clk_set_rate(ispdma->agent.hw.clock[0],
				fclk->fclk_mhz * 1000000);
		break;
	default:
		break;
	}

	return 0;
}

void isp_reset_clock(void)
{
	struct isp_ispdma_device *ispdma = g_ispdma;

	if (!ispdma)
		goto out;

	if (has_feat_isp_reset()) {
		if (atomic_read(&isp_reset_count)) {
			if (ispdma->mvisp_reset) {
				ispdma_context_save(ispdma);
				ispdma->mvisp_reset(
					(struct v4l2_ispdma_reset *) ispdma);
				ispdma_context_restore(ispdma);
			}
			complete_all(&ispdma->isp_reset_event);
			/* Disable panel path eof intr after do isp reset */
				disp_eofintr_onoff(0);
		}
	}
out:
	atomic_set(&isp_reset_count, 0);
}
EXPORT_SYMBOL(isp_reset_clock);

static int ispdma_lcd_dma_ops(int *val)
{
	if (!val)
		return -EINVAL;

	if (has_feat_isp_reset()) {
		if (*val)
			disp_onoff(1);
		else
			disp_onoff(0);
	}

	return 0;
}

static long ispdma_ioctl(struct v4l2_subdev *sd
			, unsigned int cmd, void *arg)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;
	int ret;

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
		if (ispdma->mvisp_reset) {
			ispdma_context_save(ispdma);
			ret = ispdma->mvisp_reset(
			(struct v4l2_ispdma_reset *) arg);
			ispdma_context_restore(ispdma);
		} else
			ret = -EINVAL;
		break;
	case VIDIOC_PRIVATE_ISPDMA_GETDELTA:
		ret = ispdma_get_time(
				(struct v4l2_ispdma_timeinfo *) arg);
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
	case VIDIOC_PRIVATE_ISPDMA_ISP_FUNC_CLK_OPS:
		ret = ispdma_isp_func_clk_ops(ispdma,
				(struct v4l2_ispdma_isp_func_clk *) arg);
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
		return &ispdma->agent.fmt_hw[pad];
}

/* ispdma format descriptions */
static const struct pad_formats ispdma_input_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_Y12_1X12, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YUYV8_1_5X8,	V4L2_COLORSPACE_JPEG},
};

static const struct pad_formats ispdma_disp_out_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_Y12_1X12, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YUYV8_1_5X8,	V4L2_COLORSPACE_JPEG},
};

static const struct pad_formats ispdma_codec_out_fmts[] = {
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_SBGGR8_1X8, V4L2_COLORSPACE_SRGB},
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_Y12_1X12, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YUYV8_1_5X8,	V4L2_COLORSPACE_JPEG},
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
		if (ispdma->dma_mod[ISPDMA_PAD_SINK].dma_type
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;

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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;
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
	struct v4l2_mbus_framefmt *format = &ispdma->agent.fmt_hw[pad];
	struct mvisp_device *isp = ispdma->isp;
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
	case V4L2_MBUS_FMT_YUYV8_1_5X8:
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
		mod_write(ispdma, ISPDMA_INPSDMA_PIXSZ, regval);
		/* Set input BPP */
		regval = mod_read(ispdma, ISPDMA_INPSDMA_CTRL);
		regval |= in_bpp << 1;
		mod_write(ispdma, ISPDMA_INPSDMA_CTRL, regval);

		break;
	case ISPDMA_PAD_CODE_SRC:
		regval = pitch & ISPDMA_MAX_CODEC_PITCH;
		mod_write(ispdma, ISPDMA_CODEC_PITCH, regval);

		if (isp->cpu_type == MV_PXA988) {
			/*set output format*/
			regval = mod_read(ispdma, ISPDMA_CODEC_CTRL_1);
			regval |= cfg_out_fmt << 4;
			mod_write(ispdma, ISPDMA_CODEC_CTRL_1, regval);
		}
		break;
	case ISPDMA_PAD_DISP_SRC:
		regval = pitch & ISPDMA_MAX_DISP_PITCH;
		mod_write(ispdma, ISPDMA_DISP_PITCH, regval);

		if (isp->cpu_type == MV_PXA988) {
			/*set output format*/
			regval = mod_read(ispdma, ISPDMA_DISP_CTRL_1);
			regval |= cfg_out_fmt << 4;
			mod_write(ispdma, ISPDMA_DISP_CTRL_1, regval);
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;
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
		return ret;
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;
	struct mvisp_device *isp = ispdma->isp;

	mvisp_get(isp);
	hw_tune_power(&ispdma->agent.hw, ~0);

	if (ispdma->dma_mod[ISPDMA_PAD_SINK].dma_type == ISPDMA_INPUT_CCIC_1)
		ispdma_config_csi_input(ispdma);

	ispdma_init_params(ispdma);
	return ispdma_init_formats(sd, fh);
}

static int ispdma_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;
	struct mvisp_device *isp = ispdma->isp;
	u32 regval;

	regval = mod_read(ispdma, ISPDMA_DMA_ENA);
	regval &= ~(FBRX3DMAENA | FBRX2DMAENA
				| FBRX1DMAENA | FBRX0DMAENA
				| FBTX3DMAENA | FBTX2DMAENA
				| FBTX1DMAENA | FBTX0DMAENA);
	mod_write(ispdma, ISPDMA_DMA_ENA, regval);

	mutex_lock(&ispdma->ispdma_mutex);
	if (ispdma->state != ISP_PIPELINE_STREAM_STOPPED) {
		ispdma->stream_refcnt = 1;
		mutex_unlock(&ispdma->ispdma_mutex);
		ispdma_set_stream(sd, 0);
	} else
		mutex_unlock(&ispdma->ispdma_mutex);

	if (ispdma->ispdma_fb_dc.page_mapped) {
		if (ispdma->ispdma_cfg_fb.virAddr[0] == NULL)
				ispdma_release_fb_dc(ispdma);
			else
				ispdma_unmap_fb_dc(ispdma);
	}

	hw_tune_power(&ispdma->agent.hw, 0);
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
static const struct v4l2_subdev_ops ispdma_sd_ops = {
	.core = &ispdma_v4l2_core_ops,
	.video = &ispdma_v4l2_video_ops,
	.pad = &ispdma_v4l2_pad_ops,
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops ispdma_internal_ops = {
	.open = ispdma_open,
	.close = ispdma_close,
};

static int ispdma_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_ispdma_device *ispdma = agent->drv_priv;
	struct mvisp_device *isp = ispdma->isp;
	struct isp_dma_mod *mod;

	switch (local->index | media_entity_type(remote->entity)) {
	case ISPDMA_PAD_SINK | MEDIA_ENT_T_DEVNODE:
		mod = ispdma->dma_mod + ISPDMA_PAD_SINK;
		/* read from memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (mod->dma_type != ISPDMA_INPUT_NONE)
				return -EBUSY;
			mod->dma_type = ISPDMA_INPUT_MEMORY;
		} else {
			if (mod->dma_type == ISPDMA_INPUT_MEMORY)
				mod->dma_type = ISPDMA_INPUT_NONE;
		}
		break;
	case ISPDMA_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		mod = ispdma->dma_mod + ISPDMA_PAD_SINK;
		/* read from ccic */
		if (isp->sensor_connected == false)
			return -EINVAL;
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (mod->dma_type != ISPDMA_INPUT_NONE)
				return -EBUSY;
			mod->dma_type = ISPDMA_INPUT_CCIC_1;
			ispdma_config_csi_input(ispdma);
		} else {
			if (mod->dma_type == ISPDMA_INPUT_CCIC_1)
				mod->dma_type = ISPDMA_INPUT_NONE;
		}
		break;
	case ISPDMA_PAD_CODE_SRC | MEDIA_ENT_T_DEVNODE:
		mod = ispdma->dma_mod + ISPDMA_PAD_CODE_SRC;
		/* write to memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (mod->dma_type == ISPDMA_OUTPUT_MEMORY)
				return -EBUSY;
			mod->dma_type = ISPDMA_OUTPUT_MEMORY;
		} else {
			mod->dma_type = ISPDMA_OUTPUT_NONE;
		}
		break;

	case ISPDMA_PAD_DISP_SRC | MEDIA_ENT_T_DEVNODE:
		mod = ispdma->dma_mod + ISPDMA_PAD_DISP_SRC;
		/* write to memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (mod->dma_type == ISPDMA_OUTPUT_MEMORY)
				return -EBUSY;
			mod->dma_type = ISPDMA_OUTPUT_MEMORY;
		} else {
			mod->dma_type = ISPDMA_OUTPUT_NONE;
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

int mv_ispdma_register_entities(struct isp_ispdma_device *ispdma,
	struct v4l2_device *vdev)
{
	int ret;
	/* Add W/R link to fool CE into beleive that DxO core is linked
	 * to video nodes directly */
	ret = media_entity_create_link(
		&agent_get_video(&ispdma->dma_mod[ISPDMA_PAD_SINK].agent)->\
		vdev.entity, 0,
		&ispdma->agent.subdev.entity, ISPDMA_PAD_SINK, 0);
	if (ret < 0)
		goto error;

	ret = media_entity_create_link(
		&ispdma->agent.subdev.entity, ISPDMA_PAD_CODE_SRC,
		&agent_get_video(&ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].agent)->\
		vdev.entity, 0, 0);
	if (ret < 0)
		goto error;

	ret = media_entity_create_link(
		&ispdma->agent.subdev.entity, ISPDMA_PAD_DISP_SRC,
		&agent_get_video(&ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].agent)->\
		vdev.entity, 0, 0);
	if (ret < 0)
		goto error;
	return 0;

error:
	return ret;
}

void mv_ispdma_cleanup(struct isp_ispdma_device *ispdma)
{
}

static irqreturn_t dxo_wrapper_irq_handler(int irq, void *data)
{
	struct hw_event *event = data;
	struct hw_agent *agent = event->owner;
	struct isp_ispdma_device *ispdma = container_of(agent,
						struct map_agent, hw)->drv_priv;
	u32 irqstatus, reg_dma_en;
	unsigned long dma_irq_flags;

	spin_lock_irqsave(&ispdma->dma_irq_lock, dma_irq_flags);
	irqstatus = mod_read(ispdma, ISPDMA_IRQSTAT);
	mod_write(ispdma, ISPDMA_IRQSTAT, irqstatus);

	ispdma->dma_event_cnt++;
	if (irqstatus & CSI2_BRIDEG) {
		ispdma->mipi_ovr_cnt++;
		mod_clrbit(ispdma, ISPDMA_IRQMASK, 0x20000);
	}

	if (irqstatus & DISP_PS_EOF)
		ispdma_get_time(&ispdma->dma_timeinfo.disp_ps_timeinfo);

	if (irqstatus & DISP_DMA_EOF) {
		ispdma_get_time(&ispdma->dma_timeinfo.disp_dma_timeinfo);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(isp->mcd_root_display.mcd, MCD_DMA, MCD_DMA_EOF, 1);
#endif
		ispdma_out_handler(ispdma, ISPDMA_PAD_DISP_SRC);
	}

	if (irqstatus & CODEC_PS_EOF)
		ispdma_get_time(&ispdma->dma_timeinfo.codec_ps_timeinfo);

	if (irqstatus & CODEC_DMA_EOF) {
		ispdma_get_time(&ispdma->dma_timeinfo.codec_dma_timeinfo);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(isp->mcd_root_codec.mcd, MCD_DMA, MCD_DMA_EOF, 1);
#endif
		ispdma_out_handler(ispdma, ISPDMA_PAD_CODE_SRC);
	}

	if (irqstatus & INPUT_DMA_EOF)
		ispdma_input_handler(ispdma, agent_get_video(
				&ispdma->dma_mod[ISPDMA_PAD_SINK].agent));

	/* And then send a event to all listening agent */
	event->type = irq;
	event->msg = (void *)irqstatus;
	hw_event_dispatch(agent, event);

	if (irqstatus & FBTX0_DMA_EOF) {
		if (ispdma->framebuf_count >= 1) {
			reg_dma_en = mod_read(ispdma, ISPDMA_DMA_ENA);
			reg_dma_en |= 0x40;
			mod_write(ispdma, ISPDMA_DMA_ENA, reg_dma_en);
		}
	}
	if (irqstatus & FBTX1_DMA_EOF) {
		if (ispdma->framebuf_count >= 2) {
			reg_dma_en = mod_read(ispdma, ISPDMA_DMA_ENA);
			reg_dma_en |= 0x80;
			mod_write(ispdma, ISPDMA_DMA_ENA, reg_dma_en);
		}
	}
	if (irqstatus & FBTX2_DMA_EOF) {
		if (ispdma->framebuf_count >= 3) {
			reg_dma_en = mod_read(ispdma, ISPDMA_DMA_ENA);
			reg_dma_en |= 0x100;
			mod_write(ispdma, ISPDMA_DMA_ENA, reg_dma_en);
		}
	}
	if (irqstatus & FBTX3_DMA_EOF) {
		if (ispdma->framebuf_count >= 4) {
			reg_dma_en = mod_read(ispdma, ISPDMA_DMA_ENA);
			reg_dma_en |= 0x200;
			mod_write(ispdma, ISPDMA_DMA_ENA, reg_dma_en);
		}
	}
	spin_unlock_irqrestore(&ispdma->dma_irq_lock, dma_irq_flags);

	return IRQ_HANDLED;
}

static int ispdma_hw_init(struct hw_agent *agent)
{
	printk(KERN_INFO "dxo-core: map_hw init done\n");
	return 0;
}

static void ispdma_hw_clean(struct hw_agent *agent)
{

}

static int ispdma_hw_open(struct hw_agent *agent)
{
	return 0;
}

static void ispdma_hw_close(struct hw_agent *agent)
{

}

static int ispdma_hw_power(struct hw_agent *agent, int level)
{
	struct isp_ispdma_device *dxo_core = container_of(agent,
		struct isp_ispdma_device, agent.hw);
	if (level)
		mod_write(dxo_core, ISP_IRQMASK, 0x1);
	else
		mod_write(dxo_core, ISP_IRQMASK, 0x0);
	return 0;
}

static int ispdma_hw_qos(struct hw_agent *agent, int qos_level)
{
	/* power off */
	/* platform: change function clock rate */
	/* power on */
	return 0;
}

static int ispdma_hw_clk(struct hw_agent *agent, int level)
{

	struct isp_ispdma_device *dxo_core = container_of(agent,
					struct isp_ispdma_device, agent.hw);
	struct clk *fclk = agent->clock[0];
	u32 ori_rate;
	u32 req_rate;

	if (!dxo_core->isp)
		return 0;

	req_rate = dxo_core->isp->min_fclk_mhz * 1000000;
	if (level) {
		ori_rate = clk_get_rate(fclk);
		if (req_rate && (ori_rate != req_rate))
			clk_set_rate(fclk, req_rate);
	}

	return 0;
}

struct hw_agent_ops ispdma_hw_ops = {
	.init		= ispdma_hw_init,
	.clean		= ispdma_hw_clean,
	.set_power	= ispdma_hw_power,
	.open		= ispdma_hw_open,
	.close		= ispdma_hw_close,
	.set_qos	= ispdma_hw_qos,
	.set_clock	= ispdma_hw_clk,
};

static int ispdma_map_add(struct map_agent *agent)
{
	struct isp_ispdma_device *ispdma = agent->drv_priv;
	struct v4l2_subdev *sd = &ispdma->agent.subdev;
	struct media_pad *pads = ispdma->agent.pads;
	struct media_entity *me = &sd->entity;
	struct mvisp_platform_data *pdata = ispdma->isp->pdata;
	int i, ret, cnt;
	u32 ispdma_fb_dc_handle;
	void *dma_coherent;

	g_ispdma = ispdma;
	spin_lock_init(&ispdma->ipc_irq_lock);
	spin_lock_init(&ispdma->dma_irq_lock);
	spin_lock_init(&ispdma->dmaflg_lock);
	init_completion(&ispdma->ipc_event);
	init_completion(&ispdma->isp_reset_event);
	mutex_init(&ispdma->ispdma_mutex);

	ispdma->state = ISP_PIPELINE_STREAM_STOPPED;
	ispdma->stream_refcnt = 0;

	v4l2_subdev_init(sd, &ispdma_sd_ops);
	sd->internal_ops = &ispdma_internal_ops;
	sd->grp_id = GID_AGENT_GROUP;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	me->ops = &ispdma_media_ops;
	if (ispdma->agent.ctrl_cnt)
		sd->ctrl_handler = &ispdma->agent.ctrl_handler;

	pads[ISPDMA_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
	pads[ISPDMA_PAD_CODE_SRC].flags = MEDIA_PAD_FL_SOURCE;
	pads[ISPDMA_PAD_DISP_SRC].flags = MEDIA_PAD_FL_SOURCE;
	agent->pad_vdev = -1;
	agent->pads_cnt	= ISPDMA_PADS_NUM;

	ispdma_init_params(ispdma);
	ispdma->mvisp_reset = pdata->mvisp_reset;

	for (cnt = 0; cnt < 4; cnt++) {
		dma_coherent = dma_alloc_coherent(ispdma->dev,
				sizeof(u32)*FB_DESC_ARRAY_SIZE,
				&ispdma_fb_dc_handle, GFP_KERNEL);
		if (!dma_coherent) {
			printk(KERN_NOTICE "dma_alloc_coherent got error\n");
			ret = -ENOMEM;
			goto out;
		}
		ispdma->ispdma_fb_dc.dc_vir[cnt] = (u32 *)dma_coherent;
		ispdma->ispdma_fb_dc.dc_phy_handle[cnt] = ispdma_fb_dc_handle;
	}

	ispdma->ispdma_fb_dc.dc_max_size = FB_DESC_ARRAY_SIZE;
	ispdma->ispdma_fb_dc.dc_dw_cnt = 0;
	return 0;

out:
	if (ret)
		for (i = cnt - 1; i >= 0; i--)
			dma_free_coherent(ispdma->dev,
				sizeof(u32)*FB_DESC_ARRAY_SIZE,
				ispdma->ispdma_fb_dc.dc_vir[i],
				ispdma->ispdma_fb_dc.dc_phy_handle[i]);

	return ret;
}

static void ispdma_map_remove(struct map_agent *agent)
{
}

static int ispdma_map_open(struct map_agent *agent)
{
	return 0;
}

static void ispdma_map_close(struct map_agent *agent)
{
}

static int ispdma_map_start(struct map_agent *agent)
{
	return 0;
}

static void ispdma_map_stop(struct map_agent *agent)
{
}

struct map_agent_ops ispdma_map_ops = {
	.add		= ispdma_map_add,
	.remove		= ispdma_map_remove,
	.open		= ispdma_map_open,
	.close		= ispdma_map_close,
	.start		= ispdma_map_start,
	.stop		= ispdma_map_stop,
};

/****************************** DxO core module *******************************/


static int dmax_video_qbuf_notify(struct map_vnode *vnode)
{
	struct isp_dma_mod *mod = video_get_agent(vnode)->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;
	unsigned long dma_flags;
	struct map_videobuf *buffer = NULL;

	spin_lock_irqsave(&vnode->vb_lock, dma_flags);
	if (vnode->busy_buf_cnt < vnode->min_buf_cnt) {
		buffer = map_vnode_xchg_buffer(vnode, false, false);
		ispdma_set_dma_addr(ispdma, buffer, 0, mod->pad_id);
	}
	spin_unlock_irqrestore(&vnode->vb_lock, dma_flags);

	mutex_lock(&vnode->st_lock);
	if (vnode->state == MAP_VNODE_STREAM && !mod->dma_on) {
		ispdma_start_dma(ispdma, mod->pad_id);
		buffer = map_vnode_xchg_buffer(vnode, false, false);
		if (buffer)
			ispdma_set_dma_addr(ispdma, buffer, 0, mod->pad_id);
	}
	mutex_unlock(&vnode->st_lock);
	return 0;
}

static int dmax_video_stream_on_notify(struct map_vnode *vnode)
{
	struct isp_dma_mod *mod = video_get_agent(vnode)->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	if (!mod->dma_on) {
		struct map_videobuf *buffer =
			map_vnode_xchg_buffer(vnode, false, false);
		if (buffer)
			ispdma_set_dma_addr(ispdma, buffer, 0, mod->pad_id);
		else
			return -EPERM;
		ispdma_start_dma(ispdma, mod->pad_id);
		buffer = map_vnode_xchg_buffer(vnode, false, false);
		if (buffer)
			ispdma_set_dma_addr(ispdma, buffer, 0, mod->pad_id);
	}
	return 0;
}

static int dmax_video_stream_off_notify(struct map_vnode *vnode)
{
	struct isp_dma_mod *mod = video_get_agent(vnode)->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	ispdma_stop_dma(ispdma, mod->pad_id);

	return 0;
}

static const struct vnode_ops dmax_video_ops = {
	.qbuf_notify		= dmax_video_qbuf_notify,
	.stream_on_notify	= dmax_video_stream_on_notify,
	.stream_off_notify	= dmax_video_stream_off_notify,
};

static int dmax_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_dma_mod *mod = agent->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	switch (local->index | media_entity_type(remote->entity)) {
	case ISPDMA_PAD_SINK | MEDIA_ENT_T_DEVNODE:
		mod = ispdma->dma_mod + ISPDMA_PAD_SINK;
		/* read from memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (mod->dma_type != ISPDMA_INPUT_NONE)
				return -EBUSY;
			mod->dma_type = ISPDMA_INPUT_MEMORY;
		} else {
			if (mod->dma_type == ISPDMA_INPUT_MEMORY)
				mod->dma_type = ISPDMA_INPUT_NONE;
		}
		break;
	case ISPDMA_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		mod = ispdma->dma_mod + ISPDMA_PAD_SINK;
		/* read from ccic */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (mod->dma_type != ISPDMA_INPUT_NONE)
				return -EBUSY;
			mod->dma_type = ISPDMA_INPUT_CCIC_1;
			ispdma_config_csi_input(ispdma);
		} else {
			if (mod->dma_type == ISPDMA_INPUT_CCIC_1)
				mod->dma_type = ISPDMA_INPUT_NONE;
		}
		break;
	case ISPDMA_PAD_CODE_SRC | MEDIA_ENT_T_DEVNODE:
		mod = ispdma->dma_mod + ISPDMA_PAD_CODE_SRC;
		/* write to memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (mod->dma_type == ISPDMA_OUTPUT_MEMORY)
				return -EBUSY;
			mod->dma_type = ISPDMA_OUTPUT_MEMORY;
		} else {
			mod->dma_type = ISPDMA_OUTPUT_NONE;
		}
		break;

	case ISPDMA_PAD_DISP_SRC | MEDIA_ENT_T_DEVNODE:
		mod = ispdma->dma_mod + ISPDMA_PAD_DISP_SRC;
		/* write to memory */
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (mod->dma_type == ISPDMA_OUTPUT_MEMORY)
				return -EBUSY;
			mod->dma_type = ISPDMA_OUTPUT_MEMORY;
		} else {
			mod->dma_type = ISPDMA_OUTPUT_NONE;
		}
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static const struct media_entity_operations dmax_media_ops = {
	.link_setup = dmax_link_setup,
};

static long dmax_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_dma_mod *mod = agent->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	return ispdma_ioctl(&ispdma->agent.subdev, cmd, arg);
}

static int dmax_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_dma_mod *mod = agent->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	return ispdma_set_stream(&ispdma->agent.subdev, enable);
}

static int dmax_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_dma_mod *mod = agent->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	return ispdma_enum_mbus_code(&ispdma->agent.subdev, fh, code);
}

static int dmax_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_dma_mod *mod = agent->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	return ispdma_enum_frame_size(&ispdma->agent.subdev, fh, fse);
}

static int dmax_get_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_dma_mod *mod = agent->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	return ispdma_get_format(&ispdma->agent.subdev, fh, fmt);
}

static int dmax_set_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_dma_mod *mod = agent->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	return ispdma_set_format(&ispdma->agent.subdev, fh, fmt);
}

static int dmax_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_dma_mod *mod = agent->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	return ispdma_open(&ispdma->agent.subdev, fh);
}

static int dmax_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct isp_dma_mod *mod = agent->drv_priv;
	struct isp_ispdma_device *ispdma = mod->core;

	return ispdma_close(&ispdma->agent.subdev, fh);
}

/* subdev core operations */
static const struct v4l2_subdev_core_ops dmax_v4l2_core_ops = {
	.ioctl	= dmax_ioctl,
};

/* subdev video operations */
static const struct v4l2_subdev_video_ops dmax_v4l2_video_ops = {
	.s_stream	= dmax_set_stream,
};

/* subdev pad operations */
static const struct v4l2_subdev_pad_ops dmax_v4l2_pad_ops = {
	.enum_mbus_code		= dmax_enum_mbus_code,
	.enum_frame_size	= dmax_enum_frame_size,
	.get_fmt		= dmax_get_format,
	.set_fmt		= dmax_set_format,
};

/* subdev operations */
static const struct v4l2_subdev_ops dmax_sd_ops = {
	.core	= &dmax_v4l2_core_ops,
	.video	= &dmax_v4l2_video_ops,
	.pad	= &dmax_v4l2_pad_ops,
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops dmax_internal_ops = {
	.open	= dmax_open,
	.close	= dmax_close,
};

/****************************** DxO display DMA *******************************/
static int dxo_dmad_isr(struct hw_agent *dmad_hw, struct hw_event *irq_event)
{
	int irq_num = (int)irq_event->msg;

	if (irq_num == 0)
		return 0;
	printk(KERN_INFO "agent '%s' received event '%s'\n",
		dmad_hw->name, irq_event->name);
	return 0;
}

static void dxo_dmad_hw_clean(struct hw_agent *agent)
{
	printk(KERN_INFO "dxo-dmad: map_hw will be disposed\n");
}

static int dxo_dmad_hw_init(struct hw_agent *agent)
{
	printk(KERN_INFO "dxo-dmad: map_hw init done\n");
	return 0;
}

static int dxo_dmad_hw_open(struct hw_agent *agent)
{
	struct hw_event *event = hw_local_event_find(agent, DXO_IRQ_NAME);
	return hw_event_subscribe(event, agent, &dxo_dmad_isr);
}

static void dxo_dmad_hw_close(struct hw_agent *agent)
{
	struct hw_event *event = hw_local_event_find(agent, DXO_IRQ_NAME);
	hw_event_unsubscribe(event, agent);
}

struct hw_agent_ops dxo_dmad_hw_ops = {
	.init		= dxo_dmad_hw_init,
	.clean		= dxo_dmad_hw_clean,
	.open		= dxo_dmad_hw_open,
	.close		= dxo_dmad_hw_close,
};

static int dxo_dmad_map_add(struct map_agent *agent)
{
	struct isp_dma_mod *dmad = agent->drv_priv;
	struct v4l2_subdev *sd = &agent->subdev;
	struct media_pad *pads = agent->pads;

	dmad->dma_type	= ISPDMA_OUTPUT_NONE;
	dmad->offset	= ISPDMA_DISPL_BASE;
	dmad->pad_id	= ISPDMA_PAD_DISP_SRC;

	agent->vops		= &dmax_video_ops;
	agent->video_type	= ISP_VIDEO_DISPLAY;

	v4l2_subdev_init(sd, &dmax_sd_ops);
	sd->internal_ops = &dmax_internal_ops;
	sd->grp_id = GID_AGENT_GROUP;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->entity.ops = &dmax_media_ops;
	if (dmad->agent.ctrl_cnt)
		sd->ctrl_handler = &dmad->agent.ctrl_handler;

	pads[0].flags = MEDIA_PAD_FL_SINK;
	pads[1].flags = MEDIA_PAD_FL_SOURCE;
	agent->pad_vdev = 1;
	agent->pads_cnt	= 2;

	return 0;
}

struct map_agent_ops dxo_dmad_map_ops = {
	.add		= dxo_dmad_map_add,
};

/******************************* DxO codec DMA ********************************/
static int dxo_dmac_isr(struct hw_agent *dmac_hw, struct hw_event *irq_event)
{
	int irq_num = (int)irq_event->msg;

	if (irq_num == 0)
		return 0;
	printk(KERN_INFO "agent '%s' received event '%s'\n",
		dmac_hw->name, irq_event->name);
	return 0;
}

static void dxo_dmac_hw_clean(struct hw_agent *agent)
{
	printk(KERN_INFO "dxo-dmac: map_hw will be disposed\n");
}

static int dxo_dmac_hw_init(struct hw_agent *agent)
{
	printk(KERN_INFO "dxo-dmac: map_hw init done\n");
	return 0;
}

static int dxo_dmac_hw_open(struct hw_agent *agent)
{
	struct hw_event *event = hw_local_event_find(agent, DXO_IRQ_NAME);
	return hw_event_subscribe(event, agent, &dxo_dmac_isr);
}

static void dxo_dmac_hw_close(struct hw_agent *agent)
{
	struct hw_event *event = hw_local_event_find(agent, DXO_IRQ_NAME);
	hw_event_unsubscribe(event, agent);
}

struct hw_agent_ops dxo_dmac_hw_ops = {
	.init		= dxo_dmac_hw_init,
	.clean		= dxo_dmac_hw_clean,
	.open		= dxo_dmac_hw_open,
	.close		= dxo_dmac_hw_close,
};

static int dxo_dmac_map_add(struct map_agent *agent)
{
	struct isp_dma_mod *dmac = agent->drv_priv;
	struct v4l2_subdev *sd = &agent->subdev;
	struct media_pad *pads = agent->pads;

	dmac->dma_type	= ISPDMA_OUTPUT_NONE;
	dmac->offset	= ISPDMA_CODEC_BASE;
	dmac->pad_id	= ISPDMA_PAD_CODE_SRC;

	agent->vops		= &dmax_video_ops;
	agent->video_type	= ISP_VIDEO_CODEC;

	v4l2_subdev_init(sd, &dmax_sd_ops);
	sd->internal_ops = &dmax_internal_ops;
	sd->grp_id = GID_AGENT_GROUP;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->entity.ops = &dmax_media_ops;
	if (dmac->agent.ctrl_cnt)
		sd->ctrl_handler = &dmac->agent.ctrl_handler;

	pads[0].flags = MEDIA_PAD_FL_SINK;
	pads[1].flags = MEDIA_PAD_FL_SOURCE;
	agent->pad_vdev = 1;
	agent->pads_cnt	= 2;

	return 0;
}

struct map_agent_ops dxo_dmac_map_ops = {
	.add		= dxo_dmac_map_add,
};

/******************************* DxO input DMA ********************************/
static int dxo_dmai_isr(struct hw_agent *dmai_hw, struct hw_event *irq_event)
{
	int irq_num = (int)irq_event->msg;

	if (irq_num == 0)
		return 0;
	printk(KERN_INFO "agent '%s' received event '%s'\n",
		dmai_hw->name, irq_event->name);
	return 0;
}

static void dxo_dmai_hw_clean(struct hw_agent *agent)
{
	printk(KERN_INFO "dxo-dmai: map_hw will be disposed\n");
}

static int dxo_dmai_hw_init(struct hw_agent *agent)
{
	printk(KERN_INFO "dxo-dmai: map_hw init done\n");
	return 0;
}

static int dxo_dmai_hw_open(struct hw_agent *agent)
{
	struct hw_event *event = hw_local_event_find(agent, DXO_IRQ_NAME);
	return hw_event_subscribe(event, agent, &dxo_dmai_isr);
}

static void dxo_dmai_hw_close(struct hw_agent *agent)
{
	struct hw_event *event = hw_local_event_find(agent, DXO_IRQ_NAME);
	hw_event_unsubscribe(event, agent);
}

struct hw_agent_ops dxo_dmai_hw_ops = {
	.init		= dxo_dmai_hw_init,
	.clean		= dxo_dmai_hw_clean,
	.open		= dxo_dmai_hw_open,
	.close		= dxo_dmai_hw_close,
};

static int dxo_dmai_map_add(struct map_agent *agent)
{
	struct isp_dma_mod *dmai = agent->drv_priv;
	struct v4l2_subdev *sd = &agent->subdev;
	struct media_pad *pads = agent->pads;

	dmai->dma_type	= ISPDMA_INPUT_NONE;
	dmai->offset	= 0;
	dmai->pad_id	= ISPDMA_PAD_SINK;

	agent->vops		= &dmax_video_ops;
	agent->video_type	= ISP_VIDEO_INPUT;

	v4l2_subdev_init(sd, &dmax_sd_ops);
	sd->internal_ops = &dmax_internal_ops;
	sd->grp_id = GID_AGENT_GROUP;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->entity.ops = &dmax_media_ops;
	if (dmai->agent.ctrl_cnt)
		sd->ctrl_handler = &dmai->agent.ctrl_handler;

	pads[0].flags = MEDIA_PAD_FL_SOURCE;
	pads[1].flags = MEDIA_PAD_FL_SINK;
	agent->pad_vdev = 1;
	agent->pads_cnt	= 2;

	return 0;
}

struct map_agent_ops dxo_dmai_map_ops = {
	.add		= dxo_dmai_map_add,
};

/******************************** CCIC IP Core ********************************/

static struct hw_res_req ispdma_req[] = {
	{MAP_RES_MEM,	0,	0},
	{MAP_RES_IRQ},
	{MAP_RES_CLK},
	{MAP_RES_END}
};

static struct hw_res_req dxo_dmai_res[] = {
	{MAP_RES_MEM},
	{MAP_RES_CLK},
	{MAP_RES_END}
};

static struct hw_res_req dxo_dmad_res[] = {
	{MAP_RES_MEM},
	{MAP_RES_CLK},
	{MAP_RES_END}
};

static struct hw_res_req dxo_dmac_res[] = {
	{MAP_RES_MEM},
	{MAP_RES_CLK},
	{MAP_RES_END}
};

static int ispdma_suspend(struct device *dev)
{
	return 0;
}

static int ispdma_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops ispdma_pm = {
	.suspend	= ispdma_suspend,
	.resume		= ispdma_resume,
};

static int ispdma_probe(struct platform_device *pdev)
{
	struct mvisp_platform_data *pdata = pdev->dev.platform_data;
	struct isp_ispdma_device *ispdma;
	struct resource *res, clk = {.flags = MAP_RES_CLK, .name = "ISP-CLK"};
	union agent_id pdev_mask = {
		.dev_type = PCAM_IP_DXO,
		.dev_id = pdev->dev.id,
		.mod_type = 0xFF,
		.mod_id = 0xFF,
	};
	struct hw_agent *hw;
	struct map_agent *agent;
	int ret, i;

	if (pdata == NULL)
		return -EINVAL;

	ispdma = devm_kzalloc(&pdev->dev,
				sizeof(struct isp_ispdma_device), GFP_KERNEL);
	if (!ispdma) {
		dev_err(&pdev->dev, "could not allocate memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ispdma);
	ispdma->dev = &pdev->dev;

	/* register all platform resource to map manager */
	for (i = 0; ; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res == NULL)
			break;
		ret = plat_resrc_register(&pdev->dev, res, "dxo-wrapper-mem",
			pdev_mask, i, NULL, NULL);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed register mem resource %s",
				res->name);
			return ret;
		}
	}

	/* get irqs */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource");
		return -ENXIO;
	}
	ispdma->event_irq.name = DXO_IRQ_NAME;
	ispdma->event_irq.owner_name = ISPDMA_NAME;
	memset(&ispdma->event_irq.dispatch_list, 0, \
		sizeof(ispdma->event_irq.dispatch_list));
	/* We expect DxO wrapper only take one kind of irq: the DMA irq.
	 * so the handler is the same for all, but resource id is device id */
	ret = plat_resrc_register(&pdev->dev, res, DXO_IRQ_NAME, pdev_mask, 0,
				/* irq handler */
				&dxo_wrapper_irq_handler,
				/* irq context*/
				&ispdma->event_irq);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed register irq resource %s",
			res->name);
		return ret;
	}

	/* get clocks */
	ret = plat_resrc_register(&pdev->dev, &clk, NULL, pdev_mask,
					0, NULL, NULL);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed register clock resource %s",
			res->name);
		return ret;
	}

	agent = &ispdma->agent;
	hw = &agent->hw;
	hw->id.dev_type = PCAM_IP_DXO;
	hw->id.dev_id = pdev->dev.id;
	hw->id.mod_type = MAP_AGENT_NORMAL;
	hw->id.mod_id = AGENT_DXO_CORE;
	hw->name	= ISPDMA_NAME;
	hw->req_list	= ispdma_req;
	hw->ops		= &ispdma_hw_ops;
	agent->ops	= &ispdma_map_ops;
	agent->drv_priv	= ispdma;
	plat_agent_register(hw);

	agent = &ispdma->dma_mod[ISPDMA_PAD_DISP_SRC].agent;
	hw = &agent->hw;
	hw->id.dev_type = PCAM_IP_DXO;
	hw->id.dev_id = pdev->dev.id;
	hw->id.mod_type = MAP_AGENT_DMA_OUT;
	hw->id.mod_id = AGENT_DXO_DMAD;
	hw->name	= DXO_DMAD_NAME;
	hw->req_list	= dxo_dmad_res;
	hw->ops		= &dxo_dmad_hw_ops;
	agent->ops	= &dxo_dmad_map_ops;
	agent->drv_priv	= &ispdma->dma_mod[ISPDMA_PAD_DISP_SRC];
	plat_agent_register(hw);

	agent = &ispdma->dma_mod[ISPDMA_PAD_CODE_SRC].agent;
	hw = &agent->hw;
	hw->id.dev_type = PCAM_IP_DXO;
	hw->id.dev_id = pdev->dev.id;
	hw->id.mod_type = MAP_AGENT_DMA_OUT;
	hw->id.mod_id = AGENT_DXO_DMAC;
	hw->name	= DXO_DMAC_NAME;
	hw->req_list	= dxo_dmac_res;
	hw->ops		= &dxo_dmac_hw_ops;
	agent->ops	= &dxo_dmac_map_ops;
	agent->drv_priv	= &ispdma->dma_mod[ISPDMA_PAD_CODE_SRC];
	plat_agent_register(hw);

	agent = &ispdma->dma_mod[ISPDMA_PAD_SINK].agent;
	hw = &agent->hw;
	hw->id.dev_type = PCAM_IP_DXO;
	hw->id.dev_id = pdev->dev.id;
	hw->id.mod_type = MAP_AGENT_DMA_IN;
	hw->id.mod_id = AGENT_DXO_DMAI;
	hw->name	= DXO_DMAI_NAME;
	hw->req_list	= dxo_dmai_res;
	hw->ops		= &dxo_dmai_hw_ops;
	agent->ops	= &dxo_dmai_map_ops;
	agent->drv_priv	= &ispdma->dma_mod[ISPDMA_PAD_SINK];
	plat_agent_register(hw);

	return 0;
}

static int ispdma_remove(struct platform_device *pdev)
{
	struct isp_ispdma_device *ispdma = platform_get_drvdata(pdev);
	int cnt;

	for (cnt = 0; cnt < 4; cnt++)
		dma_free_coherent(&(pdev->dev),
			sizeof(u32)*FB_DESC_ARRAY_SIZE,
			ispdma->ispdma_fb_dc.dc_vir[cnt],
			ispdma->ispdma_fb_dc.dc_phy_handle[cnt]);

	devm_kfree(ispdma->dev, ispdma);
	return 0;
}

struct platform_driver ispdma_driver = {
	.driver = {
		.name	= MC_DXODMA_DRV_NAME,
		.pm	= &ispdma_pm,
	},
	.probe	= ispdma_probe,
	.remove	= __devexit_p(ispdma_remove),
};

module_platform_driver(ispdma_driver);
