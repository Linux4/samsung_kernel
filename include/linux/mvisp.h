/*
 * mvisp.h
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


#ifndef ISP_USER_H
#define ISP_USER_H

#include <linux/videodev2.h>
#include <linux/types.h>
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
#include <media/mc_debug.h>
#endif

struct v4l2_dxoipc_set_fb {
	void	*virAddr[4];
	int		phyAddr[4];
	int		size[4];
	int		fbcnt;
	int		burst_read;
	int		burst_write;
	int		flush_fifo_by_timer;
};

struct v4l2_ispdma_timeinfo {
	unsigned long	timestamp;
	unsigned long	delta;
};

struct v4l2_dxoipc_ipcwait {
	int				timeout;
	struct v4l2_ispdma_timeinfo tickinfo;
};

struct v4l2_dxoipc_streaming_config {
	int		enable_in;
	int		enable_disp;
	int		enable_codec;
	int		enable_fbrx;
	int		enable_fbtx;
};

struct v4l2_dxoipc_config_codec {
	int		vbnum;
	int		vbsize;
	int		dma_burst_size;
};

struct v4l2_sensor_get_driver_name {
	char	driver[16];
};

struct v4l2_sensor_register_access {
	unsigned long	reg;
	unsigned long	value;
};

struct v4l2_sensor_list_regs_access {
	struct v4l2_sensor_register_access *regs;
	int continuous;
	int num;
};

/*Normal: */
/* 1. Dummy buffer will reuse filled buffer.*/
/* 2. Driver will hold one buffer in queue serve as dummy buffer.*/
/*Still: */
/* 1. Dummy buffer will only use idle buffer.*/
/* 2. Driver will not hold any buffer in queue.*/
enum ispvideo_capture_mode {
	ISPVIDEO_NORMAL_CAPTURE = 0,
	ISPVIDEO_STILL_CAPTURE,
};

enum ispdma_port {
	ISPDMA_PORT_CODEC = 0,
	ISPDMA_PORT_DISPLAY,
	ISPDMA_PORT_FBRX,
	ISPDMA_PORT_FBTX,
	ISPDMA_PORT_INPUT,
};

struct v4l2_ispdma_capture_mode {
	enum ispdma_port port;
	enum ispvideo_capture_mode mode;
};

struct v4l2_ispdma_reset {
	unsigned long	param;
};

struct v4l2_ccic_config_mipi {
	int start_mipi;
	int lanes;
};

struct v4l2_ccic_dump_registers {
	unsigned long y0_base_addr;
	unsigned long y1_base_addr;
	unsigned long y2_base_addr;
	unsigned long irq_raw_status;
	unsigned long irq_status;
	unsigned long irq_mask;
	unsigned long ctrl_0;
	unsigned long ctrl_1;
	unsigned long clock_ctrl;
	unsigned long csi2_irq_raw_status;
	unsigned long csi2_dphy3;
	unsigned long csi2_dphy5;
	unsigned long csi2_dphy6;
	unsigned long img_size;
	unsigned long img_pitch;
};

struct v4l2_ispdma_dump_registers {
	unsigned long ispdma_mainctrl;
	unsigned long ispdma_dmaena;
	unsigned long ispdma_clkena;
	unsigned long ispdma_irqraw;
	unsigned long ispdma_irqmask;
	unsigned long ispdma_irqstat;
	unsigned long ispdma_insz;
	unsigned long ispdma_inpsdma_ctrl;
	unsigned long ispdma_fbtx0_sdca;
	unsigned long ispdma_fbtx0_dcsz;
	unsigned long ispdma_fbtx0_ctrl;
	unsigned long ispdma_fbtx0_dstsz;
	unsigned long ispdma_fbtx0_dstaddr;
	unsigned long ispdma_fbtx0_tmr;
	unsigned long ispdma_fbtx0_ramctrl;
	unsigned long ispdma_fbrx0_sdca;
	unsigned long ispdma_fbrx0_dcsz;
	unsigned long ispdma_fbrx0_ctrl;
	unsigned long ispdma_fbrx0_tmr;
	unsigned long ispdma_fbrx0_ramctrl;
	unsigned long ispdma_fbrx0_stat;
	unsigned long ispdma_fbtx1_sdca;
	unsigned long ispdma_fbtx1_dcsz;
	unsigned long ispdma_fbtx1_ctrl;
	unsigned long ispdma_fbtx1_dstsz;
	unsigned long ispdma_fbtx1_dstaddr;
	unsigned long ispdma_fbtx1_tmr;
	unsigned long ispdma_fbtx1_ramctrl;
	unsigned long ispdma_fbrx1_sdca;
	unsigned long ispdma_fbrx1_dcsz;
	unsigned long ispdma_fbrx1_ctrl;
	unsigned long ispdma_fbrx1_tmr;
	unsigned long ispdma_fbrx1_ramctrl;
	unsigned long ispdma_fbrx1_stat;
	unsigned long ispdma_fbtx2_sdca;
	unsigned long ispdma_fbtx2_dcsz;
	unsigned long ispdma_fbtx2_ctrl;
	unsigned long ispdma_fbtx2_dstsz;
	unsigned long ispdma_fbtx2_dstaddr;
	unsigned long ispdma_fbtx2_tmr;
	unsigned long ispdma_fbtx2_ramctrl;
	unsigned long ispdma_fbrx2_sdca;
	unsigned long ispdma_fbrx2_dcsz;
	unsigned long ispdma_fbrx2_ctrl;
	unsigned long ispdma_fbrx2_tmr;
	unsigned long ispdma_fbrx2_ramctrl;
	unsigned long ispdma_fbrx2_stat;
	unsigned long ispdma_fbtx3_sdca;
	unsigned long ispdma_fbtx3_dcsz;
	unsigned long ispdma_fbtx3_ctrl;
	unsigned long ispdma_fbtx3_dstsz;
	unsigned long ispdma_fbtx3_dstaddr;
	unsigned long ispdma_fbtx3_tmr;
	unsigned long ispdma_fbtx3_ramctrl;
	unsigned long ispdma_fbrx3_sdca;
	unsigned long ispdma_fbrx3_dcsz;
	unsigned long ispdma_fbrx3_ctrl;
	unsigned long ispdma_fbrx3_tmr;
	unsigned long ispdma_fbrx3_ramctrl;
	unsigned long ispdma_fbrx3_stat;
	unsigned long ispdma_disp_ctrl;
	unsigned long ispdma_disp_dstsz;
	unsigned long ispdma_disp_dstaddr;
	unsigned long ispdma_disp_ramctrl;
	unsigned long ispdma_disp_pitch;
	unsigned long ispdma_codec_ctrl;
	unsigned long ispdma_codec_dstsz;
	unsigned long ispdma_codec_dstaddr;
	unsigned long ispdma_codec_ramctrl;
	unsigned long ispdma_codec_stat;
	unsigned long ispdma_codec_pitch;
	unsigned long ispdma_codec_vbsz;
	unsigned long ispdma_inpsdma_srcaddr;
	unsigned long ispdma_inpsdma_srcsz;
	unsigned long ispdma_inpsdma_pixsz;
	unsigned long isp_irqraw;
	unsigned long isp_irqmask;
	unsigned long isp_irqstat;
};

