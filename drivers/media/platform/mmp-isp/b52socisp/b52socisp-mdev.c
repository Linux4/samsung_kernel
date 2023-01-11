#include <linux/module.h>
#include <linux/export.h>
#include <linux/slab.h>

#include <media/b52socisp/b52socisp-mdev.h>

static int trace = 2;
module_param(trace, int, 0644);
MODULE_PARM_DESC(trace,
		"how many trace do you want to see? (0-4):"
		"0 - mute "
		"1 - only actual errors"
		"2 - milestone log"
		"3 - briefing log"
		"4 - detailed log");

struct isp_subdev *isp_subdev_find(struct isp_build *build, int sd_code)
{
	struct isp_subdev *ispsd;

	list_for_each_entry(ispsd, &build->ispsd_list, hook) {
		if (ispsd->sd_code == sd_code)
			return ispsd;
	}
	return NULL;
};

int isp_subdev_add_guest(struct isp_subdev *isd,
				void *guest, enum isp_gdev_type type)
{
	struct isp_dev_ptr *desc = devm_kzalloc(isd->build->dev,
						sizeof(*desc), GFP_KERNEL);
	if (desc == NULL)
		return -ENOMEM;
	desc->ptr = guest;
	desc->type = type;
	INIT_LIST_HEAD(&desc->hook);
	list_add_tail(&desc->hook, &isd->gdev_list);
	return 0;
}

int isp_subdev_add_host(struct isp_subdev *isd,
				void *host, enum isp_gdev_type type)
{
	struct isp_dev_ptr *desc = devm_kzalloc(isd->build->dev,
						sizeof(*desc), GFP_KERNEL);
	if (desc == NULL)
		return -ENOMEM;
	desc->ptr = host;
	desc->type = type;
	INIT_LIST_HEAD(&desc->hook);
	list_add_tail(&desc->hook, &isd->hdev_list);
	return 0;
}

int isp_subdev_remove_guest(struct isp_subdev *isd, void *guest)
{
	struct isp_dev_ptr *desc;

	list_for_each_entry(desc, &isd->gdev_list, hook) {
		if (guest == desc->ptr)
			goto find;
	}
	return -ENODEV;

find:
	list_del(&desc->hook);
	devm_kfree(isd->build->dev, desc);
	return 0;
}

int isp_subdev_remove_host(struct isp_subdev *isd, void *host)
{
	struct isp_dev_ptr *desc;

	list_for_each_entry(desc, &isd->hdev_list, hook) {
		if (host == desc->ptr)
			goto find;
	}
	return -ENODEV;

find:
	list_del(&desc->hook);
	devm_kfree(isd->build->dev, desc);
	return 0;
}

void isp_subdev_unregister(struct isp_subdev *ispsd)
{
	ispsd->build = NULL;
	media_entity_cleanup(&ispsd->subdev.entity);
}

int isp_subdev_register(struct isp_subdev *ispsd, struct isp_build *build)
{
	struct v4l2_subdev *sd = &ispsd->subdev;
	struct media_entity *me = &sd->entity;
	int ret;

	if (ispsd->single) {
		void *guest = isp_sd2blk(ispsd);
		if (guest) {
			struct isp_block *blk = guest;
			if (strlen(sd->name) == 0)
				strlcpy(sd->name, blk->name, sizeof(sd->name));
			isp_block_register(blk, &build->resrc_pool);
		}
	} else {
		/* Combined isp-subdev */
		if (strlen(sd->name) == 0)
			return -EINVAL;
	}

	v4l2_set_subdev_hostdata(sd, ispsd);
	if (!sd->ctrl_handler)
		sd->ctrl_handler = &ispsd->ctrl_handler;
	sd->grp_id = GID_ISP_SUBDEV;

	/* media entity init */
	ret = media_entity_init(me, ispsd->pads_cnt, ispsd->pads, 0);
	if (ret < 0)
		return ret;
	/* This media entity is contained by a subdev */
	me->type = MEDIA_ENT_T_V4L2_SUBDEV;
	/* ispsd member init */
	ispsd->build = build;
	INIT_LIST_HEAD(&ispsd->hook);
	INIT_LIST_HEAD(&ispsd->hdev_list);
	list_add_tail(&ispsd->hook, &build->ispsd_list);
	d_log("module '%s' registered to '%s'", me->name, build->name);
	return ret;
}

int isp_entity_get_fmt(struct media_entity *me, struct v4l2_subdev_format *fmt)
{
	struct v4l2_subdev *sd;

	if (unlikely(fmt->pad >= me->num_pads))
		return -EINVAL;
	if (media_entity_type(me) != MEDIA_ENT_T_V4L2_SUBDEV)
		return 0;
	sd = media_entity_to_v4l2_subdev(me);
	/* If this subdev don't care about format */
	if (!subdev_has_fn(sd, pad, get_fmt))
		return 0;
	d_inf(4, "%s: get fmt on pad(%d)", me->name, fmt->pad);
	return v4l2_subdev_call(sd, pad, get_fmt, NULL, fmt);
}

