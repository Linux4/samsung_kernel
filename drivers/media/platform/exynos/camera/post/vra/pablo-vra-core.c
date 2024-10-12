/*
 * Samsung EXYNOS CAMERA PostProcessing VRA driver
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>

#ifdef USE_CLOCK_INFO
#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#endif

#ifdef CONFIG_EXYNOS_IOVMM
#include <linux/exynos_iovmm.h>
#else
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/dma-iommu.h>
#endif

#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-sg.h>
#include <media/v4l2-ioctl.h>

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
#include <soc/samsung/exynos-sci.h>
#endif

#include "pablo-vra.h"
#include "pablo-hw-api-vra.h"
#include "is-video.h"

#ifdef FILE_DUMP

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

int write_file(char* name, char *buf, size_t size)
{
	int ret = 0;
	struct file *fp;

	fp = filp_open(name, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
	if (IS_ERR_OR_NULL(fp)) {
		ret = PTR_ERR(fp);
		printk("[err]%s(): open file error(%s), error(%d)\n", __func__, name, ret);
		goto p_err;
	}

	ret = kernel_write(fp, buf, size, &fp->f_pos);
	if (ret < 0) {
		printk("[err]%s(): file write fail(%s) ret(%d)", __func__, name, ret);
		goto p_err;
	}

p_err:
	if (!IS_ERR_OR_NULL(fp))
		filp_close(fp, NULL);

	return ret;
}

#endif

/* Flags that are set by us */
#define V4L2_BUFFER_MASK_FLAGS	(V4L2_BUF_FLAG_MAPPED | V4L2_BUF_FLAG_QUEUED | \
				 V4L2_BUF_FLAG_DONE | V4L2_BUF_FLAG_ERROR | \
				 V4L2_BUF_FLAG_PREPARED | \
				 V4L2_BUF_FLAG_IN_REQUEST | \
				 V4L2_BUF_FLAG_REQUEST_FD | \
				 V4L2_BUF_FLAG_TIMESTAMP_MASK)
/* Output buffer flags that should be passed on to the driver */
#define V4L2_BUFFER_OUT_FLAGS	(V4L2_BUF_FLAG_PFRAME | V4L2_BUF_FLAG_BFRAME | \
				 V4L2_BUF_FLAG_KEYFRAME | V4L2_BUF_FLAG_TIMECODE)

static int vra_debug_level;
module_param_named(vra_debug_level, vra_debug_level, uint, S_IRUGO | S_IWUSR);

static int vra_suspend(struct device *dev);
static int vra_resume(struct device *dev);

struct vb2_vra_buffer {
	struct v4l2_m2m_buffer mb;
	struct vra_ctx *ctx;
	ktime_t ktime;
};

static struct device *vra_device;

static const struct vra_fmt vra_formats[] = {
	{
		.name		= "RGB24",
		.pixelformat	= V4L2_PIX_FMT_RGB24,
		.bitperpixel	= { 24 },
		.num_planes	= 1,
		.h_shift	= 1,
		.v_shift	= 1,
	}, {
		.name		= "GREY",
		.pixelformat	= V4L2_PIX_FMT_GREY,
		.bitperpixel	= { 8 },
		.num_planes	= 1,
		.h_shift	= 1,
		.v_shift	= 1,
	},

};

int vra_get_debug_level(void)
{
	return vra_debug_level;
}

/* Find the matches format */
static const struct vra_fmt *vra_find_format(struct vra_dev *vra,
						u32 pixfmt, bool output_buf)
{
	const struct vra_fmt *vra_fmt;
	unsigned long i;

	vra_dbg("[VRA]\n");
	for (i = 0; i < ARRAY_SIZE(vra_formats); ++i) {
		vra_fmt = &vra_formats[i];
		if (vra_fmt->pixelformat == pixfmt) {
			return &vra_formats[i];
		}
	}

	return NULL;
}

static int vra_v4l2_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap)
{

	vra_dbg("[VRA]\n");
	strncpy(cap->driver, VRA_MODULE_NAME, sizeof(cap->driver) - 1);
	strncpy(cap->card, VRA_MODULE_NAME, sizeof(cap->card) - 1);

	cap->capabilities = V4L2_CAP_STREAMING |
		V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE;
	cap->capabilities |= V4L2_CAP_DEVICE_CAPS;
	cap->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE;

	return 0;
}

static int vra_v4l2_g_fmt_mplane(struct file *file, void *fh,
			  struct v4l2_format *f)
{
	struct vra_ctx *ctx = fh_to_vra_ctx(fh);
	const struct vra_fmt *vra_fmt;
	struct vra_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	int i;

	vra_dbg("[VRA]\n");
	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	vra_fmt = frame->vra_fmt;

	pixm->width		= frame->width;
	pixm->height		= frame->height;
	pixm->pixelformat	= frame->pixelformat;
	pixm->field		= V4L2_FIELD_NONE;
	pixm->num_planes	= frame->vra_fmt->num_planes;
	pixm->colorspace	= 0;

