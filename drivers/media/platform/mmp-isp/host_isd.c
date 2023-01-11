/*
 * b52isp.c
 *
 * Marvell B52 ISP driver, based on soc-isp framework
 *
 * Copyright:  (C) Copyright 2013 Marvell Technology Shanghai Ltd.
 *
 */
#include <linux/module.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <media/v4l2-subdev.h>

#include <media/b52socisp/b52socisp-mdev.h>
#include <media/b52socisp/host_isd.h>

static int trace = 2;
module_param(trace, int, 0644);
MODULE_PARM_DESC(trace,
		"how many trace do you want to see? (0-4):"
		"0 - mute "
		"1 - only actual errors"
		"2 - milestone log"
		"3 - briefing log"
		"4 - detailed log");

#define HOST_SUBDEV_DRV_NAME	"host-subdev-pdrv"

struct tlist_entry {
	struct list_head	hook;
	void			*item;
};

int host_subdev_add_guest(struct isp_host_subdev *hsd,
				struct v4l2_subdev *guest)
{
	enum isp_gdev_type type;
	void *ptr;
	int ret;

	if (guest->grp_id == GID_ISP_SUBDEV) {
		type = ISP_GDEV_SUBDEV;
		ptr = v4l2_get_subdev_hostdata(guest);
	} else {
		type = DEV_V4L2_SUBDEV;
		ptr = guest;
	}
	ret = isp_subdev_add_guest(&hsd->isd, ptr, type);
	if (ret < 0)
		return ret;

	if (guest->grp_id == GID_ISP_SUBDEV) {
		ret = isp_subdev_add_host(ptr, &hsd->isd, GID_ISP_SUBDEV);
		if (ret < 0) {
			isp_subdev_remove_guest(&hsd->isd, ptr);
			return ret;
		}
	}
	return 0;
}
EXPORT_SYMBOL(host_subdev_add_guest);

int host_subdev_remove_guest(struct isp_host_subdev *hsd,
				struct v4l2_subdev *guest)
{
	int ret = isp_subdev_remove_guest(&hsd->isd, guest);
	if (ret < 0)
		return ret;
	if (guest->grp_id == GID_ISP_SUBDEV)
		isp_subdev_remove_host(v4l2_get_subdev_hostdata(guest),
					&hsd->isd);
	return ret;
}
EXPORT_SYMBOL(host_subdev_remove_guest);

struct isp_host_subdev *host_subdev_create(struct device *dev,
					const char *name, int id,
					struct host_subdev_pdata *pdata)
{
	struct platform_device *pdev = devm_kzalloc(dev,
						sizeof(struct platform_device),
						GFP_KERNEL);
	struct host_subdev_pdata _pdata = *pdata;
	struct isp_host_subdev *hsd;
	int ret = 0;

	if (pdev == NULL) {
		d_inf(1, "failed to alloc memory for pdev");
		ret = -ENOMEM;
		goto err;
	}
	pdev->name = HOST_SUBDEV_DRV_NAME;
	pdev->id = id;
	_pdata.name = name;
	pdev->dev.platform_data = &_pdata;
	ret = platform_device_register(pdev);
	if (ret < 0) {
		pr_info("unable to create host subdev: %d\n", ret);
		goto err;
	}

	hsd = platform_get_drvdata(pdev);
	if (unlikely(WARN_ON(hsd == NULL))) {
		ret = -ENODEV;
		goto err;
	}
	return hsd;

err:
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(host_subdev_create);

static int host_subdev_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dptr;
	int ret = 0, skip = 0;

	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		if (ret < 0) {
			skip++;
			continue;
		}
		gsd = dptr->ptr;
		if (!gsd->internal_ops || !gsd->internal_ops->open)
			continue;
		ret = gsd->internal_ops->open(gsd, fh);
		if (ret < 0) {
			d_inf(1, "%s: failed to open guest %s: %d",
				sd->name, gsd->name, ret);
			skip++;
			goto close_gsd;
		}
	}
	return 0;

close_gsd:
	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		int tmp;
		if (skip) {
			skip--;
			continue;
		}
		gsd = dptr->ptr;
		if (!gsd->internal_ops || !gsd->internal_ops->close)
			continue;
		tmp = gsd->internal_ops->close(gsd, fh);
		if (tmp < 0)
			d_inf(1, "%s: failed to revert-opened guest %s: %d",
				sd->name, gsd->name, tmp);
	}
	return ret;
}

