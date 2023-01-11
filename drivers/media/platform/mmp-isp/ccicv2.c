/*
 * ccicv2.c
 *
 * Marvell B52 ISP
 *
 * Copyright:  (C) Copyright 2013 Marvell International Ltd.
 *              Jiaquan Su <jqsu@marvell.com>
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
 */

#include <linux/device.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <media/v4l2-ctrls.h>

#include <media/b52-sensor.h>
#include <media/mrvl-camera.h>

#include "plat_cam.h"
#include "ccicv2.h"

#define CCIC_DRV_NAME		"ccicv2-drv"
#define CCIC_CSI_NAME		"ccic-csi"
#define CCIC_DMA_NAME		"ccic-dma"
#define CCIC_IRQ_NAME		"ccic-dma-irq"

static int trace = 2;
module_param(trace, int, 0644);
MODULE_PARM_DESC(trace,
		"how many trace do you want to see? (0-4)"
		"0 - mute"
		"1 - only actual errors"
		"2 - milestone log"
		"3 - briefing log"
		"4 - detailed log");


/*************************** Hardware Abstract Code ***************************/

static irqreturn_t csi_irq_handler(struct ccic_ctrl_dev *ccic_ctrl, u32 irq)
{
	return IRQ_HANDLED;
}

static inline int ccic_dma_fill_buf(struct ccic_dma *ccic_dma,
				struct isp_videobuf *ispvb)
{
	int i;
	int ret = 0;
	struct ccic_dma_dev *dma_dev = ccic_dma->ccic_dma;
	struct plat_cam *pcam = ccic_dma->priv;
	struct plat_vnode *pvnode = NULL;

	if (ispvb == NULL) {
		d_inf(4, "%s: buffer is NULL", __func__);
		return -EINVAL;
	}

	pvnode = container_of(ispvb->vb.vb2_queue, struct plat_vnode, vnode.vq);
	if (pvnode->fill_mmu_chnl) {
		ret = pvnode->fill_mmu_chnl(pcam, &ispvb->vb, ccic_dma->nr_chnl);
		if (ret < 0) {
			d_inf(1, "ccic failed to fill mmu addr");
			return ret;
		}
	}

	for (i = 0; i < ccic_dma->nr_chnl; i++)
		ccic_dma->ccic_dma->ops->set_addr(ccic_dma->ccic_dma, (u8)i,
			(u32)ispvb->ch_info[i].daddr);

	dma_dev->ops->shadow_ready(dma_dev);

	return ret;
}

#define	  IRQ_EOF0		0x00000001	/* End of frame 0 */
#define	  IRQ_SOF0		0x00000008	/* Start of frame 0 */
#define	  IRQ_DMA_NOT_DONE	0x00000010
#define	  IRQ_SHADOW_NOT_RDY	0x00000020
#define	  IRQ_OVERFLOW		0x00000040	/* FIFO overflow */
#define	  FLAG_FRAME_SOF0 1

