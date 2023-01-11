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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <media/soc_camera.h>
#include <media/soc_mediabus.h>
#include <media/v4l2-common.h>
#include <media/v4l2-dev.h>
#include <media/videobuf2-dma-contig.h>
#include <media/videobuf2-dma-sg.h>

#include <media/mrvl-camera.h> /* TBD refined */
#include <media/mv_sc2.h>
#include <media/mv_sc2_twsi_conf.h>
#include <linux/pm_qos.h>

#include "mv_sc2_camera.h"
static struct pm_qos_request sc2_camera_qos_idle;
static struct pm_qos_request gc2dfreq_qos_req_min;
static struct pm_qos_request sc2_camera_qos_ddr;
static int sc2_camera_req_qos;
static int sc2_camera_req_ddr_qos;

static int buffer_mode = -1;
module_param(buffer_mode, int, 0444);
MODULE_PARM_DESC(buffer_mode,
		"Set to 0 for vmalloc, 1 for DMA contiguous.");

#define MV_SC2_CAMERA_NAME "mv_sc2_camera"
static const struct soc_mbus_pixelfmt ccic_formats[] = {
	{
		.fourcc	= V4L2_PIX_FMT_UYVY,
		.name = "YUV422PACKED",
		.bits_per_sample = 8,
		.packing = SOC_MBUS_PACKING_2X8_PADLO,
		.order = SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc	= V4L2_PIX_FMT_YUYV,
		.name = "YVYU422PACKED",
		.bits_per_sample = 8,
		.packing = SOC_MBUS_PACKING_2X8_PADLO,
		.order = SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc	= V4L2_PIX_FMT_VYUY,
		.name = "VYUY422PACKED",
		.bits_per_sample = 8,
		.packing = SOC_MBUS_PACKING_2X8_PADLO,
		.order = SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc	= V4L2_PIX_FMT_YVYU,
		.name = "YVYU422PACKED",
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
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.name = "YCbCrSP",
		.bits_per_sample = 12,
		.packing = SOC_MBUS_PACKING_NONE,
		.order = SOC_MBUS_ORDER_LE,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV21,
		.name = "YCrCbSP",
		.bits_per_sample = 12,
		.packing = SOC_MBUS_PACKING_NONE,
		.order = SOC_MBUS_ORDER_LE,
	},
};

void soc_camera_set_ddr_qos(s32 value)
{
	pm_qos_update_request(&sc2_camera_qos_ddr, value);
}
EXPORT_SYMBOL(soc_camera_set_ddr_qos);

void soc_camera_set_gc2d_qos(s32 value)
{
	pm_qos_update_request(&gc2dfreq_qos_req_min, value);
}
EXPORT_SYMBOL(soc_camera_set_gc2d_qos);

static int mccic_queue_setup(struct vb2_queue *vq,
			const struct v4l2_format *fmt,
			u32 *count, u32 *num_planes,
			unsigned int sizes[], void *alloc_ctxs[])
{
	struct soc_camera_device *icd = container_of(vq,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	int i;
	int minbufs = 2;

	if (mcam_dev->mp.num_planes == 0) {
		dev_err(&mcam_dev->pdev->dev, "num_planes is 0 when reqbuf\n");
		return -EINVAL;
	}

	if (*count < minbufs)
		*count = minbufs;

	*num_planes = mcam_dev->mp.num_planes;
	for (i = 0; i < mcam_dev->mp.num_planes; i++) {
		sizes[i] = mcam_dev->mp.plane_fmt[i].sizeimage;
		alloc_ctxs[i] = mcam_dev->vb_alloc_ctx;
	}

	return 0;
}

static int mccic_vb2_init(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);
	int ret = 0, i;

	INIT_LIST_HEAD(&mbuf->queue);
	mbuf->list_init_flag = 1;
	mbuf->pdev = mcam_dev->pdev;
	if (mcam_dev->buffer_mode == B_DMA_CONTIG) {
		ret = 0;
	} else if (mcam_dev->buffer_mode == B_DMA_SG) {
		if (vb->v4l2_buf.memory == V4L2_MEMORY_USERPTR) {
			for (i = 0; i < vb->num_planes; i++)
				mbuf->ch_info[i].sizeimage =
					mcam_dev->mp.plane_fmt[i].sizeimage;
			ret = mcam_dev->sc2_mmu->ops->alloc_desc(vb);
		}
	} else
		return -EINVAL;

	return ret;
}

static int mccic_vb2_prepare(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);
	unsigned long size;
	int ret = 0;

	WARN(!list_empty(&mbuf->queue), "Buffer %p on queue!\n", vb);

	if (mcam_dev->buffer_mode == B_DMA_CONTIG) {
		/* no sure the below code is necessary */
		/* TBD: To Debug */
		WARN(!list_empty(&mbuf->queue), "Buffer %p on queue!\n", vb);
		size = vb2_plane_size(vb, 0);
		vb2_set_plane_payload(vb, 0, size);
		ret = 0;
	} else if (mcam_dev->buffer_mode == B_DMA_SG) {
		if (vb->v4l2_buf.memory == V4L2_MEMORY_USERPTR)
			ret = mcam_dev->sc2_mmu->ops->setup_sglist(vb);
		else if (vb->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
			ret = mcam_dev->sc2_mmu->ops->map_dmabuf(vb);
	} else
		return -EINVAL;

	return ret;
}

static void mccic_vb2_queue(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);
	unsigned long flags = 0;

	spin_lock_irqsave(&mcam_dev->mcam_lock, flags);
	list_add_tail(&mbuf->queue, &mcam_dev->buffers);
	spin_unlock_irqrestore(&mcam_dev->mcam_lock, flags);
}

