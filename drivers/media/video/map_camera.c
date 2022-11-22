#include <linux/export.h>
#include <linux/slab.h>

#include <media/map_camera.h>

inline struct map_agent *me_to_agent(struct media_entity *me)
{
	struct v4l2_subdev *sd;

	if (media_entity_type(me) == MEDIA_ENT_T_V4L2_SUBDEV)
		sd = container_of(me, struct v4l2_subdev, entity);
	else
		return NULL;
	if (sd->grp_id == GID_AGENT_GROUP)
		return container_of(sd, struct map_agent, subdev);
	else
		return NULL;
}

void map_agent_unregister(struct map_agent *agent)
{
	agent->manager = NULL;
	media_entity_cleanup(&agent->subdev.entity);
}

int map_agent_register(struct map_agent *agent)
{
	struct map_manager *mngr = agent->hw.mngr->drv_priv;
	struct v4l2_subdev *sd = &agent->subdev;
	struct media_entity *me = &sd->entity;
	int ret;

	/* subdev init */
	strlcpy(sd->name, agent->hw.name, sizeof(sd->name));
	agent->hw.name = sd->name;
	v4l2_set_subdevdata(sd, agent);
	sd->grp_id = GID_AGENT_GROUP;
	if (agent->ctrl_cnt)
		sd->ctrl_handler = &agent->ctrl_handler;

	/* media entity init */
	ret = media_entity_init(me, agent->pads_cnt, agent->pads, 0);
	if (ret < 0)
		return ret;
	/* This media entity is contained by a subdev */
	me->type = MEDIA_ENT_T_V4L2_SUBDEV;

	/* agent*/
	agent->manager = mngr;
	agent->user_map = 0;
	spin_lock_init(&agent->user_lock);

	return ret;
}

static inline int map_agent_set_power(struct map_agent *agent, int onoff)
{
	int ret = 0;

	if (onoff == 1) {
		/* 1st: hardware power on/off */
		ret = hw_tune_power(&agent->hw, onoff);
		if (ret < 0)
			goto exit;

		/* 2nd: subdev power on/off */
		if (agent->subdev.ops->core->s_power)
			ret = (*agent->subdev.ops->core->s_power)(
				&agent->subdev, onoff);
		if (ret < 0)
			goto exit;
	} else {
		if (agent->subdev.ops->core->s_power)
			(*agent->subdev.ops->core->s_power)(
				&agent->subdev, onoff);
		hw_tune_power(&agent->hw, onoff);
	}

exit:
	return ret;
}

static inline int map_entity_get_fmt(struct media_entity *me,
				struct v4l2_subdev_format *fmt)
{
	struct map_agent *agent;
	struct v4l2_subdev *sd;

	if (unlikely(fmt->pad >= me->num_pads))
		return -EINVAL;
	if (media_entity_type(me) != MEDIA_ENT_T_V4L2_SUBDEV)
		return 0;
	sd = media_entity_to_v4l2_subdev(me);
	if ((sd->grp_id == GID_AGENT_GROUP) &&
		!subdev_has_fn(sd, pad, get_fmt)) {
		/* if the subdev locates inside a agent, and is willing to use
		 * the default version of subdev::pad::get_fmt */
		/* In map agent, the format applied to H/W is saved in the
		 * format cache, so don't bother read the H/W */
		memcpy(&fmt->format, agent->fmt_hw + fmt->pad,
			sizeof(struct v4l2_subdev_format));
		return 0;
	}

	return v4l2_subdev_call(sd, pad, get_fmt, NULL, fmt);
}

static int map_entity_try_set_fmt(struct media_entity *me,
				struct v4l2_subdev_format *fmt)
{
	struct map_agent *agent = NULL;
	struct v4l2_subdev *sd;
	struct v4l2_mbus_framefmt *fmt_now;
	int i, ret = 0;

	if (unlikely(fmt->pad >= me->num_pads))
		return -EINVAL;
	if (media_entity_type(me) != MEDIA_ENT_T_V4L2_SUBDEV)
		return 0;
	sd = media_entity_to_v4l2_subdev(me);
	if (sd->grp_id == GID_AGENT_GROUP)
		agent = v4l2_get_subdevdata(sd);

	if (agent) {
		fmt_now = agent->fmt_hw + fmt->pad;
		/* if S/W format is the same as H/W format, skip */
		if (!memcmp(fmt_now, &fmt->format, sizeof(*fmt_now)))
			goto exit;
	}