struct v4l2_ispdma_dma_timeinfo {
	struct v4l2_ispdma_timeinfo disp_dma_timeinfo;
	struct v4l2_ispdma_timeinfo disp_ps_timeinfo;
	struct v4l2_ispdma_timeinfo codec_dma_timeinfo;
	struct v4l2_ispdma_timeinfo codec_ps_timeinfo;
};

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
struct mvisp_mcd {
	struct mcd		mcd;
	struct mcd_entity	*pitem[MCD_ENTITY_END];
};
#endif /* CONFIG_VIDEO_MRVL_CAM_DEBUG */

#define VIDIOC_PRIVATE_DXOIPC_SET_FB \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct v4l2_dxoipc_set_fb)
#define VIDIOC_PRIVATE_DXOIPC_WAIT_IPC \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct v4l2_dxoipc_ipcwait)
#define VIDIOC_PRIVATE_DXOIPC_SET_STREAM \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct v4l2_dxoipc_streaming_config)
#define VIDIOC_PRIVATE_SENSER_GET_DRIVER_NAME \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 4, struct v4l2_sensor_get_driver_name)
#define VIDIOC_PRIVATE_SENSER_REGISTER_SET \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 5, struct v4l2_sensor_register_access)
#define VIDIOC_PRIVATE_SENSER_REGISTER_GET \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 6, struct v4l2_sensor_register_access)
#define VIDIOC_PRIVATE_DXOIPC_CONFIG_CODEC \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 7, struct v4l2_dxoipc_config_codec)
#define VIDIOC_PRIVATE_ISPDMA_CAPTURE_MODE \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 8, struct v4l2_ispdma_capture_mode)
#define VIDIOC_PRIVATE_ISPDMA_RESET \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 9, struct v4l2_ispdma_reset)
#define VIDIOC_PRIVATE_ISPDMA_GETDELTA \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 10, struct v4l2_ispdma_timeinfo)
#define VIDIOC_PRIVATE_CCIC_CONFIG_MIPI \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct v4l2_ccic_config_mipi)
#define VIDIOC_PRIVATE_CCIC_DUMP_REGISTERS \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 12, struct v4l2_ccic_dump_registers)
#define VIDIOC_PRIVATE_ISPDMA_DUMP_REGISTERS \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 13, struct v4l2_ispdma_dump_registers)
#define VIDIOC_PRIVATE_ISPDMA_GET_DMA_TIMEINFO \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 14, struct v4l2_ispdma_dma_timeinfo)
#define VIDIOC_PRIVATE_CCIC_SET_STREAM \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 15, int)
#define VIDIOC_PRIVATE_ISPDMA_SET_STREAM \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 16, int)
#define VIDIOC_PRIVATE_ISPDMA_GET_ISP_FUNC_CLK\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 17, int)
#define VIDIOC_PRIVATE_CCIC_GET_SENSOR_MCLK\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 18, int)
#define VIDIOC_PRIVATE_SENSER_REGS_LIST_SET \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 19, \
					struct v4l2_sensor_list_regs_access)
#define VIDIOC_PRIVATE_ISPDMA_LCD_DMA_OPS\
	_IOWR('V', BASE_VIDIOC_PRIVATE + 20, int)
#define VIDIOC_PRIVATE_DXOIPC_SET_FB_DC \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 21, struct v4l2_dxoipc_set_fb)
#endif	/* ISP_USER_H */