static int mccic_vb2_finish(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	int ret = 0;

	if (mcam_dev->buffer_mode == B_DMA_CONTIG)
		ret = 0;
	else if (mcam_dev->buffer_mode == B_DMA_SG) {
		if (vb->v4l2_buf.memory == V4L2_MEMORY_USERPTR)
			ret = mcam_dev->sc2_mmu->ops->unmap_sglist(vb);
		else if (vb->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
			ret = mcam_dev->sc2_mmu->ops->unmap_dmabuf(vb);
	} else
		return -EINVAL;

	return ret;
}

static void mccic_vb2_cleanup(struct vb2_buffer *vb)
{
	struct soc_camera_device *icd = container_of(vb->vb2_queue,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct msc2_buffer *mbuf =
		container_of(vb, struct msc2_buffer, vb2_buf);

	if (mcam_dev->buffer_mode == B_DMA_SG) {
		if (vb->v4l2_buf.memory == V4L2_MEMORY_USERPTR)
			mcam_dev->sc2_mmu->ops->free_desc(vb);
		else if (vb->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
			mcam_dev->sc2_mmu->ops->unmap_dmabuf(vb);
	}

	if (mbuf->list_init_flag)
		list_del_init(&mbuf->queue);
	mbuf->list_init_flag = 0;
}

static int msc2_setup_buffer(struct mv_camera_dev *mcam_dev)
{
	struct msc2_mmu_dev *msc2_mmu = mcam_dev->sc2_mmu;
	struct ccic_dma_dev *dma_dev = mcam_dev->dma_dev;
	struct msc2_buffer *mbuf;
	struct vb2_buffer *vbuf;
	int pid = mcam_dev->pdev->id;
	struct device *dev = &mcam_dev->pdev->dev;
	unsigned long baddr; /* Need refine for 32/64 compat */

	if (list_empty(&mcam_dev->buffers)) {
		/*
		 * The single mode algorithm may be improved
		 * TBD later
		 */
		set_bit(CF_SINGLE_BUF, &mcam_dev->flags);
		mcam_dev->frame_state.singles++;
		if (!mcam_dev->mbuf_shadow && !mcam_dev->mbuf) {
			/*
			 * Really single buffer
			 * This should never happen
			 */
			dev_err(dev, "no video buffer available\n");
			return -EINVAL;
		} else {
			/* Just Use 2 buffers to pingpang */
			/* the code should be wrong
			 * mbuf_shadow should be on done_list
			 * driver can not touch it, otherwise
			 * frames sequence may be messed
			 * can judge the mcam_dev->buffers list
			 * in mccic_frame_complete() instead of
			 * the flags to prevent putting the last
			 * buffer to the done_list. And then the
			 * below code should be OK.
			 */
			if (mcam_dev->mbuf_shadow)
				mbuf = mcam_dev->mbuf_shadow;
			else
				mbuf = mcam_dev->mbuf;
		}
	} else {
		mbuf = list_first_entry(&mcam_dev->buffers, struct msc2_buffer,
							   queue);
		list_del_init(&mbuf->queue);
		clear_bit(CF_SINGLE_BUF, &mcam_dev->flags);
		if (mcam_dev->mbuf_shadow)
			list_add_tail(&mcam_dev->mbuf_shadow->queue, &mcam_dev->buffers);
		mcam_dev->mbuf_shadow = mcam_dev->mbuf;
		mcam_dev->mbuf = mbuf;
	}

	vbuf = &mbuf->vb2_buf;

	if (mcam_dev->buffer_mode == B_DMA_CONTIG) {
		switch (mcam_dev->mp.pixelformat) {
		case V4L2_PIX_FMT_YUV422P:
		case V4L2_PIX_FMT_YUV420:
			baddr = vb2_dma_contig_plane_dma_addr(vbuf, 0);
			baddr += mbuf->vb2_buf.v4l2_planes[0].data_offset;
			dma_dev->ops->set_yaddr(dma_dev, (u32) baddr);
			baddr = vb2_dma_contig_plane_dma_addr(vbuf, 1);
			baddr += mbuf->vb2_buf.v4l2_planes[1].data_offset;
			dma_dev->ops->set_uaddr(dma_dev, (u32) baddr);
			baddr = vb2_dma_contig_plane_dma_addr(vbuf, 2);
			baddr += mbuf->vb2_buf.v4l2_planes[2].data_offset;
			dma_dev->ops->set_vaddr(dma_dev, (u32) baddr);
			break;
		case V4L2_PIX_FMT_YVU420:
			baddr = vb2_dma_contig_plane_dma_addr(vbuf, 0);
			baddr += mbuf->vb2_buf.v4l2_planes[0].data_offset;
			dma_dev->ops->set_yaddr(dma_dev, (u32) baddr);
			baddr = vb2_dma_contig_plane_dma_addr(vbuf, 1);
			baddr += mbuf->vb2_buf.v4l2_planes[1].data_offset;
			dma_dev->ops->set_vaddr(dma_dev, (u32) baddr);
			baddr = vb2_dma_contig_plane_dma_addr(vbuf, 2);
			baddr += mbuf->vb2_buf.v4l2_planes[2].data_offset;
			dma_dev->ops->set_uaddr(dma_dev, (u32) baddr);
			break;
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			baddr = vb2_dma_contig_plane_dma_addr(vbuf, 0);
			baddr += mbuf->vb2_buf.v4l2_planes[0].data_offset;
			dma_dev->ops->set_yaddr(dma_dev, (u32) baddr);
			baddr = vb2_dma_contig_plane_dma_addr(vbuf, 1);
			baddr += mbuf->vb2_buf.v4l2_planes[1].data_offset;
			dma_dev->ops->set_uaddr(dma_dev, (u32) baddr);
			dma_dev->ops->set_vaddr(dma_dev, (u32) baddr);
			break;
		default:
			baddr = vb2_dma_contig_plane_dma_addr(vbuf, 0);
			baddr += mbuf->vb2_buf.v4l2_planes[0].data_offset;
			dma_dev->ops->set_yaddr(dma_dev, (u32) baddr);
			break;
		}
	} else if (mcam_dev->buffer_mode == B_DMA_SG) {
		switch (mcam_dev->mp.pixelformat) {
		case V4L2_PIX_FMT_YUV422P:
		case V4L2_PIX_FMT_YUV420:
			if (vbuf->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
				baddr = 1 << 28;
			else
				baddr = vbuf->v4l2_planes[0].m.userptr;
			dma_dev->ops->set_yaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[0].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 0 + pid * 4, 0);
			mbuf->ch_info[0].daddr = baddr;

			if (vbuf->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
				baddr = 2 << 28;
			else
				baddr = vbuf->v4l2_planes[1].m.userptr;
			dma_dev->ops->set_uaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[1].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 1 + pid * 4, 0);
			mbuf->ch_info[1].daddr = baddr;

			if (vbuf->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
				baddr = 3 << 28;
			else
				baddr = vbuf->v4l2_planes[2].m.userptr;
			dma_dev->ops->set_vaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[2].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 2 + pid * 4, 0);
			mbuf->ch_info[2].daddr = baddr;
			break;
		case V4L2_PIX_FMT_YVU420:
			if (vbuf->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
				baddr = 1 << 28;
			else
				baddr = vbuf->v4l2_planes[0].m.userptr;
			dma_dev->ops->set_yaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[0].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 0 + pid * 4, 0);
			mbuf->ch_info[0].daddr = baddr;

			if (vbuf->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
				baddr = 2 << 28;
			else
				baddr = vbuf->v4l2_planes[2].m.userptr;
#ifdef CONFIG_MACH_COREPRIMEVELTE
			dma_dev->ops->set_uaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[1].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 1 + pid * 4, 0);
#elif CONFIG_MACH_J1ACELTE_LTN
			dma_dev->ops->set_uaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[1].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 1 + pid * 4, 0);
#else
			dma_dev->ops->set_vaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[1].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 2 + pid * 4, 0);
#endif
			mbuf->ch_info[1].daddr = baddr;

			if (vbuf->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
				baddr = 3 << 28;
			else
				baddr = vbuf->v4l2_planes[1].m.userptr;
#ifdef CONFIG_MACH_COREPRIMEVELTE
			dma_dev->ops->set_vaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[2].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 2 + pid * 4, 0);
#elif CONFIG_MACH_J1ACELTE_LTN
			dma_dev->ops->set_vaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[2].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 2 + pid * 4, 0);
#else
			dma_dev->ops->set_uaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[2].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 1 + pid * 4, 0);
#endif
			mbuf->ch_info[2].daddr = baddr;
			break;
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
			if (vbuf->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
				baddr = 1 << 28;
			else
				baddr = vbuf->v4l2_planes[0].m.userptr;
			dma_dev->ops->set_yaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[0].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 0 + pid * 4, 0);
			mbuf->ch_info[0].daddr = baddr;

			if (vbuf->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
				baddr = 2 << 28;
			else
				baddr = vbuf->v4l2_planes[1].m.userptr;
			dma_dev->ops->set_uaddr(dma_dev, (u32) baddr);
			dma_dev->ops->set_vaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[1].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 1 + pid * 4, 0);
			mbuf->ch_info[1].daddr = baddr;
			break;
		default:
			if (vbuf->v4l2_buf.memory == V4L2_MEMORY_DMABUF)
				baddr = 1 << 28;
			else
				baddr = vbuf->v4l2_planes[0].m.userptr;
			dma_dev->ops->set_yaddr(dma_dev, (u32) baddr);
			mbuf->ch_info[0].tid =  msc2_mmu->ops->get_tid(msc2_mmu,
				pid, 0 + pid * 4, 0);
			mbuf->ch_info[0].daddr = baddr;
			break;
		}
		msc2_mmu->ops->config_ch(msc2_mmu, mbuf->ch_info,
					mbuf->vb2_buf.num_planes);
	}
	return 0;
}