	/* 1st, try format on local pad*/
	if (subdev_has_fn(sd, pad, set_fmt)) {
		/* FIXME: for cached format feature, should call set_fmt here
		 * only if it's a try format */
		ret = v4l2_subdev_call(sd, pad, set_fmt, NULL, fmt);
		if (ret < 0)
			goto exit;
	}

	/* for each enabled link connected to agent::entity::pad
	 * try format on remote pad */
	for (i = 0; i < sd->entity.num_links; i++) {
		struct media_link *link = me->links + i;
		struct media_entity *remote;
		struct v4l2_subdev_format r_fmt;
		if (!(link->flags &  MEDIA_LNK_FL_ENABLED)
			|| (link->source->entity == me))
			/* don't apply on none-active link or output link */
			continue;
#if 0
		/* convert the format to the link format on the other side of
		 * the subdev */
		if (subdev_has_fn(sd, pad, get_fmt)) {
			r_fmt.pad = link->sink->index;
			ret = v4l2_subdev_call(sd, pad, get_fmt, NULL, &r_fmt);
			if (ret < 0)
				goto exit;
		} else {
			/* This entity don't care format, so just simple pass
			 * down the format to the upwind entity */
			memcpy(&r_fmt, fmt, sizeof(struct v4l2_subdev_format));
		}
#else
		/* This is just a W/R for now, should use converted format */
		memcpy(&r_fmt, fmt, sizeof(struct v4l2_subdev_format));
#endif
		r_fmt.pad = link->source->index;
		remote = link->source->entity;
		ret = map_entity_try_set_fmt(remote, &r_fmt);
		if (ret < 0) {
			printk(KERN_ERR "map_camera: try/set_fmt %X(%d, %d) " \
				"on '%s' pad%d failed with err: %d\n",
				r_fmt.format.code,
				r_fmt.format.width, r_fmt.format.height,
				remote->name, r_fmt.pad, ret);
			goto exit;
		}
	}

	if (agent) {
		memcpy(agent->fmt_hw + fmt->pad, &fmt->format,
			sizeof(struct v4l2_mbus_framefmt));
	}
exit:
	return ret;
}

struct map_vnode *agent_get_video(struct map_agent *agent)
{
	struct media_entity *vdev_me, *me = &agent->subdev.entity;
	int i;

	if (agent == NULL)
		return NULL;
	if ((agent->hw.id.mod_type != MAP_AGENT_DMA_OUT)
		&& (agent->hw.id.mod_type != MAP_AGENT_DMA_IN))
		return NULL;

	for (i = 0; i < me->num_links; i++) {
		struct media_link *link = &me->links[i];
		if (link->source->entity == me)
			vdev_me = link->sink->entity;
		else
			vdev_me = link->source->entity;
		if (vdev_me->type == MEDIA_ENT_T_DEVNODE_V4L)
			goto find;
	}
	return NULL;
find:
	return container_of(vdev_me, struct map_vnode, vdev.entity);
}

struct map_agent *video_get_agent(struct map_vnode *vnode)
{
	struct media_entity *agent_me, *me = &vnode->vdev.entity;
	struct map_agent *agent;
	int i;

	if (unlikely(vnode == NULL))
		return NULL;

	for (i = 0; i < me->num_links; i++) {
		struct media_link *link = &me->links[i];
		if (link->source->entity == me)
			agent_me = link->sink->entity;
		else
			agent_me = link->source->entity;
		if (agent_me->type == MEDIA_ENT_T_V4L2_SUBDEV)
			goto find;
	}
	return NULL;
find:
	agent = v4l2_get_subdevdata(
		container_of(agent_me, struct v4l2_subdev, entity));
	if ((agent->hw.id.mod_type != MAP_AGENT_DMA_OUT)
		&& (agent->hw.id.mod_type != MAP_AGENT_DMA_IN))
		return NULL;
	return agent;
}

