/*
* ispvideo.c
*
* Marvell DxO ISP - video node
*	Based on omap3isp
*
*
* Copyright:  (C) Copyright 2011 Marvell International Ltd.
*			   Henry Zhao <xzhao10@marvell.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*/


#include <asm/cacheflush.h>
#include <linux/clk.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include "isp.h"
#include "ispvideo.h"

static struct isp_format_convert_info formats[] = {
	{ V4L2_MBUS_FMT_SBGGR10_1X10, V4L2_PIX_FMT_SBGGR10, 10, 1},
	{ V4L2_MBUS_FMT_UYVY8_1X16, V4L2_PIX_FMT_UYVY, 16, 1},
	{ V4L2_MBUS_FMT_Y12_1X12, V4L2_PIX_FMT_YUV420M, 12, 3},
	{ V4L2_MBUS_FMT_SBGGR8_1X8, V4L2_PIX_FMT_SBGGR8, 8, 1},
};

void set_vd_dmaqueue_flg(struct isp_video *video,
	enum isp_video_dmaqueue_flags flag_val)
{
	video->dmaqueue_flags = flag_val;
}

enum isp_video_dmaqueue_flags
	get_vd_dmaqueue_flg(struct isp_video *video)
{
	unsigned long flags;
	enum isp_video_dmaqueue_flags dma_flags;

	spin_lock_irqsave(&video->irq_lock, flags);
	dma_flags = video->dmaqueue_flags;
	spin_unlock_irqrestore(&video->irq_lock, flags);

	return dma_flags;
}

static int isp_video_calc_mplane_sizeimage(
		struct v4l2_pix_format_mplane *pix_mp, int idx)
{
	unsigned int width = pix_mp->width;
	unsigned int height = pix_mp->height;
	unsigned int pitch;
	int ret = 0;

	switch (pix_mp->pixelformat) {
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SBGGR8:
		pitch = formats[idx].bpp * width >> 3;
		pix_mp->plane_fmt[0].bytesperline = pitch;
		pix_mp->plane_fmt[0].sizeimage = pitch * height;
		break;
	case V4L2_PIX_FMT_YUV420M:
		pitch = width;
		pix_mp->plane_fmt[0].bytesperline = pitch;
		pix_mp->plane_fmt[0].sizeimage = pitch * height;

		pitch = width >> 1;
		pix_mp->plane_fmt[1].bytesperline = pitch;
		pix_mp->plane_fmt[1].sizeimage =  pitch * height / 2;

		pix_mp->plane_fmt[2].bytesperline = pitch;
		pix_mp->plane_fmt[2].sizeimage = pitch * height / 2;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int isp_video_mbus_to_pix(const struct isp_video *video,
					  const struct v4l2_mbus_framefmt *mbus,
					  struct v4l2_pix_format_mplane *pix_mp)
{
	unsigned int i;

	memset(pix_mp, 0, sizeof(*pix_mp));

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (formats[i].code == mbus->code)
			break;
	}
	if (WARN_ON(i >= ARRAY_SIZE(formats)))
		return -EINVAL;

	pix_mp->width = mbus->width;
	pix_mp->height = mbus->height;
	pix_mp->pixelformat = formats[i].pixelformat;
	pix_mp->colorspace = mbus->colorspace;
	pix_mp->field = mbus->field;
	pix_mp->num_planes = formats[i].num_planes;

	return isp_video_calc_mplane_sizeimage(pix_mp, i);
}

static int isp_video_pix_to_mbus(struct v4l2_pix_format_mplane *pix_mp,
				  struct v4l2_mbus_framefmt *mbus)
{
	unsigned int i;

	memset(mbus, 0, sizeof(*mbus));

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (formats[i].pixelformat == pix_mp->pixelformat)
			break;
	}

	if (WARN_ON(i >= ARRAY_SIZE(formats)))
		return -EINVAL;

	if (formats[i].num_planes != pix_mp->num_planes)
		return -EINVAL;

	mbus->code = formats[i].code;
	mbus->width = pix_mp->width;
	mbus->height = pix_mp->height;
	mbus->colorspace = pix_mp->colorspace;
	mbus->field = pix_mp->field;

	return 0;
}

static struct v4l2_subdev *
isp_video_remote_subdev(struct isp_video *video, u32 *pad)
{
	struct media_pad *remote;

	remote = media_entity_remote_source(&video->pad);

	if (remote == NULL ||
	    media_entity_type(remote->entity) != MEDIA_ENT_T_V4L2_SUBDEV)
		return NULL;

	if (pad)
		*pad = remote->index;

	return media_entity_to_v4l2_subdev(remote->entity);
}

static int isp_video_far_end(struct isp_video **video_array
			, struct isp_video *video)
{
	struct media_entity_graph graph;
	struct media_entity *entity = &video->video.entity;
	struct media_device *mdev = entity->parent;
	struct isp_video *far_end_walk = NULL;
	struct isp_video *far_end[FAR_END_MAX_NUM];
	int cnt;

	mutex_lock(&mdev->graph_mutex);
	media_entity_graph_walk_start(&graph, entity);
	for (cnt = 0; cnt < FAR_END_MAX_NUM; cnt++)
		far_end[cnt] = NULL;
	cnt = 0;
	while ((entity = media_entity_graph_walk_next(&graph))) {
		if (entity == &video->video.entity)
			continue;

		if (media_entity_type(entity) != MEDIA_ENT_T_DEVNODE)
			continue;

		far_end_walk = to_isp_video
				(media_entity_to_video_device(entity));
		if ((far_end_walk->type != video->type)
			&& (cnt < FAR_END_MAX_NUM)) {
			switch (far_end_walk->video_type) {
			case ISP_VIDEO_DISPLAY:
				far_end[FAR_END_ISP_DISPLAY] = far_end_walk;
				cnt++;
				break;
			case ISP_VIDEO_CODEC:
				far_end[FAR_END_ISP_CODEC] = far_end_walk;
				cnt++;
				break;
			case ISP_VIDEO_INPUT:
				far_end[FAR_END_ISP_INPUT] = far_end_walk;
				cnt++;
				break;
			case ISP_VIDEO_CCIC:
				far_end[FAR_END_CCIC] = far_end_walk;
				cnt++;
				break;
			default:
				break;
			}
		}
		far_end_walk = NULL;
	}

	memcpy(video_array, far_end,
			sizeof(struct isp_video *) * FAR_END_MAX_NUM);
	mutex_unlock(&mdev->graph_mutex);
	return cnt;
}