static int mccic_acquire_sc2_chs(struct mv_camera_dev *mcam_dev, int num)
{
	struct msc2_mmu_dev *msc2_mmu = mcam_dev->sc2_mmu;
	int pid = mcam_dev->pdev->id;
	u32 fid;
	int ch, ret;

	for (ch = 0; ch < num; ch++) {
		/* calc the magic number for ccic */
		fid = msc2_mmu->ops->get_tid(msc2_mmu, pid, ch + pid * 4, 0);
		ret = msc2_mmu->ops->acquire_ch(msc2_mmu, fid);
		if (ret)
			goto failed;
	}
	return 0;
failed:
	while ((--ch) >= 0) {
		fid = msc2_mmu->ops->get_tid(msc2_mmu, pid, ch + pid * 4, 0);
		msc2_mmu->ops->release_ch(msc2_mmu, fid);
	}
	return ret;
}

static void mccic_release_sc2_chs(struct mv_camera_dev *mcam_dev, int num)
{
	struct msc2_mmu_dev *msc2_mmu = mcam_dev->sc2_mmu;
	int pid = mcam_dev->pdev->id;
	u32 fid;
	int ch;

	for (ch = 0; ch < num; ch++) {
		/* calc the magic number for ccic */
		fid = msc2_mmu->ops->get_tid(msc2_mmu, pid, ch + pid * 4, 0);
		msc2_mmu->ops->release_ch(msc2_mmu, fid);
	}
	return;
}

static int mccic_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct soc_camera_device *icd = container_of(vq,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct ccic_dma_dev *dma_dev = mcam_dev->dma_dev;
	struct ccic_ctrl_dev *ctrl_dev = mcam_dev->ctrl_dev;
	struct msc2_mmu_dev *msc2_mmu = mcam_dev->sc2_mmu;
	struct device *dev = &mcam_dev->pdev->dev;
	unsigned long flags = 0;
	int ret;

	if (list_empty(&mcam_dev->buffers)) {
		dev_err(dev, "no vb2 buffer is queued before streaming");
		return -EINVAL; /* reconsiderate it, TBD */
	}

	/* disable/enable SC2 clks to reset SC2 */
	ctrl_dev->ops->clk_disable(ctrl_dev);
	clk_disable_unprepare(mcam_dev->axi_clk);
	ctrl_dev->ops->clk_enable(ctrl_dev);
	clk_prepare_enable(mcam_dev->axi_clk);

	ret = mccic_acquire_sc2_chs(mcam_dev, mcam_dev->mp.num_planes);
	if (ret)
		return ret;

	spin_lock_irqsave(&mcam_dev->mcam_lock, flags);
	clear_bit(CF_FRAME_SOF0, &mcam_dev->flags);

	ctrl_dev->ops->config_mbus(ctrl_dev, mcam_dev->bus_type,
					mcam_dev->mbus_flags, 1);
	dma_dev->ops->setup_image(dma_dev);

/* +	/\* Not sure the new SoC need such code *\/ */
/* +	/\* if (sdata->bus_type == V4L2_MBUS_CSI2) *\/ */
/* +	/\* ccic_reg_set_bit(mcam_dev, REG_CTRL2, ISIM_FIX); *\/ */
/* +	/\* Not sure the following code is Ok for all sensor *\/ */
/* +	/\* ccic_reg_set_bit(mcam_dev, REG_CTRL1, C1_PWRDWN) *\/ */

	msc2_setup_buffer(mcam_dev);

	if (mcam_dev->buffer_mode == B_DMA_CONTIG) {
		/* nothing to do here */
	} else if (mcam_dev->buffer_mode == B_DMA_SG) {
		u32 i, tid[mcam_dev->mbuf->vb2_buf.num_planes];
		for (i = 0; i < mcam_dev->mbuf->vb2_buf.num_planes; i++)
			tid[i] = mcam_dev->mbuf->ch_info[i].tid;
		ret = msc2_mmu->ops->enable_ch(msc2_mmu, tid,
					mcam_dev->mbuf->vb2_buf.num_planes);
		if (ret) {
			dma_dev->ops->ccic_disable(dma_dev);
			ctrl_dev->ops->irq_mask(ctrl_dev, 0);
			spin_unlock_irqrestore(&mcam_dev->mcam_lock, flags);
			return ret;
		}
	} else {
		spin_unlock_irqrestore(&mcam_dev->mcam_lock, flags);
		return -EINVAL;
	}

	dma_dev->ops->shadow_ready(dma_dev);
	ctrl_dev->ops->irq_mask(ctrl_dev, 1);
	dma_dev->ops->ccic_enable(dma_dev);
	mcam_dev->state = S_STREAMING;

	spin_unlock_irqrestore(&mcam_dev->mcam_lock, flags);
	return 0;
}

static int mccic_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct soc_camera_device *icd = container_of(vq,
			struct soc_camera_device, vb2_vidq);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct ccic_dma_dev *dma_dev = mcam_dev->dma_dev;
	struct ccic_ctrl_dev *ctrl_dev = mcam_dev->ctrl_dev;
	struct msc2_mmu_dev *msc2_mmu = mcam_dev->sc2_mmu;
	unsigned long flags = 0;

	spin_lock_irqsave(&mcam_dev->mcam_lock, flags);

	mcam_dev->state = S_IDLE;
	dma_dev->ops->ccic_disable(dma_dev);
	ctrl_dev->ops->irq_mask(ctrl_dev, 0);
	ctrl_dev->ops->config_mbus(ctrl_dev, mcam_dev->bus_type,
			mcam_dev->mbus_flags, 0);

	if (mcam_dev->buffer_mode == B_DMA_CONTIG) {
		/* nothing to do here */
	} else if (mcam_dev->buffer_mode == B_DMA_SG) {
		u32 i, tid[mcam_dev->mbuf->vb2_buf.num_planes];
		for (i = 0; i < mcam_dev->mbuf->vb2_buf.num_planes; i++)
			tid[i] = mcam_dev->mbuf->ch_info[i].tid;
		msc2_mmu->ops->disable_ch(msc2_mmu, tid,
					mcam_dev->mbuf->vb2_buf.num_planes);
	} else {
		spin_unlock_irqrestore(&mcam_dev->mcam_lock, flags);
		return -EINVAL;
	}

	INIT_LIST_HEAD(&mcam_dev->buffers);
	mcam_dev->mbuf = NULL;
	mcam_dev->mbuf_shadow = NULL;

	/* reset the ccic ??? */
	dev_dbg(icd->parent, "Release %d frames, %d singles, %d delivered\n",
		mcam_dev->frame_state.frames, mcam_dev->frame_state.singles,
		mcam_dev->frame_state.delivered);
	spin_unlock_irqrestore(&mcam_dev->mcam_lock, flags);
	mccic_release_sc2_chs(mcam_dev, mcam_dev->mp.num_planes);

	return 0;
}



