/*
 * plat_cam.c
 *
 * Marvell Camera/ISP driver - Platform level module
 *
 * Copyright:  (C) Copyright 2013 Marvell Technology Shanghai Ltd.
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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <linux/pm_qos.h>
#include <linux/i2c.h>
#include <media/b52socisp/host_isd.h>
#include <media/b52-sensor.h>
#include <media/b52-vcm.h>
#include <media/b52-flash.h>

#include "plat_cam.h"
#include "b52isp.h"
#include "b52-reg.h"
#include "ccicv2.h"

#define PLAT_CAM_DRV	"platform-cam"

static int trace = 2;
module_param(trace, int, 0644);
MODULE_PARM_DESC(trace,
		"how many trace do you want to see? (0-4):"
		"0 - mute "
		"1 - only actual errors"
		"2 - milestone log"
		"3 - briefing log"
		"4 - detailed log");

/*
 * Get the AXI_ID that is written into MMU channel control register
 */
static inline __u32 plat_mmu_channel_id(struct msc2_mmu_dev *mmu,
		__u8 ip_id, __u8 block_id, __u8 port_id, __u8 yuv_id)
{
	__u8 grp_id;
	__u8 sel_id = 0;
	switch (ip_id) {
	case PCAM_IP_CCICV2:
		sel_id = block_id;
		yuv_id += block_id * 4;
		grp_id = 0;
		break;
	case PCAM_IP_B52ISP:
		switch (block_id) {
		case B52ISP_BLK_AXI1:
		case B52ISP_BLK_AXI2:
		case B52ISP_BLK_AXI3:
			grp_id = (block_id - B52ISP_BLK_AXI1 + 1);
			switch (port_id) {
			case B52AXI_PORT_W1:
				BUG_ON(yuv_id >= 3);
				sel_id = grp_id * 2;
				break;
			case B52AXI_PORT_W2:
				BUG_ON(yuv_id >= 3);
				yuv_id += 3;
				sel_id = grp_id * 2;
				break;
			case B52AXI_PORT_R1:
				BUG_ON(yuv_id >= 2);
				yuv_id += 6;
				sel_id = grp_id * 2 + 1;
				break;
			}
			break;
		default:
			BUG_ON(1);
		}
		break;
	default:
		BUG_ON(1);
	}

	return mmu->ops->get_tid(mmu, sel_id, yuv_id, grp_id);
}

/*
 * Get the AXI_ID that is written into AXI master(ISP MAC or CCIC)
 */
static __u16 plat_axi_id(__u8 port_id, __u8 yuv_id)
{
	switch (port_id) {
	case B52AXI_PORT_W1:
		BUG_ON(yuv_id >= 3);
		break;
	case B52AXI_PORT_W2:
		BUG_ON(yuv_id >= 3);
		yuv_id += 3;
		break;
	case B52AXI_PORT_R1:
		BUG_ON(yuv_id >= 2);
		yuv_id += 6;
		break;
	}
	/* TODO; Add support for CCIC */
	return yuv_id;
}

static inline void plat_put_mmu_dev(struct plat_cam *pcam)
{
	if (unlikely(WARN_ON(pcam == NULL)))
		return;

	if (unlikely(WARN_ON(atomic_read(&pcam->mmu_ref) <= 0)))
		return;
	if (atomic_dec_return(&pcam->mmu_ref) == 0) {
		msc2_put_sc2(&pcam->mmu_dev);
		pcam->mmu_dev = NULL;
	}
}

static inline int plat_get_mmu_dev(struct plat_cam *pcam)
{
	struct msc2_mmu_dev *mmu;
	int ret;

	if (unlikely(pcam == NULL))
		return -EINVAL;

	if (atomic_inc_return(&pcam->mmu_ref) == 1) {
		ret = msc2_get_sc2(&pcam->mmu_dev, 0);
		if (ret) {
			d_inf(1, "no mmu device found");
			return -ENODEV;
		}
		mmu = pcam->mmu_dev;

		ret = mmu->ops->rbypass(mmu, 0);
		if (ret) {
			d_inf(1, "unable to enable mmu for read");
			goto put_mmu;
		}
		ret = mmu->ops->wbypass(mmu, 0);
		if (ret) {
			d_inf(1, "unable to enable mmu for write");
			goto put_mmu;
		}
	}
	return 0;

put_mmu:
	plat_put_mmu_dev(pcam);
	return ret;
}

static int plat_mmu_alloc_channel(struct plat_cam *pcam,
			__u8 ip_id, __u8 blk_id, __u8 port_id, __u8 nr_chnl,
			struct mmu_chs_desc *ch_dsc)
{
	struct mmu_chs_desc chs_desc;
	struct msc2_mmu_dev *mmu;
	int i, ret;

	if (nr_chnl > 3) {
		d_inf(1, "channel number is error %d\n", nr_chnl);
		return -EINVAL;
	}

	chs_desc.nr_chs = nr_chnl;
	ret = plat_get_mmu_dev(pcam);
	if (ret < 0)
		goto err_get_mmu;

	mmu = pcam->mmu_dev;
	for (i = 0; i < nr_chnl; i++) {
		chs_desc.tid[i] = plat_mmu_channel_id(mmu, ip_id, blk_id, port_id, i);
		ret = mmu->ops->acquire_ch(mmu, chs_desc.tid[i]);
		if (ret) {
			d_inf(1, "failed to alloc MMU channel for ISP");
			goto err_alloc_mmu;
		}
	}

	*ch_dsc = chs_desc;
	return 0;

err_alloc_mmu:
	for (i--; i >= 0; i--)
		mmu->ops->release_ch(mmu, chs_desc.tid[i]);
	plat_put_mmu_dev(pcam);
err_get_mmu:
	return ret;
}

static void plat_mmu_free_channel(struct plat_cam *pcam,
				struct mmu_chs_desc *ch_dsc)
{
	struct msc2_mmu_dev *mmu;
	int i;

	if (unlikely(WARN_ON(!pcam->mmu_dev)))
		return;

