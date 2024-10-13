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
#include <videodev2_exynos_camera.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-dma-sg.h>

#include "is-video.h"
#include "is-device-sensor.h"
#include "is-device-ischain.h"
#include "is-devicemgr.h"
#include "is-common-config.h"
#include "is-vendor.h"
#include "is-core.h"
#include "pablo-icpu-itf.h"
#include "pablo-debug.h"
#include "pablo-interface-irta.h"
#include "pablo-v4l2.h"
#include "exynos-is.h"

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

static int is_queue_set_format_mplane(struct is_video_ctx *vctx,
	struct is_queue *queue,
	void *device,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 plane;
	struct v4l2_pix_format_mplane *pix;
	struct is_fmt *fmt;
	struct is_video *video;

	FIMC_BUG(!queue);

	video = GET_VIDEO(vctx);

	pix = &format->fmt.pix_mp;
	fmt = is_find_format(pix->pixelformat, pix->flags);
	if (!fmt) {
		mverr("[%s] pixel format is not found", vctx, video, queue->name);
		ret = -EINVAL;
		goto p_err;
	}

	queue->framecfg.format		= fmt;
	queue->framecfg.colorspace	= pix->colorspace;
	queue->framecfg.quantization	= pix->quantization;
	queue->framecfg.width		= pix->width;
	queue->framecfg.height		= pix->height;
	queue->framecfg.hw_pixeltype	= pix->flags;

	for (plane = 0; plane < fmt->hw_plane; ++plane)
		queue->framecfg.bytesperline[plane] = pix->plane_fmt[plane].bytesperline;

	ret = CALL_QOPS(queue, s_fmt, device, queue);
	if (ret) {
		mverr("[%s] s_fmt is fail(%d)", vctx, video, queue->name, ret);
		goto p_err;
	}

	mvinfo("[%s]pixelformat(%c%c%c%c) bit(%d) size(%dx%d) flag(0x%x)\n",
		vctx, video, queue->name,
		(char)((fmt->pixelformat >> 0) & 0xFF),
		(char)((fmt->pixelformat >> 8) & 0xFF),
		(char)((fmt->pixelformat >> 16) & 0xFF),
		(char)((fmt->pixelformat >> 24) & 0xFF),
		queue->framecfg.format->hw_bitwidth,
		queue->framecfg.width,
		queue->framecfg.height,
		queue->framecfg.hw_pixeltype);
p_err:
	return ret;
}

int is_vidioc_querycap(struct file *file, void *fh,
			struct v4l2_capability *cap)
{
	struct is_video *iv = video_drvdata(file);

	snprintf(cap->driver, sizeof(cap->driver), "%s", iv->vd.name);
	snprintf(cap->card, sizeof(cap->card), "%s", iv->vd.name);

	if (iv->video_type == IS_VIDEO_TYPE_LEADER)
		cap->capabilities |= V4L2_CAP_STREAMING
			| V4L2_CAP_VIDEO_OUTPUT
			| V4L2_CAP_VIDEO_OUTPUT_MPLANE;
	else
		cap->capabilities |= V4L2_CAP_STREAMING
			| V4L2_CAP_VIDEO_CAPTURE
			| V4L2_CAP_VIDEO_CAPTURE_MPLANE;

	cap->device_caps |= cap->capabilities;

	return 0;
}

int is_vidioc_g_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	return 0;
}

static int is_queue_open(struct is_queue *iq, struct is_video *iv)
{
	if (pablo_video_test_quirk(iv, PABLO_VIDEO_QUIRK_BIT_NEED_TO_KMAP))
		set_bit(IS_QUEUE_NEED_TO_KMAP, &iq->state);

	if (pablo_video_test_quirk(iv, PABLO_VIDEO_QUIRK_BIT_NEED_TO_REMAP)) {
		if (IS_ENABLED(SECURE_CAMERA_FACE) &&
		    IS_ENABLED(CONVERT_BUFFER_SECURE_TO_NON_SECURE) &&
		    iv->resourcemgr->scenario == IS_SCENARIO_SECURE)
			set_bit(IS_QUEUE_NEED_TO_REMAP, &iq->state);
	}

	frame_manager_probe(&iq->framemgr, iq->name);

	return 0;
}

static int queue_init(void *priv, struct vb2_queue *vbq,
	struct vb2_queue *vbq_dst)
{
	int ret = 0;
	struct is_video_ctx *vctx = priv;
	struct is_video *video;
	u32 type;

	FIMC_BUG(!vctx);
	FIMC_BUG(!GET_VIDEO(vctx));
	FIMC_BUG(!vbq);

	video = GET_VIDEO(vctx);

	if (video->video_type == IS_VIDEO_TYPE_CAPTURE)
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	vbq->type		= type;
	vbq->io_modes		= VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	vbq->drv_priv		= vctx;
	vbq->buf_struct_size	= (unsigned int)sizeof(struct is_vb2_buf);
	vbq->ops		= pablo_get_vb2_ops_default();
	vbq->mem_ops		= &vb2_dma_sg_memops;
	vbq->timestamp_flags	= V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(vbq);
	if (ret) {
		mverr("vb2_queue_init fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->queue->vbq = vbq;

p_err:
	return ret;
}

static int is_queue_alloc(struct is_video_ctx *ivc)
{
	int ret;
	struct is_video *iv = ivc->video;
	struct is_queue *queue;

	queue = kvzalloc(sizeof(struct is_queue), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(queue)) {
		mverr("alloc fail", ivc, iv);
		return -ENOMEM;
	}
	ivc->queue = queue;

	snprintf(queue->name, sizeof(queue->name), "%s", vn_name[iv->id]);

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		if (iv->video_type == IS_VIDEO_TYPE_LEADER)
			queue->qops = is_get_ischain_device_qops();
		else
			queue->qops = is_get_ischain_subdev_qops();
	} else {
		if (iv->video_type == IS_VIDEO_TYPE_LEADER)
			queue->qops = is_get_sensor_device_qops();
		else
			queue->qops = is_get_sensor_subdev_qops();
	}

	ret = is_queue_open(queue, iv);
	if (ret) {
		mverr("failure in is_queue_open(): %d", ivc, iv, ret);
		goto err;
	}

	queue->vbq = kvzalloc(sizeof(struct vb2_queue), GFP_KERNEL);
	if (ZERO_OR_NULL_PTR(queue->vbq)) {
		mverr("out of memory for vbq", ivc, iv);
		goto err;
	}

	ret = queue_init(ivc, queue->vbq, NULL);
	if (ret) {
		mverr("failure in queue_init(): %d", ivc, iv, ret);
		goto err;
	}

	return ret;

err:
	kvfree(queue->vbq);
	queue->vbq = NULL;

	kvfree(queue);
	ivc->queue = NULL;

	return ret;
}

int is_vidioc_s_fmt_mplane(struct file *file, void *fh,
			   struct v4l2_format *f)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	int ret;
	u32 condition;

