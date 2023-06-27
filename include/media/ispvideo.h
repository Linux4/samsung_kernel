/*
* ispvideo.h
*
* Marvell DxO ISP - video node module
*  Based on omap3isp
*
* Copyright:  (C) Copyright 2011 Marvell International Ltd.
*			   Henry Zhao <xzhao10@marvell.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*/


#ifndef ISP_VIDEO_H
#define ISP_VIDEO_H

#include <linux/v4l2-mediabus.h>
#include <linux/version.h>
#include <media/media-entity.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-fh.h>
#include <media/videobuf2-core.h>
#include <media/map_vnode.h>

#include <linux/mvisp.h>

#define ISP_VIDEO_DRIVER_NAME		"mvsocisp"
#define ISP_VIDEO_DRIVER_VERSION	KERNEL_VERSION(0, 0, 1)

#define MIN_DRV_BUF			2

enum isp_buf_paddr {
	ISP_BUF_PADDR = 0,
	ISP_BUF_PADDR_U,
	ISP_BUF_PADDR_V,
	ISP_BUF_MAX_PADDR,
};

#define ISP_VIDEO_INPUT_NAME	"dma_input"
#define ISP_VIDEO_CODEC_NAME	"dma_codec"
#define ISP_VIDEO_DISPLAY_NAME	"dma_display"
#define ISP_VIDEO_CCIC1_NAME	"dma_ccic1"

#define ISP_VIDEO_NR_BASE		5

enum isp_video_type {
	ISP_VIDEO_UNKNOWN = 0,
	ISP_VIDEO_INPUT,
	ISP_VIDEO_DISPLAY,
	ISP_VIDEO_CODEC,
	ISP_VIDEO_CCIC
};

enum isp_pipeline_stream_state {
	ISP_PIPELINE_STREAM_STOPPED = 0,
	ISP_PIPELINE_STREAM_CONTINUOUS = 1,
};

int mvisp_video_register(struct map_vnode *video, struct v4l2_device *vdev,
			enum isp_video_type video_type);
void mvisp_video_unregister(struct map_vnode *video);

#endif /* ISP_VIDEO_H */