static irqreturn_t dma_irq_handler(struct ccic_dma_dev *dma_dev, u32 irqs)
{
	struct ccic_dma *ccic_dma = dma_dev->priv;
	struct isp_vnode *vnode = ccic_dma->vnode;
	struct isp_videobuf *ispvb = NULL;

	if (irqs & IRQ_OVERFLOW) {
		ispvb = isp_vnode_find_busy_buffer(vnode, 0);
		ccic_dma_fill_buf(ccic_dma, ispvb);

		clear_bit(FLAG_FRAME_SOF0, &ccic_dma->flags);
		d_inf(3, "CCIC DMA FIFO overflow");
	}

	if (irqs & IRQ_SOF0) {
		if (test_bit(FLAG_FRAME_SOF0, &ccic_dma->flags))
			d_inf(2, "miss EOF\n");

		ispvb = isp_vnode_find_busy_buffer(vnode, 1);
		if (!ispvb) {
			ispvb = isp_vnode_get_idle_buffer(vnode);
			isp_vnode_put_busy_buffer(vnode, ispvb);
		}

		if (ispvb) {
			ccic_dma_fill_buf(ccic_dma, ispvb);
			set_bit(FLAG_FRAME_SOF0, &ccic_dma->flags);
		} else
			d_inf(3, "no buffer in idle queue, expect frame drop");
	}

	if ((irqs & IRQ_EOF0) &&
		test_bit(FLAG_FRAME_SOF0, &ccic_dma->flags)) {
		ispvb = isp_vnode_get_busy_buffer(vnode);
		if (!ispvb)
			d_inf(3, "busy buffer is NULL in EOF?? strange");

		if (isp_vnode_export_buffer(ispvb) < 0)
			d_inf(1, "%s: export buffer failed", vnode->vdev.name);

		clear_bit(FLAG_FRAME_SOF0, &ccic_dma->flags);
	}

	if (irqs & IRQ_SHADOW_NOT_RDY) {
		ispvb = isp_vnode_get_idle_buffer(vnode);
		if (!ispvb)
			d_inf(3, "no buffer in idle queue, expect frame drop");
		else {
			ccic_dma_fill_buf(ccic_dma, ispvb);
			isp_vnode_put_busy_buffer(vnode, ispvb);
		}

		clear_bit(FLAG_FRAME_SOF0, &ccic_dma->flags);
		d_inf(3, "CCIC DMA NOT RDY");
	}

	return IRQ_HANDLED;
}

/********************************* CCICv2 CSI *********************************/
static int ccic_csi_hw_open(struct isp_block *block)
{
	struct ccic_csi *csi = container_of(block, struct ccic_csi, block);
	struct msc2_ccic_dev *ccic_dev_host = NULL;
	struct ccic_ctrl_dev *ctrl_dev;

	/*
	 * here support csi mux repack funcion.
	 * enable csi2-> ccic1 IDI path.
	 * */
	if (csi->csi_mux_repacked) {
		if (csi->dev->id != 1)
			d_inf(1, "ccic csi device not match\n");

		msc2_get_ccic_dev(&ccic_dev_host, 0);
		ctrl_dev = ccic_dev_host->ctrl_dev;
		ctrl_dev->ops->clk_enable(ctrl_dev);
	}
	ctrl_dev = csi->ccic_ctrl;
	ctrl_dev->ops->clk_enable(ctrl_dev);
#if 0
/*
 * FIXME: ISP and SC2 use separate power, need share it
 */
	csi->ccic_ctrl->ops->power_up(csi->ccic_ctrl);
#endif
	return 0;
}

static void ccic_csi_hw_close(struct isp_block *block)
{
	struct ccic_csi *csi = container_of(block, struct ccic_csi, block);
	struct msc2_ccic_dev *ccic_dev_host = NULL;
	struct ccic_ctrl_dev *ctrl_dev;

	if (csi->csi_mux_repacked) {
		if (csi->dev->id != 1)
			d_inf(1, "ccic csi device not match\n");

		msc2_get_ccic_dev(&ccic_dev_host, 0);
		ctrl_dev = ccic_dev_host->ctrl_dev;
		ctrl_dev->ops->clk_disable(ctrl_dev);
	}
	ctrl_dev = csi->ccic_ctrl;
	ctrl_dev->ops->clk_disable(ctrl_dev);
#if 0
/*
 * FIXME: ISP and SC2 use separate power, need share it
 */
	csi->ccic_ctrl->ops->power_down(csi->ccic_ctrl);
#endif
}

static int ccic_csi_hw_s_power(struct isp_block *block, int level)
{
	if (level)
		return pm_runtime_get_sync(block->dev);
	else
		return pm_runtime_put(block->dev);
}

struct isp_block_ops ccic_csi_hw_ops = {
	.open	= ccic_csi_hw_open,
	.close	= ccic_csi_hw_close,
	.set_power = ccic_csi_hw_s_power,
};