	mmu = pcam->mmu_dev;
	mmu->ops->disable_ch(mmu, ch_dsc->tid, ch_dsc->nr_chs);
	for (i = 0; i < ch_dsc->nr_chs; i++)
		mmu->ops->release_ch(mmu, ch_dsc->tid[i]);
	plat_put_mmu_dev(pcam);
	ch_dsc->nr_chs = 0;
	memset(ch_dsc->tid, 0, sizeof(ch_dsc->tid));
}

static int plat_mmu_fill_channel(struct plat_cam *pcam,
				struct vb2_buffer *vb, int num_planes)
{
	struct isp_videobuf *buf = container_of(vb, struct isp_videobuf, vb);
	struct isp_vnode *vnode = container_of(vb->vb2_queue,
						struct isp_vnode, vq);
	struct plat_vnode *pvnode = container_of(vnode,
						struct plat_vnode, vnode);
	int ch;
	int ret;

	if (unlikely(!pcam->mmu_dev || !vb || !buf)) {
		pr_err("%s: paramter error\n", __func__);
		return -ENODEV;
	}

	for (ch = 0; ch < num_planes; ch++) {
		struct msc2_ch_info *info = &buf->ch_info[ch];
		info->tid = pvnode->mmu_ch_dsc.tid[ch];
	}
	ret = pcam->mmu_dev->ops->config_ch(pcam->mmu_dev, buf->ch_info,
						num_planes);
	if (ret < 0)
		return ret;

	return pcam->mmu_dev->ops->enable_ch(pcam->mmu_dev,
		pvnode->mmu_ch_dsc.tid, num_planes);
}

static int plat_mmu_reset_channel(struct plat_cam *pcam,
				struct isp_vnode *vnode, int num_planes)
{
	struct plat_vnode *pvnode;

	if (unlikely(!pcam || !pcam->mmu_dev || !vnode)) {
		pr_err("%s: paramter error\n", __func__);
		return -EINVAL;
	}

	pvnode = container_of(vnode, struct plat_vnode, vnode);
	return pcam->mmu_dev->ops->reset_ch(pcam->mmu_dev,
			pvnode->mmu_ch_dsc.tid, num_planes);
}

__u32 plat_get_src_tag(struct plat_topology *ppl)
{
	if (unlikely(ppl == NULL))
		return -EINVAL;

	/* Get source lag_tag */
	if (ppl->src_type == PLAT_SRC_T_SNSR) {
		struct v4l2_subdev *sensor = ppl->src.sensor;
		return (sensor->entity.id + 1) << 16;
	} else {
		struct isp_vnode *vnode = ppl->src.vnode;
		return (vnode->vdev.entity.id + 1) << 16;
	}
}