static int host_subdev_close(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dptr;
	int ret = 0;

	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		int tmp;
		gsd = dptr->ptr;
		if (!gsd->internal_ops || !gsd->internal_ops->close)
			continue;
		tmp = gsd->internal_ops->close(gsd, fh);
		if (tmp < 0) {
			d_inf(1, "%s: failed to close guest %s: %d",
				sd->name, gsd->name, ret);
			ret = tmp;
		}
	}
	return ret;
}

static const struct v4l2_subdev_internal_ops host_subdev_internal_ops = {
	.open	= host_subdev_open,
	.close	= host_subdev_close,
};



/*
 * Host-subdev is just a container, its behavior can vary from one use case to
 * another, so how host-subdev response to subdev operations depends on the use
 * case.
 * Only two profiles is defined below as examples:
 * Cascade:	A set of v4l2_subdev is integrated into a host-subdev, to act
 *		like a single v4l2_subdev. The data stream goes through the
 *		guests in sequence, so all v4l2_subdev operations must be
 *		dispatched to guests in upstream or downstream order.
 * Bundle:	A set of v4l2_subdev with different functinality is integrated
 *		to a host-subdev, in order to describe the physical device
 *		integration. For example, a camera sensos chip, a VCM, a EEPROM
 *		is integrated into a camera module. All v4l2_subdev operations
 *		are dispatch to one of the guests, depends on the guests' nature
 */
#if defined(CONFIG_MEDIA_CONTROLLER)
/****************************** Cascade behavior ******************************/
static long hsd_cascade_core_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dptr;
	int ret = 0;

	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		gsd = dptr->ptr;
		if (!subdev_has_fn(gsd, core, ioctl))
			continue;
		ret |= v4l2_subdev_call(gsd, core, ioctl, cmd, arg);
		if (ret < 0)
			d_inf(1, "%s: failed to dispatch ioctl to %s: %d",
				hsd->isd.subdev.name, gsd->name, ret);
	}

	return ret;
}

static int hsd_cascade_core_s_power(struct v4l2_subdev *sd, int on)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd = NULL;
	struct isp_dev_ptr *dscr;
	int ret = 0, tmp, skip = 0;

	if (!on)
		goto power_off;

	list_for_each_entry_reverse(dscr, &hsd->isd.gdev_list, hook) {
		if (ret < 0) {
			/*
			 * if error happened in power on, count for skipped
			 * subdevs in power off.
			 */
			skip++;
			continue;
		}
		gsd = dscr->ptr;
		ret = v4l2_subdev_call(gsd, core, s_power, 1);
		if (ret < 0) {
			skip = 1;
			d_inf(1, "%s: failed to power on %s: %d",
				sd->name, gsd->name, ret);
		} else
			d_inf(3, "%s: power on %s done", sd->name, gsd->name);
	}
	if (ret < 0)
		goto power_off;
	return 0;

power_off:
	list_for_each_entry(dscr, &hsd->isd.gdev_list, hook) {
		if (skip) {
			/*
			 * if error happens in power on path, some subdev is NOT
			 * powered on in the 1st place, so don't power off it.
			 */
			skip--;
			continue;
		}
		gsd = dscr->ptr;
		tmp = v4l2_subdev_call(gsd, core, s_power, 0);
		if (tmp < 0) {
			d_inf(1, "%s: failed to power off %s: %d",
				sd->name, gsd->name, ret);
			ret = tmp;
		} else
			d_inf(3, "%s: power off %s done", sd->name, gsd->name);
	}

	return ret;
}

static const struct v4l2_subdev_core_ops hsd_cascade_core_ops = {
	.ioctl		= &hsd_cascade_core_ioctl,
	.s_power	= &hsd_cascade_core_s_power,
};