static int ccic_csi_s_ctrl(struct v4l2_ctrl *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops ccic_csi_ctrl_ops = {
	.s_ctrl = ccic_csi_s_ctrl,
};

static int ccic_csi_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct ccic_csi *ccic_csi = isd->drv_priv;
	struct msc2_ccic_dev *ccic_dev_host = NULL;
	struct ccic_ctrl_dev *ctrl_dev;
	int ret = 0;

	if (!enable)
		goto stream_off;

	if (atomic_inc_return(&ccic_csi->stream_cnt) > 1)
		return 0;

	d_inf(3, "csi stream on");
	ret = b52_sensor_call(ccic_csi->sensor, g_csi,
				&ccic_csi->ccic_ctrl->csi);
	if (ret < 0)
		return ret;

	/*
	 * here support csi mux repack funcion.
	 * enable csi2-> ccic1 IDI path.
	 * */
	if (ccic_csi->csi_mux_repacked) {
		if (ccic_csi->dev->id != 1)
			d_inf(1, "ccic csi device not match\n");

		msc2_get_ccic_dev(&ccic_dev_host, 0);
		ctrl_dev = ccic_dev_host->ctrl_dev;
		ctrl_dev->ops->config_idi(ctrl_dev, SC2_IDI_SEL_REPACK);
		ctrl_dev->ops->config_idi(ctrl_dev, SC2_IDI_SEL_REPACK_OTHER);
	}

	ctrl_dev = ccic_csi->ccic_ctrl;
	ctrl_dev->ops->config_idi(ctrl_dev, SC2_IDI_SEL_REPACK);
	ctrl_dev->ops->config_mbus(ctrl_dev, V4L2_MBUS_CSI2, 0, 1);
	return 0;

stream_off:
	if (atomic_dec_return(&ccic_csi->stream_cnt) > 0)
		return 0;

	d_inf(3, "csi stream off");
	if (ccic_csi->csi_mux_repacked) {
		if (ccic_csi->dev->id != 1)
			d_inf(1, "ccic csi device not match\n");

		msc2_get_ccic_dev(&ccic_dev_host, 0);
		ctrl_dev = ccic_dev_host->ctrl_dev;
		ctrl_dev->ops->config_idi(ctrl_dev, SC2_IDI_SEL_NONE);
	}

	ctrl_dev = ccic_csi->ccic_ctrl;
	ctrl_dev->ops->config_idi(ctrl_dev, SC2_IDI_SEL_NONE);
	ctrl_dev->ops->config_mbus(ctrl_dev, V4L2_MBUS_CSI2, 0, 0);
	return 0;
}

static const struct v4l2_subdev_video_ops ccic_csi_video_ops = {
	.s_stream	= ccic_csi_set_stream,
};

static int ccic_csi_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	int ret = 0;
	return ret;
}

static int ccic_csi_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	int ret = 0;
	return ret;
}

static int ccic_csi_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	struct isp_subdev *ispsd = v4l2_get_subdev_hostdata(sd);
	struct ccic_csi *csi = ispsd->drv_priv;
	struct v4l2_subdev *sensorsd = &csi->sensor->sd;
	ret = v4l2_subdev_call(sensorsd, pad, get_fmt, NULL, fmt);
	if (ret < 0) {
		pr_err("camera: set_fmt failed %d\n", __LINE__);
		return ret;
	}
	return ret;
}

static int ccic_csi_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	return ret;
}

static int ccic_csi_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	return 0;
}

static int ccic_csi_set_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	return 0;
}

static const struct v4l2_subdev_pad_ops ccic_csi_pad_ops = {
	.enum_mbus_code		= ccic_csi_enum_mbus_code,
	.enum_frame_size	= ccic_csi_enum_frame_size,
	.get_fmt		= ccic_csi_get_format,
	.set_fmt		= ccic_csi_set_format,
	.get_selection		= ccic_csi_get_selection,
	.set_selection		= ccic_csi_set_selection,
};

static const struct v4l2_subdev_ops ccic_csi_subdev_ops = {
	.video	= &ccic_csi_video_ops,
	.pad	= &ccic_csi_pad_ops,
};

static int ccic_csi_node_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	return 0;
};

static int ccic_csi_node_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	return 0;
};