static inline int _mbus_fmt_same(struct v4l2_mbus_framefmt *fmt1,
				struct v4l2_mbus_framefmt *fmt2)
{
	return (fmt1->code == fmt2->code)
		&& (fmt1->width == fmt2->width)
		&& (fmt1->height == fmt2->height);
}

/* A pad can belong to a devnode / subdev */
int isp_entity_try_set_fmt(struct media_entity *me,
				struct v4l2_subdev_format *fmt)
{
	struct isp_subdev *ispsd = NULL;
	struct v4l2_subdev *sd;
	struct v4l2_mbus_framefmt *fmt_now = NULL;
	int ret = 0;

	if (media_entity_type(me) != MEDIA_ENT_T_V4L2_SUBDEV)
		return 0;
	sd = media_entity_to_v4l2_subdev(me);
	/* This media entity is a subdev, but don't care about format */
	if (!subdev_has_fn(sd, pad, set_fmt))
		return 0;

	d_inf(3, "set format on %s", sd->name);
	/* Maybe new format is the same with current format? */
	if (sd->grp_id == GID_ISP_SUBDEV) {
		ispsd = v4l2_get_subdev_hostdata(sd);
		fmt_now = ispsd->fmt_pad + fmt->pad;
		/* if current format is the same as new format, skip */
		if (_mbus_fmt_same(fmt_now, &fmt->format)) {
			d_inf(4, "new format same as current format, skip");
			return 0;
		}
	}

	/* Bad Luck, now really apply the format on entity */
	ret = v4l2_subdev_call(sd, pad, set_fmt, NULL, fmt);
	if (ret < 0)
		return ret;
	if (ispsd)
		memcpy(fmt_now, &fmt->format, sizeof(*fmt_now));

	return ret;
}

int isp_link_try_set_fmt(struct media_link *link,
				struct v4l2_mbus_framefmt *fmt, __u32 which)
{
	struct v4l2_subdev_format pad_fmt;
	int ret;

	d_inf(4, "link %s<%d> => %s<%d>: set fmt %X(%d, %d)",
		link->source->entity->name, link->source->index,
		link->sink->entity->name, link->sink->index,
		fmt->code, fmt->width, fmt->height);
	memcpy(&pad_fmt.format, fmt, sizeof(struct v4l2_mbus_framefmt));
	pad_fmt.which = which;
	pad_fmt.pad = link->source->index;

	ret = isp_entity_try_set_fmt(link->source->entity, &pad_fmt);
	if (ret < 0) {
		d_inf(1, "set format failed on source %s<%d>: ret %d",
			link->source->entity->name, link->source->index, ret);
		return ret;
	}
	pad_fmt.which = which;
	pad_fmt.pad = link->sink->index;

	ret = isp_entity_try_set_fmt(link->sink->entity, &pad_fmt);
	if (ret < 0) {
		d_inf(1, "set format failed on sink %s<%d>: ret %d",
			link->sink->entity->name, link->sink->index, ret);
		return ret;
	}
	memcpy(fmt, &pad_fmt.format, sizeof(*fmt));
	return ret;
}

