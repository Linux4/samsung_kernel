// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Copyright (c) 2023 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/videodev2_exynos_media.h>

#include "is-device-sensor.h"
#include "is-hw.h"
#include "is-video.h"
#include "pablo-icpu-itf.h"
#include "pablo-blob.h"

/*
 * copy from 'include/media/v4l2-ioctl.h'
 * #define V4L2_DEV_DEBUG_IOCTL		0x01
 * #define V4L2_DEV_DEBUG_IOCTL_ARG	0x02
 * #define V4L2_DEV_DEBUG_FOP		0x04
 * #define V4L2_DEV_DEBUG_STREAMING	0x08
 * #define V4L2_DEV_DEBUG_POLL		0x10
 */
#define V4L2_DEV_DEBUG_DMA	0x100

#define DDD_CTRL_IMAGE		0x01
#define DDD_CTRL_META		0x02
#define DDD_CTRL_TYPE_MASK

#define DDD_TYPE_PERIOD		0 /* all frame count matching period	 */
#define DDD_TYPE_INTERVAL	1 /* from any frame count to greater one */
#define DDD_TYPE_ONESHOT	2 /* specific frame count only		 */

static unsigned int dbg_dma_dump_ctrl;
static unsigned int dbg_dma_dump_type = DDD_TYPE_PERIOD;
static unsigned int dbg_dma_dump_arg1 = 30;	/* every frame(s) */
static unsigned int dbg_dma_dump_arg2;		/* for interval type */

static int param_get_dbg_dma_dump_ctrl(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "DMA dump control: ");

	if (!dbg_dma_dump_ctrl) {
		ret += sprintf(buffer + ret, "None\n");
		ret += sprintf(buffer + ret, "\t- image(0x1)\n");
		ret += sprintf(buffer + ret, "\t- meta (0x2)\n");
	} else {
		if (dbg_dma_dump_ctrl & DDD_CTRL_IMAGE)
			ret += sprintf(buffer + ret, "dump image(0x1) | ");
		if (dbg_dma_dump_ctrl & DDD_CTRL_META)
			ret += sprintf(buffer + ret, "dump meta (0x2) | ");

		ret -= 3;
		ret += sprintf(buffer + ret, "\n");
	}

	return ret;
}

static const struct kernel_param_ops param_ops_dbg_dma_dump_ctrl = {
	.set = param_set_uint,
	.get = param_get_dbg_dma_dump_ctrl,
};

static int param_get_dbg_dma_dump_type(char *buffer, const struct kernel_param *kp)
{
	int ret;

	ret = sprintf(buffer, "DMA dump type: selected(*)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_PERIOD ?
				"\t- period(0)*\n" : "\t- period(0)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_INTERVAL ?
				"\t- interval(1)*\n" : "\t- interval(1)\n");
	ret += sprintf(buffer + ret, dbg_dma_dump_type == DDD_TYPE_ONESHOT ?
				"\t- oneshot(2)*\n" : "\t- oneshot(2)\n");

	return ret;
}

static const struct kernel_param_ops param_ops_dbg_dma_dump_type = {
	.set = param_set_uint,
	.get = param_get_dbg_dma_dump_type,
};

module_param_cb(dma_dump_ctrl, &param_ops_dbg_dma_dump_ctrl,
			&dbg_dma_dump_ctrl, S_IRUGO | S_IWUSR);
module_param_cb(dma_dump_type, &param_ops_dbg_dma_dump_type,
			&dbg_dma_dump_type, S_IRUGO | S_IWUSR);
module_param_named(dma_dump_arg1, dbg_dma_dump_arg1, uint,
			S_IRUGO | S_IWUSR);
module_param_named(dma_dump_arg2, dbg_dma_dump_arg2, uint,
			S_IRUGO | S_IWUSR);

static u32 get_instance_video_ctx(struct is_video_ctx *ivc)
{
	struct is_video *iv = ivc->video;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)ivc->device;
		return idi->instance;
	} else {
		ids = (struct is_device_sensor *)ivc->device;
		return ids->instance;
	}
}

static void mdbgv_video(struct is_video_ctx *ivc, struct is_video *iv,
							const char *msg)
{
	if (iv->device_type == IS_DEVICE_ISCHAIN)
		mdbgv_ischain("%s\n", ivc, msg);
	else
		mdbgv_sensor("%s\n", ivc, msg);
}