static int isp_video_validate_pipeline(struct isp_pipeline *pipe, int index)
{
	struct v4l2_subdev_format fmt_source;
	struct v4l2_subdev_format fmt_sink;
	struct media_pad *pad;
	struct v4l2_subdev *subdev;
	int ret, max_loop;

	if (pipe->output[index] == NULL)
		return 0;

	subdev = isp_video_remote_subdev(pipe->output[index], NULL);
	if (subdev == NULL)
		return -EPIPE;

	max_loop = 100;
	while (max_loop > 0) {
		/* Retrieve the sink format */
		pad = &subdev->entity.pads[0];
		if (!(pad->flags & MEDIA_PAD_FL_SINK))
			break;

		fmt_sink.pad = pad->index;
		fmt_sink.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		ret = v4l2_subdev_call(subdev, pad, get_fmt, NULL, &fmt_sink);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EPIPE;

		/* Retrieve the source format */
		pad = media_entity_remote_source(pad);
		if (pad == NULL ||
		    media_entity_type(pad->entity) != MEDIA_ENT_T_V4L2_SUBDEV)
			break;

		subdev = media_entity_to_v4l2_subdev(pad->entity);

		fmt_source.pad = pad->index;
		fmt_source.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		ret = v4l2_subdev_call(subdev, pad, get_fmt, NULL, &fmt_source);
		if (ret < 0 && ret != -ENOIOCTLCMD)
			return -EPIPE;

		/* Check if the two ends match */
		if (fmt_source.format.code != fmt_sink.format.code ||
		    fmt_source.format.width != fmt_sink.format.width ||
		    fmt_source.format.height != fmt_sink.format.height)
			return -EPIPE;

		max_loop--;
	}

	if (max_loop == 0)
		return -EPIPE;

	return 0;
}

static int
isp_video_get_subdev_format(struct isp_video *video, struct v4l2_format *format)
{
	struct v4l2_subdev_format fmt;
	struct v4l2_subdev *subdev;
	u32 pad;
	int ret;

	subdev = isp_video_remote_subdev(video, &pad);
	if (subdev == NULL)
		return -EINVAL;

	mutex_lock(&video->fmt_lock);

	fmt.pad = pad;
	fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;

	ret = v4l2_subdev_call(subdev, pad, get_fmt, NULL, &fmt);
	if (ret == -ENOIOCTLCMD)
		ret = -EINVAL;

	mutex_unlock(&video->fmt_lock);

	if (ret)
		return ret;

	format->type = video->type;
	return isp_video_mbus_to_pix(video, &fmt.format, &format->fmt.pix_mp);
}

static int
isp_video_check_format(struct isp_video *video, struct isp_video_fh *vfh)
{
	struct v4l2_format format;
	struct v4l2_pix_format_mplane *pix_mp;
	int ret;

	memcpy(&format, &vfh->format, sizeof(format));
	ret = isp_video_get_subdev_format(video, &format);
	if (ret < 0)
		return ret;

	pix_mp = &vfh->format.fmt.pix_mp;

	if (pix_mp->pixelformat != format.fmt.pix_mp.pixelformat ||
		pix_mp->num_planes != format.fmt.pix_mp.num_planes ||
		pix_mp->height != format.fmt.pix_mp.height ||
		pix_mp->width != format.fmt.pix_mp.width)
		return -EINVAL;

	return 0;
}

static int isp_video_vb2_buf_init(struct vb2_buffer *vb)
{
	struct isp_video_buffer *buf =
		container_of(vb, struct isp_video_buffer, vb2_buf);
	int i;

	for (i = 0; i < ISP_BUF_MAX_PADDR; i++)
		buf->paddr[i] = 0;

	buf->state = ISP_BUF_STATE_IDLE;

	return 0;
}

static int isp_video_vb2_queue_setup(struct vb2_queue *vq,
			const struct v4l2_format *fmt,
			unsigned int *num_buffers,
			unsigned int *num_planes, unsigned int sizes[],
			void *alloc_ctxs[])
{
	struct isp_video_fh *isp_video_vfh =
		vb2_queue_to_isp_video_fh(vq);
	struct v4l2_pix_format_mplane *pix_mp =
		&isp_video_vfh->format.fmt.pix_mp;
	struct isp_video *isp_video = isp_video_vfh->video;
	int i;

	*num_buffers = min_t(unsigned int,
			*num_buffers, ISP_VIDEO_MAX_BUFFERS);

	*num_planes = pix_mp->num_planes;
	for (i = 0; i < *num_planes; i++) {
		sizes[i] = pix_mp->plane_fmt[i].sizeimage;
		if (sizes[i] <= 0)
			return -EINVAL;

		alloc_ctxs[i] = isp_video->vb_alloc_ctx;
	}

	return 0;
}

static void isp_video_vb2_buf_cleanup(struct vb2_buffer *vb)
{
	return;
}

static int isp_video_vb2_buf_prepare(struct vb2_buffer *vb)
{
	unsigned long size;
	int i;

	for (i = 0; i < vb->num_planes; i++) {
		size = vb2_plane_size(vb, i);
		vb2_set_plane_payload(vb, i, size);
	}

	return 0;
}

static void isp_video_vb2_buf_queue(struct vb2_buffer *vb)
{
	struct isp_video_buffer *buf =
		container_of(vb, struct isp_video_buffer, vb2_buf);
	struct isp_video_fh *isp_video_vfh =
		vb2_queue_to_isp_video_fh(vb->vb2_queue);
	struct isp_video *video = isp_video_vfh->video;
	struct isp_pipeline *pipe = to_isp_pipeline(&video->video.entity);
	enum isp_pipeline_state state;
	dma_addr_t dma_handle;
	unsigned num;
	int i;

	num = min_t(unsigned int, buf->vb2_buf.num_planes, ISP_BUF_MAX_PADDR);
	for(i = 0; i < num; i++) {
		dma_handle = vb2_dma_contig_plane_dma_addr(&buf->vb2_buf, i);
		BUG_ON(!dma_handle);
		buf->paddr[i] = dma_handle;
	}

	if (isp_buf_type_is_capture_mp(video->type)) {
		switch (video->video_type) {
		case ISP_VIDEO_DISPLAY:
			state = ISP_PIPELINE_DISPLAY_QUEUED;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
			PEG(video->isp->mcd_root_display.mcd,
				MCD_VDEV, MCD_VDEV_QBUF, 1);
#endif
			break;
		case ISP_VIDEO_CODEC:
			state = ISP_PIPELINE_CODEC_QUEUED;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
			PEG(video->isp->mcd_root_codec.mcd,
				MCD_VDEV, MCD_VDEV_QBUF, 1);
#endif
			break;
		case ISP_VIDEO_CCIC:
			state = ISP_PIPELINE_CCIC_QUEUED;
			break;
		default:
			state = 0;
			break;
		}
	} else
		state = ISP_PIPELINE_INPUT_QUEUED;

	pipe->state |= state;

	buf->state = ISP_BUF_STATE_IDLE;

	list_add_tail(&buf->dmalist, &video->dmaidlequeue);
	video->dmaidlecnt++;

	/* Notify the subdev of qbuf event */
	if (video->streaming && video->ops->qbuf_notify)
		video->ops->qbuf_notify(video);

	return;
}