static const struct v4l2_subdev_internal_ops ccic_csi_node_ops = {
	.open	= ccic_csi_node_open,
	.close	= ccic_csi_node_close,
};

static int ccic_csi_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct isp_subdev *ispsd = me_to_ispsd(entity);
	struct ccic_csi *csi = ispsd->drv_priv;
	struct v4l2_subdev *sd;
	switch (local->index) {
	case CCIC_CSI_PAD_IN:
		if (flags & MEDIA_LNK_FL_ENABLED) {
			sd = container_of(remote->entity,
					struct v4l2_subdev, entity);
			csi->sensor = to_b52_sensor(sd);
		} else
			csi->sensor = NULL;
		break;
	case CCIC_CSI_PAD_XFEED:
		d_inf(4, "TODO: add code to connect to other ccic dma output");

		break;
	case CCIC_CSI_PAD_LOCAL:
		d_inf(4, "TODO: add code to connect to local ccic dma output");

		break;
	case CCIC_CSI_PAD_ISP:
		d_inf(4, "TODO: add code to connect to B52 isp output");
		break;
	default:
		break;
	}
	return 0;
}

static const struct media_entity_operations ccic_csi_media_ops = {
	.link_setup = ccic_csi_link_setup,
};

static int ccic_csi_sd_open(struct isp_subdev *ispsd)
{
	return 0;
}

static void ccic_csi_sd_close(struct isp_subdev *ispsd)
{
}

struct isp_subdev_ops ccic_csi_sd_ops = {
	.open		= ccic_csi_sd_open,
	.close		= ccic_csi_sd_close,
};

static void ccic_csi_remove(struct ccic_csi *ccic_csi)
{
	pm_runtime_disable(ccic_csi->dev);
	msc2_put_ccic_ctrl(&ccic_csi->ccic_ctrl);
	v4l2_ctrl_handler_free(&ccic_csi->ispsd.ctrl_handler);
	devm_kfree(ccic_csi->dev, ccic_csi);
	ccic_csi = NULL;
}

static int ccic_csi_create(struct ccic_csi *ccic_csi)
{
	int ret = 0;
	struct isp_block *block = &ccic_csi->block;
	struct isp_dev_ptr *desc = &ccic_csi->desc;
	struct isp_subdev *ispsd = &ccic_csi->ispsd;
	struct v4l2_subdev *sd = &ispsd->subdev;

	ret = msc2_get_ccic_ctrl(&ccic_csi->ccic_ctrl,
			ccic_csi->dev->id, csi_irq_handler);
	if (ret < 0) {
		dev_err(ccic_csi->dev, "failed to get ccic_ctrl\n");
		return ret;
	}

	/* H/W block setup */
	block->id.dev_type = PCAM_IP_CCICV2;
	block->id.dev_id = ccic_csi->dev->id;
	block->id.mod_id = CCIC_BLK_CSI;
	block->dev = ccic_csi->dev;
	snprintf(ccic_csi->name, sizeof(ccic_csi->name),
		"ccic-csi #%d", block->id.dev_id);
	block->name = ccic_csi->name;
	block->ops = &ccic_csi_hw_ops;
	INIT_LIST_HEAD(&desc->hook);
	desc->ptr = block;
	desc->type = ISP_GDEV_BLOCK;

	/* isp-subdev setup */
	sd->entity.ops = &ccic_csi_media_ops;
	v4l2_subdev_init(sd, &ccic_csi_subdev_ops);
	sd->internal_ops = &ccic_csi_node_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ret = v4l2_ctrl_handler_init(&ispsd->ctrl_handler, 16);
	if (unlikely(ret < 0))
		return ret;
	sd->ctrl_handler = &ispsd->ctrl_handler;
	ispsd->ops = &ccic_csi_sd_ops;
	ispsd->drv_priv = ccic_csi;

	ispsd->pads[CCIC_CSI_PAD_IN].flags = MEDIA_PAD_FL_SINK;
	ispsd->pads[CCIC_CSI_PAD_LOCAL].flags = MEDIA_PAD_FL_SOURCE;
	ispsd->pads[CCIC_CSI_PAD_XFEED].flags = MEDIA_PAD_FL_SOURCE;
	ispsd->pads[CCIC_CSI_PAD_ISP].flags = MEDIA_PAD_FL_SOURCE;
	ispsd->pads_cnt = CCIC_CSI_PAD_CNT;
	ispsd->single = 1;
	INIT_LIST_HEAD(&ispsd->gdev_list);
	/* Single subdev */
	list_add_tail(&desc->hook, &ispsd->gdev_list);
	ispsd->sd_code = SDCODE_CCICV2_CSI0 + block->id.dev_id;

	ret = plat_ispsd_register(ispsd);
	if (ret < 0)
		goto exit_err;

	pm_runtime_enable(ccic_csi->dev);
	return 0;

exit_err:
	ccic_csi_remove(ccic_csi);
	return ret;
}