	mdbgv_video(ivc, iv, __func__);

	/* capture video node can skip s_input */
	if (iv->video_type == IS_VIDEO_TYPE_LEADER)
		condition = BIT(IS_VIDEO_S_INPUT)
			  | BIT(IS_VIDEO_S_FORMAT)
			  | BIT(IS_VIDEO_S_BUFS);
	else
		condition = BIT(IS_VIDEO_S_INPUT)
			  | BIT(IS_VIDEO_S_BUFS)
			  | BIT(IS_VIDEO_S_FORMAT)
			  | BIT(IS_VIDEO_OPEN)
			  | BIT(IS_VIDEO_STOP);

	if (!(ivc->state & condition)) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	if (!ivc->queue) {
		ret = is_queue_alloc(ivc);
		if (ret) {
			mverr("is_queue_alloc fail", ivc, iv);
			return ret;
		}
	}

	ret = is_queue_set_format_mplane(ivc, ivc->queue, ivc->device, f);
	if (ret) {
		mverr("failure in is_queue_set_format_mplane(): %d", ivc, iv, ret);
		return ret;
	}

	ivc->state = BIT(IS_VIDEO_S_FORMAT);

	mdbgv_vid("set_format(%d x %d)\n", ivc->queue->framecfg.width,
						ivc->queue->framecfg.height);

	return 0;
}

int is_vidioc_reqbufs(struct file *file, void* fh,
		struct v4l2_requestbuffers *b)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_subdev *leader;
	struct is_queue *iq = ivc->queue;
	struct is_framemgr *framemgr;
	int ret;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)ivc->device;

		mdbgv_ischain("%s(buffer count: %d\n", ivc, __func__, b->count);
	} else {
		ids = (struct is_device_sensor *)ivc->device;

		mdbgv_sensor("%s(buffer count: %d)\n", ivc, __func__, b->count);
	}

	if (iv->video_type == IS_VIDEO_TYPE_CAPTURE) {
		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			leader = ivc->subdev->leader;
			if (leader && test_bit(IS_SUBDEV_START, &leader->state)) {
				merr("leader%d still running, subdev%d req is not applied",
							idi, leader->id, ivc->subdev->id);
				return -EBUSY;
			}
		} else {
			if (test_bit(IS_SENSOR_BACK_START, &ids->state)) {
				err("sensor%d still running, vid%d req is not applied",
								ids->device_id, iv->id);
				return -EBUSY;
			}
		}
	}

	if (!(ivc->state
	    & (BIT(IS_VIDEO_S_FORMAT) | BIT(IS_VIDEO_STOP) | BIT(IS_VIDEO_S_BUFS)))) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	if (!iq) {
		mverr("ivc->queue is NULL", ivc, iv);
		return -EINVAL;
	}

	if (test_bit(IS_QUEUE_STREAM_ON, &iq->state)) {
		mverr("already streaming", ivc, iv);
		return -EINVAL;
	}

	/* before call queue ops if request count is zero */
	if (!b->count) {
		ret = CALL_QOPS(iq, reqbufs, ivc->device, iq, b->count);
		if (ret) {
			mverr("failure in reqbufs QOP: %d", ivc, iv, ret);
			return ret;
		}
	}

	ret = vb2_reqbufs(iq->vbq, b);
	if (ret) {
		mverr("failure in vb2_reqbufs(): %d", ivc, iv, ret);
		return ret;
	}

	iq->buf_maxcount = b->count;
	framemgr = &iq->framemgr;

	if (iq->buf_maxcount == 0) {
		iq->buf_req = 0;
		iq->buf_pre = 0;
		iq->buf_que = 0;
		iq->buf_com = 0;
		iq->buf_dqe = 0;
		iq->buf_refcount = 0;

		clear_bit(IS_QUEUE_BUFFER_PREPARED, &iq->state);

		frame_manager_close(framemgr);
	} else {
		ret = frame_manager_open(framemgr, iq->buf_maxcount, true);
		if (ret) {
			mverr("failure in frame_manager_open(): %d", ivc, iv, ret);
			return ret;
		}
	}

	/* after call queue ops if request count is not zero */
	if (b->count) {
		ret = CALL_QOPS(iq, reqbufs, ivc->device, iq, b->count);
		if (ret) {
			mverr("failure in vb2_reqbufs(): %d", ivc, iv, ret);
			return ret;
		}
	}

	ivc->state = BIT(IS_VIDEO_S_BUFS);

	return 0;
}

int is_vidioc_querybuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = ivc->queue;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	ret = vb2_querybuf(iq->vbq, b);
	if (ret) {
		mverr("failure in vb2_querybuf(): %d", ivc, iv, ret);
		return ret;
	}

	return 0;
}

int is_vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance = get_instance_video_ctx(ivc);
	int ret;

	mvdbgs(3, "%s(%02d:%d)\n", ivc, ivc->queue, __func__, b->type, b->index);

	/* FIXME: clh, isp, lme, mcs, vra */
	/*
	struct is_queue *iq = &ivc->queue;

	if (!test_bit(IS_QUEUE_STREAM_ON, &iq->state)) {
		mierr("stream off state, can NOT qbuf", instance);
		return = -EINVAL;
	}
	*/

	ret = CALL_VOPS(ivc, qbuf, b);
	if (ret) {
		mierr("failure in qbuf video op: %d", instance, ret);
		return ret;
	}

	return 0;
}

int is_vidioc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance = get_instance_video_ctx(ivc);
	bool nonblocking = file->f_flags & O_NONBLOCK;
	int ret;

	mvdbgs(3, "%s(%02d:%d)\n", ivc, ivc->queue, __func__, b->type, b->index);

	ret = CALL_VOPS(ivc, dqbuf, b, nonblocking);
	if (ret) {
		if (ret != -EAGAIN)
			mierr("failure in dqbuf video op: %d", instance, ret);
		return ret;
	}

	return 0;
}

int is_vidioc_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = ivc->queue;
	struct vb2_queue *vbq;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	if (!(ivc->state & (BIT(IS_VIDEO_S_BUFS) | BIT(IS_VIDEO_STOP)))) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	if (!iq) {
		mverr("ivc->queue is NULL", ivc, iv);
		return -EINVAL;
	}

	vbq = iq->vbq;
	if (!vbq) {
		mverr("vbq is NULL", ivc, iv);
		return -EINVAL;
	}

	ret = vb2_streamon(vbq, i);
	if (ret) {
		mverr("failure in vb2_streamon(): %d", ivc, iv, ret);
		return ret;
	}

	ivc->state = BIT(IS_VIDEO_START);

	return 0;
}