static int isp_video_vb2_start_streaming(struct vb2_queue *vq,
		unsigned int count)
{
	return 0;
}

static int isp_video_vb2_stop_streaming(struct vb2_queue *vq)
{
	return 0;
}

void isp_video_vb2_lock(struct vb2_queue *vq)
{
	struct isp_video *video = vb2_get_drv_priv(vq);

	mutex_lock(&video->video_lock);

	return;
}

void isp_video_vb2_unlock(struct vb2_queue *vq)
{
	struct isp_video *video = vb2_get_drv_priv(vq);

	mutex_unlock(&video->video_lock);

	return;
}

static struct vb2_ops isp_video_vb2_ops = {
	.queue_setup = isp_video_vb2_queue_setup,
	.buf_prepare = isp_video_vb2_buf_prepare,
	.buf_queue = isp_video_vb2_buf_queue,
	.buf_cleanup = isp_video_vb2_buf_cleanup,
	.buf_init = isp_video_vb2_buf_init,
	.start_streaming = isp_video_vb2_start_streaming,
	.stop_streaming = isp_video_vb2_stop_streaming,
	.wait_prepare = isp_video_vb2_unlock,
	.wait_finish = isp_video_vb2_lock,
};

static int isp_video_vb2_init(struct vb2_queue *vq,
				   struct isp_video *video)
{
	vq->type = video->type;
	vq->io_modes = VB2_USERPTR | VB2_MMAP;
	vq->drv_priv = video;
	vq->ops = &isp_video_vb2_ops;
	vq->mem_ops = &vb2_dma_contig_memops;
	vq->buf_struct_size = sizeof(struct isp_video_buffer);

	video->vb_alloc_ctx = vb2_dma_contig_init_ctx(&video->video.dev);

	return vb2_queue_init(vq);
}


struct isp_video_buffer *mvisp_video_get_next_work_buf(
	struct isp_video *video)
{
	struct isp_video_buffer *buf;
	struct isp_pipeline *pipe = to_isp_pipeline(&video->video.entity);
	enum isp_pipeline_state state;
	unsigned long flags;

	spin_lock_irqsave(&video->irq_lock, flags);

	if (list_empty(&video->dmaidlequeue)) {
		spin_unlock_irqrestore(&video->irq_lock, flags);
		return NULL;
	}

	if (isp_buf_type_is_capture_mp(video->type)) {
		switch (video->video_type) {
		case ISP_VIDEO_DISPLAY:
			state = ISP_PIPELINE_DISPLAY_QUEUED;
			break;
		case ISP_VIDEO_CODEC:
			state = ISP_PIPELINE_CODEC_QUEUED;
			break;
		case ISP_VIDEO_CCIC:
			state = ISP_PIPELINE_CCIC_QUEUED;
			break;
		default:
			state = 0;
			break;
		}
	} else
		state = ISP_PIPELINE_INPUT_QUEUED;

	buf = list_first_entry(&video->dmaidlequeue, struct isp_video_buffer,
				   dmalist);
	list_del(&buf->dmalist);
	video->dmaidlecnt--;

	list_add_tail(&buf->dmalist, &video->dmabusyqueue);
	video->dmabusycnt++;

	if (list_empty(&video->dmaidlequeue))
		pipe->state &= ~state;

	spin_unlock_irqrestore(&video->irq_lock, flags);

	return buf;
}

enum isp_pipeline_start_condition isp_pipeline_ready(struct isp_pipeline *pipe)
{
	enum isp_pipeline_start_condition condition = ISP_CAN_NOT_START;
	enum isp_pipeline_state state;

	state = pipe->state;

	if ((state & ISP_PIPELINE_INPUT_STREAM) &&
	(state & ISP_PIPELINE_INPUT_QUEUED) != 0) {
		condition |= ISP_INPUT_CAN_START;
	}

	if ((state & ISP_PIPELINE_DISPLAY_STREAM) &&
	(state & ISP_PIPELINE_DISPLAY_QUEUED) != 0) {
		condition |= ISP_DISPLAY_CAN_START;
	}

	if ((state & ISP_PIPELINE_CODEC_STREAM) &&
	(state & ISP_PIPELINE_CODEC_QUEUED) != 0) {
		condition |= ISP_CODEC_CAN_START;
	}

	return condition;
}