/********************************* CCICv2 DMA *********************************/
static int ccic_dma_hw_open(struct isp_block *block)
{
	return 0;
}

static void ccic_dma_hw_close(struct isp_block *block)
{
}

struct isp_block_ops ccic_dma_hw_ops = {
	.open	= ccic_dma_hw_open,
	.close	= ccic_dma_hw_close,
};

static int ccic_dma_s_ctrl(struct v4l2_ctrl *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		break;
	case V4L2_CID_CONTRAST:
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops ccic_dma_ctrl_ops = {
	.s_ctrl = ccic_dma_s_ctrl,
};

static inline void ccic_dma_enable_irq(struct v4l2_subdev *sd, int enable)
{
	struct media_pad *pad;
	struct isp_subdev *isd;
	struct ccic_csi *ccic_csi;

	pad = media_entity_remote_pad(&sd->entity.pads[CCIC_DMA_PAD_IN]);
	if (!pad) {
		d_inf(1, "%s: unable to get remote pad", __func__);
		return;
	}
	isd = v4l2_get_subdev_hostdata(
				media_entity_to_v4l2_subdev(pad->entity));
	ccic_csi = isd->drv_priv;

	if (enable)
		ccic_csi->ccic_ctrl->ops->irq_mask(ccic_csi->ccic_ctrl, 1);
	else
		ccic_csi->ccic_ctrl->ops->irq_mask(ccic_csi->ccic_ctrl, 0);
}

static int ccic_dma_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct ccic_dma *ccic_dma = isd->drv_priv;
	struct ccic_dma_dev *dma_dev = ccic_dma->ccic_dma;
	struct media_pad *vpad = media_entity_remote_pad(
					isd->pads + CCIC_DMA_PAD_OUT);
	struct isp_vnode *vnode = me_to_vnode(vpad->entity);
	struct plat_vnode *pvnode = container_of(vnode, struct plat_vnode, vnode);
	struct isp_build *build = container_of(isd->subdev.entity.parent,
						 struct isp_build, media_dev);
	struct plat_cam *pcam = build->plat_priv;
	struct isp_videobuf *ispvb = NULL;
	int ret = 0;

	if (!enable)
		goto stream_off;

	if (atomic_inc_return(&ccic_dma->stream_cnt) > 1)
		return 0;

	switch (vnode->format.fmt.pix_mp.pixelformat) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		ccic_dma->nr_chnl = 3;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		ccic_dma->nr_chnl = 2;
		break;
	default:
		ccic_dma->nr_chnl = 1;
	}

	dma_dev->pixfmt		= vnode->format.fmt.pix_mp.pixelformat;
	dma_dev->width		= isd->fmt_pad[CCIC_DMA_PAD_IN].width;
	dma_dev->height		= isd->fmt_pad[CCIC_DMA_PAD_IN].height;
	/* bytesperline is only used in jpeg mode, using plane[0]'s value */
	dma_dev->bytesperline	= vnode->format.fmt.pix_mp.
					plane_fmt[0].bytesperline;
	dma_dev->code		= isd->fmt_pad[CCIC_DMA_PAD_IN].code;
	dma_dev->priv		= ccic_dma;
	ccic_dma->priv		= pcam;

	if (pvnode->alloc_mmu_chnl) {
		ret = pvnode->alloc_mmu_chnl(pcam, PCAM_IP_CCICV2,
				ccic_dma->dev->id, 0, ccic_dma->nr_chnl,
				&pvnode->mmu_ch_dsc);
		if (unlikely(ret < 0))
			return ret;
	}

	ret = dma_dev->ops->setup_image(dma_dev);
	if (ret < 0)
		goto free_mmu;

	ispvb = isp_vnode_get_idle_buffer(vnode);
	BUG_ON(ispvb == NULL);
	ret = ccic_dma_fill_buf(ccic_dma, ispvb);
	if (ret < 0)
		goto free_mmu;
	ret = isp_vnode_put_busy_buffer(vnode, ispvb);
	if (ret < 0)
		goto free_mmu;
	ccic_dma->vnode = vnode;

	dma_dev->ops->ccic_enable(dma_dev);
	ccic_dma_enable_irq(sd, 1);
	return 0;