	for (i = 0; i < pixm->num_planes; ++i) {
		pixm->plane_fmt[i].bytesperline = (pixm->width *
				vra_fmt->bitperpixel[i]) >> 3;
		if (vra_fmt_is_ayv12(vra_fmt->pixelformat)) {
			unsigned int y_size, c_span;
			y_size = pixm->width * pixm->height;
			c_span = ALIGN(pixm->width >> 1, 16);
			pixm->plane_fmt[i].sizeimage =
				y_size + (c_span * pixm->height >> 1) * 2;
		} else {
			pixm->plane_fmt[i].sizeimage =
				pixm->plane_fmt[i].bytesperline * pixm->height;
		}

		v4l2_dbg(1, vra_debug_level, &ctx->vra_dev->m2m.v4l2_dev,
				"[%d] plane: bytesperline %d, sizeimage %d\n",
				i, pixm->plane_fmt[i].bytesperline,
				pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

static int vra_v4l2_try_fmt_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct vra_ctx *ctx = fh_to_vra_ctx(fh);
	const struct vra_fmt *vra_fmt;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	struct vra_frame *frame;
	int i;

	vra_dbg("[VRA]\n");
	if (!V4L2_TYPE_IS_MULTIPLANAR(f->type)) {
		v4l2_err(&ctx->vra_dev->m2m.v4l2_dev,
				"not supported v4l2 type\n");
		return -EINVAL;
	}

	vra_fmt = vra_find_format(ctx->vra_dev, f->fmt.pix_mp.pixelformat, V4L2_TYPE_IS_OUTPUT(f->type));
	if (!vra_fmt) {
		v4l2_err(&ctx->vra_dev->m2m.v4l2_dev,
				"not supported format type\n");
		return -EINVAL;
	}

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	for (i = 0; i < vra_fmt->num_planes; ++i) {
		/* The pixm->plane_fmt[i].sizeimage for the plane which
		 * contains the src blend data has to be calculated as per the
		 * size of the actual width and actual height of the src blend
		 * buffer */

		pixm->plane_fmt[i].bytesperline = (pixm->width *
				vra_fmt->bitperpixel[i]) >> 3;
		pixm->plane_fmt[i].sizeimage =
				pixm->plane_fmt[i].bytesperline * pixm->height;

		v4l2_dbg(1, vra_debug_level, &ctx->vra_dev->m2m.v4l2_dev,
				"[%d] plane: bytesperline %d, sizeimage %d\n",
				i, pixm->plane_fmt[i].bytesperline,
				pixm->plane_fmt[i].sizeimage);
	}

	return 0;
}

static int vra_v4l2_s_fmt_mplane(struct file *file, void *fh,
				 struct v4l2_format *f)

{
	struct vra_ctx *ctx = fh_to_vra_ctx(fh);
	struct vb2_queue *vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	struct vra_frame *frame;
	struct v4l2_pix_format_mplane *pixm = &f->fmt.pix_mp;
	const struct vra_size_limit *limit =
				&ctx->vra_dev->variant->limit_input;
	int i, ret = 0;

	vra_dbg("[VRA]\n");
	if (vb2_is_streaming(vq)) {
		v4l2_err(&ctx->vra_dev->m2m.v4l2_dev, "device is busy\n");
		return -EBUSY;
	}

	ret = vra_v4l2_try_fmt_mplane(file, fh, f);
	if (ret < 0)
		return ret;

	frame = ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	set_bit(CTX_PARAMS, &ctx->flags);

	frame->vra_fmt = vra_find_format(ctx->vra_dev, f->fmt.pix_mp.pixelformat, V4L2_TYPE_IS_OUTPUT(f->type));
	if (!frame->vra_fmt) {
		v4l2_err(&ctx->vra_dev->m2m.v4l2_dev,
				"not supported format values\n");
		return -EINVAL;
	}

	frame->num_planes = (pixm->num_planes < VRA_MAX_PLANES) ? pixm->num_planes : VRA_MAX_PLANES;

	for (i = 0; i < frame->num_planes; i++) {
		frame->bytesused[i] = pixm->plane_fmt[i].sizeimage;
	}

	if (V4L2_TYPE_IS_OUTPUT(f->type) &&
		((pixm->width > limit->max_w) ||
			 (pixm->height > limit->max_h))) {
		v4l2_err(&ctx->vra_dev->m2m.v4l2_dev,
			"%dx%d of source image is not supported: too large\n",
			pixm->width, pixm->height);
		return -EINVAL;
	}

	if (V4L2_TYPE_IS_OUTPUT(f->type) &&
		((pixm->width < limit->min_w) ||
			 (pixm->height < limit->min_h))) {
		v4l2_err(&ctx->vra_dev->m2m.v4l2_dev,
			"%dx%d of source image is not supported: too small\n",
			pixm->width, pixm->height);
		return -EINVAL;
	}

	frame->width = pixm->width;
	frame->height = pixm->height;
	frame->pixelformat = pixm->pixelformat;

	vra_info("[VRA] pixelformat(%c%c%c%c) size(%dx%d)\n",
		(char)((frame->vra_fmt->pixelformat >> 0) & 0xFF),
		(char)((frame->vra_fmt->pixelformat >> 8) & 0xFF),
		(char)((frame->vra_fmt->pixelformat >> 16) & 0xFF),
		(char)((frame->vra_fmt->pixelformat >> 24) & 0xFF),
		frame->width, frame->height);

	return 0;
}

static int vra_v4l2_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *reqbufs)
{
	struct vra_ctx *ctx = fh_to_vra_ctx(fh);

	vra_dbg("[VRA]\n");

	return v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);
}

static int vra_check_vb2_qbuf(struct vb2_queue *q, struct v4l2_buffer *b)
{
	struct vb2_buffer *vb;
	struct vb2_plane planes[VB2_MAX_PLANES];
	int plane;
	int ret = 0;

	vra_dbg("[VRA]\n");
	if (q->fileio) {
		vra_info("[ERR] file io in progress\n");
		ret = -EBUSY;
		goto q_err;
	}

	if (b->type != q->type) {
		vra_info("[ERR] buf type is invalid(%d != %d)\n",
			b->type, q->type);
		ret = -EINVAL;
		goto q_err;
	}

	if (b->index >= q->num_buffers) {
		vra_info("[ERR] buffer index out of range b_index(%d) q_num_buffers(%d)\n",
			b->index, q->num_buffers);
		ret = -EINVAL;
		goto q_err;
	}

	if (q->bufs[b->index] == NULL) {
		/* Should never happen */
		vra_info("[ERR] buffer is NULL\n");
		ret = -EINVAL;
		goto q_err;
	}

	if (b->memory != q->memory) {
		vra_info("[ERR] invalid memory type b_mem(%d) q_mem(%d)\n",
			b->memory, q->memory);
		ret = -EINVAL;
		goto q_err;
	}

	vb = q->bufs[b->index];
	if (!vb) {
		vra_info("[ERR] vb is NULL");
		ret = -EINVAL;
		goto q_err;
	}

	if (V4L2_TYPE_IS_MULTIPLANAR(b->type)) {
		/* Is memory for copying plane information present? */
		if (b->m.planes == NULL) {
			vra_info("[ERR] multi-planar buffer passed but "
				   "planes array not provided\n");
			ret = -EINVAL;
			goto q_err;
		}

		if (b->length < vb->num_planes || b->length > VB2_MAX_PLANES) {
			vra_info("[ERR] incorrect planes array length, "
				   "expected %d, got %d\n",
				   vb->num_planes, b->length);
			ret = -EINVAL;
			goto q_err;
		}
	}

	if ((b->flags & V4L2_BUF_FLAG_REQUEST_FD) &&
		vb->state != VB2_BUF_STATE_DEQUEUED) {
		vra_info("[ERR] buffer is not in dequeued state\n");
		ret = -EINVAL;
		goto q_err;
	}

	/* For detect vb2 framework err, operate some vb2 functions */
	memset(planes, 0, sizeof(planes[0]) * vb->num_planes);
	/* Copy relevant information provided by the userspace */
	ret = call_bufop(vb->vb2_queue, fill_vb2_buffer,
			 vb, planes);
	if (ret) {
		vra_info("[ERR] vb2_fill_vb2_v4l2_buffer failed (%d)\n", ret);
		goto q_err;
	}

	for (plane = 0; plane < vb->num_planes; ++plane) {
		struct dma_buf *dbuf;

		dbuf = dma_buf_get(planes[plane].m.fd);
		if (IS_ERR_OR_NULL(dbuf)) {
			vra_info("[ERR] invalid dmabuf fd(%d) for plane %d\n",
				planes[plane].m.fd, plane);
			ret = -EINVAL;
			goto q_err;
		}

		if (planes[plane].length == 0)
			planes[plane].length = (unsigned int)dbuf->size;

		if (planes[plane].length < vb->planes[plane].min_length) {
			vra_info("[ERR] invalid dmabuf length %u for plane %d, "
				"minimum length %u %u %u\n",
				planes[plane].length, plane,
				vb->planes[plane].min_length,
				vb->planes[plane].length,
				vb->planes[plane].bytesused);
			ret = -EINVAL;
			dma_buf_put(dbuf);
			goto q_err;
		}
		dma_buf_put(dbuf);
	}
q_err:
	return ret;
}

static int vra_check_qbuf(struct file *file,
	struct v4l2_m2m_ctx *m2m_ctx, struct v4l2_buffer *buf)
{
	struct vb2_queue *vq;

	vra_dbg("[VRA]\n");
	vq = v4l2_m2m_get_vq(m2m_ctx, buf->type);
	if (!V4L2_TYPE_IS_OUTPUT(vq->type) &&
		(buf->flags & V4L2_BUF_FLAG_REQUEST_FD)) {
		vra_info("[ERR] requests cannot be used with capture buffers\n");
		return -EPERM;
	}
	return vra_check_vb2_qbuf(vq, buf);
}

static int vra_v4l2_qbuf(struct file *file, void *fh,
			 struct v4l2_buffer *buf)
{
	struct vra_ctx *ctx = fh_to_vra_ctx(fh);
	struct vra_dev *vra = ctx->vra_dev;
	int ret;

