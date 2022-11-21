/*
* ispccic.c
*
* Marvell DxO ISP - CCIC module
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

#include <linux/delay.h>
#include <media/v4l2-common.h>
#include <linux/v4l2-mediabus.h>
#include <linux/mm.h>
#include <linux/clk.h>

#include "isp.h"
#include "ispreg.h"
#include "ispccic.h"

#define MAX_CCIC_CH_USED	2

static const struct pad_formats ccic_input_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
};

static const struct pad_formats ccic_output_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
};

static void __maybe_unused ccic_dump_regs(struct mvisp_device *isp)
{
	unsigned long regval;

	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_Y0_BASE_ADDR);
	dev_warn(isp->dev, "ccic_set_stream Y0: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_Y1_BASE_ADDR);
	dev_warn(isp->dev, "ccic_set_stream Y1: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_Y2_BASE_ADDR);
	dev_warn(isp->dev, "ccic_set_stream Y2: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_RAW_STATUS);
	dev_warn(isp->dev, "ccic_set_stream IRQRAWSTATE: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_STATUS);
	dev_warn(isp->dev, "ccic_set_stream IRQSTATE: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_MASK);
	dev_warn(isp->dev, "ccic_set_stream IRQMASK: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
	dev_warn(isp->dev, "ccic_set_stream CTRL0: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_1);
	dev_warn(isp->dev, "ccic_set_stream CTRL1: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CLOCK_CTRL);
	dev_warn(isp->dev, "ccic_set_stream CLKCTRL: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_IRQ_RAW_STATUS);
	dev_warn(isp->dev, "ccic_set_stream MIPI STATUS: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY3);
	dev_warn(isp->dev, "ccic_set_stream MIPI DPHY3: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY5);
	dev_warn(isp->dev, "ccic_set_stream MIPI DPHY5: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY6);
	dev_warn(isp->dev, "ccic_set_stream MIPI DPHY6: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IMG_SIZE);
	dev_warn(isp->dev, "ccic_set_stream SIZE: 0x%08lX\n", regval);
	regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IMG_PITCH);
	dev_warn(isp->dev, "ccic_set_stream PITCH: 0x%08lX\n", regval);
}

static int ccic_dump_registers(struct isp_ccic_device *ccic,
			struct v4l2_ccic_dump_registers *regs)
{
	struct mvisp_device *isp = ccic->isp;

	if (NULL == regs || NULL == isp)
		return -EINVAL;

	regs->y0_base_addr = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_Y0_BASE_ADDR);
	regs->y1_base_addr = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_Y1_BASE_ADDR);
	regs->y2_base_addr = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_Y2_BASE_ADDR);
	regs->irq_raw_status = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_RAW_STATUS);
	regs->irq_status = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_STATUS);
	regs->irq_mask = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_MASK);
	regs->ctrl_0 = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
	regs->ctrl_1 = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_1);
	regs->clock_ctrl = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CLOCK_CTRL);
	regs->csi2_irq_raw_status = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_IRQ_RAW_STATUS);
	regs->csi2_dphy3 = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY3);
	regs->csi2_dphy5 = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY5);
	regs->csi2_dphy6 = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY6);
	regs->img_size = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IMG_SIZE);
	regs->img_pitch = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_IMG_PITCH);

	return 0;
}


static void ccic_set_dma_addr(struct isp_ccic_device *ccic,
	dma_addr_t	paddr, enum isp_ccic_irq_type irqeof)
{
	struct mvisp_device *isp = ccic->isp;

	BUG_ON(paddr == 0);

	switch (irqeof) {
	case CCIC_EOF0:
		mvisp_reg_writel(isp, paddr,
			CCIC_ISP_IOMEM_1, CCIC_Y0_BASE_ADDR);
		break;
	case CCIC_EOF1:
		mvisp_reg_writel(isp, paddr,
			CCIC_ISP_IOMEM_1, CCIC_Y1_BASE_ADDR);
		break;
	case CCIC_EOF2:
		mvisp_reg_writel(isp, paddr,
			CCIC_ISP_IOMEM_1, CCIC_Y2_BASE_ADDR);
		break;
	default:
		break;
	}

	return;
}

static int ccic_clear_mipi(struct isp_ccic_device *ccic)
{
	struct mvisp_device *isp = ccic->isp;
	unsigned long mipi_lock_flags;

	spin_lock_irqsave(&ccic->mipi_flag_lock, mipi_lock_flags);
	if (ccic->mipi_config_flag == MIPI_NOT_SET) {
		spin_unlock_irqrestore(&ccic->mipi_flag_lock, mipi_lock_flags);
		return 0;
	}

	switch (ccic->ccic_id) {
	case CCIC_ID_1:
		mvisp_reg_writel(isp, 0x0,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY3);
		mvisp_reg_writel(isp, 0x0,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY5);
		mvisp_reg_writel(isp, 0x0,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY6);
		mvisp_reg_writel(isp, 0x0,
			CCIC_ISP_IOMEM_1, CCIC_CSI2_CTRL0);
		break;
	case CCIC_ID_2:
		break;
	default:
		break;
	}

	ccic->mipi_config_flag = MIPI_NOT_SET;
	spin_unlock_irqrestore(&ccic->mipi_flag_lock, mipi_lock_flags);
	return 0;
}

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
#include <mach/regs-apmu.h>

static void csi_dphy_write(void *hw_ctx, const struct csi_dphy_reg *regs)
{
	struct isp_ccic_device *ccic = hw_ctx;
	struct mvisp_device *isp = ccic->isp;
	u32 regval = 0;

	/* need stop CCIC ? */
	/* reset DPHY */
	/* Should not do APMU register R/W right here,
	 * better call platform interface*/
	regval = readl(APMU_CCIC_RST);
	writel(regval & ~0x2, APMU_CCIC_RST);
	writel(regval, APMU_CCIC_RST);

	regval = regs->hs_settle & 0xFF;
	regval = regs->hs_termen | (regval << 8);
	mvisp_reg_writel(isp, regval, CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY3);

	regval = regs->cl_settle & 0xFF;
	regval = regs->cl_termen | (regval << 8);
	mvisp_reg_writel(isp, regval, CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY6);

	regval = (1 << regs->lane) - 1;
	regval = regval | (regval << 4);
	mvisp_reg_writel(isp, regval, CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY5);

	regval = (regs->lane - 1) & 0x03;	/* support 4 lane at most */
	regval = (regval << 1) | 0x41;
	mvisp_reg_writel(isp, regval, CCIC_ISP_IOMEM_1, CCIC_CSI2_CTRL0);

	/* start CCIC */
	regval = mvisp_reg_readl(isp, CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
	regval |= 0x1;
	mvisp_reg_writel(isp, regval, CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
};

static void csi_dphy_read(void *hw_ctx, struct csi_dphy_reg *regs)
{
	struct isp_ccic_device *ccic = hw_ctx;
	struct mvisp_device *isp = ccic->isp;
	u32 phy3, phy5, phy6;

	phy3	= mvisp_reg_readl(isp, CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY3);
	phy5	= mvisp_reg_readl(isp, CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY5);
	phy6	= mvisp_reg_readl(isp, CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY6);

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

static int ccic_configure_mipi(struct isp_ccic_device *ccic)
{
	struct mvisp_device *isp = ccic->isp;
	unsigned long mipi_lock_flags;
	bool valid_sensor;
	int lanes = ccic->lanes;
	unsigned int dphy3_val;
	unsigned int dphy5_val;
	unsigned int dphy6_val;


	if (lanes == 0) {
		dev_warn(isp->dev, "CCIC lanes num not set, set it to 2\n");
		lanes = 2;
	}

	spin_lock_irqsave(&ccic->mipi_flag_lock, mipi_lock_flags);
	if (ccic->mipi_config_flag != MIPI_NOT_SET) {
		spin_unlock_irqrestore(&ccic->mipi_flag_lock, mipi_lock_flags);
		return 0;
	}

	valid_sensor = false;
	switch (ccic->ccic_id) {
	case CCIC_ID_1:
		switch (ccic->sensor_type) {
		case SENSOR_OV5642:
			dphy3_val = 0x1B0B;
			if (unlikely(lanes == 1))
				dphy5_val = 0x11;
			else
				dphy5_val = 0x33;
			dphy6_val = 0x1A03;

			valid_sensor = true;
			break;
		case SENSOR_OV882X:
		case SENSOR_OV5647:
		case SENSOR_LSI3H5:
			if (isp->cpu_type == MV_PXA988) {
				dphy3_val = 0x1806;
				dphy6_val = 0xA00;

			} else {
				dphy3_val = 0xA06;
				dphy6_val = 0x1A03;
			}

			if (lanes == 4)
				dphy5_val = 0xff;
			else if (unlikely(lanes == 1))
				dphy5_val = 0x11;
			else
				dphy5_val = 0x33;

			valid_sensor = true;

			break;
		default:
			break;
		}

		if (valid_sensor == true) {
			mvisp_reg_writel(isp, dphy3_val,
					CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY3);
			mvisp_reg_writel(isp, dphy5_val,
					CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY5);
			mvisp_reg_writel(isp, dphy6_val,
					CCIC_ISP_IOMEM_1, CCIC_CSI2_DPHY6);

			if (lanes == 4) {
				mvisp_reg_writel(isp,
				  0x47, CCIC_ISP_IOMEM_1, CCIC_CSI2_CTRL0);
			} else if (lanes == 1) {
				mvisp_reg_writel(isp,
				  0x41, CCIC_ISP_IOMEM_1, CCIC_CSI2_CTRL0);
			} else {
				mvisp_reg_writel(isp,
				  0x43, CCIC_ISP_IOMEM_1, CCIC_CSI2_CTRL0);
			}
		}
		break;
	case CCIC_ID_2:
		break;
	default:
		break;
	}

	ccic->mipi_config_flag = MIPI_SET;
	spin_unlock_irqrestore(&ccic->mipi_flag_lock, mipi_lock_flags);
	return 0;
}

/* -----------------------------------------------------------------------------
 * ISP video operations */
static struct isp_ccic_device *find_ccic_from_video(struct isp_video *video)
{
	struct isp_ccic_device *ccic;
	struct mvisp_device *isp = video->isp;

	if (&(isp->mvisp_ccic1.video_out) == video)
		ccic = &isp->mvisp_ccic1;
	else if (&(isp->mvisp_ccic2.video_out) == video)
		ccic = &isp->mvisp_ccic2;
	else
		return NULL;

	return ccic;
}

static int ccic_enable_hw(struct isp_ccic_device *ccic, struct isp_video *video)
{
	struct mvisp_device *isp = video->isp;
	unsigned long regval;

	if ((ccic->output&CCIC_OUTPUT_MEMORY) != 0) {
		mvisp_reg_writel(isp, 0xFFFFFFFF,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_STATUS);
		mvisp_reg_writel(isp, 0x3,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_MASK);
		regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
		regval |= 0x1;
		mvisp_reg_writel(isp, regval,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_0);

		ccic->dma_state = CCIC_DMA_BUSY;
	}

	return 0;

}

static int ccic_disable_hw(struct isp_ccic_device *ccic,
	struct isp_video *video)
{
	struct mvisp_device *isp = video->isp;
	unsigned long regval;

	if ((ccic->output&CCIC_OUTPUT_MEMORY) != 0) {
		regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
		regval &= ~0x1;
		mvisp_reg_writel(isp, regval,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
		mvisp_reg_writel(isp, 0x0,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_MASK);
		mvisp_reg_writel(isp, 0xFFFFFFFF,
			CCIC_ISP_IOMEM_1, CCIC_IRQ_STATUS);

		ccic->dma_state = CCIC_DMA_IDLE;
	}

	return 0;

}

static int ccic_load_buffer(struct isp_ccic_device *ccic, int ch)
{
	struct isp_video_buffer *buffer = NULL;
	unsigned long video_flags;

	buffer = mvisp_video_get_next_work_buf(&ccic->video_out);
	if (buffer == NULL)
		return -EINVAL;

	if (isp_buf_type_is_capture_mp(buffer->vb2_buf.v4l2_buf.type)) {
		spin_lock_irqsave(&ccic->video_out.irq_lock, video_flags);
		ccic_set_dma_addr(ccic, buffer->paddr[ISP_BUF_PADDR], ch);
		spin_unlock_irqrestore(&ccic->video_out.irq_lock, video_flags);
	}

	return 0;
}


static int ccic_video_stream_on_notify(struct isp_video *video)
{
	struct isp_ccic_device *ccic;
	int ch, ret;

	ccic = find_ccic_from_video(video);
	if (ccic == NULL)
		return -EINVAL;

	mutex_lock(&ccic->ccic_mutex);

	if (ccic->dma_state == CCIC_DMA_BUSY) {
		mutex_unlock(&ccic->ccic_mutex);
		return 0;
	}

	ccic_configure_mipi(ccic);

	for (ch = 0; ch < MAX_CCIC_CH_USED; ch++) {
		ret = ccic_load_buffer(ccic, ch);
		if (ret != 0)
			return -EINVAL;
	}

	ccic_enable_hw(ccic, video);

	mutex_unlock(&ccic->ccic_mutex);

	return 0;
}

static int ccic_video_stream_off_notify(struct isp_video *video)
{
	struct isp_ccic_device *ccic;

	ccic = find_ccic_from_video(video);
	if (ccic == NULL)
		return -EINVAL;

	mutex_lock(&ccic->ccic_mutex);

	if (ccic->dma_state == CCIC_DMA_IDLE) {
		mutex_unlock(&ccic->ccic_mutex);
		return 0;
	}

	ccic_disable_hw(ccic, video);

	mutex_unlock(&ccic->ccic_mutex);

	return 0;
}

static int ccic_video_qbuf_notify(struct isp_video *video)
{
	return 0;
}

static const struct isp_video_operations ccic_ispvideo_ops = {
	.qbuf_notify = ccic_video_qbuf_notify,
	.stream_on_notify = ccic_video_stream_on_notify,
	.stream_off_notify = ccic_video_stream_off_notify,
};

/* -----------------------------------------------------------------------------
 * V4L2 subdev operations
 */

static struct v4l2_mbus_framefmt *
__ccic_get_format(struct isp_ccic_device *ccic, struct v4l2_subdev_fh *fh,
		  unsigned int pad, enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &ccic->formats[pad];
}

static int
ccic_try_format(struct isp_ccic_device *ccic, struct v4l2_subdev_fh *fh,
		unsigned int pad, struct v4l2_mbus_framefmt *fmt,
		enum v4l2_subdev_format_whence which)
{
	unsigned int i;
	int ret = 0;

	switch (pad) {
	case CCIC_PAD_SINK:
		for (i = 0; i < ARRAY_SIZE(ccic_input_fmts); i++) {
			if (fmt->code == ccic_input_fmts[i].mbusfmt) {
				fmt->colorspace = ccic_input_fmts[i].colorspace;
				break;
			}
		}

		if (i >= ARRAY_SIZE(ccic_input_fmts))
			fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;

		break;

	case CCIC_PAD_ISP_SRC:
	case CCIC_PAD_DMA_SRC:
		for (i = 0; i < ARRAY_SIZE(ccic_output_fmts); i++) {
			if (fmt->code == ccic_output_fmts[i].mbusfmt) {
				fmt->colorspace = ccic_input_fmts[i].colorspace;
				break;
			}
		}

		if (i >= ARRAY_SIZE(ccic_output_fmts))
			fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	fmt->field = V4L2_FIELD_NONE;

	return ret;
}

static int ccic_enum_mbus_code(struct v4l2_subdev *sd,
			       struct v4l2_subdev_fh *fh,
			       struct v4l2_subdev_mbus_code_enum *code)
{
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);
	int ret = 0;

	mutex_lock(&ccic->ccic_mutex);
	if (code->pad == CCIC_PAD_SINK) {
		if (code->index >= ARRAY_SIZE(ccic_input_fmts)) {
			ret = -EINVAL;
			goto error;
		}
		code->code = ccic_input_fmts[code->index].mbusfmt;
	} else {
		if (code->index >= ARRAY_SIZE(ccic_output_fmts)) {
			ret = -EINVAL;
			goto error;
		}
		code->code = ccic_output_fmts[code->index].mbusfmt;
	}

error:
	mutex_unlock(&ccic->ccic_mutex);
	return ret;
}

static int ccic_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt format;
	int ret = 0;
	mutex_lock(&ccic->ccic_mutex);

	if (fse->index != 0) {
		ret = -EINVAL;
		goto error;
	}

	format.code = fse->code;
	format.width = 1;
	format.height = 1;
	ccic_try_format(ccic, fh, fse->pad, &format, V4L2_SUBDEV_FORMAT_TRY);
	fse->min_width = format.width;
	fse->min_height = format.height;

	if (format.code != fse->code) {
		ret = -EINVAL;
		goto error;
	}

	format.code = fse->code;
	format.width = -1;
	format.height = -1;
	ccic_try_format(ccic, fh, fse->pad, &format, V4L2_SUBDEV_FORMAT_TRY);
	fse->max_width = format.width;
	fse->max_height = format.height;

error:
	mutex_unlock(&ccic->ccic_mutex);
	return ret;
}

static int ccic_get_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			   struct v4l2_subdev_format *fmt)
{
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;
	int ret = 0;
	mutex_lock(&ccic->ccic_mutex);

	format = __ccic_get_format(ccic, fh, fmt->pad, fmt->which);
	if (format == NULL) {
		ret = -EINVAL;
		goto error;
	}

	fmt->format = *format;
error:
	mutex_unlock(&ccic->ccic_mutex);
	return ret;
}

static int ccic_config_format(struct isp_ccic_device *ccic, unsigned int pad)
{
	struct v4l2_mbus_framefmt *format = &ccic->formats[pad];
	struct mvisp_device *isp = ccic->isp;
	unsigned long width, height, regval, bytesperline;
	unsigned long ctrl0val = 0;
	unsigned long ypitch;
	int ret = 0;

	width = format->width;
	height = format->height;

	ctrl0val = mvisp_reg_readl(isp,
		CCIC_ISP_IOMEM_1, CCIC_CTRL_0);

	switch (format->code) {
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		ctrl0val &= ~0x000001E0;
		ctrl0val |= ((0x2 << 7) | (0x2 << 5));
		bytesperline = width * 10 / 8;
		ypitch = bytesperline / 4;
		break;
	case V4L2_MBUS_FMT_UYVY8_1X16:
		ctrl0val &= ~0x0003E1E0;
		ctrl0val |= (0x4 << 13);
		bytesperline = width * 16 / 8;
		ypitch = bytesperline / 4;
		break;
	default:
		return -EINVAL;
	}

	switch (ccic->ccic_id) {
	case CCIC_ID_1:
		regval = (height << 16) | bytesperline;
		mvisp_reg_writel(isp, regval,
			CCIC_ISP_IOMEM_1, CCIC_IMG_SIZE);
		regval = (ypitch << 2);
		mvisp_reg_writel(isp, regval,
			CCIC_ISP_IOMEM_1, CCIC_IMG_PITCH);
		mvisp_reg_writel(isp, 0,
			CCIC_ISP_IOMEM_1, CCIC_IMG_OFFSET);
		mvisp_reg_writel(isp, ctrl0val,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
		break;
	case CCIC_ID_2:
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static int ccic_set_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			   struct v4l2_subdev_format *fmt)
{
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;
	int ret = 0;
	mutex_lock(&ccic->ccic_mutex);

	format = __ccic_get_format(ccic, fh, fmt->pad, fmt->which);
	if (format == NULL) {
		ret = -EINVAL;
		goto error;
	}

	ret = ccic_try_format(ccic, fh, fmt->pad, &fmt->format, fmt->which);
	if (ret < 0)
		goto error;

	*format = fmt->format;

	if (fmt->which != V4L2_SUBDEV_FORMAT_TRY)
		ret = ccic_config_format(ccic, fmt->pad);

error:
	mutex_unlock(&ccic->ccic_mutex);
	return ret;
}

static int ccic_init_formats(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct v4l2_subdev_format format;
	struct v4l2_mbus_framefmt *format_active, *format_try;
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (fh == NULL) {
		memset(&format, 0, sizeof(format));
		format.pad = CCIC_PAD_SINK;
		format.which =
			fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
		format.format.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		format.format.width = 640;
		format.format.height = 480;
		format.format.colorspace = V4L2_COLORSPACE_SRGB;
		format.format.field = V4L2_FIELD_NONE;
		ret = ccic_set_format(sd, fh, &format);

		format.pad = CCIC_PAD_ISP_SRC;
		format.which =
			fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
		format.format.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		format.format.width = 640;
		format.format.height = 480;
		format.format.colorspace = V4L2_COLORSPACE_SRGB;
		ret = ccic_set_format(sd, fh, &format);
		format.pad = CCIC_PAD_DMA_SRC;
		ret = ccic_set_format(sd, fh, &format);
	} else {
	/* Copy the active format to a newly opened fh structure */
		mutex_lock(&ccic->ccic_mutex);
		format_active = __ccic_get_format
			(ccic, fh, CCIC_PAD_SINK, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (format_active == NULL) {
			ret = -EINVAL;
			goto error;
		}
		format_try = __ccic_get_format
			(ccic, fh, CCIC_PAD_SINK, V4L2_SUBDEV_FORMAT_TRY);
		if (format_try == NULL) {
			ret = -EINVAL;
			goto error;
		}
		memcpy(format_try, format_active,
				sizeof(struct v4l2_subdev_format));

		format_active = __ccic_get_format
			(ccic, fh, CCIC_PAD_ISP_SRC, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (format_active == NULL) {
			ret = -EINVAL;
			goto error;
		}
		format_try = __ccic_get_format
			(ccic, fh, CCIC_PAD_ISP_SRC, V4L2_SUBDEV_FORMAT_TRY);
		if (format_try == NULL) {
			ret = -EINVAL;
			goto error;
		}
		memcpy(format_try, format_active,
				sizeof(struct v4l2_subdev_format));

		format_active = __ccic_get_format
			(ccic, fh, CCIC_PAD_DMA_SRC, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (format_active == NULL) {
			ret = -EINVAL;
			goto error;
		}
		format_try = __ccic_get_format
			(ccic, fh, CCIC_PAD_DMA_SRC, V4L2_SUBDEV_FORMAT_TRY);
		if (format_try == NULL) {
			ret = -EINVAL;
			goto error;
		}
		memcpy(format_try, format_active,
				sizeof(struct v4l2_subdev_format));

error:
		mutex_unlock(&ccic->ccic_mutex);
	}

	return ret;
}

void pxa_ccic_ctrl_pixclk(struct mvisp_device *isp, int on)
{
	u32 regval;

	if (on) {
		if (isp->cpu_type == MV_MMP3)
			regval = (0x3 << 29) | (400 / 26);
		else
			regval = (0x3 << 29) | (312 / 24);
		mvisp_reg_writel(isp, regval,
			CCIC_ISP_IOMEM_1, CCIC_CLOCK_CTRL);
		regval = (1 << 25) | 0x800003C;
		mvisp_reg_writel(isp, regval,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_1);
	} else {
		regval = 0;
		mvisp_reg_writel(isp, regval,
			CCIC_ISP_IOMEM_1, CCIC_CLOCK_CTRL);
		regval = 0x800003C;
		mvisp_reg_writel(isp, regval,
			CCIC_ISP_IOMEM_1, CCIC_CTRL_1);
	}
}
static int ccic_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);
	struct mvisp_device *isp = ccic->isp;
	u32 regval;

	mutex_lock(&ccic->ccic_mutex);

	switch (enable) {
	case ISP_PIPELINE_STREAM_CONTINUOUS:
		if (ccic->stream_refcnt++ == 0) {
			ccic_configure_mipi(ccic);

			ccic->state = enable;
		}
		break;
	case ISP_PIPELINE_STREAM_STOPPED:
		if (--ccic->stream_refcnt == 0) {
			regval = mvisp_reg_readl(isp,
				CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
			regval &= ~0x1;
			mvisp_reg_writel(isp, regval,
				CCIC_ISP_IOMEM_1, CCIC_CTRL_0);
			mvisp_reg_writel(isp, 0x0,
				CCIC_ISP_IOMEM_1, CCIC_IRQ_MASK);
			mvisp_reg_writel(isp, 0xFFFFFFFF,
				CCIC_ISP_IOMEM_1, CCIC_IRQ_STATUS);
			ccic_clear_mipi(ccic);

			ccic->state = enable;
		} else if (ccic->stream_refcnt < 0)
			ccic->stream_refcnt = 0;
		break;
	default:
		break;
	}

	mutex_unlock(&ccic->ccic_mutex);

	return 0;
}

static int ccic_io_set_stream(struct v4l2_subdev *sd, int *enable)
{
	enum isp_pipeline_stream_state state;

	if ((NULL == sd) || (NULL == enable) || (*enable < 0))
		return -EINVAL;

	state = *enable ? ISP_PIPELINE_STREAM_CONTINUOUS :\
			ISP_PIPELINE_STREAM_STOPPED;

	return ccic_set_stream(sd, state);
}

static int ccic_io_config_mipi(struct isp_ccic_device *ccic,
	struct v4l2_ccic_config_mipi *mipi_cfg)
{
	if (NULL == mipi_cfg)
		return -EINVAL;

	if (mipi_cfg->start_mipi != 0) {
		ccic->lanes = mipi_cfg->lanes;
		ccic_configure_mipi(ccic);
	} else
		ccic_clear_mipi(ccic);

	return 0;
}

static int ccic_get_sensor_mclk(struct isp_ccic_device *ccic,
		int *mclk)
{
	struct mvisp_device *isp = ccic->isp;
	struct clk *clk;
	int clkdiv;

	if (!mclk || !isp)
		return -EINVAL;

	clk = isp->clock[isp->isp_clknum + isp->ccic_clknum - 1];
	if (!clk)
		return -EINVAL;

	clkdiv = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_CLOCK_CTRL) & CCIC_CLKDIV;

/*the mclk is 26M on mmp3, and can not change it through ccic*/
	if (isp->cpu_type == MV_MMP3) {
		*mclk = 26;
	} else {
		if (!clkdiv)
			return -EINVAL;

		*mclk = clk_get_rate(clk) / clkdiv / 1000000;
	}

	return 0;
}

static long ccic_ioctl(struct v4l2_subdev *sd
			, unsigned int cmd, void *arg)
{
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);
	int ret;

	switch (cmd) {
	case VIDIOC_PRIVATE_CCIC_CONFIG_MIPI:
		ret = ccic_io_config_mipi
			(ccic, (struct v4l2_ccic_config_mipi *)arg);
		break;
	case VIDIOC_PRIVATE_CCIC_DUMP_REGISTERS:
		ret = ccic_dump_registers
			(ccic, (struct v4l2_ccic_dump_registers *)arg);
		break;
	case VIDIOC_PRIVATE_CCIC_SET_STREAM:
		ret = ccic_io_set_stream(sd, (int *)arg);
		break;
	case VIDIOC_PRIVATE_CCIC_GET_SENSOR_MCLK:
		ret = ccic_get_sensor_mclk(ccic, (int *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}

	return ret;
}

static int ccic_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);
	struct mvisp_device *isp = ccic->isp;
	int ret;

	/* CSI attached, now add debug interface for it*/
	ccic->mcd_dphy = default_mcd_dphy;
	strcpy(ccic->mcd_dphy.entity.name, "dphy");
	ccic->mcd_dphy_hw.hw_ctx	= ccic;
	ccic->mcd_dphy_hw.reg_write	= &csi_dphy_write;
	ccic->mcd_dphy_hw.reg_read	= &csi_dphy_read;
	ccic->mcd_dphy.entity.priv = &ccic->mcd_dphy_hw;

	/* FIXME: mount the dphy node under LCD port, this is just a W/R
	 * need to modify MCD to support complex topology for MediaControl */
	ret = mcd_entity_init(&ccic->mcd_dphy.entity,
			&isp->mcd_root_display.mcd);
	if (ret < 0)
		return ret;
	isp->mcd_root_display.pitem[MCD_DPHY] = &ccic->mcd_dphy.entity;
	printk(KERN_INFO "cam: mount node debugfs/%s/%s\n",
			isp->mcd_root_display.mcd.name,
			ccic->mcd_dphy.entity.name);
#endif
	return ccic_init_formats(sd, fh);
}

static int ccic_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);

	if (NULL == ccic)
		return -EINVAL;

	mutex_lock(&ccic->ccic_mutex);
	if (ccic->state != ISP_PIPELINE_STREAM_STOPPED) {
		ccic->stream_refcnt = 1;
		mutex_unlock(&ccic->ccic_mutex);
		ccic_set_stream(sd, 0);
	} else
		mutex_unlock(&ccic->ccic_mutex);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	printk(KERN_INFO "cam: dismount node debugfs/%s/%s\n",
		ccic->isp->mcd_root_display.mcd.name,
		ccic->mcd_dphy.entity.name);
	mcd_entity_remove(&ccic->mcd_dphy.entity);
#endif
	return 0;
}

/* subdev core ooperations */
static const struct v4l2_subdev_core_ops ccic_core_ops = {
	.ioctl = ccic_ioctl,
};

/* subdev video operations */
static const struct v4l2_subdev_video_ops ccic_video_ops = {
	.s_stream = ccic_set_stream,
};

/* subdev pad operations */
static const struct v4l2_subdev_pad_ops ccic_pad_ops = {
	.enum_mbus_code = ccic_enum_mbus_code,
	.enum_frame_size = ccic_enum_frame_size,
	.get_fmt = ccic_get_format,
	.set_fmt = ccic_set_format,
};

/* subdev operations */
static const struct v4l2_subdev_ops ccic_ops = {
	.core = &ccic_core_ops,
	.video = &ccic_video_ops,
	.pad = &ccic_pad_ops,
};

/* subdev internal operations */
static const struct v4l2_subdev_internal_ops ccic_internal_ops = {
	.open = ccic_open,
	.close = ccic_close,
};

/* -----------------------------------------------------------------------------
 * Media entity operations
 */

/*
 * ccic_link_setup - Setup CCIC connections.
 * @entity : Pointer to media entity structure
 * @local  : Pointer to local pad array
 * @remote : Pointer to remote pad array
 * @flags  : Link flags
 * return -EINVAL or zero on success
 */
static int ccic_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct isp_ccic_device *ccic = v4l2_get_subdevdata(sd);

	switch (local->index | media_entity_type(remote->entity)) {
	case CCIC_PAD_SINK | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			ccic->input |= CCIC_INPUT_SENSOR;
		else
			ccic->input &= ~CCIC_INPUT_SENSOR;
		break;
	case CCIC_PAD_DMA_SRC | MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			ccic->output |= CCIC_OUTPUT_MEMORY;
		else
			ccic->output &= ~CCIC_OUTPUT_MEMORY;
		break;
	case CCIC_PAD_ISP_SRC | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			ccic->output |= CCIC_OUTPUT_ISP;
		else
			ccic->output &= ~CCIC_OUTPUT_ISP;
		break;
	default:
		/* Link from camera to CCIC is fixed... */
		return -EINVAL;
	}
	return 0;
}

