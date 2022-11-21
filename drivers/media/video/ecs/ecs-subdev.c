#include <linux/module.h>
#include "ecs-subdev.h"

static __attribute__((unused)) void ecs_dump_subdev_map(struct x_subdev *xsd)
{
	int *enum_map = NULL;
	struct v4l2_mbus_framefmt *state_map = NULL;
	int i;

	state_map = xsd->state_map;
	enum_map = xsd->enum_map;
	for (i = 0; state_map[i].code != 0; i++)
		printk(KERN_INFO "state=%2d, code=%04X, [w,h] = [%4d, %4d]\n", \
			state_map[i].reserved[0], state_map[i].code, \
			state_map[i].width, state_map[i].height);
	for (i = 0; enum_map[i] != 0; i += 2)
		printk(KERN_INFO "code=%04X, idx = %4d\n", \
			enum_map[i], enum_map[i+1]);

}

static int xsd_reg_cid(struct x_subdev *xsd);
/* subdev interface */
int ecs_subdev_init(struct x_subdev *xsd)
{
	struct ecs_property *prop_fmt = NULL, *prop_res = NULL;
	struct ecs_sensor *snsr;
	int *enum_map = NULL;
	struct v4l2_mbus_framefmt *state_map = NULL;
	int tab_sz, line_sz, i, j, idx_st = 0, idx_ft = 0;

	if (unlikely(xsd == NULL))
		return -EINVAL;

	if (unlikely((xsd->get_fmt_code == NULL)
		|| (xsd->get_res_desc == NULL)))
		return -EINVAL;

	/* setup ECS core */
	snsr = xsd->ecs;
	if (snsr != NULL) {
		struct x_i2c *xic = snsr->hw_ctx;
		if (xic == NULL)
			return -EINVAL;
		snsr->cfg_handler = (void *)xic->write_array;
	} else
		return -EINVAL;
	ecs_sensor_init(snsr);

	/* Setup format-resolution to state mapping table */
	prop_fmt = snsr->prop_tab + xsd->fmt_id;
	prop_res = snsr->prop_tab + xsd->res_id;
	line_sz = prop_res->stn_num;
	tab_sz = prop_fmt->stn_num * line_sz;
	state_map = xsd->state_map;
	enum_map = xsd->enum_map;

	{
	int temp[tab_sz];

	if ((state_map == NULL) || (enum_map == NULL))
		return -ENOMEM;
	memset(temp, 0, sizeof(int) * tab_sz);

	/* Establish state map */
	for (i = 0; i < xsd->state_cnt; i++) {
		struct ecs_state *cstate;
		int fmt_val = UNSET, res_val = UNSET;
		/* for state_id_list[i], find "RES" and "FMT" property, and fill
		 * into enum table */
		cstate = snsr->state_tab + xsd->state_list[i];
		if (cstate == NULL) {
			printk(KERN_ERR "x: state id %d not found\n", \
				xsd->state_list[i]);
			return -EINVAL;
		}
		for (j = 0; j < cstate->cfg_num; j++) {
			if (cstate->cfg_tab[j].prop_id == xsd->fmt_id)
				fmt_val = cstate->cfg_tab[j].prop_val;
			if (cstate->cfg_tab[j].prop_id == xsd->res_id)
				res_val = cstate->cfg_tab[j].prop_val;
		}
		if ((fmt_val == UNSET) || (res_val == UNSET))
			return -EPERM;
		if (temp[fmt_val*line_sz + res_val] != 0) {
			printk(KERN_ERR "x: duplicate state: %d and %d\n", \
				temp[fmt_val*line_sz + res_val], \
				xsd->state_list[i]);
			return -EPERM;
		}
		temp[fmt_val*line_sz + res_val] = xsd->state_list[i];
		/* Mark this format 'active' in format table */
		enum_map[fmt_val] = 1;
	};

	/* Initialize the mf table, zero one more slot to establish end sign */
	memset(state_map, 0, sizeof(struct v4l2_mbus_framefmt)*snsr->state_num);
	/* Initialize format enum map */
	memset(enum_map, 0, sizeof(int) * prop_fmt->stn_num * 2);
	/* Establish state map triplets(code, width, height) in state_map */
	/* Establish enumerate map pairs(code, index_of_start) in enum_map */
	idx_st = 0;	/* index for state_map */
	idx_ft = 0;	/* index for enum_map */
	for (i = 0; i < prop_fmt->stn_num; i++) {
		int fmt_en = 0;
		for (j = 0; j < prop_res->stn_num; j++) {
			int state_id = temp[i*line_sz + j];
			if (state_id <= 0)
				continue;
			fmt_en = 1;
			/* get mbus_code and width,height from sensor driver */
			state_map[idx_st].reserved[0] = state_id;
			(*xsd->get_fmt_code)(xsd, i, state_map + idx_st);
			(*xsd->get_res_desc)(xsd, j, state_map + idx_st);
			idx_st++;
		}
		/* If at least one resolution is decleared for this format */
		if (fmt_en) {
			/* save format code in enum_map */
			enum_map[idx_ft++] = state_map[idx_st-1].code;
			/* save index of 1st item for NEXT format, will
			 * put them to the right position later */
			enum_map[idx_ft++] = idx_st;
		}
	}

	}
	/* move the 1st item index to the right position */
	for (idx_ft--; idx_ft > 1; idx_ft -= 2)
		enum_map[idx_ft] = enum_map[idx_ft-2];
	enum_map[idx_ft] = 0;
	/*ecs_dump_subdev_map(xsd);*/

	xsd_reg_cid(xsd);
	return 0;
}
EXPORT_SYMBOL(ecs_subdev_init);