int is_vidioc_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = ivc->queue;
	struct vb2_queue *vbq;
	struct is_framemgr *framemgr;
	u32 rcnt, pcnt, ccnt;
	int ret;

	mdbgv_video(ivc, iv, __func__);

	if (!(ivc->state & BIT(IS_VIDEO_START))) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	if (!iq) {
		mverr("ivc->queue is NULL", ivc, iv);
		return -EINVAL;
	}

	framemgr = &iq->framemgr;
	framemgr_e_barrier_irq(framemgr, FMGR_IDX_0);
	rcnt = framemgr->queued_count[FS_REQUEST];
	pcnt = framemgr->queued_count[FS_PROCESS];
	ccnt = framemgr->queued_count[FS_COMPLETE];
	framemgr_x_barrier_irq(framemgr, FMGR_IDX_0);

	if (rcnt + pcnt + ccnt > 0)
		mvwarn("streamoff: queued buffer is not empty(R%d, P%d, C%d)",
						ivc, iv, rcnt, pcnt, ccnt);

	vbq = iq->vbq;
	if (!vbq) {
		mverr("vbq is NULL", ivc, iv);
		return -EINVAL;
	}

	ret = vb2_streamoff(vbq, i);
	if (ret) {
		mverr("failure in vb2_streamoff(): %d", ivc, iv, ret);
		return ret;
	}

	ret = frame_manager_flush(framemgr);
	if (ret) {
		mverr("failure in frame_manager_flush(): %d", ivc, iv, ret);
		return ret;
	}

	ivc->state = BIT(IS_VIDEO_STOP);

	return 0;
}

int is_vidioc_s_input(struct file *file, void *fs, unsigned int i)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance = get_instance_video_ctx(ivc);
	u32 scenario, stream, position, intype, leader;
	int ret;

	/* for each leader */
	if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
		scenario = (i & SENSOR_SCN_MASK) >> SENSOR_SCN_SHIFT;
		stream = (i & INPUT_STREAM_MASK) >> INPUT_STREAM_SHIFT;
		position = (i & INPUT_POSITION_MASK) >> INPUT_POSITION_SHIFT;
		intype = (i & INPUT_INTYPE_MASK) >> INPUT_INTYPE_SHIFT;
		leader = (i & INPUT_LEADER_MASK) >> INPUT_LEADER_SHIFT;

		if (iv->device_type == IS_DEVICE_ISCHAIN) {
			mdbgv_ischain("%s(input: %08X) - stream: %d, position: %d, "
					"intype: %d, leader: %d\n", ivc, __func__,
					i, stream, position, intype, leader);

			ret = is_ischain_group_s_input(ivc->device, ivc->group, stream,
							position, intype, leader);
			if (ret) {
				mierr("failure in is_ischain_group_s_input(): %d", instance, ret);
				return ret;
			}
		} else {
			mdbgv_sensor("%s(input: %08X) - scenario: %d, stream: %d, position: %d, "
					"intype: %d, leader: %d\n", ivc, __func__,
					scenario, i, stream, position, intype, leader);
			ret = is_sensor_s_input(ivc->device, position, scenario, leader);
			if (ret) {
				mierr("failure in is_sensor_s_input(): %d", instance, ret);
				return ret;
			}
		}
	}

	if (!(ivc->state
	    & (BIT(IS_VIDEO_OPEN) | BIT(IS_VIDEO_S_INPUT) | BIT(IS_VIDEO_S_BUFS)))) {
		mverr("invalid state(%lX)", ivc, iv, ivc->state);
		return -EINVAL;
	}

	ivc->state = BIT(IS_VIDEO_S_INPUT);

	return 0;
}

static int is_g_ctrl_completes(struct file *file)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = ivc->queue;
	if (!iq) {
		merr("ivc->queue is NULL", ivc);
		return -EINVAL;
	}

	return iq->framemgr.queued_count[FS_COMPLETE];
}

int is_vidioc_g_ctrl(struct file *file, void * fh, struct v4l2_control *a)
{
	int ret = 0;
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi;
	u32 instance = get_instance_video_ctx(ivc);

	mdbgv_video(ivc, iv, __func__);

	switch (a->id) {
	case V4L2_CID_IS_G_COMPLETES:
		if (iv->video_type != IS_VIDEO_TYPE_LEADER) {
			a->value = is_g_ctrl_completes(file);
		} else {
			mierr("unsupported ioctl(0x%x, 0x%x)", instance,
						a->id, a->id & 0xff);
			return -EINVAL;
		}
		break;
	case V4L2_CID_IS_END_OF_STREAM:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi) {
			minfo("G_CTRL: V4L2_CID_IS_END_OF_STREAM (%d)\n", idi, instance);
			a->value = instance;
		} else {
			return -EINVAL;
		}
		break;
	default:
		ret = is_vendor_vidioc_g_ctrl(ivc, a);
		if (ret) {
			mierr("unsupported ioctl(0x%x, 0x%x)", instance, a->id, a->id & 0xff);
			return -EINVAL;
		}
	}

	return 0;
}

static int is_g_ctrl_capture_meta(struct file *file, struct v4l2_ext_control *ext_ctrl)
{
#if IS_ENABLED(USE_DDK_INTF_CAP_META)
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_group *group = ivc->group;
	struct is_device_ischain *device = GET_DEVICE(ivc);
	struct is_cap_meta_info cap_meta_info;
	struct dma_buf *dma_buf = NULL;
	u32 size, fcount;
	ulong meta_kva = 0, cap_shot_kva;
	int dma_buf_fd, ret;

	mdbgv_video(ivc, iv, __func__);

	ret = copy_from_user(&cap_meta_info, ext_ctrl->ptr, sizeof(struct is_cap_meta_info));
	if (ret) {
		err("fail to copy_from_user, ret(%d)\n", ret);
		goto p_err;
	}

	fcount = cap_meta_info.frame_count;
	dma_buf_fd = cap_meta_info.dma_buf_fd;
	size = (sizeof(u32) * CAMERA2_MAX_IPC_VENDOR_LENGTH);

	dma_buf = dma_buf_get(dma_buf_fd);
	if (IS_ERR_OR_NULL(dma_buf)) {
		err("Failed to get dmabuf. fd %d ret %ld",
			dma_buf_fd, PTR_ERR(dma_buf));
		ret = -EINVAL;
		goto err_get_dbuf;
	}

	meta_kva = (ulong)pkv_dma_buf_vmap(dma_buf);
	if (IS_ERR_OR_NULL((void *)meta_kva)) {
		err("Failed to get kva %ld", meta_kva);
		ret = -ENOMEM;
		goto err_vmap;
	}

	/* To start the end of camera2_shot_ext */
	cap_shot_kva = meta_kva + sizeof(struct camera2_shot_ext);

	mdbgv_ischain("%s: request capture meta update. fcount(%d), size(%d), addr(%lx)\n",
		ivc, __func__, fcount, size, cap_shot_kva);

	ret = is_ischain_g_ddk_capture_meta_update(group, device, fcount, size, (ulong)cap_shot_kva);
	if (ret) {
		err("fail to ischain_g_ddk_capture_meta_upadte, ret(%d)\n", ret);
		goto p_err;
	}

p_err:
	if (!IS_ERR_OR_NULL((void *) dma_buf)) {
		pkv_dma_buf_vunmap(dma_buf, (void *)meta_kva);
		meta_kva = 0;
	}

err_vmap:
	if (!IS_ERR_OR_NULL(dma_buf))
		dma_buf_put(dma_buf);

err_get_dbuf:
#endif
	return 0;
}

