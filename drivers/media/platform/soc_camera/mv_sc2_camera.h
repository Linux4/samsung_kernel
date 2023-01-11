/*
 * Copyright (C) 2011 Marvell International Ltd.
 *               Libin Yang <lbyang@marvell.com>
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MV_SC2_CAMERA_H
#define __MV_SC2_CAMERA_H
#include <media/soc_camera.h>

#include <media/mv_sc2.h>


struct mcam_frame_stat {
	int frames;
	int singles;
	int delivered;
};

enum msc2_buffer_mode {
	B_DMA_CONTIG = 0,
	B_DMA_SG = 1,
};

enum mcam_state {
	S_IDLE,		/* Just hanging around */
	S_STREAMING,	/* Streaming data */
	S_BUFWAIT	/* streaming requested but no buffers yet */
};

struct mv_camera_dev {
	struct device *dev;
	struct soc_camera_host soc_host;
	struct soc_camera_device *icd;
	struct platform_device *pdev;
	spinlock_t mcam_lock;
	struct mutex s_mutex;
	enum msc2_buffer_mode buffer_mode;
	int id;
	struct ccic_dma_dev *dma_dev;
	struct ccic_ctrl_dev *ctrl_dev;
	struct msc2_mmu_dev *sc2_mmu;
	struct mcam_frame_stat frame_state;	/* Frame state counter */
	enum mcam_state state;
	struct v4l2_pix_format_mplane mp;
	struct vb2_alloc_ctx *vb_alloc_ctx;
	enum v4l2_mbus_type bus_type;
	unsigned int mbus_flags;
	int mbus_fmt_num;
	enum v4l2_mbus_pixelcode mbus_fmt_code[5];
	struct list_head buffers;	/* Available frames */
	struct msc2_buffer *mbuf;
	struct msc2_buffer *mbuf_shadow;
#define CF_SINGLE_BUF	0
#define CF_FRAME_SOF0	1
#define CF_FRAME_OVERFLOW	2
	unsigned long flags;		/* Indicate frame buffer state */
	struct clk *mclk;
	struct clk *axi_clk;
	int i2c_dyn_ctrl;
};

#endif
