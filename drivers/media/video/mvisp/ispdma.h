/*
 * ispdma.h
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

#ifndef ISP_DMA_H
#define ISP_DMA_H

#include <linux/mvisp.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/map_vnode.h>

#include <media/ispvideo.h>

enum ispdma_input_entity {
	ISPDMA_INPUT_NONE,
	ISPDMA_INPUT_CCIC_1,
	ISPDMA_INPUT_CCIC_2,
	ISPDMA_INPUT_MEMORY,
};

enum ispdma_output_entity {
	ISPDMA_OUTPUT_NONE,
	ISPDMA_OUTPUT_MEMORY,
};

enum ispdma_reg_context_name {
	ISPDMA_IRQMASK_CTX = 0,
	ISPDMA_DMA_ENA_CTX,
	ISPDMA_INPSDMA_CTRL_CTX,
	ISPDMA_CLKENA_CTX,
	ISPDMA_MAINCTRL_CTX,
	ISPDMA_INSZ_CTX,
	ISPDMA_FBTX0_SDCA_CTX,
	ISPDMA_FBTX0_DCSZ_CTX,
	ISPDMA_FBTX0_CTRL_CTX,
	ISPDMA_FBTX0_DSTSZ_CTX,
	ISPDMA_FBTX0_DSTADDR_CTX,
	ISPDMA_FBTX0_TMR_CTX,
	ISPDMA_FBTX0_RAMCTRL_CTX,
	ISPDMA_FBRX0_SDCA_CTX,
	ISPDMA_FBRX0_DCSZ_CTX,
	ISPDMA_FBRX0_CTRL_CTX,
	ISPDMA_FBRX0_TMR_CTX,
	ISPDMA_FBRX0_RAMCTRL_CTX,
	ISPDMA_FBRX0_STAT_CTX,
	ISPDMA_FBTX1_SDCA_CTX,
	ISPDMA_FBTX1_DCSZ_CTX,
	ISPDMA_FBTX1_CTRL_CTX,
	ISPDMA_FBTX1_DSTSZ_CTX,
	ISPDMA_FBTX1_DSTADDR_CTX,
	ISPDMA_FBTX1_TMR_CTX,
	ISPDMA_FBTX1_RAMCTRL_CTX,
	ISPDMA_FBRX1_SDCA_CTX,
	ISPDMA_FBRX1_DCSZ_CTX,
	ISPDMA_FBRX1_CTRL_CTX,
	ISPDMA_FBRX1_TMR_CTX,
	ISPDMA_FBRX1_RAMCTRL_CTX,
	ISPDMA_FBRX1_STAT_CTX,
	ISPDMA_FBTX2_SDCA_CTX,
	ISPDMA_FBTX2_DCSZ_CTX,
	ISPDMA_FBTX2_CTRL_CTX,
	ISPDMA_FBTX2_DSTSZ_CTX,
	ISPDMA_FBTX2_DSTADDR_CTX,
	ISPDMA_FBTX2_TMR_CTX,
	ISPDMA_FBTX2_RAMCTRL_CTX,
	ISPDMA_FBRX2_SDCA_CTX,
	ISPDMA_FBRX2_DCSZ_CTX,
	ISPDMA_FBRX2_CTRL_CTX,
	ISPDMA_FBRX2_TMR_CTX,
	ISPDMA_FBRX2_RAMCTRL_CTX,
	ISPDMA_FBRX2_STAT_CTX,
	ISPDMA_FBTX3_SDCA_CTX,
	ISPDMA_FBTX3_DCSZ_CTX,
	ISPDMA_FBTX3_CTRL_CTX,
	ISPDMA_FBTX3_DSTSZ_CTX,
	ISPDMA_FBTX3_DSTADDR_CTX,
	ISPDMA_FBTX3_TMR_CTX,
	ISPDMA_FBTX3_RAMCTRL_CTX,
	ISPDMA_FBRX3_SDCA_CTX,
	ISPDMA_FBRX3_DCSZ_CTX,
	ISPDMA_FBRX3_CTRL_CTX,
	ISPDMA_FBRX3_TMR_CTX,
	ISPDMA_FBRX3_RAMCTRL_CTX,
	ISPDMA_FBRX3_STAT_CTX,
	ISPDMA_DISP_CTRL_CTX,
	ISPDMA_DISP_CTRL_1_CTX,
	ISPDMA_DISP_DSTSZ_CTX,
	ISPDMA_DISP_DSTADDR_CTX,
	ISPDMA_DISP_DSTSZ_U_CTX,
	ISPDMA_DISP_DSTADDR_U_CTX,
	ISPDMA_DISP_DSTSZ_V_CTX,
	ISPDMA_DISP_DSTADDR_V_CTX,
	ISPDMA_DISP_RAMCTRL_CTX,
	ISPDMA_DISP_PITCH_CTX,
	ISPDMA_CODEC_CTRL_CTX,
	ISPDMA_CODEC_CTRL_1_CTX,
	ISPDMA_CODEC_DSTSZ_CTX,
	ISPDMA_CODEC_DSTADDR_CTX,
	ISPDMA_CODEC_RAMCTRL_CTX,
	ISPDMA_CODEC_DSTSZ_U_CTX,
	ISPDMA_CODEC_DSTADDR_U_CTX,
	ISPDMA_CODEC_DSTSZ_V_CTX,
	ISPDMA_CODEC_DSTADDR_V_CTX,
	ISPDMA_CODEC_STAT_CTX,
	ISPDMA_CODEC_PITCH_CTX,
	ISPDMA_CODEC_VBSZ_CTX,
	ISPDMA_INPSDMA_SRCADDR_CTX,
	ISPDMA_INPSDMA_SRCSZ_CTX,
	ISPDMA_INPSDMA_PIXSZ_CTX,
	ISP_IRQMASK_CTX,
	ISPDMA_INPSDMA_MAX_CTX,
};

enum ispdma_pad {
	ISPDMA_PAD_SINK = 0,
	ISPDMA_PAD_CODE_SRC,
	ISPDMA_PAD_DISP_SRC,
	ISPDMA_PADS_NUM,
};

enum {
	CORE_PADI_DPHY,
	CORE_PADO_DMAC,
	CORE_PADO_DMAD,
	CORE_PADI_DMAI,
	CORE_PADO_VDEVC,
	CORE_PADO_VDEVD,
	CORE_PADI_VDEVI,
	CORE_PAD_END,

	DMAC_PADI_CORE	= 0,
	DMAC_PADO_VDEV,
	DMAC_PAD_END,

	DMAD_PADI_CORE	= 0,
	DMAD_PADO_VDEV,
	DMAD_PAD_END,

	DMAI_PADI_VDEV,
	DMAI_PADO_CORE,
	DMAI_PAD_END,
};

enum {
	AGENT_DXO_CORE	= 0,
	AGENT_DXO_DMAI,	/* DMA memory input port */
	AGENT_DXO_DMAD,	/* DMA display output port */
	AGENT_DXO_DMAC,	/* DMA codec output port */
	AGENT_DXO_END,
};