	vra_dbg("[VRA] buf->type:%d, %s\n", buf->type, V4L2_TYPE_IS_OUTPUT(buf->type) ? "input" : "output");
	if (V4L2_TYPE_IS_OUTPUT(buf->type)) {
		if (ctx->model_param.clockMode == VRA_CLOCK_DYNAMIC) {
			if (vra->qosreq_m2m_level != ctx->model_param.clockLevel) {
				vra->qosreq_m2m_level = ctx->model_param.clockLevel;
				vra_dbg("[VRA] Update M2M clock set: %d\n", vra->qosreq_m2m_level);
				vra_pm_qos_update_request(&vra->qosreq_m2m, vra->qosreq_m2m_level);
			}
		} else {
			if (vra->qosreq_m2m_level < ctx->model_param.clockLevel) {
				vra->qosreq_m2m_level = ctx->model_param.clockLevel;
				vra_dbg("[VRA] Update M2M clock set: %d\n", vra->qosreq_m2m_level);
				vra_pm_qos_update_request(&vra->qosreq_m2m, vra->qosreq_m2m_level);
			}
		}
	}

	ret = v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
	if (ret) {
		dev_err(ctx->vra_dev->dev,
			"v4l2_m2m_qbuf failed ret(%d) check(%d)\n",
			ret, vra_check_qbuf(file, ctx->m2m_ctx, buf));
	}

	return ret;
}

static int vra_v4l2_dqbuf(struct file *file, void *fh,
			  struct v4l2_buffer *buf)
{
	struct vra_ctx *ctx = fh_to_vra_ctx(fh);
	struct vra_dev *vra = ctx->vra_dev;

	vra_dbg("[VRA] buf->type:%d, %s\n", buf->type, V4L2_TYPE_IS_OUTPUT(buf->type) ? "input" : "output");
	if (!V4L2_TYPE_IS_OUTPUT(buf->type)
		&& (ctx->model_param.clockMode == VRA_CLOCK_DYNAMIC)
		&& (ctx->model_param.clockLevel != VRA_CLOCK_MIN)) {
		vra->qosreq_m2m_level = VRA_CLOCK_MIN;
		vra_dbg("[VRA] M2M clock set: %d\n", vra->qosreq_m2m_level);
		vra_pm_qos_update_request(&vra->qosreq_m2m, vra->qosreq_m2m_level);
	}

	return v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
}

static int vra_power_clk_enable(struct vra_dev *vra)
{
#ifdef USE_CLOCK_INFO
	ulong rate_target;
#endif
	int ret;

	vra_info("[VRA]\n");
	if (in_interrupt())
		ret = pm_runtime_get(vra->dev);
	else
		ret = pm_runtime_get_sync(vra->dev);

	if (ret < 0) {
		dev_err(vra->dev,
			"%s=%d: Failed to enable local power\n", __func__, ret);
		return ret;
	}

#ifdef USE_CLOCK_INFO
	vra_dbg("vra->aclk:0x%p \n", vra->aclk);
#endif
	if (!IS_ERR(vra->aclk)) {
		ret = clk_enable(vra->aclk);
		if (ret) {
			dev_err(vra->dev, "%s: Failed to enable ACLK (err %d)\n",
				__func__, ret);
			goto err_aclk;
		}
	}
#ifdef USE_CLOCK_INFO
	vra_dbg("[@][ENABLE] : aclk (is_enabled : %d) -- \n", __clk_is_enabled(vra->aclk));
	rate_target = clk_get_rate(vra->aclk);
	vra_dbg("[@] vra: %ldMhz\n", rate_target/1000000);
#endif
	return 0;
err_aclk:
	pm_runtime_put(vra->dev);

	return ret;
}

static void vra_clk_power_disable(struct vra_dev *vra)
{
	vra_info("[VRA]\n");
	camerapp_hw_vra_stop(vra->regs_base);

	if (!IS_ERR(vra->aclk))
		clk_disable(vra->aclk);

	pm_runtime_put(vra->dev);
}

static int vra_v4l2_streamon(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	struct vra_ctx *ctx = fh_to_vra_ctx(fh);
	struct vra_dev *vra = ctx->vra_dev;
	int ret;

	vra_info("[VRA] type:%d, %s\n", type, V4L2_TYPE_IS_OUTPUT(type) ? "input" : "output");
	if (!V4L2_TYPE_IS_OUTPUT(type) && !test_bit(DEV_READY, &vra->state)) {
		ret = vra_power_clk_enable(vra);
		if (ret)
			return ret;

		set_bit(DEV_READY, &vra->state);
		vra_dbg("vra clk enable\n");
	}
	return v4l2_m2m_streamon(file, ctx->m2m_ctx, type);
}

static int vra_v4l2_streamoff(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	struct vra_ctx *ctx = fh_to_vra_ctx(fh);
	struct vra_dev *vra = ctx->vra_dev;

	vra_info("[VRA] type:%d, %s\n", type, V4L2_TYPE_IS_OUTPUT(type) ? "input" : "output");
	if (!V4L2_TYPE_IS_OUTPUT(type) && test_bit(DEV_READY, &vra->state)) {
		if (atomic_read(&vra->m2m.in_use) == 1) {
			if (camerapp_hw_vra_sw_reset(vra->regs_base))
				dev_err(vra->dev, "vra sw reset fail\n");

			vra_clk_power_disable(vra);
			clear_bit(DEV_READY, &vra->state);
		}
	}

	return v4l2_m2m_streamoff(file, ctx->m2m_ctx, type);
}

static int vra_v4l2_s_ctrl(struct file * file, void * priv,
			struct v4l2_control *ctrl)
{
	int ret = 0;
	vra_dbg("[VRA] v4l2_s_ctrl = %d (%d)\n", ctrl->id, ctrl->value);

	switch (ctrl->id) {
	case V4L2_CID_CAMERAPP_VRA_MODEL_CONTROL:
		break;
	default:
		ret = -EINVAL;

		vra_dbg("Err: Invalid ioctl id(%d)\n", ctrl->id);
		break;
	}

	return ret;
}

/*
 * API prototype is changed.
 * before v5.15: void *dma_buf_vmap(struct dma_buf *dmabuf);
 * after v5.15: int dma_buf_vmap(struct dma_buf *dmabuf, struct dma_buf_map *map);
 */