struct isp_video_buffer *mvisp_video_buffer_next(struct isp_video *video,
					      unsigned int error)
{
	struct isp_pipeline *pipe = to_isp_pipeline(&video->video.entity);
	enum isp_pipeline_state state;
	struct isp_video_buffer *buf;
	struct timespec ts;
	unsigned int min_drvbuf_cnt;
	unsigned long flags;

	spin_lock_irqsave(&video->irq_lock, flags);

	if (WARN_ON(list_empty(&video->dmabusyqueue))) {
		spin_unlock_irqrestore(&video->irq_lock, flags);
		return NULL;
	}

	buf = list_first_entry(&video->dmabusyqueue, struct isp_video_buffer,
			       dmalist);

	if (buf->state == ISP_BUF_STATE_ACTIVE) {
		ktime_get_ts(&ts);
		buf->vb2_buf.v4l2_buf.timestamp.tv_sec = ts.tv_sec;
		buf->vb2_buf.v4l2_buf.timestamp.tv_usec =
			ts.tv_nsec / NSEC_PER_USEC;

		if (video == pipe->output[0] || video == pipe->output[1])
			buf->vb2_buf.v4l2_buf.sequence =
				atomic_inc_return(&pipe->frame_number);
		else
			buf->vb2_buf.v4l2_buf.sequence =
				atomic_read(&pipe->frame_number);

		if (video->capture_mode == ISPVIDEO_STILL_CAPTURE)
			min_drvbuf_cnt = 0;
		else
			min_drvbuf_cnt = MIN_DRV_BUF;

		if ((video->dmabusycnt + video->dmaidlecnt) > min_drvbuf_cnt) {
			list_del(&buf->dmalist);
			video->dmabusycnt--;
			if (error) {
				buf->state = ISP_BUF_STATE_ERROR;
				vb2_buffer_done(&buf->vb2_buf,
					VB2_BUF_STATE_ERROR);
			} else {
				buf->state = ISP_BUF_STATE_DONE;
				vb2_buffer_done(&buf->vb2_buf,
					VB2_BUF_STATE_DONE);
			}
		}
	}

	if (list_empty(&video->dmaidlequeue)) {
		if (list_empty(&video->dmabusyqueue))
			buf = NULL;
		else {
			buf = list_first_entry(&video->dmabusyqueue,
					struct isp_video_buffer, dmalist);
			if (video->dmabusycnt > 1)
				list_move_tail(&buf->dmalist,
					&video->dmabusyqueue);
		}
	} else {
		buf = list_first_entry(&video->dmaidlequeue,
				struct isp_video_buffer,
				dmalist);
		list_del(&buf->dmalist);
		video->dmaidlecnt--;
		list_add_tail(&buf->dmalist, &video->dmabusyqueue);
		video->dmabusycnt++;
		set_vd_dmaqueue_flg(video, ISP_VIDEO_DMAQUEUE_QUEUED);
	}

	if (buf == NULL) {
		if (isp_buf_type_is_capture_mp(video->type)) {
			switch (video->video_type) {
			case ISP_VIDEO_DISPLAY:
				state = ISP_PIPELINE_DISPLAY_QUEUED;
				break;
			case ISP_VIDEO_CODEC:
				state = ISP_PIPELINE_CODEC_QUEUED;
				break;
			case ISP_VIDEO_CCIC:
				state = ISP_PIPELINE_CCIC_QUEUED;
				break;
			default:
				state = 0;
				break;
			}
		} else
			state = ISP_PIPELINE_INPUT_QUEUED;

		pipe->state &= ~state;

		set_vd_dmaqueue_flg(video, ISP_VIDEO_DMAQUEUE_UNDERRUN);
	}

	spin_unlock_irqrestore(&video->irq_lock, flags);

	return buf;
}

void mvisp_video_resume(struct isp_video *video, int continuous)
{
}

/* -----------------------------------------------------------------------------
 * V4L2 ioctls
 */

static int
isp_video_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	struct isp_video *video = video_drvdata(file);

	strlcpy(cap->driver, ISP_VIDEO_DRIVER_NAME, sizeof(cap->driver));
	strlcpy(cap->card, video->video.name, sizeof(cap->card));
	strlcpy(cap->bus_info, "media", sizeof(cap->bus_info));
	cap->version = ISP_VIDEO_DRIVER_VERSION;

	if (isp_buf_type_is_capture_mp(video->type))
		cap->capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			V4L2_CAP_STREAMING;
	else if (isp_buf_type_is_output_mp(video->type))
		cap->capabilities = V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			V4L2_CAP_STREAMING;

	return 0;
}

static int
isp_video_get_format(struct file *file, void *fh, struct v4l2_format *format)
{
	struct isp_video_fh *vfh = to_isp_video_fh(fh);
	struct isp_video *video = video_drvdata(file);

	if (format->type != video->type)
		return -EINVAL;

	mutex_lock(&video->fmt_lock);
	*format = vfh->format;
	mutex_unlock(&video->fmt_lock);

	return 0;
}

static int
isp_video_set_format(struct file *file, void *fh, struct v4l2_format *format)
{
	struct isp_video_fh *vfh = to_isp_video_fh(fh);
	struct isp_video *video = video_drvdata(file);
	struct v4l2_subdev_format subdev_fmt;
	struct v4l2_subdev *subdev;
	u32 pad;
	int ret = 0;

	if (format->type != video->type)
		return -EINVAL;

	subdev = isp_video_remote_subdev(video, &pad);
	if (subdev == NULL)
		return -EINVAL;

	mutex_lock(&video->fmt_lock);
	subdev_fmt.pad = pad;
	subdev_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;

	ret = isp_video_pix_to_mbus(&format->fmt.pix_mp, &subdev_fmt.format);
	if (ret)
		goto out;

	ret = v4l2_subdev_call(subdev, pad, set_fmt, NULL, &subdev_fmt);
	if (ret) {
		ret = (ret == -ENOIOCTLCMD) ? -EINVAL : ret;
		goto out;
	}

	ret = isp_video_mbus_to_pix(video,
			&subdev_fmt.format, &format->fmt.pix_mp);
	if (ret)
		goto out;

	vfh->format = *format;

out:
	mutex_unlock(&video->fmt_lock);
	return ret;
}

static int
isp_video_try_format(struct file *file, void *fh, struct v4l2_format *format)
{
	struct isp_video *video = video_drvdata(file);
	struct v4l2_subdev_format subdev_fmt;
	struct v4l2_subdev *subdev;
	u32 pad;
	int ret;

	if (format->type != video->type)
		return -EINVAL;

	subdev = isp_video_remote_subdev(video, &pad);
	if (subdev == NULL)
		return -EINVAL;

	mutex_lock(&video->fmt_lock);

	ret = isp_video_pix_to_mbus(&format->fmt.pix_mp, &subdev_fmt.format);
	if (ret)
		goto out;

	subdev_fmt.pad = pad;
	subdev_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(subdev, pad, get_fmt, NULL, &subdev_fmt);
	if (ret) {
		ret = (ret == -ENOIOCTLCMD) ? -EINVAL : ret;
		goto out;
	}
	ret = isp_video_mbus_to_pix(video,
			&subdev_fmt.format, &format->fmt.pix_mp);

out:
	mutex_unlock(&video->fmt_lock);
	return ret;
}

