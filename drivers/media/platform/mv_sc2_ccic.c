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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-dma-sg.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <media/mrvl-camera.h> /* TBD refined */

#include "mv_sc2_ccic.h"

#define MV_SC2_CCIC_NAME "mv_sc2_ccic"

static int trace = 2;
module_param(trace, int, 0644);
MODULE_PARM_DESC(trace,
		"how many trace do you want to see? (0-4):"
		"0 - mute "
		"1 - only actual errors"
		"2 - milestone log"
		"3 - briefing log"
		"4 - detailed log");

static LIST_HEAD(ccic_devices);
static DEFINE_MUTEX(list_lock);

/* dump register in order to debug */
void __attribute__((unused)) msc2_dump_regs(struct msc2_ccic_dev *ccic_dev)
{
	unsigned int ret;

	/*
	 * CCIC IRQ REG
	 */
	ret = ccic_reg_read(ccic_dev, REG_IRQSTAT);
	pr_info("CCIC: REG_IRQSTAT[0x%02x] is 0x%08x\n", REG_IRQSTAT, ret);
	ret = ccic_reg_read(ccic_dev, REG_IRQSTATRAW);
	pr_info("CCIC: REG_IRQSTATRAW[0x%02x] is 0x%08x\n", REG_IRQSTATRAW, ret);
	ret = ccic_reg_read(ccic_dev, REG_IRQMASK);
	pr_info("CCIC: REG_IRQMASK[0x%02x] is 0x%08x\n\n", REG_IRQMASK, ret);

	/*
	 * CCIC IMG REG
	 */
	ret = ccic_reg_read(ccic_dev, REG_IMGPITCH);
	pr_info("CCIC: REG_IMGPITCH[0x%02x] is 0x%08x\n", REG_IMGPITCH, ret);
	ret = ccic_reg_read(ccic_dev, REG_IMGSIZE);
	pr_info("CCIC: REG_IMGSIZE[0x%02x] is 0x%08x\n", REG_IMGSIZE, ret);
	ret = ccic_reg_read(ccic_dev, REG_IMGOFFSET);
	pr_info("CCIC: REG_IMGOFFSET[0x%02x] is 0x%08x\n\n", REG_IMGOFFSET, ret);

	/*
	 * CCIC CTRL REG
	 */
	ret = ccic_reg_read(ccic_dev, REG_CTRL0);
	pr_info("CCIC: REG_CTRL0[0x%02x] is 0x%08x\n", REG_CTRL0, ret);
	ret = ccic_reg_read(ccic_dev, REG_CTRL1);
	pr_info("CCIC: REG_CTRL1[0x%02x] is 0x%08x\n", REG_CTRL1, ret);
	ret = ccic_reg_read(ccic_dev, REG_CTRL2);
	pr_info("CCIC: REG_CTRL2[0x%02x] is 0x%08x\n", REG_CTRL2, ret);
	ret = ccic_reg_read(ccic_dev, REG_CTRL3);
	pr_info("CCIC: REG_CTRL3[0x%02x] is 0x%08x\n", REG_CTRL3, ret);
	ret = ccic_reg_read(ccic_dev, REG_IDI_CTRL);
	pr_info("CCIC: REG_IDI_CTRL[0x%02x] is 0x%08x\n\n", REG_IDI_CTRL, ret);
	ret = ccic_reg_read(ccic_dev, REG_LNNUM);
	pr_info("CCIC: REG_LNNUM[0x%02x] is 0x%08x\n", REG_LNNUM, ret);
	ret = ccic_reg_read(ccic_dev, REG_FRAME_CNT);
	pr_info("CCIC: REG_FRAME_CNT[0x%02x] is 0x%08x\n", REG_FRAME_CNT, ret);

	/*
	 * CCIC CSI2 REG
	 */
	ret = ccic_reg_read(ccic_dev, REG_CSI2_DPHY3);
	pr_info("CCIC: REG_CSI2_DPHY3[0x%02x] is 0x%08x\n", REG_CSI2_DPHY3, ret);
	ret = ccic_reg_read(ccic_dev, REG_CSI2_DPHY5);
	pr_info("CCIC: REG_CSI2_DPHY5[0x%02x] is 0x%08x\n", REG_CSI2_DPHY5, ret);
	ret = ccic_reg_read(ccic_dev, REG_CSI2_DPHY6);
	pr_info("CCIC: REG_CSI2_DPHY6[0x%02x] is 0x%08x\n", REG_CSI2_DPHY6, ret);
	ret = ccic_reg_read(ccic_dev, REG_CSI2_CTRL0);
	pr_info("CCIC: REG_CSI2_CTRL0[0x%02x] is 0x%08x\n\n", REG_CSI2_CTRL0, ret);
	ret = ccic_reg_read(ccic_dev, REG_CSI2_CTRL2);
	pr_info("CCIC: REG_CSI2_CTRL2[0x%02x] is 0x%08x\n\n", REG_CSI2_CTRL2, ret);
	ret = ccic_reg_read(ccic_dev, REG_CSI2_CTRL3);
	pr_info("CCIC: REG_CSI2_CTRL3[0x%02x] is 0x%08x\n\n", REG_CSI2_CTRL3, ret);

	/*
	 * CCIC YUV REG
	 */
	ret = ccic_reg_read(ccic_dev, REG_Y0BAR);
	pr_info("CCIC: REG_Y0BAR[0x%02x] 0x%08x\n", REG_Y0BAR, ret);
	ret = ccic_reg_read(ccic_dev, REG_U0BAR);
	pr_info("CCIC: REG_U0BAR[0x%02x] 0x%08x\n", REG_U0BAR, ret);
	ret = ccic_reg_read(ccic_dev, REG_V0BAR);
	pr_info("CCIC: REG_V0BAR[0x%02x] 0x%08x\n\n", REG_V0BAR, ret);

#if 0
	/*
	 * CCIC APMU REG
	 */
	ret = __raw_readl(get_apmu_base_va() + REG_CLK_CCIC_RES);
	pr_info("CCIC: APMU_CCIC_RES[0x%02x] is 0x%08x\n", REG_CLK_CCIC_RES, ret);
	ret = __raw_readl(get_apmu_base_va() + REG_CLK_CCIC2_RES);
	pr_info("CCIC: APMU_CCIC2_RES[0x%02x] is 0x%08x\n", REG_CLK_CCIC2_RES, ret);
#endif
}