static dma_addr_t vra_get_pa_from_fd(struct vra_dev *vra, struct vra_ctx *ctx, enum VRA_BUF_TYPE type, int dmafd) {
	struct device *dev = vra->dev;
	int err;
	dma_addr_t addr;
#ifdef FILE_DUMP
	char *dump;
	static dumpCount = 0;
	char pname[128];
	struct dma_buf_map map;
	int ret_dma_buf_vmap;
#endif

	vra_dbg("[VRA]\n");
	if (dmafd < 0) {
		dev_err(dev, "fd is invalid (type: %d, fd: %d)\n", type, dmafd);
		return 0;
	}

	ctx->dmabuf[type] = dma_buf_get(dmafd);
	if (IS_ERR(ctx->dmabuf[type] )) {
		err = PTR_ERR(ctx->dmabuf[type] );
		dev_err(dev, "got dmabuf fail %d (type: %d, fd: %d)\n", err, type, dmafd);
		goto err_buf_get;
	}

	vra_dbg("got dmabuf %p %s\n", ctx->dmabuf[type], ctx->dmabuf[type]->exp_name);
	ctx->attachment[type] = dma_buf_attach(ctx->dmabuf[type], dev);
	if (IS_ERR(ctx->attachment[type])) {
		err = PTR_ERR(ctx->attachment[type]);
		dev_err(dev, "Failed to attach %d (type: %d, fd: %d)\n", err, type, dmafd);
		goto err_buf_attach;
	}

	ctx->sgt[type] = dma_buf_map_attachment(ctx->attachment[type], DMA_TO_DEVICE);
	if (IS_ERR(ctx->sgt[type])) {
		err = PTR_ERR(ctx->sgt[type]);
		dev_err(dev, "Failed to attachement %d (type: %d, fd: %d)\n", err, type, dmafd);
		goto err_buf_attachment;
	}

	addr = (dma_addr_t)sg_dma_address(ctx->sgt[type]->sgl);
	vra_dbg("fd %d, type %d, addr = %pad\n", dmafd, type, &addr);

#ifdef FILE_DUMP
	if (vra_get_debug_level() == VRA_DEBUG_BUF_DUMP) {
		ret_dma_buf_vmap = dma_buf_vmap(ctx->dmabuf[type], &map);
        if (ret_dma_buf_vmap) {
                pr_err("failed to dma_buf_vmap(ret:%d)\n", ret_dma_buf_vmap);
                return 0;
        }
		dump = (char *)map.vaddr;
		vra_dbg("kva - fd(%d) : addr = %lx\n", dmafd, (unsigned long)dump);

		if (type == VRA_BUF_TYPE_INSTRUCTION) {
			snprintf(pname, sizeof(pname), "%s/%03d_%s.bin", VRA_DUMP_PATH, dumpCount, VRA_INSTRUCTION_DUMP);
		} else if (type == VRA_BUF_TYPE_CONSTANT) {
			snprintf(pname, sizeof(pname), "%s/%03d_%s.bin", VRA_DUMP_PATH, dumpCount, VRA_CONSTANT_DUMP);
		} else if (type == VRA_BUF_TYPE_TEMP) {
			snprintf(pname, sizeof(pname), "%s/%03d_%s.bin", VRA_DUMP_PATH, dumpCount, VRA_TEMP_DUMP);
		} else {
			printk("wrong type -> need to check (type: %d, fd: %d)", dmafd, type);
			snprintf(pname, sizeof(pname), "%s/%03d_%s.bin", VRA_DUMP_PATH, dumpCount, VRA_UNKNOWN_DUMP);
		}
		vra_dbg("dump - start(path : %s, size : %03d, dumpCount: %d, type: %d, fd: %d)", pname, ctx->dmabuf[type]->size, dumpCount, type, dmafd);
		write_file(pname, dump, (size_t)ctx->dmabuf[type]->size);
		vra_dbg("dump - end")

		dumpCount++;
		dma_buf_vunmap(ctx->dmabuf[type], &map);
	}
#endif

	return addr;

err_buf_attachment:
	dma_buf_detach(ctx->dmabuf[type], ctx->attachment[type]);
err_buf_attach:
	dma_buf_put(ctx->dmabuf[type]);
err_buf_get:
	ctx->dmabuf[type] = NULL;
	ctx->attachment[type] = NULL;
	ctx->sgt[type] = NULL;

	return 0;
}

int vra_free_pa(struct vra_dev *vra, struct vra_ctx *ctx, int type)
{
	struct device *dev = vra->dev;
	if(!ctx->dmabuf[type]) {
		dev_err(dev, "dma_buf is NULL(type: %d)", type);
		return -EINVAL;
	}

	dma_buf_unmap_attachment(ctx->attachment[type], ctx->sgt[type], DMA_BIDIRECTIONAL);
	dma_buf_detach(ctx->dmabuf[type], ctx->attachment[type]);
	dma_buf_put(ctx->dmabuf[type]);

	ctx->sgt[type] = NULL;
	ctx->attachment[type] = NULL;
	ctx->dmabuf[type] = NULL;

	return 0;
}

static void vra_free_all_pa(struct vra_dev *vra, struct vra_ctx *ctx)
{
	int type;
	for (type = 0; type < VRA_DMA_COUNT; type++) {
		if (ctx->dmabuf[type])
			vra_free_pa(vra, ctx, type);
	}
}


static int vra_v4l2_s_ext_ctrls(struct file * file, void * priv,
				 struct v4l2_ext_controls * ctrls)
{
	int ret = 0;
	int i;
	struct vra_ctx *ctx = fh_to_vra_ctx(file->private_data);
	struct vra_dev *vra = ctx->vra_dev;
	struct vra_model_param *model_param;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;

	vra_dbg("[VRA]\n");
	BUG_ON(!ctx);

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		vra_dbg("ctrl ID:%d\n", ext_ctrl->id);
		switch (ext_ctrl->id) {
		case V4L2_CID_CAMERAPP_VRA_MODEL_CONTROL:
			ret = copy_from_user(&ctx->model_param, ext_ctrl->ptr, sizeof(struct vra_model_param));
			if (ret) {
				dev_err(vra->dev, "copy_from_user is fail(%d)\n", ret);
				goto p_err;
			}
			model_param = &ctx->model_param;
			vra_info("[VRA] instruction(%d, %d), constant(%d, %d), temporary(%d, %d), %dx%d\n",
					model_param->instruction.address, model_param->instruction.size,
					model_param->constant.address, model_param->constant.size,
					model_param->temporary.address, model_param->temporary.size,
					model_param->inputWidth, model_param->inputHeight);

			vra_info("[VRA] timeOut(%d), clockLevel(%d), clockMode(%d)\n",
					model_param->timeOut, model_param->clockLevel, model_param->clockMode);

			if (!model_param->clockLevel) {
				vra_info("[VRA] Use default clockLevel(%d)\n", VRA_CLOCK_MIN);
				model_param->clockLevel = VRA_CLOCK_MIN;
			}
			if (!model_param->timeOut) {
				vra_info("[VRA] Use default timeOut(%d)\n", VRA_WDT_CNT);
				model_param->timeOut = VRA_WDT_CNT;
			}

			ctx->i_addr = vra_get_pa_from_fd(vra, ctx, VRA_BUF_INSTRUCTION, model_param->instruction.address);
			ctx->c_addr = vra_get_pa_from_fd(vra, ctx, VRA_BUF_CONSTANT, model_param->constant.address);
			ctx->t_addr = vra_get_pa_from_fd(vra, ctx, VRA_BUF_TEMP, model_param->temporary.address);
			break;
		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = vra_v4l2_s_ctrl(file, ctx, &ctrl);
			if (ret) {
				vra_dbg("vra_v4l2_s_ctrl is fail(%d)\n", ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return ret;
}

static const struct v4l2_ioctl_ops vra_v4l2_ioctl_ops = {
	.vidioc_querycap		= vra_v4l2_querycap,

	.vidioc_g_fmt_vid_cap_mplane	= vra_v4l2_g_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane	= vra_v4l2_g_fmt_mplane,

	.vidioc_try_fmt_vid_cap_mplane	= vra_v4l2_try_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane	= vra_v4l2_try_fmt_mplane,

	.vidioc_s_fmt_vid_cap_mplane	= vra_v4l2_s_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= vra_v4l2_s_fmt_mplane,

	.vidioc_reqbufs			= vra_v4l2_reqbufs,

	.vidioc_qbuf			= vra_v4l2_qbuf,
	.vidioc_dqbuf			= vra_v4l2_dqbuf,

	.vidioc_streamon		= vra_v4l2_streamon,
	.vidioc_streamoff		= vra_v4l2_streamoff,

	.vidioc_s_ctrl			= vra_v4l2_s_ctrl,
	.vidioc_s_ext_ctrls		= vra_v4l2_s_ext_ctrls,
};

static int vra_ctx_stop_req(struct vra_ctx *ctx)
{
	struct vra_ctx *curr_ctx;
	struct vra_dev *vra = ctx->vra_dev;
	int ret = 0;

	vra_dbg("[VRA]\n");
	curr_ctx = v4l2_m2m_get_curr_priv(vra->m2m.m2m_dev);
	if (!test_bit(CTX_RUN, &ctx->flags) || (curr_ctx != ctx))
		return 0;

	set_bit(CTX_ABORT, &ctx->flags);

	ret = wait_event_timeout(vra->wait,
			!test_bit(CTX_RUN, &ctx->flags), VRA_TIMEOUT);

	/* TODO: How to handle case of timeout event */
	if (ret == 0) {
		dev_err(vra->dev, "device failed to stop request\n");
		ret = -EBUSY;
	}

	return ret;
}

static int vra_vb2_queue_setup(struct vb2_queue *vq,
		unsigned int *num_buffers, unsigned int *num_planes,
		unsigned int sizes[], struct device *alloc_devs[])
{
	struct vra_ctx *ctx = vb2_get_drv_priv(vq);
	struct vra_frame *frame;
	int i;

	vra_dbg("[VRA]\n");
	frame = ctx_get_frame(ctx, vq->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	/* Get number of planes from format_list in driver */
	*num_planes = frame->num_planes;
	for (i = 0; i < *num_planes; i++) {
		sizes[i] = frame->bytesused[i];
		alloc_devs[i] = ctx->vra_dev->dev;
	}

	return 0;
}

static int vra_vb2_buf_prepare(struct vb2_buffer *vb)
{
	struct vra_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vra_frame *frame;
	int i;

	vra_dbg("[VRA]\n");
	frame = ctx_get_frame(ctx, vb->vb2_queue->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		for (i = 0; i < frame->vra_fmt->num_planes; i++)
			vb2_set_plane_payload(vb, i, frame->bytesused[i]);
	}

	return 0;
}

static void vra_vb2_buf_finish(struct vb2_buffer *vb)
{
	vra_dbg("[VRA]\n");
	/* No operation */
}

static void vra_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct vra_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *v4l2_buf = to_vb2_v4l2_buffer(vb);

	vra_dbg("[VRA]\n");
	if (ctx->m2m_ctx)
		v4l2_m2m_buf_queue(ctx->m2m_ctx, v4l2_buf);
}

static void vra_vb2_buf_cleanup(struct vb2_buffer *vb)
{
	vra_dbg("[VRA]\n");
	/* No operation */
}

static void vra_vb2_lock(struct vb2_queue *vq)
{
	struct vra_ctx *ctx = vb2_get_drv_priv(vq);

	vra_dbg("[VRA]\n");
	mutex_lock(&ctx->vra_dev->lock);
}

static void vra_vb2_unlock(struct vb2_queue *vq)
{
	struct vra_ctx *ctx = vb2_get_drv_priv(vq);

	vra_dbg("[VRA]\n");
	mutex_unlock(&ctx->vra_dev->lock);
}

static void vra_cleanup_queue(struct vra_ctx *ctx)
{
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	vra_dbg("[VRA]\n");
	while (v4l2_m2m_num_src_bufs_ready(ctx->m2m_ctx) > 0) {
		src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
		vra_dbg("src_index(%d)\n", src_vb->vb2_buf.index);
	}

	while (v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx) > 0) {
		dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);
		vra_dbg("dst_index(%d)\n", dst_vb->vb2_buf.index);
	}
}

static int vra_vb2_start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct vra_ctx *ctx = vb2_get_drv_priv(vq);