/* + */
/* +static void ccic_enable_clk(struct msc2_ccic_dev *ccic_dev) */
/* +{ */
/* +	/\* TBD *\/ */
/* +} */
/* + */
/* +static void ccic_disable_clk(struct msc2_ccic_dev *ccic_dev) */
/* +{ */
/* +	/\* TBD *\/ */
/* +} */
/* + */




/* + */
/* +static int mccic_init_clk(struct msc2_ccic_dev *ccic_dev) */
/* +{ */
/* +	/\* TBD *\/ */
/* +	return 0; */
/* +} */


static struct vb2_ops mccic_vb2_ops = {
	.queue_setup		= mccic_queue_setup,
	.buf_init		= mccic_vb2_init,
	.buf_prepare		= mccic_vb2_prepare,
	.buf_queue		= mccic_vb2_queue,
	.buf_finish		= mccic_vb2_finish,
	.buf_cleanup		= mccic_vb2_cleanup,
	.start_streaming	= mccic_vb2_start_streaming,
	.stop_streaming		= mccic_vb2_stop_streaming,
	.wait_prepare		= soc_camera_unlock,
	.wait_finish		= soc_camera_lock,
};


static irqreturn_t dma_irq_handler(struct ccic_dma_dev *dev, u32 irqs);
static irqreturn_t ctrl_irq_handler(struct ccic_ctrl_dev *ctrl_dev, u32 irqs);
static int mccic_add_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct soc_camera_desc *sdesc = to_soc_camera_desc(icd);
	struct soc_camera_subdev_desc *ssdd = &sdesc->subdev_desc;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct device *dev = &mcam_dev->pdev->dev;
	struct device_node *np = dev->of_node;
	struct msc2_mmu_dev *msc2_mmu;
	struct ccic_ctrl_dev *ctrl_dev;
	int ret = 0;

	/* check buffer mode, user can change it before open */
	if (buffer_mode >= 0)
		mcam_dev->buffer_mode = buffer_mode;
	if (mcam_dev->buffer_mode != B_DMA_CONTIG &&
		mcam_dev->buffer_mode != B_DMA_SG)
		return -EINVAL;

	if (mcam_dev->icd)
		return -EBUSY;
	if(icd->iface == 0) {
		pm_qos_update_request(&sc2_camera_qos_ddr, 312000);
	} 
	/* 1, initial the mcam_dev */
	/* alloc ctx, need reconsider */
	if (mcam_dev->buffer_mode == B_DMA_CONTIG)
		mcam_dev->vb_alloc_ctx = (struct vb2_alloc_ctx *)
			vb2_dma_contig_init_ctx(&mcam_dev->pdev->dev);
	else
		mcam_dev->vb_alloc_ctx = (struct vb2_alloc_ctx *)
			vb2_dma_sg_init_ctx(&mcam_dev->pdev->dev);
	if (IS_ERR(mcam_dev->vb_alloc_ctx))
		return PTR_ERR(mcam_dev->vb_alloc_ctx);

	pm_qos_update_request(&sc2_camera_qos_idle, sc2_camera_req_qos);
	/* 3.4 is not necessary soc_camera_power_on*/
	/* GPIO and regulator related for sensor */
	soc_camera_power_on(&mcam_dev->pdev->dev, ssdd, NULL);
	/* CCIC/ISP/MMU power on */
	pm_runtime_get_sync(mcam_dev->dev);
	/* ccic_enable_clk(mcam_dev); */
	/* ccic_power_up(mcam_dev); */
	/* TBD: ccic_stop(mcam_dev); */
	/* clk_set_rate(mcam_dev->mclk, 26000000); */
	/* clk_prepare_enable(mcam_dev->mclk); */
	mcam_dev->frame_state.frames = 0;
	mcam_dev->frame_state.singles = 0;
	mcam_dev->frame_state.delivered = 0;

	icd->vb2_vidq.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	mcam_dev->state = S_IDLE;

	/*
	 * Need sleep 1ms-2ms to wait for CCIC stable
	 */
	usleep_range(1000, 2000);

	/* 2. get msc2_mmu_dev */
	/* always use sc2_mmu 0 */
	ret = msc2_get_sc2(&mcam_dev->sc2_mmu, 0);
	if (ret) {
		dev_warn(dev, "no sc2 id %d found\n", 0);
		goto power_off;
	}
	msc2_mmu = mcam_dev->sc2_mmu;
	/* 3. get the buffer mode and set or get bypass status */
	/* ccic only cares write bypass */
	if (mcam_dev->buffer_mode == B_DMA_CONTIG)
		ret = msc2_mmu->ops->rbypass(msc2_mmu, 1);
	else
		ret = msc2_mmu->ops->rbypass(msc2_mmu, 0);

	if (ret) {
		ret = msc2_mmu->ops->bypass_status(msc2_mmu);
		if (((ret & (1 << SC2_WBYPASS)) &&
			 (mcam_dev->buffer_mode != B_DMA_CONTIG)) ||
			(!(ret & (1 << SC2_WBYPASS)) &&
			 (mcam_dev->buffer_mode != B_DMA_SG))) {
			dev_warn(dev, "sc2 mmu bypass mode not match ccic buffer mode\n");
			goto put_mmu;
		}
	}

	/* 4. get the ccic_dma_dev and ccic_ctrl_dev */
	ret = msc2_get_ccic_dma(&mcam_dev->dma_dev,
				mcam_dev->id, dma_irq_handler);
	if (ret) {
		dev_warn(dev, "no ccic_dma_dev %d found\n", 0);
		goto put_mmu;
	}
	mcam_dev->dma_dev->priv = mcam_dev;

	ret = msc2_get_ccic_ctrl(&mcam_dev->ctrl_dev,
				mcam_dev->id, ctrl_irq_handler);
	if (ret) {
		dev_warn(dev, "no ccic_ctrl_dev %d found\n", 0);
		goto put_dma;
	}
	ctrl_dev = mcam_dev->ctrl_dev;
	ctrl_dev->priv = mcam_dev;

	if (of_get_property(np, "sc2-i2c-dyn-ctrl", NULL))
		mcam_dev->i2c_dyn_ctrl = 1;

	if (mcam_dev->i2c_dyn_ctrl) {
		ret = sc2_select_pins_state(ctrl_dev->ccic_dev->id,
				SC2_PIN_ST_TWSI, SC2_MOD_CCIC);
		if (ret < 0)
			return ret;
	}
	ret = of_property_read_u32_array(np, "dphy_val",
			ctrl_dev->csi.dphy, ARRAY_SIZE(ctrl_dev->csi.dphy));
	if (ret)
		ctrl_dev->csi.calc_dphy = 1;
	else
		ctrl_dev->csi.calc_dphy = 0;

	/* 5. initial the HW, clk, power, ccic */
	ctrl_dev->ops->clk_enable(ctrl_dev);
	clk_prepare_enable(mcam_dev->axi_clk);
	ctrl_dev->ops->power_up(ctrl_dev);
	ctrl_dev->ops->irq_mask(ctrl_dev, 0); /* Mask interrupts */

	/* 6. call sd init */
	ret = v4l2_subdev_call(sd, core, init, 0);
	if ((ret < 0) && (ret != -ENOIOCTLCMD) && (ret != -ENODEV)) {
		dev_warn(dev,
			"camera: Failed to initialize subdev: %d\n", ret);
		goto pwd;
	}

	mcam_dev->icd = icd;
	return 0;