stream_off:
	if (atomic_dec_return(&ccic_dma->stream_cnt) > 0)
		return 0;
	ccic_dma_enable_irq(sd, 0);
	dma_dev->ops->ccic_disable(dma_dev);
free_mmu:
	if (pvnode->free_mmu_chnl)
		pvnode->free_mmu_chnl(pcam, &pvnode->mmu_ch_dsc);

	return 0;
}

static const struct v4l2_subdev_video_ops ccic_dma_video_ops = {
	.s_stream	= ccic_dma_set_stream,
};

static int ccic_dma_enum_mbus_code(struct v4l2_subdev *sd,
				  struct v4l2_subdev_fh *fh,
				  struct v4l2_subdev_mbus_code_enum *code)
{
	int ret = 0;
	return ret;
}

static int ccic_dma_enum_frame_size(struct v4l2_subdev *sd,
				   struct v4l2_subdev_fh *fh,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	int ret = 0;
	return ret;
}

static int ccic_dma_get_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *fmt)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	int ret = 0;

	/* Get format is allowed only on the input pad */
	if (fmt->pad != CCIC_DMA_PAD_IN)
		return -EINVAL;
	fmt->format = isd->fmt_pad[CCIC_DMA_PAD_IN];
	return ret;
}

static int ccic_dma_set_format(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_format *sd_fmt)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	int ret = 0;

	/* set input mbus format as while */
	isd->fmt_pad[CCIC_DMA_PAD_IN] = sd_fmt->format;
	return ret;
}

static int ccic_dma_get_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_rect *rect;

	if (sel->pad >= isd->pads_cnt)
		return -EINVAL;
	rect = isd->crop_pad + sel->pad;
	sel->r = *rect;
	return 0;
}

static int ccic_dma_set_selection(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh,
				struct v4l2_subdev_selection *sel)
{
	struct isp_subdev *isd = v4l2_get_subdev_hostdata(sd);
	struct v4l2_rect *rect;
	int ret = 0;

	if (sel->pad >= isd->pads_cnt)
		return -EINVAL;
	rect = isd->crop_pad + sel->pad;

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		goto exit;
	/* Really apply here if not using MCU */
	d_inf(3, "%s:pad[%d] crop(%d, %d)<>(%d, %d)", sd->name, sel->pad,
		sel->r.left, sel->r.top, sel->r.width, sel->r.height);
	*rect = sel->r;
exit:
	return ret;
}

static const struct v4l2_subdev_pad_ops ccic_dma_pad_ops = {
	.enum_mbus_code		= ccic_dma_enum_mbus_code,
	.enum_frame_size	= ccic_dma_enum_frame_size,
	.get_fmt		= ccic_dma_get_format,
	.set_fmt		= ccic_dma_set_format,
	.get_selection		= ccic_dma_get_selection,
	.set_selection		= ccic_dma_set_selection,
};

static const struct v4l2_subdev_ops ccic_dma_subdev_ops = {
	.video	= &ccic_dma_video_ops,
	.pad	= &ccic_dma_pad_ops,
};