	vra_dbg("[VRA]\n");
	set_bit(CTX_STREAMING, &ctx->flags);

	return 0;
}

static void vra_vb2_stop_streaming(struct vb2_queue *vq)
{
	struct vra_ctx *ctx = vb2_get_drv_priv(vq);
	int ret;

	vra_dbg("[VRA]\n");
	ret = vra_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->vra_dev->dev, "wait timeout\n");

	clear_bit(CTX_STREAMING, &ctx->flags);

	/* release all queued buffers in multi-buffer scenario*/
	vra_cleanup_queue(ctx);
}

static struct vb2_ops vra_vb2_ops = {
	.queue_setup		= vra_vb2_queue_setup,
	.buf_prepare		= vra_vb2_buf_prepare,
	.buf_finish			= vra_vb2_buf_finish,
	.buf_queue			= vra_vb2_buf_queue,
	.buf_cleanup		= vra_vb2_buf_cleanup,
	.wait_finish		= vra_vb2_lock,
	.wait_prepare		= vra_vb2_unlock,
	.start_streaming	= vra_vb2_start_streaming,
	.stop_streaming		= vra_vb2_stop_streaming,
};

static int queue_init(void *priv, struct vb2_queue *src_vq,
			struct vb2_queue *dst_vq)
{
	struct vra_ctx *ctx = priv;
	int ret;

	vra_dbg("[VRA]\n");
	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->ops = &vra_vb2_ops;
	src_vq->mem_ops = &vb2_dma_sg_memops;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct vb2_vra_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->ops = &vra_vb2_ops;
	dst_vq->mem_ops = &vb2_dma_sg_memops;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct vb2_vra_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	return vb2_queue_init(dst_vq);
}

static int vra_open(struct file *file)
{
	struct vra_dev *vra = video_drvdata(file);
	struct vra_ctx *ctx;
	int ret = 0;

	vra_info("[VRA]\n");
	ctx = vmalloc(sizeof(struct vra_ctx));

	if (!ctx) {
		dev_err(vra->dev, "no memory for open context\n");
		return -ENOMEM;
	}

	atomic_inc(&vra->m2m.in_use);

	INIT_LIST_HEAD(&ctx->node);
	ctx->vra_dev = vra;

	v4l2_fh_init(&ctx->fh, vra->m2m.vfd);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	/* Default color format */
	ctx->s_frame.vra_fmt = &vra_formats[0];
	ctx->d_frame.vra_fmt = &vra_formats[0];

	if (!IS_ERR(vra->aclk)) {
		ret = clk_prepare(vra->aclk);
		if (ret) {
			dev_err(vra->dev, "%s: failed to prepare ACLK(err %d)\n",
					__func__, ret);
			goto err_aclk_prepare;
		}
	}

	/* Setup the device context for mem2mem mode. */
	ctx->m2m_ctx = v4l2_m2m_ctx_init(vra->m2m.m2m_dev, ctx, queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		ret = -EINVAL;
		goto err_ctx;
	}

	vra_dbg("vra open = %d\n", ret);
	return 0;

err_ctx:
	if (!IS_ERR(vra->aclk))
		clk_unprepare(vra->aclk);
err_aclk_prepare:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	atomic_dec(&vra->m2m.in_use);
	vfree(ctx);

	return ret;
}

static int vra_release(struct file *file)
{
	struct vra_ctx *ctx = fh_to_vra_ctx(file->private_data);
	struct vra_dev *vra = ctx->vra_dev;

	atomic_dec(&vra->m2m.in_use);
	vra_info("[VRA] refcnt= %d", atomic_read(&vra->m2m.in_use));

	if (!atomic_read(&vra->m2m.in_use) && test_bit(DEV_RUN, &vra->state)) {
		dev_err(vra->dev, "%s, vra is still running\n", __func__);
		vra_suspend(vra->dev);
	}

	vra_free_all_pa(vra, ctx);

	if (!atomic_read(&vra->m2m.in_use) && test_bit(DEV_READY, &vra->state)) {
		vra_clk_power_disable(vra);
		clear_bit(DEV_READY, &vra->state);
	}

	vra->qosreq_m2m_level = VRA_CLOCK_MIN;

	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	if (!IS_ERR(vra->aclk))
		clk_unprepare(vra->aclk);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	vfree(ctx);

	return 0;
}

static const struct v4l2_file_operations vra_v4l2_fops = {
	.owner		= THIS_MODULE,
	.open		= vra_open,
	.release	= vra_release,
	.unlocked_ioctl	= video_ioctl2,
};