/* media operations */
static const struct media_entity_operations ccic_media_ops = {
	.link_setup = ccic_link_setup,
};

/*
 * ccic_init_entities - Initialize subdev and media entity.
 * @ccic: Pointer to ccic structure.
 * return -ENOMEM or zero on success
 */
static int ccic_init_entities(struct isp_ccic_device *ccic,
		enum isp_ccic_identity ccic_id)
{
	struct v4l2_subdev *sd = &ccic->subdev;
	struct media_pad *pads = ccic->pads;
	struct media_entity *me = &sd->entity;
	int ret;

	ccic->sensor_type = SENSOR_INVALID;
	ccic->state = ISP_PIPELINE_STREAM_STOPPED;
	ccic->dma_state = CCIC_DMA_IDLE;
	ccic->stream_refcnt = 0;
	spin_lock_init(&ccic->mipi_flag_lock);
	spin_lock_init(&ccic->irq_lock);
	ccic->ccic_id = ccic_id;
	mutex_init(&ccic->ccic_mutex);

	v4l2_subdev_init(sd, &ccic_ops);
	sd->internal_ops = &ccic_internal_ops;
	strlcpy(sd->name, "pxaccic", sizeof(sd->name));

	sd->grp_id = 1 << 16;	/* group ID for isp subdevs */
	v4l2_set_subdevdata(sd, ccic);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	pads[CCIC_PAD_ISP_SRC].flags = MEDIA_PAD_FL_SOURCE;
	pads[CCIC_PAD_DMA_SRC].flags = MEDIA_PAD_FL_SOURCE;
	pads[CCIC_PAD_SINK].flags = MEDIA_PAD_FL_SINK;

