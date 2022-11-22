#include <linux/export.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-dma-contig.h>

#include <media/map_camera.h>
#include <media/map_vnode.h>

#include "mvisp/isp.h"	/* W/R for get/put_isp, should be removed */

static struct map_format_desc map_format_table[] = {
	{ V4L2_MBUS_FMT_SBGGR10_1X10,	V4L2_PIX_FMT_SBGGR10,	10,	1},
	{ V4L2_MBUS_FMT_UYVY8_1X16,	V4L2_PIX_FMT_UYVY,	16,	1},
	{ V4L2_MBUS_FMT_Y12_1X12,	V4L2_PIX_FMT_YUV420M,	12,	3},
	{ V4L2_MBUS_FMT_SBGGR8_1X8,	V4L2_PIX_FMT_SBGGR8,	8,	1},
	{ V4L2_MBUS_FMT_YUYV8_1_5X8,    V4L2_PIX_FMT_YUV420,    12,     1},
};

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
	case V4L2_PIX_FMT_YUV420:
		pitch = map_format_table[idx].bpp * width >> 3;
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

static int isp_video_mbus_to_pix(const struct v4l2_mbus_framefmt *mbus,
					struct v4l2_pix_format_mplane *pix_mp)
{
	unsigned int i;

	memset(pix_mp, 0, sizeof(*pix_mp));

	for (i = 0; i < ARRAY_SIZE(map_format_table); ++i) {
		if (map_format_table[i].code == mbus->code)
			break;
	}
	if (WARN_ON(i >= ARRAY_SIZE(map_format_table)))
		return -EINVAL;

	pix_mp->width = mbus->width;
	pix_mp->height = mbus->height;
	pix_mp->pixelformat = map_format_table[i].pixelformat;
	pix_mp->colorspace = mbus->colorspace;
	pix_mp->field = mbus->field;
	pix_mp->num_planes = map_format_table[i].num_planes;

	return isp_video_calc_mplane_sizeimage(pix_mp, i);
}

static int isp_video_pix_to_mbus(struct v4l2_pix_format_mplane *pix_mp,
				  struct v4l2_mbus_framefmt *mbus)
{
	unsigned int i;

	memset(mbus, 0, sizeof(*mbus));

	for (i = 0; i < ARRAY_SIZE(map_format_table); ++i) {
		if (map_format_table[i].pixelformat == pix_mp->pixelformat)
			break;
	}

	if (WARN_ON(i >= ARRAY_SIZE(map_format_table)))
		return -EINVAL;

	if (map_format_table[i].num_planes != pix_mp->num_planes)
		return -EINVAL;

	mbus->code = map_format_table[i].code;
	mbus->width = pix_mp->width;
	mbus->height = pix_mp->height;
	mbus->colorspace = pix_mp->colorspace;
	mbus->field = pix_mp->field;

	return 0;
}

/* Hey! This is the tricky function here. H/W buffer handling function(can be
 * ISR or pre-stream setup function) can call this buffer exchange functino to
 * deliver the filled buffer to VB2, and also get the new buffer for next IRQ */
/* deliver:	should the oldest buffer be deliver to VB2?
 * discard:	should the frame in oldest buffer be discard and the buffer be
		reused for DMA? */