int is_vidioc_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *device;
	struct is_framemgr *framemgr;
	struct is_queue *queue;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i, instance;
	int ret = 0;

	mdbgv_video(ivc, iv, __func__);

	device = GET_DEVICE(ivc);
	queue = GET_QUEUE(ivc);
	framemgr = &queue->framemgr;
	instance = get_instance_video_ctx(ivc);

	if (a->which != V4L2_CTRL_CLASS_CAMERA) {
		mierr("invalid control class(%d)", instance, a->which);
		return -EINVAL;
	}

	ret = is_vendor_vidioc_g_ext_ctrl(ivc, a);
	if (ret != -ENOIOCTLCMD)
		return ret;

	for (i = 0; i < a->count; i++) {
		ext_ctrl = (a->controls + i);

		switch (ext_ctrl->id) {
		case V4L2_CID_IS_G_CAP_META_UPDATE:
			is_g_ctrl_capture_meta(file, ext_ctrl);
			break;
		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = is_vidioc_g_ctrl(file, fh, &ctrl);
			if (ret) {
				merr("is_vidioc_g_ctrl is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return 0;
}

static void is_s_ctrl_flip(struct file *file, struct v4l2_control *a)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_queue *iq = ivc->queue;
	unsigned int flipnr;

	if (!iq) {
		merr("ivc->queue is NULL", ivc);
		return;
	}

	if (a->id == V4L2_CID_HFLIP)
		flipnr = SCALER_FLIP_COMMAND_X_MIRROR;
	else
		flipnr = SCALER_FLIP_COMMAND_Y_MIRROR;

	if (a->value)
		set_bit(flipnr, &iq->framecfg.flip);
	else
		clear_bit(flipnr, &iq->framecfg.flip);
}

static int is_s_ctrl_dvfs_cluster(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_resourcemgr *rscmgr;

	if (iv->device_type == IS_DEVICE_ISCHAIN) {
		idi = (struct is_device_ischain *)ivc->device;
		rscmgr = idi->resourcemgr;
	} else {
		ids = (struct is_device_sensor *)ivc->device;
		rscmgr = ids->resourcemgr;
	}

	return is_resource_ioctl(rscmgr, a);
}

static int is_s_ctrl_setfile(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi = GET_DEVICE_ISCHAIN(ivc);
	u32 scenario;

	if (test_bit(IS_ISCHAIN_START, &idi->state)) {
		mverr("already start, failed to apply setfile", ivc, iv);
		return -EINVAL;
	}

	idi->setfile = a->value;
	scenario = (idi->setfile & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
	mvinfo("[S_CTRL] setfile(%d), scenario(%d)\n", idi, iv,
			idi->setfile & IS_SETFILE_MASK, scenario);

	return 0;
}

static void is_s_ctrl_dvfs_scenario(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;
	struct is_dvfs_ctrl *dvfs_ctrl = &idi->resourcemgr->dvfs_ctrl;
	u32 vendor;

	dvfs_ctrl->dvfs_scenario = a->value;
	vendor = (dvfs_ctrl->dvfs_scenario >> IS_DVFS_SCENARIO_VENDOR_SHIFT)
						& IS_DVFS_SCENARIO_VENDOR_MASK;
	mvinfo("[S_CTRL][DVFS] value(%x), common(%d), vendor(%d)\n", ivc, iv,
			dvfs_ctrl->dvfs_scenario,
			dvfs_ctrl->dvfs_scenario & IS_DVFS_SCENARIO_COMMON_MODE_MASK,
			vendor);
}

static void is_s_ctrl_dvfs_recording_size(struct file *file, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;
	struct is_dvfs_ctrl *dvfs_ctrl = &idi->resourcemgr->dvfs_ctrl;

	dvfs_ctrl->dvfs_rec_size = a->value;

	mvinfo("[S_CTRL][DVFS] rec_width(%d), rec_height(%d)\n", ivc, iv,
			dvfs_ctrl->dvfs_rec_size & 0xffff,
			dvfs_ctrl->dvfs_rec_size >> IS_DVFS_SCENARIO_HEIGHT_SHIFT);
}

static void pablo_s_ctrl_fast_ctl_lens_pos(struct file *fp, struct v4l2_control *a)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)fp->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;
	struct fast_control_mgr *fastctlmgr = &idi->fastctlmgr;
	struct is_fast_ctl *fast_ctl = NULL;
	unsigned long flags;
	u32 state = IS_FAST_CTL_FREE;

	spin_lock_irqsave(&fastctlmgr->slock, flags);

	if (fastctlmgr->queued_count[state]) {
		fast_ctl = list_first_entry(&fastctlmgr->queued_list[state],
				struct is_fast_ctl, list);
		list_del(&fast_ctl->list);
		fastctlmgr->queued_count[state]--;

		fast_ctl->lens_pos = a->value;
		fast_ctl->lens_pos_flag = true;

		state = IS_FAST_CTL_REQUEST;
		fast_ctl->state = state;
		list_add_tail(&fast_ctl->list, &fastctlmgr->queued_list[state]);
		fastctlmgr->queued_count[state]++;
	} else {
		mwarn("not enough fast_ctl free queue\n", idi);
	}

	spin_unlock_irqrestore(&fastctlmgr->slock, flags);

	mdbgv_ischain("[S_CTRL] fast control: lens pos: %d\n", ivc, a->value);
}

int is_vidioc_s_ctrl(struct file *file, void *fh, struct v4l2_control *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	struct is_device_ischain *idi;
	struct is_device_sensor *ids;
	struct is_core *core = is_get_is_core();
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
	struct is_resourcemgr *resourcemgr = &core->resourcemgr;
	struct is_dvfs_ctrl *dvfs_ctrl = &(resourcemgr->dvfs_ctrl);
#endif
	u32 instance = get_instance_video_ctx(ivc);
	int ret;

	mdbgv_video(ivc, iv, __func__);

	switch (a->id) {
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		is_s_ctrl_flip(file, a);
		break;
	case V4L2_CID_IS_G_CAPABILITY:
		/* deprecated */
		break;
	case V4L2_CID_IS_DVFS_LOCK:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi) {
			is_pm_qos_add_request(&dvfs_ctrl->user_qos,
				PM_QOS_DEVICE_THROUGHPUT, a->value);
			mvinfo("[S_CTRL][DVFS] lock: %d\n",
						ivc, iv, a->value);
		} else {
			return -EINVAL;
		}
		break;
	case V4L2_CID_IS_DVFS_UNLOCK:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi) {
			is_pm_qos_remove_request(&dvfs_ctrl->user_qos);
			mvinfo("[S_CTRL][DVFS] unlock: %d\n",
						ivc, iv, a->value);
		} else {
			return -EINVAL;
		}
		break;
	case V4L2_CID_IS_DVFS_CLUSTER0:
	case V4L2_CID_IS_DVFS_CLUSTER1:
	case V4L2_CID_IS_DVFS_CLUSTER1_HF:
	case V4L2_CID_IS_DVFS_CLUSTER2:
		return is_s_ctrl_dvfs_cluster(file, a);
	case V4L2_CID_IS_FORCE_DONE:
		if (iv->video_type == IS_VIDEO_TYPE_LEADER) {
			set_bit(IS_GROUP_REQUEST_FSTOP, &ivc->group->state);
		} else {
			mierr("unsupported ioctl(0x%x, 0x%x)", instance,
						a->id, a->id & 0xff);
			return -EINVAL;
		}
		break;
	case V4L2_CID_IS_SET_SETFILE:
		return is_s_ctrl_setfile(file, a);
	case V4L2_CID_IS_END_OF_STREAM:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi)
			return is_ischain_open_wrap(idi, true);
		else
			return -EINVAL;
	case V4L2_CID_IS_S_DVFS_SCENARIO:
		if (iv->device_type == IS_DEVICE_ISCHAIN)
			is_s_ctrl_dvfs_scenario(file, a);
		else
			return -EINVAL;
		break;
	case V4L2_CID_IS_S_DVFS_RECORDING_SIZE:
		if (iv->device_type == IS_DEVICE_ISCHAIN)
			is_s_ctrl_dvfs_recording_size(file, a);
		else
			return -EINVAL;
		break;
	case V4L2_CID_IS_OPENING_HINT:
		mvinfo("opening hint(%d)\n", ivc, iv, a->value);
		core->vendor.opening_hint = a->value;
		break;
	case V4L2_CID_IS_CLOSING_HINT:
		mvinfo("closing hint(%d)\n", ivc, iv, a->value);
		core->vendor.closing_hint = a->value;
		break;
	case V4L2_CID_IS_ICPU_PRELOAD:
		mvinfo("Preload ICPU firmware(%d)\n", ivc, iv, a->value);
		if (a->value) {
			ret = pablo_icpu_itf_preload_firmware(a->value);
			if (ret)
				return ret;
			ret = is_itf_preload_setfile();
			if (ret)
				return ret;
		}
		break;
	case V4L2_CID_IS_HF_MAX_SIZE:
		core->vendor.isp_max_width = a->value >> 16;
		core->vendor.isp_max_height = a->value & 0xffff;
		mvinfo("[S_CTRL] max buffer size: %d x %d (0x%08X)",
				ivc, iv, core->vendor.isp_max_width,
				core->vendor.isp_max_height,
				a->value);
		break;
	case V4L2_CID_IS_YUV_MAX_SIZE:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi) {
			idi->yuv_max_width = a->value >> 16;
			idi->yuv_max_height = a->value & 0xffff;
			mvinfo("[S_CTRL] max yuv buffer size: %d x %d (0x%08X)",
					ivc, iv, idi->yuv_max_width,
					idi->yuv_max_height,
					a->value);
		}
		break;
	case V4L2_CID_IS_DEBUG_DUMP:
		info("camera resource dump is requested\n");
		is_resource_dump();

		if (a->value)
			panic("and requested panic from user");
		break;
	case V4L2_CID_IS_DEBUG_SYNC_LOG:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi)
			return is_logsync(a->value);
		else
			return -EINVAL;
	case V4L2_CID_IS_HAL_VERSION: /* deprecated */
		return 0;
	case V4L2_CID_IS_S_LLC_CONFIG:
		idi = GET_DEVICE_ISCHAIN(ivc);
		if (idi)
			idi->llc_mode = a->value;
		else
			return -EINVAL;
		break;
	case V4L2_CID_IS_S_IRTA_RESULT_BUF:
		idi = GET_DEVICE_ISCHAIN(ivc);
		return pablo_interface_irta_result_buf_set(idi->pii, a->value, 0);
	case V4L2_CID_IS_S_IRTA_RESULT_IDX:
		idi = GET_DEVICE_ISCHAIN(ivc);
		return pablo_interface_irta_result_fcount_set(idi->pii, a->value);
	case V4L2_CID_IS_S_CRTA_START:
		idi = GET_DEVICE_ISCHAIN(ivc);
		return pablo_interface_irta_start(idi->pii);
	case V4L2_CID_IS_FAST_CTL_LENS_POS:
		if (iv->device_type == IS_DEVICE_ISCHAIN)
			pablo_s_ctrl_fast_ctl_lens_pos(file, a);
		else
			return -EINVAL;
		break;
	case V4L2_CID_IS_NOTIFY_HAL_INIT:
		ids = GET_DEVICE(ivc);
		ret = is_vendor_notify_hal_init(a->value, ids);
		if (ret)
			return ret;
		break;
	default:
		ret = is_vendor_vidioc_s_ctrl(ivc, a);
		if (ret) {
			mierr("unsupported ioctl(0x%x, 0x%x)", instance, a->id,
								a->id & 0xff);
			return -EINVAL;
		}
	}

	return 0;
}