static int
isp_video_cropcap(struct file *file, void *fh, struct v4l2_cropcap *cropcap)
{
	struct isp_video *video = video_drvdata(file);
	struct v4l2_subdev *subdev;
	int ret;

	subdev = isp_video_remote_subdev(video, NULL);
	if (subdev == NULL)
		return -EINVAL;

	mutex_lock(&video->fmt_lock);
	ret = v4l2_subdev_call(subdev, video, cropcap, cropcap);
	mutex_unlock(&video->fmt_lock);

	return ret == -ENOIOCTLCMD ? -EINVAL : ret;
}

static int
isp_video_get_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct isp_video *video = video_drvdata(file);
	struct v4l2_subdev_format format;
	struct v4l2_subdev *subdev;
	u32 pad;
	int ret;

	subdev = isp_video_remote_subdev(video, &pad);
	if (subdev == NULL)
		return -EINVAL;

	/* Try the get crop operation first and fallback to get format if not
	 * implemented.
	 */
	ret = v4l2_subdev_call(subdev, video, g_crop, crop);
	if (ret != -ENOIOCTLCMD)
		return ret;

	mutex_lock(&video->fmt_lock);

	format.pad = pad;
	format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	ret = v4l2_subdev_call(subdev, pad, get_fmt, NULL, &format);
	if (ret < 0) {
		mutex_unlock(&video->fmt_lock);
		return ret == -ENOIOCTLCMD ? -EINVAL : ret;
	}

	crop->c.left = 0;
	crop->c.top = 0;
	crop->c.width = format.format.width;
	crop->c.height = format.format.height;
	mutex_unlock(&video->fmt_lock);

	return 0;
}

static int
isp_video_set_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct isp_video *video = video_drvdata(file);
	struct v4l2_subdev *subdev;
	int ret;

	subdev = isp_video_remote_subdev(video, NULL);
	if (subdev == NULL)
		return -EINVAL;

	mutex_lock(&video->fmt_lock);
	ret = v4l2_subdev_call(subdev, video, s_crop, crop);
	mutex_unlock(&video->fmt_lock);

	return ret == -ENOIOCTLCMD ? -EINVAL : ret;
}

static int
isp_video_get_param(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct isp_video_fh *vfh = to_isp_video_fh(fh);
	struct isp_video *video = video_drvdata(file);

	if (!isp_buf_type_is_output_mp(video->type) || video->type != a->type)
		return -EINVAL;

	memset(a, 0, sizeof(*a));
	a->parm.output.capability = V4L2_CAP_TIMEPERFRAME;
	a->parm.output.timeperframe = vfh->timeperframe;

	return 0;
}

static int
isp_video_set_param(struct file *file, void *fh, struct v4l2_streamparm *a)
{
	struct isp_video_fh *vfh = to_isp_video_fh(fh);
	struct isp_video *video = video_drvdata(file);

	if (!isp_buf_type_is_output_mp(video->type) || video->type != a->type)
		return -EINVAL;

	if (a->parm.output.timeperframe.denominator == 0)
		a->parm.output.timeperframe.denominator = 1;

	vfh->timeperframe = a->parm.output.timeperframe;

	return 0;
}

static int
isp_video_reqbufs(struct file *file, void *fh
			, struct v4l2_requestbuffers *rb)
{
	struct isp_video_fh *vfh = to_isp_video_fh(fh);
	struct vb2_queue *vbq = &vfh->vb2_queue;
	int ret;

	ret = vb2_reqbufs(vbq, rb);

	return ret;
}

static int
isp_video_querybuf(struct file *file, void *fh
			, struct v4l2_buffer *b)
{
	struct isp_video_fh *vfh = to_isp_video_fh(fh);
	struct vb2_queue *vbq = &vfh->vb2_queue;
	int ret;

	ret = vb2_querybuf(vbq, b);

	return ret;
}

static int
isp_video_qbuf(struct file *file, void *fh
			, struct v4l2_buffer *b)
{
	struct isp_video_fh *vfh = to_isp_video_fh(fh);
	struct vb2_queue *vbq = &vfh->vb2_queue;
	int ret;

	if (b->memory == V4L2_MEMORY_USERPTR && b->length == 0)
		return -EINVAL;

	ret = vb2_qbuf(vbq, b);

	return ret;
}

static int
isp_video_dqbuf(struct file *file, void *fh
			, struct v4l2_buffer *b)
{
	struct isp_video_fh *vfh = to_isp_video_fh(fh);
	struct vb2_queue *vbq = &vfh->vb2_queue;
	int ret;

	ret = vb2_dqbuf(vbq, b, file->f_flags & O_NONBLOCK);

	return ret;
}