struct map_videobuf *map_vnode_xchg_buffer(struct map_vnode *vnode,
						bool deliver, bool discard)
{
	struct map_videobuf *del_buf = NULL, *new_buf = NULL;
	struct timespec ts;

	/* IRQ is already disabled by ISR, so don't need spin_lock_irqsave,
	 * spinlock make sure video buffers are accessed atomically by
	 * IRQ context and process context */
	spin_lock(&vnode->vb_lock);

	if (!deliver)
		goto get_frame;
	if (vnode->busy_buf_cnt + vnode->idle_buf_cnt <= vnode->min_buf_cnt) {
		printk(KERN_DEBUG "map_vnode: frame dropped, buffer count " \
			"idle:busy:min = %d:%d:%d\n", vnode->idle_buf_cnt,
			vnode->busy_buf_cnt, vnode->min_buf_cnt);
		goto get_frame;
	}
	if (unlikely(vnode->state != MAP_VNODE_STREAM)) {
		del_buf = NULL;
		printk(KERN_ERR "can't deliver buffer, video node '%s' " \
				"not stream on yet\n", vnode->vdev.name);
		goto get_frame;
	}

	del_buf = list_first_entry(&vnode->busy_buf, struct map_videobuf, hook);
	list_del_init(&del_buf->hook);
	vnode->busy_buf_cnt--;

	if (discard) {
		new_buf = del_buf;
		printk(KERN_INFO "%s: discard one frame, busy = %d\n",
			vnode->vdev.name, vnode->busy_buf_cnt);
		/* use discarded buffer as new buffer, don't bother find another
		 * new buffer */
		goto attach_frame;
	}
	ktime_get_ts(&ts);
	del_buf->vb.v4l2_buf.timestamp.tv_sec = ts.tv_sec;
	del_buf->vb.v4l2_buf.timestamp.tv_usec = ts.tv_nsec / NSEC_PER_USEC;
	vb2_buffer_done(&del_buf->vb, VB2_BUF_STATE_DONE);

get_frame:
	/* Find the next buffer that is used to put incomming frame in new_buf*/
	if (vnode->idle_buf_cnt == 0) {
		/* For DMA engine with any physical nature, a number of
		 * <min_buf_cnt> buffer should be Qed to driver. So if at least
		 * <min_buf_cnt> buffer is DMA active, we can still get the
		 * oldest active buffer and rewrite to DMA target register to
		 * get the next frame from sensor */
		if (vnode->busy_buf_cnt >= max_t(u16, vnode->min_buf_cnt, 1)) {
			new_buf = list_first_entry(&vnode->busy_buf,
					struct map_videobuf, hook);
			list_del(&new_buf->hook);
			vnode->busy_buf_cnt--;
		}
	} else {
		new_buf = list_first_entry(&vnode->idle_buf,
				struct map_videobuf, hook);
		list_del(&new_buf->hook);
		vnode->idle_buf_cnt--;
	}

attach_frame:
	if (new_buf) {
		list_add_tail(&new_buf->hook, &vnode->busy_buf);
		vnode->busy_buf_cnt++;
	}
	spin_unlock(&vnode->vb_lock);
	return new_buf;
}
EXPORT_SYMBOL(map_vnode_xchg_buffer);

/********************************** VB2 ops ***********************************/

static int map_vbq_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
			unsigned int *num_buffers, unsigned int *num_planes,
			unsigned int sizes[], void *alloc_ctxs[])
{
	struct map_vnode *vnode = vb2_get_drv_priv(vq);
	struct v4l2_pix_format_mplane *pix_mp = &vnode->format.fmt.pix_mp;
	int i;

	if (*num_buffers < vnode->min_buf_cnt)
		*num_buffers = vnode->min_buf_cnt;

	*num_planes = pix_mp->num_planes;
	for (i = 0; i < *num_planes; i++) {
		sizes[i] = ALIGN(pix_mp->plane_fmt[i].sizeimage,
				ALIGN_SIZE);
		alloc_ctxs[i] = vnode->alloc_ctx;
	}

	INIT_LIST_HEAD(&vnode->busy_buf);
	INIT_LIST_HEAD(&vnode->idle_buf);

	vnode->busy_buf_cnt = 0;
	vnode->idle_buf_cnt = 0;

	return 0;
}

static void map_vb_cleanup(struct vb2_buffer *vb)
{
#if 0
	struct map_videobuf *map_vb = container_of(vb, struct map_videobuf, vb);

	/* FIXME: clean buf DMA coherency */
	/* remove itself from buffer queue(be idle_queue or busy_queue) */
	list_del(&map_vb->hook);
#endif
	return;
}