int is_queue_setup(struct vb2_queue *vq,
		unsigned *nbuffers, unsigned *nplanes,
		unsigned sizes[], struct device *alloc_devs[])
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vq->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = ivc->queue;
	struct is_frame_cfg *ifc = &iq->framecfg;
	struct is_fmt *fmt = ifc->format;
	int p;
	struct is_mem *mem;

	*nplanes = (unsigned int)fmt->num_planes;

	if (fmt->setup_plane_sz) {
		fmt->setup_plane_sz(ifc, sizes);
	} else {
		err("failed to setup plane sizes for pixelformat(%c%c%c%c)",
			(char)((fmt->pixelformat >> 0) & 0xFF),
			(char)((fmt->pixelformat >> 8) & 0xFF),
			(char)((fmt->pixelformat >> 16) & 0xFF),
			(char)((fmt->pixelformat >> 24) & 0xFF));

			return -EINVAL;
	}

	mem = is_hw_get_iommu_mem(iv->id);

	for (p = 0; p < *nplanes; p++) {
		alloc_devs[p] = mem->dev;
		ifc->size[p] = sizes[p];
		mdbgv_vid("queue[%d] size : %d\n", p, sizes[p]);
	}

	mdbgv_video(ivc, iv, __func__);

	return 0;
}

void is_wait_prepare(struct vb2_queue *vbq)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vbq->drv_priv;
	struct is_video *iv = ivc->video;

	mutex_unlock(&iv->lock);
}

void is_wait_finish(struct vb2_queue *vbq)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vbq->drv_priv;
	struct is_video *iv = ivc->video;

	mutex_lock(&iv->lock);
}

int is_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_group *ig = ivc->group;
	struct is_queue *iq = ivc->queue;
	struct is_mem *mem;
	u32 instance = get_instance_video_ctx(ivc);
	int ret;

	mvdbgs(3, "%s(%d)\n", ivc, iq, __func__, vb->index);

	/* for each leader */
	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		ret = CALL_GROUP_OPS(ig, buffer_init, vb->index);
		if (ret)
			mierr("failure in pablo_group_buffer_init(): %d", instance, ret);
	}

	mem = is_hw_get_iommu_mem(iv->id);
	vbuf->ops = mem->is_vb2_buf_ops;
	vbuf->dbuf_ops = mem->dbuf_ops;
	vbuf->dbuf_con_ops = mem->dbuf_con_ops;

	return 0;
}

static int __update_subbuf(struct camera2_node *node, struct is_vb2_buf *vbuf, struct is_sub_dma_buf *sdbuf)
{
	int ret;
	struct is_mem *mem;
	u32 num_i_planes;
	struct v4l2_plane planes[IS_MAX_PLANES];

	mem = is_hw_get_iommu_mem(node->vid);
	node->result = 0;
	num_i_planes = node->buf.length - NUM_OF_META_PLANE;

	sdbuf->vid = node->vid;
	sdbuf->num_plane = num_i_planes;
	sdbuf->dbuf_ops = mem->dbuf_ops;
	sdbuf->dbuf_con_ops = mem->dbuf_con_ops;

	if (copy_from_user(planes, node->buf.m.planes,
				sizeof(struct v4l2_plane) * num_i_planes) != 0) {
		err("Failed copy_from_user");
		return -EINVAL;
	}

	vbuf->ops->subbuf_prepare(sdbuf, planes, mem->dev);

	ret = vbuf->ops->subbuf_dvmap(sdbuf);
	if (ret) {
		err("Failed to get dva");
		return ret;
	}

	return 0;
}

static void __release_subbuf(struct is_vb2_buf *vbuf, struct is_sub_dma_buf *sdbuf)
{
	pablo_blob_lvn_dump(is_debug_get()->blob_lvn, sdbuf);

	if (sdbuf->kva[0])
		vbuf->ops->subbuf_kunmap(sdbuf);

	vbuf->ops->subbuf_finish(sdbuf);
	sdbuf->vid = 0;
}

