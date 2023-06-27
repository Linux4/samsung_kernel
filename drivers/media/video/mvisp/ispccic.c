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
#include <linux/interrupt.h>
#include <linux/export.h>

#include "isp.h"
#include "ispreg.h"
#include "ispccic.h"

#define MC_CCIC_DRV_NAME	"ccic"
#define CCIC_CORE_NAME		"pxaccic"
#define CCIC_DPHY_NAME		"ccic-dphy"
#define CCIC_IRQ_NAME		"ccic-irq"

#define MAX_CCIC_CH_USED	2

static const struct pad_formats ccic_input_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
};

static const struct pad_formats ccic_output_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_1X16, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_COLORSPACE_SRGB},
};

static inline struct ccic_dphy_t *get_dphy(struct ccic_device *ccic)
{
	union agent_id id = {
		.dev_type	= ccic->agent.hw.id.dev_type,
		.dev_id	= ccic->agent.hw.id.dev_id,
		.mod_type	= MAP_AGENT_NORMAL,
		.mod_id	= AGENT_CCIC_DPHY,
	};
	struct map_agent *agent = container_of(
		get_agent(ccic->agent.hw.mngr, id), struct map_agent, hw);
	return agent->drv_priv;
}

static void __maybe_unused ccic_dump_regs(struct ccic_device *ccic)
{
	unsigned long regval;

	regval = ccic_read(ccic, CCIC_Y0_BASE_ADDR);
	dev_warn(ccic->dev, "ccic_set_stream Y0: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_Y1_BASE_ADDR);
	dev_warn(ccic->dev, "ccic_set_stream Y1: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_Y2_BASE_ADDR);
	dev_warn(ccic->dev, "ccic_set_stream Y2: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_IRQ_RAW_STATUS);
	dev_warn(ccic->dev, "ccic_set_stream IRQRAWSTATE: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_IRQ_STATUS);
	dev_warn(ccic->dev, "ccic_set_stream IRQSTATE: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_IRQ_MASK);
	dev_warn(ccic->dev, "ccic_set_stream IRQMASK: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_CTRL_0);
	dev_warn(ccic->dev, "ccic_set_stream CTRL0: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_CTRL_1);
	dev_warn(ccic->dev, "ccic_set_stream CTRL1: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_CLOCK_CTRL);
	dev_warn(ccic->dev, "ccic_set_stream CLKCTRL: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_CSI2_IRQ_RAW_STATUS);
	dev_warn(ccic->dev, "ccic_set_stream MIPI STATUS: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_CSI2_DPHY3);
	dev_warn(ccic->dev, "ccic_set_stream MIPI DPHY3: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_CSI2_DPHY5);
	dev_warn(ccic->dev, "ccic_set_stream MIPI DPHY5: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_CSI2_DPHY6);
	dev_warn(ccic->dev, "ccic_set_stream MIPI DPHY6: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_IMG_SIZE);
	dev_warn(ccic->dev, "ccic_set_stream SIZE: 0x%08lX\n", regval);
	regval = ccic_read(ccic, CCIC_IMG_PITCH);
	dev_warn(ccic->dev, "ccic_set_stream PITCH: 0x%08lX\n", regval);
}

static int ccic_dump_registers(struct ccic_device *ccic,
			struct v4l2_ccic_dump_registers *regs)
{
	if (NULL == regs)
		return -EINVAL;

	regs->y0_base_addr = ccic_read(ccic, CCIC_Y0_BASE_ADDR);
	regs->y1_base_addr = ccic_read(ccic, CCIC_Y1_BASE_ADDR);
	regs->y2_base_addr = ccic_read(ccic, CCIC_Y2_BASE_ADDR);
	regs->irq_raw_status = ccic_read(ccic, CCIC_IRQ_RAW_STATUS);
	regs->irq_status = ccic_read(ccic, CCIC_IRQ_STATUS);
	regs->irq_mask = ccic_read(ccic, CCIC_IRQ_MASK);
	regs->ctrl_0 = ccic_read(ccic, CCIC_CTRL_0);
	regs->ctrl_1 = ccic_read(ccic, CCIC_CTRL_1);
	regs->clock_ctrl = ccic_read(ccic, CCIC_CLOCK_CTRL);
	regs->csi2_irq_raw_status = ccic_read(ccic, CCIC_CSI2_IRQ_RAW_STATUS);
	regs->csi2_dphy3 = ccic_read(ccic, CCIC_CSI2_DPHY3);
	regs->csi2_dphy5 = ccic_read(ccic, CCIC_CSI2_DPHY5);
	regs->csi2_dphy6 = ccic_read(ccic, CCIC_CSI2_DPHY6);
	regs->img_size = ccic_read(ccic, CCIC_IMG_SIZE);
	regs->img_pitch = ccic_read(ccic, CCIC_IMG_PITCH);

	return 0;
}


static void ccic_set_dma_addr(struct ccic_device *ccic,
	dma_addr_t	paddr, enum isp_ccic_irq_type irqeof)
{
	BUG_ON(paddr == 0);

	switch (irqeof) {
	case CCIC_EOF0:
		ccic_write(ccic, CCIC_Y0_BASE_ADDR, paddr);
		break;
	case CCIC_EOF1:
		ccic_write(ccic, CCIC_Y1_BASE_ADDR, paddr);
		break;
	case CCIC_EOF2:
		ccic_write(ccic, CCIC_Y2_BASE_ADDR, paddr);
		break;
	default:
		break;
	}

	return;
}

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
#include <mach/regs-apmu.h>

static void csi_dphy_write(void *hw_ctx, const struct csi_dphy_reg *regs)
{
	struct ccic_device *ccic = hw_ctx;
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
	ccic_write(ccic, CCIC_CSI2_DPHY3, regval);

	regval = regs->cl_settle & 0xFF;
	regval = regs->cl_termen | (regval << 8);
	ccic_write(ccic, CCIC_CSI2_DPHY6, regval);

	regval = (1 << regs->lane) - 1;
	regval = regval | (regval << 4);
	ccic_write(ccic, CCIC_CSI2_DPHY5, regval);

	regval = (regs->lane - 1) & 0x03;	/* support 4 lane at most */
	regval = (regval << 1) | 0x41;
	ccic_write(ccic, CCIC_CSI2_CTRL0, regval);

	/* start CCIC */
	ccic_setbit(ccic, CCIC_CTRL_0, 0x1);
};

static void csi_dphy_read(void *hw_ctx, struct csi_dphy_reg *regs)
{
	struct ccic_device *ccic = hw_ctx;
	u32 phy3, phy5, phy6;

	phy3	= ccic_read(ccic, CCIC_CSI2_DPHY3);
	phy5	= ccic_read(ccic, CCIC_CSI2_DPHY5);
	phy6	= ccic_read(ccic, CCIC_CSI2_DPHY6);

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

/* -----------------------------------------------------------------------------
 * ISP video operations */
static struct ccic_device *find_ccic_from_video(struct map_vnode *video)
{
	return video_get_agent(video)->drv_priv;
}

static int ccic_enable_hw(struct ccic_device *ccic)
{
	if (ccic->output & CCIC_OUTPUT_MEMORY) {
		ccic_write(ccic, CCIC_IRQ_STATUS, ~0);
		ccic_write(ccic, CCIC_IRQ_MASK, 0x3);
		ccic_setbit(ccic, CCIC_CTRL_0, 0x1);
		ccic->dma_state = CCIC_DMA_BUSY;
	}
	return 0;
}

static int ccic_disable_hw(struct ccic_device *ccic)
{
	if (ccic->output & CCIC_OUTPUT_MEMORY) {
		ccic_clrbit(ccic, CCIC_CTRL_0, 0x1);
		ccic_write(ccic, CCIC_IRQ_MASK, 0);
		ccic_write(ccic, CCIC_IRQ_STATUS, ~0);
		ccic->dma_state = CCIC_DMA_IDLE;
	}
	return 0;
}

static int ccic_load_buffer(struct ccic_device *ccic, int ch)
{
	struct map_videobuf *buffer = NULL;
	unsigned long flags;

	buffer = map_vnode_xchg_buffer(agent_get_video(&ccic->agent),
			false, false);
	if (buffer == NULL)
		return -EINVAL;

	spin_lock_irqsave(&agent_get_video(&ccic->agent)->vb_lock, flags);
	ccic_set_dma_addr(ccic, buffer->paddr[ISP_BUF_PADDR], ch);
	spin_unlock_irqrestore(&agent_get_video(&ccic->agent)->vb_lock, flags);

	return 0;
}


static int ccic_video_stream_on_notify(struct map_vnode *vnode)
{
	struct ccic_device *ccic;
	int ch, ret;

	vnode->min_buf_cnt = MAX_CCIC_CH_USED;
	ccic = find_ccic_from_video(vnode);
	if (ccic == NULL)
		return -EINVAL;

	mutex_lock(&ccic->ccic_mutex);

	if (ccic->dma_state == CCIC_DMA_BUSY) {
		mutex_unlock(&ccic->ccic_mutex);
		return 0;
	}

	/*ccic_configure_mipi(get_dphy(ccic));*/

	for (ch = 0; ch < MAX_CCIC_CH_USED; ch++) {
		ret = ccic_load_buffer(ccic, ch);
		if (ret != 0)
			return -EINVAL;
	}

	ccic_enable_hw(ccic);

	mutex_unlock(&ccic->ccic_mutex);

	return 0;
}

static int ccic_video_stream_off_notify(struct map_vnode *vnode)
{
	struct ccic_device *ccic;

	ccic = find_ccic_from_video(vnode);
	if (ccic == NULL)
		return -EINVAL;

	mutex_lock(&ccic->ccic_mutex);

	if (ccic->dma_state == CCIC_DMA_IDLE) {
		mutex_unlock(&ccic->ccic_mutex);
		return 0;
	}

	ccic_disable_hw(ccic);

	mutex_unlock(&ccic->ccic_mutex);

	return 0;
}

static int ccic_video_qbuf_notify(struct map_vnode *vnode)
{
	return 0;
}

static const struct vnode_ops ccic_ispvideo_ops = {
	.qbuf_notify = ccic_video_qbuf_notify,
	.stream_on_notify = ccic_video_stream_on_notify,
	.stream_off_notify = ccic_video_stream_off_notify,
};

/* -----------------------------------------------------------------------------
 * V4L2 subdev operations
 */

static struct v4l2_mbus_framefmt *
__ccic_get_format(struct ccic_device *ccic, struct v4l2_subdev_fh *fh,
		  unsigned int pad, enum v4l2_subdev_format_whence which)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &ccic->formats[pad];
}

static int
ccic_try_format(struct ccic_device *ccic, struct v4l2_subdev_fh *fh,
		unsigned int pad, struct v4l2_mbus_framefmt *fmt,
		enum v4l2_subdev_format_whence which)
{
	unsigned int i;
	int ret = 0;

	switch (pad) {
	case CCIC_PADI_SNSR:
	case CCIC_PADI_DPHY:
		for (i = 0; i < ARRAY_SIZE(ccic_input_fmts); i++) {
			if (fmt->code == ccic_input_fmts[i].mbusfmt) {
				fmt->colorspace = ccic_input_fmts[i].colorspace;
				break;
			}
		}

		if (i >= ARRAY_SIZE(ccic_input_fmts))
			fmt->code = V4L2_MBUS_FMT_SBGGR10_1X10;

		break;

	case CCIC_PADO_DXO:
	case CCIC_PADO_VDEV:
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;
	int ret = 0;

	mutex_lock(&ccic->ccic_mutex);
	switch (code->pad) {
	case CCIC_PADI_SNSR:
	case CCIC_PADI_DPHY:
		if (code->index >= ARRAY_SIZE(ccic_input_fmts)) {
			ret = -EINVAL;
			goto error;
		}
		code->code = ccic_input_fmts[code->index].mbusfmt;
		break;
	case CCIC_PADO_DXO:
	case CCIC_PADO_VDEV:
		if (code->index >= ARRAY_SIZE(ccic_output_fmts)) {
			ret = -EINVAL;
			goto error;
		}
		code->code = ccic_output_fmts[code->index].mbusfmt;
		break;
	default:
		ret = -EINVAL;
		break;
	}

error:
	mutex_unlock(&ccic->ccic_mutex);
	return ret;
}

static int ccic_enum_frame_size(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_frame_size_enum *fse)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;
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

static int ccic_config_format(struct ccic_device *ccic, unsigned int pad)
{
	struct v4l2_mbus_framefmt *format = &ccic->formats[pad];
	unsigned long width, height, regval, bytesperline;
	unsigned long ctrl0val = 0;
	unsigned long ypitch;
	int ret = 0;

	width = format->width;
	height = format->height;

	ctrl0val = ccic_read(ccic, CCIC_CTRL_0);
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

	regval = (height << 16) | bytesperline;
	ccic_write(ccic, CCIC_IMG_SIZE, regval);
	regval = (ypitch << 2);
	ccic_write(ccic, CCIC_IMG_PITCH, regval);
	ccic_write(ccic, CCIC_IMG_OFFSET, 0);
	ccic_write(ccic, CCIC_CTRL_0, ctrl0val);
	return ret;
}

static int ccic_set_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			   struct v4l2_subdev_format *fmt)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;
	int ret = 0;

	if (fh == NULL) {
		memset(&format, 0, sizeof(format));
		format.pad = CCIC_PADI_SNSR;
		format.which =
			fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
		format.format.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		format.format.width = 640;
		format.format.height = 480;
		format.format.colorspace = V4L2_COLORSPACE_SRGB;
		format.format.field = V4L2_FIELD_NONE;
		ret = ccic_set_format(sd, fh, &format);
		format.pad = CCIC_PADI_DPHY;
		ret = ccic_set_format(sd, fh, &format);

		format.pad = CCIC_PADO_DXO;
		format.which =
			fh ? V4L2_SUBDEV_FORMAT_TRY : V4L2_SUBDEV_FORMAT_ACTIVE;
		format.format.code = V4L2_MBUS_FMT_SBGGR10_1X10;
		format.format.width = 640;
		format.format.height = 480;
		format.format.colorspace = V4L2_COLORSPACE_SRGB;
		ret = ccic_set_format(sd, fh, &format);
		format.pad = CCIC_PADO_VDEV;
		ret = ccic_set_format(sd, fh, &format);
	} else {
	/* Copy the active format to a newly opened fh structure */
		mutex_lock(&ccic->ccic_mutex);
		format_active = __ccic_get_format
			(ccic, fh, CCIC_PADI_DPHY, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (format_active == NULL) {
			ret = -EINVAL;
			goto error;
		}
		format_try = __ccic_get_format
			(ccic, fh, CCIC_PADI_DPHY, V4L2_SUBDEV_FORMAT_TRY);
		if (format_try == NULL) {
			ret = -EINVAL;
			goto error;
		}
		memcpy(format_try, format_active,
				sizeof(struct v4l2_subdev_format));

		format_active = __ccic_get_format
			(ccic, fh, CCIC_PADO_DXO, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (format_active == NULL) {
			ret = -EINVAL;
			goto error;
		}
		format_try = __ccic_get_format
			(ccic, fh, CCIC_PADO_DXO, V4L2_SUBDEV_FORMAT_TRY);
		if (format_try == NULL) {
			ret = -EINVAL;
			goto error;
		}
		memcpy(format_try, format_active,
				sizeof(struct v4l2_subdev_format));

		format_active = __ccic_get_format
			(ccic, fh, CCIC_PADO_VDEV, V4L2_SUBDEV_FORMAT_ACTIVE);
		if (format_active == NULL) {
			ret = -EINVAL;
			goto error;
		}
		format_try = __ccic_get_format
			(ccic, fh, CCIC_PADO_VDEV, V4L2_SUBDEV_FORMAT_TRY);
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

static int ccic_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;

	mutex_lock(&ccic->ccic_mutex);

	switch (enable) {
	case ISP_PIPELINE_STREAM_CONTINUOUS:
		if (ccic->stream_refcnt++ == 0) {
			struct v4l2_subdev *dphy_sd =
				&(get_dphy(ccic)->agent.subdev);
			/* just a W/R for now, should be called by pipeline */
			v4l2_subdev_call(dphy_sd, video, s_stream, 1);
			ccic->state = enable;
		}
		break;
	case ISP_PIPELINE_STREAM_STOPPED:
		if (--ccic->stream_refcnt == 0) {
			struct v4l2_subdev *dphy_sd =
				&(get_dphy(ccic)->agent.subdev);
			ccic_clrbit(ccic, CCIC_CTRL_0, 0x1);
			ccic_write(ccic, CCIC_IRQ_MASK, 0);
			ccic_write(ccic, CCIC_IRQ_STATUS, ~0);
			/* just a W/R for now, should be called by pipeline */
			v4l2_subdev_call(dphy_sd, video, s_stream, 0);

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

static int ccic_io_config_mipi(struct ccic_dphy_t *dphy,
	struct v4l2_ccic_config_mipi *mipi_cfg)
{
	if (NULL == mipi_cfg)
		return -EINVAL;

	dphy->lanes = mipi_cfg->lanes;

	return 0;
}

void ccic_set_mclk_rate(struct ccic_device *ccic, int on)
{
	struct ccic_plat_data *pdata = &ccic->pdata;
	u32 regval;

	if (on) {
		regval = (0x3 << 29) | (pdata->fclk_mhz / pdata->mclk_mhz);
		ccic_write(ccic, CCIC_CLOCK_CTRL, regval);
	} else {
		ccic_write(ccic, CCIC_CLOCK_CTRL, 0);
	}
}
EXPORT_SYMBOL(ccic_set_mclk_rate);

static inline int ccic_get_sensor_mclk(struct ccic_device *ccic,
		int *mclk)
{
	if (!mclk)
		return -EINVAL;

	*mclk = ccic->pdata.mclk_mhz;
	return 0;
}

static long ccic_ioctl(struct v4l2_subdev *sd
			, unsigned int cmd, void *arg)
{
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;
	int ret;

	switch (cmd) {
	case VIDIOC_PRIVATE_CCIC_CONFIG_MIPI:
		{/* This function don't belong to CCIC-CORE, this is a W/R */
			struct v4l2_subdev *dphy_sd =
				&(get_dphy(ccic)->agent.subdev);
			ret = v4l2_subdev_call(dphy_sd, core, ioctl, cmd, arg);
		}
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;
	hw_tune_power(&ccic->agent.hw, ~0);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;

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
	hw_tune_power(&ccic->agent.hw, 0);
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
	struct map_agent *agent = v4l2_get_subdevdata(sd);
	struct ccic_device *ccic = agent->drv_priv;

	switch (local->index | media_entity_type(remote->entity)) {
	case CCIC_PADI_DPHY | MEDIA_ENT_T_V4L2_SUBDEV:
	case CCIC_PADI_SNSR | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED)
			ccic->input |= CCIC_INPUT_SENSOR;
		else
			ccic->input &= ~CCIC_INPUT_SENSOR;
		break;
	case CCIC_PADO_VDEV | MEDIA_ENT_T_DEVNODE:
		if (flags & MEDIA_LNK_FL_ENABLED)
			ccic->output |= CCIC_OUTPUT_MEMORY;
		else
			ccic->output &= ~CCIC_OUTPUT_MEMORY;
		break;
	case CCIC_PADO_DXO | MEDIA_ENT_T_V4L2_SUBDEV:
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

static void ccic_isr_buffer(struct ccic_device *ccic,
		enum isp_ccic_irq_type irqeof)
{
	struct map_videobuf *buffer;
	struct mvisp_device *isp = ccic->isp;

	buffer = map_vnode_xchg_buffer(agent_get_video(&ccic->agent),
			true, false);
	if (buffer != NULL) {
		ccic_set_dma_addr(ccic,
			buffer->paddr[ISP_BUF_PADDR], irqeof);
	} else {
		if (isp->dummy_paddr != 0)
			ccic_set_dma_addr(ccic, isp->dummy_paddr, irqeof);
		else
			ccic_disable_hw(ccic);
	}

	return;
}

void ccic_isr_dummy_buffer(struct ccic_device *ccic,
		enum isp_ccic_irq_type irqeof)
{
	struct map_vnode *vnode;
	unsigned long flags;
	struct map_videobuf *buffer;

	vnode = agent_get_video(&ccic->agent);
	spin_lock_irqsave(&vnode->vb_lock, flags);
	if (list_empty(&vnode->idle_buf)) {
		spin_unlock_irqrestore(&vnode->vb_lock, flags);
		return;
	}
	buffer = list_first_entry(&vnode->idle_buf,
		struct map_videobuf, hook);
	list_del(&buffer->hook);
	vnode->idle_buf_cnt--;
	list_add_tail(&buffer->hook, &vnode->busy_buf);
	vnode->busy_buf_cnt++;
	ccic_set_dma_addr(ccic, buffer->paddr[ISP_BUF_PADDR], irqeof);
	spin_unlock_irqrestore(&vnode->vb_lock, flags);

	return;
}

static irqreturn_t ccic_irq_handler(int irq, void *data)
{

	struct hw_event *event = data;
	struct hw_agent *agent = event->owner;
	struct ccic_device *ccic = container_of(agent,
					struct map_agent, hw)->drv_priv;
	u32 irq_status;
	u32 regval;
	unsigned long flag;

	spin_lock_irqsave(&ccic->irq_lock, flag);

	irq_status = ccic_read(ccic, CCIC_IRQ_STATUS);
	ccic_write(ccic, CCIC_IRQ_STATUS, irq_status);

	if (irq_status & CCIC_IRQ_STATUS_EOF0) {
		regval = ccic_read(ccic, CCIC_Y0_BASE_ADDR);
		ccic_isr_buffer(ccic, CCIC_EOF0);
	}

	if (irq_status & CCIC_IRQ_STATUS_EOF1) {
		regval = ccic_read(ccic, CCIC_Y1_BASE_ADDR);
		ccic_isr_buffer(ccic, CCIC_EOF1);
	}

	if (irq_status & CCIC_IRQ_STATUS_EOF2) {
		regval = ccic_read(ccic, CCIC_Y2_BASE_ADDR);
		ccic_isr_buffer(ccic, CCIC_EOF2);
	}

	/* And then send a event to all listening agent */
	event->type = irq;
	event->msg = (void *)irq_status;
	hw_event_dispatch(agent, event);

	spin_unlock_irqrestore(&ccic->irq_lock, flag);

	return IRQ_HANDLED;
}

static int ccic_core_hw_init(struct hw_agent *agent)
{
#if 0	/* For dynamic resource allocation, don't check it for now */
	WARN_ON((agent->clock == NULL) && (agent->ops->set_clock == NULL));
	WARN_ON(agent->reg_base == NULL);
	WARN_ON(agent->irq_num == 0);
#endif
	printk(KERN_INFO "ccic-core: map_hw init done\n");
	return 0;
}

static void ccic_core_hw_clean(struct hw_agent *agent)
{
	printk(KERN_INFO "ccic-core: map_hw will be disposed\n");
}

static int ccic_core_hw_open(struct hw_agent *agent)
{
	return 0;
}

static void ccic_core_hw_close(struct hw_agent *agent)
{
}

static int ccic_core_hw_power(struct hw_agent *agent, int level)
{
	struct ccic_device *ccic_core = container_of(agent,
					struct map_agent, hw)->drv_priv;
	/* This is a W/R for dynamic resource allocation */
	agent->reg_base = (void __iomem *)CCIC1_VIRT_BASE;

	if (level)
		ccic_write(ccic_core, CCIC_CTRL_1, CCIC_CTRL1_DMA_128B |
				CCIC_CTRL1_MAGIC_NUMBER | CCIC_CTRL1_2_FRAME);
	else
		ccic_write(ccic_core, CCIC_CTRL_1,
				CCIC_CTRL1_MAGIC_NUMBER | CCIC_CTRL1_2_FRAME);
	return 0;
}

static int ccic_core_hw_qos(struct hw_agent *agent, int qos_level)
{
	/* power off */
	/* platform: change function clock rate */
	/* power on */
	return 0;
}

static int ccic_core_hw_clk(struct hw_agent *agent, int level)
{
	struct ccic_device *ccic_core =
			container_of(agent, struct map_agent, hw)->drv_priv;
	struct clk *fun_clk = agent->clock[0];
	u32 req_rate = ccic_core->pdata.fclk_mhz * MHZ;
	static u32 ori_rate;

	if (level) {
		ori_rate = clk_get_rate(fun_clk);
		if (req_rate && (ori_rate != req_rate))
			clk_set_rate(fun_clk, req_rate);
	} else {
		if (ori_rate && (ori_rate != req_rate))
			clk_set_rate(fun_clk, ori_rate);
	}

	return 0;
}


struct hw_agent_ops ccic_core_hw_ops = {
	.init		= ccic_core_hw_init,
	.clean		= ccic_core_hw_clean,
	.set_power	= ccic_core_hw_power,
	.open		= ccic_core_hw_open,
	.close		= ccic_core_hw_close,
	.set_qos	= ccic_core_hw_qos,
	.set_clock	= ccic_core_hw_clk,
};

static int ccic_core_map_add(struct map_agent *agent)
{
	struct ccic_device *ccic = agent->drv_priv;
	struct v4l2_subdev *sd = &agent->subdev;
	struct media_pad *pads = agent->pads;
	int ret = 0;

	/* prepare drv_priv */
	ccic->state = ISP_PIPELINE_STREAM_STOPPED;
	ccic->dma_state = CCIC_DMA_IDLE;
	ccic->stream_refcnt = 0;
	spin_lock_init(&ccic->irq_lock);
	mutex_init(&ccic->ccic_mutex);

	/* prepare subdev */
	v4l2_subdev_init(sd, &ccic_ops);
	sd->internal_ops = &ccic_internal_ops;
	strlcpy(sd->name, ccic->agent.hw.name, sizeof(sd->name));
	ccic->agent.hw.name = sd->name;
	sd->grp_id = GID_AGENT_GROUP;
	v4l2_set_subdevdata(sd, &ccic->agent);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->entity.ops = &ccic_media_ops;
	if (ccic->agent.ctrl_cnt)
		sd->ctrl_handler = &ccic->agent.ctrl_handler;

	/* Prepare agent */
	pads[CCIC_PADO_DXO].flags = MEDIA_PAD_FL_SOURCE;
	pads[CCIC_PADO_VDEV].flags = MEDIA_PAD_FL_SOURCE;
	pads[CCIC_PADI_SNSR].flags = MEDIA_PAD_FL_SINK;
	pads[CCIC_PADI_DPHY].flags = MEDIA_PAD_FL_SINK;
	agent->pad_vdev = CCIC_PADO_VDEV;
	agent->pads_cnt	= CCIC_PAD_END;

	agent->vops		= &ccic_ispvideo_ops;
	agent->video_type	= ISP_VIDEO_CCIC;
	return ret;
}

struct map_agent_ops ccic_core_map_ops = {
	.add		= ccic_core_map_add,
};

/********************************* CCIC-DPHY *********************************/

static int ccic_clear_mipi(struct ccic_dphy_t *dphy)
{
	unsigned long mipi_lock_flags;

	spin_lock_irqsave(&dphy->mipi_flag_lock, mipi_lock_flags);
	if (dphy->mipi_config_flag == MIPI_NOT_SET) {
		spin_unlock_irqrestore(&dphy->mipi_flag_lock, mipi_lock_flags);
		return 0;
	}

	agent_write(&dphy->agent.hw, CCIC_CSI2_DPHY3, 0);
	agent_write(&dphy->agent.hw, CCIC_CSI2_DPHY5, 0);
	agent_write(&dphy->agent.hw, CCIC_CSI2_DPHY6, 0);
	agent_write(&dphy->agent.hw, CCIC_CSI2_CTRL0, 0);

	dphy->mipi_config_flag = MIPI_NOT_SET;
	spin_unlock_irqrestore(&dphy->mipi_flag_lock, mipi_lock_flags);
	return 0;
}

struct csi_dphy_calc dphy_calc_profiles[] = {
	{
		.name		= "safe",
		.hs_termen_pos	= 0,
		.hs_settle_pos	= 50,
	},

};

int dphy_calc_reg(struct csi_dphy_desc *pdesc, \
		struct csi_dphy_calc *algo, struct csi_dphy_reg *preg)
{
	u32 ps_period, ps_ui, ps_termen_max, ps_prep_max, ps_prep_min;
	u32 ps_sot_min, ps_termen, ps_settle;
	ps_period = MHZ / pdesc->clk_freq;
	ps_ui = ps_period / 2;
	ps_termen_max	= 35000 + 4 * ps_ui;
	ps_prep_min	= 40000 + 4 * ps_ui;
	ps_prep_max	= 85000 + 6 * ps_ui;
	ps_sot_min	= 145000 + 10 * ps_ui;
	ps_termen	= ps_termen_max + algo->hs_termen_pos * ps_period;
	ps_settle	= 1000 * (pdesc->hs_prepare + pdesc->hs_zero * \
						algo->hs_settle_pos / 100);

	preg->cl_termen	= 0x00;
	preg->cl_settle	= 0x04;
	preg->cl_miss	= 0x00;
	/* term_en = round_up(ps_termen / ps_period) - 1 */
	preg->hs_termen	= (ps_termen + ps_period - 1) / ps_period - 1;
	/* For Marvell DPHY, Ths-settle started from HS-0, not VILmax */
	ps_settle -= (preg->hs_termen + 1) * ps_period;
	/* round_up(ps_settle / ps_period) - 1 */
	preg->hs_settle = (ps_settle + ps_period - 1) / ps_period - 1;
	preg->hs_rx_to	= 0xFFFF;
	preg->lane	= pdesc->nr_lane;
	return 0;
}

static int ccic_csi_dphy_write(struct ccic_dphy_t *dphy)
{
	struct csi_dphy_reg *regs = &dphy->dphy_cfg;
	struct mvisp_device *isp = dphy->isp;
	int lanes = dphy->dphy_desc.nr_lane;
	unsigned int dphy3_val;
	unsigned int dphy5_val;
	unsigned int dphy6_val;
	unsigned int ctrl0_val;

	if (lanes == 0) {
		dev_warn(dphy->dev, "CCIC lanes num not set, set it to 2\n");
		lanes = 2;
	}

	if (dphy->dphy_set) {
		dphy3_val = regs->hs_settle & 0xFF;
		dphy3_val = regs->hs_termen | (dphy3_val << 8);

		dphy6_val = regs->cl_settle & 0xFF;
		dphy6_val = regs->cl_termen | (dphy6_val << 8);
	} else {
		if (isp->cpu_type == MV_PXA988) {
				dphy3_val = 0x1806;
				dphy6_val = 0xA00;
			} else {
				dphy3_val = 0xA06;
				dphy6_val = 0x1A03;
			}
	}

	dphy5_val = (1 << lanes) - 1;
	dphy5_val = dphy5_val | (dphy5_val << 4);

	ctrl0_val = (lanes - 1) & 0x03;
	ctrl0_val = (ctrl0_val << 1) | 0x41;

	agent_write(&dphy->agent.hw, CCIC_CSI2_DPHY3, dphy3_val);
	agent_write(&dphy->agent.hw, CCIC_CSI2_DPHY5, dphy5_val);
	agent_write(&dphy->agent.hw, CCIC_CSI2_DPHY6, dphy6_val);
	agent_write(&dphy->agent.hw, CCIC_CSI2_CTRL0, ctrl0_val);

	return 0;
}

static int ccic_configure_mipi(struct ccic_dphy_t *dphy)
{
	unsigned long mipi_lock_flags;

	spin_lock_irqsave(&dphy->mipi_flag_lock, mipi_lock_flags);
	if (dphy->mipi_config_flag != MIPI_NOT_SET) {
		spin_unlock_irqrestore(&dphy->mipi_flag_lock, mipi_lock_flags);
		return 0;
	}

	if (dphy->dphy_set)
		dphy_calc_reg(&dphy->dphy_desc,
				dphy_calc_profiles, &dphy->dphy_cfg);
	ccic_csi_dphy_write(dphy);

	dphy->mipi_config_flag = MIPI_SET;
	spin_unlock_irqrestore(&dphy->mipi_flag_lock, mipi_lock_flags);
	return 0;
}

static long ccic_dphy_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct ccic_dphy_t *dphy = container_of(sd, struct ccic_dphy_t,
						agent.subdev);
	int ret;

	switch (cmd) {
	case VIDIOC_PRIVATE_CCIC_CONFIG_MIPI:
		ret = ccic_io_config_mipi(dphy,
					(struct v4l2_ccic_config_mipi *)arg);
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}

static int ccic_dphy_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct ccic_dphy_t *dphy = container_of(sd, struct ccic_dphy_t,
						agent.subdev);

	if (enable)
		return ccic_configure_mipi(dphy);
	else
		return ccic_clear_mipi(dphy);
}

static int ccic_dphy_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ccic_dphy_t *dphy = container_of(sd, struct ccic_dphy_t,
						agent.subdev);
	return hw_tune_power(&dphy->agent.hw, ~0);
}

static int ccic_dphy_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct ccic_dphy_t *dphy = container_of(sd, struct ccic_dphy_t,
						agent.subdev);
	return hw_tune_power(&dphy->agent.hw, 0);
}

static const struct v4l2_subdev_core_ops ccic_dphy_core_ops = {
	.ioctl = ccic_dphy_ioctl,
};

static const struct v4l2_subdev_video_ops ccic_dphy_video_ops = {
	.s_stream = ccic_dphy_set_stream,
};

static const struct v4l2_subdev_ops ccic_dphy_sd_ops = {
	.core = &ccic_dphy_core_ops,
	.video = &ccic_dphy_video_ops,
};

static const struct v4l2_subdev_internal_ops ccic_dphy_internal_ops = {
	.open = ccic_dphy_open,
	.close = ccic_dphy_close,
};

static int ccic_phy_isr(struct hw_agent *dphy, struct hw_event *irq)
{
	u32 irq_status = (int)irq->msg;
	/* WARNING: this event handler is in IRQ context! */
	if (irq_status == 0)
		return 0;
	printk(KERN_INFO "agent '%s' received event '%s'\n",
		dphy->name, irq->name);
	return 0;
}

static void ccic_dphy_hw_clean(struct hw_agent *agent)
{
	printk(KERN_INFO "ccic-dphy: map_hw will be disposed\n");
}

static int ccic_dphy_hw_init(struct hw_agent *agent)
{
	struct ccic_dphy_t *dphy = container_of(agent, struct ccic_dphy_t,
						agent.hw);
	struct mvisp_device *isp = agent->mngr->drv_priv;
#if 0	/* For dynamic resource allocation, don't check it for now */
	WARN_ON((agent->clock == NULL) && (agent->ops->set_clock == NULL));
	WARN_ON(agent->reg_base == NULL);
#endif
	dphy->isp = isp;

	printk(KERN_INFO "ccic-dphy: map_hw init done\n");
	return 0;
}

static int ccic_dphy_hw_open(struct hw_agent *agent)
{
	struct hw_event *event = hw_local_event_find(agent, CCIC_IRQ_NAME);
	return hw_event_subscribe(event, agent, &ccic_phy_isr);
}

static void ccic_dphy_hw_close(struct hw_agent *agent)
{
	struct hw_event *event = hw_local_event_find(agent, CCIC_IRQ_NAME);
	hw_event_unsubscribe(event, agent);
}

static int ccic_dphy_hw_power(struct hw_agent *agent, int level)
{
	/* This is a W/R for dynamic resource allocation */
	agent->reg_base = (void __iomem *)CCIC1_VIRT_BASE;
	return 0;
}

static int ccic_dphy_hw_qos(struct hw_agent *agent, int qos_level)
{
	/* power off */
	/* platform: change function clock rate */
	/* power on */
	return 0;
}

struct hw_agent_ops ccic_dphy_hw_ops = {
	.init		= ccic_dphy_hw_init,
	.clean		= ccic_dphy_hw_clean,
	.set_power	= ccic_dphy_hw_power,
	.open		= ccic_dphy_hw_open,
	.close		= ccic_dphy_hw_close,
	.set_qos	= ccic_dphy_hw_qos,
};


static int dphy_link_setup(struct media_entity *entity,
			   const struct media_pad *local,
			   const struct media_pad *remote, u32 flags)
{
	printk(KERN_DEBUG "ccic-dphy link setup not fulfilled\n");
	return 0;
}

static const struct media_entity_operations dphy_media_ops = {
	.link_setup = dphy_link_setup,
};

static int ccic_dphy_map_add(struct map_agent *agent)
{
	struct ccic_dphy_t *dphy = agent->drv_priv;
	struct v4l2_subdev *sd = &agent->subdev;

	/* Prepare struct dphy */
	spin_lock_init(&dphy->mipi_flag_lock);

	/* Prepare agent */
	v4l2_subdev_init(sd, &ccic_dphy_sd_ops);
	sd->internal_ops = &ccic_dphy_internal_ops;
	sd->grp_id = GID_AGENT_GROUP;
	v4l2_set_subdevdata(sd, agent);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	agent->pads[DPHY_PADI].flags	= MEDIA_PAD_FL_SINK;
	agent->pads[DPHY_PADO_CCIC].flags	= MEDIA_PAD_FL_SOURCE;
	agent->pads[DPHY_PADO_DXO].flags	= MEDIA_PAD_FL_SOURCE;
	agent->pad_vdev = -1;
	agent->pads_cnt = DPHY_PAD_END;

	sd->entity.ops = &dphy_media_ops;
	return 0;
}

static void ccic_dphy_map_remove(struct map_agent *agent)
{
}

static int ccic_dphy_map_open(struct map_agent *agent)
{
	return 0;
}

static void ccic_dphy_map_close(struct map_agent *agent)
{
}

static int ccic_dphy_map_start(struct map_agent *agent)
{
	return 0;
}

static void ccic_dphy_map_stop(struct map_agent *agent)
{
}

struct map_agent_ops ccic_dphy_map_ops = {
	.add		= ccic_dphy_map_add,
	.remove		= ccic_dphy_map_remove,
	.open		= ccic_dphy_map_open,
	.close		= ccic_dphy_map_close,
	.start		= ccic_dphy_map_start,
	.stop		= ccic_dphy_map_stop,
};

/******************************** CCIC IP Core ********************************/

static char *clock_name[] = {
	[CCIC_CLOCK_FUN]	= "CCICFUNCLK",
	[CCIC_CLOCK_PHY]	= "CCICPHYCLK",
	[CCIC_CLOCK_AXI]	= "CCICAXICLK",
	[CCIC_CLOCK_AHB]	= "LCDCIHCLK",
};

static struct hw_res_req ccic_core_res[] = {
	/* {MAP_RES_MEM,	0}, */
	{MAP_RES_IRQ},
	{MAP_RES_CLK,	CCIC_CLOCK_FUN},
	{MAP_RES_CLK,	CCIC_CLOCK_AHB},
	{MAP_RES_CLK,   CCIC_CLOCK_AXI},
	{MAP_RES_END}
};

static struct hw_res_req ccic_dphy_res[] = {
	/* {MAP_RES_MEM,	0x100}, */
	{MAP_RES_CLK,	CCIC_CLOCK_FUN},
	{MAP_RES_CLK,	CCIC_CLOCK_PHY},
	{MAP_RES_END}
};

static int mc_ccic_suspend(struct device *dev)
{
	return 0;
}

static int mc_ccic_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops mc_ccic_pm = {
	.suspend	= mc_ccic_suspend,
	.resume		= mc_ccic_resume,
};

static int mc_ccic_probe(struct platform_device *pdev)
{
	struct mvisp_platform_data *pdata = pdev->dev.platform_data;
	struct ccic_device *mc_ccic;
	struct ccic_dphy_t *ccic_dphy;
	struct resource *res, clk;
	union agent_id pdev_mask = {
		.dev_type = PCAM_IP_CCIC,
		.dev_id = pdev->dev.id,
		.mod_type = 0xFF,
		.mod_id = 0xFF,
	};
	struct hw_agent *hw;
	struct map_agent *agent;
	int i, j, ret;

	if (pdata == NULL)
		return -EINVAL;

	mc_ccic = devm_kzalloc(&pdev->dev,
				sizeof(struct ccic_device), GFP_KERNEL);
	if (!mc_ccic) {
		dev_err(&pdev->dev, "could not allocate memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, mc_ccic);
	mc_ccic->dev = &pdev->dev;

	ccic_dphy = devm_kzalloc(&pdev->dev, sizeof(struct ccic_dphy_t),
				GFP_KERNEL);
	if (!ccic_dphy) {
		dev_err(&pdev->dev, "could not allocate ccic_dphy struct\n");
		return -ENOMEM;
	}
	ccic_dphy->dev = &pdev->dev;
#if 0
	/* request the mem region for the camera registers */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		dev_err(&pdev->dev, "mem resource not found");
	ret = plat_resrc_register(&pdev->dev, res, res->name, pdev_mask, 0,
					NULL, NULL));
	if (ret < 0) {
		dev_err(&pdev->dev, "failed register mem resource %s",
			res->name);
		return ret;
	}
#else
	/* This is a W/R to avoid iomem conflict with smart sensor driver */
	mc_ccic->agent.hw.reg_base = (void __iomem *)CCIC1_VIRT_BASE;
	ccic_dphy->agent.hw.reg_base = (void __iomem *)CCIC1_VIRT_BASE;
#endif

	/* get irqs */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource");
		return -ENXIO;
	}
	mc_ccic->event_irq.name = CCIC_IRQ_NAME;
	mc_ccic->event_irq.owner_name = CCIC_CORE_NAME;
	memset(&mc_ccic->event_irq.dispatch_list, 0, \
		sizeof(mc_ccic->event_irq.dispatch_list));
	/* We expect DxO wrapper only take one kind of irq: the DMA irq.
	 * so the handler is the same for all, but resource id is device id */
	ret = plat_resrc_register(&pdev->dev, res, CCIC_IRQ_NAME, pdev_mask, 0,
				/* irq handler */
				&ccic_irq_handler,
				/* irq context*/
				&mc_ccic->event_irq);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed register irq resource %s",
			res->name);
		return ret;
	}

	memset(&clk, 0, sizeof(struct resource));
	clk.flags = MAP_RES_CLK;
	/* get clocks */
	for (i = 0, j = 0; j < CCIC_CLOCK_END; i++) {
		clk.name = clock_name[j];
		ret = plat_resrc_register(&pdev->dev, &clk, NULL, pdev_mask,
						j, NULL, NULL);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed register clock resource %s",
				res->name);
			return ret;
		}
		j++;
	}

	agent = &mc_ccic->agent;
	hw = &agent->hw;
	hw->id.dev_type = PCAM_IP_CCIC;
	hw->id.dev_id = pdev->dev.id;
	hw->id.mod_type = MAP_AGENT_DMA_OUT;
	hw->id.mod_id = AGENT_CCIC_CORE;
	hw->name	= CCIC_CORE_NAME;
	hw->req_list	= ccic_core_res;
	hw->ops		= &ccic_core_hw_ops;
	agent->ops	= &ccic_core_map_ops;
	agent->drv_priv	= mc_ccic;
	hw->plat_priv	= &mc_ccic->pdata;
	plat_agent_register(&mc_ccic->agent.hw);

	agent = &ccic_dphy->agent;
	hw = &agent->hw;
	hw->id.dev_type = PCAM_IP_CCIC;
	hw->id.dev_id = pdev->dev.id;
	hw->id.mod_type = MAP_AGENT_NORMAL;
	hw->id.mod_id = AGENT_CCIC_DPHY;
	hw->name	= CCIC_DPHY_NAME;
	hw->req_list	= ccic_dphy_res;
	hw->ops		= &ccic_dphy_hw_ops;
	agent->ops	= &ccic_dphy_map_ops;
	agent->drv_priv	= ccic_dphy;
	plat_agent_register(&ccic_dphy->agent.hw);

	return 0;
}

static int mc_ccic_remove(struct platform_device *pdev)
{
	struct ccic_device *mc_ccic = platform_get_drvdata(pdev);

	if (mc_ccic->agent.hw.reg_base != NULL)
		devm_iounmap(mc_ccic->dev, mc_ccic->agent.hw.reg_base);
	if (mc_ccic->agent.hw.irq_num)
		devm_free_irq(mc_ccic->dev, mc_ccic->agent.hw.irq_num, mc_ccic);
	devm_kfree(mc_ccic->dev, mc_ccic);
	return 0;
}

struct platform_driver mc_ccic_driver = {
	.driver = {
		.name	= MC_CCIC_DRV_NAME,
		.pm	= &mc_ccic_pm,
	},
	.probe	= mc_ccic_probe,
	.remove	= __devexit_p(mc_ccic_remove),
};

module_platform_driver(mc_ccic_driver);