static int map_vb_init(struct vb2_buffer *vb)
{
	struct map_videobuf *map_vb = container_of(vb, struct map_videobuf, vb);
	int i;

	for (i = 0; i < VIDEO_MAX_PLANES; i++)
		map_vb->paddr[i] = 0;
	return 0;
}

static int map_vb_prepare(struct vb2_buffer *vb)
{
	struct map_vnode *vnode = container_of(vb->vb2_queue,
					struct map_vnode, vq);
	struct map_videobuf *map_vb = container_of(vb, struct map_videobuf, vb);
	struct v4l2_pix_format_mplane *pix_mp = &vnode->format.fmt.pix_mp;
	unsigned long size;
	int i;

	if (vb->num_planes < pix_mp->num_planes)
		return -EINVAL;
	vb->num_planes = pix_mp->num_planes;
	for (i = 0; i < vb->num_planes; i++) {
		size = vb2_plane_size(vb, i);
		vb2_set_plane_payload(vb, i, size);
	}
	INIT_LIST_HEAD(&map_vb->hook);
	return 0;
}

static void map_vb_queue(struct vb2_buffer *vb)
{
	struct map_vnode *vnode = container_of(vb->vb2_queue,
					struct map_vnode, vq);
	struct map_videobuf *map_vb = container_of(vb, struct map_videobuf, vb);
	dma_addr_t dma_handle;
	int i;

	for (i = 0; i < vb->num_planes; i++) {
		dma_handle = vb2_dma_contig_plane_dma_addr(&map_vb->vb, i);
		BUG_ON(!dma_handle);
		map_vb->paddr[i] = dma_handle;
	}
	spin_lock(&vnode->vb_lock);
	list_add_tail(&map_vb->hook, &vnode->idle_buf);
	vnode->idle_buf_cnt++;
	spin_unlock(&vnode->vb_lock);

	/* Notify the subdev of qbuf event */
	if (vnode->state == MAP_VNODE_STREAM && vnode->ops->qbuf_notify)
		vnode->ops->qbuf_notify(vnode);
	return;
}

static void map_vb_lock(struct vb2_queue *vq)
{
	struct map_vnode *vnode = vb2_get_drv_priv(vq);
	mutex_lock(&vnode->vdev_lock);
	return;
}

static void map_vb_unlock(struct vb2_queue *vq)
{
	struct map_vnode *vnode = vb2_get_drv_priv(vq);
	mutex_unlock(&vnode->vdev_lock);
	return;
}

static struct vb2_ops map_vnode_vb2_ops = {
	.queue_setup	= map_vbq_setup,
	.buf_prepare	= map_vb_prepare,
	.buf_queue	= map_vb_queue,
	.buf_cleanup	= map_vb_cleanup,
	.buf_init	= map_vb_init,
	.wait_prepare	= map_vb_unlock,
	.wait_finish	= map_vb_lock,
};

static int map_vnode_vb2_init(struct vb2_queue *vq, struct map_vnode *vnode)
{
	vq->type	= vnode->buf_type;
	vq->io_modes	= VB2_USERPTR;
	vq->drv_priv	= vnode;
	vq->ops		= &map_vnode_vb2_ops;
	vq->mem_ops	= &vb2_dma_contig_memops;
	vq->buf_struct_size = sizeof(struct map_videobuf);

	/* FIXME: hard coding, not all DMA device needs contig mem, try pass
	 * alloc_ctx handle, and call */
	vnode->alloc_ctx = vb2_dma_contig_init_ctx(&vnode->vdev.dev);
	return vb2_queue_init(vq);
}

/********************************* V4L2 APIs *********************************/

static int map_vnode_querycap(struct file *file, void *fh,
				struct v4l2_capability *cap)
{
	struct map_vnode *vnode = video_drvdata(file);
	int ret = 0;

