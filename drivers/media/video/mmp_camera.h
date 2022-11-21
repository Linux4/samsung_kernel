/*
 * Register definitions for the m88alp01 camera interface.
 * Offsets in bytes as given in the spec.
 *
 * Copyright 2006 One Laptop Per Child Association, Inc.
 * Written by Jonathan Corbet <corbet@lwn.net>
 *
 * Copyright (C) 2011-2012, Marvell International Ltd.
 * Modified by Kassey Lee <ygli@marvell.com>
 *	       Albert Wang <twang13@marvell.com>
 *
 * This file may be distributed under the terms of the GNU General
 * Public License, version 2.
 */
#ifndef __MMP_CAMERA_H
#define __MMP_CAMERA_H

/* All the Micro are for samsung debug, by Vincent Wan. */
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
//define __DEBUG_DMA_DONE			/* Set frames finished interrupt indicating, defined for 'DMA done', undefined for 'EOF detect', prefer 'dma done'.*/
#else
#define __DEBUG_DMA_DONE			/* Set frames finished interrupt indicating, defined for 'DMA done', undefined for 'EOF detect', prefer 'dma done'.*/
#endif
#define __DEBUG_ENABLE_OVERFLOWIRQ	/* Enable overflow irq */
#undef __DEBUG_DUMP_REGISTER		/* Dump All CCIC registers */
#undef __DEBUG_ENABLE_RAWSTATUS		/* Read the raw status registers */
#define SSG_SKIP_FRAMES 1		/* Skip the frames, 0: no skip, 1: skip one frame,2:skip two frames */
#undef __ENABLE_DDR_MIN_312MHZ		/* Enable limit min ddr clock to 312MHZ */
#undef __DEBUG_STREAM_ON_OFF_LOG	/* Enable stream on/off kernel log */
#undef __DEBUG_ENABLE_COUNTS_CHECK
#undef __DEBUG_ENABLE_SKIP_EOF_CHECK
/*
 * Y.U.V. reg define
 */
#define REG_Y0BAR	0x00
#define REG_Y1BAR	0x04
#define REG_Y2BAR	0x08
#define REG_U0BAR	0x0c
#define REG_U1BAR	0x10
#define REG_U2BAR	0x14
#define REG_V0BAR	0x18
#define REG_V1BAR	0x1C
#define REG_V2BAR	0x20

/*
 * MIPI enable
 */
#define REG_CSI2_CTRL0	0x100
#define REG_CSI2_DPHY3  0x12c
#define REG_CSI2_DPHY5  0x134
#define REG_CSI2_DPHY6  0x138

#define REG_IMGPITCH	0x24	/* Image pitch register */
#define   IMGP_YP_SHFT	  2		/* Y pitch params */
#define   IMGP_YP_MASK	  0x00003ffc	/* Y pitch field */
#define   IMGP_UVP_SHFT   18		/* UV pitch (planar) */
#define   IMGP_UVP_MASK   0x3ffc0000

#define REG_IRQSTATRAW	0x28	/* RAW IRQ Status */

#define REG_IRQMASK	0x2c	/* IRQ mask - same bits as IRQSTAT */

#define REG_IRQSTAT	0x30	/* IRQ status / clear */
#define   IRQ_DMA_DONE0	  0x01 << 18	/* Dma done of frame 0 */
#define   IRQ_DMA_DONE1	  0x01 << 19	/* Dma done of frame 1 */
#define   IRQ_DMA_DONE2	  0x01 << 20	/* Dma done of frame 2 */

#define   IRQ_EOF0	  0x00000001	/* End of frame 0 */
#define   IRQ_EOF1	  0x00000002	/* End of frame 1 */
#define   IRQ_EOF2	  0x00000004	/* End of frame 2 */
#define   IRQ_SOF0	  0x00000008	/* Start of frame 0 */
#define   IRQ_SOF1	  0x00000010	/* Start of frame 1 */
#define   IRQ_SOF2	  0x00000020	/* Start of frame 2 */
#define   IRQ_OVERFLOW	  0x00000040	/* FIFO overflow */
#define   FRAMEIRQS_EOF   (IRQ_EOF0 | IRQ_EOF1 | IRQ_EOF2)
#define   FRAMEIRQS_DMA_DONE   (IRQ_DMA_DONE0 | IRQ_DMA_DONE1 | IRQ_DMA_DONE2)
#define   FRAMEIRQS_SOF   (IRQ_SOF0 | IRQ_SOF1 | IRQ_SOF2)
#define   FRAMEIRQS	  (FRAMEIRQS_EOF | FRAMEIRQS_SOF)
#define   FRAMEIRQS_DMA	  (FRAMEIRQS_DMA_DONE | FRAMEIRQS_SOF)
#define   ALLIRQS	  (FRAMEIRQS | IRQ_OVERFLOW)
#define   ALLIRQS_DMA	  (FRAMEIRQS_DMA | IRQ_OVERFLOW)