int ecs_subdev_remove(struct x_subdev *xsd)
{
	v4l2_ctrl_handler_free(&(xsd->ctrl_handler));
	return 0;
}
EXPORT_SYMBOL(ecs_subdev_remove);

int ecs_subdev_enum_fmt(struct v4l2_subdev *sd, unsigned int idx, \
			enum v4l2_mbus_pixelcode *code)
{
	struct ecs_property *prop_fmt = NULL;
	struct x_subdev *xsd = get_subdev(sd);
	struct ecs_sensor *snsr = xsd->ecs;
	int *enum_map;

	if (snsr == NULL)
		return -EINVAL;
	if (xsd->enum_map == NULL)
		return -EINVAL;

	enum_map = xsd->enum_map;
	prop_fmt = snsr->prop_tab + xsd->fmt_id;
	if ((idx >= prop_fmt->stn_num) || (enum_map[idx*2] == 0))
		return -EINVAL;
	*code = enum_map[idx*2];
	return 0;
}
EXPORT_SYMBOL(ecs_subdev_enum_fmt);

int ecs_subdev_enum_fsize(struct v4l2_subdev *sd, \
				struct v4l2_frmsizeenum *fsize)
{
	struct x_subdev *xsd = get_subdev(sd);
	struct ecs_sensor *snsr = xsd->ecs;
	int *enum_map;
	struct v4l2_mbus_framefmt *state_map = NULL;
	int i, code, index;

	if (snsr == NULL)
		return -EINVAL;
	if ((xsd->enum_map == NULL) || (xsd->state_map == NULL))
		return -EINVAL;

	state_map = xsd->state_map;
	enum_map = xsd->enum_map;
	code = fsize->pixel_format;
	index = fsize->index;

	for (i = 0; enum_map[i] != 0; i += 2)
		if (enum_map[i] == code)
			goto code_found;
	return -EINVAL;
code_found:
	state_map += (enum_map[i+1] + index);
	if (state_map->code != code)
		return -EINVAL;
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = state_map->width;
	fsize->discrete.height = state_map->height;
	return 0;
}
EXPORT_SYMBOL(ecs_subdev_enum_fsize);

