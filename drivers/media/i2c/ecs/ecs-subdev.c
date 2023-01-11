#include <linux/module.h>
#include <media/mrvl-camera.h>
#include "ecs-subdev.h"

static __attribute__((unused)) void ecs_dump_subdev_map(struct x_subdev *xsd)
{
	int *enum_map = NULL;
	struct v4l2_mbus_framefmt *state_map = NULL;
	int i;

	state_map = xsd->state_map;
	enum_map = xsd->enum_map;
	for (i = 0; state_map[i].code != 0; i++)
		pr_info("state=%2d, code=%04X, [w,h] = [%4d, %4d]\n",
			state_map[i].reserved[0], state_map[i].code,
			state_map[i].width, state_map[i].height);
	for (i = 0; enum_map[i] != 0; i += 2)
		pr_info("code=%04X, idx = %4d\n",
			enum_map[i], enum_map[i+1]);

}

static int xsd_set_profile(struct x_subdev *xsd, const char *profile,
			const struct xsd_spec_item *spec_list, int list_len)
{
	int i, ret;

	if (xsd == NULL || xsd->ecs == NULL || spec_list == NULL ||
		profile == NULL || strlen(profile) == 0)
		return -EINVAL;

	for (i = 0; i < list_len; i++) {
		if (spec_list[i].name && !strcmp(profile, spec_list[i].name))
			goto profile_found;
	}
	return -ENOENT;
profile_found:
	if (i == xsd->profile) {
		x_inf(2, "profile name: %s\n", profile);
		goto done;
	}

	ret = ecs_sensor_merge(xsd->ecs, spec_list[i].ecs);
	if (ret < 0)
		return ret;
	xsd->state_list	= spec_list[i].state_list;
	xsd->state_cnt	= spec_list[i].state_cnt;
done:
	xsd->profile	= i;
	ecs_sensor_reset(xsd->ecs);
	return 0;
}

static inline void merge_fp_table(void *dst_ops, void *def_ops, int size)
{
	/* dst_ops and def_ops are all pointers for a funtion pointer list */
	void (***dst)(void) = dst_ops, (***def)(void) = def_ops;
	int i;

	/* if dst list is empty, let dst_ops pointing at default list */
	if (*dst == NULL) {
		*dst = *def;
		return;
	}

	/* if dst list allocated, check list item one by one */
	for (i = 0; i < size; i++)
		(*dst)[i] = ((*dst)[i]) ? (*dst)[i] : (*def)[i];
}
#define merge_ops(a, b, item) merge_fp_table(&(a->item), &(b->item), \
	sizeof(struct v4l2_subdev_##item##_ops) / sizeof(void (*)(void)))

static inline int xsd_merge_ops(struct v4l2_subdev_ops *dst_ops,
				struct v4l2_subdev_ops *def_ops)
{
	if (dst_ops == NULL || def_ops == NULL)
		return -EINVAL;

	merge_ops(dst_ops, def_ops, core);
	merge_ops(dst_ops, def_ops, video);
	return 0;
}

static int xsd_reg_cid(struct x_subdev *xsd);
static struct v4l2_subdev_ops xsd_ops;
/* subdev interface */
static int xsd_dev_setup(struct x_subdev *xsd, const char *profile,
			const struct xsd_spec_item *spec_list, int list_len)
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

	xsd->profile = UNSET;
	/* handle specialized setting and states */
	if (profile != NULL) {
		int ret = xsd_set_profile(xsd, profile, spec_list, list_len);
		if (ret < 0)
			return ret;
	}

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

	xsd_merge_ops(xsd->ops, &xsd_ops);

	/* Setup format-resolution to state mapping table */
	prop_fmt = snsr->prop_tab + xsd->fmt_id;
	prop_res = snsr->prop_tab + xsd->res_id;
	line_sz = prop_res->stn_num;
	tab_sz = prop_fmt->stn_num * line_sz;
	state_map = xsd->state_map;
	enum_map = xsd->enum_map;
	/* For ECS subdev, states must be defined because they are used setup
	 * enumerate table*/
	if (WARN_ON(xsd->state_cnt == 0) || WARN_ON(xsd->state_list == NULL))
		return -EPERM;

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
			pr_info("x: state id %d not found\n",
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
			pr_info("x: duplicate state: %d and %d\n",
				temp[fmt_val*line_sz + res_val],
				xsd->state_list[i]);
			return -EPERM;
		}
		temp[fmt_val*line_sz + res_val] = xsd->state_list[i];
		/* Mark this format 'active' in format table */
		enum_map[fmt_val] = 1;
	}

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

	return 0;
}