int map_link_notify(struct media_pad *src, struct media_pad *dst, u32 flags)
{
	struct map_agent *src_agent = NULL, *dst_agent = NULL;
	struct v4l2_subdev *src_sd = NULL, *dst_sd = NULL;
	int ret = 0;

	if (unlikely(src == NULL || dst == NULL))
		return -EINVAL;
	/* Source and sink of the link may not be an agent, carefully convert
	 * to map_agent if possible */
	if (media_entity_type(src->entity) == MEDIA_ENT_T_V4L2_SUBDEV) {
		src_sd = container_of(src->entity, struct v4l2_subdev, entity);
		if (src_sd->grp_id == GID_AGENT_GROUP)
			src_agent = container_of(src_sd, struct map_agent,
						subdev);
	}
	if (media_entity_type(dst->entity) == MEDIA_ENT_T_V4L2_SUBDEV) {
		dst_sd = container_of(dst->entity, struct v4l2_subdev, entity);
		if (dst_sd->grp_id == GID_AGENT_GROUP)
			dst_agent = container_of(dst_sd, struct map_agent,
						subdev);
	}

	if (flags & MEDIA_LNK_FL_ENABLED) {
		if (src_agent) {
			ret = map_agent_set_power(src_agent, 1);
			if (ret < 0)
				goto exit;
		} else {
			if (src_sd)
				ret = v4l2_subdev_call(src_sd,
							core, s_power, 1);
			if (ret == -ENOIOCTLCMD)
				ret = 0;
			if (ret < 0)
				goto exit;
		}

		if (dst_agent) {
			ret = map_agent_set_power(dst_agent, 1);
			if (ret < 0)
				goto src_off;
		} else {
			if (dst_sd)
				ret = v4l2_subdev_call(dst_sd,
							core, s_power, 1);
			if (ret == -ENOIOCTLCMD)
				ret = 0;
			if (ret < 0)
				goto exit;
		}
		return 0;
	}

	if (dst_agent)
		map_agent_set_power(dst_agent, 0);
	else if (dst_sd)
		v4l2_subdev_call(dst_sd, core, s_power, 0);
src_off:
	if (src_agent)
		map_agent_set_power(src_agent, 0);
	else if (src_sd)
		v4l2_subdev_call(src_sd, core, s_power, 1);
exit:
	return ret;
}

/* recursive look for the route from end to start */
/* end is never NULL, start can be NULL */
/* FIXME: use stack to optimize later*/
int map_find_route(struct media_entity *start, struct media_entity *end,
	struct media_link **route, int max_pads, int flag_set, int flag_clr)
{
	struct media_entity *last;
	int i, ret = 0;
	/*printk("pad left%d\n", max_pads);*/
	if (max_pads < 1)
		return -ENOMEM;

	/* search for all possible source entity */
	for (i = 0; i < end->num_links; i++) {
		/*printk("link%d/%d: %s => %s [%d]\n", i, end->num_links,
	end->links[i].source->entity->name, end->links[i].sink->entity->name,
	end->links[i].flags);*/
		/* if the link is outbound or not satisfy flag, ignore */
		if ((end->links[i].sink->entity != end)
		|| (flag_set && ((end->links[i].flags & flag_set) == 0))
		|| (flag_clr && ((end->links[i].flags & flag_clr) != 0)))
			continue;
		/* The use count of each agent is not considered here, because
		 * this function only find the route, but not guarantee the
		 * nodes on the route is idle */
		last = end->links[i].source->entity;
		if (last == start)
			goto output_route;
		else {
			int tmp = map_find_route(start, last, route + 1,
					max_pads - 1, flag_set, flag_clr);
			/*printk("recur ret is %d\n", tmp);*/
			if (tmp < 0) {
				continue;
			} else {
				ret = tmp;
				goto output_route;
			}
		}
	}

	/* if pipeline input not specified, but current subdev is a sensor,
	 * we can assume that it can act as a pipeline source */
	if ((start == NULL) && (i >= end->num_links)) {
		struct v4l2_subdev *sd = NULL;
		if (end->type == MEDIA_ENT_T_V4L2_SUBDEV_SENSOR)
			sd = container_of(end, struct v4l2_subdev, entity);
		if (subdev_has_fn(sd, pad, set_fmt))
			return 0;
	}
	return -ENODEV;

output_route:
	*route = &end->links[i];
	/*printk("find route %s => %s\n", end->links[i].source->entity->name,
	end->links[i].sink->entity->name);*/
	return ret + 1;
}

static inline void map_pipeline_destroy(struct map_pipeline *pipe)
{
	mvisp_video_unregister(&pipe->output);
	list_del(&pipe->hook);
	kfree(pipe);
}