	me->ops = &ccic_media_ops;
	ret = media_entity_init(me, CCIC_PADS_NUM, pads, 0);
	if (ret < 0)
		return ret;

	ccic_init_formats(sd, NULL);

	/* Video device node */
	ccic->video_out.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ccic->video_out.ops = &ccic_ispvideo_ops;
	ccic->video_out.isp = ccic->isp;

	ret = mvisp_video_init(&ccic->video_out, ISP_VIDEO_CCIC1_NAME);
	if (ret < 0)
		return ret;

	/* Connect the CCIC subdev to the video node. */
	ret = media_entity_create_link
				(&ccic->subdev.entity, CCIC_PAD_DMA_SRC,
				&ccic->video_out.video.entity, 0, 0);
	if (ret < 0)
		return ret;

	return 0;
}

void pxa_ccic_unregister_entities(struct isp_ccic_device *ccic)
{
	media_entity_cleanup(&ccic->subdev.entity);

	v4l2_device_unregister_subdev(&ccic->subdev);
	mvisp_video_unregister(&ccic->video_out);
}

int pxa_ccic_register_entities(struct isp_ccic_device *ccic,
				    struct v4l2_device *vdev)
{
	int ret;

	/* Register the subdev and video nodes. */
	ret = v4l2_device_register_subdev(vdev, &ccic->subdev);
	if (ret < 0)
		goto error;