static int
isp_video_streamon(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct isp_video_fh *vfh = to_isp_video_fh(fh);
	struct isp_video *video = video_drvdata(file);
	enum isp_pipeline_state state;
	struct isp_pipeline *pipe;
	struct isp_video *far_end[FAR_END_MAX_NUM];
	int far_end_num;
	unsigned long flags;
	int ret, cnt;
	enum isp_pipeline_stream_state start_mode;
	enum isp_pipeline_stream_state stream_state;

	if (type != video->type)
		return -EINVAL;

	mutex_lock(&video->stream_lock);

	if (video->streaming) {
		mutex_unlock(&video->stream_lock);
		return -EBUSY;
	}

	pipe = video->video.entity.pipe
		? to_isp_pipeline(&video->video.entity) : &video->pipe;
	media_entity_pipeline_start(&video->video.entity, &pipe->pipe);

	ret = isp_video_check_format(video, vfh);
	if (ret < 0)
		goto error;

	far_end_num = isp_video_far_end(far_end, video);

	if (isp_buf_type_is_capture_mp(video->type)) {
		switch (video->video_type) {
		case ISP_VIDEO_DISPLAY:
			state = ISP_PIPELINE_DISPLAY_STREAM;
			pipe->output[FAR_END_ISP_DISPLAY] = video;
			break;
		case ISP_VIDEO_CODEC:
			state = ISP_PIPELINE_CODEC_STREAM;
			pipe->output[FAR_END_ISP_CODEC] = video;
			break;
		case ISP_VIDEO_CCIC:
			state = ISP_PIPELINE_CCIC_STREAM;
			pipe->output[FAR_END_CCIC] = video;
			break;
		default:
			state = 0;
			break;
		}

		if (far_end_num == 0) {
			pipe->input = NULL;
		} else {
			if (pipe->input == NULL)
				pipe->input = far_end[FAR_END_ISP_INPUT];
		}

	} else if (isp_buf_type_is_output_mp(video->type)) {
		if (far_end_num == 0) {
			ret = -EPIPE;
			goto error;
		}

		state = ISP_PIPELINE_INPUT_STREAM;
		pipe->input = video;
		for (cnt = 0; cnt < far_end_num; cnt++) {
			if (pipe->output[cnt] != NULL)
				pipe->output[cnt] = far_end[cnt];
		}
	} else {
		ret = -EINVAL;
		goto error;
	}

	pipe->state |= state;

	switch (video->video_type) {
	case ISP_VIDEO_DISPLAY:
		ret = isp_video_validate_pipeline(pipe, FAR_END_ISP_DISPLAY);
		if (ret < 0)
			goto error;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(video->isp->mcd_root_display.mcd,
			MCD_VDEV, MCD_VDEV_STREAM, 1);
		CLEAR(video->isp->mcd_root_display.mcd,
			MCD_VDEV, MCD_VDEV_QBUF);
		CLEAR(video->isp->mcd_root_display.mcd,
			MCD_VDEV, MCD_VDEV_DQBUF);
		SET(video->isp->mcd_root_display.mcd, MCD_VDEV, MCD_VDEV_DUMP, \
			-GET(video->isp->mcd_root_display.mcd,
				MCD_VDEV, MCD_VDEV_DUMP));
#endif
		break;
	case ISP_VIDEO_CODEC:
		ret = isp_video_validate_pipeline(pipe, FAR_END_ISP_CODEC);
		if (ret < 0)
			goto error;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(video->isp->mcd_root_codec.mcd,
			MCD_VDEV, MCD_VDEV_STREAM, 1);
		CLEAR(video->isp->mcd_root_codec.mcd, MCD_VDEV, MCD_VDEV_QBUF);
		CLEAR(video->isp->mcd_root_codec.mcd, MCD_VDEV, MCD_VDEV_DQBUF);
		SET(video->isp->mcd_root_codec.mcd, MCD_VDEV, MCD_VDEV_DUMP, \
		-GET(video->isp->mcd_root_codec.mcd, MCD_VDEV, MCD_VDEV_DUMP));
#endif
		break;
	case ISP_VIDEO_INPUT:
		for (cnt = 0; cnt < far_end_num; cnt++) {
			ret = isp_video_validate_pipeline(pipe, cnt);
			if (ret < 0)
				goto error;
		}
		break;
	case ISP_VIDEO_CCIC:
		ret = isp_video_validate_pipeline(pipe, FAR_END_CCIC);
		if (ret < 0)
			goto error;
		break;
	default:
		goto error;
		break;
	}

	/* Set the maximum time per frame as the value requested by userspace.
	 * This is a soft limit that can be overridden if the hardware doesn't
	 * support the request limit.
	 */
	if (isp_buf_type_is_output_mp(video->type))
		pipe->max_timeperframe = vfh->timeperframe;

	spin_lock_irqsave(&video->irq_lock, flags);
	video->vb2_vidq = &vfh->vb2_queue;
	INIT_LIST_HEAD(&video->dmabusyqueue);
	video->dmabusycnt = 0;
	set_vd_dmaqueue_flg(video, ISP_VIDEO_DMAQUEUE_UNDERRUN);
	spin_unlock_irqrestore(&video->irq_lock, flags);

	if (((pipe->state & ISP_PIPELINE_DISPLAY_STREAM) == 0)
		&& ((pipe->state & ISP_PIPELINE_CODEC_STREAM) == 0))
		atomic_set(&pipe->frame_number, -1);

	ret = vb2_streamon(video->vb2_vidq, video->type);
	if (ret < 0)
		goto error;

	start_mode = ISP_PIPELINE_STREAM_CONTINUOUS;

	stream_state = pipe->stream_state;

	if (stream_state == ISP_PIPELINE_STREAM_STOPPED) {
		ret = mvisp_pipeline_set_stream(pipe, start_mode);
		if (ret < 0)
			goto error;
	}

	if (video->ops->stream_on_notify != NULL)
		video->ops->stream_on_notify(video);

error:
	if (ret < 0) {
		if (video->vb2_vidq) {
			vb2_streamoff(video->vb2_vidq, video->type);
			video->vb2_vidq = NULL;
		}
		media_entity_pipeline_stop(&video->video.entity);
	}

	if (!ret)
		video->streaming = true;

	mutex_unlock(&video->stream_lock);

	return ret;
}

static int
isp_video_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct isp_video *video = video_drvdata(file);
	struct isp_pipeline *pipe = to_isp_pipeline(&video->video.entity);
	enum isp_pipeline_state state;

	unsigned long flags;

	if (type != video->type)
		return -EINVAL;

	mutex_lock(&video->stream_lock);

	if (video->streaming == false) {
		mutex_unlock(&video->stream_lock);
		return 0;
	}

	/* Update the pipeline state. */
	if (isp_buf_type_is_capture_mp(video->type)) {
		switch (video->video_type) {
		case ISP_VIDEO_DISPLAY:
			state = ISP_PIPELINE_DISPLAY_QUEUED
				| ISP_PIPELINE_DISPLAY_STREAM;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
			PEG(video->isp->mcd_root_display.mcd,
				MCD_VDEV, MCD_VDEV_STREAM, -1);
#endif
			break;
		case ISP_VIDEO_CODEC:
			state = ISP_PIPELINE_CODEC_QUEUED
				| ISP_PIPELINE_CODEC_STREAM;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
			PEG(video->isp->mcd_root_codec.mcd,
				MCD_VDEV, MCD_VDEV_STREAM, -1);
#endif
			break;
		case ISP_VIDEO_CCIC:
			state = ISP_PIPELINE_CCIC_QUEUED
				| ISP_PIPELINE_CCIC_STREAM;
			break;
		default:
			state = 0;
			break;
		}
	} else
		state = ISP_PIPELINE_INPUT_STREAM;

	pipe->state &= ~state;

	/* Stop the stream. */
	if (((pipe->state & ISP_PIPELINE_DISPLAY_STREAM) == 0)
		&& ((pipe->state & ISP_PIPELINE_CODEC_STREAM) == 0)
		&& ((pipe->state & ISP_PIPELINE_CCIC_STREAM) == 0)) {
		mvisp_pipeline_set_stream(pipe, ISP_PIPELINE_STREAM_STOPPED);
	}

	if (video->ops->stream_off_notify != NULL)
		video->ops->stream_off_notify(video);

	spin_lock_irqsave(&video->irq_lock, flags);
	vb2_streamoff(video->vb2_vidq, video->type);
	video->vb2_vidq = NULL;
	video->streaming = false;
	INIT_LIST_HEAD(&video->dmaidlequeue);
	INIT_LIST_HEAD(&video->dmabusyqueue);
	video->dmabusycnt = 0;
	video->dmaidlecnt = 0;
	set_vd_dmaqueue_flg(video, ISP_VIDEO_DMAQUEUE_UNDERRUN);
	media_entity_pipeline_stop(&video->video.entity);
	if (video->video.entity.pipe == NULL) {
		video->pipe.input = NULL;
		video->pipe.output[0] = NULL;
		video->pipe.output[1] = NULL;
	}
	spin_unlock_irqrestore(&video->irq_lock, flags);

	mutex_unlock(&video->stream_lock);
	return 0;
}