int map_pipeline_create(struct map_manager *mngr, struct map_agent *dma_agent)
{
	struct map_pipeline *pipeline;
	int ret = 0;

	pipeline = kzalloc(sizeof(struct map_pipeline), GFP_KERNEL);
	if (pipeline == NULL) {
		ret = -ENOMEM;
		goto exit;
	}
	INIT_LIST_HEAD(&pipeline->hook);
	list_add_tail(&pipeline->hook, &mngr->pipeline_pool);
	pipeline->id = mngr->pipeline_cnt++;
	mutex_init(&pipeline->st_lock);
	mutex_init(&pipeline->fmt_lock);

	pipeline->mngr = mngr;

	/* Video device node */
	pipeline->output.ops = dma_agent->vops;
	pipeline->output.pipeline = pipeline;

	ret = mvisp_video_register(&pipeline->output, &mngr->v4l2_dev,
					dma_agent->video_type);
	if (ret < 0)
		goto err_link;

	/* create link between agent and associated video_device */
	BUG_ON(dma_agent->pad_vdev < 0);
	ret = media_entity_create_link(
				&dma_agent->subdev.entity, dma_agent->pad_vdev,
				&pipeline->output.vdev.entity, 0, 0);
	if (ret < 0)
		goto err_link;
	return ret;

err_link:
	map_pipeline_destroy(pipeline);
exit:
	return ret;
}

static inline int pipeline_get(struct map_pipeline *pipeline, int *err_link)
{
	int i, ret = 0, alive;

	*err_link = 0;
	for (i = 0; i < pipeline->route_sz; i++) {
		struct map_agent *src =
				me_to_agent(pipeline->route[i]->source->entity);
		struct map_agent *dst =
				me_to_agent(pipeline->route[i]->sink->entity);
		alive = 0;
		if ((src == NULL) || (dst == NULL))
			goto set_link;

		spin_lock(&src->user_lock);
		spin_lock(&dst->user_lock);
		alive = src->user_map & dst->user_map;
		set_bit(pipeline->id, &src->user_map);
		set_bit(pipeline->id, &dst->user_map);
		spin_unlock(&dst->user_lock);
		spin_unlock(&src->user_lock);

set_link:
		if (!alive)
			ret = media_entity_setup_link(pipeline->route[i],
						MEDIA_LNK_FL_ENABLED);
		if (WARN_ON(ret < 0)) {
			if (src && dst) {
				spin_lock(&src->user_lock);
				spin_lock(&dst->user_lock);
				clear_bit(pipeline->id, &src->user_map);
				clear_bit(pipeline->id, &dst->user_map);
				spin_unlock(&dst->user_lock);
				spin_unlock(&src->user_lock);
			}
			printk(KERN_ERR "map: link %s => %s enable ret %d\n",
				pipeline->route[i]->source->entity->name,
				pipeline->route[i]->sink->entity->name, ret);
			return -EPIPE;
		}
	}
	pipeline->state = MAP_PIPELINE_IDLE;
	return 0;
}

static inline void pipeline_put(struct map_pipeline *pipeline, int start_link)
{
	int i, ret = 0, alive;

	WARN_ON((pipeline->state != MAP_PIPELINE_DEAD)
		&& (pipeline->state != MAP_PIPELINE_IDLE));

	for (i = start_link; i >= 0; i--) {
		struct map_agent *src =
				me_to_agent(pipeline->route[i]->source->entity);
		struct map_agent *dst =
				me_to_agent(pipeline->route[i]->sink->entity);
		alive = 0;
		if ((src == NULL) || (dst == NULL))
			goto set_link;

		spin_lock(&src->user_lock);
		spin_lock(&dst->user_lock);
		clear_bit(pipeline->id, &src->user_map);
		clear_bit(pipeline->id, &dst->user_map);
		alive = src->user_map & dst->user_map;
		spin_unlock(&dst->user_lock);
		spin_unlock(&src->user_lock);
set_link:
		if (!alive)
			ret = media_entity_setup_link(pipeline->route[i],
						!MEDIA_LNK_FL_ENABLED);
		if (WARN_ON(ret < 0))
			printk(KERN_ERR "map: link %s => %s disable ret %d\n",
				pipeline->route[i]->source->entity->name,
				pipeline->route[i]->sink->entity->name, ret);
	}
}