int plat_vnode_get_topology(struct plat_vnode *pvnode, struct plat_topology *topo)
{
	struct isp_vnode *vnode = &pvnode->vnode;
	struct media_pad *pad;
	struct isp_subdev *isd;
	int ret = 0;

	if (pvnode->plat_topo)
		return 0;

	/* mutex_lock(&vnode->vdev.entity.parent->graph_mutex); */

	if (vnode->buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		/* output vdev */

		/* get post-scalar subdev */
		pad = media_entity_remote_pad(&vnode->pad);
		if (WARN_ON(pad == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		isd = v4l2_get_subdev_hostdata(
				media_entity_to_v4l2_subdev(pad->entity));
		if (WARN_ON(isd == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}

		/* get path subdev */
		pad = media_entity_remote_pad(isd->pads + B52PAD_AXI_IN);
		if (WARN_ON(pad == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		isd = v4l2_get_subdev_hostdata(
				media_entity_to_v4l2_subdev(pad->entity));
		if (WARN_ON(isd == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		topo->path = isd;

		/* get pre-scalar subdev */
		pad = media_entity_remote_pad(isd->pads + B52PAD_PIPE_IN);
		if (WARN_ON(pad == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		isd = v4l2_get_subdev_hostdata(
				media_entity_to_v4l2_subdev(pad->entity));
		if (WARN_ON(isd == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		topo->scalar_a = isd;

		switch (isd->sd_code) {
		/* In case of Memory-To-Memory process */
		case SDCODE_B52ISP_A1R1:
		case SDCODE_B52ISP_A2R1:
		case SDCODE_B52ISP_A3R1:
			topo->crop_a = isd->crop_pad + B52PAD_AXI_OUT;
			topo->src_type = PLAT_SRC_T_VDEV;
			pad = media_entity_remote_pad(
				isd->pads + B52PAD_AXI_IN);
			if (WARN_ON(pad == NULL)) {
				ret = -EPIPE;
				goto unlock;
			}
			topo->src.vnode = me_to_vnode(pad->entity);
			if (WARN_ON(topo->src.vnode == NULL)) {
				ret = -EPIPE;
				goto unlock;
			}
			break;
		/* In case of Sensor-To-Memory process */
		case SDCODE_B52ISP_IDI1:
		case SDCODE_B52ISP_IDI2:
			topo->src_type = PLAT_SRC_T_SNSR;
			topo->crop_a = isd->crop_pad + pad->index;

			/* get CCIC-CTRL */
			pad = media_entity_remote_pad(
				isd->pads + B52PAD_IDI_IN);
			if (WARN_ON(pad == NULL)) {
				ret = -EPIPE;
				goto unlock;
			}
			isd = v4l2_get_subdev_hostdata(
				media_entity_to_v4l2_subdev(pad->entity));
			WARN_ON((SDCODE_CCICV2_CSI0 != isd->sd_code) &&
				(SDCODE_CCICV2_CSI1 != isd->sd_code));

			/* get sensor */
			pad = media_entity_remote_pad(
				isd->pads + CCIC_CSI_PAD_IN);
			if (WARN_ON(pad == NULL)) {
				ret = -EPIPE;
				goto unlock;
			}
			topo->src.sensor =
				media_entity_to_v4l2_subdev(pad->entity);
			if (WARN_ON(topo->src.sensor == NULL)) {
				ret = -EPIPE;
				goto unlock;
			}
			if (WARN_ON(pad->entity->type !=
				MEDIA_ENT_T_V4L2_SUBDEV_SENSOR)) {
				ret = -EPIPE;
				goto unlock;
			}
			break;
		default:
			WARN_ON(1);
			ret = -EPIPE;
			goto unlock;
		}
	} else {
		/* input vdev*/
		topo->src_type = PLAT_SRC_T_VDEV;
		topo->src.vnode = vnode;

		/* get input AXI subdev */
		pad = media_entity_remote_pad(&vnode->pad);
		if (WARN_ON(pad == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		isd = v4l2_get_subdev_hostdata(
				media_entity_to_v4l2_subdev(pad->entity));
		if (WARN_ON(isd == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		topo->scalar_a = isd;
		topo->crop_a = isd->crop_pad + B52PAD_AXI_OUT;

		/* get path subdev */
		pad = media_entity_remote_pad(isd->pads + B52PAD_AXI_OUT);
		if (WARN_ON(pad == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		isd = v4l2_get_subdev_hostdata(
				media_entity_to_v4l2_subdev(pad->entity));
		if (WARN_ON(isd == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		topo->path = isd;
	}

	topo->crop_b = topo->path->crop_pad + B52PAD_PIPE_OUT;
	return 0;

unlock:
	/* mutex_unlock(&vnode->vdev.entity.parent->graph_mutex); */
	return ret;
}

int plat_topo_get_output(struct plat_topology *ppl)
{
	struct media_device *mdev = &ppl->path->build->media_dev;
	struct isp_subdev *isd;
	struct media_pad *pad;
	int ret = MAX_OUTPUT_PER_PIPELINE, out = 0, i, id;

	ppl->dst_map = 0;
	for (i = 0; i < MAX_OUTPUT_PER_PIPELINE; i++) {
		ppl->scalar_b[i] = NULL;
		ppl->dst[i] = NULL;
	}

	mutex_lock(&mdev->graph_mutex);

	/* Figure out outputs */
	for (i = 0, id = 0; i < ppl->path->subdev.entity.num_links; i++) {
		struct media_link *link = &ppl->path->subdev.entity.links[i];
		struct isp_vnode *dst;

		if (link->source != ppl->path->pads + B52PAD_PIPE_OUT)
			continue;
		out++;
		if ((link->flags & MEDIA_LNK_FL_ENABLED) == 0)
			continue;

		/* Find linked AXI */
		isd = v4l2_get_subdev_hostdata(
			media_entity_to_v4l2_subdev(link->sink->entity));
		if (WARN_ON(isd == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		ppl->scalar_b[id] = isd;

		/* Find output vnode */
		pad = media_entity_remote_pad(isd->pads + B52PAD_AXI_OUT);
		if (WARN_ON(pad == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		dst = me_to_vnode(pad->entity);
		if (WARN_ON(dst == NULL)) {
			ret = -EPIPE;
			goto unlock;
		}
		if (WARN_ON(dst->buf_type !=
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
			ret = -EPIPE;
			goto unlock;
		}

		ppl->dst_map |= BIT(out);
		ppl->dst[id] = dst;
		id++;
	}
unlock:
	mutex_unlock(&mdev->graph_mutex);
	return ret;
}

#ifdef CONFIG_HOST_SUBDEV
/* recursive look for the link from end to start */
/* end is never NULL, start can be NULL */
/* FIXME: use stack to optimize later*/
static int plat_find_link(struct media_entity *start,
		struct media_entity *end, struct media_link **link,
		int max_pads, int flag_set, int flag_clr)
{
	struct media_entity *last;
	int i, ret = 0;

	if (max_pads < 1)
		return -ENOMEM;

	/* search for all possible source entity */
	for (i = 0; i < end->num_links; i++) {
		/* if the link is outbound or not satisfy flag, ignore */
		if ((end->links[i].sink->entity != end)
		|| (flag_set && ((end->links[i].flags & flag_set) == 0))
		|| (flag_clr && ((end->links[i].flags & flag_clr) != 0)))
			continue;
		/* The use count of each subdev is not considered here, because
		 * this function only find the link, but not guarantee the
		 * nodes on the link is idle */
		last = end->links[i].source->entity;
		if (last == start)
			goto output_link;
		else {
			int tmp = plat_find_link(start, last, link + 1,
					max_pads - 1, flag_set, flag_clr);
			if (tmp < 0) {
				continue;
			} else {
				ret = tmp;
				goto output_link;
			}
		}
	}

	/* if pipeline input not specified, but current subdev is a sensor,
	 * we can assume that it can act as a pipeline source */
	if ((start == NULL) && (i >= end->num_links)) {
		struct v4l2_subdev *sd = NULL;
		if (end->type == MEDIA_ENT_T_V4L2_SUBDEV_SENSOR)
			sd = media_entity_to_v4l2_subdev(end);
		if (subdev_has_fn(sd, pad, set_fmt))
			return 0;
	}
	return -ENODEV;

output_link:
	*link = &end->links[i];
	return ret + 1;
}

#define PLAT_PIPELINE_LEN_MAX	10

static int plat_hsd_link(struct plat_hsd *phsd, u32 link,
				struct media_entity *start,
				struct media_entity *end)
{
	if (!link)
		goto unlink;
	/* Mainly to figure out default pipeline and setup media link */
	return 0;

unlink:
	return 0;
}

static int plat_hsd_init(struct plat_hsd *phsd, u32 init,
				struct media_entity *start,
				struct media_entity *end)
{
	struct isp_host_subdev *host = phsd->hsd;
	struct v4l2_subdev *guest;
	struct media_link *link[PLAT_PIPELINE_LEN_MAX];
	int ret = 0, nr_link, i;

	if (!init)
		goto de_init;

	nr_link = ret = plat_find_link(start, end, link, PLAT_PIPELINE_LEN_MAX,
					MEDIA_LNK_FL_ENABLED, 0);
	if (ret < 0)
		return ret;
	for (i = 0; i < nr_link; i++)
		d_inf(3, "link %d: '%s' => '%s'", i,
		link[i]->source->entity->name, link[i]->sink->entity->name);

	if (media_entity_type(link[nr_link - 1]->source->entity) ==
		MEDIA_ENT_T_V4L2_SUBDEV) {
		guest = media_entity_to_v4l2_subdev(
			link[nr_link - 1]->source->entity);
		ret = host_subdev_add_guest(host, guest);
		if (ret < 0) {
			d_inf(1, "%s: failed to add guest %s",
				host->isd.subdev.name, guest->name);
			return ret;
		}
		d_inf(3, "%s: add guest <%s> to list",
			host->isd.subdev.name, guest->name);
	}
	for (i = nr_link - 1; i >= 0; i--) {
		if (WARN_ON(media_entity_type(link[i]->sink->entity) !=
			MEDIA_ENT_T_V4L2_SUBDEV))
			continue;
		guest = media_entity_to_v4l2_subdev(link[i]->sink->entity);
		ret = host_subdev_add_guest(host, guest);
		if (ret < 0) {
			d_inf(1, "%s: failed to add guest %s",
				host->isd.subdev.name, guest->name);
			return ret;
		}
		d_inf(3, "%s: add guest <%s> to list",
			host->isd.subdev.name, guest->name);
	}
	return 0;

de_init:
	while (!list_empty(&host->isd.gdev_list)) {
		struct isp_dev_ptr *dscr =
			list_first_entry(&host->isd.gdev_list,
			struct isp_dev_ptr, hook);
		switch (dscr->type) {
		case ISP_GDEV_SUBDEV:
			{
				struct isp_subdev *isd = dscr->ptr;
				host_subdev_remove_guest(host, &isd->subdev);
			}
			break;
		case DEV_V4L2_SUBDEV:
			host_subdev_remove_guest(host, dscr->ptr);
			break;
		default:
			WARN_ON(1);
		}
	}
	return ret;
}

static int plat_hsd_notify_handler(struct notifier_block *nb,
		unsigned long event, void *data)
{
	struct isp_vnode *vnode = container_of(nb, struct isp_vnode, nb);
	struct plat_hsd *phsd = vnode->notifier.priv;
	struct media_pad *pad = media_entity_remote_pad(&vnode->pad);
	int ret = 0;

	switch (event) {
	case VDEV_NOTIFY_STM_ON:
		ret = v4l2_subdev_call(&phsd->hsd->isd.subdev,
					video, s_stream, 1);
		break;
	case VDEV_NOTIFY_STM_OFF:
		ret = v4l2_subdev_call(&phsd->hsd->isd.subdev,
					video, s_stream, 0);
		break;
	case VDEV_NOTIFY_S_FMT:
		if (!phsd->drv_own)
			break;

		/*
		 * This is a specific implementation: the v4l2_format
		 * can't be passed to subdev directly, and convert it to
		 * mbus_framefmt is neither an option. So just don't
		 * pass it to subdev, let the last subdev find vnode by
		 * media link and fetch v4l2_format from vnode directly.
		 */
		ret = v4l2_subdev_call(&phsd->hsd->isd.subdev,
					video, s_mbus_fmt, data);
		break;
	case VDEV_NOTIFY_OPEN:
		ret = phsd->init(phsd, 1, NULL, pad->entity);
		if (ret < 0) {
			if (!phsd->link || !phsd->drv_own)
				return ret;
			ret = phsd->link(phsd, 1, NULL, pad->entity);
			if (ret < 0)
				return ret;
			ret = phsd->init(phsd, 1, NULL, pad->entity);
		}
		ret = phsd->hsd->group(phsd->hsd, 1);
		break;
	case VDEV_NOTIFY_CLOSE:
		ret = phsd->hsd->group(phsd->hsd, 0);
		ret |= phsd->init(phsd, 0, NULL, NULL);
		if (phsd->link && phsd->drv_own)
			ret |= phsd->link(phsd, 0, NULL, NULL);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int __maybe_unused plat_hsd_connect_video(struct plat_hsd *phsd,
					struct isp_vnode *vnode)
{
	int ret;

	vnode->nb.notifier_call = &plat_hsd_notify_handler;
	ret = blocking_notifier_chain_register(&vnode->notifier.head,
			&vnode->nb);
	if (ret < 0)
		return ret;
	vnode->notifier.priv = phsd;
	return 0;
}

static int __maybe_unused plat_hsd_disconnect_video(struct plat_hsd *phsd,
					struct isp_vnode *vnode)
{
	int ret;

	ret = blocking_notifier_chain_unregister(&vnode->notifier.head,
			&vnode->nb);
	if (ret < 0)
		return ret;
	return 0;
}

static int host_subdev_cnt;
#endif

/**************** video device related ****************/

static void plat_set_vdev_mmu(struct isp_vnode *vnode, int enable)
{
	struct plat_vnode *pvnode = container_of(vnode,
						struct plat_vnode, vnode);
	if (enable) {
		pvnode->alloc_mmu_chnl = &plat_mmu_alloc_channel;
		pvnode->free_mmu_chnl = &plat_mmu_free_channel;
		pvnode->fill_mmu_chnl = &plat_mmu_fill_channel;
		pvnode->reset_mmu_chnl = &plat_mmu_reset_channel;
		pvnode->get_axi_id = &plat_axi_id;
		vnode->mmu_enabled = true;
		d_inf(3, "%s: enable MMU", pvnode->vnode.vdev.name);
	} else {
		pvnode->alloc_mmu_chnl = NULL;
		pvnode->free_mmu_chnl = NULL;
		pvnode->fill_mmu_chnl = NULL;
		pvnode->reset_mmu_chnl = NULL;
		pvnode->get_axi_id = NULL;
		vnode->mmu_enabled = false;
		d_inf(3, "%s: disable MMU", pvnode->vnode.vdev.name);
	}
}

static int pvnode_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct plat_vnode *pvnode = ctrl->priv;

	switch (ctrl->id) {
	case V4L2_CID_VDEV_BUFFER_LAYOUT:
		if (ctrl->val == VDEV_BUFFER_LAYOUT_PA_CONTIG)
			plat_set_vdev_mmu(&pvnode->vnode, 0);
		else
			plat_set_vdev_mmu(&pvnode->vnode, 1);
		return 0;
	case V4L2_CID_VDEV_BUFFER_DETAIN_NUM:
		d_inf(3, "%s: buffer number hold by driver: %d",
			pvnode->vnode.vdev.name, ctrl->val);
		pvnode->vnode.sw_min_buf = ctrl->val;
		return 0;
	case V4L2_CID_VDEV_ENABLE_NO_ZOOM:
		pvnode->no_zoom = ctrl->val;
		return 0;
	default:
		return -EINVAL;
	}
}

static const struct v4l2_ctrl_ops pvnode_ctrl_ops = {
	.s_ctrl = pvnode_s_ctrl,
};

struct v4l2_ctrl_config pvnode_buf_layout = {
	.id	=  V4L2_CID_VDEV_BUFFER_LAYOUT,
	.name	= "video device buffer layout",
	.min	= VDEV_BUFFER_LAYOUT_PA_CONTIG,
	.max	= VDEV_BUFFER_LAYOUT_VA_CONTIG,
	.step	= 1,
	.def	= VDEV_BUFFER_LAYOUT_PA_CONTIG,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.ops	= &pvnode_ctrl_ops,
};
struct v4l2_ctrl_config pvnode_buf_detain_num = {
	.id	= V4L2_CID_VDEV_BUFFER_DETAIN_NUM,
	.name	= "video device buffe detain number",
	.min	= 0,
	.max	= VIDEO_MAX_FRAME - 1,
	.step	= 1,
	.type	= V4L2_CTRL_TYPE_INTEGER,
	.ops	= &pvnode_ctrl_ops,
};
struct v4l2_ctrl_config pvnode_enable_no_zoom = {
	.id	= V4L2_CID_VDEV_ENABLE_NO_ZOOM,
	.name	= "video device enable no zoom",
	.min	= 0,
	.max	= 1,
	.step	= 1,
	.def	= 0,
	.type	= V4L2_CTRL_TYPE_BOOLEAN,
	.ops	= &pvnode_ctrl_ops,
};

static int plat_add_vdev(struct isp_build *build, struct isp_subdev *ispsd)
{
	struct plat_vnode *pvnode;
	struct plat_cam *pcam = build->plat_priv;
#ifdef CONFIG_HOST_SUBDEV
	struct plat_hsd *phsd;
#endif
	int i, ret = 0;

	if (ispsd->single == 0)
		return 0;

	switch (ispsd->sd_type) {
	case ISD_TYPE_DMA_OUT:
		/* find output pad in i*/
		for (i = 0; i < ispsd->subdev.entity.num_pads; i++) {
			if (ispsd->subdev.entity.pads[i].flags
				& MEDIA_PAD_FL_SOURCE)
				goto attach_output;
		}
		d_inf(1, "dma output subdev %s don't have a output pad?",
			ispsd->subdev.name);
		return -EPERM;
attach_output:
		pvnode = kzalloc(sizeof(struct plat_vnode), GFP_KERNEL);
		if (pvnode == NULL)
			return -ENOMEM;
		snprintf(pvnode->vnode.vdev.name,
			sizeof(pvnode->vnode.vdev.name),
			"vout<%s>", ispsd->subdev.name);
		ret = isp_vnode_add(&pvnode->vnode, &ispsd->build->v4l2_dev,
					0, -1);
		if (ret < 0)
			return ret;
		/* create link between subdev and associated video_device */
		ret = media_entity_create_link(&ispsd->subdev.entity, i,
			&pvnode->vnode.vdev.entity, 0, 0);
		if (ret < 0)
			return ret;
#ifdef CONFIG_HOST_SUBDEV
		/* Create pipeline and connect to vnode */
		switch (ispsd->sd_code) {
		case SDCODE_CCICV2_DMA0:
		case SDCODE_CCICV2_DMA1:
			phsd = devm_kzalloc(build->dev, sizeof(*phsd),
						GFP_KERNEL);
			if (phsd == NULL)
				return -ENOMEM;
			INIT_LIST_HEAD(&phsd->hook);
			sprintf(phsd->name, "plat_host<%s>", ispsd->subdev.name);
			phsd->hsd = host_subdev_create(build->dev, phsd->name,
					host_subdev_cnt, &hsd_cascade_behaviors);
			if (IS_ERR_OR_NULL(phsd->hsd))
				return PTR_ERR(phsd->hsd);
			{
				struct isp_subdev *isd = &phsd->hsd->isd;
				isd->drv_priv = phsd->hsd;
				isd->pads_cnt = 0;
				isd->single = 0;
				INIT_LIST_HEAD(&isd->gdev_list);
				isd->sd_code = SDCODE_HOST_SUBDEV_BASE +
						host_subdev_cnt;
				ret = plat_ispsd_register(isd);
				if (ret < 0)
					return ret;
				host_subdev_cnt++;
			}
			phsd->init = plat_hsd_init;
			phsd->link = plat_hsd_link;
			/* If app don't setup media-link, driver will own it */
			/* FIXME: phsd->drv_own = 1; */
			phsd->drv_own = 0;
			list_add_tail(&phsd->hook, &pcam->host_pool);
			ret = plat_hsd_connect_video(phsd, &pvnode->vnode);
			if (ret < 0) {
				d_inf(1, "failed to connect %s and %s",
					phsd->name, pvnode->vnode.vdev.name);
				return ret;
			}
		}
#endif
		break;

	case ISD_TYPE_DMA_IN:
		/* find input pad in i*/
		for (i = 0; i < ispsd->subdev.entity.num_pads; i++) {
			if (ispsd->subdev.entity.pads[i].flags
				& MEDIA_PAD_FL_SINK)
				goto attach_input;
		}
		d_inf(1, "dma input subdev %s don't have a input pad?",
			ispsd->subdev.name);
		return -EPERM;
attach_input:
		pvnode = kzalloc(sizeof(struct plat_vnode), GFP_KERNEL);
		if (pvnode == NULL)
			return -ENOMEM;
		snprintf(pvnode->vnode.vdev.name,
			sizeof(pvnode->vnode.vdev.name),
			"vin<%s>", ispsd->subdev.name);
		ret = isp_vnode_add(&pvnode->vnode, &ispsd->build->v4l2_dev,
					1, -1);
		if (ret < 0)
			return ret;
		/* create link between subdev and associated video_device */
		ret = media_entity_create_link(&pvnode->vnode.vdev.entity, 0,
			&ispsd->subdev.entity, i, 0);
		if (ret < 0)
			return ret;
		break;
	default:
		return 0;
	}
	INIT_LIST_HEAD(&pvnode->hook);
	list_add_tail(&pvnode->hook, &pcam->vnode_pool);
	v4l2_ctrl_new_custom(&pvnode->vnode.ctrl_handler,
				&pvnode_buf_layout, pvnode);
	v4l2_ctrl_new_custom(&pvnode->vnode.ctrl_handler,
				&pvnode_buf_detain_num, pvnode);
	v4l2_ctrl_new_custom(&pvnode->vnode.ctrl_handler,
				&pvnode_enable_no_zoom, pvnode);
	if (pvnode->vnode.ctrl_handler.error) {
		ret = pvnode->vnode.ctrl_handler.error;
		d_inf(1, "failed to register video dev controls: %d", ret);
	}
	return ret;
}

static void plat_close_vdev(struct isp_build *build)
{
	struct plat_cam *pcam = build->plat_priv;
	struct plat_vnode *pvnode;
	struct media_entity *ent;

	list_for_each_entry(pvnode, &pcam->vnode_pool, hook) {
		int ret;
		unsigned long flags;
		if (pvnode->vnode.file == NULL)
			goto release_link;
		ret = pvnode->vnode.vdev.fops->release(pvnode->vnode.file);
		d_inf(1, "force release of vdev %s, ret %d",
			pvnode->vnode.vdev.name, ret);
release_link:
		ent = &pvnode->vnode.vdev.entity;
		flags = ent->links[0].flags & ~MEDIA_LNK_FL_ENABLED;
		mutex_lock(&pvnode->vnode.link_lock);
		media_entity_setup_link(&ent->links[0], flags);
		mutex_unlock(&pvnode->vnode.link_lock);
	}
}

#define pcam_add_link(src, spad, dst, dpad) \
do { \
	ret = media_entity_create_link((src), (spad), (dst), (dpad), 0); \
	if (ret < 0) \
		return ret; \
} while (0)

static int pcam_setup_links(struct isp_build *build,
				struct v4l2_subdev **sensor_sd)
{
	struct isp_subdev *sd[SDCODE_CNT], *tmp;
	int ret;

	memset(sd, 0, sizeof(sd));
	list_for_each_entry(tmp, &build->ispsd_list, hook) {
		BUG_ON(tmp->sd_code >= SDCODE_CNT);
		sd[tmp->sd_code] = tmp;
	}

	if (sd[SDCODE_B52ISP_IDI1] == NULL) {
		d_inf(1, "b52isp not found in %s", build->name);
		return -ENODEV;
	}

	if (!sd[SDCODE_CCICV2_CSI0] || !sd[SDCODE_CCICV2_CSI1])
		return 0;

	/* CCIC #0: CSI=>DMA */
	pcam_add_link(&sd[SDCODE_CCICV2_CSI0]->subdev.entity,
			CCIC_CSI_PAD_LOCAL,
			&sd[SDCODE_CCICV2_DMA0]->subdev.entity,
			CCIC_DMA_PAD_IN);
	/* CCIC #1: CSI=>DMA */
	pcam_add_link(&sd[SDCODE_CCICV2_CSI1]->subdev.entity,
			CCIC_CSI_PAD_LOCAL,
			&sd[SDCODE_CCICV2_DMA1]->subdev.entity,
			CCIC_DMA_PAD_IN);

#if 0
	/*ULC1 will estabilish the path like:
	* back sensor -> CSI0 -> DMA0
	* front sensor -> CSI1 -> DMA1
	* so the below two links are not needed,
	* otherwise, it will cause wrong path
	*/

	/* CCIC: CSI #0 => DMA #1 */
	pcam_add_link(&sd[SDCODE_CCICV2_CSI0]->subdev.entity,
			CCIC_CSI_PAD_XFEED,
			&sd[SDCODE_CCICV2_DMA1]->subdev.entity,
			CCIC_DMA_PAD_IN);
	/* CCIC: CSI #1 => DMA #0 */
	pcam_add_link(&sd[SDCODE_CCICV2_CSI1]->subdev.entity,
			CCIC_CSI_PAD_XFEED,
			&sd[SDCODE_CCICV2_DMA0]->subdev.entity,
			CCIC_DMA_PAD_IN);
#endif

	/* CCIC: CSI #0 => ISP */
	pcam_add_link(&sd[SDCODE_CCICV2_CSI0]->subdev.entity,
			CCIC_CSI_PAD_ISP,
			&sd[SDCODE_B52ISP_IDI1]->subdev.entity,
			B52PAD_IDI_IN);

	if (sd[SDCODE_B52ISP_IDI2] != NULL)
		pcam_add_link(&sd[SDCODE_CCICV2_CSI0]->subdev.entity,
				CCIC_CSI_PAD_ISP,
				&sd[SDCODE_B52ISP_IDI2]->subdev.entity,
				B52PAD_IDI_IN);

	/* CCIC: CSI #1 => ISP */
	pcam_add_link(&sd[SDCODE_CCICV2_CSI1]->subdev.entity,
			CCIC_CSI_PAD_ISP,
			&sd[SDCODE_B52ISP_IDI1]->subdev.entity,
			B52PAD_IDI_IN);

	if (sd[SDCODE_B52ISP_IDI2] != NULL)
		pcam_add_link(&sd[SDCODE_CCICV2_CSI1]->subdev.entity,
				CCIC_CSI_PAD_ISP,
				&sd[SDCODE_B52ISP_IDI2]->subdev.entity,
				B52PAD_IDI_IN);

	if (sensor_sd[0])
		pcam_add_link(&sensor_sd[0]->entity, 0,
			&sd[SDCODE_CCICV2_CSI0]->subdev.entity,
			CCIC_CSI_PAD_IN);

	if (sensor_sd[1])
		pcam_add_link(&sensor_sd[1]->entity, 0,
			&sd[SDCODE_CCICV2_CSI1]->subdev.entity,
			CCIC_CSI_PAD_IN);

	return 0;
}

static struct isp_build plat_cam = {
	.resrc_pool	= LIST_HEAD_INIT(plat_cam.resrc_pool),
	.ispsd_list	= LIST_HEAD_INIT(plat_cam.ispsd_list),
	.event_pool	= LIST_HEAD_INIT(plat_cam.event_pool),
	.name		= PLAT_CAM_DRV,
};

int plat_ispsd_register(struct isp_subdev *ispsd)
{
	return isp_subdev_register(ispsd, &plat_cam);
}
EXPORT_SYMBOL(plat_ispsd_register);

void plat_ispsd_unregister(struct isp_subdev *ispsd)
{
	isp_subdev_unregister(ispsd);
}
EXPORT_SYMBOL(plat_ispsd_unregister);

int plat_resrc_register(struct device *dev, struct resource *res,
	const char *name, struct block_id mask,
	int res_id, void *handle, void *priv)
{
	if (isp_resrc_register(dev, res, &plat_cam.resrc_pool, name, mask,
				 res_id, handle, priv) == NULL)
		return -ENOMEM;
	else
		return 0;
}
EXPORT_SYMBOL(plat_resrc_register);

static struct v4l2_subdev *b52_detect_sensor(
		struct isp_build *isb, char *name)
{
	int ret;
	u32 nr;
	char const *s;
	struct i2c_board_info info;
	struct v4l2_subdev *subdev;
	struct i2c_adapter *adapter;
	struct device_node *subdev_np = NULL, *sensor_np = NULL;
	sensor_np = of_get_child_by_name(isb->dev->of_node, name);
	if (sensor_np == NULL)
		return NULL;
	memset(&info, 0, sizeof(info));
	do {
		subdev_np = of_get_next_available_child(sensor_np,
					subdev_np);
		if (subdev_np == NULL) {
			pr_err("%s No sensor need to be registered\n",
				__func__);
			return NULL;
		}
		ret = of_property_read_string(subdev_np,
					"compatible", &s);
		if (ret < 0) {
			pr_err("%s Unable to get sensor full name\n", __func__);
			return NULL;
		}
		strcpy(info.type, s);
		ret = of_property_read_u32(subdev_np,
				"adapter", &nr);
		if (ret < 0) {
			pr_err("%s Unable to get I2C bus number\n", __func__);
			return NULL;
		}
		adapter = i2c_get_adapter(nr);
		if (adapter == NULL) {
			pr_err("%s:Unable to get I2C adapter %d for device %s\n",
				__func__,
				nr,
				info.type);
			return NULL;
		}
		ret = of_property_read_u32(subdev_np,
				"reg", (u32 *)&(info.addr));
		if (ret < 0) {
			pr_err("%s Unable to get I2C address\n", __func__);
			return NULL;
		}
		info.of_node = subdev_np;
		subdev = v4l2_i2c_new_subdev_board(&isb->v4l2_dev,
						adapter,
						&info, NULL);
		if (subdev != NULL)
			return subdev;
	} while (subdev_np != NULL);
	return NULL;
}

static inline int plat_change_isp_clk(struct isp_subdev *isd,
		struct b52_sensor *sensor)
{
	int ret;
	struct isp_block *blk;
	struct v4l2_subdev_format fmt = {
		.pad = 0,
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};
	ret = v4l2_subdev_call(&sensor->sd, pad, get_fmt, NULL, &fmt);
	if (ret < 0)
		return ret;

	blk = isp_sd2blk(isd);
	if (!blk) {
		d_inf(1, "Unable to get idi blk");
		return -EINVAL;
	}

	/* default use 30fps */

	/*Here is the special case for S5K3L2 sensor.
	* We will use 416Mhz as pipeline clock in video and capture mode.
	*/
	if (strncmp(sensor->drvdata->name, "samsung.s5k3l2", 14) == 0)
		b52isp_idi_change_clock(blk, fmt.format.width, 2322, 30);
	else
		b52isp_idi_change_clock(blk, fmt.format.width, fmt.format.height, 30);
	return 0;
}

static inline int plat_change_csi_clk(struct ccic_ctrl_dev *ccic_ctrl,
		struct b52_sensor *sensor)
{
	/* bpp mainly for dpcm */
	u8 bpp = 10;
	u8 lanes = sensor->csi.dphy_desc.nr_lane;
	u32 mipi_bps = sensor->csi.dphy_desc.clk_freq << 1;

	ccic_ctrl->ops->clk_change(ccic_ctrl, mipi_bps, lanes, bpp);

	return 0;
}

static int plat_sensor_notifier_call(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int ret = 0;
	struct media_pad *pad;
	struct ccic_csi *csi;
	struct isp_subdev *isd;
	struct b52_sensor *sensor = data;

	/* get csi */
	pad = media_entity_remote_pad(&sensor->pad);
	if (!pad) {
		d_inf(1, "Unable to get csi pad");
		return -EINVAL;
	}
	isd = v4l2_get_subdev_hostdata(
			media_entity_to_v4l2_subdev(pad->entity));
	if (!isd) {
		d_inf(1, "Unable to get csi isp_subdev");
		return -EINVAL;
	}
	csi = isd->drv_priv;

	ret = plat_change_csi_clk(csi->ccic_ctrl, sensor);
	if (ret < 0)
		return ret;

	/* get IDI */
	pad = media_entity_remote_pad(isd->pads + CCIC_CSI_PAD_ISP);
	if (!pad) {
		d_inf(1, "Unable to IDI pad");
		return -EINVAL;
	}
	isd = v4l2_get_subdev_hostdata(
			media_entity_to_v4l2_subdev(pad->entity));
	if (!isd) {
		d_inf(1, "Unable to get IDI isp_subdev");
		return -EINVAL;
	}

	ret = plat_change_isp_clk(isd, sensor);

	return ret;
}

static struct notifier_block plat_sensor_nb = {
	.notifier_call = plat_sensor_notifier_call,
};

static int plat_tune_power(struct isp_build *isb,
		enum plat_subdev_code code, int enable)
{
	struct isp_subdev *isd;
	struct isp_block *blk;

	list_for_each_entry(isd, &isb->ispsd_list, hook)
		if (isd->sd_code == code)
			break;

	if (&isd->hook == &isb->ispsd_list ||
		isd->sd_code != code)
		return -ENODEV;

	blk = isp_sd2blk(isd);
	if (!blk)
		return -EINVAL;

	isp_block_tune_power(blk, enable);
	if (code == SDCODE_B52ISP_IDI1)
		b52_set_base_addr(blk->reg_base);

	return 0;
}

int plat_tune_isp(int on)
{
	if (on)
		return plat_tune_power(&plat_cam, SDCODE_B52ISP_IDI1, 1);
	else
		return plat_tune_power(&plat_cam, SDCODE_B52ISP_IDI1, 0);
}
EXPORT_SYMBOL(plat_tune_isp);

static int plat_setup_sensor(struct isp_build *isb,
		struct v4l2_subdev **sensor_sd)
{
	int ret;
	struct b52_sensor *b52_sensor;
	struct isp_host_subdev *hsd;
#ifdef CONFIG_SUBDEV_VCM
	struct vcm_data vdata;
#endif
#ifdef CONFIG_SUBDEV_FLASH
	struct flash_data fdata;
#endif
	char hostname[20];

	ret = plat_tune_power(isb, SDCODE_B52ISP_IDI1, 1);
	ret |= plat_tune_power(isb, SDCODE_CCICV2_CSI0, 1);
	ret |= plat_tune_power(isb, SDCODE_CCICV2_CSI1, 1);
	if (ret < 0) {
		pr_err("%s: tune power failed\n", __func__);
		return ret;
	}

	sensor_sd[0] = b52_detect_sensor(isb, "backsensor");
	if (sensor_sd[0]) {
		b52_sensor = container_of(sensor_sd[0], struct b52_sensor, sd);
		blocking_notifier_chain_register(&b52_sensor->nh, &plat_sensor_nb);
		b52_sensor->sensor_pos = 0;
	} else
		pr_info("plat detect back sensor failed\n");

	sensor_sd[1] = b52_detect_sensor(isb, "frontsensor");
	if (sensor_sd[1]) {
		b52_sensor = container_of(sensor_sd[1], struct b52_sensor, sd);
		blocking_notifier_chain_register(&b52_sensor->nh, &plat_sensor_nb);
		b52_sensor->sensor_pos = 1;
	} else
		pr_info("detect front sensor failed\n");

	ret = plat_tune_power(isb, SDCODE_CCICV2_CSI1, 0);
	ret |= plat_tune_power(isb, SDCODE_CCICV2_CSI0, 0);
	ret |= plat_tune_power(isb, SDCODE_B52ISP_IDI1, 0);

	return ret;
}
static int plat_cam_remove(struct platform_device *pdev)
{
	struct plat_vnode *pvnode;
	struct plat_cam *cam = platform_get_drvdata(pdev);

	list_for_each_entry(pvnode, &cam->vnode_pool, hook)
		isp_vnode_remove(&pvnode->vnode);
	kfree(cam);
	isp_block_pool_clean(&plat_cam.resrc_pool);

	return 0;
}

static int plat_cam_probe(struct platform_device *pdev)
{
	struct plat_cam *cam;
	struct v4l2_subdev *sensor_sd[2];
	int ret;

	/* by this time, suppose all agents are registered */
	cam = kzalloc(sizeof(*cam), GFP_KERNEL);
	if (!cam) {
		dev_err(&pdev->dev, "could not allocate memory\n");
		return -ENOMEM;
	}

	cam->isb = &plat_cam;
	cam->isb->dev = &pdev->dev;

	platform_set_drvdata(pdev, cam);

	cam->isb->name = PLAT_CAM_DRV;
	cam->isb->plat_priv = cam;
	cam->isb->add_vdev = plat_add_vdev;
	cam->isb->close_vdev = plat_close_vdev;
	INIT_LIST_HEAD(&cam->vnode_pool);
	INIT_LIST_HEAD(&cam->host_pool);
	ret = isp_build_init(cam->isb);
	if (ret < 0)
		return ret;

	memset(sensor_sd, 0, sizeof(sensor_sd));
	plat_setup_sensor(cam->isb, sensor_sd);

	/* Create all the file nodes for each subdev */
	ret = isp_build_attach_ispsd(cam->isb);
	if (ret < 0)
		return ret;

	/* Setup the link between entities, this is totally platform specific */
	ret = pcam_setup_links(cam->isb, sensor_sd);
	if (ret < 0)
		return ret;

	return 0;
}

static const struct of_device_id plat_cam_dt_match[] = {
	{ .compatible = "marvell,platform-cam", .data = NULL },
	{},
};
MODULE_DEVICE_TABLE(of, plat_isp_dt_match);

static struct platform_driver plat_cam_driver = {
	.probe = plat_cam_probe,
	.remove = plat_cam_remove,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= PLAT_CAM_DRV,
		.of_match_table = of_match_ptr(plat_cam_dt_match)
	},
};

module_platform_driver(plat_cam_driver);

MODULE_AUTHOR("Jiaquan Su <jqsu@marvell.com>");
MODULE_DESCRIPTION("Marvell ISP/Camera Platform Level Driver");
MODULE_LICENSE("GPL");
