/*
 * linux/arch/arm/plat-pxa/include/plat/overlay_ioctl.h
 *
 *  Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_PLAT_FB_IOCTL_H
#define __ASM_PLAT_FB_IOCTL_H
/* ---------------------------------------------- */
/*              IOCTL Definition                  */
/* ---------------------------------------------- */
#define FB_IOC_MAGIC                        'm'
#define FB_IOCTL_CONFIG_CURSOR              _IO(FB_IOC_MAGIC, 0)
#define FB_IOCTL_DUMP_REGS                  _IO(FB_IOC_MAGIC, 1)
#define FB_IOCTL_CLEAR_IRQ                  _IO(FB_IOC_MAGIC, 2)

/*
 * There are many video mode supported.
 */
#define FB_IOCTL_SET_VIDEO_MODE             _IO(FB_IOC_MAGIC, 3)
#define FB_IOCTL_GET_VIDEO_MODE             _IO(FB_IOC_MAGIC, 4)
/* Request a new video buffer from driver. User program needs to free
 * this memory.
 */
#define FB_IOCTL_CREATE_VID_BUFFER          _IO(FB_IOC_MAGIC, 5)

/* Configure viewport in driver. */
#define FB_IOCTL_SET_VIEWPORT_INFO          _IO(FB_IOC_MAGIC, 6)
#define FB_IOCTL_GET_VIEWPORT_INFO          _IO(FB_IOC_MAGIC, 7)

/* Flip the video buffer from user mode. Vide buffer can be separated into:
 * a. Current-used buffer - user program put any data into it. It will be
 *    displayed immediately.
 * b. Requested from driver but not current-used - user programe can put any
 *    data into it. It will be displayed after calling
 *    FB_IOCTL_FLIP_VID_BUFFER.
 *    User program should free this memory when they don't use it any more.
 * c. User program alloated - user program can allocated a contiguos DMA
 *    buffer to store its video data. And flip it to driver. Notices that
 *    this momory should be free by user programs. Driver won't take care of
 *    this.
 */
#define FB_IOCTL_FLIP_VID_BUFFER            _IO(FB_IOC_MAGIC, 8)

/* Get the current buffer information. User program could use it to display
 * anything directly. If developer wants to allocate multiple video layers,
 * try to use FB_IOCTL_CREATE_VID_BUFFER  to request a brand new video
 * buffer.
 */
#define FB_IOCTL_GET_BUFF_ADDR              _IO(FB_IOC_MAGIC, 9)

/* Get/Set offset position of screen */
#define FB_IOCTL_SET_VID_OFFSET             _IO(FB_IOC_MAGIC, 10)
#define FB_IOCTL_GET_VID_OFFSET             _IO(FB_IOC_MAGIC, 11)

/* Turn on the memory toggle function to improve the frame rate while playing
 * movie.
 */
#define FB_IOCTL_SET_MEMORY_TOGGLE          _IO(FB_IOC_MAGIC, 12)

/* Turn on the memory toggle function to improve the frame rate while playing
 * movie.
 */
#define FB_IOCTL_SET_COLORKEYnALPHA         _IO(FB_IOC_MAGIC, 13)
#define FB_IOCTL_GET_COLORKEYnALPHA         _IO(FB_IOC_MAGIC, 14)
#define FB_IOCTL_SWITCH_GRA_OVLY            _IO(FB_IOC_MAGIC, 15)
#define FB_IOCTL_SWITCH_VID_OVLY            _IO(FB_IOC_MAGIC, 16)

/* For VPro integration */
#define FB_IOCTL_GET_FREELIST               _IO(FB_IOC_MAGIC, 17)

/* Wait for vsync happen. */
#define FB_IOCTL_WAIT_VSYNC                 _IO(FB_IOC_MAGIC, 18)

/* clear framebuffer: Makes resolution or color space changes look nicer */
#define FB_IOCTL_CLEAR_FRAMEBUFFER              _IO(FB_IOC_MAGIC, 19)

/* Wait for vsync each time pan display */
#define FB_IOCTL_WAIT_VSYNC_ON              _IO(FB_IOC_MAGIC, 20)
#define FB_IOCTL_WAIT_VSYNC_OFF             _IO(FB_IOC_MAGIC, 21)

/* Get and set the display surface */
#define FB_IOCTL_GET_SURFACE			_IO(FB_IOC_MAGIC, 22)
#define FB_IOCTL_SET_SURFACE			_IO(FB_IOC_MAGIC, 23)

/* Graphic partial display ctrl */
#define FB_IOCTL_GRA_PARTDISP			_IO(FB_IOC_MAGIC, 24)

/* VDMA enable/disable */
#define FB_IOCTL_VDMA_SET			_IO(FB_IOC_MAGIC, 25)

#define FB_IOCTL_FLIP_VSYNC	            _IO(FB_IOC_MAGIC, 26)


/* Global alpha blend controls - Maintaining compatibility with existing
   user programs. */