void map_pipeline_clean(struct map_pipeline *pipe)
{
	pipeline_put(pipe, pipe->route_sz - 1);
	memset(pipe->route, 0, sizeof(pipe->route));
	pipe->route_sz = 0;
	pipe->input = NULL;
	pipe->state = MAP_PIPELINE_DEAD;
}

int map_pipeline_setup(struct map_pipeline *pipe)
{
	int err, ret = 0;

	memset(pipe->route, 0, sizeof(pipe->route));
	/* the passed pads will be saved as route */
	pipe->route_sz = map_find_route(
				pipe->input, &pipe->output.vdev.entity,
				pipe->route, MC_PIPELINE_LEN_MAX, 0, 0);
	if (pipe->route_sz <= 0) {
		printk(KERN_ERR "map_cam: can't find a route from %s to %s\n",
				(pipe->input) ? pipe->input->name : "<NULL>",
				pipe->output.vdev.entity.name);
		return -EPIPE;
	}
	/* Check & get refcnt for every agent on the pipeline, link[0].sink is
	 * video_dev, no refcnt */
	ret = pipeline_get(pipe, &err);
	if (ret < 0 || err)
		goto err;
	return ret;

err:
	pipeline_put(pipe, err);
	return ret;
}

/* check the integraty of pipeline */
/* only called after pipeline is modified by user space APP */
int map_pipeline_validate(struct map_pipeline *pipeline)
{
	memset(pipeline->route, 0, sizeof(pipeline->route));
	/* When validate the pipeline, only the enabled link is passable*/
	pipeline->route_sz = map_find_route(NULL,
				&pipeline->output.vdev.entity,
				pipeline->route, MC_PIPELINE_LEN_MAX,
				MEDIA_LNK_FL_ENABLED, 0);
	if (pipeline->route_sz <= 0)
		return -EPIPE;

	pipeline->input = pipeline->route[pipeline->route_sz-1]->source->entity;
	pipeline->state = MAP_PIPELINE_IDLE;
	return 0;
}
EXPORT_SYMBOL(map_pipeline_validate);

/**************************** Implement V4L2 APIs *****************************/

int map_pipeline_querycap(struct map_pipeline *pipeline,
			struct v4l2_capability *cap)
{
	struct map_manager *mngr;
	int ret = 0;

	mutex_lock(&pipeline->st_lock);
	if (pipeline->state <= MAP_PIPELINE_DEAD) {
		ret = -EPERM;
		printk(KERN_ERR "map_vnode: pipeline not established yet\n");
		goto exit_unlock;
	}

	mngr = pipeline->mngr;
	strlcpy(cap->driver, mngr->name, sizeof(cap->driver));
	/* Get pipeline head entity name */
	strlcpy(cap->card,
		pipeline->route[pipeline->route_sz-1]->source->entity->name,
		sizeof(cap->card));
	/* TODO: get platform specific information in cap->reserved */

exit_unlock:
	mutex_unlock(&pipeline->st_lock);
	return ret;
}

int map_pipeline_set_stream(struct map_pipeline *pipeline, int enable)
{
	int i = pipeline->route_sz - 1, ret = 0, err = 0;

	if (pipeline->route_sz <= 0)
		return -EPERM;

	if (enable) {
		if (WARN_ON(pipeline->state != MAP_PIPELINE_IDLE)) {
			ret = -EPIPE;
			goto exit;
		}
		pipeline->route[0]->sink->entity->stream_count++;
		/* invoke s_stream on each subdev,
		 * down the direction of dataflow */
		for (i = 0; i < pipeline->route_sz; i++) {
			struct media_entity *me =
				pipeline->route[i]->source->entity;
			struct v4l2_subdev *sd =
				container_of(me, struct v4l2_subdev, entity);

			if (me->stream_count >= 1)
				goto inc_cnt;
			if (media_entity_type(me) != MEDIA_ENT_T_V4L2_SUBDEV)
				goto inc_cnt;
			/* TODO: If it's an agent, apply final format here! */
			ret = v4l2_subdev_call(sd, video, s_stream, 1);
			if (ret < 0) {
				printk(KERN_ERR "map_camera: stream_on failed "\
					"at %s: %d\n", me->name, ret);
				goto stream_off;
			}
inc_cnt:
			me->stream_count++;
		}
		pipeline->state = MAP_PIPELINE_BUSY;
		goto exit;
	}

stream_off:
	/* stream_on failed or duplicated stream_off*/
	if (WARN_ON(pipeline->state != MAP_PIPELINE_BUSY))
		goto exit;
	for (; i >= 0; i--) {
		struct media_entity *me =
				pipeline->route[i]->source->entity;
		struct v4l2_subdev *sd =
				container_of(me, struct v4l2_subdev, entity);

		if (me->stream_count > 1)
			goto dec_cnt;
		if (media_entity_type(me) != MEDIA_ENT_T_V4L2_SUBDEV)
			goto dec_cnt;
		ret = v4l2_subdev_call(sd, video, s_stream, 0);
		if (ret < 0) {
			err = ret;
			printk(KERN_ERR "map_camera: stream_off failed "\
					"at %s: %d\n", me->name, ret);
		}
dec_cnt:
		me->stream_count--;
	}
	pipeline->route[0]->sink->entity->stream_count--;
	pipeline->state = MAP_PIPELINE_IDLE;
exit:
	return ret | err;
};
EXPORT_SYMBOL(map_pipeline_set_stream);