static int isp_link_notify(struct media_link *link, unsigned int flags,
				unsigned int notification)
{
	struct media_entity *dst = link->sink->entity;
	struct media_entity *src = link->source->entity;
	struct isp_subdev *src_ispsd = NULL, *dst_ispsd = NULL;
	struct v4l2_subdev *src_sd = NULL, *dst_sd = NULL;
	void *guest;
	int ret = 0;

	if (((notification == MEDIA_DEV_NOTIFY_POST_LINK_CH) &&
		 (flags & MEDIA_LNK_FL_ENABLED)) ||
		((notification == MEDIA_DEV_NOTIFY_PRE_LINK_CH) &&
		 !(flags & MEDIA_LNK_FL_ENABLED)))
		return 0;

	if (unlikely(src == NULL || dst == NULL))
		return -EINVAL;
	/* Source and sink of the link may not be an ispsd, carefully convert
	 * to isp_subdev if possible */
	if (media_entity_type(src) == MEDIA_ENT_T_V4L2_SUBDEV) {
		src_sd = media_entity_to_v4l2_subdev(src);
		if (src_sd->grp_id == GID_ISP_SUBDEV)
			src_ispsd = v4l2_get_subdev_hostdata(src_sd);
	}
	if (media_entity_type(dst) == MEDIA_ENT_T_V4L2_SUBDEV) {
		dst_sd = media_entity_to_v4l2_subdev(dst);
		if (dst_sd->grp_id == GID_ISP_SUBDEV)
			dst_ispsd = v4l2_get_subdev_hostdata(dst_sd);
	}
	if ((flags & MEDIA_LNK_FL_ENABLED) == 0)
		goto dst_sd_off;

	/* power on sequence */
	if (src_ispsd && src_ispsd->single) {
		guest = isp_sd2blk(src_ispsd);
		if (guest) {
			ret = isp_block_tune_power(guest, 1);
			if (ret < 0)
				goto exit;
		}
	}
	if (src_sd) {
		/*
		 * For single subdev, simply call v4l2_subdev's s_power.
		 * For combined subdev, s_power will be implemented by shell
		 * subdev. i.e. for host_subdev, s_power setup links between
		 * guest subdev.
		 */
		ret = v4l2_subdev_call(src_sd, core, s_power, 1);
		if (ret == -ENOIOCTLCMD)
			ret = 0;
		if (ret < 0)
			goto src_hw_off;
	}

	if (dst_ispsd && dst_ispsd->single) {
		guest = isp_sd2blk(dst_ispsd);
		if (guest) {
			ret = isp_block_tune_power(guest, 1);
			if (ret < 0)
				goto src_sd_off;
		}
	}
	if (dst_sd) {
		ret = v4l2_subdev_call(dst_sd, core, s_power, 1);
		if (ret == -ENOIOCTLCMD)
			ret = 0;
		if (ret < 0)
			goto dst_hw_off;
	}
	return 0;

	/* power off sequence */
dst_sd_off:
	if (dst_sd)
		v4l2_subdev_call(dst_sd, core, s_power, 0);
dst_hw_off:
	if (dst_ispsd && dst_ispsd->single) {
		guest = isp_sd2blk(dst_ispsd);
		if (guest)
			isp_block_tune_power(guest, 0);
	}
src_sd_off:
	if (src_sd)
		v4l2_subdev_call(src_sd, core, s_power, 0);
src_hw_off:
	if (src_ispsd && src_ispsd->single) {
		guest = isp_sd2blk(src_ispsd);
		if (guest)
			isp_block_tune_power(guest, 0);
	}
exit:
	return ret;
}

static void isp_close_notify(struct media_device *mdev)
{
	struct isp_build *build = container_of(mdev,
		struct isp_build, media_dev);

	/* 1st, stream off all the video device */
	if (build->close_vdev)
		build->close_vdev(build);
	/*
	 * 2nd, release all the pipeline.
	 * It's optional, because framework will take care of it too
	 */
};

void isp_build_exit(struct isp_build *build)
{
	/* FIXME: add clean up code here */
}

int isp_build_init(struct isp_build *mngr)
{
	int ret = 0;

	/* suppose by this point, all H/W subdevs had been registed to
	 * H/W manager */
	if (unlikely(mngr == NULL))
		return -EINVAL;

	/* distribute H/W resource */

	INIT_LIST_HEAD(&mngr->pipeline_pool);
	mngr->pipeline_cnt = 0;
	mutex_init(&mngr->graph_mutex);

	/* register media entity */
	BUG_ON(mngr->name == NULL);
	mngr->media_dev.dev = mngr->dev;
	strlcpy(mngr->media_dev.model, mngr->name,
		sizeof(mngr->media_dev.model));
	mngr->media_dev.link_notify = isp_link_notify;
	mngr->media_dev.close_notify = isp_close_notify;
	ret = media_device_register(&mngr->media_dev);
	/* v4l2 device register */
	mngr->v4l2_dev.mdev = &mngr->media_dev;
	ret = v4l2_device_register(mngr->dev, &mngr->v4l2_dev);
	return ret;
}

int isp_build_attach_ispsd(struct isp_build *build)
{
	int ret;
	struct isp_subdev *ispsd;

	list_for_each_entry(ispsd, &build->ispsd_list, hook) {
		if (build->add_vdev) {
			/*
			 * Platform based ISP driver can use this call back to
			 * define video_dev/pipeline creating strategy of its
			 * own.
			 */
			ret = (*build->add_vdev)(build, ispsd);
			if (ret < 0)
				return ret;
		}
		ret = v4l2_device_register_subdev(&build->v4l2_dev,
							&ispsd->subdev);
		if (ret < 0) {
			d_inf(1, "failed to register '%s':%d",
				ispsd->subdev.name, ret);
			return ret;
		}
		d_inf(3, "subdev '%s' add to '%s'",
			ispsd->subdev.name, build->name);
	}

	/* Finally, create all the file nodes for each subdev */
	return v4l2_device_register_subdev_nodes(&build->v4l2_dev);
}