static int _is_queue_subbuf_prepare(struct device *dev,
		struct is_vb2_buf *vbuf,
		struct camera2_node_group *node_group,
		bool need_vmap)
{
	int ret;
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct is_video *video = GET_VIDEO(vctx);
	struct is_queue *queue = GET_QUEUE(vctx);
	struct camera2_node *node;
	struct is_sub_buf *sbuf;
	struct is_sub_dma_buf *sdbuf;
	u32 index = vb->index;
	u32 n;
	unsigned int dbg_draw_digit_ctrl;

	mdbgv_lvn(3, "[%s][I%d]subbuf_prepare\n", vctx, video,
			queue->name, index);

	/* Extract input subbuf */
	node = &node_group->leader;
	if (!node->request)
		return 0;
	if (node->vid >= IS_VIDEO_MAX_NUM) {
		mverr("[%s][I%d]subbuf_prepare: invalid input (req:%d, vid:%d, length:%d)\n",
			vctx, video,
			queue->name, index,
			node->request, node->vid, node->buf.length);
	}

	node->result = 0;

	dbg_draw_digit_ctrl = is_get_debug_param(IS_DEBUG_PARAM_DRAW_DIGIT);
	/* Disable SBWC for drawing digit on it */
	if (dbg_draw_digit_ctrl)
		node->flags &= ~PIXEL_TYPE_EXTRA_MASK;

#if IS_ENABLED(ENABLE_RECURSIVE_NR)
	/* leader node */
	if (node->request && node->buf.length) {
		sbuf = &queue->out_buf[index];
		sbuf->ldr_vid = video->id;

		sdbuf = &sbuf->leader;

		ret = __update_subbuf(node, vbuf, sdbuf);
		if (ret) {
			mverr("[%s][%s][I%d]Failed to __update_subbuf", vctx, video,
					queue->name, vn_name[node->vid], index);
			return ret;
		}

	}
#endif

	/* Extract output subbuf */
	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		node = &node_group->capture[n];
		if (!node->request)
			continue;

		if (node->buf.length > IS_MAX_PLANES ||
			node->vid >= IS_VIDEO_MAX_NUM) {
			mverr("[%s][I%d]subbuf_prepare: invalid output[%d] (req:%d, vid:%d, length:%d) input (req:%d, vid:%d)\n",
				vctx, video,
				queue->name, index,
				n,
				node->request, node->vid, node->buf.length,
				node_group->leader.request, node_group->leader.vid);
			/*
			 * FIXME :
			 * For normal error handling, it would be nice to return error to user
			 * but condition of system need to be preserved because of some issues
			 * So, temporarily BUG() is used here.
			 */
			BUG();
		} else if (!node->buf.length) {
			mdbgv_lvn(2, "[%s][%s][I%d]subbuf_prepare: Invalid buf.length %d",
					vctx, video, queue->name, vn_name[node->vid],
					index, node->buf.length);
			continue;
		}

		sbuf = &queue->out_buf[index];
		sbuf->ldr_vid = video->id;

		sdbuf = &sbuf->sub[n];

		ret = __update_subbuf(node, vbuf, sdbuf);
		if (ret) {
			mverr("[%s][%s][I%d]Failed to __update_subbuf", vctx, video,
					queue->name, vn_name[node->vid], index);
			return ret;
		}

		if (need_vmap || CHECK_NEED_KVADDR_LVN_ID(sdbuf->vid)) {
			ret = vbuf->ops->subbuf_kvmap(sdbuf);
			if (ret) {
				mverr("[%s][%s][I%d]Failed to get dva", vctx, video,
						queue->name,
						vn_name[node->vid],
						index);

				return ret;
			}

			/* Disable SBWC for drawing digit on it */
			if (dbg_draw_digit_ctrl)
				node->flags &= ~PIXEL_TYPE_EXTRA_MASK;
		}
	}

	return 0;
}