static int pablo_s_ext_ctrl_nfd_data(struct file *fp, struct v4l2_ext_control *e)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)fp->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;
	struct nfd_info *nfd_data = (struct nfd_info *)&idi->is_region->fd_info;
	unsigned long flags;
	int ret = 0;

	if (access_ok(e->ptr, sizeof(struct nfd_info))) {
		spin_lock_irqsave(&idi->is_region->fd_info_slock, flags);

		ret = __copy_from_user_inatomic(nfd_data, e->ptr,
						sizeof(struct nfd_info));
		if (ret) {
			merr("failed to copy nfd_info(%d)", idi, ret);
			memset(nfd_data, 0, sizeof(struct nfd_info));
		}

		if ((nfd_data->face_num < 0) || (nfd_data->face_num > MAX_FACE_COUNT))
			nfd_data->face_num = 0;

		spin_unlock_irqrestore(&idi->is_region->fd_info_slock, flags);

		mdbgv_ischain("[F%d] face num(%d)\n", ivc,
					nfd_data->frame_count, nfd_data->face_num);
	} else {
		ret = -EINVAL;
		merr("invalid nfd_info", idi);
	}

	return ret;
}

static int pablo_s_ext_ctrl_od_data(struct file *fp, struct v4l2_ext_control *e)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)fp->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;
	struct od_info *od_data = (struct od_info *)&idi->is_region->od_info;
	unsigned long flags;
	int ret = 0;

	if (access_ok(e->ptr, sizeof(struct od_info))) {
		spin_lock_irqsave(&idi->is_region->fd_info_slock, flags);

		ret = __copy_from_user_inatomic(od_data, e->ptr,
						sizeof(struct od_info));
		if (ret) {
			merr("failed to copy od_info(%d)", idi, ret);
			memset(od_data, 0, sizeof(struct od_info));
		}

		spin_unlock_irqrestore(&idi->is_region->fd_info_slock, flags);
	} else {
		ret = -EINVAL;
		merr("invalid od_info", idi);
	}

	return ret;
}