	if (vnode->pipeline == NULL)
		return -EPIPE;

	ret = map_pipeline_querycap(vnode->pipeline, cap);
	if (ret < 0)
		return ret;

	if (V4L2_TYPE_IS_OUTPUT(vnode->buf_type)) {
		cap->capabilities = V4L2_CAP_VIDEO_OUTPUT_MPLANE |
			V4L2_CAP_STREAMING;
	} else {
		cap->capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			V4L2_CAP_STREAMING;
	}

	return ret;
}

static int map_vnode_get_format(struct file *file, void *fh,
				struct v4l2_format *format)
{
	struct map_vnode *vnode = video_drvdata(file);
	struct map_pipeline *pipeline = vnode->pipeline;
	struct v4l2_mbus_framefmt fmt;
	int ret = 0;

	if (format->type != vnode->buf_type)
		return -EINVAL;

	mutex_lock(&vnode->pipeline->fmt_lock);
	ret = map_pipeline_get_format(pipeline, &fmt);
	if (ret)
		goto out;
	ret = isp_video_mbus_to_pix(&fmt, &format->fmt.pix_mp);
	if (ret)
		goto out;
	mutex_unlock(&vnode->pipeline->fmt_lock);
out:
	return ret;
}

static int map_vnode_set_format(struct file *file, void *fh,
				struct v4l2_format *format)
{
	struct map_vnode *vnode = video_drvdata(file);
	struct v4l2_mbus_framefmt fmt;
	int ret = 0;

	if (format->type != vnode->buf_type)
		return -EINVAL;

	ret = isp_video_pix_to_mbus(&format->fmt.pix_mp, &fmt);
	if (ret)
		goto out;

	if (vnode->pipeline) {
		mutex_lock(&vnode->pipeline->fmt_lock);
		ret = map_pipeline_set_format(vnode->pipeline, &fmt);
		/* TODO: also change pipeline state here */
		mutex_unlock(&vnode->pipeline->fmt_lock);
		if (ret)
			goto out;
	} else {
		struct media_pad *pad = media_entity_remote_source(&vnode->pad);
		struct v4l2_subdev *sd;
		struct v4l2_subdev_format sd_fmt;

		if (pad == NULL || pad->entity == NULL)
			return -EPIPE;
		sd = media_entity_to_v4l2_subdev(pad->entity);
		memcpy(&sd_fmt.format, &fmt, sizeof(struct v4l2_mbus_framefmt));
		sd_fmt.pad = pad->index;
		sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
		ret = v4l2_subdev_call(sd, pad, set_fmt, NULL, &sd_fmt);
		if (ret)
			goto out;
	}

	ret = isp_video_mbus_to_pix(&fmt, &format->fmt.pix_mp);
	if (ret)
		goto out;

	vnode->format = *format;
out:
	/* TODO: also change pipeline state here */
	return ret;
}

static int map_vnode_try_format(struct file *file, void *fh,
				struct v4l2_format *format)
{
	struct map_vnode *vnode = video_drvdata(file);
	struct map_pipeline *pipeline = vnode->pipeline;
	struct v4l2_mbus_framefmt fmt;
	int ret;

	if (format->type != vnode->buf_type)
		return -EINVAL;

	mutex_lock(&vnode->pipeline->fmt_lock);
	ret = isp_video_pix_to_mbus(&format->fmt.pix_mp, &fmt);
	if (ret)
		goto out;
	ret = map_pipeline_try_format(pipeline, &fmt);
	if (ret)
		goto out;
	ret = isp_video_mbus_to_pix(&fmt, &format->fmt.pix_mp);
out:
	mutex_unlock(&vnode->pipeline->fmt_lock);
	return ret;
}

static int map_vnode_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *rb)
{
	struct map_vnode *vnode = video_drvdata(file);
	return vb2_reqbufs(&vnode->vq, rb);
}