int ecs_subdev_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct x_subdev *xsd = get_subdev(sd);
	struct ecs_sensor *snsr;
	int *enum_map;
	struct v4l2_mbus_framefmt *state_map = NULL;
	int i, code;

	if ((xsd == NULL) || (xsd->ecs == NULL) || \
		(xsd->enum_map == NULL) || (xsd->state_map == NULL))
		return -EINVAL;
	snsr = xsd->ecs;
	state_map = xsd->state_map;
	enum_map = xsd->enum_map;

	code = mf->code;
	for (i = 0; enum_map[i] != 0; i += 2)
		if (enum_map[i] == code)
			goto code_found;
	return -EINVAL;
code_found:
	/* Start searching from the 1st item for this format*/
	for (i = enum_map[i+1]; state_map[i].code == code; i++) {
		if ((state_map[i].width == mf->width)
		&& (state_map[i].height == mf->height))
			return state_map[i].reserved[0];
	}
	return -EPERM;
}
EXPORT_SYMBOL(ecs_subdev_try_fmt);

int ecs_subdev_set_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct x_subdev *xsd = get_subdev(sd);
	struct ecs_sensor *snsr;
	int state = ecs_subdev_try_fmt(sd, mf);

	if (state < 0)
		return state;
	if ((xsd == NULL) || (xsd->ecs == NULL))
		return -EINVAL;
	snsr = xsd->ecs;

	return ecs_set_state(snsr, state);
}
EXPORT_SYMBOL(ecs_subdev_set_fmt);

int ecs_subdev_get_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	const struct ecs_property *prop;
	struct x_subdev *xsd = get_subdev(sd);
	struct ecs_sensor *snsr;

	if ((xsd == NULL) || (xsd->ecs == NULL))
		return -EINVAL;
	snsr = xsd->ecs;

	prop = snsr->prop_tab + xsd->fmt_id;
	if ((prop == NULL) || (prop->value_now >= prop->stn_num) || \
		(prop->value_now < 0))
		return -EINVAL;
	(*xsd->get_fmt_code)(xsd, prop->value_now, mf);

	prop = snsr->prop_tab + xsd->res_id;
	if (prop == NULL)
		return -EINVAL;
	(*xsd->get_res_desc)(xsd, prop->value_now, mf);
	return 0;
}
EXPORT_SYMBOL(ecs_subdev_get_fmt);

int ecs_subdev_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct x_subdev *xsd = get_subdev(sd);
	struct ecs_sensor *snsr;
	struct ecs_state_cfg cfg;

	if ((xsd == NULL) || (xsd->ecs == NULL))
		return -EINVAL;
	snsr = xsd->ecs;

	cfg.prop_id = xsd->str_id;
	cfg.prop_val = enable;

	return ecs_set_list(snsr, &cfg, 1);
}
EXPORT_SYMBOL(ecs_subdev_set_stream);

void ecs_subdev_default_get_fmt_code(const struct x_subdev *xsd, \
				int fmt_value, struct v4l2_mbus_framefmt *mf)
{
	struct ecs_sensor *snsr = xsd->ecs;
	struct ecs_property *prop = snsr->prop_tab + xsd->fmt_id;
	struct ecs_default_fmt_info *info = prop->stn_tab[fmt_value].info;

	mf->code = info->code;
	mf->colorspace = info->clrspc;
	mf->field = info->field;
}
EXPORT_SYMBOL(ecs_subdev_default_get_fmt_code);

void ecs_subdev_default_get_res_desc(const struct x_subdev *xsd, \
				int res_value, struct v4l2_mbus_framefmt *mf)
{
	struct ecs_sensor *snsr = xsd->ecs;
	struct ecs_property *prop = snsr->prop_tab + xsd->res_id;
	struct ecs_default_res_info *info = prop->stn_tab[res_value].info;

	mf->width = info->h_act;
	mf->height = info->v_act;
}
EXPORT_SYMBOL(ecs_subdev_default_get_res_desc);