pwd:
	if (mcam_dev->i2c_dyn_ctrl)
		sc2_select_pins_state(ctrl_dev->ccic_dev->id,
				SC2_PIN_ST_GPIO, SC2_MOD_CCIC);
	ctrl_dev->ops->power_down(ctrl_dev);
	msc2_put_ccic_ctrl(&mcam_dev->ctrl_dev);
put_dma:
	msc2_put_ccic_dma(&mcam_dev->dma_dev);
put_mmu:
	msc2_put_sc2(&mcam_dev->sc2_mmu);
	/* TBD: ccic_power_down(mcam_dev); */
	/* TBD: ccic_disable_clk(mcam_dev); */
power_off:
	pm_qos_add_request(&sc2_camera_qos_idle, PM_QOS_CPUIDLE_BLOCK,
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	pm_runtime_put(mcam_dev->dev);
	soc_camera_power_off(&mcam_dev->pdev->dev, ssdd, NULL);

	return ret;
}

static void mccic_remove_device(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct soc_camera_desc *sdesc = to_soc_camera_desc(icd);
	struct soc_camera_subdev_desc *ssdd = &sdesc->subdev_desc;
	struct ccic_ctrl_dev *ctrl_dev = mcam_dev->ctrl_dev;

	/* 1. put msc2_mmu_dev */
	msc2_put_sc2(&mcam_dev->sc2_mmu);
	mcam_dev->sc2_mmu = NULL;
	/* 2. handle HW, clk, power, ccic */
	clk_disable_unprepare(mcam_dev->axi_clk);
	/* TBD ccic_power_down(ccic_dev); */
	/* TBD ccic_disable_clk(ccic_dev); */
	/* clk_disable_unprepare(mcam_dev->mclk);*/
	pm_runtime_put(mcam_dev->dev);
	/* 3.4 is not necessary */
	soc_camera_power_off(&mcam_dev->pdev->dev, ssdd, NULL);

	if (mcam_dev->i2c_dyn_ctrl)
		sc2_select_pins_state(ctrl_dev->ccic_dev->id,
				SC2_PIN_ST_GPIO, SC2_MOD_CCIC);
	/* 3. put ccic_dma_dev and ccic_ctrl_dev */
	msc2_put_ccic_dma(&mcam_dev->dma_dev);
	mcam_dev->dma_dev = NULL;
	msc2_put_ccic_ctrl(&mcam_dev->ctrl_dev);
	mcam_dev->ctrl_dev = NULL;
	/* 4. handle ccic_dev */
	mcam_dev->icd = NULL;

	ctrl_dev->ops->clk_disable(ctrl_dev);
	/* 5. free ctx, need reconsider */
	if (mcam_dev->buffer_mode == B_DMA_CONTIG)
		vb2_dma_contig_cleanup_ctx(mcam_dev->vb_alloc_ctx);
	else
		vb2_dma_sg_cleanup_ctx(mcam_dev->vb_alloc_ctx);
	mcam_dev->vb_alloc_ctx = NULL;
	pm_qos_update_request(&sc2_camera_qos_idle,
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	if(icd->iface == 0) {
		pm_qos_update_request(&sc2_camera_qos_ddr, PM_QOS_DEFAULT_VALUE);
	}
}

/* try_fmt should not return error, TBD */
static int mccic_try_fmt(struct soc_camera_device *icd,
			struct v4l2_format *f)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	const struct soc_camera_format_xlate *xlate;
	struct v4l2_mbus_framefmt mf;
	__u32 pixfmt = pix_mp->pixelformat;
	int ret;

	if (f->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	xlate = soc_camera_xlate_by_fourcc(icd, pixfmt);
	if (!xlate) {
		dev_err(icd->parent, "camera: format: %c not found\n",
				pix_mp->pixelformat);
		return -EINVAL;
	}
	/*
	 * limit to sensor capabilities
	 */
	mf.width = pix_mp->width;
	mf.height = pix_mp->height;
	mf.field = V4L2_FIELD_NONE;
	mf.colorspace = pix_mp->colorspace;
	mf.code = xlate->code;

	ret = v4l2_subdev_call(sd, video, try_mbus_fmt, &mf);
	if (ret < 0)
		return ret;

	pix_mp->width = mf.width;
	pix_mp->height = mf.height;
	pix_mp->colorspace = mf.colorspace;

	switch (mf.field) {
	case V4L2_FIELD_ANY:
	case V4L2_FIELD_NONE:
		pix_mp->field = V4L2_FIELD_NONE;
		break;
	default:
		dev_err(icd->parent, "Field type %d unsupported\n", mf.field);
		ret = -EINVAL;
		break;
	}

	return 0;
}

static int mccic_get_num_planes(const struct soc_mbus_pixelfmt *host_fmt)
{
	switch (host_fmt->fourcc) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		return 3;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		return 2;
	default:
		return 1;
	}
	return 1;	/* should never come here */
}

static void mccic_setup_mp_pixfmt(struct device *dev,
				struct v4l2_pix_format_mplane *pix_mp,
				const struct soc_mbus_pixelfmt *host_fmt)
{
	int i;
	int bpl, lpp;

	switch (host_fmt->fourcc) {
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		bpl = pix_mp->width * 2;
		lpp = pix_mp->height;
		if (pix_mp->plane_fmt[0].bytesperline < bpl)
			pix_mp->plane_fmt[0].bytesperline = bpl;
		pix_mp->plane_fmt[0].sizeimage = bpl * lpp;
		break;
	case V4L2_PIX_FMT_YUV422P:
		bpl = pix_mp->width;
		lpp = pix_mp->height;
		if (pix_mp->plane_fmt[0].bytesperline < bpl)
			pix_mp->plane_fmt[0].bytesperline = bpl;
		pix_mp->plane_fmt[0].sizeimage = bpl * lpp;
		bpl = pix_mp->width / 2;
		lpp = pix_mp->height;
		for (i = 1; i < 3; i++) {
			if (pix_mp->plane_fmt[i].bytesperline < bpl)
				pix_mp->plane_fmt[i].bytesperline = bpl;
			pix_mp->plane_fmt[i].sizeimage = bpl * lpp;
		}
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		bpl = pix_mp->width;
		lpp = pix_mp->height;
		if (pix_mp->plane_fmt[0].bytesperline < bpl)
			pix_mp->plane_fmt[0].bytesperline = bpl;
		pix_mp->plane_fmt[0].sizeimage = bpl * lpp;
		bpl = pix_mp->width / 2;
		lpp = pix_mp->height / 2;
		for (i = 1; i < 3; i++) {
			if (pix_mp->plane_fmt[i].bytesperline < bpl)
				pix_mp->plane_fmt[i].bytesperline = bpl;
			pix_mp->plane_fmt[i].sizeimage = bpl * lpp;
		}
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		bpl = pix_mp->width;
		lpp = pix_mp->height;
		if (pix_mp->plane_fmt[0].bytesperline < bpl)
			pix_mp->plane_fmt[0].bytesperline = bpl;
		pix_mp->plane_fmt[0].sizeimage = bpl * lpp;
		bpl = pix_mp->width / 2;
		lpp = pix_mp->height;
		if (pix_mp->plane_fmt[1].bytesperline < bpl)
			pix_mp->plane_fmt[1].bytesperline = bpl;
		pix_mp->plane_fmt[1].sizeimage = bpl * lpp;
		break;
	default:
		dev_warn(dev, "camera: use userspace assigned sizeimage\n");
		/* Use the assigned value from userspace.
		 * Manually add new fmts if needed.
		 */
		break;
	}
}