int is_buf_prepare(struct vb2_buffer *vb)
{
	int ret;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct is_queue *queue = GET_QUEUE(vctx);
	struct is_video *video = GET_VIDEO(vctx);
	unsigned int index = vb->index;
	unsigned int dbg_draw_digit_ctrl;
	struct is_frame *frame = &queue->framemgr.frames[index];
	u32 num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	u32 num_buffers = 1, pos_meta_p, i;
	ulong kva_meta;
	dma_addr_t dva_meta = 0;
	bool need_vmap;
	struct is_mem *mem = is_hw_get_iommu_mem(video->id);

	dbg_draw_digit_ctrl = is_get_debug_param(IS_DEBUG_PARAM_DRAW_DIGIT);
	/* take a snapshot whether it is needed or not */
	need_vmap = (dbg_draw_digit_ctrl ||
			((dbg_dma_dump_ctrl & ~DDD_CTRL_META)
			 && (video->vd.dev_debug & V4L2_DEV_DEBUG_DMA))
		    );

	/* Unmerged dma_buf_container */
	if (IS_ENABLED(DMABUF_CONTAINER)) {
		ret = vbuf->ops->dbufcon_prepare(vbuf, mem->dev);
		if (ret) {
			mverr("failed to prepare dmabuf-container: %d",
					vctx, video, index);
			return ret;
		}

		if (vbuf->num_merged_dbufs) {
			ret = vbuf->ops->dbufcon_map(vbuf);
			if (ret) {
				mverr("failed to map dmabuf-container: %d",
						vctx, video, index);
				vbuf->ops->dbufcon_finish(vbuf);
				return ret;
			}

			num_buffers = vbuf->num_merged_dbufs;
		}
	}

	/* Get kva of image planes */
	if (test_bit(IS_QUEUE_NEED_TO_KMAP, &queue->state)) {
		for (i = 0; i < num_i_planes; i++)
			vbuf->ops->plane_kmap(vbuf, i, 0);
	} else if (need_vmap || CHECK_NEED_KVADDR_ID(video->id)) {
		if (vbuf->num_merged_dbufs) {
			for (i = 0; i < num_i_planes; i++)
				vbuf->ops->dbufcon_kmap(vbuf, i);
		} else {
			for (i = 0; i < num_i_planes; i++)
				vbuf->kva[i] = vbuf->ops->plane_kvaddr(vbuf, i);
		}
	}

	/* Disable SBWC for drawing digit on it */
	if (dbg_draw_digit_ctrl)
		queue->framecfg.hw_pixeltype &= ~PIXEL_TYPE_EXTRA_MASK;

	/* Get metadata plane */
	pos_meta_p = num_i_planes * num_buffers;
	kva_meta = vbuf->ops->plane_kmap(vbuf, num_i_planes, 0);
	if (!kva_meta) {
		mverr("[%s][I%d][P%d]Failed to get kva", vctx, video,
				queue->name, index, num_i_planes);
		return -ENOMEM;
	}

	queue->buf_kva[index][pos_meta_p] = kva_meta;

	if (IS_ENABLED(CONFIG_PABLO_ICPU)) {
		dva_meta = vbuf->ops->plane_dmap(vbuf, num_i_planes, 0, pablo_itf_get_icpu_dev());
		if (!dva_meta) {
			mverr("[%s][I%d][P%d]Failed to get dva", vctx, video,
					queue->name, index, num_i_planes);
			return -ENOMEM;
		}
	}

	/* Setup frame */
	frame->num_buffers = num_buffers;
	frame->planes = num_i_planes * num_buffers;
	frame->vbuf = vbuf;
	kva_meta = queue->buf_kva[index][pos_meta_p];
	if (video->video_type == IS_VIDEO_TYPE_LEADER) {
		/* Output node */
		frame->shot_ext = (struct camera2_shot_ext *)kva_meta;
		frame->shot = &frame->shot_ext->shot;
		frame->shot_dva = dva_meta + offsetof(struct camera2_shot_ext, shot);
		frame->shot_size = sizeof(frame->shot);

		if (frame->shot->magicNumber != SHOT_MAGIC_NUMBER) {
			mverr("[%s][I%d]Shot magic number error! 0x%08X size %zd",
					vctx, video,
					queue->name, index,
					frame->shot->magicNumber,
					sizeof(struct camera2_shot_ext));
			return -EINVAL;
		}
	} else {
		/* Capture node */
		frame->stream = (struct camera2_stream *)kva_meta;

		/* TODO : Change type of address into ulong */
		frame->stream->address = (u32)kva_meta;
	}

	if (IS_ENABLED(LOGICAL_VIDEO_NODE)) {
		queue->mode = CAMERA_NODE_LOGICAL;

		if (video->video_type == IS_VIDEO_TYPE_LEADER
				&& queue->mode == CAMERA_NODE_LOGICAL) {
			ret = _is_queue_subbuf_prepare(mem->dev, vbuf,
					&frame->shot_ext->node_group, need_vmap);
			if (ret) {
				mverr("[%s][I%d]Failed to subbuf_prepare",
						vctx, video, queue->name, index);
				return ret;
			}
		}
	}

	if (test_bit(IS_QUEUE_NEED_TO_REMAP, &queue->state)) {
		ret = vbuf->ops->remap_attr(vbuf, 0);
		if (ret) {
			mverr("failed to remap dmabuf: %d", vctx, video, index);
			return ret;
		}
	}

	queue->buf_pre++;

	return 0;
}

static void is_queue_subbuf_draw_digit(struct is_queue *queue,
		struct is_frame *frame)
{
	struct is_debug_dma_info dinfo;
	struct is_sub_node *snode = &frame->cap_node;
	struct is_sub_dma_buf *sdbuf;
	struct camera2_node *node;
	struct is_fmt *fmt;
	u32 n, b;

	/* Only handle 1st plane */
	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		node = &frame->shot_ext->node_group.capture[n];
		if (!node->request)
			continue;

		sdbuf = &queue->out_buf[frame->index].sub[n];
		fmt = is_find_format(node->pixelformat, node->flags);
		if (!fmt) {
			warn("[%s][I%d][F%d] pixelformat(%c%c%c%c) is not found",
				queue->name,
				frame->index,
				frame->fcount,
				(char)((node->pixelformat >> 0) & 0xFF),
				(char)((node->pixelformat >> 8) & 0xFF),
				(char)((node->pixelformat >> 16) & 0xFF),
				(char)((node->pixelformat >> 24) & 0xFF));

			continue;
		}

		dinfo.width = node->output.cropRegion[2];
		dinfo.height = node->output.cropRegion[3];
		dinfo.pixeltype = node->flags;
		dinfo.bpp = fmt->bitsperpixel[0];
		dinfo.pixelformat = fmt->pixelformat;

		for (b = 0; b < sdbuf->num_buffer; b++) {
			dinfo.addr = snode->sframe[n].kva[b * sdbuf->num_plane];

			if (dinfo.addr)
				is_dbg_draw_digit(&dinfo, frame->fcount);
		}
	}
}