#define DMA_NOT_WORKING				0x0
#define DMA_INPUT_WORKING			0x1
#define DMA_DISP_WORKING			0x2
#define DMA_CODEC_WORKING			0x4

enum ispdma_reg_cache {
	ISPDMA_DST_REG = 0,
	ISPDMA_SHADOW_REG = 1,
	ISPDMA_CACHE_MAX = 2,
};

struct ispdma_set_fb {
	void *virAddr[4];
	int phyAddr[4];
	int size[4];
	int burst_rd[4];
	int burst_wt[4];
	int fbcnt;
};

struct ispdma_dc_mod {
	u32	*dc_vir[4];
	u32	dc_phy_handle[4];
	int	dc_size[4];
	struct page **dc_pages[4];
	int	dc_dw_cnt; /* descriptor array size counted in double word*/
	int dc_mode;
	int dc_max_size;
	int page_mapped:1;
};

struct isp_dma_mod {
	struct map_videobuf		*regcache[ISPDMA_CACHE_MAX];
	int				dma_type;
	u32				offset;
	bool				sched_stop;
	u32				eof_cnt;
	u32				eof_max;
	enum ispdma_pad			pad_id;
	unsigned int			dma_on:1;
	struct map_agent		agent;
	struct isp_ispdma_device	*core;
};

