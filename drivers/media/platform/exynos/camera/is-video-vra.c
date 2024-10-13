/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can revratribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include <videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "pablo-smc.h"
#include "is-core.h"
#include "is-cmd.h"
#include "is-err.h"
#include "is-video.h"
#include "is-metadata.h"
#include "is-param.h"

#if defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
#include "../../../../staging/android/ion/ion.h"
#include "../../../../staging/android/ion/ion_exynos.h"
#endif

#if defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
static struct ion_buffer *ion_buffer_get(struct dma_buf *dmabuf)
{
	if (!dmabuf)
		return ERR_PTR(-EINVAL);

	if (dmabuf->ops != &ion_dma_buf_ops)
		return ERR_PTR(-EINVAL);

	return (struct ion_buffer *)dmabuf->priv;
}
#endif

static int is_vra_video_s_ext_ctrl(struct file *file, void *priv,
	struct v4l2_ext_controls *ctrls)
{
	int ret = 0;
	int i;
	struct is_video_ctx *vctx;
	struct is_device_ischain *device;
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;

	FIMC_BUG(!ctrls);
	FIMC_BUG(!file);
	FIMC_BUG(!file->private_data);
	vctx = file->private_data;

	FIMC_BUG(!GET_DEVICE(vctx));
	device = GET_DEVICE(vctx);

	mdbgv_vra("%s\n", vctx, __func__);

