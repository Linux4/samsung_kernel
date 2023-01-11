/*
 * s5k8aa Camera Driver
 *
 * Copyright (c) 2013 Marvell Ltd.
 * Libin Yang <lbyang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/platform_data/camera-mmp.h>
#include <linux/v4l2-mediabus.h>
#include "s5k8aa.h"

MODULE_DESCRIPTION("Samsumg S5K8AA Camera Driver");
MODULE_LICENSE("GPL");

static int debug;
module_param(debug, int, 0644);

static const struct s5k8aa_datafmt s5k8aa_colour_fmts[] = {
	{V4L2_MBUS_FMT_UYVY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_VYUY8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YUYV8_2X8, V4L2_COLORSPACE_JPEG},
	{V4L2_MBUS_FMT_YVYU8_2X8, V4L2_COLORSPACE_JPEG},
};

static const struct v4l2_queryctrl s5k8aa_controls[] = {
	{
		.id = V4L2_CID_BRIGHTNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "brightness",
	}
};

static struct s5k8aa_regval *regs_resolution;
static struct s5k8aa_regval *regs_display_setting;

static struct s5k8aa_regval *regs_res_all[] = {
	[S5K8AA_FMT_QCIF] = regs_res_qcif,
	[S5K8AA_FMT_240X160] = regs_res_240x160,
	[S5K8AA_FMT_QVGA] = regs_res_qvga,
	[S5K8AA_FMT_CIF] = regs_res_cif,
	[S5K8AA_FMT_VGA] = regs_res_vga,
	[S5K8AA_FMT_720X480] = regs_res_720x480,
	[S5K8AA_FMT_WVGA] = regs_res_wvga,
	[S5K8AA_FMT_720P] = regs_res_720p,
};

static struct s5k8aa *to_s5k8aa(const struct i2c_client
						 *client)
{
	return container_of(i2c_get_clientdata(client),
				struct s5k8aa, subdev);
}

static int s5k8aa_i2c_read(struct i2c_client *client, u16 addr, u16 *val)
{
	u8 wbuf[2] = {addr >> 8, addr & 0xFF};
	struct i2c_msg msg[2];
	u8 rbuf[2];
	int ret;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = wbuf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = rbuf;

	ret = i2c_transfer(client->adapter, msg, 2);
	*val = be16_to_cpu(*((u16 *)rbuf));

	v4l2_dbg(3, debug, client, "i2c_read: 0x%04X : 0x%04x\n", addr, *val);

	return ret == 2 ? 0 : ret;
}

static int s5k8aa_i2c_write(struct i2c_client *client, u16 addr, u16 val)
{
	u8 buf[4] = {addr >> 8, addr & 0xFF, val >> 8, val & 0xFF};

	int ret = i2c_master_send(client, buf, 4);
	v4l2_dbg(3, debug, client, "i2c_write: 0x%04X : 0x%04x\n", addr, val);

	return ret == 4 ? 0 : ret;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
/* The command register write, assumes Command_Wr_addH = 0x7000. */
static int s5k8aa_write(struct i2c_client *c, u16 addr, u16 val)
{
	int ret = s5k8aa_i2c_write(c, REG_CMDWR_ADDRL, addr);
	if (ret)
		return ret;
	return s5k8aa_i2c_write(c, REG_CMDBUF0_ADDR, val);
}
#endif

/* The command register read, assumes Command_Rd_addH = 0x7000. */
static int s5k8aa_read(struct i2c_client *client, u16 addr, u16 *val)
{
	int ret = s5k8aa_i2c_write(client, REG_CMDRD_ADDRL, addr);
	if (ret)
		return ret;

	return s5k8aa_i2c_read(client, REG_CMDBUF0_ADDR, val);
}

static int s5k8aa_write_raw_array(struct v4l2_subdev *sd,
				  const struct s5k8aa_regval *msg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	while (msg->addr != S5K8AA_TERM) {
		ret = s5k8aa_i2c_write(client, msg->addr, msg->val);
		if (ret)
			break;
		/* Assume that msg->addr is always less than 0xfffc */
		msg++;
	}

	return ret;
}

#if 0
static int s5k8aa_g_chip_ident(struct v4l2_subdev *sd,
				   struct v4l2_dbg_chip_ident *id)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k8aa *s5k8aa = to_s5k8aa(client);

	id->ident = s5k8aa->model;
	id->revision = 0;

	return 0;
}
#endif
#ifdef CONFIG_VIDEO_ADV_DEBUG
static int s5k8aa_g_register(struct v4l2_subdev *sd,
				 struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return s5k8aa_read(client, (u16) reg->reg,
				   (u16 *)&(reg->val));
}

static int s5k8aa_s_register(struct v4l2_subdev *sd,
				 const struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	return s5k8aa_write(client, (u16) reg->reg,
				(u16)reg->val);
}
#endif