static void is_queue_draw_digit(struct is_queue *queue, struct is_vb2_buf *vbuf)
{
	struct is_debug_dma_info dinfo;
	struct is_video_ctx *vctx = queue->vbq->drv_priv;
	struct is_video *video = GET_VIDEO(vctx);
	struct is_frame_cfg *framecfg = &queue->framecfg;
	struct is_frame *frame;
	u32 index = vbuf->vb.vb2_buf.index;
	u32 num_buffer = vbuf->num_merged_dbufs ? vbuf->num_merged_dbufs : 1;
	u32 num_i_planes = vbuf->vb.vb2_buf.num_planes - NUM_OF_META_PLANE;
	u32 b;

	frame = &queue->framemgr.frames[index];

	/* Draw digit on capture node buffer */
	if (video->video_type == IS_VIDEO_TYPE_CAPTURE) {
		dinfo.width = frame->width ? frame->width : framecfg->width;
		dinfo.height = frame->height ? frame->height : framecfg->height;
		dinfo.pixeltype = framecfg->hw_pixeltype;
		dinfo.bpp = framecfg->format->bitsperpixel[0];
		dinfo.pixelformat = framecfg->format->pixelformat;

		for (b = 0; b < num_buffer; b++) {
			dinfo.addr = queue->buf_kva[index][b * num_i_planes];

			if (dinfo.addr)
				is_dbg_draw_digit(&dinfo, frame->fcount);
		}
	}

	/* Draw digit on LVN sub node buffers */
	if (IS_ENABLED(LOGICAL_VIDEO_NODE) &&
			video->video_type == IS_VIDEO_TYPE_LEADER &&
			queue->mode == CAMERA_NODE_LOGICAL)
		is_queue_subbuf_draw_digit(queue, frame);
}

#define V4L2_BUF_FLAG_DISPOSAL	0x10000000
static void __is_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *ivb = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct vb2_queue *vbq = vb->vb2_queue;
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = ivc->queue;
	u32 framecount = iq->framemgr.frames[vb->index].fcount;
	bool ddd_trigger = false;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	int p;
	struct vb2_plane *vbp;
#if !defined(ENABLE_SKIP_PER_FRAME_MAP)
	unsigned int index = vb->index;
	struct is_sub_buf *sbuf;
	struct is_sub_dma_buf *sdbuf;
	u32 n;
#endif

	if (is_get_debug_param(IS_DEBUG_PARAM_DRAW_DIGIT))
		is_queue_draw_digit(iq, ivb);

	if (iv->vd.dev_debug & V4L2_DEV_DEBUG_DMA) {
		if (dbg_dma_dump_type == DDD_TYPE_PERIOD)
			ddd_trigger = !(framecount % dbg_dma_dump_arg1);
		else if (dbg_dma_dump_type == DDD_TYPE_INTERVAL)
			ddd_trigger = (framecount >= dbg_dma_dump_arg1)
				&& (framecount <= dbg_dma_dump_arg2);
		else if (dbg_dma_dump_type == DDD_TYPE_ONESHOT)
			ddd_trigger = (framecount == dbg_dma_dump_arg1);

		if (ddd_trigger && (dbg_dma_dump_ctrl & DDD_CTRL_IMAGE))
			is_dbg_dma_dump(iq, ivc->instance, vb->index,
					iv->id, DBG_DMA_DUMP_IMAGE);

		if (ddd_trigger && (dbg_dma_dump_ctrl & DDD_CTRL_META))
			is_dbg_dma_dump(iq, ivc->instance, vb->index,
					iv->id, DBG_DMA_DUMP_META);
	}

	if (IS_ENABLED(LOGICAL_VIDEO_NODE) &&
			iq->mode == CAMERA_NODE_LOGICAL) {
		mdbgv_lvn(3, "[%s][I%d]buf_finish\n",
				ivc, iv,
				vn_name[iv->id], index);
#ifndef ENABLE_SKIP_PER_FRAME_MAP
		sbuf = &iq->out_buf[index];

		/* release leader */
		sdbuf = &sbuf->leader;
		if (sdbuf->vid)
			__release_subbuf(ivb, sdbuf);

		/* release capture */
		for (n = 0; n < CAPTURE_NODE_MAX; n++) {
			sdbuf = &sbuf->sub[n];
			if (sdbuf->vid)
				__release_subbuf(ivb, sdbuf);
		}

		sbuf->ldr_vid = 0;
#endif
	}

	if (IS_ENABLED(DMABUF_CONTAINER) && (ivb->num_merged_dbufs)) {
		for (p = 0; p < num_i_planes && ivb->kva[p]; p++)
			ivb->ops->dbufcon_kunmap(ivb, p);

		ivb->ops->dbufcon_unmap(ivb);
		ivb->ops->dbufcon_finish(ivb);
	}

	if (IS_ENABLED(CONFIG_PABLO_ICPU))
		ivb->ops->plane_dunmap(ivb, num_i_planes, 0);

	if (test_bit(IS_QUEUE_NEED_TO_REMAP, &iq->state))
		ivb->ops->unremap_attr(ivb, 0);

	if ((vb2_v4l2_buf->flags & V4L2_BUF_FLAG_DISPOSAL) &&
			vbq->memory == VB2_MEMORY_DMABUF) {
		is_buf_cleanup(vb);

		for (p = 0; p < vb->num_planes; p++) {
			vbp = &vb->planes[p];

			if (!vbp->mem_priv)
				continue;

			if (vbp->dbuf_mapped)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
				vbq->mem_ops->unmap_dmabuf(vbp->mem_priv);
#else
				vbq->mem_ops->unmap_dmabuf(vbp->mem_priv, 0);
#endif

			vbq->mem_ops->detach_dmabuf(vbp->mem_priv);
			dma_buf_put(vbp->dbuf);
			vbp->mem_priv = NULL;
			vbp->dbuf = NULL;
			vbp->dbuf_mapped = 0;
		}
	}
}