static int ccic_dma_node_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	return 0;
};

static int ccic_dma_node_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	return 0;
};

static const struct v4l2_subdev_internal_ops ccic_dma_node_ops = {
	.open	= ccic_dma_node_open,
	.close	= ccic_dma_node_close,
};

static int ccic_dma_link_setup(struct media_entity *entity,
			      const struct media_pad *local,
			      const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	switch (local->index) {
	case CCIC_DMA_PAD_IN:
		if (flags & MEDIA_LNK_FL_ENABLED)
			d_inf(2, "%s: TODO: add code to connect ccic-csi",
				sd->name);
		else
			d_inf(2, "%s: TODO: add code to disconnect ccic-csi",
				sd->name);
		break;
	case CCIC_DMA_PAD_OUT:
		if (flags & MEDIA_LNK_FL_ENABLED)
			d_inf(2, "%s: TODO: add code to connect video port",
				sd->name);
		else
			d_inf(2, "%s: TODO: add code to disconnect video port",
				sd->name);
		break;
	default:
		break;
	}
	return 0;
}

static const struct media_entity_operations ccic_dma_media_ops = {
	.link_setup = ccic_dma_link_setup,
};

static int ccic_dma_sd_open(struct isp_subdev *ispsd)
{
	return 0;
}

static void ccic_dma_sd_close(struct isp_subdev *ispsd)
{
}

struct isp_subdev_ops ccic_dma_sd_ops = {
	.open		= ccic_dma_sd_open,
	.close		= ccic_dma_sd_close,
};

static void ccic_dma_remove(struct ccic_dma *ccic_dma)
{
	pm_runtime_disable(ccic_dma->dev);
	msc2_put_ccic_dma(&ccic_dma->ccic_dma);
	v4l2_ctrl_handler_free(&ccic_dma->ispsd.ctrl_handler);
	devm_kfree(ccic_dma->dev, ccic_dma);
	ccic_dma = NULL;
}

static int ccic_dma_create(struct ccic_dma *ccic_dma)
{
	int ret = 0;
	struct isp_block *block = &ccic_dma->block;
	struct isp_dev_ptr *desc = &ccic_dma->desc;
	struct isp_subdev *ispsd = &ccic_dma->ispsd;
	struct v4l2_subdev *sd = &ispsd->subdev;

	ret = msc2_get_ccic_dma(&ccic_dma->ccic_dma,
			ccic_dma->dev->id, &dma_irq_handler);
	if (ret < 0) {
		dev_err(ccic_dma->dev, "failed to get ccic_dma\n");
		return ret;
	}

	/* H/W block setup */
	block->id.dev_type = PCAM_IP_CCICV2;
	block->id.dev_id = ccic_dma->dev->id;
	block->id.mod_id = CCIC_BLK_DMA;
	block->dev = ccic_dma->dev;
	snprintf(ccic_dma->name, sizeof(ccic_dma->name),
		"ccic-dma #%d", ccic_dma->dev->id);
	block->name = ccic_dma->name;
	block->ops = &ccic_dma_hw_ops;
	INIT_LIST_HEAD(&desc->hook);
	desc->ptr = block;
	desc->type = ISP_GDEV_BLOCK;

	/* isp-subdev setup */
	sd->entity.ops = &ccic_dma_media_ops;
	v4l2_subdev_init(sd, &ccic_dma_subdev_ops);
	sd->internal_ops = &ccic_dma_node_ops;
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	ret = v4l2_ctrl_handler_init(&ispsd->ctrl_handler, 16);
	if (unlikely(ret < 0))
		return ret;
	sd->ctrl_handler = &ispsd->ctrl_handler;
	ispsd->ops = &ccic_dma_sd_ops;
	ispsd->drv_priv = ccic_dma;

	ispsd->pads[CCIC_DMA_PAD_IN].flags = MEDIA_PAD_FL_SINK; /* CSI input */
	ispsd->pads[CCIC_DMA_PAD_OUT].flags = MEDIA_PAD_FL_SOURCE;
	ispsd->pads_cnt = CCIC_DMA_PAD_CNT;
	ispsd->single = 1;
	INIT_LIST_HEAD(&ispsd->gdev_list);
	/* Single subdev */
	list_add_tail(&desc->hook, &ispsd->gdev_list);
	ispsd->sd_code = SDCODE_CCICV2_DMA0 + block->id.dev_id;
	ispsd->sd_type = ISD_TYPE_DMA_OUT;

	ret = plat_ispsd_register(ispsd);
	if (ret < 0)
		goto exit_err;

	pm_runtime_enable(ccic_dma->dev);
	return 0;

exit_err:
	ccic_dma_remove(ccic_dma);
	return ret;
}