static int mccic_set_fmt(struct soc_camera_device *icd,
			struct v4l2_format *f)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct ccic_dma_dev *dma_dev = mcam_dev->dma_dev;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	const struct soc_camera_format_xlate *xlate = NULL;
	struct v4l2_mbus_framefmt mf;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	int ret;

	/* 1. call s_mbuf_fmt */
	xlate = soc_camera_xlate_by_fourcc(icd, pix_mp->pixelformat);
	if (!xlate) {
		dev_err(icd->parent, "camera: format: %c not found\n",
				pix_mp->pixelformat);
		return -EINVAL;
	}

	mf.width = pix_mp->width;
	mf.height = pix_mp->height;
	mf.field = V4L2_FIELD_NONE;
	mf.colorspace = pix_mp->colorspace;
	mf.code = xlate->code;

	ret = v4l2_subdev_call(sd, video, s_mbus_fmt, &mf);
	if (ret < 0) {
		dev_err(icd->parent, "camera: set_fmt failed %d\n", __LINE__);
		return ret;
	}

	if (mf.code != xlate->code) {
		dev_err(icd->parent, "wrong xlate code\n");
		return -EINVAL;
	}


	/* 2. save the fmt in mv_camera_dev */
	pix_mp->width = mf.width;
	pix_mp->height = mf.height;
	pix_mp->field = mf.field;
	pix_mp->colorspace = mf.colorspace;
	pix_mp->num_planes = mccic_get_num_planes(xlate->host_fmt);
	mccic_setup_mp_pixfmt(icd->parent, pix_mp, xlate->host_fmt);
	icd->current_fmt = xlate;
	memcpy(&(mcam_dev->mp), pix_mp, sizeof(struct v4l2_pix_format_mplane));

	/* 3. save teh fmt in ccic_dma_dev */
	dma_dev->pixfmt = xlate->host_fmt->fourcc;
	dma_dev->width = mf.width;
	dma_dev->height = mf.height;
	/* bytesperline is only used in jpeg mode, using plane[0]'s value */
	dma_dev->bytesperline = pix_mp->plane_fmt[0].bytesperline;
	dma_dev->code = mf.code;

	return 0;
}

static int mccic_set_parm(struct soc_camera_device *icd,
			struct v4l2_streamparm *para)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);

	if (para->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	return v4l2_subdev_call(sd, video, s_parm, para);
}

static int mccic_init_vb2_queue(struct vb2_queue *q,
			struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;

	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_USERPTR | VB2_DMABUF;
	q->drv_priv = icd;
	q->ops = &mccic_vb2_ops;
	if (mcam_dev->buffer_mode == B_DMA_CONTIG)
		q->mem_ops = &vb2_dma_contig_memops;
	else if (mcam_dev->buffer_mode == B_DMA_SG)
		q->mem_ops = &vb2_dma_sg_memops;
	else
		return -EINVAL;
	q->buf_struct_size = sizeof(struct msc2_buffer);
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;

	return vb2_queue_init(q);
}

static unsigned int mccic_poll(struct file *file, poll_table *pt)
{
	struct soc_camera_device *icd = file->private_data;

	return vb2_poll(&icd->vb2_vidq, file, pt);
}

static int mccic_querycap(struct soc_camera_host *ici,
			struct v4l2_capability *cap)
{
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct soc_camera_device *icd = mcam_dev->icd;
	struct soc_camera_desc *sdesc = to_soc_camera_desc(icd);
	struct soc_camera_host_desc *shd = &sdesc->host_desc;

	cap->version = KERNEL_VERSION(0, 0, 5);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING;

	strcpy(cap->card, MV_SC2_CAMERA_NAME);
	strcpy(cap->driver, shd->module_name);

	return 0;
}

static int get_lane_num(unsigned int flags)
{
	/* We will always use the maximum number of lane */
	if (flags & V4L2_MBUS_CSI2_4_LANE)
		return 4;
	else if (flags & V4L2_MBUS_CSI2_2_LANE)
		return 2;
	else if (flags & V4L2_MBUS_CSI2_1_LANE)
		return 1;
	else
		return 0;
}


/*
 * refine the function later
 * 1. get host capability
 * 2. get sensor capability
 * 3. select a suitable setting
 * 4. call s_mbus_config to config sensor
 */
static int mccic_set_bus_param(struct soc_camera_device *icd)
{
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct soc_camera_desc *sdesc = to_soc_camera_desc(icd);
	struct soc_camera_subdev_desc *ssdd = &sdesc->subdev_desc;
	struct sensor_board_data *sdata = ssdd->drv_priv;
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct mv_camera_dev *mcam_dev = ici->priv;
	struct ccic_ctrl_dev *ctrl_dev = mcam_dev->ctrl_dev;
	struct v4l2_mbus_config cfg;
	int ret;

	ret = v4l2_subdev_call(sd, video, g_mbus_config, &cfg);
	if ((ret < 0) && (ret != -ENOIOCTLCMD)) {
		dev_dbg(icd->parent, "g_mbus_config failed\n");
		return ret;
	}

	if (ret == -ENOIOCTLCMD) {
		/* if type not set, the default will be 0,
		 * which means parallel mode
		 */
		cfg.type = sdata->bus_type; /* use default setting */
		cfg.flags = sdata->flags;
	}

	mcam_dev->bus_type = cfg.type;
	switch (cfg.type) {
	case V4L2_MBUS_CSI2:
		if ((cfg.flags & sdata->flags) == 0)
			return -EINVAL;
		mcam_dev->mbus_flags = get_lane_num(cfg.flags & sdata->flags);
		ctrl_dev->csi.dphy_desc.nr_lane =
			get_lane_num(cfg.flags & sdata->flags);
		break;
	case V4L2_MBUS_PARALLEL:
		mcam_dev->mbus_flags = (cfg.flags & sdata->flags);
		if (mcam_dev->mbus_flags == 0)
			mcam_dev->mbus_flags = V4L2_MBUS_HSYNC_ACTIVE_HIGH |
				V4L2_MBUS_VSYNC_ACTIVE_HIGH |
				V4L2_MBUS_PCLK_SAMPLE_RISING |
				V4L2_MBUS_SLAVE;

		/* we always prefer H/VSYNC HIGN, RISING PCLK */
		if (mcam_dev->mbus_flags & V4L2_MBUS_HSYNC_ACTIVE_HIGH)
			mcam_dev->mbus_flags =
				(mcam_dev->mbus_flags &
				 ~V4L2_MBUS_HSYNC_ACTIVE_LOW);
		if (mcam_dev->mbus_flags & V4L2_MBUS_VSYNC_ACTIVE_HIGH)
			mcam_dev->mbus_flags =
				(mcam_dev->mbus_flags &
				 ~V4L2_MBUS_VSYNC_ACTIVE_LOW);
		if (mcam_dev->mbus_flags & V4L2_MBUS_PCLK_SAMPLE_RISING)
			mcam_dev->mbus_flags =
				(mcam_dev->mbus_flags &
				 ~V4L2_MBUS_PCLK_SAMPLE_FALLING);
		mcam_dev->mbus_flags |= V4L2_MBUS_DATA_ACTIVE_HIGH;
		break;
	default:
		return -EINVAL;
	}
	cfg.flags = mcam_dev->mbus_flags;
	ret = v4l2_subdev_call(sd, video, s_mbus_config, &cfg);
	if (ret < 0 && ret != -ENOIOCTLCMD) {
		dev_dbg(icd->parent, "camera s_mbus_config(0x%x) returned %d\n",
			cfg.flags, ret);
		return ret;
	}
	return 0;
}