static int
isp_video_enum_input(struct file *file, void *fh, struct v4l2_input *input)
{
	if (input->index > 0)
		return -EINVAL;

	strlcpy(input->name, "ispdma_in", sizeof(input->name));
	input->type = V4L2_INPUT_TYPE_CAMERA;

	return 0;
}

static int
isp_video_g_input(struct file *file, void *fh, unsigned int *input)
{
	*input = 0;

	return 0;
}

static int
isp_video_s_input(struct file *file, void *fh, unsigned int input)
{
	return input == 0 ? 0 : -EINVAL;
}

static const struct v4l2_ioctl_ops isp_video_ioctl_ops = {
	.vidioc_querycap		= isp_video_querycap,
	.vidioc_g_fmt_vid_cap_mplane		= isp_video_get_format,
	.vidioc_s_fmt_vid_cap_mplane		= isp_video_set_format,
	.vidioc_try_fmt_vid_cap_mplane		= isp_video_try_format,
	.vidioc_g_fmt_vid_out_mplane		= isp_video_get_format,
	.vidioc_s_fmt_vid_out_mplane		= isp_video_set_format,
	.vidioc_try_fmt_vid_out_mplane		= isp_video_try_format,
	.vidioc_cropcap			= isp_video_cropcap,
	.vidioc_g_crop			= isp_video_get_crop,
	.vidioc_s_crop			= isp_video_set_crop,
	.vidioc_g_parm			= isp_video_get_param,
	.vidioc_s_parm			= isp_video_set_param,
	.vidioc_reqbufs			= isp_video_reqbufs,
	.vidioc_querybuf		= isp_video_querybuf,
	.vidioc_qbuf			= isp_video_qbuf,
	.vidioc_dqbuf			= isp_video_dqbuf,
	.vidioc_streamon		= isp_video_streamon,
	.vidioc_streamoff		= isp_video_streamoff,
	.vidioc_enum_input		= isp_video_enum_input,
	.vidioc_g_input			= isp_video_g_input,
	.vidioc_s_input			= isp_video_s_input,
};

/* -----------------------------------------------------------------------------
 * V4L2 file operations
 */

static int isp_video_open(struct file *file)
{
	struct isp_video *video = video_drvdata(file);
	struct isp_video_fh *isp_vfh;
	int ret = 0;

	isp_vfh = kzalloc(sizeof(*isp_vfh), GFP_KERNEL);
	if (isp_vfh == NULL)
		return -ENOMEM;

	v4l2_fh_init(&isp_vfh->vfh, &video->video);
	v4l2_fh_add(&isp_vfh->vfh);

	if (mvisp_get(video->isp) == NULL) {
		ret = -EBUSY;
		goto err;
	}

	ret = mvisp_pipeline_pm_use(&video->video.entity, 1);
	if (ret < 0) {
		mvisp_put(video->isp);
		goto err;
	}

	ret = isp_video_vb2_init(&isp_vfh->vb2_queue, video);
	if (ret < 0)
		goto err;

	memset(&isp_vfh->format, 0, sizeof(isp_vfh->format));
	isp_vfh->format.type = video->type;
	isp_vfh->timeperframe.denominator = 1;

	isp_vfh->video = video;
	file->private_data = &isp_vfh->vfh;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	switch (video->video_type) {
	case ISP_VIDEO_DISPLAY:
		PEG(video->isp->mcd_root_display.mcd,
			MCD_VDEV, MCD_VDEV_ACT, 1);
		break;
	case ISP_VIDEO_CODEC:
		PEG(video->isp->mcd_root_codec.mcd, MCD_VDEV, MCD_VDEV_ACT, 1);
		break;
	default:
		break;
	}
#endif
	return 0;

err:
	v4l2_fh_del(&isp_vfh->vfh);
	kfree(isp_vfh);

	return ret;
}

static int isp_video_close(struct file *file)
{
	struct isp_video *video = video_drvdata(file);
	struct v4l2_fh *vfh = file->private_data;
	struct isp_video_fh *isp_vfh = to_isp_video_fh(vfh);

	/* Disable streaming and free the buffers queue resources. */
	isp_video_streamoff(file, vfh, video->type);

	vb2_queue_release(&isp_vfh->vb2_queue);

	mvisp_pipeline_pm_use(&video->video.entity, 0);

	/* Release the file handle. */
	v4l2_fh_del(vfh);
	kfree(isp_vfh);
	file->private_data = NULL;

	mvisp_put(video->isp);
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	switch (video->video_type) {
	case ISP_VIDEO_DISPLAY:
		PEG(video->isp->mcd_root_display.mcd,
			MCD_VDEV, MCD_VDEV_ACT, -1);
		break;
	case ISP_VIDEO_CODEC:
		PEG(video->isp->mcd_root_codec.mcd, MCD_VDEV, MCD_VDEV_ACT, -1);
		break;
	default:
		break;
	}
#endif
	return 0;
}

static unsigned int isp_video_poll(struct file *file, poll_table *wait)
{
	int ret;
	struct isp_video_fh *vfh = to_isp_video_fh(file->private_data);
	struct vb2_queue *vbq = vfh->video->vb2_vidq;

	if (vbq == NULL)
		return POLLERR;

	if (vbq->num_buffers == 0 && vbq->fileio == NULL)
		return POLLERR;

	if (list_empty(&vbq->queued_list))
		return POLLERR;


	ret = vb2_poll(vbq, file, wait);

	return ret;
}