#define REG_IMGSIZE	0x34	/* Image size */
#define  IMGSZ_V_MASK	  0x1fff0000
#define  IMGSZ_V_SHIFT	  16
#define  IMGSZ_H_MASK	  0x00003fff

#define REG_IMGOFFSET	0x38	/* IMage offset */

#define REG_CTRL0	0x3c	/* Control 0 */
#define   C0_ENABLE	  0x00000001	/* Makes the whole thing go */
/* Mask for all the format bits */
#define   C0_DF_MASK	  0x00fffffc    /* Bits 2-23 */
/* RGB ordering */
#define   C0_RGB4_RGBX	  0x00000000
#define   C0_RGB4_XRGB	  0x00000004
#define   C0_RGB4_BGRX	  0x00000008
#define   C0_RGB4_XBGR	  0x0000000c
#define   C0_RGB5_RGGB	  0x00000000
#define   C0_RGB5_GRBG	  0x00000004
#define   C0_RGB5_GBRG	  0x00000008
#define   C0_RGB5_BGGR	  0x0000000c
/* Spec has two fields for DIN and DOUT, but they must match, so
   combine them here. */
#define   C0_DF_YUV	  0x00000000    /* Data is YUV */
#define   C0_DF_RGB	  0x000000a0	/* Data is RGB */
#define   C0_DF_BAYER	  0x00000140	/* Data is Bayer */
/* 8-8-8 must be missing from the below - ask */
#define   C0_RGBF_565	  0x00000000
#define   C0_RGBF_444	  0x00000800
#define   C0_RGB_BGR	  0x00001000	/* Blue comes first */
#define   C0_YUV_PLANAR   0x00000000	/* YUV 422 planar format */
#define   C0_YUV_PACKED   0x00008000	/* YUV 422 packed format */
#define   C0_YUV_420PL	  0x0000a000	/* YUV 420 planar format */
/* Think that 420 packed must be 111 - ask */
#define   C0_YUVE_YUYV	  0x00000000	/* Y1CbY0Cr */
#define   C0_YUVE_YVYU	  0x00010000	/* Y1CrY0Cb */
#define   C0_YUVE_VYUY	  0x00020000	/* CrY1CbY0 */
#define   C0_YUVE_UYVY	  0x00030000	/* CbY1CrY0 */
#define   C0_YUVE_XYUV	  0x00000000    /* 420: .YUV */
#define   C0_YUVE_XYVU	  0x00010000	/* 420: .YVU */
#define   C0_YUVE_XUVY	  0x00020000	/* 420: .UVY */
#define   C0_YUVE_XVUY	  0x00030000	/* 420: .VUY */
/* Bayer bits 18,19 if needed */
#define   C0_HPOL_LOW	  0x01000000	/* HSYNC polarity active low */
#define   C0_VPOL_LOW	  0x02000000	/* VSYNC polarity active low */
#define   C0_VCLK_LOW	  0x04000000	/* VCLK on falling edge */
#define   C0_DOWNSCALE	  0x08000000	/* Enable downscaler */
#define   C0_SIFM_MASK	  0xc0000000	/* SIF mode bits */
#define   C0_SIF_HVSYNC   0x00000000	/* Use H/VSYNC */
#define   C0_SOF_NOSYNC   0x40000000	/* Use inband active signaling */
#define   C0_EOF_VSYNC	  0x00400000	/* Generate EOF by VSYNC */
#define   C0_VEDGE_CTRL   0x00800000	/* Detecting falling edge of VSYNC */

#define REG_CTRL1	0x40	/* Control 1 */
#define   C1_RESERVED	  0x0000003c	/* Reserved and shouldn't be changed */
#define   C1_444ALPHA	  0x00f00000	/* Alpha field in RGB444 */
#define   C1_ALPHA_SHFT   20
#define   C1_DMAB64	  0x00000000	/* 64-byte DMA burst */
#define   C1_DMAB128	  0x02000000	/* 128-byte DMA burst */
#define   C1_DMAB256	  0x04000000	/* 256-byte DMA burst */
#define   C1_DMAB_MASK	  0x06000000
#define   C1_TWOBUFS	  0x08000000	/* Use only two DMA buffers */
#define   C1_PWRDWN	  0x10000000	/* Power down */
#define   C1_DMAPOSTED	  0x40000000	/* DMA Posted Select */

#define REG_CTRL3	0x1ec	/* CCIC parallel mode */

#define REG_LNNUM	0x60	/* Lines num DMA filled */

#define REG_CLKCTRL	0x88	/* Clock control */

#define REG_FRAME_CNT 0x23c	/* Frame byte count */

#define   CLK_DIV_MASK	  0x0000ffff	/* Upper bits RW "reserved" */