void is_buf_finish(struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	int ret;

	ret = call_pv_op(ivc, buf_finish, vb);
	if (ret)
		err("call_pv_op - %s: %d", "buf_finish", ret);

	__is_buf_finish(vb);
}

void is_buf_cleanup(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	struct is_video_ctx *vctx = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	int i;

	/* FIXME: doesn't support dmabuf container yet */
	if (test_bit(IS_QUEUE_NEED_TO_KMAP, &vctx->queue->state)) {
		for (i = 0; i < vb->num_planes; i++)
			vbuf->ops->plane_kunmap(vbuf, i, 0);
	} else {
		vbuf->ops->plane_kunmap(vbuf, num_i_planes, 0);
	}
}

int is_start_streaming(struct vb2_queue *vbq, unsigned int count)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vbq->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = ivc->queue;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	if (test_bit(IS_QUEUE_STREAM_ON, &iq->state)) {
		err("[%s] already streaming", iq->name);
		return -EINVAL;
	}

	ret = CALL_QOPS(iq, start_streaming, ivc->device, iq);
	if (ret) {
		err("[%s] failed to start_streaming for device: %d", iq->name, ret);
		return ret;
	}

	set_bit(IS_QUEUE_STREAM_ON, &iq->state);

	return 0;
}

void is_stop_streaming(struct vb2_queue *vbq)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vbq->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_queue *iq = ivc->queue;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	/*
	 * If you prepare or queue buffers, and then call streamoff without
	 * ever having called streamon, you would still expect those buffers
	 * to be returned to their normal dequeued state.
	 */
	if (!test_bit(IS_QUEUE_STREAM_ON, &iq->state))
		warn("[%s] streaming inactive", iq->name);

	ret = CALL_QOPS(iq, stop_streaming, ivc->device, iq);
	if (ret)
		err("[%s] failed to stop_streaming for device: %d", iq->name, ret);

	clear_bit(IS_QUEUE_STREAM_ON, &iq->state);
	clear_bit(IS_QUEUE_BUFFER_PREPARED, &iq->state);
}

static int _is_queue_buffer_tag(dma_addr_t saddr[], dma_addr_t taddr[],
	u32 pixelformat, u32 width, u32 height, u32 planes, u32 *hw_planes)
{
	int i, j;
	int ret = 0;

	*hw_planes = planes;
	switch (pixelformat) {
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		for (i = 0; i < planes; i++) {
			j = i * 2;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
		}
		*hw_planes = planes * 2;
		break;
#if PKV_VER_GE(6, 0, 0)
	case V4L2_PIX_FMT_P010:
		for (i = 0; i < planes; i++) {
			j = i * 2;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * 2) * height;
		}
		*hw_planes = planes * 2;
		break;
#endif
	case V4L2_PIX_FMT_YVU420M:
		for (i = 0; i < planes; i += 3) {
			taddr[i] = saddr[i];
			taddr[i + 1] = saddr[i + 2];
			taddr[i + 2] = saddr[i + 1];
		}
		break;
	case V4L2_PIX_FMT_YUV420:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
			taddr[j + 2] = taddr[j + 1] + (width * height / 4);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_YVU420: /* AYV12 spec: The width should be aligned by 16 pixel. */
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 2] = taddr[j] + (ALIGN(width, 16) * height);
			taddr[j + 1] = taddr[j + 2] + (ALIGN(width / 2, 16) * height / 2);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_YUV422P:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
			taddr[j + 2] = taddr[j + 1] + (width * height / 2);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_NV12M_S10B:
	case V4L2_PIX_FMT_NV21M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			taddr[j] = saddr[i];
			taddr[j + 1] = saddr[i + 1];
			taddr[j + 2] = taddr[j] + NV12M_Y_SIZE(width, height);
			taddr[j + 3] = taddr[j + 1] + NV12M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_NV16M_S10B:
	case V4L2_PIX_FMT_NV61M_S10B:
		for (i = 0; i < planes; i += 2) {
			j = i * 2;
			/* Y_ADDR, UV_ADDR, Y_2BIT_ADDR, UV_2BIT_ADDR */
			taddr[j] = saddr[i];
			taddr[j + 1] = saddr[i + 1];
			taddr[j + 2] = taddr[j] + NV16M_Y_SIZE(width, height);
			taddr[j + 3] = taddr[j + 1] + NV16M_CBCR_SIZE(width, height);
		}
		break;
	case V4L2_PIX_FMT_RGB24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j + 2] = saddr[i];
			taddr[j + 1] = taddr[j + 2] + (width * height);
			taddr[j] = taddr[j + 1] + (width * height);
		}
		*hw_planes = planes * 3;
		break;
	case V4L2_PIX_FMT_BGR24:
		for (i = 0; i < planes; i++) {
			j = i * 3;
			taddr[j] = saddr[i];
			taddr[j + 1] = taddr[j] + (width * height);
			taddr[j + 2] = taddr[j + 1] + (width * height);
		}
		*hw_planes = planes * 3;
		break;
	default:
		for (i = 0; i < planes; i++)
			taddr[i] = saddr[i];
		break;
	}

	return ret;
}