int xsd_dev_clean(struct x_subdev *xsd)
{
	v4l2_ctrl_handler_free(&(xsd->ctrl_handler));
	return 0;
}

static int xsd_enum_fmt(struct v4l2_subdev *sd, unsigned int idx,
			enum v4l2_mbus_pixelcode *code)
{
	struct ecs_property *prop_fmt = NULL;
	struct x_subdev *xsd = to_xsd(sd);
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

static int xsd_enum_fsize(struct v4l2_subdev *sd,
				struct v4l2_frmsizeenum *fsize)
{
	struct x_subdev *xsd = to_xsd(sd);
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

static int xsd_try_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct x_subdev *xsd = to_xsd(sd);
	struct ecs_sensor *snsr;
	int *enum_map;
	struct v4l2_mbus_framefmt *state_map = NULL;
	int i, code;

	if ((xsd == NULL) || (xsd->ecs == NULL) ||
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

static int xsd_set_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct x_subdev *xsd = to_xsd(sd);
	struct ecs_sensor *snsr;
	int state = xsd_try_fmt(sd, mf);

	if (state < 0)
		return state;
	if ((xsd == NULL) || (xsd->ecs == NULL))
		return -EINVAL;
	snsr = xsd->ecs;

	return ecs_set_state(snsr, state);
}

static int xsd_get_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	const struct ecs_property *prop;
	struct x_subdev *xsd = to_xsd(sd);
	struct ecs_sensor *snsr;

	if ((xsd == NULL) || (xsd->ecs == NULL))
		return -EINVAL;
	snsr = xsd->ecs;

	prop = snsr->prop_tab + xsd->fmt_id;
	if ((prop == NULL) || (prop->value_now >= prop->stn_num) ||
		(prop->value_now < 0))
		return -EINVAL;
	(*xsd->get_fmt_code)(xsd, prop->value_now, mf);

	prop = snsr->prop_tab + xsd->res_id;
	(*xsd->get_res_desc)(xsd, prop->value_now, mf);
	return 0;
}

static int xsd_set_stream(struct v4l2_subdev *sd, int enable)
{
	struct x_subdev *xsd = to_xsd(sd);
	struct ecs_sensor *snsr;
	struct ecs_state_cfg cfg;

	if ((xsd == NULL) || (xsd->ecs == NULL))
		return -EINVAL;
	snsr = xsd->ecs;

	if (xsd->str_id >= 0) {
		cfg.prop_id = xsd->str_id;
		cfg.prop_val = enable;
		return ecs_set_list(snsr, &cfg, 1);
	}

	return 0;
}

void xsd_default_get_fmt_code(const struct x_subdev *xsd,
				int fmt_value, struct v4l2_mbus_framefmt *mf)
{
	struct ecs_sensor *snsr = xsd->ecs;
	struct ecs_property *prop = snsr->prop_tab + xsd->fmt_id;
	struct ecs_default_fmt_info *info = prop->stn_tab[fmt_value].info;

	mf->code = info->code;
	mf->colorspace = info->clrspc;
	mf->field = info->field;
}
EXPORT_SYMBOL(xsd_default_get_fmt_code);

void xsd_default_get_res_desc(const struct x_subdev *xsd,
				int res_value, struct v4l2_mbus_framefmt *mf)
{
	struct ecs_sensor *snsr = xsd->ecs;
	struct ecs_property *prop = snsr->prop_tab + xsd->res_id;
	struct ecs_default_res_info *info = prop->stn_tab[res_value].info;

	mf->width = info->h_act;
	mf->height = info->v_act;
}
EXPORT_SYMBOL(xsd_default_get_res_desc);

static int __maybe_unused xsd_flash_control(struct x_subdev *xsd,
						struct v4l2_ctrl *ctrl, bool op)
{
	struct v4l2_subdev *sd = &(xsd->subdev);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);
	struct sensor_board_data *sdata = ssdd->drv_priv;