static int hsd_cascade_video_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dptr;
	struct v4l2_mbus_config cfg;
	int ret = 0, skip = 0, got_cfg = 0;

	if (!enable)
		goto stream_off;

	/* 1st, setup the guests which are sensitive for physical-link format */
	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		gsd = dptr->ptr;
		if (!got_cfg) {
			if (!subdev_has_fn(gsd, video, g_mbus_config))
				continue;
			ret = v4l2_subdev_call(gsd, video, g_mbus_config, &cfg);
			if (ret < 0) {
				d_inf(1, "%s: failed to get mbus_config from %s",
					sd->name, gsd->name);
				return ret;
			}
			got_cfg = 1;
		} else {
			if (!subdev_has_fn(gsd, video, s_mbus_config))
				continue;
			ret = v4l2_subdev_call(gsd, video, s_mbus_config, &cfg);
			if (ret < 0) {
				d_inf(1, "%s: failed to set mbus_config on %s",
					sd->name, gsd->name);
				return ret;
			}
		}
	}

	/* 2nd, stream on the guests upstream */
	ret = 0;
	list_for_each_entry_reverse(dptr, &hsd->isd.gdev_list, hook) {
		if (ret < 0) {
			skip++;
			continue;
		}
		gsd = dptr->ptr;
		if (!subdev_has_fn(gsd, video, s_stream))
			continue;
		ret = v4l2_subdev_call(gsd, video, s_stream, 1);
		if (ret < 0) {
			d_inf(1, "%s: failed to dispatch stream on to %s: %d, abort stream on",
				hsd->isd.subdev.name, gsd->name, ret);
			skip++;
		} else
			d_inf(3, "%s: dispatch stream on to %s done",
				hsd->isd.subdev.name, gsd->name);
	}
	if (ret < 0)
		goto stream_off;
	return 0;

stream_off:
	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		int tmp;
		if (skip) {
			/*
			 * if error happens in stream on dispatch, switch stream
			 * state back to off for the stream on guests
			 */
			skip--;
			continue;
		}
		gsd = dptr->ptr;
		if (!subdev_has_fn(gsd, video, s_stream))
			continue;
		tmp = v4l2_subdev_call(gsd, video, s_stream, 0);
		if (tmp < 0) {
			d_inf(1, "%s: failed to dispatch stream off to %s: %d",
				hsd->isd.subdev.name, gsd->name, tmp);
			ret |= tmp;
		} else
			d_inf(3, "%s: dispatch stream off to %s done",
				hsd->isd.subdev.name, gsd->name);
	}
	return ret;
}

static int hsd_cascade_video_g_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *itv)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dptr;

	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		gsd = dptr->ptr;
		if (!subdev_has_fn(gsd, video, g_frame_interval))
			continue;
		/* only one component in the pipeline can control the interval*/
		return v4l2_subdev_call(gsd, video, g_frame_interval, itv);
	}
	return -ENODEV;
}

static int hsd_cascade_video_s_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *itv)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dptr;

	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		gsd = dptr->ptr;
		if (!subdev_has_fn(gsd, video, s_frame_interval))
			continue;
		/* only one component in the pipeline can control the interval*/
		return v4l2_subdev_call(gsd, video, s_frame_interval, itv);
	}
	return -ENODEV;
}

static int hsd_cascade_video_s_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct tlist_entry *dsc;
	struct v4l2_subdev_format _fmt = {
		.which	= V4L2_SUBDEV_FORMAT_ACTIVE,
		.format	= *fmt,
	};
	struct v4l2_mbus_framefmt final_fmt;
	int ret = 0, final_flag = 0;

	list_for_each_entry_reverse(dsc, &hsd->trvrs_list, hook) {
		struct media_link *mlink = dsc->item;
		struct v4l2_subdev *dst = NULL, *src = NULL;
		struct v4l2_subdev_format tmp_fmt = _fmt;
		__u32 dst_pad, src_pad;

		/*
		 * For each pass, we get the proposed format from link sink,
		 * and apply it to the link source.
		 */
		if (media_entity_type(mlink->sink->entity) ==
			MEDIA_ENT_T_V4L2_SUBDEV) {
			dst = media_entity_to_v4l2_subdev(
				mlink->sink->entity);
			dst_pad = mlink->sink->index;
			if (unlikely(dst_pad >= mlink->sink->entity->num_pads))
				return -EINVAL;
			if (!subdev_has_fn(dst, pad, get_fmt))
				goto set_source;
			d_inf(3, "%s: get fmt on pad(%d)",
				mlink->sink->entity->name, dst_pad);
			tmp_fmt.pad = dst_pad;
			/* retrieve link sink format */
			ret = v4l2_subdev_call(dst, pad, get_fmt, NULL,
						&tmp_fmt);
			if (ret < 0) {
				d_inf(1, "%s: failed to get fmt: %d",
				mlink->sink->entity->name, ret);
				goto exit;
			}
		}

set_source:
		if (media_entity_type(mlink->source->entity) ==
			MEDIA_ENT_T_V4L2_SUBDEV) {
			src = media_entity_to_v4l2_subdev(
				mlink->source->entity);
			src_pad = mlink->source->index;
			if (unlikely(src_pad >=
				mlink->source->entity->num_pads))
				return -EINVAL;
			if (!subdev_has_fn(src, pad, set_fmt))
				continue;
			d_inf(3, "%s: set fmt on pad(%d)",
				mlink->source->entity->name, src_pad);
			tmp_fmt.pad = src_pad;
			/* Apply format to link source */
			ret = v4l2_subdev_call(src, pad, set_fmt, NULL,
						&tmp_fmt);
			if (ret < 0) {
				d_inf(1, "%s: failed to set fmt: %d",
				mlink->source->entity->name, ret);
				break;
			}
			if (!final_flag) {
				final_fmt = tmp_fmt.format;
				final_flag = 1;
			}
		}
		/* store subdev format for the next loop */
		_fmt = tmp_fmt;
	}

	/* it's possible that subdev propose a new format, so write back */
	*fmt = final_fmt;
	/*
	 * Beware that the "final format" just represent the format of the last
	 * guest link which is format-sensitive, and it may differs from the
	 * host-subdev's output format, because the format on this link maybe
	 * converted to another v4l2-format if the last guest is capable of
	 * re-ordering the pixels when it save the image to memory.
	 */