static int pablo_g_ext_ctrl_setfile_version(struct file *fp, struct v4l2_ext_control *e)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)fp->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;

	return is_ischain_g_ddk_setfile_version(idi, e->ptr);
}

static int pablo_s_ext_ctrl_capture_intent_info(struct file *fp, struct v4l2_ext_control *e)
{
	struct is_video_ctx *ivc = (struct is_video_ctx *)fp->private_data;
	struct is_device_ischain *idi = (struct is_device_ischain *)ivc->device;
	struct is_group *head;
	struct capture_intent_info_t info;
	int ret;

	ret = copy_from_user(&info, e->ptr, sizeof(struct capture_intent_info_t));
	if (ret) {
		err("fail to copy_from_user, ret(%d)\n", ret);
		return ret;
	}

	head = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN, idi->group[GROUP_SLOT_3AA]);

	is_vendor_s_ext_ctrl_capture_intent_info(head, info);

	mginfo("[S_CTRL] capture intent: intent(%d) count(%d) EV(%d) remaincount(%d)\n",
				head, head, info.captureIntent, info.captureCount,
				info.captureEV, head->remainCaptureIntentCount);

	return 0;
}

int is_vidioc_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *a)
{
	struct is_video *iv = video_drvdata(file);
	struct is_video_ctx *ivc = (struct is_video_ctx *)file->private_data;
	u32 instance = get_instance_video_ctx(ivc);
	struct v4l2_ext_control *e;
	struct v4l2_control ctrl;
	struct is_device_ischain *idi;
	int i;
	int ret = 0;

	mdbgv_video(ivc, iv, __func__);

	if (a->which != V4L2_CTRL_CLASS_CAMERA) {
		mierr("invalid control class(%d)", instance, a->which);
		return -EINVAL;
	}

	if (!is_vendor_vidioc_s_ext_ctrl(ivc, a))
		return 0;

	for (i = 0; i < a->count; i++) {
		e = (a->controls + i);

		switch (e->id) {
		case V4L2_CID_IS_S_NFD_DATA:
			if (iv->device_type == IS_DEVICE_ISCHAIN)
				ret = pablo_s_ext_ctrl_nfd_data(file, e);
			break;
		case V4L2_CID_IS_S_OD_DATA:
			if (iv->device_type == IS_DEVICE_ISCHAIN)
				ret = pablo_s_ext_ctrl_od_data(file, e);
			break;
		case V4L2_CID_IS_G_SETFILE_VERSION:
			if (iv->device_type == IS_DEVICE_ISCHAIN)
				ret = pablo_g_ext_ctrl_setfile_version(file, e);
			break;
		case V4L2_CID_SENSOR_SET_CAPTURE_INTENT_INFO:
			if (iv->device_type == IS_DEVICE_ISCHAIN)
				ret = pablo_s_ext_ctrl_capture_intent_info(file, e);
			break;
		case V4L2_CID_IS_S_IRTA_RESULT_BUF:
			idi = GET_DEVICE_ISCHAIN(ivc);
			return pablo_interface_irta_result_buf_set(
				idi->pii, a->request_fd, e->value);
		default:
			ctrl.id = e->id;
			ctrl.value = e->value;

			ret = is_vidioc_s_ctrl(file, fh, &ctrl);
			if (ret) {
				mierr("failure in is_vidioc_s_ctrl(): %d", instance, ret);
				return ret;
			}
		}
	}

	return ret;
}

static const struct v4l2_ioctl_ops pablo_v4l2_ioctl_ops_default = {
	.vidioc_querycap		= is_vidioc_querycap,
	.vidioc_g_fmt_vid_out_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_reqbufs			= is_vidioc_reqbufs,
	.vidioc_querybuf		= is_vidioc_querybuf,
	.vidioc_qbuf			= is_vidioc_qbuf,
	.vidioc_dqbuf			= is_vidioc_dqbuf,
	.vidioc_streamon		= is_vidioc_streamon,
	.vidioc_streamoff		= is_vidioc_streamoff,
	.vidioc_s_input			= is_vidioc_s_input,
	.vidioc_g_ctrl			= is_vidioc_g_ctrl,
	.vidioc_g_ext_ctrls		= is_vidioc_g_ext_ctrls,
	.vidioc_s_ctrl			= is_vidioc_s_ctrl,
	.vidioc_s_ext_ctrls		= is_vidioc_s_ext_ctrls,
};

const struct v4l2_ioctl_ops *pablo_get_v4l2_ioctl_ops_default(void)
{
	return &pablo_v4l2_ioctl_ops_default;
}
EXPORT_SYMBOL_GPL(pablo_get_v4l2_ioctl_ops_default);