	ret = mvisp_video_register(&ccic->video_out, vdev);
	if (ret < 0)
		goto error;

	return 0;

error:
	pxa_ccic_unregister_entities(ccic);
	return ret;
}

/* -----------------------------------------------------------------------------
 * ISP CCIC initialisation and cleanup
 */

/*
 * pxa_ccic_cleanup - Routine for module driver cleanup
 */
void pxa_ccic_cleanup(struct mvisp_device *isp)
{
}

/*
 * pxa_ccic_init - Routine for module driver init
 */
int pxa_ccic_init(struct mvisp_device *isp)
{
	struct isp_ccic_device *ccic1 = &isp->mvisp_ccic1;
	struct isp_ccic_device *ccic2 = &isp->mvisp_ccic2;
	int ret;

	ccic1->isp = isp;
	ret = ccic_init_entities(ccic1, CCIC_ID_1);
	if (ret < 0)
		goto fail;

	ccic2->isp = isp;
	ret = ccic_init_entities(ccic2, CCIC_ID_2);
	if (ret < 0)
		goto fail;

	return 0;
fail:
	pxa_ccic_cleanup(isp);
	return ret;
}

void pxa_ccic_set_sensor_type(struct isp_ccic_device *ccic,
		enum mv_isp_sensor_type sensor_type)
{
	ccic->sensor_type = sensor_type;
	return;
}