int xsd_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct x_subdev *xsd = get_subdev(sd);
	int i, ret = 0;

	if (ctrl->id < V4L2_CID_PRIVATE_BASE) {
		/* predefined CID*/
		return v4l2_subdev_s_ctrl(sd, ctrl);
	} else {/* handle private controls */
		struct ecs_state_cfg list;
		struct ecs_property *prop = NULL;

		/* find prop*/
		for (i = 0; i < xsd->cid_cnt; i++) {
			struct xsd_cid *xlate = xsd->cid_list + i;
			if (ctrl->id == xlate->cid) {
				if (xlate->prop.prop_id >= xsd->ecs->prop_num)
					return -ENXIO;
				prop = xsd->ecs->prop_tab + xlate->prop.prop_id;
				break;
			}
		}
		x_log(2, "set CID 0x%08X => set %s", ctrl->id, prop->name);
		list.prop_id = prop->id;
		list.prop_val = ctrl->value;
		ret = ecs_set_list(xsd->ecs, &list, 1);
		ret = (ret > 0) ? 0 : ret;
	}
	return ret;
}
EXPORT_SYMBOL(xsd_s_ctrl);

int xsd_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct x_subdev *xsd = get_subdev(sd);
	int i, ret = 0;

	if (ctrl->id < V4L2_CID_PRIVATE_BASE) {
		/* predefined CID*/
		return v4l2_subdev_g_ctrl(sd, ctrl);
	} else {/* handle private controls */
		struct ecs_property *prop = NULL;
		void *ptr;

		/* find prop*/
		for (i = 0; i < xsd->cid_cnt; i++) {
			struct xsd_cid *xlate = xsd->cid_list + i;
			if (ctrl->id == xlate->cid) {
				if (xlate->prop.prop_id >= xsd->ecs->prop_num)
					return -ENXIO;
				prop = xsd->ecs->prop_tab + xlate->prop.prop_id;
				break;
			}
		}
		x_log(2, "get CID 0x%08X => get %s", ctrl->id, prop->name);
		ret = ecs_get_info(xsd->ecs, prop->id, &ptr);
		if (ret < 0)
			return ret;

		if (ptr == NULL)
			/* no info for this property, return value instead */
			ctrl->value = prop->value_now;
		else
			ctrl->value = (__s32)ptr;
	}
	return 0;
}
EXPORT_SYMBOL(xsd_g_ctrl);

static int xsd_pub_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct x_subdev *xsd = container_of(ctrl->handler, struct x_subdev, \
								ctrl_handler);
	struct ecs_property *prop = NULL;
	struct ecs_state_cfg list;
	int i, ret;

	/* find prop*/
	for (i = 0; i < xsd->cid_cnt; i++) {
		struct xsd_cid *xlate = xsd->cid_list + i;
		if (ctrl->id == xlate->cid) {
			if (xlate->prop.prop_id >= xsd->ecs->prop_num)
				return -ENXIO;
			prop = xsd->ecs->prop_tab + xlate->prop.prop_id;
			break;
		}
	}
	x_log(2, "set %s => set %s", v4l2_ctrl_get_name(ctrl->id), prop->name);
	list.prop_id = prop->id;
	list.prop_val = ctrl->val;
	/* Got ecs property in prop now */
	ret = ecs_set_list(xsd->ecs, &list, 1);
	ret = (ret > 0) ? 0 : ret;
	return ret;
}

static const struct v4l2_ctrl_ops xsd_ctrl_ops = {
	.s_ctrl = xsd_pub_s_ctrl,
};