	if (sdata)
		if (sdata->v4l2_flash_if)
			sdata->v4l2_flash_if((void *)ctrl, op);

	return 0;
}

static int xsd_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct x_subdev *xsd = container_of(ctrl->handler, struct x_subdev,
								ctrl_handler);
	struct ecs_property *prop = NULL;
	struct ecs_state_cfg list;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_FLASH_STROBE:
	case V4L2_CID_FLASH_STROBE_STOP:
		ret = xsd_flash_control(xsd, ctrl, 1);
		return ret;
	case V4L2_CID_FLASH_LED_MODE:
		ret = xsd_flash_control(xsd, ctrl, 0);
		return ret;
	}

	prop = ctrl->priv;
	x_log(2, "set %s => set %s", v4l2_ctrl_get_name(ctrl->id), prop->name);
	list.prop_id = prop->id;
	list.prop_val = ctrl->val;
	/* Got ecs property in prop now */
	ret = ecs_set_list(xsd->ecs, &list, 1);
	ret = (ret > 0) ? 0 : ret;
	return ret;
}

static const struct v4l2_ctrl_ops xsd_ctrl_ops = {
	.s_ctrl = xsd_s_ctrl,
};

static int xsd_reg_cid(struct x_subdev *xsd)
{
	int ret, i;

	/* setup v4l2 controls */
	v4l2_ctrl_handler_init(&xsd->ctrl_handler, xsd->cid_cnt);
	xsd->subdev.ctrl_handler = &xsd->ctrl_handler;

	/* Only register public CID, using CID proceed routine */
	for (i = 0; i < xsd->cid_cnt; i++) {
		struct xsd_cid *xlate = xsd->cid_list + i;
		struct ecs_property *prop = NULL;
		struct v4l2_ctrl_config *cfg = xlate->cfg;

		if (xlate->prop.prop_id >= xsd->ecs->prop_num) {
			ret = -ENXIO;
			goto err;
		}
		prop = xsd->ecs->prop_tab + xlate->prop.prop_id;
		if (cfg == NULL) {
			struct v4l2_ctrl *ctrl;
			x_inf(2, "link CID<%s> to property %s",
				v4l2_ctrl_get_name(xlate->cid), prop->name);
			ctrl = v4l2_ctrl_new_std(&xsd->ctrl_handler,
				&xsd_ctrl_ops, xlate->cid, 0, prop->stn_num,
				1, xlate->prop.prop_val);
			if (ctrl)
				ctrl->priv = prop;
		} else {
			x_inf(2, "link CID<%08X> to property %s",
				cfg->id, prop->name);
			if (cfg->ops == NULL)
				cfg->ops = &xsd_ctrl_ops;
			v4l2_ctrl_new_custom(&xsd->ctrl_handler, cfg, prop);
		}
	}

	if (xsd->ctrl_handler.error) {
		ret = xsd->ctrl_handler.error;
		x_inf(2, "failed to register one or more CID: %d", ret);
		goto err;
	}

	return 0;
err:
	v4l2_ctrl_handler_free(&xsd->ctrl_handler);
	return ret;
}

static int xsd_init(struct v4l2_subdev *sd, u32 val)
{
	struct x_subdev *xsd = to_xsd(sd);
	struct ecs_sensor *snsr = xsd->ecs;
	struct ecs_state_cfg cfg = {xsd->init_id, 1};
	int ret = 0;

	ecs_sensor_reset(snsr);
	/* Initialize: global init */
	ret = ecs_set_list(snsr, &cfg, 1);
	if (ret < 0)
		return ret;
	return v4l2_ctrl_handler_setup(&xsd->ctrl_handler);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int xsd_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct x_subdev *xsd = to_xsd(sd);
	struct x_i2c *xic = xsd->ecs->hw_ctx;
	return (*xic->read)(xic, (u16) reg->reg, (unsigned char *)&(reg->val));
}