static int is_ssx_video_check_3aa_state(struct is_device_sensor *device, u32 next_id)
{
	int ret = 0;
	int i;
	struct is_core *core;
	struct exynos_platform_is *pdata;
	struct is_device_sensor *sensor;
	struct is_device_ischain *ischain;
	struct is_group *group_3aa;

	core = is_get_is_core();

	pdata = core->pdata;
	if (!pdata) {
		merr("[SS%d] The pdata is NULL", device, device->instance);
		ret = -ENXIO;
		goto p_err;
	}

	if (next_id >= pdata->num_of_ip.taa) {
		merr("[SS%d] The next_id is too big.(next_id: %d)", device, device->instance,
		     next_id);
		ret = -ENODEV;
		goto p_err;
	}

	if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
		merr("[SS%d] current sensor is not in stop state", device, device->instance);
		ret = -EBUSY;
		goto p_err;
	}

	for (i = 0; i < IS_SENSOR_COUNT; i++) {
		sensor = &core->sensor[i];
		if (test_bit(IS_SENSOR_FRONT_START, &sensor->state)) {
			ischain = sensor->ischain;
			if (!ischain)
				continue;

			group_3aa = ischain->group[GROUP_SLOT_3AA];
			if (group_3aa->id == GROUP_ID_3AA0 + next_id) {
				merr("[SS%d] 3AA%d is busy", device, device->instance, next_id);
				ret = -EBUSY;
				goto p_err;
			}
		}
	}

p_err:
	return ret;
}