/* apply set_format on the whole pipeline */
int map_pipeline_set_format(struct map_pipeline *pipeline,
				struct v4l2_mbus_framefmt *fmt)
{
	struct v4l2_subdev_format sd_fmt;
	struct media_entity *me;
	struct v4l2_subdev *sd;
	int i;

	/* pipeline format is determined by the last subdev in the pipeline,
	 * that is capable to do format convertion */
	me = pipeline->route[0]->sink->entity;
	memcpy(&sd_fmt.format, fmt, sizeof(struct v4l2_mbus_framefmt));
	sd_fmt.pad = pipeline->route[0]->sink->index;
	for (i = 0; i < pipeline->route_sz; i++) {
		if (media_entity_type(me) == MEDIA_ENT_T_V4L2_SUBDEV) {
			sd = media_entity_to_v4l2_subdev(me);
			if (subdev_has_fn(sd, pad, set_fmt))
				goto set_fmt;
		}
		me = pipeline->route[i]->source->entity;
		sd_fmt.pad = pipeline->route[i]->source->index;
	}
	return -EPIPE;

set_fmt:
	sd_fmt.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	return map_entity_try_set_fmt(me, &sd_fmt);
}
EXPORT_SYMBOL(map_pipeline_set_format);

/* apply set_format on the whole pipeline */
int map_pipeline_try_format(struct map_pipeline *pipeline,
				struct v4l2_mbus_framefmt *fmt)
{
	struct v4l2_subdev_format sd_fmt;
	struct media_entity *me;
	struct v4l2_subdev *sd;
	int i;

	/* pipeline format is determined by the last subdev in the pipeline,
	 * that is capable to do format convertion */
	me = pipeline->route[0]->sink->entity;
	sd_fmt.pad = pipeline->route[0]->sink->index;
	for (i = -1; i < pipeline->route_sz;) {
		if (media_entity_type(me) == MEDIA_ENT_T_V4L2_SUBDEV) {
			sd = media_entity_to_v4l2_subdev(me);
			if (subdev_has_fn(sd, pad, set_fmt))
				goto set_fmt;
		}
		i++;
		me = pipeline->route[i]->source->entity;
		sd_fmt.pad = pipeline->route[i]->source->index;
	}
	return -EPIPE;

set_fmt:
	sd_fmt.which = V4L2_SUBDEV_FORMAT_TRY;
	return map_entity_try_set_fmt(me, &sd_fmt);
}
EXPORT_SYMBOL(map_pipeline_try_format);

int map_pipeline_get_format(struct map_pipeline *pipeline,
				struct v4l2_mbus_framefmt *fmt)
{
	struct v4l2_subdev_format sd_fmt;
	struct media_entity *me;
	struct v4l2_subdev *sd;
	int i;