static int xsd_s_register(struct v4l2_subdev *sd,
				const struct v4l2_dbg_register *reg)
{
	struct x_subdev *xsd = to_xsd(sd);
	struct x_i2c *xic = xsd->ecs->hw_ctx;
	return (*xic->write)(xic, (u16) reg->reg, (unsigned char)reg->val);
}
#endif

/* Request bus settings on camera side */
static int ecs_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	struct x_subdev *xsd = to_xsd(sd);
	struct ecs_sensor *snsr = xsd->ecs;
	void *pdsc = NULL;
	int ret;

	if (xsd->ifc_id != UNSET) {
		ret = ecs_get_info(snsr, xsd->ifc_id, (void **)(&pdsc));
		if (ret)
			return ret;
	}
	if (*xsd->get_mbus_cfg && pdsc) {
		return (*xsd->get_mbus_cfg)(pdsc, cfg);
	} else {
		x_inf(2, "mbus config function not defined here, assume DVP\n");
		cfg->type = V4L2_MBUS_PARALLEL;
		cfg->flags = 0;
	}

	return 0;
}

static struct v4l2_subdev_core_ops xsd_core_ops = {
	.init			= xsd_init,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register		= xsd_g_register,
	.s_register		= xsd_s_register,
#endif
};

static struct v4l2_subdev_video_ops xsd_video_ops = {
	.s_stream		= &xsd_set_stream,
	.g_mbus_fmt		= &xsd_get_fmt,
	.s_mbus_fmt		= &xsd_set_fmt,
	.try_mbus_fmt		= &xsd_try_fmt,
	.enum_mbus_fmt		= &xsd_enum_fmt,
	.enum_framesizes	= &xsd_enum_fsize,
	.g_mbus_config		= ecs_g_mbus_config,
};

static struct v4l2_subdev_ops xsd_ops = {
	.core	= &xsd_core_ops,
	.video	= &xsd_video_ops,
};

static struct i2c_device_id ecs_subdev_idtable[XSD_DRV_COUNT] = {
	[0] = {"ecs-subdev", 0},
	/* more to add by sensor driver */
};
MODULE_DEVICE_TABLE(i2c, ecs_subdev_idtable);
static struct xsd_driver_id xsd_driver_table[XSD_DRV_COUNT];
/* Used to index xsd driver table AND xsd idtable */
static int xsd_drv_cnt = 1;	/* the [0] slot is reserved for "ecs-subdev" */

int xsd_add_driver(const struct xsd_driver_id *ecs_drv)
{
	int i = 0;

	while (ecs_drv[i].prototype != NULL) {
		const struct xsd_driver_id *xid = ecs_drv + i;
		strncpy(ecs_subdev_idtable[xsd_drv_cnt].name,
			xid->name, I2C_NAME_SIZE);
		ecs_subdev_idtable[xsd_drv_cnt].driver_data = xid->driver_data;
		xsd_driver_table[xsd_drv_cnt].prototype = xid->prototype;
		xsd_driver_table[xsd_drv_cnt].spec_list = xid->spec_list;
		xsd_driver_table[xsd_drv_cnt].spec_cnt = xid->spec_cnt;

		x_inf(2, "add driver<%d> '%s'", xsd_drv_cnt,
			ecs_subdev_idtable[xsd_drv_cnt].name);
		xsd_drv_cnt++;
		if (xsd_drv_cnt >= XSD_DRV_COUNT) {
			x_inf(0, "xsd: driver slot full");
			break;
		}
		i++;
	}
	return i;
}
EXPORT_SYMBOL(xsd_add_driver);