static int _is_queue_subbuf_queue(struct is_video_ctx *vctx,
		struct is_queue *queue,
		struct is_frame *frame)
{
	struct is_video *video = GET_VIDEO(vctx);
	struct camera2_node *node = NULL;
	struct is_sub_node * snode = NULL;
	struct is_sub_dma_buf *sdbuf;
	struct is_crop *crop;
	u32 index = frame->index;
	u32 n, stride_w, stride_h, hw_planes = 0;

	/* Extract output subbuf information */
	snode = &frame->cap_node;
	for (n = 0; n < CAPTURE_NODE_MAX; n++) {
		/* clear first */
		snode->sframe[n].id = 0;

		if (snode->sframe[n].kva[0])
			memset(snode->sframe[n].kva, 0x0,
				sizeof(ulong) * snode->sframe[n].num_planes);

		node = &frame->shot_ext->node_group.capture[n];
		if (!node->request)
			continue;

		sdbuf = &queue->out_buf[index].sub[n];

		/* Setup output subframe */
		snode->sframe[n].id = sdbuf->vid;
		snode->sframe[n].num_planes
			= sdbuf->num_plane * sdbuf->num_buffer;
		crop = (struct is_crop *)node->output.cropRegion;
		stride_w = max(node->width, crop->w);
		stride_h = max(node->height, crop->h);

		_is_queue_buffer_tag(sdbuf->dva,
				snode->sframe[n].dva,
				node->pixelformat,
				stride_w, stride_h,
				snode->sframe[n].num_planes,
				&hw_planes);

		snode->sframe[n].num_planes = hw_planes;

		if (sdbuf->kva[0]) {
			mdbgv_lvn(2, "[%s][%s] 0x%lx\n", vctx, video,
				queue->name, vn_name[node->vid], sdbuf->kva[0]);

			memcpy(snode->sframe[n].kva, sdbuf->kva,
				sizeof(ulong) * snode->sframe[n].num_planes);
		}

		mdbgv_lvn(4, "[%s][%s][N%d][I%d] pixelformat %c%c%c%c size %dx%d(%dx%d) length %d\n",
			vctx, video,
			queue->name, vn_name[node->vid],
			n, index,
			(char)((node->pixelformat >> 0) & 0xFF),
			(char)((node->pixelformat >> 8) & 0xFF),
			(char)((node->pixelformat >> 16) & 0xFF),
			(char)((node->pixelformat >> 24) & 0xFF),
			crop->w, crop->h,
			stride_w, stride_h,
			node->buf.length);
	}

	return 0;
}

#if IS_ENABLED(ENABLE_RECURSIVE_NR)
static inline void __update_iq_dva_2nr(struct is_queue *iq, struct is_frame *frame, unsigned int index)
{
	struct is_sub_buf *sbuf = &iq->out_buf[index];
	struct is_sub_dma_buf *sdbuf = &sbuf->leader;
	int i;

	frame->planes = sdbuf->num_plane;
	for (i = 0; i < frame->planes; i++)
		iq->buf_dva[index][i] = sdbuf->dva[i];
}
#endif