int is_ssx_video_s_ctrl(struct file *file, void *priv, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_vendor_ssx_video_s_ctrl(ctrl, device);
	if (ret) {
		merr("is_vendor_ssx_video_s_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_S_STREAM:
		{
			u32 sstream, instant, noblock, standby;

			sstream = (ctrl->value & SENSOR_SSTREAM_MASK) >> SENSOR_SSTREAM_SHIFT;
			standby =
				(ctrl->value & SENSOR_USE_STANDBY_MASK) >> SENSOR_USE_STANDBY_SHIFT;
			instant = (ctrl->value & SENSOR_INSTANT_MASK) >> SENSOR_INSTANT_SHIFT;
			noblock = (ctrl->value & SENSOR_NOBLOCK_MASK) >> SENSOR_NOBLOCK_SHIFT;
			/*
			 * nonblock(0) : blocking command
			 * nonblock(1) : non-blocking command
			 */

			minfo(" Sensor Stream %s : (cnt:%d, noblk:%d, standby:%d)\n", device,
				sstream ? "On" : "Off", instant, noblock, standby);

			device->use_standby = standby;
			device->sstream = sstream;

			if (sstream == IS_ENABLE_STREAM) {
				ret = is_sensor_front_start(device, instant, noblock);
				if (ret) {
					merr("is_sensor_front_start is fail(%d)", device, ret);
					goto p_err;
				}
			} else {
				ret = is_sensor_front_stop(device, false);
				if (ret) {
					merr("is_sensor_front_stop is fail(%d)", device, ret);
					goto p_err;
				}
			}
		}
		break;
	case V4L2_CID_IS_CHANGE_CHAIN_PATH:	/* Change OTF HW path */
		ret = is_ssx_video_check_3aa_state(device, ctrl->value);
		if (ret) {
			merr("[SS%d] 3AA%d is in invalid state(%d)", device, device->instance,
			     ctrl->value, ret);
			goto p_err;
		}

		ret = CALL_GROUP_OPS(&device->group_sensor, change_chain, ctrl->value);
		if (ret)
			merr("is_group_change_chain is fail(%d)", device, ret);
		break;
	case V4L2_CID_IS_CHECK_CHAIN_STATE:
		ret = is_ssx_video_check_3aa_state(device, ctrl->value);
		if (ret) {
			merr("[SS%d] 3AA%d is in invalid state(%d)", device, device->instance,
			     ctrl->value, ret);
			goto p_err;
		}
		break;
	/*
	 * gain boost: min_target_fps,  max_target_fps, scene_mode
	 */
	case V4L2_CID_IS_MIN_TARGET_FPS:
		minfo("%s: set min_target_fps(%d), state(0x%lx)\n", device, __func__, ctrl->value,
		      device->state);

		device->min_target_fps = ctrl->value;
		break;
	case V4L2_CID_IS_MAX_TARGET_FPS:
		minfo("%s: set max_target_fps(%d), state(0x%lx)\n", device, __func__, ctrl->value,
		      device->state);

		device->max_target_fps = ctrl->value;
		break;
	case V4L2_CID_SCENEMODE:
		if (test_bit(IS_SENSOR_FRONT_START, &device->state)) {
			err("failed to set scene_mode: %d - sensor stream on already\n",
			    ctrl->value);
			ret = -EINVAL;
		} else {
			device->scene_mode = ctrl->value;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRAME_RATE:
		if (is_sensor_s_frame_duration(device, ctrl->value)) {
			err("failed to set frame duration : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_AE_TARGET:
		if (is_sensor_s_exposure_time(device, ctrl->value)) {
			err("failed to set exposure time : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IS_S_SENSOR_SIZE:
		device->sensor_width =
			(ctrl->value & SENSOR_SIZE_WIDTH_MASK) >> SENSOR_SIZE_WIDTH_SHIFT;
		device->sensor_height =
			(ctrl->value & SENSOR_SIZE_HEIGHT_MASK) >> SENSOR_SIZE_HEIGHT_SHIFT;
		minfo("sensor size : %d x %d (0x%08X)", device, device->sensor_width,
		      device->sensor_height, ctrl->value);
		break;
	case V4L2_CID_IS_FORCE_DONE:
	case V4L2_CID_IS_END_OF_STREAM:
	case V4L2_CID_IS_SET_SETFILE:
	case V4L2_CID_IS_HAL_VERSION:
	case V4L2_CID_IS_DEBUG_DUMP:
	case V4L2_CID_IS_DVFS_CLUSTER0:
	case V4L2_CID_IS_DVFS_CLUSTER1:
	case V4L2_CID_IS_DVFS_CLUSTER1_HF:
	case V4L2_CID_IS_DVFS_CLUSTER2:
	case V4L2_CID_IS_DEBUG_SYNC_LOG:
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
	case V4L2_CID_IS_INTENT:
	case V4L2_CID_IS_CAPTURE_EXPOSURETIME:
	case V4L2_CID_IS_TRANSIENT_ACTION:
	case V4L2_CID_IS_REMOSAIC_CROP_ZOOM_RATIO:
	case V4L2_CID_IS_FORCE_FLASH_MODE:
	case V4L2_CID_IS_OPENING_HINT:
	case V4L2_CID_IS_CLOSING_HINT:
	case V4L2_CID_IS_S_LLC_CONFIG:
		ret = is_vidioc_s_ctrl(file, priv, ctrl);
		if (ret) {
			merr("is_vidioc_s_ctrl is fail(%d): 0x%x, 0x%x", device, ret, ctrl->id,
			     ctrl->id & 0xff);
			goto p_err;
		}
		break;
	case V4L2_CID_IS_SECURE_MODE:
	{
		u32 scenario;
		struct is_core *core;

		scenario = (ctrl->value & IS_SCENARIO_MASK) >> IS_SCENARIO_SHIFT;
		core = is_get_is_core();
		if (scenario == IS_SCENARIO_SECURE) {
			mvinfo("[SCENE_MODE] SECURE scenario(%d) was detected\n", device,
			       GET_VIDEO(vctx), scenario);
			device->ex_scenario = core->resourcemgr.scenario = scenario;
		}
		break;
	}
	case V4L2_CID_SENSOR_SET_GAIN:
		if (is_sensor_s_again(device, ctrl->value)) {
			err("failed to set gain : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_SHUTTER:
		if (is_sensor_s_shutterspeed(device, ctrl->value)) {
			err("failed to set shutter speed : %d\n - %d", ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IS_S_BNS:
		break;
	case V4L2_CID_SENSOR_SET_EXTENDED_MODE:
		device->ex_mode = ctrl->value;
		break;
	case V4L2_CID_SENSOR_SET_EXTENDED_MODE_EXTRA:
		device->ex_mode_extra = ctrl->value;
		break;
	case V4L2_CID_SENSOR_SET_EXTENDED_MODE_FORMAT:
		device->ex_mode_format = ctrl->value;
		break;
	case V4L2_CID_IS_S_STANDBY_OFF:
		/*
		 * User must set before qbuf for next stream on in standby on state.
		 * If it is not cleared, all qbuf buffer is returned with unprocessed.
		 */
		clear_bit(IS_GROUP_STANDBY, &device->group_sensor.state);
		minfo("Clear STANDBY state", device);
		break;
	default:
		ret = is_sensor_s_ctrl(device, ctrl);
		if (ret) {
			err("invalid ioctl(0x%08X) is requested", ctrl->id);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int is_ssx_video_s_ext_ctrls(struct file *file, void *priv, struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrls);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_sensor_s_ext_ctrls(device, ctrls);
	if (ret) {
		merr("s_ext_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ssx_video_g_ext_ctrls(struct file *file, void *priv, struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrls);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	device = GET_DEVICE(vctx);
	FIMC_BUG(!device);

	ret = is_vendor_ssx_video_g_ext_ctrl(ctrls, device);
	if (!ret)
		return 0;

	ret = is_sensor_g_ext_ctrls(device, ctrls);
	if (ret) {
		merr("g_ext_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int is_ssx_video_g_ctrl(struct file *file, void *priv, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!ctrl);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	ret = is_vendor_ssx_video_g_ctrl(ctrl, device);
	if (!ret)
		return 0;

	ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_IS_G_STREAM:
		if (device->instant_ret)
			ctrl->value = device->instant_ret;
		else
			ctrl->value = (test_bit(IS_SENSOR_FRONT_START, &device->state) ?
					       IS_ENABLE_STREAM :
					       IS_DISABLE_STREAM);
		break;
	case V4L2_CID_IS_G_BNS_SIZE:
		{
			u32 width, height;

			width = is_sensor_g_bns_width(device);
			height = is_sensor_g_bns_height(device);

			ctrl->value = (width << 16) | height;
		}
		break;
	case V4L2_CID_IS_G_SNR_STATUS:
		if (test_bit(IS_SENSOR_FRONT_SNR_STOP, &device->state))
			ctrl->value = 1;
		else
			ctrl->value = 0;
		/* additional set for ESD notification */
		if (test_bit(IS_SENSOR_ESD_RECOVERY, &device->state))
			ctrl->value += 2;
		if (test_bit(IS_SENSOR_ASSERT_CRASH, &device->state))
			ctrl->value += 4;
		break;
	case V4L2_CID_IS_G_MIPI_ERR:
		ctrl->value = is_sensor_g_csis_error(device);
		break;
	case V4L2_CID_IS_G_ACTIVE_CAMERA:
		if (test_bit(IS_SENSOR_S_INPUT, &device->state))
			ctrl->value = 1;
		else
			ctrl->value = 0;
		break;
	case V4L2_CID_IS_END_OF_STREAM:
		ret = is_vidioc_g_ctrl(file, priv, ctrl);
		if (ret) {
			merr("is_vidioc_g_ctrl is fail(%d): 0x%x, %d", device, ret, ctrl->id,
			     ctrl->id & 0xff);
			goto p_err;
		}
		break;
	default:
		ret = is_sensor_g_ctrl(device, ctrl);
		if (ret) {
			err("invalid ioctl(0x%08X) is requested", ctrl->id);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int is_ssx_video_g_parm(struct file *file, void *priv, struct v4l2_streamparm *parm)
{
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tfp;

	FIMC_BUG(!parm);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	cp = &parm->parm.capture;
	tfp = &cp->timeperframe;
	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	cp->capability |= V4L2_CAP_TIMEPERFRAME;
	tfp->numerator = 1;
	tfp->denominator = device->image.framerate;

	return 0;
}

static int is_ssx_video_s_parm(struct file *file, void *priv, struct v4l2_streamparm *parm)
{
	int ret = 0;
	struct is_video_ctx *vctx;
	struct is_device_sensor *device;

	FIMC_BUG(!parm);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_sensor("%s\n", vctx, __func__);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = is_sensor_s_framerate(device, parm);
	if (ret) {
		merr("is_ssx_s_framerate is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops is_ssx_video_ioctl_ops = {
	.vidioc_querycap = is_vidioc_querycap,
	.vidioc_g_fmt_vid_out_mplane = is_vidioc_g_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane = is_vidioc_g_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane = is_vidioc_s_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane = is_vidioc_s_fmt_mplane,
	.vidioc_reqbufs = is_vidioc_reqbufs,
	.vidioc_querybuf = is_vidioc_querybuf,
	.vidioc_qbuf = is_vidioc_qbuf,
	.vidioc_dqbuf = is_vidioc_dqbuf,
	.vidioc_streamon = is_vidioc_streamon,
	.vidioc_streamoff = is_vidioc_streamoff,
	.vidioc_s_input = is_vidioc_s_input,
	.vidioc_g_ctrl = is_ssx_video_g_ctrl,
	.vidioc_g_ext_ctrls = is_ssx_video_g_ext_ctrls,
	.vidioc_s_ctrl = is_ssx_video_s_ctrl,
	.vidioc_s_ext_ctrls = is_ssx_video_s_ext_ctrls,
	.vidioc_g_parm = is_ssx_video_g_parm,
	.vidioc_s_parm = is_ssx_video_s_parm,
};

const struct v4l2_ioctl_ops *pablo_get_v4l2_ioctl_ops_sensor(void)
{
	return &is_ssx_video_ioctl_ops;
}
EXPORT_SYMBOL_GPL(pablo_get_v4l2_ioctl_ops_sensor);

MODULE_DESCRIPTION("v4l2 ioctl module for Exynos Pablo");
MODULE_LICENSE("GPL v2");