/* State */
#define SR200PC20M_STATE_PREVIEW			0x0000	/*  preview state */
#define SR200PC20M_STATE_CAPTURE			0x0001	/*  capture state */
#define SR352_STATE_CAPTURE			0x0001	/*  capture state */
#define SR200PC20M_STATE_CAMCORDER		0x0002	/*  camcorder state */
#define SR200PC20M_STATE_HD_CAMCORDER	0x0003	/*  HD camcorder state */
#define SR200PC20M_STATE_INVALID			0x0004	/*  invalid state */

/*
 * Indicate flags for CCIC frame buffer state
 * 0	- No available buffer, will enter signle buffer mode
 * 1:3	- Normal mode, indicate which frame buffer is used in CCIC
 */
#define CF_SINGLE_BUF	0
#define CF_FRAME_SOF0	1

/*
 * CCIC can support at most 3 frame buffers
 * 2	- Use Two Buffers mode
 * 3	- Use Three Buffers mode
 */
#define MAX_DMA_BUFS	2
#define CF_FRMAE_OVERFLOW	(CF_FRAME_SOF0 + MAX_DMA_BUFS)
/*
 * Basic frame states
 */
struct mmp_frame_state {
	int frames;
	int singles;
	int delivered;
#if MAX_DMA_BUFS == 3
	int tribufs;	/* Only tribufs == 2 can enter single buffer mode */
#endif
};

struct yuv_pointer_t {
	dma_addr_t y;
	dma_addr_t u;
	dma_addr_t v;
};

/*
 * buffer for one video frame
 */
struct mmp_buffer {
	/*
	 * common v4l buffer stuff -- must be first
	 */
	struct vb2_buffer vb_buf;
	struct yuv_pointer_t yuv_p;
	struct list_head queue;
	struct page *page;
	size_t bsize;
	int list_init_flag;
};

enum mmp_camera_state {
	S_IDLE,		/* Just hanging around */
	S_STREAMING,	/* Streaming data */
	S_BUFWAIT	/* streaming requested but no buffers yet */
};

struct mmp_camera_dev {
	struct soc_camera_host soc_host;
	struct soc_camera_device *icd;
	unsigned int irq;
	void __iomem *base;
	struct platform_device *pdev;
	struct resource *res;
	struct list_head buffers;	/* Available frames */
	spinlock_t list_lock;
	struct mutex s_mutex;		/* Access to this structure */
	struct v4l2_pix_format pix_format;
	unsigned long flags;		/* Indicate frame buffer state */
	struct mmp_frame_state frame_state;	/* Frame state counter */
	enum mmp_camera_state state;
	struct mmp_buffer *vb_bufs[MAX_DMA_BUFS];
	unsigned int nbufs;		/* How many bufs are used for ccic */
	struct vb2_alloc_ctx *vb_alloc_ctx;
	int frame_rate;
	struct pm_qos_request qos_idle;
	struct pm_qos_request qos_ddr;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	struct ccic_mcd		mcd_root;
	struct mcd_dphy		mcd_dphy;
	/* in current design, DPHY and CCIC is coupled, so have to put
	 * mcd_dphy_hw here, should move to a proper place in future */
	struct mcd_dphy_hw	mcd_dphy_hw;
	struct mcd_dma		mcd_ccic;
	struct mcd_vdev		mcd_vdev;
	struct mcd_sensor	mcd_sensor;
#endif
};

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
/* Get the counter value */
#define GET(mcd, ent_id, val_id) \
	mcd_value_read(&(mcd.pentity[ent_id]->value[val_id]))
/* Set the counter value */
#define SET(mcd, ent_id, val_id, val) \
	mcd_value_set(&(mcd.pentity[ent_id]->value[val_id]), val)
/* increase or decrease the counter value */
#define PEG(mcd, ent_id, val_id, cnt) \
	mcd_value_peg(&(mcd.pentity[ent_id]->value[val_id]), (cnt))
/* Set the counter value 0 */
#define CLEAR(mcd, ent_id, val_id) \
	mcd_value_set(&(mcd.pentity[ent_id]->value[val_id]), 0)
#endif

/*
 * Device register I/O
 */
static inline u32 ccic_reg_read(struct mmp_camera_dev *pcdev, unsigned int reg)
{
	return ioread32(pcdev->base + reg);
}

static inline void ccic_reg_write(struct mmp_camera_dev *pcdev,
			unsigned int reg, u32 val)
{
	iowrite32(val, pcdev->base + reg);
}

static inline void ccic_reg_write_mask(struct mmp_camera_dev *pcdev,
			unsigned int reg, u32 val, u32 mask)
{
	u32 v = ccic_reg_read(pcdev, reg);

	v = (v & ~mask) | (val & mask);
	ccic_reg_write(pcdev, reg, v);
}

static inline void ccic_reg_set_bit(struct mmp_camera_dev *pcdev,
			unsigned int reg, u32 val)
{
	ccic_reg_write_mask(pcdev, reg, val, val);
}

static inline void ccic_reg_clear_bit(struct mmp_camera_dev *pcdev,
			unsigned int reg, u32 val)
{
	ccic_reg_write_mask(pcdev, reg, 0, val);
}

#endif