static int __is_buf_queue(struct is_queue *iq, struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = iq->vbq->drv_priv;
	struct is_video *iv = ivc->video;
	struct is_framemgr *framemgr = &iq->framemgr;
	struct vb2_v4l2_buffer *vb2_v4l2_buf = to_vb2_v4l2_buffer(vb);
	struct is_vb2_buf *vbuf = vb_to_is_vb2_buf(vb2_v4l2_buf);
	unsigned int index = vb->index;
	unsigned int num_i_planes = vb->num_planes - NUM_OF_META_PLANE;
	struct is_frame *frame;
	int i;
	int ret = 0;

	/* image planes */
	if (IS_ENABLED(DMABUF_CONTAINER) && vbuf->num_merged_dbufs) {
		/* vbuf has been sorted by the order of buffer */
		memcpy(iq->buf_dva[index], vbuf->dva,
			sizeof(dma_addr_t) * vbuf->num_merged_dbufs * num_i_planes);

		if (vbuf->kva[0])
			memcpy(iq->buf_kva[index], vbuf->kva,
				sizeof(ulong) * vbuf->num_merged_dbufs * num_i_planes);
		else
			memset(iq->buf_kva[index], 0x0,
				sizeof(ulong) * vbuf->num_merged_dbufs * num_i_planes);
	} else {
		for (i = 0; i < num_i_planes; i++) {
			if (test_bit(IS_QUEUE_NEED_TO_REMAP, &iq->state))
				iq->buf_dva[index][i] = vbuf->dva[i];
			else
				iq->buf_dva[index][i] = vbuf->ops->plane_dvaddr(vbuf, i);
		}

		if (vbuf->kva[0])
			memcpy(iq->buf_kva[index], vbuf->kva,
				sizeof(ulong) * num_i_planes);
		else
			memset(iq->buf_kva[index], 0x0,
				sizeof(ulong) * num_i_planes);
	}

	frame = &framemgr->frames[index];

	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
#ifdef MEASURE_TIME
		frame->tzone = (struct timespec64 *)frame->shot_ext->timeZone;
#endif

#if IS_ENABLED(ENABLE_RECURSIVE_NR)
		if (frame->shot_ext->node_group.leader.recursiveNrType ==
			NODE_RECURSIVE_NR_TYPE_2ND)
			__update_iq_dva_2nr(iq, frame, index);
#endif

		if (IS_ENABLED(LOGICAL_VIDEO_NODE)
				&& (iq->mode == CAMERA_NODE_LOGICAL)) {
			ret = _is_queue_subbuf_queue(ivc, iq, frame);
			if (ret) {
				mverr("[%s][I%d]Failed to subbuf_queue",
						ivc, iv, iq->name, index);
				goto err_logical_node;
			}
		}
	}

	/* uninitialized frame need to get info */
	if (!test_bit(FRAME_MEM_INIT, &frame->mem_state))
		goto set_info;

	/* plane address check */
	for (i = 0; i < frame->planes; i++) {
		if (frame->dvaddr_buffer[i] != iq->buf_dva[index][i])
			frame->dvaddr_buffer[i] = iq->buf_dva[index][i];

		if (frame->kvaddr_buffer[i] != iq->buf_kva[index][i])
			frame->kvaddr_buffer[i] = iq->buf_kva[index][i];
	}

	return 0;

set_info:
	if (test_bit(IS_QUEUE_BUFFER_PREPARED, &iq->state)) {
		mverr("already prepared but new index(%d) is came", ivc, iv, index);
		ret = -EINVAL;
		goto err_queue_prepared_already;
	}

	for (i = 0; i < frame->planes; i++) {
		frame->dvaddr_buffer[i] = iq->buf_dva[index][i];
		frame->kvaddr_buffer[i] = iq->buf_kva[index][i];
		frame->size[i] = iq->framecfg.size[i];
#ifdef PRINT_BUFADDR
		mvinfo("%s %d.%d %pad\n", ivc, iv, framemgr->name, index,
				i, &frame->dvaddr_buffer[i]);
#endif
	}

	set_bit(FRAME_MEM_INIT, &frame->mem_state);

	iq->buf_refcount++;

	if (iq->buf_maxcount == iq->buf_refcount) {
		if (IS_ENABLED(DMABUF_CONTAINER) && vbuf->num_merged_dbufs)
			mvinfo("%s number of merged buffers: %d\n",
					ivc, iv, iq->name, frame->num_buffers);
		set_bit(IS_QUEUE_BUFFER_PREPARED, &iq->state);
	}

	iq->buf_que++;

err_logical_node:
err_queue_prepared_already:
	return ret;
}

void is_buf_queue(struct vb2_buffer *vb)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)vb->vb2_queue->drv_priv;
	struct is_queue *iq = ivc->queue;
	struct pablo_device *pd = (struct pablo_device *)ivc->device;
	int ret;

	mvdbgs(3, "%s(%d)\n", ivc, iq, __func__, vb->index);

	ret = __is_buf_queue(iq, vb);
	if (ret) {
		mierr("failure in _is_buf_queue(): %d", pd->instance, ret);
		return;
	}

	ret = call_pv_op(ivc, buf_queue, vb);
	if (ret)
		err("call_pv_op - %s: %d", "buf_queue", ret);
}

const struct vb2_ops pablo_vb2_ops_default = {
	.queue_setup		= is_queue_setup,
	.wait_prepare		= is_wait_prepare,
	.wait_finish		= is_wait_finish,
	.buf_init		= is_buf_init,
	.buf_prepare		= is_buf_prepare,
	.buf_finish		= is_buf_finish,
	.buf_cleanup		= is_buf_cleanup,
	.start_streaming	= is_start_streaming,
	.stop_streaming		= is_stop_streaming,
	.buf_queue		= is_buf_queue,
};

const struct vb2_ops *pablo_get_vb2_ops_default(void)
{
	return &pablo_vb2_ops_default;
}
EXPORT_SYMBOL_GPL(pablo_get_vb2_ops_default);

MODULE_DESCRIPTION("vb2 module for Exynos Pablo");
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