static int s5k8aa_s_stream(struct v4l2_subdev *sd, int enable)
{

	int ret = 0;


	if (enable) {
		ret |= s5k8aa_write_raw_array(sd, regs_start_stream);
		ret |= s5k8aa_write_raw_array(sd, regs_select_display);

	} else {
		ret = s5k8aa_write_raw_array(sd, regs_stop_stream);
	}
	return ret;
}

static int s5k8aa_enum_fmt(struct v4l2_subdev *sd,
		unsigned int index,
		enum v4l2_mbus_pixelcode *code)
{
	if (index >= ARRAY_SIZE(s5k8aa_colour_fmts))
		return -EINVAL;
	*code = s5k8aa_colour_fmts[index].code;
	return 0;
}

static inline enum s5k8aa_res_support find_res_code(int width, int height)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(s5k8aa_resolutions); i++) {
		if (s5k8aa_resolutions[i].width == width &&
			s5k8aa_resolutions[i].height == height)
			return s5k8aa_resolutions[i].res_code;
	}
	return -EINVAL;
}

static struct s5k8aa_regval *s5k8aa_get_res_regs(int width, int height)
{
	enum s5k8aa_res_support res_code;

	res_code = find_res_code(width, height);
	if (res_code == -EINVAL)
		return NULL;
	return regs_res_all[res_code];
}

static int s5k8aa_try_fmt(struct v4l2_subdev *sd,
			   struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k8aa *s5k8aa = to_s5k8aa(client);

	switch (mf->code) {
	case V4L2_MBUS_FMT_UYVY8_2X8:
	case V4L2_MBUS_FMT_VYUY8_2X8:
	case V4L2_MBUS_FMT_YVYU8_2X8:
	case V4L2_MBUS_FMT_YUYV8_2X8:
		regs_resolution = s5k8aa_get_res_regs(mf->width, mf->height);
		if (!regs_resolution) {
			/* set default resolution */
			dev_warn(&client->dev,
				"s5k8aa: res not support, set to VGA\n");
			mf->width = S5K8AA_OUT_WIDTH_DEF;
			mf->height = S5K8AA_OUT_HEIGHT_DEF;
			regs_resolution =
				s5k8aa_get_res_regs(mf->width, mf->height);
		}
		s5k8aa->rect.width = mf->width;
		s5k8aa->rect.height = mf->height;
		/* colorspace should be set in host driver */
		mf->colorspace = V4L2_COLORSPACE_JPEG;
		if (mf->code == V4L2_MBUS_FMT_UYVY8_2X8)
			regs_display_setting = regs_display_uyvy_setting;
		else if (mf->code == V4L2_MBUS_FMT_VYUY8_2X8)
			regs_display_setting = regs_display_vyuy_setting;
		else if (mf->code == V4L2_MBUS_FMT_YUYV8_2X8)
			regs_display_setting = regs_display_yuyv_setting;
		else
			regs_display_setting = regs_display_yvyu_setting;
		break;
	default:
		/* This should never happen */
		mf->code = V4L2_MBUS_FMT_UYVY8_2X8;
		return -EINVAL;
	}
	return 0;
}

/*
 * each time before start streaming,
 * this function must be called
 */
static int s5k8aa_s_fmt(struct v4l2_subdev *sd,
			 struct v4l2_mbus_framefmt *mf)
{
	int ret = 0;

	msleep(150);
	ret = s5k8aa_write_raw_array(sd, regs_start_setting);
	msleep(20);
	ret |= s5k8aa_write_raw_array(sd, regs_trap_patch);
	ret |= s5k8aa_write_raw_array(sd, regs_stop_stream);
	ret |= s5k8aa_write_raw_array(sd, regs_analog_setting);
	ret |= s5k8aa_write_raw_array(sd, regs_otp_control);
	ret |= s5k8aa_write_raw_array(sd, regs_gas_setting);
	ret |= s5k8aa_write_raw_array(sd, regs_analog_setting2);
	ret |= s5k8aa_write_raw_array(sd, regs_awb_basic_setting);
	ret |= s5k8aa_write_raw_array(sd, regs_clk_setting);
	ret |= s5k8aa_write_raw_array(sd, regs_flicker_detection);
	ret |= s5k8aa_write_raw_array(sd, regs_ae_setting);
	ret |= s5k8aa_write_raw_array(sd, regs_ae_weight);
	ret |= s5k8aa_write_raw_array(sd, regs_ccm_setting);
	ret |= s5k8aa_write_raw_array(sd, regs_gamma_setting);
	ret |= s5k8aa_write_raw_array(sd, regs_afit_setting);
	ret |= s5k8aa_write_raw_array(sd, regs_display_setting);
	ret |= s5k8aa_write_raw_array(sd, regs_resolution);

	return ret;
}

static int s5k8aa_g_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *mf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct s5k8aa *s5k8aa = to_s5k8aa(client);

	mf->width		= s5k8aa->rect.width;
	mf->height		= s5k8aa->rect.height;
	/* all belows are fixed */
	mf->code		= V4L2_MBUS_FMT_UYVY8_2X8;
	mf->field		= V4L2_FIELD_NONE;
	mf->colorspace		= V4L2_COLORSPACE_JPEG;
	return 0;
}

