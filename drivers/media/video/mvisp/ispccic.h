/*
 * ispccic.h
 *
 * Marvell DxO ISP - DMA module
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


#ifndef ISP_CCIC_H
#define ISP_CCIC_H

#include <linux/types.h>
#include <linux/videodev2.h>

#include "ispvideo.h"

/* This is not an exhaustive list */
enum isp_ccic_pix_formats {
	CCIC_PIX_FMT_OTHERS = 0,
	CCIC_PIX_FMT_YUV422_8BIT = 0x1e,
	CCIC_PIX_FMT_YUV422_8BIT_VP = 0x9e,
};

enum isp_ccic_identity {
	CCIC_ID_INVALID = 0,
	CCIC_ID_1,
	CCIC_ID_2,
};

#define CCIC_PAD_SINK			0
#define CCIC_PAD_ISP_SRC		1
#define CCIC_PAD_DMA_SRC		2
#define CCIC_PADS_NUM			3

enum ccic_dma_state {
	CCIC_DMA_IDLE = 0,
	CCIC_DMA_BUSY,
};

enum ccic_input_type {
	CCIC_INPUT_NONE = 0x0,
	CCIC_INPUT_SENSOR = 0x1,
};

enum ccic_output_type {
	CCIC_OUTPUT_NONE = 0x0,
	CCIC_OUTPUT_ISP = 0x1,
	CCIC_OUTPUT_MEMORY = 0x2,
};

enum isp_ccic_irq_type {
	CCIC_EOF0 = 0,
	CCIC_EOF1,
	CCIC_EOF2,
};

enum ccic_mipi_state {
	MIPI_NOT_SET = 0,
	MIPI_SET,
};

enum mv_isp_sensor_type {
	SENSOR_INVALID = 0,
	SENSOR_OV882X,
	SENSOR_OV5642,
	SENSOR_OV5647,
	SENSOR_LSI3H5,
};

struct isp_ccic_device {
	struct v4l2_subdev			subdev;
	struct media_pad			pads[CCIC_PADS_NUM];
	struct v4l2_mbus_framefmt	formats[CCIC_PADS_NUM];
	struct mvisp_device			*isp;
	struct isp_video			video_out;
	enum ccic_output_type		output;
	enum ccic_input_type		input;

	enum isp_ccic_identity			ccic_id;
	enum isp_pipeline_stream_state	state;
	spinlock_t				irq_lock;

	struct mutex			ccic_mutex;
	enum ccic_mipi_state	mipi_config_flag;
	spinlock_t				mipi_flag_lock;

	enum mv_isp_sensor_type sensor_type;
	enum ccic_dma_state		dma_state;
	int	stream_refcnt;
	int lanes;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	struct mcd_dphy		mcd_dphy;
	/* in current design, DPHY and CCIC is coupled, so have to put
	 * mcd_dphy_hw here, should move to CSI2 subdev in future */
	struct mcd_dphy_hw	mcd_dphy_hw;
#endif
};

int pxa_ccic_init(struct mvisp_device *isp);
void pxa_ccic_set_sensor_type(struct isp_ccic_device *ccic,
					enum mv_isp_sensor_type sensor_type);
void pxa_ccic_cleanup(struct mvisp_device *isp);
void pxa_ccic_unregister_entities(struct isp_ccic_device *ccic);
int pxa_ccic_register_entities(struct isp_ccic_device *ccic,
				    struct v4l2_device *vdev);
void pxa_ccic_dma_isr_handler(struct isp_ccic_device *ccic,
					unsigned long irq_status);
void pxa_ccic_ctrl_pixclk(struct mvisp_device *isp, int on);

#endif	/* ISP_CCIC_H */
