/*
 * V4L2 Driver for Marvell Mobile SoC PXA910/PXA688/PXA2128 CCIC
 * (CMOS Camera Interface Controller)
 *
 * This driver is based on soc_camera and videobuf2 framework,
 * but part of the low level register function is base on cafe-driver.c
 *
 * Copyright 2006-2011 One Laptop Per Child Association, Inc.
 * Copyright 2006-2011 Jonathan Corbet <corbet@lwn.net>
 *
 * Copyright (C) 2011-2012, Marvell International Ltd.
 *	Kassey Lee <ygli@marvell.com>
 *	Angela Wan <jwan@marvell.com>
 *	Albert Wang <twang13@marvell.com>
 *	Lei Wen <leiwen@marvell.com>
 *	Fangsuo Wu <fswu@marvell.com>
 *	Sarah Zhang <xiazh@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/videodev2.h>
#include <linux/pm_qos.h>

#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-chip-ident.h>

#include <mach/regs-apmu.h>
#include <mach/camera.h>

#if defined(CONFIG_SOC_CAMERA_SR030PC50)
#include <mach/camera-sr030pc50.h>
#include <mach/regs-mcu.h>
#ifdef CONFIG_PM_DEVFREQ
#include <plat/devfreq.h>
#endif
#elif defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
#include "mmp_camera.h"
#include "s5k4ecgx.h"
#include <mach/regs-mcu.h>
#ifdef CONFIG_PM_DEVFREQ
#include <plat/devfreq.h>
#endif
#else
#include <mach/samsung_camera.h>
#endif

#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
#include <mach/samsung_camera_lt02.h>
#endif

#include "mmp_camera.h"

#define MMP_CAM_DRV_NAME "mmp-camera"
int ssg_width;
int ssg_height;
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
extern int sr352_cam_state;
#endif

static const struct soc_mbus_pixelfmt ccic_formats[] = {
	{
		.fourcc	= V4L2_PIX_FMT_UYVY,
		.name = "YUV422PACKED",
		.bits_per_sample = 8,
		.packing = SOC_MBUS_PACKING_2X8_PADLO,
		.order = SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc = V4L2_PIX_FMT_YUV422P,
		.name = "YUV422PLANAR",
		.bits_per_sample = 8,
		.packing = SOC_MBUS_PACKING_2X8_PADLO,
		.order = SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc = V4L2_PIX_FMT_YUV420,
		.name = "YUV420PLANAR",
		.bits_per_sample = 12,
		.packing = SOC_MBUS_PACKING_NONE,
		.order = SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc = V4L2_PIX_FMT_YVU420,
		.name = "YVU420PLANAR",
		.bits_per_sample = 12,
		.packing = SOC_MBUS_PACKING_NONE,
		.order = SOC_MBUS_ORDER_LE,
	},
};

static void __attribute__((unused)) dump_register(struct mmp_camera_dev *pcdev)
{
	unsigned int ret;

	/*
	 * CCIC IRQ REG
	 */
	ret = ccic_reg_read(pcdev, REG_IRQSTAT);
	printk(KERN_INFO "CCIC: REG_IRQSTAT is 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_IRQSTATRAW);
	printk(KERN_INFO "CCIC: REG_IRQSTATRAW is 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_IRQMASK);
	printk(KERN_INFO "CCIC: REG_IRQMASK is 0x%08x\n\n", ret);

	/*
	 * CCIC IMG REG
	 */
	ret = ccic_reg_read(pcdev, REG_IMGPITCH);
	printk(KERN_INFO "CCIC: REG_IMGPITCH is 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_IMGSIZE);
	printk(KERN_INFO "CCIC: REG_IMGSIZE is 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_IMGOFFSET);
	printk(KERN_INFO "CCIC: REG_IMGOFFSET is 0x%08x\n\n", ret);

	/*
	 * CCIC CTRL REG
	 */
	ret = ccic_reg_read(pcdev, REG_CTRL0);
	printk(KERN_INFO "CCIC: REG_CTRL0 is 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_CTRL1);
	printk(KERN_INFO "CCIC: REG_CTRL1 is 0x%08x\n", ret);
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	ret = ccic_reg_read(pcdev, REG_CTRL3);
	printk(KERN_INFO "CCIC: REG_CTRL3 is 0x%08x\n", ret);
#endif
	ret = ccic_reg_read(pcdev, REG_CLKCTRL);
	printk(KERN_INFO "CCIC: REG_CLKCTRL is 0x%08x\n\n", ret);

	/*
	 * CCIC CSI2 REG
	 */
	ret = ccic_reg_read(pcdev, REG_CSI2_DPHY3);
	printk(KERN_INFO "CCIC: REG_CSI2_DPHY3 is 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_CSI2_DPHY5);
	printk(KERN_INFO "CCIC: REG_CSI2_DPHY5 is 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_CSI2_DPHY6);
	printk(KERN_INFO "CCIC: REG_CSI2_DPHY6 is 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_CSI2_CTRL0);
	printk(KERN_INFO "CCIC: REG_CSI2_CTRL0 is 0x%08x\n\n", ret);

	/*
	 * CCIC YUV REG
	 */
	ret = ccic_reg_read(pcdev, REG_Y0BAR);
	printk(KERN_INFO "CCIC: REG_Y0BAR 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_U0BAR);
	printk(KERN_INFO "CCIC: REG_U0BAR 0x%08x\n", ret);
	ret = ccic_reg_read(pcdev, REG_V0BAR);
	printk(KERN_INFO "CCIC: REG_V0BAR 0x%08x\n\n", ret);

	/*
	 * CCIC APMU REG
	 */
	if (pcdev->pdev->id == 0) {
		/*
		 * CCIC1 APMU REG
		 */
		ret = readl(APMU_CCIC_GATE);
		printk(KERN_INFO "CCIC: APMU_CCIC_GATE 0x%08x\n", ret);
		ret = readl(APMU_CCIC_RST);
		printk(KERN_INFO "CCIC: APMU_CCIC_RST 0x%08x\n", ret);
	} else if (pcdev->pdev->id == 1) {
		/*
		 * CCIC2 APMU REG
		 */
		ret = readl(APMU_CCIC2_GATE);
		printk(KERN_INFO "CCIC: APMU_CCIC2_GATE 0x%08x\n", ret);
		ret = readl(APMU_CCIC2_RST);
		printk(KERN_INFO "CCIC: APMU_CCIC2_RST 0x%08x\n", ret);
	}
	ret = readl(APMU_CCIC_DBG);
	printk(KERN_INFO "CCIC: APMU_DEBUG 0x%08x\n\n", ret);
}

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
static void ccic_ctlr_reset(struct mmp_camera_dev *pcdev);
static void ccic_stop(struct mmp_camera_dev *pcdev);
static void ccic_start(struct mmp_camera_dev *pcdev);

static void csi_dphy_write(void *hw_ctx, const struct csi_dphy_reg *regs)
{
	struct mmp_camera_dev *pcdev = hw_ctx;
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;

	if (pdata->bus_type == V4L2_MBUS_CSI2_LANES) {
		u32 regval = 0;

		/* Disable CCIC */
		ccic_stop(pcdev);

		/* reset MIPI */
		ccic_ctlr_reset(pcdev);

		regval = regs->hs_settle & 0xFF;
		regval = regs->hs_termen | (regval << 8);
		ccic_reg_write(pcdev, REG_CSI2_DPHY3, regval);

		regval = regs->cl_settle & 0xFF;
		regval = regs->cl_termen | (regval << 8);
		ccic_reg_write(pcdev, REG_CSI2_DPHY6, regval);

		regval = (1 << regs->lane) - 1;
		regval = regval | (regval << 4);
		ccic_reg_write(pcdev, REG_CSI2_DPHY5, regval);

		regval = (regs->lane - 1) & 0x03;
		regval = (regval << 1) | 0x41;
		ccic_reg_write(pcdev, REG_CSI2_CTRL0, regval);

		/* Enable CCIC */
		ccic_start(pcdev);
	} else {
		ccic_reg_write(pcdev, REG_CSI2_DPHY3, 0x0);
		ccic_reg_write(pcdev, REG_CSI2_DPHY6, 0x0);
		ccic_reg_write(pcdev, REG_CSI2_DPHY5, 0x0);
		ccic_reg_write(pcdev, REG_CSI2_CTRL0, 0x0);
		pdata->mipi_enabled = 0;
	}
};

static void csi_dphy_read(void *hw_ctx, struct csi_dphy_reg *regs)
{
	struct mmp_camera_dev *pcdev = hw_ctx;
	u32 phy3, phy5, phy6;

	phy3	= ccic_reg_read(pcdev, REG_CSI2_DPHY3);
	phy5	= ccic_reg_read(pcdev, REG_CSI2_DPHY5);
	phy6	= ccic_reg_read(pcdev, REG_CSI2_DPHY6);

	regs->cl_termen	= phy6 & 0xFF;
	regs->cl_settle	= (phy6>>8) & 0xFF;
	regs->cl_miss	= 0;
	regs->hs_termen = phy3 & 0xFF;
	regs->hs_settle	= (phy3>>8) & 0xFF;
	regs->hs_rx_to	= 0xFFFF;
	regs->lane	= 0;
	phy5 &= 0xF;
	while (phy5) {
		phy5 = phy5 & (phy5-1);
		regs->lane++;
	}
	return;
};
#endif

static int ccic_config_phy(struct mmp_camera_dev *pcdev, int enable)
{
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	struct device *dev = &pcdev->pdev->dev;
	int ret = 0;

	if (pdata->bus_type == V4L2_MBUS_CSI2_LANES && enable) {
		dev_dbg(dev, "camera: DPHY3=0x%x, DPHY5=0x%x, DPHY6=0x%x\n",
			pdata->dphy[0], pdata->dphy[1], pdata->dphy[2]);
		ccic_reg_write(pcdev, REG_CSI2_DPHY3, pdata->dphy[0]);
		ccic_reg_write(pcdev, REG_CSI2_DPHY6, pdata->dphy[2]);
		ccic_reg_write(pcdev, REG_CSI2_DPHY5, pdata->dphy[1]);
		if (pdata->mipi_enabled == 0) {
			/*
			 * 0x41 actives 1 lane
			 * 0x43 actives 2 lanes
			 * 0x47 actives 4 lanes
			 * There is no 3 lanes case
			 */
			if (pdata->lane == 1)
				ccic_reg_write(pcdev, REG_CSI2_CTRL0, 0x41);
			else if (pdata->lane == 2)
				ccic_reg_write(pcdev, REG_CSI2_CTRL0, 0x43);
			else if (pdata->lane == 4)
				ccic_reg_write(pcdev, REG_CSI2_CTRL0, 0x47);
			else {
				dev_err(dev, "camera: board config wrong lane number!");
				return -EINVAL;
			}
			pdata->mipi_enabled = 1;
		}
	} else {
		ccic_reg_write(pcdev, REG_CSI2_DPHY3, 0x0);
		ccic_reg_write(pcdev, REG_CSI2_DPHY6, 0x0);
		ccic_reg_write(pcdev, REG_CSI2_DPHY5, 0x0);
		ccic_reg_write(pcdev, REG_CSI2_CTRL0, 0x0);
		pdata->mipi_enabled = 0;
	}

	return ret;
}

static void ccic_enable_clk(struct mmp_camera_dev *pcdev)
{
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	struct device *dev = &pcdev->pdev->dev;
	int ctrl1 = 0;

	pdata->enable_clk(dev, 1);
	ccic_reg_write(pcdev, REG_CLKCTRL,
			(pdata->mclk_src << 29) | pdata->mclk_div);
	dev_dbg(dev, "camera: set sensor mclk = %d MHz\n", pdata->mclk_min);

	switch (pdata->dma_burst) {
	case 128:
		ctrl1 = C1_DMAB128;
		break;
	case 256:
		ctrl1 = C1_DMAB256;
		break;
	}
	ccic_reg_write(pcdev, REG_CTRL1, ctrl1 | C1_RESERVED | C1_DMAPOSTED);
	if (pdata->bus_type != V4L2_MBUS_CSI2_LANES)
		ccic_reg_write(pcdev, REG_CTRL3, 0x4);
}

static void ccic_disable_clk(struct mmp_camera_dev *pcdev)
{
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;

	ccic_reg_write(pcdev, REG_CLKCTRL, 0x0);
	/*
	 * Bit[5:1] reserved and should not be changed
	 */
	ccic_reg_write(pcdev, REG_CTRL1, C1_RESERVED);
	pdata->enable_clk(&pcdev->pdev->dev, 0);
}

static int ccic_config_image(struct mmp_camera_dev *pcdev)
{
	struct v4l2_pix_format *fmt = &pcdev->pix_format;
	struct device *dev = &pcdev->pdev->dev;
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	u32 widthy = 0, widthuv = 0, imgsz_h, imgsz_w;
	int ret = 0;

	dev_dbg(dev, "camera: bytesperline = %d; height = %d\n",
		fmt->bytesperline, fmt->sizeimage / fmt->bytesperline);
	imgsz_h = (fmt->height << IMGSZ_V_SHIFT) & IMGSZ_V_MASK;
	imgsz_w = fmt->bytesperline & IMGSZ_H_MASK;

	if (fmt->pixelformat == V4L2_PIX_FMT_YUV420
		|| fmt->pixelformat == V4L2_PIX_FMT_YVU420)
		imgsz_w = (fmt->bytesperline * 4 / 3) & IMGSZ_H_MASK;
	else if (fmt->pixelformat == V4L2_PIX_FMT_JPEG)
		imgsz_h = (fmt->sizeimage / fmt->bytesperline) << IMGSZ_V_SHIFT;

	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
		widthy = fmt->width * 2;
		widthuv = fmt->width * 2;
		break;
	case V4L2_PIX_FMT_RGB565:
		widthy = fmt->width * 2;
		widthuv = 0;
		break;
	case V4L2_PIX_FMT_JPEG:
		widthy = fmt->bytesperline;
		widthuv = fmt->bytesperline;
		break;
	case V4L2_PIX_FMT_YUV422P:
		widthy = fmt->width;
		widthuv = fmt->width / 2;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		widthy = fmt->width;
		widthuv = fmt->width / 2;
		break;
	default:
		break;
	}

	ccic_reg_write(pcdev, REG_IMGPITCH, widthuv << 16 | widthy);
	ccic_reg_write(pcdev, REG_IMGSIZE, imgsz_h | imgsz_w);
	ccic_reg_write(pcdev, REG_IMGOFFSET, 0x0);

	/*
	 * Tell the controller about the image format we are using.
	 */
	switch (fmt->pixelformat) {
	case V4L2_PIX_FMT_YUV422P:
		ccic_reg_write_mask(pcdev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_PLANAR | C0_YUVE_YVYU, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		ccic_reg_write_mask(pcdev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_420PL | C0_YUVE_YVYU, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_YUYV:
		ccic_reg_write_mask(pcdev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_PACKED | C0_YUVE_UYVY, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_UYVY:
		ccic_reg_write_mask(pcdev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_PACKED | C0_YUVE_YUYV, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_JPEG:
		ccic_reg_write_mask(pcdev, REG_CTRL0,
			C0_DF_YUV | C0_YUV_PACKED | C0_YUVE_YUYV, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_RGB444:
		ccic_reg_write_mask(pcdev, REG_CTRL0,
			C0_DF_RGB | C0_RGBF_444 | C0_RGB4_XRGB, C0_DF_MASK);
		break;
	case V4L2_PIX_FMT_RGB565:
		ccic_reg_write_mask(pcdev, REG_CTRL0,
			C0_DF_RGB | C0_RGBF_565 | C0_RGB5_BGGR, C0_DF_MASK);
		break;
	default:
		dev_err(dev, "camera: unknown format: %c\n", fmt->pixelformat);
		break;
	}

	/*
	 * Make sure it knows we want to use hsync/vsync.
	 */
	ccic_reg_write_mask(pcdev, REG_CTRL0, C0_SIF_HVSYNC, C0_SIFM_MASK);
	/*
	 * This field controls the generation of EOF(DVP only)
	 */
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	if (pdata->bus_type != V4L2_MBUS_CSI2_LANES) {
		
		ccic_reg_set_bit(pcdev, REG_CTRL0, C0_VPOL_LOW | C0_VEDGE_CTRL);
		dev_info(dev, "camera: ccic_config_image set the REG_CTRL0 for parallel sensor!!!\n");
	}
#else
	if (pdata->bus_type != V4L2_MBUS_CSI2_LANES)
		ccic_reg_set_bit(pcdev, REG_CTRL0,
				C0_EOF_VSYNC | C0_VEDGE_CTRL);
#endif

	return ret;
}

static void ccic_frameirq_enable(struct mmp_camera_dev *pcdev)
{
	ccic_reg_clear_bit(pcdev, REG_IRQMASK, 0x0);

#ifdef __DEBUG_DMA_DONE
	dev_info(&pcdev->pdev->dev, "camera: CCIC utilize DMA done interrupt!!!\n");

	ccic_reg_write(pcdev, REG_IRQSTAT, FRAMEIRQS_DMA_DONE);
	ccic_reg_write(pcdev, REG_IRQSTAT, FRAMEIRQS_SOF);
	ccic_reg_set_bit(pcdev, REG_IRQMASK, FRAMEIRQS_DMA_DONE);
	ccic_reg_set_bit(pcdev, REG_IRQMASK, FRAMEIRQS_SOF);
#else
	ccic_reg_write(pcdev, REG_IRQSTAT, FRAMEIRQS_EOF);
	ccic_reg_write(pcdev, REG_IRQSTAT, FRAMEIRQS_SOF);
	ccic_reg_set_bit(pcdev, REG_IRQMASK, FRAMEIRQS_EOF);
	ccic_reg_set_bit(pcdev, REG_IRQMASK, FRAMEIRQS_SOF);
#endif
#ifdef __DEBUG_ENABLE_OVERFLOWIRQ
	ccic_reg_write(pcdev, REG_IRQSTAT, IRQ_OVERFLOW);
	ccic_reg_set_bit(pcdev, REG_IRQMASK, IRQ_OVERFLOW);
#endif
}

static void ccic_frameirq_disable(struct mmp_camera_dev *pcdev)
{
#ifdef __DEBUG_DMA_DONE
	ccic_reg_clear_bit(pcdev, REG_IRQMASK, FRAMEIRQS_DMA_DONE);
	ccic_reg_clear_bit(pcdev, REG_IRQMASK, FRAMEIRQS_SOF);
#else
	ccic_reg_clear_bit(pcdev, REG_IRQMASK, FRAMEIRQS_EOF);
	ccic_reg_clear_bit(pcdev, REG_IRQMASK, FRAMEIRQS_SOF);
#endif
#ifdef __DEBUG_ENABLE_OVERFLOWIRQ
	ccic_reg_clear_bit(pcdev, REG_IRQMASK, IRQ_OVERFLOW);
#endif

	ccic_reg_clear_bit(pcdev, REG_IRQMASK, 0x0);
}

static void ccic_start(struct mmp_camera_dev *pcdev)
{
	ccic_reg_set_bit(pcdev, REG_CTRL0, C0_ENABLE);
}

static void ccic_stop(struct mmp_camera_dev *pcdev)
{
	ccic_reg_clear_bit(pcdev, REG_CTRL0, C0_ENABLE);
}

static void ccic_stop_dma(struct mmp_camera_dev *pcdev)
{
	ccic_stop(pcdev);
	ccic_frameirq_disable(pcdev);
}

static void ccic_power_up(struct mmp_camera_dev *pcdev)
{
	ccic_reg_clear_bit(pcdev, REG_CTRL1, C1_PWRDWN);
}

static void ccic_power_down(struct mmp_camera_dev *pcdev)
{
	ccic_reg_set_bit(pcdev, REG_CTRL1, C1_PWRDWN);
}

/*
 * Fetch buffer from list, if single mode, we reserve the last buffer
 * until new buffer is got, or fetch directly
 */
static void mmp_set_contig_buffer(struct mmp_camera_dev *pcdev,
				unsigned int frame)
{
	struct mmp_buffer *buf;
	struct v4l2_pix_format *fmt = &pcdev->pix_format;
	unsigned long flags = 0;
	struct device *dev = &pcdev->pdev->dev;

	spin_lock_irqsave(&pcdev->list_lock, flags);
	if (list_empty(&pcdev->buffers)) {
		/*
		 * If there are no available buffers
		 * go into single buffer mode
		 */
		dev_dbg(dev, "camera drop a frame!\n");
#if MAX_DMA_BUFS == 2
		/*
		 * CCIC use Two Buffers mode
		 * will use another remaining frame buffer
		 * frame 0 -> buf 1
		 * frame 1 -> buf 0
		 */
		buf = pcdev->vb_bufs[frame ^ 0x1];
		set_bit(CF_SINGLE_BUF, &pcdev->flags);
		pcdev->frame_state.singles++;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(pcdev->mcd_root.mcd, MCD_DMA, MCD_DMA_DROP_FRAME, 1);
#endif
#elif MAX_DMA_BUFS == 3
		/*
		 * CCIC use Three Buffers mode
		 * will use the 2rd remaining frame buffer
		 * frame 0 -> buf 2
		 * frame 1 -> buf 0
		 * frame 2 -> buf 1
		 */
		buf = pcdev->vb_bufs[(frame + 0x2) % 0x3];
		if (pcdev->frame_state.tribufs == 0)
			pcdev->frame_state.tribufs++;
		else {
			set_bit(CF_SINGLE_BUF, &pcdev->flags);
			pcdev->frame_state.singles++;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(pcdev->mcd_root.mcd, MCD_DMA, MCD_DMA_DROP_FRAME, 1);
#endif
			if (pcdev->frame_state.tribufs < 2)
				pcdev->frame_state.tribufs++;
		}
#endif
	} else {
		/*
		 * OK, we have a buffer we can use.
		 */
		buf = list_first_entry(&pcdev->buffers, struct mmp_buffer,
					queue);
		list_del_init(&buf->queue);
		clear_bit(CF_SINGLE_BUF, &pcdev->flags);
#if MAX_DMA_BUFS == 3
		if (pcdev->frame_state.tribufs != 0)
			pcdev->frame_state.tribufs--;
#endif
	}

	pcdev->vb_bufs[frame] = buf;
	ccic_reg_write(pcdev, REG_Y0BAR + (frame << 2), buf->yuv_p.y);
	if (fmt->pixelformat == V4L2_PIX_FMT_YUV422P
			|| fmt->pixelformat == V4L2_PIX_FMT_YUV420
			|| fmt->pixelformat == V4L2_PIX_FMT_YVU420) {
		ccic_reg_write(pcdev, REG_U0BAR + (frame << 2), buf->yuv_p.u);
		ccic_reg_write(pcdev, REG_V0BAR + (frame << 2), buf->yuv_p.v);
	}
	spin_unlock_irqrestore(&pcdev->list_lock, flags);
}

static void mmp_dma_setup(struct mmp_camera_dev *pcdev)
{
	unsigned int frame;

	pcdev->nbufs = MAX_DMA_BUFS;
	for (frame = 0; frame < pcdev->nbufs; frame++)
		mmp_set_contig_buffer(pcdev, frame);

#if MAX_DMA_BUFS == 2
	/*
	 * CCIC use Two Buffers mode
	 */
	ccic_reg_set_bit(pcdev, REG_CTRL1, C1_TWOBUFS);
#endif
}

static void ccic_ctlr_reset(struct mmp_camera_dev *pcdev)
{
	unsigned long val;

	/*
	 * Used CCIC2
	 */
	if (pcdev->pdev->id) {
		val = readl(APMU_CCIC2_RST);
		writel(val & ~0x2, APMU_CCIC2_RST);
		writel(val | 0x2, APMU_CCIC2_RST);
	}

	val = readl(APMU_CCIC_RST);
	writel(val & ~0x2, APMU_CCIC_RST);
	writel(val | 0x2, APMU_CCIC_RST);
}

/*
 * Get everything ready, and start grabbing frames.
 */
static int mmp_read_setup(struct mmp_camera_dev *pcdev)
{
	int ret = 0;
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	ccic_ctlr_reset(pcdev);
#endif

	ret = ccic_config_phy(pcdev, 1);
	if (ret < 0)
		return ret;

	ccic_frameirq_enable(pcdev);
	mmp_dma_setup(pcdev);
	ccic_start(pcdev);
	#ifdef __DEBUG_DUMP_REGISTER
	dump_register(pcdev);
	#endif
	pcdev->state = S_STREAMING;

	return ret;
}

static int mmp_videobuf_setup(struct vb2_queue *vq,
			const struct v4l2_format *fmt,
			u32 *count, u32 *num_planes,
			unsigned int sizes[], void *alloc_ctxs[])
{
	struct soc_camera_device *icd = container_of(vq,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
			icd->current_fmt->host_fmt);

	int minbufs = 2;
	if (*count < minbufs)
		*count = minbufs;

	if (bytes_per_line < 0)
		return bytes_per_line;

	*num_planes = 1;
	sizes[0] = pcdev->pix_format.sizeimage;
	alloc_ctxs[0] = pcdev->vb_alloc_ctx;
	dev_dbg(&pcdev->pdev->dev, "count = %d, size = %u\n", *count, sizes[0]);

	return 0;
}

static int mmp_videobuf_prepare(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct mmp_buffer *buf = container_of(vb, struct mmp_buffer, vb_buf);
	unsigned long size;
	unsigned long flags = 0;
	int bytes_per_line = soc_mbus_bytes_per_line(icd->user_width,
			icd->current_fmt->host_fmt);

	if (bytes_per_line < 0)
		return bytes_per_line;

	dev_dbg(&pcdev->pdev->dev, "%s; (vb = 0x%p), 0x%p, %lu\n", __func__,
		vb, vb2_plane_vaddr(vb, 0), vb2_get_plane_payload(vb, 0));
	spin_lock_irqsave(&pcdev->list_lock, flags);
	/*
	 * Added list head initialization on alloc
	 */
	WARN(!list_empty(&buf->queue), "Buffer %p on queue!\n", vb);
	spin_unlock_irqrestore(&pcdev->list_lock, flags);
	BUG_ON(NULL == icd->current_fmt);
	size = vb2_plane_size(vb, 0);
	vb2_set_plane_payload(vb, 0, size);

	return 0;
}

static void mmp_videobuf_queue(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct mmp_buffer *buf = container_of(vb, struct mmp_buffer, vb_buf);
	unsigned long flags = 0;
	int start;
	dma_addr_t dma_handle;
	u32 base_size = icd->user_width * icd->user_height;

	mutex_lock(&pcdev->s_mutex);
	dma_handle = vb2_dma_contig_plane_dma_addr(vb, 0);
	BUG_ON(!dma_handle);
	spin_lock_irqsave(&pcdev->list_lock, flags);
	/*
	 * Wait until two buffers already queued to the list
	 * then start DMA
	 */
	start = (pcdev->state == S_BUFWAIT) && !list_empty(&pcdev->buffers);
	spin_unlock_irqrestore(&pcdev->list_lock, flags);

	if (pcdev->pix_format.pixelformat == V4L2_PIX_FMT_YUV422P) {
		buf->yuv_p.y = dma_handle;
		buf->yuv_p.u = buf->yuv_p.y + base_size;
		buf->yuv_p.v = buf->yuv_p.u + base_size / 2;
	} else if (pcdev->pix_format.pixelformat == V4L2_PIX_FMT_YUV420) {
		buf->yuv_p.y = dma_handle;
		buf->yuv_p.u = buf->yuv_p.y + base_size;
		buf->yuv_p.v = buf->yuv_p.u + base_size / 4;
	} else if (pcdev->pix_format.pixelformat == V4L2_PIX_FMT_YVU420) {
		buf->yuv_p.y = dma_handle;
		buf->yuv_p.v = buf->yuv_p.y + base_size;
		buf->yuv_p.u = buf->yuv_p.v + base_size / 4;
	} else {
		buf->yuv_p.y = dma_handle;
	}

	spin_lock_irqsave(&pcdev->list_lock, flags);
	list_add_tail(&buf->queue, &pcdev->buffers);
	spin_unlock_irqrestore(&pcdev->list_lock, flags);

	if (start)
		mmp_read_setup(pcdev);
	mutex_unlock(&pcdev->s_mutex);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	PEG(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_QBUF, 1);
#endif
}

static void mmp_videobuf_cleanup(struct vb2_buffer *vb)
{
	struct mmp_buffer *buf = container_of(vb, struct mmp_buffer, vb_buf);
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	unsigned long flags = 0;

	spin_lock_irqsave(&pcdev->list_lock, flags);
	/*
	 * queue list must be initialized before del
	 */
	if (buf->list_init_flag)
		list_del_init(&buf->queue);
	buf->list_init_flag = 0;
	spin_unlock_irqrestore(&pcdev->list_lock, flags);
}

/*
 * only the list that queued could be initialized
 */
static int mmp_videobuf_init(struct vb2_buffer *vb)
{
	struct mmp_buffer *buf = container_of(vb, struct mmp_buffer, vb_buf);
	INIT_LIST_HEAD(&buf->queue);
	buf->list_init_flag = 1;

	return 0;
}

static int mmp_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct soc_camera_device *icd = container_of(vq,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	unsigned long flags = 0;
	unsigned int frame;
	int ret = 0;

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	PEG(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_STREAM, 1);
	CLEAR(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_QBUF);
	CLEAR(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_DQBUF);
	SET(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_DUMP, \
		GET(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_DUMP));
	CLEAR(pcdev->mcd_root.mcd, MCD_DMA, MCD_DMA_SOF);
	CLEAR(pcdev->mcd_root.mcd, MCD_DMA, MCD_DMA_EOF);
	CLEAR(pcdev->mcd_root.mcd, MCD_DMA, MCD_DMA_OVERFLOW);
	CLEAR(pcdev->mcd_root.mcd, MCD_DMA, MCD_DMA_DROP_FRAME);
#endif
	mutex_lock(&pcdev->s_mutex);
	if (count < 2) {
		ret = -EINVAL;
		goto out_unlock;
	}

	if (pcdev->state != S_IDLE) {
		ret = -EINVAL;
		goto out_unlock;
	}
#ifdef __DEBUG_STREAM_ON_OFF_LOG
	dev_info(pcdev->icd->parent, "camera: Try to debug for Samsung, MMP streaming ON, Size : %dx%d\n", ssg_width, ssg_height);
#endif
	/*
	 * Videobuf2 sneakily hoards all the buffers and won't
	 * give them to us until *after* streaming starts.  But
	 * we can't actually start streaming until we have a
	 * destination.  So go into a wait state and hope they
	 * give us buffers soon.
	 */
	spin_lock_irqsave(&pcdev->list_lock, flags);
	if (list_empty(&pcdev->buffers)) {
		pcdev->state = S_BUFWAIT;
		spin_unlock_irqrestore(&pcdev->list_lock, flags);
		ret = 0;
		goto out_unlock;
	}
	spin_unlock_irqrestore(&pcdev->list_lock, flags);

	/*
	 * Ensure clear the obsolete frame flags
	 * before every really start streaming
	 */
	for (frame = 0; frame < pcdev->nbufs; frame++)
		clear_bit(CF_FRAME_SOF0 + frame, &pcdev->flags);
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	/* for ssg skip frames, set skip_frames to skip counts of frames*/
	s5k_cam_mclk_en.skip_frames = SSG_SKIP_FRAMES;
#endif
#if MAX_DMA_BUFS == 3
	pcdev->frame_state.tribufs = 0;
#endif

	ret = mmp_read_setup(pcdev);
out_unlock:
	mutex_unlock(&pcdev->s_mutex);

	return ret;
}

static int mmp_stop_streaming(struct vb2_queue *vq)
{
	struct soc_camera_device *icd = container_of(vq,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	unsigned long flags = 0;
	int ret = 0;

	mutex_lock(&pcdev->s_mutex);
	if (pcdev->state == S_BUFWAIT) {
		/* They never gave us buffers */
		pcdev->state = S_IDLE;
		goto out_unlock;
	}

	if (pcdev->state != S_STREAMING) {
		ret = -EINVAL;
		goto out_unlock;
	}

	ccic_stop_dma(pcdev);

#ifdef __DEBUG_STREAM_ON_OFF_LOG
	dev_info(pcdev->icd->parent, "camera: Try to debug for Samsung, MMP streaming OFF, Size : %dx%d\n", ssg_width, ssg_height);
#endif
	pcdev->state = S_IDLE;
	ccic_ctlr_reset(pcdev);
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	ccic_config_phy(pcdev, 0);//zswan
#endif
	spin_lock_irqsave(&pcdev->list_lock, flags);
	INIT_LIST_HEAD(&pcdev->buffers);
	spin_unlock_irqrestore(&pcdev->list_lock, flags);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	PEG(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_STREAM, -1);
#endif
out_unlock:
	mutex_unlock(&pcdev->s_mutex);

	return ret;
}

static struct vb2_ops mmp_videobuf_ops = {
	.queue_setup		= mmp_videobuf_setup,
	.buf_prepare		= mmp_videobuf_prepare,
	.buf_queue		= mmp_videobuf_queue,
	.buf_cleanup		= mmp_videobuf_cleanup,
	.buf_init		= mmp_videobuf_init,
	.start_streaming	= mmp_start_streaming,
	.stop_streaming		= mmp_stop_streaming,
	.wait_prepare		= soc_camera_unlock,
	.wait_finish		= soc_camera_lock,
};

static int mmp_camera_init_videobuf(struct vb2_queue *q,
			struct soc_camera_device *icd)
{
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	q->io_modes = VB2_USERPTR | VB2_MMAP;
	q->drv_priv = icd;
	q->ops = &mmp_videobuf_ops;
	q->mem_ops = &vb2_dma_contig_memops;
	q->buf_struct_size = sizeof(struct mmp_buffer);

	return vb2_queue_init(q);
}

/*
 * Hand a completed buffer back to user space.
 */
static void mmp_buffer_done(struct mmp_camera_dev *pcdev, unsigned int frame,
				struct vb2_buffer *vbuf)
{
	vbuf->v4l2_buf.bytesused = pcdev->pix_format.sizeimage;
	vb2_set_plane_payload(vbuf, 0, pcdev->pix_format.sizeimage);
	vb2_buffer_done(vbuf, VB2_BUF_STATE_DONE);
}

/*
 * Interrupt handler stuff
 */
static inline void mmp_frame_complete(struct mmp_camera_dev *pcdev,
				unsigned int frame)
{
	struct mmp_buffer *buf;
	unsigned long flags = 0;

	pcdev->frame_state.frames++;
	/*
	 * "This should never happen"
	 */
	if (pcdev->state != S_STREAMING)
		return;

	spin_lock_irqsave(&pcdev->list_lock, flags);
	buf = pcdev->vb_bufs[frame];
	if (!test_bit(CF_SINGLE_BUF, &pcdev->flags)) {
		pcdev->frame_state.delivered++;
		mmp_buffer_done(pcdev, frame, &buf->vb_buf);
	}
	spin_unlock_irqrestore(&pcdev->list_lock, flags);
	mmp_set_contig_buffer(pcdev, frame);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_DQBUF, 1);
		if (GET(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_DUMP) > 0) {
			vb_dump_nonblock(&buf->vb_buf, "/data/dump.yuv");
			PEG(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_DUMP, -1);
		}
#endif
}

static int check_ssg_frame_byte_count (struct mmp_camera_dev *pcdev,
				unsigned int frame, int width, int height ) {
	int counts, expectcounts;

	counts = ccic_reg_read(pcdev, REG_FRAME_CNT);
	expectcounts  = (width*height*2);
	if (counts != expectcounts)
		dev_info(pcdev->icd->parent,"--!!!error!!!----frame:%d-----Size: %d x %d-----expectcounts :%d, \
					but get real count: %d----\n",frame, width, height, expectcounts, counts);

	return 0;
}

static int check_ssg_frame_line_number (struct mmp_camera_dev *pcdev,
				unsigned int frame, int width, int height ) {
	int lines, expectlines;

	lines = ccic_reg_read(pcdev, REG_LNNUM);
	expectlines  = height;
	if (lines != expectlines)
		dev_info(pcdev->icd->parent,"--!!!error!!!----frame:%d-----Size: %d x %d-----expectlines :%d, \
					but get real lines: %d----\n",frame, width, height, expectlines, lines);

	return 0;
}
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
extern int s5k4ecgx_cam_state;
static int skip_ssg_eof1_eof2_frame (struct mmp_camera_dev *pcdev,
				unsigned int frame, int width, int height ) {
	if(s5k4ecgx_cam_state == S5K4ECGX_STATE_CAPTURE) {
		ccic_stop_dma(pcdev);
		dev_info(pcdev->icd->parent, "When do capture, we try to skip the latter  frames,as long as we got a right frame -Size: %d x %d-!\n",width, height);
	}
	return 0;
}
#endif
static irqreturn_t mmp_camera_frameirq(int irq, void *data)
{
	struct mmp_camera_dev *pcdev = data;
	struct vb2_buffer *vbuf;
	volatile u32 irqs;
	volatile u32 frame;
	volatile u32 irqsraw;

	#ifdef __DEBUG_ENABLE_RAWSTATUS
	irqsraw = ccic_reg_read(pcdev, REG_IRQSTATRAW);
	#endif
	irqs = ccic_reg_read(pcdev, REG_IRQSTAT);

	#ifdef __DEBUG_DMA_DONE
	if (!(irqs & ALLIRQS_DMA)) {
	#else
	if (!(irqs & ALLIRQS)) {
	#endif
		return IRQ_NONE;
	}

	if (irqs & IRQ_OVERFLOW) {
		set_bit(CF_FRMAE_OVERFLOW, &pcdev->flags);
	#ifdef __DEBUG_ENABLE_RAWSTATUS
		dev_info(pcdev->icd->parent,
			"irqsraw:0x%x\n", irqsraw);
	#endif
	#ifdef __DEBUG_ENABLE_OVERFLOWIRQ
		dev_info(pcdev->icd->parent,
			"irqs:0x%x, size:%dx%d\n",
			irqs, ssg_width, ssg_height);
	#endif
	}

	ccic_reg_write(pcdev, REG_IRQSTAT, irqs);

	/*
	 * Use the first loop handle the EOFx irq is more safety
	 * in potential EOFx and SOFx irqs co-exist case
	 * we may receive the EOFx of the last time and SOFx of this time
	 * during switch formats or resolutions
	 * if we can ensure this case never occur,
	 * then we can merge these 2 loops into 1 loop
	 */
	for (frame = 0; frame < pcdev->nbufs; frame++)
		#ifdef __DEBUG_DMA_DONE
		if (irqs & (IRQ_DMA_DONE0 << frame) &&
		#else
		if (irqs & (IRQ_EOF0 << frame) &&
		#endif
			test_bit(CF_FRAME_SOF0 + frame, &pcdev->flags)) {
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
			if (s5k_cam_mclk_en.skip_frames > 0) {
				s5k_cam_mclk_en.skip_frames--;
				clear_bit(CF_FRAME_SOF0 + frame, &pcdev->flags);
				dev_info(pcdev->icd->parent, "camera: ccic skip the frame: %d, flag = %d\n", frame, s5k_cam_mclk_en.skip_frames);
				return IRQ_HANDLED;
			}
#endif
	#ifdef __DEBUG_ENABLE_COUNTS_CHECK
		#ifdef __DEBUG_DMA_DONE
			check_ssg_frame_line_number(pcdev, frame, ssg_width, ssg_height);
		#else
			check_ssg_frame_byte_count(pcdev, frame, ssg_width, ssg_height);
		#endif
	#endif
			if (!test_bit(CF_FRMAE_OVERFLOW, &pcdev->flags)) {
				mmp_frame_complete(pcdev, frame);
			} else {
				dev_info(pcdev->icd->parent, "abnormal frame drop\n");
				clear_bit(CF_FRMAE_OVERFLOW, &pcdev->flags);
			}
#if defined(__DEBUG_ENABLE_SKIP_EOF_CHECK) && (defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7))
			skip_ssg_eof1_eof2_frame(pcdev, frame, ssg_width, ssg_height);
#endif
			clear_bit(CF_FRAME_SOF0 + frame, &pcdev->flags);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
			PEG(pcdev->mcd_root.mcd, MCD_DMA, MCD_DMA_EOF, 1);
#endif
		}

	for (frame = 0; frame < pcdev->nbufs; frame++)
		if (irqs & (IRQ_SOF0 << frame)) {
			set_bit(CF_FRAME_SOF0 + frame, &pcdev->flags);
			vbuf = &(pcdev->vb_bufs[frame]->vb_buf);
			do_gettimeofday(&vbuf->v4l2_buf.timestamp);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(pcdev->mcd_root.mcd, MCD_DMA, MCD_DMA_SOF, 1);
#endif
		}

	return IRQ_HANDLED;
}

static int mmp_camera_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_config cfg;
	struct device *dev = &pcdev->pdev->dev;
	int ret = 0;

	if (pcdev->icd)
		return -EBUSY;
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	if (pdata->init_pin)
		pdata->init_pin(dev, 1);
	else {
		ret = -EINVAL;
		dev_err(icd->parent,
			"camera: ccic pins are not configured!: %d\n", ret);
		return ret;
	}
#endif
	pcdev->frame_state.frames = 0;
	pcdev->frame_state.singles = 0;
	pcdev->frame_state.delivered = 0;

	pcdev->qos_idle.name = pcdev->pdev->name;
#if defined(CONFIG_CPU_PXA988) && (defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7))
	pm_qos_update_request(&pcdev->qos_idle, PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
	#ifdef __ENABLE_DDR_MIN_312MHZ
	pm_qos_update_request(&pcdev->qos_ddr, DDR_CONSTRAINT_LVL1);
	#endif
#else
	pm_qos_update_request(&pcdev->qos_idle, PM_QOS_CPUIDLE_BLOCK_DDR_VALUE);
#endif

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if ((ret < 0) && (ret != -ENOIOCTLCMD) && (ret != -ENODEV)) {
		dev_err(icd->parent,
			"camera: Failed to get mbus config: %d\n", ret);
		return ret;
	}

	if (ret != -ENODEV) {
		if (cfg.type == V4L2_MBUS_CSI2)
			pdata->bus_type = V4L2_MBUS_CSI2_LANES;
		else
			pdata->bus_type = 0;
	}

	pcdev->icd = icd;
	pcdev->state = S_IDLE;
	//move clock control to s5k43_power
	//ccic_enable_clk(pcdev);
	ccic_power_up(pcdev);
	ccic_stop(pcdev);

	/*
	 * Need sleep 1ms to wait for CCIC stable
	 * This is a workround for OV5640 MIPI
	 * TODO: Fix me in the future
	 */
	msleep(1);

	/*
	 * Mask all interrupts.
	 */
	ccic_reg_write(pcdev, REG_IRQMASK, 0);
	ret = v4l2_subdev_call(sd, core, init, 0);
	/*
	 * When v4l2_subdev_call return -ENOIOCTLCMD,
	 * means No ioctl command
	 */
	if ((ret < 0) && (ret != -ENOIOCTLCMD) && (ret != -ENODEV)) {
		dev_info(icd->parent,
			"camera: Failed to initialize subdev: %d\n", ret);
		return ret;
	}

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	PEG(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_ACT, 1);
	pcdev->mcd_sensor = default_mcd_sensor;
	strcpy(pcdev->mcd_sensor.entity.name, icd->link->module_name);
	pcdev->mcd_sensor.entity.priv = sd;
	ret = mcd_entity_init(&pcdev->mcd_sensor.entity, &pcdev->mcd_root.mcd);
	if (ret < 0)
		return ret;
	else
		pcdev->mcd_root.pitem[MCD_SENSOR] = &pcdev->mcd_ccic.entity;
	printk(KERN_INFO "cam: mount node debugfs/%s/%s\n",
		pcdev->mcd_root.mcd.name, pcdev->mcd_sensor.entity.name);

	pcdev->mcd_ccic = default_mcd_dma;
	strcpy(pcdev->mcd_ccic.entity.name, "ccic");
	ret = mcd_entity_init(&pcdev->mcd_ccic.entity, &pcdev->mcd_root.mcd);
	if (ret < 0)
		return ret;
	else
		pcdev->mcd_root.pitem[MCD_DMA] = &pcdev->mcd_ccic.entity;
	printk(KERN_INFO "cam: mount node debugfs/%s/%s\n",
		pcdev->mcd_root.mcd.name, pcdev->mcd_ccic.entity.name);

	/* CSI attached, now add debug interface for it*/
	pcdev->mcd_dphy = default_mcd_dphy;
	strcpy(pcdev->mcd_dphy.entity.name, "dphy");
	pcdev->mcd_dphy_hw.hw_ctx = pcdev;
	pcdev->mcd_dphy_hw.reg_write = &csi_dphy_write;
	pcdev->mcd_dphy_hw.reg_read = &csi_dphy_read;
	pcdev->mcd_dphy.entity.priv = &pcdev->mcd_dphy_hw;

	ret = mcd_entity_init(&pcdev->mcd_dphy.entity, &pcdev->mcd_root.mcd);
	if (ret < 0)
		return ret;
	pcdev->mcd_root.pitem[MCD_DPHY] = &pcdev->mcd_dphy.entity;
	printk(KERN_INFO "cam: mount node debugfs/%s/%s\n",
			pcdev->mcd_root.mcd.name, pcdev->mcd_dphy.entity.name);

#endif
	return 0;
}

static void mmp_camera_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	struct device *dev = &pcdev->pdev->dev;

	BUG_ON(icd != pcdev->icd);

	dev_err(dev, "Release %d frames, %d singles, %d delivered\n",
		pcdev->frame_state.frames, pcdev->frame_state.singles,
		pcdev->frame_state.delivered);
	ccic_config_phy(pcdev, 0);
	ccic_power_down(pcdev);
	// move clock control to s5k43_power
	// ccic_disable_clk(pcdev);
	pcdev->icd = NULL;

	pm_qos_update_request(&pcdev->qos_idle,
				PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	#ifdef __ENABLE_DDR_MIN_312MHZ
	pm_qos_update_request(&pcdev->qos_ddr, PM_QOS_DEFAULT_VALUE);
	#endif
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	if (pdata->init_pin)
		pdata->init_pin(dev, 0);
#endif

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	printk(KERN_INFO "cam: dismount node debugfs/%s/%s\n",
		pcdev->mcd_root.mcd.name, pcdev->mcd_dphy.entity.name);
	mcd_entity_remove(&pcdev->mcd_dphy.entity);
	printk(KERN_INFO "cam: dismount node debugfs/%s/%s\n",
		pcdev->mcd_root.mcd.name, pcdev->mcd_ccic.entity.name);
	mcd_entity_remove(&pcdev->mcd_ccic.entity);
	printk(KERN_INFO "cam: dismount node debugfs/%s/%s\n",
		pcdev->mcd_root.mcd.name, pcdev->mcd_sensor.entity.name);
	mcd_entity_remove(&pcdev->mcd_sensor.entity);
	PEG(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_ACT, -1);
#endif
}

static int mmp_camera_set_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct device *dev = &pcdev->pdev->dev;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_mbus_config cfg;
	int ret = 0;

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if ((ret < 0) && (ret != -ENOIOCTLCMD) && (ret != -ENODEV)) {
		dev_err(dev, "%s %d\n", __func__, __LINE__);
		return ret;
	}

	ret = v4l2_subdev_call(sd, video, s_mbus_config, &cfg);
	if ((ret < 0) && (ret != -ENOIOCTLCMD) && (ret != -ENODEV)) {
		dev_err(dev, "%s %d\n", __func__, __LINE__);
		return ret;
	}

	return 0;
}

static int mmp_camera_set_fmt(struct soc_camera_device *icd,
			struct v4l2_format *f)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	struct device *dev = &pcdev->pdev->dev;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate = NULL;
	struct v4l2_mbus_framefmt mf;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_subdev_frame_interval inter;
	int ret = 0;

	dev_dbg(dev, "camera: set_fmt: %c, width = %u, height = %u\n",
		pix->pixelformat, pix->width, pix->height);
	xlate = soc_camera_xlate_by_fourcc(icd, pix->pixelformat);
	if (!xlate) {
		dev_err(dev, "camera: format: %c not found\n",
			pix->pixelformat);
		return -EINVAL;
	}

	mf.width = pix->width;
	mf.height = pix->height;
	mf.field = V4L2_FIELD_NONE;
	mf.colorspace = pix->colorspace;
	mf.code = xlate->code;

	ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
	if (ret < 0) {
		dev_err(dev, "camera: set_fmt failed %d\n", __LINE__);
		return ret;
	}

	if (mf.code != xlate->code) {
		dev_err(dev, "camera: wrong code %s %d\n", __func__, __LINE__);
		return -EINVAL;
	}
#if !defined(CONFIG_MACH_LT02) && !defined(CONFIG_MACH_COCOA7)//Vincent Wan marks it, no use for ssg but to waste a little of time.
	/*
	 * To get frame_rate
	 */
	inter.pad = pdata->mclk_min;
	ret = v4l2_subdev_call(sd, video, g_frame_interval, &inter);
	if (ret < 0) {
		dev_err(dev, "camera: Can't get frame rate %s %d\n",
			__func__, __LINE__);
		pcdev->frame_rate = 0;
	} else
		pcdev->frame_rate =
			inter.interval.numerator / inter.interval.denominator;
#endif
	/*
	 * Update CSI2_DPHY3 value
	 */
	inter.pad = pdata->mclk_min; // Init inter.pad

	if (pdata->dphy3_algo == 1)
		/*
		 * dphy3_algo == 1
		 * Calculate CSI2_DPHY3 algo for PXA910
		 */
		pdata->dphy[0] = ((1 + inter.pad * 80 / 1000) & 0xff) << 8
				| (1 + inter.pad * 35 / 1000);
	else if (pdata->dphy3_algo == 2)
		/*
		 * dphy3_algo == 2
		 * Calculate CSI2_DPHY3 algo for PXA2128
		 */
		pdata->dphy[0] = ((2 + inter.pad * 110 / 1000) & 0xff) << 8
				| (1 + inter.pad * 35 / 1000);
	else{
		/*
		 * dphy3_algo == 0
		 * Use default CSI2_DPHY3 value in platform data for PXA688
		 */
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
		if((mf.width == 1280 && mf.height == 720)
				&& sr352_cam_state
				!= SR352_STATE_CAPTURE ) {
			pdata->dphy[0] = 0x1c0d;
		}
		else
			pdata->dphy[0] = 0x1208;
#endif
		dev_dbg(dev, "camera: use the default CSI2_DPHY3 value\n");
	}
	dev_info(dev, "camera: DPHY sets: dphy3=0x%x, dphy5=0x%x, dphy6=0x%x\n",
		pdata->dphy[0], pdata->dphy[1], pdata->dphy[2]);

	pix->width = mf.width;
	pix->height = mf.height;

	ssg_width = mf.width;
	ssg_height = mf.height;
	//dev_info(dev, "camera: set_fmt: %c, width = %u, height = %u\n",
		//pix->pixelformat, ssg_width, ssg_height);

	pix->field = mf.field;
	pix->colorspace = mf.colorspace;
	pcdev->pix_format.sizeimage = pix->sizeimage;
	icd->current_fmt = xlate;

	memcpy(&(pcdev->pix_format), pix, sizeof(struct v4l2_pix_format));
	ret = ccic_config_image(pcdev);

	return ret;
}

static int mmp_camera_try_fmt(struct soc_camera_device *icd,
			struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct device *dev = &pcdev->pdev->dev;
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix->pixelformat;
	int ret = 0;

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_err(dev, "camera: format: %c not found\n",
			pix->pixelformat);
		return -EINVAL;
	}

	pix->bytesperline = soc_mbus_bytes_per_line(pix->width,
						xlate->host_fmt);
	if (pix->bytesperline < 0)
		return pix->bytesperline;
	if (pix->pixelformat == V4L2_PIX_FMT_JPEG) {
		/*
		 * Todo: soc_camera_try_fmt could clear
		 * sizeimage, we can't get the value from
		 * userspace, just hard coding
		 */
		pix->bytesperline = 2048;
	} else
		pix->sizeimage = pix->height * pix->bytesperline;

	/*
	 * limit to sensor capabilities
	 */
	mf.width = pix->width;
	mf.height = pix->height;
	mf.field = V4L2_FIELD_NONE;
	mf.colorspace = pix->colorspace;
	mf.code = xlate->code;

	ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	pix->width = mf.width;
	pix->height = mf.height;
	pix->colorspace = mf.colorspace;

	switch (mf.field) {
	case V4L2_FIELD_ANY:
	case V4L2_FIELD_NONE:
		pix->field = V4L2_FIELD_NONE;
		break;
	default:
		dev_err(dev, "camera: Field type %d unsupported.\n", mf.field);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static unsigned int mmp_camera_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;

	return vb2_poll(&icd->vb2_vidq, file, pt);
}

static int mmp_camera_querycap(struct soc_camera_host *ici,
			struct v4l2_capability *cap)
{
	struct mmp_camera_dev *pcdev = ici->priv;
	struct soc_camera_device *icd = pcdev->icd;
	struct soc_camera_link *icl = to_soc_camera_link(icd);
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;

	cap->version = KERNEL_VERSION(0, 0, 5);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;

	strcpy(cap->card, pdata->name);
	strcpy(cap->driver, icl->module_name);

	return 0;
}

static int mmp_camera_set_parm(struct soc_camera_device *icd,
			struct v4l2_streamparm *para)
{
	return 0;
}

static int mmp_camera_get_formats(struct soc_camera_device *icd, u32 idx,
			struct soc_camera_format_xlate  *xlate)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct device *dev = &pcdev->pdev->dev;
	enum v4l2_mbus_pixelcode code;
	const struct soc_mbus_pixelfmt *fmt;
	int formats = 0, ret = 0, i;

	ret = v4l2_subdev_call(sd, video, enum_mbus_fmt, idx, &code);
	if (ret < 0)
		/*
		 * No more formats
		 */
		return 0;

	fmt = soc_mbus_get_fmtdesc(code);
	if (!fmt) {
		dev_err(dev, "camera: Invalid format #%u: %d\n", idx, code);
		return 0;
	}

	switch (code) {
	/*
	 * Refer to mbus_fmt struct
	 */
	case V4L2_MBUS_FMT_UYVY8_2X8:
		/*
		 * Add support for YUV420 and YUV422P
		 */
		formats = ARRAY_SIZE(ccic_formats);
		if (xlate) {
			for (i = 0; i < ARRAY_SIZE(ccic_formats); i++) {
				xlate->host_fmt = &ccic_formats[i];
				xlate->code = code;
				xlate++;
			}
		}
		return formats;
	case V4L2_MBUS_FMT_JPEG_1X8:
		if (xlate)
			dev_err(dev, "camera: Providing format: %s\n",
				fmt->name);
		break;
	default:
		/*
		 * camera controller can not support
		 * this format, which might supported by the sensor
		 */
		dev_warn(dev, "camera: Not support fmt: %s\n", fmt->name);
		return 0;
	}

	formats++;
	if (xlate) {
		xlate->host_fmt = fmt;
		xlate->code = code;
		xlate++;
	}

	return formats;
}

static struct soc_camera_host_ops mmp_soc_camera_host_ops = {
	.owner		= THIS_MODULE,
	.add		= mmp_camera_add_device,
	.remove		= mmp_camera_remove_device,
	.set_fmt	= mmp_camera_set_fmt,
	.try_fmt	= mmp_camera_try_fmt,
	.set_parm	= mmp_camera_set_parm,
	.init_videobuf2	= mmp_camera_init_videobuf,
	.poll		= mmp_camera_poll,
	.querycap	= mmp_camera_querycap,
	.set_bus_param	= mmp_camera_set_bus_param,
	.get_formats	= mmp_camera_get_formats,
};

static int __devinit mmp_camera_probe(struct platform_device *pdev)
{
	struct mmp_camera_dev *pcdev;
	struct mmp_cam_pdata *pdata;
	struct resource *res;
	void __iomem *base;
	int irq;
	#ifdef __DEBUG_DDR_CCIC_PRIOTITY
	int ddrpriority;
	#endif
	int err = 0;

	pdata = pdev->dev.platform_data;
	if (!pdata || !pdata->init_clk || !pdata->enable_clk)
		return -EINVAL;

	dev_info(&pdev->dev, "camera: probing CCIC%d\n", pdev->id + 1);

	pcdev = devm_kzalloc(&pdev->dev, sizeof(*pcdev), GFP_KERNEL);
	if (!pcdev) {
		dev_err(&pdev->dev, "camera: Could not allocate pcdev\n");
		return -ENOMEM;
	}

	pcdev->pdev = pdev;

	err = pdata->init_clk(&pdev->dev, 1);
	if (err)
		goto exit_clk;

	/* for ssg s5k sensor workaround for power sequence */
	#ifdef CONFIG_SOC_CAMERA_S5K4ECGX
	s5k_cam_mclk_en.disable_clk = ccic_disable_clk;
	s5k_cam_mclk_en.enable_clk = ccic_enable_clk;
	s5k_cam_mclk_en.pcdev = pcdev;
	#endif
	#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	s5k_cam_mclk_en.disable_clk = ccic_disable_clk;
	s5k_cam_mclk_en.enable_clk = ccic_enable_clk;
	s5k_cam_mclk_en.pcdev = pcdev;
	s5k_cam_mclk_en.skip_frames = 0;
	#endif
	#ifdef __DEBUG_DDR_CCIC_PRIOTITY
	ddrpriority = __raw_readl(DMCU_VIRT_BASE + DMCU_PORT_PRIORITY) ;
	ddrpriority |= (0x01 << 30 | 0x03 << 12);
	 __raw_writel(ddrpriority, DMCU_VIRT_BASE + DMCU_PORT_PRIORITY);
	 #endif
	//end workaround by Vincent wan.

	#ifdef CONFIG_SOC_CAMERA_SR030PC50
	sr030_cam_mclk_en.disable_clk = ccic_disable_clk;
	sr030_cam_mclk_en.enable_clk = ccic_enable_clk;
	sr030_cam_mclk_en.pcdev = pcdev;
	sr030_cam_mclk_en.skip_frames = 0;
	#endif

	INIT_LIST_HEAD(&pcdev->buffers);

	spin_lock_init(&pcdev->list_lock);
	mutex_init(&pcdev->s_mutex);

	pm_qos_add_request(&pcdev->qos_idle, PM_QOS_CPUIDLE_BLOCK,
				PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	#ifdef __ENABLE_DDR_MIN_312MHZ
	pm_qos_add_request(&pcdev->qos_ddr, PM_QOS_DDR_DEVFREQ_MIN,
				PM_QOS_DEFAULT_VALUE);
	#endif
	/*
	 * Request the regions and ioremap
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	base = devm_request_and_ioremap(&pdev->dev, res);
	if (!base) {
		dev_err(&pdev->dev,
			"camera: Failed to request and remap io memory\n");
		return -ENXIO;
	}

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	pcdev->mcd_dphy = default_mcd_dphy;
	sprintf(pcdev->mcd_root.mcd.name, "cam%d", pdev->id);
	pcdev->mcd_root.mcd.nr_entity = MCD_ENTITY_END;
	err = mcd_init(&pcdev->mcd_root.mcd);
	if (err < 0)
		return err;
	printk(KERN_INFO "cam: Marvell Camera Debug interface created in " \
			"debugfs/%s\n", pcdev->mcd_root.mcd.name);

	pcdev->mcd_vdev = default_mcd_vdev;
	strcpy(pcdev->mcd_vdev.entity.name, "vdev");
	err = mcd_entity_init(&pcdev->mcd_vdev.entity, &pcdev->mcd_root.mcd);
	if (err < 0)
		return err;
	else
		pcdev->mcd_root.pitem[MCD_VDEV] = &pcdev->mcd_vdev.entity;
	printk(KERN_INFO "cam: mount node debugfs/%s/%s\n",
			pcdev->mcd_root.mcd.name, pcdev->mcd_vdev.entity.name);
#endif

	pcdev->res = res;
	pcdev->base = base;

	/*
	 * Request irq
	 */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "camera: Failed to get irq resource\n");
		return -ENXIO;
	}

	pcdev->irq = irq;
	err = devm_request_irq(&pdev->dev, pcdev->irq, mmp_camera_frameirq,
				IRQF_SHARED, MMP_CAM_DRV_NAME, pcdev);
	if (err) {
		dev_err(&pdev->dev, "camera: Interrupt request failed\n");
		goto exit_clk;
	}

	ccic_enable_clk(pcdev);
	pcdev->soc_host.drv_name = MMP_CAM_DRV_NAME;
	pcdev->soc_host.ops = &mmp_soc_camera_host_ops;
	pcdev->soc_host.priv = pcdev;
	pcdev->soc_host.v4l2_dev.dev = &pdev->dev;
	pcdev->soc_host.nr = pdev->id;
	pcdev->vb_alloc_ctx = (struct vb2_alloc_ctx *)
				vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(pcdev->vb_alloc_ctx)) {
		err = PTR_ERR(pcdev->vb_alloc_ctx);
		goto exit_disable_clk;
	}

	err = soc_camera_host_register(&pcdev->soc_host);
	if (err)
		goto exit_free_ctx;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	PEG(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_REG, 1);
#endif

	ccic_disable_clk(pcdev);

	return 0;

exit_free_ctx:
	vb2_dma_contig_cleanup_ctx(pcdev->vb_alloc_ctx);
exit_disable_clk:
	ccic_disable_clk(pcdev);
exit_clk:
	pdata->init_clk(&pdev->dev, 0);

	return err;
}

static int __devexit mmp_camera_remove(struct platform_device *pdev)
{

	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct mmp_camera_dev *pcdev = container_of(soc_host,
			struct mmp_camera_dev, soc_host);
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;

	pm_qos_remove_request(&pcdev->qos_idle);
	pdata->init_clk(&pdev->dev, 0);
	soc_camera_host_unregister(soc_host);
	vb2_dma_contig_cleanup_ctx(pcdev->vb_alloc_ctx);
	pcdev->vb_alloc_ctx = NULL;
	dev_info(&pdev->dev, "camera: MMP Camera driver unloaded\n");
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	PEG(pcdev->mcd_root.mcd, MCD_VDEV, MCD_VDEV_REG, -1);
#endif

	return 0;
}

static int mmp_camera_suspend(struct device *dev)
{
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct soc_camera_device *icd = pcdev->icd;
	struct v4l2_subdev *sd = NULL;
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	int ret = 0;

	if (icd == NULL || icd->use_count == 0)
		return 0;

	dev_err(dev, "camera: someone is stil using ccic\n");

	mutex_lock(&pcdev->s_mutex);
	if (pcdev->state == S_STREAMING)
		ccic_stop_dma(pcdev);
	mutex_unlock(&pcdev->s_mutex);

	/*
	 * FIXME:
	 * Set sensor to hardware standby mode.The
	 * implementation is only valid for one
	 * sensor.For multi sensors. Need to power
	 * saving every active sensor.
	 */
	sd = soc_camera_to_subdev(icd);
	ret = v4l2_subdev_call(sd, core, s_power, 0);
	if (ret < 0) {
		dev_err(dev, "camera: enter sensor hardware standby failed\n");
		return ret;
	}

	ccic_disable_clk(pcdev);
	ccic_power_down(pcdev);
#ifdef CONFIG_SOC_CAMERA_SR030PC50
	if (pdata->init_pin)
		pdata->init_pin(dev, 0);
#endif
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
	if (pdata->init_pin)
		pdata->init_pin(dev, 0);
#endif

	pm_qos_update_request(&pcdev->qos_idle,
				PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);

	return ret;
}

static int mmp_camera_resume(struct device *dev)
{
	struct soc_camera_host *ici = to_soc_camera_host(dev);
	struct mmp_camera_dev *pcdev = ici->priv;
	struct soc_camera_device *icd = pcdev->icd;
	struct v4l2_subdev *sd = NULL;
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	int ret = 0;

	if (icd == NULL || icd->use_count == 0)
		return 0;
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
#ifdef CONFIG_CPU_PXA988
	pm_qos_update_request(&pcdev->qos_idle, PM_QOS_CPUIDLE_BLOCK_AXI_VALUE);
#endif
	if (pdata->init_pin)
		pdata->init_pin(dev, 1);
#else
#ifdef CONFIG_SOC_CAMERA_SR030PC50
	if (pdata->init_pin)
		pdata->init_pin(dev, 1);
#else
	pm_qos_update_request(&pcdev->qos_idle, PM_QOS_CPUIDLE_BLOCK_DDR_VALUE);
#endif
#endif
	ccic_power_up(pcdev);
	ccic_enable_clk(pcdev);

	sd = soc_camera_to_subdev(icd);
	ret = v4l2_subdev_call(sd, core, s_power, 1);
	if (ret < 0) {
		dev_err(dev, "camera: exit sensor hardware standby failed\n");
		return ret;
	}

	mutex_lock(&pcdev->s_mutex);
	if (pcdev->state == S_STREAMING) {
		ccic_frameirq_enable(pcdev);
		ccic_start(pcdev);
	}
	mutex_unlock(&pcdev->s_mutex);

	return ret;
}

static const struct dev_pm_ops mmp_camera_pm = {
	.suspend = mmp_camera_suspend,
	.resume	= mmp_camera_resume,
};

static struct platform_driver mmp_camera_driver = {
	.driver = {
		.name = MMP_CAM_DRV_NAME,
		.pm = &mmp_camera_pm,
	},
	.probe = mmp_camera_probe,
	.remove = __devexit_p(mmp_camera_remove),
};

module_platform_driver(mmp_camera_driver);

MODULE_DESCRIPTION("Marvell MMP CMOS Camera Interface Controller driver");
MODULE_AUTHOR("Kassey Lee <ygli@marvell.com>");
MODULE_AUTHOR("Angela Wan <jwan@marvell.com>");
MODULE_AUTHOR("Albert Wang <twang13@marvell.com>");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("Video");