int xsd_del_driver(const struct xsd_driver_id *ecs_drv)
{
	int i;
	const char *name = ecs_drv->name;

	if (name == NULL || strlen(name) == 0)
		return -EINVAL;
	for (i = 1; i < xsd_drv_cnt; i++) {
		if (!strcmp(ecs_subdev_idtable[i].name, name))
			break;
	}
	if (i <= 1 || i >= xsd_drv_cnt)
		return -ENXIO;

	/* driver found, move the last driver to the slot which is taken by the
	 * removed driver */
	memcpy(ecs_subdev_idtable + i, ecs_subdev_idtable + xsd_drv_cnt - 1,
		sizeof(struct i2c_device_id));
	memcpy(xsd_driver_table + i, xsd_driver_table + xsd_drv_cnt - 1,
		sizeof(struct xsd_driver_id));
	xsd_drv_cnt--;
	return 0;
}
EXPORT_SYMBOL(xsd_del_driver);

static struct list_head xsd_dev_list = LIST_HEAD_INIT(xsd_dev_list);

static int ecs_subdev_probe(struct i2c_client *client,
			const struct i2c_device_id *did)
{
	struct x_subdev *xsd, *prototype;
	struct x_i2c *xic;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	char *prof_name;
	int drv_id = did - ecs_subdev_idtable;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	if (drv_id <= 0 || drv_id >= xsd_drv_cnt) {
		x_inf(1, "sensor driver not found in xsd driver list");
		return -ENOENT;
	}
	prototype = xsd_driver_table[drv_id].prototype;

	/* attach i2c_client to xic */
	xic = prototype->ecs->hw_ctx;
	xic->client = client;
	ret = (*xic->detect)(xic);
	if (ret) {
		dev_err(&client->dev, "%s sensor NOT detected\n", did->name);
		return ret;
	}
	dev_err(&client->dev, "sensor %s detected\n", did->name);

	xsd = devm_kzalloc(&client->dev, sizeof(struct x_subdev), GFP_KERNEL);
	if (!xsd) {
		dev_err(&client->dev, "Failed to allocate ecs_subdev data!\n");
		return -ENOMEM;
	}

	/* setup generic driver based on prototype */
	/* memcpy(xsd, prototype, sizeof(*xsd)); */
	xsd = prototype;
	/* initialize ecs_subdev, and maybe customize it by profile name */
	prof_name = (xsd->get_profile) ?
		(*xsd->get_profile)(client) : NULL;
	ret = xsd_dev_setup(xsd, prof_name,
				xsd_driver_table[drv_id].spec_list,
				xsd_driver_table[drv_id].spec_cnt);
	if (ret < 0)
		return ret;
	/* link xsd to i2c_client */
	v4l2_i2c_subdev_init(&xsd->subdev, client, xsd->ops);
	ret = xsd_reg_cid(xsd);
	if (ret < 0) {
		x_inf(1, "v4l2_controls setup error: %d", ret);
		return ret;
	}

	if (prof_name == NULL)
		prof_name = "gnrc";
	sprintf(xsd->name, "%s-%s", did->name, prof_name);
	x_inf(2, "'%s' is created", xsd->name);

	list_add(&xsd->hook, &xsd_dev_list);	/* accept this device */
	return ret;
}

static int ecs_subdev_remove(struct i2c_client *client)
{
	struct x_subdev *xsd = container_of(i2c_get_clientdata(client),
					struct x_subdev, subdev);
	struct x_subdev *xsd_ptr;

	/* clear xsd in xsd dev list */
	list_for_each_entry(xsd_ptr, &xsd_dev_list, hook) {
		if (xsd_ptr == xsd) {
			list_del_init(&xsd_ptr->hook);
			break;
		}
	}
	xsd_dev_clean(xsd);
	devm_kfree(&client->dev, xsd);
	return 0;
}

static struct i2c_driver ecs_subdev_driver = {
	.driver = {
		.name = "ecs-subdev",
	},
	.probe		= ecs_subdev_probe,
	.remove		= ecs_subdev_remove,
	.id_table	= ecs_subdev_idtable,
};

module_i2c_driver(ecs_subdev_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Jiaquan Su <jqsu@marvell.com>");
MODULE_DESCRIPTION("ECS V4L2_subdev-style Camera Sensor Driver");