static void vra_job_finish(struct vra_dev *vra, struct vra_ctx *ctx)
{
	unsigned long flags;
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	vra_dbg("[VRA]\n");
	spin_lock_irqsave(&vra->slock, flags);

	ctx = v4l2_m2m_get_curr_priv(vra->m2m.m2m_dev);
	if (!ctx || !ctx->m2m_ctx) {
		dev_err(vra->dev, "current ctx is NULL\n");
		spin_unlock_irqrestore(&vra->slock, flags);
		return;
	}

	clear_bit(CTX_RUN, &ctx->flags);

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	BUG_ON(!src_vb || !dst_vb);

	v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
	v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);

	v4l2_m2m_job_finish(vra->m2m.m2m_dev, ctx->m2m_ctx);

	spin_unlock_irqrestore(&vra->slock, flags);
}

static void vra_watchdog(struct timer_list *t)
{
	struct vra_wdt *wdt = from_timer(wdt, t, timer);
	struct vra_dev *vra = container_of(wdt, typeof(*vra), wdt);
	struct vra_ctx *ctx;
	unsigned long flags;

	vra_dbg("[VRA]\n");
	if (!test_bit(DEV_RUN, &vra->state)) {
		vra_info("[ERR] VRA is not running\n");
		return;
	}

	spin_lock_irqsave(&vra->ctxlist_lock, flags);
	ctx = vra->current_ctx;
	if (!ctx) {
		vra_info("[ERR] ctx is empty\n");
		spin_unlock_irqrestore(&vra->ctxlist_lock, flags);
		return;
	}

	if (atomic_read(&vra->wdt.cnt) >= ctx->model_param.timeOut) {
		vra_info("[ERR] final timeout(cnt:%d)\n", ctx->model_param.timeOut);
		is_debug_s2d(true, "VRA watchdog s2d");
		if (camerapp_hw_vra_sw_reset(vra->regs_base))
			dev_err(vra->dev, "vra sw reset fail\n");

		atomic_set(&vra->wdt.cnt, 0);
		clear_bit(DEV_RUN, &vra->state);
		vra->current_ctx = NULL;
		spin_unlock_irqrestore(&vra->ctxlist_lock, flags);
		vra_job_finish(vra, ctx);
		return;
	}

	spin_unlock_irqrestore(&vra->ctxlist_lock, flags);

	if (test_bit(DEV_RUN, &vra->state)) {
#ifndef USE_VELOCE
		if (!atomic_read(&vra->wdt.cnt))
#endif
		{
			vra_info("[VRA] SFR Dump\n");
			camerapp_vra_sfr_dump(vra->regs_base);
		}
		atomic_inc(&vra->wdt.cnt);
		dev_err(vra->dev, "vra is still running(cnt:%d)\n", vra->wdt.cnt);
		mod_timer(&vra->wdt.timer, jiffies + VRA_TIMEOUT);
	} else {
		vra_dbg("vra finished job\n");
	}
}

static int vra_run_next_job(struct vra_dev *vra)
{
	unsigned long flags;
	struct vra_ctx *ctx;

	vra_dbg("[VRA]\n");
	spin_lock_irqsave(&vra->ctxlist_lock, flags);

	if (vra->current_ctx || list_empty(&vra->context_list)) {
		/* a job is currently being processed or no job is to run */
		spin_unlock_irqrestore(&vra->ctxlist_lock, flags);
		return 0;
	}

	ctx = list_first_entry(&vra->context_list, struct vra_ctx, node);

	list_del_init(&ctx->node);

	vra->current_ctx = ctx;

	spin_unlock_irqrestore(&vra->ctxlist_lock, flags);

	/*
	 * vra_run_next_job() must not reenter while vra->state is DEV_RUN.
	 * DEV_RUN is cleared when an operation is finished.
	 */
	BUG_ON(test_bit(DEV_RUN, &vra->state));

	camerapp_hw_vra_sw_init(vra->regs_base);

	camerapp_hw_vra_update_param(vra->regs_base, vra);

	set_bit(DEV_RUN, &vra->state);
	set_bit(CTX_RUN, &ctx->flags);
	mod_timer(&vra->wdt.timer, jiffies + VRA_TIMEOUT);

	camerapp_hw_vra_start(vra->regs_base);
	if (vra_get_debug_level() == VRA_DEBUG_MEASURE) {
		struct vb2_v4l2_buffer *vb =
			v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
		struct v4l2_m2m_buffer *mb =
			container_of(vb, typeof(*mb), vb);
		struct vb2_vra_buffer *svb =
			container_of(mb, typeof(*svb), mb);
		svb->ktime = ktime_get();
	}

	return 0;
}

static int vra_add_context_and_run(struct vra_dev *vra, struct vra_ctx *ctx)
{
	unsigned long flags;

	vra_dbg("[VRA]\n");
	spin_lock_irqsave(&vra->ctxlist_lock, flags);
	list_add_tail(&ctx->node, &vra->context_list);
	spin_unlock_irqrestore(&vra->ctxlist_lock, flags);

	return vra_run_next_job(vra);
}

static void vra_irq_finish(struct vra_dev *vra, struct vra_ctx *ctx, enum VRA_IRQ_DONE_TYPE type)
{
	struct vb2_v4l2_buffer *src_vb, *dst_vb;

	vra_dbg("[VRA]\n");
	clear_bit(DEV_RUN, &vra->state);
	del_timer(&vra->wdt.timer);
	atomic_set(&vra->wdt.cnt, 0);

	clear_bit(CTX_RUN, &ctx->flags);

	BUG_ON(ctx != v4l2_m2m_get_curr_priv(vra->m2m.m2m_dev));

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);

	BUG_ON(!src_vb || !dst_vb);

	if (type == VRA_IRQ_ERROR) {
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_ERROR);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_ERROR);

		/* Interrupt disable to ignore error */
		camerapp_hw_vra_interrupt_disable(vra->regs_base);
		camerapp_hw_vra_get_intr_status_and_clear(vra->regs_base);
	} else if (type == VRA_IRQ_FRAME_END) {
		if (vra_get_debug_level() == VRA_DEBUG_MEASURE) {
			struct v4l2_m2m_buffer *mb =
				container_of(dst_vb, typeof(*mb), vb);
			struct vb2_vra_buffer *svb =
				container_of(mb, typeof(*svb), mb);

			dst_vb->vb2_buf.timestamp =
				(__u32)ktime_us_delta(ktime_get(), svb->ktime);
			vra_info("vra_hw_latency %lld us\n",
					dst_vb->vb2_buf.timestamp);
		}
		v4l2_m2m_buf_done(src_vb, VB2_BUF_STATE_DONE);
		v4l2_m2m_buf_done(dst_vb, VB2_BUF_STATE_DONE);
	}

	/* Wake up from CTX_ABORT state */
	clear_bit(CTX_ABORT, &ctx->flags);

	spin_lock(&vra->ctxlist_lock);
	vra->current_ctx = NULL;
	spin_unlock(&vra->ctxlist_lock);

	v4l2_m2m_job_finish(vra->m2m.m2m_dev, ctx->m2m_ctx);

	vra_resume(vra->dev);
	wake_up(&vra->wait);
}

static irqreturn_t vra_irq_handler(int irq, void *priv)
{
	struct vra_dev *vra = priv;
	struct vra_ctx *ctx;
	u32 irq_status;

	vra_dbg("[VRA]\n");
	spin_lock(&vra->slock);
	irq_status = camerapp_hw_vra_get_intr_status_and_clear(vra->regs_base);

	ctx = vra->current_ctx;
	BUG_ON(!ctx);

	if (irq_status & camerapp_hw_vra_get_int_err()) {
		vra_info("[ERR] handle error interrupt -> job finish\n");
		vra_irq_finish(vra, ctx, VRA_IRQ_ERROR);
	} else if (irq_status & camerapp_hw_vra_get_int_frame_end()) {
		vra_dbg("[VRA] FRAME_END(0x%x)\n", irq_status);
		vra_irq_finish(vra, ctx, VRA_IRQ_FRAME_END);

	}
	spin_unlock(&vra->slock);

	return IRQ_HANDLED;
}