struct isp_ispdma_device {
	struct v4l2_ctrl_handler	ctrls;
	struct mvisp_device		*isp;
	struct isp_dma_mod		dma_mod[ISPDMA_PADS_NUM];

	struct device			*dev;
	struct map_agent		agent;
	struct hw_event			event_irq;

	enum isp_pipeline_stream_state	state;
	spinlock_t			ipc_irq_lock; /* protect tickinfo */
	struct v4l2_ispdma_timeinfo	tickinfo;
	spinlock_t			dma_irq_lock; /* protect following 2 */
	/* should be move to each dma port later */
	struct v4l2_ispdma_dma_timeinfo	dma_timeinfo;
	unsigned int			mipi_ovr_cnt;

	struct mutex			ispdma_mutex;

	int			stream_refcnt;
	struct completion	ipc_event;
	struct completion	isp_reset_event;

	unsigned int		ipc_event_cnt;
	unsigned int		dma_event_cnt;
	unsigned int		input_event_cnt;

	unsigned long		framebuf_count;
	spinlock_t		dmaflg_lock;

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	struct mcd_dma		mcd_dma_display;/* to track display DMA */
	struct mcd_dma		mcd_dma_codec;	/* to track codec DMA */
#endif
	int (*mvisp_reset)(void *param);
	struct ispdma_dc_mod	ispdma_fb_dc;
	struct ispdma_set_fb	ispdma_cfg_fb;
};

struct mvisp_device;


int mv_ispdma_init(struct isp_ispdma_device *ispdma);
void mv_ispdma_cleanup(struct isp_ispdma_device *ispdma);

int mv_ispdma_register_entities(struct isp_ispdma_device *wrp,
				       struct v4l2_device *vdev);
void mv_ispdma_unregister_entities(struct isp_ispdma_device *wrp);

void mv_ispdma_isr_frame_sync(struct isp_ispdma_device *wrp);

void mv_ispdma_ipc_isr_handler(struct isp_ispdma_device *wrp);

void mv_ispdma_dma_isr_handler(struct isp_ispdma_device *wrp,
	unsigned long irq_status);

void mv_ispdma_restore_context(struct mvisp_device *isp);

#if 0
static inline u32 mod_read(struct isp_ispdma_device *dma, const u32 addr)
{
	return readl(dma->reg_base + addr);
}

static inline void mod_write(struct isp_ispdma_device *dma,
				const u32 addr, const u32 value)
{
	writel(value, dma->reg_base + addr);
}

static inline void mod_setbit(struct isp_ispdma_device *dma,
				const u32 addr, const u32 mask)
{
	u32 val = readl(dma->reg_base + addr);
	val |= mask;
	writel(val, dma->reg_base + addr);
}

static inline void mod_clrbit(struct isp_ispdma_device *dma,
				const u32 addr, const u32 mask)
{
	u32 val = readl(dma->reg_base + addr);
	val &= ~(mask);
	writel(val, dma->reg_base + addr);
}
#else
#define mod_read(dxo, addr)		agent_read(&(dxo->agent.hw), addr)
#define mod_write(dxo, addr, val)	agent_write(&(dxo->agent.hw), addr, val)
#define mod_setbit(dxo, addr, mask)	agent_set(&(dxo->agent.hw), addr, mask)
#define mod_clrbit(dxo, addr, mask)	agent_clr(&(dxo->agent.hw), addr, mask)
#endif

#define dma_pad_write(ispdma, pad, reg, val) \
	mod_write(ispdma, ispdma->dma_mod[pad].offset + reg, val);

#endif	/* ISP_DMA_H */