exit:
	return ret;
}

static int hsd_cascade_video_g_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev_format _fmt = {.which = V4L2_SUBDEV_FORMAT_ACTIVE};
	struct tlist_entry *dsc;
	int ret = 0;

	list_for_each_entry_reverse(dsc, &hsd->trvrs_list, hook) {
		struct media_link *mlink = dsc->item;
		struct v4l2_subdev *gsd = NULL;
		__u32 pad;

		if (media_entity_type(mlink->source->entity) ==
			MEDIA_ENT_T_V4L2_SUBDEV) {
			gsd = media_entity_to_v4l2_subdev(
				mlink->source->entity);
			pad = mlink->source->index;
			if (unlikely(pad >= mlink->source->entity->num_pads))
				return -EINVAL;
			if (!subdev_has_fn(gsd, pad, get_fmt))
				continue;
			d_inf(3, "%s: get fmt on <%s>:pad(%d)",
				sd->name, mlink->source->entity->name, pad);
			_fmt.pad = pad;
			/* retrieve link sink format */
			ret = v4l2_subdev_call(gsd, pad, get_fmt, NULL,
						&_fmt);
			if (ret < 0)
				return ret;
			*fmt = _fmt.format;
			return ret;
		}
	}
	d_inf(1, "%s: no one handles the pad::get_fmt callback", sd->name);
	return -ENODEV;
}

static int hsd_cascade_video_g_mbus_config(struct v4l2_subdev *sd,
					struct v4l2_mbus_config *cfg)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dptr;

	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		gsd = dptr->ptr;
		if (!subdev_has_fn(gsd, video, g_mbus_config))
			continue;
		/* only one component in the pipeline knows the mbus config */
		return v4l2_subdev_call(gsd, video, g_mbus_config, cfg);
	}
	return -ENODEV;
}

static int hsd_cascade_video_s_mbus_config(struct v4l2_subdev *sd,
					const struct v4l2_mbus_config *cfg)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dptr;

	list_for_each_entry(dptr, &hsd->isd.gdev_list, hook) {
		gsd = dptr->ptr;
		if (!subdev_has_fn(gsd, video, s_mbus_config))
			continue;
		/* only one component in the pipeline controls mbus config */
		return v4l2_subdev_call(gsd, video, s_mbus_config, cfg);
	}
	return -ENODEV;
}

static const struct v4l2_subdev_video_ops hsd_cascade_video_ops = {
	.s_stream		= &hsd_cascade_video_s_stream,
	.g_frame_interval	= &hsd_cascade_video_g_frame_interval,
	.s_frame_interval	= &hsd_cascade_video_s_frame_interval,
	.s_mbus_fmt		= &hsd_cascade_video_s_mbus_fmt,
	.g_mbus_fmt		= &hsd_cascade_video_g_mbus_fmt,
	.s_mbus_config		= &hsd_cascade_video_s_mbus_config,
	.g_mbus_config		= &hsd_cascade_video_g_mbus_config,
};

const struct v4l2_subdev_ops hsd_cascade_ops = {
	.core	= &hsd_cascade_core_ops,
	.video	= &hsd_cascade_video_ops,
};
EXPORT_SYMBOL(hsd_cascade_ops);