static void vra_fill_curr_frame(struct vra_dev *vra, struct vra_ctx *ctx)
{
	struct vra_frame *s_frame, *d_frame;
	struct vb2_buffer *src_vb = (struct vb2_buffer *)v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	struct vb2_buffer *dst_vb = (struct vb2_buffer *)v4l2_m2m_next_dst_buf(ctx->m2m_ctx);

#ifdef FILE_DUMP
	char *inBuf;
	static dumpInputCount = 0;
	char pname[128];
	int inBufSize = 0;
#endif

	vra_dbg("[VRA]\n");
	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	s_frame->addr.y = vra_get_dma_address(src_vb, 0);
	d_frame->addr.y = vra_get_dma_address(dst_vb, 0);

	vra_dbg("s_frame : addr = %pad, size = %d, w = %d, h = %d\n",
			&s_frame->addr.y, s_frame->pixel_size, s_frame->width, s_frame->height);
	vra_dbg("d_frame : addr = %pad, size = %d, w = %d, h = %d\n",
			&d_frame->addr.y, d_frame->pixel_size, d_frame->width, d_frame->height);

#ifdef FILE_DUMP
	if (vra_get_debug_level() == VRA_DEBUG_BUF_DUMP) {
		inBuf = vra_get_kvaddr(src_vb, 0);
		inBufSize = s_frame->width * s_frame->height * BGR_COLOR_CHANNELS;
		vra_dbg("kva - input : addr = %lx\n", (unsigned long)inBuf);
		snprintf(pname, sizeof(pname), "%s/%s_%03d.bin", VRA_DUMP_PATH, VRA_INPUT_DUMP, dumpInputCount);
		vra_dbg("dump input - start(path: %s, inBufSize: %d)", pname, inBufSize)
		write_file(pname, inBuf, (size_t)inBufSize);
		vra_dbg("dump input - end");
		dumpInputCount++;
	}
#endif
}

static void vra_m2m_device_run(void *priv)
{
	struct vra_ctx *ctx = priv;
	struct vra_dev *vra = ctx->vra_dev;

	vra_dbg("[VRA]\n");
	if (test_bit(DEV_SUSPEND, &vra->state)) {
		dev_err(vra->dev, "VRA is in suspend state\n");
		return;
	}

	if (test_bit(CTX_ABORT, &ctx->flags)) {
		dev_err(vra->dev, "aborted VRA device run\n");
		return;
	}

	vra_fill_curr_frame(vra, ctx);

	vra_add_context_and_run(vra, ctx);
}

static void vra_m2m_job_abort(void *priv)
{
	struct vra_ctx *ctx = priv;
	int ret;

	vra_dbg("[VRA]\n");
	ret = vra_ctx_stop_req(ctx);
	if (ret < 0)
		dev_err(ctx->vra_dev->dev, "wait timeout\n");
}

static struct v4l2_m2m_ops vra_m2m_ops = {
	.device_run	= vra_m2m_device_run,
	.job_abort	= vra_m2m_job_abort,
};

static void vra_unregister_m2m_device(struct vra_dev *vra)
{

	vra_dbg("[VRA]\n");
	v4l2_m2m_release(vra->m2m.m2m_dev);
	video_device_release(vra->m2m.vfd);
	v4l2_device_unregister(&vra->m2m.v4l2_dev);
}

static int vra_register_m2m_device(struct vra_dev *vra, int dev_id)
{
	struct v4l2_device *v4l2_dev;
	struct device *dev;
	struct video_device *vfd;
	int ret = 0;

	vra_dbg("[VRA]\n");
	dev = vra->dev;
	v4l2_dev = &vra->m2m.v4l2_dev;

	scnprintf(v4l2_dev->name, sizeof(v4l2_dev->name), "%s.m2m",
			VRA_MODULE_NAME);

	ret = v4l2_device_register(dev, v4l2_dev);
	if (ret) {
		dev_err(vra->dev, "failed to register v4l2 device\n");
		return ret;
	}

	vfd = video_device_alloc();
	if (!vfd) {
		dev_err(vra->dev, "failed to allocate video device\n");
		goto err_v4l2_dev;
	}

	vfd->fops	= &vra_v4l2_fops;
	vfd->ioctl_ops	= &vra_v4l2_ioctl_ops;
	vfd->release	= video_device_release;
	vfd->lock	= &vra->lock;
	vfd->vfl_dir	= VFL_DIR_M2M;
	vfd->v4l2_dev	= v4l2_dev;
	vfd->device_caps = V4L2_CAP_VIDEO_M2M_MPLANE;
	scnprintf(vfd->name, sizeof(vfd->name), "%s:m2m", VRA_MODULE_NAME);

	video_set_drvdata(vfd, vra);

	vra->m2m.vfd = vfd;
	vra->m2m.m2m_dev = v4l2_m2m_init(&vra_m2m_ops);
	if (IS_ERR(vra->m2m.m2m_dev)) {
		dev_err(vra->dev, "failed to initialize v4l2-m2m device\n");
		ret = PTR_ERR(vra->m2m.m2m_dev);
		goto err_dev_alloc;
	}

	ret = video_register_device(vfd, VFL_TYPE_PABLO,
				EXYNOS_VIDEONODE_CAMERAPP(CAMERAPP_VIDEONODE_VRA));
	if (ret) {
		dev_err(vra->dev, "failed to register video device\n");
		goto err_m2m_dev;
	}

	return 0;

err_m2m_dev:
	v4l2_m2m_release(vra->m2m.m2m_dev);
err_dev_alloc:
	video_device_release(vra->m2m.vfd);
err_v4l2_dev:
	v4l2_device_unregister(v4l2_dev);

	return ret;
}
#ifdef CONFIG_EXYNOS_IOVMM
static int __attribute__((unused)) vra_sysmmu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token)
{
	struct vra_dev *vra = dev_get_drvdata(dev);
#else
static int vra_sysmmu_fault_handler(struct iommu_fault *fault, void *data)
{
	struct vra_dev *vra = data;
	struct device *dev = vra->dev;
	unsigned long iova = fault->event.addr;
#endif

	vra_dbg("[VRA]\n");
	if (test_bit(DEV_RUN, &vra->state)) {
		dev_info(dev, "System MMU fault called for IOVA %#lx\n", iova);
		camerapp_vra_sfr_dump(vra->regs_base);
	}

	return 0;
}

static int vra_clk_get(struct vra_dev *vra)
{

	vra_dbg("[VRA]\n");
	vra->aclk = devm_clk_get(vra->dev, "gate");
	if (IS_ERR(vra->aclk)) {
		if (PTR_ERR(vra->aclk) != -ENOENT) {
			dev_err(vra->dev, "Failed to get 'gate' clock: %ld",
				PTR_ERR(vra->aclk));
			return PTR_ERR(vra->aclk);
		}
		dev_info(vra->dev, "'gate' clock is not present\n");
	}

	return 0;
}

static void vra_clk_put(struct vra_dev *vra)
{
	vra_dbg("[VRA]\n");
	if (!IS_ERR(vra->aclk))
		devm_clk_put(vra->dev, vra->aclk);
}

#ifdef CONFIG_PM_SLEEP
static int vra_suspend(struct device *dev)
{
	struct vra_dev *vra = dev_get_drvdata(dev);
	int ret;

	vra_dbg("[VRA]\n");
	set_bit(DEV_SUSPEND, &vra->state);

	ret = wait_event_timeout(vra->wait,
			!test_bit(DEV_RUN, &vra->state), VRA_TIMEOUT * 50); /* 2sec */
	if (ret == 0)
		dev_err(vra->dev, "wait timeout\n");

	return 0;
}

static int vra_resume(struct device *dev)
{
	struct vra_dev *vra = dev_get_drvdata(dev);

	vra_dbg("[VRA]\n");
	clear_bit(DEV_SUSPEND, &vra->state);

	return 0;
}
#endif

#ifdef CONFIG_PM
static int vra_runtime_resume(struct device *dev)
{
	struct vra_dev *vra = dev_get_drvdata(dev);

	vra_info("[VRA] M2M clock set: %d\n", vra->qosreq_m2m_level);
	vra_pm_qos_update_request(&vra->qosreq_m2m, vra->qosreq_m2m_level);

	return 0;
}

static int vra_runtime_suspend(struct device *dev)
{
	struct vra_dev *vra = dev_get_drvdata(dev);

	vra_info("[VRA] M2M clock set: free\n");
	vra_pm_qos_update_request(&vra->qosreq_m2m, 0);

	return 0;
}
#endif

static const struct dev_pm_ops vra_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(vra_suspend, vra_resume)
	SET_RUNTIME_PM_OPS(NULL, vra_runtime_resume, vra_runtime_suspend)
};