static void ccic_isr_buffer(struct isp_ccic_device *ccic,
		enum isp_ccic_irq_type irqeof)
{
	struct isp_video_buffer *buffer;
	struct mvisp_device *isp = ccic->isp;

	buffer = mvisp_video_buffer_next(&ccic->video_out, 0);
	if (buffer != NULL) {
		buffer->state = ISP_BUF_STATE_ACTIVE;
		ccic_set_dma_addr(ccic,
			buffer->paddr[ISP_BUF_PADDR], irqeof);
	} else {
		if (isp->dummy_paddr != 0)
			ccic_set_dma_addr(ccic, isp->dummy_paddr, irqeof);
		else
			ccic_disable_hw(ccic, &ccic->video_out);
	}

	return;
}

void ccic_isr_dummy_buffer(struct isp_ccic_device *ccic,
		enum isp_ccic_irq_type irqeof)
{
	struct isp_video *video;
	unsigned long flags;
	struct isp_video_buffer *buffer;

	video = &ccic->video_out;
	spin_lock_irqsave(&video->irq_lock, flags);
	if (list_empty(&video->dmaidlequeue)) {
		spin_unlock_irqrestore(&video->irq_lock, flags);
		return;
	}
	buffer = list_first_entry(&video->dmaidlequeue,
		struct isp_video_buffer, dmalist);
	list_del(&buffer->dmalist);
	video->dmaidlecnt--;
	list_add_tail(&buffer->dmalist, &video->dmabusyqueue);
	video->dmabusycnt++;
	buffer->state = ISP_BUF_STATE_ACTIVE;
	ccic_set_dma_addr(ccic, buffer->paddr[ISP_BUF_PADDR], irqeof);
	spin_unlock_irqrestore(&video->irq_lock, flags);