static inline struct media_link *find_mlink_for_subdev(struct v4l2_subdev *sd1,
					struct v4l2_subdev *sd2)
{
	struct media_entity *me1 = &sd1->entity;
	struct media_entity *me2 = &sd2->entity;
	struct media_link *link = NULL;
	int i;

	for (i = 0; i < me1->num_links; i++) {
		if ((link->source->entity == me2) &&
			(link->sink->entity == me2))
			break;
	}
	return link;
}

static int hsd_cascade_group(struct isp_host_subdev *hsd, u32 group)
{
	struct v4l2_subdev *gsd1 = NULL, *gsd2 = NULL;
	struct isp_dev_ptr *dscr;
	struct tlist_entry *entry;
	struct media_link *mlink;
	int ret = 0;

	if (!group)
		goto un_group;

	/* Build up cascade container */
	INIT_LIST_HEAD(&hsd->trvrs_list);

	list_for_each_entry(dscr, &hsd->isd.gdev_list, hook) {
		switch (dscr->type) {
		case ISP_GDEV_SUBDEV:
			{
				struct isp_subdev *isd = dscr->ptr;
				gsd2 = &isd->subdev;
			}
			break;
		case DEV_V4L2_SUBDEV:
			gsd2 = dscr->ptr;
			break;
		default:
			ret = -EINVAL;
			goto un_group;
		}
		if (!gsd1)
			continue;

		/* find links between two guests */
		mlink = find_mlink_for_subdev(gsd1, gsd2);
		if (mlink == NULL) {
			d_inf(1, "%s: failed to find link between guest<%s> and guest<%s>",
				hsd->isd.subdev.name, gsd1->name, gsd2->name);
			ret = -EPIPE;
			goto un_group;
		}

		/* setup a entry in traverse list for this media_link */
		entry = devm_kzalloc(hsd->isd.subdev.entity.parent->dev,
					sizeof(*entry), GFP_KERNEL);
		if (entry == NULL) {
			ret = -ENOMEM;
			goto un_group;
		}
		entry->item = mlink;
		INIT_LIST_HEAD(&entry->hook);
		list_add_tail(&entry->hook, &hsd->trvrs_list);
		gsd1 = gsd2;
	}
#if 0
	/* The cascade host subdev has the same pads as the last guest */
	hsd->subdev.entity.ops = gsd2->entity.ops;
	ret = media_entity_init(&hsd->isd.subdev.entity, gsd2->entity.num_pads,
				gsd2->entity.pads, 0);
	if (ret < 0)
		goto un_group;
#endif
	hsd->type = TLIST_ENTRY_MLINK;
	return 0;

un_group:
	/* dismiss the guests */
	while (!list_empty(&hsd->trvrs_list)) {
		entry = list_first_entry(&hsd->trvrs_list,
					struct tlist_entry, hook);
		list_del(&entry->hook);
		devm_kfree(hsd->isd.subdev.entity.parent->dev, entry);
	}
	v4l2_ctrl_handler_free(&hsd->isd.ctrl_handler);
	media_entity_cleanup(&hsd->isd.subdev.entity);
	return ret;
}

struct host_subdev_pdata hsd_cascade_behaviors = {
	.subdev_ops	= &hsd_cascade_ops,
	.group		= &hsd_cascade_group,
};
EXPORT_SYMBOL(hsd_cascade_behaviors);
#endif

/****************************** Bundle behavior ******************************/
static long hsd_bundle_core_ioctl(struct v4l2_subdev *sd,
					unsigned int cmd, void *arg)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dscr;
	int ret = 0;

	list_for_each_entry(dscr, &hsd->isd.gdev_list, hook) {
		int tmp;
		gsd = dscr->ptr;
		if (!subdev_has_fn(gsd, core, ioctl))
			continue;
		tmp = v4l2_subdev_call(gsd, core, ioctl, cmd, arg);
		if (tmp < 0) {
			d_inf(1, "%s: failed to dispatch ioctl 0x%X to %s: %d",
				hsd->isd.subdev.name, cmd, gsd->name, ret);
			return 0;
		}
		ret |= tmp;
	}

	return ret;
}