/***************************** The CCICV2 IP Core *****************************/



static const struct of_device_id ccicv2_dt_match[] = {
	{
		.compatible = "marvell,ccicv2",
	},
	{},
};
MODULE_DEVICE_TABLE(of, ccicv2_dt_match);

static int ccicv2_probe(struct platform_device *pdev)
{
	struct ccic_csi *ccic_csi;
	struct ccic_dma *ccic_dma;
	struct device_node *np = pdev->dev.of_node;
	u32 module_type, i;
	int ret;

	ret = of_property_read_u32(np, "cciv2_type", &module_type);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get type %d\n", module_type);
		return -ENODEV;
	}

	if (module_type == CCIC_BLK_CSI) {
		ccic_csi = devm_kzalloc(&pdev->dev,
				sizeof(struct ccic_csi), GFP_KERNEL);
		if (unlikely(ccic_csi == NULL)) {
			dev_err(&pdev->dev, "could not allocate memory\n");
			return -ENOMEM;
		}
		ret = of_property_read_u32(np, "csi_id", &i);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to get csi_id\n");
			return -ENODEV;
		}
		pdev->id = pdev->dev.id = i;
		platform_set_drvdata(pdev, ccic_csi);
		ccic_csi->dev = &(pdev->dev);

		ccic_csi->csi_mux_repacked = 0;
		if (of_get_property(np, "csi_mux_repacked", NULL))
			ccic_csi->csi_mux_repacked = 1;

		ret = ccic_csi_create(ccic_csi);
		if (unlikely(ret < 0)) {
			dev_err(&pdev->dev, "failed to build CCICv2 sci-subdev\n");
			return ret;
		}
	} else if (module_type == CCIC_BLK_DMA) {
		ccic_dma = devm_kzalloc(&pdev->dev,
				sizeof(struct ccic_dma), GFP_KERNEL);
		if (unlikely(ccic_dma == NULL)) {
			dev_err(&pdev->dev, "could not allocate memory\n");
			return -ENOMEM;
		}
		ret = of_property_read_u32(np, "dma_id", &i);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed to get dma_id\n");
			return -ENODEV;
		}
		pdev->id = pdev->dev.id = i;
		platform_set_drvdata(pdev, ccic_dma);
		ccic_dma->dev = &pdev->dev;

		ret = ccic_dma_create(ccic_dma);
		if (unlikely(ret < 0)) {
			dev_err(&pdev->dev, "failed to build CCICv2 dma-subdev\n");
			return ret;
		}
	} else
		return -ENODEV;

	return 0;
}

static int ccicv2_remove(struct platform_device *pdev)
{
	struct ccic_dma *ccic_dma = platform_get_drvdata(pdev);
	struct ccic_csi *ccic_csi = platform_get_drvdata(pdev);
	if (ccic_dma->block.id.mod_id == CCIC_BLK_DMA)
		ccic_dma_remove(ccic_dma);
	else if (ccic_csi->block.id.mod_id == CCIC_BLK_CSI)
		ccic_csi_remove(ccic_csi);
	return 0;
}

struct platform_driver ccicv2_driver = {
	.driver = {
		.name	= CCIC_DRV_NAME,
		.of_match_table = of_match_ptr(ccicv2_dt_match),
	},
	.probe	= ccicv2_probe,
	.remove	= ccicv2_remove,
};

module_platform_driver(ccicv2_driver);