static int mccic_fmt_match_best(struct mv_camera_dev *mcam_dev,
			const struct soc_mbus_pixelfmt *ccic_format,
			enum v4l2_mbus_pixelcode code)
{
	struct ccic_dma_dev *dma_dev = mcam_dev->dma_dev;
	int i, score = 0, hscore = 0, tmp;

	for (i = 0; i < mcam_dev->mbus_fmt_num; i++) {
		tmp = dma_dev->ops->mbus_fmt_rating(ccic_format->fourcc,
				mcam_dev->mbus_fmt_code[i]);
		if (hscore < tmp)
			hscore = tmp;
	}
	score = dma_dev->ops->mbus_fmt_rating(ccic_format->fourcc, code);
	if (score >= hscore && score > 0)
		return 1;
	else
		return 0;
}

static int mccic_get_formats(struct soc_camera_device *icd, u32 idx,
			struct soc_camera_format_xlate	*xlate)
{
	struct v4l2_subdev *sd = soc_camera_to_subdev(icd);
	struct soc_camera_host *ici = to_soc_camera_host(icd->parent);
	struct mv_camera_dev *mcam_dev = ici->priv;
	enum v4l2_mbus_pixelcode code;
	int formats = 0, ret = 0, i;

	if (mcam_dev->mbus_fmt_num == 0) {
		i = 0;
		while (!v4l2_subdev_call(sd, video, enum_mbus_fmt, i, &code))
			mcam_dev->mbus_fmt_code[i++] = code;
		mcam_dev->mbus_fmt_num = i;
	}

	ret = v4l2_subdev_call(sd, video, enum_mbus_fmt, idx, &code);
	if (ret < 0)
		return 0;	/* No more formats */

	for (i = 0; i < ARRAY_SIZE(ccic_formats); i++) {
		ret = mccic_fmt_match_best(mcam_dev, &ccic_formats[i], code);
		if (ret == 0)
			continue;
		formats++;
		if (xlate) {
			xlate->host_fmt = &ccic_formats[i];
			xlate->code = code;
			xlate++;
		}
	}
	return formats;
}

static int mccic_clock_start(struct soc_camera_host *ici)
{
	return 0;
}

static void mccic_clock_stop(struct soc_camera_host *ici)
{
}

static struct soc_camera_host_ops mcam_soc_camera_host_ops = {
	.owner		= THIS_MODULE,
	.add		= mccic_add_device,
	.remove		= mccic_remove_device,
	.clock_start	= mccic_clock_start,
	.clock_stop	= mccic_clock_stop,
	.set_fmt	= mccic_set_fmt,
	.try_fmt	= mccic_try_fmt,
	.set_parm	= mccic_set_parm,
	.init_videobuf2	= mccic_init_vb2_queue,
	.poll		= mccic_poll,
	.querycap	= mccic_querycap,
	.set_bus_param	= mccic_set_bus_param,
	.get_formats	= mccic_get_formats,
};

static void mccic_buffer_done(struct mv_camera_dev *mcam_dev,
				struct vb2_buffer *vbuf)
{
	int i;
	for (i = 0; i < mcam_dev->mp.num_planes; i++)
		vb2_set_plane_payload(vbuf, i,
				mcam_dev->mp.plane_fmt[i].sizeimage);
	vb2_buffer_done(vbuf, VB2_BUF_STATE_DONE);
}


static inline void mccic_frame_complete(struct mv_camera_dev *mcam_dev)
{
	struct msc2_buffer *mbuf;

	mcam_dev->frame_state.frames++;

	mbuf = mcam_dev->mbuf_shadow;
	if (!test_bit(CF_SINGLE_BUF, &mcam_dev->flags) && mbuf) {
		mcam_dev->frame_state.delivered++;
		mccic_buffer_done(mcam_dev, &mbuf->vb2_buf);
		mcam_dev->mbuf_shadow = NULL;
	}
}

/*
 * SoF:
 *	 Update the baddr in ccic
 *	 Update the sc2 mmu Descriptor Chain in SG mode
 *
 * EoF:
 *	 deliver the frame to userspace
 */
/* irq bit definition is the same as HW */
#define	  IRQ_EOF0	  0x00000001
#define	  IRQ_SOF0	  0x00000008
#define	  IRQ_DMA_NOT_DONE		0x00000010
#define	  IRQ_SHADOW_NOT_RDY	0x00000020
#define	  IRQ_OVERFLOW	  0x00000040
#define	  IRQ_CSI2IDI_HBLK2HSYNC	0x00000400
#define	  FRAMEIRQS	  (IRQ_EOF0 | IRQ_SOF0)
#define	  ALLIRQS	  (FRAMEIRQS | IRQ_OVERFLOW | IRQ_DMA_NOT_DONE | \
			IRQ_SHADOW_NOT_RDY | IRQ_CSI2IDI_HBLK2HSYNC)