static int hsd_bundle_core_s_power(struct v4l2_subdev *sd, int on)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct isp_dev_ptr *dscr;
	int ret = 0, tmp, skip = 0;

	if (!on)
		goto power_off;

	ret = 0;
	list_for_each_entry(dscr, &hsd->isd.gdev_list, hook) {
		if (ret < 0) {
			/*
			 * if error happened in power on, count for skipped
			 * subdevs in power off.
			 */
			skip++;
			continue;
		}
		gsd = dscr->ptr;
		ret = v4l2_subdev_call(gsd, core, s_power, 1);
		if (ret < 0) {
			skip = 1;
			d_inf(1, "%s: failed to power on %s: %d",
				sd->name, gsd->name, ret);
		} else
			d_inf(3, "%s: power on %s done",
				sd->name, gsd->name);
	}
	if (ret < 0)
		goto power_off;
	return v4l2_ctrl_handler_setup(hsd->isd.subdev.ctrl_handler);

power_off:
	list_for_each_entry(dscr, &hsd->isd.gdev_list, hook) {
		if (skip) {
			/*
			 * if error happens in power on path, some subdev is NOT
			 * powered on in the 1st place, so don't power off it.
			 */
			skip--;
			continue;
		}
		gsd = dscr->ptr;
		tmp = v4l2_subdev_call(gsd, core, s_power, 0);
		if (tmp < 0) {
			d_inf(1, "%s: failed to power off %s: %d",
				sd->name, gsd->name, ret);
			ret = tmp;
		} else
			d_inf(3, "%s: power off %s done",
				sd->name, gsd->name);
	}

	return ret;
}

static const struct v4l2_subdev_core_ops hsd_bundle_core_ops = {
	.ioctl		= &hsd_bundle_core_ioctl,
	.s_power	= &hsd_bundle_core_s_power,
};

static int hsd_bundle_video_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct tlist_entry *entry;
	int ret = 0, skip = 0;

	if (enable)
		goto stream_off;

	ret = 0;
	list_for_each_entry(entry, &hsd->trvrs_list, hook) {
		if (ret < 0) {
			skip++;
			continue;
		}
		gsd = entry->item;
		if (!subdev_has_fn(gsd, video, s_stream))
			continue;
		ret = v4l2_subdev_call(gsd, video, s_stream, 1);
		if (ret < 0) {
			d_inf(1, "%s: failed to dispatch stream on to %s: %d, abort stream on",
				hsd->isd.subdev.name, gsd->name, ret);
			skip++;
		} else
			d_inf(3, "%s: dispatch stream on to %s done",
				hsd->isd.subdev.name, gsd->name);
	}
	if (ret < 0)
		goto stream_off;
	return 0;

stream_off:
	list_for_each_entry(entry, &hsd->trvrs_list, hook) {
		int tmp;
		if (skip) {
			/*
			 * if error happens in stream on dispatch, switch stream
			 * state back to off for the stream on guests
			 */
			skip--;
			continue;
		}
		gsd = entry->item;
		if (!subdev_has_fn(gsd, video, s_stream))
			continue;
		tmp = v4l2_subdev_call(gsd, video, s_stream, 0);
		if (tmp < 0)
			d_inf(1, "%s: failed to dispatch stream off to %s: %d",
				hsd->isd.subdev.name, gsd->name, tmp);
		else
			d_inf(3, "%s: dispatch stream off to %s done",
				hsd->isd.subdev.name, gsd->name);
		ret |= tmp;
	}
	return ret;
}

static int hsd_bundle_video_g_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *itv)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct tlist_entry *entry;

	list_for_each_entry(entry, &hsd->trvrs_list, hook) {
		gsd = entry->item;
		if (!subdev_has_fn(gsd, video, g_frame_interval))
			continue;
		/* the frame interval should be the same for all sensors */
		return v4l2_subdev_call(gsd, video, g_frame_interval, itv);
	}
	return -ENODEV;
}

static int hsd_bundle_video_s_frame_interval(struct v4l2_subdev *sd,
					struct v4l2_subdev_frame_interval *itv)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct v4l2_subdev *gsd;
	struct tlist_entry *entry;
	int ret = -ENODEV;

	list_for_each_entry(entry, &hsd->trvrs_list, hook) {
		gsd = entry->item;
		if (!subdev_has_fn(gsd, video, s_frame_interval))
			continue;
		/* only one component in the pipeline can control the interval*/
		ret = v4l2_subdev_call(gsd, video, s_frame_interval, itv);
		if (ret < 0) {
			d_inf(1, "%s: failed to set frame rate on %s: %d",
				sd->name, gsd->name, ret);
			return ret;
		}
	}
	return ret;
}