	if (ctrls->which != V4L2_CTRL_CLASS_CAMERA) {
		merr("Invalid control class(%d)", device, ctrls->which);
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < ctrls->count; i++) {
		ext_ctrl = (ctrls->controls + i);

		switch (ext_ctrl->id) {
#ifdef ENABLE_HYBRID_FD
		case V4L2_CID_IS_FDAE:
			{
				unsigned long flags = 0;
				struct is_fdae_info *fdae_info =
					(struct is_fdae_info *)&device->is_region->fdae_info;

				spin_lock_irqsave(&fdae_info->slock, flags);
				ret = copy_from_user(fdae_info, ext_ctrl->ptr, sizeof(struct is_fdae_info) - sizeof(spinlock_t));
				spin_unlock_irqrestore(&fdae_info->slock, flags);
				if (ret) {
					merr("copy_from_user of fdae_info is fail(%d)", device, ret);
					goto p_err;
				}

				mdbgv_vra("[F%d] fdae_info(id: %d, score:%d, rect(%d, %d, %d, %d), face_num:%d\n",
					vctx,
					fdae_info->frame_count,
					fdae_info->id[0],
					fdae_info->score[0],
					fdae_info->rect[0].offset_x,
					fdae_info->rect[0].offset_y,
					fdae_info->rect[0].width,
					fdae_info->rect[0].height,
					fdae_info->face_num);
			}
			break;
#endif
#if defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
		case V4L2_CID_IS_CONVERT_BUFFER_SECURE_TO_NON_SUCURE:
		{
			struct secure_buffer_convert_info info;
			struct dma_buf *s_dbuf, *n_dbuf;
			struct ion_buffer *buffer;
			struct sg_table *table;
			struct page *page;
			phys_addr_t s_paddr, n_paddr;
			unsigned long s_size, n_size;
			struct pablo_smc_ops *psmc_ops = pablo_get_smc_ops();

			ret = copy_from_user(&info, ext_ctrl->ptr, sizeof(struct secure_buffer_convert_info));

			if (ret) {
				err("fail to copy_from_user, ret(%d)\n", ret);
				goto p_err;
			}

			s_dbuf = dma_buf_get(info.secure_buffer_fd);
			if (IS_ERR_OR_NULL(s_dbuf)) {
				pr_err("failed to get dmabuf of fd: %d\n", info.secure_buffer_fd);
				return -EINVAL;
			}

			buffer = ion_buffer_get(s_dbuf);
			if (IS_ERR_OR_NULL(buffer)) {
				pr_err("failed to get buffer of s_dbuf");
				ret = -EINVAL;
				goto free_s_dbuf;
			}

			table = buffer->sg_table;
			if (IS_ERR_OR_NULL(table)) {
				pr_err("failed to get table of buffer");
				ret = -EINVAL;
				goto free_s_dbuf;
			}

			page = sg_page(table->sgl);
			if (IS_ERR_OR_NULL(page)) {
				pr_err("failed to get page of sgl");
				ret = -EINVAL;
				goto free_s_dbuf;
			}

			s_paddr = PFN_PHYS(page_to_pfn(page));
			s_size = buffer->size;

			mdbgv_vra("s_ext_ctrl CONVERT BUFFER secure: 0x%08x, size: 0x%08x, size: 0x%08x",
					vctx, s_paddr, s_size, info.secure_buffer_size);

			n_dbuf = dma_buf_get(info.non_secure_buffer_fd);
			if (IS_ERR_OR_NULL(n_dbuf)) {
				pr_err("failed to get dmabuf of fd: %d\n", info.non_secure_buffer_fd);
				ret = -EINVAL;
				goto free_s_dbuf;
			}

			buffer = ion_buffer_get(n_dbuf);
			if (IS_ERR_OR_NULL(buffer)) {
				pr_err("failed to get buffer of n_dbuf");
				ret = -EINVAL;
				goto free_n_dbuf;
			}

			table = buffer->sg_table;
			if (IS_ERR_OR_NULL(table)) {
				pr_err("failed to get table of buffer");
				ret = -EINVAL;
				goto free_n_dbuf;
			}

			page = sg_page(table->sgl);
			if (IS_ERR_OR_NULL(page)) {
				pr_err("failed to get page of sgl");
				ret = -EINVAL;
				goto free_n_dbuf;
			}

			n_paddr = PFN_PHYS(page_to_pfn(page));
			n_size = buffer->size;

			mdbgv_vra("s_ext_ctrl CONVERT BUFFER non-secure: 0x%08x, size: 0x%08x, size: 0x%08x",
					vctx, n_paddr, n_size, info.non_secure_buffer_size);

			ret = CALL_PSMC_OPS(psmc_ops, call, SMC_SECCAM_SUPPORT_VRA, n_paddr, s_paddr, n_size);
			if (ret) {
				dev_err(is_dev, "[SMC] SMC_SECCAM_SUPPORT_VRA fail(%d)", ret);
				ret = -EINVAL;
			}

free_n_dbuf:
			dma_buf_put(n_dbuf);
free_s_dbuf:
			dma_buf_put(s_dbuf);
			break;
		}
#endif

		default:
			ctrl.id = ext_ctrl->id;
			ctrl.value = ext_ctrl->value;

			ret = is_vidioc_s_ctrl(file, priv, &ctrl);
			if (ret) {
				merr("is_vidioc_s_ctrl is fail(%d)", device, ret);
				goto p_err;
			}
			break;
		}
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops is_vra_video_ioctl_ops = {
	.vidioc_querycap		= is_vidioc_querycap,
	.vidioc_g_fmt_vid_out_mplane	= is_vidioc_g_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane	= is_vidioc_s_fmt_mplane,
	.vidioc_reqbufs			= is_vidioc_reqbufs,
	.vidioc_querybuf		= is_vidioc_querybuf,
	.vidioc_qbuf			= is_vidioc_qbuf,
	.vidioc_dqbuf			= is_vidioc_dqbuf,
	.vidioc_prepare_buf		= is_vidioc_prepare_buf,
	.vidioc_streamon		= is_vidioc_streamon,
	.vidioc_streamoff		= is_vidioc_streamoff,
	.vidioc_s_input			= is_vidioc_s_input,
	.vidioc_s_ctrl			= is_vidioc_s_ctrl,
	.vidioc_s_ext_ctrls		= is_vra_video_s_ext_ctrl,
};

static int is_vra_queue_setup(struct vb2_queue *vq,
		unsigned *nbuffers, unsigned *nplanes,
		unsigned sizes[], struct device *alloc_devs[])
{
	struct is_video_ctx *ivc = vq->drv_priv;
	struct is_queue *iq = &ivc->queue;
#if !defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
	struct is_core *core = is_get_is_core();
#endif

#if !defined(CONVERT_BUFFER_SECURE_TO_NON_SECURE)
	if (IS_ENABLED(SECURE_CAMERA_FACE)
		&& (core->scenario == IS_SCENARIO_SECURE))
		set_bit(IS_QUEUE_NEED_TO_REMAP, &iq->state);
#endif

	set_bit(IS_QUEUE_NEED_TO_KMAP, &iq->state);

	return is_queue_setup(vq, nbuffers, nplanes, sizes, alloc_devs);
}

const struct vb2_ops is_vra_vb2_ops = {
	.queue_setup		= is_vra_queue_setup,
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
int is_vra_video_probe(struct is_video *video, void *data, u32 video_num, u32 index)
{
	int ret;
	struct is_core *core = (struct is_core *)data;

	video->resourcemgr = &core->resourcemgr;
	video->group_id = GROUP_ID_VRA0;
	video->group_ofs = offsetof(struct is_device_ischain, group_vra);
	video->subdev_id = ENTRY_VRA;
	video->subdev_ofs = offsetof(struct is_group, leader);
	video->buf_rdycount = VIDEO_VRA_READY_BUFFERS;

	ret = is_video_probe(video,
		IS_VIDEO_VRA_NAME,
		video_num,
		VFL_DIR_M2M,
		&core->resourcemgr.mem,
		&core->v4l2_dev,
		NULL,
		&is_vra_video_ioctl_ops,
		&is_vra_vb2_ops);
	if (ret)
		dev_err(&core->pdev->dev, "%s is fail(%d)\n", __func__, ret);

	return ret;
}