static int xsd_reg_cid(struct x_subdev *xsd)
{
	int i, ret = 0;

	/* setup v4l2 controls */
	v4l2_ctrl_handler_init(&xsd->ctrl_handler, 0);
	xsd->subdev.ctrl_handler = &xsd->ctrl_handler;

	/* Only register public CID, using CID proceed routine */
	for (i = 0; i < xsd->cid_cnt; i++) {
		struct xsd_cid *xlate = xsd->cid_list + i;
		struct ecs_property *prop = NULL;

		if (xlate->cid >= V4L2_CID_PRIVATE_BASE)
			continue;
		if (xlate->prop.prop_id >= xsd->ecs->prop_num)
			return -ENXIO;
		prop = xsd->ecs->prop_tab + xlate->prop.prop_id;
		x_inf(2, "CID<%s> link to prop %s", \
				v4l2_ctrl_get_name(xlate->cid), prop->name);
		v4l2_ctrl_new_std(&xsd->ctrl_handler, &xsd_ctrl_ops, xlate->cid,
				0, prop->stn_num, 1, xlate->prop.prop_val);
	}

	if (xsd->ctrl_handler.error) {
		ret = xsd->ctrl_handler.error;
		x_inf(2, "register failed with err: %d", ret);
		v4l2_ctrl_handler_free(&xsd->ctrl_handler);
	}
	return ret;
}

int xsd_init(struct v4l2_subdev *sd, u32 val)
{
	struct x_subdev *xsd = get_subdev(sd);
	struct ecs_sensor *snsr = xsd->ecs;
	struct ecs_state_cfg cfg = {xsd->init_id, 1};

	/* Initialize: global init */
	ecs_sensor_reset(snsr);
	return ecs_set_list(snsr, &cfg, 1);
	return 0;
}
EXPORT_SYMBOL(xsd_init);

int xsd_g_chip_ident(struct v4l2_subdev *sd,
				   struct v4l2_dbg_chip_ident *id)
{
	struct x_subdev *xsd = get_subdev(sd);

	id->ident = xsd->model;
	id->revision = 0;
	return 0;
}
EXPORT_SYMBOL(xsd_g_chip_ident);

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int xsd_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct x_subdev *xsd = get_subdev(sd);
	struct x_i2c *xic = xsd->ecs->hw_ctx;
	return (*xic->read)(xic, (u16) reg->reg, (unsigned char *)&(reg->val));
}

static int xsd_s_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct x_subdev *xsd = get_subdev(sd);
	struct x_i2c *xic = xsd->ecs->hw_ctx;
	return (*xic->write)(xic, (u16) reg->reg, (unsigned char)reg->val);
}
#endif

/* Request bus settings on camera side */
static int ecs_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	return 0;
}

struct v4l2_subdev_core_ops ecs_subdev_core_ops = {
	.queryctrl		= v4l2_subdev_queryctrl,
	.querymenu		= v4l2_subdev_querymenu,
	.g_ext_ctrls		= v4l2_subdev_g_ext_ctrls,
	.s_ext_ctrls		= v4l2_subdev_s_ext_ctrls,
	.try_ext_ctrls		= v4l2_subdev_try_ext_ctrls,
	.g_ctrl			= xsd_g_ctrl,
	.s_ctrl			= xsd_s_ctrl,
	.init			= xsd_init,
	.g_chip_ident		= xsd_g_chip_ident,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= xsd_g_register,
	.s_register		= xsd_s_register,
#endif
};

struct v4l2_subdev_video_ops ecs_subdev_video_ops = {
	.s_stream		= &ecs_subdev_set_stream,
	.g_mbus_fmt		= &ecs_subdev_get_fmt,
	.s_mbus_fmt		= &ecs_subdev_set_fmt,
	.try_mbus_fmt		= &ecs_subdev_try_fmt,
	.enum_mbus_fmt		= &ecs_subdev_enum_fmt,
	.enum_mbus_fsizes	= &ecs_subdev_enum_fsize,
	.g_mbus_config		= ecs_g_mbus_config,
};

struct v4l2_subdev_ops ecs_subdev_ops = {
	.core	= &ecs_subdev_core_ops,
	.video	= &ecs_subdev_video_ops,
};