static int vra_probe(struct platform_device *pdev)
{
	struct vra_dev *vra;
	struct resource *rsc;
	struct device_node *np;
	int ret = 0;

	vra_info("[VRA]\n");
	vra = devm_kzalloc(&pdev->dev, sizeof(struct vra_dev), GFP_KERNEL);
	if (!vra) {
		dev_err(&pdev->dev, "no memory for VRA device\n");
		return -ENOMEM;
	}

	vra->dev = &pdev->dev;
	np = vra->dev->of_node;

	vra_device = &pdev->dev;
	dev_set_drvdata(vra_device, vra);

	spin_lock_init(&vra->ctxlist_lock);
	INIT_LIST_HEAD(&vra->context_list);
	spin_lock_init(&vra->slock);
	mutex_init(&vra->lock);
	init_waitqueue_head(&vra->wait);

	rsc = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!rsc) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		ret = -ENOMEM;
		goto err_get_mem_res;
	}
	vra->regs_base = devm_ioremap(&pdev->dev, rsc->start, resource_size(rsc));
	if (IS_ERR_OR_NULL(vra->regs_base)) {
		dev_err(&pdev->dev, "Failed to ioremap for reg_base\n");
		ret = ENOMEM;
		goto err_get_mem_res;
	}

	rsc = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!rsc) {
		dev_err(&pdev->dev, "failed to get IRQ resource\n");
		ret = ENOMEM;
		goto err_get_irq_res;
	}

	vra->irq = rsc->start;
	ret = devm_request_irq(&pdev->dev, vra->irq, vra_irq_handler, 0,
			"VRA0", vra);
	if (ret) {
		dev_err(&pdev->dev, "failed to install irq\n");
		goto err_get_irq_res;
	}

	atomic_set(&vra->wdt.cnt, 0);
	timer_setup(&vra->wdt.timer, vra_watchdog, 0);

	ret = vra_clk_get(vra);
	if (ret)
		goto err_wq;

	if (pdev->dev.of_node)
		vra->dev_id = of_alias_get_id(pdev->dev.of_node, "camerapp-vra");
	else
		vra->dev_id = pdev->id;

	platform_set_drvdata(pdev, vra);

	pm_runtime_enable(&pdev->dev);

	ret = vra_register_m2m_device(vra, vra->dev_id);
	if (ret) {
		dev_err(&pdev->dev, "failed to register m2m device\n");
		goto err_get_irq_res;
	}

	vra_dbg("[VRA] vra_pm_qos_add_request(PM_QOS_M2M_THROUGHPUT:%d)\n", PM_QOS_M2M_THROUGHPUT);
	vra->qosreq_m2m_level = VRA_CLOCK_MIN;
	vra_pm_qos_add_request(&vra->qosreq_m2m, PM_QOS_M2M_THROUGHPUT, 0);

#ifdef CONFIG_EXYNOS_IOVMM
	ret = iovmm_activate(vra->dev);
	if (ret) {
		dev_err(&pdev->dev, "failed to attach iommu\n");
		goto err_iommu;
	}
#endif
	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "%s: failed to local power on (err %d)\n",
			__func__, ret);
		goto err_ver_rpm_get;
	}

	if (!IS_ERR(vra->aclk)) {
		ret = clk_prepare_enable(vra->aclk);
		if (ret) {
			dev_err(&pdev->dev,
				"%s: failed to enable ACLK (err %d)\n",
				__func__, ret);
			goto err_ver_aclk_get;
		}
	}

	vra->version = camerapp_hw_vra_get_ver(vra->regs_base);
	vra->variant = camerapp_hw_vra_get_size_constraints(vra->regs_base);

	if (!IS_ERR(vra->aclk))
		clk_disable_unprepare(vra->aclk);
	pm_runtime_put(&pdev->dev);
#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_set_fault_handler(&pdev->dev, vra_sysmmu_fault_handler, vra);
#else
	iommu_register_device_fault_handler(&pdev->dev, vra_sysmmu_fault_handler, vra);
#endif
	dev_info(&pdev->dev,
		"Driver probed successfully(version: %08x)\n",
		vra->version);

	return 0;

err_ver_aclk_get:
	pm_runtime_put(&pdev->dev);
err_ver_rpm_get:
#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_deactivate(vra->dev);
err_iommu:
#endif
	vra_pm_qos_remove_request(&vra->qosreq_m2m);
	vra_unregister_m2m_device(vra);
err_wq:
	if (vra->irq)
		devm_free_irq(&pdev->dev, vra->irq, vra);
err_get_irq_res:
	devm_iounmap(&pdev->dev, vra->regs_base);
err_get_mem_res:
	devm_kfree(&pdev->dev, vra);

	return ret;
}

static int vra_remove(struct platform_device *pdev)
{
	struct vra_dev *vra = platform_get_drvdata(pdev);

	vra_dbg("[VRA]\n");
#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_deactivate(vra->dev);
#else
	iommu_unregister_device_fault_handler(&pdev->dev);
#endif
	vra_clk_put(vra);

	if (timer_pending(&vra->wdt.timer))
		del_timer(&vra->wdt.timer);

	vra_pm_qos_remove_request(&vra->qosreq_m2m);

	devm_free_irq(&pdev->dev, vra->irq, vra);
	devm_iounmap(&pdev->dev, vra->regs_base);
	devm_kfree(&pdev->dev, vra);

	return 0;
}

static void vra_shutdown(struct platform_device *pdev)
{
	struct vra_dev *vra = platform_get_drvdata(pdev);

	vra_dbg("[VRA]\n");
	set_bit(DEV_SUSPEND, &vra->state);

	wait_event(vra->wait,
			!test_bit(DEV_RUN, &vra->state));

#ifdef CONFIG_EXYNOS_IOVMM
	iovmm_deactivate(vra->dev);
#endif
}

static const struct of_device_id exynos_vra_match[] = {
	{
		.compatible = "samsung,exynos-is-vra",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_vra_match);

static struct platform_driver vra_driver = {
	.probe		= vra_probe,
	.remove		= vra_remove,
	.shutdown	= vra_shutdown,
	.driver = {
		.name	= VRA_MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &vra_pm_ops,
		.of_match_table = of_match_ptr(exynos_vra_match),
	}
};
builtin_platform_driver(vra_driver);

MODULE_AUTHOR("SamsungLSI Camera");
MODULE_DESCRIPTION("EXYNOS CameraPP VRA driver");
MODULE_LICENSE("GPL");
