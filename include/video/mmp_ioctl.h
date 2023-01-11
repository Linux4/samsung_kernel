/*
 * linux/include/video/mmp_ioctl.h
 * Header file for Marvell MMP Display Controller
 *
 * Copyright (C) 2013 Marvell Technology Group Ltd.
 * Authors: Yu Xu <yuxu@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef _MMP_IOCTL_H_
#define _MMP_IOCTL_H_

#include <linux/ioctl.h>
#include <video/mmp_disp.h>

#define FB_IOC_MAGIC			'm'

/* Set Gamma correction */
#define FB_IOCTL_GAMMA_SET		_IO(FB_IOC_MAGIC, 7)

/* Flip buffer from user space */
#define FB_IOCTL_FLIP_USR_BUF		_IO(FB_IOC_MAGIC, 8)

/* Set color key and alpha */
#define FB_IOCTL_SET_COLORKEYnALPHA	_IO(FB_IOC_MAGIC, 13)

/* Get color key and alpha */
#define FB_IOCTL_GET_COLORKEYnALPHA	_IO(FB_IOC_MAGIC, 14)

/* Enable DMA */
#define FB_IOCTL_ENABLE_DMA		_IO(FB_IOC_MAGIC, 15)

/* Enable DMA */
#define FB_IOCTL_ENABLE_COMMIT_DMA		_IO(FB_IOC_MAGIC, 16)

/* Wait for vsync happen */
#define FB_IOCTL_WAIT_VSYNC		_IO(FB_IOC_MAGIC, 18)

/* Graphic partial display ctrl */
#define FB_IOCTL_GRA_PARTDISP		_IO(FB_IOC_MAGIC, 24)

/* Flip buffer from user space, then vait for vsync and return */
#define FB_IOCTL_FLIP_VSYNC		_IO(FB_IOC_MAGIC, 26)

/* Flip buffer from user space, for multi layer to sync */
#define FB_IOCTL_FLIP_COMMIT		_IO(FB_IOC_MAGIC, 27)

/* Query fb global info */
#define FB_IOCTL_QUERY_GLOBAL_INFO	_IO(FB_IOC_MAGIC, 29)

#define FB_IOCTL_VSMOOTH_EN		_IO(FB_IOC_MAGIC, 30)
#define FB_IOCTL_SET_DFC_RATE		_IO(FB_IOC_MAGIC, 31)
#define FB_IOCTL_GET_DFC_RATE		_IO(FB_IOC_MAGIC, 32)

/* Set path alpha */
#define FB_IOCTL_SET_PATHALPHA	_IO(FB_IOC_MAGIC, 28)

#define LEGACY_FB_VMODE 1

#ifdef LEGACY_FB_VMODE
#define FB_VMODE_RGB565			0x100
#define FB_VMODE_BGR565                 0x101
#define FB_VMODE_RGB1555		0x102
#define FB_VMODE_BGR1555                0x103
#define FB_VMODE_RGB888PACK		0x104
#define FB_VMODE_BGR888PACK		0x105
#define FB_VMODE_RGB888UNPACK		0x106
#define FB_VMODE_BGR888UNPACK		0x107
#define FB_VMODE_RGBA888		0x108
#define FB_VMODE_BGRA888		0x109
#define FB_VMODE_RGB888A		0x10A
#define FB_VMODE_BGR888A		0x10B

#define FB_VMODE_YUV422PACKED               0x0
#define FB_VMODE_YUV422PACKED_SWAPUV        0x1
#define FB_VMODE_YUV422PACKED_SWAPYUorV     0x2
#define FB_VMODE_YUV422PLANAR               0x3
#define FB_VMODE_YUV422PLANAR_SWAPUV        0x4
#define FB_VMODE_YUV422PLANAR_SWAPYUorV     0x5
#define FB_VMODE_YUV420PLANAR               0x6
#define FB_VMODE_YUV420PLANAR_SWAPUV        0x7
#define FB_VMODE_YUV420PLANAR_SWAPYUorV     0x8
#define FB_VMODE_YUV422PACKED_IRE_90_270    0x9
#define FB_VMODE_YUV420SEMIPLANAR           0xA
#define FB_VMODE_YUV420SEMIPLANAR_SWAPUV    0xB
#endif /* LEGACY_FB_VMODE */

/*
 * Used to set the alpha between path
 * User only need to select two overlays
 */
#define FB_FB0_AND_FB1		0x0
#define FB_FB0_AND_FB2		0x1
#define FB_FB0_AND_FB3		0x2
#define FB_FB1_AND_FB2		0x3
#define FB_FB1_AND_FB3		0x4
#define FB_FB2_AND_FB3		0x5
#define FB_FB0_ALPHA		0x0
#define FB_FB1_ALPHA		0x1
#define FB_FB2_ALPHA		0x2
#define FB_FB3_ALPHA		0x3

/*
 * The follow structures are used to pass data from
 * user space into the kernel for the creation of
 * overlay surfaces and setting the video mode.
 */

struct _sViewPortInfo {
	unsigned short srcWidth;        /* video source size */
	unsigned short srcHeight;
	unsigned short zoomXSize;       /* size after zooming */
	unsigned short zoomYSize;
	unsigned short yPitch;
	unsigned short uPitch;
	unsigned short vPitch;
	unsigned int rotation;
	unsigned int yuv_format;
};

struct _sViewPortOffset {
	unsigned short xOffset;         /* position on screen */
	unsigned short yOffset;
};

struct _sVideoBufferAddr {
	unsigned char   frameID;        /* which frame wants */
	 /* new buffer (PA). 3 addr for YUV planar */
	unsigned char *startAddr[3];
	unsigned char *inputData;       /* input buf address (VA) */
	unsigned int length;            /* input data's length */
};

struct _sOvlySurface {
	int videoMode;
	struct _sViewPortInfo viewPortInfo;
	struct _sViewPortOffset viewPortOffset;
	struct _sVideoBufferAddr videoBufferAddr;
};

#endif	/* _MMP_IOCTL_H_ */