static int s5k8aa_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *crop)
{
	return 0;
}

static int s5k8aa_s_crop(struct v4l2_subdev *sd, const struct v4l2_crop *crop)
{
	return 0;
}

static int s5k8aa_enum_fsizes(struct v4l2_subdev *sd,
				struct v4l2_frmsizeenum *fsize)
{
	if (!fsize)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	if (fsize->index >= ARRAY_SIZE(s5k8aa_resolutions))
		return -EINVAL;

	fsize->discrete.height = s5k8aa_resolutions[fsize->index].height;
	fsize->discrete.width = s5k8aa_resolutions[fsize->index].width;
	return 0;
}

static int s5k8aa_set_brightness(struct i2c_client *client, int value)
{
	return 0;
}

static int s5k8aa_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_BRIGHTNESS:
		ret = s5k8aa_set_brightness(client, ctrl->value);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static int s5k8aa_init(struct v4l2_subdev *sd, u32 plat)
{
	return 0;
}

static int s5k8aa_g_mbus_config(struct v4l2_subdev *sd,
				struct v4l2_mbus_config *cfg)
{
	cfg->type = V4L2_MBUS_CSI2;
	cfg->flags = V4L2_MBUS_CSI2_1_LANE;
	return 0;
}

static struct v4l2_subdev_core_ops s5k8aa_subdev_core_ops = {
/*	.g_chip_ident	= s5k8aa_g_chip_ident, */
	.init		= s5k8aa_init,
	.s_ctrl		= s5k8aa_s_ctrl,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register	= s5k8aa_g_register,
	.s_register	= s5k8aa_s_register,
#endif
};

static struct v4l2_subdev_video_ops s5k8aa_subdev_video_ops = {
	.s_stream = s5k8aa_s_stream,
	.g_mbus_fmt = s5k8aa_g_fmt,
	.s_mbus_fmt = s5k8aa_s_fmt,
	.try_mbus_fmt = s5k8aa_try_fmt,
	.enum_framesizes = s5k8aa_enum_fsizes,
	.enum_mbus_fmt = s5k8aa_enum_fmt,
	.g_crop = s5k8aa_g_crop,
	.s_crop = s5k8aa_s_crop,
	.g_mbus_config = s5k8aa_g_mbus_config,
};

static struct v4l2_subdev_ops s5k8aa_subdev_ops = {
	.core = &s5k8aa_subdev_core_ops,
	.video = &s5k8aa_subdev_video_ops,
};

static int s5k8aa_detect(struct i2c_client *client)
{
	int ret;
	u16 val;

	ret = s5k8aa_i2c_write(client, 0xfcfc, 0xd000);
	if (ret)
		return ret;
	ret = s5k8aa_i2c_write(client, 0x002c, 0x0000);
	if (ret)
		return ret;
	/* revision */
	/* s5k8aa_read(client, 0x0042, &val);	 */
	ret = s5k8aa_read(client, 0x0040, &val);
	if (ret)
		return ret;
	if (val != 0x8aa)
		return -ENODEV;
	return 0;
}


static int s5k8aa_probe(struct i2c_client *client,
				const struct i2c_device_id *did)
{
	struct s5k8aa *s5k8aa;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA)) {
		dev_warn(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_WORD\n");
		return -EIO;
	}

	ret = s5k8aa_detect(client);
	if (ret)
		return ret;
	dev_info(&adapter->dev, "cam: S5K8AA detect!\n");

	s5k8aa = kzalloc(sizeof(struct s5k8aa), GFP_KERNEL);
	if (!s5k8aa)
		return -ENOMEM;

	v4l2_i2c_subdev_init(&s5k8aa->subdev, client, &s5k8aa_subdev_ops);
	s5k8aa->rect.left = 0;
	s5k8aa->rect.top = 0;
	s5k8aa->rect.width = S5K8AA_OUT_WIDTH_DEF;
	s5k8aa->rect.height = S5K8AA_OUT_HEIGHT_DEF;
	s5k8aa->pixfmt = V4L2_PIX_FMT_UYVY;
	return ret;
}

static int s5k8aa_remove(struct i2c_client *client)
{
	struct s5k8aa *s5k8aa = to_s5k8aa(client);

	kfree(s5k8aa);
	return 0;
}

static const struct i2c_device_id s5k8aa_idtable[] = {
	{"s5k8aay", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, s5k8aa_idtable);

static struct i2c_driver s5k8aa_driver = {
	.driver = {
		   .name = "s5k8aay",
		   },
	.probe = s5k8aa_probe,
	.remove = s5k8aa_remove,
	.id_table = s5k8aa_idtable,
};

static int __init s5k8aa_mod_init(void)
{
	int ret = 0;
	ret = i2c_add_driver(&s5k8aa_driver);
	return ret;
}

static void __exit s5k8aa_mod_exit(void)
{
	i2c_del_driver(&s5k8aa_driver);
}

module_init(s5k8aa_mod_init);
module_exit(s5k8aa_mod_exit);