	/* pipeline format is determined by the last subdev in the pipeline,
	 * that is capable to do format convertion */
	me = pipeline->route[0]->sink->entity;
	sd_fmt.pad = pipeline->route[0]->sink->index;
	for (i = -1; i < pipeline->route_sz;) {
		if (media_entity_type(me) == MEDIA_ENT_T_V4L2_SUBDEV) {
			sd = media_entity_to_v4l2_subdev(me);
			if (subdev_has_fn(sd, pad, get_fmt))
				goto get_fmt;
		}
		i++;
		me = pipeline->route[i]->source->entity;
		sd_fmt.pad = pipeline->route[i]->source->index;
	}
	return -EPIPE;

get_fmt:
	return map_entity_get_fmt(me, &sd_fmt);
}
EXPORT_SYMBOL(map_pipeline_get_format);

/* a helper function to create a bunch of links between entities */
int map_create_links(struct map_manager *mngr, struct map_link_dscr *link,
			int cnt)
{
	int i, ret;
	for (i = 0; i < cnt; i++, link++) {
		struct map_agent *src, *dst;
		struct media_entity *src_me, *dst_me;
		int slot;
		slot = (*mngr->hw->get_slot)(*link->src_id);
		src = container_of(mngr->hw->agent_addr[slot],
					struct map_agent, hw);
		if (src == NULL)
			return -ENODEV;
		src_me = &src->subdev.entity;

		slot = (*mngr->hw->get_slot)(*link->dst_id);
		dst = container_of(mngr->hw->agent_addr[slot],
					struct map_agent, hw);
		if (dst == NULL)
			return -ENODEV;
		dst_me = &dst->subdev.entity;
		ret = media_entity_create_link(src_me, link->src_pad,
						dst_me, link->dst_pad, 0);
		if (ret < 0)
			return ret;
	}
	return 0;
}

void map_manager_exit(struct map_manager *mngr)
{
}

int map_manager_add_agent(struct map_agent *agent, struct map_manager *mngr)
{
	int ret = 0;

	BUG_ON(!agent || !agent->ops || !agent->ops->add);
	/* agent should use agent::ops::add to setup agent before register
	 * to manager */
	ret = (*agent->ops->add)(agent);
	if (ret < 0)
		return ret;
	ret = map_agent_register(agent);
	if (ret < 0)
		return ret;

	ret = v4l2_device_register_subdev(&mngr->v4l2_dev, &agent->subdev);
	if (ret < 0)
		return ret;

	/* for each DMA-output-capable agent, should create a pipeline
	 * and vnode for it */
	if (agent->hw.id.mod_type == MAP_AGENT_DMA_OUT) {
		ret = map_pipeline_create(mngr, agent);
		if (ret < 0)
			return ret;
	}

	if (agent->hw.id.mod_type == MAP_AGENT_DMA_IN) {
		struct map_vnode *vnode = kzalloc(sizeof(struct map_vnode),
							GFP_KERNEL);
		if (vnode == NULL)
			return -ENOMEM;
		vnode->ops	= agent->vops;
		ret = mvisp_video_register(vnode, &mngr->v4l2_dev,
						agent->video_type);
		if (ret < 0)
			return ret;

		ret = media_entity_create_link(&vnode->vdev.entity, 0,
				&agent->subdev.entity, agent->pad_vdev, 0);
	}
	return ret;
}
EXPORT_SYMBOL(map_manager_add_agent);

int map_manager_init(struct map_manager *mngr)
{
	int ret = 0;

	/* suppose by this point, all H/W agents had been registed to
	 * H/W manager */
	if (unlikely(mngr == NULL || mngr->hw == NULL
		|| mngr->hw->get_slot == NULL))
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
	mngr->media_dev.link_notify = &map_link_notify;
	ret = media_device_register(&mngr->media_dev);
	/* v4l2 device register */
	mngr->v4l2_dev.mdev = &mngr->media_dev;
	ret = v4l2_device_register(mngr->dev, &mngr->v4l2_dev);
	return ret;
}

int map_manager_attach_agents(struct map_manager *mngr)
{
	int i, ret;

	for (i = 0; i < MAX_AGENT_PER_SYSTEM; i++) {
		struct map_agent *agent;
		if (mngr->hw->agent_addr[i] == NULL)
			continue;

		agent = container_of(mngr->hw->agent_addr[i],
					struct map_agent, hw);
		ret = map_manager_add_agent(agent, mngr);
		if (ret < 0)
			return ret;
		printk(KERN_INFO "subdev '%s' add to '%s'\n",
			agent->hw.name, mngr->name);
	}

	/* Finally, create all the file nodes for each subdev */
	return v4l2_device_register_subdev_nodes(&mngr->v4l2_dev);
}