static int hsd_bundle_video_s_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct tlist_entry *entry;
	struct v4l2_subdev *gsd;
	int ret = -ENODEV;

	list_for_each_entry(entry, &hsd->trvrs_list, hook) {
		gsd = entry->item;
		if (!subdev_has_fn(gsd, video, s_mbus_fmt))
			continue;

		ret = v4l2_subdev_call(gsd, video, s_mbus_fmt, fmt);
		if (ret < 0) {
			d_inf(1, "%s: failed to set fmt on %s: %d",
				sd->name, gsd->name, ret);
			break;
		} else
			d_inf(3, "%s: set fmt on %s done",
				sd->name, gsd->name);
	}
	return ret;
}

static int hsd_bundle_video_g_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct tlist_entry *entry;
	struct v4l2_subdev *gsd;
	int ret = 0;

	list_for_each_entry(entry, &hsd->trvrs_list, hook) {
		gsd = entry->item;
		if (!subdev_has_fn(gsd, video, g_mbus_fmt))
			continue;

		ret = v4l2_subdev_call(gsd, video, g_mbus_fmt, fmt);
		if (ret < 0) {
			d_inf(1, "%s: failed to get fmt on %s: %d",
				sd->name, gsd->name, ret);
			return ret;
		} else {
			d_inf(3, "%s: get fmt on %s done", sd->name, gsd->name);
			return 0;
		}
	}

	d_inf(1, "%s: no one handles the video::g_mbus_fmt callback", sd->name);
	return -ENODEV;
}

static int hsd_bundle_video_g_mbus_config(struct v4l2_subdev *sd,
					struct v4l2_mbus_config *cfg)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct tlist_entry *entry;
	struct v4l2_subdev *gsd;

	list_for_each_entry(entry, &hsd->trvrs_list, hook) {
		gsd = entry->item;
		if (!subdev_has_fn(gsd, video, g_mbus_config))
			continue;
		/* only one component in the pipeline knows the mbus config */
		return v4l2_subdev_call(gsd, video, g_mbus_config, cfg);
	}
	return -ENODEV;
}

static int hsd_bundle_video_s_mbus_config(struct v4l2_subdev *sd,
					const struct v4l2_mbus_config *cfg)
{
	struct isp_host_subdev *hsd = container_of(v4l2_get_subdev_hostdata(sd),
					struct isp_host_subdev, isd);
	struct tlist_entry *entry;
	struct v4l2_subdev *gsd;
	int ret = -ENODEV;

	list_for_each_entry(entry, &hsd->trvrs_list, hook) {
		gsd = entry->item;
		if (!subdev_has_fn(gsd, video, s_mbus_config))
			continue;
		/* only one component in the pipeline controls mbus config */
		ret = v4l2_subdev_call(gsd, video, s_mbus_config, cfg);
		if (ret < 0) {
			d_inf(1, "%s: failed to set mbus config on %s: %d",
				sd->name, gsd->name, ret);
			break;
		} else
			d_inf(3, "%s: set mbus config on %s done",
				sd->name, gsd->name);
	}
	return ret;
}

static const struct v4l2_subdev_video_ops hsd_bundle_video_ops = {
	.s_stream		= &hsd_bundle_video_s_stream,
	.g_frame_interval	= &hsd_bundle_video_g_frame_interval,
	.s_frame_interval	= &hsd_bundle_video_s_frame_interval,
	.s_mbus_fmt		= &hsd_bundle_video_s_mbus_fmt,
	.g_mbus_fmt		= &hsd_bundle_video_g_mbus_fmt,
	.s_mbus_config		= &hsd_bundle_video_s_mbus_config,
	.g_mbus_config		= &hsd_bundle_video_g_mbus_config,
};