	return;
}

void pxa_ccic_dma_isr_handler(struct isp_ccic_device *ccic,
				unsigned long irq_status)
{
	struct mvisp_device *isp = ccic->isp;
	u32 regval;
	unsigned long flag;

	spin_lock_irqsave(&ccic->irq_lock, flag);

	if (irq_status & CCIC_IRQ_STATUS_EOF0) {
		regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_Y0_BASE_ADDR);
		if (regval != isp->dummy_paddr)
			ccic_isr_buffer(ccic, CCIC_EOF0);
		else
			ccic_isr_dummy_buffer(ccic, CCIC_EOF0);
	}

	if (irq_status & CCIC_IRQ_STATUS_EOF1) {
		regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_Y1_BASE_ADDR);
		if (regval != isp->dummy_paddr)
			ccic_isr_buffer(ccic, CCIC_EOF1);
		else
			ccic_isr_dummy_buffer(ccic, CCIC_EOF1);
	}

	if (irq_status & CCIC_IRQ_STATUS_EOF2) {
		regval = mvisp_reg_readl(isp,
			CCIC_ISP_IOMEM_1, CCIC_Y2_BASE_ADDR);
		if (regval != isp->dummy_paddr)
			ccic_isr_buffer(ccic, CCIC_EOF2);
		else
			ccic_isr_dummy_buffer(ccic, CCIC_EOF2);
	}

	spin_unlock_irqrestore(&ccic->irq_lock, flag);

	return;
}
