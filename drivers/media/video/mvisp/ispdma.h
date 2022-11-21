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

#include "ispvideo.h"

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

#define ISPDMA_PAD_SINK					0
#define ISPDMA_PAD_CODE_SRC			1
#define ISPDMA_PAD_DISP_SRC				2
#define ISPDMA_PADS_NUM					3

#define DMA_NOT_WORKING				0x0
#define DMA_INPUT_WORKING			0x1
#define DMA_DISP_WORKING			0x2
#define DMA_CODEC_WORKING			0x4

enum ispdma_reg_cache {
	ISPDMA_DST_REG = 0,
	ISPDMA_SHADOW_REG = 1,
	ISPDMA_CACHE_MAX = 2,
};

struct isp_ispdma_device {
	struct v4l2_subdev			subdev;
	struct media_pad			pads[ISPDMA_PADS_NUM];
	struct v4l2_mbus_framefmt	formats[ISPDMA_PADS_NUM];
	struct isp_video_buffer
			*regcache[ISPDMA_PADS_NUM][ISPDMA_CACHE_MAX];
	struct v4l2_ctrl_handler	ctrls;

	enum ispdma_input_entity	input;
	enum ispdma_output_entity	disp_out;
	enum ispdma_output_entity	codec_out;
	struct isp_video			vd_in;
	struct isp_video			vd_disp_out;
	struct isp_video			vd_codec_out;

	enum isp_pipeline_stream_state	state;
	spinlock_t						ipc_irq_lock;
	spinlock_t						dma_irq_lock;
	struct mutex					ispdma_mutex;

	int					stream_refcnt;
	struct completion	ipc_event;
	struct completion	isp_reset_event;

	unsigned int		ipc_event_cnt;
	unsigned int		dma_event_cnt;
	unsigned int		disp_mipi_ovr_cnt;
	unsigned int		codec_mipi_ovr_cnt;
	unsigned int		disp_eof_cnt;
	unsigned int		input_event_cnt;

	unsigned long		framebuf_count;
	spinlock_t			dmaflg_lock;
	unsigned long		dma_working_flag;

	unsigned int		codec_band_cnt;
	unsigned int		codec_band_max;

	bool				sched_stop_disp;
	bool				sched_stop_codec;
	bool				sched_stop_input;

	struct v4l2_ispdma_timeinfo tickinfo;
	struct v4l2_ispdma_dma_timeinfo dma_timeinfo;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	struct mcd_dma		mcd_dma_display;/* to track display DMA */
	struct mcd_dma		mcd_dma_codec;	/* to track codec DMA */
#endif
	int (*mvisp_reset)(void *param);
};

struct mvisp_device;


int mv_ispdma_init(struct mvisp_device *isp);
void mv_ispdma_cleanup(struct mvisp_device *isp);

int mv_ispdma_register_entities(struct isp_ispdma_device *wrp,
				       struct v4l2_device *vdev);
void mv_ispdma_unregister_entities(struct isp_ispdma_device *wrp);

void mv_ispdma_isr_frame_sync(struct isp_ispdma_device *wrp);

void mv_ispdma_ipc_isr_handler(struct isp_ispdma_device *wrp);

void mv_ispdma_dma_isr_handler(struct isp_ispdma_device *wrp,
	unsigned long irq_status);

void mv_ispdma_restore_context(struct mvisp_device *isp);

#endif	/* ISP_DMA_H */