static int map_vnode_querybuf(struct file *file, void *fh,
				struct v4l2_buffer *vb)
{
	struct map_vnode *vnode = video_drvdata(file);
	return vb2_querybuf(&vnode->vq, vb);
}

static int map_vnode_qbuf(struct file *file, void *fh, struct v4l2_buffer *vb)
{
	struct map_vnode *vnode = video_drvdata(file);
	if (vb->memory == V4L2_MEMORY_USERPTR && vb->length == 0)
		return -EINVAL;
	return vb2_qbuf(&vnode->vq, vb);
}

static int map_vnode_dqbuf(struct file *file, void *fh,	struct v4l2_buffer *vb)
{
	struct map_vnode *vnode = video_drvdata(file);
	return vb2_dqbuf(&vnode->vq, vb, file->f_flags & O_NONBLOCK);
}


static int map_vnode_streamon(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	struct map_vnode *vnode = video_drvdata(file);
	struct map_pipeline *pipeline = vnode->pipeline;
	int ret = 0;

	if (type != vnode->buf_type)
		return -EINVAL;

	mutex_lock(&vnode->st_lock);
	if (vnode->state == MAP_VNODE_STREAM) {
		ret = -EBUSY;
		goto err_exit;
	}

	/* TODO: Apply final format: maybe default or user defined */
	/* 1st: if not set format, get default format */
	/* 2nd: apply format to pipeline */

	INIT_LIST_HEAD(&vnode->busy_buf);
	vnode->busy_buf_cnt = 0;
	ret = vb2_streamon(&vnode->vq, vnode->buf_type);
	if (ret < 0)
		goto err_exit;

	if (vnode->pipeline) {
		ret = map_pipeline_set_stream(pipeline, 1);
		if (ret < 0)
			goto err_exit;
	} else {
		struct media_pad *pad = media_entity_remote_source(&vnode->pad);
		struct v4l2_subdev *sd;

		if (pad == NULL || pad->entity == NULL)
			return -EPIPE;
		sd = media_entity_to_v4l2_subdev(pad->entity);
		ret = v4l2_subdev_call(sd, video, s_stream, 1);
	}

	if (vnode->ops->stream_on_notify) {
		ret = vnode->ops->stream_on_notify(vnode);
		if (ret < 0)
			goto err_vb_num;
	}

	vnode->state = MAP_VNODE_STREAM;
	mutex_unlock(&vnode->st_lock);
	return ret;

err_vb_num:
	vb2_streamoff(&vnode->vq, vnode->buf_type);
err_exit:
	mutex_unlock(&vnode->st_lock);
	return ret;
}

static int map_vnode_streamoff(struct file *file, void *fh,
				enum v4l2_buf_type type)
{
	struct map_vnode *vnode = video_drvdata(file);
	unsigned long flags;

	if (type != vnode->buf_type)
		return -EINVAL;
	mutex_lock(&vnode->st_lock);

	if (vnode->state == MAP_VNODE_STANDBY) {
		mutex_unlock(&vnode->st_lock);
		return 0;
	}

	if (vnode->ops->stream_off_notify)
		vnode->ops->stream_off_notify(vnode);
	if (vnode->pipeline)
		map_pipeline_set_stream(vnode->pipeline, 0);

	spin_lock_irqsave(&vnode->vb_lock, flags);
	vb2_streamoff(&vnode->vq, type);
	vnode->state = MAP_VNODE_STANDBY;
	INIT_LIST_HEAD(&vnode->idle_buf);
	INIT_LIST_HEAD(&vnode->busy_buf);
	vnode->busy_buf_cnt = 0;
	vnode->idle_buf_cnt = 0;
	spin_unlock_irqrestore(&vnode->vb_lock, flags);

	mutex_unlock(&vnode->st_lock);
	return 0;
}

static int map_vnode_g_input(struct file *file, void *fh, unsigned int *input)
{
	*input = 0;
	return 0;
}