static irqreturn_t dma_irq_handler(struct ccic_dma_dev *dma_dev, u32 irqs)
{
	struct mv_camera_dev *mcam_dev = dma_dev->priv;
	struct device *dev = &mcam_dev->pdev->dev;

	/*
	 * In OVERFLOW, we don't touch bufferQ, so no need to grab buffer
	 * lock.
	 */
	if (irqs & IRQ_OVERFLOW) {
		/*
		 * In case of overflow, the 1st thing to do is to tell HW
		 * stop working by clear ready bit. But can't reset MMU
		 * right now, because it's possible that the subsequent
		 * buffer already start been DMAed on. So just make a mark
		 * here and do error handling in drop frame IRQ.
		 */
		dma_dev->ops->shadow_empty(dma_dev);
		set_bit(CF_FRAME_OVERFLOW, &mcam_dev->flags);
		irqs &= ~IRQ_OVERFLOW;
		dev_warn(dev, "irq overflow!\n");
	}

	spin_lock(&mcam_dev->mcam_lock);
	/*
	 * It's possible that SOF comes together with EOF, the sequence to
	 * handle them depends on whether DMA is active, this can be tell
	 * by checking the Start-Of-Frame flag.
	 */
	if (test_bit(CF_FRAME_SOF0, &mcam_dev->flags))
		goto handle_eof;
	else
		goto handle_sof;

handle_sof:
	if (irqs & IRQ_SOF0) {
		set_bit(CF_FRAME_SOF0, &mcam_dev->flags);
		irqs &= ~IRQ_SOF0;
		if (!test_bit(CF_FRAME_OVERFLOW, &mcam_dev->flags)) {
			msc2_setup_buffer(mcam_dev);
			dma_dev->ops->shadow_ready(dma_dev);
		}
		goto handle_eof;
	}
	/* SHADOW_NOT_READY is mutual exclusive with SOF */
	if (irqs & IRQ_SHADOW_NOT_RDY) {
		clear_bit(CF_FRAME_SOF0, &mcam_dev->flags);
		if (test_bit(CF_FRAME_OVERFLOW, &mcam_dev->flags) &&
			(mcam_dev->buffer_mode == B_DMA_SG)) {
			/* If overflow happens, reset MMU here */
			u32 i, tid[mcam_dev->mbuf->vb2_buf.num_planes];
			for (i = 0; i < mcam_dev->mbuf->vb2_buf.num_planes;
				 i++)
				tid[i] = mcam_dev->mbuf->ch_info[i].tid;
			mcam_dev->sc2_mmu->ops->reset_ch(mcam_dev->sc2_mmu, tid, i);
			WARN_ON(mcam_dev->sc2_mmu->ops->enable_ch(
				mcam_dev->sc2_mmu, tid, i));
			clear_bit(CF_FRAME_OVERFLOW, &mcam_dev->flags);
		} else
			msc2_setup_buffer(mcam_dev);
		dma_dev->ops->shadow_ready(dma_dev);
	}

handle_eof:
	if (irqs & IRQ_EOF0) {
		clear_bit(CF_FRAME_SOF0, &mcam_dev->flags);
		irqs &= ~IRQ_EOF0;
		if (!test_bit(CF_FRAME_OVERFLOW, &mcam_dev->flags))
			mccic_frame_complete(mcam_dev);
		else 
			clear_bit(CF_FRAME_OVERFLOW, &mcam_dev->flags);
	}

	if (irqs & IRQ_EOF0)
		goto handle_eof;
	if (irqs & IRQ_SOF0)
		goto handle_sof;
	spin_unlock(&mcam_dev->mcam_lock);

	if (irqs)
		return IRQ_NONE;
	else
		return IRQ_HANDLED;
}

static irqreturn_t ctrl_irq_handler(struct ccic_ctrl_dev *ctrl_dev, u32 irqs)
{
	return IRQ_NONE;
}

static int mv_camera_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct mv_camera_dev *mcam_dev;
	int ret;

	/* 1. get id, camera_id (dma_id/ctrl_id) */
	ret = of_alias_get_id(np, "mv_sc2_camera");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get alias id, errno %d\n", ret);
		return ret;
	}
	pdev->id = ret;

	ret = of_property_read_u32(np, "lpm-qos", &sc2_camera_req_qos);
	if (ret)
		return ret;
		if(!sc2_camera_qos_idle.name) {
	sc2_camera_qos_idle.name = MV_SC2_CAMERA_NAME;
	pm_qos_add_request(&sc2_camera_qos_idle, PM_QOS_CPUIDLE_BLOCK,
			PM_QOS_CPUIDLE_BLOCK_DEFAULT_VALUE);
	}
	ret = of_property_read_u32(np, "ddr-qos", &sc2_camera_req_ddr_qos);
	if (ret)
		return ret;
	if(!sc2_camera_qos_ddr.name) {
		sc2_camera_qos_ddr.name = MV_SC2_CAMERA_NAME;
		pm_qos_add_request(&sc2_camera_qos_ddr,
				PM_QOS_DDR_DEVFREQ_MIN,
				PM_QOS_DEFAULT_VALUE);
	}
	if(!gc2dfreq_qos_req_min.name) {
		gc2dfreq_qos_req_min.name = MV_SC2_CAMERA_NAME;
		pm_qos_add_request(&gc2dfreq_qos_req_min,
				PM_QOS_GPUFREQ_2D_MIN,
				156000);
	}
	/* 2. alloc mcam_dev and setup */
	mcam_dev = devm_kzalloc(&pdev->dev, sizeof(struct mv_camera_dev),
			GFP_KERNEL);
	if (!mcam_dev) {
		dev_err(&pdev->dev, "Could not allocate mcam dev\n");
		return -ENOMEM;
	}
	mcam_dev->id = pdev->id;
	mcam_dev->pdev = pdev;
	mcam_dev->dev = &pdev->dev;
	mcam_dev->buffer_mode = B_DMA_SG;
	/*
	 *mcam_dev->mclk = devm_clk_get(&pdev->dev, "SC2MCLK");
	 *if (IS_ERR(mcam_dev->mclk))
	 *	return PTR_ERR(mcam_dev->mclk);
	 */
	pm_runtime_enable(mcam_dev->dev);
	mcam_dev->axi_clk = devm_clk_get(&pdev->dev, "SC2AXICLK");
	if (IS_ERR(mcam_dev->axi_clk))
		return PTR_ERR(mcam_dev->axi_clk);
	INIT_LIST_HEAD(&mcam_dev->buffers);
	spin_lock_init(&mcam_dev->mcam_lock);
	mutex_init(&mcam_dev->s_mutex);

	/* TBD mccic_init_clk(ccic_dev); */
	/* 3. setup soc_host */
	mcam_dev->soc_host.drv_name = MV_SC2_CAMERA_NAME;
	mcam_dev->soc_host.ops = &mcam_soc_camera_host_ops;
	mcam_dev->soc_host.priv = mcam_dev;
	mcam_dev->soc_host.v4l2_dev.dev = &pdev->dev;
	mcam_dev->soc_host.nr = pdev->id;

	ret = soc_camera_host_register(&mcam_dev->soc_host);

	return ret;
}

static int mv_camera_remove(struct platform_device *pdev)
{

	struct soc_camera_host *soc_host = to_soc_camera_host(&pdev->dev);
	struct mv_camera_dev *mcam_dev = soc_host->priv;
	int i;

	for (i = 0; i  < mcam_dev->mbus_fmt_num; i++)
		mcam_dev->mbus_fmt_code[i] = 0;
	mcam_dev->mbus_fmt_num = 0;

	pm_runtime_disable(mcam_dev->dev);
	soc_camera_host_unregister(soc_host);
	dev_info(&pdev->dev, "camera driver unloaded\n");

	return 0;
}

static const struct of_device_id mv_sc2_camera_dt_match[] = {
	{ .compatible = "marvell,mv_sc2_camera", .data = NULL },
	{},
};
MODULE_DEVICE_TABLE(of, mv_sc2_camera_dt_match);

static struct platform_driver mv_sc2_camera_driver = {
	.driver = {
		.name = MV_SC2_CAMERA_NAME,
		/* TBD */
		/* .pm = &msc2_ccic_pm, */
		.of_match_table = of_match_ptr(mv_sc2_camera_dt_match),
	},
	.probe = mv_camera_probe,
	.remove = mv_camera_remove,
};

module_platform_driver(mv_sc2_camera_driver);