/*
 * CCIC supports convertion between some formats
 * This function ranks the match between fourcc and mbus pixelcode
 * The highest score is 4, the lowest is 0
 * 4 means matching best while 0 means not match.
 */
static int ccic_mbus_fmt_score(u32 fourcc, enum v4l2_mbus_pixelcode code)
{
	switch (fourcc) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV422P:
		switch (code) {
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 4;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 0;
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 0;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 0;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_UYVY:
		switch (code) {
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 4;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 3;
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 0;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 0;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_NV12:
		switch (code) {
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 4;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 2;
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 1;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 3;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_YUYV:
		switch (code) {
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 4;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 0;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 3;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 0;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_VYUY:
		switch (code) {
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 0;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 3;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 0;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 4;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_NV21:
		switch (code) {
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 3;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 2;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 1;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 4;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_YVU420:
		switch (code) {
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 4;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 3;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 2;
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 1;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_YVYU:
		switch (code) {
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 4;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 0;
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 3;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 0;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_JPEG:
		switch (code) {
		case V4L2_MBUS_FMT_JPEG_1X8:
			return 4;
		default:
			return 0;
		}
	default:
		/* Other fmts to be added later */
		return 0;
	}
	return 0;
}

static u32 ccic_yuvendfmt(u32 fourcc, enum v4l2_mbus_pixelcode code)
{
	switch (fourcc) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		switch (code) {
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return C0_YUVE_YVYU;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return C0_YUVE_YUYV;
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return C0_YUVE_VYUY;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return C0_YUVE_UYVY;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_YUYV:
		switch (code) {
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return C0_YUVE_YUYV;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 0;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return C0_YUVE_VYUY;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 0;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_VYUY:
		switch (code) {
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 0;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return C0_YUVE_YVYU;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 0;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return C0_YUVE_YUYV;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_UYVY:
		switch (code) {
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return C0_YUVE_YVYU;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return C0_YUVE_YUYV;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return 0;
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return 0;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_YVYU:
		switch (code) {
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return C0_YUVE_YUYV;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return 0;
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return C0_YUVE_VYUY;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return 0;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_NV12:
		switch (code) {
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return C0_YUVE_VYUY | C0_YUV420SP_UV_SWAP;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return C0_YUVE_YUYV;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return C0_YUVE_VYUY;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return C0_YUVE_YUYV | C0_YUV420SP_UV_SWAP;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_NV21:
		switch (code) {
		case V4L2_MBUS_FMT_YUYV8_2X8:
			return C0_YUVE_VYUY;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return C0_YUVE_YUYV | C0_YUV420SP_UV_SWAP;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			return C0_YUVE_VYUY | C0_YUV420SP_UV_SWAP;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			return C0_YUVE_YUYV;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_YUV422P:
		switch (code) {
		case V4L2_MBUS_FMT_UYVY8_2X8:
			return C0_YUVE_YVYU;
		default:
			return 0;
		}
	case V4L2_PIX_FMT_JPEG:
	default:
		/* Other fmts to be added later */
		return 0;
	}
	return 0;
}

/*
 * width, height, pixfmt, code, bytesperline in dma_dev must
 * be set before this fucntion is called
 */
static int ccic_hw_setup_image(struct ccic_dma_dev *dma_dev)
{
	struct msc2_ccic_dev *ccic_dev = dma_dev->ccic_dev;
	struct device *dev = &ccic_dev->pdev->dev;
	u32 widthy = 0, widthuv = 0, imgsz_h, imgsz_w;
	u32 yuvendfmt;
	u32 pixfmt;
	u32 dma_burst;
	int width, height, bytesperline;
	enum v4l2_mbus_pixelcode code;
	unsigned long flags = 0;

	spin_lock_irqsave(&ccic_dev->ccic_lock, flags);

	pixfmt = dma_dev->pixfmt;
	width = dma_dev->width;
	height = dma_dev->height;
	bytesperline = dma_dev->bytesperline;
	code = dma_dev->code;

	if (width == 0 && height == 0) {
		spin_unlock_irqrestore(&ccic_dev->ccic_lock, flags);
		/* The caller not setup the argument correctly */
		dev_warn(dev, "image fmt must be set before setup_image called\n");
		return -EINVAL;
	}

	imgsz_h = (height << IMGSZ_V_SHIFT) & IMGSZ_V_MASK;
	imgsz_w = (width * 2) & IMGSZ_H_MASK;

	yuvendfmt = ccic_yuvendfmt(pixfmt, code);
	switch (pixfmt) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
		widthy = width * 2;
		widthuv = width * 2;
		break;
	case V4L2_PIX_FMT_RGB565:
		widthy = width * 2;
		widthuv = 0;
		break;
	case V4L2_PIX_FMT_JPEG:
		widthy = bytesperline;
		widthuv = bytesperline;
		break;
	case V4L2_PIX_FMT_YUV422P:
		widthy = width;
		widthuv = width / 2;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		widthy = width;
		widthuv = width / 2;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		widthy = width;
		widthuv = width / 2;
		break;
	case V4L2_PIX_FMT_SBGGR8:
		widthy = width;
		widthuv = 0;
		break;
	case V4L2_PIX_FMT_SBGGR10:
		widthy = width * 10 / 8;
		widthuv = 0;
		imgsz_w = widthy;
		break;
	default:
		break;
	}

	ccic_reg_write(ccic_dev, REG_IMGPITCH, widthuv << 16 | widthy);
	ccic_reg_write(ccic_dev, REG_IMGSIZE, imgsz_h | imgsz_w);
	ccic_reg_write(ccic_dev, REG_IMGOFFSET, 0x0);

	/*
	 * Tell the controller about the image format we are using.
	 */
	switch (pixfmt) {
	case V4L2_PIX_FMT_YUV422P:
		ccic_reg_write_mask(ccic_dev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_PLANAR | yuvendfmt, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		ccic_reg_write_mask(ccic_dev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_420PL | yuvendfmt, C0_DF_MASK);
		/* YUV420 planar does not support 256-byte burst */
		if (ccic_dev->dma_burst > 128)
			ccic_dev->dma_burst = 128;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
		ccic_reg_write_mask(ccic_dev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_PACKED | yuvendfmt, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_JPEG:
		ccic_reg_write_mask(ccic_dev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_PACKED | yuvendfmt |
				C0_VEDGE_CTRL | C0_EOF_VSYNC, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_RGB444:
		ccic_reg_write_mask(ccic_dev, REG_CTRL0,
			C0_DF_RGB | C0_RGBF_444 | C0_RGB4_XRGB, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_RGB565:
		ccic_reg_write_mask(ccic_dev, REG_CTRL0,
			C0_DF_RGB | C0_RGBF_565 | C0_RGB5_BGGR, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		ccic_reg_write_mask(ccic_dev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_PLANAR | yuvendfmt | C0_YUV420SP,
			C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SBGGR10:
		ccic_reg_write_mask(ccic_dev, REG_CTRL0,
			C0_DF_BAYER, C0_DF_MASK);
		break;
	default:
		dev_err(dev, "unknown format: %c\n", pixfmt);
		break;
	}

	/*
	 * Make sure it knows we want to use hsync/vsync.
	 */
	ccic_reg_write_mask(ccic_dev, REG_CTRL0, C0_SIF_HVSYNC, C0_SIFM_MASK);

	/* Not sure the new SoC need such code */
	/* if (sdata->bus_type == V4L2_MBUS_CSI2) */
	/* ccic_reg_set_bit(ccic_dev, REG_CTRL2, ISIM_FIX); */
	/* Not sure the following code is Ok for all sensor */
	/* ccic_reg_set_bit(ccic_dev, REG_CTRL1, C1_PWRDWN) */

	/* setup the DMA burst */
	switch (ccic_dev->dma_burst) {
	case 128:
		dma_burst = C1_DMAB128;
		break;
	case 256:
		dma_burst = C1_DMAB256;
		break;
	default:
		dma_burst = C1_DMAB64;
		break;
	}
	ccic_reg_write_mask(ccic_dev, REG_CTRL1, dma_burst, C1_DMAB_MASK);
	spin_unlock_irqrestore(&ccic_dev->ccic_lock, flags);
	return 0;
}

static void ccic_shadow_ready(struct ccic_dma_dev *dma_dev)
{
	unsigned long flags = 0;
	spin_lock_irqsave(&dma_dev->ccic_dev->ccic_lock, flags);
	ccic_reg_set_bit(dma_dev->ccic_dev, REG_CTRL1, C1_SHADOW_RDY);
	spin_unlock_irqrestore(&dma_dev->ccic_dev->ccic_lock, flags);
}

static void ccic_shadow_empty(struct ccic_dma_dev *dma_dev)
{
	unsigned long flags = 0;
	spin_lock_irqsave(&dma_dev->ccic_dev->ccic_lock, flags);
	ccic_reg_clear_bit(dma_dev->ccic_dev, REG_CTRL1, C1_SHADOW_RDY);
	spin_unlock_irqrestore(&dma_dev->ccic_dev->ccic_lock, flags);
}

static void ccic_set_yaddr(struct ccic_dma_dev *dma_dev, u32 addr)
{
	struct msc2_ccic_dev *ccic_dev = dma_dev->ccic_dev;

	ccic_reg_write(ccic_dev, REG_Y0BAR, addr);
}

static void ccic_set_uaddr(struct ccic_dma_dev *dma_dev, u32 addr)
{
	struct msc2_ccic_dev *ccic_dev = dma_dev->ccic_dev;

	ccic_reg_write(ccic_dev, REG_U0BAR, addr);
}

static void ccic_set_vaddr(struct ccic_dma_dev *dma_dev, u32 addr)
{
	struct msc2_ccic_dev *ccic_dev = dma_dev->ccic_dev;

	ccic_reg_write(ccic_dev, REG_V0BAR, addr);
}

static void ccic_set_addr(struct ccic_dma_dev *dma_dev, u8 chnl, u32 addr)
{
	struct msc2_ccic_dev *ccic_dev = dma_dev->ccic_dev;

	switch (chnl) {
	case 0:
		ccic_reg_write(ccic_dev, REG_Y0BAR, addr);
		return;
	case 1:
		ccic_reg_write(ccic_dev, REG_U0BAR, addr);
		return;
	case 2:
		ccic_reg_write(ccic_dev, REG_V0BAR, addr);
		return;
	default:
		WARN_ON(1);
	}
}

/*
 * write 1 only after all registers have been
 * initialized to proper values
 * CCIC DMA start to work by calling it
 */
static void ccic_enable(struct ccic_dma_dev *dma_dev)
{
	struct msc2_ccic_dev *ccic_dev = dma_dev->ccic_dev;
	unsigned long flags = 0;
	spin_lock_irqsave(&ccic_dev->ccic_lock, flags);
	ccic_reg_set_bit(ccic_dev, REG_CTRL0, C0_ENABLE);
	spin_unlock_irqrestore(&ccic_dev->ccic_lock, flags);
}

static void ccic_disable(struct ccic_dma_dev *dma_dev)
{
	struct msc2_ccic_dev *ccic_dev = dma_dev->ccic_dev;
	unsigned long flags = 0;
	spin_lock_irqsave(&ccic_dev->ccic_lock, flags);
	ccic_reg_clear_bit(ccic_dev, REG_CTRL0, C0_ENABLE);
	spin_unlock_irqrestore(&ccic_dev->ccic_lock, flags);
}

static struct ccic_dma_ops ccic_dma_ops = {
	.setup_image = ccic_hw_setup_image,
	.shadow_ready = ccic_shadow_ready,
	.shadow_empty = ccic_shadow_empty,
	.set_yaddr = ccic_set_yaddr,
	.set_uaddr = ccic_set_uaddr,
	.set_vaddr = ccic_set_vaddr,
	.set_addr = ccic_set_addr,
	.mbus_fmt_rating = ccic_mbus_fmt_score,
	.ccic_enable = ccic_enable,
	.ccic_disable = ccic_disable,
};


static void ccic_irqmask(struct ccic_ctrl_dev *ctrl_dev, int on)
{
	struct msc2_ccic_dev *ccic_dev = ctrl_dev->ccic_dev;
	unsigned long flags = 0;

	spin_lock_irqsave(&ccic_dev->ccic_lock, flags);
	if (on)
		ccic_frameirq_enable(ccic_dev);
	else
		ccic_frameirq_disable(ccic_dev);
	spin_unlock_irqrestore(&ccic_dev->ccic_lock, flags);
}

static struct csi_dphy_calc dphy_calc_profiles[] = {
	{
		.hs_termen_pos	= 0,
		.hs_settle_pos	= 50,
	},
};

static int ccic_calc_dphy(struct csi_dphy_desc *desc,
		struct csi_dphy_calc *algo,
		struct csi_dphy_reg *reg)
{
	u32 ps_period, ps_ui, ps_termen_max, ps_prep_max, ps_prep_min;
	u32 ps_sot_min, ps_termen, ps_settle;
	ps_period = MHZ * 1000 / (desc->clk_freq / 1000);
	ps_ui = ps_period / 2;
	ps_termen_max	= NS_TO_PS(D_TERMEN_MAX) + 4 * ps_ui;
	ps_prep_min	= NS_TO_PS(HS_PREP_MIN) + 4 * ps_ui;
	ps_prep_max	= NS_TO_PS(HS_PREP_MAX) + 6 * ps_ui;
	ps_sot_min	= NS_TO_PS(HS_PREP_ZERO_MIN) + 10 * ps_ui;
	ps_termen	= ps_termen_max + algo->hs_termen_pos * ps_period;
	ps_settle	= NS_TO_PS(desc->hs_prepare + desc->hs_zero *
			algo->hs_settle_pos / HS_SETTLE_POS_MAX);

	reg->cl_termen	= 0x01;
	reg->cl_settle	= 0x10;
	reg->cl_miss	= 0x00;
	/* term_en = round_up(ps_termen / ps_period) - 1 */
	reg->hs_termen	= (ps_termen + ps_period - 1) / ps_period - 1;
	/* For Marvell DPHY, Ths-settle started from HS-0, not VILmax */
	ps_settle -= (reg->hs_termen + 1) * ps_period;
	/* DE recommend this value reset to zero */
	reg->hs_termen  = 0x0;
	/* round_up(ps_settle / ps_period) - 1 */
	reg->hs_settle	= (ps_settle + ps_period - 1) / ps_period - 1;
	reg->hs_rx_to	= 0xFFFF;
	reg->lane	= desc->nr_lane;
	return 0;
}

static int ccic_config_mipi(struct msc2_ccic_dev *ccic_dev,
		struct mipi_csi2 *csi, int enable)
{
	unsigned int dphy3_val = 0;
	unsigned int dphy5_val = 0;
	unsigned int dphy6_val = 0;
	unsigned int ctrl0_val = 0;
	struct csi_dphy_reg dphy_reg;
	int lanes = csi->dphy_desc.nr_lane;

	if (!enable)
		goto out;

	if ((lanes < 1) || (lanes > 4)) {
		dev_err(ccic_dev->dev, "wrong lanes num %d\n", lanes);
		return -EINVAL;
	}
	if (csi->calc_dphy) {
		ccic_calc_dphy(&csi->dphy_desc,
				dphy_calc_profiles, &dphy_reg);

		dphy3_val = dphy_reg.hs_settle & 0xFF;
		dphy3_val = dphy_reg.hs_termen |
			(dphy3_val << CSI2_DPHY3_HS_SETTLE_SHIFT);

		dphy6_val = dphy_reg.cl_settle & 0xFF;
		dphy6_val = dphy_reg.cl_termen |
			(dphy6_val << CSI2_DPHY6_CK_SETTLE_SHIFT);

		dphy5_val = CSI2_DPHY5_LANE_ENA(lanes);
		dphy5_val = dphy5_val |
			(dphy5_val << CSI2_DPHY5_LANE_RESC_ENA_SHIFT);
	} else {
		dphy3_val = csi->dphy[0];
		dphy5_val = csi->dphy[1];
		dphy6_val = csi->dphy[2];
	}

	ctrl0_val = ccic_reg_read(ccic_dev, REG_CSI2_CTRL0);
	ctrl0_val &= ~(CSI2_C0_LANE_NUM_MASK);
	ctrl0_val |= CSI2_C0_LANE_NUM(lanes);
	ctrl0_val |= CSI2_C0_ENABLE;
	ctrl0_val &= ~(CSI2_C0_VLEN_MASK);
	ctrl0_val |= CSI2_C0_VLEN;

out:
	ccic_reg_write(ccic_dev, REG_CSI2_DPHY3, dphy3_val);
	ccic_reg_write(ccic_dev, REG_CSI2_DPHY5, dphy5_val);
	ccic_reg_write(ccic_dev, REG_CSI2_DPHY6, dphy6_val);
	ccic_reg_write(ccic_dev, REG_CSI2_CTRL0, ctrl0_val);

	return 0;
}

static void ccic_config_mbus(struct ccic_ctrl_dev *ctrl_dev,
			enum v4l2_mbus_type bus_type,
			unsigned int mbus_flags, int enable)
{
	struct msc2_ccic_dev *ccic_dev = ctrl_dev->ccic_dev;

	if (bus_type == V4L2_MBUS_PARALLEL) {
		if (mbus_flags & V4L2_MBUS_HSYNC_ACTIVE_LOW)
			ccic_reg_set_bit(ccic_dev, REG_CTRL0, C0_HPOL_LOW);
		else
			ccic_reg_clear_bit(ccic_dev, REG_CTRL0, C0_HPOL_LOW);
		if (mbus_flags & V4L2_MBUS_VSYNC_ACTIVE_LOW)
			ccic_reg_set_bit(ccic_dev, REG_CTRL0, C0_VPOL_LOW);
		else
			ccic_reg_clear_bit(ccic_dev, REG_CTRL0, C0_VPOL_LOW);
		if (mbus_flags & V4L2_MBUS_PCLK_SAMPLE_FALLING)
			ccic_reg_set_bit(ccic_dev, REG_CTRL0, C0_VCLK_LOW);
		else
			ccic_reg_clear_bit(ccic_dev, REG_CTRL0, C0_VCLK_LOW);
	} else if (bus_type == V4L2_MBUS_CSI2)
		ccic_config_mipi(ccic_dev, &ctrl_dev->csi, enable);
}

static void ccic_config_idi(struct ccic_ctrl_dev *ctrl_dev, int sel)
{
	struct msc2_ccic_dev *ccic_dev = ctrl_dev->ccic_dev;

	switch (sel) {
	case SC2_IDI_SEL_NONE:
		ccic_reg_clear_bit(ccic_dev, REG_IDI_CTRL, IDI_RELEASE_RESET);
		ccic_reg_set_bit(ccic_dev, REG_CSI2_CTRL2, CSI2_C2_REPACK_RST);
		ccic_reg_clear_bit(ccic_dev,
				REG_CSI2_CTRL2, CSI2_C2_REPACK_ENA);
		ccic_reg_clear_bit(ccic_dev, REG_CSI2_CTRL2, CSI2_C2_DPCM_ENA);
		ccic_reg_clear_bit(ccic_dev,
				REG_CSI2_CTRL2, CSI2_C2_DPCM_REPACK_MUX_SEL_OTHER);
		return;
	case SC2_IDI_SEL_REPACK:
		ccic_reg_write_mask(ccic_dev, REG_IDI_CTRL,
				IDI_SEL_DPCM_REPACK, IDI_SEL_MASK);
		ccic_reg_clear_bit(ccic_dev,
				REG_CSI2_CTRL2, CSI2_C2_IDI_MUX_SEL_DPCM);
		ccic_reg_clear_bit(ccic_dev,
				REG_CSI2_CTRL2, CSI2_C2_REPACK_RST);
		ccic_reg_set_bit(ccic_dev, REG_CSI2_CTRL2, CSI2_C2_REPACK_ENA);
		break;
	case SC2_IDI_SEL_DPCM:
		ccic_reg_write_mask(ccic_dev, REG_IDI_CTRL,
				IDI_SEL_DPCM_REPACK, IDI_SEL_MASK);
		ccic_reg_set_bit(ccic_dev,
				REG_CSI2_CTRL2, CSI2_C2_IDI_MUX_SEL_DPCM);
		ccic_reg_set_bit(ccic_dev, REG_CSI2_CTRL2, CSI2_C2_DPCM_ENA);
		break;
	case SC2_IDI_SEL_PARALLEL:
		ccic_reg_write_mask(ccic_dev, REG_IDI_CTRL,
			IDI_SEL_PARALLEL, IDI_SEL_MASK);
		break;
	case SC2_IDI_SEL_REPACK_OTHER:
		ccic_reg_set_bit(ccic_dev,
				REG_CSI2_CTRL2, CSI2_C2_DPCM_REPACK_MUX_SEL_OTHER);
		break;
	default:
		dev_err(ccic_dev->dev, "IDI source is error %d\n", sel);
		return;
	}

	ccic_reg_set_bit(ccic_dev, REG_IDI_CTRL, IDI_RELEASE_RESET);
}

static void ccic_power_up(struct ccic_ctrl_dev *ctrl_dev)
{}

static void ccic_power_down(struct ccic_ctrl_dev *ctrl_dev)
{}

/*
 * TBD: calculate the clk rate dynamically based on
 * fps, resolution and other arguments.
 */
static void ccic_clk_set_rate(struct ccic_ctrl_dev *ctrl_dev)
{
	struct msc2_ccic_dev *ccic_dev = ctrl_dev->ccic_dev;

	clk_set_rate(ctrl_dev->csi_clk, 312000000);
	clk_set_rate(ctrl_dev->clk4x, 312000000);
	if (ccic_dev->ahb_enable)
		clk_set_rate(ctrl_dev->ahb_clk, 156000000);
	return;
}

static void ccic_clk_enable(struct ccic_ctrl_dev *ctrl_dev)
{
	struct msc2_ccic_dev *ccic_dev = ctrl_dev->ccic_dev;
	ccic_clk_set_rate(ctrl_dev);
	clk_prepare_enable(ctrl_dev->dphy_clk);
	clk_prepare_enable(ctrl_dev->csi_clk);
	clk_prepare_enable(ctrl_dev->clk4x);
	if (ccic_dev->ahb_enable)
		clk_prepare_enable(ctrl_dev->ahb_clk);
}

static void ccic_clk_disable(struct ccic_ctrl_dev *ctrl_dev)
{
	struct msc2_ccic_dev *ccic_dev = ctrl_dev->ccic_dev;

	if (ccic_dev->ahb_enable)
		clk_disable_unprepare(ctrl_dev->ahb_clk);
	clk_disable_unprepare(ctrl_dev->clk4x);
	clk_disable_unprepare(ctrl_dev->csi_clk);
	clk_disable_unprepare(ctrl_dev->dphy_clk);
}

static void ccic_clk_change(struct ccic_ctrl_dev *ctrl_dev,
					u32 mipi_bps, u8 lanes, u8 bpp)
{
	u32 rate;

	/* csi clk > mipi bps * lanes / 8*/
	rate = (mipi_bps >> 3) * lanes;
	if (rate < 208000000)
		rate = 208000000;
	else if (rate < 312000000)
		rate = 312000000;
	else if (rate < 416000000)
		rate = 416000000;
	else
		rate = 624000000;

	clk_set_rate(ctrl_dev->csi_clk, rate);

	/*
	 * CCIC_CLK4X * BPP > CSI_CLK
	 * rate = csi_clk * 8 / bpp
	 */
	rate = mipi_bps / bpp * lanes;
	if (rate < 208000000)
		rate = 208000000;
	else if (rate < 312000000)
		rate = 312000000;
	else if (rate < 416000000)
		rate = 416000000;
	else
		rate = 624000000;

	clk_set_rate(ctrl_dev->clk4x, rate);

	pr_debug("csi rate %lu\n", clk_get_rate(ctrl_dev->csi_clk));
	pr_debug("4x rate %lu\n", clk_get_rate(ctrl_dev->clk4x));
}

static struct ccic_ctrl_ops ccic_ctrl_ops = {
	.irq_mask = ccic_irqmask,
	.config_mbus = ccic_config_mbus,
	.config_idi = ccic_config_idi,
	.power_up = ccic_power_up,
	.power_down = ccic_power_down,
	.clk_enable = ccic_clk_enable,
	.clk_disable = ccic_clk_disable,
	.clk_change = ccic_clk_change,
};

static int ccic_init_clk(struct ccic_ctrl_dev *ctrl_dev)
{
	struct msc2_ccic_dev *ccic_dev = ctrl_dev->ccic_dev;
	ctrl_dev->dphy_clk = devm_clk_get(&ccic_dev->pdev->dev, "SC2DPHYCLK");
	if (IS_ERR(ctrl_dev->dphy_clk))
		return PTR_ERR(ctrl_dev->dphy_clk);
	ctrl_dev->csi_clk = devm_clk_get(&ccic_dev->pdev->dev, "SC2CSICLK");
	if (IS_ERR(ctrl_dev->csi_clk))
		return PTR_ERR(ctrl_dev->csi_clk);
	ctrl_dev->clk4x = devm_clk_get(&ccic_dev->pdev->dev, "SC24XCLK");
	if (IS_ERR(ctrl_dev->clk4x))
		return PTR_ERR(ctrl_dev->clk4x);
	if (ccic_dev->ahb_enable) {
		ctrl_dev->ahb_clk =
			devm_clk_get(&ccic_dev->pdev->dev, "SC2AHBCLK");
		if (IS_ERR(ctrl_dev->ahb_clk))
			return PTR_ERR(ctrl_dev->ahb_clk);
	}

	return 0;
}

static int ccic_device_register(struct msc2_ccic_dev *ccic_dev)
{
	struct msc2_ccic_dev *other;

	mutex_lock(&list_lock);
	list_for_each_entry(other, &ccic_devices, list) {
		if (other->id == ccic_dev->id) {
			dev_warn(ccic_dev->dev,
				"dma dev id %d already registered\n",
				ccic_dev->id);
			mutex_unlock(&list_lock);
			return -EBUSY;
		}
	}

	list_add_tail(&ccic_dev->list, &ccic_devices);
	mutex_unlock(&list_lock);
	return 0;
}

static int ccic_device_unregister(struct msc2_ccic_dev *ccic_dev)
{
	mutex_lock(&list_lock);
	list_del(&ccic_dev->list);
	mutex_unlock(&list_lock);
	return 0;
}

int msc2_get_ccic_dma(struct ccic_dma_dev **dma_host, int id,
		irqreturn_t (*handler)(struct ccic_dma_dev *, u32))
{
	struct msc2_ccic_dev *ccic_dev = NULL;
	struct ccic_dma_dev *dma_dev;

	list_for_each_entry(ccic_dev, &ccic_devices, list) {
		if (ccic_dev->id == id)
			break;
	}

	if (ccic_dev->dma_dev == NULL)
		return -ENODEV;

	dma_dev = ccic_dev->dma_dev;
	*dma_host = dma_dev;
	dma_dev->usr_cnt++;
	dma_dev->handler = handler;
	d_inf(4, "acquire ccic dma dev succeed\n");
	return 0;
}
EXPORT_SYMBOL(msc2_get_ccic_dma);

void msc2_put_ccic_dma(struct ccic_dma_dev **dma_dev)
{
	(*dma_dev)->usr_cnt--;
	(*dma_dev)->handler = NULL;
	(*dma_dev)->priv = NULL;
}
EXPORT_SYMBOL(msc2_put_ccic_dma);

int msc2_get_ccic_dev(struct msc2_ccic_dev **ccic_host, int id)
{
	struct msc2_ccic_dev *ccic_dev = NULL;

	/*Here solve ccic2 share ccic1 pin */
	list_for_each_entry(ccic_dev, &ccic_devices, list) {
		if (ccic_dev->sync_ccic1_pin)
			id = 0;
	}

	list_for_each_entry(ccic_dev, &ccic_devices, list) {
		if (ccic_dev->id == id)
			break;
	}

	if (ccic_dev == NULL)
		return -ENODEV;

	*ccic_host = ccic_dev;
	d_inf(4, "acquire ccic dev succeed\n");
	return 0;
}
EXPORT_SYMBOL(msc2_get_ccic_dev);

int msc2_get_ccic_ctrl(struct ccic_ctrl_dev **ctrl_host, int id,
		irqreturn_t (*handler)(struct ccic_ctrl_dev *, u32))
{
	struct msc2_ccic_dev *ccic_dev = NULL;
	struct ccic_ctrl_dev *ctrl_dev;

	list_for_each_entry(ccic_dev, &ccic_devices, list) {
		if (ccic_dev->id == id)
			break;
	}

	if (ccic_dev->ctrl_dev == NULL)
		return -ENODEV;

	ctrl_dev = ccic_dev->ctrl_dev;
	*ctrl_host = ctrl_dev;
	ctrl_dev->usr_cnt++;
	ctrl_dev->handler = handler;
	d_inf(4, "acquire ccic ctrl dev succeed\n");
	return 0;
}
EXPORT_SYMBOL(msc2_get_ccic_ctrl);

void msc2_put_ccic_ctrl(struct ccic_ctrl_dev **ctrl_dev)
{
	(*ctrl_dev)->usr_cnt--;
	(*ctrl_dev)->handler = NULL;
	(*ctrl_dev)->priv = NULL;
}
EXPORT_SYMBOL(msc2_put_ccic_ctrl);

static irqreturn_t ccic_irqhandler(int irq, void *data)
{
	struct msc2_ccic_dev *ccic_dev = data;
	struct ccic_dma_dev *dma_dev = ccic_dev->dma_dev;
	struct ccic_ctrl_dev *ctrl_dev = ccic_dev->ctrl_dev;
	irqreturn_t handled = IRQ_NONE;
	u32 irqs;

	if (dma_dev->usr_cnt == 0 &&
		ctrl_dev->usr_cnt == 0)
		return IRQ_NONE;

	if (dma_dev->handler == NULL &&
		ctrl_dev->handler == NULL)
		return IRQ_NONE;

	irqs = ccic_reg_read(ccic_dev, REG_IRQSTAT);
	if (!(irqs & ALLIRQS))
		return IRQ_NONE;

	/* not sure here to clear is the best point */
	ccic_reg_write(ccic_dev, REG_IRQSTAT, irqs);

	/* If overflow, need set following bit for auto-recovery */
	if (irqs & IRQ_OVERFLOW)
		ccic_reg_set_bit(ccic_dev, REG_CTRL0, C0_EOFFLUSH);

	if (dma_dev->handler)
		handled = dma_dev->handler(dma_dev, irqs);
	if (ctrl_dev->handler)
		handled |= ctrl_dev->handler(ctrl_dev, irqs);

	/* we will never use IRQ_WAKE_THREAD */
	return IRQ_RETVAL(handled);
}

static int msc2_ccic_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct msc2_ccic_dev *ccic_dev;
	struct resource *res;
	struct ccic_dma_dev *dma_dev;
	struct ccic_ctrl_dev *ctrl_dev;
	struct device *dev = &pdev->dev;
	void __iomem *base;
	int ret;
	int len;
	const u32 *tmp;
	u32 ahb_enable;

	ret = of_alias_get_id(np, "mv_sc2_ccic");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get alias id, errno %d\n", ret);
		return ret;
	}
	pdev->id = ret;
	d_inf(4, "camera: probing CCIC%d\n", pdev->id + 1);

	ccic_dev = devm_kzalloc(&pdev->dev, sizeof(*ccic_dev), GFP_KERNEL);
	if (!ccic_dev) {
		dev_err(&pdev->dev, "camera: Could not allocate ccic dev\n");
		return -ENOMEM;
	}

	dma_dev = devm_kzalloc(&pdev->dev, sizeof(*dma_dev), GFP_KERNEL);
	if (!dma_dev) {
		dev_err(&pdev->dev, "camera: Could not allocate dma dev\n");
		return -ENOMEM;
	}

	ctrl_dev = devm_kzalloc(&pdev->dev, sizeof(*ctrl_dev), GFP_KERNEL);
	if (!ctrl_dev) {
		dev_err(&pdev->dev, "camera: Could not allocate ctrl dev\n");
		return -ENOMEM;
	}

	dma_dev->ccic_dev = ccic_dev;
	dma_dev->id = pdev->id;
	dma_dev->ops = &ccic_dma_ops;
	ctrl_dev->ccic_dev = ccic_dev;
	ctrl_dev->id = pdev->id;
	ctrl_dev->ops = &ccic_ctrl_ops;

	ccic_dev->id = pdev->id;
	ccic_dev->pdev = pdev;
	ccic_dev->dev = &pdev->dev;
	ccic_dev->dma_dev = dma_dev;
	ccic_dev->ctrl_dev = ctrl_dev;
	dev_set_drvdata(dev, ccic_dev);

	INIT_LIST_HEAD(&ccic_dev->list);
	spin_lock_init(&ccic_dev->ccic_lock);

	if (!of_property_read_u32(np, "ahb_enable", &ahb_enable))
		ccic_dev->ahb_enable = ahb_enable;

	ret = ccic_init_clk(ctrl_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to init clk\n");
		return ret;
	}

	tmp = of_get_property(np, "dma-burst", &len);
	if (!tmp)
		ccic_dev->dma_burst = 256;	/* set the default value */
	else
		ccic_dev->dma_burst = be32_to_cpup(tmp);

	ccic_dev->sync_ccic1_pin = 0;
	if (of_get_property(np, "sync_ccic1_pin", NULL))
		ccic_dev->sync_ccic1_pin = 1;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		dev_err(&pdev->dev,
			"Failed to request and remap io memory\n");
		return PTR_ERR(base);
	}
	ccic_dev->res = res;
	ccic_dev->base = base;

	ret = platform_get_irq(pdev, 0);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get irq resource\n");
		return -ENXIO;
	}
	ccic_dev->irq = ret;

	/* TBD mccic_init_clk(ccic_dev); */
	ret = devm_request_irq(&ccic_dev->pdev->dev, ccic_dev->irq,
				ccic_irqhandler,
				IRQF_SHARED, MV_SC2_CCIC_NAME, ccic_dev);
	if (ret) {
		dev_err(&ccic_dev->pdev->dev, "IRQ request failed\n");
		return ret;
	}

	ccic_device_register(ccic_dev);
	/* TBD setup some coupled register */

	return ret;
}

static int msc2_ccic_remove(struct platform_device *pdev)
{
	struct msc2_ccic_dev *ccic_dev;

	ccic_dev = dev_get_drvdata(&pdev->dev);
	/* 1. close all devices??? */
	ccic_device_unregister(ccic_dev);
	/* 3. free ccic resource if needed */
	d_inf(4, "camera driver unloaded\n");

	return 0;
}

static const struct of_device_id mv_sc2_ccic_dt_match[] = {
	{ .compatible = "marvell,mmp-sc2ccic", .data = NULL },
	{},
};
MODULE_DEVICE_TABLE(of, mv_sc2_ccic_dt_match);

static struct platform_driver msc2_ccic_driver = {
	.driver = {
		.name = MV_SC2_CCIC_NAME,
		.of_match_table = of_match_ptr(mv_sc2_ccic_dt_match),
	},
	.probe = msc2_ccic_probe,
	.remove = msc2_ccic_remove,
};

module_platform_driver(msc2_ccic_driver);