static int map_vnode_s_input(struct file *file, void *fh, unsigned int input)
{
	return input == 0 ? 0 : -EINVAL;
}

static const struct v4l2_ioctl_ops map_vnode_ioctl_ops = {
	.vidioc_querycap		= map_vnode_querycap,
	.vidioc_g_fmt_vid_cap_mplane	= map_vnode_get_format,
	.vidioc_s_fmt_vid_cap_mplane	= map_vnode_set_format,
	.vidioc_try_fmt_vid_cap_mplane	= map_vnode_try_format,
	.vidioc_g_fmt_vid_out_mplane	= map_vnode_get_format,
	.vidioc_s_fmt_vid_out_mplane	= map_vnode_set_format,
	.vidioc_try_fmt_vid_out_mplane	= map_vnode_try_format,
	.vidioc_reqbufs			= map_vnode_reqbufs,
	.vidioc_querybuf		= map_vnode_querybuf,
	.vidioc_qbuf			= map_vnode_qbuf,
	.vidioc_dqbuf			= map_vnode_dqbuf,
	.vidioc_g_input			= map_vnode_g_input,
	.vidioc_s_input			= map_vnode_s_input,
	.vidioc_streamon		= map_vnode_streamon,
	.vidioc_streamoff		= map_vnode_streamoff,
};

static int map_vnode_close(struct file *file)
{
	struct map_vnode *vnode = video_drvdata(file);
	struct map_pipeline *pipeline = vnode->pipeline;

	if (pipeline == NULL)
		return 0;

	/* In case of close, make sure everything is released, regardless of
	 * pipeline state, becase app may crash and don't have the chance to do
	 * a reasonable close */
	mutex_lock(&pipeline->st_lock);
	if (pipeline->state == MAP_PIPELINE_BUSY)
		map_vnode_streamoff(file, NULL, vnode->buf_type);
	mutex_unlock(&pipeline->st_lock);

	map_pipeline_clean(pipeline);
	pipeline->drv_own = 0;
	vnode->state = MAP_VNODE_DETACH;
	printk(KERN_INFO "%s: pipeline destroyed\n", vnode->vdev.name);
	vb2_queue_release(&vnode->vq);
	{
	/* FIXME: This is just a W/R to make sure all agents are powered up */
	struct map_manager *mngr = video_get_agent(vnode)->manager;
	mvisp_put(mngr->plat_priv);
	}
	return 0;
}

static int map_vnode_open(struct file *file)
{
	struct map_vnode *vnode = video_drvdata(file);
	struct map_pipeline *pipeline = vnode->pipeline;
	int i, ret = 0;
	char buf[200];

	{
	/* FIXME: This is just a W/R to make sure all agents are powered up */
	struct map_manager *mngr = video_get_agent(vnode)->manager;
	mvisp_get(mngr->plat_priv);
	}

	ret = map_vnode_vb2_init(&vnode->vq, vnode);
	if (ret < 0)
		return ret;

	memset(&vnode->format, 0, sizeof(vnode->format));
	vnode->format.type = vnode->buf_type;

	/* For output vnode, the pipeline is never empty,
	 * but input vnode may have a empty pipeline */
	if (pipeline == NULL)
		return 0;

	mutex_lock(&pipeline->st_lock);

	if (pipeline->state != MAP_PIPELINE_DEAD) {
		ret = -EBUSY;
		goto exit_unlock;
	}

	ret = map_pipeline_validate(pipeline);
	if (ret >= 0)
		goto dump_link;
	printk(KERN_WARNING "%s: pipeline validate failed, try to find "\
			"default pipeline\n", vnode->vdev.name);
	/* Attempt to establish the default pipeline */
	pipeline->drv_own = 1;
	pipeline->input = pipeline->def_src;
	ret = map_pipeline_setup(pipeline);
	if (ret < 0)
		goto exit_unlock;
	vnode->state = MAP_VNODE_STANDBY;
dump_link:
	printk(KERN_INFO "%s: pipeline established as following:\n",
			vnode->vdev.name);
	memset(buf, 0, sizeof(buf));
	for (i = pipeline->route_sz-1; i >= 0; i--) {
		char tmp[32];
		sprintf(tmp, "[%s] => ",
			pipeline->route[i]->source->entity->name);
		strcat(buf, tmp);
	}
	printk(KERN_INFO "%s/dev/video%d\n", buf, vnode->vdev.num);

exit_unlock:
	mutex_unlock(&pipeline->st_lock);
	return ret;
}