static int isp_video_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret;
	struct isp_video_fh *vfh = to_isp_video_fh(file->private_data);
	struct vb2_queue *vbq = &vfh->vb2_queue;

	ret = vb2_mmap(vbq, vma);

	return ret;
}

static struct v4l2_file_operations isp_video_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = video_ioctl2,
	.open = isp_video_open,
	.release = isp_video_close,
	.poll = isp_video_poll,
	.mmap = isp_video_mmap,
};

/* -----------------------------------------------------------------------------
 * ISP video core
 */

static const struct isp_video_operations isp_video_dummy_ops = {
};

int mvisp_video_init(struct isp_video *video, const char *name)
{
	const char *direction;
	int ret = 0, cnt;

	switch (video->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		direction = "output";
		video->pad.flags = MEDIA_PAD_FL_SINK;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		direction = "input";
		video->pad.flags = MEDIA_PAD_FL_SOURCE;
		break;

	default:
		return -EINVAL;
	}

	ret = media_entity_init(&video->video.entity, 1, &video->pad, 0);
	if (ret < 0)
		return ret;

	mutex_init(&video->fmt_lock);

	mutex_init(&video->stream_lock);
	mutex_init(&video->video_lock);
	spin_lock_init(&video->irq_lock);

	INIT_LIST_HEAD(&video->dmaidlequeue);
	INIT_LIST_HEAD(&video->dmabusyqueue);
	video->dmaidlecnt = 0;
	video->dmabusycnt = 0;

	video->capture_mode = ISPVIDEO_NORMAL_CAPTURE;

	/* Initialize the video device. */
	if (video->ops == NULL)
		video->ops = &isp_video_dummy_ops;

	video->video.fops = &isp_video_fops;
	snprintf(video->video.name, sizeof(video->video.name),
		 "mvisp %s %s", name, direction);

	set_vd_dmaqueue_flg(video, ISP_VIDEO_DMAQUEUE_UNDERRUN);
	video->video.vfl_type = VFL_TYPE_GRABBER;
	video->video.release = video_device_release_empty;
	video->video.ioctl_ops = &isp_video_ioctl_ops;
	video->pipe.stream_state = ISP_PIPELINE_STREAM_STOPPED;
	video->pipe.state = 0;
	video->video.lock = &video->video_lock;

	video->pipe.input = NULL;
	for (cnt = 0; cnt < FAR_END_MAX_NUM; cnt++) {
		video->pipe.output[cnt] = NULL;
		video->pipe.video_state[cnt] = ISP_PIPELINE_STREAM_STOPPED;
	}

	video_set_drvdata(&video->video, video);

	if (strcmp(name, ISP_VIDEO_INPUT_NAME) == 0)
		video->video_type = ISP_VIDEO_INPUT;
	else if (strcmp(name, ISP_VIDEO_DISPLAY_NAME) == 0)
		video->video_type = ISP_VIDEO_DISPLAY;
	else if (strcmp(name, ISP_VIDEO_CODEC_NAME) == 0)
		video->video_type = ISP_VIDEO_CODEC;
	else if (strcmp(name, ISP_VIDEO_CCIC1_NAME) == 0)
		video->video_type = ISP_VIDEO_CCIC;
	else
		video->video_type = ISP_VIDEO_UNKNOWN;

	return ret;
}

int mvisp_video_register(struct isp_video *video, struct v4l2_device *vdev)
{
	int nr;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	int ret;
#endif
	video->video.v4l2_dev = vdev;

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	switch (video->video_type) {
	case ISP_VIDEO_CODEC:
		video->mcd_vdev_codec = default_mcd_vdev;
		strcpy(video->mcd_vdev_codec.entity.name, "vdev");
		ret = mcd_entity_init(&video->mcd_vdev_codec.entity,
					&video->isp->mcd_root_codec.mcd);
		if (ret < 0)
			return ret;
		else
			video->isp->mcd_root_codec.pitem[MCD_VDEV] =
				&video->mcd_vdev_codec.entity;
		printk(KERN_INFO "cam: mount node debugfs/%s/%s\n",
			video->isp->mcd_root_codec.mcd.name,
			video->mcd_vdev_codec.entity.name);
		break;
	case ISP_VIDEO_DISPLAY:
		video->mcd_vdev_display = default_mcd_vdev;
		strcpy(video->mcd_vdev_display.entity.name, "vdev");
		ret = mcd_entity_init(&video->mcd_vdev_display.entity,
					&video->isp->mcd_root_display.mcd);
		if (ret < 0)
			return ret;
		else
			video->isp->mcd_root_display.pitem[MCD_VDEV] =
				&video->mcd_vdev_display.entity;
		printk(KERN_INFO "cam: mount node debugfs/%s/%s\n",
			video->isp->mcd_root_display.mcd.name,
			video->mcd_vdev_display.entity.name);
		break;
	default:
		break;
	}
#endif
	switch (video->video_type) {
	case ISP_VIDEO_DISPLAY:
		nr = ISP_VIDEO_NR_BASE;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(video->isp->mcd_root_display.mcd,
			MCD_VDEV, MCD_VDEV_REG, 1);
#endif
		break;
	case ISP_VIDEO_CODEC:
		nr = ISP_VIDEO_NR_BASE + 1;
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
		PEG(video->isp->mcd_root_codec.mcd, MCD_VDEV, MCD_VDEV_REG, 1);
#endif
		break;
	case ISP_VIDEO_INPUT:
		nr = ISP_VIDEO_NR_BASE + 2;
		break;
	case ISP_VIDEO_CCIC:
		nr = ISP_VIDEO_NR_BASE + 3;
		break;
	default:
		nr = -1;
		break;
	}

	return video_register_device(&video->video, VFL_TYPE_GRABBER, nr);
}

void mvisp_video_unregister(struct isp_video *video)
{
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
	switch (video->video_type) {
	case ISP_VIDEO_DISPLAY:
		PEG(video->isp->mcd_root_display.mcd,
			MCD_VDEV, MCD_VDEV_REG, -1);
		break;
	case ISP_VIDEO_CODEC:
		PEG(video->isp->mcd_root_codec.mcd, MCD_VDEV, MCD_VDEV_REG, -1);
		break;
	default:
		break;
	}
#endif
	if (video_is_registered(&video->video)) {
		media_entity_cleanup(&video->video.entity);
		video_unregister_device(&video->video);
	}
}