const struct v4l2_subdev_ops hsd_bundle_ops = {
	.core	= &hsd_bundle_core_ops,
	.video	= &hsd_bundle_video_ops,
};
EXPORT_SYMBOL(hsd_bundle_ops);
static int hsd_bundle_group(struct isp_host_subdev *hsd, u32 group)
{
	struct v4l2_subdev *gsd = NULL;
	struct isp_dev_ptr *dscr;
	struct tlist_entry *entry;
	int ret = 0;

	if (!group)
		goto un_group;

	/* Build up bundle container */
	INIT_LIST_HEAD(&hsd->trvrs_list);
	v4l2_ctrl_handler_init(hsd->isd.subdev.ctrl_handler, 0);

	list_for_each_entry(dscr, &hsd->isd.gdev_list, hook) {
		switch (dscr->type) {
		case ISP_GDEV_SUBDEV:
			{
				struct isp_subdev *isd = dscr->ptr;
				gsd = &isd->subdev;
			}
			break;
		case DEV_V4L2_SUBDEV:
			gsd = dscr->ptr;
			break;
		default:
			ret = -EINVAL;
			goto un_group;
		}
		ret = v4l2_ctrl_add_handler(hsd->isd.subdev.ctrl_handler,
						gsd->ctrl_handler, NULL);
		if (ret < 0)
			goto un_group;

		if (gsd->entity.type != MEDIA_ENT_T_V4L2_SUBDEV_SENSOR)
			continue;
		/*
		 * Traverse list only contains sensor chip guests, but there can
		 * be more than one sensor chip: consider the 3D camera case.
		 */
		entry = devm_kzalloc(hsd->dev, sizeof(*entry), GFP_KERNEL);
		if (entry == NULL) {
			ret = -ENOMEM;
			goto un_group;
		}
		entry->item = gsd;
		INIT_LIST_HEAD(&entry->hook);
		list_add_tail(&entry->hook, &hsd->trvrs_list);
	}

	/* The bundle host subdev has the same pads as the core guest */
	hsd->isd.subdev.entity.ops = gsd->entity.ops;
	ret = media_entity_init(&hsd->isd.subdev.entity, gsd->entity.num_pads,
				gsd->entity.pads, 0);
	if (ret < 0)
		goto un_group;

	hsd->type = TLIST_ENTRY_SUBDEV;
	return 0;

un_group:
	/* dismiss the guests */
	while (!list_empty(&hsd->trvrs_list)) {
		entry = list_first_entry(&hsd->trvrs_list,
					struct tlist_entry, hook);
		list_del(&entry->hook);
		devm_kfree(hsd->dev, entry);
	}
	v4l2_ctrl_handler_free(&hsd->isd.ctrl_handler);
	v4l2_ctrl_handler_init(&hsd->isd.ctrl_handler, 0);
	media_entity_cleanup(&hsd->isd.subdev.entity);
	return 0;
}
struct host_subdev_pdata hsd_bundle_behaviors = {
	.subdev_ops	= &hsd_bundle_ops,
	.group		= &hsd_bundle_group,
};
EXPORT_SYMBOL(hsd_bundle_behaviors);

/************************* host subdev implementation *************************/
static int host_subdev_remove(struct platform_device *pdev)
{
	struct isp_host_subdev *host = platform_get_drvdata(pdev);

	if (unlikely(host == NULL))
		return -EINVAL;

	if (host->group)
		host->group(host, 0);

	/* If any guest exist, remove it */
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

	devm_kfree(host->dev, host);
	return 0;
}

static int host_subdev_probe(struct platform_device *pdev)
{
	/* pdev->dev.platform_data */
	struct isp_host_subdev *host;
	struct v4l2_subdev *sd;
	struct host_subdev_pdata *pdata = pdev->dev.platform_data;
	int ret = 0;

	host = devm_kzalloc(&pdev->dev, sizeof(*host), GFP_KERNEL);
	if (unlikely(host == NULL))
		return -ENOMEM;

	platform_set_drvdata(pdev, host);
	host->dev = &pdev->dev;
	host->group = pdata->group;
	INIT_LIST_HEAD(&host->isd.gdev_list);
	ret = v4l2_ctrl_handler_init(&host->isd.ctrl_handler, 0);
	if (ret < 0)
		goto err;
	host->isd.subdev.ctrl_handler = &host->isd.ctrl_handler;

	/* v4l2_subdev_init(sd, &host_subdev_ops); */
	sd = &host->isd.subdev;
	sd->internal_ops = &host_subdev_internal_ops;
	v4l2_set_subdevdata(sd, host);
	v4l2_subdev_init(sd, pdata->subdev_ops);
	sd->grp_id = GID_ISP_SUBDEV;
	strcpy(sd->name, pdata->name);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	d_inf(2, "host subdev \"%s\" created", sd->name);
	return ret;
err:
	host_subdev_remove(pdev);
	return ret;
}

static struct platform_driver __refdata host_subdev_pdrv = {
	.probe	= host_subdev_probe,
	.remove	= host_subdev_remove,
	.driver	= {
		.name   = HOST_SUBDEV_DRV_NAME,
		.owner  = THIS_MODULE,
	},
};

module_platform_driver(host_subdev_pdrv);

MODULE_DESCRIPTION("host v4l2 subdev bus driver");
MODULE_AUTHOR("Jiaquan Su <jiaquan.lnx@gmail.com>");
MODULE_ALIAS("platform:host-subdev-pdrv");