static unsigned int map_vnode_poll(struct file *file, poll_table *wait)
{
	struct map_vnode *vnode = video_drvdata(file);
	struct vb2_queue *vbq = &vnode->vq;
	if ((vbq == NULL) || (vbq->num_buffers == 0 && vbq->fileio == NULL)
		|| list_empty(&vbq->queued_list))
		return POLLERR;
	return vb2_poll(vbq, file, wait);
}

static ssize_t map_vnode_read(struct file *file, char __user *buf, size_t count,
			loff_t *ppos)
{
	struct map_vnode *vnode = video_drvdata(file);
	printk(KERN_INFO "%s: read from map_vnode\n", vnode->vdev.entity.name);
	count = 0;
	return 0;
}

static int map_vnode_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct map_vnode *vnode = video_drvdata(file);
	struct vb2_queue *vbq = &vnode->vq;
	return vb2_mmap(vbq, vma);
}

static const struct v4l2_file_operations map_vnode_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
	.open		= &map_vnode_open,
	.release	= &map_vnode_close,
	.poll		= &map_vnode_poll,
	.read		= &map_vnode_read,
	.mmap		= &map_vnode_mmap,
};

/* initialize map video node for a dma-capabile agent, dir: O-Output, 1-Input */
int map_vnode_add(struct map_vnode *vnode, struct v4l2_device *vdev,
			int dir, int min_buf, int vdev_nr)
{
	int ret = 0;

	spin_lock_init(&vnode->vb_lock);
	mutex_init(&vnode->vdev_lock);
	mutex_init(&vnode->st_lock);
	INIT_LIST_HEAD(&vnode->idle_buf);
	INIT_LIST_HEAD(&vnode->busy_buf);
	vnode->idle_buf_cnt = vnode->busy_buf_cnt = 0;
	vnode->min_buf_cnt = min_buf;
	if (dir == 0) {
		vnode->buf_type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		vnode->pad.flags	= MEDIA_PAD_FL_SINK;
	} else {
		vnode->buf_type		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		vnode->pad.flags	= MEDIA_PAD_FL_SOURCE;
	}
	ret = media_entity_init(&vnode->vdev.entity, 1, &vnode->pad, 0);
	if (ret < 0)
		goto exit;
	vnode->vdev.entity.group_id = MEDIA_ENT_T_DEVNODE_V4L;
	vnode->vdev.vfl_type	= VFL_TYPE_GRABBER;
	vnode->vdev.release	= video_device_release_empty;
	vnode->vdev.ioctl_ops	= &map_vnode_ioctl_ops;
	vnode->vdev.fops	= &map_vnode_fops;
	vnode->vdev.lock	= &vnode->vdev_lock;
	vnode->vdev.v4l2_dev	= vdev;
	video_set_drvdata(&vnode->vdev, vnode);
	ret = video_register_device(&vnode->vdev, VFL_TYPE_GRABBER, vdev_nr);
exit:
	return ret;
}
EXPORT_SYMBOL(map_vnode_add);

void map_vnode_remove(struct map_vnode *vnode)
{
	if (video_is_registered(&vnode->vdev)) {
		media_entity_cleanup(&vnode->vdev.entity);
		video_unregister_device(&vnode->vdev);
	}
}
EXPORT_SYMBOL(map_vnode_remove);