#define FB_IOCTL_PUT_VIDEO_ALPHABLEND            0xeb
#define FB_IOCTL_PUT_GLOBAL_ALPHABLEND           0xe1
#define FB_IOCTL_PUT_GRAPHIC_ALPHABLEND          0xe2

/* color swapping */
#define FB_IOCTL_SWAP_GRAPHIC_RED_BLUE       0xe3
#define FB_IOCTL_SWAP_GRAPHIC_U_V            0xe4
#define FB_IOCTL_SWAP_GRAPHIC_Y_UV           0xe5
#define FB_IOCTL_SWAP_VIDEO_RED_BLUE         0xe6
#define FB_IOCTL_SWAP_VIDEO_U_V              0xe7
#define FB_IOCTL_SWAP_VIDEO_Y_UV             0xe8

/* colorkey compatibility */
#define FB_IOCTL_GET_CHROMAKEYS		0xe9
#define FB_IOCTL_PUT_CHROMAKEYS		0xea

/* cmu operation */
#define FB_IOCTL_CMU_SWITCH		0xf1
#define FB_IOCTL_CMU_WRITE		0xf2
#define FB_IOCTL_CMU_READ		0xf3
#define FB_IOCTL_CMU_SET_ROUTE		0xf4
#define FB_IOCTL_CMU_SET_PIP		0xf5
#define FB_IOCTL_CMU_GET_RES		0xf6
#define FB_IOCTL_CMU_SET_LETTER_BOX 0xf7

/* gamma correction */
#define FB_IOCTL_GAMMA_SET		0xff

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

#define FB_HWCMODE_1BITMODE                 0x0
#define FB_HWCMODE_2BITMODE                 0x1

#define FB_DISABLE_COLORKEY_MODE            0x0
#define FB_ENABLE_Y_COLORKEY_MODE           0x1
#define FB_ENABLE_U_COLORKEY_MODE           0x2
#define FB_ENABLE_V_COLORKEY_MODE           0x4
#define FB_ENABLE_RGB_COLORKEY_MODE         0x3
#define FB_ENABLE_R_COLORKEY_MODE           0x5
#define FB_ENABLE_G_COLORKEY_MODE           0x6
#define FB_ENABLE_B_COLORKEY_MODE           0x7

#define FB_VID_PATH_ALPHA		0x0
#define FB_GRA_PATH_ALPHA		0x1
#define FB_CONFIG_ALPHA			0x2

#define FB_SYNC_COLORKEY_TO_CHROMA          1
#define FB_SYNC_CHROMA_TO_COLORKEY          2

#define MAX_QUEUE_NUM 30

/* ---------------------------------------------- */
/*              Data Structure                    */
/* ---------------------------------------------- */
/*
 * The follow structures are used to pass data from
 * user space into the kernel for the creation of
 * overlay surfaces and setting the video mode.
 */

#define FBVideoMode int

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

struct _sColorKeyNAlpha {
	unsigned int mode;
	unsigned int alphapath;
	unsigned int config;
	unsigned int Y_ColorAlpha;
	unsigned int U_ColorAlpha;
	unsigned int V_ColorAlpha;
};

struct _sOvlySurface {
	FBVideoMode videoMode;
	struct _sViewPortInfo viewPortInfo;
	struct _sViewPortOffset viewPortOffset;
	struct _sVideoBufferAddr videoBufferAddr;
};

struct mvdisp_cmu_config {
	unsigned int addr;
	unsigned int data;
};

struct mvdisp_cmu_pip {
	unsigned int left;
	unsigned int right;
	unsigned int top;
	unsigned int bottom;
};

struct mvdisp_partdisp {
	/* path id, 0->panel, 1->TV, 2->panel2 */
	int id;
	/* partial display horizontal starting pixel number
	 * NOTE: it must be 64x */
	unsigned int horpix_start;
	/* partial display vertical starting line number */
	unsigned int vertline_start;
	/* partial display horizontal ending pixel number
	 * NOTE: it must be 64x */
	unsigned int horpix_end;
	/* partial display vertical ending line number */
	unsigned int vertline_end;
	/* graphic color for partial disabled area,
	 * color format should be RGB565 */
	unsigned short color;
};

struct mvdisp_vdma {
	/* path id, 0->panel, 1->TV, 2->panel2 */
	unsigned int path;
	/* 0: grafhics, 1: video */
	unsigned int layer;
	unsigned int enable;
};

struct mvdisp_gamma {
#define GAMMA_ENABLE	(1 << 0)
#define GAMMA_DUMP	(1 << 1)
	unsigned int	flag;
#define GAMMA_TABLE_LEN	256
	char		table[GAMMA_TABLE_LEN];
};

enum {
	SYNC_SELF,
	SYNC_PANEL,
	SYNC_TV,
	SYNC_PANEL_TV,
};
#endif /* __ASM_PLAT_FB_IOCTL_H */
